#!/usr/bin/env bash
# Signs the app and its embedded camera extension with the Developer ID
# identity. Signing must proceed INSIDE-OUT: nested code first, the app last,
# or the outer signature seals a bundle whose contents then change.
set -euo pipefail

APP="${1:?usage: sign.sh /path/to/OpenOpal.app}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# digitalmud fork: the signer's identity comes from an untracked Local.xcconfig
# (see Local.xcconfig.example) or the environment, never from a tracked file.
conf() {
  [ -f "$ROOT/Local.xcconfig" ] || return 0
  sed -n "s/^[[:space:]]*$1[[:space:]]*=[[:space:]]*//p" "$ROOT/Local.xcconfig" | tail -1
}
TEAM_ID="${TEAM_ID:-$(conf TEAM_ID)}"
IDENTITY="${IDENTITY:-$(conf IDENTITY)}"
[[ "$TEAM_ID" =~ ^[A-Z0-9]{10}$ ]] || { echo "TEAM_ID missing or malformed: set it in Local.xcconfig (see Local.xcconfig.example)" >&2; exit 1; }
[ -n "$IDENTITY" ] || { echo "IDENTITY missing: set it in Local.xcconfig (see Local.xcconfig.example)" >&2; exit 1; }
case "$IDENTITY" in *"($TEAM_ID)"*) ;; *) echo "IDENTITY ($IDENTITY) is not for team $TEAM_ID" >&2; exit 1 ;; esac

# The extension bundle is named after its bundle ID: <app bundle ID>.camera.
BUNDLE_ID="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleIdentifier' "$APP/Contents/Info.plist")"
EXT="$APP/Contents/Library/SystemExtensions/$BUNDLE_ID.camera.systemextension"
[ -d "$EXT" ] || { echo "camera extension not found at $EXT" >&2; exit 1; }

# The tracked entitlements carry a literal $(TEAM_ID); render real ones to sign with.
WORK="$(mktemp -d "${TMPDIR:-/tmp}/opal-sign.XXXXXX")"
trap 'rm -rf "$WORK"' EXIT
render() {
  sed "s/\$(TEAM_ID)/$TEAM_ID/g" "$1" > "$2"
  if grep -q 'TEAM_ID' "$2"; then echo "unrendered TEAM_ID placeholder in $2" >&2; exit 1; fi
}
render "$ROOT/Sources/OpenOpal/OpenOpal.entitlements" "$WORK/app.entitlements"
render "$ROOT/Sources/OpenOpalCameraExtension/OpenOpalCameraExtension.entitlements" "$WORK/ext.entitlements"

echo "==> validating bundled dependencies"
# No Homebrew/build-machine paths may survive into hardened-runtime signing.
python3 "$ROOT/scripts/bundle-dependencies.py" "$APP" --validate-only

echo "==> embedding provisioning profiles"
# Restricted entitlements (system-extension.install) are only honored when a
# provisioning profile in the bundle grants them. Developer ID signing alone
# isn't enough: AMFI rejects the launch outright (spawn error 163).
cp "$ROOT/Provisioning/OpenOpal.provisionprofile" "$APP/Contents/embedded.provisionprofile"
cp "$ROOT/Provisioning/OpenOpalCameraExtension.provisionprofile" "$EXT/Contents/embedded.provisionprofile"

echo "==> dylibs"
for LIB in "$APP"/Contents/Frameworks/*.dylib; do
  codesign --force --timestamp --options runtime --sign "$IDENTITY" "$LIB"
done

echo "==> camera extension"
codesign --force --timestamp --options runtime \
  --entitlements "$WORK/ext.entitlements" \
  --sign "$IDENTITY" "$EXT"

echo "==> app"
codesign --force --timestamp --options runtime \
  --entitlements "$WORK/app.entitlements" \
  --sign "$IDENTITY" "$APP"

echo "==> verify"
codesign --verify --deep --strict --verbose=1 "$APP"
codesign -d --entitlements - "$APP" 2>/dev/null | grep -o "system-extension.install" || true

---
id: T-005
type: ticket
title: Virtual camera working — adopt upstream PR #1, our identity, Developer ID sign + notarize, /Applications
status: doing
priority: high
tags: [signing, notarization, virtual-camera, release]
created: 2026-09-26
updated: 2026-09-26
depends_on: [T-001]
kind: feature
release: c1-follow
sprint: 3
---

## Description

Chris, 2026-09-26 12:49: "let's pause on the follow and get the virtual camera working 100%."
Upstream publishes a CoreMediaIO camera extension, "Open Opal Camera", that Zoom, Meet and FaceTime
can pick. macOS loads it only from a Developer ID-signed, notarized app in `/Applications`. The
free personal team can't sign system extensions ("Personal development teams do not support the
System Extension capability", Apple Developer Forums), so **Chris is enrolling in the Apple
Developer Program (Path A, his decision 12:53)**.

Upstream as-is has a bug that would leave the virtual camera blank. Its feeder picks the
extension's *capture* stream instead of its *sink* (legacy CMIO uses direction 1 for input, the
opposite of `CMIOExtensionStream.Direction`). Upstream's open **PR #1** (alii/open-opal, by a
third party, tested on an Opal C1 with Google Meet showing "Open Opal Camera") fixes that. It also
bundles and validates dylibs before signing, derives the extension ID from the app's bundle ID,
and persists camera settings across restarts. This ticket adopts it, moves every identifier from
upstream's to ours **without committing a team ID**, then signs, notarizes, installs and proves
the camera works in a real call app.

## What to build

1. **Adopt PR #1:** `git fetch upstream pull/1/head:upstream-pr1` and merge it into `master`
   (merge commit, not a squash, so its authorship is kept). Resolve conflicts, if any (none
   expected: it touches Swift and scripts, not the bridge). Run its own checks
   (`python3 -m unittest discover -s scripts/tests -v`; its Swift persistence tests as its
   `docs/RELEASING.md` describes, or record why they can't run here).
2. **Our identity, no team ID in git:**
   - `project.yml`: `bundleIdPrefix: ca.digitalmud`; the app `ca.digitalmud.open-opal`, the
     extension `ca.digitalmud.open-opal.camera` (product name the same); App Group
     `group.ca.digitalmud.open-opal`; `CMIOExtensionMachServiceName`
     `group.ca.digitalmud.open-opal.camera`. Delete `DEVELOPMENT_TEAM`. In both entitlements,
     `com.apple.application-identifier` → `$(TEAM_ID).<bundle id>` and
     `com.apple.developer.team-identifier` → `$(TEAM_ID)`: a literal placeholder.
   - `scripts/sign.sh`: read `TEAM_ID` and `IDENTITY` from an untracked `Local.xcconfig`
     (`KEY = value` lines) or the environment; fail loudly if missing; render each entitlements file
     to `$TMPDIR` with `$(TEAM_ID)` replaced and sign with those; derive the extension path from
     the bundle ID. `.gitignore` gets `Local.xcconfig`. Also the other upstream-identity sites:
     `ExtensionInstaller.swift` (if PR #1 didn't already derive it), `release.sh` comments,
     `docs/RELEASING.md`.
   - `Local.xcconfig.example` (tracked) showing the two keys with fake values.
3. **Apple-side setup (Chris does these; each is ask-first):** the checklist in
   `docs/INSTALL.md` § Apple setup, with our exact IDs: a Developer ID Application certificate
   (G2 Sub-CA; plus `DeveloperIDG2CA.cer` in the keychain), the App Group, two App IDs (App Groups
   on both; System Extension on the app), two Developer ID provisioning profiles into
   `Provisioning/` (gitignored), notarization credentials via
   `xcrun notarytool store-credentials openopal …` (Chris types the app-specific password
   himself, in Terminal.app).
4. **Build, sign, notarize, install:** `scripts/release.sh` → `/Applications/OpenOpal.app`.
   Remove stale copies LaunchServices might resolve (DerivedData builds) as upstream's table
   warns. Chris clicks Install virtual camera and approves the system-extension prompt.
5. **Prove it:** "Open Opal Camera" in `system_profiler SPCameraDataType`; Chris sees the live
   processed picture in FaceTime **and** one of Zoom/Meet; quit the app → the camera shows a
   placeholder or disappears cleanly (record which); relaunch → it comes back.
6. **Docs:** `docs/INSTALL.md` (≤60 lines: Apple setup checklist, `Local.xcconfig`, release,
   install, troubleshooting pointer to `docs/SIGNING.md`); `README.md` short "This fork" section.

### Files touched

| Path | Action |
|---|---|
| `Sources/OpenOpal/Camera/CameraSettings.swift` | merged from PR #1 (persistence) |
| `Sources/OpenOpal/UI/Inspector.swift` | merged from PR #1 |
| `Sources/OpenOpal/VirtualCamera/ExtensionInstaller.swift` | merged from PR #1 (+ our identity if still hard-coded) |
| `Sources/OpenOpal/VirtualCamera/VirtualCameraFeeder.swift` | merged from PR #1 (sink-stream fix) |
| `scripts/release.sh` | merged from PR #1; our identity in comments |
| `scripts/bundle-dependencies.py` | new, from PR #1 |
| `scripts/tests/test_bundle_dependencies.py` | new, from PR #1 |
| `Tests/CameraSettingsPersistenceTests.swift` | new, from PR #1 |
| `docs/SIGNING.md` | merged from PR #1 |
| `docs/RELEASING.md` | merged from PR #1; our identity |
| `project.yml` | edit: identifiers, placeholder team ID, remove `DEVELOPMENT_TEAM` |
| `Sources/OpenOpal/OpenOpal.entitlements` | regenerated by xcodegen from `project.yml` |
| `Sources/OpenOpalCameraExtension/OpenOpalCameraExtension.entitlements` | regenerated by xcodegen from `project.yml` |
| `Sources/OpenOpalCameraExtension/Info.plist` | regenerated by xcodegen from `project.yml` |
| `scripts/sign.sh` | edit: `Local.xcconfig`, rendered entitlements, derived extension path |
| `.gitignore` | edit: `Local.xcconfig` |
| `Local.xcconfig.example` | new |
| `docs/INSTALL.md` | new |
| `README.md` | edit |

## Charter decisions (locked 2026-09-26)

| Decision | Choice |
|---|---|
| Path | A, Developer ID (Chris, 12:53: "pay for Developer ID … I can sign up") |
| Upstream PR #1 | **Adopt** by merge (Chris confirmed 2026-09-26 ~13:00). It fixes the feeder stream bug that would leave the camera blank; its settings persistence overlaps T-004 (T-004 re-scopes after) |
| Identifiers | `ca.digitalmud.open-opal`, `.camera`, `group.ca.digitalmud.open-opal`, Mach service `group.ca.digitalmud.open-opal.camera` |
| Team ID home | Untracked `Local.xcconfig`; committed files carry `$(TEAM_ID)`; `sign.sh` renders at sign time (CLAUDE.md: never commit a team ID) |
| Who touches Apple/keychain | Chris, step by step, from `docs/INSTALL.md` (CLAUDE.md § Permissions: ask first). I never read or export keys, certificates or passwords |
| Follow mode | Stays paused on `t003-follow-wip`; T-005 is built on `master` without it |
| Membership type | Chris's choice (individual activates fastest; an organization needs a D-U-N-S number). The IDs above work under either |

## Acceptance criteria

- [ ] PR #1 merged; its Python packaging tests pass (pasted)
- [ ] § Verification block passes (no team ID in git; identifiers switched; signed + notarized app in /Applications; "Open Opal Camera" listed)
- [ ] Chris sees the live picture from "Open Opal Camera" in FaceTime and in Zoom or Meet (human-assisted, recorded in the Build log)
- [ ] After a quit and relaunch, the camera works again without reinstalling (human-assisted)
- [ ] `docs/INSTALL.md` exists and a fresh reader could redo the setup

## Verification

```bash
set -euo pipefail
# no team ID committed anywhere (a 10-char team ID next to the identifier keys)
if git grep -n -E '(DEVELOPMENT_TEAM|team-identifier|NOTARY_TEAM_ID)[^A-Za-z]*[A-Z0-9]{10}' -- . ':!vendor' ':!.tickets'; then echo "team ID committed"; exit 1; fi
if git grep -n 'RD994J874S' -- project.yml Sources scripts; then echo "upstream team ID still in build files"; exit 1; fi
test -f Local.xcconfig.example
git check-ignore -q Local.xcconfig
grep -q 'ca.digitalmud.open-opal' project.yml
# PR #1's packaging checks
python3 -m unittest discover -s scripts/tests >/dev/null 2>&1 || { echo "packaging tests failed"; exit 1; }
# the installed app is Developer ID signed, notarized and accepted
APP=/Applications/OpenOpal.app
codesign -dv --verbose=2 "$APP" 2>&1 | grep -q 'Authority=Developer ID Application'
spctl -a -vv -t exec "$APP" 2>&1 | grep -q 'accepted'
xcrun stapler validate "$APP" >/dev/null
system_profiler SPCameraDataType | grep -q 'Open Opal Camera'
echo T-005 verification OK
```

## Constraints

- **Blast radius:** app/extension identifiers, signing scripts, PR #1's files, docs;
  `/Applications/OpenOpal.app`; one system extension. No bridge changes.
- **Permission class:** ask first: every Apple-portal, keychain, certificate, notarization-credential
  and system-extension step (Chris does them). Ship freely: code, docs, builds, the copy to
  `/Applications` (CLAUDE.md allows it when a ticket says so; this one does).
- **Read/write boundaries:** never commit a team ID, profile, certificate or password; never push
  to `upstream`.

## Out of scope

- Follow mode (paused). T-011's hang (separate). Launch at login. Auto-update. Publishing releases.

## Notes

- Upstream `docs/SIGNING.md` § "Things that will waste your day" is the troubleshooting table
  (e.g. `extensionNotFound` for five different real causes; staple the app, not the extension).
- Our bundle has no Homebrew dylib dependencies today (`otool -L` on the three bundled dylibs
  shows only `@rpath` and system paths), unlike upstream's v0.1.0 DMG; PR #1's validator still
  guards it.
- `sysextd` log is the honest source:
  `log show --last 5m --predicate 'process == "sysextd"' --style compact`.

---

## Principles in scope

`security.secrets-out-of-git` (team ID via untracked config) · `security.least-privilege` / CLAUDE.md § Permissions (Chris performs keychain/Apple steps) · `core.continuation` (adopt PR #1 rather than re-fix) · `verification.run-it` · `verification.behavioral-gap` (the call-app checks are human-assisted, labelled) · `core.problem-boundary` (follow and T-011 stay separate)

## Plan

Scoped 2026-09-26 ~13:00 while Chris enrols in the Apple Developer Program.

1. Merge PR #1 into `master` on a working branch `t005-virtual-camera`, build, run its tests.
2. Identity switch + placeholder mechanism; build unsigned to prove nothing broke (the app still
   runs from `build/` without signing, as today).
3. Write `docs/INSTALL.md` § Apple setup first, so Chris can work through it while I finish.
4. When Chris has the certificate, profiles and notary profile: `Local.xcconfig` (Chris fills in
   `TEAM_ID` and `IDENTITY`; I don't read the keychain), `release.sh`, install, approve, test.

**Doc impact for /deploy:** `docs/brief.md` (Status, Decisions: Path A, PR #1 adopted),
`README.md`, `docs/INSTALL.md` (new); the release manifest (T-005 title, T-004's overlap with
PR #1's persistence).

## Build log (Dev)

### What was built
- 

### Verification output
- 

### Open items for Review
- 

### Failed attempts
- 

## Review

_Review stage fills this in._

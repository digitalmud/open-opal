# Installing this fork with the virtual camera

The app runs from `build/` unsigned, but the **virtual camera** ("Open Opal Camera" in Zoom,
Meet, FaceTime) only loads from a Developer ID-signed, notarized app in `/Applications`. A
free Apple ID can't sign it: personal teams don't get the System Extension capability. So
this needs an Apple Developer Program membership. First build: `docs/BUILD-mudmini.md`.

## Apple setup (once, in Apple's site/Xcode/Terminal; nothing here goes in git)

1. **Certificate:** Xcode ▸ Settings ▸ Accounts ▸ + (your Apple ID) ▸ your team ▸ Manage
   Certificates… ▸ + ▸ **Developer ID Application**. If `security find-identity -v -p
   codesigning` doesn't list it, install Apple's `DeveloperIDG2CA.cer` (see `docs/SIGNING.md`).
2. **App Group** (developer.apple.com ▸ Identifiers ▸ + ▸ App Groups): `group.ca.digitalmud.open-opal`.
3. **App IDs** (Identifiers ▸ + ▸ App IDs ▸ App):
   - `ca.digitalmud.open-opal`: App Groups (that group) + **System Extension**;
   - `ca.digitalmud.open-opal.camera`: App Groups (same group).
4. **Profiles** (Profiles ▸ + ▸ Distribution ▸ Developer ID), one per App ID, saved as
   `Provisioning/OpenOpal.provisionprofile` and
   `Provisioning/OpenOpalCameraExtension.provisionprofile` (the folder is gitignored).
5. **Notarization:** make an app-specific password at account.apple.com, then in Terminal.app:
   `xcrun notarytool store-credentials openopal --apple-id <you> --team-id <TEAM ID>`.
6. **`Local.xcconfig`** (gitignored; copy `Local.xcconfig.example`): `TEAM_ID` and
   `IDENTITY` (the exact name `security find-identity -v -p codesigning` prints).

## Build, sign, notarize, install

```sh
./scripts/release.sh     # build -> bundle dylibs -> sign -> notarize -> staple -> /Applications
```

Then open **/Applications/OpenOpal.app** ▸ Advanced ▸ Virtual Camera ▸ **Install virtual
camera**, and approve the prompt in System Settings ▸ General ▸ Login Items & Extensions.
"Open Opal Camera" then appears in every video app while Open Opal is running.

## If it doesn't show up

`docs/SIGNING.md` § "Things that will waste your day" maps the misleading errors to real causes.
The honest log: `log show --last 5m --predicate 'process == "sysextd"' --style compact`.
Keep only one copy of the app registered: a build left in `build/DerivedData` can capture the
bundle ID (`lsregister -u <path>` unregisters it).

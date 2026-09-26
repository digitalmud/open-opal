# Building on mudmini

What worked on 2026-09-26 (macOS 26.5.2, Apple silicon, Xcode 26.6 / 17F113). Upstream's
README § Building assumes a set-up Xcode and a camera like upstream's; neither held here.

## Toolchain (once)
```sh
brew install cmake ninja xcodegen
# Xcode from the App Store, then (needs an admin password; run in Terminal.app, not a `!` pane):
sudo xcode-select -s /Applications/Xcode.app/Contents/Developer
sudo xcodebuild -license accept
xcodebuild -runFirstLaunch                       # installs CoreSimulator; no sudo needed here
xcodebuild -downloadComponent MetalToolchain     # Xcode 26 ships the Metal compiler separately (688 MB)
```

Skip the last two and `xcodebuild` fails ("failed to load a required plug-in", then "missing
Metal Toolchain"). `fetch-models.sh` needs full Xcode too (`coremlcompiler`).

## Build
```sh
./scripts/bootstrap.sh        # depthai-core v2.30.0; a few minutes cold, seconds with ~/.hunter warm
./scripts/fetch-models.sh
xcodegen generate
xcodebuild -project OpenOpal.xcodeproj -scheme OpenOpal -configuration Release \
  -derivedDataPath build/DerivedData build
open build/DerivedData/Build/Products/Release/OpenOpal.app
```

Ad-hoc (linker) signed, no team: upstream's team ID is never applied; no virtual camera (T-005).

## Chris's C1 is not upstream's C1
It enumerates as `03e7:f63b` (upstream's: `f63d`), its bootloader reports **0.0.0** (upstream's:
0.0.15) and depthai names the sensor **IMX378**. It only sits in its bootloader for ~5 s after
power-on. So: **start the app (or click Try again), then unplug and replug the C1.** The bridge
catches the bootloader window and sends it the one USB-ROM-boot command (T-006).

## Numbers (1080p default mode, Release bridge, through Chris's USB 3.1 hub)
- 30.1 fps, p50 latency 52.2 ms over 60 s (`opal_get_telemetry`, 09:39; app toolbar: 30 fps · 54 ms)
- Stock "Opal C1" webcam back 7 s after quitting the app
- Unplugging the camera mid-stream no longer kills the app: it reconnects when the camera is back (T-011).

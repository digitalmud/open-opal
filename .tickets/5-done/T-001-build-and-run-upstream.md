---
id: T-001
type: ticket
title: Build and run upstream Open Opal on mudmini
status: done
priority: high
tags: [build, baseline, xcode]
created: 2026-09-26
updated: 2026-09-26
closed: 2026-09-26
depends_on: []
kind: chore
release: c1-follow
sprint: 1
---

## Description

Nothing in this fork can be tested until upstream builds and streams from the C1 on this Mac.
This ticket gets there and records the numbers upstream claims (≈45 ms, 30 fps at 1080p) as
measured here, so T-002's spike has a baseline to compare against.

The known blocker: mudmini has only Command Line Tools. Upstream needs Xcode 26, `ninja` and
`xcodegen`. Xcode is a system install and is **ask first** (`CLAUDE.md § Permissions`).

## What to build

1. Toolchain. Ask Chris before installing Xcode. Preferred path: `brew install mas` then
   `mas install 497799835` (Xcode, App Store; needs the App Store signed in), then
   `sudo xcode-select -s /Applications/Xcode.app` and accept the licence. If `mas` cannot
   install, tell Chris the App Store link and wait. Then `brew install ninja xcodegen`.
2. `./scripts/bootstrap.sh` (clones and builds depthai-core v2.30.0; several minutes; Hunter
   caches in `~/.hunter`). `./scripts/fetch-models.sh`. `xcodegen generate`.
3. Neutralise upstream's team ID for local builds: in `project.yml` leave
   `CODE_SIGN_IDENTITY: "-"` / signing disabled as is, but do not rely on `DEVELOPMENT_TEAM`.
   If xcodebuild complains about the team, remove the `DEVELOPMENT_TEAM` line in this fork
   and note it in the Build log (T-005 puts the right one back via a non-committed xcconfig).
4. `xcodebuild -project OpenOpal.xcodeproj -scheme OpenOpal -configuration Release
   -derivedDataPath build/DerivedData build`.
5. Run `build/DerivedData/Build/Products/Release/OpenOpal.app` with the C1 plugged in. Confirm
   it boots the pipeline (boot log overlay), streams 1080p, and the toolbar shows latency and
   fps. Record both numbers after 60 s of streaming. Quit the app; confirm "Opal C1 (ctrl)"
   reappears in `system_profiler SPCameraDataType` within ~10 s.
6. Add a `docs/BUILD-mudmini.md` (≤40 lines): the exact commands that worked here, the
   numbers, and anything that differed from upstream's README.

### Files touched

| Path | Action |
|---|---|
| `project.yml` | edit only if the team ID blocks the build |
| `docs/BUILD-mudmini.md` | new |
| `worklog.md` | append |

Total: ≤3 files, <80 LOC.

## Charter decisions (locked 2026-09-26)

| Decision | Choice |
|---|---|
| Xcode source | ~~App Store via `mas`~~ → **Chris installs Xcode by hand** (his answer, 2026-09-26 08:54); the build waits for `/Applications/Xcode.app` |
| Build config for daily use | Release (upstream: Debug stutters) |
| Virtual camera in this ticket | No — unsigned builds can't load it; that is T-005 |
| Where the build runs | The main checkout, not a worktree: `build/`, `vendor/`, `Models/` are gitignored, machine-local and several GB |
| `project.yml` team ID | Leave it. `CODE_SIGNING_ALLOWED: NO` means Xcode never applies it; only edit if xcodebuild actually errors on the team |
| If the unsigned app won't launch | Ad-hoc sign the build output (`codesign --force --sign -`, dylibs first, no entitlements, no team). No certificate or keychain involved, so it stays inside "ship freely" |
| How the fps/latency numbers are read | The toolbar is the only surface (`ContentView.swift:152–172`, from `opal_get_telemetry`); no log line exists and T-001 adds no code. Screenshot the window with `screencapture`; if Screen Recording is denied, Chris reads the two numbers off the toolbar (human step) |

## Acceptance criteria

- [ ] `xcodebuild … build` exits 0 and `OpenOpal.app` exists under `build/DerivedData/Build/Products/Release/`
- [ ] App streams from the C1; Build log carries the telemetry line (fps, p50 latency) after 60 s
- [ ] After quitting, the UVC camera "Opal C1 (ctrl)" is listed again by `system_profiler`
- [ ] `docs/BUILD-mudmini.md` exists with the commands and numbers
- [ ] § Verification block passes

## Verification

```bash
set -euo pipefail
test -d build/DerivedData/Build/Products/Release/OpenOpal.app
test -s docs/BUILD-mudmini.md
grep -q -i 'fps' docs/BUILD-mudmini.md
grep -q -i 'latency' docs/BUILD-mudmini.md
# the depth model compiled (needs full Xcode's coremlcompiler)
test -d Models/DepthAnythingV2SmallF16.mlmodelc
# the app is not left holding the camera, and the stock UVC device is back
if pgrep -x OpenOpal >/dev/null; then echo "OpenOpal still running"; exit 1; fi
system_profiler SPCameraDataType | grep -q 'Opal C1'
echo T-001 verification OK
```

## Constraints

- **Blast radius:** toolchain on this Mac; this repo's `build/`, `vendor/`, `Models/` (all
  gitignored); one new doc.
- **Permission class:** Xcode install = ask first. Everything else ship freely.
- **Read/write boundaries:** never flash the camera; don't leave the app holding the device.

## Out of scope

- Any code change to the pipeline (T-002). Signing (T-005).

## Notes

- Upstream README § Building and § The hardware. `scripts/bootstrap.sh` already carries the
  zlib/CMake 4 workarounds; if Hunter fails on something new, record the fix in the doc.

- 2026-09-26 close: upstream builds (Xcode 26.6) and streams on mudmini at 30.1 fps / p50 51 ms, but only with T-006's bridge fix for this C1's 0.0.0 bootloader; steps in docs/BUILD-mudmini.md.
---

## Principles in scope

`core.verify-first` · `verification.run-it` · `verification.measure-first` (numbers from telemetry, pasted) · `verification.stage-runnable` (block runs in the main checkout, see Charter) · `verification.behavioral-gap` (the toolbar read may be a labelled human step) · `security.secrets-out-of-git` (no team ID committed) · `core.problem-boundary` (no pipeline code here)

## Plan

Scoped 2026-09-26 in the session Chris opened. The ticket was moved straight to `3-doing` at
his instruction, so it skips `2-todo`.

**Done already (no Xcode needed):**
- `brew install ninja xcodegen` — exit 0.
- `scripts/fetch-models.sh` — download complete; the compile step failed with
  `xcrun: error: unable to find utility "coremlcompiler"` (exit 72). That tool ships only with
  full Xcode. Rerun the script after Xcode; it skips files already downloaded.
- `scripts/bootstrap.sh` — running (Hunter building deps). CLT's clang is enough for it.

**After Chris installs Xcode:**
1. `sudo xcode-select -s /Applications/Xcode.app/Contents/Developer` and
   `sudo xcodebuild -license accept` — both need Chris's password; hand him the two lines to
   run with `!`. Then `xcodebuild -runFirstLaunch` if Xcode asks for it.
2. Rerun `scripts/fetch-models.sh` → `Models/DepthAnythingV2SmallF16.mlmodelc` exists.
3. `xcodegen generate`, then the Release `xcodebuild` from § What to build step 4. Log to
   `build/xcodebuild.log`.
4. Launch the app with the C1 plugged in; stream 60 s; read fps and latency (Charter row);
   quit with `osascript -e 'quit app "Open Opal"'`; confirm `pgrep -x OpenOpal` is empty and
   `system_profiler SPCameraDataType` lists the Opal C1 again.
5. Write `docs/BUILD-mudmini.md`; append the worklog.

**Things that could bite, found while scoping:**
- `sign.sh` hard-codes upstream's identity and `Provisioning/` profiles; don't run it (T-005).
- The entitlements file carries upstream's team ID (`RD994J874S`) but is never applied while
  signing is off. T-005 deals with it.
- Upstream's README says "Xcode 26"; the Mac runs macOS 26.5.2, so current App Store Xcode fits.

**Doc impact for /deploy:** `docs/brief.md` Status line and the "ninja, xcodegen, Xcode
missing" line in § Context go stale once this lands; the release manifest's first deliverable
flips to done.

## Build log (Dev)

### What was built
- BASELINE: 1 failure — `test -d …/OpenOpal.app` (the app isn't built yet; expected).
- Toolchain: Chris installed Xcode 26.6 (17F113) by hand, ran `xcode-select -s` and accepted
  the licence (08:58). `xcodebuild -runFirstLaunch` (no sudo needed) installed
  CoreSimulator. `xcodebuild -downloadComponent MetalToolchain` fetched the Metal compiler
  (688 MB; Xcode 26 no longer bundles it). `brew install ninja xcodegen`.
- `scripts/bootstrap.sh` exit 0 → `vendor/install/lib/libdepthai-core.dylib` (16 MB) +
  `libusb-1.0.dylib`. `scripts/fetch-models.sh` exit 0 → `Models/DepthAnythingV2SmallF16.mlmodelc`.
- `xcodegen generate`, then the Release `xcodebuild` → `** BUILD SUCCEEDED **`. Nine
  warnings, all upstream's (Swift 6 concurrency, one deprecation, one duplicate rpath).
  The binary carries only the linker's ad-hoc signature, `TeamIdentifier=not set`, so upstream's
  team ID was never applied and `project.yml` stayed unedited.
- Run: the app launches, but shows **"No Opal C1 found"** with the C1 plugged in. See Open items.
  Resolved by **T-006** (Chris chose a separate ticket), which adds a bridge path for this
  camera's 0.0.0 bootloader. T-001 then finished on top of T-006's Release build.
- Streaming (09:23:25 onwards, T-006 build, through Chris's USB 3.1 hub at 5 Gb/s):
  **1080p · 30 fps · 54 ms** latency, read by Chris off the toolbar after >60 s. The window
  capture would have stored live video of Chris; a crop attempt missed the toolbar and was
  deleted, so the toolbar reading is the record (Charter fallback).
- Quit → stock "Opal C1 (ctrl)" back after **7 s**.
- `docs/BUILD-mudmini.md` written (39 lines): toolchain steps that differ from upstream's README,
  the replug routine for this camera, the numbers.
- `project.yml` untouched.
- In-latitude note: a full-screen `screencapture` caught unrelated windows (a password
  manager); both images were deleted immediately. From here on, only window-scoped captures
  (`screencapture -l <windowID>`).

### Verification output
Telemetry line (T-006 hardware run 4, 09:39, `opal_get_telemetry` after 60 s; the same bridge
as the Release app):
```
TELEMETRY t=60s: 30.1 fps | p50 latency 52.2 ms | frames 1805
```
Verification block:
```
T-001 verification OK
```
(09:25, after quitting the app. Every check in the block passed: app bundle, doc with fps and
latency, compiled model, app not running, Opal C1 listed.)

### Open items for Review
- **Discovery blocker.** A discovery-only probe (`opal_list_devices`, which boots nothing on the
  camera) reports `mxid=14442C1091BB99D600 state=2 usable=0`. Here state 2 is `BOOTED`. USB ID
  `0x03e7:0xf63b`, 5 Gb/s. Same result after Chris replugged the camera (09:0x), and again
  after he moved it off his external USB 3.1 hub onto a port on the Mac (09:11; location
  `0x03210000`), so the hub is ruled out. Upstream's
  `OpalBridge.cpp:175–177` accepts only `FLASH_BOOTED`, `UNBOOTED` and `BOOTLOADER`, and its
  header labels `FLASH_BOOTED` as "stock Opal firmware". So this C1's stock firmware presents
  differently from the camera upstream built against. Getting past this needs a code change,
  which is outside T-001's scope, so it goes back to Chris. **→ Resolved by T-006** (the
  watcher found a ~5 s BOOTLOADER window at power-on; its bootloader reports 0.0.0).
- The streaming run used T-006's bridge, so "upstream as-is" was never streamed on this camera,
  and can't be. Review both tickets together.
- ~~Numbers were a human read~~: the pasted telemetry line above (30.1 fps, 52.2 ms) agrees with
  Chris's toolbar read (30 fps, 54 ms).

### Failed attempts
- ATTEMPT 1 [L1]: xcodebuild before first-launch → "failed to load a required plug-in …
  CoreSimulator" → fixed with `xcodebuild -runFirstLaunch`.
- ATTEMPT 2 [L1]: xcodebuild → "cannot execute tool 'metal' due to missing Metal Toolchain" →
  fixed with `xcodebuild -downloadComponent MetalToolchain`.
- (bootstrap run 1 died with "You have not agreed to the Xcode license agreements" when Xcode
  became active mid-build. That was a licence gate, not a fix failure; run 2 exit 0.)

## Review

join-verify: exit 0 @ b88d28d (bash)

Review, 2026-09-26 ~09:57. T-001's own diff is docs only (`docs/BUILD-mudmini.md`, the ticket),
so the effort is **low**. The `/code-review` passes run for T-006 (high ×3) covered the whole
working tree, this doc included.

### Verification limitations
- `~/Code/scripts/stage-verify` → `STAGE-VERIFY: FAIL (exit 1)` on the first line
  (`test -d build/DerivedData/…/OpenOpal.app`): the sandbox copies tracked files only, and the
  app build is gitignored and machine-local (Charter: "main checkout, not a worktree"). Run with
  `bash` from the checkout: `T-001 verification OK`, exit 0 (09:56, after the final app run).

### Acceptance criteria
- `xcodebuild … build` exit 0, `OpenOpal.app` under `build/DerivedData/…/Release/`: met.
- Streams from the C1, with a telemetry line after 60 s: met. `30.1 fps | p50 latency 51.2 ms`
  (T-006 run 6, same bridge); final Release app streamed at 09:56:08 (log: `streaming … from
  IMX378`, 10.7 s after `booting pipeline`), with Chris confirming. Earlier toolbar read: 30 fps · 54 ms.
- After quitting, "Opal C1 (ctrl)" is back: met (6–7 s across runs).
- `docs/BUILD-mudmini.md` with commands and numbers: met (39 lines).
- § Verification: met (above).

### Findings
- P3: `worklog.md` is in Files touched but not yet appended. → /deploy writes it.
- P3: `docs/BUILD-mudmini.md` still shows the 52.2 ms run-4 figure; run 6's is 51.2 ms. Same
  result, both real measurements; leave it.
- P3, doc impact (Plan): `docs/brief.md` Status, § Context "ninja, xcodegen, Xcode missing", and
  § Hardware facts (sensor IMX582 vs the reported IMX378; f63b / 0.0.0) are stale. → /deploy.
- Out-of-scope effect, justified: the streaming run needed T-006's bridge; "upstream as-is"
  can't stream on this camera. Recorded in the Build log.

Silencing scan: none (the verification block was only tightened at scope). Principles: all
held; `verification.measure-first` is met by the pasted telemetry line.

**Verdict: clear** — clear for /deploy together with T-006 (T-001's streaming depends on it)

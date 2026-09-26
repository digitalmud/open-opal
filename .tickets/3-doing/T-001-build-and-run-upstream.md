---
id: T-001
type: ticket
title: Build and run upstream Open Opal on mudmini
status: doing
priority: high
tags: [build, baseline, xcode]
created: 2026-09-26
updated: 2026-09-26
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
- 

### Verification output
- 

### Open items for Review
- 

### Failed attempts
- 

## Review

_Review stage fills this in._

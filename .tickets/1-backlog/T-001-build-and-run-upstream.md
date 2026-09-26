---
id: T-001
type: ticket
title: Build and run upstream Open Opal on mudmini
status: backlog
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
| Xcode source | App Store via `mas`; fall back to Chris installing by hand |
| Build config for daily use | Release (upstream: Debug stutters) |
| Virtual camera in this ticket | No — unsigned builds can't load it; that is T-005 |

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

_Scope stage fills this in._

## Plan

_Scope stage fills this in._

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

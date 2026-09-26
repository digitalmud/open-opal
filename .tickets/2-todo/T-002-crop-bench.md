---
id: T-002
type: ticket
title: Crop bench — measure four on-camera crop pipelines against the follow-mode budget
status: todo
priority: high
tags: [depthai, bridge, spike, pipeline]
created: 2026-09-26
updated: 2026-09-26
kind: feature
depends_on: [T-001]
release: c1-follow
sprint: 1
---

## Description

Follow mode needs the camera to take a **movable window** out of the sensor frame and send
1920×1080 over USB at call quality, so zooming in costs no sharpness. The original draft of this
ticket bet on one design (`ImageManip` cropping the full 4K frame). Prior art on the same camera
(LCM48) says that path is slow: jtannahill measured **5.9 fps** for "4K + on-device ImageManip crop
+ YuNet" and abandoned on-device cropping (their README § "What the frame rate costs";
`interview.py` docstring). So this ticket is a **measurement bench**. It builds four candidate
pipelines into the bridge behind a config value, measures each the same way on camera 3, and
recommends one. **Chris picks the design** from the numbers; T-003 then wires the chosen one into
the app. This is the design gate for the release.

## What to build

**Bridge, `Sources/OpalBridge/` (new code in new files, per CLAUDE.md "stay upstream-friendly"):**

- `include/OpalBridge.h`: an `OpalCropMode` enum (`OPAL_CROP_NONE = 0`, `OPAL_CROP_MANIP_4K`,
  `OPAL_CROP_MANIP_1440`, `OPAL_CROP_WINDOW_1080`, `OPAL_CROP_WINDOW_1440`); a new last field
  `OpalCropMode cropMode;` on `OpalPipelineConfig` (a zero-initialised config stays upstream's);
  `void opal_set_crop(OpalDeviceHandle*, float x, float y, float w, float h)` and
  `bool opal_get_crop(OpalDeviceHandle*, float* x, float* y, float* w, float* h)`, with the rect
  normalised to the full sensor frame.
- New `CropPipeline.hpp/.cpp` (namespace `opal`): `buildCropBranch(pipeline, cam, xout, mode)`,
  which wires the variant below and returns the `XLinkIn "cropcfg"` it created; and
  `makeCropConfig(mode, rect)`, which clamps the rect to the mode's zoom range, keeps 16:9, never
  upscales, and returns the `dai::ImageManipConfig` to send.
- `OpalBridge.cpp` (small, commented edits): in `opal_open`, `if(cfg.cropMode != OPAL_CROP_NONE)`
  call `opal::buildCropBranch` instead of the ISP-scale lines and `cam->video.link(xout->input)`,
  which stay exactly as they are for mode 0. The handle gains the crop queue, the mode and the
  current rect. `opal_set_crop`/`opal_get_crop` are thin wrappers (no-op + one log line in mode 0).
- `CMakeLists.txt`: add `CropPipeline.cpp`.

| Mode | Pipeline | Crop moves via | Zoom range |
|---|---|---|---|
| 0 `NONE` (baseline) | upstream: ISP 1/2 scale, `video` → XLinkOut | — | 1× |
| A `MANIP_4K` | ISP 4K, no scale; `isp` → `ImageManip` (crop + resize 1920×1080, NV12) → XLinkOut | `ImageManip.inputConfig` | 1.0–2.0× |
| B `MANIP_1440` | ISP `setIspScale(2,3)` = 2560×1440; `isp` → `ImageManip` → 1920×1080 | `ImageManip.inputConfig` | 1.0–1.33× |
| C `WINDOW_1080` | ISP 4K, `setVideoSize(1920,1080)`; `video` → XLinkOut | `ColorCamera.inputConfig`, `setCropRect(x, y, 0, 0)` (depthai example `rgb_camera_control.cpp:124`) | 2× fixed, pan only |
| D `WINDOW_1440` | ISP 4K, `setVideoSize(2560,1440)`; `video` → XLinkOut (host would zoom 1.5–2× by downscaling) | `ColorCamera.inputConfig` | 1.5–2.0× (host), pan |

All variants: one frame rate (30) everywhere; every `ImageManip` input non-blocking, queue size 1;
`setMaxOutputFrameSize` sized to the output; `setNumFramesPool(4)`; NV12 out, so the host frame
path is untouched.

**Bench, `Sources/OpalBridge/test/crop_bench.c` (committed; built with the clang line in
§ Verification, because the CMake test option is broken, see T-009):**
- Picks the usable C1 (camera 3 is `FLASH_BOOTED`, so it opens with no replug); for each mode in
  order 0, A, B, C, D: open → **discard the first 5 s** (queue drain) → **static** run of 60 s
  → **moving** run of 60 s, calling `opal_set_crop` once per received frame along a smooth path
  (a sine pan across the mode's range plus a zoom oscillation where the mode allows it) → close
  → wait for the camera to be usable again → next mode. `--modes` limits which modes run.
- Measures from its own frame callback, not the bridge's 60-sample rolling telemetry: delivered
  fps = frames / elapsed, and p50 / p95 latency over all frames. Prints one `RESULT` line per
  mode and run.
- Saves one small grey thumbnail (Y plane, 320 px wide, PGM) per mode at two crop positions under
  `build/crop-bench/` (gitignored), so the review can see the window really moved.

**Results, `docs/crop-bench.md` (new):** the table (mode, static fps/p50/p95, moving fps/p50/p95,
zoom range, pass/fail against the budget), the exact bench command, the camera serial, and a
`## Recommendation` section.

### Files touched

| Path | Action |
|---|---|
| `Sources/OpalBridge/include/OpalBridge.h` | edit: enum, one config field, two functions |
| `Sources/OpalBridge/OpalBridge.cpp` | edit: handle fields, one branch in `opal_open`, two wrappers |
| `Sources/OpalBridge/CropPipeline.hpp` / `.cpp` | new |
| `Sources/OpalBridge/CMakeLists.txt` | edit: one source |
| `Sources/OpalBridge/test/crop_bench.c` | new |
| `docs/crop-bench.md` | new |

Total: 7 files, ~400 LOC (the bench is about half). **No Swift or UI changes.**

## Charter decisions (locked 2026-09-26)

| Decision | Choice |
|---|---|
| Ticket shape | Bench first; Chris picks the design from the numbers (his answer, 2026-09-26 ~10:25). App wiring, spike keys and focus/exposure-region mapping move to T-003 |
| Camera | Camera 3, serial `19443010A1DA5F1300` (LCM48, f63d, bootloader 0.0.15): Chris's target and daily webcam |
| Camera time | One full bench run takes it for about 12 min. **Ask Chris before each full run** (it's his daily webcam); quit promptly after |
| Measurement method | 5 s drain, then 60 s static and 60 s moving per mode, at 30 fps; fps and latency from the bench's own callback (jtannahill: "short measurements lie") |
| Budget that passes | Moving crop: delivered ≥28 fps **and** p50 latency ≤90 ms |
| Recommendation rule | Among passing modes, prefer the widest real zoom range: A > D > B > C. Ties go to lower latency. The recommendation is mine, the pick is Chris's |
| If nothing passes | Record the numbers, recommend the least-bad mode or a rethink, report to Chris; T-003 waits |
| Why no `Warp` variant | `Warp`'s runtime `inputConfig` is commented out in depthai v2.30 (`Warp.hpp:39`), so its window can't move |
| depthai version | Stay on v2.30.0. If a variant is broken in it, record the evidence; don't bump |

## Acceptance criteria

- [ ] Mode 0 builds byte-for-byte upstream's pipeline (upstream lines still present; see § Verification)
- [ ] Every mode opens on camera 3 and streams; the bench prints static and moving `RESULT` lines for all five
- [ ] Thumbnails show the window moving for modes A–D (checked by eye; human-assisted, named in the Build log)
- [ ] `docs/crop-bench.md` carries the table for all five modes and a recommendation
- [ ] The app still builds; the working app (mode 0) still streams on camera 3 (Build log)
- [ ] § Verification block passes

## Verification

```bash
set -euo pipefail
S=$(mktemp -d "${TMPDIR:-/tmp}/t002.XXXXXX") || { echo "SCRATCH FAILED"; exit 1; }
H=Sources/OpalBridge/include/OpalBridge.h
C=Sources/OpalBridge/OpalBridge.cpp
# the new API and all four variants exist
grep -q 'OPAL_CROP_WINDOW_1440' "$H"
grep -q 'opal_set_crop' "$H"
grep -q 'opal::buildCropBranch' "$C"
grep -q 'setVideoSize' Sources/OpalBridge/CropPipeline.cpp
grep -q 'ImageManip' Sources/OpalBridge/CropPipeline.cpp
# mode 0 keeps upstream's exact lines
grep -q 'cam->setIspScale(cfg.ispNum, cfg.ispDen);' "$C"
grep -q 'cam->video.link(xout->input);' "$C"
# never flash: no flash/config bootloader request, no flash-named call, in any bridge source
if grep -rn -E 'request::(UpdateFlash|UpdateFlashEx|UpdateFlashEx2|UpdateFlashBootHeader|SetBootloaderConfig|BootMemory|BootloaderMemory)' Sources/OpalBridge; then echo "forbidden bootloader request"; exit 1; fi
if grep -rn -E '\b[A-Za-z_]*[Ff]lash[A-Za-z_]*\s*\(' Sources/OpalBridge; then echo "flash-named call"; exit 1; fi
# bridge, bench and app all build
export CMAKE_POLICY_VERSION_MINIMUM=3.5
cmake -S Sources/OpalBridge -B "$S/bridge" -G Ninja -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build "$S/bridge" >/dev/null
clang -ISources/OpalBridge/include Sources/OpalBridge/test/crop_bench.c -L"$S/bridge" -lOpalBridge -o "$S/crop_bench"
xcodebuild -project OpenOpal.xcodeproj -scheme OpenOpal -configuration Release -derivedDataPath build/DerivedData build -quiet
# results recorded for every mode, with a recommendation
for m in NONE MANIP_4K MANIP_1440 WINDOW_1080 WINDOW_1440; do grep -q "| $m" docs/crop-bench.md; done
grep -q '^## Recommendation' docs/crop-bench.md
echo T-002 verification OK
```

Hardware part (human-assisted, not in the block): the full bench run on camera 3 (Chris okays the
camera time), the thumbnail check, and one app launch in mode 0 to confirm nothing regressed.

## Constraints

- **Blast radius:** bridge sources, one new test program, one new doc. No Swift, render, matte or
  virtual-camera changes. Camera RAM only.
- **Permission class:** ship freely; camera time is asked for per run (Charter).
- **Read/write boundaries:** never flash; the bench closes the device on every exit path; quit it
  promptly; don't touch `scripts/bootstrap.sh`'s depthai pin.

## Out of scope

- Swift/UI, spike keys, focus/exposure-region mapping through the crop (T-003, after the pick).
- Face detection or automatic movement (T-003). Host-side zoom for variant D is only estimated here.
- The IMX378 units (T-008 territory); T-009's CMake fix.

## Notes

- 2026-09-26 (from T-001/T-006): depthai reports this C1's sensor as **IMX378**, not the brief's
  IMX582. Check the real sensor modes and 4K fps before sizing the crop. Opening this camera needs
  a replug with the app searching (`docs/BUILD-mudmini.md`). Baseline at 1080p ISP-downscale:
  30.1 fps, p50 51–52 ms (`opal_get_telemetry`).
- 2026-09-26: a second C1 (serial `…10C192A5D200`) is identical (f63b, bootloader 0.0.0, IMX378;
  30.1 fps · 49 ms on a Mac port).
- 2026-09-26: a third C1 (serial `1944…3010A1DA5F1300`) is upstream's kind: f63d, bootloader 0.0.15,
  LCM48 (48 MP); no replug needed, first frame ~3 s, 30.0 fps · 46 ms. **Chris chose camera 3 as the
  target** (brief § Decisions): T-002 is measured on the LCM48. It's also his daily webcam now, so
  quit test programs promptly.

- depthai v2.30 API checked at scope: `ImageManipConfig::setCropRect/setResize/setFrameType`,
  `ImageManip.inputConfig/inputImage/out`, `ColorCamera.isp/video/inputConfig`, `setVideoSize`,
  `setIspScale` all exist. jtannahill's traps (README § 2–3): mixed frame rates and blocking
  inputs stall every branch; short runs overstate throughput.
- Sensor is upside down; upstream rotates on the ISP (`setImageOrientation`) which happens
  before `isp` output, so the crop rect is in already-rotated coordinates.

---

## Principles in scope

`core.filter-first` (measure before wiring the app) · `verification.measure-first` · `verification.run-it` · `verification.behavioral-gap` (hardware and thumbnail checks labelled human-assisted) · `core.problem-boundary` (no Swift; T-009 not folded in) · `core.one-question` (Chris picks one design, afterwards) · `security.least-privilege` (never flash; greps enforce) · CLAUDE.md "stay upstream-friendly" (new code in new files)

## Plan

Scoped 2026-09-26 ~10:25 in the session, after the three-camera survey. Chris approved the bench
shape. Ticket moves `1-backlog → 2-todo` at this commit.

1. Read `OpalBridge.cpp` `opal_open` and the handle struct whole before editing (build discipline).
   Write `CropPipeline.{hpp,cpp}`; then the header enum/field/functions; then the small
   `opal_open` branch and wrappers. Compile with § Verification's cmake lines after each step.
2. Write `crop_bench.c`. First a short smoke run, **modes 0 and C only, 10 s each** (ask Chris for
   ~1 min of camera time), to prove the harness and the cheapest variant before the full run.
3. Ask Chris for ~12 min of camera time; run the full bench (all five modes, 60 s static + 60 s
   moving). Save stdout to `build/crop-bench/run-<time>.log`. Look at the thumbnails.
4. Write `docs/crop-bench.md` (table, command, serial, recommendation by the Charter rule).
5. Rebuild the Release app and launch it once in mode 0 on camera 3 (regression check), quit.

**Risks known at scope:** A may be far below budget (jtannahill: 5.9 fps with an NN). If
`ImageManip` refuses NV12 at 4K input in v2.30, record the error for A/B and carry on with C/D.
`setVideoSize` larger than 1920×1080 (mode D) might be refused by the ISP; the same rule applies.

**Doc impact for /deploy:** `docs/brief.md` § Context ("Why the crop must happen on the camera"
names ImageManip as the design) and § Decisions (the pick); release manifest T-002 line (new
title) and T-003's line/scope (absorbs the app wiring).

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

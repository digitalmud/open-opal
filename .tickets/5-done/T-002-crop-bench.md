---
id: T-002
type: ticket
title: Crop bench — measure four on-camera crop pipelines against the follow-mode budget
status: done
priority: high
tags: [depthai, bridge, spike, pipeline]
created: 2026-09-26
updated: 2026-09-26
closed: 2026-09-26
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
| Camera time, this session | **Waived by Chris 2026-09-26 10:22** ("Not using it at all. Go nuts."): bench runs go ahead without asking, for this session only. Still quit promptly |
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

- 2026-09-26 close: bench of 4 crop pipelines on camera 3; ImageManip fails when the window moves (15–21 fps); the ISP's own moving window holds 30 fps. Chris picked D (1440p window + Mac zoom); T-003 re-scopes around it.
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
- BASELINE: 1 failure. `grep -q 'OPAL_CROP_WINDOW_1440'` (nothing built yet; expected).
- `Sources/OpalBridge/CropPipeline.hpp/.cpp` (new): `buildCropBranch` wires modes A–D;
  `applyCropConfig` clamps a requested window to the mode's range (16:9, inside the frame, never
  an upscale) and fills an `ImageManipConfig`; `cropModeName`. Every runtime input is
  non-blocking, depth 1.
- `include/OpalBridge.h`: `OpalCropMode` enum, `cropMode` as the last field of
  `OpalPipelineConfig`, `opal_set_crop`, `opal_get_crop`.
- `OpalBridge.cpp` (+51/−2): include; four handle fields; upstream's
  `cam->video.link(xout->input);` now sits inside `if(cfg.cropMode == OPAL_CROP_NONE)` with the
  crop branch as the else; the crop queue opens depth 1, non-blocking; `cropQ.reset()` in
  `opal_close`; the two wrappers. Upstream's `setIspScale` line is untouched; crop modes override
  the scale inside `buildCropBranch`.
- `CMakeLists.txt`: + `CropPipeline.cpp`.
- `test/crop_bench.c` (new): as specified, plus a `--rate` option (see decisions).
- `docs/crop-bench.md` (new): method, table, recommendation, reproduce commands.
- In-latitude decisions:
  - `makeCropConfig(mode, rect)` became `applyCropConfig(mode, want, cfg&)`:
    `dai::ImageManipConfig` can't be copy-assigned (compile error: "copy assignment operator is
    implicitly deleted"), so the node's `initialConfig` has to be filled in place.
  - The bench needs `-Wl,-rpath,$PWD/vendor/install/lib` to run, as upstream's own CMake test
    targets do. The Verification block only compiles it, so it's unaffected.
  - A supplementary run: A with the window moved 10×/s (`--rate 10`), to test whether a slower
    servo could rescue ImageManip. It can't (21.8 fps). Labelled supplementary; the pass mark
    stays per-frame (`core.filter-first`: measured before any app wiring).
  - The "crop mode" boot-log line was moved after the device opens, because it printed a stale timestamp.
- Camera time: Chris waived per-run asks for this session ("Not using it at all. Go nuts.",
  10:22). Smoke test 10:26, full bench 10:27–10:37, supplementary 10:38, app check 10:41.

### Verification output
```
T-002 verification OK
```
(10:41, from the checkout; xcodebuild's "multiple matching destinations" notice is harmless.)

Bench (`build/crop-bench/run-1027.log`, camera 3, 60 s runs after a 5 s drain):
```
RESULT mode=NONE run=static frames=1802 secs=60.1 fps=30.01 p50_ms=49.7 p95_ms=53.3 out=1920x1080
RESULT mode=MANIP_4K run=static frames=1802 secs=60.1 fps=30.00 p50_ms=49.9 p95_ms=54.1 out=1920x1080
RESULT mode=MANIP_4K run=moving frames=908 secs=60.0 fps=15.13 p50_ms=123.7 p95_ms=226.9 out=1920x1080
RESULT mode=MANIP_1440 run=static frames=1803 secs=60.1 fps=30.01 p50_ms=49.9 p95_ms=54.2 out=1920x1080
RESULT mode=MANIP_1440 run=moving frames=1273 secs=60.0 fps=21.22 p50_ms=106.3 p95_ms=135.7 out=1920x1080
RESULT mode=WINDOW_1080 run=static frames=1802 secs=60.0 fps=30.01 p50_ms=49.9 p95_ms=54.5 out=1920x1080
RESULT mode=WINDOW_1080 run=moving frames=1800 secs=60.0 fps=30.00 p50_ms=49.9 p95_ms=53.8 out=1920x1080
RESULT mode=WINDOW_1440 run=static frames=1802 secs=60.1 fps=30.00 p50_ms=57.7 p95_ms=61.4 out=2560x1440
RESULT mode=WINDOW_1440 run=moving frames=1800 secs=60.0 fps=30.00 p50_ms=57.8 p95_ms=61.1 out=2560x1440
```
Supplementary (`run-supp-1038.log`): `RESULT mode=MANIP_4K run=moving@10Hz frames=1310 secs=60.0
fps=21.83 p50_ms=66.3 p95_ms=155.4`.

Human-assisted checks:
- **Thumbnails** (checked by eye, `build/crop-bench/*.png`): top-left windows show the window blind
  and bottom-right windows show Chris's face, at 2× (A, C), 1.33× (B, clamped) and 1.5× (D). All
  upright. The crops are real, so A's fast still-window result is genuine.
- **App regression (mode 0)**, final Release build, camera 3, 10:41: `booting pipeline` →
  `streaming … from Sony IMX582 (48MP)` in 3.2 s; quit; stock webcam listed again ~8 s later (as
  "Opal C1", USB `…f63d`, not "Opal C1 (ctrl)").

### Build round 2 (after review 1, 10:44)
- `opal_set_crop`: apply, send and record now happen under one `cropMutex` hold (the send never
  blocks).
- `applyCropConfig(mode, want, cfg, fallback)`: a request with any non-finite value is replaced
  by `fallback` (the handle passes its current window).
- `crop_bench.c`: SIGINT/SIGTERM set a flag; every timing loop checks it; the open device is
  closed and marked `closed (interrupted)`; a run that never started isn't reported. A
  "WARNING … buffer full" line prints when latency samples exceed 8192.
- Failure paths tested on camera 3 (`verification.negative-path`):
  - SIGINT 12 s into a 60 s bench (`build/crop-bench/sigint-test.log`): `closed (interrupted)`,
    exit 0, stock webcam back after 14 s, no bench process left.
  - A throwaway NaN probe (mode C): `valid request -> 0.100 0.200 0.500 0.500` then
    `NaN request -> 0.100 0.200 0.500 0.500 (kept previous: OK)`.
- Verification re-run after the fixes (10:47): `T-002 verification OK`, exit 0. The measured
  numbers are unaffected: the fixes touch only failure paths and the bench's reporting.

### Open items for Review
- **Recommendation: D (WINDOW_1440)**, per the Charter rule; **the pick is Chris's.** Caveats for
  T-003 are in `docs/crop-bench.md`: no 1.0× view while following, host zoom not yet measured,
  region mapping.
- `bridge_test.c`-style CMake test targets are still broken (T-009), so `crop_bench` isn't wired
  into CMake.
- The ticket text still says `makeCropConfig` in § What to build; the code uses
  `applyCropConfig` (reason above).

### Failed attempts
- none (the `ImageManipConfig` copy-assignment compile error and the bench's missing rpath were
  fixed on the first try; the reasons are recorded above)

## Review

join-verify: exit 0 @ c2f17d0 (bash)

Review 1, 2026-09-26 10:44, of the working tree on `c2f17d0`. `/code-review` at **medium** (runtime
code, single app; no bootloader path touched).

### Verification limitations
- `~/Code/scripts/stage-verify` → `STAGE-VERIFY: FAIL (exit 1)`: CMake can't find depthai in the
  sandboxed copy (`vendor/install` is gitignored and machine-local; the same limit as T-001/T-006).
  Run with `bash` from the checkout: `T-002 verification OK`, exit 0 (10:41).

### Findings
- **P2**: `OpalBridge.cpp` `opal_set_crop`: `cropQ->send` is outside `cropMutex`, so two concurrent
  callers can leave `opal_get_crop` reporting a window the camera isn't showing. → round 2.
- **P2**: `CropPipeline.cpp` `applyCropConfig`: no guard against NaN/inf; `std::clamp(NaN, …)`
  returns NaN, which would go to `setCropRect`. → round 2.
- **P2**: `crop_bench.c`: no SIGINT/SIGTERM handling, so Ctrl-C skips `opal_close`. That breaks the
  ticket constraint "the bench closes the device on every exit path". → round 2.
- P3: `crop_bench.c`: the 8192-sample latency buffer silently truncates runs over ~273 s. → round 2
  (warn when samples are dropped).
- Checked clean by the reviewer: mode 0 unchanged; ISP scales and output sizes per mode; window
  corner math; no upscale; 16:9 kept; queue settings; `opal_close` order; bench thumbnail bounds,
  p95 index, thread handoff.

Silencing scan: none. Files: all in the ticket's Files touched. Principles: held.

Round result: back-to-build — four fixes (three P2, one P3) chosen over deferral; together ~18 lines, over the fix-in-place limit

---

join-verify: exit 0 @ c2f17d0 (bash)

Review 2, 2026-09-26 10:48, of build round 2. `/code-review` at **low** (four small failure-path
fixes). Verification with `bash` from the checkout: `T-002 verification OK`, exit 0 (10:47);
`stage-verify` not applicable (see review 1).

### Verification limitations
- At low effort `/code-review` skips test files, so the `crop_bench.c` interrupt/warning changes
  weren't independently reviewed. Evidence instead: the hardware negative test (SIGINT mid-run →
  `closed (interrupted)`, exit 0, webcam back in 14 s) and the author's read.

### Findings
- None in the bridge fixes: one `cropMutex` hold now covers apply + send + record, and
  `opal_get_crop` takes the same lock; a non-finite request falls back to the current window
  (hardware probe: "kept previous: OK").

### For Chris (the decision this ticket exists for)
- Recommendation **D (WINDOW_1440)**: 30.0 fps, p50 58 ms with the window moving every frame;
  pans anywhere; the Mac zooms 1.5–2.0× by downscaling. No whole-room view while following.
  C is the lower-latency fallback (2× fixed, pan only). A and B fail the budget. Details:
  `docs/crop-bench.md`. The pick is recorded at /deploy.

**Verdict: clear** — review-1 fixes verified (two by forcing the failure on camera 3); recommendation D awaits Chris's pick

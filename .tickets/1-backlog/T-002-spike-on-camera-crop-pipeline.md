---
id: T-002
type: ticket
title: Spike — on-camera 4K→1080p crop pipeline with a movable window
status: backlog
priority: high
tags: [depthai, bridge, spike, pipeline]
created: 2026-09-26
updated: 2026-09-26
depends_on: [T-001]
release: c1-follow
sprint: 1
---

## Description

Follow mode only works if the camera can crop a window out of the full 4K ISP frame and
resize it to 1920×1080 *before* the frame crosses USB, at call-quality frame rate and latency.
Upstream today asks the ISP to downscale the whole frame (`setIspScale(1,2)`), which is fast
but throws away the resolution a zoom needs. This ticket adds a second pipeline shape behind a
config flag and measures it. It is the design gate for T-003: if the numbers miss the budget,
we stop and rethink before building the servo.

## What to build

**Bridge (C++), `Sources/OpalBridge/`:**

- `OpalPipelineConfig` gains `bool cropPipeline;`. When true: `ColorCamera` stays at THE_4_K
  with **no** ISP scale; add `dai::node::ImageManip manip` with
  `initialConfig.setCropRect(0.25, 0.25, 0.75, 0.75)` (a centred 2× window),
  `initialConfig.setResize(1920, 1080)`, `initialConfig.setFrameType(NV12)`,
  `setMaxOutputFrameSize(1920*1080*3/2)`, `setNumFramesPool(4)`; link `cam->isp → manip.inputImage`
  and `manip.out → xout "video"`. Add `XLinkIn "cropcfg"` linked to `manip.inputConfig`. When
  false the pipeline is byte-for-byte upstream's.
- New C entry point `void opal_set_crop(OpalDeviceHandle*, float x, float y, float w, float h)`
  — normalized rect in the *full ISP frame*; clamps to [0,1], enforces `w/h == 16/9`,
  `w ≥ 0.5` (never upscale) and sends an `ImageManipConfig` (`setCropRect` + `setResize`)
  on the `cropcfg` queue. Coalesce like `opal_set_controls`: at most one config per frame.
- Store the live crop rect on the handle; `opal_set_focus_region` and
  `opal_set_exposure_region` map their input rect (output-frame normalized) through it before
  converting to sensor coordinates, so tap-to-focus and subject metering keep pointing at the
  right pixels while cropped. When `cropPipeline` is false the mapping is identity.
- `opal_get_crop(h, float* x, y, w, h)` for the host and the UI.
- Header comments in the style of the existing ones: say *why* (bandwidth, no-upscale rule).

**Swift:**

- `CameraSettings`: `var followEnabled = false` as a **cold** setting (`coldDirty`), and
  `OutputMode` gains nothing — follow implies 1080p output.
- `OpalDevice.connect` sets `cfg.cropPipeline = settings.followEnabled`; expose
  `setCrop(_ rect: CGRect)` and `currentCrop`.
- A temporary debug affordance for this spike only: with follow on, arrow keys nudge the crop
  by 2 % and `+`/`-` change the zoom between 1.0× and 2.0×. Lives in `ContentView` behind
  `#if DEBUG`-free plain code but clearly marked `// T-002 spike control — replaced by T-003`.

**Measurement:** with the crop pipeline on, at 30 fps, record after 60 s: fps and p50 latency
from telemetry; the same for a crop rect being moved every frame for 30 s (dynamic-config
cost, see depthai-core issue #529). Compare with T-001's baseline.

### Files touched

| Path | Action |
|---|---|
| `Sources/OpalBridge/include/OpalBridge.h` | edit (config flag, 2 functions) |
| `Sources/OpalBridge/OpalBridge.cpp` | edit (pipeline branch, crop queue, region mapping) |
| `Sources/OpenOpal/Camera/CameraSettings.swift` | edit |
| `Sources/OpenOpal/Camera/OpalDevice.swift` | edit |
| `Sources/OpenOpal/UI/ContentView.swift` | edit (spike keys) |
| `docs/BUILD-mudmini.md` | new (T-001 creates it; append the measurement table if it already exists) |

Total: 6 files, <250 LOC.

## Charter decisions (locked 2026-09-26)

| Decision | Choice |
|---|---|
| Where the crop happens | `ImageManip` on the VPU fed by the unscaled ISP output. Not host-side (4K over USB is ~300 ms). Not `ColorCamera.video` crop offsets (fixed size; no zoom). |
| Zoom range | 1.0× (full frame) to 2.0× (1920×1080 window). Never upscale. |
| Aspect | Always 16:9; `opal_set_crop` corrects a non-16:9 request by shrinking the long side |
| Budget that passes the gate | ≥28 fps and p50 latency ≤90 ms with the crop moving every frame at 30 fps |
| If the budget is missed | Stop. Record numbers; try `ImageManip` on a `setIspScale(2,3)` 2560×1440 base (1.33× max zoom) as one fallback measurement; then report to Chris. T-003 does not start. |
| Frame type | NV12 in and out so `FrameSink.ingest` and the Metal path are untouched |

## Judgment calls — pre-decided

- Coalescing: reuse the bridge's existing control-queue pattern (one pending value, sent by
  the capture loop) rather than a new thread.
- The crop rect is authoritative on the host; the bridge only clamps and forwards.
- `opal_set_crop` with follow off is a no-op that logs once.

## Acceptance criteria

- [ ] With `followEnabled` off, pipeline construction code path is unchanged (diff shows the upstream branch intact)
- [ ] With follow on, the app streams 1920×1080 and arrow keys visibly move the window; `+`/`-` zoom between full frame and 2×
- [ ] Tap-to-focus at a point while zoomed 2× focuses on the object under the cursor (Chris or a visual check, noted in Build log)
- [ ] Build log carries the measurement table: static crop, moving crop, and T-001 baseline
- [ ] § Verification block passes

## Verification

```bash
set -euo pipefail
grep -q 'cropPipeline' Sources/OpalBridge/include/OpalBridge.h
grep -q 'opal_set_crop' Sources/OpalBridge/include/OpalBridge.h
grep -q 'ImageManip' Sources/OpalBridge/OpalBridge.cpp
grep -q 'cropcfg' Sources/OpalBridge/OpalBridge.cpp
grep -q 'followEnabled' Sources/OpenOpal/Camera/CameraSettings.swift
xcodebuild -project OpenOpal.xcodeproj -scheme OpenOpal -configuration Release \
  -derivedDataPath build/DerivedData build -quiet
grep -q -i 'moving crop' docs/BUILD-mudmini.md
echo T-002 verification OK
```

## Constraints

- **Blast radius:** bridge + three Swift files; no change to render, matte or virtual camera.
- **Permission class:** ship freely.
- **Read/write boundaries:** never flash; don't touch `scripts/bootstrap.sh`'s depthai pin
  unless `ImageManip` on 4K NV12 is broken in v2.30.0 (then record the evidence and stop).

## Out of scope

- Face detection or any automatic movement (T-003). UI beyond the spike keys (T-004).

## Notes

- depthai docs: ImageManip / ImageManipConfig (`setCropRect`, `setResize`), ColorCamera `isp`
  output; jtannahill/opal-c1-depthai ran five ImageManip crops off a 4K stream on this same
  hardware, so the node handles 4K input. Mixed frame rates stall the pipeline: keep one fps.
- Sensor is upside down; upstream rotates on the ISP (`setImageOrientation`) which happens
  before `isp` output, so the crop rect is in already-rotated coordinates.

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

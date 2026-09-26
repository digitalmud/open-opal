---
id: T-003
type: ticket
title: Follow plumbing — 1440p camera window + Mac zoom, view-rect API, focus mapping, hand-steered
status: todo
priority: high
tags: [follow, swift, videotoolbox, bridge]
created: 2026-09-26
updated: 2026-09-26
kind: feature
depends_on: [T-002]
release: c1-follow
sprint: 2
---

## Description

T-002 picked design D: the camera's ISP pans a 2560×1440 window over the 4K frame (30 fps,
p50 58 ms), and the Mac zooms 1.5–2.0× inside it by downscaling to 1920×1080. This ticket
builds that path into the app **without any face tracking**. A follow toggle switches the
pipeline, the Mac does the zoom, and temporary menu commands pan and zoom by hand, so the whole
chain can be measured and eyeballed before T-010 puts a face servo on it. Split from the
original T-003 at Chris's request, 2026-09-26 ~11:10.

The app will think in one **view rect**: the part of the full sensor frame the viewer sees,
normalised 0..1, always 16:9, between 1.5× (w = 2/3) and 2.0× (w = 1/2). A small pure-math layer
turns a view rect into (a) where the camera window goes and (b) what the Mac crops from inside it.
Focus and exposure regions map back through the same view rect.

**The one real risk:** frames don't say which window they were captured with (depthai v2.30's
`RawImgFrame` carries only size, stride and plane offsets). When the window moves, the Mac must
crop each frame against the window that frame actually had, or the picture jumps for a frame or
two. Every frame does carry its latency, so capture time = arrival − latency. If the camera applies
a window move after a steady delay Δ, the Mac can match each frame to its window. Step 1 measures Δ.

## What to build

**1. Measure the window-apply delay (bench, first):** `Sources/OpalBridge/test/crop_bench.c` gains
`--delay-probe`. In mode `WINDOW_1440` it settles, grabs a reference column-luma profile of the
frame at window position L (x = 0) and position R (x = 1/3), then jumps L↔R 60 times, 20 frames
apart. For each frame it classifies L or R by profile correlation, and for each jump it records
`capture time of the first frame showing the new position − send time`, where capture time is
arrival minus the frame's latency, on the same clock. It prints `DELAY` min / median / max ms, the
count of jumps where frames flipped back (mixed), and the classification margin.

**2. App plumbing (new code in new files):**
- `Sources/OpenOpal/Follow/FollowView.swift` (pure math, no UIKit/Metal):
  `struct ViewRect` (x, y, w; h ≡ w); `clamp(_:)` (zoom 1.5–2.0×, inside the frame);
  `windowFor(view:) -> CGRect` (the 2/3-size camera window, centred on the view, clamped to the
  frame); `hostCrop(view:window:) -> CGRect` (the view in the 2560×1440 window's pixel space,
  clamped inside it); `toFullFrame(_ outputPoint/outputRect:, view:)`.
- `Sources/OpenOpal/Follow/HostZoom.swift`: a `VTPixelTransferSession` with
  `kVTScalingMode_CropSourceToCleanAperture`. The crop is set as the source buffer's
  `kCVImageBufferCleanApertureKey` (offsets from centre; probe-verified at scope, see Plan), into a
  1920×1080 NV12 IOSurface/Metal-compatible `CVPixelBufferPool`. Keeps p50/p95 ms of its own work.
- `Sources/OpenOpal/Follow/FollowPipeline.swift`: thread-safe (lock) owner of the current view
  rect and a short history of `(sentAt, window)`. `setView(_:)` clamps, and when the window
  changes by more than 0.2 % sends it with `device.setWindow` and records it with its send time.
  `process(frame:latencyMs:)` picks the window in effect at the frame's capture time (per the Δ
  rule in the Charter), computes the host crop for the current view **against that window**, runs
  HostZoom and returns the 1080p buffer.

**3. Small edits to upstream files (commented):**
- `CameraSettings`: `var followEnabled = false`, a **cold** setting (`coldDirty`).
- `OpalDevice`: `cfg.cropMode = settings.followEnabled ? OPAL_CROP_WINDOW_1440 : OPAL_CROP_NONE`;
  `func setWindow(_ rect: CGRect)` → `opal_set_crop`. With follow on, `outputMode` is ignored
  (the output is always 1080p).
- `CameraModel`: when follow is on, the frame task passes each buffer through
  `followPipeline.process` before analysis and render (so matte, bokeh, preview and the virtual
  camera all see the zoomed 1080p, unchanged); `focus(at:)` and `meter(on:)` map through
  `FollowView.toFullFrame` with the current view.
- `OpenOpalApp` Camera menu: "Follow Mode" toggle (applies as a cold setting, as the Inspector's
  Apply does); temporary "Follow test" items, pan ←→↑↓ 5 % and zoom in/out 0.1×, marked
  `// T-003 hand steering — replaced by T-010`.

### Files touched

| Path | Action |
|---|---|
| `Sources/OpalBridge/test/crop_bench.c` | edit: `--delay-probe` |
| `Sources/OpenOpal/Follow/FollowView.swift` | new |
| `Sources/OpenOpal/Follow/HostZoom.swift` | new |
| `Sources/OpenOpal/Follow/FollowPipeline.swift` | new |
| `Sources/OpenOpal/Camera/CameraSettings.swift` | edit (+1 cold setting) |
| `Sources/OpenOpal/Camera/OpalDevice.swift` | edit (crop mode, `setWindow`) |
| `Sources/OpenOpal/CameraModel.swift` | edit (frame hook, region mapping) |
| `Sources/OpenOpal/OpenOpalApp.swift` | edit (menu items) |
| `Tests/FollowViewCheck/main.swift` | new: assertion checks for `FollowView`, built with `swiftc` |
| `docs/crop-bench.md` | edit: delay-probe results |

Total: 10 files, ~550 LOC. The bridge library itself is untouched (T-002's API is the contract).

## Charter decisions (locked 2026-09-26)

| Decision | Choice |
|---|---|
| Ticket shape | Plumbing only; the face servo is T-010 (Chris approved the split, ~11:10) |
| Mac-side zoom | `VTPixelTransferSession` on NV12 **before** the renderer; the renderer, matte, bokeh and virtual camera stay untouched (scope probe: p50 1.44 ms, correct crop) |
| Window-to-frame matching, **if the probe finds Δ steady** (max − min < 33 ms, the frame interval) | A frame whose capture time ≥ sendTime + (min+max)/2 uses the new window |
| …**if Δ is not steady** | Fallback: the window moves only when the view gets within 2 % of the window's edge, then sits still (host crop does all small moves; one possible glitch per recentre). Record which branch ran |
| Zoom range while following | 1.5× (w = 2/3) to 2.0× (w = 1/2); never upscale |
| Follow on + outputMode | outputMode is ignored while following; output is always 1920×1080. Its UI is T-004's |
| Where the view starts | Centred, 1.5× |
| Tests | A `swiftc`-built check program for the pure math (no XCTest target; keeps `project.yml` untouched and doesn't collide with upstream PR #1's test target) |
| Camera time | Camera 3 is free this session (Chris, 10:22) |

## Acceptance criteria

- [ ] The delay probe runs and `docs/crop-bench.md` records Δ min/median/max, the mixed-jump count and the chosen branch
- [ ] `FollowViewCheck` passes (§ Verification): zoom clamps, window contains view, host crop inside window, round-trip mapping
- [ ] With follow on in the app, hand steering pans and zooms the output. **Human-assisted:** Chris watches a pan + zoom sweep and confirms no visible jumps; verdict in the Build log
- [ ] Telemetry with follow on while panning: ≥28 fps, p50 latency ≤ 90 ms (the bridge's own number), plus HostZoom p50/p95 ms, pasted in the Build log
- [ ] Tap-to-focus while zoomed 2× on one side of the frame focuses on what's under the click (**human-assisted**: Chris, or a visible lens change on a near/far target)
- [ ] Follow off: pipeline is upstream's, 1080p, streams as before
- [ ] § Verification block passes

## Verification

```bash
set -euo pipefail
S=$(mktemp -d "${TMPDIR:-/tmp}/t003.XXXXXX") || { echo "SCRATCH FAILED"; exit 1; }
# the pieces exist
grep -q 'VTPixelTransferSessionCreate' Sources/OpenOpal/Follow/HostZoom.swift
grep -q 'kVTScalingMode_CropSourceToCleanAperture' Sources/OpenOpal/Follow/HostZoom.swift
grep -q 'OPAL_CROP_WINDOW_1440' Sources/OpenOpal/Camera/OpalDevice.swift
grep -q 'followEnabled' Sources/OpenOpal/Camera/CameraSettings.swift
grep -q 'toFullFrame' Sources/OpenOpal/CameraModel.swift
# the pure math is right
swiftc -O Sources/OpenOpal/Follow/FollowView.swift Tests/FollowViewCheck/main.swift -o "$S/followcheck"
"$S/followcheck"
# bench and app build
export CMAKE_POLICY_VERSION_MINIMUM=3.5
cmake -S Sources/OpalBridge -B "$S/bridge" -G Ninja -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build "$S/bridge" >/dev/null
clang -ISources/OpalBridge/include Sources/OpalBridge/test/crop_bench.c -L"$S/bridge" -lOpalBridge -o "$S/crop_bench"
xcodebuild -project OpenOpal.xcodeproj -scheme OpenOpal -configuration Release -derivedDataPath build/DerivedData build -quiet
# the delay measurement is recorded
grep -q '^## Window-apply delay' docs/crop-bench.md
# never flash
if grep -rn -E '\b[A-Za-z_]*[Ff]lash[A-Za-z_]*\s*\(' Sources/OpalBridge Sources/OpenOpal/Follow; then echo "flash-named call"; exit 1; fi
echo T-003 verification OK
```

Hardware and human parts (not in the block): the delay probe run, the live pan/zoom sweep with
Chris's no-jump verdict, telemetry, and tap-to-focus while zoomed.

## Constraints

- **Blast radius:** the app target's camera/model/menu code plus new `Follow/` files, the bench,
  and one doc. No bridge-library, renderer, matte, bokeh or virtual-camera changes.
- **Permission class:** ship freely.
- **Read/write boundaries:** never flash; quit the app and bench when done.

## Out of scope

- Face detection and automatic movement (T-010). Follow UI in the Inspector, framing presets,
  persistence (T-004). The IMX378 units.

## Notes

- Upstream frame flow: `OpalDevice.FrameSink.ingest` → `device.onFrame(pb, latencyMs)` →
  `CameraModel.start`'s detached task → `renderer.analyzeNow`/`render` → `exportFrame` → feeder.
  The renderer sizes itself from the buffer (`BokehRenderer.render`, `CVPixelBufferGetWidth`), which
  is why zooming to 1080p before it keeps everything downstream unchanged.
- Tap-to-focus arrives as an output-normalised point (`ContentView` → `camera.focus(at:)`); subject
  metering uses `SubjectInfo.bounds`, also output-normalised.
- Camera 3 appears in macOS as "Opal C1" (USB `f63d`) and opens with no replug.

---

## Principles in scope

`core.filter-first` (measure Δ before relying on it) · `verification.measure-first` · `verification.run-it` · `verification.behavioral-gap` (no-jump and focus checks are human-assisted, labelled) · `core.problem-boundary` (no servo, no Inspector UI) · `verification.recalled-not-verified` (the VT crop API was probed, not recalled) · CLAUDE.md "stay upstream-friendly" (new files; small commented edits) · CLAUDE.md "Regions are sensor coordinates" (mapping through the view rect)

## Plan

Scoped 2026-09-26 ~11:15. Chris approved the split; T-010 holds the servo.

**Probe run at scope (throwaway, in `$TMPDIR`, nothing left in the tree):** a Swift program made a
2560×1440 NV12 buffer with a horizontal luma gradient, attached a clean aperture (1920×1080 at
x = 640, offsets from centre), set `kVTPixelTransferPropertyKey_ScalingMode =
kVTScalingMode_CropSourceToCleanAperture`, and transferred to a 1920×1080 NV12 buffer 120 times:
`create: 0 · scaling mode: 0 · transfer status: 0 · per-frame ms: p50 1.44 p95 2.25 max 2.83 ·
dst luma left/right: 63 255 (expected ~64 / ~255)`. Note:
`kVTPixelTransferPropertyKey_SourceCropRectangle` does **not** exist in the Swift SDK; the crop is
the clean-aperture attachment.

1. Bench `--delay-probe`; run it on camera 3; write `## Window-apply delay` in
   `docs/crop-bench.md`; pick the Charter branch.
2. `FollowView.swift` + `Tests/FollowViewCheck/main.swift` (swiftc); iterate until the checks pass.
3. `HostZoom.swift`, `FollowPipeline.swift` (with the chosen branch), then the upstream-file edits.
4. Build; run the app with follow on; hand-steer; collect telemetry and HostZoom stats; Chris
   watches a sweep and tries tap-to-focus while zoomed.

**Doc impact for /deploy:** `docs/brief.md` § Context (host zoom and the Δ result); `docs/crop-bench.md`
(this ticket adds a section); the release manifest (T-003 title, T-010 line).

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

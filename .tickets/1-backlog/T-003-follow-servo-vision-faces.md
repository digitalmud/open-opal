---
id: T-003
type: ticket
title: Follow mode — Vision face servo drives the crop window
status: backlog
priority: high
tags: [follow, vision, servo, swift]
created: 2026-09-26
updated: 2026-09-26
depends_on: [T-002]
release: c1-follow
sprint: 2
---

## Description

With T-002's movable window in place, this ticket makes it follow Chris: detect the face on
each received 1080p frame with Apple Vision, and steer the crop so the face sits where a good
framing puts it, at a zoom that gives head-and-shoulders. This is the feature. It must feel
like Center Stage: calm, no hunting, no jitter, a gentle recentre when he moves, and a slow
return to the full frame when nobody is there.

## What to build

**New `Sources/OpenOpal/Follow/FollowController.swift`** (single file, ~200 LOC):

- Input: the `CVPixelBuffer` per frame (hook next to `device.onFrame` fan-out in
  `CameraModel.start`, off the main actor, on every 2nd frame), the current crop rect from
  `device.currentCrop`, and a `FollowSettings` snapshot (framing, speed).
- Detection: `VNDetectFaceRectanglesRequest` (revision 3) on the frame; take the union of all
  face boxes when several are found. Convert Vision's bottom-left normalized coords to
  top-left. Map from output-frame space into full-frame space through the crop rect that
  produced this frame (use the crop rect current at the time the frame arrived; a one-frame
  mismatch is tolerated by the loop).
- Target: face centre → crop centre at (0.5, 0.42) of the window (eyes on the upper third);
  zoom so face height ≈ framing preset: `tight` 0.30, `medium` 0.22, `wide` 0.16 of window
  height, clamped to the 1.0×–2.0× range.
- Dead zone: no move while the face centre is within ±4 % of target and face height within
  ±12 % of target.
- Smoothing: critically damped spring per axis (centre x, centre y, zoom) with a time
  constant from `speed`: `slow` 1.2 s, `normal` 0.7 s, `fast` 0.4 s. Zoom moves at half the
  translation speed so it reads as deliberate.
- Lost subject: hold the last window 2.0 s, then ease to the full frame over 3 s. On
  re-acquire, move at normal speed (no snap).
- Output: `device.setCrop(rect)` at most once per frame, only when the rect changed by more
  than 0.1 % (avoid flooding `cropcfg`).
- Also publish the face rect and the crop rect as observable state for the UI (T-004 draws
  them in the preview when follow is on).

**`CameraSettings`:** `followFraming: Framing = .medium`, `followSpeed: Speed = .normal`
(hot). `followEnabled` already exists (T-002).

**`CameraModel`:** own a `FollowController`; feed frames when `settings.followEnabled`;
route `meter(on:)` through the crop mapping so subject metering (upstream) and the follow
window agree. Remove T-002's spike key handling.

### Files touched

| Path | Action |
|---|---|
| `Sources/OpenOpal/Follow/FollowController.swift` | new |
| `Sources/OpenOpal/Camera/CameraSettings.swift` | edit |
| `Sources/OpenOpal/CameraModel.swift` | edit |
| `Sources/OpenOpal/UI/ContentView.swift` | edit (remove spike keys) |
| `project.yml` | edit only if the new folder isn't auto-globbed |

Total: ≤5 files, <300 LOC.

## Charter decisions (locked 2026-09-26)

| Decision | Choice |
|---|---|
| Detector | Vision face rectangles on the host, every 2nd frame. Not the person-segmentation subject box (too coarse, torso-heavy); not a VPU network (≈⅓ throughput). |
| Closed loop | Detect on the *cropped* output and correct relative error; never trust an absolute face position from a stale crop. |
| Multi-face | Union box. No speaker selection. |
| Feel parameters | As listed above; recorded so tuning in T-004 changes numbers, not design. |
| Return-to-wide | Yes, after 2 s hold + 3 s ease. Composer held the last frame forever; Center Stage widens. Chris wants Center Stage. |

## Judgment calls — pre-decided

- The spring runs on the frame clock (dt from frame timestamps), not a timer, so it stays
  smooth if fps changes.
- Vision requests run on a dedicated serial queue with one in flight; drop frames rather
  than queue.
- All state lives in `FollowController`; `CameraModel` only wires it. Testable without a camera:
  the servo math takes `(faceRect, crop, dt)` and returns a new crop.

## Acceptance criteria

- [ ] `FollowController` servo math is a pure function with unit tests: dead zone holds, spring converges without overshoot >2 %, zoom clamps at 1.0× and 2.0×, lost-subject easing reaches full frame within 5.5 s
- [ ] Live: Chris moves left, right, forward, back and out of frame; the window follows without visible jitter, and returns to full frame ≈5 s after he leaves (Chris's verdict in Build log, plus a 20 s screen recording path)
- [ ] Telemetry with follow on: ≥28 fps, p50 latency ≤90 ms, pasted in Build log
- [ ] Tap-to-focus still lands on the object under the cursor while following
- [ ] § Verification block passes

## Verification

```bash
set -euo pipefail
test -f Sources/OpenOpal/Follow/FollowController.swift
grep -q 'VNDetectFaceRectanglesRequest' Sources/OpenOpal/Follow/FollowController.swift
! grep -q 'T-002 spike control' Sources/OpenOpal/UI/ContentView.swift
xcodebuild -project OpenOpal.xcodeproj -scheme OpenOpal -configuration Release \
  -derivedDataPath build/DerivedData build -quiet
xcodebuild -project OpenOpal.xcodeproj -scheme OpenOpal -derivedDataPath build/DerivedData \
  test -only-testing:OpenOpalTests/FollowServoTests -quiet
echo T-003 verification OK
```

## Constraints

- **Blast radius:** app target only; bridge untouched (T-002's API is the contract).
- **Permission class:** ship freely; the live check needs Chris in front of the camera.
- **Read/write boundaries:** don't alter the bokeh/matte path except to read its frames.

## Out of scope

- UI controls and persistence (T-004). Group framing heuristics. Gesture control.

## Notes

- If `project.yml` has no test target, add `OpenOpalTests` (unit tests, macOS) with the one
  test file; keep it minimal.
- Vision coordinate origin is bottom-left; `SubjectInfo.bounds` in `MatteProvider` shows how
  upstream converts.

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

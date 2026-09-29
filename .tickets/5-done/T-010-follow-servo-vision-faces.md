---
id: T-010
type: ticket
title: Follow on the stock webcam — Vision faces steer a smooth 1440p crop into the virtual camera
status: cancelled
priority: high
tags: [follow, vision, servo, swift]
created: 2026-09-26
updated: 2026-09-28
closed: 2026-09-28
kind: feature
depends_on: [T-005]
release: c1-follow
sprint: 5
---

## Description

> **Moved 2026-09-28 to Stagehand T-002** (`~/Developer/stagehand`, github.com/digitalmud/stagehand).
> Chris split the follow feature into its own app; this ticket is closed here.

**2026-09-26 re-plan (Chris approved):** retargeted from the depthai path to the **stock webcam
feed**. The reference is `Prototypes/StockFollow/` (tuned live and locked by Chris; 8 % CPU,
42 MB): stock UVC 2560×1440 → `VNDetectFaceRectanglesRequest` ~10 Hz → spring-smoothed crop
(1.33× at his normal seat, ramping to 1.0× as he leans in, 12 % dead zone, gentle over-damped
glide, centre on (re)acquire, 1.5 s hold then back to wide) → 1920×1080 frames into the signed
"Open Opal Camera" (T-005). Reusable parts from branch `t003-follow-wip` (T-003, cancelled):
`FollowView` geometry and `HostZoom` (VTPixelTransferSession crop). Settings become user-facing in
T-014. Where the app lives (a new target or a mode of the fork) is a `/scope` decision. The text
below predates the re-plan and is superseded where it conflicts.

Stub, split from T-003 on 2026-09-26 (Chris approved); `/scope` re-checks it against T-003's
landed code. T-003 delivers a view rect the app can steer (1.5–2.0×, camera window + Mac zoom,
focus mapped through it) and hand-steering menu items. This ticket makes the view **follow
Chris**. Detect his face with Apple Vision and steer the view so the face sits where a good
framing puts it, at a head-and-shoulders zoom. It must feel like Center Stage: calm, no hunting,
no jitter, a gentle recentre when he moves, and a slow return to wide when nobody is there.

## What to build (carried from the original T-003 draft; re-check at scope)

**New `Sources/OpenOpal/Follow/FollowController.swift`:**
- Detection: `VNDetectFaceRectanglesRequest` on every 2nd frame, on a dedicated serial queue with
  one in flight (drop frames, don't queue). Run it on the **1440p camera window** (more context
  than the zoomed output), convert Vision's bottom-left coordinates to top-left, and map to the
  full frame through that frame's window (T-003's frame-to-window matching). Several faces → their
  union box.
- Target: face centre at (0.5, 0.42) of the view (eyes on the upper third); zoom so the face height
  ≈ framing preset (`tight` 0.30, `medium` 0.22, `wide` 0.16 of view height), clamped to T-003's
  1.5–2.0×.
- Dead zone: no move while the face centre is within ±4 % of target and the face height within
  ±12 %.
- Smoothing: critically damped spring per axis (x, y, zoom), time constant by speed
  (`slow` 1.2 s, `normal` 0.7 s, `fast` 0.4 s); zoom at half the translation speed. Runs on the
  frame clock (dt from frame timestamps).
- Lost subject: hold 2.0 s, then ease to the widest follow view (1.5×, centred) over 3 s; on
  re-acquire, move at normal speed. Note: "full frame" (1.0×) isn't available while following (T-002).
- Output: `followPipeline.setView(_:)`; publish the face rect and view rect for T-004's overlay.
- The servo math is a pure function `(faceRect?, view, dt, settings) -> view`, checked like
  T-003's `FollowViewCheck` (a `swiftc`-built check program): dead zone holds; the spring converges
  with ≤2 % overshoot; zoom clamps; lost-subject easing reaches wide within 5.5 s.
- Remove T-003's hand-steering menu items.

**`CameraSettings`:** `followFraming = .medium`, `followSpeed = .normal` (hot).

## Charter decisions (carried; confirm at scope)

| Decision | Choice |
|---|---|
| Detector | Vision face rectangles on the host; not the matte subject box (torso-heavy); not a VPU network (~30 % throughput, jtannahill) |
| Closed loop | Correct relative error each detection; never trust a face position from a stale window |
| Multi-face | Union box; no speaker selection |
| Return to wide | Yes: 2 s hold + 3 s ease (Center Stage behaviour, Chris's ask) |

## Acceptance criteria (draft)

- [ ] Servo checks pass (dead zone, convergence, clamps, lost-subject timing)
- [ ] Live: Chris moves left, right, forward, back and out of frame; the view follows without visible jitter and returns to wide ≈5 s after he leaves (Chris's verdict + a 20 s screen recording path)
- [ ] Telemetry with follow on: ≥28 fps, p50 ≤90 ms
- [ ] § Verification block passes

## Verification

```bash
set -euo pipefail
test -f Sources/OpenOpal/Follow/FollowController.swift
grep -q 'VNDetectFaceRectanglesRequest' Sources/OpenOpal/Follow/FollowController.swift
if grep -q 'T-003 hand steering' Sources/OpenOpal/OpenOpalApp.swift; then echo "hand steering still present"; exit 1; fi
xcodebuild -project OpenOpal.xcodeproj -scheme OpenOpal -configuration Release -derivedDataPath build/DerivedData build -quiet
echo T-010 verification OK
```

## Constraints

- **Blast radius:** app target; `Follow/` files, `CameraSettings`, `CameraModel` wiring, the menu.
- **Permission class:** ship freely; the live check needs Chris in front of the camera.

## Out of scope

- Inspector UI, presets UI, persistence (T-004). Group framing. Gestures.

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

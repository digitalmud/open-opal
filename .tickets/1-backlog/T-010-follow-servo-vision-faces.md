---
id: T-010
type: ticket
title: Follow servo — Vision faces steer the view rect, Center Stage feel
status: backlog
priority: high
tags: [follow, vision, servo, swift]
created: 2026-09-26
updated: 2026-09-26
kind: feature
depends_on: [T-003]
release: c1-follow
sprint: 2
---

## Description

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

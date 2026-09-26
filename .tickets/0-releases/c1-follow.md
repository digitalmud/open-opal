---
type: release
slug: c1-follow
title: Open Opal follow mode — Center Stage for the Opal C1, cropped from 4K
status: active
created: 2026-09-26
target: 2026-10
dashboard:
  health: on-track
  summary: Keep the Opal C1 useful after Composer by adding a follow-me crop that reads the 4K sensor and ships 1080p.
  deliverables:
    - { label: "Upstream builds and streams on mudmini", status: done }
    - { label: "4K→1080p on-camera crop measured within budget", status: pending }
    - { label: "Follow mode tracks Chris in a call", status: pending }
    - { label: "Signed build with virtual camera in /Applications", gate: "Apple Developer team decision" }
  sprints:
    - n: 1
      name: Baseline and spike
      goal: "build upstream · prove the ImageManip crop path and its numbers"
      needs: ["Xcode install approval"]
    - n: 2
      name: Follow servo
      goal: "Vision faces → smoothed crop window · region mapping"
    - n: 3
      name: Controls and shipping
      goal: "UI, persistence · signed build, virtual camera"
      needs: ["Developer ID or OBS fallback decision"]
---

## Overview

Opal has discontinued the C1; Composer 2 refuses it. `alii/open-opal` keeps the camera alive
by booting a DepthAI pipeline into it. This release adds the feature Chris wants most from the
old Composer and from Apple's Center Stage: the frame follows him, and because the crop is
taken from the 4K sensor on the camera and shipped as 1080p, zooming in costs no sharpness.

## Scope

**In:** baseline build of upstream on mudmini; an on-camera crop+resize pipeline with a
runtime-movable window; a host-side face-following servo with smoothing, zoom limits and a
lost-subject fallback; correct AE/AF region mapping through the crop; a Follow control with
framing presets, persisted; a signed Release build so the virtual camera loads.

**Out:** multi-person group framing beyond a union box; gesture control; audio (the C1 mic
already works over UVC and upstream's roadmap lists it separately); Windows or Linux; upstream
PR (welcome later, not a deliverable).

## Tickets

### `~/Developer/open-opal/.tickets/`

- T-001 — Build and run upstream Open Opal on mudmini — done 2026-09-26 (sprint 1)
- T-006 — Boot a C1 whose bootloader reports 0.0.0 (send the bare USB-ROM-boot command) — done 2026-09-26 (sprint 1; found in T-001)
- T-007 — Harden the legacy-bootloader path (T-006 deferred review findings) — backlog (sprint 2; depends T-006)
- T-008 — Easier way into the C1's bootloader window (no manual "Try again, then replug") — backlog (sprint 3; depends T-006)
- T-009 — Upstream bridge CMakeLists names a missing test/region_test.c — backlog (sprint 2)
- T-002 — Crop bench: measure four on-camera crop pipelines against the follow-mode budget — todo (sprint 1; depends T-001)
- T-003 — Follow mode: Vision face servo drives the crop window — backlog (sprint 2; depends T-002)
- T-004 — Follow controls: toggle, framing presets, speed; persist settings — backlog (sprint 3; depends T-003)
- T-005 — Signed Release build, virtual camera install, /Applications — backlog (sprint 3; depends T-001)

## Linked specs

- `docs/brief.md` § Context and § Decisions (the design and its reasons)

## Revision log

- 2026-09-26 — created at project open (Chat, Code/T-731).
- 2026-09-26 — T-001 and T-006 done: upstream streams on mudmini after a bridge fix for this C1's 0.0.0 bootloader. T-007–T-009 filed from T-006's deferred review findings.

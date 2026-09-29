---
type: release
slug: c1-follow
title: Center Stage for the Opal C1 — follow on the stock webcam, as a signed virtual camera
status: parked
created: 2026-09-26
target: 2026-10
dashboard:
  health: parked
  summary: Keep the Opal C1 as Chris's daily camera and add Center Stage-style follow: the stock webcam feed, framed on him by a small menu-bar app, published as a signed virtual camera.
  deliverables:
    - { label: "Upstream builds and streams on mudmini", status: done }
    - { label: "Crop designs measured (depthai bench) and follow proven on the stock feed (prototype)", status: done }
    - { label: "Signed virtual camera in /Applications", status: pending, gate: "Apple Developer membership (ordered 2026-09-26)" }
    - { label: "Follow on the stock webcam in a call", status: pending }
    - { label: "Menu-bar app: follow settings, on-demand camera, status icon", status: pending }
  sprints:
    - n: 1
      name: Baseline and depthai bench (done)
      goal: "build upstream · boot Chris's cameras · crop bench"
    - n: 2
      name: Depthai follow (superseded)
      goal: "historical: design D/E1 on the takeover path; code on branch t003-follow-wip"
    - n: 3
      name: Reliability (done)
      goal: "historical: survive a dropped camera link (T-011)"
    - n: 4
      name: Signing and measurement
      goal: "signed virtual camera (T-005) · glass-to-glass latency of the stock feed (T-015)"
      needs: ["Apple Developer membership active"]
    - n: 5
      name: Follow on the stock webcam
      goal: "prototype → app: Vision follow with Chris's locked settings into the virtual camera (T-010)"
    - n: 6
      name: Menu-bar app
      goal: "follow settings, launch at login, camera on demand, status icon (T-014)"
---

## Overview

Opal has discontinued the C1; Composer 2 refuses it. This release first followed upstream
(`alii/open-opal`), which drives the camera by booting a DepthAI pipeline into it. That work
landed (builds, boots Chris's cameras, a measured crop bench, survives unplugging). But on
2026-09-26 two findings changed the plan: Chris's daily camera (camera 3) offers **2560×1440 and
4K as a plain webcam** with its stock firmware, and a single-file prototype showed
**Center Stage-style follow on that stock feed** at 8 % CPU and 42 MB, tuned live by Chris. So
the release now ships follow on the stock webcam, published as a signed "Open Opal Camera" from a
small menu-bar app. The depthai takeover path stays in the repo as the parked "advanced" path.

## Scope

**In:** the signed virtual camera (upstream's extension + PR #1's fix, our identity); the
stock-feed latency check; follow on the stock webcam with Chris's locked settings; a menu-bar app
exposing those settings, launching at login, taking the camera only during calls, and showing when
it's unplugged.

**Out:** manual camera presets (stock auto is what Chris wants); background blur and face
metering (Chris: not wanted); OBS (Chris: not wanted); the depthai-path fixes (deferred); group
framing; Windows/Linux; an upstream PR (welcome later).

## Tickets

### `~/Developer/open-opal/.tickets/`

- T-001 — Build and run upstream Open Opal on mudmini — done 2026-09-26 (sprint 1)
- T-006 — Boot a C1 whose bootloader reports 0.0.0 (send the bare USB-ROM-boot command) — done 2026-09-26 (sprint 1)
- T-002 — Crop bench: measure four on-camera crop pipelines against the follow-mode budget — done 2026-09-26 (sprint 1)
- T-011 — App hangs forever when the camera link drops (watchdog → opal_close deadlock) — done 2026-09-26 (sprint 3)
- T-003 — Follow plumbing on the depthai path (design E1) — cancelled 2026-09-26: superseded by T-010; code on branch t003-follow-wip
- T-004 — Follow controls: toggle, framing presets, speed; persist settings — cancelled 2026-09-26: superseded by T-014
- T-005 — Virtual camera working: adopt upstream PR #1, our identity, Developer ID sign + notarize, /Applications — backlog, parked 2026-09-28 (branch t005-virtual-camera)
- T-015 — Glass-to-glass latency of the stock C1 webcam feed — moved 2026-09-28 to Stagehand T-004
- T-010 — Follow on the stock webcam — moved 2026-09-28 to Stagehand T-002
- T-014 — Menu-bar app — moved 2026-09-28 to Stagehand T-003
- T-007 — Harden the legacy-bootloader path (T-006 deferred review findings) — backlog (deferred: depthai path)
- T-008 — Easier way into the C1's bootloader window — backlog (deferred: depthai path)
- T-009 — Upstream bridge CMakeLists names a missing test/region_test.c — backlog (deferred: depthai path)
- T-012 — First open of an f63d C1 fails once with '__bootloader' and succeeds on retry (+~5 s) — backlog (deferred: depthai path)
- T-013 — Xcode can bundle a stale libOpalBridge.dylib after a bridge change — backlog (deferred: depthai path)

## Linked specs

- `docs/brief.md` § Decisions (the design and its reasons) · `Prototypes/StockFollow/README.md`
  (locked follow settings and measurements) · `docs/crop-bench.md` (depthai crop bench)

## Revision log

- 2026-09-26 — created at project open (Chat, Code/T-731).
- 2026-09-26 — T-001 and T-006 done: upstream streams on mudmini after a bridge fix for this C1's 0.0.0 bootloader. T-007–T-009 filed from T-006's deferred review findings.
- 2026-09-26 — T-002 done: ISP 1440p window + host zoom (design D) passes at 30 fps / 58 ms; ImageManip fails when moving. Chris picked D; T-003 re-scopes around it.
- 2026-09-26 — T-003 split (Chris): T-003 = follow plumbing (design D in the app, hand-steered), T-010 = the face servo. T-004 now depends on T-010.
- 2026-09-26 — T-003 paused by Chris (follow design switched to E1: whole frame 2560×1440, Mac crop 1.0–1.33×; code on branch t003-follow-wip). Focus moves to T-005 (virtual camera). T-011 filed.
- 2026-09-26 — T-011 done: a dropped camera link no longer hangs or aborts the app (safe close, no crash-dump hunt, timed control sends). T-012, T-013 filed.
- 2026-09-26 — T-014 filed from a design conversation with Chris (menu bar, presets, on-demand camera, no blur, no face metering); measured usage baseline recorded on it.
- 2026-09-26 — **Re-planned (Chris approved):** follow moves to the stock webcam feed (prototype proven, settings locked); the depthai path is parked. T-003/T-004 cancelled as superseded; T-010/T-014 retargeted; T-015 added; T-007/8/9/12/13 deferred; new sprints 4–6.
- 2026-09-28 — **Parked (Chris):** the follow feature is now its own app, Stagehand (`~/Developer/stagehand`, release `stagehand-1`). T-010/T-014/T-015 moved there; T-005 parked. Camo's auto framing was tried and lost to the prototype.

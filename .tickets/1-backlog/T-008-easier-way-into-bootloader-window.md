---
id: T-008
type: ticket
title: Easier way into the C1's bootloader window (no manual "Try again, then replug")
status: backlog
priority: medium
tags: [ux, bridge, discovery]
created: 2026-09-26
updated: 2026-09-26
depends_on: [T-006]
kind: feature
release: c1-follow
sprint: 3
---

## Description

Stub from T-001/T-006 (2026-09-26); `/scope` fleshes it out. Chris's C1 is bootable only
during a ~5 s bootloader window after power-on, so today the routine is "launch the app or
click Try again, then unplug and replug" (`docs/BUILD-mudmini.md`). The app's search gives up
after 20 s. Options to explore:
- keep searching while the camera shows as BOOTED (state 2), with a "replug your camera" prompt;
- whether anything read-only can move a BOOTED stock C1 back into its bootloader without a replug.

Also fix the boot-log label: state 2 reads "booted (depthai firmware, RAM)", but on this camera
it's the stock webcam firmware (`OpalBridge.cpp` `stateName`).

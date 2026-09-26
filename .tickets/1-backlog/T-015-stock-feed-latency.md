---
id: T-015
type: ticket
title: Glass-to-glass latency of the stock C1 webcam feed (1440p) vs the depthai path
status: backlog
priority: high
tags: [measurement, latency, uvc]
created: 2026-09-26
updated: 2026-09-26
kind: chore
depends_on: []
release: c1-follow
sprint: 4
---

## Description

Stub from the 2026-09-26 re-plan; `/scope` fleshes it out. Follow now runs on camera 3's **stock
webcam feed** (2560×1440 @ 29.9 fps, measured). Frames reach the app ~2 ms after their USB
timestamp, but that excludes the camera's own exposure and processing, so it isn't comparable with
the depthai path's 58 ms (sensor → host). Measure true glass-to-glass latency before this becomes
Chris's daily camera: film a millisecond clock on screen, read the clock inside the captured frame
against the time of capture, for the stock 1440p feed (and, for reference, the depthai path).
Needs no signing; can run any time.

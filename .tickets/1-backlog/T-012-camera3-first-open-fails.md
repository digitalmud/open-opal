---
id: T-012
type: ticket
title: First open of an f63d C1 (bootloader 0.0.15) fails once with '__bootloader' and succeeds on retry (+~5 s)
status: backlog
priority: low
tags: [bridge, depthai, startup]
created: 2026-09-26
updated: 2026-09-26
kind: bug
depends_on: []
release: c1-follow
sprint: deferred
---

## Description

Stub from T-011 (2026-09-26). On camera 3 (serial 19443010A1DA5F1300, USB f63d, bootloader
0.0.15) the app's first `opal_open` attempt fails with `Couldn't read data from stream:
'__bootloader' (X_LINK_ERROR)`, on a fresh launch as well as after a reconnect; attempt 2 about 5 s
later succeeds (app log, 13:58:00 and 13:59:22/50). depthai warns "Flashed bootloader version
0.0.15, less than 0.0.28 is susceptible to bootup/restart failure". The retry hides it, but every
open costs ~5 s. Find out why depthai's first BOOTLOADER-path attempt fails (a timing race with
the camera's own re-enumeration?) and whether a short wait or a different retry cadence fixes it
without flashing (the bootloader upgrade depthai suggests stays forbidden).

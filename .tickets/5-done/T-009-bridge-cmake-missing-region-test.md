---
id: T-009
type: ticket
title: Upstream bridge CMakeLists names a missing test/region_test.c
status: cancelled
priority: low
tags: [build, upstream]
created: 2026-09-26
updated: 2026-11-19
depends_on: []
kind: bug
release: c1-follow
sprint: deferred
closed: 2026-11-19
---

## Description

Stub from T-006's baseline (2026-09-26). `Sources/OpalBridge/CMakeLists.txt` adds
`region_test` from `test/region_test.c` when `OPAL_BRIDGE_TEST=ON`, but only
`test/bridge_test.c` exists upstream, so configuring with the option fails ("No SOURCES given
to target: region_test"). Either restore the test or drop the target; a candidate one-line PR
to `alii/open-opal` (with Chris).

## Groomed out, 2026-11-19

Closed as no longer relevant. It belongs to the camera-follow release, parked in September and since overtaken by another tool, one of eight paused things put to Chris as "which are alive?". He named the two that are (2026-11-19 12:55: "these are still live: The monthly Knox Radar; A sensor for the dryer") and this was not one of them. Nothing was deleted: to bring it back, move this file to `1-backlog/` and set `status: backlog`.

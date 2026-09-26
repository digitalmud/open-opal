---
id: T-009
type: ticket
title: Upstream bridge CMakeLists names a missing test/region_test.c
status: backlog
priority: low
tags: [build, upstream]
created: 2026-09-26
updated: 2026-09-26
depends_on: []
kind: bug
release: c1-follow
sprint: 2
---

## Description

Stub from T-006's baseline (2026-09-26). `Sources/OpalBridge/CMakeLists.txt` adds
`region_test` from `test/region_test.c` when `OPAL_BRIDGE_TEST=ON`, but only
`test/bridge_test.c` exists upstream, so configuring with the option fails ("No SOURCES given
to target: region_test"). Either restore the test or drop the target; a candidate one-line PR
to `alii/open-opal` (with Chris).

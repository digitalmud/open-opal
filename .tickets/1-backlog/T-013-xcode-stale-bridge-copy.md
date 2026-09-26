---
id: T-013
type: ticket
title: Xcode can bundle a stale libOpalBridge.dylib after a bridge change (needs a second build)
status: backlog
priority: low
tags: [build, xcode, bridge]
created: 2026-09-26
updated: 2026-09-26
kind: bug
depends_on: []
release: c1-follow
sprint: 3
---

## Description

Stub from T-011 (2026-09-26). After editing the bridge, the first Release build rebuilt
`build/bridge/libOpalBridge.dylib` (13:41) but the app bundle kept the previous copy (13:02):
`nm` showed no new symbol. A second build copied the fresh one. The bridge is built by a
pre-build script inside the same Xcode build that copies it, so the copy phase can see the old
file. T-011's verification now guards with `cmp` + `nm`. Fix at the source: declare the
script's output file in `project.yml` (`outputFiles`) so Xcode orders the copy after it, or copy
in the script itself; then drop the "build twice" workaround.

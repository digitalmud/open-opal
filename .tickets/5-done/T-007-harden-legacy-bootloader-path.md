---
id: T-007
type: ticket
title: Harden the legacy-bootloader path (T-006 deferred review findings)
status: cancelled
priority: medium
tags: [bridge, bootloader, robustness]
created: 2026-09-26
updated: 2026-11-19
depends_on: [T-006]
kind: bug
release: c1-follow
sprint: deferred
closed: 2026-11-19
---

## Description

Stub from T-006's reviews (2026-09-26); `/scope` fleshes it out. T-006 works on Chris's C1
(`Sources/OpalBridge/LegacyBootloader.cpp`, the call site in `OpalBridge.cpp` `opal_open`), but
its reviews deferred these, each with the evidence on T-006:

- **Hang risk:** untimed `XLinkStream` reads have no escape hatch. The timed calls fail with
  `X_LINK_ERROR` on this camera, so the candidate is a guard thread that closes the connection.
- **Crash in depthai's own error path:** segfault in `dai::BoardConfig::~BoardConfig` while
  `dai::Device::Device` unwinds (run 3, `harness-2026-09-26-093632.ips`). Seen only after an
  aborted side connection; confirm it's gone or guard it.
- No-mxid branch of `opal_open` doesn't get the kick.
- Reuse depthai's `XLinkConnection::getDeviceByMxId` for the BOOTLOADER poll.
- One stale XLink link id per kick (reset-on-close off on a dead link); check long sessions.
- Remember kicked mxids in-process so later opens skip depthai's doomed first attempt (~1.5 s),
  leaving more of the ~5 s window.
- Edge-of-window timing: a failed open can take ~20 s longer.

Keep the T-006 rules: only `GetBootloaderVersion` and `UsbRomBoot` are ever sent; upstream
cameras never reach the new code.

## Groomed out, 2026-11-19

Closed as no longer relevant. It belongs to the camera-follow release, parked in September and since overtaken by another tool, one of eight paused things put to Chris as "which are alive?". He named the two that are (2026-11-19 12:55: "these are still live: The monthly Knox Radar; A sensor for the dryer") and this was not one of them. Nothing was deleted: to bring it back, move this file to `1-backlog/` and set `status: backlog`.

---
id: T-011
type: ticket
title: App hangs forever when the camera link drops (watchdog → opal_close deadlock)
status: backlog
priority: high
tags: [bridge, depthai, reliability, virtual-camera]
created: 2026-09-26
updated: 2026-09-26
kind: bug
depends_on: []
release: c1-follow
sprint: 3
---

## Description

Stub, filed 2026-09-26 during T-003; `/scope` fleshes it out. When the USB link to the C1
dropped during a session (cause unknown; it happened once, under T-003's design-D test), the app
froze for good. Its preview stopped, telemetry stopped, and the menus hung. A thread sample showed
the main thread stuck in:

```
OpalDevice.watchdog() → OpalDevice.disconnect() → opal_close → dai::Device::~Device
  → DeviceBase::closeImpl → DeviceBase::hasCrashDump → nanorpc call → XLinkStream::write
  → XLinkWriteData → DispatcherAddEvent → XLink_sem_wait   (forever)
```

while depthai's event thread was in `eventSchedulerRun → dispatcherReset → dispatcherClean →
XLink_sem_destroy`. The bridge's capture thread had already exited (a read threw). The camera
itself recovered to its stock webcam on its own; only the app was dead, and it had to be killed.

This is upstream behaviour (the watchdog and `opal_close` predate the fork), but for "the virtual
camera works 100%" (Chris, 2026-09-26) a dead link must never hang the app: the virtual camera
goes black forever and the app has to be force-quit.

## To find out at scope

- Does unplugging the camera mid-stream reproduce it on demand? (A likely one-line repro.)
- Can `opal_close` avoid the crash-dump RPC on a dead link (depthai `DeviceBase` has
  crash-dump options), or run the close off the main actor with a timeout and abandon a stuck
  close so the app can reconnect?
- Should the watchdog reconnect without closing when the link is already gone?

## Acceptance (draft)

- Unplugging the camera mid-stream leaves the app responsive and it reconnects when the camera
  returns (with the virtual camera showing a placeholder meanwhile).

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

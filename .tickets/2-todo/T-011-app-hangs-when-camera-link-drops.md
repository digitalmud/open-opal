---
id: T-011
type: ticket
title: App hangs forever when the camera link drops (watchdog → opal_close deadlock)
status: todo
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

Found in T-003 and reproduced 2026-09-26 13:37. When the USB link to the C1 drops mid-stream
(Chris unplugged camera 3 for ~10 s), upstream's app fails in one of two ways. Both happen
inside the same line, `h->device.reset()` in `opal_close`, which upstream's watchdog reaches via
`OpalDevice.watchdog() → disconnect() → opal_close`:

1. **Abort** (reproduced on demand by an unplug): `libc++abi: terminating due to uncaught exception
   of type dai::XLinkReadError: Couldn't read data from stream: '__bootloader' (X_LINK_ERROR)`;
   the crash report (`OpenOpal-2026-09-26-133752.ips`) shows the main thread in
   `dai::Device::~Device() → std::terminate`. An exception leaving a destructor always terminates.
2. **Hang** (seen once, 11:20): the main thread stuck forever in
   `DeviceBase::closeImpl → hasCrashDump → XLinkWriteData → XLink_sem_wait`, while depthai's
   event thread was in `dispatcherReset → dispatcherClean → XLink_sem_destroy`.

Why a plain try/catch can't fix it: `DeviceBase::close()` sets `closed = true` only after
`closeImpl()` succeeds (`DeviceBase.cpp:516–521`). If the close throws, the destructor's own
`close()` runs it again, and a throw there aborts the app.

For the virtual camera to "work 100%" (Chris), a dropped link must leave the app alive, and it
must reconnect when the camera comes back.

## What to build

- New `Sources/OpalBridge/SafeClose.{hpp,cpp}` (namespace `opal`):
  `bool closeDeviceSafely(std::unique_ptr<dai::Device> dev, std::chrono::milliseconds bound)`.
  It takes ownership, runs `dev->close()` on a detached helper thread, then:
  - **success** → `delete` it (the destructor's `close()` is then a no-op);
  - **exception** → never destroy that object (a deliberate leak; destroying it would re-run
    the close and abort); log it;
  - **not done within `bound`** → return false and leave the helper thread and object behind
    (a deliberate leak); log it.
  Every path logs through the bridge's boot log and keeps the app running.
- `OpalBridge.cpp` `opal_close`: `h->device.reset();` → `opal::closeDeviceSafely(std::move(h->device), kCloseBound);`
  plus a comment; `CMakeLists.txt` + `SafeClose.cpp`.
- **Measure first:** the time a normal `opal_close` takes on camera 3 (5 open/close cycles with
  a throwaway harness). The bound is `max(2 × the slowest normal close, 8 s)`, recorded in the Build log.

### Files touched

| Path | Action |
|---|---|
| `Sources/OpalBridge/SafeClose.hpp` | new |
| `Sources/OpalBridge/SafeClose.cpp` | new |
| `Sources/OpalBridge/OpalBridge.cpp` | edit: one line in `opal_close` + include |
| `Sources/OpalBridge/CMakeLists.txt` | edit: one source |

Total: 4 files, ~70 LOC.

## Charter decisions (locked 2026-09-26)

| Decision | Choice |
|---|---|
| Where to fix | The bridge's `opal_close` (both failures happen there); the Swift watchdog stays as is |
| Failed or stuck close | Leak that one `dai::Device` on purpose (rare event); never destroy it after a throw |
| Bound | `max(2 × slowest measured normal close, 8 s)` |
| Main-thread block during a stuck close | Accepted, up to the bound: `disconnect()` stays synchronous (making it async ripples through every caller). Follow-up if it proves annoying |
| Camera unplugged longer than 20 s | Out of scope: upstream's `connect` searches for 20 s, then shows "No Opal C1 found" with Try again |
| Branch | `t011-link-drop` off `master` |

## Acceptance criteria

- [ ] § Verification block passes
- [ ] Unplug test ×2 (human-assisted: Chris unplugs camera 3 ~10 s, replugs): the app stays running (no new `OpenOpal-*.ips` crash report, process alive), and streams again after the replug; Build log has the stderr and log lines
- [ ] A normal quit still returns the stock webcam (as before)
- [ ] Normal-close timing and the chosen bound recorded

## Verification

```bash
set -euo pipefail
S=$(mktemp -d "${TMPDIR:-/tmp}/t011.XXXXXX") || { echo "SCRATCH FAILED"; exit 1; }
grep -q 'opal::closeDeviceSafely' Sources/OpalBridge/OpalBridge.cpp
if awk '/^void opal_close/,/^}/' Sources/OpalBridge/OpalBridge.cpp | grep -q 'h->device.reset()'; then echo "opal_close still destroys the device directly"; exit 1; fi
grep -q 'SafeClose.cpp' Sources/OpalBridge/CMakeLists.txt
export CMAKE_POLICY_VERSION_MINIMUM=3.5
cmake -S Sources/OpalBridge -B "$S/bridge" -G Ninja -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build "$S/bridge" >/dev/null
xcodebuild -project OpenOpal.xcodeproj -scheme OpenOpal -configuration Release -derivedDataPath build/DerivedData build -quiet
if pgrep -x OpenOpal >/dev/null; then echo "OpenOpal still running"; exit 1; fi
system_profiler SPCameraDataType | grep -q 'Opal C1'
echo T-011 verification OK
```

## Constraints

- **Blast radius:** bridge only (two new files, one line, CMake). Camera RAM only.
- **Permission class:** ship freely; the unplug test needs Chris.
- **Read/write boundaries:** never flash; quit the app when done.

## Out of scope

- Why the link dropped in T-003's session (not reproduced). Async `disconnect`. Longer
  reconnect searching. depthai upgrades.

## Notes

- Evidence: T-003 Build log (the hang sample), `build/t011-run-1314.log` (stderr of the abort),
  `~/Library/Logs/DiagnosticReports/OpenOpal-2026-09-26-133752.ips`.

---

## Principles in scope

`verification.negative-path` (the fault is forced by unplugging, not assumed) · `verification.measure-first` (the bound comes from measured normal closes) · `verification.run-it` · `verification.behavioral-gap` (the unplug test is human-assisted) · CLAUDE.md "The C1 is Chris's daily webcam … don't leave a build holding the device" · "stay upstream-friendly" (new file, one-line change)

## Plan

Scoped 2026-09-26 ~13:45 right after the repro (Chris approved starting T-011 while Apple
processes his membership). Build: measure normal closes → SafeClose → rebuild → unplug test ×2 →
quit test → Verification.

**Doc impact for /deploy:** `docs/BUILD-mudmini.md` (a line: unplugging mid-stream is survivable);
the release manifest line.

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

---
id: T-011
type: ticket
title: App hangs forever when the camera link drops (watchdog → opal_close deadlock)
status: doing
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
# the app must carry the bridge just built (Xcode can copy a stale one; see Build log)
cmp -s build/DerivedData/Build/Products/Release/OpenOpal.app/Contents/Frameworks/libOpalBridge.dylib build/bridge/libOpalBridge.dylib || { echo "app bundles a stale libOpalBridge"; exit 1; }
nm -gU build/DerivedData/Build/Products/Release/OpenOpal.app/Contents/Frameworks/libOpalBridge.dylib | grep -q closeDeviceSafely
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
- BASELINE: 1 failure. `grep -q 'opal::closeDeviceSafely'` (nothing built yet; expected).
- Repro before any change (13:37, `master` build, Chris unplugged camera 3 ~10 s): the app
  **aborted**. `libc++abi: terminating due to uncaught exception of type dai::XLinkReadError:
  Couldn't read data from stream: '__bootloader' (X_LINK_ERROR)`; crash report
  `OpenOpal-2026-09-26-133752.ips`: main thread `OpalDevice.watchdog → disconnect → opal_close →
  dai::Device::~Device → std::terminate`.
- Measured normal closes (throwaway harness, 5 open/stream 3 s/close cycles on camera 3):
  `opal_close` 1.34, 1.35, 1.35, 1.38, 1.40 s → bound = max(2 × 1.40, 8) = **8 s**.
- `Sources/OpalBridge/SafeClose.{hpp,cpp}` (new): `opal::closeDeviceSafely(unique_ptr<Device>,
  bound, log)`: explicit `close()` on a detached thread; delete only after a clean close; after a
  throw, never destroy (the destructor would re-run the close and abort); after the bound, abandon.
- `OpalBridge.cpp`: include + `opal_close`'s `h->device.reset()` → `closeDeviceSafely(…, 8 s,
  bootLog)` with a comment. `CMakeLists.txt`: + `SafeClose.cpp`.
- **Found while building, fixed in the verification:** the first build after changing the bridge
  left a **stale `libOpalBridge.dylib` inside the app** (bundle copy 13:02 vs fresh `build/bridge`
  13:41; `nm` showed no `closeDeviceSafely`). Xcode's copy step can run with the pre-build
  script's old output. A second build fixed it. So my first post-fix regression run had tested
  the *old* bridge and doesn't count; re-run below. The Verification block now also runs
  `cmp` (bundled vs freshly built bridge) and checks `nm` for the new symbol. Follow-up ticket
  for the build quirk at /deploy.

### Verification output
```
T-011 verification OK
```
(13:53, after the app quit; stock webcam listed again 12 s after quitting.)

Normal closes with SafeClose (bundled bridge confirmed current): 1.40, 1.39, 1.41, 1.37, 1.38 s;
all clean, none abandoned.

Unplug tests (human-assisted: Chris unplugged camera 3 ~10 s and replugged), same process
throughout (pid 38217), no new crash report, no "terminat" in stderr (`build/t011-fixed-1343.log`):
- #1: `13:50:17 watchdog: no frames for 3s` → `13:50:26 booting` → `13:50:29 streaming`. Chris:
  "It recovered". The ~9 s gap suggests the old close hit the 8 s bound and was abandoned
  (inferred: the bridge's line goes to the in-app boot overlay, not the system log).
- #2: `13:51:37 watchdog` → `13:51:53 booting` → `13:51:56 open attempt 1 failed: Couldn't read
  data from stream: '__bootloader' (X_LINK_ERROR)` (the error that aborted the app at 13:37, now a
  retried failure) → `13:52:01 streaming`. Chris: "Recovered; never froze".

### Build round 2 (after review 1, 13:55)
- `OpalBridge.cpp` `UsbOnlyInit`: `setenv("DEPTHAI_CRASHDUMP_TIMEOUT", "0", 0)`, so depthai skips the
  crash-dump hunt on close. Confirmed live: stderr says `Device crashed. Crash dump retrieval
  disabled.` on each unplug.
- Control-queue sends are timed (`send(ctrl, 100 ms)`): the control thread marks a delta sent only if
  it went out (else it retries next tick); the three one-shot sends (autofocus trigger, focus
  region, exposure region) use the same timeout.
- `SafeClose.cpp`: an `abandoned` flag stops a late helper from logging; a failure to create the
  thread is caught and treated as abandoned.
- Built twice (the stale-copy quirk); `T-011 verification OK` (cmp + nm confirm the bundled bridge
  is current). Normal closes: 1.37, 1.37, 1.37, 1.40, 1.40 s.
- Unplug ×2 (Chris, 13:59; "Recovered both, never froze"), pid 40842 throughout, 0 "terminat",
  no new crash report:
  `13:59:06 watchdog → 13:59:18 booting → 13:59:22 open attempt 1 failed: '__bootloader' →
  13:59:27 streaming`; `13:59:34 watchdog → 13:59:46 booting → 13:59:50 attempt 1 failed →
  13:59:55 streaming`. Stock webcam back 12 s after quitting.
- **Correction to review 1's P1 attribution:** `open attempt 1 failed: '__bootloader'` also happens
  on a **fresh launch** with no unplug (13:58:00). It's camera 3's first open failing on its own
  (fits depthai's warning that bootloader 0.0.15 is "susceptible to bootup/restart failure"), not
  the crash-dump hunt. The retry always succeeds (+~5 s per open). Follow-up ticket at /deploy. The
  crash-dump fix is still right: it stops the leaked close from reconnecting to the camera and
  shortens a dead-link close.

### Build round 3 (after review 2, 14:01)
- `opal_set_focus_region`: `if(!h->controlQ->send(ctrl, kSendTimeout)) return;`, so the AF lock
  is recorded only if it went out. The anchor for this edit first matched twice (the same two
  lines exist in `opal_trigger_autofocus`), so the script refused and nothing changed; re-applied
  with a unique anchor.
- `SafeClose.cpp`: `struct Fate { std::mutex m; bool abandoned; }` shared by the helper and the
  caller; the helper logs only under the lock and only if not abandoned; the caller sets
  `abandoned` under the same lock.
- Built twice; `T-011 verification OK`. No new hardware run: neither change touches the unplug
  path (one is a send-failure branch, the other a logging guard).

### Open items for Review
- A stuck close can block the main thread for up to 8 s (Charter accepts it). In round 2 Chris
  saw no freeze in either test.
- An abandoned `dai::Device` (and its helper thread) is leaked per link drop, by design.
- Whether the SafeClose log line reaches the system log: it doesn't (boot overlay only). Fine for
  now; noted.

### Failed attempts
- none (the stale-copy build was a build-system quirk, caught and fixed; not a code fix failure)

## Review

join-verify: exit 0 @ d0778e1 (bash)

Review 1, 2026-09-26 13:55, of the working tree on `d0778e1`. `/code-review` at **medium**
(runtime code; bridge). Verification run with `bash` from the checkout: `T-011 verification OK`
(`stage-verify` can't see the gitignored depthai build; same limit as T-001/T-006/T-002).

### Findings
- **P1**: `SafeClose.cpp`: on a dead link depthai's `closeImpl` looks for a crash dump for up to
  ~9 s (`DEFAULT_CRASHDUMP_TIMEOUT` 9000 ms + the USB watchdog, `DeviceBase.cpp:588–615`). When the
  camera reappears in BOOTLOADER/UNBOOTED, it **connects to it** (`DeviceBase rebootingDevice(…,
  dumpOnly=true)`) and competes with the reconnect. That explains unplug test #2's `open attempt 1
  failed: '__bootloader'`; the app only recovered because its retry won. Fix:
  `DEPTHAI_CRASHDUMP_TIMEOUT=0` (a `timeout > 0` guard then skips the search). → round 2.
- **P2**: `opal_close` can still hang at `h->control.join()`: the control queue is blocking
  (depth 16) and `DataInputQueue::send` blocks when the writer is stuck in `XLinkWriteData`. The
  focus/exposure region sends on the main thread have the same risk. Fix: timed
  `send(msg, 100 ms)` (`DataQueue.hpp:469`). → round 2.
- **P2**: an abandoned helper thread can call `bootLog` after the app starts exiting (the global
  mutex is destroyed, the Swift context pointer is dangling). Fix: no logging once abandoned. → round 2.
- **P3**: the `std::thread` constructor can throw through the C API. Fix: catch, and treat it as
  abandoned. → round 2.

Silencing scan: none. Principles: `verification.negative-path` upheld (the fault was forced), but
the P1 shows the recovery was partly luck.

Round result: back-to-build — one P1 (the crash-dump search fights the reconnect) plus three smaller fixes, over the fix-in-place limit

---

join-verify: exit 0 @ d0778e1 (bash)

Review 2, 2026-09-26 14:01, of build round 2. `/code-review` at **low**, scoped to the round-2 hunks.
Verification: `bash` from the checkout, `T-011 verification OK`.

### Findings
- **P2**: `opal_set_focus_region` ignored the timed send's result and still recorded the AF lock
  as sent, so a timeout (dead link) would leave the app and camera disagreeing. → round 3: return if
  not sent.
- **P2**: `SafeClose.cpp`: the helper's "check abandoned, then log" could race the caller setting
  `abandoned` near the 8 s bound (a tiny window for exactly the late log the flag exists to stop).
  → round 3: one mutex around the check-and-log and the abandon.

Round result: back-to-build — two small fixes, ~12 changed lines (just over the fix-in-place limit of 10)

---

join-verify: exit 0 @ d0778e1 (bash)

Review 3, 2026-09-26 14:02, of build round 3. `/code-review` at **low**, scoped to the two
round-3 hunks. Verification: `bash` from the checkout, `T-011 verification OK`.

### Findings
- None. The reviewer's one conditional risk (a deadlock if `closeDeviceSafely`'s caller held the log
  callback's mutex) was checked: the only caller is `opal_close`, which takes no lock
  (`awk … opal_close | grep lock` → none). A close failing right at the bound can log twice; harmless.

### Follow-ups filed at /deploy
- Camera 3's first open attempt fails with `'__bootloader'` and succeeds on retry (+~5 s per open).
- Xcode can bundle a stale `libOpalBridge.dylib` after bridge changes (the first build after a
  bridge edit); T-011's verification now guards against it with `cmp` + `nm`.

**Verdict: clear** — three review rounds; a dropped link now leaves the app alive and reconnecting (4 unplug tests on camera 3, no crash, no hang)

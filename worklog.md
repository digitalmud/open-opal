# worklog — open-opal

---

## 2026-09-26 — project opened from Chat

- Chris asked for research on the Opal C1 now that Composer is dead, then specifically for a
  Center Stage-style follow that crops the 4K sensor to 1080p. Chat probed the camera over UVC
  (only 1920×1080@60 exposed), read `alii/open-opal`, jtannahill's DepthAI project and the
  2026 postmortem, and recommended forking Open Opal and adding an on-camera crop driven by
  host-side Vision face detection.
- Forked to `digitalmud/open-opal`, cloned here, scaffolded brief, worklog, model pin,
  tickets T-001 → T-005 under release `c1-follow`. Code/T-731 records the opening.
- Found before any build: Xcode is not installed on mudmini (Command Line Tools only);
  `ninja` and `xcodegen` missing. T-001 starts there.

---

## 2026-09-26 — T-001 and T-006 landed: upstream streams on mudmini, via a bootloader fix

- Toolchain came first. Chris installed Xcode 26.6 by hand and accepted the licence in Terminal
  (a `!` pane can't take a sudo password). Xcode 26 also needed `-runFirstLaunch` and a separate
  688 MB Metal toolchain download before upstream would build. Recipe in `docs/BUILD-mudmini.md`.
- Upstream built first time but couldn't see the camera. Chris's C1 isn't the one upstream was
  written for: USB `f63b` (not `f63d`), DepthAI bootloader **0.0.0** (not 0.0.15), a ~5 s
  bootloader window at power-on, and depthai calls the sensor an **IMX378**. A read-only watcher
  found the window; depthai refused to boot a 0.0.0 bootloader. Chris approved sending it the
  one bare "restart into USB boot" command (no flash, nothing written), which became T-006.
- T-006 took four review rounds. The first two designs probed the camera before depthai did,
  and that changes behaviour for upstream's cameras whichever way the reset-on-close flag is set.
  The landed design lets depthai try first, exactly as upstream does, and kicks only after its
  0.0.0 refusal. Timed XLink calls turned out not to work on this camera. Final app build streams
  in ~11 s from replug; 30.1 fps, p50 51 ms; the webcam comes back 6 s after quitting.
- Daily routine for now: start Open Opal (or click Try again), then unplug and replug the C1.
  T-008 is the easier way in. T-007 holds the hardening items, T-009 an upstream CMake bug.
- Next: T-002 (the crop spike). It must first check the IMX378's real modes; the brief's
  IMX582 figures may not hold for this unit.
- 10:07: surveyed a second C1 (serial …10C192A5D200). It's identical to the daily unit (f63b,
  bootloader 0.0.0, IMX378) and boots first try through T-006 at 30.1 fps · 49 ms. Chris's
  cameras look like one batch, different from upstream's. It's the candidate test camera for T-002.
- 10:14: a third C1 (serial 1944…A1DA5F1300) is upstream's kind: f63d, bootloader 0.0.15, LCM48
  48 MP sensor. It opens any time with no replug (first frame ~3 s, 30.0 fps · 46 ms) and went
  through upstream's path without touching T-006's code, which confirms that promise on hardware.
  A fourth camera wasn't found. Open question for T-002's scope: which sensor follow mode targets.
- 10:16: Chris chose camera 3 (LCM48) as the follow-mode target and his daily webcam. T-002 is
  measured on it; T-008 drops to low priority (only the spare IMX378 units need a replug now).

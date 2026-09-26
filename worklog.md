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

---

## 2026-09-26 — T-002 landed: the crop bench, and Chris picked design D

- T-002 was re-scoped from "build the ImageManip crop into the app" to a bench. The reason was
  prior art on the same camera: jtannahill measured 5.9 fps for an on-camera ImageManip crop.
  Four pipelines were measured on camera 3, 60 s each with the window still and then moving
  every frame.
- ImageManip keeps up while the window stays still but collapses when it moves (4K: 15 fps /
  124 ms; 1440p: 21 fps / 106 ms; even 10 moves a second only reaches 22 fps). The camera ISP's
  own video window moves for free: a 1080p window holds 30 fps / 50 ms, a 1440p window
  30 fps / 58 ms.
- Chris picked **D**: the ISP pans a 2560×1440 window and the Mac zooms 1.5–2.0× by downscaling.
  Following has no whole-room view; follow off returns to the full frame. Numbers and caveats
  are in `docs/crop-bench.md`. T-003 re-scopes around D and must measure the host zoom.
- Review 1 caught four small failure-path bugs (lock around the send, NaN windows, Ctrl-C not
  closing the camera, silent sample truncation). All were fixed; two were proven by forcing
  the failure on camera 3.
- Camera 3 is Chris's daily webcam now. It shows in macOS as "Opal C1" (not "(ctrl)") and needs
  no replug; every open was 2–3 s.

---

## 2026-09-26 (afternoon) — follow paused, virtual camera started, T-011 landed

- Chris watched design D in the app ("very jittery, and way too close … It should frame me like a
  normal video call"). The jitter was my hand-steering test; "too close" was real. Two
  whole-frame designs were measured on camera 3 (E1 2560×1440: 30 fps / 58 ms, zoom 1.0–1.33×;
  E2 2880×1620: 30 fps / 64 ms, 1.0–1.5×) and Chris picked E1. Then he paused follow to get the
  virtual camera working. The follow code waits on branch `t003-follow-wip`.
- T-005: a free Apple ID can't sign a camera extension (personal teams don't get the System
  Extension capability), so Chris is enrolling in the Developer Program. Upstream PR #1 was
  adopted (it fixes the feeder picking the capture stream instead of the sink, which would have
  left the virtual camera blank) and the identity moved to `ca.digitalmud.*` with the team ID kept
  out of git. Waiting on Apple's order processing; the work is on branch `t005-virtual-camera`.
- T-011 landed: unplugging the camera used to abort the app (an exception out of depthai's Device
  destructor) or, once, hang it (a close RPC deadlocked on the dead link). Now the close runs safely
  with an 8 s bound, depthai's crash-dump hunt is off (it reconnected to the camera behind our
  back), and control sends are timed. Four unplug tests on camera 3 recovered with no crash or hang.
- Found along the way: camera 3's first open always fails once and retries (+~5 s, T-012), and
  Xcode can bundle a stale bridge after bridge edits (build twice; T-013).
- 14:40: Chris paused the project: "I'm not sure I'm getting much beyond the stock firmware with
  this." State: T-001/T-006/T-002/T-011 done on master; T-005 (virtual camera, Developer ID; Apple
  order still processing) on branch `t005-virtual-camera`; follow (T-003, design E1) on
  `t003-follow-wip`; T-014 (menu-bar presets, the design Chris wants if he continues) in backlog.
  The deciding test he may run: whether stock auto-exposure/white balance/focus drift bothers him
  on calls.

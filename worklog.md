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

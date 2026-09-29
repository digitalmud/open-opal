# open-opal — follow mode for the Opal C1

**Area:** Personal / System
**Status:** 2026-09-28 **parked** (Chris). The follow feature moved to its own app, **Stagehand** (`~/Developer/stagehand`, github.com/digitalmud/stagehand), which ships its own signed camera; Stagehand's Apple signing setup is done. Here: the depthai takeover path (T-001/T-006/T-002/T-011 done) and T-005 (this app's virtual camera, branch `t005-virtual-camera`) stay parked.

## Overview

Opal discontinued the C1 and Composer 2 refuses it (announcement 2026-06-01). The camera is a
Luxonis DepthAI device (Myriad X VPU + Sony IMX582 48 MP sensor), so `alii/open-opal` drives it
directly: it boots a DepthAI pipeline into the camera's RAM and gets Composer-grade controls back.
This fork adds what Chris asked for on 2026-09-26: a **Center Stage-style follow mode** that
keeps him framed by moving and resizing a crop window over the full 4K frame and shipping a
1080p result, so the zoom is a real crop of a sharper image, not an upscale.

## Context

- **Build order:** T-001 baseline build and run → T-002 crop bench (the design gate, done
  2026-09-26: design D, see Decisions) → T-003 follow servo →
  T-004 controls and persistence → T-005 signed build and virtual camera.
- **Why the crop must happen on the camera:** full 4K NV12 over USB is ~370 MB/s, measured
  ~300 ms latency at 20 fps (upstream README). Upstream downscales on the ISP to 1080p for
  ~45 ms. Follow mode instead keeps the ISP at 4K and has the ISP itself cut a 2560×1440
  `video` window that moves at runtime (`ColorCamera.inputConfig`); the Mac then zooms within
  it to 1920×1080 by downscaling. Measured 30 fps, p50 58 ms with the window moving every frame
  (`docs/crop-bench.md`). An `ImageManip` crop, the original plan, measured 15 fps when moving.
- **Why face detection stays on the host:** Apple Vision face rectangles run in ~5 ms a frame
  on the Mac; a detection network on the VPU costs about a third of throughput (jtannahill's
  measurements). The host runs a closed-loop servo: detect on the received 1080p frame,
  compute where the face sits relative to the current crop, nudge the crop. Closed loop makes
  it robust to the one-to-two frame lag between a crop command and the frame it produced.
- **Plain-webcam (UVC) formats depend on the unit.** The old daily camera (IMX378, f63b) exposed
  only 1920×1080@60 over UVC (probed 2026-09-26, morning). **Camera 3** (LCM48, f63d), Chris's
  daily camera since the afternoon, exposes 1280×720, 1920×1080, **2560×1440 and 3840×2160 at 30 fps**
  (uncompressed `420v`) with its stock firmware, and advertises standard UVC controls (a read-only
  GET_CUR from an ordinary app succeeded). So a host-side follow crop from 1440p/4K *is* possible
  without the DepthAI takeover. That's the basis of the 2026-09-26 re-plan.
- **Virtual camera needs a signed and notarized build** (CMIO system extension; ad-hoc signing
  is refused by AMFI). Without a Developer ID the app runs and previews but Zoom/Meet can't see
  it. Decision needed from Chris at T-005: use a digitalmud Apple Developer team, or fall back
  to a Syphon/NDI feed into OBS (installed, its virtual camera is already signed).
- Machine: mudmini, macOS 26.5.2, Apple silicon. Xcode 26.6 (plus first-launch components and
  the Metal toolchain), `cmake`, `ninja`, `xcodegen` installed 2026-09-26; exact steps in
  `docs/BUILD-mudmini.md`.
- Chris's existing camera apps: OBS, Hovercraft (presenter overlay, not a tracker).

## Hardware and pipeline facts (from upstream, verified against source 2026-09-26)

- **Chris's C1 differs from upstream's (measured 2026-09-26, T-001/T-006):** USB `03e7:f63b` as a
  webcam (upstream: `f63d`); DepthAI bootloader reports **0.0.0** (upstream: 0.0.15); in its
  bootloader for only ~5 s after power-on; plain-UVC modes: 1080p60 only; depthai names the sensor
  **IMX378**, not the IMX582 below. Booting needs the T-006 kick and a replug with the app searching
  (`docs/BUILD-mudmini.md`). T-002 must check the real sensor modes before relying on the list below.
- **A second unit matches (2026-09-26, serial `…10C192A5D200`):** `f63b`, bootloader 0.0.0, IMX378, boots
  first try with T-006, 30.1 fps · p50 49 ms.
- **A third unit is upstream's kind (2026-09-26, serial `1944…3010A1DA5F1300`):** `f63d`, bootloader
  0.0.15, sensor LCM48 (the 48 MP IMX582 module); opens any time with no replug, first frame in
  ~3 s, 30.0 fps · p50 46 ms. Survey: 2 × IMX378 / 0.0.0 (incl. the daily unit), 1 × LCM48 / 0.0.15;
  a fourth unit wasn't found. Chris chose it as the follow-mode target and daily webcam (Decisions).

- Pipeline today: `ColorCamera` (THE_4_K, `setIspScale(1,2)` for 1080p) → `video` (NV12) →
  `XLinkOut "video"`; `XLinkIn` carries `CameraControl`. `Sources/OpalBridge/OpalBridge.cpp`
  lines ~190–230 build it; `opal_set_focus_region` / `opal_set_exposure_region` convert
  normalized rects to sensor coordinates (lines ~555–605).
- Sensor readout modes: 3840×2160 (2–42 fps), 4000×3000 (≤30), 5312×6000 (≤10). No native
  1080p. Sensor is mounted upside down; the ISP rotates (`setImageOrientation`).
- Host side: `OpalDevice.swift` (connect/retry/watchdog, frame sink into IOSurface-backed
  `CVPixelBuffer`), `CameraSettings.swift` (hot vs cold settings; cold = pipeline rebuild),
  `CameraModel.swift` (frame fan-out, subject metering with dead-band),
  `Render/MatteProvider.swift` (Vision person segmentation, `SubjectInfo.bounds` in normalized
  output coords), `VirtualCamera/VirtualCameraFeeder.swift` (CMIO sink, BGRA, 30 fps timing).
- depthai-core pinned at v2.30.0 by `scripts/bootstrap.sh` (Hunter builds deps; zlib and
  CMake 4 workarounds already in the script).

## Links

- **Repo:** https://github.com/digitalmud/open-opal (fork; `upstream` = https://github.com/alii/open-opal)
- **Tracker:** `.tickets/` here; release manifest `.tickets/0-releases/c1-follow.md`
- **Research that led here:** Chat session note `~/Code/log/sessions/2026-09-26.md`
- **Related projects read for design:** jtannahill/opal-c1-depthai (4K ImageManip crops, host
  Vision faces, pyvirtualcam), cansik/open-opal (Python control), daniel-p-green/opal-c1-studio-2026
  (UVC-only postmortem: hardware controls blocked in user session)

## Key People

- Chris — owner, daily user, taste gate on framing feel and signing decision
- alii — upstream author (MIT); PR back is welcome but not assumed

## Deliverables

- `/Applications/OpenOpal.app` Release build with follow mode (T-005)

## References

| File | Description |
|---|---|
| `README.md` (upstream) | hardware facts, pipeline diagram, build steps |
| `docs/SIGNING.md` (upstream) | Developer ID + notarization steps for the virtual camera |
| `~/Code/knowledge/verification-runs-in-stage-environment.md` | how verification blocks must be written |

## Decisions

- **2026-09-26 (afternoon) — Follow on the stock webcam, not the depthai path (re-plan).** Chris
  wants a normal-call look, stock auto image, no blur, no OBS. The stock feed of camera 3 is 1440p/4K;
  a prototype followed him at 8 % CPU / 42 MB. Locked settings: 1.33× at his normal seat → 1.0× as he
  leans in, 12 % dead zone, gentle glide, centre on (re)acquire, 1.5 s hold then wide. The depthai
  design picks (D, then E1) are superseded; the depthai code stays as the advanced path.

- **2026-09-26: Follow mode targets camera 3 (LCM48 / IMX582, f63d, bootloader 0.0.15).** Chris's
  choice after the three-unit survey. It becomes his daily webcam: no replug routine, ~3 s boot,
  48 MP sensor with upstream's documented modes. The two IMX378 units stay usable through T-006
  as spares; T-008 drops to low priority.

- **2026-09-26: Send this C1's bootloader the bare USB-ROM-boot command.** Chris approved command
  0 (`UsbRomBoot`) only, for a bootloader reporting exactly 0.0.0, sent only after depthai's own
  attempt refuses it. It writes nothing, and a replug restores stock. The alternative that
  depthai's own warning suggests (flash a newer bootloader) stays forbidden (T-006).

- **2026-09-26 — Fork upstream rather than start from the Python projects.** Upstream already
  has the DepthAI bridge, Metal path, controls UI and CMIO virtual camera in Swift; follow
  mode is one feature on top. Python + pyvirtualcam was the faster prototype but a second
  app to maintain.
- **2026-09-26 — Crop on the camera, detect on the host (Vision).** See Context for the
  bandwidth and throughput reasons. (The camera-side mechanism was first planned as
  `ImageManip`; T-002 measured it and it lost to the ISP window, below.)
- **2026-09-26 — Follow is a cold setting.** Turning it on swaps the pipeline (ISP 4K +
  a moving 1440p video window) and rebuilds; off returns to upstream's ISP-downscale path unchanged, so the
  default experience is never slower than upstream.
- **2026-09-26 — Never upscale.** Output is always a downscale or 1:1 of real sensor pixels.
  ~~Zoom range 1.0× to 2.0×~~: superseded by the next decision. While following, the zoom is
  1.5–2.0×; follow off is the 1.0× whole-room view.
- **2026-09-26 — Follow mode uses design D (T-002, Chris's pick).** The camera's ISP moves a
  2560×1440 window over the 4K frame (30 fps, p50 58 ms, measured moving every frame), and the
  Mac zooms 1.5–2.0× within it by downscaling to 1080p. No whole-room view while following;
  turning follow off returns to today's full-frame pipeline (~3 s switch). The fallback, if
  host zoom proves costly in T-003, is C (a 1080p window, pan only, 2× fixed, p50 50 ms).

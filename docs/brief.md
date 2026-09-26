# open-opal — follow mode for the Opal C1

**Area:** Personal / System
**Status:** Repo opened 2026-09-26 from Chat (Code/T-731). Fork of `alii/open-opal` cloned; nothing built yet. Blocker to clear first: Xcode is not installed on mudmini (only Command Line Tools), and upstream needs Xcode 26. Release `c1-follow`, tickets T-001 → T-005.

## Overview

Opal discontinued the C1 and Composer 2 refuses it (announcement 2026-06-01). The camera is a
Luxonis DepthAI device (Myriad X VPU + Sony IMX582 48 MP sensor), so `alii/open-opal` drives it
directly: it boots a DepthAI pipeline into the camera's RAM and gets Composer-grade controls back.
This fork adds what Chris asked for on 2026-09-26: a **Center Stage-style follow mode** that
keeps him framed by moving and resizing a crop window over the full 4K frame and shipping a
1080p result, so the zoom is a real crop of a sharper image, not an upscale.

## Context

- **Build order:** T-001 baseline build and run → T-002 crop pipeline spike (the design gate:
  can the camera crop+resize 4K→1080p at ≥28 fps and ≤90 ms?) → T-003 follow servo →
  T-004 controls and persistence → T-005 signed build and virtual camera.
- **Why the crop must happen on the camera:** full 4K NV12 over USB is ~370 MB/s, measured
  ~300 ms latency at 20 fps (upstream README). Upstream downscales on the ISP to 1080p for
  ~45 ms. Follow mode instead keeps the ISP at 4K and adds an `ImageManip` node that crops a
  moving window and resizes to 1920×1080 before the frame crosses USB. Bandwidth stays 1080p.
- **Why face detection stays on the host:** Apple Vision face rectangles run in ~5 ms a frame
  on the Mac; a detection network on the VPU costs about a third of throughput (jtannahill's
  measurements). The host runs a closed-loop servo: detect on the received 1080p frame,
  compute where the face sits relative to the current crop, nudge the crop. Closed loop makes
  it robust to the one-to-two frame lag between a crop command and the frame it produced.
- **Over plain UVC the C1 exposes exactly one format on this Mac: 1920×1080 at 60 fps** (probed
  2026-09-26 via AVFoundation). No 4K reaches the host without the DepthAI takeover. That is
  why host-only trackers (Reframe, OBS face tracker) can't give the result Chris wants.
- **Virtual camera needs a signed and notarized build** (CMIO system extension; ad-hoc signing
  is refused by AMFI). Without a Developer ID the app runs and previews but Zoom/Meet can't see
  it. Decision needed from Chris at T-005: use a digitalmud Apple Developer team, or fall back
  to a Syphon/NDI feed into OBS (installed, its virtual camera is already signed).
- Machine: mudmini, macOS 26.5.2, Apple silicon, Swift 6.3 toolchain via CLT. `cmake` present;
  `ninja`, `xcodegen`, Xcode missing as of 2026-09-26.
- Chris's existing camera apps: OBS, Hovercraft (presenter overlay, not a tracker).

## Hardware and pipeline facts (from upstream, verified against source 2026-09-26)

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

- **2026-09-26 — Fork upstream rather than start from the Python projects.** Upstream already
  has the DepthAI bridge, Metal path, controls UI and CMIO virtual camera in Swift; follow
  mode is one feature on top. Python + pyvirtualcam was the faster prototype but a second
  app to maintain.
- **2026-09-26 — Crop on the camera (ImageManip), detect on the host (Vision).** See Context
  for the bandwidth and throughput reasons. T-002 is the spike that confirms the numbers
  before T-003 builds the servo on it.
- **2026-09-26 — Follow is a cold setting.** Turning it on swaps the pipeline (ISP 4K +
  ImageManip) and rebuilds; off returns to upstream's ISP-downscale path unchanged, so the
  default experience is never slower than upstream.
- **2026-09-26 — Zoom range is 1.0× to 2.0×, never upscaled.** The crop window is between the
  full 3840×2160 frame and 1920×1080; output is always a downscale or 1:1.

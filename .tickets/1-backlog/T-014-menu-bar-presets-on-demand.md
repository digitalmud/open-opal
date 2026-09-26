---
id: T-014
type: ticket
title: Menu-bar app — follow settings, camera on demand for calls, no preview, status icon
status: backlog
priority: high
tags: [ux, menu-bar, presets, virtual-camera, reliability]
created: 2026-09-26
updated: 2026-09-26
kind: feature
depends_on: [T-005, T-010]
release: c1-follow
sprint: 6
---

## Description

**2026-09-26 re-plan (Chris approved):** the app now runs on the **stock webcam** (T-010), so
there are **no manual camera presets** (the exposure model below applied to the depthai path and is
parked with it). The settings are the follow controls instead: follow on/off; framing
(max zoom, default 1.33×); zoom out when I lean in (on + sensitivity); calm ↔ responsive (dead zone
+ glide); when I leave (hold, then back to wide), defaults = Chris's locked prototype values, raw
constants under Advanced. Still: menu bar only, launch at login, camera on demand, status icon for
an unplugged camera, no preview, no blur, no face metering. Measured prototype baseline: 8 % CPU,
42 MB. Sections below that describe presets/manual exposure are superseded.

Stub from a design conversation with Chris, 2026-09-26 14:04–14:33; `/scope` fleshes it out
after T-005 (the signed virtual camera) lands. Chris wants Open Opal to be **rock solid and
invisible**: "ideally I can save a preset to it, and it just applies my settings. No preview …
the app I'm using can be the preview."

Today, upstream's app is an ordinary windowed app. Closing the window stops the camera
(`OpenOpalApp.swift`: `.onDisappear { camera.stop() }`), and while it runs it holds the C1 all the
time. Camera settings exist only while the app drives the camera: they go into the camera's RAM
and are lost on replug. Nothing can be stored in the camera without flashing (forbidden), and macOS
doesn't expose them in plain-webcam mode. So the preset has to live in the app.

## Decided in conversation (Chris)

| Decision | Choice |
|---|---|
| Shape | Menu-bar item only: no Dock icon, no preview window, launch at login |
| Presets | Named presets; one active; applied on every camera takeover. Hold: exposure, white balance, **focus lock**, flicker, colour/detail |
| **Exposure model** (Chris, 14:36) | **Shutter locked to a flicker-safe 1/60 or 1/30 s, stored per preset; ISO (100–1600) is the only brightness control.** Built-in trio **Bright / Mid / Dark**. Starting ISOs to tune by eye: 1/60 s → 200 / 400 / 800; 1/30 s → 100 / 200 / 400 (e.g. Dark at 1/30 for less noise) |
| 1/30 s precision | Test for faint banding: upstream's bridge caps exposure at 33,000 µs (`std::clamp(c.exposureUs, 1, 33000)`), not 33,333 µs (true 1/30 = 4 × 120 Hz light pulses). If it bands, raise the cap to 33,333 µs if the sensor allows it at 30 fps. 1/60 s (16,667 µs) is exact |
| Blur | **None.** "I don't care about the fake blur": no blur UI, and the segmentation and depth models never load |
| Face metering | **None.** "Expose for my face … too janky". The camera's own auto-exposure stays available (runs on the camera, free); optionally a fixed exposure region, which needs no model |
| Unplugged camera | The menu-bar icon changes (colour or slash) and the menu says so; reconnect is automatic (T-011) |
| Camera use | On demand: take the C1 only while a call app uses "Open Opal Camera", give it back after (the camera extension already knows when a client starts or stops its stream; it switches its splash card on that) |
| Editing presets | "Edit presets…" opens a small window with a preview, only while editing |

## Measured baseline (camera 3, master build with preview, 2026-09-26 14:13–14:30)

CPU is % of one core; GPU is system-wide (12 % with the app closed).

| State | CPU | Memory | GPU |
|---|---|---|---|
| Blur off, face metering off | 9 % | ~310 MB | ~21 % |
| + face metering | 26 % | 312 MB | 24 % |
| + blur (Balanced mask) | 36 % | 322 MB | 51 % |
| + depth-graded blur | 36 % | 408 MB (depth model stays loaded after it's switched off) | 51 % |
| Chris's all-manual settings, blur and face off | 8.6 %, energy impact 9.6 | 413 MB (depth model still resident) | 27 % |

Target for this ticket: at or below the 9 % / ~300 MB floor while live, ~0 when no call is active,
and measured before and after (CLAUDE.md "Measure, don't assert").

## Chris's current settings (a candidate first preset, read from the app 14:29)

Manual exposure 1/48 s, ISO 400 · manual focus 140 · manual WB 5500 K · saturation −2 · contrast 0 ·
brightness 0 · flicker 60 Hz · sharpness 1 · luma/chroma denoise 1 · 1080p30 · rotate 180°.
Note: 1/48 s isn't a whole number of 60 Hz light pulses (120/s), so the presets use 1/60 or 1/30
(see Exposure model); anti-banding only protects auto-exposure.

## Open for scope

- How the app learns that a call app started or stopped the virtual camera (the extension → app
  signal), and how fast the camera takeover is (~3 s open on camera 3; T-012's retry adds ~5 s).
- Where presets are stored (PR #1 already persists the settings; build on it).
- Upstream-friendliness: likely a separate app mode/target rather than rewriting upstream's UI.

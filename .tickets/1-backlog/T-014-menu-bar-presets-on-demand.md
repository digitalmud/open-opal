---
id: T-014
type: ticket
title: Menu-bar app with presets — camera on demand for calls, no preview, no blur, status icon
status: backlog
priority: high
tags: [ux, menu-bar, presets, virtual-camera, reliability]
created: 2026-09-26
updated: 2026-09-26
kind: feature
depends_on: [T-005, T-011]
release: c1-follow
sprint: 3
---

## Description

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
| Presets | Named presets (e.g. "Day", "Evening"); one active; applied on every camera takeover. Hold: exposure (manual shutter/ISO, or the camera's own auto), white balance, **focus lock**, flicker, colour/detail |
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
Note: 1/48 s isn't a whole number of 60 Hz light pulses (120/s). Suggest **1/60 or 1/30** to avoid
banding under mains lighting (anti-banding only protects auto-exposure).

## Open for scope

- How the app learns that a call app started or stopped the virtual camera (the extension → app
  signal), and how fast the camera takeover is (~3 s open on camera 3; T-012's retry adds ~5 s).
- Where presets are stored (PR #1 already persists the settings; build on it).
- Upstream-friendliness: likely a separate app mode/target rather than rewriting upstream's UI.

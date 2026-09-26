---
id: T-004
type: ticket
title: Follow controls — toggle, framing presets, speed; persist settings
status: backlog
priority: medium
tags: [ui, settings, swiftui]
created: 2026-09-26
updated: 2026-09-26
depends_on: [T-003]
release: c1-follow
sprint: 3
---

## Description

Follow mode exists after T-003 but is only reachable by flipping a setting in code. This
ticket gives it a home in the Inspector next to the existing camera controls, shows what the
tracker sees, and makes every setting survive a relaunch — upstream keeps nothing across
launches, which is fine for a hobby build and wrong for a daily webcam.

## What to build

- **Inspector (`UI/Inspector.swift`):** a "Follow" section at the top of the camera controls:
  toggle *Follow me* (cold — uses upstream's existing "apply" reboot affordance and copy),
  segmented *Framing* (Tight · Medium · Wide), segmented *Speed* (Slow · Normal · Fast),
  and a one-line hint: "Crops the 4K sensor to 1080p, so zooming in stays sharp."
- **Preview overlay (`UI/MetalPreview.swift` or a SwiftUI overlay in `ContentView`):** when
  follow is on and *Show tracking* (a small toggle in the same section, default off) is
  set, draw the face box and the crop window outline in the preview. Off by default; never
  reaches the virtual camera.
- **Persistence:** `CameraSettings` gains `save()`/`load()` via `UserDefaults` for every
  user-facing setting (exposure, focus, WB, anti-banding, tuning, bokeh, follow). Load at
  init; save debounced 500 ms after any change. Cold settings loaded at launch apply to the
  first connect, so follow-on survives a relaunch.
- **Status pill:** when following, append "Follow 1.4×" (live zoom) to the existing latency/fps
  pill.

### Files touched

| Path | Action |
|---|---|
| `Sources/OpenOpal/UI/Inspector.swift` | edit |
| `Sources/OpenOpal/UI/ContentView.swift` | edit (overlay, pill) |
| `Sources/OpenOpal/Camera/CameraSettings.swift` | edit (persistence) |
| `Sources/OpenOpal/OpenOpalApp.swift` | edit (load at launch) |

Total: 4 files, <250 LOC.

## Charter decisions (locked 2026-09-26)

| Decision | Choice |
|---|---|
| Where Follow lives | First section of the Inspector, above Exposure — it is the feature |
| Persistence store | `UserDefaults` (standard). No files, no iCloud |
| Tracking overlay | Preview-only, default off |
| Mirror | Follow direction respects `mirrorPreview` for the overlay only; frames are never mirrored |

## Judgment calls — pre-decided

- Match upstream's SwiftUI style (glass panels, section headers); no new design language.
- Debounce with a `Task` on the main actor; no Combine.

## Acceptance criteria

- [ ] Follow section visible with the four controls and the hint; toggling follow shows upstream's reboot affordance and rebuilds the pipeline
- [ ] Quit and relaunch with follow on, framing Tight, speed Fast: all three are restored and the first connect uses the crop pipeline
- [ ] Tracking overlay shows face box and window when enabled; virtual camera frames contain no overlay (checked by Chris in a Zoom/Meet preview once T-005 lands, else in the app's export path)
- [ ] § Verification block passes

## Verification

```bash
set -euo pipefail
grep -q 'Follow me' Sources/OpenOpal/UI/Inspector.swift
grep -q 'UserDefaults' Sources/OpenOpal/Camera/CameraSettings.swift
xcodebuild -project OpenOpal.xcodeproj -scheme OpenOpal -configuration Release \
  -derivedDataPath build/DerivedData build -quiet
defaults read com.openopal 2>/dev/null | grep -q -i follow || echo "run the app once to seed defaults"
echo T-004 verification OK
```

## Constraints

- **Blast radius:** app target UI and settings; no bridge, no render changes.
- **Permission class:** ship freely.
- **Read/write boundaries:** `UserDefaults` for the app's bundle id only.

## Out of scope

- Keyboard shortcuts, menu bar mode, per-app profiles.

## Notes

- Upstream's `coldDirty` + "apply" flow in `Inspector.swift` is the pattern to reuse for the
  follow toggle. Bundle id is set in `project.yml` (`bundleIdPrefix`); if T-005 changes it,
  the defaults domain follows.

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

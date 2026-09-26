---
id: T-005
type: ticket
title: Signed Release build, virtual camera install, /Applications
status: backlog
priority: medium
tags: [signing, notarization, virtual-camera, release]
created: 2026-09-26
updated: 2026-09-26
depends_on: [T-001]
kind: chore
release: c1-follow
sprint: 3
---

## Description

Follow mode is only useful if Zoom, Meet and FaceTime can see the result. Upstream publishes
a CoreMediaIO system extension ("Open Opal Camera"), and macOS loads it only when the app is
Developer ID signed and notarized (`docs/SIGNING.md`). This ticket gets a signed build into
`/Applications` with the virtual camera working, or, if Chris decides against an Apple
Developer team, wires the fallback: a Syphon server in the app feeding OBS, whose virtual
camera is already signed and installed on this Mac.

## What to build

**Step 0 — decision (ask Chris, in the session, before anything else):** does he have or want
an Apple Developer Program membership (digitalmud or personal, USD 99/yr)?

**Path A — Developer ID (preferred):**
1. Chris performs the Apple-side setup from `docs/SIGNING.md` (certificate, App Group, two App
   IDs, two profiles, notarytool credentials). You prepare the exact list with our bundle ids.
2. Change `project.yml` `bundleIdPrefix` to `ca.digitalmud` (or Chris's choice) and the App
   Group / Mach service prefixes in both `Info.plist` and entitlements to match. Set
   `DEVELOPMENT_TEAM` via an **untracked** `Local.xcconfig` (add to `.gitignore`), never in
   `project.yml`.
3. `scripts/sign.sh` + `scripts/release.sh` as upstream documents; notarize; staple.
4. Copy to `/Applications`; in-app Advanced → Virtual Camera → Install; approve the system
   extension prompt (Chris). Confirm "Open Opal Camera" appears in `system_profiler
   SPCameraDataType` and shows the processed image in FaceTime.

**Path B — OBS fallback:** add a Syphon publisher (`Syphon` framework via SPM) fed from
`renderer.exportFrame`, gated by a setting; document the OBS scene (Syphon Client source →
Start Virtual Camera). Confirm OBS Virtual Camera shows the followed frame in FaceTime.

Either path: `docs/INSTALL.md` (≤40 lines) with the steps Chris will repeat on a future
machine, and the fork's `README.md` gains a short "Follow mode" section with one screenshot.

### Files touched

| Path | Action |
|---|---|
| `project.yml` | edit (bundle prefix) — Path A |
| `Sources/OpenOpal/Info.plist` | edit (ids) — Path A |
| `Sources/OpenOpalCameraExtension/Info.plist` | edit (ids) — Path A |
| `Sources/OpenOpal/OpenOpal.entitlements` | edit (App Group) — Path A |
| `Sources/OpenOpalCameraExtension/OpenOpalCameraExtension.entitlements` | edit (App Group) — Path A |
| `.gitignore` | append `Local.xcconfig` |
| `Sources/OpenOpal/VirtualCamera/SyphonPublisher.swift` | new — Path B only |
| `docs/INSTALL.md` | new |
| `README.md` | edit |

Total: ≤9 files, <200 LOC.

## Charter decisions (locked 2026-09-26)

| Decision | Choice |
|---|---|
| Preferred path | A (Developer ID). It is upstream's design and the only way the camera shows up everywhere without OBS running |
| Bundle prefix | `ca.digitalmud` unless Chris says otherwise |
| Team ID home | untracked `Local.xcconfig`; never committed |
| Fallback | Syphon → OBS. Not NDI (heavier, network-scoped) |

## Acceptance criteria

- [ ] Path A: `codesign -dv --verbose=2 /Applications/OpenOpal.app` shows a Developer ID authority; `spctl -a -vv` accepts; `system_profiler SPCameraDataType` lists "Open Opal Camera"; FaceTime shows the followed image (Chris)
- [ ] Path B: OBS Virtual Camera shows the followed image in FaceTime (Chris); `docs/INSTALL.md` documents the OBS scene
- [ ] No team ID, profile or password in any tracked file (`git grep` clean)
- [ ] § Verification block passes

## Verification

```bash
set -euo pipefail
test -s docs/INSTALL.md
grep -q -i 'follow' README.md
! git grep -n -E 'DEVELOPMENT_TEAM: [A-Z0-9]{10}' -- project.yml
test -d /Applications/OpenOpal.app
system_profiler SPCameraDataType | grep -q -E 'Open Opal Camera|OBS Virtual Camera'
echo T-005 verification OK
```

## Constraints

- **Blast radius:** identifiers, signing scripts, one doc; optional Syphon publisher.
- **Permission class:** ask first for the team decision, every keychain/certificate step and
  the system-extension approval. Ship freely for the rest.
- **Read/write boundaries:** no secrets in git; no changes to upstream's signing script logic
  beyond identifiers.

## Out of scope

- App Store distribution. Auto-update. Publishing a GitHub release from our fork.

## Notes

- `docs/SIGNING.md` and `docs/RELEASING.md` (upstream) are the runbooks; `project.yml`
  comments explain why ad-hoc signing cannot load the extension (AMFI refuses the entitlement).
- OBS is installed at `/Applications/OBS.app` and its virtual camera extension is already
  registered on this Mac (seen in `system_profiler` 2026-09-26).

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

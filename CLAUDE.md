# open-opal — Chris's fork: Open Opal with a Center Stage-style follow mode for the Opal C1

You are building on a fork of `alii/open-opal`, a native macOS app that drives the Opal C1
webcam directly over USB (DepthAI on the camera's Myriad X). Opal discontinued the C1 and its
Composer software in 2026; this fork adds the one feature Chris misses most: **the frame follows
him**, cropped from the 4K sensor down to 1080p, so the crop costs no sharpness.

## Pickup

Read `docs/brief.md` first (state, hardware facts, decisions), then `worklog.md`, then the
ticket you are on. Upstream's own `README.md` § The hardware and § How it works are accurate
and worth reading once. The lifecycle runs inline here: `/scope` a ticket, `/build` it,
`/review` it, `/deploy` it, in that order, one ticket at a time.

## Rules

- **Never flash the C1.** Everything runs from RAM; a replug restores the stock UVC firmware.
  Nothing in this repo may call a flash or bootloader-write API. Treat any depthai call with
  `flash` in its name as forbidden.
- **The C1 is Chris's daily webcam.** When the app holds the camera, the system "Opal C1"
  camera disappears. Don't leave a build holding the device when you stop; quit the app.
- **Stay upstream-friendly.** Keep changes in additive files and small, well-commented edits so
  a PR back to `alii/open-opal` stays possible. Don't reformat files you aren't changing.
- **Measure, don't assert.** Latency and fps claims come from the app's own telemetry (the
  toolbar shows them) or from `opal_get_telemetry`, pasted into the ticket's Build log.
- **Regions are sensor coordinates.** AE/AF regions are in 3840×2160 sensor space, not output
  space. With a moving crop, every region must be mapped through the live crop rect first.
- **Signing is Chris's.** Developer ID certificates, team IDs and notarization credentials are
  set up by Chris; never commit a team ID, a provisioning profile or a password. The upstream
  `project.yml` carries the upstream author's team ID and must not be used for our builds.
- Plain language, always. Verify before claiming.

## Permissions

- **Ship freely:** code, tests, docs and tickets in this repo; `scripts/bootstrap.sh`,
  `scripts/fetch-models.sh`, `xcodegen`, `xcodebuild` into `build/`; running the app from
  `build/`; copying a Release build to `/Applications` when a ticket says so; `brew install`
  of the three build deps (`cmake ninja xcodegen`).
- **Ask first — Chris, in the session:** installing Xcode; installing or approving a system
  extension (the virtual camera); anything touching keychain, certificates or Apple Developer
  accounts; edits outside this repo; pushing to any remote other than `origin`
  (`digitalmud/open-opal`).
- **Never:** flash the camera; commit secrets or team IDs; open a PR to upstream without Chris.

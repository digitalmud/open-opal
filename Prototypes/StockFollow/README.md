# Stock-webcam follow prototype (2026-09-26)

A throwaway-quality single-file test that proved the simplest follow design: read the C1 as a
**normal webcam** (stock firmware, nothing changed on the camera) at 2560×1440, find the face with
Apple Vision (~10 Hz), glide a crop towards it, and show the 1920×1080 result in a window. No
depthai, no camera takeover, no virtual camera (that needs the signed build, T-005).

```sh
swiftc -O Prototypes/StockFollow/main.swift -o build/followtest
./build/followtest 1.33 0.27 0.33     # [maxZoom] [faceNear] [faceClose]; F = follow on/off, Q = quit
```

## Settings Chris locked (tuned live on camera 3, 15:00–15:11)

| Setting | Value |
|---|---|
| Framing at normal seat | **1.33×** (1440p limit without upscaling) |
| Zoom out when leaning in | linear ramp: 1.33× at face ≤ 27 % of frame height → **1.0×** at 33 %; end stops always reachable |
| Pan dead zone | **12 %** of the view (size dead zone 12 %) |
| Pan motion | spring ω 2.4, damping ζ 1.25 (gentle, slightly over-damped); zoom ω 1.5 |
| (Re)acquire | centre on the face immediately, then the dead zone applies |
| Lost face | hold **1.5 s**, then glide back to the full wide view (ω 2.8, ~1.5 s) |
| Face detection | `VNDetectFaceRectanglesRequest` every 3rd frame, one in flight; several faces → union box |

Learned on the way: a "head-and-shoulders" target (face = 22 % of the view) never zooms at Chris's
desk: his face is already 23–32 % of the *full* frame, so the full frame is already normal-call
framing. Follow needs a little extra zoom (up to 1.33×) to have room to move.

## Measured (camera 3, 20 s, while running with its preview window)

CPU **8.1 %** of one core · energy impact **8.2** · memory **42 MB** · GPU ~+6 points system-wide.
Stock feed: 2560×1440 at 29.9 fps; frames reach the app ~2 ms after their USB timestamp
(glass-to-glass latency not yet measured).

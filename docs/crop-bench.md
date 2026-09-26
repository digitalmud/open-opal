# Crop bench — on-camera crop pipelines for follow mode (T-002)

Measured 2026-09-26, 10:27–10:39, on camera 3: serial `19443010A1DA5F1300` (Opal C1, USB `03e7:f63d`,
bootloader 0.0.15, sensor LCM48 / IMX582), depthai-core v2.30.0, Release bridge, 30 fps,
orientation ROTATE_180. The bench is `Sources/OpalBridge/test/crop_bench.c`; the modes are in
`Sources/OpalBridge/CropPipeline.cpp`.

**Method:** open with the mode's pipeline; wait 5 s so the warm-up backlog drains; **static**: 60 s
with the window still; **moving**: 60 s with the window moved 30 times a second along a smooth
pan + zoom path (clamped to what each mode allows). fps = frames received ÷ elapsed; latency is
sensor timestamp → host, over every frame. **Pass mark (moving): ≥ 28 fps and p50 ≤ 90 ms.**

## Results

| Mode | Window | Zoom range | Static fps · p50 · p95 | Moving fps · p50 · p95 | Moving passes? |
|---|---|---|---|---|---|
| NONE (baseline, upstream) | whole frame, ISP ÷2 | 1× | 30.01 · 49.7 ms · 53.3 ms | — | (no window) |
| MANIP_4K (A) | ImageManip on 4K, resize → 1080p | 1.0–2.0× | 30.00 · 49.9 · 54.1 | **15.13 · 123.7 · 226.9** | **no** |
| MANIP_1440 (B) | ImageManip on 1440p, resize → 1080p | 1.0–1.33× | 30.01 · 49.9 · 54.2 | **21.22 · 106.3 · 135.7** | **no** |
| WINDOW_1080 (C) | ISP video window 1920×1080 over 4K | 2× fixed, pan | 30.01 · 49.9 · 54.5 | **30.00 · 49.9 · 53.8** | yes |
| WINDOW_1440 (D) | ISP video window 2560×1440 over 4K | pan; host zooms 1.5–2.0× | 30.00 · 57.7 · 61.4 | **30.00 · 57.8 · 61.1** | yes |

Supplementary (not the pass mark): MANIP_4K with the window moved **10×/s** instead of every frame:
**21.83 fps · p50 66.3 ms · p95 155.4 ms**, still a fail.

Thumbnails (grey, 320 px, local only under `build/crop-bench/`) confirm each window really moved:
top-left and bottom-right crops at 2× (A, C), 1.33× (B, clamped) and 1.5× (D).

## What the numbers say

- **ImageManip is fast while the window stays put and slow when it moves.** With a fixed config
  it crops 4K at full rate; reconfiguring it every frame halves A's frame rate and more than
  doubles its latency. B is only somewhat better. Follow mode moves the window constantly, so
  both fail. This matches jtannahill's 5.9 fps (with a face network on the camera too).
- **Moving the ISP's own video window is free.** C and D hold 30.00 fps with the window moving
  every frame, at the same latency as a still window.
- D's 1440p window costs ~8 ms more latency than 1080p (more bytes over USB: ~166 MB/s at 30 fps,
  well inside the 10 Gbps link depthai reports) and gives the host room to zoom by downscaling.
- Every mode opened in 2.2–3.3 s with no replug on this camera.

## Recommendation

**D (WINDOW_1440)** for follow mode. By the Charter rule (widest real zoom among passing modes:
A > D > B > C), A and B fail, so D wins. It holds 30 fps at p50 58 ms while panning every frame,
and a 2560×1440 window leaves the host a 1.5–2.0× zoom range done as a downscale (never an
upscale) to 1920×1080.

What D does **not** give, and T-003 must design around:
- **No 1.0× (whole-room) view while following.** The widest D view is 1.5×. "Zoom all the way out"
  means switching to the normal pipeline (mode NONE), a ~3 s reopen. A reasonable split: follow
  = D, follow off = today's full-frame view.
- The host-side zoom (1440p → 1080p crop + scale, per frame) isn't measured here. It runs on the
  Mac's GPU in the existing Metal path, so it should be cheap, but T-003 must measure it.
- Focus and exposure regions must be mapped through the window position (T-003).

**C (WINDOW_1080)** is the fallback if host zoom proves costly: lowest latency (p50 50 ms), but
pan only, at a fixed 2×.

## Reproduce

```sh
B=$PWD/build/bench-bridge
CMAKE_POLICY_VERSION_MINIMUM=3.5 cmake -S Sources/OpalBridge -B "$B" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "$B"
clang -ISources/OpalBridge/include Sources/OpalBridge/test/crop_bench.c -L"$B" -lOpalBridge \
  -Wl,-rpath,"$B" -Wl,-rpath,"$PWD/vendor/install/lib" -o build/crop_bench
./build/crop_bench --modes 0,1,2,3,4 --secs 60          # ~11 min of camera time
./build/crop_bench --modes 1 --secs 60 --rate 10        # the supplementary run
```

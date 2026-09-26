// Checks for FollowView's pure geometry (T-003, design E1). No camera, no XCTest target:
//   swiftc -O Sources/OpenOpal/Follow/FollowView.swift Tests/FollowViewCheck/main.swift -o followcheck && ./followcheck
import CoreGraphics
import Foundation

var failures = 0
func check(_ ok: Bool, _ what: String) {
    if !ok { failures += 1; print("FAIL: \(what)") }
}
func near(_ a: CGFloat, _ b: CGFloat, _ eps: CGFloat = 1e-6) -> Bool { abs(a - b) <= eps }
typealias V = FollowView.ViewRect
let src = FollowView.sourcePixels   // 2560x1440

// Zoom clamps to 1.0x...1.33x and stays in the frame.
let tooWide = FollowView.clamp(V(x: -0.2, y: 0, w: 1.4))
check(near(tooWide.w, 1.0) && near(tooWide.x, 0), "zoom clamps at 1.0x (w \(tooWide.w))")
let tooTight = FollowView.clamp(V(x: 0.4, y: 0.4, w: 0.1))
check(near(tooTight.w, 0.75), "zoom clamps at 1.33x (w \(tooTight.w))")
let offEdge = FollowView.clamp(V(x: 0.9, y: -0.2, w: 0.8))
check(offEdge.x >= 0 && offEdge.x + offEdge.w <= 1 + 1e-9 && offEdge.y >= 0 && offEdge.y + offEdge.w <= 1 + 1e-9,
      "clamped view stays inside the frame")
let bad = FollowView.clamp(V(x: .nan, y: 0.2, w: .infinity))
check(bad.w.isFinite && bad.x.isFinite && bad.y.isFinite, "non-finite input clamps to finite")
check(near(FollowView.clamp(V(x: 0.1, y: 0.1, w: 0.8)).center.x, 0.5), "clamp keeps the centre when it can")

// The host crop sits inside the 2560x1440 frame, is 16:9, and is never smaller
// than 1920 wide (no upscale to 1080p).
for v in [V.start, V(x: 0, y: 0, w: 0.75), V(x: 0.25, y: 0.25, w: 0.75), V(x: 0.1, y: 0.05, w: 0.85),
          V(x: 0.9, y: 0.9, w: 0.5)] {
    let crop = FollowView.hostCrop(view: v)
    check(crop.minX >= -1e-6 && crop.maxX <= src.width + 1e-6 && crop.minY >= -1e-6 && crop.maxY <= src.height + 1e-6,
          "host crop \(crop) inside the source frame")
    check(crop.width >= 1920 - 1e-6 && near(crop.width / crop.height, 16.0 / 9.0, 1e-6),
          "host crop \(crop) is 16:9 and never smaller than 1920 wide")
}
let whole = FollowView.hostCrop(view: .start)
check(near(whole.width, 2560) && near(whole.height, 1440) && near(whole.minX, 0), "1.0x view = the whole frame")
let tight = FollowView.hostCrop(view: V(x: 0.25, y: 0.25, w: 0.75))
check(near(tight.width, 1920) && near(tight.minX, 640) && near(tight.minY, 360), "1.33x bottom-right = last 1920x1080")

// Mapping: output corners land on the view's corners.
let v = V(x: 0.2, y: 0.1, w: 0.8)
check(FollowView.toFullFrame(CGPoint(x: 0, y: 0), view: v) == CGPoint(x: 0.2, y: 0.1), "output (0,0) -> view origin")
let br = FollowView.toFullFrame(CGPoint(x: 1, y: 1), view: v)
check(near(br.x, 1.0) && near(br.y, 0.9), "output (1,1) -> view far corner")
let r = FollowView.toFullFrame(CGRect(x: 0.5, y: 0.5, width: 0.25, height: 0.25), view: v)
check(near(r.minX, 0.6) && near(r.width, 0.2), "output rect maps and scales with the zoom")

// Round trip: the crop's centre, back in full-frame coordinates, is the view's centre.
for view in [V.start, V(x: 0.05, y: 0.2, w: 0.78), V(x: 0.2, y: 0.02, w: 0.75)] {
    let c = FollowView.clamp(view)
    let crop = FollowView.hostCrop(view: c)
    check(near(crop.midX / src.width, c.center.x, 1e-6) && near(crop.midY / src.height, c.center.y, 1e-6),
          "round trip centre for \(c)")
}

if failures == 0 {
    print("FollowViewCheck OK")
} else {
    print("FollowViewCheck: \(failures) failure(s)")
    exit(1)
}

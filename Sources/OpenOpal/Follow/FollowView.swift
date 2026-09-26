import CoreGraphics

/// Follow mode's geometry, as pure functions (no Metal, no device), so it can be
/// checked without a camera (`Tests/FollowViewCheck`).
///
/// Everything is in the FULL sensor frame, normalised 0...1 with a top-left
/// origin, after the ISP's orientation fix. The frame is 16:9 and so is every
/// view, which is why a width fraction is also the height fraction.
///
/// Design E1 (T-003, Chris's pick 2026-09-26): the camera's ISP scales the whole
/// 4K frame to 2560x1440 and sends all of it; the Mac crops the view out of that
/// and scales it to 1920x1080. Framing stays like a normal video call: 1.0x
/// (everything) to 1.33x (1920 of the 2560 pixels), so the Mac never upscales.
enum FollowView {

    /// What the camera sends while following (ISP 2/3 of 3840x2160).
    static let sourcePixels = CGSize(width: 2560, height: 1440)
    /// Widest view (1.0x, the whole frame) and tightest (1.33x, 1920 source pixels).
    static let widest: CGFloat = 1.0
    static let tightest: CGFloat = 1920.0 / 2560.0

    /// The part of the full frame the viewer sees. `w` is also the height fraction.
    struct ViewRect: Equatable, Sendable {
        var x: CGFloat
        var y: CGFloat
        var w: CGFloat

        var zoom: CGFloat { 1 / w }
        var center: CGPoint { CGPoint(x: x + w / 2, y: y + w / 2) }
        var rect: CGRect { CGRect(x: x, y: y, width: w, height: w) }

        /// The whole frame: where following starts.
        static let start = ViewRect(x: 0, y: 0, w: widest)
    }

    /// Keeps the zoom inside 1.0x...1.33x and the view inside the frame, holding
    /// the centre where possible. Non-finite input falls back to the whole frame.
    static func clamp(_ v: ViewRect) -> ViewRect {
        let w = min(max(v.w.isFinite ? v.w : widest, tightest), widest)
        let cx = v.x.isFinite && v.w.isFinite ? v.x + v.w / 2 : 0.5
        let cy = v.y.isFinite && v.w.isFinite ? v.y + v.w / 2 : 0.5
        return ViewRect(x: min(max(cx - w / 2, 0), 1 - w),
                        y: min(max(cy - w / 2, 0), 1 - w),
                        w: w)
    }

    /// The view in the source frame's pixels: the rectangle the Mac crops.
    /// `source` is the received frame's size (normally `sourcePixels`).
    static func hostCrop(view: ViewRect, source: CGSize = sourcePixels) -> CGRect {
        let v = clamp(view)
        let w = min(v.w * source.width, source.width)
        let h = min(v.w * source.height, source.height)
        return CGRect(x: min(max(v.x * source.width, 0), source.width - w),
                      y: min(max(v.y * source.height, 0), source.height - h),
                      width: w, height: h)
    }

    /// An output-normalised point (0...1 across the picture the viewer sees) in
    /// full-frame coordinates, which is what the camera's focus and exposure
    /// regions need (the bridge converts them to sensor pixels).
    static func toFullFrame(_ p: CGPoint, view: ViewRect) -> CGPoint {
        CGPoint(x: view.x + p.x * view.w, y: view.y + p.y * view.w)
    }

    static func toFullFrame(_ r: CGRect, view: ViewRect) -> CGRect {
        CGRect(x: view.x + r.minX * view.w, y: view.y + r.minY * view.w,
               width: r.width * view.w, height: r.height * view.w)
    }
}

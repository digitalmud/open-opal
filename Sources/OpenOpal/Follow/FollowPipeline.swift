import CoreGraphics
import CoreVideo
import Foundation
import OSLog

private let log = Logger(subsystem: "com.openopal", category: "follow")

/// Owns follow mode's view rect and turns each camera frame into the 1080p
/// picture of that view (T-003, design E1).
///
/// The camera sends the whole frame (2560x1440) and never moves anything, so
/// every frame can be cropped against the current view directly: no matching
/// frames to camera-side windows.
///
/// Thread-safe: `setView` may be called from any thread (menu now, the face
/// servo later); `process` runs on the frame path.
final class FollowPipeline: @unchecked Sendable {

    private let lock = NSLock()
    private var view = FollowView.ViewRect.start
    private let zoomer: HostZoom
    private var framesSinceLog = 0

    init?() {
        guard let z = HostZoom() else {
            log.error("HostZoom unavailable: follow mode can't zoom")
            return nil
        }
        zoomer = z
    }

    /// Back to the whole frame (call when a follow pipeline opens).
    func reset() { setView(.start) }

    var currentView: FollowView.ViewRect {
        lock.lock(); defer { lock.unlock() }
        return view
    }

    func setView(_ requested: FollowView.ViewRect) {
        let v = FollowView.clamp(requested)
        lock.lock(); view = v; lock.unlock()
    }

    /// The zoomed 1920x1080 frame for the current view, or nil to drop it.
    func process(_ frame: CVPixelBuffer) -> CVPixelBuffer? {
        let source = CGSize(width: CVPixelBufferGetWidth(frame), height: CVPixelBufferGetHeight(frame))
        let out = zoomer.zoom(frame, crop: FollowView.hostCrop(view: currentView, source: source))
        logStatsOccasionally()
        return out
    }

    /// Every 150 frames (~5 s), the zoom's own cost, for measuring (T-003).
    private func logStatsOccasionally() {
        lock.lock()
        framesSinceLog += 1
        let due = framesSinceLog >= 150
        if due { framesSinceLog = 0 }
        let v = view
        lock.unlock()
        guard due else { return }
        let s = zoomer.stats
        log.info("host zoom p50 \(s.p50, format: .fixed(precision: 2)) ms p95 \(s.p95, format: .fixed(precision: 2)) ms · view \(Double(v.zoom), format: .fixed(precision: 2))x")
    }
}

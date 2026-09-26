import CoreVideo
import Foundation
import VideoToolbox

/// The Mac half of follow mode's zoom (design D, T-002/T-003): crops a rectangle
/// out of the camera's 2560x1440 NV12 window and scales it to 1920x1080 NV12 on
/// the hardware scaler, before the renderer sees the frame. Everything after it
/// (matte, bokeh, preview, virtual camera) keeps receiving plain 1080p.
///
/// The crop is the source buffer's clean aperture (offsets from its centre) with
/// kVTScalingMode_CropSourceToCleanAperture; VideoToolbox has no separate "source
/// crop" property in the Swift SDK. Measured at scope: p50 1.44 ms per frame.
final class HostZoom: @unchecked Sendable {

    static let outputSize = (w: 1920, h: 1080)

    private let lock = NSLock()
    private var session: VTPixelTransferSession?
    private var pool: CVPixelBufferPool?
    private var recentMs: [Double] = []

    init?() {
        var s: VTPixelTransferSession?
        guard VTPixelTransferSessionCreate(allocator: nil, pixelTransferSessionOut: &s) == noErr, let s else { return nil }
        VTSessionSetProperty(s, key: kVTPixelTransferPropertyKey_ScalingMode,
                             value: kVTScalingMode_CropSourceToCleanAperture)
        session = s

        // Same format and backing as the camera's own frames (OpalDevice.FrameSink).
        let attrs: [String: Any] = [
            kCVPixelBufferPixelFormatTypeKey as String: kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange,
            kCVPixelBufferWidthKey as String: Self.outputSize.w,
            kCVPixelBufferHeightKey as String: Self.outputSize.h,
            kCVPixelBufferIOSurfacePropertiesKey as String: [:],
            kCVPixelBufferMetalCompatibilityKey as String: true,
        ]
        var p: CVPixelBufferPool?
        CVPixelBufferPoolCreate(nil, [kCVPixelBufferPoolMinimumBufferCountKey: 4] as CFDictionary,
                                attrs as CFDictionary, &p)
        guard let p else { return nil }
        pool = p
    }

    deinit {
        if let session { VTPixelTransferSessionInvalidate(session) }
    }

    /// `crop` is in `source` pixels. Returns a fresh 1920x1080 buffer, or nil.
    func zoom(_ source: CVPixelBuffer, crop: CGRect) -> CVPixelBuffer? {
        lock.lock()
        defer { lock.unlock() }
        guard let session, let pool else { return nil }

        var out: CVPixelBuffer?
        CVPixelBufferPoolCreatePixelBuffer(nil, pool, &out)
        guard let out else { return nil }

        // Even-pixel edges keep the 4:2:0 chroma planes aligned with luma.
        let sw = CGFloat(CVPixelBufferGetWidth(source)), sh = CGFloat(CVPixelBufferGetHeight(source))
        let w = (crop.width / 2).rounded() * 2, h = (crop.height / 2).rounded() * 2
        let x = min(max((crop.minX / 2).rounded() * 2, 0), sw - w)
        let y = min(max((crop.minY / 2).rounded() * 2, 0), sh - h)
        let aperture = [
            kCVImageBufferCleanApertureWidthKey: w,
            kCVImageBufferCleanApertureHeightKey: h,
            kCVImageBufferCleanApertureHorizontalOffsetKey: x + w / 2 - sw / 2,
            kCVImageBufferCleanApertureVerticalOffsetKey: y + h / 2 - sh / 2,
        ] as CFDictionary
        CVBufferSetAttachment(source, kCVImageBufferCleanApertureKey, aperture, .shouldNotPropagate)

        let t0 = DispatchTime.now().uptimeNanoseconds
        let status = VTPixelTransferSessionTransferImage(session, from: source, to: out)
        let ms = Double(DispatchTime.now().uptimeNanoseconds - t0) / 1e6
        CVBufferRemoveAttachment(source, kCVImageBufferCleanApertureKey)

        recentMs.append(ms)
        if recentMs.count > 120 { recentMs.removeFirst(recentMs.count - 120) }
        return status == noErr ? out : nil
    }

    /// p50 / p95 of the last 120 zooms, in milliseconds.
    var stats: (p50: Double, p95: Double) {
        lock.lock()
        defer { lock.unlock() }
        let s = recentMs.sorted()
        guard !s.isEmpty else { return (0, 0) }
        return (s[s.count / 2], s[min(s.count - 1, s.count * 95 / 100)])
    }
}

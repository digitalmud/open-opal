// Throwaway follow test (open-opal, 2026-09-26): the stock Opal C1 webcam at 2560x1440 ->
// Apple Vision face detection (~10 Hz) -> smoothed crop, 1.0-1.33x -> 1920x1080 shown in a window.
// Nothing is changed on the camera. Keys: F toggles follow on/off, Q quits.
import AppKit
import AVFoundation
import CoreMedia
import VideoToolbox
import Vision

let srcW: CGFloat = 2560, srcH: CGFloat = 1440
let widest: CGFloat = 1.0, tightest: CGFloat = 1920.0 / 2560.0   // 1.0x ... 1.33x
// Framing (Chris, 15:0x): maxZoom while his face is <= faceNear of the frame height (normal seat),
// sliding linearly to 1.0x by faceClose (leaning in). Args: [maxZoom] [faceNear] [faceClose],
// defaults 1.33 0.27 0.33.
func arg(_ i: Int, _ d: Double) -> Double { CommandLine.arguments.count > i ? (Double(CommandLine.arguments[i]) ?? d) : d }
let maxZoomArg = arg(1, 1.33)
let faceNear = CGFloat(arg(2, 0.27)), faceClose = CGFloat(arg(3, 0.33))
let fixedW: CGFloat = min(max(1 / CGFloat(maxZoomArg), tightest), widest)   // tightest view we allow
let faceHeightTarget: CGFloat = 0.22     // face height as a fraction of the view: head & shoulders
let eyeLine: CGFloat = 0.42              // face centre sits at 42 % down the view
let deadCentre: CGFloat = 0.12, deadSize: CGFloat = 0.12   // pan dead zone 12 % of the view (Chris, 15:10)
let holdLost: Double = 1.5               // seconds to hold after losing the face, then ease wide

struct View { var cx: CGFloat, cy: CGFloat, w: CGFloat }

func clampView(_ v: View) -> View {
    let w = min(max(v.w, fixedW), widest)
    return View(cx: min(max(v.cx, w / 2), 1 - w / 2), cy: min(max(v.cy, w / 2), 1 - w / 2), w: w)
}

final class Follower: NSObject, AVCaptureVideoDataOutputSampleBufferDelegate {
    let lock = NSLock()
    var followOn = true
    // spring state (x, y, zoom) and target
    var cur = View(cx: 0.5, cy: 0.5, w: fixedW), vel = (x: CGFloat(0), y: CGFloat(0), w: CGFloat(0))
    var target = View(cx: 0.5, cy: 0.5, w: fixedW)
    var lastFaceAt: Double = 0, faceSeen = false, lastLog: Double = 0, srcDims = "?"
    var lastT: Double = 0
    var visionBusy = false, frameCount = 0, fpsFrames = 0, fpsT0 = CACurrentMediaTime(), fps: Double = 0
    let visionQ = DispatchQueue(label: "vision")
    var session: VTPixelTransferSession?
    var pool: CVPixelBufferPool?
    weak var layer: AVSampleBufferDisplayLayer?

    override init() {
        super.init()
        VTPixelTransferSessionCreate(allocator: nil, pixelTransferSessionOut: &session)
        VTSessionSetProperty(session!, key: kVTPixelTransferPropertyKey_ScalingMode, value: kVTScalingMode_CropSourceToCleanAperture)
        let attrs: [String: Any] = [kCVPixelBufferPixelFormatTypeKey as String: kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange,
                                    kCVPixelBufferWidthKey as String: 1920, kCVPixelBufferHeightKey as String: 1080,
                                    kCVPixelBufferIOSurfacePropertiesKey as String: [:]]
        CVPixelBufferPoolCreate(nil, nil, attrs as CFDictionary, &pool)
    }

    // Face -> desired view, with the dead zone deciding whether the target moves.
    func consider(face: CGRect?) {
        let now = CACurrentMediaTime()
        lock.lock(); defer { lock.unlock() }
        guard followOn else { target = View(cx: 0.5, cy: 0.5, w: 1); return }
        if now - lastLog > 1 {   // diagnostics, once a second
            lastLog = now
            if let f = face {
                print(String(format: "face h %.2f  cx %.2f cy %.2f  -> wants w %.2f (zoom %.2fx)  target zoom %.2fx  current zoom %.2fx  src %@",
                             f.height, f.midX, f.midY, f.height / faceHeightTarget, faceHeightTarget / f.height, 1 / target.w, 1 / cur.w, srcDims))
            } else { print("no face  current zoom \(1 / cur.w)x  src \(srcDims)") }
            fflush(stdout)
        }
        if let f = face {
            // (Re)acquired -- at launch or back in frame: centre on him straight away,
            // before the dead zone applies (Chris, 15:10).
            let acquired = !faceSeen
            lastFaceAt = now; faceSeen = true
            // Ramp: maxZoom at faceNear or smaller, 1.0x at faceClose or larger, linear in between.
            let t = min(max((f.height - faceNear) / (faceClose - faceNear), 0), 1)
            let w: CGFloat = fixedW + (widest - fixedW) * t
            var d = View(cx: f.midX, cy: f.midY - (eyeLine - 0.5) * w, w: w)
            d = clampView(d)
            let moved = abs(d.cx - target.cx) > deadCentre * target.w || abs(d.cy - target.cy) > deadCentre * target.w
            // The size dead zone must not stop it reaching the end stops (1.0x / maxZoom).
            let atStop = (d.w >= widest - 0.001 || d.w <= fixedW + 0.001) && abs(d.w - target.w) > 0.001
            let resized = abs(d.w - target.w) / target.w > deadSize || atStop
            if acquired || moved || resized { target = d }
        } else if now - lastFaceAt > holdLost {
            faceSeen = false
            target = View(cx: 0.5, cy: 0.5, w: widest)   // nobody there: back to the full wide view
        }
    }

    // Critically damped spring towards the target; zoom at half the speed.
    func step(dt: Double) -> View {
        lock.lock(); defer { lock.unlock() }
        let wide = !faceSeen
        // Pan: slower and slightly over-damped so it eases into place gently (Chris, 15:08).
        let omega: CGFloat = wide ? 2.8 : 2.4, omegaZ = wide ? 2.8 : 1.5
        let zeta: CGFloat = wide ? 1.0 : 1.25
        let d = CGFloat(min(dt, 0.1))
        func spring(_ x: inout CGFloat, _ v: inout CGFloat, _ t: CGFloat, _ om: CGFloat) {
            let a = om * om * (t - x) - 2 * zeta * om * v
            v += a * d; x += v * d
        }
        spring(&cur.cx, &vel.x, target.cx, omega)
        spring(&cur.cy, &vel.y, target.cy, omega)
        spring(&cur.w, &vel.w, target.w, omegaZ)
        cur = clampView(cur)
        return cur
    }

    func captureOutput(_ o: AVCaptureOutput, didOutput sb: CMSampleBuffer, from c: AVCaptureConnection) {
        guard let pb = CMSampleBufferGetImageBuffer(sb) else { return }
        if srcDims == "?" { srcDims = "\(CVPixelBufferGetWidth(pb))x\(CVPixelBufferGetHeight(pb))" }
        let now = CACurrentMediaTime()
        let dt = lastT == 0 ? 1.0 / 30 : now - lastT; lastT = now
        frameCount += 1; fpsFrames += 1
        if now - fpsT0 >= 1 { fps = Double(fpsFrames) / (now - fpsT0); fpsFrames = 0; fpsT0 = now }

        // Face detection every 3rd frame (~10 Hz), one in flight, on its own queue.
        if frameCount % 3 == 0 && !visionBusy {
            visionBusy = true
            let buf = pb
            visionQ.async { [weak self] in
                let req = VNDetectFaceRectanglesRequest()
                try? VNImageRequestHandler(cvPixelBuffer: buf, options: [:]).perform([req])
                // Union of all faces; Vision's origin is bottom-left.
                let boxes = (req.results ?? []).map { CGRect(x: $0.boundingBox.minX, y: 1 - $0.boundingBox.maxY,
                                                            width: $0.boundingBox.width, height: $0.boundingBox.height) }
                let face = boxes.isEmpty ? nil : boxes.dropFirst().reduce(boxes[0]) { $0.union($1) }
                self?.consider(face: face)
                self?.visionBusy = false
            }
        }

        let v = step(dt: dt)
        // Crop in source pixels, even edges, via the clean aperture.
        let w = (v.w * srcW / 2).rounded() * 2, h = (v.w * srcH / 2).rounded() * 2
        let x = min(max((v.cx * srcW - w / 2) / 2, 0).rounded() * 2, srcW - w)
        let y = min(max((v.cy * srcH - h / 2) / 2, 0).rounded() * 2, srcH - h)
        let ap = [kCVImageBufferCleanApertureWidthKey: w, kCVImageBufferCleanApertureHeightKey: h,
                  kCVImageBufferCleanApertureHorizontalOffsetKey: x + w / 2 - srcW / 2,
                  kCVImageBufferCleanApertureVerticalOffsetKey: y + h / 2 - srcH / 2] as CFDictionary
        CVBufferSetAttachment(pb, kCVImageBufferCleanApertureKey, ap, .shouldNotPropagate)
        var outPB: CVPixelBuffer?
        CVPixelBufferPoolCreatePixelBuffer(nil, pool!, &outPB)
        guard let outPB, VTPixelTransferSessionTransferImage(session!, from: pb, to: outPB) == noErr else { return }
        CVBufferRemoveAttachment(pb, kCVImageBufferCleanApertureKey)

        var fmt: CMVideoFormatDescription?
        CMVideoFormatDescriptionCreateForImageBuffer(allocator: nil, imageBuffer: outPB, formatDescriptionOut: &fmt)
        var timing = CMSampleTimingInfo(duration: .invalid, presentationTimeStamp: CMClockGetTime(CMClockGetHostTimeClock()), decodeTimeStamp: .invalid)
        var out: CMSampleBuffer?
        CMSampleBufferCreateReadyWithImageBuffer(allocator: nil, imageBuffer: outPB, formatDescription: fmt!, sampleTiming: &timing, sampleBufferOut: &out)
        if let out, let arr = CMSampleBufferGetSampleAttachmentsArray(out, createIfNecessary: true) as? [NSMutableDictionary] {
            arr.first?[kCMSampleAttachmentKey_DisplayImmediately] = true
            DispatchQueue.main.async { [weak self] in self?.layer?.enqueue(out) }
        }
    }

    var status: String {
        lock.lock(); defer { lock.unlock() }
        return String(format: "Follow test — %@ — zoom %.2f× — face %@ — %.0f fps", followOn ? "follow ON" : "follow OFF",
                      1 / cur.w, faceSeen ? "✓" : "–", fps)
    }
}

// --- camera
// Plain globals (not `guard let`), so the app delegate below can use them.
let maybeDev = AVCaptureDevice.DiscoverySession(deviceTypes: [.external], mediaType: .video, position: .unspecified)
    .devices.first { $0.localizedName.contains("Opal") }
if maybeDev == nil { print("No Opal C1 webcam found"); exit(1) }
let dev: AVCaptureDevice = maybeDev!
let maybeFmt = dev.formats.first { let d = CMVideoFormatDescriptionGetDimensions($0.formatDescription); return d.width == 2560 && d.height == 1440 }
if maybeFmt == nil { print("This C1 offers no 2560x1440 mode"); exit(1) }
let fmt1440: AVCaptureDevice.Format = maybeFmt!
let follower = Follower()
let session = AVCaptureSession()
session.beginConfiguration()
session.addInput(try! AVCaptureDeviceInput(device: dev))
let output = AVCaptureVideoDataOutput(); output.alwaysDiscardsLateVideoFrames = true
output.videoSettings = [kCVPixelBufferPixelFormatTypeKey as String: kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange]
output.setSampleBufferDelegate(follower, queue: DispatchQueue(label: "capture"))
session.addOutput(output)
session.commitConfiguration()

// --- window: created in applicationDidFinishLaunching (a bare CLI's early windows may never show)
final class AppDelegate: NSObject, NSApplicationDelegate {
    var win: NSWindow!
    func applicationDidFinishLaunching(_ n: Notification) {
        win = NSWindow(contentRect: NSRect(x: 200, y: 200, width: 960, height: 540),
                       styleMask: [.titled, .closable, .resizable, .miniaturizable], backing: .buffered, defer: false)
        win.isReleasedWhenClosed = false
        win.contentAspectRatio = NSSize(width: 16, height: 9)
        let view = NSView(frame: NSRect(x: 0, y: 0, width: 960, height: 540))
        view.wantsLayer = true
        let display = AVSampleBufferDisplayLayer()
        display.videoGravity = .resizeAspect
        display.frame = view.bounds
        display.autoresizingMask = [.layerWidthSizable, .layerHeightSizable]
        view.layer!.backgroundColor = NSColor.black.cgColor
        view.layer!.addSublayer(display)
        win.contentView = view
        follower.layer = display
        win.title = "Follow test — starting…"
        win.center()
        win.makeKeyAndOrderFront(nil)
        NSApp.activate()

        NSEvent.addLocalMonitorForEvents(matching: .keyDown) { e in
            switch e.charactersIgnoringModifiers?.lowercased() {
            case "f": follower.lock.lock(); follower.followOn.toggle(); follower.lock.unlock()
            case "q": session.stopRunning(); NSApp.terminate(nil)
            default: break
            }
            return nil
        }
        NotificationCenter.default.addObserver(forName: NSWindow.willCloseNotification, object: win, queue: .main) { _ in
            session.stopRunning(); NSApp.terminate(nil)
        }
        Timer.scheduledTimer(withTimeInterval: 0.5, repeats: true) { [weak self] _ in self?.win.title = follower.status }

        DispatchQueue.global().async {
            session.startRunning()
            // macOS applies the session preset on start; set the 1440p format once running.
            try? dev.lockForConfiguration(); dev.activeFormat = fmt1440; dev.unlockForConfiguration()
        }
    }
}
let app = NSApplication.shared
app.setActivationPolicy(.regular)
let delegate = AppDelegate()
app.delegate = delegate
app.run()

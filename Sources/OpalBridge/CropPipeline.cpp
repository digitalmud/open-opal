// Crop pipelines for follow mode (T-002 bench).
//
// Follow mode needs a movable window out of the 4K sensor frame, delivered as
// 1080p over USB so the zoom costs no sharpness and no extra bandwidth. There
// are several ways to get a window on the Myriad X, and they differ a lot in
// cost, so the bench measures four of them side by side:
//
//   MANIP_4K     ISP at full 4K -> ImageManip crop + resize to 1920x1080.
//                Continuous 1.0-2.0x zoom. ImageManip runs on the general-purpose
//                cores; prior art on this camera saw it slow at 4K.
//   MANIP_1440   ISP scaled to 2560x1440 first -> ImageManip -> 1920x1080.
//                Smaller input, zoom only 1.0-1.33x (anything tighter would
//                upscale).
//   WINDOW_1080  ISP at 4K, ColorCamera `video` = a 1920x1080 window the ISP
//                cuts out and moves at runtime (ColorCamera.inputConfig).
//                Nearly free; fixed 2x, pan only.
//   WINDOW_1440  Same, with a 2560x1440 window; the host would zoom within it
//                (1.5-2.0x) by downscaling, never upscaling.
//
// Every runtime input is non-blocking with a queue of one: a camera that falls
// behind drops stale windows instead of back-pressuring the whole pipeline
// (a blocked input stalls every branch on this hardware).

#include "CropPipeline.hpp"

#include <algorithm>
#include <cmath>

namespace opal {

namespace {

constexpr int kOutW = 1920, kOutH = 1080;

/// The narrowest and widest window each mode can deliver, as a fraction of the
/// full frame's width (== height fraction, since everything is 16:9).
void zoomRange(OpalCropMode mode, float& minW, float& maxW) {
    switch(mode) {
        case OPAL_CROP_MANIP_4K:   minW = 0.5f;         maxW = 1.f;          break;  // 1920/3840 .. full
        case OPAL_CROP_MANIP_1440: minW = 0.75f;        maxW = 1.f;          break;  // 1920/2560 .. full
        case OPAL_CROP_WINDOW_1080: minW = maxW = 0.5f;                      break;  // 1920/3840, fixed
        case OPAL_CROP_WINDOW_1440: minW = maxW = 2560.f / 3840.f;           break;  // fixed on device
        default:                   minW = maxW = 1.f;                        break;
    }
}

bool isManip(OpalCropMode mode) {
    return mode == OPAL_CROP_MANIP_4K || mode == OPAL_CROP_MANIP_1440;
}

}  // namespace

const char* cropModeName(OpalCropMode mode) {
    switch(mode) {
        case OPAL_CROP_NONE:        return "NONE";
        case OPAL_CROP_MANIP_4K:    return "MANIP_4K";
        case OPAL_CROP_MANIP_1440:  return "MANIP_1440";
        case OPAL_CROP_WINDOW_1080: return "WINDOW_1080";
        case OPAL_CROP_WINDOW_1440: return "WINDOW_1440";
        default:                    return "UNKNOWN";
    }
}

CropRect applyCropConfig(OpalCropMode mode, CropRect want, dai::ImageManipConfig& cfg,
                         CropRect fallback) {
    // std::clamp passes NaN straight through; a bad request keeps the old window.
    if(!std::isfinite(want.x) || !std::isfinite(want.y) || !std::isfinite(want.w) || !std::isfinite(want.h))
        want = fallback;
    float minW, maxW;
    zoomRange(mode, minW, maxW);

    // Keep the window's centre, fix its size to the mode's range, then slide it
    // back inside the frame.
    const float cx = want.x + want.w / 2.f, cy = want.y + want.h / 2.f;
    const float w = std::clamp(want.w, minW, maxW);
    CropRect applied;
    applied.w = applied.h = w;
    applied.x = std::clamp(cx - w / 2.f, 0.f, 1.f - w);
    applied.y = std::clamp(cy - w / 2.f, 0.f, 1.f - w);

    if(isManip(mode)) {
        cfg.setCropRect(applied.x, applied.y, applied.x + applied.w, applied.y + applied.h);
        cfg.setResize(kOutW, kOutH);
        cfg.setFrameType(dai::ImgFrame::Type::NV12);
    } else {
        // ColorCamera.inputConfig moves a fixed-size `video` window: only the
        // top-left corner counts (depthai example rgb_camera_control.cpp). The
        // corner is a fraction of the ISP width, which is the full frame here.
        cfg.setCropRect(applied.x, applied.y, 0, 0);
    }
    return applied;
}

CropRect buildCropBranch(dai::Pipeline& pipeline,
                         const std::shared_ptr<dai::node::ColorCamera>& cam,
                         const std::shared_ptr<dai::node::XLinkOut>& xout,
                         OpalCropMode mode) {
    auto cropIn = pipeline.create<dai::node::XLinkIn>();
    cropIn->setStreamName("cropcfg");

    CropRect start;

    if(isManip(mode)) {
        cam->setIspScale(mode == OPAL_CROP_MANIP_1440 ? 2 : 1, mode == OPAL_CROP_MANIP_1440 ? 3 : 1);

        // Start at full frame (1.0x).
        auto manip = pipeline.create<dai::node::ImageManip>();
        start = applyCropConfig(mode, CropRect{}, manip->initialConfig);
        manip->setMaxOutputFrameSize(kOutW * kOutH * 3 / 2);
        manip->setNumFramesPool(4);
        manip->inputImage.setBlocking(false);
        manip->inputImage.setQueueSize(1);
        manip->inputConfig.setBlocking(false);
        manip->inputConfig.setQueueSize(1);

        cam->isp.link(manip->inputImage);
        manip->out.link(xout->input);
        cropIn->out.link(manip->inputConfig);
    } else {
        cam->setIspScale(1, 1);
        const int winW = mode == OPAL_CROP_WINDOW_1440 ? 2560 : kOutW;
        const int winH = mode == OPAL_CROP_WINDOW_1440 ? 1440 : kOutH;
        cam->setVideoSize(winW, winH);

        // `video` starts as the centre crop of the ISP frame; record that.
        float minW, maxW;
        zoomRange(mode, minW, maxW);
        start = CropRect{(1.f - minW) / 2.f, (1.f - minW) / 2.f, minW, minW};

        cam->inputConfig.setBlocking(false);
        cam->inputConfig.setQueueSize(1);
        cam->video.link(xout->input);
        cropIn->out.link(cam->inputConfig);
    }
    return start;
}

}  // namespace opal

#pragma once

#include "OpalBridge.h"

#include <depthai/depthai.hpp>

#include <memory>

namespace opal {

/// A crop window in normalised full-frame coordinates (0..1, top-left origin,
/// after the ISP's orientation fix). Width and height are equal fractions
/// because the frame and every window are 16:9.
struct CropRect {
    float x = 0.f, y = 0.f, w = 1.f, h = 1.f;
};

/// Wires the pipeline variant for `mode` (anything but OPAL_CROP_NONE) between
/// `cam` and `xout`, and adds an XLinkIn named "cropcfg" that moves the window
/// at runtime. Overrides the ISP scale the caller may have set. Returns the
/// window the pipeline starts with.
CropRect buildCropBranch(dai::Pipeline& pipeline,
                         const std::shared_ptr<dai::node::ColorCamera>& cam,
                         const std::shared_ptr<dai::node::XLinkOut>& xout,
                         OpalCropMode mode);

/// Clamps `want` to what `mode` can do (zoom range, 16:9, inside the frame,
/// never an upscale), writes the request into `cfg` (filled in place:
/// ImageManipConfig can't be copy-assigned) and returns the window applied.
/// A request with any non-finite value (NaN, inf) is replaced by `fallback`.
CropRect applyCropConfig(OpalCropMode mode, CropRect want, dai::ImageManipConfig& cfg,
                         CropRect fallback = CropRect{});

/// Human-readable name for logs and the bench: "NONE", "MANIP_4K", ...
const char* cropModeName(OpalCropMode mode);

}  // namespace opal

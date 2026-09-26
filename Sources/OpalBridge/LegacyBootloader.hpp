#pragma once

#include <depthai/xlink/XLinkConnection.hpp>

#include <exception>
#include <functional>
#include <optional>
#include <string>

namespace opal {

/// True if `e` is depthai refusing a C1 whose DepthAI bootloader reports 0.0.0
/// ("Bootloader version 0.0.2 required to send request 'UsbRomBoot'. Current
/// version 0.0.0"). Matched on depthai v2.30.0's message text.
bool isLegacyBootloaderRefusal(const std::exception& e);

/// Call only after isLegacyBootloaderRefusal. depthai's failed attempt resets
/// the camera, which then passes back through its ~5 s bootloader window: waits
/// for that, confirms the bootloader still reports exactly 0.0.0, and asks it to
/// restart into the chip's USB ROM boot mode. Returns the device to boot from
/// (state X_LINK_UNBOOTED) once the link has dropped or erred after the request
/// went out. Returns std::nullopt if the camera wasn't caught, didn't report
/// exactly 0.0.0 (then nothing beyond the version query was sent), or answered
/// the request without restarting (it was sent; the device is reset on close).
/// Only GetBootloaderVersion and UsbRomBoot are ever sent. See LegacyBootloader.cpp.
std::optional<dai::DeviceInfo> kickLegacyBootloaderToRom(const std::string& mxid,
                                                         const std::function<void(const std::string&)>& log);

}  // namespace opal

// Booting a C1 whose DepthAI bootloader reports version 0.0.0.
//
// Seen 2026-09 on a C1 that enumerates as 03e7:f63b while a webcam (upstream's
// camera: f63d), sits in BOOTLOADER for ~5 s after power-on, and reports
// bootloader version 0.0.0 (upstream's: 0.0.15). depthai boots bootloaders
// >= 0.0.12 directly; for older ones it asks them to jump to the chip's USB ROM
// boot mode (request UsbRomBoot) -- but it refuses even that below 0.0.2, so
// opening fails with "Bootloader version 0.0.2 required to send request
// 'UsbRomBoot'. Current version 0.0.0".
//
// That bootloader does speak the standard protocol: it answers
// GetBootloaderVersion with a well-formed BootloaderVersion reply. So, ONLY
// after depthai has refused it with exactly that error, we send the read-only
// version request ourselves and, only if the answer is exactly 0.0.0, the bare
// UsbRomBoot request (command 0, no payload). The chip restarts into ROM USB
// boot (state UNBOOTED) and depthai's normal path loads its firmware into RAM.
// Nothing else is ever sent: no flash, config or memory request. A replug
// restores the stock firmware.
//
// Details that matter:
//  - Cameras with any other bootloader never reach this file: depthai's own
//    attempt runs first, unchanged, and only its 0.0.0 refusal leads here.
//  - The connection keeps depthai's default reset-on-close until the moment we
//    commit to the kick. XLinkConnection's reset IS its teardown, so turning it
//    off earlier would leave a live link behind on every early return.
//  - Reads and writes are untimed, exactly as depthai's own DeviceBootloader
//    does: XLinkWriteDataWithTimeout fails outright (X_LINK_ERROR) on this
//    camera, and depthai never uses the timed calls itself.
//  - We skip depthai's bootloader watchdog pings; the exchange is far shorter
//    than its 1.5 s timeout.

#include "LegacyBootloader.hpp"

#include <depthai-bootloader-shared/Bootloader.hpp>
#include <depthai-bootloader-shared/XLinkConstants.hpp>
#include <depthai/xlink/XLinkStream.hpp>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <memory>
#include <thread>

namespace opal {

bool isLegacyBootloaderRefusal(const std::exception& e) {
    const std::string what = e.what();
    return what.find("required to send request 'UsbRomBoot'") != std::string::npos
        && what.find("Current version 0.0.0") != std::string::npos;
}

std::optional<dai::DeviceInfo> kickLegacyBootloaderToRom(const std::string& mxid,
                                                         const std::function<void(const std::string&)>& log) {
    namespace bl = dai::bootloader;
    using clock = std::chrono::steady_clock;

    // depthai's failed attempt has already reset the camera by the time its error
    // reaches us (measured: off the bus at 0.27 s, back in its bootloader at
    // 0.69 s, error at 1.52 s), and the window lasts only ~5 s. So connect as soon
    // as it's in BOOTLOADER; waiting (<= 10 s) only covers a slower reset.
    log("depthai refused bootloader 0.0.0 · catching the camera's bootloader window");
    try {
        std::optional<dai::DeviceInfo> target;
        const auto until = clock::now() + std::chrono::seconds(10);
        while(clock::now() < until && !target) {
            for(const auto& d : dai::XLinkConnection::getAllConnectedDevices(X_LINK_BOOTLOADER)) {
                if(d.getMxId() == mxid) target = d;
            }
            if(!target) std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        if(!target) {
            log("camera didn't come back in its bootloader window");
            return std::nullopt;
        }

        auto conn = std::make_shared<dai::XLinkConnection>(*target, X_LINK_BOOTLOADER);
        dai::XLinkStream stream(conn, bl::XLINK_CHANNEL_BOOTLOADER, bl::XLINK_STREAM_MAX_SIZE);

        bl::request::GetBootloaderVersion ask{};
        stream.write(&ask, sizeof(ask));
        auto reply = stream.read();

        // Same checks, in the same order, as depthai's DeviceBootloader::parseResponse.
        bl::response::BootloaderVersion ver{};
        bl::response::Command cmd{};
        if(reply.size() < sizeof(cmd)) {
            log("bootloader version reply empty · giving up");
            return std::nullopt;
        }
        std::memcpy(&cmd, reply.data(), sizeof(cmd));
        if(cmd != ver.cmd || reply.size() < sizeof(ver)) {
            log("bootloader version reply not understood · giving up");
            return std::nullopt;
        }
        std::memcpy(&ver, reply.data(), sizeof(ver));

        char line[96];
        std::snprintf(line, sizeof(line), "bootloader reports version %u.%u.%u", ver.major, ver.minor, ver.patch);
        log(line);
        if(ver.major != 0 || ver.minor != 0 || ver.patch != 0) return std::nullopt;

        log("asking bootloader 0.0.0 to restart into USB ROM boot (command 0; no flash or config write)");
        // From here the link is expected to drop; a reset on close would only
        // stall on the dead link.
        conn->setRebootOnDestruction(false);
        dai::DeviceInfo unbooted = *target;
        unbooted.state = X_LINK_UNBOOTED;
        try {
            bl::request::UsbRomBoot rom{};
            stream.write(&rom, sizeof(rom));
            stream.read();
        } catch(...) {
            // Link dropped (on the write or while waiting): the chip is restarting
            // into USB ROM boot, as depthai's bootUsbRomBootloader() expects. An
            // error after the command left counts the same -- it may already have
            // acted on it, so the device must be treated as restarting.
            return unbooted;
        }
        // The bootloader answered instead of restarting: not what we expect.
        conn->setRebootOnDestruction(true);
        log("bootloader answered the USB ROM boot request without restarting · giving up");
        return std::nullopt;
    } catch(const std::exception& e) {
        log(std::string("bootloader kick failed (") + e.what() + ")");
        return std::nullopt;
    }
}

}  // namespace opal

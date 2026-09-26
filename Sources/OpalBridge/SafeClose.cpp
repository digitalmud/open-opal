// Closing a depthai device safely when the camera link may be dead (T-011).
//
// Why this exists: DeviceBase::close() sets `closed` only after closeImpl()
// succeeds (DeviceBase.cpp, depthai v2.30). If closeImpl() throws on a dead link,
// the destructor's own close() runs it again, and a throw from a destructor calls
// std::terminate -- the app aborts ("terminating due to uncaught exception of type
// dai::XLinkReadError"). If closeImpl()'s first RPC lands while XLink is resetting
// the link, it can instead wait on a semaphore forever. So: close explicitly, off
// the caller's thread, and decide the object's fate from how that went.
//
// Measured on camera 3 (2026-09-26): a normal close takes 1.34-1.40 s; the bridge
// passes an 8 s bound. The bridge also disables depthai's crash-dump hunt on close
// (DEPTHAI_CRASHDUMP_TIMEOUT=0, OpalBridge.cpp), which otherwise keeps a dead-link
// close busy for ~9 s and reconnects to the camera behind our back.

#include "SafeClose.hpp"

#include <future>
#include <mutex>
#include <system_error>
#include <thread>

namespace opal {

bool closeDeviceSafely(std::unique_ptr<dai::Device> device,
                       std::chrono::milliseconds bound,
                       const std::function<void(const std::string&)>& log) {
    if(!device) return true;

    // From here the helper thread owns the device's fate.
    dai::Device* raw = device.release();
    auto result = std::make_shared<std::promise<bool>>();
    auto done = result->get_future();
    // Set once the caller stops waiting. An abandoned helper must not log: by the
    // time it finishes the app may be exiting (the logger's mutex and the Swift
    // context behind it may already be gone).
    struct Fate { std::mutex m; bool abandoned = false; };
    auto fate = std::make_shared<Fate>();   // one lock around "check, then log" and "abandon"

    try {
        std::thread([raw, result, fate, log]() {
            bool ok = false;
            std::string why;
            try {
                raw->close();
                ok = true;
            } catch(const std::exception& e) {
                why = e.what();
            } catch(...) {
                why = "unknown error";
            }
            // Only a cleanly closed device may be destroyed: its destructor's close()
            // is then a no-op. After a failed close, destroying it would abort.
            if(ok) delete raw;
            if(!ok) {
                std::lock_guard<std::mutex> lk(fate->m);
                if(!fate->abandoned) log("device close failed (" + why + ") · abandoning it so the app keeps running");
            }
            result->set_value(ok);
        }).detach();
    } catch(const std::system_error& e) {
        // No thread (resources exhausted after many link drops): abandon the
        // device rather than let the exception escape through the C API.
        log(std::string("couldn't start device close (") + e.what() + ") · abandoning it");
        return false;
    }

    if(done.wait_for(bound) == std::future_status::ready) return done.get();
    { std::lock_guard<std::mutex> lk(fate->m); fate->abandoned = true; }
    log("device close still stuck after " + std::to_string(bound.count() / 1000) +
        " s · abandoning it so the app keeps running");
    return false;
}

}  // namespace opal

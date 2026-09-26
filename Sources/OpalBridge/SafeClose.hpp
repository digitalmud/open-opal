#pragma once

#include <depthai/depthai.hpp>

#include <chrono>
#include <functional>
#include <memory>
#include <string>

namespace opal {

/// Closes a depthai device without ever hanging or aborting the app (T-011).
///
/// When the USB link to the camera has dropped, destroying a dai::Device can
/// either block forever (its close sends an RPC over the dead link while XLink is
/// tearing the link down) or throw out of the destructor, which aborts the
/// process. Both were observed on the C1 in 2026-09.
///
/// Takes ownership and closes on a helper thread. Returns true if it closed
/// cleanly within `bound`. On a throw, the object is intentionally never
/// destroyed (its destructor would re-run the failed close and abort); on a
/// timeout, the helper thread and object are left behind. Either way the caller
/// can carry on and reconnect. A rare, bounded leak beats a dead app.
bool closeDeviceSafely(std::unique_ptr<dai::Device> device,
                       std::chrono::milliseconds bound,
                       const std::function<void(const std::string&)>& log);

}  // namespace opal

#pragma once
#include "openxr_runtime.h"
#include <mutex>

namespace bvr::vr {
// The writer builds locally while the last complete sample remains readable.
// Publish atomically AFTER head + both eyes are located and the pose-tag ring
// is populated. Invalid tracking/session teardown explicitly clears validity.
class StereoPoseMailbox {
    mutable std::mutex mutex;
    StereoPose pose{};
    bool valid{};
public:
    void publish(const StereoPose& next, bool tracked) {
        std::lock_guard<std::mutex> lock(mutex);
        pose=next; valid=tracked;
    }
    bool read(StereoPose& out) const {
        std::lock_guard<std::mutex> lock(mutex);
        if(!valid) return false;
        out=pose; return true;
    }
    void invalidate() {
        std::lock_guard<std::mutex> lock(mutex);
        valid=false;
    }
};
}

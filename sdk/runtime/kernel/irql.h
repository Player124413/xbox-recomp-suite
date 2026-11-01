#pragma once
#include <atomic>
#include <cstdint>
#include <mutex>

namespace kernel {

// Xbox titles also use DISPATCH_LEVEL as a lock: on its single CPU another
// thread cannot enter one of these regions until the current thread lowers IRQL.
// Host threads run concurrently, so updating KPCR.Irql alone is insufficient.
class IrqlGate {
public:
    void Set(uint8_t& current, uint8_t next) {
        const bool entering = current < 2 && next >= 2;
        const bool leaving = current >= 2 && next < 2;
        if (entering) {
            dispatch_.lock();
            held_.store(true, std::memory_order_release);
        }
        current = next;
        if (leaving) {
            dispatch_.unlock();
            held_.store(false, std::memory_order_release);
        }
    }

    // true while a guest thread is inside a DISPATCH_LEVEL region
    bool Held() const { return held_.load(std::memory_order_acquire); }

private:
    std::mutex dispatch_;
    std::atomic<bool> held_{false};
};

}  // namespace kernel

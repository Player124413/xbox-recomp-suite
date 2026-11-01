// Frame pacing telemetry and the Android dynamic-resolution governor.
//
// The guest presents through app::Pace(), which reports the wall time between
// presents. The governor reads that and moves the internal render scale inside
// the user's [resolution_min, resolution_max] window until the frame time sits
// near the target. The Vulkan backend's ApplySettings() already rebuilds its
// render targets when render_percent changes, so no renderer changes are
// needed here.
#include "app/app.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "core/host_pause.h"
#include "core/log.h"

namespace app {

namespace {

constexpr size_t kWindow = 60;  // frames in the worst-frame window
constexpr int kSampleMs = 500;  // governor period
constexpr int kCooldownMs = 1800;  // minimum time between resolution changes

std::mutex g_lock;
FrameStats g_stats;
double g_window[kWindow] = {};
size_t g_window_at = 0;
bool g_window_full = false;
uint64_t g_total_frames = 0;

std::atomic<bool> g_stop{false};
std::thread g_thread;
std::condition_variable g_cv;
std::mutex g_cv_lock;

double NowSeconds() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

void Govern() {
    auto& s = Get();
    if (!s.dynamic_resolution || s.target_fps == 0) return;
    const FrameStats st = GetFrameStats();
    if (st.frames < 30) return;  // let the first second of shader compilation settle
    // A paused game stops presenting; do not learn from a stale sample.
    if (st.last_report_s && NowSeconds() - st.last_report_s > 1.0) return;
    const double target = double(s.target_fps);
    const double fps = st.fps;
    if (fps <= 0.0) return;
    uint32_t percent = s.render_percent;
    const uint32_t floor = std::min(std::max(s.resolution_min, 25u), 100u);
    const uint32_t ceiling = std::min(std::max(s.resolution_max, floor), 100u);
    if (percent < floor) percent = floor;
    if (percent > ceiling) percent = ceiling;
    uint32_t next = percent;
    if (fps < target * 0.92) {
        // A little more room when the frame is far over budget; a small step
        // when it is close, so the picture does not visibly pulse.
        next = percent - (fps < target * 0.80 ? 10u : 5u);
    } else if (fps > target * 1.06 && percent < ceiling) {
        next = percent + 5u;
    }
    next = std::clamp(next, floor, ceiling);
    if (next == percent) return;
    s.render_percent = next;
    Save();
    LOGR("performance: %.1f FPS at %u%% -> %u%% (target %u, window %u..%u%%)",
         fps, percent, next, s.target_fps, floor, ceiling);
}

void Run() {
    using clock = std::chrono::steady_clock;
    auto next = clock::now() + std::chrono::milliseconds(kSampleMs);
    auto last_change = clock::now();
    while (!g_stop.load(std::memory_order_relaxed)) {
        std::unique_lock lock(g_cv_lock);
        if (g_cv.wait_until(lock, next, [] { return g_stop.load(std::memory_order_relaxed); })) break;
        if (host::Pause().Paused()) {
            next = clock::now() + std::chrono::milliseconds(kSampleMs);
            continue;
        }
        if (clock::now() - last_change < std::chrono::milliseconds(kCooldownMs)) {
            next = clock::now() + std::chrono::milliseconds(kSampleMs);
            continue;
        }
        Govern();
        last_change = clock::now();
        next = clock::now() + std::chrono::milliseconds(kSampleMs);
    }
}

}  // namespace

void ReportFrame(double frame_ms) {
    if (!(frame_ms > 0.0) || !std::isfinite(frame_ms)) return;
    std::lock_guard lock(g_lock);
    g_window[g_window_at] = frame_ms;
    g_window_at = (g_window_at + 1) % kWindow;
    g_window_full = g_window_full || g_window_at == 0;
    g_total_frames++;
    const double instant = 1000.0 / frame_ms;
    g_stats.fps = g_stats.fps ? g_stats.fps * 0.9 + instant * 0.1 : instant;
    g_stats.frame_ms = g_stats.frame_ms ? g_stats.frame_ms * 0.9 + frame_ms * 0.1 : frame_ms;
    g_stats.frames = g_total_frames;
    g_stats.last_report_s = NowSeconds();
}

FrameStats GetFrameStats() {
    std::lock_guard lock(g_lock);
    FrameStats s = g_stats;
    const size_t n = g_window_full ? kWindow : g_window_at;
    double worst = 0.0;
    for (size_t i = 0; i < n; i++) worst = std::max(worst, g_window[i]);
    s.worst_ms = worst;
    return s;
}

void PerformanceInit() {
    if (!Get().dynamic_resolution) return;
    if (g_thread.joinable()) return;
    g_stop.store(false, std::memory_order_relaxed);
    g_thread = std::thread(Run);
    LOGR("performance: governor on, target %u FPS, window %u..%u%%, power mode %u",
         Get().target_fps, Get().resolution_min, Get().resolution_max, Get().power_mode);
}

void PerformanceShutdown() {
    g_stop.store(true, std::memory_order_relaxed);
    g_cv.notify_all();
    if (g_thread.joinable()) g_thread.join();
}

}  // namespace app

#include "game/performance.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "app/app.h"
#include "core/log.h"

#ifdef __ANDROID__
#include <sched.h>
#include <sys/resource.h>
#include <unistd.h>
#endif

namespace sonic {

namespace {

#ifdef __ANDROID__

constexpr int kMaxCpu = 16;

struct CpuInfo {
    int id;
    long max_khz;
    long min_khz;
};

std::vector<CpuInfo> ReadCpus() {
    std::vector<CpuInfo> cpus;
    for (int id = 0; id < kMaxCpu; id++) {
        std::string base = "/sys/devices/system/cpu/cpu" + std::to_string(id) + "/cpufreq/";
        std::ifstream max(base + "cpuinfo_max_freq");
        std::ifstream min(base + "cpuinfo_min_freq");
        long max_khz = 0, min_khz = 0;
        max >> max_khz;
        min >> min_khz;
        if (max_khz > 0) cpus.push_back({id, max_khz, min_khz});
    }
    return cpus;
}

void ApplyAffinity(uint32_t mode) {
    std::vector<CpuInfo> cpus = ReadCpus();
    if (cpus.empty()) {
        LOGI("performance: no cpufreq sysfs; leaving the scheduler to Android");
        return;
    }
    std::sort(cpus.begin(), cpus.end(), [](const CpuInfo& a, const CpuInfo& b) {
        return a.max_khz > b.max_khz;
    });
    cpu_set_t set;
    CPU_ZERO(&set);
    if (mode == 1) {
        // performance: the fastest cores, which on a big.LITTLE phone are the
        // Cortex-X/A78 cluster. Four is a common big-cluster size; take at
        // least two so the render thread is not alone on one core.
        const size_t count = std::min<size_t>(cpus.size(), std::min<size_t>(4, std::max<size_t>(2, cpus.size() / 2)));
        for (size_t i = 0; i < count; i++) CPU_SET(cpus[i].id, &set);
    } else if (mode == 2) {
        // powersave: the slowest cores, for a long session on a small battery
        const size_t count = std::min<size_t>(cpus.size(), std::min<size_t>(4, std::max<size_t>(2, cpus.size() / 2)));
        for (size_t i = 0; i < count; i++) CPU_SET(cpus[cpus.size() - 1 - i].id, &set);
    } else {
        for (auto& cpu : cpus) CPU_SET(cpu.id, &set);
    }
    if (sched_setaffinity(0, sizeof(set), &set) == 0) {
        std::string ids;
        for (int id = 0; id < CPU_SETSIZE; id++)
            if (CPU_ISSET(id, &set)) ids += (ids.empty() ? "" : ",") + std::to_string(id);
        LOGI("performance: render thread pinned to CPU %s (power mode %u)", ids.c_str(), mode);
    } else {
        LOGI("performance: sched_setaffinity failed: %s", std::strerror(errno));
    }
    // Android's app UID may raise its own nice value; try, but never fail.
    if (setpriority(PRIO_PROCESS, 0, mode == 2 ? 5 : -5) == 0)
        LOGI("performance: render thread nice %d", mode == 2 ? 5 : -5);
}

#endif  // __ANDROID__

}  // namespace

void InitPerformance() {
    auto& s = app::Get();
#ifdef __ANDROID__
    // A phone's panel is 60/120 Hz and its GPU is the bottleneck. Cap at 60
    // for a stable frame time, present immediately (no FIFO latency), and
    // render at 75% of the panel height. Dynamic resolution then trims the
    // scale only when a scene cannot hold the target.
    // The APK ships a settings.toml with the profile. If it is present, the
    // player's choices win; only a build without one gets the defaults.
    const bool packaged = s.render_percent_set || s.power_mode_set ||
                          s.dynamic_resolution_set || s.target_fps_set;
    if (!packaged) {
        if (s.fps_cap == 0) s.fps_cap = 60;
        s.vsync = false;
        s.render_percent = 75;
        s.dynamic_resolution = true;
        s.target_fps = 60;
        s.resolution_min = 50;
        s.resolution_max = 85;
        s.precise_pacing = true;
        s.inline_dpc = true;
        s.power_mode = 1;
    }
    LOGR("performance: Sonic Heroes profile: 60 FPS cap, %u%% render, dynamic %u..%u%%, power %u",
         s.render_percent, s.resolution_min, s.resolution_max, s.power_mode);
#else
    LOGR("performance: desktop profile: %u%% render, %u FPS cap", s.render_percent, s.fps_cap);
#endif
}

void RenderThread() {
#ifdef __ANDROID__
    ApplyAffinity(app::Get().power_mode);
#endif
}

void PresentThread() {
    static int frame = 0;
    if (++frame % 120 != 0) return;
    const app::FrameStats st = app::GetFrameStats();
    LOGR("performance: %.1f FPS, %.2f ms/frame, worst %.2f ms, render %u%%",
         st.fps, st.frame_ms, st.worst_ms, app::Get().render_percent);
}

}  // namespace sonic

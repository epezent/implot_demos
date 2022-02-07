#pragma once
#include <chrono>

using Clock        = std::chrono::high_resolution_clock;
using Milliseconds = std::chrono::milliseconds;
using Microseconds = std::chrono::microseconds;

template <typename T>
struct SmartTimer {
    SmartTimer(std::size_t &_val) : val(_val) {
        t1 = Clock::now();
    }
    ~SmartTimer() {
        auto t2 = Clock::now();
        val += std::chrono::duration_cast<T>(t2 - t1).count();
    }
    std::chrono::high_resolution_clock::time_point t1;
    std::size_t &val;
};

struct AppProfile {
    std::size_t glfw_poll_events;
    std::size_t imgui_new_frame;
    std::size_t app_update;
    std::size_t imgui_render;
    std::size_t gpu;
};
// Demo:   profiler.cpp
// Author: Evan Pezent (evanpezent.com)
// Date:  11/26/2021

#include "App.h"
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

void DoFakeWork(std::size_t us_min, std::size_t us_max) {
    auto t = Clock::now();
    auto d = RandomRange(us_min, us_max);
    auto tstop = t + std::chrono::microseconds(d);
    while (t < tstop) {
        // kill the CPU :(
    }
}

struct FakeProfile {
    std::size_t times[6];
};

struct FakeEngine {
    void UpdatePhysics() {

    }
    void UpdateScripts() {

    }
    FakeProfile m_profile;
};


struct ImProfiler : App {
    using App::App;
    void Update() override {
        ImGui::Begin("Profiler");
        ImGui::Text("%.3f FPS", ImGui::GetIO().Framerate);

        if (ImPlot::BeginPlot("Profiler")) {
            ImPlot::EndPlot();
        }
        ImGui::End();
    }
};

int main(int argc, char const *argv[])
{
    ImProfiler app("ImProfiler",640,480,argc,argv);
    app.Run();
    return 0;
}

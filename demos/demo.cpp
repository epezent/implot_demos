// Demo:   demo.cpp
// Author: Evan Pezent (evanpezent.com)
// Date:   3/26/2021

#define IMGUI_DEFINE_MATH_OPERATORS

#include "App.h"
#include <imgui_internal.h>

template <typename T> int sgn(T val) {
    return (T(0) < val) - (val < T(0));
}

struct ImPlotDemo : App {
    using App::App;
    void update() override {
        // ImGui::ShowDemoWindow();
        ImGui::ShowMetricsWindow();
        ImPlot::ShowMetricsWindow();
        ImPlot::ShowDemoWindow();      



    }
};

int main(int argc, char const *argv[])
{
    ImPlotDemo demo(1920,1080,"ImPlot Demo");
    demo.run();
    return 0;
}

// Demo:   demo.cpp
// Author: Evan Pezent (evanpezent.com)
// Date:   3/26/2021

#include "App.h"

struct ImPlotDemo : App {
    using App::App;
    void update() override {
        ImGui::ShowDemoWindow();
        ImPlot::ShowDemoWindow();
    }
};

int main(int argc, char const *argv[])
{
    ImPlotDemo demo(1920,1080,"ImPlot Demo");
    demo.run();
    return 0;
}

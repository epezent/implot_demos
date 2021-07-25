// Demo:   demo.cpp
// Author: Evan Pezent (evanpezent.com)
// Date:   3/26/2021

#include "App.h"

struct ImPlotDemo : App {
    using App::App;
    void update() override {
        ImPlot::ShowDemoWindow();   
        ImGui::ShowDemoWindow(); 
    }
};

int main(int argc, char const *argv[])
{
    ImPlotDemo app(1920,1080,"ImPlot Demo");
    app.run();
    return 0;
}

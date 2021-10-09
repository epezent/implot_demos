// Demo:   demo.cpp
// Author: Evan Pezent (evanpezent.com)
// Date:   3/26/2021

#include "App.h"

struct ImPlotDemo : App {
    using App::App;
    void Update() override {
        ImPlot::ShowDemoWindow();   
        // ImGui::ShowDemoWindow();
        ImPlot::ShowMetricsWindow();
        // ImGui::ShowMetricsWindow();  

        ImGui::Begin("Test");
        if (ImPlot::BeginPlot("MyPlot")) {

            // ImPlot::SetupAxis(ImAxis_X2);
            // ImPlot::SetupAxis(ImAxis_X3);
            // ImPlot::SetupAxis(ImAxis_Y2);
            // ImPlot::SetupAxis(ImAxis_Y3);

            ImPlot::EndPlot();
        }
        ImGui::End();
    }
};

int main(int argc, char const *argv[])
{
    ImPlotDemo app("ImPlot Demo",1920,1080,argc,argv);
    app.Run();
    return 0;
}

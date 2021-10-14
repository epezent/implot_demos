// Demo:   demo.cpp
// Author: Evan Pezent (evanpezent.com)
// Date:   3/26/2021

#include "App.h"
#include <implot_internal.h>

void MetricFormatter(double value, char* buff, int size, void* data) {
    const char* unit = (const char*)data;
    static double v[]      = {1000000000,1000000,1000,1,0.001,0.000001,0.000000001};
    static const char* p[] = {"G","M","k","","m","u","n"};
    if (value == 0) {
        snprintf(buff,size,"0 %s", unit);
        return;
    }
    for (int i = 0; i < 7; ++i) {
        if (fabs(value) >= v[i]) {
            snprintf(buff,size,"%.0f %s%s",value/v[i],p[i],unit);
            return;
        }
    }
    snprintf(buff,size,"%.0f %s%s",value/v[6],p[6],unit);
}

void ShowTest() {
    ImGui::Begin("Test Window");
    static double xs[1000], ys[1000];
    for (int i = 0; i < 1000; ++i) {
        double angle = i * 2 * 3.14 / 999.0;
        xs[i] = cos(angle); ys[i] = sin(angle);
    }
    if (ImPlot::BeginPlot("##EqualAxes",ImVec2(-1,-1),ImPlotFlags_Equal)) {
        ImPlot::SetupAxisFormat(ImAxis_Y1, MetricFormatter, "Hz");
        ImPlot::PlotLine("Circle",xs,ys,1000);
        ImPlot::EndPlot();
    }
    ImGui::End();
}



struct ImPlotDemo : App {
    using App::App;
    void Update() override {
        ImPlot::ShowDemoWindow();   
        ImPlot::ShowMetricsWindow();
        ImGui::ShowDemoWindow();
        ShowTest();
    }
};

int main(int argc, char const *argv[])
{
    ImPlotDemo app("ImPlot Demo",1920,1080,argc,argv);
    app.Run();
    return 0;
}

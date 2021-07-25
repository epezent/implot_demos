// Demo:   perlin.cpp
// Author: Evan Pezent (evanpezent.com)
// Date:   7/25/2021

#define APP_USE_DGPU
#include "App.h"

#define STB_PERLIN_IMPLEMENTATION
#include <stb/stb_perlin.h>

#include <vector>
#include <iostream>

struct ImPerlin : App {
    using App::App;

    void init() override {
        generate_noise();
        ImPlot::GetStyle().Colormap = ImPlotColormap_Spectral;
    }

    void update() override {
        ImGui::SetNextWindowPos(ImVec2(0,0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(get_window_size(), ImGuiCond_Always);
        ImGui::Begin("Perlin", nullptr, ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize);
        ImGui::Text("FPS: %.2f", ImGui::GetIO().Framerate);
        // ImPlot::ShowColormapSelector("Colormap");
        if (ImGui::DragInt2("Size",&rows,10,0,10000))        
            generate_noise();        
        if (ImGui::DragFloat("Scale",&scale,0.001f,0,10))
            generate_noise();
        if (ImGui::DragFloat("Scrub",&z,0.1f,0,10))
            generate_noise();
        static float mn = 0, mx = 1;
        ImGui::DragFloatRange2("Range",&mn,&mx,0.1f,-10,10);
        if (ImPlot::BeginPlot("##Perlin",0,0,ImVec2(-1,-1),ImPlotFlags_CanvasOnly,ImPlotAxisFlags_NoDecorations,ImPlotAxisFlags_NoDecorations)) {
            ImPlot::PlotHeatmap("##T",perlin_data.data(),rows,cols,mn,mx,NULL);
            ImPlot::EndPlot();
        }
        ImGui::End();
    }

    void generate_noise() {
        perlin_data.clear();
        perlin_data.reserve(rows*cols);
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                float v = stb_perlin_noise3_seed(r*scale, c*scale, z, 0, 0, 0, 42);
                perlin_data.push_back(v);
            }
        }
    }

    float scale = 0.005f;
    float z = 0;
    int rows = 1000;
    int cols = 1000;
    std::vector<float> perlin_data;
};

int main(int argc, char const *argv[])
{
    ImPerlin demo(640,480,"ImPerlin", false);
    demo.run();
    return 0;
}

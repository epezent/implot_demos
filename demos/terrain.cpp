// Demo:   terrain.cpp
// Author: Evan Pezent (evanpezent.com)
// Date:   3/26/2021

// #define APP_USE_DGPU
#include "App.h"

#define STB_PERLIN_IMPLEMENTATION
#include <stb/stb_perlin.h>

#include <vector>
#include <iostream>

struct TerrainDemo : App {
    using App::App;

    void init() override {

        static const ImU32 terrain_Data[6] = {
            4294934275,
            4294934275,
            4283256141,
            4283593746,
            4285583367,
            4292861919
        };
        ImPlotColormap terrain = ImPlot::AddColormap("Terrain", terrain_Data, 6, true);
        ImPlot::GetStyle().Colormap = terrain;

        generate_terrain();
    }

    void update() override {
        ImGui::SetNextWindowPos(ImVec2(0,0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(640,480), ImGuiCond_Always);
        ImGui::Begin("Terrain", nullptr, ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize);
        ImGui::Text("FPS: %.2f", ImGui::GetIO().Framerate);
        // ImPlot::ShowColormapSelector("Colormap");
        if (ImGui::DragInt2("Size",&rows,10,0,10000))        
            generate_terrain();        
        if (ImGui::DragFloat("Scale",&scale,0.001f,0,10))
            generate_terrain();
        if (ImGui::DragFloat("Scrub",&z,0.1f,0,10))
            generate_terrain();
        static float mn = 0, mx = 1;
        ImGui::DragFloatRange2("Range",&mn,&mx,0.1f,-10,10);
        if (ImPlot::BeginPlot("##Terrain",0,0,ImVec2(-1,-1),ImPlotFlags_CanvasOnly,ImPlotAxisFlags_NoDecorations,ImPlotAxisFlags_NoDecorations)) {
            ImPlot::PlotHeatmap("##T",terrain_data.data(),rows,cols,mn,mx,NULL);
            ImPlot::EndPlot();
        }
        ImGui::End();
    }

    void generate_terrain() {
        terrain_data.clear();
        terrain_data.reserve(rows*cols);
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                float v = stb_perlin_noise3_seed(r*scale, c*scale, z, 0, 0, 0, 42);
                terrain_data.push_back(v);
            }
        }
    }

    float scale = 0.005f;
    float z = 0;
    int rows = 1000;
    int cols = 1000;
    std::vector<float> terrain_data;
};

int main(int argc, char const *argv[])
{
    TerrainDemo demo(640,480,"Terrain Demo", false);
    demo.run();
    return 0;
}

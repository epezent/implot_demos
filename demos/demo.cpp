#include "App.hpp"

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

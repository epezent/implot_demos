// Demo:   demo.cpp
// Author: Evan Pezent (evanpezent.com)
// Date:   3/26/2021

#include "App.h"

void ShowDemo_DragLines() {
    ImGui::BulletText("Click and drag the horizontal and vertical lines.");
    static double x1 = 0.2;
    static double x2 = 0.8;
    static double y1 = 0.25;
    static double y2 = 0.75;
    static double f = 0.1;
    // ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    // ImGui::SetNextWindowPos(ImVec2());
    // ImGui::Begin("AAA", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize);
    ImVec2 graphWindowSize = ImGui::GetContentRegionAvail();// ImGui::GetWindowSize();
    constexpr size_t GraphAmount = 1;
    ImVec2 oneGraphSize = ImVec2(-1, graphWindowSize.y / GraphAmount);
    if (oneGraphSize.y < 150.f) {
        oneGraphSize.y = 150.f;
    }
    static ImPlotDragToolFlags flags = ImPlotDragToolFlags_None;
ImGui::CheckboxFlags("NoCursors", (unsigned int*)&flags, ImPlotDragToolFlags_NoCursors); ImGui::SameLine();
ImGui::CheckboxFlags("NoFit", (unsigned int*)&flags, ImPlotDragToolFlags_NoFit); ImGui::SameLine();
ImGui::CheckboxFlags("NoInput", (unsigned int*)&flags, ImPlotDragToolFlags_NoInputs);
ImGui::BeginChild("GraphWindow", graphWindowSize, false);
for (size_t i = 0; i < GraphAmount; ++i) {
	std::string s = "lines" + std::to_string(i);
	if (ImPlot::BeginPlot(s.c_str(), oneGraphSize)) {
		ImPlot::SetupAxesLimits(0, 1, 0, 1, ImPlotCond_Always);
		// if (x1 >= 0. && x1 <= 1.) {
        if (!ImGui::GetIO().KeyCtrl)
			ImPlot::DragLineX(0, &x1, ImVec4(1, 1, 1, 1), 1, flags);
		// }
        if (!ImGui::GetIO().KeyShift)
			ImPlot::DragLineX(1, &x2, ImVec4(1, 1, 1, 1), 1, flags); // AAAA
		
		// ImPlot::DragLineY(2, &y1, ImVec4(1, 1, 1, 1), 1, flags);
		// ImPlot::DragLineY(3, &y2, ImVec4(1, 1, 1, 1), 1, flags);
		double xs[1000], ys[1000];
		for (int j = 0; j < 1000; ++j) {
			xs[j] = (x2 + x1) / 2 + fabs(x2 - x1) * (j / 1000.0f - 0.5f);
			ys[j] = (y1 + y2) / 2 + fabs(y2 - y1) / 2 * sin(f * j / 10);
		}
		ImPlot::PlotLine("Interactive Data", xs, ys, 1000);

		// ImPlot::DragLineY(120482, &f, ImVec4(1, 0.5f, 1, 1), 1, flags);
		ImPlot::EndPlot();
	}
}
ImGui::EndChild();
// ImGui::End();
}

struct ImPlotDemo : App {
    using App::App;
    void Update() override {
        ImPlot::ShowDemoWindow();   
        ImGui::ShowDemoWindow();
        ImPlot::ShowMetricsWindow();
        ImGui::Begin("Test");
        ShowDemo_DragLines();
        ImGui::End();
    }
};

int main(int argc, char const *argv[])
{
    ImPlotDemo app("ImPlot Demo",1920,1080,argc,argv);
    app.Run();
    return 0;
}

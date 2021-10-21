// Demo:   graph.cpp
// Author: Evan Pezent (evanpezent.com)
// Date:   6/7/2021

#include "App.h"
#include <exprtk.hpp>
#include <iostream>
#include <imgui_stdlib.h>

template <typename T>
static inline T remap(T x, T x0, T x1, T y0, T y1) 
{ return y0 + (x - x0) * (y1 - y0) / (x1 - x0); }

struct Expression {
    Expression() {
        table.add_variable("x",x);
        table.add_constants();
        expr.register_symbol_table(table);
    }

    bool set(const std::string& _str) {
        str = _str;
        return valid = parser.compile(_str, expr);
    }

    double eval(double _x) {
        x = _x;
        return expr.value();
    }

    ImVec4 color;
    bool valid;
    std::string str;
    exprtk::symbol_table<double> table;
    exprtk::expression<double> expr;
    exprtk::parser<double> parser;
    double x;
};

struct ImGraph : App {

    Expression expr;
    ImPlotRect limits;

    using App::App;

    void Start() override {
        expr.set("0.25*sin(2*pi*5*x)+0.5");
        expr.color = ImVec4(1,0.75f,0,1);
    }

    void Update() override {
   
        ImGui::SetNextWindowSize(GetWindowSize());
        ImGui::SetNextWindowPos({0,0});

        ImGui::Begin("ImGraph",nullptr,ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize);
        bool valid = expr.valid;
        if (!valid)
            ImGui::PushStyleColor(ImGuiCol_FrameBg, {1,0,0,1});
        if (ImGui::InputText("f(x)",&expr.str,ImGuiInputTextFlags_EnterReturnsTrue)) 
            expr.set(expr.str);
        if (!valid)
            ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::ColorEdit4("##Color",&expr.color.x,ImGuiColorEditFlags_NoInputs);
        
        if (ImPlot::BeginPlot("##Plot",0,0,ImVec2(-1,-1),0,ImPlotAxisFlags_NoInitialFit,ImPlotAxisFlags_NoInitialFit)) {
            limits = ImPlot::GetPlotLimits();
            if (valid) {
                ImPlot::SetNextLineStyle(expr.color);
                ImPlot::PlotLineG("##item",
                    [](void* data, int idx) {
                    auto& self = *(ImGraph*)data;
                    double x = remap((double)idx, 0.0, 9999.0, self.limits.X.Min, self.limits.X.Max);
                    double y = self.expr.eval(x);
                    return ImPlotPoint(x,y);
                    },
                    this,
                    10000);
            }
            ImPlot::EndPlot();
        }
        ImGui::End();
    }
};

int main(int argc, char const *argv[])
{
    ImGraph app("ImGraph",640,480,argc,argv);
    app.Run();
    return 0;
}

// Demo:   filter.cpp
// Author: Evan Pezent (evanpezent.com)
// Date:   3/26/2021

#include "App.h"

#include <ETFE.hpp>
#include <Iir.h>

#include <random>

using namespace etfe;

namespace ImGui {
bool SliderDouble(const char* label, double* v, double v_min, double v_max, const char* format = NULL, ImGuiSliderFlags flags = 0) {
    return SliderScalar(label, ImGuiDataType_Double, v, &v_min, &v_max, format, flags);
}

bool SliderDouble2(const char* label, double v[2], double v_min, double v_max, const char* format = NULL, ImGuiSliderFlags flags = 0) {
    return SliderScalarN(label, ImGuiDataType_Double, v, 2, &v_min, &v_max, format, flags);
}
}

struct ImFilter : public App {

    using App::App;

    void Update() override {
   
        static bool init = true;

        constexpr int N = 10000;
        constexpr double Fs = 1000;
        constexpr double dt = 1.0 / Fs;

        static double f[] = {25,50};
        static double a[] = {0.5,0.5};

        static double ng = 0.25;
        static std::vector<double> noise(N);
        static std::default_random_engine generator;
        static std::normal_distribution<double> distribution(0,1);

        static int filter = 0;
        static double Fc[] = {100,100};
        static Iir::Butterworth::LowPass<2>  butt2lp; // 0
        static Iir::Butterworth::HighPass<2> butt2hp; // 1
        static Iir::Butterworth::BandPass<2> butt2bp; // 2
        static Iir::Butterworth::BandStop<2> butt2bs; // 3

        Fc[0] = std::clamp(Fc[0],1.0,499.0);

        if (init) {
            // generate static noise
            for (auto& n : noise)
                n = distribution(generator);
            // initialize filters
            butt2lp.setup(Fs,Fc[0]);
            butt2hp.setup(Fs,Fc[0]);
            butt2bp.setup(Fs,Fc[0],Fc[1]);
            butt2bs.setup(Fs,Fc[0],Fc[1]);
            init = false;
        }

        // gui inputs
        static bool signal_need_update = true;
        static bool etfe_need_update   = true;
        static bool filt_need_update    = true;

        ImGui::SetNextWindowPos(ImVec2(0,0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(GetWindowSize(), ImGuiCond_Always);
        ImGui::Begin("Filter",nullptr, ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoTitleBar);
        ImGui::BeginChild("ChildL", ImVec2(ImGui::GetWindowContentRegionWidth() * 0.5f, -1));
        ImGui::Text("Input:    x(t) = A1*sin(2*pi*F1*t) + A2*sin(2*pi*F1*t) + noise");
        ImGui::Separator();
        
        ImGui::SliderDouble2("F1 / F2",f,0,500,"%.0f Hz");
        ImGui::SliderDouble2("A1 / A2",a,0,10,"%.1f");
        ImGui::SliderDouble("Noise",&ng,0,1,"%.3f");    

        ImGui::Separator();    
        ImGui::Text("Output:    y(t) = filter(x(t))");

        ImGui::Combo("Filter Type", &filter,"LowPass\0HighPass\0BandPass\0BandStop\0");
        if (filter > 1) {
            if (ImGui::SliderDouble2("Filter Center/Width, Fc/Fw",Fc,1,499,"%.0f Hz") || filt_need_update) {
                butt2bp.setup(Fs,Fc[0],Fc[1]);
                butt2bs.setup(Fs,Fc[0],Fc[1]);
            }
        }
        else {
            if (ImGui::SliderDouble("Filter Cutoff, Fc",Fc,1,499,"%.0f Hz") || filt_need_update) {
                butt2lp.setup(Fs,Fc[0]);
                butt2hp.setup(Fs,Fc[0]);
            }
        }
        // reset filters
        butt2lp.reset();
        butt2hp.reset();
        butt2bp.reset();
        // generate waveform
        static double x[N];
        static double y[N];
        static double t[N];
        for (int i = 0; i < N; ++i) {            
            t[i] = i * dt;
            x[i] = a[0]*sin(2*pi*f[0]*t[i]) + a[1]*sin(2*pi*f[1]*t[i]) + ng * noise[i];
            y[i] = filter == 0 ? butt2lp.filter(x[i]) : 
                   filter == 1 ? butt2hp.filter(x[i]) : 
                   filter == 2 ? butt2bp.filter(x[i]) : 
                                 butt2bs.filter(x[i]);
        }

        // plot waveforms
        if (ImPlot::BeginPlot("##Filter",ImVec2(-1,-1))) {
            ImPlot::SetupAxes("Time [s]","Signal");
            ImPlot::SetupAxesLimits(0,0.5,-2,2);

            ImPlot::SetupLegend(ImPlotLocation_NorthEast);
            ImPlot::PlotLine("x(t)", t, x, N);
            ImPlot::PlotLine("y(t)", t, y, N);
            ImPlot::EndPlot();
        }
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("ChildR", ImVec2(0, -1));

        // perform ETFE
        static int window         = 0;
        static int inwindow        = 4;
        static int nwindow_opts[] = {100, 200, 500, 1000, 2000, 5000, 10000};
        static int infft          = 4;
        static int nfft_opts[]    = {100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000};
        static float overlap      = 0.5f;

        static ETFE etfe(N, Fs, hamming(nwindow_opts[inwindow]), nwindow_opts[inwindow]/2, nfft_opts[infft]);        

        ImGui::Text("Frequency Response");
        ImGui::Separator();
        if (ImGui::Combo("FFT Size", &infft, "100\0""200\0""500\0""1000\0""2000\0""5000\0""10000\0""20000\0""50000\0")) {
            inwindow = infft < inwindow ? infft : inwindow;
            etfe_need_update = true;
        }
        if (ImGui::Combo("Window Type", &window,"hamming\0hann\0winrect\0"))                                           
            etfe_need_update = true;
        if (ImGui::Combo("Window Size", &inwindow,"100\0""200\0""500\0""1000\0""2000\0""5000\0""10000\0"))               
            etfe_need_update = true;
        if (ImGui::SliderFloat("Window Overlap",&overlap,0,1,"%.2f"))                                                                  
            etfe_need_update = true;

        ImGui::NewLine();
            
        if (etfe_need_update) {
            infft = inwindow > infft ? inwindow : infft;
            int nwindow  = nwindow_opts[inwindow];
            int noverlap = (int)(nwindow * overlap);
            int nfft     = nfft_opts[infft];
            etfe.setup(N,Fs,window == 0 ? hamming(nwindow) : window == 1 ? hann(nwindow) : winrect(nwindow), noverlap, nfft);
            etfe_need_update = false;
        }

        auto& result = etfe.estimate(x,y);  
       
        if (ImGui::BeginTabBar("Plots")) {
            if (ImGui::BeginTabItem("Magnitude")) {
                static const double co = -3;
                if (ImPlot::BeginPlot("##Bode1",ImVec2(-1,-1))) {
                    ImPlot::SetupAxesLimits(1,500,-100,10);
                    ImPlot::SetupAxes("Frequency [Hz]","Magnitude [dB]",ImPlotAxisFlags_LogScale);
                    ImPlot::SetNextLineStyle({1,1,1,1});
                    ImPlot::PlotInfLines("##3dB",&co,1,ImPlotInfLinesFlags_Horizontal);
                    ImPlot::SetNextFillStyle(IMPLOT_AUTO_COL, 0.250f);
                    ImPlot::PlotShaded("##Mag1",result.f.data(),result.mag.data(),(int)result.f.size(),-INFINITY);
                    ImPlot::PlotLine("##Mag2",result.f.data(),result.mag.data(),(int)result.f.size());
                    ImPlot::Annotation(Fc[0],-3,ImVec4(0.15f,0.15f,0.15f,1),ImVec2(5,-5),true,"Half-Power Point");
                    if (ImPlot::DragLineX(148884,&Fc[0],ImVec4(0.15f,0.15f,0.15f,1)))
                        filt_need_update = true;
                    ImPlot::EndPlot();
                }
                ImGui::EndTabItem();
            }            
            if (ImGui::BeginTabItem("Phase")) {
                if (ImPlot::BeginPlot("##Bode2",ImVec2(-1,-1))) {  
                    ImPlot::SetupAxesLimits(1,500,-180,10);
                    ImPlot::SetupAxes("Frequency [Hz]","Phase Angle [deg]",ImPlotAxisFlags_LogScale);
                    ImPlot::SetNextFillStyle(IMPLOT_AUTO_COL, 0.250f);
                    ImPlot::PlotShaded("##Phase1",result.f.data(),result.phase.data(),(int)result.f.size(),-INFINITY);
                    ImPlot::PlotLine("##Phase2",result.f.data(),result.phase.data(),(int)result.f.size());
                    if (ImPlot::DragLineX(439829,&Fc[0],ImVec4(0.15f,0.15f,0.15f,1)))
                        filt_need_update = true;
                    ImPlot::EndPlot();
                }
                ImGui::EndTabItem();
            }   
            if (ImGui::BeginTabItem("Amplitude")) {
                if (ImPlot::BeginPlot("##Amp",ImVec2(-1,-1))) {
                    ImPlot::SetupAxesLimits(1,500,0,1.0);
                    ImPlot::SetupAxes("Frequency [Hz]","Amplitude [dB]");
                    ImPlot::SetupLegend(ImPlotLocation_NorthEast);
                    ImPlot::SetNextFillStyle(IMPLOT_AUTO_COL, 0.25f);
                    ImPlot::PlotShaded("x(f)",result.f.data(),result.ampx.data(),(int)result.f.size(),-INFINITY);
                    ImPlot::PlotLine("x(f)",result.f.data(),result.ampx.data(),(int)result.f.size());
                    ImPlot::SetNextFillStyle(IMPLOT_AUTO_COL, 0.25f);
                    ImPlot::PlotShaded("y(f)",result.f.data(),result.ampy.data(),(int)result.f.size(),-INFINITY);
                    ImPlot::PlotLine("y(f)",result.f.data(),result.ampy.data(),(int)result.f.size());
                    if (ImPlot::DragLineX(397391,&Fc[0],ImVec4(0.15f,0.15f,0.15f,1)))
                        filt_need_update = true;

                    if (ImPlot::DragLineY(939031,&a[0],ImVec4(0.15f,0.15f,0.15f,1)))
                        signal_need_update = true;                    
                    if (ImPlot::DragLineY(183853,&a[1],ImVec4(0.15f,0.15f,0.15f,1)))
                        signal_need_update = true;
                    ImPlot::EndPlot();
                }
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Power")) {
                static std::vector<double> pxx10, pyy10;
                pxx10.resize(result.pxx.size());
                pyy10.resize(result.pyy.size());
                for (int i = 0; i < pxx10.size(); ++i) {
                    pxx10[i] = 10*std::log10(result.pxx[i]);
                    pyy10[i] = 10*std::log10(result.pyy[i]);
                }
                if (ImPlot::BeginPlot("##Power",ImVec2(-1,-1))) {
                    ImPlot::SetupAxesLimits(1,500,-100,0);
                    ImPlot::SetupAxes("Frequency [Hz]","Power Spectral Density (dB/Hz)");
                    ImPlot::SetupLegend(ImPlotLocation_NorthEast);
                    ImPlot::SetNextFillStyle(IMPLOT_AUTO_COL, 0.25f);
                    ImPlot::PlotShaded("x(f)",result.f.data(),pxx10.data(),(int)result.f.size(),-INFINITY);
                    ImPlot::PlotLine("x(f)",result.f.data(),pxx10.data(),(int)result.f.size());
                    ImPlot::SetNextFillStyle(IMPLOT_AUTO_COL, 0.25f);
                    ImPlot::PlotShaded("y(f)",result.f.data(),pyy10.data(),(int)result.f.size(),-INFINITY);
                    ImPlot::PlotLine("y(f)",result.f.data(),pyy10.data(),(int)result.f.size());

                    ImPlot::EndPlot();
                }
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }           
        ImGui::EndChild();
        ImGui::End();

    }
};

int main(int argc, char const *argv[]) {
    ImFilter app("ImFilter",960,540,argc,argv);
    app.Run();
    return 0;
}

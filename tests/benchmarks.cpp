// Demo:   demo.cpp
// Author: Evan Pezent (evanpezent.com)
// Date:   3/26/2021

#include "App.h"
#include <implot_internal.h>
#include <chrono>
#include <vector>
#include <iostream>
#include <algorithm>

using Clock = std::chrono::high_resolution_clock;

// utility structure for realtime plot
struct ScrollingBuffer {
    int MaxSize;
    int Offset;
    ImVector<ImVec2> Data;
    ScrollingBuffer(int max_size = 2000) {
        MaxSize = max_size;
        Offset  = 0;
        Data.reserve(MaxSize);
    }
    void AddPoint(float x, float y) {
        if (Data.size() < MaxSize)
            Data.push_back(ImVec2(x,y));
        else {
            Data[Offset] = ImVec2(x,y);
            Offset =  (Offset + 1) % MaxSize;
        }
    }
    void Erase() {
        if (Data.size() > 0) {
            Data.shrink(0);
            Offset  = 0;
        }
    }
};

template <int T>
class AverageFilter  {
public:
    AverageFilter() { 
        for (int i = 0; i < T; ++i)
            s[i] = 0;
    }
    float filter(float in) {
        s[T-1] = in;
        float sum = s[0];
        for (int i = 1; i < T; ++i) {
            sum += s[i];
            s[i-1] = s[i];
        }
        return sum / T;
    }
private:
    float s[T];
};

class MedianFilter {
public:
    MedianFilter(int N)  { resize(N); }
    double filter(float sample) {
        size_t W = buffer1.size();
        buffer1[W-1] = sample;
        buffer2 = buffer1;
        std::sort(buffer2.begin(), buffer2.end());
        size_t M = W/2;
        float value = buffer2[M];
        for (size_t i = 1; i < W; ++i)
            buffer1[i-1] = buffer1[i];
        return value;
    }   
    void resize(int N) { buffer1.resize(N); buffer2.resize(N); } 
private:
    std::vector<float> buffer1;
    std::vector<float> buffer2;
};


namespace ImPlot
{

// #define IMPLOT_NORMALIZE2F_OVER_ZERO(VX, VY)                                                       \
//     {                                                                                              \
//         float d2 = VX * VX + VY * VY;                                                              \
//         if (d2 > 0.0f) {                                                                           \
//             float inv_len = 1.0f / ImSqrt(d2);                                                     \
//             VX *= inv_len;                                                                         \
//             VY *= inv_len;                                                                         \
//         }                                                                                          \
    }


#if defined __SSE__ || defined __x86_64__ || defined _M_X64
static inline float  ImInvSqrt(float x)           { return _mm_cvtss_f32(_mm_rsqrt_ss(_mm_set_ss(x))); }
#else
static inline float  ImInvSqrt(float x)           { return 1.0f / sqrtf(x); }
#endif

#define IMPLOT_NORMALIZE2F_OVER_ZERO(VX,VY) do { float d2 = VX*VX + VY*VY; if (d2 > 0.0f) { float inv_len = ImInvSqrt(d2); VX *= inv_len; VY *= inv_len; } } while (0)


void PlotLineInline(const char *label_id, const float *values, int count, double xscale, double x0, int , int )
{
    ImPlotContext &gp = *GImPlot;
    if (BeginItem(label_id, ImPlotCol_Line))
    {
        if (FitThisFrame())
        {
            for (int i = 0; i < count; ++i)
            {
                FitPoint(ImPlotPoint(x0 + xscale * i, values[i]));
            }
        }
        const ImPlotNextItemData &s = GetItemData();
        ImDrawList &DrawList = *GetPlotDrawList();
        if (count > 1 && s.RenderLine)
        {
            const ImU32 col = ImGui::GetColorU32(s.Colors[ImPlotCol_Line]);
            const float weight = s.LineWeight;
            unsigned int prims = count - 1;
            unsigned int prims_culled = 0;
            unsigned int idx = 0;
            const ImVec2 uv = DrawList._Data->TexUvWhitePixel;

            const float minx_pix = gp.PixelRange[0].Min.x;
            const float miny_pix = gp.PixelRange[0].Min.y;
            const float minx_plt = (float)gp.CurrentPlot->XAxis.Range.Min;
            const float miny_plt = (float)gp.CurrentPlot->YAxis[0].Range.Min;
            const float mx = (float)gp.Mx;
            const float my = (float)gp.My[0];

            ImRect cull_rect = gp.CurrentPlot->PlotRect;

            ImPlotPoint plt = ImPlotPoint(0, values[0]);

            ImVec2 P1 = ImVec2(minx_pix + mx * ((float)plt.x - minx_plt),
                               miny_pix + my * ((float)plt.y - miny_plt));
            int h = 0;
            while (prims)
            {
                // find how many can be reserved up to end of current draw command's limit
                unsigned int cnt = ImMin<unsigned int>(prims, (4294967295 - DrawList._VtxCurrentIdx) / 4);
                // make sure at least this many elements can be rendered to avoid situations where at the end of buffer this slow path is not taken all the time
                if (cnt >= ImMin(64u, prims))
                {
                    if (prims_culled >= cnt)
                        prims_culled -= cnt; // reuse previous reservation
                    else
                    {
                        DrawList.PrimReserve((cnt - prims_culled) * 6, (cnt - prims_culled) * 4); // add more elements to previous reservation
                        prims_culled = 0;
                    }
                }
                else
                {
                    if (prims_culled > 0)
                    {
                        DrawList.PrimUnreserve(prims_culled * 6, prims_culled * 4);
                        prims_culled = 0;
                    }
                    cnt = ImMin<unsigned int>(prims, 4294967295 / 4);
                    DrawList.PrimReserve(cnt * 6, cnt * 4); // reserve new draw command
                }
                prims -= cnt;
                for (unsigned int ie = idx + cnt; idx != ie; ++idx)
                {
                    plt = ImPlotPoint(x0 + xscale * (idx + 1), values[idx + 1]);
                    ImVec2 P2 = ImVec2(minx_pix + mx * ((float)plt.x - minx_plt),
                                       miny_pix + my * ((float)plt.y - miny_plt));
                    if (!cull_rect.Overlaps(ImRect(ImMin(P1, P2), ImMax(P1, P2))))
                        prims_culled++;
                    else
                    {
                        float dx = P2.x - P1.x;
                        float dy = P2.y - P1.y;
                        IMPLOT_NORMALIZE2F_OVER_ZERO(dx, dy);
                        dx *= (weight * 0.5f);
                        dy *= (weight * 0.5f);
                        DrawList._VtxWritePtr[0].pos.x = P1.x + dy;
                        DrawList._VtxWritePtr[0].pos.y = P1.y - dx;
                        DrawList._VtxWritePtr[0].uv = uv;
                        DrawList._VtxWritePtr[0].col = col;
                        DrawList._VtxWritePtr[1].pos.x = P2.x + dy;
                        DrawList._VtxWritePtr[1].pos.y = P2.y - dx;
                        DrawList._VtxWritePtr[1].uv = uv;
                        DrawList._VtxWritePtr[1].col = col;
                        DrawList._VtxWritePtr[2].pos.x = P2.x - dy;
                        DrawList._VtxWritePtr[2].pos.y = P2.y + dx;
                        DrawList._VtxWritePtr[2].uv = uv;
                        DrawList._VtxWritePtr[2].col = col;
                        DrawList._VtxWritePtr[3].pos.x = P1.x - dy;
                        DrawList._VtxWritePtr[3].pos.y = P1.y + dx;
                        DrawList._VtxWritePtr[3].uv = uv;
                        DrawList._VtxWritePtr[3].col = col;
                        DrawList._VtxWritePtr += 4;
                        DrawList._IdxWritePtr[0] = (ImDrawIdx)(DrawList._VtxCurrentIdx);
                        DrawList._IdxWritePtr[1] = (ImDrawIdx)(DrawList._VtxCurrentIdx + 1);
                        DrawList._IdxWritePtr[2] = (ImDrawIdx)(DrawList._VtxCurrentIdx + 2);
                        DrawList._IdxWritePtr[3] = (ImDrawIdx)(DrawList._VtxCurrentIdx);
                        DrawList._IdxWritePtr[4] = (ImDrawIdx)(DrawList._VtxCurrentIdx + 2);
                        DrawList._IdxWritePtr[5] = (ImDrawIdx)(DrawList._VtxCurrentIdx + 3);
                        DrawList._IdxWritePtr += 6;
                        DrawList._VtxCurrentIdx += 4;
                    }
                    P1 = P2;
                }
    
            }
            if (prims_culled > 0)
                DrawList.PrimUnreserve(prims_culled * 6, prims_culled * 4);
        }
        EndItem();
    }
}


}

std::vector<float>  Data;
std::vector<float>  Copy;
std::vector<double> Cast;

struct ScopedProfiler {
    ScopedProfiler(size_t& _val) : val(_val) { 
        t1 = Clock::now();
    }
    ~ScopedProfiler() {
        auto t2 = Clock::now();
        val += std::chrono::duration_cast<std::chrono::microseconds>(t2-t1).count();
    }
    std::chrono::high_resolution_clock::time_point t1;
    size_t& val;
};

struct ImPlotBenchmarks : App {
    using App::App;

    ScrollingBuffer metric1;
    
    void init() {
        Data.resize(10000);
        for (int i = 0; i < 10000; ++i)
            Data[i] = 0 + ( std::rand() % ( 1000 - 0 + 1 ) );
    }

    void update() override {

        ImGui::SetNextWindowPos(ImVec2(0,0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(get_window_size(), ImGuiCond_Always);
        ImGui::Begin("Benchmarks");

        auto t = ImGui::GetTime();
        static int stride = 1;
        static int offset = 0;
        static bool G = false;
        static bool do_inline = true;

        ImGui::SliderInt("Stride",&stride,1,8);
        ImGui::SliderInt("Offset",&offset,1,1000);
        ImGui::Checkbox("G",&G);
        ImGui::Checkbox("Inline",&do_inline);
        ImGui::Checkbox("AA",&ImPlot::GetStyle().AntiAliasedLines);
        // ImGui::Checkbox("Indexer",&GImPlot->DebugBools[0]);


        float rr[] = {1,2};

        static float us_temp = 0;

        ImGui::Text("%.3f us",us_temp);
        ImGui::Text("%.2f FPS",ImGui::GetIO().Framerate);

        int n = 50;

        if (ImPlot::BeginSubplots("##Benchs",2,1,ImVec2(-1,-1),0,rr)) {
            if (ImPlot::BeginPlot("Data")) {
                static std::vector<size_t> Ts(n);
                std::fill(Ts.begin(), Ts.end(), 0);

                if (G) {
                    for (int i = 0; i < n; ++i) {
                        ScopedProfiler pro(Ts[i]);
                        ImPlot::PlotLineG("##line",[](void*,int j) { return ImPlotPoint(j,Data[j]); },this,1000);
                    } 
                }
                else if (do_inline) {
                    for (int i = 0; i < n; ++i) {
                        ScopedProfiler pro(Ts[i]);
                        ImPlot::PlotLineInline("##line",Data.data(),1000,1,0,offset,stride*sizeof(float));     
                    }  
                }
                else {
                    for (int i = 0; i < n; ++i) {
                        ScopedProfiler pro(Ts[i]);
                        ImPlot::PlotLine("##line",Data.data(),1000,1,0,offset,stride*sizeof(float));     
                    }  
                }            

                auto m = Ts.begin() + Ts.size()/2;
                std::nth_element(Ts.begin(), m, Ts.end());
                size_t tmed = Ts[Ts.size()/2];
                us_temp = (float)tmed;
                metric1.AddPoint(t,us_temp);
                ImPlot::EndPlot();
            };            
            ImPlot::SetNextPlotLimitsX(t-10,t,ImGuiCond_Always);
            static double line1 = 10;
            ImPlot::SetNextPlotLimitsY(0,100);
            if (ImPlot::BeginPlot("Metrics")) {
                ImPlot::PlotLine("PlotLine (us)",&metric1.Data[0].x,&metric1.Data[0].y,metric1.Data.Size,metric1.Offset,sizeof(ImVec2));
                ImPlot::DragLineY("L1",&line1);
                ImPlot::EndPlot();
            };            
            ImPlot::EndSubplots();
        }
        ImGui::End();
    
    }
};

int main(int argc, char const *argv[])
{
    int offset = 10;
    int stride = sizeof(double);

    int x = ((offset == 0) << 0) | ((stride == sizeof(float)) << 1);

    std::cout << x << std::endl;

    ImPlotBenchmarks bench(500,750,"Benchmarks",false);
    bench.run();
    return 0;
}



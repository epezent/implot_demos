// Demo:   gpu.cpp
// Author: Evan Pezent (evanpezent.com)
// Date:   6/26/2021

#include "App.h"
#include "Shader.h"
#include <implot_internal.h>
#include <vector>
#include <json.hpp>
#include <iomanip>
#include <imgui_stdlib.h>
#include <string>
#include <chrono>
#include <deque>
#include "plot_line_inline.h"

using Clock = std::chrono::high_resolution_clock;

template <typename T>
inline void ImLinearRegression(const T *x, const T *y, int count, T *mOut, T *bOut)
{
    T xbar = ImMean(x, count);
    T ybar = ImMean(y, count);
    T xbar2 = xbar * xbar;
    T xbarybar = xbar * ybar;
    T sigmax2 = 0;
    T sigmaxy = 0;
    T den = (T)(1.0 / count);
    for (int i = 0; i < count; ++i)
    {
        sigmax2 += (x[i] * x[i] - xbar2) * den;
        sigmaxy += (x[i] * y[i] - xbarybar) * den;
    }
    *mOut = sigmaxy / sigmax2;
    *bOut = -xbar * *mOut + ybar;
}

inline void MakeFitLine(const float *x, const float *y, int count, ImVec2 line[2], float *m, float *b)
{
    ImLinearRegression(x, y, count, m, b);
    line[0].x = x[0];
    line[1].x = x[count - 1];
    line[0].y = (*m) * line[0].x + (*b);
    line[1].y = (*m) * line[1].x + (*b);
}

struct ScopedProfiler
{
    ScopedProfiler(int &_val) : val(_val)
    {
        t1 = Clock::now();
    }
    ~ScopedProfiler()
    {
        auto t2 = Clock::now();
        val += (int)std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
    }
    std::chrono::high_resolution_clock::time_point t1;
    int &val;
};

enum BenchmarkType
{
    BenchmarkType_Int = 0,
    BenchmarkType_Float,
    BenchmarkType_Double,
    BenchmarkType_ImVec2,
    BenchmarkType_COUNT
};

static const char *BenchmarkType_Names[] = {"int", "float", "double", "ImVec2"};
static const char *BenchmarkType_Letter[] = {"i","f","d","v"};

static constexpr int kMaxFrames = 60;
static constexpr int kMaxElems = 2000;
static constexpr int kMaxItems = 250;
static constexpr int kAddItems = 10;

template <typename T>
struct BenchmarkDataScalar
{
    BenchmarkDataScalar(int size = kMaxElems) : Col(RandomColor(0.5f)),
                                                Size(size)
    {
        float y = RandomRange<float>(2000, 998000);
        Xs = new T[Size];
        Ys = new T[Size];
        for (int i = 0; i < Size; ++i)
        {
            Xs[i] = static_cast<T>(i);
            Ys[i] = static_cast<T>(y + RandomRange<float>(-2000, 2000));
        }
    }
    ~BenchmarkDataScalar()
    {
        delete[] Xs;
        delete[] Ys;
    }
    const ImVec4 Col;
    const int Size;
    T *Xs;
    T *Ys;
};

typedef BenchmarkDataScalar<int> BenchmarkDataInt;
typedef BenchmarkDataScalar<float> BenchmarkDataFloat;
typedef BenchmarkDataScalar<double> BenchmarkDataDouble;

template <typename T>
struct BenchmarkDataVector
{
    BenchmarkDataVector(int size = kMaxElems) : Col(RandomRange(0.0f, 1.0f), RandomRange(0.0f, 1.0f), RandomRange(0.0f, 1.0f), 0.5f),
                                                Size(size)
    {
        float y = RandomRange<float>(2000, 998000);
        Vs = new T[Size];
        for (int i = 0; i < Size; ++i)
        {
            Vs[i].x = i;
            Vs[i].y = y + RandomRange<float>(-2000, 2000);
        }
    }
    ~BenchmarkDataVector() { delete[] Vs; }
    const ImVec4 Col;
    const int Size;
    T *Vs;
};

typedef BenchmarkDataVector<ImVec2> BenchmarkDataImVec2;
typedef BenchmarkDataVector<ImPlotPoint> BenchmarkDataImPlotPoint;

struct BenchmarkRecord
{
    BenchmarkRecord() {}
    BenchmarkRecord(int n)
    {
        Items.reserve(n);
        Call.reserve(n);
        Frame.reserve(n);
        Fps.reserve(n);
    }
    void AddRecord(int items, float call, float frame, float fps)
    {
        Items.push_back((float)items);
        Call.push_back(call);
        Frame.push_back(frame);
        Fps.push_back(fps);
    }
    void FitData()
    {
        MakeFitLine(Items.data(), Call.data(), Items.size(), Call_Fit, &Call_M, &Call_B);
        MakeFitLine(Items.data(), Frame.data(), Items.size(), Frame_Fit, &Frame_M, &Frame_B);
    }
    std::vector<float> Items;
    std::vector<float> Call;
    ImVec2 Call_Fit[2];
    float Call_M = 0;
    float Call_B = 0;
    std::vector<float> Frame;
    ImVec2 Frame_Fit[2];
    float Frame_M = 0;
    float Frame_B = 0;
    std::vector<float> Fps;
};

struct BenchmarkRecordCollection
{
    BenchmarkRecordCollection() : Col(RandomColor())
    { }
    void SetColorFromString(std::string& name) {
        Col = ImGui::ColorConvertU32ToFloat4(ImHashStr(name.c_str()));
        Col.w = 1.0f;
        ImGui::ColorConvertRGBtoHSV(Col.x, Col.y, Col.z, Col.x, Col.y, Col.z);
        Col.z = ImRemap(Col.z, 0.0f, 1.0f, 0.25f, 1.0f);
        ImGui::ColorConvertHSVtoRGB(Col.x, Col.y, Col.z, Col.x, Col.y, Col.z);
    }
    ImVec4 Col;
    std::vector<BenchmarkRecord> Records;
};

using json = nlohmann::json;

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ImVec2, x, y);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ImVec4, x, y, z, w);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(BenchmarkRecord, Items, Call, Call_Fit, Call_M, Call_B, Frame, Frame_Fit, Frame_M, Frame_B, Fps);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(BenchmarkRecordCollection, Col, Records);

struct IBenchmark
{
    IBenchmark(std::string name) : name(name) {}
    virtual ~IBenchmark() {}
    void RunInt(const BenchmarkDataInt *data, int iterations, int32_t &call_time)
    {
        for (int i = 0; i < iterations; ++i)
        {
            ImGui::PushID(i);
            {
                ScopedProfiler prof(call_time);
                ImPlot::SetNextLineStyle(data[i].Col);
                ImPlot::SetNextFillStyle(data[i].Col);
                PlotInt("##item", data[i].Xs, data[i].Ys, data[i].Size);
            }
            ImGui::PopID();
        }
    }
    void RunFloat(const BenchmarkDataFloat *data, int iterations, int32_t &call_time)
    {
        for (int i = 0; i < iterations; ++i)
        {
            ImGui::PushID(i);
            {
                ScopedProfiler prof(call_time);
                ImPlot::SetNextLineStyle(data[i].Col);
                ImPlot::SetNextFillStyle(data[i].Col);
                PlotFloat("##item", data[i].Xs, data[i].Ys, data[i].Size);
            }
            ImGui::PopID();
        }
    }
    void RunDouble(const BenchmarkDataDouble *data, int iterations, int32_t &call_time)
    {
        for (int i = 0; i < iterations; ++i)
        {
            ImGui::PushID(i);
            {
                ScopedProfiler prof(call_time);
                ImPlot::SetNextLineStyle(data[i].Col);
                ImPlot::SetNextFillStyle(data[i].Col);
                PlotDouble("##item", data[i].Xs, data[i].Ys, data[i].Size);
            }
            ImGui::PopID();
        }
    }
    void RunImVec2(const BenchmarkDataImVec2 *data, int iterations, int32_t &call_time)
    {
        for (int i = 0; i < iterations; ++i)
        {
            ImGui::PushID(i);
            {
                ScopedProfiler prof(call_time);
                ImPlot::SetNextLineStyle(data[i].Col);
                ImPlot::SetNextFillStyle(data[i].Col);
                PlotImVec2("##item", data[i].Vs, data[i].Size);
            }
            ImGui::PopID();
        }
    }
    virtual void PlotInt(const char *name, int *xs, int *ys, int count) = 0;
    virtual void PlotFloat(const char *name, float *xs, float *ys, int count) = 0;
    virtual void PlotDouble(const char *name, double *xs, double *ys, int count) = 0;
    virtual void PlotImVec2(const char *name, ImVec2 *vs, int count) = 0;
    const std::string name;
};

struct Benchmark_PlotLine : IBenchmark
{
    Benchmark_PlotLine() : IBenchmark("Line") {}
    virtual void PlotInt(const char *n, int *xs, int *ys, int k) override { ImPlot::PlotLine(n, xs, ys, k); }
    virtual void PlotFloat(const char *n, float *xs, float *ys, int k) override { ImPlot::PlotLine(n, xs, ys, k); }
    virtual void PlotDouble(const char *n, double *xs, double *ys, int k) override { ImPlot::PlotLine(n, xs, ys, k); }
    virtual void PlotImVec2(const char *n, ImVec2 *vs, int k) override { ImPlot::PlotLine(n, &vs[0].x, &vs[0].y, k, 0, sizeof(ImVec2)); }
};

struct Benchmark_PlotScatter : IBenchmark
{
    Benchmark_PlotScatter() : IBenchmark("Scatter") {}
    virtual void PlotInt(const char *n, int *xs, int *ys, int k) override
    {
        ImPlot::SetNextMarkerStyle(ImPlotMarker_Square, 2);
        ImPlot::PlotScatter(n, xs, ys, k);
    }
    virtual void PlotFloat(const char *n, float *xs, float *ys, int k) override
    {
        ImPlot::SetNextMarkerStyle(ImPlotMarker_Square, 2);
        ImPlot::PlotScatter(n, xs, ys, k);
    }
    virtual void PlotDouble(const char *n, double *xs, double *ys, int k) override
    {
        ImPlot::SetNextMarkerStyle(ImPlotMarker_Square, 2);
        ImPlot::PlotScatter(n, xs, ys, k);
    }
    virtual void PlotImVec2(const char *n, ImVec2 *vs, int k) override
    {
        ImPlot::SetNextMarkerStyle(ImPlotMarker_Square, 2);
        ImPlot::PlotScatter(n, &vs[0].x, &vs[0].y, k, 0, sizeof(ImVec2));
    }
};

struct Benchmark_PlotBars : IBenchmark
{
    Benchmark_PlotBars() : IBenchmark("Bars") {}
    virtual void PlotInt(const char *n, int *xs, int *ys, int k) override { ImPlot::PlotBars(n, xs, ys, k, 1); }
    virtual void PlotFloat(const char *n, float *xs, float *ys, int k) override { ImPlot::PlotBars(n, xs, ys, k, 1); }
    virtual void PlotDouble(const char *n, double *xs, double *ys, int k) override { ImPlot::PlotBars(n, xs, ys, k, 1); }
    virtual void PlotImVec2(const char *n, ImVec2 *vs, int k) override { ImPlot::PlotBars(n, &vs[0].x, &vs[0].y, k, 1, 0, sizeof(ImVec2)); }
};

struct Benchmark_PlotShaded : IBenchmark
{
    Benchmark_PlotShaded() : IBenchmark("Shaded") {}
    virtual void PlotInt(const char *n, int *xs, int *ys, int k) override { ImPlot::PlotShaded(n, xs, ys, k); }
    virtual void PlotFloat(const char *n, float *xs, float *ys, int k) override { ImPlot::PlotShaded(n, xs, ys, k); }
    virtual void PlotDouble(const char *n, double *xs, double *ys, int k) override { ImPlot::PlotShaded(n, xs, ys, k); }
    virtual void PlotImVec2(const char *n, ImVec2 *vs, int k) override { ImPlot::PlotShaded(n, &vs[0].x, &vs[0].y, k, 0, sizeof(ImVec2)); }
};

struct Benchmark_PlotLineInline : IBenchmark
{
    Benchmark_PlotLineInline() : IBenchmark("LineInline") {}
    virtual void PlotInt(const char *n, int *xs, int *ys, int k) override { ImPlot::PlotLineInline(n, xs, ys, k); }
    virtual void PlotFloat(const char *n, float *xs, float *ys, int k) override { ImPlot::PlotLineInline(n, xs, ys, k); }
    virtual void PlotDouble(const char *n, double *xs, double *ys, int k) override { ImPlot::PlotLineInline(n, xs, ys, k); }
    virtual void PlotImVec2(const char *n, ImVec2 *vs, int k) override {}
};

struct Benchmark_PlotLineStaged : IBenchmark
{
    Benchmark_PlotLineStaged() : IBenchmark("LineStaged") {}
    virtual void PlotInt(const char *n, int *xs, int *ys, int k) override { ImPlot::PlotLineStaged(n, xs, ys, k); }
    virtual void PlotFloat(const char *n, float *xs, float *ys, int k) override { ImPlot::PlotLineStaged(n, xs, ys, k); }
    virtual void PlotDouble(const char *n, double *xs, double *ys, int k) override { ImPlot::PlotLineStaged(n, xs, ys, k); }
    virtual void PlotImVec2(const char *n, ImVec2 *vs, int k) override {}
};

struct BenchmarkRun
{
    int benchmark;
    int type;
    int items;
    int elems;
    std::string name;
};

typedef std::deque<BenchmarkRun> BenchmarkQueue;

struct ImPlotBench : App
{
    std::map<std::string, std::map<std::string, BenchmarkRecordCollection>> m_records;
    std::string m_branch;
    std::vector<std::unique_ptr<IBenchmark>> m_benchmarks;

    BenchmarkDataInt m_items_int[kMaxItems];
    BenchmarkDataFloat m_items_float[kMaxItems];
    BenchmarkDataDouble m_items_double[kMaxItems];
    BenchmarkDataImVec2 m_items_imvec2[kMaxItems];

    BenchmarkQueue m_queue;
    std::map<std::string, BenchmarkQueue> m_queues;

    ImPlotBench(int argc, char const *argv[]) : App("ImPlot Benchmark", 640, 480, argc, argv)
    {
    }

    ~ImPlotBench()
    {
        json j;
        j["records"] = m_records;
        std::ofstream file("bench.json");
        if (file.is_open())
            file << std::setw(4) << j;
    }

    void Start() override
    {
        std::ifstream file("bench.json");
        if (file.is_open())
        {
            json j;
            file >> j;
            j.at("records").get_to(m_records);
        }

        GetBranchName("C:/git/implot/implot", m_branch);

        m_benchmarks.push_back(std::make_unique<Benchmark_PlotLine>());
        m_benchmarks.push_back(std::make_unique<Benchmark_PlotScatter>());
        m_benchmarks.push_back(std::make_unique<Benchmark_PlotBars>());
        m_benchmarks.push_back(std::make_unique<Benchmark_PlotShaded>());
        m_benchmarks.push_back(std::make_unique<Benchmark_PlotLineInline>());
        m_benchmarks.push_back(std::make_unique<Benchmark_PlotLineStaged>());

        m_queues["All Plots"] = {
            {0, BenchmarkType_Float, 250, 2000},
            {1, BenchmarkType_Float, 250, 2000},
            {2, BenchmarkType_Float, 250, 2000},
            {3, BenchmarkType_Float, 250, 2000},
        };

        m_queues["All Types"] = {
            {0, BenchmarkType_Int, 250, 2000},
            {0, BenchmarkType_Float, 250, 2000},
            {0, BenchmarkType_Double, 250, 2000},
            {0, BenchmarkType_ImVec2, 250, 2000},
        };
    }

    void Update() override
    {
        ImGui::SetNextWindowSize(GetWindowSize(), ImGuiCond_Always);
        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
        ImGui::Begin("ImPlot Benchmark Tool", 0, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);
        if (ImGui::BeginTabBar("Tabs"))
        {
            if (ImGui::BeginTabItem("Benchmark"))
            {
                ShowBenchmarkTool();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Results"))
            {
                ShowResultsTool();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Compare")) {
                ShowCompareTool();
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
        ImGui::End();
    }

    void ShowBenchmarkTool()
    {
        static int current_items = 0;
        static int current_frame = 0;

        static int selected_bench = 0;
        static int selected_type = BenchmarkType_Float;
        static int tcall = 0;
        static double t1 = 0;
        static double t2 = 0;
        static bool running = false;

        auto GetRunName = [&]()
        {
            auto str = m_benchmarks[selected_bench]->name + "_" + BenchmarkType_Names[selected_type];
            if (this->UsingDGPU)   
                str += "_g";
            return str;
        };

        static std::string working_name = GetRunName();
        static BenchmarkRun working_run;
        static BenchmarkRecord working_record;

        auto StartNextRun = [&]()
        {
            working_run = m_queue.front();
            m_queue.pop_front();
            selected_type = working_run.type;
            selected_bench = working_run.benchmark;
            if (working_run.name.empty())
                working_run.name = GetRunName();
            working_name = working_run.name;
            working_record = BenchmarkRecord(kMaxItems + 1);
            t1 = ImGui::GetTime();
            current_items = current_frame = 0;
        };

        if (running)
        {
            current_frame++;
            if (current_frame == kMaxFrames)
            {
                t2 = ImGui::GetTime();
                working_record.AddRecord(current_items, (tcall / kMaxFrames) * 0.001f, 1000.0 * (t2 - t1) / kMaxFrames, kMaxFrames / (t2 - t1));
                current_items += kAddItems;
                current_frame = 0;
                tcall = 0;
                t1 = ImGui::GetTime();
            }
            if (current_items > kMaxItems)
            {
                working_record.FitData();
                m_records[m_branch][working_name].Records.push_back(working_record);
                m_records[m_branch][working_name].SetColorFromString(working_name);
                if (m_queue.size() > 0)
                    StartNextRun();
                else
                    running = false;
            }
        }

        bool was_running = running;
        if (was_running)
        {
            ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.25f);
        }
        ImGui::SetNextItemWidth(150);
        if (ImGui::BeginCombo("##Benchmark", m_benchmarks[selected_bench]->name.c_str()))
        {
            for (int i = 0; i < m_benchmarks.size(); ++i)
            {
                if (ImGui::Selectable(m_benchmarks[i]->name.c_str(), i == selected_bench))
                {
                    selected_bench = i;
                    working_name = GetRunName();
                }
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(75);
        if (ImGui::Combo("##Type", &selected_type, BenchmarkType_Names, BenchmarkType_COUNT))
            working_name = GetRunName();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150);
        ImGui::InputText("##Name", &working_name);
        if (was_running)
        {
            ImGui::PopItemFlag();
            ImGui::PopStyleVar();
        }
        ImGui::SameLine();
        if (ImGui::Button("+"))
        {
            m_queue.push_back({selected_bench, selected_type, kMaxItems, kMaxElems, working_name});
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(75);
        if (ImGui::BeginCombo("##Qs", "Batch"))
        {
            for (auto &q : m_queues)
            {
                if (ImGui::Selectable(q.first.c_str(), false))
                {
                    for (auto &r : q.second)
                        m_queue.push_back(r);
                }
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        static char start_str[32];
        sprintf(start_str, "Start (%u)", m_queue.size());
        if (ImGui::Button(running ? "Stop" : start_str, ImVec2(-1, 0)))
        {
            running = !running;
            current_items = current_frame = 0;
            if (running)
            {
                if (m_queue.size() == 0)
                    m_queue.push_back({selected_bench, selected_type, kMaxItems, kMaxElems, working_name});
                StartNextRun();
            }
            else
            {
                m_queue.clear();
            }
        }
        // ImGui::SameLine();
        // if (ImGui::Button("X", ImVec2(-1,0)))
        //     m_queue.clear();
        ImGui::ProgressBar((float)current_items / (float)(kMaxItems - 1));
        if (ImPlot::BeginPlot("##Bench", ImVec2(-1, -1), ImPlotFlags_NoChild | ImPlotFlags_CanvasOnly))
        {
            ImPlot::SetupAxesLimits(0, kMaxElems, 0, 1000000, ImGuiCond_Always);
            ImPlot::SetupAxes(NULL, NULL, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_NoDecorations);
            if (running)
            {
                switch (selected_type)
                {
                case BenchmarkType_Int:
                    m_benchmarks[selected_bench]->RunInt(m_items_int, current_items, tcall);
                    break;
                case BenchmarkType_Float:
                    m_benchmarks[selected_bench]->RunFloat(m_items_float, current_items, tcall);
                    break;
                case BenchmarkType_Double:
                    m_benchmarks[selected_bench]->RunDouble(m_items_double, current_items, tcall);
                    break;
                case BenchmarkType_ImVec2:
                    m_benchmarks[selected_bench]->RunImVec2(m_items_imvec2, current_items, tcall);
                    break;
                default:
                    break;
                }
            }
            ImPlot::EndPlot();
        }
    }

    void ShowResultsTool()
    {
        static bool show_call_time = true;
        static bool show_frame_time = false;
        static bool show_fps = false;
        static bool show_fit = true;
        static bool show_data = false;
        static std::string selected_branch = m_branch;

        if (ImGui::Button("Clear"))
            m_records[selected_branch].clear();
        ImGui::SameLine();
        ImGui::Checkbox("Call Time", &show_call_time);
        ImGui::SameLine();
        ImGui::Checkbox("Frame Time", &show_frame_time);
        ImGui::SameLine();
        ImGui::Checkbox("FPS", &show_fps);
        ImGui::SameLine();
        ImGui::Checkbox("Fit", &show_fit);
        ImGui::SameLine();
        ImGui::Checkbox("Data", &show_data);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1);
        if (ImGui::BeginCombo("##Branch", selected_branch.c_str()))
        {
            for (auto &rec : m_records)
            {
                if (ImGui::Selectable(rec.first.c_str(), rec.first == selected_branch))
                    selected_branch = rec.first;
            }
            ImGui::EndCombo();
        }

        static std::vector<int> to_delete;
        to_delete.clear();

        char xlabel[128];
        sprintf(xlabel, "Num Items (%d pts ea)", kMaxElems);
        if (ImPlot::BeginPlot("##Stats", ImVec2(-1, -1), ImPlotFlags_NoChild))
        {
            ImPlot::SetupAxis(ImAxis_X1, xlabel);
            ImPlot::SetupAxis(ImAxis_Y1, "Time [ms]");            
            ImPlot::SetupAxesLimits(0, kMaxItems, 0, 50);
            ImPlot::SetupAxisLimits(ImAxis_Y2, 0, 500, ImPlotCond_Once);
            ImPlot::SetupLegend(ImPlotLocation_East, ImPlotLegendFlags_Outside);
            if (show_fps)            
                ImPlot::SetupAxis(ImAxis_Y2, "FPS [Hz]", ImPlotAxisFlags_Opposite);
            
            for (auto &collection : m_records[selected_branch])
            {
                auto name = collection.first.c_str();
                auto col  = collection.second.Col;
                ImPlot::PushStyleColor(ImPlotCol_Line, col);
                ImPlot::PushStyleColor(ImPlotCol_MarkerFill, col);

                for (auto& record : collection.second.Records)
                {
                    if (record.Items.size() > 1)
                    {

                        ImPlot::SetAxis(ImAxis_Y1);
                        if (show_call_time)
                        {
                            if (show_fit)
                            {
                                ImPlot::PlotLine(name, &record.Call_Fit[0].x, &record.Call_Fit[0].y, 2, 0, sizeof(ImVec2));
                            }
                            if (show_data)
                            {
                                ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle, 3);
                                ImPlot::PlotScatter(name, record.Items.data(), record.Call.data(), record.Items.size());
                            }
                        }
                        if (show_frame_time)
                        {
                            if (show_fit)
                            {
                                ImPlot::SetNextLineStyle(IMPLOT_AUTO_COL, 2.0f);
                                ImPlot::PlotLine(name, &record.Frame_Fit[0].x, &record.Frame_Fit[0].y, 2, 0, sizeof(ImVec2));
                            }
                            if (show_data)
                            {
                                ImPlot::SetNextMarkerStyle(ImPlotMarker_Square, 3);
                                ImPlot::PlotScatter(name, record.Items.data(), record.Frame.data(), record.Items.size());
                            }
                        }
                        if (show_fps)
                        {
                            ImPlot::SetAxis(ImAxis_Y2);
                            ImPlot::PlotLine(name, record.Items.data(), record.Fps.data(), record.Items.size());
                        }
                    }
                }
                ImPlot::PopStyleColor(2);
            }
            ImPlot::EndPlot();
        }
    }


    void ShowCompareTool() {
        static std::string branchL = m_branch;
        static std::string branchR = m_branch;

        float w = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) / 2;
        ImGui::SetNextItemWidth(w);
        if (ImGui::BeginCombo("##BranchL", branchL.c_str()))
        {
            for (auto &rec : m_records)
            {
                if (ImGui::Selectable(rec.first.c_str(), rec.first == branchL))
                    branchL = rec.first;
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(w);
        if (ImGui::BeginCombo("##BranchR", branchR.c_str()))
        {
            for (auto &rec : m_records)
            {
                if (ImGui::Selectable(rec.first.c_str(), rec.first == branchR))
                    branchR = rec.first;
            }
            ImGui::EndCombo();
        }

        static std::vector<std::string> bench_union;
        bench_union.clear();
        for (auto& bench : m_records[branchL]) {
            if (m_records[branchR].count(bench.first))
                bench_union.push_back(bench.first);
        }

        int numPlots = bench_union.size();
        if (numPlots == 0)
            return;

        int rows = IM_ROUND(ImSqrt(numPlots));
        int cols = (numPlots / rows) * rows >= numPlots ? (numPlots / rows) : (numPlots / rows) + 1;
        if (ImPlot::BeginSubplots("##Comp",rows,cols,ImVec2(-1,-1),ImPlotSubplotFlags_NoResize)) {
            for (auto& bench : bench_union) {
                if (ImPlot::BeginPlot(bench.c_str(), ImVec2(0,0), ImPlotFlags_NoLegend)) {
                    ImPlot::SetupAxis(ImAxis_X1, "##X", ImPlotAxisFlags_NoTickLabels|ImPlotAxisFlags_NoGridLines|ImPlotAxisFlags_NoTickMarks);  
                    ImPlot::SetupAxisLimits(ImAxis_X1,-0.5,1.5f,ImPlotCond_Always);
                    ImPlot::SetupAxis(ImAxis_Y1, "Items / ms",ImPlotAxisFlags_AutoFit);                    
                    float v[2];
                    v[0] = 1.0 / m_records[branchL][bench].Records[0].Call_M; 
                    v[1] = 1.0 / m_records[branchR][bench].Records[0].Call_M;

                    ImPlot::SetNextFillStyle(m_records[branchL][bench].Col);
                    ImPlot::PlotBars("##Item",v,2);

                    ImPlot::EndPlot();
                }
            }
            ImPlot::EndSubplots();
        }

    }
};

int main(int argc, char const *argv[])
{
    ImPlotBench app(argc, argv);
    app.Run();
    return 0;
}

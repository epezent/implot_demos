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
#include <algorithm>
#include "plot_line_inline.h"
#include "Profiler.h"

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
    BenchmarkType_ImPlotPoint,
    BenchmarkType_COUNT
};

static const char *BenchmarkType_Names[] = {"int", "float", "double", "ImVec2", "ImPlotPoint"};
static const char *BenchmarkType_Letter[] = {"i", "f", "d", "v", "p"};

static constexpr int kMaxFrames = 60;
static constexpr int kMaxElems = 500000;
static constexpr int kMaxElemsItem = 5000; // 5000 1000 500  100
static constexpr int kDataNoise = 2500;
static constexpr int kMaxSteps = 10;

static const int kElemValues[] = {100, 500, 1000, 5000};
static const char *kElemsStrings[] = {"100", "500", "1000", "5000"};

template <typename T>
struct BenchmarkDataScalar
{
    BenchmarkDataScalar()
    {
        Xs = new T[kMaxElems];
        Ys = new T[kMaxElems];
        for (int i = 0; i < kMaxElems; i += kMaxElemsItem) {
            for (int j = 0; j < kMaxElemsItem; ++j)
            {
                Xs[i+j] = static_cast<T>(j);
                Ys[i+j] = static_cast<T>(kMaxElems - i + RandomRange<float>(-kDataNoise, kDataNoise));
            }
        }
    }
    ~BenchmarkDataScalar()
    {
        delete[] Xs;
        delete[] Ys;
    }
    T *Xs;
    T *Ys;
};

typedef BenchmarkDataScalar<int> BenchmarkDataInt;
typedef BenchmarkDataScalar<float> BenchmarkDataFloat;
typedef BenchmarkDataScalar<double> BenchmarkDataDouble;

template <typename T>
struct BenchmarkDataVector
{
    BenchmarkDataVector()
    {
        Vs = new T[kMaxElems];
        for (int i = 0; i < kMaxElems; ++i)
        {
            Vs[i].x = RandomRange<float>(0.0f, (float)kMaxElems);
            Vs[i].y = RandomRange<float>(0.0f, (float)kMaxElems);
        }
    }
    ~BenchmarkDataVector() { delete[] Vs; }
    T *Vs;
};

typedef BenchmarkDataVector<ImVec2> BenchmarkDataImVec2;
typedef BenchmarkDataVector<ImPlotPoint> BenchmarkDataImPlotPoint;

struct BenchmarkRecord
{
    BenchmarkRecord()
    {
        Elems.reserve(kMaxSteps + 1);
        Call.reserve(kMaxSteps + 1);
        Frame.reserve(kMaxSteps + 1);
        Fps.reserve(kMaxSteps + 1);
    }
    void AddRecord(int elems, float call, float frame, float fps)
    {
        Elems.push_back((float)elems);
        Call.push_back(call);
        Frame.push_back(frame);
        Fps.push_back(fps);
    }
    void FitData()
    {
        MakeFitLine(Elems.data(), Call.data(), Elems.size(), Call_Fit, &Call_M, &Call_B);
        MakeFitLine(Elems.data(), Frame.data(), Elems.size(), Frame_Fit, &Frame_M, &Frame_B);
    }
    std::vector<float> Elems;

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
    {
    }
    void SetColorFromString(std::string &name)
    {
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
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(BenchmarkRecord, Elems, Call, Call_Fit, Call_M, Call_B, Frame, Frame_Fit, Frame_M, Frame_B, Fps);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(BenchmarkRecordCollection, Col, Records);

struct IBenchmark
{
    IBenchmark(std::string name) : name(name) {}
    virtual ~IBenchmark() {}
    void RunInt(const BenchmarkDataInt &data, int items, int elems, int32_t &call_time)
    {
        srand(0);
        for (int i = 0; i < items; ++i)
        {
            int e = i * elems;
            auto Col = RandomColor(0.5f);
            ImGui::PushID(i);
            {
                ScopedProfiler prof(call_time);
                ImPlot::SetNextLineStyle(Col);
                ImPlot::SetNextFillStyle(Col);
                PlotInt("##item", &data.Xs[e], &data.Ys[e], elems);
            }
            ImGui::PopID();
        }
    }
    void RunFloat(const BenchmarkDataFloat &data, int items, int elems, int32_t &call_time)
    {
        srand(0);
        for (int i = 0; i < items; ++i)
        {
            int e = i * elems;
            auto Col = RandomColor(0.5f);
            ImGui::PushID(i);
            {
                ScopedProfiler prof(call_time);
                ImPlot::SetNextLineStyle(Col);
                ImPlot::SetNextFillStyle(Col);
                PlotFloat("##item", &data.Xs[e], &data.Ys[e], elems);
            }
            ImGui::PopID();
        }
    }
    void RunDouble(const BenchmarkDataDouble &data, int items, int elems, int32_t &call_time)
    {
        srand(0);
        for (int i = 0; i < items; ++i)
        {
            int e = i * elems;
            auto Col = RandomColor(0.5f);
            ImGui::PushID(i);
            {
                ScopedProfiler prof(call_time);
                ImPlot::SetNextLineStyle(Col);
                ImPlot::SetNextFillStyle(Col);
                PlotDouble("##item", &data.Xs[e], &data.Ys[e], elems);
            }
            ImGui::PopID();
        }
    }
    void RunImVec2(const BenchmarkDataImVec2 &data, int items, int elems, int32_t &call_time)
    {
        srand(0);
        for (int i = 0; i < items; ++i)
        {
            int e = i * elems;
            auto Col = RandomColor(0.5f);
            ImGui::PushID(i);
            {
                ScopedProfiler prof(call_time);
                ImPlot::SetNextLineStyle(Col);
                ImPlot::SetNextFillStyle(Col);
                PlotImVec2("##item", &data.Vs[e], elems);
            }
            ImGui::PopID();
        }
    }
    void RunImPlotPoint(const BenchmarkDataImPlotPoint &data, int items, int elems, int32_t &call_time)
    {
        srand(0);
        for (int i = 0; i < items; ++i)
        {
            int e = i * elems;
            auto Col = RandomColor(0.5f);
            ImGui::PushID(i);
            {
                ScopedProfiler prof(call_time);
                ImPlot::SetNextLineStyle(Col);
                ImPlot::SetNextFillStyle(Col);
                PlotImPlotPoint("##item", &data.Vs[e], elems);
            }
            ImGui::PopID();
        }
    }
    virtual void PlotInt(const char *name, int *xs, int *ys, int count) = 0;
    virtual void PlotFloat(const char *name, float *xs, float *ys, int count) = 0;
    virtual void PlotDouble(const char *name, double *xs, double *ys, int count) = 0;
    virtual void PlotImVec2(const char *name, ImVec2 *vs, int count) = 0;
    virtual void PlotImPlotPoint(const char *name, ImPlotPoint *vs, int count) = 0;
    const std::string name;
};

struct Benchmark_PlotLine : IBenchmark
{
    Benchmark_PlotLine() : IBenchmark("Line") {}
    virtual void PlotInt(const char *n, int *xs, int *ys, int k) override { ImPlot::PlotLine(n, xs, ys, k); }
    virtual void PlotFloat(const char *n, float *xs, float *ys, int k) override { ImPlot::PlotLine(n, xs, ys, k); }
    virtual void PlotDouble(const char *n, double *xs, double *ys, int k) override { ImPlot::PlotLine(n, xs, ys, k); }
    virtual void PlotImVec2(const char *n, ImVec2 *vs, int k) override { ImPlot::PlotLine(n, &vs[0].x, &vs[0].y, k, 0, 0, sizeof(ImVec2)); }
    virtual void PlotImPlotPoint(const char *n, ImPlotPoint *vs, int k) override { ImPlot::PlotLine(n, &vs[0].x, &vs[0].y, k, 0, 0, sizeof(ImPlotPoint)); }
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
        ImPlot::PlotScatter(n, &vs[0].x, &vs[0].y, k, 0, 0, sizeof(ImVec2));
    }
    virtual void PlotImPlotPoint(const char *n, ImPlotPoint *vs, int k) override
    {
        ImPlot::SetNextMarkerStyle(ImPlotMarker_Square, 2);
        ImPlot::PlotScatter(n, &vs[0].x, &vs[0].y, k, 0, 0, sizeof(ImPlotPoint));
    }
};

struct Benchmark_PlotBars : IBenchmark
{
    Benchmark_PlotBars() : IBenchmark("Bars") {}
    virtual void PlotInt(const char *n, int *xs, int *ys, int k) override { ImPlot::PlotBars(n, xs, ys, k, 0.8); }
    virtual void PlotFloat(const char *n, float *xs, float *ys, int k) override { ImPlot::PlotBars(n, xs, ys, k, 0.8); }
    virtual void PlotDouble(const char *n, double *xs, double *ys, int k) override { ImPlot::PlotBars(n, xs, ys, k, 0.8); }
    virtual void PlotImVec2(const char *n, ImVec2 *vs, int k) override { ImPlot::PlotBars(n, &vs[0].x, &vs[0].y, k, 0.8, 0, 0, sizeof(ImVec2)); }
    virtual void PlotImPlotPoint(const char *n, ImPlotPoint *vs, int k) override { ImPlot::PlotBars(n, &vs[0].x, &vs[0].y, k, 0.8, 0, 0, sizeof(ImPlotPoint)); }
};

struct Benchmark_PlotShaded : IBenchmark
{
    Benchmark_PlotShaded() : IBenchmark("Shaded") {}
    virtual void PlotInt(const char *n, int *xs, int *ys, int k) override { ImPlot::PlotShaded(n, xs, ys, k); }
    virtual void PlotFloat(const char *n, float *xs, float *ys, int k) override { ImPlot::PlotShaded(n, xs, ys, k); }
    virtual void PlotDouble(const char *n, double *xs, double *ys, int k) override { ImPlot::PlotShaded(n, xs, ys, k); }
    virtual void PlotImVec2(const char *n, ImVec2 *vs, int k) override { ImPlot::PlotShaded(n, &vs[0].x, &vs[0].y, k, 0, 0, sizeof(ImVec2)); }
    virtual void PlotImPlotPoint(const char *n, ImPlotPoint *vs, int k) override { ImPlot::PlotShaded(n, &vs[0].x, &vs[0].y, k, 0, 0, sizeof(ImPlotPoint)); }
};

struct Benchmark_PlotLineInline : IBenchmark
{
    Benchmark_PlotLineInline() : IBenchmark("LineInline") {}
    virtual void PlotInt(const char *n, int *xs, int *ys, int k) override { ImPlot::PlotLineInline(n, xs, ys, k); }
    virtual void PlotFloat(const char *n, float *xs, float *ys, int k) override { ImPlot::PlotLineInline(n, xs, ys, k); }
    virtual void PlotDouble(const char *n, double *xs, double *ys, int k) override { ImPlot::PlotLineInline(n, xs, ys, k); }
    virtual void PlotImVec2(const char *n, ImVec2 *vs, int k) override {}
    virtual void PlotImPlotPoint(const char *n, ImPlotPoint *vs, int k) override {}
};

struct Benchmark_PlotLineStaged : IBenchmark
{
    Benchmark_PlotLineStaged() : IBenchmark("LineStaged") {}
    virtual void PlotInt(const char *n, int *xs, int *ys, int k) override { ImPlot::PlotLineStaged(n, xs, ys, k); }
    virtual void PlotFloat(const char *n, float *xs, float *ys, int k) override { ImPlot::PlotLineStaged(n, xs, ys, k); }
    virtual void PlotDouble(const char *n, double *xs, double *ys, int k) override { ImPlot::PlotLineStaged(n, xs, ys, k); }
    virtual void PlotImVec2(const char *n, ImVec2 *vs, int k) override {}
    virtual void PlotImPlotPoint(const char *n, ImPlotPoint *vs, int k) override {}
};

struct BenchmarkRun
{
    int benchmark;
    int type;
    int elems;
    bool aa;
    std::string name;
};

typedef std::deque<BenchmarkRun> BenchmarkQueue;

struct ImPlotBench : App
{
    std::map<std::string, std::map<std::string, BenchmarkRecordCollection>> m_records;
    std::string m_branch;
    std::vector<std::unique_ptr<IBenchmark>> m_benchmarks;

    BenchmarkDataInt m_items_int;
    BenchmarkDataFloat m_items_float;
    BenchmarkDataDouble m_items_double;
    BenchmarkDataImVec2 m_items_imvec2;
    BenchmarkDataImPlotPoint m_items_implot;

    BenchmarkQueue m_queue;
    std::map<std::string, BenchmarkQueue> m_queues;

    ImPlotBench(int argc, char const *argv[]) : App("ImPlot Benchmark", 640, 480, argc, argv)
    {
    }

    ~ImPlotBench()
    {
        json j;
        j["records"] = m_records;
        std::ofstream file("benchmark.json");
        if (file.is_open())
            file << std::setw(4) << j;
    }

    void Start() override
    {
        std::ifstream file("benchmark.json");
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
            {0, BenchmarkType_Double, 2, true},
            {1, BenchmarkType_Double, 2, true},
            {2, BenchmarkType_Double, 2, true},
            {3, BenchmarkType_Double, 2, true},
        };

        m_queues["All Elems"] = {
            {0, BenchmarkType_Double, 0, true},
            {0, BenchmarkType_Double, 1, true},
            {0, BenchmarkType_Double, 2, true},
            {0, BenchmarkType_Double, 3, true},
        };

        m_queues["All Types"] = {
            {0, BenchmarkType_Int, 2, true},
            {0, BenchmarkType_Float, 2, true},
            {0, BenchmarkType_Double, 2, true},
            {0, BenchmarkType_ImVec2, 2, true},
            {0, BenchmarkType_ImPlotPoint, 2, true},
        };

        m_queues["Optimize"] = {
            {0, BenchmarkType_Double, 2, true},
            {4, BenchmarkType_Double, 2, true},
            {0, BenchmarkType_Double, 2, true},
            {4, BenchmarkType_Double, 2, true},
            {0, BenchmarkType_Double, 2, true},
            {4, BenchmarkType_Double, 2, true},
            {0, BenchmarkType_Double, 2, true},
            {4, BenchmarkType_Double, 2, true},
            {0, BenchmarkType_Double, 2, true},
            {4, BenchmarkType_Double, 2, true},
        };

        BenchmarkQueue everything;
        for (int b = 0; b < 4; ++b) {
            for (int t = 0; t < 5; ++t) {
                for (int e = 0; e < 4; ++e) {
                    everything.push_back({b,t,e});
                }
            }
        }
        m_queues["Everything"] = everything;
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
            if (ImGui::BeginTabItem("Compare"))
            {
                ShowCompareTool();
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
        ImGui::End();
    }

    void ShowBenchmarkTool()
    {
        static int selected_bench_idx = 0; // PlotLine
        static int selected_type_idx = BenchmarkType_Double;
        static int selected_elems_idx = 2; // 1000
        static bool selected_aa = true;

        auto GetRunName = [&]()
        {
            auto str = m_benchmarks[selected_bench_idx]->name + "_" + BenchmarkType_Names[selected_type_idx] + "_" + kElemsStrings[selected_elems_idx];
            if (!selected_aa)
                str += "_noaa";
            if (this->UsingDGPU)
                str += "_g";
            return str;
        };

        static std::string working_name = GetRunName();
        static BenchmarkRun working_run;
        static BenchmarkRecord working_record;
        static int working_elems = kElemValues[selected_elems_idx];
        static int working_items = kMaxElems / working_elems;
        static int working_add = working_items / kMaxSteps;
        static bool working_aa = true;

        static int current_items = 0;
        static int current_frame = 0;

        static int tcall = 0;
        static int tother = 0;
        static double t1 = 0;
        static double t2 = 0;
        static bool running = false;

        static std::chrono::high_resolution_clock::time_point run_t1;
        static std::chrono::high_resolution_clock::time_point run_t2;

        auto StartNextRun = [&]()
        {
            working_run = m_queue.front();
            m_queue.pop_front();
            selected_type_idx = working_run.type;
            selected_bench_idx = working_run.benchmark;
            selected_elems_idx = working_run.elems;
            selected_aa = working_run.aa;
            if (working_run.name.empty())
                working_run.name = GetRunName();
            working_name = working_run.name;
            working_record = BenchmarkRecord();
            working_elems = kElemValues[selected_elems_idx];
            working_items = kMaxElems / working_elems;
            working_add = working_items / kMaxSteps;
            working_aa = working_run.aa;
            t1 = ImGui::GetTime();
            current_items = current_frame = 0;
        };

        if (running)
        {
            current_frame++;
            if (current_frame == kMaxFrames)
            {
                t2 = ImGui::GetTime();
                working_record.AddRecord(current_items * working_elems, (tcall / kMaxFrames) * 0.001f, 1000.0 * (t2 - t1) / kMaxFrames, kMaxFrames / (t2 - t1));
                current_items += working_add;
                current_frame = 0;
                tcall = 0;
                t1 = ImGui::GetTime();
            }
            if (current_items > working_items)
            {
                working_record.FitData();
                m_records[m_branch][working_name].Records.push_back(working_record);
                m_records[m_branch][working_name].SetColorFromString(working_name);
                if (m_queue.size() > 0) {
                    StartNextRun();
                }
                else {
                    running = false;
                    run_t2 = Clock::now();
                    size_t run_span = (size_t)std::chrono::duration_cast<std::chrono::microseconds>(run_t2 - run_t1).count();
                    printf("Run completed in %u us (%.3f s)\n", run_span, run_span / 1000000.0f);
                }
            }
        }

        bool was_running = running;
        if (was_running)
        {
            ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.25f);
        }
        ImGui::SetNextItemWidth(100);
        if (ImGui::BeginCombo("##Benchmark", m_benchmarks[selected_bench_idx]->name.c_str()))
        {
            for (int i = 0; i < m_benchmarks.size(); ++i)
            {
                if (ImGui::Selectable(m_benchmarks[i]->name.c_str(), i == selected_bench_idx))
                {
                    selected_bench_idx = i;
                    working_name = GetRunName();
                }
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(75);
        if (ImGui::Combo("##Type", &selected_type_idx, BenchmarkType_Names, BenchmarkType_COUNT))
            working_name = GetRunName();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(75);
        if (ImGui::Combo("##Elems", &selected_elems_idx, kElemsStrings, 4))
            working_name = GetRunName();
        ImGui::SameLine();
        if (ImGui::Checkbox("AA",&selected_aa))
            working_name = GetRunName();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(125);
        ImGui::InputText("##Name", &working_name);
        if (was_running)
        {
            ImGui::PopItemFlag();
            ImGui::PopStyleVar();
        }
        ImGui::SameLine();
        if (ImGui::Button("+"))
        {
            m_queue.push_back({selected_bench_idx, selected_type_idx, selected_elems_idx, selected_aa, working_name});
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
        sprintf(start_str, running ? "Stop (%u)" : "Start (%u)", m_queue.size());
        if (ImGui::Button(start_str, ImVec2(-1, 0)))
        {
            running = !running;
            current_items = current_frame = 0;
            if (running)
            {
                if (m_queue.size() == 0)
                    m_queue.push_back({selected_bench_idx, selected_type_idx, selected_elems_idx, selected_aa, working_name});
                StartNextRun();
                run_t1 = Clock::now();
            }
            else
            {
                m_queue.clear();
            }
        }
        // ImGui::SameLine();
        // if (ImGui::Button("X", ImVec2(-1,0)))
        //     m_queue.clear();
        ImGui::ProgressBar((float)current_items / (float)(working_items - 1));

        GImGui->Style.AntiAliasedLines = GImGui->Style.AntiAliasedLinesUseTex = working_aa;

        if (ImPlot::BeginPlot("##Bench", ImVec2(-1, -1), ImPlotFlags_NoChild | ImPlotFlags_CanvasOnly))
        {
            ImPlot::SetupAxesLimits(0, kMaxElemsItem, kMaxElemsItem-kDataNoise, kMaxElems+kDataNoise, ImGuiCond_Always);
            ImPlot::SetupAxes(NULL, NULL, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_NoDecorations);
            if (running)
            {
                switch (selected_type_idx)
                {
                case BenchmarkType_Int:
                    m_benchmarks[selected_bench_idx]->RunInt(m_items_int, current_items, working_elems, tcall);
                    break;
                case BenchmarkType_Float:
                    m_benchmarks[selected_bench_idx]->RunFloat(m_items_float, current_items, working_elems, tcall);
                    break;
                case BenchmarkType_Double:
                    m_benchmarks[selected_bench_idx]->RunDouble(m_items_double, current_items, working_elems, tcall);
                    break;
                case BenchmarkType_ImVec2:
                    m_benchmarks[selected_bench_idx]->RunImVec2(m_items_imvec2, current_items, working_elems, tcall);
                    break;
                case BenchmarkType_ImPlotPoint:
                    m_benchmarks[selected_bench_idx]->RunImPlotPoint(m_items_implot, current_items, working_elems, tcall);
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
        ImGui::Checkbox("Call", &show_call_time);
        ImGui::SameLine();
        ImGui::Checkbox("Frame", &show_frame_time);
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

        auto fmtr = [](double value, char *buff, int size, void *user_data)
        {
            return snprintf(buff, size, "%.0fk", value / 1000);
        };

        if (ImPlot::BeginPlot("##Stats", ImVec2(-1, -1), ImPlotFlags_NoChild))
        {
            ImPlot::SetupAxis(ImAxis_X1, "Elements");
            ImPlot::SetupAxis(ImAxis_Y1, "Time [ms]");
            ImPlot::SetupAxesLimits(0, kMaxElems, 0, 50);
            ImPlot::SetupAxisFormat(ImAxis_X1, fmtr);
            ImPlot::SetupAxisLimits(ImAxis_Y2, 0, 500, ImPlotCond_Once);
            ImPlot::SetupLegend(ImPlotLocation_East, ImPlotLegendFlags_Outside);
            if (show_fps)
                ImPlot::SetupAxis(ImAxis_Y2, "FPS [Hz]", ImPlotAxisFlags_Opposite);

            for (auto &collection : m_records[selected_branch])
            {
                auto name = collection.first.c_str();
                auto col = collection.second.Col;
                ImPlot::PushStyleColor(ImPlotCol_Line, col);
                ImPlot::PushStyleColor(ImPlotCol_MarkerFill, col);

                for (auto &record : collection.second.Records)
                {
                    if (record.Elems.size() > 1)
                    {

                        ImPlot::SetAxis(ImAxis_Y1);
                        if (show_call_time)
                        {
                            if (show_fit)
                            {
                                ImPlot::PlotLine(name, &record.Call_Fit[0].x, &record.Call_Fit[0].y, 2, 0, 0, sizeof(ImVec2));
                            }
                            if (show_data)
                            {
                                ImPlot::SetNextMarkerStyle(ImPlotMarker_Square, 3);
                                ImPlot::PlotScatter(name, record.Elems.data(), record.Call.data(), record.Elems.size());
                            }
                        }
                        if (show_frame_time)
                        {
                            if (show_fit)
                            {
                                ImPlot::SetNextLineStyle(IMPLOT_AUTO_COL, 2.0f);
                                ImPlot::PlotLine(name, &record.Frame_Fit[0].x, &record.Frame_Fit[0].y, 2, 0, 0, sizeof(ImVec2));
                            }
                            if (show_data)
                            {
                                ImPlot::SetNextMarkerStyle(ImPlotMarker_Square, 3);
                                ImPlot::PlotScatter(name, record.Elems.data(), record.Frame.data(), record.Elems.size());
                            }
                        }
                        if (show_fps)
                        {
                            ImPlot::SetAxis(ImAxis_Y2);
                            ImPlot::PlotLine(name, record.Elems.data(), record.Fps.data(), record.Elems.size());
                            static double sixty = 60;
                            ImPlot::DragLineY(0, &sixty, ImVec4(1,1,0,1), 1, ImPlotDragToolFlags_NoInputs);
                            ImPlot::TagY(sixty, ImVec4(1,1,0,1), "60");
                        }
                    }
                }
                ImPlot::PopStyleColor(2);
            }
            ImPlot::EndPlot();
        }
    }

    void ShowCompareTool()
    {
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

        for (auto &bench : m_records[branchL])
        {
            if (m_records[branchR].count(bench.first))
                bench_union.push_back(bench.first);
        }

        int numPlots = bench_union.size();
        if (numPlots == 0)
            return;

        static std::vector<float> bar;
        static std::vector<const char *> labels;
        static const char *branchLabels[2];
        static double positions[2] = {0, 1};
        bar.resize(numPlots * 2);
        labels.resize(numPlots);

        branchLabels[0] = branchL.c_str();
        branchLabels[1] = branchR.c_str();

        for (int i = 0; i < numPlots * 2; i += 2)
        {
            auto &bench = bench_union[i / 2];
            bar[i] = ImSum(m_records[branchL][bench].Records[0].Call.data(), m_records[branchL][bench].Records[0].Call.size());
            bar[i + 1] = ImSum(m_records[branchR][bench].Records[0].Call.data(), m_records[branchR][bench].Records[0].Call.size());
            labels[i / 2] = bench.c_str();
        }

        int groups = 2;

        if (ImPlot::BeginPlot("##Comp", ImVec2(-1, -1)))
        {
            ImPlot::SetupAxis(ImAxis_X1, "##Bars", ImPlotAxisFlags_NoGridLines | ImPlotAxisFlags_NoTickMarks);
            ImPlot::SetupAxisTicks(ImAxis_X1, 0, 1, 2, branchLabels);
            ImPlot::SetupAxis(ImAxis_Y1, "Total Time [ms]", ImPlotAxisFlags_AutoFit);
            ImPlot::PlotBarGroups(labels.data(), bar.data(), numPlots, 2);
            ImPlot::EndPlot();
        }

        // int rows = IM_ROUND(ImSqrt(numPlots));
        // int cols = (numPlots / rows) * rows >= numPlots ? (numPlots / rows) : (numPlots / rows) + 1;
        // if (ImPlot::BeginSubplots("##Comp",rows,cols,ImVec2(-1,-1),ImPlotSubplotFlags_NoResize)) {
        //     for (auto& bench : bench_union) {
        //         if (ImPlot::BeginPlot(bench.c_str(), ImVec2(0,0), ImPlotFlags_NoLegend)) {
        //             ImPlot::SetupAxis(ImAxis_X1, "##X", ImPlotAxisFlags_NoTickLabels|ImPlotAxisFlags_NoGridLines|ImPlotAxisFlags_NoTickMarks);
        //             ImPlot::SetupAxisLimits(ImAxis_X1,-0.5,1.5f,ImPlotCond_Always);
        //             ImPlot::SetupAxis(ImAxis_Y1, "Total Time [ms]",ImPlotAxisFlags_AutoFit);
        //             float v[2];
        //             v[0] = ImSum(m_records[branchL][bench].Records[0].Call.data(), m_records[branchL][bench].Records[0].Call.size());
        //             v[1] = ImSum(m_records[branchR][bench].Records[0].Call.data(), m_records[branchR][bench].Records[0].Call.size());
        //             ImPlot::SetNextFillStyle(m_records[branchL][bench].Col);
        //             ImPlot::PlotBars("##Item",v,2);
        //             ImPlot::EndPlot();
        //         }
        //     }
        //     ImPlot::EndSubplots();
        // }
    }
};

typedef int ImPlotSpec;
enum ImPlotSpec_ {
    ImPlotSpec_LineWeight,
    ImPlotSpec_LineColor,
    ImPlotSpec_FaceColor,
    ImPlotSpec_EdgeColor,
    ImPlotSpec_Marker,
    ImPlotSpec_MarkerSize,
    ImPlotSpec_MarkerFaceColor,
    ImPlotSpec_MarkerEdgeColor,
    ImPlotSpec_Stride,
    ImPlotSpec_Offset,
};

struct ImPlotSpecData {
    float LineWeight = 1.0f;
    ImVec4 LineColor = IMPLOT_AUTO_COL;
    ImVec4 FaceColor = IMPLOT_AUTO_COL;
    ImVec4 EdgeColor = IMPLOT_AUTO_COL;
    ImPlotMarker Marker = ImPlotMarker_None;
    float MarkerSize = 4.0f;
    ImVec4 MarkerFaceColor = IMPLOT_AUTO_COL;
    ImVec4 MarkerEdgeColor = IMPLOT_AUTO_COL;
    int    Stride = 1;
    int    Offset = 0;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ImPlotSpecData, LineWeight, LineColor, FaceColor, EdgeColor, Marker, MarkerSize, MarkerFaceColor, MarkerEdgeColor, Stride, Offset);

constexpr unsigned int str2int(const char* str, int h = 0)
{
    return !str[h] ? 5381 : (str2int(str, h+1) * 33) ^ str[h];
}

constexpr void BuildSpec(ImPlotSpecData& S, ImPlotSpec spec, const float& v) {
    switch (spec) {
        case ImPlotSpec_LineWeight : S.LineWeight = v; return;
        case ImPlotSpec_MarkerSize : S.MarkerSize = v; return;
    }
}

constexpr void BuildSpec(ImPlotSpecData& S, const char* spec, const float& v) {
    switch (str2int(spec)) {
        case str2int("LineWeight") : S.LineWeight = v; return;
        case str2int("MarkerSize") : S.MarkerSize = v; return;
    }
}

constexpr void BuildSpec(ImPlotSpecData& S, ImPlotSpec spec, const ImPlotMarker_& v) {
    static_assert(true, "Out of Spec!");
    switch (spec) {
        case ImPlotSpec_Marker : S.LineWeight = v; return;
    }
}

constexpr void BuildSpec(ImPlotSpecData& S, const char* spec, const ImPlotMarker_& v) {
    switch (str2int(spec)) {
        case str2int("Marker") : S.LineWeight = v; return;
    }
}

constexpr void BuildSpec(ImPlotSpecData& S, ImPlotSpec spec, const int& v) {
    switch (spec) {
        case ImPlotSpec_LineWeight : S.LineWeight = v; return;
        case ImPlotSpec_Stride     : S.Stride     = v; return;
        case ImPlotSpec_Offset     : S.Offset     = v; return;
        case ImPlotSpec_Marker     : S.Marker     = v; return;
    }
}

constexpr void BuildSpec(ImPlotSpecData& S, const char* spec, const int& v) {
    switch (str2int(spec)) {
        case str2int("LineWeight") : S.LineWeight = v; return;
        case str2int("Stride")     : S.Stride     = v; return;
        case str2int("Offset")     : S.Offset     = v; return;
    }
}

constexpr void BuildSpec(ImPlotSpecData& S, ImPlotSpec spec, const ImVec4& v) {
    switch (spec) {
        case ImPlotSpec_LineColor       : S.LineColor = v;         return;
        case ImPlotSpec_FaceColor       : S.FaceColor = v;         return;
        case ImPlotSpec_EdgeColor       : S.EdgeColor = v;         return;
        case ImPlotSpec_MarkerFaceColor : S.MarkerFaceColor = v;   return;
        case ImPlotSpec_MarkerEdgeColor : S.MarkerEdgeColor = v;   return;
    }
}

constexpr void BuildSpec(ImPlotSpecData& S, const char* spec, const ImVec4& v) {
    switch (str2int(spec)) {
        case str2int("LineColor")       : S.LineColor = v;        return;
        case str2int("FaceColor")       : S.FaceColor = v;        return;
        case str2int("EdgeColor")       : S.EdgeColor = v;        return;
        case str2int("MarkerFaceColor") : S.MarkerFaceColor = v;  return;
        case str2int("MarkerEdgeColor") : S.MarkerEdgeColor = v;  return;
    }
}

template <typename Arg, typename ...Args>
constexpr void BuildSpec(ImPlotSpecData& S, ImPlotSpec spec, const Arg& arg, Args... args) {
    BuildSpec(S, spec, arg);
    BuildSpec(S, args...);
}

template <typename Arg, typename ...Args>
constexpr void BuildSpec(ImPlotSpecData& S, const char* spec, const Arg& arg, Args... args) {
    BuildSpec(S, spec, arg);
    BuildSpec(S, args...);
}

void PlotLine(const char* label, float* xs, float *ys, int count, const ImPlotSpecData& S) {
    json j = S;
    std::cout << std::setw(4) << j << std::endl;
}

template <typename ...Args>
void PlotLine(const char* label, float* xs, float *ys, int count, Args... args) {
    ImPlotSpecData S;
    BuildSpec(S, args...);
    PlotLine(label, xs, ys, count, S);
}

int main(int argc, char const *argv[])
{
    ImPlotBench app(argc, argv);
    app.Run();
    // PlotLine("MyLine", nullptr, nullptr, 10, "LineColor", ImVec4(1,0,0,1), "Marker", ImPlotMarker_Cross, "MarkerFaceColor", ImVec4(1,1,0,1), "MarkerSize", 5.0f);
    return 0;
}
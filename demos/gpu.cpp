// Demo:   gpu.cpp
// Author: Evan Pezent (evanpezent.com)
// Date:   6/26/2021

#define IMGUI_DEFINE_MATH_OPERATORS
#define APP_USE_DGPU

// #include <filesystem>
#include "App.h"
#include "Shader.h"
#include <implot_internal.h>
#include <vector>
#include <json.hpp>
#include <iomanip>
#include <immintrin.h>
#include <imgui_stdlib.h>
#include <string>
#include <chrono>

#define RESTART_IDX 4294967295

#if defined __SSE__ || defined __x86_64__ || defined _M_X64
static inline float ImInvSqrt(float x) { return _mm_cvtss_f32(_mm_rsqrt_ss(_mm_set_ss(x))); }
#else
static inline float ImInvSqrt(float x) { return 1.0f / sqrtf(x); }
#endif

#define IMPLOT_NORMALIZE2F_OVER_ZERO(VX, VY) \
    do                                       \
    {                                        \
        float d2 = VX * VX + VY * VY;        \
        if (d2 > 0.0f)                       \
        {                                    \
            float inv_len = ImInvSqrt(d2);   \
            VX *= inv_len;                   \
            VY *= inv_len;                   \
        }                                    \
    } while (0)

namespace ImPlot
{

    struct ImGuiOpenGLData
    {
        bool Captured = false;
        GLuint ShaderHandle = 0;
        GLuint AttribLocationTex = 0;
        GLuint AttribLocationProjMtx = 0;
        GLuint AttribLocationVtxPos = 0;
        GLuint AttribLocationVtxUV = 0;
        GLuint AttribLocationVtxColor = 0;
    };

    struct Point
    {
        ImVec2 Position;
        ImU32 Color;
    };

    struct PlotTransform
    {

        PlotTransform() {}
        PlotTransform(ImPlotPlot *plot)
        {
            ImAxis x = plot->CurrentX;
            ImAxis y = plot->CurrentY;
            M.x = plot->Axes[x].ScaleToPixel;
            M.y = plot->Axes[y].ScaleToPixel;
            PltMin.x = (float)plot->Axes[x].Range.Min;
            PltMin.y = (float)plot->Axes[y].Range.Min;
            PixMin.x = plot->Axes[x].PixelMin;
            PixMin.y = plot->Axes[y].PixelMin;
        }

        ImVec2 M;
        ImVec2 PltMin;
        ImVec2 PixMin;
    };

    struct GpuContext
    {

        GpuContext()
        {
            glGenVertexArrays(1, &VAO);
            glGenBuffers(1, &VBO);
            glGenBuffers(1, &EBO);
            glBindVertexArray(VAO);
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Point), (GLvoid *)IM_OFFSETOF(Point, Position));
            glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Point), (GLvoid *)IM_OFFSETOF(Point, Color));
            glEnableVertexAttribArray(0);
            glEnableVertexAttribArray(1);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindVertexArray(0);
            LineShader.LoadFromFile("shaders/plot_point.vert",
                                    "shaders/color_pass.frag",
                                    "shaders/line_fringe_4.geom");
            // LineShader.LoadFromFile("shaders/plot_point.vert",
            //                     "shaders/color_uniform.frag");
        }

        void AppendData(const ImVec2 *points, int size, ImU32 col)
        {
            auto add_size = size;
            auto cur_size = Points.size();
            auto new_size = add_size + cur_size;
            Points.resize(new_size + 1);
            Indices.resize(new_size + 1);
            // this could be faster with avx?
            for (int i = 0; i < add_size; ++i)
            {
                int j = cur_size + i;
                Points[j].Position.x = points[i].x;
                Points[j].Position.y = points[i].y;
                Points[j].Color = col;
                Indices[j] = j;
            }
            Points[new_size].Position = ImVec2(0, 0);
            Points[new_size].Color = 0;
            Indices[new_size] = RESTART_IDX;
        }

        // ImPlotPlot* CurrentPlot = NULL;
        PlotTransform Transform;
        ImGuiOpenGLData ImGuiData;
        Shader LineShader;
        std::vector<Point> Points;
        std::vector<ImDrawIdx> Indices;
        unsigned int VBO;
        unsigned int VAO;
        unsigned int EBO;
    };

    GpuContext *GPU;

    void CreateGpuContext()
    {
        GPU = new GpuContext;
    }

    void DestroyGpuContext()
    {
        delete GPU;
        GPU = NULL;
    }

    static void CaptureImGuiDataCallback(const ImDrawList *, const ImDrawCmd *cmd)
    {
        glGetIntegerv(GL_CURRENT_PROGRAM, (GLint *)&GPU->ImGuiData.ShaderHandle);
        GPU->ImGuiData.AttribLocationTex = glGetUniformLocation(GPU->ImGuiData.ShaderHandle, "Texture");
        GPU->ImGuiData.AttribLocationProjMtx = glGetUniformLocation(GPU->ImGuiData.ShaderHandle, "ProjMtx");
        GPU->ImGuiData.AttribLocationVtxPos = glGetAttribLocation(GPU->ImGuiData.ShaderHandle, "Position");
        GPU->ImGuiData.AttribLocationVtxUV = glGetAttribLocation(GPU->ImGuiData.ShaderHandle, "UV");
        GPU->ImGuiData.AttribLocationVtxColor = glGetAttribLocation(GPU->ImGuiData.ShaderHandle, "Color");
    }

    static void RenderCallback(const ImDrawList *, const ImDrawCmd *cmd)
    {
        if (GPU->Points.size() == 0)
            return;

        float ProjMtx[4][4];
        glGetUniformfv(GPU->ImGuiData.ShaderHandle, GPU->ImGuiData.AttribLocationProjMtx, &ProjMtx[0][0]);

        float w = ProjMtx[0][0];
        float h = -ProjMtx[1][1];

        GPU->LineShader.Use();
        GPU->LineShader.SetMat4("ProjMtx", &ProjMtx[0][0]);
        GPU->LineShader.SetFloat2("Scale", ImVec2(w, h));
        GPU->LineShader.SetFloat2("M", GPU->Transform.M);
        GPU->LineShader.SetFloat2("PltMin", GPU->Transform.PltMin);
        GPU->LineShader.SetFloat2("PixMin", GPU->Transform.PixMin);

        glEnable(GL_PRIMITIVE_RESTART);
        glPrimitiveRestartIndex(RESTART_IDX);
        glBindBuffer(GL_ARRAY_BUFFER, GPU->VBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, GPU->EBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(Point) * GPU->Points.size(), GPU->Points.data(), GL_STREAM_DRAW);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(ImDrawIdx) * GPU->Indices.size(), &GPU->Indices[0], GL_STREAM_DRAW);
        glBindVertexArray(GPU->VAO);
        glDrawElements(GL_LINE_STRIP, GPU->Indices.size(), sizeof(ImDrawIdx) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, (GLvoid *)0);
        GPU->Points.clear();
    }

    void PlotLineGPU(const char *label_id, const ImVec2 *points, int count)
    {
        if (BeginItem(label_id, ImPlotCol_Line))
        {
            if (FitThisFrame())
            {
                for (int i = 0; i < count; ++i)
                {
                    FitPoint(ImPlotPoint(points[i].x, points[i].y));
                }
            }
            const ImPlotNextItemData &s = GetItemData();
            ImU32 col = ImGui::GetColorU32(s.Colors[ImPlotCol_Line]);
            GPU->Transform = PlotTransform(ImPlot::GetCurrentPlot());
            GPU->AppendData(points, count, col);
            ImDrawList &DrawList = *GetPlotDrawList();
            if (!GPU->ImGuiData.Captured)
            {
                DrawList.AddCallback(CaptureImGuiDataCallback, nullptr);
                GPU->ImGuiData.Captured = true;
            }
            DrawList.AddDrawCmd(); // to get scissor rect
            DrawList.AddCallback(RenderCallback, nullptr);
            DrawList.AddCallback(ImDrawCallback_ResetRenderState, nullptr);
            ImPlot::EndItem();
        }
    }

    void PlotLineMinimal(const char *label_id, const ImVec2 *values, int count)
    {
        static ImVector<float> xs;
        static ImVector<float> ys;

        xs.resize(count);
        ys.resize(count);

        for (int i = 0; i < count; ++i) {
            xs[i] = values[i].x;
            ys[i] = values[i].y;
        }

        ImPlotContext &gp = *GImPlot;
        if (BeginItem(label_id, ImPlotCol_Line))
        {
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

                const float minx_pix = gp.CurrentPlot->Axes[ImAxis_X1].PixelMin;
                const float miny_pix = gp.CurrentPlot->Axes[ImAxis_Y1].PixelMin;
                const float minx_plt = gp.CurrentPlot->Axes[ImAxis_X1].Range.Min;
                const float miny_plt = gp.CurrentPlot->Axes[ImAxis_Y1].Range.Min;
                const float mx       = gp.CurrentPlot->Axes[ImAxis_X1].ScaleToPixel;
                const float my       = gp.CurrentPlot->Axes[ImAxis_Y1].ScaleToPixel;

                ImRect cull_rect = gp.CurrentPlot->PlotRect;
                ImPlotPoint plt = ImPlotPoint(xs[0], ys[0]);
                ImVec2 P1 = ImVec2(minx_pix + mx * ((float)plt.x - minx_plt),
                                   miny_pix + my * ((float)plt.y - miny_plt));

                DrawList.PrimReserve(prims * 6, prims * 4);
                for (unsigned int idx = 0; idx < prims; ++idx)
                {
                    plt = ImPlotPoint(xs[idx + 1], ys[idx + 1]);
                    ImVec2 P2 = ImVec2(minx_pix + mx * ((float)plt.x - minx_plt),
                                       miny_pix + my * ((float)plt.y - miny_plt));
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
                    P1 = P2;
                }
            }
            EndItem();
        }
    }
} // namespace ImPlot

using Clock = std::chrono::high_resolution_clock;

struct ScopedProfiler
{
    ScopedProfiler(size_t &_val) : val(_val)
    {
        t1 = Clock::now();
    }
    ~ScopedProfiler()
    {
        auto t2 = Clock::now();
        val += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
    }
    std::chrono::high_resolution_clock::time_point t1;
    size_t &val;
};

enum BenchMode
{
    BenchMode_Line = 0,
    BenchMode_LineMinimal,
    BenchMode_LineGPU,
    BenchMode_COUNT
};

static const char *BenchMode_Names[] = {"Line", "LineG", "LineMinimal", "LineGPU"};

struct BenchRecord
{
    std::string name;
    int mode;
    std::vector<float> items;
    std::vector<float> fps;
    std::vector<float> call;
    std::vector<float> loop;
};

using json = nlohmann::json;

void to_json(json &j, const BenchRecord &p)
{
    j = json{{"name", p.name}, {"mode", p.mode}, {"items", p.items}, {"fps", p.fps}, {"call", p.call}, {"loop", p.loop}};
}

void from_json(const json &j, BenchRecord &p)
{
    j.at("name").get_to(p.name);
    j.at("mode").get_to(p.mode);
    j.at("items").get_to(p.items);
    j.at("fps").get_to(p.fps);
    j.at("call").get_to(p.call);
    j.at("loop").get_to(p.loop);
}

struct BenchData
{
    BenchData()
    {
        float y = RandomRange(0.0f, 1.0f);
        Data = new ImVec2[1000];
        for (int i = 0; i < 1000; ++i)
        {
            Data[i].x = i;
            Data[i].y = y + RandomRange(-0.01f, 0.01f);
        }
        Col = ImVec4(RandomRange(0.0f, 1.0f), RandomRange(0.0f, 1.0f), RandomRange(0.0f, 1.0f), 0.5f);
    }
    ~BenchData() { delete[] Data; }
    ImVec2 *Data;
    ImVec4 Col;
};

bool GetBranchName(const std::string &git_repo_path, std::string &branch_name)
{
    // IM_ASSERT(git_repo_path != NULL);
    // IM_ASSERT(branch_name != NULL);
    std::string head_path = git_repo_path + "/.git/HEAD";
    size_t head_size = 0;
    bool result = false;
    if (char *git_head = (char *)ImFileLoadToMemory(head_path.c_str(), "r", &head_size, 1))
    {
        const char prefix[] = "ref: refs/heads/"; // Branch name is prefixed with this in HEAD file.
        const int prefix_length = IM_ARRAYSIZE(prefix) - 1;
        strtok(git_head, "\r\n"); // Trim new line
        if (head_size > prefix_length && strncmp(git_head, prefix, prefix_length) == 0)
        {
            branch_name = (git_head + prefix_length);
            result = true;
        }
        IM_FREE(git_head);
    }
    return result;
}

struct ImPlotBench : App
{

    using App::App;

    std::vector<BenchRecord> records;
    std::string branch;

    ~ImPlotBench()
    {
        ImPlot::DestroyGpuContext();
        json j;
        j["records"] = records;
        std::ofstream file("bench.json");
        if (file.is_open())
            file << std::setw(4) << j;
    }

    void Start() override
    {
        // if (fs::exists("bench.json")) {
        std::ifstream file("bench.json");
        if (file.is_open())
        {
            json j;
            file >> j;
            j.at("records").get_to(records);
        }
        // }

        GetBranchName("C:/git/implot/implot", branch);
        glEnable(GL_MULTISAMPLE);
        ImPlot::CreateGpuContext();
    }

    void Update() override
    {
        ImGui::SetNextWindowSize(GetWindowSize(), ImGuiCond_Always);
        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
        ImGui::Begin("ImPlot Benchmark Tool", 0, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);
        if (ImGui::BeginTabBar("Tabs"))
        {
            if (ImGui::BeginTabItem("Bench"))
            {
                ShowBenchmarkTool();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Results"))
            {
                ShowResultsTool();
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
        ImGui::End();
    }

    static constexpr int max_items = 500;
    BenchData items[max_items];

    void ShowBenchmarkPlotOnly(int mode)
    {
        size_t call_time = 0;
        ImGui::Text("%.2f FPS", ImGui::GetIO().Framerate);
        if (ImPlot::BeginPlot("##Bench", NULL, NULL, ImVec2(-1, -1), ImPlotFlags_NoChild | ImPlotFlags_CanvasOnly, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_NoDecorations))
        {
            ImPlot::SetupAxesLimits(0, 1000, 0, 1, ImGuiCond_Always);
            PlotBenchmarkItems(mode, max_items, call_time);
            ImPlot::EndPlot();
        }
    }

    void ShowBenchmarkTool()
    {
        static bool running = false;
        static int frames = 60;
        static int L = 0;
        static int F = 0;
        static double t1, t2;
        static int mode = BenchMode_Line;
        static size_t call_time = 0;

        auto get_run_name = [&]()
        {
            auto str = branch + "_" + std::string(BenchMode_Names[mode]);
            return str;
        };

        static std::string run_name = get_run_name();

        if (running)
        {
            F++;
            if (F == frames)
            {
                t2 = ImGui::GetTime();
                records.back().items.push_back(L);
                records.back().fps.push_back(frames / (t2 - t1));
                records.back().call.push_back((call_time / frames) * 0.001f);
                records.back().loop.push_back(1000.0 * (t2 - t1) / frames);
                L += 10;
                F = 0;
                t1 = ImGui::GetTime();
                call_time = 0;
            }
            if (L > max_items)
            {
                running = false;
                L = 0;
            }
        }

        // ImGui::Text("ImDrawIdx: %d-bit", (int)(sizeof(ImDrawIdx) * 8));
        // ImGui::Text("ImGuiBackendFlags_RendererHasVtxOffset: %s", (ImGui::GetIO().BackendFlags & ImGuiBackendFlags_RendererHasVtxOffset) ? "True" : "False");
        // ImGui::Text("%.2f FPS", ImGui::GetIO().Framerate);
        // ImGui::Separator();

        bool was_running = running;
        if (was_running)
        {
            ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.25f);
        }

        if (ImGui::Button("Benchmark"))
        {
            running = true;
            L = F = 0;
            records.push_back(BenchRecord());
            records.back().items.reserve(max_items + 1);
            records.back().fps.reserve(max_items + 1);
            records.back().call.reserve(max_items + 1);
            records.back().loop.reserve(max_items + 1);
            records.back().mode = mode;
            records.back().name = run_name;
            t1 = ImGui::GetTime();
        }
        ImGui::SameLine();
        ImGui::SameLine();
        if (ImGui::Button("Warm"))
        {
            L = L > 0 ? 0 : max_items;
            call_time = 0;
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100);
        if (ImGui::Combo("##Mode", &mode, BenchMode_Names, BenchMode_COUNT))
            run_name = get_run_name();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##name", &run_name);
        if (was_running)
        {
            ImGui::PopItemFlag();
            ImGui::PopStyleVar();
        }
        ImGui::ProgressBar((float)L / (float)(max_items - 1));

        if (ImPlot::BeginPlot("##Bench", ImVec2(-1, -1), ImPlotFlags_NoChild | ImPlotFlags_CanvasOnly))
        {
            ImPlot::SetupAxesLimits(0, 1000, 0, 1, ImGuiCond_Always);
            ImPlot::SetupAxes(NULL, NULL, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_NoDecorations);
            PlotBenchmarkItems(mode, L, call_time);
            ImPlot::EndPlot();
        }
    }

    void ShowResultsTool()
    {

        static bool show_loop_time = true;
        static bool show_call_time = true;
        static bool show_fps = false;

        if (ImGui::Button("Clear"))
            records.clear();
        ImGui::SameLine();
        ImGui::Checkbox("Frame Time", &show_loop_time);
        ImGui::SameLine();
        ImGui::Checkbox("Call Time", &show_call_time);
        ImGui::SameLine();
        ImGui::Checkbox("FPS", &show_fps);

        // ImPlot::SetNextPlotLimitsY(0,15,2,ImPlotYAxis_2);
        int flags = ImPlotFlags_NoChild;
        if (show_fps)
            flags = flags | ImPlotFlags_YAxis2;

        static std::vector<int> to_delete;
        to_delete.clear();

        if (ImPlot::BeginPlot("##Stats", "Items (1,000 pts each)", "Time (ms)", ImVec2(-1, -1), flags, 0, 0, 2, 2, "FPS (Hz)"))
        {
            ImPlot::SetupAxesLimits(0, max_items, 0, 50);
            ImPlot::SetupAxisLimits(ImAxis_Y2, 0, 500, ImPlotCond_Once);
            for (int run = 0; run < records.size(); ++run)
            {
                if (records[run].items.size() > 1)
                {
                    ImPlot::SetAxis(ImAxis_Y1);
                    if (show_loop_time)
                        ImPlot::PlotLine(records[run].name.c_str(), records[run].items.data(), records[run].loop.data(), records[run].items.size());
                    if (show_call_time)
                        ImPlot::PlotLine(records[run].name.c_str(), records[run].items.data(), records[run].call.data(), records[run].items.size());
                    if (show_fps)
                    {
                        ImPlot::SetAxis(ImAxis_Y2);
                        ImPlot::PlotLine(records[run].name.c_str(), records[run].items.data(), records[run].fps.data(), records[run].items.size());
                    }
                }
                if (ImPlot::BeginLegendPopup(records[run].name.c_str()))
                {
                    if (ImGui::Button("Delete"))
                    {
                        to_delete.push_back(run);
                    }
                    ImPlot::EndLegendPopup();
                }
            }
            ImPlot::EndPlot();
        }

        for (int i = to_delete.size(); i-- > 0;)
        {
            int idx = to_delete[i];
            records.erase(records.begin() + idx);
        }
    }

    void PlotBenchmarkItems(int mode, int L, size_t &call_time)
    {
        if (mode == BenchMode_Line)
        {
            for (int i = 0; i < L; ++i)
            {
                ImGui::PushID(i);
                ImPlot::SetNextLineStyle(items[i].Col);
                {
                    ScopedProfiler prof(call_time);
                    ImPlot::PlotLine("##item", &items[i].Data[0].x, &items[i].Data[0].y, 1000, 0, 0, sizeof(ImVec2));
                }
                ImGui::PopID();
            }
        }
        else if (mode == BenchMode_LineMinimal)
        {
            for (int i = 0; i < L; ++i)
            {
                ImGui::PushID(i);
                ImPlot::SetNextLineStyle(items[i].Col);
                {
                    ScopedProfiler prof(call_time);
                    ImPlot::PlotLineMinimal("##item", items[i].Data, 1000);
                }
                ImGui::PopID();
            }
        }
        else if (mode == BenchMode_LineGPU)
        {
            for (int i = 0; i < L; ++i)
            {
                ImGui::PushID(i);
                ImPlot::SetNextLineStyle(items[i].Col);
                {
                    ScopedProfiler prof(call_time);
                    ImPlot::PlotLineGPU("##item", items[i].Data, 1000);
                }
                ImGui::PopID();
            }
        }
    }
};

int main(int argc, char const *argv[])
{
    ImPlotBench app("ImPlot Benchmark", 640, 480, argc, argv);
    app.Run();
    return 0;
}

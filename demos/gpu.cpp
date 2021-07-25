// Demo:   gpu.cpp
// Author: Evan Pezent (evanpezent.com)
// Date:   6/26/2021

#define IMGUI_DEFINE_MATH_OPERATORS
#define APP_USE_DGPU

#include "App.h"
#include "Shader.h"
#include <implot_internal.h>
#include <vector>
#include <json.hpp>
#include <iomanip>

#ifdef _MSC_VER
#include <filesystem>
namespace fs = std::filesystem;
#else
#include <filesystem>
namespace fs = std::filesystem;
#endif

#define RESTART_IDX 4294967295

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
            int y = plot->CurrentYAxis;
            M.x = (float)GImPlot->Mx;
            M.y = (float)GImPlot->My[y];
            PltMin.x = (float)plot->XAxis.Range.Min;
            PltMin.y = (float)plot->YAxis[y].Range.Min;
            PixMin.x = GImPlot->PixelRange[y].Min.x;
            PixMin.y = GImPlot->PixelRange[y].Min.y;
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
            LineShader.LoadFromFile("../../resources/shaders/plot_point.vert",
                                    "../../resources/shaders/color_pass.frag",
                                    "../../resources/shaders/line_fringe_4.geom");
            // LineShader.LoadFromFile("../../resources/shaders/plot_point.vert",
            //                     "../../resources/shaders/color_uniform.frag");
        }

        void AppendData(const ImVec2 *points, int size, ImU32 col)
        {
            auto add_size = size;
            auto cur_size = Points.size();
            auto new_size = add_size + cur_size;
            Points.resize(new_size + 1);
            Indices.resize(new_size + 1);
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

    void GpuPlotLine(const char *label_id, const ImVec2 *points, int count)
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

} // namespace ImPlot

struct BenchRecordGpu
{
    std::string branch;
    int mode;
    bool GPU;
    std::vector<float> items;
    std::vector<float> fps;
};

using json = nlohmann::json;

void to_json(json& j, const BenchRecordGpu& p) {
    j = json{{"branch", p.branch}, {"mode", p.mode}, {"gpu", p.GPU}, {"items",p.items},{"fps",p.fps}};
}

void from_json(const json& j, BenchRecordGpu& p) {
    j.at("branch").get_to(p.branch);
    j.at("mode").get_to(p.mode);
    j.at("gpu").get_to(p.GPU);
    j.at("items").get_to(p.items);
    j.at("fps").get_to(p.fps);
}

struct BenchDataGpu
{
    BenchDataGpu()
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
    ~BenchDataGpu() { delete[] Data; }
    ImVec2 *Data;
    ImVec4 Col;
};

bool GetBranchName(const std::string& git_repo_path, std::string& branch_name)
{
    // IM_ASSERT(git_repo_path != NULL);
    // IM_ASSERT(branch_name != NULL);
    std::string head_path = git_repo_path + "/.git/HEAD";
    size_t head_size = 0;
    bool result = false;
    if (char* git_head = (char*)ImFileLoadToMemory(head_path.c_str(), "r", &head_size, 1))
    {
        const char prefix[] = "ref: refs/heads/";       // Branch name is prefixed with this in HEAD file.
        const int prefix_length = IM_ARRAYSIZE(prefix) - 1;
        strtok(git_head, "\r\n");                       // Trim new line
        if (head_size > prefix_length&& strncmp(git_head, prefix, prefix_length) == 0)
        {
            branch_name = (git_head + prefix_length);
            result = true;
        }
        IM_FREE(git_head);
    }
    return result;
}



struct ImPlotGpuDemo : App
{

    using App::App;

    std::vector<BenchRecordGpu> records;
    std::string branch;

    ~ImPlotGpuDemo()
    {
        ImPlot::DestroyGpuContext();
        json j;
        j["records"] = records;
        std::ofstream file("bench.json");
        if (file.is_open())
            file << std::setw(4) << j;
    }

    void init() override
    {
        if (fs::exists("bench.json")) {
            std::ifstream file("bench.json");
            json j;
            file >> j;
            j.at("records").get_to(records);
        }

        GetBranchName("C:/git/implot",branch);
        glEnable(GL_MULTISAMPLE);
        ImPlot::CreateGpuContext();
    }

    void update() override
    {
        ImGui::SetNextWindowSize(get_window_size(), ImGuiCond_Always);
        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
        ImGui::Begin("ImPlot Benchmark Tool",0,ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoTitleBar);
        ShowBenchmark();
        ImGui::End();
    }

    void ShowBenchmark()
    {
        static const int max_items = 500;
        static BenchDataGpu items[max_items];
        static bool running = false;
        static bool gpu = false;
        static int frames = 60;
        static int L = 0;
        static int F = 0;
        static double t1, t2;

        if (running)
        {
            F++;
            if (F == frames)
            {
                t2 = ImGui::GetTime();
                records.back().items.push_back(L);
                records.back().fps.push_back(frames / (t2 - t1));
                L += 5;
                F = 0;
                t1 = ImGui::GetTime();
            }
            if (L > max_items)
            {
                running = false;
                L = max_items;
            }
        }

        ImGui::Text("ImDrawIdx: %d-bit", (int)(sizeof(ImDrawIdx) * 8));
        ImGui::Text("ImGuiBackendFlags_RendererHasVtxOffset: %s", (ImGui::GetIO().BackendFlags & ImGuiBackendFlags_RendererHasVtxOffset) ? "True" : "False");
        ImGui::Text("%.2f FPS", ImGui::GetIO().Framerate);

        ImGui::Separator();

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
            records.push_back(BenchRecordGpu());
            records.back().items.reserve(max_items + 1);
            records.back().fps.reserve(max_items + 1);
            records.back().GPU = gpu;
            records.back().branch = branch;
            t1 = ImGui::GetTime();
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear")) {
            records.clear();
        }
        ImGui::SameLine();
        ImGui::Checkbox("GPU Acceleration", &gpu);
        if (was_running)
        {
            ImGui::PopItemFlag();
            ImGui::PopStyleVar();
        }

        ImGui::ProgressBar((float)L / (float)(max_items - 1));

        ImPlot::SetNextPlotLimits(0, 1000, 0, 1, ImGuiCond_Always);
        if (ImPlot::BeginPlot("##Bench", NULL, NULL, ImVec2(-1, 0), ImPlotFlags_NoChild | ImPlotFlags_CanvasOnly, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_NoDecorations))
        {
            if (running)
            {
                if (gpu)
                {
                    for (int i = 0; i < L; ++i)
                    {
                        ImGui::PushID(i);
                        ImPlot::SetNextLineStyle(items[i].Col);
                        // ImPlot::PlotLine("##item", &items[i].Data[0].x, &items[i].Data[0].y, 1000, 0, sizeof(ImVec2));
                        ImPlot::GpuPlotLine("##item", items[i].Data, 1000);
                        ImGui::PopID();
                    }
                }
                else
                {
                    for (int i = 0; i < L; ++i)
                    {
                        ImGui::PushID(i);
                        ImPlot::SetNextLineStyle(items[i].Col);
                        ImPlot::PlotLine("##item", &items[i].Data[0].x, &items[i].Data[0].y, 1000, 0, sizeof(ImVec2));
                        ImGui::PopID();
                    }
                }
            }
            ImPlot::EndPlot();
        }
        ImPlot::SetNextPlotLimits(0, 500, 0, 500);
        static char buffer[128];
        if (ImPlot::BeginPlot("##Stats", "Items (1,000 pts each)", "Framerate (Hz)", ImVec2(-1, -1), ImPlotFlags_NoChild))
        {
            for (int run = 0; run < records.size(); ++run)
            {
                if (records[run].items.size() > 1)
                {
                    sprintf(buffer, "%d-%s",  run + 1, records[run].branch.c_str());
                    ImPlot::PlotLine(buffer, records[run].items.data(), records[run].fps.data(), records[run].items.size());
                }
            }
            ImPlot::EndPlot();
        }
    }
};

int main(int argc, char const *argv[])
{
    ImPlotGpuDemo demo(500, 750, "ImPlot Demo", false);
    demo.run();
    return 0;
}


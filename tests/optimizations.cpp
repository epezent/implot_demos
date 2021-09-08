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

// namespace fs = std::filesystem;

#define RESTART_IDX 4294967295

#if defined __SSE__ || defined __x86_64__ || defined _M_X64
static inline float  ImInvSqrt(float x)           { return _mm_cvtss_f32(_mm_rsqrt_ss(_mm_set_ss(x))); }
#else
static inline float  ImInvSqrt(float x)           { return 1.0f / sqrtf(x); }
#endif

#define IMPLOT_NORMALIZE2F_OVER_ZERO(VX,VY) do { float d2 = VX*VX + VY*VY; if (d2 > 0.0f) { float inv_len = ImInvSqrt(d2); VX *= inv_len; VY *= inv_len; } } while (0)


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
            int x = plot->CurrentX;
            int y = plot->CurrentY;
            M.x = (float)GImPlot->Mx[x];
            M.y = (float)GImPlot->My[y];
            PltMin.x = (float)plot->XAxis[x].Range.Min;
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

    void PlotLineGPU(const char *label_id, const ImVec2 *points, int count)  {
        if (BeginItem(label_id, ImPlotCol_Line)) {
            if (FitThisFrame()) {
                for (int i = 0; i < count; ++i) {
                    FitPoint(ImPoint(points[i].x, points[i].y));
                }
            }
            const ImPlotNextItemData &s = GetItemData();
            ImU32 col = ImGui::GetColorU32(s.Colors[ImPlotCol_Line]);
            GPU->Transform = PlotTransform(ImPlot::GetCurrentPlot());
            GPU->AppendData(points, count, col);
            ImDrawList &DrawList = *GetPlotDrawList();
            if (!GPU->ImGuiData.Captured) {
                DrawList.AddCallback(CaptureImGuiDataCallback, nullptr);
                GPU->ImGuiData.Captured = true;
            }
            DrawList.AddDrawCmd(); // to get scissor rect
            DrawList.AddCallback(RenderCallback, nullptr);
            DrawList.AddCallback(ImDrawCallback_ResetRenderState, nullptr);
            ImPlot::EndItem();
        }
    }


void PlotLineInline(const char *label_id, const ImVec2 *values, int count)
{
    ImPlotContext &gp = *GImPlot;
    if (BeginItem(label_id, ImPlotCol_Line))
    {
        if (FitThisFrame())
        {
            for (int i = 0; i < count; ++i)
            {
                FitPoint(ImPoint(values[i].x, values[i].y));
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
            const float minx_plt = (float)gp.CurrentPlot->XAxis[0].Range.Min;
            const float miny_plt = (float)gp.CurrentPlot->YAxis[0].Range.Min;
            const float mx = (float)gp.Mx[0];
            const float my = (float)gp.My[0];

            ImRect cull_rect = gp.CurrentPlot->PlotRect;
            ImPoint plt = ImPoint(values[0].x, values[0].y);
            ImVec2 P1 = ImVec2(minx_pix + mx * ((float)plt.x - minx_plt),
                               miny_pix + my * ((float)plt.y - miny_plt));
            
            DrawList.PrimReserve(prims * 6, prims * 4);
            for (unsigned int idx = 0; idx < prims; ++idx)
            {
                plt = ImPoint(values[idx+1].x, values[idx + 1].y);
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

void PlotLineNoTess(const char *label_id, const ImVec2 *values, int count)
{
    ImPlotContext &gp = *GImPlot;
    if (BeginItem(label_id, ImPlotCol_Line))
    {
        if (FitThisFrame())
        {
            for (int i = 0; i < count; ++i)
                FitPoint(ImPoint(values[i].x, values[i].y));
        }
        const ImPlotNextItemData &s = GetItemData();
        ImDrawList &DrawList = *GetPlotDrawList();
        if (count > 1 && s.RenderLine)
        {
            unsigned int prims = count - 1;
            const ImU32 col = ImGui::GetColorU32(s.Colors[ImPlotCol_Line]);
            const ImVec2 uv = DrawList._Data->TexUvWhitePixel;

            auto P = GImPlot->CurrentPlot->PlotRect.GetCenter();
                        
            DrawList.PrimReserve(prims * 6, prims * 4);
            for (unsigned int idx = 0; idx < prims; ++idx)
            {
                DrawList._VtxWritePtr[0].pos.x = P.x;  //[pos.x][pos.y][uv.xy][col] [pos.x][pos.y][uv][col] [pos.x][pos.y][uv][col]
                DrawList._VtxWritePtr[0].pos.y = P.y;
                DrawList._VtxWritePtr[1].pos.x = P.x;
                DrawList._VtxWritePtr[1].pos.y = P.y;
                DrawList._VtxWritePtr[2].pos.x = P.x;
                DrawList._VtxWritePtr[2].pos.y = P.y;
                DrawList._VtxWritePtr[3].pos.x = P.x;
                DrawList._VtxWritePtr[3].pos.y = P.y;

                DrawList._VtxWritePtr[0].uv = uv;
                DrawList._VtxWritePtr[1].uv = uv;
                DrawList._VtxWritePtr[2].uv = uv;
                DrawList._VtxWritePtr[3].uv = uv;

                DrawList._VtxWritePtr[0].col = col;
                DrawList._VtxWritePtr[1].col = col;
                DrawList._VtxWritePtr[2].col = col;
                DrawList._VtxWritePtr[3].col = col;
                
                DrawList._IdxWritePtr[0] = (ImDrawIdx)(DrawList._VtxCurrentIdx);
                DrawList._IdxWritePtr[1] = (ImDrawIdx)(DrawList._VtxCurrentIdx + 1);
                DrawList._IdxWritePtr[2] = (ImDrawIdx)(DrawList._VtxCurrentIdx + 2);
                DrawList._IdxWritePtr[3] = (ImDrawIdx)(DrawList._VtxCurrentIdx);
                DrawList._IdxWritePtr[4] = (ImDrawIdx)(DrawList._VtxCurrentIdx + 2);
                DrawList._IdxWritePtr[5] = (ImDrawIdx)(DrawList._VtxCurrentIdx + 3);

                DrawList._VtxWritePtr += 4;
                DrawList._IdxWritePtr += 6;
                DrawList._VtxCurrentIdx += 4;                    
            }                
        }
        EndItem();
    }
}

void PlotLineNoWrite(const char *label_id, const ImVec2 *values, int count)
{
    ImPlotContext &gp = *GImPlot;
    if (BeginItem(label_id, ImPlotCol_Line))
    {
        if (FitThisFrame())
        {
            for (int i = 0; i < count; ++i)
                FitPoint(ImPoint(values[i].x, values[i].y));
        }
        const ImPlotNextItemData &s = GetItemData();
        ImDrawList &DrawList = *GetPlotDrawList();
        if (count > 1 && s.RenderLine)
        {
            unsigned int prims = count - 1;                        
            DrawList.PrimReserve(prims * 6, prims * 4);  
            for (unsigned int idx = 0; idx < prims; ++idx)
            {
                DrawList._VtxWritePtr += 4;
                DrawList._IdxWritePtr += 6;
                DrawList._VtxCurrentIdx += 4;                    
            }           
        }
        EndItem();
    }
}

union float8 {
    __m256 v;
    float a[8];
};

void PlotLineAVX2(const char *label_id, const ImVec2 *V, int count) {
    ImPlotContext &gp = *GImPlot;
    if (BeginItem(label_id, ImPlotCol_Line))
    {
        if (FitThisFrame())
        {
            for (int i = 0; i < count; ++i)
            {
                FitPoint(ImPoint(V[i].x, V[i].y));
            }
        }
        const ImPlotNextItemData &s = GetItemData();
        ImDrawList &DrawList = *GetPlotDrawList();
        if (count > 1 && s.RenderLine)
        {
            // how many indices remain?
            unsigned int rem_ind = 4294967295 - DrawList._VtxCurrentIdx;
            // how many line segments can we render?
            unsigned int pos_lines = rem_ind / 4;

            const ImU32 col = ImGui::GetColorU32(s.Colors[ImPlotCol_Line]);
            const float weight = s.LineWeight;
            const ImVec2 uv = DrawList._Data->TexUvWhitePixel;

            const float minx_pix = gp.PixelRange[0].Min.x;
            const float miny_pix = gp.PixelRange[0].Min.y;
            const float minx_plt = (float)gp.CurrentPlot->XAxis[0].Range.Min;
            const float miny_plt = (float)gp.CurrentPlot->YAxis[0].Range.Min;
            const float mx = (float)gp.Mx[0];
            const float my = (float)gp.My[0];
            unsigned int prims = (count - 1)/8;
            prims *= 8;
            
            __m256 minx_pix8 = _mm256_set1_ps(minx_pix);
            __m256 miny_pix8 = _mm256_set1_ps(miny_pix);
            __m256 minx_plt8 = _mm256_set1_ps(-minx_plt);
            __m256 miny_plt8 = _mm256_set1_ps(-miny_plt);
            __m256 mx8       = _mm256_set1_ps(mx);
            __m256 my8       = _mm256_set1_ps(my);
            __m256 weight8   = _mm256_set1_ps(weight/2);

            DrawList.PrimReserve(prims * 6, prims * 4);

            auto vtx_pp = DrawList._VtxWritePtr;
            auto idx_pp = DrawList._IdxWritePtr;
            auto idx_cc = DrawList._VtxCurrentIdx; 

            // #pragma omp parallel for schedule(dynamic, 1) // REALLY FUCKING BAD
            for (int i = 0; i < prims; i += 8) {

                // SIMD before inner loop: no gain

                auto Vi = V + i;

                __m256 V1x = _mm256_set_ps(Vi[0].x,Vi[1].x,Vi[2].x,Vi[3].x,Vi[4].x,Vi[5].x,Vi[6].x,Vi[7].x);
                __m256 V2x = _mm256_set_ps(Vi[1].x,Vi[2].x,Vi[3].x,Vi[4].x,Vi[5].x,Vi[6].x,Vi[7].x,Vi[8].x);
                __m256 V1y = _mm256_set_ps(Vi[0].y,Vi[1].y,Vi[2].y,Vi[3].y,Vi[4].y,Vi[5].y,Vi[6].y,Vi[7].y);
                __m256 V2y = _mm256_set_ps(Vi[1].y,Vi[2].y,Vi[3].y,Vi[4].y,Vi[5].y,Vi[6].y,Vi[7].y,Vi[8].y);

                __m256 P1x = _mm256_add_ps(minx_pix8,_mm256_mul_ps(mx8,_mm256_add_ps(V1x,minx_plt8)));
                __m256 P2x = _mm256_add_ps(minx_pix8,_mm256_mul_ps(mx8,_mm256_add_ps(V2x,minx_plt8)));
                __m256 Vx  = _mm256_sub_ps(P2x,P1x);

                __m256 P1y = _mm256_add_ps(miny_pix8,_mm256_mul_ps(my8,_mm256_add_ps(V1y,miny_plt8)));
                __m256 P2y = _mm256_add_ps(miny_pix8,_mm256_mul_ps(my8,_mm256_add_ps(V2y,miny_plt8)));
                __m256 Vy  = _mm256_sub_ps(P2y,P1y);

                __m256 V2  = _mm256_add_ps(_mm256_mul_ps(Vx,Vx),_mm256_mul_ps(Vy,Vy));
                __m256 IV2 = _mm256_rsqrt_ps(V2);

                __m256 W   = _mm256_mul_ps(weight8,IV2);
                __m256 Dx  = _mm256_mul_ps(Vx,W);
                __m256 Dy  = _mm256_mul_ps(Vy,W);

                float8 P1y_p_Dx = { _mm256_add_ps(P1y,Dx) };
                float8 P1y_m_Dx = { _mm256_sub_ps(P1y,Dx) };
                float8 P2y_p_Dx = { _mm256_add_ps(P2y,Dx) };
                float8 P2y_m_Dx = { _mm256_sub_ps(P2y,Dx) };
                float8 P2x_p_Dy = { _mm256_add_ps(P2x,Dy) };
                float8 P2x_m_Dy = { _mm256_sub_ps(P2x,Dy) };
                float8 P1x_p_Dy = { _mm256_add_ps(P1x,Dy) };
                float8 P1x_m_Dy = { _mm256_sub_ps(P1x,Dy) };


                 for (int j = 0; j < 8; ++j) { 

                    auto vtx_p = vtx_pp + (i+j)*4;
                    auto idx_p = idx_pp + (i+j)*6;
                    auto idx_c = idx_cc + (i+j)*4;  

                    vtx_p[0].pos.x = P1x_p_Dy.a[j];
                    vtx_p[0].pos.y = P1y_m_Dx.a[j];
                    vtx_p[0].uv    = uv;
                    vtx_p[0].col   = col;
                    vtx_p[1].pos.x = P2x_p_Dy.a[j];
                    vtx_p[1].pos.y = P2y_m_Dx.a[j];
                    vtx_p[1].uv    = uv;
                    vtx_p[1].col   = col;
                    vtx_p[2].pos.x = P2x_m_Dy.a[j];
                    vtx_p[2].pos.y = P2y_p_Dx.a[j];
                    vtx_p[2].uv    = uv;
                    vtx_p[2].col   = col;
                    vtx_p[3].pos.x = P1x_m_Dy.a[j];
                    vtx_p[3].pos.y = P1y_p_Dx.a[j];
                    vtx_p[3].uv    = uv;
                    vtx_p[3].col   = col;

                    idx_p[0] = idx_c;
                    idx_p[1] = idx_c + 1;
                    idx_p[2] = idx_c + 2;
                    idx_p[3] = idx_c;
                    idx_p[4] = idx_c + 2;
                    idx_p[5] = idx_c + 3;
                }  


/*
                auto vtx_p = vtx_pp + (i)*4;
                auto idx_p = idx_pp + (i)*6;

                vtx_p[0*4+0].pos.x = P1x_p_Dy.m256_f32[0];
                vtx_p[1*4+0].pos.x = P1x_p_Dy.m256_f32[1];
                vtx_p[2*4+0].pos.x = P1x_p_Dy.m256_f32[2];
                vtx_p[3*4+0].pos.x = P1x_p_Dy.m256_f32[3];
                vtx_p[4*4+0].pos.x = P1x_p_Dy.m256_f32[4];
                vtx_p[5*4+0].pos.x = P1x_p_Dy.m256_f32[5];
                vtx_p[6*4+0].pos.x = P1x_p_Dy.m256_f32[6];
                vtx_p[7*4+0].pos.x = P1x_p_Dy.m256_f32[7];

                vtx_p[0*4+0].pos.y = P1y_m_Dx.m256_f32[0];
                vtx_p[1*4+0].pos.y = P1y_m_Dx.m256_f32[1];
                vtx_p[2*4+0].pos.y = P1y_m_Dx.m256_f32[2];
                vtx_p[3*4+0].pos.y = P1y_m_Dx.m256_f32[3];
                vtx_p[4*4+0].pos.y = P1y_m_Dx.m256_f32[4];
                vtx_p[5*4+0].pos.y = P1y_m_Dx.m256_f32[5];
                vtx_p[6*4+0].pos.y = P1y_m_Dx.m256_f32[6];
                vtx_p[7*4+0].pos.y = P1y_m_Dx.m256_f32[7];
                
                vtx_p[0*4+1].pos.x = P2x_p_Dy.m256_f32[0];
                vtx_p[1*4+1].pos.x = P2x_p_Dy.m256_f32[1];
                vtx_p[2*4+1].pos.x = P2x_p_Dy.m256_f32[2];
                vtx_p[3*4+1].pos.x = P2x_p_Dy.m256_f32[3];
                vtx_p[4*4+1].pos.x = P2x_p_Dy.m256_f32[4];
                vtx_p[5*4+1].pos.x = P2x_p_Dy.m256_f32[5];
                vtx_p[6*4+1].pos.x = P2x_p_Dy.m256_f32[6];
                vtx_p[7*4+1].pos.x = P2x_p_Dy.m256_f32[7];

                vtx_p[0*4+1].pos.y = P2y_m_Dx.m256_f32[0];
                vtx_p[1*4+1].pos.y = P2y_m_Dx.m256_f32[1];
                vtx_p[2*4+1].pos.y = P2y_m_Dx.m256_f32[2];
                vtx_p[3*4+1].pos.y = P2y_m_Dx.m256_f32[3];
                vtx_p[4*4+1].pos.y = P2y_m_Dx.m256_f32[4];
                vtx_p[5*4+1].pos.y = P2y_m_Dx.m256_f32[5];
                vtx_p[6*4+1].pos.y = P2y_m_Dx.m256_f32[6];
                vtx_p[7*4+1].pos.y = P2y_m_Dx.m256_f32[7];

                vtx_p[0*4+2].pos.x = P2x_m_Dy.m256_f32[0];
                vtx_p[1*4+2].pos.x = P2x_m_Dy.m256_f32[1];
                vtx_p[2*4+2].pos.x = P2x_m_Dy.m256_f32[2];
                vtx_p[3*4+2].pos.x = P2x_m_Dy.m256_f32[3];
                vtx_p[4*4+2].pos.x = P2x_m_Dy.m256_f32[4];
                vtx_p[5*4+2].pos.x = P2x_m_Dy.m256_f32[5];
                vtx_p[6*4+2].pos.x = P2x_m_Dy.m256_f32[6];
                vtx_p[7*4+2].pos.x = P2x_m_Dy.m256_f32[7];

                vtx_p[0*4+2].pos.y = P2y_p_Dx.m256_f32[0];
                vtx_p[1*4+2].pos.y = P2y_p_Dx.m256_f32[1];
                vtx_p[2*4+2].pos.y = P2y_p_Dx.m256_f32[2];
                vtx_p[3*4+2].pos.y = P2y_p_Dx.m256_f32[3];
                vtx_p[4*4+2].pos.y = P2y_p_Dx.m256_f32[4];
                vtx_p[5*4+2].pos.y = P2y_p_Dx.m256_f32[5];
                vtx_p[6*4+2].pos.y = P2y_p_Dx.m256_f32[6];
                vtx_p[7*4+2].pos.y = P2y_p_Dx.m256_f32[7];

                vtx_p[0*4+3].pos.x = P1x_m_Dy.m256_f32[0];
                vtx_p[1*4+3].pos.x = P1x_m_Dy.m256_f32[1];
                vtx_p[2*4+3].pos.x = P1x_m_Dy.m256_f32[2];
                vtx_p[3*4+3].pos.x = P1x_m_Dy.m256_f32[3];
                vtx_p[4*4+3].pos.x = P1x_m_Dy.m256_f32[4];
                vtx_p[5*4+3].pos.x = P1x_m_Dy.m256_f32[5];
                vtx_p[6*4+3].pos.x = P1x_m_Dy.m256_f32[6];
                vtx_p[7*4+3].pos.x = P1x_m_Dy.m256_f32[7];

                vtx_p[0*4+3].pos.y = P1y_p_Dx.m256_f32[0];
                vtx_p[1*4+3].pos.y = P1y_p_Dx.m256_f32[1];
                vtx_p[2*4+3].pos.y = P1y_p_Dx.m256_f32[2];
                vtx_p[3*4+3].pos.y = P1y_p_Dx.m256_f32[3];
                vtx_p[4*4+3].pos.y = P1y_p_Dx.m256_f32[4];
                vtx_p[5*4+3].pos.y = P1y_p_Dx.m256_f32[5];
                vtx_p[6*4+3].pos.y = P1y_p_Dx.m256_f32[6];
                vtx_p[7*4+3].pos.y = P1y_p_Dx.m256_f32[7];              

                vtx_p[0*4+0].uv    = uv;
                vtx_p[1*4+0].uv    = uv;
                vtx_p[2*4+0].uv    = uv;
                vtx_p[3*4+0].uv    = uv;
                vtx_p[4*4+0].uv    = uv;
                vtx_p[5*4+0].uv    = uv;
                vtx_p[6*4+0].uv    = uv;
                vtx_p[7*4+0].uv    = uv;
                vtx_p[0*4+1].uv    = uv;
                vtx_p[1*4+1].uv    = uv;
                vtx_p[2*4+1].uv    = uv;
                vtx_p[3*4+1].uv    = uv;
                vtx_p[4*4+1].uv    = uv;
                vtx_p[5*4+1].uv    = uv;
                vtx_p[6*4+1].uv    = uv;
                vtx_p[7*4+1].uv    = uv;
                vtx_p[0*4+2].uv    = uv;
                vtx_p[1*4+2].uv    = uv;
                vtx_p[2*4+2].uv    = uv;
                vtx_p[3*4+2].uv    = uv;
                vtx_p[4*4+2].uv    = uv;
                vtx_p[5*4+2].uv    = uv;
                vtx_p[6*4+2].uv    = uv;
                vtx_p[7*4+2].uv    = uv;
                vtx_p[0*4+3].uv    = uv;
                vtx_p[1*4+3].uv    = uv;
                vtx_p[2*4+3].uv    = uv;
                vtx_p[3*4+3].uv    = uv;
                vtx_p[4*4+3].uv    = uv;
                vtx_p[5*4+3].uv    = uv;
                vtx_p[6*4+3].uv    = uv;
                vtx_p[7*4+3].uv    = uv;

                vtx_p[0*4+0].col   = col;
                vtx_p[1*4+0].col   = col;
                vtx_p[2*4+0].col   = col;
                vtx_p[3*4+0].col   = col;
                vtx_p[4*4+0].col   = col;
                vtx_p[5*4+0].col   = col;
                vtx_p[6*4+0].col   = col;
                vtx_p[7*4+0].col   = col;
                vtx_p[0*4+1].col   = col;
                vtx_p[1*4+1].col   = col;
                vtx_p[2*4+1].col   = col;
                vtx_p[3*4+1].col   = col;
                vtx_p[4*4+1].col   = col;
                vtx_p[5*4+1].col   = col;
                vtx_p[6*4+1].col   = col;
                vtx_p[7*4+1].col   = col;
                vtx_p[0*4+2].col   = col;
                vtx_p[1*4+2].col   = col;
                vtx_p[2*4+2].col   = col;
                vtx_p[3*4+2].col   = col;
                vtx_p[4*4+2].col   = col;
                vtx_p[5*4+2].col   = col;
                vtx_p[6*4+2].col   = col;
                vtx_p[7*4+2].col   = col;
                vtx_p[0*4+3].col   = col;
                vtx_p[1*4+3].col   = col;
                vtx_p[2*4+3].col   = col;
                vtx_p[3*4+3].col   = col;
                vtx_p[4*4+3].col   = col;
                vtx_p[5*4+3].col   = col;
                vtx_p[6*4+3].col   = col;
                vtx_p[7*4+3].col   = col;

                auto idx_c = idx_cc + (i+0)*4;  

                idx_p[0*6+0] = idx_c;
                idx_p[0*6+1] = idx_c + 1;
                idx_p[0*6+2] = idx_c + 2;
                idx_p[0*6+3] = idx_c;
                idx_p[0*6+4] = idx_c + 2;
                idx_p[0*6+5] = idx_c + 3;

                idx_c = idx_cc + (i+1)*4;       

                idx_p[1*6+0] = idx_c;
                idx_p[1*6+1] = idx_c + 1;
                idx_p[1*6+2] = idx_c + 2;
                idx_p[1*6+3] = idx_c;
                idx_p[1*6+4] = idx_c + 2;
                idx_p[1*6+5] = idx_c + 3;

                idx_c = idx_cc + (i+2)*4;  

                idx_p[2*6+0] = idx_c;
                idx_p[2*6+1] = idx_c + 1;
                idx_p[2*6+2] = idx_c + 2;
                idx_p[2*6+3] = idx_c;
                idx_p[2*6+4] = idx_c + 2;
                idx_p[2*6+5] = idx_c + 3;

                idx_c = idx_cc + (i+3)*4;   

                idx_p[3*6+0] = idx_c;
                idx_p[3*6+1] = idx_c + 1;
                idx_p[3*6+2] = idx_c + 2;
                idx_p[3*6+3] = idx_c;
                idx_p[3*6+4] = idx_c + 2;
                idx_p[3*6+5] = idx_c + 3;

                idx_c = idx_cc + (i+4)*4; 

                idx_p[4*6+0] = idx_c;
                idx_p[4*6+1] = idx_c + 1;
                idx_p[4*6+2] = idx_c + 2;
                idx_p[4*6+3] = idx_c;
                idx_p[4*6+4] = idx_c + 2;
                idx_p[4*6+5] = idx_c + 3;

                idx_c = idx_cc + (i+5)*4;
                                  
                idx_p[5*6+0] = idx_c;
                idx_p[5*6+1] = idx_c + 1;
                idx_p[5*6+2] = idx_c + 2;
                idx_p[5*6+3] = idx_c;
                idx_p[5*6+4] = idx_c + 2;
                idx_p[5*6+5] = idx_c + 3;

                idx_c = idx_cc + (i+6)*4; 

                idx_p[6*6+0] = idx_c;
                idx_p[6*6+1] = idx_c + 1;
                idx_p[6*6+2] = idx_c + 2;
                idx_p[6*6+3] = idx_c;
                idx_p[6*6+4] = idx_c + 2;
                idx_p[6*6+5] = idx_c + 3;

                idx_c = idx_cc + (i+7)*4;

                idx_p[6*7+0] = idx_c;
                idx_p[6*7+1] = idx_c + 1;
                idx_p[6*7+2] = idx_c + 2;
                idx_p[6*7+3] = idx_c;
                idx_p[6*7+4] = idx_c + 2;
                idx_p[6*7+5] = idx_c + 3;
            */
            }

            DrawList._VtxWritePtr   += prims*4;
            DrawList._IdxWritePtr   += prims*6;
            DrawList._VtxCurrentIdx += prims*4; 
        }
        ImPlot::EndItem();
    }
}


} // namespace ImPlot

using Clock = std::chrono::high_resolution_clock;

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

enum BenchMode {
    BenchMode_Line = 0,
    BenchMode_LineG,
    BenchMode_LineInline,
    BenchMode_LineImGui,
    BenchMode_LineNoTess,
    BenchMode_LineNoWrite,
    BenchMode_LineGPU,
    BenchMode_LineAVX2,
    BenchMode_COUNT
};

static const char* BenchMode_Names[] = {"Line","LineG","LineInline","LineImGui","LineNoTess","LineNoWrite","LineGPU","LineAVX2"};

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

void to_json(json& j, const BenchRecord& p) {
    j = json{{"name", p.name}, {"mode", p.mode}, {"items",p.items},{"fps",p.fps},{"call",p.call},{"loop",p.loop}};
}

void from_json(const json& j, BenchRecord& p) {
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
                if (file.is_open()) {
                json j;
                file >> j;
                j.at("records").get_to(records);
            }
        // }

        GetBranchName("C:/git/implot",branch);
        glEnable(GL_MULTISAMPLE);
        ImPlot::CreateGpuContext();
    }

    void Update() override
    {
        ImGui::SetNextWindowSize(GetWindowSize(), ImGuiCond_Always);
        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
        ImGui::Begin("ImPlot Benchmark Tool",0,ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoTitleBar);
        if (ImGui::BeginTabBar("Tabs")) {
            if (ImGui::BeginTabItem("Bench")) {
                ShowBenchmarkTool();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Results")) {
                ShowResultsTool();
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
        ImGui::End();
    }

    static constexpr int max_items = 500;
    BenchData items[max_items];

    void ShowBenchmarkPlotOnly(int mode) {
        size_t call_time = 0;
        ImGui::Text("%.2f FPS", ImGui::GetIO().Framerate);
        ImPlot::SetNextPlotLimits(0, 1000, 0, 1, ImGuiCond_Always);
        if (ImPlot::BeginPlot("##Bench", NULL, NULL, ImVec2(-1, -1), ImPlotFlags_NoChild | ImPlotFlags_CanvasOnly, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_NoDecorations))
        {
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

        auto get_run_name = [&]() {
            auto str = branch + "_" + std::string(BenchMode_Names[mode]);
            if (mode == BenchMode_LineImGui)
            {
                str += GImGui->Style.AntiAliasedLines ? "-AA" : "";
                str += GImGui->Style.AntiAliasedLinesUseTex ? "-Tex" : "";
            }
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
                records.back().call.push_back( (call_time / frames) * 0.001f);
                records.back().loop.push_back(1000.0*(t2-t1)/frames);
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
        if (ImGui::Button("Warm")) {
            L = L > 0 ? 0 : max_items;
            call_time = 0;
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100);
        if (ImGui::Combo("##Mode",&mode,BenchMode_Names,BenchMode_COUNT))
            run_name = get_run_name();
        ImGui::SameLine();
        if (ImGui::Checkbox("AA",&GImGui->Style.AntiAliasedLines)) { run_name = get_run_name(); }
        ImGui::SameLine();
        if (ImGui::Checkbox("Tex",&GImGui->Style.AntiAliasedLinesUseTex)) { run_name = get_run_name(); }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##name",&run_name);
        if (was_running)
        {
            ImGui::PopItemFlag();
            ImGui::PopStyleVar();
        }
        ImGui::ProgressBar((float)L / (float)(max_items - 1));


        ImPlot::SetNextPlotLimits(0, 1000, 0, 1, ImGuiCond_Always);
        if (ImPlot::BeginPlot("##Bench", NULL, NULL, ImVec2(-1, -1), ImPlotFlags_NoChild | ImPlotFlags_CanvasOnly, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_NoDecorations))
        {
            PlotBenchmarkItems(mode, L, call_time);
            ImPlot::EndPlot();
        }


    }

    void ShowResultsTool() {

        static bool show_loop_time = true;
        static bool show_call_time = true;
        static bool show_fps       = false;

        if (ImGui::Button("Clear"))
            records.clear();
        ImGui::SameLine(); ImGui::Checkbox("Frame Time",&show_loop_time);
        ImGui::SameLine(); ImGui::Checkbox("Call Time",&show_call_time);
        ImGui::SameLine(); ImGui::Checkbox("FPS",&show_fps);

        ImPlot::SetNextPlotLimits(0, max_items, 0,50);
        ImPlot::SetNextPlotLimitsY(0,500,2,1);
        // ImPlot::SetNextPlotLimitsY(0,15,2,ImPlotYAxis_2);
        int flags = ImPlotFlags_NoChild;
        if (show_fps)
            flags = flags | ImPlotFlags_YAxis2;

        static std::vector<int> to_delete;
        to_delete.clear();

        if (ImPlot::BeginPlot("##Stats", "Items (1,000 pts each)", "Time (ms)", ImVec2(-1, -1), flags, 0,0,2,2,"FPS (Hz)"))
        {
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
                if (ImPlot::BeginLegendPopup(records[run].name.c_str())) {
                    if (ImGui::Button("Delete")) {
                        to_delete.push_back(run);
                    }
                    ImPlot::EndLegendPopup();
                }
            }
            ImPlot::EndPlot();
        }

        for (int i = to_delete.size(); i --> 0;) {
            int idx = to_delete[i];
            records.erase(records.begin()+idx);
        }
    }


    void PlotBenchmarkItems(int mode, int L, size_t& call_time) {
        if (mode == BenchMode_Line)
        {
            for (int i = 0; i < L; ++i) {
                ImGui::PushID(i);
                ImPlot::SetNextLineStyle(items[i].Col);
                {
                    ScopedProfiler prof(call_time);
                    ImPlot::PlotLine("##item", &items[i].Data[0].x, &items[i].Data[0].y, 1000, 0, sizeof(ImVec2));
                }
                ImGui::PopID();
            }
        }
        else if (mode == BenchMode_LineG) {
            for (int i = 0; i < L; ++i) {
                ImGui::PushID(i);
                ImPlot::SetNextLineStyle(items[i].Col);
                {
                    ScopedProfiler prof(call_time);
                    ImPlot::PlotLineG("##item", [](void* in, int idx) {
                        ImVec2* data = (ImVec2*)in;
                        return ImPoint(data[idx].x,data[idx].y);
                    },items[i].Data,1000);
                }
                ImGui::PopID();
            }
        }
        else if (mode == BenchMode_LineInline) {
            for (int i = 0; i < L; ++i) {
                ImGui::PushID(i);
                ImPlot::SetNextLineStyle(items[i].Col);
                {
                    ScopedProfiler prof(call_time);
                    ImPlot::PlotLineInline("##item", items[i].Data, 1000);
                }
                ImGui::PopID();
            }
        }
        else if (mode == BenchMode_LineImGui)
        {
            GImPlot->Style.AntiAliasedLines = true;
            for (int i = 0; i < L; ++i) {
                ImGui::PushID(i);
                ImPlot::SetNextLineStyle(items[i].Col);
                {
                    ScopedProfiler prof(call_time);
                    ImPlot::PlotLine("##item", &items[i].Data[0].x, &items[i].Data[0].y, 1000, 0, sizeof(ImVec2));
                }
                ImGui::PopID();
            }
            GImPlot->Style.AntiAliasedLines = false;
        }
        else if (mode == BenchMode_LineNoTess)
        {
            for (int i = 0; i < L; ++i) {
                ImGui::PushID(i);
                ImPlot::SetNextLineStyle(items[i].Col);
                {
                    ScopedProfiler prof(call_time);
                    ImPlot::PlotLineNoTess("##item", items[i].Data, 1000);
                }
                ImGui::PopID();
            }
        }
        else if (mode == BenchMode_LineNoWrite)
        {
            for (int i = 0; i < L; ++i) {
                ImGui::PushID(i);
                ImPlot::SetNextLineStyle(items[i].Col);
                {
                    ScopedProfiler prof(call_time);
                    ImPlot::PlotLineNoWrite("##item", items[i].Data, 1000);
                }
                ImGui::PopID();
            }
        }
        else if (mode == BenchMode_LineGPU) {
            for (int i = 0; i < L; ++i) {
                ImGui::PushID(i);
                ImPlot::SetNextLineStyle(items[i].Col);
                {
                    ScopedProfiler prof(call_time);
                    ImPlot::PlotLineGPU("##item", items[i].Data, 1000);
                }
                ImGui::PopID();
            }
        }
        else if (mode == BenchMode_LineAVX2) {
            for (int i = 0; i < L; ++i) {
                ImGui::PushID(i);
                ImPlot::SetNextLineStyle(items[i].Col);
                {
                    ScopedProfiler prof(call_time);
                    ImPlot::PlotLineAVX2("##item", items[i].Data, 1000);
                }
                ImGui::PopID();
            }
        }
    }
};

int main(int argc, char const *argv[])
{
    ImPlotBench app("ImPlot Benchmark",640,480,argc,argv);
    app.Run();
    return 0;
}


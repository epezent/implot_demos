#include <implot_internal.h>

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
    template <typename T>
    void PlotLineInline(const char *label_id, const T *xs, const T *ys, int count)
    {
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
                const float mx = gp.CurrentPlot->Axes[ImAxis_X1].LinM;
                const float my = gp.CurrentPlot->Axes[ImAxis_Y1].LinM;

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

    template <typename T>
    void PlotLineStaged(const char *label_id, const T *xs, const T *ys, int count)
    {
        static ImVector<double> xs_plt; xs_plt.resize(count);
        static ImVector<double> ys_plt; ys_plt.resize(count);
        static ImVector<float>  xs_pix; xs_pix.resize(count);  
        static ImVector<float>  ys_pix; ys_pix.resize(count);

        for (int i = 0; i < count; ++i) {
            xs_plt[i] = (double)xs[i];
            ys_plt[i] = (double)ys[i];
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
                const unsigned int prims = count - 1;
                const ImVec2 uv = DrawList._Data->TexUvWhitePixel;
                const float minx_pix = gp.CurrentPlot->Axes[ImAxis_X1].PixelMin;
                const float miny_pix = gp.CurrentPlot->Axes[ImAxis_Y1].PixelMin;
                const float minx_plt = gp.CurrentPlot->Axes[ImAxis_X1].Range.Min;
                const float miny_plt = gp.CurrentPlot->Axes[ImAxis_Y1].Range.Min;
                const float mx       = gp.CurrentPlot->Axes[ImAxis_X1].LinM;
                const float my       = gp.CurrentPlot->Axes[ImAxis_Y1].LinM;
                // xform data
                for (int i = 0; i < count; ++i) {
                    xs_pix[i] = minx_pix + mx * ((float)xs_plt[i] - minx_plt);
                    ys_pix[i] = miny_pix + my * ((float)ys_plt[i] - miny_plt);    
                }        
                DrawList.PrimReserve(prims * 6, prims * 4);
                for (unsigned int i = 0; i < prims; ++i)
                {
                    const float x1 = xs_pix[i];
                    const float x2 = xs_pix[i+1];
                    const float y1 = ys_pix[i];
                    const float y2 = ys_pix[i+1];
                    float dx = x2 - x1;
                    float dy = y2 - y1;
                    IMPLOT_NORMALIZE2F_OVER_ZERO(dx, dy);
                    dx *= (weight * 0.5f);
                    dy *= (weight * 0.5f);
                    DrawList._VtxWritePtr[0].pos.x = x1 + dy;
                    DrawList._VtxWritePtr[0].pos.y = y1 - dx;
                    DrawList._VtxWritePtr[0].uv = uv;
                    DrawList._VtxWritePtr[0].col = col;
                    DrawList._VtxWritePtr[1].pos.x = x2 + dy;
                    DrawList._VtxWritePtr[1].pos.y = y2 - dx;
                    DrawList._VtxWritePtr[1].uv = uv;
                    DrawList._VtxWritePtr[1].col = col;
                    DrawList._VtxWritePtr[2].pos.x = x2 - dy;
                    DrawList._VtxWritePtr[2].pos.y = y2 + dx;
                    DrawList._VtxWritePtr[2].uv = uv;
                    DrawList._VtxWritePtr[2].col = col;
                    DrawList._VtxWritePtr[3].pos.x = x1 - dy;
                    DrawList._VtxWritePtr[3].pos.y = y1 + dx;
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
            }
            EndItem();
        }
    }
}
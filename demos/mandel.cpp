// Demo:   demo.cpp
// Author: Evan Pezent (evanpezent.com)
// Date:   3/26/2021

#include "App.h"
#include "ThreadPool.h"

#include <imgui_internal.h>
#include <immintrin.h>

// https://nullprogram.com/blog/2015/07/10/

struct spec {
    /* Image Specification */
    int width;
    int height;
    int depth;
    /* Fractal Specification */
    double xlim[2];
    double ylim[2];
    int iterations;
};

template <typename T>
void mandel_basic(unsigned char *image, const struct spec *s)
{
    T xdiff = s->xlim[1] - s->xlim[0];
    T ydiff = s->ylim[1] - s->ylim[0];
    T iter_scale = 1.0f / s->iterations;
    T depth_scale = s->depth - 1;
    #pragma omp parallel for schedule(dynamic, 1)
    for (int y = 0; y < s->height; y++) {
        for (int x = 0; x < s->width; x++) {
            T cr = x * xdiff / s->width  + s->xlim[0];
            T ci = y * ydiff / s->height + s->ylim[0];
            T zr = cr;
            T zi = ci;
            int k = 0;
            T mk = 0.0f;
            while (++k < s->iterations) {
                T zr1 = zr * zr - zi * zi + cr;
                T zi1 = zr * zi + zr * zi + ci;
                zr = zr1;
                zi = zi1;
                mk += 1.0f;
                if (zr * zr + zi * zi >= 4.0f)
                    break;
            }
            mk *= iter_scale;
            mk = sqrtf(mk);
            mk *= depth_scale;
            int pixel = mk;
            image[y * s->width + x] = pixel;
        }
    }
}

template <typename T>
void mandel_avx(unsigned char *image, const struct spec *s);


template <>
void mandel_avx<float>(unsigned char *image, const struct spec *s)
{
    __m256 xmin = _mm256_set1_ps(s->xlim[0]);
    __m256 ymin = _mm256_set1_ps(s->ylim[0]);
    __m256 xscale = _mm256_set1_ps((s->xlim[1] - s->xlim[0]) / s->width);
    __m256 yscale = _mm256_set1_ps((s->ylim[1] - s->ylim[0]) / s->height);
    __m256 threshold = _mm256_set1_ps(4);
    __m256 one = _mm256_set1_ps(1);
    __m256 iter_scale = _mm256_set1_ps(1.0f / s->iterations);
    __m256 depth_scale = _mm256_set1_ps(s->depth - 1);

    #pragma omp parallel for schedule(dynamic, 1)
    for (int y = 0; y < s->height; y++) {
        for (int x = 0; x < s->width; x += 8) {
            __m256 mx = _mm256_set_ps(x + 7, x + 6, x + 5, x + 4,
                                      x + 3, x + 2, x + 1, x + 0);
            __m256 my = _mm256_set1_ps(y);
            __m256 cr = _mm256_add_ps(_mm256_mul_ps(mx, xscale), xmin);
            __m256 ci = _mm256_add_ps(_mm256_mul_ps(my, yscale), ymin);
            __m256 zr = cr;
            __m256 zi = ci;
            int k = 1;
            __m256 mk = _mm256_set1_ps(k);
            while (++k < s->iterations) {
                /* Compute z1 from z0 */
                __m256 zr2 = _mm256_mul_ps(zr, zr);
                __m256 zi2 = _mm256_mul_ps(zi, zi);
                __m256 zrzi = _mm256_mul_ps(zr, zi);
                /* zr1 = zr0 * zr0 - zi0 * zi0 + cr */
                /* zi1 = zr0 * zi0 + zr0 * zi0 + ci */
                zr = _mm256_add_ps(_mm256_sub_ps(zr2, zi2), cr);
                zi = _mm256_add_ps(_mm256_add_ps(zrzi, zrzi), ci);

                /* Increment k */
                zr2 = _mm256_mul_ps(zr, zr);
                zi2 = _mm256_mul_ps(zi, zi);
                __m256 mag2 = _mm256_add_ps(zr2, zi2);
                __m256 mask = _mm256_cmp_ps(mag2, threshold, _CMP_LT_OS);
                mk = _mm256_add_ps(_mm256_and_ps(mask, one), mk);

                /* Early bailout? */
                if (_mm256_testz_ps(mask, _mm256_set1_ps(-1)))
                    break;
            }
            mk = _mm256_mul_ps(mk, iter_scale);
            mk = _mm256_sqrt_ps(mk);
            mk = _mm256_mul_ps(mk, depth_scale);
            __m256i pixels = _mm256_cvtps_epi32(mk);
            unsigned char *dst = image + y * s->width + x;
            unsigned char *src = (unsigned char *)&pixels;
            for (int i = 0; i < 8; i++) {
                dst[i] = src[i * 4];
            }
        }
    }
}


template <>
void mandel_avx<double>(unsigned char *image, const struct spec *s)
{
    __m256d xmin = _mm256_set1_pd(s->xlim[0]);
    __m256d ymin = _mm256_set1_pd(s->ylim[0]);
    __m256d xscale = _mm256_set1_pd((s->xlim[1] - s->xlim[0]) / s->width);
    __m256d yscale = _mm256_set1_pd((s->ylim[1] - s->ylim[0]) / s->height);
    __m256d threshold = _mm256_set1_pd(4);
    __m256d one = _mm256_set1_pd(1);
    __m256d iter_scale = _mm256_set1_pd(1.0f / s->iterations);
    __m256d depth_scale = _mm256_set1_pd(s->depth - 1);

    #pragma omp parallel for schedule(dynamic, 1)
    for (int y = 0; y < s->height; y++) {
        for (int x = 0; x < s->width; x += 4) {
            __m256d mx = _mm256_set_pd(x + 3, x + 2, x + 1, x + 0);
            __m256d my = _mm256_set1_pd(y);
            __m256d cr = _mm256_add_pd(_mm256_mul_pd(mx, xscale), xmin);
            __m256d ci = _mm256_add_pd(_mm256_mul_pd(my, yscale), ymin);
            __m256d zr = cr;
            __m256d zi = ci;
            int k = 1;
            __m256d mk = _mm256_set1_pd(k);
            while (++k < s->iterations) {
                /* Compute z1 from z0 */
                __m256d zr2 = _mm256_mul_pd(zr, zr);
                __m256d zi2 = _mm256_mul_pd(zi, zi);
                __m256d zrzi = _mm256_mul_pd(zr, zi);
                /* zr1 = zr0 * zr0 - zi0 * zi0 + cr */
                /* zi1 = zr0 * zi0 + zr0 * zi0 + ci */
                zr = _mm256_add_pd(_mm256_sub_pd(zr2, zi2), cr);
                zi = _mm256_add_pd(_mm256_add_pd(zrzi, zrzi), ci);

                /* Increment k */
                zr2 = _mm256_mul_pd(zr, zr);
                zi2 = _mm256_mul_pd(zi, zi);
                __m256d mag2 = _mm256_add_pd(zr2, zi2);
                __m256d mask = _mm256_cmp_pd(mag2, threshold, _CMP_LT_OS);
                mk = _mm256_add_pd(_mm256_and_pd(mask, one), mk);

                /* Early bailout? */
                if (_mm256_testz_pd(mask, _mm256_set1_pd(-1)))
                    break;
            }
            mk = _mm256_mul_pd(mk, iter_scale);
            mk = _mm256_sqrt_pd(mk);
            mk = _mm256_mul_pd(mk, depth_scale);
            __m128i pixels = _mm256_cvtpd_epi32(mk);
            unsigned char *dst = image + y * s->width + x;
            unsigned char *src = (unsigned char *)&pixels;
            for (int i = 0; i < 4; i++) {
                dst[i] = src[i * 4];
            }
        }
    }
}



struct ImPlotDemo : App {
    

    static constexpr int kThreads = 12;
    std::vector<unsigned char> image;
    spec s;
    ThreadPool pool;
    
    ImPlotDemo() : App(640,480,"ImPlot Mandelbrot",false), pool(kThreads) { }

    void init() override {
        s.width = 960;
        s.height = 960;
        s.depth = 1024;
        s.xlim[0] = -2.5;
        s.xlim[1] =  1.5;
        s.ylim[0] = -1.5;
        s.ylim[1] =  1.5;
        s.iterations = 512;
        image.resize(s.width * s.height);
        // ImPlot::GetStyle().Colormap = ImPlotColormap_Spectral;

        ImU32 cmap[] = {
            IM_COL32(  0,   7, 100,255),
            IM_COL32( 32, 107, 203,255),
            IM_COL32(237, 255, 255,255),
            IM_COL32(255, 170,   0,255),
            IM_COL32(  0,   2,   0,255)
        };

        ImPlot::GetStyle().Colormap = ImPlot::AddColormap("Mandelbrot",cmap,5,false);

    };

    template <typename T>
    void mandel_basic_par() {
        std::future<void> results[kThreads];
        for (int i = 0; i < kThreads; ++i) {
            spec ss = s;
            ss.height /= kThreads;
            double dy = (s.ylim[1] - s.ylim[0]) / kThreads;
            ss.ylim[0] = s.ylim[0] + i * dy;
            ss.ylim[1] = ss.ylim[0] + dy;
            unsigned char* subImage = &image[s.width*s.height/kThreads*i];
            auto worker = [subImage,ss]() {
                mandel_basic<T>(subImage,&ss);
            };
            results[i] = pool.enqueue(worker);
        }
        for (int i=0; i<kThreads; i++) results[i].wait();
    }

    template <typename T>
    void mandel_avx_par() {
        std::future<void> results[kThreads];
        for (int i = 0; i < kThreads; ++i) {
            spec ss = s;
            ss.height /= kThreads;
            double dy = (s.ylim[1] - s.ylim[0]) / kThreads;
            ss.ylim[0] = s.ylim[0] + i * dy;
            ss.ylim[1] = ss.ylim[0] + dy;
            unsigned char* subImage = &image[s.width*s.height/kThreads*i];
            auto worker = [subImage,ss]() {
                mandel_avx<T>(subImage,&ss);
            };
            results[i] = pool.enqueue(worker);
        }
        for (int i=0; i<kThreads; i++) results[i].wait();
    }

    void update() override {

        static bool avx = true;
        static bool dp  = true;

        ImGui::SetNextWindowPos(ImVec2(0,0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(get_window_size(), ImGuiCond_Always);
        ImGui::Begin("##Mandel", nullptr, ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize);
        ImGui::Checkbox("AVX",&avx); ImGui::SameLine();
#ifndef _OPENMP
        static bool mt  = true;
        ImGui::Checkbox("Multithreaded",&mt); ImGui::SameLine();
#else
        static bool mt  = false;
#endif
        ImGui::Checkbox("Double",&dp); ImGui::SameLine();
        ImGui::Text("    FPS: %.2f", ImGui::GetIO().Framerate); 
        ImGui::SameLine(); 
        if (ImGui::Button(ICON_FA_HOME))
            ImPlot::SetNextPlotLimits(-2.5,1.5,-1.5,1.5,ImGuiCond_Always);
        else
            ImPlot::SetNextPlotLimits(-2.5,1.5,-1.5,1.5);
        if (ImPlot::BeginPlot("##Terrain",0,0,ImVec2(-1,-1),0,ImPlotAxisFlags_NoDecorations,ImPlotAxisFlags_NoDecorations)) {
            auto lims = ImPlot::GetPlotLimits();
            s.xlim[0] = lims.X.Min;
            s.xlim[1] = lims.X.Max;
            s.ylim[0] = lims.Y.Max;
            s.ylim[1] = lims.Y.Min;
            if (dp) {
                if (mt)
                    avx ? mandel_avx_par<double>() : mandel_basic_par<double>();
                else
                    avx ? mandel_avx<double>(image.data(),&s) : mandel_basic<double>(image.data(),&s);
            }
            else {
                if (mt)
                    avx ? mandel_avx_par<float>() : mandel_basic_par<float>();
                else
                    avx ? mandel_avx<float>(image.data(),&s) : mandel_basic<float>(image.data(),&s);
            }
            ImPlot::PlotHeatmap("##T",image.data(),s.height,s.width,0,256,NULL,lims.Min(),lims.Max());
            ImPlot::EndPlot();
        }
        ImGui::End();

    }
};

int main(int argc, char const *argv[])
{
    ImPlotDemo demo;
    demo.run();
    return 0;
}
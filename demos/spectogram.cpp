#define APP_USE_OPENGL
#include "App.hpp"

#include <kiss_fftr.h>
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include <atomic>
#include <mutex>
#include <iostream>
#include <filesystem>
#include <vector>
#include <complex>

std::atomic_bool g_playing{false}; // should audio play?
std::mutex       g_mtx;            // mutex to protect decoder data

// miniaudio playback callback
void audio_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    ma_decoder* pDecoder = (ma_decoder*)pDevice->pUserData;
    if (pDecoder == NULL)
        return;
    if (g_playing) {
        std::lock_guard<std::mutex> lock(g_mtx);
        ma_decoder_read_pcm_frames(pDecoder, pOutput, frameCount);
    }
    (void)pInput;
}

// Spectogram App
struct ImPlotSpectogram : App {

    static constexpr int WIN_W = 640;            // window width
    static constexpr int WIN_H = 480;            // window height

    static constexpr int FS    = 44100;          // sampling rate
    static constexpr int T     = 10;             // spectogram display period
    static constexpr int N_FFT = 1024;           // FFT size
    static constexpr int N_FRQ = N_FFT/2+1;      // FFT frequency count
    static constexpr int N_BIN = FS * T / N_FFT; // spectogram bin count

    kiss_fftr_cfg       m_fft;                   // FFT handle
    std::complex<float> m_fft_out[N_FFT];        // FFT output buffer
    float               m_fft_frq[N_FRQ];        // FFT output frequencies

    ma_decoder         m_decoder;                // miniaudio audio file decoder
    ma_device          m_device;                 // miniaudio playback device

    std::string        m_filename;               // filename of audio file provided
    double             m_duration;               // length of audio file in seconds
    std::vector<float> m_samples;                // local copy of audio file samples
    std::vector<float> m_spectogram;             // spectogram matric data

    float  m_t     = 0;                          // current playback time normalized to [0 1]
    double m_time  = 0;                          // current playback time in seconds

    float m_min_db = -10;                        // minimum spectogram dB
    float m_max_db =  40;                        // maximum spectogram dB

    // Consutrctor
    ImPlotSpectogram(const std::string& filepath) :
        App(WIN_W,WIN_H,"ImPlot Spectogram"),
        m_spectogram(N_FRQ*N_BIN,0)
    {
        // allocate FFT memory
        m_fft = kiss_fftr_alloc(N_FFT,0,nullptr,nullptr);
        // generate FFT frequencies
        for (int f = 0; f < N_FRQ; ++f)
            m_fft_frq[f] = f * (float)FS / (float)N_FFT;
        // get filename
        std::filesystem::path p(filepath);
        m_filename = p.filename().string();
        // initialize decoder (force float, mono, 44100 Hz)
        auto decoder_cfg = ma_decoder_config_init(ma_format_f32,1,FS);
        if (ma_decoder_init_file(filepath.c_str(), &decoder_cfg, &m_decoder) != MA_SUCCESS)
            std::runtime_error("Failed to decode audio file: " + filepath);
        // read all samples to local buffer
        auto n_samples = ma_decoder_get_length_in_pcm_frames(&m_decoder);
        m_samples.resize(n_samples);
        ma_decoder_read_pcm_frames(&m_decoder, m_samples.data(), n_samples);
        ma_decoder_seek_to_pcm_frame(&m_decoder, 0);
        // compute audio file duration
        m_duration = (double)n_samples / (double)FS;
        // prewarm spectogram
        update_spectogram(0);
        // initialize playback device
        auto device_cfg = ma_device_config_init(ma_device_type_playback);
        device_cfg.playback.format   = m_decoder.outputFormat;
        device_cfg.playback.channels = m_decoder.outputChannels;
        device_cfg.sampleRate        = m_decoder.outputSampleRate;
        device_cfg.dataCallback      = audio_callback;
        device_cfg.pUserData         = &m_decoder;
        if (ma_device_init(NULL, &device_cfg, &m_device) != MA_SUCCESS)
           std::runtime_error("Failed to open playback device.");
        // start audio playback thread
        if (ma_device_start(&m_device) != MA_SUCCESS)
            std::runtime_error("Failed to start playback device.");
        // set colormap
        ImPlot::GetStyle().Colormap = ImPlotColormap_Viridis;
    }

    // Destructor
    ~ImPlotSpectogram() {
        free(m_fft);
        ma_device_uninit(&m_device);
        ma_decoder_uninit(&m_decoder);
    }

    // Update function called once per frame
    void update() override {

        if (g_playing) {
            m_time += ImGui::GetIO().DeltaTime;
            m_t     = ImClamp((float)(m_time/m_duration),0.0f,1.0f);
            update_spectogram(m_t);
        }

        ImGui::SetNextWindowPos({0,0});
        ImGui::SetNextWindowSize({WIN_W,WIN_H});
        ImGui::Begin("ImPlot Spectogram",nullptr,ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoTitleBar);
        if (ImGui::Button(g_playing ? "Stop" : "Play", {50,0}))
            g_playing = !g_playing;
        ImGui::SameLine();
        ImGui::TextUnformatted(m_filename.c_str());
        ImGui::SameLine(560);
        ImGui::Text("FPS: %.2f", ImGui::GetIO().Framerate);
        if (ImGui::SliderFloat("Scrub",&m_t,0,1))
            seek(m_t);
        static bool show_frame = true;
        ImGui::SameLine();
        ImGui::Checkbox("Show Frame",&show_frame);
        const float tmin = (float)(m_duration-T)*m_t;
        const float tmax = tmin + T;
        ImPlot::SetNextPlotLimitsY(0,20);
        ImPlot::SetNextPlotLimitsX(tmin, tmax, ImGuiCond_Always);
        ImPlot::SetNextPlotFormatY("%g kHz");
        const float w  = ImGui::GetContentRegionAvail().x - 100 - ImGui::GetStyle().ItemSpacing.x;
        const float h = show_frame ? 320.0f : -1.0f;
        if (ImPlot::BeginPlot("##Plot1",nullptr,nullptr,{w,h},ImPlotFlags_NoMousePos,ImPlotAxisFlags_NoTickLabels,ImPlotAxisFlags_Lock)) {
            ImPlot::PlotHeatmap("##Heat",m_spectogram.data(),N_FRQ,N_BIN,m_min_db,m_max_db,NULL,{tmin,0},{tmax,m_fft_frq[N_FRQ-1]/1000});
            if (ImPlot::DragLineX("t",&m_time,true,{1,1,1,1}))
                seek((float)(m_time/m_duration));
            ImPlot::EndPlot();
        }
        ImGui::SameLine();
        ImPlot::ColormapScale("##Scale",m_min_db,m_max_db,{100,h},IMPLOT_AUTO,"%g dB");
        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
            ImGui::OpenPopup("Range");
        if (ImGui::BeginPopup("Range")) {
            ImGui::DragFloatRange2("Range [dB]",&m_min_db,&m_max_db);
            ImGui::EndPopup();
        }

        ImPlot::PushStyleVar(ImPlotStyleVar_PlotMinSize,{0,0});
        ImPlot::SetNextPlotLimits(0,1,-1,1,ImGuiCond_Always);
        if (ImPlot::BeginPlot("##Plot2",nullptr,nullptr,{-1,-1},ImPlotFlags_CanvasOnly,ImPlotAxisFlags_NoDecorations,ImPlotAxisFlags_NoDecorations)) {
            int idx = (int)((m_samples.size() - N_FFT) * m_t);
            idx -= idx % N_FFT;
            kiss_fftr(m_fft, &m_samples[idx], reinterpret_cast<kiss_fft_cpx*>(m_fft_out));
            auto getter1 = [](void* data, int i) {
                std::complex<float>* fft_out = (std::complex<float>*)data;
                double db = 20*log10(std::abs(fft_out[i]));
                double x = ImRemap01((double)i,0.0,(double)(N_FRQ-1));
                double y = ImRemap(db,-10.0,40.0,-1.0,1.0);
                return ImPlotPoint(x,y);
            };
            auto getter2 = [](void*, int i) {
                double x = ImRemap01((double)i,0.0,(double)(N_FRQ-1));
                return ImPlotPoint(x,-1.0);
            };
            ImPlot::SetNextFillStyle({1,1,1,0.1f});
            ImPlot::PlotShadedG("##FreqDomain",getter1,m_fft_out,getter2,nullptr,N_FRQ);
            ImPlot::SetNextLineStyle(ImPlot::SampleColormap(0.8f));
            ImPlot::PlotLine("##TimeDomain",&m_samples[idx],N_FFT,1.0/(N_FFT-1));
            ImPlot::EndPlot();
        }
        ImPlot::PopStyleVar();
        ImGui::End();
    };

    // Update spectogram matrix at playback time t = [0 1]
    void update_spectogram(float t) {
        int idx = (int)((m_samples.size() - N_BIN*N_FFT) * t);
        idx -= idx % N_FFT;
        for (int b = 0; b < N_BIN; ++b) {
            kiss_fftr(m_fft, &m_samples[idx], reinterpret_cast<kiss_fft_cpx*>(m_fft_out));
            for (int f = 0; f < N_FRQ; ++f)
                m_spectogram[f*N_BIN+b] = 20*log10f(std::abs(m_fft_out[N_FRQ-1-f]));
            idx += N_FFT;
        }
    }

    // Seek to playback time t = [0 1]
    void seek(float t) {
        m_t = t;
        if (!g_playing)
            update_spectogram(m_t);
        m_time = t * m_duration;
        auto frame = (int)((m_samples.size()-1) * t);
        {
            std::lock_guard<std::mutex> lock(g_mtx);
            ma_decoder_seek_to_pcm_frame(&m_decoder, frame);
        }
    }
};

// Main
int main(int argc, char const *argv[]) {
    if (argc < 2) {
        std::cout << "No input file provided!\n";
        return -1;
    }
    ImPlotSpectogram spec(argv[1]);
    spec.run();
    return 0;
}

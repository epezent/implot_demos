#include <kiss_fftr.h>
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include <stdlib.h>
#include <stdio.h>
#include <mutex>

#include "App.h"

std::mutex g_mtx;
std::vector<float> g_buffer1;
std::vector<float> g_buffer2;

void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    std::lock_guard<std::mutex> lock(g_mtx);
    const float* data = (const float*)pInput;
    // g_buffer1.clear();
    for (ma_uint32 i = 0; i < frameCount; ++i)
        g_buffer1.push_back(data[i]);
    (void)pOutput;
}

struct ImVoice : App {
    ImVoice(std::string title, int argc, char const *argv[]) :
        App(title,640,480,argc,argv)
    { 
        deviceConfig = ma_device_config_init(ma_device_type_capture);
        deviceConfig.capture.format   = ma_format_f32;
        deviceConfig.capture.channels = 1;
        deviceConfig.sampleRate       = 44100;
        deviceConfig.dataCallback     = data_callback;
        deviceConfig.pUserData        = NULL;

        result = ma_device_init(NULL, &deviceConfig, &device);
        if (result != MA_SUCCESS) {
            printf("Failed to initialize capture device.\n");
        }

        result = ma_device_start(&device);
        if (result != MA_SUCCESS) {
            ma_device_uninit(&device);
            printf("Failed to start device.\n");
        }
    }


    void Update() {
        static bool pause = false;

        if (ImGui::IsKeyPressed((ImGuiKey)GLFW_KEY_SPACE))
            pause = !pause;
        if (!pause && g_buffer1.size() >= 441) {
            std::lock_guard<std::mutex> lock(g_mtx);
            // std::cout << g_buffer.size() << std::endl;
            g_buffer2 = g_buffer1;
            g_buffer1.clear();
        }
        ImGui::Begin("Test");
        if (ImPlot::BeginPlot("Test",ImVec2(-1,-1))) {
            ImPlot::SetNextLineStyle(ImVec4(1,1,0,1));
            ImPlot::PlotLine("Voice",g_buffer2.data(),g_buffer2.size());
            ImPlot::EndPlot();
        }
        ImGui::End();
    }

    ~ImVoice() {
        ma_device_uninit(&device);
    }

    ma_result result;
    ma_device_config deviceConfig;
    ma_device device;
};

int main(int argc, char const *argv[])
{
    ImVoice voice("ImVoice",argc,argv);
    voice.Run();
    return 0;
}
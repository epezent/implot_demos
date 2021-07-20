#include "App.h"
#include "imgui_internal.h"

void StyleApp() {

    static const ImVec4 bg_dark      = ImVec4(0.15f, 0.16f, 0.21f, 1.00f);
    static const ImVec4 bg_mid       = ImVec4(0.20f, 0.21f, 0.27f, 1.00f);
    static const ImVec4 accent_dark  = ImVec4(0.292f, 0.360f, 0.594f, 1.000f);
    static const ImVec4 accent_light = ImVec4(0.635f, 0.686f, 0.863f, 1.000f);
    static const ImVec4 active       = ImVec4(0.107f, 0.118f, 0.157f, 1.000f);
    static const ImVec4 attention    = ImVec4(0.821f, 1.000f, 0.000f, 1.000f);

    auto& style = ImGui::GetStyle();
    style.WindowPadding = {6,6};
    style.FramePadding  = {6,3};
    style.CellPadding   = {6,3};
    style.ItemSpacing   = {6,6};
    style.ItemInnerSpacing = {6,6};
    style.ScrollbarSize = 16;
    style.GrabMinSize = 8;
    style.WindowBorderSize = style.ChildBorderSize = style.PopupBorderSize = style.TabBorderSize = 0;
    style.FrameBorderSize = 1;
    style.WindowRounding = style.ChildRounding = style.PopupRounding = style.ScrollbarRounding = style.GrabRounding = style.TabRounding = 4;

    ImVec4* colors = ImGui::GetStyle().Colors;
    colors[ImGuiCol_Text]                   = ImVec4(0.89f, 0.89f, 0.92f, 1.00f);
    colors[ImGuiCol_TextDisabled]           = ImVec4(0.38f, 0.45f, 0.64f, 1.00f);
    colors[ImGuiCol_WindowBg]               = bg_mid;
    colors[ImGuiCol_ChildBg]                = ImVec4(0.20f, 0.21f, 0.27f, 0.00f);
    colors[ImGuiCol_PopupBg]                = bg_mid;
    colors[ImGuiCol_Border]                 = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.06f);
    colors[ImGuiCol_FrameBg]                = ImVec4(1.00f, 1.00f, 1.00f, 0.02f);
    colors[ImGuiCol_FrameBgHovered]         = accent_light;
    colors[ImGuiCol_FrameBgActive]          = active;
    colors[ImGuiCol_TitleBg]                = accent_dark;
    colors[ImGuiCol_TitleBgActive]          = accent_dark;
    colors[ImGuiCol_TitleBgCollapsed]       = accent_dark;
    colors[ImGuiCol_MenuBarBg]              = accent_dark;
    colors[ImGuiCol_ScrollbarBg]            = bg_mid;
    colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.89f, 0.89f, 0.93f, 0.27f);
    colors[ImGuiCol_ScrollbarGrabHovered]   = accent_light;
    colors[ImGuiCol_ScrollbarGrabActive]    = active;
    colors[ImGuiCol_CheckMark]              = accent_dark;
    colors[ImGuiCol_SliderGrab]             = accent_dark;
    colors[ImGuiCol_SliderGrabActive]       = accent_light;
    colors[ImGuiCol_Button]                 = accent_dark;
    colors[ImGuiCol_ButtonHovered]          = accent_light;
    colors[ImGuiCol_ButtonActive]           = active;
    colors[ImGuiCol_Header]                 = accent_dark;
    colors[ImGuiCol_HeaderHovered]          = accent_light;
    colors[ImGuiCol_HeaderActive]           = active;
    colors[ImGuiCol_Separator]              = accent_dark;
    colors[ImGuiCol_SeparatorHovered]       = accent_light;
    colors[ImGuiCol_SeparatorActive]        = active;
    colors[ImGuiCol_ResizeGrip]             = accent_dark;
    colors[ImGuiCol_ResizeGripHovered]      = accent_light;
    colors[ImGuiCol_ResizeGripActive]       = active;
    colors[ImGuiCol_Tab]                    = ImVec4(1.00f, 1.00f, 1.00f, 0.02f);
    colors[ImGuiCol_TabHovered]             = accent_light;
    colors[ImGuiCol_TabActive]              = accent_dark;
    colors[ImGuiCol_TabUnfocused]           = ImVec4(0.24f, 0.23f, 0.29f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive]     = active;
    colors[ImGuiCol_PlotLines]              = accent_light;
    colors[ImGuiCol_PlotLinesHovered]       = active;
    colors[ImGuiCol_PlotHistogram]          = accent_light;
    colors[ImGuiCol_PlotHistogramHovered]   = active;
    colors[ImGuiCol_TableHeaderBg]          = accent_dark;
    colors[ImGuiCol_TableBorderStrong]      = ImVec4(1.00f, 1.00f, 1.00f, 0.10f);
    colors[ImGuiCol_TableBorderLight]       = ImVec4(1.00f, 1.00f, 1.00f, 0.02f);
    colors[ImGuiCol_TableRowBg]             = ImVec4(0,0,0,0);
    colors[ImGuiCol_TableRowBgAlt]          = ImVec4(1.00f, 1.00f, 1.00f, 0.02f);
    colors[ImGuiCol_TextSelectedBg]         = accent_light;
    colors[ImGuiCol_DragDropTarget]         = attention;
    colors[ImGuiCol_NavHighlight]           = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(1.00f, 0.98f, 0.95f, 0.73f);
#ifdef IMGUI_HAS_DOCK
    colors[ImGuiCol_DockingPreview]         = ImVec4(0.85f, 0.85f, 0.85f, 0.28f);
    colors[ImGuiCol_DockingEmptyBg]         = ImVec4(0.38f, 0.38f, 0.38f, 1.00f);
#endif

    ImVec4* pcolors = ImPlot::GetStyle().Colors;
    pcolors[ImPlotCol_Line]          = IMPLOT_AUTO_COL;
    pcolors[ImPlotCol_Fill]          = IMPLOT_AUTO_COL;
    pcolors[ImPlotCol_MarkerOutline] = IMPLOT_AUTO_COL;
    pcolors[ImPlotCol_MarkerFill]    = IMPLOT_AUTO_COL;
    pcolors[ImPlotCol_ErrorBar]      = IMPLOT_AUTO_COL;
    pcolors[ImPlotCol_FrameBg]       = IMPLOT_AUTO_COL;
    pcolors[ImPlotCol_PlotBg]        = ImVec4(0,0,0,0);
    pcolors[ImPlotCol_PlotBorder]    = ImVec4(0,0,0,0);
    pcolors[ImPlotCol_LegendBg]      = IMPLOT_AUTO_COL;
    pcolors[ImPlotCol_LegendBorder]  = IMPLOT_AUTO_COL;
    pcolors[ImPlotCol_LegendText]    = IMPLOT_AUTO_COL;
    pcolors[ImPlotCol_TitleText]     = IMPLOT_AUTO_COL;
    pcolors[ImPlotCol_InlayText]     = IMPLOT_AUTO_COL;
    pcolors[ImPlotCol_XAxis]         = IMPLOT_AUTO_COL;
    pcolors[ImPlotCol_XAxisGrid]     = IMPLOT_AUTO_COL;
    pcolors[ImPlotCol_YAxis]         = IMPLOT_AUTO_COL;
    pcolors[ImPlotCol_YAxisGrid]     = IMPLOT_AUTO_COL;
    pcolors[ImPlotCol_YAxis2]        = IMPLOT_AUTO_COL;
    pcolors[ImPlotCol_YAxisGrid2]    = IMPLOT_AUTO_COL;
    pcolors[ImPlotCol_YAxis3]        = IMPLOT_AUTO_COL;
    pcolors[ImPlotCol_YAxisGrid3]    = IMPLOT_AUTO_COL;
    pcolors[ImPlotCol_Selection]     = attention;
    pcolors[ImPlotCol_Query]         = ImVec4(0.000f, 1.000f, 0.866f, 1.000f);
    pcolors[ImPlotCol_Crosshairs]    = colors[ImGuiCol_Text];

    ImPlot::GetStyle().DigitalBitHeight = 20;

    auto& pstyle = ImPlot::GetStyle();
    pstyle.PlotPadding   = pstyle.LegendPadding = {12,12};
    pstyle.LabelPadding  = pstyle.LegendInnerPadding = {6,6};
    pstyle.LegendSpacing = {10,2};
}

#define APP_USE_OPENGL
#ifdef APP_USE_OPENGL

//-----------------------------------------------------------------------------
// OPENGL3
//-----------------------------------------------------------------------------

#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#if defined(IMGUI_IMPL_OPENGL_LOADER_GL3W)
#include <GL/gl3w.h>            // Initialize with gl3wInit()
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLEW)
#include <GL/glew.h>            // Initialize with glewInit()
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLAD)
#include <glad/glad.h>          // Initialize with gladLoadGL()
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLBINDING2)
#define GLFW_INCLUDE_NONE       // GLFW including OpenGL headers causes ambiguity or multiple definition errors.
#include <glbinding/Binding.h>  // Initialize with glbinding::Binding::initialize()
#include <glbinding/gl/gl.h>
using namespace gl;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLBINDING3)
#define GLFW_INCLUDE_NONE       // GLFW including OpenGL headers causes ambiguity or multiple definition errors.
#include <glbinding/glbinding.h>// Initialize with glbinding::initialize()
#include <glbinding/gl/gl.h>
using namespace gl;
#else
#include IMGUI_IMPL_OPENGL_LOADER_CUSTOM
#endif

#include <GLFW/glfw3.h>

#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif

static void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}


    App::App(int w, int h, std::string title, bool vsync) {
#ifdef _DEBUG
        title += " - OpenGL - Debug";
#else
        title += " - OpenGL";
#endif
        // Setup window
        glfwSetErrorCallback(glfw_error_callback);
        if (!glfwInit())
            abort();

        // Decide GL+GLSL versions
    #if __APPLE__
        // GL 3.2 + GLSL 150
        const char* glsl_version = "#version 150";
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
    #else
        // GL 3.0 + GLSL 130
        const char* glsl_version = "#version 130";
        glfwWindowHint(GLFW_SAMPLES, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        //glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
        //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
    #endif

        // Create window with graphics context
        Window = glfwCreateWindow(w, h, title.c_str(), NULL, NULL);
        if (Window == NULL) {
            fprintf(stderr, "Failed to initialize GLFW window!\n");
            abort();
        }
        glfwMakeContextCurrent(Window);
        glfwSwapInterval(vsync ? 1 : 0); // Enable vsync

        // Initialize OpenGL loader
    #if defined(IMGUI_IMPL_OPENGL_LOADER_GL3W)
        bool err = gl3wInit() != 0;
    #elif defined(IMGUI_IMPL_OPENGL_LOADER_GLEW)
        bool err = glewInit() != GLEW_OK;
    #elif defined(IMGUI_IMPL_OPENGL_LOADER_GLAD)
        bool err = gladLoadGL() == 0;
    #elif defined(IMGUI_IMPL_OPENGL_LOADER_GLBINDING2)
        bool err = false;
        glbinding::Binding::initialize();
    #elif defined(IMGUI_IMPL_OPENGL_LOADER_GLBINDING3)
        bool err = false;
        glbinding::initialize([](const char* name) { return (glbinding::ProcAddress)glfwGetProcAddress(name); });
    #else
        bool err = false; // If you use IMGUI_IMPL_OPENGL_LOADER_CUSTOM, your loader is likely to requires some form of initialization.
    #endif
        if (err)
        {
            fprintf(stderr, "Failed to initialize OpenGL loader!\n");
            abort();
        }

        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImPlot::CreateContext();
        StyleApp();
        ImGui_ImplGlfw_InitForOpenGL(Window, true);
        ImGui_ImplOpenGL3_Init(glsl_version);
        ImGuiIO& io = ImGui::GetIO(); 
        // auto rb = io.Fonts->AddFontFromFileTTF("fonts/Roboto-Bold.ttf", 15.0f);      
        // auto rr = io.Fonts->AddFontFromFileTTF("fonts/Roboto-Regular.ttf", 15.0f);
        // auto rm = io.Fonts->AddFontFromFileTTF("fonts/RobotoMono-Regular.ttf", 15.0f);
        // io.Fonts->AddFontDefault();  

        // add fonts
        io.Fonts->Clear();
        ImFontConfig font_cfg;
        font_cfg.FontDataOwnedByAtlas = false;

        ImFontConfig icons_config;
        icons_config.MergeMode            = true;
        icons_config.PixelSnapH           = true;
        icons_config.GlyphMinAdvanceX     = 14.0f;
        icons_config.GlyphOffset          = ImVec2(0, 0);
        icons_config.OversampleH          = 1;
        icons_config.OversampleV          = 1;
        icons_config.FontDataOwnedByAtlas = false;

        static const ImWchar fa_ranges[] = {ICON_MIN_FA, ICON_MAX_FA, 0};

        ImStrncpy(font_cfg.Name, "Roboto Bold", 40);
        Fonts[font_cfg.Name] = io.Fonts->AddFontFromMemoryTTF(Roboto_Bold_ttf, Roboto_Bold_ttf_len, 15.0f, &font_cfg);
        io.Fonts->AddFontFromMemoryTTF(fa_solid_900_ttf, fa_solid_900_ttf_len, 14.0f, &icons_config,fa_ranges);

        ImStrncpy(font_cfg.Name, "Roboto Italic", 40);
        Fonts[font_cfg.Name] = io.Fonts->AddFontFromMemoryTTF(Roboto_Italic_ttf, Roboto_Italic_ttf_len, 30, &font_cfg);
        io.Fonts->AddFontFromMemoryTTF(fa_solid_900_ttf, fa_solid_900_ttf_len, 14.0f, &icons_config,fa_ranges);

        ImStrncpy(font_cfg.Name, "Roboto Regular", 40);
        Fonts[font_cfg.Name] = io.Fonts->AddFontFromMemoryTTF(Roboto_Regular_ttf, Roboto_Regular_ttf_len, 30, &font_cfg);
        io.Fonts->AddFontFromMemoryTTF(fa_solid_900_ttf, fa_solid_900_ttf_len, 14.0f, &icons_config,fa_ranges);

        ImStrncpy(font_cfg.Name, "Roboto Mono Bold", 40);
        Fonts[font_cfg.Name] = io.Fonts->AddFontFromMemoryTTF(RobotoMono_Bold_ttf, RobotoMono_Bold_ttf_len, 30, &font_cfg);
        io.Fonts->AddFontFromMemoryTTF(fa_solid_900_ttf, fa_solid_900_ttf_len, 14.0f, &icons_config,fa_ranges);

        ImStrncpy(font_cfg.Name, "Roboto Mono Italic", 40);
        Fonts[font_cfg.Name] = io.Fonts->AddFontFromMemoryTTF(RobotoMono_Italic_ttf, RobotoMono_Italic_ttf_len, 30, &font_cfg);
        io.Fonts->AddFontFromMemoryTTF(fa_solid_900_ttf, fa_solid_900_ttf_len, 14.0f, &icons_config,fa_ranges);

        ImStrncpy(font_cfg.Name, "Roboto Mono Regular", 40);
        Fonts[font_cfg.Name] = io.Fonts->AddFontFromMemoryTTF(RobotoMono_Regular_ttf, RobotoMono_Regular_ttf_len, 30, &font_cfg);
        io.Fonts->AddFontFromMemoryTTF(fa_solid_900_ttf, fa_solid_900_ttf_len, 14.0f, &icons_config,fa_ranges);

        // ImGui::GetIO().BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
       
        // static const ImWchar fab_ranges[] = {ICON_MIN_FAB, ICON_MAX_FAB, 0};
        // io.Fonts->AddFontFromMemoryTTF(fa_brands_400_ttf, fa_brands_400_ttf_len, 14.0f, &icons_config, fab_ranges);
    }

    App::~App() {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImPlot::DestroyContext();
        ImGui::DestroyContext();
        glfwDestroyWindow(Window);
        glfwTerminate();
    }

    void App::run() {
        init();
        // Main loop
        while (!glfwWindowShouldClose(Window)) {
            glfwPollEvents();
            // Start the Dear ImGui frame
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            update();
            // Rendering
            ImGui::Render();
            int display_w, display_h;
            glfwGetFramebufferSize(Window, &display_w, &display_h);
            glViewport(0, 0, display_w, display_h);
            glClearColor(ClearColor.x, ClearColor.y, ClearColor.z, ClearColor.w);
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            glfwSwapBuffers(Window);
        }
    }

    ImVec2 App::get_window_size() const {
        int w, h;
        glfwGetWindowSize(Window, &w, &h);
        return ImVec2(w,h);
    }


#elif defined(APP_USE_DX11)

//-----------------------------------------------------------------------------
// WIN32 + DXll
//-----------------------------------------------------------------------------

#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <d3d11.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#ifdef _MSC_VER
#pragma comment(lib, "d3d11")
#endif
#include <tchar.h>

// Data
static ID3D11Device*            g_pd3dDevice = NULL;
static ID3D11DeviceContext*     g_pd3dDeviceContext = NULL;
static IDXGISwapChain*          g_pSwapChain = NULL;
static ID3D11RenderTargetView*  g_mainRenderTargetView = NULL;

// Helper functions
// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

bool CreateDeviceD3D(HWND hWnd)
{
    // Setup swap chain
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    //createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    if (D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext) != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = NULL; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = NULL; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = NULL; }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = NULL; }
}

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0 // From Windows SDK 8.1+ headers
#endif

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32 message handler
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED)
        {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    case WM_DPICHANGED:
        if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DpiEnableScaleViewports)
        {
            //const int dpi = HIWORD(wParam);
            //printf("WM_DPICHANGED to %d (%.0f%%)\n", dpi, (float)dpi / 96.0f * 100.0f);
            const RECT* suggested_rect = (RECT*)lParam;
            ::SetWindowPos(hWnd, NULL, suggested_rect->left, suggested_rect->top, suggested_rect->right - suggested_rect->left, suggested_rect->bottom - suggested_rect->top, SWP_NOZORDER | SWP_NOACTIVATE);
        }
        break;
    }
    return ::DefWindowProc(hWnd, msg, wParam, lParam);
}

    App::App(int w, int h, std::string title) {
        // Create application window
        //ImGui_ImplWin32_EnableDpiAwareness();
        wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, _T("ImGui Example"), NULL };
        ::RegisterClassEx(&wc);
#ifdef _DEBUG
        title += " - DX11 - Debug";
#else
        title += " - DX11";
#endif
        hwnd = ::CreateWindow(wc.lpszClassName, _T(title.c_str()), WS_OVERLAPPEDWINDOW, 100, 100, w, h, NULL, NULL, wc.hInstance, NULL);
        // Initialize Direct3D
        if (!CreateDeviceD3D(hwnd))
        {
            CleanupDeviceD3D();
            ::UnregisterClass(wc.lpszClassName, wc.hInstance);
            abort();
        }
        // Show the window
        ::ShowWindow(hwnd, SW_SHOWDEFAULT);
        ::UpdateWindow(hwnd);

        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImPlot::CreateContext();
        ImGui::StyleColorsDark();
        ImPlot::StyleColorsDark();
        ImGuiIO& io = ImGui::GetIO();
        // io.Fonts->AddFontDefault();
        io.Fonts->AddFontFromFileTTF("fonts/Roboto-Regular.ttf", 15.0f);
        io.Fonts->AddFontFromFileTTF("fonts/RobotoMono-Regular.ttf", 15.0f);
        ImGuiStyle& style = ImGui::GetStyle();
        ImGui_ImplWin32_Init(hwnd);
        ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);
    }

    App::~App() {
        // Cleanup
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImPlot::DestroyContext();
        ImGui::DestroyContext();
        CleanupDeviceD3D();
        ::DestroyWindow(hwnd);
        ::UnregisterClass(wc.lpszClassName, wc.hInstance);
    }

    void App::run() {
        MSG msg;
        ZeroMemory(&msg, sizeof(msg));
        while (msg.message != WM_QUIT)
        {
            if (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
            {
                ::TranslateMessage(&msg);
                ::DispatchMessage(&msg);
                continue;
            }

            ImGui_ImplDX11_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();

            update();

            // Rendering
            ImGui::Render();
            const float clear_color_with_alpha[4] = { ClearColor.x * ClearColor.w, ClearColor.y * ClearColor.w, ClearColor.z * ClearColor.w, ClearColor.w };
            g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
            g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
  
            // g_pSwapChain->Present(1, 0); // Present with vsync
            g_pSwapChain->Present(0, 0); // Present without vsync
        }

    }

#endif
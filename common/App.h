#include "Fonts/Fonts.h"
#include "Helpers.h"

#include "imgui.h"
#include "implot.h"

#include <string>
#include <map>

/// Macro to request high performance GPU in systems (usually laptops) with both
/// dedicated and discrete GPUs
#if defined(_WIN32) && defined(APP_USE_DGPU)
    extern "C" __declspec(dllexport) unsigned long NvOptimusEnablement = 1;
    extern "C" __declspec(dllexport) unsigned long AmdPowerXpressRequestHighPerformance = 1;
#endif

/// Macro to disable console on Windows
#if defined(_WIN32) && defined(APP_NO_CONSOLE)
    #pragma comment(linker, "/subsystem:windows /ENTRY:mainCRTStartup")
#endif

struct GLFWwindow;

// Barebones Application Framework
struct App 
{
    // Constructor.
    App(int w, int h, std::string title, bool vsync = true);
    // Destructor.
    virtual ~App();
    // Called at top of run
    virtual void init() { }
    // Update, called once per frame.
    virtual void update() { /*implement me*/ }
    // Runs the app.
    void run();
    // Get window size
    ImVec2 get_window_size() const;

    ImVec4 ClearColor = ImVec4(0.15f, 0.16f, 0.21f, 1.00f);  // background clear color
    GLFWwindow* Window;                                      // GLFW window handle
    std::map<std::string,ImFont*> Fonts;                     // font map
};
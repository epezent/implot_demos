cmake_minimum_required(VERSION 3.0.0)
project(implot_demos VERSION 0.1.0)

###############################################################################
# 3RD PARTY LIBS
###############################################################################

# FETCH CONTENT LIBS
include(FetchContent) 
# set(FETCHCONTENT_FULLY_DISCONNECTED ON)

FetchContent_Declare(glfw GIT_REPOSITORY https://github.com/glfw/glfw) 
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(iir GIT_REPOSITORY https://github.com/berndporr/iir1) 
FetchContent_MakeAvailable(iir)

FetchContent_Declare(curl GIT_REPOSITORY https://github.com/curl/curl) 
FetchContent_MakeAvailable(curl)

set(CPR_BUILD_TESTS_SSL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(cpr GIT_REPOSITORY https://github.com/whoshuu/cpr.git)
FetchContent_MakeAvailable(cpr)

# LOCAL LIBS
set(KISS_SRC "3rdparty/kissfft/kiss_fft.h" 
             "3rdparty/kissfft/kiss_fftr.h" 
             "3rdparty/kissfft/kiss_fft.c" 
             "3rdparty/kissfft/kiss_fftr.c")

include_directories("3rdparty" "3rdparty/kissfft")

###############################################################################
# IMGUI
###############################################################################

set(IMGUI_HEADERS
    ../imgui/imconfig.h
    ../imgui/imgui_internal.h
    ../imgui/imgui.h
    ../imgui/imstb_rectpack.h
    ../imgui/imstb_textedit.h
    ../imgui/imstb_truetype.h    
    ../imgui/examples/libs/gl3w/GL/gl3w.h
    ../imgui/examples/libs/gl3w/GL/glcorearb.h
    ../imgui/backends/imgui_impl_glfw.h
    ../imgui/backends/imgui_impl_opengl3.h
    ../imgui/misc/cpp/imgui_stdlib.h
)

set(IMGUI_SRC
    ../imgui/imgui_demo.cpp
    ../imgui/imgui_draw.cpp
    ../imgui/imgui_tables.cpp
    ../imgui/imgui_widgets.cpp
    ../imgui/imgui.cpp
    ../imgui/examples/libs/gl3w/GL/gl3w.c
    ../imgui/backends/imgui_impl_glfw.cpp
    ../imgui/backends/imgui_impl_opengl3.cpp
    ../imgui/misc/cpp/imgui_stdlib.cpp
)

find_package(OpenGL REQUIRED)

add_library(imgui ${IMGUI_HEADERS} ${IMGUI_SRC})
target_link_libraries(imgui PUBLIC glfw OpenGL::GL)
target_compile_definitions(imgui PRIVATE IMGUI_DLL_EXPORT)

include_directories(../imgui/ ../imgui/examples ../imgui/examples/libs/gl3w ../imgui/backends ../imgui/misc/cpp)

###############################################################################
# IMPLOT
###############################################################################

set(IMPLOT_HEADERS ../implot/implot.h ../implot/implot_internal.h)
set(IMPLOT_SRC     ../implot/implot.cpp ../implot/implot_items.cpp ../implot/implot_demo.cpp) 

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../implot/backends/implot_impl_opengl3.cpp")
  list(APPEND IMPLOT_SRC ../implot/backends/implot_impl_opengl3.cpp)
endif()

add_library(implot ${IMPLOT_HEADERS} ${IMPLOT_SRC})
target_link_libraries(implot PUBLIC imgui)
target_compile_definitions(implot PUBLIC IMPLOT_DEBUG IMPLOT_DLL_EXPORT IMPLOT_BACKEND_ENABLE_OPENGL3 IMGUI_IMPL_OPENGL_LOADER_GL3W)
set_property(TARGET implot PROPERTY CXX_STANDARD 11)
if(MSVC)
  target_compile_options(implot PRIVATE /W4 /WX /FA /O2 /arch:AVX2 /fp:fast )
  # set(MAX_INLINE_DEPTH 1000)
  # list(APPEND COMMON_FLAGS "$<$<STREQUAL:${CMAKE_CXX_COMPILER_ID},Clang>:-mllvm>")
  # list(APPEND COMMON_FLAGS "$<$<STREQUAL:${CMAKE_CXX_COMPILER_ID},Clang>:-inline-threshold=${MAX_INLINE_DEPTH}>")

else()
  target_compile_options(implot PRIVATE -Wall -Wextra -pedantic -Werror)
endif()

include_directories(../implot/)

###############################################################################
# APP FRAMEWORK
###############################################################################

add_library(app 
  common/App.h
  common/App.cpp
  common/Shader.h
  common/Fonts/Fonts.h
  common/Fonts/FontAwesome5.cpp
  common/Fonts/RobotoRegular.cpp
  common/Fonts/RobotoBold.cpp
  common/Fonts/RobotoItalic.cpp
  common/Fonts/RobotoMonoRegular.cpp
  common/Fonts/RobotoMonoBold.cpp
  common/Fonts/RobotoMonoItalic.cpp
)
target_include_directories(app PUBLIC common)
target_link_libraries(app implot)

###############################################################################
# TEST APPS
###############################################################################

add_executable(benchmarks "tests/benchmarks.cpp")
target_link_libraries(benchmarks app)
target_compile_options(benchmarks PUBLIC)

###############################################################################
# DEMO APPS
###############################################################################

# main ImPlot demo
add_executable(demo "demos/demo.cpp")
target_link_libraries(demo app)

add_executable(maps "demos/maps.cpp")
target_link_libraries(maps app cpr)

# GPU acceleration
add_executable(gpu "demos/gpu.cpp")
target_link_libraries(gpu app)
target_compile_features(gpu PRIVATE cxx_std_17)

# terrain visualizer with heatmaps
add_executable(terrain "demos/terrain.cpp")
target_link_libraries(terrain app)
target_include_directories(terrain PRIVATE "3rdparty")

add_executable(mandel "demos/mandel.cpp")
target_link_libraries(mandel app)
target_include_directories(mandel PRIVATE "3rdparty")
target_compile_options(mandel PRIVATE /FA /O2 /arch:AVX2 /fp:fast  /Zi /openmp)

# imdesmos demo
add_executable(imdesmos "demos/imdesmos.cpp")
target_link_libraries(imdesmos app)
target_include_directories(imdesmos PRIVATE "3rdparty")
set_target_properties(imdesmos PROPERTIES COMPILE_FLAGS "/bigobj")

# filter demo
add_executable(filter "demos/filter.cpp" ${KISS_SRC})
target_link_libraries(filter app iir::iir_static)
target_compile_features(filter PRIVATE cxx_std_17)
target_compile_definitions(filter PRIVATE kiss_fft_scalar=double NOMINMAX)

# spectrogram demo
add_executable(spectrogram "demos/spectrogram.cpp" ${KISS_SRC})              
target_link_libraries(spectrogram app)        
target_compile_features(spectrogram PRIVATE cxx_std_17)
add_custom_command(TARGET spectrogram POST_BUILD  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/resources/audio/aphex_twin_formula.wav ${CMAKE_CURRENT_BINARY_DIR}/aphex_twin_formula.wav)

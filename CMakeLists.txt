cmake_minimum_required(VERSION 3.0.0)
project(implot_demos VERSION 0.1.0)

###############################################################################
# 3RD PARTY LIBS
###############################################################################

# FETCH CONTENT LIBS
include(FetchContent) 
# set(FETCHCONTENT_FULLY_DISCONNECTED ON)

FetchContent_Declare(fmt GIT_REPOSITORY https://github.com/fmtlib/fmt)
FetchContent_MakeAvailable(fmt)

FetchContent_Declare(glfw GIT_REPOSITORY https://github.com/glfw/glfw) 
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(iir GIT_REPOSITORY https://github.com/berndporr/iir1) 
FetchContent_MakeAvailable(iir)

set(CMAKE_USE_SCHANNEL ON)

FetchContent_Declare(curl
URL                    https://github.com/curl/curl/releases/download/curl-7_75_0/curl-7.75.0.tar.xz
URL_HASH               SHA256=fe0c49d8468249000bda75bcfdf9e30ff7e9a86d35f1a21f428d79c389d55675 # the file hash for curl-7.75.0.tar.xz
USES_TERMINAL_DOWNLOAD TRUE)   # <---- This is needed only for Ninja to show download progress
FetchContent_MakeAvailable(curl)

add_library(curl_int INTERFACE)
target_link_libraries(curl_int INTERFACE libcurl)
target_include_directories(curl_int INTERFACE ${curl_SOURCE_DIR}/include ${curl_BINARY_DIR}/include/curl)
add_library(CURL::libcurl ALIAS curl_int)

# LOCAL LIBS

# KISS FFT
set(KISS_SRC "3rdparty/kissfft/kiss_fft.h" 
             "3rdparty/kissfft/kiss_fftr.h" 
             "3rdparty/kissfft/kiss_fft.c" 
             "3rdparty/kissfft/kiss_fftr.c")

# EVERYTHING ELSE
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
if(MSVC)
  target_compile_options(imgui PRIVATE /W4 /WX /FA /arch:AVX2 /fp:fast)
endif()
target_link_libraries(imgui PUBLIC glfw OpenGL::GL imm32)
target_compile_definitions(imgui PRIVATE IMGUI_DLL_EXPORT)

include_directories(../imgui/ ../imgui/examples ../imgui/examples/libs/gl3w ../imgui/backends ../imgui/misc/cpp)

###############################################################################
# IMPLOT
###############################################################################

set(IMPLOT_HEADERS ../implot/implot.h ../implot/implot_internal.h)
set(IMPLOT_SRC     ../implot/implot.cpp ../implot/implot_items.cpp ../implot/implot_demo.cpp) 

add_library(implot ${IMPLOT_HEADERS} ${IMPLOT_SRC})
target_link_libraries(implot PUBLIC imgui)
target_compile_definitions(implot PUBLIC IMPLOT_DEBUG IMPLOT_DLL_EXPORT IMPLOT_BACKEND_ENABLE_OPENGL3 IMGUI_IMPL_OPENGL_LOADER_GL3W)
set_property(TARGET implot PROPERTY CXX_STANDARD 11)
if(MSVC)
  target_compile_options(implot PRIVATE /W4 /WX /FA /arch:AVX2 /fp:fast /O2) 
else()
  target_compile_options(implot PRIVATE -Wall -Wextra -pedantic -Werror -mavx2 -Ofast)
endif()

include_directories(../implot/)

###############################################################################
# APP FRAMEWORK
###############################################################################

add_library(app 
  common/App.h
  common/App.cpp
  common/Shader.h
  common/Image.h
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
# DEMO APPS
###############################################################################

# main ImPlot demo
add_executable(demo "demos/demo.cpp")
target_link_libraries(demo app)

# maps demo
add_executable(maps "demos/maps.cpp")
target_link_libraries(maps app CURL::libcurl)
target_compile_features(maps PRIVATE cxx_std_17)

# perlin demo
add_executable(perlin "demos/perlin.cpp")
target_link_libraries(perlin app)
target_include_directories(perlin PRIVATE "3rdparty")

# mandel demo
add_executable(mandel "demos/mandel.cpp")
target_link_libraries(mandel app)
target_include_directories(mandel PRIVATE "3rdparty")
if (MSVC)
  target_compile_options(mandel PRIVATE /FA /arch:AVX2 /fp:fast /openmp)
endif()

# graph demo
add_executable(graph "demos/graph.cpp")
target_link_libraries(graph app)
target_include_directories(graph PRIVATE "3rdparty")
if (MSVC)
  set_target_properties(graph PROPERTIES COMPILE_FLAGS "/bigobj")
endif()

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

# stocks demo
add_executable(stocks "demos/stocks.cpp" )
target_link_libraries(stocks app fmt::fmt CURL::libcurl)
target_compile_features(stocks PRIVATE cxx_std_17)

###############################################################################
# TEST APPS
###############################################################################

add_executable(optimizations "tests/optimizations.cpp")
target_link_libraries(optimizations app)
target_compile_features(optimizations PRIVATE cxx_std_17)
if (MSVC)
  target_compile_options(optimizations PRIVATE /FA /arch:AVX2 /fp:fast /O2)
else()
  target_compile_options(optimizations PRIVATE -lstdc++fs -mavx2 -Ofast)
endif()
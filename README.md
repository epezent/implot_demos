# ImPlot Demo Applications

## Requirements

- C++17 compiler

## Build Instructions
- Clone this repository, [ImPlot](https://github.com/epezent/implot), and [ImGui](https://github.com/ocornut/imgui) into side-by-side folders:
- `root/`
    - `imgui/`
    - `implot/`
    - `implot_demos/`
- build with CMake:
```shell
cd implot_demos
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

## Demos

|Demo|Description|
|---|---|
|`demo.cpp`|Main demo. Displays the ImPlot and ImGui library supplied demo windows.|
|`spectogram.cpp`|Realtime audio spectogram and visualizer with device playback. Pass in an audio file at the command line: `./spectogram.exe ./aphex_twin_formula.wav`

**Note:** Ensure that the `./resources/fonts/` folder is copied to you binary output location!
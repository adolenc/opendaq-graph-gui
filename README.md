openDAQ Graph GUI
=================

A graph-based user interface for [openDAQ](https://opendaq.com).

![Image](https://github.com/user-attachments/assets/a600e20e-7e2f-4e01-92f5-5312948fa25a)

## Features

 - Nice visualizations for complex dataflows
 - Responsive user interface
 - Edit properties of multiple components at once
 - Automatic signal plotting for selected components
 - Quick signal previews on hover
 - Simple connecting and disconnecting of signals
 - Trivial adding of nested components
 - Color-coded components based parent device

## Getting Started

Prebuilt binaries are available for 64 bit Windows and Linux, on other
platforms you will need to build the project from source. The prebuilt
binaries come with reference function blocks and devices from openDAQ.

### Linux (amd64)
Download the latest artifacts from CI
[here](https://github.com/adolenc/opendaq-node-gui/actions) (click on the
latest successful build and then download the opendaq-gui-linux.zip
artifact).

Unzip the downloaded archive and run `LD_LIBRARY_PATH=. ./opendaq-gui`.

### Windows (x64)
Download the latest artifacts from CI
[here](https://github.com/adolenc/opendaq-node-gui/actions) (click on the
latest successful build and then download the opendaq-gui-windows.zip
artifact).

Unzip the downloaded archive and run `opendaq-gui.exe`.

> [!NOTE]
> If you are having trouble running the application, make sure you have the latest
> [Visual C++ Redistributable](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist)
> installed.

### Building From Source

OpenGL dev library is required to build this project.
Other dependencies are fetched and built automatically using CMake.
Clone the repository and run the following commands from the project directory:

```sh
mkdir build
cd build
cmake ..
make
./bin/opendaq-gui
```

## Acknowledgements

 - [openDAQ](https://opendaq.com)
 - [SDL2](https://www.libsdl.org/) for window management and input handling
 - [Dear ImGui](https://github.com/ocornut/imgui) for the overall GUI
 - ImGui node editor was adapted from [this gist](https://gist.github.com/ChemistAion/0cd64b71711d81661344af040c142c1c)
 - [ImPlot](https://github.com/epezent/implot) for plotting data
 - [ImSearch](https://github.com/GuusKemperman/imsearch) for searchable combo boxes

## License

Copyright (c) 2025 Andrej Dolenc

Licensed under the MIT License.

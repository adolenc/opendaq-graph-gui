openDAQ Graph GUI
=================

A graph-based user interface for [openDAQ](https://opendaq.com).

![Image](https://github.com/user-attachments/assets/a600e20e-7e2f-4e01-92f5-5312948fa25a)

## Features

 - Great for overviews of complex dataflows
 - Responsive user interface with immediate feedback
 - Edit properties of multiple components at once
 - Automatic signal plotting for selected components
 - Quick signal previews on hover
 - Simple connecting and disconnecting of signals
 - Trivial adding of nested function blocks or devices
 - Color-coded components based on their parent device
 - Errors and warnings discoverable at glance

## Getting Started

### Prebuilt Binaries

Nightly prebuilt binaries are available for 64 bit Windows, Linux, and macOS
(Apple Silicon) on the
[releases page](https://github.com/adolenc/opendaq-graph-gui/releases), on other
platforms you will need to build the project from source. The prebuilt binaries
come with reference function blocks and devices from openDAQ.

### Building From Source

OpenGL dev library is required to build this project.
Other dependencies are fetched and built automatically using CMake.

E.g. to build on Linux, clone the repository and run the following commands
from the project directory:

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

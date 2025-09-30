## Building from source

Installed OpenGL dev library is required to build this project.
Other dependencies (SDL2, imgui, implot, imsearch, openDAQ) are fetched and built automatically using CMake:

```sh
mkdir build
cd build
cmake ..
make
./bin/opendaq-gui
```

## Acknowledgements
ImGui node editor was adapted from [this gist](https://gist.github.com/ChemistAion/0cd64b71711d81661344af040c142c1c) by [ChemistAion](https://github.com/ChemistAion).

## License

Copyright (c) 2025 Andrej Dolenc

Licensed under the MIT License.

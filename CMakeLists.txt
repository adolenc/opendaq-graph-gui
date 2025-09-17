cmake_minimum_required(VERSION 3.11)
project(opendaq-gui)
set(CMAKE_CXX_STANDARD 17)

# opendaq - with cpptrace patch applied
include(FetchContent)
FetchContent_Declare(
  openDAQ
  GIT_REPOSITORY https://github.com/openDAQ/openDAQ.git
  GIT_TAG v3.20.4
  PATCH_COMMAND git apply --ignore-whitespace ${CMAKE_CURRENT_SOURCE_DIR}/patches/opendaq-cpptrace.patch
)
set(OPENDAQ_ENABLE_TESTS OFF CACHE BOOL "")
set(OPENDAQ_ENABLE_TEST_UTILS OFF CACHE BOOL "")
set(OPENDAQ_ENABLE_ACCESS_CONTROL OFF CACHE BOOL "")
set(DAQMODULES_REF_DEVICE_MODULE ON CACHE BOOL "")
set(DAQMODULES_REF_FB_MODULE ON CACHE BOOL "")
set(DAQMODULES_REF_FB_MODULE_ENABLE_RENDERER OFF CACHE BOOL "")
FetchContent_MakeAvailable(openDAQ)

# imgui
FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG v1.92.0
)
FetchContent_MakeAvailable(imgui)
add_library(imgui STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_demo.cpp
  ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp

  ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/misc/cpp ${imgui_SOURCE_DIR}/backends)

# implot
FetchContent_Declare(
  implot
  GIT_REPOSITORY https://github.com/epezent/implot.git
  GIT_TAG master
)
FetchContent_MakeAvailable(implot)
add_library(implot STATIC
  ${implot_SOURCE_DIR}/implot.cpp
  ${implot_SOURCE_DIR}/implot_items.cpp
  ${implot_SOURCE_DIR}/implot_demo.cpp
)
target_include_directories(implot PUBLIC ${implot_SOURCE_DIR} ${imgui_SOURCE_DIR})

# sdl2 and opengl
find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)
target_link_libraries(imgui PUBLIC SDL2::SDL2 SDL2::SDL2main OpenGL::GL) 


add_executable(${PROJECT_NAME} main.cpp nodes.cpp opendaq_control.cpp)
target_include_directories(${PROJECT_NAME}
  PUBLIC
  ${openDAQ_SOURCE_DIR}/../coretypes/include
  ${openDAQ_BINARY_DIR}/../coretypes/include
  ${openDAQ_SOURCE_DIR}/../coreobjects/include
  ${openDAQ_BINARY_DIR}/../coreobjects/include
  ${openDAQ_SOURCE_DIR}/../corecontainers/include
  ${openDAQ_SOURCE_DIR}/opendaq/include
  ${openDAQ_BINARY_DIR}/opendaq/include

  ${openDAQ_BINARY_DIR}/context/include
  ${openDAQ_BINARY_DIR}/functionblock/include
  ${openDAQ_BINARY_DIR}/modulemanager/include
  ${openDAQ_BINARY_DIR}/reader/include
  ${openDAQ_BINARY_DIR}/server/include
  ${openDAQ_BINARY_DIR}/streaming/include
  ${openDAQ_BINARY_DIR}/utility/include
  ${openDAQ_BINARY_DIR}/component/include
  ${openDAQ_BINARY_DIR}/device/include
  ${openDAQ_BINARY_DIR}/logger/include
  ${openDAQ_BINARY_DIR}/opendaq/include
  ${openDAQ_BINARY_DIR}/scheduler/include
  ${openDAQ_BINARY_DIR}/signal/include
  ${openDAQ_BINARY_DIR}/synchronization/include

  ${openDAQ_SOURCE_DIR}/context/include
  ${openDAQ_SOURCE_DIR}/functionblock/include
  ${openDAQ_SOURCE_DIR}/modulemanager/include
  ${openDAQ_SOURCE_DIR}/reader/include
  ${openDAQ_SOURCE_DIR}/server/include
  ${openDAQ_SOURCE_DIR}/streaming/include
  ${openDAQ_SOURCE_DIR}/utility/include
  ${openDAQ_SOURCE_DIR}/component/include
  ${openDAQ_SOURCE_DIR}/device/include
  ${openDAQ_SOURCE_DIR}/logger/include
  ${openDAQ_SOURCE_DIR}/opendaq/include
  ${openDAQ_SOURCE_DIR}/scheduler/include
  ${openDAQ_SOURCE_DIR}/signal/include
  ${openDAQ_SOURCE_DIR}/synchronization/include

  build/__external/src/fmt/include
  build/__external/src/xxHash
  build/__external/src/tsl-ordered-map/include
  build/__external/src/date/include
)
target_link_libraries(${PROJECT_NAME} PUBLIC daq::opendaq imgui implot)

cmake_minimum_required(VERSION 3.11)
project(opendaq-gui)
set(CMAKE_CXX_STANDARD 17)
include(FetchContent)

# build everything into the bin/ folder (otherwise Windows builds into different folders)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# opendaq - with cpptrace patch applied on linux
set(OPENDAQ_PATCH_COMMAND)
if (NOT WIN32)
  set(OPENDAQ_PATCH_COMMAND
    PATCH_COMMAND git apply --ignore-whitespace ${CMAKE_CURRENT_SOURCE_DIR}/patches/opendaq-cpptrace.patch || echo "Patch already applied"
  )
endif()
FetchContent_Declare(
  openDAQ
  GIT_REPOSITORY https://github.com/openDAQ/openDAQ.git
  GIT_TAG v3.20.4
  ${OPENDAQ_PATCH_COMMAND}
)
set(OPENDAQ_ENABLE_TESTS OFF CACHE BOOL "")
set(OPENDAQ_ENABLE_TEST_UTILS OFF CACHE BOOL "")
set(OPENDAQ_ENABLE_ACCESS_CONTROL OFF CACHE BOOL "")
set(OPENDAQ_ENABLE_NATIVE_STREAMING ON CACHE BOOL "")
set(DAQMODULES_OPENDAQ_CLIENT_MODULE ON CACHE BOOL "")
set(DAQMODULES_REF_DEVICE_MODULE ON CACHE BOOL "")
set(DAQMODULES_REF_FB_MODULE ON CACHE BOOL "")
set(DAQMODULES_REF_FB_MODULE_ENABLE_RENDERER OFF CACHE BOOL "")
FetchContent_MakeAvailable(openDAQ)

# imgui
FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG v1.92.0
  PATCH_COMMAND git apply --ignore-whitespace ${CMAKE_CURRENT_SOURCE_DIR}/patches/imgui-32bit-vertices.patch || echo "Patch already applied"
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
  PATCH_COMMAND git apply --ignore-whitespace ${CMAKE_CURRENT_SOURCE_DIR}/patches/implot-locked-axis.patch || echo "Patch already applied"
)
FetchContent_MakeAvailable(implot)
add_library(implot STATIC
  ${implot_SOURCE_DIR}/implot.cpp
  ${implot_SOURCE_DIR}/implot_items.cpp
  ${implot_SOURCE_DIR}/implot_demo.cpp
)
target_include_directories(implot PUBLIC ${implot_SOURCE_DIR} ${imgui_SOURCE_DIR})

# imsearch
FetchContent_Declare(
  imsearch
  GIT_REPOSITORY https://github.com/GuusKemperman/imsearch.git
  GIT_TAG main
)
FetchContent_MakeAvailable(imsearch)
add_library(imsearch STATIC
  ${imsearch_SOURCE_DIR}/imsearch.cpp
  ${imsearch_SOURCE_DIR}/imsearch_demo.cpp
)
target_include_directories(imsearch PUBLIC ${imsearch_SOURCE_DIR} ${imgui_SOURCE_DIR})

# sdl2
FetchContent_Declare(
  SDL2
  GIT_REPOSITORY  https://github.com/libsdl-org/SDL
  GIT_TAG         release-2.32.10
)
FetchContent_MakeAvailable(SDL2)

find_package(OpenGL REQUIRED)
target_include_directories(imgui PUBLIC ${SDL2_INCLUDE_DIRS})
target_link_libraries(imgui PUBLIC SDL2::SDL2 SDL2::SDL2main OpenGL::GL) 


add_executable(${PROJECT_NAME} main.cpp nodes.cpp opendaq_control.cpp properties_window.cpp component_cache.cpp signals_window.cpp signal.cpp)
target_include_directories(${PROJECT_NAME} PUBLIC ${openDAQ_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PUBLIC daq::opendaq imgui implot imsearch)

add_custom_command(
  TARGET ${PROJECT_NAME} POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_SOURCE_DIR}/fonts/Roboto-Medium.ttf
    $<TARGET_FILE_DIR:${PROJECT_NAME}>)

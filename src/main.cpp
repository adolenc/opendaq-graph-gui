#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h"
#include "implot.h"
#include "imsearch.h"
#include "ImGuiNotify.hpp"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include "opendaq_control.h"
#include "nodes.h"
#include <SDL.h>
#include <SDL_opengl.h>
#include <opendaq/version.h>
#include <string>


int main(int argc, char** argv)
{
    std::string connection_string;
    bool light_mode = false;
    for (int i = 1; i < argc; i++)
    {
        std::string arg = argv[i];
        if (arg == "--version" || arg == "-v")
        {
            unsigned int major, minor, revision;
            daqOpenDaqGetVersion(&major, &minor, &revision);
            printf("openDAQ graph GUI %s\nUsing openDAQ v%u.%u.%u\n", GIT_SHA, major, minor, revision);
            return 0;
        }
        else if ((arg == "--connection_string" || arg == "--connection-string" || arg == "-c") && i + 1 < argc)
        {
            i += 1;
            connection_string = argv[i];
        }
        else if (arg == "--light-mode")
        {
            light_mode = true;
        }
        else if (arg == "--help" || arg == "-h")
        {
            printf("Usage: %s [options]\n", argv[0]);
            printf("Options:\n");
            printf("  --connection-string, -c <string>   Connect directly to a device (usually daq.nd://<ip>).\n");
            printf("  --light-mode                       Use light theme instead of dark theme.\n");
            printf("  --version, -v                      Display program and openDAQ version.\n");
            printf("  --help, -h                         Show this help message.\n");
            return 0;
        }
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
    {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

    OpenDAQNodeEditor opendaq_editor;
    ImGui::ImGuiNodes nodes_editor(&opendaq_editor);
    opendaq_editor.nodes_ = &nodes_editor;
    opendaq_editor.Init();
    
    if (connection_string == "demo")
    {
        daq::DevicePtr dev = opendaq_editor.instance_.addDevice("daqref://device0");
        auto stat = opendaq_editor.instance_.addFunctionBlock("RefFBModuleStatistics");
        auto power = opendaq_editor.instance_.addFunctionBlock("RefFBModulePower");
        auto a = dev.addFunctionBlock("RefFBModuleStatistics");
        a.addFunctionBlock("RefFBModuleTrigger");
        dev.addFunctionBlock("RefFBModulePower");
        stat.getInputPorts()[0].connect(power.getSignals()[0]);
        daq::DevicePtr dev2 = dev.addDevice("daqref://device1");
        auto fft = dev2.addFunctionBlock("RefFBModuleFFT");
        fft.getInputPorts()[0].connect(dev.getSignalsRecursive()[1]);
        auto power2 = dev.addFunctionBlock("RefFBModulePower");
        power2.getInputPorts()[0].connect(stat.getSignals()[0]);
        auto csv = opendaq_editor.instance_.addFunctionBlock("BasicCsvRecorder");
        csv.getInputPorts()[0].connect(dev.getSignalsRecursive()[0]);
    }
    else if (!connection_string.empty())
    {
        opendaq_editor.instance_.addDevice(connection_string);
    }

    const char* glsl_version = "#version 150";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);

    SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
#ifndef NDEBUG
    SDL_Window* window = SDL_CreateWindow("floating openDAQ Graph GUI", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 2200, 1600, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
#else
    SDL_Window* window = SDL_CreateWindow("openDAQ Graph GUI", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1200, 800, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
#endif
    if (!window)
    {
        printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
        return -1;
    }

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    opendaq_editor.InitImGui();
    ImPlot::CreateContext();
    ImSearch::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    if (char *base_path = SDL_GetBasePath())
    {
        io.Fonts->AddFontFromFileTTF((std::string(base_path) + "Roboto-Medium.ttf").c_str(), 14.0f);
        
        static const ImWchar icons_ranges[] = { 0xf000, 0xf8ff, 0 };
        ImFontConfig icons_config;
        icons_config.MergeMode = true;
        icons_config.PixelSnapH = true;
        io.Fonts->AddFontFromFileTTF((std::string(base_path) + "fa-solid-900.ttf").c_str(), 14.0f, &icons_config, icons_ranges);

        SDL_free(base_path);
    }
    else
    {
        io.Fonts->AddFontFromFileTTF("Roboto-Medium.ttf", 14.0f);

        static const ImWchar icons_ranges[] = { 0xf000, 0xf8ff, 0 };
        ImFontConfig icons_config;
        icons_config.MergeMode = true;
        icons_config.PixelSnapH = true;
        io.Fonts->AddFontFromFileTTF("fa-solid-900.ttf", 14.0f, &icons_config, icons_ranges);
    }

    if (light_mode)
    {
        ImGui::StyleColorsLight();
        ImPlot::StyleColorsLight();
    }
    else
    {
        ImGui::StyleColorsDark();
        ImPlot::StyleColorsDark();
    }
    ImPlot::GetStyle().UseISO8601 = true;

    ImGui::GetStyle().Colors[ImGuiCol_DragDropTarget] = ImGui::GetStyle().Colors[ImGuiCol_NavHighlight];

    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    bool done = false;
    while (!done)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                done = true;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
                done = true;
#ifndef NDEBUG
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)
                done = true;
#endif
        }

        if (!(SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS)) // don't waste CPU if not focused
            SDL_Delay(100);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("DockSpace Window", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoBackground);
        ImGui::PopStyleVar(3);

        ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
        static bool first_time = true;
        if (first_time)
        {
            if (ImGui::DockBuilderGetNode(dockspace_id) == NULL)
            {
                ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
                ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->WorkSize);

                ImGuiID dock_main_id = dockspace_id;
                ImGuiID dock_id_left = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.16f, nullptr, &dock_main_id);
                ImGuiID dock_id_bottom = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.30f, nullptr, &dock_main_id);
                ImGuiID dock_id_right = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.25f, nullptr, &dock_main_id);

                ImGui::DockBuilderDockWindow("Nodes", dock_main_id);
                ImGui::DockBuilderDockWindow("Tree", dock_id_left);
                ImGui::DockBuilderDockWindow("Properties", dock_id_right);
                ImGui::DockBuilderDockWindow("Signals", dock_id_bottom);
                ImGui::DockBuilderFinish(dockspace_id);
            }
            first_time = false;
        }

        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
        ImGui::End();

        opendaq_editor.Render();
        if (connection_string.empty())
            opendaq_editor.ShowStartupPopup();

#ifndef NDEBUG
        ImGui::ShowDemoWindow();
#endif

        if (ImGui::Begin("Nodes", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
        {
            nodes_editor.Update();
            opendaq_editor.RenderNestedNodePopup();
        }
        ImGui::End();

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
        ImGui::RenderNotifications();
        ImGui::PopStyleVar(2);

        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImSearch::DestroyContext();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}

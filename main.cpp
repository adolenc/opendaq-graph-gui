#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "implot.h"
#include "imsearch.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include "properties_window.h"
#include "opendaq_control.h"
#include <stdio.h>
#include "nodes.h"
#include <SDL.h>
#include <SDL_opengl.h>
#include <opendaq/version.h>
#include <cstring>

int main(int argc, char** argv)
{
    if (argc > 1 && (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0))
    {
        unsigned int major, minor, revision;
        daqOpenDaqGetVersion(&major, &minor, &revision);
        printf("openDAQ GUI - built with openDAQ v%u.%u.%u\n", major, minor, revision);
        return 0;
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
    {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

    OpenDAQNodeEditor opendaq_editor;
    ImGui::ImGuiNodes nodes_editor(&opendaq_editor);
    opendaq_editor.nodes_ = &nodes_editor;
    
    daq::DevicePtr dev = opendaq_editor.instance_.addDevice(opendaq_editor.instance_.getAvailableDevices()[0].getConnectionString());
    auto stat = opendaq_editor.instance_.addFunctionBlock("RefFBModuleStatistics");
    auto power = opendaq_editor.instance_.addFunctionBlock("RefFBModulePower");
    auto a = dev.addFunctionBlock("RefFBModuleStatistics");
    a.addFunctionBlock("RefFBModuleTrigger");
    dev.addFunctionBlock("RefFBModulePower");
    stat.getInputPorts()[0].connect(power.getSignals()[0]);
    auto power2 = dev.addFunctionBlock("RefFBModulePower");
    power2.getInputPorts()[0].connect(stat.getSignals()[0]);

    const char* glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

    SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_Window* window = SDL_CreateWindow("floating opendaq gui demo", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 2000, 1200, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
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
    ImPlot::CreateContext();
    ImSearch::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    if (char *base_path = SDL_GetBasePath())
    {
        io.Fonts->AddFontFromFileTTF((std::string(base_path) + "Roboto-Medium.ttf").c_str(), 14.0f);
        SDL_free(base_path);
    }
    else
        io.Fonts->AddFontFromFileTTF("Roboto-Medium.ttf", 14.0f);

    ImGui::StyleColorsDark();
    ImPlot::GetStyle().UseISO8601 = true;

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
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)
                done = true;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
                done = true;
        }
        if (!(SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS)) // don't waste CPU if not focused
        {
            SDL_Delay(10);
            continue;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        static bool initialized = false;
        if (!initialized)
        {
            nodes_editor.BeginBatchAdd();
            opendaq_editor.RetrieveTopology(opendaq_editor.instance_);
            nodes_editor.EndBatchAdd();
            opendaq_editor.RetrieveConnections();
            nodes_editor.SetWarning(power.getGlobalId().toStdString(), "This is a warning message");
            initialized = true;
        }

        DrawPropertiesWindow(opendaq_editor.selected_components_);
        if (false)
            opendaq_editor.ShowStartupPopup();
        ImGui::ShowDemoWindow();

#ifdef IMGUI_HAS_VIEWPORT
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->GetWorkPos());
        ImGui::SetNextWindowSize(viewport->GetWorkSize());
        ImGui::SetNextWindowViewport(viewport->ID);
#else
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
#endif
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        if (ImGui::Begin("Node Editor", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus))
        {
            nodes_editor.Update();
            opendaq_editor.RenderNestedNodePopup();
        }
        ImGui::End();
        ImGui::PopStyleVar(1);

        // Rendering
        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    // Cleanup
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

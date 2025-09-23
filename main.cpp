#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_stdlib.h"
#include "implot.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include "properties_window.h"
#include "opendaq_control.h"
#include <stdio.h>
#include "nodes.h"
#include <SDL.h>
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <SDL_opengles2.h>
#else
#include <SDL_opengl.h>
#endif

// This example can also compile and run with Emscripten! See 'Makefile.emscripten' for details.
#ifdef __EMSCRIPTEN__
#include "../libs/emscripten/emscripten_mainloop_stub.h"
#endif


class NodeInteractionHandler : public ImGui::ImGuiNodesInteractionHandler
{
public:
    NodeInteractionHandler(OpenDAQHandler* opendaq_handler)
        : opendaq_handler_(opendaq_handler)
    {
    };

    void OnOutputHover(const ImGui::ImGuiNodesUid& id) override
    {
        static int64_t start_time{-1};
        static daq::TailReaderPtr reader;
        static float values[5000]{};
        static int64_t times_int[5000]{};
        static float times[5000]{};
        static std::string signal_name{""};
        static std::string signal_unit{""};

        if (id == "")
        {
            // std::cout << "Clearing reader" << std::endl;
            reader = nullptr;
            start_time = -1;
            return;
        }

        if (ImGui::BeginTooltip())
        {
            if (reader == nullptr || !reader.assigned())
            {
                // std::cout << "Making new reader" << std::endl;
                daq::SignalPtr signal = opendaq_handler_->signals_[id].component_.as<daq::ISignal>();
                reader = daq::TailReaderBuilder()
                    .setSignal(signal)
                    .setHistorySize(5000)
                    .setValueReadType(daq::SampleType::Float32)
                    .setDomainReadType(daq::SampleType::Int64)
                    .setSkipEvents(true)
                    .build();

                signal_name = signal.getName().toStdString();
                if (signal.getDescriptor().assigned() && signal.getDescriptor().getUnit().assigned() && signal.getDescriptor().getUnit().getSymbol().assigned())
                    signal_unit = signal.getDescriptor().getUnit().getSymbol().toStdString();
                else
                    signal_unit = "";

                start_time = -1;
            }

            daq::SizeT count{5000};
            if (reader != nullptr && reader.assigned())
                reader.readWithDomain(values, times_int, &count);
            else
                count = 0;

            ImGui::Text("%s [%s]", signal_name.c_str(), signal_unit.c_str());
            if (count)
            {
                if (start_time == -1)
                    start_time = times_int[0];
                for (int i = 0; i < count; i++)
                    times[i] = static_cast<float>(times_int[i] - start_time);

                static ImPlotAxisFlags flags = /*ImPlotAxisFlags_NoTickLabels | */ImPlotAxisFlags_AutoFit;
                if (ImPlot::BeginPlot("##Scrolling", ImVec2(800,300), ImPlotFlags_NoLegend))
                {
                    ImPlot::SetupAxes(nullptr, nullptr, flags | ImPlotAxisFlags_NoTickLabels, flags);
                    ImPlot::SetupAxisLimits(ImAxis_X1,times[0], times[count-1], ImGuiCond_Always);
                    ImPlot::SetupAxisLimits(ImAxis_Y1,-5,5);
                    ImPlot::PlotLine("", times, values, (int)count);
                    ImPlot::EndPlot();
                }
            }
            else
            {
                if (ImPlot::BeginPlot("##Scrolling", ImVec2(800,300), ImPlotFlags_NoLegend))
                    ImPlot::EndPlot();
            }
            ImGui::EndTooltip();
        }
    }

    void OnSelectionChanged(const std::vector<ImGui::ImGuiNodesUid>& selected_ids) override
    {
        selected_components_.clear();
        for (ImGui::ImGuiNodesUid id : selected_ids)
        {
            if (auto it = opendaq_handler_->folders_.find(id); it != opendaq_handler_->folders_.end())
                selected_components_.push_back(it->second.component_);
            if (auto it = opendaq_handler_->input_ports_.find(id); it != opendaq_handler_->input_ports_.end())
                selected_components_.push_back(it->second.component_);
            if (auto it = opendaq_handler_->signals_.find(id); it != opendaq_handler_->signals_.end())
                selected_components_.push_back(it->second.component_);
        }
    }
    
    void RenderPopupMenu(ImGui::ImGuiNodes* nodes, ImVec2 position) override
    {
        ImGui::SeparatorText("Add a function block");
        for (const auto [fb_id, desc]: opendaq_handler_->instance_.getAvailableFunctionBlockTypes())
        {
            if (ImGui::MenuItem(fb_id.toStdString().c_str()))
            {
                daq::FunctionBlockPtr fb = opendaq_handler_->instance_.addFunctionBlock(fb_id);
                nodes->AddNode({fb.getName().toStdString(), fb.getGlobalId().toStdString()}, ImColor(100, 100, 200), position,
                               {}, {},
                               "");
                opendaq_handler_->folders_[fb.getGlobalId().toStdString()] = {fb, {}};
            }
            if (ImGui::BeginItemTooltip())
            {
                ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
                ImGui::TextUnformatted(desc.getDescription().toStdString().c_str());
                ImGui::PopTextWrapPos();
                ImGui::EndTooltip();
            }
        }

        ImGui::SeparatorText("Connect to device");
        // TODO: need to cache these and refresh on demand
        // for (const auto device_info : opendaq_handler_->instance_.getAvailableDevices())
        // {
        //     auto device_connection_string = device_info.getConnectionString();
        //     if (ImGui::MenuItem(device_connection_string.toStdString().c_str()))
        //     {
        //         opendaq_handler_->instance_.addDevice(device_connection_string);
        //         nodes->AddNode(device_connection_string.toStdString().c_str(), ImColor(0, 100, 200), position,
        //                        {}, {},
        //                        "");
        //     }
        // }
        std::string device_connection_string = "daq.nd://";
        ImGui::InputText("##w", &device_connection_string);
        if (ImGui::IsItemDeactivatedAfterEdit())
        {
            opendaq_handler_->instance_.addDevice(device_connection_string);
            nodes->AddNode(device_connection_string.c_str(), ImColor(0, 100, 200), position,
                           {}, {},
                           "");
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Connect"))
        {
            opendaq_handler_->instance_.addDevice(device_connection_string);
            nodes->AddNode(device_connection_string.c_str(), ImColor(0, 100, 200), position,
                           {}, {},
                           "");
            ImGui::CloseCurrentPopup();
        }
    }

    std::vector<daq::ComponentPtr> selected_components_;
    OpenDAQHandler* opendaq_handler_;
};


int main(int, char**)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
    {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

    OpenDAQHandler instance;
    daq::DevicePtr dev = instance.instance_.addDevice(instance.instance_.getAvailableDevices()[0].getConnectionString());
    auto stat = instance.instance_.addFunctionBlock("RefFBModuleStatistics");
    auto power = instance.instance_.addFunctionBlock("RefFBModulePower");
    stat.getInputPorts()[0].connect(power.getSignals()[0]);
    auto power2 = dev.addFunctionBlock("RefFBModulePower");
    power2.getInputPorts()[0].connect(stat.getSignals()[0]);

    NodeInteractionHandler node_interaction_handler(&instance);
    ImGui::ImGuiNodes nodes_editor(&node_interaction_handler);

    // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
    // GL ES 2.0 + GLSL 100
    const char* glsl_version = "#version 100";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#elif defined(__APPLE__)
    // GL 3.2 Core + GLSL 150
    const char* glsl_version = "#version 150";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG); // Always required on Mac
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif

    // From 2.0.18: Enable native IME.
#ifdef SDL_HINT_IME_SHOW_UI
    SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
#endif

    // Create window with graphics context
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window* window = SDL_CreateWindow("floating opendaq gui demo", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 2400, 1600, window_flags);
    if (window == nullptr)
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
    ImGuiIO& io = ImGui::GetIO();

    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    bool done = false;
#ifdef __EMSCRIPTEN__
    // For an Emscripten build we are disabling file-system access, so let's not attempt to do a fopen() of the imgui.ini file.
    // You may manually call LoadIniSettingsFromMemory() to load settings from your own storage.
    io.IniFilename = nullptr;
    EMSCRIPTEN_MAINLOOP_BEGIN
#else
    while (!done)
#endif
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                done = true;
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)
                done = true;
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_q)
                done = true;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
                done = true;
        }
        if (!(SDL_GetWindowFlags(window) & (SDL_WINDOW_INPUT_FOCUS)))
        {
            SDL_Delay(10);
            continue;
        }

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        static bool initialized = false;
        if (!initialized)
        {
            instance.RetrieveTopology(instance.instance_, nodes_editor);
            initialized = true;
        }

        DrawPropertiesWindow(node_interaction_handler.selected_components_);
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
            nodes_editor.ProcessNodes();
            nodes_editor.ProcessContextMenu();
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
#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_END;
#endif

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}

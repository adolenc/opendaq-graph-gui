// Dear ImGui: standalone example application for SDL2 + OpenGL
// (SDL is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_stdlib.h"
#include "implot.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
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

void DrawPropertiesWindow(const daq::ComponentPtr&);

void DrawFb(const OpenDAQ::Topology& fb, bool show_parents)
{
    if (!canCastTo<daq::IPropertyObject>(fb.component))
        return;

    daq::ComponentPtr component = fb.component;
    while (component.assigned())
    {
        ImGui::SeparatorText(component.getName().toStdString().c_str());
        DrawPropertiesWindow(component);
        if (!show_parents)
            break;
        component = component.getParent();
    }
}

void DrawPropertiesWindow(const daq::ComponentPtr& component)
{
    const daq::PropertyObjectPtr& property_holder = castTo<daq::IPropertyObject>(component);
    for (const auto& prop : property_holder.getVisibleProperties())
    {
        OpenDAQ::Property property(property_holder, prop);
        std::string prop_name = property.name;
        if (property.unit)
            prop_name += " [" + *property.unit + ']';
        if (property.read_only)
            ImGui::BeginDisabled();
        switch (property.type)
        {
            case daq::ctBool:
                {
                    bool value = property.value;
                    if (ImGui::Checkbox(property.name.c_str(), &value))
                    {
                        property_holder.setPropertyValue(property.name, value);
                        // goto rebuild_properties;
                    }
                    break;
                }
            case daq::ctInt:
                {
                    if (property.selection_values)
                    {
                        daq::ListPtr<daq::IString> vals = *property.selection_values;
                        std::string values = "";
                        for (int i = 0; i < vals.getCount(); i++)
                            values += vals.getItemAt(i).toStdString() + '\0';
                        int value = (int64_t)property.value;
                        if (ImGui::Combo(property.name.c_str(), &value, values.c_str(), vals.getCount()))
                        {
                            property_holder.setPropertyValue(property.name, value);
                            // goto rebuild_properties;
                        }
                    }
                    else
                    {
                        int value = (int64_t)property.value;
                        if (ImGui::InputInt(property.name.c_str(), &value))
                        {
                            property_holder.setPropertyValue(property.name, value);
                            // goto rebuild_properties;
                        }
                    }
                    break;
                }
            case daq::ctFloat:
                {
                    double value = property.value;
                    if (ImGui::InputDouble(property.name.c_str(), &value))
                    {
                        property_holder.setPropertyValue(property.name, value);
                        // goto rebuild_properties;
                    }
                    break;
                }
            case daq::ctString:
                {
                    std::string value = property.value;
                    ImGui::InputText(property.name.c_str(), &value);
                    break;
                }
            case daq::ctProc:
                {
                    if (ImGui::Button(property.name.c_str()))
                    {
                        property.value.asPtr<daq::IProcedure>().dispatch();
                        // goto rebuild_properties;
                    }
                    break;
                }
            default:
                {
                    std::string n = property.name + " " + std::to_string(property.type);
                    ImGui::Text("%s", n.c_str());
                }
        }
        if (property.read_only)
            ImGui::EndDisabled();
    }

    if (!ImGui::CollapsingHeader(("Attributes##" + component.getName().toStdString()).c_str()))
        return;

    {
        std::string value = component.getName();
        if (ImGui::InputText("Name", &value))
            component.setName(value);
    }
    {
        std::string value = component.getDescription();
        if (ImGui::InputText("Description", &value))
            component.setDescription(value);
    }
    {
        bool value = component.getActive();
        if (ImGui::Checkbox("Active", &value))
            component.setActive(value);
    }
    {
        bool value = component.getVisible();
        if (ImGui::Checkbox("Visible", &value))
            component.setVisible(value);
    }
    {
        std::string value = component.getLocalId();
        ImGui::BeginDisabled();
        ImGui::InputText("Local ID", &value);
        ImGui::EndDisabled();
    }
    {
        std::string value = component.getGlobalId();
        ImGui::BeginDisabled();
        ImGui::InputText("Global ID", &value);
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNone | ImGuiHoveredFlags_AllowWhenDisabled) && ImGui::BeginTooltip())
        {
            ImGui::Text("%s", value.c_str());
            ImGui::EndTooltip();
        }
    }

        // self.attributes['Tags'] = { 'Value': node.tags.list, 'Locked': False, 'Attribute': 'tags'}
        //
        // if daq.ISignal.can_cast_from(node):
        //     signal = daq.ISignal.cast_from(node)
        //
        //     self.attributes['Public'] = {'Value': bool(
        //         signal.public), 'Locked': False, 'Attribute': 'public'}
        //     self.attributes['Domain Signal ID'] = {
        //         'Value': signal.domain_signal.global_id if signal.domain_signal else '', 'Locked': True,
        //         'Attribute': '.domain_signal'}
        //     self.attributes['Related Signals IDs'] = {'Value': os.linesep.join(
        //         [s.global_id for s in signal.related_signals]), 'Locked': True, 'Attribute': 'related_signals'}
        //     self.attributes['Streamed'] = {'Value': bool(
        //         signal.streamed), 'Locked': True, 'Attribute': 'streamed'}
        //     self.attributes['Last Value'] = {
        //         'Value': get_last_value_for_signal(signal), 'Locked': True, 'Attribute': 'last_value'}
        //
        // if daq.IInputPort.can_cast_from(node):
        //     input_port = daq.IInputPort.cast_from(node)
        //
        //     self.attributes['Signal ID'] = {
        //         'Value': input_port.signal.global_id if input_port.signal else '', 'Locked': True,
        //         'Attribute': 'signal'}
        //     self.attributes['Requires Signal'] = {'Value': bool(
        //         input_port.requires_signal), 'Locked': True, 'Attribute': 'requires_signal'}
        //
        // locked_attributes = node.locked_attributes
        //
        // self.attributes['Status'] = { 'Value': dict(node.status_container.statuses.items()) or None, 'Locked': True, 'Attribute': 'status'}
        //
        // for locked_attribute in locked_attributes:
        //     if locked_attribute not in self.attributes:
        //         continue
        //     self.attributes[locked_attribute]['Locked'] = True

    return;
rebuild_properties:
    ;
}

// Main code
int main(int, char**)
{
    // Setup SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
    {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

    auto* instance = GetTopologyInstance();
    for (auto& d : instance->GetAvailableDevices())
        std::cout << d << std::endl;
    instance->ConnectToDevice(instance->GetAvailableDevices()[0]);
    instance->RetrieveTopology(instance->instance_);

    // // save configuration to string
    // std::ofstream("config2.json") << instance.instance_.saveConfiguration();
        
    // return 0;

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
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return a nullptr. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher quality font rendering.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    // - Our Emscripten build process allows embedding fonts to be accessible at runtime from the "fonts/" folder. See Makefile.emscripten for details.
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf", 18.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());
    //IM_ASSERT(font != nullptr);

    // Our state
    bool show_demo_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);


    auto imguiNodes = ImGui::ImGuiNodes();

    // {
    //     ImGui::ImGuiNodesNodeDesc desc = ImGui::ImGuiNodesNodeDesc{"HAHAHA", ImGui::ImGuiNodesNodeType_Generic, ImColor(0.2, 0.3, 0.6, 0.0f)};
    //
    //     desc.inputs_.push_back({ "iii", ImGui::ImGuiNodesConnectorType_Int });
    //     desc.inputs_.push_back({ "Int", ImGui::ImGuiNodesConnectorType_Int });
    //     desc.inputs_.push_back({ "TextStream", ImGui::ImGuiNodesConnectorType_Text });
    //     desc.inputs_.push_back({ "TextStream1", ImGui::ImGuiNodesConnectorType_Text });
    //     desc.inputs_.push_back({ "TextStream2", ImGui::ImGuiNodesConnectorType_Text });
    //     desc.inputs_.push_back({ "TextStream3", ImGui::ImGuiNodesConnectorType_Text });
    //
    //     desc.outputs_.push_back({ "Float", ImGui::ImGuiNodesConnectorType_Float });
    //     desc.outputs_.push_back({ "Float", ImGui::ImGuiNodesConnectorType_Float });
    //     desc.outputs_.push_back({ "Float", ImGui::ImGuiNodesConnectorType_Float });
    //
    //     imguiNodes.AddNodeDesc(desc);
    // }


    // Main loop
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
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL2_ProcessEvent(&event);
            // get mouse position, but only if mouse is clicked and is dragging
            if (!io.WantCaptureMouse && (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP || event.type == SDL_MOUSEMOTION))
            {
                // if (event.button.button == SDL_BUTTON_LEFT)
            }
            if (event.type == SDL_QUIT)
                done = true;
            // on escape key set done to true
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

        ImGui::GetIO().ConfigWindowsMoveFromTitleBarOnly = true;

        // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
        if (show_demo_window)
        {
            ImGui::ShowDemoWindow(&show_demo_window);
            ImPlot::ShowDemoWindow();
        }
        
        {
            // property window
            ImGui::Begin("Property editor", NULL, ImGuiWindowFlags_AlwaysAutoResize);

            {
                static bool combine_properties = false;
                ImGui::Checkbox("Combine properties", &combine_properties);
                static bool show_parents = false;
                ImGui::Checkbox("Show parents", &show_parents);

                // ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(255, 0, 0, 30));
                ImGui::BeginChild("RefDev0", ImVec2(0, 0), ImGuiChildFlags_None | ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY);
                DrawFb(instance->folders_["RefDev0"], show_parents);
                ImGui::EndChild();
                // ImGui::PopStyleColor();

                ImGui::SameLine();

                ImGui::BeginChild("ref_fb_module_statistics_1", ImVec2(0, 0), ImGuiChildFlags_None | ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY);
                DrawFb(instance->folders_["ref_fb_module_statistics_1"], show_parents);
                ImGui::EndChild();

                ImGui::SameLine();

                ImGui::BeginChild("refch0", ImVec2(0, 0), ImGuiChildFlags_None | ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY);
                DrawFb(instance->folders_["refch0"], show_parents);
                ImGui::EndChild();
            }

            ImGui::End();
        }

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
        {
            static float f = 1.0f;
            static int counter = 3;

            ImGui::Begin(", world!");                          // Create a window called "Hello, world!" and append into it.

            ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
            ImVec2 sz = ImVec2(-FLT_MIN, 0.0f);
            ImGui::Text("Here is a tooltip:");
            ImGui::SameLine();
            // button with anchor emoji
            ImGui::Button("anch", sz);
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
            {
                ImGui::SetDragDropPayload("HELLO", (void*)0, 0);
                ImGui::Text("Draging...");
                ImGui::EndDragDropSource();
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNone) && ImGui::BeginTooltip())
            {
                ImGui::Text("I am a fancy tooltip");
                static float arr[] = { 0.6f, 0.1f, 1.0f, 0.5f, 0.92f, 0.1f, 0.2f,  0.6f, 0.1f, 1.0f, 0.5f, 0.92f, 0.1f, 0.2f, 0.6f, 0.1f, 1.0f, 0.5f, 0.92f, 0.1f, 0.2f,  0.6f, 0.1f, 1.0f, 0.5f, 0.92f, 0.1f, 0.2f  };
                for (int i = 0; i < IM_ARRAYSIZE(arr); i++)
                    arr[i] = sinf((float)i * 0.7f + ImGui::GetTime()*5.f) * 3.0f;
                ImGui::PlotLines("Curve", arr, IM_ARRAYSIZE(arr), 0, NULL, -5.0f, 5.0f, ImVec2(500, 160));
                ImGui::Text("Sin(time) = %f", sinf((float)ImGui::GetTime()));
                ImGui::EndTooltip();
            }
            ImGui::Button("Fancy", sz);
            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HELLO"))
                {
                    ImGui::Text("Dropped!");
                }
                ImGui::EndDragDropTarget();
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNone) && ImGui::BeginTooltip())
            {
                ImGui::Text("I am a fancy tooltip");
                static float arr[] = { 0.6f, 0.1f, 1.0f, 0.5f, 0.92f, 0.1f, 0.2f };
                for (int i = 0; i < IM_ARRAYSIZE(arr); i++)
                    arr[i] = sinf((float)i * 0.7f + ImGui::GetTime()*5.f) * 3.0f;
                ImGui::PlotLines("Curve", arr, IM_ARRAYSIZE(arr), 0, NULL, -5.0f, 5.0f, ImVec2(500, 160));
                ImGui::Text("Sin(time) = %f", sinf((float)ImGui::GetTime()));
                ImGui::EndTooltip();
            }
            ImGui::Button("Fancy", sz);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNone) && ImGui::BeginTooltip())
            {
                ImGui::Text("I am a fancy tooltip");
                static float arr[] = { 0.6f, 0.1f, 1.0f, 0.5f, 0.92f, 0.1f, 0.2f };
                for (int i = 0; i < IM_ARRAYSIZE(arr); i++)
                    arr[i] = sinf((float)i * 0.7f + ImGui::GetTime()*5.f) * 3.0f;
                ImGui::PlotLines("Curve", arr, IM_ARRAYSIZE(arr), 0, NULL, -5.0f, 5.0f, ImVec2(500, 160));
                ImGui::Text("Sin(time) = %f", sinf((float)ImGui::GetTime()));
                ImGui::EndTooltip();
            }

            ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
            ImGui::Checkbox("Another Window", &show_another_window);

            // if (ImGui::SliderFloat("float", &f2, 0.0f, 1.0f))           // Edit 1 float using a slider from 0.0f to 1.0f
            //     std::cout << "CHANGED! " << f2 << std::endl;
            ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

            if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
                counter++;
            ImGui::SameLine();
            ImGui::Text("counter = %d", counter);

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
            ImGui::End();
        }

        // 3. Show another simple window.
        if (show_another_window)
        {
            ImGui::Begin("Another Window", &show_another_window, ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoTitleBar);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
            ImGui::Text("Mouse x, y: %f %f", ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y);
            ImGui::Text("Hello from another window!");
            if (ImGui::Button("Close Me"))
                show_another_window = false;
            ImGui::End();
        }
#ifdef IMGUI_HAS_VIEWPORT
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->GetWorkPos());
        ImGui::SetNextWindowSize(viewport->GetWorkSize());
        ImGui::SetNextWindowViewport(viewport->ID);
#else
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
#endif
        // if we are over some window, we should not pass through any mouse events
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        if (ImGui::Begin("Node Editor", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus)) {

            static bool initialized = false;
            if (!initialized)
            {
              // imguiNodes.nodes_.push_back(imguiNodes.CreateNodeFromDesc(&(imguiNodes.nodes_desc_[1]), ImVec2(200, 200)));
              // imguiNodes.nodes_.push_back(imguiNodes.CreateNodeFromDesc(&(imguiNodes.nodes_desc_[1]), ImVec2(500, 300)));
              initialized = true;
            }
            imguiNodes.Update();
            imguiNodes.ProcessNodes();
            imguiNodes.ProcessContextMenu();
        }
        ImGui::End();
        ImGui::PopStyleVar(1);

        // Rendering
        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
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

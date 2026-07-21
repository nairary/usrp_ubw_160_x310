#include "Application.h"

Application::Application(const int width, const int height)
    : io(ImGuiIO{})
{
    temp_data.resize(FFT_SIZE);
    local_data.resize(FFT_SIZE);
    if (!glfwInit())
        throw std::runtime_error("GLFW runtime error");
    
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    // Create window with graphics context
    float main_scale = ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor()); // Valid on GLFW 3.3+ only
    window = glfwCreateWindow((int)(width * main_scale), (int)(height * main_scale), "Dear ImGui GLFW+OpenGL3 example", nullptr, nullptr);
    if (window == nullptr)
        throw std::runtime_error("Creating window error");
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync
    
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    io = ImGui::GetIO(); 
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows
    //io.ConfigViewportsNoAutoMerge = true;
    //io.ConfigViewportsNoTaskBarIcon = true;
    
    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();
    
    // Setup scaling
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);        // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
    style.FontScaleDpi = main_scale;        // Set initial font scale. (in docking branch: using io.ConfigDpiScaleFonts=true automatically overrides this for every window depending on the current monitor)
    
    io.ConfigDpiScaleFonts = true;          // [Experimental] Automatically overwrite style.FontScaleDpi in Begin() when Monitor DPI changes. This will scale fonts but _NOT_ scale sizes/padding for now.
    io.ConfigDpiScaleViewports = true;      // [Experimental] Scale Dear ImGui and Platform Windows when Monitor DPI changes.
    
    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }
    
    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);
 }

void Application::ShowPlot()
{
    bool show_demo_window = true;
    ImPlot::ShowDemoWindow(&show_demo_window);
}

void Application::ShowUSRPInterface()
{
    ImGui::Begin("USRP interface");
    {
        ImGui::Begin("Receiver Parameters");
        if (!usrp_device_exists.load())
        {
            // Включаем приёмник
            if (ImGui::Button("Turn on receiver"))
            {
                app_threads.push_back(std::jthread{ &Application::CreateUSRPDevice, this });
            }
        }
        else
        {
            ImGui::Text("USRP device connected");
            // Изменяем центральную частоту
            ImGui::SliderScalar("Frequency, MHz", ImGuiDataType_Double, &current_frequency, &frequency_min, &frequency_max, "%.2f");
            //ImGui::InputDouble("USRP Frequency, MHz", &current_frequency);
            current_frequency = (current_frequency < frequency_min) ? frequency_min :
                (current_frequency > frequency_max) ? frequency_max : current_frequency;
            usrp_device->SetFrequency(current_frequency);

        }
            
        ImGui::End();
        if (usrp_device_exists.load())
        {
            // Логика, контролирующая стрим данных
          
            if (!usrp_device->IsStreaming())
            {
                if (ImGui::Button("Start stream"))
                {
                    usrp_device->StartStream();
                    app_threads.push_back(std::jthread{ &Application::GetAndRenderStreamingData, this });
                }
            }
            else
            {
                auto freq = usrp_device->GetCenterFrequency();
                auto fd = usrp_device->GetSamplingRate();
                ImGui::Text("Im streaming");
                //std::cout << (std::to_string(local_data[0]).c_str()) << "\n";
                FFTPlot(local_data, freq, fd);
                if (ImGui::Button("Stop stream"))
                {
                    usrp_device->StopStream();
                    app_threads.pop_back();
                }
            }
        }
    }
    ImGui::End();
}

void Application::CreateUSRPDevice()
{
    usrp_device = std::make_unique<USRPDevice>();
    usrp_device_exists.store(true);
    return;
}

void Application::GetAndRenderStreamingData()
{
    std::jthread jth(&USRPDevice::GetUSRPData, usrp_device.get(), std::ref(temp_data), std::ref(is_data_ready), std::ref(receiver_mutex));
    while (usrp_device->IsStreaming())
    {
        std::lock_guard lock(receiver_mutex);
        if (is_data_ready.load())
        {
            local_data = temp_data;
            is_data_ready.store(false);
        }
    }
}

void Application::Run()
{
    bool docking_interface = true;
    bool show_demo_window = true;
    bool show_another_window = true;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    ImGuiIO& current_io = ImGui::GetIO();
    current_io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    while (!glfwWindowShouldClose(window))
    {
        
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        glfwPollEvents();
        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0)
        {
            ImGui_ImplGlfw_Sleep(10);
            continue;
        }

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGui::DockSpaceOverViewport();
        // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);

        // 3. Show another simple window.
        if (show_another_window)
        {
            ShowPlot();
        }
        ShowUSRPInterface();
        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Update and Render additional Platform Windows
        // (Platform functions may change the current OpenGL context, so we save/restore it to make it easier to paste this code elsewhere.
        //  For this specific demo app we could also call glfwMakeContextCurrent(window) directly)
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }

        glfwSwapBuffers(window);
    }
}


Application::~Application()
{

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
}



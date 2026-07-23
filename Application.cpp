#include "Application.h"

Application::Application(const int width, const int height)
    : io(ImGuiIO{})
{
    temp_data.resize(FFT_SIZE);
    local_data.resize(FFT_SIZE);
    averaged_data.resize(FFT_SIZE);
    average_sum.resize(FFT_SIZE);
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
            if (ImGui::IsItemDeactivatedAfterEdit())
            {
                usrp_device->SetFrequency(current_frequency);
            }
            ImGui::Text("Current Frequency: %.2f MHz", usrp_device->GetFrequency());

            // Изменяем частоту дискретизации
            if (sampling_rates_mhz.empty())
            {
                sampling_rates_mhz = usrp_device->GetAvailableSamplingRates();
                for (int i = 0; i < static_cast<int>(sampling_rates_mhz.size()); ++i)
                {
                    if (sampling_rates_mhz[i] >= usrp_device->GetSamplingRate())
                    {
                        selected_sampling_rate_index = i;
                        break;
                    }
                }
            }

            if (!sampling_rates_mhz.empty())
            {
                char sampling_rate_label[32];
                snprintf(sampling_rate_label, sizeof(sampling_rate_label), "%.3f MHz", sampling_rates_mhz[selected_sampling_rate_index]);
                if (ImGui::BeginCombo("Sampling rate", sampling_rate_label))
                {
                    for (int i = 0; i < static_cast<int>(sampling_rates_mhz.size()); ++i)
                    {
                        const bool is_selected = (selected_sampling_rate_index == i);
                        char item_label[32];
                        snprintf(item_label, sizeof(item_label), "%.3f MHz", sampling_rates_mhz[i]);
                        if (ImGui::Selectable(item_label, is_selected))
                        {
                            selected_sampling_rate_index = i;
                            usrp_device->SetSamplingRate(sampling_rates_mhz[i]);
                            ResetSpectrumAveraging();
                        }
                        if (is_selected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                ImGui::Text("Current sampling rate: %.3f MHz", usrp_device->GetSamplingRate());
            }

            // Изменяем усиление приемника в дБ
            ImGui::SliderScalar(
                "RX gain",
                ImGuiDataType_Double,
                &current_rx_gain,
                &rx_gain_min,
                &rx_gain_max,
                "%.1f dB"
            );

            if (ImGui::IsItemDeactivatedAfterEdit())
            {
                usrp_device->SetRXGain(current_rx_gain);
            }
            ImGui::Text("Current RX gain: %.1f dB", usrp_device->GetRXGain());

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
                auto freq = usrp_device->GetFrequency() * 1e6;
                auto fd = usrp_device->GetSamplingRate() * 1e6;
                ImGui::Text("Im streaming");
                //std::cout << (std::to_string(local_data[0]).c_str()) << "\n";
                ImGui::Checkbox("Average spectrum over time", &average_spectrum_enabled);
                ImGui::BeginDisabled(!average_spectrum_enabled);
                if (ImGui::SliderFloat("Averaging time, s", &average_time_seconds, 0.1f, 30.0f, "%.1f"))
                    ResetSpectrumAveraging();
                ImGui::EndDisabled();

                ImGui::SeparatorText("IQ recording");
                ImGui::SliderFloat("Recording time, s", &recording_time_seconds, 0.1f, 10.0f, "%.1f");
                ImGui::BeginDisabled(usrp_device->IsRecording());
                if (ImGui::Button("Record raw IQ"))
                    usrp_device->StartRecording(recording_time_seconds);
                ImGui::EndDisabled();
                if (usrp_device->IsRecording())
                    ImGui::Text("Recording raw IQ...");
                else if (!usrp_device->GetLastRecordingBasePath().empty())
                    ImGui::Text("Last recording: %s", usrp_device->GetLastRecordingBasePath().string().c_str());

                UpdateSpectrumAveraging(local_data, fd);
                FFTPlot(GetSpectrumForPlot(), freq, fd);
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
            data_sequence.fetch_add(1, std::memory_order_relaxed);
            is_data_ready.store(false);
        }
    }
}

void Application::ResetSpectrumAveraging()
{
    average_history.clear();
    std::fill(average_sum.begin(), average_sum.end(), 0.0f);
    std::fill(averaged_data.begin(), averaged_data.end(), 0.0f);
    last_averaged_sequence = data_sequence.load(std::memory_order_relaxed);
}

void Application::UpdateSpectrumAveraging(const std::vector<float>& spectrum, double sampling_rate_hz)
{
    const uint64_t sequence = data_sequence.load(std::memory_order_relaxed);
    if (!average_spectrum_enabled || sequence == last_averaged_sequence || spectrum.empty())
        return;

    if (average_sum.size() != spectrum.size())
    {
        average_sum.assign(spectrum.size(), 0.0f);
        averaged_data.assign(spectrum.size(), 0.0f);
        average_history.clear();
    }

    const double block_duration_seconds = static_cast<double>(spectrum.size()) / sampling_rate_hz;
    const size_t max_blocks = std::max<size_t>(1, static_cast<size_t>(std::ceil(average_time_seconds / block_duration_seconds)));

    average_history.push_back(spectrum);
    for (size_t i = 0; i < spectrum.size(); ++i)
        average_sum[i] += spectrum[i];

    while (average_history.size() > max_blocks)
    {
        const auto& oldest = average_history.front();
        for (size_t i = 0; i < oldest.size(); ++i)
            average_sum[i] -= oldest[i];
        average_history.pop_front();
    }

    const float scale = 1.0f / static_cast<float>(average_history.size());
    for (size_t i = 0; i < average_sum.size(); ++i)
        averaged_data[i] = average_sum[i] * scale;

    last_averaged_sequence = sequence;
}

const std::vector<float>& Application::GetSpectrumForPlot() const
{
    return average_spectrum_enabled && !average_history.empty() ? averaged_data : local_data;
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



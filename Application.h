#pragma once
#include "Core.h"
#include "imgui/implot.h"
#include <stdexcept>
#include <atomic>
#include <memory>
#include <string>

#include "USRPDevice.h"
#include "PlotFunctions.h"

class Application
{
public:
	Application() = delete;
	Application(const int width = 1920, const int height = 1080);
	void ShowPlot();
	void ShowUSRPInterface();
	void CreateUSRPDevice();
	void GetAndRenderStreamingData();
	void Run();
	~Application();
private:
	std::unique_ptr<USRPDevice> usrp_device = nullptr;
	std::atomic_bool usrp_device_exists = false;
	std::atomic_bool is_data_ready = false;
	std::mutex receiver_mutex;
	std::vector<std::jthread> app_threads;
	std::vector<float> local_data;
	std::vector<float> temp_data;
	GLFWwindow* window = nullptr;
	ImGuiIO io;

	double current_frequency = 0.0;
	double current_sampling_rate = 0.0;
	const double frequency_max = 6000;
	const double frequency_min = 10;
};
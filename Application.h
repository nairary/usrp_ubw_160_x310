#pragma once
#include "Core.h"
#include "imgui/implot.h"
#include <stdexcept>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <deque>

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
	void ResetSpectrumAveraging();
	void UpdateSpectrumAveraging(const std::vector<float>& spectrum, double sampling_rate_hz);
	const std::vector<float>& GetSpectrumForPlot() const;
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
	std::vector<float> averaged_data;
	std::vector<float> average_sum;
	std::deque<std::vector<float>> average_history;
	std::atomic_uint64_t data_sequence = 0;
	uint64_t last_averaged_sequence = 0;
	bool average_spectrum_enabled = false;
	float average_time_seconds = 1.0f;
	float recording_time_seconds = 1.0f;
	int selected_fft_exponent = DEFAULT_FFT_EXPONENT;
	GLFWwindow* window = nullptr;
	ImGuiIO io;

	double current_frequency = 10.0;
	double current_sampling_rate = 10.0;
	double current_rx_gain = 30.0;
	int selected_sampling_rate_index = 0;
	std::vector<double> sampling_rates_mhz;
	const double frequency_max = 6000;
	const double frequency_min = 10;
	const double rx_gain_max = 31.5;
	const double rx_gain_min = 0.0;
};

#pragma once

#include <uhd/usrp/multi_usrp.hpp>
#include <uhd/utils/safe_main.hpp>
#include <uhd/utils/thread.hpp>
#include <uhd/convert.hpp>
#include <boost/format.hpp>
#include <boost/program_options.hpp>
#include <chrono>
#include <complex>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <filesystem>

#include "ReaTimeFFT.h"

//#include "spectrumAnalyzer.h"
//#include "Console.h"

	class USRPDevice
	{
	public:
		USRPDevice();
		~USRPDevice();
		void StartStream();
		void StopStream();
		bool IsStreaming() const { return stream_running.load(); }
		void GetUSRPData(std::vector<float>& output_data, std::atomic_bool& is_data_ready, std::mutex& data_mutex);
		// Входная частота в МГц. Возвращает реальную частоту в МГц
		double SetFrequency(const double frequency);

		// Получить центральную частоту в Гц
		double GetCenterFrequency() const { return usrp_device->get_rx_freq(); };
		// Установить частоту дискретизации в МГц. Возвращает реальную частоту в МГц
		double SetSamplingRate(const double rate);
		// Получить частоту дискретизации в Гц
		double GetSamplingRate() const { return usrp_device->get_rx_rate(); };
	private:
		std::vector<std::complex<int16_t>> usrpBuffer;
		uhd::rx_streamer::sptr rx_stream;
 		uhd::usrp::multi_usrp::sptr usrp_device;
		uhd::rx_metadata_t md;
		double sample_rate = 10e6;
		double center_freq = 1.001e9;
		double gain = 30.0;
		std::atomic_bool stream_running = false;
		RealTimeFFT fft_processor;
	};

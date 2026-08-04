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
#include <ctime>
#include <iostream>
#include <mutex>
#include <memory>
#include <thread>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "ReaTimeFFT.h"

//#include "spectrumAnalyzer.h"
//#include "Console.h"

	class USRPDevice
	{
	public:
		USRPDevice(bool& success);
		~USRPDevice();
		void StartStream();
		void StopStream();
		bool IsStreaming() const { return stream_running.load(); }
		bool IsRecording() const { return recording_running.load(); }
		void StartRecording(double duration_seconds);
		const std::filesystem::path& GetLastRecordingBasePath() const { return last_recording_base_path; }
		void GetUSRPData(std::vector<float>& output_data, std::atomic_bool& is_data_ready, std::mutex& data_mutex);
		// Входная частота в МГц. Возвращает реальную частоту в МГц
		double SetFrequency(const double frequency);

		// Получить центральную частоту в МГц
		double GetFrequency() const { return usrp_device->get_rx_freq() / 1e6; };
		// Установить частоту дискретизации в МГц. Возвращает реальную частоту в МГц
		double SetSamplingRate(const double rate);
		// Получить частоту дискретизации в МГц
		double GetSamplingRate() const { return usrp_device->get_rx_rate() / 1e6; };
		// Получить список частот дискретизации в МГц, вычисленный из master clock rate
		std::vector<double> GetAvailableSamplingRates() const;
		// Установить усиление на приемнике в дБ.
		double SetRXGain(const double gain);
		// Получить усиление на приемнике в дБ.
		double GetRXGain() const { return usrp_device->get_rx_gain(); };
		// Изменить число точек FFT можно только при остановленном потоке.
		void SetFFTSize(size_t fft_size);
		size_t GetFFTSize() const { return fft_processor->getFFTSize(); }
	private:
		std::vector<std::complex<int16_t>> usrpBuffer;
		uhd::rx_streamer::sptr rx_stream;
		uhd::usrp::multi_usrp::sptr usrp_device;
		uhd::rx_metadata_t md;
		double rx_gain = 30.0;
		double sample_rate = 10e6;
		double center_freq = 10.0e6;
		void WriteRecordingChunk(const std::complex<int16_t>* samples, size_t sample_count);
		void FinishRecording();
		std::atomic_bool stream_running = false;
		std::atomic_bool recording_running = false;
		std::mutex recording_mutex;
		std::ofstream recording_file;
		size_t recording_samples_remaining = 0;
		std::filesystem::path last_recording_base_path;
		std::unique_ptr<RealTimeFFT> fft_processor;
	};

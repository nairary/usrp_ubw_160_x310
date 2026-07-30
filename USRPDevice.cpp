#include "USRPDevice.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
//#include "Utils.h"

USRPDevice::USRPDevice(bool& success)
	: fft_processor(RealTimeFFT(32768))
{
	uhd::set_thread_priority_safe(1.0f, true);
	std::string device_args = "type=x300";
	std::cout << "[USRP] Serach and connection to USRP X300...\n";
	try
	{
		usrp_device = uhd::usrp::multi_usrp::make(device_args);
	}
	catch (const std::exception& e)
	{
		std::cerr << "[USRP] Error in device initialization: " << e.what() << "\n";
		success = false;
		return;
	}

	usrp_device->set_rx_rate(sample_rate);
	usrp_device->set_rx_freq(center_freq);
	usrp_device->set_rx_gain(rx_gain);

	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	// Устанавливаем тип данных std::coplex<int16_t>
	uhd::stream_args_t stream_args("sc16", "sc16");
	rx_stream = usrp_device->get_rx_stream(stream_args);
	success = true;
}

USRPDevice::~USRPDevice()
{
	if (recording_running.load())
		FinishRecording();
	if (stream_running.load())
		StopStream();
}

void USRPDevice::StartStream()
{
	uhd::stream_cmd_t stream_cmd(uhd::stream_cmd_t::STREAM_MODE_START_CONTINUOUS);
	stream_cmd.stream_now = true;
	rx_stream->issue_stream_cmd(stream_cmd);
	stream_running.store(true);
}

void USRPDevice::StopStream()
{
	if (recording_running.load())
		FinishRecording();
	uhd::stream_cmd_t stop_cmd(uhd::stream_cmd_t::STREAM_MODE_STOP_CONTINUOUS);
	rx_stream->issue_stream_cmd(stop_cmd);
	stream_running.store(false);
}

void USRPDevice::StartRecording(double duration_seconds)
{
	std::lock_guard lock(recording_mutex);
	if (!stream_running.load() || recording_running.load())
		return;

	const auto now = std::chrono::system_clock::now();
	const std::time_t now_time = std::chrono::system_clock::to_time_t(now);
	std::tm timestamp{};
#if defined(_WIN32)
	localtime_s(&timestamp, &now_time);
#else
	localtime_r(&now_time, &timestamp);
#endif
	std::ostringstream name;
	name << "iq_recording_" << std::put_time(&timestamp, "%Y%m%d_%H%M%S");

	const auto recording_dir = std::filesystem::current_path() / "recordings";
	std::filesystem::create_directories(recording_dir);
	last_recording_base_path = recording_dir / name.str();

	recording_file.open(last_recording_base_path.string() + ".bin", std::ios::binary | std::ios::trunc);
	if (!recording_file.is_open())
	{
		std::cerr << "[USRP] Could not open IQ recording file\n";
		return;
	}

	recording_samples_remaining = static_cast<size_t>(std::max(1.0, std::round(duration_seconds * sample_rate)));
	std::ofstream metadata_file(last_recording_base_path.string() + ".txt", std::ios::trunc);
	metadata_file << "Central frequency, Hz: " << center_freq << "\n"
		<< "Sampling rate, Hz: " << sample_rate << "\n"
		<< "RX gain, dB: " << rx_gain << "\n"
		<< "Duration, s: " << duration_seconds << "\n"
		<< "IQ format: interleaved signed 16-bit I/Q (sc16)\n";
	recording_running.store(true);
}

void USRPDevice::WriteRecordingChunk(const std::complex<int16_t>* samples, size_t sample_count)
{
	std::lock_guard lock(recording_mutex);
	if (!recording_running.load() || !recording_file.is_open())
		return;

	const size_t samples_to_write = std::min(sample_count, recording_samples_remaining);
	recording_file.write(reinterpret_cast<const char*>(samples), static_cast<std::streamsize>(samples_to_write * sizeof(std::complex<int16_t>)));
	recording_samples_remaining -= samples_to_write;
	if (recording_samples_remaining == 0)
	{
		if (recording_file.is_open())
			recording_file.close();
		recording_running.store(false);
	}
}

void USRPDevice::FinishRecording()
{
	std::lock_guard lock(recording_mutex);
	if (recording_file.is_open())
		recording_file.close();
	recording_samples_remaining = 0;
	recording_running.store(false);
}

void USRPDevice::GetUSRPData(std::vector<float>& output_data, std::atomic_bool& is_data_ready, std::mutex& data_mutex)
{
	while (stream_running)
	{
		size_t num_rx_samps = rx_stream->recv(&usrpBuffer[0], 32768, md, 1.0);
		// Проверка на ошибки связи
		if (md.error_code == uhd::rx_metadata_t::ERROR_CODE_TIMEOUT) {
			std::cerr << "[USRP] Таймаут получения данных!" << std::endl;
			continue;
		}
		if (md.error_code == uhd::rx_metadata_t::ERROR_CODE_OVERFLOW) {
			// Буфер переполнен (процессор или сеть не успевают за USRP).
			// Печатаем "O" (стандартное поведение UHD) и продолжаем.
			std::cerr << "O" << std::flush;
			continue;
		}
		if (num_rx_samps != 32768) {
			continue; // Получили неполный пакет — пропускаем во избежание артефактов FFT
		}
		WriteRecordingChunk(usrpBuffer.data(), num_rx_samps);
		fft_processor.processBlock(usrpBuffer.data(), 32768);
		{
			std::lock_guard lk{ data_mutex };
			output_data = fft_processor.getLatestMagnitudes();
			is_data_ready.store(true);
		}
	}


}

double USRPDevice::SetFrequency(const double frequency)
{
	usrp_device->set_rx_freq(frequency * 1e6);
	center_freq = usrp_device->get_rx_freq();
	return center_freq / 1e6;
}

double USRPDevice::SetSamplingRate(const double rate)
{
	usrp_device->set_rx_rate(rate * 1e6);
	sample_rate = usrp_device->get_rx_rate();
	return sample_rate / 1e6;
}

double USRPDevice::SetRXGain(const double gain)
{
	usrp_device->set_rx_gain(gain);
	rx_gain = usrp_device->get_rx_gain();
	return rx_gain;
}

std::vector<double> USRPDevice::GetAvailableSamplingRates() const
{
	std::vector<double> rates_mhz;
	const double master_clock_rate = usrp_device->get_master_clock_rate();
	const int max_decimation = static_cast<int>(master_clock_rate / 1e6);

	std::vector<int> decimation_vector = {1, 2, 3, 4, 5, 10, 20, 40, 60, 200};

	rates_mhz.reserve(max_decimation);
	/*for (int decimation = 1; decimation <= max_decimation; ++decimation)
	{
		if ((decimation > 256 && decimation % 4 != 0) || (decimation > 128 && decimation % 2 != 0))
		{
			continue;
		}
		rates_mhz.push_back((master_clock_rate / decimation) / 1e6);
	}*/

	for (int decimation = 1; decimation <= decimation_vector.size(); ++decimation)
	{
		rates_mhz.push_back((master_clock_rate / decimation_vector[decimation - 1]) / 1e6);
	}

	std::sort(rates_mhz.begin(), rates_mhz.end());
	return rates_mhz;
}

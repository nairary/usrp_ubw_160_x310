#include "USRPDevice.h"
#include <algorithm>
//#include "Utils.h"

USRPDevice::USRPDevice()
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
		return;
	}

	usrp_device->set_rx_rate(sample_rate);
	usrp_device->set_rx_freq(center_freq);
	usrp_device->set_rx_gain(gain);

	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	// Устанавливаем тип данных std::coplex<int16_t>
	uhd::stream_args_t stream_args("sc16", "sc16");
	rx_stream = usrp_device->get_rx_stream(stream_args);
	usrpBuffer.resize(32768);
}

USRPDevice::~USRPDevice()
{
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
	uhd::stream_cmd_t stop_cmd(uhd::stream_cmd_t::STREAM_MODE_STOP_CONTINUOUS);
	rx_stream->issue_stream_cmd(stop_cmd);
	stream_running.store(false);
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

std::vector<double> USRPDevice::GetAvailableSamplingRates() const
{
	std::vector<double> rates_mhz;
	const double master_clock_rate = usrp_device->get_master_clock_rate();
	const int max_decimation = static_cast<int>(master_clock_rate / 1e6);

	rates_mhz.reserve(max_decimation);
	for (int decimation = 1; decimation <= max_decimation; ++decimation)
	{
		if ((decimation > 256 && decimation % 4 != 0) || (decimation > 128 && decimation % 2 != 0))
		{
			continue;
		}

		rates_mhz.push_back((master_clock_rate / decimation) / 1e6);
	}

	std::sort(rates_mhz.begin(), rates_mhz.end());
	return rates_mhz;
}

#include "USRPDevice.h"
#include "IqAccumulator.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>
//#include "Utils.h"

USRPDevice::USRPDevice(bool& success)
	: fft_processor(std::make_unique<RealTimeFFT>(DEFAULT_FFT_SIZE))
{
	usrpBuffer.resize(DEFAULT_FFT_SIZE);
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
	sample_rate = usrp_device->get_rx_rate();

	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	// Устанавливаем тип данных std::coplex<int16_t>
	uhd::stream_args_t stream_args("sc16", "sc16");
	rx_stream = usrp_device->get_rx_stream(stream_args);
	success = true;
}

USRPDevice::~USRPDevice()
{
    if (rx_stream) { try { StopStream(); } catch (...) {} }
    FinishRecording();
}

void USRPDevice::StartStream()
{
    ++stream_segment;
    { std::lock_guard lock(error_mutex); stream_error.clear(); }
	uhd::stream_cmd_t stream_cmd(uhd::stream_cmd_t::STREAM_MODE_START_CONTINUOUS);
	stream_cmd.stream_now = true;
	rx_stream->issue_stream_cmd(stream_cmd);
	stream_running.store(true);
}

void USRPDevice::StopStream()
{
    stream_running.store(false);
    uhd::stream_cmd_t stop_cmd(uhd::stream_cmd_t::STREAM_MODE_STOP_CONTINUOUS);
    rx_stream->issue_stream_cmd(stop_cmd);
    // Application joins the selected RX method before reconfiguration/destruction.
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
    if (!recording_file) {
        recording_file.close(); recording_running.store(false);
        throw std::runtime_error("IQ recording write failed (disk full or I/O error)");
    }
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
	const size_t fft_size = fft_processor->getFFTSize();
	while (stream_running)
	{
		size_t num_rx_samps = rx_stream->recv(usrpBuffer.data(), fft_size, md, 1.0);
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
		if (num_rx_samps != fft_size) {
			continue; // Получили неполный пакет — пропускаем во избежание артефактов FFT
		}
		WriteRecordingChunk(usrpBuffer.data(), num_rx_samps);
		fft_processor->processBlock(usrpBuffer.data(), fft_size);
		{
			std::lock_guard lk{ data_mutex };
			output_data = fft_processor->getLatestMagnitudes();
			is_data_ready.store(true);
		}
	}


}

void USRPDevice::GetWiFiData(std::vector<float>& output_data, std::atomic_bool& is_data_ready, std::mutex& data_mutex)
{
    // Wi-Fi receive mode replaces the legacy loop; never run both concurrently.
    // Its fast path copies IQ into bounded queues;
    // FFT and file writes run in the processing worker, Wi-Fi in its own worker.
    IqQueue processing(64);
    const size_t fft_size = fft_processor->getFFTSize();
    std::jthread processor([&] {
        try {
            IqAccumulator fft_pending(fft_size);
            uint64_t segment = 0, next = 0;
            IqBlock block;
            while (processing.pop(block)) {
                if (block.discontinuity || segment != block.segment || next != block.first) {
                    fft_pending.reset();
                    // A recording cannot silently stitch across an unknown gap.
                    if (segment != 0 && recording_running.load()) {
                        FinishRecording();
                        std::lock_guard lock(error_mutex);
                        stream_error="Recording stopped at an IQ discontinuity";
                    }
                }
                segment = block.segment; next = block.first + block.samples.size();
                WriteRecordingChunk(block.samples.data(), block.samples.size());
                fft_pending.process(block.samples,[&](auto fft_block) {
                    fft_processor->processBlock(fft_block.data(),fft_block.size());
                    std::lock_guard lock(data_mutex);
                    output_data = fft_processor->getLatestMagnitudes();
                    is_data_ready.store(true);
                });
            }
        } catch (const std::exception& e) {
            { std::lock_guard lock(error_mutex); stream_error=e.what(); }
            stream_running.store(false);
        }
    });
    uint64_t first = 0;
    uint64_t last_drops=0, last_overflows=0;
    bool gap = true;
    try {
        while (stream_running.load()) {
            const size_t count = rx_stream->recv(usrpBuffer.data(),usrpBuffer.size(),md,0.1);
            const bool overflow = md.error_code == uhd::rx_metadata_t::ERROR_CODE_OVERFLOW;
            const bool error = md.error_code != uhd::rx_metadata_t::ERROR_CODE_NONE &&
                               md.error_code != uhd::rx_metadata_t::ERROR_CODE_TIMEOUT;
            if (overflow) ++rx_overflows;
            if (md.out_of_sequence) ++rx_sequence_errors;
            if (error && !overflow) ++rx_errors;
            if (error || md.out_of_sequence) {
                ++stream_segment; first=0; gap=true;
            }
            // Timeouts and short successful recv calls do not imply a gap.
            // UHD can return valid samples together with metadata; keep those samples.
            if (count) {
                const std::span<const std::complex<int16_t>> samples(usrpBuffer.data(),count);
                wifi.submit(samples,first,stream_segment,gap);
                processing.push(samples,first,stream_segment,gap);
                const auto stats=processing.stats();
                processing_drops += stats.dropped_samples-last_drops;
                processing_overflows += stats.overflows-last_overflows;
                last_drops=stats.dropped_samples; last_overflows=stats.overflows;
                first += count; gap=false;
            }
        }
    } catch (const std::exception& e) {
        { std::lock_guard lock(error_mutex); stream_error=e.what(); }
        stream_running.store(false);
    }
    processing.close(); processor.join();
    FinishRecording();
}

void USRPDevice::SetFFTSize(size_t fft_size)
{
	if (stream_running.load())
		return;

	const size_t min_fft_size = size_t{1} << MIN_FFT_EXPONENT;
	const size_t max_fft_size = size_t{1} << MAX_FFT_EXPONENT;
	if (fft_size < min_fft_size || fft_size > max_fft_size || (fft_size & (fft_size - 1)) != 0)
		throw std::invalid_argument("FFT size must be a power of two from 2^11 through 2^16");

	auto new_fft_processor = std::make_unique<RealTimeFFT>(fft_size);
	std::vector<std::complex<int16_t>> new_usrp_buffer(fft_size);
	fft_processor = std::move(new_fft_processor);
	usrpBuffer = std::move(new_usrp_buffer);
}

double USRPDevice::SetFrequency(const double frequency)
{
	if (stream_running.load()) throw std::logic_error("Stop and join RX before retuning");
	usrp_device->set_rx_freq(frequency * 1e6);
	wifi.enable(wifi.snapshot().enabled, sample_rate);
	center_freq = usrp_device->get_rx_freq();
	return center_freq / 1e6;
}

double USRPDevice::SetSamplingRate(const double rate)
{
	if (stream_running.load()) throw std::logic_error("Stop and join RX before changing rate");
	usrp_device->set_rx_rate(rate * 1e6);
	sample_rate = usrp_device->get_rx_rate();
	wifi.enable(wifi.snapshot().enabled, sample_rate);
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

	std::vector<int> decimation_vector = {1, 2, 3, 4, 5, 10, 20, 40, 60, 200, 1024};

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

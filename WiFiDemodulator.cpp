#include "WiFiDemodulator.h"

std::vector<uint8_t> demodulate_bpsk(const std::vector<std::complex<float>>& subcarriers)
{
	std::vector<uint8_t> bits;
	for (const auto& sample : subcarriers)
	{
		if (sample.real() > 0.0f)
		{
			bits.push_back(1);
		}
		else
		{
			bits.push_back(0);
		}
	}
	return bits;
}

void process_phy_demodulation(const std::vector<std::complex<int16_t>>& iq_buffer)
{
	if (iq_buffer.size() < 1000) return;

	size_t packet_start_index = 0;
	bool packet_found = false;

	// Обнаружение кадра
	for (size_t i = 0; i < iq_buffer.size() - OFDM_SYMBOL_SIZE; ++i)
	{
		std::complex<double> R = 0;
		double P = 0;
		for (int m = 0; m < STS_LENGTH; ++m)
		{
			std::complex<double> x1(iq_buffer[i + m].real(), iq_buffer[i + m].imag());
			std::complex<double> x2(iq_buffer[i + m + STS_LENGTH].real(), iq_buffer[i + m + STS_LENGTH].imag());

			R += x1 * std::conj(x2);
			P += std::norm(x1);
		}

		double metric = std::norm(R) / (P * P + 1e-6);
		if (metric > 0.75)
		{
			packet_start_index = i + 160;
			packet_found = true;
			break;
		}
	}

	if (!packet_found) return;
	std::cout << "[PHY] Found packet (sc16) on sample: " << packet_start_index << std::endl;

	// Удаление СР и нормализация для БПФ
	fftwf_complex* fft_in = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex) * FFT_SIZE);
	fftwf_complex* fft_out = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex) * FFT_SIZE);
	fftwf_plan plan = fftwf_plan_dft_1d(FFT_SIZE, fft_in, fft_out, FFTW_FORWARD, FFTW_ESTIMATE);

	for (int i = 0; i < FFT_SIZE; ++i)
	{
		// пропускаем 16 сэмплов СР
		std::complex<int16_t> raw_sample = iq_buffer[packet_start_index + CP_SIZE + i];
		fft_in[i][0] = static_cast<float>(raw_sample.real()) / 32768.0f;
		fft_in[i][1] = static_cast<float>(raw_sample.imag()) / 32768.0f;
	}

	std::vector<std::complex<float>> subcarriers(FFT_SIZE);
	for (int i = 0; i < FFT_SIZE; ++i)
	{
		subcarriers[i] = std::complex<float>(fft_out[i][0], fft_out[i][1]);
	}

	fftwf_destroy_plan(plan);
	fftwf_free(fft_in);
	fftwf_free(fft_out);

	// Работа с созвездием
	std::vector<uint8_t> decoded_bits = demodulate_bpsk(subcarriers);

	std::cout << "[PHY] First 10 bits of packet: ";
	for (int i = 0; i < 10; ++i) {
		std::cout << (int)decoded_bits[i] << " ";
	}
	std::cout << "..." << std::endl;
}

#pragma once
// Пока здесь код для WiFi 2.4
#include <fftw3.h>
#include <vector>
#include <complex>
#include <iostream>
#include "Core.h"

const int FFT_SIZE = 64;
const int CP_SIZE = 16; // циклический префикс
const int OFDM_SYMBOL_SIZE = 80; // размер символа ofdm
const int STS_LENGTH = 16; // Длина преамбулы

std::vector<uint8_t> demodulate_bpsk(const std::vector<std::complex<float>>& subcarriers);
void process_phy_demodulation(const std::vector<std::complex<int16_t>>& iq_buffer);
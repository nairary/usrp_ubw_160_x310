#pragma once

#include <vector>
#include <complex>
#include <cmath>
#include <numbers>
#include <fftw3.h>

#include "Core.h"

class RealTimeFFT {
public:
    RealTimeFFT(size_t fftSize);
    ~RealTimeFFT();
    RealTimeFFT(const RealTimeFFT&) = delete;
    RealTimeFFT& operator=(const RealTimeFFT&) = delete;
    void processBlock(const std::complex<int16_t>* inputBlock, size_t count);

    /**
     * Быстрое копирование кэша амплитуд для UI-потока.
     */
    std::vector<float> getLatestMagnitudes() const {
        return m_magnitudes;
    }

    size_t getFFTSize() const { return m_fftSize; }

private:
    void generateHannWindow();

    size_t m_fftSize;

    // Указатели FFTW (требуют специального выделения памяти)
    fftwf_complex* m_inputBuffer = nullptr;
    fftwf_complex* m_outputBuffer = nullptr;
    fftwf_plan m_plan = nullptr;

    // Стандартные контейнеры C++
    std::vector<float> m_window;
    std::vector<float> m_magnitudes;
};



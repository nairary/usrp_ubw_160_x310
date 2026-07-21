#include "ReaTimeFFT.h"

RealTimeFFT::RealTimeFFT(size_t fftSize)
    : m_fftSize(fftSize)
{
    // 1. Выделяем выровненную память для FFTW (для работы SSE/AVX инструкций)
    m_inputBuffer = (fftwf_complex*)fftwf_alloc_complex(m_fftSize);
    m_outputBuffer = (fftwf_complex*)fftwf_alloc_complex(m_fftSize);

    m_window.resize(m_fftSize);
    m_magnitudes.resize(m_fftSize);
   // m_magnitudes.resize(m_fftSize / 2 + 1); // Предел Найквиста

    generateHannWindow();

    // 2. Создаем план (FFTW_MEASURE тратит время при старте, но подбирает самый быстрый алгоритм)
    // Для real-time критически важно создать план ДО начала обработки данных.
    m_plan = fftwf_plan_dft_1d(
        static_cast<int>(m_fftSize),
        m_inputBuffer,
        m_outputBuffer,
        FFTW_FORWARD,
        FFTW_MEASURE
    );
}

RealTimeFFT::~RealTimeFFT()
{
    if (m_plan) {
        fftwf_destroy_plan(m_plan);
    }
    if (m_inputBuffer) {
        fftwf_free(m_inputBuffer);
    }
    if (m_outputBuffer) {
        fftwf_free(m_outputBuffer);
    }
}

void RealTimeFFT::processBlock(const std::complex<int16_t>* inputBlock, size_t count)
{
    if (count != m_fftSize || inputBlock == nullptr) return;
    for (size_t i = 0; i < m_fftSize; ++i) {
        float realNorm = static_cast<float>(inputBlock[i].real()) / float(m_fftSize);
        float imagNorm = static_cast<float>(inputBlock[i].imag()) / float(m_fftSize);

        m_inputBuffer[i][0] = realNorm * m_window[i]; // Real
        m_inputBuffer[i][1] = imagNorm * m_window[i]; // Imag
    }
    fftwf_execute(m_plan);

    size_t halfSize = m_fftSize / 2;
    for (size_t i = 0; i < m_fftSize; ++i) {
        float r = 0.0f;
        float im = 0.0f;
        if (i < halfSize)
        {
            r = m_outputBuffer[i + halfSize][0];
            im = m_outputBuffer[i + halfSize][1];
        }
        else
        {
            r = m_outputBuffer[i- halfSize][0];
            im = m_outputBuffer[i- halfSize][1];
        }
        m_magnitudes[i] = 20 * std::log10(std::sqrt(r * r + im * im));
    }
}

void RealTimeFFT::generateHannWindow()
{
    for (size_t i = 0; i < m_fftSize; ++i) {
        m_window[i] = 0.5f * (1.0f - std::cos(2.0f * std::numbers::pi * i / (m_fftSize - 1)));
    }
}

#pragma once
#include <cstddef>
#include <complex>
#include <cstdint>
#include <istream>
#include <vector>
// Explicit little-endian decoding, independent of host representation/endianness.
std::vector<std::complex<int16_t>> readSc16(std::istream& input, size_t samples);

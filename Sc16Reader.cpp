#include "Sc16Reader.h"
#include <bit>
#include <limits>
#include <stdexcept>
std::vector<std::complex<int16_t>> readSc16(std::istream& input, size_t samples) {
    if (!samples || samples > static_cast<size_t>(std::numeric_limits<std::streamsize>::max())/4)
        throw std::invalid_argument("invalid sc16 block size");
    std::vector<unsigned char> bytes(samples*4);
    input.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
    const size_t count=static_cast<size_t>(input.gcount());
    if (input.bad() || (input.fail() && !input.eof())) throw std::runtime_error("IQ read failed");
    if (count%4) throw std::runtime_error("truncated sc16 I/Q sample");
    std::vector<std::complex<int16_t>> result;
    result.reserve(count/4);
    for (size_t i=0;i<count;i+=4) {
        const auto decode=[&](size_t n){return std::bit_cast<int16_t>(static_cast<uint16_t>(bytes[n] | (uint16_t(bytes[n+1])<<8)));};
        result.emplace_back(decode(i),decode(i+2));
    }
    return result;
}

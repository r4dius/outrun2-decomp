#pragma once
// SHA-256 (FIPS 180-4), used to check the player's game files.
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
namespace outrun::platform {
using Sha256Digest=std::array<std::uint8_t,32>;
class Sha256 {
public:
    Sha256();
    void update(const std::uint8_t* data,std::size_t size);
    Sha256Digest finish();
private:
    void block(const std::uint8_t* p);
    std::uint32_t h_[8];
    std::uint8_t buffer_[64];
    std::size_t used_=0;
    std::uint64_t total_=0;
};
Sha256Digest sha256(const std::uint8_t* data,std::size_t size);
std::string sha256_hex(const Sha256Digest& digest);
}

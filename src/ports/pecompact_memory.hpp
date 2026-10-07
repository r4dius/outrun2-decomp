#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace outrun_pecompact {
constexpr std::uint32_t image_base = 0x400000;
constexpr std::size_t image_size = 0x590000;
// Supported input: Steam OR2006C2C.EXE, 962560 bytes, SHA-256
// 4876712f501640951d892069db826e431aede3e4ab87686fc1d556e26df9fec1.
// Output is a data image indexed by RVA (VA - image_base), not a runnable PE.
// No file access, native code execution, import resolution or OS API calls.
// On failure, image is unchanged. Input and output may alias.
bool decompress(const std::uint8_t* file, std::size_t size,
                std::vector<std::uint8_t>& image, std::string* error = nullptr);
}

#include "pecompact_memory.hpp"
#include <algorithm>
#include <array>
#include <exception>
#include <stdexcept>
#include <utility>

namespace outrun_pecompact {
namespace {
struct Invalid : std::runtime_error { using std::runtime_error::runtime_error; };
struct Reader {
    const std::uint8_t* data;
    std::size_t size, pos = 0;
    Reader(const std::uint8_t* p, std::size_t n) : data(p), size(n) {}
    std::uint8_t byte() {
        if (pos == size) throw Invalid("truncated compressed stream");
        return data[pos++];
    }
};
std::uint32_t rd32(const std::uint8_t* p) {
    return std::uint32_t(p[0]) | (std::uint32_t(p[1]) << 8) |
           (std::uint32_t(p[2]) << 16) | (std::uint32_t(p[3]) << 24);
}
void wr32(std::uint8_t* p, std::uint32_t v) {
    for (unsigned i = 0; i < 4; ++i) p[i] = std::uint8_t(v >> (i * 8));
}
void match(std::vector<std::uint8_t>& out, std::size_t offset,
           std::size_t count, std::size_t limit) {
    if (!offset || offset > out.size() || count > limit - out.size())
        throw Invalid("invalid compressed match");
    // Sequential copying is essential: matches may overlap themselves.
    while (count--) out.push_back(out[out.size() - offset]);
}
// Translation of this build's small stage-one bitstream decoder.
std::vector<std::uint8_t> stage_one(Reader src, std::size_t limit) {
    std::vector<std::uint8_t> out;
    out.reserve(limit);
    unsigned tag = 0, bits = 0, state = 2;
    std::size_t previous = 0;
    auto bit = [&]() {
        if (!bits) { tag = src.byte(); bits = 8; }
        const auto value = tag >> 7;
        tag = (tag << 1) & 255; --bits; return value;
    };
    auto gamma = [&]() {
        std::size_t value = 1;
        do {
            if (value > limit / 2) throw Invalid("stage-one integer overflow");
            value = value * 2 + bit();
        } while (bit());
        return value;
    };
    out.push_back(src.byte());
    for (;;) {
        if (!bit()) {
            if (out.size() == limit) throw Invalid("stage-one output overflow");
            out.push_back(src.byte()); state = 2;
        } else if (!bit()) {
            auto offset = gamma();
            if (offset < state) throw Invalid("stage-one invalid offset");
            offset -= state;
            std::size_t count;
            if (!offset) { count = gamma(); offset = previous; }
            else {
                offset = ((offset - 1) << 8) + src.byte(); count = gamma();
                if (offset >= 32000) count += 2;
                else if (offset >= 1280) ++count;
                else if (offset <= 127) count += 2;
                previous = offset;
            }
            match(out, offset, count, limit); state = 1;
        } else if (!bit()) {
            const auto value = src.byte(); const auto offset = value >> 1;
            if (!offset) {
                if (out.size() != limit) throw Invalid("stage-one size mismatch");
                return out;
            }
            match(out, offset, 2 + (value & 1), limit); previous = offset; state = 1;
        } else {
            unsigned offset = 0;
            for (unsigned i = 0; i < 4; ++i) offset = offset * 2 + bit();
            if (offset) match(out, offset, 1, limit);
            else {
                if (out.size() == limit) throw Invalid("stage-one output overflow");
                out.push_back(0);
            }
            state = 2;
        }
    }
}
// Build-specific raw LZMA1 decoder: lc=8, lp=0, pb=2.
// Probabilities are 16-bit; addresses in the original x86 code never become
// host pointers. This configuration exceeds liblzma's lc+lp <= 4 restriction.
struct Lzma {
    Reader src;
    std::uint32_t range = 0xffffffffu, code = 0;
    std::vector<std::uint16_t> probabilities;
    explicit Lzma(Reader input) : src(input), probabilities(1846 + (768 << 8), 1024) {
        if (src.byte() != 0) throw Invalid("invalid LZMA range header");
        for (unsigned i = 0; i < 4; ++i) code = (code << 8) | src.byte();
    }
    void normalize() {
        if (range < (1u << 24)) { range <<= 8; code = (code << 8) | src.byte(); }
    }
    unsigned bit(std::size_t index) {
        if (index >= probabilities.size()) throw Invalid("LZMA model overflow");
        auto& p = probabilities[index]; const auto bound = (range >> 11) * p;
        unsigned value;
        if (code < bound) { range = bound; p = std::uint16_t(p + ((2048 - p) >> 5)); value = 0; }
        else { range -= bound; code -= bound; p = std::uint16_t(p - (p >> 5)); value = 1; }
        normalize(); return value;
    }
    std::uint32_t tree(std::size_t base, unsigned bits) {
        std::uint32_t v = 1; const auto root = 1u << bits;
        while (bits--) v = v * 2 + bit(base + v);
        return v - root;
    }
    std::uint32_t reverse(std::size_t base, unsigned bits) {
        std::uint32_t v = 1, result = 0;
        for (unsigned i = 0; i < bits; ++i) {
            const auto b = bit(base + v); v = v * 2 + b; result |= b << i;
        }
        return result;
    }
    std::uint32_t direct(unsigned bits) {
        std::uint32_t result = 0;
        while (bits--) {
            range >>= 1; const unsigned b = code >= range;
            if (b) code -= range;
            result = result * 2 + b; normalize();
        }
        return result;
    }
    std::uint32_t length(std::size_t base, unsigned position) {
        if (!bit(base)) return tree(base + 2 + position * 8, 3);
        if (!bit(base + 1)) return 8 + tree(base + 130 + position * 8, 3);
        return 16 + tree(base + 258, 8);
    }
    std::vector<std::uint8_t> decode(std::size_t size) {
        std::vector<std::uint8_t> out; out.reserve(size);
        unsigned state = 0; std::array<std::uint32_t, 4> reps{};
        while (out.size() < size) {
            const unsigned position = unsigned(out.size() & 3);
            if (!bit(state * 16 + position)) {
                const auto prev = out.empty() ? 0 : out.back();
                const std::size_t base = 1846 + 768 * std::size_t(prev);
                unsigned symbol = 1;
                if (state >= 7) {
                    if (reps[0] >= out.size()) throw Invalid("invalid LZMA literal match");
                    unsigned value = out[out.size() - reps[0] - 1];
                    while (symbol < 256) {
                        const auto mb = (value >> 7) & 1; value = (value << 1) & 255;
                        const auto b = bit(base + ((1 + mb) << 8) + symbol);
                        symbol = symbol * 2 + b; if (mb != b) break;
                    }
                }
                while (symbol < 256) symbol = symbol * 2 + bit(base + symbol);
                out.push_back(std::uint8_t(symbol - 256));
                state = state < 4 ? 0 : state < 10 ? state - 3 : state - 6;
                continue;
            }
            std::uint32_t count;
            if (bit(192 + state)) {
                if (!bit(204 + state)) {
                    if (!bit(240 + state * 16 + position)) {
                        state = state < 7 ? 9 : 11; count = 1;
                    } else { count = length(1332, position) + 2; state = state < 7 ? 8 : 11; }
                } else {
                    const unsigned index = !bit(216 + state) ? 1 : !bit(228 + state) ? 2 : 3;
                    const auto dist = reps[index];
                    for (unsigned i = index; i > 0; --i) reps[i] = reps[i - 1];
                    reps[0] = dist; count = length(1332, position) + 2; state = state < 7 ? 8 : 11;
                }
            } else {
                count = length(818, position) + 2; state = state < 7 ? 7 : 10;
                const auto slot = tree(432 + std::min(count - 2, 3u) * 64, 6);
                std::uint32_t dist = slot;
                if (slot >= 4) {
                    const auto bits = (slot >> 1) - 1; dist = (2 + (slot & 1)) << bits;
                    if (slot < 14) dist += reverse(688 + dist - slot - 1, bits);
                    else dist += direct(bits - 4) * 16 + reverse(802, 4);
                }
                for (unsigned i = 3; i > 0; --i) reps[i] = reps[i - 1];
                reps[0] = dist;
            }
            match(out, std::uint64_t(reps[0]) + 1, count, size);
        }
        return out;
    }
};
std::uint64_t fingerprint(const std::uint8_t* file, std::size_t size) {
    std::uint64_t h = 14695981039346656037ull;
    for (std::size_t i = 0; i < size; ++i) h = (h ^ file[i]) * 1099511628211ull;
    return h;
}
void move_section(std::vector<std::uint8_t>& image, std::size_t dst,
                  std::size_t src, std::size_t count) {
    const std::vector<std::uint8_t> temp(image.begin() + src, image.begin() + src + count);
    std::fill_n(image.begin() + src, count, 0);
    std::copy(temp.begin(), temp.end(), image.begin() + dst);
}
}
bool decompress(const std::uint8_t* file, std::size_t size,
                std::vector<std::uint8_t>& image, std::string* error) {
    try {
        // Fixed offsets below apply only to this exact build. FNV is an input
        // compatibility/corruption check, not a cryptographic authenticity check.
        if (!file || size != 962560 || fingerprint(file, size) != 0xde76e0a0a491d8b4ull)
            throw Invalid("unsupported file: expected the 962560-byte Steam PECompact build");
        const auto stage = stage_one({file + 0xea068, 3460}, 7368);
        if (rd32(stage.data() + 0x97c) != 0x37f466 || rd32(stage.data() + 0x98c) != 6)
            throw Invalid("unexpected PECompact metadata");
        // Restore the seven bytes overwritten by the entry-point trampoline.
        std::vector<std::uint8_t> stream(file + 0x400, file + 0x400 + 0xe901b);
        std::copy_n(file + 0xea05f, 7, stream.begin());
        if (rd32(stream.data()) != 0x37e466 || stream[4] != 98)
            throw Invalid("unexpected LZMA properties");
        Lzma decoder({stream.data() + 17, stream.size() - 17});
        const auto payload = decoder.decode(0x37e466);
        std::vector<std::uint8_t> result(image_size, 0);
        std::copy(payload.begin(), payload.end(), result.begin() + 0x1000);
        std::copy_n(file + 0xe9600, 0x1a00, result.begin() + 0x58e000);
        move_section(result, 0x22f000, 0x22e367, 0x1510ff);
        move_section(result, 0x196000, 0x195881, 0x98ae6);
        // Undo PECompact's E8/E9 address filter over the original code bytes.
        for (std::size_t i = 0; i < 0x19487b;) {
            const auto op = result[0x1000 + i++];
            if (op != 0xe8 && op != 0xe9) continue;
            auto* p = result.data() + 0x1000 + i;
            if (p[0] == (op == 0xe8 ? 0x2c : 0x62)) {
                const std::uint32_t address = (std::uint32_t(p[1]) << 16) |
                                             (std::uint32_t(p[2]) << 8) | p[3];
                wr32(p, address - std::uint32_t(i - 1));
            }
            i += 4;
        }
        // Same normalization as the project's exe_image_map_pe().
        std::fill_n(result.begin() + 0x196000, 0x300, 0);
        image = std::move(result);
        if (error) error->clear();
        return true;
    } catch (const std::exception& e) {
        if (error) *error = e.what();
        return false;
    }
}
}

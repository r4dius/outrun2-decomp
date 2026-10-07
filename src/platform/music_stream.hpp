#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace outrun::platform {
// PC 401000 / 42FBA0 streamed BGM: ".\Sound\%s" of the track table 771AD8
// (Ogg Vorbis), played on the single stream object 95B24C (a new request
// closes the previous stream; 401030 / 401050 close it). The volume word
// 95B240 is .bss 0 (0 dB, unattenuated) and a stream is only started while it
// is above -10000. Decoding is libvorbisfile; the device side is the shared
// 48 kHz stereo mix of MenuAudio (linear resampling from the file rate).
const char* music_track_name_771ad8(unsigned track);   // nullptr outside 0..0x42
constexpr unsigned MusicTrackCount=0x43u;

class MusicStream {
public:
    MusicStream();
    ~MusicStream();
    MusicStream(const MusicStream&)=delete;
    MusicStream& operator=(const MusicStream&)=delete;
    // Takes the whole file (the retail Sound folder); false with the reason
    // when the file is not an Ogg Vorbis stream or no decoder was built.
    bool open(std::vector<std::uint8_t> file,bool loop,std::string& error);
    void stop();
    bool playing()const;
    // Adds `frames` 48 kHz stereo frames (scaled by gain) to acc (interleaved).
    bool mix_add(double* acc,std::size_t frames,double gain,std::string& error);
    struct Stats { std::uint64_t opens{},stops{},frames{},loops{},ends{}; std::uint32_t rate{},channels{}; } stats;
    static bool decoder_available();
private:
    bool refill(std::string& error);
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}

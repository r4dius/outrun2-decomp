#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
namespace outrun::ps5_runtime {
// Owns the native output session and DMA buffers until the service releases them.
class Ps5Audio {
public:
    Ps5Audio()=default;
    ~Ps5Audio();
    Ps5Audio(const Ps5Audio&)=delete;
    Ps5Audio& operator=(const Ps5Audio&)=delete;
    bool open(std::string& error);
    bool submit(const std::vector<std::int16_t>& pcm,std::string& error);
    void close();
    std::uint64_t submitted_frames()const{return submitted_;}
    // Stereo frames handed to the service and not yet released, plus frames
    // waiting for a free buffer: the output latency still ahead of the mixer.
    std::size_t queued_frames()const;
private:
    void* impl_{};
    std::uint64_t submitted_{};
};
}

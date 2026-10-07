#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
namespace outrun::switch_runtime {
// Owns the native output session and DMA buffers until the service releases them.
class SwitchAudio {
public:
    SwitchAudio()=default;
    ~SwitchAudio();
    SwitchAudio(const SwitchAudio&)=delete;
    SwitchAudio& operator=(const SwitchAudio&)=delete;
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

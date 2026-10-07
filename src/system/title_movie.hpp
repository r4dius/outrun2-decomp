#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace outrun::platform {
class TitleMovie {
public:
    TitleMovie()=default;
    ~TitleMovie();
    TitleMovie(const TitleMovie&)=delete;
    TitleMovie& operator=(const TitleMovie&)=delete;
    bool open(const std::string& path,std::string& error);
    bool advance(double elapsed_seconds,std::string& error);
    void close();
    const std::vector<std::uint8_t>& rgba()const{return pixels_;}
    unsigned width()const{return width_;}
    unsigned height()const{return height_;}
    std::uint64_t decoded_frames()const{return decoded_;}
    // Interleaved signed-16 stereo at 48 kHz, decoded from the movie itself.
    void take_audio(std::vector<std::int16_t>& samples){samples.clear();samples.swap(audio_);}
    std::uint64_t decoded_audio_frames()const{return audio_frames_;}
    bool has_audio()const{return has_audio_;}
    bool active()const{return impl_!=nullptr;}
private:
    void* impl_{};
    std::vector<std::uint8_t> pixels_;
    unsigned width_{},height_{};
    std::uint64_t decoded_{};
    std::vector<std::int16_t> audio_;
    std::uint64_t audio_frames_{};
    bool has_audio_{};
};
}

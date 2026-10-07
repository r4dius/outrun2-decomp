#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include "platform/music_stream.hpp"
#include "platform/pc_sound.hpp"

namespace outrun::platform {
// MENU/FE banks of PC 42F4C0/42F0D0. Device conversion is 48 kHz stereo S16;
// command IDs, source rates, gain and voice limits remain PC data.
class MenuAudio {
    struct Voice { std::uint64_t phase{}; bool playing{},loop{}; double left{},right{}; };
    struct Sound { unsigned id{},rate{},category{}; std::vector<std::int16_t> pcm; std::vector<Voice> voices; };
    std::vector<Sound> sounds_;
    std::vector<std::pair<Sound*,Voice*>> active_;   // playing voices of the current mix call
    std::vector<std::int16_t> movie_;
    std::size_t movie_offset_{};
    bool loaded_{};
    MusicStream music_;
    std::vector<double> music_mix_;
    std::string music_error_;
    std::int32_t music_volume_95b240_{};
    PcSound* driver_{};      // race banks / ICS voices mixed with the menu voices
    bool play(unsigned command,std::string&);
    bool load_bank(const std::vector<std::uint8_t>&,bool frontend,std::string&);
public:
    struct Stats { std::uint64_t commands{},starts{},saturated{},stops{},mixed_frames{},movie_frames{}; } stats;
    bool load(const std::vector<std::uint8_t>&,std::string&);
    bool load_file(const std::string&,std::string&);
    bool load_frontend_file(const std::string&,std::string&);
    void clear();
    // False means unimplemented/unavailable resource, NOT a successful silent
    // device. PC's valid command return value is always zero (incl. saturation).
    bool command(unsigned,std::string&);
    bool queue_movie(const std::vector<std::int16_t>&,std::string&);
    void stop_movie(); // does not stop effects or close the shared output device
    void stop_effects(); // every effect voice (race 427630 ClearAllSound); not the movie
    bool mix(std::size_t frames,std::vector<std::int16_t>&,std::string&);
    // Streamed BGM (401000 / 401030): one stream, mixed at 95B240 millibels (0 at boot).
    bool play_music(std::vector<std::uint8_t> file,bool loop,std::string& error){return music_.open(std::move(file),loop,error);}
    // 42FC90(level): trunc(float(log10(level) * 3000 - 600)), -10000 for level <= 0.
    void music_volume_42fc90(float level);
    std::int32_t music_volume()const{return music_volume_95b240_;}
    void stop_music(){music_.stop();}
    const MusicStream& music()const{return music_;}
    const std::string& music_error()const{return music_error_;}
    void bind_driver(PcSound* driver){driver_=driver;}
    std::size_t active_voices()const;
    bool loaded()const{return loaded_;}
};
}

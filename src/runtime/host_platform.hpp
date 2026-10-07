#pragma once
// What a platform gives the shared game host (runtime/game_host.hpp): the
// pad in one button layout, an audio output and the presentation layer that
// shows the frontend, the title movie and the PC scene. The Switch
// (switch/source), macOS (mac/) and the Linux host runner implement them;
// nothing here names a platform API.
#include "platform/frontend_list.hpp"
#include "platform/frontend_text.hpp"
#include "platform/frontend_window.hpp"
#include "platform/vehicle_visual.hpp"
#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
namespace outrun::platform { class PcD3D9Device; }
namespace outrun::runtime {
// Pad buttons (the libnx HidNpadButton bits; other platforms map onto them).
enum PadButton : std::uint64_t {
    PadA=1ull<<0,PadB=1ull<<1,PadX=1ull<<2,PadY=1ull<<3,PadStickL=1ull<<4,PadStickR=1ull<<5,
    PadL=1ull<<6,PadR=1ull<<7,PadZL=1ull<<8,PadZR=1ull<<9,PadPlus=1ull<<10,PadMinus=1ull<<11,
    PadLeft=1ull<<12,PadUp=1ull<<13,PadRight=1ull<<14,PadDown=1ull<<15,
};
struct PadStick { std::int32_t x{},y{}; };           // -32768..32767, y up
struct PadSnapshot {
    bool connected{};
    std::uint64_t held{},down{},up{};                 // PadButton bits
    PadStick left,right;
};
class AudioOutput {
public:
    virtual ~AudioOutput()=default;
    virtual bool open(std::string& error)=0;
    virtual bool submit(const std::vector<std::int16_t>& stereo_pcm,std::string& error)=0;   // 48 kHz
    virtual void close()=0;
    virtual std::uint64_t submitted_frames()const=0;
    // Frames handed to the output and not played yet (the latency ahead of the mixer).
    virtual std::size_t queued_frames()const=0;
};
// The platform renderer as the frame code drives it. Every setter returns
// false when the renderer refused the request (the host then stops).
class Presenter {
public:
    virtual ~Presenter()=default;
    virtual bool set_start_loading_scene(std::uint32_t scene)=0;
    virtual bool set_start_loading_visible(bool)=0;
    virtual bool set_frontend_visible(bool)=0;
    virtual bool set_frontend_glyphs(const std::vector<platform::FrontendGlyph>&)=0;
    virtual bool set_frontend_images(const std::vector<platform::FrontendListImage>&)=0;
    virtual bool set_frontend_icons(const std::vector<platform::FrontendWindowIcon>&)=0;
    virtual bool set_frontend_token(std::uint32_t token)=0;
    virtual bool set_frontend_overlay(std::uint32_t token,float frame)=0;
    virtual bool set_frontend_game_backdrop(bool)=0;
    virtual bool set_movie_frame(const std::uint8_t* rgba,std::uint32_t width,std::uint32_t height)=0;
    virtual bool set_pc_scene(std::function<void(platform::PcD3D9Device&)> scene)=0;
    virtual bool draw()=0;
    // Authored SUMO_FE animation of the frontend finished (fed back to key 22).
    virtual bool frontend_animation_complete()const=0;
    // The 2D layer of the PC scene drawn at full resolution from here (42D710).
    virtual void scene_to_frame()=0;
    // Development: next PC shader diagnostic mode / uber shaders only.
    virtual void cycle_pc_diagnostic()=0;
    virtual void disable_pc_specialised_shaders()=0;
};
}

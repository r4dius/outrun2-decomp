#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace outrun::platform {
// PC sound driver on a native 48 kHz stereo mixer:
//   banks      427700 request / 4278D0 unload / 4276B0 unload all but 5,A,B
//              (42F620 file states, 42F4C0 buffers, 42F290 release)
//   effects    42F0D0 command / 42F1A0 play / 427630 ClearAllSound
//   engine     42EFF0 ICS channel parameters, 42F790 definition, 42FA30
//              curves, 42F330 tick (every 4th call: 42F850 volume/pan,
//              42F980 frequency, 412120 fades)
//   voices     the DirectSound layer 411CD0..412C60: one buffer per sound id
//              (7763F0) or ICS sample (7790F8), each with its voice copies
//              (773868 / 774170); volume and pan in millibels, frequency in Hz.
// Tables come from the EXE (pc_sound_tables.inc). The 412770 refill 4124E0
// (arg5 != 0) rewrites the buffer with its own sample data: no audible effect.
struct PcSoundIdInfo { std::uint16_t rate,category; std::uint32_t voices,voices_ics; std::uint16_t ics_rate; };
struct PcSoundBankInfo { const char* name; std::uint16_t first,count; };
struct PcIcsDefinition { std::uint32_t id; std::uint16_t first_layer,layers; };

class PcSound {
public:
    using FileReader=std::function<bool(const std::string& name,std::vector<std::uint8_t>& bytes,std::string& error)>;
    static constexpr std::uint32_t Ids=0x241u,Slots=12u,Channels=17u,Layers=16u;
    FileReader read_file;                 // "Sound/<name>" of the retail tree
    bool ready{};                         // 95B248 (the audio device is open)
    std::uint32_t language_95b210{};      // 427DB0: 4493C0
    std::uint32_t voice_set_754b0c{4u};   // .data 4 (SPRACE)
    std::int32_t master_957be4{0x41};     // 424600 -> 42EE10(A0010000, 0x40): 0x40+1
    bool mute_95b250{};                   // 42F320 (pause)

    PcSound();
    void boot_424600();                                     // slots empty, master 65
    // 42EFA0(level) = 42EE10(A0010000, (short)level >> 1): master = that + 1
    // (no 42EF70 duck is modelled). Effects started afterwards use it; ICS
    // layers pick it up at their next 42F850 tick.
    void master_volume_42efa0(std::uint32_t level){master_957be4=(std::int32_t(std::int16_t(level))>>1)+1;}
    std::uint32_t request_427700(std::uint32_t arg);        // 0 done, 1 busy
    void unload_4278c0(std::uint32_t arg){unload_4278d0((arg&0xffffu)>>8);}
    void unload_4278d0(std::uint32_t slot);
    void unload_all_4276b0();
    void voice_set_427db0(std::uint32_t language,bool heart_attack,std::uint32_t variant_780258,std::uint32_t profile_208);
    void clear_all_427630();
    void effect_42f0d0(std::uint32_t command);
    void ics_42eff0(std::uint32_t channel,std::uint32_t value,std::uint32_t code);
    void tick_42f330();
    // Adds `frames` 48 kHz stereo frames to acc (interleaved).
    void mix_add(double* acc,std::size_t frames);

    struct Stats {
        std::uint64_t requests{},loads{},load_bytes{},unloads{},effects{},effect_plays{},effect_stops{},
            unmapped{},saturated{},ics_params{},ics_starts{},ics_stops{},ticks{},fades{},mixed_frames{},replaced{};
    } stats;
    std::map<std::uint32_t,std::uint32_t> unmapped_ids;    // 42F1A0 "not loaded" id -> count
    std::vector<std::string> errors;
    std::string status()const;
    bool slot_has(std::uint32_t slot,std::uint32_t bank)const{return slot<Slots&&slot_bank_[slot]==bank;}
    // 42F1A0 would play it: the id is mapped to a slot and has a buffer.
    bool mapped(std::uint32_t command)const{const auto id=command&0x7ffu;return id<Ids&&slot_[id]<0xffu&&bool(se_[id]);}

private:
    struct Voice {
        bool playing{},loop{};
        double pos{};
        std::int32_t volume{},pan{};
        std::uint32_t freq{};
        float fade_in_ms{},fade_out_ms{};   // +38 / +3C
        std::int32_t fade_from{};           // +40
        double fade_start{};                // +44
    };
    struct Buffer {
        std::shared_ptr<const std::vector<std::int16_t>> pcm;
        std::uint32_t rate{};
        std::vector<Voice> voices;
        explicit operator bool()const{return bool(pcm);}
    };
    struct Layer { std::int32_t pitch{},volume{},pan{0x40},prev_pitch{},prev_volume{},prev_pan{},unused18{},unused1c{},voice{-1}; };
    struct Channel {
        std::int32_t volume{},pitch{},pan{},unused0c{},unused10{},x{};   // +0 +4 +8 +C +10 +14
        std::uint32_t id{0xffffffffu};                                  // +18
        const PcIcsDefinition* def{};                                   // +1C
        std::array<Layer,Layers> layers{};
    };
    struct FadeRef { Buffer* buffer; std::size_t voice; };

    std::array<Buffer,Ids> se_{},ics_{};
    std::array<std::uint16_t,Ids> index_{},slot_{};       // 7763E0 / 7763E2
    std::array<std::uint16_t,Slots> slot_bank_{};          // 956128
    std::array<std::uint32_t,Slots> slot_state_{};         // 95A450+8
    std::uint32_t request_state_956110{};
    std::uint32_t tick_98abd8{};
    std::array<Channel,Channels> channels_{};
    std::vector<FadeRef> fade_in_,fade_out_;               // 74358C lists (32 each)
    std::vector<double> scratch_;

    double now_ms()const;
    std::int32_t load_42f620(std::uint32_t slot,const std::string& name);
    void parse_42f4c0(const std::vector<std::uint8_t>& pak,std::uint32_t slot);
    void release_42f290(std::uint32_t slot);
    void stop_slot_42f160(std::uint32_t slot);
    void play_42f1a0(std::uint32_t command);
    std::int32_t play_412770(Buffer&,bool loop,std::int32_t volume,std::int32_t freq,std::int32_t pan,float arg5,float arg6);
    void stop_412880(Buffer&,std::int32_t voice);
    void stop_4129c0(Buffer&,std::int32_t voice);
    void definition_42f790(std::uint32_t ch,std::uint32_t id);
    void curves_42fa30(std::uint32_t ch);
    void volume_pan_42f850(std::uint32_t ch,std::uint32_t layer);
    void frequency_42f980(std::uint32_t ch,std::uint32_t layer);
    void fades_412120();
    void error(const std::string&);
};
}

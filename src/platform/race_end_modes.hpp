#pragma once
// PC race end modes (OR2006C2C.EXE mode table 5995B8):
//   20 TIME OVER   49A1E0 init / 49A280 control / 49D1E0 exit (49A4F0)
//   25 RESULT      49A790 / 49A800 / 49A920  (event 0x188: function 0x15
//                  route map 4AE750/4AE960/4AED20, or 0x50 Time Attack
//                  4AED50/4AEEC0/4AEEE0; SPRANI bank 0x34 ROUTE_CVT)
//   27 GAME OVER   49ADC0 / 49AEA0 / 49AED0  (bookkeeping 452xxx, 453370)
//   28 TO MENU     49B170 / 49AF90 / (49A650)  (49AD00 closes the race)
// Line-by-line transliteration over PC addresses (PcRaceMemory): the
// caller maps the blocks of PcRaceEndState, the race globals read here
// (780258 variant, 78024C preset, 799D18 -> the player car, 8A8CDC, the
// active license 7C23E0, 6840EC/6840F0/6840F4 = 4AEEF0's goal words) and a
// scratch page at PcRaceEndLocals (the PC stack matrices passed by
// address). Every other PC function is one PcRaceService call:
//   mode globals   43F900(n) 43FA90 43F980 43F990(m) 43F8C0(m) 43F8E0
//                  43F860 43F960 4957F0 55A930(ECX=7F9460) 4AEF20
//   events         440A10(first,n) 440A30(first,n) 440330(first,n)
//                  4401D0(id) 440110(id,function)
//   race manager   4505D0 450240 450230(v) 451350(i) 44C940(k) 4505A0(i)
//                  4505E0 4505F0(i) 4506A0(i) 47EF30(preset)
//   course release 4489C0 44C3D0 44A1A0 43DE50(i) 4F11B0(i) 4F0600(i)
//                  46FC30(i) 4F2210 44FD00 42EBF0 4489F0 42DFD0 429A10
//                  47ED40 465FA0 451A00 4276B0 45AEF0 4999D0
//   resources      42DEB0(id,mode) 429920(id,mode) 42DF90 4299A0
//                  42DFB0(id) 4299C0(id)
//   sprites        428320(token,layer,mode) 428460(token,layer,mode,
//                  first,last) 4285A0(h) 428770(h,mode) 4287B0(h,&matrix)
//                  428800(h,speed bits) 428880(h) 428600
//   audio          427630 401000(channel,track,loop) 401030(channel)
//   input          4035F0 (the event-36 object) 4536F0(mask)
//   other          42E020(a,b,c) 455710 4EDCE0(ECX,arg) 498400 48B140
//                  48B160 48B1F0 47F110 496170 4E85E0 496180 4E8620
//                  499890 455730
// 428440(token,layer,first,last) is called as 428460(token,layer,0,first,
// last) and D3DXMatrixTranslation is computed here (identity + x, y, z).
#include "platform/race_area.hpp"
#include <array>
#include <cstdint>
namespace outrun::platform {
constexpr std::uint32_t PcRaceEndLocals=0x7ffd0000u;   // PC stack matrices passed to 4287B0
struct PcRaceEndState {
    std::array<std::uint8_t,2> bytes_836ce0{};          // 836CE0, 836CE1
    std::array<std::uint8_t,16> words_836d08{};         // 836D08 result, 836D0C name, 836D10 to-menu stages, 836D14 mode 33 state
    std::array<std::uint8_t,4> word_8367ac{};           // 47EF30 ranking entry
    static constexpr std::uint32_t RouteBase=0x842048u,RouteEnd=0x842100u;
    std::array<std::uint8_t,RouteEnd-RouteBase> route{}; // 4AE750 block (842048..8420FF)
    std::array<std::uint8_t,8> common_687774{{0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff}};   // .data -1, -1
    std::array<std::uint8_t,4> common_842888{};
    std::array<std::uint8_t,0x2c> arcade_84288c{};       // 84288C..8428B7: the entry module's network wait banner (4B88D0..4B8C00)
    std::array<std::uint8_t,0x68> arcade_842820{};       // 842820..842887: the arcade entry module (4B6F80, 4B7380 / 4B7450)
    static constexpr std::uint32_t NameBase=0x842288u,NameEnd=0x842820u;
    std::array<std::uint8_t,NameEnd-NameBase> name_842288{}; // the name entry module (race_name_entry.cpp: 4B07B0..4B5141)
    // OUTRUN2SP arcade frontend (arcade_attract.cpp): the event-4 work, its .bss blocks (84A320 =
    // the 446xxx embedded slots object, 0x68C bytes) and the .data tables it writes (EXE images).
    std::array<std::uint8_t,0x70> work_780440{};
    std::array<std::uint8_t,0x844ac0u-0x844928u> arcade_844928{};
    std::array<std::uint8_t,0x84aa00u-0x84a208u> arcade_84a208{};
    std::array<std::uint8_t,0x68a000u-0x6898c0u> arcade_6898c0{};
    std::array<std::uint8_t,0x20> arcade_65a7a0{};
    std::array<std::uint8_t,0x20> attract_830360{};       // 830368 attract state, 83037C frames, 830380 (never set)
    // [63960C] = 7D3A90: the rankings book (0x2C90 bytes, rankings.dat; the
    // first 0x100 are the bookkeeping header), set up by 453440 at boot
    // (sp_rankings.cpp), and the SP ranking tables 84DF40..850B00.
    static constexpr std::uint32_t BookBase=0x7d3a90u,BookSize=0x2c90u;
    std::array<std::uint8_t,BookSize> bookkeeping{};
    static constexpr std::uint32_t SpTablesBase=0x84df40u,SpTablesEnd=0x850b00u;
    std::array<std::uint8_t,SpTablesEnd-SpTablesBase> sp_tables{};
    std::array<std::uint8_t,0x44> stats_7d6720{};       // 7D6720..7D6763 (7D6764 is the race manager's)
    std::array<std::uint8_t,8> stats_7d6768{};          // 7D6768..7D676F
    std::array<std::uint8_t,0x60> boot_83dae0{};         // 83DAE0..83DB3F: modes 1..5 / 8 (logos, demo route) stage words
    std::array<std::uint8_t,0x20> adv_7f1958{};         // 45AE10: 7F1960 = 2, 7F196C = 0xF
    std::array<std::uint8_t,4> flag_67ee3c{};
    std::array<std::uint8_t,4> flag_65994c{};
    std::array<std::uint8_t,0x14> words_836d18{};       // 836D18 roll flag, 836D1C..836D23 digits, 836D28 continue stage
    // 8369B4 scene record, 8369B8 frames, 8369BC, 8369C0, 8369C8.. {init, ctrl, exit}
    // table (never written; kinds index it up to 836CE0, .bss nothing references).
    std::array<std::uint8_t,0x32c> goal_8369b4{};
    std::array<std::uint8_t,0x0c> handles_67f638{{0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff}};   // 67F638/63C/640 (.data -1)
    static constexpr std::uint32_t MilesBase=0x84b900u,MilesEnd=0x84bd00u;
    std::array<std::uint8_t,MilesEnd-MilesBase> miles{};  // 4EF280 block (84BD00 = the HUD's 4EF410 float)
    std::array<std::uint8_t,0x2030> camera_651758{};     // .data goal camera scripts (4871A0 sets +28)
    std::array<std::uint8_t,0x18> network_7d68bc{};      // 7D68BC session / 7D68D2 (offline: 0)
    std::array<std::uint8_t,4> frames_836ce4{};          // 499A30 START display frames (START 49DE3C sets 0)
    std::array<std::uint8_t,2> players_8367f0{};         // word: 456D60 players (START 49DD01, 499A30)
    // The arcade goal ending (race_ending.cpp): 7D3A74..7D3A8F (state, 7D3A78, 7D3A7C motion
    // set, 7D3A80 car, 7D3A84, 7D3A88), its .data 638DF0..638F6F (EXE initial values), the
    // mode 24 wait 836848 and the loading animation handles 8367A0 / 8367A4.
    std::array<std::uint8_t,0x1c> ending_7d3a74{};
    std::array<std::uint8_t,0x180> ending_638df0{};
    std::array<std::uint8_t,4> ending_836848{};
    std::array<std::uint8_t,8> ending_8367a0{{0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff}};
    std::array<std::uint8_t,4> stage_67ee50{{0xff,0xff,0xff,0xff}};   // 67EE50 mode 36 stage (67EE4C, the reward, is GAD_PUB data)
    std::array<std::uint8_t,0x14> reward_8366ac{};                                          // 8366AC delay, 8366B0..BC sprite handles
    std::array<std::uint8_t,8> board_68697c{{0x0f,0,0,0,0xf1,0xff,0xff,0xff}};   // .data 68697C / 686980: the letter board cursor (4B0F10, 0xF / -0xF)
    PcRaceEndState();
    void map(PcRaceMemory& m){
        m.map(0x836ce0u,bytes_836ce0.data(),2);m.map(0x836d08u,words_836d08.data(),16);m.map(0x8367acu,word_8367ac.data(),4);
        m.map(RouteBase,route.data(),route.size());m.map(0x687774u,common_687774.data(),8);m.map(0x842888u,common_842888.data(),4);m.map(0x842820u,arcade_842820.data(),arcade_842820.size());m.map(NameBase,name_842288.data(),name_842288.size());m.map(0x84288cu,arcade_84288c.data(),arcade_84288c.size());
        m.map(0x780440u,work_780440.data(),work_780440.size());m.map(0x844928u,arcade_844928.data(),arcade_844928.size());
        m.map(0x84a208u,arcade_84a208.data(),arcade_84a208.size());m.map(0x6898c0u,arcade_6898c0.data(),arcade_6898c0.size());
        m.map(0x65a7a0u,arcade_65a7a0.data(),arcade_65a7a0.size());
        m.map(0x830368u,attract_830360.data()+8,4);m.map(0x830380u,attract_830360.data()+0x20-4,4);
        m.map(BookBase,bookkeeping.data(),bookkeeping.size());m.map(SpTablesBase,sp_tables.data(),sp_tables.size());m.map(0x7d6720u,stats_7d6720.data(),stats_7d6720.size());
        m.map(0x7d6768u,stats_7d6768.data(),stats_7d6768.size());m.map(0x7f1958u,adv_7f1958.data(),adv_7f1958.size());m.map(0x83dae0u,boot_83dae0.data(),boot_83dae0.size());
        m.map(0x67ee3cu,flag_67ee3c.data(),4);m.map(0x65994cu,flag_65994c.data(),4);
        m.map(0x836d18u,words_836d18.data(),words_836d18.size());m.map(0x67f638u,handles_67f638.data(),handles_67f638.size());
        m.map(MilesBase,miles.data(),miles.size());m.map(0x8369b4u,goal_8369b4.data(),goal_8369b4.size());
        m.map(0x651758u,camera_651758.data(),camera_651758.size());m.map(0x7d68bcu,network_7d68bc.data(),network_7d68bc.size());
        m.map(0x7d3a74u,ending_7d3a74.data(),ending_7d3a74.size());m.map(0x638df0u,ending_638df0.data(),ending_638df0.size());
        m.map(0x836848u,ending_836848.data(),4);m.map(0x8367a0u,ending_8367a0.data(),8);
        m.map(0x836ce4u,frames_836ce4.data(),4);m.map(0x8367f0u,players_8367f0.data(),2);
        m.map(0x67ee50u,stage_67ee50.data(),4);m.map(0x8366acu,reward_8366ac.data(),reward_8366ac.size());
        m.map(0x68697cu,board_68697c.data(),board_68697c.size());
    }
};
// EXE .rdata 5C4740..5C4C00 (route tree 5C4748, points 5C4830, tokens
// 5C48B0/5C4B08, course kinds 5C4B80, 5C4BF8/5C4BFC) and 5A7AB4 (453370
// route keys, 20 words).
void race_end_map_tables(PcRaceMemory&);
// Mode callbacks.
void mode20_init_49a1e0(PcRaceContext&);
void mode20_control_49a280(PcRaceContext&);
void mode20_exit_49d1e0(PcRaceContext&);
void mode21_init_49a2e0(PcRaceContext&);
void mode21_control_49d210(PcRaceContext&);
void mode22_init_49a370(PcRaceContext&);
void mode22_control_49a400(PcRaceContext&);
void race_close_49a4f0(PcRaceContext&);
// Mode 23: 49A450 init (music 0x1C after 424AE0) / 49A480 control (accelerate or
// brake shortens the wait, then mode 0x19) / 49A4F0 exit.
// 449AC0(&hours, &minutes, &seconds, &ms, time): the time split into words.
void race_time_words_449ac0(PcRaceMemory&,std::uint32_t hours,std::uint32_t minutes,std::uint32_t seconds,std::uint32_t ms,std::uint32_t time);
void mode23_init_49a450(PcRaceContext&);
void mode23_control_49a480(PcRaceContext&);
// Mode 29 (the pause menu's quit, 43F990(0x1D)): 49B130 requests mode 0x1C; 49B140 exit
// stops the music, closes the race events (49AD00), the camera and the area loaders.
void mode29_control_49b130(PcRaceContext&);
void mode29_exit_49b140(PcRaceContext&);
// Mode 31: 49AB90 init (banks 0x3D/0x12/0x14/0x15/0x36/0x37, common animation) /
// 49AC00 control (events 0x18B/0x17F, then 4B0960 and mode 0x1B) / 49AC80 exit.
void mode31_init_49ab90(PcRaceContext&);
void mode31_control_49ac00(PcRaceContext&);
void mode31_exit_49ac80(PcRaceContext&);
void mode25_init_49a790(PcRaceContext&);
void mode25_control_49a800(PcRaceContext&);
void mode25_exit_49a920(PcRaceContext&);
void mode27_init_49adc0(PcRaceContext&);
void mode27_control_49aea0(PcRaceContext&);
void mode27_exit_49aed0(PcRaceContext&);
void mode28_init_49b170(PcRaceContext&);
void mode28_control_49af90(PcRaceContext&);
void race_events_close_49ad00(PcRaceContext&);
// Event 0x188 functions.
void route_init_4ae750(PcRaceContext&);
void route_control_4ae960(PcRaceContext&);
void route_destroy_4aed20(PcRaceContext&);
void ta_result_init_4aed50(PcRaceContext&);
void ta_result_control_4aeec0(PcRaceContext&);
void ta_result_destroy_4aeee0(PcRaceContext&);
// SUMO_FE (console, [780260] = 1) flow:
//   34 MILES     49B470 / 49B570 / 49B7C0 (OutRun Miles 4EF550 / 4EF510)
//   35 CONTINUE  49B7E0 / 49D9C0 / 49DAE0 (retry -> 30, exit -> 28)
//   30 RETRY     49B170 / 49B190 (-> START 13)
void mode34_init_49b470(PcRaceContext&);
void mode34_control_49b570(PcRaceContext&);
void mode34_exit_49b7c0(PcRaceContext&);
void mode35_init_49b7e0(PcRaceContext&);
void mode35_control_49d9c0(PcRaceContext&);
void mode35_exit_49dae0(PcRaceContext&);
// mode 36 (C2C reward screen; its text display 498010 is in race_end_runtime.cpp)
void mode36_init_4981a0(PcRaceContext&);
void mode36_control_4981f0(PcRaceContext&);
void mode36_exit_4983d0(PcRaceContext&);
// 5C1E58 reward record r (18 words), nullptr outside 0..8.
const std::uint32_t* mode36_reward_record(std::int32_t r);
void mode30_control_49b190(PcRaceContext&);
// OUTRUN2SP (arcade, [780260] = 0) route: 7 (48AE40 / 49F360 / 49F370) -> 9 (48AE40 / 48AE50)
// -> 10 attract.
void mode7_init_48ae40(PcRaceContext&);
void mode7_control_49f360(PcRaceContext&);
void mode7_exit_49f370(PcRaceContext&);
void mode9_control_48ae50(PcRaceContext&);
void camera_reset_487240(PcRaceContext&);
void arcade_reset_4b6f80(PcRaceContext&);
// 10 ATTRACT 48B210 / 48AE90 / 48B070, 11 48B0B0, 12 48B0F0 / 48B120 / 48B2A0.
void mode10_init_48b210(PcRaceContext&);
void mode10_control_48ae90(PcRaceContext&);
void mode10_exit_48b070(PcRaceContext&);
void mode11_control_48b0b0(PcRaceContext&);
void mode12_init_48b0f0(PcRaceContext&);
void mode12_control_48b120(PcRaceContext&);
void mode12_exit_48b2a0(PcRaceContext&);
void race_close_49cdf0(PcRaceContext&);          // after a goal (mode 19/35 exit)
void model_release_499bb0(PcRaceContext&);       // 8367C0 flags -> 448990(5C2258 resource)
// OutRun Miles module (.bss 84B900..84BD00; 84BD00 float = 4EF410's).
// 4EF280 loads \Scripts\bin\OutrunMiles through the services 4F12A0
// (name, 84BCB0, 0, 0), 4F1A90(name, 84BCB0) -> records, 4F1210(84BCB0,
// records) -> count and 580C38 (delete of [84BCB4]).
void miles_init_4ef280(PcRaceContext&);
void miles_compute_4ef550(PcRaceContext&,std::uint32_t variant);
void miles_commit_4ef510(PcRaceContext&);
// Goal: 19 GOAL 49C9C0 / 49CC50 (+ goal scene 49B2E0) / 49CFD0, 33 ranking
// upload 49D4A0 / 49D850 (offline: straight on).
void mode19_init_49c9c0(PcRaceContext&);
void mode19_control_49cc50(PcRaceContext&);
void goal_scene_49b2e0(PcRaceContext&);
void goal_camera_4871a0(PcRaceContext&,std::uint32_t scene);
void bookkeeping_452dd0(PcRaceMemory&,std::uint32_t variant,std::uint32_t preset);
void bookkeeping_452e60(PcRaceMemory&,std::uint32_t variant,std::uint32_t preset,std::uint32_t time);
void mode19_exit_49cfd0(PcRaceContext&);
std::uint32_t goal_route_4b1670(PcRaceContext&);
std::uint32_t goal_index_4b1680(std::uint32_t route,std::uint32_t level);
void license_bit_499960(PcRaceMemory&,std::uint32_t license,std::int32_t bit);
void mode33_init_49d4a0(PcRaceContext&);
void mode33_control_49d850(PcRaceContext&);
// GAD_PUB display 4998C0 (event 389) for the end modes: dispatch by 78026C to
// 499A30 / 49A060 / 498670 GOAL / 499020 TIME OVER / 497900 (services), 497960
// GAME OVER, 48C5F0 (mode 33 window), 497FA0 Miles digits. The GAD globals
// 836630..836718 and 67EE38..67EE50 are mapped by the caller.
void gad_display_4998c0(PcRaceContext&);
void gad_game_over_497960(PcRaceContext&);
void gad_start_499a30(PcRaceContext&);
void gad_goal_498670(PcRaceContext&);
void gad_time_over_499020(PcRaceContext&);
void miles_digits_497fa0(PcRaceContext&);
// Shared helpers (also PC functions).
void common_anim_4b72f0(PcRaceContext&,std::uint32_t kind);
void common_anim_release_4b7630(PcRaceContext&);
std::uint32_t result_input_4bfb20(PcRaceContext&);
void adv_release_45af40(PcRaceContext&);        // 45AEF0 (VM entry = mov eax,[7F1964]) is the same body
void bookkeeping_453370(PcRaceContext&);
// Raised on a PC path whose value the original reads from an uninitialised
// stack slot (4AE960 default case).
struct PcRaceEndUndefined { std::uint32_t pc; };
// Raised on a PC path that no PC code reaches (the name entry's letter input).
struct PcRaceEndUnreachable { std::uint32_t pc; };
}

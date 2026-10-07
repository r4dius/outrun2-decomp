// Diagnostic only, Linux x86-64. Input is verified by run_pc_oracle.py.
// Never boot the game. Only invoke the explicitly listed leaf/closed routines.
#include "driving/pc_driving.hpp"
#include "driving/pc_wheel_dynamics.hpp"
#include "driving/pc_steering.hpp"
#include "driving/pc_tire_geometry.hpp"
#include "driving/pc_transmission.hpp"
#include "driving/pc_road_mu.hpp"
#include "driving/pc_driving_control.hpp"
#include "driving/pc_suspension.hpp"
#include "driving/pc_contact.hpp"
#include "driving/pc_collision.hpp"
#include "driving/pc_wall_controller.hpp"
#include "driving/pc_body_wall.hpp"
#include "driving/pc_coli_car.hpp"
#include "driving/pc_chassis.hpp"
#include "driving/pc_common_control.hpp"
#include "driving/pc_car_services.hpp"
#include "driving/pc_car_services_ghost.hpp"
#include "driving/pc_car_services_net.hpp"
#include "platform/frontend_welcome.hpp"
#include "platform/frontend_license_editor.hpp"
#include "platform/frontend_sprites.hpp"
#include "platform/frontend_text.hpp"
#include "platform/frontend_glyph_render.hpp"
#include "platform/frontend_images.hpp"
#include "platform/frontend_license_widgets.hpp"
#include "platform/frontend_list.hpp"
#include "platform/frontend_ui_resources.hpp"
#include "platform/frontend_fixed_choice.hpp"
#include "platform/frontend_categories.hpp"
#include "platform/frontend_missions.hpp"
#include "platform/frontend_rankings.hpp"
#include "platform/frontend_mission_tables.hpp"
#include "platform/frontend_record_manager.hpp"
#include "platform/frontend_vehicle_loader.hpp"
#include "platform/frontend_vehicle_data.hpp"
#include "platform/frontend_vehicle_menu.hpp"
#include "platform/frontend_vehicle_preview.hpp"
#include "platform/vehicle_constructor.hpp"
#include "platform/vehicle_body_init.hpp"
#include "platform/vehicle_preview_event.hpp"
#include "driving/pc_environment_blend.hpp"
#include "platform/vehicle_preview_init.hpp"
#include "driving/pc_d3dx.hpp"
#include "driving/pc_camera.hpp"
#include "driving/pc_x87.hpp"
#include "driving/pc_race_camera.hpp"
#include "platform/pc_scene_display.hpp"
#include "platform/pc_vehicle_display.hpp"
#include "platform/pc_frame_render.hpp"
#include "platform/pc_car_reflection.hpp"
#include "platform/pc_scene_environment.hpp"
#include "platform/frontend_transmission.hpp"
#include "platform/frontend_music.hpp"
#include "platform/mission_manager.hpp"
#include "platform/race_hud.hpp"
#include "platform/race_hud_navi.hpp"
#include "platform/race_events.hpp"
#include "platform/race_manager.hpp"
#include "platform/race_sound.hpp"                  // race SOUND (event 383) / COMM_TRANS (360)
#include "platform/race_input.hpp"
#include "platform/embedded_exe_data.hpp"
#include "platform/course_asset_pack.hpp"
#include "platform/racer_setup.hpp"
#include "platform/scene_owner_49ba80.hpp"
#include "platform/pc_d3d9_state.hpp"
#include "platform/embedded_camera_data.hpp"
#include "platform/vehicle_model_draw.hpp"
#include "platform/pc_render_queue.hpp"
#include "platform/pc_pmt_loader.hpp"
#include "platform/pc_render_flush.hpp"
#include "platform/embedded_vehicle_draw_data.hpp"
#include <functional>
#include <sstream>
#include "platform/frontend_vehicle_data.hpp"
#include <fstream>
#include <zlib.h>
#include <map>
#include "driving/pc_course_spline.hpp"
#include "driving/pc_course_topology.hpp"
#include "platform/title_owner.hpp"
#include "platform/frontend_title_widgets.hpp"
#include "platform/frontend_profiles.hpp"
#include "../../tests/support/course_query_fixture.hpp"
#include "../../tests/support/course_world_fixture.hpp"
#include "../../tests/support/ground_fixture.hpp"
#include "../../tests/support/wall_geometry_fixture.hpp"
#include "../../tests/support/wall_response_fixture.hpp"
#include "../../tests/support/wall_rebound_fixture.hpp"
#include "../../tests/support/crash_fixture.hpp"
#include "../../tests/support/crash_entry_fixture.hpp"
#include <sys/mman.h>
#include <malloc.h>
#include <sys/resource.h>
#include <signal.h>
#include <ucontext.h>
#include <unistd.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <random>
#include <sstream>
#include <string>
#include <vector>
using namespace outrun::driving;
extern "C" {
struct Call {std::uint32_t entry,sp,eax,ecx,edx,ebx,esi,edi,ebp,xmm0,out_eax,out_xmm0,out_st0,out_sp,st0,mxcsr,x87_cw,out_ecx;std::uint8_t out_st0_raw[12];};
Call guest_call{};
void run_original32();
void course_query_stub32();
extern std::uint32_t course_stub_index,course_stub_overflow;
extern std::uint32_t course_stub_y[4],course_stub_collision[4],course_stub_flags[4],course_stub_ret[4];
extern std::uint32_t course_stub_seen_x[4],course_stub_seen_y[4],course_stub_seen_z[4];
void pas_transform_stub32();
void pas_road_stub32();
void pas_tire_stub32();
void pas_normal_stub32();
extern std::uint32_t pas_stub_index,pas_stub_overflow;
extern std::uint32_t pas_transform_xyz[12],pas_road_y[4],pas_road_polygon[4],pas_road_ret[4],pas_tire_xyz[12],pas_normal_xyz[12];
extern std::uint32_t pas_seen_transform_x[4],pas_seen_transform_y[4],pas_seen_transform_z[4];
extern std::uint32_t pas_seen_road_x[4],pas_seen_road_y[4],pas_seen_road_z[4];
extern std::uint32_t pas_seen_tire_index[4],pas_seen_normal_polygon[4],pas_seen_normal_type[4];
}
static_assert(sizeof(Call)==84);
namespace {
constexpr std::uint32_t E=0x2f001000,W=0x2f003000,P=0x2f005000,S=0x3101f000;
using Event=std::array<std::uint8_t,event_size>;
using Work=std::array<std::uint8_t,work_size>;
using Params=std::array<std::uint8_t,parameter_size>;
std::uint32_t fbits(float x){std::uint32_t u;std::memcpy(&u,&x,4);return u;}
void* map_at(std::uint32_t address,std::size_t size,int prot){
    void* p=mmap(reinterpret_cast<void*>(std::uintptr_t(address)),size,prot,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
    if(p==MAP_FAILED)throw std::runtime_error("mmap failed at "+std::to_string(address)+": "+std::to_string(errno));
    return p;
}
bool g_steam_exe=false;   // the unpacked Steam build (5,828,096 bytes) instead of the FXT EXE
std::vector<std::uint8_t> load_file(const char* path){
    std::ifstream f(path,std::ios::binary);if(!f)throw std::runtime_error("cannot open PE");
    f.seekg(0,std::ios::end);auto n=f.tellg();
    if(n!=14950400&&n!=5828096)throw std::runtime_error("wrong PE length; use the unpacked Steam build or the pinned FXT EXE");
    g_steam_exe=n==5828096;
    f.seekg(0);std::vector<std::uint8_t>b(static_cast<std::size_t>(n));
    if(!f.read(reinterpret_cast<char*>(b.data()),n))throw std::runtime_error("short PE read");
    return b;
}
// The unpacked Steam build (the base) has plain code where the FXT EXE jumps into
// the protection. A gate patch checks the FXT bytes; on Steam it checks and later
// restores the plain bytes the gate replaced (saved at the first patch).
void r060_raw_write(std::uint32_t a,const std::uint8_t* p,std::size_t n);
const std::uint8_t* gate_original(std::uint32_t addr,const std::uint8_t* fxt,std::size_t n){
    if(!g_steam_exe)return fxt;
    static std::map<std::uint32_t,std::vector<std::uint8_t>> saved;
    auto it=saved.find(addr);
    if(it==saved.end()){const auto* q=reinterpret_cast<const std::uint8_t*>(std::uintptr_t(addr));it=saved.emplace(addr,std::vector<std::uint8_t>(q,q+n)).first;}
    return it->second.data();
}
void map_pe(std::vector<std::uint8_t>& data){
    Bytes f(data.data(),data.size());auto pe=f.u32(0x3c),opt=pe+24;
    if(f.u32(pe)!=0x4550||f.i16(pe+4)!=0x14c||f.u32(opt+28)!=0x400000)throw std::runtime_error("unsupported PE");
    auto section=opt+static_cast<std::uint16_t>(f.i16(pe+20));
    const auto count=static_cast<std::uint16_t>(f.i16(pe+6));
    // Map only .text/.rdata/.data. Packed/obfuscated sections are intentionally excluded.
    for(unsigned i=0;i<count;++i,section+=40){
        char name[9]{};for(unsigned k=0;k<8;++k)name[k]=char(f.u8(section+k));
        if(std::string(name)!=".text"&&std::string(name)!=".rdata"&&std::string(name)!=".data")continue;
        const auto va=f.u32(section+12)+0x400000,raw=f.u32(section+16),off=f.u32(section+20);
        const auto size=(std::max(f.u32(section+8),raw)+4095u)&~4095u;
        f.check(off,raw);void* dst=map_at(va,size,PROT_READ|PROT_WRITE);std::memcpy(dst,data.data()+off,raw);
        int prot=PROT_READ;const auto flags=f.u32(section+36);
        if(flags&0x20000000)prot|=PROT_EXEC;
        // Arithmetic tests must not modify any static game table or global.
        if(mprotect(dst,size,prot))throw std::runtime_error("mprotect failed");
    }
    map_at(0x2f000000,0x10000,PROT_READ|PROT_WRITE);
    map_at(0x31001000,0x20000,PROT_READ|PROT_WRITE); // unmapped guard regions either side
    // Controlled mutable input page used only by CalcToeAngle -> GetVolume(1).
    // The original function reads channel state here; code pages remain unmodified.
    if(mprotect(reinterpret_cast<void*>(0x7d6000),0x1000,PROT_READ|PROT_WRITE))
        throw std::runtime_error("mprotect input page failed");
}

void patch_get_y_position_spl_query_calls(bool restore=false){
    static bool patched=false;
    if(patched==!restore)return;
    const auto stub=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&course_query_stub32));
    const auto target=restore?0x0043eb60u:stub;
    for(const std::uint32_t address:{0x00518e44u,0x00518e99u,0x00518eeeu,0x00518f53u}){
        auto p=reinterpret_cast<std::uint8_t*>(address);
        std::int32_t displacement;std::memcpy(&displacement,p+1,4);
        if(p[0]!=0xe8||address+5u+static_cast<std::uint32_t>(displacement)!=(restore?stub:0x0043eb60u))
            throw std::runtime_error("course wrapper CALL target changed");
        auto page=reinterpret_cast<void*>(address&~0xfffu);
        if(mprotect(page,4096,PROT_READ|PROT_WRITE|PROT_EXEC))throw std::runtime_error("course wrapper patch protection");
        const std::uint32_t relative=target-(address+5u);std::memcpy(p+1,&relative,4);
        if(mprotect(page,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("course wrapper patch restore protection");
    }
    patched=!restore;
}

constexpr std::uint32_t BkStubBlock=0x32006000u;
constexpr std::uint32_t BkStateBase=0x33012000u;
void map_bk_selector_stubs(){
    auto* code=static_cast<std::uint8_t*>(map_at(BkStubBlock,4096,PROT_READ|PROT_WRITE));
    map_at(BkStateBase,4096,PROT_READ|PROT_WRITE);
    auto make_stub=[&](std::size_t off,std::uint32_t cell){
        std::uint8_t bytes[]={0xa1,0,0,0,0,0xc3}; // mov eax,[abs]; ret
        std::memcpy(bytes+1,&cell,4);std::memcpy(code+off,bytes,sizeof(bytes));
        return BkStubBlock+std::uint32_t(off);
    };
    const auto branch=make_stub(0x000,BkStateBase+0x00);
    const auto gate0 =make_stub(0x010,BkStateBase+0x04);
    const auto gate1 =make_stub(0x020,BkStateBase+0x08);
    const auto gate2 =make_stub(0x030,BkStateBase+0x0c);
    auto patch_call=[&](std::uint32_t va,std::uint32_t expected,std::uint32_t target){
        auto* p=reinterpret_cast<std::uint8_t*>(va);std::int32_t rel{};std::memcpy(&rel,p+1,4);
        if(p[0]!=0xe8||std::uint32_t(std::int64_t(va+5u)+rel)!=expected)
            throw std::runtime_error("GetYPositionProg_BK selector CALL target changed");
        auto* page=reinterpret_cast<void*>(va&~0xfffu);
        if(mprotect(page,4096,PROT_READ|PROT_WRITE|PROT_EXEC))throw std::runtime_error("BK selector patch protection");
        const auto newrel=target-(va+5u);std::memcpy(p+1,&newrel,4);
        if(mprotect(page,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("BK selector patch restore protection");
    };
    patch_call(0x43eeedu,0x450380u,branch);patch_call(0x43eef3u,0x44c940u,branch);patch_call(0x43eef9u,0x451350u,branch);
    patch_call(0x43f115u,0x450380u,branch);patch_call(0x43f11bu,0x44c940u,branch);patch_call(0x43f121u,0x451350u,branch);   // 43F110
    patch_call(0x43ef31u,0x4957f0u,gate0);patch_call(0x43ef3au,0x48b310u,gate1);patch_call(0x43ef43u,0x495490u,gate2);
    if(mprotect(code,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("BK selector stub mprotect");
}
void set_bk_selector_inputs(std::uint32_t branch,bool gate0,bool gate1,bool gate2){
    *reinterpret_cast<std::uint32_t*>(BkStateBase+0x00)=branch;
    *reinterpret_cast<std::uint32_t*>(BkStateBase+0x04)=gate0?1u:0u;
    *reinterpret_cast<std::uint32_t*>(BkStateBase+0x08)=gate1?1u:0u;
    *reinterpret_cast<std::uint32_t*>(BkStateBase+0x0c)=gate2?1u:0u;
}
constexpr std::uint32_t RearGripStubBlock=0x32007000u;
constexpr std::uint32_t RearGripStateBase=0x33013000u;
void map_rear_grip_volume_stubs(){
    auto* code=static_cast<std::uint8_t*>(map_at(RearGripStubBlock,4096,PROT_READ|PROT_WRITE));
    map_at(RearGripStateBase,4096,PROT_READ|PROT_WRITE);
    auto make_stub=[&](std::size_t off,std::uint32_t cell1,std::uint32_t cell2){
        std::uint8_t bytes[]={0x8b,0x44,0x24,0x04,0x83,0xf8,0x01,0x75,0x06,0xa1,0,0,0,0,0xc3,0xa1,0,0,0,0,0xc3};
        std::memcpy(bytes+10,&cell1,4);std::memcpy(bytes+16,&cell2,4);std::memcpy(code+off,bytes,sizeof(bytes));
        return RearGripStubBlock+std::uint32_t(off);
    };
    const auto current=make_stub(0x000,RearGripStateBase+0x00,RearGripStateBase+0x04);
    const auto old=make_stub(0x040,RearGripStateBase+0x08,RearGripStateBase+0x0c);
    auto patch_call=[&](std::uint32_t va,std::uint32_t expected,std::uint32_t target){
        auto* p=reinterpret_cast<std::uint8_t*>(va);std::int32_t rel{};std::memcpy(&rel,p+1,4);
        if(p[0]!=0xe8||std::uint32_t(std::int64_t(va+5u)+rel)!=expected)throw std::runtime_error("RearGripCtrl volume CALL target changed");
        auto* page=reinterpret_cast<void*>(va&~0xfffu);if(mprotect(page,4096,PROT_READ|PROT_WRITE|PROT_EXEC))throw std::runtime_error("RearGripCtrl patch protection");
        const auto newrel=target-(va+5u);std::memcpy(p+1,&newrel,4);if(mprotect(page,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("RearGripCtrl patch restore protection");
    };
    patch_call(0x4a2fa5u,0x453720u,current);patch_call(0x4a2faeu,0x453720u,current);
    patch_call(0x4a2fb7u,0x453750u,old);patch_call(0x4a2fc0u,0x453750u,old);
    if(mprotect(code,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("RearGripCtrl stub mprotect");
}
void set_rear_grip_inputs(const RearGripInputs& v){
    *reinterpret_cast<std::int32_t*>(RearGripStateBase+0x00)=v.volume1;*reinterpret_cast<std::int32_t*>(RearGripStateBase+0x04)=v.volume2;
    *reinterpret_cast<std::int32_t*>(RearGripStateBase+0x08)=v.old_volume1;*reinterpret_cast<std::int32_t*>(RearGripStateBase+0x0c)=v.old_volume2;
}
constexpr std::uint32_t NightTunnelStubBlock=0x32008000u;
constexpr std::uint32_t NightTunnelStateBase=0x33014000u;
void map_night_tunnel_stubs(){
    auto* code=static_cast<std::uint8_t*>(map_at(NightTunnelStubBlock,4096,PROT_READ|PROT_WRITE));
    map_at(NightTunnelStateBase,4096,PROT_READ|PROT_WRITE);
    auto make_stub=[&](std::size_t off,std::uint32_t cell){
        std::uint8_t bytes[]={0xa1,0,0,0,0,0xc3};
        std::memcpy(bytes+1,&cell,4);std::memcpy(code+off,bytes,sizeof(bytes));return NightTunnelStubBlock+std::uint32_t(off);
    };
    const auto night=make_stub(0x000,NightTunnelStateBase+0x00);
    const auto tunnel=make_stub(0x010,NightTunnelStateBase+0x04);
    const auto course=make_stub(0x020,NightTunnelStateBase+0x08);
    auto patch_call=[&](std::uint32_t va,std::uint32_t expected,std::uint32_t target){
        auto* q=reinterpret_cast<std::uint8_t*>(va);std::int32_t rel{};std::memcpy(&rel,q+1,4);
        if(q[0]!=0xe8||std::uint32_t(std::int64_t(va+5u)+rel)!=expected)throw std::runtime_error("CheckNightAndTunnel CALL target changed");
        auto* page=reinterpret_cast<void*>(va&~0xfffu);if(mprotect(page,4096,PROT_READ|PROT_WRITE|PROT_EXEC))throw std::runtime_error("night/tunnel patch protection");
        const auto newrel=target-(va+5u);std::memcpy(q+1,&newrel,4);if(mprotect(page,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("night/tunnel patch restore protection");
    };
    patch_call(0x4a25f1u,0x4afb60u,night);patch_call(0x4a2611u,0x4afb90u,tunnel);patch_call(0x4a262au,0x44bdb0u,course);
    if(mprotect(code,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("night/tunnel stub mprotect");
}
void set_night_tunnel_inputs(const PcNightTunnelInputs& v){
    *reinterpret_cast<std::uint32_t*>(NightTunnelStateBase+0x00)=v.night?1u:0u;
    *reinterpret_cast<std::uint32_t*>(NightTunnelStateBase+0x04)=v.tunnel?1u:0u;
    *reinterpret_cast<std::uint32_t*>(NightTunnelStateBase+0x08)=v.course_flags;
}
constexpr std::uint32_t DrivingSkillStubBlock=0x32009000u;
constexpr std::uint32_t DrivingSkillStateBase=0x33015000u;
void map_driving_skill_stubs(){
    auto* code=static_cast<std::uint8_t*>(map_at(DrivingSkillStubBlock,4096,PROT_READ|PROT_WRITE));
    map_at(DrivingSkillStateBase,4096,PROT_READ|PROT_WRITE);
    auto make_stub=[&](std::size_t off,std::uint32_t cell){
        std::uint8_t bytes[]={0xa1,0,0,0,0,0xc3}; // mov eax,[abs]; ret
        std::memcpy(bytes+1,&cell,4);std::memcpy(code+off,bytes,sizeof(bytes));return DrivingSkillStubBlock+std::uint32_t(off);
    };
    const auto stage=make_stub(0x000,DrivingSkillStateBase+0x00);
    const auto nodes=make_stub(0x010,DrivingSkillStateBase+0x04);
    auto patch_call=[&](std::uint32_t va,std::uint32_t expected,std::uint32_t target){
        auto* q=reinterpret_cast<std::uint8_t*>(va);std::int32_t rel{};std::memcpy(&rel,q+1,4);
        if(q[0]!=0xe8||std::uint32_t(std::int64_t(va+5u)+rel)!=expected)throw std::runtime_error("CheckDrivingSkill CALL target changed");
        auto* page=reinterpret_cast<void*>(va&~0xfffu);if(mprotect(page,4096,PROT_READ|PROT_WRITE|PROT_EXEC))throw std::runtime_error("driving-skill patch protection");
        const auto newrel=target-(va+5u);std::memcpy(q+1,&newrel,4);if(mprotect(page,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("driving-skill patch restore protection");
    };
    patch_call(0x4a484fu,0x44c940u,stage);
    patch_call(0x4a48a5u,0x456d60u,nodes);
    if(mprotect(code,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("driving-skill stub mprotect");
}
void set_driving_skill_inputs(const PcDrivingSkillInputs& v){
    *reinterpret_cast<std::int32_t*>(DrivingSkillStateBase+0x00)=v.stage_level;
    *reinterpret_cast<std::uint32_t*>(DrivingSkillStateBase+0x04)=v.entry_nodes;
}
constexpr std::uint32_t ChickenDriverStubBlock=0x3200a000u;
constexpr std::uint32_t ChickenDriverStateBase=0x33016000u;
void map_chicken_driver_stubs(){
    auto* code=static_cast<std::uint8_t*>(map_at(ChickenDriverStubBlock,4096,PROT_READ|PROT_WRITE));
    map_at(ChickenDriverStateBase,4096,PROT_READ|PROT_WRITE);
    auto make_stub=[&](std::size_t off,std::uint32_t cell){
        std::uint8_t bytes[]={0xa1,0,0,0,0,0xc3}; // mov eax,[abs]; ret
        std::memcpy(bytes+1,&cell,4);std::memcpy(code+off,bytes,sizeof(bytes));return ChickenDriverStubBlock+std::uint32_t(off);
    };
    const auto maxcs=make_stub(0x000,ChickenDriverStateBase+0x00);
    const auto qs0=make_stub(0x010,ChickenDriverStateBase+0x04);
    const auto qs1=make_stub(0x020,ChickenDriverStateBase+0x08);
    const auto qs2=make_stub(0x030,ChickenDriverStateBase+0x0c);
    const auto dir0=make_stub(0x040,ChickenDriverStateBase+0x10);
    const auto dir1=make_stub(0x050,ChickenDriverStateBase+0x14);
    const auto dir2=make_stub(0x060,ChickenDriverStateBase+0x18);
    const auto volume=make_stub(0x070,ChickenDriverStateBase+0x1c);
    auto patch_call=[&](std::uint32_t va,std::uint32_t expected,std::uint32_t target){
        auto* q=reinterpret_cast<std::uint8_t*>(va);std::int32_t rel{};std::memcpy(&rel,q+1,4);
        if(q[0]!=0xe8||std::uint32_t(std::int64_t(va+5u)+rel)!=expected)throw std::runtime_error("CheckChickenDriver CALL target changed");
        auto* page=reinterpret_cast<void*>(va&~0xfffu);if(mprotect(page,4096,PROT_READ|PROT_WRITE|PROT_EXEC))throw std::runtime_error("chicken-driver patch protection");
        const auto newrel=target-(va+5u);std::memcpy(q+1,&newrel,4);if(mprotect(page,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("chicken-driver patch restore protection");
    };
    patch_call(0x4a495cu,0x43d470u,maxcs);
    patch_call(0x4a4991u,0x43d920u,qs0);patch_call(0x4a49b7u,0x43d920u,qs1);patch_call(0x4a49ddu,0x43d920u,qs2);
    patch_call(0x4a49f7u,0x43d340u,dir0);patch_call(0x4a4a06u,0x43d340u,dir1);patch_call(0x4a4a1au,0x43d340u,dir2);
    patch_call(0x4a4b0bu,0x453720u,volume);
    if(mprotect(code,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("chicken-driver stub mprotect");
}
void set_chicken_driver_inputs(const PcChickenDriverInputs& v){
    *reinterpret_cast<std::int32_t*>(ChickenDriverStateBase+0x00)=0x7fff;
    for(unsigned k=0;k<3;++k){
        *reinterpret_cast<std::int32_t*>(ChickenDriverStateBase+0x04+k*4u)=v.query_status[k];
        *reinterpret_cast<std::int32_t*>(ChickenDriverStateBase+0x10+k*4u)=v.offset_direction[k];
    }
    *reinterpret_cast<std::int32_t*>(ChickenDriverStateBase+0x1c)=v.volume1;
}
constexpr std::uint32_t AssistChickenStubBlock=0x3200c000u;
constexpr std::uint32_t AssistChickenStateBase=0x33018000u;
void map_assist_chicken_stubs(){
    auto* code=static_cast<std::uint8_t*>(map_at(AssistChickenStubBlock,4096,PROT_READ|PROT_WRITE));
    map_at(AssistChickenStateBase,4096,PROT_READ|PROT_WRITE);
    auto make_stub=[&](std::size_t off,std::uint32_t cell){
        std::uint8_t bytes[]={0xa1,0,0,0,0,0xc3};
        std::memcpy(bytes+1,&cell,4);std::memcpy(code+off,bytes,sizeof(bytes));return AssistChickenStubBlock+std::uint32_t(off);
    };
    const auto nodes=make_stub(0x000,AssistChickenStateBase+0x00);
    const auto vol1=make_stub(0x010,AssistChickenStateBase+0x04);
    const auto vol2=make_stub(0x020,AssistChickenStateBase+0x08);
    auto patch_call=[&](std::uint32_t va,std::uint32_t expected,std::uint32_t target){
        auto* q=reinterpret_cast<std::uint8_t*>(va);std::int32_t rel{};std::memcpy(&rel,q+1,4);
        if(q[0]!=0xe8||std::uint32_t(std::int64_t(va+5u)+rel)!=expected)throw std::runtime_error("AssistChickenDriver CALL target changed");
        auto* page=reinterpret_cast<void*>(va&~0xfffu);if(mprotect(page,4096,PROT_READ|PROT_WRITE|PROT_EXEC))throw std::runtime_error("assist-chicken patch protection");
        const auto newrel=target-(va+5u);std::memcpy(q+1,&newrel,4);if(mprotect(page,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("assist-chicken patch restore protection");
    };
    patch_call(0x4a4ba1u,0x456d60u,nodes);
    patch_call(0x4a4c80u,0x453720u,vol1);
    patch_call(0x4a4c9fu,0x453720u,vol2);
    if(mprotect(code,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("assist-chicken stub mprotect");
}
void set_assist_chicken_inputs(const PcAssistChickenInputs& v){
    *reinterpret_cast<std::uint32_t*>(AssistChickenStateBase+0x00)=v.entry_nodes;
    *reinterpret_cast<std::int32_t*>(AssistChickenStateBase+0x04)=v.volume1;
    *reinterpret_cast<std::int32_t*>(AssistChickenStateBase+0x08)=v.volume2;
}
constexpr std::uint32_t VibrateHistoryBase=0x841bd0u;
constexpr std::size_t VibrateHistorySlots=8u;
constexpr std::size_t VibrateHistoryBytes=0x78u*VibrateHistorySlots;
void map_vibrate_history(){
    // This original PC moving-average buffer is zero-fill process state inside
    // the mapped .data virtual range. The oracle normally keeps static game
    // state read-only, so open only the single controlled history page.
    if(mprotect(reinterpret_cast<void*>(0x841000u),4096,PROT_READ|PROT_WRITE))
        throw std::runtime_error("vibrate history mprotect");
}
constexpr std::uint32_t RecordGhostHistoryBase=0x83db30u;
constexpr std::size_t RecordGhostHistoryBytes=0x3cu;
void map_record_ghost_state(){
    // RecordGhostCar owns one 30-word process-global moving-average buffer and
    // reads two steering service cells. Make only those mapped .data pages
    // writable so every oracle case can install deterministic explicit input.
    for(const std::uint32_t page:{0x82e000u,0x83d000u})
        if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE))
            throw std::runtime_error("record-ghost state mprotect");
}
void set_record_ghost_state(const PcRecordGhostInputs& in,const std::array<std::uint8_t,RecordGhostHistoryBytes>& history){
    *reinterpret_cast<std::int32_t*>(0x82e7ecu)=in.steering_override_active;
    *reinterpret_cast<std::uint16_t*>(0x82e7ccu)=static_cast<std::uint16_t>(in.steering_override);
    *reinterpret_cast<std::int32_t*>(0x78026cu)=in.game_mode;
    std::memcpy(reinterpret_cast<void*>(RecordGhostHistoryBase),history.data(),history.size());
}

constexpr std::uint32_t R043SessionStateBase=0x3301c000u;
void map_r043_session_state(){
    map_at(R043SessionStateBase,4096,PROT_READ|PROT_WRITE);
    if(mprotect(reinterpret_cast<void*>(0x836000u),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("r043 session page mprotect");
}
void set_r043_session_state(const PcSessionModeInputs& in){
    *reinterpret_cast<std::uint8_t*>(0x836374u)=in.manager_active?1u:0u;
    *reinterpret_cast<std::uint32_t*>(0x83637cu)=in.state_present?R043SessionStateBase:0u;
    *reinterpret_cast<std::uint32_t*>(R043SessionStateBase+0x20u)=in.state_code;
}

constexpr std::uint32_t R043OthcarStubBlock=0x32010000u;
constexpr std::uint32_t R043OthcarStateBase=0x3301d000u;
void map_r043_othcar_stubs(){
    auto* code=static_cast<std::uint8_t*>(map_at(R043OthcarStubBlock,4096,PROT_READ|PROT_WRITE));
    map_at(R043OthcarStateBase,4096,PROT_READ|PROT_WRITE);
    auto emit_u32=[](std::vector<std::uint8_t>& b,std::uint32_t v){for(unsigned k=0;k<4;++k)b.push_back(std::uint8_t(v>>(8u*k)));};
    // GetMaxCsLen: mov eax,[state.max]; ret.
    {std::vector<std::uint8_t>b{0xa1};emit_u32(b,R043OthcarStateBase+0x08u);b.push_back(0xc3);std::memcpy(code+0x000,b.data(),b.size());}
    // Road-info service: consume one scripted center/status per call.
    {
        std::vector<std::uint8_t>b;
        b.push_back(0x53); // push ebx
        b.push_back(0xa1);emit_u32(b,R043OthcarStateBase+0x00u); // eax=index
        b.insert(b.end(),{0x89,0xc3,0x6b,0xdb,0x0c}); // ebx=eax; imul ebx,12
        b.insert(b.end(),{0x8b,0x54,0x24,0x08}); // edx=out (push changed stack)
        for(unsigned k=0;k<3;++k){
            b.insert(b.end(),{0x8b,0x8b});emit_u32(b,R043OthcarStateBase+0x30u+k*4u);
            b.insert(b.end(),{0x89,0x4a,std::uint8_t(0x08u+k*4u)});
        }
        b.insert(b.end(),{0x8b,0x0c,0x85});emit_u32(b,R043OthcarStateBase+0x10u); // ecx=status[eax]
        b.push_back(0x40); // inc eax
        b.push_back(0xa3);emit_u32(b,R043OthcarStateBase+0x00u);
        b.insert(b.end(),{0x89,0xc8,0x5b,0xc3});
        std::memcpy(code+0x080,b.data(),b.size());
    }
    // CheckNextPlace: consume one scripted boolean per call. No place mutation is
    // needed because the road stub above intentionally ignores the serialized place.
    {
        std::vector<std::uint8_t>b{0xa1};emit_u32(b,R043OthcarStateBase+0x04u);
        b.insert(b.end(),{0x8b,0x0c,0x85});emit_u32(b,R043OthcarStateBase+0x20u);
        b.push_back(0x40);b.push_back(0xa3);emit_u32(b,R043OthcarStateBase+0x04u);
        b.insert(b.end(),{0x89,0xc8,0xc3});std::memcpy(code+0x180,b.data(),b.size());
    }
    auto patch_call=[&](std::uint32_t va,std::uint32_t expected,std::uint32_t target){
        auto* q=reinterpret_cast<std::uint8_t*>(va);std::int32_t rel{};std::memcpy(&rel,q+1,4);
        if(q[0]!=0xe8||std::uint32_t(std::int64_t(va+5u)+rel)!=expected)throw std::runtime_error("othcarGetR CALL target changed");
        auto* page=reinterpret_cast<void*>(va&~0xfffu);if(mprotect(page,4096,PROT_READ|PROT_WRITE|PROT_EXEC))throw std::runtime_error("othcarGetR patch protection");
        const auto nr=target-(va+5u);std::memcpy(q+1,&nr,4);if(mprotect(page,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("othcarGetR patch restore protection");
    };
    patch_call(0x4796a1u,0x43d470u,R043OthcarStubBlock+0x000u);
    patch_call(0x479710u,0x43e570u,R043OthcarStubBlock+0x080u);
    patch_call(0x479760u,0x43e570u,R043OthcarStubBlock+0x080u);
    patch_call(0x4797acu,0x43e570u,R043OthcarStubBlock+0x080u);
    patch_call(0x47973fu,0x46fe70u,R043OthcarStubBlock+0x180u);
    patch_call(0x47978fu,0x46fe70u,R043OthcarStubBlock+0x180u);
    if(mprotect(code,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("othcarGetR stub mprotect");
    if(mprotect(reinterpret_cast<void*>(0x80f000u),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("othcarGetR gate page mprotect");
}
void set_r043_othcar_inputs(const PcOthcarGetRInputs& in){
    *reinterpret_cast<std::uint32_t*>(R043OthcarStateBase+0x00u)=0u;
    *reinterpret_cast<std::uint32_t*>(R043OthcarStateBase+0x04u)=0u;
    *reinterpret_cast<std::uint32_t*>(R043OthcarStateBase+0x08u)=in.max_cs_len;
    for(unsigned i=0;i<3;++i){
        *reinterpret_cast<std::uint32_t*>(R043OthcarStateBase+0x10u+i*4u)=in.road_ok[i]?1u:0u;
        *reinterpret_cast<float*>(R043OthcarStateBase+0x30u+i*12u+0u)=in.centers[i].x;
        *reinterpret_cast<float*>(R043OthcarStateBase+0x30u+i*12u+4u)=in.centers[i].y;
        *reinterpret_cast<float*>(R043OthcarStateBase+0x30u+i*12u+8u)=in.centers[i].z;
    }
    for(unsigned i=0;i<2;++i)*reinterpret_cast<std::uint32_t*>(R043OthcarStateBase+0x20u+i*4u)=in.next_ok[i]?1u:0u;
    *reinterpret_cast<std::uint32_t*>(0x80fb14u)=in.alternate_radius?1u:0u;
}

constexpr std::uint32_t R045CourseStateBase=0x3301b000u;
void map_r045_course_service_state(){
    map_at(R045CourseStateBase,4096,PROT_READ|PROT_WRITE);
    for(const std::uint32_t page:{0x7d2000u,0x7d3000u})
        if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("r045 course-service page mprotect");
}

constexpr std::uint32_t R046StubBlock=0x32012000u;
constexpr std::uint32_t R046StateBase=0x37000000u;
void map_r046_road_stubs(){
    auto* code=static_cast<std::uint8_t*>(map_at(R046StubBlock,4096,PROT_READ|PROT_WRITE));
    map_at(R046StateBase,8192,PROT_READ|PROT_WRITE);
    for(const std::uint32_t page:{0x6a5000u,0x7d2000u})
        if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("r046 data page mprotect");
    // Both road-table families are static .data.  The secondary family spans
    // well beyond its first page when selectors/rows are varied by the oracle.
    for(std::uint32_t page=0x804000u;page<=0x82a000u;page+=0x1000u)
        if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("r046 road table page mprotect");
    auto emit32=[](std::vector<std::uint8_t>& b,std::uint32_t v){for(unsigned k=0;k<4;++k)b.push_back(std::uint8_t(v>>(8u*k)));};
    {std::vector<std::uint8_t>b{0xa1};emit32(b,R046StateBase+0x00u);b.push_back(0xc3);std::memcpy(code+0x000,b.data(),b.size());}
    {std::vector<std::uint8_t>b{0xd9,0x05};emit32(b,R046StateBase+0x04u);b.push_back(0xc3);std::memcpy(code+0x020,b.data(),b.size());}
    {std::vector<std::uint8_t>b{0xa1};emit32(b,R046StateBase+0x08u);b.push_back(0xc3);std::memcpy(code+0x030,b.data(),b.size());}
    // Protected 0x44F0F0 tail: MOV preserves CMP flags, then jump back to 0x44F10D.
    {std::vector<std::uint8_t>b{0xa1};emit32(b,R046StateBase+0x14u);b.push_back(0xe9);const std::uint32_t from=R046StubBlock+0x040u+std::uint32_t(b.size())+4u;emit32(b,0x44f10du-from);std::memcpy(code+0x040,b.data(),b.size());}
    {
        std::vector<std::uint8_t>b{0x8b,0x54,0x24,0x04};
        for(unsigned off=0;off<0x58u;off+=4u){b.push_back(0xa1);emit32(b,R046StateBase+0x100u+off);b.insert(b.end(),{0x89,0x42,std::uint8_t(off)});}
        b.push_back(0xa1);emit32(b,R046StateBase+0x0cu);b.push_back(0xc3);std::memcpy(code+0x080,b.data(),b.size());
    }
    auto patch_call=[&](std::uint32_t va,std::uint32_t expected,std::uint32_t target){
        auto* q=reinterpret_cast<std::uint8_t*>(va);std::int32_t rel{};std::memcpy(&rel,q+1,4);
        if(q[0]!=0xe8||std::uint32_t(std::int64_t(va+5u)+rel)!=expected)throw std::runtime_error("r046 CALL target changed");
        auto* page=reinterpret_cast<void*>(va&~0xfffu);if(mprotect(page,4096,PROT_READ|PROT_WRITE|PROT_EXEC))throw std::runtime_error("r046 patch protection");
        const auto nr=target-(va+5u);std::memcpy(q+1,&nr,4);if(mprotect(page,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("r046 patch restore protection");
    };
    patch_call(0x44dc55u,0x44c8d0u,R046StubBlock+0x000u);
    patch_call(0x47b8fdu,0x479d90u,R046StubBlock+0x020u);
    patch_call(0x47b94du,0x46f7a0u,R046StubBlock+0x030u);
    patch_call(0x4a3fd2u,0x43e3b0u,R046StubBlock+0x080u);
    {
        auto* q=reinterpret_cast<std::uint8_t*>(0x44f108u);std::int32_t rel{};std::memcpy(&rel,q+1,4);
        (void)gate_original(0x44f108u,q,5);if(!g_steam_exe&&(q[0]!=0xe9||std::uint32_t(std::int64_t(0x44f10du)+rel)!=0x40e705u))throw std::runtime_error("r046 protected gate jump changed");
        auto* page=reinterpret_cast<void*>(0x44f000u);if(mprotect(page,4096,PROT_READ|PROT_WRITE|PROT_EXEC))throw std::runtime_error("r046 gate patch protection");
        // Steam: 44F108 is the plain `mov eax,1` (B8): the JMP opcode is written too.
        const std::uint32_t nr=(R046StubBlock+0x040u)-0x44f10du;q[0]=0xe9;std::memcpy(q+1,&nr,4);if(mprotect(page,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("r046 gate patch restore protection");
    }
    {
        auto* q=reinterpret_cast<std::uint8_t*>(0x4a3f8au);static const std::uint8_t expected[6]={0xff,0x25,0xc0,0x9a,0x03,0x01};
        (void)gate_original(0x4a3f8au,q,6);if(!g_steam_exe&&std::memcmp(q,expected,6)!=0)throw std::runtime_error("r046 selector entry changed");
        auto* page=reinterpret_cast<void*>(0x4a3000u);if(mprotect(page,4096,PROT_READ|PROT_WRITE|PROT_EXEC))throw std::runtime_error("r046 selector patch protection");
        q[0]=0xa1;std::uint32_t addr=R046StateBase+0x10u;std::memcpy(q+1,&addr,4);q[5]=0x90;
        if(mprotect(page,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("r046 selector patch restore protection");
    }
    if(mprotect(code,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("r046 stub mprotect");
    *reinterpret_cast<std::uint32_t*>(0x7d2df4u)=R046StateBase+0x600u;
    *reinterpret_cast<std::uint32_t*>(0x6a55c8u)=R046StateBase+0x700u;
}
void set_r046_descriptor(bool present,std::uint32_t value,std::uint32_t fallback){
    *reinterpret_cast<std::uint32_t*>(R046StateBase+0x00u)=present?R046StateBase+0x400u:0u;
    *reinterpret_cast<std::uint32_t*>(R046StateBase+0x400u+0x14u)=R046StateBase+0x500u;
    *reinterpret_cast<std::uint32_t*>(R046StateBase+0x500u)=value;
    *reinterpret_cast<std::uint32_t*>(R046StateBase+0x600u)=fallback;
}
void set_r046_lane_inputs(const PcRoadLaneInputs& in){
    *reinterpret_cast<float*>(R046StateBase+0x04u)=in.route_width;
    *reinterpret_cast<std::uint32_t*>(R046StateBase+0x08u)=in.route_flags;
    set_r046_descriptor(true,std::uint32_t(in.stage_unique),std::uint32_t(in.stage_unique));
    *reinterpret_cast<std::uint16_t*>(R046StateBase+0x700u+0x7eu)=in.rolling_reference;
    *reinterpret_cast<std::uint32_t*>(R046StateBase+0x14u)=in.protected_gate_value;
}
void set_r046_cache_inputs(const PcRoadCacheRefreshInputs& in){
    *reinterpret_cast<std::int32_t*>(R046StateBase+0x10u)=in.selector_result;
    *reinterpret_cast<std::uint32_t*>(R046StateBase+0x0cu)=in.query_success?1u:0u;
    std::memcpy(reinterpret_cast<void*>(R046StateBase+0x100u),in.query_output.data(),in.query_output.size());
}


constexpr std::uint32_t R046ParentStubBlock=0x32013000u;
constexpr std::uint32_t R046ParentStateBase=0x37002000u;
void map_r046_common_parent_stubs(){
    auto* code=static_cast<std::uint8_t*>(map_at(R046ParentStubBlock,4096,PROT_READ|PROT_WRITE));
    map_at(R046ParentStateBase,4096,PROT_READ|PROT_WRITE);
    auto emit32=[](std::vector<std::uint8_t>& b,std::uint32_t v){for(unsigned k=0;k<4;++k)b.push_back(std::uint8_t(v>>(8u*k)));};
    auto make_stub=[&](std::size_t slot,std::uint32_t id,int result_off){
        std::vector<std::uint8_t>b;
        b.push_back(0x50);                         // push eax
        b.push_back(0xa1);emit32(b,R046ParentStateBase+0x08u); // eax=count
        b.insert(b.end(),{0xc7,0x04,0x85});emit32(b,R046ParentStateBase+0x100u);emit32(b,id);
        b.push_back(0x40);                         // inc eax
        b.push_back(0xa3);emit32(b,R046ParentStateBase+0x08u);
        b.push_back(0x58);                         // restore caller eax
        if(result_off>=0){b.push_back(0xa1);emit32(b,R046ParentStateBase+std::uint32_t(result_off));}
        b.push_back(0xc3);
        if(b.size()>0x20u)throw std::runtime_error("r046 parent stub too large");
        std::memcpy(code+slot*0x20u,b.data(),b.size());
    };
    struct Patch{std::uint32_t site,target;int result_off;};
    static const Patch patches[]={
        {0x4a810cu,0x49b2d0u,0x00},
        {0x4a8149u,0x4a4010u,-1},{0x4a8151u,0x4a61f0u,-1},{0x4a8159u,0x4a0000u,-1},
        {0x4a8175u,0x517410u,-1},{0x4a8181u,0x4a2fa0u,-1},{0x4a8188u,0x4a3310u,-1},
        {0x4a8191u,0x4a3950u,-1},{0x4a819au,0x4a7ec0u,-1},{0x4a8219u,0x519830u,-1},
        {0x4a8220u,0x4a1b70u,-1},{0x4a8227u,0x4a1a90u,-1},{0x4a822eu,0x4a63c0u,-1},
        {0x4a8235u,0x4a65c0u,-1},{0x4a823au,0x4a1140u,-1},{0x4a8240u,0x4a1680u,-1},
        {0x4a8246u,0x458e40u,-1},{0x4a824bu,0x4a4830u,-1},{0x4a8252u,0x4a4900u,-1},
        {0x4a8257u,0x4a4ba0u,-1},{0x4a825du,0x4a2400u,-1},{0x4a8263u,0x4a25f0u,-1},
        {0x4a826cu,0x4a2650u,-1},{0x4a8273u,0x4a2d70u,-1},{0x4a8279u,0x4a2910u,-1},
        {0x4a8280u,0x4a3d40u,-1},{0x4a8286u,0x4a45f0u,-1},{0x4a828cu,0x46eb40u,-1},
        {0x4a8292u,0x4a4710u,-1},{0x4a82a4u,0x4962a0u,0x04},{0x4a82aeu,0x47f780u,-1},
        {0x4a82b6u,0x4671d0u,-1},{0x4a82bfu,0x479670u,-1}
    };
    auto* page=reinterpret_cast<void*>(0x4a8000u);
    if(mprotect(page,4096,PROT_READ|PROT_WRITE|PROT_EXEC))throw std::runtime_error("r046 parent patch protection");
    for(std::size_t i=0;i<std::size(patches);++i){
        const auto& p=patches[i];auto* q=reinterpret_cast<std::uint8_t*>(p.site);std::int32_t rel{};std::memcpy(&rel,q+1,4);
        if(q[0]!=0xe8||std::uint32_t(std::int64_t(p.site+5u)+rel)!=p.target)throw std::runtime_error("r046 CommonPlCar CALL target changed");
        make_stub(i,p.target,p.result_off);const std::uint32_t nr=(R046ParentStubBlock+std::uint32_t(i*0x20u))-(p.site+5u);std::memcpy(q+1,&nr,4);
    }
    if(mprotect(page,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("r046 parent patch restore protection");
    if(mprotect(code,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("r046 parent stub mprotect");
}
void reset_r046_common_parent_state(std::int16_t timer,bool session_mode4){
    std::memset(reinterpret_cast<void*>(R046ParentStateBase),0,4096);
    *reinterpret_cast<std::uint32_t*>(R046ParentStateBase+0x00u)=std::uint16_t(timer);
    *reinterpret_cast<std::uint32_t*>(R046ParentStateBase+0x04u)=session_mode4?1u:0u;
}
std::vector<std::uint32_t> r046_common_parent_trace(){
    const auto n=*reinterpret_cast<std::uint32_t*>(R046ParentStateBase+0x08u);
    if(n>40u)throw std::runtime_error("r046 CommonPlCar trace overflow");
    auto* p=reinterpret_cast<std::uint32_t*>(R046ParentStateBase+0x100u);
    return {p,p+n};
}

void r046_write_rel32(std::uint32_t site,std::uint8_t opcode,std::uint32_t target){
    auto* page=reinterpret_cast<void*>(site&~0xfffu);
    if(mprotect(page,4096,PROT_READ|PROT_WRITE|PROT_EXEC))throw std::runtime_error("r046 toggle protection");
    auto* q=reinterpret_cast<std::uint8_t*>(site);q[0]=opcode;const std::uint32_t rel=target-(site+5u);std::memcpy(q+1,&rel,4);
    if(mprotect(page,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("r046 toggle restore protection");
}
void set_r046_road_patches(bool enabled){
    struct C{std::uint32_t site,orig,stub;};
    static const C calls[]={
        {0x44dc55u,0x44c8d0u,R046StubBlock+0x000u},
        {0x47b8fdu,0x479d90u,R046StubBlock+0x020u},
        {0x47b94du,0x46f7a0u,R046StubBlock+0x030u},
        {0x4a3fd2u,0x43e3b0u,R046StubBlock+0x080u}
    };
    for(const auto& c:calls)r046_write_rel32(c.site,0xe8,enabled?c.stub:c.orig);
    if(g_steam_exe&&!enabled){r060_raw_write(0x44f108u,gate_original(0x44f108u,nullptr,5),5);}
    else r046_write_rel32(0x44f108u,0xe9,enabled?R046StubBlock+0x040u:0x40e705u);
    auto* page=reinterpret_cast<void*>(0x4a3000u);if(mprotect(page,4096,PROT_READ|PROT_WRITE|PROT_EXEC))throw std::runtime_error("r046 selector toggle protection");
    auto* q=reinterpret_cast<std::uint8_t*>(0x4a3f8au);
    if(enabled){q[0]=0xa1;std::uint32_t a=R046StateBase+0x10u;std::memcpy(q+1,&a,4);q[5]=0x90;}
    else{static const std::uint8_t original[6]={0xff,0x25,0xc0,0x9a,0x03,0x01};std::memcpy(q,gate_original(0x4a3f8au,original,6),6);}
    if(mprotect(page,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("r046 selector toggle restore protection");
}
void set_r046_common_parent_patches(bool enabled){
    struct P{std::uint32_t site,target;};
    static const P patches[]={
        {0x4a810cu,0x49b2d0u},{0x4a8149u,0x4a4010u},{0x4a8151u,0x4a61f0u},{0x4a8159u,0x4a0000u},
        {0x4a8175u,0x517410u},{0x4a8181u,0x4a2fa0u},{0x4a8188u,0x4a3310u},{0x4a8191u,0x4a3950u},
        {0x4a819au,0x4a7ec0u},{0x4a8219u,0x519830u},{0x4a8220u,0x4a1b70u},{0x4a8227u,0x4a1a90u},
        {0x4a822eu,0x4a63c0u},{0x4a8235u,0x4a65c0u},{0x4a823au,0x4a1140u},{0x4a8240u,0x4a1680u},
        {0x4a8246u,0x458e40u},{0x4a824bu,0x4a4830u},{0x4a8252u,0x4a4900u},{0x4a8257u,0x4a4ba0u},
        {0x4a825du,0x4a2400u},{0x4a8263u,0x4a25f0u},{0x4a826cu,0x4a2650u},{0x4a8273u,0x4a2d70u},
        {0x4a8279u,0x4a2910u},{0x4a8280u,0x4a3d40u},{0x4a8286u,0x4a45f0u},{0x4a828cu,0x46eb40u},
        {0x4a8292u,0x4a4710u},{0x4a82a4u,0x4962a0u},{0x4a82aeu,0x47f780u},{0x4a82b6u,0x4671d0u},
        {0x4a82bfu,0x479670u}
    };
    for(std::size_t i=0;i<std::size(patches);++i)r046_write_rel32(patches[i].site,0xe8,enabled?R046ParentStubBlock+std::uint32_t(i*0x20u):patches[i].target);
}


constexpr std::uint32_t R047StubBlock=0x32024000u;
constexpr std::uint32_t R047StateBase=0x33060000u;
void map_r047_game_control_stubs(){
    auto* code=static_cast<std::uint8_t*>(map_at(R047StubBlock,4096,PROT_READ|PROT_WRITE));
    map_at(R047StateBase,4096,PROT_READ|PROT_WRITE);
    auto emit32=[](std::uint8_t* p,std::uint32_t v){std::memcpy(p,&v,4);};
    // generic GetVolume(channel): mov eax,[esp+4]; mov eax,[state+eax*4]; ret
    {std::uint8_t b[]={0x8b,0x44,0x24,0x04,0x8b,0x04,0x85,0,0,0,0,0xc3};emit32(b+7,R047StateBase+0x00u);std::memcpy(code+0x000,b,sizeof(b));}
    // generic course-end lookup 0/1.
    {std::uint8_t b[]={0x8b,0x44,0x24,0x04,0x8b,0x04,0x85,0,0,0,0,0xc3};emit32(b+7,R047StateBase+0x10u);std::memcpy(code+0x020,b,sizeof(b));}
    // gate getter.
    {std::uint8_t b[]={0xa1,0,0,0,0,0xc3};emit32(b+1,R047StateBase+0x18u);std::memcpy(code+0x040,b,sizeof(b));}
    auto patch_call=[&](std::uint32_t va,std::uint32_t expected,std::uint32_t target){
        auto* q=reinterpret_cast<std::uint8_t*>(va);std::int32_t rel{};std::memcpy(&rel,q+1,4);
        if(q[0]!=0xe8||std::uint32_t(std::int64_t(va+5u)+rel)!=expected)throw std::runtime_error("r047 CALL target changed");
        auto* page=reinterpret_cast<void*>(va&~0xfffu);if(mprotect(page,4096,PROT_READ|PROT_WRITE|PROT_EXEC))throw std::runtime_error("r047 patch protection");
        const std::uint32_t nr=target-(va+5u);std::memcpy(q+1,&nr,4);if(mprotect(page,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("r047 patch restore protection");
    };
    for(auto va:{0x49faffu,0x49fb24u,0x49fb39u,0x49fb46u,0x4a5190u,0x4a51a3u})patch_call(va,0x453720u,R047StubBlock+0x000u);
    for(auto va:{0x4a215bu,0x4a218bu,0x4a21b3u})patch_call(va,0x43d470u,R047StubBlock+0x020u);
    patch_call(0x4a2229u,0x4957f0u,R047StubBlock+0x040u);
    if(mprotect(code,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("r047 stub mprotect");
    for(std::uint32_t page:{0x00780000u,0x007f1000u,0x007f2000u,0x007df000u,0x00680000u,0x00841000u})
        if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("r047 data page mprotect");
}
void set_r047_volumes(std::int32_t v0,std::int32_t v1,std::int32_t v2){
    *reinterpret_cast<std::int32_t*>(R047StateBase+0)=v0;*reinterpret_cast<std::int32_t*>(R047StateBase+4)=v1;*reinterpret_cast<std::int32_t*>(R047StateBase+8)=v2;
}
void set_r047_progress_services(std::uint16_t e0,std::uint16_t e1,bool gate){
    *reinterpret_cast<std::uint32_t*>(R047StateBase+0x10u)=e0;*reinterpret_cast<std::uint32_t*>(R047StateBase+0x14u)=e1;*reinterpret_cast<std::uint32_t*>(R047StateBase+0x18u)=gate?1u:0u;
}

constexpr std::uint32_t R048StubBlock=0x32015000u;
constexpr std::uint32_t R048StateBase=0x33070000u;
struct R048Patch {std::uint32_t site,target;int result_off;};
static const R048Patch R048ParentPatches[]={
 {0x4a833fu,0x43f9f0u,0x00},{0x4a8348u,0x43f9c0u,0x04},{0x4a8351u,0x44ff10u,-1},
 {0x4a8416u,0x43cc20u,-1},{0x4a84eau,0x44c940u,0x0c},{0x4a84f0u,0x451350u,0x08},
 {0x4a853au,0x409ef0u,-1},{0x4a8540u,0x487740u,-1},{0x4a8546u,0x40a270u,-1},
 {0x4a8555u,0x40a7d0u,-1},{0x4a855fu,0x40a270u,-1},{0x4a8569u,0x40a0d0u,-1},
 {0x4a85aeu,0x49fad0u,-1},{0x4a85bau,0x49fb70u,-1},{0x4a85bfu,0x4a4d20u,-1},
 {0x4a85c4u,0x4a50f0u,-1},{0x4a85ceu,0x4a5260u,-1},{0x4a85d9u,0x502c90u,-1},
 {0x4a85e4u,0x4a8100u,-1},{0x4a85e9u,0x40a010u,-1},{0x4a85efu,0x455f50u,-1},
 {0x4a85f5u,0x4a2130u,-1},{0x4a8605u,0x43d470u,0x10},{0x4a8653u,0x45a2b0u,0x14},
 {0x4a866bu,0x45c440u,0x18},{0x4a867cu,0x4a5650u,-1},{0x4a8688u,0x55a930u,0x1c},
 {0x4a8697u,0x46c390u,-1},{0x4a86a0u,0x4a2ee0u,-1},{0x4a8713u,0x457770u,-1}
};
void map_r048_parent_stubs(){
    auto* code=static_cast<std::uint8_t*>(map_at(R048StubBlock,4096,PROT_READ|PROT_WRITE));
    map_at(R048StateBase,4096,PROT_READ|PROT_WRITE);
    for(std::uint32_t page:{0x007d3000u,0x007f8000u,0x0082e000u,0x00841000u,0x00716000u})
        if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("r048 data page mprotect");
    auto emit32=[](std::vector<std::uint8_t>& b,std::uint32_t v){for(unsigned k=0;k<4;++k)b.push_back(std::uint8_t(v>>(8u*k)));};
    auto make_stub=[&](std::size_t slot,std::uint32_t id,int result_off){
        std::vector<std::uint8_t>b;b.push_back(0x50);b.push_back(0xa1);emit32(b,R048StateBase+0x20u);
        b.insert(b.end(),{0xc7,0x04,0x85});emit32(b,R048StateBase+0x100u);emit32(b,id);
        b.push_back(0x40);b.push_back(0xa3);emit32(b,R048StateBase+0x20u);b.push_back(0x58);
        if(result_off>=0){b.push_back(0xa1);emit32(b,R048StateBase+std::uint32_t(result_off));}
        if(id==0x4a2ee0u){const std::uint32_t here=R048StubBlock+std::uint32_t(slot*0x20u+b.size());const std::uint32_t helper=R048StubBlock+0x800u;b.push_back(0xe9);emit32(b,helper-(here+5u));}
        else b.push_back(0xc3);
        if(b.size()>0x20u)throw std::runtime_error("r048 parent stub too large");std::memcpy(code+slot*0x20u,b.data(),b.size());
    };
    {std::vector<std::uint8_t> h;const std::uint32_t disp[6]={0x2b2u,0x3a6u,0x2b0u,0x3a4u,0x498u,0x58cu};for(unsigned k=0;k<6;++k){h.push_back(0x66);h.push_back(0xa1);emit32(h,R048StateBase+0x40u+k*4u);h.insert(h.end(),{0x66,0x89,0x84,0x24});emit32(h,disp[k]);}h.push_back(0xc3);std::memcpy(code+0x800u,h.data(),h.size());}
    auto* page=reinterpret_cast<void*>(0x4a8000u);if(mprotect(page,4096,PROT_READ|PROT_WRITE|PROT_EXEC))throw std::runtime_error("r048 parent patch protection");
    for(std::size_t i=0;i<std::size(R048ParentPatches);++i){const auto& p=R048ParentPatches[i];auto* q=reinterpret_cast<std::uint8_t*>(p.site);std::int32_t rel{};std::memcpy(&rel,q+1,4);if(q[0]!=0xe8||std::uint32_t(std::int64_t(p.site+5u)+rel)!=p.target)throw std::runtime_error("r048 GamePlCar CALL target changed");make_stub(i,p.target,p.result_off);const std::uint32_t nr=(R048StubBlock+std::uint32_t(i*0x20u))-(p.site+5u);std::memcpy(q+1,&nr,4);}
    if(mprotect(page,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("r048 parent patch restore protection");if(mprotect(code,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("r048 parent stub mprotect");
}
void set_r048_parent_patches(bool enabled){for(std::size_t i=0;i<std::size(R048ParentPatches);++i)r046_write_rel32(R048ParentPatches[i].site,0xe8,enabled?R048StubBlock+std::uint32_t(i*0x20u):R048ParentPatches[i].target);}
void reset_r048_parent_state(const PcGamePlCarParentInputs& in){std::memset(reinterpret_cast<void*>(R048StateBase),0,4096);*reinterpret_cast<std::uint32_t*>(R048StateBase+0x00u)=in.game_state_byte;*reinterpret_cast<std::uint32_t*>(R048StateBase+0x04u)=in.game_flag;*reinterpret_cast<std::uint32_t*>(R048StateBase+0x08u)=in.entry_mode;*reinterpret_cast<std::uint32_t*>(R048StateBase+0x0cu)=7u;*reinterpret_cast<std::uint32_t*>(R048StateBase+0x10u)=in.course_end;*reinterpret_cast<std::uint32_t*>(R048StateBase+0x14u)=in.rank;*reinterpret_cast<std::uint32_t*>(R048StateBase+0x18u)=in.heart_mode;*reinterpret_cast<std::uint32_t*>(R048StateBase+0x1cu)=in.network_tail_active?1u:0u;for(unsigned k=0;k<6;++k)*reinterpret_cast<std::uint32_t*>(R048StateBase+0x40u+k*4u)=in.capture_words[k];}
std::vector<std::uint32_t> r048_parent_trace(){const auto n=*reinterpret_cast<std::uint32_t*>(R048StateBase+0x20u);if(n>64u)throw std::runtime_error("r048 trace overflow");auto* p=reinterpret_cast<std::uint32_t*>(R048StateBase+0x100u);return {p,p+n};}

constexpr std::uint32_t R049StubBlock=0x32016000u;
constexpr std::uint32_t R049StateBase=0x33071000u;
void map_r049_child_stubs(){
    auto* code=static_cast<std::uint8_t*>(map_at(R049StubBlock,4096,PROT_READ|PROT_WRITE));
    map_at(R049StateBase,4096,PROT_READ|PROT_WRITE);
    auto put32=[](std::uint8_t* p,std::uint32_t v){std::memcpy(p,&v,4);};
    // 0x000: stand-in for the external thiscall network-manager method.  Record
    // ECX and its two stack arguments, then pop those arguments like the original.
    {
        std::uint8_t b[]={
            0xa1,0,0,0,0,0x40,0xa3,0,0,0,0,
            0x8b,0x44,0x24,0x04,0xa3,0,0,0,0,
            0x8b,0x44,0x24,0x08,0xa3,0,0,0,0,
            0x89,0x0d,0,0,0,0,0xc2,0x08,0x00};
        put32(b+1,R049StateBase+0x00u);put32(b+7,R049StateBase+0x00u);
        put32(b+16,R049StateBase+0x04u);put32(b+25,R049StateBase+0x08u);put32(b+31,R049StateBase+0x0cu);
        std::memcpy(code+0x000,b,sizeof(b));
    }
    // 0x040: deterministic rank provider used only to validate the 0x45A2B0
    // indirect-jump ABI: return 0x12340007 + 3*player_id.
    {
        std::uint8_t b[]={0x0f,0xb6,0x44,0x24,0x04,0x8d,0x04,0x40,0x05,0,0,0,0,0xc3};
        put32(b+9,0x12340007u);std::memcpy(code+0x040,b,sizeof(b));
    }
    if(mprotect(code,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("r049 stub mprotect");
    if(mprotect(reinterpret_cast<void*>(0x635000u),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("r049 stage page mprotect");
}
void set_r049_network_patch(bool enabled){
    r046_write_rel32(0x46c39fu,0xe8,enabled?R049StubBlock+0x000u:0x4fb870u);
}
void reset_r049_network_state(){std::memset(reinterpret_cast<void*>(R049StateBase),0,0x20);}

constexpr std::uint32_t R050CandidateBase=0x35000000u;
constexpr std::size_t R050CandidateCount=23u;
void map_r050_slipstream_state(){
    map_at(R050CandidateBase,R050CandidateCount*event_size,PROT_READ|PROT_WRITE);
    for(std::uint32_t page:{0x00799000u,0x0079a000u,0x0079f000u})
        if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("r050 slipstream data page mprotect");
}
void load_r050_slipstream_original(const PcCheckSlipStreamInputs& in){
    *reinterpret_cast<std::uint32_t*>(0x7f94c0u)=in.network_session_active?1u:0u;
    for(std::size_t k=0;k<R050CandidateCount;++k){
        const std::uint32_t ptr=R050CandidateBase+std::uint32_t(k*event_size);
        std::memset(reinterpret_cast<void*>(std::uintptr_t(ptr)),0,event_size);
        auto& c=in.candidates[k];Bytes e(reinterpret_cast<void*>(std::uintptr_t(ptr)),event_size);
        e.put32(0x04,c.flags);e.put32(0x0d14,c.network_state);
        e.putf(0x14,c.position.x);e.putf(0x18,c.position.y);e.putf(0x1c,c.position.z);
        e.putf(0x20,c.direction.x);e.putf(0x24,c.direction.y);e.putf(0x28,c.direction.z);
        e.putf(0x1c4,c.speed);e.put16(0x0b72,std::uint16_t(c.cooldown));
        *reinterpret_cast<std::uint32_t*>(0x799d54u+std::uint32_t(k*0x3cu))=ptr;
        *reinterpret_cast<std::uint8_t*>(0x79fb48u+std::uint32_t(k+9u))=c.open_state;
    }
}
std::array<std::uint8_t,R050CandidateCount*2u> r050_original_cooldowns(){
    std::array<std::uint8_t,R050CandidateCount*2u> out{};
    for(std::size_t k=0;k<R050CandidateCount;++k){
        Bytes e(reinterpret_cast<void*>(std::uintptr_t(R050CandidateBase+std::uint32_t(k*event_size))),event_size);
        const auto u=std::uint16_t(e.i16(0x0b72));out[k*2u]=std::uint8_t(u);out[k*2u+1u]=std::uint8_t(u>>8);
    }
    return out;
}
std::array<std::uint8_t,R050CandidateCount*2u> r050_native_cooldowns(const PcCheckSlipStreamInputs& in){
    std::array<std::uint8_t,R050CandidateCount*2u> out{};
    for(std::size_t k=0;k<R050CandidateCount;++k){const auto u=std::uint16_t(in.candidates[k].cooldown);out[k*2u]=std::uint8_t(u);out[k*2u+1u]=std::uint8_t(u>>8);}return out;
}


constexpr std::uint32_t R051StubBlock=0x32017000u;
constexpr std::uint32_t R051StateBase=0x33072000u;
constexpr std::uint32_t R051RecordBase=0x39000000u;
struct R051Patch {std::uint32_t site,target;};
static const R051Patch R051ParentPatches[]={
    {0x47572du,0x4755c0u},{0x475739u,0x4872f0u},
    {0x47574bu,0x4a2650u},{0x475750u,0x409f30u},{0x47575cu,0x40a0d0u},
    {0x475761u,0x40a010u},{0x475767u,0x4a2ee0u},{0x47576du,0x4a4710u},
    {0x475778u,0x4a8330u},{0x475787u,0x409f90u},
    {0x4757abu,0x40a7d0u},{0x4757e1u,0x43eb60u},{0x475858u,0x46bbf0u},
    {0x475893u,0x43d390u},{0x4758a5u,0x40a010u}
};
void map_r051_parent_stubs(){
    auto* code=static_cast<std::uint8_t*>(map_at(R051StubBlock,4096,PROT_READ|PROT_WRITE));
    map_at(R051StateBase,4096,PROT_READ|PROT_WRITE);
    map_at(R051RecordBase,4096,PROT_READ|PROT_WRITE);
    if(mprotect(reinterpret_cast<void*>(0x82e000u),4096,PROT_READ|PROT_WRITE))
        throw std::runtime_error("r051 road-pointer page mprotect");
    auto emit32=[](std::vector<std::uint8_t>& b,std::uint32_t v){for(unsigned k=0;k<4;++k)b.push_back(std::uint8_t(v>>(8u*k)));};
    auto custom_target=[](std::uint32_t id)->std::uint32_t{
        if(id==0x40a7d0u)return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&pas_transform_stub32));
        if(id==0x43eb60u)return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&pas_road_stub32));
        if(id==0x46bbf0u)return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&pas_tire_stub32));
        if(id==0x43d390u)return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&pas_normal_stub32));
        return 0u;
    };
    auto make_stub=[&](std::size_t slot,std::uint32_t id){
        constexpr std::uint32_t stride=0x30u;
        std::vector<std::uint8_t>b;
        b.push_back(0x50);                                      // push eax
        b.push_back(0xa1);emit32(b,R051StateBase+0x04u);        // eax=count
        b.insert(b.end(),{0xc7,0x04,0x85});emit32(b,R051StateBase+0x100u);emit32(b,id);
        b.push_back(0x40);                                      // inc eax
        b.push_back(0xa3);emit32(b,R051StateBase+0x04u);        // count=eax
        b.push_back(0x58);                                      // pop eax
        if(id==0x4872f0u){b.push_back(0xa1);emit32(b,R051StateBase+0x00u);b.push_back(0xc3);}
        else if(const auto target=custom_target(id);target){
            const std::uint32_t here=R051StubBlock+std::uint32_t(slot*stride+b.size());
            b.push_back(0xe9);emit32(b,target-(here+5u));
        }else b.push_back(0xc3);
        if(b.size()>stride)throw std::runtime_error("r051 parent stub too large");
        std::memcpy(code+slot*stride,b.data(),b.size());
    };
    for(std::size_t i=0;i<std::size(R051ParentPatches);++i){
        const auto& p=R051ParentPatches[i];auto* q=reinterpret_cast<std::uint8_t*>(p.site);std::int32_t rel{};std::memcpy(&rel,q+1,4);
        if(q[0]!=0xe8||std::uint32_t(std::int64_t(p.site+5u)+rel)!=p.target)
            throw std::runtime_error("r051 PasPlCar CALL target changed");
        make_stub(i,p.target);
    }
    if(mprotect(code,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("r051 parent stub mprotect");
}
void set_r051_parent_patches(bool enabled){
    constexpr std::uint32_t stride=0x30u;
    for(std::size_t i=0;i<std::size(R051ParentPatches);++i)
        r046_write_rel32(R051ParentPatches[i].site,0xe8,enabled?R051StubBlock+std::uint32_t(i*stride):R051ParentPatches[i].target);
}
void reset_r051_parent_state(const PcPasPlCarInputs& in,const std::array<std::array<std::uint8_t,0x100>,4>& records){
    std::memset(reinterpret_cast<void*>(R051StateBase),0,4096);
    *reinterpret_cast<std::uint32_t*>(R051StateBase+0x00u)=in.petty_auto_scene_list;
    pas_stub_index=0;pas_stub_overflow=0;
    std::fill(std::begin(pas_seen_tire_index),std::end(pas_seen_tire_index),0xffffffffu);
    std::fill(std::begin(pas_seen_normal_polygon),std::end(pas_seen_normal_polygon),0xffffffffu);
    std::fill(std::begin(pas_seen_normal_type),std::end(pas_seen_normal_type),0xffffffffu);
    for(unsigned k=0;k<4;++k){
        const auto& w=in.wheel[k];
        pas_transform_xyz[k*3u+0u]=fbits(w.transformed_point.x);pas_transform_xyz[k*3u+1u]=fbits(w.transformed_point.y);pas_transform_xyz[k*3u+2u]=fbits(w.transformed_point.z);
        pas_road_y[k]=fbits(w.road_y);pas_road_polygon[k]=w.road_polygon;pas_road_ret[k]=w.road_result;
        pas_tire_xyz[k*3u+0u]=fbits(w.tire_position.x);pas_tire_xyz[k*3u+1u]=fbits(w.tire_position.y);pas_tire_xyz[k*3u+2u]=fbits(w.tire_position.z);
        pas_normal_xyz[k*3u+0u]=fbits(w.road_normal.x);pas_normal_xyz[k*3u+1u]=fbits(w.road_normal.y);pas_normal_xyz[k*3u+2u]=fbits(w.road_normal.z);
        const auto address=R051RecordBase+k*0x100u;
        std::memcpy(reinterpret_cast<void*>(std::uintptr_t(address)),records[k].data(),records[k].size());
        *reinterpret_cast<std::uint32_t*>(0x82ea38u+k*4u)=address;
    }
}
std::vector<std::uint32_t> r051_parent_trace(){
    const auto n=*reinterpret_cast<std::uint32_t*>(R051StateBase+0x04u);if(n>64u)throw std::runtime_error("r051 PasPlCar trace overflow");
    auto* p=reinterpret_cast<std::uint32_t*>(R051StateBase+0x100u);return {p,p+n};
}


constexpr std::uint32_t R052StubBlock=0x32018000u;
constexpr std::uint32_t R052StateBase=0x33073000u;
constexpr std::uint32_t R052RecordBase=0x799b30u;
constexpr std::uint32_t R052FlagsBase=0x79fb48u;
constexpr std::uint32_t R052CurrentPtr=0x7a01d0u;
constexpr std::uint32_t R052CallbackCount=8u;
constexpr std::uint32_t R052CallbackStride=0x40u;
constexpr std::uint32_t R052SetupStub=R052StubBlock+0x300u;
constexpr std::size_t R052TraceSlots=64u;
constexpr std::uint32_t R052TraceCount=R052StateBase+0x00u;
constexpr std::uint32_t R052SetupId=R052StateBase+0x04u;
constexpr std::uint32_t R052SetupFn=R052StateBase+0x08u;
constexpr std::uint32_t R052TraceCb=R052StateBase+0x100u;
constexpr std::uint32_t R052TraceWork=R052StateBase+0x200u;

void map_r052_event_scheduler(){
    auto* code=static_cast<std::uint8_t*>(map_at(R052StubBlock,4096,PROT_READ|PROT_WRITE));
    map_at(R052StateBase,4096,PROT_READ|PROT_WRITE);
    // Runtime records, split flags and current-event pointer all live in this span.
    if(mprotect(reinterpret_cast<void*>(0x799000u),0x8000u,PROT_READ|PROT_WRITE))
        throw std::runtime_error("r052 event-state mprotect");
    auto emit32=[](std::vector<std::uint8_t>& b,std::uint32_t v){for(unsigned k=0;k<4;++k)b.push_back(std::uint8_t(v>>(8u*k)));};
    for(std::uint32_t slot=0;slot<R052CallbackCount;++slot){
        const std::uint32_t token=R052StubBlock+slot*R052CallbackStride;
        std::vector<std::uint8_t>b;
        b.push_back(0x50); // push eax
        b.push_back(0x51); // push ecx
        b.push_back(0xa1);emit32(b,R052TraceCount); // eax=count
        b.insert(b.end(),{0xc7,0x04,0x85});emit32(b,R052TraceCb);emit32(b,token); // cb[count]=token
        b.insert(b.end(),{0x8b,0x4c,0x24,0x0c}); // ecx=[esp+12] original callback arg
        b.insert(b.end(),{0x89,0x0c,0x85});emit32(b,R052TraceWork); // work[count]=ecx
        b.push_back(0x40); // inc eax
        b.push_back(0xa3);emit32(b,R052TraceCount);
        b.push_back(0x59);b.push_back(0x58);b.push_back(0xc3);
        if(b.size()>R052CallbackStride)throw std::runtime_error("r052 callback stub too large");
        std::memcpy(code+slot*R052CallbackStride,b.data(),b.size());
    }
    // EventOpen provider boundary: log (event_id,function_id), do not mutate the slot.
    std::vector<std::uint8_t>b;
    b.push_back(0x50); // push eax
    b.insert(b.end(),{0x8b,0x44,0x24,0x08}); // eax=id after push
    b.push_back(0xa3);emit32(b,R052SetupId);
    b.insert(b.end(),{0x8b,0x44,0x24,0x0c}); // eax=function id
    b.push_back(0xa3);emit32(b,R052SetupFn);
    b.push_back(0x58);b.push_back(0xc3);
    std::memcpy(code+0x300u,b.data(),b.size());
    if(mprotect(code,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("r052 stub mprotect");
}
std::uint32_t r052_callback(std::uint32_t slot){return R052StubBlock+(slot%R052CallbackCount)*R052CallbackStride;}
void set_r052_event_open_patch(bool enabled){
    constexpr std::uint32_t site=0x440197u,expected=0x440110u;
    auto* p=reinterpret_cast<std::uint8_t*>(site);std::int32_t rel{};std::memcpy(&rel,p+1,4);
    const auto current=std::uint32_t(std::int64_t(site+5u)+rel);
    const auto from=enabled?expected:R052SetupStub;
    if(p[0]!=0xe8||current!=from)throw std::runtime_error("r052 EventOpen provider CALL target changed");
    auto* page=reinterpret_cast<void*>(site&~0xfffu);
    if(mprotect(page,4096,PROT_READ|PROT_WRITE|PROT_EXEC))throw std::runtime_error("r052 EventOpen patch protection");
    const auto target=enabled?R052SetupStub:expected;const std::uint32_t nr=target-(site+5u);std::memcpy(p+1,&nr,4);
    if(mprotect(page,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("r052 EventOpen patch restore protection");
}
void reset_r052_trace(){std::memset(reinterpret_cast<void*>(R052StateBase),0,4096);}
void store_r052_state(const PcEventControlState& state){
    std::memset(reinterpret_cast<void*>(R052RecordBase),0,PcEventSlotCount*0x3cu);
    std::memset(reinterpret_cast<void*>(R052FlagsBase),0,PcEventSlotCount);
    for(std::uint32_t id=0;id<PcEventSlotCount;++id){
        const auto& q=state.slots[id];Bytes r(reinterpret_cast<void*>(std::uintptr_t(R052RecordBase+id*0x3cu)),0x3cu);
        r.put32(0x00,q.descriptor_token);r.put32(0x04,q.event_id);r.put32(0x08,q.work_token);r.put32(0x0c,q.display_scene);
        r.put32(0x10,q.init_callback);r.put32(0x14,q.ctrl_callback);r.put32(0x18,q.disp_callback);
        r.put32(0x1c,q.shadow_callback);r.put32(0x20,q.dest_callback);r.put32(0x24,q.aux24);r.put32(0x28,q.aux28);
        r.put32(0x2c,q.close_guard);r.put32(0x30,q.function_id);r.put32(0x34,q.aux34);r.put32(0x38,q.aux38);
        *reinterpret_cast<std::uint8_t*>(R052FlagsBase+id)=q.flags;
    }
    if(state.current_slot<PcEventSlotCount)*reinterpret_cast<std::uint32_t*>(R052CurrentPtr)=R052RecordBase+state.current_slot*0x3cu;
    else *reinterpret_cast<std::uint32_t*>(R052CurrentPtr)=0u;
}
std::array<std::uint8_t,PcEventSlotCount*0x3cu> native_r052_records(const PcEventControlState& state){
    std::array<std::uint8_t,PcEventSlotCount*0x3cu> out{};
    for(std::uint32_t id=0;id<PcEventSlotCount;++id){
        const auto& q=state.slots[id];Bytes r(out.data()+id*0x3cu,0x3cu);
        r.put32(0x00,q.descriptor_token);r.put32(0x04,q.event_id);r.put32(0x08,q.work_token);r.put32(0x0c,q.display_scene);
        r.put32(0x10,q.init_callback);r.put32(0x14,q.ctrl_callback);r.put32(0x18,q.disp_callback);
        r.put32(0x1c,q.shadow_callback);r.put32(0x20,q.dest_callback);r.put32(0x24,q.aux24);r.put32(0x28,q.aux28);
        r.put32(0x2c,q.close_guard);r.put32(0x30,q.function_id);r.put32(0x34,q.aux34);r.put32(0x38,q.aux38);
    }
    return out;
}

std::array<std::uint8_t,PcEventSlotCount> native_r052_flags(const PcEventControlState& state){
    std::array<std::uint8_t,PcEventSlotCount> out{};for(std::size_t k=0;k<out.size();++k)out[k]=state.slots[k].flags;return out;
}
struct R052NativeTrace{
    std::uint32_t count{};std::uint32_t setup_id{},setup_fn{};
    std::array<std::uint32_t,R052TraceSlots> cb{},work{};
};
void r052_native_invoke(void* user,std::uint32_t cb,std::uint32_t work,std::uint32_t){
    auto* t=static_cast<R052NativeTrace*>(user);if(t->count<R052TraceSlots){t->cb[t->count]=cb;t->work[t->count]=work;}++t->count;
}
void r052_native_setup(void* user,PcEventControlState&,std::uint32_t id,std::uint32_t fn){auto* t=static_cast<R052NativeTrace*>(user);t->setup_id=id;t->setup_fn=fn;}
std::vector<std::uint8_t> r052_trace_blob(const R052NativeTrace& t){
    std::vector<std::uint8_t> out(0x300u);std::memcpy(out.data()+0x00,&t.count,4);std::memcpy(out.data()+0x04,&t.setup_id,4);std::memcpy(out.data()+0x08,&t.setup_fn,4);
    std::memcpy(out.data()+0x100,t.cb.data(),t.cb.size()*4u);std::memcpy(out.data()+0x200,t.work.data(),t.work.size()*4u);return out;
}
PcEventControlState r052_event_fixture(unsigned n){
    PcEventControlState st{};
    static constexpr std::uint8_t patterns[]={0u,1u,2u,4u,6u,5u,0x0au,0x12u,0x18u};
    for(std::uint32_t id=0;id<PcEventSlotCount;++id){auto& q=st.slots[id];q.flags=patterns[(id+n)%std::size(patterns)];q.event_id=0x1000u+id+(n&0xffu)*0x10000u;q.work_token=0x51000000u^(id*0x101u)^(n*0x10001u);q.close_guard=((id+n)%13u)==0u?1u:0u;}
    // Four explicit callback-bearing records; all other transitions still exercise the full 410-slot loop.
    st.slots[3].flags=4u;st.slots[3].dest_callback=r052_callback(0);st.slots[3].work_token=0x71000300u+n;
    st.slots[17].flags=1u;st.slots[17].init_callback=r052_callback(1);st.slots[17].ctrl_callback=r052_callback(2);st.slots[17].work_token=0x71001100u+n;
    st.slots[201].flags=2u;st.slots[201].ctrl_callback=r052_callback(3);st.slots[201].work_token=0x7100c900u+n;
    st.slots[409].flags=0x0au;st.slots[409].ctrl_callback=r052_callback(4);st.slots[409].work_token=0x71019900u+n;
    return st;
}
std::array<std::uint32_t,PcEventSlotCount> r052_default_tokens(){
    std::array<std::uint32_t,PcEventSlotCount> a{};for(std::uint32_t id=0;id<PcEventSlotCount;++id)a[id]=*reinterpret_cast<const std::uint32_t*>(0x59980cu+id*0x18u);return a;
}
std::array<PcEventInitDescriptor,PcEventSlotCount> r053_init_descriptors(){
    std::array<PcEventInitDescriptor,PcEventSlotCount> out{};
    for(std::uint32_t id=0;id<PcEventSlotCount;++id){
        const auto a=0x599808u+id*0x18u;auto& d=out[id];
        d.descriptor_token=*reinterpret_cast<const std::uint32_t*>(a+0x00u);
        d.work_token=*reinterpret_cast<const std::uint32_t*>(a+0x04u);
        d.aux28=*reinterpret_cast<const std::uint32_t*>(a+0x08u);
        d.startup=*reinterpret_cast<const std::uint32_t*>(a+0x0cu);
        d.function_id=*reinterpret_cast<const std::uint32_t*>(a+0x10u);
        d.display_scene=*reinterpret_cast<const std::uint32_t*>(a+0x14u);
    }
    return out;
}
std::array<PcEventFunctionDescriptor,PcEventFunctionTableCount> r053_function_descriptors(){
    std::array<PcEventFunctionDescriptor,PcEventFunctionTableCount> out{};
    for(std::uint32_t id=0;id<PcEventFunctionTableCount;++id){
        const auto a=0x59be78u+id*0x14u;auto& f=out[id];
        f.init_callback=*reinterpret_cast<const std::uint32_t*>(a+0x00u);
        f.ctrl_callback=*reinterpret_cast<const std::uint32_t*>(a+0x04u);
        f.disp_callback=*reinterpret_cast<const std::uint32_t*>(a+0x08u);
        f.shadow_callback=*reinterpret_cast<const std::uint32_t*>(a+0x0cu);
        f.dest_callback=*reinterpret_cast<const std::uint32_t*>(a+0x10u);
    }
    return out;
}

// r054: event pause and current-event work ownership boundaries.
constexpr std::uint32_t R054StubBlock=0x32019000u;
constexpr std::uint32_t R054StateBase=0x33074000u;
constexpr std::uint32_t R054AllocBase=0x3a000000u;
constexpr std::size_t R054AllocSize=0x20000u;
constexpr std::uint32_t R054TraceCount=R054StateBase+0x00u;
constexpr std::uint32_t R054AllocReturn=R054StateBase+0x04u;
constexpr std::uint32_t R054TracePc=R054StateBase+0x100u;
constexpr std::uint32_t R054TraceArg=R054StateBase+0x200u;
constexpr std::size_t R054TraceSlots=64u;

void map_r054_event_work(){
    auto* code=static_cast<std::uint8_t*>(map_at(R054StubBlock,4096,PROT_READ|PROT_WRITE));
    map_at(R054StateBase,4096,PROT_READ|PROT_WRITE);
    map_at(R054AllocBase,R054AllocSize,PROT_READ|PROT_WRITE);
    auto emit32=[](std::vector<std::uint8_t>& b,std::uint32_t v){for(unsigned k=0;k<4;++k)b.push_back(std::uint8_t(v>>(8u*k)));};
    auto emit_arg_stub=[&](std::uint32_t off,std::uint32_t pc,bool has_arg,bool returns_alloc){
        std::vector<std::uint8_t>b;
        if(returns_alloc){
            b.push_back(0x51); // push ecx
            b.push_back(0xa1);emit32(b,R054TraceCount); // eax=count
            b.insert(b.end(),{0xc7,0x04,0x85});emit32(b,R054TracePc);emit32(b,pc);
            b.insert(b.end(),{0x8b,0x4c,0x24,0x08}); // ecx=original arg0
            b.insert(b.end(),{0x89,0x0c,0x85});emit32(b,R054TraceArg);
            b.push_back(0x40);b.push_back(0xa3);emit32(b,R054TraceCount);
            b.push_back(0x59); // pop ecx
            b.push_back(0xa1);emit32(b,R054AllocReturn); // eax=deterministic allocation
            b.push_back(0xc3);
        }else{
            b.push_back(0x50);b.push_back(0x51); // save eax/ecx
            b.push_back(0xa1);emit32(b,R054TraceCount);
            b.insert(b.end(),{0xc7,0x04,0x85});emit32(b,R054TracePc);emit32(b,pc);
            if(has_arg){b.insert(b.end(),{0x8b,0x4c,0x24,0x0c});b.insert(b.end(),{0x89,0x0c,0x85});emit32(b,R054TraceArg);}
            else{b.insert(b.end(),{0xc7,0x04,0x85});emit32(b,R054TraceArg);emit32(b,0u);}
            b.push_back(0x40);b.push_back(0xa3);emit32(b,R054TraceCount);
            b.push_back(0x59);b.push_back(0x58);b.push_back(0xc3);
        }
        if(b.size()>0x50u)throw std::runtime_error("r054 trace stub too large");
        std::memcpy(code+off,b.data(),b.size());
    };
    // Distinct stubs encode the original callee in the trace.
    emit_arg_stub(0x000u,0x43f9d0u,true,false);
    emit_arg_stub(0x050u,0x429810u,true,false);
    emit_arg_stub(0x0a0u,0x449040u,true,false);
    emit_arg_stub(0x0f0u,0x440d90u,true,false);
    emit_arg_stub(0x140u,0x440d10u,true,false);
    emit_arg_stub(0x190u,0x440d50u,true,false);
    emit_arg_stub(0x1e0u,0x580253u,true,true);
    emit_arg_stub(0x230u,0x440d30u,false,false);
    emit_arg_stub(0x280u,0x440d70u,false,false);
    emit_arg_stub(0x2d0u,0x49a650u,true,false);
    emit_arg_stub(0x320u,0x580bc2u,true,false);
    // Protected SetEvPauseFlag continuation: ecx must address slot1.work (+0x08).
    {
        std::vector<std::uint8_t>b{0xb9};emit32(b,R052RecordBase+0x3cu+0x08u);b.push_back(0xe9);
        const auto from=R054StubBlock+0x380u+std::uint32_t(b.size())+4u;const auto rel=0x44094fu-from;emit32(b,rel);
        std::memcpy(code+0x380u,b.data(),b.size());
    }
    if(mprotect(code,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("r054 stub mprotect");

    // Pause service boundaries and protected continuation.
    r046_write_rel32(0x440932u,0xe8,R054StubBlock+0x000u);
    r046_write_rel32(0x440939u,0xe8,R054StubBlock+0x050u);
    r046_write_rel32(0x440940u,0xe8,R054StubBlock+0x0a0u);
    r046_write_rel32(0x44094au,0xe9,R054StubBlock+0x380u);
    r046_write_rel32(0x4409dbu,0xe8,R054StubBlock+0x050u);
    r046_write_rel32(0x4409e2u,0xe8,R054StubBlock+0x000u);
    r046_write_rel32(0x4409e9u,0xe8,R054StubBlock+0x0a0u);
    // MallocNowEventWork child boundaries.
    r046_write_rel32(0x440a6au,0xe8,R054StubBlock+0x0f0u);
    r046_write_rel32(0x440a86u,0xe8,R054StubBlock+0x140u);
    r046_write_rel32(0x440ab2u,0xe8,R054StubBlock+0x190u);
    r046_write_rel32(0x440ac2u,0xe8,R054StubBlock+0x1e0u);
    r046_write_rel32(0x440ad7u,0xe8,R054StubBlock+0x230u);
    r046_write_rel32(0x440adcu,0xe8,R054StubBlock+0x280u);
    r046_write_rel32(0x440aebu,0xe8,R054StubBlock+0x2d0u);
    r046_write_rel32(0x440af9u,0xe8,R054StubBlock+0x2d0u);
    r046_write_rel32(0x440b00u,0xe8,R054StubBlock+0x0f0u);
    // Ownership helper boundaries used both directly and through FreeNowEventWork.
    r046_write_rel32(0x440cefu,0xe8,R054StubBlock+0x140u);
    r046_write_rel32(0x440cfau,0xe8,R054StubBlock+0x320u);
    r046_write_rel32(0x440d02u,0xe8,R054StubBlock+0x230u);
}
void reset_r054_trace(std::uint32_t alloc_return=R054AllocBase){
    std::memset(reinterpret_cast<void*>(R054StateBase),0,4096);
    *reinterpret_cast<std::uint32_t*>(R054AllocReturn)=alloc_return;
}
struct R054NativeTrace{
    std::uint32_t count{};
    std::array<std::uint32_t,R054TraceSlots> pc{},arg{};
    std::uint32_t alloc_return{R054AllocBase};
};
void r054_native_log(R054NativeTrace& t,std::uint32_t pc,std::uint32_t arg){if(t.count<R054TraceSlots){t.pc[t.count]=pc;t.arg[t.count]=arg;}++t.count;}
void r054_native_boundary(void* u,std::uint32_t pc,std::uint32_t arg){r054_native_log(*static_cast<R054NativeTrace*>(u),pc,arg);}
std::uint32_t r054_native_alloc(void* u,std::uint32_t bytes){auto& t=*static_cast<R054NativeTrace*>(u);r054_native_log(t,0x580253u,bytes);return t.alloc_return;}
void r054_native_release(void* u,std::uint32_t base){r054_native_log(*static_cast<R054NativeTrace*>(u),0x580bc2u,base);}
std::vector<std::uint8_t> r054_trace_blob(const R054NativeTrace& t){
    std::vector<std::uint8_t> out(0x300u);std::memcpy(out.data()+0x00,&t.count,4);std::memcpy(out.data()+0x04,&t.alloc_return,4);
    std::memcpy(out.data()+0x100,t.pc.data(),t.pc.size()*4u);std::memcpy(out.data()+0x200,t.arg.data(),t.arg.size()*4u);return out;
}
PcEventWorkHandle r054_handle_from_guest(std::uint32_t wrapper){
    PcEventWorkHandle h{};if(wrapper==0u)return h;h.wrapper_token=wrapper;h.base_token=*reinterpret_cast<const std::uint32_t*>(wrapper);h.heap_type=*reinterpret_cast<const std::uint32_t*>(wrapper+4u);h.live=true;return h;
}

// r055: allocator state stacks + two compact ownership/service leaves.
constexpr std::uint32_t R055StubBlock=0x3201a000u;
constexpr std::uint32_t R055StateBase=0x33075000u;
constexpr std::uint32_t R055StackB=0x7b1194u;
constexpr std::uint32_t R055StackA=0x7b11a8u;
constexpr std::uint32_t R055DepthA=0x7b11bcu;
constexpr std::uint32_t R055DepthB=0x7b11c0u;
void map_r055_allocator_state(){
    auto* code=static_cast<std::uint8_t*>(map_at(R055StubBlock,4096,PROT_READ|PROT_WRITE));
    map_at(R055StateBase,4096,PROT_READ|PROT_WRITE);
    if(mprotect(reinterpret_cast<void*>(0x7b1000u),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("r055 globals mprotect");
    auto emit32=[](std::vector<std::uint8_t>& b,std::uint32_t v){for(unsigned k=0;k<4;++k)b.push_back(std::uint8_t(v>>(8u*k)));};
    // 43FB40 replacement: capture the three arguments supplied by 440CA0.
    std::vector<std::uint8_t>b;
    b.insert(b.end(),{0xa1});emit32(b,R055StateBase+0x00u); // eax=count
    b.insert(b.end(),{0x8b,0x4c,0x24,0x04});b.insert(b.end(),{0x89,0x0d});emit32(b,R055StateBase+0x04u);
    b.insert(b.end(),{0x8b,0x4c,0x24,0x08});b.insert(b.end(),{0x89,0x0d});emit32(b,R055StateBase+0x08u);
    b.insert(b.end(),{0x8b,0x4c,0x24,0x0c});b.insert(b.end(),{0x89,0x0d});emit32(b,R055StateBase+0x0cu);
    b.push_back(0x40);b.push_back(0xa3);emit32(b,R055StateBase+0x00u);
    b.push_back(0xb8);emit32(b,0x55aa55aau);b.push_back(0xc3);
    std::memcpy(code,b.data(),b.size());
    if(mprotect(code,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("r055 stub mprotect");
    // Replace only the child service call in 440CA0.
    r046_write_rel32(0x440cb3u,0xe8,R055StubBlock);
    // 440D50 enters through a protection-only loader that resolves the depth.
    // Replace its five-byte JMP with the semantic equivalent MOV EAX,[depthB],
    // leaving the original body at 440D55 untouched.
    auto* q=reinterpret_cast<std::uint8_t*>(0x440d50u);
    auto* page=reinterpret_cast<void*>(0x440000u);
    if(mprotect(page,0x1000u,PROT_READ|PROT_WRITE|PROT_EXEC))throw std::runtime_error("r055 d50 patch protection");
    const std::uint8_t repl[5]={0xa1,0xc0,0x11,0x7b,0x00};std::memcpy(q,repl,5);
    if(mprotect(page,0x1000u,PROT_READ|PROT_EXEC))throw std::runtime_error("r055 d50 restore protection");
}
void store_r055_allocator_state(const PcAllocatorStateStacks& st){
    std::memcpy(reinterpret_cast<void*>(R055StackA),st.stack_a.data(),st.stack_a.size()*4u);
    std::memcpy(reinterpret_cast<void*>(R055StackB),st.stack_b.data(),st.stack_b.size()*4u);
    *reinterpret_cast<std::uint32_t*>(R055DepthA)=st.depth_a;*reinterpret_cast<std::uint32_t*>(R055DepthB)=st.depth_b;
}
std::array<std::uint8_t,0x30> r055_allocator_blob(){
    std::array<std::uint8_t,0x30> out{};std::memcpy(out.data(),reinterpret_cast<void*>(R055StackB),out.size());return out;
}
struct R055MaskTrace{std::uint32_t count{},mask{},zero{},id{};};
void r055_mask_cb(void* u,std::uint32_t mask,std::uint32_t zero,std::uint32_t id){auto& t=*static_cast<R055MaskTrace*>(u);++t.count;t.mask=mask;t.zero=zero;t.id=id;}
std::array<std::uint8_t,16> r055_mask_blob(const R055MaskTrace& t){std::array<std::uint8_t,16> o{};std::memcpy(o.data()+0,&t.count,4);std::memcpy(o.data()+4,&t.mask,4);std::memcpy(o.data()+8,&t.zero,4);std::memcpy(o.data()+12,&t.id,4);return o;}

// r056: compact object/state leaves and child-tail wrappers.
constexpr std::uint32_t R056StubBlock=0x3201b000u;
constexpr std::uint32_t R056StateBase=0x33076000u;
constexpr std::uint32_t R056ObjectBase=0x3b000000u;
constexpr std::size_t R056ObjectSize=0x1000u;
void r056_emit_u32(std::vector<std::uint8_t>& b,std::uint32_t v){for(unsigned k=0;k<4;++k)b.push_back(std::uint8_t(v>>(8u*k)));}
void r056_emit_void_stub(std::uint8_t* dst,std::uint32_t pc){
    std::vector<std::uint8_t>b;
    b.push_back(0xa1);r056_emit_u32(b,R056StateBase+0x00u);b.push_back(0x40);b.push_back(0xa3);r056_emit_u32(b,R056StateBase+0x00u);
    b.insert(b.end(),{0xc7,0x05});r056_emit_u32(b,R056StateBase+0x04u);r056_emit_u32(b,pc);
    b.insert(b.end(),{0x89,0x0d});r056_emit_u32(b,R056StateBase+0x08u);b.push_back(0xc3);std::memcpy(dst,b.data(),b.size());
}
void r056_emit_bool2_stub(std::uint8_t* dst,std::uint32_t pc){
    std::vector<std::uint8_t>b;
    b.push_back(0xa1);r056_emit_u32(b,R056StateBase+0x00u);b.push_back(0x40);b.push_back(0xa3);r056_emit_u32(b,R056StateBase+0x00u);
    b.insert(b.end(),{0xc7,0x05});r056_emit_u32(b,R056StateBase+0x04u);r056_emit_u32(b,pc);
    b.insert(b.end(),{0x89,0x0d});r056_emit_u32(b,R056StateBase+0x08u);
    b.insert(b.end(),{0x8b,0x44,0x24,0x04,0xa3});r056_emit_u32(b,R056StateBase+0x0cu);
    b.insert(b.end(),{0x8b,0x44,0x24,0x08,0xa3});r056_emit_u32(b,R056StateBase+0x10u);
    b.push_back(0xa1);r056_emit_u32(b,R056StateBase+0x14u);b.insert(b.end(),{0xc2,0x08,0x00});std::memcpy(dst,b.data(),b.size());
}
void set_r056_object_patches(bool enabled){
    struct P{std::uint32_t site,original,stub;};
    constexpr P p[]={{0x440e50u,0x446fc0u,R056StubBlock+0x000u},{0x440e90u,0x4464f0u,R056StubBlock+0x080u},
                     {0x440ea6u,0x446d90u,R056StubBlock+0x100u},{0x440eb6u,0x4464b0u,R056StubBlock+0x180u},
                     {0x440ec6u,0x4464d0u,R056StubBlock+0x200u},{0x440edfu,0x446f30u,R056StubBlock+0x280u}};
    for(const auto& x:p)r046_write_rel32(x.site,0xe9,enabled?x.stub:x.original);
}
bool r056_oracle_name(const std::string& q){
    return q=="object_take_token_440dc0_r056"||q=="object_set_token_440de0_r056"||q=="object_set_field4_440df0_r056"||
           q=="object_set_field8_440e00_r056"||q=="object_push_byte_state_440e10_r056"||q=="object_pop_byte_state_440e60_r056"||
           q=="object_child_call_a_440ea0_r056"||q=="object_child_call_b_440eb0_r056"||q=="object_child_call_c_440ec0_r056"||
           q=="object_child_conditional_440ed0_r056"||q=="r056_object_state_batch";
}
void map_r056_object_helpers(){
    auto* code=static_cast<std::uint8_t*>(map_at(R056StubBlock,4096,PROT_READ|PROT_WRITE));
    map_at(R056StateBase,4096,PROT_READ|PROT_WRITE);map_at(R056ObjectBase,R056ObjectSize,PROT_READ|PROT_WRITE);
    r056_emit_void_stub(code+0x000u,0x446fc0u);r056_emit_void_stub(code+0x080u,0x4464f0u);r056_emit_void_stub(code+0x100u,0x446d90u);
    r056_emit_void_stub(code+0x180u,0x4464b0u);r056_emit_void_stub(code+0x200u,0x4464d0u);r056_emit_bool2_stub(code+0x280u,0x446f30u);
    if(mprotect(code,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("r056 stub mprotect");
}
void reset_r056_trace(std::uint32_t ret=0x5au){std::memset(reinterpret_cast<void*>(R056StateBase),0,0x40);*reinterpret_cast<std::uint32_t*>(R056StateBase+0x14u)=ret;}
struct R056NativeTrace{std::uint32_t count{},pc{},child_offset{},arg0{},arg1{},ret{0x5au};};
void r056_native_void(void* u,std::uint32_t pc,std::uint32_t child){auto& t=*static_cast<R056NativeTrace*>(u);++t.count;t.pc=pc;t.child_offset=child;}
std::uint8_t r056_native_bool2(void* u,std::uint32_t pc,std::uint32_t child,std::uint32_t a,std::uint32_t b){auto& t=*static_cast<R056NativeTrace*>(u);++t.count;t.pc=pc;t.child_offset=child;t.arg0=a;t.arg1=b;return std::uint8_t(t.ret);}
std::array<std::uint8_t,24> r056_native_trace_blob(const R056NativeTrace& t){std::array<std::uint8_t,24> o{};std::memcpy(o.data()+0,&t.count,4);std::memcpy(o.data()+4,&t.pc,4);std::memcpy(o.data()+8,&t.child_offset,4);std::memcpy(o.data()+12,&t.arg0,4);std::memcpy(o.data()+16,&t.arg1,4);std::memcpy(o.data()+20,&t.ret,4);return o;}
std::array<std::uint8_t,24> r056_guest_trace_blob(){std::array<std::uint8_t,24> o{};auto* x=reinterpret_cast<const std::uint32_t*>(R056StateBase);std::uint32_t child=x[2]?x[2]-R056ObjectBase:0u;std::memcpy(o.data()+0,&x[0],4);std::memcpy(o.data()+4,&x[1],4);std::memcpy(o.data()+8,&child,4);std::memcpy(o.data()+12,&x[3],4);std::memcpy(o.data()+16,&x[4],4);std::memcpy(o.data()+20,&x[5],4);return o;}
std::array<std::uint8_t,0x600> r056_object_fixture(unsigned n){std::array<std::uint8_t,0x600> o{};std::mt19937 g(0x440dc0u^(n*2654435761u));for(auto& b:o)b=std::uint8_t(g());return o;}

// r057: float-transition controller and its simple global-state leaves.
constexpr std::uint32_t R057StubBlock=0x3201c000u;
constexpr std::uint32_t R057StateBase=0x33077000u;
constexpr std::uint32_t R057ObjectBase=0x3c000000u;
constexpr std::size_t R057ObjectSize=0x1000u;
constexpr std::uint32_t R057GlobalToken=0x68a670u;
constexpr std::uint32_t R057GlobalPrimary=0x68a66cu;
constexpr std::uint32_t R057GlobalSecondary=0x68a674u;
constexpr std::uint32_t R057GlobalActive=0x84a9fdu;
void map_r057_transition(){
    auto* code=static_cast<std::uint8_t*>(map_at(R057StubBlock,4096,PROT_READ|PROT_WRITE));
    map_at(R057StateBase,4096,PROT_READ|PROT_WRITE);map_at(R057ObjectBase,R057ObjectSize,PROT_READ|PROT_WRITE);
    if(mprotect(reinterpret_cast<void*>(0x68a000u),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("r057 globals mprotect A");
    if(mprotect(reinterpret_cast<void*>(0x84a000u),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("r057 globals mprotect B");
    // External activation boundary used only by the 441020 tail path: increment trace and return.
    std::vector<std::uint8_t>b;
    b.push_back(0xa1);for(unsigned k=0;k<4;++k)b.push_back(std::uint8_t((R057StateBase+0u)>>(8u*k)));
    b.push_back(0x40);b.push_back(0xa3);for(unsigned k=0;k<4;++k)b.push_back(std::uint8_t((R057StateBase+0u)>>(8u*k)));
    b.push_back(0xc3);std::memcpy(code,b.data(),b.size());
    if(mprotect(code,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("r057 stub mprotect");
}
void set_r057_transition_patch(bool enabled){r046_write_rel32(0x44111au,0xe9,enabled?R057StubBlock:0x4c50d0u);}
bool r057_oracle_name(const std::string& q){
    return q=="transition_global_get_active_4c50c0_r057"||q=="transition_global_set_token_4c50e0_r057"||
           q=="transition_global_set_primary_4c50f0_r057"||q=="transition_global_set_secondary_4c5100_r057"||
           q=="transition_global_clear_active_4c5110_r057"||q=="object_transition_init_440ef0_r057"||
           q=="object_transition_config_440f70_r057"||q=="object_transition_update_441020_r057"||
           q=="object_transition_latch_441130_r057"||q=="r057_transition_batch";
}
std::array<std::uint8_t,0x600> r057_object_fixture(unsigned n){
    std::array<std::uint8_t,0x600> o{};std::mt19937 g(0x440ef057u^(n*2654435761u));for(auto& b:o)b=std::uint8_t(g());return o;
}
PcFloatTransitionGlobals r057_guest_globals(){
    PcFloatTransitionGlobals g{};g.token=*reinterpret_cast<const std::uint32_t*>(R057GlobalToken);
    g.primary=*reinterpret_cast<const float*>(R057GlobalPrimary);g.secondary=*reinterpret_cast<const float*>(R057GlobalSecondary);
    g.active=*reinterpret_cast<const std::uint8_t*>(R057GlobalActive);return g;
}
void r057_store_globals(const PcFloatTransitionGlobals& g){
    *reinterpret_cast<std::uint32_t*>(R057GlobalToken)=g.token;*reinterpret_cast<float*>(R057GlobalPrimary)=g.primary;
    *reinterpret_cast<float*>(R057GlobalSecondary)=g.secondary;*reinterpret_cast<std::uint8_t*>(R057GlobalActive)=g.active;
}
std::array<std::uint8_t,16> r057_globals_blob(const PcFloatTransitionGlobals& g){
    std::array<std::uint8_t,16> o{};std::memcpy(o.data()+0,&g.token,4);std::memcpy(o.data()+4,&g.primary,4);
    std::memcpy(o.data()+8,&g.secondary,4);o[12]=g.active;return o;
}
std::array<std::uint8_t,16> r057_guest_globals_blob(){return r057_globals_blob(r057_guest_globals());}
struct R057Trace{std::uint32_t activate{};};
void r057_activate(void* u){++static_cast<R057Trace*>(u)->activate;}


// r058: compact runtime/object lifecycle at 0x4411A0..0x441366 plus six leaves.
constexpr std::uint32_t R058StubBlock=0x3201d000u;
constexpr std::uint32_t R058StateBase=0x33079000u;
constexpr std::uint32_t R058ObjectBase=0x3d000000u;
constexpr std::uint32_t R058ManagerBase=0x3d002000u;
constexpr std::uint32_t R058ChildBase=0x3d004000u;
constexpr std::uint32_t R058VtableBase=0x3d006000u;
constexpr std::size_t R058MapSize=0x10000u;
void r058_emit32(std::vector<std::uint8_t>& b,std::uint32_t v){for(unsigned k=0;k<4;++k)b.push_back(std::uint8_t(v>>(8u*k)));}
void r058_emit_inc_abs(std::vector<std::uint8_t>& b,std::uint32_t a){b.insert(b.end(),{0xff,0x05});r058_emit32(b,a);}
void r058_emit_store_ecx(std::vector<std::uint8_t>& b,std::uint32_t a){b.insert(b.end(),{0x89,0x0d});r058_emit32(b,a);}
void r058_emit_store_eax(std::vector<std::uint8_t>& b,std::uint32_t a){b.push_back(0xa3);r058_emit32(b,a);}
void map_r058_runtime(){
    auto* code=static_cast<std::uint8_t*>(map_at(R058StubBlock,4096,PROT_READ|PROT_WRITE));
    map_at(R058StateBase,4096,PROT_READ|PROT_WRITE);map_at(R058ObjectBase,R058MapSize,PROT_READ|PROT_WRITE);
    if(mprotect(reinterpret_cast<void*>(0x659000u),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("r058 reset-word page mprotect");
    if(mprotect(reinterpret_cast<void*>(0x836000u),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("r058 mode page mprotect");
    auto emit_simple=[&](std::size_t off,std::uint32_t count,std::uint32_t pc,bool save_ecx,bool ret4){
        std::vector<std::uint8_t>b;r058_emit_inc_abs(b,R058StateBase+count);
        {b.insert(b.end(),{0xc7,0x05});r058_emit32(b,R058StateBase+0x60u+count);r058_emit32(b,pc);}
        if(save_ecx)r058_emit_store_ecx(b,R058StateBase+0x80u+count);
        if(ret4)b.insert(b.end(),{0xc2,0x04,0x00});else b.push_back(0xc3);std::memcpy(code+off,b.data(),b.size());
    };
    emit_simple(0x000u,0x00u,0x453c40u,true,false); // cleanup
    {std::vector<std::uint8_t>b;r058_emit_inc_abs(b,R058StateBase+0x04u);r058_emit_store_ecx(b,R058StateBase+0x84u);b.insert(b.end(),{0x8b,0x44,0x24,0x04});r058_emit_store_eax(b,R058StateBase+0xa4u);b.insert(b.end(),{0xc2,0x04,0x00});std::memcpy(code+0x080u,b.data(),b.size());}
    emit_simple(0x100u,0x08u,0x4411e0u,true,false); // virtual +0x18
    {std::vector<std::uint8_t>b;r058_emit_inc_abs(b,R058StateBase+0x0cu);b.push_back(0xa0);r058_emit32(b,R058StateBase+0x20u);b.push_back(0xc3);std::memcpy(code+0x180u,b.data(),b.size());}
    {std::vector<std::uint8_t>b;r058_emit_inc_abs(b,R058StateBase+0x10u);r058_emit_store_ecx(b,R058StateBase+0x90u);r058_emit_store_eax(b,R058StateBase+0x94u);b.push_back(0xa0);r058_emit32(b,R058StateBase+0x24u);b.push_back(0xc3);std::memcpy(code+0x200u,b.data(),b.size());}
    emit_simple(0x280u,0x14u,0x441210u,true,false); // target vtbl +0x10
    {std::vector<std::uint8_t>b;r058_emit_inc_abs(b,R058StateBase+0x18u);r058_emit_store_ecx(b,R058StateBase+0x98u);b.insert(b.end(),{0x8b,0x44,0x24,0x04});r058_emit_store_eax(b,R058StateBase+0x9cu);b.insert(b.end(),{0xc2,0x04,0x00});std::memcpy(code+0x300u,b.data(),b.size());}
    {std::vector<std::uint8_t>b;r058_emit_inc_abs(b,R058StateBase+0x1cu);b.push_back(0xa0);r058_emit32(b,R058StateBase+0x28u);b.push_back(0xe9);const std::uint32_t site=R058StubBlock+0x380u+std::uint32_t(b.size());const std::uint32_t rel=0x441221u-(site+4u);r058_emit32(b,rel);std::memcpy(code+0x380u,b.data(),b.size());}
    *reinterpret_cast<std::uint32_t*>(R058VtableBase+0x00u)=R058StubBlock+0x300u;
    *reinterpret_cast<std::uint32_t*>(R058VtableBase+0x10u)=R058StubBlock+0x280u;
    *reinterpret_cast<std::uint32_t*>(R058VtableBase+0x18u)=R058StubBlock+0x100u;
    if(mprotect(code,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("r058 stub mprotect");
}
void set_r058_runtime_patches(bool enabled){
    r046_write_rel32(0x4411bcu,0xe8,enabled?R058StubBlock+0x000u:0x453c40u);
    r046_write_rel32(0x4411c8u,0xe8,enabled?R058StubBlock+0x080u:0x490530u);
    r046_write_rel32(0x441287u,0xe8,enabled?R058StubBlock+0x200u:0x453ca0u);
    r046_write_rel32(0x44129bu,0xe8,enabled?R058StubBlock+0x200u:0x453ca0u);
    r046_write_rel32(0x45424cu,0xe9,enabled?R058StubBlock+0x180u:0x4946e0u);
    constexpr std::uint32_t site=0x44121bu;auto* page=reinterpret_cast<void*>(site&~0xfffu);
    if(mprotect(page,4096,PROT_READ|PROT_WRITE|PROT_EXEC))throw std::runtime_error("r058 protected-jump patch protection");
    auto* q=reinterpret_cast<std::uint8_t*>(site);
    if(enabled){q[0]=0xe9;const std::uint32_t rel=(R058StubBlock+0x380u)-(site+5u);std::memcpy(q+1,&rel,4);q[5]=0x90;}
    else{const std::uint8_t original[6]={0xff,0x25,0xa0,0x9b,0x03,0x01};std::memcpy(q,original,6);}
    if(mprotect(page,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("r058 protected-jump restore protection");
}
bool r058_oracle_name(const std::string& q){
    return q=="runtime_shutdown_4411a0_r058"||q=="runtime_release_handle_441200_r058"||q=="runtime_ready_441260_r058"||
           q=="runtime_status_4412c0_r058"||q=="runtime_has_handle_4412f0_r058"||q=="runtime_close_if_status_441300_r058"||
           q=="runtime_manager_reset_454200_r058"||q=="runtime_set_gate_454220_r058"||q=="runtime_get_gate_454230_r058"||
           q=="runtime_finish_454240_r058"||q=="runtime_child_counter_dec_4464f0_r058"||q=="runtime_child_state_564c90_r058"||
           q=="r058_runtime_batch";
}
void reset_r058_trace(std::uint8_t finish_ret=0u,std::uint8_t pair_ret=0u,std::int8_t release_index=0){
    std::memset(reinterpret_cast<void*>(R058StateBase),0,0x200);*reinterpret_cast<std::uint8_t*>(R058StateBase+0x20u)=finish_ret;
    *reinterpret_cast<std::uint8_t*>(R058StateBase+0x24u)=pair_ret;*reinterpret_cast<std::uint8_t*>(R058StateBase+0x28u)=std::uint8_t(release_index);
}
PcRuntimeControlGlobals r058_guest_globals(){
    PcRuntimeControlGlobals g{};g.enabled_d2=*reinterpret_cast<std::uint8_t*>(0x7d68d2u);g.blocked_bf=*reinterpret_cast<std::uint8_t*>(0x7d68bfu);
    g.gate_d4=*reinterpret_cast<std::uint8_t*>(0x7d68d4u);g.started_d1=*reinterpret_cast<std::uint8_t*>(0x7d68d1u);
    g.current_ac=*reinterpret_cast<std::uint32_t*>(0x7d68acu);g.alternate_b0=*reinterpret_cast<std::uint32_t*>(0x7d68b0u);
    g.mode_836130=*reinterpret_cast<std::uint32_t*>(0x836130u);g.reset_word_659930=*reinterpret_cast<std::uint32_t*>(0x659930u);return g;
}
void r058_store_globals(const PcRuntimeControlGlobals& g){
    *reinterpret_cast<std::uint8_t*>(0x7d68d2u)=g.enabled_d2;*reinterpret_cast<std::uint8_t*>(0x7d68bfu)=g.blocked_bf;
    *reinterpret_cast<std::uint8_t*>(0x7d68d4u)=g.gate_d4;*reinterpret_cast<std::uint8_t*>(0x7d68d1u)=g.started_d1;
    *reinterpret_cast<std::uint32_t*>(0x7d68acu)=g.current_ac;*reinterpret_cast<std::uint32_t*>(0x7d68b0u)=g.alternate_b0;
    *reinterpret_cast<std::uint32_t*>(0x836130u)=g.mode_836130;*reinterpret_cast<std::uint32_t*>(0x659930u)=g.reset_word_659930;
}
std::array<std::uint8_t,20> r058_globals_blob(const PcRuntimeControlGlobals& g){
    std::array<std::uint8_t,20> o{};o[0]=g.enabled_d2;o[1]=g.blocked_bf;o[2]=g.gate_d4;o[3]=g.started_d1;
    std::memcpy(o.data()+4,&g.current_ac,4);std::memcpy(o.data()+8,&g.alternate_b0,4);std::memcpy(o.data()+12,&g.mode_836130,4);std::memcpy(o.data()+16,&g.reset_word_659930,4);return o;
}
std::array<std::uint8_t,20> r058_guest_globals_blob(){return r058_globals_blob(r058_guest_globals());}
struct R058NativeTrace{std::uint32_t cleanup{},manager{},virt18{},finish{},pair{},pre{},arg{},protect{};std::uint8_t finish_ret{},pair_ret{};std::int8_t release_index{};std::uint32_t child_state{};};
void r058_native_void(void* u,std::uint32_t pc,std::uint32_t){auto& t=*static_cast<R058NativeTrace*>(u);if(pc==0x453c40u)++t.cleanup;else if(pc==0x4411e0u)++t.virt18;else if(pc==0x441210u)++t.pre;}
void r058_native_void_arg(void* u,std::uint32_t pc,std::uint32_t,std::uint32_t){auto& t=*static_cast<R058NativeTrace*>(u);if(pc==0x490530u)++t.manager;else if(pc==0x441219u)++t.arg;}
std::uint8_t r058_native_u8(void* u,std::uint32_t pc,std::uint32_t){auto& t=*static_cast<R058NativeTrace*>(u);if(pc==0x4946e0u){++t.finish;return t.finish_ret;}return 0u;}
std::uint32_t r058_native_u32(void* u,std::uint32_t pc,std::uint32_t){auto& t=*static_cast<R058NativeTrace*>(u);return pc==0x564c90u?t.child_state:0u;}
std::uint8_t r058_native_pair(void* u,std::uint32_t,std::uint32_t,std::uint32_t){auto& t=*static_cast<R058NativeTrace*>(u);++t.pair;return t.pair_ret;}
std::int8_t r058_native_i8(void* u,std::uint32_t,std::uint32_t){auto& t=*static_cast<R058NativeTrace*>(u);++t.protect;return t.release_index;}
PcRuntimeControlServices r058_services(R058NativeTrace& t){return {&t,r058_native_void,r058_native_void_arg,r058_native_u8,r058_native_u32,r058_native_pair,r058_native_i8};}
std::array<std::uint8_t,32> r058_trace_blob(const R058NativeTrace& t){std::array<std::uint8_t,32> o{};const std::uint32_t v[8]={t.cleanup,t.manager,t.virt18,t.finish,t.pair,t.pre,t.arg,t.protect};std::memcpy(o.data(),v,sizeof(v));return o;}
std::array<std::uint8_t,32> r058_guest_trace_blob(){std::array<std::uint8_t,32> o{};std::memcpy(o.data(),reinterpret_cast<void*>(R058StateBase),32);return o;}
std::array<std::uint8_t,0x1000> r058_object_fixture(unsigned n){std::array<std::uint8_t,0x1000> o{};std::mt19937 g(0x4411a058u^(n*2654435761u));for(auto& b:o)b=std::uint8_t(g());return o;}
// r059: compact UI/resource controller at 0x441370..0x441442 plus directly consumed leaves.
constexpr std::uint32_t R059StubBlock=0x32022000u;
constexpr std::uint32_t R059StateBase=0x33083000u;
constexpr std::uint32_t R059ObjectBase=0x3e000000u;
constexpr std::uint32_t R059TableBase=0x3e002000u;
constexpr std::size_t R059ObjectSize=0x1000u;
constexpr std::size_t R059TableSize=0x2000u;
void r059_emit32(std::vector<std::uint8_t>& b,std::uint32_t v){for(unsigned k=0;k<4;++k)b.push_back(std::uint8_t(v>>(8u*k)));}
void r059_inc(std::vector<std::uint8_t>& b,std::uint32_t a){b.insert(b.end(),{0xff,0x05});r059_emit32(b,a);}
void r059_store_eax(std::vector<std::uint8_t>& b,std::uint32_t a){b.push_back(0xa3);r059_emit32(b,a);}
void map_r059_ui(){
    auto* code=static_cast<std::uint8_t*>(map_at(R059StubBlock,4096,PROT_READ|PROT_WRITE));
    map_at(R059StateBase,4096,PROT_READ|PROT_WRITE);map_at(R059ObjectBase,R059ObjectSize,PROT_READ|PROT_WRITE);map_at(R059TableBase,R059TableSize,PROT_READ|PROT_WRITE);
    for(const std::uint32_t page:{0x631000u,0x956000u})if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("r059 data page mprotect");
    {std::vector<std::uint8_t>b;r059_inc(b,R059StateBase+0x00u);b.insert(b.end(),{0x8b,0x44,0x24,0x04});r059_store_eax(b,R059StateBase+0x04u);b.push_back(0xc3);std::memcpy(code+0x000,b.data(),b.size());}
    {std::vector<std::uint8_t>b;r059_inc(b,R059StateBase+0x08u);b.insert(b.end(),{0x8b,0x44,0x24,0x04});r059_store_eax(b,R059StateBase+0x0cu);b.push_back(0xa1);r059_emit32(b,R059StateBase+0x10u);b.push_back(0xc3);std::memcpy(code+0x040,b.data(),b.size());}
    {std::vector<std::uint8_t>b;r059_inc(b,R059StateBase+0x14u);b.insert(b.end(),{0x8b,0x44,0x24,0x04});r059_store_eax(b,R059StateBase+0x18u);b.push_back(0xc3);std::memcpy(code+0x080,b.data(),b.size());}
    {std::vector<std::uint8_t>b;r059_inc(b,R059StateBase+0x1cu);b.insert(b.end(),{0x8b,0x44,0x24,0x04});r059_store_eax(b,R059StateBase+0x20u);b.insert(b.end(),{0x8b,0x44,0x24,0x08});r059_store_eax(b,R059StateBase+0x24u);b.push_back(0xc3);std::memcpy(code+0x0c0,b.data(),b.size());}
    if(mprotect(code,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("r059 stub mprotect");
}
void set_r059_patches(bool enabled){
    r046_write_rel32(0x465263u,0xe8,enabled?R059StubBlock+0x000u:0x4285a0u);
    r046_write_rel32(0x4652f0u,0xe8,enabled?R059StubBlock+0x040u:0x428880u);
    r046_write_rel32(0x44138au,0xe8,enabled?R059StubBlock+0x080u:0x42ca60u);
    r046_write_rel32(0x4413ceu,0xe8,enabled?R059StubBlock+0x0c0u:0x42cdd0u);
    r046_write_rel32(0x4413e2u,0xe8,enabled?R059StubBlock+0x0c0u:0x42cdd0u);
    constexpr std::uint32_t site=0x465eb4u;auto* page=reinterpret_cast<void*>(site&~0xfffu);
    if(mprotect(page,4096,PROT_READ|PROT_WRITE|PROT_EXEC))throw std::runtime_error("r059 lookup patch protection");
    auto* q=reinterpret_cast<std::uint8_t*>(site);
    if(enabled){q[0]=0xb9;std::uint32_t a=R059TableBase;std::memcpy(q+1,&a,4);q[5]=0x90;}
    else{const std::uint8_t original[6]={0xff,0x25,0x8c,0x98,0x03,0x01};std::memcpy(q,original,6);}
    if(mprotect(page,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("r059 lookup patch restore protection");
}
bool r059_oracle_name(const std::string& q){
    return q=="ui_notify_441370_r059"||q=="ui_set_active_4413f0_r059"||q=="compact_u32_list_441410_r059"||
           q=="ui_resource_reset_465250_r059"||q=="ui_resource_ready_4652e0_r059"||q=="ui_text_set_xy_42cc00_r059"||
           q=="ui_text_set_color_42cca0_r059"||q=="ui_text_set_mode_42ccb0_r059"||q=="ui_table_lookup_465eb0_r059"||q=="r059_ui_batch";
}
void reset_r059_trace(std::int32_t status){std::memset(reinterpret_cast<void*>(R059StateBase),0,0x100);*reinterpret_cast<std::int32_t*>(R059StateBase+0x10u)=status;}
PcUiNotifyGlobals r059_guest_globals(){
    PcUiNotifyGlobals g{};g.x=*reinterpret_cast<std::int16_t*>(0x956bb8u);g.y=*reinterpret_cast<std::int16_t*>(0x956bbau);g.base_x=*reinterpret_cast<std::int16_t*>(0x956bb4u);g.base_y=*reinterpret_cast<std::int16_t*>(0x956bb6u);
    g.color=*reinterpret_cast<std::uint32_t*>(0x956bccu);g.mode=*reinterpret_cast<std::uint32_t*>(0x956bd0u);g.source_x=*reinterpret_cast<std::uint32_t*>(0x631b30u);g.source_y=*reinterpret_cast<std::uint32_t*>(0x631b34u);g.alternate=*reinterpret_cast<std::uint8_t*>(0x7d68bcu);return g;
}
void r059_store_globals(const PcUiNotifyGlobals& g){
    *reinterpret_cast<std::int16_t*>(0x956bb8u)=g.x;*reinterpret_cast<std::int16_t*>(0x956bbau)=g.y;*reinterpret_cast<std::int16_t*>(0x956bb4u)=g.base_x;*reinterpret_cast<std::int16_t*>(0x956bb6u)=g.base_y;
    *reinterpret_cast<std::uint32_t*>(0x956bccu)=g.color;*reinterpret_cast<std::uint32_t*>(0x956bd0u)=g.mode;*reinterpret_cast<std::uint32_t*>(0x631b30u)=g.source_x;*reinterpret_cast<std::uint32_t*>(0x631b34u)=g.source_y;*reinterpret_cast<std::uint8_t*>(0x7d68bcu)=g.alternate;
}
std::array<std::uint8_t,25> r059_globals_blob(const PcUiNotifyGlobals& g){
    std::array<std::uint8_t,25> o{};std::memcpy(o.data()+0,&g.x,2);std::memcpy(o.data()+2,&g.y,2);std::memcpy(o.data()+4,&g.base_x,2);std::memcpy(o.data()+6,&g.base_y,2);std::memcpy(o.data()+8,&g.color,4);std::memcpy(o.data()+12,&g.mode,4);std::memcpy(o.data()+16,&g.source_x,4);std::memcpy(o.data()+20,&g.source_y,4);o[24]=g.alternate;return o;
}
std::array<std::uint8_t,25> r059_guest_globals_blob(){return r059_globals_blob(r059_guest_globals());}
struct R059Trace{std::uint32_t free_n{},free_h{},status_n{},status_h{},style_n{},style_v{},draw_n{},draw_fmt{},draw_arg{},lookup474{},lookup476{};std::int32_t status{};};
void r059_native_hv(void* u,std::uint32_t pc,std::uint32_t h){auto& t=*static_cast<R059Trace*>(u);if(pc==0x4285a0u){++t.free_n;t.free_h=h;}}
std::int32_t r059_native_hi(void* u,std::uint32_t pc,std::uint32_t h){auto& t=*static_cast<R059Trace*>(u);if(pc==0x428880u){++t.status_n;t.status_h=h;}return t.status;}
void r059_native_vv(void* u,std::uint32_t pc,std::uint32_t v){auto& t=*static_cast<R059Trace*>(u);if(pc==0x42ca60u){++t.style_n;t.style_v=v;}}
std::uint32_t r059_native_lookup(void* u,std::uint32_t,std::uint32_t id){auto& t=*static_cast<R059Trace*>(u);return id==0x476u?t.lookup476:t.lookup474;}
void r059_native_draw(void* u,std::uint32_t pc,std::uint32_t fmt,std::uint32_t arg){auto& t=*static_cast<R059Trace*>(u);if(pc==0x42cdd0u){++t.draw_n;t.draw_fmt=fmt;t.draw_arg=arg;}}
PcUiNotifyServices r059_services(R059Trace& t){return {&t,r059_native_hv,r059_native_hi,r059_native_vv,r059_native_lookup,r059_native_draw};}
std::array<std::uint8_t,40> r059_trace_blob(const R059Trace& t){std::array<std::uint8_t,40> o{};const std::uint32_t v[10]={t.free_n,t.free_h,t.status_n,t.status_h,std::uint32_t(t.status),t.style_n,t.style_v,t.draw_n,t.draw_fmt,t.draw_arg};std::memcpy(o.data(),v,sizeof(v));return o;}
std::array<std::uint8_t,40> r059_guest_trace_blob(){std::array<std::uint8_t,40> o{};std::memcpy(o.data(),reinterpret_cast<void*>(R059StateBase),40);return o;}
std::array<std::uint8_t,R059ObjectSize> r059_object_fixture(unsigned n){std::array<std::uint8_t,R059ObjectSize> o{};std::mt19937 g(0x44137059u^(n*2654435761u));for(auto& b:o)b=std::uint8_t(g());return o;}
// r060: first continuous normal-path factory-wrapper block at 0x441450..0x441928.
constexpr std::uint32_t R060StubBlock=0x32023000u;
constexpr std::uint32_t R060StateBase=0x33084000u;
struct R060FactorySpec {
    const char* label;
    std::uint32_t start,bytes,ctor;
    std::uint32_t(*native)(const PcFactoryServices&);
};
static const R060FactorySpec R060Factories[]={
    {"object_factory_441450_r060",0x441450u,0x758u,0x4c5120u,object_factory_441450},
    {"object_factory_4414b0_r060",0x4414b0u,0x154u,0x4c5550u,object_factory_4414b0},
    {"object_factory_441510_r060",0x441510u,0x154u,0x4c5980u,object_factory_441510},
    {"object_factory_441570_r060",0x441570u,0x14cu,0x4c5cd0u,object_factory_441570},
    {"object_factory_4415d0_r060",0x4415d0u,0x14cu,0x4c5ef0u,object_factory_4415d0},
    {"object_factory_441630_r060",0x441630u,0x1f4u,0x4c60b0u,object_factory_441630},
    {"object_factory_441690_r060",0x441690u,0x2200u,0x4c6300u,object_factory_441690},
    {"object_factory_4416f0_r060",0x4416f0u,0x31c8u,0x4c6fd0u,object_factory_4416f0},
    {"object_factory_441750_r060",0x441750u,0x1f10u,0x4c8030u,object_factory_441750},
    {"object_factory_4417b0_r060",0x4417b0u,0x1674u,0x4c8de0u,object_factory_4417b0},
    {"object_factory_441810_r060",0x441810u,0x474u,0x4c9a90u,object_factory_441810},
    {"object_factory_441870_r060",0x441870u,0x27f9cu,0x4cb300u,object_factory_441870},
    {"object_factory_4418d0_r060",0x4418d0u,0x27f9cu,0x4ccfc0u,object_factory_4418d0}
};
// r061: exact continuation of the same 0x60-stride factory series.
static const R060FactorySpec R061Factories[]={
    {"object_factory_441930_r061",0x441930u,0x279c4u,0x4ce170u,object_factory_441930},
    {"object_factory_441990_r061",0x441990u,0x27a68u,0x4cebf0u,object_factory_441990},
    {"object_factory_4419f0_r061",0x4419f0u,0x27ba8u,0x4d05e0u,object_factory_4419f0},
    {"object_factory_441a50_r061",0x441a50u,0x27ce8u,0x4d2710u,object_factory_441a50},
    {"object_factory_441ab0_r061",0x441ab0u,0x28fcu,0x4d4230u,object_factory_441ab0},
    {"object_factory_441b10_r061",0x441b10u,0x1d90u,0x4d7140u,object_factory_441b10},
    {"object_factory_441b70_r061",0x441b70u,0x5ecu,0x4d9010u,object_factory_441b70},
    {"object_factory_441bd0_r061",0x441bd0u,0x27b08u,0x4da2f0u,object_factory_441bd0},
    {"object_factory_441c30_r061",0x441c30u,0x275d8u,0x4db680u,object_factory_441c30},
    {"object_factory_441c90_r061",0x441c90u,0x1324u,0x4926f0u,object_factory_441c90},
    {"object_factory_441cf0_r061",0x441cf0u,0x12b0u,0x48c490u,object_factory_441cf0},
    {"object_factory_441d50_r061",0x441d50u,0xd8u,0x4dc080u,object_factory_441d50},
    {"object_factory_441db0_r061",0x441db0u,0x12e8u,0x4dcc00u,object_factory_441db0},
    {"object_factory_441e10_r061",0x441e10u,0x1f4u,0x4dd410u,object_factory_441e10},
    {"object_factory_441e70_r061",0x441e70u,0x11a4u,0x4dd5c0u,object_factory_441e70},
    {"object_factory_441ed0_r061",0x441ed0u,0x12ecu,0x4de6f0u,object_factory_441ed0},
    {"object_factory_441f30_r061",0x441f30u,0x17cu,0x4de9d0u,object_factory_441f30},
    {"object_factory_441f90_r061",0x441f90u,0x17b8u,0x4dec60u,object_factory_441f90},
    {"object_factory_441ff0_r061",0x441ff0u,0x271cu,0x4df820u,object_factory_441ff0},
    {"object_factory_442050_r061",0x442050u,0x1b68u,0x4dfaf0u,object_factory_442050},
    {"object_factory_4420b0_r061",0x4420b0u,0x1444u,0x4e0680u,object_factory_4420b0},
    {"object_factory_442110_r061",0x442110u,0xc768u,0x4e1890u,object_factory_442110},
    {"object_factory_442170_r061",0x442170u,0x448cu,0x4e3ac0u,object_factory_442170},
    {"object_factory_4421d0_r061",0x4421d0u,0x449cu,0x4e4c60u,object_factory_4421d0},
    {"object_factory_442230_r061",0x442230u,0x20b0u,0x4e5590u,object_factory_442230},
    {"object_factory_442290_r061",0x442290u,0x1768u,0x4e5720u,object_factory_442290},
    {"object_factory_4422f0_r061",0x4422f0u,0x1decu,0x4e5b20u,object_factory_4422f0},
    {"object_factory_442350_r061",0x442350u,0x1b48u,0x4e60c0u,object_factory_442350},
    {"object_factory_4423b0_r061",0x4423b0u,0x26f8u,0x4e6220u,object_factory_4423b0},
    {"object_factory_442410_r061",0x442410u,0x22d4u,0x493020u,object_factory_442410},
    {"object_factory_442470_r061",0x442470u,0x130e8u,0x4e7600u,object_factory_442470},
    {"object_factory_4424d0_r061",0x4424d0u,0x88a8u,0x4e7a90u,object_factory_4424d0},
    {"object_factory_442530_r061",0x442530u,0x132cu,0x4e7d60u,object_factory_442530},
    {"object_factory_442590_r061",0x442590u,0x674u,0x4e8130u,object_factory_442590},
    {"object_factory_4425f0_r061",0x4425f0u,0x4488u,0x4e9160u,object_factory_4425f0},
    {"object_factory_442650_r061",0x442650u,0x2408u,0x4e9e50u,object_factory_442650},
    {"object_factory_4426b0_r061",0x4426b0u,0x1c4cu,0x4eb100u,object_factory_4426b0},
    {"object_factory_442710_r061",0x442710u,0x1c10u,0x4302c0u,object_factory_442710},
    {"object_factory_442770_r061",0x442770u,0x1c10u,0x4afe30u,object_factory_442770},
    {"object_factory_4427d0_r061",0x4427d0u,0x1c14u,0x4eb2e0u,object_factory_4427d0},
    {"object_factory_442830_r061",0x442830u,0x1c10u,0x4eb5b0u,object_factory_442830},
    {"object_factory_442890_r061",0x442890u,0x1c10u,0x4eb8d0u,object_factory_442890},
    {"object_factory_4428f0_r061",0x4428f0u,0x1c10u,0x4eb9e0u,object_factory_4428f0},
    {"object_factory_442950_r061",0x442950u,0x1c10u,0x4ebd10u,object_factory_442950}
};
void r060_emit32(std::vector<std::uint8_t>& b,std::uint32_t v){for(unsigned k=0;k<4;++k)b.push_back(std::uint8_t(v>>(8u*k)));}
void r060_inc(std::vector<std::uint8_t>& b,std::uint32_t a){b.insert(b.end(),{0xff,0x05});r060_emit32(b,a);}
void map_r060_factory_stubs(){
    auto* code=static_cast<std::uint8_t*>(map_at(R060StubBlock,4096,PROT_READ|PROT_WRITE));map_at(R060StateBase,4096,PROT_READ|PROT_WRITE);
    // allocator(size): trace call/size and return scripted token.
    {std::vector<std::uint8_t>b;r060_inc(b,R060StateBase+0x08u);b.insert(b.end(),{0x8b,0x44,0x24,0x04,0xa3});r060_emit32(b,R060StateBase+0x0cu);b.push_back(0xa1);r060_emit32(b,R060StateBase+0x00u);b.push_back(0xc3);std::memcpy(code+0x000u,b.data(),b.size());}
    // constructor(this): trace call/ECX and return scripted EAX.
    {std::vector<std::uint8_t>b;r060_inc(b,R060StateBase+0x10u);b.insert(b.end(),{0x89,0x0d});r060_emit32(b,R060StateBase+0x14u);b.push_back(0xa1);r060_emit32(b,R060StateBase+0x04u);b.push_back(0xc3);std::memcpy(code+0x080u,b.data(),b.size());}
    if(mprotect(code,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("r060 stub mprotect");
}
void r060_raw_write(std::uint32_t a,const std::uint8_t* p,std::size_t n){
    const std::uint32_t first=a&~0xfffu,last=(a+std::uint32_t(n-1u))&~0xfffu;
    for(std::uint32_t page=first;;page+=0x1000u){if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE|PROT_EXEC))throw std::runtime_error("r060 raw patch protection");if(page==last)break;}
    std::memcpy(reinterpret_cast<void*>(a),p,n);
    for(std::uint32_t page=first;;page+=0x1000u){if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_EXEC))throw std::runtime_error("r060 raw patch restore protection");if(page==last)break;}
}
void r060_check_call(std::uint32_t site,std::uint32_t expected){
    auto* q=reinterpret_cast<const std::uint8_t*>(site);std::int32_t rel{};std::memcpy(&rel,q+1,4);
    if(q[0]!=0xe8||std::uint32_t(std::int64_t(site+5u)+rel)!=expected)throw std::runtime_error("r060 original CALL target changed");
}
void set_r060_factory_patches(bool enabled){
    static const std::uint8_t fs_load[6]={0x64,0xa1,0,0,0,0};
    static const std::uint8_t fs_set_esp[7]={0x64,0x89,0x25,0,0,0,0};
    static const std::uint8_t fs_set_ecx[7]={0x64,0x89,0x0d,0,0,0,0};
    static const std::uint8_t zero_eax[6]={0x31,0xc0,0x90,0x90,0x90,0x90};
    static const std::uint8_t nops7[7]={0x90,0x90,0x90,0x90,0x90,0x90,0x90};
    for(const auto& f:R060Factories){
        if(enabled){r060_check_call(f.start+0x1bu,0x5802cfu);r060_check_call(f.start+0x34u,f.ctor);}
        r046_write_rel32(f.start+0x1bu,0xe8,enabled?R060StubBlock+0x000u:0x5802cfu);
        r046_write_rel32(f.start+0x34u,0xe8,enabled?R060StubBlock+0x080u:f.ctor);
        r060_raw_write(f.start+0x07u,enabled?zero_eax:fs_load,6);
        r060_raw_write(f.start+0x0eu,enabled?nops7:fs_set_esp,7);
        r060_raw_write(f.start+0x3du,enabled?nops7:fs_set_ecx,7);
        r060_raw_write(f.start+0x4eu,enabled?nops7:fs_set_ecx,7);
    }
}
bool r060_oracle_name(const std::string& q){if(q=="r060_factory_batch")return true;for(const auto& f:R060Factories)if(q==f.label)return true;return false;}
void set_r061_factory_patches(bool enabled){
    static const std::uint8_t fs_load[6]={0x64,0xa1,0,0,0,0};
    static const std::uint8_t fs_set_esp[7]={0x64,0x89,0x25,0,0,0,0};
    static const std::uint8_t fs_set_ecx[7]={0x64,0x89,0x0d,0,0,0,0};
    static const std::uint8_t zero_eax[6]={0x31,0xc0,0x90,0x90,0x90,0x90};
    static const std::uint8_t nops7[7]={0x90,0x90,0x90,0x90,0x90,0x90,0x90};
    for(const auto& f:R061Factories){
        if(enabled){r060_check_call(f.start+0x1bu,0x5802cfu);r060_check_call(f.start+0x34u,f.ctor);}
        r046_write_rel32(f.start+0x1bu,0xe8,enabled?R060StubBlock+0x000u:0x5802cfu);
        r046_write_rel32(f.start+0x34u,0xe8,enabled?R060StubBlock+0x080u:f.ctor);
        r060_raw_write(f.start+0x07u,enabled?zero_eax:fs_load,6);
        r060_raw_write(f.start+0x0eu,enabled?nops7:fs_set_esp,7);
        r060_raw_write(f.start+0x3du,enabled?nops7:fs_set_ecx,7);
        r060_raw_write(f.start+0x4eu,enabled?nops7:fs_set_ecx,7);
    }
}
bool r061_oracle_name(const std::string& q){if(q=="r061_factory_batch")return true;for(const auto& f:R061Factories)if(q==f.label)return true;return false;}
// r062: first constructor/state-initializer shape break after the factory series.
// Child constructor/helper bodies stay explicit boundaries.  The original parent
// code is executed directly; only SEH fs:0 plumbing and those child CALLs are stubbed.
constexpr std::uint32_t R062StubBlock=0x32025000u;
constexpr std::uint32_t R062StateBase=0x33085000u;
constexpr std::uint32_t R062ObjectBase=0x3f000000u;
constexpr std::size_t R062ObjectSize=0x3000u;
constexpr unsigned R062SiteCount=14u;
struct R062CallSpec{std::uint32_t site,target;unsigned slot;enum Kind{This,ThisArg,Array} kind;};
static const R062CallSpec R062Calls[]={
    {0x4429cdu,0x48f480u,0u,R062CallSpec::This},
    {0x4429e6u,0x4ed950u,1u,R062CallSpec::This},
    {0x4429f6u,0x48c490u,2u,R062CallSpec::This},
    {0x442a18u,0x5816bdu,3u,R062CallSpec::Array},
    {0x442a2au,0x490f10u,4u,R062CallSpec::ThisArg},
    {0x442a47u,0x5816bdu,5u,R062CallSpec::Array},
    {0x442a7du,0x48f480u,6u,R062CallSpec::This},
    {0x442a93u,0x4ed950u,7u,R062CallSpec::This},
    {0x442aa0u,0x48c490u,8u,R062CallSpec::This},
    {0x442b13u,0x5816bdu,9u,R062CallSpec::Array},
    {0x442b53u,0x442ac0u,10u,R062CallSpec::This},
    {0x442b62u,0x465160u,11u,R062CallSpec::This},
    {0x442b72u,0x465160u,12u,R062CallSpec::This},
    {0x442b82u,0x465160u,13u,R062CallSpec::This},
};
constexpr std::uint32_t r062_slot(unsigned slot){return R062StateBase+0x20u+slot*0x20u;}
void r062_store_eax(std::vector<std::uint8_t>& b,std::uint32_t a){b.push_back(0xa3);r060_emit32(b,a);}
void r062_record_prefix(std::vector<std::uint8_t>& b,const R062CallSpec& c){
    r060_inc(b,R062StateBase);b.push_back(0xa1);r060_emit32(b,R062StateBase);r062_store_eax(b,r062_slot(c.slot));
    b.push_back(0xb8);r060_emit32(b,c.target);r062_store_eax(b,r062_slot(c.slot)+4u);
}
void map_r062_constructor_stubs(){
    auto* code=static_cast<std::uint8_t*>(map_at(R062StubBlock,4096,PROT_READ|PROT_WRITE));
    map_at(R062StateBase,4096,PROT_READ|PROT_WRITE);map_at(R062ObjectBase,R062ObjectSize,PROT_READ|PROT_WRITE);
    for(const auto& c:R062Calls){
        std::vector<std::uint8_t>b;r062_record_prefix(b,c);
        const std::uint32_t slot=r062_slot(c.slot);
        if(c.kind==R062CallSpec::This){
            b.insert(b.end(),{0x89,0x0d});r060_emit32(b,slot+8u);b.push_back(0xc3);
        }else if(c.kind==R062CallSpec::ThisArg){
            b.insert(b.end(),{0x89,0x0d});r060_emit32(b,slot+8u);
            b.insert(b.end(),{0x8b,0x44,0x24,0x04});r062_store_eax(b,slot+12u);b.insert(b.end(),{0xc2,0x04,0x00});
        }else{
            for(unsigned a=0;a<5u;++a){b.insert(b.end(),{0x8b,0x44,0x24,std::uint8_t(4u+4u*a)});r062_store_eax(b,slot+8u+4u*a);}
            b.insert(b.end(),{0xc2,0x14,0x00});
        }
        const std::size_t off=std::size_t(c.slot)*0x80u;if(off+b.size()>4096u)throw std::runtime_error("r062 stub block overflow");
        std::memcpy(code+off,b.data(),b.size());
    }
    if(mprotect(code,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("r062 stub mprotect");
}
void set_r062_constructor_patches(bool enabled){
    static const std::uint8_t fs_load[6]={0x64,0xa1,0,0,0,0};
    static const std::uint8_t fs_set_esp[7]={0x64,0x89,0x25,0,0,0,0};
    static const std::uint8_t fs_set_ecx[7]={0x64,0x89,0x0d,0,0,0,0};
    static const std::uint8_t zero_eax[6]={0x31,0xc0,0x90,0x90,0x90,0x90};
    static const std::uint8_t nops7[7]={0x90,0x90,0x90,0x90,0x90,0x90,0x90};
    for(const auto& c:R062Calls){if(enabled)r060_check_call(c.site,c.target);r046_write_rel32(c.site,0xe8,enabled?R062StubBlock+c.slot*0x80u:c.target);}
    const std::uint32_t seh[][3]={{0x4429b0u,0x442a53u,0u},{0x442a60u,0x442aacu,0u},{0x442b20u,0x442c0du,0u}};
    for(const auto& f:seh){const std::uint32_t start=f[0],restore=f[1];r060_raw_write(start+0x07u,enabled?zero_eax:fs_load,6);r060_raw_write(start+0x0eu,enabled?nops7:fs_set_esp,7);r060_raw_write(restore,enabled?nops7:fs_set_ecx,7);}
}
bool r062_oracle_name(const std::string& q){return q=="object_ctor_4429b0_r062"||q=="object_ctor_442a60_r062"||q=="object_block_init_442ac0_r062"||q=="object_state_ctor_442b20_r062"||q=="r062_constructor_batch";}
void reset_r062_trace(){std::memset(reinterpret_cast<void*>(R062StateBase),0,0x200u);}
struct R062Trace{std::uint32_t count{};unsigned next{};std::array<std::array<std::uint32_t,7>,R062SiteCount> site{};};
void r062_native_record(R062Trace& t,std::uint32_t pc,std::uint32_t ptr,std::uint32_t a0=0,std::uint32_t a1=0,std::uint32_t a2=0,std::uint32_t a3=0){
    if(t.next>=R062SiteCount)throw std::runtime_error("r062 native trace overflow");auto& r=t.site[t.next++];r={++t.count,pc,ptr,a0,a1,a2,a3};
}
void r062_native_this(void* u,std::uint32_t pc,Bytes,std::size_t off){auto& t=*static_cast<R062Trace*>(u);r062_native_record(t,pc,R062ObjectBase+std::uint32_t(off));}
void r062_native_this_arg(void* u,std::uint32_t pc,Bytes,std::size_t off,std::uint32_t a0){auto& t=*static_cast<R062Trace*>(u);r062_native_record(t,pc,R062ObjectBase+std::uint32_t(off),a0);}
void r062_native_array(void* u,std::uint32_t pc,Bytes,std::size_t off,std::uint32_t elem,std::uint32_t count,std::uint32_t ctor,std::uint32_t dtor){auto& t=*static_cast<R062Trace*>(u);r062_native_record(t,pc,R062ObjectBase+std::uint32_t(off),elem,count,ctor,dtor);}
PcObjectInitServices r062_services(R062Trace& t,unsigned first){t.next=first;return {&t,r062_native_this,r062_native_this_arg,r062_native_array};}
std::array<std::uint8_t,0x200> r062_trace_blob(const R062Trace& t){std::array<std::uint8_t,0x200> o{};std::memcpy(o.data(),&t.count,4);for(unsigned i=0;i<R062SiteCount;++i)std::memcpy(o.data()+0x20u+i*0x20u,t.site[i].data(),7u*4u);return o;}
std::array<std::uint8_t,0x200> r062_guest_trace_blob(){std::array<std::uint8_t,0x200> o{};std::memcpy(o.data(),reinterpret_cast<void*>(R062StateBase),o.size());return o;}
std::array<std::uint8_t,R062ObjectSize> r062_object_fixture(unsigned n){std::array<std::uint8_t,R062ObjectSize> o{};std::mt19937 g(0x4429b062u^(n*2654435761u));for(auto& b:o)b=std::uint8_t(g());return o;}
void r062_set_mode(std::uint8_t v){*reinterpret_cast<std::uint8_t*>(0x6319a1u)=v;}

// r063: state reset at 0x442C20, direct timer getter 0x4AF500 and the two
// selector routines at 0x442CB0 / 0x442D70.  Virtual calls use synthetic
// objects/vtables; raw guest handles never cross into reconstructed C++.
constexpr std::uint32_t R063StubBlock=0x32026000u;
constexpr std::uint32_t R063StateBase=0x33087000u;
constexpr std::uint32_t R063ObjectBase=0x3f100000u;
constexpr std::uint32_t R063ChildBase=0x3f104000u;
constexpr std::uint32_t R063VtableBase=0x3f10c000u;
constexpr std::uint32_t R063ManagerBase=0x3f10d000u;
constexpr std::size_t R063ObjectSize=0x2000u;
constexpr std::size_t R063MapSize=0x10000u;
void map_r063_state_selectors(){
    auto* code=static_cast<std::uint8_t*>(map_at(R063StubBlock,4096,PROT_READ|PROT_WRITE));
    map_at(R063StateBase,4096,PROT_READ|PROT_WRITE);
    map_at(R063ObjectBase,R063MapSize,PROT_READ|PROT_WRITE);
    // 0x4AF500 boundary: record one call, return scripted float on x87 ST0.
    {std::vector<std::uint8_t>b;r060_inc(b,R063StateBase+0x04u);b.insert(b.end(),{0xd9,0x05});r060_emit32(b,R063StateBase+0x00u);b.push_back(0xc3);std::memcpy(code+0x000u,b.data(),b.size());}
    // Synthetic virtual slot +0x08: record ECX and return scripted EAX.
    {std::vector<std::uint8_t>b;r060_inc(b,R063StateBase+0x0cu);b.insert(b.end(),{0x89,0x0d});r060_emit32(b,R063StateBase+0x10u);b.push_back(0xa1);r060_emit32(b,R063StateBase+0x08u);b.push_back(0xc3);std::memcpy(code+0x080u,b.data(),b.size());}
    // 0x4EE930 boundary: record manager ECX; return value is ignored by parent.
    {std::vector<std::uint8_t>b;r060_inc(b,R063StateBase+0x14u);b.insert(b.end(),{0x89,0x0d});r060_emit32(b,R063StateBase+0x18u);b.push_back(0xc3);std::memcpy(code+0x100u,b.data(),b.size());}
    *reinterpret_cast<std::uint32_t*>(R063VtableBase+0x08u)=R063StubBlock+0x080u;
    for(unsigned k=0;k<4u;++k)*reinterpret_cast<std::uint32_t*>(R063ChildBase+k*0x100u)=R063VtableBase;
    if(mprotect(code,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("r063 stub mprotect");
    for(const std::uint32_t page:{0x7d6000u,0x842000u})
        if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("r063 data-page mprotect");
}
void set_r063_patches(bool enabled){
    if(enabled){r060_check_call(0x442c5cu,0x4af500u);r060_check_call(0x442d56u,0x4ee930u);}
    r046_write_rel32(0x442c5cu,0xe8,enabled?R063StubBlock+0x000u:0x4af500u);
    r046_write_rel32(0x442d56u,0xe8,enabled?R063StubBlock+0x100u:0x4ee930u);
}
bool r063_oracle_name(const std::string& q){
    return q=="timer_value_4af500_r063"||q=="object_state_reset_442c20_r063"||
           q=="object_select_primary_442cb0_r063"||q=="object_select_secondary_442d70_r063"||
           q=="r063_state_selector_batch";
}
struct R063Trace{float timer{};std::uint32_t float_calls{},virtual_ret{},virtual_calls{},virtual_handle{},void_calls{},void_handle{};};
void reset_r063_trace(float timer,std::uint32_t virtual_ret){
    std::memset(reinterpret_cast<void*>(R063StateBase),0,0x40u);
    *reinterpret_cast<float*>(R063StateBase+0x00u)=timer;
    *reinterpret_cast<std::uint32_t*>(R063StateBase+0x08u)=virtual_ret;
}
float r063_native_float(void* u,std::uint32_t pc){auto& t=*static_cast<R063Trace*>(u);if(pc!=0x4af500u)throw std::runtime_error("r063 float target");++t.float_calls;return timer_value_4af500(t.timer);}
std::uint32_t r063_native_virtual(void* u,std::uint32_t handle,std::uint32_t slot){auto& t=*static_cast<R063Trace*>(u);if(slot!=8u)throw std::runtime_error("r063 virtual slot");++t.virtual_calls;t.virtual_handle=handle;return t.virtual_ret;}
void r063_native_void(void* u,std::uint32_t pc,std::uint32_t handle){auto& t=*static_cast<R063Trace*>(u);if(pc!=0x4ee930u)throw std::runtime_error("r063 void target");++t.void_calls;t.void_handle=handle;}
PcObjectResetServices r063_reset_services(R063Trace& t){return {&t,r063_native_float};}
PcObjectSelectorServices r063_selector_services(R063Trace& t){return {&t,r063_native_virtual,r063_native_void};}
std::array<std::uint8_t,0x20> r063_trace_blob(const R063Trace& t){
    std::array<std::uint8_t,0x20> o{};const std::uint32_t v[7]={fbits(t.timer),t.float_calls,t.virtual_ret,t.virtual_calls,t.virtual_handle,t.void_calls,t.void_handle};std::memcpy(o.data(),v,sizeof(v));return o;
}
std::array<std::uint8_t,0x20> r063_guest_trace_blob(){std::array<std::uint8_t,0x20> o{};std::memcpy(o.data(),reinterpret_cast<void*>(R063StateBase),o.size());return o;}
std::array<std::uint8_t,R063ObjectSize> r063_object_fixture(unsigned n){std::array<std::uint8_t,R063ObjectSize> o{};std::mt19937 g(0x442c2063u^(n*2654435761u));for(auto& b:o)b=std::uint8_t(g());return o;}
std::uint32_t r063_child(unsigned k){return R063ChildBase+(k&3u)*0x100u;}
void r063_clear_selector_fields(Bytes b){b.puti(0x518u,0);b.put8(0x494u,0u);b.put8(0x48cu,0u);b.puti(0x484u,0);b.put32(0x498u,0u);b.put32(0x490u,0u);b.put32(0x488u,0u);}

// r064: dispatcher/table/query helpers at 0x442E00..0x4430AE.  The
// already-closed 0x564C90 leaf executes directly against synthetic children.
// Virtual +0x0C and the 0x446A50 embedded tail remain explicit boundaries.
constexpr std::uint32_t R064StubBlock=0x32027000u;
constexpr std::uint32_t R064StateBase=0x33088000u;
constexpr std::uint32_t R064ObjectBase=0x3f120000u;
constexpr std::uint32_t R064ChildBase=0x3f124000u;
constexpr std::uint32_t R064VtableBase=0x3f12f000u;
constexpr std::size_t R064ObjectSize=0x2000u;
constexpr std::size_t R064MapSize=0x10000u;
void map_r064_object_state(){
    auto* code=static_cast<std::uint8_t*>(map_at(R064StubBlock,4096,PROT_READ|PROT_WRITE));
    map_at(R064StateBase,4096,PROT_READ|PROT_WRITE);
    map_at(R064ObjectBase,R064MapSize,PROT_READ|PROT_WRITE);
    // Synthetic virtual slot +0x0C: append ECX handle to trace.
    {std::vector<std::uint8_t>b;b.push_back(0xa1);r060_emit32(b,R064StateBase+0x00u);
     b.insert(b.end(),{0x89,0x0c,0x85});r060_emit32(b,R064StateBase+0x20u);b.push_back(0x40);
     b.push_back(0xa3);r060_emit32(b,R064StateBase+0x00u);b.push_back(0xc3);std::memcpy(code+0x000u,b.data(),b.size());}
    // 0x446A50 embedded tail: record ECX=self+0x51C and return.
    {std::vector<std::uint8_t>b;r060_inc(b,R064StateBase+0x04u);b.insert(b.end(),{0x89,0x0d});r060_emit32(b,R064StateBase+0x08u);b.push_back(0xc3);std::memcpy(code+0x080u,b.data(),b.size());}
    *reinterpret_cast<std::uint32_t*>(R064VtableBase+0x0cu)=R064StubBlock+0x000u;
    for(unsigned k=0;k<132u;++k)*reinterpret_cast<std::uint32_t*>(R064ChildBase+k*0x40u)=R064VtableBase;
    if(mprotect(code,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("r064 stub mprotect");
}
void r064_check_rel32(std::uint32_t site,std::uint8_t opcode,std::uint32_t target){
    auto* q=reinterpret_cast<const std::uint8_t*>(site);std::int32_t rel{};std::memcpy(&rel,q+1,4);
    if(q[0]!=opcode||std::uint32_t(std::int64_t(site+5u)+rel)!=target)throw std::runtime_error("r064 original rel32 target changed");
}
void set_r064_patches(bool enabled){
    if(enabled)r064_check_rel32(0x442e6bu,0xe9,0x446a50u);
    r046_write_rel32(0x442e6bu,0xe9,enabled?R064StubBlock+0x080u:0x446a50u);
}
bool r064_oracle_name(const std::string& q){
    return q=="object_dispatch_state_442e00_r064"||q=="object_refresh_state_table_442e70_r064"||
           q=="object_query_previous_state_442ec0_r064"||q=="object_store_depth_pair_442f20_r064"||
           q=="object_query_last_state_443040_r064"||q=="object_has_active_state_443060_r064"||
           q=="r064_object_state_batch";
}
struct R064Trace{std::uint32_t virtual_count{},tail_count{},tail_this{};std::array<std::uint32_t,4> handles{};};
void reset_r064_trace(){std::memset(reinterpret_cast<void*>(R064StateBase),0,0x40u);}
void r064_native_virtual(void* u,std::uint32_t h,std::uint32_t slot){auto& t=*static_cast<R064Trace*>(u);if(slot!=0x0cu)throw std::runtime_error("r064 virtual slot");if(t.virtual_count>=t.handles.size())throw std::runtime_error("r064 virtual trace overflow");t.handles[t.virtual_count++]=h;}
void r064_native_embedded(void* u,std::uint32_t pc,Bytes,std::size_t off){auto& t=*static_cast<R064Trace*>(u);if(pc!=0x446a50u||off!=0x51cu)throw std::runtime_error("r064 embedded boundary");++t.tail_count;t.tail_this=R064ObjectBase+std::uint32_t(off);}
std::uint32_t r064_native_u32(void*,std::uint32_t pc,std::uint32_t h){if(pc!=0x564c90u)throw std::runtime_error("r064 u32 target");return *reinterpret_cast<const std::uint32_t*>(std::uintptr_t(h)+8u);}
PcObjectStateServices r064_services(R064Trace& t){return {&t,r064_native_virtual,r064_native_embedded,r064_native_u32};}
std::array<std::uint8_t,0x30> r064_trace_blob(const R064Trace& t){std::array<std::uint8_t,0x30> o{};std::memcpy(o.data()+0x00u,&t.virtual_count,4);std::memcpy(o.data()+0x04u,&t.tail_count,4);std::memcpy(o.data()+0x08u,&t.tail_this,4);std::memcpy(o.data()+0x20u,t.handles.data(),t.handles.size()*4u);return o;}
std::array<std::uint8_t,0x30> r064_guest_trace_blob(){std::array<std::uint8_t,0x30> o{};std::memcpy(o.data(),reinterpret_cast<void*>(R064StateBase),o.size());return o;}
std::array<std::uint8_t,R064ObjectSize> r064_object_fixture(unsigned n){std::array<std::uint8_t,R064ObjectSize> o{};std::mt19937 g(0x442e0064u^(n*2654435761u));for(auto& b:o)b=std::uint8_t(g());return o;}
std::uint32_t r064_child(unsigned k){return R064ChildBase+(k%132u)*0x40u;}
void r064_prepare_children(unsigned seed){for(unsigned k=0;k<132u;++k){auto* p=reinterpret_cast<std::uint8_t*>(r064_child(k));*reinterpret_cast<std::uint32_t*>(p)=R064VtableBase;std::uint32_t state=(seed*0x101u+k*7u)&0xffu;if(k%17u==0u)state=4u;else if(k%19u==0u)state=10u;else if(k%23u==0u)state=21u;*reinterpret_cast<std::uint32_t*>(p+8u)=state;}}

// r065: state-pair lookup/update and flagged-handle release at 0x442F50,
// 0x442FD0 and 0x4430B0.  0x446EA0 stays an explicit boundary.  The two
// 0x441200 calls remain real original child executions, with tiny call-site
// wrappers only to record which flagged branch invoked them.
constexpr std::uint32_t R065StubBlock=0x32028000u;
constexpr std::uint32_t R065StateBase=0x33089000u;
constexpr std::uint32_t R065ObjectBase=0x3f140000u;
constexpr std::uint32_t R065ChildBase=0x3f144000u;
constexpr std::size_t R065ObjectSize=0x2000u;
constexpr std::size_t R065MapSize=0x10000u;
void map_r065_state_update(){
    auto* code=static_cast<std::uint8_t*>(map_at(R065StubBlock,4096,PROT_READ|PROT_WRITE));
    map_at(R065StateBase,4096,PROT_READ|PROT_WRITE);
    map_at(R065ObjectBase,R065MapSize,PROT_READ|PROT_WRITE);
    // 0x446EA0(self+0x51C,state): record ECX and stack argument, then ret 4.
    {std::vector<std::uint8_t>b;r060_inc(b,R065StateBase+0x00u);b.insert(b.end(),{0x89,0x0d});r060_emit32(b,R065StateBase+0x04u);
     b.insert(b.end(),{0x8b,0x44,0x24,0x04});b.push_back(0xa3);r060_emit32(b,R065StateBase+0x08u);b.insert(b.end(),{0xc2,0x04,0x00});std::memcpy(code+0x000u,b.data(),b.size());}
    auto release_wrapper=[&](std::size_t off,std::uint32_t count_off,std::uint32_t handle_off){
        std::vector<std::uint8_t>b;r060_inc(b,R065StateBase+count_off);b.insert(b.end(),{0x8b,0x44,0x24,0x04});b.push_back(0xa3);r060_emit32(b,R065StateBase+handle_off);
        b.push_back(0xe9);const auto next=R065StubBlock+std::uint32_t(off)+std::uint32_t(b.size())+4u;const auto rel=std::uint32_t(std::int64_t(0x441200u)-std::int64_t(next));r060_emit32(b,rel);std::memcpy(code+off,b.data(),b.size());
    };
    release_wrapper(0x100u,0x0cu,0x10u);
    release_wrapper(0x180u,0x14u,0x18u);
    if(mprotect(code,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("r065 stub mprotect");
}
void r065_check_rel32(std::uint32_t site,std::uint8_t opcode,std::uint32_t target){
    auto* q=reinterpret_cast<const std::uint8_t*>(site);std::int32_t rel{};std::memcpy(&rel,q+1,4);
    if(q[0]!=opcode||std::uint32_t(std::int64_t(site+5u)+rel)!=target)throw std::runtime_error("r065 original rel32 target changed");
}
void set_r065_patches(bool enabled){
    if(enabled){r065_check_rel32(0x443030u,0xe8,0x446ea0u);r065_check_rel32(0x4430cbu,0xe8,0x441200u);r065_check_rel32(0x4430f1u,0xe8,0x441200u);}
    r046_write_rel32(0x443030u,0xe8,enabled?R065StubBlock+0x000u:0x446ea0u);
    r046_write_rel32(0x4430cbu,0xe8,enabled?R065StubBlock+0x100u:0x441200u);
    r046_write_rel32(0x4430f1u,0xe8,enabled?R065StubBlock+0x180u:0x441200u);
}
bool r065_oracle_name(const std::string& q){
    return q=="object_store_state_pair_442f50_r065"||q=="object_update_state_442fd0_r065"||
           q=="object_release_flagged_handles_4430b0_r065"||q=="r065_state_update_batch";
}
void reset_r065_trace(){std::memset(reinterpret_cast<void*>(R065StateBase),0,0x40u);}
struct R065UpdateTrace{std::uint32_t calls{},this_ptr{},argument{};};
std::uint32_t r065_native_query(void*,std::uint32_t pc,std::uint32_t h){if(pc!=0x564c90u)throw std::runtime_error("r065 query target");return *reinterpret_cast<const std::uint32_t*>(std::uintptr_t(h)+8u);}
void r065_native_embedded(void* u,std::uint32_t pc,Bytes,std::size_t off,std::uint32_t arg){auto& t=*static_cast<R065UpdateTrace*>(u);if(pc!=0x446ea0u||off!=0x51cu)throw std::runtime_error("r065 embedded target");++t.calls;t.this_ptr=R065ObjectBase+std::uint32_t(off);t.argument=arg;}
PcObjectStateUpdateServices r065_update_services(R065UpdateTrace& t){return {&t,r065_native_query,r065_native_embedded};}
std::array<std::uint8_t,12> r065_update_trace_blob(const R065UpdateTrace& t){std::array<std::uint8_t,12> o{};const std::uint32_t v[3]={t.calls,t.this_ptr,t.argument};std::memcpy(o.data(),v,sizeof(v));return o;}
std::array<std::uint8_t,12> r065_guest_update_trace_blob(){std::array<std::uint8_t,12> o{};std::memcpy(o.data(),reinterpret_cast<void*>(R065StateBase),o.size());return o;}
std::array<std::uint8_t,R065ObjectSize> r065_object_fixture(unsigned n){std::array<std::uint8_t,R065ObjectSize> o{};std::mt19937 g(0x442f5065u^(n*2654435761u));for(auto& b:o)b=std::uint8_t(g());return o;}
std::uint32_t r065_child(unsigned k){return R065ChildBase+(k%16u)*0x40u;}
void r065_prepare_children(unsigned seed){
    static constexpr std::uint32_t states[]={0u,1u,4u,11u,43u,60u,9u,0x53u,21u,55u,12u,58u,34u,59u,14u,3u};
    for(unsigned k=0;k<16u;++k){auto* q=reinterpret_cast<std::uint8_t*>(r065_child(k));std::memset(q,0,0x40u);*reinterpret_cast<std::uint32_t*>(q)=R058VtableBase;*reinterpret_cast<std::uint32_t*>(q+8u)=states[(k+seed)%16u];}
}
PcObjectStatePairTable r065_pair_table(){const auto& t=pc_object_state_pair_table_r065();return {t.data(),t.size()};}
struct R065RuntimeTrace{std::uint32_t pre{},destroy{},protect{};std::int8_t release_index{};std::array<std::uint32_t,2> handles{};};
void r065_runtime_void(void* u,std::uint32_t pc,std::uint32_t h){auto& t=*static_cast<R065RuntimeTrace*>(u);if(pc==0x441210u){if(t.pre<t.handles.size())t.handles[t.pre]=h;++t.pre;}}
void r065_runtime_arg(void* u,std::uint32_t pc,std::uint32_t,std::uint32_t a){auto& t=*static_cast<R065RuntimeTrace*>(u);if(pc==0x441219u&&a==1u)++t.destroy;}
std::int8_t r065_runtime_i8(void* u,std::uint32_t pc,std::uint32_t){auto& t=*static_cast<R065RuntimeTrace*>(u);if(pc!=0x01039ba0u)throw std::runtime_error("r065 protect target");++t.protect;return t.release_index;}
PcRuntimeControlServices r065_runtime_services(R065RuntimeTrace& t){return {&t,r065_runtime_void,r065_runtime_arg,nullptr,nullptr,nullptr,r065_runtime_i8};}
std::array<std::uint8_t,28> r065_release_guest_blob(){std::array<std::uint8_t,28> o{};std::memcpy(o.data(),reinterpret_cast<void*>(R065StateBase+0x0cu),16u);const std::uint32_t internal[3]={*reinterpret_cast<std::uint32_t*>(R058StateBase+0x14u),*reinterpret_cast<std::uint32_t*>(R058StateBase+0x18u),*reinterpret_cast<std::uint32_t*>(R058StateBase+0x1cu)};std::memcpy(o.data()+16u,internal,sizeof(internal));return o;}
std::array<std::uint8_t,28> r065_release_native_blob(const R065RuntimeTrace& t,bool a_call,bool b_call){std::array<std::uint8_t,28> o{};std::size_t pos=0u;const auto ah=a_call&&pos<t.pre?t.handles[pos++]:0u;const auto bh=b_call&&pos<t.pre?t.handles[pos++]:0u;const std::uint32_t v[7]={a_call?1u:0u,ah,b_call?1u:0u,bh,t.pre,t.destroy,t.protect};std::memcpy(o.data(),v,sizeof(v));return o;}

// r066: UI/resource parent 0x443110 and table dispatcher 0x4432B0. The three
// still-open resource routines are replaced only at their parent call sites by
// deterministic trace stubs; already-closed UI/query children execute normally.
constexpr std::uint32_t R066StubBlock=0x32029000u;
constexpr std::uint32_t R066StateBase=0x3308a000u;
constexpr std::uint32_t R066ObjectBase=0x3f160000u;
constexpr std::size_t R066ObjectSize=0x2000u;
constexpr std::size_t R066MapSize=0x10000u;
struct R066ConfigRec{std::uint32_t count{},this_ptr{};std::array<std::uint32_t,11> args{};};
struct R066VoidRec{std::uint32_t count{},this_ptr{};};
struct R066OpenTrace{std::array<R066ConfigRec,4> config{};std::array<R066VoidRec,6> call{};};
void map_r066_ui_dispatch(){
    auto* code=static_cast<std::uint8_t*>(map_at(R066StubBlock,4096,PROT_READ|PROT_WRITE));
    map_at(R066StateBase,4096,PROT_READ|PROT_WRITE);map_at(R066ObjectBase,R066MapSize,PROT_READ|PROT_WRITE);
    auto config_stub=[&](std::size_t off,std::uint32_t state){
        std::vector<std::uint8_t>b;r060_inc(b,state+0u);b.insert(b.end(),{0x89,0x0d});r060_emit32(b,state+4u);
        for(unsigned k=0;k<11u;++k){b.insert(b.end(),{0x8b,0x44,0x24,std::uint8_t(4u+k*4u)});b.push_back(0xa3);r060_emit32(b,state+8u+k*4u);}
        b.insert(b.end(),{0xc2,0x2c,0x00});std::memcpy(code+off,b.data(),b.size());
    };
    auto void_stub=[&](std::size_t off,std::uint32_t state){std::vector<std::uint8_t>b;r060_inc(b,state);b.insert(b.end(),{0x89,0x0d});r060_emit32(b,state+4u);b.push_back(0xc3);std::memcpy(code+off,b.data(),b.size());};
    for(unsigned k=0;k<4u;++k)config_stub(k*0x100u,R066StateBase+k*0x40u);
    for(unsigned k=0;k<6u;++k)void_stub(0x500u+k*0x20u,R066StateBase+0x100u+k*8u);
    if(mprotect(code,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("r066 stub mprotect");
}
void r066_check_rel32(std::uint32_t site,std::uint8_t opcode,std::uint32_t target){
    auto* q=reinterpret_cast<const std::uint8_t*>(site);std::int32_t rel{};std::memcpy(&rel,q+1,4);
    if(q[0]!=opcode||std::uint32_t(std::int64_t(site+5u)+rel)!=target)throw std::runtime_error("r066 original rel32 target changed");
}
void set_r066_patches(bool enabled){
    struct P{std::uint32_t site;std::uint8_t op;std::uint32_t target,stub;};
    static constexpr P p[]={
        {0x443168u,0xe8,0x465860u,R066StubBlock+0x000u},{0x4431e6u,0xe8,0x465860u,R066StubBlock+0x100u},
        {0x443243u,0xe8,0x465860u,R066StubBlock+0x200u},{0x44327cu,0xe8,0x465860u,R066StubBlock+0x300u},
        {0x44316fu,0xe8,0x465970u,R066StubBlock+0x500u},{0x443184u,0xe8,0x4659f0u,R066StubBlock+0x520u},
        {0x443191u,0xe8,0x4659f0u,R066StubBlock+0x540u},{0x4431edu,0xe8,0x465970u,R066StubBlock+0x560u},
        {0x44324au,0xe8,0x465970u,R066StubBlock+0x580u},{0x443286u,0xe9,0x465970u,R066StubBlock+0x5a0u}
    };
    for(const auto& x:p){if(enabled)r066_check_rel32(x.site,x.op,x.target);r046_write_rel32(x.site,x.op,enabled?x.stub:x.target);}
}
bool r066_oracle_name(const std::string& q){return q=="object_ui_state_update_443110_r066"||q=="object_event_dispatch_4432b0_r066"||q=="r066_ui_dispatch_batch";}
void reset_r066_trace(){std::memset(reinterpret_cast<void*>(R066StateBase),0,0x140u);}
std::array<std::uint8_t,0x140> r066_guest_trace_blob(){std::array<std::uint8_t,0x140> o{};std::memcpy(o.data(),reinterpret_cast<void*>(R066StateBase),o.size());return o;}
std::array<std::uint8_t,0x140> r066_native_trace_blob(const R066OpenTrace& t){
    std::array<std::uint8_t,0x140> o{};for(unsigned i=0;i<4u;++i){const std::size_t b=i*0x40u;std::memcpy(o.data()+b,&t.config[i].count,4);std::memcpy(o.data()+b+4u,&t.config[i].this_ptr,4);std::memcpy(o.data()+b+8u,t.config[i].args.data(),44u);}for(unsigned i=0;i<6u;++i){const std::size_t b=0x100u+i*8u;std::memcpy(o.data()+b,&t.call[i].count,4);std::memcpy(o.data()+b+4u,&t.call[i].this_ptr,4);}return o;
}
int r066_config_index(std::uint32_t site){switch(site){case 0x443168u:return 0;case 0x4431e6u:return 1;case 0x443243u:return 2;case 0x44327cu:return 3;default:return -1;}}
int r066_void_index(std::uint32_t site){switch(site){case 0x44316fu:return 0;case 0x443184u:return 1;case 0x443191u:return 2;case 0x4431edu:return 3;case 0x44324au:return 4;case 0x443286u:return 5;default:return -1;}}
void r066_native_config(void* u,std::uint32_t site,std::uint32_t pc,Bytes,std::size_t off,const std::array<std::uint32_t,11>& a){auto& t=*static_cast<R066OpenTrace*>(u);if(pc!=0x465860u)throw std::runtime_error("r066 config target");const int n=r066_config_index(site);if(n<0)throw std::runtime_error("r066 config site");auto& r=t.config[unsigned(n)];++r.count;r.this_ptr=R066ObjectBase+std::uint32_t(off);r.args=a;}
void r066_native_void(void* u,std::uint32_t site,std::uint32_t pc,Bytes,std::size_t off){auto& t=*static_cast<R066OpenTrace*>(u);const int n=r066_void_index(site);if(n<0)throw std::runtime_error("r066 void site");if((n==1||n==2)?pc!=0x4659f0u:pc!=0x465970u)throw std::runtime_error("r066 void target");auto& r=t.call[unsigned(n)];++r.count;r.this_ptr=R066ObjectBase+std::uint32_t(off);}
PcObjectUiOpenServices r066_open_services(R066OpenTrace& t){return {&t,r066_native_void,r066_native_config};}
std::array<std::uint8_t,R066ObjectSize> r066_object_fixture(unsigned n){std::array<std::uint8_t,R066ObjectSize> o{};std::mt19937 g(0x44311066u^(n*2654435761u));for(auto& b:o)b=std::uint8_t(g());return o;}
void r066_set_resource(Bytes b,std::size_t off,std::uint32_t handle,std::int32_t busy){b.put32(off+8u,handle);b.puti(off+0x24u,busy);b.put32(off+0x28u,0u);b.put32(off+0x2cu,0u);}

// r067: event callback dispatcher 0x443420 plus the two normal-path factories
// at 0x4434C0 / 0x443520.  The 83-entry callback table is runtime data, so the
// oracle populates it with deterministic synthetic keys/callback stubs.
constexpr std::uint32_t R067StubBlock=0x3202a000u;
constexpr std::uint32_t R067StateBase=0x3308b000u;
constexpr std::uint32_t R067ObjectBase=0x3f170000u;
constexpr std::size_t R067ObjectSize=0x2000u;
constexpr std::size_t R067MapSize=0x10000u;
constexpr std::uint32_t R067CallbackTableBase=0x7b11d8u;
static std::array<std::uint8_t,PcObjectEventCallbackCount*8u> R067OriginalCallbackTable{};
static bool R067OriginalCallbackTableSaved=false;
struct R067Trace{std::uint32_t callback_ret{0x67abcdefu},push_n{},push_this{},set_n{},set_this{},set_arg{},cb_n{},cb_index{};};
static const R060FactorySpec R067Factories[]={
    {"object_factory_4434c0_r067",0x4434c0u,0x2980u,0x4429b0u,object_factory_4434c0},
    {"object_factory_443520_r067",0x443520u,0x1320u,0x442a60u,object_factory_443520}
};
void map_r067_event_dispatch(){
    auto* code=static_cast<std::uint8_t*>(map_at(R067StubBlock,4096,PROT_READ|PROT_WRITE));
    map_at(R067StateBase,4096,PROT_READ|PROT_WRITE);map_at(R067ObjectBase,R067MapSize,PROT_READ|PROT_WRITE);
    if(mprotect(reinterpret_cast<void*>(R067CallbackTableBase&~0xfffu),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("r067 callback-table mprotect");
    std::memcpy(R067OriginalCallbackTable.data(),reinterpret_cast<void*>(R067CallbackTableBase),R067OriginalCallbackTable.size());R067OriginalCallbackTableSaved=true;
    // 446FC0(this): trace ECX and return.
    {std::vector<std::uint8_t>b;r060_inc(b,R067StateBase+0x04u);b.insert(b.end(),{0x89,0x0d});r060_emit32(b,R067StateBase+0x08u);b.push_back(0xc3);std::memcpy(code+0x000u,b.data(),b.size());}
    // 446EA0(this,key): trace ECX/key and callee-pop 4 bytes.
    {std::vector<std::uint8_t>b;r060_inc(b,R067StateBase+0x0cu);b.insert(b.end(),{0x89,0x0d});r060_emit32(b,R067StateBase+0x10u);b.insert(b.end(),{0x8b,0x44,0x24,0x04,0xa3});r060_emit32(b,R067StateBase+0x14u);b.insert(b.end(),{0xc2,0x04,0x00});std::memcpy(code+0x080u,b.data(),b.size());}
    // One no-argument callback stub per table slot; record selected index and
    // return scripted EAX.  0x20-byte stride keeps stubs easy to audit.
    for(unsigned i=0;i<PcObjectEventCallbackCount;++i){
        std::vector<std::uint8_t>b;r060_inc(b,R067StateBase+0x18u);b.push_back(0xb8);r060_emit32(b,i);b.push_back(0xa3);r060_emit32(b,R067StateBase+0x1cu);b.push_back(0xa1);r060_emit32(b,R067StateBase+0x00u);b.push_back(0xc3);
        const std::size_t off=0x200u+std::size_t(i)*0x20u;if(off+b.size()>4096u)throw std::runtime_error("r067 callback stub overflow");std::memcpy(code+off,b.data(),b.size());
    }
    if(mprotect(code,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("r067 stub mprotect");
}
void r067_fill_callback_table(){
    for(unsigned i=0;i<PcObjectEventCallbackCount;++i){
        *reinterpret_cast<std::uint32_t*>(R067CallbackTableBase+i*8u)=i;
        *reinterpret_cast<std::uint32_t*>(R067CallbackTableBase+i*8u+4u)=R067StubBlock+0x200u+i*0x20u;
    }
}
void r067_restore_callback_table(){
    if(!R067OriginalCallbackTableSaved)throw std::runtime_error("r067 callback table not saved");
    std::memcpy(reinterpret_cast<void*>(R067CallbackTableBase),R067OriginalCallbackTable.data(),R067OriginalCallbackTable.size());
}
void set_r067_patches(bool enabled){
    static const std::uint8_t fs_load[6]={0x64,0xa1,0,0,0,0};
    static const std::uint8_t fs_set_esp[7]={0x64,0x89,0x25,0,0,0,0};
    static const std::uint8_t fs_set_ecx[7]={0x64,0x89,0x0d,0,0,0,0};
    static const std::uint8_t zero_eax[6]={0x31,0xc0,0x90,0x90,0x90,0x90};
    static const std::uint8_t nops7[7]={0x90,0x90,0x90,0x90,0x90,0x90,0x90};
    if(enabled){r060_check_call(0x44348du,0x446fc0u);r060_check_call(0x443499u,0x446ea0u);}
    r046_write_rel32(0x44348du,0xe8,enabled?R067StubBlock+0x000u:0x446fc0u);
    r046_write_rel32(0x443499u,0xe8,enabled?R067StubBlock+0x080u:0x446ea0u);
    for(const auto& f:R067Factories){
        if(enabled){r060_check_call(f.start+0x1bu,0x5802cfu);r060_check_call(f.start+0x34u,f.ctor);}
        r046_write_rel32(f.start+0x1bu,0xe8,enabled?R060StubBlock+0x000u:0x5802cfu);
        r046_write_rel32(f.start+0x34u,0xe8,enabled?R060StubBlock+0x080u:f.ctor);
        r060_raw_write(f.start+0x07u,enabled?zero_eax:fs_load,6);
        r060_raw_write(f.start+0x0eu,enabled?nops7:fs_set_esp,7);
        r060_raw_write(f.start+0x3du,enabled?nops7:fs_set_ecx,7);
        r060_raw_write(f.start+0x4eu,enabled?nops7:fs_set_ecx,7);
    }
}
bool r067_oracle_name(const std::string& q){return q=="object_dispatch_callback_443420_r067"||q=="object_factory_4434c0_r067"||q=="object_factory_443520_r067"||q=="r067_event_factory_batch";}
void reset_r067_trace(std::uint32_t ret){std::memset(reinterpret_cast<void*>(R067StateBase),0,0x40u);*reinterpret_cast<std::uint32_t*>(R067StateBase+0x00u)=ret;}
std::array<std::uint8_t,0x20> r067_guest_trace_blob(){std::array<std::uint8_t,0x20> o{};std::memcpy(o.data(),reinterpret_cast<void*>(R067StateBase),o.size());return o;}
void r067_native_push(void* u,std::uint32_t pc,Bytes,std::size_t off){auto& t=*static_cast<R067Trace*>(u);if(pc!=0x446fc0u)throw std::runtime_error("r067 push target");++t.push_n;t.push_this=R067ObjectBase+std::uint32_t(off);}
void r067_native_set(void* u,std::uint32_t pc,Bytes,std::size_t off,std::uint32_t arg){auto& t=*static_cast<R067Trace*>(u);if(pc!=0x446ea0u)throw std::runtime_error("r067 set target");++t.set_n;t.set_this=R067ObjectBase+std::uint32_t(off);t.set_arg=arg;}
std::uint32_t r067_native_callback(void* u,std::uint32_t,std::size_t idx){auto& t=*static_cast<R067Trace*>(u);++t.cb_n;t.cb_index=std::uint32_t(idx);return t.callback_ret;}
PcObjectEventDispatchServices r067_services(R067Trace& t){return {&t,r067_native_push,r067_native_set,r067_native_callback};}
std::array<std::uint8_t,0x20> r067_trace_blob(const R067Trace& t){std::array<std::uint8_t,0x20> o{};const std::uint32_t v[8]={t.callback_ret,t.push_n,t.push_this,t.set_n,t.set_this,t.set_arg,t.cb_n,t.cb_index};std::memcpy(o.data(),v,sizeof(v));return o;}
std::array<std::uint8_t,R067ObjectSize> r067_object_fixture(unsigned n){std::array<std::uint8_t,R067ObjectSize> o{};std::mt19937 g(0x44342067u^(n*2654435761u));for(auto& b:o)b=std::uint8_t(g());return o;}
std::array<PcObjectEventCallbackEntry,PcObjectEventCallbackCount> r067_native_callback_table(){std::array<PcObjectEventCallbackEntry,PcObjectEventCallbackCount> t{};for(unsigned i=0;i<t.size();++i)t[i]={i,R067StubBlock+0x200u+i*0x20u};return t;}

// r068: paired deleting-destructor wrappers 0x443580/0x443640 and bodies
// 0x4435A0/0x443660.  Every child destructor and delete call is patched to a
// trace stub so parent-owned order/arguments can be compared without counting
// child bodies transitively.
constexpr std::uint32_t R068StubBlock=0x3202b000u;
constexpr std::uint32_t R068StateBase=0x3308c000u;
constexpr std::uint32_t R068ObjectBase=0x3f180000u;
constexpr std::size_t R068ObjectSize=0x3000u;
struct R068Rec{std::uint32_t seq{},pc{},ptr{},elem{},count{},dtor{};};
struct R068Trace{std::uint32_t seq{};std::array<R068Rec,10> rec{};};
void r068_prefix(std::vector<std::uint8_t>& b,std::uint32_t target){
    r060_inc(b,R068StateBase);
    b.insert(b.end(),{0x8b,0x15});r060_emit32(b,R068StateBase); // mov edx,[seq]
    b.insert(b.end(),{0x4a,0xc1,0xe2,0x05,0x81,0xc2});r060_emit32(b,R068StateBase+0x20u); // --edx; shl 5; add base
    b.insert(b.end(),{0xa1});r060_emit32(b,R068StateBase);b.insert(b.end(),{0x89,0x02}); // seq
    b.insert(b.end(),{0xc7,0x42,0x04});r060_emit32(b,target);
}
void r068_store_stack(std::vector<std::uint8_t>& b,std::uint8_t disp,std::uint8_t field){b.insert(b.end(),{0x8b,0x44,0x24,disp,0x89,0x42,field});}
void map_r068_destructor_stubs(){
    auto* code=static_cast<std::uint8_t*>(map_at(R068StubBlock,4096,PROT_READ|PROT_WRITE));map_at(R068StateBase,4096,PROT_READ|PROT_WRITE);map_at(R068ObjectBase,0x4000,PROT_READ|PROT_WRITE);
    auto vec=[&](std::size_t off,std::uint32_t target){std::vector<std::uint8_t>b;r068_prefix(b,target);r068_store_stack(b,4,8);r068_store_stack(b,8,12);r068_store_stack(b,12,16);r068_store_stack(b,16,20);b.insert(b.end(),{0xc2,0x10,0x00});std::memcpy(code+off,b.data(),b.size());};
    auto th=[&](std::size_t off,std::uint32_t target){std::vector<std::uint8_t>b;r068_prefix(b,target);b.insert(b.end(),{0x89,0x4a,0x08,0xc3});std::memcpy(code+off,b.data(),b.size());};
    auto fr=[&](std::size_t off,std::uint32_t target){std::vector<std::uint8_t>b;r068_prefix(b,target);r068_store_stack(b,4,8);b.push_back(0xc3);std::memcpy(code+off,b.data(),b.size());};
    vec(0x000u,0x58165du);vec(0x100u,0x58165du);th(0x200u,0x48c520u);th(0x240u,0x4edab0u);th(0x280u,0x48f4d0u);fr(0x2c0u,0x5801a7u);
    th(0x300u,0x48c520u);th(0x340u,0x4edab0u);th(0x380u,0x48f4d0u);fr(0x3c0u,0x5801a7u);
    if(mprotect(code,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("r068 stub mprotect");
}
void set_r068_patches(bool enabled){
    struct P{std::uint32_t site,target,stub;};static constexpr P p[]={
        {0x4435d8u,0x58165du,R068StubBlock+0x000u},{0x4435f5u,0x58165du,R068StubBlock+0x100u},
        {0x443605u,0x48c520u,R068StubBlock+0x200u},{0x443615u,0x4edab0u,R068StubBlock+0x240u},{0x443624u,0x48f4d0u,R068StubBlock+0x280u},{0x443590u,0x5801a7u,R068StubBlock+0x2c0u},
        {0x443688u,0x48c520u,R068StubBlock+0x300u},{0x443695u,0x4edab0u,R068StubBlock+0x340u},{0x4436a4u,0x48f4d0u,R068StubBlock+0x380u},{0x443650u,0x5801a7u,R068StubBlock+0x3c0u}};
    for(const auto& x:p){if(enabled)r060_check_call(x.site,x.target);r046_write_rel32(x.site,0xe8,enabled?x.stub:x.target);}
    static const std::uint8_t fs_load[6]={0x64,0xa1,0,0,0,0},zero_eax[6]={0x31,0xc0,0x90,0x90,0x90,0x90};
    static const std::uint8_t fs_set_esp[7]={0x64,0x89,0x25,0,0,0,0},fs_set_ecx[7]={0x64,0x89,0x0d,0,0,0,0},nops7[7]={0x90,0x90,0x90,0x90,0x90,0x90,0x90};
    for(const auto& q:std::array<std::array<std::uint32_t,2>,2>{{{{0x4435a0u,0x44362eu}},{{0x443660u,0x4436aeu}}}}){
        r060_raw_write(q[0]+0x07u,enabled?zero_eax:fs_load,6);r060_raw_write(q[0]+0x0eu,enabled?nops7:fs_set_esp,7);r060_raw_write(q[1],enabled?nops7:fs_set_ecx,7);
    }
}
bool r068_oracle_name(const std::string& q){return q=="object_destroy_443580_r068"||q=="object_destroy_body_4435a0_r068"||q=="object_destroy_443640_r068"||q=="object_destroy_body_443660_r068"||q=="r068_destructor_batch";}
void reset_r068_trace(){std::memset(reinterpret_cast<void*>(R068StateBase),0,0x180u);}
std::array<std::uint8_t,0x180> r068_guest_trace_blob(){std::array<std::uint8_t,0x180> o{};std::memcpy(o.data(),reinterpret_cast<void*>(R068StateBase),o.size());return o;}
void r068_native_add(R068Trace& t,std::uint32_t pc,std::uint32_t ptr,std::uint32_t elem=0,std::uint32_t count=0,std::uint32_t dtor=0){if(t.seq>=t.rec.size())throw std::runtime_error("r068 native trace overflow");auto& r=t.rec[t.seq];r={++t.seq,pc,ptr,elem,count,dtor};}
void r068_native_this(void* u,std::uint32_t pc,Bytes,std::size_t off){auto& t=*static_cast<R068Trace*>(u);r068_native_add(t,pc,R068ObjectBase+std::uint32_t(off));}
void r068_native_vec(void* u,std::uint32_t pc,Bytes,std::size_t off,std::uint32_t elem,std::uint32_t count,std::uint32_t dtor){auto& t=*static_cast<R068Trace*>(u);r068_native_add(t,pc,R068ObjectBase+std::uint32_t(off),elem,count,dtor);}
void r068_native_free(void* u,std::uint32_t pc,std::uint32_t token){auto& t=*static_cast<R068Trace*>(u);r068_native_add(t,pc,token);}
PcObjectDestroyServices r068_services(R068Trace& t){return {&t,r068_native_this,r068_native_vec,r068_native_free};}
std::array<std::uint8_t,0x180> r068_trace_blob(const R068Trace& t){std::array<std::uint8_t,0x180> o{};std::memcpy(o.data(),&t.seq,4);for(std::size_t i=0;i<t.rec.size();++i)std::memcpy(o.data()+0x20u+i*0x20u,&t.rec[i],sizeof(R068Rec));return o;}
std::array<std::uint8_t,R068ObjectSize> r068_object_fixture(unsigned n){std::array<std::uint8_t,R068ObjectSize> o{};std::mt19937 g(0x44358068u^(n*2654435761u));for(auto& b:o)b=std::uint8_t(g());return o;}

// r069: callback-table / runtime-object initializer 0x4436C0..0x443C26.
// The only still-open child is the embedded 0x4470F0 initializer; patch it to
// a trace-only thiscall stub.  The already-closed 0x465250 resource reset runs
// as original x86, with fixtures forcing its handle to -1 so no external leaf
// can escape this oracle.  The whole 83-entry global table is compared.
constexpr std::uint32_t R069StubBlock=0x3202c000u;
constexpr std::uint32_t R069StateBase=0x3308e000u;
constexpr std::uint32_t R069ObjectBase=0x3f1c0000u;
constexpr std::size_t R069ObjectSize=0x2000u;
constexpr std::uint32_t R069TableBase=0x007b11d8u;
constexpr std::size_t R069TableBytes=PcObjectEventCallbackCount*sizeof(PcObjectEventCallbackEntry);
void map_r069_runtime_init(){
    auto* code=static_cast<std::uint8_t*>(map_at(R069StubBlock,4096,PROT_READ|PROT_WRITE));
    map_at(R069StateBase,4096,PROT_READ|PROT_WRITE);map_at(R069ObjectBase,0x2000,PROT_READ|PROT_WRITE);
    std::vector<std::uint8_t>b;r060_inc(b,R069StateBase+0x00u);
    b.insert(b.end(),{0xc7,0x05});r060_emit32(b,R069StateBase+0x04u);r060_emit32(b,0x4470f0u);
    b.insert(b.end(),{0x89,0x0d});r060_emit32(b,R069StateBase+0x08u);b.push_back(0xc3);
    std::memcpy(code,b.data(),b.size());if(mprotect(code,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("r069 stub mprotect");
}
void set_r069_patch(bool enabled){
    if(enabled)r060_check_call(0x443b9fu,0x4470f0u);
    r046_write_rel32(0x443b9fu,0xe8,enabled?R069StubBlock:0x4470f0u);
}
bool r069_oracle_name(const std::string& q){return q=="object_runtime_init_4436c0_r069"||q=="r069_runtime_init_batch";}
struct R069Trace{std::uint32_t count{},pc{},ptr{};};
void reset_r069_trace(){std::memset(reinterpret_cast<void*>(R069StateBase),0,0x20u);}
std::array<std::uint8_t,12> r069_guest_trace_blob(){std::array<std::uint8_t,12> o{};std::memcpy(o.data(),reinterpret_cast<void*>(R069StateBase),o.size());return o;}
void r069_native_embedded(void* u,std::uint32_t pc,Bytes,std::size_t off){auto& t=*static_cast<R069Trace*>(u);++t.count;t.pc=pc;t.ptr=R069ObjectBase+std::uint32_t(off);}
std::array<std::uint8_t,12> r069_trace_blob(const R069Trace& t){std::array<std::uint8_t,12> o{};std::memcpy(o.data(),&t,sizeof(t));return o;}
std::array<std::uint8_t,R069ObjectSize> r069_object_fixture(unsigned n){
    std::array<std::uint8_t,R069ObjectSize> o{};std::mt19937 g(0x4436c069u^(n*2654435761u));for(auto& b:o)b=std::uint8_t(g());
    Bytes x(o.data(),o.size());x.put32(0x0ba8u+0x08u,0xffffffffu);return o;
}
std::array<PcObjectEventCallbackEntry,PcObjectEventCallbackCount> r069_table_fixture(unsigned n){
    std::array<PcObjectEventCallbackEntry,PcObjectEventCallbackCount> t{};std::mt19937 g(0x7b11d869u^(n*2246822519u));
    for(auto& e:t){e.key=g();e.callback_token=g();}return t;
}

// r070: runtime teardown/reset 0x443C30..0x443E87.  The original entry
// contains a protected indirect transfer at 0x443C38; the oracle replaces only
// those six bytes with NOPs so execution reaches the mapped linear continuation
// at 0x443C3E.  Open runtime/global calls and handle virtuals are traced.
constexpr std::uint32_t R070StubBlock=0x3202d000u;
constexpr std::uint32_t R070StateBase=0x33090000u;
constexpr std::uint32_t R070ObjectBase=0x3f200000u;
constexpr std::uint32_t R070HandleBase=0x3f204000u;
constexpr std::uint32_t R070VtableBase=0x3f205000u;
constexpr std::size_t R070ObjectSize=0x2000u;
struct R070Rec{std::uint32_t seq{},pc{},a{},b{},kind{},ret{};};
struct R070Trace{std::uint32_t seq{};std::array<R070Rec,24> rec{};};
void r070_prefix(std::vector<std::uint8_t>& b,std::uint32_t pc,std::uint32_t kind){
    r060_inc(b,R070StateBase);
    b.insert(b.end(),{0x8b,0x15});r060_emit32(b,R070StateBase); // mov edx,[seq]
    b.insert(b.end(),{0x4a,0x6b,0xd2,0x18,0x81,0xc2});r060_emit32(b,R070StateBase+0x20u);
    b.push_back(0xa1);r060_emit32(b,R070StateBase);b.insert(b.end(),{0x89,0x02});
    b.insert(b.end(),{0xc7,0x42,0x04});r060_emit32(b,pc);
    b.insert(b.end(),{0xc7,0x42,0x10});r060_emit32(b,kind);
}
void map_r070_teardown(){
    auto* code=static_cast<std::uint8_t*>(map_at(R070StubBlock,4096,PROT_READ|PROT_WRITE));
    map_at(R070StateBase,4096,PROT_READ|PROT_WRITE);map_at(R070ObjectBase,0x2000,PROT_READ|PROT_WRITE);
    map_at(R070HandleBase,4096,PROT_READ|PROT_WRITE);map_at(R070VtableBase,4096,PROT_READ|PROT_WRITE);
    auto put=[&](std::size_t off,const std::vector<std::uint8_t>& b){std::memcpy(code+off,b.data(),b.size());};
    // 447090(this): trace owning object pointer.
    {std::vector<std::uint8_t>b;r070_prefix(b,0x447090u,0u);b.insert(b.end(),{0x89,0x4a,0x08,0x31,0xc0,0x89,0x42,0x0c,0x89,0x42,0x14,0xc3});put(0x000,b);}
    // virtual +0x10(this): no stack argument.
    {std::vector<std::uint8_t>b;r070_prefix(b,0x10u,1u);b.insert(b.end(),{0x89,0x4a,0x08,0x31,0xc0,0x89,0x42,0x0c,0x89,0x42,0x14,0xc3});put(0x100,b);}
    // virtual +0(this,1): callee pops one argument.
    {std::vector<std::uint8_t>b;r070_prefix(b,0x00u,1u);b.insert(b.end(),{0x89,0x4a,0x08,0x8b,0x44,0x24,0x04,0x89,0x42,0x0c,0x31,0xc0,0x89,0x42,0x14,0xc2,0x04,0x00});put(0x180,b);}
    // 428600(): no arg.
    {std::vector<std::uint8_t>b;r070_prefix(b,0x428600u,2u);b.insert(b.end(),{0x31,0xc0,0x89,0x42,0x08,0x89,0x42,0x0c,0x89,0x42,0x14,0xc3});put(0x200,b);}
    auto one_arg=[&](std::size_t off,std::uint32_t pc,bool final){std::vector<std::uint8_t>b;r070_prefix(b,pc,2u);b.insert(b.end(),{0x8b,0x44,0x24,0x04,0x89,0x42,0x08,0xc7,0x42,0x0c,0x01,0x00,0x00,0x00});if(final){b.insert(b.end(),{0x05,0x00,0x00,0x00,0x70,0x89,0x42,0x14,0xc3});}else{b.insert(b.end(),{0x31,0xc0,0x89,0x42,0x14,0xc3});}put(off,b);};
    one_arg(0x280,0x4299c0u,false);one_arg(0x300,0x42dfb0u,true);
    if(mprotect(code,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("r070 stub mprotect");
    *reinterpret_cast<std::uint32_t*>(R070VtableBase+0x00u)=R070StubBlock+0x180u;
    *reinterpret_cast<std::uint32_t*>(R070VtableBase+0x10u)=R070StubBlock+0x100u;
    for(unsigned i=0;i<16u;++i)*reinterpret_cast<std::uint32_t*>(R070HandleBase+i*0x20u)=R070VtableBase;
}
void set_r070_patches(bool enabled){
    static const std::uint8_t original_jump[6]={0xff,0x25,0xe0,0x99,0x03,0x01};
    static const std::uint8_t nops[6]={0x90,0x90,0x90,0x90,0x90,0x90};
    const auto* orig=gate_original(0x443c38u,original_jump,6);
    if(enabled&&std::memcmp(reinterpret_cast<void*>(0x443c38u),orig,6)!=0)throw std::runtime_error("r070 protected transfer changed");
    r060_raw_write(0x443c38u,enabled?nops:orig,6);
    struct P{std::uint32_t site,target,stub;};static constexpr P ps[]={
        {0x443c3eu,0x447090u,R070StubBlock+0x000u},{0x443e5du,0x428600u,R070StubBlock+0x200u},
        {0x443e64u,0x4299c0u,R070StubBlock+0x280u},{0x443e6bu,0x4299c0u,R070StubBlock+0x280u},
        {0x443e72u,0x42dfb0u,R070StubBlock+0x300u},{0x443e79u,0x42dfb0u,R070StubBlock+0x300u}};
    for(const auto& x:ps){if(enabled)r060_check_call(x.site,x.target);r046_write_rel32(x.site,0xe8,enabled?x.stub:x.target);}
}
bool r070_oracle_name(const std::string& q){return q=="object_runtime_teardown_443c30_r070"||q=="r070_runtime_teardown_batch";}
void reset_r070_trace(){std::memset(reinterpret_cast<void*>(R070StateBase),0,0x280u);}
std::array<std::uint8_t,0x260> r070_guest_trace_blob(){std::array<std::uint8_t,0x260> o{};std::memcpy(o.data(),reinterpret_cast<void*>(R070StateBase),o.size());return o;}
void r070_native_add(R070Trace& t,std::uint32_t pc,std::uint32_t a,std::uint32_t b,std::uint32_t kind,std::uint32_t ret){if(t.seq>=t.rec.size())throw std::runtime_error("r070 native trace overflow");t.rec[t.seq]={t.seq+1u,pc,a,b,kind,ret};++t.seq;}
void r070_native_reset(void* u,std::uint32_t pc,Bytes,std::size_t off){r070_native_add(*static_cast<R070Trace*>(u),pc,R070ObjectBase+std::uint32_t(off),0u,0u,0u);}
std::uint32_t r070_native_handle(void* u,std::uint32_t slot,std::uint32_t h,std::uint32_t arg){r070_native_add(*static_cast<R070Trace*>(u),slot,h,arg,1u,0u);return 0u;}
std::uint32_t r070_native_global(void* u,std::uint32_t pc,std::uint32_t arg,bool has){const auto ret=(pc==0x42dfb0u)?(0x70000000u+arg):0u;r070_native_add(*static_cast<R070Trace*>(u),pc,arg,has?1u:0u,2u,ret);return ret;}
PcObjectRuntimeTeardownServices r070_services(R070Trace& t){PcUiNotifyServices ui{};return {&t,r070_native_reset,r070_native_handle,r070_native_global,ui};}
std::array<std::uint8_t,0x260> r070_trace_blob(const R070Trace& t){std::array<std::uint8_t,0x260> o{};std::memcpy(o.data(),&t.seq,4);for(std::size_t i=0;i<t.rec.size();++i)std::memcpy(o.data()+0x20u+i*sizeof(R070Rec),&t.rec[i],sizeof(R070Rec));return o;}
std::array<std::uint8_t,R070ObjectSize> r070_object_fixture(unsigned n){
    std::array<std::uint8_t,R070ObjectSize> o{};std::mt19937 g(0x443c3070u^(n*2654435761u));for(auto& b:o)b=std::uint8_t(g());Bytes x(o.data(),o.size());
    const auto h=[&](unsigned salt)->std::uint32_t{return ((n+salt)%3u)==0u?0u:(R070HandleBase+((n+salt)%12u)*0x20u);};
    const std::uint32_t c0=n%5u,c1=(n/3u)%5u;x.put32(0x484u,c0);x.put32(0x518u,c1);
    for(unsigned i=0;i<5u;++i){x.put32(0x284u+i*4u,h(i+1u));x.put32(0x498u+i*4u,h(i+11u));}
    x.put8(0x494u,std::uint8_t((n>>1)&1u));x.put32(0x490u,h(21u));x.put8(0x48cu,std::uint8_t((n>>2)&1u));x.put32(0x488u,h(31u));
    x.put32(0xba8u+8u,0xffffffffu);x.put32(0xcf4u+8u,0xffffffffu);return o;
}


// r071: second callback dispatcher 0x443EB0, handle-opening wrapper 0x443FA0
// and compact state-code dispatcher 0x443FF0 with local jump/class tables.
constexpr std::uint32_t R071StubBlock=0x3202e000u;
constexpr std::uint32_t R071StateBase=0x33091000u;
constexpr std::uint32_t R071ObjectBase=0x3f220000u;
constexpr std::uint32_t R071RelatedBase=0x3f224000u;
constexpr std::uint32_t R071HandleBase=0x3f228000u;
constexpr std::uint32_t R071VtableBase=0x3f229000u;
constexpr std::size_t R071ObjectSize=0x2000u;
constexpr std::uint32_t R071CallbackTableBase=0x7b11d8u;
static std::array<std::uint8_t,PcObjectEventCallbackCount*8u> R071OriginalCallbackTable{};
static bool R071OriginalCallbackTableSaved=false;
struct R071Trace{
    std::uint32_t callback_ret{},ready_ret{},release_index{};
    std::uint32_t push_n{},push_this{},set_n{},set_this{},set_arg{};
    std::uint32_t state_n{},state_arg{},pause_n{},cb_n{},cb_index{};
    std::uint32_t ready_n{},ready_handle{},ready_slot{};
    std::uint32_t rel10_n{},rel10_handle{},rel0_n{},rel0_handle{},rel0_arg{},reli8_n{},resolver_n{};
};
void map_r071_event_open(){
    auto* code=static_cast<std::uint8_t*>(map_at(R071StubBlock,4096,PROT_READ|PROT_WRITE));
    map_at(R071StateBase,4096,PROT_READ|PROT_WRITE);map_at(R071ObjectBase,0x3000,PROT_READ|PROT_WRITE);
    map_at(R071RelatedBase,4096,PROT_READ|PROT_WRITE);map_at(R071HandleBase,4096,PROT_READ|PROT_WRITE);map_at(R071VtableBase,4096,PROT_READ|PROT_WRITE);
    if(mprotect(reinterpret_cast<void*>(R071CallbackTableBase&~0xfffu),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("r071 callback table mprotect");
    std::memcpy(R071OriginalCallbackTable.data(),reinterpret_cast<void*>(R071CallbackTableBase),R071OriginalCallbackTable.size());R071OriginalCallbackTableSaved=true;
    auto put=[&](std::size_t off,const std::vector<std::uint8_t>& b){if(off+b.size()>4096u)throw std::runtime_error("r071 stub overflow");std::memcpy(code+off,b.data(),b.size());};
    // 446FC0(this)
    {std::vector<std::uint8_t>b;r060_inc(b,R071StateBase+0x0cu);b.insert(b.end(),{0x89,0x0d});r060_emit32(b,R071StateBase+0x10u);b.push_back(0xc3);put(0x000,b);}
    // 446EA0(this,key)
    {std::vector<std::uint8_t>b;r060_inc(b,R071StateBase+0x14u);b.insert(b.end(),{0x89,0x0d});r060_emit32(b,R071StateBase+0x18u);b.insert(b.end(),{0x8b,0x44,0x24,0x04,0xa3});r060_emit32(b,R071StateBase+0x1cu);b.insert(b.end(),{0xc2,0x04,0x00});put(0x080,b);}
    // 43F9E0(1): cdecl, caller cleans.
    {std::vector<std::uint8_t>b;r060_inc(b,R071StateBase+0x20u);b.insert(b.end(),{0x8b,0x44,0x24,0x04,0xa3});r060_emit32(b,R071StateBase+0x24u);b.push_back(0xc3);put(0x100,b);}
    // 440930(): pause boundary.
    {std::vector<std::uint8_t>b;r060_inc(b,R071StateBase+0x28u);b.push_back(0xc3);put(0x140,b);}
    // virtual +4(this): return scripted dword.
    {std::vector<std::uint8_t>b;r060_inc(b,R071StateBase+0x34u);b.insert(b.end(),{0x89,0x0d});r060_emit32(b,R071StateBase+0x38u);b.insert(b.end(),{0xc7,0x05});r060_emit32(b,R071StateBase+0x3cu);r060_emit32(b,4u);b.push_back(0xa1);r060_emit32(b,R071StateBase+0x04u);b.push_back(0xc3);put(0x180,b);}
    // release virtual +0x10(this)
    {std::vector<std::uint8_t>b;r060_inc(b,R071StateBase+0x40u);b.insert(b.end(),{0x89,0x0d});r060_emit32(b,R071StateBase+0x44u);b.push_back(0xc3);put(0x1c0,b);}
    // release virtual +0(this,1)
    {std::vector<std::uint8_t>b;r060_inc(b,R071StateBase+0x48u);b.insert(b.end(),{0x89,0x0d});r060_emit32(b,R071StateBase+0x4cu);b.insert(b.end(),{0x8b,0x44,0x24,0x04,0xa3});r060_emit32(b,R071StateBase+0x50u);b.insert(b.end(),{0xc2,0x04,0x00});put(0x200,b);}
    // protected release-index continuation: return scripted AL then jump 441221.
    {std::vector<std::uint8_t>b;r060_inc(b,R071StateBase+0x54u);b.push_back(0xa0);r060_emit32(b,R071StateBase+0x08u);b.push_back(0xe9);const auto site=R071StubBlock+0x240u+std::uint32_t(b.size());r060_emit32(b,0x441221u-(site+4u));put(0x240,b);}
    // 4035F0(): resolver for 443FF0 state 46.
    {std::vector<std::uint8_t>b;r060_inc(b,R071StateBase+0x58u);b.push_back(0xb8);r060_emit32(b,R071RelatedBase);b.push_back(0xc3);put(0x280,b);}
    // 83 callback stubs.
    for(unsigned i=0;i<PcObjectEventCallbackCount;++i){std::vector<std::uint8_t>b;r060_inc(b,R071StateBase+0x2cu);b.push_back(0xb8);r060_emit32(b,i);b.push_back(0xa3);r060_emit32(b,R071StateBase+0x30u);b.push_back(0xa1);r060_emit32(b,R071StateBase+0x00u);b.push_back(0xc3);put(0x400u+std::size_t(i)*0x20u,b);}
    if(mprotect(code,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("r071 stub mprotect");
    *reinterpret_cast<std::uint32_t*>(R071VtableBase+0x00u)=R071StubBlock+0x200u;
    *reinterpret_cast<std::uint32_t*>(R071VtableBase+0x04u)=R071StubBlock+0x180u;
    *reinterpret_cast<std::uint32_t*>(R071VtableBase+0x10u)=R071StubBlock+0x1c0u;
    for(unsigned i=0;i<16u;++i){auto* h=reinterpret_cast<std::uint8_t*>(R071HandleBase+i*0x40u);*reinterpret_cast<std::uint32_t*>(h)=R071VtableBase;*reinterpret_cast<std::uint32_t*>(h+8u)=21u;}
}
void r071_fill_callback_table(){for(unsigned i=0;i<PcObjectEventCallbackCount;++i){*reinterpret_cast<std::uint32_t*>(R071CallbackTableBase+i*8u)=i;*reinterpret_cast<std::uint32_t*>(R071CallbackTableBase+i*8u+4u)=R071StubBlock+0x400u+i*0x20u;}}
void r071_restore_callback_table(){if(!R071OriginalCallbackTableSaved)throw std::runtime_error("r071 table not saved");std::memcpy(reinterpret_cast<void*>(R071CallbackTableBase),R071OriginalCallbackTable.data(),R071OriginalCallbackTable.size());}
void set_r071_patches(bool enabled){
    struct P{std::uint32_t site,target,stub;};static constexpr P p[]={
        {0x443f1du,0x446fc0u,R071StubBlock+0x000u},{0x443f29u,0x446ea0u,R071StubBlock+0x080u},
        {0x443f5eu,0x43f9e0u,R071StubBlock+0x100u},{0x443f66u,0x440930u,R071StubBlock+0x140u},
        {0x444014u,0x4035f0u,R071StubBlock+0x280u}};
    for(const auto& x:p){if(enabled)r060_check_call(x.site,x.target);r046_write_rel32(x.site,0xe8,enabled?x.stub:x.target);}
    constexpr std::uint32_t site=0x44121bu;auto* page=reinterpret_cast<void*>(site&~0xfffu);
    if(mprotect(page,4096,PROT_READ|PROT_WRITE|PROT_EXEC))throw std::runtime_error("r071 release patch protect");auto* q=reinterpret_cast<std::uint8_t*>(site);
    if(enabled){q[0]=0xe9;const std::uint32_t rel=(R071StubBlock+0x240u)-(site+5u);std::memcpy(q+1,&rel,4);q[5]=0x90;}
    else{const std::uint8_t orig[6]={0xff,0x25,0xa0,0x9b,0x03,0x01};std::memcpy(q,orig,6);}
    if(mprotect(page,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("r071 release patch restore protect");
}
bool r071_oracle_name(const std::string& q){return q=="object_dispatch_callback_443eb0_r071"||q=="object_open_callback_443fa0_r071"||q=="object_state_code_443ff0_r071"||q=="r071_event_open_batch";}
void reset_r071_trace(std::uint32_t callback_ret,std::uint32_t ready,std::int8_t index){std::memset(reinterpret_cast<void*>(R071StateBase),0,0x100u);*reinterpret_cast<std::uint32_t*>(R071StateBase+0x00u)=callback_ret;*reinterpret_cast<std::uint32_t*>(R071StateBase+0x04u)=ready;*reinterpret_cast<std::uint8_t*>(R071StateBase+0x08u)=std::uint8_t(index);}
std::array<std::uint8_t,0x60> r071_guest_trace_blob(){std::array<std::uint8_t,0x60> o{};std::memcpy(o.data(),reinterpret_cast<void*>(R071StateBase),o.size());return o;}
void r071_native_push(void* u,std::uint32_t pc,Bytes,std::size_t off){auto& t=*static_cast<R071Trace*>(u);if(pc!=0x446fc0u)throw std::runtime_error("r071 push");++t.push_n;t.push_this=R071ObjectBase+std::uint32_t(off);}
void r071_native_set(void* u,std::uint32_t pc,Bytes,std::size_t off,std::uint32_t arg){auto& t=*static_cast<R071Trace*>(u);if(pc!=0x446ea0u)throw std::runtime_error("r071 set");++t.set_n;t.set_this=R071ObjectBase+std::uint32_t(off);t.set_arg=arg;}
std::uint32_t r071_native_cb(void* u,std::uint32_t,std::size_t idx){auto& t=*static_cast<R071Trace*>(u);++t.cb_n;t.cb_index=std::uint32_t(idx);return t.callback_ret;}
void r071_native_global(void* u,std::uint32_t pc,std::uint32_t arg,bool has){auto& t=*static_cast<R071Trace*>(u);if(pc==0x43f9e0u){++t.state_n;t.state_arg=arg;if(!has)throw std::runtime_error("r071 state arg");}else if(pc==0x440930u){++t.pause_n;if(has)throw std::runtime_error("r071 pause arg");}else throw std::runtime_error("r071 global target");}
std::uint32_t r071_native_ready(void* u,std::uint32_t h,std::uint32_t slot){auto& t=*static_cast<R071Trace*>(u);++t.ready_n;t.ready_handle=h;t.ready_slot=slot;return t.ready_ret;}
void r071_native_rel_void(void* u,std::uint32_t pc,std::uint32_t h){auto& t=*static_cast<R071Trace*>(u);if(pc!=0x441210u)throw std::runtime_error("r071 rel10");++t.rel10_n;t.rel10_handle=h;}
void r071_native_rel_arg(void* u,std::uint32_t pc,std::uint32_t h,std::uint32_t a){auto& t=*static_cast<R071Trace*>(u);if(pc!=0x441219u)throw std::runtime_error("r071 rel0");++t.rel0_n;t.rel0_handle=h;t.rel0_arg=a;}
std::int8_t r071_native_rel_i8(void* u,std::uint32_t pc,std::uint32_t){auto& t=*static_cast<R071Trace*>(u);if(pc!=0x01039ba0u)throw std::runtime_error("r071 reli8");++t.reli8_n;return std::int8_t(t.release_index);}
std::uint32_t r071_native_state_u32(void*,std::uint32_t pc,std::uint32_t h){if(pc!=0x564c90u)throw std::runtime_error("r071 state u32");return *reinterpret_cast<const std::uint32_t*>(std::uintptr_t(h)+8u);}
PcObjectEventDispatch443eb0Services r071_dispatch_services(R071Trace& t){return {&t,r071_native_push,r071_native_set,r071_native_cb,r071_native_global};}
PcRuntimeControlServices r071_runtime_services(R071Trace& t){return {&t,r071_native_rel_void,r071_native_rel_arg,nullptr,nullptr,nullptr,r071_native_rel_i8};}
PcObjectOpenCallbackServices r071_open_services(R071Trace& t){return {&t,r071_native_ready};}
PcObjectStateServices r071_state_services(R071Trace& t){return {&t,nullptr,nullptr,r071_native_state_u32};}
std::array<std::uint8_t,0x60> r071_trace_blob(const R071Trace& t){std::array<std::uint8_t,0x60> o{};const std::uint32_t v[]={t.callback_ret,t.ready_ret,t.release_index,t.push_n,t.push_this,t.set_n,t.set_this,t.set_arg,t.state_n,t.state_arg,t.pause_n,t.cb_n,t.cb_index,t.ready_n,t.ready_handle,t.ready_slot,t.rel10_n,t.rel10_handle,t.rel0_n,t.rel0_handle,t.rel0_arg,t.reli8_n,t.resolver_n};std::memcpy(o.data(),v,sizeof(v));return o;}
std::array<std::uint8_t,R071ObjectSize> r071_object_fixture(unsigned n){std::array<std::uint8_t,R071ObjectSize> o{};std::mt19937 g(0x443eb071u^(n*2654435761u));for(auto& b:o)b=std::uint8_t(g());Bytes x(o.data(),o.size());x.put8(0x220u,std::uint8_t(n%34u));x.put32(0x218u,(n%3u)==0u?3u:1u);x.put32(0x518u,0u);x.put32(0x484u,0u);x.put8(0x48cu,0u);x.put8(0x494u,0u);x.put32(0x488u,0u);x.put32(0x490u,0u);x.put8(0xd94u,(n%9u)==0u?0u:1u);return o;}
std::array<PcObjectEventCallbackEntry,PcObjectEventCallbackCount> r071_native_callback_table(){std::array<PcObjectEventCallbackEntry,PcObjectEventCallbackCount> t{};for(unsigned i=0;i<t.size();++i)t[i]={i,R071StubBlock+0x400u+i*0x20u};return t;}
std::uint32_t r071_handle(unsigned i){return R071HandleBase+(i%16u)*0x40u;}

// r072: active runtime list/open/commit parents 0x4440F0, 0x4442A0 and
// 0x444350.  Already-closed children execute normally; only still-open leaf
// boundaries (48D870, 48F4E0, 4EEC80 and mode-specific platform calls) are
// patched to deterministic stubs.  Handle virtuals use mapped synthetic
// vtables so parent-owned ordering and object mutations remain executable.
constexpr std::uint32_t R072StubBlock=0x32030000u;
constexpr std::uint32_t R072ObjectBase=0x3f230000u;
constexpr std::uint32_t R072HandleBase=0x3f234000u;
constexpr std::uint32_t R072VtableBase=0x3f235000u;
constexpr std::size_t R072ObjectSize=0x2000u;
constexpr std::uint32_t R072SnapshotBase=0x007b15d8u;
void map_r072_runtime_list(){
    auto* code=static_cast<std::uint8_t*>(map_at(R072StubBlock,4096,PROT_READ|PROT_WRITE));
    map_at(R072ObjectBase,0x3000,PROT_READ|PROT_WRITE);map_at(R072HandleBase,4096,PROT_READ|PROT_WRITE);map_at(R072VtableBase,4096,PROT_READ|PROT_WRITE);
    auto put=[&](std::size_t off,std::initializer_list<std::uint8_t> b){std::memcpy(code+off,b.begin(),b.size());};
    put(0x000,{0x31,0xc0,0xc2,0x04,0x00});                         // v+0(this,1)
    put(0x020,{0x8b,0x41,0x14,0xc3});                              // v+4 -> handle+14
    put(0x040,{0x31,0xc0,0xc3});                                   // v+10
    put(0x060,{0x8b,0x41,0x0c,0xc2,0x04,0x00});                   // v+18(key)
    put(0x080,{0x8b,0x41,0x10,0xc2,0x04,0x00});                   // v+1c(key)
    put(0x0a0,{0x31,0xc0,0xc2,0x04,0x00});                         // 48D870(this,new)
    put(0x0c0,{0x8b,0x41,0x18,0xc3});                              // 48F4E0(this)
    put(0x0e0,{0x31,0xc0,0xc3});                                   // cdecl/no-arg no-op
    if(mprotect(code,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("r072 stub mprotect");
    *reinterpret_cast<std::uint32_t*>(R072VtableBase+0x00u)=R072StubBlock+0x000u;
    *reinterpret_cast<std::uint32_t*>(R072VtableBase+0x04u)=R072StubBlock+0x020u;
    *reinterpret_cast<std::uint32_t*>(R072VtableBase+0x10u)=R072StubBlock+0x040u;
    *reinterpret_cast<std::uint32_t*>(R072VtableBase+0x18u)=R072StubBlock+0x060u;
    *reinterpret_cast<std::uint32_t*>(R072VtableBase+0x1cu)=R072StubBlock+0x080u;
    for(unsigned i=0;i<16u;++i){auto* h=reinterpret_cast<std::uint8_t*>(R072HandleBase+i*0x40u);std::memset(h,0,0x40u);*reinterpret_cast<std::uint32_t*>(h)=R072VtableBase;}
}
void set_r072_patches(bool enabled){
    struct P{std::uint32_t site,target,stub;};static constexpr P p[]={
        {0x44421eu,0x48d870u,R072StubBlock+0x0a0u},
        {0x4442c4u,0x48f4e0u,R072StubBlock+0x0c0u},{0x4442ddu,0x48f4e0u,R072StubBlock+0x0c0u},
        {0x4442f6u,0x48f4e0u,R072StubBlock+0x0c0u},{0x444316u,0x48f4e0u,R072StubBlock+0x0c0u},
        {0x44435au,0x4eec80u,R072StubBlock+0x0e0u},{0x444398u,0x43f870u,R072StubBlock+0x0e0u},
        {0x4443acu,0x4957e0u,R072StubBlock+0x0e0u},{0x4443b5u,0x44da00u,R072StubBlock+0x0e0u}};
    for(const auto& x:p){if(enabled)r060_check_call(x.site,x.target);r046_write_rel32(x.site,0xe8,enabled?x.stub:x.target);}
}
bool r072_oracle_name(const std::string& q){return q=="object_insert_callback_4440f0_r072"||q=="object_reopen_callback_4442a0_r072"||q=="object_runtime_commit_444350_r072"||q=="r072_runtime_list_batch";}
std::uint32_t r072_handle(unsigned i){return R072HandleBase+(i%16u)*0x40u;}
void r072_setup_handle(unsigned i,std::uint32_t state,std::uint32_t raw18,std::uint32_t claim1c,std::uint32_t ready,std::uint32_t key){
    auto* h=reinterpret_cast<std::uint8_t*>(std::uintptr_t(r072_handle(i)));std::memset(h+4,0,0x3cu);
    *reinterpret_cast<std::uint32_t*>(h)=R072VtableBase;*reinterpret_cast<std::uint32_t*>(h+8)=state;
    *reinterpret_cast<std::uint32_t*>(h+0x0cu)=raw18;*reinterpret_cast<std::uint32_t*>(h+0x10u)=claim1c;
    *reinterpret_cast<std::uint32_t*>(h+0x14u)=ready;*reinterpret_cast<std::uint32_t*>(h+0x18u)=key;
}
std::uint32_t r072_native_get(void*,std::uint32_t pc,std::uint32_t h){
    if(h==0u)return 0u;const auto* p=reinterpret_cast<const std::uint8_t*>(std::uintptr_t(h));
    if(pc==0x564c90u)return *reinterpret_cast<const std::uint32_t*>(p+8u);
    if(pc==0x48f4e0u)return *reinterpret_cast<const std::uint32_t*>(p+0x18u);
    throw std::runtime_error("r072 get target");
}
std::uint32_t r072_native_virtual(void*,std::uint32_t h,std::uint32_t slot,std::uint32_t,bool){
    if(h==0u)return 0u;const auto* p=reinterpret_cast<const std::uint8_t*>(std::uintptr_t(h));
    if(slot==0x04u)return *reinterpret_cast<const std::uint32_t*>(p+0x14u);
    if(slot==0x18u)return *reinterpret_cast<const std::uint32_t*>(p+0x0cu);
    if(slot==0x1cu)return *reinterpret_cast<const std::uint32_t*>(p+0x10u);
    if(slot==0x00u||slot==0x10u)return 0u;
    throw std::runtime_error("r072 virtual slot");
}
void r072_native_link(void*,std::uint32_t pc,std::uint32_t,std::uint32_t){if(pc!=0x48d870u)throw std::runtime_error("r072 link target");}
PcObjectListServices r072_list_services(){return {nullptr,r072_native_get,r072_native_virtual,r072_native_link};}
void r072_native_prepare(void*,std::uint32_t pc,Bytes,std::size_t){if(pc!=0x4eec80u)throw std::runtime_error("r072 prepare target");}
void r072_native_global(void*,std::uint32_t pc,std::uint32_t,std::uint32_t,std::uint32_t){if(pc!=0x43f870u&&pc!=0x4957e0u&&pc!=0x44da00u)throw std::runtime_error("r072 global target");}
std::uint32_t r072_native_commit_virtual(void*,std::uint32_t,std::uint32_t slot,std::uint32_t,bool){if(slot!=0x10u&&slot!=0x00u)throw std::runtime_error("r072 commit virtual");return 0u;}
PcObjectRuntimeCommitServices r072_commit_services(){return {nullptr,r072_native_prepare,r072_native_global,r072_native_commit_virtual};}
PcObjectStateServices r072_state_services(){return {nullptr,nullptr,nullptr,r072_native_get};}
std::array<std::uint8_t,R072ObjectSize> r072_object_fixture(unsigned n){std::array<std::uint8_t,R072ObjectSize> o{};std::mt19937 g(0x4440f072u^(n*2654435761u));for(auto& b:o)b=std::uint8_t(g());Bytes x(o.data(),o.size());x.put32(0x518u,0u);x.put32(0x484u,0u);x.put8(0x48cu,0u);x.put8(0x494u,0u);x.put8(0x220u,6u);x.put32(0x51cu+0x408u,9u);x.put32(0x218u,1u);return o;}


// r073: functional-first closure of the unconditional 4EEC80 runtime prepare
// chain plus the two callback-object primitives exposed by r072.
constexpr std::uint32_t R073StubBlock=0x32032000u;
constexpr std::uint32_t R073StateBase=0x3f236000u;
constexpr std::uint32_t R073ObjectBase=0x3f237000u;
constexpr std::uint32_t R073CallbackBase=0x3f238000u;
void map_r073_runtime_prepare(){
    auto* code=static_cast<std::uint8_t*>(map_at(R073StubBlock,4096,PROT_READ|PROT_WRITE));
    map_at(R073StateBase,4096,PROT_READ|PROT_WRITE);
    map_at(R073ObjectBase,4096,PROT_READ|PROT_WRITE);
    map_at(R073CallbackBase,4096,PROT_READ|PROT_WRITE);
    for(const std::uint32_t page:{0x780000u,0x656000u,0x830000u,0x836000u,0x7f9000u})
        if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("r073 global page mprotect");
    auto emit32=[](std::vector<std::uint8_t>& b,std::uint32_t v){for(unsigned k=0;k<4;++k)b.push_back(std::uint8_t(v>>(8u*k)));};
    // 495930(category): log/count call, return scripted count at state+0.
    {
        std::vector<std::uint8_t>b;
        b.insert(b.end(),{0xff,0x05});emit32(b,R073StateBase+0x10u); // inc count_n
        b.insert(b.end(),{0x8b,0x44,0x24,0x04,0xa3});emit32(b,R073StateBase+0x14u);
        b.push_back(0xa1);emit32(b,R073StateBase+0x00u);b.push_back(0xc3);
        std::memcpy(code+0x000u,b.data(),b.size());
    }
    // 4958A0(category,index): log both args and return.
    {
        std::vector<std::uint8_t>b;
        b.insert(b.end(),{0xff,0x05});emit32(b,R073StateBase+0x04u); // inc select_n
        b.insert(b.end(),{0x8b,0x44,0x24,0x04,0xa3});emit32(b,R073StateBase+0x08u);
        b.insert(b.end(),{0x8b,0x44,0x24,0x08,0xa3});emit32(b,R073StateBase+0x0cu);
        b.push_back(0xc3);std::memcpy(code+0x080u,b.data(),b.size());
    }
    if(mprotect(code,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("r073 stub mprotect");
}
void set_r073_patches(bool enabled){
    struct P{std::uint32_t site,target,stub;};static constexpr P p[]={
        {0x4eeb74u,0x495930u,R073StubBlock+0x000u},
        {0x4eeb8eu,0x4958a0u,R073StubBlock+0x080u},
        {0x4eee40u,0x4958a0u,R073StubBlock+0x080u}};
    for(const auto& x:p){if(enabled)r060_check_call(x.site,x.target);r046_write_rel32(x.site,0xe8,enabled?x.stub:x.target);}
}
bool r073_oracle_name(const std::string& q){return q=="runtime_primary_mode_4eea70_r073"||q=="runtime_route_mode_4eead0_r073"||q=="runtime_route_config_4eeb50_r073"||q=="runtime_prepare_4eec80_r073"||q=="callback_key_48f4e0_r073"||q=="callback_append_48d870_r073"||q=="r073_runtime_prepare_batch";}
struct R073Trace{std::uint32_t count_ret{},select_n{},select_cat{},select_idx{},count_n{},count_cat{};};
void reset_r073_trace(std::uint32_t count){std::memset(reinterpret_cast<void*>(R073StateBase),0,0x40u);*reinterpret_cast<std::uint32_t*>(R073StateBase)=count;}
R073Trace r073_guest_trace(){R073Trace t{};t.count_ret=*reinterpret_cast<std::uint32_t*>(R073StateBase+0x00u);t.select_n=*reinterpret_cast<std::uint32_t*>(R073StateBase+0x04u);t.select_cat=*reinterpret_cast<std::uint32_t*>(R073StateBase+0x08u);t.select_idx=*reinterpret_cast<std::uint32_t*>(R073StateBase+0x0cu);t.count_n=*reinterpret_cast<std::uint32_t*>(R073StateBase+0x10u);t.count_cat=*reinterpret_cast<std::uint32_t*>(R073StateBase+0x14u);return t;}
std::uint32_t r073_native_count(void* u,std::uint32_t cat){auto& t=*static_cast<R073Trace*>(u);++t.count_n;t.count_cat=cat;return t.count_ret;}
void r073_native_select(void* u,std::uint32_t cat,std::uint32_t idx){auto& t=*static_cast<R073Trace*>(u);++t.select_n;t.select_cat=cat;t.select_idx=idx;}
PcRuntimePrepareServices r073_services(R073Trace& t){return {&t,r073_native_count,r073_native_select};}
PcRuntimePrepareState r073_guest_state(){
    PcRuntimePrepareState x{};x.primary_mode_78024c=*reinterpret_cast<std::uint32_t*>(0x78024cu);x.route_state_780258=*reinterpret_cast<std::uint32_t*>(0x780258u);x.output_code_656234=*reinterpret_cast<std::uint32_t*>(0x656234u);x.output_flag_830395=*reinterpret_cast<std::uint8_t*>(0x830395u);x.alternate_code_836174=*reinterpret_cast<std::uint32_t*>(0x836174u);x.selection_active_836374=*reinterpret_cast<std::uint8_t*>(0x836374u);for(unsigned i=0;i<5u;++i)x.event_config_7f94c4[i]=*reinterpret_cast<std::uint32_t*>(0x7f94c4u+i*4u);return x;
}
void r073_store_guest_state(const PcRuntimePrepareState& x){
    *reinterpret_cast<std::uint32_t*>(0x78024cu)=x.primary_mode_78024c;*reinterpret_cast<std::uint32_t*>(0x780258u)=x.route_state_780258;*reinterpret_cast<std::uint32_t*>(0x656234u)=x.output_code_656234;*reinterpret_cast<std::uint8_t*>(0x830395u)=x.output_flag_830395;*reinterpret_cast<std::uint32_t*>(0x836174u)=x.alternate_code_836174;*reinterpret_cast<std::uint8_t*>(0x836374u)=x.selection_active_836374;for(unsigned i=0;i<5u;++i)*reinterpret_cast<std::uint32_t*>(0x7f94c4u+i*4u)=x.event_config_7f94c4[i];
}
std::array<std::uint8_t,40> r073_state_blob(const PcRuntimePrepareState& x){std::array<std::uint8_t,40> o{};std::size_t n=0;auto put32=[&](std::uint32_t v){std::memcpy(o.data()+n,&v,4);n+=4;};put32(x.primary_mode_78024c);put32(x.route_state_780258);put32(x.output_code_656234);put32(x.output_flag_830395);put32(x.alternate_code_836174);put32(x.selection_active_836374);for(auto v:x.event_config_7f94c4)put32(v);return o;}
std::array<std::uint8_t,24> r073_trace_blob(const R073Trace& t){std::array<std::uint8_t,24> o{};const std::uint32_t v[6]={t.count_ret,t.select_n,t.select_cat,t.select_idx,t.count_n,t.count_cat};std::memcpy(o.data(),v,sizeof(v));return o;}
PcRuntimePrepareState r073_initial_state(unsigned i){PcRuntimePrepareState x{};x.primary_mode_78024c=(i*3u+1u)%5u;x.route_state_780258=(i*5u+2u)%11u;x.output_code_656234=0x73000000u^i;x.output_flag_830395=std::uint8_t(i&1u);x.alternate_code_836174=0x73100000u^(i*17u);x.selection_active_836374=std::uint8_t((i>>1u)&1u);for(unsigned k=0;k<5u;++k)x.event_config_7f94c4[k]=0x73200000u+i*0x100u+k;return x;}
std::uint32_t r073_packed(unsigned i){
    const auto p=i&3u;const auto code=(i*7u+5u)&0x3fu;switch(i%12u){
      case 0:return p|(1u<<2);case 1:return p|(2u<<2);case 2:return p|(0u<<5)|((i%7u)<<8)|(((i*3u)&0x3fu)<<12)|(((i+2u)&7u)<<18);
      case 3:return 1u|(4u<<5)|(code<<12);case 4:return 1u|(4u<<5)|(5u<<12)|(1u<<11);case 5:return 1u|(4u<<5)|(11u<<12)|(1u<<11);
      case 6:return 0u|(4u<<5)|(code<<12)|(1u<<11);case 7:return 1u|(5u<<5)|(code<<12);case 8:return 2u|(5u<<5)|(code<<12)|(1u<<11);
      case 9:return p|(7u<<5);case 10:return p|(1u<<5);default:return p|(0u<<5)|(7u<<8)|((code&0x3fu)<<12);}
}

void reset_r060_trace(std::uint32_t alloc_ret,std::uint32_t ctor_ret){std::memset(reinterpret_cast<void*>(R060StateBase),0,0x40);*reinterpret_cast<std::uint32_t*>(R060StateBase+0x00u)=alloc_ret;*reinterpret_cast<std::uint32_t*>(R060StateBase+0x04u)=ctor_ret;}
struct R060Trace{std::uint32_t alloc_n{},bytes{},ctor_n{},object{},alloc_ret{},ctor_ret{};};
std::uint32_t r060_native_alloc(void* u,std::uint32_t pc,std::uint32_t bytes){auto& t=*static_cast<R060Trace*>(u);if(pc==0x5802cfu){++t.alloc_n;t.bytes=bytes;}return t.alloc_ret;}
std::uint32_t r060_native_ctor(void* u,std::uint32_t pc,std::uint32_t object){auto& t=*static_cast<R060Trace*>(u);++t.ctor_n;t.object=object;(void)pc;return t.ctor_ret;}
PcFactoryServices r060_services(R060Trace& t){return {&t,r060_native_alloc,r060_native_ctor};}
std::array<std::uint8_t,16> r060_trace_blob(const R060Trace& t){std::array<std::uint8_t,16> o{};const std::uint32_t v[4]={t.alloc_n,t.bytes,t.ctor_n,t.object};std::memcpy(o.data(),v,sizeof(v));return o;}
std::array<std::uint8_t,16> r060_guest_trace_blob(){std::array<std::uint8_t,16> o{};std::memcpy(o.data(),reinterpret_cast<void*>(R060StateBase+0x08u),16);return o;}

constexpr std::uint32_t R044StubBlock=0x32011000u;
constexpr std::uint32_t R044StateBase=0x3301e000u;
constexpr std::uint32_t R044PacketBase=0x3301f000u;
constexpr std::uint32_t R044RecordsBase=0x36000000u;
constexpr std::size_t R044RecordsSize=0x8000u;
void map_r044_platform_stubs(){
    auto* code=static_cast<std::uint8_t*>(map_at(R044StubBlock,4096,PROT_READ|PROT_WRITE));
    map_at(R044StateBase,4096,PROT_READ|PROT_WRITE);
    map_at(R044PacketBase,4096,PROT_READ|PROT_WRITE);
    map_at(R044RecordsBase,R044RecordsSize,PROT_READ|PROT_WRITE);
    for(const std::uint32_t page:{0x7d3000u,0x656000u,0x7f8000u,0x7f9000u,0x64b000u,0x780000u,0x810000u,0x813000u})
        if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("r044 input-page mprotect");
    auto emit32=[](std::uint8_t* q,std::uint32_t v){std::memcpy(q,&v,4);};
    // Generic mov eax,[abs]; ret gate/timer stubs.
    {std::uint8_t b[]={0xa1,0,0,0,0,0xc3};emit32(b+1,R044StateBase+0x00u);std::memcpy(code+0x000,b,sizeof(b));}
    {std::uint8_t b[]={0xa1,0,0,0,0,0xc3};emit32(b+1,R044StateBase+0x04u);std::memcpy(code+0x010,b,sizeof(b));}
    // Serializer boundary: mov dword ptr [abs],1; ret.
    {std::uint8_t b[]={0xc7,0x05,0,0,0,0,1,0,0,0,0xc3};emit32(b+2,R044StateBase+0x08u);std::memcpy(code+0x020,b,sizeof(b));}
    // 0x47F780 service results / boundary markers.
    {std::uint8_t b[]={0xa1,0,0,0,0,0xc3};emit32(b+1,R044StateBase+0x0cu);std::memcpy(code+0x030,b,sizeof(b));}
    {std::uint8_t b[]={0xa1,0,0,0,0,0xc3};emit32(b+1,R044StateBase+0x10u);std::memcpy(code+0x040,b,sizeof(b));}
    {std::uint8_t b[]={0xa1,0,0,0,0,0xc3};emit32(b+1,R044StateBase+0x14u);std::memcpy(code+0x050,b,sizeof(b));}
    {std::uint8_t b[]={0xc7,0x05,0,0,0,0,1,0,0,0,0xc3};emit32(b+2,R044StateBase+0x18u);std::memcpy(code+0x060,b,sizeof(b));}
    {std::uint8_t b[]={0xc7,0x05,0,0,0,0,1,0,0,0,0xc3};emit32(b+2,R044StateBase+0x1cu);std::memcpy(code+0x070,b,sizeof(b));}
    // Mid-function CommonPlCar-tail wrapper: preserve the register roles, build
    // the four saved-register words + leftover call argument, then jump tail.
    {
        std::vector<std::uint8_t>b{0x55,0x56,0x53,0x57,0x6a,0x00,0xe9};
        const std::uint32_t from=R044StubBlock+0x200u+std::uint32_t(b.size())+4u;
        const std::uint32_t rel=0x4a82c4u-from;
        for(unsigned k=0;k<4;++k)b.push_back(std::uint8_t(rel>>(8u*k)));
        std::memcpy(code+0x200,b.data(),b.size());
    }
    auto patch_call=[&](std::uint32_t va,std::uint32_t expected,std::uint32_t target){
        auto* q=reinterpret_cast<std::uint8_t*>(va);std::int32_t rel{};std::memcpy(&rel,q+1,4);
        if(q[0]!=0xe8||std::uint32_t(std::int64_t(va+5u)+rel)!=expected)throw std::runtime_error("r044 CALL target changed");
        auto* page=reinterpret_cast<void*>(va&~0xfffu);if(mprotect(page,4096,PROT_READ|PROT_WRITE|PROT_EXEC))throw std::runtime_error("r044 patch protection");
        const auto nr=target-(va+5u);std::memcpy(q+1,&nr,4);if(mprotect(page,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("r044 patch restore protection");
    };
    auto patch_jump=[&](std::uint32_t va,std::uint32_t expected,std::uint32_t target){
        auto* q=reinterpret_cast<std::uint8_t*>(va);std::int32_t rel{};std::memcpy(&rel,q+1,4);
        if(q[0]!=0xe9||std::uint32_t(std::int64_t(va+5u)+rel)!=expected)throw std::runtime_error("r044 JMP target changed");
        auto* page=reinterpret_cast<void*>(va&~0xfffu);if(mprotect(page,4096,PROT_READ|PROT_WRITE|PROT_EXEC))throw std::runtime_error("r044 jump patch protection");
        const auto nr=target-(va+5u);std::memcpy(q+1,&nr,4);if(mprotect(page,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("r044 jump patch restore protection");
    };
    patch_call(0x450753u,0x43f960u,R044StubBlock+0x000u);
    patch_call(0x4671f1u,0x49b2d0u,R044StubBlock+0x010u);
    patch_call(0x467248u,0x466e50u,R044StubBlock+0x020u);
    patch_call(0x47f78bu,0x44c940u,R044StubBlock+0x030u);
    patch_call(0x47f7b4u,0x450130u,R044StubBlock+0x040u);
    patch_call(0x47f7d8u,0x451180u,R044StubBlock+0x050u);
    patch_call(0x47f8eau,0x49b2d0u,R044StubBlock+0x010u);
    patch_jump(0x47f8fau,0x47ed90u,R044StubBlock+0x060u);
    patch_call(0x47f91eu,0x47f330u,R044StubBlock+0x070u);
    if(mprotect(code,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("r044 stub mprotect");
}
void set_r044_helper_state(std::uint32_t flags,bool external_gate,std::int32_t counter){
    *reinterpret_cast<std::uint32_t*>(0x7d39f0u)=flags;
    *reinterpret_cast<std::uint32_t*>(R044StateBase+0x00u)=external_gate?1u:0u;
    *reinterpret_cast<std::int32_t*>(0x656234u)=counter;
}
void set_r044_ghost_state(const PcPlatformGhost4671Inputs& in,const PcPlatformGhost4671State& st){
    *reinterpret_cast<std::int32_t*>(0x78026cu)=in.game_mode;
    *reinterpret_cast<std::int32_t*>(0x780258u)=in.route_state;
    set_r043_session_state(in.session);
    *reinterpret_cast<std::uint32_t*>(R044StateBase+0x04u)=static_cast<std::uint16_t>(in.timer);
    *reinterpret_cast<std::uint32_t*>(R044StateBase+0x08u)=0u;
    *reinterpret_cast<std::uint8_t*>(0x7f9220u)=in.packet_flags;
    *reinterpret_cast<std::int32_t*>(0x656234u)=in.frame_counter;
    *reinterpret_cast<std::uint32_t*>(0x7f9228u)=R044PacketBase;
    *reinterpret_cast<std::uint32_t*>(0x7f91f0u)=R044PacketBase+st.writer_offset;
    *reinterpret_cast<std::uint32_t*>(0x7f8d84u)=R044PacketBase+st.reader_offset;
    *reinterpret_cast<std::uint32_t*>(0x7f8ef4u)=st.stream_index;
    *reinterpret_cast<std::uint8_t*>(R044PacketBase+0x11bu)=st.packet_11b;
    *reinterpret_cast<std::uint8_t*>(R044PacketBase+0x11cu)=st.packet_11c;
    *reinterpret_cast<std::uint8_t*>(0x64bfecu)=st.service_pending;
    *reinterpret_cast<std::uint32_t*>(0x7f92b8u)=st.short_window;
}
PcPlatformGhost4671State get_r044_ghost_state(){
    PcPlatformGhost4671State st{};
    st.writer_offset=*reinterpret_cast<std::uint32_t*>(0x7f91f0u)-R044PacketBase;
    st.reader_offset=*reinterpret_cast<std::uint32_t*>(0x7f8d84u)-R044PacketBase;
    st.stream_index=*reinterpret_cast<std::uint32_t*>(0x7f8ef4u);
    st.packet_11b=*reinterpret_cast<std::uint8_t*>(R044PacketBase+0x11bu);
    st.packet_11c=*reinterpret_cast<std::uint8_t*>(R044PacketBase+0x11cu);
    st.service_pending=*reinterpret_cast<std::uint8_t*>(0x64bfecu);
    st.short_window=*reinterpret_cast<std::uint32_t*>(0x7f92b8u);
    st.serialize_called=*reinterpret_cast<std::uint32_t*>(R044StateBase+0x08u)!=0u;
    return st;
}
void set_r044_record_state(const PcPlatformGhost47f780Inputs& in,const PcPlatformGhost47f780State& st,
                           const std::vector<std::uint8_t>& records,const std::array<std::uint32_t,32>& index_table,
                           const std::array<std::uint32_t,128>& pair_table){
    if(records.size()>R044RecordsSize)throw std::runtime_error("r044 record fixture too large");
    std::memset(reinterpret_cast<void*>(R044RecordsBase),0xcd,R044RecordsSize);
    std::memcpy(reinterpret_cast<void*>(R044RecordsBase),records.data(),records.size());
    *reinterpret_cast<std::uint32_t*>(0x81373cu)=R044RecordsBase;
    *reinterpret_cast<std::int32_t*>(0x78026cu)=in.game_mode;
    *reinterpret_cast<std::int32_t*>(0x780258u)=in.route_state;
    *reinterpret_cast<std::uint32_t*>(0x7d39f0u)=in.flags;
    *reinterpret_cast<std::uint32_t*>(R044StateBase+0x00u)=in.slot_external_gate?1u:0u;
    *reinterpret_cast<std::uint32_t*>(R044StateBase+0x04u)=static_cast<std::uint16_t>(in.timer);
    *reinterpret_cast<std::uint32_t*>(R044StateBase+0x0cu)=static_cast<std::uint32_t>(in.entry_slot);
    *reinterpret_cast<std::uint32_t*>(R044StateBase+0x10u)=in.protected_gate?1u:0u;
    *reinterpret_cast<std::uint32_t*>(R044StateBase+0x14u)=in.allocate_result;
    *reinterpret_cast<std::uint32_t*>(R044StateBase+0x18u)=0u;
    *reinterpret_cast<std::uint32_t*>(R044StateBase+0x1cu)=0u;
    *reinterpret_cast<std::uint32_t*>(0x810440u)=in.slot_token;
    *reinterpret_cast<std::uint32_t*>(0x81010cu)=st.init_state;
    *reinterpret_cast<std::uint32_t*>(0x810110u)=st.sequence;
    *reinterpret_cast<std::uint32_t*>(0x81043cu)=st.frame_counter;
    *reinterpret_cast<std::uint32_t*>(0x810114u)=st.reset114_bits;
    *reinterpret_cast<std::uint32_t*>(0x810118u)=st.reset118_bits;
    *reinterpret_cast<std::uint32_t*>(0x81011cu)=st.reset11c_bits;
    *reinterpret_cast<std::uint8_t*>(0x810121u)=st.marker121;
    *reinterpret_cast<std::uint8_t*>(0x810123u)=st.marker123;
    *reinterpret_cast<std::uint8_t*>(0x813748u)=st.record_ready;
    for(unsigned k=0;k<index_table.size();++k)*reinterpret_cast<std::uint32_t*>(0x7d3954u+k*4u)=index_table[k];
    for(unsigned k=0;k<pair_table.size();++k)*reinterpret_cast<std::uint32_t*>(0x7d3748u+k*4u)=pair_table[k];
}
PcPlatformGhost47f780State get_r044_record_state(){
    PcPlatformGhost47f780State st{};
    st.init_state=*reinterpret_cast<std::uint32_t*>(0x81010cu);
    st.sequence=*reinterpret_cast<std::uint32_t*>(0x810110u);
    st.frame_counter=*reinterpret_cast<std::uint32_t*>(0x81043cu);
    st.reset114_bits=*reinterpret_cast<std::uint32_t*>(0x810114u);
    st.reset118_bits=*reinterpret_cast<std::uint32_t*>(0x810118u);
    st.reset11c_bits=*reinterpret_cast<std::uint32_t*>(0x81011cu);
    st.marker121=*reinterpret_cast<std::uint8_t*>(0x810121u);
    st.marker123=*reinterpret_cast<std::uint8_t*>(0x810123u);
    st.record_ready=*reinterpret_cast<std::uint8_t*>(0x813748u);
    st.reset_service_called=*reinterpret_cast<std::uint32_t*>(R044StateBase+0x18u)!=0u;
    st.record_service_called=*reinterpret_cast<std::uint32_t*>(R044StateBase+0x1cu)!=0u;
    return st;
}

constexpr std::uint32_t HandicapStubBlock=0x3200b000u;
constexpr std::uint32_t HandicapStateBase=0x33017000u;
void map_handicap_stubs(){
    auto* code=static_cast<std::uint8_t*>(map_at(HandicapStubBlock,4096,PROT_READ|PROT_WRITE));
    map_at(HandicapStateBase,4096,PROT_READ|PROT_WRITE);
    // HandicapControl reads three mutable race globals directly. map_pe() deliberately
    // leaves game .data read-only for normal arithmetic oracles, so open only these
    // controlled input pages for the r039 fixture.
    for(const std::uint32_t page:{0x7dd000u,0x7de000u,0x7df000u})
        if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE))
            throw std::runtime_error("handicap input-page mprotect");
    auto make_stub=[&](std::size_t off,std::uint32_t cell){
        std::uint8_t bytes[]={0xa1,0,0,0,0,0xc3};
        std::memcpy(bytes+1,&cell,4);std::memcpy(code+off,bytes,sizeof(bytes));return HandicapStubBlock+std::uint32_t(off);
    };
    const auto ready=make_stub(0x000,HandicapStateBase+0x00);
    const auto refpos=make_stub(0x010,HandicapStateBase+0x04);
    const auto stage0=make_stub(0x020,HandicapStateBase+0x08);
    const auto stage1=make_stub(0x030,HandicapStateBase+0x0c);
    const auto max0=make_stub(0x040,HandicapStateBase+0x10);
    const auto max1=make_stub(0x050,HandicapStateBase+0x14);
    auto patch_call=[&](std::uint32_t va,std::uint32_t expected,std::uint32_t target){
        auto* q=reinterpret_cast<std::uint8_t*>(va);std::int32_t rel{};std::memcpy(&rel,q+1,4);
        if(q[0]!=0xe8||std::uint32_t(std::int64_t(va+5u)+rel)!=expected)throw std::runtime_error("HandicapControl CALL target changed");
        auto* page=reinterpret_cast<void*>(va&~0xfffu);if(mprotect(page,4096,PROT_READ|PROT_WRITE|PROT_EXEC))throw std::runtime_error("handicap patch protection");
        const auto newrel=target-(va+5u);std::memcpy(q+1,&newrel,4);if(mprotect(page,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("handicap patch restore protection");
    };
    patch_call(0x458e8bu,0x4963e0u,ready);patch_call(0x458eaau,0x456bb0u,refpos);
    patch_call(0x458f40u,0x44c940u,stage0);patch_call(0x458f4au,0x44c980u,stage1);patch_call(0x458f55u,0x43d470u,max0);
    patch_call(0x4590c7u,0x44c940u,stage0);patch_call(0x4590d1u,0x44c980u,stage1);patch_call(0x4590e8u,0x43d470u,max0);patch_call(0x459157u,0x43d470u,max1);
    if(mprotect(code,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("handicap stub mprotect");
}
void set_handicap_inputs(const PcHandicapInputs& v){
    *reinterpret_cast<std::int32_t*>(HandicapStateBase+0x00)=v.race_ready?1:0;
    *reinterpret_cast<std::int32_t*>(HandicapStateBase+0x04)=v.reference_course_position;
    *reinterpret_cast<std::int32_t*>(HandicapStateBase+0x08)=v.stage_current;
    *reinterpret_cast<std::int32_t*>(HandicapStateBase+0x0c)=v.stage_reference;
    *reinterpret_cast<std::int32_t*>(HandicapStateBase+0x10)=v.max_cs_len0;
    *reinterpret_cast<std::int32_t*>(HandicapStateBase+0x14)=v.max_cs_len1;
    *reinterpret_cast<std::uint8_t*>(0x7df10fu)=v.active_nodes;
    *reinterpret_cast<std::uint8_t*>(0x7dd138u)=v.local_car_id;
    *reinterpret_cast<std::uint8_t*>(0x7de418u)=v.handicap_table_index;
}
constexpr std::uint32_t ReverseCarStubBlock=0x3200d000u;
constexpr std::uint32_t ReverseCarStateBase=0x33019000u;
void map_reverse_car_stubs(){
    auto* code=static_cast<std::uint8_t*>(map_at(ReverseCarStubBlock,4096,PROT_READ|PROT_WRITE));
    map_at(ReverseCarStateBase,4096,PROT_READ|PROT_WRITE);
    // GetCarParam-like lookup: return the controlled P arena regardless of kind.
    { const std::uint8_t q[]={0xb8,0x00,0x50,0x00,0x2f,0xc3};std::memcpy(code+0x000,q,sizeof(q)); }
    // RNG: return the deterministic harness cell.
    { std::uint8_t q[]={0xa1,0,0,0,0,0xc3};const std::uint32_t cell=ReverseCarStateBase;std::memcpy(q+1,&cell,4);std::memcpy(code+0x020,q,sizeof(q)); }
    auto patch_call=[&](std::uint32_t va,std::uint32_t expected,std::uint32_t target){
        auto* q=reinterpret_cast<std::uint8_t*>(va);std::int32_t rel{};std::memcpy(&rel,q+1,4);
        if(q[0]!=0xe8||std::uint32_t(std::int64_t(va+5u)+rel)!=expected)throw std::runtime_error("CheckReverseCar CALL target changed");
        auto* page=reinterpret_cast<void*>(va&~0xfffu);if(mprotect(page,4096,PROT_READ|PROT_WRITE|PROT_EXEC))throw std::runtime_error("reverse-car patch protection");
        const auto newrel=target-(va+5u);std::memcpy(q+1,&newrel,4);if(mprotect(page,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("reverse-car patch restore protection");
    };
    patch_call(0x4a291eu,0x4866c0u,ReverseCarStubBlock+0x000u);
    patch_call(0x4a29eeu,0x580f40u,ReverseCarStubBlock+0x020u);
    if(mprotect(code,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("reverse-car stub mprotect");
    // Game mode 0x78026c is a controlled explicit input for this target.
    if(mprotect(reinterpret_cast<void*>(0x780000u),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("reverse-car mode page mprotect");
}
void set_reverse_car_inputs(const PcReverseCarInputs& v){
    *reinterpret_cast<std::uint32_t*>(ReverseCarStateBase)=v.random_value;
    *reinterpret_cast<std::int32_t*>(0x78026cu)=v.game_mode;
}
constexpr std::uint32_t OfsLaneStubBlock=0x3200e000u;
constexpr std::uint32_t OfsLaneStateBase=0x3301a000u;
void map_ofs_left_lane_stubs(){
    auto* code=static_cast<std::uint8_t*>(map_at(OfsLaneStubBlock,4096,PROT_READ|PROT_WRITE));
    map_at(OfsLaneStateBase,4096,PROT_READ|PROT_WRITE);
    auto emit_u32=[](std::vector<std::uint8_t>& b,std::uint32_t v){for(unsigned k=0;k<4;++k)b.push_back(std::uint8_t(v>>(8u*k)));};
    auto emit_load_abs=[&](std::vector<std::uint8_t>& b,std::uint32_t addr){b.push_back(0xa1);emit_u32(b,addr);};
    auto emit_load_edx_arg=[&](std::vector<std::uint8_t>& b,std::uint8_t off){b.insert(b.end(),{0x8b,0x54,0x24,off});};
    auto emit_store_eax=[&](std::vector<std::uint8_t>& b,unsigned off){
        if(off==0)b.insert(b.end(),{0x89,0x02}); else b.insert(b.end(),{0x89,0x42,std::uint8_t(off)});
    };
    // PC 0x43D130 road service: six cdecl arguments, four float output pointers.
    std::vector<std::uint8_t> rates;
    for(unsigned i=0;i<4;++i){emit_load_abs(rates,OfsLaneStateBase+0x04u+i*4u);emit_load_edx_arg(rates,std::uint8_t(0x0c+4*i));emit_store_eax(rates,0);}
    rates.push_back(0xc3);std::memcpy(code+0x000,rates.data(),rates.size());
    // PC 0x43D1D0 collision service: same leading selectors, four Vec3 outputs.
    std::vector<std::uint8_t> points;
    for(unsigned i=0;i<4;++i){
        emit_load_edx_arg(points,std::uint8_t(0x0c+4*i));
        for(unsigned k=0;k<3;++k){emit_load_abs(points,OfsLaneStateBase+0x20u+i*12u+k*4u);emit_store_eax(points,k*4u);}
    }
    points.push_back(0xc3);std::memcpy(code+0x100,points.data(),points.size());
    auto patch_call=[&](std::uint32_t va,std::uint32_t expected,std::uint32_t target){
        auto* q=reinterpret_cast<std::uint8_t*>(va);std::int32_t rel{};std::memcpy(&rel,q+1,4);
        if(q[0]!=0xe8||std::uint32_t(std::int64_t(va+5u)+rel)!=expected)throw std::runtime_error("CalcOfsLeftLane CALL target changed");
        auto* page=reinterpret_cast<void*>(va&~0xfffu);if(mprotect(page,4096,PROT_READ|PROT_WRITE|PROT_EXEC))throw std::runtime_error("lane patch protection");
        const auto newrel=target-(va+5u);std::memcpy(q+1,&newrel,4);if(mprotect(page,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("lane patch restore protection");
    };
    patch_call(0x4a4659u,0x43d130u,OfsLaneStubBlock+0x000u);
    patch_call(0x4a4674u,0x43d1d0u,OfsLaneStubBlock+0x100u);
    // Replace the six-byte packed/protected selector jump with mov edi,[state].
    {
        auto* q=reinterpret_cast<std::uint8_t*>(0x4a45f9u);
        const std::uint8_t expected[]={0xff,0x25,0xb8,0x98,0x03,0x01};
        if(!g_steam_exe&&std::memcmp(q,expected,sizeof(expected))!=0)throw std::runtime_error("CalcOfsLeftLane protected entry changed");
        auto* page=reinterpret_cast<void*>(0x4a4000u);if(mprotect(page,4096,PROT_READ|PROT_WRITE|PROT_EXEC))throw std::runtime_error("lane selector patch protection");
        q[0]=0x8b;q[1]=0x3d;const std::uint32_t cell=OfsLaneStateBase;std::memcpy(q+2,&cell,4);
        if(mprotect(page,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("lane selector patch restore protection");
    }
    if(mprotect(code,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("lane stub mprotect");
}
void set_ofs_left_lane_inputs(const PcOfsLeftLaneInputs& v){
    *reinterpret_cast<std::int32_t*>(OfsLaneStateBase)=v.selector;
    for(unsigned i=0;i<4;++i)*reinterpret_cast<float*>(OfsLaneStateBase+0x04u+i*4u)=v.rates[i];
    for(unsigned i=0;i<4;++i){
        *reinterpret_cast<float*>(OfsLaneStateBase+0x20u+i*12u+0u)=v.points[i].x;
        *reinterpret_cast<float*>(OfsLaneStateBase+0x20u+i*12u+4u)=v.points[i].y;
        *reinterpret_cast<float*>(OfsLaneStateBase+0x20u+i*12u+8u)=v.points[i].z;
    }
}
constexpr std::uint32_t CourseTableArena=0x33020000;
constexpr std::uint32_t CourseLengthArena=0x33040000;
constexpr std::uint32_t D3DX_BASE=0x18000000;
constexpr std::uint32_t MatrixArena=0x33000000;

std::uint32_t u32le(const std::vector<std::uint8_t>& b,std::size_t o){
    if(o+4>b.size())throw std::runtime_error("PE u32 outside file");
    return std::uint32_t(b[o])|(std::uint32_t(b[o+1])<<8)|(std::uint32_t(b[o+2])<<16)|(std::uint32_t(b[o+3])<<24);
}
std::uint16_t u16le(const std::vector<std::uint8_t>& b,std::size_t o){
    if(o+2>b.size())throw std::runtime_error("PE u16 outside file");
    return std::uint16_t(b[o]|(std::uint16_t(b[o+1])<<8));
}
std::size_t pe_file_offset(const std::vector<std::uint8_t>& b,std::uint32_t va){
    const auto pe=u32le(b,0x3c),opt=pe+24u;
    const std::uint32_t image=u32le(b,opt+28u),section=opt+u16le(b,pe+20u); const std::uint16_t count=u16le(b,pe+6u);
    if(va<image)throw std::runtime_error("VA below PE image");
    const auto rva=va-image;
    for(unsigned i=0;i<count;++i){
        const auto o=section+i*40u,vs=u32le(b,o+8u),srva=u32le(b,o+12u),raw=u32le(b,o+16u),ptr=u32le(b,o+20u);
        if(rva>=srva && rva<srva+raw){const auto f=std::size_t(ptr)+std::size_t(rva-srva);if(f>=b.size())break;return f;}
        (void)vs;
    }
    throw std::runtime_error("VA has no raw PE bytes");
}
bool protected_bridge_mapped=false;
// The pinned public secondary/query entries use the original arithmetic VM
// bridge. Map its original sections without entering the game, substituting
// imports, patching its calculations, or embedding any commercial bytes.
void map_original_arithmetic_bridge(const std::vector<std::uint8_t>& file){
    const auto pe=u32le(file,0x3c),opt=pe+24u,sec=opt+u16le(file,pe+20u);
    for(unsigned j=0;j<u16le(file,pe+6u);++j){const auto o=sec+j*40u;char name[9]{};std::memcpy(name,file.data()+o,8);
        if(std::string(name)!="credo"&&std::string(name)!="quia"&&std::string(name)!="absurdum"&&std::string(name)!=".rld")continue;
        const auto va=0x400000u+u32le(file,o+12),raw=u32le(file,o+16),ptr=u32le(file,o+20);
        const auto len=(std::max(raw,u32le(file,o+8))+4095u)&~4095u;
        if(std::uint64_t(ptr)+raw>file.size())throw std::runtime_error("invalid bridge section");
        auto* mem=map_at(va,len,PROT_READ|PROT_WRITE|PROT_EXEC);std::memcpy(mem,file.data()+ptr,raw);
    }
    protected_bridge_mapped=true;
}
void map_original_read_page(const std::vector<std::uint8_t>& file,std::uint32_t page){
    if(protected_bridge_mapped&&page>=0x100a000u&&page<0x145a000u)return;
    auto* dst=static_cast<std::uint8_t*>(map_at(page,4096,PROT_READ|PROT_WRITE));
    std::memset(dst,0,4096);
    for(std::size_t i=0;i<4096;++i){
        try{dst[i]=file[pe_file_offset(file,page+std::uint32_t(i))];}
        catch(const std::runtime_error&){break;}
    }
    if(mprotect(dst,4096,PROT_READ))throw std::runtime_error("protected data page mprotect");
}
void map_original_exec_page(const std::vector<std::uint8_t>& file,std::uint32_t page){
    if(protected_bridge_mapped&&page>=0x100a000u&&page<0x145a000u)return;
    auto* dst=static_cast<std::uint8_t*>(map_at(page,4096,PROT_READ|PROT_WRITE));
    std::memset(dst,0,4096);
    for(std::size_t i=0;i<4096;++i){
        try{dst[i]=file[pe_file_offset(file,page+std::uint32_t(i))];}
        catch(const std::runtime_error&){break;}
    }
    if(mprotect(dst,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("protected code page mprotect");
}
struct D3dxExports {
    std::uint32_t matrix_multiply=0,vec3_normalize=0,vec3_transform_coord=0,vec3_transform_normal=0;
    std::uint32_t matrix_rotation_axis=0,matrix_rotation_x=0,matrix_rotation_y=0,matrix_rotation_z=0,matrix_translation=0;
};
std::vector<std::uint8_t> load_any(const char* path){
    std::ifstream f(path,std::ios::binary);if(!f)throw std::runtime_error(std::string("cannot open ")+path);
    f.seekg(0,std::ios::end);auto n=f.tellg();if(n<=0)throw std::runtime_error("empty input file");
    f.seekg(0);std::vector<std::uint8_t>b(static_cast<std::size_t>(n));
    if(!f.read(reinterpret_cast<char*>(b.data()),n))throw std::runtime_error("short auxiliary file read");
    return b;
}
D3dxExports map_d3dx29(const char* path){
    auto file=load_any(path);
    if(file.size()!=2332368u)throw std::runtime_error("unexpected d3dx9_29.dll size");
    const auto pe=u32le(file,0x3c),opt=pe+24u;
    if(u32le(file,pe)!=0x4550u||u16le(file,pe+4u)!=0x14cu)throw std::runtime_error("unsupported D3DX PE");
    const auto preferred=u32le(file,opt+28u),image_size=u32le(file,opt+56u),headers=u32le(file,opt+60u);
    if(preferred!=0x400000u||image_size!=0x252000u)throw std::runtime_error("unexpected D3DX image layout");
    auto* image=static_cast<std::uint8_t*>(map_at(D3DX_BASE,image_size,PROT_READ|PROT_WRITE));
    std::memset(image,0,image_size);std::memcpy(image,file.data(),std::min<std::size_t>(headers,file.size()));
    const std::uint32_t section=opt+u16le(file,pe+20u); const std::uint16_t count=u16le(file,pe+6u);
    struct Sec{std::uint32_t rva,size,flags;};std::vector<Sec> sections;
    for(unsigned i=0;i<count;++i){
        const auto o=section+i*40u,vs=u32le(file,o+8u),rva=u32le(file,o+12u),raw=u32le(file,o+16u),ptr=u32le(file,o+20u),flags=u32le(file,o+36u);
        if(std::uint64_t(rva)+std::max(vs,raw)>image_size||std::uint64_t(ptr)+raw>file.size())throw std::runtime_error("D3DX section outside image/file");
        if(raw)std::memcpy(image+rva,file.data()+ptr,raw);
        sections.push_back({rva,std::max(vs,raw),flags});
    }
    const auto delta=D3DX_BASE-preferred;
    const auto reloc_rva=u32le(file,opt+96u+5u*8u),reloc_size=u32le(file,opt+96u+5u*8u+4u);
    if(!reloc_rva||std::uint64_t(reloc_rva)+reloc_size>image_size)throw std::runtime_error("missing/invalid D3DX relocations");
    std::uint32_t pos=reloc_rva,end=reloc_rva+reloc_size;
    while(pos+8u<=end){
        const auto page=*reinterpret_cast<std::uint32_t*>(image+pos),block=*reinterpret_cast<std::uint32_t*>(image+pos+4u);
        if(block<8u||pos+block>end)throw std::runtime_error("invalid D3DX relocation block");
        const auto entries=(block-8u)/2u;
        for(std::uint32_t i=0;i<entries;++i){
            const std::uint16_t e=*reinterpret_cast<std::uint16_t*>(image+pos+8u+i*2u); const std::uint16_t type=std::uint16_t(e>>12),off=std::uint16_t(e&0xfffu);
            if(type==0)continue;
            if(type!=3)throw std::runtime_error("unsupported D3DX relocation type");
            if(std::uint64_t(page)+off+4u>image_size)throw std::runtime_error("D3DX relocation outside image");
            auto* cell=reinterpret_cast<std::uint32_t*>(image+page+off);*cell+=delta;
        }
        pos+=block;
    }
    // The DLL normally rewrites these dispatch slots during DllMain/CPU probing.
    // We bypass platform initialization and select its own generic x87 routines.
    auto patch_dispatch=[&](std::uint32_t dst_va,std::uint32_t generic_va){
        *reinterpret_cast<std::uint32_t*>(image+(dst_va-preferred))=D3DX_BASE+(generic_va-preferred);
    };
    patch_dispatch(0x610054u,0x440943u); // D3DXMatrixMultiply
    patch_dispatch(0x610060u,0x440070u); // D3DXVec3TransformNormal
    patch_dispatch(0x610064u,0x4438c4u); // D3DXVec3Normalize
    patch_dispatch(0x610070u,0x443aa8u); // D3DXVec3TransformCoord
    for(const auto& sec:sections){
        const auto start=sec.rva&~0xfffu,finish=(sec.rva+sec.size+0xfffu)&~0xfffu;
        int prot=PROT_READ;if(sec.flags&0x20000000u)prot|=PROT_EXEC;if(sec.flags&0x80000000u)prot|=PROT_WRITE;
        if(finish>start && mprotect(image+start,finish-start,prot))throw std::runtime_error("D3DX mprotect failed");
    }
    return {D3DX_BASE+(0x440943u-preferred),D3DX_BASE+(0x4438c4u-preferred),
            D3DX_BASE+(0x443aa8u-preferred),D3DX_BASE+(0x440070u-preferred),
            D3DX_BASE+(0x4415d7u-preferred),D3DX_BASE+(0x4413beu-preferred),
            D3DX_BASE+(0x44146fu-preferred),D3DX_BASE+(0x441521u-preferred),
            D3DX_BASE+(0x44132bu-preferred)};
}
void wire_d3dx_imports(const D3dxExports& ex){
    auto* page=reinterpret_cast<void*>(0x596000u);
    if(mprotect(page,4096,PROT_READ|PROT_WRITE))throw std::runtime_error("IAT mprotect RW failed");
    *reinterpret_cast<std::uint32_t*>(0x5962d8u)=ex.matrix_multiply;
    *reinterpret_cast<std::uint32_t*>(0x5962a4u)=ex.vec3_normalize;
    *reinterpret_cast<std::uint32_t*>(0x5962d4u)=ex.vec3_transform_coord;
    *reinterpret_cast<std::uint32_t*>(0x5962a8u)=ex.vec3_transform_normal;
    *reinterpret_cast<std::uint32_t*>(0x5962acu)=ex.matrix_rotation_axis;
    *reinterpret_cast<std::uint32_t*>(0x5962bcu)=ex.matrix_rotation_x;
    *reinterpret_cast<std::uint32_t*>(0x5962c4u)=ex.matrix_rotation_y;
    *reinterpret_cast<std::uint32_t*>(0x5962c0u)=ex.matrix_rotation_z;
    *reinterpret_cast<std::uint32_t*>(0x5962ccu)=ex.matrix_translation;
    if(mprotect(page,4096,PROT_READ))throw std::runtime_error("IAT mprotect R failed");
}
void init_course_table_oracle(){
    map_at(CourseTableArena,0x20000,PROT_READ|PROT_WRITE);
    map_at(CourseLengthArena,0x20000,PROT_READ|PROT_WRITE);
    auto* page=reinterpret_cast<void*>(0x780000u);
    if(mprotect(page,4096,PROT_READ|PROT_WRITE))throw std::runtime_error("course globals mprotect failed");
    *reinterpret_cast<std::uint32_t*>(0x780110u)=CourseTableArena;
    *reinterpret_cast<std::uint32_t*>(0x780114u)=CourseTableArena+0x10000u;
    *reinterpret_cast<std::uint32_t*>(0x780228u)=CourseLengthArena;
    *reinterpret_cast<std::uint32_t*>(0x78022cu)=CourseLengthArena+0x10000u;
    // Controlled area-display matrices returned by GetAreaDispMatrix (0x44BEF0).
    if(mprotect(reinterpret_cast<void*>(0x7d3000u),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("course area-matrix page mprotect failed");
}
void init_matrix_oracle_globals(){
    map_at(MatrixArena,0x10000,PROT_READ|PROT_WRITE);
    auto* page=reinterpret_cast<void*>(0x89b000u);
    if(mprotect(page,4096,PROT_READ|PROT_WRITE))throw std::runtime_error("matrix globals mprotect failed");
}
void reset_matrix_oracle_globals(){
    std::memset(reinterpret_cast<void*>(MatrixArena),0,0x10000);
    *reinterpret_cast<std::uint32_t*>(0x89b564u)=MatrixArena;
    *reinterpret_cast<std::int32_t*>(0x89b568u)=0;
    *reinterpret_cast<std::int32_t*>(0x89b56cu)=64;
}
// Original inline block has no RET and leaves one temporary argument on the stack.
// Copy it unchanged except its one relative CALL relocation, then append add esp,4; ret.
// No game function is patched; this is NOT a replacement for DrivingControl.
constexpr std::uint32_t ContactMatrixBlock=0x32005000;
void map_contact_matrix_body(){
    // CalcContactMatrix public entry enters the .rld protector after establishing
    // EBP/alignment.  The symbolized Lindbergh twin proves 0x4A63CC is the
    // semantic body.  This harness-only wrapper supplies a private local frame
    // and jumps to that body; no game arithmetic instruction is patched.
    auto* code=static_cast<std::uint8_t*>(map_at(ContactMatrixBlock,4096,PROT_READ|PROT_WRITE));
    std::size_t o=0;
    auto emit=[&](std::initializer_list<std::uint8_t> v){for(auto b:v)code[o++]=b;};
    emit({0x55,0x8b,0xec,0x83,0xe4,0xf0});             // push ebp; mov ebp,esp; and esp,-16
    emit({0x81,0xec,0x00,0x01,0x00,0x00});            // sub esp,0x100
    emit({0xe9});
    const std::int32_t rel=std::int32_t(0x004a63ccu-(ContactMatrixBlock+std::uint32_t(o)+4u));
    std::memcpy(code+o,&rel,4);o+=4;
    if(mprotect(code,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("contact matrix body wrapper mprotect");
}
void patch_maximum_velocity_contact_call(){
    // MaximumVelocityCheck calls the protected public CalcContactMatrix entry.
    // Route only that CALL through the already-audited semantic-body wrapper so
    // the oracle exercises original MaximumVelocityCheck arithmetic unchanged.
    constexpr std::uint32_t call_va=0x004a67dbu;
    auto* page=reinterpret_cast<void*>(call_va & ~0xfffu);
    if(mprotect(page,4096,PROT_READ|PROT_WRITE|PROT_EXEC))throw std::runtime_error("maximum velocity call patch mprotect RWX failed");
    auto* op=reinterpret_cast<std::uint8_t*>(call_va);
    if(op[0]!=0xe8)throw std::runtime_error("MaximumVelocityCheck contact CALL opcode audit failed");
    std::int32_t old_rel=0;std::memcpy(&old_rel,op+1,4);
    if(call_va+5u+std::uint32_t(old_rel)!=0x004a63c0u)throw std::runtime_error("MaximumVelocityCheck contact CALL target audit failed");
    const std::int32_t rel=std::int32_t(ContactMatrixBlock-(call_va+5u));
    std::memcpy(op+1,&rel,4);
    if(mprotect(page,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("maximum velocity call patch mprotect RX failed");
}
constexpr std::uint32_t BrakeBlock=0x32000000;
constexpr std::uint32_t DirectionAnglesBlock=0x32001000;
constexpr std::uint32_t AutoTransmissionBlock=0x32002000;
constexpr std::uint32_t ManualTransmissionBlock=0x32003000;
constexpr std::uint32_t ManualStateBase=0x33010000;
constexpr std::uint32_t RoadMuStubBlock=0x32004000;
constexpr std::uint32_t RoadMuStateBase=0x33011000;
void map_brake_block(){
    constexpr std::uint32_t begin=0x502ce2,end=0x502d9d;
    auto* code=static_cast<std::uint8_t*>(map_at(BrakeBlock,4096,PROT_READ|PROT_WRITE));
    std::memcpy(code,reinterpret_cast<void*>(begin),end-begin);
    Bytes b(code,4096);
    if(b.u8(4)!=0xe8 || begin+9+b.u32(5)!=0x520960)throw std::runtime_error("brake block CALL audit failed");
    b.put32(5,0x520960-(BrakeBlock+9));
    const std::uint8_t tail[]={0x83,0xc4,0x04,0xc3};std::memcpy(code+(end-begin),tail,sizeof(tail));
    if(mprotect(code,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("brake block mprotect");
}
// Closed prefix of CalcTireDirection, ending immediately before its D3DX
// matrix/vector calls. Copying avoids executing unresolved Windows imports.
void map_direction_angles_block(){
    constexpr std::uint32_t begin=0x500700,end=0x500874;
    auto* code=static_cast<std::uint8_t*>(map_at(DirectionAnglesBlock,4096,PROT_READ|PROT_WRITE));
    std::memcpy(code,reinterpret_cast<void*>(begin),end-begin);
    Bytes b(code,4096);
    constexpr std::uint32_t calls[]={0x500721,0x500747,0x50076d,0x50078f};
    for(auto va:calls){
        const auto off=va-begin;
        if(b.u8(off)!=0xe8)throw std::runtime_error("direction-angle CALL audit failed");
        const std::int32_t old_rel=b.i32(off+1);
        const std::uint32_t target=va+5u+static_cast<std::uint32_t>(old_rel);
        if(target!=0x4493b0)throw std::runtime_error("direction-angle helper target changed");
        const auto new_rel=static_cast<std::uint32_t>(target-(DirectionAnglesBlock+off+5u));
        b.put32(off+1,new_rel);
    }
    const std::uint8_t tail[]={0x5f,0x5e,0x5d,0x5b,0x83,0xc4,0x30,0xc3};
    std::memcpy(code+(end-begin),tail,sizeof(tail));
    if(mprotect(code,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("direction-angle block mprotect");
}
void map_auto_transmission_block(){
    constexpr std::uint32_t begin=0x502420,end=0x5024b1;
    auto* code=static_cast<std::uint8_t*>(map_at(AutoTransmissionBlock,4096,PROT_READ|PROT_WRITE));
    std::memcpy(code,reinterpret_cast<void*>(begin),end-begin);Bytes b(code,4096);
    const auto off=0x502428u-begin;
    if(!g_steam_exe&&(b.u8(off)!=0xff||b.u8(off+1)!=0x25||b.u32(off+2)!=0x01039cecu))
        throw std::runtime_error("AutoTransmission protection trampoline changed");
    // Lindbergh symbolized AutoTransmission compares pedal > 0x7f here.
    // PC control flow uses JL for its low-pedal table, hence CMP ESI,0x80.
    const std::uint8_t cmp[]={0x81,0xfe,0x80,0x00,0x00,0x00};
    std::memcpy(code+off,cmp,sizeof(cmp));
    if(mprotect(code,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("auto transmission block mprotect");
}
void map_manual_transmission_block(){
    constexpr std::uint32_t begin=0x5024c0,end=0x5025b2;
    auto* code=static_cast<std::uint8_t*>(map_at(ManualTransmissionBlock,4096,PROT_READ|PROT_WRITE));
    std::memcpy(code,reinterpret_cast<void*>(begin),end-begin);Bytes b(code,4096);
    // Three platform queries become deterministic tiny x86 stubs.  The game
    // code itself, including all state changes and predicted-RPM call, remains
    // the original PC byte sequence.
    auto patch_call=[&](std::uint32_t va,std::uint32_t expected,std::uint32_t target){
        const auto off=va-begin;if(b.u8(off)!=0xe8)throw std::runtime_error("manual CALL audit opcode");
        const auto old=std::uint32_t(std::int64_t(va+5)+std::int32_t(b.u32(off+1)));
        if(old!=expected)throw std::runtime_error("manual CALL audit target");
        b.put32(off+1,target-(ManualTransmissionBlock+off+5u));
    };
    const auto stub=[&](std::size_t off,std::uint32_t cell){
        std::uint8_t bytes[]={0xa1,0,0,0,0,0xc3};std::memcpy(bytes+1,&cell,4);std::memcpy(code+off,bytes,sizeof(bytes));
        return ManualTransmissionBlock+std::uint32_t(off);
    };
    const auto inhibited=stub(0x300,ManualStateBase+0),up=stub(0x310,ManualStateBase+4),down=stub(0x320,ManualStateBase+8);
    patch_call(0x5024d1,0x502420,AutoTransmissionBlock);
    patch_call(0x5024d9,0x43f9f0,inhibited);
    patch_call(0x5024eb,0x4536f0,up);
    patch_call(0x502528,0x4536f0,down);
    patch_call(0x502550,0x502070,0x502070);
    if(mprotect(code,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("manual transmission block mprotect");
    map_at(ManualStateBase,4096,PROT_READ|PROT_WRITE);
}
void set_manual_inputs(bool inhibited,bool up,bool down){
    *reinterpret_cast<std::uint32_t*>(ManualStateBase+0)=inhibited?1u:0u;
    *reinterpret_cast<std::uint32_t*>(ManualStateBase+4)=up?1u:0u;
    *reinterpret_cast<std::uint32_t*>(ManualStateBase+8)=down?1u:0u;
}
void map_road_mu_stubs(){
    auto* state=static_cast<std::uint8_t*>(map_at(RoadMuStateBase,4096,PROT_READ|PROT_WRITE));
    std::memset(state,0,4096);
    *reinterpret_cast<float*>(state+8)=0.0625f;
    *reinterpret_cast<float*>(state+12)=0.25f;
    auto* code=static_cast<std::uint8_t*>(map_at(RoadMuStubBlock,4096,PROT_READ|PROT_WRITE));
    // road-system-ready(): mov eax,[state]; ret
    code[0]=0xa1;std::memcpy(code+1,&RoadMuStateBase,4);code[5]=0xc3;
    // lookup(route,surface): return float(((route^surface)&255))/16 + 0.25 in ST0.
    std::size_t o=0x100;auto emit=[&](std::initializer_list<std::uint8_t> v){for(auto b:v)code[o++]=b;};
    auto abs32=[&](std::uint32_t v){std::memcpy(code+o,&v,4);o+=4;};
    emit({0x8b,0x44,0x24,0x04});              // mov eax,[esp+4]
    emit({0x33,0x44,0x24,0x08});              // xor eax,[esp+8]
    emit({0x25,0xff,0x00,0x00,0x00});         // and eax,255
    emit({0xa3});abs32(RoadMuStateBase+4);      // mov [scratch],eax
    emit({0xdb,0x05});abs32(RoadMuStateBase+4); // fild dword [scratch]
    emit({0xd8,0x0d});abs32(RoadMuStateBase+8); // fmul dword [1/16]
    emit({0xd8,0x05});abs32(RoadMuStateBase+12);// fadd dword [1/4]
    emit({0xc3});
    if(mprotect(code,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("road Mu stub mprotect");

    constexpr std::uint32_t page=0x500000u;
    if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE|PROT_EXEC))
        throw std::runtime_error("road Mu original page mprotect RWX");
    Bytes b(reinterpret_cast<void*>(page),4096);
    auto patch=[&](std::uint32_t va,std::uint32_t expected,std::uint32_t target){
        const auto off=std::size_t(va-page);
        if(b.u8(off)!=0xe8)throw std::runtime_error("road Mu CALL audit opcode");
        const auto old=std::uint32_t(std::int64_t(va+5)+std::int32_t(b.u32(off+1)));
        if(old!=expected)throw std::runtime_error("road Mu CALL audit target");
        b.put32(off+1,target-(va+5u));
    };
    const std::uint32_t ready=RoadMuStubBlock,lookup=RoadMuStubBlock+0x100;
    patch(0x500b74,0x55a930,ready);patch(0x500ba4,0x55a930,ready);
    patch(0x500bff,0x55a930,ready);patch(0x500c1d,0x55a930,ready);
    patch(0x500b88,0x46c450,lookup);
    if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_EXEC))
        throw std::runtime_error("road Mu original page mprotect RX");
}
void set_road_mu_available(bool available){
    *reinterpret_cast<std::uint32_t*>(RoadMuStateBase)=available?1u:0u;
}
float deterministic_road_mu(void*,std::uint32_t road,std::uint32_t surface){
    return float((road^surface)&0xffu)*0.0625f+0.25f;
}
void patch_full_driving_platform(){
    constexpr std::uint32_t page=0x502000u;
    if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE|PROT_EXEC))
        throw std::runtime_error("DrivingControl platform page mprotect RWX");
    Bytes b(reinterpret_cast<void*>(page),4096);
    // Deprotect the one hidden AutoTransmission comparison in-place.  The
    // replacement is the same comparison validated against Lindbergh + PC body.
    const auto auto_off=std::size_t(0x502428u-page);
    if(!g_steam_exe&&(b.u8(auto_off)!=0xff||b.u8(auto_off+1)!=0x25||b.u32(auto_off+2)!=0x01039cecu))
        throw std::runtime_error("full DrivingControl AutoTransmission trampoline changed");
    const std::uint8_t cmp[]={0x81,0xfe,0x80,0x00,0x00,0x00};
    std::memcpy(reinterpret_cast<void*>(0x502428u),cmp,sizeof(cmp));
    auto patch=[&](std::uint32_t va,std::uint32_t expected,std::uint32_t target){
        const auto off=std::size_t(va-page);if(b.u8(off)!=0xe8)throw std::runtime_error("full manual CALL audit opcode");
        const auto old=std::uint32_t(std::int64_t(va+5)+std::int32_t(b.u32(off+1)));
        if(old!=expected)throw std::runtime_error("full manual CALL audit target");
        b.put32(off+1,target-(va+5u));
    };
    patch(0x5024d9,0x43f9f0,ManualTransmissionBlock+0x300);
    patch(0x5024eb,0x4536f0,ManualTransmissionBlock+0x310);
    patch(0x502528,0x4536f0,ManualTransmissionBlock+0x320);
    if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_EXEC))
        throw std::runtime_error("DrivingControl platform page mprotect RX");
}
void (*g_signal_dump)()=nullptr;   // probe hook: dump its own state when the original faults
void signal_handler(int n,siginfo_t* si,void* vp){
    auto* u=static_cast<ucontext_t*>(vp);
    if(g_signal_dump)g_signal_dump();
    dprintf(2,"Original-code oracle signal=%d address=%p RIP=%llx entry=%x\n",n,si->si_addr,
      static_cast<unsigned long long>(u->uc_mcontext.gregs[REG_RIP]),guest_call.entry);
    if(getenv("OR2_SIGNAL_STACK")){   // the guest stack around the fault (32-bit dwords)
        const auto sp=static_cast<unsigned long long>(u->uc_mcontext.gregs[REG_RSP]);
        dprintf(2,"rsp=%llx eax=%llx ecx=%llx edx=%llx ebx=%llx ebp=%llx esi=%llx edi=%llx\n",sp,
          static_cast<unsigned long long>(u->uc_mcontext.gregs[REG_RAX]),static_cast<unsigned long long>(u->uc_mcontext.gregs[REG_RCX]),
          static_cast<unsigned long long>(u->uc_mcontext.gregs[REG_RDX]),static_cast<unsigned long long>(u->uc_mcontext.gregs[REG_RBX]),
          static_cast<unsigned long long>(u->uc_mcontext.gregs[REG_RBP]),static_cast<unsigned long long>(u->uc_mcontext.gregs[REG_RSI]),
          static_cast<unsigned long long>(u->uc_mcontext.gregs[REG_RDI]));
        if(sp>=0x10000&&sp<0x100000000ull)for(int i=-8;i<200;++i)dprintf(2,"%x%c",*reinterpret_cast<const unsigned*>(sp+4ll*i),i%8==7?'\n':' ');
    }
    _exit(128+n);
}
void setup_signals(){
    static std::array<char,65536> alternate{};
    stack_t ss{};ss.ss_sp=alternate.data();ss.ss_size=alternate.size();sigaltstack(&ss,nullptr);
    struct sigaction sa{};sa.sa_sigaction=signal_handler;sa.sa_flags=SA_SIGINFO|SA_ONSTACK;
    sigaction(SIGSEGV,&sa,nullptr);sigaction(SIGILL,&sa,nullptr);sigaction(SIGBUS,&sa,nullptr);sigaction(SIGFPE,&sa,nullptr);
}
struct Fixture {
    Event e{};Work w{};Params p{};
    Bytes ev(){return Bytes(e.data(),e.size());} Bytes wk(){return Bytes(w.data(),w.size());}Bytes pa(){return Bytes(p.data(),p.size());}
    std::array<Bytes,4> wheels(){return {wk().sub(0x258,0xf4),wk().sub(0x34c,0xf4),wk().sub(0x440,0xf4),wk().sub(0x534,0xf4)};}
    void guest(){std::memcpy(reinterpret_cast<void*>(E),e.data(),e.size());std::memcpy(reinterpret_cast<void*>(W),w.data(),w.size());std::memcpy(reinterpret_cast<void*>(P),p.data(),p.size());}
};
struct Stat {std::uint64_t cases=0,exact=0,fail=0,bytes=0;std::uint32_t max_ulp=0;};
std::map<std::string,Stat> stats;
std::uint32_t x87_control=0x037f;
bool inject_mismatch=false;
std::ofstream snapshot_file;
std::uint32_t snapshot_count=0;
void snapshot_u32(std::uint32_t u){
    char b[4];for(unsigned j=0;j<4;++j)b[j]=char((u>>(8*j))&255);snapshot_file.write(b,4);
}
void begin_snapshots(const char* path){
    snapshot_file.open(path,std::ios::binary|std::ios::trunc);
    if(!snapshot_file)throw std::runtime_error("cannot open snapshot output");
    snapshot_file.write("OR2W005\0",8);
    snapshot_u32(1);snapshot_u32(event_size);snapshot_u32(work_size);snapshot_u32(parameter_size);
    snapshot_u32(0);snapshot_u32(x87_control);
    snapshot_file.write("bdafa88a5abdd2a9743f6bdcc5e2189288c0203790412fce10b9bb649933482f",64);
}
void snapshot_case(std::uint32_t id,unsigned n,const Fixture& before){
    if(!snapshot_file.is_open() || !(n<14 || n==31 || n==63))return;
    snapshot_u32(id);snapshot_u32(n);
    auto block=[&](const void* p,std::size_t size){snapshot_file.write(static_cast<const char*>(p),std::streamsize(size));};
    block(before.e.data(),before.e.size());block(before.w.data(),before.w.size());block(before.p.data(),before.p.size());
    // Expected bytes are copied from the ORIGINAL x86 arena, not native output.
    block(reinterpret_cast<void*>(E),before.e.size());block(reinterpret_cast<void*>(W),before.w.size());block(reinterpret_cast<void*>(P),before.p.size());
    if(!snapshot_file)throw std::runtime_error("snapshot write failed");
    ++snapshot_count;
}
void finish_snapshots(){
    if(snapshot_file.is_open()){
        snapshot_file.seekp(24);snapshot_u32(snapshot_count);snapshot_file.close();
        if(!snapshot_file)throw std::runtime_error("snapshot finalize failed");
    }
}
std::mt19937 rng(0x4f523034u);
std::uint32_t draw(std::uint32_t max){return rng()%(max+1);}
float sample(float lo,float hi){return lo+(hi-lo)*(float(draw(65535))/65535.0f);}
Fixture fixture(unsigned n){
    Fixture x;
    // Nonzero canaries in unused fields make unintended writes detectable.
    for(auto& b:x.e)b=std::uint8_t(rng());
    for(auto& b:x.w)b=std::uint8_t(rng());
    for(auto& b:x.p)b=std::uint8_t(rng());
    auto e=x.ev(),w=x.wk(),p=x.pa();
    e.put32(0x2b4,P);e.put32(4,(n%13==0?8u:0u)|(n%3==0?0x100000u:0u));
    e.put8(0x13,std::uint8_t(n%2));e.put8(0x11,std::uint8_t(n%21));e.put32(0xe84,n%5==0?1:0);
    e.putf(0x21c,sample(-20,1250));e.puti(0x34,int(draw(255)));e.puti(0x38,int(draw(255)));e.puti(0x3c,int(draw(300))-20);
    e.put32(0x208,n%7);e.put32(0x1f4,n%9==0?0:draw(300));e.put32(0x48,draw(8000));e.put32(0x210,draw(8000));
    e.putf(0x22c,sample(0,1));e.putf(0x2a0,sample(0,1));e.putf(0x2f8,n%17==0?1.0f:0.0f);
    e.putf(0xe6c,sample(-0.2f,1.2f));e.put8(0x296,std::uint8_t(n%7));e.put8(0x297,std::uint8_t((n/7)%7));e.puti(0x29c,int(draw(255)));
    e.put8(0x283,n%11==0?1:0);e.put32(0x304,n%4==0?0:draw(1000));e.put32(0x308,1000);
    e.put16(0x4e,std::uint16_t(draw(65535)));
    p.putf(0x1644,sample(700,1000));p.putf(0x1690,1250);p.putf(0x15f8,sample(70,140));p.put32(0x10a0,6);
    p.putf(0x1560,sample(0.07f,0.11f));p.putf(0x15ac,sample(0.06f,0.1f));p.putf(0x1514,8);
    p.putf(0x1728,sample(0.7f,1.3f));p.putf(0x1774,sample(100,700));
    p.putf(0xb94,sample(0.2f,0.4f));p.putf(0x134c,sample(2,5));p.putf(0x10ec,sample(0.7f,1.3f));
    for(unsigned gear=0;gear<=7;++gear)p.putf((gear+0x3a)*0x4c,sample(0.5f,3));
    p.putf(0xe40,sample(1,2));p.putf(0xed8,sample(0.3f,1));p.putf(0xe8c,sample(1,2));p.putf(0xf24,sample(0.3f,1));
    for(unsigned i=0;i<4;++i){auto off=embedded_wheel_offsets[i];w.put32(0x248+i*4,W+std::uint32_t(off));
        w.putf(off+0x34,sample(0,12000));w.putf(off+0x38,sample(1000,6000));w.putf(off+0xe8,sample(0,2));w.putf(off+0xc0,sample(0,10000));}
    w.putf(0x524,sample(0,2));w.putf(0x618,sample(0,2));w.putf(0x514,sample(-100,100));w.putf(0x608,sample(-100,100));
    // Exact boundaries around the spin rear-wheel cutoff.
    constexpr std::uint16_t angles[]={0,8191,8192,8193,57345,57344,57343,32768,32767};
    w.put16(0x52e,angles[n%9]);w.put16(0x622,std::uint16_t(draw(65535)));
    // Targeted boundaries layered over deterministic finite-domain random cases.
    switch(n%64){
    case 0:e.putf(0x21c,-0.0f);break;
    case 1:e.putf(0x21c,0.0f);break;
    case 2:e.putf(0x21c,p.f32(0x1644));break;
    case 3:e.putf(0x21c,p.f32(0x1690));break;
    case 4:e.putf(0x21c,1500.0f);break;
    case 5:e.putf(0x21c,p.f32(0x15f8));break;
    case 6:e.putf(0x2a0,0.0f);break;
    case 7:e.putf(0x2a0,1.0f);break;
    case 8:e.puti(0x34,0);break;
    case 9:e.puti(0x34,255);break;
    case 10:e.puti(0x3c,127);break;
    case 11:e.puti(0x3c,128);break;
    case 12:e.puti(0x3c,255);break;
    case 13:e.puti(0x3c,256);break;
    case 14:e.put32(0x304,0xffffffff);e.put32(0x308,0xfffffffe);break;
    case 15:e.put32(0x304,1);e.put32(0x308,1);break;
    case 16:e.put32(0x1f4,0xffffffff);break;
    case 17:e.puti(0x3c,0x7fffffff);break;
    case 18:e.put32(0x3c,0x80000000);break;
    case 19:e.put8(0x296,0xff);e.put8(0x297,0xff);break;
    case 20:w.putf(0x524,1.0f);w.putf(0x618,1.0f);break;
    case 21:e.putf(0x21c,std::nextafter(p.f32(0x1644),0.0f));break;
    case 22:e.putf(0x21c,std::nextafter(p.f32(0x1644),2000.0f));break;
    default:break;
    }
    return x;
}
// Separate RNG: adding wheel tests does not change the r004 input corpus.
Fixture wheel_fixture(const Fixture& base,unsigned n){
    Fixture x=base;std::mt19937 gen(0x4f523035u ^ (n*1664525u));
    auto uf=[&](float lo,float hi){return lo+(hi-lo)*(float(gen()%65536)/65535.0f);};
    auto e=x.ev(),p=x.pa();
    e.putf(0x214,uf(-800,1400));e.putf(0x228,uf(0,600));e.putf(0x2a4,uf(-1,1));
    e.puti(0xd90,int(n%3)-1);e.putf(0xdc0,uf(0,3));e.puti(0x20c,int(gen()%18000));
    e.putf(0x21c,uf(0,1250));e.puti(0x3c,int(n%256));e.putf(0x22c,float(n%256)/255.0f);
    e.put32(0x1f4,n%6==0?19:n%6==1?20:n%6==2?21:n%6==3?0:100);
    e.put32(0x304,n%8==0?120:0);e.put32(0x308,120);
    p.putf(0xb48,uf(0.2f,0.5f));p.putf(0xb94,uf(0.2f,0.5f));
    p.putf(0xc78,uf(0.2f,6));p.putf(0xcc4,uf(0.2f,6));p.putf(0x16dc,uf(0.05f,1.5f));
    p.putf(0xf70,uf(2000,100000));p.putf(0xfbc,uf(2000,100000));
    p.putf(0xda8,uf(10000,1000000));p.putf(0xdf4,uf(10000,1000000));
    p.putf(0xd10,uf(0,0.15f));p.putf(0xd5c,uf(0,0.15f));
    p.putf(0,uf(800,2000));p.putf(0x17c0,uf(3,15));p.putf(0x180c,uf(0.1f,3));p.putf(0x1858,uf(0.1f,3));
    const std::uint16_t angle_edges[]={0,1,0xffff,0x3fff,0x4000,0x4001,0x7fff,0x8000,0x8001,0xbfff,0xc000,0xc001};
    for(unsigned i=0;i<4;++i){auto q=x.wheels()[i];
        q.putf(0xd0,uf(-0.001f,0.001f));q.putf(0xc8,uf(0.0f,0.004f));
        q.putf(0xd4,uf(-5,120));q.putf(0xd8,uf(-400,600));q.putf(0xdc,uf(-400,600));
        q.putf(0xc4,uf(0,4000));q.putf(0xac,uf(-30000,30000));q.putf(0xb0,uf(-30000,30000));
        q.put16(0xee,n%4==0?angle_edges[(n/4+i)%12]:std::uint16_t(gen()));
        q.put16(0xec,std::uint16_t(gen()));q.put16(0x32,std::uint16_t(gen()));
        switch(n%32){
        case 0:q.putf(0xc0,0.0f);q.putf(0xd4,0.0f);q.putf(0xd8,0.0f);q.putf(0xdc,0.0f);break;
        case 1:q.putf(0xac,0.0f);q.putf(0xb0,0.0f);q.putf(0xc0,0.0f);break;
        case 2:q.putf(0xac,0.0f);q.putf(0xb0,0.0f);q.putf(0xc0,1000.0f);break;
        case 3:q.putf(0xc0,1000);q.putf(0xac,0);q.putf(0xb0,250);break;
        case 4:q.putf(0xc0,1000);q.putf(0xac,0);q.putf(0xb0,4000);break;
        case 5:q.putf(0xc0,1000);q.putf(0xac,0);q.putf(0xb0,std::nextafter(250.0f,1000.0f));break;
        case 6:q.putf(0xc0,1000);q.putf(0xac,0);q.putf(0xb0,std::nextafter(4000.0f,1000.0f));break;
        case 7:q.putf(0xc4,0);break;
        case 8:q.put16(0x32,0x4000);q.put16(0xec,0);break;
        case 9:q.put16(0x32,0xc000);q.put16(0xec,0);break;
        case 10:q.putf(0xd4,0);break;
        case 11:q.putf(0xd4,-0.0f);break;
        default:break;
        }
    }
    if(n%7==0)e.putf(0x228,0);
    if(n%16==0)e.puti(0x38,250);
    if(n%16==1)e.puti(0x38,251);
    if(n%32==12)e.put32(0x20c,0x7fffffff);
    if(n%32==13)e.put32(0x20c,0x80000000);
    return x;
}
Fixture steering_fixture(const Fixture& base,unsigned n){
    Fixture x=base;std::mt19937 gen(0x4f523036u ^ (n*22695477u));
    auto uf=[&](float lo,float hi){return lo+(hi-lo)*(float(gen()%65536)/65535.0f);};
    auto e=x.ev(),w=x.wk(),p=x.pa();
    p.putf(0x130,uf(0.8f,1.4f));
    p.putf(0x193c,uf(3.5f,5.0f));
    p.putf(0x17c,uf(0.5f,1.5f));
    p.putf(0x18a4,uf(0.6f,1.4f));
    p.putf(0x18f0,uf(0.6f,1.4f));
    e.put16(0x202,std::uint16_t(gen()));
    e.put32(0x1f4,n%5==0?0:n%5==1?20:n%5==2?21:std::uint32_t(22+gen()%800));
    e.put16(0x16a,std::uint16_t(gen()%0x1000));
    e.put8(0xd36,std::uint8_t(n%3==0?1:0));
    w.put16(0x52c,std::uint16_t(gen()));
    w.put16(0x620,std::uint16_t(gen()));
    for(std::size_t axle=0;axle<2;++axle){
        p.putf((axle+0x1a)*0x4c,uf(-0.08f,0.08f));
        p.putf((axle+0x1c)*0x4c,uf(-0.08f,0.08f));
        p.putf((axle+0x1e)*0x4c,uf(-0.002f,0.002f));
        p.putf((axle+0x20)*0x4c,uf(-0.08f,0.08f));
        p.putf((axle+0x22)*0x4c,uf(-0.002f,0.002f));
        p.putf((axle+0x24)*0x4c,uf(-0.08f,0.08f));
    }
    for(unsigned i=0;i<4;++i){auto q=x.wheels()[i];q.putf(0xb0,uf(-12000,12000));q.putf(0xcc,uf(-0.2f,0.2f));q.put16(0xec,std::uint16_t(gen()));}
    // Explicit steering-angle and road-average boundaries.
    static constexpr std::uint16_t edges[]={0,1,0xffff,0x1fff,0x2000,0x2001,0xdfff,0xe000,0xe001,0x7000,0x9000,0x8000};
    if(n<sizeof(edges)/sizeof(edges[0]))e.put16(0x202,edges[n]);
    if(n%32==12){w.put16(0x52c,0x2000);w.put16(0x620,0x2000);}
    if(n%32==13){w.put16(0x52c,0xe000);w.put16(0x620,0xe000);}
    return x;
}
Fixture resistance_fixture(const Fixture& base,unsigned n){
    Fixture x=base;std::mt19937 gen(0x4f523037u ^ (n*1103515245u));
    auto uf=[&](float lo,float hi){return lo+(hi-lo)*(float(gen()%65536)/65535.0f);};
    auto e=x.ev();
    e.put32(0x1f4,n%12==0?0u:n%12==1?1u:n%12==2?20u:n%12==3?21u:std::uint32_t(gen()%2500));
    e.put8(0x282,std::uint8_t(n%3==0?1:0));e.put8(0x283,std::uint8_t(n%5==0?1:0));
    e.put32(0xdf8,n%4==0?0u:1u);e.putf(0xdb4,uf(-0.5f,1.5f));
    static constexpr std::uint32_t flags[]={0,0x4,0x8,0x10,0x801c,0x8000,0x20};
    e.put32(0x248,flags[n%(sizeof(flags)/sizeof(flags[0]))]);
    for(unsigned i=0;i<4;++i){auto q=x.wheels()[i];
        q.put32(0x14,flags[(n+i+2)%(sizeof(flags)/sizeof(flags[0]))]);
        q.put16(0xee,std::uint16_t(gen()));
        q.putf(0x58,uf(-2,2));q.putf(0x5c,uf(-2,2));q.putf(0x60,uf(-2,2));
        q.putf(0x94,uf(-100,100));q.putf(0x98,uf(-100,100));q.putf(0x9c,uf(-100,100));
    }
    return x;
}
Fixture physical_fixture(const Fixture& base,unsigned n){
    Fixture x=base;std::mt19937 gen(0x4f523038u ^ (n*214013u));
    auto uf=[&](float lo,float hi){return lo+(hi-lo)*(float(gen()%65536)/65535.0f);};
    auto e=x.ev(),w=x.wk();e.put8(0xd36,std::uint8_t(int(n%35)-10));
    w.putf(0x42c,uf(-500,500));w.putf(0x338,uf(-500,500));w.putf(0x614,uf(-500,500));w.putf(0x520,uf(-500,500));
    for(unsigned i=0;i<4;++i){auto q=x.wheels()[i];
        for(auto off:{0x4c,0x58,0x64}){q.putf(off,uf(-2,2));q.putf(off+4,uf(-2,2));q.putf(off+8,uf(-2,2));}
        q.putf(0xac,uf(-20000,20000));q.putf(0xb4,uf(-20000,20000));q.putf(0xb8,uf(-20000,20000));q.putf(0xbc,uf(-20000,20000));
        if(n%37==0){q.putf(0x58,0);q.putf(0x5c,0);q.putf(0x60,0);}
        if(n%41==0)q.putf(0xac,0);
    }
    return x;
}

void put_basis(Bytes w,std::size_t off,unsigned variant){
    static constexpr float m[6][16]={
      {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1},
      {0,0,-1,0, 0,1,0,0, 1,0,0,0, 0,0,0,1},
      {-1,0,0,0, 0,1,0,0, 0,0,-1,0, 0,0,0,1},
      {0,0,1,0, 0,1,0,0, -1,0,0,0, 0,0,0,1},
      {1,0,0,0, 0,0,1,0, 0,-1,0,0, 0,0,0,1},
      {0,1,0,0, -1,0,0,0, 0,0,1,0, 0,0,0,1}
    };
    for(unsigned i=0;i<16;++i)w.putf(off+i*4,m[variant%6][i]);
}
Fixture velocity_fixture(const Fixture& base,unsigned n){
    Fixture x=base;std::mt19937 gen(0x4f523039u ^ (n*747796405u));
    auto uf=[&](float lo,float hi){return lo+(hi-lo)*(float(gen()%65536)/65535.0f);};
    auto w=x.wk();put_basis(w,0x10,n%6);put_basis(w,0x1e0,(n/3+1)%6);
    w.putf(0x50,uf(-0.04f,0.04f));w.putf(0x54,uf(-0.04f,0.04f));w.putf(0x58,uf(-0.04f,0.04f));
    w.putf(0x5c,uf(-40,40));w.putf(0x60,uf(-3,3));w.putf(0x64,uf(-40,40));
    const unsigned axis=n%3;w.putf(0x628,axis==0?1.0f:0.0f);w.putf(0x62c,axis==1?1.0f:0.0f);w.putf(0x630,axis==2?1.0f:0.0f);
    for(unsigned i=0;i<4;++i){auto q=x.wheels()[i];
        q.putf(0x4,uf(-2.2f,2.2f));q.putf(0x0c,uf(-4.5f,4.5f));q.putf(0x2c,uf(-1.0f,1.0f));
        // A finite fallback tangent to the selected normal.
        q.putf(0x40,axis==0?0.0f:1.0f);q.putf(0x44,axis==1?0.0f:(axis==0?1.0f:0.0f));q.putf(0x48,axis==2?0.0f:(axis==0?0.0f:1.0f));
        q.putf(0x58,uf(-2,2));q.putf(0x5c,uf(-2,2));q.putf(0x60,uf(-2,2));
        q.putf(0x64,uf(-2,2));q.putf(0x68,uf(-2,2));q.putf(0x6c,uf(-2,2));
        q.putf(0x70,uf(-2,2));q.putf(0x74,uf(-2,2));q.putf(0x78,uf(-2,2));q.putf(0xd4,uf(-2,2));q.put16(0xec,std::uint16_t(gen()));
    }
    // Exact stationary/near-threshold cases exercise the fallback branch.
    if(n%64==0){w.putf(0x50,0);w.putf(0x54,0);w.putf(0x58,0);w.putf(0x5c,0);w.putf(0x60,0);w.putf(0x64,0);}
    if(n%64==1){w.putf(0x50,0);w.putf(0x54,0);w.putf(0x58,0);w.putf(0x5c,0.000099f);w.putf(0x60,0);w.putf(0x64,0);}
    if(n%64==2){w.putf(0x50,0);w.putf(0x54,0);w.putf(0x58,0);w.putf(0x5c,0.000101f);w.putf(0x60,0);w.putf(0x64,0);}
    return x;
}
Fixture automatic_fixture(const Fixture& base,unsigned n){
    Fixture x=base;std::mt19937 gen(0x4f52303bu ^ (n*1597334677u));auto e=x.ev(),p=x.pa();
    const std::uint32_t maximum=1u+(gen()%6u);p.put32(0x10a0,maximum);
    const std::uint32_t gear=1u+(gen()%maximum);e.put32(0x208,gear);
    const std::int32_t pedal=(n%8==0?127:n%8==1?128:n%8==2?129:int(gen()%256));e.puti(0x38,pedal);
    e.put8(0x282,std::uint8_t(n%5==0?1:0));
    for(unsigned g=0;g<8;++g){
        const float up=100.0f+float(g)*90.0f;const float low=70.0f+float(g)*75.0f;const float high=55.0f+float(g)*65.0f;
        e.putf(0xe10+g*4,up);e.putf(0xe2c+g*4,low);e.putf(0xe48+g*4,high);
    }
    float speed=90.0f+float(gear)*80.0f;
    switch(n%7){case 0:speed=e.f32(0xe10+gear*4);break;case 1:speed=std::nextafter(e.f32(0xe10+gear*4),10000.0f);break;
      case 2:speed=e.f32((pedal<128?0xe2c:0xe48)+gear*4);break;case 3:speed=std::nextafter(e.f32((pedal<128?0xe2c:0xe48)+gear*4),-10000.0f);break;default:break;}
    e.putf(0x1c4,speed);e.put8(0x296,std::uint8_t(gen()));return x;
}
Fixture manual_fixture(const Fixture& base,unsigned n){
    Fixture x=automatic_fixture(base,n);std::mt19937 gen(0x4f52303cu ^ (n*2246822519u));auto e=x.ev(),w=x.wk(),p=x.pa();
    const std::uint32_t maximum=2u+(gen()%5u);p.put32(0x10a0,maximum);e.put32(0x208,1u+(gen()%maximum));
    e.put32(0xe84,n%9==0?1u:0u);e.puti(0x34,n%11==0?16:n%11==1?17:int(gen()%256));
    e.puti(0x38,n%13==0?127:n%13==1?128:int(gen()%256));e.put8(0xd36,std::uint8_t(n%7==0?11:n%7==1?12:gen()%100));
    e.put32(0xd94,gen());e.put32(0xd98,gen());e.put32(0xd9c,gen());
    // Keep rear wheel state and ratios finite for PredictedEngineSpeed.
    for(unsigned i=2;i<4;++i){auto q=x.wheels()[i];q.putf(0x34,500.0f+float(gen()%9000));q.putf(0x38,1200.0f+float(gen()%4000));}
    for(unsigned gear=0;gear<=7;++gear)p.putf((gear+0x3a)*0x4c,0.5f+float((gen()%2000))/1000.0f);
    p.putf(0xe40,1.4f);p.putf(0xed8,0.8f);p.putf(0xe8c,1.3f);p.putf(0xf24,0.75f);
    // Explicit predicted-RPM threshold boundaries occur naturally around this range.
    p.putf(0x1644,n%17==0?0.0f:700.0f+float(gen()%400));
    (void)w;return x;
}
Fixture road_mu_fixture(const Fixture& base,unsigned n){
    Fixture x=base;std::mt19937 gen(0x4f52303du ^ (n*3266489917u));auto e=x.ev();
    e.put32(0xd38,gen());
    static constexpr std::uint32_t surfaces[]={
        1u,2u,3u,4u,8u,16u,0x100u,0x400u,0x8000u,0x100000u,
        0x200000u,0x400000u,0x800000u,0u,0x20u,0xffffffffu
    };
    for(unsigned i=0;i<4;++i){auto q=x.wheels()[i];q.put32(0x14,surfaces[(n+i*3u)%std::size(surfaces)]);q.putf(0xe8,float(gen()%4000)/1000.0f);}
    static constexpr std::int16_t rear2[]={-32768,-1,0,1,32767};
    static constexpr std::int16_t rear3[]={32767,1,0,-1,-32768};
    x.wheels()[2].put16(0xee,std::uint16_t(rear2[n%5]));
    x.wheels()[3].put16(0xee,std::uint16_t(rear3[(n/5)%5]));
    return x;
}
Fixture direction_fixture(const Fixture& base,unsigned n){
    Fixture x=steering_fixture(base,n);std::mt19937 gen(0x4f52303au ^ (n*2891336453u));
    auto w=x.wk();put_basis(w,0x10,(n*5u+1u)%6u);
    static constexpr float normals[10][3]={
      {1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1},
      {0.7071067690849304f,0.7071067690849304f,0},
      {0.7071067690849304f,0,0.7071067690849304f},
      {0,0.7071067690849304f,0.7071067690849304f},
      {0.5773502588272095f,0.5773502588272095f,0.5773502588272095f}
    };
    for(unsigned i=0;i<4;++i){auto q=x.wheels()[i];
        const auto& nn=normals[(n+i)%10];q.putf(0x70,nn[0]);q.putf(0x74,nn[1]);q.putf(0x78,nn[2]);
        q.putf(0x40,float(int(gen()%2001)-1000)/1000.0f);q.putf(0x44,float(int(gen()%2001)-1000)/1000.0f);q.putf(0x48,float(int(gen()%2001)-1000)/1000.0f);
        q.putf(0x4c,float(int(gen()%2001)-1000)/1000.0f);q.putf(0x50,float(int(gen()%2001)-1000)/1000.0f);q.putf(0x54,float(int(gen()%2001)-1000)/1000.0f);
    }
    return x;
}
Fixture driving_control_fixture(const Fixture& base,unsigned n){
    Fixture x=wheel_fixture(base,n);
    x=velocity_fixture(x,n);
    x=direction_fixture(x,n);
    x=manual_fixture(x,n);
    x=resistance_fixture(x,n);
    x=physical_fixture(x,n);
    x=road_mu_fixture(x,n);
    x.ev().put8(0x13,std::uint8_t(n&1u));
    return x;
}
Fixture suspension_fixture(const Fixture& base,unsigned n){
    Fixture x=direction_fixture(base,n);std::mt19937 gen(0x4f52303eu ^ (n*668265263u));
    auto e=x.ev(),p=x.pa();auto q=x.wheels();
    e.put8(0x283,std::uint8_t(n%13u==0u));
    p.putf(0,900.0f+float(gen()%1500u));
    for(unsigned axle=0;axle<2;++axle){
        const float center=0.12f+float(gen()%180u)/1000.0f;
        p.putf((axle+0x0e)*0x4c,center);
        p.putf((axle+0x10)*0x4c,center-(0.03f+float(gen()%80u)/1000.0f));
        p.putf((axle+0x12)*0x4c,center+(0.03f+float(gen()%80u)/1000.0f));
        p.putf((axle+0x16)*0x4c,3000.0f+float(gen()%24000u));
        p.putf((axle+0x18)*0x4c,500.0f+float(gen()%9000u));
    }
    for(unsigned i=0;i<4;++i){
        q[i].put8(0,std::uint8_t((n+i)%19u==0u ? 1u : 0u));
        q[i].putf(0x08,float(gen()%5000u)/10000.0f);
        q[i].putf(0x28,float(gen()%3000u)/10000.0f);
        q[i].putf(0x18,float(gen()%4000u)/10000.0f);
        q[i].putf(0x1c,float(gen()%4000u)/10000.0f);
        q[i].putf(0x20,float(int(gen()%12001u)-6000)/100.0f);
        q[i].putf(0x24,float(int(gen()%400001u)-100000)/100.0f);
        q[i].putf(0x34,500.0f+float(gen()%7000u));
        q[i].putf(0x38,1000.0f+float(gen()%5000u));
    }
    // Boundary-focused travel states.
    if((n%17u)==0u){q[0].putf(0x08,p.f32(0x0e*0x4c));q[0].putf(0x28,0.0f);}
    if((n%17u)==1u){q[1].putf(0x20,0.0f);}
    if((n%17u)==2u){q[2].putf(0x24,-0.0f);}
    if((n%17u)==3u){q[3].putf(0x24,0.0f);}
    return x;
}
Fixture contact_fixture(const Fixture& base,unsigned n){
    Fixture x=base;std::mt19937 gen(0x4f52303fu ^ (n*374761393u));
    auto uf=[&](float lo,float hi){return lo+(hi-lo)*(float(gen()%65536)/65535.0f);};
    auto w=x.wk();put_basis(w,0x10,(n*7u+1u)%6u);
    // Translation is part of the body matrix and participates in the projection.
    w.putf(0x10+12*4,uf(-200.0f,200.0f));w.putf(0x10+13*4,uf(-20.0f,20.0f));w.putf(0x10+14*4,uf(-200.0f,200.0f));
    static constexpr float normals[10][3]={
      {1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1},
      {0.7071067690849304f,0.7071067690849304f,0},
      {0.7071067690849304f,0,0.7071067690849304f},
      {0,0.7071067690849304f,0.7071067690849304f},
      {0.5773502588272095f,0.5773502588272095f,0.5773502588272095f}};
    const auto& nn=normals[n%10u];w.putf(0x628,nn[0]);w.putf(0x62c,nn[1]);w.putf(0x630,nn[2]);
    w.putf(0x640,w.f32(0x10+12*4)+uf(-12.0f,12.0f));
    w.putf(0x644,w.f32(0x10+13*4)+uf(-4.0f,4.0f));
    w.putf(0x648,w.f32(0x10+14*4)+uf(-12.0f,12.0f));
    // Canary outputs: a full-memory compare catches every write and over-write.
    for(unsigned i=0;i<16;++i)w.put32(0x1e0+i*4,gen());
    w.put32(0x210,gen());w.put32(0x214,gen());w.put32(0x218,gen());
    return x;
}
Fixture car_sus_coli_fixture(const Fixture& base,unsigned n){
    Fixture x=contact_fixture(suspension_fixture(base,n+900000u),n+900000u);
    { auto pp=x.pa(); std::mt19937 gg(0x53434f4cu ^ n); for(unsigned axle=0;axle<2;++axle) pp.putf((axle+0x26u)*0x4cu,0.10f+float(gg()%500u)/1000.0f); }
    auto w=x.wk(),p=x.pa();auto q=x.wheels();
    // First 64 cases exhaust all 16 contact masks under both previous bit-2 states,
    // with exact/adjacent height boundaries and deliberately different +0x08/+0x28.
    if(n<64u){
        put_basis(w,0x10,0);w.putf(0x40,0.0f);w.putf(0x44,0.0f);w.putf(0x48,0.0f);
        w.putf(0x628,0.0f);w.putf(0x62c,1.0f);w.putf(0x630,0.0f);
        w.put32(0x244,(w.u32(0x244)&~0x86u)|(((n/16u)&1u)?0x4u:0u));
        const unsigned mask=n&15u;
        for(unsigned axle=0;axle<2;++axle)p.putf((axle+0x26u)*0x4cu,0.25f+0.05f*float(axle));
        for(unsigned i=0;i<4;++i){
            const unsigned axle=i>>1;const float a=p.f32((axle+0x26u)*0x4cu);
            q[i].put32(0,(q[i].u32(0)&~1u)|(((mask>>i)&1u)?0u:1u));
            q[i].putf(0x04,float(i)-1.5f);q[i].putf(0x0c,float(i)*0.5f);
            q[i].putf(0x08,50.0f+float(i)); // poison the historically-confused offset
            q[i].putf(0x28,0.5f+a);
            const float world_y=0.5f;
            const bool want_contact=((mask>>i)&1u)!=0;
            if((n/32u)&1u) q[i].putf(0x3c,want_contact?std::nextafter(world_y,100.0f):std::nextafter(world_y,-100.0f));
            else q[i].putf(0x3c,want_contact?world_y:std::nextafter(world_y,-100.0f));
        }
    }
    // Exact orientation boundaries: zero is accepted, immediately negative is rejected.
    if(n==64u||n==65u){
        put_basis(w,0x10,0);w.putf(0x628,1.0f);w.putf(0x62c,n==64u?0.0f:std::nextafter(0.0f,-1.0f));w.putf(0x630,0.0f);
        for(unsigned i=0;i<4;++i)q[i].putf(0x3c,1000.0f);
    }
    return x;
}
Fixture collision_fixture(const Fixture& base,unsigned n){
    Fixture x=contact_fixture(suspension_fixture(base,n),n);
    std::mt19937 gen(0x4f523041u ^ (n*3266489917u));
    auto w=x.wk(),p=x.pa();auto q=x.wheels();
    // Collision wheel geometry consumed by CarSusBumpPush.
    for(unsigned axle=0;axle<2;++axle){
        p.putf((axle+0x26u)*0x4cu,0.10f+float(gen()%500u)/1000.0f);
        // +0x10 term is already populated by suspension_fixture.
    }
    for(unsigned i=0;i<4;++i){
        q[i].putf(0x04,float(int(gen()%40001u)-20000)/100.0f);
        q[i].putf(0x08,float(int(gen()%10001u)-3000)/100.0f);
        q[i].putf(0x0c,float(int(gen()%40001u)-20000)/100.0f);
    }
    w.putf(0x40,w.f32(0x10+12*4));
    w.putf(0x44,w.f32(0x10+13*4));
    w.putf(0x48,w.f32(0x10+14*4));
    return x;
}
Fixture maximum_velocity_fixture(const Fixture& base,unsigned n){
    Fixture x=contact_fixture(base,n);std::mt19937 gen(0x4f523040u ^ (n*2246822519u));
    auto w=x.wk();
    auto put_vec=[&](std::size_t off,float x0,float y0,float z0){w.putf(off,x0);w.putf(off+4,y0);w.putf(off+8,z0);};
    // Exercise below/at/above both velocity limits and arbitrary directions.
    static constexpr float linear_len[]={0.0f,1.0f,166.0f,166.6666717529296875f,166.7f,250.0f,1000.0f};
    static constexpr float angular_len[]={0.0f,1.0f,60.0f,60.200000762939453125f,60.25f,120.0f,500.0f};
    auto make_vec=[&](float len,unsigned salt){
        float a=float(int((gen()+salt)%2001u)-1000)/1000.0f;
        float b=float(int((gen()+salt*3u)%2001u)-1000)/1000.0f;
        float c=float(int((gen()+salt*7u)%2001u)-1000)/1000.0f;
        float l=std::sqrt(a*a+b*b+c*c);if(l==0.0f){a=1.0f;l=1.0f;}
        return std::array<float,3>{a/l*len,b/l*len,c/l*len};
    };
    auto lv=make_vec(linear_len[n%(sizeof(linear_len)/sizeof(*linear_len))],1u);
    auto av=make_vec(angular_len[(n/7u)%(sizeof(angular_len)/sizeof(*angular_len))],2u);
    put_vec(0x5c,lv[0],lv[1],lv[2]);put_vec(0x50,av[0],av[1],av[2]);
    // Height/vertical velocity cover both ceiling branches.
    const float top=static_cast<float>(w.f32(0x644)+10.0f);
    switch(n%6u){
      case 0:w.putf(0x44,top-1.0f);w.putf(0x60,5.0f);break;
      case 1:w.putf(0x44,top);w.putf(0x60,5.0f);break;
      case 2:w.putf(0x44,top+0.001f);w.putf(0x60,5.0f);break;
      case 3:w.putf(0x44,top+50.0f);w.putf(0x60,-5.0f);break;
      case 4:w.putf(0x44,top+50.0f);w.putf(0x60,0.0f);break;
      default:w.putf(0x44,top-50.0f);w.putf(0x60,-5.0f);break;
    }
    return x;
}

struct CopyCarFixture {
    Fixture x;
    // The common Params view grew beyond the old 0x2440 local buffer. Both
    // mirrors must retain that same extent; otherwise the two full-view
    // copies below overwrite/read past this fixture before comparison.
    Params body{};
};
CopyCarFixture copy_car_fixture(const Fixture& base,unsigned n){
    CopyCarFixture q;q.x=base;
    std::mt19937 gen(0x4f523037u ^ (n*2654435761u));
    auto e=q.x.ev(),w=q.x.wk();
    for(std::size_t k=0;k<q.body.size();++k)q.body[k]=std::uint8_t(gen());
    std::memcpy(q.body.data(),q.x.p.data(),q.x.p.size());
    Bytes bp(q.body.data(),q.body.size());
    auto rf=[&](float lo,float hi){return lo+(hi-lo)*(float(gen()&0xffffu)/65535.0f);};
    e.put32(0x2b4,P);
    e.put32(4,(e.u32(4)&~8u)|((n%5u)==0u?8u:0u));
    e.put16(0x2c,std::uint16_t(std::int16_t(int(gen()%50001u)-25000)));
    e.put16(0x2e,std::uint16_t(std::int16_t(int(gen()%50001u)-25000)));
    e.put16(0x30,std::uint16_t(std::int16_t(int(gen()%50001u)-25000)));
    e.putf(0x1c4,rf(0.0f,25.0f));e.putf(0x2ac,rf(-2.0f,2.0f));e.putf(0x2b0,rf(-2.0f,2.0f));
    // Valid affine chassis matrix. Non-special cases exercise angle extraction;
    // special cases rebuild this orientation from event angles.
    for(unsigned k=0;k<16;++k)w.putf(0x10+k*4u,0.0f);
    const float a=rf(-1.0f,1.0f),c=std::cos(a),ss=std::sin(a);
    w.putf(0x10,c);w.putf(0x18,ss);w.putf(0x24,1.0f);w.putf(0x30,-ss);w.putf(0x38,c);w.putf(0x4c,1.0f);
    w.putf(0x40,rf(-100.0f,100.0f));w.putf(0x44,rf(-5.0f,20.0f));w.putf(0x48,rf(-100.0f,100.0f));
    w.putf(0x220,rf(-2.5f,2.5f));w.putf(0x224,rf(-1.5f,1.5f));w.putf(0x228,rf(-4.0f,4.0f));
    w.putf(0x5c,rf(-90.0f,90.0f));w.putf(0x60,rf(-12.0f,12.0f));w.putf(0x64,rf(-90.0f,90.0f));
    constexpr CourseProbe normals[]={{0,1,0},{0.25f,0.93541437f,0.25f},{-0.5f,0.70710677f,0.5f},{0.0f,0.0f,1.0f}};
    const auto no=normals[n%4u];w.putf(0x628,no.x);w.putf(0x62c,no.y);w.putf(0x630,no.z);
    bp.putf(0x4c0,rf(-1.0f,1.0f));bp.putf(0x50c,rf(-1.0f,1.0f));
    bp.putf(0x20a8,(n%3u)==0u?0.0f:rf(0.05f,1.0f));bp.putf(0x2438,rf(0.2f,2.0f));
    for(unsigned i=0;i<4;++i){
        const auto off=embedded_wheel_offsets[i];
        w.put32(0x248+i*4u,W+std::uint32_t(off));
        w.putf(off+0x08,rf(-2.0f,3.0f));w.putf(off+0x28,rf(-0.5f,2.5f));w.put32(off+0x14,(gen()&0xfu)<<i);
    }
    for(const std::size_t o:{0x268u,0x35cu,0x450u,0x544u,0x26cu,0x360u,0x454u,0x548u})w.put32(o,gen());
    // Boundary cases for low-speed fallback / scalar correction / special path.
    if(n%16u==1u){w.putf(0x5c,0.0f);w.putf(0x60,0.0f);w.putf(0x64,0.0f);}
    if(n%16u==2u){bp.putf(0x20a8,-0.0f);bp.putf(0x2438,3.0f);}
    if(n%16u==3u){bp.putf(0x20a8,0.0f);bp.putf(0x2438,-0.25f);}
    // Keep the native parameter mirror identical for offsets inside the legacy view.
    std::memcpy(q.x.p.data(),q.body.data(),q.x.p.size());
    return q;
}
Fixture set_car_camera_fixture(const Fixture& base,unsigned n){
    Fixture x=base;std::mt19937 gen(0x4f523038u ^ (n*2246822519u));
    auto e=x.ev(),w=x.wk();
    auto rf=[&](float lo,float hi){return lo+(hi-lo)*(float(gen()&0xffffu)/65535.0f);};
    static constexpr CourseProbe normals[]={
      {0,1,0},{0.25f,0.93541437f,0.25f},{-0.5f,0.70710677f,0.5f},
      {0,0,1},{0.5773502588f,0.5773502588f,0.5773502588f}};
    const auto normal=normals[n%std::size(normals)];
    w.putf(0x628,normal.x);w.putf(0x62c,normal.y);w.putf(0x630,normal.z);
    // Keep the original camera vector finite and away from exact parallelism for
    // the broad random corpus; explicit boundary cases below cover degenerate axes.
    CourseProbe h{rf(-1.5f,1.5f),rf(-1.5f,1.5f),rf(-1.5f,1.5f)};
    if(std::fabs(h.x*normal.x+h.y*normal.y+h.z*normal.z)>1.35f)h={1.0f,0.25f,-0.75f};
    e.putf(0x90,h.x);e.putf(0x94,h.y);e.putf(0x98,h.z);e.putf(0x9c,rf(-2,2));
    e.putf(0x20,rf(-0.03f,0.03f));e.putf(0x24,rf(-0.03f,0.03f));e.putf(0x28,rf(-0.03f,0.03f));
    e.put16(0x160,std::uint16_t(std::int16_t(int(gen()%65536u)-32768)));
    e.put16(0x2e,std::uint16_t(std::int16_t(int(gen()%65536u)-32768)));
    e.put16(0x162,std::uint16_t(gen()));e.put16(0x1fc,std::uint16_t(gen()));
    e.put16(0x1fe,std::uint16_t(gen()));e.put16(0x200,std::uint16_t(gen()));
    // Target the 0.015 velocity blend transition and exact signed-zero paths.
    if(n%32u==0u){e.putf(0x20,0.0f);e.putf(0x24,0.0f);e.putf(0x28,0.0f);}
    if(n%32u==1u){e.putf(0x20,0.014999f);e.putf(0x24,0.0f);e.putf(0x28,0.0f);}
    if(n%32u==2u){e.putf(0x20,0.015001f);e.putf(0x24,0.0f);e.putf(0x28,0.0f);}
    if(n%64u==3u){e.putf(0x90,-0.0f);e.putf(0x94,0.0f);e.putf(0x98,1.0f);}
    return x;
}
PcDrivingSkillInputs driving_skill_inputs(unsigned n){
    return {std::int32_t((n/3u)%8u),std::uint8_t((n/5u)%5u)};
}
Fixture driving_skill_fixture(const Fixture& base,unsigned n){
    Fixture x=base;std::mt19937 gen(0x4f523039u ^ (n*3266489917u));auto e=x.ev();
    e.puti(0xdec,std::int32_t(n%7u)-2);            // timer, including <=0 and 1 transition
    e.puti(0xdf0,std::int32_t((n*13u)%180u));      // accumulated success count
    e.puti(0xdf4,std::int32_t((n/7u)%8u));         // current skill bucket
    e.puti(0xdf8,std::int32_t(gen()));             // output canary
    e.putf(0xdb4,float((n%13u))*0.1f);              // progress gate around 0.5
    e.putf(0xe04,float((n*3u)%9u));                 // secondary skill accumulator
    if(n%32u==0u)e.putf(0xdb4,0.5f);
    if(n%32u==1u)e.putf(0xdb4,std::nextafter(0.5f,0.0f));
    if(n%32u==2u)e.putf(0xdb4,std::nextafter(0.5f,1.0f));
    if(n%64u==3u)e.putf(0xdb4,std::numeric_limits<float>::quiet_NaN());
    return x;
}
PcChickenDriverInputs chicken_driver_inputs(unsigned n){
    PcChickenDriverInputs in{};
    in.query_status={1,1,1};
    switch(n%11u){case 1:in.query_status[0]=0;break;case 2:in.query_status[1]=0;break;case 3:in.query_status[2]=-1;break;default:break;}
    std::uint32_t a=0x13579bdfu+n*2654435761u,b=0x2468ace0u+n*2246822519u,c=0x10203040u+n*3266489917u;
    in.offset_direction={static_cast<std::int32_t>(a),static_cast<std::int32_t>(b),static_cast<std::int32_t>(c)};
    if(n%64u==4u)in.offset_direction={0,0x000007ff,static_cast<std::int32_t>(0xfffff801u)};
    if(n%64u==5u)in.offset_direction={0,0x00000800,static_cast<std::int32_t>(0xfffff800u)};
    if(n%64u==6u)in.offset_direction={0,0x00001e00,static_cast<std::int32_t>(0xffffe200u)};
    if(n%64u==7u)in.offset_direction={0,0x00008000,0x00000000};
    in.volume1=std::int32_t((n*37u)%256u);
    return in;
}
Fixture chicken_driver_fixture(const Fixture& base,unsigned n){
    Fixture x=base;std::mt19937 gen(0x4f52303au ^ (n*668265263u));auto e=x.ev();
    e.put32(0x5c,(n%4u)==0u?0u:1u+(n%3u));
    e.put16(0x64,std::uint16_t((n*257u)&0x7fffu));
    e.putf(0xe0c,float(int(n%151u)-25)*0.01f);
    e.put8(0x283,std::uint8_t((n/7u)%3u==0u?0u:1u));
    e.putf(0x2c8,float(int(n%9u)-4)*0.125f);e.putf(0x2f8,float(int((n/3u)%9u)-4)*0.125f);
    e.put8(0xd36,std::uint8_t(std::int8_t(int(n%7u)-3)));
    e.put16(0x260,std::uint16_t((n*97u)%65536u));e.put16(0x262,std::uint16_t((n*53u+1000u)%65536u));
    e.puti(0xdfc,std::int32_t((n*7u)%800u));e.puti(0xe00,std::int32_t((n*11u)%1200u));e.putf(0xe04,float(gen()&0xffffu)/65535.0f);
    if(n%64u==8u)e.putf(0xe0c,0.8999999761581421f);
    if(n%64u==9u)e.putf(0xe0c,std::numeric_limits<float>::quiet_NaN());
    if(n%64u==10u)e.putf(0x2c8,std::numeric_limits<float>::quiet_NaN());
    if(n%64u==11u)e.putf(0x2f8,std::numeric_limits<float>::quiet_NaN());
    if(n%64u==12u){e.puti(0xe00,9);e.puti(0xdfc,4);}
    if(n%64u==13u){e.puti(0xe00,10);e.puti(0xdfc,5);}
    if(n%64u==14u){e.puti(0xe00,999);e.puti(0xdfc,750);}
    if(n%64u==15u){e.puti(0xe00,1000);e.puti(0xdfc,750);}
    return x;
}
PcAssistChickenInputs assist_chicken_inputs(unsigned n){
    PcAssistChickenInputs in{};
    in.entry_nodes=std::uint8_t(n%6u);
    in.volume1=std::int32_t((n*37u)%256u);
    in.volume2=std::int32_t((n*83u+17u)%256u);
    return in;
}
Fixture assist_chicken_fixture(const Fixture& base,unsigned n){
    Fixture x=base;std::mt19937 gen(0x4f52303cu ^ (n*2246822519u));auto e=x.ev();
    e.putf(0x1c4,0.00005f+float(n%200u)*0.0015f);
    e.put8(0x282,std::uint8_t((n/7u)%4u==0u?1u:0u));
    e.put8(0x283,std::uint8_t((n/11u)%4u==0u?1u:0u));
    e.putf(0x2c8,float(int(n%9u)-4)*0.125f);
    e.putf(0x2f8,float(int((n/3u)%9u)-4)*0.125f);
    e.put8(0x06,std::uint8_t((n/13u)%5u==0u?1u:0u));
    e.putf(0xe64,0.25f+float((n*7u)%180u)*0.01f);
    e.putf(0xdbc,0.5f+float((n*5u)%140u)*0.01f);
    e.putf(0xdb4,float((n*13u)%101u)*0.01f);
    e.putf(0xe04,float((n*17u)%101u)*0.01f);
    e.putf(0xe08,float(int((n*19u)%81u)-40)*0.002f);
    if(n%64u==0u)e.putf(0x1c4,9.999999747378752e-05f);
    if(n%64u==1u)e.putf(0x1c4,std::nextafter(9.999999747378752e-05f,0.0f));
    if(n%64u==2u)e.putf(0x1c4,std::nextafter(9.999999747378752e-05f,1.0f));
    if(n%64u==3u)e.putf(0x1c4,std::numeric_limits<float>::quiet_NaN());
    if(n%64u==4u)e.putf(0x2c8,std::numeric_limits<float>::quiet_NaN());
    if(n%64u==5u)e.putf(0x2f8,std::numeric_limits<float>::quiet_NaN());
    if(n%64u==6u)e.putf(0xdbc,std::numeric_limits<float>::quiet_NaN());
    if(n%64u==7u)e.putf(0xe08,std::numeric_limits<float>::quiet_NaN());
    (void)gen;
    return x;
}
PcOfsLeftLaneInputs ofs_left_lane_inputs(unsigned n){
    PcOfsLeftLaneInputs in{};
    in.selector=(n%17u==0u)?-1:std::int32_t(n%9u);
    std::mt19937 gen(0x4f523041u ^ (n*3266489917u));
    auto rf=[&](float lo,float hi){return lo+(hi-lo)*(float(gen()&0xffffu)/65535.0f);};
    for(unsigned i=0;i<4;++i){
        in.rates[i]=rf(-0.25f,1.5f);
        in.points[i]={rf(-250.0f,250.0f),rf(-30.0f,50.0f),rf(-250.0f,250.0f)};
    }
    // Repeated/equidistant points exercise strict best-distance replacement.
    if(n%64u==1u){in.points[0]={1,0,0};in.points[1]={-1,0,0};in.points[2]={0,1,0};in.points[3]={0,-1,0};}
    if(n%64u==2u)in.points[2].x=std::numeric_limits<float>::quiet_NaN();
    if(n%64u==3u)in.rates[3]=std::numeric_limits<float>::quiet_NaN();
    return in;
}
Fixture ofs_left_lane_fixture(const Fixture& base,unsigned n){
    Fixture x=base;auto e=x.ev();std::mt19937 gen(0x4f523042u ^ (n*2246822519u));
    auto rf=[&](float lo,float hi){return lo+(hi-lo)*(float(gen()&0xffffu)/65535.0f);};
    e.putf(0x14,rf(-150.0f,150.0f));e.putf(0x18,rf(-20.0f,40.0f));e.putf(0x1c,rf(-150.0f,150.0f));
    e.putf(0x58,rf(-0.5f,1.5f));e.put32(0x5c,gen());
    e.put32(0x1f4,(n%7u==0u)?200u:((n%7u==1u)?201u:std::uint32_t(n%401u)));
    if(n%64u==4u)e.putf(0x58,-0.0f);
    if(n%64u==5u)e.putf(0x58,std::numeric_limits<float>::quiet_NaN());
    return x;
}
std::array<std::uint8_t,VibrateHistoryBytes> vibrate_history_fixture(unsigned n){
    std::array<std::uint8_t,VibrateHistoryBytes> raw{};Bytes b(raw.data(),raw.size());
    for(unsigned slot=0;slot<VibrateHistorySlots;++slot)for(unsigned i=0;i<30;++i){
        const int q=int((n*17u+slot*53u+i*29u)%401u)-200;
        b.putf(slot*0x78u+i*4u,float(q)*0.03125f);
    }
    if(n%64u==10u)b.putf((n%VibrateHistorySlots)*0x78u+7u*4u,-0.0f);
    if(n%64u==11u)b.putf((n%VibrateHistorySlots)*0x78u+19u*4u,0.0f);
    return raw;
}
Fixture vibrate_fixture(const Fixture& base,unsigned n){
    Fixture x=base;auto e=x.ev();std::mt19937 gen(0x4f52303eu ^ (n*2246822519u));
    e.put32(0,8u+(n%VibrateHistorySlots));
    std::uint32_t flags=gen();if((n%9u)==0u)flags&=~1u;else flags|=1u;e.put32(4,flags);
    e.putf(0x1c8,float(int((n*31u)%513u)-256)*0.00390625f);
    e.put32(0x1cc,gen());
    if(n%64u==0u)e.putf(0x1c8,0.0f);
    if(n%64u==1u)e.putf(0x1c8,-0.0f);
    if(n%64u==2u)e.putf(0x1c8,1.0f);
    if(n%64u==3u)e.putf(0x1c8,-1.0f);
    return x;
}
PcReverseCarInputs reverse_car_inputs(unsigned n){
    PcReverseCarInputs in{};
    in.param20=-0.12f-float((n*7u)%80u)*0.001f;
    in.param24= 0.12f+float((n*11u)%100u)*0.001f;
    in.param28= 0.08f+float((n*13u)%90u)*0.001f;
    in.param2c= 0.20f+float((n*17u)%120u)*0.001f;
    in.random_value=0x12345678u+n*1664525u+1013904223u;
    in.game_mode=(n%7u)==0u?3:std::int32_t(n%6u);
    switch(n%64u){
      case 0:in.param20=-0.0f;break;
      case 1:in.param24=0.0f;break;
      case 2:in.param28=-0.0f;break;
      case 3:in.param2c=1.0f;break;
      case 4:in.random_value=0;break;
      case 5:in.random_value=0x7fffu;break;
      case 6:in.random_value=0xffffu;break;
      default:break;
    }
    return in;
}
Fixture reverse_car_fixture(const Fixture& base,unsigned n,const PcReverseCarInputs& in){
    Fixture x=base;auto e=x.ev(),p=x.pa();std::mt19937 gen(0x4f523041u^(n*2246822519u));
    p.putf(0x20,in.param20);p.putf(0x24,in.param24);p.putf(0x28,in.param28);p.putf(0x2c,in.param2c);
    e.put32(0x1f4,(n%9u==0u)?std::uint32_t(n%101u):101u+((n*977u)%120000u));
    e.putf(0x310,float((n*31u)%26000u)*0.01f);
    e.putf(0x30c,0.01f+float((n*19u)%200u)*0.0025f);
    std::uint32_t flags=gen();if(n&1u)flags|=1u;else flags&=~1u;e.put32(0x04,flags);
    e.putf(0x1cc,float(int((n*37u)%801u)-400)*0.01f);
    e.put16(0xd34,std::uint16_t((n*1229u)&0xffffu));
    for(unsigned k=0;k<16;++k)e.put32(0xf0+k*4u,gen());
    if(n%64u==7u)e.put32(0x1f4,100u);
    if(n%64u==8u)e.put32(0x1f4,101u);
    if(n%64u==9u)e.put32(0x1f4,0x7fffffffu);
    if(n%64u==10u)e.put32(0x1f4,0x80000000u);
    if(n%64u==11u)e.put32(0x1f4,0xffffffffu);
    if(n%64u==12u)e.putf(0x310,254.99998474121094f);
    if(n%64u==13u)e.putf(0x310,255.0f);
    if(n%64u==14u)e.putf(0x1cc,-1.5707963705062866f);
    if(n%64u==15u)e.putf(0x1cc,1.5707963705062866f);
    if(n%64u==16u)e.putf(0x1cc,std::nextafter(-1.5707963705062866f,-10.0f));
    if(n%64u==17u)e.putf(0x1cc,std::nextafter(1.5707963705062866f,10.0f));
    if(n%64u==18u)e.putf(0x1cc,std::numeric_limits<float>::quiet_NaN());
    if(n%64u==19u)e.put16(0xd34,0x8000u);
    if(n%64u==20u)e.put16(0xd34,0x7fffu);
    return x;
}
PcRecordGhostInputs record_ghost_inputs(unsigned n){
    PcRecordGhostInputs in{};
    in.steering_override_active=(n%5u==0u)?std::int32_t(1u+(n%7u)):0;
    static constexpr std::int16_t overrides[]={0,1,-1,5460,5461,5462,-5460,-5461,-5462,32767,static_cast<std::int16_t>(0x8000u)};
    in.steering_override=overrides[n%std::size(overrides)];
    in.game_mode=(n%7u==0u)?0x10:std::int32_t(n%19u);
    // Explicitly cover active override while game mode is 0x10: the original
    // still takes the clamped *6 override path in that combination.
    if(n%64u==31u){in.steering_override_active=1;in.game_mode=0x10;in.steering_override=5462;}
    if(n%64u==32u){in.steering_override_active=1;in.game_mode=0x10;in.steering_override=-5462;}
    return in;
}
struct RecordGhostFixture {
    Fixture x;
    std::array<std::uint8_t,RecordGhostHistoryBytes> history{};
};
RecordGhostFixture record_ghost_fixture(const Fixture& base,unsigned n){
    RecordGhostFixture out{base,{}};auto e=out.x.ev();Bytes h(out.history.data(),out.history.size());
    for(unsigned i=0;i<30;++i){
        const std::uint32_t bits=(n*40503u+i*7919u+0x1234u)&0xffffu;
        h.put16(i*2u,std::uint16_t(bits));
    }
    static constexpr std::int16_t steering[]={0,1,-1,5460,5461,5462,-5460,-5461,-5462,32767,static_cast<std::int16_t>(0x8000u)};
    e.put16(0x32,static_cast<std::uint16_t>(steering[(n*3u)%std::size(steering)]));
    e.put16(0x202,static_cast<std::uint16_t>(steering[(n*5u+2u)%std::size(steering)]));
    e.put16(0x204,std::uint16_t(0xa500u^(n&0xffffu)));
    // A few controlled histories make the signed average easy to stress at
    // extrema and around zero, while the general deterministic cases remain broad.
    if(n%64u==0u)for(unsigned i=0;i<30;++i)h.put16(i*2u,0);
    if(n%64u==1u)for(unsigned i=0;i<30;++i)h.put16(i*2u,0x7fffu);
    if(n%64u==2u)for(unsigned i=0;i<30;++i)h.put16(i*2u,0x8000u);
    if(n%64u==3u)for(unsigned i=0;i<30;++i)h.put16(i*2u,std::uint16_t((i&1u)?0x7fffu:0x8000u));
    return out;
}

Fixture calc_light_rate_fixture(const Fixture& base,unsigned n){
    Fixture x=base;auto e=x.ev();std::mt19937 gen(0x4f523043u^(n*3266489917u));
    auto rf=[&](float lo,float hi){return lo+(hi-lo)*(float(gen()&0xffffu)/65535.0f);};
    e.put8(0x283,std::uint8_t((n%11u)==0u?1u:0u));
    e.put16(0xd50,std::uint16_t((n*7u)%449u));
    const std::int16_t ref=static_cast<std::int16_t>((n*1229u)&0xffffu);
    e.put16(0xd4c,static_cast<std::uint16_t>(ref));
    auto add16=[&](std::int32_t d){return std::uint16_t(static_cast<std::uint16_t>(ref)+static_cast<std::uint16_t>(d));};
    // Cycle exact threshold-adjacent and broad wraparound deltas.
    static constexpr std::int32_t deltas[]={0,0x27ff,0x2800,0x2801,-0x27ff,-0x2800,-0x2801,0x3fff,0x4000,-0x4000,0x7fff,-0x8000};
    e.put16(0x160,add16(deltas[n%std::size(deltas)]));
    e.put16(0x2e,add16(deltas[(n/3u+5u)%std::size(deltas)]));
    e.putf(0x20,rf(-5.0f,5.0f));e.putf(0x24,rf(-2.0f,2.0f));e.putf(0x28,rf(-5.0f,5.0f));
    // A valid affine matrix with arbitrary translation/row-w canaries.  The
    // routine is expected to replace only the nine orientation words.
    for(unsigned k=0;k<16;++k)e.putf(0x70+k*4u,0.0f);
    const float a=rf(-2.5f,2.5f),c=std::cos(a),sn=std::sin(a);
    e.putf(0x70,c);e.putf(0x78,sn);e.putf(0x84,1.0f);e.putf(0x90,-sn);e.putf(0x98,c);e.putf(0xac,1.0f);
    e.putf(0x7c,rf(-1,1));e.putf(0x8c,rf(-1,1));e.putf(0x9c,rf(-1,1));
    e.putf(0xa0,rf(-100,100));e.putf(0xa4,rf(-10,10));e.putf(0xa8,rf(-100,100));
    if(n%64u==0u){e.putf(0x20,0.0f);e.putf(0x24,0.0f);e.putf(0x28,0.0f);}
    if(n%64u==1u){e.putf(0x20,0.00005f);e.putf(0x24,0.0f);e.putf(0x28,0.0f);}
    if(n%64u==2u){e.putf(0x20,1.0f);e.putf(0x24,0.0f);e.putf(0x28,0.0f);}
    if(n%64u==3u)e.put16(0xd50,0u);
    if(n%64u==4u)e.put16(0xd50,447u);
    if(n%64u==5u)e.put16(0xd50,448u);
    if(n%64u==6u)e.put16(0xd50,0xffffu);
    return x;
}

PcSessionModeInputs r043_session_inputs(unsigned n){
    PcSessionModeInputs in{};in.manager_active=(n%4u)!=0u;in.state_present=(n%5u)!=0u;
    static constexpr std::uint32_t codes[]={0u,3u,4u,5u,6u,7u,0xffffffffu};in.state_code=codes[n%std::size(codes)];return in;
}
std::array<CourseProbe,3> r043_radius_points(unsigned n){
    std::array<CourseProbe,3> p{};std::mt19937 g(0x4f523044u^(n*2654435761u));
    auto rf=[&](){return (float(int(g()%40001u)-20000))*0.01f;};
    for(auto& q:p)q={rf(),rf()*0.05f,rf()};
    switch(n%32u){
      case 0:p={CourseProbe{0,0,0},CourseProbe{1,0,0},CourseProbe{2,0,0}};break;
      case 1:p={CourseProbe{0,0,0},CourseProbe{1,0,0.00001f},CourseProbe{2,0,0}};break;
      case 2:p={CourseProbe{0,0,0},CourseProbe{1,0,-0.00001f},CourseProbe{2,0,0}};break;
      case 3:p={CourseProbe{-100000,0,0},CourseProbe{0,0,1},CourseProbe{100000,0,0}};break;
      case 4:p[0].x=-0.0f;break;
      case 5:p[2].z=0.0f;break;
      default:break;
    }
    return p;
}
PcOthcarGetRInputs r043_othcar_inputs(unsigned n){
    PcOthcarGetRInputs in{};in.max_cs_len=std::uint16_t(180u+(n%80u));in.road_ok={true,true,true};in.next_ok={true,true};
    in.centers=r043_radius_points(n+17u);in.alternate_radius=(n&1u)!=0u;
    switch(n%13u){case 5:in.road_ok[0]=false;break;case 6:in.next_ok[0]=false;break;case 7:in.road_ok[1]=false;break;case 8:in.next_ok[1]=false;break;case 9:in.road_ok[2]=false;break;default:break;}return in;
}
Fixture r043_othcar_fixture(const Fixture& base,unsigned n,const PcOthcarGetRInputs& in){
    Fixture x=base;auto e=x.ev();e.put8(0xb52,0u);e.putf(0xb54,float(int(n%401u)-200)*3.25f);
    e.put32(0x5c,(n%3u)==0u?0u:1u);e.puti(0x60,int(n*31u));e.put16(0x64,std::uint16_t(20u+(n%120u)));e.put16(0x66,std::uint16_t(n));e.puti(0x68,int(n*17u));e.puti(0x1c0,int(n*13u));
    std::uint8_t f=e.u8(0x04);if(n&2u)f|=0x20u;else f&=std::uint8_t(~0x20u);e.put8(0x04,f);
    switch(n%16u){
      case 0:e.put8(0xb52,3u);break;
      case 1:e.put8(0xb52,0xffu);break;
      case 2:e.put8(0xb52,0x80u);break;
      case 3:e.put16(0x64,std::uint16_t(in.max_cs_len-2u));break;
      case 4:e.put32(0x5c,1u);e.put16(0x64,9u);break;
      case 5:e.put32(0x5c,1u);e.put16(0x64,171u);break;
      case 6:e.put32(0x5c,0u);e.put16(0x64,0xffffu);break;
      default:break;
    }
    return x;
}

PcPlatformGhost4671Inputs r044_ghost_inputs(unsigned n){
    PcPlatformGhost4671Inputs in{};
    static constexpr std::int32_t modes[]={0,0x10,0x12,0x13};
    static constexpr std::int32_t routes[]={0,6,7,8};
    static constexpr std::int16_t timers[]={-1,0,1,59,60,61,62,32767};
    static constexpr std::int32_t counters[]={-1,0,58,59,60,61,100,std::numeric_limits<std::int32_t>::min(),std::numeric_limits<std::int32_t>::max()};
    in.game_mode=modes[n%std::size(modes)];
    in.route_state=routes[(n/3u)%std::size(routes)];
    in.session=r043_session_inputs(n+9u);
    in.timer=timers[(n/5u)%std::size(timers)];
    in.packet_flags=std::uint8_t((n*7u)&0xffu);
    in.frame_counter=counters[(n/11u)%std::size(counters)];
    // Deterministic branch sweeps for the three important paths.
    switch(n%16u){
      case 0:in.game_mode=0x10;in.route_state=7;in.timer=61;in.packet_flags=0;break;
      case 1:in.game_mode=0x12;in.route_state=7;in.timer=60;in.packet_flags=0;in.frame_counter=59;break;
      case 2:in.game_mode=0x10;in.route_state=7;in.timer=60;in.packet_flags=1;break;
      case 3:in.game_mode=0x10;in.route_state=6;in.session={true,true,4u};in.timer=61;break;
      case 4:in.game_mode=0x10;in.route_state=6;in.session={true,true,3u};in.timer=61;break;
      default:break;
    }
    return in;
}
PcPlatformGhost4671State r044_ghost_initial(unsigned n){
    PcPlatformGhost4671State st{};
    st.writer_offset=0x20u+(n%64u);st.reader_offset=0x80u+(n%64u);st.stream_index=0x10203040u+n;
    st.packet_11b=std::uint8_t(0xa0u+n);st.packet_11c=std::uint8_t(0x50u+n*3u);st.service_pending=std::uint8_t(0x40u+n);
    st.short_window=0x55660000u+n;st.serialize_called=false;return st;
}
Fixture r044_ghost_fixture(const Fixture& base,unsigned n){
    Fixture x=base;auto e=x.ev();e.put8(0x11,std::uint8_t(0x11u+n*5u));e.put8(0x12,std::uint8_t(0x22u+n*9u));return x;
}

PcPlatformGhost47f780Inputs r044_record_inputs(unsigned n){
    PcPlatformGhost47f780Inputs in{};
    in.entry_slot=1+std::int32_t((n*5u)%6u);
    in.game_mode=(n%5u)==0u?0x12:0x10;
    in.route_state=(n%7u)==0u?1:0;
    in.protected_gate=(n&1u)!=0u;
    in.flags=(n&2u)?4u:0u;
    in.allocate_result=0xa1000000u^(n*0x10203u);
    in.slot_external_gate=(n&4u)!=0u;
    static constexpr std::int16_t timers[]={-1,0,59,60,61,100,32767};
    in.timer=timers[(n/3u)%std::size(timers)];
    in.slot_token=0xb2000000u^(n*0x30405u);
    switch(n%16u){
      case 0:in.game_mode=0x10;in.route_state=0;in.protected_gate=false;in.flags=0;in.timer=60;break; // skip init, writer
      case 1:in.game_mode=0x10;in.route_state=0;in.protected_gate=true;in.flags=0;in.timer=60;break; // protected init, writer
      case 2:in.game_mode=0x10;in.route_state=0;in.protected_gate=false;in.flags=4;in.timer=60;break; // flag init then final early return
      case 3:in.game_mode=0x12;in.route_state=0;in.protected_gate=true;in.flags=0;in.timer=61;break; // mode skips init, reset service
      case 4:in.game_mode=0x10;in.route_state=1;in.protected_gate=true;in.flags=0;in.timer=61;break; // route skips init, reset service
      case 5:in.game_mode=0x10;in.route_state=0;in.protected_gate=true;in.flags=0;in.timer=61;break; // init then reset service
      default:break;
    }
    return in;
}
PcPlatformGhost47f780State r044_record_initial(unsigned n){
    PcPlatformGhost47f780State st{};
    st.init_state=(n%3u)==0u?0u:1u;
    st.sequence=0x10200000u+n;st.frame_counter=0x30400000u+n;
    st.reset114_bits=0x3f000000u+n;st.reset118_bits=0x40000000u+n;st.reset11c_bits=0x40400000u+n;
    st.marker121=std::uint8_t(0x40u+n);st.marker123=std::uint8_t(0x80u+n);st.record_ready=std::uint8_t(0x20u+n);
    return st;
}
std::vector<std::uint8_t> r044_record_blob(unsigned n){
    constexpr std::size_t size=8u*0xfd4u;std::vector<std::uint8_t> out(size);std::mt19937 g(0x440780u^(n*2654435761u));
    for(auto& b:out)b=std::uint8_t(g());return out;
}
std::array<std::uint32_t,32> r044_index_table(unsigned n){
    std::array<std::uint32_t,32> a{};for(unsigned k=0;k<a.size();++k)a[k]=0x51000000u^(n*0x10101u)^(k*0x203u);return a;
}
std::array<std::uint32_t,128> r044_pair_table(unsigned n){
    std::array<std::uint32_t,128> a{};for(unsigned k=0;k<a.size();++k)a[k]=0x62000000u^(n*0x1001u)^(k*0x40507u);return a;
}
Fixture r044_record_fixture(const Fixture& base,unsigned n){
    Fixture x=base;x.ev().put8(0x68,std::uint8_t(0x30u+n*7u));return x;
}

PcHandicapInputs handicap_inputs(unsigned n){
    PcHandicapInputs in{};
    in.active_nodes=std::uint8_t(n%9u);
    in.local_car_id=std::uint8_t((n*3u)%8u);
    in.race_ready=(n%7u)!=0u;
    in.reference_course_position=std::uint16_t((n*271u)%60000u);
    in.handicap_table_index=std::uint8_t(n%9u);
    in.stage_current=std::int32_t((n/5u)%16u);
    in.stage_reference=(n%4u)==0u?in.stage_current:std::int32_t((in.stage_current+1)%16);
    in.max_cs_len0=std::uint16_t(200u+(n*17u)%30000u);
    in.max_cs_len1=std::uint16_t(250u+(n*19u)%30000u);
    return in;
}
Fixture handicap_fixture(const Fixture& base,unsigned n,const PcHandicapInputs& in){
    Fixture x=base;std::mt19937 gen(0x4f52303bu ^ (n*374761393u));auto e=x.ev();
    e.put8(0x10,(n%6u)==0u?std::uint8_t(in.local_car_id^1u):in.local_car_id);
    std::uint32_t flags=gen();if(n%5u==0u)flags|=0x00200000u;else flags&=~0x00200000u;e.put32(0x04,flags);
    e.put16(0x260,std::uint16_t((n*149u)%60000u));
    e.put16(0x64,std::uint16_t((n*193u)%32000u));
    e.put16(0xdb8,std::uint16_t((n*31u)%1400u));
    e.put16(0xdba,std::uint16_t((n*29u)%1400u));
    e.putf(0x1c4,float(n%200u)*0.00125f);e.putf(0x178,float((n*7u)%200u)*0.00125f);
    e.put32(0x5c,(n%3u)==0u?0u:1u);
    e.put8(0xdb1,std::uint8_t(gen()));e.putf(0xdb4,float(gen()&0xffffu));e.putf(0xdbc,float(gen()&0xffffu));e.putf(0xdc0,float(gen()&0xffffu));
    if(n%64u==16u)e.put16(0xdb8,0);
    if(n%64u==17u)e.put16(0xdb8,0x4b0);
    if(n%64u==18u)e.put16(0xdba,1);
    if(n%64u==19u)e.put16(0xdba,0x4b0);
    if(n%64u==20u)e.put16(0xdba,0x4b1);
    if(n%64u==21u)e.putf(0x1c4,std::numeric_limits<float>::quiet_NaN());
    return x;
}
void prepare(std::uint32_t va){guest_call={};guest_call.entry=va;guest_call.sp=S;guest_call.mxcsr=0x1f80;guest_call.x87_cw=x87_control;}
void run(){run_original32();if(guest_call.out_sp!=S)throw std::runtime_error("guest stack imbalance");}
void compare_memory(const std::string& name,Fixture& expected){
    auto& s=stats[name];++s.cases;s.bytes+=expected.e.size()+expected.w.size()+expected.p.size();
    if(inject_mismatch && name=="cornering_power" && s.cases==1)expected.w[0x320]^=1;
    bool good=true;
    auto check=[&](const char* label,const void* actual,const auto& exp){
        auto* b=static_cast<const std::uint8_t*>(actual);
        for(std::size_t i=0;i<exp.size();++i)if(b[i]!=exp[i]){
            good=false;
            if(s.fail<4){
                std::cerr<<name<<" case="<<s.cases<<" "<<label<<" mismatch offset=0x"<<std::hex<<i
                         <<" actual="<<unsigned(b[i])<<" native="<<unsigned(exp[i])<<std::dec;
                if((name=="calc_suspension_force"||name=="car_sus_coli_check") && std::string(label)=="work"){
                    const std::size_t base=i&~std::size_t(3); float af=0,nf=0;
                    if(base+4<=exp.size()){std::memcpy(&af,b+base,4);std::memcpy(&nf,exp.data()+base,4);std::cerr<<" f32_actual="<<std::setprecision(10)<<af<<" f32_native="<<nf;}
                }
                std::cerr<<"\n";
            }
            break;
        }
    };
    check("event",reinterpret_cast<void*>(E),expected.e);check("work",reinterpret_cast<void*>(W),expected.w);check("parameters",reinterpret_cast<void*>(P),expected.p);
    if(good)++s.exact;else ++s.fail;
}
template<std::size_t N>void compare_blob(const std::string& name,const void* actual,const std::array<std::uint8_t,N>& expected){
    auto& st=stats[name];++st.cases;st.bytes+=N;const auto* p=static_cast<const std::uint8_t*>(actual);bool good=true;
    for(std::size_t i=0;i<N;++i)if(p[i]!=expected[i]){good=false;if(st.fail<5)std::cerr<<name<<" case="<<st.cases<<" mismatch offset=0x"<<std::hex<<i<<" actual="<<unsigned(p[i])<<" native="<<unsigned(expected[i])<<std::dec<<"\n";break;}
    if(good)++st.exact;else ++st.fail;
}
void compare_blob(const std::string& name,const void* actual,const std::vector<std::uint8_t>& expected){
    auto& st=stats[name];++st.cases;st.bytes+=expected.size();const auto* p=static_cast<const std::uint8_t*>(actual);bool good=true;
    for(std::size_t i=0;i<expected.size();++i)if(p[i]!=expected[i]){good=false;if(st.fail<5)std::cerr<<name<<" case="<<st.cases<<" mismatch offset=0x"<<std::hex<<i<<" actual="<<unsigned(p[i])<<" native="<<unsigned(expected[i])<<std::dec<<"\n";break;}
    if(good)++st.exact;else ++st.fail;
}
void compare_float(const std::string& name,float actual,float native){
    auto& s=stats[name];++s.cases;
    auto a=fbits(actual),b=fbits(native);auto key=[](std::uint32_t v){return v&0x80000000?~v:(v|0x80000000);};
    auto ka=key(a),kb=key(b);auto ulp=ka>kb?ka-kb:kb-ka;s.max_ulp=std::max(s.max_ulp,ulp);
    if(a==b)++s.exact;else{++s.fail;if(s.fail<=5)std::cerr<<name<<" mismatch actual="<<std::setprecision(10)<<actual<<" native="<<native<<" ULP="<<ulp<<"\n";}
}
void compare_u32(const std::string& name,std::uint32_t actual,std::uint32_t native){
    auto& st=stats[name];++st.cases;st.bytes+=4;
    if(actual==native)++st.exact;else{++st.fail;if(st.fail<=5)std::cerr<<name<<" mismatch actual="<<actual<<" native="<<native<<"\n";}
}

void compare_matrix_current(const std::string& name,Bytes native){
    native.check(0,64);auto& st=stats[name];++st.cases;st.bytes+=64;bool good=true;
    auto* actual=reinterpret_cast<const std::uint8_t*>(MatrixArena);
    for(std::size_t i=0;i<64;++i)if(actual[i]!=native.u8(i)){
        good=false;if(st.fail<5)std::cerr<<name<<" case="<<st.cases<<" matrix mismatch offset=0x"<<std::hex<<i<<" actual="<<unsigned(actual[i])<<" native="<<unsigned(native.u8(i))<<std::dec<<"\n";break;
    }
    if(good)++st.exact;else ++st.fail;
}
void compare_prediction_state(const EasyLctPredictionState& native){
    auto& st=stats["update_easy_lct_prediction_table"];++st.cases;st.bytes+=18u*4u;
    bool good=*reinterpret_cast<std::uint32_t*>(0x780240u)==native.cursor &&
              *reinterpret_cast<std::uint32_t*>(0x78023cu)==native.easy;
    for(unsigned k=0;k<16;++k)good=good&&(*reinterpret_cast<std::uint32_t*>(0x7801a8u+k*4u)==native.recent[k]);
    if(good)++st.exact;else{++st.fail;if(st.fail<=5)std::cerr<<"update_easy_lct_prediction_table state mismatch case="<<st.cases<<"\n";}
}
float ffrom(std::uint32_t u){float f;std::memcpy(&f,&u,4);return f;}
#include "course_spline_cases.hpp"
#include "course_query_cases.hpp"
#include "course_world_cases.hpp"
#include "ground_cases.hpp"
#include "wall_geometry_cases.hpp"
#include "wall_response_cases.hpp"
#include "wall_rebound_cases.hpp"
#include "crash_cases.hpp"
#include "crash_entry_cases.hpp"
RunningResistanceTuning running_tuning(){
    return {ffrom(*reinterpret_cast<std::uint32_t*>(0x6ae4e4)),ffrom(*reinterpret_cast<std::uint32_t*>(0x6ae4e8)),
            ffrom(*reinterpret_cast<std::uint32_t*>(0x6ae4ec)),ffrom(*reinterpret_cast<std::uint32_t*>(0x6ae4f0)),
            ffrom(*reinterpret_cast<std::uint32_t*>(0x6ae4f4)),ffrom(*reinterpret_cast<std::uint32_t*>(0x6ae4f8))};
}
}

// r074: functional-first closure of the runtime category services, scalar gates,
// native handle lookup leaves and the parent runtime gate 0x444470.
constexpr std::uint32_t R074StubBlock=0x32034000u;
constexpr std::uint32_t R074StateBase=0x33095000u;
constexpr std::uint32_t R074ObjectBase=0x3f250000u;
constexpr std::uint32_t R074CurrentBase=0x3f253000u;
constexpr std::uint32_t R074CategoryBase=0x3f258000u;
constexpr std::uint32_t R074RecordsBase=0x3f259000u;
constexpr std::size_t R074ObjectSize=0x2000u;
struct R074GateTrace{std::uint32_t configure_n{},configure_this{},selector{},flag{};};
void map_r074_runtime_gate(){
    auto* code=static_cast<std::uint8_t*>(map_at(R074StubBlock,4096,PROT_READ|PROT_WRITE));
    map_at(R074StateBase,4096,PROT_READ|PROT_WRITE);
    map_at(R074ObjectBase,0x3000,PROT_READ|PROT_WRITE);
    map_at(R074CurrentBase,0x2000,PROT_READ|PROT_WRITE);
    map_at(R074CategoryBase,4096,PROT_READ|PROT_WRITE);
    map_at(R074RecordsBase,4096,PROT_READ|PROT_WRITE);
    for(const std::uint32_t page:{0x67e000u,0x67f000u,0x780000u,0x7d3000u,0x7d6000u,0x82e000u,0x836000u})
        if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("r074 global page mprotect");
    auto put=[&](std::size_t off,const std::vector<std::uint8_t>& b){if(off+b.size()>4096u)throw std::runtime_error("r074 stub overflow");std::memcpy(code+off,b.data(),b.size());};
    // 4F1A90(...): return synthetic 0x44-stride record base.
    {std::vector<std::uint8_t>b{0xb8};r060_emit32(b,R074RecordsBase);b.push_back(0xc3);put(0x000,b);}
    // 4F1BA0(...): return synthetic record count from state+0.
    {std::vector<std::uint8_t>b{0xa1};r060_emit32(b,R074StateBase+0x00u);b.push_back(0xc3);put(0x040,b);}
    // 4035F0(): explicit current-object singleton view.
    {std::vector<std::uint8_t>b{0xb8};r060_emit32(b,R074CurrentBase);b.push_back(0xc3);put(0x080,b);}
    // 446F30(this,1,1): record the call and callee-clean two arguments.
    {std::vector<std::uint8_t>b;r060_inc(b,R074StateBase+0x20u);b.insert(b.end(),{0x89,0x0d});r060_emit32(b,R074StateBase+0x24u);b.insert(b.end(),{0x8b,0x44,0x24,0x04,0xa3});r060_emit32(b,R074StateBase+0x28u);b.insert(b.end(),{0x8b,0x44,0x24,0x08,0xa3});r060_emit32(b,R074StateBase+0x2cu);b.insert(b.end(),{0xc2,0x08,0x00});put(0x100,b);}
    if(mprotect(code,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("r074 stub mprotect");
}
void set_r074_patches(bool enabled){
    struct P{std::uint32_t site,target,stub;};static constexpr P p[]={
        {0x495955u,0x4f1a90u,R074StubBlock+0x000u},{0x495966u,0x4f1ba0u,R074StubBlock+0x040u},
        {0x44448du,0x4035f0u,R074StubBlock+0x080u},{0x444502u,0x446f30u,R074StubBlock+0x100u}};
    for(const auto& x:p){if(enabled)r060_check_call(x.site,x.target);r046_write_rel32(x.site,0xe8,enabled?x.stub:x.target);}
}
bool r074_oracle_name(const std::string& q){return q=="runtime_select_entry_4958a0_r074"||q=="runtime_count_entries_495930_r074"||q=="runtime_game_flag_43f870_r074"||q=="runtime_selection_disable_4957e0_r074"||q=="runtime_external_block_4872e0_r074"||q=="runtime_system_handle_active_4999c0_r074"||q=="runtime_menu_state_450240_r074"||q=="runtime_current_player_47f110_r074"||q=="runtime_feature_mask_4536f0_r074"||q=="object_runtime_gate_444470_r074"||q=="r074_runtime_gate_batch";}
void reset_r074_state(std::uint32_t record_count=0u){std::memset(reinterpret_cast<void*>(R074StateBase),0,0x80u);*reinterpret_cast<std::uint32_t*>(R074StateBase)=record_count;}
R074GateTrace r074_guest_gate_trace(){R074GateTrace t{};t.configure_n=*reinterpret_cast<std::uint32_t*>(R074StateBase+0x20u);t.configure_this=*reinterpret_cast<std::uint32_t*>(R074StateBase+0x24u);t.selector=*reinterpret_cast<std::uint32_t*>(R074StateBase+0x28u);t.flag=*reinterpret_cast<std::uint32_t*>(R074StateBase+0x2cu);return t;}
void r074_native_configure(void* u,Bytes embedded,std::uint32_t selector,std::uint8_t flag){auto& t=*static_cast<R074GateTrace*>(u);++t.configure_n;t.configure_this=R074ObjectBase+0x51cu;t.selector=selector;t.flag=flag;(void)embedded;}
PcObjectEventDispatch443eb0Services r074_dispatch_services(R071Trace& t){return {&t,r071_native_push,r071_native_set,r071_native_cb,r071_native_global};}
PcRuntimeControlServices r074_runtime_services(R071Trace& t){return r071_runtime_services(t);}
PcObjectOpenCallbackServices r074_open_services(R071Trace& t){return r071_open_services(t);}
std::vector<PcNativeHandleBinding> r074_handle_bindings(){std::vector<PcNativeHandleBinding> b;for(unsigned i=0;i<16u;++i)b.push_back({r071_handle(i),reinterpret_cast<void*>(std::uintptr_t(r071_handle(i))),0x40u});return b;}
std::array<std::uint8_t,R074ObjectSize> r074_object_fixture(unsigned n){
    std::array<std::uint8_t,R074ObjectSize> o{};std::mt19937 g(0x44447074u^(n*2654435761u));for(auto& b:o)b=std::uint8_t(g());Bytes x(o.data(),o.size());
    x.put32(0x518u,0u);x.put32(0x484u,0u);x.put8(0x48cu,0u);x.put8(0x494u,0u);x.put8(0x220u,0u);x.put32(0x218u,(n%3u)==0u?2u:1u);x.put32(0x488u,0u);return o;
}


// r075: close 44DA00 course preset selection plus 446BB0/446F30 embedded
// configuration. Large downstream loader/UI/effect bodies remain explicit
// trace boundaries, while already-closed 465250 executes directly.
constexpr std::uint32_t R075StubBlock=0x32035000u;
constexpr std::uint32_t R075StateBase=0x33096000u;
constexpr std::uint32_t R075ObjectBase=0x3f260000u;
constexpr std::size_t R075ObjectSize=0x700u;
struct R075Trace{
    std::uint32_t cfg_n{},cfg_this{},effect{},x{},y{},fin_n{},fin_this{},fx_n{},fx_value{};
};
void map_r075_runtime_blockers(){
    auto* code=static_cast<std::uint8_t*>(map_at(R075StubBlock,4096,PROT_READ|PROT_WRITE));
    map_at(R075StateBase,4096,PROT_READ|PROT_WRITE);
    map_at(R075ObjectBase,4096,PROT_READ|PROT_WRITE);
    if(mprotect(reinterpret_cast<void*>(0x7d2000u),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("r075 global page mprotect");
    auto put=[&](std::size_t off,const std::vector<std::uint8_t>& b){if(off+b.size()>4096u)throw std::runtime_error("r075 stub overflow");std::memcpy(code+off,b.data(),b.size());};
    // 44D720 cdecl: capture first two string arguments (32 bytes each),
    // capture arg6 (alternate path), return state+0x84.
    {std::vector<std::uint8_t>b{0x56,0x57,0x51,0x8b,0x74,0x24,0x10,0xbf};r060_emit32(b,R075StateBase+0x00u);b.insert(b.end(),{0xb9,0x08,0x00,0x00,0x00,0xf3,0xa5,0x8b,0x74,0x24,0x14,0xbf});r060_emit32(b,R075StateBase+0x40u);b.insert(b.end(),{0xb9,0x08,0x00,0x00,0x00,0xf3,0xa5,0x8b,0x44,0x24,0x24,0xa3});r060_emit32(b,R075StateBase+0x80u);b.push_back(0xa1);r060_emit32(b,R075StateBase+0x84u);b.insert(b.end(),{0x59,0x5f,0x5e,0xc3});put(0x000,b);}
    // 465860 thiscall, 11 callee-clean args: trace this/effect/x/y.
    {std::vector<std::uint8_t>b;r060_inc(b,R075StateBase+0x100u);b.insert(b.end(),{0x89,0x0d});r060_emit32(b,R075StateBase+0x104u);b.insert(b.end(),{0x8b,0x44,0x24,0x04,0xa3});r060_emit32(b,R075StateBase+0x108u);b.insert(b.end(),{0x8b,0x44,0x24,0x18,0xa3});r060_emit32(b,R075StateBase+0x10cu);b.insert(b.end(),{0x8b,0x44,0x24,0x1c,0xa3});r060_emit32(b,R075StateBase+0x110u);b.insert(b.end(),{0xc2,0x2c,0x00});put(0x100,b);}
    // 465970 thiscall: trace finalizer.
    {std::vector<std::uint8_t>b;r060_inc(b,R075StateBase+0x114u);b.insert(b.end(),{0x89,0x0d});r060_emit32(b,R075StateBase+0x118u);b.push_back(0xc3);put(0x180,b);}
    // 4249F0 cdecl one arg: trace global effect value.
    {std::vector<std::uint8_t>b;r060_inc(b,R075StateBase+0x11cu);b.insert(b.end(),{0x8b,0x44,0x24,0x04,0xa3});r060_emit32(b,R075StateBase+0x120u);b.push_back(0xc3);put(0x1c0,b);}
    // 5802DD sprintf-like helper used by 44DA00. The protected PC body jumps
    // through .rld; reproduce only this call site's fixed "%s_course" contract.
    {std::vector<std::uint8_t>b{
        0x56,0x57,                         // push esi; push edi
        0x8b,0x7c,0x24,0x0c,             // mov edi,[esp+0x0c] (dest)
        0x8b,0x74,0x24,0x14,             // mov esi,[esp+0x14] (source)
        0xac,                              // copy: lodsb
        0xaa,                              // stosb
        0x84,0xc0,                         // test al,al
        0x75,0xfa,                         // jne copy
        0x4f,                              // dec edi (overwrite copied NUL)
        0xb8,0x5f,0x63,0x6f,0x75,         // mov eax,"_cou"
        0xab,                              // stosd
        0xb8,0x72,0x73,0x65,0x00,         // mov eax,"rse\0"
        0xab,                              // stosd
        0x5f,0x5e,0xc3                    // pop edi; pop esi; ret
    };put(0x240,b);}
    if(mprotect(code,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("r075 stub mprotect");
}
void set_r075_course_patch(bool enabled){
    if(enabled)r060_check_call(0x44db3eu,0x5802ddu);
    r046_write_rel32(0x44db3eu,0xe8,enabled?R075StubBlock+0x240u:0x5802ddu);
    for(const auto site:{0x44db6bu,0x44db9bu}){if(enabled)r060_check_call(site,0x44d720u);r046_write_rel32(site,0xe8,enabled?R075StubBlock+0x000u:0x44d720u);}
}
void set_r075_embedded_patches(bool enabled){
    struct P{std::uint32_t site,target,stub;};static constexpr P p[]={
        {0x446c3du,0x465860u,R075StubBlock+0x100u},{0x446c44u,0x465970u,R075StubBlock+0x180u},
        {0x446f9eu,0x4249f0u,R075StubBlock+0x1c0u}};
    for(const auto& x:p){if(enabled)r060_check_call(x.site,x.target);r046_write_rel32(x.site,0xe8,enabled?x.stub:x.target);}
}
bool r075_oracle_name(const std::string& q){return q=="runtime_course_load_44da00_r075"||q=="embedded_slot_refresh_446bb0_r075"||q=="embedded_configure_446f30_r075"||q=="r075_runtime_blocker_batch";}
void reset_r075_state(){std::memset(reinterpret_cast<void*>(R075StateBase),0,0x140u);}
R075Trace r075_guest_trace(){R075Trace t{};t.cfg_n=*reinterpret_cast<std::uint32_t*>(R075StateBase+0x100u);t.cfg_this=*reinterpret_cast<std::uint32_t*>(R075StateBase+0x104u);t.effect=*reinterpret_cast<std::uint32_t*>(R075StateBase+0x108u);t.x=*reinterpret_cast<std::uint32_t*>(R075StateBase+0x10cu);t.y=*reinterpret_cast<std::uint32_t*>(R075StateBase+0x110u);t.fin_n=*reinterpret_cast<std::uint32_t*>(R075StateBase+0x114u);t.fin_this=*reinterpret_cast<std::uint32_t*>(R075StateBase+0x118u);t.fx_n=*reinterpret_cast<std::uint32_t*>(R075StateBase+0x11cu);t.fx_value=*reinterpret_cast<std::uint32_t*>(R075StateBase+0x120u);return t;}
struct R075NativeTrace{R075Trace t{};};
void r075_native_cfg(void* u,Bytes,std::uint32_t effect,std::uint32_t slot,std::uint32_t x,std::uint32_t y){auto& t=static_cast<R075NativeTrace*>(u)->t;++t.cfg_n;t.effect=effect;t.cfg_this=slot;t.x=x;t.y=y;}
void r075_native_fin(void* u,Bytes){++static_cast<R075NativeTrace*>(u)->t.fin_n;}
void r075_native_fx(void* u,std::uint32_t value){auto& t=static_cast<R075NativeTrace*>(u)->t;++t.fx_n;t.fx_value=value;}
std::uint32_t r075_native_load(void* u,const char* data,const char* course,std::uint8_t alt){(void)u;std::memset(reinterpret_cast<void*>(R075StateBase+0x200u),0,0x80u);std::strncpy(reinterpret_cast<char*>(R075StateBase+0x200u),data,31u);std::strncpy(reinterpret_cast<char*>(R075StateBase+0x240u),course,31u);*reinterpret_cast<std::uint32_t*>(R075StateBase+0x280u)=alt;return *reinterpret_cast<std::uint32_t*>(R075StateBase+0x84u);}
std::array<std::uint8_t,R075ObjectSize> r075_fixture(unsigned n){std::array<std::uint8_t,R075ObjectSize> o{};std::mt19937 g(0x446f3075u^(n*2654435761u));for(auto& b:o)b=std::uint8_t(g());Bytes x(o.data(),o.size());const auto depth=n%3u;x.put32(0x408u,depth);for(unsigned k=0;k<4u;++k){const auto off=0x40cu+k*0xa0u;x.put32(off+8u,0x75001000u+k);x.put32(off+0x24u,1u);x.put32(off+0x28u,0x11110000u+k);x.put32(off+0x2cu,0x22220000u+k);}return o;}
PcEmbeddedConfigServices446f30 r075_native_services(R075NativeTrace& t){PcEmbeddedConfigServices446f30 s{};s.user=&t;s.configure_ui=r075_native_cfg;s.finalize_ui=r075_native_fin;s.global_effect=r075_native_fx;return s;}

// r076: course runtime builder and its direct children.  The parent oracle uses
// the original direct-record path, so no Windows file/category service is
// fabricated. Immutable descriptor tables remain the original mapped data.
constexpr std::uint32_t R076StubBlock=0x32036000u;
constexpr std::uint32_t R076StateBase=0x33097000u;
constexpr std::uint32_t R076RecordsBase=0x3f270000u;
void map_r076_course_runtime(){
    auto* code=static_cast<std::uint8_t*>(map_at(R076StubBlock,4096,PROT_READ|PROT_WRITE));
    map_at(R076StateBase,4096,PROT_READ|PROT_WRITE);map_at(R076RecordsBase,0x3000,PROT_READ|PROT_WRITE);
    for(const std::uint32_t page:{0x635000u,0x7d3000u,0x7f9000u})
        if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("r076 global page mprotect");
    // Deterministic replacement only for the PC RNG call inside 44BF30.
    std::vector<std::uint8_t>b{0xa1};r060_emit32(b,R076StateBase+0x00u);b.push_back(0xc3);std::memcpy(code,b.data(),b.size());
    if(mprotect(code,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("r076 stub mprotect");
}
void set_r076_rng_patch(bool enabled){if(enabled)r060_check_call(0x44bf40u,0x580f40u);r046_write_rel32(0x44bf40u,0xe8,enabled?R076StubBlock:0x580f40u);}
bool r076_oracle_name(const std::string& q){return q=="runtime_course_shuffle_44bf30_r076"||q=="runtime_course_tree_44c850_r076"||q=="runtime_course_matrix_44c0d0_r076"||q=="runtime_course_force_mode_46c360_r076"||q=="runtime_apply_course_data_44d720_r076"||q=="r076_course_runtime_batch";}
std::int32_t r076_native_rng(void*){return *reinterpret_cast<std::int32_t*>(R076StateBase+0x00u);}
std::array<PcCourseDescriptor44d720,24> r076_primary_table(){std::array<PcCourseDescriptor44d720,24> out{};for(std::size_t k=0;k<out.size();++k){const auto token=*reinterpret_cast<std::uint32_t*>(0x6a54e0u+k*4u);out[k]={token,reinterpret_cast<void*>(std::uintptr_t(token)),0x98u};}return out;}
std::array<std::uint32_t,24> r076_secondary_table(){std::array<std::uint32_t,24> out{};for(std::size_t k=0;k<out.size();++k)out[k]=*reinterpret_cast<std::uint32_t*>(0x6a55e8u+k*4u);return out;}
std::vector<std::uint8_t> r076_records(unsigned seed,std::int32_t count){
    std::vector<std::uint8_t> out(std::size_t(count)*PcCourseRecord44d720Size);std::mt19937 g(0x44d72076u^(seed*2654435761u));for(auto& x:out)x=std::uint8_t(g());Bytes all(out.data(),out.size());
    for(std::int32_t k=0;k<count;++k){auto r=all.sub(std::size_t(k)*PcCourseRecord44d720Size,PcCourseRecord44d720Size);r.put32(0x04u,std::uint32_t((k+1)*10));r.put32(0x1cu,std::uint32_t((k+int(seed))%20));r.put32(0x20u,std::uint32_t((k*3+int(seed))%20));r.put32(0x2cu,k+1<count?std::uint32_t((k+2)*10):0xffffffffu);r.put32(0x30u,0xffffffffu);}
    if(count>=6){auto r=all.sub(0,PcCourseRecord44d720Size);r.put32(0x30u,60u);}return out;
}
void r076_prefill_globals(){
    *reinterpret_cast<std::uint32_t*>(0x635f34u)=0x42u;*reinterpret_cast<std::uint32_t*>(0x635f38u)=0x7600aa55u;*reinterpret_cast<std::uint32_t*>(0x7d33b0u)=0u;
    *reinterpret_cast<std::uint32_t*>(0x7d33c0u)=0u;*reinterpret_cast<std::uint32_t*>(0x7d33c4u)=0u;*reinterpret_cast<std::uint32_t*>(0x7f95a8u)=0x76feed00u;
    std::memset(reinterpret_cast<void*>(0x7d2da0u),0,0x420u);
}


// r077: exact category hash and fast-path lookup/count over the relocatable
// container published by 4F12A0. No filesystem calls are needed for these
// direct entry comparisons.
constexpr std::uint32_t R077StubBlock=0x32037000u;
constexpr std::uint32_t R077BlobBase=0x3f2a0000u;
constexpr std::uint32_t R077LoaderBase=0x3f2a6000u;
constexpr std::uint32_t R077NameBase=0x3f2a7000u;
void map_r077_course_provider(){
    auto* code=static_cast<std::uint8_t*>(map_at(R077StubBlock,0x1000,PROT_READ|PROT_WRITE));
    const std::uint8_t upper[]={0x8b,0x44,0x24,0x04,0x83,0xf8,0x61,0x7c,0x08,0x83,0xf8,0x7a,0x7f,0x03,0x83,0xe8,0x20,0xc3};
    std::memcpy(code,upper,sizeof(upper));
    if(mprotect(code,0x1000,PROT_READ|PROT_EXEC))throw std::runtime_error("r077 stub mprotect");
    map_at(R077BlobBase,0x6000,PROT_READ|PROT_WRITE);
    map_at(R077LoaderBase,0x1000,PROT_READ|PROT_WRITE);
    map_at(R077NameBase,0x1000,PROT_READ|PROT_WRITE);
}
void set_r077_hash_patch(bool enabled){if(enabled)r060_check_call(0x4f1275u,0x5822d2u);r046_write_rel32(0x4f1275u,0xe8,enabled?R077StubBlock:0x5822d2u);}
bool r077_oracle_name(const std::string& q){return q=="runtime_category_hash_4f1260_r077"||q=="runtime_category_records_4f1a90_r077"||q=="runtime_category_count_4f1ba0_r077"||q=="r077_course_provider_batch";}
struct R077Fixture{std::string query;PcRelocCategoryBlobR077 view{};};
R077Fixture r077_fixture(unsigned seed){
    std::memset(reinterpret_cast<void*>(R077BlobBase),0,0x6000u);
    std::memset(reinterpret_cast<void*>(R077LoaderBase),0,0x1000u);
    std::memset(reinterpret_cast<void*>(R077NameBase),0,0x1000u);
    const std::array<std::string,4> names{{"alpha_course","csc_data_cvt_course","OMEGA_COURSE","zeta_course"}};
    struct E{std::uint32_t hash,count,offset;std::string name;};std::array<E,4> e{};
    for(std::size_t k=0;k<e.size();++k)e[k]={runtime_category_hash_4f1260(names[k].c_str()),std::uint32_t(1u+((seed+unsigned(k)*3u)%9u)),std::uint32_t(0x200u+k*0x300u),names[k]};
    std::sort(e.begin(),e.end(),[](const E& a,const E& b){return a.hash<b.hash;});
    Bytes b(reinterpret_cast<void*>(R077BlobBase),0x6000u);b.put32(0,4u);b.put32(4,0u);
    for(std::size_t k=0;k<e.size();++k){const auto o=8u+k*12u;b.put32(o,e[k].hash);b.put32(o+4u,e[k].count);b.put32(o+8u,e[k].offset);for(std::size_t j=0;j<32u;++j)b.put8(e[k].offset+j,std::uint8_t(seed+k+j));}
    const auto sent=8u+e.size()*12u;b.put32(sent,0xffffffffu);b.put32(sent+4u,0u);b.put32(sent+8u,0u);
    Bytes l(reinterpret_cast<void*>(R077LoaderBase),16u);l.put32(0,R077BlobBase+8u);l.put32(4,R077BlobBase);l.put32(8,3u);
    const bool miss=(seed%5u)==0u;std::string q=miss?"missing_course":e[seed%e.size()].name;if(seed%3u==0u)for(char& c:q)if(c>='a'&&c<='z')c=char(c-'a'+'A');
    std::memcpy(reinterpret_cast<void*>(R077NameBase),q.c_str(),q.size()+1u);
    PcRelocCategoryBlobR077 view{};if(!course_reloc_blob_open_r077(reinterpret_cast<void*>(R077BlobBase),0x6000u,view))throw std::runtime_error("r077 fixture blob parse");
    return {q,view};
}

// r079: compact result dispatchers 0x445430/0x4454B0 plus the next concrete
// startup/update owner 0x445BE0.  Already-closed selector/commit/release code is
// reused by the native implementation; still-open owner children are patched to
// deterministic trace-only thiscall stubs so this proof does not overclaim them.
constexpr std::uint32_t R079StubBlock=0x32038000u;
constexpr std::uint32_t R079StateBase=0x33098000u;
constexpr std::uint32_t R079ObjectBase=0x3f2b0000u;
constexpr std::size_t R079ObjectSize=0x2000u;
constexpr std::size_t R079TraceSlots=16u;
struct R079Trace{
    std::uint32_t count{},selector_ret{};
    std::array<std::uint32_t,R079TraceSlots> pc{},ptr{},arg0{},arg1{};
};
void r079_emit_trace_prefix(std::vector<std::uint8_t>& b,std::uint32_t pc){
    // EAX = current trace index; store pc/ECX into parallel arrays.
    b.push_back(0xa1);r060_emit32(b,R079StateBase+0x00u);
    b.insert(b.end(),{0xc7,0x04,0x85});r060_emit32(b,R079StateBase+0x40u);r060_emit32(b,pc);
    b.insert(b.end(),{0x89,0x0c,0x85});r060_emit32(b,R079StateBase+0x80u);
    b.push_back(0x40);b.push_back(0xa3);r060_emit32(b,R079StateBase+0x00u);
}
void map_r079_runtime_owner(){
    auto* code=static_cast<std::uint8_t*>(map_at(R079StubBlock,0x1000,PROT_READ|PROT_WRITE));
    map_at(R079StateBase,0x1000,PROT_READ|PROT_WRITE);map_at(R079ObjectBase,0x2000,PROT_READ|PROT_WRITE);
    auto emit_this=[&](std::size_t off,std::uint32_t pc,bool set_mode){std::vector<std::uint8_t>b;r079_emit_trace_prefix(b,pc);if(set_mode)b.insert(b.end(),{0xc7,0x01,0x0e,0x00,0x00,0x00});b.push_back(0xc3);std::memcpy(code+off,b.data(),b.size());};
    emit_this(0x000,0x445500u,true);emit_this(0x040,0x441020u,false);emit_this(0x080,0x443110u,false);
    emit_this(0x0c0,0x444840u,false);emit_this(0x100,0x446cf0u,false);emit_this(0x140,0x445810u,false);
    emit_this(0x180,0x444530u,false);emit_this(0x1c0,0x445a50u,false);emit_this(0x200,0x4411a0u,false);
    emit_this(0x280,0x444470u,false);emit_this(0x2c0,0x444650u,false);
    // selector(this,&kind): write kind 3, return configured result, callee pops pointer.
    {std::vector<std::uint8_t>b;b.insert(b.end(),{0x8b,0x54,0x24,0x04,0xc7,0x02,0x03,0x00,0x00,0x00,0xa1});r060_emit32(b,R079StateBase+0x04u);b.insert(b.end(),{0xc2,0x04,0x00});std::memcpy(code+0x240,b.data(),b.size());}
    auto emit_action=[&](std::size_t off,std::uint32_t pc){std::vector<std::uint8_t>b;
        // Capture using current index before prefix increments it.
        b.push_back(0xa1);r060_emit32(b,R079StateBase+0x00u);
        b.insert(b.end(),{0x8b,0x54,0x24,0x04,0x89,0x14,0x85});r060_emit32(b,R079StateBase+0x0c0u);
        b.insert(b.end(),{0x8b,0x54,0x24,0x08,0x89,0x14,0x85});r060_emit32(b,R079StateBase+0x100u);
        r079_emit_trace_prefix(b,pc);b.insert(b.end(),{0x31,0xc0,0xc2,0x08,0x00});std::memcpy(code+off,b.data(),b.size());};
    emit_action(0x400,0x4450a0u);emit_action(0x440,0x445160u);emit_action(0x480,0x4448c0u);
    emit_action(0x4c0,0x4442a0u);emit_action(0x500,0x445310u);
    // r085: deterministic replacement for protected 0x4035F0 used by the
    // registered callback thunk 0x49E4A0. Return the same mapped owner object.
    {std::vector<std::uint8_t>b{0xb8};r060_emit32(b,R079ObjectBase);b.push_back(0xc3);std::memcpy(code+0x540,b.data(),b.size());}
    if(mprotect(code,0x1000,PROT_READ|PROT_EXEC))throw std::runtime_error("r079 stub mprotect");
}
void set_r079_dispatch_patches(bool enabled){
    struct P{std::uint32_t site,target,stub;};static constexpr P p[]={
        {0x44544au,0x4450a0u,R079StubBlock+0x400u},{0x445460u,0x445160u,R079StubBlock+0x440u},
        {0x445476u,0x4442a0u,R079StubBlock+0x4c0u},{0x44548cu,0x4448c0u,R079StubBlock+0x480u},
        {0x4454a2u,0x445310u,R079StubBlock+0x500u},{0x4454d9u,0x4442a0u,R079StubBlock+0x4c0u},
        {0x4454eau,0x445160u,R079StubBlock+0x440u}};
    for(const auto& x:p){if(enabled)r060_check_call(x.site,x.target);r046_write_rel32(x.site,0xe8,enabled?x.stub:x.target);}
}
void set_r079_owner_patches(bool enabled){
    struct P{std::uint32_t site,target,stub;};static constexpr P p[]={
        {0x445c29u,0x445500u,R079StubBlock+0x000u},{0x445c47u,0x441020u,R079StubBlock+0x040u},
        {0x445c4eu,0x443110u,R079StubBlock+0x080u},{0x445c55u,0x444840u,R079StubBlock+0x0c0u},
        {0x445c60u,0x446cf0u,R079StubBlock+0x100u},{0x445c67u,0x445810u,R079StubBlock+0x140u},
        {0x445c6eu,0x444530u,R079StubBlock+0x180u},{0x445c75u,0x445a50u,R079StubBlock+0x1c0u},
        {0x445c7cu,0x4411a0u,R079StubBlock+0x200u},{0x445c88u,0x442cb0u,R079StubBlock+0x240u},
        {0x445ce3u,0x444470u,R079StubBlock+0x280u},{0x445ceau,0x444650u,R079StubBlock+0x2c0u},
        {0x445cf5u,0x446cf0u,R079StubBlock+0x100u},{0x445d01u,0x442d70u,R079StubBlock+0x240u}};
    for(const auto& x:p){if(enabled)r060_check_call(x.site,x.target);r046_write_rel32(x.site,0xe8,enabled?x.stub:x.target);}
}
bool r079_oracle_name(const std::string& q){return q=="object_runtime_dispatch_primary_445430_r079"||q=="object_runtime_dispatch_secondary_4454b0_r079"||q=="object_runtime_owner_445be0_r079"||q=="r079_runtime_owner_batch";}
void set_r085_entry_patch(bool enabled){
    if(enabled)r060_check_call(0x49e4a0u,0x4035f0u);
    r046_write_rel32(0x49e4a0u,0xe8,enabled?R079StubBlock+0x540u:0x4035f0u);
}
bool r085_oracle_name(const std::string& q){return q=="runtime_owner_entry_49e4a0_r085"||q=="r085_callback_entry_batch";}

// r086: EvFuncID 36 static provider and lifecycle thunks.  The provider's
// protected transfer at 0x440114 is replaced only by its recovered semantic
// operation (load the current slot flags into CL).  The three 0x4035F0
// singleton lookups become deterministic current-object stubs so the original
// thunk continues into the already-closed runtime bodies.
constexpr std::uint32_t R086StubBlock=0x3203c000u;
void map_r086_event_function36(){
    auto* code=static_cast<std::uint8_t*>(map_at(R086StubBlock,4096,PROT_READ|PROT_WRITE));
    auto put_current=[&](std::size_t off,std::uint32_t object){std::vector<std::uint8_t>b{0xb8};r060_emit32(b,object);b.push_back(0xc3);std::memcpy(code+off,b.data(),b.size());};
    put_current(0x000u,R069ObjectBase); // 49E490 init -> 4436C0
    put_current(0x040u,R064ObjectBase); // 49E4B0 display -> 442E00
    put_current(0x080u,R070ObjectBase); // 49E4C0 destroy -> 443E90/443C30
    if(mprotect(code,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("r086 stub mprotect");
}
void set_r086_provider_patch(bool enabled){
    static const std::uint8_t original[6]={0xff,0x25,0xc8,0x9a,0x03,0x01};
    static const std::uint8_t semantic[6]={0x8a,0x8a,0x48,0xfb,0x79,0x00}; // mov cl,[edx+0x79FB48]
    const auto* orig=gate_original(0x440114u,original,6);
    if(enabled&&std::memcmp(reinterpret_cast<void*>(0x440114u),orig,6)!=0)throw std::runtime_error("r086 440110 protected transfer changed");
    r060_raw_write(0x440114u,enabled?semantic:orig,6);
}
void set_r086_init_entry_patch(bool enabled){
    if(enabled)r060_check_call(0x49e490u,0x4035f0u);
    r046_write_rel32(0x49e490u,0xe8,enabled?R086StubBlock+0x000u:0x4035f0u);
}
void set_r086_display_entry_patch(bool enabled){
    if(enabled)r060_check_call(0x49e4b0u,0x4035f0u);
    r046_write_rel32(0x49e4b0u,0xe8,enabled?R086StubBlock+0x040u:0x4035f0u);
}
void set_r086_destroy_entry_patch(bool enabled){
    if(enabled)r060_check_call(0x49e4c0u,0x4035f0u);
    r046_write_rel32(0x49e4c0u,0xe8,enabled?R086StubBlock+0x080u:0x4035f0u);
}
bool r086_oracle_name(const std::string& q){
    return q=="event_setup_440110_r086"||q=="runtime_init_entry_49e490_r086"||
           q=="runtime_display_entry_49e4b0_r086"||q=="runtime_teardown_alias_443e90_r086"||
           q=="runtime_destroy_entry_49e4c0_r086"||q=="r086_event_function36_batch";
}


// r087: fixed top-level bootstrap 0x417810.  All non-event child initializers
// are replaced by trace stubs, while the real original InitEventControl body
// at 0x440BF0 executes unchanged.  The two virtual calls use a deterministic
// dummy system object/vtable, so the original push/call ABI is exercised too.
constexpr std::uint32_t R087StubBlock=0x3203d000u;
constexpr std::uint32_t R087StateBase=0x3309c000u;
constexpr std::uint32_t R087SystemBase=0x3f240000u;
constexpr std::uint32_t R087VtableBase=R087SystemBase+0x100u;
constexpr std::size_t R087TraceSlots=16u;
constexpr std::uint32_t R087TraceCount=R087StateBase+0x00u;
constexpr std::uint32_t R087FinalReturn=R087StateBase+0x04u;
constexpr std::uint32_t R087TracePc=R087StateBase+0x40u;
constexpr std::uint32_t R087TraceA0=R087StateBase+0x80u;
constexpr std::uint32_t R087TraceA1=R087StateBase+0xc0u;
constexpr std::uint32_t R087TraceA2=R087StateBase+0x100u;

void r087_emit32(std::vector<std::uint8_t>& b,std::uint32_t v){for(unsigned k=0;k<4;++k)b.push_back(std::uint8_t(v>>(8u*k)));}
void r087_make_stub(std::uint8_t* code,std::size_t off,std::uint32_t pc,int argc,bool ret12=false,bool final_ret=false){
    std::vector<std::uint8_t>b;
    b.push_back(0x50);b.push_back(0x51);b.push_back(0x52); // save eax/ecx/edx
    b.push_back(0xa1);r087_emit32(b,R087TraceCount); // eax=count
    b.insert(b.end(),{0xc7,0x04,0x85});r087_emit32(b,R087TracePc);r087_emit32(b,pc);
    auto store_arg=[&](std::uint32_t base,int index){
        if(index<argc){
            // after 3 pushes: return=[esp+12], arg0=[esp+16]
            b.insert(b.end(),{0x8b,0x4c,0x24,std::uint8_t(16+index*4)});
            b.insert(b.end(),{0x89,0x0c,0x85});r087_emit32(b,base);
        }else{
            b.insert(b.end(),{0xc7,0x04,0x85});r087_emit32(b,base);r087_emit32(b,0u);
        }
    };
    store_arg(R087TraceA0,0);store_arg(R087TraceA1,1);store_arg(R087TraceA2,2);
    b.push_back(0x40);b.push_back(0xa3);r087_emit32(b,R087TraceCount);
    b.push_back(0x5a);b.push_back(0x59);b.push_back(0x58);
    if(final_ret){b.push_back(0xa1);r087_emit32(b,R087FinalReturn);}
    if(ret12){b.push_back(0xc2);b.push_back(0x0c);b.push_back(0x00);}else b.push_back(0xc3);
    if(b.size()>0x80u)throw std::runtime_error("r087 stub too large");
    std::memcpy(code+off,b.data(),b.size());
}
void map_r087_bootstrap(){
    auto* code=static_cast<std::uint8_t*>(map_at(R087StubBlock,4096,PROT_READ|PROT_WRITE));
    map_at(R087StateBase,4096,PROT_READ|PROT_WRITE);
    map_at(R087SystemBase,4096,PROT_READ|PROT_WRITE);
    if(mprotect(reinterpret_cast<void*>(0x89b000u),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("r087 system-global page mprotect");
    struct D{std::size_t off;std::uint32_t pc;int argc;bool ret12;bool final_ret;};
    static const D d[]={
        {0x000,0x4487d0u,0,false,false},{0x080,0x429b60u,0,false,false},
        {0x100,0x413b30u,0,false,false},{0x180,0x43f880u,0,false,false},
        {0x200,0x448ce0u,0,false,false},{0x280,0x44f7d0u,1,false,false},
        {0x300,0x49a650u,0,false,false},{0x380,0x41784fu,3,true,false},
        {0x400,0x414ce0u,0,false,false},{0x480,0x41786cu,3,true,false},
        {0x500,0x49a650u,0,false,false},{0x580,0x45ae10u,0,false,false},
        {0x600,0x44a630u,0,false,false},{0x680,0x416a40u,0,false,false},
        {0x700,0x448730u,0,false,true}};
    for(const auto& x:d)r087_make_stub(code,x.off,x.pc,x.argc,x.ret12,x.final_ret);
    if(mprotect(code,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("r087 stub mprotect");
    *reinterpret_cast<std::uint32_t*>(R087SystemBase)=R087VtableBase;
    *reinterpret_cast<std::uint32_t*>(R087VtableBase+0x1a8u)=R087StubBlock+0x380u;
    *reinterpret_cast<std::uint32_t*>(R087VtableBase+0x16cu)=R087StubBlock+0x480u;
}
void set_r087_bootstrap_patches(bool enabled){
    struct P{std::uint32_t site,target,stub;};static const P p[]={
        {0x417810u,0x4487d0u,R087StubBlock+0x000u},{0x417815u,0x429b60u,R087StubBlock+0x080u},
        {0x41781au,0x413b30u,R087StubBlock+0x100u},{0x41781fu,0x43f880u,R087StubBlock+0x180u},
        {0x417829u,0x448ce0u,R087StubBlock+0x200u},{0x417830u,0x44f7d0u,R087StubBlock+0x280u},
        {0x417838u,0x49a650u,R087StubBlock+0x300u},{0x417855u,0x414ce0u,R087StubBlock+0x400u},
        {0x417872u,0x49a650u,R087StubBlock+0x500u},{0x417877u,0x45ae10u,R087StubBlock+0x580u},
        {0x41787cu,0x44a630u,R087StubBlock+0x600u},{0x417881u,0x416a40u,R087StubBlock+0x680u}};
    for(const auto& x:p){if(enabled)r060_check_call(x.site,x.target);r046_write_rel32(x.site,0xe8,enabled?x.stub:x.target);}
    // tail JMP 448730
    r046_write_rel32(0x417886u,0xe9,enabled?R087StubBlock+0x700u:0x448730u);
}
void reset_r087_trace(std::uint32_t final_ret){std::memset(reinterpret_cast<void*>(R087StateBase),0,4096);*reinterpret_cast<std::uint32_t*>(R087FinalReturn)=final_ret;}
struct R087Trace{std::uint32_t count{};std::uint32_t final_ret{};std::array<std::uint32_t,R087TraceSlots> pc{},a0{},a1{},a2{};};
std::uint32_t r087_native_call(void* u,std::uint32_t pc,std::uint32_t a0,std::uint32_t a1,std::uint32_t a2){auto& t=*static_cast<R087Trace*>(u);if(t.count>=R087TraceSlots)throw std::runtime_error("r087 native trace overflow");const auto k=t.count++;t.pc[k]=pc;t.a0[k]=a0;t.a1[k]=a1;t.a2[k]=a2;return pc==0x448730u?t.final_ret:0u;}
std::array<std::uint8_t,0x140u> r087_trace_blob(const R087Trace& t){std::array<std::uint8_t,0x140u> o{};Bytes b(o.data(),o.size());b.put32(0,t.count);b.put32(4,t.final_ret);for(std::size_t k=0;k<R087TraceSlots;++k){b.put32(0x40u+k*4u,t.pc[k]);b.put32(0x80u+k*4u,t.a0[k]);b.put32(0xc0u+k*4u,t.a1[k]);b.put32(0x100u+k*4u,t.a2[k]);}return o;}
std::array<std::uint8_t,0x140u> r087_guest_trace_blob(){std::array<std::uint8_t,0x140u> o{};std::memcpy(o.data(),reinterpret_cast<void*>(R087StateBase),o.size());return o;}
bool r087_oracle_name(const std::string& q){return q=="runtime_bootstrap_417810_r087"||q=="r087_bootstrap_batch";}

// r088: direct startup/shutdown owner above r087.  The original 0x4176E0
// keeps the real original r087 bootstrap call.  Only its gate, the two larger
// post-bootstrap stages, three platform shutdown calls and the two virtual
// release methods are deterministic trace stubs.
constexpr std::uint32_t R088StubBlock=0x3203e000u;
constexpr std::uint32_t R088StateBase=0x3309d000u;
constexpr std::uint32_t R088SecondaryBase=0x3f2e0000u;
constexpr std::uint32_t R088SecondaryVtable=R088SecondaryBase+0x100u;
constexpr std::size_t R088TraceSlots=8u;
constexpr std::uint32_t R088TraceCount=R088StateBase+0x00u;
constexpr std::uint32_t R088GateValue=R088StateBase+0x04u;
constexpr std::uint32_t R088TracePc=R088StateBase+0x40u;
constexpr std::uint32_t R088TraceA0=R088StateBase+0x80u;
void r088_make_stub(std::uint8_t* code,std::size_t off,std::uint32_t pc,int argc,unsigned ret_bytes,bool gate_ret=false){
    std::vector<std::uint8_t>b;b.push_back(0x50);b.push_back(0x51);b.push_back(0x52);
    b.push_back(0xa1);r087_emit32(b,R088TraceCount);
    b.insert(b.end(),{0xc7,0x04,0x85});r087_emit32(b,R088TracePc);r087_emit32(b,pc);
    if(argc>0){b.insert(b.end(),{0x8b,0x4c,0x24,0x10});b.insert(b.end(),{0x89,0x0c,0x85});r087_emit32(b,R088TraceA0);}else{b.insert(b.end(),{0xc7,0x04,0x85});r087_emit32(b,R088TraceA0);r087_emit32(b,0u);}
    b.push_back(0x40);b.push_back(0xa3);r087_emit32(b,R088TraceCount);b.push_back(0x5a);b.push_back(0x59);b.push_back(0x58);
    if(gate_ret){b.push_back(0xa1);r087_emit32(b,R088GateValue);}
    if(ret_bytes){b.push_back(0xc2);b.push_back(std::uint8_t(ret_bytes));b.push_back(0x00);}else b.push_back(0xc3);
    if(b.size()>0x80u)throw std::runtime_error("r088 stub too large");std::memcpy(code+off,b.data(),b.size());
}
void map_r088_startup(){
    auto* code=static_cast<std::uint8_t*>(map_at(R088StubBlock,4096,PROT_READ|PROT_WRITE));map_at(R088StateBase,4096,PROT_READ|PROT_WRITE);map_at(R088SecondaryBase,4096,PROT_READ|PROT_WRITE);
    r088_make_stub(code,0x000,0x417740u,0,0,true);r088_make_stub(code,0x080,0x417b20u,0,0);r088_make_stub(code,0x100,0x417e30u,0,0);
    r088_make_stub(code,0x180,0x49a650u,0,0);r088_make_stub(code,0x200,0x40e3b0u,0,0);r088_make_stub(code,0x280,0x403fb0u,0,0);
    r088_make_stub(code,0x300,0x417713u,1,4);r088_make_stub(code,0x380,0x41772cu,1,4);
    if(mprotect(code,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("r088 stub mprotect");
    *reinterpret_cast<std::uint32_t*>(R087VtableBase+0x08u)=R088StubBlock+0x300u;
    *reinterpret_cast<std::uint32_t*>(R088SecondaryBase)=R088SecondaryVtable;*reinterpret_cast<std::uint32_t*>(R088SecondaryVtable+0x08u)=R088StubBlock+0x380u;
}
void set_r088_startup_patches(bool enabled){
    struct P{std::uint32_t site,target,stub;};static const P p[]={
        {0x4176e0u,0x417740u,R088StubBlock+0x000u},{0x4176eeu,0x417b20u,R088StubBlock+0x080u},{0x4176f3u,0x417e30u,R088StubBlock+0x100u},
        {0x4176f8u,0x49a650u,R088StubBlock+0x180u},{0x4176fdu,0x40e3b0u,R088StubBlock+0x200u},{0x417702u,0x403fb0u,R088StubBlock+0x280u}};
    for(const auto& x:p){if(enabled)r060_check_call(x.site,x.target);r046_write_rel32(x.site,0xe8,enabled?x.stub:x.target);}
}
void reset_r088_trace(std::uint32_t gate){std::memset(reinterpret_cast<void*>(R088StateBase),0,4096);*reinterpret_cast<std::uint32_t*>(R088GateValue)=gate;}
struct R088Trace{std::uint32_t count{},gate{};std::array<std::uint32_t,R088TraceSlots> pc{},a0{};};
struct R088NativeContext{R088Trace parent{};R087Trace child{};bool child_phase{};};
std::uint32_t r088_native_call(void* u,std::uint32_t pc,std::uint32_t a0,std::uint32_t a1,std::uint32_t a2){
    auto& c=*static_cast<R088NativeContext*>(u);
    if(c.child_phase||pc==0x4487d0u){c.child_phase=true;auto r=r087_native_call(&c.child,pc,a0,a1,a2);if(pc==0x448730u)c.child_phase=false;return r;}
    if(c.parent.count>=R088TraceSlots)throw std::runtime_error("r088 native trace overflow");const auto k=c.parent.count++;c.parent.pc[k]=pc;c.parent.a0[k]=a0;return pc==0x417740u?c.parent.gate:0u;
}
std::array<std::uint8_t,0xc0u> r088_trace_blob(const R088Trace& t){std::array<std::uint8_t,0xc0u>o{};Bytes b(o.data(),o.size());b.put32(0,t.count);b.put32(4,t.gate);for(std::size_t k=0;k<R088TraceSlots;++k){b.put32(0x40u+k*4u,t.pc[k]);b.put32(0x80u+k*4u,t.a0[k]);}return o;}
std::array<std::uint8_t,0xc0u> r088_guest_trace_blob(){std::array<std::uint8_t,0xc0u>o{};std::memcpy(o.data(),reinterpret_cast<void*>(R088StateBase),o.size());return o;}
bool r088_oracle_name(const std::string& q){return q=="runtime_startup_owner_4176e0_r088"||q=="r088_startup_owner_batch";}


// r090: direct platform/runtime init gate 0x417740.  All subsystem bodies and
// Win32 CreateThread/SetThreadPriority are deterministic trace stubs.  The
// original function still performs its real global/table writes and branching.
constexpr std::uint32_t R090StubBlock=0x3203f000u;
constexpr std::uint32_t R090StateBase=0x3309e000u;
constexpr std::size_t R090TraceSlots=16u;
constexpr std::uint32_t R090TraceCount=R090StateBase+0x00u;
constexpr std::uint32_t R090GateValue=R090StateBase+0x04u;
constexpr std::uint32_t R090ThreadValue=R090StateBase+0x08u;
constexpr std::uint32_t R090TracePc=R090StateBase+0x40u;
constexpr std::uint32_t R090TraceA0=R090StateBase+0x80u;
constexpr std::uint32_t R090TraceA1=R090StateBase+0xc0u;
constexpr std::uint32_t R090TraceA2=R090StateBase+0x100u;
enum class R090ArgMode{Stack,Eax,CreateThread};
void r090_make_stub(std::uint8_t* code,std::size_t off,std::uint32_t pc,int argc,unsigned ret_bytes,R090ArgMode mode=R090ArgMode::Stack,int ret_kind=0){
    std::vector<std::uint8_t>b;
    b.push_back(0x50);b.push_back(0x51);b.push_back(0x52);
    b.push_back(0xa1);r087_emit32(b,R090TraceCount);
    b.insert(b.end(),{0xc7,0x04,0x85});r087_emit32(b,R090TracePc);r087_emit32(b,pc);
    auto zero=[&](std::uint32_t base){b.insert(b.end(),{0xc7,0x04,0x85});r087_emit32(b,base);r087_emit32(b,0u);};
    auto from_stack=[&](std::uint32_t base,int index){b.insert(b.end(),{0x8b,0x4c,0x24,std::uint8_t(16+index*4)});b.insert(b.end(),{0x89,0x0c,0x85});r087_emit32(b,base);};
    if(mode==R090ArgMode::Eax){
        // original EAX was the first saved register and is now [esp+8]
        b.insert(b.end(),{0x8b,0x4c,0x24,0x08});b.insert(b.end(),{0x89,0x0c,0x85});r087_emit32(b,R090TraceA0);zero(R090TraceA1);zero(R090TraceA2);
    }else if(mode==R090ArgMode::CreateThread){
        // CreateThread arg2 is lpStartAddress; after 3 saves it is [esp+24].
        from_stack(R090TraceA0,2);zero(R090TraceA1);zero(R090TraceA2);
    }else{
        if(argc>0)from_stack(R090TraceA0,0);else zero(R090TraceA0);
        if(argc>1)from_stack(R090TraceA1,1);else zero(R090TraceA1);
        if(argc>2)from_stack(R090TraceA2,2);else zero(R090TraceA2);
    }
    b.push_back(0x40);b.push_back(0xa3);r087_emit32(b,R090TraceCount);
    b.push_back(0x5a);b.push_back(0x59);b.push_back(0x58);
    if(ret_kind==1){b.push_back(0xa1);r087_emit32(b,R090GateValue);}
    else if(ret_kind==2){b.push_back(0xa1);r087_emit32(b,R090ThreadValue);}
    if(ret_bytes){b.push_back(0xc2);b.push_back(std::uint8_t(ret_bytes));b.push_back(0x00);}else b.push_back(0xc3);
    if(b.size()>0x80u)throw std::runtime_error("r090 stub too large");std::memcpy(code+off,b.data(),b.size());
}
void r090_patch_import_call(std::uint32_t site,std::uint32_t iat,std::uint32_t stub,bool enabled){
    auto* page=reinterpret_cast<void*>(site&~0xfffu);if(mprotect(page,4096,PROT_READ|PROT_WRITE|PROT_EXEC))throw std::runtime_error("r090 import patch protection");
    auto* q=reinterpret_cast<std::uint8_t*>(site);const std::array<std::uint8_t,6> original{{0xff,0x15,std::uint8_t(iat),std::uint8_t(iat>>8),std::uint8_t(iat>>16),std::uint8_t(iat>>24)}};
    if(enabled){if(std::memcmp(q,original.data(),6)!=0)throw std::runtime_error("r090 imported CALL bytes changed");q[0]=0xe8;const std::uint32_t rel=stub-(site+5u);std::memcpy(q+1,&rel,4);q[5]=0x90;}
    else std::memcpy(q,original.data(),6);
    if(mprotect(page,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("r090 import patch restore protection");
}
void map_r090_platform_init(){
    auto* code=static_cast<std::uint8_t*>(map_at(R090StubBlock,4096,PROT_READ|PROT_WRITE));map_at(R090StateBase,4096,PROT_READ|PROT_WRITE);
    r090_make_stub(code,0x000,0x49a650u,0,0);r090_make_stub(code,0x080,0x40e470u,0,0,R090ArgMode::Eax,1);r090_make_stub(code,0x100,0x409bb0u,0,0);
    r090_make_stub(code,0x180,0x417779u,3,12);r090_make_stub(code,0x200,0x4231e0u,3,0);r090_make_stub(code,0x280,0x42ed00u,0,0);r090_make_stub(code,0x300,0x403de0u,0,0);
    r090_make_stub(code,0x380,0x4535f0u,0,0);r090_make_stub(code,0x400,0x49a650u,0,0);r090_make_stub(code,0x480,0x453440u,0,0);r090_make_stub(code,0x500,0x45acb0u,0,0);
    r090_make_stub(code,0x580,0x4493d0u,1,0);r090_make_stub(code,0x600,0x465df0u,0,0);r090_make_stub(code,0x680,0x427db0u,0,0);
    r090_make_stub(code,0x700,0x4177e6u,0,24,R090ArgMode::CreateThread,2);r090_make_stub(code,0x780,0x4177f3u,2,8);
    if(mprotect(code,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("r090 stub mprotect");
    // Reuse the deterministic system object already mapped for r087/r088.
    *reinterpret_cast<std::uint32_t*>(R087VtableBase+0x1d8u)=R090StubBlock+0x180u;
    for(std::uint32_t pg:{0x8a8000u,0x89f000u,0x899000u,0x955000u,0x740000u})if(mprotect(reinterpret_cast<void*>(pg),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("r090 global page mprotect");
}
void set_r090_platform_patches(bool enabled){
    struct P{std::uint32_t site,target,stub;};static const P p[]={
        {0x41774fu,0x49a650u,R090StubBlock+0x000u},{0x417759u,0x40e470u,R090StubBlock+0x080u},{0x417765u,0x409bb0u,R090StubBlock+0x100u},
        {0x417791u,0x4231e0u,R090StubBlock+0x200u},{0x417796u,0x42ed00u,R090StubBlock+0x280u},{0x41779bu,0x403de0u,R090StubBlock+0x300u},
        {0x4177aeu,0x4535f0u,R090StubBlock+0x380u},{0x4177b4u,0x49a650u,R090StubBlock+0x400u},{0x4177b9u,0x453440u,R090StubBlock+0x480u},
        {0x4177beu,0x45acb0u,R090StubBlock+0x500u},{0x4177cau,0x4493d0u,R090StubBlock+0x580u},{0x4177d2u,0x465df0u,R090StubBlock+0x600u},{0x4177d7u,0x427db0u,R090StubBlock+0x680u}};
    for(const auto& x:p){if(enabled)r060_check_call(x.site,x.target);r046_write_rel32(x.site,0xe8,enabled?x.stub:x.target);}
    r090_patch_import_call(0x4177e6u,0x596128u,R090StubBlock+0x700u,enabled);
    r090_patch_import_call(0x4177f3u,0x596130u,R090StubBlock+0x780u,enabled);
}
void reset_r090_trace(std::uint32_t gate,std::uint32_t thread){std::memset(reinterpret_cast<void*>(R090StateBase),0,4096);*reinterpret_cast<std::uint32_t*>(R090GateValue)=gate;*reinterpret_cast<std::uint32_t*>(R090ThreadValue)=thread;}
struct R090Trace{std::uint32_t count{},gate{},thread{};std::array<std::uint32_t,R090TraceSlots> pc{},a0{},a1{},a2{};};
std::uint32_t r090_native_call(void* u,std::uint32_t pc,std::uint32_t a0,std::uint32_t a1,std::uint32_t a2){auto& t=*static_cast<R090Trace*>(u);if(t.count>=R090TraceSlots)throw std::runtime_error("r090 native trace overflow");const auto k=t.count++;t.pc[k]=pc;t.a0[k]=a0;t.a1[k]=a1;t.a2[k]=a2;if(pc==0x40e470u)return t.gate;if(pc==0x4177e6u)return t.thread;return 0u;}
std::array<std::uint8_t,0x140u> r090_trace_blob(const R090Trace& t){std::array<std::uint8_t,0x140u>o{};Bytes b(o.data(),o.size());b.put32(0,t.count);b.put32(4,t.gate);b.put32(8,t.thread);for(std::size_t k=0;k<R090TraceSlots;++k){b.put32(0x40u+k*4u,t.pc[k]);b.put32(0x80u+k*4u,t.a0[k]);b.put32(0xc0u+k*4u,t.a1[k]);b.put32(0x100u+k*4u,t.a2[k]);}return o;}
std::array<std::uint8_t,0x140u> r090_guest_trace_blob(){std::array<std::uint8_t,0x140u>o{};std::memcpy(o.data(),reinterpret_cast<void*>(R090StateBase),o.size());return o;}
std::array<std::uint8_t,0x200u> r090_state_blob(const PcPlatformInitState417740& s){std::array<std::uint8_t,0x200u>o{};Bytes b(o.data(),o.size());b.put32(0x00,s.window_token);b.put32(0x04,s.resource_token_740ca0);b.put32(0x08,s.global_8a8ce0);b.put32(0x0c,s.global_8a8cac);b.put32(0x10,s.global_89f684);b.put32(0x14,s.global_89f66c);b.put32(0x18,s.thread_token);for(std::size_t k=0;k<s.scratch_8999c0.size();++k)b.put32(0x20u+k*4u,s.scratch_8999c0[k]);return o;}
std::array<std::uint8_t,0x200u> r090_guest_state_blob(){PcPlatformInitState417740 s{};s.window_token=*reinterpret_cast<std::uint32_t*>(0x8a8c88u);s.resource_token_740ca0=*reinterpret_cast<std::uint32_t*>(0x740ca0u);s.global_8a8ce0=*reinterpret_cast<std::uint32_t*>(0x8a8ce0u);s.global_8a8cac=*reinterpret_cast<std::uint32_t*>(0x8a8cacu);s.global_89f684=*reinterpret_cast<std::uint32_t*>(0x89f684u);s.global_89f66c=*reinterpret_cast<std::uint32_t*>(0x89f66cu);s.thread_token=*reinterpret_cast<std::uint32_t*>(0x955ad8u);std::memcpy(s.scratch_8999c0.data(),reinterpret_cast<void*>(0x8999c0u),s.scratch_8999c0.size()*4u);return r090_state_blob(s);}
bool r090_oracle_name(const std::string& q){return q=="runtime_platform_init_417740_r090"||q=="r090_platform_init_batch";}


// r091: loop-local cleanup 0x417970.  Only the direct 0x414C80 child is
// patched; virtual releases execute through deterministic dummy objects whose
// vtables point to trace stubs.  The original function still owns the global
// tests, clear ordering and optional-object branch.
constexpr std::uint32_t R091StubBlock=0x32040000u;
constexpr std::uint32_t R091TraceBase=0x330a0000u;
constexpr std::uint32_t R091ObjectBase=0x33110000u;
constexpr std::uint32_t R091VtableBase=0x33111000u;
constexpr std::size_t R091TraceSlots=12u;
constexpr std::uint32_t R091TraceCount=R091TraceBase+0x00u;
constexpr std::uint32_t R091TraceToken=R091TraceBase+0x40u;
constexpr std::uint32_t R091TraceOp=R091TraceBase+0x80u;
enum class R091ArgMode{StackThis,EcxThis,Direct};
void r091_make_stub(std::uint8_t* code,std::size_t off,std::uint32_t op,R091ArgMode mode,unsigned ret_bytes){
    std::vector<std::uint8_t>b;
    b.push_back(0x50);b.push_back(0x51);b.push_back(0x52); // save eax/ecx/edx
    b.push_back(0xa1);r087_emit32(b,R091TraceCount);       // eax=count
    if(mode==R091ArgMode::StackThis){
        b.insert(b.end(),{0x8b,0x4c,0x24,0x10});          // ecx=original [esp+4]
        b.insert(b.end(),{0x89,0x0c,0x85});r087_emit32(b,R091TraceToken);
    }else if(mode==R091ArgMode::EcxThis){
        b.insert(b.end(),{0x8b,0x4c,0x24,0x04});          // ecx=saved original ecx
        b.insert(b.end(),{0x89,0x0c,0x85});r087_emit32(b,R091TraceToken);
    }else{
        b.insert(b.end(),{0xc7,0x04,0x85});r087_emit32(b,R091TraceToken);r087_emit32(b,0u);
    }
    b.insert(b.end(),{0xc7,0x04,0x85});r087_emit32(b,R091TraceOp);r087_emit32(b,op);
    b.push_back(0x40);b.push_back(0xa3);r087_emit32(b,R091TraceCount);
    b.push_back(0x5a);b.push_back(0x59);b.push_back(0x58);
    if(ret_bytes){b.push_back(0xc2);b.push_back(std::uint8_t(ret_bytes));b.push_back(0x00);}else b.push_back(0xc3);
    if(b.size()>0x80u)throw std::runtime_error("r091 stub too large");std::memcpy(code+off,b.data(),b.size());
}
void map_r091_loop_cleanup(){
    auto* code=static_cast<std::uint8_t*>(map_at(R091StubBlock,4096,PROT_READ|PROT_WRITE));
    map_at(R091TraceBase,4096,PROT_READ|PROT_WRITE);map_at(R091ObjectBase,4096,PROT_READ|PROT_WRITE);map_at(R091VtableBase,4096,PROT_READ|PROT_WRITE);
    r091_make_stub(code,0x000,0x08u,R091ArgMode::StackThis,4);
    r091_make_stub(code,0x080,0x30u,R091ArgMode::StackThis,4);
    r091_make_stub(code,0x100,0x28u,R091ArgMode::EcxThis,0);
    r091_make_stub(code,0x180,0x414c80u,R091ArgMode::Direct,0);
    if(mprotect(code,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("r091 stub mprotect");
    *reinterpret_cast<std::uint32_t*>(R091VtableBase+0x08u)=R091StubBlock+0x000u;
    *reinterpret_cast<std::uint32_t*>(R091VtableBase+0x30u)=R091StubBlock+0x080u;
    *reinterpret_cast<std::uint32_t*>(R091VtableBase+0x28u)=R091StubBlock+0x100u;
    for(std::size_t k=0;k<8u;++k)*reinterpret_cast<std::uint32_t*>(R091ObjectBase+k*0x20u)=R091VtableBase;
    for(std::uint32_t pg:{0x8a8000u,0x95a000u,0x95b000u,0x7f9000u,0x89f000u})
        if(mprotect(reinterpret_cast<void*>(pg),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("r091 global page mprotect");
}
void set_r091_cleanup_patch(bool enabled){
    if(enabled)r060_check_call(0x41799du,0x414c80u);
    r046_write_rel32(0x41799du,0xe8,enabled?R091StubBlock+0x180u:0x414c80u);
}
void reset_r091_trace(){std::memset(reinterpret_cast<void*>(R091TraceBase),0,0x100u);}
struct R091Trace{std::uint32_t count{};std::array<std::uint32_t,R091TraceSlots> token{},op{};};
void r091_native_call(void* u,std::uint32_t pc,std::uint32_t token,std::uint32_t slot){
    auto& t=*static_cast<R091Trace*>(u);if(t.count>=R091TraceSlots)throw std::runtime_error("r091 native trace overflow");const auto k=t.count++;t.token[k]=token;t.op[k]=(pc==0x414c80u)?0x414c80u:slot;
}
std::array<std::uint8_t,0xc0u> r091_trace_blob(const R091Trace& t){std::array<std::uint8_t,0xc0u>o{};Bytes b(o.data(),o.size());b.put32(0,t.count);for(std::size_t k=0;k<R091TraceSlots;++k){b.put32(0x40u+k*4u,t.token[k]);b.put32(0x80u+k*4u,t.op[k]);}return o;}
std::array<std::uint8_t,0xc0u> r091_guest_trace_blob(){std::array<std::uint8_t,0xc0u>o{};std::memcpy(o.data(),reinterpret_cast<void*>(R091TraceBase),o.size());return o;}
std::array<std::uint8_t,0x20u> r091_state_blob(const PcRuntimeLoopCleanupState417970& st){std::array<std::uint8_t,0x20u>o{};Bytes b(o.data(),o.size());b.put32(0x00,st.handle_8a89f4);b.put32(0x04,st.handle_8a8a00);b.put32(0x08,st.handle_95afcc);b.put32(0x0c,st.handle_95afc4);b.put32(0x10,st.handle_95afc8);b.put32(0x14,st.object_95b218);b.put32(0x18,st.optional_7f94e8);b.put32(0x1c,st.handle_89f680);return o;}
PcRuntimeLoopCleanupState417970 r091_guest_state(){return {*reinterpret_cast<std::uint32_t*>(0x8a89f4u),*reinterpret_cast<std::uint32_t*>(0x8a8a00u),*reinterpret_cast<std::uint32_t*>(0x95afccu),*reinterpret_cast<std::uint32_t*>(0x95afc4u),*reinterpret_cast<std::uint32_t*>(0x95afc8u),*reinterpret_cast<std::uint32_t*>(0x95b218u),*reinterpret_cast<std::uint32_t*>(0x7f94e8u),*reinterpret_cast<std::uint32_t*>(0x89f680u)};}
bool r091_oracle_name(const std::string& q){return q=="runtime_loop_cleanup_417970_r091"||q=="r091_loop_cleanup_batch";}


// r092: companion loop setup 0x417A20.  Direct children are trace stubs; the
// 3-byte virtual calls use deterministic dummy vtable slots, while the two
// 6-byte +0x178 sites and +0x1D8 site are rewritten to unique direct stubs.
constexpr std::uint32_t R092StubBlock=0x32041000u;
constexpr std::uint32_t R092TraceBase=0x330a1000u;
constexpr std::size_t R092TraceSlots=16u;
constexpr std::uint32_t R092TraceCount=R092TraceBase+0x00u;
constexpr std::uint32_t R092Handle1=R092TraceBase+0x04u;
constexpr std::uint32_t R092Handle2=R092TraceBase+0x08u;
constexpr std::uint32_t R092Handle3=R092TraceBase+0x0cu;
constexpr std::uint32_t R092FinalRet=R092TraceBase+0x10u;
constexpr std::uint32_t R092TraceOp=R092TraceBase+0x40u;
constexpr std::uint32_t R092TraceToken=R092TraceBase+0x80u;
constexpr std::uint32_t R092SystemObj=R091ObjectBase+0x100u;
constexpr std::uint32_t R092AudioObj=R091ObjectBase+0x120u;
constexpr std::uint32_t R092OptionalObj=R091ObjectBase+0x140u;
enum class R092ArgMode{Direct,StackThis,EcxThis};
void r092_make_stub(std::uint8_t* code,std::size_t off,std::uint32_t op,R092ArgMode mode,unsigned ret_bytes,std::uint32_t write_global=0u,std::uint32_t handle_value=0u,bool return_final=false){
    std::vector<std::uint8_t>b;b.push_back(0x50);b.push_back(0x51);b.push_back(0x52);
    b.push_back(0xa1);r087_emit32(b,R092TraceCount);
    if(mode==R092ArgMode::StackThis){b.insert(b.end(),{0x8b,0x4c,0x24,0x10});b.insert(b.end(),{0x89,0x0c,0x85});r087_emit32(b,R092TraceToken);}
    else if(mode==R092ArgMode::EcxThis){b.insert(b.end(),{0x8b,0x4c,0x24,0x04});b.insert(b.end(),{0x89,0x0c,0x85});r087_emit32(b,R092TraceToken);}
    else {b.insert(b.end(),{0xc7,0x04,0x85});r087_emit32(b,R092TraceToken);r087_emit32(b,0u);}
    b.insert(b.end(),{0xc7,0x04,0x85});r087_emit32(b,R092TraceOp);r087_emit32(b,op);
    b.push_back(0x40);b.push_back(0xa3);r087_emit32(b,R092TraceCount);
    if(write_global){b.push_back(0xa1);r087_emit32(b,handle_value);b.push_back(0xa3);r087_emit32(b,write_global);}
    b.push_back(0x5a);b.push_back(0x59);b.push_back(0x58);
    if(return_final){b.push_back(0xa1);r087_emit32(b,R092FinalRet);}
    if(ret_bytes){b.push_back(0xc2);b.push_back(std::uint8_t(ret_bytes));b.push_back(0x00);}else b.push_back(0xc3);
    if(b.size()>0x80u)throw std::runtime_error("r092 stub too large");std::memcpy(code+off,b.data(),b.size());
}
void r092_patch_indirect6(std::uint32_t site,const std::array<std::uint8_t,6>& original,std::uint32_t stub,bool enabled){
    auto* q=reinterpret_cast<std::uint8_t*>(site);if(mprotect(reinterpret_cast<void*>(site&~0xfffu),4096,PROT_READ|PROT_WRITE|PROT_EXEC))throw std::runtime_error("r092 indirect patch protect");
    if(enabled){if(std::memcmp(q,original.data(),6)!=0)throw std::runtime_error("r092 indirect bytes changed");q[0]=0xe8;const std::uint32_t rel=stub-(site+5u);std::memcpy(q+1,&rel,4);q[5]=0x90;}else std::memcpy(q,original.data(),6);
    if(mprotect(reinterpret_cast<void*>(site&~0xfffu),4096,PROT_READ|PROT_EXEC))throw std::runtime_error("r092 indirect patch restore protect");
}
void map_r092_loop_setup(){
    auto* code=static_cast<std::uint8_t*>(map_at(R092StubBlock,4096,PROT_READ|PROT_WRITE));map_at(R092TraceBase,4096,PROT_READ|PROT_WRITE);
    r092_make_stub(code,0x000,0x4041e0u,R092ArgMode::Direct,0);
    r092_make_stub(code,0x080,0x417a36u,R092ArgMode::StackThis,16);
    r092_make_stub(code,0x100,0x417a4du,R092ArgMode::StackThis,16);
    r092_make_stub(code,0x180,0x417a6fu,R092ArgMode::StackThis,32,0x8a89f4u,R092Handle1);
    r092_make_stub(code,0x200,0x417a93u,R092ArgMode::StackThis,36,0x8a8a00u,R092Handle2);
    r092_make_stub(code,0x280,0x46bb90u,R092ArgMode::Direct,0);
    r092_make_stub(code,0x300,0x477ed0u,R092ArgMode::Direct,0);
    r092_make_stub(code,0x380,0x414c00u,R092ArgMode::Direct,0);
    r092_make_stub(code,0x400,0x4227c0u,R092ArgMode::Direct,0);
    r092_make_stub(code,0x480,0x417acau,R092ArgMode::StackThis,4);
    r092_make_stub(code,0x500,0x417ad9u,R092ArgMode::EcxThis,0);
    r092_make_stub(code,0x580,0x417aebu,R092ArgMode::StackThis,12,0x89f680u,R092Handle3);
    r092_make_stub(code,0x600,0x42fd90u,R092ArgMode::Direct,0,0,0,true);
    if(mprotect(code,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("r092 stub mprotect");
    *reinterpret_cast<std::uint32_t*>(R092SystemObj)=R091VtableBase;*reinterpret_cast<std::uint32_t*>(R092AudioObj)=R091VtableBase;*reinterpret_cast<std::uint32_t*>(R092OptionalObj)=R091VtableBase;
    *reinterpret_cast<std::uint32_t*>(R091VtableBase+0x64u)=R092StubBlock+0x180u;*reinterpret_cast<std::uint32_t*>(R091VtableBase+0x74u)=R092StubBlock+0x200u;*reinterpret_cast<std::uint32_t*>(R091VtableBase+0x34u)=R092StubBlock+0x480u;*reinterpret_cast<std::uint32_t*>(R091VtableBase+0x24u)=R092StubBlock+0x500u;
    for(std::uint32_t pg:{0x89b000u,0x95b000u,0x7f9000u,0x79f000u,0x780000u,0x8a8000u,0x89f000u,0x73e000u})if(mprotect(reinterpret_cast<void*>(pg),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("r092 global page mprotect");
}
void set_r092_setup_patches(bool enabled){
    struct P{std::uint32_t site,target,stub;};static const P p[]={
        {0x417a20u,0x4041e0u,R092StubBlock+0x000u},{0x417aaeu,0x46bb90u,R092StubBlock+0x280u},{0x417ab3u,0x477ed0u,R092StubBlock+0x300u},{0x417ab8u,0x414c00u,R092StubBlock+0x380u},{0x417abdu,0x4227c0u,R092StubBlock+0x400u}};
    for(const auto& x:p){if(enabled)r060_check_call(x.site,x.target);r046_write_rel32(x.site,0xe8,enabled?x.stub:x.target);}
    if(enabled){auto* q=reinterpret_cast<const std::uint8_t*>(0x417b0cu);std::int32_t rel{};std::memcpy(&rel,q+1,4);if(q[0]!=0xe9||std::uint32_t(std::int64_t(0x417b11u)+rel)!=0x42fd90u)throw std::runtime_error("r092 tail target changed");}
    r046_write_rel32(0x417b0cu,0xe9,enabled?R092StubBlock+0x600u:0x42fd90u);
    r092_patch_indirect6(0x417a36u,{{0xff,0x91,0x78,0x01,0x00,0x00}},R092StubBlock+0x080u,enabled);
    r092_patch_indirect6(0x417a4du,{{0xff,0x92,0x78,0x01,0x00,0x00}},R092StubBlock+0x100u,enabled);
    r092_patch_indirect6(0x417aebu,{{0xff,0x91,0xd8,0x01,0x00,0x00}},R092StubBlock+0x580u,enabled);
}
void reset_r092_trace(std::uint32_t h1,std::uint32_t h2,std::uint32_t h3,std::uint32_t final_ret){std::memset(reinterpret_cast<void*>(R092TraceBase),0,0x100u);*reinterpret_cast<std::uint32_t*>(R092Handle1)=h1;*reinterpret_cast<std::uint32_t*>(R092Handle2)=h2;*reinterpret_cast<std::uint32_t*>(R092Handle3)=h3;*reinterpret_cast<std::uint32_t*>(R092FinalRet)=final_ret;}
struct R092Trace{std::uint32_t count{},h1{},h2{},h3{},final_ret{};std::array<std::uint32_t,R092TraceSlots> op{},token{};};
std::uint32_t r092_native_call(void* u,std::uint32_t pc,const std::uint32_t* a,std::size_t n){auto& t=*static_cast<R092Trace*>(u);if(t.count>=R092TraceSlots)throw std::runtime_error("r092 native trace overflow");const auto k=t.count++;t.op[k]=pc;const bool virt=pc==0x417a36u||pc==0x417a4du||pc==0x417a6fu||pc==0x417a93u||pc==0x417acau||pc==0x417ad9u||pc==0x417aebu;t.token[k]=(virt&&n)?a[0]:0u;if(pc==0x417a6fu)return t.h1;if(pc==0x417a93u)return t.h2;if(pc==0x417aebu)return t.h3;if(pc==0x42fd90u)return t.final_ret;return 0u;}
std::array<std::uint8_t,0xc0u> r092_trace_blob(const R092Trace& t){std::array<std::uint8_t,0xc0u>o{};Bytes b(o.data(),o.size());b.put32(0,t.count);b.put32(0x04u,t.h1);b.put32(0x08u,t.h2);b.put32(0x0cu,t.h3);b.put32(0x10u,t.final_ret);for(std::size_t k=0;k<R092TraceSlots;++k){b.put32(0x40u+k*4u,t.op[k]);b.put32(0x80u+k*4u,t.token[k]);}return o;}
std::array<std::uint8_t,0xc0u> r092_guest_trace_blob(){std::array<std::uint8_t,0xc0u>o{};std::memcpy(o.data(),reinterpret_cast<void*>(R092TraceBase),o.size());return o;}
std::array<std::uint8_t,0x30u> r092_state_blob(const PcRuntimeLoopSetupState417a20& st){std::array<std::uint8_t,0x30u>o{};Bytes b(o.data(),o.size());b.put32(0x00,st.system_token);b.put32(0x04,st.object_95b218);b.put32(0x08,st.optional_7f94e8);b.put32(0x0c,st.feature_79fb50);b.put32(0x10,st.mode_78026c);b.put32(0x14,st.handle_8a89f4);b.put32(0x18,st.handle_8a8a00);b.put32(0x1c,st.handle_89f680);b.put32(0x20,st.global_89f684);b.put32(0x24,st.global_89f66c);b.put32(0x28,st.flag_73e2b0);return o;}
PcRuntimeLoopSetupState417a20 r092_guest_state(){PcRuntimeLoopSetupState417a20 s{};s.system_token=*reinterpret_cast<std::uint32_t*>(0x89bd60u);s.object_95b218=*reinterpret_cast<std::uint32_t*>(0x95b218u);s.optional_7f94e8=*reinterpret_cast<std::uint32_t*>(0x7f94e8u);s.feature_79fb50=*reinterpret_cast<std::uint8_t*>(0x79fb50u);s.mode_78026c=*reinterpret_cast<std::uint32_t*>(0x78026cu);s.handle_8a89f4=*reinterpret_cast<std::uint32_t*>(0x8a89f4u);s.handle_8a8a00=*reinterpret_cast<std::uint32_t*>(0x8a8a00u);s.handle_89f680=*reinterpret_cast<std::uint32_t*>(0x89f680u);s.global_89f684=*reinterpret_cast<std::uint32_t*>(0x89f684u);s.global_89f66c=*reinterpret_cast<std::uint32_t*>(0x89f66cu);s.flag_73e2b0=*reinterpret_cast<std::uint8_t*>(0x73e2b0u);return s;}
bool r092_oracle_name(const std::string& q){return q=="runtime_loop_setup_417a20_r092"||q=="r092_loop_setup_batch";}

// r093: QPC/QPF frame quantizer 0x417890.  The protected front transfer is
// left intact: it computes/pushes 0x8A8C98, the hidden QueryPerformanceFrequency
// out pointer.  Only the two imported clock calls are deterministic stubs.
constexpr std::uint32_t R093StubBlock=0x32042000u;
constexpr std::uint32_t R093TraceBase=0x330a2000u;
constexpr std::uint32_t R093TraceCount=R093TraceBase+0x00u;
constexpr std::uint32_t R093FreqValue=R093TraceBase+0x04u;
constexpr std::uint32_t R093CounterValue=R093TraceBase+0x0cu;
constexpr std::uint32_t R093TraceOp=R093TraceBase+0x20u;
void r093_make_query_stub(std::uint8_t* code,std::size_t off,std::uint32_t value_addr,std::uint32_t op){
    std::vector<std::uint8_t>b;
    b.insert(b.end(),{0x8b,0x54,0x24,0x04}); // edx = LARGE_INTEGER* out
    b.push_back(0xa1);r087_emit32(b,value_addr);b.insert(b.end(),{0x89,0x02});
    b.push_back(0xa1);r087_emit32(b,value_addr+4u);b.insert(b.end(),{0x89,0x42,0x04});
    b.insert(b.end(),{0x8b,0x0d});r087_emit32(b,R093TraceCount);
    b.insert(b.end(),{0xc7,0x04,0x8d});r087_emit32(b,R093TraceOp);r087_emit32(b,op);
    b.push_back(0x41);b.insert(b.end(),{0x89,0x0d});r087_emit32(b,R093TraceCount);
    b.push_back(0xb8);r087_emit32(b,1u);b.insert(b.end(),{0xc2,0x04,0x00});
    if(b.size()>0x80u)throw std::runtime_error("r093 query stub too large");std::memcpy(code+off,b.data(),b.size());
}
void map_r093_timing(){
    auto* code=static_cast<std::uint8_t*>(map_at(R093StubBlock,4096,PROT_READ|PROT_WRITE));map_at(R093TraceBase,4096,PROT_READ|PROT_WRITE);
    r093_make_query_stub(code,0x000,R093FreqValue,0x596104u);r093_make_query_stub(code,0x080,R093CounterValue,0x5960fcu);
    if(mprotect(code,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("r093 stub mprotect");
    for(std::uint32_t pg:{0x8a8000u,0x95a000u})if(mprotect(reinterpret_cast<void*>(pg),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("r093 global page mprotect");
}
void set_r093_timing_patches(bool enabled){
    auto* page=reinterpret_cast<void*>(0x417894u&~0xfffu);if(mprotect(page,4096,PROT_READ|PROT_WRITE|PROT_EXEC))throw std::runtime_error("r093 protection patch mprotect");
    auto* q=reinterpret_cast<std::uint8_t*>(0x417894u);const std::array<std::uint8_t,5> original{{0xe9,0xeb,0x06,0x03,0x00}};
    const auto* orig=gate_original(0x417894u,original.data(),5);
    if(enabled){if(std::memcmp(q,orig,5)!=0)throw std::runtime_error("r093 protected jump bytes changed");q[0]=0x68;const std::uint32_t out=0x8a8c98u;std::memcpy(q+1,&out,4);}else std::memcpy(q,orig,5);
    if(mprotect(page,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("r093 protection patch restore");
    r090_patch_import_call(0x417899u,0x596104u,R093StubBlock+0x000u,enabled);
    r090_patch_import_call(0x4178a4u,0x5960fcu,R093StubBlock+0x080u,enabled);
}
void r093_store64(std::uint32_t addr,std::int64_t v){std::memcpy(reinterpret_cast<void*>(addr),&v,sizeof(v));}
std::int64_t r093_load64(std::uint32_t addr){std::int64_t v{};std::memcpy(&v,reinterpret_cast<const void*>(addr),sizeof(v));return v;}
void reset_r093_trace(std::int64_t freq,std::int64_t current){std::memset(reinterpret_cast<void*>(R093TraceBase),0,0x100u);r093_store64(R093FreqValue,freq);r093_store64(R093CounterValue,current);}
struct R093Trace{std::int64_t freq{},current{};std::uint32_t count{};std::array<std::uint32_t,4> op{};};
std::int64_t r093_native_query(void* u,std::uint32_t op){auto& t=*static_cast<R093Trace*>(u);if(t.count>=t.op.size())throw std::runtime_error("r093 native trace overflow");t.op[t.count++]=op;return op==0x596104u?t.freq:t.current;}
std::array<std::uint8_t,0x40u> r093_trace_blob(const R093Trace& t){std::array<std::uint8_t,0x40u>o{};Bytes b(o.data(),o.size());b.put32(0,t.count);std::uint64_t f{},c{};std::memcpy(&f,&t.freq,8);std::memcpy(&c,&t.current,8);b.put32(4,std::uint32_t(f));b.put32(8,std::uint32_t(f>>32));b.put32(0x0c,std::uint32_t(c));b.put32(0x10,std::uint32_t(c>>32));for(std::size_t k=0;k<t.op.size();++k)b.put32(0x20u+k*4u,t.op[k]);return o;}
std::array<std::uint8_t,0x40u> r093_guest_trace_blob(){std::array<std::uint8_t,0x40u>o{};std::memcpy(o.data(),reinterpret_cast<void*>(R093TraceBase),o.size());return o;}
std::array<std::uint8_t,0x30u> r093_state_blob(const PcRuntimeTimingState417890& st){std::array<std::uint8_t,0x30u>o{};Bytes b(o.data(),o.size());auto put64=[&](std::size_t off,std::int64_t v){std::uint64_t u{};std::memcpy(&u,&v,8);b.put32(off,std::uint32_t(u));b.put32(off+4,std::uint32_t(u>>32));};put64(0x00,st.frequency_8a8c98);put64(0x08,st.current_counter_8a8ca0);put64(0x10,st.previous_counter_8a8c90);put64(0x18,st.accumulator_8a8cd0);b.put32(0x20,st.gate_8a8cc8);b.put32(0x24,fbits(st.frame_scale_95af40));return o;}
PcRuntimeTimingState417890 r093_guest_state(){PcRuntimeTimingState417890 s{};s.frequency_8a8c98=r093_load64(0x8a8c98u);s.current_counter_8a8ca0=r093_load64(0x8a8ca0u);s.previous_counter_8a8c90=r093_load64(0x8a8c90u);s.accumulator_8a8cd0=r093_load64(0x8a8cd0u);s.gate_8a8cc8=*reinterpret_cast<std::uint32_t*>(0x8a8cc8u);s.frame_scale_95af40=*reinterpret_cast<float*>(0x95af40u);return s;}
bool r093_oracle_name(const std::string& q){return q=="runtime_frame_ticks_417890_r093"||q=="r093_timing_batch";}


// r094: platform-neutral extraction of the central frame block inside 0x417B20.
// The original block 0x417C7B..0x417DD4 executes in place.  External subsystem
// calls are deterministic trace stubs; 0x417890 remains the original routine
// with the already-validated r093 deterministic QPF/QPC patch.  The two block
// exits are temporarily converted to marker returns (1=wait, 0=no wait).
constexpr std::uint32_t R094StubBlock=0x32043000u;
constexpr std::uint32_t R094TraceBase=0x330b2000u;
constexpr std::uint32_t R094TraceCount=R094TraceBase+0x00u;
constexpr std::uint32_t R094Cooperative=R094TraceBase+0x04u;
constexpr std::uint32_t R094Network=R094TraceBase+0x08u;
constexpr std::uint32_t R094Present=R094TraceBase+0x0cu;
constexpr std::uint32_t R094Elapsed1=R094TraceBase+0x10u;
constexpr std::uint32_t R094Elapsed2=R094TraceBase+0x14u;
constexpr std::uint32_t R094TraceOp=R094TraceBase+0x20u;
constexpr std::uint32_t R094TraceArg=R094TraceBase+0x120u;
constexpr std::size_t R094TraceSlots=64u;
enum class R094ArgMode{None,Stack,Ecx};
void r094_make_stub(std::uint8_t* code,std::size_t off,std::uint32_t op,R094ArgMode mode,unsigned ret_bytes,std::uint32_t return_addr=0u,bool st0=false){
    std::vector<std::uint8_t>b;b.push_back(0x50);b.push_back(0x51);b.push_back(0x52);
    b.push_back(0xa1);r087_emit32(b,R094TraceCount);
    b.insert(b.end(),{0xc7,0x04,0x85});r087_emit32(b,R094TraceOp);r087_emit32(b,op);
    if(mode==R094ArgMode::Stack){b.insert(b.end(),{0x8b,0x4c,0x24,0x10});b.insert(b.end(),{0x89,0x0c,0x85});r087_emit32(b,R094TraceArg);}
    else if(mode==R094ArgMode::Ecx){b.insert(b.end(),{0x8b,0x4c,0x24,0x04});b.insert(b.end(),{0x89,0x0c,0x85});r087_emit32(b,R094TraceArg);}
    else {b.insert(b.end(),{0xc7,0x04,0x85});r087_emit32(b,R094TraceArg);r087_emit32(b,0u);}
    b.push_back(0x40);b.push_back(0xa3);r087_emit32(b,R094TraceCount);
    b.push_back(0x5a);b.push_back(0x59);b.push_back(0x58);
    if(st0){b.insert(b.end(),{0xd9,0x05});r087_emit32(b,return_addr);}
    else if(return_addr){b.push_back(0xa1);r087_emit32(b,return_addr);}
    if(ret_bytes){b.push_back(0xc2);b.push_back(std::uint8_t(ret_bytes));b.push_back(0x00);}else b.push_back(0xc3);
    if(b.size()>0x80u)throw std::runtime_error("r094 stub too large");std::memcpy(code+off,b.data(),b.size());
}
void map_r094_frame_step(){
    auto* code=static_cast<std::uint8_t*>(map_at(R094StubBlock,4096,PROT_READ|PROT_WRITE));map_at(R094TraceBase,4096,PROT_READ|PROT_WRITE);
    r094_make_stub(code,0x000,0x449430u,R094ArgMode::None,0);
    r094_make_stub(code,0x080,0x455c20u,R094ArgMode::None,0,R094Cooperative);
    r094_make_stub(code,0x100,0x43fa10u,R094ArgMode::Stack,0);
    r094_make_stub(code,0x180,0x453bb0u,R094ArgMode::None,0);
    r094_make_stub(code,0x200,0x42f330u,R094ArgMode::None,0);
    r094_make_stub(code,0x280,0x455130u,R094ArgMode::None,0);
    r094_make_stub(code,0x300,0x43fa20u,R094ArgMode::None,0);
    r094_make_stub(code,0x380,0x43fab0u,R094ArgMode::None,0);
    r094_make_stub(code,0x400,0x480f80u,R094ArgMode::None,0);
    r094_make_stub(code,0x480,0x4666a0u,R094ArgMode::None,0);
    r094_make_stub(code,0x500,0x454670u,R094ArgMode::None,0);
    r094_make_stub(code,0x580,0x417d2bu,R094ArgMode::Stack,4);
    r094_make_stub(code,0x600,0x449050u,R094ArgMode::None,0);
    r094_make_stub(code,0x680,0x55a930u,R094ArgMode::Ecx,0,R094Network);
    r094_make_stub(code,0x700,0x4819c0u,R094ArgMode::Ecx,0);
    r094_make_stub(code,0x780,0x417d56u,R094ArgMode::Stack,4);
    r094_make_stub(code,0x800,0x417d68u,R094ArgMode::Stack,20,R094Present);
    r094_make_stub(code,0x880,0x4021b0u,R094ArgMode::Stack,0);
    r094_make_stub(code,0x900,0x44fba0u,R094ArgMode::Stack,0);
    r094_make_stub(code,0x980,0x449df0u,R094ArgMode::None,0,R094Elapsed1,true);
    r094_make_stub(code,0xa00,0x449df0u,R094ArgMode::None,0,R094Elapsed2,true);
    if(mprotect(code,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("r094 stub mprotect");
    if(mprotect(reinterpret_cast<void*>(0x8a8000u),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("r094 8a8 page mprotect");
    if(mprotect(reinterpret_cast<void*>(0x95a000u),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("r094 95a page mprotect");
}
void r094_patch_bytes(std::uint32_t addr,const std::uint8_t* expected,const std::uint8_t* repl,std::size_t n,bool enabled){
    auto* q=reinterpret_cast<std::uint8_t*>(addr);if(mprotect(reinterpret_cast<void*>(addr&~0xfffu),4096,PROT_READ|PROT_WRITE|PROT_EXEC))throw std::runtime_error("r094 patch protect");
    if(enabled)expected=gate_original(addr,expected,n);else if(g_steam_exe)expected=gate_original(addr,expected,n);
    const auto* want=enabled?expected:repl;if(std::memcmp(q,want,n)!=0)throw std::runtime_error("r094 patch bytes changed");std::memcpy(q,enabled?repl:expected,n);
    if(mprotect(reinterpret_cast<void*>(addr&~0xfffu),4096,PROT_READ|PROT_EXEC))throw std::runtime_error("r094 patch restore protect");
}
void set_r094_frame_patches(bool enabled){
    struct P{std::uint32_t site,target,stub;};static const P p[]={
      {0x417c7bu,0x449430u,R094StubBlock+0x000u},{0x417c80u,0x455c20u,R094StubBlock+0x080u},{0x417cc8u,0x43fa10u,R094StubBlock+0x100u},
      {0x417ce0u,0x453bb0u,R094StubBlock+0x180u},{0x417ce5u,0x42f330u,R094StubBlock+0x200u},{0x417ceau,0x455130u,R094StubBlock+0x280u},
      {0x417cefu,0x43fa20u,R094StubBlock+0x300u},{0x417cf4u,0x43fab0u,R094StubBlock+0x380u},{0x417cf9u,0x480f80u,R094StubBlock+0x400u},
      {0x417cfeu,0x4666a0u,R094StubBlock+0x480u},{0x417d1eu,0x454670u,R094StubBlock+0x500u},{0x417d31u,0x449050u,R094StubBlock+0x600u},
      {0x417d3bu,0x55a930u,R094StubBlock+0x680u},{0x417d49u,0x4819c0u,R094StubBlock+0x700u},{0x417d77u,0x4021b0u,R094StubBlock+0x880u},
      {0x417d8du,0x44fba0u,R094StubBlock+0x900u},{0x417d95u,0x449df0u,R094StubBlock+0x980u},{0x417dc3u,0x449df0u,R094StubBlock+0xa00u}};
    for(const auto& x:p){if(enabled)r060_check_call(x.site,x.target);r046_write_rel32(x.site,0xe8,enabled?x.stub:x.target);}
    static const std::array<std::uint8_t,6>a4{{0xff,0x91,0xa4,0x00,0x00,0x00}},a8{{0xff,0x92,0xa8,0x00,0x00,0x00}};
    r092_patch_indirect6(0x417d2bu,a4,R094StubBlock+0x580u,enabled);r092_patch_indirect6(0x417d56u,a8,R094StubBlock+0x780u,enabled);
    static const std::uint8_t wait_orig[6]={0x6a,0x01,0x53,0x53,0x53,0x8d},wait_ret[6]={0xb8,0x01,0x00,0x00,0x00,0xc3};
    static const std::uint8_t nowait_orig[6]={0x39,0x1d,0xac,0x8c,0x8a,0x00},nowait_ret[6]={0x31,0xc0,0xc3,0x90,0x90,0x90};
    r094_patch_bytes(0x417dd5u,wait_orig,wait_ret,6,enabled);r094_patch_bytes(0x417e10u,nowait_orig,nowait_ret,6,enabled);
}
void reset_r094_trace(std::uint32_t cooperative,std::uint32_t network,std::uint32_t present,float e1,float e2){
    std::memset(reinterpret_cast<void*>(R094TraceBase),0,0x300u);*reinterpret_cast<std::uint32_t*>(R094Cooperative)=cooperative;*reinterpret_cast<std::uint32_t*>(R094Network)=network;*reinterpret_cast<std::uint32_t*>(R094Present)=present;*reinterpret_cast<float*>(R094Elapsed1)=e1;*reinterpret_cast<float*>(R094Elapsed2)=e2;
}
struct R094Trace{std::uint32_t count{},cooperative{},network{},present{};float e1{},e2{};std::array<std::uint32_t,R094TraceSlots> op{},arg{};};
void r094_native_log(R094Trace& t,std::uint32_t pc,std::uint32_t arg){if(t.count>=R094TraceSlots)throw std::runtime_error("r094 native trace overflow");t.op[t.count]=pc;t.arg[t.count]=arg;++t.count;}
std::uint32_t r094_native_call(void* u,std::uint32_t pc,const std::uint32_t* a,std::size_t n){auto& t=*static_cast<R094Trace*>(u);r094_native_log(t,pc,n?a[0]:0u);if(pc==0x455c20u)return t.cooperative;if(pc==0x55a930u)return t.network;if(pc==0x417d68u)return t.present;return 0u;}
float r094_native_elapsed(void* u,std::uint32_t pc){auto& t=*static_cast<R094Trace*>(u);r094_native_log(t,pc,0u);const auto k=std::count(t.op.begin(),t.op.begin()+t.count,0x449df0u);return k==1?t.e1:t.e2;}
std::array<std::uint8_t,0x240u> r094_trace_blob(const R094Trace& t){std::array<std::uint8_t,0x240u>o{};Bytes b(o.data(),o.size());b.put32(0,t.count);b.put32(4,t.cooperative);b.put32(8,t.network);b.put32(0x0c,t.present);b.put32(0x10,fbits(t.e1));b.put32(0x14,fbits(t.e2));for(std::size_t k=0;k<R094TraceSlots;++k){b.put32(0x20u+k*4u,t.op[k]);b.put32(0x120u+k*4u,t.arg[k]);}return o;}
std::array<std::uint8_t,0x240u> r094_guest_trace_blob(){std::array<std::uint8_t,0x240u>o{};std::memcpy(o.data(),reinterpret_cast<void*>(R094TraceBase),o.size());return o;}
std::array<std::uint8_t,0x28u> r094_state_blob(const PcRuntimeFrameState417c7b& st){std::array<std::uint8_t,0x28u>o{};Bytes b(o.data(),o.size());b.put32(0x00,st.mode_78026c);b.put32(0x04,st.updates_95af48);b.put32(0x08,st.update_index_8a8cdc);b.put32(0x0c,st.primary_system_token);b.put32(0x10,st.frame_counter_95af0c);b.put32(0x14,fbits(st.elapsed_8a8cb4));b.put32(0x18,st.slow_frame_flag_8a8cc0);b.put32(0x1c,st.slow_frame_count_8a8cc4);return o;}
PcRuntimeFrameState417c7b r094_guest_state(){PcRuntimeFrameState417c7b s{};s.mode_78026c=*reinterpret_cast<std::uint32_t*>(0x78026cu);s.updates_95af48=*reinterpret_cast<std::uint32_t*>(0x95af48u);s.update_index_8a8cdc=*reinterpret_cast<std::uint32_t*>(0x8a8cdcu);s.primary_system_token=*reinterpret_cast<std::uint32_t*>(0x89bd60u);s.frame_counter_95af0c=*reinterpret_cast<std::uint32_t*>(0x95af0cu);s.elapsed_8a8cb4=*reinterpret_cast<float*>(0x8a8cb4u);s.slow_frame_flag_8a8cc0=*reinterpret_cast<std::uint32_t*>(0x8a8cc0u);s.slow_frame_count_8a8cc4=*reinterpret_cast<std::uint32_t*>(0x8a8cc4u);return s;}
bool r094_oracle_name(const std::string& q){return q=="runtime_frame_step_417c7b_r094"||q=="r094_frame_step_batch";}

void reset_r079_trace(std::uint32_t selector_ret){std::memset(reinterpret_cast<void*>(R079StateBase),0,0x180u);*reinterpret_cast<std::uint32_t*>(R079StateBase+4u)=selector_ret;}
std::array<std::uint8_t,0x140> r079_guest_trace_blob(){std::array<std::uint8_t,0x140> o{};std::memcpy(o.data(),reinterpret_cast<void*>(R079StateBase),o.size());return o;}
void r079_native_record(R079Trace& t,std::uint32_t pc,std::uint32_t ptr,std::uint32_t a0=0u,std::uint32_t a1=0u){if(t.count>=R079TraceSlots)throw std::runtime_error("r079 trace overflow");const auto i=t.count++;t.pc[i]=pc;t.ptr[i]=ptr;t.arg0[i]=a0;t.arg1[i]=a1;}
void r079_native_this(void* u,std::uint32_t pc,Bytes object,std::size_t off){auto& t=*static_cast<R079Trace*>(u);r079_native_record(t,pc,R079ObjectBase+std::uint32_t(off));if(pc==0x445500u)object.put32(0,0x0eu);}
void r079_native_action(void* u,std::uint32_t pc,Bytes,std::int32_t selected,std::uint32_t variant){r079_native_record(*static_cast<R079Trace*>(u),pc,R079ObjectBase,std::uint32_t(selected),variant);}
std::uint32_t r079_native_selector(void* u,std::uint32_t,std::uint32_t slot){if(slot!=0x08u)throw std::runtime_error("r079 selector slot");return static_cast<R079Trace*>(u)->selector_ret;}
void r079_native_manager(void*,std::uint32_t,std::uint32_t){}
PcRuntimeOwnerServices445be0 r079_owner_services(R079Trace& t){
    PcRuntimeOwnerServices445be0 s{};s.user=&t;s.call_this=r079_native_this;
    s.selector_services={&t,r079_native_selector,r079_native_manager};
    s.transition_services={&t,r079_native_action};s.state_services=r072_state_services();
    s.runtime_services={};s.commit_services=r072_commit_services();return s;
}
std::array<std::uint8_t,0x140> r079_trace_blob(const R079Trace& t){std::array<std::uint8_t,0x140> o{};Bytes b(o.data(),o.size());b.put32(0,t.count);b.put32(4,t.selector_ret);for(std::size_t i=0;i<R079TraceSlots;++i){b.put32(0x40u+i*4u,t.pc[i]);b.put32(0x80u+i*4u,t.ptr[i]);b.put32(0xc0u+i*4u,t.arg0[i]);b.put32(0x100u+i*4u,t.arg1[i]);}return o;}
std::array<std::uint8_t,R079ObjectSize> r079_object_fixture(unsigned n,std::uint32_t state){
    std::array<std::uint8_t,R079ObjectSize> o{};std::mt19937 g(0x445be079u^(n*2654435761u));for(auto& b:o)b=std::uint8_t(g());Bytes x(o.data(),o.size());
    x.put32(0x218u,state);x.put32(0x04u,0u);x.puti(0x484u,0);x.put8(0x48cu,0u);x.put8(0x494u,0u);x.puti(0x518u,1);x.put32(0x498u,0x79000000u+n);x.put8(0x220u,std::uint8_t(n%31u));
    const float old=float(std::int32_t(n%200u)-100)/13.0f;x.putf(0xd9cu,old);x.putf(0xd98u,0.0f);x.putf(0xda0u,0.0f);return o;
}

// r080: first mandatory state-2 child below r079.  0x4659F0 is allowed to
// execute its already-closed 0x4652E0/0x465250 leaves; the remaining larger UI
// bodies are deterministic trace stubs.  0x444840 executes 0x4659F0 and
// 0x564C90 directly, with only the still-open 0x4447D0 body patched.
constexpr std::uint32_t R080StubBlock=0x32039000u;
constexpr std::uint32_t R080StateBase=0x33099000u;
constexpr std::uint32_t R080ObjectBase=0x3f2d0000u;
constexpr std::uint32_t R080HandleBase=0x3f2d3000u;
constexpr std::size_t R080ObjectSize=0x2000u;
constexpr std::size_t R080TraceSlots=8u;
struct R080Trace{
    std::uint32_t count{};
    std::array<std::uint32_t,R080TraceSlots> pc{},ptr{},arg0{};
};
void r080_emit_trace_prefix(std::vector<std::uint8_t>& b,std::uint32_t pc){
    b.push_back(0xa1);r060_emit32(b,R080StateBase+0x00u);
    b.insert(b.end(),{0xc7,0x04,0x85});r060_emit32(b,R080StateBase+0x40u);r060_emit32(b,pc);
    b.insert(b.end(),{0x89,0x0c,0x85});r060_emit32(b,R080StateBase+0x80u);
    b.push_back(0x40);b.push_back(0xa3);r060_emit32(b,R080StateBase+0x00u);
}
void map_r080_ui_tick(){
    auto* code=static_cast<std::uint8_t*>(map_at(R080StubBlock,0x1000,PROT_READ|PROT_WRITE));
    map_at(R080StateBase,0x1000,PROT_READ|PROT_WRITE);map_at(R080ObjectBase,R080ObjectSize,PROT_READ|PROT_WRITE);map_at(R080HandleBase,0x1000,PROT_READ|PROT_WRITE);
    auto emit_this=[&](std::size_t off,std::uint32_t pc){std::vector<std::uint8_t>b;r080_emit_trace_prefix(b,pc);b.push_back(0xc3);std::memcpy(code+off,b.data(),b.size());};
    emit_this(0x000u,0x465970u);emit_this(0x040u,0x465590u);emit_this(0x080u,0x465460u);emit_this(0x0c0u,0x4656b0u);
    // 4447D0(this,key): capture the pushed key in arg0, then callee-pop it.
    {std::vector<std::uint8_t>b;b.push_back(0xa1);r060_emit32(b,R080StateBase+0x00u);b.insert(b.end(),{0x8b,0x54,0x24,0x04,0x89,0x14,0x85});r060_emit32(b,R080StateBase+0xc0u);r080_emit_trace_prefix(b,0x4447d0u);b.insert(b.end(),{0xc2,0x04,0x00});std::memcpy(code+0x100u,b.data(),b.size());}
    if(mprotect(code,0x1000,PROT_READ|PROT_EXEC))throw std::runtime_error("r080 stub mprotect");
}
void set_r080_patches(bool enabled){
    struct P{std::uint32_t site,target,stub;std::uint8_t op;};static constexpr P p[]={
        {0x465a45u,0x465970u,R080StubBlock+0x000u,0xe8u},
        {0x465a7bu,0x465590u,R080StubBlock+0x040u,0xe8u},
        {0x465a8cu,0x465460u,R080StubBlock+0x080u,0xe8u},
        {0x465a94u,0x4656b0u,R080StubBlock+0x0c0u,0xe9u},
        {0x444876u,0x4447d0u,R080StubBlock+0x100u,0xe8u}};
    for(const auto& x:p){if(enabled){const auto* q=reinterpret_cast<const std::uint8_t*>(x.site);if(q[0]!=x.op)throw std::runtime_error("r080 patch opcode");}r046_write_rel32(x.site,x.op,enabled?x.stub:x.target);}
}
bool r080_oracle_name(const std::string& q){return q=="ui_resource_tick_4659f0_r080"||q=="object_runtime_ui_tick_444840_r080"||q=="r080_runtime_ui_batch";}
void reset_r080_trace(){std::memset(reinterpret_cast<void*>(R080StateBase),0,0x100u);}
std::array<std::uint8_t,0xe0> r080_guest_trace_blob(){std::array<std::uint8_t,0xe0> o{};std::memcpy(o.data(),reinterpret_cast<void*>(R080StateBase),o.size());return o;}
void r080_native_record(R080Trace& t,std::uint32_t pc,std::uint32_t ptr,std::uint32_t arg0=0u){if(t.count>=R080TraceSlots)throw std::runtime_error("r080 trace overflow");const auto k=t.count++;t.pc[k]=pc;t.ptr[k]=ptr;t.arg0[k]=arg0;}
void r080_native_tick(void* u,std::uint32_t pc,Bytes){r080_native_record(*static_cast<R080Trace*>(u),pc,R080ObjectBase+0xcf4u);}
void r080_native_open(void* u,std::uint32_t pc,Bytes,std::uint32_t key){r080_native_record(*static_cast<R080Trace*>(u),pc,R080ObjectBase,key);}
std::array<std::uint8_t,0xe0> r080_trace_blob(const R080Trace& t){std::array<std::uint8_t,0xe0> o{};Bytes b(o.data(),o.size());b.put32(0,t.count);for(std::size_t i=0;i<R080TraceSlots;++i){b.put32(0x40u+i*4u,t.pc[i]);b.put32(0x80u+i*4u,t.ptr[i]);b.put32(0xc0u+i*4u,t.arg0[i]);}return o;}
std::array<std::uint8_t,R080ObjectSize> r080_object_fixture(unsigned n,bool parent){
    std::array<std::uint8_t,R080ObjectSize> o{};Bytes b(o.data(),o.size());auto r=b.sub(0xcf4u,o.size()-0xcf4u);
    const auto mode=n%8u;const std::uint32_t h=0x80800000u^(n*0x10203u);r.put32(0x08u,(mode==0u||mode==1u)?0xffffffffu:h);r.put32(0x0cu,0x800c0000u+n);r.put32(0x10u,0x80100000u+n);
    r.put32(0x18u,(mode==2u||mode==3u)?1u:0u);r.put32(0x1cu,(mode==2u||mode==3u)?1u:0u);r.put32(0x20u,mode==7u?1u:0u);r.put32(0x24u,(mode==2u||mode==3u)?0u:1u);r.put32(0x2cu,mode==2u?1u:0u);
    r.put32(0x30u,0x80300000u+n);r.put32(0x34u,0x80340000u+n);r.put32(0x38u,0x80380000u+n);r.put32(0x3cu,0x803c0000u+n);
    r.put32(0x88u,(mode==4u||mode==6u)?1u:0u);r.put32(0x8cu,(mode==5u||mode==6u)?1u:0u);r.put32(0x9cu,mode==3u?1u:0u);
    if(parent){b.put8(0xd94u,mode==1u?0u:1u);const auto idx=n%4u;b.put32(0x484u,idx);b.put32(0x280u+idx*4u,R080HandleBase);}
    return o;
}


// r081: the four-slot embedded UI controller immediately following r080's
// 0x444840 state-2 child.  The original 0x465250/0x4652E0/0x4659F0 bodies run
// directly; only 0x465860 (large configure body) and 0x465970 (finalizer) are
// replaced by trace stubs.  This validates 0x446A80, 0x446B20 and 0x446CF0
// without fabricating their key/effect/coordinate logic.
constexpr std::uint32_t R081StubBlock=0x3203a000u;
constexpr std::uint32_t R081StateBase=0x3309a000u;
constexpr std::uint32_t R081ObjectBase=0x3f2d5000u;
constexpr std::size_t R081ObjectSize=0x1000u;
constexpr std::size_t R081TraceSlots=16u;
struct R081Trace{
    std::uint32_t count{};
    std::array<std::uint32_t,R081TraceSlots> pc{},ptr{},effect{},x{},y{};
};
void r081_emit_cfg_stub(std::uint8_t* out,std::uint32_t pc_entry){
    std::vector<std::uint8_t>b;
    b.push_back(0xa1);r060_emit32(b,R081StateBase+0x00u); // eax=count
    b.insert(b.end(),{0xc7,0x04,0x85});r060_emit32(b,R081StateBase+0x40u);r060_emit32(b,pc_entry);
    b.insert(b.end(),{0x89,0x0c,0x85});r060_emit32(b,R081StateBase+0x80u); // this
    b.insert(b.end(),{0x8b,0x54,0x24,0x04,0x89,0x14,0x85});r060_emit32(b,R081StateBase+0xc0u); // effect
    b.insert(b.end(),{0x8b,0x54,0x24,0x18,0x89,0x14,0x85});r060_emit32(b,R081StateBase+0x100u); // x
    b.insert(b.end(),{0x8b,0x54,0x24,0x1c,0x89,0x14,0x85});r060_emit32(b,R081StateBase+0x140u); // y
    b.push_back(0x40);b.push_back(0xa3);r060_emit32(b,R081StateBase+0x00u);
    b.insert(b.end(),{0xc2,0x2c,0x00});
    std::memcpy(out,b.data(),b.size());
}
void r081_emit_finalize_stub(std::uint8_t* out){
    std::vector<std::uint8_t>b;
    b.push_back(0xa1);r060_emit32(b,R081StateBase+0x00u);
    b.insert(b.end(),{0xc7,0x04,0x85});r060_emit32(b,R081StateBase+0x40u);r060_emit32(b,0x465970u);
    // ptr/effect/x/y remain zero for this trace slot by design.
    b.push_back(0x40);b.push_back(0xa3);r060_emit32(b,R081StateBase+0x00u);b.push_back(0xc3);
    std::memcpy(out,b.data(),b.size());
}
void map_r081_embedded_slots(){
    auto* code=static_cast<std::uint8_t*>(map_at(R081StubBlock,0x1000,PROT_READ|PROT_WRITE));
    map_at(R081StateBase,0x1000,PROT_READ|PROT_WRITE);map_at(R081ObjectBase,R081ObjectSize,PROT_READ|PROT_WRITE);
    r081_emit_cfg_stub(code+0x000u,0x446a80u);r081_emit_cfg_stub(code+0x100u,0x446b20u);r081_emit_finalize_stub(code+0x200u);
    r081_emit_cfg_stub(code+0x300u,0x446c50u);
    if(mprotect(code,0x1000,PROT_READ|PROT_EXEC))throw std::runtime_error("r081 stub mprotect");
}
void set_r081_patches(bool enabled){
    struct P{std::uint32_t site,target,stub;};static constexpr P p[]={
        {0x446afeu,0x465860u,R081StubBlock+0x000u},{0x446b05u,0x465970u,R081StubBlock+0x200u},
        {0x446b9eu,0x465860u,R081StubBlock+0x100u},{0x446ba5u,0x465970u,R081StubBlock+0x200u},
        {0x446cddu,0x465860u,R081StubBlock+0x300u},{0x446ce4u,0x465970u,R081StubBlock+0x200u}};
    for(const auto& x:p){if(enabled)r060_check_call(x.site,x.target);r046_write_rel32(x.site,0xe8u,enabled?x.stub:x.target);}
}
bool r081_oracle_name(const std::string& q){return q=="embedded_slot_open_primary_446a80_r081"||q=="embedded_slot_open_secondary_446b20_r081"||q=="embedded_slots_tick_446cf0_r081"||q=="r081_embedded_slots_batch";}
void reset_r081_trace(){std::memset(reinterpret_cast<void*>(R081StateBase),0,0x200u);}
std::array<std::uint8_t,0x180> r081_guest_trace_blob(){std::array<std::uint8_t,0x180> o{};std::memcpy(o.data(),reinterpret_cast<void*>(R081StateBase),o.size());return o;}
void r081_record(R081Trace& t,std::uint32_t pc,std::uint32_t ptr=0u,std::uint32_t effect=0u,std::uint32_t x=0u,std::uint32_t y=0u){
    if(t.count>=R081TraceSlots)throw std::runtime_error("r081 trace overflow");const auto k=t.count++;t.pc[k]=pc;t.ptr[k]=ptr;t.effect[k]=effect;t.x[k]=x;t.y[k]=y;
}
void r081_native_cfg(void* u,std::uint32_t pc,Bytes,std::uint32_t effect,std::uint32_t slot,std::uint32_t x,std::uint32_t y){
    r081_record(*static_cast<R081Trace*>(u),pc,R081ObjectBase+0x40cu+slot*0xa0u,effect,x,y);
}
void r081_native_fin(void* u,std::uint32_t pc,Bytes){r081_record(*static_cast<R081Trace*>(u),pc);}
std::array<std::uint8_t,0x180> r081_trace_blob(const R081Trace& t){
    std::array<std::uint8_t,0x180> o{};Bytes b(o.data(),o.size());b.put32(0,t.count);
    for(std::size_t i=0;i<R081TraceSlots;++i){b.put32(0x40u+i*4u,t.pc[i]);b.put32(0x80u+i*4u,t.ptr[i]);b.put32(0xc0u+i*4u,t.effect[i]);b.put32(0x100u+i*4u,t.x[i]);b.put32(0x140u+i*4u,t.y[i]);}return o;
}
std::array<std::uint8_t,R081ObjectSize> r081_fixture(unsigned n,bool parent){
    std::array<std::uint8_t,R081ObjectSize> o{};std::mt19937 g(0x446cf081u^(n*2654435761u));for(auto& x:o)x=std::uint8_t(g());Bytes b(o.data(),o.size());
    const auto depth=n%4u;b.put32(0x408u,depth);static constexpr std::uint32_t keys[]{4u,8u,0x10u,0x20u,1u,2u,0x8000u,0x4000u,0xffffffffu,0x12345678u};
    for(std::uint32_t slot=0;slot<4u;++slot){const auto child=0x40cu+slot*0xa0u;b.put32(child+0x08u,0xffffffffu);b.put32(0x08u+std::size_t(depth)*0x20u+slot*4u,keys[(n+slot)%std::size(keys)]);b.put8(0x01u+slot,std::uint8_t((n+slot)&1u));}
    if(parent){b.put8(0x00u,std::uint8_t((n%5u)!=0u));for(std::uint32_t slot=0;slot<4u;++slot)b.put32(0x18u+std::size_t(depth)*0x20u+slot*4u,((n+slot)%3u)==0u?0x297u:(((n+slot)%3u)==1u?0x298u:0x123u));}
    return o;
}
PcEmbeddedSlotsServices446cf0 r081_services(R081Trace& t){PcEmbeddedSlotsServices446cf0 s{};s.user=&t;s.configure_ui=r081_native_cfg;s.finalize_ui=r081_native_fin;return s;}

// r083: fourth mandatory 0x445BE0 state-2 child.  The parent uses only
// already-closed primitives.  Direct original execution keeps 0x441260,
// 0x564C90, 0x443EB0, 0x4430B0 and 0x441200 real; r071's existing deterministic
// callback/virtual stubs isolate only their already-explicit external leaves.
bool r083_oracle_name(const std::string& q){return q=="object_runtime_transition_444530_r083"||q=="r083_runtime_transition_batch";}
struct R083Case{
    bool runtime_ready{};
    std::uint8_t request{},active{};
    bool previous_present{};
    std::uint8_t previous_flag{};
    std::uint32_t previous_state{0x0eu},created_state{0x0eu},ready_ret{1u};
};
R083Case r083_case(unsigned n){
    R083Case c{};c.active=std::uint8_t((n>>3u)&1u);
    switch(n%10u){
        case 0: break; // not ready, no request: no-op
        case 1: c.runtime_ready=true;break; // open 0E
        case 2: c.request=1u;c.created_state=0x0fu;break; // forced 0F
        case 3: c.runtime_ready=true;c.previous_present=true;c.previous_state=0x0eu;break; // exact match
        case 4: c.runtime_ready=true;c.previous_present=true;c.previous_state=0x0fu;break; // 0F blocks replacement
        case 5: c.request=1u;c.previous_present=true;c.previous_flag=1u;c.previous_state=0x0eu;c.created_state=0x0eu;break; // reject non-0F new
        case 6: c.request=1u;c.previous_present=true;c.previous_flag=1u;c.previous_state=0x0eu;c.created_state=0x0fu;break; // replace old
        case 7: c.runtime_ready=true;c.ready_ret=0u;break; // new 0E fails v+4
        case 8: c.request=1u;c.previous_present=true;c.previous_state=0x20u;c.created_state=0x0fu;break; // unflagged overwrite
        case 9: c.runtime_ready=true;c.previous_present=true;c.previous_flag=1u;c.previous_state=0x20u;c.created_state=0x0fu;break; // flagged old -> new state F
    }
    return c;
}
void r083_set_ready_globals(bool ready){
    *reinterpret_cast<std::uint32_t*>(0x7d68b0u)=0u;
    *reinterpret_cast<std::uint32_t*>(0x7d68acu)=0u;
    *reinterpret_cast<std::uint32_t*>(0x836130u)=ready?1u:0u;
    *reinterpret_cast<std::uint8_t*>(0x7d68d4u)=0u;
}

// r084: last still-open state-2 child below 0x445BE0.  The protected
// singleton and formatter/output leaves are deterministic boundaries; 0x434DE0,
// 0x4459F0 and 0x445A50 themselves execute as original PC bodies.
constexpr std::uint32_t R084StubBlock=0x3203b000u;
constexpr std::uint32_t R084StateBase=0x3309b000u;
constexpr std::uint32_t R084ManagerBase=0x3f2f0000u;
struct R084Trace{
    std::uint32_t action_n{},action_key{},format_n{},format_token{};
    std::uint32_t emit_n{},emit_mode{},emit_lifetime{},emit_limit{};
};
void map_r084_runtime_queue(){
    auto* code=static_cast<std::uint8_t*>(map_at(R084StubBlock,4096,PROT_READ|PROT_WRITE));
    map_at(R084StateBase,4096,PROT_READ|PROT_WRITE);map_at(R084ManagerBase,0x2000,PROT_READ|PROT_WRITE);
    for(const std::uint32_t page:{0x7d6000u,0x836000u,0x988000u,0x989000u,0x98a000u})
        if(mprotect(reinterpret_cast<void*>(std::uintptr_t(page)),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("r084 global page mprotect");
    auto put=[&](std::size_t off,const std::vector<std::uint8_t>& b){if(off+b.size()>4096u)throw std::runtime_error("r084 stub overflow");std::memcpy(code+off,b.data(),b.size());};
    // 444FE0(this,key): explicit alternate path of 4459F0, callee pops key.
    {std::vector<std::uint8_t>b;r060_inc(b,R084StateBase+0x00u);b.insert(b.end(),{0x8b,0x44,0x24,0x04,0xa3});r060_emit32(b,R084StateBase+0x04u);b.insert(b.end(),{0xc2,0x04,0x00});put(0x000u,b);}
    // 5802DD(dest,fmt,src): record fmt, return dest; caller cleans all args.
    {std::vector<std::uint8_t>b;r060_inc(b,R084StateBase+0x08u);b.insert(b.end(),{0x8b,0x44,0x24,0x08,0xa3});r060_emit32(b,R084StateBase+0x0cu);b.insert(b.end(),{0x8b,0x44,0x24,0x04,0xc3});put(0x080u,b);}
    // 492690(mode,buffer,-1,10): record externally visible scalar arguments.
    {std::vector<std::uint8_t>b;r060_inc(b,R084StateBase+0x10u);b.insert(b.end(),{0x8b,0x44,0x24,0x04,0xa3});r060_emit32(b,R084StateBase+0x14u);b.insert(b.end(),{0x8b,0x44,0x24,0x0c,0xa3});r060_emit32(b,R084StateBase+0x18u);b.insert(b.end(),{0x8b,0x44,0x24,0x10,0xa3});r060_emit32(b,R084StateBase+0x1cu);b.insert(b.end(),{0x31,0xc0,0xc3});put(0x100u,b);}
    if(mprotect(code,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("r084 stub mprotect");
}
void set_r084_patches(bool enabled){
    struct P{std::uint32_t site,target,stub;};static constexpr P p[]={
        {0x4459fdu,0x444fe0u,R084StubBlock+0x000u},
        {0x445a73u,0x4035f0u,R074StubBlock+0x080u},
        {0x445b69u,0x5802ddu,R084StubBlock+0x080u},{0x445bb0u,0x5802ddu,R084StubBlock+0x080u},
        {0x445bc0u,0x492690u,R084StubBlock+0x100u}};
    for(const auto& x:p){if(enabled)r060_check_call(x.site,x.target);r046_write_rel32(x.site,0xe8,enabled?x.stub:x.target);}
}
bool r084_oracle_name(const std::string& q){return q=="runtime_pair_idle_434de0_r084"||q=="object_runtime_open_4459f0_r084"||q=="object_runtime_queue_445a50_r084"||q=="r084_runtime_queue_batch";}
void reset_r084_trace(){std::memset(reinterpret_cast<void*>(R084StateBase),0,0x40u);}
std::array<std::uint8_t,32> r084_guest_trace_blob(){std::array<std::uint8_t,32> o{};std::memcpy(o.data(),reinterpret_cast<void*>(R084StateBase),o.size());return o;}
std::array<std::uint8_t,32> r084_trace_blob(const R084Trace& t){std::array<std::uint8_t,32> o{};std::memcpy(o.data(),&t,o.size());return o;}
void r084_native_action(void* u,std::uint32_t pc,Bytes,std::uint32_t key){if(pc!=0x444fe0u)throw std::runtime_error("r084 action pc");auto& t=*static_cast<R084Trace*>(u);++t.action_n;t.action_key=key;}
void r084_native_emit(void* u,std::uint32_t pc,std::uint32_t mode,std::uint32_t fmt,Bytes,std::size_t,std::int32_t life,std::uint32_t limit){if(pc!=0x492690u)throw std::runtime_error("r084 emit pc");auto& t=*static_cast<R084Trace*>(u);++t.format_n;t.format_token=fmt;++t.emit_n;t.emit_mode=mode;t.emit_lifetime=std::uint32_t(life);t.emit_limit=limit;}
PcRuntimeOpen4459f0Services r084_action_services(R084Trace& t){return {&t,r084_native_action};}
PcRuntimeQueueServices445a50 r084_queue_services(R084Trace& t){return {&t,r084_native_emit};}
struct R084Case{
    std::uint8_t gate_d2{},alternate{1u},request{},busy{},flag494{};
    std::uint32_t current_state{2u},gate44{},active{},manager_a{},manager_b{};
    bool runtime_ready{};unsigned queue_kind{};std::uint32_t ready_ret{1u};
};
R084Case r084_case(unsigned n){
    R084Case c{};
    switch(n%14u){
        case 0:c.gate_d2=1u;break;case 1:c.alternate=0u;break;case 2:c.current_state=1u;break;
        case 3:c.manager_a=1u;break;case 4:c.flag494=1u;break;case 5:c.runtime_ready=true;break;
        case 6:c.request=1u;break;case 7:c.gate44=1u;break;case 8:c.active=1u;break;case 9:c.busy=1u;break;
        case 10:c.queue_kind=1u;c.ready_ret=(n&1u)?1u:0u;break;case 11:c.queue_kind=5u;c.ready_ret=(n&1u)?1u:0u;break;
        case 12:c.queue_kind=7u;break;case 13:c.queue_kind=99u;break;
    }
    return c;
}
PcRuntimeQueueState445a50 r084_queue_fixture(unsigned n,const R084Case& c){
    PcRuntimeQueueState445a50 q{};q.gate_7d68d2=c.gate_d2;q.alternate_7d68bc=c.alternate;q.request_7d68cb=c.request;q.busy_98a5f4=c.busy;q.gate_988f44=c.gate44;q.active_989318=c.active;
    q.out_989320=0x84000000u^n;q.out_989324=0x84100000u^n;q.out_989328=0x84200000u^n;q.out_98932c=0x84300000u^n;
    auto fill=[&](std::size_t idx,std::uint32_t type,std::uint8_t ch){Bytes b(q.slots[idx].data(),q.slots[idx].size());b.put32(0,type);b.put32(8,0x10000000u+n+std::uint32_t(idx));b.put32(0x0c,0x20000000u+n+std::uint32_t(idx));b.put8(0x10,ch);b.put32(0x21,0x30000000u+n+std::uint32_t(idx));b.put32(0x25,0x40000000u+n+std::uint32_t(idx));};
    if(c.queue_kind==1u)fill(0,1u,'A');else if(c.queue_kind==5u){q.slots[0][0x29u]=1u;fill(1,5u,'B');}
    else if(c.queue_kind==7u){fill(0,7u,'X');fill(1,1u,'C');}
    else if(c.queue_kind==99u)for(auto& a:q.slots)a[0x29u]=1u;
    else fill(0,1u,'N');
    return q;
}
void r084_store_guest_queue(const PcRuntimeQueueState445a50& q){
    *reinterpret_cast<std::uint8_t*>(0x7d68d2u)=q.gate_7d68d2;*reinterpret_cast<std::uint8_t*>(0x7d68bcu)=q.alternate_7d68bc;*reinterpret_cast<std::uint8_t*>(0x7d68cbu)=q.request_7d68cb;
    *reinterpret_cast<std::uint8_t*>(0x98a5f4u)=q.busy_98a5f4;*reinterpret_cast<std::uint32_t*>(0x988f44u)=q.gate_988f44;*reinterpret_cast<std::uint32_t*>(0x989318u)=q.active_989318;
    *reinterpret_cast<std::uint32_t*>(0x989320u)=q.out_989320;*reinterpret_cast<std::uint32_t*>(0x989324u)=q.out_989324;*reinterpret_cast<std::uint32_t*>(0x989328u)=q.out_989328;*reinterpret_cast<std::uint32_t*>(0x98932cu)=q.out_98932c;
    std::memcpy(reinterpret_cast<void*>(0x988f58u),q.slots.data(),PcRuntimeQueueSlotCount445a50*PcRuntimeQueueSlotSize445a50);
}
std::array<std::uint8_t,PcRuntimeQueueSlotCount445a50*PcRuntimeQueueSlotSize445a50> r084_slots_blob(const PcRuntimeQueueState445a50& q){std::array<std::uint8_t,PcRuntimeQueueSlotCount445a50*PcRuntimeQueueSlotSize445a50> o{};std::memcpy(o.data(),q.slots.data(),o.size());return o;}
#include "platform/race_sky.hpp"
#include "platform/race_area_runtime.hpp"
#include "platform/race_area.hpp"
#include "platform/race_ghosts.hpp"
#include "platform/race_end_modes.hpp"
#include "platform/arcade_attract.hpp"
#include "platform/race_name_entry.hpp"
#include "platform/race_autoscene.hpp"
#include "platform/race_ending.hpp"
#include "platform/race_ending_services.hpp"
#include "platform/race_records.hpp"
#include "platform/pc_address_view.hpp"
#include "platform/race_traffic.hpp"
#include "platform/race_goal_camera.hpp"
#include "platform/race_scene_effects.hpp"   // scn-efc
#include "platform/race_particles.hpp"   // scn-efc
#include "platform/race_robots.hpp"
#include "platform/rob_motion_tables.hpp"
#include "platform/rob_motion.hpp"
#include "platform/rob_disp.hpp"
#include "platform/object_db.hpp"
#include "platform/rob_osage.hpp"
#include "platform/rob_motion_engine.hpp"
#include "platform/rob_flag.hpp"
#include "platform/pc_shadow_volume.hpp"
#include "platform/embedded_vehicle_draw_data.hpp"
#include "platform/pc_sprite_2d.hpp"
#include "platform/sp_rankings.hpp"
#include <filesystem>               // 2D queue flush 42D710
#include "fake_d3d9.inc"
int main(int argc,char** argv){try{
    // Loading the player's EXE image (OR2_EXE) before main frees a large buffer,
    // which raises glibc's mmap threshold: later big allocations would grow the
    // brk heap up into the fixed guest ranges (0x2F000000..). Pin it back.
    mallopt(M_MMAP_THRESHOLD,128*1024);
    // At -O0 some diagnostic case functions retain large local fixture frames.
    // Raise only the runner's soft stack limit; this does not affect guest memory
    // or the reconstructed outrun_driving library.
    struct rlimit stack_limit{};
    if(getrlimit(RLIMIT_STACK,&stack_limit)==0){
        constexpr rlim_t DesiredStack=64u*1024u*1024u;
        const rlim_t wanted=(stack_limit.rlim_max==RLIM_INFINITY)?DesiredStack:std::min(stack_limit.rlim_max,DesiredStack);
        if(stack_limit.rlim_cur<wanted){stack_limit.rlim_cur=wanted;setrlimit(RLIMIT_STACK,&stack_limit);}
    }
    if(argc<2){std::cerr<<"Use run_pc_oracle.py for hash verification.\n";return 2;}
    std::string only=argc>3?argv[3]:"all";
    setup_signals();auto file=load_file(argv[1]);map_pe(file);map_r047_game_control_stubs();map_r048_parent_stubs();map_r049_child_stubs();map_r050_slipstream_state();map_r051_parent_stubs();map_r052_event_scheduler();map_r054_event_work();map_r055_allocator_state();map_r056_object_helpers();map_r057_transition();map_r058_runtime();map_r059_ui();map_r060_factory_stubs();map_r062_constructor_stubs();map_r063_state_selectors();map_r064_object_state();map_r065_state_update();map_r066_ui_dispatch();map_r067_event_dispatch();map_r068_destructor_stubs();map_r069_runtime_init();map_r070_teardown();map_r071_event_open();map_r072_runtime_list();map_r073_runtime_prepare();map_r074_runtime_gate();map_r075_runtime_blockers();map_r076_course_runtime();map_r077_course_provider();map_r079_runtime_owner();map_r080_ui_tick();map_r081_embedded_slots();map_r084_runtime_queue();map_r086_event_function36();map_r087_bootstrap();map_r088_startup();map_r090_platform_init();map_r091_loop_cleanup();map_r092_loop_setup();map_r093_timing();map_r094_frame_step();set_r077_hash_patch(r077_oracle_name(only)||only=="all");set_r056_object_patches(r056_oracle_name(only));set_r057_transition_patch(r057_oracle_name(only));set_r058_runtime_patches(r058_oracle_name(only)||r065_oracle_name(only));set_r059_patches((r059_oracle_name(only)||r080_oracle_name(only)||r084_oracle_name(only))&&only!="all");set_r080_patches(r080_oracle_name(only)&&only!="all");set_r081_patches(r081_oracle_name(only)&&only!="all");set_r060_factory_patches(r060_oracle_name(only)&&only!="all");set_r061_factory_patches(r061_oracle_name(only)&&only!="all");set_r062_constructor_patches(r062_oracle_name(only)&&only!="all");set_r063_patches(r063_oracle_name(only)&&only!="all");set_r064_patches(r064_oracle_name(only)&&only!="all");set_r065_patches(r065_oracle_name(only)&&only!="all");set_r066_patches(r066_oracle_name(only)&&only!="all");set_r067_patches(r067_oracle_name(only)&&only!="all");set_r068_patches(r068_oracle_name(only)&&only!="all");set_r069_patch(r069_oracle_name(only)&&only!="all");set_r070_patches(r070_oracle_name(only)&&only!="all");set_r071_patches((r071_oracle_name(only)||r083_oracle_name(only)||r084_oracle_name(only))&&only!="all");set_r073_patches(r073_oracle_name(only)||only=="all");set_r074_patches(r074_oracle_name(only)||only=="all");set_r084_patches(r084_oracle_name(only)&&only!="all");set_r085_entry_patch(r085_oracle_name(only)||only=="all");if(only=="all")set_r048_parent_patches(false);map_vibrate_history();map_record_ghost_state();map_r043_session_state();map_r043_othcar_stubs();map_r044_platform_stubs();map_r045_course_service_state();map_r046_road_stubs();map_r046_common_parent_stubs();if(only=="all"||world_enabled(only)){set_r046_road_patches(false);set_r046_common_parent_patches(false);}map_bk_selector_stubs();map_rear_grip_volume_stubs();map_night_tunnel_stubs();map_driving_skill_stubs();map_chicken_driver_stubs();map_assist_chicken_stubs();map_handicap_stubs();map_reverse_car_stubs();map_ofs_left_lane_stubs();if(course_query_enabled(only)||world_enabled(only)||ground_enabled(only)||wall_enabled(only)||response_enabled(only)||rebound_enabled(only)||crash_enabled(only)||entry_enabled(only))map_original_arithmetic_bridge(file);map_contact_matrix_body();patch_maximum_velocity_contact_call();map_brake_block();map_direction_angles_block();map_auto_transmission_block();map_manual_transmission_block();map_road_mu_stubs();patch_full_driving_platform();if(only=="course_collision_ext_flags"||only=="course_collision_offset_direction"||only=="course_length"||only=="update_easy_lct_prediction"||only=="all")init_course_table_oracle();if(only=="update_easy_lct_prediction"||only=="all"){map_original_exec_page(file,0x0103c000u);if(only!="all")map_original_read_page(file,0x01039000u);}if(only=="get_y_position_spl_chk")map_original_read_page(file,0x01039000u);
    if(only=="race_sound_probe"||only=="start_fight_vm_probe"||only=="course_field_vm_probe"||only=="title_owner_vm_probe"||only=="car_services_probe"||only=="car_services_net_e2e_probe")map_original_arithmetic_bridge(file);
    Tables tables{{Bytes(reinterpret_cast<void*>(0x5e94d8),648),Bytes(reinterpret_cast<void*>(0x5e9760),648)},Bytes(reinterpret_cast<void*>(0x5e9df0),1024)};
    const unsigned count=argc>2?unsigned(std::stoul(argv[2])):2000;
    const bool need_d3dx=(only=="all"||only=="race_sound_probe"||only=="car_services_probe"||only=="car_services_net_e2e_probe"||only=="copy_car_work_4a1140_r037"||only=="set_car_camera_4a1680_r038"||only=="check_reverse_car_4a2910_r041"||only=="calc_light_rate_4a3d40_r042"||only=="check_wanderer_4a5260_r049"||only=="r049_game_children_batch"||(only=="get_road_ofs_4a4010_r046"||only=="r046_road_services_batch")||only=="runtime_course_matrix_44c0d0_r076"||only=="runtime_apply_course_data_44d720_r076"||only=="r076_course_runtime_batch"||only=="tire_velocity"||only=="tire_direction"||only=="driving_control"||only=="tire_load"||only=="contact_matrix"||only=="maximum_velocity"||only=="car_sus_coli_check"||only=="car_sus_bump_push"||only=="collision_suspension_chain"||world_enabled(only)||ground_enabled(only)||wall_enabled(only)||response_enabled(only)||rebound_enabled(only)||crash_enabled(only)||entry_enabled(only));
    if(need_d3dx){
        if(argc<=7||std::string(argv[7])=="-")throw std::runtime_error("tire-velocity oracle requires pinned d3dx9_29.dll path");
        map_original_read_page(file,0x01039000u);
        init_matrix_oracle_globals();wire_d3dx_imports(map_d3dx29(argv[7]));
    }
    if(only=="rank_provider_gateway_45a2b0_r049")map_original_read_page(file,0x01039000u);
    if(r074_oracle_name(only)&&only!="all")map_original_read_page(file,0x01039000u);
    if(spline_enabled(only)||course_query_enabled(only)||world_enabled(only)||ground_enabled(only)||wall_enabled(only))init_spline_oracle();
    if(world_enabled(only)||ground_enabled(only)||wall_enabled(only))init_world_oracle();
    if(wall_enabled(only))init_wall_oracle();
    if(response_enabled(only))init_response_oracle();
    if(rebound_enabled(only))init_rebound_oracle();
    if(crash_enabled(only))init_crash_oracle();
    if(entry_enabled(only))init_entry_oracle(only);
    if(argc>4)x87_control=std::uint32_t(std::stoul(argv[4],nullptr,0));
    if(x87_control!=0x037f&&x87_control!=0x027f&&x87_control!=0x007f)throw std::runtime_error("only x87 0x037f / 0x027f / 0x007f (Direct3D single precision) supported");
    outrun::driving::x87_set_control(std::uint16_t(x87_control)); // native X87 arithmetic follows the oracle control word
    #include "vehicle_menu_probe.inc"
    #include "vehicle_preview_probe.inc"
    #include "vehicle_constructor_probe.inc"
    #include "vehicle_body_probe.inc"
    #include "environment_blend_probe.inc"
    #include "vehicle_preview_init_probe.inc"
    #include "camera_probe.inc"
    #include "vehicle_model_draw_probe.inc"
    #include "race_model_draw_probe.inc"
    #include "bridge_snippet_probe.inc"
    #include "race_events_probe.inc"
    #include "race_input_probe.inc"
    #include "area_bridge_probe.inc"
    #include "race_area_probe.inc"
    #include "renderer_init_probe.inc"                  // renderer initialisation 404250
    #include "scene_effects_probe.inc"   // scn-efc
    #include "particles_probe.inc"   // scn-efc
    #include "race_robots_probe.inc"
    #include "race_area_services_probe.inc"   // race AREA/SKY runtime services
    #include "course_objects_probe.inc"         // stage-object lanes 4EFD20/4F0D10/4F0430
    #include "sprite_2d_flush_probe.inc"         // 2D queue flush 42D710 (42A3A0, 42A800 masks)
    #include "sprite_2d_sprani_probe.inc"        // SPRANI scene renderer 429460 (retail animations)
    #include "rob_motion_probe.inc"              // RobMotion tables 4F2470
    #include "rob_setbone_probe.inc"             // RobMotion SetBone 4F1CF0
    #include "rob_disp_probe.inc"                // rob_disp_init 514BF0 (513840, 514680)
    #include "object_db_probe.inc"               // object name database 448B90 / 448CD0
    #include "rob_osage_probe.inc"               // rob_osage_init 514F60 (osage chains)
    #include "rob_setloop_observe.inc"           // black box of the protected SetLoop 4F1E60
    #include "rob_motion_engine_probe.inc"       // RobMotion engine (SetMotion/SetFrame/Connect/Calc)
    #include "char_grip_probe.inc"               // 46BC30 GetHandleGripPositionLocal
    #include "rob_display_probe.inc"             // rob_disp_disp 514E60 (robot draw)
    #include "rob_flag_probe.inc"                // flagman flag cloth 402100 / 402140
    #include "rob_flag_draw_probe.inc"           // flagman flag cloth draw 401B80
    #include "shadow_alloc_observe.inc"          // protected 46BA20 observation
    #include "shadow_volume_probe.inc"           // car shadow volumes 46BA20..422740
    #include "sprite_2d_image_probe.inc"         // 2D image producers 42D280 / 42D300 / 42D5F0
    #include "render_queue_probe.inc"
    #include "pmt_loader_probe.inc"
    #include "render_flush_probe.inc"
    #include "scene_display_probe.inc"
    #include "environment_render_probe.inc"
    #include "car_reflection_probe.inc"            // car reflection cube 413B30 / 414340 / 414050 / 40A6D0
    #include "frame_render_probe.inc"
    #include "scene_environment_probe.inc"
    #include "bridge_observe_probe.inc"
    #include "race_manager_probe.inc"
    #include "race_sound_probe.inc"                     // race SOUND (event 383) / COMM_TRANS (360)
    #include "transmission_probe.inc"
    #include "music_probe.inc"
    #include "rankings_probe.inc"
    #include "ghost_start_probe.inc"
    #include "ghost_car_probe.inc"
    #include "ghost_draw_probe.inc"
    #include "race_end_probe.inc"
    #include "arcade_probe.inc"
    #include "name_entry_probe.inc"
    #include "autoscene_table_observe.inc"
    #include "autoscene_vm_observe.inc"
    #include "autoscene_probe.inc"
    #include "ending_probe.inc"
    #include "ending_model_draw_probe.inc"
    #include "select_model_draw_probe.inc"
    #include "ending_services_probe.inc"
    #include "race_records_probe.inc"
    #include "record_car_probe.inc"
    #include "race_traffic_probe.inc"
    #include "race_objects_probe.inc"
    #include "sp_rankings_probe.inc"
    #include "goal_camera_probe.inc"
    #include "mission_manager_probe.inc"
    #include "racer_setup_probe.inc"
    #include "scene_owner_probe.inc"
    #include "race_hud_probe.inc"
    #include "car_services_probe.inc"
    #include "car_services_verify.inc"
    #include "car_services_ghost_probe.inc"
    #include "car_services_net_probe.inc"
    if(only=="frontend_vehicle_loader_probe"){
        using namespace outrun::platform;
        constexpr unsigned base=0x3a800000,trace=base+0x1000,readiness=base+0x4000;
        map_at(base,0x10000,PROT_READ|PROT_WRITE|PROT_EXEC);
        map_original_arithmetic_bridge(file);
        if(mprotect(reinterpret_cast<void*>(0x7c2000),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("vehicle license fixture");
        // Only platform resource leaves are substituted. The full scheduling,
        // eviction, flush and model-table routines execute original x86 code.
        unsigned stub_slot{};
        auto capture=[&](unsigned pc,bool two_args,bool is_ready){
            std::vector<std::uint8_t> c{0x8b,0x15};
            auto imm=[&](unsigned v){for(unsigned i=0;i<4;++i)c.push_back(std::uint8_t(v>>(8*i)));};
            imm(trace);c.insert(c.end(),{0xc1,0xe2,4,0xc7,0x82});imm(trace+16);imm(pc);
            c.insert(c.end(),{0x8b,0x44,0x24,4,0x89,0x82});imm(trace+20);
            if(two_args)c.insert(c.end(),{0x8b,0x4c,0x24,8});else c.insert(c.end(),{0x31,0xc9});
            c.insert(c.end(),{0x89,0x8a});imm(trace+24);
            c.insert(c.end(),{0xff,0x05});imm(trace);
            if(is_ready){c.insert(c.end(),{0x8b,0x04,0x85});imm(readiness);}
            c.push_back(0xc3);
            const unsigned destination=base+0x8000+256*stub_slot++;
            r060_raw_write(destination,c.data(),c.size());
            std::array<std::uint8_t,5> jump{{0xe9,0,0,0,0}};
            const auto delta=destination-pc-5;std::memcpy(jump.data()+1,&delta,4);
            r060_raw_write(pc,jump.data(),jump.size());
        };
        capture(0x448ad0,true,false);capture(0x448990,false,false);capture(0x448960,false,true);
        struct Io {std::vector<std::array<unsigned,4>> calls;std::array<unsigned,0x223> ready{};} io;
        FrontendVehicleLoaderServices api{&io,
            [](void* p,unsigned id,unsigned mode){static_cast<Io*>(p)->calls.push_back({0x448ad0,id,mode,0});return true;},
            [](void* p,unsigned id,bool& ready){auto& io=*static_cast<Io*>(p);io.calls.push_back({0x448960,id,0,0});ready=io.ready[id]!=0;return true;},
            [](void* p,unsigned id){static_cast<Io*>(p)->calls.push_back({0x448990,id,0,0});return true;}};
        unsigned cases=0,mismatches=0;const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
        auto check=[&](bool ok,const char* label){++cases;if(!ok){++mismatches;if(mismatches<12)std::cerr<<"vehicle loader "<<cases<<" "<<label<<"\n";}};
        auto compare=[&](FrontendVehicleLoader& s,unsigned entry){
            std::memcpy(reinterpret_cast<void*>(base),s.object.data(),s.object.size());
            std::memset(reinterpret_cast<void*>(trace),0,0x1000);
            std::memcpy(reinterpret_cast<void*>(readiness),io.ready.data(),sizeof(io.ready));io.calls.clear();
            prepare(entry);guest_call.ecx=base;run_original32();
            check(entry==0x48bf20?frontend_vehicle_loader_init_48bf20(s,api):frontend_vehicle_loader_tick_48bfe0(s,api),"native call");
            check(guest_call.out_sp==S,"cdecl stack");
            if(inject&&cases==2)s.object[0x18]^=1;
            check(!std::memcmp(s.object.data(),reinterpret_cast<void*>(base),s.object.size()),"whole object");
            check(io.calls.size()==*reinterpret_cast<unsigned*>(trace)&&
                !std::memcmp(io.calls.data(),reinterpret_cast<void*>(trace+16),16*io.calls.size()),"ordered resource calls");
        };
        for(unsigned fill:{0u,0x5au,0xffu}){FrontendVehicleLoader s;s.object.fill(std::uint8_t(fill));compare(s,0x48bf20);}
        // Exhaust states for all three slots, mixed ready results and flush,
        // with permutations, duplicate desired models and sentinel entries.
        for(unsigned states=0;states<64;++states)for(unsigned ready=0;ready<8;++ready)
        for(unsigned flush=0;flush<2;++flush)for(unsigned target=0;target<8;++target){
            FrontendVehicleLoader s;s.object.fill(0x5a);Bytes b(s.object.data(),s.object.size());b.put8(0,std::uint8_t(flush));
            for(unsigned slot=0;slot<3;++slot){const auto state=(states>>(2*slot))&3;
                b.put32(0x10+12*slot,11+slot);b.put32(0x14+12*slot,state?slot:30);b.put32(0x18+12*slot,state);
                const unsigned desired=target<3?(target+slot)%3:target==3?slot+3:target==4?0:target==5?30:target==6?(slot==0?3:30):(slot?slot:3);
                b.put32(4+4*slot,desired);io.ready[VehicleResourceIds[slot]]=(ready>>slot)&1;
            }
            compare(s,0x48bfe0);
        }
        // Retained state across full forward/backward menu sweeps, loading
        // delays and flushes. Never reset to a convenient empty slot per frame.
        FrontendVehicleLoader live;compare(live,0x48bf20);Bytes lb(live.object.data(),live.object.size());
        for(unsigned frame=0;frame<2400;++frame){
            const auto menu=frame<1200?(frame/9)%30:29-(frame/7)%30;lb.put8(0,frame%101<4);
            if(frame%3==0)for(unsigned j=0;j<3;++j)lb.put32(4+4*j,VehicleMenuModels[(menu+j)%30]);
            for(unsigned id:VehicleResourceIds)io.ready[id]=(frame+id)%7>1;
            compare(live,0x48bfe0);
            for(unsigned model=0;model<=30;++model){
                prepare(0x48bf80);guest_call.ecx=base;*reinterpret_cast<unsigned*>(S)=model;run_original32();
                check(bool(guest_call.out_eax&255)==frontend_vehicle_loader_ready_48bf80(live,model),"ready query");
            }
            prepare(0x48bfc0);guest_call.ecx=base;run_original32();
            check(bool(guest_call.out_eax&255)==frontend_vehicle_loader_empty_48bfc0(live),"empty query");
        }
        for(unsigned model=0;model<30;++model){prepare(0x46bbe0);*reinterpret_cast<unsigned*>(S)=model;run_original32();unsigned id{};
            check(vehicle_resource_46bbe0(model,id)&&id==guest_call.out_eax,"original model mapping");}
        for(unsigned mask=0;mask<170;++mask){
            PcLicense profile{};
            if(mask<138)profile[0x28+mask/8]=std::uint8_t(1u<<(mask%8));
            else if(mask==139)profile.fill(255);
            else if(mask>139)for(unsigned i=0;i<profile.size();++i)profile[i]=std::uint8_t((mask*31+i*17)^(i>>2));
            std::memcpy(reinterpret_cast<void*>(0x7c23e0),profile.data(),profile.size());
            for(unsigned index=0;index<30;++index){
                prepare(0x4c8f90);*reinterpret_cast<unsigned*>(S)=index;run_original32();
                check(bool(guest_call.out_eax&255)==vehicle_unlocked_4c8f90(profile,index),"original car unlock");
                for(unsigned colour=0;colour<8;++colour){
                    prepare(0x4c8fc0);*reinterpret_cast<unsigned*>(S)=index;*reinterpret_cast<unsigned*>(S+4)=colour;run_original32();
                    check(bool(guest_call.out_eax&255)==vehicle_colour_unlocked_4c8fc0(profile,index,colour),"original colour unlock");
                }
            }
        }
        std::cout<<"{\"routine\":\"frontend_vehicle_loader_probe\",\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}\n";return mismatches?1:0;
    }
    if(only=="frontend_record_manager_probe"){
        using namespace outrun::platform;
        constexpr unsigned base=0x3a800000,trace=base+0x22000,virt=base+0x23000,object=base+0x24000;
        map_at(base,0x30000,PROT_READ|PROT_WRITE|PROT_EXEC);map_original_arithmetic_bridge(file);
        if(mprotect(reinterpret_cast<void*>(0x98a000),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("record request globals");
        auto capture=[&](unsigned pc,unsigned id,unsigned pop){
            std::vector<std::uint8_t> code{0x8b,0x15};auto imm=[&](unsigned v){for(unsigned i=0;i<4;++i)code.push_back(std::uint8_t(v>>(8*i)));};
            imm(trace);code.insert(code.end(),{0xc1,0xe2,4,0xc7,0x82});imm(trace+16);imm(id);
            code.insert(code.end(),{0x89,0x8a});imm(trace+20);
            if(pop)code.insert(code.end(),{0x8b,0x44,0x24,4});else code.insert(code.end(),{0x31,0xc0});
            code.insert(code.end(),{0x89,0x82});imm(trace+24);code.insert(code.end(),{0xff,0x05});imm(trace);
            code.insert(code.end(),{0xc2,std::uint8_t(pop),0});r060_raw_write(pc,code.data(),code.size());
        };
        capture(0x525f80,0x525f80,0);capture(virt,0,4);
        *reinterpret_cast<unsigned*>(object)=object+0x80;*reinterpret_cast<unsigned*>(object+0x80)=virt;
        for(auto field:std::array<std::pair<unsigned,unsigned>,3>{{{0x435f70,6},{0x435f7e,7},{0x435fbf,7}}}){
            auto [address,size]=field;std::array<std::uint8_t,7> code{};std::memcpy(code.data(),reinterpret_cast<void*>(address),size);
            code[0]=0x90;const unsigned tls=base+0x25000;std::memcpy(code.data()+size-4,&tls,4);r060_raw_write(address,code.data(),size);
        }
        struct Trace {std::vector<std::array<unsigned,4>> calls;};
        auto release=[](void* p,unsigned pc,unsigned token,unsigned arg){static_cast<Trace*>(p)->calls.push_back({pc,token,arg,0});return true;};
        unsigned cases=0,mismatches=0;const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
        auto check=[&](bool ok,const char* label){++cases;if(!ok||(inject&&cases==1)){++mismatches;if(mismatches<12)std::cerr<<"record manager "<<cases<<" "<<label<<"\n";}};
        for(unsigned seed:{0u,0x5au,0xffu}){
            std::array<std::uint8_t,PcRecordRequestBytes> query;query.fill(std::uint8_t(seed));
            std::memcpy(reinterpret_cast<void*>(0x98a708),query.data(),query.size());std::memset(reinterpret_cast<void*>(trace),0,256);
            prepare(0x435f70);*reinterpret_cast<unsigned*>(S)=0x98a708;run_original32();unsigned missing{};
            check(record_request_construct_435f70(Bytes(query.data(),query.size()),missing),"constructor native");
            for(unsigned i=0;i<query.size();++i)if(query[i]!=*reinterpret_cast<std::uint8_t*>(0x98a708+i)&&mismatches<3)std::cerr<<"constructor difference +"<<std::hex<<i<<std::dec<<"\n";
            check(!std::memcmp(query.data(),reinterpret_cast<void*>(0x98a708),query.size()),"constructor whole state");
        }
        for(unsigned mask=0;mask<128;++mask)for(unsigned seed:{0u,0x5au,0xffu}){
            std::array<std::uint8_t,PcRecordManagerBytes> records;std::array<std::uint8_t,PcRecordRequestBytes> query;
            for(unsigned i=0;i<records.size();++i)records[i]=std::uint8_t(seed+i*17);
            query.fill(std::uint8_t(seed));Bytes qb(query.data(),query.size());unsigned bit=0;
            for(unsigned offset:{0x24u,0x28u,0x2cu,0x18u,0x1cu,0x20u,0x30u}){
                qb.put32(offset,(mask&(1u<<bit))?(offset==0x30?object:object+0x100+bit*16):0);++bit;
            }
            std::memcpy(reinterpret_cast<void*>(base),records.data(),records.size());
            std::memcpy(reinterpret_cast<void*>(0x98a708),query.data(),query.size());std::memset(reinterpret_cast<void*>(trace),0,256);
            prepare(0x4940d0);guest_call.ecx=base;run_original32();Trace calls;unsigned missing{};
            check(record_manager_reset_4940d0(Bytes(records.data(),records.size()),qb,{&calls,release},missing),"reset native");
            check(!std::memcmp(records.data(),reinterpret_cast<void*>(base),records.size()),"whole records state");
            check(!std::memcmp(query.data(),reinterpret_cast<void*>(0x98a708),query.size()),"whole query state");
            check(calls.calls.size()==*reinterpret_cast<unsigned*>(trace)&&!std::memcmp(calls.calls.data(),reinterpret_cast<void*>(trace+16),calls.calls.size()*16),"ordered releases including virtual deleting flag");
        }
        std::cout<<"{\"routine\":\"frontend_record_manager_probe\",\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}\n";return mismatches?1:0;
    }
    if(only=="frontend_fixed_choice_probe"){
        using namespace outrun::platform;
        constexpr unsigned base=0x3a900000,root=base+0x1000,trace=base+0x3000;
        map_at(base,0x10000,PROT_READ|PROT_WRITE|PROT_EXEC);map_original_arithmetic_bridge(file);
        if(mprotect(reinterpret_cast<void*>(0x842000),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("choice timer");
        auto stub=[](unsigned pc,unsigned value,unsigned short pop=0){std::array<std::uint8_t,8> code{0xb8,0,0,0,0,0xc2,0,0};
            std::memcpy(code.data()+1,&value,4);std::memcpy(code.data()+6,&pop,2);r060_raw_write(pc,code.data(),code.size());};
        // Keep both original controller bodies and the entire 51B7B0..51BF06
        // widget code. Replace only resource/device/command/manager leaves.
        stub(0x4035f0,root);stub(0x465250,0);stub(0x465970,0);stub(0x4659f0,0);stub(0x4652e0,1);
        stub(0x447000,0,8);stub(0x440ea0,0,32);stub(0x4249f0,0);
        for(unsigned pc:{0x4940d0u,0x4165c0u,0x4f3cc0u})stub(pc,1);
        std::vector<std::uint8_t> capture;
        auto imm=[&](unsigned v){for(unsigned i=0;i<4;++i)capture.push_back(std::uint8_t(v>>(8*i)));};
        for(unsigned i=0;i<11;++i){capture.insert(capture.end(),{0x8b,0x44,0x24,std::uint8_t(4+i*4),0xa3});imm(trace+4*i);}
        capture.insert(capture.end(),{0xc2,44,0});r060_raw_write(0x465860,capture.data(),capture.size());
        unsigned cases=0,mismatches=0;const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
        auto check=[&](bool ok,const char* label){++cases;if(!ok||(inject&&cases==1)){++mismatches;if(mismatches<16)std::cerr<<"fixed choice "<<cases<<" "<<label<<"\n";}};
        for(unsigned key:{1u,2u})for(unsigned flags=0;flags<256;flags+=4){
            std::array<std::uint8_t,0x154> child;child.fill(0x5a);unsigned repeat{};
            check(fixed_choice_construct(child.data(),child.size(),key,repeat),"constructor fixture");
            std::array<std::uint8_t,0xe00> owner{};Bytes nb(child.data(),child.size()),rb(owner.data(),owner.size());
            rb.put32(0x20c,flags|0x8000);std::memcpy(reinterpret_cast<void*>(base),child.data(),child.size());
            std::memcpy(reinterpret_cast<void*>(root),owner.data(),owner.size());*reinterpret_cast<float*>(0x842110)=3.5f;
            prepare(key==1?0x4c5450:0x4c58b0);guest_call.ecx=base;run_original32();
            FrontendSprites sprites;check(sprites.bind_timing(0x44,std::vector<FrontendSpriteTiming>(224,{120,60})),"timing fixture");
            FrontendUiResources ui{sprites};PcUiNotifyGlobals globals;unsigned result{};
            check(ui.commands(0x442ac0,rb.sub(0x51c,owner.size()-0x51c),nullptr,0,globals,result),"commands constructor");
            FrontendInputSnapshot input;FixedChoiceServices services{ui,rb,globals,input,repeat,3.5f};
            services.external=[](void*,unsigned){return true;};ui.effect_4249f0=[](void*,unsigned){return true;};
            check(fixed_choice_init(nb,key,services),"native init");
            check(!std::memcmp(child.data(),reinterpret_cast<void*>(base),child.size()),"whole initialized child");
            Bytes pcroot(reinterpret_cast<void*>(root),owner.size());
            check(rb.u32(0x20c)==pcroot.u32(0x20c)&&rb.u32(0x218)==pcroot.u32(0x218)&&rb.u32(0x21c)==pcroot.u32(0x21c),"root init fields");
        }
        for(unsigned key:{1u,2u})for(unsigned choice=0;choice<5;++choice)for(unsigned action:{0u,1u,2u,3u,4u,5u,12u})for(unsigned latched:{0u,1u})for(unsigned motion=0;motion<4;++motion){
            std::array<std::uint8_t,0x154> child{};std::array<std::uint8_t,0xe00> owner{};unsigned repeat{},result{};
            Bytes nb(child.data(),child.size()),rb(owner.data(),owner.size());
            FrontendSprites sprites;sprites.bind_timing(0x44,std::vector<FrontendSpriteTiming>(224,{120,60}));
            FrontendUiResources ui{sprites};PcUiNotifyGlobals globals;FrontendInputSnapshot input;
            ui.commands(0x442ac0,rb.sub(0x51c,owner.size()-0x51c),nullptr,0,globals,result);
            FixedChoiceServices services{ui,rb,globals,input,repeat,3.5f};
            services.external=[](void*,unsigned){return true;};ui.effect_4249f0=[](void*,unsigned){return true;};
            fixed_choice_construct(child.data(),child.size(),key,repeat);fixed_choice_init(nb,key,services);
            rb.put32(0x218,0);rb.put32(0x20c,0xabcdef);rb.put32(4,0x21);nb.put32(4,0xdead);
            nb.put32(0xe8,choice);nb.put8(0x14c,std::uint8_t(latched));nb.put32(0x150,0x12);
            auto widget=nb.sub(0x34,0x118);widget.put32(0x104,motion&1);widget.put32(0x108,motion>>1);
            for(unsigned current:{0xbcu,0xe0u})for(unsigned axis=0;axis<3;++axis){
                widget.putf(current+4*axis,float(axis)+0.25f);
                widget.putf(current+12+4*axis,float(axis)+(axis==1?-0.5f:0.5f));
                widget.putf(current+24+4*axis,axis==1?-1.25f:axis==2?0.0f:1.25f);
            }
            std::memcpy(reinterpret_cast<void*>(base),child.data(),child.size());std::memcpy(reinterpret_cast<void*>(root),owner.data(),owner.size());
            std::memset(reinterpret_cast<void*>(trace),0,44);*reinterpret_cast<float*>(0x842110)=4.0f;
            stub(0x48f5f0,action,4);prepare(key==1?0x4c5620:0x4c5a50);guest_call.ecx=base;run_original32();
            constexpr unsigned masks[]{4,8,0x400,0x1000,0x800,0x2000};input.feature_mask=action<6?masks[action]:0;
            services.timer=4.0f;check(fixed_choice_tick(child.data(),child.size(),key,services,result),"native tick");
            // GPU resource bytes and input-repeat bytes intentionally differ:
            // native executes those services, original receives capture leaves.
            check(nb.u32(4)==*reinterpret_cast<unsigned*>(base+4),"next child key");
            check(!std::memcmp(child.data()+0xd8,reinterpret_cast<void*>(base+0xd8),0x154-0xd8),"widget/controller tail");
            Bytes pcroot(reinterpret_cast<void*>(root),owner.size());
            check(rb.u32(4)==pcroot.u32(4)&&rb.u32(0x20c)==pcroot.u32(0x20c),"mode bits and parent selection");
            check(result==guest_call.out_eax,"latched return");
            if(!latched&&action!=0&&action!=1){
                const auto* image=sprites.get(nb.u32(0x40));Bytes args(reinterpret_cast<void*>(trace),44);
                if(image&&(image->first!=float(args.i32(4))||image->last!=float(args.i32(8))))
                    std::cerr<<"range key="<<key<<" choice="<<choice<<" action="<<action<<" native="<<image->first<<","<<image->last<<" pc="<<args.i32(4)<<","<<args.i32(8)<<"\n";
                check(image&&image->allocated&&image->token==args.u32(0)&&image->first==float(args.i32(4))&&
                    image->last==float(args.i32(8))&&image->mode==(args.u32(16)==3?1u:args.u32(16))&&
                    nb.u32(0x54)==(args.u32(16)==3?1u:0u)&&image->speed==args.f32(36),"original table/animation range/speed");
            }
        }
        std::cout<<"{\"routine\":\"frontend_fixed_choice_probe\",\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}\n";return mismatches?1:0;
    }
    if(only=="title_menu_control_probe"){
        using namespace outrun::platform;
        constexpr unsigned base=0x3ae00000,root=base+0x3000,manager=base+0x4000,modeobject=base+0x5000,vt=base+0x6000;
        map_at(base,0x10000,PROT_READ|PROT_WRITE|PROT_EXEC);map_original_arithmetic_bridge(file);
        for(unsigned page:{0x692000u,0x780000u,0x7d6000u})if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("title menu globals");
        auto stub=[](unsigned pc,unsigned value,unsigned short pop=0){std::array<std::uint8_t,8> code{0xb8,0,0,0,0,0xc2,0,0};
            std::memcpy(code.data()+1,&value,4);std::memcpy(code.data()+6,&pop,2);r060_raw_write(pc,code.data(),code.size());};
        stub(0x4035f0,root);
        constexpr unsigned traces=base+0x7000;
        auto capture=[&](unsigned pc,unsigned id,unsigned response,unsigned pop,bool arg){
            std::vector<std::uint8_t> code{0x8b,0x15};
            auto imm=[&](unsigned v){for(unsigned i=0;i<4;++i)code.push_back(std::uint8_t(v>>(8*i)));};
            imm(traces);code.insert(code.end(),{0xc1,0xe2,4,0xc7,0x82});imm(traces+16);imm(id);
            code.insert(code.end(),{0x89,0x8a});imm(traces+20);
            if(arg)code.insert(code.end(),{0x8b,0x44,0x24,4});else code.insert(code.end(),{0x31,0xc0});
            code.insert(code.end(),{0x89,0x82});imm(traces+24);code.insert(code.end(),{0xff,0x05});imm(traces);
            code.push_back(0xb8);imm(response);code.insert(code.end(),{0xc2,std::uint8_t(pop),std::uint8_t(pop>>8)});
            r060_raw_write(pc,code.data(),code.size());
        };
        capture(0x48dda0,0x48dda0,0,0,false);capture(0x48dc10,0x48dc10,0,4,true);capture(0x48dc60,0x48dc60,0,4,true);
        *reinterpret_cast<unsigned*>(vt+0x14)=vt+0x100;*reinterpret_cast<unsigned*>(vt+0x10)=vt+0x200;
        unsigned cases=0,mismatches=0;const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
        auto check=[&](bool ok,const char* label){++cases;if(!ok||(inject&&cases==1)){++mismatches;if(mismatches<12)std::cerr<<"title menu "<<cases<<" "<<label<<"\n";}};
        struct Calls{std::array<std::array<unsigned,4>,8> values{};unsigned count{},input{};};
        const float delays[]{-1,0,.5f,1,1.5f,15.05f,std::numeric_limits<float>::quiet_NaN()};
        for(unsigned state:{2u,3u,4u})for(unsigned variant:{1u,4u})for(unsigned mgr:{0u,6u,7u})
        for(unsigned selection=0;selection<(state==2?4u:state==3?(variant==4&&(mgr==0||mgr==7)?5u:6u):3u);++selection)
        for(unsigned action=0;action<14;++action)for(unsigned feature:{0u,1u})for(float delay:delays){
            std::array<std::uint8_t,TitleOwnerPcSize> native;native.fill(0x5a);Bytes nb(native.data(),native.size());
            nb.put32(0,vt);nb.put32(0x34,selection);nb.put32(0x9ac,1);std::memcpy(reinterpret_cast<void*>(base),native.data(),native.size());
            std::memset(reinterpret_cast<void*>(traces),0,256);capture(vt+0x100,0x48f5f0,action,4,true);stub(0x4536f0,feature);
            Bytes(reinterpret_cast<void*>(root),0x300).put32(0x218,state);
            *reinterpret_cast<unsigned*>(0x780258)=variant;*reinterpret_cast<float*>(0x692c9c)=delay;
            *reinterpret_cast<unsigned*>(0x7d68ac)=mgr?manager:0;*reinterpret_cast<unsigned*>(manager+0x5c)=modeobject;*reinterpret_cast<unsigned*>(modeobject+8)=mgr;
            prepare(0x4d7300);guest_call.ecx=base;run_original32();
            TitleMenuGlobals globals{state,variant,mgr!=0,mgr,feature,delay};Calls calls;calls.input=action;unsigned result{};
            const auto call=[](void* p,unsigned pc,std::uint8_t*,std::size_t off,int arg,unsigned& r){
                auto& c=*static_cast<Calls*>(p);c.values[c.count++]={pc,base+unsigned(off),unsigned(arg),0};r=pc==0x48f5f0?c.input:0;return true;};
            check(title_menu_control_4d7300(native.data(),native.size(),globals,{&calls,call,state},result),"native services");
            check(!std::memcmp(native.data(),reinterpret_cast<void*>(base),native.size()),"whole parent object");
            check(!std::memcmp(&globals.delay_692c9c,reinterpret_cast<void*>(0x692c9c),4),"original cooldown");
            check(result==guest_call.out_eax,"return");
            check(calls.count==*reinterpret_cast<unsigned*>(traces)&&!std::memcmp(calls.values.data(),reinterpret_cast<void*>(traces+16),calls.count*16),"ordered service calls");
        }
        capture(0x48e440,0x48e440,0,0,false);capture(vt+0x200,16,0,0,false);
        for(unsigned seed=0;seed<64;++seed){
            std::array<std::uint8_t,TitleOwnerPcSize> native;native.fill(std::uint8_t(seed));Bytes nb(native.data(),native.size());nb.put32(0x9bc,vt);
            std::memcpy(reinterpret_cast<void*>(base),native.data(),native.size());std::memset(reinterpret_cast<void*>(traces),0,256);
            prepare(0x4d6090);guest_call.ecx=base;run_original32();Calls calls;
            check(title_menu_close_4d6090(native.data(),native.size(),{&calls,[](void* p,unsigned pc,std::uint8_t*,std::size_t off,int arg,unsigned& r){
                auto& c=*static_cast<Calls*>(p);c.values[c.count++]={pc,base+unsigned(off),unsigned(arg),0};r=0;return true;},2}),"close native services");
            check(!std::memcmp(native.data(),reinterpret_cast<void*>(base),native.size()),"close whole parent");
            check(calls.count==*reinterpret_cast<unsigned*>(traces)&&!std::memcmp(calls.values.data(),reinterpret_cast<void*>(traces+16),calls.count*16),"close ordered clear/suspend");
        }
        std::cout<<"{\"routine\":\"title_menu_control_probe\",\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}\n";return mismatches?1:0;
    }
    // Options > Settings / Controls / Audio controls 4D86F0 / 4D8890 / 4D89B0
    // (with 4D76F0 / 4D7AB0 and the 446010 / 446050 / 4469F0 / 446A20 /
    // 446080 / 446420 leaves executed as original code), and the widget
    // initialisers 445FE0 / 446100 / 446340 / 446430. The 4D86F0 protected
    // bridge runs unchanged. Captured leaves: list 48DDA0/48DC10/48DC60, input
    // virtual, slider 4469C0/446440, 4249F0, 42EFA0, 42FC90 (cdecl: ecx 0).
    if(only=="title_options_probe"){
        using namespace outrun::platform;
        constexpr unsigned base=0x3ae00000,root=base+0x3000,vt=base+0x6000,traces=base+0x7000,license=0x7c23e0;
        map_at(base,0x10000,PROT_READ|PROT_WRITE|PROT_EXEC);map_original_arithmetic_bridge(file);
        for(unsigned page:{0x7c2000u})if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("license page");
        auto imm=[](std::vector<std::uint8_t>& c,unsigned v){for(unsigned i=0;i<4;++i)c.push_back(std::uint8_t(v>>(8*i)));};
        auto stub=[&](unsigned pc,unsigned value){std::vector<std::uint8_t> c{0xb8};imm(c,value);c.push_back(0xc3);r060_raw_write(pc,c.data(),c.size());};
        stub(0x4035f0,root);
        auto capture=[&](unsigned pc,unsigned id,unsigned response,unsigned pop,bool arg,bool this_call){
            std::vector<std::uint8_t> code{0x8b,0x15};imm(code,traces);code.insert(code.end(),{0xc1,0xe2,4,0xc7,0x82});imm(code,traces+16);imm(code,id);
            if(this_call){code.insert(code.end(),{0x89,0x8a});imm(code,traces+20);}
            else{code.insert(code.end(),{0xc7,0x82});imm(code,traces+20);imm(code,0);}
            if(arg)code.insert(code.end(),{0x8b,0x44,0x24,4});else code.insert(code.end(),{0x31,0xc0});
            code.insert(code.end(),{0x89,0x82});imm(code,traces+24);code.insert(code.end(),{0xff,0x05});imm(code,traces);
            code.push_back(0xb8);imm(code,response);code.insert(code.end(),{0xc2,std::uint8_t(pop),std::uint8_t(pop>>8)});
            r060_raw_write(pc,code.data(),code.size());
        };
        capture(0x48dda0,0x48dda0,0,0,false,true);capture(0x48dc10,0x48dc10,0,4,true,true);capture(0x48dc60,0x48dc60,0,4,true,true);
        capture(0x4469c0,0x4469c0,0,0,false,true);capture(0x446440,0x446440,0,0,false,true);
        capture(0x4249f0,0x4249f0,0,0,true,false);capture(0x42efa0,0x42efa0,0,0,true,false);capture(0x42fc90,0x42fc90,0,0,true,false);
        *reinterpret_cast<unsigned*>(vt+0x14)=vt+0x100;
        unsigned cases=0,mismatches=0;const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
        auto check=[&](bool ok,const char* label){++cases;if(!ok||(inject&&cases==1)){++mismatches;if(mismatches<12)std::cerr<<"title options "<<cases<<" "<<label<<"\n";}};
        std::mt19937 rng(0x4d86f0u);auto rnd=[&](unsigned n){return unsigned(rng()%n);};
        struct Calls{std::array<std::array<unsigned,4>,16> values{};unsigned count{},input{};};
        // Owner widgets around their live ranges: options (first, last, current,
        // wrap), sliders (low, value, range), the selected list row and stage words.
        auto fixture=[&](Bytes nb){
            for(std::size_t i=0;i<nb.size();++i)nb.put8(i,std::uint8_t(rng()));
            nb.put32(0,vt);nb.puti(0x34,int(rnd(7))-1);
            for(unsigned off:{0x6dcu,0x6f4u,0x70cu,0x724u,0x97cu}){const int first=int(rnd(40)),last=first+int(rnd(6));
                nb.puti(off,first);nb.puti(off+4,last);nb.puti(off+8,first+int(rnd(unsigned(last-first)+3))-1);nb.puti(off+0xc,last-first);nb.put8(off+0x10,std::uint8_t(rnd(2)));}
            for(unsigned off:{0x73cu,0x7fcu,0x8bcu}){const int range=int(rnd(12));nb.puti(off+0xa4,int(rnd(3)));nb.puti(off+0xb0,range);
                nb.puti(off+0xac,rnd(16)==0?int(rng()):int(rnd(unsigned(range)+3))-1);}
        };
        for(unsigned pc:{0x4d86f0u,0x4d8890u,0x4d89b0u})for(unsigned state:{2u,3u,4u})for(unsigned action=0;action<14;++action)
        for(unsigned feature:{0u,1u})for(unsigned rep=0;rep<48;++rep){
            std::vector<std::uint8_t> native(TitleOwnerPcSize);Bytes nb(native.data(),native.size());fixture(nb);
            std::vector<std::uint8_t> lic(PcLicenseBytes);for(auto& b:lic)b=std::uint8_t(rng());
            std::memcpy(reinterpret_cast<void*>(base),native.data(),native.size());std::memcpy(reinterpret_cast<void*>(license),lic.data(),lic.size());
            std::memset(reinterpret_cast<void*>(traces),0,0x200);capture(vt+0x100,0x48f5f0,action,4,true,true);stub(0x4536f0,feature);
            Bytes(reinterpret_cast<void*>(root),0x300).put32(0x218,state);
            prepare(pc);guest_call.ecx=base;run_original32();
            TitleMenuGlobals globals{state,1,false,0,feature,0};Calls calls;calls.input=action;
            const auto call=[](void* p,unsigned id,std::uint8_t*,std::size_t off,int arg,unsigned& r){
                auto& c=*static_cast<Calls*>(p);if(c.count>=c.values.size())return false;
                const bool this_call=id!=0x4249f0&&id!=0x42efa0&&id!=0x42fc90;
                c.values[c.count++]={id,this_call?base+unsigned(off):0u,unsigned(arg),0};r=id==0x48f5f0?c.input:0;return true;};
            const TitleControllerServices services{&calls,call,state};Bytes lb(lic.data(),lic.size());
            const bool ok=pc==0x4d86f0?title_settings_control_4d86f0(native.data(),native.size(),lb,globals,services):
                          pc==0x4d8890?title_controls_control_4d8890(native.data(),native.size(),lb,globals,services):
                          title_audio_control_4d89b0(native.data(),native.size(),lb,globals,services);
            check(ok,"native services");
            check(!std::memcmp(native.data(),reinterpret_cast<void*>(base),native.size()),"whole owner");
            check(!std::memcmp(lic.data(),reinterpret_cast<void*>(license),lic.size()),"license 7C23E0");
            check(guest_call.out_eax==0,"return 0");
            const unsigned pc_calls=*reinterpret_cast<unsigned*>(traces);
            check(calls.count==pc_calls&&!std::memcmp(calls.values.data(),reinterpret_cast<void*>(traces+16),calls.count*16),"ordered service calls");
            if(mismatches&&mismatches<4&&(calls.count!=pc_calls||std::memcmp(calls.values.data(),reinterpret_cast<void*>(traces+16),calls.count*16))){
                std::cerr<<std::hex<<"pc="<<pc<<" state="<<state<<" action="<<action<<" sel="<<nb.i32(0x34)<<" native:";
                for(unsigned i=0;i<calls.count;++i)std::cerr<<" "<<calls.values[i][0]<<"/"<<calls.values[i][1]<<"/"<<calls.values[i][2];
                std::cerr<<" pc:";for(unsigned i=0;i<pc_calls&&i<16;++i){const auto* v=reinterpret_cast<unsigned*>(traces+16+i*16);std::cerr<<" "<<v[0]<<"/"<<v[1]<<"/"<<v[2];}
                std::cerr<<std::dec<<"\n";}
        }
        // Initialisers: 445FE0(first, last, wrap, layer), 446100(index),
        // 446340(table, low, high, x, y, layer), 446430(value).
        for(unsigned rep=0;rep<4096;++rep){
            std::array<std::uint8_t,0xc0> native{};for(auto& b:native)b=std::uint8_t(rng());Bytes nb(native.data(),native.size());
            const unsigned a=rng()%64,b=a+rng()%8,layer=rng(),wrap=rng();const int index=int(rnd(12))-2;
            std::memcpy(reinterpret_cast<void*>(base),native.data(),native.size());
            prepare(0x445fe0);guest_call.ecx=base;Bytes st(reinterpret_cast<void*>(S),32);st.put32(0,a);st.put32(4,b);st.put32(8,wrap);st.put32(12,layer);run_original32();
            check(guest_call.out_sp==S+16,"445FE0 ret 10");
            prepare(0x446100);guest_call.ecx=base;st.puti(0,index);run_original32();
            frontend_option_init_445fe0(nb,int(a),int(b),std::uint8_t(wrap),layer);frontend_option_set_446100(nb,index);
            check(!std::memcmp(native.data(),reinterpret_cast<void*>(base),PcFrontendOptionBytes),"445FE0 / 446100 option");
            const float x=float(int(rnd(400))-200),y=float(int(rnd(400))-200);const int low=int(rnd(3)),high=low+int(rnd(11)),value=int(rnd(14))-2;
            std::memcpy(reinterpret_cast<void*>(base),native.data(),native.size());
            prepare(0x446340);guest_call.ecx=base;st.put32(0,0x5cc048);st.puti(4,low);st.puti(8,high);st.putf(12,x);st.putf(16,y);st.put32(20,layer);run_original32();
            check(guest_call.out_sp==S+24,"446340 ret 18");
            prepare(0x446430);guest_call.ecx=base;st.puti(0,value);run_original32();
            frontend_slider_init_446340(nb,0x5cc048,low,high,x,y,layer);nb.puti(0xac,value);
            check(!std::memcmp(native.data(),reinterpret_cast<void*>(base),native.size()),"446340 / 446430 slider");
        }
        std::cout<<"{\"routine\":\"title_options_probe\",\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}\n";return mismatches?1:0;
    }
    if(only=="frontend_input_probe"){
        using namespace outrun::platform;
        constexpr unsigned base=0x3af00000,device=base+0x100,axes=base+0x120,keys=base+0x140,trace=base+0x180;
        map_at(base,0x1000,PROT_READ|PROT_WRITE|PROT_EXEC);map_original_arithmetic_bridge(file);
        if(mprotect(reinterpret_cast<void*>(0x659000),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("input globals");
        auto write=[](unsigned pc,std::vector<std::uint8_t> c){r060_raw_write(pc,c.data(),c.size());};
        auto imm=[](std::vector<std::uint8_t>& c,unsigned v){for(unsigned k=0;k<4;++k)c.push_back(std::uint8_t(v>>(k*8)));};
        std::vector<std::uint8_t> c{0xb8};imm(c,device);c.push_back(0xc3);write(0x407930,c);write(0x4035f0,c);
        c={0x8b,0x44,0x24,4,0x0f,0xbf,0x04,0x45};imm(c,axes-6);c.push_back(0xc3);write(0x453720,c);
        c={0xa1};imm(c,keys);c.insert(c.end(),{0x23,0x44,0x24,4,0xc3});write(0x4536f0,c);
        c={0xff,0x05};imm(c,trace);c.insert(c.end(),{0x8b,0x44,0x24,4,0xa3});imm(c,trace+4);
        c.insert(c.end(),{0x8b,0x44,0x24,8,0xa3});imm(c,trace+8);c.insert(c.end(),{0xc2,8,0});write(0x440ed0,c);
        unsigned cases=0,mismatches=0;const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
        auto check=[&](bool ok,const char* label){++cases;if(!ok||(inject&&cases==1)){++mismatches;if(mismatches<12)std::cerr<<"input "<<cases<<" "<<label<<"\n";}};
        std::array<std::uint8_t,0x40> native{};unsigned previous=12;
        auto compare=[&](const FrontendInputSnapshot& in,int arg,bool axis_only){
            std::memcpy(reinterpret_cast<void*>(base),native.data(),native.size());
            std::memcpy(reinterpret_cast<void*>(axes),in.axes.data(),8);
            *reinterpret_cast<unsigned*>(keys)=in.feature_mask;
            *reinterpret_cast<unsigned*>(device+8)=in.device_held;
            *reinterpret_cast<unsigned*>(0x6591e4)=previous;
            std::memset(reinterpret_cast<void*>(trace),0,12);
            prepare(axis_only?0x48f4f0:0x48f5f0);guest_call.ecx=base;
            Bytes(reinterpret_cast<void*>(S),4).put32(0,unsigned(arg));run_original32();
            unsigned result=~0u;std::array<unsigned,3> calls{};
            if(axis_only)check(frontend_input_axes_48f4f0(native.data(),native.size(),in),"axes available");
            else{
                check(frontend_input_action_48f5f0(native.data(),native.size(),in,arg,previous,&calls,
                    [](void* p,unsigned key,int a){auto& t=*static_cast<std::array<unsigned,3>*>(p);++t[0];t[1]=key;t[2]=unsigned(a);return true;},result),"action available");
                check(result==guest_call.out_eax,"action return");
                check(!std::memcmp(calls.data(),reinterpret_cast<void*>(trace),12),"feedback count/key/argument");
            }
            check(previous==*reinterpret_cast<unsigned*>(0x6591e4),"shared previous action");
            check(!std::memcmp(native.data(),reinterpret_cast<void*>(base),native.size()),"whole object and guards");
        };
        constexpr std::int16_t values[]{-32768,-128,-65,-64,-63,0,63,64,65,127,32767};
        constexpr unsigned arguments[]{0,1,2,12,255,256,~0u};
        // All combinations of feature bits, independently varying raw device
        // bits, prior action and feedback arguments. Constructors/guards vary.
        for(unsigned i=0;i<65536;++i){
            native.fill(std::uint8_t(i));title_base_construct_48f480(native.data(),native.size(),previous);
            previous=i%15;FrontendInputSnapshot in;in.feature_mask=i;in.device_held=(i%4)*0x1000;
            compare(in,int(arguments[i%7]),false);
        }
        // Signed axis extrema/thresholds and every counter branch, including
        // INT_MIN and INT_MAX. Compare both standalone update and action.
        constexpr unsigned counters[]{0,1,20,~0u,0xfffffffeu,0x80000000,0x7fffffff};
        for(unsigned axis=0;axis<4;++axis)for(auto v:values)for(auto timer:counters)for(unsigned p=0;p<15;++p){
            native.fill(0x5a);title_base_construct_48f480(native.data(),native.size(),previous);previous=p;
            Bytes(native.data(),native.size()).put32(0x1c+axis*4,timer);
            FrontendInputSnapshot in;in.axes[axis]=v;compare(in,int(arguments[p%7]),p%2);
        }
        // Retained repeat traces: neutral, held, sign reversal without neutral,
        // release, simultaneous axes, and shared state changed by another menu.
        native.fill(0xa5);title_base_construct_48f480(native.data(),native.size(),previous);
        for(unsigned frame=0;frame<2048;++frame){
            FrontendInputSnapshot in;
            for(unsigned axis=0;axis<4;++axis)in.axes[axis]=values[(frame/(37+axis*13))%11];
            if(frame%101==0)previous=frame%13;
            in.device_held=(frame/157%4)*0x1000;in.feature_mask=frame%59==0?4:0;
            compare(in,int(arguments[frame%7]),false);
        }
        std::cout<<"{\"routine\":\"frontend_input_probe\",\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}\n";return mismatches?1:0;
    }
    if(only=="frontend_commands_probe"){
        map_original_arithmetic_bridge(file);set_r081_patches(true);set_r059_patches(true);
        unsigned cases=0,mismatches=0;const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
        auto check=[&](bool ok,const char* label){++cases;if(!ok||(inject&&cases==1)){++mismatches;if(mismatches<12)std::cerr<<"commands "<<cases<<" "<<label<<"\n";}};
        for(unsigned i=0;i<384;++i)for(unsigned op=0;op<6;++op){
            auto native=r081_fixture(i,true);Bytes nb(native.data(),native.size());
            // Include active resources, but leave matrix/motion outside this
            // differential corpus. Configure/finalize remain captured leaves.
            for(unsigned slot=0;slot<4;++slot){auto c=nb.sub(0x40c+slot*0xa0,0xa0);c.put32(8,(i>>slot)&1?100+slot:~0u);c.put32(0x24,1);c.put32(0x20,1);}
            if(op==2&&i%4==0)nb.put32(0x408,31); // full history is a no-op
            std::array<unsigned,8> args{4,0x295,0x8000,0x297,0x4000,0x29a,8,0x296};
            if(i&1)args={1,0x298,~0u,~0u,2,99,0x20,0x297};
            std::memcpy(reinterpret_cast<void*>(R081ObjectBase),native.data(),native.size());reset_r081_trace();
            std::memset(reinterpret_cast<void*>(R059StateBase),0,64);
            *reinterpret_cast<unsigned char*>(0x7d68bc)=i&1;
            constexpr unsigned pc[]{0x446c50,0x446d90,0x446fc0,0x447000,0x447090,0x446ea0};
            prepare(pc[op]);guest_call.ecx=R081ObjectBase;
            auto stack=Bytes(reinterpret_cast<void*>(S),32);
            if(op==0)stack.put32(0,i%4);
            if(op==1)for(unsigned j=0;j<8;++j)stack.put32(j*4,args[j]);
            if(op==3){stack.put32(0,(i/2)%3);stack.put32(4,(i/6)%2);}
            if(op==5)stack.put32(0,i%96==95?~0u:i%96);
            run_original32();const auto original_return=guest_call.out_eax&255;
            R081Trace trace{};auto sv=r081_services(trace);PcUiNotifyGlobals gl{};gl.alternate=i&1;
            switch(op){
            case 0:embedded_slot_close_446c50(nb,i%4,sv);break;
            case 1:check(embedded_slots_set_446d90(nb,args,gl,sv)==bool(original_return),"set result");break;
            case 2:embedded_slots_push_446fc0(nb,gl,sv);break;
            case 3:embedded_slots_visible_447000(nb,(i/2)%3,(i/6)%2,sv);break;
            case 4:embedded_slots_clear_447090(nb,gl,sv);break;
            case 5:check(embedded_slots_select_446ea0(nb,i%96==95?~0u:i%96,gl,sv)==bool(original_return),"table result");break;
            }
            const auto original=r081_guest_trace_blob(),actual=r081_trace_blob(trace);
            check(!std::memcmp(reinterpret_cast<void*>(R081ObjectBase),native.data(),native.size()),"whole object");
            check(original==actual,"ordered configure/finalize calls");
        }
        std::cout<<"{\"routine\":\"frontend_commands_probe\",\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}\n";return mismatches?1:0;
    }
    if(only=="frontend_list_probe"){
        using namespace outrun::platform;
        constexpr unsigned base=0x3aa00000,heap=base+0x1000,gcount=base+0x6000,icount=gcount+4,scount=gcount+8;
        constexpr unsigned gout=base+0x10000,iout=base+0x50000,next=base+0x6204,timer=base+0x6200;
        map_at(base,0x80000,PROT_READ|PROT_WRITE|PROT_EXEC);map_original_arithmetic_bridge(file);
        for(auto page:{0x956000u,0x95b000u,0x76f000u})if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("list globals");
        auto stub=[](unsigned pc,unsigned short pop=0){const std::uint8_t c[]{0xc2,std::uint8_t(pop),std::uint8_t(pop>>8)};r060_raw_write(pc,c,3);};
        auto invoke=[&](unsigned pc,unsigned self,std::initializer_list<unsigned> args={}){
            prepare(pc);guest_call.ecx=self;std::copy(args.begin(),args.end(),reinterpret_cast<unsigned*>(S));run_original32();
            if(guest_call.out_sp!=S+4*args.size())throw std::runtime_error("list thiscall stack");return guest_call.out_eax;};
        // Native allocation is owned storage. The original allocator uses an
        // explicit zeroed test arena; SEH FS:0 points to private TLS storage.
        std::vector<std::uint8_t> alloc{0xa1};auto emit=[](auto& v,unsigned u){for(unsigned i=0;i<4;++i)v.push_back(std::uint8_t(u>>(8*i)));};
        emit(alloc,next);alloc.insert(alloc.end(),{0x81,0x05});emit(alloc,next);emit(alloc,0x180);alloc.push_back(0xc3);r060_raw_write(0x5802cf,alloc.data(),alloc.size());
        stub(0x440d50);stub(0x440d70);stub(0x5801a7);
        for(unsigned address:{0x4ed107u,0x4ed10eu,0x4ed14fu,0x4ed167u,0x4ed16eu,0x4ed23fu,
                              0x4ed9b7u,0x4ed9beu,0x4eda53u,0x4ed027u,0x4ed02eu,0x4ed075u}){
            const auto size=*reinterpret_cast<std::uint8_t*>(address+1)==0xa1?6u:7u;std::array<std::uint8_t,7> c{};
            std::memcpy(c.data(),reinterpret_cast<void*>(address),size);c[0]=0x90;const unsigned tls=base+0x6300;
            std::memcpy(c.data()+size-4,&tls,4);r060_raw_write(address,c.data(),size);
        }
        auto capture=[&](unsigned pc,bool glyph){
            const unsigned counter=glyph?gcount:icount,output=glyph?gout:iout;
            std::vector<std::uint8_t> c{0x56,0x57,0x8b,0x3d};emit(c,counter);c.insert(c.end(),{0xc1,0xe7,7,0x81,0xc7});emit(c,output);
            if(glyph)c.insert(c.end(),{0x8b,0x74,0x24,12});
            else {c.insert(c.end(),{0xc7,0x07});emit(c,pc);c.insert(c.end(),{0x83,0xc7,4,0x8d,0x74,0x24,12});}
            c.push_back(0xb9);emit(c,glyph?18:pc==0x42d280?6:pc==0x465860?11:9);c.insert(c.end(),{0xf3,0xa5,0xff,0x05});emit(c,counter);c.insert(c.end(),{0x5f,0x5e});
            if(pc==0x465860)c.insert(c.end(),{0xc2,44,0});else c.push_back(0xc3);r060_raw_write(pc,c.data(),c.size());
        };
        capture(0x42cfe0,true);capture(0x42d280,false);capture(0x42d300,false);
        const std::uint8_t sound[]{0xff,0x05,std::uint8_t(scount),std::uint8_t(scount>>8),std::uint8_t(scount>>16),std::uint8_t(scount>>24),0xc3};r060_raw_write(0x4249f0,sound,sizeof(sound));
        const std::uint8_t clock[]{0xd9,0x05,std::uint8_t(timer),std::uint8_t(timer>>8),std::uint8_t(timer>>16),std::uint8_t(timer>>24),0xc3};r060_raw_write(0x4af500,clock,sizeof(clock));
        // UI animation allocation/submission is a fixture here. Text glyphs,
        // list layout, raw highlight/arrow image arguments run original code.
        stub(0x465250);stub(0x465970);stub(0x465860,44);
        FrontendFontPack fonts;if(!load_frontend_font_pack("runtime-data/switch/outrun2006/or2_fonts.bin",fonts))throw std::runtime_error("list font fixture missing");
        // 42CA60 must see a resident font texture; its missing-texture branch
        // deliberately does NOT set the first-character field. Give the PC a
        // COM GetLevelDesc fixture, not a stubbed font-select routine.
        *reinterpret_cast<unsigned*>(0x956d88)=2;*reinterpret_cast<unsigned*>(0x956d90)=base+0x6700;
        for(unsigned i=0;i<10;++i)*reinterpret_cast<unsigned*>(base+0x6700+4*i)=base+0x6800;
        *reinterpret_cast<unsigned*>(base+0x6800)=base+0x6900;*reinterpret_cast<unsigned*>(base+0x6944)=base+0x70000;
        const std::uint8_t descriptor[]{0x8b,0x44,0x24,12,0xc7,0x40,24,0,2,0,0,0xc7,0x40,28,0,2,0,0,0xc2,12,0};
        r060_raw_write(base+0x70000,descriptor,sizeof(descriptor));
        Bytes globals(reinterpret_cast<void*>(0x956ba0),0x50);globals.put32(0,1);globals.putf(0x24,1);globals.putf(0x28,1);
        *reinterpret_cast<int*>(0x95b21c)=0;*reinterpret_cast<int*>(0x76f7e4)=640;
        unsigned cases=0,mismatches=0;const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";std::map<std::string,unsigned> failures;
        auto check=[&](bool ok,const char* label){++cases;if(!ok||(inject&&cases==1)){++mismatches;if(++failures[label]<4)std::cerr<<"list "<<cases<<" "<<label<<"\n";}};
        for(unsigned seed:{0u,0x5au,0xa5u}){
            std::array<std::uint8_t,PcFrontendListBytes> object;object.fill(std::uint8_t(seed));std::memcpy(reinterpret_cast<void*>(base),object.data(),object.size());
            FrontendSprites sprites;FrontendUiResources ui{sprites};FrontendList list(Bytes(object.data(),object.size()),ui,fonts.fonts[9]);
            invoke(0x4ed950,base);check(!std::memcmp(reinterpret_cast<void*>(base),object.data(),object.size()),"constructor byte preservation");
        }
        // Exhaustive nonempty visibility/enabled masks and original operations.
        for(unsigned count=1;count<=7;++count)for(unsigned mask=0;mask<(1u<<count);++mask)for(unsigned selected=0;selected<count;++selected){
            std::array<std::uint8_t,PcFrontendListBytes> object{};Bytes b(object.data(),object.size());FrontendSprites sprites;FrontendUiResources ui{sprites};FrontendList list(b,ui,fonts.fonts[9]);
            list.initialize_4ecfb0(0,{},320,1,0,0);unsigned index{};const std::string_view label="LICENSE";
            for(unsigned i=0;i<count;++i){list.add_text_4ed160(&label,0,index);list.row(i).put32(8,(mask>>i)&1);list.row(i).put32(0xc,(mask>>i)&1);}
            b.put32(0,selected);
            auto copy=[&](){std::memcpy(reinterpret_cast<void*>(base),object.data(),object.size());*reinterpret_cast<unsigned*>(base+8)=heap;
                for(unsigned i=0;i<count;++i){Bytes dest(reinterpret_cast<void*>(heap+i*0x180),0x178);auto r=list.row(i);
                    for(unsigned j=0;j<0x178;++j)dest.put8(j,r.u8(j));dest.put32(0x174,i+1<count?heap+(i+1)*0x180:0);}};
            auto match=[&](){Bytes original(reinterpret_cast<void*>(base),0x3c);bool ok=true;
                for(unsigned i=0;i<0x3c;++i)if(i<8||i>=12)ok&=original.u8(i)==b.u8(i);
                for(unsigned i=0;i<count;++i)for(unsigned o:{8u,12u})ok&=list.row(i).u32(o)==*reinterpret_cast<unsigned*>(heap+i*0x180+o);
                return ok;};
            copy();check(int(invoke(0x4ed7c0,base))==list.visible_index_4ed7c0(),"visible selection");
            for(unsigned scroll=0;scroll<2;++scroll){b.put8(0x30,scroll);b.put32(0x38,3);copy();check(int(invoke(0x4ed810,base))==list.height_count_4ed810(),"height count");}
            copy();invoke(0x4ed8d0,base);list.reset_selection_4ed8d0();check(match(),"reset selection");b.put32(0,selected);
            for(unsigned pc:{0x4ed300u,0x4ed360u,0x4ed390u}){copy();invoke(pc,base,{selected});
                if(pc==0x4ed390)list.show_4ed390(selected);else list.set_enabled_4ed300(selected,pc==0x4ed360);check(match(),"disable/hide/show");}
            for(bool forward:{false,true}){copy();*reinterpret_cast<unsigned*>(scount)=0;invoke(forward?0x4ed2a0:0x4ed250,base);bool snd{};
                check(list.move(forward,snd)&&match()&&unsigned(snd)==*reinterpret_cast<unsigned*>(scount),"move and sound");}
            for(int target:{-2,-1,0,9}){b.put32(0,selected);copy();const auto result=invoke(0x4ed930,base,{unsigned(target)});
                check(list.select_4ed930(target)==bool(result&255)&&match(),"selection validates old index");}
        }
        // Allocation/init bodies, all three row child constructors, and the
        // scrolling text/display chain. Only Windows allocation/SEH/GPU leaves
        // are adapted; native strings and the original FONT metrics are used.
        for(unsigned variant=0;variant<160;++variant){
            std::array<std::uint8_t,PcFrontendListBytes> object{};Bytes b(object.data(),object.size());FrontendSprites sprites;FrontendUiResources ui{sprites};FrontendList list(b,ui,fonts.fonts[9]);
            std::memset(reinterpret_cast<void*>(heap),0,0x4000);*reinterpret_cast<unsigned*>(next)=heap;
            const float width=variant%2?92.75f:360.5f;list.initialize_4ecfb0(0,{},width,variant&1,(variant>>1)&1,3);
            std::memcpy(reinterpret_cast<void*>(base),object.data(),object.size());invoke(0x4ecfb0,base,{0,fbits(width),variant&1,(variant>>1)&1,3});
            check(!std::memcmp(reinterpret_cast<void*>(base),object.data(),object.size()),"initialize bytes");
            const unsigned xy[]{fbits(float(int(variant%7)-2)+150.75f),fbits(80.5f)};list.setter(0x4ed8a0,xy,2);invoke(0x4ed8a0,base,{xy[0],xy[1]});
            for(unsigned i=0;i<7;++i){const std::string_view label=i==3?"CREATE NEW LICENSE":i==4?"LOAD":"OR2C2C";unsigned index{};
                std::memcpy(reinterpret_cast<void*>(base+0x6400),label.data(),label.size());*reinterpret_cast<char*>(base+0x6400+label.size())=0;
                const auto result=invoke(0x4ed160,base,{base+0x6400,i*2});check(list.add_text_4ed160(&label,i*2,index)&&index==result,"append text result");
                auto r=list.row(i);bool same=true;Bytes gr(reinterpret_cast<void*>(heap+i*0x180),0x178);
                for(unsigned j=0;j<0x174;++j){same&=r.u8(j)==gr.u8(j);if(variant<2&&i==0&&r.u8(j)!=gr.u8(j))std::cerr<<"row diff "<<std::hex<<j<<" "<<unsigned(r.u8(j))<<"/"<<unsigned(gr.u8(j))<<std::dec<<"\n";}check(same,"row constructor/init bytes");
            }
            b.put32(0,variant%7);b.put8(0x13,(variant>>2)&1);b.put8(0x15,1);b.put8(0x12,variant%19==0);b.puti(0x34,int(variant%6)-1);
            for(unsigned i=0;i<7;++i){list.row(i).put32(8,((variant+i)%4)!=0);*reinterpret_cast<unsigned*>(heap+i*0x180+8)=list.row(i).u32(8);}
            for(unsigned off:{0u,0x13u,0x15u,0x12u,0x34u}){if(off==0||off==0x34)*reinterpret_cast<unsigned*>(base+off)=b.u32(off);else *reinterpret_cast<std::uint8_t*>(base+off)=b.u8(off);}
            for(unsigned frame=0;frame<240;++frame){*reinterpret_cast<unsigned*>(gcount)=0;*reinterpret_cast<unsigned*>(icount)=0;*reinterpret_cast<float*>(timer)=(variant&1?-1.f:1.f)*(float(variant*7+frame)+0.75f);
                invoke(0x4ed3e0,base);std::vector<FrontendGlyph> glyphs;std::vector<FrontendListImage> images;
                check(list.display_4ed3e0(*reinterpret_cast<float*>(timer),glyphs,images),"native display");
                check(b.u32(0x34)==*reinterpret_cast<unsigned*>(base+0x34)&&b.u32(0x38)==*reinterpret_cast<unsigned*>(base+0x38),"window scrolling");
                check(glyphs.size()==*reinterpret_cast<unsigned*>(gcount),"glyph count");
                for(unsigned j=0;j<glyphs.size()&&j<*reinterpret_cast<unsigned*>(gcount);++j){const auto& g=glyphs[j];Bytes og(reinterpret_cast<void*>(gout+j*128),72);
                    check(og.u32(0)==g.token&&og.i32(4)==g.left&&og.i32(8)==g.top&&og.i32(12)==g.right&&og.i32(16)==g.bottom&&og.f32(36)==g.x&&og.f32(40)==g.y&&og.u32(44)==g.color,"exact text glyph");}
                check(images.size()==*reinterpret_cast<unsigned*>(icount),"image count");
                for(unsigned j=0;j<images.size()&&j<*reinterpret_cast<unsigned*>(icount);++j){const auto& im=images[j];Bytes oi(reinterpret_cast<void*>(iout+j*128),44);bool same=oi.u32(0)==im.pc;
                    if(im.pc==0x42d280)same&=oi.u32(4)==im.token&&oi.i32(8)==im.x&&oi.i32(12)==im.y&&oi.i32(16)==im.frame&&oi.f32(20)==im.layer&&oi.u32(24)==im.color;
                    else same&=oi.u32(4)==im.mode&&oi.u32(8)==im.token&&oi.i32(12)==im.x&&oi.i32(16)==im.y&&oi.f32(20)==im.width&&oi.f32(24)==im.height&&oi.i32(28)==im.frame&&oi.f32(32)==im.layer&&oi.u32(36)==im.color;
                    check(same,"original highlight/arrow arguments");}
                for(unsigned i=0;i<7;++i)check(list.row(i).i32(0xe8+0xc)==*reinterpret_cast<int*>(heap+i*0x180+0xe8+0xc),"text scrolling phase");
            }
            invoke(0x4eda60,base);check(list.clear_4eda60()&&b.u32(0)==*reinterpret_cast<unsigned*>(base)&&b.u32(4)==0&&*reinterpret_cast<unsigned*>(base+4)==0,"clear preserves selection");
        }
        capture(0x465860,false);
        for(unsigned variant=0;variant<64;++variant){
            std::array<std::uint8_t,PcFrontendListBytes> object{};Bytes b(object.data(),object.size());FrontendSprites sprites;
            sprites.bind_timing(0x44,std::vector<FrontendSpriteTiming>(224,{60,60}));FrontendUiResources ui{sprites};FrontendList list(b,ui,fonts.fonts[9]);
            const std::vector<std::array<unsigned,3>> table{{0x440001,2,7},{0x440004,0,3}};
            std::memcpy(reinterpret_cast<void*>(base+0x6400),table.data(),24);std::memset(reinterpret_cast<void*>(heap),0,0x4000);*reinterpret_cast<unsigned*>(next)=heap;
            list.initialize_4ecfb0(base+0x6400,table,315.5f,1,0,0);std::memcpy(reinterpret_cast<void*>(base),object.data(),object.size());
            unsigned index{};for(unsigned i=0;i<2;++i){check(list.add_sprite_4ed9b0(i,int(i)*9,index)&&index==invoke(0x4ed9b0,base,{i,i*9}),"sprite append");}
            b.put8(0x10,variant&1);b.put32(0,variant%2);b.puti(0x2c,int(variant)-32);b.putf(0x20,float(variant)+100.25f);b.putf(0x24,-50.5f);
            const unsigned head=*reinterpret_cast<unsigned*>(base+8);std::memcpy(reinterpret_cast<void*>(base),object.data(),object.size());*reinterpret_cast<unsigned*>(base+8)=head;
            *reinterpret_cast<unsigned*>(gcount)=0;*reinterpret_cast<unsigned*>(icount)=0;invoke(0x4ed3e0,base);
            std::vector<FrontendGlyph> glyphs;std::vector<FrontendListImage> images;check(list.display_4ed3e0(0,glyphs,images),"sprite layout");
            unsigned seen=0;for(unsigned i=0;i<*reinterpret_cast<unsigned*>(icount);++i){Bytes args(reinterpret_cast<void*>(iout+i*128),48);if(args.u32(0)!=0x465860)continue;
                auto resource=list.row(seen++).sub(0x14,0xa0);const auto* sprite=sprites.get(resource.u32(8));
                check(sprite&&sprite->token==args.u32(4)&&sprite->first==float(args.i32(8))&&sprite->last==float(args.i32(12))&&
                    resource.u32(0x14)==args.u32(16)&&sprite->matrix[12]==args.f32(24)&&sprite->matrix[13]==args.f32(28),"authored sprite args/transform");}
            check(seen==2,"both authored rows submitted");
        }
        // Shared generic window motion: run the original 48CC00 vector helpers
        // and widget positions, replacing only 4035F0's singleton lookup.
        const unsigned root=base+0x72000;const std::uint8_t singleton[]{0xb8,std::uint8_t(root),std::uint8_t(root>>8),std::uint8_t(root>>16),std::uint8_t(root>>24),0xc3};r060_raw_write(0x4035f0,singleton,sizeof(singleton));
        for(unsigned variant=0;variant<300;++variant){
            std::array<std::uint8_t,0x12b0> object{};Bytes b(object.data(),object.size());b.putf(0x1280,float(int(variant)-100));b.putf(0x1284,float(variant%19)*3.75f);
            b.put16(0x36,320+variant%250);b.put16(0x38,80+variant%100);std::memcpy(reinterpret_cast<void*>(base),object.data(),object.size());
            const float x=float(variant%13)*10.25f,y=float(int(variant%27)-10)*5.5f,duration=variant%5==0?0.f:float(variant%9+1)*0.5f;
            if(variant&1){invoke(0x48cb00,base,{fbits(x),fbits(y),fbits(duration)});check(title_controller_move_48cb00(object.data(),object.size(),x,y,duration),"native window move");}
            else {const float width=float(int(variant)-100)*3.25f,height=float(variant%100)*3.5f;
                invoke(0x48d4e0,base,{fbits(x),fbits(y),fbits(width),fbits(height)});check(title_controller_rect_48d4e0(object.data(),object.size(),x,y,width,height),"native window rect");}
            check(!std::memcmp(reinterpret_cast<void*>(base),object.data(),object.size()),"window setup bytes");
            for(unsigned frame=0;frame<120;++frame){const float delta=frame%11==0?0.f:0.125f;
                *reinterpret_cast<float*>(root+0xda0)=delta;invoke(0x48cc00,base);check(title_controller_motion_48cc00(object.data(),object.size(),delta)&&
                    !std::memcmp(reinterpret_cast<void*>(base),object.data(),object.size()),"retained original window motion/layout");}
        }
        // Window initialization/layout/display runs original code. The CRT
        // formatting fixture supports literal strings and the original "%s"
        // at 626468 used by 48F3C0. No generic printf emulation is claimed.
        const std::uint8_t literal_copy[]{0x56,0x57,0x8b,0x7c,0x24,12,0x8b,0x74,0x24,20,
            0x81,0xfe,0x68,0x64,0x62,0,0x75,6,0x8b,0x74,0x24,24,0x8b,0x36,
            0xac,0xaa,0x84,0xc0,0x75,0xfa,0x5f,0x5e,0xc3};
        r060_raw_write(0x580265,literal_copy,sizeof(literal_copy));
        FrontendTextTable text;if(!text.load("../OutRun2006 Coast 2 Coast (FXT)/Text/English_US.bin"))throw std::runtime_error("window text fixture");
        const unsigned loc=base+0x74000;std::vector<std::uint8_t> localized{0x8b,0x44,0x24,4,0x8b,0x04,0x85};emit(localized,loc);localized.push_back(0xc3);r060_raw_write(0x465eb0,localized.data(),localized.size());
        unsigned string_cursor=base+0x78000;
        for(unsigned id:{0x1f1u,0x1f2u,0x295u,0x296u,0x3a6u,0x3a7u,0x233u,0x237u,0x234u,0x2d7u,0x2d8u,0x2dau,0x2d9u}){
            const auto* value=text.get(id);if(!value||value->find('%')!=std::string::npos)throw std::runtime_error("nonliteral window text");
            *reinterpret_cast<unsigned*>(loc+4*id)=string_cursor;std::memcpy(reinterpret_cast<void*>(string_cursor),value->c_str(),value->size()+1);string_cursor+=unsigned(value->size()+1);
        }
        capture(0x429530,false); // first five words are the immediate icon args
        for(unsigned variant=0;variant<256;++variant){
            std::array<std::uint8_t,PcFrontendWindowBytes> object;object.fill(std::uint8_t(variant));unsigned repeat{};
            title_controller_construct_48c490(object.data(),object.size(),repeat);title_controller_init_48c5b0(object.data(),object.size());
            std::memcpy(reinterpret_cast<void*>(base),object.data(),object.size());Bytes b(object.data(),object.size());
            FrontendTextLines lines;std::memset(reinterpret_cast<void*>(0x8303e8),0,2048);
            const auto title=*text.get(0x3a6),body=variant%3?*text.get(0x3a7):std::string{};
            const unsigned body_ptr=base+0x6400;std::memcpy(reinterpret_cast<void*>(body_ptr),body.c_str(),body.size()+1);
            const float x=float(int(variant%13)-4)+100.75f,y=float(variant%17)+80.25f,width=variant%5?float(variant%200)+270.5f:80.f,height=150.75f+float(variant%11);
            const unsigned flags=variant%8,layer=variant%10;
            invoke(0x48d0c0,base,{*reinterpret_cast<unsigned*>(loc+4*0x3a6),body_ptr,flags,fbits(x),fbits(y),fbits(width),fbits(height),layer,variant%2});
            check(frontend_window_init_48d0c0(b,fonts,text,lines,title,body,flags,x,y,width,height,layer,variant%2),"native dialog init");
            bool identical=!std::memcmp(reinterpret_cast<void*>(base),object.data(),object.size());
            if(!identical&&variant<2)for(unsigned j=0;j<object.size();++j)if(object[j]!=*reinterpret_cast<std::uint8_t*>(base+j))std::cerr<<"window diff "<<std::hex<<j<<" "<<unsigned(object[j])<<"/"<<unsigned(*reinterpret_cast<std::uint8_t*>(base+j))<<std::dec<<"\n";
            check(identical,"original dialog init bytes");
            for(unsigned frame=0;frame<3;++frame){
                if(frame){const unsigned center=frame==1,extra=variant%31;invoke(0x48cef0,base,{center,extra});
                    check(frontend_window_height_48cef0(b,fonts,lines,std::uint8_t(center),int(extra))&&!std::memcmp(reinterpret_cast<void*>(base),object.data(),object.size()),"dialog text-driven height");}
                *reinterpret_cast<unsigned*>(gcount)=0;*reinterpret_cast<unsigned*>(icount)=0;invoke(0x48c5f0,base);
                std::vector<FrontendGlyph> glyphs;std::vector<FrontendListImage> images;std::vector<FrontendWindowIcon> icons;
                check(frontend_window_display_48c5f0(b,fonts,lines,glyphs,images,icons),"native dialog display");
                check(glyphs.size()==*reinterpret_cast<unsigned*>(gcount),"dialog glyph count");
                for(unsigned j=0;j<glyphs.size()&&j<*reinterpret_cast<unsigned*>(gcount);++j){const auto& g=glyphs[j];Bytes og(reinterpret_cast<void*>(gout+j*128),72);
                    check(og.u32(0)==g.token&&og.i32(4)==g.left&&og.i32(8)==g.top&&og.i32(12)==g.right&&og.i32(16)==g.bottom&&og.f32(36)==g.x&&og.f32(40)==g.y&&og.u32(44)==g.color,"dialog glyph placement");}
                check(images.size()+icons.size()==*reinterpret_cast<unsigned*>(icount),"dialog image/icon count");unsigned im=0,ic=0;
                for(unsigned j=0;j<*reinterpret_cast<unsigned*>(icount);++j){Bytes args(reinterpret_cast<void*>(iout+j*128),40);
                    if(args.u32(0)==0x429530){if(ic>=icons.size()){check(false,"missing dialog icon");continue;}const auto& icon=icons[ic++];
                        check(args.u32(4)==icon.token&&args.f32(8)==icon.x&&args.f32(12)==icon.y&&args.i32(16)==icon.layer&&args.i32(20)==icon.frame,"dialog icon args");}
                    else {if(im>=images.size()){check(false,"missing dialog image");continue;}const auto& image=images[im++];
                        check(args.u32(4)==image.mode&&args.u32(8)==image.token&&args.i32(12)==image.x&&args.i32(16)==image.y&&args.f32(20)==image.width&&args.f32(24)==image.height&&args.i32(28)==image.frame&&args.f32(32)==image.layer&&args.u32(36)==image.color,"dialog border args");}}
            }
            invoke(0x48ca30,base);frontend_window_suspend_48ca30(b);check(!std::memcmp(reinterpret_cast<void*>(base),object.data(),object.size()),"dialog suspend bytes");
        }
        // Complete context creation/close, including the real nested window
        // and seven list rows. No dialog/list leaf is substituted here.
        for(unsigned flags=0;flags<16;++flags)for(unsigned selected=0;selected<4;++selected){
            constexpr unsigned owner=base+0x20000;
            std::vector<std::uint8_t> object(PcLicenseChooserBytes,0xa5);Bytes b(object.data(),object.size());
            FrontendSprites sprites;FrontendUiResources ui{sprites};FrontendLicenseWidgets widgets(b,ui,fonts,text);unsigned repeat{},result{};
            check(widgets.construct_window(0x9d54,repeat)&&widgets.construct_list(0xb004),"context children constructed");
            auto& svc=widgets.services();check(svc.call(svc.user,4,0x9d54,nullptr,0,result),"context window initialized");
            b.put32(0x44,selected);b.put8(0x1320,flags&1);b.put8(0x1321,(flags>>1)&1);b.put8(0x1322,(flags>>2)&1);b.put8(0x9c9c+12*selected,(flags>>3)&1);
            std::memcpy(reinterpret_cast<void*>(owner),object.data(),object.size());
            std::memset(reinterpret_cast<void*>(heap),0,0x4000);*reinterpret_cast<unsigned*>(next)=heap;
            std::memset(reinterpret_cast<void*>(0x8303e8),0,2048);
            invoke(0x4e12b0,owner);check(widgets.open_context_4e12b0(),"native context open");
            auto compare_owner=[&](){bool same=true;for(unsigned j=0;j<object.size();++j){if(j>=0xb00c&&j<0xb010)continue;
                    if(object[j]!=*reinterpret_cast<std::uint8_t*>(owner+j)){same=false;if(selected==0)std::cerr<<"context diff flags="<<flags<<" "<<std::hex<<j<<" "<<unsigned(object[j])<<"/"<<unsigned(*reinterpret_cast<std::uint8_t*>(owner+j))<<std::dec<<"\n";}}
                return same;};
            check(compare_owner(),"complete context object");
            auto* list=widgets.list(0xb004);check(list&&list->size()==7,"context seven rows");
            for(unsigned i=0;i<7;++i){auto row=list->row(i);bool same=true;
                for(unsigned j=0;j<0x174;++j)same&=row.u8(j)==*reinterpret_cast<std::uint8_t*>(heap+i*0x180+j);
                check(same,"context row state");}
            invoke(0x4e1680,owner);check(widgets.close_context_4e1680()&&compare_owner()&&list->size()==0,"complete context close");
        }
        // Older full-widget choice list used by the real delete confirmation.
        // Preserve its own resource/row layout rather than substituting the
        // compact context-list representation.
        for(unsigned address:{0x48dad7u,0x48dadeu,0x48db1fu,0x48db37u,0x48db3eu,0x48dbfbu,0x48da27u,0x48da2eu,0x48da75u,0x48e397u,0x48e39eu,0x48e42cu}){
            const auto size=*reinterpret_cast<std::uint8_t*>(address+1)==0xa1?6u:7u;std::array<std::uint8_t,7> c{};
            std::memcpy(c.data(),reinterpret_cast<void*>(address),size);c[0]=0x90;const unsigned tls=base+0x6300;
            std::memcpy(c.data()+size-4,&tls,4);r060_raw_write(address,c.data(),size);
        }
        // This part's rows are 0x5E0, not the earlier compact list's 0x178.
        std::vector<std::uint8_t> choice_alloc{0xa1};emit(choice_alloc,next);choice_alloc.insert(choice_alloc.end(),{0x81,0x05});emit(choice_alloc,next);emit(choice_alloc,0x600);choice_alloc.push_back(0xc3);
        r060_raw_write(0x5802cf,choice_alloc.data(),choice_alloc.size());
        for(unsigned pattern:{0u,0x5au,0xa5u}){
            std::array<std::uint8_t,PcFrontendChoiceListBytes> object;object.fill(std::uint8_t(pattern));std::memcpy(reinterpret_cast<void*>(base),object.data(),object.size());
            FrontendSprites sprites;FrontendUiResources ui{sprites};FrontendChoiceList list(Bytes(object.data(),object.size()),ui,fonts);
            invoke(0x48e310,base);check(!std::memcmp(reinterpret_cast<void*>(base),object.data(),object.size()),"choice constructor untouched bytes");
        }
        const unsigned owner=base+0x20000;
        for(unsigned page:{0x692000u,0x780000u,0x7d6000u})if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("title globals");
        for(unsigned state:{2u,3u,4u})for(unsigned mode:{1u,4u})for(unsigned manager_key:{0u,6u,7u}){
            std::array<std::uint8_t,TitleOwnerPcSize> object{};Bytes b(object.data(),object.size());unsigned repeat{};std::uint8_t game{},flag{};
            check(title_owner_construct_complete_4d7140(object.data(),object.size(),game,flag,repeat),"title parent construction");
            FrontendSprites sprites;sprites.bind_timing(0x2c,std::vector<FrontendSpriteTiming>(319,{60,60}));FrontendUiResources ui{sprites};
            FrontendChoiceList choices(b.sub(0x34,PcFrontendChoiceListBytes),ui,fonts);FrontendTextLines lines;
            TitleMenuGlobals menu{state,mode,manager_key!=0,manager_key};TitleOwnerGlobals layers;const unsigned layer=state==2?8:14;
            for(unsigned i=0;i<3;++i){layers.scene_ids[i]=layer+i;*reinterpret_cast<unsigned*>(0x692b28+i*4)=layer+i;}
            *reinterpret_cast<unsigned*>(root+0x218)=state;*reinterpret_cast<unsigned*>(0x780258)=mode;
            *reinterpret_cast<unsigned*>(0x7d68ac)=manager_key?base+0x73000:0;
            *reinterpret_cast<unsigned*>(base+0x7305c)=base+0x73100;*reinterpret_cast<unsigned*>(base+0x73108)=manager_key;
            std::memcpy(reinterpret_cast<void*>(owner),object.data(),object.size());
            std::memset(reinterpret_cast<void*>(heap),0,0x4000);*reinterpret_cast<unsigned*>(next)=heap;
            invoke(0x4d5e40,owner);
            check(frontend_title_init_4d5e40(b,choices,fonts,text,lines,menu,layers,{},repeat),"full native title init");
            bool same=true;unsigned shown=0;
            for(unsigned j=0;j<object.size();++j){if((j>=0x3c&&j<0x40)||(j>=0x481&&j<0x484))continue;
                if(object[j]!=*reinterpret_cast<std::uint8_t*>(owner+j)){same=false;if(cases<2000000&&shown++<6)std::cerr<<"title init offset "<<std::hex<<j<<" "<<unsigned(object[j])<<"/"<<unsigned(*reinterpret_cast<std::uint8_t*>(owner+j))<<std::dec<<"\n";}}
            check(same,"full title parent/window/list bytes");check(menu.delay_692c9c==*reinterpret_cast<float*>(0x692c9c),"initial title input delay");
            for(unsigned i=0;i<choices.size();++i){auto row=choices.row(i);bool equal=true;
                for(unsigned j=0;j<0x5dc;++j)equal&=row.u8(j)==*reinterpret_cast<std::uint8_t*>(heap+i*0x600+j);
                check(equal,"title sprite row constructor and table index");}
            for(unsigned variant=0;variant<8;++variant){
                auto header=b.sub(0x34,PcFrontendChoiceListBytes);header.put32(0,variant%choices.size());header.put8(0x18,variant%2);header.put8(0x1b,(variant/2)%2);
                *reinterpret_cast<unsigned*>(owner+0x34)=header.u32(0);*reinterpret_cast<std::uint8_t*>(owner+0x34+0x18)=header.u8(0x18);
                *reinterpret_cast<std::uint8_t*>(owner+0x34+0x1b)=header.u8(0x1b);
                *reinterpret_cast<unsigned*>(gcount)=0;*reinterpret_cast<unsigned*>(icount)=0;*reinterpret_cast<float*>(timer)=0;
                invoke(0x48dda0,owner+0x34);std::vector<FrontendGlyph> glyphs;
                check(choices.display_48dda0(0,lines,glyphs),"original title sprite display");
                const unsigned per_row=variant%2?2:1;check(*reinterpret_cast<unsigned*>(icount)==choices.size()*per_row&&glyphs.empty(),"title submitted sprite count");
                for(unsigned i=0;i<choices.size()*per_row;++i){Bytes args(reinterpret_cast<void*>(iout+i*128),48);
                    auto resource=choices.row(i/per_row).sub(per_row==2&&i%2?0xb0:0x10,0xa0);const auto* sprite=sprites.get(resource.u32(8));
                    check(sprite&&sprite->token==args.u32(4)&&sprite->first==args.i32(8)&&sprite->last==args.i32(12)&&resource.u32(0x14)==args.u32(16)&&
                          sprite->matrix[12]==args.f32(24)&&sprite->matrix[13]==args.f32(28),"title original sprite layers/placement/ranges");}
            }
            check(choices.clear_48e440()&&sprites.used(layer+1)==0&&sprites.used(layer+2)==0,"title sprites release");
        }
        for(unsigned variant=0;variant<12;++variant){
            std::vector<std::uint8_t> object(PcLicenseChooserBytes);Bytes b(object.data(),object.size());unsigned repeat{};
            FrontendSprites sprites;sprites.bind_timing(0x44,std::vector<FrontendSpriteTiming>(224,{60,60}));FrontendUiResources ui{sprites};
            FrontendLicenseWidgets widgets(b,ui,fonts,text);
            check(widgets.construct_window(0xb040,repeat)&&widgets.construct_choice_list(0xc2f0,repeat),"delete children");
            std::memcpy(reinterpret_cast<void*>(owner),object.data(),object.size());
            std::memset(reinterpret_cast<void*>(heap),0,0x4000);*reinterpret_cast<unsigned*>(next)=heap;
            invoke(0x4e16a0,owner);check(widgets.open_delete_4e16a0(),"native delete open");
            auto* choices=widgets.choice_list(0xc2f0);check(choices&&choices->size()==2,"original two choices");
            bool same=true;for(unsigned j=0;j<object.size();++j){
                if((j>=0xc2f8&&j<0xc2fc)||(j>=0xc2f0+0x30+0x41d&&j<0xc2f0+0x30+0x420))continue;
                if(object[j]!=*reinterpret_cast<std::uint8_t*>(owner+j)){same=false;if(variant==0)std::cerr<<"delete field "<<std::hex<<j<<" "<<unsigned(object[j])<<"/"<<unsigned(*reinterpret_cast<std::uint8_t*>(owner+j))<<std::dec<<"\n";}}
            check(same,"delete window and list state");
            for(unsigned i=0;i<2;++i){auto r=choices->row(i);bool equal=true;
                for(unsigned j=0;j<0x5dc;++j)if(r.u8(j)!=*reinterpret_cast<std::uint8_t*>(heap+i*0x600+j)){equal=false;
                    if(variant==0)std::cerr<<"choice row "<<i<<" "<<std::hex<<j<<" "<<unsigned(r.u8(j))<<"/"<<unsigned(*reinterpret_cast<std::uint8_t*>(heap+i*0x600+j))<<std::dec<<"\n";}
                check(equal,"choice text/resource row bytes");}
            auto native_header=b.sub(0xc2f0,PcFrontendChoiceListBytes);const unsigned original_header=owner+0xc2f0;
            // Execute the original parent control entry too, not only the list
            // leaves. Only its owner input virtual is a platform fixture.
            const unsigned input_table=base+0x71000,input_code=base+0x76000,input_value=base+0x71300;
            *reinterpret_cast<unsigned*>(owner)=input_table;
            *reinterpret_cast<unsigned*>(input_table+0x14)=input_code;
            std::vector<std::uint8_t> input_stub{0xa1};emit(input_stub,input_value);
            input_stub.insert(input_stub.end(),{0xc2,4,0});r060_raw_write(input_code,input_stub.data(),input_stub.size());
            struct DeleteInput {int value{};unsigned sounds{};} input;
            widgets.external(&input,[](void* p,LicenseEditorServices&,unsigned pc,std::size_t off,const unsigned* a,std::size_t n,unsigned& r){
                auto& f=*static_cast<DeleteInput*>(p);r=0;
                if(pc==0x48f5f0&&off==0&&n==1&&a[0]==1){r=unsigned(f.value);return true;}
                if(pc==0x4249f0&&n==1&&a[0]==1){++f.sounds;return true;}return false;
            });
            for(unsigned selected=0;selected<2;++selected)for(int action:{-1,0,1,2,3,4,5,255}){
                native_header.put32(0,selected);*reinterpret_cast<unsigned*>(original_header)=selected;
                b.put8(0xb016,0x5a);*reinterpret_cast<std::uint8_t*>(owner+0xb016)=0x5a;
                input.value=action;input.sounds=0;*reinterpret_cast<unsigned*>(input_value)=unsigned(action);
                *reinterpret_cast<unsigned*>(scount)=0;
                const unsigned expected=invoke(0x4e17e0,owner);unsigned actual{};
                check(widgets.control_delete_4e17e0(actual)&&actual==expected&&
                    native_header.u32(0)==*reinterpret_cast<unsigned*>(original_header)&&
                    b.u8(0xb016)==*reinterpret_cast<std::uint8_t*>(owner+0xb016)&&
                    input.sounds==*reinterpret_cast<unsigned*>(scount),"delete original parent commands");
            }
            for(unsigned frame=0;frame<6;++frame){
                if(frame){bool sound{};const bool forward=(frame+variant)%2;
                    *reinterpret_cast<unsigned*>(scount)=0;invoke(forward?0x48dc60:0x48dc10,original_header,{1});
                    check(choices->move(forward,sound)&&sound&&*reinterpret_cast<unsigned*>(scount)==1&&native_header.u32(0)==*reinterpret_cast<unsigned*>(original_header),"choice navigation and sound");}
                *reinterpret_cast<unsigned*>(gcount)=0;*reinterpret_cast<unsigned*>(icount)=0;*reinterpret_cast<float*>(timer)=float(frame);
                invoke(0x48dda0,original_header);FrontendTextLines lines;std::vector<FrontendGlyph> glyphs;
                check(choices->display_48dda0(float(frame),lines,glyphs),"native choice display");
                check(glyphs.size()==*reinterpret_cast<unsigned*>(gcount),"choice glyph count");
                for(unsigned j=0;j<glyphs.size()&&j<*reinterpret_cast<unsigned*>(gcount);++j){const auto& g=glyphs[j];Bytes og(reinterpret_cast<void*>(gout+j*128),72);
                    check(og.u32(0)==g.token&&og.i32(4)==g.left&&og.i32(8)==g.top&&og.i32(12)==g.right&&og.i32(16)==g.bottom&&og.f32(36)==g.x&&og.f32(40)==g.y&&og.u32(44)==g.color,"choice glyph placement");}
                check(*reinterpret_cast<unsigned*>(icount)==2,"choice authored backgrounds");
                for(unsigned j=0;j<2;++j){Bytes args(reinterpret_cast<void*>(iout+j*128),48);auto r=choices->row(j);const auto* sprite=sprites.get(r.u32(0xb8));
                    check(sprite&&sprite->token==args.u32(4)&&sprite->first==float(args.i32(8))&&sprite->last==float(args.i32(12))&&
                        sprite->matrix[12]==args.f32(24)&&sprite->matrix[13]==args.f32(28)&&r.u32(0xb0+0x14)==args.u32(16),"choice original animation args");}
            }
            invoke(0x48e440,original_header);check(choices->clear_48e440()&&choices->size()==0&&native_header.u32(4)==2&&sprites.used(5)==0,"choice cleanup preserves original count");
        }
        // Complete chooser construction/lifecycle. Nested constructors, array
        // construction and panel initialization execute original instructions.
        for(unsigned address:{0x4dd5c7u,0x4dd5ceu,0x4dd6adu,0x468e47u,0x468e4eu,0x468f0eu,
                              0x4e1897u,0x4e189eu,0x4e19cbu,0x4e0ab7u,0x4e0abeu,0x4e0b7fu,
                              0x48c497u,0x48c49eu,0x48c50du,0x5834a9u,0x5834d8u,0x5834e2u}){
            const auto op=*reinterpret_cast<std::uint8_t*>(address+1);const unsigned size=op==0xa1||op==0xa3?6:7;
            std::array<std::uint8_t,7> code{};std::memcpy(code.data(),reinterpret_cast<void*>(address),size);code[0]=0x90;
            const unsigned tls=base+0x6300;std::memcpy(code.data()+size-4,&tls,4);r060_raw_write(address,code.data(),size);
        }
        if(mprotect(reinterpret_cast<void*>(0x7b1000),0x12000,PROT_READ|PROT_WRITE))throw std::runtime_error("chooser common save globals");
        for(unsigned page:{0x698000u})
            if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("chooser globals");
        const unsigned bank=base+0x40000,owner_calls=base+0x71400,owner_args=base+0x30000;
        *reinterpret_cast<unsigned*>(0x7b17f0)=bank;
        for(unsigned pc:{0x42efa0u,0x42fc90u,0x49a650u})stub(pc);
        auto owner_capture=[&](unsigned pc,unsigned count){
            std::vector<std::uint8_t> code{0x56,0x57,0x51,0x8b,0x3d};emit(code,owner_calls);
            code.insert(code.end(),{0xc1,0xe7,6,0x81,0xc7});emit(code,owner_args);
            code.insert(code.end(),{0xc7,0x07});emit(code,pc);code.insert(code.end(),{0x83,0xc7,4,0x8d,0x74,0x24,16,0xb9});emit(code,count);
            code.insert(code.end(),{0xf3,0xa5,0xff,0x05});emit(code,owner_calls);
            code.insert(code.end(),{0x59,0x5f,0x5e,0x31,0xc0,0xc2,std::uint8_t(count*4),0});r060_raw_write(pc,code.data(),code.size());
        };
        owner_capture(0x442f20,2);owner_capture(0x440ea0,8);
        struct ChooserCalls{std::vector<std::vector<unsigned>> calls;};
        for(unsigned variant=0;variant<48;++variant){
            std::vector<std::uint8_t> object(PcLicenseEditorBytes,std::uint8_t(variant*7));
            std::memcpy(reinterpret_cast<void*>(owner),object.data(),object.size());unsigned repeat{};
            FrontendSprites sprites;FrontendUiResources ui{sprites};FrontendLicenseWidgets widgets(Bytes(object.data(),object.size()),ui,fonts,text);
            invoke(0x4dd5c0,owner);
            check(widgets.construct_editor_4dd5c0(repeat)&&repeat==12&&
                !std::memcmp(reinterpret_cast<void*>(owner),object.data(),object.size()),"whole original editor construction");
        }
        for(unsigned variant=0;variant<48;++variant){
            std::vector<std::uint8_t> object(PcLicenseChooserBytes,std::uint8_t(variant*7));Bytes b(object.data(),object.size());
            std::memcpy(reinterpret_cast<void*>(owner),object.data(),object.size());unsigned repeat{};
            FrontendSprites sprites;FrontendUiResources ui{sprites};FrontendLicenseWidgets widgets(b,ui,fonts,text);
            invoke(0x4e1890,owner);check(widgets.construct_chooser_4e1890(repeat)&&repeat==12&&
                !std::memcmp(reinterpret_cast<void*>(owner),object.data(),object.size()),"whole original chooser construction");
            FrontendProfiles profiles;for(unsigned i=0;i<4;++i){frontend_license_reset_4471a0(profiles.licenses[i],i+variant);
                profiles.licenses[i][0x3f4]|=1;}
            frontend_profiles_select_448520(profiles,variant%4);LicenseChooserState state;state.restore_slot=variant%3==0?-1:std::int8_t(variant%4);
            auto globals=[&](){std::memcpy(reinterpret_cast<void*>(bank),profiles.licenses.data(),sizeof(profiles.licenses));
                Bytes(profiles.common.data(),profiles.common.size()).put32(0,profiles.selected);
                std::memcpy(reinterpret_cast<void*>(0x7b17f8),profiles.common.data(),profiles.common.size());
                std::memcpy(reinterpret_cast<void*>(0x7c23e0),profiles.active.data(),profiles.active.size());
                *reinterpret_cast<unsigned*>(0x7b17f8)=profiles.selected;*reinterpret_cast<std::int8_t*>(0x698af9)=state.restore_slot;};
            auto equal=[&](){return !std::memcmp(reinterpret_cast<void*>(owner),object.data(),object.size())&&
                !std::memcmp(reinterpret_cast<void*>(0x7b17f8),profiles.common.data(),profiles.common.size())&&
                !std::memcmp(reinterpret_cast<void*>(bank),profiles.licenses.data(),sizeof(profiles.licenses))&&
                !std::memcmp(reinterpret_cast<void*>(0x7c23e0),profiles.active.data(),profiles.active.size())&&
                profiles.selected==*reinterpret_cast<unsigned*>(0x7b17f8)&&state.restore_slot==*reinterpret_cast<std::int8_t*>(0x698af9);};
            ChooserCalls calls;widgets.external(&calls,[](void* p,LicenseEditorServices&,unsigned pc,std::size_t off,const unsigned* args,std::size_t n,unsigned& r){
                r=0;if(off||!((pc==0x442f20&&n==2)||(pc==0x440ea0&&n==8)))return false;
                auto& c=*static_cast<ChooserCalls*>(p);c.calls.push_back({pc});c.calls.back().insert(c.calls.back().end(),args,args+n);return true;});
            globals();*reinterpret_cast<unsigned*>(owner_calls)=0;
            const unsigned init=invoke(0x4e19e0,owner);
            check(license_chooser_init_4e19e0(b,profiles,state,widgets.services())&&(init&255)==1&&equal(),"original chooser init and restored profile");
            check(calls.calls.size()==2&&*reinterpret_cast<unsigned*>(owner_calls)==2,"original owner command count");
            for(unsigned i=0;i<calls.calls.size();++i)check(!std::memcmp(reinterpret_cast<void*>(owner_args+i*64),calls.calls[i].data(),calls.calls[i].size()*4),"original owner command arguments including protected push");
            // Exercise signed mapping separately from a direct bank selection.
            const int previous=int(variant%5)-1;b.put8(0x54,std::uint8_t(previous));
            for(int i=-1;i<4;++i)b.put32(std::size_t(0x9c94+12*i),unsigned(i+2)%4);
            if(variant&1)profiles.active[0x3f4]&=~1u;
            if(variant%7==0)profiles.selected=~0u;
            std::memcpy(reinterpret_cast<void*>(owner),object.data(),object.size());globals();
            invoke(0x4e1a70,owner);check(license_chooser_reset_4e1a70(b,profiles,state)&&equal(),"original reset signed slot mapping");
            invoke(0x4e1ac0,owner);check(license_chooser_suspend_4e1ac0(b,profiles,state,widgets.services())&&equal(),"original suspend windows rows profiles and resources");
        }
        for(const auto& f:failures)std::cerr<<f.first<<": "<<f.second<<"\n";
        std::cout<<"{\"routine\":\"frontend_list\",\"cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<",\"raw_gpu_submission_captured\":true}\n";return mismatches?1:0;
    }
    if(only=="frontend_license_panels_probe"){
        using namespace outrun::platform;
        constexpr unsigned base=0x3a900000,bank=base+0x10000,tc=base+0x20000,sc=base+0x20004;
        constexpr unsigned texts=base+0x21000,resources=base+0x23000;
        map_at(base,0x40000,PROT_READ|PROT_WRITE|PROT_EXEC);map_original_arithmetic_bridge(file);
        for(auto page:{0x7b1000u,0x7c2000u,0x84b000u,0x780000u,0x698000u})
            if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("panels globals protection");
        *reinterpret_cast<unsigned*>(0x7b17f0)=bank;
        auto stub=[](unsigned pc,unsigned value,unsigned short pop=0){
            std::array<std::uint8_t,8> c{{0xb8,0,0,0,0,0xc2,0,0}};
            std::memcpy(c.data()+1,&value,4);std::memcpy(c.data()+6,&pop,2);r060_raw_write(pc,c.data(),c.size());};
        auto capture=[&](unsigned pc,unsigned counter,unsigned output,unsigned short pop){
            std::vector<std::uint8_t> c{0x56,0x57,0x51,0x8b,0x3d};
            auto imm=[&](unsigned v){for(unsigned i=0;i<4;++i)c.push_back(std::uint8_t(v>>(8*i)));};
            imm(counter);c.insert(c.end(),{0xc1,0xe7,6,0x81,0xc7});imm(output);
            c.insert(c.end(),{0x89,0x0f,0x83,0xc7,4,0x8d,0x74,0x24,16,0xb9});imm(12);
            c.insert(c.end(),{0xf3,0xa5,0xff,0x05});imm(counter);
            c.insert(c.end(),{0x59,0x5f,0x5e,0x31,0xc0,0xc2,std::uint8_t(pop),std::uint8_t(pop>>8)});
            r060_raw_write(pc,c.data(),c.size());
        };
        capture(0x48f280,tc,texts,0);capture(0x465860,sc,resources,44);
        stub(0x465970,0);stub(0x4659f0,0);stub(0x48f3c0,0);
        // Localization and the table-dependent 447400 completion leaf are
        // fixtures. The original panel controller, rank lookup and time math
        // execute untouched. UI/text calls record arguments, not fake pixels.
        std::memcpy(reinterpret_cast<void*>(base+0x20100),"CREATE NEW LICENSE",19);stub(0x465eb0,base+0x20100);
        *reinterpret_cast<double*>(base+0x20180)=25.125;
        std::array<std::uint8_t,7> original_completion;
        std::memcpy(original_completion.data(),reinterpret_cast<void*>(0x447400),7);
        std::array<std::uint8_t,7> fld{{0xdd,0x05,0,0,0,0,0xc3}};const unsigned fp=base+0x20180;
        std::memcpy(fld.data()+2,&fp,4);r060_raw_write(0x447400,fld.data(),fld.size());
        unsigned cases=0,mismatches=0;const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
        auto check=[&](bool ok,const char* what){++cases;if(!ok||(inject&&cases==1)){
            ++mismatches;if(mismatches<12)std::cerr<<"panels "<<cases<<": "<<what<<"\n";}};
        for(unsigned n=0;n<10000;++n){
            unsigned frames=n<5000?n:(n*7919u);
            prepare(0x449b30);Bytes stack(reinterpret_cast<void*>(S),24);
            for(unsigned i=0;i<4;++i)stack.put32(i*4,base+0x20200+2*i);
            stack.put32(16,frames);stack.put32(20,0);run();const auto time=license_time_449b30(frames);
            check(!std::memcmp(reinterpret_cast<void*>(base+0x20200),time.data(),8),"original time conversion");
        }
        FrontendFontPack fonts;FrontendTextTable table;
        if(!load_frontend_font_pack("runtime-data/switch/outrun2006/or2_fonts.bin",fonts))throw std::runtime_error("font fixture missing");
        const std::string localized="CREATE NEW LICENSE";
        // Same little-endian table envelope as retail Text/English_US.bin.
        // Load the repository fixture supplied by the comparison environment.
        if(!table.load("../OutRun2006 Coast 2 Coast (FXT)/Text/English_US.bin"))throw std::runtime_error("retail text missing");
        FrontendSprites pool;std::vector<FrontendSpriteTiming> timing(224,{60,60});pool.bind_timing(0x44,timing);FrontendUiResources ui{pool};
        for(unsigned occupied=0;occupied<=4;++occupied)for(unsigned selected=0;selected<(occupied<4?occupied+1:occupied);++selected)
        for(unsigned variant=0;variant<8;++variant){
            std::vector<std::uint8_t> object(PcLicenseChooserBytes);Bytes b(object.data(),object.size());
            FrontendProfiles p;for(auto& l:p.licenses)frontend_license_reset_4471a0(l,123);
            for(unsigned i=0;i<occupied;++i){const unsigned slot=(i+2)%4;auto& l=p.licenses[slot];l[0x3f4]|=1;
                Bytes lb(l.data(),l.size());std::memcpy(l.data(),"FERRARI",8);
                lb.putf(0x104,3);lb.putf(0x108,variant&1?4:0);lb.put32(0x10c,variant*98765);lb.putf(0x24,1234.25f);
                lb.put32(0x118,(variant+i)%24);lb.put32(0x18,i);lb.put32(0x1c,i+1);lb.put32(0x20,i+2);
                b.put32(0x9c94+12*i,slot);b.puti(0x9c98+12*i,variant&2?1:-1);b.put8(0x9c9c+12*i,variant&4?1:0);
            }
            p.selected=occupied?2:~0u;p.active_loaded=occupied!=0;if(occupied)p.active=p.licenses[2];
            b.put32(0x44,selected);b.put32(0x40,occupied<4?occupied+1:occupied);b.puti(0x3c,occupied<4?int(occupied):-1);
            FrontendLicenseWidgets w(b,ui,fonts,table);unsigned repeat{};
            w.construct_widget(0xa08,repeat);w.construct_widget(0xe94,repeat);
            for(unsigned i=0;i<3;++i)w.construct_panel_4e0ab0(0x3580+i*0x225c,repeat);
            w.initialize_panels_4e0c70();w.profiles(p,nullptr,[](void*,const PcLicense&,double& v){v=25.125;return true;});
            std::memcpy(reinterpret_cast<void*>(base),object.data(),object.size());
            std::memcpy(reinterpret_cast<void*>(bank),p.licenses.data(),sizeof(p.licenses));
            std::memcpy(reinterpret_cast<void*>(0x7c23e0),p.active.data(),p.active.size());
            *reinterpret_cast<unsigned*>(0x7b17f8)=p.selected;
            *reinterpret_cast<unsigned*>(tc)=0;*reinterpret_cast<unsigned*>(sc)=0;
            prepare(0x4e2150);guest_call.ecx=base;run();pool.reset();
            check(w.populate_panels_4e2150(),"native population");
            const auto text_count=*reinterpret_cast<unsigned*>(tc);check(text_count<=22,"capture bounded");
            for(unsigned i=0;i<text_count&&i<22;++i){Bytes rec(reinterpret_cast<void*>(texts+64*i),64);
                const unsigned off=rec.u32(4)-base;const char* fmt=reinterpret_cast<const char*>(rec.u32(8));char expected[256];
                if(!std::strcmp(fmt,"%s"))std::snprintf(expected,sizeof(expected),fmt,reinterpret_cast<const char*>(rec.u32(20)));
                else if(!std::strcmp(fmt,"%s (%d)"))std::snprintf(expected,sizeof(expected),fmt,reinterpret_cast<const char*>(rec.u32(20)),rec.i32(24));
                else if(!std::strcmp(fmt,"%5.2f%%")||!std::strcmp(fmt,"%.0f")){
                    double v;const auto lo=rec.u32(20),hi=rec.u32(24);const std::uint64_t bits=lo|(std::uint64_t(hi)<<32);std::memcpy(&v,&bits,8);
                    std::snprintf(expected,sizeof(expected),fmt,v);
                }else if(!std::strcmp(fmt,"%2d''%02d'%02d"))std::snprintf(expected,sizeof(expected),fmt,rec.i32(20),rec.i32(24),rec.i32(28));
                else if(!std::strcmp(fmt,"0.00%%"))std::snprintf(expected,sizeof(expected),"0.00%%");
                else std::snprintf(expected,sizeof(expected),"%s",fmt);
                check(off+PcTextWidgetBytes<=object.size(),"widget offset");
                if(off+PcTextWidgetBytes<=object.size()){
                    const auto actual=frontend_text_value_48eec0(b.sub(off,PcTextWidgetBytes));
                    if(actual!=expected&&mismatches<5)std::cerr<<"off="<<std::hex<<off<<std::dec<<" expected="<<expected<<" actual="<<actual<<"\n";
                    check(actual==expected,"PC formatted text");check(b.u32(off+0x450)==rec.u32(12)&&b.u32(off+0x474)==rec.u32(16),"font/color");
                }
            }
            check(!std::memcmp(reinterpret_cast<void*>(base+0x1320),object.data()+0x1320,3),"context availability flags");
            const auto sprite_count=*reinterpret_cast<unsigned*>(sc);check(sprite_count<=9,"sprite capture bounded");
            for(unsigned i=0;i<sprite_count&&i<9;++i){Bytes rec(reinterpret_cast<void*>(resources+64*i),64);
                const unsigned off=rec.u32(0)-base;
                check(off+0xa0<=object.size(),"sprite receiver offset");
                if(off+0xa0<=object.size()){
                    const auto* instance=pool.get(b.u32(off+8));check(instance!=nullptr,"profile sprite committed");
                    if(instance){check(instance->token==rec.u32(4)&&instance->first==float(rec.i32(8))&&instance->last==float(rec.i32(12)),"authored token/frame range");
                        check(instance->matrix[12]==rec.f32(24)&&instance->matrix[13]==rec.f32(28)&&
                              instance->matrix[0]==rec.f32(32)&&instance->matrix[5]==rec.f32(36),"original portrait placement/scale");}
                }
            }
            // Compare original placement independently of the already-tested
            // native glyph renderer, which uses the retail font fixture.
            prepare(0x4e2640);guest_call.ecx=base;run();check(w.display_panel_4e2640(),"native display");
            for(unsigned j=0;j<7;++j){const unsigned off=0x3588+j*0x48c;
                check(!std::memcmp(reinterpret_cast<void*>(base+off+0x34),object.data()+off+0x34,8),"original widget placement");}
            check(!std::memcmp(reinterpret_cast<void*>(bank),p.licenses.data(),sizeof(p.licenses)),"active bank synchronization");
        }
        // Special previous-owner 12/13 card: actual manager pointer lookup,
        // 4EEEE0 rank thresholds and display placement, not a local-save copy.
        if(mprotect(reinterpret_cast<void*>(0x659000),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("special profile globals");
        for(unsigned i=0;i<24;++i)for(int side=-1;side<=1;++side){
            float score=*reinterpret_cast<float*>(0x5ced30+i*4);
            if(side)score=std::nextafter(score,side<0?-INFINITY:INFINITY);
            prepare(0x4eeee0);Bytes(reinterpret_cast<void*>(S),4).putf(0,score);run();
            const auto* original=reinterpret_cast<const char*>(guest_call.out_eax);
            const auto* native=license_rank_4eeee0(score);
            check(original&&native?!std::strcmp(original,native):original==native,"special rank threshold equality/neighbors");
        }
        for(unsigned variant=0;variant<96;++variant){
            pool.reset();std::vector<std::uint8_t> object(PcLicenseChooserBytes);Bytes b(object.data(),object.size());
            FrontendLicenseWidgets w(b,ui,fonts,table);unsigned repeat{};check(w.construct_panel_4e0ab0(0x1324,repeat),"special panel construction");
            std::array<std::uint8_t,0x28> manager{};std::array<std::uint8_t,0x94> record{};
            Bytes rec(record.data(),record.size());const auto data=rec.sub(0x20,0x74);
            const char* name=variant&1?"ORIGINAL DRIVER":"PROFILE";std::memcpy(record.data()+0x20,name,std::strlen(name)+1);
            data.putf(0x40,float(variant)*1.25f);data.putf(0x44,float(variant)/3);data.putf(0x48,variant*125.5f);
            data.putf(0x60,float(variant)*39-5);data.put32(0x64,variant*71829);
            for(unsigned field:{0x68u,0x6cu,0x70u})data.put32(field,variant%3);
            constexpr unsigned mgr=base+0x29000,remote=base+0x29100;
            const auto kind=variant%6;
            Bytes(manager.data(),manager.size()).put32(0x20,kind==2?0:remote);
            const PcNativeHandleBinding bindings[]{{mgr,manager.data(),manager.size()},{remote,record.data(),record.size()}};
            LicenseSpecialProfileSource source{kind==0?-1:2,kind==1?0u:mgr,{bindings,2}};w.special_profile_source(source);
            b.put32(0x44,variant&1);b.put32(0x3c,0);
            std::memcpy(reinterpret_cast<void*>(base),object.data(),object.size());
            std::memcpy(reinterpret_cast<void*>(mgr),manager.data(),manager.size());std::memcpy(reinterpret_cast<void*>(remote),record.data(),record.size());
            *reinterpret_cast<int*>(0x659940)=source.selected;*reinterpret_cast<unsigned*>(0x7d68ac)=source.manager;
            *reinterpret_cast<unsigned*>(tc)=0;*reinterpret_cast<unsigned*>(sc)=0;
            prepare(0x4e0dd0);guest_call.ecx=base;run();check(w.populate_special_4e0dd0(),"native special card population");
            const unsigned count=*reinterpret_cast<unsigned*>(tc);check(count==(kind<3?0u:6u),"original special source absence");
            for(unsigned i=0;i<count&&i<6;++i){Bytes call(reinterpret_cast<void*>(texts+64*i),64);
                const auto off=call.u32(4)-base;const auto* fmt=reinterpret_cast<const char*>(call.u32(8));char expected[128];
                if(!std::strcmp(fmt,"%s"))std::snprintf(expected,sizeof(expected),fmt,reinterpret_cast<const char*>(call.u32(20)));
                else if(!std::strcmp(fmt,"%s (%d)"))std::snprintf(expected,sizeof(expected),fmt,reinterpret_cast<const char*>(call.u32(20)),call.i32(24));
                else if(!std::strcmp(fmt,"%2d''%02d'%02d"))std::snprintf(expected,sizeof(expected),fmt,call.i32(20),call.i32(24),call.i32(28));
                else {double v;std::uint64_t bits=call.u32(20)|(std::uint64_t(call.u32(24))<<32);std::memcpy(&v,&bits,8);std::snprintf(expected,sizeof(expected),fmt,v);}
                check(frontend_text_value_48eec0(b.sub(off,PcTextWidgetBytes))==expected,"special original formatted field");
                check(b.u32(off+0x450)==call.u32(12)&&b.u32(off+0x474)==call.u32(16)&&b.u32(off+0x454)==3,"special font/color/layer");
            }
            const unsigned count_sprites=*reinterpret_cast<unsigned*>(sc);check(count_sprites==(kind<3?0u:3u),"special sprite count");
            for(unsigned i=0;i<count_sprites&&i<3;++i){Bytes call(reinterpret_cast<void*>(resources+64*i),64);const auto off=call.u32(0)-base;
                const auto* sprite=pool.get(b.u32(off+8));check(sprite&&sprite->allocated,"special live sprite");
                if(sprite)check(sprite->token==call.u32(4)&&sprite->first==float(call.i32(8))&&sprite->last==float(call.i32(12))&&
                    sprite->matrix[12]==call.f32(24)&&sprite->matrix[13]==call.f32(28),"special original sprite arguments");
            }
            prepare(0x4e1060);guest_call.ecx=base;run();w.begin_frame();check(w.display_special_4e1060(),"native special card display");
            for(unsigned i=0;i<6;++i)check(!std::memcmp(reinterpret_cast<void*>(base+0x132c+i*0x48c+0x34),object.data()+0x132c+i*0x48c+0x34,8),"special original placement");
        }
        // Context controller: UI/dialog/save/audio are explicit fixtures, but
        // 4E1CF0, 448520, 4E1230 and the protected transition branch run intact.
        for(unsigned pc:{0x4e1680u,0x4e16a0u,0x4e0d80u,0x465250u,0x4ed250u,0x4ed2a0u,
                         0x48e440u,0x4164d0u,0x416420u,0x416500u,0x42efa0u,0x42fc90u,0x49a650u})stub(pc,0);
        stub(0x4035f0,base);const unsigned vt=base+0x27000,noop=base+0x28100,input_stub=base+0x28120;
        stub(noop,0);Bytes vtable(reinterpret_cast<void*>(vt),32);
        vtable.put32(4,noop);vtable.put32(8,noop);vtable.put32(16,noop);vtable.put32(20,input_stub);
        struct ContextInput {int input,answer;unsigned previous;} context{};
        LicenseEditorServices services;services.user=&context;
        services.call=[](void* user,unsigned pc,std::size_t,const unsigned*,std::size_t,unsigned& r){
            auto& in=*static_cast<ContextInput*>(user);
            if(pc==0x48f5f0)r=unsigned(in.input);
            if(pc==0x4e17e0)r=unsigned(in.answer);
            if(pc==0x442ec0)r=in.previous;return true;};
        std::memcpy(services.name_text.data(),"CREATE NEW LICEN",16);
        for(int input=-1;input<=6;++input)for(unsigned choice=0;choice<7;++choice)
        for(unsigned selected=0;selected<3;++selected)for(unsigned previous:{0u,12u})
        for(unsigned confirm=0;confirm<4;++confirm){
            context={input,confirm==1?-1:confirm==2?1:0,previous};
            stub(input_stub,unsigned(input),4);stub(0x4e17e0,unsigned(context.answer));stub(0x442ec0,previous);
            std::vector<std::uint8_t> object(PcLicenseChooserBytes);Bytes b(object.data(),object.size());
            b.put32(0,vt);b.put32(0xb040,vt);b.put32(0x9d54,vt);b.put32(0x38,4);
            b.put32(0x44,selected);b.put32(0x40,3);b.put32(0x3c,2);b.put32(0x48,0);b.put32(0x578,2);
            b.put8(0x36,confirm!=0);b.put32(0xb004,choice);
            b.put32(0x9c94,2);b.put32(0x9ca0,0);b.put32(0x9cac,1);
            FrontendProfiles p;for(auto& l:p.licenses)frontend_license_reset_4471a0(l,123);
            p.licenses[0][0x3f4]|=1;p.licenses[2][0x3f4]|=1;frontend_profiles_select_448520(p,2);
            std::memcpy(reinterpret_cast<void*>(base),object.data(),object.size());
            std::memcpy(reinterpret_cast<void*>(bank),p.licenses.data(),sizeof(p.licenses));
            std::memcpy(reinterpret_cast<void*>(0x7c23e0),p.active.data(),p.active.size());
            *reinterpret_cast<unsigned*>(0x7b17f8)=p.selected;
            *reinterpret_cast<std::uint8_t*>(0x698af9)=0xff;*reinterpret_cast<std::uint8_t*>(0x84b214)=0;
            services.editing_existing=0;LicenseChooserState slide;Bytes guest_slide(reinterpret_cast<void*>(base+0x20220),12);
            for(unsigned i=0;i<3;++i)guest_slide.put32(i*4,0);
            prepare(0x4e1cf0);guest_call.ecx=base;Bytes(reinterpret_cast<void*>(S),4).put32(0,base+0x20220);
            run_original32();check(guest_call.out_sp==S+4,"context stack");
            unsigned result{};check(license_chooser_context_4e1cf0(b,p,slide,services,result),"native context");
            if(std::memcmp(reinterpret_cast<void*>(base),object.data(),object.size())&&mismatches<5){
                std::cerr<<"context input="<<input<<" choice="<<choice<<" selected="<<selected<<" prev="<<previous<<" confirm="<<confirm<<"\n";
                auto* guest=reinterpret_cast<std::uint8_t*>(base);for(unsigned j=0;j<object.size();++j)if(guest[j]!=object[j])
                    std::cerr<<std::hex<<j<<"="<<unsigned(guest[j])<<"/"<<unsigned(object[j])<<std::dec<<" ";std::cerr<<"\n";
            }
            check(!std::memcmp(reinterpret_cast<void*>(base),object.data(),object.size()),"context object bytes");
            check(!std::memcmp(reinterpret_cast<void*>(0x7c23e0),p.active.data(),p.active.size()),"context selected record");
            check(p.selected==*reinterpret_cast<unsigned*>(0x7b17f8)&&result==guest_call.out_eax,"context selection/action");
            check(slide.slide==guest_slide.f32(0)&&slide.step==guest_slide.f32(4)&&slide.target==guest_slide.f32(8),"exit slide");
            check(slide.restore_slot==*reinterpret_cast<std::int8_t*>(0x698af9)&&services.editing_existing==*reinterpret_cast<std::uint8_t*>(0x84b214),"edit/restore globals");
        }
        // Completion provider: run its original complete seven-group chain,
        // including the protected 4E81D0 address calculation. Only category
        // count lookup is a fixture; championship record tables are native data.
        r060_raw_write(0x447400,original_completion.data(),original_completion.size());
        const unsigned counts=base+0x30000;
        std::array<std::uint8_t,12> count_code{{0x8b,0x44,0x24,4,0x0f,0xb6,0x80,0,0,0,0,0xc3}};
        std::memcpy(count_code.data()+7,&counts,4);r060_raw_write(0x495930,count_code.data(),count_code.size());
        for(unsigned variant=0;variant<400;++variant){
            PcLicense record{};frontend_license_reset_4471a0(record,123);
            LicenseProgressTables tables;tables.categories_ready=true;tables.mode=variant%9==0?16:32;
            for(unsigned c=0;c<40;++c){tables.category_counts[c]=std::uint8_t((variant+c)%6);
                for(unsigned j=0;j<5;++j)record[0x1dd+5*c+j]=std::uint8_t((variant*7+c+j)%8);}
            for(unsigned t=0;t<66;++t){record[0x36e + 2*t]=std::uint8_t(((variant+t)%8)|((variant*3+t)%8)<<4);
                record[0x36f+2*t]=std::uint8_t(((variant*5+t)%8)|((variant*7+t)%8)<<4);}
            for(unsigned g=0;g<3;++g){tables.championship_present[g]=(variant>>g)&1;
                const unsigned table=base+0x31000+g*0x800;
                *reinterpret_cast<unsigned*>(0x84b7e0+4*g)=tables.championship_present[g]?table:0;
                for(unsigned race=0;race<15;++race){const unsigned type=(variant+g*15+race)%66;
                    tables.championship_types[g][race]=type;*reinterpret_cast<unsigned*>(table+race*0x78+0x1c)=type;}}
            std::memcpy(reinterpret_cast<void*>(counts),tables.category_counts.data(),40);
            std::memcpy(reinterpret_cast<void*>(bank),record.data(),record.size());
            *reinterpret_cast<unsigned*>(0x78026c)=tables.mode;
            prepare(0x447400);guest_call.ecx=bank;guest_call.st0=1;run();
            long double original{};std::memcpy(&original,guest_call.out_st0_raw,10);double native{};
            check(frontend_license_completion_447400(record,tables,native),"native completion");
            if(double(original)!=native&&mismatches<10)std::cerr<<"completion "<<variant<<" original="<<double(original)<<" native="<<native<<"\n";
            check(double(original)==native,"original completion percentage");
            // Shared original grade/unlock/picture chain used by key3A. Do not
            // replace those bodies with a boolean eligibility fixture.
            std::memcpy(reinterpret_cast<void*>(0x7c23e0),record.data(),record.size());
            for(unsigned group=0;group<7;++group){
                prepare(0x4e82a0);Bytes(reinterpret_cast<void*>(S),8).put32(0,group);
                Bytes(reinterpret_cast<void*>(S),8).put32(4,bank);run_original32();int grade{};
                check(frontend_group_grade_4e82a0(record,tables,group,grade)&&unsigned(grade)==guest_call.out_eax,"shared group grade");
                prepare(0x4e8410);Bytes(reinterpret_cast<void*>(S),4).put32(0,group);run_original32();bool unlocked{};
                check(frontend_category_unlocked_4e8410(record,tables,group,unlocked)&&unlocked==bool(guest_call.out_eax&255),"category unlock");
                *reinterpret_cast<unsigned*>(0x84b7f0)=group;prepare(0x4e8b20);run_original32();unsigned picture{};
                check(frontend_category_picture_4e8b20(record,tables,group,picture)&&picture==guest_call.out_eax,"category picture frame");
                if(variant<8)for(unsigned action:{0u,1u,2u,3u,4u,5u,12u}){
                    constexpr unsigned category_root=base+0x35000;
                    stub(0x4035f0,category_root);stub(0x465250,0);stub(0x4249f0,0);stub(0x48f5f0,action,4);
                    FrontendCategoryState child;unsigned repeat{},result{};
                    frontend_categories_construct_4e8130(child,repeat);child.selected_84b7f0=group;
                    Bytes cb(child.object.data(),child.object.size());cb.put32(4,0xdead);
                    std::array<std::uint8_t,0xe00> category_owner{};Bytes cr(category_owner.data(),category_owner.size());cr.put32(0x20c,0xabcdef);
                    std::memcpy(reinterpret_cast<void*>(category_root),category_owner.data(),category_owner.size());
                    std::memcpy(reinterpret_cast<void*>(base),child.object.data(),child.object.size());
                    *reinterpret_cast<unsigned*>(0x84b7f0)=group;*reinterpret_cast<unsigned*>(sc)=0;
                    prepare(0x4e8780);guest_call.ecx=base;run_original32();const auto original_result=guest_call.out_eax;
                    FrontendSprites category_pool;category_pool.bind_timing(0x44,std::vector<FrontendSpriteTiming>(224,{240,60}));
                    FrontendUiResources category_ui{category_pool};category_ui.effect_4249f0=[](void*,unsigned){return true;};
                    FrontendInputSnapshot input;constexpr unsigned masks[]{4,8,0x400,0x1000,0x800,0x2000};input.feature_mask=action<6?masks[action]:0;
                    FrontendCategoryServices svc{category_ui,cr,record,tables,input,repeat,&fonts,&table};
                    check(frontend_categories_control_4e8780(child,svc,result),"native category control");
                    check(result==original_result&&cb.u32(4)==*reinterpret_cast<unsigned*>(base+4),"category action/next key");
                    check(cr.u32(0x20c)==*reinterpret_cast<unsigned*>(category_root+0x20c)&&child.selected_84b7f0==*reinterpret_cast<unsigned*>(0x84b7f0),"category parent flags/selection");
                    if(action>=2&&action<=5){const auto* sprite=category_pool.get(cb.u32(0x5dc));Bytes a(reinterpret_cast<void*>(resources+4),44);
                        check(sprite&&sprite->token==a.u32(0)&&sprite->first==float(a.u32(4))&&sprite->last==float(a.u32(8)),"category selection original animation");}
                    *reinterpret_cast<unsigned*>(0x84b7f0)=group;
                }
                if(variant<8){
                    // Original display constructs all nine authored resources.
                    // Font/GPU leaves are captures; retail glyph rendering is
                    // exercised separately in frontend_menu_render.
                    for(unsigned pc:{0x42ca60u,0x42cc60u,0x42ccb0u,0x42cca0u,0x42c360u,0x42cc00u,0x42cdd0u})stub(pc,0);
                    FrontendCategoryState child;unsigned repeat{};frontend_categories_construct_4e8130(child,repeat);
                    child.selected_84b7f0=group;std::memcpy(reinterpret_cast<void*>(base),child.object.data(),child.object.size());
                    *reinterpret_cast<unsigned*>(sc)=0;prepare(0x4e8c60);guest_call.ecx=base;run_original32();
                    FrontendSprites category_pool;category_pool.bind_timing(0x44,std::vector<FrontendSpriteTiming>(224,{240,60}));
                    FrontendUiResources category_ui{category_pool};std::array<std::uint8_t,0xe00> root_bytes{};
                    FrontendInputSnapshot input;FrontendCategoryServices svc{category_ui,Bytes(root_bytes.data(),root_bytes.size()),record,tables,input,repeat,&fonts,&table};
                    check(frontend_categories_display_4e8c60(child,svc),"native category display");
                    check(*reinterpret_cast<unsigned*>(sc)==9,"original category resource count");
                    for(unsigned i=0;i<9;++i){Bytes captured(reinterpret_cast<void*>(resources+i*64),64);
                        const auto off=captured.u32(0)-base;Bytes resource(child.object.data()+off,0xa0);auto args=captured.sub(4,44);
                        const auto* image=category_pool.get(resource.u32(8));
                        check(image&&image->token==args.u32(0)&&image->first==float(args.u32(4))&&image->last==float(args.u32(8))&&
                            resource.u32(8)/64==args.u32(12)&&image->mode==args.u32(16)&&resource.u32(0x40)==args.u32(20)&&resource.u32(0x44)==args.u32(24),"category original sprite/layer/frame/position");
                    }
                }
            }
        }
        // Key3B: execute the original controller, unlock graph and display.
        // Input/audio/GPU/text output are leaves; Races records and mapping are
        // the retail bytes. 4958C0 executes its original lookup and protected
        // bridge, with only the generic archive accessors returning those bytes.
        constexpr unsigned mission_root=base+0x35000,race_rows=base+0x37000,mapping=base+0x39000;
        if(mprotect(reinterpret_cast<void*>(0x836000),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("mission globals");
        RaceAssetPack races;RaceAssignmentPack assignment;std::string mission_error;
        if(!load_race_asset_pack_file("../OutRun2006 Coast 2 Coast (FXT)/Scripts/bin/Races.bin",races,&mission_error)||
           !load_race_assignment_pack_file("../OutRun2006 Coast 2 Coast (FXT)/Scripts/bin/RaceAssignment.bin",assignment,&mission_error))throw std::runtime_error(mission_error);
        std::memcpy(reinterpret_cast<void*>(race_rows),races.bytes.data()+races.races_offset,races.race_count*0x44);
        for(unsigned i=0;i<40;++i)*reinterpret_cast<unsigned*>(mapping+8*i)=assignment.menu_race_keys[i];
        *reinterpret_cast<unsigned*>(0x836370)=mapping;
        stub(0x4f1a90,race_rows);stub(0x4f1ba0,races.race_count);stub(0x4035f0,mission_root);stub(0x48ee80,0);
        capture(0x465eb0,tc,texts,0);
        LicenseProgressTables progress;progress.categories_ready=true;
        for(unsigned i=0;i<40;++i)for(unsigned j=0;j<races.race_count;++j)
            progress.category_counts[i]+=*reinterpret_cast<unsigned*>(race_rows+j*0x44)==assignment.menu_race_keys[i];
        std::memcpy(reinterpret_cast<void*>(counts),progress.category_counts.data(),40);
        for(unsigned category=0;category<40;++category)for(unsigned sub=1;sub<=progress.category_counts[category];++sub){
            prepare(0x4958c0);Bytes args(reinterpret_cast<void*>(S),8);args.put32(0,category);args.put32(4,sub);run_original32();
            unsigned t{};check(race_menu_type_4958c0(races,assignment,category,sub,t)&&guest_call.out_eax>=race_rows&&
                guest_call.out_eax<race_rows+races.race_count*0x44&&t==*reinterpret_cast<unsigned*>(guest_call.out_eax+0x20),"original mission race lookup");
        }
        check(!std::memcmp(reinterpret_cast<void*>(0x5ce4d8),mission_tables::backgrounds,sizeof(mission_tables::backgrounds)),"mission background table");
        check(!std::memcmp(reinterpret_cast<void*>(0x5ce6a8),mission_tables::grades,sizeof(mission_tables::grades)),"mission grades including PC sentinel descriptor");
        for(unsigned group=0;group<4;++group){
            const unsigned num=mission_tables::counts[group];
            check(!std::memcmp(reinterpret_cast<void*>(*reinterpret_cast<unsigned*>(0x69e848+4*group)),mission_tables::highlights[group],num*8),"mission highlight points");
            check(!std::memcmp(reinterpret_cast<void*>(*reinterpret_cast<unsigned*>(0x69e858+4*group)),mission_tables::positions[group],num*8),"mission node points");
            check(!std::memcmp(reinterpret_cast<void*>(*reinterpret_cast<unsigned*>(0x69e868+4*group)),mission_tables::neighbors[group],num*16),"mission neighbor graph");
            for(unsigned grade=0;grade<8;++grade)for(unsigned node=0;node<num;++node){
                PcLicense profile;frontend_license_reset_4471a0(profile,1);
                for(unsigned i=0;i<200;++i)profile[0x1dd+i]=std::uint8_t(grade);
                std::memcpy(reinterpret_cast<void*>(0x7c23e0),profile.data(),profile.size());
                FrontendMissionState child;FrontendCategoryState category;category.selection_84b7f4=node;
                unsigned repeat{},result{};frontend_missions_construct_4e9160(child,repeat);Bytes cb(child.object.data(),child.object.size());
                FrontendSprites sprites;sprites.bind_timing(0x44,std::vector<FrontendSpriteTiming>(224,{240,60}));
                FrontendUiResources mission_ui{sprites};mission_ui.effect_4249f0=[](void*,unsigned){return true;};
                std::array<std::uint8_t,0xe00> root{};Bytes rb(root.data(),root.size());
                FrontendInputSnapshot input;FrontendMissionServices svc{{mission_ui,rb,profile,progress,input,repeat,&fonts,&table},category,&races,&assignment};
                auto globals=[&]{rb.put32(0x20c,(0xabcdef&~0x700u)|(group<<8));
                    std::memcpy(reinterpret_cast<void*>(mission_root),root.data(),root.size());
                    *reinterpret_cast<unsigned*>(0x84b7f4)=node;category.selection_84b7f4=node;};
                globals();std::memcpy(reinterpret_cast<void*>(base),child.object.data(),child.object.size());
                *reinterpret_cast<unsigned*>(sc)=0;prepare(0x4e92d0);guest_call.ecx=base;run_original32();
                check(frontend_missions_init_4e92d0(child,svc),"mission native initialization");
                check(!std::memcmp(reinterpret_cast<void*>(base+0x3ffc),child.object.data()+0x3ffc,PcTextWidgetBytes),"mission original text widget initialization");
                for(unsigned detail=0;detail<2;++detail)for(unsigned action:{0u,1u,2u,3u,4u,5u,12u}){
                    globals();cb.put32(4,0xdead);cb.put8(0x34,std::uint8_t(detail));cb.put32(0x38,grade%5);
                    cb.put8(0xc,0);cb.put8(0xd,0);repeat=0;
                    std::memcpy(reinterpret_cast<void*>(base),child.object.data(),child.object.size());
                    stub(0x48f5f0,action,4);*reinterpret_cast<unsigned*>(sc)=0;
                    prepare(0x4e9360);guest_call.ecx=base;run_original32();const unsigned expected=guest_call.out_eax;
                    constexpr unsigned masks[]{4,8,0x400,0x1000,0x800,0x2000};input={};input.feature_mask=action<6?masks[action]:0;
                    check(frontend_missions_control_4e9360(child,svc,result),"mission native control");
                    check(result==expected&&cb.u32(4)==*reinterpret_cast<unsigned*>(base+4),"mission action and next child");
                    check(rb.u32(0x20c)==*reinterpret_cast<unsigned*>(mission_root+0x20c)&&category.selection_84b7f4==*reinterpret_cast<unsigned*>(0x84b7f4),"mission parent flags and map node");
                    check(cb.u8(0x34)==*reinterpret_cast<std::uint8_t*>(base+0x34)&&cb.u32(0x38)==*reinterpret_cast<unsigned*>(base+0x38),"mission detail mode and race cursor");
                }
                globals();sprites.reset();frontend_missions_construct_4e9160(child,repeat);frontend_missions_init_4e92d0(child,svc);
                std::memcpy(reinterpret_cast<void*>(base),child.object.data(),child.object.size());
                *reinterpret_cast<unsigned*>(sc)=0;*reinterpret_cast<unsigned*>(tc)=0;
                prepare(0x4e95a0);guest_call.ecx=base;run_original32();
                check(frontend_missions_display_4e95a0(child,svc),"mission native display");
                const unsigned draws=*reinterpret_cast<unsigned*>(sc);check(draws<64,"mission sprite capture bounded");
                for(unsigned i=0;i<draws&&i<64;++i){Bytes call(reinterpret_cast<void*>(resources+64*i),64);const unsigned off=call.u32(0)-base;
                    check(off>=0x3c&&off<0x3ffc,"mission resource receiver");if(off<0x3c||off>=0x3ffc)continue;
                    auto resource=cb.sub(off,0xa0);auto a=call.sub(4,44);
                    const bool forced=inject&&group==0&&grade==0&&node==0&&i==0;
                    check(!forced&&resource.u32(0)==a.u32(0)&&resource.u32(0xc)==a.u32(4)&&resource.u32(0x10)==a.u32(8)&&
                        resource.u32(0x14)==a.u32(12)&&resource.u32(0x18)==a.u32(16)&&resource.u32(0x40)==a.u32(20)&&resource.u32(0x44)==a.u32(24),"mission original sprite token/frame/layer/position");
                }
                const auto caption_id=*reinterpret_cast<unsigned*>(texts+4);const auto* caption=table.get(caption_id);
                check(*reinterpret_cast<unsigned*>(tc)==1&&caption&&*caption==frontend_text_value_48eec0(cb.sub(0x3ffc,PcTextWidgetBytes)),"mission original localized caption id");
                check(frontend_missions_suspend_4e9bb0(child,svc),"mission suspend");
                for(unsigned layer=0;layer<21;++layer)check(sprites.used(layer)==0,"mission resource lifecycle");
            }
        }
        std::cout<<"{\"routine\":\"frontend_license_panels\",\"cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<",\"graphics_and_completion_leaves_fixture\":true,\"completion_chain_separately_compared\":true,\"mission_controller_compared\":true}\n";
        return mismatches?1:0;
    }
    if(only=="frontend_glyph_render_probe"||only=="frontend_image_render_probe"){
        using namespace outrun::platform;
        constexpr unsigned base=0x3a800000;
        map_at(base,0x10000,PROT_READ|PROT_WRITE|PROT_EXEC);
        for(auto page:{0x95b000u,0x740000u})
            if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("glyph globals protection");
        Bytes mem(reinterpret_cast<void*>(base),0x10000);
        *reinterpret_cast<unsigned*>(0x95b218)=base+0x100;
        *reinterpret_cast<float*>(0x740c94)=1;*reinterpret_cast<float*>(0x740c98)=1;
        mem.put32(0x100,base+0x200);mem.put32(0x104,base+0x300);
        mem.put32(0x214,base+0x1000);mem.put32(0x224,base+0x1100);mem.put32(0x344,base+0x1200);
        // Device methods are data capture only; original 42A0A0 is untouched.
        const std::uint8_t noop[]={0x31,0xc0,0xc2,8,0};r060_raw_write(base+0x1000,noop,sizeof(noop));
        auto emit=[&](unsigned at,std::vector<std::uint8_t> code){r060_raw_write(at,code.data(),code.size());};
        auto imm=[](std::vector<std::uint8_t>& c,unsigned v){for(unsigned i=0;i<4;++i)c.push_back(std::uint8_t(v>>(8*i)));};
        std::vector<std::uint8_t> draw{0x56,0x57,0x8b,0x74,0x24,0x14,0xbf};imm(draw,base+0x60);
        draw.insert(draw.end(),{0xb9,4,0,0,0,0xf3,0xa5,0x8b,0x44,0x24,0x20,0xa3});imm(draw,base+0x70);
        draw.insert(draw.end(),{0x5f,0x5e,0x31,0xc0,0xc2,24,0});emit(base+0x1100,draw);
        std::vector<std::uint8_t> desc{0x8b,0x54,0x24,0x0c,0xa1};imm(desc,base+0x40);
        desc.insert(desc.end(),{0x89,0x42,0x18,0xa1});imm(desc,base+0x44);
        desc.insert(desc.end(),{0x89,0x42,0x1c,0x31,0xc0,0xc2,12,0});emit(base+0x1200,desc);
        std::vector<std::uint8_t> matrix{0x8b,0x44,0x24,0x10,0x8b,0x10,0x89,0x15};imm(matrix,base+0x48);
        matrix.insert(matrix.end(),{0x8b,0x50,4,0x89,0x15});imm(matrix,base+0x4c);
        matrix.insert(matrix.end(),{0x8b,0x44,0x24,0x1c,0x8b,0x10,0x89,0x15});imm(matrix,base+0x50);
        matrix.insert(matrix.end(),{0x8b,0x50,4,0x89,0x15});imm(matrix,base+0x54);
        matrix.insert(matrix.end(),{0xc2,28,0});emit(0x439400,matrix);
        unsigned cases=0,mismatches=0;const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
        auto check=[&](bool ok,const char* message){++cases;if(!ok||(inject&&cases==1)){
            ++mismatches;if(mismatches<10)std::cerr<<"glyph case "<<cases<<": "<<message<<"\n";}};
        auto near=[](float a,float b){return std::abs(a-b)<0.001f;};
        if(only=="frontend_image_render_probe"){
            map_original_arithmetic_bridge(file);
            if(mprotect(reinterpret_cast<void*>(0x956000),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("image globals protection");
            auto bank_record=Bytes(reinterpret_cast<void*>(0x956d88+3*44),44);
            bank_record.put32(0,2);bank_record.put32(4,base+0x3000);bank_record.put32(8,base+0x5000);
            mem.put32(0x3018,117);mem.put32(0x301c,base+0x4000);
            mem.put32(0x5000+11*4,base+0x104);
            // Retail bank-3 border/cursor crop records, represented as a small
            // controlled metadata fixture. The XST parser is exercised with
            // the complete retail archive by test_frontend_menu_render.
            const unsigned crops[16][4]={{0,34,4,64},{4,38,110,64},{110,38,119,64},{119,38,123,64},
                {0,18,11,34},{11,18,20,34},{20,18,22,34},{22,19,36,34},{36,19,50,34},
                {50,19,62,34},{62,24,69,34},{69,26,78,34},{78,28,82,34},{82,30,86,34},{86,32,92,34},{92,32,95,34}};
            FrontendImageBank bank;bank.regions.resize(117);bank.textures.resize(13);
            bank.textures[11].width=128;bank.textures[11].height=64;mem.put32(0x40,128);mem.put32(0x44,64);
            for(unsigned i=0;i<16;++i){auto& r=bank.regions[64+i];r.texture=11;
                const auto off=0x4000+(64+i)*28;mem.put32(off,11);
                for(unsigned j=0;j<4;++j){r.crop[j]=crops[i][j];const std::uint16_t v=std::uint16_t(crops[i][j]);std::memcpy(reinterpret_cast<void*>(base+off+20+j*2),&v,2);}}
            auto capture=[&](unsigned pc,unsigned dest,unsigned words){
                std::vector<std::uint8_t> c{0x56,0x57,0x8b,0x74,0x24,0x0c,0xbf};imm(c,base+dest);
                c.push_back(0xb9);imm(c,words);c.insert(c.end(),{0xf3,0xa5,0x5f,0x5e,0xc3});emit(pc,c);};
            capture(0x42cfe0,0x6000,18);capture(0x42d0c0,0x7000,46);
            // Continue the captured material through its actual 42D9F4
            // consumer. 42A3A0 permutes UVs and adds half-texel offsets;
            // comparing the queued material alone misses both operations.
            for(auto page:{0x89b000u,0x98b000u})if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("image device globals");
            *reinterpret_cast<unsigned*>(0x89bd60)=base+0x800;
            mem.put32(0x800,base+0x900);mem.put32(0xa04,base+0x1300);mem.put32(0xa10,base+0x1400);
            mem.put32(0xa14,base+0x1500);mem.put32(0xa4c,base+0x1600);
            emit(0x42a2b0,{0xc3}); // render-state calls only
            emit(base+0x1300,{0x31,0xc0,0xc2,12,0});
            emit(base+0x1400,{0x8b,0x44,0x24,0x10,0xc7,0x00,3,0,0,0,0x31,0xc0,0xc2,16,0});
            emit(base+0x1500,{0x31,0xc0,0xc2,16,0});emit(base+0x1600,{0x31,0xc0,0xc2,20,0});
            // D3DXVec4Transform with the identity matrix authored by 42D300.
            emit(0x4393b2,{0x56,0x57,0x8b,0x7c,0x24,0x0c,0x8b,0x74,0x24,0x10,0xb9,4,0,0,0,0xf3,0xa5,0x5f,0x5e,0xc2,12,0});
            Bytes args(reinterpret_cast<void*>(S),64);const unsigned order[4]={0,2,3,1};
            for(unsigned index=64;index<80;++index)for(int x:{-37,0,125,639})for(int y:{-15,0,77,479})
            for(unsigned mode:{0u,1u,2u,3u,4u})for(float width:{0.f,1.f,37.5f,480.f}){
                FrontendListImage im;im.pc=0x42d300;im.token=0x30000+index;im.mode=mode;
                im.x=x;im.y=y;im.width=width;im.height=width*.5f;im.layer=5;im.color=0x9a12cd34;
                args.put32(0,mode);args.put32(4,im.token);args.puti(8,x);args.puti(12,y);
                args.putf(16,im.width);args.putf(20,im.height);args.put32(24,0);args.putf(28,im.layer);args.put32(32,im.color);
                prepare(im.pc);run();FrontendGlyphQuad q;unsigned texture{};
                check(frontend_image_quad(im,bank,q,texture)&&texture==11,"image quad");
                const auto m=mem.sub(0x7000,184);
                check(m.u32(0)==0x3000b&&m.u32(4)==im.color&&m.u32(8)==0x45&&m.u32(12)==base+0x104,"image material");
                prepare(0x42a3a0);guest_call.ebx=base+0x7000;run();const Bytes vertices(reinterpret_cast<void*>(0x98b868),112);
                for(unsigned j=0;j<4;++j){const auto k=order[j];
                    check(near((q[j].position[0]+.675f)/1.35f*640,vertices.f32(k*28))&&
                          near((.9f-q[j].position[1])/1.8f*480,vertices.f32(k*28+4)),"image position");
                    check(fbits(q[j].uv[0])==vertices.u32(k*28+20)&&fbits(q[j].uv[1])==vertices.u32(k*28+24),"image UV");}
            }
            for(unsigned index=64;index<80;++index)for(unsigned flip=0;flip<8;++flip)for(int x:{-37,0,125,639}){
                FrontendListImage im;im.pc=0x42d280;im.token=0x30000+index;im.x=x;im.y=77;im.frame=int(flip);im.layer=5;im.color=0x9a12cd34;
                args.put32(0,im.token);args.puti(4,x);args.puti(8,im.y);args.put32(12,flip);args.putf(16,im.layer);args.put32(20,im.color);
                prepare(im.pc);run();auto d=mem.sub(0x6000,72);check(d.u32(0)==0x3000b&&d.u32(52)==(flip&3),"image descriptor/flip");
                d.put32(56,base+0x104);prepare(0x42a0a0);guest_call.esi=base+0x6000;run();
                FrontendGlyphQuad q;unsigned texture{};check(frontend_image_quad(im,bank,q,texture),"unscaled image quad");
                const float sx=mem.f32(0x48),sy=mem.f32(0x4c),tx=mem.f32(0x50),ty=mem.f32(0x54);
                const float x1=tx+(mem.i32(0x68)-mem.i32(0x60))*sx,y1=ty+(mem.i32(0x6c)-mem.i32(0x64))*sy;
                check(near((q[0].position[0]+.675f)/1.35f*640,std::min(tx,x1))&&near((q[2].position[0]+.675f)/1.35f*640,std::max(tx,x1))&&
                      near((.9f-q[0].position[1])/1.8f*480,std::min(ty,y1))&&near((.9f-q[2].position[1])/1.8f*480,std::max(ty,y1)),"unscaled image bounds");
                check(near(q[0].uv[0],float(mem.i32(sx>=0?0x60:0x68))/128)&&near(q[2].uv[0],float(mem.i32(sx>=0?0x68:0x60))/128)&&
                      near(q[0].uv[1],float(mem.i32(sy>=0?0x64:0x6c))/64)&&near(q[2].uv[1],float(mem.i32(sy>=0?0x6c:0x64))/64),"unscaled image UV");
            }
            std::cout<<"{\"routine\":\"frontend_image_render\",\"cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<",\"material_and_device_leaves_captured\":true}\n";
            return mismatches?1:0;
        }
        GameUiTexture t;t.width=512;t.height=256;mem.put32(0x40,t.width);mem.put32(0x44,t.height);
        for(int left:{0,1,32,100})for(int top:{0,3,32,100})for(int width:{0,1,2,3,16,64})
        for(int height:{1,2,16,64})for(float sx:{-1.f,0.f,.5f,1.f,2.f})for(float sy:{-.5f,0.f,.5f,1.f,2.f}){
            FrontendGlyph g{9,left,top,left+width,top+height,sx,sy,121.25f,83.75f,0x9a12cd34,4};
            std::memset(reinterpret_cast<void*>(base+0x400),0,72);auto d=mem.sub(0x400,72);
            d.put32(0,g.token);d.puti(4,g.left);d.puti(8,g.top);d.puti(12,g.right);d.puti(16,g.bottom);
            d.putf(20,sx);d.putf(24,sy);d.putf(36,g.x);d.putf(40,g.y);d.put32(44,g.color);d.put32(56,base+0x104);
            prepare(0x42a0a0);guest_call.esi=base+0x400;run();
            FrontendGlyphQuad q;check(frontend_glyph_quad(g,t,q),"native quad");
            const float x0=(q[0].position[0]+.675f)/1.35f*640.f;
            const float x1=(q[1].position[0]+.675f)/1.35f*640.f;
            const float y0=(.9f-q[0].position[1])/1.8f*480.f;
            const float y1=(.9f-q[2].position[1])/1.8f*480.f;
            check(near(x0,mem.f32(0x50))&&near(x1,mem.f32(0x50)+(mem.i32(0x68)-mem.i32(0x60))*mem.f32(0x48)),"PC X bounds");
            check(near(y1,mem.f32(0x54))&&near(y0,mem.f32(0x54)+(mem.i32(0x6c)-mem.i32(0x64))*mem.f32(0x4c)),"PC Y bounds");
            check(near(q[0].uv[0],float(mem.i32(0x60))/t.width)&&near(q[2].uv[0],float(mem.i32(0x68))/t.width)&&
                  near(q[0].uv[1],float(mem.i32(0x6c))/t.height)&&near(q[2].uv[1],float(mem.i32(0x64))/t.height),"PC crop orientation");
            check(mem.u32(0x70)==g.color&&near(q[0].color[0],18.f/255)&&near(q[0].color[1],205.f/255)&&
                  near(q[0].color[2],52.f/255)&&near(q[0].color[3],154.f/255),"ARGB channels");
        }
        std::cout<<"{\"routine\":\"frontend_glyph_render\",\"cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<",\"device_and_matrix_leaves_captured\":true}\n";
        return mismatches?1:0;
    }
    if(only=="frontend_keyboard_probe"){
        using namespace outrun::platform;
        constexpr unsigned base=0x33500000;
        map_at(base,0x10000,PROT_READ|PROT_WRITE);
        map_original_arithmetic_bridge(file);
        for(auto page:{0x7f9000u,0x95a000u,0x64c000u})
            if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("keyboard globals protection");
        auto stub=[](unsigned pc,unsigned value,unsigned short pop=0){
            std::array<std::uint8_t,8> code{{0xb8,0,0,0,0,0xc2,0,0}};
            std::memcpy(code.data()+1,&value,4);std::memcpy(code.data()+6,&pop,2);r060_raw_write(pc,code.data(),code.size());};
        for(auto pc:{0x465250u,0x465970u,0x4659f0u})stub(pc,0);
        stub(0x4652e0,1);stub(0x465860,0,44);stub(0x4653c0,0,16);
        const unsigned sound_at=base+0x2000;
        std::vector<std::uint8_t> sound_code{0x8b,0x44,0x24,0x04,0xa3};
        auto imm=[&](unsigned u){for(unsigned i=0;i<4;++i)sound_code.push_back(std::uint8_t(u>>(i*8)));};
        imm(sound_at);sound_code.insert(sound_code.end(),{0xff,0x05});imm(sound_at+4);sound_code.push_back(0xc3);
        r060_raw_write(0x4249f0,sound_code.data(),sound_code.size());
        unsigned cases=0,mismatches=0;const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
        auto check=[&](bool ok,const char* message){++cases;if(!ok||(inject&&cases==1)){
            ++mismatches;if(mismatches<10)std::cerr<<"keyboard case "<<cases<<": "<<message<<"\n";}};
        FrontendSprites pool;std::vector<FrontendSpriteTiming> timing(319,{60,60});pool.bind_timing(0x2c,timing);
        FrontendUiResources ui{pool};
        struct Input {int action=-1;unsigned sound=~0u,count=0;} in;
        FrontendKeyboardServices s{&in,
            [](void* p,Bytes,int& a){a=static_cast<Input*>(p)->action;return true;},
            [](void* p,unsigned sound){auto& i=*static_cast<Input*>(p);i.sound=sound;++i.count;return true;},
            [](void*,bool){return true;}};
        std::array<std::uint8_t,PcKeyboardBytes> object{};Bytes b(object.data(),object.size());unsigned repeat{},result{};
        auto copy_guest=[&]{std::memcpy(reinterpret_cast<void*>(base),object.data(),object.size());};
        auto compare_core=[&]{const auto* g=reinterpret_cast<const std::uint8_t*>(base);
            const bool ok=!std::memcmp(g,object.data(),0x4c)&&!std::memcmp(g+0xec,object.data()+0xec,4)&&
                !std::memcmp(g+0x550,object.data()+0x550,4)&&!std::memcmp(g+0x5f4,object.data()+0x5f4,PcKeyboardBytes-0x5f4);
            if(!ok&&mismatches<5)for(unsigned i=0;i<object.size();++i)
                if((i<0x4c||(i>=0xec&&i<0xf0)||(i>=0x550&&i<0x554)||i>=0x5f4)&&g[i]!=object[i])
                    std::cerr<<std::hex<<i<<"="<<unsigned(g[i])<<"/"<<unsigned(object[i])<<std::dec<<" ";
            check(ok,"controller/name bytes");};
        keyboard_construct_468e40(b,repeat);
        for(unsigned pattern:{0u,0x5au,0xa5u}){
            object.fill(std::uint8_t(pattern));keyboard_construct_468e40(b,repeat);copy_guest();
            *reinterpret_cast<unsigned*>(0x95aec4)=0;*reinterpret_cast<unsigned*>(0x95af38)=0;
            prepare(0x468880);guest_call.ecx=base;run();pool.reset();s.focused=false;
            check(keyboard_init_468880(b,ui,s),"native init");compare_core();
            check((guest_call.out_eax&255)==1&&*reinterpret_cast<unsigned*>(0x95af38)==base,"original init/active keyboard");
        }
        for(unsigned length:{0u,1u,14u,15u,16u,254u,255u,256u}){
            const std::string name(length,'N');std::memcpy(reinterpret_cast<void*>(base+0x3000),name.c_str(),name.size()+1);
            copy_guest();prepare(0x468710);guest_call.ecx=base;Bytes(reinterpret_cast<void*>(S),4).put32(0,base+0x3000);run_original32();
            check(guest_call.out_sp==S+4,"name stack");check(keyboard_name_468710(b,name),"native name");compare_core();
        }
        for(unsigned alphabet=1;alphabet<=5;++alphabet)for(unsigned cell=0;cell<38;++cell){
            b.put32(0xec,alphabet);copy_guest();prepare(0x468d40);guest_call.ecx=base;Bytes(reinterpret_cast<void*>(S),4).put32(0,cell);
            run_original32();check(guest_call.out_sp==S+4,"character stack");check((guest_call.out_eax&255)==keyboard_character_468d40(b,cell),"retail character table");
        }
        for(int maximum:{0,1,2,16,255,256,500})for(int minimum:{-1,0,1,16,300}){
            copy_guest();prepare(0x468780);guest_call.ecx=base;Bytes a(reinterpret_cast<void*>(S),8);a.puti(0,minimum);a.puti(4,maximum);
            run_original32();check(guest_call.out_sp==S+8,"limit stack");keyboard_limits_468780(b,minimum,maximum);compare_core();
        }
        for(unsigned length:{0u,1u,14u,15u,16u,254u,255u,256u})for(int limit:{1,16,255}){
            keyboard_name_468710(b,std::string(length,'x'));keyboard_limits_468780(b,1,limit);copy_guest();
            prepare(0x468d80);guest_call.ecx=base;Bytes(reinterpret_cast<void*>(S),4).put32(0,'Z');run_original32();
            check(guest_call.out_sp==S+4,"append stack");keyboard_append_468d80(b,'Z');compare_core();
        }
        for(unsigned length:{0u,1u,14u,15u})for(unsigned minimum:{0u,1u,16u})for(unsigned accept:{0u,1u}){
            pool.reset();s.focused=false;in={};object.fill(0);keyboard_construct_468e40(b,repeat);keyboard_init_468880(b,ui,s);
            keyboard_name_468710(b,std::string(length,'x'));keyboard_limits_468780(b,minimum,16);copy_guest();
            prepare(0x469030);guest_call.ecx=base;Bytes(reinterpret_cast<void*>(S),4).put32(0,accept);run_original32();
            check(guest_call.out_sp==S+4,"finish stack");check(keyboard_finish_469030(b,ui,s,accept!=0),"native finish");compare_core();
        }
        // Exercise all cells/actions/alphabets in both navigation graphs and
        // each disabled-alphabet mask. UI state starts at a completed opening.
        for(unsigned flags:{0u,2u,4u,8u,16u,30u})for(unsigned alphabet=1;alphabet<=5;++alphabet)
        for(unsigned cell=0;cell<44;++cell)for(int action=-1;action<=5;++action)for(unsigned length:{0u,14u,15u}){
            pool.reset();s.focused=false;in={};object.fill(0);keyboard_construct_468e40(b,repeat);
            if(!keyboard_init_468880(b,ui,s))throw std::runtime_error("native keyboard init");
            for(unsigned i=0;i<60;++i)pool.tick();keyboard_tick_469130(b,ui,s,result);
            b.put32(0x708,flags);b.put32(0xec,alphabet);keyboard_limits_468780(b,1,16);keyboard_name_468710(b,std::string(length,'x'));
            s.selected=cell;in={action,~0u,0};copy_guest();
            *reinterpret_cast<unsigned*>(0x7f9420)=cell;*reinterpret_cast<unsigned*>(0x64c838)=~0u;
            *reinterpret_cast<unsigned*>(0x95af38)=base;*reinterpret_cast<unsigned*>(sound_at)=~0u;*reinterpret_cast<unsigned*>(sound_at+4)=0;
            stub(0x48f5f0,unsigned(action),4);prepare(0x469130);guest_call.ecx=base;run();
            check(keyboard_tick_469130(b,ui,s,result),"native tick");compare_core();
            check(guest_call.out_eax==result,"action result");check(*reinterpret_cast<unsigned*>(0x7f9420)==s.selected,"navigation graph");
            check(*reinterpret_cast<unsigned*>(sound_at)==in.sound&&*reinterpret_cast<unsigned*>(sound_at+4)==in.count,"sound decisions");
        }
        std::cout<<"{\"routine\":\"frontend_keyboard\",\"cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<",\"graphics_leaves_stubbed\":true}\n";
        return mismatches?1:0;
    }
    if(only=="frontend_text_probe"){
        using namespace outrun::platform;
        constexpr unsigned base=0x33400000u;
        map_at(base,0x10000,PROT_READ|PROT_WRITE);
        map_original_arithmetic_bridge(file);
        for(auto page:{0x956000u,0x76f000u,0x830000u})
            if(mprotect(reinterpret_cast<void*>(page),0x1000,PROT_READ|PROT_WRITE))throw std::runtime_error("text globals protection");
        Bytes globals(reinterpret_cast<void*>(0x956ba0),0x50),fixture(reinterpret_cast<void*>(base),0x10000);
        unsigned cases=0,mismatches=0;
        const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
        auto check=[&](bool ok,const char* what){++cases;if(!ok||(inject&&cases==1)){
            ++mismatches;if(mismatches<12)std::cerr<<"text case "<<cases<<" "<<what<<"\n";}};
        // The sole draw leaf captures the original 72-byte sprite submission.
        // 42C860 itself (bearing, source crop, clipping) executes unchanged.
        std::vector<std::uint8_t> capture{0x56,0x57,0x8b,0x74,0x24,0x0c,0xbf};
        auto imm=[&](unsigned u){for(unsigned i=0;i<4;++i)capture.push_back(std::uint8_t(u>>(8*i)));};
        imm(base+0x5000);capture.push_back(0xb9);imm(18);
        capture.insert(capture.end(),{0xf3,0xa5,0x5f,0x5e,0xb8});imm(1);capture.push_back(0xa3);imm(base+0x4800);capture.push_back(0xc3);
        r060_raw_write(0x42cfe0,capture.data(),capture.size());
        auto setup=[&](const FrontendFont& f,Bytes descriptor,const FrontendTextStyle& st,const FrontendTextCursor& c){
            globals.put32(0,1);globals.put32(4,descriptor.u32(4));globals.put32(8,descriptor.u32(8));globals.put32(12,f.token);
            globals.put16(0x14,c.origin_x);globals.put16(0x18,c.x);globals.put16(0x1a,c.y);
            globals.put16(0x1c,f.width);globals.put16(0x1e,f.height);globals.put16(0x20,std::uint16_t(f.metrics.size()));
            globals.putf(0x24,st.scale_x);globals.putf(0x28,st.scale_y);globals.put32(0x2c,st.color);globals.put32(0x30,st.mode);
            globals.put32(0x34,f.first);globals.put32(0x38,st.flags);globals.putf(0x3c,f.spacing_x);globals.putf(0x40,f.spacing_y);
            *reinterpret_cast<int*>(0x95b21c)=st.clip_left;*reinterpret_cast<int*>(0x76f7e4)=st.clip_right;
        };
        if(mprotect(reinterpret_cast<void*>(0x95b000),0x1000,PROT_READ|PROT_WRITE))throw std::runtime_error("text clip protection");
        for(unsigned fi=0;fi<10;++fi){
            const auto address=*reinterpret_cast<unsigned*>(0x76f7b8+4*fi);Bytes d(reinterpret_cast<void*>(address),32);
            FrontendFont f;f.token=d.u32(0);f.width=d.i16(12);f.height=d.i16(14);f.first=d.u32(16);f.spacing_x=float(d.i32(20));f.spacing_y=float(d.i32(24));
            const auto count=d.u32(8)?d.u32(28):128-f.first;f.metrics.resize(count);
            std::memcpy(f.metrics.data(),reinterpret_cast<void*>(d.u32(4)),count*2);
            if(d.u32(8)){f.kerning.resize(count*count);std::memcpy(f.kerning.data(),reinterpret_cast<void*>(d.u32(8)),count*count);}
            for(float scale:{0.5f,1.0f,1.25f,2.0f})for(int x:{-20,-1,0,200,630,645}){
                FrontendTextStyle st;st.scale_x=scale;st.scale_y=0.75f;st.flags=0x101;FrontendTextCursor c{42,std::int16_t(x),120};
                for(unsigned ch=std::max(32u,f.first);ch<f.first+count;++ch){
                    setup(f,d,st,c);fixture.put32(0x4800,0);prepare(0x42c860);guest_call.eax=ch;run();
                    FrontendGlyph g;const bool draw=frontend_text_glyph_42c860(f,st,c,std::uint8_t(ch),g);Bytes pcg(reinterpret_cast<void*>(base+0x5000),72);
                    bool ok=draw==(fixture.u32(0x4800)!=0);
                    if(draw)ok=ok&&pcg.u32(0)==g.token&&pcg.i32(4)==g.left&&pcg.i32(8)==g.top&&pcg.i32(12)==g.right&&pcg.i32(16)==g.bottom&&
                        pcg.f32(20)==g.scale_x&&pcg.f32(24)==g.scale_y&&pcg.f32(36)==g.x&&pcg.f32(40)==g.y&&pcg.u32(44)==g.color;
                    check(ok,"glyph crop/bearing/clip");
                    for(unsigned next:{0u,32u,65u,86u}){
                        setup(f,d,st,c);prepare(0x42c610);guest_call.eax=ch;guest_call.ecx=next;run();auto nc=c;
                        frontend_text_advance_42c610(f,st,nc,std::uint8_t(ch),std::uint8_t(next));
                        check(globals.i16(0x18)==nc.x&&globals.i16(0x1a)==nc.y,"kerning advance");
                    }
                }
                // 42C720 / 42C5A0 (in-race 42CCC0): every byte value, monospace.
                for(unsigned ch=0;ch<0x100;ch+=(ch<0x80?1u:37u)){
                    setup(f,d,st,c);fixture.put32(0x4800,0);prepare(0x42c720);guest_call.eax=ch;run();
                    FrontendGlyph g;const bool draw=frontend_text_glyph_42c720(f,st,c,std::uint8_t(ch),g);Bytes pcg(reinterpret_cast<void*>(base+0x5000),72);
                    bool ok=draw==(fixture.u32(0x4800)!=0);
                    if(draw){ok=ok&&pcg.u32(0)==g.token&&pcg.i32(4)==g.left&&pcg.i32(8)==g.top&&pcg.i32(12)==g.right&&pcg.i32(16)==g.bottom&&
                        pcg.f32(20)==g.scale_x&&pcg.f32(24)==g.scale_y&&pcg.f32(36)==g.x&&pcg.f32(40)==g.y&&pcg.u32(44)==g.color;
                        for(unsigned o:{28u,32u})ok=ok&&pcg.u32(o)==0u;
                        for(unsigned o=48;o<72;o+=4)ok=ok&&pcg.u32(o)==0u;}
                    check(ok,"42C720 monospace glyph");
                    setup(f,d,st,c);prepare(0x42c5a0);guest_call.eax=ch;run();auto nc=c;
                    frontend_text_advance_42c5a0(f,st,nc,std::uint8_t(ch));
                    check(globals.i16(0x18)==nc.x&&globals.i16(0x1a)==nc.y,"42C5A0 monospace advance");
                }
                for(unsigned flags:{1u,0x100u,0x104u})for(const auto* text:{"OR2C2C","CREATE NEW LICENSE","AV AVA  W","Hello\nWorld","0123456789"}){
                    st.flags=flags;setup(f,d,st,c);std::strcpy(reinterpret_cast<char*>(base+0x1000),text);
                    prepare(0x42c480);guest_call.eax=base+0x1000;Bytes(reinterpret_cast<void*>(S),4).putf(0,0.3f);run();
                    check(int(guest_call.out_eax)==frontend_text_width_42c480(f,st,text),"string measurement");
                }
                for(unsigned flags:{0x101u,0x104u,0x102u,0x109u,0x111u}){
                    st.flags=flags;setup(f,d,st,c);std::strcpy(reinterpret_cast<char*>(base+0x1000),"OR2C2C");
                    prepare(0x42c390);guest_call.eax=base+0x1000;run();
                    FrontendTextCursor aligned{c.origin_x,globals.i16(0x18),globals.i16(0x1a)};
                    std::vector<FrontendGlyph> output;check(frontend_text_draw(f,st,c,"OR2C2C",output),"text draw completed");
                    FrontendGlyph first;const bool visible=frontend_text_glyph_42c860(f,st,aligned,'O',first);
                    // Off-screen initial glyphs can be clipped; compare alignment
                    // only where the first glyph is actually submitted.
                    if(visible)check(!output.empty()&&output[0].x==first.x&&output[0].y==first.y,"original text alignment");
                }
            }
            if(f.token!=9)continue;
            for(int width:{8,40,75,150,400})for(int skip:{0,1,3})for(const auto* text:{"OR2C2C","CREATE NEW LICENSE","AA-BB CC\nDD","AV AVA  W","LONGUNBROKENSTRINGWITHLETTERS"}){
                std::array<std::uint8_t,PcTextWidgetBytes> obj{};Bytes b(obj.data(),obj.size());frontend_text_init_48e640(b);
                frontend_text_set_48f280(b,text,9,~0u);b.puti(0x468,width);b.puti(0x488,skip);
                std::memcpy(reinterpret_cast<void*>(base),obj.data(),obj.size());std::memset(reinterpret_cast<void*>(0x8303e8),0,2048);
                setup(f,d,FrontendTextStyle{},{});prepare(0x48eb60);guest_call.ecx=base;run();FrontendTextLines lines;
                check(frontend_text_wrap_48eb60(b,f,lines),"native wrap completed");
                for(unsigned line=0;line<16;++line)check(!std::strcmp(reinterpret_cast<char*>(0x8303e8+line*128),lines.lines[line].data()),"wrapped line");
            }
        }
        for(unsigned pc:{0x48e640u,0x48e530u,0x48eed0u,0x48eee0u,0x48ef30u,0x48ef40u,0x48ef50u,0x48ee60u}){
            const unsigned n=pc==0x48eee0?4:pc==0x48e530||pc==0x48ef50?2:pc==0x48e640||pc==0x48ee60?0:1;
            for(unsigned seed=0;seed<12;++seed){std::array<std::uint8_t,PcTextWidgetBytes> obj;obj.fill(std::uint8_t(seed*19));
                std::memcpy(reinterpret_cast<void*>(base),obj.data(),obj.size());unsigned args[]{seed,seed*5,seed*10,seed*20};
                prepare(pc);guest_call.ecx=base;std::memcpy(reinterpret_cast<void*>(S),args,n*4);run_original32();
                if(guest_call.out_sp!=S+n*4)throw std::runtime_error("widget setter stack imbalance");
                check(frontend_text_setter(pc,Bytes(obj.data(),obj.size()),args,n)&&!std::memcmp(reinterpret_cast<void*>(base),obj.data(),obj.size()),"widget setter bytes");
            }
        }
        for(int limit:{8,16,31,64,128,1024})for(const auto* text:{"ONE TWO THREE FOUR", "ONE<newline>TWO", "LONGUNBROKENSTRINGWITHLETTERS", "ONE TWO THREE<newline>FOUR FIVE"}){
            std::array<std::uint8_t,PcTextWidgetBytes> obj{};Bytes b(obj.data(),obj.size());frontend_text_init_48e640(b);b.puti(0x45c,limit);
            frontend_text_set_48f280(b,text,9,~0u);std::memcpy(reinterpret_cast<void*>(base),obj.data(),obj.size());
            std::memset(reinterpret_cast<void*>(0x8303e8),0,2048);prepare(0x48ea50);guest_call.ecx=base;
            Bytes(reinterpret_cast<void*>(S),4).put32(0,base+0x4e);run_original32();
            if(guest_call.out_sp!=S+4)throw std::runtime_error("split stack imbalance");
            FrontendTextLines lines;check(frontend_text_split_48ea50(b,text,lines),"split completed");
            for(unsigned line=0;line<16;++line)check(!std::strcmp(reinterpret_cast<char*>(0x8303e8+line*128),lines.lines[line].data()),"split line");
            check(b.i32(0x45c)==fixture.i32(0x45c),"split limit mutation");
        }
        // Actual composite panel constructors and initialization, not stubs for
        // the 21 text widgets / 12 resource constructors inside the chooser.
        // Remap the three Windows FS:0 SEH-chain accesses to private TLS storage
        // in the test process. Constructor bodies and all children stay intact.
        for(auto address:{0x4e0ab7u,0x4e0abeu,0x4e0b7fu}){
            std::array<std::uint8_t,7> code{};const auto size=address==0x4e0ab7u?6u:7u;
            std::memcpy(code.data(),reinterpret_cast<void*>(address),size);code[0]=0x90;
            const unsigned tls=base+0xfff0;std::memcpy(code.data()+size-4,&tls,4);r060_raw_write(address,code.data(),size);
        }
        if(mprotect(reinterpret_cast<void*>(0x659000),0x1000,PROT_READ|PROT_WRITE))throw std::runtime_error("text repeat protection");
        for(unsigned seed:{0u,0x5au,0xa5u}){
            std::vector<std::uint8_t> obj(PcLicenseChooserBytes,std::uint8_t(seed));std::memcpy(reinterpret_cast<void*>(base),obj.data(),obj.size());
            FrontendSprites pool;FrontendUiResources ui{pool};FrontendFontPack fonts;FrontendTextTable text;
            FrontendLicenseWidgets widgets(Bytes(obj.data(),obj.size()),ui,fonts,text);unsigned repeat{};
            for(unsigned i=0;i<3;++i){const unsigned off=0x3580+i*0x225c;
                prepare(0x4e0ab0);guest_call.ecx=base+off;run();check(widgets.construct_panel_4e0ab0(off,repeat),"panel construction");
                check(!std::memcmp(reinterpret_cast<void*>(base),obj.data(),obj.size()),"panel constructor bytes");
            }
            prepare(0x4e0c70);guest_call.ecx=base;run();check(widgets.initialize_panels_4e0c70(),"panel initialization");
            check(!std::memcmp(reinterpret_cast<void*>(base),obj.data(),obj.size()),"panel init bytes");
        }
        std::cout<<"{\"routine\":\"frontend_text\",\"cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<",\"gpu_submission_captured\":true,\"panel_seh_tls_remapped\":true}\n";
        return mismatches?1:0;
    }
    if(only=="frontend_sprites_probe"){
        using namespace outrun::platform;
        constexpr unsigned base=0x33300000u,pool_base=0x95e028u;
        map_at(base,0x1000,PROT_READ|PROT_WRITE);
        for(auto page:{0x956000u,0x95b000u,0x740000u,0x8a8000u})
            if(mprotect(reinterpret_cast<void*>(page),page==0x956000u?0x31000u:0x1000u,PROT_READ|PROT_WRITE))
                throw std::runtime_error("sprite globals protection");
        Bytes source(reinterpret_cast<void*>(base),0x1000);
        source.put32(0,1);source.put32(4,base+0x40);source.putf(0x48,52);source.putf(0x4c,60);
        source.put32(0x100,base);
        *reinterpret_cast<unsigned*>(0x9568b8u+0x44u*4u)=base+0x100;
        unsigned cases=0,mismatches=0;
        const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
        auto compare=[&](const FrontendSprites& pool,unsigned handle){
            ++cases;const auto* s=pool.get(handle);Bytes g(reinterpret_cast<void*>(pool_base+handle*0x7c),0x7c);
            const auto eq=[](float x,float y){return std::memcmp(&x,&y,4)==0;};
            const bool ok=g.u32(0)==unsigned(s->allocated)&&eq(g.f32(0xc),s->frame)&&
                g.u32(0x10)==s->mode&&eq(g.f32(0x18),s->first)&&eq(g.f32(0x1c),s->last)&&
                g.u32(0x24)==s->status&&*reinterpret_cast<unsigned*>(0x956558u+handle/64*4u)==pool.used(handle/64);
            if(!ok||(inject&&cases==1)){
                if(++mismatches<5)std::cerr<<"sprite case "<<cases<<" frame "<<g.f32(0xc)<<"/"<<s->frame<<" status "<<g.u32(0x24)<<"/"<<s->status<<"\n";
            }
        };
        FrontendSprites pool;pool.bind_timing(0x44,{{52,60}});
        for(unsigned mode=0;mode<8;++mode)for(int direction:{-1,1})
        for(float speed:{-2.0f,-1.0f,0.0f,0.5f,1.0f,3.0f})for(unsigned clock=0;clock<8;++clock){
            pool.reset();std::memset(reinterpret_cast<void*>(pool_base),0,0x7c*FrontendSprites::Count);
            std::memset(reinterpret_cast<void*>(0x956500),0,0xacu);
            *reinterpret_cast<unsigned*>(0x95b214)=clock&4u?1u:0u;
            *reinterpret_cast<unsigned*>(0x740ca4)=clock&1u;
            *reinterpret_cast<int*>(0x8a8cdc)=clock&2u?1:0;
            *reinterpret_cast<int*>(0x8a8ca8)=1;
            for(unsigned slot=0;slot<3;++slot){
                const int first=direction<0?2:0,last=direction<0?0:2;
                const unsigned domain=slot==1?1u:0u;
                *reinterpret_cast<unsigned*>(0x95b214)=domain;
                prepare(0x428460);Bytes args(reinterpret_cast<void*>(S),20);
                args.put32(0,0x440000);args.put32(4,slot==2?20:0);args.put32(8,mode);args.puti(12,first);args.puti(16,last);run();
                const auto h=pool.create(0x440000,slot==2?20:0,mode,first,last,true,domain);
                if(guest_call.out_eax!=h)throw std::runtime_error("sprite allocator disagreement");
                pool.set_speed(h,speed);Bytes(reinterpret_cast<void*>(pool_base+h*0x7c),0x7c).putf(0x20,speed);
                compare(pool,h);
            }
            *reinterpret_cast<unsigned*>(0x95b214)=clock&4u?1u:0u;
            for(unsigned step=0;step<7;++step){
                prepare(0x427f70);run();pool.tick({(clock&1u)!=0,(clock&2u)!=0,(clock&4u)!=0});
                for(auto h:{0u,1u,1280u})compare(pool,h);
            }
        }
        // Only the elapsed-time leaf is replaced. Vector arithmetic and the
        // position/scale setters and controls execute from the original image.
        const std::array<std::uint8_t,7> clock_stub{{0xd9,0x05,0x00,0x02,0x30,0x33,0xc3}};
        r060_raw_write(0x449e30,clock_stub.data(),clock_stub.size());
        auto compare_resource=[&](const auto& native){
            ++cases;const auto* guest=reinterpret_cast<const std::uint8_t*>(base+0x300);
            if(std::memcmp(guest,native.data(),native.size())){
                ++mismatches;if(mismatches<5){std::cerr<<"UI motion mismatch case "<<cases<<": ";
                    for(unsigned j=0;j<native.size();++j)if(guest[j]!=native[j])std::cerr<<std::hex<<j<<" ";
                    std::cerr<<std::dec<<"\n";}
            }
        };
        for(auto setter:{0x465310u,0x4653c0u})for(float duration:{0.0f,1.0f,7.0f,30.0f})
        for(float x:{-38.0f,0.0f,24.0f})for(float y:{-16.0f,0.0f,30.0f})
        for(float delta:{0.0f,1000.0f/60.0f,1000.0f/30.0f,20.0f,1000.0f})for(unsigned release:{0u,1u}){
            source.putf(0x200,delta);pool.reset();pool.create(0x440000,0,1);
            std::array<std::uint8_t,0xa0> native{};Bytes nb(native.data(),native.size());
            FrontendUiResources ui{pool};ui.motion_step=float(static_cast<long double>(delta)*0.001f);
            unsigned result{};ui.call(0x465160,nb,nullptr,0,result);nb.put32(8,0);
            std::memcpy(reinterpret_cast<void*>(base+0x300),native.data(),native.size());
            std::array<unsigned,4> args{};Bytes ab(args.data(),sizeof(args));
            ab.putf(0,x);ab.putf(4,y);ab.putf(8,duration);ab.put32(12,release);
            prepare(setter);guest_call.ecx=base+0x300;std::memcpy(reinterpret_cast<void*>(S),args.data(),sizeof(args));
            run_original32();if(guest_call.out_sp!=S+16)throw std::runtime_error("UI setter stack imbalance");
            ui.call(setter,nb,args.data(),4,result);compare_resource(native);
            for(unsigned step=0;step<10;++step){
                const auto pc=setter==0x465310u?0x465460u:0x465590u;
                prepare(pc);guest_call.ecx=base+0x300;run();ui.call(pc,nb,nullptr,0,result);compare_resource(native);
            }
        }
        std::cout<<"{\"routine\":\"frontend_sprites\",\"cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<",\"clock_leaf_stubbed\":true}\n";
        return mismatches?1:0;
    }
    if(only=="frontend_license_probe"){
        using namespace outrun::platform;
        constexpr unsigned base=0x33100000u,active=0x7c23e0u;
        map_at(base,0x3000u,PROT_READ|PROT_WRITE);
        if(mprotect(reinterpret_cast<void*>(0x7c2000u),0x1000u,PROT_READ|PROT_WRITE))
            throw std::runtime_error("license globals protection");
        map_original_arithmetic_bridge(file);
        auto stub=[](unsigned pc,unsigned value,unsigned short pop=0){
            std::array<std::uint8_t,8> code{{0xb8,0,0,0,0,0xc2,0,0}};
            std::memcpy(code.data()+1,&value,4);std::memcpy(code.data()+6,&pop,2);
            r060_raw_write(pc,code.data(),code.size());
        };
        stub(0x465860,0,44);stub(0x465970,0);stub(0x465250,0);stub(0x4659f0,0);stub(0x4249f0,0);
        LicenseEditorServices services;
        services.call=[](void*,unsigned,std::size_t,const unsigned*,std::size_t,unsigned&){return true;};
        unsigned cases=0,mismatches=0;
        const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
        auto compare=[&](const char* name,const void* guest,const auto& native){
            ++cases;if(std::memcmp(guest,native.data(),native.size())){
                ++mismatches;
                if(mismatches<5){
                    std::cerr<<name<<" mismatch case "<<cases<<": ";
                    const auto* g=static_cast<const std::uint8_t*>(guest);
                    for(std::size_t i=0,n=0;i<native.size()&&n<12;++i)if(g[i]!=native[i]){
                        std::cerr<<std::hex<<i<<"="<<unsigned(g[i])<<"/"<<unsigned(native[i])<<" ";++n;
                    }
                    std::cerr<<std::dec<<"\n";
                }
            }
        };
        // Actual CRT rand body, adapting only its thread-data lookup. Then the
        // original 4162D0 four-license initialization; Windows allocation/SEH
        // and unrelated replay storage layout are explicit fixture services.
        stub(0x585bec,base+0x2400);
        for(unsigned seed:{0u,1u,12345u,0x80000000u,0xffffffffu}){
            unsigned native_seed=seed;*reinterpret_cast<unsigned*>(base+0x2414)=seed;
            for(unsigned i=0;i<64;++i){
                prepare(0x580f40);run();const auto value=frontend_crt_random_580f40(native_seed);
                ++cases;if(guest_call.out_eax!=value||*reinterpret_cast<unsigned*>(base+0x2414)!=native_seed)++mismatches;
            }
        }
        stub(0x580c33,base);stub(0x5802cf,base+0x2200);stub(0x416610,0);
        for(auto page:{0x7b1000u,0x8a8000u})
            if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("profile bank globals");
        for(unsigned address:{0x4162d7u,0x4162deu,0x41636cu}){
            const auto size=address==0x4162d7u?6u:7u;std::array<std::uint8_t,7> code{};
            std::memcpy(code.data(),reinterpret_cast<void*>(address),size);code[0]=0x90;
            const unsigned tls=base+0x2500;std::memcpy(code.data()+size-4,&tls,4);r060_raw_write(address,code.data(),size);
        }
        for(unsigned pattern:{0u,0x55u,0xaau,0xffu}){
            FrontendProfiles profiles;for(auto& p:profiles.licenses)p.fill(std::uint8_t(pattern));
            std::memcpy(reinterpret_cast<void*>(base),profiles.licenses.data(),4*PcLicenseBytes);
            unsigned seed=pattern+1;*reinterpret_cast<unsigned*>(base+0x2414)=seed;
            std::array<unsigned,4> values{};for(auto& v:values)v=frontend_crt_random_580f40(seed);
            prepare(0x4162d0);run();frontend_profiles_initialize_bank_4162d0(profiles,values);
            for(unsigned i=0;i<4;++i)compare("bank constructor",reinterpret_cast<void*>(base+i*PcLicenseBytes),profiles.licenses[i]);
            ++cases;if(*reinterpret_cast<unsigned*>(base+0x2414)!=seed)++mismatches;
        }
        stub(0x580f40,12345);
        // Original save parent: capture actual filename/size/payload contracts
        // at the OS-file wrapper, while retaining its protected middle body.
        for(unsigned page=0x7b1000;page<0x7d0000;page+=4096)
            if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("save globals");
        if(mprotect(reinterpret_cast<void*>(0x745000),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("save enabled");
        const unsigned save_count=base+0x2700,save_results=base+0x2710,save_records=base+0x2800;
        auto append32=[](auto& v,unsigned x){for(unsigned b=0;b<4;++b)v.push_back(std::uint8_t(x>>(b*8)));};
        std::vector<std::uint8_t> save_stub{0x52,0x8b,0x15};append32(save_stub,save_count);
        save_stub.insert(save_stub.end(),{0xc1,0xe2,4,0x81,0xc2});append32(save_stub,save_records);
        save_stub.insert(save_stub.end(),{0x89,0x0a,0x89,0x5a,4,0x8b,0x4c,0x24,8,0x89,0x4a,8,0x8b,0x15});append32(save_stub,save_count);
        save_stub.insert(save_stub.end(),{0x8b,0x04,0x95});append32(save_stub,save_results);
        save_stub.insert(save_stub.end(),{0xff,0x05});append32(save_stub,save_count);save_stub.insert(save_stub.end(),{0x5a,0xc3});
        r060_raw_write(0x406c50,save_stub.data(),save_stub.size());
        for(unsigned enabled:{0u,1u})for(unsigned selected:{~0u,0u,1u,2u,3u})for(unsigned common_status:{0u,1u,7u})for(unsigned status:{0u,1u,2u,7u}){
            PcLicense p;p.fill(0x5a);std::memcpy(reinterpret_cast<void*>(active),p.data(),p.size());
            std::memset(reinterpret_cast<void*>(base),0,4*PcLicenseBytes);
            *reinterpret_cast<unsigned*>(0x7b17f0)=base;*reinterpret_cast<unsigned*>(0x7b17f8)=selected;
            *reinterpret_cast<std::uint8_t*>(0x7457a9)=std::uint8_t(enabled);
            *reinterpret_cast<unsigned*>(save_count)=0;*reinterpret_cast<unsigned*>(save_results)=common_status;
            *reinterpret_cast<unsigned*>(save_results+4)=status;
            prepare(0x416420);run();const unsigned count=enabled?(selected==~0u?1:2):0;
            ++cases;if(*reinterpret_cast<unsigned*>(save_count)!=count||guest_call.out_eax!=(enabled&&selected!=~0u?1u+(status!=0):1u))++mismatches;
            if(count){Bytes record(reinterpret_cast<void*>(save_records),32);
                ++cases;if(record.u32(0)!=0x625d78||record.u32(4)!=PcCommonSaveBytes||record.u32(8)!=0x7b17f8)++mismatches;
                if(count==2){
                    ++cases;if(record.u32(16)!=*reinterpret_cast<unsigned*>(0x7457ac+selected*4)||record.u32(20)!=PcLicenseBytes||std::memcmp(reinterpret_cast<void*>(record.u32(24)),p.data(),p.size()))++mismatches;
                    compare("save active to bank",reinterpret_cast<void*>(base+selected*PcLicenseBytes),p);
                }
            }
        }
        for(unsigned status:{0u,1u,2u,7u}){
            *reinterpret_cast<unsigned*>(save_count)=0;*reinterpret_cast<unsigned*>(save_results)=status;
            prepare(0x4164d0);run();Bytes record(reinterpret_cast<void*>(save_records),16);
            ++cases;if(guest_call.out_eax!=1||*reinterpret_cast<unsigned*>(save_count)!=1||record.u32(4)!=PcCommonSaveBytes||record.u32(8)!=0x7b17f8)++mismatches;
        }
        for(unsigned pattern:{0u,0x55u,0xaau,0xffu}){
            PcLicense license;license.fill(std::uint8_t(pattern));
            std::memcpy(reinterpret_cast<void*>(base),license.data(),license.size());
            prepare(0x4471a0);guest_call.ecx=base;run();
            frontend_license_reset_4471a0(license,12345);
            if(inject&&pattern==0)license[0]^=1;
            compare("constructor",reinterpret_cast<void*>(base),license);
            std::memcpy(reinterpret_cast<void*>(base),license.data(),license.size());
            prepare(0x447360);guest_call.ecx=base;run();frontend_license_unlock_447360(license);
            compare("unlock",reinterpret_cast<void*>(base),license);
        }
        for(unsigned pattern:{0u,0x55u,0xaau,0xffu}){
            PcLicense license;license.fill(std::uint8_t(pattern));
            std::array<std::uint8_t,16> name{};std::memcpy(name.data(),"FERRARI",8);
            std::memcpy(reinterpret_cast<void*>(base),license.data(),license.size());
            std::memcpy(reinterpret_cast<void*>(base+0x2400),name.data(),name.size());
            prepare(0x4dd590);guest_call.ecx=base;Bytes(reinterpret_cast<void*>(S),4).put32(0,base+0x2400);
            run_original32();if(guest_call.out_sp!=S+4)throw std::runtime_error("license name stack imbalance");
            frontend_license_name_4dd590(license,name);compare("name",reinterpret_cast<void*>(base),license);
        }
        {
            // Record the reset receiver to resolve the protected offset load at
            // 4DD994 by executing the original bridge, not patching that load.
            const unsigned capture=base+0x2800;
            std::array<std::uint8_t,7> code{{0x89,0x0d,0,0,0,0,0xc3}};
            std::memcpy(code.data()+2,&capture,4);r060_raw_write(0x465250,code.data(),code.size());
            prepare(0x4dd990);guest_call.ecx=base;Bytes(reinterpret_cast<void*>(S),4).put32(0,4);
            run_original32();if(guest_call.out_sp!=S+4)throw std::runtime_error("license highlight stack imbalance");
            std::cerr<<"highlight UI offset="<<std::hex<<(*reinterpret_cast<unsigned*>(capture)-base)<<std::dec<<"\n";
            ++cases;if(*reinterpret_cast<unsigned*>(capture)!=base+0x4ac)++mismatches;
            stub(0x465250,0);
        }
        for(unsigned stage=3;stage<=5;++stage){
            const unsigned count=stage==3?6:stage==4?26:12;
            const unsigned pc=stage==3?0x4ddef0:stage==4?0x4de020:0x4de160;
            for(unsigned item=0;item<count;++item)for(int input=-1;input<=6;++input){
                std::array<std::uint8_t,PcLicenseEditorBytes> object{};
                Bytes b(object.data(),object.size());b.put32(0x38,stage);b.put32(0x40+(stage-3)*4,item);
                PcLicense license;license.fill(0x5a);
                std::memcpy(reinterpret_cast<void*>(base),object.data(),object.size());
                std::memcpy(reinterpret_cast<void*>(active),license.data(),license.size());
                prepare(pc);guest_call.ecx=base;Bytes(reinterpret_cast<void*>(S),4).puti(0,input);
                run_original32();if(guest_call.out_sp!=S+4)throw std::runtime_error("license grid stack imbalance");
                if(!license_editor_grid(b,license,stage,input,services))throw std::runtime_error("native grid failed");
                compare("grid object",reinterpret_cast<void*>(base),object);
                compare("grid license",reinterpret_cast<void*>(active),license);
            }
        }
        // Run the original complete key-24 parent, not a state-machine model.
        // Only external graphics, keyboard/text widget and save leaves are
        // replaced. All its internal menus/grids/name/cancel logic runs intact.
        constexpr unsigned code_base=0x33200000;
        map_at(code_base,4096,PROT_READ|PROT_WRITE|PROT_EXEC);
        if(mprotect(reinterpret_cast<void*>(0x84b000),4096,PROT_READ|PROT_WRITE))
            throw std::runtime_error("license editor global protection");
        auto vtable=Bytes(reinterpret_cast<void*>(base+0x2000),0x200);
        vtable.put32(0x14,code_base);vtable.put32(0x84,code_base+0x20);
        vtable.put32(0x8c,code_base+0x20);vtable.put32(0xc4,code_base+0x20);
        vtable.put32(0xc8,code_base+0x40);
        stub(code_base+0x20,0);
        stub(0x48eec0,base+0x2400);stub(0x468770,base+0x2400);
        stub(0x48f280,0);stub(0x48ee80,0);stub(0x48e530,0,8);stub(0x48ef40,0,4);
        stub(0x468780,0,8);stub(0x468710,0,4);stub(0x4687c0,0,8);stub(0x468d70,0,4);
        struct EditorInputs{int input;unsigned ready,keyboard,save;} in{};
        services.user=&in;
        services.call=[](void* u,unsigned pc,std::size_t off,const unsigned*,std::size_t,unsigned& result){
            const auto& i=*static_cast<EditorInputs*>(u);
            if(pc==0x48f5f0)result=unsigned(i.input);
            if(pc==0x4652e0)result=i.ready;
            if(pc==8&&off==0xa78)result=i.keyboard;
            if(pc==0x416420)result=i.save;
            return true;
        };
        for(unsigned stage=0;stage<8;++stage)for(unsigned item=0;item<5;++item)
        for(int input=-1;input<=5;++input)for(unsigned ready=0;ready<2;++ready)
        for(unsigned existing=0;existing<2;++existing){
            in={input,ready,unsigned((input+1)%6),2};
            stub(code_base,unsigned(input),4);stub(code_base+0x40,in.keyboard);
            stub(0x4652e0,ready);stub(0x416420,in.save);
            std::array<std::uint8_t,PcLicenseEditorBytes> object{};Bytes b(object.data(),object.size());
            b.put32(0,base+0x2000);b.put32(0x5ec,base+0x2080);b.put32(0xa78,base+0x20c0);
            b.put32(0x38,stage);b.put32(0x3c,item);b.put32(0x40,4);b.put32(0x44,24);b.put32(0x48,10);
            b.put8(0x1184,stage==2?std::uint8_t(ready):0);
            b.put8(0x1185,stage==6?std::uint8_t(item&1):0);b.put32(0x11a0,existing?2:1);
            b.put32(0x1198,3);b.put32(0x119c,8);
            for(unsigned i=0;i<16;++i)b.put8(0x1186+i,std::uint8_t(i));
            PcLicense license{};frontend_license_reset_4471a0(license,123);license[0x3f4]|=1;
            std::memcpy(license.data(),"FERRARI",8);
            std::memcpy(reinterpret_cast<void*>(base),object.data(),object.size());
            std::memcpy(reinterpret_cast<void*>(active),license.data(),license.size());
            services.name_text.fill(0);std::memcpy(services.name_text.data(),"NEW NAME",9);
            std::memcpy(reinterpret_cast<void*>(base+0x2400),services.name_text.data(),16);
            *reinterpret_cast<std::uint8_t*>(0x84b214)=std::uint8_t(existing);
            services.editing_existing=std::uint8_t(existing);
            prepare(0x4de2b0);guest_call.ecx=base;run();
            unsigned action{};
            if(!license_editor_tick_4de2b0(b,license,services,action))throw std::runtime_error("editor parent service failure");
            compare("editor object",reinterpret_cast<void*>(base),object);
            compare("editor license",reinterpret_cast<void*>(active),license);
            ++cases;if(action!=guest_call.out_eax||services.editing_existing!=*reinterpret_cast<std::uint8_t*>(0x84b214)){
                ++mismatches;if(mismatches<5)std::cerr<<"editor return/global stage="<<stage<<" input="<<input<<"\n";
            }
        }
        std::cout<<"{\"routine\":\"frontend_license\",\"cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<",\"ui_services_stubbed\":true}\n";
        return mismatches?1:0;
    }
    if(only=="frontend_welcome_probe"){
        const bool detect_mismatch=argc>6&&std::string(argv[6])=="inject-mismatch";
        constexpr std::uint32_t base=0x33100000u;
        map_at(base,0x3000u,PROT_READ|PROT_WRITE);
        const auto stub=[](std::uint32_t pc,std::uint32_t value,std::uint16_t pop=0u){
            std::array<std::uint8_t,8> code{{0xb8,0,0,0,0,0xc2,0,0}};
            std::memcpy(code.data()+1,&value,4);std::memcpy(code.data()+6,&pop,2);
            r060_raw_write(pc,code.data(),code.size());
        };
        for(auto pc:{0x401030u,0x4afea0u,0x4285a0u,0x414790u,0x4249f0u,0x492690u,0x401000u,0x4040f0u})stub(pc,0u);
        stub(0x4035f0u,base+0x1000u);stub(0x442f20u,0u,8u);
        stub(0x447000u,0u,8u);stub(0x440ea0u,0u,32u);stub(0x4413f0u,0u,4u);
        stub(0x428320u,0x74000002u);stub(0x465eb0u,0x12340000u);
        struct Inputs {std::uint32_t held{},pressed{},target{};} inputs;
        auto call=[](void* user,std::uint32_t pc,const std::uint32_t* args,std::size_t)->std::uint32_t{
            const auto& in=*static_cast<Inputs*>(user);
            if(pc==0x4536c0u)return in.held&args[0];
            if(pc==0x4536f0u)return in.pressed&args[0];
            if(pc==0x416380u)return in.target;
            if(pc==0x428320u)return 0x74000002u;
            if(pc==0x465eb0u)return 0x12340000u;
            return 0u;
        };
        unsigned cases=0u,mismatches=0u;
        for(auto held:{0u,1u,4u,5u})for(auto pressed:{0u,2u,8u,10u})
        for(auto stage:{0u,1u,2u,3u})for(auto timer:{0u,1u,2u})for(auto target:{1u,21u}){
            inputs={held,pressed,target};
            // These two cdecl stubs implement the original mask leaves;
            // the controller at 4C5210 itself is not patched.
            for(auto pair:{std::pair{0x4536c0u,held},std::pair{0x4536f0u,pressed}}){
                std::array<std::uint8_t,10> code{{0xb8,0,0,0,0,0x23,0x44,0x24,0x04,0xc3}};
                std::memcpy(code.data()+1,&pair.second,4);r060_raw_write(pair.first,code.data(),code.size());
            }
            stub(0x416380u,target);
            std::array<std::uint8_t,0x758u> native{};
            Bytes nb(native.data(),native.size());nb.put32(0x740u,stage);nb.put32(0x754u,timer);
            nb.put32(0x744u,0x74000002u);nb.put8(0x751u,std::uint8_t(timer&1u));
            std::memcpy(reinterpret_cast<void*>(base),native.data(),native.size());
            prepare(0x4c5210u);guest_call.ecx=base;run();
            const auto result=outrun::platform::frontend_welcome_control_4c5210(nb,{&inputs,call});
            if(detect_mismatch&&cases==0u)native[0]^=1u;
            ++cases;if(result!=guest_call.out_eax||std::memcmp(native.data(),reinterpret_cast<void*>(base),native.size()))++mismatches;
        }
        for(auto pc:{0x4c5180u,0x4c5350u})for(auto active:{0u,1u}){
            std::array<std::uint8_t,0x758u> native{};Bytes nb(native.data(),native.size());
            nb.put8(0x750u,std::uint8_t(active));nb.put32(0x744u,0x74000002u);
            std::memcpy(reinterpret_cast<void*>(base),native.data(),native.size());
            prepare(pc);guest_call.ecx=base;run();
            if(pc==0x4c5180u)outrun::platform::frontend_welcome_init_4c5180(nb,{&inputs,call});
            else outrun::platform::frontend_welcome_suspend_4c5350(nb,{&inputs,call});
            ++cases;if(std::memcmp(native.data(),reinterpret_cast<void*>(base),native.size()))++mismatches;
        }
        std::cout<<"{\"routine\":\"frontend_welcome\",\"cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<",\"external_services_stubbed\":true}\n";
        return mismatches?1:0;
    }
    if(only=="start_fight_vm_probe"){
        // Isolated original-code experiment: vary only known input globals.
        // Each observation is the unmodified public entry and original VM.
        for(const std::uint32_t page:{0x842000u,0x836000u})
            if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE))
                throw std::runtime_error("START VM input page protection");
        auto* mode=reinterpret_cast<std::uint32_t*>(0x8421c0u);
        auto* selection=reinterpret_cast<std::uint8_t*>(0x836374u);
        const auto old_mode=*mode;
        const auto old_selection=*selection;
        std::cout<<"{\"entry\":\"0x004b00d0\",\"observations\":[";
        bool first=true;
        unsigned mismatches=0u;
        constexpr std::array<std::uint32_t,15> mode_cases{{
            0u,1u,2u,3u,4u,5u,6u,7u,8u,9u,
            0x7fffffffu,0x80000000u,0xffffffffu,0x10000u,0x01010101u
        }};
        for(const auto m:mode_cases)for(std::uint32_t s=0;s<=1u;++s){
            *mode=m;*selection=static_cast<std::uint8_t>(s);
            prepare(0x004b00d0u);run();
            if(guest_call.out_eax!=(m!=0u)||guest_call.out_ecx!=m)++mismatches;
            if(!first)std::cout<<',';first=false;
            std::cout<<"{\"mode_8421c0\":"<<m<<",\"selection_836374\":"<<s
                     <<",\"eax\":"<<guest_call.out_eax<<",\"ecx\":"<<guest_call.out_ecx<<'}';
        }
        *mode=old_mode;*selection=old_selection;
        std::cout<<"],\"hypothesis\":\"eax = state != 0; ecx = state\","
                 <<"\"total_mismatching_cases\":"<<mismatches<<"}\n";
        return mismatches?1:0;
    }
    if(only=="course_field_vm_probe"){
        constexpr std::uint32_t first_object=R076RecordsBase;
        constexpr std::uint32_t second_object=R076RecordsBase+0x100u;
        for(std::uint32_t offset=0;offset<0x100u;offset+=4u){
            *reinterpret_cast<std::uint32_t*>(first_object+offset)=0xa0000000u+offset;
            *reinterpret_cast<std::uint32_t*>(second_object+offset)=0xb0000000u+offset;
        }
        *reinterpret_cast<std::uint32_t*>(0x7d30bcu)=first_object;
        *reinterpret_cast<std::uint32_t*>(0x7d30c0u)=second_object;
        constexpr std::array<std::uint32_t,18> expected{{
            0xa000000cu,0xb000000cu,0xb0000018u,0xb000001cu,
            0xa0000020u,0xa0000024u,0xa0000028u,0xa000002cu,
            0xa0000030u,0xb0000030u,0xa0000034u,0xb0000034u,
            0xa0000064u,0xa0000068u,0xb0000068u,0xa000006cu,0u,0u
        }};
        std::cout<<"{\"entry\":\"0x0044c220\",\"observations\":[";
        unsigned mismatches=0u;
        for(std::uint32_t index=0;index<18u;++index){
            prepare(0x0044c220u);
            Bytes(reinterpret_cast<void*>(S),8).put32(0,index);
            run();
            if(guest_call.out_eax!=expected[index])++mismatches;
            if(index)std::cout<<',';
            std::cout<<"{\"index\":"<<index<<",\"eax\":"<<guest_call.out_eax<<'}';
        }
        std::cout<<"],\"hypothesis\":\"six secondary indices, ten primary indices, default zero\","
                 <<"\"total_mismatching_cases\":"<<mismatches<<"}\n";
        return mismatches?1:0;
    }
    if(only=="title_owner_vm_probe"){
        constexpr std::uint32_t code_base=0x32090000u;
        constexpr std::uint32_t data_base=0x330d0000u;
        constexpr std::uint32_t title_object=data_base;
        constexpr std::uint32_t singleton=data_base+0x2000u;
        constexpr std::uint32_t child_vtable=data_base+0x3000u;
        auto* code=static_cast<std::uint8_t*>(map_at(code_base,4096,PROT_READ|PROT_WRITE));
        map_at(data_base,0x5000u,PROT_READ|PROT_WRITE);
        if(mprotect(reinterpret_cast<void*>(0x692000u),4096,PROT_READ|PROT_WRITE)||
           mprotect(reinterpret_cast<void*>(0x780000u),4096,PROT_READ|PROT_WRITE)||
           mprotect(reinterpret_cast<void*>(0x84b000u),4096,PROT_READ|PROT_WRITE))
            throw std::runtime_error("title probe globals protection");
        const std::uint8_t getter[]={0xb8,0,0,0,0,0xc3};
        std::memcpy(code,getter,sizeof(getter));
        std::memcpy(code+1,&singleton,4);
        code[0x20]=0xc3; // external child callback: inert, separately audited
        *reinterpret_cast<std::uint32_t*>(title_object+0x9bcu)=child_vtable;
        *reinterpret_cast<std::uint32_t*>(child_vtable+4u)=code_base+0x20u;
        r060_check_call(0x4d5d26u,0x4035f0u);
        r060_check_call(0x4d5d81u,0x4035f0u);
        r060_check_call(0x4d5d9au,0x440930u);
        r046_write_rel32(0x4d5d26u,0xe8,code_base);
        r046_write_rel32(0x4d5d81u,0xe8,code_base);
        r046_write_rel32(0x4d5d9au,0xe8,code_base+0x20u);
        if(mprotect(code,4096,PROT_READ|PROT_EXEC))throw std::runtime_error("title probe code protection");
        std::cout<<"{\"entry\":\"0x004d5d00\",\"observations\":[";
        bool first=true;
        std::uint32_t mismatches{};
        for(std::uint32_t state=0u;state<=8u;++state)
        for(const std::uint32_t mode:{1u,4u}){
            std::array<std::uint8_t,outrun::platform::TitleOwnerPcSize> native_object{};
            outrun::platform::TitleOwnerGlobals native_globals{};
            const outrun::platform::TitleOwnerServices native_services{
                nullptr,[](void*){},[](void*){}};
            const bool native_ok=outrun::platform::title_owner_init_4d5d00(
                native_object.data(),native_object.size(),state,mode,
                native_globals,native_services);
            *reinterpret_cast<std::uint32_t*>(singleton+0x218u)=state;
            *reinterpret_cast<std::uint32_t*>(0x780258u)=mode;
            *reinterpret_cast<std::uint8_t*>(0x84b200u)=0u;
            for(const std::uint32_t address:{0x692b28u,0x692b2cu,0x692b30u})
                *reinterpret_cast<std::uint32_t*>(address)=0u;
            prepare(0x004d5d00u);guest_call.ecx=title_object;run();
            std::uint32_t native_9ac{},native_9b0{},native_9b4{};
            std::memcpy(&native_9ac,native_object.data()+0x9acu,4);
            std::memcpy(&native_9b0,native_object.data()+0x9b0u,4);
            std::memcpy(&native_9b4,native_object.data()+0x9b4u,4);
            const bool matches=native_ok&&
                native_9ac==*reinterpret_cast<std::uint32_t*>(title_object+0x9acu)&&
                native_9b0==*reinterpret_cast<std::uint32_t*>(title_object+0x9b0u)&&
                native_9b4==*reinterpret_cast<std::uint32_t*>(title_object+0x9b4u)&&
                native_object[0x9b8u]==*reinterpret_cast<std::uint8_t*>(title_object+0x9b8u)&&
                native_globals.scene_ids[0]==*reinterpret_cast<std::uint32_t*>(0x692b28u)&&
                native_globals.scene_ids[1]==*reinterpret_cast<std::uint32_t*>(0x692b2cu)&&
                native_globals.scene_ids[2]==*reinterpret_cast<std::uint32_t*>(0x692b30u)&&
                native_globals.pause_flag==*reinterpret_cast<std::uint8_t*>(0x84b200u);
            if(!matches)++mismatches;
            if(!first)std::cout<<',';first=false;
            std::cout<<"{\"singleton_state\":"<<state<<",\"mode\":"<<mode
                     <<",\"globals\":["<<*reinterpret_cast<std::uint32_t*>(0x692b28u)
                     <<','<<*reinterpret_cast<std::uint32_t*>(0x692b2cu)
                     <<','<<*reinterpret_cast<std::uint32_t*>(0x692b30u)
                     <<"],\"pause_flag\":"<<unsigned(*reinterpret_cast<std::uint8_t*>(0x84b200u))
                     <<",\"native_match\":"<<(matches?"true":"false")
                     <<'}';
        }
        std::cout<<"],\"stubbed_singleton\":true,\"stubbed_child\":true,"
                 <<"\"stubbed_pause_service\":true,\"probe_only\":true,"
                 <<"\"total_mismatching_cases\":"<<mismatches<<"}\n";
        return mismatches?1:0;
    }
    if(only=="title_owner_dispatch_probe"){
        constexpr std::uint32_t data_base=0x330e0000u;
        constexpr std::uint32_t code_base=0x320a0000u;
        map_at(data_base,0x3000u,PROT_READ|PROT_WRITE);
        auto* code=static_cast<std::uint8_t*>(map_at(code_base,4096u,PROT_READ|PROT_WRITE));
        code[0]=0xc3; // child virtual +0xC, not the owner dispatch
        *reinterpret_cast<std::uint32_t*>(data_base+0x9bcu)=data_base+0x2000u;
        *reinterpret_cast<std::uint32_t*>(data_base+0x200cu)=code_base;
        if(mprotect(code,4096u,PROT_READ|PROT_EXEC))throw std::runtime_error("title dispatch stub protection");
        constexpr std::array<std::uint32_t,5> handlers{{
            0x4d60c0u,0x4d62a0u,0x4d6370u,0x4d6510u,0x4d6a60u}};
        if(mprotect(reinterpret_cast<void*>(0x4d6000u),0x2000u,
                    PROT_READ|PROT_WRITE|PROT_EXEC))
            throw std::runtime_error("title dispatch handler protection");
        for(const auto target:handlers){
            auto* p=reinterpret_cast<std::uint8_t*>(std::uintptr_t(target));
            p[0]=0xb8;std::memcpy(p+1,&target,4);p[5]=0xc3;
        }
        if(mprotect(reinterpret_cast<void*>(0x4d6000u),0x2000u,
                    PROT_READ|PROT_EXEC))
            throw std::runtime_error("title dispatch handler restore protection");
        std::uint32_t mismatches{};
        std::cout<<"{\"entry\":\"0x004d7260\",\"observations\":[";
        for(std::uint32_t state=0u;state<=24u;++state){
            *reinterpret_cast<std::uint32_t*>(data_base+0x9acu)=state;
            prepare(0x4d7260u);guest_call.ecx=data_base;run();
            const std::uint32_t observed=
                std::find(handlers.begin(),handlers.end(),guest_call.out_eax)!=handlers.end()
                    ?guest_call.out_eax:0u;
            const auto native=outrun::platform::title_owner_control_target_4d7260(state);
            if(observed!=native)++mismatches;
            if(state)std::cout<<',';
            std::cout<<"{\"state\":"<<state<<",\"target\":"<<observed
                     <<",\"native_match\":"<<(observed==native?"true":"false")<<'}';
        }
        std::cout<<"],\"stubbed_child\":true,\"stubbed_handlers\":true,"
                 <<"\"total_mismatching_cases\":"<<mismatches<<"}\n";
        return mismatches?1:0;
    }
    if(only=="title_owner_constructor_probe"){
        constexpr std::uint32_t code_base=0x320b0000u;
        constexpr std::uint32_t trace_base=0x330d8000u;
        constexpr std::uint32_t object_base=0x330f0000u;
        struct Site{std::uint32_t call,target;bool array;};
        constexpr std::array<Site,9> sites{{
            {0x4d715du,0x48f480u,false},{0x4d7173u,0x48e310u,false},
            {0x4d7195u,0x5816bdu,true},{0x4d71a5u,0x465160u,false},
            {0x4d71b5u,0x465160u,false},{0x4d71c5u,0x465160u,false},
            {0x4d71d5u,0x48c490u,false},{0x4d71e5u,0x4ed950u,false},
            {0x4d71f5u,0x465160u,false}}};
        auto* code=static_cast<std::uint8_t*>(map_at(code_base,4096u,PROT_READ|PROT_WRITE));
        map_at(trace_base,4096u,PROT_READ|PROT_WRITE);
        map_at(object_base,0x2000u,PROT_READ|PROT_WRITE);
        for(std::size_t i=0;i<sites.size();++i){
            const auto& s=sites[i];r060_check_call(s.call,s.target);
            std::vector<std::uint8_t> b;
            r060_inc(b,trace_base);
            const auto slot=trace_base+0x20u+std::uint32_t(i)*0x20u;
            b.insert(b.end(),{0xc7,0x05});r060_emit32(b,slot);r060_emit32(b,s.target);
            if(s.array){
                for(std::uint8_t a=0;a<5u;++a){
                    b.insert(b.end(),{0x8b,0x44,0x24,std::uint8_t(4u+4u*a),0xa3});
                    r060_emit32(b,slot+4u+4u*a);
                }
                b.insert(b.end(),{0xc2,0x14,0x00});
            }else{
                b.insert(b.end(),{0x89,0x0d});r060_emit32(b,slot+4u);
                b.push_back(0xc3);
            }
            if(b.size()>0x80u)throw std::runtime_error("title constructor stub size");
            std::memcpy(code+i*0x80u,b.data(),b.size());
            r046_write_rel32(s.call,0xe8u,code_base+std::uint32_t(i)*0x80u);
        }
        static const std::uint8_t zero_eax[6]={0x31,0xc0,0x90,0x90,0x90,0x90};
        static const std::uint8_t nops7[7]={0x90,0x90,0x90,0x90,0x90,0x90,0x90};
        r060_raw_write(0x4d7147u,zero_eax,6u);
        r060_raw_write(0x4d714eu,nops7,7u);
        r060_raw_write(0x4d7228u,nops7,7u);
        if(mprotect(code,4096u,PROT_READ|PROT_EXEC)||
           mprotect(reinterpret_cast<void*>(0x780000u),4096u,PROT_READ|PROT_WRITE)||
           mprotect(reinterpret_cast<void*>(0x95b000u),4096u,PROT_READ|PROT_WRITE))
            throw std::runtime_error("title constructor protection");
        struct NativeTrace{
            std::uint8_t* base{};
            std::size_t next{};
            std::array<std::array<std::uint32_t,7>,9> rows{};
            static void child(void* user,std::uint32_t pc,std::uint8_t* ptr){
                auto& t=*static_cast<NativeTrace*>(user);
                if(t.next>=t.rows.size())throw std::runtime_error("title child overflow");
                t.rows[t.next++]={pc,0x330f0000u+std::uint32_t(ptr-t.base)};
            }
            static void array(void* user,std::uint32_t pc,std::uint8_t* ptr,
                              std::uint32_t elem,std::uint32_t count,
                              std::uint32_t ctor,std::uint32_t dtor){
                auto& t=*static_cast<NativeTrace*>(user);
                if(t.next>=t.rows.size())throw std::runtime_error("title array overflow");
                t.rows[t.next++]={pc,0x330f0000u+std::uint32_t(ptr-t.base),elem,count,ctor,dtor};
            }
        };
        std::uint32_t mismatches{};
        std::cout<<"{\"entry\":\"0x004d7140\",\"observations\":[";
        for(std::uint32_t case_index=0;case_index<32u;++case_index){
            std::array<std::uint8_t,outrun::platform::TitleOwnerPcSize> native{};
            std::mt19937 rng(0x4d7140u^case_index*2654435761u);
            for(auto& byte:native)byte=std::uint8_t(rng());
            std::memcpy(reinterpret_cast<void*>(object_base),native.data(),native.size());
            std::memset(reinterpret_cast<void*>(trace_base),0,4096u);
            std::uint8_t game=std::uint8_t(case_index),flag=std::uint8_t(case_index+3u);
            *reinterpret_cast<std::uint8_t*>(0x780270u)=game;
            *reinterpret_cast<std::uint8_t*>(0x95b250u)=flag;
            NativeTrace trace{};trace.base=native.data();
            const bool native_ok=outrun::platform::title_owner_construct_4d7140(
                native.data(),native.size(),game,flag,
                {&trace,NativeTrace::child,NativeTrace::array});
            prepare(0x4d7140u);guest_call.ecx=object_base;run();
            bool matches=native_ok&&guest_call.out_eax==object_base&&
                std::memcmp(reinterpret_cast<void*>(object_base),native.data(),native.size())==0&&
                game==*reinterpret_cast<std::uint8_t*>(0x780270u)&&
                flag==*reinterpret_cast<std::uint8_t*>(0x95b250u)&&
                *reinterpret_cast<std::uint32_t*>(trace_base)==sites.size()&&
                trace.next==sites.size();
            for(std::size_t i=0;i<sites.size();++i){
                std::array<std::uint32_t,7> pc{};
                std::memcpy(pc.data(),reinterpret_cast<void*>(trace_base+0x20u+i*0x20u),
                            pc.size()*sizeof(std::uint32_t));
                if(pc!=trace.rows[i])matches=false;
            }
            if(!matches)++mismatches;
            if(case_index)std::cout<<',';
            std::cout<<"{\"case\":"<<case_index<<",\"native_match\":"
                     <<(matches?"true":"false")<<'}';
        }
        std::cout<<"],\"stubbed_nested_constructors\":true,\"seh_neutralized\":true,"
                 <<"\"total_mismatching_cases\":"<<mismatches<<"}\n";
        return mismatches?1:0;
    }
    if(only=="title_owner_children_probe"){
        constexpr std::uint32_t object_base=0x33100000u;
        map_at(object_base,4096u,PROT_READ|PROT_WRITE);
        if(mprotect(reinterpret_cast<void*>(0x659000u),4096u,PROT_READ|PROT_WRITE))
            throw std::runtime_error("title child global protection");
        constexpr std::array<std::uint32_t,3> entries{{0x48f480u,0x465160u,0x4ed950u}};
        std::uint32_t mismatches{};
        std::cout<<"{\"entries\":[\"0x0048f480\",\"0x00465160\",\"0x004ed950\"],\"observations\":[";
        bool first=true;
        for(const auto entry:entries)
        for(std::uint32_t case_index=0u;case_index<32u;++case_index){
            std::array<std::uint8_t,0xa0u> native{};
            std::mt19937 rng(entry^case_index*2654435761u);
            for(auto& byte:native)byte=std::uint8_t(rng());
            std::memcpy(reinterpret_cast<void*>(object_base),native.data(),native.size());
            std::uint32_t base_global=case_index+0x100u;
            *reinterpret_cast<std::uint32_t*>(0x6591e4u)=base_global;
            bool ok{};
            if(entry==0x48f480u)
                ok=outrun::platform::title_base_construct_48f480(
                    native.data(),native.size(),base_global);
            else if(entry==0x465160u)
                ok=outrun::platform::title_ui_resource_construct_465160(
                    native.data(),native.size());
            else ok=outrun::platform::title_list_construct_4ed950(
                    native.data(),native.size());
            prepare(entry);guest_call.ecx=object_base;run();
            const bool matches=ok&&guest_call.out_eax==object_base&&
                std::memcmp(reinterpret_cast<void*>(object_base),native.data(),native.size())==0&&
                (entry!=0x48f480u||base_global==*reinterpret_cast<std::uint32_t*>(0x6591e4u));
            if(!matches)++mismatches;
            if(!first)std::cout<<',';first=false;
            std::cout<<"{\"entry\":"<<entry<<",\"case\":"<<case_index
                     <<",\"native_match\":"<<(matches?"true":"false")<<'}';
        }
        std::cout<<"],\"total_mismatching_cases\":"<<mismatches<<"}\n";
        return mismatches?1:0;
    }
    if(only=="title_owner_full_constructor_probe"){
        constexpr std::uint32_t code_base=0x320c0000u;
        constexpr std::uint32_t object_base=0x33200000u;
        auto* code=static_cast<std::uint8_t*>(map_at(code_base,4096u,PROT_READ|PROT_WRITE));
        map_at(object_base,0x2000u,PROT_READ|PROT_WRITE);
        r060_check_call(0x4d7195u,0x5816bdu);
        const std::uint8_t array_noop[3]={0xc2,0x14,0x00};
        std::memcpy(code,array_noop,sizeof(array_noop));
        r046_write_rel32(0x4d7195u,0xe8u,code_base);
        static const std::uint8_t zero_eax[6]={0x31,0xc0,0x90,0x90,0x90,0x90};
        static const std::uint8_t nops7[7]={0x90,0x90,0x90,0x90,0x90,0x90,0x90};
        r060_raw_write(0x4d7147u,zero_eax,6u);
        r060_raw_write(0x4d714eu,nops7,7u);
        r060_raw_write(0x4d7228u,nops7,7u);
        r060_raw_write(0x48c497u,zero_eax,6u);
        r060_raw_write(0x48c49eu,nops7,7u);
        r060_raw_write(0x48c50du,nops7,7u);
        if(mprotect(code,4096u,PROT_READ|PROT_EXEC)||
           mprotect(reinterpret_cast<void*>(0x780000u),4096u,PROT_READ|PROT_WRITE)||
           mprotect(reinterpret_cast<void*>(0x95b000u),4096u,PROT_READ|PROT_WRITE)||
           mprotect(reinterpret_cast<void*>(0x659000u),4096u,PROT_READ|PROT_WRITE))
            throw std::runtime_error("title full constructor protection");
        std::uint32_t mismatches{};
        std::cout<<"{\"entry\":\"0x004d7140\",\"observations\":[";
        for(std::uint32_t case_index=0;case_index<32u;++case_index){
            std::array<std::uint8_t,outrun::platform::TitleOwnerPcSize> native{};
            std::mt19937 rng(0x4d7140u^case_index*2654435761u);
            for(auto& byte:native)byte=std::uint8_t(rng());
            std::memcpy(reinterpret_cast<void*>(object_base),native.data(),native.size());
            std::uint8_t game=std::uint8_t(case_index),flag=std::uint8_t(case_index+3u);
            std::uint32_t base_global=case_index+0x100u;
            *reinterpret_cast<std::uint8_t*>(0x780270u)=game;
            *reinterpret_cast<std::uint8_t*>(0x95b250u)=flag;
            *reinterpret_cast<std::uint32_t*>(0x6591e4u)=base_global;
            const bool native_ok=outrun::platform::title_owner_construct_complete_4d7140(
                native.data(),native.size(),game,flag,base_global);
            prepare(0x4d7140u);guest_call.ecx=object_base;run();
            const bool matches=native_ok&&guest_call.out_eax==object_base&&
                std::memcmp(reinterpret_cast<void*>(object_base),native.data(),native.size())==0&&
                game==*reinterpret_cast<std::uint8_t*>(0x780270u)&&
                flag==*reinterpret_cast<std::uint8_t*>(0x95b250u)&&
                base_global==*reinterpret_cast<std::uint32_t*>(0x6591e4u);
            if(!matches)++mismatches;
            if(case_index)std::cout<<',';
            std::cout<<"{\"case\":"<<case_index<<",\"native_match\":"
                     <<(matches?"true":"false")<<'}';
        }
        std::cout<<"],\"array_element_constructor_noop\":true,"
                 <<"\"seh_neutralized\":true,\"total_mismatching_cases\":"
                 <<mismatches<<"}\n";
        return mismatches?1:0;
    }
    if(only=="title_owner_controller_init_probe"){
        constexpr std::uint32_t object_base=0x33210000u;
        map_at(object_base,0x2000u,PROT_READ|PROT_WRITE);
        std::uint32_t mismatches{};
        std::cout<<"{\"entry\":\"0x0048c5b0\",\"observations\":[";
        for(std::uint32_t case_index=0;case_index<32u;++case_index){
            std::array<std::uint8_t,0x12a6u> native{};
            std::mt19937 rng(0x48c5b0u^case_index*2654435761u);
            for(auto& byte:native)byte=std::uint8_t(rng());
            std::memcpy(reinterpret_cast<void*>(object_base),native.data(),native.size());
            const bool native_ok=outrun::platform::title_controller_init_48c5b0(
                native.data(),native.size());
            prepare(0x48c5b0u);guest_call.ecx=object_base;run();
            const bool matches=native_ok&&guest_call.out_eax==1u&&
                std::memcmp(reinterpret_cast<void*>(object_base),native.data(),native.size())==0;
            if(!matches)++mismatches;
            if(case_index)std::cout<<',';
            std::cout<<"{\"case\":"<<case_index<<",\"native_match\":"
                     <<(matches?"true":"false")<<'}';
        }
        std::cout<<"],\"total_mismatching_cases\":"<<mismatches<<"}\n";
        return mismatches?1:0;
    }
    if(only=="title_owner_tick_dispatch_probe"){
        constexpr std::uint32_t object_base=0x33220000u;
        constexpr std::uint32_t child_vtable=0x33222000u;
        constexpr std::uint32_t trace_word=0x33223000u;
        constexpr std::uint32_t stub_code=0x320d0000u;
        map_at(object_base,0x4000u,PROT_READ|PROT_WRITE);
        auto* code=static_cast<std::uint8_t*>(map_at(stub_code,4096u,PROT_READ|PROT_WRITE));
        code[0]=0xc3; // controller virtual +8, tested separately as a boundary
        *reinterpret_cast<std::uint32_t*>(child_vtable+8u)=stub_code;
        if(mprotect(code,4096u,PROT_READ|PROT_EXEC))
            throw std::runtime_error("title tick child stub protection");
        constexpr std::array<std::uint32_t,21> handlers{{
            0x4d5e40u,0x4d7300u,0x4d6090u,0x4d7440u,0x4d86f0u,
            0x4d7780u,0x4d8890u,0x4d6340u,0x4d78c0u,0x4d89b0u,
            0x4d63f0u,0x4d6430u,0x4d7b20u,0x4d7b80u,0x4d6530u,
            0x4d7c70u,0x4d6630u,0x4d66d0u,0x4d7da0u,0x4d7e00u,
            0x4d7fb0u}};
        for(const auto pc:handlers){
            const std::uint8_t b[]{0xc7u,0x05u,
                std::uint8_t(trace_word),std::uint8_t(trace_word>>8u),
                std::uint8_t(trace_word>>16u),std::uint8_t(trace_word>>24u),
                std::uint8_t(pc),std::uint8_t(pc>>8u),
                std::uint8_t(pc>>16u),std::uint8_t(pc>>24u),
                0x31u,0xc0u,0xc3u};
            r060_raw_write(pc,b,sizeof(b));
        }
        constexpr std::uint32_t last_handler=0x4d6e50u;
        const std::uint8_t tail[]{0xc7u,0x05u,
            std::uint8_t(trace_word),std::uint8_t(trace_word>>8u),
            std::uint8_t(trace_word>>16u),std::uint8_t(trace_word>>24u),
            std::uint8_t(last_handler),std::uint8_t(last_handler>>8u),
            std::uint8_t(last_handler>>16u),std::uint8_t(last_handler>>24u),
            0x31u,0xc0u,0xc3u};
        r060_raw_write(last_handler,tail,sizeof(tail));
        struct NativeTickTrace {
            std::uint32_t last{};
            static bool call(void* user,std::uint32_t pc,std::uint8_t*,
                             std::size_t,std::uint32_t& value){
                auto& t=*static_cast<NativeTickTrace*>(user);
                if(pc!=0x48d420u)t.last=pc;
                value=0u;return true;
            }
        };
        std::cout<<"{\"entry\":\"0x004d8c50\",\"observations\":[";
        std::uint32_t mismatches{};
        for(std::uint32_t stage=0u;stage<24u;++stage){
            std::array<std::uint8_t,outrun::platform::TitleOwnerPcSize> native{};
            std::memcpy(native.data()+0x9acu,&stage,4u);
            std::memcpy(native.data()+0x9bcu,&child_vtable,4u);
            std::memcpy(reinterpret_cast<void*>(object_base),native.data(),native.size());
            *reinterpret_cast<std::uint32_t*>(trace_word)=0u;
            NativeTickTrace trace{};std::uint32_t native_result=0xffffffffu;
            const bool ok=outrun::platform::title_owner_tick_4d8c50(
                native.data(),native.size(),{&trace,NativeTickTrace::call,2u},native_result);
            prepare(0x4d8c50u);guest_call.ecx=object_base;run();
            const auto guest_last=*reinterpret_cast<std::uint32_t*>(trace_word);
            const bool match=ok&&guest_call.out_eax==native_result&&
                guest_last==trace.last&&
                std::memcmp(reinterpret_cast<void*>(object_base),native.data(),native.size())==0;
            if(!match)++mismatches;
            if(stage)std::cout<<',';
            std::cout<<"{\"state\":"<<stage<<",\"handler\":"<<guest_last
                     <<",\"native_match\":"<<(match?"true":"false")<<'}';
        }
        std::cout<<"],\"stubbed_controller\":true,\"stubbed_handlers\":true,"
                 <<"\"total_mismatching_cases\":"<<mismatches<<"}\n";
        return mismatches?1:0;
    }
    if(argc>5 && std::string(argv[5])!="-")begin_snapshots(argv[5]);
    if(argc>6 && std::string(argv[6])=="inject-mismatch")inject_mismatch=true;
    if(argc>8 && std::string(argv[8])!="-")begin_spline_golden(argv[8]);
    if(argc>9 && std::string(argv[9])!="-")begin_query_golden(argv[9]);
    if(argc>10 && std::string(argv[10])!="-")begin_world_golden(argv[10]);
    if(argc>11 && std::string(argv[11])!="-")begin_ground_golden(argv[11]);
    if(argc>12 && std::string(argv[12])!="-")begin_wall_golden(argv[12]);
    if(argc>13 && std::string(argv[13])!="-")begin_response_golden(argv[13]);
    if(argc>14 && std::string(argv[14])!="-")begin_rebound_golden(argv[14]);
    if(argc>15 && std::string(argv[15])!="-")begin_crash_golden(argv[15]);
    if(argc>16 && std::string(argv[16])!="-")begin_entry_golden(argv[16]);
    auto enabled=[&](const char* n){
        if(only=="all"||only==n)return true;
        if(only=="r043_postghost_batch"){
            const std::string q=n;
            return q=="session_mode4_4962a0_r043"||q=="session_mode6_4962d0_r043"||
                   q=="othcar_calc_r_46f040_r043"||q=="othcar_calc_r_alt_46f190_r043"||
                   q=="othcar_get_r_479670_r043";
        }
        if(only=="r044_platform_batch"){
            const std::string q=n;
            return q=="platform_flag_bit2_44ff10_r044"||q=="platform_index_value_4505a0_r044"||
                   q=="platform_pair_value_450630_r044"||q=="platform_slot_kind_450750_r044"||
                   q=="platform_counter_lt_60_48b350_r044"||q=="platform_ghost_route_4671d0_r044"||
                   q=="platform_ghost_record_47f780_r044"||q=="common_pl_car_inline_tail_4a82c4_r044";
        }
        if(only=="r045_course_services_batch"){
            const std::string q=n;
            return q=="course_nested_marker_44bdb0_r045"||q=="course_stage_limit_44be00_r045"||
                   q=="course_active_slot_44be10_r045"||q=="course_primary_ready_44be30_r045"||
                   q=="course_secondary_ready_44be40_r045"||q=="course_type_gate_44be50_r045"||
                   q=="course_index_lookup_44be80_r045"||q=="course_disp_matrix_choice_44bed0_r045"||
                   q=="course_area_matrix_choice_44bef0_r045"||q=="course_clear_service_state_44bf10_r045"||
                   q=="course_mark_ready_44c080_r045"||q=="course_get_mode_byte_44c090_r045"||
                   q=="course_set_mode_byte_44c0a0_r045"||q=="course_copy_snapshot_44c0b0_r045";
        }
        if(only=="r046_road_services_batch"){
            const std::string q=n;
            return q=="course_stage_unique_44dc50_r046"||q=="road_stage_window_44ddc0_r046"||
                   q=="road_stage_gate_44f0f0_r046"||q=="road_decode_sample_46ffc0_r046"||
                   q=="road_table_choice_4700d0_r046"||q=="road_side_test_479a70_r046"||
                   q=="road_lane_classify_47b890_r046"||q=="refresh_cached_road_4a3f80_r046"||
                   q=="get_road_ofs_4a4010_r046"||q=="common_pl_car_4a8100_r046";
        }
        if(only=="r047_game_control_batch"){
            const std::string q=n;
            return q=="game_flag_43f9c0_r047"||q=="set_game_flag_43f9d0_r047"||
                   q=="set_game_state_byte_43f9e0_r047"||q=="game_state_byte_43f9f0_r047"||
                   q=="game_state_dword_43fa00_r047"||q=="set_game_state_dword_43fa10_r047"||
                   q=="game_broadcast_43cc20_r047"||q=="operation_input_49fad0_r047"||
                   q=="check_shift_warning_4a50f0_r047"||q=="car_calc_total_cs_len_455f50_r047"||
                   q=="car_calc_current_stage_progress_4a2130_r047"||q=="get_now_heart_calc_mode_45c440_r047"||
                   q=="set_old_param_buffer_4a2ee0_r047";
        }
        if(only=="r048_game_parent_batch"){
            const std::string q=n;
            return q=="race_counter_44fdf0_r048"||q=="set_timeup_counter_44fe30_r048"||
                   q=="get_timeup_counter_44fe40_r048"||q=="set_race_flag0_44fe50_r048"||
                   q=="get_race_flag0_44fe70_r048"||q=="set_race_flag2_44fef0_r048"||
                   q=="ham_nos_speed_45d0e0_r048"||q=="game_pl_car_ctrl_4a8330_r048";
        }
        if(only=="r049_game_children_batch"){
            const std::string q=n;
            return q=="control_timeup_braking_49fb70_r049"||q=="check_wanderer_4a5260_r049"||
                   q=="ham_nos_set_speed_4a5650_r049"||q=="network_tail_state_55a930_r049"||
                   q=="network_tail_forward_46c390_r049"||q=="rank_provider_gateway_45a2b0_r049";
        }
        if(only=="r050_game_children_batch")return std::string(n)=="check_slipstream_4a4d20_r050";
        if(only=="r051_pas_parent_batch")return std::string(n)=="pas_pl_car_ctrl_475720_r051";
        if(only=="r052_event_scheduler_batch"){
            const std::string q=n;
            return q=="event_control_43fab0_r052"||q=="event_open_440180_r052"||q=="event_close_4401d0_r052"||
                   q=="event_close_immediate_440200_r052"||q=="event_close_all_440240_r052"||q=="event_close_serial_440330_r052"||
                   q=="check_event_destructing_440370_r052"||q=="get_event_id_440b30_r052"||q=="get_now_event_id_440b80_r052"||
                   q=="change_now_event_ctrl_func_440b90_r052"||q=="change_now_event_shadow_func_440ba0_r052"||
                   q=="change_ctrl_func_440bb0_r052"||q=="change_disp_scene_440bd0_r052";
        }
        if(only=="r053_event_bootstrap_batch"){
            const std::string q=n;
            return q=="init_event_control_440bf0_r053"||q=="event_suspend_440a10_r053"||
                   q=="event_resume_440a30_r053"||q=="check_event_suspend_440a50_r053";
        }
        if(only=="r054_event_work_batch"){
            const std::string q=n;
            return q=="set_ev_pause_flag_440930_r054"||q=="clr_ev_pause_flag_4409c0_r054"||
                   q=="malloc_now_event_work_440a60_r054"||q=="free_now_event_work_440b20_r054"||
                   q=="free_event_work_handle_440cd0_r054";
        }
        if(only=="r055_allocator_state_batch"){
            const std::string q=n;
            return q=="push_alloc_state_a_440d10_r055"||q=="pop_alloc_state_a_440d30_r055"||
                   q=="push_alloc_state_b_440d50_r055"||q=="pop_alloc_state_b_440d70_r055"||
                   q=="hmm_handle_move_440cc0_r055"||q=="masked_service_440ca0_r055";
        }
        if(only=="r056_object_state_batch")return r056_oracle_name(n)&&std::string(n)!="r056_object_state_batch";
        if(only=="r057_transition_batch")return r057_oracle_name(n)&&std::string(n)!="r057_transition_batch";
        if(only=="r058_runtime_batch")return r058_oracle_name(n)&&std::string(n)!="r058_runtime_batch";
        if(only=="r059_ui_batch")return r059_oracle_name(n)&&std::string(n)!="r059_ui_batch";
        if(only=="r060_factory_batch")return r060_oracle_name(n)&&std::string(n)!="r060_factory_batch";
        if(only=="r061_factory_batch")return r061_oracle_name(n)&&std::string(n)!="r061_factory_batch";
        if(only=="r062_constructor_batch")return r062_oracle_name(n)&&std::string(n)!="r062_constructor_batch";
        if(only=="r063_state_selector_batch")return r063_oracle_name(n)&&std::string(n)!="r063_state_selector_batch";
        if(only=="r064_object_state_batch")return r064_oracle_name(n)&&std::string(n)!="r064_object_state_batch";
        if(only=="r065_state_update_batch")return r065_oracle_name(n)&&std::string(n)!="r065_state_update_batch";
        if(only=="r066_ui_dispatch_batch")return r066_oracle_name(n)&&std::string(n)!="r066_ui_dispatch_batch";
        if(only=="r067_event_factory_batch")return r067_oracle_name(n)&&std::string(n)!="r067_event_factory_batch";
        if(only=="r068_destructor_batch")return r068_oracle_name(n)&&std::string(n)!="r068_destructor_batch";
        if(only=="r069_runtime_init_batch")return r069_oracle_name(n)&&std::string(n)!="r069_runtime_init_batch";
        if(only=="r070_runtime_teardown_batch")return r070_oracle_name(n)&&std::string(n)!="r070_runtime_teardown_batch";
        if(only=="r071_event_open_batch")return r071_oracle_name(n)&&std::string(n)!="r071_event_open_batch";
        if(only=="r072_runtime_list_batch")return r072_oracle_name(n)&&std::string(n)!="r072_runtime_list_batch";
        if(only=="r073_runtime_prepare_batch")return r073_oracle_name(n)&&std::string(n)!="r073_runtime_prepare_batch";
        if(only=="r074_runtime_gate_batch")return r074_oracle_name(n)&&std::string(n)!="r074_runtime_gate_batch";
        if(only=="r075_runtime_blocker_batch")return r075_oracle_name(n)&&std::string(n)!="r075_runtime_blocker_batch";
        if(only=="r076_course_runtime_batch")return r076_oracle_name(n)&&std::string(n)!="r076_course_runtime_batch";
        if(only=="r077_course_provider_batch")return r077_oracle_name(n)&&std::string(n)!="r077_course_provider_batch";
        if(only=="r079_runtime_owner_batch")return r079_oracle_name(n)&&std::string(n)!="r079_runtime_owner_batch";
        if(only=="r085_callback_entry_batch")return r085_oracle_name(n)&&std::string(n)!="r085_callback_entry_batch";
        if(only=="r086_event_function36_batch")return r086_oracle_name(n)&&std::string(n)!="r086_event_function36_batch";
        if(only=="r087_bootstrap_batch")return r087_oracle_name(n)&&std::string(n)!="r087_bootstrap_batch";
        if(only=="r090_platform_init_batch")return r090_oracle_name(n)&&std::string(n)!="r090_platform_init_batch";
        if(only=="r091_loop_cleanup_batch")return r091_oracle_name(n)&&std::string(n)!="r091_loop_cleanup_batch";
        if(only=="r092_loop_setup_batch")return r092_oracle_name(n)&&std::string(n)!="r092_loop_setup_batch";
        if(only=="r093_timing_batch")return r093_oracle_name(n)&&std::string(n)!="r093_timing_batch";
        if(only=="r094_frame_step_batch")return r094_oracle_name(n)&&std::string(n)!="r094_frame_step_batch";
        if(only=="r080_runtime_ui_batch")return r080_oracle_name(n)&&std::string(n)!="r080_runtime_ui_batch";
        if(only=="r081_embedded_slots_batch")return r081_oracle_name(n)&&std::string(n)!="r081_embedded_slots_batch";
        if(only=="r083_runtime_transition_batch")return r083_oracle_name(n)&&std::string(n)!="r083_runtime_transition_batch";
        if(only=="r084_runtime_queue_batch")return r084_oracle_name(n)&&std::string(n)!="r084_runtime_queue_batch";
        return false;
    };
    std::array<std::uint8_t,PcEventSlotCount*0x3cu> r052_saved_records{};
    std::array<std::uint8_t,PcEventSlotCount> r052_saved_flags{};
    std::memcpy(r052_saved_records.data(),reinterpret_cast<void*>(R052RecordBase),r052_saved_records.size());
    std::memcpy(r052_saved_flags.data(),reinterpret_cast<void*>(R052FlagsBase),r052_saved_flags.size());
    const std::uint32_t r052_saved_current=*reinterpret_cast<std::uint32_t*>(R052CurrentPtr);
    const std::uint32_t r052_saved_pause_depth=*reinterpret_cast<std::uint32_t*>(0x7a0db0u);
    const auto r055_saved_allocator=r055_allocator_blob();
    const auto r057_saved_globals=r057_guest_globals();
    const auto r058_saved_globals=r058_guest_globals();
    const auto r073_saved_state=r073_guest_state();
    for(unsigned i=0;i<count;++i){
        auto base=fixture(i);
        if(spline_enabled(only))run_course_spline_cases(i,only);
        if(course_query_enabled(only))run_course_query_cases(i,only);
        if(world_enabled(only))run_world_cases(i,only);
        if(ground_enabled(only))run_ground_cases(i,only);
        if(wall_enabled(only))run_wall_cases(i,only);
        if(response_enabled(only))run_response_cases(i,only);
        if(rebound_enabled(only))run_rebound_cases(i,only);
        if(crash_enabled(only))run_crash_cases(i,only);
        if(entry_enabled(only))run_entry_cases(i,only);
        if(enabled("friction")){
            auto x=base;x.guest();float rpm=x.ev().f32(0x21c);
            prepare(0x501fb0);guest_call.esi=E;guest_call.xmm0=fbits(rpm);run();
            compare_float("engine_friction",ffrom(guest_call.out_xmm0),engine_friction(x.ev(),x.pa(),tables,rpm));
            compare_memory("engine_friction_memory",x);
        }
        if(enabled("clutch")){
            auto x=base;x.guest();prepare(0x500950);guest_call.ecx=E;guest_call.edi=W;run();
            auto_clutch_control(x.ev(),x.wk(),x.pa());compare_memory("auto_clutch_control",x);
        }
        if(enabled("spin")){
            auto x=base;x.guest();prepare(0x518360);Bytes stack(reinterpret_cast<void*>(S),16);stack.put32(0,E);stack.put32(4,W);run();
            induce_spin(x.ev(),x.wk());compare_memory("induce_spin",x);
        }
        if(enabled("grip")){
            auto x=base;x.guest();prepare(0x500c60);guest_call.eax=E;guest_call.esi=W;run();
            tire_grip(x.ev(),x.wk(),x.pa(),x.wheels());compare_memory("tire_grip_with_spin",x);
        }
        if(enabled("torque")){
            auto x=base;x.guest();prepare(0x502a00);guest_call.eax=E;run();
            engine_torque(x.ev(),x.pa(),tables);compare_memory("engine_torque",x);
        }
        if(enabled("predicted")){
            auto x=base;x.guest();prepare(0x502070);guest_call.eax=x.ev().u32(0x208);guest_call.ebx=E;guest_call.esi=W;run();
            compare_float("predicted_engine_speed",ffrom(guest_call.out_xmm0),predicted_engine_speed(x.ev(),x.wk(),x.pa(),x.ev().u32(0x208)));
            compare_memory("predicted_engine_speed_memory",x);
        }
        if(enabled("accel")){
            auto x=base;x.guest();prepare(0x5025c0);guest_call.eax=E;guest_call.esi=W;run();
            accel_operation(x.ev(),x.wk(),x.pa());compare_memory("accel_operation",x);
        }
        if(enabled("auto_transmission")){
            auto x=automatic_fixture(base,i);x.guest();prepare(AutoTransmissionBlock);Bytes(reinterpret_cast<void*>(S),16).put32(0,E);run();
            auto_transmission(x.ev(),x.pa());compare_memory("auto_transmission_deprotected_pc_body",x);
        }
        if(enabled("manual_transmission")){
            auto x=manual_fixture(base,i);const bool inhibited=(i%11==0),up=(i%6==0),down=(i%6==1);set_manual_inputs(inhibited,up,down);
            x.guest();prepare(ManualTransmissionBlock);guest_call.eax=E;Bytes(reinterpret_cast<void*>(S),16).put32(0,W);run();
            manual_transmission(x.ev(),x.wk(),x.pa(),inhibited,up,down);compare_memory("manual_transmission_platform_explicit",x);
        }
        if(enabled("road_mu")){
            auto x=road_mu_fixture(base,i);const bool available=(i%3)!=0;set_road_mu_available(available);
            x.guest();prepare(0x500b30);guest_call.ebx=E;Bytes(reinterpret_cast<void*>(S),16).put32(0,W);run();
            RoadMuSource source{available,nullptr,&deterministic_road_mu};get_road_mu(x.ev(),x.wk(),source);
            compare_memory("get_road_mu_platform_explicit",x);
        }
        if(enabled("driving_control")){
            auto x=driving_control_fixture(base,i);
            const std::int32_t channel=int((i*37u)%256u);
            const bool inhibited=(i%17u)==0,up=(i%11u)==0,down=(i%11u)==1,road_available=(i%3u)!=0;
            const float reaction=0.2f+float(i%23u)*0.03125f;
            *reinterpret_cast<std::int32_t*>(0x7d6824)=channel;
            set_manual_inputs(inhibited,up,down);set_road_mu_available(road_available);
            x.pa().putf(0x20a8,reaction);x.guest();reset_matrix_oracle_globals();
            prepare(0x502c90);Bytes stack(reinterpret_cast<void*>(S),16);stack.put32(0,E);stack.put32(4,W);run();
            if(*reinterpret_cast<std::uint32_t*>(0x89b564u)!=MatrixArena||*reinterpret_cast<std::int32_t*>(0x89b568u)!=0)
                throw std::runtime_error("DrivingControl leaked matrix-stack state");
            DrivingControlInputs in{};in.analog_channel_1=channel;in.input_inhibited=inhibited;in.shift_up=up;in.shift_down=down;
            in.road_mu={road_available,nullptr,&deterministic_road_mu};in.running_resistance=running_tuning();in.reaction_blend_parameter=reaction;
            driving_control(x.ev(),x.wk(),x.pa(),tables,in);compare_memory("driving_control_platform_explicit",x);
        }
        if(enabled("brake")){
            const int pedal=int(i%512)-128;prepare(0x520960);guest_call.st0=1;Bytes(reinterpret_cast<void*>(S),16).puti(0,pedal);run();
            compare_float("brake_pressure",ffrom(guest_call.out_st0),brake_pressure(pedal,tables));
        }
        if(enabled("specific_tire_load")){
            auto x=suspension_fixture(base,i);x.guest();prepare(0x518120);Bytes(reinterpret_cast<void*>(S),16).put32(0,W);run();
            ass_specific_amount_of_tire_load(x.wk());compare_memory("ass_specific_amount_of_tire_load",x);
        }
        if(enabled("diagonal_tire_load")){
            auto x=suspension_fixture(base,i);x.guest();prepare(0x518240);Bytes st(reinterpret_cast<void*>(S),16);st.put32(0,E);st.put32(4,W);run();
            ass_diagonal_tire_load(x.wk(),x.pa());compare_memory("ass_diagonal_tire_load",x);
        }
        if(enabled("suspension_force")){
            auto x=suspension_fixture(base,i);x.guest();prepare(0x4a1b70);Bytes st(reinterpret_cast<void*>(S),16);st.put32(0,E);st.put32(4,W);run();
            suspension_force(x.wk(),x.pa());compare_memory("calc_suspension_force",x);
        }
        if(enabled("tire_load")){
            auto x=suspension_fixture(base,i);x.guest();reset_matrix_oracle_globals();prepare(0x4a1a90);Bytes st(reinterpret_cast<void*>(S),16);st.put32(0,E);st.put32(4,W);run();
            tire_load(x.ev(),x.wk(),x.pa());compare_memory("calc_tire_load",x);
        }
        if(enabled("calc_collision_area")){
            float x=0.0f,z=0.0f;
            switch(i%8u){
              case 0:x=-3072.0f+float(i%257u)*24.0f;z=-3072.0f+float((i*17u)%257u)*24.0f;break;
              case 1:x=std::nextafter(-3072.0f+float(i%257u)*24.0f,-INFINITY);z=std::nextafter(-3072.0f+float((i*13u)%257u)*24.0f,INFINITY);break;
              case 2:x=std::nextafter(-3072.0f+float(i%257u)*24.0f,INFINITY);z=std::nextafter(-3072.0f+float((i*11u)%257u)*24.0f,-INFINITY);break;
              case 3:x=1.0e8f;z=-1.0e8f;break;
              case 4:x=-1.0e8f;z=1.0e8f;break;
              default:x=-5000.0f+float((i*7919u)%100000u)*0.125f;z=-7000.0f+float((i*3571u)%120000u)*0.125f;break;
            }
            prepare(0x43cb40);Bytes st(reinterpret_cast<void*>(S),16);st.putf(0,x);st.putf(4,z);run();
            compare_u32("calc_collision_area",guest_call.out_eax,calc_collision_area(x,z));
        }
        if(enabled("course_triangle_plane_y")||enabled("course_quad_plane_y")){
            auto poly=Bytes(reinterpret_cast<void*>(E),0x40);std::memset(reinterpret_cast<void*>(E),0,0x40);
            const float x0=-200.0f+float((i*37u)%1000u)*0.125f,z0=-150.0f+float((i*53u)%900u)*0.125f;
            const float dx=1.0f+float(i%31u)*0.125f,dz=1.25f+float(i%29u)*0.125f;
            const float sy=-3.0f+float(i%17u)*0.25f,sx=-2.0f+float(i%13u)*0.125f,sz=-1.5f+float(i%11u)*0.1875f;
            auto py=[&](float x,float z){return static_cast<float>(sy+static_cast<float>(sx*x)+static_cast<float>(sz*z));};
            auto putv=[&](std::size_t o,float x,float z){poly.putf(o,x);poly.putf(o+4,py(x,z));poly.putf(o+8,z);};
            // Perimeter order used by PC records: p0 -> z edge -> diagonal p2 -> x edge.
            putv(0x00,x0,z0);putv(0x0c,x0,z0+dz);putv(0x18,x0+dx,z0+dz);putv(0x24,x0+dx,z0);
            const float tx=(i%7u==0u)?0.5f:float((i*17u)%1001u)/1000.0f;
            const float tz=(i%11u==0u)?tx:float((i*23u)%1001u)/1000.0f;
            const float qx=static_cast<float>(x0+static_cast<float>(dx*tx)),qz=static_cast<float>(z0+static_cast<float>(dz*tz));
            if(enabled("course_triangle_plane_y")){
                prepare(0x43cc90);guest_call.eax=E;Bytes st(reinterpret_cast<void*>(S),16);st.putf(0,qx);st.putf(4,qz);run();
                compare_float("course_triangle_plane_y",ffrom(guest_call.out_xmm0),course_triangle_plane_y(poly,qx,qz));
            }
            if(enabled("course_quad_plane_y")){
                prepare(0x43de80);guest_call.eax=E;Bytes st(reinterpret_cast<void*>(S),16);st.putf(0,qx);st.putf(4,qz);run();
                compare_float("course_quad_plane_y",ffrom(guest_call.out_xmm0),course_quad_plane_y(poly,qx,qz));
            }
        }
        if(enabled("update_easy_lct_prediction")){
            EasyLctPredictionState native{};native.cursor=(i*2654435761u)^0xa5a50000u;native.easy=0xdeadbeefu;
            for(unsigned k=0;k<16;++k)native.recent[k]=(((i+k*7u)%5u)==0u)?0u:((i+k)&3u);
            *reinterpret_cast<std::uint32_t*>(0x780240u)=native.cursor;*reinterpret_cast<std::uint32_t*>(0x78023cu)=native.easy;
            for(unsigned k=0;k<16;++k)*reinterpret_cast<std::uint32_t*>(0x7801a8u+k*4u)=native.recent[k];
            const std::uint32_t type=((i*3u)%7u)==0u?0u:1u+(i%3u);
            prepare(0x43cbc0);guest_call.edx=type;run();
            update_easy_lct_prediction_table(native,type);compare_prediction_state(native);
        }
        if(enabled("course_collision_offset_direction")){
            const std::uint32_t course=i&1u,idx=(i*29u)%1024u;const bool present=(i%19u)!=0u;
            const std::uint32_t base=CourseTableArena+course*0x10000u;auto table=Bytes(reinterpret_cast<void*>(std::uintptr_t(base)),0x10000);
            const std::int16_t local=static_cast<std::int16_t>((i*40503u)^0xa55au);table.put16(std::size_t(idx)*0x40u+0x3eu,static_cast<std::uint16_t>(local));
            const float yaw=-3.5f+float(i%14001u)*0.0005f;
            *reinterpret_cast<float*>(course?0x7d317cu:0x7d3128u)=yaw;
            *reinterpret_cast<std::uint32_t*>(0x780110u+course*4u)=present?base:0u;
            prepare(0x43d340);Bytes st(reinterpret_cast<void*>(S),16);st.put32(0,idx);st.put32(4,course);run();
            compare_u32("course_collision_offset_direction",guest_call.out_eax,static_cast<std::uint32_t>(course_collision_offset_direction(table,idx,yaw,present)));
            *reinterpret_cast<std::uint32_t*>(0x780110u+course*4u)=base;
        }
        if(enabled("course_length")){
            const std::uint32_t course=i&1u;const std::int32_t idx=(i%23u==0u)?-1:std::int32_t((i*31u)%8192u);
            const std::uint32_t base=CourseLengthArena+course*0x10000u;auto table=Bytes(reinterpret_cast<void*>(std::uintptr_t(base)),0x10000);
            if(idx>=0)table.put16(std::size_t(idx)*2u,std::uint16_t((i*31337u)^0x5aa5u));
            *reinterpret_cast<std::uint32_t*>(0x780228u+course*4u)=base;
            prepare(0x401810);Bytes st(reinterpret_cast<void*>(S),16);st.put32(0,course);st.puti(4,idx);run();
            compare_u32("course_length",guest_call.out_eax&0xffffu,course_length(table,idx));
        }
        if(enabled("course_collision_ext_flags")){
            auto table=Bytes(reinterpret_cast<void*>(CourseTableArena),0x10000);const std::int32_t idx=(i%17u==0)?-1:std::int32_t(i%512u);
            if(idx>=0)table.put16(std::size_t(idx)*0x40u+0x3cu,std::uint16_t((i*40503u)^0xa55au));
            prepare(0x43d440);Bytes st(reinterpret_cast<void*>(S),16);st.put32(0,0);st.puti(4,idx);run();
            compare_u32("course_collision_ext_flags",guest_call.out_eax&0xffffu,course_collision_ext_flags(table,idx));
        }
        if(enabled("get_y_position_spl_chk")){
            patch_get_y_position_spl_query_calls();
            const float x=1.25f+float(i%7u)*0.125f,y=2.5f-float(i%5u)*0.25f,z=-3.75f+float(i%11u)*0.0625f;
            std::array<CourseQueryResult,4> script{};
            const unsigned hits=1u+(i%4u);
            for(unsigned k=0;k<4;++k){script[k].y=10.0f+float(k)+float(i%3u)*0.5f;script[k].course_or_return=0x100u+k+i;script[k].collision_index=0x200u+k+i;script[k].flags=(k+1u<hits)?1u:((i&1u)?0x00200002u:0u);}
            course_stub_index=course_stub_overflow=0;
            for(unsigned k=0;k<4;++k){course_stub_y[k]=fbits(script[k].y);course_stub_collision[k]=script[k].collision_index;course_stub_flags[k]=script[k].flags;course_stub_ret[k]=script[k].course_or_return;course_stub_seen_x[k]=course_stub_seen_y[k]=course_stub_seen_z[k]=0;}
            auto* point=reinterpret_cast<float*>(E);point[0]=x;point[1]=y;point[2]=z;
            Bytes st(reinterpret_cast<void*>(S),32);st.put32(0,E);st.put32(4,E+0x20);st.put32(8,E+0x24);st.put32(12,E+0x28);
            *reinterpret_cast<std::uint32_t*>(E+0x20)=0xdeadbeefu;*reinterpret_cast<std::uint32_t*>(E+0x24)=0xcafebabeu;*reinterpret_cast<std::uint32_t*>(E+0x28)=0xabcdef01u;
            prepare(0x518e10);run();
            const float guest_y=point[1];const auto guest_ci=*reinterpret_cast<std::uint32_t*>(E+0x20),guest_si=*reinterpret_cast<std::uint32_t*>(E+0x24),guest_fl=*reinterpret_cast<std::uint32_t*>(E+0x28),guest_ret=guest_call.out_eax;
            std::array<CourseProbe,4> seen{};unsigned ncalls=0;
            struct Local{std::array<CourseQueryResult,4>* s;std::array<CourseProbe,4>* seen;unsigned* n;};Local loc{&script,&seen,&ncalls};
            auto cb=[](std::uint32_t mask,const CourseProbe& p,void* u)->CourseQueryResult{auto& l=*static_cast<Local*>(u);if(mask!=0x400u)throw std::runtime_error("native course mask mismatch");(*l.seen)[*l.n]=p;return (*l.s)[(*l.n)++];};
            CourseProbe np{x,y,z};std::uint32_t ci=0xdeadbeefu,si=0xcafebabeu,fl=0xabcdef01u,ret=0;CourseQuery cq{cb,&loc};
            if(!get_y_position_spl_chk(np,&ci,&si,&fl,cq,&ret))throw std::runtime_error("native course wrapper rejected callback");
            compare_float("get_y_position_spl_chk_y",guest_y,np.y);compare_u32("get_y_position_spl_chk_collision",guest_ci,ci);compare_u32("get_y_position_spl_chk_special",guest_si,si);compare_u32("get_y_position_spl_chk_flags",guest_fl,fl);compare_u32("get_y_position_spl_chk_return",guest_ret,ret);
            if(course_stub_overflow||course_stub_index!=ncalls)throw std::runtime_error("course query call-count mismatch");
            for(unsigned k=0;k<ncalls;++k){compare_float("get_y_position_spl_chk_probe_x",ffrom(course_stub_seen_x[k]),seen[k].x);compare_float("get_y_position_spl_chk_probe_y",ffrom(course_stub_seen_y[k]),seen[k].y);compare_float("get_y_position_spl_chk_probe_z",ffrom(course_stub_seen_z[k]),seen[k].z);}
        }
        if(only=="all"||only=="get_y_position_spl_chk")patch_get_y_position_spl_query_calls(true);
        if(enabled("car_sus_coli_check")){
            auto x=car_sus_coli_fixture(base,i);x.guest();reset_matrix_oracle_globals();
            prepare(0x40a170);Bytes st0(reinterpret_cast<void*>(S),16);st0.put32(0,W+0x10);run();
            prepare(0x518fa0);Bytes st(reinterpret_cast<void*>(S),16);st.put32(0,E);st.put32(4,W);run();
            car_sus_coli_check(x.ev(),x.wk(),x.pa(),x.wheels());
            compare_memory("car_sus_coli_check",x);
        }
        if(enabled("collision_suspension_chain")){
            auto x=car_sus_coli_fixture(base,i);x.guest();reset_matrix_oracle_globals();
            prepare(0x40a170);Bytes setm(reinterpret_cast<void*>(S),16);setm.put32(0,W+0x10);run();
            prepare(0x518fa0);Bytes s1(reinterpret_cast<void*>(S),16);s1.put32(0,E);s1.put32(4,W);run();
            car_sus_coli_check(x.ev(),x.wk(),x.pa(),x.wheels());compare_memory("chain_car_sus_coli_check",x);
            prepare(0x519170);Bytes s2(reinterpret_cast<void*>(S),16);s2.put32(0,E);s2.put32(4,W);run();
            car_sus_bump_push(x.ev(),x.wk(),x.pa(),x.wheels());compare_memory("chain_car_sus_bump_push",x);
            prepare(0x4a1b70);Bytes s3(reinterpret_cast<void*>(S),16);s3.put32(0,E);s3.put32(4,W);run();
            suspension_force(x.wk(),x.pa());compare_memory("chain_calc_suspension_force",x);
            prepare(0x4a1a90);Bytes s4(reinterpret_cast<void*>(S),16);s4.put32(0,E);s4.put32(4,W);run();
            tire_load(x.ev(),x.wk(),x.pa());compare_memory("chain_calc_tire_load",x);
        }
        if(enabled("car_sus_bump_push")){
            auto x=collision_fixture(base,i);x.guest();reset_matrix_oracle_globals();
            // ColiCar loads work+0x10 into the game's current matrix before invoking this leaf.
            prepare(0x40a170);Bytes st0(reinterpret_cast<void*>(S),16);st0.put32(0,W+0x10);run();
            prepare(0x519170);Bytes st(reinterpret_cast<void*>(S),16);st.put32(0,E);st.put32(4,W);run();
            car_sus_bump_push(x.ev(),x.wk(),x.pa(),x.wheels());
            compare_memory("car_sus_bump_push",x);
        }
        if(enabled("copy_car_work_4a1140_r037")){
            auto q=copy_car_fixture(base,i);auto& x=q.x;
            x.guest();std::memcpy(reinterpret_cast<void*>(P),q.body.data(),q.body.size());reset_matrix_oracle_globals();
            prepare(0x4a1140);guest_call.esi=E;guest_call.edi=W;run();
            std::array<std::uint8_t,64> native_matrix{};PcMatrixStack ms{Bytes(native_matrix.data(),native_matrix.size()),0,0,1};
            copy_car_work_4a1140(x.ev(),x.wk(),Bytes(q.body.data(),q.body.size()),x.wheels(),ms);
            compare_memory("copy_car_work",x);compare_matrix_current("copy_car_work_matrix",ms.current());
            compare_u32("copy_car_work_matrix_pointer",*reinterpret_cast<std::uint32_t*>(0x89b564u),MatrixArena);
            compare_u32("copy_car_work_matrix_depth",std::uint32_t(*reinterpret_cast<std::int32_t*>(0x89b568u)),0u);
        }
        if(enabled("set_car_camera_4a1680_r038")){
            auto x=set_car_camera_fixture(base,i);x.guest();reset_matrix_oracle_globals();
            prepare(0x4a1680);guest_call.esi=E;Bytes st(reinterpret_cast<void*>(S),16);st.put32(0,W);run();
            std::array<std::uint8_t,64> native_matrix{};PcMatrixStack ms{Bytes(native_matrix.data(),native_matrix.size()),0,0,1};
            set_car_camera_4a1680(x.ev(),x.wk(),ms);
            compare_memory("set_car_camera",x);compare_matrix_current("set_car_camera_matrix",ms.current());
            compare_u32("set_car_camera_matrix_pointer",*reinterpret_cast<std::uint32_t*>(0x89b564u),MatrixArena);
            compare_u32("set_car_camera_matrix_depth",std::uint32_t(*reinterpret_cast<std::int32_t*>(0x89b568u)),0u);
        }
        if(enabled("check_driving_skill_4a4830_r038")){
            auto x=driving_skill_fixture(base,i);const auto in=driving_skill_inputs(i);x.guest();set_driving_skill_inputs(in);
            prepare(0x4a4830);guest_call.esi=E;run();
            check_driving_skill_4a4830(x.ev(),in);compare_memory("check_driving_skill",x);
        }
        if(enabled("check_chicken_driver_4a4900_r039")){
            auto x=chicken_driver_fixture(base,i);const auto in=chicken_driver_inputs(i);x.guest();set_chicken_driver_inputs(in);
            prepare(0x4a4900);guest_call.ebx=E;run();
            check_chicken_driver_4a4900(x.ev(),in);compare_memory("check_chicken_driver",x);
        }
        if(enabled("assist_chicken_driver_4a4ba0_r040")){
            const auto in=assist_chicken_inputs(i);auto x=assist_chicken_fixture(base,i);x.guest();set_assist_chicken_inputs(in);
            prepare(0x4a4ba0);guest_call.esi=E;run();
            assist_chicken_driver_4a4ba0(x.ev(),in);compare_memory("assist_chicken_driver",x);
        }
        if(enabled("calc_vibrate_matrix_4a2d70_r040")){
            auto x=vibrate_fixture(base,i);auto hist=vibrate_history_fixture(i);auto native_hist=hist;x.guest();
            std::memcpy(reinterpret_cast<void*>(VibrateHistoryBase),hist.data(),hist.size());
            prepare(0x4a2d70);guest_call.eax=E;run();
            calc_vibrate_matrix_4a2d70(x.ev(),Bytes(native_hist.data(),native_hist.size()));
            compare_memory("calc_vibrate_matrix",x);
            compare_blob("calc_vibrate_matrix_history",reinterpret_cast<void*>(VibrateHistoryBase),native_hist);
        }
        if(enabled("check_reverse_car_4a2910_r041")){
            const auto in=reverse_car_inputs(i);auto x=reverse_car_fixture(base,i,in);x.guest();set_reverse_car_inputs(in);reset_matrix_oracle_globals();
            prepare(0x4a2910);Bytes st(reinterpret_cast<void*>(S),16);st.put32(0,E);run();
            std::array<std::uint8_t,128> native_matrix{};PcMatrixStack ms{Bytes(native_matrix.data(),native_matrix.size()),0,0,2};
            check_reverse_car_4a2910(x.ev(),in,ms);
            compare_memory("check_reverse_car",x);
            compare_blob("check_reverse_car_matrix_arena",reinterpret_cast<void*>(MatrixArena),native_matrix);
            compare_u32("check_reverse_car_matrix_pointer",*reinterpret_cast<std::uint32_t*>(0x89b564u),MatrixArena);
            compare_u32("check_reverse_car_matrix_depth",std::uint32_t(*reinterpret_cast<std::int32_t*>(0x89b568u)),0u);
        }

        if(enabled("course_nested_marker_44bdb0_r045")){
            const bool present=(i&1u)!=0u;const std::uint32_t marker=0x51000000u^(i*0x10203u);
            *reinterpret_cast<std::uint32_t*>(0x7d3188u)=present?R045CourseStateBase:0u;
            *reinterpret_cast<std::uint32_t*>(R045CourseStateBase+0x14u)=R045CourseStateBase+0x100u;
            *reinterpret_cast<std::uint32_t*>(R045CourseStateBase+0x100u+0x3cu)=marker;
            prepare(0x44bdb0u);run();compare_u32("course_nested_marker",guest_call.out_eax,course_nested_marker_44bdb0(present,marker));
        }
        if(enabled("course_stage_limit_44be00_r045")){
            const std::uint32_t v=0x89abcdefu^(i*0x9e3779b9u);*reinterpret_cast<std::uint32_t*>(0x7d33acu)=v;
            prepare(0x44be00u);run();compare_u32("course_stage_limit",guest_call.out_eax,course_stage_limit_44be00(v));
        }
        if(enabled("course_active_slot_44be10_r045")){
            const bool present=(i%3u)!=0u;const std::uint32_t slot=(i*7u)%32u;
            *reinterpret_cast<std::uint32_t*>(0x7d31dcu)=present?R045CourseStateBase+0x200u:0u;
            *reinterpret_cast<std::uint32_t*>(R045CourseStateBase+0x204u)=slot;
            prepare(0x44be10u);run();compare_u32("course_active_slot",guest_call.out_eax,course_active_slot_44be10(present,slot));
        }
        if(enabled("course_primary_ready_44be30_r045")){
            static constexpr std::int32_t vals[]={-2147483647-1,-1,0,9,10,17,18,19,20,21,2147483647};const auto v=vals[i%std::size(vals)];
            *reinterpret_cast<std::int32_t*>(0x7d2e80u)=v;prepare(0x44be30u);run();compare_u32("course_primary_ready",guest_call.out_eax,course_primary_ready_44be30(v)?1u:0u);
        }
        if(enabled("course_secondary_ready_44be40_r045")){
            static constexpr std::int32_t vals[]={-2147483647-1,-1,0,9,10,17,18,19,20,21,2147483647};const auto v=vals[(i*3u)%std::size(vals)];
            *reinterpret_cast<std::int32_t*>(0x7d2e88u)=v;prepare(0x44be40u);run();compare_u32("course_secondary_ready",guest_call.out_eax,course_secondary_ready_44be40(v)?1u:0u);
        }
        if(enabled("course_type_gate_44be50_r045")){
            static constexpr std::int32_t vals[]={-1,0,8,9,10,17,18,19,20,21,99};const auto p=vals[i%std::size(vals)],q=vals[(i*5u+2u)%std::size(vals)];const std::int32_t type=(i%4u)==0u?0:std::int32_t((i%7u)+1u);
            *reinterpret_cast<std::int32_t*>(0x7d2e80u)=p;*reinterpret_cast<std::int32_t*>(0x7d2e88u)=q;
            prepare(0x44be50u);Bytes(reinterpret_cast<void*>(S),16).puti(0,type);run();compare_u32("course_type_gate",guest_call.out_eax,course_type_gate_44be50(type,p,q)?1u:0u);
        }
        if(enabled("course_index_lookup_44be80_r045")){
            const std::int32_t idx=std::int32_t((i*13u)%72u);prepare(0x44be80u);Bytes(reinterpret_cast<void*>(S),16).puti(0,idx);run();
            compare_u32("course_index_lookup",guest_call.out_eax,course_index_lookup_44be80(Bytes(reinterpret_cast<void*>(0x635f40u),66u*4u),idx));
        }
        if(enabled("course_disp_matrix_choice_44bed0_r045")){
            const bool secondary=(i&1u)!=0u;prepare(0x44bed0u);Bytes(reinterpret_cast<void*>(S),16).put32(0,secondary?1u:0u);run();
            const auto native=course_disp_matrix_choice_44bed0(secondary);const std::uint32_t expected=native==PcCourseMatrixChoice::Secondary?0x7d3190u:0x7d2da0u;compare_u32("course_disp_matrix_choice",guest_call.out_eax,expected);
        }
        if(enabled("course_area_matrix_choice_44bef0_r045")){
            const bool secondary=(i&1u)!=0u;prepare(0x44bef0u);Bytes(reinterpret_cast<void*>(S),16).put32(0,secondary?1u:0u);run();
            const auto native=course_area_matrix_choice_44bef0(secondary);const std::uint32_t expected=native==PcCourseMatrixChoice::Secondary?0x7d3178u:0x7d3124u;compare_u32("course_area_matrix_choice",guest_call.out_eax,expected);
        }
        if(enabled("course_clear_service_state_44bf10_r045")){
            PcCourseServiceState native{0x11110000u^i,0x22220000u^(i*3u),0x33330000u^(i*5u),0x44440000u^(i*7u),std::uint8_t(i)};
            *reinterpret_cast<std::uint32_t*>(0x7d33bcu)=native.clear_bc;*reinterpret_cast<std::uint32_t*>(0x7d33c4u)=native.clear_c4;*reinterpret_cast<std::uint32_t*>(0x7d3188u)=native.manager_ptr;
            prepare(0x44bf10u);run();course_clear_service_state_44bf10(native);
            compare_u32("course_clear_bc",*reinterpret_cast<std::uint32_t*>(0x7d33bcu),native.clear_bc);compare_u32("course_clear_c4",*reinterpret_cast<std::uint32_t*>(0x7d33c4u),native.clear_c4);compare_u32("course_clear_manager",*reinterpret_cast<std::uint32_t*>(0x7d3188u),native.manager_ptr);
        }
        if(enabled("course_mark_ready_44c080_r045")){
            PcCourseServiceState native{};native.ready=0x55550000u^i;*reinterpret_cast<std::uint32_t*>(0x7d2d8cu)=native.ready;prepare(0x44c080u);run();course_mark_ready_44c080(native);compare_u32("course_mark_ready",*reinterpret_cast<std::uint32_t*>(0x7d2d8cu),native.ready);
        }
        if(enabled("course_get_mode_byte_44c090_r045")){
            PcCourseServiceState native{};native.mode_byte=std::uint8_t(i*37u);*reinterpret_cast<std::uint8_t*>(0x7d31d0u)=native.mode_byte;prepare(0x44c090u);run();compare_u32("course_get_mode_byte",guest_call.out_eax,course_get_mode_byte_44c090(native));
        }
        if(enabled("course_set_mode_byte_44c0a0_r045")){
            PcCourseServiceState native{};native.mode_byte=std::uint8_t(~i);*reinterpret_cast<std::uint8_t*>(0x7d31d0u)=native.mode_byte;const std::uint8_t v=std::uint8_t(i*53u+7u);
            prepare(0x44c0a0u);Bytes(reinterpret_cast<void*>(S),16).put32(0,v);run();course_set_mode_byte_44c0a0(native,v);compare_u32("course_set_mode_byte",*reinterpret_cast<std::uint8_t*>(0x7d31d0u),native.mode_byte);
        }
        if(enabled("course_copy_snapshot_44c0b0_r045")){
            std::array<std::uint8_t,120> src{},dst{},native_src{},native_dst{};Bytes sv(src.data(),src.size()),dv(dst.data(),dst.size());
            for(unsigned k=0;k<30;++k){sv.put32(k*4u,0x72000000u^(i*0x10101u)^(k*0x10203u));dv.put32(k*4u,0x4a000000u^(i*17u)^k);}native_src=src;native_dst=dst;
            std::memcpy(reinterpret_cast<void*>(0x7d2de0u),src.data(),src.size());std::memcpy(reinterpret_cast<void*>(0x7d30a8u),dst.data(),dst.size());
            prepare(0x44c0b0u);run();course_copy_snapshot_44c0b0(Bytes(native_dst.data(),native_dst.size()),Bytes(native_src.data(),native_src.size()));
            compare_blob("course_copy_snapshot",reinterpret_cast<void*>(0x7d30a8u),native_dst);compare_blob("course_copy_snapshot_source",reinterpret_cast<void*>(0x7d2de0u),native_src);
        }


        if(only=="all"){set_r046_road_patches(true);set_r046_common_parent_patches(true);}
        if(only=="all"||only=="r046_road_services_batch"||only=="course_stage_unique_44dc50_r046"||only=="road_stage_window_44ddc0_r046"||only=="road_stage_gate_44f0f0_r046"||only=="road_lane_classify_47b890_r046"||only=="get_road_ofs_4a4010_r046"){
            *reinterpret_cast<std::uint32_t*>(0x7d2df4u)=R046StateBase+0x600u;
            *reinterpret_cast<std::uint32_t*>(0x6a55c8u)=R046StateBase+0x700u;
        }
        if(enabled("course_stage_unique_44dc50_r046")){
            const bool present=(i%3u)!=0u;const std::uint32_t value=0x11000000u^(i*0x10103u),fallback=0x22000000u^(i*0x30507u);
            set_r046_descriptor(present,value,fallback);prepare(0x44dc50u);Bytes(reinterpret_cast<void*>(S),16).put32(0,i%80u);run();
            compare_u32("course_stage_unique",guest_call.out_eax,std::uint32_t(course_stage_unique_44dc50(present,value,fallback)));
        }
        if(enabled("road_stage_window_44ddc0_r046")){
            static constexpr std::int32_t stages[]={0x1c,0x3a,0,1,0x7f};const auto stage=stages[i%std::size(stages)];
            const std::uint16_t pos=std::uint16_t((i*97u+0x40u)&0xffffu),rolling=std::uint16_t((i*131u+0x280u)&0xffffu);
            *reinterpret_cast<std::uint16_t*>(R046StateBase+0x700u+0x7eu)=rolling;prepare(0x44ddc0u);Bytes st(reinterpret_cast<void*>(S),16);st.puti(0,stage);st.put32(4,pos);run();
            compare_u32("road_stage_window",guest_call.out_eax,std::uint32_t(road_stage_window_44ddc0(stage,pos,rolling)));
        }
        if(enabled("road_stage_gate_44f0f0_r046")){
            static constexpr std::int32_t stages[]={0x1c,0x3a,0,0x44};const auto stage=stages[i%std::size(stages)];
            const std::uint16_t pos=std::uint16_t((i*59u+0x50u)&0xffffu),rolling=std::uint16_t((i*113u+0x260u)&0xffffu);const std::uint32_t gate=0xa5000000u^(i*0x10203u);
            *reinterpret_cast<std::uint16_t*>(R046StateBase+0x700u+0x7eu)=rolling;*reinterpret_cast<std::uint32_t*>(R046StateBase+0x14u)=gate;
            prepare(0x44f0f0u);Bytes st(reinterpret_cast<void*>(S),16);st.puti(0,stage);st.put32(4,pos);run();
            compare_u32("road_stage_gate",guest_call.out_eax,road_stage_gate_44f0f0(stage,pos,rolling,gate));
        }
        if(enabled("road_decode_sample_46ffc0_r046")){
            std::array<std::uint8_t,6> packed{};std::array<std::uint8_t,0x14> native{};Bytes pb(packed.data(),packed.size()),nb(native.data(),native.size());
            const std::int16_t x=std::int16_t((i*911u)^0x5a5au),z=std::int16_t((i*613u)^0xa55au);const std::uint16_t rawy=std::uint16_t((i*389u)&0x7fffu)|((i&3u)==0u?0x8000u:0u);
            pb.put16(0,std::uint16_t(x));pb.put16(2,std::uint16_t(z));pb.put16(4,rawy);std::memcpy(reinterpret_cast<void*>(W),packed.data(),packed.size());std::memset(reinterpret_cast<void*>(W+0x20u),0x5a,native.size());
            prepare(0x46ffc0u);Bytes st(reinterpret_cast<void*>(S),16);st.put32(0,W);st.put32(4,W+0x20u);run();road_decode_sample_46ffc0(pb,nb);
            compare_blob("road_decode_sample",reinterpret_cast<void*>(W+0x20u),native);
        }
        if(enabled("road_table_choice_4700d0_r046")){
            const std::int32_t type=(i&1u)?1:0,row=std::int32_t((i*3u)%6u),selector=type?std::int32_t((i*5u)%12u):std::int32_t((i*5u)%8u);
            prepare(0x4700d0u);Bytes st(reinterpret_cast<void*>(S),16);st.puti(0,type);st.puti(4,row);st.puti(8,selector);run();const auto c=road_table_choice_4700d0(type,row,selector);
            const std::uint32_t expected=!c.valid?0u:std::uint32_t((c.secondary?0x80a670u:0x804480u)+c.byte_offset);compare_u32("road_table_choice",guest_call.out_eax,expected);
        }
        if(enabled("road_side_test_479a70_r046")){
            std::array<Bytes,6> prim{Bytes(reinterpret_cast<void*>(0x804480u+0u*0x1030u),0x100),Bytes(reinterpret_cast<void*>(0x804480u+1u*0x1030u),0x100),Bytes(reinterpret_cast<void*>(0x804480u+2u*0x1030u),0x100),Bytes(reinterpret_cast<void*>(0x804480u+3u*0x1030u),0x100),Bytes(reinterpret_cast<void*>(0x804480u+4u*0x1030u),0x100),Bytes(reinterpret_cast<void*>(0x804480u+5u*0x1030u),0x100)};
            std::array<Bytes,12> sec{Bytes(reinterpret_cast<void*>(0x80a670u+0u*0x70cu),0x100),Bytes(reinterpret_cast<void*>(0x80a670u+1u*0x70cu),0x100),Bytes(reinterpret_cast<void*>(0x80a670u+2u*0x70cu),0x100),Bytes(reinterpret_cast<void*>(0x80a670u+3u*0x70cu),0x100),Bytes(reinterpret_cast<void*>(0x80a670u+4u*0x70cu),0x100),Bytes(reinterpret_cast<void*>(0x80a670u+5u*0x70cu),0x100),Bytes(reinterpret_cast<void*>(0x80a670u+6u*0x70cu),0x100),Bytes(reinterpret_cast<void*>(0x80a670u+7u*0x70cu),0x100),Bytes(reinterpret_cast<void*>(0x80a670u+8u*0x70cu),0x100),Bytes(reinterpret_cast<void*>(0x80a670u+9u*0x70cu),0x100),Bytes(reinterpret_cast<void*>(0x80a670u+10u*0x70cu),0x100),Bytes(reinterpret_cast<void*>(0x80a670u+11u*0x70cu),0x100)};
            const std::int32_t type=(i&1u)?1:0;const std::int8_t cur=std::int8_t(type?(i*3u)%12u:(i*3u)%6u),alt=std::int8_t(type?(i*5u+1u)%12u:(i*5u+1u)%6u);const std::int16_t cs=std::int16_t(i%8u);const std::size_t rec=4u+std::size_t(cs)*6u;
            Bytes cb=type?sec[std::size_t(cur)]:prim[std::size_t(cur)],ab=type?sec[std::size_t(alt)]:prim[std::size_t(alt)];for(std::size_t k=0;k<0x80u;++k){cb.put8(k,0);ab.put8(k,0);}cb.put16(0,0x4f53u);ab.put16(0,0x4f53u);
            cb.put16(rec,std::uint16_t(std::int16_t(200+int(i%97u))));cb.put16(rec+2,std::uint16_t(std::int16_t(-120+int(i%83u))));cb.put16(rec+4,std::uint16_t(i&0x3fffu));
            ab.put16(rec,std::uint16_t(std::int16_t(-180+int(i%67u))));ab.put16(rec+2,std::uint16_t(std::int16_t(140-int(i%71u))));ab.put16(rec+4,std::uint16_t((i*7u)&0x3fffu));
            if(i%11u==0u)cb.put16(0,0u);else if(i%13u==0u)cb.put8(rec+5,cb.u8(rec+5)|0x80u);else if(i%17u==0u)ab.put16(0,0u);
            std::array<std::uint8_t,0x10> place{};Bytes pl(place.data(),place.size());pl.puti(0,type);pl.put16(8,std::uint16_t(cs));std::memcpy(reinterpret_cast<void*>(W+0x100u),place.data(),place.size());
            const CourseProbe point{float(int(i%41u)-20)*6.25f,0.0f,float(int(i%37u)-18)*5.75f};std::memcpy(reinterpret_cast<void*>(W+0x200u),&point,sizeof(point));
            prepare(0x479a70u);Bytes st(reinterpret_cast<void*>(S),20);st.put32(0,W+0x200u);st.put32(4,W+0x100u);st.put32(8,std::uint32_t(std::int32_t(cur)));st.put32(12,std::uint32_t(std::int32_t(alt)));run();
            const PcRoadSampleTables tables{prim.data(),prim.size(),sec.data(),sec.size()};const bool native=road_side_test_479a70(point,pl,cur,alt,tables);compare_u32("road_side_test",guest_call.out_eax,native?1u:0u);
        }
        if(enabled("road_lane_classify_47b890_r046")){
            std::array<std::uint8_t,event_size> native{};Bytes ne(native.data(),native.size());
            ne.put8(4,(i%3u)==0u?0u:1u);ne.puti(0x5c,(i%5u)==0u?1:((i%7u)==0u?2:0));ne.puti(0x60,(i&1u)?100:101);ne.put16(0x64,std::uint16_t((i*41u+0x60u)&0x3ffu));ne.put8(0x66,std::uint8_t(std::int8_t(int(i%9u)-2)));ne.putf(0x268,float(int(i%41u)-20)*0.75f);ne.putf(0xb08,float(int(i%31u)-15)*0.5f);ne.put8(0xc32,0xa5u);ne.put8(0xc33,0x5au);
            PcRoadLaneInputs in{};static constexpr float widths[]={0.5f,1.0f,1.5f,2.5f,4.0f,5.0f};in.route_width=widths[i%std::size(widths)];in.route_flags=std::uint8_t((i&1u?0x20u:0u)|(i&2u?0x10u:0u));in.stage_unique=(i%4u)==0u?0x1c:((i%4u)==1u?0x3a:0);in.rolling_reference=std::uint16_t((i*73u+0x280u)&0xffffu);in.protected_gate_value=(i%3u)?1u:0u;
            std::memcpy(reinterpret_cast<void*>(E),native.data(),native.size());set_r046_lane_inputs(in);prepare(0x47b890u);Bytes(reinterpret_cast<void*>(S),16).put32(0,E);run();road_lane_classify_47b890(ne,in);compare_blob("road_lane_classify",reinterpret_cast<void*>(E),native);
        }
        if(enabled("refresh_cached_road_4a3f80_r046")){
            std::array<std::uint8_t,0x1100> native{};Bytes ne(native.data(),native.size());for(std::size_t k=0;k<native.size();++k)native[k]=std::uint8_t((k*37u+i*19u)&0xffu);
            const std::int32_t hint=std::int32_t(i%12u),selector=(i%5u)==0u?-1:std::int32_t((i*3u)%12u);ne.puti(0x5c,int(i%3u));ne.put16(0x64,std::uint16_t(i*17u));ne.put32(0x68,0x12000000u^i);ne.puti(0x10d0,99);
            if(i%7u==0u && selector>=0){ne.put32(0x10c0,ne.u32(0x5c));ne.put16(0x10c8,std::uint16_t(ne.i16(0x64)));ne.put32(0x10cc,ne.u32(0x68));}
            PcRoadCacheRefreshInputs in{};in.selector_result=(i%7u==0u&&selector>=0)?hint:selector;in.query_success=(i%4u)!=0u;for(std::size_t k=0;k<in.query_output.size();++k)in.query_output[k]=std::uint8_t((i*11u+k*13u)^0x5au);
            std::memcpy(reinterpret_cast<void*>(E),native.data(),native.size());set_r046_cache_inputs(in);prepare(0x4a3f80u);Bytes st(reinterpret_cast<void*>(S),16);st.put32(0,E);st.puti(4,hint);run();const bool nr=refresh_cached_road_4a3f80(ne,hint,in);compare_u32("refresh_cached_road_return",guest_call.out_eax,nr?1u:0u);compare_blob("refresh_cached_road_memory",reinterpret_cast<void*>(E),native);
        }


        if(enabled("get_road_ofs_4a4010_r046")){
            std::array<std::uint8_t,0x1100> native{};Bytes ne(native.data(),native.size());
            // A simple rectangular road patch gives deterministic neutral,
            // positive and negative side domains as vehicle Z crosses its ends.
            const unsigned domain=i%3u;const CourseProbe pos{float(int(i%7u)-3)*0.25f,0.25f,float(domain==0?0:(domain==1?-15:15))};
            ne.putf(0x14,pos.x);ne.putf(0x18,pos.y);ne.putf(0x1c,pos.z);ne.puti(0x1c0,3);
            const std::int32_t type=(i&1u)?1:0;ne.puti(0x5c,type);ne.puti(0x60,(i&2u)?101:100);const std::uint16_t cs=std::uint16_t(0x60u+(i%8u));ne.put16(0x64,cs);ne.put8(0x66,std::uint8_t(std::int8_t(int(i%7u)-1)));ne.put32(0x68,i%9u);
            PcGetRoadOfsInputs in{};in.cache.selector_result=-1;in.cache.query_success=(i%19u)!=0u;
            Bytes qo(in.cache.query_output.data(),in.cache.query_output.size());qo.putf(8,0.0f);qo.putf(12,0.0f);qo.putf(16,0.0f);
            const CourseProbe corners[4]={{-5,0,10},{-5,0,-10},{5,0,10},{5,0,-10}};const unsigned oo[4]={0x24,0x30,0x3c,0x48};
            for(unsigned j=0;j<4;++j){qo.putf(oo[j],corners[j].x);qo.putf(oo[j]+4,corners[j].y);qo.putf(oo[j]+8,corners[j].z);}
            in.lane.route_width=1.0f+float(i%4u);in.lane.route_flags=std::uint8_t((i&4u?0x20u:0u)|(i&8u?0x10u:0u));in.lane.stage_unique=(i%4u)==0u?0x1c:0;in.lane.rolling_reference=0x300u;in.lane.protected_gate_value=(i%4u)==0u?1u:0u;in.use_lane_classifier=(i%5u)==0u;
            // Populate both PC road-table families for all selectors used by
            // GetRoadOfs side tests (0/3 and 2/5) at this course-position index.
            const std::size_t rec=4u+std::size_t(cs)*6u;
            auto fill_block=[&](Bytes b,int sel){b.put16(0,0x4f53u);b.put16(rec,std::uint16_t(std::int16_t(-8+sel*4)));b.put16(rec+2,std::uint16_t(std::int16_t(12-sel*3)));b.put16(rec+4,std::uint16_t((i*17u+unsigned(sel))&0x3fffu));};
            for(int sel:{0,2,3,5}){fill_block(Bytes(reinterpret_cast<void*>(0x804480u+std::uint32_t(sel)*0x1030u),0x1030u),sel);fill_block(Bytes(reinterpret_cast<void*>(0x80a670u+std::uint32_t(sel)*0x70cu),0x70cu),sel);}
            std::array<Bytes,6> prim{Bytes(reinterpret_cast<void*>(0x804480u+0u*0x1030u),0x1030),Bytes(reinterpret_cast<void*>(0x804480u+1u*0x1030u),0x1030),Bytes(reinterpret_cast<void*>(0x804480u+2u*0x1030u),0x1030),Bytes(reinterpret_cast<void*>(0x804480u+3u*0x1030u),0x1030),Bytes(reinterpret_cast<void*>(0x804480u+4u*0x1030u),0x1030),Bytes(reinterpret_cast<void*>(0x804480u+5u*0x1030u),0x1030)};
            std::array<Bytes,12> sec{Bytes(reinterpret_cast<void*>(0x80a670u+0u*0x70cu),0x70c),Bytes(reinterpret_cast<void*>(0x80a670u+1u*0x70cu),0x70c),Bytes(reinterpret_cast<void*>(0x80a670u+2u*0x70cu),0x70c),Bytes(reinterpret_cast<void*>(0x80a670u+3u*0x70cu),0x70c),Bytes(reinterpret_cast<void*>(0x80a670u+4u*0x70cu),0x70c),Bytes(reinterpret_cast<void*>(0x80a670u+5u*0x70cu),0x70c),Bytes(reinterpret_cast<void*>(0x80a670u+6u*0x70cu),0x70c),Bytes(reinterpret_cast<void*>(0x80a670u+7u*0x70cu),0x70c),Bytes(reinterpret_cast<void*>(0x80a670u+8u*0x70cu),0x70c),Bytes(reinterpret_cast<void*>(0x80a670u+9u*0x70cu),0x70c),Bytes(reinterpret_cast<void*>(0x80a670u+10u*0x70cu),0x70c),Bytes(reinterpret_cast<void*>(0x80a670u+11u*0x70cu),0x70c)};
            const PcRoadSampleTables tables{prim.data(),prim.size(),sec.data(),sec.size()};
            std::array<std::uint8_t,64> native_m{};Bytes nm(native_m.data(),native_m.size());for(unsigned k=0;k<16;++k)nm.putf(k*4u,k%5u==0u?1.0f:0.0f);const float tx=float(int(i%5u)-2)*0.5f,tz=float(int(i%7u)-3)*0.25f;nm.putf(0x30,tx);nm.putf(0x38,tz);
            std::memcpy(reinterpret_cast<void*>(0x7d2da0u),native_m.data(),native_m.size());std::memcpy(reinterpret_cast<void*>(E),native.data(),native.size());set_r046_cache_inputs(in.cache);set_r046_lane_inputs(in.lane);*reinterpret_cast<std::uint32_t*>(0x80fb14u)=in.use_lane_classifier?1u:0u;reset_matrix_oracle_globals();
            prepare(0x4a4010u);Bytes(reinterpret_cast<void*>(S),16).put32(0,E);run();get_road_ofs_4a4010(ne,in,nm,tables);compare_blob("get_road_ofs_memory",reinterpret_cast<void*>(E),native);
            compare_u32("get_road_ofs_matrix_pointer",*reinterpret_cast<std::uint32_t*>(0x89b564u),MatrixArena);compare_u32("get_road_ofs_matrix_depth",std::uint32_t(*reinterpret_cast<std::int32_t*>(0x89b568u)),0u);
        }


        if(enabled("common_pl_car_4a8100_r046")){
            Event initial_e{};Work initial_w{};Params initial_p{};
            std::mt19937 rng(0x4a810046u^(i*1664525u));
            for(auto& b:initial_e)b=std::uint8_t(rng());
            for(auto& b:initial_w)b=std::uint8_t(rng());
            for(auto& b:initial_p)b=std::uint8_t(rng());
            Bytes ie(initial_e.data(),initial_e.size()),iw(initial_w.data(),initial_w.size()),ip(initial_p.data(),initial_p.size());
            ie.put32(0x2b4,P);
            for(unsigned k=0;k<4;++k)iw.put32(0x248u+k*4u,W+std::uint32_t(embedded_wheel_offsets[k]));
            static constexpr float wheel_heights[]={-2.0f,-0.5f,0.0f,0.25f,1.0f,3.5f,10.0f};
            for(unsigned k=0;k<4;++k)iw.putf(embedded_wheel_offsets[k]+0x08u,wheel_heights[(i+k)%std::size(wheel_heights)]+float(k)*0.125f);
            ip.putf(0x558,float(int(i%13u)-6)*0.25f);ip.putf(0x5a4,float(int(i%11u)-5)*0.375f);
            ie.put8(0xd22,std::uint8_t(std::int8_t(int(i%7u)-3)));ie.put8(0xda4,std::uint8_t(std::int8_t(int((i/3u)%7u)-3)));ie.puti(0xd90,int(i%9u)-4);
            iw.put16(0x288,std::uint16_t(0x1100u+(i&0xffu)));iw.put16(0x37c,std::uint16_t(0x2200u+(i&0xffu)));iw.put16(0x470,std::uint16_t(0x3300u+(i&0xffu)));iw.put16(0x564,std::uint16_t(0x4400u+(i&0xffu)));
            PcCommonPlCarParentInputs in{};
            switch(i%4u){case 0:in.game_mode=0x0d;in.timer=0;break;case 1:in.game_mode=0x10;in.timer=61;break;case 2:in.game_mode=0x10;in.timer=60;break;default:in.game_mode=3;in.timer=-1;break;}
            switch((i/4u)%3u){case 0:in.route_state=7;in.session_mode4=false;break;case 1:in.route_state=2;in.session_mode4=true;break;default:in.route_state=2;in.session_mode4=false;break;}

            auto native_e=initial_e;auto native_w=initial_w;auto native_p=initial_p;
            std::memcpy(reinterpret_cast<void*>(E),initial_e.data(),initial_e.size());
            std::memcpy(reinterpret_cast<void*>(W),initial_w.data(),initial_w.size());
            std::memcpy(reinterpret_cast<void*>(P),initial_p.data(),initial_p.size());
            *reinterpret_cast<std::int32_t*>(0x78026cu)=in.game_mode;*reinterpret_cast<std::int32_t*>(0x780258u)=in.route_state;
            reset_r046_common_parent_state(in.timer,in.session_mode4);
            prepare(0x4a8100u);Bytes st(reinterpret_cast<void*>(S),16);st.put32(0,E);st.put32(4,W);run();
            const auto pc_trace=r046_common_parent_trace();

            struct NativeTrace{std::vector<std::uint32_t> v;};NativeTrace nt;
            auto cb=[](void* u,std::uint32_t entry){static_cast<NativeTrace*>(u)->v.push_back(entry);};
            Bytes ne(native_e.data(),native_e.size()),nw(native_w.data(),native_w.size()),np(native_p.data(),native_p.size());
            std::array<Bytes,4> wheels{nw.sub(embedded_wheel_offsets[0],0xf4),nw.sub(embedded_wheel_offsets[1],0xf4),nw.sub(embedded_wheel_offsets[2],0xf4),nw.sub(embedded_wheel_offsets[3],0xcc)};
            common_pl_car_4a8100(ne,nw,np,wheels,in,{&nt,cb});
            compare_blob("common_pl_car_event",reinterpret_cast<void*>(E),native_e);
            compare_blob("common_pl_car_work",reinterpret_cast<void*>(W),native_w);
            compare_u32("common_pl_car_trace_count",std::uint32_t(pc_trace.size()),std::uint32_t(nt.v.size()));
            const auto n=std::min(pc_trace.size(),nt.v.size());
            for(std::size_t k=0;k<n;++k)compare_u32("common_pl_car_trace_"+std::to_string(k),pc_trace[k],nt.v[k]);
        }

        if(only=="all"){set_r046_road_patches(false);set_r046_common_parent_patches(false);}


        const auto r048_saved_7d3884=*reinterpret_cast<std::uint32_t*>(0x7d3884u);
        const auto r048_saved_7d394c=*reinterpret_cast<std::uint32_t*>(0x7d394cu);
        const auto r048_saved_7d39f0=*reinterpret_cast<std::uint32_t*>(0x7d39f0u);
        const auto r048_saved_7f8abc=*reinterpret_cast<std::uint32_t*>(0x7f8abcu);
        const auto r048_saved_780258=*reinterpret_cast<std::uint32_t*>(0x780258u);
        const auto r048_saved_841fa4=*reinterpret_cast<std::uint32_t*>(0x841fa4u);
        const auto r048_saved_7162c4=*reinterpret_cast<std::uint32_t*>(0x7162c4u);
        const auto r048_saved_82e84c=*reinterpret_cast<std::uint32_t*>(0x82e84cu);
        const auto r048_saved_82e850=*reinterpret_cast<std::uint32_t*>(0x82e850u);
        const auto r048_saved_82e854=*reinterpret_cast<std::uint32_t*>(0x82e854u);
        const std::uint32_t r048_cap_addr[6]={0x82ea7au,0x82eb6eu,0x82ea78u,0x82eb6cu,0x82ec60u,0x82ed54u};
        std::uint16_t r048_saved_caps[6]{};for(unsigned k=0;k<6;++k)r048_saved_caps[k]=*reinterpret_cast<std::uint16_t*>(std::uintptr_t(r048_cap_addr[k]));

        if(enabled("race_counter_44fdf0_r048")){
            const std::uint32_t v=0x12340000u+i*97u;*reinterpret_cast<std::uint32_t*>(0x7d3884u)=v;prepare(0x44fdf0u);run();compare_u32("race_counter_44fdf0",guest_call.out_eax,race_counter_44fdf0(v));
        }
        if(enabled("set_timeup_counter_44fe30_r048")){
            std::uint32_t native=0xdeadbeefu,v=i*2654435761u;*reinterpret_cast<std::uint32_t*>(0x7d394cu)=native;prepare(0x44fe30u);Bytes(reinterpret_cast<void*>(S),16).put32(0,v);run();set_timeup_counter_44fe30(native,v);compare_u32("set_timeup_counter",*reinterpret_cast<std::uint32_t*>(0x7d394cu),native);
        }
        if(enabled("get_timeup_counter_44fe40_r048")){
            const std::uint32_t v=i*1664525u+1013904223u;*reinterpret_cast<std::uint32_t*>(0x7d394cu)=v;prepare(0x44fe40u);run();compare_u32("get_timeup_counter",guest_call.out_eax,get_timeup_counter_44fe40(v));
        }
        if(enabled("set_race_flag0_44fe50_r048")){
            std::uint32_t native=0xa5a50000u^(i*0x10203u);const bool on=(i&1u)!=0u;*reinterpret_cast<std::uint32_t*>(0x7d39f0u)=native;prepare(0x44fe50u);Bytes(reinterpret_cast<void*>(S),16).put32(0,on?1u:0u);run();set_race_flag0_44fe50(native,on);compare_u32("set_race_flag0",*reinterpret_cast<std::uint32_t*>(0x7d39f0u),native);
        }
        if(enabled("get_race_flag0_44fe70_r048")){
            const std::uint32_t v=(i*0x10001u)^0x5a5a5a5au;*reinterpret_cast<std::uint32_t*>(0x7d39f0u)=v;prepare(0x44fe70u);run();compare_u32("get_race_flag0",guest_call.out_eax,get_race_flag0_44fe70(v)?1u:0u);
        }
        if(enabled("set_race_flag2_44fef0_r048")){
            std::uint32_t native=0x12345670u^(i*0x01010101u);const bool on=(i&2u)!=0u;*reinterpret_cast<std::uint32_t*>(0x7d39f0u)=native;prepare(0x44fef0u);Bytes(reinterpret_cast<void*>(S),16).put32(0,on?1u:0u);run();set_race_flag2_44fef0(native,on);compare_u32("set_race_flag2",*reinterpret_cast<std::uint32_t*>(0x7d39f0u),native);
        }
        if(enabled("ham_nos_speed_45d0e0_r048")){
            float v=float(int(i%200u)-100)*0.125f;if(i%31u==0u)v=-0.0f;*reinterpret_cast<float*>(0x7f8abcu)=v;prepare(0x45d0e0u);guest_call.st0=1;run();compare_float("ham_nos_speed",ffrom(guest_call.out_st0),ham_nos_speed_45d0e0(v));
        }
        if(enabled("game_pl_car_ctrl_4a8330_r048")){
            std::array<std::uint8_t,0x1100> initial{};std::mt19937 rg(0x4a833048u^(i*1103515245u));for(auto& b:initial)b=std::uint8_t(rg());Bytes ie(initial.data(),initial.size());
            const bool capture_branch=(i%7u)==0u;ie.put32(4,capture_branch?(ie.u32(4)|0x00800000u):(ie.u32(4)&~0x00800000u));ie.put32(8,0x10203040u^(i*0x01020304u));ie.putf(0x1c4,float(int(i%100u)-50)*0.5f);ie.putf(0x178,float(int(i%70u)-35)*0.25f);ie.putf(0x1dc,float(int(i%20u)-10));ie.putf(0x2dc,float(int(i%30u)-15));ie.putf(0x20,float(i%11u));ie.putf(0x24,float(i%13u));ie.putf(0x28,float(i%17u));ie.put8(0xd23,std::uint8_t(i%4u));ie.puti(0x5c,(i&1u)?1:0);ie.put16(0x64,std::uint16_t(std::int16_t(int(i%400u)-200)));ie.put32(0x2f0,ie.u32(0x2f0)|1u);
            PcGamePlCarParentInputs in{};in.game_state_byte=std::uint8_t((i%3u)==0u?0u:1u);in.game_flag=std::uint8_t((i%5u)==0u?0u:1u);in.entry_mode=i%4u;in.route_state=(i%3u==0u)?2:((i%3u==1u)?4:1);in.rank=std::uint8_t(i%15u);in.heart_mode=(i%4u==0u)?0x12u:7u;in.network_tail_active=(i&1u)!=0u;in.course_end=std::uint16_t(100u+(i%100u));in.stage_denominator_base=10u+(i%20u);in.world_scale_divisor=1.0f+float(i%7u)*0.25f;for(unsigned k=0;k<6;++k)in.capture_words[k]=std::uint16_t((i*257u+k*0x1111u)^0x5a5au);
            auto native=initial;Bytes ne(native.data(),native.size());PcGamePlCarParentState ns{};ns.stage_denominator_base=in.stage_denominator_base;
            std::memcpy(reinterpret_cast<void*>(E),initial.data(),initial.size());*reinterpret_cast<std::int32_t*>(0x780258u)=in.route_state;*reinterpret_cast<std::uint32_t*>(0x841fa4u)=in.stage_denominator_base;*reinterpret_cast<float*>(0x7162c4u)=in.world_scale_divisor;reset_r048_parent_state(in);set_r048_parent_patches(true);
            prepare(0x4a8330u);Bytes(reinterpret_cast<void*>(S),16).put32(0,E);run();const auto pc_trace=r048_parent_trace();set_r048_parent_patches(false);
            struct NT{std::vector<std::uint32_t> v;} nt;auto cb=[](void* u,std::uint32_t pc){static_cast<NT*>(u)->v.push_back(pc);};game_pl_car_ctrl_4a8330(ne,in,ns,{&nt,cb});
            compare_blob("game_pl_car_event",reinterpret_cast<void*>(E),native);compare_u32("game_pl_car_stage_base",*reinterpret_cast<std::uint32_t*>(0x841fa4u),ns.stage_denominator_base);compare_float("game_pl_car_world_x",*reinterpret_cast<float*>(0x82e84cu),ns.world_position[0]);compare_float("game_pl_car_world_y",*reinterpret_cast<float*>(0x82e850u),ns.world_position[1]);compare_float("game_pl_car_world_z",*reinterpret_cast<float*>(0x82e854u),ns.world_position[2]);if(capture_branch){const std::uint32_t cap_addr[6]={0x82ea7au,0x82eb6eu,0x82ea78u,0x82eb6cu,0x82ec60u,0x82ed54u};for(unsigned k=0;k<6;++k)compare_u32("game_pl_car_capture_"+std::to_string(k),*reinterpret_cast<std::uint16_t*>(std::uintptr_t(cap_addr[k])),ns.capture_words[k]);}compare_u32("game_pl_car_trace_count",std::uint32_t(pc_trace.size()),std::uint32_t(nt.v.size()));const auto nn=std::min(pc_trace.size(),nt.v.size());for(std::size_t k=0;k<nn;++k)compare_u32("game_pl_car_trace_"+std::to_string(k),pc_trace[k],nt.v[k]);
        }
        if(only=="all"){
            set_r048_parent_patches(false);
            *reinterpret_cast<std::uint32_t*>(0x7d3884u)=r048_saved_7d3884;
            *reinterpret_cast<std::uint32_t*>(0x7d394cu)=r048_saved_7d394c;
            *reinterpret_cast<std::uint32_t*>(0x7d39f0u)=r048_saved_7d39f0;
            *reinterpret_cast<std::uint32_t*>(0x7f8abcu)=r048_saved_7f8abc;
            *reinterpret_cast<std::uint32_t*>(0x780258u)=r048_saved_780258;
            *reinterpret_cast<std::uint32_t*>(0x841fa4u)=r048_saved_841fa4;
            *reinterpret_cast<std::uint32_t*>(0x7162c4u)=r048_saved_7162c4;
            *reinterpret_cast<std::uint32_t*>(0x82e84cu)=r048_saved_82e84c;
            *reinterpret_cast<std::uint32_t*>(0x82e850u)=r048_saved_82e850;
            *reinterpret_cast<std::uint32_t*>(0x82e854u)=r048_saved_82e854;
            for(unsigned k=0;k<6;++k)*reinterpret_cast<std::uint16_t*>(std::uintptr_t(r048_cap_addr[k]))=r048_saved_caps[k];
        }

        // r052: event scheduler, callback setters and EventOpen boundary.
        if(enabled("event_control_43fab0_r052")){
            auto ns=r052_event_fixture(i);store_r052_state(ns);reset_r052_trace();R052NativeTrace nt{};
            prepare(0x43fab0u);run();event_control_43fab0(ns,PcEventServices{&nt,r052_native_invoke,nullptr});
            const auto flags=native_r052_flags(ns);compare_blob("event_control_flags",reinterpret_cast<void*>(R052FlagsBase),flags);
            compare_u32("event_control_current_slot",(*reinterpret_cast<std::uint32_t*>(R052CurrentPtr)-R052RecordBase)/0x3cu,ns.current_slot);
            const auto trace=r052_trace_blob(nt);compare_blob("event_control_trace",reinterpret_cast<void*>(R052StateBase),trace);
        }
        if(enabled("event_open_440180_r052")){
            auto ns=r052_event_fixture(i);const std::uint32_t id=(i*17u+7u)%PcEventSlotCount,fn=0x200u+(i%37u);auto& q=ns.slots[id];q.flags=(i%4u==0u)?0u:1u;q.init_callback=(i%5u==0u)?0u:r052_callback(i%R052CallbackCount);q.work_token=0x72000000u+i;q.event_id=id;
            store_r052_state(ns);reset_r052_trace();R052NativeTrace nt{};set_r052_event_open_patch(true);
            prepare(0x440180u);Bytes st(reinterpret_cast<void*>(S),16);st.put32(0,id);st.put32(4,fn);run();set_r052_event_open_patch(false);
            event_open_440180(ns,id,fn,PcEventServices{&nt,r052_native_invoke,r052_native_setup});
            const auto flags=native_r052_flags(ns);compare_blob("event_open_flags",reinterpret_cast<void*>(R052FlagsBase),flags);const auto trace=r052_trace_blob(nt);compare_blob("event_open_trace",reinterpret_cast<void*>(R052StateBase),trace);
        }
        if(enabled("event_close_4401d0_r052")){
            auto ns=r052_event_fixture(i);const std::uint32_t id=(i*29u+3u)%PcEventSlotCount;ns.slots[id].flags=std::uint8_t((i%4u)==0u?1u:((i%4u)==1u?2u:((i%4u)==2u?3u:0x12u)));store_r052_state(ns);
            prepare(0x4401d0u);Bytes(reinterpret_cast<void*>(S),16).put32(0,id);run();event_close_4401d0(ns,id);compare_u32("event_close_flag",*reinterpret_cast<std::uint8_t*>(R052FlagsBase+id),ns.slots[id].flags);
        }
        if(enabled("event_close_immediate_440200_r052")){
            auto ns=r052_event_fixture(i);const std::uint32_t id=(i*31u+5u)%PcEventSlotCount;auto& q=ns.slots[id];q.flags=std::uint8_t((i%3u)==0u?1u:((i%3u)==1u?2u:0x12u));q.dest_callback=(i%4u==0u)?0u:r052_callback(5);q.work_token=0x73000000u+i;q.event_id=id;store_r052_state(ns);reset_r052_trace();R052NativeTrace nt{};
            prepare(0x440200u);Bytes(reinterpret_cast<void*>(S),16).put32(0,id);run();event_close_immediate_440200(ns,id,PcEventServices{&nt,r052_native_invoke,nullptr});compare_u32("event_close_immediate_flag",*reinterpret_cast<std::uint8_t*>(R052FlagsBase+id),ns.slots[id].flags);const auto trace=r052_trace_blob(nt);compare_blob("event_close_immediate_trace",reinterpret_cast<void*>(R052StateBase),trace);
        }
        if(enabled("event_close_all_440240_r052")){
            auto ns=r052_event_fixture(i);store_r052_state(ns);prepare(0x440240u);run();event_close_all_440240(ns);const auto flags=native_r052_flags(ns);compare_blob("event_close_all_flags",reinterpret_cast<void*>(R052FlagsBase),flags);
        }
        if(enabled("event_close_serial_440330_r052")){
            auto ns=r052_event_fixture(i);const std::uint32_t first=(i*7u)%390u,count=i%21u;store_r052_state(ns);prepare(0x440330u);Bytes st(reinterpret_cast<void*>(S),16);st.put32(0,first);st.put32(4,count);run();event_close_serial_440330(ns,first,count);const auto flags=native_r052_flags(ns);compare_blob("event_close_serial_flags",reinterpret_cast<void*>(R052FlagsBase),flags);
        }
        if(enabled("check_event_destructing_440370_r052")){
            auto ns=r052_event_fixture(i);const std::uint32_t id=(i*11u+9u)%PcEventSlotCount;ns.slots[id].flags=std::uint8_t(i);store_r052_state(ns);prepare(0x440370u);Bytes(reinterpret_cast<void*>(S),16).put32(0,id);run();compare_u32("check_event_destructing",guest_call.out_eax?1u:0u,check_event_destructing_440370(ns,id)?1u:0u);
        }
        if(enabled("get_event_id_440b30_r052")){
            const auto defaults=r052_default_tokens();const bool missing=(i%5u)==0u;std::uint32_t token=missing?(0xf1000000u+i):defaults[(i*37u+13u)%PcEventSlotCount];if(missing){while(std::find(defaults.begin(),defaults.end(),token)!=defaults.end())++token;}
            prepare(0x440b30u);Bytes(reinterpret_cast<void*>(S),16).put32(0,token);run();compare_u32("get_event_id",guest_call.out_eax,get_event_id_440b30(defaults,token));
        }
        if(enabled("get_now_event_id_440b80_r052")){
            auto ns=r052_event_fixture(i);const std::uint32_t id=(i*41u+2u)%PcEventSlotCount;ns.current_slot=id;store_r052_state(ns);prepare(0x440b80u);run();compare_u32("get_now_event_id",guest_call.out_eax,get_now_event_id_440b80(ns));
        }
        if(enabled("change_now_event_ctrl_func_440b90_r052")){
            auto ns=r052_event_fixture(i);const std::uint32_t id=(i*43u+1u)%PcEventSlotCount,token=0x74100000u+i;ns.current_slot=id;store_r052_state(ns);prepare(0x440b90u);Bytes(reinterpret_cast<void*>(S),16).put32(0,token);run();change_now_event_ctrl_func_440b90(ns,token);compare_u32("change_now_event_ctrl_func",*reinterpret_cast<std::uint32_t*>(R052RecordBase+id*0x3cu+0x14u),ns.slots[id].ctrl_callback);
        }
        if(enabled("change_now_event_shadow_func_440ba0_r052")){
            auto ns=r052_event_fixture(i);const std::uint32_t id=(i*47u+4u)%PcEventSlotCount,token=0x74200000u+i;ns.current_slot=id;store_r052_state(ns);prepare(0x440ba0u);Bytes(reinterpret_cast<void*>(S),16).put32(0,token);run();change_now_event_shadow_func_440ba0(ns,token);compare_u32("change_now_event_shadow_func",*reinterpret_cast<std::uint32_t*>(R052RecordBase+id*0x3cu+0x1cu),ns.slots[id].shadow_callback);
        }
        if(enabled("change_ctrl_func_440bb0_r052")){
            auto ns=r052_event_fixture(i);const std::uint32_t id=(i*53u+8u)%PcEventSlotCount,token=0x74300000u+i;store_r052_state(ns);prepare(0x440bb0u);Bytes st(reinterpret_cast<void*>(S),16);st.put32(0,id);st.put32(4,token);run();change_ctrl_func_440bb0(ns,id,token);compare_u32("change_ctrl_func",*reinterpret_cast<std::uint32_t*>(R052RecordBase+id*0x3cu+0x14u),ns.slots[id].ctrl_callback);
        }
        if(enabled("change_disp_scene_440bd0_r052")){
            auto ns=r052_event_fixture(i);const std::uint32_t id=(i*59u+6u)%PcEventSlotCount,scene=0x74400000u+i;store_r052_state(ns);prepare(0x440bd0u);Bytes st(reinterpret_cast<void*>(S),16);st.put32(0,id);st.put32(4,scene);run();change_disp_scene_440bd0(ns,id,scene);compare_u32("change_disp_scene",*reinterpret_cast<std::uint32_t*>(R052RecordBase+id*0x3cu+0x0cu),ns.slots[id].display_scene);
        }

        // r053: bootstrap and suspend/resume leaves around the event scheduler.
        if(enabled("init_event_control_440bf0_r053")){
            static const auto desc=r053_init_descriptors();
            static const auto funcs=r053_function_descriptors();
            auto ns=r052_event_fixture(i);
            // Poison all record fields/flags first: InitEventControl owns the full
            // 0x3c record reset and must not accidentally depend on prior state.
            for(std::uint32_t id=0;id<PcEventSlotCount;++id){
                auto& q=ns.slots[id];
                q.flags=std::uint8_t((id*37u+i*13u+0x5au)&0xffu);
                q.descriptor_token=0xa1000000u^(id*0x10201u+i);q.event_id=0xa2000000u^id;
                q.work_token=0xa3000000u^(id+i);q.display_scene=0xa4000000u^i;
                q.init_callback=0xa5000000u^id;q.ctrl_callback=0xa6000000u^id;q.disp_callback=0xa7000000u^id;
                q.shadow_callback=0xa8000000u^id;q.dest_callback=0xa9000000u^id;q.aux24=0xaa000000u^id;
                q.aux28=0xab000000u^id;q.close_guard=0xac000000u^id;q.function_id=0xad000000u^id;
                q.aux34=0xae000000u^id;q.aux38=0xaf000000u^id;
            }
            ns.current_slot=(i*61u+17u)%PcEventSlotCount;store_r052_state(ns);
            const auto current_before=*reinterpret_cast<std::uint32_t*>(R052CurrentPtr);
            prepare(0x440bf0u);run();
            init_event_control_440bf0(ns,desc,funcs);
            const auto records=native_r052_records(ns);const auto flags=native_r052_flags(ns);
            compare_blob("init_event_control_records",reinterpret_cast<void*>(R052RecordBase),records);
            compare_blob("init_event_control_flags",reinterpret_cast<void*>(R052FlagsBase),flags);
            compare_u32("init_event_control_current",*reinterpret_cast<std::uint32_t*>(R052CurrentPtr),current_before);
        }
        if(enabled("event_suspend_440a10_r053")){
            auto ns=r052_event_fixture(i);const std::uint32_t first=(i*17u)%400u,count=i%11u;store_r052_state(ns);
            prepare(0x440a10u);Bytes st(reinterpret_cast<void*>(S),16);st.put32(0,first);st.put32(4,count);run();
            event_suspend_440a10(ns,first,count);const auto flags=native_r052_flags(ns);
            compare_blob("event_suspend_flags",reinterpret_cast<void*>(R052FlagsBase),flags);
        }
        if(enabled("event_resume_440a30_r053")){
            auto ns=r052_event_fixture(i);const std::uint32_t first=(i*19u)%400u,count=i%11u;
            for(std::uint32_t k=first;k<first+count;++k)ns.slots[k].flags=std::uint8_t(ns.slots[k].flags|0x10u);
            store_r052_state(ns);prepare(0x440a30u);Bytes st(reinterpret_cast<void*>(S),16);st.put32(0,first);st.put32(4,count);run();
            event_resume_440a30(ns,first,count);const auto flags=native_r052_flags(ns);
            compare_blob("event_resume_flags",reinterpret_cast<void*>(R052FlagsBase),flags);
        }
        if(enabled("check_event_suspend_440a50_r053")){
            auto ns=r052_event_fixture(i);const std::uint32_t id=(i*53u+11u)%PcEventSlotCount;
            if(i&1u)ns.slots[id].flags=std::uint8_t(ns.slots[id].flags|0x10u);else ns.slots[id].flags=std::uint8_t(ns.slots[id].flags&~0x10u);
            store_r052_state(ns);prepare(0x440a50u);Bytes(reinterpret_cast<void*>(S),16).put32(0,id);run();
            compare_u32("check_event_suspend",guest_call.out_eax,check_event_suspend_440a50(ns,id));
        }

        // r054: pause nesting and current-event work allocation ownership.
        if(enabled("set_ev_pause_flag_440930_r054")){
            auto ns=r052_event_fixture(i);
            ns.pause_depth=i%4u;
            for(std::uint32_t id=0;id<PcEventSlotCount;++id){
                auto& q=ns.slots[id];
                q.work_token=((id+i)%7u)==0u?0u:(0x52000000u^(id*0x101u)^(i*0x10001u));
                q.flags=std::uint8_t(q.flags&0xf7u);
            }
            store_r052_state(ns);*reinterpret_cast<std::uint32_t*>(0x7a0db0u)=ns.pause_depth;reset_r054_trace();
            prepare(0x440930u);run();
            R054NativeTrace nt{};PcEventPauseServices ps{&nt,r054_native_boundary};set_ev_pause_flag_440930(ns,ps);
            const auto flags=native_r052_flags(ns);compare_blob("set_ev_pause_flags",reinterpret_cast<void*>(R052FlagsBase),flags);
            compare_u32("set_ev_pause_depth",*reinterpret_cast<std::uint32_t*>(0x7a0db0u),ns.pause_depth);
            compare_blob("set_ev_pause_trace",reinterpret_cast<void*>(R054StateBase),r054_trace_blob(nt));
        }
        if(enabled("clr_ev_pause_flag_4409c0_r054")){
            auto ns=r052_event_fixture(i);ns.pause_depth=i%4u;
            for(auto& q:ns.slots){q.work_token=q.work_token?q.work_token:0x53000001u;q.flags=std::uint8_t(q.flags|0x08u);}
            store_r052_state(ns);*reinterpret_cast<std::uint32_t*>(0x7a0db0u)=ns.pause_depth;reset_r054_trace();
            prepare(0x4409c0u);run();
            R054NativeTrace nt{};PcEventPauseServices ps{&nt,r054_native_boundary};clr_ev_pause_flag_4409c0(ns,ps);
            const auto flags=native_r052_flags(ns);compare_blob("clr_ev_pause_flags",reinterpret_cast<void*>(R052FlagsBase),flags);
            compare_u32("clr_ev_pause_depth",*reinterpret_cast<std::uint32_t*>(0x7a0db0u),ns.pause_depth);
            compare_blob("clr_ev_pause_trace",reinterpret_cast<void*>(R054StateBase),r054_trace_blob(nt));
        }
        if(enabled("malloc_now_event_work_440a60_r054")){
            auto ns=r052_event_fixture(i);const std::uint32_t id=(i*67u+5u)%PcEventSlotCount;ns.current_slot=id;
            ns.slots[id].descriptor_token=0x59000000u^(i*0x101u);ns.slots[id].work_token=0u;ns.slots[id].aux24=0u;
            const std::uint32_t requested=4u+((i*29u)%0x3f0u),type=i%11u,base_addr=R054AllocBase+(i%16u)*0x1000u;
            std::memset(reinterpret_cast<void*>(base_addr),0xa5,0x800u);store_r052_state(ns);reset_r054_trace(base_addr);
            prepare(0x440a60u);Bytes st(reinterpret_cast<void*>(S),16);st.put32(0,requested);st.put32(4,type);run();
            R054NativeTrace nt{};nt.alloc_return=base_addr;PcEventWorkServices ws{&nt,r054_native_boundary,r054_native_alloc,r054_native_release};PcEventWorkHandle h{};
            const auto native_ret=malloc_now_event_work_440a60(ns,h,requested,type,ws);
            compare_u32("malloc_now_event_work_return",guest_call.out_eax,native_ret);
            const auto records=native_r052_records(ns);compare_blob("malloc_now_event_work_records",reinterpret_cast<void*>(R052RecordBase),records);
            compare_u32("malloc_now_event_work_trailer_base",*reinterpret_cast<std::uint32_t*>(base_addr+requested),h.base_token);
            compare_u32("malloc_now_event_work_trailer_type",*reinterpret_cast<std::uint32_t*>(base_addr+requested+4u),h.heap_type);
            compare_blob("malloc_now_event_work_trace",reinterpret_cast<void*>(R054StateBase),r054_trace_blob(nt));
        }
        if(enabled("free_event_work_handle_440cd0_r054")){
            auto ns=r052_event_fixture(i);const std::uint32_t id=(i*71u+7u)%PcEventSlotCount;ns.current_slot=id;
            const std::uint32_t base_addr=R054AllocBase+(i%16u)*0x1000u,requested=0x80u+((i*13u)%0x180u),type=i%11u,wrapper=base_addr+requested;
            ns.slots[id].work_token=base_addr;ns.slots[id].aux24=wrapper;*reinterpret_cast<std::uint32_t*>(wrapper)=base_addr;*reinterpret_cast<std::uint32_t*>(wrapper+4u)=type;
            store_r052_state(ns);reset_r054_trace();PcEventWorkHandle h=r054_handle_from_guest(wrapper);
            prepare(0x440cd0u);Bytes(reinterpret_cast<void*>(S),16).put32(0,R052RecordBase+id*0x3cu+0x24u);run();
            R054NativeTrace nt{};PcEventWorkServices ws{&nt,r054_native_boundary,r054_native_alloc,r054_native_release};free_event_work_handle_440cd0(ns,h,ws);
            const auto records=native_r052_records(ns);compare_blob("free_event_work_handle_records",reinterpret_cast<void*>(R052RecordBase),records);
            compare_blob("free_event_work_handle_trace",reinterpret_cast<void*>(R054StateBase),r054_trace_blob(nt));
        }
        if(enabled("free_now_event_work_440b20_r054")){
            auto ns=r052_event_fixture(i);const std::uint32_t id=(i*73u+9u)%PcEventSlotCount;ns.current_slot=id;
            const std::uint32_t base_addr=R054AllocBase+(i%16u)*0x1000u,requested=0x90u+((i*17u)%0x170u),type=i%11u,wrapper=base_addr+requested;
            ns.slots[id].work_token=base_addr;ns.slots[id].aux24=wrapper;*reinterpret_cast<std::uint32_t*>(wrapper)=base_addr;*reinterpret_cast<std::uint32_t*>(wrapper+4u)=type;
            store_r052_state(ns);reset_r054_trace();PcEventWorkHandle h=r054_handle_from_guest(wrapper);
            prepare(0x440b20u);run();
            R054NativeTrace nt{};PcEventWorkServices ws{&nt,r054_native_boundary,r054_native_alloc,r054_native_release};free_now_event_work_440b20(ns,h,ws);
            const auto records=native_r052_records(ns);compare_blob("free_now_event_work_records",reinterpret_cast<void*>(R052RecordBase),records);
            compare_blob("free_now_event_work_trace",reinterpret_cast<void*>(R054StateBase),r054_trace_blob(nt));
        }
        // r055: allocator state-stack leaves and compact ownership/service wrappers.
        if(enabled("push_alloc_state_a_440d10_r055")){
            PcAllocatorStateStacks ns{};for(unsigned k=0;k<5;++k){ns.stack_a[k]=0xa1000000u^(i*0x101u+k);ns.stack_b[k]=0xb1000000u^(i*0x103u+k);}
            ns.depth_a=i%6u;ns.depth_b=(i*3u)%5u;store_r055_allocator_state(ns);const std::uint32_t value=0x51000000u^(i*0x10001u);
            prepare(0x440d10u);Bytes(reinterpret_cast<void*>(S),16).put32(0,value);run();push_alloc_state_a_440d10(ns,value);
            const auto blob=r055_allocator_blob();std::array<std::uint8_t,0x30> native{};
            std::memcpy(native.data()+0x14,ns.stack_a.data(),20);std::memcpy(native.data()+0x00,ns.stack_b.data(),20);
            std::memcpy(native.data()+0x28,&ns.depth_a,4);std::memcpy(native.data()+0x2c,&ns.depth_b,4);compare_blob("push_alloc_state_a",blob.data(),native);
        }
        if(enabled("pop_alloc_state_a_440d30_r055")){
            PcAllocatorStateStacks ns{};for(unsigned k=0;k<5;++k){ns.stack_a[k]=0xa2000000u^(i*0x101u+k);ns.stack_b[k]=0xb2000000u^(i*0x103u+k);}
            ns.depth_a=1u+(i%4u);ns.depth_b=(i*5u)%5u;store_r055_allocator_state(ns);prepare(0x440d30u);run();const auto nv=pop_alloc_state_a_440d30(ns);
            compare_u32("pop_alloc_state_a_return",guest_call.out_eax,nv);const auto blob=r055_allocator_blob();std::array<std::uint8_t,0x30> native{};
            std::memcpy(native.data()+0x14,ns.stack_a.data(),20);std::memcpy(native.data()+0x00,ns.stack_b.data(),20);std::memcpy(native.data()+0x28,&ns.depth_a,4);std::memcpy(native.data()+0x2c,&ns.depth_b,4);compare_blob("pop_alloc_state_a",blob.data(),native);
        }
        if(enabled("push_alloc_state_b_440d50_r055")){
            PcAllocatorStateStacks ns{};for(unsigned k=0;k<5;++k){ns.stack_a[k]=0xa3000000u^(i*0x101u+k);ns.stack_b[k]=0xb3000000u^(i*0x103u+k);}
            ns.depth_a=(i*7u)%5u;ns.depth_b=i%6u;store_r055_allocator_state(ns);const std::uint32_t value=0x52000000u^(i*0x10003u);prepare(0x440d50u);Bytes(reinterpret_cast<void*>(S),16).put32(0,value);run();push_alloc_state_b_440d50(ns,value);
            const auto blob=r055_allocator_blob();std::array<std::uint8_t,0x30> native{};std::memcpy(native.data()+0x14,ns.stack_a.data(),20);std::memcpy(native.data()+0x00,ns.stack_b.data(),20);std::memcpy(native.data()+0x28,&ns.depth_a,4);std::memcpy(native.data()+0x2c,&ns.depth_b,4);compare_blob("push_alloc_state_b",blob.data(),native);
        }
        if(enabled("pop_alloc_state_b_440d70_r055")){
            PcAllocatorStateStacks ns{};for(unsigned k=0;k<5;++k){ns.stack_a[k]=0xa4000000u^(i*0x101u+k);ns.stack_b[k]=0xb4000000u^(i*0x103u+k);}
            ns.depth_a=(i*11u)%5u;ns.depth_b=1u+(i%4u);store_r055_allocator_state(ns);prepare(0x440d70u);run();const auto nv=pop_alloc_state_b_440d70(ns);compare_u32("pop_alloc_state_b_return",guest_call.out_eax,nv);
            const auto blob=r055_allocator_blob();std::array<std::uint8_t,0x30> native{};std::memcpy(native.data()+0x14,ns.stack_a.data(),20);std::memcpy(native.data()+0x00,ns.stack_b.data(),20);std::memcpy(native.data()+0x28,&ns.depth_a,4);std::memcpy(native.data()+0x2c,&ns.depth_b,4);compare_blob("pop_alloc_state_b",blob.data(),native);
        }
        if(enabled("hmm_handle_move_440cc0_r055")){
            auto mem=Bytes(reinterpret_cast<void*>(S+0x100u),32);const std::uint32_t src=S+0x108u,dst=S+0x10cu,value=0x61000000u^(i*0x10201u);mem.put32(8,value);mem.put32(12,0xdeadbeefu);prepare(0x440cc0u);auto st=Bytes(reinterpret_cast<void*>(S),16);st.put32(0,dst);st.put32(4,src);run();std::uint32_t native_dst=0xdeadbeefu;hmm_handle_move_440cc0(native_dst,value);compare_u32("hmm_handle_move_dest",*reinterpret_cast<std::uint32_t*>(dst),native_dst);
        }
        if(enabled("masked_service_440ca0_r055")){
            *reinterpret_cast<std::uint32_t*>(R055StateBase+0x00u)=0u;*reinterpret_cast<std::uint32_t*>(R055StateBase+0x04u)=0u;*reinterpret_cast<std::uint32_t*>(R055StateBase+0x08u)=0u;*reinterpret_cast<std::uint32_t*>(R055StateBase+0x0cu)=0u;
            const std::uint32_t bit=i*7u+3u;prepare(0x440ca0u);Bytes(reinterpret_cast<void*>(S),16).put32(0,bit);run();R055MaskTrace nt{};masked_service_440ca0(bit,&nt,r055_mask_cb);const auto nb=r055_mask_blob(nt);compare_blob("masked_service_trace",reinterpret_cast<void*>(R055StateBase),nb);
        }
        // r056: compact object-state leaves and wrappers.
        // In the aggregate oracle the wrappers must use deterministic child stubs only
        // while this sub-batch runs; restore the original tail targets immediately after.
        if(only=="all") set_r056_object_patches(true);
        if(enabled("object_take_token_440dc0_r056")){
            auto native=r056_object_fixture(i);Bytes nb(native.data(),native.size());const std::uint32_t value=0x56000000u^(i*0x10101u);nb.put32(0x21c,value);std::memcpy(reinterpret_cast<void*>(R056ObjectBase),native.data(),native.size());prepare(0x440dc0u);guest_call.ecx=R056ObjectBase;run();const auto nr=object_take_token_440dc0(nb);compare_u32("object_take_token_return_440dc0_r056",guest_call.out_eax,nr);compare_blob("object_take_token_440dc0_r056",reinterpret_cast<void*>(R056ObjectBase),native);
        }
        if(enabled("object_set_token_440de0_r056")){
            auto native=r056_object_fixture(i);Bytes nb(native.data(),native.size());std::memcpy(reinterpret_cast<void*>(R056ObjectBase),native.data(),native.size());const std::uint32_t v=0x56100000u^(i*0x10003u);prepare(0x440de0u);guest_call.ecx=R056ObjectBase;Bytes(reinterpret_cast<void*>(S),16).put32(0,v);run_original32();if(guest_call.out_sp!=S+4u)throw std::runtime_error("r056 440de0 stack imbalance");object_set_token_440de0(nb,v);compare_blob("object_set_token_440de0_r056",reinterpret_cast<void*>(R056ObjectBase),native);
        }
        if(enabled("object_set_field4_440df0_r056")){
            auto native=r056_object_fixture(i);Bytes nb(native.data(),native.size());std::memcpy(reinterpret_cast<void*>(R056ObjectBase),native.data(),native.size());const std::uint32_t v=0x56200000u^(i*0x10007u);prepare(0x440df0u);guest_call.ecx=R056ObjectBase;Bytes(reinterpret_cast<void*>(S),16).put32(0,v);run_original32();if(guest_call.out_sp!=S+4u)throw std::runtime_error("r056 440df0 stack imbalance");object_set_field4_440df0(nb,v);compare_blob("object_set_field4_440df0_r056",reinterpret_cast<void*>(R056ObjectBase),native);
        }
        if(enabled("object_set_field8_440e00_r056")){
            auto native=r056_object_fixture(i);Bytes nb(native.data(),native.size());std::memcpy(reinterpret_cast<void*>(R056ObjectBase),native.data(),native.size());const std::uint32_t v=0x56300000u^(i*0x10009u);prepare(0x440e00u);guest_call.ecx=R056ObjectBase;Bytes(reinterpret_cast<void*>(S),16).put32(0,v);run_original32();if(guest_call.out_sp!=S+4u)throw std::runtime_error("r056 440e00 stack imbalance");object_set_field8_440e00(nb,v);compare_blob("object_set_field8_440e00_r056",reinterpret_cast<void*>(R056ObjectBase),native);
        }
        if(enabled("object_push_byte_state_440e10_r056")){
            auto native=r056_object_fixture(i);Bytes nb(native.data(),native.size());const std::uint8_t depths[]={0u,1u,15u,31u,32u,33u,0xffu};nb.put8(0x220,depths[i%7u]);std::memcpy(reinterpret_cast<void*>(R056ObjectBase),native.data(),native.size());const std::uint32_t ret=0x40u+(i&0x3fu);reset_r056_trace(ret);R056NativeTrace nt{};nt.ret=ret;PcObjectChildServices sv{&nt,r056_native_void,r056_native_bool2};prepare(0x440e10u);guest_call.ecx=R056ObjectBase;run();object_push_byte_state_440e10(nb,sv);compare_blob("object_push_byte_state_440e10_r056",reinterpret_cast<void*>(R056ObjectBase),native);auto gt=r056_guest_trace_blob(),ntb=r056_native_trace_blob(nt);compare_blob("object_push_byte_state_trace_440e10_r056",gt.data(),ntb);
        }
        if(enabled("object_pop_byte_state_440e60_r056")){
            auto native=r056_object_fixture(i);Bytes nb(native.data(),native.size());const std::uint8_t depths[]={0u,1u,2u,31u,32u,0xffu};nb.put8(0x220,depths[i%6u]);std::memcpy(reinterpret_cast<void*>(R056ObjectBase),native.data(),native.size());const std::uint32_t ret=0x50u+(i&0x3fu);reset_r056_trace(ret);R056NativeTrace nt{};nt.ret=ret;PcObjectChildServices sv{&nt,r056_native_void,r056_native_bool2};prepare(0x440e60u);guest_call.ecx=R056ObjectBase;run();object_pop_byte_state_440e60(nb,sv);compare_blob("object_pop_byte_state_440e60_r056",reinterpret_cast<void*>(R056ObjectBase),native);auto gt=r056_guest_trace_blob(),ntb=r056_native_trace_blob(nt);compare_blob("object_pop_byte_state_trace_440e60_r056",gt.data(),ntb);
        }
        if(enabled("object_child_call_a_440ea0_r056")||enabled("object_child_call_b_440eb0_r056")||enabled("object_child_call_c_440ec0_r056")){
            auto runwrap=[&](const char* label,std::uint32_t entry,auto nativefn,std::uint32_t retseed){auto native=r056_object_fixture(i);Bytes nb(native.data(),native.size());std::memcpy(reinterpret_cast<void*>(R056ObjectBase),native.data(),native.size());reset_r056_trace(retseed);R056NativeTrace nt{};nt.ret=retseed;PcObjectChildServices sv{&nt,r056_native_void,r056_native_bool2};prepare(entry);guest_call.ecx=R056ObjectBase;run();nativefn(nb,sv);compare_blob(label,reinterpret_cast<void*>(R056ObjectBase),native);auto gt=r056_guest_trace_blob(),ntb=r056_native_trace_blob(nt);compare_blob(std::string(label)+"_trace",gt.data(),ntb);};
            if(enabled("object_child_call_a_440ea0_r056"))runwrap("object_child_call_a_440ea0_r056",0x440ea0u,object_child_call_a_440ea0,0x61u);
            if(enabled("object_child_call_b_440eb0_r056"))runwrap("object_child_call_b_440eb0_r056",0x440eb0u,object_child_call_b_440eb0,0x62u);
            if(enabled("object_child_call_c_440ec0_r056"))runwrap("object_child_call_c_440ec0_r056",0x440ec0u,object_child_call_c_440ec0,0x63u);
        }
        if(enabled("object_child_conditional_440ed0_r056")){
            auto native=r056_object_fixture(i);Bytes nb(native.data(),native.size());const std::uint32_t mode=(i%3u)==0u?2u:(i%4u);nb.put32(0x218,mode);std::memcpy(reinterpret_cast<void*>(R056ObjectBase),native.data(),native.size());const std::uint32_t ret=0x70u+(i&0x7fu),a=0x11110000u^i,b=0x22220000u^(i*3u);reset_r056_trace(ret);R056NativeTrace nt{};nt.ret=ret;PcObjectChildServices sv{&nt,r056_native_void,r056_native_bool2};prepare(0x440ed0u);guest_call.ecx=R056ObjectBase;Bytes(reinterpret_cast<void*>(S),16).put32(0,a);Bytes(reinterpret_cast<void*>(S),16).put32(4,b);run_original32();if(guest_call.out_sp!=S+8u)throw std::runtime_error("r056 440ed0 stack imbalance");const auto nr=object_child_conditional_440ed0(nb,a,b,sv);compare_u32("object_child_conditional_return_440ed0_r056",guest_call.out_eax&0xffu,nr);compare_blob("object_child_conditional_440ed0_r056",reinterpret_cast<void*>(R056ObjectBase),native);auto gt=r056_guest_trace_blob(),ntb=r056_native_trace_blob(nt);compare_blob("object_child_conditional_trace_440ed0_r056",gt.data(),ntb);
        }
        if(only=="all") set_r056_object_patches(false);
        if(only=="all"){
            std::memcpy(reinterpret_cast<void*>(R052RecordBase),r052_saved_records.data(),r052_saved_records.size());
            std::memcpy(reinterpret_cast<void*>(R052FlagsBase),r052_saved_flags.data(),r052_saved_flags.size());
            *reinterpret_cast<std::uint32_t*>(R052CurrentPtr)=r052_saved_current;
            *reinterpret_cast<std::uint32_t*>(0x7a0db0u)=r052_saved_pause_depth;
            std::memcpy(reinterpret_cast<void*>(R055StackB),r055_saved_allocator.data(),r055_saved_allocator.size());
        }

        // r057: float transition controller + five simple global-state leaves.
        if(only=="all")set_r057_transition_patch(true);
        auto seed57=[&](){PcFloatTransitionGlobals g{};g.token=0x57000000u^(i*0x10001u);g.primary=float(int(i%13u)-6)*0.25f;g.secondary=float(int(i%9u)-4)*0.125f;g.active=std::uint8_t(i&1u);return g;};
        if(enabled("transition_global_get_active_4c50c0_r057")){
            auto ng=seed57();r057_store_globals(ng);prepare(0x4c50c0u);guest_call.eax=0xa5a50000u;run();compare_u32("transition_global_get_active_4c50c0_r057",guest_call.out_eax&0xffu,transition_global_get_active_4c50c0(ng));
        }
        if(enabled("transition_global_set_token_4c50e0_r057")){
            auto ng=seed57();r057_store_globals(ng);const std::uint32_t v=0x57e00000u^(i*0x10103u);prepare(0x4c50e0u);Bytes(reinterpret_cast<void*>(S),16).put32(0,v);run();transition_global_set_token_4c50e0(ng,v);auto gb=r057_guest_globals_blob(),nb=r057_globals_blob(ng);compare_blob("transition_global_set_token_4c50e0_r057",gb.data(),nb);
        }
        if(enabled("transition_global_set_primary_4c50f0_r057")){
            auto ng=seed57();r057_store_globals(ng);const float v=float(int(i%31u)-15)*0.125f;std::uint32_t bits{};std::memcpy(&bits,&v,4);prepare(0x4c50f0u);Bytes(reinterpret_cast<void*>(S),16).put32(0,bits);run();transition_global_set_primary_4c50f0(ng,v);auto gb=r057_guest_globals_blob(),nb=r057_globals_blob(ng);compare_blob("transition_global_set_primary_4c50f0_r057",gb.data(),nb);
        }
        if(enabled("transition_global_set_secondary_4c5100_r057")){
            auto ng=seed57();r057_store_globals(ng);const float v=float(int(i%29u)-14)*0.0625f;std::uint32_t bits{};std::memcpy(&bits,&v,4);prepare(0x4c5100u);Bytes(reinterpret_cast<void*>(S),16).put32(0,bits);run();transition_global_set_secondary_4c5100(ng,v);auto gb=r057_guest_globals_blob(),nb=r057_globals_blob(ng);compare_blob("transition_global_set_secondary_4c5100_r057",gb.data(),nb);
        }
        if(enabled("transition_global_clear_active_4c5110_r057")){
            auto ng=seed57();ng.active=1u;r057_store_globals(ng);prepare(0x4c5110u);run();transition_global_clear_active_4c5110(ng);auto gb=r057_guest_globals_blob(),nb=r057_globals_blob(ng);compare_blob("transition_global_clear_active_4c5110_r057",gb.data(),nb);
        }
        if(enabled("object_transition_init_440ef0_r057")){
            auto native=r057_object_fixture(i);Bytes nb(native.data(),native.size());auto ng=seed57();r057_store_globals(ng);std::memcpy(reinterpret_cast<void*>(R057ObjectBase),native.data(),native.size());prepare(0x440ef0u);guest_call.ecx=R057ObjectBase;run();object_transition_init_440ef0(nb,ng);compare_blob("object_transition_init_440ef0_r057",reinterpret_cast<void*>(R057ObjectBase),native);auto gb=r057_guest_globals_blob(),ngb=r057_globals_blob(ng);compare_blob("object_transition_init_globals_440ef0_r057",gb.data(),ngb);
        }
        if(enabled("object_transition_config_440f70_r057")){
            auto native=r057_object_fixture(i);Bytes nb(native.data(),native.size());auto ng=seed57();r057_store_globals(ng);std::memcpy(reinterpret_cast<void*>(R057ObjectBase),native.data(),native.size());const std::uint32_t mode=(i%3u)?1u:0u,frames=1u+(i%300u),unused=0xdead0000u^i;prepare(0x440f70u);guest_call.ecx=R057ObjectBase;Bytes(reinterpret_cast<void*>(S),16).put32(0,mode);Bytes(reinterpret_cast<void*>(S),16).put32(4,frames);Bytes(reinterpret_cast<void*>(S),16).put32(8,unused);run_original32();if(guest_call.out_sp!=S+12u)throw std::runtime_error("r057 440f70 stack imbalance");object_transition_config_440f70(nb,std::uint8_t(mode),frames,unused,ng);compare_blob("object_transition_config_440f70_r057",reinterpret_cast<void*>(R057ObjectBase),native);auto gb=r057_guest_globals_blob(),ngb=r057_globals_blob(ng);compare_blob("object_transition_config_globals_440f70_r057",gb.data(),ngb);
        }
        if(enabled("object_transition_update_441020_r057")){
            auto native=r057_object_fixture(i);Bytes nb(native.data(),native.size());auto ng=seed57();R057Trace nt{};PcFloatTransitionServices sv{&nt,r057_activate};const unsigned kind=i%6u;
            float ba=1.0f,bb=1.0f,ta=3.0f,tb=0.75f,ca=1.0f,cb=1.0f;std::int32_t counter=0,total=4;
            if(kind==0u){ta=ca=1.0f;tb=cb=0.75f;}
            else if(kind==2u){ng.active=1u;}
            else if(kind==3u){ba=0.5f;bb=0.5f;ta=1.0f;tb=1.0f;ca=0.75f;cb=0.75f;counter=1;total=2;ng.active=1u;}
            else if(kind==4u){ba=1.0f;bb=1.0f;ta=0.5f;tb=0.5f;ca=1.0f;cb=1.0f;counter=0;total=4;ng.active=0u;}
            else if(kind==5u){ba=1.0f;bb=1.0f;ta=2.0f;tb=0.25f;ca=1.25f;cb=0.8f;counter=2;total=7;ng.active=0u;}
            nb.putf(0x268,ba);nb.putf(0x274,bb);nb.putf(0x26c,ta);nb.putf(0x278,tb);nb.putf(0x264,ca);nb.putf(0x270,cb);nb.puti(0x27c,counter);nb.puti(0x280,total);
            r057_store_globals(ng);std::memcpy(reinterpret_cast<void*>(R057ObjectBase),native.data(),native.size());*reinterpret_cast<std::uint32_t*>(R057StateBase)=0u;prepare(0x441020u);guest_call.ecx=R057ObjectBase;run();object_transition_update_441020(nb,ng,sv);compare_blob("object_transition_update_441020_r057",reinterpret_cast<void*>(R057ObjectBase),native);auto gb=r057_guest_globals_blob(),ngb=r057_globals_blob(ng);compare_blob("object_transition_update_globals_441020_r057",gb.data(),ngb);compare_u32("object_transition_update_activate_441020_r057",*reinterpret_cast<std::uint32_t*>(R057StateBase),nt.activate);
        }
        if(enabled("object_transition_latch_441130_r057")){
            auto native=r057_object_fixture(i);Bytes nb(native.data(),native.size());nb.put8(0x262,(i%3u)==0u?1u:0u);nb.put8(0x261,(i%4u)==0u?0u:1u);nb.put32(0x264,0x3f000000u^(i&0x7ffu));nb.put32(0x270,0x3f400000u^(i&0x3ffu));nb.put32(0x27c,i*7u);std::memcpy(reinterpret_cast<void*>(R057ObjectBase),native.data(),native.size());prepare(0x441130u);guest_call.ecx=R057ObjectBase;run();object_transition_latch_441130(nb);compare_blob("object_transition_latch_441130_r057",reinterpret_cast<void*>(R057ObjectBase),native);
        }
        if(only=="all"){set_r057_transition_patch(false);r057_store_globals(r057_saved_globals);}

        // r058: compact runtime/object lifecycle and the six leaves it directly consumes.
        if(only=="all")set_r058_runtime_patches(true);
        auto seed58=[&](){PcRuntimeControlGlobals g{};g.enabled_d2=1u;g.blocked_bf=0u;g.gate_d4=std::uint8_t(i*17u);g.started_d1=std::uint8_t(i&1u);g.current_ac=R058ManagerBase;g.alternate_b0=(i&1u)?(R058ManagerBase+0x100u):0u;g.mode_836130=(i%7u)==0u?1u:0u;g.reset_word_659930=0x58000000u^i;return g;};
        if(enabled("runtime_manager_reset_454200_r058")){
            std::array<std::uint8_t,0x100> mgr{};{std::mt19937 rg(0x45420058u^(i*2654435761u));for(auto& b:mgr)b=std::uint8_t(rg());}Bytes nb(mgr.data(),mgr.size());auto ng=seed58();r058_store_globals(ng);std::memcpy(reinterpret_cast<void*>(R058ManagerBase),mgr.data(),mgr.size());prepare(0x454200u);guest_call.ecx=R058ManagerBase;run();runtime_manager_reset_454200(nb,ng);compare_blob("runtime_manager_reset_454200_r058",reinterpret_cast<void*>(R058ManagerBase),mgr);auto gb=r058_guest_globals_blob(),ngb=r058_globals_blob(ng);compare_blob("runtime_manager_reset_globals_454200_r058",gb.data(),ngb);
        }
        if(enabled("runtime_set_gate_454220_r058")){
            auto ng=seed58();r058_store_globals(ng);const std::uint8_t v=std::uint8_t(i*37u+5u);prepare(0x454220u);Bytes(reinterpret_cast<void*>(S),16).put32(0,v);run();runtime_set_gate_454220(ng,v);auto gb=r058_guest_globals_blob(),ngb=r058_globals_blob(ng);compare_blob("runtime_set_gate_454220_r058",gb.data(),ngb);
        }
        if(enabled("runtime_get_gate_454230_r058")){
            auto ng=seed58();r058_store_globals(ng);prepare(0x454230u);run();compare_u32("runtime_get_gate_454230_r058",guest_call.out_eax&0xffu,runtime_get_gate_454230(ng));
        }
        if(enabled("runtime_finish_454240_r058")){
            auto ng=seed58();const std::uint8_t ret=std::uint8_t((i*29u)&1u);r058_store_globals(ng);reset_r058_trace(ret);R058NativeTrace nt{};nt.finish_ret=ret;auto sv=r058_services(nt);prepare(0x454240u);run();const auto nr=runtime_finish_454240(ng,sv);compare_u32("runtime_finish_return_454240_r058",guest_call.out_eax&0xffu,nr);auto gb=r058_guest_globals_blob(),ngb=r058_globals_blob(ng);compare_blob("runtime_finish_globals_454240_r058",gb.data(),ngb);auto gt=r058_guest_trace_blob(),ntb=r058_trace_blob(nt);compare_blob("runtime_finish_trace_454240_r058",gt.data(),ntb);
        }
        if(enabled("runtime_child_counter_dec_4464f0_r058")){
            std::array<std::uint8_t,0x500> native{};std::mt19937 rg(0x4464f058u^(i*2654435761u));for(auto& b:native)b=std::uint8_t(rg());Bytes nb(native.data(),native.size());nb.put32(0x408,(i%5u)==0u?0u:(1u+i%100u));std::memcpy(reinterpret_cast<void*>(R058ChildBase),native.data(),native.size());prepare(0x4464f0u);guest_call.ecx=R058ChildBase;run();runtime_child_counter_dec_4464f0(nb);compare_blob("runtime_child_counter_dec_4464f0_r058",reinterpret_cast<void*>(R058ChildBase),native);
        }
        if(enabled("runtime_child_state_564c90_r058")){
            std::array<std::uint8_t,0x40> native{};Bytes nb(native.data(),native.size());const std::uint32_t v=0x58000000u^(i*0x10101u);nb.put32(8,v);std::memcpy(reinterpret_cast<void*>(R058ChildBase),native.data(),native.size());prepare(0x564c90u);guest_call.ecx=R058ChildBase;run();compare_u32("runtime_child_state_564c90_r058",guest_call.out_eax,runtime_child_state_564c90(nb));
        }
        if(enabled("runtime_shutdown_4411a0_r058")){
            std::array<std::uint8_t,0x100> mgr{};{std::mt19937 rg(0x45420058u^(i*2654435761u));for(auto& b:mgr)b=std::uint8_t(rg());}Bytes nb(mgr.data(),mgr.size());nb.put32(0,R058VtableBase);auto ng=seed58();const unsigned kind=i%5u;if(kind==0u)ng.enabled_d2=0u;else if(kind==1u)ng.blocked_bf=1u;else if(kind==2u)ng.current_ac=0u;const std::uint8_t ret=std::uint8_t((i/3u)&1u);r058_store_globals(ng);std::memcpy(reinterpret_cast<void*>(R058ManagerBase),mgr.data(),mgr.size());reset_r058_trace(ret);R058NativeTrace nt{};nt.finish_ret=ret;auto sv=r058_services(nt);prepare(0x4411a0u);run();runtime_shutdown_4411a0(ng,nb,sv);compare_blob("runtime_shutdown_manager_4411a0_r058",reinterpret_cast<void*>(R058ManagerBase),mgr);auto gb=r058_guest_globals_blob(),ngb=r058_globals_blob(ng);compare_blob("runtime_shutdown_globals_4411a0_r058",gb.data(),ngb);auto gt=r058_guest_trace_blob(),ntb=r058_trace_blob(nt);compare_blob("runtime_shutdown_trace_4411a0_r058",gt.data(),ntb);
        }
        if(enabled("runtime_release_handle_441200_r058")){
            auto native=r058_object_fixture(i);Bytes nb(native.data(),native.size());const std::uint32_t handle=(i%7u)==0u?0u:R058ChildBase;const std::int8_t idx=(i%5u)==0u?0:std::int8_t(1u+(i%12u));const std::uint8_t depth=std::uint8_t(1u+(i%8u));nb.put8(0x220,depth);nb.put32(0x924,1u+(i%9u));if(idx>0){nb.put8(0x221u+std::uint8_t(idx),0xa5u);nb.put8(0x241u+depth,0x5au);}std::memcpy(reinterpret_cast<void*>(R058ObjectBase),native.data(),native.size());std::memset(reinterpret_cast<void*>(R058ChildBase),0,0x100u);*reinterpret_cast<std::uint32_t*>(R058ChildBase)=R058VtableBase;reset_r058_trace(0u,0u,idx);R058NativeTrace nt{};nt.release_index=idx;auto sv=r058_services(nt);prepare(0x441200u);guest_call.ecx=R058ObjectBase;Bytes(reinterpret_cast<void*>(S),16).put32(0,handle);run_original32();if(guest_call.out_sp!=S+4u)throw std::runtime_error("r058 441200 stack imbalance");runtime_release_handle_441200(nb,handle,sv);compare_blob("runtime_release_handle_441200_r058",reinterpret_cast<void*>(R058ObjectBase),native);auto gt=r058_guest_trace_blob(),ntb=r058_trace_blob(nt);compare_blob("runtime_release_trace_441200_r058",gt.data(),ntb);
        }
        if(enabled("runtime_ready_441260_r058")){
            auto native=r058_object_fixture(i);Bytes nb(native.data(),native.size());auto ng=seed58();const std::uint32_t child=(i%4u)==0u?0u:R058ChildBase;const std::uint32_t state=(i%6u)==0u?0x0eu:(0x20u+(i%17u));nb.put32(0x488,child);std::memcpy(reinterpret_cast<void*>(R058ObjectBase),native.data(),native.size());std::memset(reinterpret_cast<void*>(R058ChildBase),0,0x40u);*reinterpret_cast<std::uint32_t*>(R058ChildBase+8u)=state;const std::uint8_t pair=std::uint8_t((i%5u)==1u);if(i%9u==0u)ng.alternate_b0=0u;if(i%11u==0u)ng.current_ac=0u;r058_store_globals(ng);reset_r058_trace(0u,pair,0);R058NativeTrace nt{};nt.pair_ret=pair;nt.child_state=state;auto sv=r058_services(nt);prepare(0x441260u);guest_call.ecx=R058ObjectBase;run();const auto nr=runtime_ready_441260(nb,ng,sv);compare_u32("runtime_ready_return_441260_r058",guest_call.out_eax&0xffu,nr);auto gt=r058_guest_trace_blob(),ntb=r058_trace_blob(nt);compare_blob("runtime_ready_trace_441260_r058",gt.data(),ntb);
        }
        if(enabled("runtime_status_4412c0_r058")){
            auto native=r058_object_fixture(i);Bytes nb(native.data(),native.size());const std::uint32_t child=(i%3u)==0u?0u:R058ChildBase,state=0x30u+(i%33u);nb.put32(0x488,child);nb.put8(0x48c,std::uint8_t((i%4u)!=0u));std::memcpy(reinterpret_cast<void*>(R058ObjectBase),native.data(),native.size());std::memset(reinterpret_cast<void*>(R058ChildBase),0,0x40u);*reinterpret_cast<std::uint32_t*>(R058ChildBase+8u)=state;R058NativeTrace nt{};nt.child_state=state;auto sv=r058_services(nt);prepare(0x4412c0u);guest_call.ecx=R058ObjectBase;run();compare_u32("runtime_status_4412c0_r058",guest_call.out_eax,runtime_status_4412c0(nb,sv));
        }
        if(enabled("runtime_has_handle_4412f0_r058")){
            auto native=r058_object_fixture(i);Bytes nb(native.data(),native.size());const std::uint8_t v=std::uint8_t(i*41u);nb.put8(0x48c,v);std::memcpy(reinterpret_cast<void*>(R058ObjectBase),native.data(),native.size());prepare(0x4412f0u);guest_call.ecx=R058ObjectBase;run();compare_u32("runtime_has_handle_4412f0_r058",guest_call.out_eax&0xffu,runtime_has_handle_4412f0(nb));
        }
        if(enabled("runtime_close_if_status_441300_r058")){
            auto native=r058_object_fixture(i);Bytes nb(native.data(),native.size());const std::uint32_t child=(i%5u)==0u?0u:R058ChildBase,state=0x40u+(i%19u);nb.put32(0x488,child);nb.put8(0x48c,std::uint8_t((i%6u)!=0u));nb.put8(0x220,std::uint8_t(1u+(i%6u)));nb.put32(0x924,1u+(i%7u));const std::int8_t idx=(i%4u)==0u?0:std::int8_t(1u+(i%10u));if(idx>0){nb.put8(0x221u+std::uint8_t(idx),0xa5u);nb.put8(0x241u+nb.u8(0x220),0x5au);}const std::uint32_t requested=(i%4u)==0u?0x53u:((i%4u)==1u?state:0x99u);std::memcpy(reinterpret_cast<void*>(R058ObjectBase),native.data(),native.size());std::memset(reinterpret_cast<void*>(R058ChildBase),0,0x100u);*reinterpret_cast<std::uint32_t*>(R058ChildBase)=R058VtableBase;*reinterpret_cast<std::uint32_t*>(R058ChildBase+8u)=state;reset_r058_trace(0u,0u,idx);R058NativeTrace nt{};nt.child_state=state;nt.release_index=idx;auto sv=r058_services(nt);prepare(0x441300u);guest_call.ecx=R058ObjectBase;Bytes(reinterpret_cast<void*>(S),16).put32(0,requested);run_original32();if(guest_call.out_sp!=S+4u)throw std::runtime_error("r058 441300 stack imbalance");const auto nr=runtime_close_if_status_441300(nb,requested,sv);compare_u32("runtime_close_return_441300_r058",guest_call.out_eax&0xffu,nr);compare_blob("runtime_close_object_441300_r058",reinterpret_cast<void*>(R058ObjectBase),native);auto gt=r058_guest_trace_blob(),ntb=r058_trace_blob(nt);compare_blob("runtime_close_trace_441300_r058",gt.data(),ntb);
        }
        if(only=="all"){set_r058_runtime_patches(false);r058_store_globals(r058_saved_globals);}

        // r059: compact UI/resource controller and directly consumed leaves.
        if(only=="all")set_r059_patches(true);
        auto seed59=[&](){PcUiNotifyGlobals g{};g.x=std::int16_t(i*17u);g.y=std::int16_t(i*29u);g.base_x=std::int16_t(i*31u);g.base_y=std::int16_t(i*43u);g.color=0x10203040u^(i*0x01010101u);g.mode=i%11u;g.source_x=0x70000000u+(i*3u);g.source_y=0x71000000u+(i*5u);g.alternate=std::uint8_t(i&1u);return g;};
        if(enabled("ui_resource_reset_465250_r059")){
            auto native=r059_object_fixture(i);Bytes nb(native.data(),native.size());auto rv=nb.sub(0xcf4,nb.size()-0xcf4);const std::uint32_t h=(i%5u)==0u?0xffffffffu:(0x12000000u+i);rv.put32(8,h);rv.put32(0x24,(i%3u)==0u?1u:0u);rv.put32(0x28,0xaabbccddu);rv.put32(0x2c,0x11223344u);
            std::memcpy(reinterpret_cast<void*>(R059ObjectBase),native.data(),native.size());reset_r059_trace(0);R059Trace nt{};auto sv=r059_services(nt);prepare(0x465250u);guest_call.ecx=R059ObjectBase+0xcf4u;run();ui_resource_reset_465250(rv,sv);compare_blob("ui_resource_reset_465250_r059",reinterpret_cast<void*>(R059ObjectBase),native);auto gt=r059_guest_trace_blob(),ntb=r059_trace_blob(nt);compare_blob("ui_resource_reset_trace_465250_r059",gt.data(),ntb);
        }
        if(enabled("ui_resource_ready_4652e0_r059")){
            auto native=r059_object_fixture(i);Bytes nb(native.data(),native.size());auto rv=nb.sub(0xcf4,nb.size()-0xcf4);const std::uint32_t h=(i%7u)==0u?0xffffffffu:(0x22000000u+i);const std::int32_t hold=(i%5u)==0u?1:0;const std::int32_t status=std::int32_t(i%4u);rv.put32(8,h);rv.puti(0x24,hold);
            std::memcpy(reinterpret_cast<void*>(R059ObjectBase),native.data(),native.size());reset_r059_trace(status);R059Trace nt{};nt.status=status;auto sv=r059_services(nt);prepare(0x4652e0u);guest_call.ecx=R059ObjectBase+0xcf4u;run();const auto nr=ui_resource_ready_4652e0(rv,sv);compare_u32("ui_resource_ready_return_4652e0_r059",guest_call.out_eax,nr);auto gt=r059_guest_trace_blob(),ntb=r059_trace_blob(nt);compare_blob("ui_resource_ready_trace_4652e0_r059",gt.data(),ntb);
        }
        if(enabled("ui_text_set_xy_42cc00_r059")){
            auto ng=seed59();r059_store_globals(ng);const std::uint32_t x=0x12340000u^(i*17u),y=0xabcd0000u^(i*29u);prepare(0x42cc00u);Bytes st(reinterpret_cast<void*>(S),16);st.put32(0,x);st.put32(4,y);run();ui_text_set_xy_42cc00(ng,x,y);auto gb=r059_guest_globals_blob(),nb=r059_globals_blob(ng);compare_blob("ui_text_set_xy_42cc00_r059",gb.data(),nb);
        }
        if(enabled("ui_text_set_color_42cca0_r059")){
            auto ng=seed59();r059_store_globals(ng);const std::uint32_t v=0xff000000u^(i*0x010203u);prepare(0x42cca0u);Bytes(reinterpret_cast<void*>(S),8).put32(0,v);run();ui_text_set_color_42cca0(ng,v);auto gb=r059_guest_globals_blob(),nb=r059_globals_blob(ng);compare_blob("ui_text_set_color_42cca0_r059",gb.data(),nb);
        }
        if(enabled("ui_text_set_mode_42ccb0_r059")){
            auto ng=seed59();r059_store_globals(ng);const std::uint32_t v=i%17u;prepare(0x42ccb0u);Bytes(reinterpret_cast<void*>(S),8).put32(0,v);run();ui_text_set_mode_42ccb0(ng,v);auto gb=r059_guest_globals_blob(),nb=r059_globals_blob(ng);compare_blob("ui_text_set_mode_42ccb0_r059",gb.data(),nb);
        }
        if(enabled("ui_table_lookup_465eb0_r059")){
            std::array<std::uint8_t,R059TableSize> native{};Bytes tb(native.data(),native.size());const std::uint32_t idx=i%0x700u;const std::uint32_t v=0x59000000u^(i*2654435761u);tb.put32(idx*4u,v);std::memcpy(reinterpret_cast<void*>(R059TableBase),native.data(),native.size());prepare(0x465eb0u);Bytes(reinterpret_cast<void*>(S),8).put32(0,idx);run();compare_u32("ui_table_lookup_465eb0_r059",guest_call.out_eax,ui_table_lookup_465eb0(tb,idx));
        }
        if(enabled("ui_set_active_4413f0_r059")){
            auto native=r059_object_fixture(i);Bytes nb(native.data(),native.size());auto rv=nb.sub(0xcf4,nb.size()-0xcf4);rv.put32(8,(i%6u)==0u?0xffffffffu:(0x33000000u+i));rv.put32(0x24,(i%4u)==0u?1u:0u);rv.put32(0x28,0x99u);rv.put32(0x2c,0x88u);const std::uint8_t active=std::uint8_t((i%3u)!=0u);
            std::memcpy(reinterpret_cast<void*>(R059ObjectBase),native.data(),native.size());reset_r059_trace(0);R059Trace nt{};auto sv=r059_services(nt);prepare(0x4413f0u);guest_call.ecx=R059ObjectBase;Bytes(reinterpret_cast<void*>(S),8).put32(0,active);run_original32();ui_set_active_4413f0(nb,active,sv);compare_blob("ui_set_active_4413f0_r059",reinterpret_cast<void*>(R059ObjectBase),native);auto gt=r059_guest_trace_blob(),ntb=r059_trace_blob(nt);compare_blob("ui_set_active_trace_4413f0_r059",gt.data(),ntb);
        }
        if(enabled("compact_u32_list_441410_r059")){
            auto native=r059_object_fixture(i);Bytes nb(native.data(),native.size());const std::int32_t count=1+std::int32_t(i%31u);const std::int32_t index=std::int32_t(i%std::uint32_t(count));nb.puti(0x80,count);for(int k=0;k<count+1;++k)nb.put32(std::size_t(k)*4u,0x59000000u+std::uint32_t(k)*0x101u+i);
            std::memcpy(reinterpret_cast<void*>(R059ObjectBase),native.data(),native.size());prepare(0x441410u);guest_call.ecx=R059ObjectBase;Bytes(reinterpret_cast<void*>(S),8).puti(0,index);run_original32();compact_u32_list_441410(nb,index);compare_blob("compact_u32_list_441410_r059",reinterpret_cast<void*>(R059ObjectBase),native);
        }
        if(enabled("ui_notify_441370_r059")){
            auto native=r059_object_fixture(i);Bytes nb(native.data(),native.size());auto rv=nb.sub(0xc48,nb.size()-0xc48);const unsigned kind=i%5u;nb.put32(0xc50,kind==0u?0xffffffffu:std::uint32_t(i));rv.put32(8,kind==1u?0xffffffffu:(0x44000000u+i));rv.puti(0x24,kind==2u?1:0);const std::int32_t status=kind==3u?1:2;auto ng=seed59();ng.alternate=std::uint8_t((i/2u)&1u);r059_store_globals(ng);
            std::array<std::uint8_t,R059TableSize> table{};Bytes tb(table.data(),table.size());const std::uint32_t v474=0x70000474u^(i*3u),v476=0x70000476u^(i*5u);tb.put32(0x474u*4u,v474);tb.put32(0x476u*4u,v476);std::memcpy(reinterpret_cast<void*>(R059TableBase),table.data(),table.size());std::memcpy(reinterpret_cast<void*>(R059ObjectBase),native.data(),native.size());reset_r059_trace(status);R059Trace nt{};nt.status=status;nt.lookup474=v474;nt.lookup476=v476;auto sv=r059_services(nt);prepare(0x441370u);guest_call.ecx=R059ObjectBase;run();ui_notify_441370(nb,ng,sv);compare_blob("ui_notify_object_441370_r059",reinterpret_cast<void*>(R059ObjectBase),native);auto gb=r059_guest_globals_blob(),ngb=r059_globals_blob(ng);compare_blob("ui_notify_globals_441370_r059",gb.data(),ngb);auto gt=r059_guest_trace_blob(),ntb=r059_trace_blob(nt);compare_blob("ui_notify_trace_441370_r059",gt.data(),ntb);
        }
        if(only=="all")set_r059_patches(false);
        // r060: first 13 SEH-wrapped object factories.  The oracle executes
        // the original normal-path branch/size/return logic with only Windows
        // fs:0 SEH plumbing neutralized and allocator/constructor boundaries scripted.
        if(only=="all")set_r060_factory_patches(true);
        for(const auto& f:R060Factories)if(enabled(f.label)){
            const bool fail=(i%5u)==0u;const std::uint32_t alloc_ret=fail?0u:(0x37000000u+(i&0xfffu)*0x100u+(f.start&0xffu));
            const std::uint32_t ctor_ret=0x76000000u^(i*2654435761u)^f.ctor;reset_r060_trace(alloc_ret,ctor_ret);
            prepare(f.start);run();R060Trace nt{};nt.alloc_ret=alloc_ret;nt.ctor_ret=ctor_ret;auto sv=r060_services(nt);const auto nr=f.native(sv);
            compare_u32(f.label,guest_call.out_eax,nr);auto gt=r060_guest_trace_blob(),nb=r060_trace_blob(nt);compare_blob((std::string(f.label)+"_trace").c_str(),gt.data(),nb);
        }
        if(only=="all")set_r060_factory_patches(false);
        // r061: 44-factory continuation up to the first real shape break at 0x4429B0.
        if(only=="all")set_r061_factory_patches(true);
        for(const auto& f:R061Factories)if(enabled(f.label)){
            const bool fail=(i%5u)==0u;const std::uint32_t alloc_ret=fail?0u:(0x39000000u+(i&0xfffu)*0x100u+(f.start&0xffu));
            const std::uint32_t ctor_ret=0x77000000u^(i*2654435761u)^f.ctor;reset_r060_trace(alloc_ret,ctor_ret);
            prepare(f.start);run();R060Trace nt{};nt.alloc_ret=alloc_ret;nt.ctor_ret=ctor_ret;auto sv=r060_services(nt);const auto nr=f.native(sv);
            compare_u32(f.label,guest_call.out_eax,nr);auto gt=r060_guest_trace_blob(),nb=r060_trace_blob(nt);compare_blob((std::string(f.label)+"_trace").c_str(),gt.data(),nb);
        }
        if(only=="all")set_r061_factory_patches(false);
        // r062: four in-place constructors/initializers.  Child bodies are
        // explicitly stubbed; object bytes, call order/arguments and EAX are compared.
        if(only=="all")set_r062_constructor_patches(true);
        const std::uint8_t r062_saved_mode=*reinterpret_cast<std::uint8_t*>(0x6319a1u);
        if(enabled("object_ctor_4429b0_r062")){
            auto native=r062_object_fixture(i);Bytes nb(native.data(),native.size());std::memcpy(reinterpret_cast<void*>(R062ObjectBase),native.data(),native.size());reset_r062_trace();R062Trace nt{};auto sv=r062_services(nt,0u);
            prepare(0x4429b0u);guest_call.ecx=R062ObjectBase;run();const auto nr=object_ctor_4429b0(nb,R062ObjectBase,sv);compare_u32("object_ctor_4429b0_return_r062",guest_call.out_eax,std::uint32_t(nr));compare_blob("object_ctor_4429b0_r062",reinterpret_cast<void*>(R062ObjectBase),native);auto gt=r062_guest_trace_blob(),ntb=r062_trace_blob(nt);compare_blob("object_ctor_4429b0_trace_r062",gt.data(),ntb);
        }
        if(enabled("object_ctor_442a60_r062")){
            auto native=r062_object_fixture(i);Bytes nb(native.data(),native.size());std::memcpy(reinterpret_cast<void*>(R062ObjectBase),native.data(),native.size());reset_r062_trace();R062Trace nt{};auto sv=r062_services(nt,6u);
            prepare(0x442a60u);guest_call.ecx=R062ObjectBase;run();const auto nr=object_ctor_442a60(nb,R062ObjectBase,sv);compare_u32("object_ctor_442a60_return_r062",guest_call.out_eax,std::uint32_t(nr));compare_blob("object_ctor_442a60_r062",reinterpret_cast<void*>(R062ObjectBase),native);auto gt=r062_guest_trace_blob(),ntb=r062_trace_blob(nt);compare_blob("object_ctor_442a60_trace_r062",gt.data(),ntb);
        }
        if(enabled("object_block_init_442ac0_r062")){
            auto native=r062_object_fixture(i);Bytes nb(native.data(),native.size());std::memcpy(reinterpret_cast<void*>(R062ObjectBase),native.data(),native.size());reset_r062_trace();R062Trace nt{};auto sv=r062_services(nt,9u);
            prepare(0x442ac0u);guest_call.ecx=R062ObjectBase;run();const auto nr=object_block_init_442ac0(nb,R062ObjectBase,sv);compare_u32("object_block_init_442ac0_return_r062",guest_call.out_eax,std::uint32_t(nr));compare_blob("object_block_init_442ac0_r062",reinterpret_cast<void*>(R062ObjectBase),native);auto gt=r062_guest_trace_blob(),ntb=r062_trace_blob(nt);compare_blob("object_block_init_442ac0_trace_r062",gt.data(),ntb);
        }
        if(enabled("object_state_ctor_442b20_r062")){
            auto native=r062_object_fixture(i);Bytes nb(native.data(),native.size());std::memcpy(reinterpret_cast<void*>(R062ObjectBase),native.data(),native.size());reset_r062_trace();R062Trace nt{};auto sv=r062_services(nt,10u);const std::uint8_t mode=std::uint8_t((i%3u)==0u?0u:((i%3u)==1u?1u:0x80u));r062_set_mode(mode);
            prepare(0x442b20u);guest_call.ecx=R062ObjectBase;run();const auto nr=object_state_ctor_442b20(nb,R062ObjectBase,mode,sv);compare_u32("object_state_ctor_442b20_return_r062",guest_call.out_eax,std::uint32_t(nr));compare_blob("object_state_ctor_442b20_r062",reinterpret_cast<void*>(R062ObjectBase),native);auto gt=r062_guest_trace_blob(),ntb=r062_trace_blob(nt);compare_blob("object_state_ctor_442b20_trace_r062",gt.data(),ntb);
        }
        r062_set_mode(r062_saved_mode);
        if(only=="all")set_r062_constructor_patches(false);

        // r063: direct timer getter, state reset and two state selectors.  The
        // 0x4AF500 / 0x4EE930 direct calls and virtual +0x08 calls remain explicit boundaries.
        if(only=="all")set_r063_patches(true);
        if(enabled("timer_value_4af500_r063")){
            const float saved=*reinterpret_cast<float*>(0x842110u);
            float timer=float(int(i%8192u)-4096)*0.03125f;if(i%31u==0u)timer=-0.0f;
            *reinterpret_cast<float*>(0x842110u)=timer;prepare(0x4af500u);guest_call.st0=1u;run();
            compare_float("timer_value_4af500_r063",ffrom(guest_call.out_st0),timer_value_4af500(timer));
            *reinterpret_cast<float*>(0x842110u)=saved;
        }
        if(enabled("object_state_reset_442c20_r063")){
            auto native=r063_object_fixture(i);Bytes nb(native.data(),native.size());std::memcpy(reinterpret_cast<void*>(R063ObjectBase),native.data(),native.size());
            const std::uint8_t saved_mode=*reinterpret_cast<std::uint8_t*>(0x6319a1u);const std::uint8_t mode=std::uint8_t((i%3u)==0u?0u:((i%3u)==1u?1u:0x80u));*reinterpret_cast<std::uint8_t*>(0x6319a1u)=mode;
            float timer=float(int(i%8192u)-4096)*0.03125f;if(i%29u==0u)timer=-0.0f;const auto vret=0x63000000u^(i*2654435761u);reset_r063_trace(timer,vret);
            R063Trace nt{};nt.timer=timer;nt.virtual_ret=vret;auto sv=r063_reset_services(nt);const float scale=*reinterpret_cast<const float*>(0x62812cu);
            prepare(0x442c20u);guest_call.ecx=R063ObjectBase;run();const auto nr=object_state_reset_442c20(nb,mode,scale,sv);
            compare_u32("object_state_reset_442c20_return_r063",guest_call.out_eax&0xffu,nr);compare_blob("object_state_reset_442c20_r063",reinterpret_cast<void*>(R063ObjectBase),native);auto gt=r063_guest_trace_blob(),ntb=r063_trace_blob(nt);compare_blob("object_state_reset_442c20_trace_r063",gt.data(),ntb);
            *reinterpret_cast<std::uint8_t*>(0x6319a1u)=saved_mode;
        }
        if(enabled("object_select_primary_442cb0_r063")){
            auto native=r063_object_fixture(i);Bytes nb(native.data(),native.size());r063_clear_selector_fields(nb);const unsigned mode=i%8u;const std::uint32_t vret=0x63100000u^(i*0x10203u);
            PcObjectSelectorGlobals ng{};
            if(mode==0u||mode==1u){nb.puti(0x518u,1);nb.put32(0x498u,r063_child(3));}
            else if(mode==2u){nb.puti(0x518u,-1);nb.put8(0x494u,1u);nb.put32(0x490u,r063_child(2));}
            else if(mode==3u||mode==7u){nb.puti(0x518u,-1);nb.put8(0x48cu,1u);nb.put32(0x488u,r063_child(1));}
            else if(mode==4u){nb.puti(0x518u,-1);const std::int32_t index=1+std::int32_t(i%8u);nb.puti(0x484u,index);nb.put32(0x280u+std::size_t(index)*4u,r063_child(0));}
            else nb.puti(0x518u,-1);
            if(mode==1u||mode==6u||mode==7u)ng={R063ManagerBase,1u,1u};else if(mode==3u)ng={R063ManagerBase,0u,1u};
            std::memcpy(reinterpret_cast<void*>(R063ObjectBase),native.data(),native.size());std::memset(reinterpret_cast<void*>(R063ManagerBase),0,0x40u);
            *reinterpret_cast<std::uint8_t*>(R063ManagerBase+5u)=ng.manager_flag5;*reinterpret_cast<std::uint8_t*>(R063ManagerBase+8u)=ng.manager_flag8;
            const std::uint32_t saved_manager=*reinterpret_cast<std::uint32_t*>(0x7d68acu);*reinterpret_cast<std::uint32_t*>(0x7d68acu)=ng.manager_handle;
            *reinterpret_cast<std::uint32_t*>(R063StateBase+0x40u)=0xccccccccu;reset_r063_trace(0.0f,vret);R063Trace nt{};nt.virtual_ret=vret;auto sv=r063_selector_services(nt);std::int32_t selected=-77;
            prepare(0x442cb0u);guest_call.ecx=R063ObjectBase;Bytes(reinterpret_cast<void*>(S),16).put32(0,R063StateBase+0x40u);run_original32();if(guest_call.out_sp!=S+4u)throw std::runtime_error("r063 442cb0 stack imbalance");
            const auto nr=object_select_primary_442cb0(nb,selected,ng,sv);compare_u32("object_select_primary_442cb0_return_r063",guest_call.out_eax,nr);compare_u32("object_select_primary_442cb0_kind_r063",*reinterpret_cast<std::uint32_t*>(R063StateBase+0x40u),std::uint32_t(selected));
            compare_blob("object_select_primary_442cb0_r063",reinterpret_cast<void*>(R063ObjectBase),native);compare_u32("object_select_primary_442cb0_manager_flag8_r063",*reinterpret_cast<std::uint8_t*>(R063ManagerBase+8u),ng.manager_flag8);auto gt=r063_guest_trace_blob(),ntb=r063_trace_blob(nt);compare_blob("object_select_primary_442cb0_trace_r063",gt.data(),ntb);*reinterpret_cast<std::uint32_t*>(0x7d68acu)=saved_manager;
        }
        if(enabled("object_select_secondary_442d70_r063")){
            auto native=r063_object_fixture(i);Bytes nb(native.data(),native.size());r063_clear_selector_fields(nb);const unsigned mode=i%6u;const std::uint32_t vret=0x63200000u^(i*0x30405u);
            nb.puti(0x518u,-1);if(mode==0u){nb.puti(0x518u,1);nb.put32(0x498u,r063_child(3));}
            else if(mode==1u){nb.put8(0x494u,1u);nb.put32(0x490u,r063_child(2));}
            else if(mode==2u){nb.put8(0x494u,1u);nb.put32(0x490u,0u);nb.put8(0x48cu,1u);nb.put32(0x488u,r063_child(1));}
            else if(mode==3u){nb.put8(0x48cu,1u);nb.put32(0x488u,r063_child(1));}
            else if(mode==4u){nb.put8(0x48cu,1u);nb.put32(0x488u,0u);}
            std::memcpy(reinterpret_cast<void*>(R063ObjectBase),native.data(),native.size());*reinterpret_cast<std::uint32_t*>(R063StateBase+0x40u)=0xddddddddu;reset_r063_trace(0.0f,vret);R063Trace nt{};nt.virtual_ret=vret;auto sv=r063_selector_services(nt);std::int32_t selected=-77;
            prepare(0x442d70u);guest_call.ecx=R063ObjectBase;Bytes(reinterpret_cast<void*>(S),16).put32(0,R063StateBase+0x40u);run_original32();if(guest_call.out_sp!=S+4u)throw std::runtime_error("r063 442d70 stack imbalance");
            const auto nr=object_select_secondary_442d70(nb,selected,sv);compare_u32("object_select_secondary_442d70_return_r063",guest_call.out_eax,nr);compare_u32("object_select_secondary_442d70_kind_r063",*reinterpret_cast<std::uint32_t*>(R063StateBase+0x40u),std::uint32_t(selected));
            compare_blob("object_select_secondary_442d70_r063",reinterpret_cast<void*>(R063ObjectBase),native);auto gt=r063_guest_trace_blob(),ntb=r063_trace_blob(nt);compare_blob("object_select_secondary_442d70_trace_r063",gt.data(),ntb);
        }
        if(only=="all")set_r063_patches(false);

        // r064: immediate dispatcher/state-table/query block.  0x564C90 is
        // already closed and executes directly on synthetic child objects; virtual
        // +0x0C and 0x446A50 remain explicit traced boundaries.
        if(only=="all")set_r064_patches(true);
        r064_prepare_children(i);
        if(enabled("object_dispatch_state_442e00_r064")){
            auto native=r064_object_fixture(i);Bytes nb(native.data(),native.size());nb.puti(0x484,0);nb.puti(0x518,0);nb.put8(0x494,0);nb.put8(0x48c,0);
            const unsigned mode=i%16u;
            if(mode&1u){const auto index=1+std::int32_t(i%12u);nb.puti(0x484,index);nb.put32(0x280u+std::size_t(index)*4u,r064_child(1u+unsigned(index)));}
            if(mode&2u){nb.puti(0x518,1);nb.put32(0x498,r064_child(40u));}
            if(mode&4u){nb.put8(0x494,1);nb.put32(0x490,(i%9u)==0u?0u:r064_child(41u));}
            if(mode&8u){nb.put8(0x48c,1);nb.put32(0x488,(i%11u)==0u?0u:r064_child(42u));}
            std::memcpy(reinterpret_cast<void*>(R064ObjectBase),native.data(),native.size());reset_r064_trace();R064Trace nt{};auto sv=r064_services(nt);
            prepare(0x442e00u);guest_call.ecx=R064ObjectBase;run();object_dispatch_state_442e00(nb,sv);compare_blob("object_dispatch_state_442e00_r064",reinterpret_cast<void*>(R064ObjectBase),native);auto gt=r064_guest_trace_blob(),ntb=r064_trace_blob(nt);compare_blob("object_dispatch_state_442e00_trace_r064",gt.data(),ntb);
        }
        if(enabled("object_refresh_state_table_442e70_r064")){
            auto native=r064_object_fixture(i);Bytes nb(native.data(),native.size());const std::int32_t choices[]={-3,0,1,2,3,7,31,64,127,128};const auto count=choices[i%(sizeof(choices)/sizeof(choices[0]))];nb.puti(0x484,count);
            if(count>0)for(std::int32_t k=0;k<count;++k)nb.put32(0x284u+std::size_t(k)*4u,r064_child(unsigned(k)));
            std::memcpy(reinterpret_cast<void*>(R064ObjectBase),native.data(),native.size());R064Trace nt{};auto sv=r064_services(nt);prepare(0x442e70u);guest_call.ecx=R064ObjectBase;run();object_refresh_state_table_442e70(nb,sv);compare_blob("object_refresh_state_table_442e70_r064",reinterpret_cast<void*>(R064ObjectBase),native);
        }
        if(enabled("object_query_previous_state_442ec0_r064")){
            auto native=r064_object_fixture(i);Bytes nb(native.data(),native.size());const std::int32_t choices[]={-5,0,1,2,3,9,64,128};const auto count=choices[i%(sizeof(choices)/sizeof(choices[0]))];nb.puti(0x484,count);if(count>=2)nb.put32(0x27cu+std::size_t(count)*4u,r064_child(unsigned(count)));std::memcpy(reinterpret_cast<void*>(R064ObjectBase),native.data(),native.size());R064Trace nt{};auto sv=r064_services(nt);prepare(0x442ec0u);guest_call.ecx=R064ObjectBase;run();compare_u32("object_query_previous_state_442ec0_r064",guest_call.out_eax,object_query_previous_state_442ec0(nb,sv));
        }
        if(enabled("object_store_depth_pair_442f20_r064")){
            auto native=r064_object_fixture(i);Bytes nb(native.data(),native.size());const auto depth=std::uint8_t(i%32u),a=std::uint8_t(i*37u+3u),b=std::uint8_t(i*53u+7u);nb.put8(0x220u,depth);std::memcpy(reinterpret_cast<void*>(R064ObjectBase),native.data(),native.size());prepare(0x442f20u);guest_call.ecx=R064ObjectBase;auto st=Bytes(reinterpret_cast<void*>(S),16);st.put32(0,a);st.put32(4,b);run_original32();if(guest_call.out_sp!=S+8u)throw std::runtime_error("r064 442f20 stack imbalance");object_store_depth_pair_442f20(nb,a,b);compare_blob("object_store_depth_pair_442f20_r064",reinterpret_cast<void*>(R064ObjectBase),native);
        }
        if(enabled("object_query_last_state_443040_r064")){
            auto native=r064_object_fixture(i);Bytes nb(native.data(),native.size());const std::int32_t choices[]={-4,0,1,2,5,17,64,128};const auto count=choices[i%(sizeof(choices)/sizeof(choices[0]))];nb.puti(0x484,count);if(count>0)nb.put32(0x280u+std::size_t(count)*4u,r064_child(unsigned(count)));std::memcpy(reinterpret_cast<void*>(R064ObjectBase),native.data(),native.size());R064Trace nt{};auto sv=r064_services(nt);prepare(0x443040u);guest_call.ecx=R064ObjectBase;run();compare_u32("object_query_last_state_443040_r064",guest_call.out_eax,object_query_last_state_443040(nb,sv));
        }
        if(enabled("object_has_active_state_443060_r064")){
            auto native=r064_object_fixture(i);Bytes nb(native.data(),native.size());nb.put32(0x218,(i%3u)==0u?2u:((i%3u)==1u?1u:3u));nb.puti(0x518,(i%5u)==0u?1:0);nb.put8(0x48c,(i%7u)==0u?1u:0u);nb.put8(0x494,(i%11u)==0u?1u:0u);nb.puti(0x484,(i%13u)==0u?2:0);std::memcpy(reinterpret_cast<void*>(R064ObjectBase),native.data(),native.size());prepare(0x443060u);guest_call.ecx=R064ObjectBase;run();compare_u32("object_has_active_state_443060_r064",guest_call.out_eax&0xffu,object_has_active_state_443060(nb));
        }
        if(only=="all")set_r064_patches(false);

        // r065: keyed pair-table update and flagged-handle release.  The
        // 0x446EA0 state consumer remains an explicit boundary; 0x441200 is
        // the already-closed r058 helper and executes directly on synthetic
        // child objects so parent ordering/effects are checked transitively.
        if(only=="all"){set_r058_runtime_patches(true);set_r065_patches(true);}
        r065_prepare_children(i);
        if(enabled("object_store_state_pair_442f50_r065")){
            auto native=r065_object_fixture(i);Bytes nb(native.data(),native.size());const auto& canonical=pc_object_state_pair_table_r065();const auto table=r065_pair_table();
            const auto depth=std::uint8_t(i%32u);nb.put8(0x220u,depth);
            std::uint32_t key=0x53u;if(i%5u==0u)key=canonical[i%canonical.size()].key;else if(i%5u==1u)key=11u;else if(i%5u==2u)key=43u;else if(i%5u==3u)key=0x100u+(i%0x80u);
            std::memcpy(reinterpret_cast<void*>(R065ObjectBase),native.data(),native.size());prepare(0x442f50u);guest_call.ecx=R065ObjectBase;Bytes(reinterpret_cast<void*>(S),8).put32(0,key);run_original32();if(guest_call.out_sp!=S+4u)throw std::runtime_error("r065 442f50 stack imbalance");
            object_store_state_pair_442f50(nb,key,table);compare_blob("object_store_state_pair_442f50_r065",reinterpret_cast<void*>(R065ObjectBase),native);
        }
        if(enabled("object_update_state_442fd0_r065")){
            auto native=r065_object_fixture(i);Bytes nb(native.data(),native.size());const auto table=r065_pair_table();
            nb.puti(0x518u,0);nb.put8(0x494u,0u);nb.put32(0x490u,0u);nb.put8(0x48cu,0u);nb.put32(0x488u,0u);nb.puti(0x484u,0);nb.put8(0x220u,std::uint8_t(i%32u));
            const unsigned mode=i%7u;
            if(mode==0u){nb.puti(0x518u,1);nb.put32(0x498u,r065_child(0u));}
            else if(mode==1u){nb.put8(0x494u,1u);nb.put32(0x490u,r065_child(1u));}
            else if(mode==2u){nb.put8(0x494u,1u);nb.put32(0x490u,0u);nb.put8(0x48cu,1u);nb.put32(0x488u,r065_child(2u));}
            else if(mode==3u){nb.put8(0x48cu,1u);nb.put32(0x488u,r065_child(3u));}
            else if(mode==4u){const auto count=1+std::int32_t(i%8u);nb.puti(0x484u,count);nb.put32(0x280u+std::size_t(count)*4u,r065_child(4u+unsigned(count)));}
            else if(mode==5u){nb.put8(0x494u,1u);nb.put32(0x490u,0u);const auto count=1+std::int32_t(i%6u);nb.puti(0x484u,count);nb.put32(0x280u+std::size_t(count)*4u,r065_child(10u+unsigned(count)));}
            std::memcpy(reinterpret_cast<void*>(R065ObjectBase),native.data(),native.size());reset_r065_trace();R065UpdateTrace nt{};auto sv=r065_update_services(nt);
            prepare(0x442fd0u);guest_call.ecx=R065ObjectBase;run();object_update_state_442fd0(nb,table,sv);compare_blob("object_update_state_442fd0_r065",reinterpret_cast<void*>(R065ObjectBase),native);auto gt=r065_guest_update_trace_blob(),ntb=r065_update_trace_blob(nt);compare_blob("object_update_state_trace_442fd0_r065",gt.data(),ntb);
        }
        if(enabled("object_release_flagged_handles_4430b0_r065")){
            auto native=r065_object_fixture(i);Bytes nb(native.data(),native.size());const unsigned mode=i%7u;
            const std::uint8_t flag_a=(mode==0u||mode==1u||mode==3u||mode==5u)?1u:0u;const std::uint8_t flag_b=(mode==0u||mode==2u||mode==4u||mode==5u)?1u:0u;
            const std::uint32_t h_a=flag_a&&mode!=3u?r065_child(0u):0u;const std::uint32_t h_b=flag_b&&mode!=4u?r065_child(1u):0u;
            nb.put8(0x494u,flag_a);nb.put32(0x490u,h_a);nb.put8(0x48cu,flag_b);nb.put32(0x488u,h_b);nb.put8(0x220u,std::uint8_t(4u+(i%8u)));nb.put32(0x924u,2u+(i%9u));
            const auto depth0=nb.u8(0x220u);const std::int8_t release_index=(i%4u)==0u?0:std::int8_t(1u+(i%8u));if(release_index>0){nb.put8(0x221u+std::uint8_t(release_index),0xa5u);nb.put8(0x241u+depth0,0x5au);}
            std::memcpy(reinterpret_cast<void*>(R065ObjectBase),native.data(),native.size());reset_r065_trace();reset_r058_trace(0u,0u,release_index);R065RuntimeTrace nt{};nt.release_index=release_index;auto sv=r065_runtime_services(nt);
            prepare(0x4430b0u);guest_call.ecx=R065ObjectBase;run();object_release_flagged_handles_4430b0(nb,sv);compare_blob("object_release_flagged_handles_4430b0_r065",reinterpret_cast<void*>(R065ObjectBase),native);
            const bool a_call=flag_a!=0u&&h_a!=0u,b_call=flag_b!=0u&&h_b!=0u;auto gt=r065_release_guest_blob(),ntb=r065_release_native_blob(nt,a_call,b_call);compare_blob("object_release_flagged_handles_trace_4430b0_r065",gt.data(),ntb);
        }
        if(only=="all"){set_r065_patches(false);set_r058_runtime_patches(false);}

        // r066: parent UI/resource state machine and local event-id dispatcher.
        // 0x465860/0x465970/0x4659F0 are explicit parent boundaries; the
        // already-closed UI leaves and 0x442EC0 execute transitively.
        if(only=="all"){set_r066_patches(true);set_r059_patches(true);}
        else if(r066_oracle_name(only))set_r059_patches(true);
        if(enabled("object_ui_state_update_443110_r066")){
            auto native=r066_object_fixture(i);Bytes nb(native.data(),native.size());
            const unsigned mode=i%8u;const std::uint32_t a=0x66000000u^(i*0x10203u),b=0x66100000u^(i*0x30405u);
            nb.put8(0xcf0u,mode==0u?0u:1u);nb.put8(0xcf1u,0u);nb.put32(0xce8u,0xffffffffu);nb.put32(0xcecu,0x6600b000u^(i*13u));
            r066_set_resource(nb,0xba8u,0xffffffffu,0);r066_set_resource(nb,0xc48u,0xffffffffu,0);
            std::int32_t status=0;
            if(mode==0u){r066_set_resource(nb,0xba8u,a,0);r066_set_resource(nb,0xc48u,b,0);}
            else if(mode==1u){nb.put32(0xce8u,a);r066_set_resource(nb,0xba8u,b,1);}
            else if(mode==2u){nb.put32(0xce8u,a);r066_set_resource(nb,0xba8u,b,0);r066_set_resource(nb,0xc48u,0x66200000u^(i*7u),0);}
            else if(mode==3u){nb.put8(0xcf1u,1u);nb.put32(0xce8u,a);}
            else if(mode==4u){/* idle dual-config path */}
            else if(mode==5u){r066_set_resource(nb,0xc48u,b,0);status=0;}
            else if(mode==6u){r066_set_resource(nb,0xba8u,a,0);status=1;}
            else {r066_set_resource(nb,0xc48u,b,0);status=1;}
            PcUiNotifyGlobals ng{};ng.x=std::int16_t(i);ng.y=std::int16_t(i*3u);ng.base_x=-7;ng.base_y=9;ng.color=0x12345678u;ng.mode=5u;ng.source_x=0x100u+(i&0xffu);ng.source_y=0x200u+((i*3u)&0xffu);ng.alternate=std::uint8_t(i&1u);
            const std::uint32_t v474=0x66474000u^(i*17u),v476=0x66476000u^(i*19u);
            r059_store_globals(ng);auto table=Bytes(reinterpret_cast<void*>(R059TableBase),R059TableSize);table.put32(0x474u*4u,v474);table.put32(0x476u*4u,v476);
            std::memcpy(reinterpret_cast<void*>(R066ObjectBase),native.data(),native.size());reset_r059_trace(status);reset_r066_trace();
            R059Trace nui{};nui.status=status;nui.lookup474=v474;nui.lookup476=v476;auto uis=r059_services(nui);R066OpenTrace no{};auto os=r066_open_services(no);
            prepare(0x443110u);guest_call.ecx=R066ObjectBase;run();object_ui_state_update_443110(nb,ng,uis,os);
            compare_blob("object_ui_state_update_443110_r066",reinterpret_cast<void*>(R066ObjectBase),native);
            auto ggt=r066_guest_trace_blob(),ngt=r066_native_trace_blob(no);compare_blob("object_ui_state_open_trace_443110_r066",ggt.data(),ngt);
            auto gut=r059_guest_trace_blob(),nut=r059_trace_blob(nui);compare_blob("object_ui_state_ui_trace_443110_r066",gut.data(),nut);
            auto gug=r059_guest_globals_blob(),nug=r059_globals_blob(ng);compare_blob("object_ui_state_globals_443110_r066",gug.data(),nug);
        }
        if(enabled("object_event_dispatch_4432b0_r066")){
            auto native=r066_object_fixture(i);Bytes nb(native.data(),native.size());const std::uint32_t states[]={0u,1u,2u,3u,4u,5u,6u,10u,11u,21u,24u,36u,37u,38u,39u,43u,44u,46u,47u,49u,55u,57u,58u,59u,60u,61u,0x53u};const auto state=states[i%(sizeof(states)/sizeof(states[0]))];
            nb.put32(0x20cu,i&3u);nb.puti(0x484u,2);const auto h=r064_child(70u+(i%16u));nb.put32(0x284u,h);const std::uint32_t prevs[]={1u,37u,55u,61u,0u,49u};*reinterpret_cast<std::uint32_t*>(std::uintptr_t(h)+8u)=prevs[i%(sizeof(prevs)/sizeof(prevs[0]))];
            std::memcpy(reinterpret_cast<void*>(R066ObjectBase),native.data(),native.size());R064Trace qt{};auto qs=r064_services(qt);prepare(0x4432b0u);guest_call.ecx=R066ObjectBase;Bytes(reinterpret_cast<void*>(S),8).put32(0,state);run_original32();if(guest_call.out_sp!=S+4u)throw std::runtime_error("r066 4432b0 stack imbalance");
            const auto nr=object_event_dispatch_4432b0(nb,state,qs);compare_u32("object_event_dispatch_4432b0_r066",guest_call.out_eax,nr);compare_blob("object_event_dispatch_object_4432b0_r066",reinterpret_cast<void*>(R066ObjectBase),native);
        }
        if(only=="all"){set_r066_patches(false);set_r059_patches(false);}
        else if(r066_oracle_name(only))set_r059_patches(false);

        // r067: callback-table dispatcher plus two normal-path allocation wrappers.
        if(only=="all")set_r067_patches(true);
        if(enabled("object_dispatch_callback_443420_r067")){
            auto native=r067_object_fixture(i);Bytes nb(native.data(),native.size());
            const std::int8_t depths[]={-3,-1,0,1,7,30,31,32,40,127};
            nb.put8(0x220u,std::uint8_t(depths[i%(sizeof(depths)/sizeof(depths[0]))]));
            const bool miss=(i%11u)==0u;const std::uint32_t key=miss?0xdead0000u^(i*17u):std::uint32_t(i%PcObjectEventCallbackCount);
            const auto ret=0x67000000u^(i*0x10203u);reset_r067_trace(ret);r067_fill_callback_table();
            std::memcpy(reinterpret_cast<void*>(R067ObjectBase),native.data(),native.size());
            auto ntbl_a=r067_native_callback_table();const PcObjectEventCallbackTable ntbl{ntbl_a.data(),ntbl_a.size()};
            const auto ptab=r065_pair_table();R067Trace nt{};nt.callback_ret=ret;auto sv=r067_services(nt);
            prepare(0x443420u);guest_call.ecx=R067ObjectBase;Bytes(reinterpret_cast<void*>(S),8).put32(0,key);run_original32();if(guest_call.out_sp!=S+4u)throw std::runtime_error("r067 443420 stack imbalance");
            const auto nr=object_dispatch_callback_443420(nb,key,ptab,ntbl,sv);
            compare_u32("object_dispatch_callback_443420_r067",guest_call.out_eax,nr);
            compare_blob("object_dispatch_callback_object_443420_r067",reinterpret_cast<void*>(R067ObjectBase),native);
            auto gt=r067_guest_trace_blob(),ntb=r067_trace_blob(nt);compare_blob("object_dispatch_callback_trace_443420_r067",gt.data(),ntb);
        }
        for(const auto& f:R067Factories){if(enabled(f.label)){
            const bool fail=(i%5u)==0u;const std::uint32_t alloc_ret=fail?0u:(0x67010000u^(i*0x101u));const std::uint32_t ctor_ret=0x67020000u^(i*0x303u);
            reset_r060_trace(alloc_ret,ctor_ret);R060Trace nt{};nt.alloc_ret=alloc_ret;nt.ctor_ret=ctor_ret;auto sv=r060_services(nt);
            prepare(f.start);run_original32();const auto nr=f.native(sv);compare_u32(f.label,guest_call.out_eax,nr);auto gt=r060_guest_trace_blob(),ntb=r060_trace_blob(nt);compare_blob(std::string(f.label)+"_trace",gt.data(),ntb);
        }}
        if(only=="all"){set_r067_patches(false);r067_restore_callback_table();}

        // r068: paired deleting-destructor wrappers and in-place bodies.
        if(only=="all")set_r068_patches(true);
        if(enabled("object_destroy_body_4435a0_r068")){
            auto native=r068_object_fixture(i);Bytes nb(native.data(),native.size());std::memcpy(reinterpret_cast<void*>(R068ObjectBase),native.data(),native.size());reset_r068_trace();R068Trace nt{};auto sv=r068_services(nt);
            prepare(0x4435a0u);guest_call.ecx=R068ObjectBase;run();object_destroy_body_4435a0(nb,sv);compare_blob("object_destroy_body_4435a0_r068",reinterpret_cast<void*>(R068ObjectBase),native);auto gt=r068_guest_trace_blob(),ntb=r068_trace_blob(nt);compare_blob("object_destroy_body_trace_4435a0_r068",gt.data(),ntb);
        }
        if(enabled("object_destroy_443580_r068")){
            auto native=r068_object_fixture(i);Bytes nb(native.data(),native.size());std::memcpy(reinterpret_cast<void*>(R068ObjectBase),native.data(),native.size());reset_r068_trace();R068Trace nt{};auto sv=r068_services(nt);const std::uint32_t flags=i&3u;
            prepare(0x443580u);guest_call.ecx=R068ObjectBase;Bytes(reinterpret_cast<void*>(S),8).put32(0,flags);run_original32();if(guest_call.out_sp!=S+4u)throw std::runtime_error("r068 443580 stack imbalance");const auto nr=object_destroy_443580(nb,R068ObjectBase,std::uint8_t(flags),sv);compare_u32("object_destroy_return_443580_r068",guest_call.out_eax,nr);compare_blob("object_destroy_443580_r068",reinterpret_cast<void*>(R068ObjectBase),native);auto gt=r068_guest_trace_blob(),ntb=r068_trace_blob(nt);compare_blob("object_destroy_trace_443580_r068",gt.data(),ntb);
        }
        if(enabled("object_destroy_body_443660_r068")){
            auto native=r068_object_fixture(i);Bytes nb(native.data(),native.size());std::memcpy(reinterpret_cast<void*>(R068ObjectBase),native.data(),native.size());reset_r068_trace();R068Trace nt{};auto sv=r068_services(nt);
            prepare(0x443660u);guest_call.ecx=R068ObjectBase;run();object_destroy_body_443660(nb,sv);compare_blob("object_destroy_body_443660_r068",reinterpret_cast<void*>(R068ObjectBase),native);auto gt=r068_guest_trace_blob(),ntb=r068_trace_blob(nt);compare_blob("object_destroy_body_trace_443660_r068",gt.data(),ntb);
        }
        if(enabled("object_destroy_443640_r068")){
            auto native=r068_object_fixture(i);Bytes nb(native.data(),native.size());std::memcpy(reinterpret_cast<void*>(R068ObjectBase),native.data(),native.size());reset_r068_trace();R068Trace nt{};auto sv=r068_services(nt);const std::uint32_t flags=(i*3u)&3u;
            prepare(0x443640u);guest_call.ecx=R068ObjectBase;Bytes(reinterpret_cast<void*>(S),8).put32(0,flags);run_original32();if(guest_call.out_sp!=S+4u)throw std::runtime_error("r068 443640 stack imbalance");const auto nr=object_destroy_443640(nb,R068ObjectBase,std::uint8_t(flags),sv);compare_u32("object_destroy_return_443640_r068",guest_call.out_eax,nr);compare_blob("object_destroy_443640_r068",reinterpret_cast<void*>(R068ObjectBase),native);auto gt=r068_guest_trace_blob(),ntb=r068_trace_blob(nt);compare_blob("object_destroy_trace_443640_r068",gt.data(),ntb);
        }
        if(only=="all")set_r068_patches(false);

        // r069: initialize the 83-slot callback table and owning runtime object.
        if(enabled("object_runtime_init_4436c0_r069")){
            auto native=r069_object_fixture(i);Bytes nb(native.data(),native.size());
            auto ntbl=r069_table_fixture(i);auto gtbl=ntbl;const std::uint8_t mode=std::uint8_t((i*13u)&0xffu);
            std::array<std::uint8_t,R069TableBytes> saved_table{};std::memcpy(saved_table.data(),reinterpret_cast<void*>(R069TableBase),saved_table.size());
            const auto saved_mode=*reinterpret_cast<std::uint8_t*>(0x006319a1u);
            std::memcpy(reinterpret_cast<void*>(R069ObjectBase),native.data(),native.size());
            std::memcpy(reinterpret_cast<void*>(R069TableBase),gtbl.data(),R069TableBytes);*reinterpret_cast<std::uint8_t*>(0x006319a1u)=mode;
            reset_r069_trace();R069Trace nt{};PcUiNotifyServices ui{};PcObjectRuntimeInitServices sv{&nt,r069_native_embedded,ui};
            if(only=="all")set_r069_patch(true);
            prepare(0x4436c0u);guest_call.ecx=R069ObjectBase;run();
            const auto nr=object_runtime_init_4436c0(nb,ntbl,mode,sv);
            compare_u32("object_runtime_init_return_4436c0_r069",guest_call.out_eax,nr);
            compare_blob("object_runtime_init_object_4436c0_r069",reinterpret_cast<void*>(R069ObjectBase),native);
            std::array<std::uint8_t,R069TableBytes> ntbl_bytes{};std::memcpy(ntbl_bytes.data(),ntbl.data(),ntbl_bytes.size());
            compare_blob("object_runtime_init_table_4436c0_r069",reinterpret_cast<void*>(R069TableBase),ntbl_bytes);
            auto gt=r069_guest_trace_blob(),ntb=r069_trace_blob(nt);compare_blob("object_runtime_init_trace_4436c0_r069",gt.data(),ntb);
            if(only=="all")set_r069_patch(false);
            std::memcpy(reinterpret_cast<void*>(R069TableBase),saved_table.data(),saved_table.size());*reinterpret_cast<std::uint8_t*>(0x006319a1u)=saved_mode;
        }

        // r070: full runtime teardown/reset, with protected-entry continuation and
        // every open external boundary traced deterministically.
        if(enabled("object_runtime_teardown_443c30_r070")){
            auto native=r070_object_fixture(i);Bytes nb(native.data(),native.size());std::memcpy(reinterpret_cast<void*>(R070ObjectBase),native.data(),native.size());reset_r070_trace();R070Trace nt{};auto sv=r070_services(nt);
            if(only=="all")set_r070_patches(true);
            prepare(0x443c30u);guest_call.ecx=R070ObjectBase;run();const auto nr=object_runtime_teardown_443c30(nb,sv);
            compare_u32("object_runtime_teardown_return_443c30_r070",guest_call.out_eax,nr);
            compare_blob("object_runtime_teardown_object_443c30_r070",reinterpret_cast<void*>(R070ObjectBase),native);
            auto gt=r070_guest_trace_blob(),ntb=r070_trace_blob(nt);compare_blob("object_runtime_teardown_trace_443c30_r070",gt.data(),ntb);
            if(only=="all")set_r070_patches(false);
        }


        // r071: second callback dispatcher, handle-open wrapper and state-code table.
        if(enabled("object_dispatch_callback_443eb0_r071")){
            auto native=r071_object_fixture(i);Bytes nb(native.data(),native.size());std::memcpy(reinterpret_cast<void*>(R071ObjectBase),native.data(),native.size());
            const auto ntbl=r071_native_callback_table();PcObjectEventCallbackTable tv{ntbl.data(),ntbl.size()};const auto& pt=pc_object_state_pair_table_r065();PcObjectStatePairTable pv{pt.data(),pt.size()};
            const std::uint32_t key=(i%17u)==0u?0xdeadbeefu:(i%PcObjectEventCallbackCount);const std::uint32_t ret=0x71000000u^(i*0x101u);reset_r071_trace(ret,1u,0);R071Trace nt{};nt.callback_ret=ret;nt.ready_ret=1u;
            PcObjectEventModeInputs in{std::uint8_t((i%5u)==0u),((i%7u)==0u)?4u:3u,((i%11u)==0u)?0x10u:3u,((i%13u)==0u)?0x10u:2u};auto ds=r071_dispatch_services(nt);
            const auto save248=*reinterpret_cast<std::uint8_t*>(0x780248u);const auto save258=*reinterpret_cast<std::uint32_t*>(0x780258u);const auto save25c=*reinterpret_cast<std::uint32_t*>(0x78025cu);const auto save26c=*reinterpret_cast<std::uint32_t*>(0x78026cu);
            *reinterpret_cast<std::uint8_t*>(0x780248u)=in.game_flag_780248;*reinterpret_cast<std::uint32_t*>(0x780258u)=in.route_state_780258;*reinterpret_cast<std::uint32_t*>(0x78025cu)=in.fallback_state_78025c;*reinterpret_cast<std::uint32_t*>(0x78026cu)=in.game_mode_78026c;
            r071_fill_callback_table();if(only=="all")set_r071_patches(true);prepare(0x443eb0u);guest_call.ecx=R071ObjectBase;Bytes(reinterpret_cast<void*>(S),8).put32(0,key);run_original32();if(guest_call.out_sp!=S+4u)throw std::runtime_error("r071 443eb0 stack imbalance");const auto nr=object_dispatch_callback_443eb0(nb,key,pv,tv,in,ds);
            compare_u32("object_dispatch_callback_return_443eb0_r071",guest_call.out_eax,nr);compare_blob("object_dispatch_callback_object_443eb0_r071",reinterpret_cast<void*>(R071ObjectBase),native);auto gt=r071_guest_trace_blob(),ntb=r071_trace_blob(nt);compare_blob("object_dispatch_callback_trace_443eb0_r071",gt.data(),ntb);
            if(only=="all")set_r071_patches(false);r071_restore_callback_table();*reinterpret_cast<std::uint8_t*>(0x780248u)=save248;*reinterpret_cast<std::uint32_t*>(0x780258u)=save258;*reinterpret_cast<std::uint32_t*>(0x78025cu)=save25c;*reinterpret_cast<std::uint32_t*>(0x78026cu)=save26c;
        }
        if(enabled("object_open_callback_443fa0_r071")){
            auto native=r071_object_fixture(i);Bytes nb(native.data(),native.size());const auto ntbl=r071_native_callback_table();PcObjectEventCallbackTable tv{ntbl.data(),ntbl.size()};const auto& pt=pc_object_state_pair_table_r065();PcObjectStatePairTable pv{pt.data(),pt.size()};
            const std::uint32_t key=(i%19u)==0u?0xdeadbeefu:(i%PcObjectEventCallbackCount);const std::uint32_t handle=r071_handle(i);const std::uint32_t ready=(i&1u);const std::int8_t relidx=0;Bytes nx(native.data(),native.size());if((i%4u)==0u){nx.put8(0x48cu,1u);nx.put32(0x488u,r071_handle(i+1u));}if((i%6u)==0u){nx.put8(0x494u,1u);nx.put32(0x490u,r071_handle(i+2u));}
            std::memcpy(reinterpret_cast<void*>(R071ObjectBase),native.data(),native.size());reset_r071_trace(handle,ready,relidx);R071Trace nt{};nt.callback_ret=handle;nt.ready_ret=ready;nt.release_index=std::uint32_t(std::uint8_t(relidx));
            PcObjectEventModeInputs in{0u,3u,3u,2u};auto ds=r071_dispatch_services(nt);auto rs=r071_runtime_services(nt);auto os=r071_open_services(nt);
            const auto save248=*reinterpret_cast<std::uint8_t*>(0x780248u);const auto save258=*reinterpret_cast<std::uint32_t*>(0x780258u);const auto save25c=*reinterpret_cast<std::uint32_t*>(0x78025cu);const auto save26c=*reinterpret_cast<std::uint32_t*>(0x78026cu);
            *reinterpret_cast<std::uint8_t*>(0x780248u)=in.game_flag_780248;*reinterpret_cast<std::uint32_t*>(0x780258u)=in.route_state_780258;*reinterpret_cast<std::uint32_t*>(0x78025cu)=in.fallback_state_78025c;*reinterpret_cast<std::uint32_t*>(0x78026cu)=in.game_mode_78026c;
            r071_fill_callback_table();if(only=="all")set_r071_patches(true);prepare(0x443fa0u);guest_call.ecx=R071ObjectBase;Bytes(reinterpret_cast<void*>(S),8).put32(0,key);run_original32();if(guest_call.out_sp!=S+4u)throw std::runtime_error("r071 443fa0 stack imbalance");object_open_callback_443fa0(nb,key,pv,tv,in,ds,rs,os);
            compare_blob("object_open_callback_object_443fa0_r071",reinterpret_cast<void*>(R071ObjectBase),native);auto gt=r071_guest_trace_blob(),ntb=r071_trace_blob(nt);compare_blob("object_open_callback_trace_443fa0_r071",gt.data(),ntb);
            if(only=="all")set_r071_patches(false);r071_restore_callback_table();*reinterpret_cast<std::uint8_t*>(0x780248u)=save248;*reinterpret_cast<std::uint32_t*>(0x780258u)=save258;*reinterpret_cast<std::uint32_t*>(0x78025cu)=save25c;*reinterpret_cast<std::uint32_t*>(0x78026cu)=save26c;
        }
        if(enabled("object_state_code_443ff0_r071")){
            auto native=r071_object_fixture(i);Bytes nb(native.data(),native.size());std::memcpy(reinterpret_cast<void*>(R071ObjectBase),native.data(),native.size());
            std::array<std::uint8_t,0x600> rel{};std::mt19937 rg(0x443ff071u^(i*0x9e3779b9u));for(auto& b:rel)b=std::uint8_t(rg());Bytes rb(rel.data(),rel.size());rb.put32(0x484u,2u);const auto h=r071_handle(i+3u);rb.put32(0x284u,h);const std::uint32_t prev=(i%4u)==0u?21u:48u;*reinterpret_cast<std::uint32_t*>(std::uintptr_t(h)+8u)=prev;std::memcpy(reinterpret_cast<void*>(R071RelatedBase),rel.data(),rel.size());
            const std::uint32_t state=(i%7u)==0u?46u:(i%65u);reset_r071_trace(0u,0u,0);R071Trace nt{};auto ss=r071_state_services(nt);if(only=="all")set_r071_patches(true);prepare(0x443ff0u);guest_call.ecx=R071ObjectBase;Bytes(reinterpret_cast<void*>(S),8).put32(0,state);run_original32();if(guest_call.out_sp!=S+4u)throw std::runtime_error("r071 443ff0 stack imbalance");const auto nr=object_state_code_443ff0(nb,state,rb,ss);compare_u32("object_state_code_443ff0_r071",guest_call.out_eax,nr);compare_blob("object_state_code_object_443ff0_r071",reinterpret_cast<void*>(R071ObjectBase),native);if(only=="all")set_r071_patches(false);
        }

        // r072: active runtime list insertion/reopen and snapshot commit.
        if(enabled("object_insert_callback_4440f0_r072")){
            auto native=r072_object_fixture(i);Bytes nb(native.data(),native.size());const std::uint32_t key=i%PcObjectEventCallbackCount;
            const unsigned variant=i%8u;std::int32_t count=(variant==0u)?0:2;Bytes nx(native.data(),native.size());nx.puti(0x518u,count);
            const auto h0=r072_handle(0u),h1=r072_handle(1u),created=r072_handle(14u);r072_setup_handle(0u,key+0x100u,0xffffffffu,0u,1u,(key+1u)%PcObjectEventCallbackCount);r072_setup_handle(1u,key+0x101u,0xffffffffu,0u,1u,(key+2u)%PcObjectEventCallbackCount);
            if(count>0){nx.put32(0x498u,h0);nx.put32(0x49cu,h1);}
            if(variant==1u)*reinterpret_cast<std::uint32_t*>(std::uintptr_t(h1)+0x0cu)=0u;
            if(variant==2u)*reinterpret_cast<std::uint32_t*>(std::uintptr_t(h1)+0x0cu)=1u;
            if(variant==3u)*reinterpret_cast<std::uint32_t*>(std::uintptr_t(h1)+0x0cu)=2u;
            if(variant==4u)*reinterpret_cast<std::uint32_t*>(std::uintptr_t(h1)+0x0cu)=3u;
            if(variant==5u)*reinterpret_cast<std::uint32_t*>(std::uintptr_t(h1)+8u)=key;
            if(variant==6u)*reinterpret_cast<std::uint32_t*>(std::uintptr_t(h1)+0x10u)=1u;
            if(variant==7u)*reinterpret_cast<std::uint32_t*>(std::uintptr_t(h1)+0x0cu)=0xffffffffu;
            const std::uint32_t created_ret=(i%17u)==0u?0u:created;const std::uint32_t ready=(i%5u)==0u?0u:1u;r072_setup_handle(14u,0x70u,0u,0u,ready,key);
            std::memcpy(reinterpret_cast<void*>(R072ObjectBase),native.data(),native.size());reset_r067_trace(created_ret);R067Trace dt{};dt.callback_ret=created_ret;auto ds=r067_services(dt);auto ct=r067_native_callback_table();PcObjectEventCallbackTable cv{ct.data(),ct.size()};const auto pt=r065_pair_table();const auto ls=r072_list_services();
            r067_fill_callback_table();set_r067_patches(true);set_r072_patches(true);prepare(0x4440f0u);guest_call.ecx=R072ObjectBase;Bytes(reinterpret_cast<void*>(S),8).put32(0,key);run_original32();if(guest_call.out_sp!=S+4u)throw std::runtime_error("r072 4440f0 stack imbalance");const auto nr=object_insert_callback_4440f0(nb,key,pt,cv,ds,ls);compare_u32("object_insert_callback_return_4440f0_r072",guest_call.out_eax,nr);compare_blob("object_insert_callback_object_4440f0_r072",reinterpret_cast<void*>(R072ObjectBase),native);set_r072_patches(false);set_r067_patches(false);r067_restore_callback_table();
        }
        if(enabled("object_reopen_callback_4442a0_r072")){
            auto native=r072_object_fixture(i);Bytes nx(native.data(),native.size());const std::uint32_t mode=i%5u;const std::uint32_t key=(i*7u)%PcObjectEventCallbackCount;const auto source=r072_handle(2u),tail=r072_handle(3u),created=r072_handle(15u);r072_setup_handle(2u,0x72u,0u,0u,1u,key);r072_setup_handle(3u,0x73u,0u,0u,1u,(key+1u)%PcObjectEventCallbackCount);const std::uint32_t ready=(i%4u)==0u?0u:1u;r072_setup_handle(15u,0x74u,0u,0u,ready,key);
            nx.put32(0x484u,1u);nx.put32(0x284u,source);nx.put32(0x488u,source);nx.put8(0x48cu,0u);nx.put32(0x490u,source);nx.put8(0x494u,0u);nx.put32(0x518u,2u);nx.put32(0x498u,source);nx.put32(0x49cu,tail);nx.put32(0x218u,1u);nx.put8(0x220u,8u);nx.put32(0x51cu+0x408u,10u);
            std::memcpy(reinterpret_cast<void*>(R072ObjectBase),native.data(),native.size());const std::int8_t relidx=(i%3u)==0u?1:0;reset_r071_trace(created,ready,relidx);R071Trace nt{};nt.callback_ret=created;nt.ready_ret=ready;nt.release_index=std::uint32_t(std::uint8_t(relidx));const auto ds=r071_dispatch_services(nt);const auto rs=r071_runtime_services(nt);const auto os=r071_open_services(nt);auto ct=r071_native_callback_table();PcObjectEventCallbackTable cv{ct.data(),ct.size()};const auto pt=r065_pair_table();PcObjectEventModeInputs in{0u,3u,3u,2u};const auto ls=r072_list_services();
            r071_fill_callback_table();set_r071_patches(true);set_r072_patches(true);prepare(0x4442a0u);guest_call.ecx=R072ObjectBase;Bytes(reinterpret_cast<void*>(S),12).put32(0,mode);Bytes(reinterpret_cast<void*>(S),12).put32(4,0xdeadbeefu);run_original32();if(guest_call.out_sp!=S+8u)throw std::runtime_error("r072 4442a0 stack imbalance");const auto nr=object_reopen_callback_4442a0(nx,mode,0xdeadbeefu,pt,cv,in,ds,rs,os,ls);compare_u32("object_reopen_callback_return_4442a0_r072",guest_call.out_eax,nr);compare_blob("object_reopen_callback_object_4442a0_r072",reinterpret_cast<void*>(R072ObjectBase),native);set_r072_patches(false);set_r071_patches(false);r071_restore_callback_table();
        }
        if(enabled("object_runtime_commit_444350_r072")){
            auto native=r072_object_fixture(i);Bytes nx(native.data(),native.size());const std::int32_t count=std::int32_t(i%5u);nx.puti(0x484u,count);nx.put32(0x0004u,i%4u);nx.put8(0x220u,12u);nx.put32(0x51cu+0x408u,15u);
            static constexpr std::uint32_t states[5]={4u,10u,21u,5u,17u};for(std::int32_t j=0;j<count;++j){r072_setup_handle(unsigned(4+j),states[j],0u,0u,1u,unsigned(states[j]%PcObjectEventCallbackCount));nx.put32(0x284u+std::size_t(j)*4u,r072_handle(unsigned(4+j)));}
            if((i%3u)==0u){r072_setup_handle(10u,31u,0u,0u,1u,31u);nx.put8(0x48cu,1u);nx.put32(0x488u,r072_handle(10u));}else{nx.put8(0x48cu,0u);nx.put32(0x488u,0u);}if((i%4u)==0u){r072_setup_handle(11u,32u,0u,0u,1u,32u);nx.put8(0x494u,1u);nx.put32(0x490u,r072_handle(11u));}else{nx.put8(0x494u,0u);nx.put32(0x490u,0u);}
            std::memcpy(reinterpret_cast<void*>(R072ObjectBase),native.data(),native.size());std::array<std::uint8_t,0x218u> saved{};std::memcpy(saved.data(),reinterpret_cast<void*>(R072SnapshotBase),saved.size());std::memset(reinterpret_cast<void*>(R072SnapshotBase),0xa5,0x210u);const std::uint8_t initial_active=std::uint8_t(i&1u);*reinterpret_cast<std::uint8_t*>(R072SnapshotBase+0x214u)=initial_active;
            const std::int8_t relidx=1;reset_r071_trace(0u,0u,relidx);R071Trace rt{};rt.release_index=std::uint32_t(std::uint8_t(relidx));const auto rs=r071_runtime_services(rt);PcObjectRuntimeSnapshot444350 snap{};snap.active=initial_active;const auto ss=r072_state_services();const auto cs=r072_commit_services();
            set_r071_patches(true);set_r072_patches(true);prepare(0x444350u);guest_call.ecx=R072ObjectBase;run_original32();if(guest_call.out_sp!=S)throw std::runtime_error("r072 444350 stack imbalance");object_runtime_commit_444350(nx,snap,ss,rs,cs);compare_blob("object_runtime_commit_object_444350_r072",reinterpret_cast<void*>(R072ObjectBase),native);compare_blob("object_runtime_commit_snapshot_444350_r072",reinterpret_cast<void*>(R072SnapshotBase),snap.bytes);compare_u32("object_runtime_commit_active_444350_r072",*reinterpret_cast<std::uint8_t*>(R072SnapshotBase+0x214u),snap.active);set_r072_patches(false);set_r071_patches(false);std::memcpy(reinterpret_cast<void*>(R072SnapshotBase),saved.data(),saved.size());
        }
        // r073: close the runtime-prepare chain exposed by r072 plus the two
        // callback-object primitives. Only 495930/4958A0 are patched dynamic
        // table services; all global setters/copies execute from the original.
        if(enabled("runtime_primary_mode_4eea70_r073")){
            const auto sel=i%6u;auto ns=r073_initial_state(i);r073_store_guest_state(ns);prepare(0x4eea70u);Bytes(reinterpret_cast<void*>(S),8).put32(0,sel);run();const auto nr=runtime_primary_mode_4eea70(sel,ns);compare_u32("runtime_primary_mode_return_4eea70_r073",guest_call.out_eax&0xffu,nr?1u:0u);auto gs=r073_state_blob(r073_guest_state()),nb=r073_state_blob(ns);compare_blob("runtime_primary_mode_state_4eea70_r073",gs.data(),nb);r073_store_guest_state(r073_saved_state);
        }
        if(enabled("runtime_route_mode_4eead0_r073")){
            const auto sel=i%9u;auto ns=r073_initial_state(i+13u);r073_store_guest_state(ns);prepare(0x4eead0u);Bytes(reinterpret_cast<void*>(S),8).put32(0,sel);run();const auto nr=runtime_route_mode_4eead0(sel,ns);compare_u32("runtime_route_mode_return_4eead0_r073",guest_call.out_eax&0xffu,nr?1u:0u);auto gs=r073_state_blob(r073_guest_state()),nb=r073_state_blob(ns);compare_blob("runtime_route_mode_state_4eead0_r073",gs.data(),nb);r073_store_guest_state(r073_saved_state);
        }
        if(enabled("runtime_route_config_4eeb50_r073")){
            const auto sel=i%9u,arg2=(i*11u+3u)&0x3fu,arg3=(i*17u+5u)&0x3fu,count=i%9u;auto ns=r073_initial_state(i+29u);r073_store_guest_state(ns);reset_r073_trace(count);R073Trace nt{};nt.count_ret=count;auto sv=r073_services(nt);prepare(0x4eeb50u);auto st=Bytes(reinterpret_cast<void*>(S),16);st.put32(0,sel);st.put32(4,arg2);st.put32(8,arg3);run();const auto nr=runtime_route_config_4eeb50(sel,arg2,arg3,ns,sv);compare_u32("runtime_route_config_return_4eeb50_r073",guest_call.out_eax&0xffu,nr?1u:0u);auto gs=r073_state_blob(r073_guest_state()),nb=r073_state_blob(ns);compare_blob("runtime_route_config_state_4eeb50_r073",gs.data(),nb);auto gt=r073_trace_blob(r073_guest_trace()),ntr=r073_trace_blob(nt);compare_blob("runtime_route_config_trace_4eeb50_r073",gt.data(),ntr);r073_store_guest_state(r073_saved_state);
        }
        if(enabled("runtime_prepare_4eec80_r073")){
            std::array<std::uint8_t,0x300u> obj{};std::mt19937 rg(0x4eec8073u^(i*2654435761u));for(auto& b:obj)b=std::uint8_t(rg());Bytes no(obj.data(),obj.size());const auto packed=r073_packed(i);no.put32(0x208u,packed);no.put32(0x20cu,(i*0x10203u)^1u);std::memcpy(reinterpret_cast<void*>(R073ObjectBase),obj.data(),obj.size());auto ns=r073_initial_state(i+47u);r073_store_guest_state(ns);const auto count=i%11u;reset_r073_trace(count);R073Trace nt{};nt.count_ret=count;auto sv=r073_services(nt);prepare(0x4eec80u);Bytes(reinterpret_cast<void*>(S),8).put32(0,R073ObjectBase);run();runtime_prepare_4eec80(no,ns,sv);auto gs=r073_state_blob(r073_guest_state()),nb=r073_state_blob(ns);compare_blob("runtime_prepare_state_4eec80_r073",gs.data(),nb);auto gt=r073_trace_blob(r073_guest_trace()),ntr=r073_trace_blob(nt);compare_blob("runtime_prepare_trace_4eec80_r073",gt.data(),ntr);compare_blob("runtime_prepare_object_4eec80_r073",reinterpret_cast<void*>(R073ObjectBase),obj);r073_store_guest_state(r073_saved_state);
        }
        if(enabled("callback_key_48f4e0_r073")){
            std::array<std::uint8_t,0x40u> cb{};std::mt19937 rg(0x48f4e073u^(i*2654435761u));for(auto& b:cb)b=std::uint8_t(rg());Bytes nb(cb.data(),cb.size());const auto key=0x73000000u^(i*0x10007u);nb.put32(4u,key);std::memcpy(reinterpret_cast<void*>(R073CallbackBase),cb.data(),cb.size());prepare(0x48f4e0u);guest_call.ecx=R073CallbackBase;run();compare_u32("callback_key_48f4e0_r073",guest_call.out_eax,callback_key_48f4e0(nb));compare_blob("callback_key_object_48f4e0_r073",reinterpret_cast<void*>(R073CallbackBase),cb);
        }
        if(enabled("callback_append_48d870_r073")){
            std::array<std::uint8_t,0x600u> cb{};std::mt19937 rg(0x48d87073u^(i*2654435761u));for(auto& b:cb)b=std::uint8_t(rg());Bytes nb(cb.data(),cb.size());const auto count=i%8u,handle=(i%7u)==0u?0u:(0x73100000u^(i*0x101u));nb.put32(0x4d0u,count);std::memcpy(reinterpret_cast<void*>(R073CallbackBase),cb.data(),cb.size());prepare(0x48d870u);guest_call.ecx=R073CallbackBase;Bytes(reinterpret_cast<void*>(S),8).put32(0,handle);run_original32();if(guest_call.out_sp!=S+4u)throw std::runtime_error("r073 48d870 stack imbalance");const auto nr=callback_append_48d870(nb,handle);compare_u32("callback_append_return_48d870_r073",guest_call.out_eax&0xffu,nr?1u:0u);compare_blob("callback_append_object_48d870_r073",reinterpret_cast<void*>(R073CallbackBase),cb);
        }
        // r074: native category services, scalar runtime leaves, token resolver
        // integration and the parent runtime gate on the actual 443FA0 child path.
        if(enabled("runtime_select_entry_4958a0_r074")){
            constexpr unsigned K=12;std::array<std::uint32_t,K> keys{};for(unsigned k=0;k<K;++k)keys[k]=0x74000000u+(i*0x101u)+k*0x111u;
            for(unsigned k=0;k<K;++k){*reinterpret_cast<std::uint32_t*>(R074CategoryBase+k*8u)=keys[k];*reinterpret_cast<std::uint32_t*>(R074CategoryBase+k*8u+4u)=0xfeed0000u+k;}
            const auto cat=i%K,ind=(i*13u+7u)%97u;const auto save_tbl=*reinterpret_cast<std::uint32_t*>(0x836370u),save_key=*reinterpret_cast<std::uint32_t*>(0x67e6a4u),save_idx=*reinterpret_cast<std::uint32_t*>(0x67e6a8u);
            *reinterpret_cast<std::uint32_t*>(0x836370u)=R074CategoryBase;*reinterpret_cast<std::uint32_t*>(0x67e6a4u)=0xaaaaaaaa;*reinterpret_cast<std::uint32_t*>(0x67e6a8u)=0xbbbbbbbb;
            prepare(0x4958a0u);auto st=Bytes(reinterpret_cast<void*>(S),12);st.put32(0,cat);st.put32(4,ind);run();PcRuntimeCategoryState ns{3u,keys.data(),keys.size(),nullptr,0u,0xaaaaaaaau,0xbbbbbbbbu};runtime_select_entry_4958a0(ns,cat,ind);
            compare_u32("runtime_select_entry_key_4958a0_r074",*reinterpret_cast<std::uint32_t*>(0x67e6a4u),ns.selected_key_67e6a4);compare_u32("runtime_select_entry_index_4958a0_r074",*reinterpret_cast<std::uint32_t*>(0x67e6a8u),ns.selected_index_67e6a8);
            *reinterpret_cast<std::uint32_t*>(0x836370u)=save_tbl;*reinterpret_cast<std::uint32_t*>(0x67e6a4u)=save_key;*reinterpret_cast<std::uint32_t*>(0x67e6a8u)=save_idx;
        }
        if(enabled("runtime_count_entries_495930_r074")){
            constexpr unsigned K=9,R=15;std::array<std::uint32_t,K> keys{};std::array<PcRuntimeCategoryRecord495930,R> rec{};for(unsigned k=0;k<K;++k)keys[k]=0x74100000u+k*0x101u;
            for(unsigned k=0;k<K;++k){*reinterpret_cast<std::uint32_t*>(R074CategoryBase+k*8u)=keys[k];*reinterpret_cast<std::uint32_t*>(R074CategoryBase+k*8u+4u)=k;}
            for(unsigned k=0;k<R;++k){rec[k].key=keys[(k*3u+i)%K];std::memset(reinterpret_cast<void*>(R074RecordsBase+k*0x44u),0x5a,0x44u);*reinterpret_cast<std::uint32_t*>(R074RecordsBase+k*0x44u)=rec[k].key;}
            const auto cat=i%K,mode=(i%5u)==0u?2u:3u;const auto save_tbl=*reinterpret_cast<std::uint32_t*>(0x836370u),save_mode=*reinterpret_cast<std::uint32_t*>(0x836358u);*reinterpret_cast<std::uint32_t*>(0x836370u)=R074CategoryBase;*reinterpret_cast<std::uint32_t*>(0x836358u)=mode;reset_r074_state(R);
            prepare(0x495930u);Bytes(reinterpret_cast<void*>(S),8).put32(0,cat);run();PcRuntimeCategoryState ns{mode,keys.data(),keys.size(),rec.data(),rec.size(),0u,0u};compare_u32("runtime_count_entries_495930_r074",guest_call.out_eax,runtime_count_entries_495930(ns,cat));
            *reinterpret_cast<std::uint32_t*>(0x836370u)=save_tbl;*reinterpret_cast<std::uint32_t*>(0x836358u)=save_mode;
        }
        if(enabled("runtime_game_flag_43f870_r074")){
            const auto save=*reinterpret_cast<std::uint8_t*>(0x780260u),v=std::uint8_t(i*37u+11u);prepare(0x43f870u);Bytes(reinterpret_cast<void*>(S),8).put32(0,v);run();PcRuntimePrepareState ns{};runtime_game_flag_43f870(ns,v);compare_u32("runtime_game_flag_43f870_r074",*reinterpret_cast<std::uint8_t*>(0x780260u),ns.game_flag_780260);*reinterpret_cast<std::uint8_t*>(0x780260u)=save;
        }
        if(enabled("runtime_selection_disable_4957e0_r074")){
            const auto save=*reinterpret_cast<std::uint8_t*>(0x836374u);*reinterpret_cast<std::uint8_t*>(0x836374u)=std::uint8_t((i&1u)+1u);prepare(0x4957e0u);run();PcRuntimePrepareState ns{};ns.selection_active_836374=1u;runtime_selection_disable_4957e0(ns);compare_u32("runtime_selection_disable_4957e0_r074",*reinterpret_cast<std::uint8_t*>(0x836374u),ns.selection_active_836374);*reinterpret_cast<std::uint8_t*>(0x836374u)=save;
        }
        if(enabled("runtime_external_block_4872e0_r074")){
            const auto save=*reinterpret_cast<std::uint32_t*>(0x82e7d8u),v=0x74200000u^(i*0x10101u);*reinterpret_cast<std::uint32_t*>(0x82e7d8u)=v;prepare(0x4872e0u);run();compare_u32("runtime_external_block_4872e0_r074",guest_call.out_eax,runtime_external_block_4872e0(v));*reinterpret_cast<std::uint32_t*>(0x82e7d8u)=save;
        }
        if(enabled("runtime_system_handle_active_4999c0_r074")){
            const auto save=*reinterpret_cast<std::uint32_t*>(0x67f614u),v=(i%3u)==0u?0xffffffffu:(0x74300000u^i);*reinterpret_cast<std::uint32_t*>(0x67f614u)=v;prepare(0x4999c0u);run();compare_u32("runtime_system_handle_active_4999c0_r074",guest_call.out_eax,runtime_system_handle_active_4999c0(v)?1u:0u);*reinterpret_cast<std::uint32_t*>(0x67f614u)=save;
        }
        if(enabled("runtime_menu_state_450240_r074")){
            const auto save=*reinterpret_cast<std::uint32_t*>(0x7d38f0u),v=(i*7u)%11u;*reinterpret_cast<std::uint32_t*>(0x7d38f0u)=v;prepare(0x450240u);run();compare_u32("runtime_menu_state_450240_r074",guest_call.out_eax,runtime_menu_state_450240(v));*reinterpret_cast<std::uint32_t*>(0x7d38f0u)=save;
        }
        if(enabled("runtime_current_player_47f110_r074")){
            prepare(0x47f110u);run();compare_u32("runtime_current_player_47f110_r074",guest_call.out_eax,std::uint32_t(runtime_current_player_47f110()));
        }
        if(enabled("runtime_feature_mask_4536f0_r074")){
            const auto save=*reinterpret_cast<std::uint32_t*>(0x7d6778u),mask=0x74400000u^(i*0x11011u),req=1u<<(i%16u);*reinterpret_cast<std::uint32_t*>(0x7d6778u)=mask;prepare(0x4536f0u);Bytes(reinterpret_cast<void*>(S),8).put32(0,req);run();compare_u32("runtime_feature_mask_4536f0_r074",guest_call.out_eax,runtime_feature_mask_4536f0(mask,req));*reinterpret_cast<std::uint32_t*>(0x7d6778u)=save;
        }
        if(enabled("object_runtime_gate_444470_r074")){
            auto native=r074_object_fixture(i);auto current=r074_object_fixture(i+101u);Bytes nb(native.data(),native.size()),nc(current.data(),current.size());
            const auto variant=i%10u;PcRuntimeGate444470Inputs gi{0x10u,0u,0xffffffffu,0u,1u};
            const auto current_h=r071_handle(i+1u),own_h=r071_handle(i+2u),created=r071_handle(i+3u);for(unsigned k=0;k<16u;++k){auto* h=reinterpret_cast<std::uint8_t*>(std::uintptr_t(r071_handle(k)));*reinterpret_cast<std::uint32_t*>(h)=R071VtableBase;*reinterpret_cast<std::uint32_t*>(h+8u)=0x10u;}
            if(variant==1u)gi.game_mode_78026c=3u;else if(variant==2u)gi.external_block_82e7d8=1u;else if(variant==3u){nc.put8(0x48cu,1u);nc.put32(0x488u,current_h);*reinterpret_cast<std::uint32_t*>(std::uintptr_t(current_h)+8u)=0x1bu;}else if(variant==4u)nb.put32(0x518u,1u);else if(variant==5u)gi.system_handle_67f614=0x1234u;else if(variant==6u)gi.menu_state_7d38f0=7u;else if(variant==7u)gi.player0_feature_mask=0u;else if(variant==8u){nb.put32(0x488u,own_h);*reinterpret_cast<std::uint32_t*>(std::uintptr_t(own_h)+8u)=0x2cu;}else if(variant==9u)nb.put32(0x218u,1u);
            std::memcpy(reinterpret_cast<void*>(R074ObjectBase),native.data(),native.size());std::memcpy(reinterpret_cast<void*>(R074CurrentBase),current.data(),current.size());
            const auto save_mode=*reinterpret_cast<std::uint32_t*>(0x78026cu),save_ext=*reinterpret_cast<std::uint32_t*>(0x82e7d8u),save_sys=*reinterpret_cast<std::uint32_t*>(0x67f614u),save_menu=*reinterpret_cast<std::uint32_t*>(0x7d38f0u),save_mask=*reinterpret_cast<std::uint32_t*>(0x7d6778u),save258=*reinterpret_cast<std::uint32_t*>(0x780258u),save25c=*reinterpret_cast<std::uint32_t*>(0x78025cu);const auto save248=*reinterpret_cast<std::uint8_t*>(0x780248u);
            *reinterpret_cast<std::uint32_t*>(0x78026cu)=gi.game_mode_78026c;*reinterpret_cast<std::uint32_t*>(0x82e7d8u)=gi.external_block_82e7d8;*reinterpret_cast<std::uint32_t*>(0x67f614u)=gi.system_handle_67f614;*reinterpret_cast<std::uint32_t*>(0x7d38f0u)=gi.menu_state_7d38f0;*reinterpret_cast<std::uint32_t*>(0x7d6778u)=gi.player0_feature_mask;*reinterpret_cast<std::uint8_t*>(0x780248u)=0u;*reinterpret_cast<std::uint32_t*>(0x780258u)=3u;*reinterpret_cast<std::uint32_t*>(0x78025cu)=2u;
            reset_r074_state();reset_r071_trace(created,1u,0);r071_fill_callback_table();set_r071_patches(true);prepare(0x444470u);guest_call.ecx=R074ObjectBase;run();set_r071_patches(false);r071_restore_callback_table();
            R071Trace nt{};nt.callback_ret=created;nt.ready_ret=1u;R074GateTrace ng{};auto bindings=r074_handle_bindings();PcNativeHandleResolver hr{bindings.data(),bindings.size()};auto ct=r071_native_callback_table();PcObjectEventCallbackTable cv{ct.data(),ct.size()};const auto pt=r065_pair_table();PcObjectEventModeInputs ei{0u,3u,gi.game_mode_78026c,2u};auto ds=r074_dispatch_services(nt);auto rs=r074_runtime_services(nt);auto os=r074_open_services(nt);PcRuntimeGate444470Services gs{&ng,r074_native_configure};object_runtime_gate_444470(nb,nc,hr,gi,pt,cv,ei,ds,rs,os,gs);
            compare_blob("object_runtime_gate_object_444470_r074",reinterpret_cast<void*>(R074ObjectBase),native);const auto gg=r074_guest_gate_trace();compare_u32("object_runtime_gate_configure_count_444470_r074",gg.configure_n,ng.configure_n);compare_u32("object_runtime_gate_configure_selector_444470_r074",gg.selector,ng.selector);compare_u32("object_runtime_gate_configure_flag_444470_r074",gg.flag,ng.flag);
            *reinterpret_cast<std::uint32_t*>(0x78026cu)=save_mode;*reinterpret_cast<std::uint32_t*>(0x82e7d8u)=save_ext;*reinterpret_cast<std::uint32_t*>(0x67f614u)=save_sys;*reinterpret_cast<std::uint32_t*>(0x7d38f0u)=save_menu;*reinterpret_cast<std::uint32_t*>(0x7d6778u)=save_mask;*reinterpret_cast<std::uint8_t*>(0x780248u)=save248;*reinterpret_cast<std::uint32_t*>(0x780258u)=save258;*reinterpret_cast<std::uint32_t*>(0x78025cu)=save25c;
        }
        // r075: mandatory runtime blockers exposed by r072/r074.
        if(enabled("runtime_course_load_44da00_r075")){
            static constexpr std::uint32_t modes[]{0u,1u,3u,4u,5u,6u,7u,8u,9u,10u};const auto mode=modes[i%std::size(modes)];const auto alt=std::uint8_t((i/3u)&1u);const auto ret=(i%4u)==0u?0u:0x12345678u;
            const auto saved=*reinterpret_cast<std::uint32_t*>(0x7d2d8cu);reset_r075_state();*reinterpret_cast<std::uint32_t*>(R075StateBase+0x84u)=ret;set_r075_course_patch(true);prepare(0x44da00u);Bytes(reinterpret_cast<void*>(S),12).put32(0,mode);Bytes(reinterpret_cast<void*>(S),12).put32(4,alt);run_original32();set_r075_course_patch(false);
            const std::string gd(reinterpret_cast<char*>(R075StateBase+0x00u)),gc(reinterpret_cast<char*>(R075StateBase+0x40u));const auto gf=*reinterpret_cast<std::uint32_t*>(0x7d2d8cu);reset_r075_state();*reinterpret_cast<std::uint32_t*>(R075StateBase+0x84u)=ret;R075NativeTrace nt{};PcCourseLoadState44da00 ns{};PcCourseLoadServices44da00 sv{&nt,r075_native_load};const auto nr=runtime_course_load_44da00(mode,alt,ns,sv);const std::string nd(reinterpret_cast<char*>(R075StateBase+0x200u)),nc(reinterpret_cast<char*>(R075StateBase+0x240u));compare_u32("runtime_course_load_return_44da00_r075",guest_call.out_eax&0xffu,nr?1u:0u);compare_u32("runtime_course_load_force_44da00_r075",gf,alt?saved:1u);compare_u32("runtime_course_load_data_string_44da00_r075",gd==nd?1u:0u,1u);compare_u32("runtime_course_load_course_string_44da00_r075",gc==nc?1u:0u,1u);compare_u32("runtime_course_load_native_force_44da00_r075",ns.force_sync_7d2d8c,alt?0u:1u);*reinterpret_cast<std::uint32_t*>(0x7d2d8cu)=saved;
        }
        if(enabled("embedded_slot_refresh_446bb0_r075")){
            auto native=r075_fixture(i);Bytes nb(native.data(),native.size());const auto slot=i%4u,depth=nb.u32(0x408u);static constexpr std::uint32_t keys[]{4u,8u,0x10u,0x20u,1u,2u,0x8000u,0x4000u,0x55u};const auto key=keys[i%std::size(keys)];nb.put32(0x08u+std::size_t(depth)*0x20u+slot*4u,key);std::memcpy(reinterpret_cast<void*>(R075ObjectBase),native.data(),native.size());reset_r075_state();set_r075_embedded_patches(true);prepare(0x446bb0u);guest_call.ecx=R075ObjectBase;Bytes(reinterpret_cast<void*>(S),8).put32(0,slot);run_original32();set_r075_embedded_patches(false);R075NativeTrace nt{};auto sv=r075_native_services(nt);embedded_slot_refresh_446bb0(nb,slot,sv);compare_blob("embedded_slot_refresh_object_446bb0_r075",reinterpret_cast<void*>(R075ObjectBase),native);const auto gt=r075_guest_trace();compare_u32("embedded_slot_refresh_cfg_n_446bb0_r075",gt.cfg_n,nt.t.cfg_n);compare_u32("embedded_slot_refresh_effect_446bb0_r075",gt.effect,nt.t.effect);compare_u32("embedded_slot_refresh_x_446bb0_r075",gt.x,nt.t.x);compare_u32("embedded_slot_refresh_y_446bb0_r075",gt.y,nt.t.y);compare_u32("embedded_slot_refresh_fin_n_446bb0_r075",gt.fin_n,nt.t.fin_n);
        }
        if(enabled("embedded_configure_446f30_r075")){
            auto native=r075_fixture(i+17u);Bytes nb(native.data(),native.size());static constexpr std::uint32_t selectors[]{1u,4u,8u,0x10u,0x20u,0x4000u,0x8000u,0x55u};const auto selector=selectors[i%std::size(selectors)];const auto flag=std::int8_t((i%3u)==0u?-1:(i%3u));const auto depth=nb.u32(0x408u);for(unsigned k=0;k<4u;++k)nb.put32(0x08u+std::size_t(depth)*0x20u+k*4u,((k+i)%3u)==0u?selector:0x55u);std::memcpy(reinterpret_cast<void*>(R075ObjectBase),native.data(),native.size());reset_r075_state();set_r075_embedded_patches(true);prepare(0x446f30u);guest_call.ecx=R075ObjectBase;Bytes(reinterpret_cast<void*>(S),12).put32(0,selector);Bytes(reinterpret_cast<void*>(S),12).put32(4,std::uint32_t(std::uint8_t(flag)));run_original32();set_r075_embedded_patches(false);R075NativeTrace nt{};auto sv=r075_native_services(nt);const auto nr=embedded_configure_446f30(nb,selector,flag,sv);compare_u32("embedded_configure_return_446f30_r075",guest_call.out_eax&0xffu,nr?1u:0u);compare_blob("embedded_configure_object_446f30_r075",reinterpret_cast<void*>(R075ObjectBase),native);const auto gt=r075_guest_trace();compare_u32("embedded_configure_cfg_n_446f30_r075",gt.cfg_n,nt.t.cfg_n);compare_u32("embedded_configure_effect_446f30_r075",gt.effect,nt.t.effect);compare_u32("embedded_configure_fin_n_446f30_r075",gt.fin_n,nt.t.fin_n);compare_u32("embedded_configure_fx_n_446f30_r075",gt.fx_n,nt.t.fx_n);compare_u32("embedded_configure_fx_value_446f30_r075",gt.fx_value,nt.t.fx_value);
        }
        // r076: course-runtime builder and direct children.
        if(enabled("runtime_course_shuffle_44bf30_r076")){
            const auto n=std::int32_t(2u+i%14u);auto native=r076_records(i,n);std::memcpy(reinterpret_cast<void*>(R076RecordsBase),native.data(),native.size());*reinterpret_cast<std::int32_t*>(R076StateBase)=std::int32_t((i*1103515245u+12345u)&0x7fffu);
            set_r076_rng_patch(true);prepare(0x44bf30u);guest_call.eax=std::uint32_t(n);guest_call.ebx=R076RecordsBase;run();set_r076_rng_patch(false);
            runtime_course_shuffle_44bf30(Bytes(native.data(),native.size()),n,nullptr,r076_native_rng);compare_blob("runtime_course_shuffle_44bf30_r076",reinterpret_cast<void*>(R076RecordsBase),native);
        }
        if(enabled("runtime_course_tree_44c850_r076")){
            const auto n=std::int32_t(6u+i%6u);auto native=r076_records(i+31u,n);Bytes nb(native.data(),native.size());
            for(std::int32_t k=0;k<n;++k){auto r=nb.sub(std::size_t(k)*PcCourseRecord44d720Size,PcCourseRecord44d720Size);for(const auto& p:std::array<std::pair<std::size_t,std::size_t>,2>{{{0x2cu,0x24u},{0x30u,0x28u}}}){r.put32(p.second,0xffffffffu);const auto key=r.u32(p.first);for(std::int32_t j=0;j<n;++j)if(nb.sub(std::size_t(j)*PcCourseRecord44d720Size,PcCourseRecord44d720Size).u32(0x04u)==key){r.puti(p.second,j);break;}}}
            std::memcpy(reinterpret_cast<void*>(R076RecordsBase),native.data(),native.size());*reinterpret_cast<std::uint32_t*>(0x7d33bcu)=R076RecordsBase;*reinterpret_cast<std::int32_t*>(0x7d33c0u)=0;prepare(0x44c850u);Bytes(reinterpret_cast<void*>(S),16).put32(0,R076RecordsBase);Bytes(reinterpret_cast<void*>(S),16).put32(4,0u);Bytes(reinterpret_cast<void*>(S),16).put32(8,0u);run();
            std::int32_t md=0;runtime_course_tree_44c850(nb,n,0,0,0,md);compare_blob("runtime_course_tree_records_44c850_r076",reinterpret_cast<void*>(R076RecordsBase),native);compare_u32("runtime_course_tree_depth_44c850_r076",std::uint32_t(*reinterpret_cast<std::int32_t*>(0x7d33c0u)),std::uint32_t(md));
        }
        if(enabled("runtime_course_matrix_44c0d0_r076")){
            const auto primary=r076_primary_table();const auto& d=primary[i%primary.size()];r076_prefill_globals();reset_matrix_oracle_globals();*reinterpret_cast<std::uint32_t*>(0x7d30bcu)=d.token;prepare(0x44c0d0u);run();
            std::array<std::uint8_t,128> nm{};PcMatrixStack ms{Bytes(nm.data(),nm.size()),0,0,2};PcCourseRuntimeState44d720 st{};runtime_course_matrix_44c0d0(d,ms,st);
            compare_blob("runtime_course_matrix_a_44c0d0_r076",reinterpret_cast<void*>(0x7d2da0u),st.matrix_7d2da0);compare_blob("runtime_course_matrix_b_44c0d0_r076",reinterpret_cast<void*>(0x7d3130u),st.matrix_7d3130);compare_blob("runtime_course_matrix_final_44c0d0_r076",reinterpret_cast<void*>(0x7d3190u),st.matrix_7d3190);compare_u32("runtime_course_matrix_depth_44c0d0_r076",std::uint32_t(*reinterpret_cast<std::int32_t*>(0x89b568u)),std::uint32_t(ms.depth));
        }
        if(enabled("runtime_course_force_mode_46c360_r076")){
            const auto v=0x76000000u^(i*0x10203u);*reinterpret_cast<std::uint32_t*>(0x7f95a8u)=0xdeadbeefu;prepare(0x46c360u);Bytes(reinterpret_cast<void*>(S),8).put32(0,v);run();PcCourseRuntimeState44d720 st{};st.force_mode_7f95a8=0xdeadbeefu;runtime_course_force_mode_46c360(st,v);compare_u32("runtime_course_force_mode_46c360_r076",*reinterpret_cast<std::uint32_t*>(0x7f95a8u),st.force_mode_7f95a8);
        }
        if(enabled("runtime_apply_course_data_44d720_r076")){
            const bool shuffle=(i%4u)==0u;const std::int32_t n=shuffle?15:std::int32_t(6u+i%7u);const bool force=(i%3u)==0u;const auto selected_key=std::uint32_t((1u+(i%std::uint32_t(n)))*10u);auto native=r076_records(i+79u,n);std::memcpy(reinterpret_cast<void*>(R076RecordsBase),native.data(),native.size());
            r076_prefill_globals();reset_matrix_oracle_globals();const auto rv=std::int32_t((i*1664525u+1013904223u)&0x7fffu);*reinterpret_cast<std::int32_t*>(R076StateBase)=rv;
            if (shuffle) set_r076_rng_patch(true);
            prepare(0x44d720u);
            Bytes args(reinterpret_cast<void*>(S), 44);
            args.put32(0, 0u);
            args.put32(4, 0u);
            args.put32(8, 0u);
            args.put32(12, selected_key);
            args.put32(16, force ? 1u : 0u);
            args.put32(20, 0u);
            args.put32(24, shuffle ? 1u : 0u);
            args.put32(28, R076RecordsBase);
            args.put32(32, std::uint32_t(n));
            args.put32(36, 0u);
            run();
            if (shuffle) set_r076_rng_patch(false);
            const auto primary=r076_primary_table();const auto secondary=r076_secondary_table();std::array<std::uint8_t,128> nm{};PcMatrixStack ms{Bytes(nm.data(),nm.size()),0,0,2};PcCourseRuntimeState44d720 st{};st.resource_type_635f34=0x42u;st.resource_source_635f38=0x7600aa55u;st.force_mode_7f95a8=0x76feed00u;PcCourseApplyRequest44d720 rq{};rq.data_name="oracle";rq.category_name="oracle_course";rq.selected_key=selected_key;rq.force_selected=force;rq.shuffle_groups=shuffle;rq.direct_records=native.data();rq.direct_bytes=native.size();rq.direct_count=n;PcCourseRuntimeServices44d720 sv{};sv.random_value=r076_native_rng;const auto nr=runtime_apply_course_data_44d720(rq,{primary.data(),primary.size(),secondary.data(),secondary.size()},st,ms,sv);
            compare_u32("runtime_apply_course_return_44d720_r076",guest_call.out_eax,nr?1u:0u);compare_blob("runtime_apply_course_records_44d720_r076",reinterpret_cast<void*>(R076RecordsBase),native);compare_blob("runtime_apply_course_selected_a_44d720_r076",reinterpret_cast<void*>(0x7d30a8u),st.selected_7d30a8);compare_blob("runtime_apply_course_selected_b_44d720_r076",reinterpret_cast<void*>(0x7d2de0u),st.selected_7d2de0);compare_blob("runtime_apply_course_matrix_a_44d720_r076",reinterpret_cast<void*>(0x7d2da0u),st.matrix_7d2da0);compare_blob("runtime_apply_course_matrix_b_44d720_r076",reinterpret_cast<void*>(0x7d3130u),st.matrix_7d3130);compare_blob("runtime_apply_course_matrix_final_44d720_r076",reinterpret_cast<void*>(0x7d3190u),st.matrix_7d3190);
            compare_u32("runtime_apply_course_count_44d720_r076",std::uint32_t(*reinterpret_cast<std::int32_t*>(0x7d33c4u)),std::uint32_t(st.active_count_7d33c4));compare_u32("runtime_apply_course_depth_44d720_r076",std::uint32_t(*reinterpret_cast<std::int32_t*>(0x7d33c0u)),std::uint32_t(st.max_depth_7d33c0));compare_u32("runtime_apply_course_force_44d720_r076",*reinterpret_cast<std::uint32_t*>(0x7f95a8u),st.force_mode_7f95a8);compare_u32("runtime_apply_course_stage_key_44d720_r076",*reinterpret_cast<std::uint32_t*>(0x635f2cu),std::uint32_t(st.stage_key_635f2c));compare_u32("runtime_apply_course_stage_value_44d720_r076",*reinterpret_cast<std::uint32_t*>(0x635f30u),std::uint32_t(st.stage_value_635f30));compare_u32("runtime_apply_course_selected_index_44d720_r076",(*reinterpret_cast<std::uint32_t*>(0x7d33bcu)-R076RecordsBase)/std::uint32_t(PcCourseRecord44d720Size),std::uint32_t(st.selected_index));compare_u32("runtime_apply_course_selected_ptr_44d720_r076",*reinterpret_cast<std::uint32_t*>(0x7d3188u)==0x7d30a8u?1u:0u,st.selected_copy_active?1u:0u);
        }

        // r077: category hash and exact fast-path accessors.
        if(enabled("runtime_category_hash_4f1260_r077")){
            const auto fx=r077_fixture(i);prepare(0x4f1260u);guest_call.eax=R077NameBase;run();compare_u32("runtime_category_hash_4f1260_r077",guest_call.out_eax,runtime_category_hash_4f1260(fx.query.c_str()));
        }
        if(enabled("runtime_category_records_4f1a90_r077")){
            const auto fx=r077_fixture(i+17u);prepare(0x4f1a90u);Bytes a(reinterpret_cast<void*>(S),16u);a.put32(0,R077NameBase);a.put32(4,R077LoaderBase);run();auto* p=runtime_category_records_4f1a90(fx.query.c_str(),fx.view);compare_u32("runtime_category_records_4f1a90_r077",guest_call.out_eax,p?std::uint32_t(std::uintptr_t(p)):0u);
        }
        if(enabled("runtime_category_count_4f1ba0_r077")){
            const auto fx=r077_fixture(i+31u);prepare(0x4f1ba0u);Bytes a(reinterpret_cast<void*>(S),16u);a.put32(0,R077NameBase);a.put32(4,R077LoaderBase);run();compare_u32("runtime_category_count_4f1ba0_r077",guest_call.out_eax,std::uint32_t(runtime_category_count_4f1ba0(fx.query.c_str(),fx.view)));
        }

        // r079: the two transition-result dispatchers and their concrete
        // five-state startup/update owner above 0x444350.  Still-open prepare
        // children are trace stubs on both sides; result code 5 deliberately
        // reaches the already-closed commit/release path.
        if(enabled("object_runtime_dispatch_primary_445430_r079")){
            const std::uint32_t result=i%8u;const std::int32_t selected=std::int32_t((i%11u))-5;const std::uint32_t trace_seed=0x79000000u^(i*0x10203u);
            reset_r079_trace(trace_seed);set_r079_dispatch_patches(true);prepare(0x445430u);guest_call.ecx=R079ObjectBase;Bytes a(reinterpret_cast<void*>(S),16u);a.put32(0,result);a.put32(4,std::uint32_t(selected));run_original32();set_r079_dispatch_patches(false);if(guest_call.out_sp!=S+8u)throw std::runtime_error("r079 445430 stack imbalance");
            R079Trace nt{};nt.selector_ret=trace_seed;PcRuntimeTransitionServicesR079 ts{&nt,r079_native_action};const auto nr=object_runtime_dispatch_primary_445430(Bytes(reinterpret_cast<void*>(R079ObjectBase),R079ObjectSize),result,selected,ts);
            compare_u32("object_runtime_dispatch_primary_445430_r079",guest_call.out_eax&0xffu,nr?1u:0u);auto gt=r079_guest_trace_blob(),nb=r079_trace_blob(nt);compare_blob("object_runtime_dispatch_primary_trace_445430_r079",gt.data(),nb);
        }
        if(enabled("object_runtime_dispatch_secondary_4454b0_r079")){
            const std::uint32_t result=i%8u;const std::int32_t selected=std::int32_t((i%13u))-6;const std::uint32_t trace_seed=0x79100000u^(i*0x30405u);
            reset_r079_trace(trace_seed);set_r079_dispatch_patches(true);prepare(0x4454b0u);guest_call.ecx=R079ObjectBase;Bytes a(reinterpret_cast<void*>(S),16u);a.put32(0,result);a.put32(4,std::uint32_t(selected));run_original32();set_r079_dispatch_patches(false);if(guest_call.out_sp!=S+8u)throw std::runtime_error("r079 4454b0 stack imbalance");
            R079Trace nt{};nt.selector_ret=trace_seed;PcRuntimeTransitionServicesR079 ts{&nt,r079_native_action};const auto nr=object_runtime_dispatch_secondary_4454b0(Bytes(reinterpret_cast<void*>(R079ObjectBase),R079ObjectSize),result,selected,ts);
            compare_u32("object_runtime_dispatch_secondary_4454b0_r079",guest_call.out_eax&0xffu,nr?1u:0u);auto gt=r079_guest_trace_blob(),nb=r079_trace_blob(nt);compare_blob("object_runtime_dispatch_secondary_trace_4454b0_r079",gt.data(),nb);
        }
        if(enabled("object_runtime_owner_445be0_r079")){
            const std::uint32_t state=i%7u;const std::uint32_t selector_ret=(i*3u+1u)%8u;auto native=r079_object_fixture(i+79u,state);Bytes nb(native.data(),native.size());std::memcpy(reinterpret_cast<void*>(R079ObjectBase),native.data(),native.size());
            // Keep the concrete selector path deterministic: +0x518 is active,
            // so both original selector call-sites (patched here) and the native
            // selector resolve selected_kind=3.  No manager-tail shortcut is used.
            reset_r079_trace(selector_ret);PcObjectSelectorGlobals sg{};R079Trace nt{};nt.selector_ret=selector_ret;auto sv=r079_owner_services(nt);
            const float saved_timer=*reinterpret_cast<float*>(0x842110u);const float timer=float(std::int32_t((i*17u)%4096u)-2048)*0.03125f;*reinterpret_cast<float*>(0x842110u)=timer;const float scale=*reinterpret_cast<const float*>(0x62812cu);
            std::array<std::uint8_t,0x218u> saved_snapshot{};std::memcpy(saved_snapshot.data(),reinterpret_cast<void*>(R072SnapshotBase),saved_snapshot.size());std::memset(reinterpret_cast<void*>(R072SnapshotBase),0,0x218u);PcObjectRuntimeSnapshot444350 snap{};
            set_r079_owner_patches(true);set_r079_dispatch_patches(true);set_r072_patches(true);prepare(0x445be0u);guest_call.ecx=R079ObjectBase;run_original32();set_r072_patches(false);set_r079_dispatch_patches(false);set_r079_owner_patches(false);if(guest_call.out_sp!=S)throw std::runtime_error("r079 445be0 stack imbalance");
            const auto nr=object_runtime_owner_445be0(nb,snap,sg,{timer,scale},sv);compare_u32("object_runtime_owner_return_445be0_r079",guest_call.out_eax&0xffu,nr);compare_blob("object_runtime_owner_object_445be0_r079",reinterpret_cast<void*>(R079ObjectBase),native);auto gt=r079_guest_trace_blob(),ntb=r079_trace_blob(nt);compare_blob("object_runtime_owner_trace_445be0_r079",gt.data(),ntb);
            compare_blob("object_runtime_owner_snapshot_445be0_r079",reinterpret_cast<void*>(R072SnapshotBase),snap.bytes);compare_u32("object_runtime_owner_snapshot_active_445be0_r079",*reinterpret_cast<std::uint8_t*>(R072SnapshotBase+0x214u),snap.active);
            std::memcpy(reinterpret_cast<void*>(R072SnapshotBase),saved_snapshot.data(),saved_snapshot.size());*reinterpret_cast<float*>(0x842110u)=saved_timer;
        }
        if(enabled("runtime_owner_entry_49e4a0_r085")){
            const std::uint32_t state=i%7u;const std::uint32_t selector_ret=(i*5u+3u)%8u;auto native=r079_object_fixture(i+850u,state);Bytes nb(native.data(),native.size());std::memcpy(reinterpret_cast<void*>(R079ObjectBase),native.data(),native.size());
            reset_r079_trace(selector_ret);PcObjectSelectorGlobals sg{};R079Trace nt{};nt.selector_ret=selector_ret;auto sv=r079_owner_services(nt);
            const float saved_timer=*reinterpret_cast<float*>(0x842110u);const float timer=float(std::int32_t((i*29u)%4096u)-2048)*0.015625f;*reinterpret_cast<float*>(0x842110u)=timer;const float scale=*reinterpret_cast<const float*>(0x62812cu);
            std::array<std::uint8_t,0x218u> saved_snapshot{};std::memcpy(saved_snapshot.data(),reinterpret_cast<void*>(R072SnapshotBase),saved_snapshot.size());std::memset(reinterpret_cast<void*>(R072SnapshotBase),0,0x218u);PcObjectRuntimeSnapshot444350 snap{};
            set_r079_owner_patches(true);set_r079_dispatch_patches(true);set_r072_patches(true);prepare(0x49e4a0u);run_original32();set_r072_patches(false);set_r079_dispatch_patches(false);set_r079_owner_patches(false);if(guest_call.out_sp!=S)throw std::runtime_error("r085 49e4a0 stack imbalance");
            const auto nr=runtime_owner_entry_49e4a0(nb,snap,sg,{timer,scale},sv);compare_u32("runtime_owner_entry_return_49e4a0_r085",guest_call.out_eax&0xffu,nr);compare_blob("runtime_owner_entry_object_49e4a0_r085",reinterpret_cast<void*>(R079ObjectBase),native);auto gt=r079_guest_trace_blob(),ntb=r079_trace_blob(nt);compare_blob("runtime_owner_entry_trace_49e4a0_r085",gt.data(),ntb);
            compare_blob("runtime_owner_entry_snapshot_49e4a0_r085",reinterpret_cast<void*>(R072SnapshotBase),snap.bytes);compare_u32("runtime_owner_entry_snapshot_active_49e4a0_r085",*reinterpret_cast<std::uint8_t*>(R072SnapshotBase+0x214u),snap.active);
            std::memcpy(reinterpret_cast<void*>(R072SnapshotBase),saved_snapshot.data(),saved_snapshot.size());*reinterpret_cast<float*>(0x842110u)=saved_timer;
        }
        // r086: static EventOpen provider and the four concrete EvFuncID 36
        // lifecycle entries surrounding the r085 control callback.
        if(enabled("event_setup_440110_r086")){
            auto ns=r052_event_fixture(i+860u);const std::uint32_t id=(i*17u+7u)%PcEventSlotCount,fn=(i*11u+3u)%PcEventFunctionTableCount;
            const auto initial_records=native_r052_records(ns);std::memcpy(reinterpret_cast<void*>(R052RecordBase+id*0x3cu),initial_records.data()+id*0x3cu,0x3cu);*reinterpret_cast<std::uint8_t*>(R052FlagsBase+id)=ns.slots[id].flags;const auto descriptors=r053_init_descriptors();const auto functions=r053_function_descriptors();
            set_r086_provider_patch(true);prepare(0x440110u);Bytes st(reinterpret_cast<void*>(S),16);st.put32(0,id);st.put32(4,fn);run_original32();set_r086_provider_patch(false);
            event_setup_440110(ns,id,fn,descriptors,functions);
            const auto records=native_r052_records(ns);const auto flags=native_r052_flags(ns);std::array<std::uint8_t,0x3cu> expected_record{};std::memcpy(expected_record.data(),records.data()+id*0x3cu,expected_record.size());
            compare_blob("event_setup_record_440110_r086",reinterpret_cast<void*>(R052RecordBase+id*0x3cu),expected_record);
            compare_u32("event_setup_flag_440110_r086",*reinterpret_cast<std::uint8_t*>(R052FlagsBase+id),flags[id]);
        }
        if(enabled("runtime_init_entry_49e490_r086")){
            auto native=r069_object_fixture(i+861u);Bytes nb(native.data(),native.size());auto ntbl=r069_table_fixture(i+861u);auto gtbl=ntbl;
            const std::uint8_t mode=std::uint8_t((i*29u)&0xffu);std::array<std::uint8_t,R069TableBytes> saved_table{};std::memcpy(saved_table.data(),reinterpret_cast<void*>(R069TableBase),saved_table.size());const auto saved_mode=*reinterpret_cast<std::uint8_t*>(0x006319a1u);
            std::memcpy(reinterpret_cast<void*>(R069ObjectBase),native.data(),native.size());std::memcpy(reinterpret_cast<void*>(R069TableBase),gtbl.data(),R069TableBytes);*reinterpret_cast<std::uint8_t*>(0x006319a1u)=mode;
            reset_r069_trace();R069Trace nt{};PcUiNotifyServices ui{};PcObjectRuntimeInitServices sv{&nt,r069_native_embedded,ui};
            set_r069_patch(true);set_r086_init_entry_patch(true);prepare(0x49e490u);run_original32();set_r086_init_entry_patch(false);set_r069_patch(false);
            const auto nr=runtime_init_entry_49e490(nb,ntbl,mode,sv);compare_u32("runtime_init_entry_return_49e490_r086",guest_call.out_eax,nr);compare_blob("runtime_init_entry_object_49e490_r086",reinterpret_cast<void*>(R069ObjectBase),native);
            std::array<std::uint8_t,R069TableBytes> ntbl_bytes{};std::memcpy(ntbl_bytes.data(),ntbl.data(),ntbl_bytes.size());compare_blob("runtime_init_entry_table_49e490_r086",reinterpret_cast<void*>(R069TableBase),ntbl_bytes);auto gt=r069_guest_trace_blob(),ntb=r069_trace_blob(nt);compare_blob("runtime_init_entry_trace_49e490_r086",gt.data(),ntb);
            std::memcpy(reinterpret_cast<void*>(R069TableBase),saved_table.data(),saved_table.size());*reinterpret_cast<std::uint8_t*>(0x006319a1u)=saved_mode;
        }
        if(enabled("runtime_display_entry_49e4b0_r086")){
            r064_prepare_children(i+862u);auto native=r064_object_fixture(i+862u);Bytes nb(native.data(),native.size());nb.puti(0x484,0);nb.puti(0x518,0);nb.put8(0x494,0);nb.put8(0x48c,0);
            const unsigned mode=i%16u;if(mode&1u){const auto index=1+std::int32_t(i%12u);nb.puti(0x484,index);nb.put32(0x280u+std::size_t(index)*4u,r064_child(1u+unsigned(index)));}if(mode&2u){nb.puti(0x518,1);nb.put32(0x498,r064_child(40u));}if(mode&4u){nb.put8(0x494,1);nb.put32(0x490,(i%9u)==0u?0u:r064_child(41u));}if(mode&8u){nb.put8(0x48c,1);nb.put32(0x488,(i%11u)==0u?0u:r064_child(42u));}
            std::memcpy(reinterpret_cast<void*>(R064ObjectBase),native.data(),native.size());reset_r064_trace();R064Trace nt{};auto sv=r064_services(nt);
            set_r064_patches(true);set_r086_display_entry_patch(true);prepare(0x49e4b0u);run_original32();set_r086_display_entry_patch(false);set_r064_patches(false);runtime_display_entry_49e4b0(nb,sv);
            compare_blob("runtime_display_entry_object_49e4b0_r086",reinterpret_cast<void*>(R064ObjectBase),native);auto gt=r064_guest_trace_blob(),ntb=r064_trace_blob(nt);compare_blob("runtime_display_entry_trace_49e4b0_r086",gt.data(),ntb);
        }
        if(enabled("runtime_teardown_alias_443e90_r086")){
            auto native=r070_object_fixture(i+863u);Bytes nb(native.data(),native.size());std::memcpy(reinterpret_cast<void*>(R070ObjectBase),native.data(),native.size());reset_r070_trace();R070Trace nt{};auto sv=r070_services(nt);
            set_r070_patches(true);prepare(0x443e90u);guest_call.ecx=R070ObjectBase;run_original32();set_r070_patches(false);const auto nr=runtime_teardown_alias_443e90(nb,sv);
            compare_u32("runtime_teardown_alias_return_443e90_r086",guest_call.out_eax,nr);compare_blob("runtime_teardown_alias_object_443e90_r086",reinterpret_cast<void*>(R070ObjectBase),native);auto gt=r070_guest_trace_blob(),ntb=r070_trace_blob(nt);compare_blob("runtime_teardown_alias_trace_443e90_r086",gt.data(),ntb);
        }
        if(enabled("runtime_destroy_entry_49e4c0_r086")){
            auto native=r070_object_fixture(i+864u);Bytes nb(native.data(),native.size());std::memcpy(reinterpret_cast<void*>(R070ObjectBase),native.data(),native.size());reset_r070_trace();R070Trace nt{};auto sv=r070_services(nt);
            set_r070_patches(true);set_r086_destroy_entry_patch(true);prepare(0x49e4c0u);run_original32();set_r086_destroy_entry_patch(false);set_r070_patches(false);const auto nr=runtime_destroy_entry_49e4c0(nb,sv);
            compare_u32("runtime_destroy_entry_return_49e4c0_r086",guest_call.out_eax,nr);compare_blob("runtime_destroy_entry_object_49e4c0_r086",reinterpret_cast<void*>(R070ObjectBase),native);auto gt=r070_guest_trace_blob(),ntb=r070_trace_blob(nt);compare_blob("runtime_destroy_entry_trace_49e4c0_r086",gt.data(),ntb);
        }


        // r087: execute the original 0x417810 orchestration with every
        // non-event subsystem redirected to deterministic trace stubs.  The
        // real original 0x440BF0 InitEventControl executes and is compared to
        // the native bootstrap's event table plus exact call order/arguments.
        if(enabled("runtime_bootstrap_417810_r087")){
            static const auto desc=r053_init_descriptors();static const auto funcs=r053_function_descriptors();
            auto ns=r052_event_fixture(i+870u);
            for(std::uint32_t id=0;id<PcEventSlotCount;++id){auto& q=ns.slots[id];q.flags=std::uint8_t((id*19u+i*7u+0x33u)&0xffu);q.descriptor_token=0xb1000000u^id;q.event_id=0xb2000000u^id;q.work_token=0xb3000000u^(id+i);q.display_scene=0xb4000000u^i;q.init_callback=0xb5000000u^id;q.ctrl_callback=0xb6000000u^id;q.disp_callback=0xb7000000u^id;q.shadow_callback=0xb8000000u^id;q.dest_callback=0xb9000000u^id;q.aux24=0xba000000u^id;q.aux28=0xbb000000u^id;q.close_guard=0xbc000000u^id;q.function_id=0xbd000000u^id;q.aux34=0xbe000000u^id;q.aux38=0xbf000000u^id;}
            ns.current_slot=(i*67u+23u)%PcEventSlotCount;store_r052_state(ns);
            const auto saved_system=*reinterpret_cast<std::uint32_t*>(0x89bd60u);*reinterpret_cast<std::uint32_t*>(0x89bd60u)=R087SystemBase;
            const std::uint32_t final_ret=0x87000000u^(i*0x10203u);reset_r087_trace(final_ret);
            set_r087_bootstrap_patches(true);prepare(0x417810u);run_original32();set_r087_bootstrap_patches(false);
            *reinterpret_cast<std::uint32_t*>(0x89bd60u)=saved_system;
            R087Trace nt{};nt.final_ret=final_ret;const auto nr=runtime_bootstrap_417810(ns,desc,funcs,{&nt,r087_native_call,R087SystemBase});
            compare_u32("runtime_bootstrap_return_417810_r087",guest_call.out_eax,nr);
            const auto records=native_r052_records(ns);const auto flags=native_r052_flags(ns);compare_blob("runtime_bootstrap_event_records_417810_r087",reinterpret_cast<void*>(R052RecordBase),records);compare_blob("runtime_bootstrap_event_flags_417810_r087",reinterpret_cast<void*>(R052FlagsBase),flags);
            const auto gt=r087_guest_trace_blob(),ntb=r087_trace_blob(nt);compare_blob("runtime_bootstrap_trace_417810_r087",gt.data(),ntb);
        }


        // r088: direct owner around the r087 bootstrap.  Gate true executes
        // original 417810 with r087 child patches; gate false skips it.  Both
        // paths compare owner trace, event state and release/null semantics.
        if(enabled("runtime_startup_owner_4176e0_r088")){
            static const auto desc=r053_init_descriptors();static const auto funcs=r053_function_descriptors();
            auto ns=r052_event_fixture(i+880u);for(std::uint32_t id=0;id<PcEventSlotCount;++id){auto& q=ns.slots[id];q.flags=std::uint8_t((id*23u+i*11u+0x55u)&0xffu);q.descriptor_token=0xc1000000u^id;q.event_id=0xc2000000u^id;q.work_token=0xc3000000u^(id+i);q.display_scene=0xc4000000u^i;q.init_callback=0xc5000000u^id;q.ctrl_callback=0xc6000000u^id;q.disp_callback=0xc7000000u^id;q.shadow_callback=0xc8000000u^id;q.dest_callback=0xc9000000u^id;q.aux24=0xca000000u^id;q.aux28=0xcb000000u^id;q.close_guard=0xcc000000u^id;q.function_id=0xcd000000u^id;q.aux34=0xce000000u^id;q.aux38=0xcf000000u^id;}ns.current_slot=(i*71u+29u)%PcEventSlotCount;store_r052_state(ns);
            const auto save_a=*reinterpret_cast<std::uint32_t*>(0x89bd60u),save_b=*reinterpret_cast<std::uint32_t*>(0x89bd9cu);*reinterpret_cast<std::uint32_t*>(0x89bd60u)=R087SystemBase;*reinterpret_cast<std::uint32_t*>(0x89bd9cu)=R088SecondaryBase;
            const std::uint32_t gate=(i&1u),final_ret=0x88000000u^(i*0x10203u);reset_r088_trace(gate);reset_r087_trace(final_ret);
            set_r087_bootstrap_patches(true);set_r088_startup_patches(true);prepare(0x4176e0u);run_original32();set_r088_startup_patches(false);set_r087_bootstrap_patches(false);
            const auto guest_a=*reinterpret_cast<std::uint32_t*>(0x89bd60u),guest_b=*reinterpret_cast<std::uint32_t*>(0x89bd9cu);*reinterpret_cast<std::uint32_t*>(0x89bd60u)=save_a;*reinterpret_cast<std::uint32_t*>(0x89bd9cu)=save_b;
            R088NativeContext nt{};nt.parent.gate=gate;nt.child.final_ret=final_ret;std::uint32_t na=R087SystemBase,nb=R088SecondaryBase;const auto nr=runtime_startup_owner_4176e0(ns,desc,funcs,na,nb,{&nt,r088_native_call});
            compare_u32("runtime_startup_owner_return_4176e0_r088",guest_call.out_eax,nr);compare_u32("runtime_startup_owner_primary_4176e0_r088",guest_a,na);compare_u32("runtime_startup_owner_secondary_4176e0_r088",guest_b,nb);
            const auto records=native_r052_records(ns);const auto flags=native_r052_flags(ns);compare_blob("runtime_startup_owner_event_records_4176e0_r088",reinterpret_cast<void*>(R052RecordBase),records);compare_blob("runtime_startup_owner_event_flags_4176e0_r088",reinterpret_cast<void*>(R052FlagsBase),flags);
            const auto gp=r088_guest_trace_blob(),np=r088_trace_blob(nt.parent);compare_blob("runtime_startup_owner_trace_4176e0_r088",gp.data(),np);
            const auto gc=r087_guest_trace_blob(),nc=r087_trace_blob(nt.child);compare_blob("runtime_startup_owner_child_trace_4176e0_r088",gc.data(),nc);
        }


        // r090: execute the original 0x417740 platform-init orchestration with
        // every child/service stubbed.  Compare both the early-failure and
        // success path, all owned globals/scratch/thread state and call order.
        if(enabled("runtime_platform_init_417740_r090")){
            const auto save_window=*reinterpret_cast<std::uint32_t*>(0x8a8c88u),save_resource=*reinterpret_cast<std::uint32_t*>(0x740ca0u),save_g0=*reinterpret_cast<std::uint32_t*>(0x8a8ce0u),save_g1=*reinterpret_cast<std::uint32_t*>(0x8a8cacu),save_g2=*reinterpret_cast<std::uint32_t*>(0x89f684u),save_g3=*reinterpret_cast<std::uint32_t*>(0x89f66cu),save_thread=*reinterpret_cast<std::uint32_t*>(0x955ad8u),save_system=*reinterpret_cast<std::uint32_t*>(0x89bd60u);
            std::array<std::uint32_t,0x75> save_scratch{};std::memcpy(save_scratch.data(),reinterpret_cast<void*>(0x8999c0u),save_scratch.size()*4u);
            PcPlatformInitState417740 ns{};ns.window_token=0x8a000000u^(i*0x101u);ns.resource_token_740ca0=0x74000000u^(i*0x203u);ns.global_8a8ce0=0x11110000u^i;ns.global_8a8cac=0x22220000u^i;ns.global_89f684=0x33330000u^i;ns.global_89f66c=0x44440000u^i;ns.thread_token=0x55550000u^i;for(std::size_t k=0;k<ns.scratch_8999c0.size();++k)ns.scratch_8999c0[k]=0x90000000u^std::uint32_t(k*0x10203u+i);
            *reinterpret_cast<std::uint32_t*>(0x8a8c88u)=ns.window_token;*reinterpret_cast<std::uint32_t*>(0x740ca0u)=ns.resource_token_740ca0;*reinterpret_cast<std::uint32_t*>(0x8a8ce0u)=ns.global_8a8ce0;*reinterpret_cast<std::uint32_t*>(0x8a8cacu)=ns.global_8a8cac;*reinterpret_cast<std::uint32_t*>(0x89f684u)=ns.global_89f684;*reinterpret_cast<std::uint32_t*>(0x89f66cu)=ns.global_89f66c;*reinterpret_cast<std::uint32_t*>(0x955ad8u)=ns.thread_token;*reinterpret_cast<std::uint32_t*>(0x89bd60u)=R087SystemBase;std::memcpy(reinterpret_cast<void*>(0x8999c0u),ns.scratch_8999c0.data(),ns.scratch_8999c0.size()*4u);
            const std::uint32_t gate=i&1u,thread=0x77000000u^(i*0x405u);reset_r090_trace(gate,thread);set_r090_platform_patches(true);prepare(0x417740u);run_original32();set_r090_platform_patches(false);
            const auto gs=r090_guest_state_blob();const auto gt=r090_guest_trace_blob();const auto gr=guest_call.out_eax;
            *reinterpret_cast<std::uint32_t*>(0x8a8c88u)=save_window;*reinterpret_cast<std::uint32_t*>(0x740ca0u)=save_resource;*reinterpret_cast<std::uint32_t*>(0x8a8ce0u)=save_g0;*reinterpret_cast<std::uint32_t*>(0x8a8cacu)=save_g1;*reinterpret_cast<std::uint32_t*>(0x89f684u)=save_g2;*reinterpret_cast<std::uint32_t*>(0x89f66cu)=save_g3;*reinterpret_cast<std::uint32_t*>(0x955ad8u)=save_thread;*reinterpret_cast<std::uint32_t*>(0x89bd60u)=save_system;std::memcpy(reinterpret_cast<void*>(0x8999c0u),save_scratch.data(),save_scratch.size()*4u);
            R090Trace nt{};nt.gate=gate;nt.thread=thread;const auto nr=runtime_platform_init_417740(ns,R087SystemBase,{&nt,r090_native_call});const auto nsb=r090_state_blob(ns);const auto ntb=r090_trace_blob(nt);
            compare_u32("runtime_platform_init_return_417740_r090",gr,nr);compare_blob("runtime_platform_init_state_417740_r090",gs.data(),nsb);compare_blob("runtime_platform_init_trace_417740_r090",gt.data(),ntb);
        }

        // r093: QPC/QPF frame quantizer.  Imported clock queries are deterministic;
        // the original body owns all 64-bit accumulation/division/remainder semantics.
        if(enabled("runtime_frame_ticks_417890_r093")){
            const auto save=r093_guest_state();PcRuntimeTimingState417890 ns{};
            const std::int32_t rates[4]={24,30,60,120};const auto rate=rates[i%4u];
            const std::int64_t freq=10000000ll+std::int64_t(i%17u)*1000ll;
            ns.previous_counter_8a8c90=1000000000ll+std::int64_t(i)*100003ll;
            ns.accumulator_8a8cd0=std::int64_t((i*97u)%10000u);
            const auto quantum=freq/rate;const auto frames=std::int64_t(1u+(i%4u));const auto extra=std::int64_t((i*131u)%std::uint32_t(std::max<std::int64_t>(1,quantum)));
            const std::int64_t current=ns.previous_counter_8a8c90+quantum*frames+extra;
            ns.frequency_8a8c98=-7;ns.current_counter_8a8ca0=-9;ns.gate_8a8cc8=(i%7u)==0u?0u:1u;ns.frame_scale_95af40=-3.25f;
            r093_store64(0x8a8c98u,ns.frequency_8a8c98);r093_store64(0x8a8ca0u,ns.current_counter_8a8ca0);r093_store64(0x8a8c90u,ns.previous_counter_8a8c90);r093_store64(0x8a8cd0u,ns.accumulator_8a8cd0);*reinterpret_cast<std::uint32_t*>(0x8a8cc8u)=ns.gate_8a8cc8;*reinterpret_cast<float*>(0x95af40u)=ns.frame_scale_95af40;
            reset_r093_trace(freq,current);set_r093_timing_patches(true);prepare(0x417890u);Bytes(reinterpret_cast<void*>(S),8).put32(0,std::uint32_t(rate));run_original32();set_r093_timing_patches(false);if(guest_call.out_sp!=S)throw std::runtime_error("r093 timing stack imbalance");const auto gs=r093_state_blob(r093_guest_state());const auto gt=r093_guest_trace_blob();const auto gr=guest_call.out_eax;
            r093_store64(0x8a8c98u,save.frequency_8a8c98);r093_store64(0x8a8ca0u,save.current_counter_8a8ca0);r093_store64(0x8a8c90u,save.previous_counter_8a8c90);r093_store64(0x8a8cd0u,save.accumulator_8a8cd0);*reinterpret_cast<std::uint32_t*>(0x8a8cc8u)=save.gate_8a8cc8;*reinterpret_cast<float*>(0x95af40u)=save.frame_scale_95af40;
            R093Trace nt{};nt.freq=freq;nt.current=current;const auto nr=runtime_frame_ticks_417890(ns,rate,{&nt,r093_native_query});const auto nsb=r093_state_blob(ns);const auto ntb=r093_trace_blob(nt);compare_u32("runtime_frame_ticks_return_417890_r093",gr,nr);compare_blob("runtime_frame_ticks_state_417890_r093",gs.data(),nsb);compare_blob("runtime_frame_ticks_trace_417890_r093",gt.data(),ntb);
        }


        // r094: direct execution of the original central frame block.  It is an
        // extracted basic-block region, not a separately counted PC routine.
        if(enabled("runtime_frame_step_417c7b_r094")){
            const auto save94=r094_guest_state();const auto save93=r093_guest_state();
            PcRuntimeFrameState417c7b ns{};const std::uint32_t modes[5]={0x10u,0x20u,0x24u,0x18u,0x11u};ns.mode_78026c=modes[i%5u];ns.updates_95af48=0xdeadc000u^i;ns.update_index_8a8cdc=0xbeef0000u^i;ns.primary_system_token=R087SystemBase;ns.frame_counter_95af0c=100u+i;ns.elapsed_8a8cb4=-7.0f;ns.slow_frame_flag_8a8cc0=0xa5a50000u^i;ns.slow_frame_count_8a8cc4=i%9u;
            PcRuntimeTimingState417890 ts{};const std::int64_t freq=6000000ll+std::int64_t(i%7u)*600ll;ts.previous_counter_8a8c90=900000000ll+std::int64_t(i)*1009ll;ts.accumulator_8a8cd0=std::int64_t((i*73u)%5000u);ts.gate_8a8cc8=(i%11u)==0u?0u:1u;const auto quantum=freq/60ll;const auto desired=std::int64_t(i%5u);const auto extra=std::int64_t((i*37u)%std::uint32_t(std::max<std::int64_t>(1,quantum)));const std::int64_t current=ts.previous_counter_8a8c90+quantum*desired+extra;
            const std::uint32_t cooperative=i%3u,network=(i>>1u)&1u,present=(i%4u)==0u?0x88760868u:0x12340000u^i;float e1=(i%4u)==0u?18.0f:((i%4u)==1u?17.0f:16.0f);float e2=(i%3u)==0u?0.010f:0.020f;if((i%29u)==0u){const std::uint32_t nan=0x7fc00001u;std::memcpy(&e1,&nan,4);}if((i%31u)==0u){const std::uint32_t nan=0x7fc00002u;std::memcpy(&e2,&nan,4);}
            *reinterpret_cast<std::uint32_t*>(0x78026cu)=ns.mode_78026c;*reinterpret_cast<std::uint32_t*>(0x95af48u)=ns.updates_95af48;*reinterpret_cast<std::uint32_t*>(0x8a8cdcu)=ns.update_index_8a8cdc;*reinterpret_cast<std::uint32_t*>(0x89bd60u)=ns.primary_system_token;*reinterpret_cast<std::uint32_t*>(0x95af0cu)=ns.frame_counter_95af0c;*reinterpret_cast<float*>(0x8a8cb4u)=ns.elapsed_8a8cb4;*reinterpret_cast<std::uint32_t*>(0x8a8cc0u)=ns.slow_frame_flag_8a8cc0;*reinterpret_cast<std::uint32_t*>(0x8a8cc4u)=ns.slow_frame_count_8a8cc4;
            r093_store64(0x8a8c98u,ts.frequency_8a8c98);r093_store64(0x8a8ca0u,ts.current_counter_8a8ca0);r093_store64(0x8a8c90u,ts.previous_counter_8a8c90);r093_store64(0x8a8cd0u,ts.accumulator_8a8cd0);*reinterpret_cast<std::uint32_t*>(0x8a8cc8u)=ts.gate_8a8cc8;*reinterpret_cast<float*>(0x95af40u)=ts.frame_scale_95af40;
            reset_r093_trace(freq,current);reset_r094_trace(cooperative,network,present,e1,e2);const auto save_v44=*reinterpret_cast<std::uint32_t*>(R087VtableBase+0x44u);*reinterpret_cast<std::uint32_t*>(R087VtableBase+0x44u)=R094StubBlock+0x800u;set_r093_timing_patches(true);set_r094_frame_patches(true);prepare(0x417c7bu);guest_call.ebx=0u;run_original32();set_r094_frame_patches(false);set_r093_timing_patches(false);*reinterpret_cast<std::uint32_t*>(R087VtableBase+0x44u)=save_v44;if(guest_call.out_sp!=S)throw std::runtime_error("r094 frame block stack imbalance");const auto gs=r094_state_blob(r094_guest_state());const auto gts=r093_state_blob(r093_guest_state());const auto gt=r094_guest_trace_blob();const auto gct=r093_guest_trace_blob();const auto gr=guest_call.out_eax;
            *reinterpret_cast<std::uint32_t*>(0x78026cu)=save94.mode_78026c;*reinterpret_cast<std::uint32_t*>(0x95af48u)=save94.updates_95af48;*reinterpret_cast<std::uint32_t*>(0x8a8cdcu)=save94.update_index_8a8cdc;*reinterpret_cast<std::uint32_t*>(0x89bd60u)=save94.primary_system_token;*reinterpret_cast<std::uint32_t*>(0x95af0cu)=save94.frame_counter_95af0c;*reinterpret_cast<float*>(0x8a8cb4u)=save94.elapsed_8a8cb4;*reinterpret_cast<std::uint32_t*>(0x8a8cc0u)=save94.slow_frame_flag_8a8cc0;*reinterpret_cast<std::uint32_t*>(0x8a8cc4u)=save94.slow_frame_count_8a8cc4;
            r093_store64(0x8a8c98u,save93.frequency_8a8c98);r093_store64(0x8a8ca0u,save93.current_counter_8a8ca0);r093_store64(0x8a8c90u,save93.previous_counter_8a8c90);r093_store64(0x8a8cd0u,save93.accumulator_8a8cd0);*reinterpret_cast<std::uint32_t*>(0x8a8cc8u)=save93.gate_8a8cc8;*reinterpret_cast<float*>(0x95af40u)=save93.frame_scale_95af40;
            R094Trace nt{};nt.cooperative=cooperative;nt.network=network;nt.present=present;nt.e1=e1;nt.e2=e2;R093Trace nct{};nct.freq=freq;nct.current=current;const PcRuntimeTimingServices417890 tsvc{&nct,r093_native_query};const auto nr=runtime_frame_step_417c7b(ns,{&nt,r094_native_call,r094_native_elapsed,&ts,&tsvc});const auto nsb=r094_state_blob(ns);const auto nts=r093_state_blob(ts);const auto ntb=r094_trace_blob(nt);const auto nctb=r093_trace_blob(nct);
            compare_u32("runtime_frame_step_wait_417c7b_r094",gr,nr.platform_wait_before_next_frame?1u:0u);compare_blob("runtime_frame_step_state_417c7b_r094",gs.data(),nsb);compare_blob("runtime_frame_step_timing_417c7b_r094",gts.data(),nts);compare_blob("runtime_frame_step_trace_417c7b_r094",gt.data(),ntb);compare_blob("runtime_frame_step_clock_trace_417c7b_r094",gct.data(),nctb);
        }

        // r092: setup helper paired with r091 cleanup.  Child calls are
        // deterministic services; the original still owns feature/mode branches,
        // global clears, flag set and tail-return behavior.
        if(enabled("runtime_loop_setup_417a20_r092")){
            const auto save=r092_guest_state();PcRuntimeLoopSetupState417a20 ns{};ns.system_token=R092SystemObj;ns.object_95b218=R092AudioObj;ns.optional_7f94e8=(i&1u)?R092OptionalObj:0u;ns.feature_79fb50=std::uint8_t(i&3u);const std::uint32_t modes[3]={0x11u,0x20u,0x0au};ns.mode_78026c=modes[i%3u];ns.handle_8a89f4=0x11110000u^i;ns.handle_8a8a00=0x22220000u^i;ns.handle_89f680=0x33330000u^i;ns.global_89f684=0x44440000u^i;ns.global_89f66c=0x55550000u^i;ns.flag_73e2b0=std::uint8_t(i);
            *reinterpret_cast<std::uint32_t*>(0x89bd60u)=ns.system_token;*reinterpret_cast<std::uint32_t*>(0x95b218u)=ns.object_95b218;*reinterpret_cast<std::uint32_t*>(0x7f94e8u)=ns.optional_7f94e8;*reinterpret_cast<std::uint8_t*>(0x79fb50u)=ns.feature_79fb50;*reinterpret_cast<std::uint32_t*>(0x78026cu)=ns.mode_78026c;*reinterpret_cast<std::uint32_t*>(0x8a89f4u)=ns.handle_8a89f4;*reinterpret_cast<std::uint32_t*>(0x8a8a00u)=ns.handle_8a8a00;*reinterpret_cast<std::uint32_t*>(0x89f680u)=ns.handle_89f680;*reinterpret_cast<std::uint32_t*>(0x89f684u)=ns.global_89f684;*reinterpret_cast<std::uint32_t*>(0x89f66cu)=ns.global_89f66c;*reinterpret_cast<std::uint8_t*>(0x73e2b0u)=ns.flag_73e2b0;
            const std::uint32_t h1=0xa6000000u^(i*0x101u),h2=0xa9000000u^(i*0x203u),h3=0xab000000u^(i*0x405u),fr=0x92000000u^(i*0x809u);reset_r092_trace(h1,h2,h3,fr);set_r092_setup_patches(true);prepare(0x417a20u);run_original32();set_r092_setup_patches(false);if(guest_call.out_sp!=S)throw std::runtime_error("r092 setup stack imbalance");const auto gs=r092_state_blob(r092_guest_state());const auto gt=r092_guest_trace_blob();const auto gr=guest_call.out_eax;
            *reinterpret_cast<std::uint32_t*>(0x89bd60u)=save.system_token;*reinterpret_cast<std::uint32_t*>(0x95b218u)=save.object_95b218;*reinterpret_cast<std::uint32_t*>(0x7f94e8u)=save.optional_7f94e8;*reinterpret_cast<std::uint8_t*>(0x79fb50u)=save.feature_79fb50;*reinterpret_cast<std::uint32_t*>(0x78026cu)=save.mode_78026c;*reinterpret_cast<std::uint32_t*>(0x8a89f4u)=save.handle_8a89f4;*reinterpret_cast<std::uint32_t*>(0x8a8a00u)=save.handle_8a8a00;*reinterpret_cast<std::uint32_t*>(0x89f680u)=save.handle_89f680;*reinterpret_cast<std::uint32_t*>(0x89f684u)=save.global_89f684;*reinterpret_cast<std::uint32_t*>(0x89f66cu)=save.global_89f66c;*reinterpret_cast<std::uint8_t*>(0x73e2b0u)=save.flag_73e2b0;
            R092Trace nt{};nt.h1=h1;nt.h2=h2;nt.h3=h3;nt.final_ret=fr;const auto nr=runtime_loop_setup_417a20(ns,{&nt,r092_native_call});const auto nsb=r092_state_blob(ns);const auto ntb=r092_trace_blob(nt);compare_u32("runtime_loop_setup_return_417a20_r092",gr,nr);compare_blob("runtime_loop_setup_state_417a20_r092",gs.data(),nsb);compare_blob("runtime_loop_setup_trace_417a20_r092",gt.data(),ntb);
        }

        // r091: cleanup helper used by the runtime loop.  Compare the six owned
        // handle clears, two borrowed objects and normalized virtual/direct call order.
        if(enabled("runtime_loop_cleanup_417970_r091")){
            const std::uint32_t tok[8]={R091ObjectBase+0x00u,R091ObjectBase+0x20u,R091ObjectBase+0x40u,R091ObjectBase+0x60u,R091ObjectBase+0x80u,R091ObjectBase+0xa0u,R091ObjectBase+0xc0u,R091ObjectBase+0xe0u};
            PcRuntimeLoopCleanupState417970 ns{};
            ns.handle_8a89f4=(i&1u)?tok[0]:0u;ns.handle_8a8a00=(i&2u)?tok[1]:0u;ns.handle_95afcc=(i&4u)?tok[2]:0u;ns.handle_95afc4=(i&8u)?tok[3]:0u;ns.handle_95afc8=(i&16u)?tok[4]:0u;ns.object_95b218=tok[5];ns.optional_7f94e8=(i&32u)?tok[6]:0u;ns.handle_89f680=(i&64u)?tok[7]:0u;
            const auto save=r091_guest_state();
            *reinterpret_cast<std::uint32_t*>(0x8a89f4u)=ns.handle_8a89f4;*reinterpret_cast<std::uint32_t*>(0x8a8a00u)=ns.handle_8a8a00;*reinterpret_cast<std::uint32_t*>(0x95afccu)=ns.handle_95afcc;*reinterpret_cast<std::uint32_t*>(0x95afc4u)=ns.handle_95afc4;*reinterpret_cast<std::uint32_t*>(0x95afc8u)=ns.handle_95afc8;*reinterpret_cast<std::uint32_t*>(0x95b218u)=ns.object_95b218;*reinterpret_cast<std::uint32_t*>(0x7f94e8u)=ns.optional_7f94e8;*reinterpret_cast<std::uint32_t*>(0x89f680u)=ns.handle_89f680;
            reset_r091_trace();set_r091_cleanup_patch(true);prepare(0x417970u);run_original32();set_r091_cleanup_patch(false);if(guest_call.out_sp!=S)throw std::runtime_error("r091 cleanup stack imbalance");
            const auto gs=r091_state_blob(r091_guest_state());const auto gt=r091_guest_trace_blob();
            *reinterpret_cast<std::uint32_t*>(0x8a89f4u)=save.handle_8a89f4;*reinterpret_cast<std::uint32_t*>(0x8a8a00u)=save.handle_8a8a00;*reinterpret_cast<std::uint32_t*>(0x95afccu)=save.handle_95afcc;*reinterpret_cast<std::uint32_t*>(0x95afc4u)=save.handle_95afc4;*reinterpret_cast<std::uint32_t*>(0x95afc8u)=save.handle_95afc8;*reinterpret_cast<std::uint32_t*>(0x95b218u)=save.object_95b218;*reinterpret_cast<std::uint32_t*>(0x7f94e8u)=save.optional_7f94e8;*reinterpret_cast<std::uint32_t*>(0x89f680u)=save.handle_89f680;
            R091Trace nt{};runtime_loop_cleanup_417970(ns,{&nt,r091_native_call});const auto nsb=r091_state_blob(ns);const auto ntb=r091_trace_blob(nt);
            compare_blob("runtime_loop_cleanup_state_417970_r091",gs.data(),nsb);compare_blob("runtime_loop_cleanup_trace_417970_r091",gt.data(),ntb);
        }

        if(enabled("ui_resource_tick_4659f0_r080")){
            auto native=r080_object_fixture(i+17u,false);Bytes nb(native.data(),native.size());std::memcpy(reinterpret_cast<void*>(R080ObjectBase),native.data(),native.size());
            const auto status=(i%5u)==0u?1:0;reset_r059_trace(status);reset_r080_trace();
            if(only=="all"){set_r059_patches(true);set_r080_patches(true);}
            prepare(0x4659f0u);guest_call.ecx=R080ObjectBase+0xcf4u;run_original32();
            if(only=="all"){set_r080_patches(false);set_r059_patches(false);}if(guest_call.out_sp!=S)throw std::runtime_error("r080 4659f0 stack imbalance");
            R059Trace nui{};nui.status=status;R080Trace nt{};auto ui=r059_services(nui);PcUiResourceTickServices4659f0 ts{&nt,r080_native_tick};
            ui_resource_tick_4659f0(nb.sub(0xcf4u,nb.size()-0xcf4u),ui,ts);
            compare_blob("ui_resource_tick_object_4659f0_r080",reinterpret_cast<void*>(R080ObjectBase),native);
            auto gr59=r059_guest_trace_blob(),nr59=r059_trace_blob(nui);compare_blob("ui_resource_tick_r059_trace_4659f0_r080",gr59.data(),nr59);
            auto gt=r080_guest_trace_blob(),ntb=r080_trace_blob(nt);compare_blob("ui_resource_tick_trace_4659f0_r080",gt.data(),ntb);
        }
        if(enabled("object_runtime_ui_tick_444840_r080")){
            auto native=r080_object_fixture(i+79u,true);Bytes nb(native.data(),native.size());std::memcpy(reinterpret_cast<void*>(R080ObjectBase),native.data(),native.size());
            std::array<std::uint8_t,0x20u> nh{};Bytes nhb(nh.data(),nh.size());const auto key=0x20u+(i%0x50u);nhb.put32(0x08u,key);std::memset(reinterpret_cast<void*>(R080HandleBase),0,0x20u);*reinterpret_cast<std::uint32_t*>(R080HandleBase+0x08u)=key;
            const auto status=(i%7u)==0u?1:0;reset_r059_trace(status);reset_r080_trace();
            if(only=="all"){set_r059_patches(true);set_r080_patches(true);}
            prepare(0x444840u);guest_call.ecx=R080ObjectBase;run_original32();
            if(only=="all"){set_r080_patches(false);set_r059_patches(false);}if(guest_call.out_sp!=S)throw std::runtime_error("r080 444840 stack imbalance");
            R059Trace nui{};nui.status=status;R080Trace nt{};auto ui=r059_services(nui);PcUiResourceTickServices4659f0 ts{&nt,r080_native_tick};PcRuntimeUiTickServices444840 os{&nt,r080_native_open};
            const PcNativeHandleBinding hb{R080HandleBase,nh.data(),nh.size()};const PcNativeHandleResolver hr{&hb,1u};object_runtime_ui_tick_444840(nb,hr,ui,ts,os);
            compare_blob("object_runtime_ui_tick_object_444840_r080",reinterpret_cast<void*>(R080ObjectBase),native);
            auto gr59=r059_guest_trace_blob(),nr59=r059_trace_blob(nui);compare_blob("object_runtime_ui_tick_r059_trace_444840_r080",gr59.data(),nr59);
            auto gt=r080_guest_trace_blob(),ntb=r080_trace_blob(nt);compare_blob("object_runtime_ui_tick_trace_444840_r080",gt.data(),ntb);
        }
        // r081: direct original embedded slot configurators and their four-slot owner.
        if(enabled("embedded_slot_open_primary_446a80_r081")){
            const auto slot=i%4u;auto native=r081_fixture(i+11u,false);Bytes nb(native.data(),native.size());std::memcpy(reinterpret_cast<void*>(R081ObjectBase),native.data(),native.size());reset_r081_trace();
            if(only=="all")set_r081_patches(true);prepare(0x446a80u);guest_call.ecx=R081ObjectBase;Bytes(reinterpret_cast<void*>(S),8).put32(0,slot);run_original32();if(only=="all")set_r081_patches(false);
            R081Trace nt{};auto sv=r081_services(nt);embedded_slot_open_primary_446a80(nb,slot,sv);
            compare_blob("embedded_slot_open_primary_object_446a80_r081",reinterpret_cast<void*>(R081ObjectBase),native);auto gt=r081_guest_trace_blob(),nbt=r081_trace_blob(nt);compare_blob("embedded_slot_open_primary_trace_446a80_r081",gt.data(),nbt);
        }
        if(enabled("embedded_slot_open_secondary_446b20_r081")){
            const auto slot=i%4u;auto native=r081_fixture(i+37u,false);Bytes nb(native.data(),native.size());std::memcpy(reinterpret_cast<void*>(R081ObjectBase),native.data(),native.size());reset_r081_trace();
            if(only=="all")set_r081_patches(true);prepare(0x446b20u);guest_call.ecx=R081ObjectBase;Bytes(reinterpret_cast<void*>(S),8).put32(0,slot);run_original32();if(only=="all")set_r081_patches(false);
            R081Trace nt{};auto sv=r081_services(nt);embedded_slot_open_secondary_446b20(nb,slot,sv);
            compare_blob("embedded_slot_open_secondary_object_446b20_r081",reinterpret_cast<void*>(R081ObjectBase),native);auto gt=r081_guest_trace_blob(),nbt=r081_trace_blob(nt);compare_blob("embedded_slot_open_secondary_trace_446b20_r081",gt.data(),nbt);
        }
        if(enabled("embedded_slots_tick_446cf0_r081")){
            auto native=r081_fixture(i+73u,true);Bytes nb(native.data(),native.size());std::memcpy(reinterpret_cast<void*>(R081ObjectBase),native.data(),native.size());reset_r081_trace();
            const auto alt=std::uint8_t(i&1u);const auto old_alt=*reinterpret_cast<std::uint8_t*>(0x7d68bcu);*reinterpret_cast<std::uint8_t*>(0x7d68bcu)=alt;
            if(only=="all")set_r081_patches(true);prepare(0x446cf0u);guest_call.ecx=R081ObjectBase;run_original32();if(only=="all")set_r081_patches(false);*reinterpret_cast<std::uint8_t*>(0x7d68bcu)=old_alt;
            R081Trace nt{};auto sv=r081_services(nt);PcUiNotifyGlobals gl{};gl.alternate=alt;embedded_slots_tick_446cf0(nb,gl,sv);
            compare_blob("embedded_slots_tick_object_446cf0_r081",reinterpret_cast<void*>(R081ObjectBase),native);auto gt=r081_guest_trace_blob(),nbt=r081_trace_blob(nt);compare_blob("embedded_slots_tick_trace_446cf0_r081",gt.data(),nbt);
        }
        // r083: direct 0x444530 transition arbiter.  Existing child routines
        // execute as their original bodies; only r071's callback/virtual leaves
        // are deterministic stubs on both original and native sides.
        if(enabled("object_runtime_transition_444530_r083")){
            const auto cfg=r083_case(i);auto native=r071_object_fixture(i+83u);Bytes nb(native.data(),native.size());
            nb.put32(0x0218u,1u);nb.put8(0x0220u,0u);nb.put32(0x0518u,0u);nb.put32(0x0488u,0u);nb.put8(0x048cu,0u);
            const auto old_index=i%8u,new_index=8u+(i%8u);const auto old_guest=r071_handle(old_index),new_guest=r071_handle(new_index);
            nb.put32(0x0490u,cfg.previous_present?old_guest:0u);nb.put8(0x0494u,cfg.previous_flag);
            std::memcpy(reinterpret_cast<void*>(R071ObjectBase),native.data(),native.size());
            *reinterpret_cast<std::uint32_t*>(old_guest+8u)=cfg.previous_state;*reinterpret_cast<std::uint32_t*>(new_guest+8u)=cfg.created_state;
            const auto save_b0=*reinterpret_cast<std::uint32_t*>(0x7d68b0u),save_ac=*reinterpret_cast<std::uint32_t*>(0x7d68acu),save_mode=*reinterpret_cast<std::uint32_t*>(0x836130u);
            const auto save_gate=*reinterpret_cast<std::uint8_t*>(0x7d68d4u),save_req=*reinterpret_cast<std::uint8_t*>(0x7d68cbu),save_active=*reinterpret_cast<std::uint8_t*>(0x7d68d0u);
            r083_set_ready_globals(cfg.runtime_ready);*reinterpret_cast<std::uint8_t*>(0x7d68cbu)=cfg.request;*reinterpret_cast<std::uint8_t*>(0x7d68d0u)=cfg.active;
            r071_fill_callback_table();reset_r071_trace(new_guest,cfg.ready_ret,0);
            if(only=="all")set_r071_patches(true);
            prepare(0x444530u);guest_call.ecx=R071ObjectBase;run_original32();
            if(only=="all")set_r071_patches(false);if(guest_call.out_sp!=S)throw std::runtime_error("r083 444530 stack imbalance");

            std::array<std::uint8_t,0x40u> old_native{},new_native{};Bytes(old_native.data(),old_native.size()).put32(8u,cfg.previous_state);Bytes(new_native.data(),new_native.size()).put32(8u,cfg.created_state);
            const std::array<PcNativeHandleBinding,2> binds{{{old_guest,old_native.data(),old_native.size()},{new_guest,new_native.data(),new_native.size()}}};const PcNativeHandleResolver resolver{binds.data(),binds.size()};
            R071Trace nt{};nt.callback_ret=new_guest;nt.ready_ret=cfg.ready_ret;nt.release_index=0;auto ds=r071_dispatch_services(nt);auto rs=r071_runtime_services(nt);auto os=r071_open_services(nt);
            PcRuntimeControlGlobals rg{};rg.mode_836130=cfg.runtime_ready?1u:0u;PcRuntimeTransitionFlags444530 fl{cfg.request,cfg.active};const auto pairs=pc_object_state_pair_table_r065();const PcObjectStatePairTable pv{pairs.data(),pairs.size()};const auto callbacks=r071_native_callback_table();const PcObjectEventCallbackTable tv{callbacks.data(),callbacks.size()};PcObjectEventModeInputs ei{};
            object_runtime_transition_444530(nb,fl,rg,resolver,pv,tv,ei,ds,rs,os);
            compare_blob("object_runtime_transition_object_444530_r083",reinterpret_cast<void*>(R071ObjectBase),native);
            compare_u32("object_runtime_transition_request_444530_r083",*reinterpret_cast<std::uint8_t*>(0x7d68cbu),fl.request_7d68cb);
            compare_u32("object_runtime_transition_active_444530_r083",*reinterpret_cast<std::uint8_t*>(0x7d68d0u),fl.active_7d68d0);
            auto gt=r071_guest_trace_blob(),ntb=r071_trace_blob(nt);compare_blob("object_runtime_transition_trace_444530_r083",gt.data(),ntb);
            r071_restore_callback_table();*reinterpret_cast<std::uint32_t*>(0x7d68b0u)=save_b0;*reinterpret_cast<std::uint32_t*>(0x7d68acu)=save_ac;*reinterpret_cast<std::uint32_t*>(0x836130u)=save_mode;*reinterpret_cast<std::uint8_t*>(0x7d68d4u)=save_gate;*reinterpret_cast<std::uint8_t*>(0x7d68cbu)=save_req;*reinterpret_cast<std::uint8_t*>(0x7d68d0u)=save_active;
        }
        // r084: final state-2 child.  Parent-side formatter/output leaves are
        // trace stubs; already-closed 434DE0/4459F0 children execute directly.
        if(enabled("runtime_pair_idle_434de0_r084")){
            auto* gm=reinterpret_cast<std::uint8_t*>(std::uintptr_t(R084ManagerBase));std::memset(gm,0,0x2000u);Bytes gb(gm,0x2000u);gb.put32(0x16d4u,(i&1u)?0u:1u);gb.put32(0x16d8u,(i%3u)?0u:2u);
            prepare(0x434de0u);guest_call.ecx=R084ManagerBase;run_original32();compare_u32("runtime_pair_idle_434de0_r084",guest_call.out_eax&0xffu,runtime_pair_idle_434de0(gb));
        }
        if(enabled("object_runtime_open_4459f0_r084")){
            auto native=r071_object_fixture(i+84u);Bytes nb(native.data(),native.size());nb.put32(0x0488u,0u);nb.put8(0x048cu,0u);nb.put8(0x0494u,0u);nb.put8(0x0220u,0u);nb.put32(0x0218u,1u);std::memcpy(reinterpret_cast<void*>(R071ObjectBase),native.data(),native.size());
            const auto flag=std::uint8_t(i&1u);
            const auto new_guest=r071_handle(8u+(i%8u));r071_fill_callback_table();reset_r071_trace(new_guest,(i%5u)?1u:0u,0);reset_r084_trace();
            if(only=="all"){set_r071_patches(true);set_r084_patches(true);}prepare(0x4459f0u);guest_call.ecx=R071ObjectBase;Bytes(reinterpret_cast<void*>(S),8).put32(0,flag);run_original32();if(only=="all"){set_r084_patches(false);set_r071_patches(false);}if(guest_call.out_sp!=S+4u)throw std::runtime_error("r084 4459f0 stack imbalance");
            R071Trace nt{};nt.callback_ret=new_guest;nt.ready_ret=(i%5u)?1u:0u;auto ds=r071_dispatch_services(nt);auto rs=r071_runtime_services(nt);auto os=r071_open_services(nt);R084Trace qt{};auto as=r084_action_services(qt);const auto pairs=pc_object_state_pair_table_r065();PcObjectStatePairTable pv{pairs.data(),pairs.size()};const auto callbacks=r071_native_callback_table();PcObjectEventCallbackTable tv{callbacks.data(),callbacks.size()};PcObjectEventModeInputs ei{};
            object_runtime_open_4459f0(nb,flag,pv,tv,ei,ds,rs,os,as);
            compare_blob("object_runtime_open_object_4459f0_r084",reinterpret_cast<void*>(R071ObjectBase),native);auto gt=r071_guest_trace_blob(),ntb=r071_trace_blob(nt);compare_blob("object_runtime_open_r071_trace_4459f0_r084",gt.data(),ntb);auto qg=r084_guest_trace_blob(),qn=r084_trace_blob(qt);compare_blob("object_runtime_open_action_trace_4459f0_r084",qg.data(),qn);r071_restore_callback_table();
        }
        if(enabled("object_runtime_queue_445a50_r084")){
            const auto cfg=r084_case(i);auto native=r071_object_fixture(i+184u);Bytes nb(native.data(),native.size());nb.put32(0x0488u,0u);nb.put8(0x048cu,0u);nb.put8(0x0494u,cfg.flag494);nb.put8(0x0220u,0u);nb.put32(0x0218u,1u);std::memcpy(reinterpret_cast<void*>(R071ObjectBase),native.data(),native.size());
            std::array<std::uint8_t,0x2000u> current{};Bytes cb(current.data(),current.size());cb.put32(0x218u,cfg.current_state);std::memcpy(reinterpret_cast<void*>(R074CurrentBase),current.data(),current.size());
            auto q=r084_queue_fixture(i,cfg);r084_store_guest_queue(q);*reinterpret_cast<std::uint32_t*>(0x988f40u+0x16d4u)=cfg.manager_a;*reinterpret_cast<std::uint32_t*>(0x988f40u+0x16d8u)=cfg.manager_b;
            const auto save_b0=*reinterpret_cast<std::uint32_t*>(0x7d68b0u);
            const auto save_ac=*reinterpret_cast<std::uint32_t*>(0x7d68acu);
            const auto save_mode=*reinterpret_cast<std::uint32_t*>(0x836130u);
            const auto save_gate=*reinterpret_cast<std::uint8_t*>(0x7d68d4u);r083_set_ready_globals(cfg.runtime_ready);
            std::array<std::uint8_t,R059TableSize> table{};Bytes tb(table.data(),table.size());tb.put32(0x340u*4u,0x84340000u^(i*3u));tb.put32(0x341u*4u,0x84341000u^(i*5u));std::memcpy(reinterpret_cast<void*>(R059TableBase),table.data(),table.size());
            const auto new_guest=r071_handle(8u+(i%8u));r071_fill_callback_table();reset_r071_trace(new_guest,cfg.ready_ret,0);reset_r084_trace();
            if(only=="all"){set_r059_patches(true);set_r071_patches(true);set_r084_patches(true);}prepare(0x445a50u);guest_call.ecx=R071ObjectBase;run_original32();if(only=="all"){set_r084_patches(false);set_r071_patches(false);set_r059_patches(false);}if(guest_call.out_sp!=S)throw std::runtime_error("r084 445a50 stack imbalance");
            std::array<std::uint8_t,0x1700u> manager{};Bytes mb(manager.data(),manager.size());mb.put32(0x16d4u,cfg.manager_a);mb.put32(0x16d8u,cfg.manager_b);PcRuntimeControlGlobals rg{};rg.mode_836130=cfg.runtime_ready?1u:0u;R071Trace nt{};nt.callback_ret=new_guest;nt.ready_ret=cfg.ready_ret;auto ds=r071_dispatch_services(nt);auto rs=r071_runtime_services(nt);auto os=r071_open_services(nt);R084Trace qt{};auto as=r084_action_services(qt);auto qs=r084_queue_services(qt);const auto pairs=pc_object_state_pair_table_r065();PcObjectStatePairTable pv{pairs.data(),pairs.size()};const auto callbacks=r071_native_callback_table();PcObjectEventCallbackTable tv{callbacks.data(),callbacks.size()};PcObjectEventModeInputs ei{};
            object_runtime_queue_445a50(nb,cb,mb,tb,q,rg,pv,tv,ei,ds,rs,os,as,qs);
            compare_blob("object_runtime_queue_object_445a50_r084",reinterpret_cast<void*>(R071ObjectBase),native);auto slots=r084_slots_blob(q);compare_blob("object_runtime_queue_slots_445a50_r084",reinterpret_cast<void*>(0x988f58u),slots);compare_u32("object_runtime_queue_busy_445a50_r084",*reinterpret_cast<std::uint8_t*>(0x98a5f4u),q.busy_98a5f4);compare_u32("object_runtime_queue_out0_445a50_r084",*reinterpret_cast<std::uint32_t*>(0x989320u),q.out_989320);compare_u32("object_runtime_queue_out1_445a50_r084",*reinterpret_cast<std::uint32_t*>(0x989324u),q.out_989324);compare_u32("object_runtime_queue_out2_445a50_r084",*reinterpret_cast<std::uint32_t*>(0x989328u),q.out_989328);compare_u32("object_runtime_queue_out3_445a50_r084",*reinterpret_cast<std::uint32_t*>(0x98932cu),q.out_98932c);
            auto gt=r071_guest_trace_blob(),ntb=r071_trace_blob(nt);compare_blob("object_runtime_queue_r071_trace_445a50_r084",gt.data(),ntb);auto qg=r084_guest_trace_blob(),qn=r084_trace_blob(qt);compare_blob("object_runtime_queue_output_trace_445a50_r084",qg.data(),qn);r071_restore_callback_table();*reinterpret_cast<std::uint32_t*>(0x7d68b0u)=save_b0;*reinterpret_cast<std::uint32_t*>(0x7d68acu)=save_ac;*reinterpret_cast<std::uint32_t*>(0x836130u)=save_mode;*reinterpret_cast<std::uint8_t*>(0x7d68d4u)=save_gate;
        }
        // r049: direct original GamePlCar children / explicit external gateways.
        const auto r049_saved_timeup=*reinterpret_cast<std::uint32_t*>(0x7d394cu);
        const auto r049_saved_nos=*reinterpret_cast<std::uint32_t*>(0x7f8abcu);
        const auto r049_saved_route=*reinterpret_cast<std::uint32_t*>(0x780258u);
        const auto r049_saved_stage_key=*reinterpret_cast<std::uint32_t*>(0x635f2cu);
        const auto r049_saved_stage_value=*reinterpret_cast<std::uint32_t*>(0x635f30u);

        if(enabled("check_slipstream_4a4d20_r050")){
            auto ev=base.e;Bytes oe(ev.data(),ev.size());
            PcCheckSlipStreamInputs in{};in.network_session_active=(i%5u)==0u;
            oe.put32(0x0c,0x00040000u|((i*0x10203u)&0x00ffffffu));
            oe.putf(0x14,float(int(i%17u)-8)*0.25f);oe.putf(0x18,float(int(i%11u)-5)*0.05f);oe.putf(0x1c,float(int(i%19u)-9)*0.3f);
            oe.putf(0x20,0.0f);oe.putf(0x24,0.0f);oe.putf(0x28,-1.0f);
            oe.puti(0x34,(i%13u==0u)?0:255);oe.puti(0x38,(i%17u==0u)?255:int(i%80u));
            oe.put16(0x162,std::uint16_t(std::int16_t((i%9u==0u)?1700:int(i%1200u)-600)));
            oe.put16(0xd46,std::uint16_t(std::int16_t(int(i*577u)%12000-6000)));
            oe.put8(0xe70,std::uint8_t(i%181u));oe.putf(0xe6c,float(i%37u)*0.0025f);oe.put32(0xe78,i%7u);
            const CourseProbe self{oe.f32(0x14),oe.f32(0x18),oe.f32(0x1c)};
            for(std::size_t k=0;k<R050CandidateCount;++k){
                auto& c=in.candidates[k];c.open_state=((k+i)%11u==0u)?1u:2u;c.flags=((k+i)%17u==0u)?0x40u:0u;c.network_state=((k+i)%19u==0u)?1u:0u;
                const float side=float(int((k*7u+i)%9u)-4)*0.15f;const float dist=8.0f+float((k*5u+i)%47u);
                c.position={self.x+side,self.y+float(int(k%3u)-1)*0.03f,self.z-dist};
                c.direction={(k+i)%13u==0u?0.15f:0.0f,0.0f,-1.0f};
                c.speed=((k+i)%7u==0u)?0.7f:(2.0f+float(k%5u)*0.2f);c.cooldown=std::int16_t(int((k*13u+i)%220u)-20);
            }
            auto native=ev;Bytes ne(native.data(),native.size());
            std::memcpy(reinterpret_cast<void*>(E),ev.data(),ev.size());load_r050_slipstream_original(in);
            prepare(0x4a4d20u);guest_call.esi=E;run();check_slipstream_4a4d20(ne,in);
            compare_blob("check_slipstream_event",reinterpret_cast<void*>(E),native);
            const auto actual_cd=r050_original_cooldowns(),native_cd=r050_native_cooldowns(in);
            compare_blob("check_slipstream_cooldowns",actual_cd.data(),native_cd);
        }
        if(enabled("pas_pl_car_ctrl_475720_r051")){
            Event ev{};std::mt19937 rg(0x47572051u^(i*2654435761u));for(auto& b:ev)b=std::uint8_t(rg());Bytes ie(ev.data(),ev.size());
            const std::uint8_t scenes[6]={0u,3u,8u,14u,15u,31u};PcPasPlCarInputs in{};in.petty_auto_scene_list=scenes[i%6u];
            ie.put32(0x04,ie.u32(0x04)&~0x00800000u);ie.put32(0x5c,0x400u+(i%3u));
            for(unsigned k=0;k<4;++k){
                const std::size_t off=0x130u+k*12u;
                ie.putf(off+0,float(int((i+k*5u)%29u)-14)*0.35f);
                ie.putf(off+4,float(int((i+k*7u)%31u)-15)*0.2f);
                ie.putf(off+8,float(int((i+k*11u)%37u)-18)*0.3f);
                auto& w=in.wheel[k];
                w.transformed_point={ie.f32(off)+0.25f*float(k+1u),ie.f32(off+4)+0.5f*float(k+1u),ie.f32(off+8)-0.125f*float(k+1u)};
                w.road_y=w.transformed_point.y+float(int((i+k)%9u)-4)*0.125f;
                w.road_polygon=0x1000u+i*17u+k*0x101u;
                w.road_result=((i+k)%5u==0u)?1u:(((i+k)%3u)+2u);
                w.tire_position={float(k)*0.1f,0.25f+float((i+k)%7u)*0.075f,-float(k)*0.2f};
                w.road_normal={0.05f*float(k),1.0f-0.03f*float(k),-0.07f*float(k)};
            }
            std::array<std::array<std::uint8_t,0x100>,4> records{};for(unsigned k=0;k<4;++k)for(auto& b:records[k])b=std::uint8_t(rg());
            auto native_ev=ev;std::array<std::array<std::uint8_t,0x100>,4> native_records=records;std::array<Bytes,4> nr={Bytes(native_records[0].data(),0x100),Bytes(native_records[1].data(),0x100),Bytes(native_records[2].data(),0x100),Bytes(native_records[3].data(),0x100)};
            std::memcpy(reinterpret_cast<void*>(E),ev.data(),ev.size());reset_r051_parent_state(in,records);set_r051_parent_patches(true);
            prepare(0x475720u);Bytes(reinterpret_cast<void*>(S),16).put32(0,E);run();const auto pc_trace=r051_parent_trace();set_r051_parent_patches(false);
            struct PT{std::vector<std::uint32_t> v;} pt;auto cb=[](void* u,std::uint32_t pc,std::uint32_t){static_cast<PT*>(u)->v.push_back(pc);};
            pas_pl_car_ctrl_475720(Bytes(native_ev.data(),native_ev.size()),nr,in,{&pt,cb});
            compare_blob("pas_pl_car_event",reinterpret_cast<void*>(E),native_ev);
            for(unsigned k=0;k<4;++k){compare_blob("pas_pl_car_record_"+std::to_string(k),reinterpret_cast<void*>(std::uintptr_t(R051RecordBase+k*0x100u)),native_records[k]);}
            compare_u32("pas_pl_car_stub_index",pas_stub_index,4u);compare_u32("pas_pl_car_stub_overflow",pas_stub_overflow,0u);
            compare_u32("pas_pl_car_trace_count",std::uint32_t(pc_trace.size()),std::uint32_t(pt.v.size()));const auto tn=std::min(pc_trace.size(),pt.v.size());for(std::size_t k=0;k<tn;++k)compare_u32("pas_pl_car_trace_"+std::to_string(k),pc_trace[k],pt.v[k]);
            for(unsigned k=0;k<4;++k){
                const std::size_t off=0x130u+k*12u;
                compare_u32("pas_transform_src_x_"+std::to_string(k),pas_seen_transform_x[k],fbits(ie.f32(off)));
                compare_u32("pas_transform_src_y_"+std::to_string(k),pas_seen_transform_y[k],fbits(ie.f32(off+4)));
                compare_u32("pas_transform_src_z_"+std::to_string(k),pas_seen_transform_z[k],fbits(ie.f32(off+8)));
                compare_u32("pas_road_seen_x_"+std::to_string(k),pas_seen_road_x[k],fbits(in.wheel[k].transformed_point.x));
                compare_u32("pas_road_seen_y_"+std::to_string(k),pas_seen_road_y[k],fbits(in.wheel[k].transformed_point.y));
                compare_u32("pas_road_seen_z_"+std::to_string(k),pas_seen_road_z[k],fbits(in.wheel[k].transformed_point.z));
                if(in.wheel[k].road_result!=1u){
                    compare_u32("pas_tire_index_"+std::to_string(k),pas_seen_tire_index[k],k);
                    compare_u32("pas_normal_polygon_"+std::to_string(k),pas_seen_normal_polygon[k],in.wheel[k].road_polygon);
                    compare_u32("pas_normal_type_"+std::to_string(k),pas_seen_normal_type[k],ie.u32(0x5c));
                }
            }
        }
        if(enabled("control_timeup_braking_49fb70_r049")){
            Event ev{};Work wk{};Params pa{};std::mt19937 rg(0x49fb7049u^(i*2654435761u));for(auto& b:ev)b=std::uint8_t(rg());for(auto& b:wk)b=std::uint8_t(rg());for(auto& b:pa)b=std::uint8_t(rg());
            Bytes e49(ev.data(),ev.size()),w49(wk.data(),wk.size()),p49(pa.data(),pa.size());
            const std::int32_t counter=int(i%76u)-7;e49.put32(0x2b4,P);e49.putf(0x1c4,float(int(i%401u)-200)*0.75f);e49.putf(0x1c8,float(int(i%41u)-20)*0.01f);e49.puti(0x38,int(i%20u)-3);e49.puti(0x34,int(i));e49.put16(0x202,std::uint16_t(i*97u));e49.put16(0xd4c,std::uint16_t(std::int16_t(int(i*977u)%65536-32768)));p49.putf(0x17c0,0.25f+float(i%30u)*0.2f);
            w49.putf(0x5c,float(int(i%31u)-15)*0.7f);w49.putf(0x60,float(int(i%17u)-8)*0.25f);w49.putf(0x64,float(int(i%23u)-11)*0.55f);w49.putf(0x628,0.0f);w49.putf(0x62c,1.0f);w49.putf(0x630,0.0f);
            auto nev=ev; auto nwk=wk;Bytes ne(nev.data(),nev.size()),nw(nwk.data(),nwk.size()),np(pa.data(),pa.size());
            std::memcpy(reinterpret_cast<void*>(E),ev.data(),ev.size());std::memcpy(reinterpret_cast<void*>(W),wk.data(),wk.size());std::memcpy(reinterpret_cast<void*>(P),pa.data(),pa.size());*reinterpret_cast<std::uint32_t*>(0x7d394cu)=std::uint32_t(counter);
            prepare(0x49fb70u);guest_call.eax=E;Bytes(reinterpret_cast<void*>(S),16).put32(0,W);run();control_timeup_braking_49fb70(ne,nw,np,counter);
            compare_blob("control_timeup_braking_event",reinterpret_cast<void*>(E),nev);compare_blob("control_timeup_braking_work",reinterpret_cast<void*>(W),nwk);
        }
        if(enabled("check_wanderer_4a5260_r049")){
            Event ev{};Work wk{};std::mt19937 rg(0x4a526049u^(i*2246822519u));for(auto& b:ev)b=std::uint8_t(rg());for(auto& b:wk)b=std::uint8_t(rg());Bytes e49(ev.data(),ev.size()),w49(wk.data(),wk.size());
            PcCheckWandererInputs in{};in.route_state=(i%11u==0u)?0:1;in.stage_level=int(i&1u);const std::uint32_t key=0x40000000u+i*13u;e49.put32(0x68,key);e49.put32(0x244,(i%4u==0u||i%4u==3u)?0u:1u);e49.put32(0xe90,(i%4u==2u)?1u:0u);e49.puti(0xe88,int(i%131u));e49.putf(0x26c,float(int(i%31u)-15)*0.05f);e49.put16(0xd4c,std::uint16_t(std::int16_t(int(i*613u)%65536-32768)));e49.putf(0x264,0.5f+float(i%19u)*0.2f);e49.putf(0x268,0.75f+float(i%23u)*0.15f);e49.put8(0x283,(i%13u==0u)?1u:0u);e49.putf(0x2c8,(i%17u==0u)?1.0f:0.0f);e49.putf(0x2f8,(i%19u==0u)?1.0f:0.0f);e49.put8(0xd36,(i%29u==0u)?1u:0u);
            w49.putf(0x5c,1.0f+float(i%7u)*0.3f);w49.putf(0x60,float(int(i%5u)-2)*0.1f);w49.putf(0x64,float(int(i%9u)-4)*0.25f);w49.putf(0x628,0.0f);w49.putf(0x62c,1.0f);w49.putf(0x630,0.0f);
            auto nev=ev;Bytes ne(nev.data(),nev.size()),nw(wk.data(),wk.size());std::memcpy(reinterpret_cast<void*>(E),ev.data(),ev.size());std::memcpy(reinterpret_cast<void*>(W),wk.data(),wk.size());*reinterpret_cast<std::uint32_t*>(0x780258u)=std::uint32_t(in.route_state);*reinterpret_cast<std::uint32_t*>(0x635f2cu)=key;*reinterpret_cast<std::uint32_t*>(0x635f30u)=std::uint32_t(in.stage_level);
            prepare(0x4a5260u);guest_call.esi=E;guest_call.eax=W;run();check_wanderer_4a5260(ne,nw,in);compare_blob("check_wanderer_event",reinterpret_cast<void*>(E),nev);
        }
        if(enabled("ham_nos_set_speed_4a5650_r049")){
            Event ev{};Work wk{};Params pa{};std::mt19937 rg(0x4a565049u^(i*3266489917u));for(auto& b:ev)b=std::uint8_t(rg());for(auto& b:wk)b=std::uint8_t(rg());for(auto& b:pa)b=std::uint8_t(rg());Bytes e49(ev.data(),ev.size()),w49(wk.data(),wk.size()),p49(pa.data(),pa.size());
            const float nos=(i%9u==0u)?0.0f:((i%13u==0u)?-1.0f:(0.25f+float(i%80u)*0.125f));e49.put32(0x2b4,P);e49.putf(0x178,float(i%40u)*0.4f);e49.put32(0x1f4,(i&1u)?1u:0u);e49.putf(0x20,1.0f+float(i%7u));e49.putf(0x24,float(int(i%9u)-4)*0.25f);e49.putf(0x28,-2.0f-float(i%5u));e49.put16(0x2e,std::uint16_t(std::int16_t(int(i*811u)%65536-32768)));p49.put32(0x10a0,1u+(i%8u));p49.putf(0x1644,float(int(i%200u)-50)*0.5f);p49.putf(0xb48,1.0f+float(i%11u)*0.25f);p49.putf(0xb94,1.25f+float(i%13u)*0.2f);
            auto nev=ev; auto nwk=wk;Bytes ne(nev.data(),nev.size()),nw(nwk.data(),nwk.size()),np(pa.data(),pa.size());std::memcpy(reinterpret_cast<void*>(E),ev.data(),ev.size());std::memcpy(reinterpret_cast<void*>(W),wk.data(),wk.size());std::memcpy(reinterpret_cast<void*>(P),pa.data(),pa.size());*reinterpret_cast<float*>(0x7f8abcu)=nos;
            prepare(0x4a5650u);guest_call.esi=E;guest_call.edi=W;run();ham_nos_set_speed_4a5650(ne,nw,np,nos);compare_blob("ham_nos_set_speed_event",reinterpret_cast<void*>(E),nev);compare_blob("ham_nos_set_speed_work",reinterpret_cast<void*>(W),nwk);
        }
        if(enabled("network_tail_state_55a930_r049")){
            Work wk{};Bytes w49(wk.data(),wk.size());const std::uint32_t v=0x10203040u^(i*0x01010101u);w49.put32(0x60,v);std::memcpy(reinterpret_cast<void*>(W),wk.data(),wk.size());prepare(0x55a930u);guest_call.ecx=W;run();compare_u32("network_tail_state",guest_call.out_eax,network_tail_state_55a930(w49));
        }
        if(enabled("network_tail_forward_46c390_r049")){
            reset_r049_network_state();set_r049_network_patch(true);prepare(0x46c390u);Bytes st(reinterpret_cast<void*>(S),16);st.put32(0,E);st.put32(4,W);run();set_r049_network_patch(false);
            struct NF{std::uint32_t calls{};std::size_t a{},b{};} nf;auto cb=[](void* u,Bytes a,Bytes b){auto* q=static_cast<NF*>(u);++q->calls;q->a=a.size();q->b=b.size();};Event ev{};Work wk{};network_tail_forward_46c390(Bytes(ev.data(),ev.size()),Bytes(wk.data(),wk.size()),{&nf,cb});
            compare_u32("network_tail_forward_calls",*reinterpret_cast<std::uint32_t*>(R049StateBase+0x00u),nf.calls);compare_u32("network_tail_forward_arg0",*reinterpret_cast<std::uint32_t*>(R049StateBase+0x04u),E);compare_u32("network_tail_forward_arg1",*reinterpret_cast<std::uint32_t*>(R049StateBase+0x08u),W);compare_u32("network_tail_forward_manager",*reinterpret_cast<std::uint32_t*>(R049StateBase+0x0cu),0x7f9460u);compare_u32("network_tail_forward_native_sizes",std::uint32_t(nf.a+nf.b),std::uint32_t(event_size+work_size));
        }
        if(enabled("rank_provider_gateway_45a2b0_r049")&&g_steam_exe){
            // Steam build: 45A2B0 is plain code. Mode / variant / 7DF118 inputs drive it;
            // the LAN branch (mode 16, variants 3/4) is left to the network port.
            auto* p7df=reinterpret_cast<void*>(0x7df000u);if(mprotect(p7df,4096,PROT_READ|PROT_WRITE))throw std::runtime_error("r049 rank 7DF page");
            const std::uint8_t id=std::uint8_t(i%8u);const std::uint32_t mode=(i%3u==0u)?0x10u:((i%3u==1u)?0x0du:0x13u);
            std::uint32_t variant=i%11u;if(mode==0x10u&&(variant==3u||variant==4u))variant=5u;
            const auto saved_mode=*reinterpret_cast<std::uint32_t*>(0x78026cu),saved_variant=*reinterpret_cast<std::uint32_t*>(0x780258u);
            *reinterpret_cast<std::uint32_t*>(0x78026cu)=mode;*reinterpret_cast<std::uint32_t*>(0x780258u)=variant;
            std::array<std::uint8_t,8> ranks{};for(unsigned k=0;k<8;++k)ranks[k]=std::uint8_t(0x30u+i+7u*k);
            std::memcpy(reinterpret_cast<void*>(0x7df118u),ranks.data(),8);
            prepare(0x45a2b0u);Bytes(reinterpret_cast<void*>(S),16).put32(0,id);run();
            *reinterpret_cast<std::uint32_t*>(0x78026cu)=saved_mode;*reinterpret_cast<std::uint32_t*>(0x780258u)=saved_variant;
            if(mprotect(p7df,4096,PROT_READ))throw std::runtime_error("r049 rank 7DF page restore");
            PcRankProviderServices sv{};sv.game_mode_78026c=mode;sv.game_variant_780258=variant;sv.ranks_7df118=Bytes(ranks.data(),ranks.size());
            compare_u32("rank_provider_gateway",guest_call.out_eax&0xffu,rank_provider_gateway_45a2b0(id,sv));
        }else if(enabled("rank_provider_gateway_45a2b0_r049")){
            auto* rank_page=reinterpret_cast<void*>(0x01039000u);if(mprotect(rank_page,4096,PROT_READ|PROT_WRITE|PROT_EXEC))throw std::runtime_error("r049 rank page mprotect");const auto saved=*reinterpret_cast<std::uint32_t*>(0x01039cccu);*reinterpret_cast<std::uint32_t*>(0x01039cccu)=R049StubBlock+0x040u;
            const std::uint8_t id=std::uint8_t(i);prepare(0x45a2b0u);Bytes(reinterpret_cast<void*>(S),16).put32(0,id);run();*reinterpret_cast<std::uint32_t*>(0x01039cccu)=saved;const int restore_prot=protected_bridge_mapped?(PROT_READ|PROT_WRITE|PROT_EXEC):PROT_READ;if(mprotect(rank_page,4096,restore_prot))throw std::runtime_error("r049 rank page restore");
            struct RF{} rf;auto cb=[](void*,std::uint8_t player)->std::uint32_t{return 0x12340007u+3u*player;};compare_u32("rank_provider_gateway",guest_call.out_eax&0xffu,rank_provider_gateway_45a2b0(id,{&rf,cb}));
        }
        set_r049_network_patch(false);
        *reinterpret_cast<std::uint32_t*>(0x7d394cu)=r049_saved_timeup;*reinterpret_cast<std::uint32_t*>(0x7f8abcu)=r049_saved_nos;*reinterpret_cast<std::uint32_t*>(0x780258u)=r049_saved_route;*reinterpret_cast<std::uint32_t*>(0x635f2cu)=r049_saved_stage_key;*reinterpret_cast<std::uint32_t*>(0x635f30u)=r049_saved_stage_value;

        if(enabled("game_flag_43f9c0_r047")){
            PcGameControlGlobals st{};st.flag_780248=std::uint8_t(i*37u);*reinterpret_cast<std::uint8_t*>(0x780248u)=st.flag_780248;
            prepare(0x43f9c0u);run();compare_u32("game_flag_43f9c0",guest_call.out_eax,game_flag_43f9c0(st));
        }
        if(enabled("set_game_flag_43f9d0_r047")){
            PcGameControlGlobals st{};st.flag_780248=std::uint8_t(i*13u);*reinterpret_cast<std::uint8_t*>(0x780248u)=st.flag_780248;const auto v=std::uint8_t(i*71u+3u);
            prepare(0x43f9d0u);Bytes(reinterpret_cast<void*>(S),16).put32(0,v);run();set_game_flag_43f9d0(st,v);compare_u32("set_game_flag_43f9d0",*reinterpret_cast<std::uint8_t*>(0x780248u),st.flag_780248);
        }
        if(enabled("set_game_state_byte_43f9e0_r047")){
            PcGameControlGlobals st{};st.flag_780270=std::uint8_t(i*11u);*reinterpret_cast<std::uint8_t*>(0x780270u)=st.flag_780270;const auto v=std::uint8_t(i*53u+5u);
            prepare(0x43f9e0u);Bytes(reinterpret_cast<void*>(S),16).put32(0,v);run();set_game_state_byte_43f9e0(st,v);compare_u32("set_game_state_byte_43f9e0",*reinterpret_cast<std::uint8_t*>(0x780270u),st.flag_780270);
        }
        if(enabled("game_state_byte_43f9f0_r047")){
            PcGameControlGlobals st{};st.flag_780270=std::uint8_t(i*29u);*reinterpret_cast<std::uint8_t*>(0x780270u)=st.flag_780270;prepare(0x43f9f0u);run();compare_u32("game_state_byte_43f9f0",guest_call.out_eax,game_state_byte_43f9f0(st));
        }
        if(enabled("game_state_dword_43fa00_r047")){
            PcGameControlGlobals st{};st.value_780278=0x9e3779b9u*i^0xa55a1234u;*reinterpret_cast<std::uint32_t*>(0x780278u)=st.value_780278;prepare(0x43fa00u);run();compare_u32("game_state_dword_43fa00",guest_call.out_eax,game_state_dword_43fa00(st));
        }
        if(enabled("set_game_state_dword_43fa10_r047")){
            PcGameControlGlobals st{};st.value_780278=0x12340000u+i;*reinterpret_cast<std::uint32_t*>(0x780278u)=st.value_780278;const auto v=0xdead0000u^(i*0x10203u);prepare(0x43fa10u);Bytes(reinterpret_cast<void*>(S),16).put32(0,v);run();set_game_state_dword_43fa10(st,v);compare_u32("set_game_state_dword_43fa10",*reinterpret_cast<std::uint32_t*>(0x780278u),st.value_780278);
        }
        if(enabled("game_broadcast_43cc20_r047")){
            PcGameBroadcastState st{};for(unsigned k=0;k<15;++k){st.slots[k]=0x11110000u+k+i;*reinterpret_cast<std::uint32_t*>(0x7801a8u+k*4u)=st.slots[k];}st.state_780240=0xabcdef01u;st.value_78023c=0x23456789u;*reinterpret_cast<std::uint32_t*>(0x780240u)=st.state_780240;*reinterpret_cast<std::uint32_t*>(0x78023cu)=st.value_78023c;
            const auto v=0x80000000u^(i*0x01010101u);prepare(0x43cc20u);Bytes(reinterpret_cast<void*>(S),16).put32(0,v);run();game_broadcast_43cc20(st,v);for(unsigned k=0;k<15;++k)compare_u32("game_broadcast_slot",*reinterpret_cast<std::uint32_t*>(0x7801a8u+k*4u),st.slots[k]);compare_u32("game_broadcast_zero",*reinterpret_cast<std::uint32_t*>(0x780240u),st.state_780240);compare_u32("game_broadcast_value",*reinterpret_cast<std::uint32_t*>(0x78023cu),st.value_78023c);
        }
        if(enabled("operation_input_49fad0_r047")){
            auto x=base;auto native=x.e;Bytes ne(native.data(),native.size());PcOperationInputInputs in{};in.game_mode=(i%17u==0)?0x0d:((i%19u==0)?0x12:0);in.volume0=int((i*17u)%300u)-20;in.volume1=int((i*31u)%300u)-20;in.volume2=int((i*43u)%300u)-20;ne.put16(0xd50,std::uint16_t(std::int16_t(int(i%180u)-20)));ne.putf(0x2f8,(i%7u==0)?1.0f:((i%11u==0)?std::numeric_limits<float>::quiet_NaN():-0.5f));std::memcpy(reinterpret_cast<void*>(E),native.data(),native.size());*reinterpret_cast<std::int32_t*>(0x78026cu)=in.game_mode;set_r047_volumes(in.volume0,in.volume1,in.volume2);prepare(0x49fad0u);guest_call.esi=E;run();operation_input_49fad0(ne,in);compare_blob("operation_input",reinterpret_cast<void*>(E),native);
        }
        if(enabled("check_shift_warning_4a50f0_r047")){
            auto ev=base.e;auto pa=base.p;Bytes ne(ev.data(),ev.size()),np(pa.data(),pa.size());PcShiftWarningInputs in{int((i*37u)%260u),int((i*19u)%20u)-10};const std::uint32_t gear=i%6u;ne.put8(0x13,1);ne.put8(0x282,(i%31u)==0u);ne.put8(0x283,(i%37u)==0u);ne.putf(0x2c8,(i%41u)==0u?1.0f:0.0f);ne.putf(0x2f8,(i%43u)==0u?1.0f:0.0f);ne.puti(0xe84,0);ne.put32(0x208,gear);np.put32(0x10a0,6);ne.putf(0x1c4,7000.0f+float(i%100u));for(unsigned k=0;k<7;++k)ne.putf(0xe10u+k*4u,6500.0f+float(k*50u));ne.puti(0x3c,255);ne.put8(0xd36,0);ne.puti(0xe7c,int(i%500u)-10);ne.put8(0x296,(i%47u)==0u);ne.put32(0x2b4,P);std::memcpy(reinterpret_cast<void*>(E),ev.data(),ev.size());std::memcpy(reinterpret_cast<void*>(P),pa.data(),pa.size());set_r047_volumes(0,in.volume1,in.volume2);prepare(0x4a50f0u);guest_call.esi=E;run();check_shift_warning_4a50f0(ne,np,in);compare_blob("check_shift_warning",reinterpret_cast<void*>(E),ev);
        }
        if(enabled("car_calc_total_cs_len_455f50_r047")){
            auto ev=base.e;Bytes ne(ev.data(),ev.size());std::array<std::uint8_t,64u*0x24u> hist{};for(std::size_t k=0;k<hist.size();++k)hist[k]=std::uint8_t(k*17u+i);auto nh=hist;const auto counter=0x12340000u+i*97u;ne.putf(0x14,float(int(i%200)-100)*1.25f);ne.putf(0x1c,float(int(i%160)-80)*0.75f);ne.putf(0x16c,float(int(i%40)-20));ne.putf(0x174,float(int(i%50)-25));ne.put16(0x2e,std::uint16_t(std::int16_t(i*101u)));ne.put16(0x17e,std::uint16_t(std::int16_t(i*59u)));ne.put32(4,i*0x40000u);ne.put16(0x260,std::uint16_t(i*211u));ne.put32(0x5c,i%4u);ne.put8(0x10,std::uint8_t(i*7u));std::memcpy(reinterpret_cast<void*>(E),ev.data(),ev.size());std::memcpy(reinterpret_cast<void*>(0x7df350u),hist.data(),hist.size());*reinterpret_cast<std::uint32_t*>(0x7f1938u)=counter;prepare(0x455f50u);Bytes(reinterpret_cast<void*>(S),16).put32(0,E);run();car_calc_total_cs_len_455f50(ne,Bytes(nh.data(),nh.size()),counter);compare_blob("car_calc_total_cs_len",reinterpret_cast<void*>(0x7df350u),nh);
        }
        if(enabled("car_calc_current_stage_progress_4a2130_r047")){
            auto ev=base.e;Bytes ne(ev.data(),ev.size());PcStageProgressInputs in{};in.course_end0=std::uint16_t(1000u+(i*17u)%30000u);in.course_end1=std::uint16_t(1200u+(i*19u)%30000u);in.global_gate=(i&1u)!=0u;in.record_gate=(i%3u)!=0u;ne.put32(0x5c,i%3u);ne.put16(0x64,std::uint16_t(i*977u));ne.put16(0x260,std::uint16_t(i*977u+(i%5u==0?20000u:37u)));ne.put16(0x25e,std::uint16_t(i*31u));ne.put8(4,std::uint8_t(i&1u));std::memcpy(reinterpret_cast<void*>(E),ev.data(),ev.size());PcStageProgressHistory nh{};nh.count=i%32u;for(unsigned k=0;k<30;++k){nh.position[k]=std::uint16_t(k*101u+i);nh.offset[k]=std::uint16_t(k*79u+i);*reinterpret_cast<std::uint16_t*>(0x841b50u+k*4u)=nh.position[k];*reinterpret_cast<std::uint16_t*>(0x841b52u+k*4u)=nh.offset[k];}*reinterpret_cast<std::uint32_t*>(0x680bd0u)=nh.count;*reinterpret_cast<std::uint32_t*>(0x80fb14u)=in.global_gate?1u:0u;set_r047_progress_services(in.course_end0,in.course_end1,in.record_gate);prepare(0x4a2130u);Bytes(reinterpret_cast<void*>(S),16).put32(0,E);run();car_calc_current_stage_progress_4a2130(ne,in,nh);compare_blob("car_calc_current_stage_progress_event",reinterpret_cast<void*>(E),ev);compare_u32("stage_progress_count",*reinterpret_cast<std::uint32_t*>(0x680bd0u),nh.count);for(unsigned k=0;k<30;++k){compare_u32("stage_progress_position",*reinterpret_cast<std::uint16_t*>(0x841b50u+k*4u),nh.position[k]);compare_u32("stage_progress_offset",*reinterpret_cast<std::uint16_t*>(0x841b52u+k*4u),nh.offset[k]);}
        }
        if(enabled("get_now_heart_calc_mode_45c440_r047")){
            const auto v=0xa5a50000u^(i*0x12345u);*reinterpret_cast<std::uint32_t*>(0x7f2428u)=v;prepare(0x45c440u);run();compare_u32("get_now_heart_calc_mode",guest_call.out_eax,get_now_heart_calc_mode_45c440(v));
        }
        if(enabled("set_old_param_buffer_4a2ee0_r047")){
            auto ev=base.e;Bytes ne(ev.data(),ev.size());for(unsigned k=0;k<24;++k)ne.put16(0x190u+k*2u,std::uint16_t(std::int16_t(int(i*37u+k*503u))));ne.put16(0xc2c,std::uint16_t(i*113u));ne.put16(0x162,std::uint16_t(i*79u));ne.put8(4,std::uint8_t(i&1u));ne.put8(0xd36,std::uint8_t(std::int8_t(int(i%7u)-3)));ne.put32(0x0c,0x55aa55aau^i);std::memcpy(reinterpret_cast<void*>(E),ev.data(),ev.size());prepare(0x4a2ee0u);Bytes(reinterpret_cast<void*>(S),16).put32(0,E);run();set_old_param_buffer_4a2ee0(ne);compare_blob("set_old_param_buffer",reinterpret_cast<void*>(E),ev);
        }

        if(enabled("platform_flag_bit2_44ff10_r044")){
            const std::uint32_t flags=0x9e3779b9u*i ^ (i<<2u);set_r044_helper_state(flags,false,0);
            prepare(0x44ff10u);run();compare_u32("platform_flag_bit2",guest_call.out_eax,platform_flag_bit2_44ff10(flags)?1u:0u);
        }
        if(enabled("platform_index_value_4505a0_r044")){
            std::array<std::uint8_t,64u*4u> table{};Bytes tv(table.data(),table.size());
            for(unsigned k=0;k<64;++k){const auto v=0xa5000000u^(i*0x101u)^(k*0x10203u);tv.put32(k*4u,v);*reinterpret_cast<std::uint32_t*>(0x7d3954u+k*4u)=v;}
            const std::uint32_t idx=(i*13u)%64u;prepare(0x4505a0u);Bytes(reinterpret_cast<void*>(S),16).put32(0,idx);run();
            compare_u32("platform_index_value",guest_call.out_eax,platform_index_value_4505a0(tv,idx));
        }
        if(enabled("platform_pair_value_450630_r044")){
            std::array<std::uint8_t,128u*4u> table{};Bytes tv(table.data(),table.size());
            for(unsigned k=0;k<128;++k){const auto v=0x5a000000u^(i*0x10001u)^(k*0x20305u);tv.put32(k*4u,v);*reinterpret_cast<std::uint32_t*>(0x7d3748u+k*4u)=v;}
            const std::uint32_t row=(i*7u)%24u,col=(i*11u)%4u;prepare(0x450630u);Bytes st(reinterpret_cast<void*>(S),16);st.put32(0,row);st.put32(4,col);run();
            compare_u32("platform_pair_value",guest_call.out_eax,platform_pair_value_450630(tv,row,col));
        }
        if(enabled("platform_slot_kind_450750_r044")){
            const bool gate=(i&1u)!=0u;const std::int32_t slot=int(i%20u)-2;set_r044_helper_state(0,gate,0);
            prepare(0x450750u);Bytes(reinterpret_cast<void*>(S),16).puti(0,slot);run();compare_u32("platform_slot_kind",guest_call.out_eax,platform_slot_kind_450750(slot,gate)?1u:0u);
        }
        if(enabled("platform_counter_lt_60_48b350_r044")){
            static constexpr std::int32_t vals[]={-2147483647-1,-1,0,1,58,59,60,61,100,2147483647};const auto v=vals[i%std::size(vals)];set_r044_helper_state(0,false,v);
            prepare(0x48b350u);run();compare_u32("platform_counter_lt_60",guest_call.out_eax,platform_counter_lt_60_48b350(v)?1u:0u);
        }
        if(enabled("platform_ghost_route_4671d0_r044")){
            const auto in=r044_ghost_inputs(i);auto native=r044_ghost_initial(i);auto x=r044_ghost_fixture(base,i);x.guest();set_r044_ghost_state(in,native);
            prepare(0x4671d0u);Bytes(reinterpret_cast<void*>(S),16).put32(0,E);run();
            platform_ghost_route_4671d0(x.ev(),in,native);const auto actual=get_r044_ghost_state();compare_memory("platform_ghost_route_memory",x);
            compare_u32("platform_ghost_writer",actual.writer_offset,native.writer_offset);compare_u32("platform_ghost_reader",actual.reader_offset,native.reader_offset);
            compare_u32("platform_ghost_stream_index",actual.stream_index,native.stream_index);compare_u32("platform_ghost_packet_11b",actual.packet_11b,native.packet_11b);
            compare_u32("platform_ghost_packet_11c",actual.packet_11c,native.packet_11c);compare_u32("platform_ghost_pending",actual.service_pending,native.service_pending);
            compare_u32("platform_ghost_short_window",actual.short_window,native.short_window);compare_u32("platform_ghost_serialize",actual.serialize_called?1u:0u,native.serialize_called?1u:0u);
        }
        if(enabled("platform_ghost_record_47f780_r044")){
            const auto in=r044_record_inputs(i);auto native=r044_record_initial(i);auto records=r044_record_blob(i);auto native_records=records;
            auto index_table=r044_index_table(i);auto pair_table=r044_pair_table(i);auto x=r044_record_fixture(base,i);x.guest();
            set_r044_record_state(in,native,records,index_table,pair_table);
            prepare(0x47f780u);Bytes(reinterpret_cast<void*>(S),16).put32(0,E);run();
            platform_ghost_record_47f780(x.ev(),Bytes(native_records.data(),native_records.size()),
                Bytes(index_table.data(),sizeof(index_table)),Bytes(pair_table.data(),sizeof(pair_table)),in,native);
            const auto actual=get_r044_record_state();compare_memory("platform_ghost_record_memory",x);
            compare_blob("platform_ghost_record_table",reinterpret_cast<void*>(R044RecordsBase),native_records);
            compare_u32("platform_ghost_record_init",actual.init_state,native.init_state);
            compare_u32("platform_ghost_record_sequence",actual.sequence,native.sequence);
            compare_u32("platform_ghost_record_frame",actual.frame_counter,native.frame_counter);
            compare_u32("platform_ghost_record_reset114",actual.reset114_bits,native.reset114_bits);
            compare_u32("platform_ghost_record_reset118",actual.reset118_bits,native.reset118_bits);
            compare_u32("platform_ghost_record_reset11c",actual.reset11c_bits,native.reset11c_bits);
            compare_u32("platform_ghost_record_marker121",actual.marker121,native.marker121);
            compare_u32("platform_ghost_record_marker123",actual.marker123,native.marker123);
            compare_u32("platform_ghost_record_ready",actual.record_ready,native.record_ready);
            compare_u32("platform_ghost_record_reset_service",actual.reset_service_called?1u:0u,native.reset_service_called?1u:0u);
            compare_u32("platform_ghost_record_record_service",actual.record_service_called?1u:0u,native.record_service_called?1u:0u);
        }
        if(enabled("common_pl_car_inline_tail_4a82c4_r044")){
            auto x=base;auto e=x.ev(),w=x.wk();e.put8(0xd22,std::uint8_t(i));e.put8(0xda4,std::uint8_t(i*3u));e.puti(0xd90,std::int32_t(i*17u)-50);
            w.put16(0x288,std::uint16_t(i*11u));w.put16(0x37c,std::uint16_t(i*13u));w.put16(0x470,std::uint16_t(i*17u));w.put16(0x564,std::uint16_t(i*19u));x.guest();
            prepare(R044StubBlock+0x200u);guest_call.ebp=E;guest_call.edi=W;run();common_pl_car_inline_tail_4a82c4(x.ev(),x.wk());compare_memory("common_pl_car_inline_tail",x);
        }
        if(enabled("session_mode4_4962a0_r043")){
            const auto in=r043_session_inputs(i);set_r043_session_state(in);prepare(0x4962a0u);run();compare_u32("session_mode4",guest_call.out_eax,session_mode4_4962a0(in)?1u:0u);
        }
        if(enabled("session_mode6_4962d0_r043")){
            const auto in=r043_session_inputs(i);set_r043_session_state(in);prepare(0x4962d0u);run();compare_u32("session_mode6",guest_call.out_eax,session_mode6_4962d0(in)?1u:0u);
        }
        if(enabled("othcar_calc_r_46f040_r043")||enabled("othcar_calc_r_alt_46f190_r043")){
            const auto pts=r043_radius_points(i);auto x=base;x.guest();auto ww=x.wk();
            auto put=[&](std::size_t o,const CourseProbe& q){ww.putf(o,q.x);ww.putf(o+4,q.y);ww.putf(o+8,q.z);};put(0x00,pts[0]);put(0x20,pts[1]);put(0x40,pts[2]);x.guest();
            if(enabled("othcar_calc_r_46f040_r043")){prepare(0x46f040u);Bytes st(reinterpret_cast<void*>(S),16);st.put32(0,W);st.put32(4,W+0x20u);st.put32(8,W+0x40u);guest_call.st0=1;run();compare_float("othcar_calc_r",ffrom(guest_call.out_st0),othcar_calc_r_46f040(pts[0],pts[1],pts[2]));}
            if(enabled("othcar_calc_r_alt_46f190_r043")){prepare(0x46f190u);Bytes st(reinterpret_cast<void*>(S),16);st.put32(0,W);st.put32(4,W+0x20u);st.put32(8,W+0x40u);guest_call.st0=1;run();compare_float("othcar_calc_r_alt",ffrom(guest_call.out_st0),othcar_calc_r_alt_46f190(pts[0],pts[1],pts[2]));}
        }
        if(enabled("othcar_get_r_479670_r043")){
            const auto in=r043_othcar_inputs(i);auto x=r043_othcar_fixture(base,i,in);x.guest();set_r043_othcar_inputs(in);prepare(0x479670u);Bytes st(reinterpret_cast<void*>(S),16);st.put32(0,E);run();othcar_get_r_479670(x.ev(),in);compare_memory("othcar_get_r",x);
        }
        if(enabled("calc_light_rate_4a3d40_r042")){
            auto x=calc_light_rate_fixture(base,i);x.guest();reset_matrix_oracle_globals();
            prepare(0x4a3d40);guest_call.eax=E;run();
            std::array<std::uint8_t,128> native_matrix{};PcMatrixStack ms{Bytes(native_matrix.data(),native_matrix.size()),0,0,2};
            calc_light_rate_4a3d40(x.ev(),ms);
            compare_memory("calc_light_rate",x);
            compare_blob("calc_light_rate_matrix_arena",reinterpret_cast<void*>(MatrixArena),native_matrix);
            compare_u32("calc_light_rate_matrix_pointer",*reinterpret_cast<std::uint32_t*>(0x89b564u),MatrixArena);
            compare_u32("calc_light_rate_matrix_depth",std::uint32_t(*reinterpret_cast<std::int32_t*>(0x89b568u)),0u);
        }
        if(enabled("record_ghost_car_4a4710_r042")){
            const auto in=record_ghost_inputs(i);auto c=record_ghost_fixture(base,i);auto native_history=c.history;c.x.guest();set_record_ghost_state(in,c.history);
            prepare(0x4a4710);Bytes st(reinterpret_cast<void*>(S),16);st.put32(0,E);run();
            record_ghost_car_4a4710(c.x.ev(),Bytes(native_history.data(),native_history.size()),in);
            compare_memory("record_ghost_car",c.x);
            compare_blob("record_ghost_car_history",reinterpret_cast<void*>(RecordGhostHistoryBase),native_history);
        }
        if(enabled("calc_ofs_left_lane_4a45f0_r041")){
            const auto in=ofs_left_lane_inputs(i);auto x=ofs_left_lane_fixture(base,i);x.guest();set_ofs_left_lane_inputs(in);
            prepare(0x4a45f0);Bytes st(reinterpret_cast<void*>(S),16);st.put32(0,E);run();
            calc_ofs_left_lane_4a45f0(x.ev(),in);compare_memory("calc_ofs_left_lane",x);
        }
        if(enabled("handicap_control_458e40_r039")){
            const auto in=handicap_inputs(i);auto x=handicap_fixture(base,i,in);x.guest();set_handicap_inputs(in);
            prepare(0x458e40);Bytes st(reinterpret_cast<void*>(S),16);st.put32(0,E);run();
            handicap_control_458e40(x.ev(),in);compare_memory("handicap_control",x);
        }
        if(enabled("contact_matrix")){
            auto x=contact_fixture(base,i);
            x.guest();reset_matrix_oracle_globals();prepare(ContactMatrixBlock);guest_call.ebx=W;run();
            contact_matrix(x.wk());compare_memory("calc_contact_matrix",x);
        }
        if(enabled("maximum_velocity")){
            auto x=maximum_velocity_fixture(base,i);
            x.guest();reset_matrix_oracle_globals();prepare(0x4a65c0);guest_call.eax=W;run();
            const std::uint32_t guest_ret=guest_call.out_eax;
            const bool native=maximum_velocity_check(x.wk());
            compare_u32("maximum_velocity_return",guest_ret,native?1u:0u);
            compare_memory("maximum_velocity_check",x);
        }
        const auto dynamics=wheel_fixture(base,i);
        if(enabled("cornering")){
            auto x=dynamics;x.guest();prepare(0x500ea0);guest_call.eax=E;guest_call.ecx=W;run();
            snapshot_case(1,i,dynamics);
            cornering_power(x.ev(),x.pa(),x.wheels());compare_memory("cornering_power",x);
        }
        if(enabled("side")){
            auto x=dynamics;x.guest();prepare(0x5019c0);guest_call.eax=W;run();
            snapshot_case(2,i,dynamics);
            side_force(x.wheels());compare_memory("side_force",x);
        }
        if(enabled("front_force")){
            auto x=dynamics;x.guest();prepare(0x500ff0);guest_call.edi=E;guest_call.eax=W;run();
            snapshot_case(3,i,dynamics);
            front_driving_force(x.pa(),x.wheels());compare_memory("front_driving_force",x);
        }
        if(enabled("rear_force")){
            auto x=dynamics;x.guest();prepare(0x501240);guest_call.edi=E;guest_call.esi=W;run();
            snapshot_case(4,i,dynamics);
            rear_driving_force(x.ev(),x.wk(),x.pa());compare_memory("rear_driving_force",x);
        }
        if(enabled("circle")){
            auto x=dynamics;x.guest();prepare(0x5026f0);guest_call.ebx=E;Bytes(reinterpret_cast<void*>(S),16).put32(0,W);run();
            snapshot_case(5,i,dynamics);
            friction_circle(x.ev(),x.pa(),x.wheels());compare_memory("friction_circle",x);
        }
        if(enabled("resolve")){
            auto x=dynamics;x.guest();prepare(0x501ad0);guest_call.eax=W;run();
            snapshot_case(6,i,dynamics);
            resolve_wheel_forces(x.wheels());compare_memory("resolve_wheel_forces",x);
        }
        if(enabled("front_rotation")){
            auto x=dynamics;x.guest();prepare(0x501190);guest_call.edi=E;guest_call.edx=W;run();
            snapshot_case(7,i,dynamics);
            front_wheel_rotation(x.pa(),x.wheels());compare_memory("front_wheel_rotation",x);
        }
        if(enabled("rear_rotation")){
            auto x=dynamics;x.guest();prepare(0x501680);guest_call.esi=E;guest_call.ecx=W;run();
            snapshot_case(8,i,dynamics);
            rear_wheel_rotation(x.ev(),x.wk(),x.pa());compare_memory("rear_wheel_rotation",x);
        }
        if(enabled("rolling")){
            auto x=dynamics;x.guest();prepare(0x501c90);guest_call.eax=E;guest_call.ecx=W;run();
            snapshot_case(9,i,dynamics);
            rolling_resistance(x.ev(),x.pa(),x.wheels());compare_memory("rolling_resistance",x);
        }
        if(enabled("slip")){
            auto x=dynamics;x.guest();prepare(0x501ba0);guest_call.eax=W;Bytes(reinterpret_cast<void*>(S),16).put32(0,E);run();
            snapshot_case(10,i,dynamics);
            slip_ratio(x.pa(),x.wheels());compare_memory("slip_ratio",x);
        }
        if(enabled("brake_distribution")){
            auto x=dynamics;x.guest();prepare(BrakeBlock);guest_call.ebx=E;guest_call.ebp=W;run();
            distribute_brake_torque(x.ev(),x.wk(),x.pa(),tables);compare_memory("brake_distribution_inline",x);
        }
        const auto steering=steering_fixture(base,i);
        if(enabled("steering")){
            auto x=steering;x.guest();prepare(0x5003c0);guest_call.eax=W;guest_call.esi=E;run();
            steering_operation(x.ev(),x.wk(),x.pa());compare_memory("steering_operation",x);
        }
        if(enabled("toe")){
            auto x=steering;const std::int32_t channel=int(i%256);
            *reinterpret_cast<std::int32_t*>(0x7d6824)=channel;
            x.guest();prepare(0x500580);Bytes(reinterpret_cast<void*>(S),16).put32(0,E);Bytes(reinterpret_cast<void*>(S),16).put32(4,W);run();
            if(*reinterpret_cast<std::int32_t*>(0x7d6824)!=channel)throw std::runtime_error("toe input global unexpectedly modified");
            toe_angle(x.ev(),x.wk(),x.pa(),channel);compare_memory("toe_angle",x);
        }
        if(enabled("direction_angles")){
            auto x=steering;x.guest();prepare(DirectionAnglesBlock);guest_call.eax=W;guest_call.ecx=E;run();
            tire_direction_angles(x.ev(),x.wk());compare_memory("tire_direction_angles",x);
        }
        if(enabled("tire_direction")){
            auto x=direction_fixture(base,i);x.guest();reset_matrix_oracle_globals();prepare(0x500700);guest_call.eax=W;guest_call.ecx=E;run();
            if(*reinterpret_cast<std::uint32_t*>(0x89b564u)!=MatrixArena||*reinterpret_cast<std::int32_t*>(0x89b568u)!=0)
                throw std::runtime_error("CalcTireDirection leaked matrix-stack state");
            tire_direction(x.ev(),x.wk());compare_memory("tire_direction",x);
        }
        if(enabled("tire_velocity")){
            auto x=velocity_fixture(base,i);x.guest();reset_matrix_oracle_globals();prepare(0x500230);guest_call.ebx=W;run();
            if(*reinterpret_cast<std::uint32_t*>(0x89b564u)!=MatrixArena||*reinterpret_cast<std::int32_t*>(0x89b568u)!=0)
                throw std::runtime_error("CalcTireVelocity leaked matrix-stack state");
            tire_velocity(x.wk());compare_memory("tire_velocity",x);
        }
        if(enabled("running")){
            auto x=resistance_fixture(base,i);x.guest();prepare(0x502120);guest_call.ebx=E;guest_call.ecx=W;run();
            running_resistance(x.ev(),x.wk(),running_tuning());compare_memory("running_resistance",x);
        }
        if(enabled("copy_physical")){
            auto x=physical_fixture(base,i);const float coeff=0.25f+float(i%17)*0.05f;
            x.pa().putf(0x20a8,coeff);x.guest();prepare(0x502270);
            Bytes(reinterpret_cast<void*>(S),16).put32(0,E);Bytes(reinterpret_cast<void*>(S),16).put32(4,W);run();
            copy_physical_work(x.ev(),x.wk(),coeff);compare_memory("copy_physical_work",x);
        }
    }
    // An ordered subset, NOT DrivingControl: feed external wheel/engine state,
    // retain event/work writes between frames, and check each phase separately.
    // No invented vehicle movement or collision simulation is added here.
    if(only=="all"){
        for(unsigned sequence=0;sequence<8;++sequence){
            auto x=fixture(100000+sequence);
            x.ev().put8(0x296,0);x.ev().put8(0x297,0);x.ev().puti(0x3c,255);
            x.ev().put32(4,0);x.ev().putf(0x2f8,0);x.ev().putf(0x2a0,1);
            x.ev().put32(0x304,128);x.ev().put32(0x308,128);x.guest();
            Bytes ge(reinterpret_cast<void*>(E),event_size);
            for(unsigned frame=0;frame<256;++frame){
                const int pedal=int((frame*13+sequence*17)%256);
                const float speed=float((frame*19+sequence*41)%1400);
                auto inputs=[&](Bytes ev){ev.puti(0x34,pedal);ev.puti(0x38,int((frame*7)%256));
                    ev.putf(0x21c,speed);ev.put32(0x208,(frame/32)%7);
                    if(frame%32==0)ev.put8(0x296,frame%64==0?1:3);};
                inputs(x.ev());inputs(ge);
                prepare(0x5025c0);guest_call.eax=E;guest_call.esi=W;run();
                accel_operation(x.ev(),x.wk(),x.pa());compare_memory("sequence_accel",x);
                prepare(0x500950);guest_call.ecx=E;guest_call.edi=W;run();
                auto_clutch_control(x.ev(),x.wk(),x.pa());compare_memory("sequence_clutch",x);
                prepare(0x502a00);guest_call.eax=E;run();
                engine_torque(x.ev(),x.pa(),tables);compare_memory("sequence_torque",x);
                prepare(0x500c60);guest_call.eax=E;guest_call.esi=W;run();
                tire_grip(x.ev(),x.wk(),x.pa(),x.wheels());compare_memory("sequence_grip",x);
            }
        }
    }
    // r005: retained engine and wheel rotation state over longer ordered stages.
    // Only contact inputs and driver requests are supplied externally. No fake
    // chassis integration feeds back into the original calculation.
    if(only=="all"){
        for(unsigned sequence=0;sequence<16;++sequence){
            auto x=wheel_fixture(fixture(200000+sequence),200000+sequence);
            auto e=x.ev(),p=x.pa();
            e.put32(4,0);e.putf(0x2f8,0);e.putf(0x2a0,1);e.putf(0xe6c,0);e.putf(0x2a4,0);
            e.put8(0x296,0);e.put8(0x297,0);e.puti(0x3c,255);e.putf(0x22c,1);e.putf(0x228,400);
            e.putf(0x21c,400);e.put32(0x48,3800);e.put32(0x20c,3800);e.put32(0x304,0);e.put32(0x308,120);
            e.put8(0x13,std::uint8_t(sequence%2));e.put8(0x11,0);e.put32(0xe84,0);
            p.putf(0x16dc,0.4f);p.putf(0xc78,2);p.putf(0xcc4,2);p.putf(0xb48,0.32f);p.putf(0xb94,0.34f);
            p.putf(0x1644,1000);p.putf(0x1690,1250);p.putf(0x15f8,100);
            for(auto q:x.wheels()){q.putf(0xd8,40);q.putf(0xd0,0);q.putf(0xe8,1);q.putf(0x34,3000);q.putf(0x38,3000);}
            x.guest();
            Bytes ge(reinterpret_cast<void*>(E),event_size),gw(reinterpret_cast<void*>(W),work_size);
            for(unsigned frame=0;frame<512;++frame){
                auto inputs=[&](Bytes ev,Bytes work){
                    const auto section=(frame/64)%8;
                    ev.puti(0x34,section==0?0:section==1?255:int((frame*13+sequence*17)%256));
                    ev.puti(0x38,section==3?255:section==4?250:section==5?251:section==7?128:0);
                    ev.put32(0x208,section==0?0:1+(frame/64)%6);
                    ev.put32(0x1f4,(frame*3+sequence*7)%260);ev.puti(0xd90,section==1?1:0);
                    ev.put8(0x283,frame%127==0?1:0);
                    if(frame%64==0)ev.put8(0x296,section%2?1:3);
                    if(frame==160)ev.put32(0x304,120);
                    const auto qs=embedded_wheels(work);
                    for(unsigned j=0;j<4;++j){auto q=qs[j];
                        const float speed=float((frame*11+sequence*7+j)%900)*0.1f;
                        q.putf(0xd4,speed);
                        q.putf(0x34,frame%127==0?0:float(2000+(frame*23+j*503)%2500));
                        q.putf(0xe8,section==6?0.3f:1.0f);
                        // CalcTireVelocity/remaining CalcTireDirection are not in this
                        // sequence yet, so feed their vector outputs explicitly with
                        // finite synthetic values instead of leaving random canaries.
                        q.putf(0x58,0.75f+0.03f*float(j));q.putf(0x5c,0.01f*float((frame+j)%5));q.putf(0x60,0.45f-0.02f*float(j));
                        q.putf(0x64,-0.35f+0.04f*float(j));q.putf(0x68,0.08f);q.putf(0x6c,0.85f-0.03f*float(j));
                        q.putf(0x4c,0.65f);q.putf(0x50,0.02f*float(j));q.putf(0x54,0.70f);
                        const auto direction=std::uint16_t(int((frame*31+j*389+sequence*113)%12000)-6000);
                        q.put16(0xee,direction);q.put16(0xec,0);q.put16(0x32,direction);
                    }
                };
                inputs(x.ev(),x.wk());inputs(ge,gw);auto whole=x;
                prepare(0x5025c0);guest_call.eax=E;guest_call.esi=W;run();
                accel_operation(x.ev(),x.wk(),x.pa());compare_memory("wheel_sequence_01_accel",x);
                prepare(BrakeBlock);guest_call.ebx=E;guest_call.ebp=W;run();
                distribute_brake_torque(x.ev(),x.wk(),x.pa(),tables);compare_memory("wheel_sequence_02_brake_distribution",x);
                prepare(0x500950);guest_call.ecx=E;guest_call.edi=W;run();
                auto_clutch_control(x.ev(),x.wk(),x.pa());compare_memory("wheel_sequence_03_clutch",x);
                prepare(0x502a00);guest_call.eax=E;run();
                engine_torque(x.ev(),x.pa(),tables);compare_memory("wheel_sequence_04_torque",x);
                prepare(0x500c60);guest_call.eax=E;guest_call.esi=W;run();
                tire_grip(x.ev(),x.wk(),x.pa(),x.wheels());compare_memory("wheel_sequence_05_grip",x);
                prepare(0x500ea0);guest_call.eax=E;guest_call.ecx=W;run();
                cornering_power(x.ev(),x.pa(),x.wheels());compare_memory("wheel_sequence_06_cornering",x);
                prepare(0x5019c0);guest_call.eax=W;run();
                side_force(x.wheels());compare_memory("wheel_sequence_07_side",x);
                prepare(0x500ff0);guest_call.edi=E;guest_call.eax=W;run();
                front_driving_force(x.pa(),x.wheels());compare_memory("wheel_sequence_08_front_force",x);
                prepare(0x501240);guest_call.edi=E;guest_call.esi=W;run();
                rear_driving_force(x.ev(),x.wk(),x.pa());compare_memory("wheel_sequence_09_rear_force",x);
                prepare(0x5026f0);guest_call.ebx=E;Bytes(reinterpret_cast<void*>(S),16).put32(0,W);run();
                friction_circle(x.ev(),x.pa(),x.wheels());compare_memory("wheel_sequence_10_circle",x);
                prepare(0x501ad0);guest_call.eax=W;run();
                resolve_wheel_forces(x.wheels());compare_memory("wheel_sequence_11_resolve",x);
                prepare(0x501190);guest_call.edi=E;guest_call.edx=W;run();
                front_wheel_rotation(x.pa(),x.wheels());compare_memory("wheel_sequence_12_front_rotation",x);
                prepare(0x501680);guest_call.esi=E;guest_call.ecx=W;run();
                rear_wheel_rotation(x.ev(),x.wk(),x.pa());compare_memory("wheel_sequence_13_rear_rotation",x);
                prepare(0x501c90);guest_call.eax=E;guest_call.ecx=W;run();
                rolling_resistance(x.ev(),x.pa(),x.wheels());compare_memory("wheel_sequence_14_rolling",x);
                prepare(0x501ba0);guest_call.eax=W;Bytes(reinterpret_cast<void*>(S),16).put32(0,E);run();
                slip_ratio(x.pa(),x.wheels());compare_memory("wheel_sequence_15_slip",x);
                prepare(0x502120);guest_call.ebx=E;guest_call.ecx=W;run();
                running_resistance(x.ev(),x.wk(),running_tuning());compare_memory("wheel_sequence_16_running",x);
                const float tail_coeff=0.35f+float((frame+sequence)%11)*0.04f;
                *reinterpret_cast<float*>(P+0x20a8)=tail_coeff;x.pa().putf(0x20a8,tail_coeff);whole.pa().putf(0x20a8,tail_coeff);
                prepare(0x502270);Bytes(reinterpret_cast<void*>(S),16).put32(0,E);Bytes(reinterpret_cast<void*>(S),16).put32(4,W);run();
                copy_physical_work(x.ev(),x.wk(),tail_coeff);compare_memory("wheel_sequence_17_copy_physical",x);
                known_driving_tail_from_contacts(whole.ev(),whole.wk(),whole.pa(),tables,running_tuning(),tail_coeff);
                compare_memory("known_driving_tail_from_contacts_whole",whole);
            }
        }
    }
    if(stats.empty())throw std::runtime_error("no enabled routine or no comparisons");
    finish_snapshots();finish_spline_golden();finish_query_golden();finish_world_golden();finish_ground_golden();finish_wall_golden();finish_response_golden();finish_rebound_golden();finish_crash_golden();finish_entry_golden();
    std::uint64_t failures=0;
    std::cout<<"{\n  \"x87_control_word\": "<<x87_control<<",\n  \"seed\": \"0x4f523034\",\n  \"cases_per_routine\": "<<count<<",\n  \"floating_environment\": \"selected x87 control, MXCSR 0x1f80, host no-fast-math no-contract\",\n  \"results\": [\n";
    bool first=true;for(const auto& kv:stats){const auto& name=kv.first;const auto& s=kv.second;
        if(!first)std::cout<<",\n";
        first=false;failures+=s.fail;
        std::cout<<"    {\"routine\": \""<<name<<"\", \"cases\": "<<s.cases<<", \"bit_exact\": "<<s.exact
            <<", \"mismatch\": "<<s.fail<<", \"max_ulp\": "<<s.max_ulp<<", \"bytes_compared\": "<<s.bytes<<"}";
    }
    std::cout<<"\n  ],\n  \"crash_coverage\": {";
    bool first_crash=true;
    for(const auto& kv:entry_coverage)crash_coverage["r021_"+kv.first]+=kv.second;
    for(const auto& kv:crash_coverage){if(!first_crash)std::cout<<",";first_crash=false;std::cout<<"\n    \""<<kv.first<<"\": "<<kv.second;}
    std::cout<<"\n  },\n  \"rebound_coverage\": {";
    bool first_rebound=true;
    for(const auto& kv:rebound_coverage){if(!first_rebound)std::cout<<",";first_rebound=false;std::cout<<"\n    \""<<kv.first<<"\": "<<kv.second;}
    std::cout<<"\n  },\n  \"response_coverage\": {";
    bool first_response=true;
    for(const auto& kv:response_coverage){if(!first_response)std::cout<<",";first_response=false;std::cout<<"\n    \""<<kv.first<<"\": "<<kv.second;}
    std::cout<<"\n  },\n  \"wall_coverage\": {";
    bool first_wall_coverage=true;
    for(const auto& kv:wall_coverage){if(!first_wall_coverage)std::cout<<",";first_wall_coverage=false;std::cout<<"\n    \""<<kv.first<<"\": "<<kv.second;}
    std::cout<<"\n  },\n  \"ground_coverage\": {";
    bool first_coverage=true;for(const auto& kv:ground_coverage){if(!first_coverage)std::cout<<",";first_coverage=false;std::cout<<"\n    \""<<kv.first<<"\": "<<kv.second;}
    std::cout<<"\n  },\n  \"total_mismatching_cases\": "<<failures<<"\n}\n";
    return failures?1:0;
}catch(const std::exception& e){std::cerr<<"ERROR: "<<e.what()<<"\n";return 2;}}

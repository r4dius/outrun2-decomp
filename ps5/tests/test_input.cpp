#include "input/dual_sense.hpp"
#include "input/runtime_adapter.hpp"
#include "platform/race_input.hpp"
#include <cstdio>
#include <cstdlib>
namespace {
unsigned checks=0;
void check(bool ok,const char* message){++checks;if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
}
int main(){
    using namespace outrun;
    using namespace outrun::ps5;
    DualSenseInput input;
    PadSample pad;pad.connected=true;
    auto sample=input.update(pad,16);
    check(sample.connected&&sample.pc_device.axes_94[0x0e]==0,"neutral controller");
    pad.left_y=0;pad.right_y=255;sample=input.update(pad,16);
    check(sample.frontend.axes[1]==-128&&sample.frontend.axes[3]==127,"menu axes: stick up is negative (game_host map_pad convention)");
    pad.left_y=128;pad.right_y=128;
    platform::PcInputAnalog analog{};float filter=0;
    const auto cfg=platform::pc_input_joystick_config(0);
    pad.r2=173;pad.l2=51;pad.left_x=255;
    sample=input.update(pad,16);
    platform::pc_input_analog_453860(analog,filter,sample.pc_device,cfg,4,16);
    check(analog.current[1]==173&&analog.current[2]==51,"analog triggers reach recovered PC pedals");
    check(analog.current[0]>120,"right steering reaches PC filter");
    pad.left_x=0;
    sample=input.update(pad,16);
    platform::pc_input_analog_453860(analog,filter,sample.pc_device,cfg,4,16);
    check(analog.current[0]==-127,"left steering reaches PC filter");
    for(unsigned v=0;v<256;++v){pad.r2=std::uint8_t(v);sample=input.update(pad,16);
        platform::pc_input_analog_453860(analog,filter,sample.pc_device,cfg,4,16);
        check(analog.current[1]==int(v),"all trigger levels preserved");}
    pad.buttons=R1;
    sample=input.update(pad,16);
    platform::PcInputSwitchRecord switches{};
    platform::pc_input_switch_453640(switches,sample.pc_device,cfg);
    check(sample.shift_up&&(switches.pressed&0x80u),"R1 gear shift reaches recovered PC switch record");
    check(!(sample.frontend.feature_mask&0x20u),"race R1 does not open frontend selector");
    sample=input.update(pad,16);
    platform::pc_input_switch_453640(switches,sample.pc_device,cfg);
    check(!sample.shift_up&&!(switches.pressed&0x80u),"held shift does not repeat");
    pad.buttons=0;input.update(pad,16);pad.buttons=Options;
    sample=input.update(pad,16);
    check(sample.start&&!sample.exit_requested,"Options pauses without quitting");
    pad.buttons=Options|L1|R1;sample=input.update(pad,16);
    check(sample.exit_requested,"explicit exit chord");
    pad.buttons=0;input.update(pad,34);pad.buttons=Cross;
    sample=input.update(pad,34);
    platform::pc_input_switch_453640(switches,sample.pc_device,cfg);
    check(sample.menu_confirm&&(switches.held&4u),"result-screen Cross produces PC confirm");
    sample=input.update(pad,16);
    check(!(sample.pc_device.buttons_04&2u),"GAME Cross does not shift gear");
    pad.buttons=Right;sample=input.update(pad,0);
    check(sample.menu_right&&sample.frontend.device_held==0,"menu navigation does not toggle car class");
    pad.buttons=R2;sample=input.update(pad,0);
    check(sample.frontend.device_held==0x2000u,"R2 cycles class/music");
    sample=input.update(pad,0);
    check(sample.frontend.device_held==0,"class toggle fires once per press");
    pad.buttons=Square|Triangle;sample=input.update(pad,0);
    check(!sample.menu_preview&&!sample.menu_start_probe,"no diagnostic GAME/START bypass");
    pad.connected=false;sample=input.update(pad,16);
    check(!sample.connected&&sample.accelerator==0&&sample.pc_device.buttons_04==0,"disconnect releases pedals and buttons");
    check(sample.raw_buttons_up==(Square|Triangle),"disconnect emits release edges");
    pad.connected=true;pad.buttons=Cross;sample=input.update(pad,0);
    check(sample.menu_confirm,"reconnection starts with fresh edge history");
    struct Backend {PadSample pad;bool reads_ok{true};std::uint32_t mode{16};} backend;
    RuntimeAdapter adapter;adapter.backend=&backend;
    adapter.read_pad=[](void* user,PadSample& out){auto& b=*static_cast<Backend*>(user);out=b.pad;return b.reads_ok;};
    adapter.current_mode=[](void* user){return static_cast<Backend*>(user)->mode;};
    auto native_input=make_runtime_input(adapter);
    auto native_platform=make_runtime_platform(adapter);
    check(platform::runtime_input_ready(native_input)&&platform::runtime_platform_ready(native_platform),"native callback contracts ready");
    backend.pad.connected=true;backend.pad.r2=99;
    platform::runtime_input_sample(native_input,sample);
    check(sample.accelerator==99&&native_input.sample_calls==1,"native callback carries hardware sample");
    backend.reads_ok=false;platform::runtime_input_sample(native_input,sample);
    check(!sample.connected&&sample.accelerator==0,"failed read clears stale hardware data");
    platform::runtime_platform_begin_frame(native_platform);
    check(native_platform.cached_frequency==1000000000ull&&platform::runtime_platform_poll(native_platform),"portable clock and lifecycle");
    backend.reads_ok=true;backend.pad.buttons=Options|L1|R1;
    platform::runtime_input_sample(native_input,sample);
    check(!platform::runtime_platform_poll(native_platform),"exit chord stops native loop");
    std::printf("ps5_input: %u checks passed (real PC input consumers, host only)\n",checks);
}

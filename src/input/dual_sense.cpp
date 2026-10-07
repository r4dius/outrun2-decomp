#include "dual_sense.hpp"
#include <algorithm>

namespace outrun::ps5 {
namespace {
// 128 is exactly neutral. Preserve both extrema of the PC signed axis.
std::int16_t axis_word(std::uint8_t value){
    const auto v=std::int32_t(value)-128;
    return std::int16_t(v<0?v*256:v*32767/127);
}
std::int16_t axis_byte(std::uint8_t value){return std::int16_t(std::int32_t(value)-128);}
}
platform::NativeInputState DualSenseInput::update(const PadSample& p,std::uint32_t mode){
    platform::NativeInputState out{};
    const auto held=p.connected?p.buttons:0u;
    const auto down=held&~previous_;
    const auto up=previous_&~held;
    previous_=held;
    out.connected=p.connected;
    out.raw_buttons_held=held;out.raw_buttons_down=down;out.raw_buttons_up=up;
    // Report release edges on disconnect, but never retain a pedal/axis.
    if(!p.connected)return out;
    out.left_stick_x=axis_word(p.left_x);out.left_stick_y=-std::int32_t(axis_word(p.left_y));
    out.right_stick_x=axis_word(p.right_x);out.right_stick_y=-std::int32_t(axis_word(p.right_y));
    out.steering=axis_byte(p.left_x);
    if(held&Left)out.steering=-128;
    if(held&Right)out.steering=127;
    out.accelerator=(held&Cross)?255u:p.r2;
    out.brake=(held&Circle)?255u:p.l2;
    out.shift_up=(down&R1)!=0;out.shift_down=(down&L1)!=0;
    out.menu_left=(down&Left)!=0;out.menu_right=(down&Right)!=0;
    out.menu_up=(down&Up)!=0;out.menu_down=(down&Down)!=0;
    out.menu_confirm=(down&(Cross|Options))!=0;out.menu_cancel=(down&Circle)!=0;
    out.start=(down&Options)!=0;
    constexpr auto exit_mask=Options|L1|R1;
    out.exit_requested=(held&exit_mask)==exit_mask&&(down&exit_mask)!=0;
    const bool in_race=mode==16u||mode==18u;
    // Same convention as the shared pad mapping (game_host map_pad): y grows downwards, as on PC.
    out.frontend.axes={axis_byte(p.left_x),axis_byte(p.left_y),
                       axis_byte(p.right_x),axis_byte(p.right_y)};
    auto& f=out.frontend.feature_mask;
    if(down&Options)f|=1u;
    if(down&Cross)f|=4u;
    if(down&Circle)f|=8u;
    if(down&Up)f|=0x400u;
    if(down&Down)f|=0x800u;
    if(down&Left)f|=0x1000u;
    if(down&Right)f|=0x2000u;
    if(!in_race){if(down&L1)f|=0x10u;if(down&R1)f|=0x20u;}
    if(down&Square)f|=0x8000u|0x40000u;
    if(down&Triangle)f|=0x4000u;
    out.frontend.device_held=((down&L2)?0x1000u:0u)|((down&R2)?0x2000u:0u);
    auto& d=out.pc_device;
    d.axes_94[0x0e]=axis_word(p.left_x);
    d.axes_94[0x0d]=std::int16_t(out.accelerator);
    d.axes_94[0x0c]=std::int16_t(out.brake);
    if(held&Options)d.buttons_04|=1u;
    if(held&R1)d.buttons_04|=2u;
    if(held&Circle)d.buttons_04|=4u;
    if(held&L1)d.buttons_04|=8u;
    if(held&Square)d.buttons_04|=0x10u;
    if(held&Right)d.buttons_04|=0x80u;
    if(held&Left)d.buttons_04|=0x100u;
    // Match r155's result-screen confirm without shifting gear on Cross in GAME.
    if((held&Cross)&&mode!=16u)d.buttons_04|=2u;
    // The pad as the PC DirectInput joystick (pc_input_devices.hpp), in the
    // Switch layout's places: 0 Cross, 1 Circle, 2 Square, 3 Triangle, 4 L1,
    // 5 R1, 6 Share, 7 Options, 8/9 stick clicks, 10 L2, 11 R2 (pressed past a
    // quarter of the trigger); POV = D-pad; lX/lY left stick, lRx/lRy right stick.
    out.pc_input_layer=true;
    auto& pad=out.pc_pad;
    static constexpr std::uint32_t order[10]{Cross,Circle,Square,Triangle,L1,R1,Share,Options,StickL,StickR};
    for(unsigned k=0;k<10;++k)pad.buttons[k]=(held&order[k])?0x80u:0u;
    pad.buttons[10]=((held&L2)||p.l2>0x40u)?0x80u:0u;
    pad.buttons[11]=((held&R2)||p.r2>0x40u)?0x80u:0u;
    pad.axes[0]=axis_word(p.left_x);pad.axes[1]=axis_word(p.left_y);pad.axes[3]=axis_word(p.right_x);pad.axes[4]=axis_word(p.right_y);
    const bool u=held&Up,dn=held&Down,l=held&Left,r=held&Right;
    pad.pov=u?(r?4500u:l?31500u:0u):dn?(r?13500u:l?22500u:18000u):r?9000u:l?27000u:0xffffffffu;
    return out;
}
} // namespace outrun::ps5

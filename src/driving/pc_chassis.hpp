#pragma once
#include "driving/pc_matrix_stack.hpp"
#include <array>
namespace outrun::driving {
// PC 0x00517410 ActionForce2. `body` is the bounded RIGID_BODY prefix through
// +0xd7; matrix-stack state is explicit instead of hidden in guest globals.
// PC 0x00517010 SetForce_obsolete. Flags: bit0 linear force, bit1 torque.
void set_force_obsolete(Bytes body,const CourseProbe& force,const CourseProbe& point,std::uint32_t flags,PcMatrixStack& matrices);
void action_force2(Bytes body,float step_scale,PcMatrixStack& matrices);
// PC 0x0049FD50 MakeForceWorkSus: accumulate the four suspension forces.
// Guest pointers work+0x248..0x254 are supplied as explicit bounded views.
void make_force_work_sus(Bytes event,Bytes work,const std::array<Bytes,4>& suspensions,PcMatrixStack& matrices);
// PC 0x0049FE00 CalcGameResist.  The parameter block normally reached through
// event+0x2b4 is explicit in the native API so no guest pointer is dereferenced.
void calc_game_resist(Bytes event,Bytes work,Bytes params,PcMatrixStack& matrices);
// PC 0x0049FED0 force-work helper retained under its address-derived name.
// r032 pins MakeForceWorkTire to PC 0x004A0000 instead; event+0x2b4 is an
// explicit bounded view here.
void force_work_49fed0(Bytes event,Bytes work,Bytes params,PcMatrixStack& matrices);
// PC 0x00518010 / 0x005180B0, corroborated by the symbolised Lindbergh build.
void ass_cancel_incline_resistance(Bytes work,PcMatrixStack& matrices);
void ass_press_down_against_road(Bytes work,PcMatrixStack& matrices);
// PC 0x004A0000 tire-force accumulator used by CalcPlBody_2nd.  The four
// work+0x248 guest tire pointers are explicit bounded views.
void make_force_work_tire_4a0000(Bytes event,Bytes work,Bytes params,const std::array<Bytes,4>& tires,PcMatrixStack& matrices);
// PC 0x004A61F0 force-work wrapper.  Suspension and parameter guest pointers
// are explicit bounded inputs; all observable event/work effects remain native.
void make_force_work(Bytes event,Bytes work,Bytes params,const std::array<Bytes,4>& suspensions,PcMatrixStack& matrices);
}

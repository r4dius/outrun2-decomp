#include "platform/race_ending.hpp"
#include "platform/vehicle_model_draw.hpp"
#include "driving/pc_matrix_stack.hpp"
#include <cstring>
namespace outrun::platform {
namespace {
std::uint32_t call(PcRaceContext& c,std::uint32_t pc,std::initializer_list<std::uint32_t> args){
    PcRaceCall k;k.pc=pc;k.argc=std::uint32_t(args.size());
    std::size_t i=0;for(auto a:args)k.args[i++]=a;
    return c.service(k);
}
std::uint32_t fb(float f){std::uint32_t v;std::memcpy(&v,&f,4);return v;}
void draw(PcRaceContext& c,std::uint32_t pc,std::initializer_list<std::uint32_t> args){
    if(!c.draws)throw std::logic_error("ending: display without a draw list");
    PcVehicleDrawCall d{};d.pc=pc;d.argc=std::uint32_t(args.size());
    unsigned i=0;for(auto a:args)d.args[i++]=a;
    const auto cur=c.matrices.current();for(unsigned k=0;k<64;++k)d.matrix[k]=cur.u8(k);
    c.draws->push_back(d);
}
// The ending's resource requests 448AD0(id, mode).
void request(PcRaceContext& c,std::uint32_t id,std::uint32_t mode){call(c,0x448ad0u,{id,mode});}
// 452080 (ESI = model token, alpha): the background model, opaque (alpha >= 0.9 at 6280F0) or
// blended, through the 4044F0 pass states, 4056D0 and 404540; models with several parts
// (406780 > 1) draw a 4162B0 fog part first.
void background_452080(PcRaceContext& c,std::uint32_t token,float alpha){
    const std::int32_t parts=std::int32_t(call(c,0x406780u,{token,1u}));
    const bool opaque=alpha>=c.m.f32(0x6280f0u);                      // comiss / jb: unordered is blended
    if(parts>1){
        if(opaque){
            draw(c,0x4044f0u,{0u,0u,0u,0u,0xfu});draw(c,0x4044f0u,{1u,1u,0u,0u,8u});
            draw(c,0x4056d0u,{token,0u,0u,call(c,0x4162b0u,{0u,0u,0xffffffffu})});
            draw(c,0x4044f0u,{0u,0u,0u,0u,0u});draw(c,0x4044f0u,{1u,1u,0u,8u,7u});
            draw(c,0x4056d0u,{token,fb(alpha),0u,0xffffffffu});
        }else{
            draw(c,0x4044f0u,{0u,1u,0u,8u,7u});draw(c,0x4044f0u,{1u,1u,0u,8u,7u});
            draw(c,0x4056d0u,{token,fb(alpha),0u,0xffffffffu});
        }
    }else if(opaque){
        draw(c,0x4044f0u,{0u,0u,0u,0u,0xfu});draw(c,0x4044f0u,{1u,0u,0u,0u,0xfu});
        draw(c,0x4056d0u,{token,call(c,0x4162b0u,{0u,0u,0xffffffffu}),0u,0xffffffffu});
    }else{
        draw(c,0x4044f0u,{0u,1u,0u,8u,7u});draw(c,0x4044f0u,{1u,1u,0u,8u,7u});
        draw(c,0x4056d0u,{token,fb(alpha),0u,0xffffffffu});
    }
    draw(c,0x404540u,{});
}
}

// 452B10: the car's +58 = 1.0, 44C0D0, 4AF580, state 0, [638E98] clamped below 0x26, [7D3A88] = 0,
// [638EA0] = -1; without [7D3A84]: 40EC60(0), 448FD0(0x100, 49A650).
void ending_reset_452b10(PcRaceContext& c){
    auto& m=c.m;
    m.putf(m.u32(0x799d18u)+0x58u,m.f32(0x62806cu));
    call(c,0x44c0d0u,{});call(c,0x4af580u,{});
    m.put32(0x7d3a74u,0);
    if(!(m.i32(0x638e98u)<0x26))m.put32(0x638e98u,0x25u);
    m.put8(0x7d3a88u,0);m.put32(0x638ea0u,0xffffffffu);
    if(m.u32(0x7d3a84u)==0u){call(c,0x40ec60u,{0u});call(c,0x448fd0u,{0x100u,0x49a650u});}
}
void mode24_init_49a660(PcRaceContext& c){
    auto& m=c.m;
    ending_reset_452b10(c);
    m.put32(0x836848u,8u);
    m.put32(0x8367a0u,call(c,0x428320u,{0x330001u,1u,0u}));
    m.put32(0x8367a4u,call(c,0x428320u,{0x330000u,1u,0u}));
    call(c,0x429a10u,{});call(c,0x42dfd0u,{});call(c,0x4489c0u,{});call(c,0x44c3d0u,{});call(c,0x44a1a0u,{});
    for(std::uint32_t i=0;i<4u;++i)call(c,0x43de50u,{i});
    for(std::uint32_t i=0;i<3u;++i)call(c,0x4f11b0u,{i});
    for(std::uint32_t i=0;i<3u;++i)call(c,0x4f0600u,{i});
    call(c,0x46fc30u,{0u});call(c,0x46fc30u,{1u});
    call(c,0x4f2210u,{});
}
void mode24_control_49a710(PcRaceContext& c){
    auto& m=c.m;
    if(const std::uint32_t n=m.u32(0x836848u)){
        m.put32(0x836848u,n-1u);
        if(n-1u==0u)ending_choice_4527f0(c);
        return;
    }
    if(!call(c,0x42df90u,{})||!call(c,0x4299a0u,{}))return;
    for(std::uint32_t a:{0x8367a0u,0x8367a4u})
        if(m.u32(a)!=0xffffffffu){call(c,0x4285a0u,{m.u32(a)});m.put32(a,0xffffffffu);}
    ending_states_4524e0(c);
}
void mode24_exit_4527a0(PcRaceContext& c){
    auto& m=c.m;
    call(c,0x448fd0u,{0x100u,0u});call(c,0x40ec60u,{0xffffffffu});
    call(c,0x4489c0u,{});call(c,0x4f2210u,{});call(c,0x401030u,{0u});
    call(c,0x4285a0u,{m.u32(0x638ea0u)});
    call(c,0x42dfb0u,{0x43u});call(c,0x4299c0u,{0x43u});
}
// 4524B0: a variant-0 record (47EF30(preset)) or 4B1DD0.
std::uint32_t ending_record_4524b0(PcRaceContext& c){
    if(call(c,0x47ef30u,{c.m.u32(0x78024cu)}))return 1u;
    return call(c,0x4b1dd0u,{})?1u:0u;
}
void ending_choice_4527f0(PcRaceContext& c){
    auto& m=c.m;
    if(m.u32(0x7d3a78u)){
        call(c,0x401000u,{0u,0x1cu,1u});
        call(c,0x4af580u,{});
    }else{
        m.put8(0x7d3a80u,m.u8(m.u32(0x799d18u)+0x11u));
        const std::uint32_t preset=m.u32(0x78024cu),variant=m.u32(0x780258u);
        std::int32_t column=(std::int32_t(call(c,0x450380u,{8u}))-0xa)%5;
        m.put32(0x638e9cu,std::uint32_t(column));
        if(preset==0u||preset==2u){column+=5;m.put32(0x638e9cu,std::uint32_t(column));}
        m.put32(0x7d3a7cu,0);
        const bool fifteen=preset==0u||preset==2u;
        switch(variant){
        case 0:{
            const std::uint32_t r=ending_record_4524b0(c);
            m.put32(0x638e98u,(r?1u:0u)+0xeu);
            const std::uint32_t k=m.u32(0x638e9cu);
            m.put32(0x7d3a7cu,(k==0u||k==2u||k==4u||k==5u||k==7u||k==9u)?1u:3u);
            break;}
        case 2:{
            const std::uint32_t r=call(c,0x45bf30u,{4u})-3u;
            if(r<=2u){m.put32(0x638e98u,fifteen?0x24u:1u);}
            else if(r==3u){m.put32(0x638e98u,m.u32(0x5a464cu+std::uint32_t(column)*4u));}
            else if(r==4u){m.put32(0x638e98u,m.u32((fifteen?0x5a4674u:0x5a464cu)+m.u32(0x638e9cu)*4u));}
            else{m.put32(0x638e98u,0);if(fifteen)m.put32(0x638e98u,0x21u);}
            m.put32(0x7d3a7cu,3u);
            break;}
        default:   // 1 and the others
            m.put32(0x638e98u,m.u32(0x5a4624u+std::uint32_t(column)*4u));
            m.put32(0x7d3a7cu,1u);
            break;
        }
    }
    call(c,0x44fcc0u,{0u});call(c,0x44fce0u,{0u});
    request(c,0x57u,8u);
    request(c,0xd0u,2u);                                                // VM snippet 44794F: push 0xD0 (measured)
    request(c,0xbau,8u);request(c,0xbbu,8u);
    const std::uint32_t id=m.u32(0x638e98u);
    if(m.u32(0x5a4878u+id*0x4cu))request(c,call(c,0x46bbe0u,{std::uint32_t(m.u8(0x7d3a80u))}),8u);
    const std::int32_t col=m.i32(0x638e9cu);
    if(col>4&&col!=-1)request(c,m.u32(0x5a4740u+std::uint32_t(col)*24u),9u);
    request(c,m.u32(0x5a46a0u+(std::uint32_t(m.u8(0x7d3a88u))+std::uint32_t(col)*2u)*8u),9u);
    request(c,0xccu,2u);
    if(const std::int32_t set=m.i32(0x7d3a7cu))request(c,m.u32(0x5a4618u+std::uint32_t((set-1)%3)*4u),9u);
    else{request(c,0xcdu,9u);request(c,0xcfu,9u);}
    for(std::uint32_t i=0;i<15u;++i)request(c,m.u32(0x5a4838u+(m.u32(0x638e98u)*0x13u+i)*4u),9u);
    call(c,0x4f2020u,{m.u32(0x5a4874u+m.u32(0x638e98u)*0x4cu),0u});
    call(c,0x49a650u,{m.u32(0x5a4874u+m.u32(0x638e98u)*0x4cu)});      // RET over 4F2020's arguments
    call(c,0x42deb0u,{0x43u,9u});call(c,0x429920u,{0x43u,9u});
}
// 4523E0(EAX = ending, ECX = column): the three lights of 5A6AB8[column + ending * 10].
void ending_lights_4523e0(PcRaceContext& c,std::uint32_t ending,std::uint32_t column){
    auto& m=c.m;
    std::uint32_t l=m.u32(0x5a6ab8u+(column+ending*10u)*4u);
    for(std::uint32_t i=0;i<3u;++i,l+=0x40u){
        call(c,0x407cf0u,{i,0u,0u,m.u32(l),m.u32(l+4u),m.u32(l+8u)});
        call(c,0x407dd0u,{i,0u,0u,m.u32(l+0xcu),m.u32(l+0x10u),m.u32(l+0x14u)});
        call(c,0x408390u,{i,m.u32(l+0x18u),m.u32(l+0x1cu),m.u32(l+0x20u)});
        call(c,0x4083c0u,{i,m.u32(l+0x24u),m.u32(l+0x28u),m.u32(l+0x2cu)});
        const float k=m.f32(0x6281c4u);
        call(c,0x407f40u,{i,0u,0u,fb(m.f32(l+0x30u)*k),fb(m.f32(l+0x34u)*k)});
        call(c,0x408030u,{i,0u,0u,m.u32(l+0x38u)});
        call(c,0x407d60u,{i,0u,0u,m.u32(l+0x3cu)});
    }
}
void ending_states_4524e0(PcRaceContext& c){
    auto& m=c.m;
    auto state=[&]{return m.u32(0x7d3a74u);};
    const std::uint32_t s=state();
    bool tail=true;
    if(s>5u){
        for(std::uint32_t id:{8u,0x186u,0x187u,0x18du,0x181u,0x17fu,0x183u,6u})call(c,0x4401d0u,{id});
        call(c,0x440330u,{0x16au,0x15u});
        call(c,0x43f8c0u,{m.u32(0x780258u)==5u?0x1du:0x19u});
    }else switch(s){
    case 0:
        if(call(c,0x448b90u,{}))break;
        m.put32(0x7d3a74u,1u);
        [[fallthrough]];
    case 1:
        if(!call(c,0x448980u,{})||!call(c,0x4f21b0u,{})||!call(c,0x42df90u,{})||!call(c,0x4299a0u,{})||call(c,0x427700u,{0xa3u}))break;
        m.put32(0x7d3a74u,2u);
        [[fallthrough]];
    case 2:{
        const std::uint32_t id=m.u32(0x638e98u);
        if(m.u32(0x5a4878u+id*0x4cu))call(c,0x440110u,{8u,0x2du});
        call(c,0x440110u,{0x186u,0x44u});call(c,0x440110u,{0x187u,0x45u});call(c,0x440110u,{0x18du,0x1bu});
        call(c,0x440110u,{0x181u,0xdu});call(c,0x440110u,{0x17fu,0x17u});call(c,0x440110u,{0x183u,0x34u});
        call(c,0x440110u,{6u,0x19u});
        const std::int32_t col=m.i32(0x638e9cu);
        if(col>4&&col!=-1)call(c,0x4103f0u,{m.u32(0x5a4744u+std::uint32_t(col)*24u),5u});
        m.put32(0x638ea0u,call(c,0x428320u,{m.u32(0x638df0u+m.u32(0x638e98u)*4u),7u,1u}));
        m.put32(0x7d3a74u,3u);
        break;}
    case 3:
        call(c,0x4b5f60u,{m.u32(0x5a4830u+m.u32(0x638e98u)*0x4cu)});
        m.put32(0x7d3a74u,4u);
        [[fallthrough]];
    case 4:
        if(call(c,0x4b5fd0u,{})!=3u)break;
        call(c,0x44fcc0u,{0u});call(c,0x44fce0u,{0u});
        ending_lights_4523e0(c,m.u32(0x638e98u),m.u32(0x638e9cu));
        call(c,0x4afba0u,{});
        m.put32(0x7d3a74u,5u);
        [[fallthrough]];
    case 5:{
        if(m.u32(0x638e98u)==6u&&std::int32_t(call(c,0x4b5fc0u,{}))>=0x460){
            const std::uint32_t a=m.u32(0x79f010u),b=m.u32(0x79f04cu);
            m.put32(a+4u,m.u32(a+4u)|0x10000u);m.put32(b+4u,m.u32(b+4u)|0x10000u);
        }
        const std::uint32_t st=call(c,0x4b5fd0u,{});
        bool done=false;
        if(st==5u||call(c,0x4b5fd0u,{})==0u)done=call(c,0x428880u,{m.u32(0x638ea0u)})==3u;
        if(!done)done=call(c,0x4bfb20u,{})!=0u;
        if(done)m.put32(0x7d3a74u,6u);
        break;}
    default:break;
    }
    if(tail)call(c,0x428730u,{m.u32(0x638ea0u),m.u32(0x7d3a84u)==0u?1u:0u});
}
// 451D10 (event 0x187 init): three lights 451800(i, 0, 1, 1500, 0), direction (-pi/4, pi/4),
// 300, white diffuse / specular, zero ambient, 0.255 colours.
void ending_lights_init_451d10(PcRaceContext& c){
    for(std::uint32_t i=0;i<3u;++i){
        call(c,0x451800u,{i,0u,0x3f800000u,0x44bb8000u,0u});
        call(c,0x407f40u,{i,0u,0u,0xbf490fdbu,0x3f490fdbu});
        call(c,0x408030u,{i,0u,0u,0x43960000u});
        call(c,0x407cf0u,{i,0u,0u,0x3f800000u,0x3f800000u,0x3f800000u});
        call(c,0x407dd0u,{i,0u,0u,0x3f800000u,0x3f800000u,0x3f800000u});
        call(c,0x407e40u,{i,0u,0u,0u,0u,0u});
        call(c,0x408390u,{i,0x3e828f5cu,0x3e828f5cu,0x3e828f5cu});
        call(c,0x4083c0u,{i,0x3e828f5cu,0x3e828f5cu,0x3e828f5cu});
    }
}
// 4522D0 (event 0x187 display): the course column's background model (5A4744 + column * 24:
// token, position, alpha) without depth test.
void ending_background_4522d0(PcRaceContext& c){
    auto& m=c.m;
    const std::int32_t col=m.i32(0x638e9cu);
    if(!(col>4))return;                                                // 452370: -1 unless column > 4
    draw(c,PcRaceSetRenderState,{7u,0u});
    const std::uint32_t row=0x5a4744u+std::uint32_t(col)*24u;
    const std::uint32_t token=m.u32(row);const float alpha=m.f32(row+0x10u);
    driving::pc_matrix_push_unit(c.matrices);
    driving::pc_matrix_translate_vector(c.matrices,{m.f32(row+4u),m.f32(row+8u),m.f32(row+0xcu)});
    background_452080(c,token,alpha);
    driving::pc_matrix_pop(c.matrices);
    draw(c,PcRaceSetRenderState,{7u,1u});
}
// 49F410 (event 8 function 0x2D init): the player car for the ending: model [7D3A80], colour
// (its own +12), cleared motion words and angles, +4 bit 0 set / 0x42 cleared, +30C = [6281C0],
// identity matrices +B0 / +F0, 4F6E40, 46BA20 (shadow), 46BBC0 (environment map).
void ending_car_init_49f410(PcRaceContext& c,std::uint32_t car){
    auto& m=c.m;
    m.put8(car+0x11u,m.u8(0x7d3a80u));
    m.put8(car+0x12u,m.u8(m.u32(0x799d18u)+0x12u));
    for(std::uint32_t o:{0x30u,0x2eu,0x2cu,0x160u,0x162u,0x166u,0x164u})m.put16(car+o,0);
    m.putf(car+0x1cu,0.0f);m.putf(car+0x18u,0.0f);m.putf(car+0x14u,0.0f);m.putf(car+0x1c4u,0.0f);
    m.put32(car+4u,(m.u32(car+4u)&0xffffffbdu)|1u);
    m.put32(car+0x30cu,m.u32(0x6281c0u));
    for(std::uint32_t mt:{car+0xb0u,car+0xf0u})for(std::uint32_t k=0;k<16u;++k)m.putf(mt+k*4u,(k%5u)==0u?1.0f:0.0f);
    call(c,0x4f6e40u,{car});call(c,0x46ba20u,{car});call(c,0x46bbc0u,{car});
}
std::uint32_t ending_model_452340(PcRaceContext& c){
    auto& m=c.m;
    const std::uint32_t at=0x5a46a4u+(std::uint32_t(m.u8(0x7d3a88u))+m.u32(0x638e9cu)*2u)*8u;
    const std::uint32_t h=call(c,0x448cd0u,{m.u32(at)});
    return h==0xffffffffu?2u:h;
}
void ending_area_display_44b890(PcRaceContext& c){
    driving::pc_matrix_push_unit(c.matrices);
    draw(c,0x4044e0u,{6u});
    const std::uint32_t token=ending_model_452340(c);
    draw(c,0x405360u,{token,0u,0u,0u,0xffffffffu,0u});
    draw(c,0x4044e0u,{7u});
    driving::pc_matrix_pop(c.matrices);
}
void ending_car_dest_49f4e0(PcRaceContext& c,std::uint32_t car){call(c,0x49a650u,{car});call(c,0x46bb20u,{car});}
}

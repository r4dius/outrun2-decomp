#include "platform/vehicle_model_draw.hpp"
#include "driving/pc_x87.hpp"
#include "driving/pc_car_services_ghost.hpp"
#include "platform/embedded_vehicle_draw_data.hpp"
#include <cmath>
#include <stdexcept>
namespace outrun::platform {
namespace {
using driving::Bytes;
using driving::CourseProbe;
using driving::PcMatrixStack;
using driving::X87;using driving::x87_atan2;using driving::x87_cos;
using X=X87;
constexpr float AngleUnit=9.58738019107841e-05f; // 628254: 2pi/65536
constexpr std::uint32_t None=0xffffffffu;
float word_angle(std::int32_t w){return float(X(w)*AngleUnit);} // 4493A0 * 628254
CourseProbe get3(Bytes b,std::size_t o){return {b.f32(o),b.f32(o+4),b.f32(o+8)};}
bool nonzero(float v){return !(v==0.f);} // UCOMISS/LAHF/JNP: NaN rotates
// Model-5 roof angle (46B264 and the protected bridge at 46B5BD).
void roof_angle(Bytes car){
    float x1=car.f32(0x1c4)*216.720001f;x1*=0.729166687f;
    float out;
    if(300.f>=x1){x1*=0.00333333341f;x1*=-0.305432618f;out=x1;}
    else{
        float x0=350.f-x1;x0*=0.0199999996f;x0*=-0.305432618f;
        out=x0>0.f?0.f:x0;
    }
    car.putf(0x2fc,out);
}
}
std::array<float,3> matrix_angles_449640(Bytes m){return driving::pc_matrix_angles_449640(m);}
void vehicle_model_draw_46ae70(const PcVehicleModelDraw& in,PcMatrixStack& s,std::vector<PcVehicleDrawCall>& calls){
    Bytes car=in.car;car.check(0,0xb6c);in.body_82e7f0.check(0x288,0x564-0x288+4);
    const std::int32_t index=in.model+in.variant;
    if(index<0||index>=30)throw std::out_of_range("46AE70 layout index outside 5B2F68's thirty records");
    const Bytes layout(const_cast<std::uint8_t*>(EmbeddedVehicleLayouts)+std::size_t(index)*0x128,0x128);
    const auto car_model=car.i8(0x11);
    if(car_model<0||car_model>=30)throw std::out_of_range("46AE70 colour table index");
    const std::uint32_t ct=EmbeddedVehicleColourIds[car_model],cb=car.u8(0x12),fade=car.u32(0xb68);
    auto record=[&](std::uint32_t pc,std::initializer_list<std::uint32_t> args){
        PcVehicleDrawCall c{};c.pc=pc;c.argc=std::uint32_t(args.size());
        unsigned k=0;for(auto a:args)c.args[k++]=a;
        const auto m=s.current();for(unsigned i=0;i<64;++i)c.matrix[i]=m.u8(i);
        calls.push_back(c);
    };
    auto draw_a=[&](std::uint32_t o){record(0x405360,{o,1,0,ct,cb,0});};
    auto draw_b=[&](std::uint32_t o){record(0x4056d0,{o,fade,ct,cb});};
    auto draw_a0=[&](std::uint32_t o){record(0x405360,{o,1,0,0,None,0});};
    auto draw_b0=[&](std::uint32_t o){record(0x4056d0,{o,fade,0,None});};
    auto states=[&]{record(0x4044f0,{0,0,0,0x80,0x1000000f});record(0x4044f0,{1,1,0,8,0xf});};
    auto alt_states=[&]{record(0x4044f0,{0,0,0,8,0x10000000});record(0x4044f0,{1,0,0,8,0});record(0x405350,{});
        record(0x4044f0,{0,1,0,8,7});record(0x4044f0,{1,1,0,8,7});};
    static constexpr std::size_t Wheels[4]{0x288,0x37c,0x470,0x564};
    std::array<float,4> spin{},steer{};
    for(unsigned k=0;k<4;++k){spin[k]=word_angle(in.body_82e7f0.i16(Wheels[k]));steer[k]=word_angle(in.body_82e7f0.i16(Wheels[k]+2));}
    const std::uint32_t age=car.u32(0x1f4);const std::int32_t c38=car.i32(0x38);
    std::uint32_t flags=car.u32(4);
    flags=layout.u32(0x64)==None?(flags|0x800u):(flags&~0x800u);car.put32(4,flags);
    const std::uint32_t vm=(flags>>14)&3u;std::int32_t alt=std::int32_t((flags>>6)&1u);
    std::array<std::uint8_t,64> local{};Bytes l(local.data(),64);
    if(vm==1)for(unsigned k=0;k<64;++k)local[k]=car.u8(0xf0+k);
    else for(unsigned k=0;k<16;++k)l.putf(k*4,(k%5u)==0u?1.f:0.f);
    if(in.debug_alt_64d198>=0)alt=in.debug_alt_64d198;
    {   // 409FD0: push and pre-multiply by the local matrix.
        const bool fits=s.depth+1<s.capacity;driving::pc_matrix_push(s);
        if(fits)driving::pc_matrix_multiply_current(s,l);
    }
    states();if(alt==1)record(0x404540,{});states();record(0x4052b0,{});
    auto translate=[&](CourseProbe v){driving::pc_matrix_translate_vector(s,v);};
    if(vm==1){
        for(unsigned i=0;i<2;++i){const auto e=layout.sub(0x28+i*0x10,0x10);
            driving::pc_matrix_push(s);translate(get3(e,4));
            if(alt==0)draw_a(e.u32(0));else draw_b(e.u32(0));
            driving::pc_matrix_pop(s);}
        const auto o14=layout.u32(0x14);bool second_a=false;
        if(o14!=None){if(alt==0){draw_a(o14);second_a=true;}else draw_b(o14);}
        if(second_a||alt==0)draw_a(layout.u32(0));else draw_b(layout.u32(0));
        driving::pc_matrix_push(s);translate(get3(layout,0x4c));driving::pc_matrix_rotate_x(s,layout.f32(0x58));
        {   // Protected bridge 1039BB0: steering word +204, negated.
            const std::int32_t w=-std::int32_t(car.i16(0x204));
            driving::pc_matrix_rotate_z(s,float((X(w)*AngleUnit)*0.416666657f));
        }
        if(alt==1)draw_b0(layout.u32(0x48));else draw_a0(layout.u32(0x48));
        driving::pc_matrix_pop(s);
        if(layout.u32(0x18)!=None){
            driving::pc_matrix_push(s);translate(get3(layout,0x1c));
            if(car.i8(0x11)==5){
                roof_angle(car);driving::pc_matrix_rotate_x(s,car.f32(0x2fc));
                if(alt==0)draw_a(layout.u32(0x18));else draw_b(layout.u32(0x18));
            }else if(alt==1)draw_b0(layout.u32(0x18));else draw_a0(layout.u32(0x18));
            driving::pc_matrix_pop(s);
        }
    }else if(vm==2){
        if(alt==0)draw_a(layout.u32(4));else draw_b(layout.u32(4));
    }
    if(layout.u32(0x64)!=None){
        const auto f=car.u32(4);
        const float top=layout.f32(0x78);
        if(f&0x500u){
            if(top>car.f32(0x2fc)){
                float v=top/layout.f32(0x74);v+=car.f32(0x2fc);car.putf(0x2fc,v);
                if(v>top)car.putf(0x2fc,top);
            }else car.put32(4,f|0x800u);
        }else if(car.f32(0x2fc)>0.f){
            float v=car.f32(0x2fc)-top/layout.f32(0x74);car.putf(0x2fc,v);
            if(0.f>v)car.putf(0x2fc,0.f);
        }
        const float roof=car.f32(0x2fc);
        if(vm!=0){
            driving::pc_matrix_push(s);translate(get3(layout,0x68));driving::pc_matrix_rotate_x(s,roof);
            if(alt==0)draw_a(layout.u32(0x64));else draw_b(layout.u32(0x64));
            driving::pc_matrix_pop(s);
        }
    }
    {
        const auto o60=layout.u32(0x60);const auto f=car.u32(4);
        if(o60!=None&&(f&0x100u)&&(f&0x800u)){if(alt==1)draw_b0(o60);else draw_a0(o60);}
    }
    bool to_alt_states=false;
    if(vm==1){
        if(c38==0){
            const auto obj=(car.u32(4)&0x300u)?layout.u32(0x80):layout.u32(0x7c);
            if(alt==1){draw_b0(obj);to_alt_states=true;}else draw_a0(obj);
        }else{
            if(alt==1)draw_b0(layout.u32(0x84));else draw_a0(layout.u32(0x84));
            if(car.i8(0x11)==5){
                driving::pc_matrix_push(s);translate(get3(layout,0x1c));
                roof_angle(car); // protected bridge 1039D08
                driving::pc_matrix_rotate_x(s,car.f32(0x2fc));
                if(alt==1)draw_b0(0x4001a);else draw_a0(0x4001a);
                driving::pc_matrix_pop(s);
            }
        }
    }
    if(to_alt_states||alt==1)alt_states();
    record(0x4052c0,{});if(alt==1)record(0x404540,{});
    driving::pc_matrix_pop(s);
    if(alt==0&&vm==1&&layout.u32(0xc)!=None){
        // Shadow: 49A650 is a bare RET in this build, but the matrix and the
        // 449640 row normalisation of +B0 still happen.
        driving::pc_matrix_push_unit(s);translate(get3(car,0x14));
        driving::pc_matrix_rotate_y(s,word_angle(car.i16(0x2e)));
        const auto f=car.u32(4);
        if(f&0x800000u){
            translate({car.f32(0x2d8),0.f,car.f32(0x2e0)});
            if(in.scene_82e7d4==6||in.scene_82e7d4==15){
                const auto angles=matrix_angles_449640(car.sub(0xb0,0x30));
                driving::pc_matrix_rotate_y(s,angles[1]);
            }else driving::pc_matrix_rotate_y(s,car.f32(0x2e8));
        }else if(car.f32(0x2c8)>0.f){
            driving::pc_matrix_rotate_y(s,car.f32(0x2e8));
        }
        driving::pc_matrix_pop(s);
    }
    states();
    if(vm!=1){record(0x404540,{});return;}
    record(0x4052b0,{});
    for(unsigned k=0;k<4;++k){
        const auto w=layout.sub(0x8c+k*0x20,0x20);
        driving::pc_matrix_push(s);translate(get3(car,0x130+k*0xc));
        if(nonzero(w.f32(0x18)))driving::pc_matrix_rotate_z(s,w.f32(0x18));
        if(nonzero(steer[k]))driving::pc_matrix_rotate_y(s,steer[k]);
        if(w.u32(0)!=None){if(alt==1)draw_b0(w.u32(0));else draw_a0(w.u32(0));}
        if(nonzero(spin[k]))driving::pc_matrix_rotate_x(s,spin[k]);
        const auto hub=layout.u32(0x88+k*0x20);
        if(alt==1)draw_b0(hub);else draw_a0(hub);
        if(w.u32(4)!=None){
            const auto obj=(c38>200&&age!=0)?w.u32(8):w.u32(4);
            if(alt==1)draw_b0(obj);else draw_a0(obj);
        }
        driving::pc_matrix_pop(s);
        if(alt==1)alt_states();
    }
    record(0x4052c0,{});if(alt==1)record(0x404540,{});
    record(0x404540,{});
}
void vehicle_select_model_draw_46a560(const PcVehicleModelDraw& in,PcMatrixStack& s,std::vector<PcVehicleDrawCall>& calls){
    Bytes car=in.car;
    if(!car.size())throw std::invalid_argument("46A560 without a car (the PC faults on the colour bytes)");
    car.check(0,0xb6c);in.body_82e7f0.check(0x288,0x564-0x288+4);
    const std::int32_t index=in.model+in.variant;
    if(index<0||index>=30)throw std::out_of_range("46A560 layout index outside 5B2F68's thirty records");
    const Bytes layout(const_cast<std::uint8_t*>(EmbeddedVehicleLayouts)+std::size_t(index)*0x128,0x128);
    const auto car_model=car.i8(0x11);
    if(car_model<0||car_model>=30)throw std::out_of_range("46A560 colour table index");
    const std::uint32_t ct=EmbeddedVehicleColourIds[car_model],cb=car.u8(0x12);
    auto record=[&](std::uint32_t pc,std::initializer_list<std::uint32_t> args){
        PcVehicleDrawCall c{};c.pc=pc;c.argc=std::uint32_t(args.size());
        unsigned k=0;for(auto a:args)c.args[k++]=a;
        const auto m=s.current();for(unsigned i=0;i<64;++i)c.matrix[i]=m.u8(i);
        calls.push_back(c);
    };
    auto draw_a=[&](std::uint32_t o){record(0x405360,{o,1,0,ct,cb,0});};
    auto draw_0=[&](std::uint32_t o){record(0x405360,{o,1,0,0,None,0});};
    auto states=[&]{record(0x4044f0,{0,0,0,0x80,0x1000000f});record(0x4044f0,{1,1,0,8,0xf});};
    auto translate=[&](CourseProbe v){driving::pc_matrix_translate_vector(s,v);};
    constexpr std::uint32_t Light1=0x899b98u+160u+0x44u;                          // 4082B0(1,0,0)+44
    static constexpr std::size_t Wheels[4]{0x288,0x37c,0x470,0x564};
    std::array<float,4> spin{},steer{};
    for(unsigned k=0;k<4;++k){spin[k]=word_angle(in.body_82e7f0.i16(Wheels[k]));steer[k]=word_angle(in.body_82e7f0.i16(Wheels[k]+2));}
    const std::int32_t c38=car.i32(0x38);const std::uint32_t age=car.u32(0x1f4);
    std::uint32_t flags=car.u32(4);
    flags=layout.u32(0x64)==None?(flags|0x800u):(flags&~0x800u);car.put32(4,flags);
    const std::uint32_t vm=(flags>>14)&3u;
    std::array<std::uint8_t,64> local{};Bytes l(local.data(),64);
    if(vm==1)for(unsigned k=0;k<64;++k)local[k]=car.u8(0xf0+k);
    else for(unsigned k=0;k<16;++k)l.putf(k*4,(k%5u)==0u?1.f:0.f);              // 40A060
    {   // 409FD0: push and pre-multiply by the local matrix.
        const bool fits=s.depth+1<s.capacity;driving::pc_matrix_push(s);
        if(fits)driving::pc_matrix_multiply_current(s,l);
    }
    if(vm==1&&layout.u32(8)!=None)record(0x422550,{car.u32(0x2bc),Light1});
    states();record(0x4052b0,{});
    if(vm==1){
        for(unsigned i=0;i<2;++i){const auto e=layout.sub(0x28+i*0x10,0x10);
            driving::pc_matrix_push(s);translate(get3(e,4));draw_a(e.u32(0));driving::pc_matrix_pop(s);}
        if(layout.u32(0x14)!=None)draw_a(layout.u32(0x14));
    }
    record(0x4052c0,{});
    if(vm==1&&layout.u32(0x10)!=None)record(0x422550,{car.u32(0x2c4),Light1});
    states();record(0x4052b0,{});
    if(vm==1){
        draw_a(layout.u32(0));
        driving::pc_matrix_push(s);translate(get3(layout,0x4c));driving::pc_matrix_rotate_x(s,layout.f32(0x58));
        driving::pc_matrix_rotate_z(s,float((X(-std::int32_t(car.i16(0x204)))*AngleUnit)*0.416666657f));   // 5B3A24
        draw_0(layout.u32(0x48));
        driving::pc_matrix_pop(s);
        if(layout.u32(0x18)!=None){
            driving::pc_matrix_push(s);translate(get3(layout,0x1c));
            if(car_model==5){roof_angle(car);driving::pc_matrix_rotate_x(s,car.f32(0x2fc));draw_a(layout.u32(0x18));}
            else draw_0(layout.u32(0x18));
            driving::pc_matrix_pop(s);
        }
    }else if(vm==2)draw_a(layout.u32(4));
    if(layout.u32(0x64)!=None){
        const auto f=car.u32(4);const float top=layout.f32(0x78);
        if(f&0x500u){
            if(top>car.f32(0x2fc)){
                float v=top/layout.f32(0x74);v+=car.f32(0x2fc);car.putf(0x2fc,v);
                if(v>top)car.putf(0x2fc,top);
            }else car.put32(4,f|0x800u);
        }else if(car.f32(0x2fc)>0.f){
            float v=car.f32(0x2fc)-top/layout.f32(0x74);car.putf(0x2fc,v);
            if(0.f>v)car.putf(0x2fc,0.f);
        }
        if(vm!=0){
            driving::pc_matrix_push(s);translate(get3(layout,0x68));driving::pc_matrix_rotate_x(s,car.f32(0x2fc));
            draw_a(layout.u32(0x64));
            driving::pc_matrix_pop(s);
        }
    }
    {const auto o60=layout.u32(0x60);const auto f=car.u32(4);
     if(o60!=None&&(f&0x100u)&&(f&0x800u))draw_0(o60);}
    if(vm==1){
        if(c38==0)draw_0((car.u32(4)&0x300u)?layout.u32(0x80):layout.u32(0x7c));
        else{
            draw_0(layout.u32(0x84));
            if(car_model==5){
                driving::pc_matrix_push(s);translate(get3(layout,0x1c));
                driving::pc_matrix_rotate_x(s,car.f32(0x2fc));
                draw_0(0x4001a);
                driving::pc_matrix_pop(s);
            }
        }
    }
    record(0x4052c0,{});
    driving::pc_matrix_pop(s);
    if(vm==1&&layout.u32(0xc)!=None){
        // Shadow: 49A650 is a bare RET in this build, but the matrix and the
        // 449640 row normalisation of +B0 still happen.
        driving::pc_matrix_push_unit(s);translate(get3(car,0x14));
        driving::pc_matrix_rotate_y(s,word_angle(car.i16(0x2e)));
        const auto f=car.u32(4);
        if(f&0x800000u){
            translate({car.f32(0x2d8),0.f,car.f32(0x2e0)});
            if(in.scene_82e7d4==6||in.scene_82e7d4==15){
                const auto angles=matrix_angles_449640(car.sub(0xb0,0x30));
                driving::pc_matrix_rotate_y(s,angles[1]);
            }else driving::pc_matrix_rotate_y(s,car.f32(0x2e8));
        }else if(car.f32(0x2c8)>0.f){
            driving::pc_matrix_rotate_y(s,car.f32(0x2e8));
        }
        driving::pc_matrix_pop(s);
    }
    states();
    if(vm!=1){record(0x404540,{});return;}
    record(0x4052b0,{});
    for(unsigned k=0;k<4;++k){
        const auto w=layout.sub(0x8c+k*0x20,0x20);
        driving::pc_matrix_push(s);translate(get3(car,0x130+k*0xc));
        if(nonzero(w.f32(0x18)))driving::pc_matrix_rotate_z(s,w.f32(0x18));
        if(nonzero(steer[k]))driving::pc_matrix_rotate_y(s,steer[k]);
        if(w.u32(0)!=None)draw_0(w.u32(0));
        if(nonzero(spin[k]))driving::pc_matrix_rotate_x(s,spin[k]);
        draw_0(layout.u32(0x88+k*0x20));
        if(w.u32(4)!=None)draw_0((c38>200&&age!=0)?w.u32(8):w.u32(4));
        driving::pc_matrix_pop(s);
    }
    record(0x4052c0,{});
    record(0x404540,{});
}
void vehicle_ending_model_draw_469ff0(Bytes car,std::int32_t model,PcMatrixStack& s,std::vector<PcVehicleDrawCall>& calls){
    car.check(0,0x300);
    if(model<0||model>=30)throw std::out_of_range("469FF0 layout index outside 5B2F68's thirty records");
    const Bytes layout(const_cast<std::uint8_t*>(EmbeddedVehicleLayouts)+std::size_t(model)*0x128,0x128);
    const auto car_model=car.i8(0x11);
    if(car_model<0||car_model>=30)throw std::out_of_range("469FF0 colour table index");
    const std::uint32_t colour=EmbeddedVehicleColourIds[car_model],cbyte=car.u8(0x12);
    auto record=[&](std::uint32_t pc,std::initializer_list<std::uint32_t> args){
        PcVehicleDrawCall c{};c.pc=pc;c.argc=std::uint32_t(args.size());
        unsigned k=0;for(auto a:args)c.args[k++]=a;
        const auto m=s.current();for(unsigned i=0;i<64;++i)c.matrix[i]=m.u8(i);
        calls.push_back(c);
    };
    auto translate=[&](CourseProbe v){driving::pc_matrix_translate_vector(s,v);};
    auto draw_c=[&](std::uint32_t o){record(0x405360,{o,1,0,colour,cbyte,0});};
    auto draw_0=[&](std::uint32_t o){record(0x405360,{o,1,0,0,None,0});};
    constexpr std::uint32_t Light1=0x899b98u+160u+0x44u;                          // 4082B0(1,0,0)+44
    const std::array<float,4> spin{word_angle(car.i16(0x40)),word_angle(car.i16(0x42)),word_angle(car.i16(0x44)),word_angle(car.i16(0x46))};
    const std::array<float,4> steer{word_angle(car.i16(0x32)),word_angle(car.i16(0x32)),0.f,0.f};
    const std::int32_t c38=car.i32(0x38);const std::uint32_t age=car.u32(0x1f4);
    std::uint32_t flags=car.u32(4);
    flags=layout.u32(0x64)==None?(flags|0x800u):(flags&~0x800u);car.put32(4,flags);
    if(layout.u32(8)!=None)record(0x422550,{car.u32(0x2bc),Light1});
    record(0x4052b0,{});
    for(unsigned i=0;i<2;++i){const auto e=layout.sub(0x28+i*0x10,0x10);
        driving::pc_matrix_push(s);translate(get3(e,4));
        driving::pc_matrix_rotate_y(s,word_angle(car.i16(0x164+i*2)));
        draw_c(e.u32(0));driving::pc_matrix_pop(s);}
    if(layout.u32(0x14)!=None)draw_c(layout.u32(0x14));
    record(0x4052c0,{});
    if(layout.u32(0x10)!=None)record(0x422550,{car.u32(0x2c4),Light1});           // bridge 46A190
    record(0x422740,{car.u32(0x2bc)});
    record(0x4052b0,{});
    draw_c(layout.u32(0));
    driving::pc_matrix_push(s);translate(get3(layout,0x4c));driving::pc_matrix_rotate_x(s,layout.f32(0x58));
    draw_0(layout.u32(0x48));
    driving::pc_matrix_pop(s);
    if(layout.u32(0x18)!=None){
        driving::pc_matrix_push(s);translate(get3(layout,0x1c));
        if(car_model==5){roof_angle(car);driving::pc_matrix_rotate_x(s,car.f32(0x2fc));draw_c(layout.u32(0x18));}
        else draw_0(layout.u32(0x18));
        driving::pc_matrix_pop(s);
    }
    if(layout.u32(0x64)!=None){
        const auto f=car.u32(4);const float top=layout.f32(0x78);
        if(f&0x100u){
            if(top>car.f32(0x2fc)){
                float v=top/layout.f32(0x74);v+=car.f32(0x2fc);car.putf(0x2fc,v);
                if(v>top)car.putf(0x2fc,top);
            }else car.put32(4,f|0x800u);
        }else if(car.f32(0x2fc)>0.f){
            float v=car.f32(0x2fc)-top/layout.f32(0x74);car.putf(0x2fc,v);
            if(0.f>v)car.putf(0x2fc,0.f);
        }
        driving::pc_matrix_push(s);translate(get3(layout,0x68));driving::pc_matrix_rotate_x(s,car.f32(0x2fc));
        draw_c(layout.u32(0x64));
        driving::pc_matrix_pop(s);
    }
    draw_0(layout.u32(0xc));
    {const auto o60=layout.u32(0x60);const auto f=car.u32(4);
     if(o60!=None&&(f&0x100u)&&(f&0x800u))draw_0(o60);}
    if(c38==0)draw_0((car.u32(4)&0x300u)?layout.u32(0x80):layout.u32(0x7c));
    else{
        draw_0(layout.u32(0x84));
        if(car_model==5){
            driving::pc_matrix_push(s);translate(get3(layout,0x1c));
            driving::pc_matrix_rotate_x(s,car.f32(0x2fc));
            draw_0(0x4001a);
            driving::pc_matrix_pop(s);
        }
    }
    for(unsigned k=0;k<4;++k){
        const auto w=layout.sub(0x8c+k*0x20,0x20);
        driving::pc_matrix_push(s);translate(get3(w,0xc));
        if(nonzero(w.f32(0x18)))driving::pc_matrix_rotate_z(s,w.f32(0x18));
        if(nonzero(steer[k]))driving::pc_matrix_rotate_y(s,steer[k]);
        if(w.u32(0)!=None)draw_0(w.u32(0));
        if(nonzero(spin[k]))driving::pc_matrix_rotate_x(s,spin[k]);
        draw_0(layout.u32(0x88+k*0x20));
        if(w.u32(4)!=None)draw_0((c38>200&&age!=0)?w.u32(8):w.u32(4));
        driving::pc_matrix_pop(s);
    }
    record(0x4052c0,{});
}
void vehicle_race_model_draw_469600(const PcRaceModelDraw& in,PcMatrixStack& s,std::vector<PcVehicleDrawCall>& calls){
    Bytes car=in.car;car.check(0,0xea0);in.body_82e7f0.check(0x288,0x564-0x288+4);
    const std::int32_t index=in.model+in.variant;
    if(index<0||index>=30)throw std::out_of_range("469600 layout index outside 5B2F68's thirty records");
    const Bytes layout(const_cast<std::uint8_t*>(EmbeddedVehicleLayouts)+std::size_t(index)*0x128,0x128);
    auto record=[&](std::uint32_t pc,std::initializer_list<std::uint32_t> args){
        PcVehicleDrawCall c{};c.pc=pc;c.argc=std::uint32_t(args.size());
        unsigned k=0;for(auto a:args)c.args[k++]=a;
        const auto m=s.current();for(unsigned i=0;i<64;++i)c.matrix[i]=m.u8(i);
        calls.push_back(c);
    };
    auto translate=[&](CourseProbe v){driving::pc_matrix_translate_vector(s,v);};
    static constexpr std::size_t Wheels[4]{0x288,0x37c,0x470,0x564};
    std::array<float,4> spin{},steer{};
    for(unsigned k=0;k<4;++k){spin[k]=word_angle(in.body_82e7f0.i16(Wheels[k]));steer[k]=word_angle(in.body_82e7f0.i16(Wheels[k]+2));}
    const std::int32_t c38=car.i32(0x38);const std::uint32_t age=car.u32(0x1f4);
    std::uint32_t flags=car.u32(4);
    flags=layout.u32(0x64)==None?(flags|0x800u):(flags&~0x800u);car.put32(4,flags);
    const std::uint32_t vm=(flags>>14)&3u;
    std::int32_t alt=0;
    std::array<std::uint8_t,64> local{};Bytes l(local.data(),64);
    if(vm==2&&1.f>car.f32(0xe9c))alt=1;
    if(vm==1)for(unsigned k=0;k<64;++k)local[k]=car.u8(0xf0+k);
    else for(unsigned k=0;k<16;++k)l.putf(k*4,(k%5u)==0u?1.f:0.f);           // 40A060
    const auto car_model=car.i8(0x11);
    if(car_model<0||car_model>=30)throw std::out_of_range("469600 colour table index");
    const std::uint32_t colour=EmbeddedVehicleColourIds[car_model];
    const std::uint32_t cbyte=car.u8(0x12),fade=car.u32(0xe9c);
    auto draw_c=[&](std::uint32_t o){record(0x405360,{o,1,0,colour,cbyte,0});};
    auto draw_0=[&](std::uint32_t o){record(0x405360,{o,1,0,0,None,0});};
    auto draw_alt=[&](std::uint32_t o){if(alt)record(0x4056d0,{o,fade,colour,cbyte});else draw_c(o);};
    constexpr std::uint32_t Light1=0x899b98u+160u+0x44u;                          // 4082B0(1,0,0)+44
    {   // 409FD0: push and pre-multiply by the local matrix.
        const bool fits=s.depth+1<s.capacity;driving::pc_matrix_push(s);
        if(fits)driving::pc_matrix_multiply_current(s,l);
    }
    if(vm==1&&layout.u32(8)!=None)record(0x422550,{car.u32(0x2bc),Light1});
    record(0x4052b0,{});
    if(vm==1){
        for(unsigned i=0;i<2;++i){const auto e=layout.sub(0x28+i*0x10,0x10);
            driving::pc_matrix_push(s);translate(get3(e,4));draw_c(e.u32(0));driving::pc_matrix_pop(s);}
        if(layout.u32(0x14)!=None)draw_c(layout.u32(0x14));
    }
    record(0x4052c0,{});
    if(vm==1){                                                                    // bridge 46989F
        if(layout.u32(0x10)!=None)record(0x422550,{car.u32(0x2c4),Light1});
        if(car.u32(0x2bc)!=0u)record(0x422740,{car.u32(0x2bc)});
    }
    record(0x4052b0,{});
    if(vm==1){
        draw_c(layout.u32(0));
        driving::pc_matrix_push(s);translate(get3(layout,0x4c));driving::pc_matrix_rotate_x(s,layout.f32(0x58));
        {const std::int32_t w=-std::int32_t(car.i16(0x204));
         driving::pc_matrix_rotate_z(s,float((X(w)*AngleUnit)*0.416666657f));}
        draw_0(layout.u32(0x48));
        driving::pc_matrix_pop(s);
        if(layout.u32(0x18)!=None){
            driving::pc_matrix_push(s);translate(get3(layout,0x1c));
            if(car_model==5){roof_angle(car);driving::pc_matrix_rotate_x(s,car.f32(0x2fc));draw_c(layout.u32(0x18));}
            else draw_0(layout.u32(0x18));
            driving::pc_matrix_pop(s);
        }
    }else if(vm==2)draw_alt(layout.u32(4));
    if(layout.u32(0x64)!=None){
        const auto f=car.u32(4);const float top=layout.f32(0x78);
        if(f&0x500u){
            if(top>car.f32(0x2fc)){
                float v=top/layout.f32(0x74);v+=car.f32(0x2fc);car.putf(0x2fc,v);
                if(v>top)car.putf(0x2fc,top);
            }else car.put32(4,f|0x800u);
        }else{
            if(car.f32(0x2fc)>0.f){
                float v=car.f32(0x2fc)-top/layout.f32(0x74);car.putf(0x2fc,v);
                if(0.f>v)car.putf(0x2fc,0.f);
            }
            car.put32(4,f&~0x800u);
        }
        const float roof=car.f32(0x2fc);
        if(vm!=0){
            driving::pc_matrix_push(s);translate(get3(layout,0x68));driving::pc_matrix_rotate_x(s,roof);
            draw_alt(layout.u32(0x64));
            driving::pc_matrix_pop(s);
        }
    }
    if(vm!=0){
        const auto o60=layout.u32(0x60);const auto f=car.u32(4);
        if(o60!=None&&(f&0x100u)&&(f&0x800u))draw_0(o60);
    }
    if(vm==1){
        if(c38==0)draw_0((car.u32(4)&0x300u)?layout.u32(0x80):layout.u32(0x7c));
        else{
            if(layout.u32(0x84)!=None)draw_0(layout.u32(0x84));
            if(car_model==5){
                driving::pc_matrix_push(s);translate(get3(layout,0x1c));
                driving::pc_matrix_rotate_x(s,car.f32(0x2fc));
                draw_0(0x4001a);
                driving::pc_matrix_pop(s);
            }
        }
    }
    if(alt){record(0x4044f0,{0,0,0,8,0x10000000});record(0x4044f0,{1,0,0,8,0});record(0x405350,{});
        record(0x4044f0,{0,1,0,8,7});record(0x4044f0,{1,1,0,8,7});}
    record(0x4052c0,{});if(alt)record(0x404540,{});
    driving::pc_matrix_pop(s);
    if(vm!=1)return;                                                              // bridge 469CDB
    if(layout.u32(0xc)!=None){
        // Flat shadow: 49A650 is a bare RET; the matrix work and the 449640
        // row normalisation of +B0 still happen.
        driving::pc_matrix_push_unit(s);
        if(car.u32(4)&0x800000u){
            translate(get3(car,0x14));
            driving::pc_matrix_rotate_y(s,word_angle(car.i16(0x2e)));
            translate({car.f32(0x2d8),0.f,car.f32(0x2e0)});
            if(in.scene_82e7d4==6||in.scene_82e7d4==15){
                const auto angles=matrix_angles_449640(car.sub(0xb0,0x30));
                driving::pc_matrix_rotate_y(s,angles[1]);
            }else driving::pc_matrix_rotate_y(s,car.f32(0x2e8));
        }else{
            translate(get3(car,0xe0));
            driving::pc_matrix_rotate_y(s,word_angle(car.i16(0x2e)));
            if(car.f32(0x2c8)>0.f)driving::pc_matrix_rotate_y(s,car.f32(0x2e8));
        }
        driving::pc_matrix_pop(s);
    }
    record(0x4052b0,{});
    for(unsigned k=0;k<4;++k){
        const auto w=layout.sub(0x8c+k*0x20,0x20);
        driving::pc_matrix_push(s);translate(get3(car,0x130+k*0xc));
        if(nonzero(w.f32(0x18)))driving::pc_matrix_rotate_z(s,w.f32(0x18));
        if(nonzero(steer[k]))driving::pc_matrix_rotate_y(s,steer[k]);
        if(w.u32(0)!=None)draw_0(w.u32(0));
        if(nonzero(spin[k]))driving::pc_matrix_rotate_x(s,spin[k]);
        draw_0(layout.u32(0x88+k*0x20));
        if(w.u32(4)!=None)draw_0((c38>200&&age!=0)?w.u32(8):w.u32(4));
        driving::pc_matrix_pop(s);
    }
    record(0x4052c0,{});
}
}

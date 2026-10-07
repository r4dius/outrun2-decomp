#include "driving/pc_car_services_ghost.hpp"
#include "driving/pc_d3dx.hpp"
#include "driving/pc_x87.hpp"
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
namespace outrun::driving {
namespace {
using X=X87;
constexpr float AngleUnit=9.58738019107841e-05f;   // 628254: 2pi/65536
constexpr float AngleScale=10430.3779296875f;        // 6282C0: 65536/2pi
constexpr std::uint32_t RecordStride=0xfd4;
// CVTTSS2SI: out-of-range and NaN give the integer indefinite value.
std::int32_t cvtt(float v){
    if(!(v>=-2147483648.0f&&v<2147483648.0f))return std::numeric_limits<std::int32_t>::min();
    return static_cast<std::int32_t>(v);
}
float quiet(float v){std::uint32_t u;std::memcpy(&u,&v,4);u|=0x400000u;std::memcpy(&v,&u,4);return v;}
// SUBSS/ADDSS a,b: a NaN first operand wins, then b (both quietened).
float sse_sub(float a,float b){if(std::isnan(a))return quiet(a);if(std::isnan(b))return quiet(b);return a-b;}
float sse_add(float a,float b){if(std::isnan(a))return quiet(a);if(std::isnan(b))return quiet(b);return a+b;}
float word_angle(std::int16_t w){return static_cast<float>(static_cast<X>(w)*AngleUnit);} // 4493A0 fild * 628254
std::int32_t sar(std::uint32_t v,unsigned n){std::int32_t s;std::memcpy(&s,&v,4);return s>>n;}
// 40A7D0: D3DXVec3TransformCoord(out, in, current). The generic D3DX path
// divides by w even when w is 0 or not finite (singular area matrices leave
// the un-inverted current matrix in place), so use the unrestricted port.
CourseProbe current_point(const PcMatrixStack& s,float x,float y,float z){
    const Bytes c=s.current();PcMatrix16 m{};for(unsigned k=0;k<16;++k)m[k]=c.f32(k*4);
    const auto r=pc_d3dx_vec3_transform_coord({x,y,z},m);return {r[0],r[1],r[2]};
}
// Frame flag bit 0 test shared by 466A20/4667D0 (words/bytes as stored).
bool packet_nonzero(Bytes p,std::size_t w0,std::size_t w1,std::size_t b0,std::size_t b1,std::uint8_t cl){
    return p.i16(w1)!=0||p.i16(w0)!=0||p.u8(b0)!=0||p.u8(b1)!=0||cl!=0;
}
}

std::array<float,3> pc_matrix_angles_449640(Bytes m){
    m.check(0,0x30);
    for(unsigned row=0;row<3;++row){
        CourseProbe v{m.f32(row*0x10),m.f32(row*0x10+4),m.f32(row*0x10+8)};pc_unit_vector_40eeb0(v);
        m.putf(row*0x10,v.x);m.putf(row*0x10+4,v.y);m.putf(row*0x10+8,v.z);
    }
    std::array<float,3> out{};
    const float m0=m.f32(0),m1=m.f32(4),m2=m.f32(8),m4=m.f32(0x10),m5=m.f32(0x14),m6=m.f32(0x18),
                m8=m.f32(0x20),m9=m.f32(0x24),m10=m.f32(0x28);
    if(std::fabs(m5)>9.99999975e-05f){
        const float t=float(x87_atan2(m1,m5));out[2]=t;
        const float u=float(X(m5)/x87_cos(t));
        const float a=float(x87_atan2(0.f-m9,u));out[0]=a;
        out[1]=std::fabs(a)>1.57079637f?float(x87_atan2(0.f-m8,0.f-m10)):float(x87_atan2(m8,m10));
    }else if(std::fabs(m1)>9.99999975e-05f){
        out[2]=1.57079637f;
        out[0]=float(x87_atan2(0.f-m9,m1));
        out[1]=float(x87_atan2(m6,0.f-m4));
    }else{
        out[2]=0.f;
        out[0]=m9>0.f?-1.57079637f:1.57079637f;
        out[1]=float(x87_atan2(0.f-m2,m0));
    }
    return out;
}

std::int32_t pc_ghost_slot_limit_450780(std::int32_t mode){return (mode==2||mode==3)?14:4;}

void pc_ghost_record_reset_47ed90(PcGhostRecorderGlobals& g,Bytes records,const PcGhostResetInputs& in){
    Bytes r=g.recorder_81010c;r.check(0,0x18);
    g.player_model_810445=in.event_11;
    r.put32(0x10,0xc7c34fffu);r.put32(0x0c,0xc7c34fffu);r.put32(0x08,0xc7c34fffu); // 5B4488 = -99999.99
    r.put8(0x15,0);r.put8(0x17,0);r.put32(0x04,0);r.put32(0x00,1);
    const std::int32_t limit=pc_ghost_slot_limit_450780(in.game_mode_78024c);
    for(std::int32_t n=0;n<=limit;++n){
        const std::size_t o=std::size_t(n)*RecordStride;
        records.put8(o+0x30,0);records.put8(o+0x28,in.event_11);records.put16(o+0x24,0x132);
        records.put8(o+0x33,0x10);records.put8(o+0x31,in.event_12);records.put8(o+0x2a,in.event_13);
        records.put8(o+0x29,in.byte_83036d);
    }
    records.put8(0x2f,0);g.frame_81043c=0;
}

void pc_ghost_record_write_47f330(Bytes rs,Bytes rec,Bytes car,const PcGhostRecordWriteInputs& in,PcMatrixStack& s){
    rs.check(0,0x18);
    if(rs.i32(4)>=0xf60){rec.put32(0,0);rs.put32(0,0);return;}
    if(in.divisor==0)throw std::domain_error("47F330 DIV by zero (#DE on the PC)");
    if(in.frame_81043c%in.divisor)return;
    pc_matrix_push_load(s,car.u32(0x5c)?in.area_matrix_7d3190:in.area_matrix_7d2da0); // 44BED0, 409F90
    pc_d3dx_matrix_inverse(s.current(),nullptr,s.current());                            // 40A240
    const CourseProbe p=current_point(s,car.f32(0x14),car.f32(0x18),car.f32(0x1c)); // 40A7D0
    pc_matrix_rotate_y(s,word_angle(car.i16(0x2e)));
    pc_matrix_rotate_x(s,word_angle(car.i16(0x2c)));
    pc_matrix_rotate_z(s,word_angle(car.i16(0x30)));
    std::array<std::uint8_t,64> local{};pc_matrix_get(s,Bytes(local.data(),64));          // 449640(out,NULL)
    const auto ang=pc_matrix_angles_449640(Bytes(local.data(),64));
    pc_matrix_pop(s);
    const float q[3]{float(cvtt(p.x*100.f))*0.00999999978f,float(cvtt(p.y*100.f))*0.00999999978f,
                     float(cvtt(p.z*100.f))*0.00999999978f};
    const std::uint8_t bx=std::uint8_t(sar(std::uint32_t(cvtt(ang[0]*AngleScale)),8));
    const std::uint8_t by=std::uint8_t(sar(std::uint32_t(cvtt(ang[1]*AngleScale)),8));
    const std::uint8_t bz=std::uint8_t(sar(std::uint32_t(cvtt(ang[2]*AngleScale)),8));
    const std::int32_t count=rs.i32(4);
    std::uint8_t dl=0;
    if(count>0){
        if(car.u8(0x33)==rs.u8(0x17))dl=2;
        if(bx==rs.u8(0x14))dl|=4;
        if(by==rs.u8(0x15))dl|=8;
        if(bz==rs.u8(0x16))dl|=0x10;
    }
    if(car.f32(0x2c8)>0.f&&car.i32(4)<0)dl|=1;
    if(car.u32(0x5c)==0)dl|=0x20;
    if(car.i32(0x38)>0)dl|=0x40;
    // EDI = slot+0x34; a negative count indexes back into the slot header.
    Bytes buf=rec;
    std::int32_t n=count;
    auto at=[&](std::int32_t k){const std::int64_t o=std::int64_t(k)+0x34;if(o<0)throw std::out_of_range("47F330 record offset before slot view");return std::size_t(o);};
    static constexpr float Scale[3]{4.09600019f,40.9599991f,4.09600019f}; // 5B4374, 5B448C, 5B4374
    for(unsigned c=0;c<3;++c){
        const float old=rs.f32(0x08+c*4);
        if(q[c]==old&&!std::isnan(q[c])&&!std::isnan(old)){buf.put8(at(n),0x80);n+=1;}
        else{const std::int32_t v=cvtt(q[c]*Scale[c]);buf.put8(at(n),std::uint8_t(v>>8));buf.put8(at(n+1),std::uint8_t(v));n+=2;}
        rs.puti(4,n);
    }
    for(unsigned c=0;c<3;++c)rs.putf(0x08+c*4,q[c]);
    buf.put8(at(n),dl);n+=1;rs.puti(4,n);
    if(!(dl&4)){buf.put8(at(n),bx);n+=1;rs.puti(4,n);}
    if(!(dl&8)){buf.put8(at(n),by);n+=1;rs.puti(4,n);}
    if(!(dl&0x10)){buf.put8(at(n),bz);n+=1;rs.puti(4,n);}
    rs.put8(0x15,by);rs.put8(0x14,bx);rs.put8(0x16,bz);
    if(!(dl&2)){buf.put8(at(n),car.u8(0x33));n+=1;rs.puti(4,n);}
    if(!(dl&1)){car.putf(0x2c8,0.f);return;}
    const std::uint32_t bits=car.u32(0x2c8);
    buf.put8(at(n),std::uint8_t(bits>>24));buf.put8(at(n+1),std::uint8_t(bits>>16));
    buf.put8(at(n+2),std::uint8_t(bits>>8));buf.put8(at(n+3),std::uint8_t(bits));
    n+=4;rs.puti(4,n);
    for(unsigned c=0;c<3;++c){
        const std::int32_t v=cvtt(car.f32(0x2d8+c*4)*4096.f);
        buf.put8(at(n),std::uint8_t(v>>8));buf.put8(at(n+1),std::uint8_t(v));n+=2;rs.puti(4,n);
    }
    for(unsigned c=0;c<3;++c){
        buf.put8(at(n),std::uint8_t(sar(std::uint32_t(cvtt(car.f32(0x2e4+c*4)*AngleScale)),8)));n+=1;rs.puti(4,n);
    }
}

void pc_ghost_key_encode_466a20(Bytes p,Bytes f){
    p.check(0,0x1c);f.check(0,0x30);
    std::uint32_t esi=p.u32(0);
    esi^=((std::uint32_t(std::uint16_t(f.i16(0x12)))<<1)^esi)&0x7ffeu;p.put32(0,esi);
    p.put32(4,f.u32(0));p.put32(8,f.u32(4));p.put32(0xc,f.u32(8));
    p.put8(0x10,f.u8(0xd));p.put8(0x11,f.u8(0xf));p.put8(0x12,f.u8(0x11));
    esi^=((std::uint32_t(std::uint16_t(f.i16(0x2c)))<<7)^esi)&0x7f8000u;p.put32(0,esi);
    esi^=((std::uint32_t(std::uint16_t(f.i16(0x2e)))<<15)^esi)&0x7f800000u;p.put32(0,esi);
    p.put16(0x14,std::uint16_t(cvtt(f.f32(0x14)*4096.f)));
    p.put16(0x16,std::uint16_t(cvtt(f.f32(0x1c)*4096.f)));
    p.put8(0x18,f.u8(0x25));p.put8(0x19,f.u8(0x27));
    const std::uint8_t cl=f.u8(0x29);p.put8(0x1a,cl);
    p.put32(0,packet_nonzero(p,0x14,0x16,0x18,0x19,cl)?(p.u32(0)|1u):(p.u32(0)&~1u));
}

void pc_ghost_key_decode_466af0(Bytes f,Bytes p){
    p.check(0,0x1c);f.check(0,0x30);
    // Protected bridge 0x10398E8 between 466AF4 and 466AFA, measured as a black
    // box (car_services_ghost_probe): DX = ([packet] >> 1) & 0x3FFF.
    const std::uint32_t edx=(p.u32(0)>>1)&0x3fffu;
    f.put16(0x12,std::uint16_t(edx));
    f.put32(0,p.u32(4));f.put32(4,p.u32(8));f.put32(8,p.u32(0xc));
    f.put16(0xc,std::uint16_t(p.u8(0x10)<<8));f.put16(0xe,std::uint16_t(p.u8(0x11)<<8));f.put16(0x10,std::uint16_t(p.u8(0x12)<<8));
    f.put16(0x2c,std::uint16_t(std::uint32_t(sar(p.u32(0)<<9,16))&0xffffff00u));
    f.put16(0x2e,std::uint16_t(std::uint32_t(sar(p.u32(0)<<1,16))&0xffffff00u));
    if(p.u8(0)&1){
        f.putf(0x14,float(p.i16(0x14))*0.000244140625f);
        f.putf(0x1c,float(p.i16(0x16))*0.000244140625f);
        f.put16(0x24,std::uint16_t(p.u8(0x18)<<8));f.put16(0x26,std::uint16_t(p.u8(0x19)<<8));
        const std::uint16_t dx=std::uint16_t(p.u8(0x1a)<<8);
        f.put16(0x2a,std::uint16_t(f.i16(0x2e)));f.putf(0x18,0.f);f.putf(0x20,0.f);f.put16(0x28,dx);
    }else{
        f.put16(0x24,0);f.put16(0x26,0);f.put16(0x28,0);
        const std::uint16_t cx=std::uint16_t(f.i16(0x2e));
        f.putf(0x14,0.f);f.putf(0x1c,0.f);f.put16(0x2a,cx);f.putf(0x18,0.f);f.putf(0x20,0.f);
    }
}

void pc_ghost_frame_delta_466030(Bytes d,Bytes f,Bytes prev){
    d.check(0,0x30);f.check(0,0x30);prev.check(0,0x30);
    d.put16(0x12,std::uint16_t(f.i16(0x12)-prev.i16(0x12)));
    d.put16(0x2c,std::uint16_t(f.i16(0x2c)-prev.i16(0x2c)));
    d.put16(0x2e,std::uint16_t(f.i16(0x2e)-prev.i16(0x2e)));
    auto clamp=[&](std::size_t o,float hi,float lo){
        const float v=sse_sub(f.f32(o),prev.f32(o));d.putf(o,v);
        if(v>hi)d.putf(o,hi);else if(lo>v)d.putf(o,lo);
    };
    clamp(0,63.f,-63.f);clamp(4,31.f,-31.f);clamp(8,63.f,-63.f);
    d.put16(0xc,std::uint16_t(f.i16(0xc)));d.put16(0xe,std::uint16_t(f.i16(0xe)));d.put16(0x10,std::uint16_t(f.i16(0x10)));
    d.put32(0x14,f.u32(0x14));d.put32(0x1c,f.u32(0x1c));
    d.put16(0x24,std::uint16_t(f.i16(0x24)));d.put16(0x26,std::uint16_t(f.i16(0x26)));d.put16(0x28,std::uint16_t(f.i16(0x28)));
}

void pc_ghost_delta_encode_4667d0(Bytes p,Bytes d){
    p.check(0,0x10);d.check(0,0x30);
    const std::int16_t dx=d.i16(0x12);
    if(dx<0)p.put32(0,p.u32(0)|0xeu);
    else if(dx>6)p.put32(0,(p.u32(0)&~2u)|0xcu);
    else{std::uint32_t esi=p.u32(0);esi^=((std::uint32_t(std::uint16_t(dx))<<1)^esi)&0xeu;p.put32(0,esi);}
    {std::uint32_t esi=p.u32(4);esi^=((std::uint32_t(cvtt(d.f32(0)*32.f))<<12)^esi)&0xfff000u;p.put32(4,esi);}
    {std::uint32_t esi=p.u32(0);esi^=((std::uint32_t(cvtt(d.f32(4)*8.f))<<7)^esi)&0xff80u;p.put32(0,esi);}
    {std::uint32_t esi=p.u32(4);esi^=(std::uint32_t(cvtt(d.f32(8)*32.f))^esi)&0xfffu;p.put32(4,esi);}
    p.put8(7,d.u8(0xd));p.put8(2,d.u8(0xf));p.put8(3,d.u8(0x11));
    {std::uint32_t esi=p.u32(0);esi^=(std::uint32_t(d.u8(0x2d))^esi)&0x70u;p.put32(0,esi);}
    p.put16(8,std::uint16_t(cvtt(d.f32(0x14)*4096.f)));
    p.put16(0xa,std::uint16_t(cvtt(d.f32(0x1c)*4096.f)));
    p.put8(0xc,d.u8(0x25));p.put8(0xd,d.u8(0x27));
    const std::uint8_t cl=d.u8(0x29);p.put8(0xe,cl);
    p.put32(0,packet_nonzero(p,0x8,0xa,0xc,0xd,cl)?(p.u32(0)|1u):(p.u32(0)&~1u));
}

void pc_ghost_delta_decode_4668f0(Bytes d,Bytes p){
    p.check(0,0x10);d.check(0,0x30);
    const std::uint32_t k=(p.u32(0)>>1)&7u;
    d.put16(0x12,k==7?0xffffu:std::uint16_t(k));
    d.putf(0,float(sar(p.u32(4)<<8,20))*0.03125f);
    d.putf(4,float(sar(p.u32(0)<<16,23))*0.125f);
    d.putf(8,float(sar(p.u32(4)<<20,20))*0.03125f);
    d.put16(0xc,std::uint16_t(std::uint16_t(p.i16(6))&0xff00u));
    d.put16(0xe,std::uint16_t(p.u8(2)<<8));
    d.put16(0x10,std::uint16_t(std::uint16_t(p.i16(2))&0xff00u));
    d.put16(0x2c,std::uint16_t(std::uint32_t(sar(p.u32(0)<<25,17))&0xfffff000u));
    d.put16(0x2e,0);
    if(p.u8(0)&1){
        d.putf(0x14,float(p.i16(8))*0.000244140625f);
        d.putf(0x1c,float(p.i16(0xa))*0.000244140625f);
        d.put16(0x24,std::uint16_t(p.u8(0xc)<<8));d.put16(0x26,std::uint16_t(p.u8(0xd)<<8));
        const std::uint16_t dx=std::uint16_t(p.u8(0xe)<<8);
        d.put16(0x2a,0);d.putf(0x18,0.f);d.putf(0x20,0.f);d.put16(0x28,dx);
    }else{
        d.put16(0x24,0);d.put16(0x26,0);d.put16(0x28,0);d.put16(0x2a,0);
        d.putf(0x14,0.f);d.putf(0x1c,0.f);d.putf(0x18,0.f);d.putf(0x20,0.f);
    }
}

void pc_ghost_frame_add_466720(Bytes f,Bytes d,Bytes prev){
    f.check(0,0x30);d.check(0,0x30);prev.check(0,0x30);
    f.put16(0x12,std::uint16_t(prev.i16(0x12)+d.i16(0x12)));
    f.putf(0,sse_add(prev.f32(0),d.f32(0)));f.putf(4,sse_add(prev.f32(4),d.f32(4)));f.putf(8,sse_add(prev.f32(8),d.f32(8)));
    f.put16(0xc,std::uint16_t(d.i16(0xc)));f.put16(0xe,std::uint16_t(d.i16(0xe)));f.put16(0x10,std::uint16_t(d.i16(0x10)));
    f.put16(0x2c,std::uint16_t(prev.i16(0x2c)+d.i16(0x2c)));
    f.put16(0x2e,std::uint16_t(prev.i16(0x2e)+d.i16(0x2e)));
    f.put32(0x14,d.u32(0x14));f.put32(0x1c,d.u32(0x1c));
    f.put16(0x24,std::uint16_t(d.i16(0x24)));f.put16(0x26,std::uint16_t(d.i16(0x26)));
    const std::uint16_t cx=std::uint16_t(d.i16(0x28)),dx=std::uint16_t(f.i16(0x2e));
    f.put16(0x28,cx);f.put16(0x2a,dx);f.putf(0x18,0.f);f.putf(0x20,0.f);
}

void pc_ghost_packet_write_466e50(PcGhostPacketState& st,PcGhostPacketBuffer buf,Bytes car,
    const PcGhostPacketInputs& in,PcMatrixStack& s){
    if(st.count_7f8ef4>=0xf61u||std::uint32_t(st.cursor_7f91f0-st.base_7f8d84+0x1cu)>0x938cu){st.overflow_7f8ef8=1;return;}
    std::array<std::uint8_t,0x30> fr{};Bytes F(fr.data(),fr.size());
    F.put32(0,car.u32(0x14));F.put32(4,car.u32(0x18));F.put32(8,car.u32(0x1c));
    F.put16(0xc,std::uint16_t(car.i16(0x2c)));F.put16(0xe,std::uint16_t(car.i16(0x2e)));
    F.put16(0x10,std::uint16_t(car.i16(0x30)));F.put16(0x12,std::uint16_t(car.i16(0x260)));
    F.put32(0x14,car.u32(0x2c8));
    F.put32(0x18,car.u32(0x2d8));F.put32(0x1c,car.u32(0x2dc));F.put32(0x20,car.u32(0x2e0));
    F.put16(0x24,std::uint16_t(cvtt(car.f32(0x2e4)*AngleScale)));
    F.put16(0x26,std::uint16_t(cvtt(car.f32(0x2e8)*AngleScale)));
    F.put16(0x28,std::uint16_t(cvtt(car.f32(0x2ec)*AngleScale)));
    F.put16(0x2a,std::uint16_t(car.i16(0x40)));F.put16(0x2c,std::uint16_t(car.i16(0x32)));F.put16(0x2e,std::uint16_t(car.i16(0x44)));
    if(in.frame_counter_656234<0x3c){ // 48B350
        if(in.player_5c==0)for(unsigned k=0;k<64;k+=4)st.matrix_7f9300.put32(k,in.area_matrix_7d2da0.u32(k)); // 44BEA0
        pc_matrix_push(s);pc_matrix_unit_rotation(s);
        pc_matrix_rotate_y(s,word_angle(F.i16(0xe)));
        pc_matrix_rotate_x(s,word_angle(F.i16(0xc)));
        pc_matrix_rotate_z(s,word_angle(F.i16(0x10)));
        std::array<std::uint8_t,64> m1{};pc_matrix_get(s,Bytes(m1.data(),64));pc_matrix_pop(s);
        pc_matrix_push_load(s,st.matrix_7f9300);
        pc_d3dx_matrix_inverse(s.current(),nullptr,s.current());
        const CourseProbe p=current_point(s,F.f32(0),F.f32(4),F.f32(8)); // 40A7D0
        pc_matrix_multiply_current(s,Bytes(m1.data(),64));
        std::array<std::uint8_t,64> m2{};pc_matrix_get(s,Bytes(m2.data(),64));
        const auto a=pc_matrix_angles_449640(Bytes(m2.data(),64));
        F.put16(0xc,std::uint16_t(cvtt(a[0]*AngleScale)));
        F.put16(0xe,std::uint16_t(cvtt(a[1]*AngleScale)));
        F.put16(0x10,std::uint16_t(cvtt(a[2]*AngleScale)));
        pc_matrix_pop(s);
        F.putf(0,p.x);F.putf(4,p.y);F.putf(8,p.z);
    }
    const std::uint32_t cursor=st.cursor_7f91f0;
    if(cursor<buf.guest_base)throw std::out_of_range("466E50 cursor before explicit packet buffer");
    const std::size_t off=cursor-buf.guest_base;
    if(st.count_7f8ef4==0){
        Bytes P=buf.bytes.sub(off,0x1c);
        pc_ghost_key_encode_466a20(P,F);pc_ghost_key_decode_466af0(F,P);
    }else{
        Bytes P=buf.bytes.sub(off,0x10);
        std::array<std::uint8_t,0x30> dd{};Bytes D(dd.data(),dd.size());
        pc_ghost_frame_delta_466030(D,F,st.prev_frame_7f92c8);
        pc_ghost_delta_encode_4667d0(P,D);pc_ghost_delta_decode_4668f0(D,P);
        pc_ghost_frame_add_466720(F,D,st.prev_frame_7f92c8);
    }
    for(unsigned k=0;k<0x30;k+=4)st.prev_frame_7f92c8.put32(k,F.u32(k));
    const bool full=buf.bytes.u8(off)&1u;
    st.cursor_7f91f0=cursor+(full?(st.count_7f8ef4?0x10u:0x1cu):(st.count_7f8ef4?0x8u:0x14u));
    st.count_7f8ef4+=1;
}
}

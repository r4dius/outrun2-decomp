#include "driving/pc_race_camera.hpp"
#include <array>
#include "driving/pc_x87.hpp"
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
namespace outrun::driving {
namespace {
using X=X87;
constexpr float AngleUnit=9.58738019107841e-05f; // 628254
CourseProbe get3(Bytes b,std::size_t o){return {b.f32(o),b.f32(o+4),b.f32(o+8)};}
void put3(Bytes b,std::size_t o,const CourseProbe& v){b.putf(o,v.x);b.putf(o+4,v.y);b.putf(o+8,v.z);}
void copy_words(Bytes to,std::size_t t,Bytes from,std::size_t f,unsigned n){for(unsigned k=0;k<n;++k)to.put32(t+k*4,from.u32(f+k*4));}
float word_angle(std::int32_t w){return float(X(w)*AngleUnit);}          // 4493A0 * 628254

// 582194 (_ftol2): truncation toward zero of ST0; the x87 indefinite value
// for NaN/overflow.
std::int64_t ftol2(X v){
    if(!(v.v>=-9223372036854775808.0L&&v.v<9223372036854775808.0L))return std::numeric_limits<std::int64_t>::min();
    return std::int64_t(v.v);
}
// 4494D0: angle blend (1-t)*a' + b*t with a' unwrapped towards b, wrapped to [-pi,pi].
float angle_blend_4494d0(float a,float b,float t){
    constexpr float pi=3.14159274101257324f,two_pi=6.28318548202514648f,minus_pi=-3.14159274101257324f;
    float x1=a;const float d=a-b;
    if(d>pi)x1=x1-two_pi;else if(minus_pi>d)x1=x1+two_pi;
    float r=(1.f-t)*x1;r=r+b*t;
    if(r>pi){do r-=two_pi;while(r>pi);}
    if(minus_pi>r){do r+=two_pi;while(minus_pi>r);}
    return r;
}
// 5E08EC: view-change time table (4F6580, i8 index * 8).
constexpr std::uint32_t ViewTimes5e08ec[32]{0x3f800000u,0x40200000u,0x40000000u,0x3f800000u,0x3fc00000u,0x40000000u,0x40200000u,0x3fc00000u,
    0x40000000u,0x40000000u,0x40200000u,0x40400000u,0x3fc00000u,0x40000000u,0x00000000u,0x40200000u,0x40400000u,0x40400000u,0x40400000u,
    0x40400000u,0x3b973320u,0x3dcccccdu,0x0000007fu,0x3b973320u,0x00000000u,0x00000002u,0x3b973320u,0x3dcccccdu,0x00000003u,0x3e3cffe8u,
    0x00000000u,0x00000001u};
float view_time_4f6580(std::uint8_t b){
    const std::int8_t i=std::int8_t(b);
    if(i<0||i>=32)throw std::out_of_range("4F6580 index outside the 5E08EC table");
    float f;std::uint32_t w=ViewTimes5e08ec[i];std::memcpy(&f,&w,4);return f;
}
void rotate_word_y(PcMatrixStack& s,Bytes car,std::size_t o){pc_matrix_rotate_y(s,word_angle(car.i16(o)));}
void rotate_word_x(PcMatrixStack& s,Bytes car,std::size_t o){pc_matrix_rotate_x(s,word_angle(car.i16(o)));}
void rotate_word_z(PcMatrixStack& s,Bytes car,std::size_t o){pc_matrix_rotate_z(s,word_angle(car.i16(o)));}
// 580F40: CRT rand().
std::int32_t crt_rand(const PcRaceCameraInputs& in){
    if(!in.crt_random_580f40)throw std::logic_error("camera: rand() without the CRT state");
    auto& r=*in.crt_random_580f40;r=r*214013u+2531011u;return std::int32_t((r>>16)&0x7fffu);
}
// rand() % n - n / 2 (signed, as IDIV and SAR do).
std::int32_t centred_rand(const PcRaceCameraInputs& in,std::int32_t n){
    if(n==0)throw std::domain_error("camera shake: division by a zero frame count");
    return crt_rand(in)%n-n/2;
}
}
float race_camera_yaw_483110(Bytes cam,Bytes car){
    std::uint16_t di=std::uint16_t(car.i16(0x1fe));
    const std::int8_t view=cam.i8(0x34a);
    if(view==0||view==1){
        const float a=word_angle(car.i16(0x162));
        const float s=float(x87_sin(a));
        const X f=x87_abs(X(s));
        const std::int32_t d=std::int16_t(std::uint16_t(car.i16(0x2e))-di);
        const X k=view==0?X(-0.600000024f):X(-0.800000012f);                 // 62811C / 5BA954
        di=std::uint16_t(di-std::uint16_t(std::uint32_t(ftol2((f*X(d))*k))));
    }
    const std::int16_t n=cam.i16(0x346);
    if(n>0){
        const float r=float(1.0L-X(std::int32_t(n))/X(std::int32_t(cam.i16(0x348))));
        return angle_blend_4494d0(word_angle(car.i16(0xd8c)),word_angle(car.i16(0x2e)),r);
    }
    const float speed=car.f32(0x1c4);
    if(speed>0.00922849774f)return word_angle(std::int16_t(di));                  // 5BA950
    if(speed>0.00461424887f){                                                      // 5B43A4
        float r=1.f-(speed-0.00461424887f)*216.720001f;
        if(0.f>r)r=0.f;else if(r>1.f)r=1.f;
        return angle_blend_4494d0(word_angle(std::int16_t(di)),word_angle(car.i16(0x2e)),r);
    }
    return word_angle(car.i16(0x2e));
}
void race_camera_car_matrices_484810(Bytes cam,Bytes car,PcMatrixStack& s,Bytes table){
    const std::int32_t view=cam.i8(0x34a);
    const Bytes t=table.sub(std::size_t(std::int64_t(view)*0x34+0),0x34);            // bridge 484818
    pc_matrix_push(s);pc_matrix_identity(s);
    rotate_word_y(s,car,0x2e);rotate_word_x(s,car,0x2c);
    CourseProbe v{0.f,0.f,t.f32(0x2c)};v=pc_matrix_point(s,v);
    pc_matrix_identity(s);pc_matrix_translate_vector(s,get3(car,0x14));
    rotate_word_y(s,car,0x2e);rotate_word_x(s,car,0x2c);
    if(view<=0x15)rotate_word_z(s,car,0x30);
    pc_matrix_get(s,cam.sub(0x280,64));
    pc_matrix_identity(s);pc_matrix_translate_vector(s,v);pc_matrix_translate_vector(s,get3(car,0x14));
    pc_matrix_rotate_y(s,race_camera_yaw_483110(cam,car));rotate_word_x(s,car,0x2c);
    if(view<=0x15)rotate_word_z(s,car,0x30);
    if(car.f32(0x2c8)>0.f){
        pc_matrix_translate_vector(s,get3(car,0x2d8));
        pc_matrix_rotate_z(s,car.f32(0x2ec));pc_matrix_rotate_y(s,car.f32(0x2e8));pc_matrix_rotate_x(s,car.f32(0x2e4));
    }
    pc_matrix_get(s,cam.sub(0x200,64));
    pc_matrix_identity(s);pc_matrix_translate_vector(s,v);pc_matrix_translate_vector(s,get3(car,0x14));
    pc_matrix_rotate_y(s,race_camera_yaw_483110(cam,car));rotate_word_x(s,car,0x2c);rotate_word_z(s,car,0x200);
    pc_matrix_get(s,cam.sub(0x240,64));
    pc_matrix_pop(s);
}
void race_camera_angles_484df0(Bytes c){
    const float ex=c.f32(0xf8),ey=c.f32(0xfc),ez=c.f32(0x100),tx=c.f32(0x104),ty=c.f32(0x108),tz=c.f32(0x10c);
    // 40F140(target xz, eye xz): x87 distance with y = 0.
    const X dx=X(tx)-ex,dy=X(0.f)-0.f,dz=X(tz)-ez;
    const float d=float(x87_sqrt((dz*dz+dx*dx)+dy*dy));
    c.putf(0x128,float(x87_atan2(X(ty-ey),X(d))));
    c.putf(0x12c,float(x87_atan2(X(ex-tx),X(ez-tz))));
}
void race_camera_save_4833e0(Bytes c,std::uint16_t cx){
    c.put8(0x34b,c.u8(0x34a));
    copy_words(c,0x110,c,0xe0,3);
    const std::int16_t before=c.i16(0x33c);
    copy_words(c,0x11c,c,0xec,3);
    c.put32(0x134,c.u32(0x130));c.put32(0x350,c.u32(0x34c));c.put32(0xa8,c.u32(0xa4));
    c.put32(0xcc,c.u32(0xc4));c.put32(0xd0,c.u32(0xc8));c.put32(0x35c,c.u32(0x358));
    c.put16(0x33c,cx);
    if(before>0)c.put16(0x368,std::uint16_t(c.i16(0x368)+1));else c.put16(0x368,0);
}
void race_camera_view_timers_4832f0(Bytes c,Bytes car,std::uint32_t network,std::int32_t variant){
    bool forced=false;
    if(network!=0u&&car.i32(0xd18)>0)forced=true;
    else if((car.u8(0x2f0)&1u)==0u)return;
    if(car.i16(0xd68)>2){
        const std::uint8_t kind=std::uint8_t((car.u32(0x2f0)>>2)&0xffffff1fu);
        const std::int64_t n=ftol2(X(view_time_4f6580(kind))*X(-60.2000008f));   // 5BA958
        const std::uint16_t w=std::uint16_t(1u-std::uint32_t(n));
        c.put16(0x346,w);c.put16(0x348,w);
    }
    std::uint8_t kind;
    if(forced)kind=0;
    else{
        const std::uint32_t k=(car.u32(0x2f0)>>2)&0x1fu;
        if(k!=1u&&k!=2u&&k!=5u)return;
        kind=std::uint8_t(k);
    }
    c.put8(0x360,kind);
    const std::int64_t n=ftol2(X(view_time_4f6580(c.u8(0x360)))*X(-60.2000008f));
    const std::uint16_t w=std::uint16_t(1u-std::uint32_t(n));
    c.put16(0x340,w);c.put16(0x342,w);
    c.put16(0x344,variant!=0?0x5au:0u);
}
void race_camera_view_switch_484a40(Bytes c,std::uint32_t mask,std::uint8_t byte_780270){
    const std::int16_t t340=c.i16(0x340);
    if(t340!=0&&c.u8(0x360)!=0xeu){
        if(t340==c.i16(0x342)){
            const std::uint8_t bl=c.u8(0x34b);
            race_camera_save_4833e0(c,0x1e);
            if(c.u8(0x34a)==0x1du)c.put8(0x34b,bl);
            c.put16(0x340,std::uint16_t(c.i16(0x340)-1));
            c.put8(0x34a,c.u8(0x360)==1u?0x1cu:0x1bu);
        }else if(t340==1){
            const std::uint8_t bl=c.u8(0x34b);
            race_camera_save_4833e0(c,0x1e);
            // 49A650(0x16A,1) / 49A650(0x16B,1): bare RET.
            if(c.i16(0x344)!=0){
                c.put16(0x340,std::uint16_t(c.i16(0x340)-1));c.put8(0x34b,bl);c.put8(0x34a,0x1du);
            }else{c.put8(0x34a,bl);c.put16(0x340,std::uint16_t(c.i16(0x340)-1));}
        }else c.put16(0x340,std::uint16_t(c.i16(0x340)-1));
    }else{
        const std::int16_t t344=c.i16(0x344);
        if(t344!=0){
            if(t344==1){
                const std::uint8_t bl=c.u8(0x34b);
                race_camera_save_4833e0(c,0x1e);
                // 49A650(0x16A,0) / 49A650(0x16B,0): bare RET.
                c.put8(0x34a,bl);
            }
            c.put16(0x344,std::uint16_t(c.i16(0x344)-1));
        }else if((mask&0x40000u)!=0u&&byte_780270==0u){
            race_camera_save_4833e0(c,0x1e);
            const std::int8_t v=c.i8(0x34a);
            c.put8(0x34a,v>0?std::uint8_t(v-1):2u);
        }
    }
    const std::int16_t t346=c.i16(0x346);
    if(t346>0)c.put16(0x346,std::uint16_t(t346-1));
}
namespace {
// 483CE0 (EDI camera, EBX car; entry): steering history +300..+33A and the
// view-specific eye swing.
void eye_swing_483ce0(Bytes c,Bytes car,Bytes e,Bytes live_813750){
    std::int32_t sum=0;
    for(unsigned k=0;k<0x1d;++k){
        const std::size_t to=0x33a-k*2;const std::int16_t w=c.i16(to-2);
        c.put16(to,std::uint16_t(w));sum+=w;
    }
    const std::int16_t newest=car.i16(0x1fc);c.put16(0x300,std::uint16_t(newest));sum+=newest;
    const std::int8_t view=c.i8(0x34a);
    if(view<2||view>0x15)return;
    const std::int32_t index=(view-2)*0x1e + car.i8(0x11);
    const Bytes t=live_813750.sub(std::size_t(index)*0x24,0x24);
    const float avg=float(sum)*0.0333333351f;                                   // 5A91BC
    const std::int32_t w=std::int32_t(avg);                                      // CVTTSS2SI
    float a=float((X(w)*AngleUnit)*X(t.f32(0x18)));
    const float high=t.f32(0x1c)*0.0174532924f;                                  // 6281C4
    if(a>=high)a=high;
    const float low=t.f32(0x20)*0.0174532924f;
    if(low>=a)a=low;
    const float r=float(x87_abs(X(e.f32(0x14)-e.f32(0x8))));                // 449390
    const float ny=float(x87_sin(a)*X(r)+X(e.f32(4)));
    const float nz=float(x87_cos(a)*X(r));
    e.putf(4,ny);e.putf(8,nz);                                                   // +0 is written back unchanged
}
}
namespace {
// +130 view-yaw term shared by 4834A0 / 483600: x87 fild/fmul/fadd chain.
float yaw_term(Bytes car,float base,bool quarter,bool add_2ec){
    std::int16_t w=car.i16(0x200);
    if(quarter){const std::uint16_t cx=std::uint16_t(w);const std::int32_t q=std::int32_t(w)/4;w=std::int16_t(std::uint16_t(cx-std::uint16_t(q)));}
    X v=X(std::int32_t(w))*AngleUnit;
    if(add_2ec)v+=X(car.f32(0x2ec));
    v+=X(base);
    return float(v);
}
// 4834A0 (EDI entry, ESI camera; car): snap to the view entry.
void view_snap_4834a0(Bytes c,Bytes e,Bytes car,PcMatrixStack& s){
    pc_matrix_push(s);
    pc_matrix_load(s,c.sub(e.u32(0x30)!=0u?0x200:0x240,64));
    pc_matrix_get(s,c.sub(0x2c0,64));
    c.put32(0x34c,e.u32(0x30));
    copy_words(c,0xe0,e,0,3);copy_words(c,0xec,e,0xc,3);
    put3(c,0xf8,pc_matrix_point(s,get3(c,0xe0)));
    put3(c,0x104,pc_matrix_point(s,get3(c,0xec)));
    pc_matrix_pop(s);
    c.put32(0x130,e.u32(0x20));
    const std::int8_t view=c.i8(0x34a);
    if(e.u32(0x30)!=0u){
        if(view>=0&&view<=1)c.putf(0x130,yaw_term(car,c.f32(0x130),true,true));
    }else if((view>=2&&view<=0x15)||view==0x1b||view==0x1c||view==0x1d)
        c.putf(0x130,yaw_term(car,c.f32(0x130),false,false));
    c.put32(0xa4,e.u32(0x24));c.put32(0xac,e.u32(0x24));
    c.put32(0xc4,e.u32(0x18));c.put32(0xc8,e.u32(0x1c));
    c.put32(0x358,e.u32(0x28));c.putf(0x364,0.f);
}
// 483600 (EBX camera; car, entry, t): blend from the previous view.
void view_blend_483600(Bytes c,Bytes car,Bytes e,float t,PcMatrixStack& s){
    const float u=1.f-t;
    std::array<float,16> a{},b{};
    {const Bytes pa=c.sub(c.u32(0x350)!=0u?0x200:0x240,64),pb=c.sub(e.u32(0x30)!=0u?0x200:0x240,64);
     for(unsigned k=0;k<16;++k){a[k]=pa.f32(k*4);b[k]=pb.f32(k*4);}}
    for(unsigned k:{0u,1u,2u,4u,5u,6u,8u,9u,10u}){float v=b[k]-a[k];v=v*u;a[k]=v+a[k];}
    CourseProbe cz=pc_normalize_vector_40ef00({a[8],a[9],a[10]});
    float d=a[1]*cz.y;d=d+a[0]*cz.x;d=d+a[2]*cz.z;
    a[8]=cz.x;a[9]=cz.y;a[10]=cz.z;
    CourseProbe dx{0.f-(d*cz.x-a[0]),0.f-(d*cz.y-a[1]),0.f-(d*cz.z-a[2])};
    dx=pc_normalize_vector_40ef00(dx);
    a[0]=dx.x;a[1]=dx.y;a[2]=dx.z;
    CourseProbe ey{float(X(dx.z)*cz.y-X(cz.z)*dx.y),float(X(cz.z)*dx.x-X(dx.z)*cz.x),float(X(cz.x)*dx.y-X(dx.x)*cz.y)}; // 40EFF0
    ey=pc_normalize_vector_40ef00(ey);
    a[4]=ey.x;a[5]=ey.y;a[6]=ey.z;
    for(unsigned k:{12u,13u,14u}){float v=b[k]*u;v=v+a[k]*t;a[k]=v;}
    std::array<std::uint8_t,64> m{};Bytes mb(m.data(),64);for(unsigned k=0;k<16;++k)mb.putf(k*4,a[k]);
    pc_matrix_push_load(s,mb);
    pc_matrix_get(s,c.sub(0x2c0,64));
    for(unsigned k=0;k<6;++k){float v=c.f32(0x110+k*4)*t;v=v+e.f32(k*4)*u;c.putf(0xe0+k*4,v);}
    put3(c,0xf8,pc_matrix_point(s,get3(c,0xe0)));
    put3(c,0x104,pc_matrix_point(s,get3(c,0xec)));
    pc_matrix_pop(s);
    float r=e.f32(0x20);
    const std::int8_t view=c.i8(0x34a);
    if(e.u32(0x30)!=0u){
        if(view>=0&&view<=1)c.putf(0x130,yaw_term(car,c.f32(0x130),true,true)); // overwritten below, as on PC
    }else if((view>=2&&view<=0x15)||view==0x1b||view==0x1c||view==0x1d)r=yaw_term(car,r,false,false);
    {float v=c.f32(0x134)*t;v=v+r*u;c.putf(0x130,v);}
    {float v=c.f32(0xa8)*t;v=v+e.f32(0x24)*u;c.putf(0xa4,v);c.putf(0xac,v);}
    {float v=c.f32(0xcc)*t;v=v+e.f32(0x18)*u;c.putf(0xc4,v);}
    {float v=c.f32(0xd0)*t;v=v+e.f32(0x1c)*u;c.putf(0xc8,v);}
    {float v=c.f32(0x35c)*t;v=v+e.f32(0x28)*u;c.putf(0x358,v);}
}
}
void race_camera_eye_look_485590(Bytes car,Bytes c,Bytes e,std::int32_t frames,PcMatrixStack& s,const PcRaceCameraInputs& in){
    std::array<std::uint32_t,6> saved{};for(unsigned k=0;k<6;++k)saved[k]=e.u32(k*4);
    const std::int8_t view=c.i8(0x34a);                                          // bridge 4855C8
    const std::int32_t model=car.i8(0x11);
    if(view==1){
        const Bytes m=in.live_818bb0.sub(std::size_t(model)*0x24,0x24);
        copy_words(e,0,m,0,3);copy_words(e,0xc,m,0,3);
        e.putf(0x14,e.f32(0x14)-1.f);
    }else if(view>=2&&view<=0x15){
        const Bytes m=in.live_813750.sub(std::size_t((view-2)*0x1e + model)*0x24,0x24);
        copy_words(e,0,m,0,3);copy_words(e,0xc,m,0xc,3);
    }else if(view==0x1b||view==0x1c)e.putf(0x10,car.f32(0x2dc)+e.f32(0x10));
    e.putf(8,e.f32(8)-e.f32(0x2c));
    eye_swing_483ce0(c,car,e,in.live_813750);
    const std::int16_t n=c.i16(0x33c);
    if(n!=0){
        const float f=frames==0?1.f:float(std::int32_t(n))/float(frames);
        c.putf(0x364,f);
        view_blend_483600(c,car,e,f,s);
        c.put16(0x33c,std::uint16_t(c.i16(0x33c)-1));
    }else view_snap_4834a0(c,e,car,s);
    if(!in.ground_43eb60)throw std::logic_error("485590 needs the 43EB60 course query");
    CourseProbe p=get3(c,0xf8);
    in.ground_43eb60(p);
    const float y=p.y+0.75f;                                                     // 6282A0
    if(y>=c.f32(0xfc))c.putf(0xfc,y);
    for(unsigned k=0;k<6;++k)e.put32(k*4,saved[k]);
}

void race_camera_start_485250(Bytes c,PcMatrixStack& s,PcCameraDevice& d,const PcCameraScreen& screen,
    const PcCameraBlend& blend,const PcRaceCameraInputs& in){
    Bytes car=in.car;
    copy_words(c,0xd4,car,0x14,3);
    race_camera_car_matrices_484810(c,car,s,in.live_818fe8);
    camera_fov_483c10(c,screen);
    c.put32(0x34c,1);
    pc_matrix_push_load(s,c.sub(0x280,64));
    put3(c,0xf8,pc_matrix_point(s,get3(c,0xe0)));
    put3(c,0x104,pc_matrix_point(s,get3(c,0xec)));
    pc_matrix_pop(s);
    race_camera_angles_484df0(c);
    camera_project_484bd0(c,s,d,screen,blend);
    if(in.timer_8367bc==0x12bu)c.put8(0x34a,in.byte_7c24b8);
    else if(in.timer_8367bc==0xb5u)race_camera_save_4833e0(c,0);
}
void race_camera_race_4858d0(Bytes c,PcMatrixStack& s,PcCameraDevice& d,const PcCameraScreen& screen,
    const PcCameraBlend& blend,const PcRaceCameraInputs& in){
    Bytes car=in.car;
    copy_words(c,0xd4,car,0x14,3);
    race_camera_car_matrices_484810(c,car,s,in.live_818fe8);
    if(c.i16(0x340)==0)race_camera_view_timers_4832f0(c,car,in.network_7f9460_60,in.game_variant_780258);
    if(in.steering_82e7ec==0u)race_camera_view_switch_484a40(c,in.feature_mask_7d6778,in.byte_780270);
    const std::int32_t view=c.i8(0x34a);
    race_camera_eye_look_485590(car,c,in.live_818fe8.sub(std::size_t(std::int64_t(view)*0x34),0x34),0x1e,s,in);
    if(in.network_7f9460_60!=0u){                                               // 55A930: LAN session
        if(c.u32(0x354)!=0u){c.put32(0x37c,0);c.put32(0x378,0);}
        else{
            const float push=in.network_shake_800ad0;                           // 46C480
            if(push>0.f){
                // eye += (eye - target) * push * 0.2; eye.y -= min(push * 0.25, 0.3)
                CourseProbe back{c.f32(0xf8)-c.f32(0x104),c.f32(0xfc)-c.f32(0x108),c.f32(0x100)-c.f32(0x10c)};   // 40EFA0
                const float k=push*0.200000003f;                                  // 5B0068
                back={k*back.x,k*back.y,k*back.z};                              // 40F050
                put3(c,0xf8,{c.f32(0xf8)+back.x,back.y+c.f32(0xfc),back.z+c.f32(0x100)});   // 40EF10
                float drop=push*0.25f;                                          // 628088
                if(drop>0.300000012f)drop=0.300000012f;                         // 6280E4
                c.putf(0xfc,c.f32(0xfc)-drop);
            }
            if(c.u32(0x378)!=0u){
                const float sign=(crt_rand(in)&1)?-1.f:1.f;                     // 6280C4 / 62806C
                const std::int32_t n=c.i32(0x37c);
                const float amount=c.f32(0x380);
                c.putf(0x36c,float(centred_rand(in,n))*(amount*0.5f));          // 628064
                const float frames=float(c.i32(0x37c))*0.5f;
                const float r=float(crt_rand(in))*3.05175781e-05f;              // 628154
                c.putf(0x370,(((r*frames)*sign)+frames)*(c.f32(0x380)*2.f));    // 6280B0
                c.putf(0x374,float(centred_rand(in,c.i32(0x37c)))*(c.f32(0x380)*0.5f));
                put3(c,0xf8,{c.f32(0xf8)+c.f32(0x36c),c.f32(0x370)+c.f32(0xfc),c.f32(0x374)+c.f32(0x100)});   // 40EF10
                const std::int32_t left=c.i32(0x37c)-1;
                c.put32(0x37c,std::uint32_t(left));
                if(left<=0){c.put32(0x37c,0);c.put32(0x378,0);}
            }
        }
    }
    race_camera_angles_484df0(c);
    camera_project_484bd0(c,s,d,screen,blend);
}
void race_camera_start_lan_485300(Bytes c,PcMatrixStack& s,PcCameraDevice& d,const PcCameraScreen& screen,
    const PcCameraBlend& blend,const PcRaceCameraInputs& in){
    for(unsigned o:{0xd4u,0xd8u,0xdcu})c.putf(o,0.f);
    pc_matrix_push_unit(s);
    pc_matrix_get(s,c.sub(0x280,64));pc_matrix_get(s,c.sub(0x200,64));pc_matrix_get(s,c.sub(0x240,64));
    pc_matrix_pop(s);
    camera_fov_483c10(c,screen);
    c.put32(0x34c,1);
    pc_matrix_push_load(s,c.sub(0x280,64));
    put3(c,0xf8,pc_matrix_point(s,get3(c,0xe0)));
    put3(c,0x104,pc_matrix_point(s,get3(c,0xec)));
    pc_matrix_pop(s);
    race_camera_angles_484df0(c);
    camera_project_484bd0(c,s,d,screen,blend);
    if(in.timer_8367bc==0x167u)c.put8(0x34a,in.byte_7c24b8);
    else if(in.timer_8367bc==0xb5u)race_camera_save_4833e0(c,0);
}
void race_camera_demo_485b40(Bytes c,PcMatrixStack& s,PcCameraDevice& d,const PcCameraScreen& screen,
    const PcCameraBlend& blend,const PcRaceCameraInputs& in){
    Bytes car=in.car;
    const Bytes rec=in.demo_record_4b6f40;
    if(!rec.size()){
        c.put8(0x34a,2);
        copy_words(c,0xd4,car,0x14,3);
        pc_matrix_push_unit(s);
        rotate_word_y(s,car,0x2e);rotate_word_x(s,car,0x2c);
        const CourseProbe back=pc_matrix_point(s,{0.f,0.f,in.live_818fe8.f32(0x94)});   // 81907C: view 2 +2C
        pc_matrix_identity(s);
        pc_matrix_translate_vector(s,back);pc_matrix_translate_vector(s,get3(car,0x14));
        rotate_word_y(s,car,0x1fe);rotate_word_x(s,car,0x1fc);rotate_word_z(s,car,0x200);
        pc_matrix_get(s,c.sub(0x240,64));pc_matrix_get(s,c.sub(0x200,64));
        pc_matrix_pop(s);
        if(c.i16(0x340)==0)race_camera_view_timers_4832f0(c,car,in.network_7f9460_60,in.game_variant_780258);
        race_camera_eye_look_485590(car,c,in.live_818fe8.sub(0x68,0x34),0x1e,s,in);   // 819050: view 2
        race_camera_angles_484df0(c);
        camera_project_484bd0(c,s,d,screen,blend);
        return;
    }
    rec.check(0,0x44);
    c.putf(0xbc,rec.f32(0x40)>0.f?rec.f32(0x40):0.100000001f);                 // 62813C
    if(rec.u32(4)==1u){
        copy_words(c,0xd4,car,0x14,3);
        race_camera_car_matrices_484810(c,car,s,in.live_818fe8);
        if(c.i16(0x340)==0)race_camera_view_timers_4832f0(c,car,in.network_7f9460_60,in.game_variant_780258);
        race_camera_view_switch_484a40(c,in.feature_mask_7d6778,in.byte_780270);
        const std::int8_t view=std::int8_t(rec.u8(0x30)-1u);
        c.put8(0x34a,std::uint8_t(view));
        race_camera_eye_look_485590(car,c,in.live_818fe8.sub(std::size_t(std::int64_t(view)*0x34),0x34),0x1e,s,in);
    }else if(rec.u32(4)==0u){
        const float near_fov=rec.f32(0x24)*0.0174532924f,far_fov=rec.f32(0x28)*0.0174532924f;   // 6281C4
        const float reach=rec.f32(0x2c),jitter=rec.f32(0x20),half=jitter*0.5f;
        pc_matrix_push_load(s,car.u32(0x5c)?in.area_matrix_7d3190:in.area_matrix_7d2da0);
        const CourseProbe from=pc_matrix_point(s,get3(rec,8));
        pc_matrix_pop(s);
        pc_matrix_push_load(s,car.sub(0xb0,64));
        put3(c,0x104,pc_matrix_point(s,get3(rec,0x14)));
        pc_matrix_pop(s);
        if(jitter>0.f)for(unsigned o:{0x104u,0x108u,0x10cu})
            c.putf(o,((float(crt_rand(in))*3.05175781e-05f)*jitter+c.f32(o))+half);   // 628154
        const float dz=c.f32(0x10c)-from.z,dy=c.f32(0x108)-from.y,dx=c.f32(0x104)-from.x;
        const float squared=(dz*dz+dy*dy)+dx*dx;
        const X length=x87_sqrt(X(squared));                                     // 449380
        float fov=near_fov;
        if(X(reach)>length)fov=(1.f-float(length)/reach)*(far_fov-near_fov)+near_fov;
        c.putf(0xa4,fov);c.putf(0xac,fov);
        put3(c,0xf8,{rec.f32(0x34)*dx+from.x,rec.f32(0x38)*dy+from.y,rec.f32(0x3c)*dz+from.z});
    }
    race_camera_angles_484df0(c);
    camera_project_484bd0(c,s,d,screen,blend);
}
void race_camera_goal_4853d0(Bytes c,PcMatrixStack& s,PcCameraDevice& d,const PcCameraScreen& screen,
    const PcCameraBlend& blend,const PcRaceCameraInputs& in){
    Bytes car=in.car;
    copy_words(c,0xd4,car,0x14,3);
    race_camera_car_matrices_484810(c,car,s,in.live_818fe8);
    camera_fov_483c10(c,screen);
    c.put32(0x34c,1);
    pc_matrix_push_load(s,c.sub(0x280,64));
    put3(c,0xf8,pc_matrix_point(s,get3(c,0xe0)));
    put3(c,0x104,pc_matrix_point(s,get3(c,0xec)));
    pc_matrix_pop(s);
    copy_words(c,0xd4,c,0xf8,3);
    race_camera_angles_484df0(c);
    camera_project_484bd0(c,s,d,screen,blend);
}
// 485830 (modes 20/21/22, Time Over): the race camera on views 1A..1D; any
// other view is saved (4833E0 with 0x1E frames) and view 1A taken (then
// 49A650(0x16A, 1) and 49A650(0x16B, 1), both RET).
void race_camera_timeover_485830(Bytes c,PcMatrixStack& s,PcCameraDevice& d,const PcCameraScreen& screen,
    const PcCameraBlend& blend,const PcRaceCameraInputs& in){
    Bytes car=in.car;                                                        // [799D18]
    copy_words(c,0xd4,car,0x14,3);
    race_camera_car_matrices_484810(c,car,s,in.live_818fe8);
    const std::int32_t v=c.i8(0x34a);
    if(v<0x1a||v>0x1d){race_camera_save_4833e0(c,0x1e);c.put8(0x34a,0x1a);}
    const std::int32_t view=c.i8(0x34a);
    race_camera_eye_look_485590(car,c,in.live_818fe8.sub(std::size_t(std::int64_t(view)*0x34),0x34),0x1e,s,in);
    race_camera_angles_484df0(c);
    camera_project_484bd0(c,s,d,screen,blend);
}
void race_camera_ending_485470(Bytes c,PcMatrixStack& s,PcCameraDevice& d,const PcCameraScreen& screen,const PcCameraBlend& blend){
    copy_words(c,0xf8,c,0xe0,3);copy_words(c,0x104,c,0xec,3);copy_words(c,0xd4,c,0xe0,3);
    c.put32(0x34c,1);
    race_camera_angles_484df0(c);
    camera_project_484bd0(c,s,d,screen,blend);
}
void race_camera_mode16(Bytes c,PcMatrixStack& s,PcCameraDevice& d,const PcCameraScreen& screen,
    const PcCameraBlend& blend,const PcRaceCameraInputs& in){
    if(std::int16_t(in.timer_8367bc)>0xb4){
        if(in.game_variant_780258==3||in.game_variant_780258==4)race_camera_start_lan_485300(c,s,d,screen,blend,in);
        else race_camera_start_485250(c,s,d,screen,blend,in);
    }else race_camera_race_4858d0(c,s,d,screen,blend,in);
}
}

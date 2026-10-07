#include "driving/pc_camera.hpp"
#include "driving/pc_race_camera.hpp"
#include "driving/pc_course_spline.hpp"
#include "driving/pc_d3dx.hpp"
#include "driving/pc_x87.hpp"
#include <cmath>
#include <stdexcept>
namespace outrun::driving {
namespace {
using X=X87;
X x87_atan(X v){return x87_atan2(v,X(1.0f));} // fld1; fpatan
CourseProbe get3(Bytes b,std::size_t o){return {b.f32(o),b.f32(o+4),b.f32(o+8)};}
void put3(Bytes b,std::size_t o,const CourseProbe& v){b.putf(o,v.x);b.putf(o+4,v.y);b.putf(o+8,v.z);}
void copy_words(Bytes b,std::size_t to,std::size_t from,unsigned n){for(unsigned k=0;k<n;++k)b.put32(to+k*4,b.u32(from+k*4));}
float wrap(float v){ // SSE comiss loops; NaN leaves the value unchanged
    constexpr float pi=3.14159274101257324f,two_pi=6.28318548202514648f,minus_pi=-3.14159274101257324f;
    if(v>pi){do v-=two_pi;while(v>pi);}
    if(minus_pi>v){do v+=two_pi;while(minus_pi>v);}
    return v;
}
}
float camera_blend_4493e0(const PcCameraBlend& b){
    if(b.interpolation_override_82e7d8!=0)return 1.f;
    const std::int32_t mode=(b.owner_flags_79fb4e&3u)==2u?b.owner_mode_799ca0_1c:0; // 4B5FD0
    return mode==3?1.f:b.frame_blend_634b34;
}
std::array<float,4> pc_d3dx_plane_from_points(const CourseProbe& p1,const CourseProbe& p2,const CourseProbe& p3){
    const X a0=X(p1.x)-p2.x,a1=X(p1.y)-p2.y,a2=X(p1.z)-p2.z;
    const float b0=float(X(p1.x)-p3.x),b1=float(X(p1.y)-p3.y);
    const X b2=X(p1.z)-p3.z;
    CourseProbe n{float(b2*a1-X(b1)*a2),float(a2*b0-b2*a0),float(X(b1)*a0-X(b0)*a1)};
    n=pc_normalize_vector_40ef00(n);
    const float d=float(-((X(n.y)*p1.y+X(n.z)*p1.z)+X(n.x)*p1.x));
    return {n.x,n.y,n.z,d};
}
float camera_angle_lerp_449580(float from,float to,float t){
    const float a=wrap(from),b=wrap(to);
    const float d=wrap(b-a);
    return float(X(d)*t+a);
}
void camera_fov_483c10(Bytes c,const PcCameraScreen& s){
    const X p=x87_tan(X(c.f32(0xac))*0.5f)*(X(s.width_740c8c)/s.height_740c90)*0.75f;
    const X q=(X(s.height_740c90)/s.width_740c8c)*p;
    const X a4=x87_atan(p),a0=x87_atan(q);
    c.putf(0xa4,float(a4+a4));
    c.putf(0xa0,float(a0+a0));
}
void camera_perspective_404310(PcMatrixStack& s,float fov,float aspect,float zn,float zf,
    float shift_x,float shift_y,PcCameraDevice& d){
    const X t=x87_tan(X(float(X(fov)*0.5f)))*zn; // fstp to the float argument, then FPTAN
    const X u=X(aspect)*t;
    const float neg_u=float(-u);
    const X tsy=t*shift_y;
    const float ushift=float(u*shift_x);
    const float top=float(t+tsy),bottom=float(-t+tsy);
    const float right=float(u+ushift),left=float(X(neg_u)+ushift);
    d.frustum_95bf40={left,right,bottom,top,zn,zf};
    pc_d3dx_perspective_off_center_rh(s.current(),left,right,bottom,top,zn,zf);
    const CourseProbe o{0.f,0.f,0.f};
    const CourseProbe v1{-left,-top,zn},v2{-right,-top,zn},v3{-right,-bottom,zn},v4{-left,-bottom,zn};
    d.planes_95bf58[0]=pc_d3dx_plane_from_points(o,v1,v4);
    d.planes_95bf58[1]=pc_d3dx_plane_from_points(o,v3,v2);
    d.planes_95bf58[2]=pc_d3dx_plane_from_points(o,v4,v3);
    d.planes_95bf58[3]=pc_d3dx_plane_from_points(o,v2,v1);
}
void camera_device_store(PcMatrixStack& s,PcCameraDevice& d,std::uint32_t slot){
    PcCameraDevice::Store e{slot,{}};
    const auto m=s.current();for(unsigned k=0;k<64;++k)e.matrix[k]=m.u8(k);
    if(slot<d.slots_95d860.size())d.slots_95d860[slot]=e.matrix;
    d.stores.push_back(e);d.transforms.push_back(slot);
}
void camera_view_482f20(Bytes c,PcMatrixStack& s,PcCameraDevice& d,float f){
    c.check(0,0x3b4);
    const auto eye=get3(c,0xf8),prev_eye=get3(c,0x390);
    bool reset=course_vec3_distance({eye.x,eye.y,eye.z},{prev_eye.x,prev_eye.y,prev_eye.z})>10.0L;
    if(!reset){const auto t=get3(c,0x104),pt=get3(c,0x39c);
        reset=course_vec3_distance({t.x,t.y,t.z},{pt.x,pt.y,pt.z})>10.0L;}
    if(reset){copy_words(c,0x384,0x128,3);copy_words(c,0x3a8,0xd4,3);copy_words(c,0x390,0xf8,3);copy_words(c,0x39c,0x104,3);}
    const CourseProbe l{camera_angle_lerp_449580(c.f32(0x384),c.f32(0x128),f),
                        camera_angle_lerp_449580(c.f32(0x388),c.f32(0x12c),f),
                        camera_angle_lerp_449580(c.f32(0x38c),c.f32(0x130),f)};
    const float g=1.f-f;
    const auto a=get3(c,0xf8),b=get3(c,0x390);
    const CourseProbe p{float(X(b.x)*g+X(a.x)*f),
                        float(X(float(X(b.y)*g))+X(a.y)*f),
                        float(X(float(X(b.z)*g))+float(X(a.z)*f))};
    pc_matrix_push_unit(s);
    pc_matrix_rotate_z(s,0.f-l.z);pc_matrix_rotate_x(s,0.f-l.x);pc_matrix_rotate_y(s,0.f-l.y);
    pc_matrix_translate_vector(s,{-p.x,-p.y,-p.z}); // 40A310
    copy_words(c,0x180,0x140,16);
    pc_matrix_get(s,c.sub(0x140,64));
    camera_device_store(s,d,0);
    pc_matrix_pop(s);
    copy_words(c,0x384,0x128,3);copy_words(c,0x390,0xf8,3);copy_words(c,0x39c,0x104,3);
}
void camera_project_484bd0(Bytes c,PcMatrixStack& s,PcCameraDevice& d,
    const PcCameraScreen& screen,const PcCameraBlend& blend){
    c.check(0,0x3b4);
    pc_matrix_push(s);
    camera_fov_483c10(c,screen);
    camera_perspective_404310(s,c.f32(0xa0),c.f32(0xb8),c.f32(0xbc),c.f32(0xc0),c.f32(0xc4),c.f32(0xc8),d);
    camera_device_store(s,d,1);
    pc_matrix_pop(s);
    camera_view_482f20(c,s,d,camera_blend_4493e0(blend));
    pc_matrix_push_load(s,c.sub(0x140,64));
    pc_d3dx_matrix_inverse(s.current(),nullptr,s.current()); // 40A240
    pc_matrix_get(s,c.sub(0x1c0,64));
    pc_matrix_pop(s);
    c.putf(0xb4,float(X(240.f)/x87_tan(X(c.f32(0xa0))*0.5f)));
    c.putf(0xb0,float(X(320.f)/x87_tan(X(c.f32(0xa4))*0.5f)));
}
void camera_init_484ee0(Bytes c,PcMatrixStack& s,PcCameraDevice& d,const PcCameraScreen& screen,
    const PcCameraBlend& blend,std::int32_t steering,PcCameraTables& t){
    c.check(0,0x3b4);
    t.rom_5b4a30.check(0,0x64c);t.rom_5ba4e0.check(0,0x438);t.rom_5b5080.check(0,0x5460);
    t.live_818fe8.check(0,0x64c);t.live_818bb0.check(0,0x438);t.live_813750.check(0,0x5460);
    c.put32(0x80,0x3f8fced9u); // 5BA96C 1.1235
    c.putf(0x84,0.5f);c.putf(0x88,3200.f);
    for(unsigned o:{0x0u,0x4u,0x8u,0x18u,0x1cu,0x20u})c.putf(o,0.f);
    c.putf(0x8c,0.f);c.putf(0x90,0.f);c.put32(0x94,0);
    for(unsigned k=0;k<16;++k)c.putf(0x40+k*4,(k%5u)==0u?1.f:0.f); // 40A060 identity
    c.putf(0xb8,screen.width_740c8c/screen.height_740c90);
    c.putf(0xbc,0.1f);c.putf(0xc0,5000.f);
    c.put8(0x34b,0x1e);c.put8(0x34a,0x1e);
    auto row=[&]{return t.rom_5b4a30.sub(std::size_t(std::int32_t(c.i8(0x34a))*0x34),0x34);};
    if(!(blend.interpolation_override_82e7d8!=0&&steering!=0)){
        c.put32(0xa8,0x3f5f66f3u);c.put32(0xa4,0x3f5f66f3u);c.put32(0xac,0x3f5f66f3u); // 5BA964
        camera_fov_483c10(c,screen);
        const auto r=row();
        for(unsigned k=0;k<3;++k){c.put32(0x110+k*4,r.u32(k*4));c.put32(0xe0+k*4,r.u32(k*4));
            c.put32(0x11c+k*4,r.u32(0xc+k*4));c.put32(0xec+k*4,r.u32(0xc+k*4));}
    }
    copy_words(c,0x390,0xf8,3);copy_words(c,0x3a8,0xd4,3);copy_words(c,0x39c,0x104,3);
    const auto r=row();
    c.putf(0x384,0.f);c.putf(0x388,0.f);c.putf(0x38c,0.f);
    c.put32(0xcc,r.u32(0x18));c.put32(0xd0,r.u32(0x1c));c.put32(0xc4,r.u32(0x18));c.put32(0xc8,r.u32(0x1c));
    c.put32(0x35c,r.u32(0x28));c.put32(0x358,r.u32(0x28));
    for(unsigned o:{0x33cu,0x368u,0x33eu,0x340u,0x342u,0x344u,0x346u,0x348u})c.put16(o,0);
    c.put32(0x34c,r.u32(0x30));
    pc_matrix_push_unit(s);
    for(unsigned o:{0x140u,0x180u,0x1c0u,0x200u,0x240u,0x280u,0x2c0u})pc_matrix_get(s,c.sub(o,64));
    pc_matrix_pop(s);
    for(unsigned k=0;k<0x64c;++k)t.live_818fe8.put8(k,t.rom_5b4a30.u8(k));
    for(unsigned k=0;k<0x438;++k)t.live_818bb0.put8(k,t.rom_5ba4e0.u8(k));
    for(unsigned k=0;k<0x5460;++k)t.live_813750.put8(k,t.rom_5b5080.u8(k));
    for(unsigned k=0;k<15;++k)c.put32(0x300+k*4,0);
    c.putf(0x374,0.f);c.putf(0x370,0.f);c.putf(0x36c,0.f);
    c.put32(0x354,0);c.put32(0x378,0);c.put32(0x37c,0);c.putf(0x380,1.f);
    camera_project_484bd0(c,s,d,screen,blend);
}
void camera_car_flags_4a2ba0(Bytes car,Bytes c,std::uint8_t display,std::int32_t mode){
    car.check(0,0xea2);c.check(0,0x3b4);
    std::int8_t dl=2;std::int16_t di=0;bool counted=false; // counted: 4A2BE6 path
    auto flags=[&]{return car.u32(4);};auto set=[&](std::uint32_t v){car.put32(4,v);};
    if((display&3u)==2u){
        const std::uint32_t m=std::uint32_t(mode)-15u;
        if(m<=12u){
            static constexpr std::uint8_t kind[13]{0,0,0,0,2,0,0,0,2,2,2,2,1};
            if(kind[m]==0){dl=c.i8(0x34a);counted=true;}
            else if(kind[m]==1){const auto a=c.i8(0x34a);if(a>=2){dl=a;counted=true;}}
        }
    }
    bool to_c87=false;
    if(counted){
        di=c.i16(0x33c);
        if(di>0){
            auto v=(flags()&~0x8000u)|0x4000u;set(v);
            if(v&0x10u){
                if(dl==0&&c.i16(0x368)!=0)to_c87=true;
                else set((v&~0x4000u)|0x8000u);
            }
            goto after;
        }
    }
    switch(dl){
    case 0: if(car.i16(0xea0)>0)set((flags()&~0x4000u)|0x8000u);else set(flags()&~0xc000u);break;
    case 1: set((flags()&~0x4000u)|0x8000u);break;
    default:set((flags()&~0x8000u)|0x4000u);break;
    }
after:
    if(!to_c87&&dl!=0){car.put16(0xea0,0);car.putf(0xe9c,1.f);return;}
    if(car.i16(0xea0)<di){
        const std::int32_t yaw=car.i16(0x162);car.put16(0xea0,std::uint16_t(di));
        const float angle=float(X(yaw)*9.58738019107841e-05f); // 4493A0 * 628254
        car.putf(0xe9c,float(x87_cos(angle)));
    }
    if((flags()&0xc000u)==0x8000u){
        const std::int16_t count=car.i16(0xea0);
        const float step=car.f32(0xe9c)/float(count);
        car.putf(0xe9c,car.f32(0xe9c)-step);car.put16(0xea0,std::uint16_t(count-1));
        return;
    }
    car.putf(0xe9c,1.f);
    if(car.i16(0xea0)>10)car.put16(0xea0,10);
}
void camera_car_test_482e80(Bytes c,const PcCameraControl& k){
    auto car=k.car_799d18;car.check(0,0xea2);
    car.put32(4,car.u32(4)&~0x10u);
    c.put32(0x354,0);
    const auto box=k.box_5ba4e0.sub(0xc+std::size_t(std::int32_t(car.i8(0x11))*0x24),0x18);
    const float x=c.f32(0xe0),y=c.f32(0xe4),z=c.f32(0xe8)-1.f;
    const bool outside=x>box.f32(0)||y>box.f32(4)||z>box.f32(8)||
        box.f32(0xc)>c.f32(0xe0)||box.f32(0x10)>c.f32(0xe4)||box.f32(0x14)>z;
    if(!outside){car.put32(4,car.u32(4)|0x10u);c.put32(0x354,1);}
    camera_car_flags_4a2ba0(car,c,k.display_79fcc9,k.game_mode_78026c);
}
// 486080 jump table: modes 13/14/16/17/18 share the race case 48618D.
bool race_camera_mode_48618d(std::int32_t mode){return mode==13||mode==14||mode==16||mode==17||mode==18;}
bool camera_control_485fe0(Bytes c,PcMatrixStack& s,PcCameraDevice& d,const PcCameraScreen& screen,
    const PcCameraBlend& blend,const PcCameraControl& k){
    c.check(0,0x3b4);
    const bool paused=k.pause_780248!=0&&c.u32(0x94)==0;
    const bool frontend=c.u32(0x94)==0&&c.u32(0x98)==0&&c.u32(0x9c)==0&&(k.game_mode_78026c==32||k.game_mode_78026c==36);   // 486080 entry 6 (4860F7): modes 32 and 36
    const bool race=c.u32(0x94)==0&&c.u32(0x98)==0&&c.u32(0x9c)==0&&race_camera_mode_48618d(k.game_mode_78026c)&&k.race!=nullptr;
    const std::int32_t mode=k.game_mode_78026c;
    const bool goal=c.u32(0x94)==0&&c.u32(0x98)==0&&c.u32(0x9c)==0&&k.race!=nullptr&&
        (mode==0x13||mode==0x1b||mode==0x22||mode==0x23);                     // 4861F8 -> 4853D0
    const bool ending=c.u32(0x94)==0&&c.u32(0x98)==0&&c.u32(0x9c)==0&&mode==0x18;      // 4861D0 -> 485470
    const bool timeover=c.u32(0x94)==0&&c.u32(0x98)==0&&c.u32(0x9c)==0&&k.race!=nullptr&&
        (mode==20||mode==21||mode==22);                                         // 4861C0 -> 485830
    // 486080 table entries 4861D5 (RET after the head): every mode but 3, 10, 13/14/16/17/18,
    // 19/27/34/35, 20/21/22, 24 and 32/36 (and the modes outside 3..36).
    const bool idle=c.u32(0x94)==0&&c.u32(0x98)==0&&c.u32(0x9c)==0&&!(mode==3||mode==10||race_camera_mode_48618d(mode)||
        mode==0x13||mode==0x1b||mode==0x22||mode==0x23||mode==20||mode==21||mode==22||mode==0x18||mode==32||mode==36);
    const bool attract=c.u32(0x94)==0&&c.u32(0x98)==0&&c.u32(0x9c)==0&&mode==10;   // 48608E
    const bool demo=c.u32(0x94)==0&&c.u32(0x98)==0&&c.u32(0x9c)==0&&mode==3&&k.race!=nullptr;   // 48616A
    // +94/+98/+9C (4844B0 / 484220 / 483ED0, pad-driven debug cameras): no retail code
    // makes them non-zero (484EE0 clears +94; only those cameras write +98/+9C).
    if(!paused&&!frontend&&!race&&!goal&&!ending&&!timeover&&!idle&&!attract&&!demo)return false;
    if(attract&&!paused){   // eye 5BA980.. (0, 4.65, 0), angles 5BA970.. (-0.192, 0, 0), fov 5BA97C, 484BD0
        copy_words(c,0x3a8,0xd4,3);
        camera_car_test_482e80(c,k);
        static constexpr std::uint32_t eye[3]{0u,0x4094cccdu,0u},angle[3]{0xbe449809u,0u,0u};
        for(unsigned n=0;n<3;++n){c.put32(0xf8+n*4,eye[n]);c.put32(0x128+n*4,angle[n]);}
        for(unsigned o:{0xa0u,0xa4u,0xacu})c.put32(o,0x3f1c61aau);
        camera_project_484bd0(c,s,d,screen,blend);
        return true;
    }
    if(idle&&!paused){
        copy_words(c,0x3a8,0xd4,3);
        camera_car_test_482e80(c,k);
        return true;
    }
    if(race&&!paused){
        copy_words(c,0x3a8,0xd4,3);
        camera_car_test_482e80(c,k);
        race_camera_mode16(c,s,d,screen,blend,*k.race);
        return true;
    }
    if(demo&&!paused){
        copy_words(c,0x3a8,0xd4,3);
        camera_car_test_482e80(c,k);
        // 49EED0 attract step 3/4 with the 82E7D8 override (4872E0): the ending view 485470.
        const std::int32_t v=k.race->demo_round_83daf4;
        if(v>2&&v<=4&&blend.interpolation_override_82e7d8!=0)race_camera_ending_485470(c,s,d,screen,blend);
        else race_camera_demo_485b40(c,s,d,screen,blend,*k.race);
        return true;
    }
    if(goal&&!paused){
        copy_words(c,0x3a8,0xd4,3);
        camera_car_test_482e80(c,k);
        race_camera_goal_4853d0(c,s,d,screen,blend,*k.race);
        return true;
    }
    if(timeover&&!paused){
        copy_words(c,0x3a8,0xd4,3);
        camera_car_test_482e80(c,k);
        race_camera_timeover_485830(c,s,d,screen,blend,*k.race);
        return true;
    }
    if(ending&&!paused){
        copy_words(c,0x3a8,0xd4,3);
        camera_car_test_482e80(c,k);
        race_camera_ending_485470(c,s,d,screen,blend);
        return true;
    }
    if(frontend&&!paused&&(k.preset_819634<0||k.preset_819634>1))
        throw std::out_of_range("camera preset 819634 outside the two authored entries");
    copy_words(c,0x3a8,0xd4,3);
    // 407930 only loads a protected constant into EAX; nothing is stored.
    if(paused){camera_project_484bd0(c,s,d,screen,blend);return true;}
    camera_car_test_482e80(c,k);
    static constexpr std::uint32_t eye[2][3]{{0xbfc66666u,0x40b80000u,0x40300000u},{0xc0400000u,0x40e4cccdu,0x40e00000u}};
    static constexpr std::uint32_t angle[2][3]{{0xbe449809u,0,0},{0xbe449809u,0,0}};
    const auto p=std::size_t(k.preset_819634);
    for(unsigned n=0;n<3;++n){c.put32(0xf8+n*4,eye[p][n]);c.put32(0x128+n*4,angle[p][n]);}
    for(unsigned o:{0xa0u,0xa4u,0xacu})c.put32(o,0x3f1c61aau); // 5BA948
    camera_project_484bd0(c,s,d,screen,blend);
    return true;
}
}

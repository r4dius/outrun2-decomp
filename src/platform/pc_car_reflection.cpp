#include "platform/pc_car_reflection.hpp"
#include "driving/pc_camera.hpp"
#include "driving/pc_d3dx.hpp"
#include "driving/pc_x87.hpp"
#include <cmath>
#include <cstring>
#include <stdexcept>
#include "driving/service_hole.hpp"
namespace outrun::platform {
namespace {
using driving::Bytes;
using driving::CourseProbe;
using driving::PcMatrixStack;
using X=driving::X87;
using M=std::array<float,16>;
float fb(std::uint32_t u){float f;std::memcpy(&f,&u,4);return f;}
// EXE .rdata 6222F0..622344 (413B30 sources).
constexpr std::uint32_t Const6222f0[22]{0x3fe66666u,0u,0x3c23d70au,0x40c00000u,0x40c00000u,0u,0xbf800000u,0x40400000u,
    0x40800000u,0u,0xbeb33333u,0x40e00000u,0x3f4ccccdu,0u,0x3e99999au,0x3fc00000u,
    0x3dcccccdu,0x3dcccccdu,0x3f266666u,0x3e800000u,0x3f800000u,0x3f800000u};
M current(const PcMatrixStack& s){M m{};const auto b=s.current();for(unsigned k=0;k<16;++k)m[k]=b.f32(k*4);return m;}
void put_current(PcMatrixStack& s,const M& m){auto b=s.current();for(unsigned k=0;k<16;++k)b.putf(k*4,m[k]);}
// D3DXMatrixTranslation (bit moves into an identity) then current = T * current (439394).
void translate_left(PcMatrixStack& s,float x,float y,float z){
    std::array<float,16> t{1,0,0,0, 0,1,0,0, 0,0,1,0, x,y,z,1};
    driving::pc_matrix_multiply_current(s,Bytes(t.data(),64));
}
void scale_left(PcMatrixStack& s,float k){
    std::array<float,16> t{};driving::pc_d3dx_matrix_scaling(Bytes(t.data(),64),k,k,k);
    driving::pc_matrix_multiply_current(s,Bytes(t.data(),64));
}
// Inline pop (dec [89B568]; js; ptr -= 0x40): true when the pointer moved.
bool pop_moved(PcMatrixStack& s){const auto before=s.current_offset;driving::pc_matrix_pop(s);return s.current_offset!=before;}
// 404310 into the current matrix; the frustum globals 95BF40/95BF58 and the culling copy.
void perspective(PcFlushContext& c,PcMatrixStack& s,PcRenderView& view,float fov,float aspect,float zn,float zf,float sx,float sy){
    driving::PcCameraDevice d;
    driving::camera_perspective_404310(s,fov,aspect,zn,zf,sx,sy,d);
    for(unsigned k=0;k<6;++k)c.g.putf(0x95bf40u+k*4,d.frustum_95bf40[k]);
    for(unsigned p=0;p<4;++p)for(unsigned k=0;k<4;++k)c.g.putf(0x95bf58u+p*16+k*4,d.planes_95bf58[p][k]);
    view.frustum_95bf40=d.frustum_95bf40;view.planes_95bf58=d.planes_95bf58;
}
// 410FF0 on 95D860 (as 410F90 slot 0) and the culling copy of the view.
void set_view(PcFlushContext& c,PcRenderView& view,const M& m){
    render_set_matrix_410f90(c,m,0);
    std::memcpy(view.view_95d860.data(),m.data(),64);
}
}
void matrix_push_slot_409ea0(PcMatrixStack& s,PcRenderGlobals& g,std::uint32_t slot){
    const auto before=s.current_offset;
    driving::pc_matrix_push(s);
    if(s.current_offset==before)return;                 // depth >= capacity: no copy, no move
    const auto m=g.matrix(0x95d860u+slot*0x40u);
    auto below=s.storage.sub(std::size_t(before),64);
    for(unsigned k=0;k<16;++k)below.putf(k*4,m[k]);
}
void car_reflection_init_413b30(PcFlushContext& c,PcMatrixStack& s){
    auto& g=c.g;auto& d=c.device;
    for(std::uint32_t a:{0x8a89f0u,0x8a89f4u,0x8a89f8u,0x8a89fcu,0x8a8a00u})g.w(a)=0;
    g.w(0x8a8bd0)=1;g.w(0x8a8bf8)=0;g.w(0x8a8bf4)=0;
    g.w(0x8a89f4)=d.create_cube_texture(0x80,1,1,0x15,0);          // D3DUSAGE_RENDERTARGET, A8R8G8B8, D3DPOOL_DEFAULT
    g.w(0x8a8a00)=d.create_depth_stencil_surface(0x80,0x80,0x4b,0,0,0); // D24S8
    for(unsigned k=0;k<16;++k)g.w(0x8a8a04u+k*4)=Const6222f0[k];
    {std::array<float,4> c9{};for(unsigned k=0;k<4;++k)c9[k]=g.f(0x8a8a04u+k*4);d.set_vertex_shader_constant_f(9,c9.data(),1);}
    for(unsigned k=0;k<6;++k)g.w(0x8a8bd4u+k*4)=Const6222f0[16+k];
    g.w(0x8a8bec)=0x43c80000u;g.w(0x8a8bf0)=0x42b40000u;             // 400, 90
    g.w(0x8a8a44)=0;g.w(0x8a8a48)=0x40000000u;g.w(0x8a8a4c)=0x3fc00000u; // eye offset (0, 2, 1.5)
    auto face=[&](std::uint32_t at,int axis,std::uint32_t angle){
        driving::pc_matrix_identity(s);
        if(axis=='Y')driving::pc_matrix_rotate_y(s,fb(angle));else if(axis=='X')driving::pc_matrix_rotate_x(s,fb(angle));
        g.put_matrix(at,current(s));
    };
    face(0x8a8a50,'Y',0x3fc90fdbu);face(0x8a8a90,'Y',0xbfc90fdbu);
    face(0x8a8ad0,'X',0xbfc90fdbu);face(0x8a8b10,'X',0x3fc90fdbu);
    face(0x8a8b50,0,0);face(0x8a8b90,'Y',0x40490fdbu);
    g.w(0x8a89f0)|=1u;
}
void car_reflection_release_413f50(PcFlushContext& c){
    auto& g=c.g;
    if(g.w(0x8a8a00)){c.device.release(g.w(0x8a8a00));g.w(0x8a8a00)=0;}
    if(g.w(0x8a89f4))c.device.release(g.w(0x8a89f4));
    for(std::uint32_t a:{0x8a89f0u,0x8a89f4u,0x8a89f8u,0x8a89fcu,0x8a8a00u})g.w(a)=0;
    g.w(0x8a8bd0)=1;g.w(0x8a8bf8)=0;g.w(0x8a8bf4)=0;
}
void car_reflection_restore_413fc0(PcFlushContext& c,std::uint8_t flags){
    if((flags&3u)!=2u)return;
    auto& g=c.g;auto& d=c.device;
    d.set_depth_stencil_surface(g.w(0x8a89fc));
    d.set_render_target(0,g.w(0x8a89f8));
    if(g.w(0x8a89f8)){d.release(g.w(0x8a89f8));g.w(0x8a89f8)=0;}
    if(g.w(0x8a89fc)){d.release(g.w(0x8a89fc));g.w(0x8a89fc)=0;}
    g.w(0x8a89f0)&=~2u;
}
void car_reflection_align_40a6d0(PcMatrixStack& s,const CourseProbe& a,const CourseProbe& b){
    CourseProbe axis{float(X(a.z)*b.y-X(b.z)*a.y),float(X(b.z)*a.x-X(a.z)*b.x),float(X(b.x)*a.y-X(a.x)*b.y)};
    const float dot=float((X(b.x)*a.x+X(b.z)*a.z)+X(b.y)*a.y);
    const X sq=(X(axis.z)*axis.z+X(axis.y)*axis.y)+X(axis.x)*axis.x;
    const X mag=sq.v<0?-sq:sq;                                       // fabs
    const bool below=std::isnan(mag.v)||mag<X(fb(0x34000000u));      // fcomp [6281F0]: C0 (or unordered)
    if(!below){
        const float sn=float(driving::pc_unit_vector_40eeb0(axis));  // 40EEB0: length, axis normalised in place
        render_rotate_axis_40a580(s,axis,sn,dot);
        return;
    }
    if(dot<0.f)render_rotate_axis_40a580(s,{b.y,b.z,b.x},0.f,-1.f); // fcomp [619A34] = 0.0: C0 alone
}
void car_reflection_scene_414050(PcFlushContext& c,PcMatrixStack& s,const PcCarReflectionInputs& in,
                                 const PcCarReflectionServices& sv,PcCarReflectionStats& st){
    auto& g=c.g;auto& d=c.device;
    driving::pc_matrix_push(s);
    const std::uint32_t alpha=std::uint32_t(driving::x87_ftol32(X(g.f(0x8a8bd8))*X(255.f)));   // 582194
    d.clear(7,(alpha<<24)|0x808080u,1.f,0);
    std::array<std::uint8_t,0xa0> light{};
    in.light_899c38.check(0,0xa0);for(unsigned k=0;k<0xa0;++k)light[k]=in.light_899c38.u8(k);
    Bytes l(light.data(),light.size());
    for(unsigned o:{0x08u,0x0cu,0x10u,0x6cu,0x70u,0x74u,0x78u,0x7cu,0x80u})l.putf(o,float(X(g.f(0x8a8bd4))*l.f32(o)));
    render_lights_reset_410740(c);
    render_light_add_4107a0(c,Bytes(light.data(),0x94));
    g.w(0x897d30)=1;g.w(0x897d40)=1;g.w(0x897d44)=7;g.w(0x897d34)=0;g.w(0x897d38)=0;g.w(0x897d3c)=0x80;
    g.w(0x897d48)=1;g.w(0x897d58)=0;g.w(0x897d5c)=7;g.w(0x897d4c)=1;g.w(0x897d50)=0;g.w(0x897d54)=8;
    if(!sv.env_model_44cd30)throw std::runtime_error("414050: no 44CD30 service");
    sv.env_model_44cd30();
    if(sv.flush_alpha_405830)sv.flush_alpha_405830();else outrun::driving::service_hole("car_reflection_scene_414050","sv.flush_alpha_405830");
    render_light_add_4107a0(c,in.light_899c38.sub(0,0x94));
    render_pass_defaults_404540(g,in.layer_7d25f0);
    {const std::array<float,4> c10{g.f(0x8a8bdc),g.f(0x8a8bdc),g.f(0x8a8bdc),g.f(0x8a8be0)};d.set_vertex_shader_constant_f(10,c10.data(),1);}
    if(!sv.env_sky_451c20)throw std::runtime_error("414050: no 451C20 service");
    sv.env_sky_451c20();
    if(in.scn_efc.size()==0){++st.skipped_closed_scn_efc;}           // PC: [79F5EC] = 0 faults here
    else if(in.scn_efc.u8(0)&2u){
        // Inline push of 95DBA0 (the face's inverse view), its rotation reset to identity.
        const auto before=s.current_offset;driving::pc_matrix_push(s);
        if(s.current_offset!=before)put_current(s,g.matrix(0x95dba0u));
        driving::pc_matrix_unit_rotation(s);
        car_reflection_align_40a6d0(s,{l.f32(0x44),l.f32(0x48),l.f32(0x4c)},{0.f,0.f,1.f});   // 624BEC
        translate_left(s,0.f,0.f,-g.f(0x8a8bec));
        scale_left(s,g.f(0x8a8bf0));
        if(!sv.draw_4056d0)throw std::runtime_error("414050: no 4056D0 service");
        sv.draw_4056d0(0x57000fu,in.scn_efc.u32(0x9c),0u,0xffffffffu);
        driving::pc_matrix_pop(s);
    }
    driving::pc_matrix_pop(s);
}
void car_reflection_frame_414340(PcFlushContext& c,PcMatrixStack& s,PcRenderView& view,
                                 const PcCarReflectionInputs& in,const PcCarReflectionServices& sv,PcCarReflectionStats& st){
    if((in.player_flags_79fb50&3u)!=2u)return;
    if(in.final_stage_7d33d0!=0u&&in.car.u32(0x5c)!=0u)return;
    auto& g=c.g;auto& d=c.device;
    ++st.frames;
    const std::uint32_t cube=g.w(0x8a89f4);
    matrix_push_slot_409ea0(s,g,1);
    matrix_push_slot_409ea0(s,g,0);
    driving::pc_matrix_push(s);
    driving::pc_matrix_load(s,in.car.sub(0xb0,64));
    const auto eye=driving::pc_d3dx_vec3_transform_coord({g.f(0x8a8a44),g.f(0x8a8a48),g.f(0x8a8a4c)},current(s));
    perspective(c,s,view,fb(0x3fc90fdbu),1.f,fb(0x3dcccccdu),2000.f,0.f,0.f);
    g.put_matrix(0x95d8a0u,current(s));
    if((in.player_flags_79fb50&3u)==2u){
        g.w(0x8a89f8)=d.get_render_target(0);
        g.w(0x8a89fc)=d.get_depth_stencil_surface();
        g.w(0x8a89f0)|=2u;
    }
    std::uint32_t count;
    if((in.car.u32(4)&0xc000u)==0x8000u)count=3;
    else count=g.f(0x8a8bf8)<fb(0x3fb33333u)?3u:2u;                   // [6282DC] = 1.4 ms
    std::uint32_t face=g.w(0x8a8bd0);
    for(std::uint32_t n=count;n;--n){
        put_current(s,g.matrix(0x8a8a50u+face*0x40u));
        translate_left(s,-eye[0],-eye[1],-eye[2]);
        set_view(c,view,current(s));                                   // 95D860, 410FF0
        driving::pc_matrix_identity(s);
        render_set_matrix_410f90(c,current(s),6);                      // 95D9E0, 411060
        if((in.player_flags_79fb50&3u)==2u){
            const std::uint32_t surface=d.get_cube_map_surface(cube,face,0);
            d.set_depth_stencil_surface(g.w(0x8a8a00));
            d.set_render_target(0,surface);
            if(surface)d.release(surface);
        }
        const float t0=sv.elapsed_449df0?sv.elapsed_449df0():(outrun::driving::service_hole("car_reflection_frame_414340","sv.elapsed_449df0"),0.f);
        car_reflection_scene_414050(c,s,in,sv,st);
        const float t1=sv.elapsed_449df0?sv.elapsed_449df0():(outrun::driving::service_hole("car_reflection_frame_414340","sv.elapsed_449df0"),0.f);
        ++face;++st.faces;
        g.putf(0x8a8bf4,float((X(t1)-X(t0))+X(g.f(0x8a8bf4))));
        if(face==6u){face=0;g.w(0x8a8bf8)=g.w(0x8a8bf4);g.w(0x8a8bf4)=0;}
    }
    g.w(0x8a8bd0)=face;
    car_reflection_restore_413fc0(c,in.player_flags_79fb50);
    const auto cam=in.camera;
    perspective(c,s,view,cam.f32(0xa0),cam.f32(0xb8),cam.f32(0xbc),cam.f32(0xc0),cam.f32(0xc4),cam.f32(0xc8));
    g.put_matrix(0x95d8a0u,current(s));
    driving::pc_matrix_pop(s);
    // The saved view (409EA0(0)) back into 95D860, the matrix above copied down.
    if(pop_moved(s)){
        set_view(c,view,current(s));
        const auto above=s.storage.sub(std::size_t(s.current_offset+64),64);
        for(unsigned k=0;k<64;k+=4)s.current().put32(k,above.u32(k));
    }
    // The saved projection (409EA0(1)) back into 95D8A0, the same copy.
    if(pop_moved(s)){
        g.put_matrix(0x95d8a0u,current(s));
        const auto above=s.storage.sub(std::size_t(s.current_offset+64),64);
        for(unsigned k=0;k<64;k+=4)s.current().put32(k,above.u32(k));
    }
}
}

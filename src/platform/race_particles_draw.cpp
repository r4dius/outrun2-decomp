// PART_EFC display 41BD10: render state save/restore (41B5C0/41B7A0), the
// tire marks (486450) and nlParticleDraw (418490) with the draw functions of
// the source types used by the event: 2 = 418CF0 (screen quads), 3 = 419210
// -> 419DC0 (camera-facing quads; [74F7C4] is 3 in .data and no code writes it,
// so its 4197D0 sorted quads (1) and point sprites (0) never run),
// 5 = 41A6F0 and 6 = 41AE40 (point sprites). Types 0, 1 and 4 (418590,
// 418910 screen quads, 41A520 lines) are set by no source of the event.
// Device calls go to PcParticleDevice in PC order; the renderer leaves
// 408880, 411060 (after the 95D9E0 copy) and 410F90 (via 409DF0) go through
// the service in the same order.
#include "platform/race_particles_port.hpp"
#include "platform/pc_screen.hpp"
namespace outrun::platform {
using namespace particles_detail;
namespace {
struct D {
    P& p;PcParticleDevice& d;
    D(P& pp,PcParticleDevice& dd):p(pp),d(dd){}
    // "get; if different set" (render / texture stage / sampler states).
    void rs(std::uint32_t s,std::uint32_t v){if(d.get_render_state(s)!=v)d.set_render_state(s,v);}
    void tss(std::uint32_t st_,std::uint32_t t,std::uint32_t v){if(d.get_texture_stage_state(st_,t)!=v)d.set_texture_stage_state(st_,t,v);}
    void ss(std::uint32_t st_,std::uint32_t t,std::uint32_t v){if(d.get_sampler_state(st_,t)!=v)d.set_sampler_state(st_,t,v);}
    void draw(std::uint32_t type,std::uint32_t count,std::uint32_t data,std::uint32_t stride,std::uint32_t vertices){
        d.draw_primitive_up(type,count,p.m.at(data,std::size_t(vertices)*stride),stride);
    }
    void matrix(std::array<float,16>& out)const{const auto b=p.current();for(unsigned k=0;k<16;++k)out[k]=b.f32(k*4);}
    void set_transform(std::uint32_t state){std::array<float,16> m{};matrix(m);d.set_transform(state,m.data());}
};
// D3DXVec4Transform (generic 44067A) of (x, y, z, w) by the current matrix.
std::array<float,4> vec4(const P& p,float x,float y,float z,float w){
    const auto m=p.current();
    auto f=[&](unsigned r,unsigned c){return X(m.f32(r*16+c*4));};
    const X vx=x,vy=y,vz=z,vw=w;
    return {st(((f(3,0)*vw+f(1,0)*vy)+f(2,0)*vz)+vx*f(0,0)),
            st(((f(3,1)*vw+f(1,1)*vy)+f(0,1)*vx)+f(2,1)*vz),
            st(((f(3,2)*vw+f(1,2)*vy)+f(0,2)*vx)+f(2,2)*vz),
            st(((f(3,3)*vw+f(1,3)*vy)+f(0,3)*vx)+f(2,3)*vz)};
}
// The inline push of the renderer matrix 95D8A0 and the two products with
// 95D860 and the previous stack entry (418CF0/419DC0).
std::array<float,4> push_view(P& p,bool products){
    p.push_load(0x95d8a0u);
    const auto v=vec4(p,1.f,1.f,0.f,1.f);
    if(products){
        driving::pc_matrix_multiply_current(p.c.matrices,p.m.bytes(0x95d860u,64));
        const auto cur=p.c.matrices.current();
        std::array<std::uint8_t,64> prev{};
        const auto pb=p.c.matrices.storage.sub(std::size_t(p.c.matrices.current_offset-64),64);
        for(unsigned k=0;k<64;++k)prev[k]=pb.u8(k);
        (void)cur;
        driving::pc_matrix_multiply_current(p.c.matrices,driving::Bytes(prev.data(),64));
    }
    return v;
}
// 41B550: protected snippet (EAX = [89BD60]) then render states 13/14 and
// the pixel shader [95AF9C].
void pixel_state_41b550(D& d){d.rs(0x13,2);d.rs(0x14,6);d.d.set_pixel_shader(d.p.u(0x95af9cu));}
constexpr auto& SavedStates=ParticleSavedStates41b5c0;
// 41B5C0: saves the render states above and the stage 0..3 textures.
void save_41b5c0(D& d){
    for(const auto& s:SavedStates)d.p.put(s[1],d.d.get_render_state(s[0]));
    for(std::uint32_t k=0;k<4;++k)d.p.put(0x8a92d0u+k*4,d.d.get_texture(k));
}
// 41B7A0: restores them (texture references released).
void restore_41b7a0(D& d){
    for(const auto& s:SavedStates)d.rs(s[0],d.p.u(s[1]));
    for(std::uint32_t k=0;k<4;++k){
        const std::uint32_t t=d.p.u(0x8a92d0u+k*4);
        d.d.set_texture(k,t);
        if(t){d.d.release(t);d.p.put(0x8a92d0u+k*4,0);}
    }
    d.p.call(PcParticleRenderGlobal,{0x89ede0u,0});d.p.call(PcParticleRenderGlobal,{0x89ede8u,0});
}
// 486450 tire_mark_disp.
void tire_mark_disp_486450(D& d){
    P& p=d.p;
    (void)p.u(0x95af0cu);                                 // 40ECB0 (result unused)
    for(std::uint32_t k=0;k<4;++k){
        d.d.set_texture_stage_state(k,1,1);d.d.set_texture_stage_state(k,2,0);d.d.set_texture_stage_state(k,3,2);
        d.d.set_texture_stage_state(k,4,1);d.d.set_texture_stage_state(k,5,0);d.d.set_texture_stage_state(k,6,2);
        d.d.set_sampler_state(k,5,2);d.d.set_sampler_state(k,6,2);d.d.set_sampler_state(k,7,0);d.d.set_sampler_state(k,1,3);d.d.set_sampler_state(k,2,3);
        d.d.set_texture_stage_state(k,0xb,k);d.d.set_texture_stage_state(k,0x18,0);
    }
    d.d.set_texture_stage_state(0,1,4);d.d.set_texture_stage_state(0,4,4);
    d.d.set_pixel_shader(0);d.d.set_fvf(0x142);d.d.set_vertex_shader(0);
    for(const auto& s:{std::array<std::uint32_t,2>{0x16,1},{0x1d,0},{0x18,0},{0x1b,1},{0x13,5},{0x14,6},{0x89,0},{0x07,1},{0x0e,0}})
        d.d.set_render_state(s[0],s[1]);
    driving::pc_matrix_push_unit(p.c.matrices);
    p.call(0x409df0u,{6});                                 // 409DF0(6) -> 410F90(current, 6)
    d.set_transform(0x100);                                // 409E00(6): D3DTS_WORLD
    for(std::uint32_t esi=0x64fe28u,n=0;n<3;++n,esi+=0x14){
        d.d.set_texture(0,p.u(esi+4));
        const std::uint32_t count=p.u(esi-4)*2u;
        d.draw(4,count,p.u(esi),0x18,count*3u);
    }
    p.pop();
}
// ---- draw functions --------------------------------------------------------
std::uint32_t draw_418cf0(D& d,std::uint32_t buffer,std::uint32_t src){
    P& p=d.p;
    const std::uint32_t tex=p.u(src+0x50);
    std::uint32_t count=0;
    const float scale=push_view(p,true)[0];
    d.d.set_vertex_shader(0);d.d.set_fvf(0x144);d.d.set_texture(0,tex);
    d.rs(0x16,1);d.rs(0x18,0x80);d.rs(0x1b,1);d.rs(0x13,p.u(src+0x70));d.rs(0x14,p.u(src+0x74));d.rs(0x07,1);d.rs(0x0e,1);d.rs(0xa8,7);
    std::uint32_t esi=0x8bc378u;
    for(std::uint32_t k=0,edi=buffer;k<p.u(src+4);++k,edi+=0x50){
        if(p.i(edi)>=0)continue;
        const auto v=vec4(p,p.f(edi+4),p.f(edi+8),p.f(edi+0xc),1.f);
        {const X w=X(v[3]);if(!(w>X(0.0f))&&!(c2(w,X(0.0f))))continue;}   // jnp: w <= 0 skips, NaN draws
        const float inv=st(X(1.0f)/X(v[3]));
        const float sx=st((p.x(0x740c8cu)*X(v[0]))*X(inv)+p.x(0x740c8cu));
        const float sy=st((-(p.x(0x740c90u)*X(v[1])))*X(inv)+p.x(0x740c90u));
        X s=(((p.x(edi+0x3c)*p.x(src+0x44))*p.x(0x740c8cu))*X(inv))*X(scale);
        if(!c0c3(s,p.x(0x74f7bcu)))s=(s-p.x(0x74f7bcu))*p.x(0x74f7c0u)+p.x(0x74f7bcu);
        const std::uint32_t colour=p.u(edi+0x38);
        const float z=st(X(inv)*X(v[2]));
        const std::uint32_t base=esi-0x80u;                // six vertices of 0x1C
        for(unsigned k2=0;k2<6;++k2){const std::uint32_t a=base+k2*0x1cu;p.put(a+0x10,colour);p.putf(a+8,z);p.put(a+0xc,0x3f800000u);}
        const float x0=st(X(sx)-s),y0=st(X(sy)-s),x1=st(s+X(sx)),y1=st(X(sy)+s);
        auto uv=[&](unsigned k2,float x,float y,std::uint32_t u,std::uint32_t vv){const std::uint32_t a=base+k2*0x1cu;p.putf(a,x);p.putf(a+4,y);p.put(a+0x14,u);p.put(a+0x18,vv);};
        const std::uint32_t u0=p.u(edi+0x40),v0=p.u(edi+0x44),u1=p.u(edi+0x48),v1=p.u(edi+0x4c);
        uv(0,x0,y0,u0,v0);uv(1,x0,y1,u1,v0);uv(2,x1,y0,u0,v1);uv(3,x1,y0,u0,v1);uv(4,x0,y1,u1,v0);uv(5,x1,y1,u1,v1);
        ++count;
        esi+=0xa8u;
        if(esi==0x8d1378u)break;
    }
    if(count)d.draw(4,count*2u,0x8bc2f8u,0x1c,count*6u);
    p.pop();
    d.rs(0xa8,0xf);d.rs(0x0f,1);d.d.set_texture(0,0);
    return count;
}
// 418590 / 418910: one screen quad per live particle, drawn alone from the
// four 0x1C vertices 74F708 (strip, z and colour per particle; the UV and
// RHW words are the EXE's). 418590: records of 0x20, size [src+44] and colour
// [src+4C]; 418910: records of 0x50 with their own size (+3C) and colour
// (+38), depth test on and depth writes off.
std::uint32_t draw_screen_quads(D& d,std::uint32_t buffer,std::uint32_t src,bool own){
    P& p=d.p;
    const std::uint32_t tex=p.u(src+0x50);
    std::uint32_t count=0;
    const float scale=push_view(p,true)[0];
    d.d.set_vertex_shader(0);d.d.set_fvf(0x144);d.d.set_texture(0,tex);
    d.rs(0x16,1);d.rs(0x18,0);d.rs(0x1b,1);d.rs(0x13,p.u(src+0x70));d.rs(0x14,p.u(src+0x74));
    if(own){d.rs(0x07,1);d.rs(0x0e,0);}
    for(std::uint32_t k=0,edi=buffer;k<p.u(src+4);++k,edi+=own?0x50u:0x20u){
        if(p.i(edi)>=0)continue;
        const auto v=vec4(p,p.f(edi+4),p.f(edi+8),p.f(edi+0xc),1.f);
        {const X w=X(v[3]);if(!(w>X(0.0f))&&!(c2(w,X(0.0f))))continue;}   // jnp: w <= 0 skips, NaN draws
        const float inv=st(X(1.0f)/X(v[3]));
        const float sx=st((p.x(0x740c8cu)*X(v[0]))*X(inv)+p.x(0x740c8cu));
        const float sy=st((-(p.x(0x740c90u)*X(v[1])))*X(inv)+p.x(0x740c90u));
        X s=own?(((p.x(edi+0x3c)*p.x(src+0x44))*p.x(0x740c8cu))*X(inv))*X(scale)
               :((p.x(0x740c8cu)*p.x(src+0x44))*X(inv))*X(scale);
        if(!c0c3(s,p.x(0x74f7bcu)))s=(s-p.x(0x74f7bcu))*p.x(0x74f7c0u)+p.x(0x74f7bcu);
        const std::uint32_t colour=own?p.u(edi+0x38):p.u(src+0x4c);
        const float z=st(X(inv)*X(v[2]));
        for(std::uint32_t a=0x74f708u;a<0x74f778u;a+=0x1cu){p.putf(a+8,z);p.put(a+0x10,colour);}
        p.putf(0x74f708u,st(X(sx)-s));p.putf(0x74f70cu,st(X(sy)-s));
        p.putf(0x74f724u,st(s+X(sx)));p.putf(0x74f728u,st(X(sy)-s));
        p.putf(0x74f740u,st(s+X(sx)));p.putf(0x74f744u,st(s+X(sy)));
        p.putf(0x74f75cu,st(X(sx)-s));p.putf(0x74f760u,st(s+X(sy)));
        d.draw(5,2,0x74f708u,0x1c,4);
        ++count;
    }
    p.pop();
    d.d.set_texture(0,0);
    return count;
}
// 41A520: one line per live particle (records of 0x50) from its position
// (+4) to position + 74F7C8 * velocity (+10), XYZ|DIFFUSE, in the colour of
// the first record; the current transform, no matrix push.
std::uint32_t draw_lines_41a520(D& d,std::uint32_t buffer,std::uint32_t src){
    P& p=d.p;
    const std::uint32_t colour=p.u(buffer+0x38),tex=p.u(src+0x50);
    d.d.set_vertex_shader(0);d.d.set_fvf(0x42);d.d.set_texture(0,tex);
    d.rs(0x16,1);d.rs(0x18,0);d.rs(0x1b,1);d.rs(0x13,5);d.rs(0x14,6);
    std::uint32_t count=0;
    std::array<std::uint8_t,0x20> v{};
    const std::uint32_t V=0x7fff0300u;
    const std::size_t mark=p.m.mark();p.m.map(V,v.data(),v.size());
    try{
        for(std::uint32_t k=0,esi=buffer;k<p.u(src+4);++k,esi+=0x50){
            if(p.i(esi)>=0)continue;
            const X t=p.x(0x74f7c8u);
            p.put(V,p.u(esi+4));p.put(V+4,p.u(esi+8));p.put(V+8,p.u(esi+0xc));p.put(V+0xc,colour);
            p.putf(V+0x10,st(t*p.x(esi+0x10)+p.x(esi+4)));
            p.putf(V+0x14,st(t*p.x(esi+0x14)+p.x(esi+8)));
            p.putf(V+0x18,st(t*p.x(esi+0x18)+p.x(esi+0xc)));
            p.put(V+0x1c,colour);
            d.draw(2,1,V,0x10,2);
            ++count;
        }
    }catch(...){p.m.release(mark);throw;}
    p.m.release(mark);
    return count;
}
std::uint32_t draw_419dc0(D& d,std::uint32_t buffer,std::uint32_t src){
    P& p=d.p;
    const bool shader=p.u(src+0x78)&&p.u(0x74f7b8u);
    if(shader)pixel_state_41b550(d);
    else{d.rs(0x13,p.u(src+0x70));d.rs(0x14,p.u(src+0x74));}
    const std::uint32_t tex=p.u(src+0x50);
    push_view(p,true);
    d.ss(0,5,2);d.ss(0,6,2);d.ss(0,7,0);d.ss(0,1,3);d.ss(0,2,3);
    d.tss(0,0xb,0);d.tss(0,0x18,0);d.rs(0x89,0);d.rs(0x1c,0);
    d.d.set_vertex_shader(0);d.d.set_fvf(0x142);d.d.set_texture(0,tex);
    d.rs(0x16,1);d.rs(0x18,0);d.rs(0x1b,1);d.rs(0xab,1);
    d.rs(0x07,p.u(src+0x7c)?1u:0u);d.rs(0x0e,0);
    std::uint32_t count=0;
    for(std::uint32_t k=0,list=0x8d1318u,a=buffer;k<p.u(src+4);++k,a+=0x5c)
        if(p.i(a)<0){++count;p.put(list,a);list+=0x14;}
    const std::uint32_t M=p.u(0x79f574u)+0x140u;               // 483EC0
    std::uint32_t n=count;
    if(std::int32_t(n)>=0x200){n=0x200;}
    for(std::uint32_t k=0,list=0x8d1318u,ecx=0x8a92f4u;k<n;++k,list+=0x14,ecx+=0x90){
        const std::uint32_t q=p.u(list);
        const X s=p.x(q+0x20)*p.x(src+0x44);
        const float a=st(p.x(M)*s),b=st(p.x(M+0x10)*s);
        const float c=st(p.x(M+0x20)*s),dd=st(p.x(M+4)*s),e=st(p.x(M+0x14)*s);
        const X f=s*p.x(M+0x24);
        auto vert=[&](std::uint32_t at,float x,float y,float z,std::uint32_t col,std::uint32_t u,std::uint32_t v){
            p.putf(at,x);p.putf(at+4,y);p.putf(at+8,z);p.put(at+0xc,col);p.put(at+0x10,u);p.put(at+0x14,v);};
        // vertex 0 (ecx-4)
        {const float xa=st(p.x(q+4)-X(a)),ya=st(p.x(q+8)-X(b));
         const X zc=p.x(q+0xc)-X(c);const X x1=X(xa)-X(dd),y1=X(ya)-X(e);
         const float z1=st(zc-f);
         vert(ecx-4,st(x1),st(y1),z1,p.u(q+0x24),p.u(q+0x34),p.u(q+0x38));}
        // vertices 1 and 4 (ecx+14, ecx+5C)
        {const float xa=st(p.x(q+4)-X(a)),ya=st(p.x(q+8)-X(b));
         const X zc=p.x(q+0xc)-X(c);const X x2=X(dd)+X(xa),y2=X(e)+X(ya);
         const float z2=st(f+zc);
         vert(ecx+0x14,st(x2),st(y2),z2,p.u(q+0x28),p.u(q+0x34),p.u(q+0x40));
         vert(ecx+0x5c,st(x2),st(y2),z2,p.u(q+0x28),p.u(q+0x34),p.u(q+0x40));}
        // vertices 2 and 3 (ecx+2C, ecx+44)
        {const float xa=st(X(a)+p.x(q+4)),ya=st(X(b)+p.x(q+8));
         const X zc=X(c)+p.x(q+0xc);const X x3=X(xa)-X(dd),y3=X(ya)-X(e);
         const float z3=st(zc-f);
         vert(ecx+0x2c,st(x3),st(y3),z3,p.u(q+0x2c),p.u(q+0x3c),p.u(q+0x38));
         vert(ecx+0x44,st(x3),st(y3),z3,p.u(q+0x2c),p.u(q+0x3c),p.u(q+0x38));}
        // vertex 5 (ecx+74)
        {const float xa=st(X(a)+p.x(q+4)),ya=st(X(b)+p.x(q+8));
         const float zc=st(X(c)+p.x(q+0xc));const X x5=X(dd)+X(xa),y5=X(e)+X(ya);
         const float z5=st(f+X(zc));
         vert(ecx+0x74,st(x5),st(y5),z5,p.u(q+0x30),p.u(q+0x3c),p.u(q+0x40));}
    }
    p.pop();
    if(n)d.draw(4,n*2u,0x8a92f0u,0x18,n*6u);
    d.d.set_texture(0,0);
    if(shader)d.d.set_pixel_shader(0);
    return n;
}
// Point sprites: 41A6F0 (type 5, 0x50-byte particles) and 41AE40 (type 6,
// 0x28-byte particles, one draw per texture frame).
std::uint32_t draw_41a6f0(D& d,std::uint32_t buffer,std::uint32_t src){
    P& p=d.p;
    const std::uint32_t colour=p.u(src+0x4c);
    std::uint32_t count=0;
    p.push_load(0x95d8a0u);
    const auto v=vec4(p,1.f,1.f,0.f,1.f);
    const float size=st(((p.x(src+0x48)/p.x(0x740c90u))*p.x(0x74f7ccu))*X(v[0]));
    p.pop();
    d.d.set_vertex_shader(0);d.d.set_fvf(0x42);d.d.set_texture(0,p.u(src+0x50));
    d.tss(0,1,4);d.tss(0,2,0);d.tss(0,3,2);d.tss(0,4,4);d.tss(0,5,2);d.tss(0,6,3);d.ss(0,1,3);d.ss(0,2,3);
    d.rs(0x9c,1);d.rs(0x9d,1);d.rs(0x9a,bf(size));d.rs(0x9b,p.u(0x74f7d0u));d.rs(0xa6,p.u(0x74f7d4u));
    d.rs(0x9e,0);d.rs(0x9f,0);d.rs(0xa0,0x3f800000u);d.rs(0x1d,0);d.rs(0x18,0);d.rs(0x1b,1);
    d.rs(0x13,p.u(src+0x70));d.rs(0x14,p.u(src+0x74));d.rs(0x07,1);d.rs(0x0e,0);d.rs(0xa8,7);
    std::uint32_t out=0x8d630cu;
    for(std::uint32_t k=0,a=buffer;k<p.u(src+4);++k,a+=0x50){
        if(p.i(a)>=0)continue;
        p.put(out-4,p.u(a+4));p.put(out,p.u(a+8));p.put(out+4,p.u(a+0xc));p.put(out+8,colour);
        ++count;out+=0x10;
        if(out>=0x8de30cu)break;
    }
    if(count)d.draw(1,count,0x8d6308u,0x10,count);
    d.d.set_texture(3,0);d.tss(3,1,1);
    d.rs(0x9c,0);d.rs(0x9d,0);d.rs(0x1b,0);d.rs(0x1d,1);d.rs(0xa8,0xf);
    return count;
}
std::uint32_t draw_41ae40(D& d,std::uint32_t buffer,std::uint32_t src){
    P& p=d.p;
    const std::uint32_t colour=p.u(src+0x4c);
    const float size=st(p.x(src+0x48)*X(fb(0x3b088889u)));
    std::uint32_t count=0;
    d.d.set_vertex_shader(0);d.d.set_fvf(0x42);
    d.tss(0,1,4);d.tss(0,2,0);d.tss(0,3,2);d.tss(0,4,4);d.tss(0,5,2);d.tss(0,6,3);d.ss(0,1,3);d.ss(0,2,3);
    d.rs(0x9c,1);d.rs(0x9d,1);d.rs(0x9a,bf(size));d.rs(0x9b,0x40000000u);d.rs(0xa6,0x42800000u);
    d.rs(0x9e,0);d.rs(0x9f,0);d.rs(0xa0,0x3f800000u);d.rs(0x1d,0);d.rs(0x18,0);d.rs(0x1b,1);
    d.rs(0x13,p.u(src+0x70));d.rs(0x14,p.u(src+0x74));d.rs(0x07,1);d.rs(0x0e,0);d.rs(0xa8,7);
    for(std::int32_t frame=0;frame<p.i(src+0x58);++frame){
        d.d.set_texture(0,p.u(p.u(src+0x54)+std::uint32_t(frame)*4u));
        count=0;
        std::uint32_t out=0x8d630cu;
        for(std::uint32_t k=0,a=buffer;k<p.u(src+4);++k,a+=0x28){
            if(std::int32_t(p.i16(a+0x26))!=frame||p.i(a)>=0)continue;
            p.put(out-4,p.u(a+4));p.put(out,p.u(a+8));p.put(out+4,p.u(a+0xc));p.put(out+8,colour);
            ++count;out+=0x10;
            if(out>=0x8de30cu)break;
        }
        if(count)d.draw(1,count,0x8d6308u,0x10,count);
    }
    d.d.set_texture(3,0);d.tss(3,1,1);
    d.rs(0x9c,0);d.rs(0x9d,0);d.rs(0x1b,0);d.rs(0x1d,1);d.rs(0xa8,0xf);
    return count;
}
// 418490 nlParticleDraw.
void particle_draw_418490(D& d){
    P& p=d.p;
    p.call(0x408880u,{});
    driving::pc_matrix_push_unit(p.c.matrices);
    p.call(0x411060u,{});                                        // 95D9E0 = current, then 411060(95D9E0)
    d.set_transform(0x100);
    for(std::uint32_t k=0,s=0x8a8d18u;k<11;++k,s+=0x84u){
        const std::uint32_t f=p.u(s);
        if(std::int32_t(f)>=0)continue;
        const std::uint32_t fn=p.u(0x74e99cu+(f&7u)*4u);
        p.put(0x95af4cu,fn);
        if(!fn)continue;
        std::uint32_t r;
        g_pc_gpu_zone=fn==0x418cf0u?2u:fn==0x419210u?3u:fn==0x41a6f0u?4u:fn==0x41ae40u?5u:6u;
        struct ZoneEnd{~ZoneEnd(){g_pc_gpu_zone=0;}} zone_end;
        switch(fn){
        case 0x418cf0u:r=draw_418cf0(d,p.u(s+8),s);break;
        case 0x419210u:
            if(p.u(0x74f7c4u)<2u)throw std::logic_error("particles: 419210 with [74F7C4] < 2 (no retail code writes it)");
            r=draw_419dc0(d,p.u(s+8),s);
            break;
        case 0x41a6f0u:r=draw_41a6f0(d,p.u(s+8),s);break;
        case 0x41ae40u:r=draw_41ae40(d,p.u(s+8),s);break;
        case 0x418590u:r=draw_screen_quads(d,p.u(s+8),s,false);break;
        case 0x418910u:r=draw_screen_quads(d,p.u(s+8),s,true);break;
        case 0x41a520u:r=draw_lines_41a520(d,p.u(s+8),s);break;
        default:
            p.call(fn,{p.u(s+8),s});
            throw std::logic_error("particles: draw function outside the 74E99C table");
        }
        p.put(0x8a8cecu+k*4u,r);
    }
    p.pop();
    p.call(0x408880u,{});
}
}
// 41BD10 ParticleEfc_Disp.
void particles_display_41bd10(PcParticleContext& pc){
    P p(pc);
    if(!p.u(0x74fe7cu))return;
    if(!pc.device)throw std::logic_error("particles: display without device");
    D d(p,*pc.device);
    save_41b5c0(d);
    g_pc_gpu_zone=1;
    tire_mark_disp_486450(d);
    g_pc_gpu_zone=0;
    restore_41b7a0(d);
    save_41b5c0(d);
    particle_draw_418490(d);
    restore_41b7a0(d);
}
}

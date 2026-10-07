#include "platform/pc_headlight_pool.hpp"
#include "driving/pc_d3dx.hpp"
#include "driving/pc_x87.hpp"
#include <cstdlib>
#include <cstring>

namespace outrun::platform {
namespace {
using driving::X87;
using driving::x87_float;
float fb(std::uint32_t u){float f;std::memcpy(&f,&u,4);return f;}
// .data constants (no writer in the EXE).
const float K746b0c=fb(0x00000000u),K746b10=fb(0x3f99999au),K746b14=fb(0x40833333u),
            K746b18=fb(0x3e810625u),K746b1c=fb(0x3e779a6bu),K746b20=fb(0x3ebae148u),
            K62813c=fb(0x3dcccccdu),K6280c4=fb(0xbf800000u),
            K628084=fb(0x3f59999au),K628080=fb(0x3f2aaaaau),K62807c=fb(0x3eaaaaaau),K628078=fb(0xb3000000u);
void put(std::uint8_t* p,std::size_t off,float v){std::memcpy(p+off,&v,4);}
void putu(std::uint8_t* p,std::size_t off,std::uint32_t v){std::memcpy(p+off,&v,4);}
// 417550: eight vertices from the six words at p (a1, a0, z1, z0, x1, x0):
// u = 1, 5/6, 2/3, 1/2 across the strip, x scaled by 1, 2/3, 1/3, ~0.
std::uint8_t* strip_417550(std::uint8_t* out,const float p[6]){
    const float scale[4]{1.0f,K628080,K62807c,K628078};
    const std::uint32_t u[4]{0x3f800000u,0x3f555555u,0x3f2aaaaau,0x3f000000u};
    for(unsigned k=0;k<4;++k){
        for(unsigned side=0;side<2;++side){                  // side 0: (a1, z1, x1), side 1: (a0, z0, x0)
            std::uint8_t* v=out+(k*2+side)*PcHeadlightStride;
            const float x=p[4+side];
            put(v,0x0,k==0?x:x87_float(X87(x)*X87(scale[k])));   // the first pair is moved, not multiplied
            putu(v,0x4,0u);
            put(v,0x8,-p[2+side]);                                // FCHS: exact
            putu(v,0xc,0x80808080u);
            putu(v,0x10,u[k]);
            put(v,0x14,x87_float(X87(p[side])*X87(K628084)));
        }
    }
    return out+8*PcHeadlightStride;
}
}
std::vector<std::uint8_t> pc_headlight_vertices_4171f0(){
    std::vector<std::uint8_t> vb(PcHeadlightVertices*PcHeadlightStride,0);
    // Locals: V2 = (-tan(746B20), 0, 0), V1 = (0, -tan(746B1C), 0), V3 = (0, 0, 1).
    driving::PcVec3 v2{x87_float(-driving::x87_tan(X87(K746b20))),0.0f,0.0f};
    driving::PcVec3 v1{0.0f,x87_float(-driving::x87_tan(X87(K746b1c))),0.0f};
    driving::PcVec3 v3{0.0f,0.0f,1.0f};
    // 409F30 identity, D3DXMatrixRotationX(-746B18), current = R * current
    // (= R: identity products and zero sums are exact), three
    // D3DXVec3TransformNormal in place, 40A010.
    const float angle=-K746b18;
    const float s=x87_float(driving::x87_sin(X87(angle))),c=x87_float(driving::x87_cos(X87(angle)));
    const driving::PcMatrix16 r{1,0,0,0, 0,c,s,0, 0,-s,c,0, 0,0,0,1};
    const auto m=driving::pc_d3dx_matrix_multiply(r,driving::PcMatrix16{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1});
    v2=driving::pc_d3dx_vec3_transform_normal(v2,m);
    v1=driving::pc_d3dx_vec3_transform_normal(v1,m);
    v3=driving::pc_d3dx_vec3_transform_normal(v3,m);
    std::uint8_t* out=vb.data();
    for(int i=0;i<10;++i){
        const X87 a1=X87(i+1)*X87(K62813c),a0=X87(i)*X87(K62813c);   // FILD exact, FMUL rounded
        // Points t*V1 + V2 + V3 (x, y stored as floats, z kept in a register).
        auto point=[&](X87 t,float& x,float& y,X87& z){
            const float tz=x87_float(t*X87(v1[2]));
            const float tx=x87_float(t*X87(v1[0])+X87(v2[0]));
            const X87 ty=t*X87(v1[1])+X87(v2[1]);
            const X87 zz=X87(tz)+X87(v2[2]);
            x=x87_float(X87(tx)+X87(v3[0]));
            y=x87_float(ty+X87(v3[1]));
            z=zz+X87(v3[2]);
        };
        float px,py,qx,qy;X87 pz,qz;
        point(a1,px,py,pz);point(a0,qx,qy,qz);
        // Projection: k = 746B10 / y; (x*k - 746B0C, y*k - 746B10, z*k - 746B14).
        const X87 k1=X87(K746b10)/X87(py);
        const X87 xk1=X87(px)*k1;
        const float b1=x87_float(k1*X87(py));
        const float c1=x87_float(pz*k1);
        const float r0=x87_float(xk1-X87(K746b0c));
        (void)x87_float(X87(b1)-X87(K746b10));                    // y, stored but not used
        const float r2=x87_float(X87(c1)-X87(K746b14));
        const X87 k2=X87(K746b10)/X87(qy);
        const float d0=x87_float(X87(qx)*k2);
        (void)x87_float(X87(qy)*k2);
        const X87 qzk=qz*k2;
        const float s0=x87_float(X87(d0)-X87(K746b0c));
        const float s2=x87_float(qzk-X87(K746b14));
        const float a1f=x87_float(a1),a0f=x87_float(a0);
        const float p[6]{a1f,a0f,r2,s2,r0,s0};
        out=strip_417550(out,p);
        const float mirror[6]{a1f,a0f,r2,s2,x87_float(X87(r0)*X87(K6280c4)),x87_float(X87(s0)*X87(K6280c4))};
        out=strip_417550(out,mirror);
    }
    return vb;
}
bool pc_headlight_display_4695c0(driving::Bytes car,PcFlushContext& flush,PcD3D9Device& d,
    driving::PcMatrixStack& matrices,const std::vector<std::uint8_t>& vertices,std::uint32_t texture){
    static const bool force=std::getenv("OR2_HEADLIGHT_FORCE")!=nullptr;   // host check: draw regardless of the car state
    if(car.size()<0x2cc||(!force&&car.f32(0x2c8)>0.0f))return false;          // COMISS / JA: NaN draws too
    const auto flags=car.u32(4);
    if(!force&&(!(flags&0x100u)||!(flags&0x800u)))return false;
    if(!texture||vertices.size()<PcHeadlightVertices*PcHeadlightStride)return false;
    driving::pc_matrix_push_load(matrices,car.sub(0xb0,64));
    // 416AA0: world = current (95D9E0, 411060), SetTransform(WORLD).
    std::array<float,16> world{};
    {const auto cur=matrices.current();for(unsigned k=0;k<16;++k)world[k]=cur.f32(k*4u);}
    render_set_matrix_410f90(flush,world,6);
    d.set_transform(0x100,world.data());
    d.set_texture(0,texture);d.set_texture(1,0);d.set_texture(2,0);d.set_texture(3,0);
    static constexpr std::uint32_t States[9]{7,0xe,0x18,0x19,0x1b,0x14,0x13,0xab,0x16};
    static constexpr std::uint32_t Values[9]{0,0,1,7,1,2,5,1,1};
    std::uint32_t saved[9];
    for(unsigned k=0;k<9;++k)saved[k]=d.get_render_state(States[k]);
    for(unsigned k=0;k<9;++k)if(d.get_render_state(States[k])!=Values[k])d.set_render_state(States[k],Values[k]);
    // Sampler 0 (vtable +110/+114): ADDRESSU mirror (written twice: the PC
    // repeats type 1), MIN/MAG/MIP filters linear.
    auto samp=[&](std::uint32_t type,std::uint32_t v){if(d.get_sampler_state(0,type)!=v)d.set_sampler_state(0,type,v);};
    samp(1,2);samp(1,2);samp(6,2);samp(5,2);samp(7,2);
    // Stage 0 (+108/+10C): colour = texture * diffuse, alpha = texture * diffuse.
    auto tss=[&](std::uint32_t type,std::uint32_t v){if(d.get_texture_stage_state(0,type)!=v)d.set_texture_stage_state(0,type,v);};
    tss(2,2);tss(3,0);tss(1,4);tss(5,2);tss(6,0);tss(4,4);
    d.set_fvf(0x142);d.set_vertex_shader(0);
    // SetStreamSource(8A8C84) + DrawPrimitive(TRIANGLESTRIP, i, 6) for i = 0, 8 .. 152:
    // the buffer is static, the same vertices go through the UP path.
    for(std::size_t first=0;first<PcHeadlightVertices;first+=8)
        d.draw_primitive_up(5,6,vertices.data()+first*PcHeadlightStride,PcHeadlightStride);
    for(unsigned k=0;k<9;++k)if(d.get_render_state(States[k])!=saved[k])d.set_render_state(States[k],saved[k]);
    d.set_texture(0,0);
    driving::pc_matrix_pop(matrices);
    return true;
}
}

#include "platform/rob_flag.hpp"
#include "platform/rob_osage.hpp"
#include "platform/pc_pmt_loader.hpp"
#include "platform/race_particles.hpp"
#include "driving/pc_d3dx.hpp"
#include "driving/pc_matrix_stack.hpp"
#include "driving/pc_x87.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <stdexcept>
#include "driving/service_hole.hpp"
namespace outrun::platform {
namespace {
using driving::X87;
using driving::x87_float;
float fbits(std::uint32_t u){float f;std::memcpy(&f,&u,4);return f;}
constexpr std::uint32_t NodeSize=0x5cu,VertexSize=0x28u;
// EXE .data (never written): 73772C/737738 rest points, 737720/737724 1.0,
// 73771C 0.0027222 (gravity), 628064 0.5.
constexpr std::uint32_t A73772c[3]={0x3e158106u,0x3e3851ecu,0x3be56042u},B737738[3]={0x3e333333u,0x3f2e147bu,0x3c75c28fu};
const float One737720=1.0f,One737724=1.0f,Gravity73771c=0.0027222000062465668f,Half628064=0.5f;
#define OR2_PC_RECORD struct __attribute__((may_alias))
// Cloth node (0x5C, 95B27C + (row*5 + column)*0x5C).
OR2_PC_RECORD ClothNode {
    std::uint32_t edges;         // +00 1 up, 2 down, 4 left, 8 right neighbours
    float pos[3];                // +04
    float unknown10[3];
    float velocity[3];           // +1C
    float rest[3];               // +28 row 0: the hold point in the robot's hand space
    float prev[3];               // +34 position of the previous frame
    float wind[3];               // +40 wind direction (hand space)
    float length_up,length_down,length_left,length_right;   // +4C..+58 rest lengths
};
// Strip vertex (0x28, FVF 1D2: position, normal, diffuse, uv).
OR2_PC_RECORD ClothVertex {
    float pos[3];                // +00
    float normal[3];             // +0C
    std::uint32_t colour;        // +18
    std::uint32_t unknown1c;
    float uv[2];                 // +20
};
static_assert(sizeof(ClothNode)==NodeSize&&sizeof(ClothVertex)==VertexSize,"flag cloth record layouts");
// 401790 on node positions: P pulled back to `length` from Q.
void limit_401790(float* a,const float* b,float length){
    const X87 dx=X87(a[0])-X87(b[0]);
    const float dy=x87_float(X87(a[1])-X87(b[1]));                       // spilled to the argument slot
    const X87 dz=X87(a[2])-X87(b[2]);
    const X87 d2=(dz*dz+X87(dy)*X87(dy))+dx*dx;
    const X87 l2=X87(length)*X87(length);
    if(!(d2>l2))return;
    const X87 k=X87(length)/driving::x87_sqrt(d2);
    a[0]=x87_float(k*dx+X87(b[0]));
    a[1]=x87_float(X87(dy)*k+X87(b[1]));
    a[2]=x87_float(k*dz+X87(b[2]));
}
// 401720(P, Q, L, length): pull of P by the neighbour Q (d = Q - L, L the
// start position), then 401790(P, Q, length).
void spring_401720(float* p,const float* q,const float* l,float length){
    const X87 dx=X87(q[0])-X87(l[0]),dy=X87(q[1])-X87(l[1]);
    const float dz=x87_float(X87(q[2])-X87(l[2]));
    const float k=x87_float(-((X87(Gravity73771c)/X87(length))*dy));
    const float kx=x87_float(dx*X87(k));
    const X87 ky=dy*X87(k),kz=X87(dz)*X87(k);
    p[0]=x87_float(X87(kx)+X87(p[0]));
    p[1]=x87_float(ky+X87(p[1]));
    p[2]=x87_float(kz+X87(p[2]));
    limit_401790(p,q,length);
}
// 40F2C0(p0, p1, p2, out): normalised (p0 - p1) x (p2 - p0); left unnormalised
// when the cross product is 0.
void normal_40f2c0(const float* p0,const float* p1,const float* p2,float* out){
    const X87 ax=X87(p0[0])-X87(p1[0]),ay=X87(p0[1])-X87(p1[1]),az=X87(p0[2])-X87(p1[2]);
    const float bx=x87_float(X87(p2[0])-X87(p0[0])),by=x87_float(X87(p2[1])-X87(p0[1]));
    const X87 bz=X87(p2[2])-X87(p0[2]);
    out[0]=x87_float(bz*ay-X87(by)*az);
    out[1]=x87_float(X87(bx)*az-bz*ax);
    out[2]=x87_float(X87(by)*ax-X87(bx)*ay);
    const float x=out[0],y=out[1],z=out[2];
    const X87 l2=(X87(x)*X87(x)+X87(y)*X87(y))+X87(z)*X87(z);
    if(l2==X87(0.0f))return;                                   // FUCOMPP / TEST AH,44h / JP (unordered: normalised)
    const X87 k=X87(1.0f)/driving::x87_sqrt(l2);
    out[0]=x87_float(k*X87(x));out[1]=x87_float(k*X87(y));out[2]=x87_float(k*X87(z));
}
std::array<float,16> current(PcRaceContext& c){std::array<float,16> a{};auto cur=c.matrices.current();for(unsigned k=0;k<16;++k)a[k]=cur.f32(k*4u);return a;}
}
void rob_flag_cloth_401080(PcRaceMemory& m,std::uint32_t cloth,std::uint32_t cols,std::uint32_t rows,std::uint32_t a,std::uint32_t b){
    const float ax=m.f32(a),ay=m.f32(a+4),az=m.f32(a+8);
    const float dx=x87_float(X87(m.f32(b))-X87(ax));                    // FST [2C]
    const X87 dy=X87(m.f32(b+4))-X87(ay);
    const X87 dze=X87(m.f32(b+8))-X87(az);
    const float dz=x87_float(dze);                                       // FST [1C]
    const X87 len=driving::x87_sqrt((dze*dze+X87(dx)*X87(dx))+dy*dy);
    const float seg=x87_float(len/X87(std::int32_t(cols)-1));
    // Direction scaled to the segment length (unless shorter than 1e-4).
    float sx=dx;X87 sy=dy,sz=X87(dz);
    {
        const X87 l2=driving::x87_sqrt((X87(dz)*X87(dz)+dy*dy)+X87(dx)*X87(dx));
        if(l2>X87(0.0001)){
            const X87 k=X87(seg)/l2;
            sx=x87_float(X87(dx)*k);
            sy=dy*k;sz=X87(dz)*k;
        }
    }
    m.put32(cloth,m.u32(cloth)|3u);
    m.put32(cloth+4,cols);m.put32(cloth+8,rows);
    // Node template (0x5C, the PC stack block copied by REP MOVSD): +34..+3C
    // are stack contents on the PC (0 here), overwritten before use.
    for(std::uint32_t r=0;std::int32_t(r)<std::int32_t(rows);++r){
        const std::uint32_t row=cloth+0xc+r*0x1ccu;                        // 5 * 0x5C (hard-coded)
        for(std::uint32_t c=0;std::int32_t(c)<std::int32_t(cols);++c){
            const std::uint32_t n=row+c*NodeSize;
            std::uint32_t f=r?1u:0u;
            if(r!=rows-1u)f|=2u;
            if(c)f|=4u;
            if(c!=cols-1u)f|=8u;
            std::array<std::uint32_t,23> t{};
            t[0]=f;
            t[1]=m.u32(a);t[2]=m.u32(a+4);t[3]=m.u32(a+8);
            t[4]=0;t[5]=0;t[6]=0x3f800000u;t[7]=0;t[8]=0;t[9]=0;
            t[10]=m.u32(a);t[11]=m.u32(a+4);t[12]=m.u32(a+8);
            t[16]=0;t[17]=0;t[18]=0;
            std::uint32_t sb;std::memcpy(&sb,&seg,4);t[19]=sb;t[20]=sb;t[21]=sb;t[22]=sb;
            for(unsigned k=0;k<23;++k)m.put32(n+k*4u,t[k]);
        }
    }
    // Row 0 positions: A + k * step (float accumulation, the y/z steps extended).
    float px=ax,py=ay,pz=az;
    for(std::uint32_t c=0;std::int32_t(c)<std::int32_t(cols);++c){
        const std::uint32_t n=cloth+0xc+c*NodeSize;
        m.putf(n+4,px);m.putf(n+8,py);m.putf(n+0xc,pz);
        m.putf(n+0x28,px);m.putf(n+0x2c,py);m.putf(n+0x30,pz);
        px=x87_float(X87(px)+X87(sx));py=x87_float(X87(py)+sy);pz=x87_float(X87(pz)+sz);
    }
}
void rob_flag_mesh_401830(PcRaceMemory& m,std::uint32_t idx,std::uint32_t verts,std::uint32_t cols,std::uint32_t rows){
    std::uint32_t e=0;
    auto put=[&](std::uint32_t v){m.put16(idx+e*2u,std::uint16_t(v));++e;};
    for(std::int32_t r=0;r<std::int32_t(rows)-1;++r){
        const std::uint32_t top=std::uint32_t(r)*cols,bottom=std::uint32_t(r+1)*cols;
        put(top);put(bottom);
        std::uint32_t c=0;
        for(;std::int32_t(c)<std::int32_t(cols)-1;++c){put(c+top+1u);put(c+bottom+1u);}
        put(c+bottom);put(bottom);
    }
    const X87 u0=(X87(std::int32_t(cols))-X87(1.0f))*X87(Half628064);
    for(std::int32_t r=0;r<std::int32_t(rows);++r){
        const float y=x87_float(X87(r)-(X87(std::int32_t(rows))-X87(1.0f))*X87(Half628064));
        const float span=float(std::int32_t(cols)-1);
        const X87 v=X87(r)/X87(std::int32_t(rows)-1);
        for(std::int32_t c=0;c<std::int32_t(cols);++c){
            const std::uint32_t p=verts+(std::uint32_t(r)*cols+std::uint32_t(c))*VertexSize;
            m.put32(p+4,0);m.putf(p+8,y);m.put32(p+0xc,0);m.putf(p+0x10,1.0f);m.put32(p+0x14,0);
            m.putf(p,x87_float(X87(c)-u0));
            m.put32(p+0x18,0xffffffffu);m.put32(p+0x1c,0);
            m.putf(p+0x24,x87_float(X87(c)/X87(span)));
            m.putf(p+0x20,x87_float(v));
        }
    }
}
void rob_flag_normals_401990(PcRaceMemory& m,std::uint32_t verts,std::uint32_t nodes,std::uint32_t rows,std::uint32_t cols){
    const std::size_t count=std::size_t(rows)*cols;
    auto* v=reinterpret_cast<ClothVertex*>(m.at(verts,count*sizeof(ClothVertex),true));
    const auto* n=reinterpret_cast<const ClothNode*>(m.at(nodes,count*sizeof(ClothNode)));
    for(std::size_t i=0;i<count;++i)std::memcpy(v[i].pos,n[i].pos,12);
    // Each normal from its right and lower neighbours; the last column and row copy theirs.
    for(std::uint32_t r=0;r+1u<rows;++r){
        ClothVertex* row=v+std::size_t(r)*cols;
        for(std::uint32_t c=0;c+1u<cols;++c)normal_40f2c0(row[c].pos,row[c+1].pos,row[c+cols].pos,row[c].normal);
        std::memcpy(row[cols-1].normal,row[cols-2].normal,12);
    }
    ClothVertex* last=v+std::size_t(rows-1u)*cols;
    for(std::uint32_t c=0;c+1u<cols;++c)std::memcpy(last[c].normal,(last-cols)[c].normal,12);
    std::memcpy(last[cols-1].normal,last[cols-2].normal,12);
}
void rob_flag_init_402100(PcRaceContext& c){
    auto& m=c.m;
    const std::uint32_t cl=PcRobFlagState::ClothBase;
    m.put32(cl,m.u32(cl)&~1u);
    std::array<std::uint8_t,24> ab{};
    for(unsigned k=0;k<3;++k){std::memcpy(ab.data()+k*4,&A73772c[k],4);std::memcpy(ab.data()+12+k*4,&B737738[k],4);}
    const std::uint32_t Local=0x7ffd0000u;
    const std::size_t mark=m.mark();m.map_const(Local,ab.data(),ab.size());
    try{rob_flag_cloth_401080(m,cl,5,7,Local,Local+12);}catch(...){m.release(mark);throw;}
    m.release(mark);
    rob_flag_mesh_401830(m,PcRobFlagState::Indices,PcRobFlagState::Vertices,5,7);
}
void rob_flag_ctrl_402140(PcRaceContext& c){
    auto& m=c.m;
    std::uint8_t* cloth=m.at(PcRobFlagState::ClothBase,PcRobFlagState::ClothEnd-PcRobFlagState::ClothBase,true);
    std::uint32_t cols,rows;std::memcpy(&cols,cloth+4,4);std::memcpy(&rows,cloth+8,4);
    float scale;std::memcpy(&scale,cloth+0xcac,4);                      // wind scale
    ClothNode* n=reinterpret_cast<ClothNode*>(cloth+0xc);
    const float* wind=reinterpret_cast<const float*>(m.at(0x95aeb8u,12));   // never written (0)
    const float bias=m.f32(0x95aeacu);                                    // never written (0)
    // 401490: row 0 from the rest positions through the current matrix.
    const auto mx=current(c);
    for(std::int32_t k=0;k<std::int32_t(cols);++k,++n){
        const auto p=driving::pc_d3dx_vec3_transform_coord({n->rest[0],n->rest[1],n->rest[2]},mx);
        n->pos[0]=p[0];n->pos[1]=p[1];n->pos[2]=p[2];
    }
    for(std::int32_t r=1;r<std::int32_t(rows);++r){
        for(std::int32_t k=0;k<std::int32_t(cols);++k,++n){
            std::memcpy(n->prev,n->pos,12);
            // 401530 / 4015E0: the wind pushed through the hand matrix, the velocity, the wind vector.
            const float ox=n->pos[0],oy=n->pos[1],oz=n->pos[2];
            const auto t=driving::pc_d3dx_vec3_transform_normal({n->wind[0],n->wind[1],n->wind[2]},mx);
            const X87 tx=X87(t[0])*X87(scale),ty=X87(t[1])*X87(scale);
            const float tz=x87_float(X87(t[2])*X87(scale));
            const float px1=x87_float(tx+X87(ox));
            const X87 py1=ty+X87(oy),pz1=X87(tz)+X87(oz);
            float P[3]={x87_float(X87(px1)+X87(n->velocity[0])),x87_float(py1+X87(n->velocity[1])),x87_float(pz1+X87(n->velocity[2]))};
            P[0]=x87_float(X87(P[0])+X87(wind[0]));
            P[1]=x87_float(X87(wind[1])+X87(P[1]));
            P[2]=x87_float(X87(wind[2])+X87(P[2]));
            const float L[3]={ox,oy,oz};
            const std::uint32_t f=n->edges&0xffu;
            if(f&2u)spring_401720(P,n[cols].pos,L,n->length_down);
            if(f&4u)spring_401720(P,n[-1].pos,L,x87_float(X87(bias)+X87(n->length_left)));
            if(f&8u)spring_401720(P,n[1].pos,L,x87_float(X87(bias)+X87(n->length_right)));
            spring_401720(P,(n-cols)->pos,L,n->length_up);
            // 401530: velocity = (P - old) * 737724, y minus 737720 * gravity.
            n->velocity[0]=x87_float((X87(P[0])-X87(ox))*X87(One737724));
            n->pos[0]=P[0];n->pos[1]=P[1];n->pos[2]=P[2];
            n->velocity[1]=x87_float((X87(P[1])-X87(oy))*X87(One737724)-X87(One737720)*X87(Gravity73771c));
            n->velocity[2]=x87_float((X87(P[2])-X87(oz))*X87(One737724));
        }
    }
    rob_flag_normals_401990(m,PcRobFlagState::Vertices,PcRobFlagState::ClothBase+0xc,7,5);
}
std::uint32_t rob_flag_texture_401b80(PcPmtResources& r){
    const auto v=r.view();
    if(!(1u<v.u32(8)))return 0u;
    return v.u32(v.u32(r.texture_table_24)+4u);
}
void rob_flag_draw_401b80(PcRobFlagDraw& f,std::uint32_t vertices,std::uint32_t indices,std::uint32_t cols,std::uint32_t rows){
    auto& d=f.d;
    auto rs=[&](std::uint32_t s,std::uint32_t v){if(d.get_render_state(s)!=v)d.set_render_state(s,v);};
    auto ss=[&](std::uint32_t s,std::uint32_t t,std::uint32_t v){if(d.get_sampler_state(s,t)!=v)d.set_sampler_state(s,t,v);};
    auto tss=[&](std::uint32_t s,std::uint32_t t,std::uint32_t v){if(d.get_texture_stage_state(s,t)!=v)d.set_texture_stage_state(s,t,v);};
    d.set_texture(0,f.texture);
    // Material: diffuse = ambient = emissive = [737728] (0xFFC0C0C0) / 255
    // (628BC), specular 0, power 1.
    const std::uint32_t colour=0xffc0c0c0u;
    const float k=0.003921568859368563f;
    const float r=float((colour>>16)&0xffu)*k,g=float((colour>>8)&0xffu)*k,b=float(colour&0xffu)*k,a=float(colour>>24)*k;
    const float material[17]={r,g,b,a, r,g,b,a, 0,0,0,0, r,g,b,a, 1.f};
    d.set_material(material);
    // 41B5C0.
    for(const auto& s:ParticleSavedStates41b5c0)f.save.put32(s[1],d.get_render_state(s[0]));
    for(std::uint32_t t=0;t<4;++t)f.save.put32(0x8a92d0u+t*4,d.get_texture(t));
    ss(0,5,2);ss(0,6,2);ss(0,7,0);ss(0,1,3);ss(0,2,3);
    tss(0,0x18,0);tss(0,0xb,0);
    rs(0x1b,0);rs(0xab,1);rs(0x13,2);rs(0x14,1);rs(0x07,1);rs(0x0e,1);rs(0x0f,0);
    d.set_pixel_shader(0);d.set_vertex_shader(0);d.set_fvf(0x1d2u);
    rs(0x16,1);rs(0x89,1);rs(0x1c,0);rs(0x1d,1);
    const std::uint32_t n=(cols+1u)*(rows-1u)*2u;
    const std::uint32_t count=n-4u,used=n-2u;
    const std::uint32_t index_count=count+2u;
    std::uint32_t top=0;
    const std::uint8_t* ip=f.mesh.at(indices,std::size_t(index_count)*2u);
    for(std::uint32_t i=0;i<index_count;++i){std::uint16_t x;std::memcpy(&x,ip+i*2u,2);top=std::max<std::uint32_t>(top,x);}
    const std::uint8_t* vp=f.mesh.at(vertices,std::size_t(top+1u)*0x28u);
    d.draw_indexed_primitive_up(5,0,used,count,ip,0x65u,vp,0x28u);
    // 41B7A0.
    for(const auto& s:ParticleSavedStates41b5c0)rs(s[0],f.save.u32(s[1]));
    for(std::uint32_t t=0;t<4;++t){
        const std::uint32_t tex=f.save.u32(0x8a92d0u+t*4);
        d.set_texture(t,tex);
        if(tex){d.release(tex);f.save.put32(0x8a92d0u+t*4,0);}
    }
    if(f.render_global){f.render_global(0x89ede0u,0);f.render_global(0x89ede8u,0);}else outrun::driving::service_hole("rob_flag_draw_401b80","f.render_global");
}
void rob_flag_dest_4021a0(PcRaceMemory& m){m.put32(PcRobFlagState::ClothBase,m.u32(PcRobFlagState::ClothBase)&~1u);}
}

#include "platform/pc_shadow_volume.hpp"
#include "platform/pc_render_flush.hpp"
#include "platform/embedded_vehicle_draw_data.hpp"
#include "driving/pc_d3dx.hpp"
#include "driving/pc_x87.hpp"
#include <cstring>
#include <stdexcept>
namespace outrun::platform {
namespace {
using driving::X87;
using driving::x87_float;
constexpr std::uint32_t EdgeSize=0x24u;
// 5B2F68[model] -> 0x128-byte layout (5B0CB8..): +08 / +10 shadow model IDs.
std::uint32_t layout_word(std::int32_t model,std::uint32_t offset){
    if(model<0||model>=30)throw std::out_of_range("46BA20 model outside 5B2F68's thirty layouts");
    std::uint32_t v;std::memcpy(&v,EmbeddedVehicleLayouts+std::size_t(model)*0x128u+offset,4);return v;
}
// 77FF30..77FF8F (.data, never written): the six darkening vertices (x, y, z, rhw).
constexpr std::uint32_t Quad77ff30[6][4]={{0,0,0,0x3f800000u},{0x44200000u,0,0,0x3f800000u},{0,0x43dc0000u,0,0x3f800000u},
    {0x44200000u,0x43dc0000u,0,0x3f800000u},{0,0x43f00000u,0,0x3f800000u},{0x44200000u,0x43f00000u,0,0x3f800000u}};
// "fcomp; fnstsw; test ah,1": C0 (below or unordered).
bool below_zero(X87 v){return !(v>=X87(0.f));}
struct Model {
    PcShadowServices& s;
    std::uint32_t p(std::uint32_t field)const{return s.m.u32(field)+s.reloc(field);}
    std::uint32_t u(std::uint32_t a)const{return s.m.u32(a);}
};
// 421680(edges, V, i0, i1, normal) with EAX = count, EDX = first edge of the group.
std::uint32_t add_edge_421680(PcRaceMemory& m,std::uint32_t count,std::uint32_t start,std::uint32_t edges,const std::uint8_t* v,
    std::uint32_t i0,std::uint32_t i1,const float n[3]){
    auto same=[&](std::uint32_t a,std::uint32_t b){return std::memcmp(v+std::size_t(a)*4u,v+std::size_t(b)*4u,12)==0;};
    auto put_normal=[&](std::uint32_t at){for(unsigned k=0;k<3;++k)m.putf(at+k*4,n[k]);};
    for(std::uint32_t e=start;e<count;++e){
        const std::uint32_t r=edges+e*EdgeSize;
        if(!same(m.u32(r),i1))continue;
        if(!same(m.u32(r+4),i0))continue;
        const std::int8_t faces=std::int8_t(m.u8(r+0x20));
        if(std::int32_t(faces)+1>1)break;                       // a third face: new edge
        m.put8(r+0x20,std::uint8_t(faces+1));
        put_normal(r+0x14);return count;
    }
    const std::uint32_t r=edges+count*EdgeSize;
    m.put32(r,i0);m.put8(r+0x20,0);m.put32(r+4,i1);put_normal(r+8);
    return count+1u;
}
}
std::uint32_t shadow_build_420e50(PcShadowServices& s,std::uint32_t id,std::uint32_t object,std::uint32_t kind){
    auto& m=s.m;const Model mo{s};
    std::uint32_t o=0;
    if(id!=0xffffffffu){
        std::uint32_t count=0,table=0;
        if(s.bank(id>>16,count,table)&&(id&0xffffu)<count)o=table+(id&0xffffu)*0x3cu;
    }
    m.put32(object+0x1c,o);
    if(!o)return 1;
    // Sizes from the first draw of each primitive record of the object's group.
    const std::uint32_t n=m.u32(mo.p(o+0x1c)+0x28);
    const std::uint32_t entry=mo.p(o+0x20)+n*8u;
    const std::uint32_t first=m.u32(entry);
    const std::int32_t prims=m.i32(entry+4);
    std::uint32_t edges=0,vertices=0;
    for(std::int32_t k=0;k<prims;++k){
        const std::uint32_t prim=mo.p(o+0x24)+(first+std::uint32_t(k))*0x14u;
        const std::uint32_t draw=mo.p(o+0x28)+m.u32(prim+4)*0x20u;
        std::uint32_t range=mo.p(o+0x2c)+m.u32(draw+8)*0x10u;
        for(std::int32_t r=0;r<m.i32(draw+0xc);++r,range+=0x10u){
            if(m.u32(range)==6u)m.put32(range,5u);
            const std::uint32_t t=m.u32(range);
            const std::uint32_t mult=(t==4u||t==5u)?3u:0u;
            const std::uint32_t c=m.u32(range+8)*mult;
            edges+=c;vertices+=c*4u;
        }
    }
    const std::uint32_t edge_bytes=((edges*9u)*4u+0x1fu)&~0x1fu;
    const std::uint32_t range_bytes=(std::uint32_t(prims)*8u+0x1fu)&~0x1fu;
    const std::uint32_t vertex_bytes=((vertices*3u)*4u+0x1fu)&~0x1fu;
    const std::uint32_t total=vertex_bytes+range_bytes+edge_bytes;
    m.put32(object+8,kind);m.put32(object+4,0);
    const std::uint32_t block=s.malloc(total+8u);
    const std::uint32_t tail=block+total;
    m.put32(tail,block);m.put32(tail+4,0);
    m.put32(object+0x98,tail);
    if(m.u32(tail)==0u)return 0x8876017cu;
    m.put32(object+0xc,block);
    m.put32(object+0x14,block+edge_bytes);
    m.put32(object+0x10,block+edge_bytes+range_bytes);
    for(unsigned q=0;q<6;++q){
        const std::uint32_t v=object+0x20+q*0x14u;
        for(unsigned k=0;k<4;++k)m.put32(v+k*4,Quad77ff30[q][k]);
        m.put32(v+0x10,q<4?0x5f000000u:0u);
        m.putf(v,x87_float(X87(s.screen_740c94)*X87(m.f32(v))));
        m.putf(v+4,x87_float(X87(s.screen_740c98)*X87(m.f32(v+4))));
    }
    shadow_edges_4211f0(s,object);
    return 0;
}
void shadow_edges_4211f0(PcShadowServices& s,std::uint32_t object){
    auto& m=s.m;const Model mo{s};
    const std::uint32_t o=m.u32(object+0x1c),edges=m.u32(object+0xc);
    const std::uint32_t n=m.u32(mo.p(o+0x1c)+0x28);
    const std::uint32_t entry=mo.p(o+0x20)+n*8u;
    const std::int32_t prims=m.i32(entry+4);
    std::uint32_t prim=mo.p(o+0x24)+m.u32(entry)*0x14u;
    std::uint32_t count=0;
    for(std::int32_t k=0;k<prims;++k,prim+=0x14u){
        const std::uint32_t g=m.u32(prim);
        const std::uint32_t group=mo.p(o+0x30)+g*0x2cu;
        std::uint32_t ib_size=0,vb_size=0;
        const std::uint8_t* ib=s.lock(m.u32(mo.p(o+4)+g*4u),0,ib_size);
        const std::uint8_t* vb=s.lock(m.u32(mo.p(o+8)+g*16u),0x10,vb_size);
        if(!ib||!vb)throw std::runtime_error("4211F0: shadow model buffer not lockable");
        const std::uint32_t stride=m.u32(group+0x24)>>2;
        const std::uint32_t start=count;
        auto index=[&](std::uint32_t i)->std::uint32_t{
            if(std::size_t(i)*2u+2u>ib_size)throw std::out_of_range("4211F0 index outside the index buffer");
            std::uint16_t x;std::memcpy(&x,ib+std::size_t(i)*2u,2);return x;};
        auto vertex=[&](std::uint32_t f)->const float*{
            if(std::size_t(f)*4u+12u>vb_size)throw std::out_of_range("4211F0 vertex outside the vertex buffer");
            return reinterpret_cast<const float*>(vb+std::size_t(f)*4u);};
        auto rd=[&](std::uint32_t f,unsigned c){float x;std::memcpy(&x,vertex(f)+c,4);return x;};
        // Face normal of (v0, v1, v2) as 4211F0 computes it (d1 = v2 - v1, d2 = v0 - v1).
        auto face=[&](std::uint32_t a,std::uint32_t b,std::uint32_t c,float nrm[3]){
            const X87 d1x=X87(rd(c,0))-X87(rd(b,0)),d1y=X87(rd(c,1))-X87(rd(b,1)),d1z=X87(rd(c,2))-X87(rd(b,2));
            const float d2x=x87_float(X87(rd(a,0))-X87(rd(b,0))),d2y=x87_float(X87(rd(a,1))-X87(rd(b,1)));
            const X87 d2z=X87(rd(a,2))-X87(rd(b,2));
            const float nx=x87_float(d2z*d1y-d1z*X87(d2y));
            const float ny=x87_float(d1z*X87(d2x)-d2z*d1x);
            const float nz=x87_float(d1x*X87(d2y)-X87(d2x)*d1y);
            const auto u=driving::pc_d3dx_vec3_normalize({nx,ny,nz});
            nrm[0]=u[0];nrm[1]=u[1];nrm[2]=u[2];
        };
        const std::int32_t draws=m.i32(prim+0xc);
        std::uint32_t draw=mo.p(o+0x28)+m.u32(prim+4)*0x20u;
        for(std::int32_t dk=0;dk<draws;++dk,draw+=0x20u){
            const std::uint32_t base=m.u32(draw);
            std::uint32_t range=mo.p(o+0x2c)+m.u32(draw+8)*0x10u;
            for(std::int32_t r=0;r<m.i32(draw+0xc);++r,range+=0x10u){
                const std::uint32_t type=m.u32(range);
                if(type==5u){
                    const std::uint32_t begin=m.u32(range+4)+base,end=m.u32(range+8)+begin;
                    std::uint32_t parity=0;
                    for(std::uint32_t i=begin;i<end;++i,++parity){
                        const std::uint32_t v0=(index(i)+base)*stride;
                        std::uint32_t v1,v2;
                        if(parity&1u){v1=index(i+2);v2=index(i+1);}else{v1=index(i+1);v2=index(i+2);}
                        v1=(v1+base)*stride;v2=(v2+base)*stride;
                        if(v0==v1||v1==v2||v2==v0)continue;
                        float nrm[3];face(v0,v1,v2,nrm);
                        const std::uint8_t* V=vb;
                        count=add_edge_421680(m,count,start,edges,V,v0,v1,nrm);
                        count=add_edge_421680(m,count,start,edges,V,v1,v2,nrm);
                        count=add_edge_421680(m,count,start,edges,V,v2,v0,nrm);
                    }
                }else if(type==4u){
                    const std::uint32_t begin=m.u32(range+4)+base,end=begin+m.u32(range+8)*3u;
                    for(std::uint32_t i=begin;i<end;i+=3){
                        const std::uint32_t v2=(index(i+2)+base)*stride,v1=(index(i+1)+base)*stride,v0=(index(i)+base)*stride;
                        float nrm[3];face(v0,v1,v2,nrm);
                        count=add_edge_421680(m,count,start,edges,vb,v0,v1,nrm);
                        count=add_edge_421680(m,count,start,edges,vb,v1,v2,nrm);
                        count=add_edge_421680(m,count,start,edges,vb,v2,v0,nrm);
                    }
                }
            }
        }
        const std::uint32_t out=m.u32(object+0x14)+std::uint32_t(k)*8u;
        m.put32(out,start);m.put32(out+4,count);
    }
}
namespace {
// Edge record of 4211F0 (0x24): two vertex float indices, the normals of
// its one or two faces, and the face count minus one.
struct __attribute__((may_alias)) ShadowEdge {
    std::uint32_t vertex[2];     // +00 float index in the vertex buffer
    float normal[2][3];          // +08 first face, +14 second face
    std::int8_t faces;           // +20 0: one face, 1: two faces
    std::uint8_t pad[3];
};
static_assert(sizeof(ShadowEdge)==EdgeSize);
}
// 421740: the silhouette edges for the light (model space): an edge whose
// one face is lit, or whose two faces disagree, becomes a quad extruded
// away from the light (six vertices, two triangles, wound by which face is
// lit); the count goes to object +18.
void shadow_silhouette_421740(PcShadowServices& s,std::uint32_t object,const float light[3]){
    auto& m=s.m;const Model mo{s};
    const std::uint32_t o=m.u32(object+0x1c);
    const std::uint32_t n=m.u32(mo.p(o+0x1c)+0x28);
    const std::uint32_t entry=mo.p(o+0x20)+n*8u;
    std::uint32_t prim=mo.p(o+0x24)+m.u32(entry)*0x14u;
    std::uint32_t out=m.u32(object+0x10),ranges=m.u32(object+0x14);
    const std::uint32_t edges=m.u32(object+0xc);
    std::uint32_t total=0;
    const std::int32_t prims=m.i32(entry+4);
    auto lit=[&](const float nrm[3]){   // 421850: n . light >= 0
        return !below_zero((X87(nrm[1])*X87(light[1])+X87(nrm[2])*X87(light[2]))+X87(nrm[0])*X87(light[0]));};
    for(std::int32_t k=0;k<prims;++k,prim+=0x14u,ranges+=8u){
        std::uint32_t size=0;
        const std::uint8_t* vb=s.lock(m.u32(mo.p(o+8)+m.u32(prim)*16u),0x10,size);
        if(!vb)throw std::runtime_error("421740: shadow model vertex buffer not lockable");
        auto vertex=[&](std::uint32_t f,float r[3]){
            if(std::size_t(f)*4u+12u>size)throw std::out_of_range("421850 vertex outside the vertex buffer");
            std::memcpy(r,vb+std::size_t(f)*4u,12);};
        std::uint32_t range[2];std::memcpy(range,m.at(ranges,8),8);   // first edge, end
        for(std::uint32_t e=range[0];e<range[1];++e){
            const auto& edge=*reinterpret_cast<const ShadowEdge*>(m.at(edges+e*EdgeSize,EdgeSize));
            const bool front=lit(edge.normal[0]);
            bool reversed=false;
            if(edge.faces==0){if(!front)continue;}
            else{
                const bool back=lit(edge.normal[1]);
                if(front==back)continue;     // both lit or both dark: not on the silhouette
                reversed=back;
            }
            float a[3],b[3];vertex(edge.vertex[0],a);vertex(edge.vertex[1],b);
            float al[3],bl[3];     // the far ends: minus the light vector
            for(unsigned c=0;c<3;++c){al[c]=x87_float(X87(a[c])-X87(light[c]));bl[c]=x87_float(X87(b[c])-X87(light[c]));}
            auto* v=reinterpret_cast<float*>(m.at(out,0x48,true));
            auto put=[&](unsigned slot,const float p[3]){std::memcpy(v+slot*3u,p,12);};
            if(!reversed){put(0,a);put(1,b);put(2,al);put(3,b);put(4,bl);put(5,al);}
            else{put(0,b);put(1,a);put(2,bl);put(3,bl);put(4,a);put(5,al);}
            out+=0x48u;total+=4u;
        }
    }
    m.put32(object+0x18,total);
}
void shadow_alloc_46ba20(PcShadowServices& s,std::uint32_t car){
    auto& m=s.m;
    const std::int32_t model=std::int8_t(m.u8(car+0x11));
    for(const std::uint32_t slot:{0u,8u}){
        const std::uint32_t id=layout_word(model,slot==0u?8u:0x10u);
        if(id==0xffffffffu)continue;
        const std::uint32_t block=s.malloc(0xa8u);
        const std::uint32_t tail=block+0xa0u;
        m.put32(tail,block);m.put32(tail+4,0);
        m.put32(car+0x2b8+slot,tail);
        const std::uint32_t object=m.u32(tail);
        m.put32(car+0x2bc+slot,object);
        (void)shadow_build_420e50(s,id,object,3);
    }
}
void shadow_free_46bb20(PcShadowServices& s,std::uint32_t car){
    auto& m=s.m;
    const std::int32_t model=std::int8_t(m.u8(car+0x11));
    for(const std::uint32_t slot:{0u,8u}){
        if(layout_word(model,slot==0u?8u:0x10u)==0xffffffffu)continue;
        const std::uint32_t object=m.u32(car+0x2bc+slot);
        s.free_tail(object+0x98u);                                   // 4211D0
        s.free_tail(car+0x2b8+slot);
        m.put32(car+0x2bc+slot,0);
    }
}
namespace {
struct States {
    PcD3D9Device& d;
    void rs(std::uint32_t k,std::uint32_t v){if(d.get_render_state(k)!=v)d.set_render_state(k,v);}
    void tss(std::uint32_t st,std::uint32_t k,std::uint32_t v){if(d.get_texture_stage_state(st,k)!=v)d.set_texture_stage_state(st,k,v);}
};
void save_textures(PcShadowFrame& f,PcD3D9Device& d){for(std::uint32_t k=0;k<4;++k)f.saved_955a30[k]=d.get_texture(k);}
void restore_textures(PcShadowFrame& f,PcD3D9Device& d){
    for(std::uint32_t k=0;k<4;++k){
        const std::uint32_t t=f.saved_955a30[k];
        d.set_texture(k,t);
        if(t){d.release(t);f.saved_955a30[k]=0;}
    }
}
// 421A90.
void volume_421a90(PcShadowServices& s,PcShadowFrame& f,std::uint32_t object){
    auto& m=s.m;auto& d=s.d;States st{d};
    const std::int32_t count=m.i32(object+0x18);
    if(count<=0)return;
    render_reset_states_408880(f.fl,f.layer_7d25f0);
    st.rs(0x0e,0);st.rs(0x34,1);st.rs(0x09,1);st.rs(0x38,8);st.rs(0x36,1);st.rs(0x35,1);st.rs(0x39,1);st.rs(0x3a,0xff);st.rs(0x3b,0xff);
    st.rs(0x0f,0);st.rs(0xa8,0);st.rs(0x13,2);st.rs(0x14,1);
    for(std::uint32_t k=0;k<4;++k){
        d.set_texture(k,0);
        st.tss(k,3,1);st.tss(k,1,3);st.tss(k,6,1);st.tss(k,4,3);st.tss(k,0x1c,1);
    }
    d.set_pixel_shader(0);d.set_fvf(2);
    {std::array<float,16> w{};const auto cur=f.matrices.current();for(unsigned k=0;k<16;++k)w[k]=cur.f32(k*4);
     render_set_matrix_410f90(f.fl,w,6);}                        // 95D9E0 = current, 411060
    d.set_vertex_shader(f.vertex_shader_955a40);
    const std::uint32_t triangles=std::uint32_t(count/2);
    const std::uint8_t* data=m.at(m.u32(object+0x10),std::size_t(triangles)*3u*12u);
    st.rs(0x16,3);st.rs(0x37,4);
    d.draw_primitive_up(4,triangles,data,0xc);
    st.rs(0x16,2);st.rs(0x37,5);
    d.draw_primitive_up(4,triangles,data,0xc);
    d.set_stream_source(0,0,0,0);
    st.rs(0x09,2);st.rs(0x0e,1);st.rs(0xa8,0xf);st.rs(0x0f,1);st.rs(0x16,2);st.rs(0x34,1);st.rs(0x38,8);st.rs(0x37,2);
    f.fl.g.w(0x89ede0)=0;f.fl.g.w(0x89ede8)=0;
}
// 4220F0.
void darken_4220f0(PcShadowServices& s,PcShadowFrame& f,std::uint32_t object){
    if(!object)return;
    auto& d=s.d;States st{d};
    st.rs(0x07,0);st.rs(0x34,1);st.rs(0x1b,1);st.rs(0x18,0);st.rs(0x13,5);st.rs(0x14,6);st.rs(0x16,3);
    st.tss(0,2,2);st.tss(0,3,0);st.tss(0,1,4);st.tss(0,5,2);st.tss(0,6,0);st.tss(0,4,4);
    st.rs(0x39,0);st.rs(0x38,2);st.rs(0x37,1);
    d.set_fvf(0x44);
    d.draw_primitive_up(5,4,s.m.at(object+0x20,6u*0x14u),0x14);
    st.rs(0x07,1);st.rs(0x34,0);st.rs(0x38,8);st.rs(0x37,1);st.rs(0x1b,0);st.rs(0x16,2);
    f.fl.g.w(0x89ede0)=0;f.fl.g.w(0x89ede8)=0;
}
}
void shadow_draw_422550(PcShadowServices& s,PcShadowFrame& f,std::uint32_t object,std::uint32_t light){
    auto& m=s.m;auto& ms=f.matrices;
    if(m.u32(object+0x1c)==0u)return;
    const float k=-25.f;                                             // 6282D0
    const X87 lx=X87(m.f32(light))*X87(k);
    const float ly=x87_float(X87(m.f32(light+4))*X87(k));
    const X87 lz=X87(m.f32(light+8))*X87(k);
    // Inline push: depth + 1; the next slot is a copy of the current one when
    // it exists (else the current slot is overwritten below).
    ++ms.depth;
    std::ptrdiff_t slot=ms.current_offset;
    if(ms.depth<ms.capacity){
        const auto cur=ms.current();
        for(unsigned q=0;q<64;++q)ms.storage.put8(std::size_t(slot+64+q),cur.u8(q));
        slot+=64;ms.current_offset=slot;
    }
    // 40A100: the 3x3 of the current matrix into a zeroed local, copied to the slot.
    std::array<std::uint8_t,64> local{};
    {const auto cur=ms.current();for(unsigned q:{0u,1u,2u,4u,5u,6u,8u,9u,10u}){const std::uint32_t w=cur.u32(q*4);std::memcpy(local.data()+q*4,&w,4);}}
    // The words 40A100 does not write (+0C/+1C/+2C/+30..+3C) are set to 0; the
    // stack slot receives the whole local matrix.
    for(unsigned q=0;q<64;++q)ms.storage.put8(std::size_t(slot+q),local[q]);
    const auto M=ms.storage.sub(std::size_t(slot),64);
    const X87 r0=(lz*X87(M.f32(8))+X87(ly)*X87(M.f32(4)))+lx*X87(M.f32(0));
    --ms.depth;
    const float x0=x87_float(r0);
    const float y0=x87_float((lz*X87(M.f32(0x18))+lx*X87(M.f32(0x10)))+X87(ly)*X87(M.f32(0x14)));
    const float z0=x87_float((lz*X87(M.f32(0x28))+lx*X87(M.f32(0x20)))+X87(ly)*X87(M.f32(0x24)));
    if(ms.depth>=0){slot-=64;ms.current_offset=slot;}
    const float l[3]{x0,y0,z0};
    shadow_silhouette_421740(s,object,l);
    save_textures(f,s.d);
    s.d.set_stream_source(0,0,0,0);s.d.set_stream_source(1,0,0,0);
    volume_421a90(s,f,object);
    restore_textures(f,s.d);
    render_reset_states_408880(f.fl,f.layer_7d25f0);
    const std::uint32_t frame=m.u32(object+4)+1u;
    m.put32(object+4,std::int32_t(frame)<m.i32(object+8)?frame:0u);
}
void shadow_darken_422740(PcShadowServices& s,PcShadowFrame& f,std::uint32_t object){
    save_textures(f,s.d);
    darken_4220f0(s,f,object);
    restore_textures(f,s.d);
    render_reset_states_408880(f.fl,f.layer_7d25f0);
}
}

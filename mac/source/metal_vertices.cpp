// Uses the retained CPU vertex programs with a dense per-draw cache.
// Constants are copied once per draw and homogeneous clipping runs on Metal.
#include "metal_d3d9.hpp"
#include <algorithm>
#include <cstring>
namespace outrun::mac {
using namespace platform;
using pc_shader::V4;
bool MetalD3D9Device::shade_cached_vertex(std::uint32_t index,std::int32_t base,const V4* constants,Vertex& out){
    auto* decl=object(declaration);auto* vs=object(vertex_shader);
    if(!decl||!vs||vs->kind!=3)return false;
    pc_shader::VsState s;s.C=constants;
    for(auto& v:s.V)v={0,0,0,1};
    for(const auto& e:decl->elements){
        if(e.stream==PcDeclEndStream)break;
        const auto& st=streams.at(e.stream);auto* b=object(st.buffer);if(!b)return false;
        const std::size_t at=std::size_t(st.offset)+std::size_t(std::int64_t(base)+index)*st.stride+e.offset;
        V4 v{0,0,0,1};
        auto rd=[&](unsigned k){float f;std::memcpy(&f,b->bytes.data()+at+k*4,4);return f;};
        const unsigned need=e.type<=3?4u*(e.type+1u):4u;
        if(at+need>b->bytes.size())return false;
        switch(e.type){
        case 0:v={rd(0),0,0,1};break;case 1:v={rd(0),rd(1),0,1};break;case 2:v={rd(0),rd(1),rd(2),1};break;case 3:v={rd(0),rd(1),rd(2),rd(3)};break;
        case 4:{const auto* p=b->bytes.data()+at;v={p[2]/255.f,p[1]/255.f,p[0]/255.f,p[3]/255.f};break;}
        case 5:{const auto* p=b->bytes.data()+at;v={float(p[0]),float(p[1]),float(p[2]),float(p[3])};break;}
        default:break;
        }
        for(const auto& in:vs->program.inputs)if(in.usage==e.usage&&in.usage_index==e.usage_index)s.V[in.reg]=v;
    }
    pc_shader_run_vs(vs->program,s);
    std::memcpy(out.pos,&s.oPos,16);
    out.d[0]=pc_shader::sat(s.oD[0]);out.d[1]=pc_shader::sat(s.oD[1]);
    for(unsigned k=0;k<8;++k)out.t[k]=s.oT[k];
    out.fog=pc_shader::clamp01(s.oFog.x);
    return true;
}

std::uint32_t MetalD3D9Device::draw_indexed_primitive(std::uint32_t type,std::int32_t base,std::uint32_t min_index,std::uint32_t vertices,std::uint32_t start,std::uint32_t count){
    (void)min_index;(void)vertices;batch_.clear();
    auto* ib=object(indices);if(!ib)return 1;
    const std::size_t total=type==5?std::size_t(count)+2:type==4?std::size_t(count)*3:0;
    if(!total||(std::size_t(start)+total)*2>ib->bytes.size())return 1;
    note_dip_state(count);if(count_only)return 0;
    if(!targets_consistent()){errors.push_back("draw with the depth surface smaller than the render target");return 1;}
    auto* decl=object(declaration);auto* vs=object(vertex_shader);
    if(!decl||!vs||vs->kind!=3)return 1;
    bool supported=vs->program.blobs.size()<=32&&std::getenv("OR2_MAC_CPU_VERTEX")==nullptr;
    for(const auto& field:decl->elements)if(field.stream!=PcDeclEndStream&&field.type>5)supported=false;
    if(supported){
        unsigned low=65535,high=0;
        for(std::size_t k=start;k<std::size_t(start)+total;++k){std::uint16_t v;std::memcpy(&v,ib->bytes.data()+k*2,2);low=std::min(low,unsigned(v));high=std::max(high,unsigned(v));}
        if(std::int64_t(base)+low<0)return 1;
        for(const auto& field:decl->elements){
            if(field.stream==PcDeclEndStream)break;
            if(field.stream>=streams.size())return 1;
            const auto& stream=streams[field.stream];auto* buffer=object(stream.buffer);if(!buffer)return 1;
            const auto need=field.type<=3?4u*(field.type+1u):4u;
            if(std::uint64_t(stream.offset)+std::uint64_t(std::int64_t(base)+high)*stream.stride+field.offset+need>buffer->bytes.size())return 1;
        }
        native_vertices_=true;native_type_=type;native_base_=base;native_start_=start;native_count_=unsigned(total);
        triangles_drawn+=count;flush();native_vertices_=false;return 0;
    }
    if(vertex_cache_.empty()){vertex_cache_.resize(65536);vertex_generation_.resize(65536);}
    if(++generation_==0){std::fill(vertex_generation_.begin(),vertex_generation_.end(),0);generation_=1;}
    std::array<V4,256> constants;
    static_assert(sizeof(constants)==sizeof(vs_constants),"Vertex constants layout");
    std::memcpy(constants.data(),vs_constants.data(),sizeof constants);
    auto index=[&](std::uint32_t k){std::uint16_t v;std::memcpy(&v,ib->bytes.data()+std::size_t(k)*2,2);return v;};
    auto vertex=[&](unsigned i)->const Vertex*{
        if(vertex_generation_[i]!=generation_){
            if(!shade_cached_vertex(i,base,constants.data(),vertex_cache_[i]))return nullptr;
            vertex_generation_[i]=generation_;
        }return &vertex_cache_[i];
    };
    pretransformed_=false;
    for(unsigned p=0;p<count;++p){
        unsigned a,b,c;
        if(type==5){a=index(start+p);b=index(start+p+1);c=index(start+p+2);if(p&1)std::swap(a,b);}
        else{a=index(start+p*3);b=index(start+p*3+1);c=index(start+p*3+2);}
        const auto* va=vertex(a);const auto* vb=vertex(b);const auto* vc=vertex(c);
        if(!va||!vb||!vc){batch_.clear();return 1;}
        const Vertex tri[]{*va,*vb,*vc};raster(tri,3);
    }
    flush();return 0;
}
}

#include "pc_pmt.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <sstream>

namespace outrun::assets {
namespace {
void check(bool c,const char* message) {if(!c) throw FormatError(message);}
std::size_t mul(std::size_t a,std::size_t b) {
    check(!b || a<=std::numeric_limits<std::size_t>::max()/b,"Resource size overflow"); return a*b;
}
std::size_t add(std::size_t a,std::size_t b) {
    check(b<=std::numeric_limits<std::size_t>::max()-a,"Resource offset overflow"); return a+b;
}
struct ParseBudget {
    std::size_t allocated=0;
    void charge(std::size_t count,std::size_t size) {
        allocated=add(allocated,mul(count,size));
        check(allocated<=128u*1024u*1024u,"PMT metadata allocation budget exceeded");
    }
};
void records(ByteView b,std::size_t off,std::size_t n,std::size_t stride) {b.require(off,mul(n,stride));}
ByteView sub(ByteView b,std::size_t off,std::size_t n) {b.require(off,n);return ByteView(b.data+off,n);}
std::uint16_t u16(ByteView b,std::size_t o) {b.require(o,2);return std::uint16_t(b.data[o]) | std::uint16_t(std::uint16_t(b.data[o+1])<<8);}
float f32(ByteView b,std::size_t o) {const auto u=b.u32(o); float f;static_assert(sizeof(f)==sizeof(u));std::memcpy(&f,&u,4);check(std::isfinite(f),"Non-finite resource float");return f;}
std::int32_t i32(ByteView b,std::size_t o) {const auto u=b.u32(o);return u<=0x7fffffffu?std::int32_t(u):std::int32_t(std::int64_t(u)-0x100000000LL);}
std::size_t interval_count(std::uint32_t first,std::uint32_t end,std::size_t stride) {
    check(end>=first && (end-first)%stride==0,"Table boundary / record stride mismatch");return (end-first)/stride;
}
std::size_t index_count(const PmtPrimitive& p) {
    switch(p.type) {case 5:return mul(p.count,3);case 6:check(p.count>0,"Empty triangle strip");return std::size_t(p.count)+2;case 8:return mul(p.count,4);default:throw FormatError("Unsupported primitive topology");}
}
struct Layout {std::uint32_t normal=0,uv=0,color=0;bool has_uv=false,has_color=false;};
Layout layout(const PmtFormat& f) {
    check(f.streams==1 && f.shader==0,"Preview unsupported: animated/custom vertex shader");
    check((f.fvf&0x0e)==2 && (f.fvf&0x10),"Preview unsupported: non-XYZ/normal vertex format (skinning)");
    check((f.fvf & ~0x0f52u)==0,"Preview unsupported: extended FVF bits");
    const unsigned uv_count=(f.fvf>>8)&15;check(uv_count<=8,"Too many UV channels");
    Layout l;l.normal=12;l.has_color=(f.fvf&0x40)!=0;l.color=24;
    l.uv=24+(l.has_color?4:0);l.has_uv=uv_count!=0;
    check(f.stride==l.uv+uv_count*8,"Preview unsupported: packed normal or vertex stride");
    return l;
}
}
DdsInfo inspect_dds(ByteView b) {
    b.require(0,128);check(b.u32(0)==0x20534444 && b.u32(4)==124 && b.u32(76)==32,"Invalid DDS header");
    DdsInfo d;d.height=b.u32(12);d.width=b.u32(16);d.mips=std::max(1u,b.u32(28));d.bits=b.u32(88);d.fourcc=b.u32(84);
    check(d.width && d.height && d.width<=16384 && d.height<=16384,"DDS dimensions outside supported bounds");
    std::uint32_t max_mips=1;for(auto n=std::max(d.width,d.height);n>1;n>>=1)++max_mips;
    check(d.mips<=max_mips,"Invalid DDS mip count");
    const auto caps2=b.u32(112),pf=b.u32(80);check(!(caps2&0x200000u),"Volume DDS not implemented");
    d.faces=1;
    // Some supplied PC DDS omit the generic cube bit but DO set all face bits.
    if(caps2&(0x200u|0xfc00u)) {d.faces=0;for(unsigned bit=10;bit<=15;++bit)d.faces+=(caps2>>bit)&1u;check(d.faces!=0,"DDS cubemap with no faces");}
    unsigned block=0;
    if(pf&4) {switch(d.fourcc) {case 0x31545844:block=8;break;case 0x32545844:case 0x33545844:case 0x34545844:case 0x35545844:block=16;break;default:throw FormatError("Unsupported DDS FourCC");}}
    else check((pf&(0x40u|0x20000u|0x2u)) && (d.bits==8 || d.bits==16 || d.bits==24 || d.bits==32),"Unsupported DDS pixel layout");
    std::size_t bytes=0;auto w=d.width,h=d.height;
    for(unsigned mip=0;mip<d.mips;++mip) {
        std::size_t level;
        if(block) level=mul(mul(std::max(1u,(w+3)/4),std::max(1u,(h+3)/4)),block);
        else {
            std::size_t pitch=(mul(w,d.bits)+7)/8;
            if(mip==0 && (b.u32(8)&8u)) {check(b.u32(20)>=pitch,"DDS pitch too small");pitch=b.u32(20);}
            level=mul(pitch,h);
        }
        check(level<=std::numeric_limits<std::size_t>::max()-bytes,"DDS size overflow");bytes+=level;
        w=std::max(1u,w/2);h=std::max(1u,h/2);
    }
    d.file_bytes=add(128,mul(bytes,d.faces));b.require(0,d.file_bytes);return d;
}
PmtView inspect_pc_pmt(ByteView b) {
    check(b.size<=128u*1024u*1024u,"PMT exceeds 128 MiB input budget");
    ParseBudget budget;
    b.require(0,16);const auto object_count=b.u32(0),texture_count=b.u32(4),ss=b.u32(8),vs=b.u32(12);
    check(std::uint64_t(ss)+vs+16==b.size,"PMT segment lengths do not match input");
    PmtView p{sub(b,16,ss),sub(b,16+std::size_t(ss),vs),{},{},0};auto s=p.system;
    s.require(0,24);check(s.u32(4)==object_count && s.u32(8)==texture_count,"PMT repeated counts disagree");
    records(s,24,object_count,60);
    const auto texture_slots=24+mul(object_count,60);records(s,texture_slots,std::size_t(texture_count)+1,4);
    const auto xpr=texture_slots+mul(std::size_t(texture_count)+1,4);s.require(xpr,12);
    check(s.u32(xpr)==0x30525058u,"Missing indexed XPR header (no signature scanning)");
    const auto xpr_header=s.u32(xpr+8),xpr_total=s.u32(xpr+4);
    check(xpr_header>=12+mul(texture_count,20) && xpr_total>=xpr_header,"Invalid XPR sizes");
    s.require(xpr,xpr_header);const auto texture_bytes=xpr_total-xpr_header;p.video.require(0,texture_bytes);
    budget.charge(texture_count,sizeof(TextureView)+sizeof(std::uint32_t));
    budget.charge(object_count,sizeof(PmtObject));
    std::vector<std::uint32_t> texture_offsets;
    texture_offsets.reserve(texture_count);
    for(std::size_t i=0;i<texture_count;++i) {
        const auto off=s.u32(xpr+12+i*20+4);
        check(off<=texture_bytes,"Texture offset outside XPR payload");
        texture_offsets.push_back(off);
    }
    std::sort(texture_offsets.begin(),texture_offsets.end());
    p.textures.reserve(texture_count);
    for(std::size_t i=0;i<texture_count;++i) {
        const auto t=xpr+12+i*20;TextureView tex;
        tex.data_offset=s.u32(t+4);tex.format_word=s.u32(t+12);tex.size_word=s.u32(t+16);
        check(tex.data_offset<=texture_bytes,"Texture offset outside XPR payload");
        auto end=texture_bytes;
        const auto next=std::upper_bound(texture_offsets.begin(),texture_offsets.end(),tex.data_offset);
        if(next!=texture_offsets.end())end=*next;
        tex.available_bytes=end-tex.data_offset;
        auto blob=sub(p.video,tex.data_offset,tex.available_bytes);
        if(blob.size>=4 && blob.u32(0)==0x20534444u) {tex.is_dds=true;tex.dds=inspect_dds(blob);}
        p.textures.push_back(tex);
    }
    p.objects.reserve(object_count);
    for(std::size_t oi=0;oi<object_count;++oi) {
        const auto start=24+oi*60;std::array<std::uint32_t,15> q{};for(unsigned j=0;j<15;++j)q[j]=s.u32(start+j*4);
        s.require(q[5],52);std::array<std::uint32_t,13> h{};for(unsigned j=0;j<13;++j)h[j]=s.u32(q[5]+j*4);
        constexpr unsigned map[8]={0,2,3,4,5,6,7,8};bool absolute=true,relative=true;
        for(unsigned j=0;j<8;++j) {absolute &= q[j+7]==h[map[j]];relative &= std::uint64_t(q[6])+h[map[j]]==q[j+7];}
        check(absolute || relative,"Object header cannot be reconciled with relocation table");
        PmtObject o;o.header_relative=!absolute;o.header_offset=q[5];
        for(unsigned j=9;j<13;++j)check(h[j]<=0x7fffffffu,"Negative object table count");
        records(s,q[12],h[9],44);records(s,q[10],h[10],32);records(s,q[13],h[11],88);records(s,q[14],h[12],72);
        records(s,q[1],h[9],4);records(s,q[2],h[9],16);
        budget.charge(h[9],sizeof(PmtFormat));
        budget.charge(h[10],sizeof(PmtMaterialGroup));
        budget.charge(h[11],sizeof(PmtMaterial));
        for(std::size_t j=0;j<h[9];++j) {
            const auto off=q[12]+j*44;PmtFormat f;
            f.streams=s.u32(off);f.index_bytes=s.u32(off+24);f.vertex_bytes=s.u32(off+28);f.fvf=s.u32(off+32);f.stride=s.u32(off+36);f.shader=s.u32(off+40);
            check(f.streams>=1 && f.streams<=4 && f.stride>0 && f.stride<=1024,"Invalid vertex stream description");
            check(f.vertex_bytes%f.stride==0 && f.index_bytes%2==0,"Partial vertex/index record");
            const auto ip=s.u32(q[1]+j*4);s.require(ip,12);f.index_offset=s.u32(ip+4);s.require(f.index_offset,f.index_bytes);
            for(unsigned k=0;k<f.streams;++k) {const auto vp=s.u32(q[2]+j*16+k*4);s.require(vp,12);f.vertex_offsets[k]=s.u32(vp+4);p.video.require(f.vertex_offsets[k],f.vertex_bytes);}
            o.formats.push_back(f);
        }
        o.materials.reserve(h[11]);
        for(std::size_t j=0;j<h[11];++j) {
            const auto off=q[13]+j*88;PmtMaterial m;const auto ci=i32(s,off);m.attrib=s.u32(off+4);
            check(ci>=-1 && (ci<0 || std::uint32_t(ci)<h[12]),"Material color index out of range");
            if(ci>=0)for(unsigned k=0;k<4;++k)m.diffuse[k]=f32(s,q[14]+std::size_t(ci)*72+k*4);
            for(unsigned k=0;k<4;++k) {m.texture_indices[k]=i32(s,off+24+k*20);m.texture_attribs[k]=s.u32(off+8+k*20);
                check(m.texture_indices[k]>=-1 && (m.texture_indices[k]<0 || std::uint32_t(m.texture_indices[k])<texture_count),"Material texture index out of range");}
            o.materials.push_back(m);
        }
        o.material_groups.reserve(h[10]);
        for(std::size_t j=0;j<h[10];++j) {
            const auto off=q[10]+j*32;PmtMaterialGroup g;g.base_vertex=s.u32(off);g.material=s.u32(off+4);
            check(g.material<o.materials.size(),"Material group references invalid material");
            const auto first=s.u32(off+8),count=s.u32(off+12);const auto po=std::size_t(q[11])+mul(first,16);records(s,po,count,16);
            budget.charge(count,sizeof(PmtPrimitive));
            g.primitives.reserve(count);
            for(std::size_t k=0;k<count;++k)g.primitives.push_back({s.u32(po+k*16),s.u32(po+k*16+4),s.u32(po+k*16+8),s.u32(po+k*16+12)});
            o.material_groups.push_back(std::move(g));
        }
        const auto group_count=interval_count(q[9],q[10],20);records(s,q[9],group_count,20);
        budget.charge(group_count,sizeof(PmtVertexGroup));
        for(std::size_t j=0;j<group_count;++j) {
            const auto off=q[9]+j*20;PmtVertexGroup g;g.format=s.u32(off);check(g.format<o.formats.size(),"Vertex group format out of range");const auto& f=o.formats[g.format];
            for(unsigned a=0;a<2;++a) {
                g.first_material_group[a]=s.u32(off+4+a*4);g.material_group_count[a]=s.u32(off+12+a*4);
                const auto first=g.first_material_group[a],count=g.material_group_count[a];
                check(count==0 || (first<=o.material_groups.size() && count<=o.material_groups.size()-first),"Material group interval out of range");
                for(std::size_t k=first;k<std::size_t(first)+count;++k) {
                    const auto& m=o.material_groups[k];
                    for(const auto& primitive:m.primitives) {
                        const auto n=index_count(primitive);check(primitive.start<=f.index_bytes/2 && n<=f.index_bytes/2-primitive.start,"Primitive exceeds index buffer");
                        check(n<=64000000u-p.checked_index_references,"PMT index-validation work budget exceeded");
                        for(std::size_t index=0;index<n;++index) {
                            const auto value=u16(s,f.index_offset+2*(std::size_t(primitive.start)+index));
                            check(std::uint64_t(m.base_vertex)+value<f.vertex_bytes/f.stride,"Index + base vertex outside vertex buffer");
                        }
                        p.checked_index_references+=n;
                    }
                }
            }
            o.vertex_groups.push_back(g);
        }
        p.objects.push_back(std::move(o));
    }
    return p;
}
std::vector<std::uint32_t> triangulate(const std::vector<std::uint32_t>& ii,std::uint32_t type,std::uint64_t* removed) {
    std::vector<std::uint32_t> out;std::uint64_t deg=0;
    auto triangle=[&](auto a,auto b,auto c) {if(a==b || b==c || a==c){++deg;return;}out.insert(out.end(),{a,b,c});};
    if(type==6) {check(ii.size()>=3,"Short triangle strip");for(std::size_t i=0;i+2<ii.size();++i) {if(i&1)triangle(ii[i+1],ii[i],ii[i+2]);else triangle(ii[i],ii[i+1],ii[i+2]);}}
    else if(type==5) {check(ii.size()%3==0,"Partial triangle list");for(std::size_t i=0;i<ii.size();i+=3)triangle(ii[i],ii[i+1],ii[i+2]);}
    else if(type==8) {check(ii.size()%4==0,"Partial quad list");for(std::size_t i=0;i<ii.size();i+=4) {triangle(ii[i],ii[i+1],ii[i+2]);triangle(ii[i],ii[i+2],ii[i+3]);}}
    else throw FormatError("Unsupported primitive topology");
    if(removed) *removed=deg;
    return out;
}
std::vector<PreviewMesh> decode_static_object(const PmtView& p,std::size_t object_index) {
    check(object_index<p.objects.size(),"Object selection out of range");const auto& o=p.objects[object_index];std::vector<PreviewMesh> meshes;
    for(std::size_t gi=0;gi<o.vertex_groups.size();++gi) {
        const auto& g=o.vertex_groups[gi];const auto& f=o.formats[g.format];const auto l=layout(f);const auto vertices=f.vertex_bytes/f.stride;
        // Retain source vertex numbering here; compact per batch after triangulation.
        for(unsigned a=0;a<2;++a)for(std::size_t mgi=g.first_material_group[a];mgi<std::size_t(g.first_material_group[a])+g.material_group_count[a];++mgi) {
            const auto& mg=o.material_groups[mgi];PreviewMesh mesh;mesh.object=std::uint32_t(object_index);mesh.group=std::uint32_t(mgi);mesh.material=mg.material;mesh.alpha_class=a;
            for(const auto& pr:mg.primitives) {
                std::vector<std::uint32_t> idx;const auto n=index_count(pr);idx.reserve(n);
                for(std::size_t i=0;i<n;++i)idx.push_back(u16(p.system,f.index_offset+2*(std::size_t(pr.start)+i))+mg.base_vertex);
                std::uint64_t deg=0;auto tri=triangulate(idx,pr.type,&deg);mesh.indices.insert(mesh.indices.end(),tri.begin(),tri.end());mesh.degenerate_triangles+=deg;
            }
            std::vector<std::uint32_t> remap(vertices,0xffffffffu);
            for(auto& index:mesh.indices) {
                if(remap[index]==0xffffffffu) {
                    const auto off=std::size_t(f.vertex_offsets[0])+std::size_t(index)*f.stride;PreviewVertex v;
                    for(unsigned k=0;k<3;++k) {v.position[k]=f32(p.video,off+k*4);v.normal[k]=f32(p.video,off+l.normal+k*4);}
                    if(l.has_uv)for(unsigned k=0;k<2;++k)v.uv[k]=f32(p.video,off+l.uv+k*4);
                    if(l.has_color){const auto c=p.video.u32(off+l.color);v.color={float((c>>16)&255)/255,float((c>>8)&255)/255,float(c&255)/255,float((c>>24)&255)/255};}
                    remap[index]=std::uint32_t(mesh.vertices.size());mesh.vertices.push_back(v);
                }
                index=remap[index];
            }
            meshes.push_back(std::move(mesh));
        }
    }
    return meshes;
}
} // namespace outrun::assets

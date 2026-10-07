#include "platform/pc_pmt_loader.hpp"
#include <cstring>
#include <stdexcept>
namespace outrun::platform {
namespace {
using driving::Bytes;
const std::uint8_t* video_bytes(const PcPmtResources& r,std::uint32_t source,std::uint32_t length){
    if(source>r.video_size||length>r.video_size-source)throw std::out_of_range("PMT video section");
    return r.video+source;
}
std::uint32_t upload(PcD3D9Device& device,std::uint32_t buffer,const std::uint8_t* from,std::uint32_t length){
    auto* p=device.lock(buffer,0,length,0);
    if(!p)throw std::runtime_error("PMT buffer Lock failed (the PC would copy through an invalid pointer)");
    std::memcpy(p,from,length);
    device.unlock(buffer);
    return buffer;
}
}
void pmt_objects_begin_42e490(PcPmtResources& r,PcPmtLoadCursor& c){
    auto s=r.view();
    r.object_table_20=0x14;
    s.put32(0x14,0x18);        // the PC stores the absolute pointer system+0x18
    r.object_count_0c=s.u32(4);
    c.object_95b230=0;c.offset_95b234=0x18;
    r.texture_table_24=0;
}
bool pmt_objects_step_42e4f0(PcPmtResources& r,PcPmtLoadCursor& c,PcD3D9Device& device,PcShaderCache& cache,
    const PcShaderGlobals& globals){
    auto s=r.view();
    if(c.object_95b230>=r.object_count_0c)return true;
    const auto o=c.offset_95b234+c.object_95b230*0x3cu;
    s.check(o,0x3c); // fields +04..+38 are relocated by the PC; they stay offsets here
    const auto info=s.u32(o+0x18);
    const auto groups=s.i32(info+0x24);
    for(std::int32_t g=0;g<groups;++g){
        const auto group=s.u32(o+0x30)+std::uint32_t(g)*0x2cu;
        for(std::int32_t k=0;k<s.i32(group);++k){
            const auto slot=s.u32(o+0x08)+std::uint32_t(g)*16u+std::uint32_t(k)*4u;
            const auto source=s.u32(s.u32(slot)+4);
            const auto length=s.u32(group+0x1c);
            const auto vb=device.create_vertex_buffer(length,0,0,1);
            s.put32(slot,vb?upload(device,vb,video_bytes(r,source,length),length):0u);
        }
        {
            const auto slot=s.u32(o+0x04)+std::uint32_t(g)*4u;
            const auto source=s.u32(s.u32(slot)+4);
            const auto length=s.u32(group+0x18);
            const auto ib=device.create_index_buffer(length,0,0x65,1); // D3DFMT_INDEX16, D3DPOOL_MANAGED
            s.check(source,length); // index data is copied from the system section
            s.put32(slot,ib?upload(device,ib,r.system.data()+source,length):0u);
        }
        const auto count=s.i32(s.u32(o+0x18)+0x2c);
        for(std::int32_t k=0;k<count;++k){
            const auto rec=s.u32(o+0x0c)+std::uint32_t(count*g+k)*0x18u;
            const auto material=s.u32(o+0x34)+std::uint32_t(k)*0x58u;
            material_setup_40fd70(s.sub(rec,0x18),s.sub(material,0x58),s.sub(group,0x2c),device,cache,globals);
        }
    }
    ++c.object_95b230;
    if(c.object_95b230<r.object_count_0c)return false;
    c.offset_95b234+=r.object_count_0c*0x3cu;
    return true;
}
void pmt_textures_begin_42e850(PcPmtResources& r,PcPmtLoadCursor& c){
    auto s=r.view();
    const auto header=c.offset_95b234;
    r.texture_table_24=header;
    s.put32(header,header+4); // absolute pointer on the PC
    c.offset_95b234=header+s.u32(8)*4u+4u;
    r.texture_records_04=c.offset_95b234+0xc;
    c.texture_record_957be0=r.texture_records_04;
    c.texture_95b22c=0;
}
bool pmt_textures_step_42e8c0(PcPmtResources& r,PcPmtLoadCursor& c,PcD3D9Device& device){
    auto s=r.view();
    const auto count=s.u32(8);
    if(c.texture_95b22c>=count)return true;
    const auto table=s.u32(r.texture_table_24);
    const auto slot=table+c.texture_95b22c*4u;
    const auto record=c.texture_record_957be0;
    const auto source=s.u32(record+4);
    s.put32(slot,0);
    PcTextureFileRequest q{};
    q.size=s.u32(record+0x10)+0x80u;
    q.data=video_bytes(r,source,q.size);
    std::uint32_t caps2;std::memcpy(&caps2,video_bytes(r,source+0x70,4),4); // DDS dwCaps2
    q.width=q.height=0xffffffffu;q.usage=0;q.format=0;q.pool=1;q.filter=1;q.colour_key=0;
    if(caps2){q.cube=true;q.mip_levels=0xffffffffu;q.mip_filter=1;}
    else{q.cube=false;q.mip_levels=4;q.mip_filter=3;}
    s.put32(slot,device.create_texture_from_file(q));
    c.texture_record_957be0+=0x14;
    ++c.texture_95b22c;
    return c.texture_95b22c>=s.u32(8);
}
std::uint32_t pmt_object_shaders_4103f0(PcPmtResources& r,std::uint32_t index,std::uint32_t kind,PcD3D9Device& device,
    PcShaderCache& cache,const PcShaderGlobals& globals){
    device.set_vertex_shader(0);
    if(index>=r.object_count_0c)return 0u;
    auto s=r.view();
    const auto o=s.u32(r.object_table_20)+index*0x3cu;
    const auto info=s.u32(o+0x18);
    const std::int32_t groups=s.i32(info+0x24),count=s.i32(info+0x2c);
    for(std::int32_t g=0;g<groups;++g){
        const auto group=s.u32(o+0x30)+std::uint32_t(g)*0x2cu;
        for(std::int32_t k=0;k<count;++k){
            const auto rec=s.u32(o+0x0c)+std::uint32_t(count*g+k)*0x18u;
            const auto material=s.u32(o+0x34)+std::uint32_t(k)*0x58u;
            material_kind_setup_4104d0(s.sub(rec,0x18),s.sub(material,0x58),s.sub(group,0x2c),kind,device,cache,globals);
        }
    }
    return 1u;
}
std::uint32_t pmt_object_group_type_4066d0(PcPmtResources& r,std::uint32_t index,std::uint32_t type){
    if(index>=r.object_count_0c)return 0u;
    auto s=r.view();
    const auto o=s.u32(r.object_table_20)+index*0x3cu;
    const std::int32_t groups=s.i32(s.u32(o+0x18)+0x24);
    for(std::int32_t g=0;g<groups;++g)s.put32(s.u32(o+0x30)+std::uint32_t(g)*0x2cu+0x28u,type);
    return 1u;
}
std::uint32_t pmt_object_mesh_flags_406730(PcPmtResources& r,std::uint32_t index,std::uint32_t and_mask,std::uint32_t or_mask){
    if(index>=r.object_count_0c)return 0u;
    auto s=r.view();
    const auto mesh=s.u32(s.u32(r.object_table_20)+index*0x3cu+0x1c);
    s.put32(mesh,(s.u32(mesh)&and_mask)|or_mask);
    return 1u;
}
}

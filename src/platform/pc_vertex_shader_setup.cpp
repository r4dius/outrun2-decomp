#include <cstdio>
#include "platform/pc_vertex_shader_setup.hpp"
#include "platform/embedded_shader_data.hpp"
#include <stdexcept>
namespace outrun::platform {
namespace {
using driving::Bytes;
constexpr std::uint32_t End=0xffffu;
std::uint32_t le32(const std::uint8_t* p){
    return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|(std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);
}
unsigned type_size(std::uint8_t type){
    switch(type){case 0:return 4;case 1:return 8;case 2:return 12;case 3:return 16;case 4:case 5:return 4;default:return 0;}
}
struct Builder {
    PcVertexDeclaration& out;unsigned count{};std::uint16_t offset{};
    void add(std::uint8_t type,std::uint8_t usage,std::uint8_t index){
        if(count+1>=PcMaxFvfDeclSize)throw std::length_error("FVF declaration");
        out[count++]=PcVertexElement{0,offset,type,0,usage,index};offset=std::uint16_t(offset+type_size(type));
    }
    void end(){out[count]=PcVertexElement{PcDeclEndStream,0,17,0,0,0};}
};
// 411380: fills one element and returns the size of the types FLOAT1..D3DCOLOR
// (the PC table returns 0 for FLOAT4).
std::uint32_t set_element_411380(PcVertexElement& e,std::uint16_t stream,std::uint8_t type,std::uint16_t offset,
    std::uint8_t method,std::uint8_t usage,std::uint8_t index){
    e=PcVertexElement{stream,offset,type,method,usage,index};
    switch(type){case 0:return 4;case 1:return 8;case 2:return 12;case 4:return 4;default:return 0;}
}
// 40DFD0 (bridge 103C56F compares the first element with the end marker):
// true when elements are absent or contain TEXCOORD `index`.
bool has_texcoord_40dfd0(const PcVertexElement* e,std::uint32_t index){
    if(!e)return true;
    for(;e->stream!=PcDeclEndStream;++e)if(e->usage==5&&e->usage_index==index)return true;
    return false;
}
std::uint32_t adler32(const std::uint32_t* words,std::uint32_t count){
    std::uint32_t a=1,b=0;
    for(std::uint32_t k=0;k<count;++k)for(unsigned s=0;s<4;++s){
        a=(a+((words[k]>>(s*8))&0xffu))%65521u;b=(b+a)%65521u;}
    return (b<<16)|a;
}
}
std::uint32_t pc_adler32_43a6f0(const std::uint8_t* data,std::size_t size){
    // Same sums as reducing after every byte, reduced once per 5552 bytes
    // (zlib NMAX: the sums cannot overflow 32 bits before that).
    std::uint32_t a=1,b=0;
    while(size){
        const std::size_t n=size<5552u?size:5552u;size-=n;
        for(std::size_t k=0;k<n;++k){a+=data[k];b+=a;}
        data+=n;a%=65521u;b%=65521u;
    }
    return (b<<16)|a;
}
std::uint32_t pc_shader_data_u32(std::uint32_t va){
    for(std::size_t k=0;k<EmbeddedRenderRangeCount;++k){
        const auto& r=EmbeddedRenderRanges[k];
        if(va>=r.va&&va-r.va<=r.size-4u)return le32(r.bytes+(va-r.va));
    }
    char text[64];std::snprintf(text,sizeof text,"PC renderer data address %08x",va);
    throw std::out_of_range(text);
}
std::vector<std::uint32_t> pc_shader_tokens(std::uint32_t va){
    std::vector<std::uint32_t> t;
    for(std::uint32_t k=0;;++k){t.push_back(pc_shader_data_u32(va+k*4u));if(t.back()==End)return t;}
}
PcVertexDeclaration pc_shader_declaration(std::uint32_t va){
    PcVertexDeclaration d{};
    for(unsigned k=0;k<PcMaxFvfDeclSize;++k){
        const auto w0=pc_shader_data_u32(va+k*8u),w1=pc_shader_data_u32(va+k*8u+4u);
        d[k]=PcVertexElement{std::uint16_t(w0),std::uint16_t(w0>>16),std::uint8_t(w1),std::uint8_t(w1>>8),
            std::uint8_t(w1>>16),std::uint8_t(w1>>24)};
        if(d[k].stream==PcDeclEndStream)return d;
    }
    throw std::runtime_error("PC declaration without end marker");
}
bool d3dx_declarator_from_fvf(std::uint32_t fvf,PcVertexDeclaration& out){
    if(fvf&0x6001u)return false; // D3DFVF_RESERVED0 | D3DFVF_RESERVED2
    out={};Builder b{out};
    const auto position=fvf&0x400eu;
    if(position){
        const bool blend=(fvf&0xeu)>=6u;
        std::uint32_t weights=1u+(((fvf&0xeu)-6u)>>1);
        const bool indices=(fvf&0x8000u)||(fvf&0x1000u);
        if(indices)--weights;
        if(position==0x4002u)b.add(3,0,0);
        else if(position==0x4u)b.add(3,9,0);
        else b.add(2,0,0);
        if(blend){
            if(weights>=1u&&weights<=4u)b.add(std::uint8_t(weights-1u),1,0);
            else if(weights!=0u)return false;
            if(indices)b.add((fvf&0x1000u)?5:4,2,0);
        }
    }
    if(fvf&0x10u)b.add(2,3,0);
    if(fvf&0x20u)b.add(0,4,0);
    if(fvf&0x40u)b.add(4,10,0);
    if(fvf&0x80u)b.add(4,10,1);
    const unsigned textures=(fvf&0xf00u)>>8;
    if(textures>8u)return false;
    static constexpr std::uint8_t format_type[4]={1,2,3,0};
    for(unsigned k=0;k<textures;++k)b.add(format_type[(fvf>>(16u+2u*k))&3u],5,std::uint8_t(k));
    b.end();return true;
}
namespace {
// 4101C0 / 40FEE0 declaration: they differ only in the TEXCOORD usage index
// of the stream-1 sets (4101C0: 4.., 40FEE0: the set count ..).
void stream1_declaration(std::uint32_t fvf,PcVertexDeclaration& e,bool from_set_count){
    if(!d3dx_declarator_from_fvf(fvf,e))throw std::runtime_error("4101C0 FVF declaration");
    unsigned n=0;while(e[n].stream!=PcDeclEndStream)++n;
    auto need=[&](unsigned count){if(n+count>=PcMaxFvfDeclSize)throw std::length_error("4101C0 declaration");};
    need(3);
    // The end element becomes the stream-1 position (its Type stays FLOAT3
    // after the rewrite below), then the stream-1 normal.
    e[n].offset=0;e[n].method=0;e[n].usage=0;
    e[n].stream=1;e[n].usage_index=1;e[n].type=2;++n;
    e[n]=PcVertexElement{1,0x0c,2,0,3,1};++n;
    std::uint16_t offset=0x18;
    if(fvf&0x40u){e[n]=PcVertexElement{1,0x18,4,0,10,1};offset=0x1c;++n;}
    const auto t=fvf&0xf00u;
    unsigned sets=t==0x100u?1u:t==0x200u?2u:t==0x300u?3u:t==0x400u?4u:0u;
    need(sets+1);
    for(unsigned k=0;k<sets;++k){
        offset=std::uint16_t(offset+set_element_411380(e[n],1,1,offset,0,5,std::uint8_t((from_set_count?sets:4u)+k)));++n;
    }
    e[n]=PcVertexElement{PcDeclEndStream,0,0x11,0,0,0};
}
}
void type2_declaration_4101c0(std::uint32_t fvf,PcVertexDeclaration& e){stream1_declaration(fvf,e,false);}
void type2_declaration_40fee0(std::uint32_t fvf,PcVertexDeclaration& e){stream1_declaration(fvf,e,true);}
std::array<std::uint32_t,2> layer_texcoord_40e010(Bytes layer,const PcVertexElement* elements,std::uint32_t type){
    std::array<std::uint32_t,2> r{0,0};
    if(layer.i32(0x10)<0)return r;
    const auto word=layer.u32(0),flags=word&0x3ffu;
    if(flags&0x8eu){
        if(flags&0x80u)return {0xc,2};
        if(flags&0xau)return {0xb,2};
        return {9,1};
    }
    const auto source=word>>28;
    const std::uint32_t index=source==1u?1u:source==2u?2u:source==3u?3u:0u;
    if(has_texcoord_40dfd0(elements,index))r[0]=type==2u?5u+index:1u+index;
    return r;
}
std::uint32_t material_config_40de10(Bytes group,Bytes m,const PcVertexElement* elements){
    const auto type=group.u32(0x28),fvf=group.u32(0x20);
    std::uint32_t c=0;
    if(type==2u)c=1;
    else{
        const auto blend=fvf&0x400eu;
        c=blend==6u?2u:blend==8u?3u:blend==0xau?4u:0u;
    }
    if(fvf&0x40u)c=type==2u?((c&~8u)|0x10u):((c&~0x10u)|8u);
    else c&=~0x18u;
    const auto flags=m.u32(4)&0xffu;
    const auto layers=(m.u32(0x44)|m.u32(0x30)|m.u32(0x08)|m.u32(0x1c))&0x3ffu;
    bool lit=(flags&1u)||(layers&1u);
    if((layers&0x80u)&&(m.u32(4)&0x18000000u)==0x18000000u)lit=false;
    if(flags&4u)c=lit?((c&~0x180u)|0x60u):((c&~0x1a0u)|0x40u);
    else c=lit?((c&~0x1c0u)|0x20u):(c&~0x1e0u);
    for(unsigned k=0;k<3;++k){
        const auto s=layer_texcoord_40e010(m.sub(8u+k*0x14u,0x14),elements,type);
        const unsigned shift=9u+k*6u;
        c=(c&~(0x3fu<<shift))|((((s[1]&3u)<<4)|(s[0]&0xfu))<<shift);
    }
    return c;
}
std::vector<std::uint32_t> link_vertex_shader_40e140(std::uint32_t c){
    std::array<std::uint32_t,12> decl{},code{};unsigned nd=0,nc=0;
    auto w=pc_shader_data_u32;
    code[0]=w(0x73f004u+(c&7u)*8u);decl[0]=w(0x73f008u+(c&7u)*8u);
    code[1]=w(0x73f02cu+((c>>3)&3u)*8u);decl[1]=w(0x73f030u+((c>>3)&3u)*8u);
    code[2]=w(0x73f044u+((c>>5)&0xfu)*4u);
    nd=2;nc=3;
    const auto a=(c>>9)&0xfu;
    if(a){
        const auto t=(c>>13)&3u;
        code[3]=w(0x73f068u+a*8u);decl[2]=w(0x73f06cu+a*8u);
        code[4]=t==0u?w(0x73f100u):w(0x73f0d0u+t*4u);
        nd=3;nc=5;
    }
    auto layer=[&](std::uint32_t index,std::uint32_t t,std::uint32_t plain,std::uint32_t table){
        if(t==0u){
            if(index!=a){code[nc++]=w(0x73f068u+index*8u);decl[nd++]=w(0x73f06cu+index*8u);}
            code[nc++]=w(plain);
        }else{
            code[nc++]=w(0x73f068u+index*8u);decl[nd++]=w(0x73f06cu+index*8u);
            code[nc++]=w(table+t*4u);
        }
    };
    if(const auto b=(c>>15)&0xfu)layer(b,(c>>19)&3u,0x73f104u,0x73f0e0u);
    if(const auto d=(c>>21)&0xfu)layer(d,(c>>25)&3u,0x73f108u,0x73f0f0u);
    code[nc++]=0x621648u;
    std::vector<std::uint32_t> out{0xfffe0101u};
    auto append=[&](std::uint32_t fragment){
        for(std::uint32_t k=1;;++k){const auto t=w(fragment+k*4u);if(t==End)return;out.push_back(t);}
    };
    for(unsigned k=0;k<nd;++k)append(decl[k]);
    for(unsigned k=0;k<nc;++k)append(code[k]);
    out.push_back(End);
    return out;
}
std::uint32_t shader_cache_40f4e0(PcShaderCache& cache,PcD3D9Device& device,std::uint32_t key,const PcVertexElement* elements,
    const std::uint32_t* tokens,std::uint32_t key3,std::uint32_t& declaration){
    std::uint32_t n=0;while(tokens[n]!=End)++n;
    const auto hash=adler32(tokens,n);
    std::uint32_t k=0;
    for(;std::int32_t(k)<std::int32_t(cache.count);++k){
        auto& e=cache.entries.at(k);
        if(e.key3==key3&&e.key==key&&e.hash==hash){++e.references;declaration=e.declaration;return e.shader;}
    }
    if(k>=cache.entries.size())throw std::length_error("40F4E0 shader cache full");
    auto& e=cache.entries[k];
    e.key=key;e.key3=key3;e.hash=hash;e.references=1;
    e.declaration=device.create_vertex_declaration(elements);
    declaration=e.declaration;
    // On failure the PC disassembles the shader for its debug output only.
    e.shader=device.create_vertex_shader(tokens);
    ++cache.count;if(std::int32_t(cache.count)>std::int32_t(cache.high_water))cache.high_water=cache.count;
    return e.shader;
}
void shader_cache_release_40f650(PcShaderCache& cache,PcD3D9Device& device){
    for(std::uint32_t k=0;std::int32_t(k)<std::int32_t(cache.count);++k){
        auto& e=cache.entries.at(k);
        if(e.shader){device.release(e.shader);e.shader=0;}
        if(e.declaration){device.release(e.declaration);e.declaration=0;}
    }
}
namespace {
void fixed_shader(Bytes rec,std::uint32_t key,std::uint32_t decl_va,std::uint32_t code_pointer,PcD3D9Device& device,PcShaderCache& cache){
    rec.put32(8,key);rec.put32(0xc,0xffffffffu);rec.put32(0x14,1);
    const auto elements=pc_shader_declaration(decl_va);
    const auto tokens=pc_shader_tokens(pc_shader_data_u32(code_pointer));
    std::uint32_t declaration=0;
    const auto shader=shader_cache_40f4e0(cache,device,key,elements.data(),tokens.data(),0xffffffffu,declaration);
    rec.put32(4,declaration);rec.put32(0,shader);
}
void fvf_declaration(std::uint32_t fvf,PcVertexDeclaration& elements,const char* who){
    if(!d3dx_declarator_from_fvf(fvf,elements))
        throw std::runtime_error(std::string(who)+" D3DXDeclaratorFromFVF failed (the PC would use an uninitialised declaration)");
}
// 40FE30: linked shader of the material configuration.
void linked_shader_40fe30(Bytes rec,Bytes material,Bytes group,const PcVertexDeclaration& elements,PcD3D9Device& device,
    PcShaderCache& cache,const PcShaderGlobals& globals){
    const auto key=globals.generated_key_1039ec0;
    rec.put32(8,key);
    rec.put32(0x14,(globals.mode_89edbc!=0u&&globals.override_89edd4==0u)?key:0u);
    const auto config=material_config_40de10(group,material,elements.data());
    rec.put32(0xc,config);
    const auto tokens=link_vertex_shader_40e140(config);
    std::uint32_t declaration=0;
    const auto shader=shader_cache_40f4e0(cache,device,key,elements.data(),tokens.data(),config,declaration);
    rec.put32(4,declaration);rec.put32(0,shader);
}
// 40FE90 (and 4105FA inline): table shader 74209C[type] over the FVF declaration.
void table_shader_40fe90(Bytes rec,std::uint32_t type,std::uint32_t fvf,PcD3D9Device& device,PcShaderCache& cache){
    PcVertexDeclaration elements{};
    fvf_declaration(fvf,elements,"40FE90");
    rec.put32(8,type);rec.put32(0xc,0xffffffffu);rec.put32(0x14,1);
    const auto tokens=pc_shader_tokens(pc_shader_data_u32(0x74209cu+type*4u));
    std::uint32_t declaration=0;
    const auto shader=shader_cache_40f4e0(cache,device,type,elements.data(),tokens.data(),0xffffffffu,declaration);
    rec.put32(4,declaration);rec.put32(0,shader);
}
// 40FEE0: table shader 74209C[type] over the stream-1 declaration.
void stream1_shader_40fee0(Bytes rec,std::uint32_t type,std::uint32_t fvf,PcD3D9Device& device,PcShaderCache& cache){
    PcVertexDeclaration elements{};
    type2_declaration_40fee0(fvf,elements);
    rec.put32(0x14,1);rec.put32(8,type);rec.put32(0xc,0xffffffffu);
    const auto tokens=pc_shader_tokens(pc_shader_data_u32(0x74209cu+type*4u));
    std::uint32_t declaration=0;
    const auto shader=shader_cache_40f4e0(cache,device,type,elements.data(),tokens.data(),0xffffffffu,declaration);
    rec.put32(4,declaration);rec.put32(0,shader);
}
}
void material_setup_40fd70(Bytes rec,Bytes material,Bytes group,PcD3D9Device& device,PcShaderCache& cache,
    const PcShaderGlobals& globals){
    const auto type=group.u32(0x28);
    if(type==1u){fixed_shader(rec,2,0x625c10u,0x7420a4u,device,cache);return;}
    if(type==3u){fixed_shader(rec,3,0x625be0u,0x7420a8u,device,cache);return;}
    PcVertexDeclaration elements{};
    if(type==2u)type2_declaration_4101c0(group.u32(0x20),elements);
    else fvf_declaration(group.u32(0x20),elements,"40FD70");
    linked_shader_40fe30(rec,material,group,elements,device,cache,globals);
}
void material_kind_setup_4104d0(Bytes rec,Bytes material,Bytes group,std::uint32_t kind,PcD3D9Device& device,
    PcShaderCache& cache,const PcShaderGlobals& globals){
    const auto type=group.u32(0x28),fvf=group.u32(0x20);
    switch(kind){
    case 1:{   // 4101C0 (type 2) / 410180
        PcVertexDeclaration elements{};
        if(type==2u)type2_declaration_4101c0(fvf,elements);
        else fvf_declaration(fvf,elements,"410180");
        linked_shader_40fe30(rec,material,group,elements,device,cache,globals);
        return;}
    case 2:fixed_shader(rec,2,0x625c10u,0x7420a4u,device,cache);return;    // 410100
    case 3:fixed_shader(rec,3,0x625be0u,0x7420a8u,device,cache);return;    // 410140
    case 5:
        if(fvf&0x40u){
            if(type==2u){stream1_shader_40fee0(rec,7,fvf,device,cache);return;}
            table_shader_40fe90(rec,(fvf&0x200u)?6u:0xdu,fvf,device,cache);return;
        }
        table_shader_40fe90(rec,(fvf&0x200u)?5u:0xcu,fvf,device,cache);return;
    case 10:table_shader_40fe90(rec,(fvf&0x100u)?0xau:0xbu,fvf,device,cache);return;
    default:table_shader_40fe90(rec,kind,fvf,device,cache);return;          // 4105FA
    }
}
}

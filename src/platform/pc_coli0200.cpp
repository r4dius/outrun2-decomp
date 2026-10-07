#include "platform/course_collision_pack.hpp"
#include <algorithm>
#include <cmath>
#include <bitset>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace outrun::platform {
namespace {
constexpr std::size_t PrefixBytes=4u,HeaderBytes=64u,GridBytes=65536u*2u;
constexpr std::size_t MaxBytes=16u*1024u*1024u;
std::uint32_t u32(const std::uint8_t* p){
    return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8u)|
        (std::uint32_t(p[2])<<16u)|(std::uint32_t(p[3])<<24u);
}
std::uint16_t u16(const std::uint8_t* p){
    return std::uint16_t(p[0])|(std::uint16_t(p[1])<<8u);
}
float f32(const std::uint8_t* p){
    const auto bits=u32(p);float value;std::memcpy(&value,&bits,4u);return value;
}
bool reject(std::string* error,const char* message){
    if(error)*error=message;
    return false;
}
void put16(std::vector<std::uint8_t>& bytes,std::size_t at,std::uint32_t value){
    bytes[at]=static_cast<std::uint8_t>(value);
    bytes[at+1u]=static_cast<std::uint8_t>(value>>8u);
}
void check_open(const CourseCollisionPack& pack){
    const auto& layout=pack.pc_layout;
    if(pack.pc_coli0200.size()!=pack.source_bytes||
       pack.pc_coli0200.size()<PrefixBytes+HeaderBytes+GridBytes||
       layout.polygon_count==0u||pack.quads.size()!=layout.polygon_count||
       pack.primary_ranges.size()!=std::size_t(layout.primary_length_count)*4u)
        throw std::invalid_argument("COLI0200 native tables not admitted");
}
}

bool parse_pc_coli0200(const std::uint8_t* data,std::size_t size,
                       CourseCollisionPack& pack,std::string* error){
    if(error)error->clear();
    if(!data||size<PrefixBytes+HeaderBytes+GridBytes||size>MaxBytes||
       std::uint64_t(u32(data))+PrefixBytes!=size||
       std::memcmp(data+PrefixBytes,"COLI0200",8u)!=0||u32(data+20u)!=64u)
        return reject(error,"COLI0200 size/header mismatch");
    CourseCollisionLayout layout{};
    layout.polygon_count=u32(data+12u);
    layout.primary_polygon_count=u32(data+16u);
    if(layout.polygon_count==0u||layout.polygon_count>100000u||
       layout.primary_polygon_count>layout.polygon_count||
       layout.primary_polygon_count>65535u)
        return reject(error,"COLI0200 polygon counts outside bounds");
    for(std::size_t i=0;i<layout.sections.size();++i)
        layout.sections[i]=u32(data+24u+i*4u);
    const auto& sections=layout.sections;
    if(sections[0]!=HeaderBytes+GridBytes)
        return reject(error,"COLI0200 grid/area boundary mismatch");
    for(std::size_t i=0;i<sections.size();++i){
        if(std::uint64_t(sections[i])+PrefixBytes>size||
           (i!=0u&&sections[i]<sections[i-1u]))
            return reject(error,"COLI0200 section outside source");
    }
    const auto room=[&](std::size_t section,std::uint64_t bytes){
        const std::size_t end=section+1u<sections.size()?sections[section+1u]:size-PrefixBytes;
        return bytes<=end-sections[section];
    };
    const auto n=layout.polygon_count;
    if(!room(0u,2u)||!room(1u,n)||!room(2u,std::uint64_t(n)*64u)||
       !room(3u,std::uint64_t(n)*48u)||!room(4u,n)||!room(5u,std::uint64_t(n)*2u))
        return reject(error,"COLI0200 truncated native table");

    // Validate every reachable list once. A grid cell stores a WORD offset,
    // not a polygon index, byte address or host pointer. Zero still has to
    // be backed because the recovered query reads it before its no-hit gate.
    std::bitset<65536u> checked{};
    const auto* grid=data+PrefixBytes+HeaderBytes;
    const auto* lists=data+PrefixBytes+sections[0];
    const auto list_bytes=sections[1]-sections[0];
    for(std::size_t cell=0;cell<65536u;++cell){
        const auto word=u16(grid+cell*2u);
        if(checked[word])continue;
        checked.set(word);
        const std::size_t at=std::size_t(word)*2u;
        if(at>list_bytes||list_bytes-at<2u)
            return reject(error,"COLI0200 grid points outside area lists");
        const auto count=u16(lists+at);
        if(std::size_t(count)*2u>list_bytes-at-2u)
            return reject(error,"COLI0200 area list overruns section");
        for(std::size_t i=0;i<count;++i)
            if(u16(lists+at+2u+i*2u)>=n)
                return reject(error,"COLI0200 area list has invalid polygon");
        if(word!=0u)++layout.referenced_area_lists;
    }

    // PC 0x43DCD2..0x43DD78 builds the [first,last] pair per course length
    // (root 0x780218) from the primary prefix. A length that is not the
    // current one closes the open pair; a greater one opens the next length
    // (one step at a time); a smaller one only clears 0x780190. Background
    // roots (e.g. \BK\coli_LBK_PALM, whose last entry drops to 8) are
    // therefore accepted as on PC. Only lane 0 uses the table there.
    const auto* lengths=data+PrefixBytes+sections[5];
    std::vector<std::uint8_t> ranges;
    if(layout.primary_polygon_count!=0u){
        if(u16(lengths)!=0u)
            return reject(error,"COLI0200 primary length domain does not start at zero");
        std::uint32_t current=0u;bool open=true;
        std::vector<std::uint32_t> pairs{0u,0u};
        for(std::uint32_t i=0u;i<layout.primary_polygon_count;++i){
            const auto length=u16(lengths+i*2u);
            if(open&&length!=current){pairs[current*2u+1u]=i-1u;open=false;}
            if(length<current)layout.primary_lengths_ordered=false;
            if(length>current){
                ++current;pairs.resize(std::size_t(current)*2u+2u,0u);pairs[current*2u]=i;open=true;
            }
        }
        pairs[current*2u+1u]=layout.primary_polygon_count-1u;
        if(current>32767u)
            return reject(error,"COLI0200 primary length outside signed query domain");
        layout.primary_length_count=current+1u;
        ranges.resize(std::size_t(layout.primary_length_count)*4u);
        for(std::uint32_t k=0;k<layout.primary_length_count;++k){
            put16(ranges,std::size_t(k)*4u,pairs[k*2u]);put16(ranges,std::size_t(k)*4u+2u,pairs[k*2u+1u]);
        }
    }

    CourseCollisionPack next{};
    next.pc_layout=layout;
    next.primary_ranges=std::move(ranges);
    next.source_bytes=static_cast<std::uint32_t>(size);
    next.source_quad_offset=sections[2];
    next.quads.resize(n);
    for(std::size_t i=0;i<n;++i){
        const auto* polygon=data+PrefixBytes+sections[2]+i*64u;
        const auto* normals=data+PrefixBytes+sections[3]+i*48u;
        auto& quad=next.quads[i];
        quad.flags=u16(polygon+60u);
        quad.material=1u<<(data[PrefixBytes+sections[1]+i]&31u);
        for(unsigned vertex=0;vertex<4u;++vertex){
            for(unsigned axis=0;axis<3u;++axis){
                const auto at=(vertex*3u+axis)*4u;
                quad.vertices[vertex][axis]=f32(polygon+at);
                if(!std::isfinite(quad.vertices[vertex][axis])||!std::isfinite(f32(normals+at)))
                    return reject(error,"COLI0200 non-finite geometry/normal");
            }
        }
        // Source +0x30/+0x34/+0x38 is the XYZ center, not YZ.
        quad.center_xz={{f32(polygon+48u),f32(polygon+56u)}};
        if(!std::isfinite(quad.center_xz[0])||!std::isfinite(quad.center_xz[1]))
            return reject(error,"COLI0200 non-finite center");
    }
    next.pc_coli0200.assign(data,data+size);
    pack=std::move(next);
    return true;
}

driving::CourseCollisionTables course_collision_tables(
    const CourseCollisionPack& pack,std::uint32_t load_type){
    check_open(pack);
    if(load_type>=4u)throw std::out_of_range("COLI0200 lane outside four PC roots");
    const auto& layout=pack.pc_layout;
    const auto& s=layout.sections;
    const auto count=layout.polygon_count;
    const driving::Bytes raw(const_cast<std::uint8_t*>(pack.pc_coli0200.data()),pack.pc_coli0200.size());
    const driving::Bytes ranges(const_cast<std::uint8_t*>(pack.primary_ranges.data()),pack.primary_ranges.size());
    return {{raw.sub(PrefixBytes,HeaderBytes),raw.sub(PrefixBytes+s[5],std::size_t(count)*2u),
              ranges,static_cast<std::int32_t>(layout.primary_length_count),false,true},
            raw.sub(PrefixBytes+s[1],count),raw.sub(PrefixBytes+s[2],std::size_t(count)*64u),
            raw.sub(PrefixBytes+s[3],std::size_t(count)*48u),raw.sub(PrefixBytes+s[0],s[1]-s[0]),
            load_type,true};
}

driving::Bytes course_collision_grid(const CourseCollisionPack& pack){
    check_open(pack);
    return driving::Bytes(const_cast<std::uint8_t*>(pack.pc_coli0200.data()),pack.pc_coli0200.size())
        .sub(PrefixBytes+HeaderBytes,GridBytes);
}
} // namespace outrun::platform

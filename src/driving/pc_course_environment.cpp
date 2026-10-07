#include "driving/pc_course_environment.hpp"
#include <cmath>
#include <stdexcept>
#include <vector>

namespace outrun::driving {
namespace {
constexpr std::size_t ListStride=0xb0u,SplineStride=0x2cu;
constexpr float SplineSkip=-100.0f,SplineEnd=-999.9f;
}
PcEnvironmentLayout inspect_course_environment(const PcEnvironmentPayloads& payloads){
    PcEnvironmentLayout result{};
    for(std::size_t lane=0;lane<2u;++lane){
        const auto bytes=payloads[lane];
        if(bytes.size()==0u)continue;
        bytes.check(0u,12u);
        for(std::size_t slot=0;slot<3u;++slot){
            auto& list=result.lists[lane][slot];
            list.present=true;list.offset=bytes.u32(slot*4u);
            // Native admission deliberately excludes self-modifying headers.
            // It is a safety restriction, not a new interpretation of zero.
            if(list.offset<12u)throw std::invalid_argument("environment list overlaps its root header");
            std::size_t offset=list.offset;
            for(;;offset+=ListStride){
                const auto tag=static_cast<std::uint16_t>(bytes.i16(offset));
                if(tag==0xffffu)break;
                // Every nonterminal entry advances by the full original stride.
                bytes.check(offset,ListStride);
                ++list.records;
                if(tag==0xfffeu)++list.skipped;
                else ++list.active;
            }
        }
        // Exact aliases and suffix lists are valid. Incongruent overlapping
        // records could let one +8 store overwrite another list's tag/end
        // marker and invalidate the previously checked walk; reject them.
        for(std::size_t a=0;a<3u;++a)for(std::size_t b=a+1u;b<3u;++b){
            const auto& x=result.lists[lane][a];const auto& y=result.lists[lane][b];
            const auto end_x=std::size_t(x.offset)+x.records*ListStride+2u;
            const auto end_y=std::size_t(y.offset)+y.records*ListStride+2u;
            if(std::size_t(x.offset)<end_y&&std::size_t(y.offset)<end_x&&
               x.offset%ListStride!=y.offset%ListStride)
                throw std::invalid_argument("environment lists have incompatible overlapping strides");
        }
    }
    const auto spline=payloads[2];
    if(spline.size()!=0u){
        result.spline_present=true;
        for(std::size_t offset=0;;offset+=SplineStride){
            const float marker=spline.f32(offset+0x0cu);
            if(marker==SplineEnd)break;
            spline.check(offset,SplineStride);
            ++result.spline_records;
            if(marker==SplineSkip)++result.spline_skipped;
            else {
                ++result.spline_active;
                for(std::size_t k=0;k<3u;++k)
                    if(!std::isfinite(spline.f32(offset+0x10u+k*4u)))
                        throw std::invalid_argument("nonfinite environment spline position");
            }
        }
    }
    return result;
}
PcEnvironmentLayout course_environment_init_44a940(
    const PcEnvironmentPayloads& payloads,Bytes primary_matrix){
    const auto layout=inspect_course_environment(payloads);
    primary_matrix.check(0u,64u);
    for(std::size_t k=0;k<16u;++k)
        if(!std::isfinite(primary_matrix.f32(k*4u)))
            throw std::invalid_argument("nonfinite course environment matrix");
    // Calculate before publishing any mutation. A bad homogeneous coordinate
    // must not leave half-initialized fog/sun records behind.
    struct Write {std::size_t offset;CourseProbe point;};
    std::vector<Write> transformed;
    transformed.reserve(layout.spline_active);
    const auto spline=payloads[2];
    for(std::size_t i=0;i<layout.spline_records;++i){
        const auto offset=i*SplineStride;
        if(spline.f32(offset+0x0cu)==SplineSkip)continue;
        const auto point=pc_transform_point(primary_matrix,
            {spline.f32(offset+0x10u),spline.f32(offset+0x14u),spline.f32(offset+0x18u)});
        if(!std::isfinite(point.x)||!std::isfinite(point.y)||!std::isfinite(point.z))
            throw std::invalid_argument("nonfinite transformed environment position");
        transformed.push_back({offset+0x10u,point});
    }
    // The PC alternates fog/sun for each slot. Preserve even aliased list order.
    for(std::size_t slot=0;slot<3u;++slot)for(std::size_t lane=0;lane<2u;++lane){
        const auto& list=layout.lists[lane][slot];
        const auto bytes=payloads[lane];
        for(std::size_t i=0;i<list.records;++i){
            const auto offset=std::size_t(list.offset)+i*ListStride;
            if(static_cast<std::uint16_t>(bytes.i16(offset))!=0xfffeu)
                bytes.put32(offset+0x08u,0x7f7fffffu); // original 0x59943C, FLT_MAX
        }
    }
    for(const auto& write:transformed){
        spline.putf(write.offset,write.point.x);
        spline.putf(write.offset+4u,write.point.y);
        spline.putf(write.offset+8u,write.point.z);
    }
    return layout;
}
void course_environment_finish_44aa80(std::uint32_t mode,
    std::array<std::uint32_t,6>& flags,std::uint32_t& phase){
    if(mode==13u||mode==14u){flags.fill(1u);phase=3u;}
    else phase=0u;
}
} // namespace outrun::driving

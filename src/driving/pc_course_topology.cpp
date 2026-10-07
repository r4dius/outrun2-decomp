#include "driving/pc_course_topology.hpp"
#include <algorithm>
#include <stdexcept>
namespace outrun::driving {
std::int32_t find_primary_course_run(const CourseRunTables& t,std::int16_t requested,
                                    std::int32_t hint,std::int32_t& first,std::int32_t& last){
    if(!t.present)return 0;
    if(t.total_length<=0)throw std::invalid_argument("course length domain is empty");
    const std::int32_t target=std::min<std::int32_t>(requested,t.total_length-1);
    const std::int32_t count=t.header.i32(0x0c);
    if(hint>=count)hint=-1;
    if(t.force_ranges||hint==-1){
        if(target<0)throw std::out_of_range("negative course range index");
        const auto off=static_cast<std::size_t>(target)*4u;
        t.ranges.check(off,4);
        // Pre-read both values so native rejected views cause no partial write.
        const auto a=static_cast<std::uint16_t>(t.ranges.i16(off));
        const auto b=static_cast<std::uint16_t>(t.ranges.i16(off+2));
        last=b;first=a;return last-first+1;
    }
    if(hint<0)throw std::out_of_range("invalid negative course hint");
    if(count<0)throw std::out_of_range("negative course polygon count");
    t.lengths.check(0,static_cast<std::size_t>(count)*2u);
    auto length=[&](std::int32_t i){return static_cast<std::int32_t>(static_cast<std::uint16_t>(t.lengths.i16(static_cast<std::size_t>(i)*2u)));};
    const auto current=length(hint);
    std::int32_t begin=hint,end=hint;
    if(current==target){
        while(begin>0&&length(begin-1)==target)--begin;
        while(end<count-1&&length(end+1)==target)++end;
    }else if(current<target){
        begin=hint+1;
        while(begin<count&&length(begin)!=target)++begin;
        if(begin>=count)return 0;
        end=begin;
        while(end<count-1&&length(end+1)==target)++end;
    }else{
        end=hint;
        while(end>=0&&length(end)!=target)--end;
        if(end<0)return 0;
        begin=end;
        while(begin>0&&length(begin-1)==target)--begin;
    }
    first=begin;last=end;return last-first+1;
}
}

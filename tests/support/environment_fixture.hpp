#pragma once
#include "driving/pc_course_environment.hpp"
#include <array>
#include <cstring>
#include <vector>

namespace outrun::test {
using EnvironmentFixture=std::array<std::vector<std::uint8_t>,3>;
inline driving::PcEnvironmentPayloads environment_views(EnvironmentFixture& data){
    return {{driving::Bytes(data[0].data(),data[0].size()),
             driving::Bytes(data[1].data(),data[1].size()),
             driving::Bytes(data[2].data(),data[2].size())}};
}
inline std::array<std::uint8_t,64> environment_matrix(){
    std::array<std::uint8_t,64> result{};driving::Bytes bytes(result.data(),result.size());
    for(unsigned i=0;i<4;++i)bytes.putf(i*20u,1.0f);
    return result;
}
inline EnvironmentFixture environment_fixture(unsigned seed,bool active_spline){
    EnvironmentFixture data{};
    for(unsigned lane=0;lane<2;++lane){
        if(((seed+lane)%11u)==0u)continue;
        auto& bytes=data[lane];bytes.assign(12u,0u);
        for(unsigned slot=0;slot<3;++slot){
            const auto start=bytes.size();
            const auto count=(seed+lane*3u+slot)%7u;
            bytes.resize(start+(count+1u)*0xb0u,0x53u);
            driving::Bytes view(bytes.data(),bytes.size());view.put32(slot*4u,static_cast<std::uint32_t>(start));
            for(unsigned i=0;i<count;++i){
                view.put16(start+i*0xb0u,((seed+i+slot)%3u)==0u?0xfffeu:std::uint16_t(i+1u));
                view.put32(start+i*0xb0u+8u,0xdead0000u+seed+i);
            }
            view.put16(start+count*0xb0u,0xffffu);
        }
        if(seed%4u==0u){
            driving::Bytes view(bytes.data(),bytes.size());view.put32(8u,view.u32(0u));
        }else if(seed%4u==1u){
            driving::Bytes view(bytes.data(),bytes.size());
            const auto first=view.u32(0u);const auto count=(seed+lane*3u)%7u;
            view.put32(8u,first+(count==0u?0u:0xb0u));
        }
    }
    if(seed%5u!=0u){
        const auto count=seed%5u;
        auto& bytes=data[2];bytes.assign((count+1u)*0x2cu,0x39u);
        driving::Bytes view(bytes.data(),bytes.size());
        for(unsigned i=0;i<count;++i){
            view.putf(i*0x2cu+0x0cu,active_spline&&(i%2u)==0u?7.0f:-100.0f);
            view.putf(i*0x2cu+0x10u,float(i)+2.0f);
            view.putf(i*0x2cu+0x14u,float(i)-3.0f);
            view.putf(i*0x2cu+0x18u,float(i)+5.0f);
        }
        view.putf(count*0x2cu+0x0cu,-999.9f);
    }
    return data;
}
inline std::vector<std::uint8_t> environment_raw(const std::vector<std::uint8_t>& payload){
    std::vector<std::uint8_t> raw(4u+payload.size());
    driving::Bytes(raw.data(),raw.size()).put32(0u,static_cast<std::uint32_t>(payload.size()));
    if(!payload.empty())std::memcpy(raw.data()+4u,payload.data(),payload.size());
    return raw;
}
} // namespace outrun::test

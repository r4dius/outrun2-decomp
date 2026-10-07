#include "platform/course_environment_runtime.hpp"
#include <algorithm>
#include <exception>
#include <stdexcept>
#include <utility>

namespace outrun::platform {
namespace {
bool reject(std::string* error,const char* message){if(error)*error=message;return false;}
driving::PcEnvironmentPayloads views(std::array<std::vector<std::uint8_t>,3>& data){
    return {{driving::Bytes(data[0].data(),data[0].size()),
             driving::Bytes(data[1].data(),data[1].size()),
             driving::Bytes(data[2].data(),data[2].size())}};
}
}
CourseEnvironmentRuntime::CourseEnvironmentRuntime(CourseEnvironmentRuntime&& other) noexcept {
    *this=std::move(other);
}
CourseEnvironmentRuntime& CourseEnvironmentRuntime::operator=(CourseEnvironmentRuntime&& other) noexcept {
    if(this==&other)return *this;
    source_=std::move(other.source_);runtime_=std::move(other.runtime_);
    identities_=other.identities_;admitted_=other.admitted_;
    layout_=other.layout_;initialized_=other.initialized_;
    other.reset();return *this;
}
bool CourseEnvironmentRuntime::admit_lane(std::uint32_t lane,const std::uint8_t* raw,
    std::size_t size,CourseWorldSourceIdentity identity,std::string* error){
    if(error)error->clear();
    if(lane>=3u||!raw||size<4u||size>2u*1024u*1024u)
        return reject(error,"environment source size/lane outside bounds");
    try {
        driving::Bytes header(const_cast<std::uint8_t*>(raw),size);
        if(header.u32(0u)!=size-4u||size==4u)
            return reject(error,"environment size prefix mismatch or empty payload");
        std::array<std::vector<std::uint8_t>,3> trial{};
        trial[lane].assign(raw+4u,raw+size);
        driving::inspect_course_environment(views(trial));
        source_[lane]=std::move(trial[lane]);identities_[lane]=identity;admitted_[lane]=true;
        runtime_={};layout_={};initialized_=false;
        return true;
    }catch(const std::exception& e){return reject(error,e.what());}
}
bool CourseEnvironmentRuntime::admit_absent_lane(std::uint32_t lane,std::string* error){
    if(error)error->clear();
    if(lane>=3u)return reject(error,"environment absent lane outside bounds");
    source_[lane].clear();identities_[lane]={};admitted_[lane]=true;
    runtime_={};layout_={};initialized_=false;return true;
}
bool CourseEnvironmentRuntime::initialize(const std::array<std::uint8_t,64>& matrix,
    std::string* error){
    if(error)error->clear();
    if(!inputs_ready())return reject(error,"environment lanes not yet admitted");
    try {
        auto candidate=source_;
        const auto layout=driving::course_environment_init_44a940(views(candidate),
            driving::Bytes(const_cast<std::uint8_t*>(matrix.data()),matrix.size()));
        runtime_=std::move(candidate);layout_=layout;initialized_=true;return true;
    }catch(const std::exception& e){return reject(error,e.what());}
}
bool CourseEnvironmentRuntime::update_frame(driving::Bytes vehicle,driving::Bytes camera,
    const CourseWorldRuntime& world,const driving::PcEnvironmentTransition& transition,
    std::array<std::uint32_t,6>& flags,std::uint32_t& phase,CourseEnvironmentLiveState& live,
    std::string* error){
    if(error)error->clear();
    if(!initialized_)return reject(error,"environment records not initialized");
    if(vehicle.size()<0x66u||camera.size()<0x110u)return reject(error,"environment vehicle/camera view too small");
    if(!world.query_ready()||!world.lane_loaded(0u))return reject(error,"primary collision course not loaded");
    try {
        const auto tables=world.tables();
        if(tables.transforms[0].size()!=64u)return reject(error,"primary course matrix 7D2DA0 unavailable");
        // Stage every mutable root; publish only when the whole frame succeeds.
        auto records=runtime_;auto staged=live;auto staged_flags=flags;
        std::array<std::uint8_t,64> matrix{};
        for(std::size_t k=0;k<matrix.size();++k)matrix[k]=tables.transforms[0].u8(k);
        std::array<std::uint8_t,4*64> arena{};
        driving::PcMatrixStack matrices{driving::Bytes(arena.data(),arena.size()),0,0,4};
        driving::PcEnvironmentBlendContext context{phase,staged.time_7d2934,staged.duration_7d28d8,
            driving::Bytes(staged.saved_sun_7d26d0.data(),staged.saved_sun_7d26d0.size()),
            driving::Bytes(staged.saved_fog_7d28e0.data(),staged.saved_fog_7d28e0.size()),
            driving::Bytes(staged.lights_899b98.data(),3u*0xa0u),
            driving::Bytes(staged.fog_7d3a10.data(),staged.fog_7d3a10.size()),
            driving::Bytes(matrix.data(),matrix.size()),matrices};
        driving::PcEnvironmentFrame frame{};
        for(std::uint32_t slot=0;slot<3u;++slot)for(std::uint32_t lane=0;lane<2u;++lane){
            const auto& list=layout_.lists[lane][slot];
            if(!list.present)continue;
            auto& payload=records[lane];
            const driving::Bytes view(payload.data()+list.offset,payload.size()-list.offset);
            (lane?frame.sun_lists:frame.fog_lists)[slot]=view;
        }
        if(layout_.spline_present)frame.light_list=driving::Bytes(records[2].data(),records[2].size());
        frame.vehicle=vehicle;frame.camera=camera;
        frame.flags_7d28b0=driving::Bytes(reinterpret_cast<std::uint8_t*>(staged_flags.data()),24u);
        frame.nearest_7d2d58=driving::Bytes(staged.nearest_7d2d58.data(),staged.nearest_7d2d58.size());
        frame.lights_899d78=driving::Bytes(staged.lights_899b98.data()+3u*0xa0u,6u*0xa0u);
        driving::course_environment_update_44a8df(frame,tables.courses[0],transition,context);
        staged.time_7d2934=context.time_7d2934;staged.duration_7d28d8=context.duration_7d28d8;
        runtime_=std::move(records);live=staged;flags=staged_flags;phase=context.phase_7d28c8;
        return true;
    }catch(const std::exception& e){return reject(error,e.what());}
}
void CourseEnvironmentRuntime::reset(){
    source_={};runtime_={};identities_={};admitted_={};layout_={};initialized_=false;
}
bool CourseEnvironmentRuntime::inputs_ready() const {
    return std::all_of(admitted_.begin(),admitted_.end(),[](bool value){return value;});
}
bool CourseEnvironmentRuntime::lane_admitted(std::uint32_t lane) const {
    return lane<3u&&admitted_[lane];
}
const std::vector<std::uint8_t>& CourseEnvironmentRuntime::payload(std::uint32_t lane) const {
    if(lane>=3u||!initialized_)throw std::logic_error("environment payload not initialized");
    return runtime_[lane];
}
const CourseWorldSourceIdentity& CourseEnvironmentRuntime::source_identity(std::uint32_t lane) const {
    if(!lane_admitted(lane))throw std::logic_error("environment source not admitted");
    return identities_[lane];
}
} // namespace outrun::platform

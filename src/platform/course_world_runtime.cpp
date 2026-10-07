#include "platform/course_world_runtime.hpp"
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace outrun::platform {
namespace {
bool reject(std::string* error,const char* text){
    if(error)*error=text;
    return false;
}
}
CourseWorldRuntime::CourseWorldRuntime(CourseWorldRuntime&& other) noexcept {
    *this=std::move(other);
}
CourseWorldRuntime& CourseWorldRuntime::operator=(CourseWorldRuntime&& other) noexcept {
    if(this==&other)return *this;
    lanes_=std::move(other.lanes_);identities_=other.identities_;loaded_=other.loaded_;
    transforms_=other.transforms_;transform_known_=other.transform_known_;
    prediction_=other.prediction_;generation_=other.generation_;
    other.loaded_={};other.transform_known_={};other.prediction_={};
    other.identities_={};++other.generation_;
    return *this;
}
bool CourseWorldRuntime::admit_lane(std::uint32_t index,const std::uint8_t* data,
                                    std::size_t size,CourseWorldSourceIdentity identity,
                                    std::string* error){
    if(error)error->clear();
    if(index>=lanes_.size())return reject(error,"collision lane outside four PC roots");
    CourseCollisionPack candidate{};
    if(!parse_pc_coli0200(data,size,candidate,error))return false;
    if(index==0u&&candidate.pc_layout.primary_polygon_count==0u)
        return reject(error,"primary collision root has no primary road");
    lanes_[index]=std::move(candidate);identities_[index]=identity;loaded_[index]=true;
    // A replacement source must receive its real transform again. Nonzero
    // roots share group 1, just as the recovered world-query code does.
    transform_known_[index==0u?0u:1u]=false;
    prediction_={};++generation_;
    // 43D470(lane): the header's record count - 1 (16-bit), 0 without a root
    auto last=[&](std::uint32_t k)->std::int32_t{
        if(!loaded_[k])return 0;const auto t=course_collision_tables(lanes_[k],k);const auto& h=t.runs.header;
        if(h.size()<0x10u)return 0;return std::int32_t(std::uint16_t(h.u32(0xcu)-1u));};
    if(index==0u){const auto n=last(1);if(n)length_offsets_[0]=length_offsets_[1]+n+1;}
    else if(index==1u){const auto n=last(0);if(n){length_offsets_[1]=length_offsets_[0]+n+1;length_offsets_[2]=length_offsets_[1];}}
    return true;
}
bool CourseWorldRuntime::set_transform(std::uint32_t group,
    const std::array<float,16>& matrix,std::string* error){
    if(error)error->clear();
    if(group>=transforms_.size())return reject(error,"collision transform group outside bounds");
    for(float value:matrix)if(!std::isfinite(value))
        return reject(error,"collision transform contains a non-finite value");
    // Use the existing byte view to preserve the PC's explicit little-endian
    // representation rather than storing architecture-dependent host pointers.
    auto next=transforms_[group];
    driving::Bytes bytes(next.data(),next.size());
    for(std::size_t i=0;i<matrix.size();++i)bytes.putf(i*4u,matrix[i]);
    transforms_[group]=next;transform_known_[group]=true;
    prediction_={};++generation_;
    return true;
}
void CourseWorldRuntime::reset(){
    lanes_={};identities_={};loaded_={};transforms_={};transform_known_={};length_offsets_={};
    prediction_={};++generation_;
}
void CourseWorldRuntime::release_lane(std::uint32_t index){   // course-prog: PC 0x43DB00
    if(index>=lanes_.size()||!loaded_[index])return;
    lanes_[index]={};identities_[index]={};loaded_[index]=false;length_offsets_[index]=0;
    prediction_={};++generation_;
}
bool CourseWorldRuntime::lane_loaded(std::uint32_t index) const {
    return index<lanes_.size()&&loaded_[index];
}
bool CourseWorldRuntime::query_ready() const {
    bool any=false;
    for(std::size_t i=0;i<loaded_.size();++i)if(loaded_[i]){
        any=true;if(!transform_known_[i==0u?0u:1u])return false;
    }
    return any;
}
const CourseCollisionPack& CourseWorldRuntime::lane(std::uint32_t index) const {
    if(!lane_loaded(index))throw std::out_of_range("collision lane not loaded");
    return lanes_[index];
}
const CourseWorldSourceIdentity& CourseWorldRuntime::source_identity(std::uint32_t index) const {
    if(!lane_loaded(index))throw std::out_of_range("collision lane not loaded");
    return identities_[index];
}
driving::CourseWorldTables CourseWorldRuntime::tables() const {
    if(!query_ready())throw std::logic_error("collision world transform not initialized");
    driving::CourseWorldTables result{};
    for(std::uint32_t i=0;i<lanes_.size();++i)if(loaded_[i]){
        result.courses[i]=course_collision_tables(lanes_[i],i);
        result.grids[i]=course_collision_grid(lanes_[i]);result.grids_present[i]=true;
    }
    for(std::size_t i=0;i<transforms_.size();++i)if(transform_known_[i])
        result.transforms[i]=driving::Bytes(const_cast<std::uint8_t*>(transforms_[i].data()),64u);
    return result;
}
bool CourseWorldRuntime::ground_at(float x,float z,float height,CourseWorldGroundSample& sample){
    if(!query_ready()||!std::isfinite(x)||!std::isfinite(z)||!std::isfinite(height))return false;
    const auto previous_prediction=prediction_;
    try{
        auto views=tables();
        std::array<std::uint8_t,128> storage{};
        driving::PcMatrixStack matrices{driving::Bytes(storage.data(),storage.size()),0,0,2};
        driving::pc_matrix_identity(matrices);
        driving::CourseWorldQuery query{views,matrices,prediction_};
        driving::CourseProbe point{x,height,z};
        std::uint32_t polygon=0xffffffffu,kind=1u;
        const auto index=driving::get_y_position_spl_chk(query,point,&polygon,nullptr,&kind);
        if(kind==1u||!lane_loaded(index)||polygon>=lanes_[index].quads.size()||
           !std::isfinite(point.y))return false;
        const auto normal=driving::course_collision_world_normal(views.courses[index],
            static_cast<std::int32_t>(polygon),matrices,views.transforms[index==0u?0u:1u]);
        if(!std::isfinite(normal.x)||!std::isfinite(normal.y)||!std::isfinite(normal.z)){
            prediction_=previous_prediction;return false;
        }
        sample={{point.y,{normal.x,normal.y,normal.z},polygon,kind},index,
                lanes_[index].quads[polygon].material};
        return true;
    }catch(const std::exception&){
        prediction_=previous_prediction;
        return false;
    }
}
std::array<float,16> course_world_identity_transform(){
    std::array<float,16> matrix{};
    for(std::size_t i=0;i<4u;++i)matrix[i*5u]=1.0f;
    return matrix;
}
} // namespace outrun::platform

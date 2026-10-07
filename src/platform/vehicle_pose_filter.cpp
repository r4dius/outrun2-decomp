#include "platform/vehicle_pose_filter.hpp"
#include <algorithm>
#include <cmath>

namespace outrun::platform {
namespace {
constexpr float MaximumNormalStep=0.018f;
constexpr float HeightResponse=0.15f;
constexpr float MaximumHeightStep=0.025f;

bool normalized(std::array<float,3> value,std::array<float,3>& result){
    const float length=std::sqrt(value[0]*value[0]+value[1]*value[1]+value[2]*value[2]);
    if(!std::isfinite(length)||length<=1.0e-6f)return false;
    for(unsigned axis=0;axis<3u;++axis)result[axis]=value[axis]/length;
    return result[1]>=0.0f;
}
float dot(const std::array<float,3>& a,const std::array<float,3>& b){
    return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];
}
}

bool initialize_vehicle_pose_filter(VehiclePoseFilterState& state,
                                    float ground_height,
                                    const std::array<float,3>& normal){
    std::array<float,3> unit{};
    if(!std::isfinite(ground_height)||!normalized(normal,unit))return false;
    state=VehiclePoseFilterState{};state.ground_height=ground_height;
    state.normal=unit;state.initialized=true;return true;
}

bool update_vehicle_pose_filter(VehiclePoseFilterState& state,
                                float target_ground_height,
                                const std::array<float,3>& target_normal){
    if(!state.initialized)return initialize_vehicle_pose_filter(
        state,target_ground_height,target_normal);
    std::array<float,3> target{};
    if(!std::isfinite(target_ground_height)||!normalized(target_normal,target))return false;
    const float cosine=std::clamp(dot(state.normal,target),-1.0f,1.0f);
    const float target_angle=std::acos(cosine);
    state.max_target_angle=std::max(state.max_target_angle,target_angle);
    const float applied_angle=std::min(target_angle,MaximumNormalStep);
    if(target_angle>MaximumNormalStep)++state.normal_limited;
    if(target_angle>1.0e-6f){
        const float sine=std::sin(target_angle);
        if(std::fabs(sine)>1.0e-6f){
            const float alpha=applied_angle/target_angle;
            const float from_weight=std::sin((1.0f-alpha)*target_angle)/sine;
            const float to_weight=std::sin(alpha*target_angle)/sine;
            std::array<float,3> next{{
                from_weight*state.normal[0]+to_weight*target[0],
                from_weight*state.normal[1]+to_weight*target[1],
                from_weight*state.normal[2]+to_weight*target[2]}};
            if(!normalized(next,state.normal))return false;
        }else state.normal=target;
    }
    state.max_applied_angle=std::max(state.max_applied_angle,applied_angle);
    const float desired_height_step=(target_ground_height-state.ground_height)*HeightResponse;
    const float height_step=std::clamp(
        desired_height_step,-MaximumHeightStep,MaximumHeightStep);
    if(std::fabs(desired_height_step)>MaximumHeightStep)++state.height_limited;
    state.ground_height+=height_step;
    state.max_applied_height=std::max(state.max_applied_height,std::fabs(height_step));
    ++state.updates;
    return true;
}

} // namespace outrun::platform

#include "platform/vehicle_road_contact.hpp"
#include "platform/vehicle_visual.hpp"
#include <algorithm>
#include <cmath>

namespace outrun::platform {
namespace {

template<class GroundQuery>
bool sample_contacts(GroundQuery& query,
                                  const std::array<float,3>& body_position,
                                  float yaw,
                                  VehicleRoadContactFrame& frame){
    frame=VehicleRoadContactFrame{};
    if(!std::isfinite(yaw))return false;
    for(float value:body_position)if(!std::isfinite(value))return false;
    const float sy=std::sin(yaw),cy=std::cos(yaw);
    const std::array<float,3> right{{cy,0.0f,sy}};
    const std::array<float,3> back{{-sy,0.0f,cy}};
    std::array<float,3> normal_sum{{0.0f,0.0f,0.0f}};
    for(std::size_t i=0;i<frame.wheels.size();++i){
        const auto& local=VehicleVisualObjectTranslations[i+1u];
        auto& wheel=frame.wheels[i];
        wheel.world_position[0]=body_position[0]+right[0]*local[0]+back[0]*local[2];
        wheel.world_position[2]=body_position[2]+right[2]*local[0]+back[2]*local[2];
        CourseWorldGroundSample hit{};
        if(!query(wheel.world_position[0],wheel.world_position[2],body_position[1],hit))continue;
        const auto& sample=hit.ground;
        wheel.supported=true;
        wheel.world_position[1]=sample.height;
        wheel.normal=sample.normal;
        wheel.quad_index=sample.quad_index;
        wheel.material=hit.material;
        wheel.course_lane=hit.lane;
        frame.hit_mask|=1u<<i;
        ++frame.hit_count;
        if(frame.hit_count==1u){frame.primary_quad=sample.quad_index;frame.primary_lane=hit.lane;}
        for(unsigned axis=0;axis<3u;++axis)normal_sum[axis]+=sample.normal[axis];
    }
    if(frame.hit_count<3u)return false;
    const float length=std::sqrt(normal_sum[0]*normal_sum[0]+normal_sum[1]*normal_sum[1]+
                                 normal_sum[2]*normal_sum[2]);
    if(!std::isfinite(length)||length<=1.0e-6f)return false;
    for(unsigned axis=0;axis<3u;++axis)frame.normal[axis]=normal_sum[axis]/length;
    if(frame.normal[1]<0.05f)return false;

    // Each sample implies the plane's height at the body center. Averaging
    // those values preserves a slope while allowing small independent wheel
    // travel over non-planar collision quads.
    float center_height=0.0f;
    for(const auto& wheel:frame.wheels)if(wheel.supported){
        const float dx=wheel.world_position[0]-body_position[0];
        const float dz=wheel.world_position[2]-body_position[2];
        center_height+=wheel.world_position[1]+
            (frame.normal[0]*dx+frame.normal[2]*dz)/frame.normal[1];
    }
    frame.ground_height=center_height/float(frame.hit_count);
    if(!std::isfinite(frame.ground_height))return false;
    for(auto& wheel:frame.wheels){
        if(!wheel.supported){wheel.suspension_delta=-0.15f;continue;}
        const float dx=wheel.world_position[0]-body_position[0];
        const float dz=wheel.world_position[2]-body_position[2];
        const float expected=frame.ground_height-
            (frame.normal[0]*dx+frame.normal[2]*dz)/frame.normal[1];
        wheel.suspension_delta=std::clamp(wheel.world_position[1]-expected,-0.15f,0.15f);
    }
    frame.supported=true;
    return true;
}

template<class GroundQuery>
bool sample_swept(GroundQuery& query,
    const std::array<float,3>& from_position,float from_yaw,
    const std::array<float,3>& to_position,float to_yaw,
    VehicleRoadContactFrame& frame,VehicleRoadSweepStats& stats,float maximum_step){
    stats={};frame={};
    const auto reject=[&](VehicleRoadSweepFailure reason){stats.failure=reason;return false;};
    if(!std::isfinite(from_yaw)||!std::isfinite(to_yaw)||
       !std::isfinite(maximum_step)||maximum_step<=0.0f)
        return reject(VehicleRoadSweepFailure::InvalidInput);
    for(float value:from_position)if(!std::isfinite(value))
        return reject(VehicleRoadSweepFailure::InvalidInput);
    for(float value:to_position)if(!std::isfinite(value))
        return reject(VehicleRoadSweepFailure::InvalidInput);
    const double dx=double(to_position[0])-from_position[0];
    const double dy=double(to_position[1])-from_position[1];
    const double dz=double(to_position[2])-from_position[2];
    constexpr double TwoPi=6.28318530717958647692;
    const double yaw_start=std::remainder(double(from_yaw),TwoPi);
    const double yaw_delta=std::remainder(
        std::remainder(double(to_yaw),TwoPi)-yaw_start,TwoPi);
    double radius=0.0;
    for(std::size_t i=1u;i<=4u;++i){
        const auto& wheel=VehicleVisualObjectTranslations[i];
        radius=std::max(radius,std::hypot(double(wheel[0]),double(wheel[2])));
    }
    const double travel=std::hypot(std::hypot(dx,dz),dy)+radius*std::fabs(yaw_delta);
    const double required=std::max(1.0,std::ceil(travel/double(maximum_step)));
    if(!std::isfinite(required)||required>VehicleRoadSweepMaximumSegments)
        return reject(VehicleRoadSweepFailure::BudgetExceeded);
    const auto steps=static_cast<std::uint32_t>(required);
    for(std::uint32_t step=0u;step<=steps;++step){
        const double alpha=double(step)/double(steps);
        std::array<float,3> position{};
        for(unsigned axis=0;axis<3u;++axis)
            position[axis]=static_cast<float>(double(from_position[axis])+
                (double(to_position[axis])-from_position[axis])*alpha);
        VehicleRoadContactFrame candidate{};
        const bool supported=sample_contacts(query,position,
            static_cast<float>(yaw_start+yaw_delta*alpha),candidate);
        ++stats.sampled_poses;stats.wheel_queries+=4u;stats.wheel_hits+=candidate.hit_count;
        frame=candidate;
        if(!supported)return reject(VehicleRoadSweepFailure::Unsupported);
    }
    return frame.supported;
}
struct PackQuery {
    const CourseCollisionPack& pack;
    bool operator()(float x,float z,float y,CourseWorldGroundSample& hit){
        if(!course_collision_ground_at(pack,x,z,y,hit.ground))return false;
        hit.lane=0u;hit.material=pack.quads[hit.ground.quad_index].material;
        return true;
    }
};
struct WorldQuery {
    CourseWorldRuntime& world;
    bool operator()(float x,float z,float y,CourseWorldGroundSample& hit){
        return world.ground_at(x,z,y,hit);
    }
};
} // namespace

bool sample_vehicle_road_contacts(const CourseCollisionPack& pack,
    const std::array<float,3>& position,float yaw,VehicleRoadContactFrame& frame){
    PackQuery query{pack};return sample_contacts(query,position,yaw,frame);
}
bool sample_vehicle_road_contacts(CourseWorldRuntime& world,
    const std::array<float,3>& position,float yaw,VehicleRoadContactFrame& frame){
    WorldQuery query{world};return sample_contacts(query,position,yaw,frame);
}
bool sample_vehicle_road_contacts_swept(const CourseCollisionPack& pack,
    const std::array<float,3>& from,float from_yaw,const std::array<float,3>& to,float to_yaw,
    VehicleRoadContactFrame& frame,VehicleRoadSweepStats& stats,float maximum_step){
    PackQuery query{pack};return sample_swept(query,from,from_yaw,to,to_yaw,frame,stats,maximum_step);
}
bool sample_vehicle_road_contacts_swept(CourseWorldRuntime& world,
    const std::array<float,3>& from,float from_yaw,const std::array<float,3>& to,float to_yaw,
    VehicleRoadContactFrame& frame,VehicleRoadSweepStats& stats,float maximum_step){
    WorldQuery query{world};return sample_swept(query,from,from_yaw,to,to_yaw,frame,stats,maximum_step);
}
} // namespace outrun::platform

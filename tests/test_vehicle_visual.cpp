#include "platform/vehicle_visual.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
using namespace outrun::platform;
namespace {
unsigned checks{};
void require(bool value,const char* message){++checks;if(!value){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
bool near(float a,float b,float epsilon=1.0e-5f){return std::fabs(a-b)<=epsilon;}
std::array<float,4> apply(const VehicleVisualMatrix& m,const std::array<float,4>& v){
    std::array<float,4> out{};for(unsigned row=0;row<4u;++row)for(unsigned k=0;k<4u;++k)out[row]+=m[k*4u+row]*v[k];return out;
}
}
int main(){
    require(near(pc_wheel_visual_angle(0),0.0f),"zero PC angle");
    require(near(pc_wheel_visual_angle(0x4000),1.57079637f),"quarter-turn PC angle");
    MeshPreviewTransform base{};base.position=vehicle_visual_identity();base.normal=vehicle_visual_identity();
    VehicleVisualWheel wheel{};wheel.translation_delta={1.0f,2.0f,3.0f};wheel.spin=0x4000;
    const auto transform=compose_vehicle_visual_transform(base,wheel);
    const auto point=apply(transform.position,{0.0f,1.0f,0.0f,1.0f});
    require(near(point[0],1.0f)&&near(point[1],2.0f)&&near(point[2],4.0f),"translation then X spin order");
    wheel={};wheel.direction=0x4000;const auto steer=compose_vehicle_visual_transform(base,wheel);
    const auto forward=apply(steer.position,{0.0f,0.0f,1.0f,1.0f});
    require(near(forward[0],1.0f)&&near(forward[2],0.0f),"Y steering rotation");
    VehicleVisualTransform course{};
    const std::array<float,3> position{{10.0f,2.0f,3.0f}},up{{0.0f,1.0f,0.0f}};
    require(compose_course_vehicle_transform(base,position,0.0f,up,0u,nullptr,course),"course vehicle body transform");
    const auto body_origin=apply(course.position,{0.0f,0.0f,0.0f,1.0f});
    require(near(body_origin[0],10.0f)&&near(body_origin[1],2.0f)&&near(body_origin[2],3.0f),"course vehicle world translation");
    require(compose_course_vehicle_transform(base,position,0.0f,up,1u,nullptr,course),"course vehicle wheel base transform");
    const auto wheel_origin=apply(course.position,{0.0f,0.0f,0.0f,1.0f});
    require(near(wheel_origin[0],10.0f+VehicleVisualObjectTranslations[1][0])&&
            near(wheel_origin[1],2.0f+VehicleVisualObjectTranslations[1][1])&&
            near(wheel_origin[2],3.0f+VehicleVisualObjectTranslations[1][2]),"recovered wheel translation retained");
    require(compose_course_vehicle_transform(base,position,1.57079632679f,up,0u,nullptr,course),"course vehicle yaw transform");
    const auto nose=apply(course.position,{0.0f,0.0f,-1.0f,1.0f});
    require(near(nose[0],11.0f)&&near(nose[2],3.0f),"vehicle local nose follows world yaw");
    require(compose_course_vehicle_transform(base,position,0.0f,up,5u,nullptr,course),"left door transform");
    const auto door_origin=apply(course.position,{0.0f,0.0f,0.0f,1.0f});
    require(near(door_origin[0],position[0]-0.6340000033f)&&
            near(door_origin[2],position[2]-0.3070000112f),"PC door placement retained");
    require(!compose_course_vehicle_transform(base,position,0.0f,{{0.0f,0.0f,0.0f}},0u,nullptr,course),"zero ground normal rejected");
    std::printf("vehicle_visual: %u checks passed\n",checks);return 0;
}

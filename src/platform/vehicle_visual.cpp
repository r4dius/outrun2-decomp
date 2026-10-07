#include "platform/vehicle_visual.hpp"
#include <algorithm>
#include <cmath>

namespace outrun::platform {
namespace {
VehicleVisualMatrix translation(const std::array<float,3>& value){
    auto matrix=vehicle_visual_identity();matrix[12]=value[0];matrix[13]=value[1];matrix[14]=value[2];return matrix;
}
VehicleVisualMatrix rotation_x(float angle){
    auto matrix=vehicle_visual_identity();const float c=std::cos(angle),s=std::sin(angle);
    matrix[5]=c;matrix[6]=s;matrix[9]=-s;matrix[10]=c;return matrix;
}
VehicleVisualMatrix rotation_y(float angle){
    auto matrix=vehicle_visual_identity();const float c=std::cos(angle),s=std::sin(angle);
    matrix[0]=c;matrix[2]=-s;matrix[8]=s;matrix[10]=c;return matrix;
}
VehicleVisualMatrix rotation_z(float angle){
    auto matrix=vehicle_visual_identity();const float c=std::cos(angle),s=std::sin(angle);
    matrix[0]=c;matrix[1]=s;matrix[4]=-s;matrix[5]=c;return matrix;
}
}

float pc_wheel_visual_angle(std::int16_t value){
    // DispCarModel_Common performs Long2Float, *pi, *2^-15 in this order.
    return (static_cast<float>(value)*3.1415927410125732421875f)*0.000030517578125f;
}

VehicleVisualMatrix vehicle_visual_identity(){
    VehicleVisualMatrix matrix{};matrix[0]=matrix[5]=matrix[10]=matrix[15]=1.0f;return matrix;
}

VehicleVisualMatrix vehicle_visual_multiply(const VehicleVisualMatrix& left,
                                             const VehicleVisualMatrix& right){
    VehicleVisualMatrix result{};
    for(unsigned column=0;column<4u;++column)for(unsigned row=0;row<4u;++row){
        float value=0.0f;for(unsigned k=0;k<4u;++k)value+=left[k*4u+row]*right[column*4u+k];
        result[column*4u+row]=value;
    }
    return result;
}

VehicleVisualTransform compose_vehicle_visual_transform(
    const MeshPreviewTransform& base,const VehicleVisualWheel& wheel){
    const auto rz=rotation_z(wheel.model_rotation_z);
    const auto ry=rotation_y(pc_wheel_visual_angle(wheel.direction));
    const auto rx=rotation_x(pc_wheel_visual_angle(wheel.spin));
    const auto rotation=vehicle_visual_multiply(vehicle_visual_multiply(rz,ry),rx);
    VehicleVisualTransform result{};
    result.position=vehicle_visual_multiply(base.position,
        vehicle_visual_multiply(translation(wheel.translation_delta),rotation));
    result.normal=vehicle_visual_multiply(base.normal,rotation);
    return result;
}

bool compose_course_vehicle_transform(
    const MeshPreviewTransform& camera,
    const std::array<float,3>& vehicle_position,
    float vehicle_yaw,
    const std::array<float,3>& ground_normal,
    std::size_t object_index,
    const VehicleVisualWheel* wheel,
    VehicleVisualTransform& transform){
    if(object_index>=VehicleVisualObjectTranslations.size()||!std::isfinite(vehicle_yaw))return false;
    for(float value:vehicle_position)if(!std::isfinite(value))return false;
    for(float value:ground_normal)if(!std::isfinite(value))return false;
    const auto normalize=[](std::array<float,3> value,std::array<float,3>& out){
        const float length=std::sqrt(value[0]*value[0]+value[1]*value[1]+value[2]*value[2]);
        if(!std::isfinite(length)||length<=1.0e-6f)return false;
        for(unsigned i=0;i<3u;++i)out[i]=value[i]/length;
        return true;
    };
    const auto dot=[](const std::array<float,3>& a,const std::array<float,3>& b){
        return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];
    };
    const auto cross=[](const std::array<float,3>& a,const std::array<float,3>& b){
        return std::array<float,3>{{a[1]*b[2]-a[2]*b[1],
            a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]}};
    };
    std::array<float,3> up{};
    if(!normalize(ground_normal,up)||up[1]<0.0f)return false;
    const std::array<float,3> horizontal_forward{{std::sin(vehicle_yaw),0.0f,-std::cos(vehicle_yaw)}};
    const float along_up=dot(horizontal_forward,up);
    std::array<float,3> forward{};
    if(!normalize({{horizontal_forward[0]-along_up*up[0],
                    horizontal_forward[1]-along_up*up[1],
                    horizontal_forward[2]-along_up*up[2]}},forward))return false;
    std::array<float,3> right{};
    if(!normalize(cross(forward,up),right))return false;
    const std::array<float,3> back{{-forward[0],-forward[1],-forward[2]}};
    auto world=vehicle_visual_identity();
    for(unsigned axis=0;axis<3u;++axis){
        world[axis]=right[axis];world[4u+axis]=up[axis];world[8u+axis]=back[axis];
        world[12u+axis]=vehicle_position[axis];
    }
    auto world_normal=world;world_normal[12]=world_normal[13]=world_normal[14]=0.0f;
    MeshPreviewTransform base{};
    base.position=vehicle_visual_multiply(camera.position,
        vehicle_visual_multiply(world,translation(VehicleVisualObjectTranslations[object_index])));
    base.normal=vehicle_visual_multiply(camera.normal,world_normal);
    transform=wheel?compose_vehicle_visual_transform(base,*wheel):VehicleVisualTransform{base.position,base.normal};
    return true;
}

} // namespace outrun::platform

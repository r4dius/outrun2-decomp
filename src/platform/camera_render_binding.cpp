#include "platform/camera_render_binding.hpp"
#include <cmath>
#include <cstring>
namespace outrun::platform {
namespace {
using driving::Bytes;
bool finite(const std::array<std::uint8_t,64>& m){
    for(unsigned k=0;k<16;++k){float v;std::memcpy(&v,m.data()+k*4,4);if(!std::isfinite(v))return false;}
    return true;
}
// left*right with the ported D3DXMatrixMultiply (current = left * current).
std::array<std::uint8_t,64> multiply(const std::array<std::uint8_t,64>& left,const std::array<std::uint8_t,64>& right){
    std::array<std::uint8_t,128> arena{};std::array<std::uint8_t,64> l=left;
    driving::PcMatrixStack s{Bytes(arena.data(),arena.size()),0,0,2};
    driving::pc_matrix_load(s,Bytes(const_cast<std::uint8_t*>(right.data()),64));
    driving::pc_matrix_multiply_current(s,Bytes(l.data(),64));
    std::array<std::uint8_t,64> out{};std::memcpy(out.data(),arena.data(),64);return out;
}
bool publish(const std::array<std::uint8_t,64>& clip,const std::array<std::uint8_t,64>& view,MeshPreviewTransform& out){
    if(!finite(clip))return false;
    MeshPreviewTransform t{};
    std::memcpy(t.position.data(),clip.data(),64);
    // Normals: n * View rotation (row-vector) == column-major copy of the 3x3.
    for(unsigned r=0;r<3;++r)for(unsigned c=0;c<3;++c)std::memcpy(&t.normal[r*4+c],view.data()+(r*4+c)*4,4);
    t.normal[15]=1.f;
    out=t;return true;
}
}
bool camera_scene_transform(const std::array<std::uint8_t,64>& view,
    const std::array<std::uint8_t,64>& projection,MeshPreviewTransform& out){
    if(!finite(view)||!finite(projection))return false;
    return publish(multiply(view,projection),view,out);
}
bool camera_object_transform(const std::array<std::uint8_t,64>& world,
    const std::array<std::uint8_t,64>& view,const std::array<std::uint8_t,64>& projection,
    MeshPreviewTransform& out){
    if(!finite(world)||!finite(view)||!finite(projection))return false;
    const auto world_view=multiply(world,view);
    return publish(multiply(world_view,projection),world_view,out);
}
PcVehicleDisplay vehicle_display_46c140(Bytes car){
    car.check(0,0xf0);
    PcVehicleDisplay d{};
    d.model=car.i8(0x11);
    for(unsigned k=0;k<64;++k)d.world[k]=car.u8(0xb0+k);
    d.colour_scale=car.f32(0x58);
    return d;
}
}

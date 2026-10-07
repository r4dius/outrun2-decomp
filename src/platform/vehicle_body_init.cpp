#include "vehicle_body_init.hpp"
#include "driving/pc_x87.hpp"
#include "vehicle_constructor_data.hpp"
#include <cmath>
#include <cstring>
#include <limits>
namespace outrun::platform {
using namespace driving;
namespace {
float bits(unsigned u){float value;std::memcpy(&value,&u,4);return value;}
}
void vehicle_rigid_body_516e10(Bytes w,PcMatrixStack& matrices){
    w.check(0,0xe4);matrices.current();
    pc_matrix_push(matrices);pc_matrix_identity(matrices);pc_matrix_get(matrices,w.sub(0x10,64));pc_matrix_pop(matrices);
    for(unsigned o:{0x50u,0x5cu,0x68u,0x74u,0x80u,0x8cu,0xc0u,0xccu,0xd8u})
        for(unsigned k=0;k<3;++k)w.put32(o+k*4,0);
    w.put32(0,1);
    for(unsigned o=0x98;o<=0xb4;o+=4)w.putf(o,1.0f);
}
void vehicle_wheel_positions_4a1e80(Bytes w,Bytes p){
    w.check(0,PcVehicleBodyBytes);p.check(0,DrivingParameterViewBytes);
    const float x=w.f32(0x220),y=w.f32(0x224),z=bits(0x3db851e8)-w.f32(0x228);
    for(unsigned i=0;i<4;++i){
        const unsigned off=unsigned(embedded_wheel_offsets[i]),axle=i/2;
        float value=p.f32(axle?0x1c8:0x17c)*(i%2?0.5f:-0.5f);w.putf(off+4,value-x);
        w.putf(off+8,p.f32(axle?0x3dc:0x390)-y);
        value=p.f32(0x130)*0.5f;w.putf(off+12,axle?value+z:z-value);
    }
}
void vehicle_body_corners_4a1fc0(Bytes w){
    w.check(0,PcVehicleBodyBytes);w.put32(0x68c,4);
    for(unsigned i=0;i<4;++i){
        const unsigned off=0x690+i*12;
        w.putf(off,w.f32(i%2?0x678:0x674)-w.f32(0x220));
        w.putf(off+4,w.f32(0x67c)-w.f32(0x224));
        w.putf(off+8,w.f32(i/2?0x688:0x684)-w.f32(0x228));
    }
}
void vehicle_body_parameters_4a6840(Bytes w,Bytes p){
    w.check(0,PcVehicleBodyBytes);p.check(0,DrivingParameterViewBytes);
    const float mass=p.f32(0);w.putf(0x98,mass);
    for(unsigned i=0;i<3;++i)w.putf(0xa0+i*4,p.f32(0x4c*(i+1))*mass);
    const float reciprocal=1.0f/mass;w.putf(0x9c,reciprocal);
    // PC divides the reciprocal by each coefficient, NOT 1/(mass*coefficient).
    for(unsigned i=0;i<3;++i)w.putf(0xac+i*4,reciprocal/p.f32(0x4c*(i+1)));
    float z=p.f32(0x130)/(p.f32(0x214)+1.0f);
    z=z-p.f32(0x130)*0.5f;z=z+bits(0x3db851e8);
    w.put32(0x220,0);w.put32(0x224,p.u32(0x260));w.putf(0x228,z);
    vehicle_wheel_positions_4a1e80(w,p);
    const float radius=std::fabs(p.f32(0x1ab8));w.putf(0x678,radius);w.putf(0x674,-radius);
    for(unsigned i=0;i<4;++i)w.put32(0x67c+i*4,p.u32(0x1b04+i*0x4c));
}
void vehicle_body_init_4a69f0(Bytes e,Bytes w,Bytes p,std::uint32_t address,CourseWorldQuery& query){
    e.check(0,PcVehicleObjectBytes);w.check(0,PcVehicleBodyBytes);p.check(0,DrivingParameterViewBytes);
    if(address>std::numeric_limits<std::uint32_t>::max()-PcVehicleBodyBytes)
        throw std::out_of_range("vehicle work semantic address overflow");
    auto& matrices=query.matrices;matrices.current();pc_matrix_push(matrices);
    for(unsigned o=0;o<PcVehicleBodyBytes;o+=4)w.put32(o,0);
    vehicle_rigid_body_516e10(w,matrices);
    const float distribution=p.f32(0x214);
    float front=p.f32(0)/(1.0f/distribution+1.0f);front=front*0.5f;
    float rear=p.f32(0)/(distribution+1.0f);rear=rear*0.5f;
    const float rate=bits(0x4270cccd),velocity=e.f32(0x1c4)*rate;
    const float front_spin=(e.f32(0x1c4)/p.f32(0xb48))*rate;
    const float rear_spin=(e.f32(0x1c4)/p.f32(0xb94))*rate;
    for(unsigned i=0;i<4;++i){
        const unsigned off=unsigned(embedded_wheel_offsets[i]);
        w.put32(0x248+i*4,address+off);
        for(unsigned k:{0x24u,0x34u,0x38u})w.putf(off+k,i<2?front:rear);
        w.putf(off+0xd4,velocity);w.putf(off+0xd8,i<2?front_spin:rear_spin);
        w.put32(off+0x40,0);w.put32(off+0x44,0);w.putf(off+0x48,-1.0f);w.putf(off+0xe4,1.0f);
    }
    vehicle_body_parameters_4a6840(w,p);vehicle_body_corners_4a1fc0(w);
    e.put32(0x18,0);CourseProbe position{e.f32(0x14),0,e.f32(0x1c)};
    get_y_position_prog(query,0x100,position);e.putf(0x18,position.y);
    pc_matrix_identity(matrices);
    constexpr float unit=9.58738019107841e-05f; // 628254
    auto angle=[&](unsigned o){return driving::x87_float(driving::X87(int(e.i16(o)))*driving::X87(unit));}; // fild; fmul; fstp
    pc_matrix_rotate_y(matrices,angle(0x2e));pc_matrix_rotate_x(matrices,angle(0x2c));pc_matrix_rotate_z(matrices,angle(0x30));
    pc_matrix_get(matrices,e.sub(0x70,64));pc_matrix_set_translation(matrices,position);
    pc_matrix_get(matrices,w.sub(0x10,64));pc_matrix_identity(matrices);pc_matrix_get(matrices,w.sub(0x1e0,64));
    pc_matrix_pop(matrices);
}
void vehicle_ground_init_519300(Bytes e,Bytes w,CourseWorldQuery& query,const std::array<float,2>& area_yaw){
    e.check(0,PcVehicleObjectBytes);w.check(0,PcVehicleBodyBytes);
    const CourseProbe position{w.f32(0x40),w.f32(0x44),w.f32(0x48)};
    CourseProbe point=position;
    auto polygon=e.u32(0x230),special=e.u32(0x1c0);
    const auto type=get_y_position_spl_chk(query,point,&polygon,&special);
    e.put32(0x230,polygon);e.put32(0x1c0,special);e.put32(0x5c,type);
    e.put16(0x25c,0);e.put16(0x64,0);
    w.putf(0x640,point.x);w.putf(0x644,point.y);w.putf(0x648,point.z);
    const float center_half=point.y*0.5f;
    point={position.x+0.5f,position.y,position.z};get_y_position_spl_chk(query,point);
    const float x_half=point.y*0.5f;
    point={position.x,position.y,position.z+0.5f};get_y_position_spl_chk(query,point);
    const CourseProbe normal=pc_normalize_vector_40ef00({center_half-x_half,0.25f,center_half-point.y*0.5f});
    w.putf(0x628,normal.x);w.putf(0x62c,normal.y);w.putf(0x630,normal.z);
    // The original adds work+640 (X) to ALL three components here. Preserve
    // that behavior, even though adding XYZ would look more geometrical.
    w.putf(0x634,normal.x*0.01f+w.f32(0x640));
    w.putf(0x638,normal.y*0.01f+w.f32(0x640));
    w.putf(0x63c,normal.z*0.01f+w.f32(0x640));
    if(type>=query.tables.courses.size())throw std::out_of_range("vehicle initial ground course type");
    const auto& course=query.tables.courses[type];
    e.put16(0x294,std::uint16_t(course_collision_offset_direction(course.polygons,special,area_yaw[type?1:0],course.polygons_present)));
}
void vehicle_collision_init_4f6e40(Bytes e){
    e.check(0,PcVehicleObjectBytes);
    const auto model=e.u8(0x11);
    if(model>=VehicleCollisionData.size())throw std::out_of_range("vehicle collision model");
    // Bytes is a read/write ABI view; source is a local immutable-table copy.
    auto descriptor=VehicleCollisionData[model];
    vehicle_collision_init_4f6e40(e,Bytes(descriptor.data(),descriptor.size()));
}
void vehicle_collision_init_4f6e40(Bytes e,Bytes source){
    e.check(0,PcVehicleObjectBytes);
    const auto model=e.u8(0x11);
    const unsigned count=source.u8(0);
    source.check(4,count*28);e.check(0x338,count*0x1ec);
    e.put8(0x334,model);e.put8(0x335,std::uint8_t(count));
    for(unsigned i=0;i<count;++i){
        auto p=source.sub(4+i*28,28);const unsigned o=0x338+i*0x1ec;
        for(unsigned k=0;k<7;++k)e.put32(o+k*4,p.u32(k*4));
        const float half=(p.f32(8)-p.f32(20))*0.5f;
        const float squared=p.f32(24)*p.f32(24)-half*half;
        // 449390 (fabs) stored to float, 449380 fsqrt: rounds to the x87 precision control.
        const driving::X87 radius=driving::x87_sqrt(driving::X87(std::fabs(squared)));
        e.putf(o+28,driving::x87_float(driving::X87(p.f32(0))-radius));
        e.putf(o+32,(p.f32(4)+p.f32(16))*0.5f);
        e.putf(o+36,(p.f32(20)+p.f32(8))*0.5f);
        e.putf(o+40,driving::x87_float(radius+driving::X87(p.f32(12))));
        e.put32(o+44,e.u32(o+32));e.put32(o+48,e.u32(o+36));e.put32(o+52,0);
        for(unsigned start:{0x40u,0x114u})for(unsigned j=0;j<4;++j){
            const auto q=o+start+j*0x34;e.put32(q-4,0);e.put32(q,0xffffffff);e.put32(q-8,0);
        }
        e.put32(o+0x108,0);
        for(unsigned j=0;j<4;++j)e.put32(o+0x1dc+j*4,0);
    }
}
}

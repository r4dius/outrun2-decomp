#include "pc_environment_blend.hpp"
#include "driving/pc_x87.hpp"
#include <cmath>
#include <limits>
namespace outrun::driving {
namespace {
float atan_spilled(float y,float x){
    return x87_float(x87_atan2(X87(y),X87(x))); // fld y; fld x; fpatan; fstp m32
}
float lerp(float old,float now,float t,float inverse){return old*t+now*inverse;}
float extended_lerp(float old,float now,float t,float inverse){return float(X87(old)*t+X87(now)*inverse);}
std::uint8_t color_lerp(unsigned old,unsigned now,float t,float inverse){
    const X87 value=X87(old)*t+X87(now)*inverse;
    return std::uint8_t(x87_ftol64(value)); // original ftol2 low byte (indefinite -> 0)
}
void copy_words(Bytes to,Bytes from,unsigned count){for(unsigned k=0;k<count;++k)to.put32(k*4,from.u32(k*4));}
}
PcEnvironmentAngles environment_angles_44a480(CourseProbe v){
    constexpr float epsilon=1.1920928955078125e-7f;
    if(std::fabs(v.x)<epsilon&&std::fabs(v.z)<epsilon)v.z=epsilon;
    const X87 length=x87_sqrt(X87(v.z)*v.z+X87(v.x)*v.x);
    float horizontal=float(length);if(length<epsilon)horizontal=epsilon;
    return {atan_spilled(v.y,horizontal),atan_spilled(-v.x,-v.z)};
}
CourseProbe environment_direction_44a430(float pitch,float yaw,PcMatrixStack& s){
    pc_matrix_push_unit(s);pc_matrix_rotate_y(s,yaw);pc_matrix_rotate_x(s,pitch);
    auto result=pc_matrix_vector(s,{0,0,-1});pc_matrix_pop(s);return result;
}
void course_environment_blend_44ab10(const std::array<Bytes,3>& records,unsigned lane,PcEnvironmentBlendContext& c){
    if(c.phase_7d28c8!=0&&c.phase_7d28c8!=2)return;
    if(lane>1)return;
    auto live=lane?c.live_sun_899b98:c.live_fog_7d3a10;
    auto saved=lane?c.saved_sun_7d26d0:c.saved_fog_7d28e0;
    const unsigned stride=lane?0xa0:0x1c;
    live.check(0,3*stride);saved.check(0,3*stride);
    if(c.phase_7d28c8==0){copy_words(saved,live,3*stride/4);return;}
    for(unsigned slot=0;slot<3;++slot){
        auto source=records[slot];if(source.size()==0)continue;
        source.check(0,lane?0x70:0x4b);
        auto old=saved.sub(slot*stride,stride),out=live.sub(slot*stride,stride);
        const float t=float(c.time_7d2934)/c.duration_7d28d8,inverse=1.f-t;
        if(lane==0){
            unsigned packed=0;
            for(unsigned k=0;k<3;++k){const auto v=color_lerp(old.u8(0x18+k),source.u8(0x48+k),t,inverse);out.put8(0x18+k,v);packed=(packed<<8)|v;}
            out.put32(0x14,packed);out.put32(4,source.u32(0x34));
            for(unsigned k=0;k<3;++k)out.putf(8+k*4,lerp(old.f32(8+k*4),source.f32(0x38+k*4),t,inverse));
            continue;
        }
        std::array<float,12> values{};
        for(unsigned k=0;k<12;++k){const unsigned previous=k<3?8+k*4:k<6?0x18+(k-3)*4:0x6c+(k-6)*4;
            values[k]=lerp(old.f32(previous),source.f32(0x30+k*4),t,inverse);}
        constexpr float degrees=0.01745329238474369f;
        const float pitch=float(X87(source.f32(0x60))*degrees);
        const float yaw=float(X87(source.f32(0x64))*degrees);
        pc_matrix_push_load(c.matrices,c.primary_matrix_7d2da0);
        auto vector=environment_direction_44a430(pitch,yaw,c.matrices);
        vector=pc_matrix_vector(c.matrices,vector);
        const auto angles=environment_angles_44a480(vector);
        const float result_pitch=extended_lerp(old.f32(0x84),angles.pitch,t,inverse);
        const float result_yaw=extended_lerp(old.f32(0x88),angles.yaw,t,inverse);
        pc_matrix_pop(c.matrices);
        const float strength=extended_lerp(old.f32(0x8c),source.f32(0x68),t,inverse);
        const float range=lerp(old.f32(0x90),source.f32(0x6c),t,inverse);
        out.putf(0x84,result_pitch);out.putf(0x88,result_yaw);
        const auto direction=environment_direction_44a430(result_pitch,result_yaw,c.matrices);
        out.putf(0x44,direction.x);out.putf(0x48,direction.y);out.putf(0x4c,direction.z);
        out.putf(0x8c,strength);
        for(unsigned k=0;k<3;++k){out.putf(8+k*4,values[k]);out.putf(0x18+k*4,values[3+k]);out.putf(0x6c+k*4,values[6+k]);out.putf(0x78+k*4,values[9+k]);}
        out.putf(0x14,1);out.put32(0x24,0);out.putf(0x90,range);
    }
}
void course_environment_record_44a520(Bytes record,const CourseCollisionTables& primary,PcEnvironmentBlendContext& c){
    record.check(0,0x30);
    std::array<std::uint8_t,12> request{};std::array<std::uint8_t,0x58> result{};
    Bytes r(request.data(),request.size()),o(result.data(),result.size());
    r.put16(8,std::uint16_t(record.i16(0)));course_run_geometry_43e570(primary,r,o,-1);
    pc_matrix_push_load(c.matrices,c.primary_matrix_7d2da0);
    auto start=pc_matrix_point(c.matrices,{o.f32(8),o.f32(12),o.f32(16)});
    record.putf(4,start.x);record.putf(8,start.y);record.putf(12,start.z);
    if(record.i16(0)==record.i16(0x10)){
        for(unsigned k=0;k<3;++k){record.put32(0x14+k*4,record.u32(4+k*4));record.put32(0x24+k*4,0);}
        record.putf(0x20,std::numeric_limits<float>::max());
    }else{
        r.put16(8,std::uint16_t(record.i16(0x10)));course_run_geometry_43e570(primary,r,o,-1);
        auto end=pc_matrix_point(c.matrices,{o.f32(8),o.f32(12),o.f32(16)});
        record.putf(0x14,end.x);record.putf(0x18,end.y);record.putf(0x1c,end.z);
        record.putf(0x20,float(X87(1.0f)/(course_vec3_distance_x87({start.x,start.y,start.z},{end.x,end.y,end.z}))));
        CourseProbe direction{start.x-end.x,start.y-end.y,start.z-end.z};pc_unit_vector_40eeb0(direction);
        record.putf(0x24,direction.x);record.putf(0x28,direction.y);record.putf(0x2c,direction.z);
    }
    pc_matrix_pop(c.matrices);
}
void course_environment_progress_44b020(const std::array<Bytes,3>& lists,unsigned lane,
    Bytes vehicle,Bytes flags,const CourseCollisionTables& primary,PcEnvironmentBlendContext& c){
    if(c.phase_7d28c8!=3){course_environment_blend_44ab10(lists,lane,c);return;}
    // Other lanes still consume flags and prepare records on PC; only the
    // final leaf stores are skipped. flags index = slot + 3*lane.
    vehicle.check(0,0x66);flags.check(0,(lane*3+3)*4);
    pc_matrix_push_load(c.matrices,c.primary_matrix_7d2da0);
    for(unsigned slot=0;slot<3;++slot){
        auto list=lists[slot];if(!list.size())continue;
        std::size_t at=0;
        for(;;at+=0xb0){const auto tag=std::uint16_t(list.i16(at));
            if(tag==0xffff)break;
            if(tag==0xfffe)continue;
            auto source=list.sub(at,0xb0);const auto end=std::uint16_t(source.i16(0x10));
            if(!(tag<=vehicle.i16(0x64)&&end>=vehicle.i16(0x64))&&!flags.u32((lane*3+slot)*4))continue;
            flags.put32((lane*3+slot)*4,0);
            const bool fixed=tag==end;float t=0,inverse=1;
            if(!fixed){
                if(source.f32(8)==std::numeric_limits<float>::max())course_environment_record_44a520(source,primary,c);
                CourseProbe delta{source.f32(4)-vehicle.f32(0x14),source.f32(8)-vehicle.f32(0x18),source.f32(12)-vehicle.f32(0x1c)};
                const auto distance=pc_unit_vector_40eeb0(delta);
                // FCOMIP/JB: unordered values take the calculation branch.
                if(!(X87(distance)<=X87(1.1920928955078125e-7))){
                    const X87 dot=(X87(source.f32(0x2c))*delta.z+X87(source.f32(0x28))*delta.y)+X87(source.f32(0x24))*delta.x;
                    if(!(dot<=X87(1.1920928955078125e-7))){t=(float(dot)*float(distance))*source.f32(0x20);if(t>=1.f)t=1.f;}
                }
                inverse=1.f-t;
            }
            if(lane>1)break;
            if(lane==0){
                auto out=c.live_fog_7d3a10.sub(slot*0x1c,0x1c);unsigned packed=0;
                for(unsigned k=0;k<3;++k){const auto color=fixed?source.u8(0x48+k):color_lerp(source.u8(0x48+k),source.u8(0x64+k),inverse,t);
                    out.put8(0x18+k,color);packed=(packed<<8)|color;
                    out.putf(8+k*4,fixed?source.f32(0x38+k*4):lerp(source.f32(0x38+k*4),source.f32(0x54+k*4),inverse,t));}
                out.put32(0x14,packed);out.put32(4,source.u32(0x34));
            }else{
                auto out=c.live_sun_899b98.sub(slot*0xa0,0xa0);std::array<float,16> value{};
                for(unsigned k=0;k<16;++k)value[k]=fixed?source.f32(0x30+k*4):lerp(source.f32(0x30+k*4),source.f32(0x70+k*4),inverse,t);
                constexpr float degrees=0.01745329238474369f;
                auto direction=environment_direction_44a430(value[12]*degrees,value[13]*degrees,c.matrices);
                direction=pc_matrix_vector(c.matrices,direction);auto angles=environment_angles_44a480(direction);
                out.putf(0x84,angles.pitch);out.putf(0x88,angles.yaw);
                direction=environment_direction_44a430(angles.pitch,angles.yaw,c.matrices);
                out.putf(0x44,direction.x);out.putf(0x48,direction.y);out.putf(0x4c,direction.z);
                out.putf(0x8c,value[14]);out.putf(0x90,value[15]);
                for(unsigned k=0;k<3;++k){out.putf(8+k*4,value[k]);out.putf(0x18+k*4,value[3+k]);out.putf(0x6c+k*4,value[6+k]);out.putf(0x78+k*4,value[9+k]);}
                out.putf(0x14,1);out.put32(0x24,0);
            }
            break;
        }
    }
    pc_matrix_pop(c.matrices);
}
void course_environment_lights_44a1d0(Bytes list,Bytes camera,Bytes nearest,Bytes lights){
    camera.check(0xf8,24);nearest.check(0,24);lights.check(0,6*0xa0);
    constexpr float epsilon=1.1920928955078125e-7f;
    const float maximum=std::numeric_limits<float>::max();
    nearest.putf(0,maximum);nearest.putf(8,maximum);
    std::array<float,3> eye{},forward{};
    for(unsigned k=0;k<3;++k){eye[k]=camera.f32(0xf8+k*4);forward[k]=camera.f32(0x104+k*4)-eye[k];}
    if(list.size())for(unsigned index=0;;++index){
        auto source=list.sub(index*0x2c,0x2c);
        if(source.f32(0xc)==-999.9f)break;
        if(source.f32(0xc)==-100.f)continue;
        std::array<X87,3> delta{};std::array<float,3> vector{};
        for(unsigned k=0;k<3;++k){delta[k]=X87(eye[k])-source.f32(0x10+k*4);vector[k]=source.f32(0x10+k*4)-eye[k];}
        const float distance=float(x87_sqrt((delta[2]*delta[2]+delta[0]*delta[0])+delta[1]*delta[1]));
        if(distance>=500.f||distance>=nearest.f32(8))continue;
        const X87 dot=(X87(forward[2])*vector[2]+X87(forward[1])*vector[1])+X87(forward[0])*vector[0];
        if(dot<epsilon&&distance>source.f32(0x1c))continue;
        unsigned slot=0;while(slot<2&&!(nearest.f32(slot*8)>distance))++slot;
        if(slot==2)continue;
        for(unsigned i=2;i>slot;--i){nearest.put32(i*8,nearest.u32((i-1)*8));nearest.put32(i*8+4,nearest.u32((i-1)*8+4));}
        nearest.putf(slot*8,distance);nearest.put32(slot*8+4,index);
    }
    for(unsigned slot=0;slot<2;++slot){
        const bool enabled=nearest.f32(slot*8)!=maximum;
        for(unsigned group=1;group<=2;++group){
            auto out=lights.sub((group*2+slot)*0xa0,0xa0);out.put32(0,enabled?1:0);
            if(!enabled)continue;
            auto source=list.sub(nearest.u32(slot*8+4)*0x2c,0x2c);
            for(unsigned k=0;k<3;++k){
                out.put32(8+k*4,source.u32(k*4));out.put32(0x38+k*4,source.u32(0x10+k*4));
                const float attenuation=source.f32(0x20+k*4);
                out.putf(0x58+k*4,std::fabs(attenuation)<epsilon?epsilon:attenuation);
            }
            out.putf(0x14,1.f);out.put32(0x50,source.u32(0x1c));out.put32(0x90,source.u32(0xc));
        }
    }
}
std::int16_t environment_trigger_44c610(const PcEnvironmentTransition& t){
    if((t.mode_79fcce&3u)!=2u||t.record.size()==0)return 100;
    return t.record.i16(0);
}
std::int8_t environment_duration_44c640(const PcEnvironmentTransition& t){
    if((t.mode_79fcce&3u)!=2u||t.record.size()==0)return 2;
    return std::int8_t(t.record.u8(2));
}
void course_environment_phase_44a000(Bytes vehicle,const PcEnvironmentTransition& transition,PcEnvironmentBlendContext& c){
    switch(c.phase_7d28c8){
    case 0:{
        const auto seconds=environment_duration_44c640(transition);
        c.time_7d2934=std::int16_t(std::int16_t(seconds)*60);
        c.duration_7d28d8=float(seconds)*60.2f; // cvtsi2ss, mulss 0x628110
        c.phase_7d28c8=1;return;}
    case 1:
        vehicle.check(0x64,2);
        if(vehicle.i16(0x64)<environment_trigger_44c610(transition))return;
        c.phase_7d28c8=2;
        [[fallthrough]];
    case 2:
        c.time_7d2934=std::int16_t(c.time_7d2934-1);
        if(c.time_7d2934<0)c.phase_7d28c8=3;
        return;
    default:return;
    }
}
void course_environment_fog_4517d0(PcEnvironmentFrame& f,const CourseCollisionTables& primary,PcEnvironmentBlendContext& c){
    f.vehicle.check(0x5c,4);
    if(f.vehicle.u32(0x5c)==0)course_environment_progress_44b020(f.fog_lists,0,f.vehicle,f.flags_7d28b0,primary,c);
    else course_environment_blend_44ab10(f.fog_lists,0,c);
}
void course_environment_sun_449f50(PcEnvironmentFrame& f,const CourseCollisionTables& primary,PcEnvironmentBlendContext& c){
    f.vehicle.check(0x5c,4);
    if(f.vehicle.u32(0x5c)==0)course_environment_progress_44b020(f.sun_lists,1,f.vehicle,f.flags_7d28b0,primary,c);
    else course_environment_blend_44ab10(f.sun_lists,1,c);
    course_environment_lights_44a1d0(f.light_list,f.camera,f.nearest_7d2d58,f.lights_899d78);
}
void course_environment_update_44a8df(PcEnvironmentFrame& f,const CourseCollisionTables& primary,
    const PcEnvironmentTransition& transition,PcEnvironmentBlendContext& c){
    course_environment_fog_4517d0(f,primary,c);
    course_environment_sun_449f50(f,primary,c);
    course_environment_phase_44a000(f.vehicle,transition,c);
}
}

#include "driving/pc_body_wall.hpp"
#include <array>
#include <stdexcept>
namespace outrun::driving {
void car_body_wall_coli_check(Bytes e,Bytes w,PcBodyWallContext& c){
    e.check(0,event_size);w.check(0,work_size);
    const auto count=w.i32(0x68c);
    if(count>8)throw std::out_of_range("CarBodyWallColiCheck local contact count exceeds original stack capacity");
    if(count>0)w.check(0x690u,std::size_t(count)*12u);
    auto& matrix=c.query.matrices;
    pc_matrix_push(matrix);
    pc_matrix_load(matrix,w.sub(0x10,64));
    if(e.u32(4)&0x80000000u)pc_matrix_rotate_y(matrix,e.f32(0x2e8));

    w.put32(0x244,w.u32(0x244)&0xffffffc7u);
    e.put32(4,e.u32(4)&0xfffdffffu);
    std::uint32_t aggregate=0u,intersection=0xffffffffu;
    std::array<std::uint8_t,8u*16u> local{};
    Bytes contacts(local.data(),local.size());
    for(std::int32_t i=0;i<count;++i){
        const auto probe=CourseProbe{w.f32(0x690u+std::size_t(i)*12u),w.f32(0x694u+std::size_t(i)*12u),w.f32(0x698u+std::size_t(i)*12u)};
        CourseProbe world=pc_matrix_point(matrix,probe);
        std::uint32_t polygon=0xffffffffu,special=0u,kind=1u,type=0u;
        if(e.u32(4)&1u)type=get_y_position_prog_bk(c.query,c.bk,0u,world,&polygon,&special,&kind);
        else type=get_y_position_prog(c.query,0u,world,&polygon,&special,&kind);
        auto r=contacts.sub(std::size_t(i)*16u,16u);
        r.put32(0,polygon);r.put32(4,special);r.put32(8,kind);r.put32(12,type);
        aggregate|=kind;intersection&=kind;
    }
    e.put32(0x2a8,aggregate);
    if(intersection&1u){
        if(c.body_params.size()<0x15fcu)throw std::out_of_range("CarBodyWallColiCheck body parameter view too small");
        if(c.wheel_block.size()<4u*0xf4u)throw std::out_of_range("CarBodyWallColiCheck wheel block too small");
        pc_pl_wrecker(e,w,c.body_params,c.wheel_block,1,c.wrecker);
        pc_matrix_pop(matrix);return;
    }
    if(!(intersection&2u) && (aggregate&0x010f4060u)){
        cbw_coli_wall(e,w,contacts.sub(0,count>0?std::size_t(count)*16u:0u),matrix,c.wall);
        e.put32(4,e.u32(4)|0x00020000u);
        pc_matrix_pop(matrix);return;
    }
    w.put32(0x670,0u);
    pc_matrix_pop(matrix);
}
}

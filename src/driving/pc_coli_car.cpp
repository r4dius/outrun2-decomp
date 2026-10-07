#include "driving/pc_coli_car.hpp"
#include <stdexcept>
namespace outrun::driving {
void coli_car(Bytes e,Bytes w,PcColiCarContext& c){
    e.check(0,event_size);w.check(0,work_size);
    auto& stack=c.body_wall.query.matrices;
    if(&c.ground.world.matrices!=&stack)
        throw std::invalid_argument("ColiCar ground/body contexts must share one matrix stack");
    pc_matrix_push(stack);
    pc_matrix_load(stack,w.sub(0x10,64));
    calc_ground_coli_face(e,w,c.parameters,c.wheels,c.ground);
    car_sus_coli_check(e,w,c.parameters,c.wheels,stack);
    car_sus_bump_push(e,w,c.parameters,c.wheels,stack);
    car_body_wall_coli_check(e,w,c.body_wall);
    pc_matrix_pop(stack);
}
}

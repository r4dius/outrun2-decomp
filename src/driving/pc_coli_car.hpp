#pragma once
#include "driving/pc_body_wall.hpp"
#include "driving/pc_ground_collision.hpp"
#include "driving/pc_collision.hpp"
namespace outrun::driving {
// Explicit dependencies of PC ColiCar 0x00519830.  All four original stages
// share the same matrix stack and bounded native wheel/parameter views.
struct PcColiCarContext {
    GroundCollisionContext& ground;
    PcBodyWallContext& body_wall;
    Bytes parameters;
    std::array<Bytes,4> wheels;
};
// Complete ordered PC dispatcher: Push/Load body matrix, ground face,
// suspension contact, suspension bump push, body-wall controller, Pop.
void coli_car(Bytes event,Bytes work,PcColiCarContext&);
}

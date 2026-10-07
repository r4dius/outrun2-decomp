#include "platform/course_world_runtime.hpp"
#include "platform/pc_vehicle_control.hpp"
#include "driving/pc_ground_collision.hpp"
#include "driving/pc_suspension.hpp"
#include "driving/pc_wheel_dynamics.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <utility>
using namespace outrun;
namespace {
unsigned checks{};
void require(bool value,const char* text){
    ++checks;if(!value)throw std::runtime_error(text);
}
void put16(std::vector<std::uint8_t>& bytes,std::size_t at,std::uint16_t value){
    bytes[at]=std::uint8_t(value);bytes[at+1u]=std::uint8_t(value>>8u);
}
void put32(std::vector<std::uint8_t>& bytes,std::size_t at,std::uint32_t value){
    for(unsigned i=0;i<4u;++i)bytes[at+i]=std::uint8_t(value>>(i*8u));
}
void check_layout(const platform::CourseCollisionPack& pack,unsigned total,unsigned primary,
                  unsigned lengths,unsigned lists){
    const auto table=platform::course_collision_tables(pack);
    require(table.runs.header.u32(8u)==total&&table.runs.header.u32(12u)==primary,
            "native header must exclude file size prefix and distinguish primary/total");
    require(pack.pc_layout.primary_length_count==lengths&&
            pack.pc_layout.referenced_area_lists==lists,"source topology counts");
    require(table.runs.ranges.size()==pack.primary_ranges.size()&&
            pack.primary_ranges.size()!=pack.pc_coli0200.size()-4u-pack.pc_layout.sections[6],
            "runtime range table is not source section six");
    const driving::Bytes source(const_cast<std::uint8_t*>(pack.pc_coli0200.data()),pack.pc_coli0200.size());
    for(std::size_t i=0;i<pack.quads.size();++i){
        const auto at=4u+pack.pc_layout.sections[2]+i*64u;
        require(pack.quads[i].center_xz[0]==source.f32(at+48u)&&
                pack.quads[i].center_xz[1]==source.f32(at+56u),
                "source center is X/Z, not Y/Z");
    }
    for(unsigned length=0;length<lengths;++length){
        std::int32_t first=-9,last=-9;
        const auto count=driving::find_primary_course_run(table.runs,
            static_cast<std::int16_t>(length),-1,first,last);
        require(count>0&&first>=0&&last>=first&&unsigned(last)<primary,
                "indexed primary run bounds");
        for(const auto hint:{0,std::int32_t(primary/2u),std::int32_t(primary-1u),
                             std::int32_t(primary),std::int32_t(total-1u)}){
            std::int32_t a=-7,b=-7;
            const auto n=driving::find_primary_course_run(table.runs,
                static_cast<std::int16_t>(length),hint,a,b);
            require(n==count&&a==first&&b==last,
                    "generated lookup equals recovered forward/backward hint scan");
        }
        for(auto i=first;i<=last;++i)
            require(table.runs.lengths.i16(std::size_t(i)*2u)==std::int16_t(length),
                    "every primary polygon belongs to its exact source course length");
    }
}
void check_rejections(const platform::CourseCollisionPack& pack){
    auto retained=pack;const auto original=pack.pc_coli0200;std::string error;
    const auto rejects=[&](const std::vector<std::uint8_t>& bad){
        require(!platform::parse_pc_coli0200(bad.data(),bad.size(),retained,&error)&&!error.empty(),
                "malformed source must fail with a reason");
        require(retained.pc_coli0200==original&&retained.primary_ranges==pack.primary_ranges,
                "failed admission preserves previous owned tables");
    };
    auto bad=original;bad[0]^=1u;rejects(bad);
    bad=original;put32(bad,16u,pack.pc_layout.polygon_count+1u);rejects(bad);
    bad=original;put32(bad,28u,0u);rejects(bad);
    bad=original;put16(bad,0x44u,0xffffu);rejects(bad);
    // The first referenced list is valid in the original; poison its first index.
    const auto grid=platform::course_collision_grid(pack);
    for(std::size_t i=0;i<65536u;++i){
        const auto word=static_cast<std::uint16_t>(grid.i16(i*2u));
        if(word==0u)continue;
        bad=original;
        put16(bad,4u+pack.pc_layout.sections[0]+std::size_t(word)*2u+2u,0xffffu);
        rejects(bad);break;
    }
    bad=original;put16(bad,4u+pack.pc_layout.sections[5],1u);rejects(bad);
    // PC 43DCD2 accepts a decreasing primary length (\BK\coli_LBK_PALM ends
    // with 8): it only clears 0x780190. Admission follows it.
    {
        bad=original;const auto last=pack.pc_layout.primary_polygon_count-1u;
        put16(bad,4u+pack.pc_layout.sections[5]+last*2u,1u);
        platform::CourseCollisionPack irregular;std::string reason;
        require(platform::parse_pc_coli0200(bad.data(),bad.size(),irregular,&reason)&&
                !irregular.pc_layout.primary_lengths_ordered,"a decreasing primary length is admitted as on PC");
    }
    bad=original;put32(bad,4u+pack.pc_layout.sections[3],0x7fc00000u);rejects(bad);
    bad=original;put32(bad,4u+pack.pc_layout.sections[2],0x7f800000u);rejects(bad);
    for(const auto size:{0u,1u,4u,63u,0x20043u}){
        bad.assign(original.begin(),original.begin()+size);rejects(bad);
    }
}
void check_owner(const platform::CourseCollisionPack& pack){
    platform::CourseWorldRuntime world{};std::string error;
    const platform::CourseWorldSourceIdentity id{15u,0x005d1128u,0x005d7000u,0x12345678u};
    require(world.admit_lane(0u,pack.pc_coli0200.data(),pack.pc_coli0200.size(),id,&error),
            "native world admits source bytes");
    require(world.lane_loaded(0u)&&!world.query_ready(),
            "byte readiness must not fabricate original area transform");
    platform::CourseWorldGroundSample sample{};sample.ground.height=9876.0f;
    require(!world.ground_at(0,-4,0,sample)&&sample.ground.height==9876.0f,
            "unknown transform leaves output untouched");
    require(world.set_transform(0u,platform::course_world_identity_transform(),&error)&&world.query_ready(),
            "explicit source-local test transform makes query available");
    unsigned hits=0u;
    for(std::size_t i=0;i<pack.pc_layout.primary_polygon_count;i+=7u){
        const auto& q=pack.quads[i];platform::CourseGroundSample cold{};
        const bool a=platform::course_collision_ground_at(pack,q.center_xz[0],q.center_xz[1],q.vertices[0][1],cold);
        const bool b=world.ground_at(q.center_xz[0],q.center_xz[1],q.vertices[0][1],sample);
        require(a==b,"retained world and fresh source query agree on hit/miss");
        if(a){
            ++hits;
            require(sample.lane==0u&&sample.ground.quad_index==cold.quad_index&&
                    sample.ground.surface_flags==cold.surface_flags&&
                    std::fabs(sample.ground.height-cold.height)<1e-5f,
                    "retained native source grid/list/spline result matches fresh query");
        }
    }
    require(hits>100u,"real course corpus must exercise many successful native queries");
    auto copy=world;world.reset();
    require(!world.query_ready()&&copy.query_ready(),"copy owns independent backing bytes");
    const auto expected=copy.lane(0u).pc_coli0200;
    auto moved=std::move(copy);
    require(!copy.lane_loaded(0u)&&!copy.query_ready()&&moved.query_ready()&&
            moved.lane(0u).pc_coli0200==expected,"move detaches source and rebuilds views");
    auto assigned=platform::CourseWorldRuntime{};assigned=moved;
    moved.reset();require(assigned.query_ready(),"copy assignment has no self-pointer alias");
    const auto generation=assigned.generation();auto bad=expected;bad[0]^=1u;
    require(!assigned.admit_lane(0u,bad.data(),bad.size(),{},&error)&&
            assigned.generation()==generation&&assigned.query_ready()&&
            assigned.source_identity(0u).path_token==id.path_token,
            "rejected replacement preserves world identity and generation");
    auto matrix=platform::course_world_identity_transform();matrix[12]=std::numeric_limits<float>::infinity();
    require(!assigned.set_transform(0u,matrix,&error)&&assigned.generation()==generation,
            "non-finite transform rejection is atomic");
    require(!assigned.admit_lane(4u,expected.data(),expected.size(),{},&error),"fifth PC root rejects");
    // A real translation verifies that the owner forwards the supplied area
    // transform, rather than always using diagnostic identity behind the API.
    const auto& q=pack.quads[0];platform::CourseWorldGroundSample before{},after{};
    require(assigned.ground_at(q.center_xz[0],q.center_xz[1],q.vertices[0][1],before),"source origin query");
    matrix=platform::course_world_identity_transform();matrix[12]=16;matrix[13]=3;matrix[14]=-16;
    require(assigned.set_transform(0u,matrix,&error)&&
            assigned.ground_at(q.center_xz[0]+16,q.center_xz[1]-16,q.vertices[0][1]+3,after)&&
            after.ground.quad_index==before.ground.quad_index&&
            std::fabs(after.ground.height-before.ground.height-3.0f)<1e-4f,
            "source-to-world transform reaches native grid and output height");
    std::printf("native world: %u source-center hits, primary=%u, lengths=%u\n",hits,
                pack.pc_layout.primary_polygon_count,pack.pc_layout.primary_length_count);
}
void check_ground_chain(const platform::CourseCollisionPack& pack,const char* driving_path){
    platform::DrivingDataPack data{};std::string error;
    require(platform::load_driving_data_pack_file(driving_path,data,&error),"retail driving data loads");
    platform::CourseWorldRuntime world{};
    require(world.admit_lane(0u,pack.pc_coli0200.data(),pack.pc_coli0200.size(),{},&error)&&
            world.set_transform(0u,platform::course_world_identity_transform(),&error),"explicit ground-chain source frame");
    auto views=world.tables();
    unsigned poses=0u,stages=0u;
    for(std::size_t i=0;i<pack.pc_layout.primary_polygon_count&&poses<24u;i+=5u){
        const auto& q=pack.quads[i];
        std::array<float,3> position{{q.center_xz[0],q.vertices[0][1],q.center_xz[1]}};
        platform::VehicleRoadContactFrame footprints{};
        if(!platform::sample_vehicle_road_contacts(world,position,0,footprints)||footprints.hit_count!=4u)continue;
        platform::PcVehicleControlState car{};
        require(platform::configure_pc_vehicle_control(car,data,5,0),"retail parameters configure explicit legacy arithmetic views");
        driving::Bytes e(car.event.data(),car.event.size()),w(car.work.data(),car.work.size()),p(car.parameters.data(),car.parameters.size());
        const auto params=car.parameters;
        std::array<std::uint8_t,128> bytes{};
        driving::PcMatrixStack matrix{driving::Bytes(bytes.data(),bytes.size()),0,0,2};
        driving::pc_matrix_identity(matrix);
        driving::pc_matrix_set_translation(matrix,{position[0],footprints.ground_height+0.25f,position[2]});
        driving::pc_matrix_get(matrix,w.sub(0x10u,64u));
        driving::EasyLctPredictionState prediction{};
        driving::CourseWorldQuery query{views,matrix,prediction};
        driving::GroundCollisionContext context{query,{0,0}};
        const auto wheels=driving::embedded_wheels(w);
        for(unsigned repeat=0;repeat<3u;++repeat){
            driving::calc_ground_coli_face(e,w,p,wheels,context);++stages;
            require(e.u32(0x230)<pack.quads.size()&&static_cast<std::uint16_t>(e.i16(0x64))<pack.pc_layout.primary_length_count,
                    "original car center resolves source polygon and course length");
            for(const auto& wheel:wheels)
                require(wheel.u32(0x10)<pack.quads.size()&&wheel.u32(0x14)!=1u&&std::isfinite(wheel.f32(0x3c)),
                        "original CalcGroundColiFace resolves four source wheel contacts");
            driving::car_sus_coli_check(e,w,p,wheels,matrix);++stages;
            driving::car_sus_bump_push(e,w,p,wheels,matrix);++stages;
            driving::suspension_force(w,p);++stages;
            driving::tire_load(e,w,p,{},matrix);++stages;
            require(matrix.depth==0&&matrix.current_offset==0&&params==car.parameters,
                    "native ground/suspension/load chain preserves stack and source parameters");
            for(const auto& wheel:wheels)
                require(std::isfinite(wheel.f32(0x34))&&std::isfinite(wheel.f32(0x38)),
                        "native suspension force and tire load stay finite");
            require(e.u32(0x2b4)==0xdead0001u,"ground chain never follows guest parameter pointer");
        }
        ++poses;
    }
    require(poses==24u,"real course must support 24 independent original ground-chain poses");
    std::printf("original ground chain: %u poses, %u retained native stage calls\n",poses,stages);
}
}
int main(int argc,char** argv){
    try{
        if(argc!=7){std::fprintf(stderr,"usage: test_course_world_runtime COL2 DRV1 total primary lengths lists\n");return 2;}
        platform::CourseCollisionPack pack{};std::string error;
        require(platform::load_course_collision_pack_file(argv[1],pack,&error),"owned COLI0200 pack loads");
        check_layout(pack,unsigned(std::stoul(argv[3])),unsigned(std::stoul(argv[4])),
                     unsigned(std::stoul(argv[5])),unsigned(std::stoul(argv[6])));
        check_rejections(pack);check_owner(pack);check_ground_chain(pack,argv[2]);
        std::printf("course_world_runtime: %u checks passed\n",checks);return 0;
    }catch(const std::exception& ex){std::fprintf(stderr,"FAIL after %u checks: %s\n",checks,ex.what());return 1;}
}

#include "platform/course_environment_runtime.hpp"
#include "platform/world_source_pack.hpp"
#include "support/environment_fixture.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <utility>
using namespace outrun;
namespace {
unsigned checks{};
void require(bool condition,const char* message){
    ++checks;if(!condition){std::fprintf(stderr,"FAILED: %s\n",message);std::exit(1);}
}
void admit(platform::CourseEnvironmentRuntime& owner,const test::EnvironmentFixture& source){
    std::string error;
    for(unsigned lane=0;lane<3u;++lane){
        if(source[lane].empty())require(owner.admit_absent_lane(lane,&error),"explicit absent source");
        else {
            const auto raw=test::environment_raw(source[lane]);
            require(owner.admit_lane(lane,raw.data(),raw.size(),{15u,0x1234u,lane+1u,42u},&error),"admit bounded source");
        }
    }
}
}
int main(int argc,char** argv){
    auto matrix=test::environment_matrix();
    // Exact integer-valued 90 degree Y rotation with nonzero translation.
    driving::Bytes m(matrix.data(),64u);
    m.putf(0u,0.0f);m.putf(8u,-1.0f);m.putf(0x20u,1.0f);m.putf(0x28u,0.0f);
    m.putf(0x30u,11.0f);m.putf(0x34u,13.0f);m.putf(0x38u,17.0f);
    for(unsigned seed=0;seed<128u;++seed){
        const auto source=test::environment_fixture(seed,true);
        auto copy=source;
        const auto before=driving::inspect_course_environment(test::environment_views(copy));
        platform::CourseEnvironmentRuntime owner;admit(owner,source);std::string error;
        require(owner.inputs_ready()&&!owner.initialized(),"admitted does not mean initialized");
        require(owner.initialize(matrix,&error),"initialize original environment walks");
        require(owner.initialized(),"complete native environment published");
        for(unsigned lane=0;lane<2u;++lane){
            auto expected=source[lane];driving::Bytes bytes(expected.data(),expected.size());
            for(unsigned slot=0;slot<3u;++slot){
                const auto& list=before.lists[lane][slot];
                for(unsigned i=0;i<list.records;++i){
                    const auto at=list.offset+i*0xb0u;
                    if(static_cast<std::uint16_t>(bytes.i16(at))!=0xfffeu)bytes.put32(at+8u,0x7f7fffffu);
                }
            }
            require(owner.payload(lane)==expected,"all other fog/sun bytes and aliases preserved");
        }
        auto expected=source[2];driving::Bytes bytes(expected.data(),expected.size());
        for(unsigned i=0;i<before.spline_records;++i){
            const auto at=i*0x2cu;
            if(bytes.f32(at+0x0cu)==-100.0f)continue;
            const auto x=bytes.f32(at+0x10u),y=bytes.f32(at+0x14u),z=bytes.f32(at+0x18u);
            bytes.putf(at+0x10u,z+11.0f);bytes.putf(at+0x14u,y+13.0f);bytes.putf(at+0x18u,-x+17.0f);
        }
        require(owner.payload(2u)==expected,"spline active XYZ transformed; skipped/end records untouched");
        require(owner.initialize(matrix)&&owner.payload(2u)==expected,"reinitialize starts from pristine source");
        auto copied=owner;owner.reset();
        require(!owner.inputs_ready()&&!owner.initialized()&&copied.payload(2u)==expected,"copy has independent ownership");
        auto moved=std::move(copied);
        require(!copied.initialized()&&!copied.inputs_ready()&&moved.payload(2u)==expected,"move clears old readiness");
        auto invalid_matrix=matrix;driving::Bytes(invalid_matrix.data(),64).putf(0,std::numeric_limits<float>::quiet_NaN());
        require(!moved.initialize(invalid_matrix,&error)&&!error.empty()&&moved.payload(2u)==expected,
                "bad matrix leaves last initialized world unchanged");
    }
    auto source=test::environment_fixture(2u,true);
    platform::CourseEnvironmentRuntime owner;admit(owner,source);
    require(owner.initialize(matrix),"valid baseline before malformed admissions");
    const auto old=owner.payload(0u);
    auto raw=test::environment_raw(source[0]);
    driving::Bytes(raw.data(),raw.size()).put32(4u,0xfffffffcu);
    std::string error;
    require(!owner.admit_lane(0,raw.data(),raw.size(),{},&error)&&owner.payload(0u)==old,
            "out-of-bounds root rejected atomically");
    raw=test::environment_raw(source[0]);driving::Bytes(raw.data(),raw.size()).put32(0,1u);
    require(!owner.admit_lane(0,raw.data(),raw.size(),{},&error),"incorrect size prefix rejected");
    // Every truncation is given a VALID transport prefix; bounded parsing must
    // reject it until all three list terminators are accessible.
    for(std::size_t length=0;length<source[0].size();length+=17u){
        auto partial=source[0];partial.resize(length);auto bad=test::environment_raw(partial);
        platform::CourseEnvironmentRuntime trial;
        const bool ok=trial.admit_lane(0,bad.data(),bad.size(),{},&error);
        if(ok){
            // Trailing bytes after the final two-byte terminator are not read
            // by the PC, so truncating only that padding is legitimately valid.
            auto views=test::environment_views(source);
            const auto layout=driving::inspect_course_environment(views);
            std::size_t required=12u;
            for(const auto& list:layout.lists[0])
                required=std::max(required,std::size_t(list.offset)+list.records*0xb0u+2u);
            require(length>=required,"only unread tail padding may be truncated");
        }else require(!error.empty(),"malformed admission describes its failure");
    }
    // Header/record overlaps must not turn an admitted sentinel walk into a
    // different control flow after the first +8 reset. Exact suffix aliases
    // remain valid and are exercised by the original-code fixtures as well.
    raw=test::environment_raw(source[0]);driving::Bytes(raw.data(),raw.size()).put32(4u,0u);
    require(!owner.admit_lane(0,raw.data(),raw.size(),{},&error),"header-overlapping root rejected");
    test::EnvironmentFixture overlap{};overlap[0].resize(12u+2u*0xb0u+16u,0u);
    driving::Bytes overlap_bytes(overlap[0].data(),overlap[0].size());
    overlap_bytes.put32(0u,12u);overlap_bytes.put32(4u,20u);overlap_bytes.put32(8u,12u);
    overlap_bytes.put16(12u+0xb0u,0xffffu);overlap_bytes.put16(20u+0xb0u,0xffffu);
    raw=test::environment_raw(overlap[0]);
    require(!owner.admit_lane(0,raw.data(),raw.size(),{},&error),"incompatible overlapping record walks rejected");
    overlap_bytes.put32(4u,12u+0xb0u);raw=test::environment_raw(overlap[0]);
    platform::CourseEnvironmentRuntime suffix;
    require(suffix.admit_lane(0,raw.data(),raw.size(),{},&error),"exact terminal suffix alias remains accepted");
    // Host errors are not silently treated as the original -100 skip marker.
    auto malformed=source;driving::Bytes(malformed[2].data(),malformed[2].size()).putf(0x10u,std::numeric_limits<float>::infinity());
    auto bad=test::environment_raw(malformed[2]);
    require(!owner.admit_lane(2,bad.data(),bad.size(),{},&error),"nonfinite active position rejected");
    require(!owner.admit_absent_lane(3u,&error),"fourth environment lane rejected");
    {
        // Per-frame 44A8DF binding refuses to run without its real inputs and
        // leaves the retained state untouched when it does.
        platform::CourseEnvironmentRuntime frame_owner;platform::CourseWorldRuntime world;
        platform::CourseEnvironmentLiveState live{};live.time_7d2934=7;
        std::array<std::uint32_t,6> flags{{1u,0u,1u,0u,1u,0u}};std::uint32_t phase=3u;
        std::array<std::uint8_t,0x70> car{};std::array<std::uint8_t,0x110> camera{};
        const driving::Bytes cv(car.data(),car.size()),cam(camera.data(),camera.size());
        const driving::PcEnvironmentTransition transition{};
        require(!frame_owner.update_frame(cv,cam,world,transition,flags,phase,live,&error)&&!error.empty(),
            "frame update requires initialized environment records");
        admit(frame_owner,test::environment_fixture(3u,true));
        require(frame_owner.initialize(matrix,&error),"frame owner initialized");
        require(!frame_owner.update_frame(cv,cam,world,transition,flags,phase,live,&error)&&!error.empty(),
            "frame update requires the primary collision course");
        require(!frame_owner.update_frame(cv.sub(0,0x60),cam,world,transition,flags,phase,live,&error),
            "truncated vehicle view rejected");
        require(phase==3u&&flags[0]==1u&&flags[4]==1u&&live.time_7d2934==7,"rejected frames publish nothing");
    }
    if(argc>1){
        platform::WorldSourcePack pack;
        require(platform::load_world_source_pack_file(argv[1],pack,&error),"retail world source available");
        platform::CourseEnvironmentRuntime retail;
        constexpr std::array<std::uint32_t,3> fields{{0x24u,0x28u,0x2cu}};
        for(unsigned lane=0;lane<3;++lane){
            const platform::WorldSourceEntry* entry=nullptr;
            for(const auto& e:pack.entries)if(e.descriptor_field==fields[lane])entry=&e;
            require(entry!=nullptr,"real field resolves original environment");
            require(retail.admit_lane(lane,platform::world_source_bytes(pack,*entry),entry->size,
                {pack.descriptor_index,pack.descriptor_token,entry->guest_path_token,entry->crc32},&error),"real source structurally admitted");
        }
        require(retail.initialize(matrix,&error),"retail BEAC environment initialized");
        for(unsigned lane=0;lane<2;++lane)for(unsigned slot=0;slot<3;++slot)
            require(retail.layout().lists[lane][slot].active==1u&&
                    retail.layout().lists[lane][slot].records==1u,"retail BEAC list has one active record");
        require(retail.layout().spline_present&&retail.layout().spline_records==0u,
            "retail BEAC spline is an end marker, not a fog table");
    }
    std::printf("course_environment: %u checks passed\n",checks);
}

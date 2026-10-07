#include "platform/vehicle_road_contact.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include "platform/vehicle_visual.hpp"

using namespace outrun::platform;
namespace {
unsigned checks{};
void require(bool value,const char* message){++checks;if(!value){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
CourseCollisionQuad quad(float x0,float x1,float z0,float z1,float y0,float y1,
                         std::uint32_t material){
    CourseCollisionQuad q{};q.flags=7u;q.material=material;
    q.vertices={{{{x0,y0,z0}},{{x1,y0,z0}},{{x1,y1,z1}},{{x0,y1,z1}}}};
    q.center_xz={{(x0+x1)*0.5f,(z0+z1)*0.5f}};return q;
}
}
int main(int argc,char** argv){
    CourseCollisionPack pack{};pack.quads.push_back(quad(-4,4,-4,4,1,2,3));
    VehicleRoadContactFrame frame{};
    require(sample_vehicle_road_contacts(pack,{{0,1.5f,0}},0.0f,frame),
            "four wheel footprints resolve sloped collision");
    require(frame.supported&&frame.hit_count==4u&&frame.hit_mask==15u,
            "four contacts form supported chassis plane");
    require(frame.normal[1]>0.9f&&frame.ground_height>1.4f&&frame.ground_height<1.6f,
            "aggregate plane retains slope and center height");
    for(const auto& wheel:frame.wheels)
        require(wheel.supported&&wheel.material==3u&&std::fabs(wheel.suspension_delta)<0.001f,
                "planar wheel contact has no false suspension residual");
    require(!sample_vehicle_road_contacts(pack,{{20,1.5f,20}},0.0f,frame)&&
            frame.hit_count==0u,"unsupported vehicle pose rejects");
    VehicleRoadSweepStats sweep{};
    require(sample_vehicle_road_contacts_swept(
                pack,{{-1.0f,1.5f,0.0f}},0.0f,{{1.0f,1.5f,0.0f}},0.0f,
                frame,sweep,0.2f)&&sweep.sampled_poses==11u&&
            sweep.wheel_queries==44u,"swept footprint samples the complete movement");
    require(!sample_vehicle_road_contacts_swept(
                pack,{{0.0f,1.5f,0.0f}},0.0f,{{20.0f,1.5f,0.0f}},0.0f,
                frame,sweep,0.2f)&&sweep.sampled_poses<101u&&
            sweep.failure==VehicleRoadSweepFailure::Unsupported,
            "swept footprint rejects before unsupported endpoint");
    require(!sample_vehicle_road_contacts_swept(pack,{{20,1.5f,0}},0,{{0,1.5f,0}},0,
                frame,sweep)&&sweep.sampled_poses==1u,
            "unsupported starting footprint cannot jump back onto road");
    require(sample_vehicle_road_contacts_swept(pack,{{0,1.5f,0}},0,{{0,1.5f,0}},3.0f,
                frame,sweep)&&sweep.sampled_poses>2u,
            "stationary rotation samples intermediate wheel arcs");
    CourseCollisionPack isolated{};
    for(unsigned i=1u;i<=4u;++i){
        const auto& local=VehicleVisualObjectTranslations[i];
        for(float sign:{-1.0f,1.0f}){
            const float x=sign*local[0],z=sign*local[2];
            isolated.quads.push_back(quad(x-0.12f,x+0.12f,z-0.12f,z+0.12f,0,0,2));
        }
    }
    require(sample_vehicle_road_contacts(isolated,{{0,0,0}},0,frame)&&
            sample_vehicle_road_contacts(isolated,{{0,0,0}},3.14159265f,frame),
            "both endpoints of rotation have four isolated supported wheel pads");
    require(!sample_vehicle_road_contacts_swept(isolated,{{0,0,0}},0,{{0,0,0}},3.14159265f,
                frame,sweep)&&sweep.failure==VehicleRoadSweepFailure::Unsupported&&sweep.sampled_poses>1u,
            "rotation sweep detects missing intermediate support despite valid endpoints");
    const float huge=std::numeric_limits<float>::max();
    require(!sample_vehicle_road_contacts_swept(pack,{{-huge,0,0}},0,{{huge,0,0}},0,
                frame,sweep)&&sweep.failure==VehicleRoadSweepFailure::BudgetExceeded&&
            sweep.wheel_queries==0u,"huge finite delta rejects before integer conversion");
    require(!sample_vehicle_road_contacts_swept(pack,{{0,0,0}},0,{{0,0,-1}},0,
                frame,sweep,std::numeric_limits<float>::denorm_min())&&
            sweep.failure==VehicleRoadSweepFailure::BudgetExceeded,
            "tiny step rejects rather than overflowing sweep count");
    require(!sample_vehicle_road_contacts_swept(pack,{{0,0,0}},0,{{0,0,-1}},0,
                frame,sweep,0)&&sweep.failure==VehicleRoadSweepFailure::InvalidInput,
            "invalid step has explicit failure reason");
    require(sample_vehicle_road_contacts_swept(pack,{{0,1.5f,0}},3.13f,{{0,1.5f,0}},-3.13f,
                frame,sweep)&&sweep.sampled_poses==2u,
            "angle wrapping follows short wheel arc, not a full revolution");
    if(argc==2){
        CourseCollisionPack cape{};std::string error;
        require(load_course_collision_pack_file(argv[1],cape,&error),
                "Cape Town collision pack loads");
        require(sample_vehicle_road_contacts(cape,{{0.0f,0.0f,-4.0f}},0.0f,frame),
                "Cape Town start pose supports at least three wheel footprints");
        require(frame.hit_count>=3u&&frame.normal[1]>0.05f&&
                std::isfinite(frame.ground_height),
                "Cape Town start contact plane is finite");
        require(frame.wheels[0].material==2u&&frame.wheels[1].material==2u&&
                frame.wheels[2].material==2u&&frame.wheels[3].material==2u,
                "all start wheels receive original PC kind bit instead of float bits");
        std::printf("Cape Town start mask=%x materials=%u/%u/%u/%u ground=%.3f\n",
                    frame.hit_mask,frame.wheels[0].material,frame.wheels[1].material,
                    frame.wheels[2].material,frame.wheels[3].material,
                    frame.ground_height);
    }else if(argc!=1){
        std::fprintf(stderr,"usage: test_vehicle_road_contact [OR2COL2]\n");return 2;
    }
    std::printf("vehicle road contacts: %u checks passed\n",checks);return 0;
}

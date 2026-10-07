#include "platform/vehicle_pose_filter.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
using namespace outrun::platform;
namespace {
unsigned checks{};
void require(bool value,const char* message){++checks;if(!value){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
}
int main(){
    VehiclePoseFilterState state{};
    require(initialize_vehicle_pose_filter(state,1.0f,{{0.0f,1.0f,0.0f}}),"flat pose initializes");
    require(update_vehicle_pose_filter(state,2.0f,{{0.5f,0.8660254f,0.0f}}),"large pose delta updates");
    require(state.max_applied_angle<=0.01801f&&state.max_applied_height<=0.02501f,
            "single-frame pose response is bounded");
    require(state.normal_limited==1u&&state.height_limited==1u,"large deltas are reported");
    for(unsigned i=0;i<240u;++i)
        require(update_vehicle_pose_filter(state,2.0f,{{0.5f,0.8660254f,0.0f}}),"pose converges");
    require(std::fabs(state.ground_height-2.0f)<0.001f&&
            std::fabs(state.normal[0]-0.5f)<0.001f,"filtered pose reaches target");
    require(!update_vehicle_pose_filter(state,2.0f,{{0.0f,0.0f,0.0f}}),"invalid target rejected");
    std::printf("vehicle pose filter: %u checks passed\n",checks);return 0;
}

#include "platform/native_runtime.hpp"
#include <array>
#include <cstdio>
#include <stdexcept>
using namespace outrun::platform;
namespace {
unsigned checks{};
void require(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
bool owned_call(void* user,std::uint32_t pc,const std::uint32_t* args,std::size_t count,std::uint32_t& result){
    return native_start_owned_call(*static_cast<NativeRuntimeContext*>(user),pc,args,count,result);
}
bool owned_ready(void* user,std::uint32_t pc){
    return native_start_owned_resource_ready(*static_cast<NativeRuntimeContext*>(user),pc);
}
}
int main(){
    try{
        NativeRuntimeContext c{};
        c.mode_descriptors[13u]={0u,0x0049db20u,0x0049dd40u,0x00499d80u};
        c.mode_descriptors[16u]={0u,0x00499d90u,0x0049c840u,0x00499e50u};
        c.mode_state.current=13u;
        c.game_mode.game_variant=1u;
        c.game_mode.course_load_success=1u; // START already owns the selected course.
        auto& s=c.start_mode;
        s.active=true;s.stage=59u;s.selection_active_836374=true;
        s.course_choice_655b59=0u;
        s.mode_countdown_780250=2;
        const NativeModeServices services{&c,nullptr,owned_ready,owned_call};

        require(native_runtime_mode_control(c,services),"first r148 START tail dispatcher tick");
        require(s.stage==64u&&s.stage60_phase==3u&&s.stage60_route==11u,
                "offline stage59/60 reaches authored route 11 and stage64");
        require(s.start_ready_4557f0_checks==1u&&s.start_bootstrap_440380_calls==1u&&
                s.start_bootstrap_event8_setups==1u&&s.start_bootstrap_records_49fa80==1u,
                "4557F0/440380 ordinary offline path is owned once");
        outrun::driving::Bytes creation(c.vehicle_creation.object.data(),c.vehicle_creation.object.size());
        require(creation.u8(0x109)==1&&creation.u32(0)==8&&creation.u32(4)==0x4081&&creation.u32(8)==0&&creation.u8(13)==1,
                "START writes the real creation descriptor, not just an invocation counter");
        require((c.event_state.slots[8u].flags&1u)!=0u&&
                (c.event_state.slots[0x167u].flags&1u)!=0u&&
                (c.event_state.slots[0x16cu].flags&1u)!=0u,
                "ordinary offline bootstrap installs event8 and START race events");
        require(s.start_route_aux_4f0d10_calls==std::array<std::uint32_t,2>{{1u,1u}},
                "both null protected route roots complete as PC no-ops");
        require(s.start_route_init_4871a0_calls==1u&&s.start_route_scene_82e7d4==11u&&
                s.start_route_mode_82e7d8==1u&&s.start_route_state_82e7e4==2u&&
                s.start_route_entry_count==24u&&s.start_route_player_and_mask==0xffffffb9u&&
                s.start_route_player_or_bits==0x38u,
                "4871A0 route11 globals/table span and player flag patch match PC");
        require(c.mode_state.transition_pending==0u&&s.game_requests==0u,
                "stage60 never fabricates GAME request");

        require(native_runtime_mode_control(c,services),"second r148 START tail dispatcher tick");
        require(s.stage==65u&&s.game_requests==0u&&s.mode_countdown_780250==1,
                "stage64 cleanup completes but 43FA90 timer still gates GAME");
        require(s.start_gate_45a920_checks==1u&&s.start_cleanup_4999f0_calls==1u&&
                s.start_cleanup_428600_calls==1u,
                "offline stage64 gate and first two cleanup owners execute");
        require(s.start_cleanup_42dfb0_count==3u&&
                s.start_cleanup_42dfb0_ids==std::array<std::uint32_t,3>{{0x15u,0x2fu,0x49u}},
                "shared pool cleanup uses exact 15/2F/49 IDs");
        require(s.start_cleanup_4299c0_count==2u&&
                s.start_cleanup_4299c0_ids==std::array<std::uint32_t,2>{{0x2fu,0x49u}},
                "frontend pool cleanup uses exact 2F/49 IDs");
        require(s.start_loader_mode_writes==1u&&s.start_loader_mode_7d34c0==0u,
                "idle synchronous loader accepts final mode 0 write");

        require(native_runtime_mode_control(c,services),"third r148 START tail dispatcher tick");
        require(s.game_requests==1u&&c.mode_state.requested==16u&&
                c.mode_state.transition_pending==1u&&!s.active&&s.exit_calls==1u,
                "43FA90 expiry requests GAME and exits START naturally");
        require(s.start_loader_mode_writes==2u,
                "final loader mode service remains owned on countdown ticks");

        require(native_runtime_mode_control(c,services),"fourth r148 dispatcher commits GAME");
        require(c.mode_state.current==16u&&c.mode_state.previous==13u&&c.game_mode.active&&
                c.game_mode.init_calls==1u&&c.game_mode.control_calls==1u,
                "mode dispatcher commits natural START to GAME transition");
        require(c.game_mode.course_load_success==1u&&c.game_mode.course_load_calls==0u,
                "GAME reuses course already established by START");

        // Explicit fail-closed branches: r148 owns only the first-playable offline path.
        NativeRuntimeContext network{};std::uint32_t result=0u;
        network.start_mode.network_player_count_7df10f=2u;
        require(!native_start_owned_call(network,0x004557f0u,nullptr,0u,result),
                "multiplayer 4557F0 synchronization remains external");
        network.start_mode.network_manager_7df34c_present=true;
        require(!native_start_owned_call(network,0x0045a920u,nullptr,0u,result),
                "network 45A920 branch remains external");
        const std::uint32_t lane=0u;network.start_mode.route_aux_roots_84d6cc[0]=true;
        require(!native_start_owned_call(network,0x004f0d10u,&lane,1u,result),
                "non-null protected route root remains external");
        network.start_mode.start_system_handle_67f614=7;
        require(native_start_owned_call(network,0x004999f0u,nullptr,0u,result)&&network.start_mode.start_system_handle_67f614==-1,
                "live 67F614 loading icon released by 4999F0");
        network.event_function36.loader_resource_pending=1u;const std::uint32_t zero=0u;
        require(native_start_owned_call(network,0x0044fce0u,&zero,1u,result)&&
                network.start_mode.start_loader_mode_writes==0u,
                "44FCE0 does not write mode while native async work is pending");
        require(native_start_owned_call(network,0x0048b310u,nullptr,0u,result)&&result==0u&&
                native_start_owned_call(network,0x00495490u,nullptr,0u,result)&&result==0u,
                "48B310/495490 protected getters expose their zero-initialized offline values");

        NativeRuntimeContext selected{};selected.game_mode.game_variant=1;
        for(unsigned model=0;model<30;++model)for(unsigned colour=0;colour<10;++colour)for(unsigned lane=0;lane<8;++lane){
            require(vehicle_creation_reset_49fa60(selected.vehicle_creation),"reset actual shared creation queue");
            const unsigned variant=model>=15;
            require(native_start_owned_call(selected,0x48b130,&model,1,result)&&native_start_owned_call(selected,0x48b150,&colour,1,result)&&
                native_start_owned_call(selected,0x48b190,&variant,1,result),"original menu setters own model colour variant");
            require(native_start_owned_call(selected,0x48b140,nullptr,0,result)&&result==model,"model getter");
            require(native_start_owned_call(selected,0x48b160,nullptr,0,result)&&result==colour,"colour getter");
            require(native_start_owned_call(selected,0x48b1a0,nullptr,0,result)&&result==variant,"variant getter");
            selected.race.car_world.slot_7dd138=std::uint8_t(lane);
            require(native_start_owned_call(selected,0x440380,nullptr,0,result),"owned offline creation");
            outrun::driving::Bytes q(selected.vehicle_creation.object.data(),selected.vehicle_creation.object.size());
            require(q.u8(0x109)==1&&q.u32(0)==8&&q.u32(4)==0x4081&&q.u32(8)==model&&q.u8(12)==lane&&
                q.u8(13)==(colour<1?1:colour>8?8:colour),"selection reaches exact queued START request");
        }
        selected.race.car_world.slot_7dd138=8;
        require(!native_start_owned_call(selected,0x440380,nullptr,0,result)&&selected.vehicle_creation.fault==0x455ad0,"invalid slot does not create a substitute player");
        NativeRuntimeContext restarted{};restarted.vehicle_creation.object.fill(0xa5);
        auto expected=restarted.vehicle_creation;require(vehicle_creation_reset_49fa60(expected),"expected PC queue reset");
        restarted.mode_descriptors[13]={0,0x49db20,0x49dd40,0x499d80};
        restarted.mode_state.current=32;restarted.mode_state.requested=13;restarted.mode_state.transition_pending=1;
        restarted.start_mode.course_choice_655b59=24;restarted.start_mode.vehicle_colour_655b5a=4;
        restarted.start_mode.vehicle_variant_83036d=1;restarted.race.car_world.slot_7dd138=3;
        require(native_runtime_mode_control(restarted,{}),"ordinary mode transition runs START_Init");
        require(restarted.vehicle_creation.object==expected.object&&restarted.start_mode.course_choice_655b59==24&&
                restarted.start_mode.vehicle_colour_655b5a==4&&restarted.start_mode.vehicle_variant_83036d==1&&restarted.race.car_world.slot_7dd138==3,
                "START_Init resets queue exactly but preserves committed selection");
        restarted.vehicle_creation.fault=0x49fa80;restarted.mode_state.transition_pending=1;
        require(!native_runtime_mode_control(restarted,{})&&!restarted.start_mode.active&&restarted.start_mode.last_missing_service==0x49fa80,
                "a failed creation queue cannot silently restart START");
        std::printf("start_tail_r148: %u checks passed; callback-free offline stage59 -> GAME tail validated\n",checks);
        return 0;
    }catch(const std::exception& e){std::fprintf(stderr,"FAILED: %s\n",e.what());return 1;}
}

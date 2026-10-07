// The network layer ported from the PC code, over the network module's memory (the
// State above). A function not ported yet runs translated (run); a call leaving the
// network code goes to network_external. Included inside pc_network.cpp's namespace.
constexpr std::uint32_t NativeStackBase=0x0efc0000u,NativeStackSize=0x10000u;
struct N {
    NativeRuntimeContext& c;State& s;PcRaceMemory& m;
    N(NativeRuntimeContext& cc,State& ss):c(cc),s(ss),m(ss.memory){}
    std::uint8_t u8(std::uint32_t a){return m.u8(a);}
    std::int8_t i8(std::uint32_t a){return m.i8(a);}
    std::uint16_t u16(std::uint32_t a){return m.u16(a);}
    std::int16_t i16(std::uint32_t a){return m.i16(a);}
    std::uint32_t u32(std::uint32_t a){return m.u32(a);}
    std::int32_t i32(std::uint32_t a){return m.i32(a);}
    float f32(std::uint32_t a){return m.f32(a);}
    void p8(std::uint32_t a,std::uint32_t v){m.put8(a,std::uint8_t(v));}
    void p16(std::uint32_t a,std::uint32_t v){m.put16(a,std::uint16_t(v));}
    void p32(std::uint32_t a,std::uint32_t v){m.put32(a,v);}
    void pf(std::uint32_t a,float v){m.putf(a,v);}
    // A network function (native when ported, else translated) or a call leaving it.
    std::uint32_t call(std::uint32_t pc,std::initializer_list<std::uint32_t> args={},std::uint32_t ecx=0){
        return call_list(pc,std::vector<std::uint32_t>(args),ecx);
    }
    std::uint32_t call_list(std::uint32_t pc,const std::vector<std::uint32_t>& args,std::uint32_t ecx=0){
        const TranslatedModule module{network_functions,nullptr,{},nullptr,nullptr,nullptr,network_code_data};
        if(translated_has(module,pc))return run_list(c,pc,ecx,args);
        std::uint32_t eax=0;
        if(network_native(c,s,pc,ecx,args.data(),args.size(),eax))return eax;
        PcRaceCall k{};k.pc=pc;k.ecx=ecx;k.argc=std::uint32_t(args.size());
        for(std::size_t i=0;i<args.size();++i)k.args.at(i)=args[i];
        return network_external(c,s,k);
    }
    // A Winsock import (index into the WS2_32 table 5961F8) with its arguments.
    std::uint32_t ws(std::uint32_t index,std::initializer_list<std::uint32_t> args){
        const std::vector<std::uint32_t> a(args);std::uint32_t pop=0;
        return winsock_call(s,index,[&](unsigned k){return k<a.size()?a[k]:0u;},pop);
    }
    // thiscall helper
    std::uint32_t method(std::uint32_t pc,std::uint32_t self,std::initializer_list<std::uint32_t> args={}){return call(pc,args,self);}
    // ---- guest scratch frames (locals passed by address) ----
    static std::uint32_t& sp(){static std::uint32_t v=NativeStackBase+NativeStackSize;return v;}
    struct Frame {
        N& n;std::uint32_t saved,base;
        Frame(N& nn,std::uint32_t size):n(nn),saved(sp()){
            sp()=(sp()-size)&~0xfu;base=sp();
            if(base<NativeStackBase)throw std::runtime_error("network: native scratch stack overflow");
            for(std::uint32_t k=0;k<size;k+=4)n.m.put32(base+k,0);
        }
        ~Frame(){sp()=saved;}
        std::uint32_t operator()(std::uint32_t off)const{return base+off;}
    };
#include "platform/pc_network_tick.inc"
#include "platform/pc_network_session.inc"
#include "platform/pc_network_init.inc"
#include "platform/pc_network_bd.inc"
#include "platform/pc_network_lobby.inc"
#include "platform/pc_network_message.inc"
};
bool network_native(NativeRuntimeContext& c,State& s,std::uint32_t pc,std::uint32_t ecx,const std::uint32_t* args,std::size_t n,std::uint32_t& eax){
    // debug: OR2_NET_TR=all (or a comma list of hex pcs) keeps those translated
    static const std::vector<std::uint32_t> kept=[]{
        std::vector<std::uint32_t> v;const char* e=std::getenv("OR2_NET_TR");
        if(e){if(std::string(e)=="all")v.push_back(0xffffffffu);
            else for(const char* q=e;*q;){char* end=nullptr;v.push_back(std::uint32_t(std::strtoul(q,&end,16)));q=*end?end+1:end;}}
        return v;}();
    for(const auto k:kept)if(k==0xffffffffu||k==pc)return false;
    N net(c,s);
    auto a=[&](std::size_t k){return k<n?args[k]:0u;};
    switch(pc){
    case 0x454670u:net.tick_454670();eax=0;return true;
    case 0x453c60u:net.listeners_453c60(ecx);eax=0;return true;
    case 0x454260u:eax=net.session_event_454260(ecx)?1u:0u;return true;
    case 0x4f5680u:net.race_start_4f5680();eax=0;return true;
    case 0x4942e0u:eax=net.service_event_4942e0(ecx);return true;
    case 0x494790u:net.service_494790(ecx);eax=0;return true;
    case 0x491990u:eax=net.open_socket_491990(a(0));return true;
    case 0x48f8e0u:net.clear_event_48f8e0(ecx);eax=ecx;return true;
    case 0x56ee70u:eax=net.u32(ecx+0x14u);return true;
    case 0x4f1f00u:eax=net.client_state(ecx);return true;
    case 0x494370u:eax=net.array_at_494370(ecx,a(0));return true;
    case 0x516d10u:eax=net.interfaces_516d10(ecx,a(0));return true;
    case 0x516dc0u:eax=net.local_address_516dc0(ecx);return true;
    case 0x524e00u:net.p32(ecx,a(0));eax=ecx;return true;          // bdInAddr(v)
    case 0x524cf0u:net.p32(ecx,0xdeadbeefu);eax=0;return true;     // ~bdInAddr
    case 0x534a20u:net.p32(ecx,0xff00ff00u);eax=ecx;return true;   // bdInAddr()
    case 0x45acb0u:net.startup_45acb0();eax=0;return true;
    case 0x4f52e0u:net.race_start_init_4f52e0(a(0));eax=0;return true;
    case 0x494270u:net.service_start_494270(ecx);eax=0;return true;
    case 0x523d60u:net.string_523d60(ecx,a(0));eax=ecx;return true;
    case 0x523cf0u:net.string_copy_523cf0(ecx,a(0));eax=ecx;return true;
    case 0x523df0u:net.string_free_523df0(ecx);eax=0;return true;
    case 0x45aae0u:net.array_grow(ecx,a(0),true);eax=0;return true;
    case 0x45ab50u:net.array_grow(ecx,a(0),false);eax=0;return true;
    case 0x45aaa0u:net.strings_clear_45aaa0(ecx);eax=0;return true;
    case 0x494730u:eax=net.service_list_494730();return true;
    case 0x4944a0u:net.service_list_4944a0(ecx);eax=ecx;return true;
    case 0x45ac10u:eax=net.client_45ac10();return true;
    case 0x438c60u:eax=net.registry_438c60();return true;
    case 0x531060u:net.client_531060(ecx);eax=ecx;return true;
    case 0x533510u:net.task_533510(ecx,a(0));eax=ecx;return true;
    case 0x52fd20u:case 0x531c10u:{float l;const std::uint32_t b=a(1);std::memcpy(&l,&b,4);net.hashmap(ecx,a(0),l);eax=0;return true;}
    case 0x567c10u:net.ptr_grow_567c10(ecx,a(0));eax=0;return true;
    case 0x534090u:net.ptr_push_534090(ecx,a(0));eax=0;return true;
    case 0x5253d0u:eax=N::pow2_5253d0(a(0));return true;
    case 0x534080u:net.p32(ecx,0);net.p32(ecx+4u,0);net.p32(ecx+8u,0);eax=ecx;return true;
    case 0x490940u:net.update_490940(ecx);eax=0;return true;
    case 0x490690u:net.receive_all_490690(ecx,a(0));eax=0;return true;
    case 0x490030u:eax=net.parse_490030(ecx,a(0),a(1));return true;
    case 0x491a90u:eax=net.receive_491a90(ecx,a(0),a(1));return true;
    case 0x491b60u:eax=net.send_491b60(ecx,a(0),a(1));return true;
    case 0x491860u:net.read_491860(ecx,a(0),a(1));eax=0;return true;
    case 0x491900u:net.write_491900(ecx,a(0),a(1));eax=0;return true;
    case 0x491940u:net.write_491940(ecx,a(0),a(1));eax=0;return true;
    case 0x491840u:eax=net.checksum_491840(ecx,a(0),a(1));return true;
    case 0x4917a0u:net.address_4917a0(ecx,a(0),a(1),a(2),a(3));eax=1;return true;
    case 0x4917d0u:net.write_address_4917d0(ecx,a(0));eax=0;return true;
    case 0x491800u:net.read_address_491800(ecx,a(0));eax=0;return true;
    case 0x491890u:net.p32(ecx+0x10u,0);net.p32(ecx+8u,0);eax=0;return true;
    case 0x490670u:net.stream_490670(ecx);eax=ecx;return true;
    case 0x490810u:net.names_490810();eax=0;return true;
    case 0x4f4590u:net.own_player_4f4590(ecx);eax=0;return true;
    case 0x520840u:net.player_info_520840(ecx,a(0));eax=0;return true;
    case 0x4f4790u:net.write_state_4f4790(ecx,a(0));eax=0;return true;
    case 0x4f4ed0u:net.queue_4f4ed0(ecx,a(0));eax=0;return true;
    case 0x4f4d80u:net.flush_4f4d80(ecx);eax=0;return true;
    case 0x4f4950u:net.read_state_4f4950(ecx,a(0));eax=0;return true;
    case 0x4f4b80u:net.read_state_4f4950(a(1),a(0));eax=0;return true;
    case 0x4f51e0u:net.race_records_4f51e0();eax=0;return true;
    case 0x490020u:net.listener_reset_490020(ecx);eax=0;return true;
    case 0x490530u:net.close_490530(ecx,a(0));eax=0;return true;
    case 0x48f900u:net.event_clear_48f900(ecx);eax=0;return true;
    case 0x48f910u:net.event_base_48f910(ecx);eax=ecx;return true;
    case 0x4f46a0u:net.set_id_4f46a0(ecx,a(0)&0xffffu);eax=0;return true;
    case 0x4549f0u:net.drop_slots_4549f0(a(0));eax=0;return true;
    case 0x454bb0u:net.remove_player_454bb0(ecx,a(0));eax=0;return true;
    case 0x454200u:net.end_session_454200(ecx);eax=0;return true;
    case 0x4f4cd0u:net.goodbye_4f4cd0(ecx,a(0));eax=0;return true;
    case 0x4f4700u:eax=net.state_byte_4f4700(ecx);return true;
    case 0x515ec0u:net.host_counters_515ec0(a(0));eax=0;return true;
    case 0x515ee0u:net.host_setup_515ee0(a(0));eax=0;return true;
    case 0x515ef0u:net.host_players_515ef0(a(0));eax=0;return true;
    case 0x515f70u:net.host_online_515f70(a(0));eax=0;return true;
    case 0x5163c0u:net.host_messages_5163c0();eax=0;return true;
    case 0x4f4bd0u:net.player_reset_4f4bd0(ecx);eax=0;return true;
    case 0x5207f0u:net.info_reset_5207f0(ecx);eax=0;return true;
    case 0x454490u:net.slot_insert_454490(ecx,a(0),a(1));eax=0;return true;
    case 0x4544f0u:net.slot_remove_4544f0(ecx,a(0));eax=0;return true;
    case 0x454a90u:net.player_slots_454a90(a(0));eax=0;return true;
    case 0x4f4680u:eax=std::uint32_t(std::int32_t(net.u16(net.u32(a(0))))-std::int32_t(net.u16(net.u32(a(1)))));return true;
    case 0x45a280u:net.race_frame_45a280();eax=0;return true;
    case 0x45a0c0u:net.race_frame_45a0c0();eax=0;return true;
    case 0x459f60u:net.drop_lost_459f60(ecx);eax=0;return true;
    case 0x4f5b90u:net.ping_4f5b90();eax=0;return true;
    case 0x4f5c70u:net.progress_4f5c70(a(0),a(1));eax=0;return true;
    case 0x4f5340u:case 0x4f5a60u:net.race_state_4f5a60(a(0),a(1));eax=0;return true;
    case 0x4575b0u:eax=net.racing_4575b0(a(0));return true;
    case 0x456ae0u:eax=net.connected_456ae0(ecx,a(0))?1u:0u;return true;
    case 0x4f53b0u:eax=net.versus_4f53b0()?1u:0u;return true;
    case 0x455d40u:eax=net.u32(0x7f1938u);return true;
    case 0x455d30u:net.p32(0x7f1938u,a(0));eax=a(0);return true;
    case 0x454c60u:if(ecx==0x98a650u)return false;   // the online session start stays translated (Demonware)
        net.start_session_454c60(ecx,a(0)&0xffu);eax=0;return true;
    case 0x430630u:net.make_current_430630(ecx);eax=0;return true;
    case 0x430690u:net.host_430690();eax=0;return true;
    case 0x4306a0u:net.search_4306a0();eax=0;return true;
    case 0x430700u:net.join_slot_430700(a(0));eax=0;return true;
    case 0x4306d0u:net.address_4306d0(a(0));eax=0;return true;
    case 0x430740u:net.join_address_430740(a(0));eax=0;return true;
    case 0x453d20u:net.leave_453d20(ecx,a(0));eax=0;return true;
    case 0x453da0u:net.host_only_453da0(ecx);eax=0;return true;
    case 0x453d50u:net.close_453d50(ecx);eax=0;return true;
    case 0x453dc0u:net.mark_players_453dc0(ecx);eax=0;return true;
    case 0x4305b0u:net.lobby_tick_4305b0(ecx);eax=0;return true;
    case 0x4f58f0u:net.record_reset_4f58f0(ecx);eax=0;return true;
    case 0x4f4580u:net.p32(ecx+0x294u,0x80000000u);eax=0;return true;
    case 0x456ce0u:net.p32(0x7de418u,0);eax=0;return true;
    case 0x491e60u:net.p32(0x830c08u,ecx);eax=net.vcall(ecx,0xc);return true;
    case 0x457210u:net.unpack_race_457210(a(0),a(1),a(2));eax=0;return true;
    case 0x457410u:net.clock_457410(a(0),a(1));eax=0;return true;
    case 0x4f5910u:net.race_state_in_4f5910(a(0),a(1));eax=0;return true;
    case 0x4f53d0u:net.start_sync_4f53d0(a(0),a(1));eax=0;return true;
    case 0x4f55d0u:net.start_ask_4f55d0();eax=0;return true;
    case 0x4f6020u:net.control_in_4f6020(a(0),a(1));eax=0;return true;
    case 0x457790u:net.result_457790(a(0),a(1),a(2),a(4),a(5));eax=0;return true;
    case 0x4578d0u:net.result_4578d0(a(0),a(1),a(3),a(4));eax=0;return true;
    case 0x4f5970u:net.result_in_4f5970(a(0),a(1));eax=0;return true;
    case 0x4f59f0u:net.result_in_4f59f0(a(0),a(1));eax=0;return true;
    case 0x4567d0u:net.packed_4567d0(ecx,a(0),a(1),a(2));eax=0;return true;
    case 0x456560u:net.entry_456560(ecx,a(0));eax=0;return true;
    case 0x4566c0u:net.block_4566c0(ecx);eax=0;return true;
    case 0x456cb0u:net.commrace_456cb0();eax=0;return true;
    case 0x456870u:eax=net.all_done_456870(ecx)?1u:0u;return true;
    case 0x456d70u:eax=net.all_done_456870(0x7de418u)?1u:0u;return true;
    case 0x456b40u:eax=net.connected_456b40(ecx);return true;
    case 0x456e10u:eax=net.connected_456b40(0x7de418u);return true;
    case 0x455ef0u:net.histories_455ef0();eax=0;return true;
    case 0x459210u:net.race_reset_459210();eax=0;return true;
    case 0x4f6250u:for(std::uint32_t k=0;k<0x28u;k+=4)net.p32(a(0)+k,0);eax=0;return true;
    case 0x4591c0u:{if(!net.u32(0x7d68acu)){eax=0;return true;}
        const std::uint32_t v=net.u8(0x7de426u+a(0)*0x6cu);eax=v<1u?1u:(v>8u?8u:v);return true;}
    case 0x4591f0u:eax=net.u32(0x7d68acu)?net.u8(0x7de425u+a(0)*0x6cu):0u;return true;
    case 0x4f5390u:eax=net.u32(net.u32(0x7d68acu)+0x14u);return true;
    case 0x4f5380u:eax=net.u32(0x850b1cu);return true;
    case 0x455ad0u:eax=net.u8(0x7dd138u);return true;
    case 0x515c30u:eax=net.host_in_515c30(a(0),a(1));return true;
    case 0x453e70u:eax=net.player_by_id_453e70(ecx,a(0));return true;
    case 0x4f4740u:net.set_state_byte_4f4740(ecx,a(0));eax=0;return true;
    case 0x5153e0u:eax=net.counters_in_5153e0(a(0));return true;
    case 0x515410u:eax=net.setup_in_515410(a(0));return true;
    case 0x515440u:eax=net.players_in_515440(a(0));return true;
    case 0x5155a0u:eax=net.online_ids_in_5155a0(a(0));return true;
    case 0x515660u:eax=net.lobby_in_515660(a(0));return true;
    case 0x5156a0u:eax=net.starts_in_5156a0(a(0));return true;
    case 0x515780u:eax=net.ready_in_515780(a(0));return true;
    case 0x516c10u:eax=net.guest_in_516c10(a(0),a(1),a(2),a(3));return true;
    case 0x516cf0u:eax=net.stranger_where_516cf0(a(0),a(1),a(2));return true;
    case 0x516ac0u:eax=net.stranger_join_516ac0(a(0),a(1),a(2));return true;
    case 0x516550u:net.start_answer_516550();eax=0;return true;
    case 0x516440u:net.host_message_5162f0(6);eax=0;return true;
    case 0x48ffd0u:{std::uint32_t v=0;N::Frame L(net,0x10);net.read_491860(ecx,L(0),4);v=net.u32(L(0));eax=v;return true;}
    case 0x5165f0u:eax=net.host_frame_5165f0(a(0),a(1));return true;
    case 0x515800u:eax=net.guest_frame_515800();return true;
    case 0x4f4b90u:eax=net.may_send_4f4b90(ecx)?1u:0u;return true;
    case 0x4918a0u:net.tagged_4918a0(ecx,a(0),a(1));eax=0;return true;
    case 0x491c50u:net.tagged_491c50(ecx,a(0),a(1));eax=0;return true;
    case 0x490290u:eax=net.take_start_490290();return true;
    case 0x4904e0u:eax=net.in_race_4904e0();return true;
    case 0x4f5320u:net.start_reset_4f5320();eax=1;return true;
    case 0x4f5360u:net.start_arm_4f5360();eax=0;return true;
    case 0x4f53a0u:eax=net.u32(0x850b10u)==1u?1u:0u;return true;
    case 0x4f5150u:net.race_work_4f5150();eax=0;return true;
    case 0x4f5270u:net.race_begin_4f5270();eax=0;return true;
    case 0x4f50d0u:net.new_race_4f50d0();eax=0;return true;
    case 0x4edce0u:net.p32(ecx+0x14u,a(0));eax=a(0);return true;
    case 0x49b450u:net.p32(0x8369c0u,a(0));eax=a(0);return true;
    case 0x48b190u:net.p8(0x83036du,a(0));eax=a(0);return true;
    case 0x4f5f00u:net.finish_send_4f5f00(a(0),a(1),a(2));eax=0;return true;
    case 0x430770u:eax=net.reply_4307b0(a(0));return true;
    case 0x4307b0u:eax=net.reply_4307b0(ecx);return true;
    case 0x430b40u:net.search_430b40(ecx);eax=0;return true;
    case 0x430b00u:net.search_reset_430b00(ecx);eax=0;return true;
    case 0x430ee0u:net.arm_430ee0(ecx);eax=0;return true;
    case 0x491cc0u:net.p32(ecx+0x14u,0);net.event_clear_48f900(ecx);eax=0;return true;
    case 0x491cd0u:net.join_frame_491cd0(ecx);eax=0;return true;
    case 0x48ff00u:net.lobby_state_48ff00(a(0));eax=0;return true;
    case 0x454220u:net.p8(0x7d68d4u,a(0));eax=a(0);return true;
    case 0x454230u:eax=net.u8(0x7d68d4u);return true;
    case 0x431ba0u:net.copy_431ba0(a(0),a(1),a(2));eax=0;return true;
    case 0x435ff0u:net.query_reset_435ff0(ecx);eax=0;return true;
    case 0x4940d0u:net.service_reset_4940d0(ecx);eax=0;return true;
    case 0x515700u:eax=net.reset_in_515700(a(0));return true;
    case 0x51c000u:net.vector_51c000(ecx);eax=ecx;return true;
    case 0x523200u:{const std::uint32_t f=net.u32(0x85de90u);eax=f?net.call(f,{a(0)}):0u;return true;}
    case 0x459bb0u:eax=net.oldest_459bb0(ecx);return true;
    case 0x459d10u:eax=net.rank_459d10(ecx,a(0));return true;
    case 0x45a0b0u:eax=net.rank_459d10(0x7de418u,a(0));return true;
    case 0x453ca0u:{eax=0;for(std::uint32_t l=net.u32(ecx+0xcu);l;l=net.u32(l+0x10u))
        if((l!=0x830becu||net.u8(ecx+5u))&&net.u32(l+4u)==1u){eax=1;break;}
        return true;}
    case 0x49a650u:eax=0;return true;
    case 0x495930u:eax=net.course_count_495930(a(0));return true;
    case 0x4e81d0u:eax=net.grade_4e81d0(a(0),a(1));return true;
    case 0x4e8230u:eax=net.mission_grade_4e8230(a(0),a(1));return true;
    case 0x4e9fd0u:eax=net.stage_score_4e9fd0(a(0),a(1),a(2));return true;
    case 0x4e82a0u:eax=net.group_grade_4e82a0(a(0),a(1));return true;
    case 0x4474e0u:eax=net.bit_4474e0(ecx,a(0));return true;
    case 0x4c8fc0u:eax=net.unlocked_4c8fc0(a(0),a(1));return true;
    case 0x447730u:eax=net.unlocked_4c8fc0(std::uint32_t(std::int32_t(std::int8_t(net.u8(0x632e8cu+a(0))))),a(1)-1u);return true;
    case 0x48b130u:net.p8(0x655b59u,a(0));eax=a(0);return true;
    case 0x48b140u:eax=net.u8(0x655b59u);return true;
    case 0x48b150u:net.p8(0x655b5au,a(0));eax=a(0);return true;
    case 0x48b160u:eax=net.u8(0x655b5au);return true;
    case 0x48b180u:eax=net.u8(0x830374u);return true;
    case 0x48b1e0u:eax=net.u8(0x830364u);return true;
    case 0x4f1ce0u:net.p32(ecx,a(0));eax=a(0);return true;
    case 0x570ac0u:eax=ecx;return true;
    case 0x48e4d0u:net.placed_48e4d0(ecx);eax=ecx;return true;
    case 0x48e500u:net.p32(ecx,0x5c1888u);net.p32(ecx,0x5c18c8u);eax=0;return true;
    case 0x48e590u:net.text_48e590(ecx);eax=ecx;return true;
    case 0x48e630u:net.p32(ecx,0x5c18a0u);net.p32(ecx,0x5c1888u);net.p32(ecx,0x5c18c8u);eax=0;return true;
    case 0x48e640u:net.text_reset_48e640(ecx);eax=0;return true;
    case 0x48e530u:case 0x48e510u:net.p32(ecx+0x34u,a(0));net.p32(ecx+0x38u,a(1));eax=0;return true;
    case 0x48ef40u:net.p32(ecx+0x454u,a(0));eax=a(0);return true;
    case 0x48ee60u:net.p8(ecx+0x4cu,0);eax=0;return true;
    case 0x48ee80u:{std::vector<std::uint32_t> va;for(std::size_t k=2;k<n;++k)va.push_back(args[k]);
        net.text_print_48ee80(a(0),a(1),va);eax=0;return true;}
    case 0x48f2d0u:net.text_style_48f2d0(ecx,a(0));eax=0;return true;
    case 0x48f3c0u:net.text_draw_48f3c0(ecx);eax=0;return true;
    case 0x48f0b0u:net.draw_wrapped_48f0b0(ecx);eax=0;return true;
    case 0x48f040u:net.draw_lines_48f040(ecx);eax=0;return true;
    case 0x48ea50u:net.lines_48ea50(ecx,a(0));eax=0;return true;
    case 0x48eb60u:net.wrap_48eb60(ecx);eax=0;return true;
    case 0x48d570u:net.style_48d570(ecx);eax=ecx;return true;
    case 0x48d970u:{const std::uint32_t v[8]={a(0),a(1),a(2),a(3),a(4),a(5),a(6),a(7)};net.list_init_48d970(ecx,v);eax=ecx;return true;}
    case 0x48e310u:net.list_48e310(ecx);eax=ecx;return true;
    case 0x48da20u:net.item_free_48da20(ecx);eax=0;return true;
    case 0x48dad0u:net.item_48dad0(ecx);eax=ecx;return true;
    case 0x48da80u:net.list_append_48da80(ecx,a(0));eax=0;return true;
    case 0x48db30u:eax=net.list_add_48db30(ecx,a(0));return true;
    case 0x48dc60u:net.list_next_48dc60(ecx,a(0));eax=0;return true;
    case 0x48e440u:net.list_clear_48e440(ecx);eax=0;return true;
    case 0x48dda0u:net.list_draw_48dda0(ecx);eax=0;return true;
    case 0x48e250u:net.p32(ecx+0x20u,a(0));eax=0;return true;
    case 0x48e260u:net.p32(ecx+0x24u,a(0));net.p32(ecx+0x28u,a(1));eax=0;return true;
    case 0x48e280u:net.p32(ecx+0x45cu,a(0));net.p32(ecx+0x460u,a(1));eax=0;return true;
    case 0x48e2a0u:net.p32(ecx+0x2cu,a(0));eax=a(0);return true;
    case 0x48ca30u:net.p8(ecx+0x34u,0);net.p8(ecx+0x41u,0);net.p8(ecx+0x42u,0);eax=0;return true;
    case 0x48c490u:net.panel_48c490(ecx);eax=ecx;return true;
    case 0x440d50u:net.push_440d50(a(0));eax=0;return true;
    case 0x440d70u:eax=net.pop_440d70();return true;
    case 0x440df0u:net.p32(ecx+4u,a(0));eax=a(0);return true;
    case 0x40f050u:{float s;const std::uint32_t b=a(2);std::memcpy(&s,&b,4);net.vscale_40f050(a(0),a(1),s);eax=0;return true;}
    case 0x40ef10u:net.vadd_40ef10(a(0),a(1));eax=0;return true;
    case 0x48c5b0u:eax=net.panel_reset_48c5b0(ecx);return true;
    case 0x48c520u:net.panel_free_48c520(ecx);eax=0;return true;
    case 0x48ced0u:net.panel_free_48c520(ecx);if(a(0)&1u)net.call(0x5801a7u,{ecx});eax=ecx;return true;
    case 0x48cc00u:net.panel_move_48cc00(ecx);eax=0;return true;
    case 0x48ef90u:eax=net.text_height_48ef90(ecx);return true;
    case 0x48cef0u:net.panel_fit_48cef0(ecx,a(0),a(1));eax=0;return true;
    case 0x48f280u:{std::vector<std::uint32_t> va;for(std::size_t k=4;k<n;++k)va.push_back(args[k]);
        net.text_set_48f280(a(0),a(1),a(2),a(3),va);eax=0;return true;}
    case 0x48f1b0u:{std::uint32_t v[12];for(std::uint32_t k=0;k<12u;++k)v[k]=a(1+k);
        std::vector<std::uint32_t> va;for(std::size_t k=13;k<n;++k)va.push_back(args[k]);
        net.text_box_48f1b0(a(0),v,va);eax=0;return true;}
    case 0x48d0c0u:{std::uint32_t v[9];for(std::uint32_t k=0;k<9u;++k)v[k]=a(k);eax=net.panel_open_48d0c0(ecx,v);return true;}
    case 0x48d420u:eax=net.panel_update_48d420(ecx);return true;
    case 0x48c5f0u:net.panel_draw_48c5f0(ecx);eax=0;return true;
    case 0x48ca90u:eax=std::uint32_t(net.i16(ecx+0x36u)-0x25);return true;
    case 0x48e490u:net.list_free_48e490(ecx);eax=0;return true;
    case 0x47f110u:eax=0;return true;
    case 0x4edc80u:net.label_4edc80(ecx);eax=ecx;return true;
    case 0x4edcf0u:net.label_text_4edcf0(ecx,a(0));eax=0;return true;
    case 0x4edd50u:net.label_text_4edd50(ecx,a(0));eax=0;return true;
    case 0x4edaf0u:net.label_init_4edaf0(ecx,a(0),a(1),a(2),a(3),a(4));eax=0;return true;
    case 0x4edb90u:net.label_draw_4edb90(ecx);eax=0;return true;
    case 0x4edcc0u:net.p32(ecx+4u,a(0));net.p32(ecx+8u,a(1));eax=0;return true;
    case 0x4edda0u:net.p32(ecx+0x88u,a(0));eax=a(0);return true;
    case 0x445e60u:{float x,y,t;const std::uint32_t b0=a(0),b1=a(1),b2=a(2);std::memcpy(&x,&b0,4);std::memcpy(&y,&b1,4);std::memcpy(&t,&b2,4);
        net.mover_445e60(ecx,x,y,t);eax=0;return true;}
    case 0x51f560u:case 0x51f570u:net.p8(ecx+0x30u,0);net.p8(ecx+0x31u,0);eax=ecx;return true;
    case 0x51f580u:net.bar_51f580(ecx,a(0),a(1),a(2),a(3));eax=0;return true;
    case 0x51f5b0u:net.bar_draw_51f5b0(ecx);eax=0;return true;
    case 0x51f6a0u:net.p8(ecx+0x31u,a(0));eax=a(0);return true;
    case 0x4ed950u:net.menu_4ed950(ecx);eax=ecx;return true;
    case 0x4ecfb0u:net.menu_init_4ecfb0(ecx,a(0),a(1),a(2),a(3),a(4));eax=0;return true;
    case 0x4ed100u:net.menu_item_4ed100(ecx);eax=ecx;return true;
    case 0x4ed020u:net.menu_item_free_4ed020(ecx);eax=0;return true;
    case 0x4ed080u:net.menu_append_4ed080(ecx,a(0));eax=0;return true;
    case 0x4ed160u:eax=net.menu_add_4ed160(ecx,a(0),a(1));return true;
    case 0x4eda60u:net.menu_clear_4eda60(ecx);eax=0;return true;
    case 0x4edab0u:net.menu_free_4edab0(ecx);eax=0;return true;
    case 0x4ed250u:net.menu_prev_4ed250(ecx);eax=0;return true;
    case 0x4ed2a0u:net.menu_next_4ed2a0(ecx);eax=0;return true;
    case 0x4ed3e0u:net.menu_draw_4ed3e0(ecx);eax=0;return true;
    case 0x4ed8a0u:net.p32(ecx+0x20u,a(0));net.p32(ecx+0x24u,a(1));eax=0;return true;
    case 0x4ed880u:net.p32(ecx+0x1cu,a(0));eax=0;return true;
    case 0x4ed8c0u:net.p32(ecx+0x28u,a(0));eax=a(0);return true;
    case 0x48dc10u:net.list_prev_48dc10(ecx,a(0));eax=0;return true;
    case 0x4ee390u:eax=net.course_allowed_4ee390(a(0));return true;
    case 0x4ee720u:net.courses_4ee720();eax=0;return true;
    case 0x4ee7d0u:net.cars_4ee7d0(a(0));eax=0;return true;
    case 0x4ee930u:eax=net.setup_check_4ee930();return true;
    case 0x48b170u:net.p8(0x830374u,a(0));eax=a(0);return true;
    case 0x4ee5d0u:net.screen_4ee5d0(ecx);eax=ecx;return true;
    case 0x4ee6c0u:net.screen_close_4ee6c0(ecx);eax=0;return true;
    case 0x4ee850u:net.screen_free_4ee850(ecx);eax=0;return true;
    case 0x4eea50u:net.screen_free_4ee850(ecx);if(a(0)&1u)net.call(0x5801a7u,{ecx});eax=ecx;return true;
    case 0x4ee2e0u:net.labels_4ee2e0(ecx);eax=0;return true;
    case 0x4ede20u:eax=net.setting_text_4ede20(ecx,a(0));return true;
    case 0x4edf00u:net.settings_4edf00(ecx);eax=0;return true;
    case 0x4edfc0u:net.labels_refresh_4edfc0(ecx,false);eax=0;return true;
    case 0x4ee090u:net.screen_draw_4ee090(ecx);eax=0;return true;
    case 0x4ede00u:net.list_next_48dc60(ecx+0x574u,1);eax=0;return true;
    case 0x4ee380u:net.method(0x465250u,ecx+0x4c8u);eax=0;return true;
    case 0x4c6300u:net.hosts_4c6300(ecx);eax=ecx;return true;
    case 0x4c69c0u:net.hosts_free_4c69c0(ecx);eax=0;return true;
    case 0x4c6c90u:net.hosts_free_4c69c0(ecx);if(a(0)&1u)net.call(0x5801a7u,{ecx});eax=ecx;return true;
    case 0x4c6860u:eax=net.hosts_fill_4c6860(ecx);return true;
    case 0x4c6a90u:eax=net.hosts_open_4c6a90(ecx);return true;
    case 0x4c6390u:net.hosts_draw_4c6390(ecx);eax=0;return true;
    case 0x4c63f0u:net.hosts_close_4c63f0(ecx);eax=0;return true;
    case 0x4c6670u:net.hosts_leave_4c6670(ecx);eax=0;return true;
    case 0x4c6700u:net.hosts_settings_4c6700(ecx);eax=0;return true;
    case 0x4c67c0u:net.labels_refresh_4edfc0(ecx,true);eax=0;return true;
    case 0x4c6640u:if(net.u32(0x7d68acu))net.list_next_48dc60(ecx+0x1d80u,1);eax=0;return true;
    case 0x4c6660u:net.list_prev_48dc10(ecx+0x1d80u,1);eax=0;return true;
    case 0x4c6440u:net.hosts_ask_4c6440(ecx);eax=0;return true;
    case 0x4c6540u:eax=net.hosts_answer_4c6540(ecx,a(0));return true;
    case 0x4c6610u:net.hosts_back_4c6610(ecx);eax=0;return true;
    case 0x4c6b40u:eax=net.hosts_input_4c6b40(ecx,a(0));return true;
    case 0x4c6cb0u:eax=net.hosts_update_4c6cb0(ecx);return true;
    case 0x490f50u:net.setup_unpack_490f50(ecx);eax=0;return true;
    case 0x48f930u:net.setup_unpack_490f50(0x65a7a4u);eax=0;return true;
    case 0x447520u:eax=net.course_open_447520(ecx,a(0));return true;
    case 0x491090u:eax=net.setup_course_491090(ecx,a(0));return true;
    case 0x491280u:net.setup_groups_491280(ecx);eax=0;return true;
    case 0x491370u:net.setup_pack_491370(ecx);eax=0;return true;
    case 0x48f940u:net.setup_pack_491370(0x65a7a4u);eax=0;return true;
    case 0x490fd0u:net.setup_random_490fd0(ecx);eax=0;return true;
    case 0x48f960u:net.setup_random_490fd0(0x65a7a4u);eax=0;return true;
    case 0x48fdf0u:eax=net.name_banned_48fdf0(a(0));return true;
    case 0x48fea0u:eax=net.name_ok_48fea0(a(0));return true;
    case 0x492690u:eax=net.message_492690(a(0),a(1),a(2),a(3));return true;
    case 0x51bf10u:net.vector_51c000(ecx);eax=ecx;return true;
    case 0x51bf50u:net.number_51bf50(ecx,a(0),a(1),a(2),a(3),a(4),a(5));eax=0;return true;
    case 0x51bfd0u:net.number_draw_51bfd0(ecx);eax=0;return true;
    case 0x4465e0u:net.ready_marks_4465e0(a(0),a(1),a(2));eax=0;return true;
    case 0x535ef0u:eax=ecx+4u;return true;
    case 0x564c90u:eax=net.u32(ecx+8u);return true;
    case 0x48b1d0u:net.p8(0x830364u,a(0));eax=a(0);return true;
    case 0x48cbc0u:net.text_print_48ee80(ecx+0xdecu,a(0),{});eax=0;return true;
    case 0x4ede10u:net.list_prev_48dc10(ecx+0x574u,1);eax=0;return true;
    case 0x4c6fd0u:net.room_4c6fd0(ecx);eax=ecx;return true;
    case 0x4c7360u:net.room_rows_reset_4c7360(ecx);eax=0;return true;
    case 0x4c7ab0u:net.room_close_4c7ab0(ecx);eax=0;return true;
    case 0x4c7e30u:net.room_free_4c7e30(ecx);eax=0;return true;
    case 0x4c7f90u:net.room_free_4c7e30(ecx);if(a(0)&1u)net.call(0x5801a7u,{ecx});eax=ecx;return true;
    case 0x4c70f0u:net.room_fill_4c70f0(ecx);eax=0;return true;
    case 0x4c76a0u:eax=net.room_open_4c76a0(ecx);return true;
    case 0x4c7b20u:net.room_row_4c7b20(ecx,a(0),a(1));eax=0;return true;
    case 0x4c7f30u:net.room_rows_4c7f30(ecx);eax=0;return true;
    case 0x4c7fb0u:net.room_draw_4c7fb0(ecx);eax=0;return true;
    case 0x4c73a0u:net.room_next_4c73a0(ecx);eax=0;return true;
    case 0x4c73e0u:net.room_prev_4c73e0(ecx);eax=0;return true;
    case 0x4c7410u:net.room_leave_4c7410(ecx);eax=0;return true;
    case 0x4c74c0u:eax=net.room_can_start_4c74c0(ecx);return true;
    case 0x4c7570u:net.room_panel_4c7570(ecx);eax=0;return true;
    case 0x4c7630u:net.room_panel_close_4c7630(ecx);eax=0;return true;
    case 0x4c7670u:eax=net.room_panel_frame_4c7670(ecx);return true;
    case 0x4c7770u:eax=net.room_update_4c7770(ecx);return true;
    case 0x4c8030u:net.guest_room_4c8030(ecx);eax=ecx;return true;
    case 0x4c87f0u:net.guest_room_close_4c87f0(ecx);eax=0;return true;
    case 0x4c8b90u:net.guest_room_free_4c8b90(ecx);eax=0;return true;
    case 0x4c8ce0u:net.guest_room_free_4c8b90(ecx);if(a(0)&1u)net.call(0x5801a7u,{ecx});eax=ecx;return true;
    case 0x4c8140u:net.guest_room_fill_4c8140(ecx);eax=0;return true;
    case 0x4c8710u:eax=net.guest_room_open_4c8710(ecx);return true;
    case 0x4c8880u:net.room_row(ecx,a(0),a(1),0x68e360u);eax=0;return true;
    case 0x4c8c80u:net.guest_room_rows_4c8c80(ecx);eax=0;return true;
    case 0x4c8d00u:net.guest_room_draw_4c8d00(ecx);eax=0;return true;
    case 0x4c85f0u:net.guest_room_next_4c85f0(ecx);eax=0;return true;
    case 0x4c8630u:net.guest_room_prev_4c8630(ecx);eax=0;return true;
    case 0x4c8660u:net.guest_room_leave_4c8660(ecx);eax=0;return true;
    case 0x4c8360u:eax=net.guest_room_update_4c8360(ecx);return true;
    case 0x4dc080u:net.netwait_4dc080(ecx);eax=ecx;return true;
    case 0x4dc0e0u:net.netwait_free_4dc0e0(ecx);eax=0;return true;
    case 0x4dcbe0u:net.netwait_free_4dc0e0(ecx);if(a(0)&1u)net.call(0x5801a7u,{ecx});eax=ecx;return true;
    case 0x4dc140u:eax=net.netwait_open_4dc140(ecx);return true;
    case 0x4dcbc0u:net.netwait_close_4dcbc0(ecx);eax=0;return true;
    case 0x4dc1a0u:eax=net.netwait_update_4dc1a0(ecx);return true;
    case 0x4dcc00u:net.neterror_4dcc00(ecx);eax=ecx;return true;
    case 0x4dcc60u:net.neterror_free_4dcc60(ecx);eax=0;return true;
    case 0x4dd1c0u:net.neterror_free_4dcc60(ecx);if(a(0)&1u)net.call(0x5801a7u,{ecx});eax=ecx;return true;
    case 0x4dccc0u:eax=net.neterror_4dccc0(ecx);return true;
    case 0x4dcd60u:eax=net.neterror_update_4dcd60(ecx);return true;
    case 0x4dd190u:net.vslot(ecx+0x38u,2);net.vslot(ecx+0x38u,3);eax=0;return true;
    case 0x4dd1b0u:net.vslot(ecx+0x38u,4);eax=0;return true;
    case 0x525340u:net.core_init_525340(a(0));eax=0;return true;
    case 0x523e40u:net.log_header_523e40(ecx,a(0),a(1),a(2),a(3));eax=ecx;return true;
    case 0x523f40u:eax=net.logger_523f40();return true;
    case 0x5255a0u:{float l;const std::uint32_t b=a(1);std::memcpy(&l,&b,4);net.hashmap_5255a0(ecx,a(0),l);eax=0;return true;}
    case 0x525860u:net.channel_525860(ecx,a(0));eax=ecx;return true;
    case 0x523e80u:net.logger_root_523e80(ecx,a(0));eax=0;return true;
    case 0x5254a0u:eax=net.string_hash_5254a0(a(0));return true;
    case 0x523d00u:eax=net.string_equal_523d00(ecx,a(0))?1u:0u;return true;
    case 0x523d50u:eax=net.u32(net.u32(ecx)-8u);return true;
    case 0x48c360u:eax=net.u32(ecx);return true;
    case 0x5254e0u:eax=net.hashmap_find_5254e0(ecx,a(0));return true;
    case 0x525670u:net.channel_log_525670(ecx,a(0),a(1),a(2),a(3),a(4));eax=0;return true;
    case 0x523fb0u:{std::vector<std::uint32_t> va;for(std::size_t k=3;k<n;++k)va.push_back(args[k]);net.log_523fb0(a(0),a(1),a(2),va);eax=0;return true;}
    case 0x523170u:net.p32(0x85de8cu,a(0));eax=a(0);return true;
    case 0x523180u:net.p32(0x85de90u,a(0));eax=a(0);return true;
    case 0x523190u:net.p32(0x85de94u,a(0));eax=a(0);return true;
    case 0x5231a0u:net.p32(0x85de98u,a(0));eax=a(0);return true;
    case 0x5231b0u:net.p32(0x85de9cu,a(0));eax=a(0);return true;
    case 0x5231c0u:net.p32(0x85dea0u,a(0));eax=a(0);return true;
    case 0x441690u:{const std::uint32_t o=net.call(0x5802cfu,{0x2200u});if(o)net.hosts_4c6300(o);eax=o;return true;}
    case 0x4416f0u:{const std::uint32_t o=net.call(0x5802cfu,{0x31c8u});if(o)net.room_4c6fd0(o);eax=o;return true;}
    case 0x441750u:{const std::uint32_t o=net.call(0x5802cfu,{0x1f10u});if(o)net.guest_room_4c8030(o);eax=o;return true;}
    case 0x441d50u:{const std::uint32_t o=net.call(0x5802cfu,{0xd8u});if(o)net.netwait_4dc080(o);eax=o;return true;}
    case 0x456c30u:eax=net.racing_count_456c30(ecx);return true;
    case 0x456e30u:eax=net.racing_count_456c30(0x7de418u);return true;
    // the network message dialog, screen 46 (pc_network_message.inc)
    case 0x441c90u:eax=net.message_new_441c90();return true;
    case 0x4926f0u:eax=net.message_4926f0(ecx);return true;
    case 0x492750u:net.message_free_492750(ecx);eax=0;return true;
    case 0x492b70u:eax=net.message_delete_492b70(ecx,a(0));return true;
    case 0x4927b0u:eax=net.message_init_4927b0(ecx);return true;
    case 0x492b90u:eax=net.message_control_492b90(ecx);return true;
    case 0x492fb0u:net.message_display_492fb0(ecx);eax=0;return true;
    case 0x492b50u:eax=net.message_suspend_492b50(ecx);return true;
    case 0x492a00u:eax=net.message_close_ready_492a00(ecx);return true;
    case 0x492a40u:net.message_title_492a40();eax=0;return true;
    case 0x492a80u:eax=net.message_cancel_492a80(ecx);return true;
    case 0x48ce20u:eax=net.message_scroll_48ce20(ecx,a(0),a(1),a(2));return true;
    case 0x434c60u:net.query_answer_434c60(ecx,a(0));eax=0;return true;
    case 0x434e00u:net.query_leave_434e00();eax=0;return true;
    case 0x48e6d0u:eax=net.text_scroll_48e6d0(ecx,a(0),std::int32_t(a(1)),std::int32_t(a(2)),std::int32_t(a(3)),std::int32_t(a(4)),std::int32_t(a(5)));return true;
    case 0x491fe0u:net.box_answer_491fe0(ecx,a(0));eax=0;return true;
    case 0x494410u:return net.service_shutdown_494410(eax);
    case 0x459aa0u:net.message_459aa0(ecx,a(0),a(1));eax=0;return true;
    case 0x525f80u:net.release_525f80(ecx);eax=0;return true;
    case 0x431380u:net.listener_close_431380(ecx);eax=0;return true;
    case 0x432c60u:net.request_432c60(ecx);eax=0;return true;
    case 0x4355f0u:net.guest_close_4355f0(ecx);eax=0;return true;
    case 0x49e4d0u:eax=net.owner_page_49e4d0(a(0));return true;
    case 0x4bfa70u:eax=net.arcade_car_4bfa70(a(0),a(1));return true;
    case 0x434eb0u:net.settings_434eb0(ecx);eax=0;return true;
    case 0x431b10u:net.host_open_431b10(a(0));eax=0;return true;
    case 0x435ef0u:net.join_435ef0(ecx);eax=0;return true;
    case 0x435ae0u:net.guest_open_435ae0(a(0));eax=0;return true;
    case 0x48fc70u:eax=net.error_notice_48fc70(a(0));return true;
    case 0x453c10u:net.add_listener_453c10(ecx,a(0));eax=0;return true;
    case 0x453c40u:net.reset_listeners_453c40(ecx);eax=0;return true;
    case 0x453cd0u:eax=net.pending_453cd0(ecx);return true;
    case 0x453df0u:eax=net.new_player_453df0(ecx);return true;
    case 0x453ee0u:eax=net.find_player_453ee0(ecx,a(0));return true;
    case 0x453f80u:net.weights_453f80(ecx,a(0),a(1));eax=0;return true;
    case 0x453fe0u:net.new_id_453fe0(a(0));eax=0;return true;
    case 0x454060u:net.colour_454060(ecx);eax=0;return true;
    case 0x454100u:net.lan_menu_454100(std::vector<std::uint32_t>(args,args+n));eax=0;return true;
    case 0x454140u:net.lan_menu_454140(a(0),a(1));eax=0;return true;
    case 0x454180u:net.lan_menu_454180(a(0),a(1));eax=0;return true;
    case 0x4541f0u:eax=net.call_list(0x4f5270u,std::vector<std::uint32_t>(args,args+n),ecx);return true;
    default:return false;
    }
}

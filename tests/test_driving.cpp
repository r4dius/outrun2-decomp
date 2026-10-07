#include "driving/pc_driving.hpp"
#include "driving/pc_common_control.hpp"
#include <array>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>
#include <limits>
#include <stdexcept>
#include <string>
using namespace outrun::driving;
namespace {
unsigned assertions=0;
void require(bool b,const char* message){++assertions;if(!b)throw std::runtime_error(message);}
template<class F>void throws(F&& f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}require(caught,"expected checked exception");}
std::uint32_t bits(float f){std::uint32_t u;std::memcpy(&u,&f,4);return u;}
}
int main(){try{
    std::array<std::uint8_t,event_size> es{};std::array<std::uint8_t,work_size> ws{};
    std::array<std::uint8_t,parameter_size> ps{};std::array<std::uint8_t,648> ta{},tb{};std::array<std::uint8_t,1024> br{};
    Bytes e(es.data(),es.size()),w(ws.data(),ws.size()),p(ps.data(),ps.size());
    Tables t{{Bytes(ta.data(),ta.size()),Bytes(tb.data(),tb.size())},Bytes(br.data(),br.size())};
    t.torque[0].putf(0,0.1f);t.torque[0].putf(4,4);t.torque[1].putf(0,0.1f);t.torque[1].putf(4,5);
    for(unsigned i=0;i<160;++i){t.torque[0].putf(8+i*4,100+float(i));t.torque[1].putf(8+i*4,200+float(i));}
    for(unsigned i=0;i<256;++i)t.brake.putf(i*4,float(i)*0.25f);
    p.putf(0x1644,1000);p.putf(0x1690,1250);p.putf(0x15f8,100);p.put32(0x10a0,6);
    p.putf(0x1560,0.08f);p.putf(0x15ac,0.08f);p.putf(0x1514,8);p.putf(0x1728,1);p.putf(0x1774,500);
    e.putf(0x2a0,1);e.put32(0x208,0);
    // Bounds, endian preservation and signed integer bit patterns.
    e.put32(0,0x78563412);require(es[0]==0x12&&es[3]==0x78,"little-endian");
    e.put32(0,0x80000000);require(e.i32(0)==std::numeric_limits<std::int32_t>::min(),"signed bits");
    e.putf(0,-0.0f);require(bits(e.f32(0))==0x80000000,"negative zero");
    throws([&]{(void)e.u32(event_size-3);});throws([&]{e.put32(event_size,1);});
    throws([&]{(void)e.sub(event_size-1,2);});throws([&]{(void)Bytes(nullptr,1);});
    // r041 CheckReverseCar: bounded matrix arena and explicit parameter/RNG inputs.
    std::array<std::uint8_t,128> reverse_matrix{};PcMatrixStack reverse_ms{Bytes(reverse_matrix.data(),reverse_matrix.size()),0,0,2};
    e.put32(4,0u);e.put32(0x1f4,0u);e.putf(0x310,0.0f);e.putf(0x30c,0.0f);e.putf(0x1cc,0.0f);e.put16(0xd34,0u);
    PcReverseCarInputs reverse_in{-0.25f,0.25f,0.15f,0.35f,0x12345678u,0};
    check_reverse_car_4a2910(e,reverse_in,reverse_ms);
    require(reverse_ms.depth==0&&reverse_ms.current_offset==0,"reverse matrix stack restored");
    require(e.f32(0xf0)==1.0f&&e.f32(0x104)==1.0f&&e.f32(0x118)==1.0f&&e.f32(0x12c)==1.0f,"reverse neutral matrix identity");
    // r044 platform-tail dependency batch.
    require(platform_flag_bit2_44ff10(0x4u)&&!platform_flag_bit2_44ff10(0x2u),"platform flag bit2");
    std::array<std::uint8_t,256> platform_table_bytes{};Bytes platform_table(platform_table_bytes.data(),platform_table_bytes.size());
    for(unsigned k=0;k<64;++k)platform_table.put32(k*4u,0x10000000u+k*17u);
    require(platform_index_value_4505a0(platform_table,7)==0x10000000u+7u*17u,"platform indexed table");
    require(platform_pair_value_450630(platform_table,3,2)==0x10000000u+14u*17u,"platform pair table");
    require(platform_slot_kind_450750(14,true)&&!platform_slot_kind_450750(4,true),"platform slot active");
    require(platform_slot_kind_450750(4,false)&&!platform_slot_kind_450750(14,false),"platform slot inactive");
    require(platform_counter_lt_60_48b350(59)&&!platform_counter_lt_60_48b350(60)&&platform_counter_lt_60_48b350(-1),"platform signed counter");
    PcPlatformGhost4671State pg{};pg.writer_offset=7;pg.reader_offset=9;pg.stream_index=11;pg.packet_11b=0xaa;pg.packet_11c=0xbb;pg.service_pending=0x5a;pg.short_window=0x1234;
    e.put8(0x11,0x31);e.put8(0x12,0x42);
    PcPlatformGhost4671Inputs pgi{};pgi.game_mode=0x10;pgi.route_state=7;pgi.timer=61;
    platform_ghost_route_4671d0(e,pgi,pg);
    require(pg.writer_offset==0x120&&pg.reader_offset==0x120&&pg.stream_index==0,"platform ghost high timer offsets");
    require(pg.packet_11b==0x31&&pg.packet_11c==0x42&&!pg.serialize_called,"platform ghost high timer packet");
    pg={};pgi.timer=60;pgi.packet_flags=0;pgi.frame_counter=59;platform_ghost_route_4671d0(e,pgi,pg);
    require(pg.service_pending==1&&pg.serialize_called&&pg.short_window==1,"platform ghost low timer service");
    pg={};pg.short_window=77;pgi.packet_flags=1;platform_ghost_route_4671d0(e,pgi,pg);
    require(pg.service_pending==1&&!pg.serialize_called&&pg.short_window==77,"platform ghost packet gate");
    std::array<std::uint8_t,8u*0xfd4u> platform_records{};Bytes platform_records_view(platform_records.data(),platform_records.size());
    std::array<std::uint8_t,32u*4u> platform_index_bytes{};Bytes platform_index(platform_index_bytes.data(),platform_index_bytes.size());
    std::array<std::uint8_t,128u*4u> platform_pair_bytes{};Bytes platform_pair(platform_pair_bytes.data(),platform_pair_bytes.size());
    for(unsigned k=0;k<32;++k)platform_index.put32(k*4u,0x51000000u+k);
    for(unsigned k=0;k<128;++k)platform_pair.put32(k*4u,0x62000000u+k);
    e.put8(0x68,0x5a);PcPlatformGhost47f780Inputs pgr{};pgr.entry_slot=5;pgr.game_mode=0x10;pgr.route_state=0;pgr.protected_gate=true;pgr.flags=0;pgr.slot_external_gate=false;pgr.timer=60;pgr.slot_token=0x778899aa;
    PcPlatformGhost47f780State pgrs{};pgrs.init_state=1;pgrs.sequence=0x12345678;pgrs.frame_counter=9;pgrs.marker121=3;pgrs.marker123=4;pgrs.record_ready=5;
    platform_ghost_record_47f780(e,platform_records_view,platform_index,platform_pair,pgr,pgrs);
    constexpr std::size_t pgr_target=4u*0xfd4u;
    require(platform_records_view.u32(pgr_target)==0x544f4851u&&platform_records_view.u32(pgr_target+4)==0x51000004u,"platform record header");
    require(platform_records_view.u32(pgr_target+0x14)==0x62000010u&&platform_records_view.u32(pgr_target+0x20)==0x51000004u,"platform record tables");
    require(platform_records_view.u32(pgr_target+0x1c)==0x51000004u,"platform record slot-kind copy");
    require(pgrs.frame_counter==0&&pgrs.sequence==0&&pgrs.init_state==1&&pgrs.record_ready==1&&pgrs.record_service_called,"platform record state/writer");
    require(pgrs.reset114_bits==0xc7c34fffu&&pgrs.reset118_bits==0xc7c34fffu&&pgrs.reset11c_bits==0xc7c34fffu,"platform record reset floats");
    e.put8(0xd22,2);e.put8(0xda4,1);e.puti(0xd90,3);w.put16(0x288,0x1111);w.put16(0x37c,0x2222);w.put16(0x470,0x3333);w.put16(0x564,0x4444);
    common_pl_car_inline_tail_4a82c4(e,w);
    require(e.u8(0xd22)==1&&e.u8(0xda4)==0&&e.i32(0xd90)==2,"CommonPlCar inline counters");
    require(e.i16(0x40)==0x1111&&e.i16(0x42)==0x2222&&e.i16(0x44)==0x3333&&e.i16(0x46)==0x4444,"CommonPlCar inline wheel copies");
    // r045 course-service batch: direct closures used around GetRoadOfs.
    require(course_nested_marker_44bdb0(false,0x12345678u)==0u,"course nested marker absent");
    require(course_nested_marker_44bdb0(true,0x12345678u)==0x12345678u,"course nested marker present");
    require(course_stage_limit_44be00(0x89abcdefu)==0x89abcdefu,"course stage limit");
    require(course_active_slot_44be10(false,7u)==15u&&course_active_slot_44be10(true,7u)==7u,"course active slot");
    require(!course_primary_ready_44be30(17)&&course_primary_ready_44be30(18),"course primary gate");
    require(!course_secondary_ready_44be40(20)&&course_secondary_ready_44be40(19),"course secondary gate");
    require(course_type_gate_44be50(0,10,0)&&!course_type_gate_44be50(0,9,20),"course type primary gate");
    require(course_type_gate_44be50(1,0,20)&&!course_type_gate_44be50(1,99,19),"course type secondary gate");
    std::array<std::uint8_t,66u*4u> course_lookup_bytes{};Bytes course_lookup(course_lookup_bytes.data(),course_lookup_bytes.size());
    for(unsigned k=0;k<66;++k)course_lookup.put32(k*4u,0x71000000u+k*0x101u);
    require(course_index_lookup_44be80(course_lookup,3)==0x71000303u,"course indexed lookup");
    require(course_index_lookup_44be80(course_lookup,66)==0x71000000u,"course indexed fallback");
    throws([&]{(void)course_index_lookup_44be80(course_lookup,-1);});
    require(course_disp_matrix_choice_44bed0(false)==PcCourseMatrixChoice::Primary&&course_disp_matrix_choice_44bed0(true)==PcCourseMatrixChoice::Secondary,"course display selector");
    require(course_area_matrix_choice_44bef0(false)==PcCourseMatrixChoice::Primary&&course_area_matrix_choice_44bef0(true)==PcCourseMatrixChoice::Secondary,"course area selector");
    PcCourseServiceState css{1u,2u,3u,4u,5u};course_clear_service_state_44bf10(css);
    require(css.clear_bc==0u&&css.clear_c4==0u&&css.manager_ptr==0u&&css.ready==4u&&css.mode_byte==5u,"course clear globals");
    course_mark_ready_44c080(css);require(css.ready==1u,"course ready flag");
    course_set_mode_byte_44c0a0(css,0xa5u);require(course_get_mode_byte_44c090(css)==0xa5u,"course byte get/set");
    std::array<std::uint8_t,120> course_src_bytes{},course_dst_bytes{};Bytes course_src(course_src_bytes.data(),course_src_bytes.size()),course_dst(course_dst_bytes.data(),course_dst_bytes.size());
    for(unsigned k=0;k<30;++k)course_src.put32(k*4u,0x72000000u+k*0x10203u);
    course_copy_snapshot_44c0b0(course_dst,course_src);for(unsigned k=0;k<30;++k)require(course_dst.u32(k*4u)==course_src.u32(k*4u),"course snapshot copy");
    // r046 road-offset service batch.
    require(course_stage_unique_44dc50(true,0x1234u,0x5678u)==0x1234,"course stage unique descriptor");
    require(course_stage_unique_44dc50(false,0x1234u,0x5678u)==0x5678,"course stage unique fallback");
    require(road_stage_window_44ddc0(0x1c,0x60,0)==0x4fb&&road_stage_window_44ddc0(0x1c,0x23f,0)==-1,"road stage fixed window");
    require(road_stage_window_44ddc0(0x3a,0x200,0x300)==0x4fb,"road stage rolling window");
    require(road_stage_gate_44f0f0(0x1c,0x100,0,0xdeadbeefu)==0xdeadbeefu,"road stage protected gate");
    std::array<std::uint8_t,6> packed_sample{};Bytes packed_view(packed_sample.data(),packed_sample.size());
    packed_view.put16(0,static_cast<std::uint16_t>(100));packed_view.put16(2,static_cast<std::uint16_t>(-200));packed_view.put16(4,static_cast<std::uint16_t>(0x0010));
    std::array<std::uint8_t,0x14> decoded_sample{};Bytes decoded_view(decoded_sample.data(),decoded_sample.size());road_decode_sample_46ffc0(packed_view,decoded_view);
    require(std::fabs(decoded_view.f32(0)-24.4140625f)<1e-6f&&std::fabs(decoded_view.f32(8)+48.828125f)<1e-6f,"road packed xz decode");
    require(decoded_view.f32(0x0c)==1.384033203125f&&decoded_view.u32(0x10)==0u,"road packed surface decode");
    auto pc0=road_table_choice_4700d0(0,1,2);require(pc0.valid&&!pc0.secondary&&pc0.index==8&&pc0.byte_offset==8u*0x1030u,"road table primary choice");
    auto pc1=road_table_choice_4700d0(1,2,3);require(pc1.valid&&pc1.secondary&&pc1.index==27&&pc1.byte_offset==27u*0x70cu,"road table secondary choice");
    require(!road_table_choice_4700d0(0,0,6).valid,"road table primary invalid selector");
    std::array<std::array<std::uint8_t,64>,6> primary_storage{},secondary_storage{};
    std::array<Bytes,6> primary_blocks{Bytes(primary_storage[0].data(),64),Bytes(primary_storage[1].data(),64),Bytes(primary_storage[2].data(),64),Bytes(primary_storage[3].data(),64),Bytes(primary_storage[4].data(),64),Bytes(primary_storage[5].data(),64)};
    std::array<Bytes,6> secondary_blocks{Bytes(secondary_storage[0].data(),64),Bytes(secondary_storage[1].data(),64),Bytes(secondary_storage[2].data(),64),Bytes(secondary_storage[3].data(),64),Bytes(secondary_storage[4].data(),64),Bytes(secondary_storage[5].data(),64)};
    for(auto& b:primary_blocks){b.put16(0,0x4f53);b.put16(4,0);b.put16(6,0);b.put16(8,0);}
    PcRoadSampleTables road_tables{primary_blocks.data(),primary_blocks.size(),secondary_blocks.data(),secondary_blocks.size()};
    std::array<std::uint8_t,16> place_bytes{};Bytes place_view(place_bytes.data(),place_bytes.size());place_view.puti(0,0);place_view.put16(8,0);
    require(road_side_test_479a70({0,0,0},place_view,0,1,road_tables),"road side equal points");
    std::array<std::uint8_t,0x1100> road_event_bytes{};Bytes road_event(road_event_bytes.data(),road_event_bytes.size());
    road_event.put8(4,0);road_event.putf(0xb08,8.0f);road_event.put8(0x66,1);road_event.puti(0x5c,1);road_event.puti(0x60,100);
    road_lane_classify_47b890(road_event,PcRoadLaneInputs{});require(road_event.u8(0xc32)<=2,"road lane fixed branch/clamp");
    PcRoadCacheRefreshInputs cache_in{};cache_in.selector_result=3;cache_in.query_success=true;for(unsigned k=0;k<cache_in.query_output.size();++k)cache_in.query_output[k]=std::uint8_t(k^0x5a);
    road_event.puti(0x5c,1);road_event.puti(0x60,99);road_event.put16(0x64,7);road_event.puti(0x68,0x1234);
    require(refresh_cached_road_4a3f80(road_event,3,cache_in),"road cache refresh success");

    std::array<std::uint8_t,64> road_disp{};Bytes road_m(road_disp.data(),road_disp.size());for(unsigned k=0;k<16;++k)road_m.putf(k*4,k%5==0?1.0f:0.0f);
    std::array<std::uint8_t,0x1030*6> road_primary{};std::array<std::uint8_t,0x70c*12> road_secondary{};
    std::array<Bytes,6> road_pb{Bytes(road_primary.data()+0*0x1030,0x1030),Bytes(road_primary.data()+1*0x1030,0x1030),Bytes(road_primary.data()+2*0x1030,0x1030),Bytes(road_primary.data()+3*0x1030,0x1030),Bytes(road_primary.data()+4*0x1030,0x1030),Bytes(road_primary.data()+5*0x1030,0x1030)};
    std::array<Bytes,12> road_sb{Bytes(road_secondary.data()+0*0x70c,0x70c),Bytes(road_secondary.data()+1*0x70c,0x70c),Bytes(road_secondary.data()+2*0x70c,0x70c),Bytes(road_secondary.data()+3*0x70c,0x70c),Bytes(road_secondary.data()+4*0x70c,0x70c),Bytes(road_secondary.data()+5*0x70c,0x70c),Bytes(road_secondary.data()+6*0x70c,0x70c),Bytes(road_secondary.data()+7*0x70c,0x70c),Bytes(road_secondary.data()+8*0x70c,0x70c),Bytes(road_secondary.data()+9*0x70c,0x70c),Bytes(road_secondary.data()+10*0x70c,0x70c),Bytes(road_secondary.data()+11*0x70c,0x70c)};
    PcGetRoadOfsInputs go{};go.cache.query_success=true;go.cache.selector_result=-1;go.lane.stage_unique=0;go.use_lane_classifier=false;
    Bytes qo(go.cache.query_output.data(),go.cache.query_output.size());qo.putf(8,0);qo.putf(12,0);qo.putf(16,0); // center
    const CourseProbe corners[4]={{-5,0,10},{-5,0,-10},{5,0,10},{5,0,-10}};const unsigned oo[4]={0x24,0x30,0x3c,0x48};for(unsigned j=0;j<4;++j){qo.putf(oo[j],corners[j].x);qo.putf(oo[j]+4,corners[j].y);qo.putf(oo[j]+8,corners[j].z);}road_event.puti(0x1c0,3);road_event.putf(0x14,0);road_event.putf(0x18,0);road_event.putf(0x1c,0);road_event.puti(0x5c,0);road_event.puti(0x60,100);road_event.put16(0x64,1);
    get_road_ofs_4a4010(road_event,go,road_m,PcRoadSampleTables{road_pb.data(),road_pb.size(),road_sb.data(),road_sb.size()});require(road_event.i32(0x27c)==0,"get road ofs neutral");
    require(road_event.i32(0x10d0)==3&&road_event.u32(0x10c0)==road_event.u32(0x5c)&&road_event.u32(0x105c)==0u,"road cache published");
    // r046 CommonPlCar parent: independently-closed children are callback boundaries;
    // this smoke covers parent-owned gating, copy, wheel offsets and branch order.
    std::array<std::uint32_t,40> parent_trace{};std::size_t parent_trace_n=0;
    struct ParentTraceState{std::array<std::uint32_t,40>* a;std::size_t* n;} pts{&parent_trace,&parent_trace_n};
    auto parent_cb=[](void* u,std::uint32_t entry){auto* x=static_cast<ParentTraceState*>(u);(*x->a)[(*x->n)++]=entry;};
    for(unsigned k=0;k<4;++k)w.putf(embedded_wheel_offsets[k]+0x08u,1.0f+float(k));p.putf(0x558,0.25f);p.putf(0x5a4,0.5f);
    e.put32(4,0u);e.put8(0xd22,2);e.put8(0xda4,1);e.puti(0xd90,2);w.put16(0x288,0x1111);w.put16(0x37c,0x2222);w.put16(0x470,0x3333);w.put16(0x564,0x4444);
    std::array<Bytes,4> parent_wheels{w.sub(embedded_wheel_offsets[0],0xf4),w.sub(embedded_wheel_offsets[1],0xf4),w.sub(embedded_wheel_offsets[2],0xf4),w.sub(embedded_wheel_offsets[3],0xcc)};
    PcCommonPlCarParentInputs parent_in{0x10,61,2,false};common_pl_car_4a8100(e,w,p,parent_wheels,parent_in,{&pts,parent_cb});
    require((e.u32(4)&8u)!=0u&&w.u32(0)==0u,"CommonPlCar initial game-mode gate");
    require(parent_trace_n==32&&parent_trace[0]==0x49b2d0u&&parent_trace[29]==0x4962a0u&&parent_trace[30]==0x47f780u&&parent_trace[31]==0x479670u,"CommonPlCar service order");
    require(w.f32(embedded_wheel_offsets[0]+0x28u)==0.75f&&w.f32(embedded_wheel_offsets[2]+0x28u)==2.5f,"CommonPlCar wheel offsets");
    require(e.u8(0xd22)==1&&e.u8(0xda4)==0&&e.i32(0xd90)==1,"CommonPlCar complete inline tail");
    // r047 GamePlCar_Ctrl dependency batch.
    {
    std::array<std::uint8_t,event_size> r47_event_raw{};
    std::array<std::uint8_t,parameter_size> r47_param_raw{};
    Bytes e47(r47_event_raw.data(),r47_event_raw.size()),p47(r47_param_raw.data(),r47_param_raw.size());
    PcGameControlGlobals game_globals{};
    set_game_flag_43f9d0(game_globals,0xa5u);require(game_flag_43f9c0(game_globals)==0xa5u,"game flag get/set");
    set_game_state_byte_43f9e0(game_globals,0x5au);require(game_state_byte_43f9f0(game_globals)==0x5au,"game state byte get/set");
    set_game_state_dword_43fa10(game_globals,0x89abcdefu);require(game_state_dword_43fa00(game_globals)==0x89abcdefu,"game state dword get/set");
    PcGameBroadcastState broadcast{};broadcast.state_780240=7u;game_broadcast_43cc20(broadcast,0x13579bdfu);
    require(broadcast.state_780240==0u&&broadcast.value_78023c==0x13579bdfu,"game broadcast globals");
    for(auto v:broadcast.slots)require(v==0x13579bdfu,"game broadcast slots");
    e47.puti(0xd50,0);e47.putf(0x2f8,0.0f);operation_input_49fad0(e47,PcOperationInputInputs{0,0x12,0x34,0x56});
    require(e47.i32(0x34)==0x34&&e47.i32(0x38)==0x56&&std::uint16_t(e47.i16(0x202))==0x1200u,"OperationInput analogue path");
    operation_input_49fad0(e47,PcOperationInputInputs{0x0d,0xff,0xee,0xdd});
    require(e47.i32(0x34)==0&&e47.i32(0x38)==0&&std::uint16_t(e47.i16(0x202))==0u,"OperationInput disabled mode");
    e47.put8(0x13,1);e47.put8(0x282,0);e47.put8(0x283,0);e47.putf(0x2c8,0);e47.putf(0x2f8,0);e47.puti(0xe84,0);e47.put32(0x208,1);e47.putf(0x1c4,500);e47.putf(0xe14,100);e47.puti(0x3c,255);e47.put8(0xd36,0);e47.put8(0x296,0);e47.puti(0xe7c,179);p47.put32(0x10a0,6);
    check_shift_warning_4a50f0(e47,p47,PcShiftWarningInputs{200,0});require(e47.i32(0xe7c)==180&&e47.i32(0xe80)==1,"shift warning threshold");
    std::array<std::uint8_t,64u*0x24u> cs_hist_raw{};Bytes cs_hist(cs_hist_raw.data(),cs_hist_raw.size());e47.putf(0x14,4.0f);e47.putf(0x1c,7.0f);e47.putf(0x16c,1.0f);e47.putf(0x174,2.0f);e47.put16(0x2e,12);e47.put16(0x17e,5);e47.put16(0x260,0x1234);e47.put32(0x5c,2u);e47.put8(0x10,0x5a);e47.put32(4,1u<<18u);
    car_calc_total_cs_len_455f50(e47,cs_hist,65u);require(cs_hist.u32(0x24u)==65u&&cs_hist.u32(0x28u)==bits(4.0f),"total cs ring selection");
    require(cs_hist.f32(0x30u)==3.0f&&cs_hist.f32(0x34u)==5.0f,"total cs deltas");
    PcStageProgressHistory stage_hist{};e47.put32(0x5c,0u);e47.put16(0x64,23448u);e47.put16(0x260,23485u);e47.put16(0x262,0x4321u);e47.put16(0x25e,744u);e47.put8(4,0u);
    car_calc_current_stage_progress_4a2130(e47,PcStageProgressInputs{1408u,1656u,false,false},stage_hist);
    require(std::uint16_t(e47.i16(0x260))==23485u&&std::uint16_t(e47.i16(0x262))==0x4321u,"stage progress wrapped early return");
    require(get_now_heart_calc_mode_45c440(0xcafebabeu)==0xcafebabeu,"heart mode getter");
    for(unsigned k=0;k<21;++k)e47.put16(0x194u+k*2u,std::uint16_t(k*17u));e47.put16(0xc2c,5u);e47.put16(0x162,7u);e47.put32(4,0u);set_old_param_buffer_4a2ee0(e47);
    require(std::uint16_t(e47.i16(0x1bcu))==323u&&std::uint16_t(e47.i16(0x194))==12u,"old param history/heading sample");
    }
    // r048 timer/flag services and complete GamePlCar_Ctrl parent smoke.
    {
    std::uint32_t timeup=17u,flags48=0xa5a5a5a0u;
    require(race_counter_44fdf0(0x12345678u)==0x12345678u,"race counter getter");
    set_timeup_counter_44fe30(timeup,42u);require(get_timeup_counter_44fe40(timeup)==42u,"timeup get/set");
    set_race_flag0_44fe50(flags48,true);require(get_race_flag0_44fe70(flags48),"race flag0 set/get");
    set_race_flag0_44fe50(flags48,false);require(!get_race_flag0_44fe70(flags48),"race flag0 clear");
    set_race_flag2_44fef0(flags48,true);require((flags48&4u)!=0u,"race flag2 set");
    set_race_flag2_44fef0(flags48,false);require((flags48&4u)==0u,"race flag2 clear");
    require(bits(ham_nos_speed_45d0e0(-0.0f))==0x80000000u,"nos speed getter bits");

    std::array<std::uint8_t,0x1100> pe_raw{};Bytes pe(pe_raw.data(),pe_raw.size());
    pe.put32(8,0x12345678u);pe.put32(4,0x00010000u);pe.putf(0x1c4,12.5f);pe.putf(0x178,2.25f);
    pe.put32(0x208,0x11223344u);pe.putf(0x1d0,1.25f);pe.putf(0x1dc,4.0f);pe.putf(0x2dc,9.0f);
    pe.putf(0x20,4.0f);pe.putf(0x24,6.0f);pe.putf(0x28,8.0f);pe.putf(0x1e0,3.0f);
    pe.put8(0xd23,2u);pe.putf(0x14,1.0f);pe.putf(0x18,2.0f);pe.putf(0x1c,3.0f);
    pe.put16(0x2c,10);pe.put16(0x2e,20);pe.put16(0x30,30);pe.put32(0x5c,1u);pe.put16(0x64,50u);pe.put32(0x68,7u);
    pe.put32(0x2d8,1u);pe.put32(0x2dc,2u);pe.put32(0x2e0,3u);pe.put32(0x2e4,4u);pe.put32(0x2e8,5u);pe.put32(0x2ec,6u);pe.put32(0x2f0,3u);
    std::array<std::uint32_t,40> tr{};std::size_t tn=0;struct TS{std::array<std::uint32_t,40>* a;std::size_t* n;} ts{&tr,&tn};
    auto cb48=[](void* u,std::uint32_t pc){auto* q=static_cast<TS*>(u);(*q->a)[(*q->n)++]=pc;};
    PcGamePlCarParentInputs pin{};pin.game_state_byte=0;pin.game_flag=0;pin.entry_mode=1;pin.route_state=2;pin.rank=3;pin.heart_mode=0x12;pin.network_tail_active=false;pin.stage_denominator_base=9;pin.world_scale_divisor=2.0f;
    PcGamePlCarParentState pst{};game_pl_car_ctrl_4a8330(pe,pin,pst,{&ts,cb48});
    require(pe.i8(0xd23)==1,"GamePlCar d23 countdown");require(pe.u32(0x2f0)==2u,"GamePlCar parent bit clear");
    require(pe.i32(0x60)==101&&pe.u8(0xdb0)==3,"GamePlCar mode/rank publish");
    require(std::fabs(pe.f32(0x102c)-(59.0f/251.0f))<1e-6f,"GamePlCar progress ratio");
    require(std::fabs(pst.world_position[0]-2.0f)<1e-6f&&std::fabs(pst.world_position[2]-4.0f)<1e-6f,"GamePlCar world publish");
    require(tn>20&&tr[0]==0x43f9f0u&&tr[1]==0x43f9c0u&&tr[2]==0x44ff10u,"GamePlCar initial trace");
    bool saw_common=false,saw_nos=false;for(std::size_t k=0;k<tn;++k){saw_common|=tr[k]==0x4a8100u;saw_nos|=tr[k]==0x4a5650u;}require(saw_common&&saw_nos,"GamePlCar child branches");
    }
    // r050 CheckSlipStream: candidate scan, trigger/cooldown and network filtering.
    {
    std::array<std::uint8_t,event_size> e50raw{};Bytes e50(e50raw.data(),e50raw.size());
    e50.putf(0x14,0.0f);e50.putf(0x18,0.0f);e50.putf(0x1c,0.0f);
    e50.putf(0x20,0.0f);e50.putf(0x24,0.0f);e50.putf(0x28,-1.0f);
    e50.puti(0x34,255);e50.puti(0x38,0);e50.put16(0x162,0);e50.put16(0xd46,0);
    e50.put8(0xe70,0);e50.putf(0xe6c,0.0f);e50.put32(0xe78,0);
    PcCheckSlipStreamInputs si50{};si50.candidates[0].open_state=2;si50.candidates[0].position={0.0f,0.0f,-20.0f};si50.candidates[0].direction={0.0f,0.0f,-1.0f};si50.candidates[0].speed=2.0f;
    check_slipstream_4a4d20(e50,si50);
    require(e50.i32(0xe74)==9,"slipstream best candidate id");
    require(e50.f32(0xe68)>0.18f,"slipstream best score");
    require((e50.u32(0x0c)&0x00040000u)!=0u&&e50.u32(0xe78)==1u,"slipstream trigger flag/count");
    require(si50.candidates[0].cooldown==179,"slipstream same-frame cooldown decrement");
    require(e50.u8(0xe70)==1u&&e50.f32(0xe6c)>0.0f,"slipstream ramp attack");
    const float prior=e50.f32(0xe6c);const std::uint8_t prior_count=e50.u8(0xe70);
    PcCheckSlipStreamInputs blocked{};blocked.network_session_active=true;blocked.candidates[0]=si50.candidates[0];blocked.candidates[0].network_state=1;blocked.candidates[0].open_state=2;
    check_slipstream_4a4d20(e50,blocked);
    require(e50.i32(0xe74)==-1&&e50.f32(0xe68)==0.0f,"slipstream network gate");
    require(e50.u8(0xe70)==std::uint8_t(prior_count-1u)&&e50.f32(0xe6c)<prior,"slipstream ramp decay");
    }
    // r051 PasPlCar_Ctrl modular parent: petty-auto gate and four road records.
    {
    std::array<std::uint8_t,event_size> e51raw{};Bytes e51(e51raw.data(),e51raw.size());
    e51.put32(4,0u);e51.put32(0x5c,0x402u);
    std::array<std::array<std::uint8_t,0x100>,4> rr{};std::array<Bytes,4> rb={Bytes(rr[0].data(),0x100),Bytes(rr[1].data(),0x100),Bytes(rr[2].data(),0x100),Bytes(rr[3].data(),0x100)};
    PcPasPlCarInputs pi51{};pi51.petty_auto_scene_list=0u;
    for(unsigned k=0;k<4;++k){
        const auto off=0x130u+k*12u;e51.putf(off,1.0f+float(k));e51.putf(off+4,2.0f+float(k));e51.putf(off+8,3.0f+float(k));
        e51.putf(0x134u+k*12u,10.0f+float(k));
        auto& q=pi51.wheel[k];q.transformed_point={4.0f+float(k),5.0f+float(k),6.0f+float(k)};q.road_y=7.0f+float(k);q.road_polygon=0x100u+k;q.road_result=(k==1u)?1u:2u;
        q.tire_position={0.0f,0.5f+float(k),0.0f};q.road_normal={0.1f*float(k),1.0f,-0.1f*float(k)};
    }
    std::array<std::uint32_t,32> t51{};std::size_t n51=0;struct T51{std::array<std::uint32_t,32>* a;std::size_t* n;} st51{&t51,&n51};
    auto cb51=[](void* u,std::uint32_t pc,std::uint32_t){auto* q=static_cast<T51*>(u);(*q->a)[(*q->n)++]=pc;};
    pas_pl_car_ctrl_475720(e51,rb,pi51,{&st51,cb51});
    require((e51.u32(4)&0x00800000u)!=0u,"PasPlCar parent flag");
    require(e51.u32(0x24c)==2u&&e51.u32(0x250)==0u&&e51.u32(0x254)==2u&&e51.u32(0x258)==2u,"PasPlCar road results");
    require(rr[0][0xee]==0u&&std::fabs(rb[0].f32(0xe4)-1.0f)<1e-6f,"PasPlCar road record state");
    require(std::fabs(e51.f32(0x134)-(10.0f+0.5f+7.0f-5.0f))<1e-6f,"PasPlCar tire height correction");
    require(n51>10u&&t51[0]==0x4755c0u&&t51[1]==0x4872f0u&&t51[2]==0x4a8330u,"PasPlCar normal path trace");
    pi51.petty_auto_scene_list=3u;n51=0;pas_pl_car_ctrl_475720(e51,rb,pi51,{&st51,cb51});
    require(n51>8u&&t51[2]==0x4a2650u&&t51[3]==0x409f30u,"PasPlCar petty path trace");
    }
    // r052 PC event scheduler and player-controller registration block.
    {
    PcEventControlState evs{};
    struct EvTrace { std::array<std::uint32_t,32> cb{}; std::array<std::uint32_t,32> work{}; std::array<std::uint32_t,32> id{}; std::size_t n{}; } et{};
    auto invoke=[](void* u,std::uint32_t cb,std::uint32_t work,std::uint32_t id){
        auto* t=static_cast<EvTrace*>(u); if(t->n<t->cb.size()){t->cb[t->n]=cb;t->work[t->n]=work;t->id[t->n]=id;} ++t->n;
    };
    for(std::uint32_t k=0;k<4u;++k){evs.slots[k].event_id=100u+k;evs.slots[k].work_token=0x1000u+k;}
    evs.slots[0].flags=0x04u;evs.slots[0].dest_callback=0xd001u;
    evs.slots[1].flags=0x01u;evs.slots[1].init_callback=0xa001u;evs.slots[1].ctrl_callback=0xc001u;
    evs.slots[2].flags=0x02u;evs.slots[2].ctrl_callback=0xc002u;
    evs.slots[3].flags=0x0au;evs.slots[3].ctrl_callback=0xc003u;
    event_control_43fab0(evs,PcEventServices{&et,invoke,nullptr});
    require(et.n==4u&&et.cb[0]==0xd001u&&et.cb[1]==0xa001u&&et.cb[2]==0xc001u&&et.cb[3]==0xc002u,"EventControl callback order");
    require(evs.slots[0].flags==0u&&evs.slots[1].flags==2u&&evs.slots[3].flags==0x0au,"EventControl flag transitions");
    require(evs.current_slot==PcEventSlotCount-1u,"EventControl current slot tail");

    PcEventControlState open_state{};open_state.slots[7].event_id=7u;open_state.slots[7].work_token=0x7777u;
    struct OpenCtx { EvTrace* trace; std::uint32_t seen_id{},seen_fn{}; } oc{&et};
    auto setup=[](void* u,PcEventControlState& st,std::uint32_t id,std::uint32_t fn){
        auto* c=static_cast<OpenCtx*>(u);c->seen_id=id;c->seen_fn=fn;auto& q=st.slots[id];q.event_id=id;q.work_token=0x7000u+id;q.init_callback=0xa700u+fn;q.flags=1u;
    };
    auto invoke_open=[](void* u,std::uint32_t cb,std::uint32_t work,std::uint32_t id){
        auto* c=static_cast<OpenCtx*>(u);auto* t=c->trace;if(t->n<t->cb.size()){t->cb[t->n]=cb;t->work[t->n]=work;t->id[t->n]=id;}++t->n;
    };
    et.n=0;event_open_440180(open_state,7u,3u,PcEventServices{&oc,invoke_open,setup});
    require(oc.seen_id==7u&&oc.seen_fn==3u&&et.n==1u&&et.cb[0]==0xa703u,"EventOpen setup/init");
    require(open_state.slots[7].flags==2u,"EventOpen active transition");

    PcEventControlState cls{};cls.slots[4].flags=1u;event_close_4401d0(cls,4u);require(cls.slots[4].flags==0u,"EventClose pending init");
    cls.slots[4].flags=2u;event_close_4401d0(cls,4u);require(cls.slots[4].flags==4u,"EventClose deferred dest");
    cls.slots[5].event_id=5u;cls.slots[5].work_token=0x55u;cls.slots[5].dest_callback=0xd005u;cls.slots[5].flags=2u;et.n=0;
    event_close_immediate_440200(cls,5u,PcEventServices{&et,invoke,nullptr});require(cls.slots[5].flags==0u&&et.n==1u&&et.cb[0]==0xd005u,"EventCloseImmediate dest");
    cls.slots[6].flags=2u;cls.slots[6].close_guard=0u;cls.slots[7].flags=2u;cls.slots[7].close_guard=1u;event_close_all_440240(cls);
    require(cls.slots[6].flags==4u&&cls.slots[7].flags==2u,"EventCloseAll close guard");
    for(unsigned k=10;k<14;++k)cls.slots[k].flags=2u;event_close_serial_440330(cls,11u,2u);
    require(cls.slots[10].flags==2u&&cls.slots[11].flags==4u&&cls.slots[12].flags==4u&&cls.slots[13].flags==2u,"EventCloseSerial range");
    require(check_event_destructing_440370(cls,11u)&&!check_event_destructing_440370(cls,10u),"CheckEventDestructing");

    std::array<std::uint32_t,PcEventSlotCount> defs{};defs[0]=0x1111u;defs[123]=0x7777u;
    require(get_event_id_440b30(defs,0x7777u)==123u&&get_event_id_440b30(defs,0xdeadu)==PcEventSlotCount,"GetEventId lookup/sentinel");
    cls.current_slot=12u;cls.slots[12].event_id=0x345u;require(get_now_event_id_440b80(cls)==0x345u,"GetNowEventId");
    change_now_event_ctrl_func_440b90(cls,0xabcdu);change_now_event_shadow_func_440ba0(cls,0xdef0u);
    require(cls.slots[12].ctrl_callback==0xabcdu&&cls.slots[12].shadow_callback==0xdef0u,"ChangeNowEvent callbacks");
    change_ctrl_func_440bb0(cls,8u,0x12345678u);change_disp_scene_440bd0(cls,8u,0x87654321u);
    require(cls.slots[8].ctrl_callback==0x12345678u&&cls.slots[8].display_scene==0x87654321u,"ChangeCtrl/DispScene");

    std::array<std::uint8_t,0x20> select_raw{};Bytes select_event(select_raw.data(),select_raw.size());select_event.put32(4,0xffffffffu);
    select_pl_car_ctrl_486942(select_event,cls,0);require(cls.slots[8].ctrl_callback==0x004a8330u&&(select_event.u32(4)&0x00800000u)==0u,"player ctrl Game selection");
    select_pl_car_ctrl_486942(select_event,cls,1);require(cls.slots[8].ctrl_callback==0x00475720u&&(select_event.u32(4)&0x00800000u)!=0u,"player ctrl Pas selection");
    const auto keep_ctrl=cls.slots[8].ctrl_callback,keep_flags=select_event.u32(4);select_pl_car_ctrl_486942(select_event,cls,2);
    require(cls.slots[8].ctrl_callback==keep_ctrl&&select_event.u32(4)==keep_flags,"player ctrl unknown no-op");
    }
    // r053 event bootstrap + suspend/resume leaves.
    {
    std::array<PcEventInitDescriptor,PcEventSlotCount> d53{};
    std::array<PcEventFunctionDescriptor,PcEventFunctionTableCount> f53{};
    for(std::uint32_t k=0;k<PcEventSlotCount;++k){
        d53[k].descriptor_token=0x51000000u+k;d53[k].work_token=0x52000000u+k;
        d53[k].aux28=k^0x55u;d53[k].display_scene=(k%32u);
    }
    d53[0].startup=1u;d53[0].function_id=3u;d53[17].startup=7u;d53[17].function_id=5u;
    f53[3]={0x11110003u,0x22220003u,0x33330003u,0x44440003u,0x55550003u};
    f53[5]={0x11110005u,0x22220005u,0x33330005u,0x44440005u,0x55550005u};
    PcEventControlState i53{};for(auto& q:i53.slots){q.flags=0xffu;q.ctrl_callback=0xffffffffu;q.aux34=0xdeadbeefu;}
    i53.current_slot=123u;init_event_control_440bf0(i53,d53,f53);
    require(i53.current_slot==123u,"InitEventControl leaves current pointer state untouched");
    require(i53.slots[0].flags==1u&&i53.slots[0].event_id==0u&&i53.slots[0].descriptor_token==0x51000000u,"InitEventControl slot0 base fields");
    require(i53.slots[0].init_callback==0x11110003u&&i53.slots[0].dest_callback==0x55550003u,"InitEventControl callback table");
    require(i53.slots[17].flags==1u&&i53.slots[17].close_guard==7u&&i53.slots[17].function_id==5u,"InitEventControl startup descriptor");
    require(i53.slots[18].flags==0u&&i53.slots[18].ctrl_callback==0u&&i53.slots[18].aux34==0u,"InitEventControl clears inactive slot");
    require(i53.slots[409].event_id==409u&&i53.slots[409].work_token==0x52000199u,"InitEventControl final slot");

    i53.slots[10].flags=0x02u;i53.slots[11].flags=0x11u;i53.slots[12].flags=0x04u;i53.slots[13].flags=0x80u;
    event_suspend_440a10(i53,10u,3u);
    require(i53.slots[10].flags==0x12u&&i53.slots[11].flags==0x11u&&i53.slots[12].flags==0x14u&&i53.slots[13].flags==0x80u,"EventSuspend range/bit");
    require(check_event_suspend_440a50(i53,10u)==0x10u&&check_event_suspend_440a50(i53,13u)==0u,"CheckEventSuspend PC mask return");
    event_resume_440a30(i53,11u,2u);
    require(i53.slots[10].flags==0x12u&&i53.slots[11].flags==0x01u&&i53.slots[12].flags==0x04u,"EventResume range/bit");
    }
    // r054 event pause ownership + current-event work allocation/free.
    {
    struct Trace54 {std::uint32_t n{};std::array<std::uint32_t,32> pc{},arg{};std::uint32_t next_base{0x61000000u};};
    auto log54=[](void* u,std::uint32_t pc,std::uint32_t arg){auto* t=static_cast<Trace54*>(u);if(t->n<t->pc.size()){t->pc[t->n]=pc;t->arg[t->n]=arg;}++t->n;};
    auto alloc54=[](void* u,std::uint32_t bytes)->std::uint32_t{auto* t=static_cast<Trace54*>(u);if(t->n<t->pc.size()){t->pc[t->n]=0x580253u;t->arg[t->n]=bytes;}++t->n;return t->next_base;};
    auto free54=[](void* u,std::uint32_t base){auto* t=static_cast<Trace54*>(u);if(t->n<t->pc.size()){t->pc[t->n]=0x580bc2u;t->arg[t->n]=base;}++t->n;};

    PcEventControlState p54{};p54.slots[0].aux28=1u;p54.slots[0].flags=0x02u;p54.slots[1].work_token=5u;p54.slots[1].aux28=0u;p54.slots[1].flags=0x0bu;p54.slots[409].aux28=9u;p54.slots[409].flags=0x80u;   // 440930 tests record +28, not the work
    Trace54 pt{};set_ev_pause_flag_440930(p54,PcEventPauseServices{&pt,log54});
    require(p54.pause_depth==1u&&p54.slots[0].flags==0x0au&&p54.slots[1].flags==0x0bu&&p54.slots[409].flags==0x88u,"SetEvPauseFlag +28-owned slots");
    require(pt.n==3u&&pt.pc[0]==0x43f9d0u&&pt.pc[1]==0x429810u&&pt.pc[2]==0x449040u&&pt.arg[0]==1u&&pt.arg[1]==1u&&pt.arg[2]==1u,"SetEvPauseFlag service order");
    set_ev_pause_flag_440930(p54,PcEventPauseServices{&pt,log54});require(p54.pause_depth==2u,"SetEvPauseFlag nesting depth");
    const auto before_nested_clear=pt.n;clr_ev_pause_flag_4409c0(p54,PcEventPauseServices{&pt,log54});
    require(p54.pause_depth==1u&&pt.n==before_nested_clear&&p54.slots[0].flags==0x0au,"ClrEvPauseFlag nested no-op");
    clr_ev_pause_flag_4409c0(p54,PcEventPauseServices{&pt,log54});
    require(p54.pause_depth==0u&&(p54.slots[0].flags&8u)==0u&&(p54.slots[1].flags&8u)==0u&&(p54.slots[409].flags&8u)==0u,"ClrEvPauseFlag final clear");
    require(pt.n==9u&&pt.pc[6]==0x429810u&&pt.pc[7]==0x43f9d0u&&pt.pc[8]==0x449040u&&pt.arg[6]==0u&&pt.arg[7]==0u&&pt.arg[8]==0u,"ClrEvPauseFlag service order");

    PcEventControlState w54{};w54.current_slot=5u;w54.slots[5].descriptor_token=0x510055aau;w54.slots[5].work_token=0xdead0001u;
    PcEventWorkHandle h54{};Trace54 wt{};PcEventWorkServices ws54{&wt,log54,alloc54,free54};
    const auto base54=malloc_now_event_work_440a60(w54,h54,0x80u,3u,ws54);
    require(base54==0x61000000u&&h54.live&&h54.base_token==base54&&h54.wrapper_token==0x61000080u&&h54.heap_type==3u,"MallocNowEventWork trailer metadata");
    require(w54.slots[5].work_token==base54&&w54.slots[5].aux24==0x61000080u,"MallocNowEventWork event fields");
    const std::array<std::uint32_t,9> malloc_pc{{0x440d90u,0x440d10u,0x440d50u,0x580253u,0x440d30u,0x440d70u,0x49a650u,0x49a650u,0x440d90u}};
    bool malloc_trace_ok=wt.n==malloc_pc.size();for(std::size_t k=0;k<malloc_pc.size()&&malloc_trace_ok;++k)malloc_trace_ok=wt.pc[k]==malloc_pc[k];
    require(malloc_trace_ok&&wt.arg[0]==0x510055aau&&wt.arg[1]==1u&&wt.arg[2]==1u&&wt.arg[3]==0x88u&&wt.arg[6]==0x61000080u&&wt.arg[8]==0u,"MallocNowEventWork child call order");
    free_now_event_work_440b20(w54,h54,ws54);
    require(!h54.live&&w54.slots[5].aux24==0u&&w54.slots[5].work_token==base54,"FreeNowEventWork ownership/stale work token");
    require(wt.n==12u&&wt.pc[9]==0x440d10u&&wt.arg[9]==1u&&wt.pc[10]==0x580bc2u&&wt.arg[10]==base54&&wt.pc[11]==0x440d30u,"Free event-work child call order");
    const auto n54=wt.n;free_now_event_work_440b20(w54,h54,ws54);require(wt.n==n54,"FreeNowEventWork null handle no-op");
    }
    // r055 allocator-state stacks + handle move + compact masked-service wrapper.
    {
    PcAllocatorStateStacks st55{};
    st55.stack_a[4]=0xa4a4a4a4u;st55.stack_b[4]=0xb4b4b4b4u;
    push_alloc_state_a_440d10(st55,0x11u);push_alloc_state_a_440d10(st55,0x22u);
    require(st55.depth_a==2u&&st55.stack_a[0]==0x11u&&st55.stack_a[1]==0x22u,"alloc stack A push");
    st55.stack_a[2]=0x12345678u;const auto pa55=pop_alloc_state_a_440d30(st55);
    require(pa55==0x12345678u&&st55.depth_a==1u,"alloc stack A pop PC read-before-decrement");
    st55.depth_a=4u;const auto before_a=st55.stack_a;push_alloc_state_a_440d10(st55,0xffffffffu);
    require(st55.depth_a==4u&&st55.stack_a==before_a,"alloc stack A depth cap");
    push_alloc_state_b_440d50(st55,7u);push_alloc_state_b_440d50(st55,9u);st55.stack_b[2]=0x87654321u;
    const auto pb55=pop_alloc_state_b_440d70(st55);require(pb55==0x87654321u&&st55.depth_b==1u,"alloc stack B push/pop");
    st55.depth_b=4u;const auto before_b=st55.stack_b;push_alloc_state_b_440d50(st55,0x55u);require(st55.depth_b==4u&&st55.stack_b==before_b,"alloc stack B depth cap");

    std::uint32_t dst55=0x11111111u;hmm_handle_move_440cc0(dst55,0x22223333u);require(dst55==0x22223333u,"hmmHandleMove slot transfer");
    struct Mask55{std::uint32_t n{},mask{},zero{},id{};} m55{};
    auto cb55=[](void* u,std::uint32_t mask,std::uint32_t zero,std::uint32_t id){auto* t=static_cast<Mask55*>(u);++t->n;t->mask=mask;t->zero=zero;t->id=id;};
    masked_service_440ca0(3u,&m55,cb55);require(m55.n==1u&&m55.mask==8u&&m55.zero==0u&&m55.id==0x199u,"masked service standard bit");
    masked_service_440ca0(35u,&m55,cb55);require(m55.n==2u&&m55.mask==8u,"masked service x86 CL mask");
    }
    // r056 compact object/state helpers at 0x440DC0..0x440ED0.
    {
    std::array<std::uint8_t,0x600> raw56{};for(std::size_t k=0;k<raw56.size();++k)raw56[k]=std::uint8_t(k*37u+11u);
    Bytes o56(raw56.data(),raw56.size());o56.put32(0x21c,0x12345678u);
    require(object_take_token_440dc0(o56)==0x12345678u&&o56.u32(0x21c)==0xffffffffu,"r056 take/reset token");
    object_set_token_440de0(o56,0xaabbccddu);object_set_field4_440df0(o56,0x10203040u);object_set_field8_440e00(o56,0x55667788u);
    require(o56.u32(0x21c)==0xaabbccddu&&o56.u32(4)==0x10203040u&&o56.u32(8)==0x55667788u,"r056 field setters");
    struct Trace56{std::uint32_t n{},pc{},off{},a{},b{};std::uint8_t ret{0x5au};} t56{};
    auto void56=[](void* u,std::uint32_t pc,std::uint32_t off){auto* t=static_cast<Trace56*>(u);++t->n;t->pc=pc;t->off=off;};
    auto bool56=[](void* u,std::uint32_t pc,std::uint32_t off,std::uint32_t a,std::uint32_t b)->std::uint8_t{auto* t=static_cast<Trace56*>(u);++t->n;t->pc=pc;t->off=off;t->a=a;t->b=b;return t->ret;};
    PcObjectChildServices sv56{&t56,void56,bool56};
    o56.put8(0x220,0);o56.put8(0x221,0x44);o56.put8(0x241,0x88);object_push_byte_state_440e10(o56,sv56);
    require(o56.u8(0x220)==1&&o56.u8(0x222)==0x44&&o56.u8(0x242)==0x88&&t56.pc==0x446fc0u&&t56.off==0x51cu,"r056 byte-state push");
    object_pop_byte_state_440e60(o56,sv56);require(o56.u8(0x220)==0&&o56.u8(0x222)==0&&o56.u8(0x242)==0&&t56.pc==0x4464f0u,"r056 byte-state pop");
    const auto n0=t56.n;o56.put8(0x220,0x20);object_push_byte_state_440e10(o56,sv56);require(t56.n==n0&&o56.u8(0x220)==0x20,"r056 push signed cap");
    o56.put8(0x220,0);object_pop_byte_state_440e60(o56,sv56);require(t56.n==n0&&o56.u8(0x220)==0,"r056 pop zero no-op");
    object_child_call_a_440ea0(o56,sv56);require(t56.pc==0x446d90u&&t56.off==0x51cu,"r056 child wrapper A");
    object_child_call_b_440eb0(o56,sv56);require(t56.pc==0x4464b0u,"r056 child wrapper B");
    object_child_call_c_440ec0(o56,sv56);require(t56.pc==0x4464d0u,"r056 child wrapper C");
    o56.put32(0x218,1);const auto n1=t56.n;require(object_child_conditional_440ed0(o56,3,4,sv56)==0&&t56.n==n1,"r056 conditional false");
    o56.put32(0x218,2);require(object_child_conditional_440ed0(o56,0x11112222u,0x33334444u,sv56)==0x5au&&t56.pc==0x446f30u&&t56.a==0x11112222u&&t56.b==0x33334444u,"r056 conditional forward");
    }
    // r057 float transition controller + global leaves.
    {
    std::array<std::uint8_t,0x600> raw57{};Bytes o57(raw57.data(),raw57.size());PcFloatTransitionGlobals g57{};
    g57.token=0x12345678u;g57.primary=9.0f;g57.secondary=8.0f;g57.active=1u;
    require(transition_global_get_active_4c50c0(g57)==1u,"r057 global active getter");
    transition_global_set_token_4c50e0(g57,0xaabbccddu);transition_global_set_primary_4c50f0(g57,2.5f);transition_global_set_secondary_4c5100(g57,0.5f);transition_global_clear_active_4c5110(g57);
    require(g57.token==0xaabbccddu&&g57.primary==2.5f&&g57.secondary==0.5f&&g57.active==0u,"r057 global leaves");
    object_transition_init_440ef0(o57,g57);
    require(o57.f32(0x264)==1.0f&&o57.f32(0x268)==1.0f&&o57.f32(0x26c)==1.0f&&o57.f32(0x270)==1.0f&&o57.f32(0x274)==1.0f&&o57.f32(0x278)==1.0f,"r057 init floats");
    require(o57.u32(0x27c)==0u&&o57.u32(0x280)==0u&&o57.u8(0x261)==0u&&o57.u8(0x262)==0u&&g57.primary==1.0f&&g57.secondary==1.0f&&g57.active==0u,"r057 init state");
    object_transition_config_440f70(o57,0u,4u,0xdeadbeefu,g57);
    require(o57.f32(0x26c)==3.0f&&o57.f32(0x278)==0.75f&&o57.u32(0x280)==4u&&o57.u8(0x261)==0u&&g57.primary==3.0f&&g57.secondary==0.75f,"r057 config mode0");
    struct Tr57{unsigned activate{};} tr57{};auto a57=[](void* u){++static_cast<Tr57*>(u)->activate;};PcFloatTransitionServices sv57{&tr57,a57};
    g57.active=0;object_transition_update_441020(o57,g57,sv57);
    require(o57.u32(0x27c)==1u&&o57.f32(0x264)==1.5f&&o57.f32(0x270)==0.9375f&&g57.primary==1.5f&&g57.secondary==0.9375f&&tr57.activate==1u,"r057 update interpolation/activate");
    g57.active=1;object_transition_update_441020(o57,g57,sv57);require(o57.u32(0x27c)==2u&&tr57.activate==1u,"r057 active gate");
    o57.put8(0x262,0);o57.put8(0x261,0);object_transition_latch_441130(o57);
    require(o57.u32(0x268)==o57.u32(0x264)&&o57.u32(0x274)==o57.u32(0x270)&&o57.f32(0x26c)==1.0f&&o57.f32(0x278)==1.0f&&o57.u32(0x280)==2u&&o57.u32(0x27c)==0u&&o57.u8(0x262)==1u,"r057 latch state");
    const auto snap57=raw57;o57.put8(0x262,1);o57.put8(0x261,0);object_transition_latch_441130(o57);require(raw57==snap57,"r057 latch skip");
    object_transition_config_440f70(o57,1u,7u,0u,g57);require(g57.primary==1.0f&&g57.secondary==1.0f&&o57.u8(0x261)==1u,"r057 config mode1");
    }
    // r058 compact runtime/object controller and directly consumed leaves.
    {
    std::array<std::uint8_t,0x40> mgr58raw{};Bytes mgr58(mgr58raw.data(),mgr58raw.size());
    std::array<std::uint8_t,0x1000> obj58raw{};Bytes obj58(obj58raw.data(),obj58raw.size());
    PcRuntimeControlGlobals g58{};g58.enabled_d2=1;g58.current_ac=0x11110000u;g58.reset_word_659930=0xdeadbeefu;
    mgr58.put8(5,7);mgr58.put8(7,8);mgr58.put8(8,9);
    struct Tr58{std::uint32_t n{},pc{},a{},b{};std::uint8_t u8{};std::uint32_t u32{};std::uint8_t pair{};std::int8_t i8{};} t58{};
    auto v58=[](void* u,std::uint32_t pc,std::uint32_t h){auto* t=static_cast<Tr58*>(u);++t->n;t->pc=pc;t->a=h;};
    auto va58=[](void* u,std::uint32_t pc,std::uint32_t h,std::uint32_t a){auto* t=static_cast<Tr58*>(u);++t->n;t->pc=pc;t->a=h;t->b=a;};
    auto u858=[](void* u,std::uint32_t pc,std::uint32_t h)->std::uint8_t{auto* t=static_cast<Tr58*>(u);++t->n;t->pc=pc;t->a=h;return t->u8;};
    auto u3258=[](void* u,std::uint32_t pc,std::uint32_t h)->std::uint32_t{auto* t=static_cast<Tr58*>(u);++t->n;t->pc=pc;t->a=h;return t->u32;};
    auto p58=[](void* u,std::uint32_t pc,std::uint32_t a,std::uint32_t b)->std::uint8_t{auto* t=static_cast<Tr58*>(u);++t->n;t->pc=pc;t->a=a;t->b=b;return t->pair;};
    auto i858=[](void* u,std::uint32_t pc,std::uint32_t h)->std::int8_t{auto* t=static_cast<Tr58*>(u);++t->n;t->pc=pc;t->a=h;return t->i8;};
    PcRuntimeControlServices sv58{&t58,v58,va58,u858,u3258,p58,i858};
    t58.u8=0;runtime_shutdown_4411a0(g58,mgr58,sv58);
    require(g58.enabled_d2==0u&&g58.started_d1==1u&&g58.reset_word_659930==0u&&mgr58.u8(5)==0u&&mgr58.u8(7)==0u&&mgr58.u8(8)==0u,"r058 shutdown/reset");
    runtime_set_gate_454220(g58,0x7au);require(runtime_get_gate_454230(g58)==0x7au,"r058 gate leaf pair");
    std::array<std::uint8_t,0x420> c58raw{};Bytes c58(c58raw.data(),c58raw.size());c58.put32(8,0x0eu);c58.put32(0x408,2u);
    require(runtime_child_state_564c90(c58)==0x0eu,"r058 child-state leaf");runtime_child_counter_dec_4464f0(c58);require(c58.u32(0x408)==1u,"r058 child-counter leaf");
    obj58.put32(0x488,0x22220000u);obj58.put8(0x48c,1u);t58.u32=0x0eu;require(runtime_ready_441260(obj58,g58,sv58)==0u,"r058 ready child-state gate");
    t58.u32=7u;g58.current_ac=0x33330000u;g58.alternate_b0=0x44440000u;t58.pair=1u;require(runtime_ready_441260(obj58,g58,sv58)==1u,"r058 ready relation gate");
    t58.pair=0u;g58.mode_836130=0u;g58.gate_d4=0u;require(runtime_ready_441260(obj58,g58,sv58)==0u,"r058 ready false");
    t58.u32=0x2au;require(runtime_status_4412c0(obj58,sv58)==0x2au&&runtime_has_handle_4412f0(obj58)==1u,"r058 status/flag leaves");
    obj58.put8(0x220,2u);obj58.put8(0x224,0x55u);obj58.put8(0x243,0x66u);obj58.put32(0x924,3u);t58.i8=3;
    runtime_release_handle_441200(obj58,0x22220000u,sv58);require(obj58.u8(0x224)==0u&&obj58.u8(0x243)==0u&&obj58.u8(0x220)==1u&&obj58.u32(0x924)==2u,"r058 release-handle body");
    obj58.put32(0x488,0x22220000u);obj58.put8(0x48c,1u);obj58.put8(0x220,1u);obj58.put8(0x224,0x77u);obj58.put8(0x242,0x88u);obj58.put32(0x924,2u);t58.u32=0x2au;t58.i8=3;
    require(runtime_close_if_status_441300(obj58,0x2au,sv58)==1u&&obj58.u32(0x488)==0u&&obj58.u8(0x48c)==0u,"r058 close matching status");
    obj58.put32(0x488,0x22220000u);obj58.put8(0x48c,1u);t58.u32=0x2au;require(runtime_close_if_status_441300(obj58,0x11u,sv58)==0u&&obj58.u32(0x488)!=0u,"r058 close mismatch");
    }
    // r059 compact UI/resource controller and directly consumed leaves.
    {
    std::array<std::uint8_t,0x1000> o59raw{};Bytes o59(o59raw.data(),o59raw.size());
    PcUiNotifyGlobals g59{};g59.source_x=0x12345u;g59.source_y=0xabcdeu;
    struct Tr59{unsigned free_n{},status_n{},style_n{},lookup_n{},draw_n{};std::uint32_t h{},style{},id{},fmt{},arg{};std::int32_t status{};} t59{};
    auto hv59=[](void* u,std::uint32_t pc,std::uint32_t h){auto* t=static_cast<Tr59*>(u);if(pc==0x4285a0u){++t->free_n;t->h=h;}};
    auto hi59=[](void* u,std::uint32_t pc,std::uint32_t h)->std::int32_t{auto* t=static_cast<Tr59*>(u);if(pc==0x428880u){++t->status_n;t->h=h;}return t->status;};
    auto vv59=[](void* u,std::uint32_t pc,std::uint32_t v){auto* t=static_cast<Tr59*>(u);if(pc==0x42ca60u){++t->style_n;t->style=v;}};
    auto lk59=[](void* u,std::uint32_t pc,std::uint32_t id)->std::uint32_t{auto* t=static_cast<Tr59*>(u);if(pc==0x465eb0u){++t->lookup_n;t->id=id;}return 0x71000000u+id;};
    auto dr59=[](void* u,std::uint32_t pc,std::uint32_t fmt,std::uint32_t arg){auto* t=static_cast<Tr59*>(u);if(pc==0x42cdd0u){++t->draw_n;t->fmt=fmt;t->arg=arg;}};
    PcUiNotifyServices sv59{&t59,hv59,hi59,vv59,lk59,dr59};
    auto r59=o59.sub(0xcf4,o59.size()-0xcf4);r59.put32(8,0x11223344u);r59.put32(0x24,0u);r59.put32(0x28,7u);r59.put32(0x2c,8u);
    ui_resource_reset_465250(r59,sv59);require(r59.u32(8)==0xffffffffu&&r59.u32(0x28)==0u&&r59.u32(0x2c)==0u&&t59.free_n==1&&t59.h==0x11223344u,"r059 resource reset/free");
    r59.put32(8,0x55667788u);r59.puti(0x24,0);t59.status=1;require(ui_resource_ready_4652e0(r59,sv59)==0u&&t59.status_n==1,"r059 resource busy status");
    t59.status=2;require(ui_resource_ready_4652e0(r59,sv59)==1u,"r059 resource ready status");r59.puti(0x24,1);require(ui_resource_ready_4652e0(r59,sv59)==0u,"r059 resource positive hold");
    ui_text_set_xy_42cc00(g59,0x12345u,0xfedcbu);ui_text_set_color_42cca0(g59,0xaabbccddu);ui_text_set_mode_42ccb0(g59,7u);
    require(std::uint16_t(g59.x)==0x2345u&&std::uint16_t(g59.y)==0xedcbu&&g59.color==0xaabbccddu&&g59.mode==7u,"r059 ui leaf globals");
    std::array<std::uint8_t,0x40> tbl59{};Bytes tb59(tbl59.data(),tbl59.size());tb59.put32(5u*4u,0xdeadbeefu);require(ui_table_lookup_465eb0(Bytes(tbl59.data(),tbl59.size()),5u)==0xdeadbeefu,"r059 indexed lookup");
    o59.put32(0xc50,0u);auto c59=o59.sub(0xc48,o59.size()-0xc48);c59.put32(8,0x01020304u);c59.puti(0x24,0);t59.status=2;t59.style_n=t59.lookup_n=t59.draw_n=0;g59.source_x=0x11112222u;g59.source_y=0x33334444u;g59.alternate=0u;
    ui_notify_441370(o59,g59,sv59);require(t59.style_n==1&&t59.style==9u&&t59.lookup_n==1&&t59.id==0x474u&&t59.draw_n==1&&t59.fmt==0x71000474u&&t59.arg==0x474u,"r059 notify normal trace");
    require(g59.mode==7u&&g59.color==0xffffffffu&&std::uint16_t(g59.x)==0x2222u&&std::uint16_t(g59.y)==0x4444u,"r059 notify globals");
    g59.alternate=1u;ui_notify_441370(o59,g59,sv59);require(t59.id==0x476u&&t59.arg==0x00830c20u,"r059 notify alternate");
    r59.put32(8,0x10203040u);r59.put32(0x24,1u);o59.put8(0xd94,1u);ui_set_active_4413f0(o59,0u,sv59);require(o59.u8(0xd94)==0u&&r59.u32(8)==0xffffffffu,"r059 active reset child");
    o59.puti(0x80,4);o59.put32(0,10);o59.put32(4,20);o59.put32(8,30);o59.put32(12,40);compact_u32_list_441410(o59,1);require(o59.i32(0x80)==3&&o59.u32(0)==10&&o59.u32(4)==30&&o59.u32(8)==40,"r059 compact list");
    }
    // r060 first continuous SEH factory-wrapper block: normal-path semantic ownership.
    {
    struct Tr60{unsigned alloc_n{},ctor_n{};std::uint32_t alloc_pc{},bytes{},ctor_pc{},object{},alloc_ret{},ctor_ret{};} t60{};
    auto a60=[](void* u,std::uint32_t pc,std::uint32_t bytes)->std::uint32_t{auto* t=static_cast<Tr60*>(u);++t->alloc_n;t->alloc_pc=pc;t->bytes=bytes;return t->alloc_ret;};
    auto c60=[](void* u,std::uint32_t pc,std::uint32_t object)->std::uint32_t{auto* t=static_cast<Tr60*>(u);++t->ctor_n;t->ctor_pc=pc;t->object=object;return t->ctor_ret;};
    PcFactoryServices sv60{&t60,a60,c60};
    struct F60{std::uint32_t(*fn)(const PcFactoryServices&);std::uint32_t bytes,ctor;};
    const F60 f60[]={
        {object_factory_441450,0x758u,0x4c5120u},{object_factory_4414b0,0x154u,0x4c5550u},
        {object_factory_441510,0x154u,0x4c5980u},{object_factory_441570,0x14cu,0x4c5cd0u},
        {object_factory_4415d0,0x14cu,0x4c5ef0u},{object_factory_441630,0x1f4u,0x4c60b0u},
        {object_factory_441690,0x2200u,0x4c6300u},{object_factory_4416f0,0x31c8u,0x4c6fd0u},
        {object_factory_441750,0x1f10u,0x4c8030u},{object_factory_4417b0,0x1674u,0x4c8de0u},
        {object_factory_441810,0x474u,0x4c9a90u},{object_factory_441870,0x27f9cu,0x4cb300u},
        {object_factory_4418d0,0x27f9cu,0x4ccfc0u}
    };
    for(unsigned k=0;k<std::size(f60);++k){
        t60={};t60.alloc_ret=0x36000000u+k*0x1000u;t60.ctor_ret=0x76000000u+k;
        require(f60[k].fn(sv60)==t60.ctor_ret&&t60.alloc_n==1u&&t60.ctor_n==1u&&t60.alloc_pc==0x5802cfu&&t60.bytes==f60[k].bytes&&t60.ctor_pc==f60[k].ctor&&t60.object==t60.alloc_ret,"r060 factory success path");
        t60={};t60.alloc_ret=0u;t60.ctor_ret=0xdeadbeefu;
        require(f60[k].fn(sv60)==0u&&t60.alloc_n==1u&&t60.ctor_n==0u&&t60.bytes==f60[k].bytes,"r060 factory null allocation path");
    }
    }
    // r061 continuation of the continuous SEH factory-wrapper series.
    {
    struct Tr61{unsigned alloc_n{},ctor_n{};std::uint32_t alloc_pc{},bytes{},ctor_pc{},object{},alloc_ret{},ctor_ret{};} t61{};
    auto a61=[](void* u,std::uint32_t pc,std::uint32_t bytes)->std::uint32_t{auto* t=static_cast<Tr61*>(u);++t->alloc_n;t->alloc_pc=pc;t->bytes=bytes;return t->alloc_ret;};
    auto c61=[](void* u,std::uint32_t pc,std::uint32_t object)->std::uint32_t{auto* t=static_cast<Tr61*>(u);++t->ctor_n;t->ctor_pc=pc;t->object=object;return t->ctor_ret;};
    PcFactoryServices sv61{&t61,a61,c61};
    struct F61{std::uint32_t(*fn)(const PcFactoryServices&);std::uint32_t bytes,ctor;};
    const F61 f61[]={
        {object_factory_441930,0x279c4u,0x4ce170u},{object_factory_441990,0x27a68u,0x4cebf0u},{object_factory_4419f0,0x27ba8u,0x4d05e0u},
        {object_factory_441a50,0x27ce8u,0x4d2710u},{object_factory_441ab0,0x28fcu,0x4d4230u},{object_factory_441b10,0x1d90u,0x4d7140u},
        {object_factory_441b70,0x5ecu,0x4d9010u},{object_factory_441bd0,0x27b08u,0x4da2f0u},{object_factory_441c30,0x275d8u,0x4db680u},
        {object_factory_441c90,0x1324u,0x4926f0u},{object_factory_441cf0,0x12b0u,0x48c490u},{object_factory_441d50,0xd8u,0x4dc080u},
        {object_factory_441db0,0x12e8u,0x4dcc00u},{object_factory_441e10,0x1f4u,0x4dd410u},{object_factory_441e70,0x11a4u,0x4dd5c0u},
        {object_factory_441ed0,0x12ecu,0x4de6f0u},{object_factory_441f30,0x17cu,0x4de9d0u},{object_factory_441f90,0x17b8u,0x4dec60u},
        {object_factory_441ff0,0x271cu,0x4df820u},{object_factory_442050,0x1b68u,0x4dfaf0u},{object_factory_4420b0,0x1444u,0x4e0680u},
        {object_factory_442110,0xc768u,0x4e1890u},{object_factory_442170,0x448cu,0x4e3ac0u},{object_factory_4421d0,0x449cu,0x4e4c60u},
        {object_factory_442230,0x20b0u,0x4e5590u},{object_factory_442290,0x1768u,0x4e5720u},{object_factory_4422f0,0x1decu,0x4e5b20u},
        {object_factory_442350,0x1b48u,0x4e60c0u},{object_factory_4423b0,0x26f8u,0x4e6220u},{object_factory_442410,0x22d4u,0x493020u},
        {object_factory_442470,0x130e8u,0x4e7600u},{object_factory_4424d0,0x88a8u,0x4e7a90u},{object_factory_442530,0x132cu,0x4e7d60u},
        {object_factory_442590,0x674u,0x4e8130u},{object_factory_4425f0,0x4488u,0x4e9160u},{object_factory_442650,0x2408u,0x4e9e50u},
        {object_factory_4426b0,0x1c4cu,0x4eb100u},{object_factory_442710,0x1c10u,0x4302c0u},{object_factory_442770,0x1c10u,0x4afe30u},
        {object_factory_4427d0,0x1c14u,0x4eb2e0u},{object_factory_442830,0x1c10u,0x4eb5b0u},{object_factory_442890,0x1c10u,0x4eb8d0u},
        {object_factory_4428f0,0x1c10u,0x4eb9e0u},{object_factory_442950,0x1c10u,0x4ebd10u}
    };
    for(unsigned k=0;k<std::size(f61);++k){
        t61={};t61.alloc_ret=0x38000000u+k*0x1000u;t61.ctor_ret=0x77000000u+k;
        require(f61[k].fn(sv61)==t61.ctor_ret&&t61.alloc_n==1u&&t61.ctor_n==1u&&t61.alloc_pc==0x5802cfu&&t61.bytes==f61[k].bytes&&t61.ctor_pc==f61[k].ctor&&t61.object==t61.alloc_ret,"r061 factory success path");
        t61={};t61.alloc_ret=0u;t61.ctor_ret=0xdeadbeefu;
        require(f61[k].fn(sv61)==0u&&t61.alloc_n==1u&&t61.ctor_n==0u&&t61.bytes==f61[k].bytes,"r061 factory null allocation path");
    }
    }
    // r062 first post-factory constructor/state-initializer block.  Child bodies
    // remain services; these checks cover parent-owned writes, arguments and ordering.
    {
    struct R62Rec{std::uint32_t pc{},off{},a{},b{},c{},d{},seq{};};
    struct R62Trace{std::array<R62Rec,12> rec{};unsigned n{};} tr{};
    auto this_cb=[](void* u,std::uint32_t pc,Bytes o,std::size_t off){
        auto* t=static_cast<R62Trace*>(u);auto& r=t->rec[t->n++];r={pc,std::uint32_t(off),0,0,0,0,t->n};
        o.put32(off,0x62000000u|t->n);
    };
    auto arg_cb=[](void* u,std::uint32_t pc,Bytes o,std::size_t off,std::uint32_t a){
        auto* t=static_cast<R62Trace*>(u);auto& r=t->rec[t->n++];r={pc,std::uint32_t(off),a,0,0,0,t->n};
        o.put32(off,0x62100000u|t->n);
    };
    auto array_cb=[](void* u,std::uint32_t pc,Bytes o,std::size_t off,std::uint32_t elem,std::uint32_t count,std::uint32_t ctor,std::uint32_t dtor){
        auto* t=static_cast<R62Trace*>(u);auto& r=t->rec[t->n++];r={pc,std::uint32_t(off),elem,count,ctor,dtor,t->n};
        o.put32(off,0x62200000u|t->n);
    };
    PcObjectInitServices sv62{&tr,this_cb,arg_cb,array_cb};
    std::array<std::uint8_t,0x3000> raw62{};raw62.fill(0xa5u);Bytes o62(raw62.data(),raw62.size());
    require(object_ctor_4429b0(o62,0x44290001u,sv62)==0x44290001u,"r062 4429b0 return self");
    require(tr.n==6u,"r062 4429b0 child count");
    require(tr.rec[0].pc==0x48f480u&&tr.rec[0].off==0u&&tr.rec[0].seq==1u,"r062 4429b0 base order");
    require(tr.rec[1].pc==0x4ed950u&&tr.rec[1].off==0x1a8u&&tr.rec[1].seq==2u,"r062 4429b0 child1 order");
    require(tr.rec[2].pc==0x48c490u&&tr.rec[2].off==0x1e4u&&tr.rec[2].seq==3u,"r062 4429b0 child2 order");
    require(tr.rec[3].pc==0x5816bdu&&tr.rec[3].off==0x1494u&&tr.rec[3].a==0x8cu&&tr.rec[3].b==4u&&tr.rec[3].c==0x570ac0u&&tr.rec[3].d==0x49a650u,"r062 4429b0 array0 args");
    require(tr.rec[4].pc==0x490f10u&&tr.rec[4].off==0x16c4u&&tr.rec[4].a==0u,"r062 4429b0 arg child");
    require(tr.rec[5].pc==0x5816bdu&&tr.rec[5].off==0x174cu&&tr.rec[5].a==0x48cu&&tr.rec[5].b==4u&&tr.rec[5].c==0x48e590u&&tr.rec[5].d==0x48e630u,"r062 4429b0 array1 args");
    require(o62.u32(0)==0x0059dabcu,"r062 4429b0 vtable after base");
    require(o62.u32(0x1a8u)==0x62000002u&&o62.u32(0x1e4u)==0x62000003u&&o62.u32(0x1494u)==0x62200004u&&o62.u32(0x16c4u)==0x62100005u&&o62.u32(0x174cu)==0x62200006u,"r062 4429b0 service effects retained");

    tr={};raw62.fill(0x5au);
    require(object_ctor_442a60(o62,0x442a6001u,sv62)==0x442a6001u,"r062 442a60 return self");
    require(tr.n==3u&&tr.rec[0].pc==0x48f480u&&tr.rec[1].pc==0x4ed950u&&tr.rec[1].off==0x34u&&tr.rec[2].pc==0x48c490u&&tr.rec[2].off==0x70u,"r062 442a60 child order");
    require(o62.u32(0)==0x0059dad4u,"r062 442a60 vtable after base");

    tr={};raw62.fill(0x3cu);
    require(object_block_init_442ac0(o62,0x442ac001u,sv62)==0x442ac001u,"r062 442ac0 return self");
    require(o62.u8(0)==0u&&o62.u32(0x8u)==0xffffffffu&&o62.u32(0x24u)==0xffffffffu&&o62.u32(0x3e8u)==0xffffffffu&&o62.u32(0x404u)==0xffffffffu,"r062 442ac0 sentinel grid");
    require(o62.u32(0x408u)==0u,"r062 442ac0 array count state");
    require(tr.n==1u&&tr.rec[0].pc==0x5816bdu&&tr.rec[0].off==0x40cu&&tr.rec[0].a==0xa0u&&tr.rec[0].b==4u&&tr.rec[0].c==0x465160u&&tr.rec[0].d==0x465250u,"r062 442ac0 vector args");

    for(unsigned flag=0;flag<2u;++flag){
        tr={};raw62.fill(0xc3u);
        require(object_state_ctor_442b20(o62,0x442b2001u,std::uint8_t(flag),sv62)==0x442b2001u,"r062 442b20 return self");
        require(tr.n==4u&&tr.rec[0].pc==0x442ac0u&&tr.rec[0].off==0x51cu&&tr.rec[1].pc==0x465160u&&tr.rec[1].off==0xba8u&&tr.rec[2].off==0xc48u&&tr.rec[3].off==0xcf4u,"r062 442b20 child order");
        require(o62.u32(0)==(flag?0u:3u)&&o62.u32(4)==5u&&o62.u32(8)==0u,"r062 442b20 mode/header");
        require(o62.u32(0x0cu)==0x53u&&o62.u32(0x208u)==0x53u&&o62.u32(0x20cu)==0u,"r062 442b20 fill range");
        require(o62.u32(0x214u)==0u&&o62.u32(0x218u)==0u&&o62.u32(0x21cu)==0xffffffffu&&o62.u8(0x220u)==0u&&o62.u8(0x221u)==0u&&o62.u8(0x240u)==0u&&o62.u8(0x241u)==0u&&o62.u8(0x260u)==0u,"r062 442b20 state bytes");
        require(bits(o62.f32(0xd98u))==0u&&bits(o62.f32(0xd9cu))==0u&&bits(o62.f32(0xda0u))==0u,"r062 442b20 zero floats");
        require(o62.u32(0x484u)==0u&&o62.u32(0x518u)==0u,"r062 442b20 pointers clear");
    }
    }
    // r063 state reset + selector block. Raw PC handles remain tokens; virtual
    // dispatch and the 0x4EE930 manager service are explicit callbacks.
    {
    struct R63Trace{unsigned float_calls{},virtual_calls{},void_calls{};std::uint32_t float_pc{},handle{},slot{},void_pc{},void_handle{},virtual_ret{0x63abcdefu};float timer{120.0f};} tr63{};
    auto fcb=[](void* u,std::uint32_t pc)->float{auto* t=static_cast<R63Trace*>(u);++t->float_calls;t->float_pc=pc;return timer_value_4af500(t->timer);};
    auto vcb=[](void* u,std::uint32_t handle,std::uint32_t slot)->std::uint32_t{auto* t=static_cast<R63Trace*>(u);++t->virtual_calls;t->handle=handle;t->slot=slot;return t->virtual_ret;};
    auto dcb=[](void* u,std::uint32_t pc,std::uint32_t handle){auto* t=static_cast<R63Trace*>(u);++t->void_calls;t->void_pc=pc;t->void_handle=handle;};
    PcObjectResetServices rsv63{&tr63,fcb};PcObjectSelectorServices ssv63{&tr63,vcb,dcb};
    std::array<std::uint8_t,0x2000> raw63{};raw63.fill(0xa5u);Bytes o63(raw63.data(),raw63.size());
    std::uint32_t scale_bits63=0x3c881469u;float scale63{};std::memcpy(&scale63,&scale_bits63,4);
    require(bits(timer_value_4af500(-0.0f))==0x80000000u,"r063 4af500 exact getter");
    require(object_state_reset_442c20(o63,0u,scale63,rsv63)==1u,"r063 442c20 return AL");
    require(tr63.float_calls==1u&&tr63.float_pc==0x4af500u,"r063 442c20 child boundary");
    require(o63.u32(0)==3u&&o63.u32(0x214u)==0u&&o63.u32(0x218u)==1u&&o63.u32(0x21cu)==0xffffffffu&&o63.u8(0x220u)==0u,"r063 442c20 mode0 state");
    require(bits(o63.f32(0xd98u))==bits(tr63.timer*scale63)&&bits(o63.f32(0xd9cu))==bits(tr63.timer*scale63)&&bits(o63.f32(0xda0u))==scale_bits63,"r063 442c20 float state");
    require(o63.u8(0x221u)==0u&&o63.u8(0x240u)==0u&&o63.u8(0x241u)==0u&&o63.u8(0x260u)==0u,"r063 442c20 byte stacks clear");
    tr63.float_calls=0;raw63.fill(0x5au);require(object_state_reset_442c20(o63,1u,scale63,rsv63)==1u,"r063 442c20 nonzero mode return");
    require(tr63.float_calls==0u&&o63.u32(0)==0u&&bits(o63.f32(0xd98u))==0u&&bits(o63.f32(0xd9cu))==0u&&bits(o63.f32(0xda0u))==0u,"r063 442c20 nonzero mode no child");

    auto reset_selector=[&]{raw63.fill(0u);tr63.virtual_calls=0;tr63.void_calls=0;tr63.handle=0;tr63.slot=0;tr63.void_pc=0;tr63.void_handle=0;};
    std::int32_t kind63=-99;PcObjectSelectorGlobals g63{};
    reset_selector();o63.puti(0x518u,1);o63.put32(0x498u,0x63000300u);g63={0x6300f000u,1u,1u};
    require(object_select_primary_442cb0(o63,kind63,g63,ssv63)==5u&&kind63==3,"r063 442cb0 priority3 manager override");
    require(tr63.virtual_calls==1u&&tr63.handle==0x63000300u&&tr63.slot==8u&&tr63.void_calls==1u&&tr63.void_pc==0x4ee930u&&tr63.void_handle==0x6300f000u&&g63.manager_flag8==0u,"r063 442cb0 manager side effect");
    reset_selector();o63.put8(0x494u,1u);o63.put32(0x490u,0x63000200u);g63={0x6300f000u,1u,1u};kind63=-99;
    require(object_select_primary_442cb0(o63,kind63,g63,ssv63)==tr63.virtual_ret&&kind63==2&&tr63.virtual_calls==1u&&tr63.handle==0x63000200u&&tr63.void_calls==0u&&g63.manager_flag8==1u,"r063 442cb0 priority2 suppress tail");
    reset_selector();o63.puti(0x484u,3);o63.put32(0x280u+3u*4u,0x63000000u);g63={};kind63=-99;
    require(object_select_primary_442cb0(o63,kind63,g63,ssv63)==tr63.virtual_ret&&kind63==0&&tr63.handle==0x63000000u,"r063 442cb0 indexed fallback");
    reset_selector();g63={0x6300f000u,1u,1u};kind63=-99;require(object_select_primary_442cb0(o63,kind63,g63,ssv63)==5u&&kind63==-1&&tr63.virtual_calls==0u&&tr63.void_calls==1u&&g63.manager_flag8==0u,"r063 442cb0 manager-only tail");

    reset_selector();o63.put8(0x494u,1u);o63.put32(0x490u,0u);o63.put8(0x48cu,1u);o63.put32(0x488u,0x63000100u);kind63=-99;
    require(object_select_secondary_442d70(o63,kind63,ssv63)==tr63.virtual_ret&&kind63==1&&tr63.virtual_calls==1u&&tr63.handle==0x63000100u,"r063 442d70 null priority2 falls to priority1");
    reset_selector();o63.put8(0x48cu,1u);o63.put32(0x488u,0u);kind63=-99;require(object_select_secondary_442d70(o63,kind63,ssv63)==0u&&kind63==-1&&tr63.virtual_calls==0u,"r063 442d70 null priority1 returns zero");
    }
    // PC 0x4DEF50 belongs to the selected child, not the owner selector.
    require(frontend_gate_choice_class_4def50(0,0)==0u&&
            frontend_gate_choice_class_4def50(0,1)==2u&&
            frontend_gate_choice_class_4def50(0,2)==3u,
            "r131 empty frontend gate choices");
    require(frontend_gate_choice_class_4def50(3,2)==1u&&
            frontend_gate_choice_class_4def50(3,4)==1u&&
            frontend_gate_choice_class_4def50(3,5)==2u&&
            frontend_gate_choice_class_4def50(3,6)==3u&&
            frontend_gate_choice_class_4def50(3,7)==4u&&
            frontend_gate_choice_class_4def50(3,8)==0u,
            "r131 short frontend gate choice range");
    require(frontend_gate_choice_class_4def50(9,10)==1u&&
            frontend_gate_choice_class_4def50(9,11)==3u&&
            frontend_gate_choice_class_4def50(9,12)==4u&&
            frontend_gate_choice_class_4def50(9,13)==0u,
            "r131 long frontend gate choice range");
    {
        std::array<std::uint8_t,0x17b8u> gate_storage{};
        Bytes gate(gate_storage.data(),gate_storage.size());
        PcFrontendGateList empty_list{};
        frontend_gate_list_initialize_4ded80(gate,0u,empty_list);
        require(gate.u32(0x34u)==0u&&gate.u32(0x3cu)==4u&&
                empty_list.count==4u&&empty_list.enabled[0u]==1u&&
                empty_list.enabled[3u]==0u,"PC empty-profile menu list");
        require(frontend_gate_list_move_4ed250_4ed2a0(gate,empty_list,false)&&
                gate.u32(0x38u)==2u&&
                frontend_gate_list_move_4ed250_4ed2a0(gate,empty_list,true)&&
                gate.u32(0x38u)==0u,"PC menu wraps past disabled last row");
        PcFrontendGateList full_list{};
        frontend_gate_list_initialize_4ded80(gate,8u,full_list);
        require(full_list.count==13u&&gate.u32(0x3cu)==13u&&
                full_list.enabled[10u]==0u&&full_list.enabled[12u]==1u,
                "PC full-profile menu disables create row");
        gate.put32(0x38u,9u);
        require(frontend_gate_list_move_4ed250_4ed2a0(gate,full_list,true)&&
                gate.u32(0x38u)==11u,"PC full-profile cursor skips disabled row");
        frontend_gate_list_initialize_4ded80(gate,0u,empty_list);
        gate.put32(0x17b4u,2u); // 0x4DEC60 constructor default
        require(frontend_gate_control_4df120(gate,0u,false,false,0u)==0u&&
                gate.u32(0x04u)==0u,"r140 key22 waits for 0x48CBB0 animation readiness");
        require(frontend_gate_control_4df120(gate,0u,true,true,0u)==0u&&
                gate.u32(0x04u)==0u,"r140 owner overlay suppresses key22 input");
        require(frontend_gate_control_4df120(gate,0u,true,false,0u)==0u&&
                gate.u32(0x04u)==1u&&gate.u32(0x17b4u)==1u,
                "r140 empty profile confirm latches key1, not START mode");
        gate.put8(0x17b0u,1u);
        require(frontend_gate_control_4df120(gate,0xffffffffu,true,false,0u)==1u,
                "r140 completed key22 animation returns owner action1");
        gate.put32(0x17b4u,0u);gate.put32(0x34u,3u);gate.put32(0x38u,3u);
        require(frontend_gate_control_4df120(gate,0xffffffffu,true,false,0u)==2u&&
                gate.u32(0x17b4u)==2u,"r140 existing profile returns owner action2");
        gate.put32(0x17b4u,0u);gate.put32(0x38u,5u);
        require(frontend_gate_control_4df120(gate,0xffffffffu,true,false,0u)==4u&&
                gate.u32(0x04u)==30u,"r140 empty profile new-entry branch targets key30");
    }
    // r064 immediate object dispatch/state-table helpers. Child handles remain opaque
    // 32-bit guest tokens; the host callbacks only decode the synthetic test range.
    {
    struct R64Trace{std::vector<std::uint32_t> virt{};unsigned tail_calls{};std::uint32_t tail_pc{};std::size_t tail_off{};std::array<std::uint32_t,132> states{};} tr64{};
    constexpr std::uint32_t hbase64=0x64000000u,hstride64=0x40u;
    auto vh64=[](unsigned i){return hbase64+std::uint32_t(i)*hstride64;};
    auto vcb64=[](void* u,std::uint32_t h,std::uint32_t slot){auto* t=static_cast<R64Trace*>(u);if(slot!=0x0cu)throw std::runtime_error("r064 virtual slot");t->virt.push_back(h);};
    auto ecb64=[](void* u,std::uint32_t pc,Bytes,std::size_t off){auto* t=static_cast<R64Trace*>(u);++t->tail_calls;t->tail_pc=pc;t->tail_off=off;};
    auto ucb64=[](void* u,std::uint32_t pc,std::uint32_t h)->std::uint32_t{auto* t=static_cast<R64Trace*>(u);if(pc!=0x564c90u||h<hbase64||((h-hbase64)%hstride64)!=0u)throw std::runtime_error("r064 u32 service");const auto i=(h-hbase64)/hstride64;if(i>=t->states.size())throw std::runtime_error("r064 handle index");return t->states[i];};
    PcObjectStateServices sv64{&tr64,vcb64,ecb64,ucb64};
    std::array<std::uint8_t,0x2000> raw64{};Bytes o64(raw64.data(),raw64.size());

    raw64.fill(0u);o64.puti(0x484u,2);o64.put32(0x280u+2u*4u,vh64(1));o64.puti(0x518u,1);o64.put32(0x498u,vh64(2));o64.put8(0x494u,1u);o64.put32(0x490u,vh64(3));o64.put8(0x48cu,1u);o64.put32(0x488u,vh64(4));
    object_dispatch_state_442e00(o64,sv64);
    require(tr64.virt==std::vector<std::uint32_t>({vh64(1),vh64(2),vh64(3),vh64(4)}),"r064 442e00 dispatch order");
    require(tr64.tail_calls==1u&&tr64.tail_pc==0x446a50u&&tr64.tail_off==0x51cu,"r064 442e00 tail boundary");
    tr64.virt.clear();tr64.tail_calls=0;o64.put32(0x490u,0u);o64.put32(0x488u,0u);object_dispatch_state_442e00(o64,sv64);
    require(tr64.virt==std::vector<std::uint32_t>({vh64(1),vh64(2)})&&tr64.tail_calls==1u,"r064 442e00 null flagged handles skipped");

    raw64.fill(0xa5u);tr64.states.fill(0x66u);tr64.states[0]=4u;tr64.states[1]=10u;tr64.states[2]=21u;tr64.states[3]=77u;o64.puti(0x484u,4);
    for(unsigned i=0;i<4u;++i)o64.put32(0x284u+i*4u,vh64(i));
    object_refresh_state_table_442e70(o64,sv64);
    require(o64.u32(0x0cu)==0x53u&&o64.u32(0x10u)==0x53u&&o64.u32(0x14u)==0x53u&&o64.u32(0x18u)==77u,"r064 442e70 suppress states");
    require(o64.u32(0x208u)==0x53u,"r064 442e70 full 128-slot fill");

    raw64.fill(0u);tr64.states[7]=0x1234abcdu;o64.puti(0x484u,2);o64.put32(0x284u,vh64(7));
    require(object_query_previous_state_442ec0(o64,sv64)==0x1234abcdu,"r064 442ec0 previous state");
    o64.puti(0x484u,1);require(object_query_previous_state_442ec0(o64,sv64)==0x53u,"r064 442ec0 short list sentinel");

    raw64.fill(0u);o64.put8(0x220u,31u);object_store_depth_pair_442f20(o64,0x12u,0x34u);
    require(o64.u8(0x240u)==0x12u&&o64.u8(0x260u)==0x34u,"r064 442f20 depth-indexed pair");

    raw64.fill(0u);tr64.states[9]=0xc001d00du;o64.puti(0x484u,3);o64.put32(0x280u+3u*4u,vh64(9));
    require(object_query_last_state_443040(o64,sv64)==0xc001d00du,"r064 443040 last state");
    o64.puti(0x484u,0);require(object_query_last_state_443040(o64,sv64)==0x53u,"r064 443040 empty sentinel");

    raw64.fill(0u);o64.put32(0x218u,2u);o64.puti(0x484u,1);require(object_has_active_state_443060(o64)==1u,"r064 443060 mode2 indexed active");
    o64.put32(0x218u,1u);require(object_has_active_state_443060(o64)==0u,"r064 443060 nonmode2 ignores indexed");
    o64.put8(0x494u,1u);require(object_has_active_state_443060(o64)==1u,"r064 443060 flag active");
    o64.put8(0x494u,0u);o64.puti(0x518u,1);require(object_has_active_state_443060(o64)==1u,"r064 443060 count active");
    }
    // r065 state-pair lookup/update and flagged-handle release.  The lookup table
    // is portable immutable data copied from the 48 PC records before sentinel 0x53.
    {
    const auto& pairs65=pc_object_state_pair_table_r065();
    require(pairs65.size()==PcObjectStatePairCount&&pairs65.front().key==0u&&pairs65.back().key==60u,"r065 state-pair table extent");
    const PcObjectStatePairTable table65{pairs65.data(),pairs65.size()};
    std::array<std::uint8_t,0x2000> raw65{};Bytes o65(raw65.data(),raw65.size());
    o65.put8(0x220u,3u);object_store_state_pair_442f50(o65,11u,table65);
    require(o65.u8(0x224u)==1u&&o65.u8(0x244u)==0u,"r065 442f50 key11 pair");
    object_store_state_pair_442f50(o65,4u,table65);require(o65.u8(0x224u)==0u&&o65.u8(0x244u)==0u,"r065 442f50 key4 pair");
    object_store_state_pair_442f50(o65,9u,table65);require(o65.u8(0x224u)==1u&&o65.u8(0x244u)==1u,"r065 442f50 missing-key fallback");
    object_store_state_pair_442f50(o65,0x53u,table65);require(o65.u8(0x224u)==1u&&o65.u8(0x244u)==1u,"r065 442f50 sentinel fallback");

    struct R65UpdateTrace{std::vector<std::uint32_t> queried{};unsigned embedded_calls{};std::uint32_t embedded_pc{},embedded_arg{};std::size_t embedded_off{};} tr65{};
    auto q65=[](void* u,std::uint32_t pc,std::uint32_t h)->std::uint32_t{auto* t=static_cast<R65UpdateTrace*>(u);if(pc!=0x564c90u)throw std::runtime_error("r065 query pc");t->queried.push_back(h);if(h==0x65000100u)return 11u;if(h==0x65000200u)return 4u;if(h==0x65000300u)return 43u;if(h==0x65000400u)return 60u;if(h==0u)return 9u;return 0x53u;};
    auto e65=[](void* u,std::uint32_t pc,Bytes,std::size_t off,std::uint32_t arg){auto* t=static_cast<R65UpdateTrace*>(u);++t->embedded_calls;t->embedded_pc=pc;t->embedded_off=off;t->embedded_arg=arg;};
    PcObjectStateUpdateServices us65{&tr65,q65,e65};
    auto reset65=[&]{raw65.fill(0u);o65.put8(0x220u,2u);tr65=R65UpdateTrace{};};
    reset65();o65.puti(0x518u,1);o65.put32(0x498u,0x65000100u);o65.put8(0x494u,1u);o65.put32(0x490u,0x65000200u);
    object_update_state_442fd0(o65,table65,us65);
    require(tr65.queried==std::vector<std::uint32_t>({0x65000100u})&&tr65.embedded_calls==1u&&tr65.embedded_pc==0x446ea0u&&tr65.embedded_off==0x51cu&&tr65.embedded_arg==11u,"r065 442fd0 primary state priority");
    require(o65.u8(0x223u)==1u&&o65.u8(0x243u)==0u,"r065 442fd0 pair from selected state");
    reset65();o65.put8(0x494u,1u);o65.put32(0x490u,0u);o65.put8(0x48cu,1u);o65.put32(0x488u,0x65000200u);object_update_state_442fd0(o65,table65,us65);
    require(tr65.queried==std::vector<std::uint32_t>({0x65000200u})&&tr65.embedded_arg==4u&&o65.u8(0x223u)==0u&&o65.u8(0x243u)==0u,"r065 442fd0 null priority2 falls through");
    reset65();o65.put8(0x48cu,1u);o65.put32(0x488u,0u);object_update_state_442fd0(o65,table65,us65);
    require(tr65.queried==std::vector<std::uint32_t>({0u})&&tr65.embedded_arg==9u,"r065 442fd0 priority1 keeps null query");
    reset65();o65.puti(0x484u,3);o65.put32(0x280u+3u*4u,0x65000300u);object_update_state_442fd0(o65,table65,us65);
    require(tr65.queried==std::vector<std::uint32_t>({0x65000300u})&&tr65.embedded_arg==43u&&o65.u8(0x223u)==1u&&o65.u8(0x243u)==1u,"r065 442fd0 indexed fallback");
    reset65();object_update_state_442fd0(o65,table65,us65);require(tr65.queried.empty()&&tr65.embedded_arg==0x53u&&o65.u8(0x223u)==1u&&o65.u8(0x243u)==1u,"r065 442fd0 empty sentinel");

    struct R65ReleaseTrace{std::vector<std::uint32_t> pre{};std::vector<std::uint32_t> destroy{};std::vector<std::uint32_t> protect{};} rr65{};
    auto rv65=[](void* u,std::uint32_t pc,std::uint32_t h){auto* t=static_cast<R65ReleaseTrace*>(u);if(pc==0x441210u)t->pre.push_back(h);};
    auto ra65=[](void* u,std::uint32_t pc,std::uint32_t h,std::uint32_t a){auto* t=static_cast<R65ReleaseTrace*>(u);if(pc!=0x441219u||a!=1u)throw std::runtime_error("r065 release arg");t->destroy.push_back(h);};
    auto ri65=[](void* u,std::uint32_t pc,std::uint32_t h)->std::int8_t{auto* t=static_cast<R65ReleaseTrace*>(u);if(pc!=0x01039ba0u)throw std::runtime_error("r065 release protect pc");t->protect.push_back(h);return h==0x65001000u?2:1;};
    PcRuntimeControlServices rs65{&rr65,rv65,ra65,nullptr,nullptr,nullptr,ri65};
    raw65.fill(0u);o65.put8(0x220u,4u);o65.put8(0x223u,0xa5u);o65.put8(0x245u,0x5au);o65.put8(0x222u,0xa6u);o65.put8(0x244u,0x5bu);o65.put32(0x51cu+0x408u,3u);
    o65.put8(0x494u,1u);o65.put32(0x490u,0x65001000u);o65.put8(0x48cu,1u);o65.put32(0x488u,0x65002000u);
    object_release_flagged_handles_4430b0(o65,rs65);
    require(rr65.pre==std::vector<std::uint32_t>({0x65001000u,0x65002000u})&&rr65.destroy==rr65.pre&&rr65.protect==rr65.pre,"r065 4430b0 release order");
    require(o65.u32(0x490u)==0u&&o65.u8(0x494u)==0u&&o65.u32(0x488u)==0u&&o65.u8(0x48cu)==0u,"r065 4430b0 clears active handles");
    require(o65.u8(0x220u)==2u&&o65.u8(0x223u)==0u&&o65.u8(0x245u)==0u&&o65.u8(0x222u)==0u&&o65.u8(0x244u)==0u&&o65.u32(0x51cu+0x408u)==1u,"r065 4430b0 reuses 441200 effects");
    raw65.fill(0u);rr65=R65ReleaseTrace{};o65.put32(0x490u,0x65001000u);o65.put32(0x488u,0x65002000u);object_release_flagged_handles_4430b0(o65,rs65);
    require(rr65.pre.empty()&&o65.u32(0x490u)==0x65001000u&&o65.u32(0x488u)==0x65002000u,"r065 4430b0 inactive flags preserve handles");
    }
    // r066 UI resource parent + event-id dispatcher. The three open resource
    // operations are explicit service boundaries; already-closed UI/query leaves
    // execute through their native reconstructions.
    {
    struct R66OpenRec{std::uint32_t site{},pc{};std::size_t off{};std::array<std::uint32_t,11> args{};bool config{};};
    struct R66Trace{std::vector<R66OpenRec> recs{};} tr66{};
    auto ov66=[](void* u,std::uint32_t site,std::uint32_t pc,Bytes,std::size_t off){auto* t=static_cast<R66Trace*>(u);t->recs.push_back({site,pc,off,{},false});};
    auto oc66=[](void* u,std::uint32_t site,std::uint32_t pc,Bytes,std::size_t off,const std::array<std::uint32_t,11>& a){auto* t=static_cast<R66Trace*>(u);t->recs.push_back({site,pc,off,a,true});};
    PcObjectUiOpenServices os66{&tr66,ov66,oc66};PcUiNotifyGlobals g66{};PcUiNotifyServices us66{};
    std::array<std::uint8_t,0x2000> raw66{};Bytes o66(raw66.data(),raw66.size());
    auto clear66=[&]{raw66.fill(0u);tr66.recs.clear();o66.put32(0xba8u+8u,0xffffffffu);o66.put32(0xc48u+8u,0xffffffffu);o66.put32(0xc50u,0xffffffffu);o66.put32(0xce8u,0xffffffffu);};
    clear66();o66.put8(0xcf0u,0u);o66.put32(0xba8u+8u,0x66000100u);o66.put32(0xc48u+8u,0x66000200u);o66.put32(0xba8u+0x24u,1u);o66.put32(0xc48u+0x24u,1u);object_ui_state_update_443110(o66,g66,us66,os66);
    require(o66.u32(0xba8u+8u)==0xffffffffu&&o66.u32(0xc48u+8u)==0xffffffffu&&tr66.recs.empty(),"r066 443110 disabled resets both resources");
    clear66();o66.put8(0xcf0u,1u);o66.put32(0xce8u,0x6600aaaau);o66.put32(0xcecu,0x6600bbbbu);o66.put32(0xba8u+0x24u,1u);object_ui_state_update_443110(o66,g66,us66,os66);
    require(o66.u8(0xcf1u)==1u&&o66.u32(0xce8u)==0x6600aaaau&&tr66.recs.size()==4u&&tr66.recs[0].site==0x443168u&&tr66.recs[0].args[0]==0x6600bbbbu&&tr66.recs[1].site==0x44316fu,"r066 443110 initial resource branch");
    clear66();o66.put8(0xcf0u,1u);o66.put8(0xcf1u,1u);o66.put32(0xce8u,0x6600ccccu);o66.put32(0xcecu,0x6600ddddu);object_ui_state_update_443110(o66,g66,us66,os66);
    require(o66.u8(0xcf1u)==0u&&o66.u32(0xce8u)==0xffffffffu&&o66.u32(0xcecu)==0x6600ccccu&&tr66.recs.size()==4u&&tr66.recs[2].site==0x4431e6u&&tr66.recs[3].site==0x4431edu,"r066 443110 pending-id commit branch");
    clear66();o66.put8(0xcf0u,1u);o66.put32(0xcecu,0x6600eeeeu);object_ui_state_update_443110(o66,g66,us66,os66);
    require(tr66.recs.size()==6u&&tr66.recs[2].site==0x443243u&&tr66.recs[2].args[0]==0x6600eeeeu&&tr66.recs[4].site==0x44327cu&&tr66.recs[4].args[0]==0x004400d3u&&tr66.recs[5].site==0x443286u,"r066 443110 idle dual-resource branch");

    struct R66Query{std::uint32_t ret{55u};unsigned calls{};} q66{};
    auto qu66=[](void* u,std::uint32_t pc,std::uint32_t)->std::uint32_t{auto* q=static_cast<R66Query*>(u);if(pc!=0x564c90u)throw std::runtime_error("r066 query pc");++q->calls;return q->ret;};
    PcObjectStateServices qs66{&q66,nullptr,nullptr,qu66};
    clear66();require(object_event_dispatch_4432b0(o66,1u,qs66)==0x00440051u,"r066 4432b0 fixed dispatch");
    o66.put32(0x20cu,0u);require(object_event_dispatch_4432b0(o66,37u,qs66)==0x00440049u,"r066 4432b0 conditional zero");
    o66.put32(0x20cu,3u);require(object_event_dispatch_4432b0(o66,37u,qs66)==0x00440048u,"r066 4432b0 conditional nonzero");
    o66.puti(0x484u,2);o66.put32(0x284u,0x66001000u);q66.calls=0;q66.ret=55u;require(object_event_dispatch_4432b0(o66,46u,qs66)==0x00440054u&&q66.calls==1u,"r066 4432b0 previous-state redispatch");
    require(object_event_dispatch_4432b0(o66,61u,qs66)==0xffffffffu,"r066 4432b0 out-of-range");
    }
    // r067 event callback dispatcher + two normal-path factories.  The runtime
    // callback table is explicit data; embedded 446FC0/446EA0 and the selected
    // callback remain service boundaries while 442F50 executes natively.
    {
    std::array<PcObjectEventCallbackEntry,PcObjectEventCallbackCount> cb67{};
    for(std::size_t i=0;i<cb67.size();++i)cb67[i]={std::uint32_t(i),0x67000000u+std::uint32_t(i)};
    const PcObjectEventCallbackTable cbt67{cb67.data(),cb67.size()};
    const auto& pairs67a=pc_object_state_pair_table_r065();const PcObjectStatePairTable pairs67{pairs67a.data(),pairs67a.size()};
    struct R67Trace{unsigned push{},set{},cb{};std::size_t push_off{},set_off{},index{};std::uint32_t set_arg{},token{},ret{0x67abcdefu};} tr67{};
    auto pv67=[](void* u,std::uint32_t pc,Bytes,std::size_t off){auto* t=static_cast<R67Trace*>(u);if(pc!=0x446fc0u)throw std::runtime_error("r067 push pc");++t->push;t->push_off=off;};
    auto pu67=[](void* u,std::uint32_t pc,Bytes,std::size_t off,std::uint32_t a){auto* t=static_cast<R67Trace*>(u);if(pc!=0x446ea0u)throw std::runtime_error("r067 set pc");++t->set;t->set_off=off;t->set_arg=a;};
    auto pc67=[](void* u,std::uint32_t token,std::size_t index)->std::uint32_t{auto* t=static_cast<R67Trace*>(u);++t->cb;t->token=token;t->index=index;return t->ret;};
    PcObjectEventDispatchServices ds67{&tr67,pv67,pu67,pc67};
    std::array<std::uint8_t,0x2000> raw67{};Bytes o67(raw67.data(),raw67.size());
    o67.put8(0x220u,0u);o67.put8(0x221u,0u);o67.put8(0x241u,1u);
    require(object_dispatch_callback_443420(o67,11u,pairs67,cbt67,ds67)==tr67.ret,"r067 443420 callback return");
    require(o67.u8(0x220u)==1u&&o67.u8(0x222u)==1u&&o67.u8(0x242u)==0u,"r067 443420 history copy and pair overwrite");
    require(tr67.push==1u&&tr67.push_off==0x51cu&&tr67.set==1u&&tr67.set_off==0x51cu&&tr67.set_arg==11u&&tr67.cb==1u&&tr67.index==11u&&tr67.token==0x6700000bu,"r067 443420 boundary order payload");
    raw67.fill(0u);tr67=R67Trace{};o67.put8(0x220u,31u);o67.put8(0x240u,0u);o67.put8(0x260u,1u);
    object_dispatch_callback_443420(o67,4u,pairs67,cbt67,ds67);
    require(o67.u8(0x220u)==32u&&tr67.push==1u&&o67.u8(0x241u)==0u&&o67.u8(0x261u)==0u,"r067 443420 depth32 sentinel slot");
    raw67.fill(0u);tr67=R67Trace{};o67.put8(0x220u,32u);o67.put8(0x241u,0x5au);o67.put8(0x261u,0xa5u);
    object_dispatch_callback_443420(o67,60u,pairs67,cbt67,ds67);
    require(o67.u8(0x220u)==32u&&tr67.push==0u&&tr67.set==1u&&tr67.cb==1u,"r067 443420 saturated depth skips push");
    const auto snap67=raw67;tr67=R67Trace{};require(object_dispatch_callback_443420(o67,0xdeadbeefu,pairs67,cbt67,ds67)==0u&&raw67==snap67&&tr67.push==0u&&tr67.set==0u&&tr67.cb==0u,"r067 443420 missing key no-op");

    struct F67{unsigned alloc{},ctor{};std::uint32_t bytes{},ctor_pc{},obj{},alloc_ret{},ctor_ret{};} f67{};
    auto fa67=[](void* u,std::uint32_t pc,std::uint32_t bytes)->std::uint32_t{auto* f=static_cast<F67*>(u);if(pc!=0x5802cfu)throw std::runtime_error("r067 alloc pc");++f->alloc;f->bytes=bytes;return f->alloc_ret;};
    auto fc67=[](void* u,std::uint32_t pc,std::uint32_t obj)->std::uint32_t{auto* f=static_cast<F67*>(u);++f->ctor;f->ctor_pc=pc;f->obj=obj;return f->ctor_ret;};
    PcFactoryServices fs67{&f67,fa67,fc67};
    f67.alloc_ret=0x67001000u;f67.ctor_ret=0x67002000u;require(object_factory_4434c0(fs67)==0x67002000u&&f67.bytes==0x2980u&&f67.ctor_pc==0x4429b0u&&f67.obj==0x67001000u,"r067 4434c0 factory success");
    f67=F67{};f67.alloc_ret=0u;require(object_factory_4434c0(fs67)==0u&&f67.alloc==1u&&f67.ctor==0u,"r067 4434c0 factory null");
    f67=F67{};f67.alloc_ret=0x67003000u;f67.ctor_ret=0x67004000u;require(object_factory_443520(fs67)==0x67004000u&&f67.bytes==0x1320u&&f67.ctor_pc==0x442a60u,"r067 443520 factory success");
    }
    // r068 paired deleting destructors.  Child bodies remain explicit services;
    // checks cover exact order, offsets, vector metadata, flags and self return.
    {
    struct R68Rec{std::uint32_t pc{},off{},elem{},count{},dtor{},token{};};
    struct R68Trace{std::vector<R68Rec> rec;} tr68;
    auto d68=[](void* u,std::uint32_t pc,Bytes,std::size_t off){static_cast<R68Trace*>(u)->rec.push_back({pc,std::uint32_t(off),0,0,0,0});};
    auto v68=[](void* u,std::uint32_t pc,Bytes,std::size_t off,std::uint32_t elem,std::uint32_t count,std::uint32_t dtor){static_cast<R68Trace*>(u)->rec.push_back({pc,std::uint32_t(off),elem,count,dtor,0});};
    auto f68=[](void* u,std::uint32_t pc,std::uint32_t token){static_cast<R68Trace*>(u)->rec.push_back({pc,0,0,0,0,token});};
    PcObjectDestroyServices sv68{&tr68,d68,v68,f68};
    std::array<std::uint8_t,0x3000> raw68{};raw68.fill(0x68u);Bytes o68(raw68.data(),raw68.size());const auto snap68=raw68;
    object_destroy_body_4435a0(o68,sv68);
    require(raw68==snap68&&tr68.rec.size()==5u,"r068 4435a0 body no owned writes/count");
    require(tr68.rec[0].pc==0x58165du&&tr68.rec[0].off==0x174cu&&tr68.rec[0].elem==0x48cu&&tr68.rec[0].count==4u&&tr68.rec[0].dtor==0x48e630u,"r068 4435a0 vector1");
    require(tr68.rec[1].pc==0x58165du&&tr68.rec[1].off==0x1494u&&tr68.rec[1].elem==0x8cu&&tr68.rec[1].count==4u&&tr68.rec[1].dtor==0x49a650u,"r068 4435a0 vector2");
    require(tr68.rec[2].pc==0x48c520u&&tr68.rec[2].off==0x1e4u&&tr68.rec[3].pc==0x4edab0u&&tr68.rec[3].off==0x1a8u&&tr68.rec[4].pc==0x48f4d0u&&tr68.rec[4].off==0u,"r068 4435a0 child order");
    tr68.rec.clear();require(object_destroy_443580(o68,0x68123456u,0u,sv68)==0x68123456u&&tr68.rec.size()==5u,"r068 443580 no-delete wrapper");
    tr68.rec.clear();require(object_destroy_443580(o68,0x68123456u,1u,sv68)==0x68123456u&&tr68.rec.size()==6u&&tr68.rec.back().pc==0x5801a7u&&tr68.rec.back().token==0x68123456u,"r068 443580 deleting wrapper");
    tr68.rec.clear();object_destroy_body_443660(o68,sv68);require(tr68.rec.size()==3u&&tr68.rec[0].pc==0x48c520u&&tr68.rec[0].off==0x70u&&tr68.rec[1].pc==0x4edab0u&&tr68.rec[1].off==0x34u&&tr68.rec[2].pc==0x48f4d0u&&tr68.rec[2].off==0u,"r068 443660 child order");
    tr68.rec.clear();require(object_destroy_443640(o68,0x68654321u,3u,sv68)==0x68654321u&&tr68.rec.size()==4u&&tr68.rec.back().pc==0x5801a7u&&tr68.rec.back().token==0x68654321u,"r068 443640 deleting wrapper flags bit0");
    }
    // r069 callback-table/object runtime initializer.  The 4470F0 child stays
    // an explicit boundary; the already-closed 465250 reset executes natively.
    {
    struct R69Trace{unsigned embedded{},released{};std::uint32_t pc{},handle{};std::size_t off{};} tr69{};
    auto e69=[](void* u,std::uint32_t pc,Bytes,std::size_t off){auto* t=static_cast<R69Trace*>(u);++t->embedded;t->pc=pc;t->off=off;};
    auto h69=[](void* u,std::uint32_t pc,std::uint32_t handle){auto* t=static_cast<R69Trace*>(u);if(pc!=0x4285a0u)throw std::runtime_error("r069 release pc");++t->released;t->handle=handle;};
    PcUiNotifyServices ui69{&tr69,h69,nullptr,nullptr,nullptr,nullptr};
    PcObjectRuntimeInitServices sv69{&tr69,e69,ui69};
    std::array<PcObjectEventCallbackEntry,PcObjectEventCallbackCount> tab69{};
    for(std::size_t i=0;i<tab69.size();++i)tab69[i]={0x69000000u+std::uint32_t(i),0x69f00000u+std::uint32_t(i)};
    const auto untouched3=tab69[3]; const auto untouched82=tab69[82];
    std::array<std::uint8_t,0x2000> raw69{};raw69.fill(0x69u);Bytes o69(raw69.data(),raw69.size());
    o69.put32(0xba8u+8u,0x69123456u);o69.put32(0xba8u+0x24u,0u);
    require(object_runtime_init_4436c0(o69,tab69,0u,sv69)==0xffffff01u,"r069 4436c0 exact return");
    require(tab69[0].key==0u&&tab69[0].callback_token==0x441450u&&tab69[32].key==32u&&tab69[32].callback_token==0x442170u&&tab69[74].key==74u&&tab69[74].callback_token==0x442950u,"r069 callback populated slots");
    require(tab69[3].key==untouched3.key&&tab69[3].callback_token==untouched3.callback_token&&tab69[82].key==untouched82.key&&tab69[82].callback_token==untouched82.callback_token,"r069 untouched callback slots preserved");
    require(o69.u32(0)==3u&&o69.u32(4)==5u&&o69.u32(8)==0u&&o69.u32(0x0cu)==0x53u&&o69.u32(0x208u)==0x53u,"r069 core state/mode init");
    bool hist69=true;for(std::size_t i=0;i<0x20u;++i)hist69=hist69&&o69.u8(0x221u+i)==0u&&o69.u8(0x241u+i)==0u;require(hist69&&o69.u8(0x220u)==0u,"r069 history reset");
    require(tr69.embedded==1u&&tr69.pc==0x4470f0u&&tr69.off==0x51cu,"r069 embedded init boundary");
    require(tr69.released==1u&&tr69.handle==0x69123456u&&o69.u32(0xba8u+8u)==0xffffffffu&&o69.u32(0xba8u+0x28u)==0u&&o69.u32(0xba8u+0x2cu)==0u,"r069 closed resource reset");
    require(o69.u32(0x488u)==0u&&o69.u8(0x48cu)==0u&&o69.u32(0x490u)==0u&&o69.u8(0x494u)==0u&&o69.u32(0xce8u)==0xffffffffu&&o69.u32(0xcecu)==0xffffffffu&&o69.u8(0xcf0u)==0u&&o69.u8(0xcf1u)==0u&&o69.u32(0x218u)==0u&&o69.u8(0xd94u)==1u,"r069 handle/pending tail reset");
    tr69=R69Trace{};ui69.user=&tr69;sv69.user=&tr69;sv69.ui_services=ui69;o69.put32(0xba8u+8u,0xffffffffu);object_runtime_init_4436c0(o69,tab69,1u,sv69);require(o69.u32(0)==0u,"r069 nonzero global mode selects zero");
    }
    // r070 runtime teardown/reset.  Virtual handle calls and final global calls
    // are explicit boundaries; already-closed child counter/UI resource resets run natively.
    {
    struct R70Rec{std::uint32_t pc{},a{},b{},kind{};};
    struct R70Trace{std::vector<R70Rec> rec;std::uint32_t final_ret{0x70abcdefu};} tr70;
    auto reset70=[](void* u,std::uint32_t pc,Bytes,std::size_t off){static_cast<R70Trace*>(u)->rec.push_back({pc,std::uint32_t(off),0u,0u});};
    auto handle70=[](void* u,std::uint32_t slot,std::uint32_t h,std::uint32_t arg)->std::uint32_t{static_cast<R70Trace*>(u)->rec.push_back({slot,h,arg,1u});return h^slot^arg;};
    auto global70=[](void* u,std::uint32_t pc,std::uint32_t arg,bool has)->std::uint32_t{auto* t=static_cast<R70Trace*>(u);t->rec.push_back({pc,arg,has?1u:0u,2u});return pc==0x42dfb0u&&arg==0x47u?t->final_ret:0u;};
    PcUiNotifyServices ui70{};
    PcObjectRuntimeTeardownServices sv70{&tr70,reset70,handle70,global70,ui70};
    std::array<std::uint8_t,0x2000> raw70{};raw70.fill(0x70u);Bytes o70(raw70.data(),raw70.size());
    o70.put32(0x51cu+0x408u,9u);o70.put8(0x220u,4u);
    for(unsigned d=1;d<=4;++d){o70.put8(0x221u+d,std::uint8_t(0x80u+d));o70.put8(0x241u+d,std::uint8_t(0x90u+d));}
    o70.put32(0x484u,3u);o70.put32(0x284u,0x70001000u);o70.put32(0x288u,0u);o70.put32(0x28cu,0x70002000u);
    o70.put32(0x518u,2u);o70.put32(0x498u,0x70003000u);o70.put32(0x49cu,0u);
    o70.put8(0x494u,1u);o70.put32(0x490u,0x70004000u);o70.put8(0x48cu,1u);o70.put32(0x488u,0x70005000u);
    o70.put32(0xba8u+8u,0xffffffffu);o70.put32(0xcf4u+8u,0xffffffffu);
    require(object_runtime_teardown_443c30(o70,sv70)==0x70abcdefu,"r070 teardown exact final boundary return");
    require(o70.u32(0x484u)==0u&&o70.u32(0x518u)==0u&&o70.u32(0x490u)==0u&&o70.u8(0x494u)==0u&&o70.u32(0x488u)==0u&&o70.u8(0x48cu)==0u,"r070 teardown handle state cleared");
    require(o70.u8(0x220u)==0u&&o70.u32(0x51cu+0x408u)==9u,"r070 teardown pre-cleared depth leaves child counter unchanged");
    bool hist70=true;for(unsigned d=1;d<=4;++d)hist70=hist70&&o70.u8(0x221u+d)==0u&&o70.u8(0x241u+d)==0u;require(hist70,"r070 teardown history lanes cleared");
    require(o70.u32(0x218u)==4u&&o70.u32(0x214u)==0u,"r070 teardown final object mode");
    require(tr70.rec.size()==16u,"r070 teardown boundary count");
    require(tr70.rec[0].pc==0x447090u&&tr70.rec[1].pc==0x10u&&tr70.rec[1].a==0x70001000u&&tr70.rec[2].pc==0u&&tr70.rec[2].b==1u,"r070 teardown first handle order");
    require(tr70.rec[9].pc==0x10u&&tr70.rec[9].a==0x70005000u&&tr70.rec[10].pc==0u&&tr70.rec[10].a==0x70005000u,"r070 teardown last handle order");
    require(tr70.rec[11].pc==0x428600u&&tr70.rec[12].pc==0x4299c0u&&tr70.rec[12].a==0x44u&&tr70.rec[13].pc==0x4299c0u&&tr70.rec[13].a==0x47u&&tr70.rec[14].pc==0x42dfb0u&&tr70.rec[14].a==0x44u&&tr70.rec[15].pc==0x42dfb0u&&tr70.rec[15].a==0x47u,"r070 teardown final global sequence");
    }
    // r071 second callback dispatcher, handle-opening wrapper and state-code classifier.
    {
    struct R71Trace{std::vector<std::uint32_t> seq;std::uint32_t cb_ret{0x71123456u};std::uint32_t ready{1u};unsigned rel_void{},rel_arg{};};
    auto evvoid=[](void* u,std::uint32_t pc,Bytes,std::size_t off){auto* t=static_cast<R71Trace*>(u);t->seq.push_back(pc);t->seq.push_back(std::uint32_t(off));};
    auto evu32=[](void* u,std::uint32_t pc,Bytes,std::size_t off,std::uint32_t a){auto* t=static_cast<R71Trace*>(u);t->seq.push_back(pc);t->seq.push_back(std::uint32_t(off));t->seq.push_back(a);};
    auto evcb=[](void* u,std::uint32_t tok,std::size_t idx)->std::uint32_t{auto* t=static_cast<R71Trace*>(u);t->seq.push_back(0x71000000u|std::uint32_t(idx));t->seq.push_back(tok);return t->cb_ret;};
    auto evg=[](void* u,std::uint32_t pc,std::uint32_t a,bool has){auto* t=static_cast<R71Trace*>(u);t->seq.push_back(pc);t->seq.push_back(has?a:0xffffffffu);};
    auto ready=[](void* u,std::uint32_t h,std::uint32_t slot)->std::uint32_t{auto* t=static_cast<R71Trace*>(u);t->seq.push_back(0x71010000u|slot);t->seq.push_back(h);return t->ready;};
    auto rv=[](void* u,std::uint32_t,std::uint32_t){++static_cast<R71Trace*>(u)->rel_void;};
    auto rva=[](void* u,std::uint32_t,std::uint32_t,std::uint32_t){++static_cast<R71Trace*>(u)->rel_arg;};
    auto ri8=[](void*,std::uint32_t,std::uint32_t)->std::int8_t{return 0;};
    R71Trace tr71{};PcObjectEventDispatch443eb0Services ds71{&tr71,evvoid,evu32,evcb,evg};
    PcObjectOpenCallbackServices os71{&tr71,ready};PcRuntimeControlServices rs71{&tr71,rv,rva,nullptr,nullptr,nullptr,ri8};
    std::array<PcObjectEventCallbackEntry,PcObjectEventCallbackCount> tab71{};for(std::size_t i=0;i<tab71.size();++i)tab71[i]={std::uint32_t(i),0x71000000u+std::uint32_t(i)};
    PcObjectEventCallbackTable view71{tab71.data(),tab71.size()};
    const auto& p71=pc_object_state_pair_table_r065();PcObjectStatePairTable pair71{p71.data(),p71.size()};
    std::array<std::uint8_t,0x2000> raw71{};Bytes o71(raw71.data(),raw71.size());o71.put8(0x220u,0u);o71.put32(0x218u,3u);o71.put32(0x518u,0u);o71.put8(0x48cu,0u);o71.put8(0x494u,0u);
    PcObjectEventModeInputs in71{};in71.route_state_780258=3u;in71.game_mode_78026c=0x10u;
    require(object_dispatch_callback_443eb0(o71,11u,pair71,view71,in71,ds71)==0x71123456u,"r071 443eb0 callback return");
    require(o71.u8(0x220u)==1u&&o71.u32(0x21cu)==0x12u,"r071 443eb0 history/mode side effect");
    require(tr71.seq.size()>=9u&&tr71.seq[0]==0x446fc0u&&tr71.seq[2]==0x446ea0u,"r071 443eb0 embedded order");
    bool saw_pause=false,saw_state=false;for(auto v:tr71.seq){saw_pause=saw_pause||v==0x440930u;saw_state=saw_state||v==0x43f9e0u;}require(saw_pause&&saw_state,"r071 443eb0 mode3 global calls");
    const auto snap71=raw71;tr71.seq.clear();require(object_dispatch_callback_443eb0(o71,0xdeadbeefu,pair71,view71,in71,ds71)==0u&&raw71==snap71&&tr71.seq.empty(),"r071 443eb0 miss no-op");
    raw71.fill(0u);o71.put32(0x218u,0u);o71.put8(0x220u,0u);tr71.seq.clear();tr71.cb_ret=0x71101010u;tr71.ready=1u;
    object_open_callback_443fa0(o71,5u,pair71,view71,in71,ds71,rs71,os71);
    require(o71.u32(0x488u)==0x71101010u&&o71.u8(0x48cu)==1u,"r071 443fa0 keeps ready handle");
    tr71.seq.clear();tr71.ready=0u;tr71.cb_ret=0x71102020u;o71.put8(0x48cu,0u);o71.put32(0x488u,0u);
    object_open_callback_443fa0(o71,6u,pair71,view71,in71,ds71,rs71,os71);
    require(o71.u32(0x488u)==0u&&o71.u8(0x48cu)==0u&&tr71.rel_void>0u&&tr71.rel_arg>0u,"r071 443fa0 releases rejected handle");
    std::array<std::uint8_t,0x600> relraw71{};Bytes rel71(relraw71.data(),relraw71.size());rel71.put32(0x484u,2u);rel71.put32(0x284u,0x1234u);
    struct R71State{std::uint32_t value{21u};} st71;auto su32=[](void* u,std::uint32_t,std::uint32_t)->std::uint32_t{return static_cast<R71State*>(u)->value;};PcObjectStateServices ss71{&st71,nullptr,nullptr,su32};
    o71.put8(0xd94u,1u);require(object_state_code_443ff0(o71,0u,rel71,ss71)==0x440094u&&object_state_code_443ff0(o71,48u,rel71,ss71)==0x440090u,"r071 443ff0 direct classifier");
    require(object_state_code_443ff0(o71,46u,rel71,ss71)==0x440094u,"r071 443ff0 redirected previous state");
    o71.put8(0xd94u,0u);require(object_state_code_443ff0(o71,0u,rel71,ss71)==0xffffffffu,"r071 443ff0 disabled gate");
    }

    // r072 callback-list insertion/reopen and runtime snapshot commit.
    {
    struct R72Trace{std::vector<std::uint32_t> seq;std::uint32_t created{0x72000101u};std::uint32_t mode_raw{0xffffffffu};std::uint32_t ready{1u};};
    auto get72=[](void*,std::uint32_t pc,std::uint32_t h)->std::uint32_t{return pc==0x48f4e0u?(h&0xffu):((h>>8)&0xffu);};
    auto virt72=[](void* u,std::uint32_t h,std::uint32_t slot,std::uint32_t arg,bool has)->std::uint32_t{auto* t=static_cast<R72Trace*>(u);t->seq.push_back(0x72000000u|slot);t->seq.push_back(h);t->seq.push_back(has?arg:0xffffffffu);if(slot==0x18u)return t->mode_raw;if(slot==0x1cu)return 0u;if(slot==0x04u)return t->ready;return 0u;};
    auto link72=[](void* u,std::uint32_t pc,std::uint32_t a,std::uint32_t b){auto* t=static_cast<R72Trace*>(u);t->seq.push_back(pc);t->seq.push_back(a);t->seq.push_back(b);};
    auto push72=[](void*,std::uint32_t,Bytes,std::size_t){};auto set72=[](void*,std::uint32_t,Bytes,std::size_t,std::uint32_t){};
    auto cb72=[](void* u,std::uint32_t,std::size_t)->std::uint32_t{return static_cast<R72Trace*>(u)->created;};
    auto eg72=[](void*,std::uint32_t,std::uint32_t,bool){};
    auto ready72=[](void* u,std::uint32_t,std::uint32_t)->std::uint32_t{return static_cast<R72Trace*>(u)->ready;};
    auto rv72=[](void* u,std::uint32_t,std::uint32_t h){auto* t=static_cast<R72Trace*>(u);t->seq.push_back(0x72100000u);t->seq.push_back(h);};
    auto rva72=[](void* u,std::uint32_t,std::uint32_t h,std::uint32_t a){auto* t=static_cast<R72Trace*>(u);t->seq.push_back(0x72110000u|a);t->seq.push_back(h);};
    auto ri872=[](void*,std::uint32_t,std::uint32_t)->std::int8_t{return 0;};
    R72Trace tr72{};PcObjectListServices ls72{&tr72,get72,virt72,link72};PcObjectEventDispatchServices ed72{&tr72,push72,set72,cb72};
    PcObjectEventDispatch443eb0Services edb72{&tr72,push72,set72,cb72,eg72};PcObjectOpenCallbackServices op72{&tr72,ready72};PcRuntimeControlServices rr72{&tr72,rv72,rva72,nullptr,nullptr,nullptr,ri872};
    const auto& pp72=pc_object_state_pair_table_r065();PcObjectStatePairTable pair72{pp72.data(),pp72.size()};
    std::array<PcObjectEventCallbackEntry,PcObjectEventCallbackCount> tab72{};for(std::size_t i=0;i<tab72.size();++i)tab72[i]={std::uint32_t(i),0x72010000u+std::uint32_t(i)};PcObjectEventCallbackTable tv72{tab72.data(),tab72.size()};
    std::array<std::uint8_t,0x2000> raw72{};Bytes o72(raw72.data(),raw72.size());o72.put32(0x518u,0u);o72.put8(0x220u,0u);o72.put32(0x51cu+0x408u,3u);
    tr72.created=0x72000505u;tr72.ready=1u;tr72.mode_raw=0xffffffffu;require(object_insert_callback_4440f0(o72,5u,pair72,tv72,ed72,ls72)==0x72000505u,"r072 4440f0 append ready return");
    require(o72.u32(0x518u)==1u&&o72.u32(0x498u)==0x72000505u&&o72.u8(0x220u)==1u,"r072 4440f0 append/history");
    o72.put32(0x518u,1u);o72.put32(0x498u,0x72000101u);tr72.mode_raw=1u;tr72.created=0x72000606u;tr72.ready=1u;require(object_insert_callback_4440f0(o72,6u,pair72,tv72,ed72,ls72)==0x72000606u,"r072 4440f0 insert return");
    require(o72.u32(0x518u)==2u&&o72.u32(0x498u)==0x72000606u&&o72.u32(0x49cu)==0x72000101u,"r072 4440f0 insert before last");
    o72.put32(0x518u,1u);o72.put32(0x498u,0x72000101u);tr72.mode_raw=2u;tr72.created=0x72000707u;tr72.ready=1u;tr72.seq.clear();require(object_insert_callback_4440f0(o72,7u,pair72,tv72,ed72,ls72)==0x72000707u,"r072 4440f0 replace/link return");
    bool sawlink72=false;for(auto v:tr72.seq)sawlink72=sawlink72||v==0x48d870u;require(sawlink72&&o72.u32(0x518u)==1u,"r072 4440f0 link without count change");
    PcObjectEventModeInputs in72{};o72.put32(0x484u,1u);o72.put32(0x284u,0x72000009u);o72.put8(0x48cu,0u);o72.put8(0x494u,0u);tr72.created=0x72000a0au;tr72.ready=1u;require(object_reopen_callback_4442a0(o72,0u,0xdeadbeefu,pair72,tv72,in72,edb72,rr72,op72,ls72)==0u,"r072 4442a0 exact zero return");
    require(o72.u32(0x488u)==0x72000a0au&&o72.u8(0x48cu)==1u,"r072 4442a0 reopened indexed key");
    struct C72{R72Trace* t;unsigned prep{},glob{};} c72{&tr72};
    auto prep72=[](void* u,std::uint32_t pc,Bytes b,std::size_t off){auto* c=static_cast<C72*>(u);++c->prep;c->t->seq.push_back(pc);b.put32(off,2u);};
    auto glob72=[](void* u,std::uint32_t pc,std::uint32_t a0,std::uint32_t a1,std::uint32_t argc){auto* c=static_cast<C72*>(u);++c->glob;c->t->seq.push_back(pc);c->t->seq.push_back(argc);c->t->seq.push_back(a0);c->t->seq.push_back(a1);};
    auto cv72=[](void* u,std::uint32_t h,std::uint32_t slot,std::uint32_t arg,bool has)->std::uint32_t{auto* t=static_cast<C72*>(u)->t;t->seq.push_back(0x72000000u|slot);t->seq.push_back(h);t->seq.push_back(has?arg:0xffffffffu);if(slot==0x18u)return t->mode_raw;if(slot==0x1cu)return 0u;if(slot==0x04u)return t->ready;return 0u;};
    PcObjectRuntimeCommitServices cs72{&c72,prep72,glob72,cv72};struct S72{ } st72;auto su3272=[](void*,std::uint32_t,std::uint32_t h)->std::uint32_t{return (h>>8)&0xffu;};PcObjectStateServices ss72{&st72,nullptr,nullptr,su3272};
    std::array<std::uint8_t,0x2000> cbuf72{};Bytes co72(cbuf72.data(),cbuf72.size());co72.put32(0x484u,2u);co72.put32(0x284u,0x72001100u);co72.put32(0x288u,0x72001200u);co72.put8(0x48cu,0u);co72.put8(0x494u,0u);co72.put8(0x220u,2u);co72.put32(0x51cu+0x408u,5u);PcObjectRuntimeSnapshot444350 snap72{};snap72.active=0u;object_runtime_commit_444350(co72,snap72,ss72,rr72,cs72);
    require(c72.prep==1u&&co72.u32(4u)==2u&&co72.u32(0x21cu)==7u,"r072 444350 prepare/mode2");
    require(snap72.active==1u&&snap72.bytes[0]==2u&&snap72.bytes[8]==0x11u&&co72.u32(0x484u)==0u,"r072 444350 snapshot and clear count");
    require(co72.u8(0x220u)==0u&&co72.u32(0x51cu+0x408u)==3u,"r072 444350 primary inline history pop");
    require(c72.glob==1u,"r072 444350 mode2 global boundary");
    }
    // r073 functional blockers: 4EEC80 mode decoder + callback key/list primitives.
    {
    PcRuntimePrepareState st73{};
    require(runtime_primary_mode_4eea70(0u,st73)&&st73.primary_mode_78024c==1u,"r073 4eea70 selector0");
    require(runtime_primary_mode_4eea70(1u,st73)&&st73.primary_mode_78024c==0u,"r073 4eea70 selector1");
    require(runtime_primary_mode_4eea70(2u,st73)&&st73.primary_mode_78024c==3u,"r073 4eea70 selector2");
    require(runtime_primary_mode_4eea70(3u,st73)&&st73.primary_mode_78024c==2u&&!runtime_primary_mode_4eea70(4u,st73),"r073 4eea70 selector3/invalid");
    st73.route_state_780258=0x55u;require(runtime_route_mode_4eead0(0u,st73)&&st73.route_state_780258==0x55u,"r073 4eead0 zero noop");
    require(runtime_route_mode_4eead0(4u,st73)&&st73.route_state_780258==7u,"r073 4eead0 route4");
    require(runtime_route_mode_4eead0(6u,st73)&&st73.route_state_780258==10u&&!runtime_route_mode_4eead0(7u,st73),"r073 4eead0 route6/invalid");
    struct R73Svc{std::uint32_t count{4u},count_n{},count_cat{},sel_n{},sel_cat{},sel_idx{};} tr73;
    auto cnt73=[](void* u,std::uint32_t cat)->std::uint32_t{auto* t=static_cast<R73Svc*>(u);++t->count_n;t->count_cat=cat;return t->count;};
    auto sel73=[](void* u,std::uint32_t cat,std::uint32_t idx){auto* t=static_cast<R73Svc*>(u);++t->sel_n;t->sel_cat=cat;t->sel_idx=idx;};
    PcRuntimePrepareServices sv73{&tr73,cnt73,sel73};
    st73={};require(runtime_route_config_4eeb50(2u,7u,10u,st73,sv73),"r073 4eeb50 menu path");
    require(st73.selection_active_836374==1u&&st73.route_state_780258==6u&&tr73.count_n==1u&&tr73.count_cat==7u&&tr73.sel_n==1u&&tr73.sel_cat==7u&&tr73.sel_idx==4u,"r073 4eeb50 count/clamp/select");
    st73={};tr73={};require(runtime_route_config_4eeb50(4u,3u,2u,st73,sv73),"r073 4eeb50 cfg4");
    require(st73.event_config_7f94c4==std::array<std::uint32_t,5>{{1u,1u,2u,2u,1u}}&&st73.primary_mode_78024c==0u&&st73.route_state_780258==5u,"r073 4eeb50 cfg4 fields");
    st73={};require(runtime_route_config_4eeb50(5u,0u,6u,st73,sv73),"r073 4eeb50 cfg5");
    require(st73.event_config_7f94c4==std::array<std::uint32_t,5>{{0u,0u,0u,0u,0u}}&&st73.primary_mode_78024c==1u&&st73.route_state_780258==5u,"r073 4eeb50 cfg5 fields");
    st73={};require(runtime_route_config_4eeb50(6u,2u,3u,st73,sv73),"r073 4eeb50 cfg6");
    require(st73.event_config_7f94c4==std::array<std::uint32_t,5>{{3u,1u,1u,3u,2u}}&&st73.primary_mode_78024c==0u&&st73.route_state_780258==5u,"r073 4eeb50 cfg6 fields");
    std::array<std::uint8_t,0x300> pr73{};Bytes rp73(pr73.data(),pr73.size());
    st73={};tr73={};rp73.put32(0x208u,1u|(1u<<2));runtime_prepare_4eec80(rp73,st73,sv73);
    require(st73.primary_mode_78024c==0u&&tr73.sel_n==1u&&tr73.sel_cat==0u&&tr73.sel_idx==1u,"r073 4eec80 branch1 select");
    st73={};tr73={};rp73.put32(0x208u,1u|(4u<<5)|(5u<<12)|(1u<<11));runtime_prepare_4eec80(rp73,st73,sv73);
    require(st73.primary_mode_78024c==2u&&st73.route_state_780258==7u&&st73.output_code_656234==0x52u&&st73.output_flag_830395==0u,"r073 4eec80 route4 alternate special");
    st73={};tr73={};rp73.put32(0x20cu,1u);rp73.put32(0x208u,1u|(4u<<5)|(20u<<12));runtime_prepare_4eec80(rp73,st73,sv73);
    require(st73.primary_mode_78024c==0u&&st73.route_state_780258==7u&&st73.output_code_656234==50u&&st73.output_flag_830395==1u,"r073 4eec80 route4 normal output");
    st73={};tr73={};rp73.put32(0x208u,2u|(5u<<5)|(6u<<12)|(1u<<11));runtime_prepare_4eec80(rp73,st73,sv73);
    require(st73.primary_mode_78024c==3u&&st73.route_state_780258==8u&&st73.alternate_code_836174==0x47u,"r073 4eec80 route5 alternate output");
    std::array<std::uint8_t,0x600> cb73{};Bytes b73(cb73.data(),cb73.size());b73.put32(4u,0x73123456u);require(callback_key_48f4e0(b73)==0x73123456u,"r073 48f4e0 key getter");
    b73.put32(0x4d0u,2u);require(callback_append_48d870(b73,0x73abcdefu)&&b73.u32(0x4b8u)==0x73abcdefu&&b73.u32(0x4d0u)==3u,"r073 48d870 append");
    require(!callback_append_48d870(b73,0u)&&b73.u32(0x4d0u)==3u,"r073 48d870 null reject");
    }
    // r074 native category table, scalar runtime leaves, handle resolver and 444470 gate.
    {
    std::array<std::uint32_t,4> keys74{{0x10u,0x20u,0x30u,0x40u}};
    std::array<PcRuntimeCategoryRecord495930,7> rec74{};
    rec74[0].key=0x10u;rec74[1].key=0x20u;rec74[2].key=0x20u;rec74[3].key=0x30u;rec74[4].key=0x20u;rec74[5].key=0x40u;rec74[6].key=0x10u;
    PcRuntimeCategoryState cat74{3u,keys74.data(),keys74.size(),rec74.data(),rec74.size(),0u,0u};
    require(runtime_count_entries_495930(cat74,1u)==3u,"r074 495930 count key matches");
    cat74.mode_836358=2u;require(runtime_count_entries_495930(cat74,1u)==0u,"r074 495930 mode gate");cat74.mode_836358=3u;
    runtime_select_entry_4958a0(cat74,2u,7u);require(cat74.selected_key_67e6a4==0x30u&&cat74.selected_index_67e6a8==7u,"r074 4958a0 selection state");
    PcRuntimePrepareServices cps74{&cat74,runtime_count_entries_495930_service,runtime_select_entry_4958a0_service};
    PcRuntimePrepareState pst74{};require(runtime_route_config_4eeb50(1u,1u,99u,pst74,cps74),"r074 prepare uses native category services");
    require(cat74.selected_key_67e6a4==0x20u&&cat74.selected_index_67e6a8==3u,"r074 native count clamp integration");
    runtime_game_flag_43f870(pst74,0xabu);require(pst74.game_flag_780260==0xabu,"r074 43f870 flag setter");
    pst74.selection_active_836374=1u;runtime_selection_disable_4957e0(pst74);require(pst74.selection_active_836374==0u,"r074 4957e0 selection clear");
    require(runtime_external_block_4872e0(0x12345678u)==0x12345678u,"r074 4872e0 getter");
    require(!runtime_system_handle_active_4999c0(0xffffffffu)&&runtime_system_handle_active_4999c0(0u),"r074 4999c0 handle predicate");
    require(runtime_menu_state_450240(7u)==7u&&runtime_current_player_47f110()==0&&runtime_feature_mask_4536f0(5u,1u)==1u,"r074 scalar gates");

    std::array<std::uint8_t,0x40> h0raw{},h1raw{},h2raw{};Bytes h0(h0raw.data(),h0raw.size()),h1(h1raw.data(),h1raw.size()),h2(h2raw.data(),h2raw.size());
    h0.put32(8u,0x1bu);h0.put32(4u,0x74111111u);h1.put32(8u,0x2cu);h1.put32(4u,0x74222222u);h2.put32(8u,0x10u);h2.put32(4u,0x74333333u);
    std::array<PcNativeHandleBinding,3> bind74{{{0x74001000u,h0raw.data(),h0raw.size()},{0x74002000u,h1raw.data(),h1raw.size()},{0x74003000u,h2raw.data(),h2raw.size()}}};
    PcNativeHandleResolver hr74{bind74.data(),bind74.size()};
    require(native_handle_state_564c90(hr74,0x74002000u)==0x2cu&&native_handle_key_48f4e0(hr74,0x74003000u)==0x74333333u,"r074 native handle resolver leaves");

    std::array<std::uint8_t,0x2000> og74raw{},cg74raw{};Bytes og74(og74raw.data(),og74raw.size()),cg74(cg74raw.data(),cg74raw.size());
    std::array<PcObjectEventCallbackEntry,PcObjectEventCallbackCount> cb74{};for(std::size_t i=0;i<cb74.size();++i)cb74[i]={std::uint32_t(i),0x74010000u+std::uint32_t(i)};
    const auto& pairs74a=pc_object_state_pair_table_r065();PcObjectStatePairTable pairs74{pairs74a.data(),pairs74a.size()};PcObjectEventCallbackTable cbt74{cb74.data(),cb74.size()};
    struct T74{unsigned cfg{},cb{},ready{};std::uint32_t ret{0x74003000u};} t74;
    auto cfg74=[](void* u,Bytes,std::uint32_t sel,std::uint8_t flag){auto* t=static_cast<T74*>(u);require(sel==1u&&flag==1u,"r074 446f30 args");++t->cfg;};
    auto cbf74=[](void* u,std::uint32_t,std::size_t)->std::uint32_t{auto* t=static_cast<T74*>(u);++t->cb;return t->ret;};
    auto ready74=[](void* u,std::uint32_t,std::uint32_t slot)->std::uint32_t{auto* t=static_cast<T74*>(u);++t->ready;return slot==4u?1u:0u;};
    PcObjectEventDispatch443eb0Services ds74{&t74,nullptr,nullptr,cbf74,nullptr};PcRuntimeControlServices rs74{};PcObjectOpenCallbackServices os74{&t74,ready74};PcRuntimeGate444470Services gs74{&t74,cfg74};
    PcObjectEventModeInputs ei74{};PcRuntimeGate444470Inputs gi74{0x10u,0u,0xffffffffu,0u,1u};
    og74.put32(0x218u,2u);og74.put32(0x484u,0u);og74.put32(0x518u,0u);og74.put8(0x48cu,0u);og74.put8(0x494u,0u);og74.put8(0x220u,0u);cg74.put8(0x48cu,0u);
    object_runtime_gate_444470(og74,cg74,hr74,gi74,pairs74,cbt74,ei74,ds74,rs74,os74,gs74);
    require(t74.cfg==1u&&t74.cb==1u&&t74.ready==1u&&og74.u32(0x488u)==0x74003000u&&og74.u8(0x48cu)==1u,"r074 444470 success opens key2c");
    t74={};og74.put8(0x48cu,0u);og74.put32(0x488u,0u);cg74.put8(0x48cu,1u);cg74.put32(0x488u,0x74001000u);
    object_runtime_gate_444470(og74,cg74,hr74,gi74,pairs74,cbt74,ei74,ds74,rs74,os74,gs74);
    require(t74.cb==0u&&og74.u32(0x488u)==0u,"r074 444470 current state1b blocks");
    t74={};cg74.put8(0x48cu,0u);gi74.system_handle_67f614=0x1234u;
    object_runtime_gate_444470(og74,cg74,hr74,gi74,pairs74,cbt74,ei74,ds74,rs74,os74,gs74);
    require(t74.cb==0u,"r074 444470 system handle gate");
    }
    // r075 functional blockers: 44DA00 course selector, 446BB0/446F30 embedded config, mutable handle registry.
    {
    struct Load75{std::string data,course;std::uint8_t alt{};std::uint32_t ret{1u};unsigned calls{};} l75;
    auto apply75=[](void* u,const char* data,const char* course,std::uint8_t alt)->std::uint32_t{auto* t=static_cast<Load75*>(u);++t->calls;t->data=data;t->course=course;t->alt=alt;return t->ret;};
    PcCourseLoadState44da00 ls75{};PcCourseLoadServices44da00 lsv75{&l75,apply75};
    require(runtime_course_load_44da00(0u,0u,ls75,lsv75)&&l75.data=="csc_data_cvt"&&l75.course=="csc_data_cvt_course"&&ls75.force_sync_7d2d8c==1u,"r075 44da00 mode0 sync");
    l75={};l75.ret=0u;ls75={};require(!runtime_course_load_44da00(7u,1u,ls75,lsv75)&&l75.data=="csc_data_cvt_ren"&&l75.course=="csc_data_cvt_ren_course"&&ls75.force_sync_7d2d8c==0u,"r075 44da00 alternate return");
    l75={};ls75={};require(runtime_course_load_44da00(2u,0u,ls75,lsv75)&&l75.data=="csc_data_easy","r075 44da00 easy preset");

    struct E75{unsigned cfg{},fin{},fx{};std::uint32_t effect{},slot{},x{},y{},fxv{};} e75;
    auto cfg75=[](void* u,Bytes,std::uint32_t effect,std::uint32_t slot,std::uint32_t x,std::uint32_t y){auto* t=static_cast<E75*>(u);++t->cfg;t->effect=effect;t->slot=slot;t->x=x;t->y=y;};
    auto fin75=[](void* u,Bytes){++static_cast<E75*>(u)->fin;};
    auto fx75=[](void* u,std::uint32_t v){auto* t=static_cast<E75*>(u);++t->fx;t->fxv=v;};
    PcEmbeddedConfigServices446f30 es75{};es75.user=&e75;es75.configure_ui=cfg75;es75.finalize_ui=fin75;es75.global_effect=fx75;
    std::array<std::uint8_t,0x700> eraw75{};Bytes eb75(eraw75.data(),eraw75.size());eb75.put32(0x408u,1u);
    for(std::uint32_t k=0;k<4u;++k){const auto off=0x40cu+std::size_t(k)*0xa0u;eb75.put32(off+8u,0x75001000u+k);eb75.put32(off+0x24u,1u);eb75.put32(0x08u+0x20u+std::size_t(k)*4u,k==2u?4u:0x55u);}
    embedded_slot_refresh_446bb0(eb75,2u,es75);require(e75.cfg==1u&&e75.fin==1u&&e75.effect==0x002c013bu&&e75.slot==2u&&e75.x==0x43160000u&&e75.y==0x433c0000u,"r075 446bb0 map/coords");require(eb75.u32(0x40cu+2u*0xa0u+8u)==0xffffffffu,"r075 446bb0 native reset");
    e75={};for(std::uint32_t k=0;k<4u;++k)eb75.put32(0x08u+0x20u+std::size_t(k)*4u,0x10u);require(embedded_configure_446f30(eb75,0x10u,1,es75)&&e75.fx==4u&&e75.fxv==0x40u,"r075 446f30 effect per slot");
    e75={};for(std::uint32_t k=0;k<4u;++k)eb75.put32(0x08u+0x20u+std::size_t(k)*4u,0x55u);embedded_configure_446f30(eb75,8u,-1,es75);require(e75.fx==0u,"r075 446f30 signed flag gate");

    std::array<PcNativeHandleBinding,4> regbuf75{};PcNativeHandleRegistry reg75{regbuf75.data(),regbuf75.size(),0u,0x75000010u};std::array<std::uint8_t,0x20> ro75{},ro275{};
    const auto rt75=native_handle_register(reg75,ro75.data(),ro75.size());const auto rt275=native_handle_register(reg75,ro275.data(),ro275.size(),0x75000080u);require(rt75==0x75000010u&&rt275==0x75000080u&&reg75.count==2u,"r075 registry register tokens");
    require(native_handle_find(native_handle_resolver(reg75),rt275)!=nullptr&&native_handle_unregister(reg75,rt75)&&reg75.count==1u&&!native_handle_unregister(reg75,rt75),"r075 registry resolve/unregister");
    }
    // r076 course runtime builder: shuffle, tree annotation, matrix finalization,
    // forced-selection setter and the 44D720 direct-record path.
    {
    std::array<std::uint8_t,3u*PcCourseRecord44d720Size> shraw{};Bytes sh76(shraw.data(),shraw.size());
    for(std::uint32_t k=0;k<3u;++k){auto r=sh76.sub(std::size_t(k)*PcCourseRecord44d720Size,PcCourseRecord44d720Size);r.put32(0x1cu,10u+k);r.put32(0x20u,20u+k);}
    struct Rng76{std::array<std::int32_t,2> v{{1,0}};std::size_t n{};} rng76;
    auto rand76=[](void* u)->std::int32_t{auto* r=static_cast<Rng76*>(u);return r->v[r->n++%r->v.size()];};
    runtime_course_shuffle_44bf30(sh76,3,&rng76,rand76);
    require(sh76.u32(0x1cu)==12u&&sh76.u32(PcCourseRecord44d720Size+0x1cu)==10u&&sh76.u32(2u*PcCourseRecord44d720Size+0x1cu)==11u,"r076 44bf30 pair shuffle");
    require(sh76.u32(0x20u)==22u&&sh76.u32(PcCourseRecord44d720Size+0x20u)==20u&&sh76.u32(2u*PcCourseRecord44d720Size+0x20u)==21u,"r076 44bf30 secondary shuffle");

    std::array<std::uint8_t,4u*PcCourseRecord44d720Size> traw{};Bytes tree76(traw.data(),traw.size());
    for(std::uint32_t k=0;k<4u;++k)tree76.sub(std::size_t(k)*PcCourseRecord44d720Size,PcCourseRecord44d720Size).put32(0x04u,(k+1u)*10u);
    auto t0=tree76.sub(0,PcCourseRecord44d720Size),t1=tree76.sub(PcCourseRecord44d720Size,PcCourseRecord44d720Size),t2=tree76.sub(2u*PcCourseRecord44d720Size,PcCourseRecord44d720Size),t3=tree76.sub(3u*PcCourseRecord44d720Size,PcCourseRecord44d720Size);
    t0.put32(0x2cu,20u);t0.put32(0x24u,1u);t0.put32(0x30u,30u);t0.put32(0x28u,2u);t1.put32(0x2cu,40u);t1.put32(0x24u,3u);t1.put32(0x30u,0xffffffffu);t2.put32(0x2cu,0xffffffffu);t2.put32(0x30u,0xffffffffu);t3.put32(0x2cu,0xffffffffu);t3.put32(0x30u,0xffffffffu);
    std::int32_t depth76=0;runtime_course_tree_44c850(tree76,4,0,0,0,depth76);
    require(t0.i32(0x08u)==0&&t0.u8(0x0cu)==0u&&t1.i32(0x08u)==1&&t1.u8(0x0cu)==0u&&t3.i32(0x08u)==2&&t3.u8(0x0cu)==0u,"r076 44c850 first branch depths");
    require(t2.i32(0x08u)==1&&t2.u8(0x0cu)==1u&&depth76==2,"r076 44c850 second branch/max depth");

    std::array<std::uint8_t,0xa0> descraw{};Bytes desc76(descraw.data(),descraw.size());desc76.put32(0,0x7600abcdu);desc76.putf(0x80u,1.0f);desc76.putf(0x84u,2.0f);desc76.putf(0x88u,3.0f);desc76.putf(0x8cu,0.0f);desc76.putf(0x90u,0.0f);desc76.putf(0x94u,0.0f);
    std::array<std::uint8_t,128> mraw76{};Bytes mb76(mraw76.data(),mraw76.size());for(unsigned k=0;k<16u;++k)mb76.put32(k*4u,k%5u==0u?0x3f800000u:0u);PcMatrixStack ms76{mb76,0,0,2};
    PcCourseRuntimeState44d720 rs76{};runtime_course_matrix_44c0d0({0x76001000u,descraw.data(),descraw.size()},ms76,rs76);
    Bytes mm76(rs76.matrix_7d3190.data(),rs76.matrix_7d3190.size());require(mm76.f32(0x30u)==1.0f&&mm76.f32(0x34u)==2.0f&&mm76.f32(0x38u)==3.0f&&ms76.depth==0&&ms76.current_offset==0,"r076 44c0d0 matrix translate/restore");
    runtime_course_force_mode_46c360(rs76,7u);require(rs76.force_mode_7f95a8==7u,"r076 46c360 setter");

    std::array<std::array<std::uint8_t,0xa0>,4> draw76{};std::array<PcCourseDescriptor44d720,4> pd76{};std::array<std::uint32_t,4> sd76{{0x76002000u,0x76002010u,0x76002020u,0x76002030u}};
    for(std::uint32_t k=0;k<4u;++k){Bytes d(draw76[k].data(),draw76[k].size());d.put32(0,k+0x100u);d.putf(0x80u,float(k+1u));d.putf(0x84u,float(k+2u));d.putf(0x88u,float(k+3u));pd76[k]={0x76001000u+k*0x10u,draw76[k].data(),draw76[k].size()};}
    std::array<std::uint8_t,4u*PcCourseRecord44d720Size> rraw76{};Bytes rb76(rraw76.data(),rraw76.size());
    for(std::uint32_t k=0;k<4u;++k){auto r=rb76.sub(std::size_t(k)*PcCourseRecord44d720Size,PcCourseRecord44d720Size);r.put32(0x04u,(k+1u)*10u);r.put32(0x1cu,k);r.put32(0x20u,k);r.put32(0x2cu,k<3u?(k+2u)*10u:0xffffffffu);r.put32(0x30u,0xffffffffu);}
    PcCourseApplyRequest44d720 rq76{};rq76.data_name="unit";rq76.category_name="unit_course";rq76.direct_records=rraw76.data();rq76.direct_bytes=rraw76.size();rq76.direct_count=4;
    std::array<std::uint8_t,128> pmraw76{};Bytes pmb76(pmraw76.data(),pmraw76.size());for(unsigned k=0;k<16u;++k)pmb76.put32(k*4u,k%5u==0u?0x3f800000u:0u);PcMatrixStack pms76{pmb76,0,0,2};PcCourseRuntimeState44d720 pst76{};
    require(runtime_apply_course_data_44d720(rq76,{pd76.data(),pd76.size(),sd76.data(),sd76.size()},pst76,pms76,{}),"r076 44d720 direct success");
    require(rb76.u32(0x00u)==0x100u&&rb76.u32(0x14u)==0x76001000u&&rb76.u32(0x18u)==0x76002000u&&rb76.i32(0x24u)==1,"r076 44d720 descriptor/link resolve");
    require(pst76.active_count_7d33c4==4&&pst76.max_depth_7d33c0==3&&pst76.selected_index==0&&pst76.selected_copy_active,"r076 44d720 selected tree state");
    Bytes snap76(pst76.selected_7d30a8.data(),pst76.selected_7d30a8.size());require(snap76.u32(0x04u)==10u&&snap76.i32(0x08u)==0,"r076 44d720 selected snapshot");
    rq76.force_selected=true;rq76.selected_key=30u;std::array<std::uint8_t,128> fmraw76{};Bytes fmb76(fmraw76.data(),fmraw76.size());for(unsigned k=0;k<16u;++k)fmb76.put32(k*4u,k%5u==0u?0x3f800000u:0u);PcMatrixStack fms76{fmb76,0,0,2};PcCourseRuntimeState44d720 fst76{};
    // Reinitialize raw link/index fields because the first parent call resolved them in place.
    for(std::uint32_t k=0;k<4u;++k){auto r=rb76.sub(std::size_t(k)*PcCourseRecord44d720Size,PcCourseRecord44d720Size);r.put32(0x1cu,k);r.put32(0x20u,k);r.put32(0x2cu,k<3u?(k+2u)*10u:0xffffffffu);r.put32(0x30u,0xffffffffu);}
    require(runtime_apply_course_data_44d720(rq76,{pd76.data(),pd76.size(),sd76.data(),sd76.size()},fst76,fms76,{}),"r076 44d720 forced success");
    require(fst76.selected_index==2&&fst76.active_count_7d33c4==1&&fst76.force_mode_7f95a8==1u,"r076 44d720 forced selection state");
    auto forced=rb76.sub(2u*PcCourseRecord44d720Size,PcCourseRecord44d720Size);require(forced.u32(0x10u)==99u&&forced.u32(0x24u)==0u&&forced.u32(0x28u)==0xffffffffu&&forced.u32(0x20u)==0u,"r076 44d720 forced leaf rewrite");
    rq76.selected_key=999u;for(std::uint32_t k=0;k<4u;++k){auto r=rb76.sub(std::size_t(k)*PcCourseRecord44d720Size,PcCourseRecord44d720Size);r.put32(0x10u,0u);r.put32(0x1cu,k);r.put32(0x20u,k);r.put32(0x2cu,k<3u?(k+2u)*10u:0xffffffffu);r.put32(0x30u,0xffffffffu);}
    std::array<std::uint8_t,128> nmraw76{};Bytes nmb76(nmraw76.data(),nmraw76.size());for(unsigned k=0;k<16u;++k)nmb76.put32(k*4u,k%5u==0u?0x3f800000u:0u);PcMatrixStack nms76{nmb76,0,0,2};PcCourseRuntimeState44d720 nst76{};
    require(runtime_apply_course_data_44d720(rq76,{pd76.data(),pd76.size(),sd76.data(),sd76.size()},nst76,nms76,{}),"r076 44d720 absent forced key success");
    require(nst76.selected_index==0&&nst76.active_count_7d33c4==1&&nst76.force_mode_7f95a8==1u&&nst76.max_depth_7d33c0==3,"r076 44d720 absent forced key keeps full tree");
    }

    // r077 relocatable category container, exact hash/accessors and the concrete
    // 44DA00 -> 44D720 provider bridge.
    {
    require(runtime_category_hash_4f1260("csc_data_cvt_course")==runtime_category_hash_4f1260("CSC_DATA_CVT_COURSE"),"r077 4f1260 case-insensitive hash");
    std::array<std::uint8_t,0x400> blob77{};Bytes bb77(blob77.data(),blob77.size());
    const char* cat77="csc_data_cvt_course";const auto h77=runtime_category_hash_4f1260(cat77);
    bb77.put32(0x00u,1u);bb77.put32(0x04u,0u); // one category, no relocation pairs
    bb77.put32(0x08u,h77);bb77.put32(0x0cu,3u);bb77.put32(0x10u,0x40u);
    bb77.put32(0x14u,0xffffffffu);bb77.put32(0x18u,0u);bb77.put32(0x1cu,0u); // sentinel
    Bytes cr77=bb77.sub(0x40u,3u*PcCourseRecord44d720Size);
    for(std::uint32_t k=0;k<3u;++k){auto r=cr77.sub(std::size_t(k)*PcCourseRecord44d720Size,PcCourseRecord44d720Size);r.put32(0x04u,(k+1u)*10u);r.put32(0x1cu,k);r.put32(0x20u,k);r.put32(0x2cu,k<2u?(k+2u)*10u:0xffffffffu);r.put32(0x30u,0xffffffffu);}
    PcRelocCategoryBlobR077 view77{};require(course_reloc_blob_open_r077(blob77.data(),0x40u+3u*PcCourseRecord44d720Size,view77),"r077 reloc blob open");
    require(runtime_category_count_4f1ba0(cat77,view77)==3&&runtime_category_records_4f1a90(cat77,view77)==blob77.data()+0x40u,"r077 4f1a90/4f1ba0 fast lookup");
    require(runtime_category_records_4f1a90("missing",view77)==nullptr&&runtime_category_count_4f1ba0("missing",view77)==0,"r077 category miss");
    auto bad77=blob77;Bytes badb77(bad77.data(),bad77.size());badb77.put32(0x04u,1u);badb77.put32(0x08u,0xfffffffeu);badb77.put32(0x0cu,0u);PcRelocCategoryBlobR077 badview77{};require(!course_reloc_blob_open_r077(bad77.data(),bad77.size(),badview77),"r077 reject bad relocation patch");

    struct Reader77{const std::uint8_t* src{};std::size_t size{};std::string path{};std::uint32_t mode{},required{};unsigned calls{};} rd77{blob77.data(),0x40u+3u*PcCourseRecord44d720Size};
    auto read77=[](void* u,const char* path,std::uint32_t mode,std::uint32_t required,void* dst,std::size_t cap,std::size_t& got)->bool{auto* r=static_cast<Reader77*>(u);++r->calls;r->path=path?path:"";r->mode=mode;r->required=required;if(r->size>cap)return false;std::memcpy(dst,r->src,r->size);got=r->size;return true;};
    std::array<std::uint8_t,0x800> store77{};PcCourseProviderR077 prov77{};prov77.storage=store77.data();prov77.capacity=store77.size();prov77.read_user=&rd77;prov77.read_file=read77;
    auto psvc77=course_provider_services_r077(prov77);
    PcCourseRuntimeState44d720 tmp77{};require(course_provider_load_binary_r077(&prov77,"\\Scripts\\bin\\csc_data_cvt.bin",0u,1u,tmp77),"r077 provider load");
    auto src77=course_provider_lookup_records_r077(&prov77,cat77);require(src77.records==store77.data()+0x40u&&src77.count==3&&src77.bytes==3u*PcCourseRecord44d720Size,"r077 provider record source");

    std::array<std::array<std::uint8_t,0xa0>,4> dmem77{};std::array<PcCourseDescriptor44d720,4> pd77{};std::array<std::uint32_t,4> sd77{{0x77002000u,0x77002010u,0x77002020u,0x77002030u}};
    for(std::uint32_t k=0;k<4u;++k){Bytes d(dmem77[k].data(),dmem77[k].size());d.put32(0,0x200u+k);d.putf(0x80u,float(k+1u));d.putf(0x84u,float(k+2u));d.putf(0x88u,float(k+3u));pd77[k]={0x77001000u+k*0x10u,dmem77[k].data(),dmem77[k].size()};}
    std::array<std::uint8_t,128> mx77{};Bytes mxb77(mx77.data(),mx77.size());for(unsigned k=0;k<16u;++k)mxb77.put32(k*4u,k%5u==0u?0x3f800000u:0u);PcMatrixStack ms77{mxb77,0,0,2};
    PcCourseRuntimeState44d720 state77{};PcCourseRuntimeBridgeR077 bridge77{};bridge77.tables={pd77.data(),pd77.size(),sd77.data(),sd77.size()};bridge77.state=&state77;bridge77.matrices=&ms77;bridge77.services=course_provider_services_r077(prov77);
    PcCourseLoadState44da00 load77{};rd77.calls=0;require(runtime_course_load_44da00(0u,0u,load77,runtime_course_load_services_r077(bridge77)),"r077 integrated course load");
    require(rd77.calls==1u&&rd77.path=="\\Scripts\\bin\\csc_data_cvt.bin"&&rd77.mode==0u&&rd77.required==1u,"r077 bridge path/mode/required");
    require(bridge77.loader.phase==3&&prov77.loaded&&state77.active_count_7d33c4==3&&state77.max_depth_7d33c0==2,"r077 bridge native runtime applied");
    Bytes rs77(store77.data()+0x40u,3u*PcCourseRecord44d720Size);require(rs77.u32(0x14u)==0x77001000u&&rs77.i32(0x24u)==1,"r077 bridge descriptor/link population");
    (void)psvc77;
    }

    // r078 portable descriptor pack and the concrete 0x444350 -> 0x44DA00 ->
    // 0x44D720 owner bridge. The fixture is synthetic: no commercial table or
    // csc_data bytes are embedded in the test suite.
    {
    constexpr std::size_t pcount78=4u,scount78=4u;
    std::array<std::uint8_t,24u+pcount78*32u+scount78*4u> packraw78{};
    Bytes packb78(packraw78.data(),packraw78.size());
    const char magic78[8]={'O','R','C','7','8','T','B','L'};
    for(std::size_t i=0;i<8u;++i)packb78.put8(i,std::uint8_t(magic78[i]));
    packb78.put32(8u,1u);packb78.put32(12u,pcount78);packb78.put32(16u,scount78);packb78.put32(20u,0u);
    for(std::uint32_t i=0;i<pcount78;++i){
        const auto off=24u+std::size_t(i)*32u;
        packb78.put32(off+0u,0x78001000u+i*0x10u); // opaque primary token
        packb78.put32(off+4u,0x300u+i);             // descriptor +0
        packb78.putf(off+8u,float(i+1u));packb78.putf(off+12u,float(i+2u));packb78.putf(off+16u,float(i+3u));
        packb78.putf(off+20u,0.0f);packb78.putf(off+24u,0.0f);packb78.putf(off+28u,0.0f);
    }
    const auto sec78=24u+pcount78*32u;
    for(std::uint32_t i=0;i<scount78;++i)packb78.put32(sec78+std::size_t(i)*4u,0x78002000u+i*0x10u);
    PcCourseDescriptorPackR078 pack78{};
    require(course_descriptor_pack_open_r078(packraw78.data(),packraw78.size(),pack78),"r078 descriptor pack open");
    require(pack78.primary_count==pcount78&&pack78.secondary_count==scount78,"r078 descriptor pack counts");
    require(pack78.primary[2].token==0x78001020u&&pack78.secondary_tokens[3]==0x78002030u,"r078 descriptor pack tokens");
    Bytes pdm78(pack78.primary[2].data,pack78.primary[2].size);
    require(pdm78.u32(0)==0x302u&&pdm78.f32(0x80u)==3.0f&&pdm78.f32(0x84u)==4.0f&&pdm78.f32(0x88u)==5.0f,"r078 descriptor pack expanded fields");
    auto badpack78=packraw78;badpack78[0]^=1u;PcCourseDescriptorPackR078 badparsed78{};
    require(!course_descriptor_pack_open_r078(badpack78.data(),badpack78.size(),badparsed78),"r078 descriptor pack bad magic reject");

    // One-category csc blob matching the real 4F12A0/4F1A90 serialized shape.
    std::array<std::uint8_t,0x400> blob78{};Bytes bb78(blob78.data(),blob78.size());
    const char* cat78="csc_data_cvt_course";const auto h78=runtime_category_hash_4f1260(cat78);
    bb78.put32(0x00u,1u);bb78.put32(0x04u,0u);bb78.put32(0x08u,h78);bb78.put32(0x0cu,3u);bb78.put32(0x10u,0x40u);
    bb78.put32(0x14u,0xffffffffu);bb78.put32(0x18u,0u);bb78.put32(0x1cu,0u);
    Bytes cr78=bb78.sub(0x40u,3u*PcCourseRecord44d720Size);
    for(std::uint32_t i=0;i<3u;++i){auto r=cr78.sub(std::size_t(i)*PcCourseRecord44d720Size,PcCourseRecord44d720Size);r.put32(0x04u,(i+1u)*10u);r.put32(0x1cu,i);r.put32(0x20u,i);r.put32(0x2cu,i<2u?(i+2u)*10u:0xffffffffu);r.put32(0x30u,0xffffffffu);}
    struct Reader78{const std::uint8_t* src{};std::size_t size{};std::string path{};std::uint32_t mode{},required{};unsigned calls{};} rd78{blob78.data(),0x40u+3u*PcCourseRecord44d720Size};
    auto read78=[](void* u,const char* path,std::uint32_t mode,std::uint32_t required,void* dst,std::size_t cap,std::size_t& got)->bool{auto* r=static_cast<Reader78*>(u);++r->calls;r->path=path?path:"";r->mode=mode;r->required=required;if(r->size>cap)return false;std::memcpy(dst,r->src,r->size);got=r->size;return true;};
    std::array<std::uint8_t,0x800> store78{};PcCourseProviderR077 provider78{};provider78.storage=store78.data();provider78.capacity=store78.size();provider78.read_user=&rd78;provider78.read_file=read78;
    std::array<std::uint8_t,128> mx78{};Bytes mxb78(mx78.data(),mx78.size());for(unsigned i=0;i<16u;++i)mxb78.put32(i*4u,i%5u==0u?0x3f800000u:0u);PcMatrixStack matrices78{mxb78,0,0,2};
    PcCourseRuntimeState44d720 course_state78{};PcCourseRuntimeBridgeR077 course_bridge78{};course_bridge78.tables=course_descriptor_tables_r078(pack78);course_bridge78.state=&course_state78;course_bridge78.matrices=&matrices78;course_bridge78.services=course_provider_services_r077(provider78);
    PcCourseLoadState44da00 course_load78{};PcRuntimePrepareState prepare78{};
    PcRuntimeCommitBridgeR078 commit_bridge78{};commit_bridge78.prepare_state=&prepare78;commit_bridge78.course_load_state=&course_load78;commit_bridge78.course_services=runtime_course_load_services_r077(course_bridge78);
    std::array<std::uint8_t,0x600> objraw78{};Bytes obj78(objraw78.data(),objraw78.size());obj78.put32(0x04u,3u);obj78.put32(0x020cu,0u);obj78.put32(0x0210u,0u);obj78.puti(0x0484u,0);
    PcObjectRuntimeSnapshot444350 snap78{};
    object_runtime_commit_444350(obj78,snap78,{}, {},runtime_commit_services_r078(commit_bridge78));
    require(rd78.calls==1u&&rd78.path=="\\Scripts\\bin\\csc_data_cvt.bin"&&rd78.mode==0u&&rd78.required==1u,"r078 owner course path/mode/required");
    require(course_load78.force_sync_7d2d8c==1u&&course_bridge78.loader.phase==3&&provider78.loaded,"r078 owner course loader state");
    require(course_state78.active_count_7d33c4==3&&course_state78.max_depth_7d33c0==2&&course_state78.selected_index==0,"r078 owner course tree state");
    require(prepare78.selection_active_836374==0u&&obj78.u32(0x021cu)==0x0du,"r078 owner prepare/selection/commit state");
    Bytes applied78(store78.data()+0x40u,3u*PcCourseRecord44d720Size);require(applied78.u32(0x14u)==0x78001000u&&applied78.u32(0x18u)==0x78002000u,"r078 owner descriptor resolution");
    }
    // r079 parent above 0x444350 plus its two compact result dispatchers.
    {
    struct Action79{unsigned calls{};std::uint32_t pc{};std::int32_t selected{};std::uint32_t variant{};} a79;
    auto action79=[](void* u,std::uint32_t pc,Bytes,std::int32_t selected,std::uint32_t variant){auto* a=static_cast<Action79*>(u);++a->calls;a->pc=pc;a->selected=selected;a->variant=variant;};
    PcRuntimeTransitionServicesR079 ats79{&a79,action79};
    std::array<std::uint8_t,0x1000> o79raw{};Bytes o79(o79raw.data(),o79raw.size());
    struct DCase79{std::uint32_t code,pc,variant;bool result;};
    const DCase79 primary79[]={{0,0,0,false},{1,0x4450a0u,0,false},{2,0x445160u,0,false},{3,0x4448c0u,0,false},{4,0x4442a0u,0,false},{5,0,0,true},{6,0x445310u,0,false},{7,0,0,false}};
    for(const auto& c:primary79){a79={};const auto r=object_runtime_dispatch_primary_445430(o79,c.code,-3,ats79);require(r==c.result,"r079 primary dispatcher result");require(a79.calls==(c.pc?1u:0u),"r079 primary dispatcher call count");if(c.pc){require(a79.pc==c.pc&&a79.selected==-3&&a79.variant==c.variant,"r079 primary dispatcher payload");}}
    const DCase79 secondary79[]={{0,0,0,false},{1,0x445160u,1,false},{2,0x445160u,1,false},{3,0x445160u,1,false},{4,0x4442a0u,1,false},{5,0,0,true},{6,0,0,false}};
    for(const auto& c:secondary79){a79={};const auto r=object_runtime_dispatch_secondary_4454b0(o79,c.code,2,ats79);require(r==c.result,"r079 secondary dispatcher result");require(a79.calls==(c.pc?1u:0u),"r079 secondary dispatcher call count");if(c.pc)require(a79.pc==c.pc&&a79.selected==2&&a79.variant==c.variant,"r079 secondary dispatcher payload");}

    struct Owner79{std::array<std::uint32_t,16> pc{};std::array<std::size_t,16> off{};unsigned n{};std::uint32_t selector_ret{};unsigned manager_calls{};} tr79;
    auto this79=[](void* u,std::uint32_t pc,Bytes object,std::size_t off){auto* t=static_cast<Owner79*>(u);if(t->n<t->pc.size()){t->pc[t->n]=pc;t->off[t->n]=off;++t->n;}if(pc==0x445500u)object.put32(0,0x0eu);};
    auto virt79=[](void* u,std::uint32_t,std::uint32_t)->std::uint32_t{return static_cast<Owner79*>(u)->selector_ret;};
    auto mgr79=[](void* u,std::uint32_t pc,std::uint32_t){auto* t=static_cast<Owner79*>(u);if(pc==0x4ee930u)++t->manager_calls;};
    PcObjectSelectorServices sel79{&tr79,virt79,mgr79};
    PcRuntimeOwnerServices445be0 os79{};os79.user=&tr79;os79.call_this=this79;os79.selector_services=sel79;os79.transition_services=ats79;
    // Commit/release services can stay empty when the fixture has no handles.
    PcObjectRuntimeSnapshot444350 snap79{};PcObjectSelectorGlobals sg79{};
    const PcRuntimeOwnerInputs445be0 in79{8.0f,0.25f};

    o79raw.fill(0);o79.putf(0x0d9cu,1.25f);o79.put32(0x218u,0u);tr79={};
    require(object_runtime_owner_445be0(o79,snap79,sg79,in79,os79)==1u,"r079 owner state0 result");
    require(o79.f32(0x0d98u)==1.25f&&o79.f32(0x0d9cu)==2.0f&&o79.f32(0x0da0u)==0.75f,"r079 owner timer bookkeeping");

    o79raw.fill(0);o79.put32(0x218u,1u);tr79={};require(object_runtime_owner_445be0(o79,snap79,sg79,in79,os79)==1u,"r079 owner state1 result");
    require(tr79.n==1u&&tr79.pc[0]==0x445500u&&o79.u32(0x218u)==2u,"r079 owner state1 transition");

    o79raw.fill(0);o79.put32(0x218u,2u);o79.put32(0x04u,0u);o79.puti(0x484u,0);o79.put8(0x494u,0u);o79.put8(0x48cu,0u);tr79={};sg79={0x12345678u,1u,1u};
    require(object_runtime_owner_445be0(o79,snap79,sg79,in79,os79)==0u,"r079 owner state2 commit result");
    const std::uint32_t exp79[]={0x441020u,0x443110u,0x444840u,0x446cf0u,0x445810u,0x444530u,0x445a50u,0x4411a0u};
    require(tr79.n==8u,"r079 owner state2 child count");for(unsigned i=0;i<8u;++i)require(tr79.pc[i]==exp79[i],"r079 owner state2 child order");
    require(tr79.off[3]==0x51cu&&tr79.manager_calls==1u&&sg79.manager_flag8==0u,"r079 owner state2 selector/embedded offset");
    require(o79.u32(0x21cu)==0x0du,"r079 owner state2 reached 444350");

    o79raw.fill(0);o79.put32(0x218u,3u);o79.puti(0x518u,1);o79.put32(0x498u,0x100u);o79.put8(0x494u,0u);o79.put8(0x48cu,0u);tr79={};tr79.selector_ret=5u;sg79={};
    require(object_runtime_owner_445be0(o79,snap79,sg79,in79,os79)==0u,"r079 owner state3 terminal result");
    require(tr79.n==3u&&tr79.pc[0]==0x444470u&&tr79.pc[1]==0x444650u&&tr79.pc[2]==0x446cf0u&&tr79.off[2]==0x51cu,"r079 owner state3 child order");

    o79raw.fill(0);o79.put32(0x218u,4u);o79.put8(0x220u,7u);o79.put32(0x214u,0xdeadbeefu);tr79={};
    require(object_runtime_owner_445be0(o79,snap79,sg79,in79,os79)==1u,"r079 owner state4 result");
    require(o79.u32(0x214u)==0u&&o79.u8(0x228u)==1u&&o79.u8(0x248u)==0u&&o79.u32(0x218u)==3u,"r079 owner state4 history transition");
    }
    // r106 exact stage-3 slice of 445500 and complete 48BF20 child.  The two
    // 448AD0 resource-table calls stay observable boundaries in original order.
    {
    std::array<std::uint8_t,0x600> object106raw{};Bytes object106(object106raw.data(),object106raw.size());
    std::array<std::uint8_t,PcRuntimeLoaderState48bf20Size> loader106raw{};loader106raw.fill(0xa5u);Bytes loader106(loader106raw.data(),loader106raw.size());
    struct Loader106{std::vector<std::array<std::uint32_t,3>> requests;} trace106;
    auto request106=[](void* u,std::uint32_t pc,std::uint32_t id,std::uint32_t mode){static_cast<Loader106*>(u)->requests.push_back({pc,id,mode});};
    const PcRuntimeLoaderServices48bf20 services106{&trace106,request106};
    object106.put32(0u,3u);object106.puti(0x518u,0);
    require(object_runtime_loader_stage3_4455df(object106,loader106,services106),"r106 loader stage3 slice accepted");
    require(object106.u32(0u)==4u,"r106 loader stage3 advances to stage4");
    require(trace106.requests.size()==2u&&trace106.requests[0]==std::array<std::uint32_t,3>{0x448ad0u,0xbau,2u}&&trace106.requests[1]==std::array<std::uint32_t,3>{0x448ad0u,0xbbu,2u},"r106 loader resource request order");
    require(loader106.u8(0u)==1u&&loader106.u32(4u)==30u&&loader106.u32(8u)==30u&&loader106.u32(0x0cu)==30u,"r106 loader leading state");
    for(const auto off:{0x10u,0x1cu,0x28u})require(loader106.u32(off)==11u,"r106 loader resource kind triplet");
    for(const auto off:{0x14u,0x20u,0x2cu})require(loader106.u32(off)==30u,"r106 loader timeout triplet");
    for(const auto off:{0x18u,0x24u,0x30u})require(loader106.u32(off)==0u,"r106 loader cleared triplet");
    require(loader106.u8(1u)==0xa5u&&loader106.u8(2u)==0xa5u&&loader106.u8(3u)==0xa5u,"r106 loader byte write preserves padding");
    trace106.requests.clear();object106.put32(0u,4u);
    require(!object_runtime_loader_stage3_4455df(object106,loader106,services106)&&trace106.requests.empty(),"r106 loader rejects other stage");
    object106.put32(0u,3u);object106.puti(0x518u,1);
    require(!object_runtime_loader_stage3_4455df(object106,loader106,services106)&&object106.u32(0u)==3u,"r106 loader defers active handle");
    }
    // r107 closes the BA/BB request mutation, readiness predicate, top-loader
    // stage 4 and the complete seven-state 49E580 child in one integration lot.
    {
    PcRuntimeResourceEntry448ad0 entry107{0u,7u,0u};std::uint32_t pending107{};
    require(runtime_resource_request_448ad0(entry107,0xbau,2u,pending107),"r107 448AD0 accepts ready entry");
    require(pending107==1u&&entry107.status_14==0u&&entry107.request_mode_30==2u,"r107 448AD0 exact mutation");
    require(!runtime_resource_request_448ad0(entry107,0xbau,9u,pending107),"r107 448AD0 rejects non-ready entry");
    entry107={1u,7u,0u};require(!runtime_resource_request_448ad0(entry107,0xbau,2u,pending107),"r107 448AD0 rejects ownership one");
    entry107={0u,7u,0u};require(!runtime_resource_request_448ad0(entry107,0x223u,2u,pending107),"r107 448AD0 rejects out-of-range id");
    require(!runtime_resource_ready_448980(1u)&&runtime_resource_ready_448980(0u),"r107 448980 pending predicate");

    std::array<std::uint8_t,0x600> top107raw{};Bytes top107(top107raw.data(),top107raw.size());top107.put32(0u,4u);top107.puti(0x518u,0);
    require(!object_runtime_loader_stage4_4455f2(top107,1u)&&top107.u32(0u)==4u,"r107 top loader waits for resources");
    require(object_runtime_loader_stage4_4455f2(top107,0u)&&top107.u32(0u)==5u,"r107 top loader advances stage4 to5");

    struct Shared107{bool group_ready{};std::vector<std::array<std::uint32_t,3>> calls;} shared107;
    auto call107=[](void* u,std::uint32_t pc,const std::uint32_t* args,std::size_t count)->std::uint32_t{auto* t=static_cast<Shared107*>(u);t->calls.push_back({pc,count?args[0]:0u,count>1u?args[1]:0u});if(pc==0x42df90u)return t->group_ready?1u:0u;if(pc==0x4299a0u)return 1u;return 0u;};
    PcRuntimeSharedLoaderState49e580 shared_state107{};const PcRuntimeSharedLoaderServices49e580 shared_services107{&shared107,call107};
    require(runtime_shared_loader_49e580(shared_state107,shared_services107)==0u&&shared_state107.stage_83db18==3u,"r107 shared loader reaches async stage3");
    require(shared107.calls.size()==8u&&shared107.calls[0][0]==0x4f2470u&&shared107.calls[4]==std::array<std::uint32_t,3>{0x42deb0u,0x2cu,8u}&&shared107.calls[6][1]==0x48u&&shared107.calls[7][0]==0x42df90u,"r107 shared loader initial call order");
    shared107.group_ready=true;shared107.calls.clear();require(runtime_shared_loader_49e580(shared_state107,shared_services107)==1u&&shared_state107.stage_83db18==6u,"r107 shared loader completes after readiness");
    const std::uint32_t finish_order107[]={0x42df90u,0x429920u,0x429920u,0x429920u,0x448ab0u,0x4299a0u,0x448b90u,0x40ed70u};require(shared107.calls.size()==8u,"r107 shared loader completion call count");for(unsigned i=0;i<8u;++i)require(shared107.calls[i][0]==finish_order107[i],"r107 shared loader completion order");
    }
    // r110 continues 445500 from stage 6 through the real 0x44/9 resource,
    // exact 0xB0*0x80 select-table fread, and the honest 4E8620 boundary.
    {
    struct Top110{bool group_ready{},async_ready{true},bulk_ready{},base_ready{true};std::vector<std::array<std::uint32_t,6>> calls;} top110;
    auto call110=[](void* u,std::uint32_t pc,const std::uint32_t* args,std::size_t count)->std::uint32_t{
        auto* t=static_cast<Top110*>(u);std::array<std::uint32_t,6> q{};q[0]=pc;q[1]=static_cast<std::uint32_t>(count);for(std::size_t i=0;i<count&&i<4u;++i)q[i+2u]=args[i];t->calls.push_back(q);
        if(pc==0x42df90u)return t->group_ready?1u:0u;
        if(pc==0x4239c0u)return 0x71000001u;
        if(pc==0x4299a0u)return 1u;
        if(pc==0x496180u)return t->async_ready?1u:0u;
        if(pc==0x4e8620u)return t->bulk_ready?1u:0u;
        if(pc==0x448980u)return t->base_ready?1u:0u;
        return 0u;
    };
    std::array<std::uint8_t,0x600> top110raw{};Bytes object110(top110raw.data(),top110raw.size());object110.put32(0u,6u);const PcRuntimeTopLoaderServicesR110 services110{&top110,call110};
    require(!object_runtime_loader_stages6_to11_445627(object110,services110)&&object110.u32(0u)==6u,"r110 stage6 waits for 44/9 group");
    require(top110.calls.size()==1u&&top110.calls[0][0]==0x42df90u,"r110 stage6 wait is isolated");
    top110.group_ready=true;top110.calls.clear();require(!object_runtime_loader_stages6_to11_445627(object110,services110)&&object110.u32(0u)==7u,"r110 stage6 completes then stops at bulk loader");
    const std::uint32_t order110a[]={0x42df90u,0x4239c0u,0x423cb0u,0x423bd0u,0x427700u,0x496170u,0x4e85e0u,0x429920u,0x4299a0u,0x496180u,0x4e8620u};require(top110.calls.size()==11u,"r110 stage6/7 call count");for(unsigned i=0;i<11u;++i)require(top110.calls[i][0]==order110a[i],"r110 stage6/7 exact call order");
    require(top110.calls[1][2]==0x59daecu&&top110.calls[1][3]==0x62563cu,"r110 target fopen tokens");
    require(top110.calls[2][2]==0x844a08u&&top110.calls[2][3]==0xb0u&&top110.calls[2][4]==0x80u&&top110.calls[2][5]==0x71000001u,"r110 target fread arguments");
    require(top110.calls[7][2]==0x44u&&top110.calls[7][3]==9u,"r110 frontend resource release arguments");
    top110.bulk_ready=true;top110.calls.clear();require(object_runtime_loader_stages6_to11_445627(object110,services110)&&object110.u32(0u)==12u,"r110 ready stages fall through to stage12");
    const std::uint32_t order110b[]={0x4299a0u,0x496180u,0x4e8620u,0x448980u};require(top110.calls.size()==4u,"r110 stage7/8 ready call count");for(unsigned i=0;i<4u;++i)require(top110.calls[i][0]==order110b[i],"r110 stage7/8 ready call order");
    }
    // r111 closes the full 4E85E0/4E8620 group: four fixed request files,
    // 60 formatted circuit files, exact early return and retained spare lanes.
    {
    struct Bulk111{bool loads_ready{};std::vector<std::array<std::uint32_t,6>> calls;} bulk111;
    auto call111=[](void* u,std::uint32_t pc,const std::uint32_t* args,std::size_t count)->std::uint32_t{
        auto* t=static_cast<Bulk111*>(u);std::array<std::uint32_t,6> q{};q[0]=pc;q[1]=static_cast<std::uint32_t>(count);for(std::size_t i=0;i<count&&i<4u;++i)q[i+2u]=args[i];t->calls.push_back(q);
        if(pc==0x4f12a0u)return t->loads_ready?1u:0u;
        if(pc==0x4f1a90u)return 0x73000000u+static_cast<std::uint32_t>(t->calls.size());
        return 0u;
    };
    PcFrontendBulkLoaderState4e8620 state111{};frontend_bulk_loader_initialize_4e85e0(state111);
    require(state111.special_status[0]==1u&&state111.special_status[3]==1u&&state111.script_status[0]==1u&&state111.script_status[61]==1u,"r111 frontend initializer covers 4+62 PC statuses");
    const PcFrontendBulkLoaderServicesR111 services111{&bulk111,call111};
    require(!frontend_bulk_loader_4e8620(state111,services111),"r111 frontend bulk loader preserves first pending return");
    require(bulk111.calls.size()==1u&&bulk111.calls[0][0]==0x4f12a0u&&bulk111.calls[0][2]==0x5ce138u&&bulk111.calls[0][3]==0x84b3c0u,"r111 first special request tokens");
    bulk111.loads_ready=true;bulk111.calls.clear();require(frontend_bulk_loader_4e8620(state111,services111),"r111 frontend bulk loader completes 64 owned lanes");
    require(bulk111.calls.size()==128u,"r111 bulk loader 4 load/bind plus 60 format/load pairs");
    require(bulk111.calls[0][0]==0x4f12a0u&&bulk111.calls[1][0]==0x4f1a90u&&bulk111.calls[7][0]==0x4f1a90u,"r111 special load/bind ordering");
    require(bulk111.calls[8][0]==0x5802ddu&&bulk111.calls[8][2]==0x84b400u&&bulk111.calls[8][3]==0x5a4310u&&bulk111.calls[8][4]==0x5b4a14u,"r111 first formatted script tokens");
    require(bulk111.calls[127][0]==0x4f12a0u&&bulk111.calls[127][2]==0x5b460cu&&bulk111.calls[127][3]==0x84b7b0u,"r111 final reverse-circuit request tokens");
    bool ready111=true;for(auto v:state111.special_status)ready111=ready111&&v==3u;for(std::size_t i=0;i<60u;++i)ready111=ready111&&state111.script_status[i]==3u;
    require(ready111&&state111.script_status[60]==1u&&state111.script_status[61]==1u,"r111 exact 64 ready lanes retain two PC spare statuses");
    }
    // r112 terminal top-loader body: optional resource gate, embedded reset,
    // callback insertion, transition init and exact stage 12 -> 13 -> 14 path.
    {
    struct Stage112Trace{std::vector<std::uint32_t> global,this_pc;std::uint32_t status{3u},handle{0x71200001u},ready{1u};} t112;
    auto global112=[](void* u,std::uint32_t pc,const std::uint32_t*,std::size_t)->std::uint32_t{auto* t=static_cast<Stage112Trace*>(u);t->global.push_back(pc);return pc==0x428880u?t->status:0u;};
    auto this112=[](void* u,std::uint32_t pc,Bytes,std::size_t,const std::uint32_t*,std::size_t)->std::uint32_t{auto* t=static_cast<Stage112Trace*>(u);t->this_pc.push_back(pc);return pc==0x443eb0u?t->handle:0u;};
    auto virtual112=[](void* u,std::uint32_t h,std::uint32_t slot)->std::uint32_t{auto* t=static_cast<Stage112Trace*>(u);return h==t->handle&&slot==4u?t->ready:0u;};
    const PcRuntimeTopLoaderStage12ServicesR112 sv112{&t112,global112,this112,virtual112};
    std::array<std::uint8_t,0x1000> raw112{};Bytes o112(raw112.data(),raw112.size());o112.put32(0u,12u);
    PcRuntimeTopLoaderStage12StateR112 s112{};s112.mode_6319a1=1u;s112.optional_resource_631b38=0x11223344u;s112.optional_index_7b17f8=7;s112.optional_flags_7c27d4=2u;
    t112.status=2u;require(!object_runtime_loader_stage12_4456ea(o112,s112,sv112)&&o112.u32(0u)==12u&&t112.global.size()==1u&&t112.global[0]==0x428880u,"r112 optional resource status blocks stage12");
    t112.status=3u;t112.global.clear();require(object_runtime_loader_stage12_4456ea(o112,s112,sv112),"r112 stage12 terminal body completes");
    require(o112.u32(0u)==14u&&o112.u32(0x484u)==1u&&o112.u32(0x284u)==t112.handle&&s112.mode_6319a1==0u,"r112 stage14 and callback list mutation");
    require(o112.f32(0x264u)==1.0f&&o112.f32(0x278u)==1.0f&&s112.transition_globals.primary==1.0f&&s112.transition_globals.secondary==1.0f,"r112 native transition initializer");
    require(t112.this_pc.size()==3u&&t112.this_pc[0]==0x4470f0u&&t112.this_pc[1]==0x447000u&&t112.this_pc[2]==0x443eb0u,"r112 embedded and callback dispatch order");
    require(t112.global.size()==6u&&t112.global[0]==0x428880u&&t112.global[1]==0x4285a0u&&t112.global[2]==0x4999f0u&&t112.global[3]==0x48c370u&&t112.global[4]==0x4edce0u&&t112.global[5]==0x416420u,"r112 global call order including optional tail");
    raw112.fill(0u);o112.put32(0u,12u);t112=Stage112Trace{};s112=PcRuntimeTopLoaderStage12StateR112{};s112.alternate_7b17ec=1u;require(object_runtime_loader_stage12_4456ea(o112,s112,sv112)&&o112.u32(0x484u)==0u&&t112.this_pc.size()==3u&&t112.this_pc[2]==0x444e10u,"r112 alternate branch skips callback insertion");
    bool audio112=false;for(auto pc:t112.global)audio112=audio112||pc==0x401000u;require(audio112,"r112 alternate branch audio request");
    raw112.fill(0u);o112.put32(0u,12u);t112=Stage112Trace{};t112.ready=0u;s112=PcRuntimeTopLoaderStage12StateR112{};require(!object_runtime_loader_stage12_4456ea(o112,s112,sv112)&&o112.u32(0u)==12u&&t112.this_pc.size()==5u&&t112.this_pc[3]==0x441200u&&t112.this_pc[4]==0x443c30u,"r112 rejected callback release and teardown tail");
    }
    // r080 first mandatory state-2 child: embedded UI resource tick 4659F0 and
    // parent 444840. Larger UI bodies remain explicit callbacks.
    {
    struct Ui80 { std::array<std::uint32_t,16> pc{}; unsigned n{}; int ready_status{}; unsigned frees{}; std::uint32_t freed{}; std::uint32_t open_key{}; } t80;
    auto hv80=[](void* u,std::uint32_t pc,std::uint32_t h){auto* t=static_cast<Ui80*>(u);if(pc==0x4285a0u){++t->frees;t->freed=h;}};
    auto hi80=[](void* u,std::uint32_t pc,std::uint32_t)->std::int32_t{auto* t=static_cast<Ui80*>(u);return pc==0x428880u?t->ready_status:0;};
    PcUiNotifyServices ui80{&t80,hv80,hi80,nullptr,nullptr,nullptr};
    auto tick80=[](void* u,std::uint32_t pc,Bytes){auto* t=static_cast<Ui80*>(u);if(t->n<t->pc.size())t->pc[t->n++]=pc;};
    PcUiResourceTickServices4659f0 ts80{&t80,tick80};
    std::array<std::uint8_t,0x100> rr80{};Bytes r80(rr80.data(),rr80.size());

    r80.put32(0x08u,0xffffffffu);t80={};ui_resource_tick_4659f0(r80,ui80,ts80);require(t80.n==0u,"r080 tick invalid-handle early return");

    rr80.fill(0);r80.put32(0x08u,0x80000011u);r80.put32(0x18u,1u);r80.put32(0x1cu,1u);r80.put32(0x24u,0u);r80.put32(0x2cu,1u);
    r80.put32(0x30u,0x80300030u);r80.put32(0x34u,0x80340034u);r80.put32(0x38u,0x80380038u);t80={};t80.ready_status=0;ui_resource_tick_4659f0(r80,ui80,ts80);
    require(t80.frees==1u&&t80.freed==0x80000011u&&r80.u32(0x08u)==0xffffffffu,"r080 tick reset/free");
    require(r80.u32(0x10u)==0x80340034u&&r80.u32(0x0cu)==0x80300030u&&r80.u32(0x3cu)==0x80380038u&&r80.u32(0x18u)==0u,"r080 tick transition copies");
    require(t80.n==1u&&t80.pc[0]==0x465970u,"r080 tick transition finalizer");

    auto check_flags80=[&](std::uint32_t f9c,std::uint32_t f88,std::uint32_t f8c,std::initializer_list<std::uint32_t> expect){
        rr80.fill(0);r80.put32(0x08u,0x80000022u);r80.put32(0x20u,0u);r80.put32(0x24u,1u);r80.put32(0x9cu,f9c);r80.put32(0x88u,f88);r80.put32(0x8cu,f8c);t80={};
        ui_resource_tick_4659f0(r80,ui80,ts80);require(t80.n==expect.size(),"r080 downstream call count");unsigned k=0;for(auto pc:expect)require(t80.pc[k++]==pc,"r080 downstream call order");
    };
    check_flags80(1u,0u,0u,{0x4656b0u});
    check_flags80(0u,1u,0u,{0x465590u,0x4656b0u});
    check_flags80(0u,0u,1u,{0x465460u,0x4656b0u});
    check_flags80(1u,1u,1u,{0x465590u,0x465460u,0x4656b0u});
    rr80.fill(0);r80.put32(0x08u,0x80000033u);r80.put32(0x20u,1u);r80.put32(0x24u,1u);r80.put32(0x88u,1u);t80={};ui_resource_tick_4659f0(r80,ui80,ts80);require(t80.n==0u,"r080 +20 downstream gate");

    std::array<std::uint8_t,0x1000> oraw80{};Bytes o80(oraw80.data(),oraw80.size());
    std::array<std::uint8_t,0x20> hraw80{};Bytes h80(hraw80.data(),hraw80.size());h80.put32(0x08u,0x2cu);
    const PcNativeHandleBinding hb80{0x80808080u,hraw80.data(),hraw80.size()};const PcNativeHandleResolver hr80{&hb80,1u};
    auto open80=[](void* u,std::uint32_t pc,Bytes,std::uint32_t key){auto* t=static_cast<Ui80*>(u);require(pc==0x4447d0u,"r080 parent open pc");t->open_key=key;++t->n;};
    PcRuntimeUiTickServices444840 os80{&t80,open80};
    // Tick is unconditional, but conditional open is suppressed while resource +8 != -1.
    o80.put32(0xcfcu,0x80000044u);o80.put32(0xd18u,1u);o80.put8(0xd94u,1u);t80={};object_runtime_ui_tick_444840(o80,hr80,ui80,ts80,os80);require(t80.n==0u,"r080 parent non-minus-one gate");
    // Invalid resource handle makes 4659F0 return immediately; the parent then
    // resolves the indexed object handle and forwards 564C90's +8 state.
    oraw80.fill(0);o80.put32(0xcfcu,0xffffffffu);o80.put8(0xd94u,1u);o80.put32(0x484u,2u);o80.put32(0x280u+2u*4u,0x80808080u);t80={};
    object_runtime_ui_tick_444840(o80,hr80,ui80,ts80,os80);require(t80.n==1u&&t80.open_key==0x2cu,"r080 parent indexed handle/state forward");
    o80.put8(0xd94u,0u);t80={};object_runtime_ui_tick_444840(o80,hr80,ui80,ts80,os80);require(t80.n==0u,"r080 parent active-byte gate");
    }
    // r114 closes the ordinary 465860 configure and 465970 commit parents,
    // then executes the valid state-code path through partial parent 4447D0.
    {
    struct Ui114 {
        unsigned releases{},finalizes{},invalid{};
        std::uint32_t released{},create_result{0x74110001u};
        std::vector<std::uint32_t> pc;
        std::vector<std::vector<std::uint32_t>> args;
    } t114;
    auto release114=[](void* u,std::uint32_t pc,std::uint32_t h){auto* t=static_cast<Ui114*>(u);require(pc==0x4285a0u,"r114 release pc");++t->releases;t->released=h;};
    auto call114=[](void* u,std::uint32_t pc,const std::uint32_t* a,std::size_t n)->std::uint32_t{auto* t=static_cast<Ui114*>(u);t->pc.push_back(pc);t->args.emplace_back(a,a+n);return pc==0x428320u||pc==0x428460u?t->create_result:0u;};
    auto final114=[](void* u,std::uint32_t pc,Bytes){auto* t=static_cast<Ui114*>(u);require(pc==0x4656b0u,"r114 finalize boundary pc");++t->finalizes;};
    auto invalid114=[](void* u,std::uint32_t pc,Bytes,std::uint32_t){auto* t=static_cast<Ui114*>(u);require(pc==0x1039cc4u,"r114 protected invalid boundary cell");++t->invalid;};
    PcUiNotifyServices ui114{};ui114.user=&t114;ui114.handle_void=release114;
    const PcUiResourceCommitServices465970 commit114{&t114,call114,final114};
    std::array<std::uint8_t,0xa0> raw114{};Bytes r114(raw114.data(),raw114.size());
    r114.put32(8u,0x1140aaaau);r114.put32(0x24u,0u);
    const std::array<std::uint32_t,11> a114{{0x11400000u,0x11400001u,0x11400002u,0x11400003u,3u,0x3f100005u,0x3f200006u,0x3f300007u,0x3f400008u,0x3f500009u,0u}};
    ui_resource_configure_465860(r114,a114,ui114);
    require(t114.releases==1u&&t114.released==0x1140aaaau&&r114.u32(8u)==0xffffffffu,"r114 configure releases prior handle");
    require(r114.u32(0)==a114[0]&&r114.u32(4)==0xffffffffu&&r114.u32(0x0c)==a114[1]&&r114.u32(0x10)==a114[2]&&r114.u32(0x14)==a114[3],"r114 configure identity fields");
    require(r114.u32(0x18)==1u&&r114.u32(0x1c)==1u&&r114.u32(0x20)==0u&&r114.u32(0x24)==0u,"r114 configure special mode3 fields");
    require(r114.u32(0x40)==a114[5]&&r114.u32(0x44)==a114[6]&&r114.u32(0x48)==0u&&r114.u32(0x4c)==a114[5]&&r114.u32(0x50)==a114[6]&&r114.u32(0x54)==0u,"r114 configure position vectors");
    require(r114.u32(0x64)==a114[7]&&r114.u32(0x68)==a114[8]&&r114.u32(0x6c)==0x3f800000u&&r114.u32(0x70)==a114[7]&&r114.u32(0x74)==a114[8]&&r114.u32(0x78)==0x3f800000u,"r114 configure scale vectors");
    require(r114.u32(0x3c)==a114[9]&&r114.u32(0x88)==0u&&r114.u32(0x8c)==0u&&r114.u32(0x98)==0u&&r114.u32(0x9c)==0u,"r114 configure animation reset");

    ui_resource_commit_465970(r114,commit114);
    require(t114.pc.size()==2u&&t114.pc[0]==0x428460u&&t114.pc[1]==0x428800u&&t114.finalizes==1u,"r114 commit create5/property/finalize order");
    require(t114.args[0]==std::vector<std::uint32_t>({a114[0],a114[3],1u,a114[1],a114[2]})&&t114.args[1]==std::vector<std::uint32_t>({t114.create_result,a114[9]})&&r114.u32(8u)==t114.create_result,"r114 commit create5 argument payload");
    t114.pc.clear();t114.args.clear();t114.finalizes=0u;r114.put32(0x0cu,0xffffffffu);r114.put32(0x10u,7u);r114.put32(0x24u,0u);
    ui_resource_commit_465970(r114,commit114);require(t114.pc.size()==2u&&t114.pc[0]==0x428320u&&t114.args[0]==std::vector<std::uint32_t>({a114[0],a114[3],1u})&&t114.finalizes==1u,"r114 commit create3 fallback");
    t114.pc.clear();t114.args.clear();t114.finalizes=0u;r114.put32(0x24u,2u);r114.put32(8u,0xdeadbeefu);
    ui_resource_commit_465970(r114,commit114);require(t114.pc.empty()&&t114.finalizes==1u&&r114.u32(8u)==0u,"r114 commit external-handle finalize branch");
    t114.pc.clear();t114.args.clear();t114.finalizes=0u;t114.create_result=0xffffffffu;r114.put32(0x24u,0u);r114.put32(0x0cu,0xffffffffu);
    ui_resource_commit_465970(r114,commit114);require(t114.pc.size()==1u&&t114.finalizes==0u&&r114.u32(8u)==0xffffffffu,"r114 failed create returns without property/finalize");

    std::array<std::uint8_t,0x1000> object_raw114{};Bytes object114(object_raw114.data(),object_raw114.size());object114.put8(0xd94u,1u);object114.put32(0xcfcu,0x1140bbbbu);object114.put32(0xd18u,0u);t114=Ui114{};
    PcRuntimeUiOpenServices4447d0 open114{};open114.ui_services.user=&t114;open114.ui_services.handle_void=release114;open114.commit_services={&t114,call114,final114};open114.invalid_user=&t114;open114.invalid_tail=invalid114;
    require(object_runtime_ui_open_known_4447d0(object114,0u,object114,{},open114),"r114 4447D0 valid state path");
    require(t114.releases==1u&&t114.pc.size()==2u&&t114.pc[0]==0x428460u&&t114.finalizes==1u&&object114.u32(0xcf4u)==0x440094u,"r114 4447D0 configures and commits token");
    const auto preserved114=object114.u32(0xcf4u);require(!object_runtime_ui_open_known_4447d0(object114,61u,object114,{},open114)&&t114.invalid==1u&&object114.u32(0xcf4u)==preserved114,"r114 invalid state remains protected boundary");
    }
    // r081 second mandatory state-2 child: the four embedded resource slots,
    // both configure variants and the 0x446CF0 owner loop.
    {
    struct Ui81 { std::array<std::uint32_t,32> pc{}; std::array<std::uint32_t,32> effect{}; std::array<std::uint32_t,32> slot{}; std::array<std::uint32_t,32> x{}; std::array<std::uint32_t,32> y{}; unsigned n{}; unsigned finalized{}; } t81;
    auto cfg81=[](void* u,std::uint32_t pc,Bytes,std::uint32_t effect,std::uint32_t slot,std::uint32_t x,std::uint32_t y){auto* t=static_cast<Ui81*>(u);const auto i=t->n++;t->pc[i]=pc;t->effect[i]=effect;t->slot[i]=slot;t->x[i]=x;t->y[i]=y;};
    auto fin81=[](void* u,std::uint32_t pc,Bytes){auto* t=static_cast<Ui81*>(u);require(pc==0x465970u,"r081 finalizer pc");++t->finalized;};
    PcEmbeddedSlotsServices446cf0 sv81{};sv81.user=&t81;sv81.configure_ui=cfg81;sv81.finalize_ui=fin81;
    std::array<std::uint8_t,0x700> eraw81{};Bytes e81(eraw81.data(),eraw81.size());
    e81.put32(0x408u,0u);for(unsigned i=0;i<4u;++i)e81.put32(0x40cu+i*0xa0u+8u,0xffffffffu);
    e81.put32(0x08u,0x04u);e81.put8(1u,1u);t81={};embedded_slot_open_primary_446a80(e81,0u,sv81);
    require(t81.n==1u&&t81.pc[0]==0x446a80u&&t81.effect[0]==0x002c013bu&&t81.slot[0]==0u,"r081 primary effect lookup");
    require(t81.x[0]==0xc37c0000u&&t81.y[0]==0x433c0000u&&t81.finalized==1u&&e81.u8(1u)==0u,"r081 primary coords/finalize/clear");
    e81.put32(0x0cu,0x08u);e81.put8(2u,1u);t81={};embedded_slot_open_secondary_446b20(e81,1u,sv81);
    require(t81.n==1u&&t81.pc[0]==0x446b20u&&t81.effect[0]==0x002c013au&&t81.slot[0]==1u,"r081 secondary effect lookup");
    require(t81.x[0]==0xc3180000u&&t81.y[0]==0x433c0000u&&t81.finalized==1u&&e81.u8(2u)==1u,"r081 secondary preserves flag");
    e81.put32(0x10u,0xffffffffu);t81={};embedded_slot_open_primary_446a80(e81,2u,sv81);require(t81.n==0u&&t81.finalized==0u,"r081 invalid history key");
    e81.put32(0x14u,0x12345678u);t81={};embedded_slot_open_secondary_446b20(e81,3u,sv81);require(t81.n==0u&&t81.finalized==0u,"r081 unknown history key");

    // Parent: active embedded block, all resources invalid/ready. It must
    // normalize the marker table, tick every resource, and choose A/B from
    // bytes +1..+4. Invalid resource handles make the 4659F0 calls no-op.
    eraw81.fill(0);e81.put8(0u,1u);e81.put32(0x408u,0u);
    const std::uint32_t keys81[4]={0x04u,0x08u,0x10u,0x20u};
    for(unsigned i=0;i<4u;++i){e81.put32(0x08u+i*4u,keys81[i]);e81.put32(0x18u+i*4u,(i&1u)?0x297u:0x298u);e81.put32(0x40cu+i*0xa0u+8u,0xffffffffu);e81.put8(1u+i,(i&1u)?0u:1u);}
    PcUiNotifyGlobals g81{};g81.alternate=1u;t81={};embedded_slots_tick_446cf0(e81,g81,sv81);
    require(t81.n==4u&&t81.finalized==4u,"r081 parent configured four slots");
    require(t81.pc[0]==0x446a80u&&t81.pc[1]==0x446b20u&&t81.pc[2]==0x446a80u&&t81.pc[3]==0x446b20u,"r081 parent primary secondary routing");
    for(unsigned i=0;i<4u;++i)require(e81.u32(0x18u+i*4u)==0x297u,"r081 alternate marker normalization");
    require(e81.u8(1u)==0u&&e81.u8(2u)==0u&&e81.u8(3u)==0u&&e81.u8(4u)==0u,"r081 primary flags clear only selected A slots");

    // Inactive block still normalizes/ticks but does not enter either setup body.
    eraw81.fill(0);e81.put8(0u,0u);e81.put32(0x408u,0u);for(unsigned i=0;i<4u;++i){e81.put32(0x18u+i*4u,0x297u);e81.put32(0x40cu+i*0xa0u+8u,0xffffffffu);}g81.alternate=0u;t81={};embedded_slots_tick_446cf0(e81,g81,sv81);
    require(t81.n==0u&&t81.finalized==0u,"r081 inactive suppresses setup");for(unsigned i=0;i<4u;++i)require(e81.u32(0x18u+i*4u)==0x298u,"r081 normal marker normalization");
    }
    // r082 third mandatory state-2 child: feature/history gate at 0x445810.
    {
    std::array<std::uint8_t,0x1000> oraw82{},craw82{},hcur82{},hidx82{},hown82{};
    Bytes o82(oraw82.data(),oraw82.size()),c82(craw82.data(),craw82.size());
    Bytes hc82(hcur82.data(),hcur82.size()),hi82(hidx82.data(),hidx82.size()),ho82(hown82.data(),hown82.size());
    hc82.put32(0x08u,0x10u);hi82.put32(0x08u,0x14u);ho82.put32(0x08u,0x20u);
    std::array<PcNativeHandleBinding,3> hb82{{{0x82001000u,hcur82.data(),hcur82.size()},{0x82002000u,hidx82.data(),hidx82.size()},{0x82003000u,hown82.data(),hown82.size()}}};
    PcNativeHandleResolver hr82{hb82.data(),hb82.size()};
    c82.put32(0x488u,0x82001000u);c82.put8(0x48cu,1u);
    struct T82{unsigned cfg{},act{};std::uint32_t selector{},key{};}t82;
    auto cfg82=[](void* u,Bytes,std::uint32_t sel,std::uint8_t flag){auto* t=static_cast<T82*>(u);++t->cfg;t->selector=sel;require(flag==1u,"r082 config flag");};
    auto act82=[](void* u,std::uint32_t pc,Bytes,std::uint32_t key){auto* t=static_cast<T82*>(u);++t->act;t->key=key;require(pc==0x444fe0u,"r082 action pc");};
    PcRuntimeGate445810Services sv82{&t82,cfg82,act82};
    PcRuntimeGate445810Inputs in82{};in82.system_handle_67f614=0xffffffffu;in82.menu_state_7d38f0=0u;
    PcObjectStatePairTable empty_pairs82{};PcObjectEventCallbackTable empty_callbacks82{};PcObjectEventModeInputs empty_event82{};PcObjectEventDispatch443eb0Services empty_dispatch82{};PcRuntimeControlServices empty_runtime82{};PcObjectOpenCallbackServices empty_open82{};
    o82.put32(0x218u,2u);o82.put8(0x220u,0u);o82.put8(0x241u,1u);o82.put32(0x484u,1u);o82.put32(0x284u,0x82002000u);o82.put32(0x488u,0x82003000u);
    in82.player0_feature_mask=0x20u;t82={};object_runtime_gate_445810(o82,c82,hr82,in82,empty_pairs82,empty_callbacks82,empty_event82,empty_dispatch82,empty_runtime82,empty_open82,sv82);
    require(t82.cfg==1u&&t82.selector==0x4000u&&t82.act==1u&&t82.key==0x15u,"r082 4000 configure/action branch");
    oraw82.fill(0);o82.put32(0x218u,2u);o82.put8(0x220u,0u);o82.put32(0x488u,0x82003000u);in82.player0_feature_mask=0x10u;t82={};object_runtime_gate_445810(o82,c82,hr82,in82,empty_pairs82,empty_callbacks82,empty_event82,empty_dispatch82,empty_runtime82,empty_open82,sv82);
    require(t82.cfg==1u&&t82.selector==0x8000u&&t82.act==0u,"r082 8000 configure branch");
    oraw82.fill(0);o82.put32(0x218u,3u);o82.put8(0x220u,0u);o82.put8(0x221u,1u);o82.put32(0x488u,0x82003000u);o82.put32(0x21cu,0u);in82.player0_feature_mask=0x10u;in82.game_mode_78026c=0x10u;t82={};object_runtime_gate_445810(o82,c82,hr82,in82,empty_pairs82,empty_callbacks82,empty_event82,empty_dispatch82,empty_runtime82,empty_open82,sv82);
    require(o82.u32(0x21cu)==0x12u&&t82.act==0u,"r082 state3 normalization");
    hc82.put32(0x08u,0x1bu);oraw82.fill(0);o82.put32(0x218u,2u);in82.player0_feature_mask=0x20u;t82={};object_runtime_gate_445810(o82,c82,hr82,in82,empty_pairs82,empty_callbacks82,empty_event82,empty_dispatch82,empty_runtime82,empty_open82,sv82);require(t82.cfg==0u&&t82.act==0u,"r082 current-handle 1b gate");hc82.put32(0x08u,0x10u);
    }
    // r083 fourth mandatory state-2 child: transition handle arbiter at 0x444530.
    {
    std::array<std::uint8_t,0x1000> oraw83{},hprev83{},hnew83{};
    Bytes o83(oraw83.data(),oraw83.size()),hp83(hprev83.data(),hprev83.size()),hn83(hnew83.data(),hnew83.size());
    constexpr std::uint32_t Prev83=0x83001000u,New83=0x83002000u;
    std::array<PcNativeHandleBinding,2> hb83{{{Prev83,hprev83.data(),hprev83.size()},{New83,hnew83.data(),hnew83.size()}}};
    PcNativeHandleResolver hr83{hb83.data(),hb83.size()};
    std::array<PcObjectStatePairEntry,2> pairs83{{{0x0eu,0u,0u},{0x0fu,0u,0u}}};
    PcObjectStatePairTable pt83{pairs83.data(),pairs83.size()};
    std::array<PcObjectEventCallbackEntry,PcObjectEventCallbackCount> callbacks83{};
    for(std::size_t k=0;k<callbacks83.size();++k)callbacks83[k]={std::uint32_t(k),0x83000000u+std::uint32_t(k)};
    PcObjectEventCallbackTable ct83{callbacks83.data(),callbacks83.size()};
    struct T83{unsigned cb{},ready{},rel10{},rel0{},reli8{};std::uint32_t callback_token{},ready_ret{1u};}t83;
    auto cb83=[](void* u,std::uint32_t token,std::size_t)->std::uint32_t{auto* t=static_cast<T83*>(u);++t->cb;t->callback_token=token;return New83;};
    auto ready83=[](void* u,std::uint32_t h,std::uint32_t slot)->std::uint32_t{auto* t=static_cast<T83*>(u);++t->ready;require(h==New83&&slot==4u,"r083 ready handle/slot");return t->ready_ret;};
    auto relv83=[](void* u,std::uint32_t pc,std::uint32_t){auto* t=static_cast<T83*>(u);require(pc==0x441210u,"r083 release virtual10 pc");++t->rel10;};
    auto rela83=[](void* u,std::uint32_t pc,std::uint32_t,std::uint32_t a){auto* t=static_cast<T83*>(u);require(pc==0x441219u&&a==1u,"r083 release virtual0 pc/arg");++t->rel0;};
    auto reli83=[](void* u,std::uint32_t pc,std::uint32_t)->std::int8_t{auto* t=static_cast<T83*>(u);require(pc==0x01039ba0u,"r083 release index pc");++t->reli8;return 0;};
    PcObjectEventDispatch443eb0Services ds83{&t83,nullptr,nullptr,cb83,nullptr};
    PcRuntimeControlServices rs83{&t83,relv83,rela83,nullptr,nullptr,nullptr,reli83};
    PcObjectOpenCallbackServices os83{&t83,ready83};
    PcObjectEventModeInputs ei83{};PcRuntimeControlGlobals rg83{};PcRuntimeTransitionFlags444530 fl83{};
    auto reset83=[&]{oraw83.fill(0);hprev83.fill(0);hnew83.fill(0);hp83.put32(0x08u,0x0eu);hn83.put32(0x08u,0x0eu);o83.put8(0x220u,0u);t83={};fl83={};rg83={};};

    reset83(); // not runtime-ready and no explicit 0x0F request => strict no-op.
    object_runtime_transition_444530(o83,fl83,rg83,hr83,pt83,ct83,ei83,ds83,rs83,os83);
    require(t83.cb==0u&&o83.u32(0x490u)==0u&&o83.u8(0x494u)==0u,"r083 inactive no-op");

    reset83();rg83.mode_836130=1u;hnew83.fill(0);hn83.put32(0x08u,0x0eu);
    object_runtime_transition_444530(o83,fl83,rg83,hr83,pt83,ct83,ei83,ds83,rs83,os83);
    require(t83.cb==1u&&t83.callback_token==0x8300000eu&&o83.u32(0x490u)==New83&&o83.u8(0x494u)==1u&&t83.ready==1u,"r083 ready opens 0e");

    reset83();fl83.request_7d68cb=1u;hn83.put32(0x08u,0x0fu);
    object_runtime_transition_444530(o83,fl83,rg83,hr83,pt83,ct83,ei83,ds83,rs83,os83);
    require(fl83.request_7d68cb==0u&&fl83.active_7d68d0==1u&&t83.callback_token==0x8300000fu&&o83.u32(0x490u)==New83,"r083 forced 0f flags/open");

    reset83();rg83.mode_836130=1u;o83.put32(0x490u,Prev83);hp83.put32(0x08u,0x0eu);
    object_runtime_transition_444530(o83,fl83,rg83,hr83,pt83,ct83,ei83,ds83,rs83,os83);
    require(t83.cb==0u&&o83.u32(0x490u)==Prev83,"r083 matching existing state preserved");

    reset83();rg83.mode_836130=1u;o83.put32(0x490u,Prev83);hp83.put32(0x08u,0x0fu);
    object_runtime_transition_444530(o83,fl83,rg83,hr83,pt83,ct83,ei83,ds83,rs83,os83);
    require(t83.cb==0u&&o83.u32(0x490u)==Prev83,"r083 existing 0f blocks replacement");

    reset83();fl83.request_7d68cb=1u;o83.put32(0x490u,Prev83);o83.put8(0x494u,1u);hp83.put32(0x08u,0x0eu);hn83.put32(0x08u,0x0eu);
    object_runtime_transition_444530(o83,fl83,rg83,hr83,pt83,ct83,ei83,ds83,rs83,os83);
    require(t83.cb==1u&&t83.rel10==1u&&t83.rel0==1u&&t83.reli8==1u&&o83.u32(0x490u)==Prev83&&o83.u8(0x494u)==1u,"r083 rejects non-0f replacement and releases new");

    reset83();fl83.request_7d68cb=1u;o83.put32(0x490u,Prev83);o83.put8(0x494u,1u);hp83.put32(0x08u,0x0eu);hn83.put32(0x08u,0x0fu);
    object_runtime_transition_444530(o83,fl83,rg83,hr83,pt83,ct83,ei83,ds83,rs83,os83);
    require(t83.cb==1u&&t83.rel10==1u&&t83.rel0==1u&&t83.reli8==1u&&t83.ready==1u&&o83.u32(0x490u)==New83&&o83.u8(0x494u)==1u,"r083 replaces flagged old handle");

    reset83();rg83.mode_836130=1u;hn83.put32(0x08u,0x0eu);t83.ready_ret=0u;
    object_runtime_transition_444530(o83,fl83,rg83,hr83,pt83,ct83,ei83,ds83,rs83,os83);
    require(t83.cb==1u&&t83.ready==1u&&t83.rel10==1u&&o83.u32(0x490u)==0u&&o83.u8(0x494u)==0u,"r083 not-ready new handle released/cleared");
    }
    // r084 final mandatory state-2 child: manager-idle predicate, key-0x2E
    // open helper and 20-slot runtime queue parent.
    {
    std::array<std::uint8_t,0x1800> manager84{};Bytes m84(manager84.data(),manager84.size());
    require(runtime_pair_idle_434de0(m84)==1u,"r084 manager idle true");
    m84.put32(0x16d4u,1u);require(runtime_pair_idle_434de0(m84)==0u,"r084 manager idle first busy");
    m84.put32(0x16d4u,0u);m84.put32(0x16d8u,1u);require(runtime_pair_idle_434de0(m84)==0u,"r084 manager idle second busy");m84.put32(0x16d8u,0u);

    std::array<std::uint8_t,0x1000> oraw84{},craw84{},hnew84{},table84raw{};
    Bytes o84(oraw84.data(),oraw84.size()),c84(craw84.data(),craw84.size()),hn84(hnew84.data(),hnew84.size()),tbl84(table84raw.data(),table84raw.size());
    constexpr std::uint32_t New84=0x84002000u;hn84.put32(0x08u,0x2eu);
    std::array<PcObjectStatePairEntry,1> pairs84{{{0x2eu,0u,0u}}};PcObjectStatePairTable pt84{pairs84.data(),pairs84.size()};
    std::array<PcObjectEventCallbackEntry,PcObjectEventCallbackCount> callbacks84{};for(std::size_t k=0;k<callbacks84.size();++k)callbacks84[k]={std::uint32_t(k),0x84000000u+std::uint32_t(k)};PcObjectEventCallbackTable ct84{callbacks84.data(),callbacks84.size()};
    struct T84{unsigned cb{},ready{},action{},emit{};std::uint32_t mode{},fmt{},pc{},key{},ready_ret{1u};std::uint8_t first_char{};}t84;
    auto cb84=[](void* u,std::uint32_t token,std::size_t)->std::uint32_t{auto* t=static_cast<T84*>(u);++t->cb;t->key=token;return New84;};
    auto ready84=[](void* u,std::uint32_t h,std::uint32_t slot)->std::uint32_t{auto* t=static_cast<T84*>(u);++t->ready;require(h==New84&&slot==4u,"r084 ready handle/slot");return t->ready_ret;};
    auto action84=[](void* u,std::uint32_t pc,Bytes,std::uint32_t key){auto* t=static_cast<T84*>(u);++t->action;t->pc=pc;t->key=key;};
    auto emit84=[](void* u,std::uint32_t pc,std::uint32_t mode,std::uint32_t fmt,Bytes slot,std::size_t text,std::int32_t life,std::uint32_t limit){auto* t=static_cast<T84*>(u);++t->emit;t->pc=pc;t->mode=mode;t->fmt=fmt;t->first_char=slot.u8(text);require(life==-1&&limit==10u,"r084 emit lifetime/limit");};
    PcObjectEventDispatch443eb0Services ds84{&t84,nullptr,nullptr,cb84,nullptr};PcRuntimeControlServices rs84{};PcObjectOpenCallbackServices os84{&t84,ready84};PcRuntimeOpen4459f0Services as84{&t84,action84};PcObjectEventModeInputs ei84{};
    object_runtime_open_4459f0(o84,1u,pt84,ct84,ei84,ds84,rs84,os84,as84);require(t84.action==1u&&t84.pc==0x444fe0u&&t84.key==0x2eu,"r084 direct action boundary");
    oraw84.fill(0);t84={};object_runtime_open_4459f0(o84,0u,pt84,ct84,ei84,ds84,rs84,os84,as84);require(t84.cb==1u&&t84.ready==1u&&o84.u32(0x488u)==New84&&o84.u8(0x48cu)==1u,"r084 native key2e open");

    PcRuntimeQueueState445a50 q84{};q84.alternate_7d68bc=1u;c84.put32(0x218u,2u);PcRuntimeControlGlobals rg84{};PcRuntimeQueueServices445a50 qs84{&t84,emit84};
    tbl84.put32(0x340u*4u,0x84340000u);tbl84.put32(0x341u*4u,0x84341000u);
    auto reset_open84=[&]{oraw84.fill(0);t84={};q84.busy_98a5f4=0u;for(auto& x:q84.slots)x.fill(0);q84.out_989320=0u;q84.out_989324=0u;q84.out_989328=0xaabbccddu;q84.out_98932c=0x11223344u;};
    reset_open84();{Bytes s84(q84.slots[0].data(),q84.slots[0].size());s84.put32(0,1u);s84.put32(8,0x11111111u);s84.put32(0xc,0x22222222u);s84.put8(0x10,'A');}
    object_runtime_queue_445a50(o84,c84,m84,tbl84,q84,rg84,pt84,ct84,ei84,ds84,rs84,os84,as84,qs84);
    require(t84.emit==1u&&t84.mode==0u&&t84.fmt==0x84340000u&&t84.first_char=='A',"r084 type1 format emit");
    require(q84.out_989320==0x11111111u&&q84.out_989324==0x22222222u&&q84.out_989328==0xaabbccddu&&q84.out_98932c==0x11223344u,"r084 type1 outputs preserve extended pair");
    require(q84.busy_98a5f4==1u&&q84.slots[0][0x29u]==1u&&o84.u8(0x48cu)==1u,"r084 type1 busy/open");

    reset_open84();q84.slots[0][0x29u]=1u;{Bytes s84(q84.slots[1].data(),q84.slots[1].size());s84.put32(0,5u);s84.put32(8,0x33333333u);s84.put32(0xc,0x44444444u);s84.put32(0x21u,0x55555555u);s84.put32(0x25u,0x66666666u);s84.put8(0x10,'B');}
    object_runtime_queue_445a50(o84,c84,m84,tbl84,q84,rg84,pt84,ct84,ei84,ds84,rs84,os84,as84,qs84);
    require(t84.emit==1u&&t84.mode==1u&&t84.fmt==0x84341000u&&q84.out_989328==0x55555555u&&q84.out_98932c==0x66666666u,"r084 type5 extended outputs");

    reset_open84();{Bytes a(q84.slots[0].data(),q84.slots[0].size()),b(q84.slots[1].data(),q84.slots[1].size());a.put32(0,7u);b.put32(0,1u);b.put8(0x10,'C');}
    object_runtime_queue_445a50(o84,c84,m84,tbl84,q84,rg84,pt84,ct84,ei84,ds84,rs84,os84,as84,qs84);
    require(q84.slots[0][0x29u]==1u&&q84.slots[1][0x29u]==1u&&t84.emit==1u,"r084 unknown slot marked then scan continues");

    reset_open84();q84.alternate_7d68bc=0u;object_runtime_queue_445a50(o84,c84,m84,tbl84,q84,rg84,pt84,ct84,ei84,ds84,rs84,os84,as84,qs84);require(t84.emit==0u&&q84.busy_98a5f4==0u,"r084 alternate gate");q84.alternate_7d68bc=1u;
    reset_open84();q84.busy_98a5f4=1u;object_runtime_queue_445a50(o84,c84,m84,tbl84,q84,rg84,pt84,ct84,ei84,ds84,rs84,os84,as84,qs84);require(t84.emit==0u,"r084 busy gate");
    }
    // r085 registered callback thunk above 0x445BE0. Native ownership makes the
    // protected 0x4035F0 singleton result an explicit object view.
    {
    std::array<std::uint8_t,0x1000> o85raw{};Bytes o85(o85raw.data(),o85raw.size());
    PcObjectRuntimeSnapshot444350 snap85{};PcObjectSelectorGlobals sg85{};PcRuntimeOwnerServices445be0 sv85{};
    const PcRuntimeOwnerInputs445be0 in85{12.0f,0.125f};
    o85.put32(0x218u,0u);o85.putf(0xd9cu,0.5f);
    require(runtime_owner_entry_49e4a0(o85,snap85,sg85,in85,sv85)==1u,"r085 callback thunk state0 result");
    require(o85.f32(0xd98u)==0.5f&&o85.f32(0xd9cu)==1.5f&&o85.f32(0xda0u)==1.0f,"r085 callback thunk owner timing");
    o85raw.fill(0);o85.put32(0x218u,4u);o85.put8(0x220u,5u);o85.put32(0x214u,0xdeadbeefu);
    require(runtime_owner_entry_49e4a0(o85,snap85,sg85,in85,sv85)==1u,"r085 callback thunk state4 result");
    require(o85.u32(0x214u)==0u&&o85.u8(0x226u)==1u&&o85.u8(0x246u)==0u&&o85.u32(0x218u)==3u,"r085 callback thunk state4 owner transition");
    }
    // r086 closes the static EventOpen provider and completes the EvFuncID 36
    // callback record surrounding the r085 control thunk.
    {
    std::array<PcEventInitDescriptor,PcEventSlotCount> d86{};
    std::array<PcEventFunctionDescriptor,PcEventFunctionTableCount> f86{};
    constexpr std::uint32_t ev86=17u,fn86=36u;
    d86[ev86].display_scene=0x12345678u;
    f86[fn86]={0x0049e490u,0x0049e4a0u,0x0049e4b0u,0u,0x0049e4c0u};
    PcEventControlState st86{};st86.slots[ev86].flags=0xffu;
    event_setup_440110(st86,ev86,fn86,d86,f86);
    const auto& q86=st86.slots[ev86];
    require(q86.flags==0xe7u&&q86.display_scene==0x12345678u,"r086 provider flags/display scene");
    require(q86.init_callback==0x0049e490u&&q86.ctrl_callback==0x0049e4a0u&&
            q86.disp_callback==0x0049e4b0u&&q86.shadow_callback==0u&&q86.dest_callback==0x0049e4c0u,
            "r086 provider EvFuncID36 callbacks");
    struct Ev86Trace{std::array<std::uint32_t,8> cb{};unsigned n{};} tr86;
    auto invoke86=[](void* u,std::uint32_t cb,std::uint32_t,std::uint32_t){auto* t=static_cast<Ev86Trace*>(u);if(t->n<t->cb.size())t->cb[t->n++]=cb;};
    PcEventServices es86{&tr86,invoke86,nullptr};
    st86.slots[ev86].flags=0u;st86.slots[ev86].event_id=ev86;st86.slots[ev86].work_token=0x861234u;
    event_open_static_440180(st86,ev86,fn86,d86,f86,es86);
    require(st86.slots[ev86].flags==0x02u&&tr86.n==1u&&tr86.cb[0]==0x0049e490u,"r086 direct EventOpen init");
    event_control_43fab0(st86,es86);
    require(tr86.n==2u&&tr86.cb[1]==0x0049e4a0u,"r086 EventControl reaches owner callback");
    event_close_4401d0(st86,ev86);event_control_43fab0(st86,es86);
    require(tr86.n==3u&&tr86.cb[2]==0x0049e4c0u&&st86.slots[ev86].flags==0u,"r086 EventClose destroy callback");

    std::array<std::uint8_t,0x1000> a86raw{},b86raw{};Bytes a86(a86raw.data(),a86raw.size()),b86(b86raw.data(),b86raw.size());
    std::array<PcObjectEventCallbackEntry,PcObjectEventCallbackCount> at86{},bt86{};
    PcObjectRuntimeInitServices is86{};
    const auto ar86=runtime_init_entry_49e490(a86,at86,1u,is86);
    const auto br86=object_runtime_init_4436c0(b86,bt86,1u,is86);
    bool same86=true;for(std::size_t i=0;i<at86.size();++i)same86=same86&&at86[i].key==bt86[i].key&&at86[i].callback_token==bt86[i].callback_token;
    require(ar86==br86&&a86raw==b86raw&&same86,"r086 init thunk equals closed body");
    PcObjectStateServices ds86{};runtime_display_entry_49e4b0(a86,ds86);object_dispatch_state_442e00(b86,ds86);
    require(a86raw==b86raw,"r086 display thunk equals closed body");
    PcObjectRuntimeTeardownServices ts86{};
    const auto ta86=runtime_destroy_entry_49e4c0(a86,ts86);const auto tb86=object_runtime_teardown_443c30(b86,ts86);
    require(ta86==tb86&&a86raw==b86raw,"r086 destroy thunk/alias equals closed teardown");
    }
    // r087 top-level bootstrap: fixed subsystem order, native InitEventControl,
    // two virtual registration boundaries and final tail-call return propagation.
    {
    PcEventControlState ev87{};
    std::array<PcEventInitDescriptor,PcEventSlotCount> desc87{};
    std::array<PcEventFunctionDescriptor,PcEventFunctionTableCount> funcs87{};
    for(std::size_t k=0;k<desc87.size();++k){
        desc87[k].descriptor_token=0x87000000u+std::uint32_t(k);
        desc87[k].work_token=0x87100000u+std::uint32_t(k);
        desc87[k].display_scene=std::uint32_t(k&0x3fu);
    }
    desc87[405].startup=1u;desc87[405].function_id=36u;desc87[405].display_scene=0x20u;
    funcs87[36]={0x49e490u,0x49e4a0u,0x49e4b0u,0u,0x49e4c0u};
    struct T87{std::vector<std::array<std::uint32_t,4>> calls;}t87;
    auto cb87=[](void* u,std::uint32_t pc,std::uint32_t a0,std::uint32_t a1,std::uint32_t a2)->std::uint32_t{
        auto* t=static_cast<T87*>(u);t->calls.push_back({pc,a0,a1,a2});return pc==0x448730u?0x87abcdefu:0u;
    };
    const auto ret87=runtime_bootstrap_417810(ev87,desc87,funcs87,{&t87,cb87,0x89bd6000u});
    const std::array<std::uint32_t,15> order87{{
        0x4487d0u,0x429b60u,0x413b30u,0x43f880u,0x448ce0u,
        0x44f7d0u,0x49a650u,0x41784fu,0x414ce0u,0x41786cu,
        0x49a650u,0x45ae10u,0x44a630u,0x416a40u,0x448730u}};
    require(t87.calls.size()==order87.size(),"r087 bootstrap boundary count");
    for(std::size_t k=0;k<order87.size();++k)require(t87.calls[k][0]==order87[k],"r087 bootstrap boundary order");
    require(t87.calls[5][1]==0u,"r087 44f7d0 zero argument");
    require(t87.calls[7][1]==0x89bd6000u&&t87.calls[7][2]==0x623830u&&t87.calls[7][3]==0x95af9cu,"r087 first virtual arguments");
    require(t87.calls[9][1]==0x89bd6000u&&t87.calls[9][2]==0x623e38u&&t87.calls[9][3]==0x955a40u,"r087 second virtual arguments");
    require(ret87==0x87abcdefu,"r087 tail return propagation");
    require(ev87.slots[405].flags==1u&&ev87.slots[405].close_guard==1u&&ev87.slots[405].function_id==36u,"r087 startup event405 installed");
    require(ev87.slots[405].init_callback==0x49e490u&&ev87.slots[405].ctrl_callback==0x49e4a0u&&ev87.slots[405].disp_callback==0x49e4b0u&&ev87.slots[405].shadow_callback==0u&&ev87.slots[405].dest_callback==0x49e4c0u,"r087 event405 EvFuncID36 callbacks");
    }
    // r088 startup/shutdown owner above r087: gate, real native bootstrap,
    // later runtime boundaries and deterministic release/null of both system globals.
    {
    auto make_desc88=[](){
        std::array<PcEventInitDescriptor,PcEventSlotCount> d{};
        for(std::size_t k=0;k<d.size();++k){d[k].descriptor_token=0x88000000u+std::uint32_t(k);d[k].work_token=0x88100000u+std::uint32_t(k);d[k].display_scene=std::uint32_t(k&0x3fu);}
        d[405].startup=1u;d[405].function_id=36u;d[405].display_scene=0x20u;return d;
    };
    std::array<PcEventFunctionDescriptor,PcEventFunctionTableCount> funcs88{};funcs88[36]={0x49e490u,0x49e4a0u,0x49e4b0u,0u,0x49e4c0u};
    struct T88{std::vector<std::array<std::uint32_t,4>> calls;bool gate{};}t88;
    auto cb88=[](void* u,std::uint32_t pc,std::uint32_t a0,std::uint32_t a1,std::uint32_t a2)->std::uint32_t{
        auto* t=static_cast<T88*>(u);t->calls.push_back({pc,a0,a1,a2});if(pc==0x417740u)return t->gate?1u:0u;return pc==0x448730u?0x88abcdefu:0u;
    };
    PcEventControlState ev88{};auto desc88=make_desc88();std::uint32_t sys88a=0x89bd6000u,sys88b=0x89bd9c00u;t88.gate=true;
    const auto ret88=runtime_startup_owner_4176e0(ev88,desc88,funcs88,sys88a,sys88b,{&t88,cb88});
    const std::array<std::uint32_t,23> order88{{
        0x417740u,
        0x4487d0u,0x429b60u,0x413b30u,0x43f880u,0x448ce0u,0x44f7d0u,0x49a650u,0x41784fu,0x414ce0u,0x41786cu,0x49a650u,0x45ae10u,0x44a630u,0x416a40u,0x448730u,
        0x417b20u,0x417e30u,0x49a650u,0x40e3b0u,0x403fb0u,0x417713u,0x41772cu}};
    require(ret88==0u&&t88.calls.size()==order88.size(),"r088 gated owner return/call count");
    for(std::size_t k=0;k<order88.size();++k)require(t88.calls[k][0]==order88[k],"r088 gated owner call order");
    require(t88.calls[21][1]==0x89bd6000u&&t88.calls[22][1]==0x89bd9c00u,"r088 release object arguments");
    require(sys88a==0u&&sys88b==0u,"r088 release clears both system globals");
    require(ev88.slots[405].flags==1u&&ev88.slots[405].function_id==36u,"r088 gated owner executes native r087 event bootstrap");

    t88.calls.clear();t88.gate=false;PcEventControlState ev88b{};auto desc88b=make_desc88();std::uint32_t sys88c=0x11110000u,sys88d=0x22220000u;
    runtime_startup_owner_4176e0(ev88b,desc88b,funcs88,sys88c,sys88d,{&t88,cb88});
    const std::array<std::uint32_t,6> order88b{{0x417740u,0x49a650u,0x40e3b0u,0x403fb0u,0x417713u,0x41772cu}};
    require(t88.calls.size()==order88b.size(),"r088 ungated owner call count");for(std::size_t k=0;k<order88b.size();++k)require(t88.calls[k][0]==order88b[k],"r088 ungated owner call order");
    require(ev88b.slots[405].flags==0u&&sys88c==0u&&sys88d==0u,"r088 ungated owner skips bootstrap but releases systems");
    }
    // r090 native platform-init gate: early failure preserves later state; success
    // owns globals/scratch/thread and exact service ordering/arguments.
    {
    struct T90{std::vector<std::array<std::uint32_t,4>> calls;std::uint32_t gate{},thread{};}t90;
    auto cb90=[](void* u,std::uint32_t pc,std::uint32_t a0,std::uint32_t a1,std::uint32_t a2)->std::uint32_t{
        auto* t=static_cast<T90*>(u);t->calls.push_back({pc,a0,a1,a2});
        if(pc==0x40e470u)return t->gate;
        if(pc==0x4177e6u)return t->thread;
        return 0u;
    };
    PcPlatformInitState417740 st90{};st90.window_token=0x8a8c8800u;st90.resource_token_740ca0=0x740ca000u;
    st90.global_8a8ce0=1;st90.global_8a8cac=2;st90.global_89f684=3;st90.global_89f66c=4;st90.thread_token=0xdeadbeefu;st90.scratch_8999c0.fill(0xfeedfaceu);
    t90.gate=0u;t90.thread=0x955ad800u;
    require(runtime_platform_init_417740(st90,0x89bd6000u,{&t90,cb90})==0u,"r090 platform init failure return");
    require(t90.calls.size()==2u&&t90.calls[0][0]==0x49a650u&&t90.calls[1][0]==0x40e470u,"r090 platform init failure order");
    require(t90.calls[1][1]==0x8a8c8800u,"r090 platform init window argument");
    require(st90.global_8a8ce0==0u&&st90.global_8a8cac==0u,"r090 early globals cleared");
    require(st90.global_89f684==3u&&st90.thread_token==0xdeadbeefu&&st90.scratch_8999c0[0]==0xfeedfaceu,"r090 early exit preserves later state");

    t90.calls.clear();t90.gate=1u;st90.global_8a8ce0=5;st90.global_8a8cac=6;st90.global_89f684=7;st90.global_89f66c=8;st90.thread_token=0;st90.scratch_8999c0.fill(0xa5a5a5a5u);
    require(runtime_platform_init_417740(st90,0x89bd6000u,{&t90,cb90})==1u,"r090 platform init success return");
    const std::array<std::uint32_t,16> order90{{0x49a650u,0x40e470u,0x409bb0u,0x417779u,0x4231e0u,0x42ed00u,0x403de0u,0x4535f0u,0x49a650u,0x453440u,0x45acb0u,0x4493d0u,0x465df0u,0x427db0u,0x4177e6u,0x4177f3u}};
    require(t90.calls.size()==order90.size(),"r090 platform init success call count");for(std::size_t k=0;k<order90.size();++k)require(t90.calls[k][0]==order90[k],"r090 platform init success order");
    require(t90.calls[3][1]==0x89bd6000u&&t90.calls[3][2]==9u&&t90.calls[3][3]==0x89f680u,"r090 system virtual args");
    require(t90.calls[4][1]==2u&&t90.calls[4][2]==1u&&t90.calls[4][3]==0x20u,"r090 4231e0 args");
    require(t90.calls[11][1]==0x740ca000u,"r090 resource argument");
    require(t90.calls[14][1]==0x424090u&&t90.calls[15][1]==0x955ad800u,"r090 thread create/priority arguments");
    bool zero90=true;for(auto v:st90.scratch_8999c0)zero90=zero90&&v==0u;
    require(st90.global_8a8ce0==0u&&st90.global_8a8cac==0u&&st90.global_89f684==0u&&st90.global_89f66c==0u&&zero90&&st90.thread_token==0x955ad800u,"r090 success state ownership");
    }
    // r093 timing helper: portable clock service, frame quantization and remainder.
    struct T93{std::int64_t freq{1000};std::vector<std::int64_t> counters{1017,1033,1049};std::size_t pos{};std::vector<std::uint32_t> q;};
    auto q93=[](void* u,std::uint32_t id)->std::int64_t{auto& t=*static_cast<T93*>(u);t.q.push_back(id);if(id==0x596104u)return t.freq;if(id==0x5960fcu)return t.counters.at(t.pos++);return 0;};
    T93 t93{};PcRuntimeTimingState417890 st93{};st93.previous_counter_8a8c90=1000;st93.gate_8a8cc8=1u;
    require(runtime_frame_ticks_417890(st93,60,{&t93,q93})==1u,"r093 timing first frame");
    require(st93.frequency_8a8c98==1000&&st93.current_counter_8a8ca0==1017&&st93.previous_counter_8a8c90==1017,"r093 timing sampled state");
    require(st93.accumulator_8a8cd0==1&&bits(st93.frame_scale_95af40)==0x3f800000u,"r093 timing remainder and scale");
    require(runtime_frame_ticks_417890(st93,60,{&t93,q93})==1u&&st93.accumulator_8a8cd0==1,"r093 timing second frame remainder");
    st93.gate_8a8cc8=0u;require(runtime_frame_ticks_417890(st93,60,{&t93,q93})==1u&&st93.accumulator_8a8cd0==0,"r093 timing disabled gate reset");
    require(t93.q==std::vector<std::uint32_t>({0x596104u,0x5960fcu,0x596104u,0x5960fcu,0x596104u,0x5960fcu}),"r093 timing query order");

    // r094 frame-step bridge: central update block without the Win32 message pump.
    struct T94{std::vector<std::uint32_t> pc;std::vector<std::vector<std::uint32_t>> args;std::vector<float> elapsed{18.0f,0.010f};std::size_t ep{};};
    auto c94=[](void* u,std::uint32_t pc,const std::uint32_t* a,std::size_t n)->std::uint32_t{auto& t=*static_cast<T94*>(u);t.pc.push_back(pc);t.args.emplace_back(a,a+n);if(pc==0x455c20u)return 0u;if(pc==0x55a930u)return 1u;if(pc==0x417d68u)return 0x88760868u;return 0u;};
    auto e94=[](void* u,std::uint32_t)->float{auto& t=*static_cast<T94*>(u);return t.elapsed.at(t.ep++);};
    T93 clock94{};clock94.freq=6000;clock94.counters={350};PcRuntimeTimingState417890 ts94{};ts94.previous_counter_8a8c90=0;ts94.gate_8a8cc8=1u;
    T94 t94{};PcRuntimeFrameState417c7b fs94{};fs94.mode_78026c=0x10u;fs94.primary_system_token=0x89bd6000u;fs94.frame_counter_95af0c=7u;fs94.slow_frame_count_8a8cc4=2u;
    const PcRuntimeTimingServices417890 clk94{&clock94,q93};const auto rr94=runtime_frame_step_417c7b(fs94,{&t94,c94,e94,&ts94,&clk94});
    require(rr94.updates==3u&&rr94.platform_wait_before_next_frame,"r094 frame updates/wait result");
    require(fs94.updates_95af48==3u&&fs94.update_index_8a8cdc==0u,"r094 frame update state");
    require(fs94.frame_counter_95af0c==8u&&bits(fs94.elapsed_8a8cb4)==bits(18.0f),"r094 frame counter/elapsed");
    require(fs94.slow_frame_flag_8a8cc0==1u&&fs94.slow_frame_count_8a8cc4==3u,"r094 slow frame state");
    require(std::count(t94.pc.begin(),t94.pc.end(),0x453bb0u)==3&&std::count(t94.pc.begin(),t94.pc.end(),0x43fab0u)==3,"r094 per-update loop count");
    require(std::find(t94.pc.begin(),t94.pc.end(),0x4819c0u)!=t94.pc.end()&&std::find(t94.pc.begin(),t94.pc.end(),0x4021b0u)!=t94.pc.end(),"r094 conditional post calls");
    require(t94.pc.front()==0x449430u&&t94.pc[1]==0x455c20u&&t94.pc[2]==0x43fa10u,"r094 frame prefix order");
    require(t94.args[2]==std::vector<std::uint32_t>({3u}),"r094 frame count argument");
    require(t94.args.back()==std::vector<std::uint32_t>({0u}),"r094 file control arg");
    T93 clock94b{};clock94b.freq=6000;clock94b.counters={850};PcRuntimeTimingState417890 ts94b{};ts94b.previous_counter_8a8c90=350;ts94b.gate_8a8cc8=1u;T94 t94b{};t94b.elapsed={1.0f,0.020f};PcRuntimeFrameState417c7b fs94b{};fs94b.mode_78026c=0x20u;fs94b.primary_system_token=1u;const PcRuntimeTimingServices417890 clk94b{&clock94b,q93};const auto rr94b=runtime_frame_step_417c7b(fs94b,{&t94b,c94,e94,&ts94b,&clk94b});
    require(rr94b.updates==1u&&!rr94b.platform_wait_before_next_frame,"r094 special mode forces one update");
    require(ts94b.previous_counter_8a8c90==850,"r094 special mode still advances timing");

    // r092 loop-local setup helper: exact resource/system call ordering,
    // feature-gated pair, out-handle ownership and tail-return propagation.
    {
    struct C92{std::uint32_t pc{};std::vector<std::uint32_t> args;};struct T92{std::vector<C92> calls;}t92;
    auto cb92=[](void* u,std::uint32_t pc,const std::uint32_t* a,std::size_t n)->std::uint32_t{
        auto* t=static_cast<T92*>(u);t->calls.push_back({pc,std::vector<std::uint32_t>(a,a+n)});
        if(pc==0x417a6fu)return 0xa6f00001u;if(pc==0x417a93u)return 0xa9300002u;if(pc==0x417aebu)return 0xaeb00003u;if(pc==0x42fd90u)return 0x92abcdefu;return 0u;
    };
    PcRuntimeLoopSetupState417a20 st92{};st92.system_token=0x89bd6000u;st92.object_95b218=0x95b21800u;st92.optional_7f94e8=0x7f94e800u;st92.feature_79fb50=3u;st92.mode_78026c=0x11u;st92.global_89f684=7u;st92.global_89f66c=8u;st92.flag_73e2b0=0u;
    require(runtime_loop_setup_417a20(st92,{&t92,cb92})==0x92abcdefu,"r092 setup tail return");
    const std::array<std::uint32_t,13> order92{{0x4041e0u,0x417a36u,0x417a4du,0x417a6fu,0x417a93u,0x46bb90u,0x477ed0u,0x414c00u,0x4227c0u,0x417acau,0x417ad9u,0x417aebu,0x42fd90u}};
    require(t92.calls.size()==order92.size(),"r092 setup call count");for(std::size_t k=0;k<order92.size();++k)require(t92.calls[k].pc==order92[k],"r092 setup call order");
    require(t92.calls[1].args==std::vector<std::uint32_t>({0x89bd6000u,0u,0x625668u,1u}),"r092 setup first system args");
    require(t92.calls[3].args.size()==8u&&t92.calls[3].args[1]==0x80u&&t92.calls[3].args[4]==0x15u&&t92.calls[3].args[6]==0x8a89f4u,"r092 setup primary create args");
    require(t92.calls[4].args.size()==9u&&t92.calls[4].args[1]==0x80u&&t92.calls[4].args[2]==0x80u&&t92.calls[4].args[3]==0x4bu&&t92.calls[4].args[7]==0x8a8a00u,"r092 setup secondary create args");
    require(st92.handle_8a89f4==0xa6f00001u&&st92.handle_8a8a00==0xa9300002u&&st92.handle_89f680==0xaeb00003u,"r092 setup owns returned handles");
    require(st92.global_89f684==0u&&st92.global_89f66c==0u&&st92.flag_73e2b0==1u,"r092 setup final globals");
    t92.calls.clear();st92.feature_79fb50=3u;st92.mode_78026c=0x20u;st92.optional_7f94e8=0u;runtime_loop_setup_417a20(st92,{&t92,cb92});
    require(t92.calls.size()==10u,"r092 setup feature/optional skipped count");
    for(const auto& q:t92.calls)require(q.pc!=0x46bb90u&&q.pc!=0x477ed0u&&q.pc!=0x417ad9u,"r092 setup feature/optional skipped");
    }
    // r091 loop-local cleanup helper: release order, mandatory/optional virtual
    // calls, and exact clearing of only the six owned handle globals.
    {
    struct T91{std::vector<std::array<std::uint32_t,3>> calls;}t91;
    auto cb91=[](void* u,std::uint32_t pc,std::uint32_t token,std::uint32_t slot){
        static_cast<T91*>(u)->calls.push_back({pc,token,slot});
    };
    PcRuntimeLoopCleanupState417970 st91{0x101u,0x202u,0x303u,0x404u,0x505u,0x606u,0x707u,0x808u};
    runtime_loop_cleanup_417970(st91,{&t91,cb91});
    const std::array<std::uint32_t,9> order91{{0x41797fu,0x417994u,0x414c80u,0x4179aeu,0x4179c3u,0x4179d8u,0x4179e9u,0x4179f8u,0x417a07u}};
    require(t91.calls.size()==order91.size(),"r091 cleanup full call count");
    for(std::size_t k=0;k<order91.size();++k)require(t91.calls[k][0]==order91[k],"r091 cleanup full call order");
    require(t91.calls[0][1]==0x101u&&t91.calls[0][2]==8u&&t91.calls[6][1]==0x606u&&t91.calls[6][2]==0x30u&&t91.calls[7][2]==0x28u,"r091 cleanup virtual arguments");
    require(st91.handle_8a89f4==0u&&st91.handle_8a8a00==0u&&st91.handle_95afcc==0u&&st91.handle_95afc4==0u&&st91.handle_95afc8==0u&&st91.handle_89f680==0u,"r091 cleanup clears owned handles");
    require(st91.object_95b218==0x606u&&st91.optional_7f94e8==0x707u,"r091 cleanup preserves borrowed objects");
    t91.calls.clear();st91={0u,0u,0u,0u,0u,0x999u,0u,0u};runtime_loop_cleanup_417970(st91,{&t91,cb91});
    require(t91.calls.size()==2u&&t91.calls[0][0]==0x414c80u&&t91.calls[1][0]==0x4179e9u,"r091 cleanup null optional path");
    }
    // r049 direct GamePlCar child closures: time-up braking, wanderer, NOS and service leaves.
    {
    std::array<std::uint8_t,event_size> e49raw{};std::array<std::uint8_t,work_size> w49raw{};std::array<std::uint8_t,parameter_size> p49raw{};
    Bytes e49(e49raw.data(),e49raw.size()),w49(w49raw.data(),w49raw.size()),p49(p49raw.data(),p49raw.size());
    e49.put16(0x202,0x7fffu);e49.puti(0x38,3);e49.puti(0x34,99);control_timeup_braking_49fb70(e49,w49,p49,0);
    require(std::uint16_t(e49.i16(0x202))==0u&&e49.i32(0x38)==255&&e49.i32(0x34)==0,"timeup zero-counter braking");
    e49.puti(0x38,2);e49.putf(0x1c4,0.0f);e49.putf(0x1c8,0.0f);e49.put16(0xd4c,0);p49.putf(0x17c0,100.0f);
    w49.putf(0x5c,10.0f);w49.putf(0x60,0.0f);w49.putf(0x64,0.0f);w49.putf(0x628,0.0f);w49.putf(0x62c,1.0f);w49.putf(0x630,0.0f);
    control_timeup_braking_49fb70(e49,w49,p49,20);require(e49.i32(0x38)==10,"timeup minimum brake");
    require(std::fabs(w49.f32(0x5c)-9.025f)<1e-5f,"timeup longitudinal/lateral decay");

    e49.put32(0x1f4,0u);e49.put16(0x2e,0);e49.putf(0x178,0.0f);p49.put32(0x10a0,7u);p49.putf(0x1644,12.5f);p49.putf(0xb48,2.0f);p49.putf(0xb94,4.0f);
    ham_nos_set_speed_4a5650(e49,w49,p49,10.0f);require(e49.u32(0x208)==7u&&e49.i32(0x20c)==119&&e49.i32(0x210)==119&&e49.i32(0x48)==119,"NOS parameter/ftol writes");
    require(e49.f32(0x24)==0.0f&&e49.f32(0x28)<0.0f&&e49.i32(0x3c)==255,"NOS heading velocity/brake state");
    require(std::fabs(w49.f32(0x424)-w49.f32(0x330))<1e-6f&&std::fabs(w49.f32(0x60c)-w49.f32(0x518))<1e-6f,"NOS axle speed pairs");

    e49.put32(0x244,0u);e49.put32(0x26c,0x3f000000u);e49.puti(0xe88,10);e49.put16(0xd4c,0);e49.put8(0x283,0);e49.putf(0x2c8,0);e49.putf(0x2f8,0);e49.put8(0xd36,0);
    check_wanderer_4a5260(e49,w49,PcCheckWandererInputs{1,0});require(e49.u32(0xe90)==1u&&e49.u32(0xe94)==0x3f000000u&&e49.i32(0xe88)==12&&e49.i32(0xe8c)==12,"wanderer activation/timer");
    e49.put32(0x244,1u);w49.putf(0x5c,1.0f);w49.putf(0x60,0.0f);w49.putf(0x64,0.0f);w49.putf(0x628,0.0f);w49.putf(0x62c,1.0f);w49.putf(0x630,0.0f);e49.putf(0x264,2.0f);e49.putf(0x268,3.0f);
    check_wanderer_4a5260(e49,w49,PcCheckWandererInputs{1,0});require(e49.u32(0xe90)==0u&&e49.i32(0xe88)==11&&std::isfinite(e49.f32(0xe98)),"wanderer release geometry");

    std::array<std::uint8_t,0x80> netraw{};Bytes net(netraw.data(),netraw.size());net.put32(0x60,0x13579bdfu);require(network_tail_state_55a930(net)==0x13579bdfu,"network tail getter");
    struct Net49{unsigned calls{};std::size_t a{},b{};} n49;auto ncb=[](void* u,Bytes a,Bytes b){auto* n=static_cast<Net49*>(u);++n->calls;n->a=a.size();n->b=b.size();};
    network_tail_forward_46c390(e49,w49,{&n49,ncb});require(n49.calls==1&&n49.a==event_size&&n49.b==work_size,"network forwarding wrapper");
    struct Rank49{std::uint8_t seen{};} r49;auto rcb=[](void* u,std::uint8_t id)->std::uint32_t{auto* r=static_cast<Rank49*>(u);r->seen=id;return 0x12340000u+id+7u;};
    require(rank_provider_gateway_45a2b0(5u,{&r49,rcb})==12u&&r49.seen==5u,"rank provider gateway ABI");
    }
    // r043 post-ghost batch: two session gates, two curvature helpers and othcarGetR.
    require(session_mode4_4962a0(PcSessionModeInputs{true,true,4u}),"session mode4 true");
    require(!session_mode4_4962a0(PcSessionModeInputs{true,true,6u}),"session mode4 false");
    require(session_mode6_4962d0(PcSessionModeInputs{true,true,6u}),"session mode6 true");
    require(!session_mode6_4962d0(PcSessionModeInputs{false,true,6u}),"session inactive false");
    const CourseProbe r0{0,0,0},r1{1,0,1},r2{2,0,0};
    require(std::fabs(othcar_calc_r_46f040(r0,r1,r2)+1.0f)<1e-6f,"othcar radius signed");
    require(std::fabs(othcar_calc_r_alt_46f190(r0,r1,r2)+1.0f)<1e-6f,"othcar alt radius signed");
    require(othcar_calc_r_46f040({0,0,0},{1,0,0},{2,0,0})==-100000.0f,"othcar straight normal sentinel");
    require(othcar_calc_r_alt_46f190({0,0,0},{1,0,0},{2,0,0})==100000.0f,"othcar straight alt sentinel");
    PcOthcarGetRInputs oth{};oth.max_cs_len=200;oth.road_ok={true,true,true};oth.next_ok={true,true};oth.centers={r0,r1,r2};
    e.put8(0xb52,0);e.putf(0xb54,7.0f);e.put32(0x5c,0);e.put16(0x64,20);e.put8(4,0);othcar_get_r_479670(e,oth);
    require(e.u8(0xb52)==4&&std::fabs(e.f32(0xb54)+1.0f)<1e-6f,"othcarGetR success");
    e.put8(0xb52,2);e.putf(0xb54,123.0f);othcar_get_r_479670(e,oth);require(e.u8(0xb52)==1&&e.f32(0xb54)==123.0f,"othcarGetR throttle");
    e.put8(0xb52,0);e.put16(0x64,198);othcar_get_r_479670(e,oth);require(e.f32(0xb54)==100000.0f,"othcarGetR end gate");
    // r042 CalcLightRate: stable-heading reset plus bounded matrix correction path.
    std::array<std::uint8_t,128> light_matrix{};PcMatrixStack light_ms{Bytes(light_matrix.data(),light_matrix.size()),0,0,2};
    for(unsigned k=0;k<16;++k)e.putf(0x70+k*4u,0.0f);e.putf(0x70,1.0f);e.putf(0x84,1.0f);e.putf(0x98,1.0f);e.putf(0xac,1.0f);
    e.put8(0x283,0);e.put16(0xd50,7);e.put16(0xd4c,0);e.put16(0x160,0);e.put16(0x2e,0);
    e.putf(0x20,1.0f);e.putf(0x24,0.0f);e.putf(0x28,0.0f);calc_light_rate_4a3d40(e,light_ms);
    require(e.i16(0xd50)==0,"light-rate counter reset");
    e.put16(0xd50,0);e.put16(0x160,0x4000);e.put16(0x2e,0x4000);calc_light_rate_4a3d40(e,light_ms);
    require(e.i16(0xd50)==1&&light_ms.depth==0&&light_ms.current_offset==0,"light-rate active path and stack restore");
    // r042 RecordGhostCar: explicit 30-word history and platform steering state.
    std::array<std::uint8_t,0x3c> ghost_hist_raw{};Bytes ghost_hist(ghost_hist_raw.data(),ghost_hist_raw.size());
    for(unsigned k=0;k<30;++k)ghost_hist.put16(k*2u,std::uint16_t(std::int16_t(k-15)));
    e.put16(0x32,100);e.put16(0x202,300);e.put16(0x204,0);
    record_ghost_car_4a4710(e,ghost_hist,PcRecordGhostInputs{0,0,0});
    require(ghost_hist.i16(0)==-600&&ghost_hist.i16(2)==-15,"ghost history shift/sample");
    require(e.i16(0x204)==-20,"ghost moving average");
    // r041 CalcOfsLeftLane: nearest controlled road point and >200/smoothed output paths.
    PcOfsLeftLaneInputs lane_in{};lane_in.selector=2;lane_in.rates={0.1f,0.2f,0.3f,0.4f};
    lane_in.points={CourseProbe{10,0,0},CourseProbe{3,0,0},CourseProbe{1,0,0},CourseProbe{5,0,0}};
    e.putf(0x14,0);e.putf(0x18,0);e.putf(0x1c,0);e.putf(0x58,0);e.put32(0x1f4,201u);
    calc_ofs_left_lane_4a45f0(e,lane_in);require(e.f32(0x58)==0.3f,"lane nearest direct rate");
    e.putf(0x58,0);e.put32(0x1f4,200u);calc_ofs_left_lane_4a45f0(e,lane_in);
    require(std::fabs(e.f32(0x58)-0.12f)<1e-6f,"lane smoothed rate");
    lane_in.selector=-1;e.putf(0x58,-5.0f);calc_ofs_left_lane_4a45f0(e,lane_in);require(e.f32(0x58)==1.0f,"lane neutral selector");
    // r040 CalcVibrateMatrix: explicit per-car 30-float history, exact shift shape.
    std::array<std::uint8_t,0x78u*2u> vibrate_bytes{};Bytes vibrate(vibrate_bytes.data(),vibrate_bytes.size());
    for(unsigned i=0;i<30;++i)vibrate.putf(i*4u,float(i+1u));
    e.put32(0,8u);e.put32(4,1u);e.putf(0x1c8,0.5f);e.putf(0x1cc,-123.0f);
    calc_vibrate_matrix_4a2d70(e,vibrate);
    require(vibrate.f32(0)==500.0f&&vibrate.f32(4)==1.0f&&vibrate.f32(29u*4u)==29.0f,"vibrate history shift");
    require(e.f32(0x1cc)>31.16f&&e.f32(0x1cc)<31.17f,"vibrate moving average");
    e.put32(4,0u);const auto frozen=bits(vibrate.f32(0));e.putf(0x1cc,7.0f);calc_vibrate_matrix_4a2d70(e,vibrate);
    require(bits(vibrate.f32(0))==frozen&&e.f32(0x1cc)==7.0f,"vibrate inactive no-op");
    for(int i=-256;i<512;++i)require(brake_pressure(i,t)==float(std::max(0,std::min(255,i)))*0.25f,"brake clamp");
    require(engine_friction(e,p,t,-10)==engine_friction(e,p,t,0),"negative speed clamp");
    require(engine_friction(e,p,t,2000)==engine_friction(e,p,t,1000),"upper friction clamp");
    e.put8(0x13,2);throws([&]{(void)engine_friction(e,p,t,10);});
    e.put32(0xe84,1);require(std::isfinite(engine_friction(e,p,t,10)),"override chooses table zero");e.put8(0x13,0);e.put32(0xe84,0);
    // Kill/reset path must write all required clutch fields, not all event bytes.
    e.put32(4,8);e.puti(0x3c,255);e.put8(0x296,4);auto_clutch_control(e,w,p);
    require(e.i32(0x3c)==0&&e.u8(0x296)==0&&e.u8(0x297)==1,"reset clutch states");
    require(e.f32(0x22c)==0&&e.f32(0x228)==0,"reset clutch outputs");
    e.put32(4,0);e.put32(0x1f4,1);e.put8(0x296,4);w.putf(0x524,0);w.putf(0x618,0);e.puti(0x3c,2);
    auto_clutch_control(e,w,p);require(e.i32(0x3c)==0,"slip clutch lower clamp");
    w.putf(0x524,2);w.putf(0x618,2);e.puti(0x3c,254);auto_clutch_control(e,w,p);
    require(e.i32(0x3c)==255&&e.f32(0x22c)==1.0f,"slip clutch upper clamp");
    // Preserve x86 signed wrap at INT_MAX, not undefined C++ signed overflow.
    e.put8(0x296,0);e.put8(0x297,0);e.puti(0x3c,0x7fffffff);auto_clutch_control(e,w,p);
    require(e.u32(0x3c)==0x80000009u,"clutch wrap");
    for(int angle:{-8193,-8192,-8191,0,8191,8192,8193}){
        e.put32(0x304,5);e.put32(0x308,10);w.put16(0x52e,static_cast<std::uint16_t>(angle));w.put16(0x622,0);
        for(auto o:{0x318,0x40c,0x500,0x5f4})w.putf(o,100);
        induce_spin(e,w);require(e.u32(0x304)==4,"spin timer decrement");
        require(w.f32(0x318)==50&&w.f32(0x40c)==50,"front grip scaling");
        require(w.f32(0x500)==(std::abs(angle)<8192?50:100),"rear angle cutoff");
    }
    e.put32(0x304,1);e.put32(0x308,0);throws([&]{induce_spin(e,w);});e.put32(0x304,0);
    auto wheels=std::array<Bytes,4>{w.sub(0x258,0xf4),w.sub(0x34c,0xf4),w.sub(0x440,0xf4),w.sub(0x534,0xf4)};
    p.putf(0xe40,2);p.putf(0xed8,1);p.putf(0xe8c,2);p.putf(0xf24,1);
    for(auto q:wheels){q.putf(0x34,1000);q.putf(0x38,1000);q.putf(0xe8,1);}
    e.put32(4,0);e.put8(0x283,0);tire_grip(e,w,p,wheels);
    for(auto q:wheels)require(q.f32(0xc0)==1000,"load-equal tire grip");
    e.put8(0x283,1);tire_grip(e,w,p,wheels);for(auto q:wheels)require(q.f32(0xc0)==0,"disabled grip");
    wheels[0].putf(0x38,0);throws([&]{tire_grip(e,w,p,wheels);});wheels[0].putf(0x38,1000);
    // Core torque exhaustive throttle range, finite synthetic table and parameters.
    e.puti(0x3c,255);e.putf(0x22c,1);e.putf(0x21c,500);e.putf(0xe6c,0);
    for(int pedal=0;pedal<=255;++pedal){e.puti(0x34,pedal);e.putf(0x2a0,1);engine_torque(e,p,t);
        require(std::isfinite(e.f32(0x214)),"finite torque");require(e.f32(0x2a0)>=0&&e.f32(0x2a0)<=1,"limiter bounds");}
    e.puti(0x34,-1);throws([&]{engine_torque(e,p,t);});e.puti(0x34,256);throws([&]{engine_torque(e,p,t);});e.puti(0x34,100);
    e.putf(0x21c,500);require(predicted_engine_speed(e,w,p,0)==500,"neutral speed helper");
    throws([&]{(void)predicted_engine_speed(e,w,p,17);});
    e.put8(0x296,0);e.puti(0x34,123);accel_operation(e,w,p);require(e.i32(0x29c)==123,"accel default records input");
    e.put8(0x296,1);e.puti(0x29c,40);e.puti(0x3c,255);accel_operation(e,w,p);require(e.i32(0x34)==0&&e.i32(0x29c)==0,"accel upshift floor");
    std::cout<<"driving: "<<assertions<<" checks passed (synthetic host unit tests; binary differential tests separate)\n";
    return 0;
}catch(const std::exception& x){std::cerr<<"FAIL after "<<assertions<<" checks: "<<x.what()<<"\n";return 1;}}

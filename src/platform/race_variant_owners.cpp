// Owners of the race variants without a menu path in the retail frontend
// (race_variant_owners.hpp): variant 8, event 0x194 / function 0x23, and
// variant 9 (Race Attack), event 0x192 / function 0x21.
#include "platform/race_variant_owners.hpp"
#include "platform/native_runtime.hpp"
#include "platform/race_manager.hpp"
#include "platform/race_manager_runtime.hpp"
#include "platform/race_end_runtime.hpp"
#include "platform/race_area_runtime.hpp"
#include "platform/race_traffic_runtime.hpp"
#include "platform/racer_setup.hpp"
#include "platform/retail_asset_store.hpp"
#include "platform/frontend_ui_resources.hpp"
#include "platform/frontend_text.hpp"
#include "platform/pc_scene_renderer.hpp"
#include "platform/pc_render_queue.hpp"
#include "platform/vehicle_model_draw.hpp"
#include "driving/pc_course_query.hpp"
#include "driving/pc_car_services.hpp"
#include "driving/pc_common_control.hpp"
#include "driving/pc_x87.hpp"
#include <array>
#include <cstdio>
#include <cstring>
#include <map>
#include <stdexcept>
namespace outrun::platform {
namespace {
// Variant 8 state (.bss 836154..8361BB; 836174 is the frontend's code, 8361B4 the route gate).
struct Variant8 {
    std::uint32_t by_sector_836154[4]{};   // points by sector (450650 sector 0..3)
    std::uint32_t loader_836164{},loader_836168{},phase_83616c{1};   // 48B550 loader of the course
    std::uint32_t by_stage_836178[15]{};    // points by stage (836178 also the last total, 495688)
    std::uint32_t total_8361b8{};           // points of the current stage
};
Variant8& variant8(NativeRuntimeContext& c){static std::map<const NativeRuntimeContext*,Variant8> s;return s[&c];}
void set_gate_8361b4(NativeRuntimeContext& c,std::uint8_t v){c.start_mode.route_gate_8361b4=v;c.race.car_world.gate_8361b4=v;}
float f32(const std::uint8_t* p){float v;std::memcpy(&v,p,4);return v;}
// 495740 (function 0x23 init): gate 8361B4 = 1, loader phase 1, every count cleared.
void variant8_init_495740(NativeRuntimeContext& c){
    set_gate_8361b4(c,1);
    auto& v=variant8(c);v=Variant8{};v.phase_83616c=1u;
}
// 4960A0(player): the speed (+1C4 * 216.72) above the drift limit of the
// car's parameters (+134C), x0.95 unless the selected Races record is of type 3.
bool drifting_4960a0(NativeRuntimeContext& c){
    const auto& car=c.event_function36.car_select.car_799d18;
    const auto& params=c.event_function36.car_select.parameters;
    const float speed=f32(car.data()+0x1c4)*216.720001f;
    float limit=(f32(params.data()+0x134c)-2.29999995f)*-131.386780f+318.0f;
    bool type3=false;
    const auto* races=c.start_mode.scene_owner_race_assets;const auto i=c.mission.manager.record_83637c;
    if(races&&i>=0&&std::uint32_t(i)<races->race_count){
        const std::size_t at=races->races_offset+std::size_t(i)*0x44u+0x20u;
        if(at+4u<=races->bytes.size()){std::uint32_t t;std::memcpy(&t,races->bytes.data()+at,4);type3=t==3u;}
    }
    if(!type3)limit=limit*0.949999988f;
    return speed>limit;
}
// 495790 -> 495520 (function 0x23 control): the course of the code 836174 (48B550 on
// the loader 836164); in the race (mode 16, not over), while the car drifts
// (+D36 > 0) faster than 10: one point (two above the drift limit) for the
// sector, the stage and the stage total.
void variant8_control_495520(NativeRuntimeContext& c){
    auto& v=variant8(c);
    if(!native_goal_course_48b550(c,c.start_mode.frontend_prepare.alternate_code_836174,0,v.phase_83616c))return;
    if(c.mode_state.current!=0x10u)return;
    auto& ms=c.race.manager.state;
    if(race_over_state_450240(ms))return;
    const auto& car=c.event_function36.car_select.car_799d18;
    if(std::int8_t(car[0xd36])<=0)return;
    if(!(f32(car.data()+0x1c4)*216.720001f>10.0f))return;          // comiss: jbe skips
    std::uint32_t stage=ms.u32(0x7d3944u),sector=ms.u32(0x7d3874u);   // 450650
    if(sector==3u){++stage;sector=0;}
    const std::uint32_t points=drifting_4960a0(c)?2u:1u;
    if(sector<4u)v.by_sector_836154[sector]+=points;               // 450650 keeps both in range on the PC
    v.total_8361b8+=points;
    if(stage<15u)v.by_stage_836178[stage]+=points;
}
// 4957A0 (function 0x23 destroy): gate cleared, the course script lists released (4F1860),
// the loader back to phase 1.
void variant8_destroy_4957a0(NativeRuntimeContext& c){
    set_gate_8361b4(c,0);
    c.event_function36.frontend_course_tables={};
    auto& v=variant8(c);v.phase_83616c=1u;v.loader_836164=0;v.loader_836168=0;
}
// 4483D0(code, points, a, b, course) (thiscall 7B17F8, read from the Steam build): one
// entry into the code's table of ten 0x1C-byte entries (+0 name, +10 car, +14 points,
// +18 flags) at 7B17F8 + C494 + code*0x118, highest first. The launcher check of the
// PC (the protection's named object) is not reproduced (as 447750).
bool record_insert_4483d0(NativeRuntimeContext& c,std::uint32_t code,std::uint32_t points,std::uint32_t a,
                          std::uint32_t b,std::uint32_t course){
    auto& common=c.event_function36.frontend_profiles.common;
    const std::size_t base=0xc494u+std::size_t(code)*0x118u;
    if(base+10u*0x1cu>common.size())throw std::out_of_range("4483D0: record table outside the common save");
    auto u32=[&](std::size_t at){std::uint32_t x;std::memcpy(&x,common.data()+at,4);return x;};
    auto entry=[&](std::int32_t k){return base+std::size_t(k)*0x1cu;};
    if(u32(entry(9)+0x14u)>points)return false;
    std::int32_t k=9;
    while(k>=0&&!(u32(entry(k)+0x14u)>points))--k;
    if(k==0)native_race_end(c).state.miles[0x84bcfcu-0x84b900u]=1u;   // 4EF430
    for(std::int32_t j=9;j>=k+2;--j)std::memcpy(common.data()+entry(j),common.data()+entry(j-1),0x1cu);
    const std::size_t r=entry(k+1);
    std::memcpy(common.data()+r+0x14u,&points,4);
    common[r+0x18u]=std::uint8_t((((b&1u)|((course&0xffu)<<1))<<1)|(a&1u));
    const auto& license=c.event_function36.frontend_profiles.active;
    common[r+0x10u]=license[0x7c23fcu-0x7c23e0u];
    for(std::size_t i=0;;++i){const auto ch=license[i];common[r+i]=ch;if(!ch)break;}   // strcpy (license name)
    return true;
}
// 448230(k, points, stages, a, b, course, time) (thiscall 7B17F8): the multi-stage twin of
// 4483D0: table k at 7B17F8 + 9F14 + k*0x1E0, ten 0x30-byte entries (+0 name, +10 car,
// +14 points, +18 the five stage counts, +2C flags), highest first. The launcher check
// (the protection's named object) is not reproduced, as in 4483D0; time is not stored.
bool record_insert_448230(NativeRuntimeContext& c,std::uint32_t k,std::uint32_t points,const std::uint32_t* stages,
                          std::uint32_t a,std::uint32_t b,std::uint32_t course){
    auto& common=c.event_function36.frontend_profiles.common;
    const std::size_t base=0x9f14u+std::size_t(k&0xffu)*0x1e0u;
    if(base+10u*0x30u>common.size())throw std::out_of_range("448230: record table outside the common save");
    auto u32=[&](std::size_t at){std::uint32_t x;std::memcpy(&x,common.data()+at,4);return x;};
    auto entry=[&](std::int32_t j){return base+std::size_t(j)*0x30u;};
    if(u32(entry(9)+0x14u)>points)return false;
    std::int32_t j=9;
    while(j>=0&&!(u32(entry(j)+0x14u)>points))--j;
    if(j==0)native_race_end(c).state.miles[0x84bcfcu-0x84b900u]=1u;   // 4EF430
    for(std::int32_t i=9;i>=j+2;--i)std::memcpy(common.data()+entry(i),common.data()+entry(i-1),0x30u);
    const std::size_t r=entry(j+1);
    std::memcpy(common.data()+r+0x14u,&points,4);
    std::memcpy(common.data()+r+0x18u,stages,5u*4u);
    common[r+0x2cu]=std::uint8_t((((b&1u)|((course&0xffu)<<1))<<1)|(a&1u));
    const auto& license=c.event_function36.frontend_profiles.active;
    common[r+0x10u]=license[0x7c23fcu-0x7c23e0u];
    for(std::size_t i=0;;++i){const auto ch=license[i];common[r+i]=ch;if(!ch)break;}   // strcpy (license name)
    return true;
}
// ---- variant 9 (Race Attack, event 0x192 / function 0x21) ----
// RaceAttack.bin relocated as 4F12A0 leaves it, at a synthetic base next to Races.bin.
constexpr std::uint32_t RaceAttackBase=0x31000000u;
constexpr std::uint32_t RaceAttackRecordBytes=0x4cu;
constexpr std::uint32_t Data686254=0x686254u;
// .data 686274: the rival speed factors by rank (4B01A0), 0.0 after the sixth.
constexpr float RankFactor686274[7]{0.600000024f,0.5f,0.400000006f,0.300000012f,0.200000003f,0.100000001f,0.0f};
// Variant 9 state: .bss 8421A0..8421E0 (8421C0 is start_mode.scene_state_8421c0), the
// 8421E0 message resource, 842280 and the .data words 686254..686273.
struct Variant9 {
    std::vector<std::uint8_t> blob;            // handle 8421B0
    std::uint32_t records_8421d0{},count_8421cc{},record_8421d4{};
    std::uint32_t level_8421d8{},timer_842280{};
    std::uint8_t boost_8421c4{};
    // 686254 record index, 686258 target segment, 68625C last rank, 686260.. rank by stage
    std::array<std::uint8_t,0x20> data_686254{};
    std::array<std::uint8_t,0xa0> ui_8421e0{};
    bool ui_constructed{},load_reported{};
    Variant9(){put(0x686254u,0xffffffffu);put(0x686258u,0xffffffffu);for(std::uint32_t a=0x68625cu;a<0x686274u;a+=4)put(a,7u);}
    std::uint32_t get(std::uint32_t a)const{std::uint32_t v;std::memcpy(&v,data_686254.data()+(a-Data686254),4);return v;}
    void put(std::uint32_t a,std::uint32_t v){std::memcpy(data_686254.data()+(a-Data686254),&v,4);}
    std::uint8_t* at(std::uint32_t pc_address,std::size_t n){
        const std::uint32_t off=pc_address-RaceAttackBase;
        if(pc_address<RaceAttackBase||off>blob.size()||blob.size()-off<n)throw std::out_of_range("RaceAttack record outside the file");
        return blob.data()+off;}
    std::uint32_t field(std::uint32_t rec,std::uint32_t o){std::uint32_t v;std::memcpy(&v,at(rec+o,4),4);return v;}
    void set_field(std::uint32_t rec,std::uint32_t o,std::uint32_t v){std::memcpy(at(rec+o,4),&v,4);}
};
Variant9& variant9(NativeRuntimeContext& c){static std::map<const NativeRuntimeContext*,Variant9> s;return s[&c];}
const auto& player_car(NativeRuntimeContext& c){return c.event_function36.car_select.car_799d18;}
std::int32_t player_segment(NativeRuntimeContext& c){std::int16_t v;std::memcpy(&v,player_car(c).data()+0x64,2);return v;}
// 44B780: the segment count of the course section ([[7D3188]+14]+7C), 0 without one.
std::int32_t segments_44b780(NativeRuntimeContext& c){
    if(!native_race_area_bind_records(c))throw std::runtime_error("44B780: course record table (7D33BC) not identified");
    auto& m=native_race_area_memory(c,nullptr,false);
    const std::uint32_t sel=m.u32(0x7d3188u);
    return sel?std::int32_t(m.i16(m.u32(sel+0x14u)+0x7cu)):0;
}
// 4B0040 (init 4B0400): state 1, the record cleared, ranks 7, 43F940(9).
void variant9_init_4b0040(NativeRuntimeContext& c){
    auto& v=variant9(c);
    v.records_8421d0=v.count_8421cc=v.record_8421d4=0;v.level_8421d8=0;v.boost_8421c4=0;
    c.start_mode.scene_state_8421c0=1u;c.start_mode.scene_state_8421c0_known=true;
    v.put(0x686254u,0xffffffffu);
    for(std::uint32_t a=0x68625cu;a<0x686274u;a+=4)v.put(a,7u);
    c.game_mode.game_variant=9u;
}
// The current record of index i (4B0110 / 4B0590 state 2): its target segment +18
// (-1: ten before the course section's end), then +18 = -1.
void select_record(NativeRuntimeContext& c,std::uint32_t i){
    auto& v=variant9(c);
    if(i>=v.count_8421cc){v.record_8421d4=0;return;}
    const std::uint32_t rec=v.records_8421d0+i*RaceAttackRecordBytes;
    v.record_8421d4=rec;
    const std::uint32_t t=v.field(rec,0x18u);
    v.put(0x686258u,t==0xffffffffu?std::uint32_t(segments_44b780(c)-10):t);
    v.set_field(rec,0x18u,0xffffffffu);
}
// 4B02D0(1): 4F12A0("\\Scripts\\Bin\\RaceAttack.bin", 8421B0), then the category
// RaceAttack_Outrun2Course_Races (686298[1]): records 8421D0, count 8421CC.
bool load_4b02d0(NativeRuntimeContext& c){
    auto& v=variant9(c);
    std::vector<std::uint8_t> file;std::string err;
    auto* store=c.event_function36.retail_assets;
    if(!store||!retail_asset_read_relative(*store,"Scripts/bin/RaceAttack.bin",file,1u<<20,&err)){
        if(!v.load_reported){v.load_reported=true;std::fprintf(stderr,"[variant9] RaceAttack.bin not loaded: %s\n",store?err.c_str():"no retail store");}
        return false;}
    driving::PcRelocCategoryBlobR077 blob{};
    std::vector<std::uint8_t> view(file);
    if(!driving::course_reloc_blob_open_r077(view.data(),view.size(),blob)||!pc_relocate_blob(file,RaceAttackBase,v.blob))
        throw std::runtime_error("4F12A0: RaceAttack.bin is not a category file");
    const char* name="RaceAttack_Outrun2Course_Races";
    const auto* p=static_cast<const std::uint8_t*>(driving::runtime_category_records_4f1a90(name,blob));
    v.records_8421d0=p?RaceAttackBase+std::uint32_t(p-view.data()):0u;
    if(p){
        const auto n=driving::runtime_category_count_4f1ba0(name,blob);
        v.count_8421cc=n>0?std::uint32_t(n):0u;
        if(std::size_t(p-view.data())+std::size_t(v.count_8421cc)*RaceAttackRecordBytes>view.size())
            throw std::runtime_error("4F1BA0: RaceAttack records outside the file");
    }
    return true;
}
std::array<std::uint8_t,0x40>& score_stack(){static std::array<std::uint8_t,0x40> b{};return b;}
// 47DA40: the racers' cars released (4763D0 on the traffic module), then 47D970: the
// rival factors 505370(+76, +20) of the racers with +60 set, the racer tables freed.
void racers_release_47da40(NativeRuntimeContext& c,driving::PcMatrixStack& st){
    auto& r=c.mission.racers;
    for(std::uint32_t i=0;i<r.count_80fb04;++i){
        std::uint32_t id;std::memcpy(&id,r.racers_80fb00.data()+std::size_t(i)*RacerRecordBytes+0x4cu,4);
        if(id!=0xffffffffu&&!native_race_score(c,st,0x4763d0u,id))throw std::runtime_error("47DA40: 4763D0 refused");
    }
    r.special_80fb28=0;c.start_mode.scene_owner_special_driver_80fb28=0;
    r.table_80fb20.clear();r.table_80fb1c.clear();r.table_80fb30.clear();
    // 4957F0 -> 496320 belongs to the mission selection (variant 6), not set in variant 9
    for(std::uint32_t i=0;i<r.count_80fb04&&!r.racers_80fb00.empty();++i){
        const auto* p=r.racers_80fb00.data()+std::size_t(i)*RacerRecordBytes;
        std::uint32_t used;std::memcpy(&used,p+0x60,4);
        if(!used)continue;
        const std::uint8_t k=p[0x76];
        if(k>=r.rival_8514a4.size())throw std::out_of_range("505370: rival index outside 8514A4");
        std::memcpy(&r.rival_8514a4[k],p+0x20,4);
    }
    r.racers_80fb00.clear();r.count_80fb04=0;
    c.race.car_world.gate_80fb14=0;r.config_80fb0c=0;
}
// 4B02B0: 47D950(record) = 47CF40(record, 1) then 47D8B0 (the racers on the course).
void racers_spawn_4b02b0(NativeRuntimeContext& c,driving::PcMatrixStack& st){
    auto& v=variant9(c);
    if(!v.record_8421d4)return;
    PcAddressView view;view.add(RaceAttackBase,v.blob.data(),v.blob.size());view.add_exe();
    RacerSetupServices rs;rs.view=&view;
    const auto& car=player_car(c);
    rs.car_present=car.size()>0x260;if(rs.car_present){rs.car_byte11=std::int8_t(car[0x11]);std::memcpy(&rs.car_word25e,car.data()+0x25e,2);}
    rs.network_686258=v.get(0x686258u);
    rs.selection_836374=c.start_mode.selection_active_836374;
    const auto* text=c.event_function36.frontend_text;{const auto* t=text?text->get(0x3d7):nullptr;rs.text_3d7=t?*t:std::string();}
    if(!racer_setup_47cf40(c.mission.racers,rs,v.record_8421d4,true))
        throw std::runtime_error("47CF40: racer setup stopped at "+std::to_string(rs.missing));
    c.start_mode.scene_owner_special_driver_80fb28=std::int32_t(c.mission.racers.special_80fb28);
    if(!native_race_traffic_hook(c,0x47d8b0u,st))throw std::runtime_error("47D8B0 not run by the traffic module");
}
FrontendUiResources ui_resources(NativeRuntimeContext& c){
    auto& st=c.event_function36;
    FrontendUiResources ui{st.frontend_sprites};
    ui.motion_step=st.frontend_ui_motion_step;ui.pause_domain=st.title_pause_flag_95b214;
    ui.effect_user=st.frontend_effect_user;ui.effect_4249f0=st.frontend_effect;
    return ui;
}
bool ui_call(FrontendUiResources& ui,Variant9& v,std::uint32_t pc,std::initializer_list<std::uint32_t> a={}){
    std::array<std::uint32_t,11> args{};std::size_t n=0;for(auto x:a)args[n++]=x;std::uint32_t r{};
    return ui.call(pc,driving::Bytes(v.ui_8421e0.data(),v.ui_8421e0.size()),n?args.data():nullptr,n,r)&&!ui.missing_pc;
}
// 4B01E0: the level message 2B0040 + level - 1 (465860(token, -1, -1, 0xA, 3, 0, 0, 1, 1, 1, 0), 465970).
void level_message_4b01e0(NativeRuntimeContext& c,FrontendUiResources& ui){
    auto& v=variant9(c);
    if(v.level_8421d8<1u||v.level_8421d8>3u)return;
    const std::uint32_t one=0x3f800000u;
    if(!ui_call(ui,v,0x465860u,{0x2b0040u+v.level_8421d8-1u,~0u,~0u,0xau,3u,0u,0u,one,one,one,0u})||!ui_call(ui,v,0x465970u))
        throw std::runtime_error("4B01E0: message resource refused");
}
// 4B0590 (control 4B0780).
void variant9_control_4b0590(NativeRuntimeContext& c,driving::PcMatrixStack& st){
    auto& v=variant9(c);auto& state=c.start_mode.scene_state_8421c0;
    if(std::int32_t(v.timer_842280)>0)--v.timer_842280;
    auto ui=ui_resources(c);
    if(!v.ui_constructed){std::uint32_t r{};ui.call(0x465160u,driving::Bytes(v.ui_8421e0.data(),v.ui_8421e0.size()),nullptr,0,r);v.ui_constructed=true;}
    if(!ui_call(ui,v,0x4659f0u))throw std::runtime_error("4659F0: message resource refused");
    const auto& car=player_car(c);
    switch(state){
    case 1:state=2u;return;
    case 2:
        if(load_4b02d0(c))state=3u;
        v.put(0x686254u,0u);
        if(v.count_8421cc)select_record(c,0);else v.record_8421d4=0;
        return;
    case 3:
        if(!v.record_8421d4||player_segment(c)<=0)return;
        racers_spawn_4b02b0(c,st);state=4u;return;
    case 4:{
        std::int32_t limit=std::int32_t(v.field(v.record_8421d4,0x14u))-7;
        if(limit<10)limit=10;
        if(player_segment(c)<limit)return;
        if(!native_race_traffic_hook(c,0x4b0330u,st))throw std::runtime_error("4B0330 not run by the traffic module");
        ++v.timer_842280;state=5u;v.boost_8421c4=1;return;}
    case 5:{
        if(player_segment(c)>=std::int32_t(v.get(0x686258u))){
            const std::uint32_t rank=car[0xc36];
            if(v.get(0x686254u)>=1u){
                std::uint32_t lv=v.level_8421d8;
                if(lv<2u?rank<=2u:rank==1u)v.level_8421d8=lv+1u;
                else if(rank<=4u&&lv!=0u)v.level_8421d8=lv-1u;
            }
            std::uint32_t key;std::memcpy(&key,car.data()+0x68,4);
            const std::uint32_t stage=race_area_value_44c940(native_race_area_memory(c,nullptr,false),key);
            if(stage>=5u)throw std::out_of_range("4B0590: stage rank outside 686260");
            v.put(0x686260u+stage*4u,rank);v.put(0x68625cu,rank);
            racers_release_47da40(c,st);
            v.record_8421d4=0;
            if(v.get(0x686254u)<10u){state=6u;v.timer_842280=0x96u;return;}
            v.put(0x686254u,3u);
        }
        if(v.timer_842280==0u)v.boost_8421c4=0;
        return;}
    case 6:
        if(v.timer_842280!=0u)return;
        level_message_4b01e0(c,ui);state=3u;return;
    default:return;
    }
}
// 4B0410 (destroy 4B07A0): the racers of a running attack released, the scripts (4F1860)
// and every word cleared.
void variant9_destroy_4b0410(NativeRuntimeContext& c,driving::PcMatrixStack& st){
    auto& v=variant9(c);
    if(c.start_mode.scene_state_8421c0==5u)racers_release_47da40(c,st);
    c.start_mode.scene_state_8421c0=0u;c.start_mode.scene_state_8421c0_known=true;
    v.count_8421cc=v.records_8421d0=v.record_8421d4=0;v.put(0x686254u,0xffffffffu);
    c.event_function36.frontend_course_tables={};v.blob.clear();
    v.level_8421d8=0;v.boost_8421c4=0;
}
// ---- variant 9 display (4B0790 -> 4B0490) ----
// .data 64A9E8 / 64AA00: the road marker objects by language (45AF80 trigger, 45AF90 target).
constexpr std::uint32_t TriggerMarker64a9e8[6]{0x57003du,0x57003au,0x57003bu,0x57003cu,0x57003eu,0x57003du};
constexpr std::uint32_t TargetMarker64aa00[6]{0x570028u,0x570025u,0x570026u,0x570027u,0x570029u,0x570028u};
using driving::CourseProbe;
// 40F180(a, 0.5, b, 0.5): the halves are exact, the sums rounded once to binary32.
CourseProbe half_sum_40f180(const driving::Bytes& o,std::size_t a,std::size_t b){
    return {0.5f*o.f32(a)+0.5f*o.f32(b),0.5f*o.f32(a+4)+0.5f*o.f32(b+4),0.5f*o.f32(a+8)+0.5f*o.f32(b+8)};
}
// 40EFF0(a, b): a x b under the PC's single-precision x87 control word.
CourseProbe cross_40eff0(const CourseProbe& a,const CourseProbe& b){
    using driving::X87;
    return {driving::x87_float(X87(a.y)*X87(b.z)-X87(a.z)*X87(b.y)),
            driving::x87_float(X87(a.z)*X87(b.x)-X87(a.x)*X87(b.z)),
            driving::x87_float(X87(a.x)*X87(b.y)-X87(a.y)*X87(b.x))};
}
// 4FB090(segment, lane, &pos, &angles, hint) (thiscall 7F9460): the centre of the course run
// at the segment (43E570 on the course of the lane), and the angles (449640) of the frame
// {forward, side, across} built from the mid-points of the run's corners +24/+30/+3C/+48.
void road_place_4fb090(NativeRuntimeContext& c,std::int16_t segment,std::uint32_t lane,std::int32_t hint,
                       CourseProbe& pos,std::array<float,3>& angles){
    if(lane>3u)throw std::out_of_range("4FB090: course type outside 0..3");
    const auto tables=c.start_mode.scene_owner_course_world.tables_or_empty();
    std::array<std::uint8_t,0xc> request{};std::array<std::uint8_t,0x58> out{};
    driving::Bytes rq(request.data(),request.size()),o(out.data(),out.size());
    rq.put32(0,lane);rq.put32(4,lane?0x65u:0u);rq.put16(8,std::uint16_t(segment));
    (void)driving::course_run_geometry_43e570(tables.courses[lane],rq,o,hint);
    pos={o.f32(8),o.f32(0xc),o.f32(0x10)};
    const auto p1=half_sum_40f180(o,0x24,0x3c),p2=half_sum_40f180(o,0x30,0x48);
    const auto p3=half_sum_40f180(o,0x24,0x30),p4=half_sum_40f180(o,0x3c,0x48);
    CourseProbe forward{p2.x-p1.x,p2.y-p1.y,p2.z-p1.z},across{p4.x-p3.x,p4.y-p3.y,p4.z-p3.z};   // 40EFA0
    (void)driving::pc_unit_vector_40eeb0(forward);(void)driving::pc_unit_vector_40eeb0(across);
    const auto side=cross_40eff0(across,forward);
    std::array<float,12> rows{forward.x,forward.y,forward.z,0.0f,side.x,side.y,side.z,0.0f,across.x,across.y,across.z,0.0f};
    angles=matrix_angles_449640(driving::Bytes(reinterpret_cast<std::uint8_t*>(rows.data()),0x30));
}
// 505CA0(segment, object, lift, sx, sy) (thiscall 7F9460): within 50 segments before the
// segment (and 2 after), the marker object on the course run at the segment, lifted by
// lift + 4, turned to the run and scaled (sx, sy, 1); a second one on the branch's run when
// the stage has one there (44F0F0, hint 44DDC0).
void road_marker_505ca0(NativeRuntimeContext& c,PcSceneRenderer& r,std::int32_t segment,std::uint32_t object,
                        float lift,float sx,float sy){
    if(c.start_mode.manager_state_7f94c0)throw std::runtime_error("505CA0: the LAN offset [7F9460+190] is not ported");
    const std::int32_t player=player_segment(c);
    if(player>=segment+2||player<=segment-50)return;
    auto& m=native_race_area_memory(c,&r,false);
    auto& q=r.queue_context();auto& st=q.matrices;
    CourseProbe pos{};std::array<float,3> angles{};
    auto draw=[&]{
        driving::pc_matrix_load(st,m.bytes(0x7d2da0u,64));                               // 44BEA0, 40A170
        driving::pc_matrix_translate_vector(st,{pos.x,pos.y,pos.z});                     // 40A290
        driving::pc_matrix_rotate_y(st,angles[1]);driving::pc_matrix_rotate_x(st,angles[0]);
        driving::pc_matrix_rotate_z(st,angles[2]);                                       // 40A410 / 40A3E0 / 40A440
        std::array<float,16> k{sx,0,0,0, 0,sy,0,0, 0,0,1.0f,0, 0,0,0,1};                 // 40A360
        driving::pc_matrix_multiply_current(st,driving::Bytes(reinterpret_cast<std::uint8_t*>(k.data()),64));
        render_object_405360(q,object,0,driving::Bytes(nullptr,0),0u,-1,0);
    };
    road_place_4fb090(c,std::int16_t(segment),0u,-1,pos,angles);
    pos.y=pos.y+lift+4.0f;                                                               // [6280C0]
    render_queue_mode_4052b0(q);
    driving::pc_matrix_push(st);                                                         // 409EF0
    draw();
    const std::uint32_t stage=m.u32(m.u32(m.u32(0x7d3188u)+0x14u));
    const std::uint16_t rolling=m.u16(m.u32(0x6a55c8u)+0x7eu);
    if(driving::pc_road_stage_gate_44f0f0(std::int32_t(stage),std::uint16_t(segment),rolling)){
        const auto hint=driving::road_stage_window_44ddc0(std::int32_t(stage),std::uint16_t(segment),rolling);
        road_place_4fb090(c,std::int16_t(segment),0u,hint,pos,angles);
        pos.y=pos.y+lift+4.0f;
        driving::pc_matrix_identity(st);                                                 // 40A020
        draw();
    }
    driving::pc_matrix_pop(st);                                                          // 40A010
    render_queue_flush_4052c0(r.flush_context(),q);
}
}
bool native_race_variant_invoke(NativeRuntimeContext& c,std::uint32_t callback,driving::PcMatrixStack& matrices){
    switch(callback){
    case 0x495740u:variant8_init_495740(c);return true;
    case 0x495790u:variant8_control_495520(c);return true;
    case 0x4957a0u:variant8_destroy_4957a0(c);return true;
    case 0x4b0400u:variant9_init_4b0040(c);return true;
    case 0x4b0780u:variant9_control_4b0590(c,matrices);return true;
    case 0x4b07a0u:variant9_destroy_4b0410(c,matrices);return true;
    default:return false;
    }
}
std::uint32_t native_race_attack_value(NativeRuntimeContext& c,std::uint32_t pc,const std::uint32_t* a,std::size_t n){
    auto& v=variant9(c);const auto state=c.start_mode.scene_state_8421c0;
    auto arg=[&](std::size_t i){if(i>=n)throw std::invalid_argument("variant 9 accessor without its argument");return a[i];};
    switch(pc){
    case 0x4b00d0u:return state!=0u?1u:0u;
    case 0x4b00e0u:return state==5u?1u:0u;
    case 0x4b00f0u:return v.get(0x68625cu);
    case 0x4b0100u:{const auto i=arg(0);if(i>=5u)throw std::out_of_range("4B0100: stage outside 686260");return v.get(0x686260u+i*4u);}
    case 0x4b0190u:return v.level_8421d8;
    case 0x4b02a0u:return v.get(0x686258u);
    case 0x4b0110u:{   // stage hook: the record of the stage key for the level (+0 / +0xC / +0x15 / +0x1A)
        std::uint32_t i=arg(0);const auto level=arg(1);
        if(level==1u)i+=0xcu;else if(level==2u)i+=0x15u;else if(level==3u)i+=0x1au;
        v.put(0x686254u,i);select_record(c,i);return 0u;}
    case 0x4b01a0u:{   // rival speed: 0 outside the attack, else (686274[rank] + 0.95) * the player's speed
        if(state!=5u)return 0u;
        if(!v.boost_8421c4)return arg(0);
        const auto rank=arg(1)&0xffu;
        if(rank>=7u)throw std::out_of_range("4B01A0: rank outside 686274");
        float speed;std::memcpy(&speed,player_car(c).data()+0x1c4,4);
        const float r=driving::x87_float((driving::X87(RankFactor686274[rank])+driving::X87(0.949999988f))*driving::X87(speed));
        std::uint32_t bits;std::memcpy(&bits,&r,4);return bits;}
    default:throw std::invalid_argument("not a variant 9 accessor");
    }
}
void native_race_attack_map(NativeRuntimeContext& c,PcRaceMemory& m){
    auto& v=variant9(c);
    m.map(Data686254,v.data_686254.data(),v.data_686254.size());
    if(!v.blob.empty())m.map(RaceAttackBase,v.blob.data(),v.blob.size());
}
// 4B0390(models): the model flags of every record's class list (477290: 64E090[+24])
// and of its special racers (+34, 0x28 bytes each until a negative car, 46C8A0).
bool native_race_attack_models_4b0390(NativeRuntimeContext& c,std::uint8_t* models,std::size_t size){
    auto& v=variant9(c);static const PcAddressView exe=[]{PcAddressView e;e.add_exe();return e;}();
    auto flag=[&](std::int32_t m){if(m<0||std::size_t(m)>=size)return false;models[m]=1u;return true;};
    for(std::uint32_t i=0;i<v.count_8421cc;++i){
        const std::uint32_t rec=v.records_8421d0+i*RaceAttackRecordBytes;
        const std::uint32_t k=v.field(rec,0x24u);std::uint32_t list,count;
        if(!exe.u32(0x64e090u+k*8u,list)||!exe.u32(0x64e094u+k*8u,count))return false;
        for(std::uint32_t j=0;std::int32_t(j)<std::int32_t(count);++j){std::uint8_t b;if(!exe.u8(list+j,b)||!flag(std::int8_t(b)))return false;}
        for(std::uint32_t e=v.field(rec,0x34u);e;e+=0x28u){
            const std::int32_t car=std::int32_t(v.field(e,0u));
            if(car<0)break;
            std::int32_t model=0x1d;
            if(std::uint8_t(car)!=0x1eu){                                // 46C8A0: 5B3C74 pairs
                for(std::uint32_t t=0;;++t){std::uint8_t key,val;
                    if(!exe.u8(0x5b3c74u+t*2u,key)||!exe.u8(0x5b3c75u+t*2u,val))return false;
                    if(std::int8_t(key)>=0&&key==std::uint8_t(car)){model=std::int8_t(val);break;}}
            }
            if(!flag(model))return false;
        }
    }
    return true;
}
// 4956C0 (variant 8 goal, 49D175): a multi-stage code (836174 >= 3C) records its total and
// the five stage counts 836178.. (448230 with code - 3C).
void native_race_variant8_goal_4956c0(NativeRuntimeContext& c){
    auto& v=variant8(c);auto& st=c.start_mode;
    const std::uint32_t code=st.frontend_prepare.alternate_code_836174;
    if(std::int32_t(code)<0x3c)return;
    const std::uint32_t course=std::uint32_t(std::int32_t(std::int8_t(st.course_choice_655b59)));          // 48B140
    const std::uint32_t a=c.event_function36.car_select.transmission_830374==1u?1u:0u;                     // 48B180
    const std::uint32_t b=st.vehicle_variant_83036d==1u?1u:0u;                                              // 48B1A0
    (void)record_insert_448230(c,code-0x3cu,v.total_8361b8,v.by_stage_836178,a,b,course);
}
// 495610 (race manager stage end, variant 8): for a single-stage code (below 3C) the
// stage counts into the records (451350(0) == 1 sets 44FEF0(1) first), the total kept
// in 836178 and cleared; the sector counts cleared.
void native_race_variant8_stage_end_495610(NativeRuntimeContext& c){
    auto& v=variant8(c);auto& st=c.start_mode;
    const std::uint32_t code=st.frontend_prepare.alternate_code_836174;
    if(std::int32_t(code)<0x3c){
        const std::uint32_t zero=0;std::uint32_t eax{};
        if(!native_race_manager_call(c,0x451350u,&zero,1,eax,true))throw std::runtime_error("495610: 451350 refused");
        if(eax==1u)race_set_flag2_44fef0(c.race.manager.state,1u);
        const std::uint32_t course=std::uint32_t(std::int32_t(std::int8_t(st.course_choice_655b59)));          // 48B140
        const std::uint32_t a=c.event_function36.car_select.transmission_830374==1u?1u:0u;                     // 48B180
        const std::uint32_t b=st.vehicle_variant_83036d==1u?1u:0u;                                              // 48B1A0
        (void)record_insert_4483d0(c,code,v.total_8361b8,a,b,course);
        v.by_stage_836178[0]=v.total_8361b8;v.total_8361b8=0;
    }
    for(auto& x:v.by_sector_836154)x=0;
}
}
namespace outrun::platform {
// 4B0490 (display 4B0790): in the race (mode 16) from state 3, the trigger marker of the
// current record (+14, 45AF80) and, with the attack running (state 5), the rank markers
// (4B04D0, through ranks) and the target marker (686258, 45AF90).
bool native_race_attack_display_4b0490(NativeRuntimeContext& c,PcSceneRenderer& r,const std::function<bool()>& ranks){
    if(c.mode_state.current!=0x10u)return true;
    const auto state=c.start_mode.scene_state_8421c0;
    if(state<3u||state>5u)return true;
    auto& v=variant9(c);
    if(state==5u&&!ranks())return false;
    if(!v.record_8421d4)return true;
    const auto lang=native_race_end(c).language_7d2698;
    if(lang>=6u)throw std::out_of_range("4B0490: language outside the marker tables");
    road_marker_505ca0(c,r,std::int32_t(v.field(v.record_8421d4,0x14u)),TriggerMarker64a9e8[lang],0.0f,1.0f,1.0f);
    if(state==5u)road_marker_505ca0(c,r,std::int32_t(v.get(0x686258u)),TargetMarker64aa00[lang],0.0f,1.0f,1.0f);
    return true;
}
}

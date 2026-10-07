#include "platform/native_runtime.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
using namespace outrun::platform;
namespace {
unsigned checks{};
void require(bool value,const char* message){
    ++checks;if(!value){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}
}
void put32(std::vector<std::uint8_t>& b,std::size_t at,std::uint32_t value){
    for(unsigned i=0u;i<4u;++i)b[at+i]=std::uint8_t(value>>(8u*i));
}
std::vector<std::uint8_t> synthetic(){
    constexpr std::size_t races=8u+94u*8u+3u*12u;
    constexpr std::size_t linked=races+94u*0x44u;
    std::vector<std::uint8_t> b(linked+2u*0x78u);
    put32(b,0u,2u);put32(b,4u,94u);
    for(std::uint32_t i=0u;i<94u;++i){
        put32(b,8u+i*8u,static_cast<std::uint32_t>(races+i*0x44u+0x18u));
        put32(b,12u+i*8u,static_cast<std::uint32_t>(linked));
        put32(b,races+i*0x44u,i+1u);put32(b,races+i*0x44u+4u,1u);
        put32(b,races+i*0x44u+0x14u,3u);
    }
    const auto index=8u+94u*8u;
    put32(b,index,outrun::driving::runtime_category_hash_4f1260("Races"));
    put32(b,index+4u,94u);put32(b,index+8u,static_cast<std::uint32_t>(races));
    put32(b,index+12u,0xffffffffu);put32(b,index+16u,2u);
    put32(b,index+20u,static_cast<std::uint32_t>(linked));
    put32(b,linked+0x1cu,15u);put32(b,linked+0x20u,15u);
    put32(b,linked+0x2cu,1u);put32(b,linked+0x30u,0xffffffffu);
    put32(b,linked+0x78u+4u,1u);
    put32(b,linked+0x78u+0x1cu,15u);
    put32(b,linked+0x78u+0x20u,15u);
    put32(b,linked+0x78u+0x2cu,0xffffffffu);
    put32(b,linked+0x78u+0x30u,0xffffffffu);
    return b;
}
std::vector<std::uint8_t> synthetic_assignment(){
    std::vector<std::uint8_t> b(340u);
    put32(b,0u,1u);
    put32(b,8u,outrun::driving::runtime_category_hash_4f1260("RACE_MAPPING_ARRAY"));
    put32(b,12u,40u);put32(b,16u,20u);
    for(std::uint32_t i=0;i<40u;++i){
        put32(b,20u+i*8u,i+1u);put32(b,24u+i*8u,1u);
    }
    return b;
}
}
int main(int argc,char** argv){
    auto raw=synthetic();RaceAssetPack pack{};std::string error;
    const bool parsed=parse_race_asset_pack(raw.data(),raw.size(),pack,&error);
    if(!parsed)std::fprintf(stderr,"synthetic parse: %s\n",error.c_str());
    require(parsed,"synthetic Races category and 94 relocations accepted");
    RaceCourseSelection selected{};
    require(race_course_select_4965a0(pack,1u,1u,selected)&&selected.race_index==0u&&
            selected.race_kind==3u&&selected.course_count==2u&&
            selected.course_records.size()==2u*0x78u,
            "PC two-key selection resolves relocated direct course group");
    require(race_course_select_4965a0(pack,94u,1u,selected)&&selected.race_index==93u,
            "last retail-style race record selected");
    require(!race_course_select_4965a0(pack,1u,2u,selected),"unknown sub-key fails closed");
    auto assignment_bytes=synthetic_assignment();RaceAssignmentPack assignment{};
    require(parse_race_assignment_pack(assignment_bytes.data(),assignment_bytes.size(),
                                       assignment,&error)&&assignment.menu_count==40u,
            "synthetic original menu mapping category accepted");
    std::uint32_t race_key=0u,sub_key=0u;
    require(race_menu_choice_4eeb50(pack,assignment,0u,0u,race_key,sub_key)&&
            race_key==1u&&sub_key==1u,
            "original menu index zero maps to first Races record");
    require(!race_menu_choice_4eeb50(pack,assignment,40u,0u,race_key,sub_key),
            "out-of-range menu choice fails closed");
    auto bad=raw;put32(bad,12u,static_cast<std::uint32_t>(bad.size()+1u));
    require(!parse_race_asset_pack(bad.data(),bad.size(),pack,&error),"invalid relocation target rejected");
    bad=raw;put32(bad,8u,static_cast<std::uint32_t>(bad.size()));
    require(!parse_race_asset_pack(bad.data(),bad.size(),pack,&error),"invalid relocation patch rejected");
    if(argc>=2){
        require(load_race_asset_pack_file(argv[1],pack,&error),"owned retail Races.bin accepted");
        require(pack.race_count==94u&&race_course_select_4965a0(pack,1u,1u,selected)&&
                selected.course_count==2u&&selected.race_index==0u,
                "owned first race resolves two original course records");
    }
    if(argc>=5){
        require(load_race_assignment_pack_file(argv[4],assignment,&error),
                "owned retail RaceAssignment.bin accepted");
        require(race_menu_choice_4eeb50(pack,assignment,0u,0u,race_key,sub_key)&&
                race_key==1u&&sub_key==1u,
                "retail menu index zero selects race 1/1");
        for(std::uint32_t menu=0u;menu<assignment.menu_count;++menu){
            require(race_menu_choice_4eeb50(pack,assignment,menu,0u,
                                             race_key,sub_key)&&
                    race_course_select_4965a0(pack,race_key,sub_key,selected),
                    "every authored menu assignment resolves retail course records");
        }
    }
    // Course selection now runs in the event-0x191 mission manager (mission-manager oracle).
    std::printf("race_asset_pack: %u checks passed\n",checks);return 0;
}

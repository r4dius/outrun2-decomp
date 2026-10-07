#include "frontend_course_fixture.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
using namespace outrun::platform;
using frontend_fixture::put;
namespace {
unsigned checks{};
void req(bool v,const char* m){++checks;if(!v){std::fprintf(stderr,"FAIL: %s\n",m);std::exit(1);}}
}
int main(int argc,char** argv){
    std::array<FrontendCourseTable,4> tables{};std::string error;
    for(unsigned lane=0;lane<4;++lane){
        auto b=frontend_fixture::script(lane);const auto original=b;
        req(parse_frontend_course_table(lane,b.data(),b.size(),tables[lane],&error),"valid category bound");
        req(b==original&&tables[lane].count==15&&tables[lane].records.size()==15*0x78,
            "parser preserves source and owns exactly the category records");
        const auto saved=tables[lane].records;
        for(std::size_t n=0;n<b.size();++n)
            req(!parse_frontend_course_table(lane,b.data(),n,tables[lane])&&tables[lane].records==saved,
                "all truncated lengths reject atomically");
        put(b,12,0xffffffffu);req(!parse_frontend_course_table(lane,b.data(),b.size(),tables[lane]),"negative count");
        b=original;put(b,16,0xffffffffu);req(!parse_frontend_course_table(lane,b.data(),b.size(),tables[lane]),"invalid category offset");
        b=original;put(b,8,0);req(!parse_frontend_course_table(lane,b.data(),b.size(),tables[lane]),"wrong category");
        b=original;put(b,32+0x1c,66);req(!parse_frontend_course_table(lane,b.data(),b.size(),tables[lane]),"grade type 66 rejected");
        req(!parse_frontend_course_table(4,original.data(),original.size(),tables[lane]),"invalid lane");
    }
    RaceAssetPack races{};races.race_count=94;races.bytes.resize(94*0x44);
    RaceAssignmentPack assignment{};assignment.menu_count=40;
    for(unsigned i=0;i<40;++i)assignment.menu_race_keys[i]=i+1;
    for(unsigned i=0;i<94;++i)put(races.bytes,i*0x44,(i%40)+1);
    LicenseProgressTables progress{};
    req(frontend_license_progress_tables(&races,&assignment,tables,32,progress)&&progress.categories_ready,
        "real record counts and course fields form completion input");
    for(unsigned i=0;i<40;++i)req(progress.category_counts[i]==(i<14?3:2),"mapping count includes all matching records");
    for(unsigned lane=0;lane<3;++lane)for(unsigned i=0;i<15;++i)
        req(progress.championship_present[lane]&&progress.championship_types[lane][i]==lane*15+i,"actual +1C field used");
    const auto expected=progress;
    auto missing=tables;missing[1]={};
    req(!frontend_license_progress_tables(&races,&assignment,missing,32,progress)&&
        progress.championship_types==expected.championship_types,"missing table cannot become empty completed group");
    auto bad_races=races;for(unsigned i=0;i<6;++i)put(bad_races.bytes,i*0x44,1);
    req(!frontend_license_progress_tables(&bad_races,&assignment,tables,32,progress),"more than five grades rejected");
    req(!frontend_license_progress_tables(nullptr,&assignment,tables,32,progress),"missing race source rejected");
    req(frontend_license_progress_tables(nullptr,nullptr,{},16,progress)&&progress.mode==16,
        "non-frontend PC bypass does not require data");
    PcLicense license{};frontend_license_reset_4471a0(license,123);
    double completion=-1;
    req(frontend_license_completion_447400(license,progress,completion)&&completion==0,"non-FE completion zero");
    if(argc>1){
        const std::string root=argv[1];
        req(load_race_asset_pack_file((root+"/Scripts/bin/Races.bin").c_str(),races,&error),"retail Races parsed");
        req(load_race_assignment_pack_file((root+"/Scripts/bin/RaceAssignment.bin").c_str(),assignment,&error),"retail mapping parsed");
        RetailAssetStore store{};req(retail_asset_store_open(store,root,&error),"owned retail source");
        auto runtime=std::make_unique<NativeRuntimeContext>();
        req(native_runtime_attach_retail_assets(*runtime,store),"retail store attached");
        req(native_runtime_attach_race_assets(*runtime,races),"retail Races source attached for stage-seven readiness");
        req(native_runtime_event_function36_invoke(*runtime,0x49e490,405),"real owner constructor");
        auto& state=runtime->event_function36;
        // Target the actual stage-7 loader boundary, not fake script callbacks.
        outrun::driving::Bytes owner(state.object.data(),state.object.size());
        owner.put32(0x218,1);owner.put32(0,7);state.frontend_init_calls=2;
        outrun::driving::frontend_bulk_loader_initialize_4e85e0(state.frontend_bulk_loader);
        req(native_runtime_event_function36_invoke(*runtime,0x49e4a0,405),"native frontend loader executes");
        req(state.frontend_bulk_ready_count==64&&state.frontend_bulk_pending==0&&
            state.frontend_bulk_missing_pc==0,"all retail scripts validated by production loader");
        tables=state.frontend_course_tables;
        req(frontend_license_progress_tables(&races,&assignment,tables,32,progress),"retail progress provider");
        unsigned total=0;for(auto n:progress.category_counts)total+=n;
        req(total==94,"forty original categories cover all ninety-four retail races");
        req(frontend_license_completion_447400(license,progress,completion)&&completion==0,"new license has zero retail progress");
        for(unsigned grade=0;grade<=6;++grade){
            for(unsigned i=0;i<200;++i)license[0x1dd+i]=std::uint8_t(grade);
            for(unsigned i=0;i<132;++i)license[0x36e + i]=std::uint8_t(grade*17);
            req(frontend_license_completion_447400(license,progress,completion),"retail championship grade lookup bounded");
            constexpr double weights[]={10,20,30,40,60,80,100};
            req(completion>weights[grade]-0.001&&completion<weights[grade]+0.001,"seven groups use original completion weights");
        }
    }
    // Production boundary rejects a file that exists but is not a container.
    auto runtime=std::make_unique<NativeRuntimeContext>();LoaderAssetPack assets{};
    for(auto id:{0xbau,0xbbu})assets.records.push_back({id,2,{1}});
    for(auto id:{0x2cu,0x33u,0x48u})assets.records.push_back({id,8,{1}});
    assets.records.push_back({0x44,9,{1}});
    assets.records.push_back({LoaderAssetSelectTableId,0,std::vector<std::uint8_t>(LoaderAssetSelectTableBytes)});
    for(unsigned i=0;i<64;++i)assets.records.push_back({LoaderAssetFrontendScriptBaseId+i,0,frontend_fixture::script(i)});
    assets.records[7].bytes={0};
    req(native_runtime_attach_loader_assets(*runtime,assets),"synthetic loader source attached");
    req(native_runtime_event_function36_invoke(*runtime,0x49e490,405),"boundary owner initialized");
    auto& state=runtime->event_function36;
    // This fixture isolates script loading, with the independent Races
    // loader already complete. Initialization counters are not readiness.
    runtime->mission.manager.status_836358=3u;
    outrun::driving::Bytes owner(state.object.data(),state.object.size());
    owner.put32(0x218,1);owner.put32(0,7);state.frontend_init_calls=2;
    outrun::driving::frontend_bulk_loader_initialize_4e85e0(state.frontend_bulk_loader);
    req(native_runtime_event_function36_invoke(*runtime,0x49e4a0,405),"malformed load poll");
    req(owner.u32(0)==7&&state.frontend_bulk_pending==1&&state.frontend_bulk_ready_count==0&&
        state.frontend_bulk_missing_pc==0x4f12a0&&state.frontend_bulk_loader.special_handles[0]==0,
        "existing malformed file cannot advance stage seven or publish a handle");
    assets.records[7].bytes=frontend_fixture::script(0);
    req(native_runtime_event_function36_invoke(*runtime,0x49e4a0,405),"retry after corrected data");
    req(state.frontend_bulk_ready_count==64&&state.frontend_bulk_pending==0&&state.frontend_bulk_missing_pc==0,
        "retry owns exactly sixty-four containers without counting the failed attempt");
    // Restart through the real stage-6 initialization. Old handles/data must
    // disappear, even when the next load fails part-way through the four tables.
    for(unsigned cycle=0;cycle<3;++cycle){
        owner.put32(0x218,1);owner.put32(0,6);
        state.frontend_resource_requests=state.frontend_ready_count=1;
        state.frontend_resource_pending=0;
        assets.records[7].bytes=frontend_fixture::script(0);
        put(assets.records[7].bytes,32+0x1c,65-cycle);
        assets.records[8].bytes={0};
        req(native_runtime_event_function36_invoke(*runtime,0x49e4a0,405),"retained stage six restart");
        req(owner.u32(0)==7&&state.frontend_bulk_ready_count==0,
            "missing Races source blocks script polling after real reinitialization");
        runtime->mission.manager.status_836358=3u; // fixture's separate loader completes
        req(native_runtime_event_function36_invoke(*runtime,0x49e4a0,405),"independent Races completion resumes script polling");
        req(owner.u32(0)==7&&state.frontend_bulk_ready_count==1&&state.frontend_bulk_pending==1&&
            state.frontend_course_tables[1].records.empty()&&state.frontend_bulk_loader.special_handles[1]==0,
            "restart removes stale table and handle before failure");
        req(state.frontend_course_tables[0].records[0x1c]==65-cycle,"restart reads new course data");
        assets.records[8].bytes=frontend_fixture::script(1);
        req(native_runtime_event_function36_invoke(*runtime,0x49e4a0,405)&&
            state.frontend_bulk_ready_count==64&&state.frontend_bulk_pending==0,"retained retry completes once");
    }
    runtime->mode_state.current=32;
    req(native_runtime_attach_race_assets(*runtime,races)&&native_runtime_attach_race_assignment(*runtime,assignment),
        "progress sources attached to runtime");
    req(native_runtime_license_completion(*runtime,license,completion),"live mode completion provider");
    req(native_runtime_event_function36_invoke(*runtime,0x49e4c0,405)&&
        !native_runtime_license_completion(*runtime,license,completion),"destroy invalidates progress data");
    runtime->mode_state.current=16;
    req(native_runtime_license_completion(*runtime,license,completion)&&completion==0,"live non-FE mode bypass");
    std::printf("frontend_progress: %u checks passed\n",checks);
}

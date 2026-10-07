#include "platform/frontend_vehicle_loader.hpp"
#include "platform/frontend_vehicle_data.hpp"
#include "platform/native_runtime.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <set>
using namespace outrun::platform;
using outrun::driving::Bytes;
static unsigned checks;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"vehicle loader line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
struct Io {
    std::vector<std::array<unsigned,3>> calls;
    std::set<unsigned> ready;
    unsigned fail{};
    FrontendVehicleLoaderServices api(){return {this,
        [](void* p,unsigned id,unsigned mode){auto& s=*static_cast<Io*>(p);s.calls.push_back({0x448ad0,id,mode});return s.fail!=0x448ad0;},
        [](void* p,unsigned id,bool& ready){auto& s=*static_cast<Io*>(p);s.calls.push_back({0x448960,id,0});ready=s.ready.count(id)!=0;return s.fail!=0x448960;},
        [](void* p,unsigned id){auto& s=*static_cast<Io*>(p);s.calls.push_back({0x448990,id,0});return s.fail!=0x448990;}};}
};
static std::vector<std::uint8_t> read(const char* path){std::ifstream f(path,std::ios::binary);CHECK(bool(f));return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char** argv){
    CHECK(argc==3);const auto exe=read(argv[1]);
    auto u16=[&](std::size_t p){CHECK(p+2<=exe.size());return unsigned(exe[p])|(unsigned(exe[p+1])<<8);};
    auto u32=[&](std::size_t p){return u16(p)|(u16(p+2)<<16);};
    const auto pe=u32(0x3c),sections=u16(pe+6),optional=u16(pe+20),image=u32(pe+24+28);
    auto offset=[&](unsigned va){for(unsigned i=0;i<sections;++i){const auto p=pe+24+optional+40*i,rva=u32(p+12),size=u32(p+16),raw=u32(p+20);if(va>=image+rva&&va-image-rva<size)return std::size_t(raw+va-image-rva);}CHECK(false);return std::size_t(0);};
    RetailAssetStore retail;std::string error;CHECK(retail_asset_store_open(retail,argv[2],&error));
    for(unsigned i=0;i<30;++i){
        CHECK(VehicleResourceIds[i]==u32(offset(0x5b2fe0+8*i)));
        CHECK(VehicleMenuModels[i]==u32(offset(0x68ea10+4*i)));
        CHECK(VehicleMenuUnlocks[i]==std::int16_t(u16(offset(0x68ea88+2*i))));
        CHECK(VehicleDefaultColours[i]==std::int8_t(exe[offset(0x68eca8+i)]));
        for(unsigned j=0;j<8;++j)CHECK(VehicleColourUnlocks[i][j]==std::int16_t(u16(offset(0x68eac8+2*(8*i+j)))));
        const auto name=offset(u32(offset(0x633558+4*VehicleResourceIds[i])));
        std::string path;for(auto j=name;j<exe.size()&&exe[j];++j)path+=char(exe[j]);
        std::replace(path.begin(),path.end(),'\\','/');CHECK(path=="/"+std::string(VehicleResourcePaths[i]));
        // Both callers read the actual file; same bytes, separate ownership key.
        const auto* race=retail_asset_lookup(retail,VehicleResourceIds[i],8,&error);
        const auto* preview=retail_asset_lookup(retail,VehicleResourceIds[i],11,&error);
        CHECK(race&&preview&&!race->bytes.empty()&&race!=preview&&race->bytes==preview->bytes);
        CHECK(!retail_asset_lookup(retail,VehicleResourceIds[i],7,&error));
        // Exercise the connected START stage with retail data, not a success stub.
        NativeRuntimeContext context{};CHECK(native_runtime_attach_retail_assets(context,retail));
        context.start_mode.scene_owner_stage=24;context.start_mode.course_choice_655b59=i;
        unsigned result{};CHECK(native_start_owned_call(context,0x49ba80,nullptr,0,result));
        // 49BF62: 49B3F0(46BBE0(48B140()), 8) is the first model request of stage 24.
        const auto& models=context.start_mode.scene_owner_list_836850;
        CHECK(!models.empty()&&models.front()==VehicleResourceIds[i]);
        CHECK(context.start_mode.scene_owner_resource_ids[0]==VehicleResourceIds[i]&&context.start_mode.scene_owner_resource_modes[0]==8u);
    }
    unsigned untouched=123;CHECK(!vehicle_resource_46bbe0(30,untouched)&&untouched==123);
    for(unsigned model:{0u,15u,29u,30u,~0u}){
        NativeRuntimeContext context{};context.start_mode.scene_owner_stage=24;
        context.start_mode.course_choice_655b59=model;unsigned result{};
        CHECK(native_start_owned_call(context,0x49ba80,nullptr,0,result));
        CHECK(context.start_mode.scene_owner_resource_count==0);
        CHECK(context.start_mode.scene_owner_fault==(model<30?0x448ad0u:0x46bbe0u));
    }
    PcLicense profile{};
    for(unsigned bit=0;bit<138;++bit){
        profile.fill(0);profile[0x28+bit/8]=std::uint8_t(1u<<(bit%8));
        for(unsigned i=0;i<30;++i){
            CHECK(vehicle_unlocked_4c8f90(profile,i)==(i<2||unsigned(VehicleMenuUnlocks[i])==bit));
            for(unsigned j=0;j<8;++j){const auto k=VehicleColourUnlocks[i][j];CHECK(vehicle_colour_unlocked_4c8fc0(profile,i,j)==(k==-1||(k>=0&&unsigned(k)==bit)));}
        }
    }
    CHECK(!vehicle_unlocked_4c8f90(profile,30));CHECK(!vehicle_colour_unlocked_4c8fc0(profile,0,8));
    Io io;FrontendVehicleLoader loader;loader.object.fill(0x5a);
    CHECK(frontend_vehicle_loader_init_48bf20(loader,io.api()));Bytes b(loader.object.data(),loader.object.size());
    CHECK(frontend_vehicle_loader_empty_48bfc0(loader));CHECK(b.u8(0)==1&&b.u8(1)==0x5a);
    CHECK((io.calls==std::vector<std::array<unsigned,3>>{{0x448ad0,0xba,2},{0x448ad0,0xbb,2}}));
    b.put8(0,0);for(unsigned i=0;i<3;++i)b.put32(4+4*i,i);
    CHECK(frontend_vehicle_loader_tick_48bfe0(loader,io.api()));CHECK(b.u32(0x18)==1);
    CHECK(frontend_vehicle_loader_tick_48bfe0(loader,io.api()));CHECK(b.u32(0x18)==2);
    const auto saved=loader.object;
    for(unsigned i=0;i<20;++i)CHECK(frontend_vehicle_loader_tick_48bfe0(loader,io.api())&&loader.object==saved);
    io.ready.insert(VehicleResourceIds.begin(),VehicleResourceIds.end());
    for(unsigned i=0;i<5;++i)CHECK(frontend_vehicle_loader_tick_48bfe0(loader,io.api()));
    for(unsigned i=0;i<3;++i)CHECK(frontend_vehicle_loader_ready_48bf80(loader,i));
    // Repeated scrolling/reversal preserves neighbors and only evicts outsiders.
    for(unsigned choice=0;choice<90;++choice){
        const auto index=choice<45?choice%30:29-choice%30;
        for(unsigned i=0;i<3;++i)b.put32(4+4*i,VehicleMenuModels[(index+i)%30]);
        for(unsigned i=0;i<10;++i)CHECK(frontend_vehicle_loader_tick_48bfe0(loader,io.api()));
        for(unsigned i=0;i<3;++i)CHECK(frontend_vehicle_loader_ready_48bf80(loader,b.u32(4+4*i)));
    }
    b.put8(0,1);CHECK(frontend_vehicle_loader_tick_48bfe0(loader,io.api()));CHECK(frontend_vehicle_loader_empty_48bfc0(loader));
    for(unsigned i=0;i<3;++i)CHECK(b.u32(4+4*i)==30);
    // Flush while loading waits for completion before release, as on PC.
    // The original excludes slots whose model is present in desired[], even
    // the empty sentinel. Use the three-model caller contract (duplicates OK).
    for(unsigned i=0;i<3;++i)b.put32(4+4*i,0);
    CHECK(frontend_vehicle_loader_tick_48bfe0(loader,io.api()));
    CHECK(frontend_vehicle_loader_tick_48bfe0(loader,io.api()));
    CHECK(frontend_vehicle_loader_tick_48bfe0(loader,io.api()));CHECK(frontend_vehicle_loader_ready_48bf80(loader,0));
    CHECK(frontend_vehicle_loader_tick_48bfe0(loader,io.api()));CHECK(frontend_vehicle_loader_empty_48bfc0(loader));
    FrontendVehicleLoader absent;CHECK(!frontend_vehicle_loader_init_48bf20(absent,{}));CHECK(absent.fault==0x448ad0);
    CHECK(!frontend_vehicle_loader_tick_48bfe0(absent,io.api()));
    for(unsigned pc:{0x448ad0u,0x448960u,0x448990u}){
        FrontendVehicleLoader fail;CHECK(frontend_vehicle_loader_init_48bf20(fail,io.api()));Bytes f(fail.object.data(),fail.object.size());
        f.put8(0,pc==0x448990);f.put32(0x14,0);f.put32(0x18,pc==0x448ad0?1:pc==0x448960?2:3);
        io.fail=pc;CHECK(!frontend_vehicle_loader_tick_48bfe0(fail,io.api())&&fail.fault==pc);io.fail=0;
    }
    std::printf("vehicle data/preloader: %u checks; all 30 retail models reach START stage25\n",checks);
}

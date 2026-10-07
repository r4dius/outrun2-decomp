#include "platform/frontend_vehicle_preview.hpp"
#include "platform/frontend_vehicle_data.hpp"
#include "platform/frontend_vehicle_menu.hpp"
#include "platform/native_runtime.hpp"
#include "platform/vehicle_constructor.hpp"
#include "platform/embedded_exe_data.hpp"
#include <memory>
#include <cstdio>
#include <cstdlib>
#include <vector>
using namespace outrun::platform;
using outrun::driving::Bytes;
static unsigned checks;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"preview line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
struct Fixture {
    std::array<std::uint8_t,0x94> object{};FrontendVehicleLoader loader;
    std::array<std::uint8_t,PcVehicleObjectBytes> vehicle{};std::array<std::uint8_t,12> lists{};
    Bytes b{object.data(),object.size()},l{loader.object.data(),loader.object.size()},v{vehicle.data(),vehicle.size()};
    std::vector<std::array<unsigned,5>> calls;
    unsigned fail{},ready_mask{3},delay{};
    FrontendVehiclePreviewServices s{l,v,Bytes(lists.data(),lists.size())};
    FrontendVehicleLoaderServices io{};
    Fixture(){
        s.user=this;s.call=[](void* p,unsigned pc,const unsigned* a,std::size_t n){auto& f=*static_cast<Fixture*>(p);
            if(n>4)return false;std::array<unsigned,5> row{pc,0,0,0,0};for(std::size_t i=0;i<n;++i)row[i+1]=a[i];f.calls.push_back(row);return f.fail!=pc;};
        s.ready_448960=[](void* p,unsigned id,bool& ready){auto& f=*static_cast<Fixture*>(p);
            if(f.fail==0x448960)return false;
            ready=id==0xba?(f.ready_mask&1)!=0:id==0xbb?(f.ready_mask&2)!=0:!f.delay;
            if(id!=0xba&&id!=0xbb&&f.delay)--f.delay;return true;};
        io={this,[](void* p,unsigned id,unsigned mode){auto& f=*static_cast<Fixture*>(p);f.calls.push_back({0x448ad0,id,mode,0,0});return f.fail!=0x448ad0;},s.ready_448960,
            [](void* p,unsigned id){auto& f=*static_cast<Fixture*>(p);f.calls.push_back({0x448990,id,0,0,0});return f.fail!=0x448990;}};
        CHECK(frontend_vehicle_preview_construct_48bf00(b));CHECK(frontend_vehicle_loader_init_48bf20(loader,io));
    }
    void tick(){CHECK(frontend_vehicle_loader_tick_48bfe0(loader,io));CHECK(frontend_vehicle_preview_tick_48c3f0(b,s));}
};
int main(){
    for(unsigned index=0;index<30;++index)for(unsigned unlocked=0;unlocked<2;++unlocked){
        Fixture f;CHECK(frontend_vehicle_preview_init_48c170(f.b,index,unlocked!=0,f.s));
        CHECK(f.l.u32(4)==VehicleMenuModels[(index+29)%30]&&f.l.u32(8)==VehicleMenuModels[index]&&f.l.u32(12)==VehicleMenuModels[(index+1)%30]);
        CHECK(f.s.scene_lists.u32(4)==0x844a08&&f.s.scene_lists.u32(8)==0x844a08&&!f.l.u8(0));
        f.ready_mask=0;f.delay=4;for(unsigned i=0;i<20;++i){f.tick();CHECK(!f.b.u32(0));}
        f.ready_mask=1;for(unsigned i=0;i<3;++i){f.tick();CHECK(!f.b.u32(0));}
        f.ready_mask=3;f.tick();CHECK(f.b.u32(0)==1);
        CHECK((f.calls.back()==std::array<unsigned,5>{0x440110,0x181,13,0,0}));
        CHECK((f.calls[f.calls.size()-2]==std::array<unsigned,5>{0x4406f0,VehicleMenuModels[index],1,unsigned(!unlocked),0x3e800000}));
        const auto count=f.calls.size();f.tick();CHECK(f.calls.size()==count);
        f.v.put32(4,0xa55a55a5);CHECK(frontend_vehicle_preview_select_48c290(f.b,index,0x107,unlocked==0,f.s));
        CHECK(f.v.u8(0x12)==7&&f.b.u8(8)==7&&f.b.u8(0x90)==!unlocked);
        CHECK(f.v.u32(4)==((0xa55a55a5u&~0x40u)|(unlocked?0x40:0))&&f.calls.size()==count);
        const auto next=(index+1)%30;CHECK(frontend_vehicle_preview_select_48c290(f.b,next,3,true,f.s));
        CHECK(!f.b.u32(0));CHECK(f.l.u32(4)==VehicleMenuModels[next]&&f.l.u32(8)==VehicleMenuModels[index]);
        CHECK((f.calls.back()==std::array<unsigned,5>{0x440330,8,24,0,0}));
        for(unsigned i=0;i<20;++i)f.tick();CHECK(f.b.u32(0)==1);
        CHECK(frontend_vehicle_preview_suspend_48c450(f.b,f.s));CHECK(f.l.u8(0)==1&&!f.b.u32(0));
        for(unsigned i=0;i<10;++i)CHECK(frontend_vehicle_loader_tick_48bfe0(f.loader,f.io));
        CHECK(frontend_vehicle_loader_empty_48bfc0(f.loader));
        CHECK(frontend_vehicle_preview_init_48c170(f.b,index,false,f.s));for(unsigned i=0;i<20;++i)f.tick();CHECK(f.b.u32(0));
    }
    for(unsigned pc:{0x40ec60u,0x44c0a0u,0x49fa60u,0x49a650u,0x44c0d0u}){
        Fixture f;f.fail=pc;CHECK(!frontend_vehicle_preview_init_48c170(f.b,0,true,f.s));CHECK(f.s.fault==pc);
        f.fail=0;CHECK(!frontend_vehicle_preview_tick_48c3f0(f.b,f.s));
    }
    for(unsigned pc:{0x49fa60u,0x4406f0u,0x440110u}){Fixture f;f.fail=pc;CHECK(!frontend_vehicle_preview_open_48c220(f.b,0,1,true,f.s)&&f.s.fault==pc);}
    for(unsigned pc:{0x4401d0u,0x440330u,0x44c3d0u,0x44a1a0u,0x4f2210u}){Fixture f;f.fail=pc;CHECK(!frontend_vehicle_preview_suspend_48c450(f.b,f.s)&&f.s.fault==pc);}
    {Fixture f;f.fail=0x448960;CHECK(!frontend_vehicle_preview_tick_48c3f0(f.b,f.s)&&f.s.fault==0x448960);}
    {Fixture f;f.s.vehicle=Bytes(nullptr,0);CHECK(!frontend_vehicle_preview_select_48c290(f.b,1,3,false,f.s)&&f.s.fault==0x799d18);}
    {Fixture f;CHECK(!frontend_vehicle_preview_init_48c170(f.b,30,true,f.s));}
    // Actual menu -> preview -> shared scheduler chain. Only external scene/
    // event/model leaves remain fixtures; do not replace the preview calls.
    {
        Fixture f;auto runtime=std::make_unique<NativeRuntimeContext>();
        DrivingDataPack driving;CHECK(parse_driving_data_pack(EmbeddedDrivingData,EmbeddedDrivingDataSize,driving));
        std::array<std::uint8_t,60> constructor_shared{};
        auto& creation=runtime->vehicle_creation;f.s.creation=&creation;
        FrontendVehicleMenu menu;FrontendSprites sprites;FrontendUiResources ui{sprites};
        CHECK(sprites.bind_timing(0x44,std::vector<FrontendSpriteTiming>(224,{640,60})));
        CHECK(sprites.bind_timing(0x2c,std::vector<FrontendSpriteTiming>(64,{52,60})));
        std::array<std::uint8_t,0xe00> owner{};Bytes root(owner.data(),owner.size());
        outrun::driving::PcUiNotifyGlobals globals;PcLicense license;license.fill(255);
        FrontendInputSnapshot input;unsigned repeat{},action{};
        CHECK(ui.commands(0x442ac0,root.sub(0x51c,owner.size()-0x51c),nullptr,0,globals,action));
        ui.effect_4249f0=[](void*,unsigned){return true;};
        FrontendVehicleMenuServices api{ui,root,globals,license,input,repeat};api.preview=&f.s;api.user=runtime.get();
        api.external=[](void* p,unsigned pc,Bytes,const unsigned* a,std::size_t n){
            switch(pc){case 0x4c50d0:case 0x4c50f0:case 0x4c5100:case 0x4c5110:return true;
                case 0x48b130:case 0x48b190:case 0x48b150:{unsigned result{};return native_start_owned_call(*static_cast<NativeRuntimeContext*>(p),pc,a,n,result);}}
            return false; // Especially reject 48C170/290/3F0/450 fixture bypass.
        };
        CHECK(frontend_vehicle_menu_construct_4c8de0(menu,repeat));
        Bytes preview(menu.object.data()+0x34,0x94);
        auto tick=[&]{CHECK(frontend_vehicle_loader_tick_48bfe0(f.loader,f.io));sprites.tick();api.timer+=1;
            CHECK(frontend_vehicle_menu_control_4c9290(menu,api,action));
            // Service the pending event's common constructor using the same
            // shared car that 48C290 edits. Event scheduling remains a fixture;
            // preview-specific init/display are not claimed by this test.
            Bytes q(creation.object.data(),creation.object.size());
            if(q.u8(0x108)<q.u8(0x109)){
                VehicleParameterSelection selection;selection.flag_8514a0=1;
                CHECK(vehicle_construct_4a5830(f.v,creation,driving,selection,Bytes(constructor_shared.data(),constructor_shared.size())));
            }};
        for(unsigned cycle=0;cycle<4;++cycle){
            CHECK(frontend_vehicle_menu_init_4c9010(menu,api));
            for(unsigned i=0;i<15;++i){
                input={};for(unsigned wait=0;wait<12;++wait)tick();CHECK(preview.u32(0)==1);
                const unsigned selected=unsigned(menu.variant_84b0e9*15+menu.cursor_84b0e8);
                CHECK(preview.u32(4)==selected);CHECK(frontend_vehicle_loader_ready_48bf80(f.loader,VehicleMenuModels[selected]));
                Bytes queued(creation.object.data(),creation.object.size());
                CHECK(queued.u8(0x109)==1&&queued.u32(0)==8&&queued.u32(8)==VehicleMenuModels[selected]&&queued.u32(4)==0x4081);
                CHECK(queued.u8(0x108)==1&&f.v.u8(0x11)==VehicleMenuModels[selected]&&f.v.u8(0x12)==preview.u8(8));
                const unsigned model=VehicleMenuModels[selected],map=model>=15?3:0;
                CHECK(f.v.u32(0x2b4)==0x5e3140+4*driving.selection_maps[map][model%15]);
                api.colour_held_4536c0=true;tick();api.colour_held_4536c0=false;tick();
                CHECK(f.v.u8(0x12)==preview.u8(8));
                input.feature_mask=4;tick();CHECK(action==4&&runtime->start_mode.course_choice_655b59==VehicleMenuModels[selected]&&
                    runtime->start_mode.vehicle_variant_83036d==selected/15&&runtime->start_mode.vehicle_colour_655b5a==preview.u8(8));
                input.feature_mask=cycle%2?0x1000:0x2000;tick();
            }
            input={};input.device_held=0x2000;tick();input={};for(unsigned i=0;i<12;++i)tick();
            CHECK(frontend_vehicle_menu_suspend_4c8d90(menu,api));
            for(unsigned i=0;i<12;++i)CHECK(frontend_vehicle_loader_tick_48bfe0(f.loader,f.io));
            CHECK(frontend_vehicle_loader_empty_48bfc0(f.loader));
        }
        CHECK(frontend_vehicle_menu_init_4c9010(menu,api));f.fail=0x448960;
        CHECK(!frontend_vehicle_menu_control_4c9290(menu,api,action)&&menu.fault==0x448960);
    }
    {VehicleCreationQueue q;q.object.fill(0xa5);CHECK(vehicle_creation_reset_49fa60(q));
        CHECK(q.object[0]==0xa5&&q.object[0x100]==0xa5&&q.object[0x108]==0&&q.object[0x109]==0&&q.object[0x10a]==0xa5&&q.object[0x4d0]==0xa5);
        for(unsigned i=0;i<16;++i)CHECK(vehicle_creation_append_49fa80(q,i,i,i,i,i));
        auto before=q.object;CHECK(!vehicle_creation_append_49fa80(q,0,0,0,0,0)&&q.fault==0x49fa80&&q.object==before);}
    {VehicleCreationQueue q;auto before=q.object;CHECK(!vehicle_creation_preview_4406f0(q,9,1,0,{}));CHECK(q.fault==0x440110&&q.object==before);}
    std::printf("preview/preloader: %u checks; all models, delays, shared colour/flags, event lifetime and errors\n",checks);
}

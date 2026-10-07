#include "platform/frontend_sprites.hpp"
#include "platform/frontend_ui_resources.hpp"
#include "platform/frontend_preview_pack.hpp"
#include "platform/frontend_license_editor.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
using namespace outrun;
static unsigned checks;
static void req(bool ok,const char* msg){++checks;if(!ok){std::fprintf(stderr,"FAIL %s\n",msg);std::exit(1);}}
int main(int argc,char** argv){
    platform::FrontendSprites pool;
    req(pool.create(0x440000,0,0)==~0u,"missing bank never produces a synthetic handle");
    req(pool.bind_timing(0x44,{{52,60},{12,30}}),"explicit source metadata");
    req(!pool.bind_timing(0x44,{{0,60}}),"bad metadata rejected transactionally");
    for(unsigned layer=0;layer<pool.Layers;++layer){
        for(unsigned slot=0;slot<pool.Slots;++slot)
            req(pool.create(0x440000,layer,2)==layer*64+slot,"PC layer/slot handle allocation");
        req(pool.create(0x440000,layer,0)==~0u,"64 slots per layer");
    }
    req(pool.release(0)&&pool.create(0x440000,0,0)==0,"handle zero is valid and reusable");
    req(pool.release(67)&&pool.create(0x440001,1,0)==67,"released slot reused in its own layer");
    req(!pool.release(~0u),"invalid release safe");pool.reset();
    for(unsigned mode=0;mode<=5;++mode){
        auto h=pool.create(0x440000,mode,mode,0,2,true);
        pool.set_frame(h,2);pool.set_speed(h,1);
    }
    pool.tick();
    req(pool.get(0)->frame==0&&pool.status(0)==2,"loop wraps at upper boundary");
    req(pool.get(64)->frame==2&&pool.status(64)==3,"clamp ends with status 3");
    req(pool.get(128)->frame==2&&pool.status(128)==1,"mode 2 frozen");
    req(!pool.get(192)->allocated&&pool.used(3)==0,"mode 3 releases on end");
    req(pool.get(256)->frame==3&&pool.get(256)->allocated,"mode 4 advances until drawn");
    pool.drawn(256);req(!pool.get(256)->allocated&&pool.used(4)==0,"mode 4 released after draw");
    req(pool.get(320)->frame==2&&pool.status(320)==4,"mode 5 ends with status 4");
    pool.reset();const auto reverse=pool.create(0x440000,0,1,51,0,true);
    // 427F70 at the game's x87 precision (24 bits): 60*-1*(1/60) rounds to
    // exactly -1, so the frame reaches 0.0 after 51 ticks and crosses on the 52nd.
    pool.set_speed(reverse,-1);for(unsigned n=0;n<51;++n)pool.tick();
    req(pool.status(reverse)==1,"reverse stays active before endpoint");
    pool.tick();req(pool.status(reverse)==3,"reverse finishes after crossing endpoint");
    pool.reset();const auto equal=pool.create(0x440000,0,1,0,2,true);
    pool.set_frame(equal,2);pool.set_speed(equal,0);pool.tick();
    req(pool.status(equal)==1,"exact endpoint equality is not completion");
    pool.reset();const auto p=pool.create(0x440000,0,0),q=pool.create(0x440000,0,0,-1,-1,false,1);
    pool.tick({true,true,true});req(pool.get(p)->frame==0&&pool.get(q)->frame==1,"pause domains preserved");
    pool.tick({false,false,false});req(pool.get(q)->frame==1,"50 Hz scheduling gate");
    pool.tick({false,true,false});req(std::abs(pool.get(p)->frame-1.2f)<1e-6f,"retail 50 Hz constant");
    req(!pool.set_speed(p,std::numeric_limits<float>::infinity()),"nonfinite speed refused");
    pool.reset();auto a=pool.create(0x440000,0,3,0,0,true);auto b=pool.create(0x440000,0,3,0,0,true);
    pool.tick();req(!pool.get(a)->allocated&&!pool.get(b)->allocated,"release does not skip the next live slot");
    platform::FrontendUiResources ui{pool};std::array<std::uint8_t,0xa0> storage{};
    driving::Bytes resource(storage.data(),storage.size());std::uint32_t result{};
    req(ui.call(0x465160,resource,nullptr,0,result),"resource constructor");
    const std::array<std::uint32_t,11> args{{0x440000,0,2,5,1,0,0,0x3f800000,0x3f800000,0x3f800000,0}};
    req(ui.call(0x465860,resource,args.data(),args.size(),result),"resource config");
    req(ui.call(0x465970,resource,nullptr,0,result),"resource commit");
    req(ui.call(0x4652e0,resource,nullptr,0,result)&&result==0,"new resource is not ready");
    const auto h=resource.u32(8);req(h==320,"UI uses requested PC layer");
    resource.putf(0x40,30);resource.putf(0x44,-10);resource.putf(0x64,2);
    req(ui.finalize(resource)&&pool.get(h)->matrix[12]==30&&pool.get(h)->matrix[0]==2,"UI matrix reaches actual instance");
    platform::GameUiDraw draw{};draw.corners[0]={{320,240}};
    platform::frontend_sprite_transform(draw,*pool.get(h),640,480);
    req(draw.corners[0][0]==350&&draw.corners[0][1]==230,"instance transform around authored canvas centre");
    pool.tick();pool.tick();pool.tick();
    req(ui.call(0x4652e0,resource,nullptr,0,result)&&result==1,"ready follows completed animation");
    req(ui.call(0x465250,resource,nullptr,0,result)&&!pool.get(h)->allocated&&resource.u32(8)==~0u,"resource release owns its sprite");
    req(!ui.call(0x123456,resource,nullptr,0,result)&&ui.missing_pc==0x123456,"unknown UI leaves are explicit");
    // Retained parent command lifecycle against the actual shared sprite pool.
    // The timing is explicit here; retail ETC composition has a separate test.
    pool.reset();ui.missing_pc=0;
    req(pool.bind_timing(0x2c,std::vector<platform::FrontendSpriteTiming>(319,{51,60})),"command icon bank");
    const auto unrelated=pool.create(0x440000,14,0);
    std::array<std::uint8_t,0x68c> command_storage{};
    driving::Bytes commands(command_storage.data(),command_storage.size());driving::PcUiNotifyGlobals globals{};
    req(ui.commands(0x442ac0,commands,nullptr,0,globals,result),"command object constructor");
    const unsigned visible[]{1,0},hidden[]{0,0},immediate[]{0,1};
    const unsigned chooser[]{4,0x295,~0u,~0u,~0u,~0u,8,0x296};
    for(unsigned cycle=0;cycle<4;++cycle){
        req(ui.commands(0x4470f0,commands,nullptr,0,globals,result),"clear at menu re-entry");
        req(ui.commands(0x447000,commands,visible,2,globals,result),"show commands");
        req(ui.commands(0x440ea0,commands,chooser,8,globals,result)&&result==1,"original chooser command args");
        req(pool.used(14)==3,"two owner icons do not replace an unrelated sprite");
        auto* icon=pool.get(commands.u32(0x414));
        req(icon&&icon->allocated&&icon->first==0&&icon->last==25&&icon->speed==1,"primary animation original range");
        req(icon->matrix[12]==-252&&icon->matrix[13]==188,"original button placement");
        for(unsigned frame=0;frame<28;++frame){pool.tick();req(ui.commands(0x446cf0,commands,nullptr,0,globals,result),"command frame update");}
        icon=pool.get(commands.u32(0x414));
        req(icon&&icon->first==25&&icon->last==25,"completed intro becomes stationary icon");
        req(ui.commands(0x446fc0,commands,nullptr,0,globals,result)&&commands.u32(0x408)==1,"push actual command history");
        const unsigned menu=1;
        req(ui.commands(0x446ea0,commands,&menu,1,globals,result)&&result==1,"original owner menu table");
        const auto saved=command_storage;const unsigned missing=21;
        req(ui.commands(0x446ea0,commands,&missing,1,globals,result)&&result==0&&saved==command_storage,"absent table entry leaves history unchanged");
        req(ui.commands(0x447000,commands,hidden,2,globals,result),"animated command hide");
        icon=pool.get(commands.u32(0x414));
        req(icon&&icon->first==25&&icon->last==0&&icon->speed==-1,"hide uses reverse animation");
        req(ui.commands(0x447000,commands,immediate,2,globals,result)&&pool.used(14)==1,"immediate hide frees only owned icons");
        req(ui.commands(0x447090,commands,nullptr,0,globals,result)&&commands.u32(0x408)==0,"clear resets stack depth");
        req(pool.get(unrelated)->allocated,"other menu layer resources survive every lifecycle");
    }
    {
        std::array<std::uint8_t,0xc00> root_storage{};driving::Bytes root(root_storage.data(),root_storage.size());
        auto embedded=root.sub(0x51c,root.size()-0x51c);
        req(ui.commands(0x442ac0,embedded,nullptr,0,globals,result),"feedback embedded constructor");
        req(ui.input_feedback(root,4,1),"non-state-2 root emits no feedback");
        root.put32(0x218,2);
        req(ui.commands(0x447000,embedded,visible,2,globals,result),"feedback commands visible");
        req(ui.commands(0x440ea0,embedded,chooser,8,globals,result),"feedback command list");
        unsigned sound_calls{},sound_value{};std::array<unsigned*,2> sound{&sound_calls,&sound_value};
        ui.effect_user=&sound;ui.effect_4249f0=[](void* p,unsigned value){auto& s=*static_cast<std::array<unsigned*,2>*>(p);++*s[0];*s[1]=value;return true;};
        req(ui.input_feedback(root,4,-1)&&sound_calls==0,"negative argument keeps pressed icon without confirm sound");
        auto* icon=pool.get(embedded.u32(0x414));
        req(icon&&icon->first==25&&icon->last==50&&icon->speed==1,"pressed icon uses original refresh range");
        req(ui.input_feedback(root,4,1)&&sound_calls==1&&sound_value==0x40,"confirm sound policy");
        req(ui.input_feedback(root,8,-1)&&sound_calls==1,"negative cancel is silent");
        req(ui.input_feedback(root,8,256)&&sound_calls==2&&sound_value==0,"original signed byte feedback argument");
        req(ui.input_feedback(root,0x400,1)&&sound_calls==2,"absent command key does not invent feedback");
        ui.effect_4249f0=nullptr;ui.effect_user=nullptr;
        req(!ui.input_feedback(root,4,1)&&ui.missing_pc==0x4249f0,"missing required audio remains explicit");ui.missing_pc=0;
        req(ui.commands(0x447090,embedded,nullptr,0,globals,result),"feedback resources released");
        req(pool.used(14)==1&&pool.get(unrelated)->allocated,"feedback leaves other owners alive");
    }
    req(!ui.commands(0x440ea0,commands,chooser,7,globals,result)&&ui.missing_pc==0x440ea0,"incomplete original arguments are not silently accepted");
    if(argc==2){platform::FrontendPreviewPack pack;platform::GameUiPack animation;std::string error;
        req(platform::load_frontend_preview_pack_file(argv[1],pack,&error),error.c_str());
        req(platform::make_frontend_animation_view(pack,animation,&error),error.c_str());
        req(pool.bind(0x44,animation),"all retail root duration/rate records bind");
        for(unsigned i=0;i<animation.scene_count;++i){pool.reset();const auto id=pool.create(0x440000+i,0,1);
            req(id!=~0u,"every retail scene creates an instance");
            std::printf("scene=%u frames=%.1f rate=%.1f\n",i,pool.get(id)->last+1,pool.get(id)->rate);
        }
        // The actual editor initialization now executes against real graphics
        // services and retail scene timing, not an always-ready callback.
        pool.reset();
        struct EditorFixture {
            std::array<std::uint8_t,platform::PcLicenseEditorBytes> object{};
            platform::FrontendUiResources ui;
        } editor{{},platform::FrontendUiResources{pool}};
        driving::Bytes object(editor.object.data(),editor.object.size());
        for(auto offset:{0x4cu,0xecu,0x18cu,0x22cu,0x2ccu,0x36cu,0x40cu,0x4acu,0x54cu})
            req(editor.ui.call(0x465160,object.sub(offset,0xa0),nullptr,0,result),"construct editor graphics resource");
        platform::LicenseEditorServices services{};services.user=&editor;
        services.call=[](void* p,std::uint32_t pc,std::size_t offset,const std::uint32_t* args,std::size_t n,std::uint32_t& out){
            auto& e=*static_cast<EditorFixture*>(p);
            if(offset>e.object.size()||e.object.size()-offset<0xa0)return false;
            return e.ui.call(pc,driving::Bytes(e.object.data()+offset,0xa0),args,n,out);
        };
        platform::PcLicense license{};platform::frontend_license_reset_4471a0(license,12345);
        req(platform::license_editor_init_4dd7c0(object,license,services),"original editor creates actual retail graphics instances");
        req(pool.used(4)==1&&pool.used(5)==1,"editor foreground/background have distinct layers");
        auto panel=object.sub(0x4ac,0xa0);
        req(editor.ui.call(0x4652e0,panel,nullptr,0,result)&&result==0,"editor waits for its live animation");
        for(unsigned i=0;i<60;++i)pool.tick();
        req(editor.ui.call(0x4652e0,panel,nullptr,0,result)&&result==1,"retail animation unlocks editor readiness");
        const auto* sprite=pool.get(panel.u32(8));std::vector<platform::GameUiDraw> draws;
        req(platform::game_ui_scene_frame_draws(animation,sprite->token&0xffffu,sprite->frame,draws)&&!draws.empty(),"editor instance resolves to retail atlas layers");
    }
    std::printf("frontend_sprites: %u checks passed\n",checks);
}

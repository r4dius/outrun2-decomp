// Host visual check of the ported renderer: loads a retail car PMT and the
// colour bank with the native loader into the software D3D9 device, runs the
// SCN_ENV event (449FC0 init, 44A890 control in root mode 0x20 with no course
// environment lists, as in the frontend before any course is loaded), the
// frontend camera (484EE0 + 485FE0 mode-32 preset) and the frame renderer
// 449050 with event 8 (CAR01, display 49F500 -> 46C140) and event 396
// (EXEC_DRAW, display 405830), and writes a PNG. The car object is a minimal
// preview car (model, colour, start position), not a full 4A7270 run.
//
// Usage: pc_render_car <retail data dir> <car 0..29> <preset 0|1> <colour byte> <out.png>
#include "driving/pc_camera.hpp"
#include "platform/embedded_camera_data.hpp"
#include "platform/embedded_vehicle_draw_data.hpp"
#include "platform/frontend_vehicle_data.hpp"
#include "platform/pc_frame_render.hpp"
#include "platform/pc_render_flush.hpp"
#include "platform/pc_scene_environment.hpp"
#include "platform/pc_vehicle_display.hpp"
#include "platform/pc_soft_d3d9.hpp"
#include "platform/vehicle_model_draw.hpp"
#include "platform/vehicle_preview_init.hpp"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <zlib.h>
using namespace outrun;
using namespace outrun::platform;
using driving::Bytes;
namespace {
std::vector<std::uint8_t> inflate(const std::string& path){
    std::ifstream in(path,std::ios::binary);if(!in)throw std::runtime_error("missing "+path);
    std::vector<std::uint8_t> packed((std::istreambuf_iterator<char>(in)),std::istreambuf_iterator<char>());
    std::vector<std::uint8_t> out(64u*1024u*1024u);uLongf size=uLongf(out.size());
    if(uncompress(out.data(),&size,packed.data(),uLong(packed.size()))!=Z_OK)throw std::runtime_error("inflate "+path);
    out.resize(size);return out;
}
struct Bank { std::vector<std::uint8_t> pmt; PcPmtResources r; };
void load(Bank& b,const std::string& path,PcD3D9Device& dev,PcShaderCache& cache){
    b.pmt=inflate(path);Bytes pb(b.pmt.data(),b.pmt.size());
    const auto ss=pb.u32(8),vs=pb.u32(12);
    b.r.system.assign(b.pmt.begin()+16,b.pmt.begin()+16+ss);b.r.video=b.pmt.data()+16+ss;b.r.video_size=vs;
    PcShaderGlobals globals{};PcPmtLoadCursor cur{};
    pmt_objects_begin_42e490(b.r,cur);while(!pmt_objects_step_42e4f0(b.r,cur,dev,cache,globals)){}
    pmt_textures_begin_42e850(b.r,cur);while(!pmt_textures_step_42e8c0(b.r,cur,dev)){}
}
}
int main(int argc,char** argv){
    try{
        if(argc!=6){std::fprintf(stderr,"usage: pc_render_car <data dir> <car> <preset> <colour> <out.png>\n");return 2;}
        const std::string root=argv[1];const unsigned car=unsigned(std::atoi(argv[2]));const int preset=std::atoi(argv[3]);
        const int colour_byte=std::atoi(argv[4]);
        if(car>=30)throw std::runtime_error("car index");
        driving::PcCameraScreen screen{};
        PcSoftD3D9Device dev(unsigned(screen.width_740c8c),unsigned(screen.height_740c90));
        PcShaderCache cache{};
        Bank colour,vehicle;
        load(colour,root+"/COMMON/obj_pc_color_pmt.sz",dev,cache);
        load(vehicle,root+"/Cars/"+std::string(VehicleResourcePaths[car]+5),dev,cache);
        const unsigned resource=VehicleResourceIds[car];
        std::map<std::uint32_t,PcPmtResources*> banks{{0xbbu,&colour.r},{resource,&vehicle.r}};
        PcRenderGlobals g;
        PcFlushContext fc{dev,g,[&](std::uint32_t r)->PcPmtResources*{auto it=banks.find(r);return it==banks.end()?nullptr:it->second;},
            pc_colour_list_alt_408c80,1};
        render_state_init_40f6b0(fc);
        g.putf(0x8a8c1c,1.f);
        // Matrix stack 89B564.
        std::vector<std::uint8_t> marena(64*64);driving::PcMatrixStack ms{Bytes(marena.data(),marena.size()),0,0,64};
        driving::pc_matrix_identity(ms);
        // Preview car (the fields read by 46C140/46AE70).
        std::vector<std::uint8_t> carv(0x10f0);Bytes cb(carv.data(),carv.size());
        cb.put8(0x11,std::uint8_t(car));cb.put8(0x12,std::uint8_t(colour_byte));cb.put32(4,0x4081u);
        cb.putf(0x58,1.f);cb.put32(0x5c,1);
        for(unsigned k=0;k<16;++k)cb.putf(0xf0+k*4,(k%5u)==0u?1.f:0.f);
        for(unsigned k=0;k<16;++k)cb.putf(0xb0+k*4,(k%5u)==0u?1.f:0.f);
        {   const auto start=vehicle_preview_start_position(0,0);
            cb.putf(0x14,start.x);cb.putf(0x18,start.y);cb.putf(0x1c,start.z);
            cb.putf(0xb0+0x30,start.x);cb.putf(0xb0+0x34,start.y);cb.putf(0xb0+0x38,start.z);}
        cb.putf(0xb68,1.f);
        // Camera 79F574: 484EE0 then 485FE0 (mode 32 preset), stores through 410F90.
        std::vector<std::uint8_t> cam(0x400),live_fe8(0x64c),live_bb0(0x438),live_750(0x5460);
        Bytes c(cam.data(),cam.size());
        driving::PcCameraTables tables{Bytes(const_cast<std::uint8_t*>(EmbeddedCameraTable5b4a30),0x64c),
            Bytes(const_cast<std::uint8_t*>(EmbeddedCameraTable5ba4e0),0x438),Bytes(const_cast<std::uint8_t*>(EmbeddedCameraTable5b5080),0x5460),
            Bytes(live_fe8.data(),live_fe8.size()),Bytes(live_bb0.data(),live_bb0.size()),Bytes(live_750.data(),live_750.size())};
        const driving::PcCameraBlend blend{0,0,0,1.f};
        driving::PcCameraDevice camdev;
        driving::camera_init_484ee0(c,ms,camdev,screen,blend,0,tables);
        camdev=driving::PcCameraDevice{};
        driving::PcCameraControl control{0,32,preset,0,cb,Bytes(const_cast<std::uint8_t*>(EmbeddedCameraTable5ba4e0),0x438)};
        if(!driving::camera_control_485fe0(c,ms,camdev,screen,blend,control))throw std::runtime_error("camera control");
        for(const auto& st:camdev.stores){std::array<float,16> m{};std::memcpy(m.data(),st.matrix.data(),64);render_set_matrix_410f90(fc,m,st.slot);}
        // SCN_ENV (event 386): init, then the root-mode-0x20 control (44A8DF).
        PcSceneEnvironment env;scene_environment_init_449fc0(env,dev,ms);
        std::vector<std::uint8_t> saved_sun(0x1e0),saved_fog(0x54),flags(24),nearest(24),matrix_7d2da0(64);
        for(unsigned k=0;k<16;++k)Bytes(matrix_7d2da0.data(),64).putf(k*4,(k%5u)==0u?1.f:0.f);
        {   Bytes fl(flags.data(),flags.size());for(unsigned k=0;k<6;++k)fl.put32(k*4,env.flags_7d28b0[k]);}
        driving::PcEnvironmentBlendContext ec{env.phase_7d28c8,0,0.f,Bytes(saved_sun.data(),saved_sun.size()),Bytes(saved_fog.data(),saved_fog.size()),
            Bytes(env.lights_899b98.data(),3*0xa0),Bytes(env.fog_7d3a10.data(),env.fog_7d3a10.size()),Bytes(matrix_7d2da0.data(),64),ms};
        driving::PcEnvironmentFrame ef{};
        ef.vehicle=cb;ef.camera=c;ef.flags_7d28b0=Bytes(flags.data(),flags.size());ef.nearest_7d2d58=Bytes(nearest.data(),nearest.size());
        ef.lights_899d78=Bytes(env.lights_899b98.data()+0x1e0,6*0xa0);
        const Bytes none(nullptr,0);
        driving::CourseCollisionTables no_course{driving::CourseRunTables{none,none,none,0,false,false},none,none,none,none,0,false};
        PcSceneEnvironmentControl ctl{};ctl.mode_78026c=0x20;
        ctl.update_44a8df=[&]{driving::course_environment_update_44a8df(ef,no_course,driving::PcEnvironmentTransition{},ec);};
        scene_environment_control_44a890(env,ctl);
        env.phase_7d28c8=ec.phase_7d28c8;
        // Events: 8 CAR01 (preview display 49F500), 386 SCN_ENV, 396 EXEC_DRAW.
        driving::PcEventControlState events{};
        events.slots[8].flags=2;events.slots[8].event_id=8;events.slots[8].work_token=1;
        events.slots[8].display_scene=0x100;events.slots[8].disp_callback=0x49f500;
        events.slots[386].flags=2;events.slots[386].event_id=386;
        events.slots[396].flags=2;events.slots[396].event_id=396;events.slots[396].display_scene=0x1e0;events.slots[396].disp_callback=0x405830;
        // Queues and the 46C140 context.
        PcRenderQueue opaque,alpha;render_queues_reset_405160(opaque,alpha);
        PcRenderView view{};std::memcpy(view.view_95d860.data(),camdev.slots_95d860[0].data(),64);
        view.frustum_95bf40=camdev.frustum_95bf40;view.planes_95bf58=camdev.planes_95bf58;
        for(unsigned k=0;k<3;++k){view.eye[k]=c.f32(0xf8+k*4);view.target[k]=c.f32(0x104+k*4);}
        PcModelBank qbank{resource,vehicle.r.view(),vehicle.r.object_count_0c};
        PcRenderContext qc{opaque,alpha,view,ms,[&](std::uint32_t id)->const PcModelBank*{return id==resource?&qbank:nullptr;},7};
        qc.immediate_8999b0=1;qc.flush_4052c0=[&]{render_queue_flush_4052c0(fc,qc);};
        std::vector<std::uint8_t> body(0x800);
        PcEnvironmentRenderTables tables_env{Bytes(env.lights_899b98.data(),env.lights_899b98.size()),Bytes(env.fog_7d3a10.data(),env.fog_7d3a10.size()),0};
        PcFrameState fs{};fs.clear_colour_89bd5c=0xff404850u;fs.device_ready_7d2614=1;
        PcVehicleDisplayContext vc{fc,qc,tables_env,2,0,ms,Bytes(body.data(),body.size()),32,0};
        PcSceneDisplayGlobals sg{};
        struct Owner{PcVehicleDisplayContext* vc;PcFrameState* fs;Bytes car;PcFlushContext* fc;PcRenderQueue* alpha;unsigned displays{};};
        Owner owner{&vc,&fs,cb,&fc,&alpha};
        auto alpha_flush=[](Owner& o){std::vector<std::uint32_t> order(o.alpha->count);for(std::uint32_t k=0;k<o.alpha->count;++k)order[k]=k;
            if(o.alpha->count)render_queue_sort_4499e0(*o.alpha,order,0,std::int32_t(o.alpha->count)-1);
            render_queue_flush_405890(*o.fc,*o.alpha,order);o.alpha->count=0;o.alpha->matrix_count=0;};
        static decltype(alpha_flush)* flush_fn=&alpha_flush;
        PcSceneDisplayServices display{&owner,
            [](void* u,std::uint32_t callback,std::uint32_t,std::uint32_t){auto& o=*static_cast<Owner*>(u);
                if(callback==0x49f500u){o.vc->layer_7d25f0=o.fs->layer_7d25f0;vehicle_display_46c140(*o.vc,o.car);++o.displays;}
                else if(callback==0x405830u)(*flush_fn)(o);
                else throw std::runtime_error("display callback not ported");},
            nullptr,
            [](void* u){(*flush_fn)(*static_cast<Owner*>(u));},
            [](void* u){render_lights_reset_410740(*static_cast<Owner*>(u)->fc);},
            [](void* u,std::uint8_t* l){render_light_add_4107a0(*static_cast<Owner*>(u)->fc,Bytes(l,0x94));}};
        PcFrameInputs in{};in.game_mode_78026c=0x20;in.screen_740c94=1.f;in.screen_740c98=1.f;
        for(unsigned k=0;k<16;++k)in.matrix_7d2da0[k]=(k%5u)==0u?1.f:0.f;
        std::vector<std::uint32_t> leaves;
        PcFrameServices sv{[&](std::uint32_t pc,std::uint32_t){leaves.push_back(pc);},events,sg,env.lights_899b98.data()+0x140,display,tables_env};
        frame_render_449050(fc,fs,in,sv);
        if(!dev.write_png(argv[5]))throw std::runtime_error("png");
        std::printf("car displays %u, triangles %llu, pixels %llu, errors %zu, unported leaves",owner.displays,(unsigned long long)dev.triangles_drawn,
            (unsigned long long)dev.pixels_shaded,dev.errors.size());
        for(auto pc:leaves)std::printf(" %x",pc);
        std::printf("\n");
        for(const auto& e:dev.errors)std::fprintf(stderr,"device: %s\n",e.c_str());
        return 0;
    }catch(const std::exception& e){std::fprintf(stderr,"pc_render_car: %s\n",e.what());return 1;}
}

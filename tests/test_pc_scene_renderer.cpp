// PcSceneRenderer: SCN_ENV callbacks owned natively, one 449050 frame over a
// runtime-like event state on the software reference device, honest
// reporting of unported displays/leaves and of unanswered 44A890 gates.
#include "platform/pc_scene_renderer.hpp"
#include "platform/pc_soft_d3d9.hpp"
#include "platform/retail_asset_store.hpp"
#include "platform/vehicle_preview_init.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace outrun;
namespace {
unsigned checks=0;
void require(bool ok,const char* what){++checks;if(!ok){std::fprintf(stderr,"FAIL: %s\n",what);std::exit(1);}}
}
int main(int argc,char** argv){
    platform::PcSoftD3D9Device device(640,480);
    platform::PcSceneRenderer r(device);
    driving::PcEventControlState events{};
    events.slots[386].flags=2;events.slots[386].event_id=386;
    events.slots[396].flags=2;events.slots[396].event_id=396;events.slots[396].display_scene=0x1e0;events.slots[396].disp_callback=0x405830;
    events.slots[398].flags=2;events.slots[398].event_id=398;events.slots[398].display_scene=0x20;events.slots[398].disp_callback=0x428170;
    require(!r.invoke(0x12345678u,0,1,0x20,0),"foreign callbacks are not owned");
    require(r.invoke(0x449fc0u,0,386,0x20,0)&&r.environment_inits==1,"SCN_ENV init owned");
    driving::Bytes lights(r.environment().lights_899b98.data(),0x960);
    require(lights.u32(0)==1&&lights.u32(4)==3&&lights.u32(0x140)==1&&lights.u32(0x144)==3,"407940 sun defaults");
    require(lights.u32(0x1e0)==0&&lights.u32(0x1e4)==1&&lights.u32(0x5a0)==0,"407940 local lights disabled");
    require(r.environment().phase_7d28c8==3,"449FC0 phase 3");
    // Control without the shared car: reported, nothing guessed.
    require(r.invoke(0x44a890u,0,386,0x20,0)&&r.environment_unanswered==1&&r.environment_controls==0,"44A8DF needs the car");
    std::vector<std::uint8_t> car(0x1000),camera(0x400);
    r.vehicle_camera=[&](driving::Bytes& v,driving::Bytes& c){v=driving::Bytes(car.data(),car.size());c=driving::Bytes(camera.data(),camera.size());return true;};
    require(r.invoke(0x44a890u,0,386,0x20,0)&&r.environment_controls==1,"44A8DF over absent lists");
    const float px=lights.f32(0x140+0x38);
    require(px!=0.f||lights.f32(0x140+0x44)==0.f,"408310 sun position from its direction");
    // Mode 3 needs the protected 49EED0 bridge: reported.
    const auto before=r.environment_unanswered;
    require(r.invoke(0x44a890u,0,386,3,0)&&r.environment_unanswered==before+1,"49EED0 gate reported");
    // One frame.
    device.clear(0xff000000u,1.f);
    r.render_frame(events,0x20);
    require(r.frames==1&&device.errors.empty(),"frame rendered without device errors");
    require(r.alpha_flushes>=4,"EXEC_DRAW flushes on layers 5..8");
    require(r.unported_displays.count(0x428170)==1,"SPRANI display reported as unported");
    require(r.unported_leaves.count(0x414340)&&r.unported_leaves.count(0x42d710),"layer-0 and 42D710 leaves reported");
    // 40EC70 cleared the back buffer with 89BD5C (0 here): opaque black.
    const auto& rgba=device.rgba();
    require(rgba[0]==0&&rgba[3]==0,"448DB0 clear through the D3D9 form");
    if(argc>1){
        // Retail F50 through the owner: banks, CAMERA 385 init/control, event 8
        // display 49F500 -> 46C140 at layer 8.
        platform::RetailAssetStore store;std::string error;
        require(platform::retail_asset_store_open(store,argv[1],&error),"retail root");
        std::vector<std::uint8_t> colour,f50;
        require(platform::retail_asset_read_relative_inflated(store,"COMMON/obj_pc_color_pmt.sz",colour,128u<<20,&error),"colour bank archive");
        require(platform::retail_asset_read_relative_inflated(store,platform::VehicleResourcePaths[0],f50,128u<<20,&error),"F50 archive");
        platform::PcSoftD3D9Device dev(640,480);
        platform::PcSceneRenderer scene(dev);
        require(scene.load_bank(0xbb,colour,error)&&scene.load_bank(platform::VehicleResourceIds[0],f50,error),"banks loaded");
        // A reflection producer and a bank slot own independent references.
        const auto reflection=dev.create_cube_texture(16,1,1,21,0);
        auto* resource=scene.bank_resources(platform::VehicleResourceIds[0]);
        auto slots=resource->view();
        require(slots.u32(8)>0,"car bank has texture slots");
        const auto original_texture=slots.u32(slots.u32(resource->texture_table_24));
        dev.add_ref(original_texture);
        const auto token=platform::VehicleResourceIds[0]<<16;
        require(scene.texture_swap_406630(token,reflection)&&scene.texture_swap_406630(token,reflection),"repeated reflection assignment");
        unsigned rw=0,rh=0;
        require(dev.texture_level_size(reflection,0,rw,rh)&&rw==16,"same-handle swap does not delete producer");
        require(scene.texture_swap_406630(token,0),"reflection slot clears");
        require(dev.texture_level_size(reflection,0,rw,rh),"clearing slot retains producer");
        dev.release(reflection);require(!dev.texture_level_size(reflection,0,rw,rh),"last reflection reference releases");
        require(scene.texture_swap_406630(token,original_texture),"restore car texture after ownership regression");
        dev.release(original_texture);
        std::vector<std::uint8_t> carv(0x10f0),cam(0x400),body(0x800);
        driving::Bytes cb(carv.data(),carv.size());
        cb.put8(0x11,0);cb.put8(0x12,0);cb.put32(4,0x4081u);cb.putf(0x58,1.f);cb.put32(0x5c,1);cb.putf(0xb68,1.f);
        for(unsigned k=0;k<16;++k){cb.putf(0xf0+k*4,(k%5u)==0u?1.f:0.f);cb.putf(0xb0+k*4,(k%5u)==0u?1.f:0.f);}
        const auto start=platform::vehicle_preview_start_position(0,0);
        cb.putf(0x14,start.x);cb.putf(0x18,start.y);cb.putf(0x1c,start.z);
        cb.putf(0xb0+0x30,start.x);cb.putf(0xb0+0x34,start.y);cb.putf(0xb0+0x38,start.z);
        scene.vehicle_camera=[&](driving::Bytes& v,driving::Bytes& c){v=cb;c=driving::Bytes(cam.data(),cam.size());return true;};
        scene.body_view=[&](driving::Bytes& b){b=driving::Bytes(body.data(),body.size());return true;};
        driving::PcEventControlState ev{};
        ev.slots[8].flags=2;ev.slots[8].event_id=8;ev.slots[8].display_scene=0x100;ev.slots[8].disp_callback=0x49f500;
        ev.slots[385].flags=2;ev.slots[385].event_id=385;
        ev.slots[386].flags=2;ev.slots[386].event_id=386;
        ev.slots[396].flags=2;ev.slots[396].event_id=396;ev.slots[396].display_scene=0x1e0;ev.slots[396].disp_callback=0x405830;
        scene.set_events(&ev);
        require(scene.invoke(0x449fc0,0,386,0x20,0)&&scene.invoke(0x44a890,0,386,0x20,0),"SCN_ENV");
        require(scene.invoke(0x484ee0,0,385,0x20,0)&&scene.camera_inits==1,"CAMERA init");
        require(scene.invoke(0x485fe0,0,385,0x20,0)&&scene.camera_controls==1&&scene.camera_unanswered==0,"CAMERA control (mode 32)");
        scene.render_frame(ev,0x20);
        require(scene.car_displays==1&&scene.unported_displays.count(0x49f500)==0,"car displayed through 46C140");
        require(dev.triangles_drawn>1000&&dev.pixels_shaded>1000&&dev.errors.empty(),"car pixels drawn");
        if(argc>2)require(dev.write_png(argv[2]),"png");
        std::printf("pc_scene_renderer retail: triangles %llu pixels %llu\n",(unsigned long long)dev.triangles_drawn,(unsigned long long)dev.pixels_shaded);
    }
    std::printf("pc_scene_renderer: %u checks passed\n",checks);
}

// macOS adaptation of ps5/source/renderer_present.inc; reference baseline remains unchanged.
// Included inside outrun::mac_runtime after RendererImpl and the UI helpers.
namespace {
bool present_frame(RendererImpl& r){
    try{
        const auto begin=std::chrono::steady_clock::now();
        bool rasterize=true;
#ifdef OR2_MAC_HOST
        if(const char* n=std::getenv("OR2_MAC_RENDER_EVERY")){
            const auto every=std::max(1ul,std::strtoul(n,nullptr,10));rasterize=r.stats.frames%every==0u;
        }
        if(const char* first=std::getenv("OR2_MAC_RENDER_BEGIN")){
            rasterize=r.stats.frames>=std::strtoul(first,nullptr,10);
            if(const char* last=std::getenv("OR2_MAC_RENDER_END"))rasterize&=r.stats.frames<=std::strtoul(last,nullptr,10);
        }
#endif
        // Count-only is a host integration acceleration only. The real macOS
        // binary always executes the pixel shaders and presents every frame.
        r.device->count_only=!rasterize;
#ifndef OR2_MAC_OPENGL
        if(r.video_pending){r.video_pending=false;r.device->resize_back_buffer(r.video_width,r.video_height);}
        r.device->set_ui_rect(g_widescreen);r.device->set_fxaa(r.video_fxaa);r.device->set_msaa(r.video_msaa);
#endif
        r.device->begin_frame();
        if(!r.start_loading_visible&&r.scene){
            ProfileScope record(profile_key(ProfileRenderer,0,ProfPcRecord));
            r.device->clear(3u,0xff000000u,1.0f,0u);
            r.scene(*r.device);
            if(!r.device->errors.empty())throw std::runtime_error(r.device->errors.back());
        }
        {ProfileScope fence(profile_key(ProfileRenderer,0,ProfPcFence));r.device->end_frame();}
        if(rasterize)r.stats.last_gpu_ns=r.device->gpu_nanoseconds();
        if(rasterize)frame_profile().add(profile_key(ProfileRenderer,0,ProfPcGpu),r.stats.last_gpu_ns);
        r.device->count_only=false;
        if(!rasterize)return true;
        auto& display=*r.display;display.clear();
        if(r.start_loading_visible){
            const auto index=r.loading_scene_index+r.loading_scene_offset;
            if(index>=r.frontend.scenes.size())throw std::runtime_error("loading scene index outside bank");
            const auto token=r.frontend.scenes[index].token;
            std::vector<platform::GameUiDraw> draws;
            if(!platform::game_ui_scene_frame_draws(r.loading_animation,token&0xffffu,r.loading_frame,draws,&r.last_error))return false;
            // The loading pack's textures follow the frontend's in the shared bank (append_start_loading).
            constexpr auto base=platform::FrontendPreviewSourceTextureCount;
            for(auto& d:draws){const auto slot=frontend_texture_slot(r.frontend,d.texture+base);
                if(slot>=r.frontend.textures.size())throw std::runtime_error("loading texture outside bank");
                const auto& texture=r.frontend.textures[slot];
                display.quad(make_animated_frontend_quad(r.frontend,d,base),display.texels(&texture,texture.format,texture.width,texture.height,texture.bytes),texture.width,texture.height);}
            r.stats.loading_animation_frame=r.loading_frame++;++r.stats.loading_animation_updates;
        }else{
            const auto& pc=*r.device;
            const auto ow=g_widescreen?FramebufferWidth:FramebufferHeight*4u/3u;
            const auto ox=(FramebufferWidth-ow)/2u;
#ifdef OR2_MAC_OPENGL
            display.pc_frame(pc.back_texture(),int(ox),0,int(ow),int(FramebufferHeight));
#else
            // D3D9 presents RGB regardless of the back buffer alpha. The PC
            // sprite flush writes RGB only; treating it as an overlay hides it.
            display.rgba(r.device.get(),pc.rgba().data(),pc.presentation_width(),pc.presentation_height(),int(ox),0,int(ow),int(FramebufferHeight),true);
#endif
            // The title movie stays 4:3 in a 16:9 frame.
            constexpr auto mw=FramebufferHeight*4u/3u,mx=(FramebufferWidth-mw)/2u;
            if(r.movie_visible&&!r.movie_rgba.empty())display.rgba(&r.movie_rgba,r.movie_rgba.data(),r.movie_width,r.movie_height,int(mx),0,int(mw),int(FramebufferHeight));
            if(r.frontend_visible){
                if(r.frontend_sprites)paint_frontend(r,display,r.stats.frames);
                else for(const auto& d:r.frontend_frame_draws){const auto slot=frontend_texture_slot(r.frontend,d.texture);
                    if(slot>=r.frontend.textures.size())throw std::runtime_error("frontend texture outside bank");
                    const auto& t=r.frontend.textures[slot];display.quad(make_animated_frontend_quad(r.frontend,d,0u),display.texels(&t,t.format,t.width,t.height,t.bytes),t.width,t.height);}
                if(r.frontend_overlay_token){std::vector<platform::GameUiDraw> draws;
                    if(!platform::game_ui_scene_frame_draws(r.frontend_animation,r.frontend_overlay_token&0xffffu,r.frontend_overlay_frame,draws,&r.last_error))return false;
                    for(const auto& d:draws){const auto slot=frontend_texture_slot(r.frontend,d.texture);if(slot>=r.frontend.textures.size())return false;
                        const auto& t=r.frontend.textures[slot];display.quad(make_animated_frontend_quad(r.frontend,d,0u),display.texels(&t,t.format,t.width,t.height,t.bytes),t.width,t.height);}}
            }
        }
        if(!display.present(r.last_error))return false;
        if(r.screen_hz_age++%120u==0u)r.screen_hz=screen_max_frames_per_second();   // the window may move to another screen
        if(enhancements::display_rate()<=60u&&r.screen_hz>=100u){
            const auto due=r.last_present+std::chrono::microseconds(16667);
            if(std::chrono::steady_clock::now()<due)std::this_thread::sleep_until(due);
        }
        r.last_present=std::chrono::steady_clock::now();
        const auto us=std::uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-begin).count());
        ++r.raster_frames;r.raster_us+=us;r.raster_peak_us=std::max(r.raster_peak_us,us);
        return true;
    }catch(const std::exception& e){r.last_error=e.what();std::fprintf(stderr,"macOS renderer: %s\n",e.what());return false;}
}
}
platform::PcD3D9Device* mac_renderer_pc_device(MacRenderer& renderer){
    auto* r=static_cast<RendererImpl*>(renderer.impl);return r?r->device.get():nullptr;
}
bool mac_renderer_set_pc_scene(MacRenderer& renderer,std::function<void(platform::PcD3D9Device&)> scene){
    auto* r=static_cast<RendererImpl*>(renderer.impl);if(!r)return false;r->scene=std::move(scene);return true;
}
std::string mac_renderer_pc_error(const MacRenderer& renderer){
    const auto* r=static_cast<const RendererImpl*>(renderer.impl);return r?r->last_error:"macOS renderer not initialized";
}

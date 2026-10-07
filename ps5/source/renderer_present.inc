// Included inside outrun::ps5_runtime after RendererImpl and the UI helpers.
namespace {
bool present_frame(RendererImpl& r){
    try{
        const auto begin=std::chrono::steady_clock::now();
        bool rasterize=true;
#ifdef OR2_PS5_HOST_TEST
        if(const char* n=std::getenv("OR2_PS5_RENDER_EVERY")){
            const auto every=std::max(1ul,std::strtoul(n,nullptr,10));rasterize=r.stats.frames%every==0u;
        }
        if(const char* first=std::getenv("OR2_PS5_RENDER_BEGIN")){
            rasterize=r.stats.frames>=std::strtoul(first,nullptr,10);
            if(const char* last=std::getenv("OR2_PS5_RENDER_END"))rasterize&=r.stats.frames<=std::strtoul(last,nullptr,10);
        }
#endif
        // Count-only is a host integration acceleration only. The real PS5
        // binary always executes the pixel shaders and presents every frame.
        r.device->count_only=!rasterize;
#if defined(OR2_PS5_OPENGL) || defined(OR2_PS5_VULKAN)
        if(r.video_pending){r.video_pending=false;r.device->resize_back_buffer(r.video_width,r.video_height);}
        r.device->set_ui_rect(g_widescreen);r.device->set_fxaa(r.video_fxaa);r.device->set_msaa(r.video_msaa);
        r.device->begin_frame();
#endif
        if(!r.start_loading_visible&&r.scene){
            r.device->clear(3u,0xff000000u,1.0f,0u);
            r.scene(*r.device);
            if(!r.device->errors.empty())throw std::runtime_error(r.device->errors.back());
        }
#if defined(OR2_PS5_OPENGL) || defined(OR2_PS5_VULKAN)
        r.device->end_frame();
        if(rasterize)r.stats.last_gpu_ns=r.device->gpu_nanoseconds();
#endif
        r.device->count_only=false;
        const auto recorded=std::chrono::steady_clock::now();
        r.stats.last_record_ns=std::uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(recorded-begin).count());
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
#if defined(OR2_PS5_OPENGL) || defined(OR2_PS5_VULKAN)
            display.pc_frame(pc.back_texture(),int(ox),0,int(ow),int(FramebufferHeight));
#else
            display.rgba(r.device.get(),pc.rgba().data(),pc.width(),pc.height(),int(ox),0,int(ow),int(FramebufferHeight));
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
        const auto swap_begin=std::chrono::steady_clock::now();
        if(!display.present(r.last_error))return false;
#ifdef OR2_PS5_VULKAN
        // Native end_frame defers submission until scene + compositor are ready.
        r.stats.last_gpu_ns=r.device->gpu_nanoseconds();
#endif
        const auto swap_end=std::chrono::steady_clock::now();
        r.stats.last_present_ns=std::uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(swap_end-swap_begin).count());
#if (defined(OR2_PS5_OPENGL) || defined(OR2_PS5_VULKAN)) && defined(OR2_PS5_PAYLOAD)
        // One bounded report per 120 frames; never emit/flush logs per draw.
        struct Timings {std::uint64_t record{},compose{},swap{},frames{},draws{};};
        static Timings timings;
        timings.record+=r.stats.last_record_ns;
        timings.compose+=std::uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(swap_begin-recorded).count());
        timings.swap+=r.stats.last_present_ns;
        const auto draws=r.device->submitted_draws;
        if(++timings.frames==120){
            std::fprintf(stdout,"[ps5-frame] record=%.3fms compose=%.3fms swap=%.3fms draws=%.1f resolution=%ux%u\n",
                timings.record/120e6,timings.compose/120e6,timings.swap/120e6,
                double(draws-timings.draws)/120,r.device->presentation_width(),r.device->presentation_height());
#ifdef OR2_PS5_VULKAN
            static std::uint64_t vertex_ns=0,command_ns=0,native_draws=0,timed_frames=0;
            const auto vertices=r.device->vertex_prepare_ns,commands=r.device->command_record_ns;
            const auto native=r.device->native_vertex_draws;
            const auto samples=r.device->timed_frames-timed_frames;
            const double divisor=double(std::max<std::uint64_t>(1,samples))*1e6;
            std::fprintf(stdout,"[ps5-vk-cpu] vertices+clip=%.3fms commands=%.3fms GPU-vertex-draws=%.1f sample-frames=%llu\n",(vertices-vertex_ns)/divisor,(commands-command_ns)/divisor,double(native-native_draws)/120,(unsigned long long)samples);
            timed_frames=r.device->timed_frames;
            native_draws=native;
            vertex_ns=vertices;command_ns=commands;
            static std::uint64_t state_commands=0,state_saved=0,index_hits=0,index_scans=0,sets=0,sets_reused=0;
            const auto& vk=r.display->context();
            std::fprintf(stdout,"[ps5-vk-cache] states=%.1f skipped=%.1f index-hits=%.1f scanned=%.1f descriptors=%.1f reused=%.1f\n",
                double(vk.state_commands-state_commands)/120,double(vk.state_commands_saved-state_saved)/120,
                double(r.device->index_range_hits-index_hits)/120,double(r.device->index_scan_count-index_scans)/120,
                double(vk.descriptor_sets_allocated-sets)/120,double(vk.descriptor_sets_reused-sets_reused)/120);
            state_commands=vk.state_commands;state_saved=vk.state_commands_saved;
            index_hits=r.device->index_range_hits;index_scans=r.device->index_scan_count;
            sets=vk.descriptor_sets_allocated;sets_reused=vk.descriptor_sets_reused;
            const auto& profile=runtime::frame_profile();
            if(profile.frames){runtime::frame_profile_report(stdout,profile,10);}
#endif
            timings={0,0,0,0,draws};
        }
#endif
        const auto us=std::uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-begin).count());
        ++r.raster_frames;r.raster_us+=us;r.raster_peak_us=std::max(r.raster_peak_us,us);
        return true;
    }catch(const std::exception& e){r.last_error=e.what();std::fprintf(stderr,"PS5 renderer: %s\n",e.what());return false;}
}
}
platform::PcD3D9Device* ps5_renderer_pc_device(Ps5Renderer& renderer){
    auto* r=static_cast<RendererImpl*>(renderer.impl);return r?r->device.get():nullptr;
}
bool ps5_renderer_set_pc_scene(Ps5Renderer& renderer,std::function<void(platform::PcD3D9Device&)> scene){
    auto* r=static_cast<RendererImpl*>(renderer.impl);if(!r)return false;r->scene=std::move(scene);return true;
}
std::string ps5_renderer_pc_error(const Ps5Renderer& renderer){
    const auto* r=static_cast<const RendererImpl*>(renderer.impl);return r?r->last_error:"PS5 renderer not initialized";
}

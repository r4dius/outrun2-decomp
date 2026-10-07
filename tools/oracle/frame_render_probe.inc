if(only=="frame_render_probe"){
    // Original frame renderer 449050 (with 448DB0/448ED0/448E40/40EC70,
    // 410670, 404540, 40D840, 4089A0, 40BD80/40BE70, 4400C0, 440CA0/43FB40
    // and the 410740/4107A0/405830 calls of 43FB40 running unchanged) on the
    // logging fake IDirect3DDevice9, against frame_render_449050. The
    // subsystems that are not ported (course passes, 414340, glare, layer
    // callbacks, 42D710, 49F4D0, event display callbacks) are logging stubs
    // on the guest side and leaf services on the native side. Device calls,
    // leaf calls (in order), renderer globals and the frame state compared.
    using namespace outrun::platform;
    using outrun::driving::PcEventControlState;
    if(argc<=7||std::string(argv[7])=="-")throw std::runtime_error("frame render oracle requires pinned d3dx9_29.dll");
    const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
    constexpr unsigned base=0x3a800000,works=0x3a900000,Stubs=base+0x1000,LeafLog=base+0x8000,LeafCap=0x800;
    map_at(base,0x40000,PROT_READ|PROT_WRITE|PROT_EXEC);map_at(works,0x40000,PROT_READ|PROT_WRITE);
    map_original_arithmetic_bridge(file);
    init_matrix_oracle_globals();wire_d3dx_imports(map_d3dx29(argv[7]));
    {   auto* iat=reinterpret_cast<void*>(0x596000u);
        if(mprotect(iat,4096,PROT_READ|PROT_WRITE))throw std::runtime_error("frame IAT");
        *reinterpret_cast<std::uint32_t*>(0x5962dcu)=D3DX_BASE+(0x440e9cu-0x400000u); // D3DXMatrixInverse (generic)
        if(mprotect(iat,4096,PROT_READ))throw std::runtime_error("frame IAT restore");}
    {   static const std::uint8_t finite_stub[]{0x8b,0x44,0x24,0x08,0x25,0x00,0x00,0xf0,0x7f,0x3d,0x00,0x00,0xf0,0x7f,0x0f,0x95,0xc0,0x0f,0xb6,0xc0,0xc3};
        std::memcpy(reinterpret_cast<void*>(base+0x7000),finite_stub,sizeof(finite_stub));
        auto* iat=reinterpret_cast<void*>(D3DX_BASE+0x1000u);
        if(mprotect(iat,4096,PROT_READ|PROT_WRITE))throw std::runtime_error("d3dx IAT page");
        *reinterpret_cast<std::uint32_t*>(D3DX_BASE+0x1270u)=base+0x7000u;
        if(mprotect(iat,4096,PROT_READ))throw std::runtime_error("d3dx IAT restore");}
    for(std::uint32_t page=0x62f000u;page<0x98d000u;page+=0x1000u)mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE);
    fake_d3d9::install();
    auto g32=[](std::uint32_t a){return *reinterpret_cast<const std::uint32_t*>(a);};
    auto p32=[](std::uint32_t a,std::uint32_t v){*reinterpret_cast<std::uint32_t*>(a)=v;};
    // Leaf logging stubs: record (id, argument) where the argument comes from
    // [esp+4], [esp+8], edx, eax or nothing.
    enum Src{None,Arg0,Arg1,Edx,Eax};
    unsigned next_stub=Stubs;
    auto stub=[&](std::uint32_t id,Src src){
        fake_d3d9::Emit e;
        e.u8({0x56,0x57,0x51,0x52,0x50});                       // push esi edi ecx edx eax
        e.u8({0x8b,0x0d});e.u32(LeafLog);                        // mov ecx,[count]
        e.u8({0x8d,0x3c,0xcd});e.u32(LeafLog+0x10);              // lea edi,[ecx*8+records]
        e.u8({0xc7,0x07});e.u32(id);                             // mov [edi],id
        switch(src){
        case None:e.u8({0xc7,0x47,0x04});e.u32(0);break;
        case Arg0:e.u8({0x8b,0x44,0x24,0x18,0x89,0x47,0x04});break;   // 5 pushes + ret: arg0 at +0x18
        case Arg1:e.u8({0x8b,0x44,0x24,0x1c,0x89,0x47,0x04});break;
        case Edx:e.u8({0x89,0x57,0x04});break;
        case Eax:e.u8({0x8b,0x04,0x24,0x89,0x47,0x04});break;       // saved eax
        }
        e.u8({0x41,0x89,0x0d});e.u32(LeafLog);                   // inc ecx; mov [count],ecx
        e.u8({0x58,0x5a,0x59,0x5f,0x5e,0xc3});
        std::memcpy(reinterpret_cast<void*>(next_stub),e.b.data(),e.b.size());
        const auto at=next_stub;next_stub+=0x40;return at;};
    struct Site{std::uint32_t site;std::uint8_t op;std::uint32_t id;Src src;};
    const Site sites[]{
        {0x449119,0xe8,0x4c50a0,None},{0x449165,0xe8,0x414340,None},{0x44918c,0xe8,0x414e50,None},{0x449191,0xe8,0x414f00,None},
        {0x4491d7,0xe8,0x42d710,Arg1},{0x4491fb,0xe8,0x49f4d0,Arg0},
        {0x40be0e,0xe8,0x40bf10,Edx},{0x40be2d,0xe8,0x40c1b0,Arg0},{0x40be4b,0xe8,0x40c550,Arg0},{0x40be63,0xe8,0x422820,Arg0},
        {0x40bea3,0xe8,0x40c150,None},{0x40beb7,0xe8,0x40c8d0,None},{0x40bec6,0xe8,0x40c4a0,None},
        {0x40beef,0xe9,0x40cbc0,Eax},{0x40befb,0xe9,0x422f20,None}};
    for(const auto& s:sites)r046_write_rel32(s.site,s.op,stub(s.id,s.src));
    // map_r055 redirects 440CA0's call for its own oracle: restore the original 43FB40 target.
    r046_write_rel32(0x440cb3u,0xe8,0x43fb40u);
    constexpr unsigned CallbackCount=6;std::array<std::uint32_t,CallbackCount> display_cb{},layer_cb{};
    for(unsigned k=0;k<CallbackCount;++k){display_cb[k]=next_stub;stub(next_stub,Arg0);}
    for(unsigned k=0;k<CallbackCount;++k){layer_cb[k]=next_stub;stub(next_stub,None);}
    // ---- native logging device ----
    struct NativeDevice final:PcD3D9Device{
        std::vector<std::vector<std::uint32_t>> calls;std::array<std::uint32_t,0x400> render{};
        std::vector<std::vector<std::uint32_t>>* leaves{};
        [[noreturn]] static void unused(){throw std::runtime_error("unexpected device call in the frame renderer");}
        void log(std::vector<std::uint32_t> c){calls.push_back(std::move(c));}
        std::uint32_t create_vertex_buffer(std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t)override{unused();}
        std::uint32_t create_index_buffer(std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t)override{unused();}
        std::uint8_t* lock(std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t)override{unused();}
        void unlock(std::uint32_t)override{unused();}
        std::uint32_t create_vertex_declaration(const PcVertexElement*)override{unused();}
        std::uint32_t create_vertex_shader(const std::uint32_t*)override{unused();}
        void set_vertex_shader_constant_f(std::uint32_t start,const float* d,std::uint32_t n)override{
            std::vector<std::uint32_t> c{94,start,n};for(std::uint32_t k=0;k<n*4;++k){std::uint32_t u;std::memcpy(&u,d+k,4);c.push_back(u);}log(c);}
        std::uint32_t create_texture_from_file(const PcTextureFileRequest&)override{unused();}
        void release(std::uint32_t)override{log({2});}
        std::uint32_t get_render_state(std::uint32_t s)override{log({58,s});return render[s&0x3ffu];}
        void set_render_state(std::uint32_t s,std::uint32_t v)override{log({57,s,v});render[s&0x3ffu]=v;}
        std::uint32_t get_texture(std::uint32_t)override{unused();}
        void set_texture(std::uint32_t,std::uint32_t)override{unused();}
        std::uint32_t get_texture_stage_state(std::uint32_t,std::uint32_t)override{unused();}
        void set_texture_stage_state(std::uint32_t,std::uint32_t,std::uint32_t)override{unused();}
        std::uint32_t get_sampler_state(std::uint32_t,std::uint32_t)override{unused();}
        void set_sampler_state(std::uint32_t,std::uint32_t,std::uint32_t)override{unused();}
        std::uint32_t draw_indexed_primitive(std::uint32_t,std::int32_t,std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t)override{unused();}
        void set_vertex_declaration(std::uint32_t)override{unused();}
        void set_vertex_shader(std::uint32_t)override{unused();}
        void set_stream_source(std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t)override{unused();}
        void set_indices(std::uint32_t)override{unused();}
        std::uint32_t create_pixel_shader(const std::uint32_t*)override{unused();}
        void set_pixel_shader(std::uint32_t)override{unused();}
        void set_pixel_shader_constant_f(std::uint32_t,const float*,std::uint32_t)override{unused();}
        std::uint32_t get_declaration(std::uint32_t,PcVertexElement*)override{unused();}
        void set_light(std::uint32_t i,const std::uint32_t* l)override{std::vector<std::uint32_t> c{51,i};c.insert(c.end(),l,l+26);log(c);}
        void light_enable(std::uint32_t i,std::uint32_t e)override{log({53,i,e});}
        std::uint32_t get_render_target(std::uint32_t i)override{log({38,i});return fake_d3d9::BackBuffer;}
        void set_render_target(std::uint32_t i,std::uint32_t s)override{log({37,i,s});}
        std::uint32_t get_depth_stencil_surface()override{log({40});return fake_d3d9::DepthSurface;}
        void set_depth_stencil_surface(std::uint32_t s)override{log({39,s});}
        void clear(std::uint32_t f,std::uint32_t c,float z,std::uint32_t st)override{std::uint32_t u;std::memcpy(&u,&z,4);log({43,0,0,f,c,u,st});}
        void get_viewport(std::uint32_t out[6])override{log({48});PcD3D9Device::get_viewport(out);}
        void set_viewport(const std::uint32_t v[6])override{log({47,v[0],v[1],v[2],v[3],v[4],v[5]});PcD3D9Device::set_viewport(v);}
        void seed_viewport(const std::uint32_t v[6]){PcD3D9Device::set_viewport(v);}
    } native;
    PcRenderGlobals rg;
    for(auto& r:rg.ranges)if(r.base!=0x740c98u)for(std::uint32_t k=0;k<r.size;k+=4)r.words[k/4]=g32(r.base+k);
    PcFlushContext fc{native,rg,[](std::uint32_t)->PcPmtResources*{return nullptr;},pc_colour_list_alt_408c80,1};
    auto guest_calls=[&]{
        std::vector<std::vector<std::uint32_t>> out;std::uint32_t data=0;const auto* df=fake_d3d9::data_log();
        auto words=[&](std::vector<std::uint32_t>& c,unsigned n){for(unsigned w=0;w<n;++w){std::uint32_t u;std::memcpy(&u,df+data+w,4);c.push_back(u);}data+=n;};
        const auto n=fake_d3d9::log_count();
        for(std::uint32_t k=0;k<n;++k){
            const auto* r=fake_d3d9::log_record(k);const auto id=r[0];const auto* a=r+2;
            switch(id){
            case 57:out.push_back({57,a[0],a[1]});break;
            case 58:out.push_back({58,a[0]});break;
            case 53:out.push_back({53,a[0],a[1]});break;
            case 51:{std::vector<std::uint32_t> c{51,a[0]};words(c,26);out.push_back(c);break;}
            case 94:{std::vector<std::uint32_t> c{94,a[0],a[2]};words(c,a[2]*4u);if(a[0]==0x30u||a[0]==0x31u)c[6]=0;out.push_back(c);break;}
            case 38:out.push_back({38,a[0]});break;
            case 37:out.push_back({37,a[0],a[1]});break;
            case 40:out.push_back({40});break;
            case 39:out.push_back({39,a[0]});break;
            case 43:out.push_back({43,a[0],a[1],a[2],a[3],a[4],a[5]});break;
            case 48:out.push_back({48});break;
            case 47:{std::vector<std::uint32_t> c{47};words(c,6);out.push_back(c);break;}
            case 0x4002:out.push_back({2});break;
            default:out.push_back({0xdead0000u|id});break;
            }
        }
        return out;
    };
    std::mt19937 rng(0x449050u);
    auto real=[&](float lo,float hi){return lo+(hi-lo)*(float(rng()&0xffffu)/65535.f);};
    auto fbits=[](float f){std::uint32_t u;std::memcpy(&u,&f,4);return u;};
    const unsigned cases=std::max(count,1u)*1500u;
    unsigned mismatches=0,device_calls=0,leaf_calls=0,displays=0,clears=0,target_switches=0;
    auto report=[&](const std::string& what){++mismatches;if(mismatches<=12)std::cerr<<"MISMATCH "<<what<<char(10);};
    std::vector<std::uint8_t> lights(0x960),fog(0x54);
    PcRenderQueue alpha_queue;{PcRenderQueue o;render_queues_reset_405160(o,alpha_queue);}
    for(unsigned c=0;c<cases;++c){
        const std::string tag=" case "+std::to_string(c);
        // Events: display masks over the layers, display/shadow callbacks.
        PcEventControlState st{};
        for(unsigned i=0;i<outrun::driving::PcEventSlotCount;++i){
            auto& s=st.slots[i];
            if(rng()%6u){continue;}
            static const std::uint8_t fl[]{0,1,2,2,2,3,6,0x0a,0x12};
            s.flags=fl[rng()%sizeof(fl)];s.event_id=i;s.work_token=works+(rng()%64u)*0x1000u;
            s.display_scene=(rng()%4u)?(1u<<(rng()%9u)):rng();
            s.disp_callback=(rng()%5u)?display_cb[rng()%CallbackCount]:0u;s.shadow_callback=(rng()%3u)?display_cb[rng()%CallbackCount]:0u;
        }
        const std::uint32_t course_work=works+0x3f000u;st.slots[387].work_token=course_work;
        // Events 386 (environment), 387 (course) and 391: their flag bytes are
        // 79FCCA / 79FCCB / 79FCCF.
        for(unsigned i:{386u,387u,391u}){static const std::uint8_t fl[]{2,2,2,1,3,0,6,0x12};st.slots[i].flags=fl[rng()%8u];}
        if(rng()%2u){st.slots[8].flags=2;st.slots[8].work_token=works+0x3e000u;}
        for(unsigned w=0;w<64;++w)p32(works+w*0x1000u+0xd10u,rng()%7u);
        const std::uint32_t course_flags=(rng()%4u)?(rng()&0xfu):rng();p32(course_work,course_flags);
        for(unsigned i=0;i<outrun::driving::PcEventSlotCount;++i){
            const auto& s=st.slots[i];const unsigned r=0x799b30u+i*0x3cu;
            const std::uint32_t w[15]{s.descriptor_token,s.event_id,s.work_token,s.display_scene,s.init_callback,s.ctrl_callback,
                s.disp_callback,s.shadow_callback,s.dest_callback,s.aux24,s.aux28,s.close_guard,s.function_id,s.aux34,s.aux38};
            for(unsigned k=0;k<15;++k)p32(r+k*4,w[k]);
            *reinterpret_cast<std::uint8_t*>(0x79fb48u+i)=s.flags;
        }
        // Scene display globals.
        PcSceneDisplayGlobals sg{};
        static const std::uint32_t modes[]{2,2,0,1,3};sg.mode_780258=modes[rng()%5u];
        static const std::uint32_t scenes[]{2,6,0x14,0,7};sg.scene_7f2428=scenes[rng()%5u];
        sg.paused_780248=(rng()%4u)==0u;sg.object_7f9460_60=(rng()%2u)?rng():0u;sg.shadow_79f5ec=std::uint8_t(course_flags);
        sg.phase_7a0db4=real(0,20);sg.phase_7a0db8=real(0,20);for(auto& w:sg.saved_7a0dbc)w=rng();
        p32(0x780258,sg.mode_780258);p32(0x7f2428,sg.scene_7f2428);*reinterpret_cast<std::uint8_t*>(0x780248)=sg.paused_780248;
        p32(0x7f9460+0x60,sg.object_7f9460_60);p32(0x7a0db4,fbits(sg.phase_7a0db4));p32(0x7a0db8,fbits(sg.phase_7a0db8));
        for(unsigned k=0;k<12;++k)p32(0x7a0dbc+k*4,sg.saved_7a0dbc[k]);
        p32(0x7a01d0,0x799b30u);st.current_slot=0;
        // Environments.
        for(unsigned r=0;r<15;++r){Bytes l(lights.data()+r*0xa0,0xa0);
            for(unsigned k=0;k<0xa0;k+=4)l.putf(k,real(-1.f,1.f));
            l.put32(0,rng()%4u!=0u);l.put32(4,(r%3u)==0u&&r<3?3u:1u+rng()%3u);
            for(unsigned k=0x08;k<0x38;k+=4)l.putf(k,real(0.f,1.2f));
            l.putf(0x50,real(1.f,500.f));l.putf(0x64,real(0.2f,1.5f));l.putf(0x68,real(0.1f,1.2f));l.putf(0x90,real(0.5f,2.5f));}
        for(unsigned r=0;r<3;++r){Bytes f(fog.data()+r*0x1c,0x1c);for(unsigned k=0;k<0x1c;k+=4)f.put32(k,rng());
            f.put32(0,1);f.put32(4,rng()%4u);f.putf(8,real(0,100));f.putf(0xc,real(200,3000));f.putf(0x10,real(0.0005f,0.02f));}
        std::memcpy(reinterpret_cast<void*>(0x899b98u),lights.data(),lights.size());
        std::memcpy(reinterpret_cast<void*>(0x7d3a10u),fog.data(),fog.size());
        PcFrameInputs in{};
        static const std::uint32_t game_modes[]{0,0x18,0x20,4,0x18};in.game_mode_78026c=game_modes[rng()%5u];
        in.screen_740c94=real(0.5f,2.f);in.screen_740c98=real(0.5f,2.f);
        in.glare_95af09=std::uint8_t(rng()%2u);
        in.mode_84a318=(rng()%2u)?2u:rng()%4u;in.course_work_79f5ec=course_work;in.course_flags=course_flags;
        for(auto& v:in.matrix_7d2da0)v=real(-1.f,1.f);
        in.matrix_7d2da0[0]+=2.f;in.matrix_7d2da0[5]+=2.f;in.matrix_7d2da0[10]+=2.f;in.matrix_7d2da0[15]=1.f;
        const std::uint8_t pixel=std::uint8_t(rng()%2u);
        p32(0x78026c,in.game_mode_78026c);p32(0x740c94,fbits(in.screen_740c94));p32(0x740c98,fbits(in.screen_740c98));
        *reinterpret_cast<std::uint8_t*>(0x95af09)=in.glare_95af09;rg.put_byte(0x95af09,in.glare_95af09);
        *reinterpret_cast<std::uint8_t*>(0x740c88)=pixel;p32(0x84a318,in.mode_84a318);
        for(unsigned k=0;k<16;++k)p32(0x7d2da0+k*4,fbits(in.matrix_7d2da0[k]));
        // Frame targets.
        PcFrameState fs{};
        static const std::uint32_t surfaces[]{0,0,0,fake_d3d9::BackBuffer,fake_d3d9::DepthSurface,fake_d3d9::SurfaceA,fake_d3d9::SurfaceB};
        for(unsigned l=0;l<9;++l){
            fs.layers[l].colour=(rng()%3u)?0u:surfaces[rng()%7u];fs.layers[l].depth=(rng()%3u)?0u:surfaces[rng()%7u];
            fs.layers[l].callback=(rng()%4u)?0u:layer_cb[rng()%CallbackCount];
            p32(0x7d2618+l*12,fs.layers[l].colour);p32(0x7d261c+l*12,fs.layers[l].depth);p32(0x7d2620+l*12,fs.layers[l].callback);
        }
        fs.device_ready_7d2614=rng()%2u;p32(0x7d2614,fs.device_ready_7d2614);
        fs.clear_colour_89bd5c=rng();p32(0x89bd5c,fs.clear_colour_89bd5c);
        for(std::uint32_t a:{0x7d25e8u,0x7d25ecu,0x7d25f0u,0x7d25f4u,0x7d2610u,0x73e2a4u,0x73e2a8u})p32(a,0x77u);
        fs.current_colour_7d25e8=fs.backbuffer_7d25ec=fs.layer_7d25f0=fs.depth_7d25f4=fs.current_depth_7d2610=fs.course_73e2a4=fs.course_73e2a8=0x77u;
        {const std::uint32_t v[6]{rng()%64u,rng()%64u,640,480,0,0x3f800000u};std::memcpy(reinterpret_cast<void*>(fake_d3d9::ViewportState),v,24);native.seed_viewport(v);}
        // Render states the environment touches: random on both sides.
        for(std::uint32_t s:{0x1cu,0x22u,0x23u,0x24u,0x25u,0x26u}){const std::uint32_t v=rng()%4u;p32(fake_d3d9::RenderStates+s*4,v);native.render[s]=v;}
        // Empty render queues (43FB40's 405830 alpha flushes).
        prepare(0x405160);run_original32();{PcRenderQueue o;render_queues_reset_405160(o,alpha_queue);}
        // Run.
        fake_d3d9::reset_log();native.calls.clear();p32(LeafLog,0);
        prepare(0x449050);run_original32();
        std::vector<std::vector<std::uint32_t>> nleaf;
        PcSceneDisplayServices display{&nleaf,
            [](void* u,std::uint32_t cb,std::uint32_t work,std::uint32_t){static_cast<std::vector<std::vector<std::uint32_t>>*>(u)->push_back({cb,work});},
            [](void*,std::uint32_t work){return *reinterpret_cast<const std::uint32_t*>(work+0xd10u);},nullptr,nullptr,nullptr};
        struct Owner{PcFlushContext* fc;PcRenderQueue* alpha;std::vector<std::vector<std::uint32_t>>* leaves;} owner{&fc,&alpha_queue,&nleaf};
        display.user=&owner;
        display.display=[](void* u,std::uint32_t cb,std::uint32_t work,std::uint32_t){static_cast<Owner*>(u)->leaves->push_back({cb,work});};
        display.flush_alpha=[](void* u){auto& o=*static_cast<Owner*>(u);
            std::vector<std::uint32_t> order(o.alpha->count);for(std::uint32_t k=0;k<o.alpha->count;++k)order[k]=k;
            if(o.alpha->count)render_queue_sort_4499e0(*o.alpha,order,0,std::int32_t(o.alpha->count)-1);
            render_queue_flush_405890(*o.fc,*o.alpha,order);o.alpha->count=0;o.alpha->matrix_count=0;};
        display.lights_reset=[](void* u){render_lights_reset_410740(*static_cast<Owner*>(u)->fc);};
        display.light_add=[](void* u,std::uint8_t* l){render_light_add_4107a0(*static_cast<Owner*>(u)->fc,Bytes(l,0x94));};
        PcFrameServices sv{[&](std::uint32_t pc,std::uint32_t arg){nleaf.push_back({pc,arg});},st,sg,lights.data()+0x140,display,
            PcEnvironmentRenderTables{Bytes(lights.data(),lights.size()),Bytes(fog.data(),fog.size()),pixel}};
        frame_render_449050(fc,fs,in,sv);
        if(inject&&c==9u)rg.w(0x897d3c)^=1u;
        // Compare device calls.
        auto gl=guest_calls();const auto& nl=native.calls;device_calls+=unsigned(gl.size());
        for(const auto& x:nl){if(x[0]==43)++clears;if(x[0]==37)++target_switches;}
        if(gl.size()!=nl.size())report("device call count guest "+std::to_string(gl.size())+" native "+std::to_string(nl.size())+tag);
        for(std::size_t k=0;k<std::min(gl.size(),nl.size());++k)if(gl[k]!=nl[k]){
            std::ostringstream o;o<<"device call "<<k<<" guest";for(auto v:gl[k])o<<" "<<std::hex<<v;o<<" | native";for(auto v:nl[k])o<<" "<<std::hex<<v;
            report(o.str()+tag);break;}
        // Leaves.
        const unsigned ln=g32(LeafLog);if(ln>=LeafCap)throw std::runtime_error("frame leaf log overflow");
        std::vector<std::vector<std::uint32_t>> gleaf;for(unsigned k=0;k<ln;++k)gleaf.push_back({g32(LeafLog+0x10+k*8),g32(LeafLog+0x14+k*8)});
        leaf_calls+=ln;for(const auto& x:gleaf)for(auto cb:display_cb)if(x[0]==cb)++displays;
        if(gleaf!=nleaf){std::ostringstream o;o<<"leaf calls guest "<<gleaf.size()<<" native "<<nleaf.size();
            for(std::size_t k=0;k<std::min(gleaf.size(),nleaf.size());++k)if(gleaf[k]!=nleaf[k]){o<<" first diff "<<k<<" guest "<<std::hex<<gleaf[k][0]<<"/"<<gleaf[k][1]<<" native "<<nleaf[k][0]<<"/"<<nleaf[k][1];break;}
            report(o.str()+tag);
            if(std::getenv("OR2_FRAME_TRACE")&&c==unsigned(std::atoi(std::getenv("OR2_FRAME_TRACE")))){for(auto& x:gleaf)std::cerr<<" G "<<std::hex<<x[0]<<"/"<<x[1];std::cerr<<char(10);for(auto& x:nleaf)std::cerr<<" N "<<std::hex<<x[0]<<"/"<<x[1];std::cerr<<std::dec<<char(10);
                std::cerr<<"mode "<<sg.mode_780258<<" scene "<<sg.scene_7f2428<<" obj "<<sg.object_7f9460_60<<" slot387 flags "<<unsigned(st.slots[387].flags)<<" disp "<<std::hex<<st.slots[387].display_scene<<" cb "<<st.slots[387].disp_callback<<" shadow "<<st.slots[387].shadow_callback<<" course "<<course_flags<<std::dec<<char(10);}}
        // Globals and frame state.
        bool ok=true;
        for(auto& r:rg.ranges){if(r.base==0x740c98u)continue;
            for(std::uint32_t k=0;k<r.size&&ok;k+=4)if(g32(r.base+k)!=r.words[k/4]){
                std::ostringstream o;o<<"global 0x"<<std::hex<<r.base+k<<" guest "<<g32(r.base+k)<<" native "<<r.words[k/4]<<tag;report(o.str());ok=false;}}
        if(!ok)for(auto& r:rg.ranges)if(r.base!=0x740c98u)for(std::uint32_t k=0;k<r.size;k+=4)r.words[k/4]=g32(r.base+k);
        const std::pair<std::uint32_t,std::uint32_t> state[]{{0x7d25e8,fs.current_colour_7d25e8},{0x7d25ec,fs.backbuffer_7d25ec},{0x7d25f0,fs.layer_7d25f0},
            {0x7d25f4,fs.depth_7d25f4},{0x7d2610,fs.current_depth_7d2610},{0x73e2a4,fs.course_73e2a4},{0x73e2a8,fs.course_73e2a8}};
        for(const auto& [a,v]:state)if(g32(a)!=v){std::ostringstream o;o<<"frame state 0x"<<std::hex<<a<<" guest "<<g32(a)<<" native "<<v<<tag;report(o.str());break;}
        for(unsigned k=0;k<6;++k)if(g32(0x7d25f8+k*4)!=fs.viewport_7d25f8[k]){report("viewport 7D25F8"+tag);break;}
        if(std::memcmp(reinterpret_cast<const void*>(0x899b98u),lights.data(),lights.size()))report("light table"+tag);
        if(g32(0x7a0db4)!=fbits(sg.phase_7a0db4)||g32(0x7a0db8)!=fbits(sg.phase_7a0db8))report("scene phases"+tag);
        if(g32(0x7a01d0)!=0x799b30u+st.current_slot*0x3cu)report("current event slot"+tag);
    }
    std::cerr<<"frame render: device calls="<<device_calls<<" leaf calls="<<leaf_calls<<" display callbacks="<<displays
             <<" clears="<<clears<<" target switches="<<target_switches<<char(10);
    std::cout<<"{\"routine\":\"frame_render_probe\",\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}"<<'\n';return 0;
}

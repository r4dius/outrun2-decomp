if(only=="scene_environment_probe"){
    // Original SCN_ENV event functions: init 449FC0 (4518E0, 407940 with
    // 44A430 and the LIGHTING render state, 449F80, flags/phase) and control
    // 44A890 (mode table 44A910, 49EED0/49B2D0 gates, 4518C0, 408310) against
    // pc_scene_environment. The 44A8DF update calls (4517D0/449F50/44A000,
    // ported and verified separately) and the two protected queries are
    // stubs; everything else runs unchanged.
    using namespace outrun::platform;
    const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
    if(argc<=7||std::string(argv[7])=="-")throw std::runtime_error("scene environment oracle requires pinned d3dx9_29.dll");
    constexpr unsigned base=0x3a800000,Stubs=base+0x100,Log=base+0x1000,Cells=base+0x20;
    map_at(base,0x10000,PROT_READ|PROT_WRITE|PROT_EXEC);
    map_original_arithmetic_bridge(file);
    init_matrix_oracle_globals();wire_d3dx_imports(map_d3dx29(argv[7]));
    for(std::uint32_t page=0x62f000u;page<0x98d000u;page+=0x1000u)mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE);
    fake_d3d9::install();
    auto g32=[](std::uint32_t a){return *reinterpret_cast<const std::uint32_t*>(a);};
    auto p32=[](std::uint32_t a,std::uint32_t v){*reinterpret_cast<std::uint32_t*>(a)=v;};
    // Query stubs: mov eax,[cell]; ret. Update stubs: append id to the log.
    auto emit=[&](unsigned at,std::vector<std::uint8_t> b){std::memcpy(reinterpret_cast<void*>(at),b.data(),b.size());return at;};
    auto le=[](std::vector<std::uint8_t>& b,std::uint32_t v){for(unsigned k=0;k<4;++k)b.push_back(std::uint8_t(v>>(k*8)));};
    std::vector<std::uint8_t> q1{0xa1};le(q1,Cells);q1.push_back(0xc3);
    std::vector<std::uint8_t> q2{0xa1};le(q2,Cells+4);q2.push_back(0xc3);
    const auto bridge=emit(Stubs,q1),timer=emit(Stubs+0x10,q2);
    auto logger=[&](unsigned at,std::uint32_t id){
        std::vector<std::uint8_t> b{0xa1};le(b,Log);                     // mov eax,[Log]
        b.insert(b.end(),{0xc7,0x04,0x85});le(b,Log+4);le(b,id);         // mov [eax*4+Log+4],id
        b.insert(b.end(),{0x40,0xa3});le(b,Log);b.push_back(0xc3);       // inc eax; mov [Log],eax; ret
        return emit(at,b);};
    r046_write_rel32(0x44a8ab,0xe8,bridge);r046_write_rel32(0x44a8c4,0xe8,timer);r046_write_rel32(0x44a8d4,0xe8,timer);
    r046_write_rel32(0x44a8df,0xe8,logger(Stubs+0x40,0x4517d0));
    r046_write_rel32(0x44a8e4,0xe8,logger(Stubs+0x80,0x449f50));
    r046_write_rel32(0x44a8e9,0xe8,logger(Stubs+0xc0,0x44a000));
    std::mt19937 rng(0x449fc0u);
    auto real=[&](float lo,float hi){return lo+(hi-lo)*(float(rng()&0xffffu)/65535.f);};
    const unsigned cases=std::max(count,1u)*3000u;
    unsigned mismatches=0,inits=0,updates=0,fog_offs=0;
    auto report=[&](const std::string& what){++mismatches;if(mismatches<=12)std::cerr<<"MISMATCH "<<what<<char(10);};
    struct NativeDevice final:PcD3D9StateDevice{std::vector<std::vector<std::uint32_t>> calls;
        void set_render_state(std::uint32_t s,std::uint32_t v)override{calls.push_back({57,s,v});}
        std::uint32_t create_vertex_buffer(std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t)override{return 0;}
        std::uint32_t create_index_buffer(std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t)override{return 0;}
        std::uint8_t* lock(std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t)override{return nullptr;}
        void unlock(std::uint32_t)override{}
        std::uint32_t create_vertex_declaration(const PcVertexElement*)override{return 0;}
        std::uint32_t create_vertex_shader(const std::uint32_t*)override{return 0;}
        std::uint32_t create_texture_from_file(const PcTextureFileRequest&)override{return 0;}
        void release(std::uint32_t)override{}
        std::uint32_t draw_indexed_primitive(std::uint32_t,std::int32_t,std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t)override{return 0;}
        std::uint32_t create_pixel_shader(const std::uint32_t*)override{return 0;}
        std::uint32_t get_declaration(std::uint32_t,PcVertexElement*)override{return 0;}
    } native;
    for(unsigned c=0;c<cases;++c){
        const std::string tag=" case "+std::to_string(c);
        PcSceneEnvironment env{};
        for(unsigned k=0;k<0x960;k+=4){float f=real(-2,2);std::memcpy(env.lights_899b98.data()+k,&f,4);}
        for(auto& b:env.fog_7d3a10)b=std::uint8_t(rng());
        for(auto& w:env.fog_lists_7d3a00)w=rng();for(auto& w:env.sun_lists_7d26a8)w=rng();
        for(auto& w:env.flags_7d28b0)w=rng()%3u;env.phase_7d28c8=rng()%4u;
        auto store=[&]{
            std::memcpy(reinterpret_cast<void*>(0x899b98u),env.lights_899b98.data(),0x960);
            std::memcpy(reinterpret_cast<void*>(0x7d3a10u),env.fog_7d3a10.data(),0x54);
            for(unsigned k=0;k<3;++k){p32(0x7d3a00+k*4,env.fog_lists_7d3a00[k]);p32(0x7d26a8+k*4,env.sun_lists_7d26a8[k]);}
            for(unsigned k=0;k<6;++k)p32(0x7d28b0+k*4,env.flags_7d28b0[k]);p32(0x7d28c8,env.phase_7d28c8);};
        store();
        reset_matrix_oracle_globals();
        std::vector<std::uint8_t> arena(0x1000);Bytes ar(arena.data(),arena.size());
        for(unsigned k=0;k<16;++k)ar.putf(k*4,(k%5u)==0u?1.f:0.f);
        std::memcpy(reinterpret_cast<void*>(MatrixArena),arena.data(),arena.size());
        PcMatrixStack stack{ar,0,0,64};
        fake_d3d9::reset_log();native.calls.clear();p32(Log,0);
        std::vector<std::uint32_t> nlog;
        const bool init=(rng()%3u)==0u;
        if(init){++inits;prepare(0x449fc0);run_original32();scene_environment_init_449fc0(env,native,stack);}
        else{
            PcSceneEnvironmentControl ctl{};
            ctl.mode_78026c=(rng()%8u)?rng()%0x28u:rng();ctl.race_mode_780258=rng()%6u;
            const std::uint32_t b=(rng()%2u)?1u:rng()%3u;const std::uint16_t t=std::uint16_t((rng()%3u)==0u?0x12cu:(rng()%3u)==0u?0x168u:rng());
            p32(0x78026c,ctl.mode_78026c);p32(0x780258,ctl.race_mode_780258);p32(Cells,b);p32(Cells+4,0xabcd0000u|t);
            ctl.bridge_49eed0=[&]{return b;};ctl.timer_49b2d0=[&]{return t;};
            ctl.update_44a8df=[&]{nlog.push_back(0x4517d0);nlog.push_back(0x449f50);nlog.push_back(0x44a000);++updates;};
            prepare(0x44a890);run_original32();
            scene_environment_control_44a890(env,ctl);
            if(g32(0x7d3a10)==0&&g32(0x7d3a2c)==0&&g32(0x7d3a48)==0)++fog_offs;
        }
        if(inject&&c==5u)env.lights_899b98[0x44]^=1u;
        // Compare.
        std::vector<std::uint32_t> glog;for(unsigned k=0;k<g32(Log);++k)glog.push_back(g32(Log+4+k*4));
        if(glog!=nlog)report("update calls"+tag);
        if(std::memcmp(reinterpret_cast<const void*>(0x899b98u),env.lights_899b98.data(),0x960)){
            for(unsigned k=0;k<0x960;k+=4)if(g32(0x899b98u+k)!=Bytes(env.lights_899b98.data(),0x960).u32(k)){
                std::ostringstream o;o<<"light table +0x"<<std::hex<<k<<" guest "<<g32(0x899b98u+k)<<" native "<<Bytes(env.lights_899b98.data(),0x960).u32(k)<<tag;report(o.str());break;}}
        if(std::memcmp(reinterpret_cast<const void*>(0x7d3a10u),env.fog_7d3a10.data(),0x54))report("fog records"+tag);
        for(unsigned k=0;k<3;++k)if(g32(0x7d3a00+k*4)!=env.fog_lists_7d3a00[k]||g32(0x7d26a8+k*4)!=env.sun_lists_7d26a8[k]){report("lists"+tag);break;}
        for(unsigned k=0;k<6;++k)if(g32(0x7d28b0+k*4)!=env.flags_7d28b0[k]){report("flags"+tag);break;}
        if(g32(0x7d28c8)!=env.phase_7d28c8)report("phase"+tag);
        std::vector<std::vector<std::uint32_t>> gcalls;
        for(std::uint32_t k=0;k<fake_d3d9::log_count();++k){const auto* r=fake_d3d9::log_record(k);
            if(r[0]==57)gcalls.push_back({57,r[2],r[3]});else gcalls.push_back({0xdead0000u|r[0]});}
        if(gcalls!=native.calls)report("device calls"+tag);
    }
    std::cerr<<"scene environment: inits="<<inits<<" updates="<<updates<<" fog-off states="<<fog_offs<<char(10);
    std::cout<<"{\"routine\":\"scene_environment_probe\",\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}"<<'\n';return 0;
}

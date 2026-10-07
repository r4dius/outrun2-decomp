if(only=="vehicle_preview_init_probe"){
    // Complete original 4A7270: 4A5830 queue constructor, 449F50 scene
    // environment (protected 799D18 bridge, 44B020/44AB10, 44A1D0), 4A69F0,
    // 519300, 45A2B0 gateway, 4F6E40, 49A650 and 4A2EE0, all unpatched.
    using namespace outrun::platform;
    if(argc<=7||std::string(argv[7])=="-")throw std::runtime_error("preview init oracle requires pinned d3dx9_29.dll");
    constexpr unsigned base=0x3a800000,camera_at=base+0x3000,stub_at=base+0x7800,lists_at=base+0x8000;
    map_at(base,0x20000,PROT_READ|PROT_WRITE|PROT_EXEC);map_original_arithmetic_bridge(file);
    init_matrix_oracle_globals();wire_d3dx_imports(map_d3dx29(argv[7]));init_spline_oracle();init_world_oracle();
    for(unsigned page:{0x830000u,0x841000u,0x780000u,0x65a000u,0x851000u,0x7de000u,0x83d000u,0x82e000u,0x82f000u,0x899000u,0x89a000u,0x799000u,0x79f000u})
        if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("preview init globals");
    // The 45A2B0 rank provider answers through the stub: FXT, its protection page slot
    // [1039CCC]; Steam (plain 45A2B0, no protection page), a JMP at its entry.
    std::uint32_t saved_rank=0;std::array<std::uint8_t,5> saved_45a2b0{};
    if(g_steam_exe){std::memcpy(saved_45a2b0.data(),reinterpret_cast<void*>(0x45a2b0u),5);r046_write_rel32(0x45a2b0u,0xe9,stub_at);}
    else{
        if(mprotect(reinterpret_cast<void*>(0x01039000u),4096,PROT_READ|PROT_WRITE|PROT_EXEC))throw std::runtime_error("rank gateway page");
        saved_rank=*reinterpret_cast<std::uint32_t*>(0x01039cccu);
        *reinterpret_cast<std::uint32_t*>(0x01039cccu)=stub_at;
    }
    DrivingDataPack pack;
    auto* arena=reinterpret_cast<std::uint8_t*>(0x5e3140);
    pack.parameter_arena.assign(arena,arena+DrivingParameterArenaBytes);
    std::memcpy(pack.selection_maps.data(),reinterpret_cast<void*>(0x5e3050),240);
    unsigned cases{},mismatches{};const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
    auto check=[&](bool ok,const char* what){++cases;if(!ok){++mismatches;if(mismatches<12)std::cerr<<"preview init "<<cases<<" "<<what<<"\n";}};
    auto compare=[&](const auto& native,unsigned address,const char* what){
        const bool same=!std::memcmp(native.data(),reinterpret_cast<void*>(address),native.size());
        if(!same&&mismatches<6)for(unsigned o=0;o<native.size();++o)if(native[o]!=*reinterpret_cast<std::uint8_t*>(address+o)){
            unsigned nw=0;std::memcpy(&nw,native.data()+(o&~3u),4);
            std::cerr<<what<<" first byte "<<std::hex<<o<<" original word="<<*reinterpret_cast<unsigned*>(address+(o&~3u))<<" native word="<<nw<<std::dec<<"\n";break;}
        check(same,what);
    };
    struct RankState{std::uint8_t value;};
    auto rank_callback=[](void* user,std::uint8_t)->std::uint32_t{return 0xabcd0000u|static_cast<RankState*>(user)->value;};
    unsigned run=0;
    for(unsigned model=0;model<30;++model)for(unsigned setup=0;setup<12;++setup){
        ++run;
        const std::uint8_t scene=std::uint8_t(setup%4u==3u?255u:setup%4u);
        const std::uint8_t transmission=std::uint8_t(setup%3u);
        const unsigned phase=setup&1u?3u:0u;
        RankState rank{std::uint8_t((model*7u+setup)%12u)};
        const std::uint8_t stub[]{0xb8,rank.value,0x00,0xcd,0xab,0xc3}; // mov eax,0xabcd00rr; ret
        std::memcpy(reinterpret_cast<void*>(stub_at),stub,sizeof(stub));
        outrun::testing::CourseWorldFixture world;
        outrun::testing::configure_world_boundary(world,setup%8u,setup%4u);
        auto wb=world.bytes();wb.putf(0x4638,(int(setup)-6)*0.21f);wb.putf(0x463c,(int(model%5)-2)*0.33f);
        world.rebuild_grids();WorldBinding binding(world);
        auto matrices=world.matrix();auto prediction=world.prediction();auto tables=world.tables();
        CourseWorldQuery query{tables,matrices,prediction};
        VehicleParameterSelection selection{std::uint8_t(setup%3u),scene,4,std::uint8_t(setup&1u),0};
        *reinterpret_cast<std::uint8_t*>(0x83036d)=selection.variant_83036d;
        *reinterpret_cast<std::uint8_t*>(0x7de418)=selection.loading_scene_7de418;
        *reinterpret_cast<unsigned*>(0x780258)=selection.game_mode_780258;
        *reinterpret_cast<std::uint8_t*>(0x65a7ac)=selection.flag_65a7ac;
        *reinterpret_cast<unsigned*>(0x8514a0)=selection.flag_8514a0;
        *reinterpret_cast<std::uint8_t*>(0x830374)=transmission;
        const std::uint16_t yaw=std::uint16_t(model*2311u+setup*977u);*reinterpret_cast<std::uint16_t*>(0x841fa0)=yaw;
        VehicleCreationQueue queue;vehicle_creation_reset_49fa60(queue);
        vehicle_creation_append_49fa80(queue,8,setup%8u,model,1u+setup%8u,0x4081u|((setup&2u)<<5));
        std::memcpy(reinterpret_cast<void*>(0x841ac0),queue.object.data(),queue.object.size());
        std::array<std::uint8_t,76> shared;shared.fill(std::uint8_t(0xc0+setup));
        std::memcpy(reinterpret_cast<void*>(0x83db30),shared.data(),shared.size());
        std::array<std::uint8_t,PcVehicleObjectBytes+16> event;event.fill(std::uint8_t(0x5a^model));
        std::array<std::uint8_t,PcVehicleBodyBytes> body;body.fill(std::uint8_t(0x3c+setup));
        std::memcpy(reinterpret_cast<void*>(base),event.data(),event.size());
        std::memcpy(reinterpret_cast<void*>(0x82e7f0),body.data(),body.size());
        // Environment: one fixed and one run-interpolated record per list.
        std::array<std::uint8_t,6*3*0xb0> lists{};Bytes ls(lists.data(),lists.size());
        for(unsigned list=0;list<6;++list){
            auto l=ls.sub(list*3*0xb0,3*0xb0);
            for(unsigned k=0;k<2*0xb0;k+=4)l.putf(k,(int((k/4+list*11+model)%23)-11)*0.17f);
            l.put16(0,0);l.put16(0x10,std::uint16_t(setup%2u));
            l.put16(0xb0,1);l.put16(0xb0+0x10,4);l.put32(0xb0+8,0x7f7fffffu);
            l.put16(0x160,0xffff);
            if(list<3)for(unsigned k=0;k<3;++k){l.put8(0x48+k,std::uint8_t(model*9+k*40));l.put8(0x64+k,std::uint8_t(setup*21+k));
                l.put8(0xb0+0x48+k,std::uint8_t(k*70));l.put8(0xb0+0x64+k,std::uint8_t(255-k*30));}
        }
        std::memcpy(reinterpret_cast<void*>(lists_at),lists.data(),lists.size());
        std::array<std::uint8_t,0x400> environment;for(unsigned i=0;i<environment.size();++i)environment[i]=std::uint8_t(i*11+run);
        Bytes env(environment.data(),environment.size());
        PcEnvironmentFrame frame{};
        for(unsigned slot=0;slot<3;++slot){
            const bool present=((model+slot)%4u)!=0u;
            env.put32(0xa8+slot*4,present?lists_at+(3+slot)*3*0xb0:0);
            *reinterpret_cast<unsigned*>(0x7d3a00+slot*4)=present?lists_at+slot*3*0xb0:0;
            frame.fog_lists[slot]=present?ls.sub(slot*3*0xb0,3*0xb0):Bytes(nullptr,0);
            frame.sun_lists[slot]=present?ls.sub((3+slot)*3*0xb0,3*0xb0):Bytes(nullptr,0);
        }
        env.put32(0xb4,0);frame.light_list=Bytes(nullptr,0);
        for(unsigned k=0;k<6;++k)env.put32(0x2b0+k*4,(setup>>k)&1u);
        env.put32(0x2c8,phase);env.put16(0x334,std::uint16_t(setup));env.putf(0x2d8,40.f);
        std::memcpy(reinterpret_cast<void*>(0x7d2600),environment.data(),environment.size());
        std::array<std::uint8_t,9*0xa0> lights;for(unsigned i=0;i<lights.size();++i)lights[i]=std::uint8_t(i*3+model);
        std::array<std::uint8_t,0x54> fog;for(unsigned i=0;i<fog.size();++i)fog[i]=std::uint8_t(i*7+setup);
        std::array<std::uint8_t,24> nearest{};std::array<std::uint8_t,0x110> camera{};
        Bytes lv(lights.data(),lights.size()),fv(fog.data(),fog.size()),cam(camera.data(),camera.size());
        for(unsigned k=0;k<lights.size()/4;++k)lv.putf(k*4,(int(k%13)-6)*0.25f);
        for(unsigned k=0;k<3;++k){cam.putf(0xf8+k*4,float(k)*3.f-2.f);cam.putf(0x104+k*4,float(k)+5.f);}
        std::memcpy(reinterpret_cast<void*>(0x899b98),lights.data(),lights.size());
        std::memcpy(reinterpret_cast<void*>(0x7d3a10),fog.data(),fog.size());
        std::memcpy(reinterpret_cast<void*>(0x7d2d58),nearest.data(),nearest.size());
        std::memcpy(reinterpret_cast<void*>(camera_at),camera.data(),camera.size());
        *reinterpret_cast<unsigned*>(0x79f574)=camera_at;*reinterpret_cast<unsigned*>(0x799d18)=base;
        prepare(0x4a7270);*reinterpret_cast<unsigned*>(S)=base;run_original32();

        Bytes e(event.data(),event.size());
        std::array<std::uint8_t,DrivingParameterViewBytes> parameters{};
        frame.vehicle=e;frame.camera=cam;frame.flags_7d28b0=env.sub(0x2b0,24);
        frame.nearest_7d2d58=Bytes(nearest.data(),nearest.size());frame.lights_899d78=lv.sub(3*0xa0,6*0xa0);
        PcEnvironmentBlendContext context{phase,env.i16(0x334),env.f32(0x2d8),env.sub(0xd0,0x1e0),env.sub(0x2e0,0x54),
            lv.sub(0,3*0xa0),fv,tables.transforms[0],matrices};
        PcRankProviderServices rank_services{&rank,rank_callback};
        VehiclePreviewInit init{queue,pack,selection,Bytes(shared.data(),shared.size()),Bytes(parameters.data(),parameters.size()),
            frame,tables.courses[0],context,Bytes(body.data(),body.size()),query,{wb.f32(0x4638),wb.f32(0x463c)},
            transmission,&rank_services,yaw};
        bool ok=false;
        {
            WorldNativeControl control(static_cast<unsigned short>(x87_control));
            try{ok=vehicle_preview_init_4a7270(e,init);}
            catch(const std::exception& ex){if(mismatches<12)std::cerr<<"native 4A7270 rejected model="<<model<<" setup="<<setup<<": "<<ex.what()<<"\n";}
        }
        check(ok,"native 4A7270 completes");
        env.put32(0x2c8,context.phase_7d28c8);
        if(inject&&model==29&&setup==11)event[0xb68]^=1;
        compare(event,base,"whole preview event and canary");
        compare(body,0x82e7f0,"shared body work 82E7F0");
        compare(queue.object,0x841ac0,"creation queue");
        compare(shared,0x83db30,"shared history 83DB30");
        compare(environment,0x7d2600,"environment globals");compare(lists,lists_at,"environment records");
        compare(lights,0x899b98,"lights");compare(fog,0x7d3a10,"fog");compare(nearest,0x7d2d58,"nearest");
        world.save(matrices,prediction);binding.capture_globals();compare(world.image,WorldArena,"world/stack/prediction");
    }
    if(g_steam_exe)r060_raw_write(0x45a2b0u,saved_45a2b0.data(),5);
    else *reinterpret_cast<std::uint32_t*>(0x01039cccu)=saved_rank;
    std::cout<<"{\"routine\":\"vehicle_preview_init_probe\",\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}\n";return mismatches?1:0;
}

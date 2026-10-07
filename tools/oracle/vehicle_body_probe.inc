if(only=="vehicle_body_probe"){
    using namespace outrun::platform;
    if(argc<=7||std::string(argv[7])=="-")throw std::runtime_error("body oracle requires pinned d3dx9_29.dll");
    constexpr unsigned base=0x3a800000,body=base+0x2000;
    map_at(base,0x10000,PROT_READ|PROT_WRITE|PROT_EXEC);map_original_arithmetic_bridge(file);
    init_matrix_oracle_globals();wire_d3dx_imports(map_d3dx29(argv[7]));init_spline_oracle();init_world_oracle();
    unsigned cases{},mismatches{};const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
    auto check=[&](bool ok,const char* what){++cases;if(!ok){++mismatches;if(mismatches<12)std::cerr<<"body "<<cases<<" "<<what<<"\n";}};
    auto compare=[&](const auto& native,unsigned address,const char* what){
        const bool same=!std::memcmp(native.data(),reinterpret_cast<void*>(address),native.size());
        if(!same&&mismatches<5)for(unsigned o=0;o<native.size();++o)if(native[o]!=*reinterpret_cast<std::uint8_t*>(address+o)){
            unsigned nw=0;std::memcpy(&nw,native.data()+(o&~3u),4);
            std::cerr<<what<<" first byte "<<std::hex<<o<<" original word="<<*reinterpret_cast<unsigned*>(address+(o&~3u))<<" native word="<<nw<<std::dec<<"\n";break;}
        check(same,what);
    };
    for(unsigned column=0;column<19;++column)for(unsigned scenario=0;scenario<16;++scenario){
        outrun::testing::CourseWorldFixture world;
        outrun::testing::configure_world_boundary(world,scenario%8,scenario%4);
        world.rebuild_grids();WorldBinding binding(world);
        auto matrices=world.matrix();auto prediction=world.prediction();auto tables=world.tables();CourseWorldQuery query{tables,matrices,prediction};
        std::array<std::uint8_t,PcVehicleObjectBytes+16> event{};
        std::array<std::uint8_t,0x4010> work{};
        std::array<std::uint8_t,DrivingParameterViewBytes> parameters;
        const unsigned param=0x5e3140+column*4;
        std::memcpy(parameters.data(),reinterpret_cast<void*>(param),parameters.size());
        Bytes e(event.data(),event.size()),w(work.data(),work.size()),p(parameters.data(),parameters.size());
        vehicle_clear_4874f0(e,true);e.put32(0x2b4,param);
        e.putf(0x14,scenario&1?5.f:40.f);e.putf(0x18,23.f);e.putf(0x1c,5.f);
        e.putf(0x1c4,(int(scenario)-3)*0.375f);
        e.put16(0x2c,std::uint16_t(scenario*179));e.put16(0x2e,std::uint16_t(scenario*13001));e.put16(0x30,std::uint16_t(65535-scenario*823));
        work.fill(0xab);std::memcpy(reinterpret_cast<void*>(base),event.data(),event.size());std::memcpy(reinterpret_cast<void*>(body),work.data(),work.size());
        prepare(0x4a69f0);guest_call.eax=body;guest_call.ecx=base;run_original32();
        vehicle_body_init_4a69f0(e,w,p,body,query);
        if(inject&&column==0&&scenario==0)w.put32(0x248,0);
        compare(event,base,"whole event");compare(work,body,"whole body and tail");
        world.save(matrices,prediction);binding.capture_globals();compare(world.image,WorldArena,"world/stack/prediction");
        // Standalone parameter refresh must preserve fields it does not own.
        for(unsigned i=0;i<work.size();++i)work[i]=std::uint8_t(i*7+scenario);
        std::memcpy(reinterpret_cast<void*>(body),work.data(),work.size());
        prepare(0x4a6840);guest_call.esi=body;guest_call.edi=base;run_original32();vehicle_body_parameters_4a6840(w,p);
        compare(work,body,"standalone parameter refresh");
        prepare(0x4a1fc0);guest_call.eax=body;run_original32();vehicle_body_corners_4a1fc0(w);
        compare(work,body,"standalone contact corners");
    }
    for(unsigned scenario=0;scenario<256;++scenario){
        outrun::testing::CourseWorldFixture world;
        outrun::testing::configure_world_boundary(world,scenario%16,scenario%4);
        auto b=world.bytes();b.puti(0x4558,15);
        b.putf(0x4638,(int(scenario%13)-6)*0.37f);b.putf(0x463c,(int(scenario%7)-3)*0.63f);
        for(unsigned type=0;type<4;++type){
            auto c=b.sub(type*4096,4096);
            const float sx=(int(scenario%9)-4)*0.13f,sz=(int(scenario%11)-5)*0.07f;
            for(unsigned v=0;v<4;++v)c.putf(0x404+v*12,c.f32(0x400+v*12)*sx+c.f32(0x408+v*12)*sz);
            c.put16(0x43e,std::uint16_t(scenario*321+type*4321));
        }
        world.rebuild_grids();WorldBinding binding(world);
        auto matrices=world.matrix();auto prediction=world.prediction();auto tables=world.tables();CourseWorldQuery query{tables,matrices,prediction};
        std::array<std::uint8_t,PcVehicleObjectBytes+16> event{};
        std::array<std::uint8_t,PcVehicleBodyBytes+16> work{};
        event.fill(0xab);work.fill(0xcd);
        Bytes e(event.data(),event.size()),w(work.data(),work.size());
        e.put32(0x1c0,0);w.putf(0x40,b.f32(0x4600));w.putf(0x44,float(scenario));w.putf(0x48,b.f32(0x4608));
        std::memcpy(reinterpret_cast<void*>(base),event.data(),event.size());std::memcpy(reinterpret_cast<void*>(body),work.data(),work.size());
        prepare(0x519300);Bytes stack(reinterpret_cast<void*>(S),16);stack.put32(0,base);stack.put32(4,body);run_original32();
        vehicle_ground_init_519300(e,w,query,{b.f32(0x4638),b.f32(0x463c)});
        compare(event,base,"ground whole event");compare(work,body,"ground whole body");
        world.save(matrices,prediction);binding.capture_globals();compare(world.image,WorldArena,"ground world/stack/prediction");
    }
    for(unsigned model=0;model<30;++model)for(unsigned pattern=0;pattern<16;++pattern){
        std::array<std::uint8_t,PcVehicleObjectBytes+16> event{};
        for(unsigned k=0;k<event.size();++k)event[k]=std::uint8_t(k*17+pattern*13);
        Bytes e(event.data(),event.size());e.put8(0x11,std::uint8_t(model));
        std::memcpy(reinterpret_cast<void*>(base),event.data(),event.size());
        prepare(0x4f6e40);*reinterpret_cast<unsigned*>(S)=base;run_original32();
        vehicle_collision_init_4f6e40(e);compare(event,base,"model collision entire event");
    }
    // Complete original preview control and leaves; only the Windows CRT
    // thread-data lookup is replaced, NOT rand, metadata or matrix routines.
    // The legacy single-routine runner installs reverse-car leaf fixtures at
    // startup. Undo both call-site patches for this integrated comparison.
    for(unsigned address:{0x4a291eu,0x4a29eeu})
        r060_raw_write(address,file.data()+pe_file_offset(file,address),5);
    for(unsigned page:{0x680000u,0x83d000u,0x841000u})
        if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("preview globals protection");
    {std::uint8_t code[]{0xb8,0,0,0,0,0xc3};const unsigned thread=base+0x7000;std::memcpy(code+1,&thread,4);r060_raw_write(0x585bec,code,sizeof(code));}
    for(unsigned model=0;model<30;++model)for(unsigned mode=0;mode<8;++mode){
        outrun::testing::CourseWorldFixture world;
        outrun::testing::configure_world_boundary(world,0,0);world.rebuild_grids();WorldBinding binding(world);
        auto matrices=world.matrix();
        std::array<std::uint8_t,PcVehicleObjectBytes+16> event{};
        std::array<std::uint8_t,64> history{};std::array<std::uint8_t,32> owner{};
        Bytes e(event.data(),event.size()),h(history.data(),history.size()),o(owner.data(),owner.size());
        vehicle_clear_4874f0(e,true);e.put8(0x11,model);e.put32(4,model%2?0x4081:0x4080);e.putf(0x30c,0.015f);
        e.putf(0x1cc,-0.003f);e.put16(0xd34,std::uint16_t(model*123));e.put16(0x32,std::uint16_t(model*47));
        for(unsigned i=0;i<30;++i)h.put16(i*2,std::uint16_t(i*173));
        o.put32(0x1c,mode==2?3:mode);std::memcpy(reinterpret_cast<void*>(base+0x7100),owner.data(),owner.size());
        unsigned seed=0x9182317u+model;std::uint16_t yaw=std::uint16_t(65500+model);
        VehiclePreviewControl c{{1.25f,-3.5f,6.75f},yaw,seed,h,matrices,int(mode==1),std::uint8_t(mode&3),o,0.375f,std::uint8_t(mode),mode==6?16:3,int(mode==4),-6789};
        *reinterpret_cast<unsigned*>(base+0x7014)=seed;
        *reinterpret_cast<unsigned*>(0x799ca0)=base+0x7100;*reinterpret_cast<std::uint8_t*>(0x79fb4e)=c.owner_flags_79fb4e;
        *reinterpret_cast<int*>(0x82e7d8)=c.interpolation_override_82e7d8;*reinterpret_cast<float*>(0x634b34)=c.frame_blend_634b34;
        *reinterpret_cast<unsigned*>(0x82e7d4)=c.scene_82e7d4;*reinterpret_cast<int*>(0x78026c)=c.game_mode_78026c;
        *reinterpret_cast<int*>(0x82e7ec)=c.steering_override_82e7ec;*reinterpret_cast<std::int16_t*>(0x82e7cc)=c.steering_82e7cc;
        *reinterpret_cast<std::uint16_t*>(0x841fa0)=yaw;
        std::memcpy(reinterpret_cast<void*>(0x680c04),&c.position_680c04,12);
        std::memcpy(reinterpret_cast<void*>(0x83db30),history.data(),history.size());std::memcpy(reinterpret_cast<void*>(base),event.data(),event.size());
        for(unsigned frame=0;frame<128;++frame){
            // External event age is updated independently on both sides;
            // preview control itself neither increments nor fabricates it.
            e.put32(0x1f4,frame);*reinterpret_cast<unsigned*>(base+0x1f4)=frame;
            prepare(0x4a5b20);*reinterpret_cast<unsigned*>(S)=base;run_original32();
            vehicle_preview_control_4a5b20(e,c);
            compare(event,base,"retained preview event");compare(history,0x83db30,"retained shared history");
            check(yaw==*reinterpret_cast<std::uint16_t*>(0x841fa0),"shared yaw");
            check(seed==*reinterpret_cast<unsigned*>(base+0x7014),"shared CRT seed");
            auto prediction=world.prediction();world.save(matrices,prediction);binding.capture_globals();
            compare(world.image,WorldArena,"preview shared matrices");
        }
    }
    std::cout<<"{\"routine\":\"vehicle_body_probe\",\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}\n";return mismatches?1:0;
}

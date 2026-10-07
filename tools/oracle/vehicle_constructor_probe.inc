if(only=="vehicle_constructor_probe"){
    using namespace outrun::platform;
    constexpr unsigned base=0x3a800000;
    map_at(base,0x20000,PROT_READ|PROT_WRITE|PROT_EXEC);map_original_arithmetic_bridge(file);
    for(unsigned page:{0x830000u,0x841000u,0x780000u,0x65a000u,0x851000u,0x7de000u,0x83d000u})
        if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("vehicle constructor globals");
    DrivingDataPack pack;
    auto* arena=reinterpret_cast<std::uint8_t*>(0x5e3140);
    pack.parameter_arena.assign(arena,arena+DrivingParameterArenaBytes);
    std::memcpy(pack.selection_maps.data(),reinterpret_cast<void*>(0x5e3050),240);
    unsigned cases{},mismatches{};const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
    auto check=[&](bool ok,const char* what){++cases;if(!ok){++mismatches;if(mismatches<12)std::cerr<<"constructor "<<cases<<" "<<what<<"\n";}};
    auto original=[&](unsigned pc,std::initializer_list<unsigned> args){prepare(pc);guest_call.esi=base;
        unsigned i=0;for(unsigned a:args)reinterpret_cast<unsigned*>(S)[i++]=a;run_original32();};
    auto globals=[&](const VehicleParameterSelection& s){
        *reinterpret_cast<std::uint8_t*>(0x83036d)=s.variant_83036d;
        *reinterpret_cast<std::uint8_t*>(0x7de418)=s.loading_scene_7de418;
        *reinterpret_cast<unsigned*>(0x780258)=s.game_mode_780258;
        *reinterpret_cast<std::uint8_t*>(0x65a7ac)=s.flag_65a7ac;
        *reinterpret_cast<unsigned*>(0x8514a0)=s.flag_8514a0;
    };
    std::array<std::uint8_t,PcVehicleObjectBytes+16> object{};
    Bytes e(object.data(),object.size());
    for(unsigned model=0;model<30;++model)for(unsigned variant=0;variant<256;++variant)
      for(unsigned mode:{0u,4u,0xffffffffu})for(unsigned flag:{0u,1u,2u,255u})for(unsigned scene:{0u,1u,2u,255u}){
        VehicleParameterSelection s{std::uint8_t(variant),std::uint8_t(scene),mode,std::uint8_t(flag),flag};
        globals(s);object.fill(0xab);std::memcpy(reinterpret_cast<void*>(base),object.data(),object.size());
        original(0x5051d0,{base,model});VehicleParameterChoice choice;
        check(vehicle_parameter_choice_5051d0(pack,model,s,choice),"selector resolves");e.put32(0x2b4,choice.pc_address);
        check(!std::memcmp(object.data(),reinterpret_cast<void*>(base),object.size()),"whole selector event");
    }
    for(unsigned seed=0;seed<32;++seed)for(bool clear:{false,true}){
        for(unsigned i=0;i<object.size();++i)object[i]=std::uint8_t(seed*13+i*17);
        std::memcpy(reinterpret_cast<void*>(base),object.data(),object.size());original(0x4874f0,{base,unsigned(clear)});
        check(vehicle_clear_4874f0(e,clear)&&!std::memcmp(object.data(),reinterpret_cast<void*>(base),object.size()),"clear including untouched bytes/canary");
        original(0x455a90,{base});check(vehicle_reset_timer_455a90(e)&&!std::memcmp(object.data(),reinterpret_cast<void*>(base),object.size()),"timer including untouched bytes");
    }
    VehicleCreationQueue queue;std::array<std::uint8_t,76> shared;
    for(unsigned model=0;model<30;++model)for(unsigned variant:{0u,1u,2u,255u})
      for(unsigned flag:{0u,1u})for(unsigned lane=0;lane<8;++lane)for(unsigned colour=1;colour<=8;++colour){
        VehicleParameterSelection s{std::uint8_t(variant),2,4,std::uint8_t(flag),0};globals(s);
        queue.object.fill(0x5a);queue.fault=0;vehicle_creation_reset_49fa60(queue);
        // Consume a retained sixteen-entry queue, not only a fresh first record.
        for(unsigned i=0;i<16;++i)vehicle_creation_append_49fa80(queue,8+i,lane,(model+i)%30,colour,0x4081|((i&1)<<6));
        std::memcpy(reinterpret_cast<void*>(0x841ac0),queue.object.data(),queue.object.size());
        for(unsigned i=0;i<16;++i){
            object.fill(0xaa);shared.fill(0xcc);std::memcpy(reinterpret_cast<void*>(base),object.data(),object.size());
            std::memcpy(reinterpret_cast<void*>(0x83db30),shared.data(),shared.size());
            original(0x4a5830,{});VehicleParameterChoice choice;
            check(vehicle_construct_4a5830(e,queue,pack,s,Bytes(shared.data(),shared.size()),&choice),"constructor succeeds");
            if(inject&&model==0&&variant==0&&flag==0&&lane==0&&colour==1&&i==0)e.put8(0x11,29);
            const bool same=!std::memcmp(object.data(),reinterpret_cast<void*>(base),object.size());
            if(!same&&mismatches<5)for(unsigned o=0;o<object.size();++o)if(object[o]!=*reinterpret_cast<std::uint8_t*>(base+o)){
                std::cerr<<"first differing byte "<<std::hex<<o<<std::dec<<"\n";break;}
            check(same,"whole constructor object and canary");
            check(!std::memcmp(queue.object.data(),reinterpret_cast<void*>(0x841ac0),queue.object.size()),"retained queue/read cursor");
            check(!std::memcmp(shared.data(),reinterpret_cast<void*>(0x83db30),shared.size()),"shared reset and trailing bytes");
        }
    }
    std::cout<<"{\"routine\":\"vehicle_constructor_probe\",\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}\n";return mismatches?1:0;
}

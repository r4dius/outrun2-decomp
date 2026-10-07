if(only=="race_input_probe"){
    // 453640 switch record and 453860 analogue channels (with 4537C0) for
    // player 0, on random device records (8999C0) and all five configurations.
    using namespace outrun::platform;
    constexpr unsigned base=0x3a800000,cfg_at=base+0x100;
    map_at(base,0x1000,PROT_READ|PROT_WRITE|PROT_EXEC);map_original_arithmetic_bridge(file);
    for(std::uint32_t page:{0x899000u,0x7d6000u,0x7c2000u,0x780000u})
        if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("input globals");
    const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
    unsigned cases{},mismatches{};
    auto check=[&](bool ok,const char* what,unsigned i){++cases;if(!ok){++mismatches;if(mismatches<9)std::cerr<<what<<" case "<<i<<"\n";}};
    auto g32=[](std::uint32_t a){return *reinterpret_cast<const std::uint32_t*>(a);};
    std::mt19937 rng(0x453860u);
    PcInputSwitchRecord rec{};PcInputAnalog an{};float filter=0.f;
    std::memset(reinterpret_cast<void*>(0x7d6770u),0,0x10);std::memset(reinterpret_cast<void*>(0x7d6810u),0,0x70);
    *reinterpret_cast<float*>(0x7d6884u)=0.f;
    unsigned digital=0,analog_cases=0;
    for(unsigned i=0;i<60000;++i){
        const unsigned which=i%5u;
        const PcInputConfig cfg=which==0?pc_input_keyboard_config():pc_input_joystick_config(which-1u);
        std::uint32_t words[7]{cfg.steer_axis,cfg.accel_axis,cfg.brake_axis,cfg.shift_up_mask,cfg.shift_down_mask,cfg.view_mask,cfg.flags};
        std::memcpy(reinterpret_cast<void*>(cfg_at),words,sizeof words);
        PcInputDevice d{};d.buttons_04=(i%3u==0u)?rng():(rng()&0x3ffu);
        for(auto& a:d.axes_94){const unsigned k=rng()%6u;a=std::int16_t(k==0?0:k==1?32767:k==2?-32768:k==3?int(rng()%512)-256:std::int16_t(rng()));}
        if(i%4u==0u){d.axes_94[cfg.accel_axis]=std::int16_t(rng()%256u);d.axes_94[cfg.brake_axis]=std::int16_t(rng()%256u);}
        std::memcpy(reinterpret_cast<void*>(0x8999c0u+4u),&d.buttons_04,4);
        std::memcpy(reinterpret_cast<void*>(0x8999c0u+0x94u),d.axes_94.data(),sizeof(d.axes_94));
        const std::int8_t option=std::int8_t(rng()%9u);const std::int32_t mode=(i%2u)?16:std::int32_t(rng()%40u);
        *reinterpret_cast<std::int8_t*>(0x7c24cbu)=option;*reinterpret_cast<std::int32_t*>(0x78026cu)=mode;
        if(i%50u==0u){filter=float(int(rng()%201u)-100)/100.f;*reinterpret_cast<float*>(0x7d6884u)=filter;}
        prepare(0x453640);guest_call.eax=0;guest_call.ebx=cfg_at;run_original32();
        pc_input_switch_453640(rec,d,cfg);
        prepare(0x453860);guest_call.eax=0;guest_call.edi=cfg_at;run_original32();
        pc_input_analog_453860(an,filter,d,cfg,option,mode);
        if(!(cfg.flags&1u))++digital;else ++analog_cases;
        if(inject&&i==59999)an.current[0]^=1;
        check(g32(0x7d6770u)==rec.held&&g32(0x7d6774u)==rec.previous&&g32(0x7d6778u)==rec.pressed&&g32(0x7d677cu)==rec.released,"453640 record",i);
        bool ok=true;
        for(unsigned k=0;k<7;++k){ok=ok&&std::int32_t(g32(0x7d6814u+k*0x10u))==an.current[k]&&std::int32_t(g32(0x7d6818u+k*0x10u))==an.previous[k];}
        ok=ok&&std::int32_t(g32(0x7d682cu))==an.accel_edge_1c&&std::int32_t(g32(0x7d683cu))==an.brake_edge_2c;
        std::uint32_t fb;std::memcpy(&fb,&filter,4);ok=ok&&g32(0x7d6884u)==fb;
        check(ok,"453860 analogue",i);
    }
    std::cerr<<"input coverage: digital="<<digital<<" analogue="<<analog_cases<<"\n";
    std::cout<<"{\"routine\":\"race_input_probe\",\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}"<<'\n';return mismatches?1:0;
}

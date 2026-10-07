// Included by race_hud_probe.inc (shared sprite pool/bank set-up, d3dx wired).
// Original 496A30 (GAME part after 4965A0 returned 1 at stage 4) with its real
// callees: 4659F0/465860/465970/465310/4653C0/465250 on the static UI
// resources and the sprite pool, 45BD30/45BCD0 voice queue with _ftol2,
// 4960A0/496580/4B8EF0/496460/495A20/477350/477430/477B60/496230/495C20/
// 47DA80/477DC0/495E90/495D90 (409F90/40A7D0/40A170/449940/40A010 with the
// pinned D3DX) and the protected 49B2D0/49701D bridges. Substituted leaves:
// 424940 (sound request) and 4763D0 (traffic-car release) are capture stubs;
// 449E30 (elapsed ms for the UI motion) returns a scripted float.
{
    constexpr unsigned rows=base+0x40000,car_at=base+0x48000,spec_at=base+0x49000,camera_at=base+0x4b000,
        racers_at=base+0x4c000,rank_at=base+0x4d000,stubs=base+0x50000,calls=base+0x58000,clock=base+0x5f000;
    for(auto page:{0x7d3000u,0x799000u,0x79f000u,0x80f000u,0x64e000u,0x7c2000u,0x7f1000u,0x7f8000u,0x7f9000u,0x842000u,0x655000u,0x780000u})
        if(mprotect(reinterpret_cast<void*>(page),0x1000u,PROT_READ|PROT_WRITE))throw std::runtime_error("mission race globals protection");
    RaceAssetPack races;std::string error;
    if(!load_race_asset_pack_file("../OutRun2006 Coast 2 Coast (FXT)/Scripts/bin/Races.bin",races,&error)&&
       !load_race_asset_pack_file("../../OutRun2006 Coast 2 Coast (FXT)/Scripts/bin/Races.bin",races,&error))throw std::runtime_error(error);
    if(races.race_count*0x44u>0x8000u)throw std::runtime_error("Races.bin too large for the fixture");
    std::memcpy(reinterpret_cast<void*>(rows),races.bytes.data()+races.races_offset,races.race_count*0x44);
    // Capture stubs: calls[16+8n] = {pc, value}; 424940 records its argument,
    // 4763D0 its ESI car index. 449E30 loads the scripted milliseconds.
    unsigned stub_at=stubs;
    auto install=[&](unsigned pc,const std::vector<std::uint8_t>& code){r060_raw_write(stub_at,code.data(),code.size());r046_write_rel32(pc,0xe9,stub_at);stub_at+=0x100;};
    auto recorder=[&](unsigned pc,bool esi){std::vector<std::uint8_t> c{0x8b,0x15};auto imm=[&](unsigned v){for(unsigned j=0;j<4;++j)c.push_back(std::uint8_t(v>>(8*j)));};
        imm(calls);if(!esi)c.insert(c.end(),{0x8b,0x44,0x24,0x04});
        c.insert(c.end(),{0xc7,0x04,0xd5});imm(calls+16);imm(pc);
        c.insert(c.end(),{0x89,std::uint8_t(esi?0x34:0x04),0xd5});imm(calls+20);
        c.insert(c.end(),{0xff,0x05});imm(calls);c.push_back(0xc3);install(pc,c);};
    recorder(0x424940,false);recorder(0x4763d0,true);
    {std::vector<std::uint8_t> c{0xd9,0x05};for(unsigned j=0;j<4;++j)c.push_back(std::uint8_t(clock>>(8*j)));c.push_back(0xc3);install(0x449e30,c);}
    const float delta_ms=1000.0f/60.0f;std::memcpy(reinterpret_cast<void*>(clock),&delta_ms,4);
    FrontendUiResources ui{pool};ui.motion_step=float(static_cast<long double>(delta_ms)*0.001f);
    auto g8=[](unsigned a){return *reinterpret_cast<std::uint8_t*>(a);};
    auto p8=[](unsigned a,std::uint8_t v){*reinterpret_cast<std::uint8_t*>(a)=v;};
    auto rf=[&](float lo,float hi){return lo+(hi-lo)*float(rng()%100000)/100000.0f;};
    unsigned frames_run{},banner_frames{},scored{},markers{},releases{},sounds_played{},race_ends{};
    for(unsigned rec=0;rec<races.race_count;++rec)for(unsigned scenario=0;scenario<(races.bytes[races.races_offset+rec*0x44+0x20]?16u:2u);++scenario){
        // Fresh pool and static UI resources (465160).
        pool.reset();std::memset(reinterpret_cast<void*>(pool_base),0,0x7c*FrontendSprites::Count);std::memset(reinterpret_cast<void*>(0x956500),0,0xac);
        p32(0x95b214,0);ui.pause_domain=0;
        MissionManagerState m;m.record_83637c=std::int32_t(rec);m.stage_836384=4;
        mission_manager_construct_ui_465160(m,ui);
        for(unsigned a:{0x836590u,0x836450u,0x8364f0u,0x8363b0u}){std::memset(reinterpret_cast<void*>(a),0,0xa0);prepare(0x465160);guest_call.ecx=a;run();}
        check(!std::memcmp(m.ui_836590.data(),reinterpret_cast<void*>(0x836590),0xa0)&&!std::memcmp(m.ui_836450.data(),reinterpret_cast<void*>(0x836450),0xa0)&&
              !std::memcmp(m.ui_8364f0.data(),reinterpret_cast<void*>(0x8364f0),0xa0)&&!std::memcmp(m.ui_8363b0.data(),reinterpret_cast<void*>(0x8363b0),0xa0),"465160 UI resources");
        // Random race state.
        std::vector<std::uint8_t> car(0x1000),camera(0x400);for(auto& b:car)b=std::uint8_t(rng());for(auto& b:camera)b=std::uint8_t(rng());
        Bytes cb(car.data(),car.size()),kb(camera.data(),camera.size());
        auto matrix=[&](Bytes b,unsigned off,float spread){for(unsigned k=0;k<16;++k)b.putf(off+k*4,k==15?1.0f:(k%4==3?0.0f:(k>=12?rf(-spread,spread):rf(-1.5f,1.5f))));};
        matrix(cb,0xb0,400.0f);matrix(kb,0x140,40.0f);
        cb.put32(0x2b4,spec_at);const float spec=rf(1.0f,4.0f);std::memcpy(reinterpret_cast<void*>(spec_at+0x134c),&spec,4);
        kb.put8(0x34a,std::uint8_t(rng()%4));kb.put32(0x354,rng()%6==0);kb.putf(0xb0,rf(200.0f,400.0f));kb.putf(0xb4,rf(-300.0f,300.0f));
        const auto* recp=races.bytes.data()+races.races_offset+rec*0x44;std::uint32_t type;std::memcpy(&type,recp+0x20,4);
        RacerSetupState racers;racers.count_80fb04=rng()%7;if(type==2u||type==3u)racers.count_80fb04=1+rng()%6;racers.racers_80fb00.resize(racers.count_80fb04*0xa0);
        for(auto& b:racers.racers_80fb00)b=std::uint8_t(rng());
        // +0x76 indexes the 6-entry 67E6F8/67E710 tables (496E3E/495840 do not bound it).
        for(std::uint32_t i=0;i<racers.count_80fb04;++i)racers.racers_80fb00[i*0xa0+0x76]=std::uint8_t(rng()%6);
        racers.v80fb2c=type==2u||type==3u?2+rng()%5:1+rng()%6;racers.v64e194=rng()%3==0||racers.count_80fb04==0?-1:std::int32_t(rng()%racers.count_80fb04);
        racers.v64e190=rng()%2||racers.count_80fb04==0?-1:std::int32_t(rng()%racers.count_80fb04);
        racers.table_80fb1c.resize(0x40);for(auto& b:racers.table_80fb1c)b=0;
        {const std::int32_t k=racers.count_80fb04?std::int32_t(rng()%racers.count_80fb04):-1;std::memcpy(racers.table_80fb1c.data()+0x24,&k,4);}
        PcLicense profile{};for(auto& b:profile)b=std::uint8_t(rng()%9);
        NaviVoiceQueue voice;voice.tail_7f1994=std::int32_t(rng()%8);voice.head_7f1c64=rng()%2?voice.tail_7f1994:std::int32_t(rng()%8);
        for(auto& e:voice.entries_7f1998){e.id=rng()%4?std::int32_t(0x100+rng()%0x200):-1;e.duration=std::int32_t(rng()%200);e.priority=std::int16_t(rng()%20);e.pad=std::uint16_t(rng());}
        float time=float(rng()%100000)/60.0f;voice.last_7f8b40=std::uint32_t(time)-rng()%300;
        std::uint32_t variant=rng()%3?6u:(rng()%2?1u:2u);std::uint8_t model=std::uint8_t(rng()%4);std::uint32_t busy=rng()%8==0;
        m.countdown_836390=0x168;m.timer_836388=rng()%3000;m.flag_67e6ac=1;m.selection_836374=rng()%4!=0;
        for(unsigned frame=0;frame<48;++frame){
            // Perturbations that reach every branch.
            const unsigned pick=rng()%16;
            if(pick==0)m.countdown_836390=0xb4;else if(pick==1)m.countdown_836390=rng()%4;else if(pick==2)m.countdown_836390=0x168;
            if(pick==3)m.timer_836388=std::uint32_t(std::int32_t(rng()%3)-1);else if(pick==4)m.timer_836388=0x168;else if(pick==5)m.timer_836388=60u*(rng()%7)+rng()%3;
            else if(pick==6)m.timer_836388=rng()%400;
            if(pick==7)cb.put8(0xc36,std::uint8_t(racers.v80fb2c-1));else if(pick==8)cb.put8(0xc36,std::uint8_t(rng()%6));
            if(rng()%5==0)m.v836394=rng()%4;if(rng()%6==0)m.v83638c=rng()%2?600:rng()%3;
            if(rng()%4==0)m.v8363a0=m.timer_836388+std::uint32_t(int(rng()%1400)-700);
            if(rng()%5==0)m.v8363a8=rng()%7;if(rng()%5==0)m.flag_67e6ac=std::uint8_t(rng()%2);
            if(rng()%8==0)racers.v64e194=rng()%3==0||racers.count_80fb04==0?-1:std::int32_t(rng()%racers.count_80fb04);
            if(rng()%3==0){cb.put8(0x244,std::uint8_t(rng()%4?0:rng()));cb.put8(0xda4,std::uint8_t(int(rng()%5)-2));cb.put8(0xd36,std::uint8_t(int(rng()%5)-2));
                cb.put32(4,(rng()%2?0x10000u:0)|(rng()%2?0x4000000u:0)|(rng()&0xff));cb.putf(0x1c4,rf(0.0f,2.0f));cb.putf(0xe68,rf(0.0f,0.4f));
                cb.put16(0x162,std::uint16_t(std::int16_t(int(rng()%4000)-2000)));}
            for(std::uint32_t i=0;i<racers.count_80fb04;++i)if(rng()%4==0){auto rc=Bytes(racers.racers_80fb00.data()+i*0xa0,0xa0);
                rc.put8(0x74,std::uint8_t(rng()%6));rc.put8(0x76,std::uint8_t(rng()%6));rc.put8(0x71,std::uint8_t(rng()));rc.put32(0x6c,rng()%3==0?rng():0);
                rc.put32(0x98,rng()%3==0?rng()%3:rng()%900);rc.put32(0x9c,rng()%3==0?600:rng()%620);rc.put32(0x4c,rng()%3?0xffffffffu:rng()%20);}
            std::uint32_t mode=rng()%12==0?15u:16u,end=rng()%14==0?1u:0u;if(const char* d=std::getenv("OR2_DBG_MODE"))mode=std::uint32_t(std::stoul(d));if(std::getenv("OR2_DBG_CD"))m.countdown_836390=std::uint32_t(std::stoul(std::getenv("OR2_DBG_CD")));const std::int16_t pause=std::int16_t(rng()%14==0?int(rng()%5)-2:0);
            time+=1.0f/60.0f;
            // Guest state.
            p32(0x836384,4);p32(0x83637c,rows+rec*0x44);p32(0x836388,m.timer_836388);p32(0x83638c,m.v83638c);p32(0x836390,m.countdown_836390);
            p32(0x836394,m.v836394);p32(0x8363a0,m.v8363a0);p8(0x8363a4,m.v8363a4);p32(0x8363a8,m.v8363a8);p8(0x67e6ac,m.flag_67e6ac);
            p32(0x67e6b0,m.v67e6b0);p32(0x67e6b8,std::uint32_t(m.v67e6b8));p8(0x836374,m.selection_836374);
            p32(0x780258,variant);p32(0x78026c,mode);p32(0x7d38f0,end);*reinterpret_cast<std::int16_t*>(0x8367bc)=pause;p8(0x655b59,model);
            std::memcpy(reinterpret_cast<void*>(car_at),car.data(),car.size());p32(0x799d18,car_at);
            std::memcpy(reinterpret_cast<void*>(camera_at),camera.data(),camera.size());p32(0x79f574,camera_at);
            if(!racers.racers_80fb00.empty())std::memcpy(reinterpret_cast<void*>(racers_at),racers.racers_80fb00.data(),racers.racers_80fb00.size());
            p32(0x80fb00,racers.count_80fb04?racers_at:0);p32(0x80fb04,racers.count_80fb04);p32(0x80fb2c,racers.v80fb2c);
            std::memcpy(reinterpret_cast<void*>(rank_at),racers.table_80fb1c.data(),racers.table_80fb1c.size());p32(0x80fb1c,rank_at);
            p32(0x64e190,std::uint32_t(racers.v64e190));p32(0x64e194,std::uint32_t(racers.v64e194));
            std::memcpy(reinterpret_cast<void*>(0x7c23e0),profile.data(),profile.size());
            p32(0x7f1994,std::uint32_t(voice.tail_7f1994));p32(0x7f1c64,std::uint32_t(voice.head_7f1c64));p32(0x7f8b40,voice.last_7f8b40);
            std::memcpy(reinterpret_cast<void*>(0x7f1998),voice.entries_7f1998.data(),96);p32(0x7f94c0,busy);p32(0x7f95c4,busy?7u:0u);
            std::memcpy(reinterpret_cast<void*>(0x842110),&time,4);
            reset_matrix_oracle_globals();
            std::vector<std::uint8_t> arena(0x10000);std::memcpy(arena.data(),reinterpret_cast<void*>(MatrixArena),arena.size());
            PcMatrixStack stack{Bytes(arena.data(),arena.size()),0,0,64};
            p32(calls,0);
            if(std::getenv("OR2_DBG")){for(unsigned a:{0x836450u,0x8364f0u}){prepare(0x4659f0);guest_call.ecx=a;run();std::cerr<<"tick ok"<<char(10);}
              prepare(0x4965a0);run();std::cerr<<"4965a0 ok "<<guest_call.out_eax<<char(10);prepare(0x45bcd0);run();std::cerr<<"pump ok"<<char(10);prepare(0x495e90);run();std::cerr<<"marker ok"<<char(10);for(unsigned f:{0x450240u,0x49b2d0u,0x4960a0u,0x4b8ef0u,0x496460u,0x477350u,0x477b60u,0x495c20u}){std::cerr<<std::hex<<f<<std::dec<<char(10);prepare(f);reinterpret_cast<unsigned*>(S)[0]=car_at;run_original32();std::cerr<<" ok"<<char(10);}p32(calls,0);}
            prepare(0x496a30);run();
            // Native.
            MissionManagerServices s;s.races=&races;s.variant_780258=variant;
            MissionRaceServices r;r.ui=&ui;r.car=cb;r.car_spec_134c=spec;r.camera=kb;r.matrices=&stack;
            {const auto idx=std::int8_t(model);float v[3];std::memcpy(v,reinterpret_cast<void*>(0x5e0ac8+idx*12),12);r.model_offset_5e0ac8={v[0],v[1],v[2]};}
            r.racers=&racers;r.profile=&profile;r.voice=&voice;r.voice_services.time_842110=time;r.voice_services.busy_7f94c0=busy;r.voice_services.busy_7f95c4=busy?7u:0u;
            r.mode_78026c=mode;r.race_end_7d38f0=end;r.pause_8367bc=pause;
            std::vector<std::uint32_t> released,sounds;r.user=&released;r.sounds_424940=&sounds;
            r.release_car_4763d0=[](void* u,std::uint32_t i){static_cast<std::vector<std::uint32_t>*>(u)->push_back(i);return true;};
            ui.missing_pc=0;
            const bool native_ok=mission_manager_control_496a30(m,s,r);
            if(!native_ok&&mismatches<14)std::cerr<<"native 496A30 missing "<<std::hex<<r.missing<<std::dec<<" rec "<<rec<<" type "<<type<<char(10);
            check(native_ok,"native 496A30");
            if(inject&&!injected&&frame==20){m.timer_836388^=1;injected=true;}
            ++frames_run;if(m.countdown_836390)++banner_frames;
            // Compare.
            const bool globals=g32(0x836388)==m.timer_836388&&g32(0x83638c)==m.v83638c&&g32(0x836390)==m.countdown_836390&&g32(0x836394)==m.v836394&&
                g32(0x8363a0)==m.v8363a0&&g8(0x8363a4)==m.v8363a4&&g32(0x8363a8)==m.v8363a8&&g8(0x67e6ac)==m.flag_67e6ac&&g32(0x67e6b0)==m.v67e6b0&&
                g32(0x67e6b8)==std::uint32_t(m.v67e6b8)&&g32(0x836384)==4u;
            if(!globals&&mismatches<14)std::cerr<<"rec "<<rec<<" type "<<type<<" frame "<<frame<<" 836388 "<<std::int32_t(g32(0x836388))<<"/"<<std::int32_t(m.timer_836388)
                <<" 83638c "<<g32(0x83638c)<<"/"<<m.v83638c<<" 836390 "<<g32(0x836390)<<"/"<<m.countdown_836390<<" 836394 "<<g32(0x836394)<<"/"<<m.v836394
                <<" 8363a0 "<<g32(0x8363a0)<<"/"<<m.v8363a0<<" 8363a8 "<<g32(0x8363a8)<<"/"<<m.v8363a8<<" 67e6b0 "<<g32(0x67e6b0)<<"/"<<m.v67e6b0
                <<" 67e6b8 "<<std::hex<<g32(0x67e6b8)<<"/"<<m.v67e6b8<<std::dec<<" 67e6ac "<<unsigned(g8(0x67e6ac))<<"/"<<unsigned(m.flag_67e6ac)<<char(10);
            check(globals,"mission globals 836384..8363A8 / 67E6AC..67E6B8");
            auto elem=[&](const std::array<std::uint8_t,0xa0>& n,unsigned a,const char* label){
                const bool ok=!std::memcmp(n.data(),reinterpret_cast<void*>(a),0xa0);
                if(!ok&&mismatches<14){std::cerr<<label<<" rec "<<rec<<" frame "<<frame<<" offsets";for(unsigned k=0;k<0xa0;k+=4)if(std::memcmp(n.data()+k,reinterpret_cast<void*>(a+k),4))std::cerr<<" "<<std::hex<<k<<":"<<g32(a+k)<<"/"<<Bytes(const_cast<std::uint8_t*>(n.data()),0xa0).u32(k)<<std::dec;std::cerr<<char(10);}
                check(ok,label);};
            elem(m.ui_836450,0x836450,"UI 836450");elem(m.ui_8364f0,0x8364f0,"UI 8364F0");elem(m.ui_836590,0x836590,"UI 836590");elem(m.ui_8363b0,0x8363b0,"UI 8363B0");
            compare_pool("mission sprite pool",true);
            {const bool ok=!std::memcmp(car.data(),reinterpret_cast<void*>(car_at),car.size());
             if(!ok&&mismatches<14){std::cerr<<"car offsets";for(unsigned k=0;k<car.size();++k)if(car[k]!=g8(car_at+k))std::cerr<<" "<<std::hex<<k<<std::dec;std::cerr<<char(10);}
             check(ok,"car object (C36/DA4/DBC)");}
            check(racers.racers_80fb00.empty()||!std::memcmp(racers.racers_80fb00.data(),reinterpret_cast<void*>(racers_at),racers.racers_80fb00.size()),"80FB00 racers");
            check(!std::memcmp(profile.data(),reinterpret_cast<void*>(0x7c23e0),profile.size()),"7C23E0 profile (495A20)");
            check(g32(0x7f1994)==std::uint32_t(voice.tail_7f1994)&&g32(0x7f1c64)==std::uint32_t(voice.head_7f1c64)&&g32(0x7f8b40)==voice.last_7f8b40&&
                  !std::memcmp(reinterpret_cast<void*>(0x7f1998),voice.entries_7f1998.data(),96),"7F1994 voice queue");
            check(g32(0x7d38f0)==r.race_end_7d38f0,"7D38F0 race end");
            {std::vector<std::uint32_t> gs,gr;for(unsigned i=0;i<g32(calls);++i){const auto pc=g32(calls+16+8*i),v=g32(calls+20+8*i);(pc==0x424940?gs:gr).push_back(v);}
             check(gs==sounds,"ordered 424940 sounds");check(gr==released,"ordered 4763D0 car releases");
             sounds_played+=unsigned(gs.size());releases+=unsigned(gr.size());}
            check(std::int32_t(g32(0x89b568))==stack.depth&&g32(0x89b564)==MatrixArena+unsigned(stack.current_offset)&&
                  !std::memcmp(arena.data(),reinterpret_cast<void*>(MatrixArena),64*4),"matrix stack");
            if(r.race_end_7d38f0)++race_ends;
            if(m.timer_836388!=g32(0x836388))++scored;
            if(m.ui_8364f0[8]!=0xff||m.ui_8363b0[8]!=0xff)++markers;
        }
    }
    {unsigned hist[8]{};for(unsigned i=0;i<races.race_count;++i){std::uint32_t t;std::memcpy(&t,races.bytes.data()+races.races_offset+i*0x44+0x20,4);++hist[t<7?t:7];}
     std::cerr<<"record types";for(unsigned t=0;t<8;++t)std::cerr<<" "<<t<<":"<<hist[t];std::cerr<<char(10);}
    std::cerr<<"frames "<<frames_run<<" banner "<<banner_frames<<" markers "<<markers<<" sounds "<<sounds_played<<" releases "<<releases<<" race_ends "<<race_ends<<char(10);
    coverage=frames_run;
}

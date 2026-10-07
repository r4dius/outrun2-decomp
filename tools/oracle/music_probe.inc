// Included inside the compare_pc dispatcher; uses its private x86 harness.
if(only=="frontend_music_probe"){
    // Original key-4 owner (4C9A90 construct, 4C9790 init, 4C9C20 control,
    // 4C9910 suspend, with 4C9940/4C99D0/4C9A30/4C9750, 4474E0, 48B1D0, the
    // CRT rand 580F40 and the real carousel 51B7B0/51BE30/51BB10/51BBA0/
    // 51BC30/51BDB0 and 442F20 on the owner) against frontend_music. UI
    // resource calls are capture stubs (configure arguments recorded), 48F5F0
    // returns the scripted action, 4536C0/407930 the scripted held inputs;
    // 4249F0 sounds and 401000/401030 music calls are recorded in order.
    using namespace outrun::platform;
    constexpr unsigned base=0x3a700000,root=base+0x2000,trace=base+0x4000,config=base+0x8000,device=base+0xa000,ptd=base+0xc000;
    map_at(base,0x20000,PROT_READ|PROT_WRITE|PROT_EXEC);map_original_arithmetic_bridge(file);
    for(unsigned page:{0x7c2000u,0x84a000u,0x84b000u,0x842000u,0x830000u})
        if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("music globals");
    auto stub=[](unsigned pc,unsigned value,unsigned short pop=0){std::array<std::uint8_t,8> code{0xb8,0,0,0,0,0xc2,0,0};
        std::memcpy(code.data()+1,&value,4);std::memcpy(code.data()+6,&pop,2);r060_raw_write(pc,code.data(),code.size());};
    unsigned stub_index{};
    auto install=[&](unsigned pc,const std::vector<std::uint8_t>& code){const unsigned dst=base+0x10000+512*stub_index++;
        r060_raw_write(dst,code.data(),code.size());std::array<std::uint8_t,5> jump{{0xe9,0,0,0,0}};
        const auto delta=dst-pc-5;std::memcpy(jump.data()+1,&delta,4);r060_raw_write(pc,jump.data(),jump.size());};
    auto capture=[&](unsigned pc,unsigned nargs,unsigned pop){std::vector<std::uint8_t> code{0x8b,0x15};
        auto imm=[&](unsigned v){for(unsigned j=0;j<4;++j)code.push_back(std::uint8_t(v>>(8*j)));};
        imm(trace);code.insert(code.end(),{0xc1,0xe2,4,0xc7,0x82});imm(trace+16);imm(pc);
        for(unsigned i=0;i<3;++i){if(i<nargs)code.insert(code.end(),{0x8b,0x44,0x24,std::uint8_t(4+4*i)});else code.insert(code.end(),{0x31,0xc0});
            code.insert(code.end(),{0x89,0x82});imm(trace+20+4*i);}
        code.insert(code.end(),{0xff,0x05});imm(trace);code.insert(code.end(),{0xc2,std::uint8_t(pop),0});install(pc,code);};
    capture(0x4249f0,1,0);capture(0x401000,3,0);capture(0x401030,1,0);
    stub(0x4035f0,root);stub(0x407930,device);stub(0x440ea0,0,32);stub(0x465970,0);stub(0x4659f0,0);stub(0x585bec,ptd);
    {std::vector<std::uint8_t> code{0xa1};for(unsigned j=0;j<4;++j)code.push_back(std::uint8_t((config+0x1000)>>(8*j)));code.push_back(0xc3);install(0x4652e0,code);}
    auto resource_stub=[&](bool configure){std::vector<std::uint8_t> code{0x89,0xca,0x81,0xea};
        auto imm=[&](unsigned v){for(unsigned j=0;j<4;++j)code.push_back(std::uint8_t(v>>(8*j)));};imm(base);
        code.insert(code.end(),{0xc7,0x82});imm(config);imm(configure?1:0);
        code.insert(code.end(),{0xc7,0x41,8});imm(configure?123:~0u);
        if(configure)for(unsigned i=0;i<11;++i){code.insert(code.end(),{0x8b,0x44,0x24,std::uint8_t(4+4*i),0x89,0x82});imm(config+4+4*i);}
        code.insert(code.end(),{0xc2,std::uint8_t(configure?44:0),0});install(configure?0x465860:0x465250,code);};
    resource_stub(false);resource_stub(true);
    unsigned cases{},mismatches{};const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
    auto check=[&](bool ok,const char* label){++cases;if(!ok){++mismatches;if(mismatches<15)std::cerr<<"music "<<cases<<" "<<label<<"\n";}};
    struct Trace {std::vector<std::array<unsigned,4>> calls;} calls;
    auto effect=[](void* p,unsigned id){static_cast<Trace*>(p)->calls.push_back({0x4249f0,id,0,0});return true;};
    auto play=[](void* p,unsigned channel,unsigned track,unsigned loop){static_cast<Trace*>(p)->calls.push_back({0x401000,channel,track,loop});return true;};
    auto stop=[](void* p,unsigned channel){static_cast<Trace*>(p)->calls.push_back({0x401030,channel,0,0});return true;};
    FrontendMusicGlobals music{};std::uint32_t seed{};
    auto sync=[&](FrontendMusic& c,const PcLicense& profile,const std::array<std::uint8_t,0xe00>& owner){
        std::memcpy(reinterpret_cast<void*>(base),c.object.data(),c.object.size());std::memcpy(reinterpret_cast<void*>(root),owner.data(),owner.size());
        std::memcpy(reinterpret_cast<void*>(0x7c23e0),profile.data(),profile.size());
        std::memcpy(reinterpret_cast<void*>(0x84b0ec),music.cursors_84b0ec.data(),6);*reinterpret_cast<std::int16_t*>(0x84b0f4)=music.list_84b0f4;
        *reinterpret_cast<std::uint8_t*>(0x830364)=music.track_830364;*reinterpret_cast<std::uint32_t*>(ptd+0x14)=seed;
        std::memset(reinterpret_cast<void*>(trace),0,0x3000);std::memset(reinterpret_cast<void*>(config),0,0x1000);calls.calls.clear();};
    auto compare=[&](FrontendMusic& c,const std::array<std::uint8_t,0xe00>& owner){
        Bytes b(c.object.data(),c.object.size()),g(reinterpret_cast<void*>(base),c.object.size());
        check(b.u8(0x34)==g.u8(0x34)&&b.u8(0x35)==g.u8(0x35)&&b.u32(0x38)==g.u32(0x38)&&b.u32(4)==g.u32(4),"latches +34/+35/+38");
        check(!std::memcmp(c.object.data()+0x35c+0xa4,reinterpret_cast<void*>(base+0x35c+0xa4),0x74),"carousel tail");
        check(!std::memcmp(owner.data(),reinterpret_cast<void*>(root),0x51c),"owner object (442F20)");
        check(!std::memcmp(music.cursors_84b0ec.data(),reinterpret_cast<void*>(0x84b0ec),6)&&music.list_84b0f4==*reinterpret_cast<std::int16_t*>(0x84b0f4),"84B0EC cursors / 84B0F4 list");
        check(music.track_830364==*reinterpret_cast<std::uint8_t*>(0x830364),"830364 chosen track");
        check(seed==*reinterpret_cast<std::uint32_t*>(ptd+0x14),"CRT random state");
        check(calls.calls.size()==*reinterpret_cast<unsigned*>(trace)&&!std::memcmp(calls.calls.data(),reinterpret_cast<void*>(trace+16),16*calls.calls.size()),"ordered sound/music calls");
        constexpr unsigned fields[]{0,0xc,0x10,0x14,0x18,0x40,0x44,0x64,0x68,0x3c,0x24};
        for(unsigned off:{0x3cu,0xdcu,0x17cu,0x21cu,0x2bcu,0x360u})if(*reinterpret_cast<unsigned*>(config+off)){
            const auto* a=reinterpret_cast<unsigned*>(config+off+4);
            for(unsigned i=0;i<11;++i)check(b.u32(off+fields[i])==(i==4&&a[i]==3?1:a[i]),"authored resource argument");
        }
    };
    constexpr unsigned masks[]{4,8,0x400,0x1000,0x800,0x2000,0x10,0};
    constexpr int counts[3]{9,7,12};
    unsigned played{},stopped{},picked{},refused{},moved{};
    auto tally=[&](const FrontendMusicGlobals& before){for(auto& r:calls.calls){played+=r[0]==0x401000;stopped+=r[0]==0x401030;refused+=r[0]==0x4249f0&&r[1]==3;}
        picked+=before.track_830364!=music.track_830364;moved+=before.cursors_84b0ec!=music.cursors_84b0ec||before.list_84b0f4!=music.list_84b0f4;};
    for(unsigned run=0;run<12;++run)for(unsigned list=0;list<3;++list)for(unsigned where=0;where<3;++where){
        FrontendMusic c;std::array<std::uint8_t,0xe00> owner{};Bytes rb(owner.data(),owner.size());PcLicense profile{};
        for(unsigned k=0;k<0x51c;k+=4)rb.put32(k,run*0x01010101u+k);rb.put8(0x220,std::uint8_t(run&7));
        if(run==1)profile.fill(255);else if(run>1)for(unsigned i=0;i<0x40;++i)profile[0x28+i]=std::uint8_t(i*37+run*11);
        FrontendSprites sprites;sprites.bind_timing(0x44,std::vector<FrontendSpriteTiming>(224,{400,60}));
        FrontendUiResources ui{sprites};ui.effect_user=&calls;ui.effect_4249f0=effect;PcUiNotifyGlobals globals;FrontendInputSnapshot input;unsigned repeat{},result{};
        ui.commands(0x442ac0,rb.sub(0x51c,owner.size()-0x51c),nullptr,0,globals,result);
        music={};music.list_84b0f4=std::int16_t(list);
        for(unsigned l=0;l<3;++l)music.cursors_84b0ec[l]=std::int16_t(where==0?0:where==1?(run+l)%unsigned(counts[l]):counts[l]-1);
        music.track_830364=std::uint8_t(run*5);seed=run*0x9e3779b9u;
        FrontendMusicServices s{ui,rb,globals,profile,input,repeat,music,seed,false,3.5f,&calls,play,stop};
        check(frontend_music_construct_4c9a90(c,repeat),"native construct");
        sync(c,profile,owner);*reinterpret_cast<float*>(0x842110)=3.5f;prepare(0x4c9790);guest_call.ecx=base;run_original32();
        check(frontend_music_init_4c9790(c,s),"native init");
        if(inject&&run==2&&list==1&&where==1)music.cursors_84b0ec[1]^=1;
        compare(c,owner);check((guest_call.out_eax&255)==1,"init result");
        const auto initialized=c;const auto initialized_sprites=sprites;const auto initialized_owner=owner;const auto m0=music;const auto seed0=seed;
        for(unsigned phase=0;phase<5;++phase)for(unsigned action=0;action<8;++action)for(unsigned ready=0;ready<2;++ready){
            if(phase==2&&action)continue;                                    // random pick ignores the action
            c=initialized;sprites=initialized_sprites;owner=initialized_owner;music=m0;seed=seed0+phase*7+action;s.timer=4;
            Bytes cb(c.object.data(),c.object.size());
            // 0 starting frame, 1 leaving (+38 = 2 or 5), 2 random held, 3 list switch held, 4 idle.
            cb.put8(0x35,phase==0);if(phase==1){cb.put8(0x34,1);cb.put32(0x38,action&1?5:2);}
            {auto r=cb.sub(0xdc,0xa0);if(ready){r.put32(0x24,0);r.put32(8,~0u);}else r.put32(0x24,1);}
            repeat=action<7?action:12;input.feature_mask=masks[action];input.device_held=phase==3?0x2000:0;s.random_held_4536c0=phase==2;
            sync(c,profile,owner);*reinterpret_cast<unsigned*>(device+8)=input.device_held;*reinterpret_cast<float*>(0x842110)=4;
            *reinterpret_cast<unsigned*>(config+0x1000)=ready;
            stub(0x48f5f0,action<7?action:12,4);stub(0x4536c0,s.random_held_4536c0?0x10:0);
            prepare(0x4c9c20);guest_call.ecx=base;run_original32();const auto original=guest_call.out_eax;
            s.ui.missing_pc=0;c.fault=0;
            check(frontend_music_control_4c9c20(c,s,result),"native control");compare(c,owner);check(result==original,"control result");tally(m0);
        }
        c=initialized;sprites=initialized_sprites;owner=initialized_owner;music=m0;seed=seed0;
        sync(c,profile,owner);prepare(0x4c9910);guest_call.ecx=base;run_original32();
        check(frontend_music_suspend_4c9910(c,s),"native suspend");compare(c,owner);
    }
    // Random pick over every unlock pattern: the retry loop, the 100-try cap and 48B1D0.
    for(unsigned pattern=0;pattern<64;++pattern){
        FrontendMusic c;std::array<std::uint8_t,0xe00> owner{};Bytes rb(owner.data(),owner.size());PcLicense profile{};
        for(unsigned i=0;i<0x40;++i)profile[0x28+i]=std::uint8_t(pattern&1?0:(i*pattern*29+pattern));
        FrontendSprites sprites;sprites.bind_timing(0x44,std::vector<FrontendSpriteTiming>(224,{400,60}));
        FrontendUiResources ui{sprites};ui.effect_user=&calls;ui.effect_4249f0=effect;PcUiNotifyGlobals globals;FrontendInputSnapshot input;unsigned repeat{},result{};
        ui.commands(0x442ac0,rb.sub(0x51c,owner.size()-0x51c),nullptr,0,globals,result);
        music={};seed=pattern*0x01000193u+7;
        FrontendMusicServices s{ui,rb,globals,profile,input,repeat,music,seed,true,4.f,&calls,play,stop};
        check(frontend_music_construct_4c9a90(c,repeat),"native construct");
        sync(c,profile,owner);*reinterpret_cast<float*>(0x842110)=4;prepare(0x4c9790);guest_call.ecx=base;run_original32();
        check(frontend_music_init_4c9790(c,s),"native init");compare(c,owner);
        Bytes(c.object.data(),c.object.size()).put8(0x35,0);repeat=12;input.feature_mask=0;
        sync(c,profile,owner);*reinterpret_cast<unsigned*>(device+8)=0;stub(0x48f5f0,12,4);stub(0x4536c0,0x10);
        prepare(0x4c9c20);guest_call.ecx=base;run_original32();const auto original=guest_call.out_eax;
        check(frontend_music_control_4c9c20(c,s,result),"native random control");compare(c,owner);check(result==original,"random result");
    }
    std::cout<<"{\"routine\":\"frontend_music_probe\",\"coverage\":{\"music_plays\":"<<played<<",\"music_stops\":"<<stopped<<",\"tracks_chosen\":"<<picked<<",\"locked_refusals\":"<<refused<<",\"cursor_or_list_moves\":"<<moved<<"},\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}\n";return mismatches?1:0;
}

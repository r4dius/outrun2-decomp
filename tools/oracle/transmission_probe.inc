// Included inside the compare_pc dispatcher; uses its private x86 harness.
if(only=="frontend_transmission_probe"){
    // Original key-16 owner (4DD410 construct, 4DD250 init, 4DD4A0 control,
    // 4DD2E0 suspend) with the real carousel 51B7B0/51BE30/51BB10/51BBA0/51BC30,
    // 440F70/441130/440DF0 on the owner and 48B170, against
    // frontend_transmission. UI resource calls are capture stubs (configure
    // arguments recorded), 48F5F0 returns the scripted action, 4249F0 sounds
    // are recorded.
    using namespace outrun::platform;
    constexpr unsigned base=0x3a700000,root=base+0x2000,trace=base+0x4000,config=base+0x8000;
    map_at(base,0x20000,PROT_READ|PROT_WRITE|PROT_EXEC);map_original_arithmetic_bridge(file);
    for(unsigned page:{0x68a000u,0x84a000u,0x84b000u,0x842000u,0x830000u})
        if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("transmission globals");
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
    capture(0x4249f0,1,0);
    stub(0x4035f0,root);stub(0x465970,0);stub(0x4659f0,0);
    // 4652E0 (resource ready) returns the scripted value in [config+0x1000].
    {std::vector<std::uint8_t> code{0xa1};for(unsigned j=0;j<4;++j)code.push_back(std::uint8_t((config+0x1000)>>(8*j)));code.push_back(0xc3);install(0x4652e0,code);}
    auto resource_stub=[&](bool configure){std::vector<std::uint8_t> code{0x89,0xca,0x81,0xea};
        auto imm=[&](unsigned v){for(unsigned j=0;j<4;++j)code.push_back(std::uint8_t(v>>(8*j)));};imm(base);
        code.insert(code.end(),{0xc7,0x82});imm(config);imm(configure?1:0);
        code.insert(code.end(),{0xc7,0x41,8});imm(configure?123:~0u);
        if(configure)for(unsigned i=0;i<11;++i){code.insert(code.end(),{0x8b,0x44,0x24,std::uint8_t(4+4*i),0x89,0x82});imm(config+4+4*i);}
        code.insert(code.end(),{0xc2,std::uint8_t(configure?44:0),0});install(configure?0x465860:0x465250,code);};
    resource_stub(false);resource_stub(true);
    unsigned cases{},mismatches{};const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
    auto check=[&](bool ok,const char* label){++cases;if(!ok){++mismatches;if(mismatches<15)std::cerr<<"transmission "<<cases<<" "<<label<<"\n";}};
    struct Trace {std::vector<std::array<unsigned,4>> calls;} calls;
    auto effect=[](void* p,unsigned id){static_cast<Trace*>(p)->calls.push_back({0x4249f0,id,0,0});return true;};
    outrun::driving::PcFloatTransitionGlobals transition{};std::uint32_t selection{};std::uint8_t gear{};
    auto sync=[&](FrontendTransmission& c,const std::array<std::uint8_t,0xe00>& owner){
        std::memcpy(reinterpret_cast<void*>(base),c.object.data(),c.object.size());std::memcpy(reinterpret_cast<void*>(root),owner.data(),owner.size());
        *reinterpret_cast<std::uint32_t*>(0x68a670)=transition.token;*reinterpret_cast<float*>(0x68a66c)=transition.primary;
        *reinterpret_cast<float*>(0x68a674)=transition.secondary;*reinterpret_cast<std::uint8_t*>(0x84a9fd)=transition.active;
        *reinterpret_cast<std::uint32_t*>(0x84b210)=selection;*reinterpret_cast<std::uint8_t*>(0x830374)=gear;
        std::memset(reinterpret_cast<void*>(trace),0,0x3000);std::memset(reinterpret_cast<void*>(config),0,0x1000);calls.calls.clear();};
    auto compare=[&](FrontendTransmission& c,const std::array<std::uint8_t,0xe00>& owner){
        Bytes b(c.object.data(),c.object.size()),g(reinterpret_cast<void*>(base),c.object.size());
        check(b.u32(4)==g.u32(4)&&b.u8(0x1ec)==g.u8(0x1ec)&&b.u8(0x1ed)==g.u8(0x1ed),"next child and latches");
        check(!std::memcmp(c.object.data()+0x34+0xa4,reinterpret_cast<void*>(base+0x34+0xa4),0x74),"carousel tail");
        check(!std::memcmp(owner.data(),reinterpret_cast<void*>(root),owner.size()),"owner object (440F70/441130/440DF0)");
        check(transition.token==*reinterpret_cast<std::uint32_t*>(0x68a670)&&transition.active==*reinterpret_cast<std::uint8_t*>(0x84a9fd)&&
              transition.primary==*reinterpret_cast<float*>(0x68a66c)&&transition.secondary==*reinterpret_cast<float*>(0x68a674),"transition globals");
        check(selection==*reinterpret_cast<std::uint32_t*>(0x84b210)&&gear==*reinterpret_cast<std::uint8_t*>(0x830374),"84B210 / 830374");
        check(calls.calls.size()==*reinterpret_cast<unsigned*>(trace)&&!std::memcmp(calls.calls.data(),reinterpret_cast<void*>(trace+16),16*calls.calls.size()),"ordered sounds");
        constexpr unsigned fields[]{0,0xc,0x10,0x14,0x18,0x40,0x44,0x64,0x68,0x3c,0x24};
        if(*reinterpret_cast<unsigned*>(config+0x14c)){
            const auto* a=reinterpret_cast<unsigned*>(config+0x14c+4);
            for(unsigned i=0;i<11;++i)check(b.u32(0x14c+fields[i])==(i==4&&a[i]==3?1:a[i]),"caption resource argument");
        }
    };
    constexpr unsigned masks[]{4,8,0x400,0x1000,0x800,0x2000,0x10,0};
    for(unsigned run=0;run<64;++run){
        FrontendTransmission c;std::array<std::uint8_t,0xe00> owner{};Bytes rb(owner.data(),owner.size());
        for(unsigned k=0;k<owner.size();k+=4)rb.put32(k,run*0x01010101u+k);
        FrontendSprites sprites;sprites.bind_timing(0x44,std::vector<FrontendSpriteTiming>(224,{400,60}));
        FrontendUiResources ui{sprites};ui.effect_user=&calls;ui.effect_4249f0=effect;FrontendInputSnapshot input;unsigned repeat{},result{};
        transition={run,1.f,0.5f,std::uint8_t(run&1)};selection=run;gear=std::uint8_t(run*3);
        FrontendTransmissionServices s{ui,rb,transition,input,repeat,selection,gear,3.5f};
        check(frontend_transmission_construct_4dd410(c,repeat),"native construct");
        sync(c,owner);*reinterpret_cast<float*>(0x842110)=3.5f;prepare(0x4dd250);guest_call.ecx=base;run_original32();
        check(frontend_transmission_init_4dd250(c,s),"native init");
        if(inject&&run==3)c.object[0x1ec]^=1;
        compare(c,owner);check((guest_call.out_eax&255)==1,"init result");
        const auto initialized=c;const auto initialized_sprites=sprites;const auto initialized_owner=owner;
        const auto t0=transition;const auto sel0=selection;const auto gear0=gear;
        for(unsigned step=0;step<3;++step)for(unsigned action=0;action<8;++action)for(unsigned ready=0;ready<2;++ready)for(unsigned latch=0;latch<3;++latch){
            c=initialized;sprites=initialized_sprites;owner=initialized_owner;transition=t0;selection=sel0;gear=gear0;s.timer=4;
            Bytes cb(c.object.data(),c.object.size());
            cb.put32(0xe8,step==2?1u:0u);
            if(latch){cb.put8(0x1ed,1);cb.put8(0x1ec,latch==2);}
            // 4652E0 is a constant stub on the guest; put the native caption
            // resource in the state for which the same logic returns it.
            {auto r=cb.sub(0x14c,0xa0);if(ready){r.put32(0x24,0);r.put32(8,~0u);}else r.put32(0x24,1);}
            repeat=action<7?action:12;input.feature_mask=masks[action];
            sync(c,owner);*reinterpret_cast<float*>(0x842110)=4;*reinterpret_cast<unsigned*>(config+0x1000)=ready;
            stub(0x48f5f0,action<7?action:12,4);
            prepare(0x4dd4a0);guest_call.ecx=base;run_original32();const auto original=guest_call.out_eax;
            s.ui.missing_pc=0;c.fault=0;
            check(frontend_transmission_control_4dd4a0(c,s,result),"native control");compare(c,owner);check(result==original,"control result");
        }
        c=initialized;sprites=initialized_sprites;owner=initialized_owner;transition=t0;selection=sel0;gear=gear0;
        sync(c,owner);prepare(0x4dd2e0);guest_call.ecx=base;run_original32();
        check(frontend_transmission_suspend_4dd2e0(c,s),"native suspend");compare(c,owner);
    }
    std::cout<<"{\"routine\":\"frontend_transmission_probe\",\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}\n";return mismatches?1:0;
}

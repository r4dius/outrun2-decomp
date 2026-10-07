// Included inside the compare_pc dispatcher; uses its private x86 harness.
if(only=="frontend_vehicle_menu_probe"){
    using namespace outrun::platform;
    constexpr unsigned base=0x3a700000,root=base+0x2000,trace=base+0x4000,config=base+0x8000,device=base+0xa000;
    map_at(base,0x20000,PROT_READ|PROT_WRITE|PROT_EXEC);map_original_arithmetic_bridge(file);
    for(unsigned page:{0x7c2000u,0x68e000u,0x84a000u,0x84b000u,0x842000u})
        if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("vehicle menu globals");
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
            if((pc==0x48c170&&i==1)||(pc==0x48c290&&i==2))code.insert(code.end(),{0x25,255,0,0,0});
            code.insert(code.end(),{0x89,0x82});imm(trace+20+4*i);}
        code.insert(code.end(),{0xff,0x05});imm(trace);code.insert(code.end(),{0xc2,std::uint8_t(pop),0});install(pc,code);};
    for(unsigned pc:{0x48c3f0u,0x4c50d0u,0x4c5110u,0x48c450u,0x48d420u,0x48ca30u,0x48c5f0u})capture(pc,0,0);
    for(unsigned pc:{0x4c50f0u,0x4c5100u,0x48b130u,0x48b190u,0x48b150u,0x4249f0u})capture(pc,1,0);
    capture(0x48c170,2,8);capture(0x48c290,3,12);
    stub(0x4035f0,root);stub(0x407930,device);stub(0x440ea0,0,32);
    stub(0x465970,0);stub(0x4659f0,0);stub(0x4652e0,0);
    // Bounded GPU allocation fixture: preserve handle presence for the real
    // controller/carousel branches, without reading the native result.
    auto resource_stub=[&](bool configure){std::vector<std::uint8_t> code{0x89,0xca,0x81,0xea};
        auto imm=[&](unsigned v){for(unsigned j=0;j<4;++j)code.push_back(std::uint8_t(v>>(8*j)));};imm(base);
        code.insert(code.end(),{0xc7,0x82});imm(config);imm(configure?1:0);
        code.insert(code.end(),{0xc7,0x41,8});imm(configure?123:~0u);
        if(configure)for(unsigned i=0;i<11;++i){code.insert(code.end(),{0x8b,0x44,0x24,std::uint8_t(4+4*i),0x89,0x82});imm(config+4+4*i);}
        code.insert(code.end(),{0xc2,std::uint8_t(configure?44:0),0});install(configure?0x465860:0x465250,code);};
    resource_stub(false);resource_stub(true);
    unsigned cases{},mismatches{};const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
    auto check=[&](bool ok,const char* label){++cases;if(!ok){++mismatches;if(mismatches<15)std::cerr<<"vehicle menu "<<cases<<" "<<label<<"\n";}};
    struct Trace {std::vector<std::array<unsigned,4>> calls;} calls;
    auto ext=[](void* p,unsigned pc,Bytes,const unsigned* a,std::size_t n){std::array<unsigned,4> row{pc,0,0,0};for(unsigned i=0;i<n;++i)row[i+1]=a[i];static_cast<Trace*>(p)->calls.push_back(row);return true;};
    auto effect=[](void* p,unsigned id){static_cast<Trace*>(p)->calls.push_back({0x4249f0,id,0,0});return true;};
    auto sync=[&](FrontendVehicleMenu& c,const PcLicense& profile,const std::array<std::uint8_t,0xe00>& owner){
        std::memcpy(reinterpret_cast<void*>(base),c.object.data(),c.object.size());std::memcpy(reinterpret_cast<void*>(root),owner.data(),owner.size());
        std::memcpy(reinterpret_cast<void*>(0x7c23e0),profile.data(),profile.size());std::memcpy(reinterpret_cast<void*>(0x68eca8),c.colours_68eca8.data(),30);
        *reinterpret_cast<std::int8_t*>(0x84b0e8)=c.cursor_84b0e8;*reinterpret_cast<std::int8_t*>(0x84b0e9)=c.variant_84b0e9;
        *reinterpret_cast<std::uint8_t*>(0x84b0ea)=c.initialized_84b0ea;
        std::memset(reinterpret_cast<void*>(trace),0,0x3000);std::memset(reinterpret_cast<void*>(config),0,0x1800);calls.calls.clear();};
    auto compare=[&](FrontendVehicleMenu& c){
        Bytes b(c.object.data(),c.object.size()),g(reinterpret_cast<void*>(base),c.object.size());
        check(b.u32(4)==g.u32(4),"next child");
        check(!std::memcmp(c.object.data()+0x16c,reinterpret_cast<void*>(base+0x16c),0x74),"whole carousel tail");
        check(b.u8(0x15d0)==g.u8(0x15d0)&&b.u8(0x15d1)==g.u8(0x15d1),"dialog/colour latch");
        check(c.cursor_84b0e8==*reinterpret_cast<std::int8_t*>(0x84b0e8)&&c.variant_84b0e9==*reinterpret_cast<std::int8_t*>(0x84b0e9)&&c.initialized_84b0ea==bool(*reinterpret_cast<std::uint8_t*>(0x84b0ea)),"process cursor/variant/init");
        check(!std::memcmp(c.colours_68eca8.data(),reinterpret_cast<void*>(0x68eca8),30),"all retained colours");
        check(calls.calls.size()==*reinterpret_cast<unsigned*>(trace)&&!std::memcmp(calls.calls.data(),reinterpret_cast<void*>(trace+16),16*calls.calls.size()),"ordered preview/selection/sound calls");
        constexpr unsigned fields[]{0,0xc,0x10,0x14,0x18,0x40,0x44,0x64,0x68,0x3c,0x24};
        for(unsigned off:{0xccu,0x1530u,0x15d4u})if(*reinterpret_cast<unsigned*>(config+off)){
            const auto* a=reinterpret_cast<unsigned*>(config+off+4);
            for(unsigned i=0;i<11;++i)check(b.u32(off+fields[i])==(i==4&&a[i]==3?1:a[i]),"authored resource argument");
        }
    };
    constexpr unsigned masks[]{4,8,0x400,0x1000,0x800,0x2000,0x10,0};
    for(unsigned index=0;index<30;++index)for(unsigned unlock=0;unlock<3;++unlock)for(unsigned colour=0;colour<8;++colour){
        FrontendVehicleMenu c;std::array<std::uint8_t,0xe00> owner{};Bytes rb(owner.data(),owner.size());PcLicense profile{};
        profile.fill(unlock==1?255:0);if(unlock==2)for(unsigned i=0;i<18;++i)profile[0x28+i]=std::uint8_t(i*13+index);
        FrontendSprites sprites;sprites.bind_timing(0x44,std::vector<FrontendSpriteTiming>(224,{400,60}));sprites.bind_timing(0x2c,std::vector<FrontendSpriteTiming>(64,{52,60}));
        FrontendUiResources ui{sprites};ui.effect_user=&calls;ui.effect_4249f0=effect;PcUiNotifyGlobals globals;FrontendInputSnapshot input;unsigned repeat{},result{};
        ui.commands(0x442ac0,rb.sub(0x51c,owner.size()-0x51c),nullptr,0,globals,result);
        FrontendVehicleMenuServices s{ui,rb,globals,profile,input,repeat,3.5f,false,&calls,ext};
        frontend_vehicle_menu_construct_4c8de0(c,repeat);c.cursor_84b0e8=std::int8_t(index%15);c.variant_84b0e9=std::int8_t(index/15);c.colours_68eca8[index]=std::int8_t(colour);
        sync(c,profile,owner);*reinterpret_cast<float*>(0x842110)=3.5f;prepare(0x4c9010);guest_call.ecx=base;run_original32();
        check(frontend_vehicle_menu_init_4c9010(c,s),"native init");if(inject&&index==0&&unlock==0&&colour==0)c.colours_68eca8[0]^=1;compare(c);
        check((guest_call.out_eax&255)==1,"init result");
        const auto initialized=c;const auto initialized_sprites=sprites;const auto initialized_owner=owner;
        for(unsigned action=0;action<8;++action)for(unsigned held=0;held<4;++held){
            c=initialized;sprites=initialized_sprites;owner=initialized_owner;s.timer=4;repeat=action<7?action:12;
            input.feature_mask=masks[action];input.device_held=held&1?0x2000:0;s.colour_held_4536c0=(held&2)!=0;
            sync(c,profile,owner);*reinterpret_cast<unsigned*>(device+8)=input.device_held;*reinterpret_cast<float*>(0x842110)=4;
            stub(0x48f5f0,action<7?action:12,4);stub(0x4536c0,s.colour_held_4536c0);
            prepare(0x4c9290);guest_call.ecx=base;run_original32();const auto original_result=guest_call.out_eax;
            check(frontend_vehicle_menu_control_4c9290(c,s,result),"native control");compare(c);check(result==original_result,"owner action including overlay4");
            check(rb.u32(0x218)==*reinterpret_cast<unsigned*>(root+0x218)&&rb.u32(0x21c)==*reinterpret_cast<unsigned*>(root+0x21c),"parent depth pair");
        }
        for(unsigned dialog=0;dialog<2;++dialog){
            c=initialized;sprites=initialized_sprites;owner=initialized_owner;
            Bytes(c.object.data(),c.object.size()).put8(0x15d0,std::uint8_t(dialog));
            sync(c,profile,owner);prepare(0x4c8d70);guest_call.ecx=base;run_original32();
            check(frontend_vehicle_menu_display_4c8d70(c,s),"native display");compare(c);
            sync(c,profile,owner);prepare(0x4c8d90);guest_call.ecx=base;run_original32();
            check(frontend_vehicle_menu_suspend_4c8d90(c,s),"native suspend");compare(c);
            if(dialog)for(unsigned pressed:{0u,1u,12u}){
                c=initialized;sprites=initialized_sprites;owner=initialized_owner;
                Bytes(c.object.data(),c.object.size()).put8(0x15d0,1);
                input={};input.feature_mask=pressed==0?4:pressed==1?8:0;repeat=pressed;
                s.colour_held_4536c0=false;s.timer=4;
                sync(c,profile,owner);*reinterpret_cast<unsigned*>(device+8)=0;*reinterpret_cast<float*>(0x842110)=4;
                stub(0x48f5f0,pressed,4);stub(0x4536c0,0);
                prepare(0x4c9290);guest_call.ecx=base;run_original32();const auto original_result=guest_call.out_eax;
                check(frontend_vehicle_menu_control_4c9290(c,s,result),"native dialog control");compare(c);
                check(result==original_result,"dialog result");
            }
        }
    }
    std::cout<<"{\"routine\":\"frontend_vehicle_menu_probe\",\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}\n";return mismatches?1:0;
}

if(only=="scene_display_probe"){
    // Original 43FB40 (event display with the sun light colouring) and 4400C0
    // (layer-2 shadow displays) against pc_scene_display. The event slots,
    // flags, globals and the 4082B0(2,0,0) light are randomised; display
    // callbacks and the 410740/4107A0/405830 calls of 43FB40 are logging
    // stubs recording (id, argument, current slot 7A01D0, light lanes
    // +8..+0x38). Logs, light, phases, saved lanes and 7A01D0 are compared.
    using namespace outrun::platform;
    using outrun::driving::PcEventControlState;
    const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
    constexpr unsigned base=0x3a800000,works=0x3a900000,LogCount=base+0x10,ShadowByte=base+0x20,
        Stubs=base+0x100,Log=base+0x10000,LogCap=0x1000;
    map_at(base,0x60000,PROT_READ|PROT_WRITE|PROT_EXEC);map_at(works,0x20000,PROT_READ|PROT_WRITE);
    map_original_arithmetic_bridge(file);
    for(std::uint32_t page=0x62f000u;page<0x98d000u;page+=0x1000u)mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE);
    auto g32=[](std::uint32_t a){return *reinterpret_cast<const std::uint32_t*>(a);};
    auto p32=[](std::uint32_t a,std::uint32_t v){*reinterpret_cast<std::uint32_t*>(a)=v;};
    prepare(0x4082b0);{auto* a=reinterpret_cast<unsigned*>(S);a[0]=2;a[1]=0;a[2]=0;}run_original32();
    const std::uint32_t light_va=guest_call.out_eax;
    if(light_va!=0x899cd8u)throw std::runtime_error("4082B0(2,0,0) returned "+std::to_string(light_va));
    // Logging stubs: id, [esp+4] (or 0), [7A01D0], 12 dwords from light+8.
    auto stub=[&](unsigned at,std::uint32_t id,bool has_arg){
        fake_d3d9::Emit e;
        e.u8({0x56,0x57,0x51,0xa1});e.u32(LogCount);e.u8({0x3d});e.u32(LogCap);e.u8({0x73,0x00});
        const auto patch=e.b.size();
        e.u8({0x6b,0xc8,0x40,0x81,0xc1});e.u32(Log);e.u8({0xc7,0x01});e.u32(id);
        if(has_arg)e.u8({0x8b,0x54,0x24,0x10,0x89,0x51,0x04});else{e.u8({0xc7,0x41,0x04});e.u32(0);}
        e.u8({0x8b,0x15});e.u32(0x7a01d0);e.u8({0x89,0x51,0x08,0x8d,0x79,0x0c,0xbe});e.u32(light_va+8);
        e.u8({0x51,0xb9});e.u32(12);e.u8({0xf3,0xa5,0x59,0x40,0xa3});e.u32(LogCount);
        e.b[patch-1]=std::uint8_t(e.b.size()-patch);
        e.u8({0x59,0x5f,0x5e,0xc3});
        std::memcpy(reinterpret_cast<void*>(at),e.b.data(),e.b.size());return at;};
    constexpr unsigned CallbackCount=8;
    std::array<std::uint32_t,CallbackCount> callbacks{};
    for(unsigned k=0;k<CallbackCount;++k)callbacks[k]=stub(Stubs+k*0x80,Stubs+k*0x80,true);
    const unsigned reset_stub=stub(Stubs+0x800,0x410740,false),add_stub=stub(Stubs+0x880,0x4107a0,true),flush_stub=stub(Stubs+0x900,0x405830,false);
    for(unsigned site:{0x43fcbeu,0x43ff7bu,0x44000du})r046_write_rel32(site,0xe8,reset_stub);
    for(unsigned site:{0x43fcc4u,0x43ff85u,0x440017u})r046_write_rel32(site,0xe8,add_stub);
    for(unsigned site:{0x43ff76u,0x43ff9fu})r046_write_rel32(site,0xe8,flush_stub);
    struct Rec{std::uint32_t id,arg,slot;std::array<std::uint32_t,12> lanes;bool operator==(const Rec& o)const{return id==o.id&&arg==o.arg&&slot==o.slot&&lanes==o.lanes;}};
    struct Native{std::vector<Rec> log;std::array<std::uint8_t,0xa0> light{};PcEventControlState* state{};};
    Native nat;
    auto nrec=[](Native& n,std::uint32_t id,std::uint32_t arg){Rec r{id,arg,0x799b30u+n.state->current_slot*0x3cu,{}};
        std::memcpy(r.lanes.data(),n.light.data()+8,48);n.log.push_back(r);};
    PcSceneDisplayServices services{&nat,
        [](void* u,std::uint32_t cb,std::uint32_t work,std::uint32_t){auto& n=*static_cast<Native*>(u);
            Rec r{cb,work,0x799b30u+n.state->current_slot*0x3cu,{}};std::memcpy(r.lanes.data(),n.light.data()+8,48);n.log.push_back(r);},
        [](void*,std::uint32_t work){return *reinterpret_cast<const std::uint32_t*>(work+0xd10u);},
        [](void* u){auto& n=*static_cast<Native*>(u);Rec r{0x405830,0,0x799b30u+n.state->current_slot*0x3cu,{}};std::memcpy(r.lanes.data(),n.light.data()+8,48);n.log.push_back(r);},
        [](void* u){auto& n=*static_cast<Native*>(u);Rec r{0x410740,0,0x799b30u+n.state->current_slot*0x3cu,{}};std::memcpy(r.lanes.data(),n.light.data()+8,48);n.log.push_back(r);},
        [](void* u,std::uint8_t* l){auto& n=*static_cast<Native*>(u);Rec r{0x4107a0,l==n.light.data()?0x899cd8u:0u,0x799b30u+n.state->current_slot*0x3cu,{}};std::memcpy(r.lanes.data(),n.light.data()+8,48);n.log.push_back(r);}};
    (void)nrec;
    std::mt19937 rng(0x43fb40u);
    const unsigned cases=std::max(count,1u)*20000u;
    unsigned mismatches=0,checks=0,calls_seen=0,coloured=0;
    auto report=[&](const std::string& what){++mismatches;if(mismatches<=12)std::cerr<<"MISMATCH "<<what<<char(10);};
    auto rfloat=[&]()->std::uint32_t{
        switch(rng()%6u){
        case 0:return 0u;
        case 1:{float f=float(int(rng()%2000u)-1000)*0.01f;std::uint32_t u;std::memcpy(&u,&f,4);return u;}
        case 2:{float f=float(rng()%100000u)*0.37f;std::uint32_t u;std::memcpy(&u,&f,4);return u;}
        default:{float f=float(rng()%1000u)/1000.0f;std::uint32_t u;std::memcpy(&u,&f,4);return u;}}};
    for(unsigned c=0;c<cases;++c){
        PcEventControlState st{};nat.state=&st;nat.log.clear();
        const unsigned density=rng()%4u;
        for(unsigned i=0;i<outrun::driving::PcEventSlotCount;++i){
            auto& s=st.slots[i];
            if(rng()%4u>density){s={};continue;}
            static const std::uint8_t fl[]{0,1,2,2,2,3,4,6,0x0a,0x12,0x1a,0xff};
            s.flags=fl[rng()%sizeof(fl)];
            s.descriptor_token=rng();
            s.event_id=(rng()%8u)?i:(rng()%64u);
            s.work_token=works+(rng()%32u)*0x1000u;
            static const std::uint32_t masks[]{0x40,0x40,0x41,0x1ff,0x1,0x4,0x20,0x80,0x100,0};
            s.display_scene=(rng()%5u)?masks[rng()%10u]:rng();
            auto cb=[&]{return (rng()%6u)?callbacks[rng()%CallbackCount]:0u;};
            s.init_callback=rng();s.ctrl_callback=rng();s.disp_callback=cb();s.shadow_callback=cb();s.dest_callback=rng();
            s.aux24=rng();s.aux28=rng();s.close_guard=rng();s.function_id=rng();s.aux34=rng();s.aux38=rng();
        }
        // 79F5EC is the work pointer of slot 387 (0x183, the course event); 4AF5A0
        // reads bit 2 of its first byte.
        st.slots[387].work_token=ShadowByte;
        for(unsigned w=0;w<32;++w)p32(works+w*0x1000u+0xd10u,(rng()%4u)?rng()%7u:rng());
        for(unsigned i=0;i<outrun::driving::PcEventSlotCount;++i){
            const auto& s=st.slots[i];const unsigned r=0x799b30u+i*0x3cu;
            const std::uint32_t words[15]{s.descriptor_token,s.event_id,s.work_token,s.display_scene,s.init_callback,s.ctrl_callback,
                s.disp_callback,s.shadow_callback,s.dest_callback,s.aux24,s.aux28,s.close_guard,s.function_id,s.aux34,s.aux38};
            for(unsigned k=0;k<15;++k)p32(r+k*4,words[k]);
            *reinterpret_cast<std::uint8_t*>(0x79fb48u+i)=s.flags;
        }
        PcSceneDisplayGlobals g{};
        static const std::uint32_t modes[]{2,2,2,0,1,3,4,8};
        g.mode_780258=modes[rng()%8u];
        static const std::uint32_t scenes[]{2,6,0x14,0,1,7,0x13};
        g.scene_7f2428=scenes[rng()%7u];
        g.paused_780248=(rng()%3u)==0u?std::uint8_t(1u+rng()%255u):0u;
        g.object_7f9460_60=(rng()%3u)?rng():0u;
        g.shadow_79f5ec=std::uint8_t(rng());
        {std::uint32_t u=rfloat();std::memcpy(&g.phase_7a0db4,&u,4);u=rfloat();std::memcpy(&g.phase_7a0db8,&u,4);}
        for(auto& w:g.saved_7a0dbc)w=rng();
        for(unsigned k=0;k<0xa0;k+=4){std::uint32_t u=rfloat();std::memcpy(nat.light.data()+k,&u,4);}
        const std::uint32_t init_slot=rng()%outrun::driving::PcEventSlotCount;st.current_slot=init_slot;
        p32(0x780258,g.mode_780258);p32(0x7f2428,g.scene_7f2428);*reinterpret_cast<std::uint8_t*>(0x780248)=g.paused_780248;
        p32(0x7f9460+0x60,g.object_7f9460_60);*reinterpret_cast<std::uint8_t*>(ShadowByte)=g.shadow_79f5ec;
        {std::uint32_t u;std::memcpy(&u,&g.phase_7a0db4,4);p32(0x7a0db4,u);std::memcpy(&u,&g.phase_7a0db8,4);p32(0x7a0db8,u);}
        for(unsigned k=0;k<12;++k)p32(0x7a0dbc+k*4,g.saved_7a0dbc[k]);
        std::memcpy(reinterpret_cast<void*>(light_va),nat.light.data(),0xa0);
        p32(0x7a01d0,0x799b30u+init_slot*0x3cu);
        p32(LogCount,0);
        const bool shadow=(rng()%5u)==0u;
        std::uint32_t mask{};std::int32_t first{},last{};
        if(shadow){prepare(0x4400c0);run_original32();event_shadow_display_4400c0(st,g,services);}
        else{
            static const std::uint32_t mk[]{0x40,0x40,0x40,1,2,4,8,0x10,0x20,0x80,0x100};
            mask=(rng()%8u)?mk[rng()%11u]:rng();
            switch(rng()%4u){case 0:first=0;last=0x199;break;case 1:first=8;last=0x1f;break;
                case 2:first=std::int32_t(rng()%410u);last=std::int32_t(rng()%410u);break;default:first=std::int32_t(rng()%64u);last=first+std::int32_t(rng()%64u);}
            prepare(0x43fb40);{auto* a=reinterpret_cast<unsigned*>(S);a[0]=mask;a[1]=std::uint32_t(first);a[2]=std::uint32_t(last);}run_original32();
            event_display_43fb40(st,mask,first,last,nat.light.data(),g,services);
        }
        if(inject&&c==7u)nat.light[0x10]^=1u;
        const unsigned n=g32(LogCount);
        if(n>=LogCap)throw std::runtime_error("scene display log overflow");
        std::vector<Rec> gl(n);
        for(unsigned k=0;k<n;++k){const auto* p=reinterpret_cast<const std::uint32_t*>(Log+k*0x40u);gl[k].id=p[0];gl[k].arg=p[1];gl[k].slot=p[2];std::memcpy(gl[k].lanes.data(),p+3,48);}
        calls_seen+=n;for(const auto& r:gl)if(r.id==0x4107a0u)++coloured;
        const std::string tag=" case "+std::to_string(c)+(shadow?" 4400C0":" 43FB40");
        ++checks;if(gl.size()!=nat.log.size())report("log size guest "+std::to_string(gl.size())+" native "+std::to_string(nat.log.size())+tag);
        else for(std::size_t k=0;k<gl.size();++k){++checks;if(!(gl[k]==nat.log[k])){std::ostringstream o;o<<"log "<<k<<" id guest 0x"<<std::hex<<gl[k].id<<" native 0x"<<nat.log[k].id<<" arg "<<gl[k].arg<<"/"<<nat.log[k].arg<<" slot "<<gl[k].slot<<"/"<<nat.log[k].slot<<" l0 "<<gl[k].lanes[0]<<"/"<<nat.log[k].lanes[0]<<tag;report(o.str());break;}}
        ++checks;if(std::memcmp(reinterpret_cast<const void*>(light_va),nat.light.data(),0xa0))report("light"+tag);
        std::uint32_t u;std::memcpy(&u,&g.phase_7a0db4,4);++checks;if(g32(0x7a0db4)!=u)report("phase 7A0DB4"+tag);
        std::memcpy(&u,&g.phase_7a0db8,4);++checks;if(g32(0x7a0db8)!=u)report("phase 7A0DB8"+tag);
        for(unsigned k=0;k<12;++k){++checks;if(g32(0x7a0dbc+k*4)!=g.saved_7a0dbc[k]){report("saved lane word "+std::to_string(k)+tag);break;}}
        ++checks;if(g32(0x7a01d0)!=0x799b30u+st.current_slot*0x3cu)report("current slot"+tag);
    }
    std::cerr<<"scene display: checks="<<checks<<" guest calls="<<calls_seen<<" light installs="<<coloured<<char(10);
    std::cout<<"{\"routine\":\"scene_display_probe\",\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}"<<'\n';return 0;
}

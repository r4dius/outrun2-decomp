// Included inside the compare_pc dispatcher; uses its private x86 harness.
if(only=="race_sound_probe"){
    // Race sound manager (event 383 fn 0x17: 424650 init, 424700 control,
    // 424790 destroy) with every sound-manager callee, and COMM_TRANS 45A280
    // (event 360 fn 0x48), against outrun::platform::race_sound.
    // Foreign callees are capture stubs (entry, EAX, ECX, EDX, four stack
    // arguments, scripted EAX): 44DC50 44C940 44DD60 44DD90 44B7B0 44B800
    // 450570 48B310 48B350 48B1C0 43F9C0 55A930 and the audio layer 42EFF0 /
    // 42F0D0 / 427630 (ClearAllSound). Run original: all sound functions, the
    // protected SetIcsQueue entry 4247B0 (VM), CRT _ftol2 / _CIpow (x87 path) /
    // _isnan, the fsin/fsqrt/fabs and vector leaves, D3DXVec3Normalize through
    // the IAT (pinned d3dx9_29 generic path). Compared after every call: the
    // whole block 955AE0..956467, 95B208/95B20C, 98ABD0/98ABD4, 780024/780028,
    // the event works (must stay untouched), the ordered foreign calls with
    // their arguments and return values.
    using namespace outrun::platform;
    constexpr unsigned base=0x3ab00000,trace=base,values=base+0x8000,code=base+0x9000,works=base+0x10000,params=base+0x28000,
        scratch=base+0x2c000,tmp_eax=base+0x2f000;
    constexpr unsigned kWork=0x1000,kRec=48,kParams=0x1100,kBody=0x624,kDist=576;
    map_at(base,0x30000,PROT_READ|PROT_WRITE|PROT_EXEC);   // protected sections: mapped by the dispatcher (race_sound_probe listed there)
    for(unsigned page:{0x955000u,0x956000u,0x95b000u,0x98a000u,0x780000u,0x799000u,0x79a000u,0x79f000u,0x802000u,0x803000u,0x82e000u,0x7d6000u})
        if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("race sound globals");
    auto restore=[&](unsigned lo,unsigned hi){std::vector<std::uint8_t> b(hi-lo);for(unsigned a=lo;a<hi;++a)b[a-lo]=file[pe_file_offset(file,a)];
        r060_raw_write(lo,b.data(),b.size());};
    restore(0x424650,0x427a80);restore(0x449360,0x4493a0);restore(0x40efa0,0x40f050);restore(0x5811af,0x5811e0);restore(0x4c882e,0x4c8839);
    restore(0x45a280,0x45a2a2);
    struct Stub{unsigned pc;unsigned pop;std::vector<unsigned> args;bool eax,ecx,edx;};
    std::vector<Stub> stubs={
        {0x44dc50,0,{0},0,0,0},{0x44c940,0,{0},0,0,0},{0x44dd60,0,{0,1},0,0,0},{0x44dd90,0,{0,1},0,0,0},{0x44b7b0,0,{0},0,0,0},
        {0x44b800,0,{},0,0,0},{0x450570,0,{},0,0,0},{0x48b310,0,{},0,0,0},{0x48b350,0,{},0,0,0},{0x48b1c0,0,{},0,0,0},{0x43f9c0,0,{},0,0,0},
        {0x55a930,0,{},0,1,0},{0x42eff0,0,{},1,1,1},{0x42f0d0,0,{},1,0,0},{0x427630,0,{},0,0,0},{0x45a0c0,0,{},0,0,0}};
    std::map<unsigned,unsigned> slot;
    auto imm=[](std::vector<std::uint8_t>& c,unsigned v){for(unsigned j=0;j<4;++j)c.push_back(std::uint8_t(v>>(8*j)));};
    for(std::size_t k=0;k<stubs.size();++k){
        const auto& e=stubs[k];slot[e.pc]=unsigned(k);const unsigned vs=values+32*unsigned(k);
        std::vector<std::uint8_t> c{0x51,0x52,0xa3};imm(c,tmp_eax);                                   // push ecx; push edx; mov [tmp],eax
        c.push_back(0xa1);imm(c,trace);c.insert(c.end(),{0x6b,0xc0,std::uint8_t(kRec)});            // mov eax,[trace]; imul eax,48
        c.insert(c.end(),{0xc7,0x80});imm(c,trace+16);imm(c,e.pc);                                  // mov [eax+rec],pc
        c.insert(c.end(),{0x8b,0x0d});imm(c,tmp_eax);c.insert(c.end(),{0x89,0x88});imm(c,trace+20); // eax
        c.insert(c.end(),{0x8b,0x4c,0x24,0x04,0x89,0x88});imm(c,trace+24);                          // ecx
        c.insert(c.end(),{0x8b,0x0c,0x24,0x89,0x88});imm(c,trace+28);                               // edx
        for(unsigned i=0;i<4;++i){c.insert(c.end(),{0x8b,0x4c,0x24,std::uint8_t(12+4*i),0x89,0x88});imm(c,trace+32+4*i);}
        c.insert(c.end(),{0xff,0x05});imm(c,trace);c.push_back(0xa1);imm(c,vs);c.insert(c.end(),{0x5a,0x59});
        if(e.pop)c.insert(c.end(),{0xc2,std::uint8_t(e.pop),0});else c.push_back(0xc3);
        if(c.size()>160)throw std::runtime_error("race sound stub too large");
        const unsigned dst=code+160*unsigned(k);std::memcpy(reinterpret_cast<void*>(dst),c.data(),c.size());
        std::array<std::uint8_t,5> jump{{0xe9,0,0,0,0}};const auto delta=dst-e.pc-5;std::memcpy(jump.data()+1,&delta,4);r060_raw_write(e.pc,jump.data(),5);
    }
    auto g32=[](unsigned a){return *reinterpret_cast<std::uint32_t*>(a);};
    auto p32=[](unsigned a,std::uint32_t v){*reinterpret_cast<std::uint32_t*>(a)=v;};
    auto fb=[](float f){std::uint32_t u;std::memcpy(&u,&f,4);return u;};
    auto val=[&](unsigned pc){return g32(values+32*slot.at(pc));};
    auto setv=[&](unsigned pc,std::uint32_t v){p32(values+32*slot.at(pc),v);};
    auto gw=[&](unsigned id){return works+(id-8)*kWork;};

    // ---- native side --------------------------------------------------------------
    struct Env{RaceSoundState st;RaceSoundWorld w;std::array<std::uint8_t,0x180> flags{};
        std::vector<std::array<std::uint8_t,0x1000>> nw=std::vector<std::array<std::uint8_t,0x1000>>(24);
        std::vector<std::uint8_t> body=std::vector<std::uint8_t>(kBody),prm=std::vector<std::uint8_t>(kParams);std::vector<float> dist=std::vector<float>(kDist);};
    Env env;
    env.w.event_flags_79fb48=env.flags.data();
    for(unsigned id=8;id<32;++id)env.w.event_work[id]={gw(id),env.nw[id-8].data(),kWork};
    env.w.car_82e7f0={0x82e7f0u,env.body.data(),kBody};env.w.params={params,env.prm.data(),kParams};
    env.w.distance_802af0=env.dist.data();env.w.distance_count=kDist;
    struct NCall{unsigned pc,eax,ecx,edx;std::array<unsigned,4> a;};
    struct Probe final:RaceSoundServices{
        std::vector<NCall> calls;std::function<unsigned(unsigned)> v;
        unsigned rec(unsigned pc,std::initializer_list<unsigned> a,unsigned eax=0,unsigned ecx=0,unsigned edx=0){
            NCall c{pc,eax,ecx,edx,{}};unsigned i=0;for(auto x:a)c.a[i++]=x;calls.push_back(c);return v(pc);}
        std::int32_t pc_44dc50(std::uint32_t k)override{return std::int32_t(rec(0x44dc50,{k}));}
        std::uint32_t pc_44c940(std::uint32_t k)override{return rec(0x44c940,{k});}
        std::uint16_t pc_44dd60(std::uint32_t k,std::uint32_t s)override{return std::uint16_t(rec(0x44dd60,{k,s}));}
        std::uint16_t pc_44dd90(std::uint32_t k,std::uint32_t s)override{return std::uint16_t(rec(0x44dd90,{k,s}));}
        std::uint8_t pc_44b7b0(std::uint32_t a)override{return std::uint8_t(rec(0x44b7b0,{a}));}
        std::uint32_t pc_44b800()override{return rec(0x44b800,{});}
        std::uint32_t pc_450570()override{return rec(0x450570,{});}
        std::uint8_t pc_48b310()override{return std::uint8_t(rec(0x48b310,{}));}
        std::uint8_t pc_48b350()override{return std::uint8_t(rec(0x48b350,{}));}
        std::uint8_t pc_48b1c0()override{return std::uint8_t(rec(0x48b1c0,{}));}
        std::uint8_t pc_43f9c0()override{return std::uint8_t(rec(0x43f9c0,{}));}
        std::uint32_t pc_55a930()override{return rec(0x55a930,{},0,0x7f9460u);}
        void pc_42eff0(std::uint32_t ch,std::uint32_t value,std::uint32_t code)override{rec(0x42eff0,{},ch,value,code);}
        void pc_42f0d0(std::uint32_t c)override{rec(0x42f0d0,{},c);}
        void pc_427630()override{rec(0x427630,{});}
    };
    Probe services;services.v=[&](unsigned pc){return val(pc);};

    // ---- randomisation ----------------------------------------------------------
    std::mt19937 rng(std::getenv("OR2_RACE_SOUND_SEED")?std::uint32_t(std::strtoul(std::getenv("OR2_RACE_SOUND_SEED"),nullptr,0)):0x424700u);
    auto R=[&](unsigned n){return unsigned(rng()%n);};
    auto rf=[&](float lo,float hi){return lo+(hi-lo)*float(rng()%1000001u)/1000000.f;};
    const std::uint32_t surface_masks[]={0x2800,0x101000,0x8004,0x200010,0x8008,0x200,0x800000,0x2000000,0x80,0x400000,0xf00402,0x10};
    const std::uint32_t wall_masks[]={0x20,0x1000000,0x80000,0x20000,0x10000,0x40000,0x4};
    auto bits_from=[&](const std::uint32_t* m,unsigned n){std::uint32_t r=0;const unsigned k=R(3);for(unsigned i=0;i<k;++i)r|=m[R(n)];if(R(10)==0)r=rng();return r;};
    auto randomize_values=[&]{
        for(const auto& e:stubs)setv(e.pc,R(2));
        setv(0x44dc50,R(4)==0?(R(2)?0x11u:0x16u):(R(3)==0?std::uint32_t(0x3c+R(3)):R(64)));
        {const unsigned special[]={0,15,60,61,12,25,28,29,30,45,0x11,0x16};if(R(2))setv(0x44dc50,special[R(12)]);}
        setv(0x44c940,R(3)==0?0u:R(15));setv(0x44dd60,R(700));setv(0x44dd90,R(700));setv(0x450570,R(3)==0?0x4b0u+R(0x140):R(3000));
        setv(0x48b1c0,R(5)==0?rng():R(3));setv(0x43f9c0,R(6)==0?1u:0u);setv(0x55a930,R(3)==0?1u:0u);
        setv(0x42eff0,R(2)?0u:0x80004005u);setv(0x42f0d0,0);setv(0x44b7b0,R(3)==0?1u:0u);
    };
    auto rand_float=[&](float lo,float hi){if(R(40)==0){const float sp[]={0.f,-0.f,1.f,0.999f,0.2f,0.27f,0.01f,-1.f,8400.f,8500.f};return sp[R(10)];}return rf(lo,hi);};
    auto car_pc=[&]{return R(3)==0?0u:gw(9+R(23));};
    auto randomize_work=[&](unsigned id){
        auto* p=env.nw[id-8].data();for(unsigned b=0;b<kWork;++b)p[b]=std::uint8_t(rng());
        auto put=[&](unsigned o,std::uint32_t v){std::memcpy(p+o,&v,4);};auto putf=[&](unsigned o,float v){std::memcpy(p+o,&v,4);};
        auto put16=[&](unsigned o,std::uint16_t v){std::memcpy(p+o,&v,2);};
        put(0,R(24));p[4]=std::uint8_t(rng());p[0x13]=std::uint8_t(R(3));
        for(unsigned k=0;k<3;++k){putf(0x14+4*k,rand_float(-500.f,500.f));putf(0x20+4*k,rand_float(-6.f,6.f));}
        if(R(30)==0)putf(0x14,std::numeric_limits<float>::quiet_NaN());
        put(0x34,R(257));put(0x38,R(257));put(0x48,R(10)==0?rng():R(11000));put(0x5c,R(3)==0?1u:(R(2)?0u:2u));
        {const std::uint16_t pos[]={0x50,0x4f,0x96,0xbd,0xbe,0xe5,0xe6,0x11e,0x11f,0x1e6,0x1e7,4,5};put16(0x64,R(3)==0?pos[R(13)]:std::uint16_t(int(R(720))-10));}
        put(0x68,R(20));putf(0x1c4,rand_float(0.f,6.f));put(0x1cc,fb(rand_float(-60.f,60.f)));put(0x208,R(8));
        for(unsigned o:{0x24cu,0x250u,0x254u,0x258u})put(o,bits_from(surface_masks,12));
        put16(0x260,std::uint16_t(R(40)));p[0x281]=std::uint8_t(R(6));put(0x2a8,bits_from(wall_masks,7));put(0x2b4,params);
        putf(0x2c8,R(3)==0?rf(-1.f,1.f):0.f);putf(0x2f8,R(3)==0?rf(-1.f,1.f):0.f);
        put(0x2f0,R(2)?rng():(R(20)<<2)|2u);put(0x2f4,R(3)==0?(R(2)?0x70u:(R(2)?0x29u:0x2fu)):R(3));
        put(0x31c,R(8)==0?2u:R(2));put(0xd14,R(7));put(0xd9c,R(3)==0?R(40):0u);put(0xe68,fb(rand_float(-1.f,3.f)));
    };
    auto randomize_body=[&]{
        auto* p=env.body.data();for(auto& b:env.body)b=std::uint8_t(rng());
        auto putf=[&](unsigned o,float v){std::memcpy(p+o,&v,4);};
        for(unsigned o:{0x330u,0x338u,0x424u,0x42cu,0x518u,0x520u,0x60cu,0x614u})putf(o,rand_float(-2.f,2.f));
        for(unsigned o:{0x33cu,0x430u,0x524u,0x618u})putf(o,rand_float(0.9f,1.1f));
    };
    auto randomize_state=[&]{
        auto& s=env.st;for(auto& b:s.block)b=std::uint8_t(rng());
        for(std::uint32_t a=0x955ae0u;a<0x955c78u;a+=4)s.put32(a,R(4)==0?0x7fffu:(R(3)==0?0u:R(128)));
        s.put32(0x955c78u,R(88));s.put32(0x956140u,R(8)==0?s.u32(0x955c78u):R(88));
        if(R(4)==0)s.put32(0x956140u,(s.u32(0x955c78u)+87u)%88u);
        for(unsigned q=0;q<88;++q){s.put32(0x955ca0u+q*12,R(17));s.put32(0x955ca4u+q*12,R(6));}
        s.put32(0x955c7cu,R(2));s.put32(0x955c9cu,R(3)==0?R(0x47):0u);s.put32(0x9560c8u,rng());s.putf(0x9560ccu,rand_float(0.f,1.f));
        for(unsigned k=0;k<7;++k){s.putf(0x9560d0u+4*k,rf(0.f,1.5f));s.putf(0x9560f0u+4*k,rf(0.f,1.5f));}
        for(std::uint32_t a:{0x956114u,0x956118u,0x95611cu,0x956120u})s.put32(a,R(2));
        s.put32(0x9560c0u,R(32));s.put32(0x956124u,R(8)==0?s.u32(0x9560c0u):R(32));
        for(std::uint32_t a=0x9563e8u;a<0x956468u;a+=4)s.put32(a,R(3)==0?0xffffffffu:(R(2)?R(0x100):rng()));
        const std::uint32_t c=0x956148u;
        for(unsigned k=0;k<3;++k){s.putf(c+4*k,rand_float(-500.f,500.f));s.putf(c+0xc+4*k,rand_float(-300.f,300.f));}
        s.put32(c+0x18,R(2)?gw(8)+0x70u:gw(9+R(23))+0x70u);
        s.putf(c+0x1c,rand_float(0.f,11000.f));s.putf(c+0x20,float(R(257))/256.f);s.putf(c+0x28,rf(0.f,1.f));s.putf(c+0x30,rand_float(0.f,400.f));
        s.putf(c+0x34,rand_float(-60.f,60.f));s.put32(c+0x38,R(6));s.put32(c+0x3c,bits_from(wall_masks,7));s.put32(c+0x40,R(5));
        s.put32(c+0x44,R(3)==0?s.u32(c+0x40):R(5));s.put32(c+0x48,R(3));s.putf(c+0x4c,rand_float(-1.f,3.f));s.put32(c+0x50,R(2));
        for(unsigned o:{0x54u,0x6cu,0x84u,0x9cu})s.put32(c+o,bits_from(surface_masks,12));
        for(unsigned o:{0x60u,0x78u,0x90u,0xa8u})s.putf(c+o,rand_float(-2.f,2.f));
        for(unsigned o:{0x64u,0x7cu,0x94u,0xacu})s.putf(c+o,rand_float(0.95f,1.05f));
        s.put32(c+0xc8,R(3)==0?1u:0u);s.put32(c+0xcc,R(8));
        for(unsigned k=0;k<4;++k){const std::uint32_t sl=c+0xd0u+k*0x2cu;s.put32(sl,car_pc());
            for(unsigned j=0;j<3;++j){s.putf(sl+4+4*j,rand_float(-500.f,500.f));s.putf(sl+0x10+4*j,rand_float(-300.f,300.f));}
            s.putf(sl+0x1c,rand_float(0.f,30.f));s.putf(sl+0x20,rf(0.f,1.f));s.putf(sl+0x24,rand_float(0.f,11000.f));}
        for(unsigned k=0;k<24;++k)s.put32(c+0x230u+4*k,R(2));
        s.put32(c+0x290,R(2)?0xffffffffu:R(12));s.putf(c+0x294,rf(0.f,1.f));s.putf(c+0x298,rf(-1.f,1.f));s.putf(c+0x29c,rand_float(0.f,1.2f));
        s.put32(0x956214u,R(9));
        s.cheer_95b208=R(3)==0?0u:R(0x80);s.cheer_95b20c=R(3)==0?0u:R(0x80);s.counter_98abd0=rng();s.phase_98abd4=rf(-10.f,1000.f);
        s.surface_780024=R(3)?R(6):rng();s.surface_780028=R(3)?R(6):rng();
    };
    auto randomize_world=[&]{
        env.w.enabled_95b248=R(10)?1:0;const std::int32_t modes[]={0x10,0x10,0x10,0xd,0x13,0x18,0x3,0x12};env.w.mode_78026c=modes[R(8)];
        for(auto& f:env.flags)f=std::uint8_t(R(4)?(rng()&0xecu)|2u:rng());
        for(auto& d:env.dist)d=rand_float(0.f,60.f);
    };
    auto randomize_all=[&]{randomize_values();randomize_world();for(unsigned id=8;id<32;++id)randomize_work(id);randomize_body();randomize_state();};

    // ---- guest sync / comparison -------------------------------------------------
    auto sync=[&]{
        const auto& s=env.st;const auto& w=env.w;
        std::memcpy(reinterpret_cast<void*>(RaceSoundState::base),s.block.data(),s.block.size());
        p32(0x95b208,s.cheer_95b208);p32(0x95b20c,s.cheer_95b20c);p32(0x98abd0,s.counter_98abd0);p32(0x98abd4,fb(s.phase_98abd4));
        p32(0x780024,s.surface_780024);p32(0x780028,s.surface_780028);
        *reinterpret_cast<std::uint8_t*>(0x95b248)=w.enabled_95b248;p32(0x78026c,std::uint32_t(w.mode_78026c));
        std::memcpy(reinterpret_cast<void*>(0x79fb48),env.flags.data(),env.flags.size());
        for(unsigned id=8;id<32;++id){std::memcpy(reinterpret_cast<void*>(gw(id)),env.nw[id-8].data(),kWork);p32(0x799b38+id*0x3c,gw(id));}
        std::memcpy(reinterpret_cast<void*>(0x82e7f0),env.body.data(),kBody);std::memcpy(reinterpret_cast<void*>(params),env.prm.data(),kParams);
        std::memcpy(reinterpret_cast<void*>(0x802af0),env.dist.data(),kDist*4);
        p32(trace,0);services.calls.clear();
    };
    unsigned cases{},mismatches{};const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
    auto check=[&](bool ok,const std::string& label){++cases;if(!ok){++mismatches;if(mismatches<(std::getenv("OR2_RS_ALL")?100000u:25u))std::cerr<<"race sound case "<<cases<<": "<<label<<"\n";}};
    std::map<unsigned,unsigned> hits;
    struct Coverage{unsigned ics{},se{},frames{},inits{},paused{},engine{},road{},skid{},wall{},enemy{},passing{},cheer{},env{},comm{},comm_transfer{};} cov;
    auto compare=[&](const std::string& label){
        bool ok=true;std::ostringstream why;const auto& s=env.st;
        const auto* g=reinterpret_cast<const std::uint8_t*>(RaceSoundState::base);
        unsigned diffs=0;
        for(unsigned b=0;b<s.block.size();b+=4)if(std::memcmp(g+b,s.block.data()+b,4)){
            if(++diffs<=6)why<<" state@"<<std::hex<<RaceSoundState::base+b<<" guest "<<g32(RaceSoundState::base+b)<<" native "<<s.u32(RaceSoundState::base+b)<<std::dec;ok=false;}
        if(g32(0x95b208)!=s.cheer_95b208||g32(0x95b20c)!=s.cheer_95b20c){why<<" cheer "<<g32(0x95b208)<<"/"<<g32(0x95b20c)<<" native "<<s.cheer_95b208<<"/"<<s.cheer_95b20c;ok=false;}
        if(g32(0x98abd0)!=s.counter_98abd0||g32(0x98abd4)!=fb(s.phase_98abd4)){why<<" phase";ok=false;}
        if(g32(0x780024)!=s.surface_780024||g32(0x780028)!=s.surface_780028){why<<" surfaces";ok=false;}
        for(unsigned id=8;id<32;++id)if(std::memcmp(reinterpret_cast<const void*>(gw(id)),env.nw[id-8].data(),kWork)){why<<" work"<<id<<" written";ok=false;}
        const unsigned count=g32(trace);
        if(count!=services.calls.size()){why<<" calls guest "<<count<<" native "<<services.calls.size();ok=false;}
        for(unsigned i=0;i<std::min<unsigned>(count,unsigned(services.calls.size()));++i){
            const unsigned at=trace+16+kRec*i;const auto& c=services.calls[i];
            bool same=g32(at)==c.pc;
            if(same){const auto& e=stubs[slot.at(c.pc)];for(unsigned k:e.args)same&=g32(at+16+4*k)==c.a[k];
                if(e.eax)same&=g32(at+4)==c.eax;if(e.ecx)same&=g32(at+8)==c.ecx;if(e.edx)same&=g32(at+12)==c.edx;}
            if(!same){why<<" call"<<i<<" guest "<<std::hex<<g32(at)<<"(eax "<<g32(at+4)<<" ecx "<<g32(at+8)<<" edx "<<g32(at+12)<<" a0 "<<g32(at+16)<<") native "<<c.pc<<"(eax "<<c.eax<<" ecx "<<c.ecx<<" edx "<<c.edx<<" a0 "<<c.a[0]<<")"<<std::dec;ok=false;break;}
        }
        for(const auto& c:services.calls){++hits[c.pc];if(c.pc==0x42eff0)++cov.ics;if(c.pc==0x42f0d0)++cov.se;}
        check(ok,label+why.str());
        if(!ok)sync();
    };
    auto native=[&](const std::function<void()>& f,const std::string& label)->bool{
        try{f();return true;}catch(const std::exception& ex){check(false,label+" native threw "+ex.what());sync();return false;}};
    auto args=[&](std::initializer_list<unsigned> a){unsigned k=0;for(auto x:a)reinterpret_cast<unsigned*>(S)[k++]=x;};
    const std::uint32_t cs=0x956148u;

    // ---- 1. isolated functions on random states ------------------------------------
    for(unsigned i=0;i<40000;++i){
        randomize_all();sync();
        const unsigned e=gw(8);const unsigned which=i%26;std::string label="fn"+std::to_string(which);
        auto& s=env.st;auto& w=env.w;
        switch(which){
        case 0:{prepare(0x4279c0);run();if(!native([&]{race_sound_balance_4279c0(s,w,services);},"4279C0"))break;compare("4279C0");
            // 427B10 (music preview): option from its own 48B1C0 call, arg3 = held.
            sync();setv(0x48b1c0,R(3));const unsigned held=R(3)==0?0u:(R(2)?1u:rng());
            prepare(0x427b10);args({0u,0u,held});run();
            if(!native([&]{race_sound_preview_427b10(s,std::int8_t(services.pc_48b1c0()),held!=0u);},"427B10"))break;compare("427B10");break;}
        case 1:{const std::uint32_t ch=R(20);const std::int32_t v=R(4)==0?std::int32_t(rng()):std::int32_t(R(300))-50;
            prepare(0x427a20);guest_call.eax=ch;args({unsigned(v)});run();check(guest_call.out_eax==race_sound_volume_427a20(s,ch,v),"427A20 eax");break;}
        case 2:{const std::uint32_t ch=R(17),p=R(6),v=R(4)==0?rng():R(200);
            prepare(0x4247b0);guest_call.eax=v;guest_call.ebx=p;guest_call.edi=ch;run();
            if(!native([&]{race_sound_set_ics_4247b0(s,ch,p,v);},"4247B0"))break;compare("4247B0");break;}
        case 3:{prepare(0x424820);run();if(!native([&]{race_sound_flush_ics_424820(s,services);},"424820"))break;compare("424820");break;}
        case 4:{prepare(0x424880);run();race_sound_init_se_queue_424880(s);compare("424880");
            sync();prepare(0x424a00);run();race_sound_init_car_424a00(s);compare("424A00");break;}
        case 5:{const std::uint32_t cmd=R(2)?s.u32(0x9563e8u+4*R(32)):R(0x3000);prepare(0x424940);args({cmd});run();
            if(!native([&]{race_sound_set_se_424940(s,w,cmd);},"424940"))break;compare("424940");break;}
        case 6:{prepare(0x4249a0);run();if(!native([&]{race_sound_flush_se_4249a0(s,services);},"4249A0"))break;compare("4249A0");break;}
        case 7:{prepare(0x424b10);run();race_sound_pause_clear_424b10(s,services);compare("424B10");
            sync();prepare(0x427320);run();if(!native([&]{race_sound_voices_427320(s);},"427320"))break;compare("427320");break;}
        case 8:{const unsigned pick=R(4);std::uint32_t r=0;const unsigned pcs[]={0x424b50,0x424b90,0x424bf0,0x424c60};
            prepare(pcs[pick]);if(pick<2)args({e});else guest_call.eax=e;run();
            if(!native([&]{r=pick==0?race_sound_stage_424b50(w,services,e):pick==1?race_sound_stage_424b90(w,services,e):
                pick==2?race_sound_stage_424bf0(w,services,e):race_sound_stage_424c60(w,services,e);},"stage predicate"))break;
            check(guest_call.out_eax==r,"stage predicate eax");compare("stage predicate");break;}
        case 9:case 10:{prepare(0x424c80);run();if(!native([&]{race_sound_cheer_targets_424c80(s,w,services);},"424C80"))break;compare("424C80");break;}
        case 11:{prepare(0x424f80);run();if(!native([&]{race_sound_cheer_424f80(s,w,services);},"424F80"))break;++cov.cheer;compare("424F80");break;}
        case 12:{prepare(0x4256f0);guest_call.ecx=cs;guest_call.edx=e;guest_call.esi=0x82e7f0;run();
            if(!native([&]{race_sound_car_param_4256f0(s,w,cs,e);},"4256F0"))break;compare("4256F0");break;}
        case 13:{const std::uint32_t a=R(3),b=R(3);prepare(0x4252e0);guest_call.eax=cs;guest_call.ebx=a;args({b});run();
            if(!native([&]{race_sound_engine_4252e0(s,cs,a,b);},"4252E0"))break;++cov.engine;compare("4252E0");break;}
        case 14:{const std::uint32_t a=5,b=4;prepare(0x425a40);guest_call.eax=a;guest_call.ecx=cs;args({b});run();
            if(!native([&]{race_sound_road_425a40(s,cs,a,b);},"425A40"))break;++cov.road;compare("425A40");break;}
        case 15:{prepare(0x4260e0);guest_call.eax=3;guest_call.ecx=cs;run();
            if(!native([&]{race_sound_skid_4260e0(s,cs,3);},"4260E0"))break;++cov.skid;compare("4260E0");break;}
        case 16:{prepare(0x4264c0);guest_call.edi=6;args({cs});run();
            if(!native([&]{race_sound_wall_4264c0(s,cs,6);},"4264C0"))break;++cov.wall;compare("4264C0");break;}
        case 17:{const std::uint32_t pair=R(2);prepare(0x4267c0);args({cs,e,pair});run();
            if(!native([&]{race_sound_near_cars_4267c0(s,w,cs,e,pair);},"4267C0"))break;compare("4267C0");break;}
        case 18:{const std::uint32_t ch=10+R(2),sl=cs+0xd0u+0x2cu*R(4);const float range=R(3)?20.f:rf(1.f,40.f),gain=R(3)?1.f:rf(0.f,2.f);
            prepare(0x426b10);guest_call.eax=ch;args({cs,sl,fb(range),fb(gain)});run();
            if(!native([&]{race_sound_enemy_426b10(s,w,ch,cs,sl,range,gain);},"426B10"))break;++cov.enemy;compare("426B10");break;}
        case 19:{prepare(0x426f50);args({e,cs});run();if(!native([&]{race_sound_passing_426f50(s,w,services,e,cs);},"426F50"))break;
            for(const auto& c:services.calls)(void)c;++cov.passing;compare("426F50");break;}
        case 20:{prepare(0x425100);run();if(!native([&]{race_sound_car_425100(s,w,services);},"425100"))break;compare("425100");break;}
        case 21:{prepare(0x427110);run();if(!native([&]{race_sound_environment_427110(s,w,services);},"427110"))break;++cov.env;compare("427110");break;}
        case 22:{prepare(0x424650);run();if(!native([&]{race_sound_init_424650(s,w,services);},"424650"))break;++cov.inits;compare("424650");break;}
        case 23:case 24:{prepare(0x424700);run();if(!native([&]{race_sound_control_424700(s,w,services);},"424700"))break;
            if(val(0x43f9c0))++cov.paused;compare("424700");break;}
        default:{prepare(0x424790);run();race_sound_destroy_424790(s,services);compare("424790");
            // COMM_TRANS 45A280 on a poked manager / session
            const unsigned mgr=scratch,ses=scratch+0x100;CommTransWorld cw{true,R(4)?mgr:0u,R(4)?ses:0u,0u};
            const std::uint32_t states[]={0,7,1,2,3,6,8,0xffffffffu};cw.state_8=states[R(8)];
            p32(0x7d68ac,cw.manager_7d68ac);p32(mgr+0x58,cw.session_58);p32(ses+8,cw.state_8);p32(trace,0);
            prepare(0x45a280);run();const bool called=g32(trace)==1&&g32(trace+16)==0x45a0c0;
            const auto r=comm_trans_control_45a280(cw);++cov.comm;if(called)++cov.comm_transfer;
            check(called==(r==CommTransResult::Transfer45a0c0)&&g32(trace)<=1,"45A280 COMM_TRANS gate");break;}
        }
    }
    // ---- 2. engine volume over every accelerator step (both _CIpow calls) ----------
    for(unsigned k=0;k<=256;++k)for(unsigned rep=0;rep<8;++rep){
        randomize_all();env.st.putf(cs+0x20,float(k)/256.f);if(rep&1){env.st.putf(cs+0x30,20.f);env.st.putf(cs+0x1c,9000.f);env.st.put32(cs+0x44,env.st.u32(cs+0x40));}
        env.st.put32(0x955c78u,0);env.st.put32(0x956140u,0x57u);env.st.putf(0x9560ccu,rf(0.5f,1.f));
        for(std::uint32_t a=0x955ae0u;a<0x955c78u;a+=4)env.st.put32(a,0x7fffu);
        sync();prepare(0x4252e0);guest_call.eax=cs;guest_call.ebx=0;args({1});run();
        if(!native([&]{race_sound_engine_4252e0(env.st,cs,0,1);},"4252E0 accel sweep"))continue;++cov.engine;compare("4252E0 accel "+std::to_string(k));
    }
    // ---- 3. sequences: init, then control frames on an evolving race -------------------
    for(unsigned seq=0;seq<300;++seq){
        randomize_all();env.w.enabled_95b248=1;env.w.mode_78026c=R(4)?0x10:0xd;
        for(unsigned id=8;id<32;++id)env.flags[id]=std::uint8_t(id==8||R(2)?2u:(R(2)?1u:0u));
        env.flags[0x17f]=2;setv(0x43f9c0,0);sync();
        prepare(0x424650);run();if(!native([&]{race_sound_init_424650(env.st,env.w,services);},"seq init"))continue;++cov.inits;compare("seq init");
        auto* ew=env.nw[0].data();std::int32_t rpm=int(R(3000));std::int32_t acc=int(R(256));
        for(unsigned frame=0;frame<50;++frame){
            randomize_values();setv(0x43f9c0,R(25)==0?1u:0u);
            rpm=std::max(0,std::min(12000,rpm+int(R(800))-300));acc=std::max(0,std::min(256,acc+int(R(80))-40));
            std::memcpy(ew+0x48,&rpm,4);std::memcpy(ew+0x34,&acc,4);
            float sp=rf(0.f,6.f);std::memcpy(ew+0x1c4,&sp,4);
            for(unsigned k=0;k<3;++k){float v=rf(-500.f,500.f);std::memcpy(ew+0x14+4*k,&v,4);}
            if(R(4)==0)for(unsigned id=9;id<32;++id)randomize_work(id);
            if(R(10)==0)env.flags[9+R(23)]^=3u;
            sync();
            prepare(0x424700);run();if(!native([&]{race_sound_control_424700(env.st,env.w,services);},"seq control"))break;
            ++cov.frames;if(inject&&seq==299&&frame==49)env.st.put32(0x9560c8u,env.st.u32(0x9560c8u)^1u);
            compare("seq control frame "+std::to_string(frame));
        }
        sync();prepare(0x424790);run();race_sound_destroy_424790(env.st,services);compare("seq destroy");
    }
    unsigned never=0;for(const auto& e:stubs)if(!hits[e.pc]&&e.pc!=0x45a0c0){++never;std::cerr<<"callee never reached: "<<std::hex<<e.pc<<std::dec<<'\n';}
    std::cout<<"{\"routine\":\"race_sound_probe\",\"coverage\":{\"callees_reached\":"<<stubs.size()-1-never<<",\"callees\":"<<stubs.size()-1<<",\"inits\":"<<cov.inits
             <<",\"control_frames\":"<<cov.frames<<",\"ics_commands\":"<<cov.ics<<",\"se_commands\":"<<cov.se<<",\"paused_controls\":"<<cov.paused
             <<",\"comm_trans\":"<<cov.comm<<",\"comm_transfer\":"<<cov.comm_transfer<<"},\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}\n";
    return mismatches?1:0;
}

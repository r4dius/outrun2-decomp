// Included inside the compare_pc dispatcher; uses its private x86 harness.
if(only=="race_manager_probe"){
    // Race manager (event 359 fn 0): 450790 init, 4515B0 control, 44FE00
    // display, 44FE10 destroy, every manager-owned callee and the small state
    // accessors, against outrun::platform::race_manager.
    // Every foreign callee is a capture stub (entry, EAX, ECX, six stack
    // arguments; optional scripted writes through pointer arguments; scripted
    // EAX). The manager functions, 44C940/44C8D0 (stage records + cache
    // 635F2C), 449B30/449C10 and the protected route setter 451140 (VM) run
    // original. Harness call-site patches inside these ranges are reverted to
    // the EXE bytes first. Compared after every call: the whole block
    // 7D3650..7D39FF, the 24 event works (ids 8..31, 0x1000 bytes each), the
    // stage cache, the 7DE418 save block (0x36C, must stay untouched), 84490C, the ordered foreign
    // calls with their arguments, and return values.
    using namespace outrun::platform;
    constexpr unsigned base=0x3a900000,trace=base,values=base+0x8000,code=base+0x9000,course=base+0xe000,stages=base+0xf000,
        works=base+0x10000,scratch=base+0x28000;
    constexpr unsigned kWork=0x1000,kRec=48,kSave=0x36c;
    map_at(base,0x30000,PROT_READ|PROT_WRITE|PROT_EXEC);map_original_arithmetic_bridge(file);
    for(unsigned page:{0x7d2000u,0x7d3000u,0x799000u,0x79a000u,0x79f000u,0x780000u,0x7c2000u,0x637000u,0x680000u,0x80f000u,0x7f9000u,
                       0x7f1000u,0x635000u,0x7de000u,0x7dd000u,0x844000u})
        if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("race manager globals");
    auto restore=[&](unsigned lo,unsigned hi){std::vector<std::uint8_t> b(hi-lo);for(unsigned a=lo;a<hi;++a)b[a-lo]=file[pe_file_offset(file,a)];
        r060_raw_write(lo,b.data(),b.size());};
    restore(0x44fdf0,0x4517d0);restore(0x44dc70,0x44dd60);restore(0x44c8d0,0x44c980);restore(0x449b30,0x449c40);restore(0x44b9c0,0x44ba00);
    struct Stub{unsigned pc,nargs,pop;std::vector<std::array<unsigned,2>> out;std::vector<unsigned> compare;};
    auto all=[](unsigned n){std::vector<unsigned> v;for(unsigned i=0;i<n;++i)v.push_back(i);return v;};
    std::vector<Stub> stubs;
    for(unsigned pc:{0x46fab0u,0x47dac0u,0x47dc00u,0x47ec00u,0x44c2c0u,0x43f860u,0x48b1a0u,0x43f960u,0x456d60u,0x4f0dd0u,0x4f0e40u,0x4f2ac0u,
                     0x4f2df0u,0x55a930u,0x48b350u,0x46c500u,0x44bec0u,0x40a240u,0x40a010u,0x4957f0u,0x48b310u,0x495490u,0x44bdd0u,0x495b60u,
                     0x495b10u,0x495b70u,0x495b30u,0x4b00d0u,0x4b0190u,0x48b3a0u,0x495610u,0x45a0a0u,0x456d10u,0x49b2d0u,0x456d50u,0x456d40u,
                     0x458450u,0x467190u,0x4962a0u,0x4ef710u,0x465f20u,0x48b330u,0x48b340u,0x48b3d0u,0x467e00u})stubs.push_back({pc,0,0,{},{}});
    for(unsigned pc:{0x4f2b20u,0x45a2b0u,0x409f90u,0x44b900u,0x46c410u,0x46c400u,0x44b7b0u,0x476760u,0x456de0u,0x43d470u,0x453080u,0x44dc50u,0x456d20u})
        stubs.push_back({pc,1,0,{},all(1)});
    stubs.push_back({0x456720,1,4,{},all(1)});
    stubs.push_back({0x4b0110,2,0,{},all(2)});
    stubs.push_back({0x4aeef0,3,0,{},all(3)});
    stubs.push_back({0x40a7d0,2,0,{{0,3}},{1}});
    stubs.push_back({0x456dc0,3,0,{{1,1}},{0,2}});
    stubs.push_back({0x4a4440,5,0,{{3,1},{4,1}},{0,1,2}});
    std::map<unsigned,unsigned> slot;
    auto imm=[](std::vector<std::uint8_t>& c,unsigned v){for(unsigned j=0;j<4;++j)c.push_back(std::uint8_t(v>>(8*j)));};
    for(std::size_t k=0;k<stubs.size();++k){
        const auto& e=stubs[k];slot[e.pc]=unsigned(k);const unsigned vs=values+32*unsigned(k);
        std::vector<std::uint8_t> c{0x51,0x52,0x8b,0x15};imm(c,trace);                         // push ecx; push edx; mov edx,[trace]
        c.insert(c.end(),{0x6b,0xd2,std::uint8_t(kRec),0xc7,0x82});imm(c,trace+16);imm(c,e.pc);  // imul edx,48; mov [edx+rec],pc
        c.insert(c.end(),{0x89,0x82});imm(c,trace+20);c.insert(c.end(),{0x89,0x8a});imm(c,trace+24);
        for(unsigned i=0;i<6;++i){
            if(i<e.nargs)c.insert(c.end(),{0x8b,0x4c,0x24,std::uint8_t(12+4*i)});else c.insert(c.end(),{0x31,0xc9});
            c.insert(c.end(),{0x89,0x8a});imm(c,trace+28+4*i);}
        c.insert(c.end(),{0xff,0x05});imm(c,trace);                                            // inc [trace]
        unsigned voff=4;
        for(const auto& o:e.out){c.insert(c.end(),{0x8b,0x4c,0x24,std::uint8_t(12+4*o[0])});
            for(unsigned j=0;j<o[1];++j){c.push_back(0xa1);imm(c,vs+voff);c.insert(c.end(),{0x89,0x81});imm(c,4*j);voff+=4;}}
        c.push_back(0xa1);imm(c,vs);c.insert(c.end(),{0x5a,0x59});
        if(e.pop)c.insert(c.end(),{0xc2,std::uint8_t(e.pop),0});else c.push_back(0xc3);
        if(c.size()>160)throw std::runtime_error("race manager stub too large");
        const unsigned dst=code+160*unsigned(k);std::memcpy(reinterpret_cast<void*>(dst),c.data(),c.size());
        std::array<std::uint8_t,5> jump{{0xe9,0,0,0,0}};const auto delta=dst-e.pc-5;std::memcpy(jump.data()+1,&delta,4);r060_raw_write(e.pc,jump.data(),5);
    }
    auto g32=[](unsigned a){return *reinterpret_cast<std::uint32_t*>(a);};
    auto p32=[](unsigned a,std::uint32_t v){*reinterpret_cast<std::uint32_t*>(a)=v;};
    auto fb=[](float f){std::uint32_t u;std::memcpy(&u,&f,4);return u;};
    auto bf=[](std::uint32_t u){float f;std::memcpy(&f,&u,4);return f;};
    auto val=[&](unsigned pc,unsigned k=0){return g32(values+32*slot.at(pc)+4*k);};
    auto setv=[&](unsigned pc,std::uint32_t v,unsigned k=0){p32(values+32*slot.at(pc)+4*k,v);};
    auto gw=[&](unsigned id){return works+(id-8)*kWork;};

    // ---- native side --------------------------------------------------------------
    struct Env{RaceManagerState st;RaceManagerWorld w;std::vector<std::array<std::uint8_t,0x1000>> nw=std::vector<std::array<std::uint8_t,0x1000>>(24);
        std::vector<std::uint8_t> records=std::vector<std::uint8_t>(0x78*24);std::vector<std::int16_t> table=std::vector<std::int16_t>(225);
        std::vector<std::uint8_t> save=std::vector<std::uint8_t>(kSave);};
    Env env;
    for(unsigned id=8;id<32;++id)env.w.event_work_799b38[id]=env.nw[id-8].data();
    env.w.stage_records_7d33bc=env.records.data();env.w.time_table_7d2e98=env.table.data();
    std::uint8_t save_slot=0;std::uint32_t clock=0;   // guest-only: must stay untouched (451140 goes through 456D20)
    auto map_ptr=[&](const std::uint8_t* p)->unsigned{
        for(unsigned k=0;k<24;++k){const auto* b=env.nw[k].data();if(p>=b&&p<b+kWork)return gw(k+8)+unsigned(p-b);}
        return 0xdead0000u;};
    struct NCall{unsigned pc,ecx;std::array<unsigned,6> a;};
    struct Probe final:RaceManagerServices{
        std::vector<NCall> calls;std::function<unsigned(unsigned,unsigned)> v;std::function<unsigned(const std::uint8_t*)> map;
        unsigned rec(unsigned pc,std::initializer_list<unsigned> a,unsigned ecx=0){NCall c{pc,ecx,{}};unsigned i=0;for(auto x:a)c.a[i++]=x;calls.push_back(c);return v(pc,0);}
        static float f(unsigned u){float x;std::memcpy(&x,&u,4);return x;}
        void pc_46fab0()override{rec(0x46fab0,{});}
        void pc_47dac0()override{rec(0x47dac0,{});}
        void pc_47dc00()override{rec(0x47dc00,{});}
        void pc_47ec00()override{rec(0x47ec00,{});}
        std::uint32_t pc_44c2c0()override{return rec(0x44c2c0,{});}
        std::uint8_t pc_43f860()override{return std::uint8_t(rec(0x43f860,{}));}
        std::uint8_t pc_48b1a0()override{return std::uint8_t(rec(0x48b1a0,{}));}
        std::uint32_t pc_43f960()override{return rec(0x43f960,{});}
        std::uint8_t pc_456d60()override{return std::uint8_t(rec(0x456d60,{}));}
        void pc_4f0dd0()override{rec(0x4f0dd0,{});}
        void pc_4f0e40()override{rec(0x4f0e40,{});}
        void pc_456720(std::uint32_t p)override{rec(0x456720,{p},0x7de418);}
        void pc_4f2ac0()override{rec(0x4f2ac0,{});}
        void pc_4f2df0()override{rec(0x4f2df0,{});}
        void pc_4f2b20(std::int32_t s)override{rec(0x4f2b20,{unsigned(s)});}
        std::uint8_t pc_45a2b0(std::uint8_t c)override{return std::uint8_t(rec(0x45a2b0,{c}));}
        std::uint32_t pc_55a930()override{return rec(0x55a930,{},0x7f9460);}
        std::uint8_t pc_48b350()override{return std::uint8_t(rec(0x48b350,{}));}
        std::uint32_t pc_46c500()override{return rec(0x46c500,{});}
        std::uint32_t pc_44bec0()override{return rec(0x44bec0,{});}
        void pc_409f90(std::uint32_t m)override{rec(0x409f90,{m});}
        void pc_40a240()override{rec(0x40a240,{});}
        RaceVec3 pc_40a7d0(const std::uint8_t* p)override{rec(0x40a7d0,{0,map(p)});return {f(v(0x40a7d0,1)),f(v(0x40a7d0,2)),f(v(0x40a7d0,3))};}
        void pc_40a010()override{rec(0x40a010,{});}
        void pc_44b900(std::uint32_t s)override{rec(0x44b900,{s});}
        void pc_46c410(std::uint32_t s)override{rec(0x46c410,{s});}
        void pc_46c400(std::uint32_t s)override{rec(0x46c400,{s});}
        std::uint8_t pc_44b7b0(std::uint32_t a)override{return std::uint8_t(rec(0x44b7b0,{a}));}
        std::uint8_t pc_4957f0()override{return std::uint8_t(rec(0x4957f0,{}));}
        std::uint8_t pc_48b310()override{return std::uint8_t(rec(0x48b310,{}));}
        std::uint8_t pc_495490()override{return std::uint8_t(rec(0x495490,{}));}
        std::uint32_t pc_44bdd0()override{return rec(0x44bdd0,{});}
        void pc_495b60()override{rec(0x495b60,{});}
        std::uint32_t pc_495b10()override{return rec(0x495b10,{});}
        std::uint32_t pc_495b70()override{return rec(0x495b70,{});}
        std::uint32_t pc_495b30()override{return rec(0x495b30,{});}
        std::uint8_t pc_4b00d0()override{return std::uint8_t(rec(0x4b00d0,{}));}
        std::uint32_t pc_4b0190()override{return rec(0x4b0190,{});}
        void pc_4b0110(std::uint32_t k,std::uint32_t x)override{rec(0x4b0110,{k,x});}
        void pc_48b3a0()override{rec(0x48b3a0,{});}
        void pc_495610()override{rec(0x495610,{});}
        void pc_45a0a0()override{rec(0x45a0a0,{});}
        std::uint32_t pc_456d10()override{return rec(0x456d10,{});}
        void pc_456d20(std::uint32_t p)override{rec(0x456d20,{p});}
        void pc_476760(RaceEventWork w)override{rec(0x476760,{map(w)});}
        std::uint32_t pc_456dc0(std::uint32_t n,std::uint32_t& key,std::uint32_t fl)override{const auto r=rec(0x456dc0,{n,0,fl});key=v(0x456dc0,1);return r;}
        std::uint16_t pc_49b2d0()override{return std::uint16_t(rec(0x49b2d0,{}));}
        std::uint32_t pc_456d50()override{return rec(0x456d50,{});}
        std::int32_t pc_456d40()override{return std::int32_t(rec(0x456d40,{}));}
        std::uint32_t pc_456de0(std::int32_t t)override{return rec(0x456de0,{unsigned(t)});}
        std::uint16_t pc_43d470(std::uint32_t c)override{return std::uint16_t(rec(0x43d470,{c}));}
        void pc_4aeef0(std::uint32_t a,std::uint32_t l,float r)override{unsigned u;std::memcpy(&u,&r,4);rec(0x4aeef0,{a,l,u});}
        void pc_458450()override{rec(0x458450,{});}
        void pc_467190()override{rec(0x467190,{});}
        std::uint8_t pc_4962a0()override{return std::uint8_t(rec(0x4962a0,{}));}
        void pc_453080(std::uint32_t x)override{rec(0x453080,{x});}
        std::uint32_t pc_4ef710()override{return rec(0x4ef710,{});}
        void pc_465f20()override{rec(0x465f20,{});}
        std::int32_t pc_44dc50(std::uint32_t k)override{return std::int32_t(rec(0x44dc50,{k}));}
        std::uint32_t pc_48b330()override{return rec(0x48b330,{});}
        void pc_48b340()override{rec(0x48b340,{});}
        std::pair<float,float> pc_4a4440(std::uint32_t x,const std::uint8_t* a,const std::uint8_t* b)override{
            rec(0x4a4440,{x,map(a),map(b),0,0});return {f(v(0x4a4440,1)),f(v(0x4a4440,2))};}
        void pc_48b3d0()override{rec(0x48b3d0,{});}
        void pc_467e00()override{rec(0x467e00,{});}
    };
    Probe services;services.v=[&](unsigned pc,unsigned k){return val(pc,k);};services.map=map_ptr;

    // ---- randomisation ----------------------------------------------------------
    std::mt19937 rng(std::getenv("OR2_RACE_MANAGER_SEED")?std::uint32_t(std::strtoul(std::getenv("OR2_RACE_MANAGER_SEED"),nullptr,0)):0x450790u);
    auto R=[&](unsigned n){return unsigned(rng()%n);};
    auto rf=[&](float lo,float hi){return lo+(hi-lo)*float(rng()%1000001u)/1000000.f;};
    auto randomize_values=[&]{
        for(const auto& e:stubs)for(unsigned k=0;k<8;++k)setv(e.pc,R(2),k);
        for(unsigned pc:{0x43f860u,0x43f960u,0x55a930u,0x46c500u,0x48b350u,0x48b310u,0x4957f0u,0x495490u,0x4b00d0u,0x4962a0u,0x44b7b0u,0x48b330u,0x456de0u})
            setv(pc,R(3)==0?1u:0u);
        setv(0x48b350,R(2));setv(0x44b7b0,R(2));setv(0x48b1a0,R(3));
        setv(0x44c2c0,R(9)==0?15+R(3):R(15));setv(0x44bdd0,R(17));setv(0x45a2b0,R(8));setv(0x49b2d0,R(4)==0?0u:R(90));
        setv(0x456d60,R(4));setv(0x456d50,R(5)==0?2u:R(2));setv(0x456d40,std::uint32_t(int(R(5000))-100));
        setv(0x456dc0,R(2));setv(0x456dc0,R(17),1);setv(0x43d470,R(20)==0?0u:(R(4)==0?0xabcd0000u+R(800):R(800)));
        setv(0x44dc50,R(10)==0?0x3c+R(6):R(70));
        setv(0x4a4440,fb(rf(-20.f,20.f)),1);setv(0x4a4440,fb(rf(-20.f,20.f)),2);
        for(unsigned k=1;k<4;++k)setv(0x40a7d0,fb(rf(-10.f,10.f)),k);
        setv(0x4ef710,R(2)?4u:14u);setv(0x495b10,R(4));setv(0x495b70,R(10));setv(0x495b30,R(10));setv(0x4b0190,rng());
        setv(0x456d10,rng());setv(0x44bec0,0x7d3190);
    };
    auto randomize_world=[&]{
        auto& w=env.w;
        const unsigned variants[]={0,1,2,3,4,4,5,6,7,7,8,9,3,4};
        w.variant_780258=std::int32_t(variants[R(14)]);
        const unsigned modes[]={0x10,0x10,0x12,0x13,3,0x1b,0x10,0x0d};
        w.mode_78026c=std::int32_t(modes[R(8)]);
        w.difficulty_7c24c0=std::int32_t(R(5));w.player_events_680ad4=R(10)==0?std::int32_t(R(9)):std::int32_t(1+R(4));
        w.time_decrement_637911=R(6)?1:0;w.ranking_80fb14=R(3)==0?1u:0u;w.network_7f95a8=R(2);
        w.course_present_7d3188=true;w.course_10=R(6)==0?std::int32_t(1+R(5)):0;
        const std::int32_t sides[]={-1,-1,0,1,2};w.course_2c=sides[R(5)];w.course_30=sides[R(5)];w.course_14_5c=std::int16_t(R(700));
        w.area_7d33b0=R(3)==0?1u:0u;
        for(auto& t:env.table)t=std::int16_t(int(R(4000))-500);
        w.stage_count_7d33c4=R(15)==0?0:18;
        std::vector<unsigned> keys(18);for(unsigned k=0;k<18;++k)keys[k]=k;std::shuffle(keys.begin(),keys.end(),rng);
        for(unsigned k=0;k<24;++k){auto* r=env.records.data()+k*0x78;for(unsigned b=0;b<0x78;++b)r[b]=std::uint8_t(rng());
            const unsigned key=k<18?keys[k]:100+k,level=keys[k%18]<15&&R(4)?keys[k%18]:R(15);std::memcpy(r+4,&key,4);std::memcpy(r+8,&level,4);}
        w.stage_cache_key_635f2c=R(3)==0?0xffffffffu:R(18);w.stage_cache_value_635f30=R(15);
        for(auto& b:env.save)b=std::uint8_t(rng());
        for(unsigned s=0;s<8;++s){const unsigned n=R(16);std::memcpy(env.save.data()+s*0x6c+0x38,&n,4);}
        save_slot=std::uint8_t(R(4));clock=rng();w.flag_84490c=std::uint8_t(R(2));
        for(unsigned id=0;id<32;++id)w.event_open_79fb48[id]=std::uint8_t(R(4)?(rng()&0xfcu)|2u:rng());
    };
    auto randomize_work=[&](unsigned id){
        auto* p=env.nw[id-8].data();for(unsigned b=0;b<kWork;++b)p[b]=std::uint8_t(rng());
        auto put=[&](unsigned o,std::uint32_t v){std::memcpy(p+o,&v,4);};auto put16=[&](unsigned o,std::uint16_t v){std::memcpy(p+o,&v,2);};
        put(0,R(5)==0?8+R(24):id);p[4]=std::uint8_t(rng());p[0x10]=std::uint8_t(R(8));
        put(0x5c,R(3)==0?1u:(R(2)?0u:2u));
        const std::uint16_t positions[]={0x38,0x38,0x96,0x97,0};put16(0x64,R(4)==0?positions[R(5)]:std::uint16_t(R(700)));
        put(0x68,R(17));put(0x184,R(3)==0?1u:0u);put16(0x18c,R(3)==0?0x38:std::uint16_t(R(700)));
        const float speed=rf(0.f,400.f);std::memcpy(p+0x1c4,&speed,4);
        put16(0x25c,std::uint16_t(rng()&(R(2)?0x0d90u:0xffffu)));
        for(unsigned k=0;k<3;++k){const float c=rf(-50.f,50.f);std::memcpy(p+0xd28+4*k,&c,4);}
    };
    auto randomize_state=[&]{
        auto& s=env.st;auto* raw=reinterpret_cast<std::uint8_t*>(&s);for(unsigned b=0;b<sizeof(s);++b)raw[b]=std::uint8_t(rng());
        for(auto& v:s.sector_ms_7d3650)v=R(3)?-1:std::int32_t(R(600000));
        for(auto& v:s.sector_stage_ms_7d3748)v=R(3)?-1:std::int32_t(R(600000));
        for(auto& v:s.stage_speed_7d3838)v=rf(0.f,300.f);
        s.goal_first_stage_7d3740=R(2);s.sector_index_7d3874=std::int32_t(R(4));s.goal_ratio_7d3878=rf(0.f,1.f);
        s.split_ms_7d387c=std::int32_t(R(600000));s.time_adjust_7d3880=R(3)?0:std::int32_t(R(300));s.countdown_frames_7d3884=R(1000);
        s.average_speed_7d3888=rf(0.f,300.f);for(auto& v:s.rank_event_7d388c)v=8+R(4);
        for(auto& v:s.stage_flag_7d38ac)v=R(2);s.w7d38e4=R(2);s.branch_state_7d38e8=R(64);s.display_count_7d38ec=R(100000);
        s.over_state_7d38f0=R(8);for(auto& v:s.stage_key_7d38f4)v=R(3)?0xffffffffu:R(15);
        const std::uint32_t frames[]={0,1,0x57e3e,0x57e3f,0x57e40,0x80000000u};
        s.stage_frames_7d3930=R(8)==0?frames[R(6)]:R(20000);s.sector_banner_7d3934=R(4)==0?1u:(R(2)?0u:R(300));
        s.speed_sum_7d3938=rf(0.f,1e6f);s.sector_ms_7d393c=std::int32_t(R(600000));s.goal_key_7d3940=std::uint16_t(R(15));
        s.sector_level_7d3944=std::int32_t(R(15));s.last_stage_frames_7d3948=R(20000);
        s.time_7d394c=R(8)==0?std::int32_t(R(3)):std::int32_t(int(R(6000))-100);s.stage_clear_count_7d3950=R(5);
        for(auto& v:s.stage_split_7d3954)v=std::int32_t(R(600000));
        s.extend_count_7d3990=R(5);s.sector_new_7d3994=R(2);s.sector_set_7d3998=R(2);
        s.sector_zone_prev_7d399c=std::int8_t(R(4));s.sector_zone_7d399d=std::int8_t(R(4));
        for(auto& v:s.route_7d39a0)v=R(3);
        s.total_frames_7d39dc=R(8)==0?frames[R(6)]:R(200000);
        s.flags_7d39f0=R(4)==0?rng():R(128);s.sector_goal_7d39f4=R(2);s.stage_speed_latch_7d39f8=rf(0.f,300.f);
    };
    auto randomize_all=[&]{randomize_values();randomize_world();for(unsigned id=8;id<32;++id)randomize_work(id);randomize_state();};

    // ---- guest sync / comparison -------------------------------------------------
    auto sync=[&]{
        const auto& w=env.w;
        std::memcpy(reinterpret_cast<void*>(RaceManagerState::base),&env.st,sizeof(RaceManagerState));
        for(unsigned id=8;id<32;++id){std::memcpy(reinterpret_cast<void*>(gw(id)),env.nw[id-8].data(),kWork);p32(0x799b38+id*0x3c,gw(id));}
        for(unsigned id=0;id<32;++id)*reinterpret_cast<std::uint8_t*>(0x79fb48+id)=w.event_open_79fb48[id];
        p32(0x680ad4,std::uint32_t(w.player_events_680ad4));p32(0x780258,std::uint32_t(w.variant_780258));p32(0x78026c,std::uint32_t(w.mode_78026c));
        p32(0x7c24c0,std::uint32_t(w.difficulty_7c24c0));*reinterpret_cast<std::uint8_t*>(0x637911)=w.time_decrement_637911;
        p32(0x80fb14,w.ranking_80fb14);p32(0x7f95a8,w.network_7f95a8);
        p32(0x7d3188,w.course_present_7d3188?course:0u);p32(course+0x10,std::uint32_t(w.course_10));p32(course+0x14,course+0x100);
        *reinterpret_cast<std::int16_t*>(course+0x15c)=w.course_14_5c;p32(course+0x2c,std::uint32_t(w.course_2c));p32(course+0x30,std::uint32_t(w.course_30));
        p32(0x7d33b0,w.area_7d33b0);std::memcpy(reinterpret_cast<void*>(0x7d2e98),env.table.data(),450);
        std::memcpy(reinterpret_cast<void*>(stages),env.records.data(),env.records.size());p32(0x7d33bc,stages);p32(0x7d33c4,std::uint32_t(w.stage_count_7d33c4));
        p32(0x635f2c,w.stage_cache_key_635f2c);p32(0x635f30,w.stage_cache_value_635f30);
        std::memcpy(reinterpret_cast<void*>(0x7de418),env.save.data(),kSave);*reinterpret_cast<std::uint8_t*>(0x7dd138)=save_slot;
        p32(0x7f1938,clock);*reinterpret_cast<std::uint8_t*>(0x84490c)=w.flag_84490c;
        p32(trace,0);services.calls.clear();
    };
    unsigned cases{},mismatches{};const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
    auto check=[&](bool ok,const std::string& label){++cases;if(!ok){++mismatches;if(mismatches<25)std::cerr<<"race manager case "<<cases<<": "<<label<<"\n";}};
    std::map<unsigned,unsigned> hits;
    auto compare=[&](const std::string& label){
        bool ok=true;std::ostringstream why;
        const auto* g=reinterpret_cast<const std::uint8_t*>(RaceManagerState::base);const auto* n=reinterpret_cast<const std::uint8_t*>(&env.st);
        unsigned diffs=0;
        for(unsigned b=0;b<sizeof(RaceManagerState);b+=4)if(std::memcmp(g+b,n+b,4)){
            if(++diffs<=6)why<<" state@"<<std::hex<<RaceManagerState::base+b<<" guest "<<g32(RaceManagerState::base+b)<<" native "<<*reinterpret_cast<const std::uint32_t*>(n+b)<<std::dec;ok=false;}
        for(unsigned id=8;id<32;++id)if(std::memcmp(reinterpret_cast<const void*>(gw(id)),env.nw[id-8].data(),kWork)){
            for(unsigned b=0;b<kWork;++b)if(reinterpret_cast<const std::uint8_t*>(gw(id))[b]!=env.nw[id-8][b]){why<<" work"<<id<<"+"<<std::hex<<b<<std::dec;break;}ok=false;}
        if(g32(0x635f2c)!=env.w.stage_cache_key_635f2c||g32(0x635f30)!=env.w.stage_cache_value_635f30){why<<" stage cache";ok=false;}
        if(std::memcmp(reinterpret_cast<const void*>(0x7de418),env.save.data(),kSave)){why<<" save block";ok=false;}
        if(*reinterpret_cast<const std::uint8_t*>(0x84490c)!=env.w.flag_84490c){why<<" 84490C";ok=false;}
        const unsigned count=g32(trace);
        if(count!=services.calls.size()){why<<" calls guest "<<count<<" native "<<services.calls.size();ok=false;}
        for(unsigned i=0;i<std::min<unsigned>(count,unsigned(services.calls.size()));++i){
            const unsigned at=trace+16+kRec*i;const auto& c=services.calls[i];
            bool same=g32(at)==c.pc;
            if(same){const auto& e=stubs[slot.at(c.pc)];for(unsigned k:e.compare)same&=g32(at+12+4*k)==c.a[k];
                if(c.pc==0x55a930||c.pc==0x456720)same&=g32(at+8)==c.ecx;}
            if(!same){why<<" call"<<i<<" guest "<<std::hex<<g32(at)<<"("<<g32(at+12)<<","<<g32(at+16)<<","<<g32(at+20)<<") native "<<c.pc<<"("<<c.a[0]<<","<<c.a[1]<<","<<c.a[2]<<")"<<std::dec;ok=false;break;}
        }
        if(!ok&&mismatches<25){why<<" | guest:"<<std::hex;for(unsigned i=0;i<count&&i<40;++i)why<<" "<<g32(trace+16+kRec*i)<<"("<<g32(trace+28+kRec*i)<<")";
            why<<" | native:";for(const auto& c:services.calls)why<<" "<<c.pc<<"("<<c.a[0]<<")";why<<std::dec;}
        for(const auto& c:services.calls)++hits[c.pc];
        check(ok,label+why.str());
        if(!ok){sync();}   // resynchronise the guest from the native master
    };
    auto native=[&](const std::function<void()>& f,const std::string& label)->bool{
        try{f();return true;}catch(const std::exception& ex){check(false,label+" native threw "+ex.what());sync();return false;}};
    auto flip=[&](bool last){if(inject&&last)env.st.time_7d394c^=1;};
    struct Coverage{unsigned inits{},frames{},stage_clear{},goal{},timeup{},extend{},sectors{},route_set{},save_route{},ranking{},attack_restart{};} cov;

    // ---- 1. isolated functions on random states ------------------------------------
    for(unsigned i=0;i<24000;++i){
        randomize_all();sync();
        RaceEventWork e=env.w.work(8);const unsigned ge=gw(8);
        const unsigned which=i%20;std::string label="fn"+std::to_string(which);
        switch(which){
        case 0:{const std::int32_t variant=std::int32_t(R(9)),key=R(4)==0?std::int32_t(15+R(5)):std::int32_t(R(15)),diff=std::int32_t(R(5));
            prepare(0x44dc70);auto* a=reinterpret_cast<unsigned*>(S);a[0]=unsigned(variant);a[1]=unsigned(key);a[2]=unsigned(diff);run();
            std::int32_t r=0;if(!native([&]{r=race_extended_time_44dc70(env.w,services,variant,key,diff);},"44DC70"))break;
            check(guest_call.out_eax==unsigned(r),"44DC70 eax");compare("44DC70");break;}
        case 1:{prepare(0x44fe80);run();if(!native([&]{race_rank_table_44fe80(env.st,env.w,services);},"44FE80"))break;compare("44FE80");break;}
        case 2:{if(R(4)==0){env.w.course_present_7d3188=false;for(unsigned pc:{0x4957f0u,0x48b310u,0x495490u})setv(pc,0);sync();}
            prepare(0x4513c0);guest_call.esi=ge;run();if(!native([&]{race_check_goal_4513c0(env.st,env.w,services,e);},"4513C0"))break;compare("4513C0");break;}
        case 3:{env.st.branch_state_7d38e8=R(3)?2u|(R(2)?0x10u:0u):R(64);sync();
            prepare(0x44ff20);guest_call.esi=ge;run();bool r=false;if(!native([&]{r=race_check_stage_clear_44ff20(env.st,env.w,services,e);},"44FF20"))break;
            if(r)++cov.stage_clear;check((guest_call.out_eax&0xffu)==(r?1u:0u),"44FF20 al");compare("44FF20");break;}
        case 4:{env.st.branch_state_7d38e8=R(3)?8u:R(64);sync();
            prepare(0x451220);guest_call.edi=ge;run();if(!native([&]{race_check_branch_off_451220(env.st,env.w,services,e);},"451220"))break;compare("451220");break;}
        case 5:{env.st.branch_state_7d38e8=R(3)?8u:R(64);sync();const unsigned id=9+R(23);
            prepare(0x4512c0);reinterpret_cast<unsigned*>(S)[0]=gw(id);run();if(!native([&]{race_check_branch_off_4512c0(env.st,env.w,services,env.w.work(id));},"4512C0"))break;compare("4512C0");break;}
        case 6:{if(R(2))env.st.flags_7d39f0|=8u;sync();
            prepare(0x450cc0);guest_call.eax=ge;run();if(!native([&]{race_check_time_extend_450cc0(env.st,env.w,services,e);},"450CC0"))break;
            if(env.st.flags_7d39f0&8u)++cov.extend;compare("450CC0");break;}
        case 7:{prepare(0x450ac0);run();if(!native([&]{race_check_game_timer_450ac0(env.st,env.w,services);},"450AC0"))break;
            if(env.st.flags_7d39f0&1u)++cov.timeup;compare("450AC0");break;}
        case 8:{prepare(0x450170);run();if(!native([&]{race_check_game_over_450170(env.st,env.w,services);},"450170"))break;compare("450170");break;}
        case 9:case 10:{for(unsigned k=0;k<4;++k){const unsigned lvl=R(15);for(unsigned s2=0;s2<4;++s2)env.st.sector_ms_7d3650[lvl*4+s2]=-1;}sync();
            prepare(0x450e10);guest_call.eax=ge;run();if(!native([&]{race_check_sector_time_450e10(env.st,env.w,services,e);},"450E10"))break;
            if(env.st.sector_set_7d3998)++cov.sectors;compare("450E10");break;}
        case 11:{prepare(0x4506b0);guest_call.esi=ge;run();float r=0;if(!native([&]{r=race_sector_progress_4506b0(services,e);},"4506B0"))break;
            check(guest_call.out_xmm0==fb(r),"4506B0 xmm0");compare("4506B0");break;}
        case 12:{const std::uint32_t frames=R(6)==0?rng():R(0x57e40);float frac=rf(-2.f,2.f);if(R(8)==0)frac=float(int(R(5))-2);
            const unsigned out=scratch;std::memset(reinterpret_cast<void*>(out),0x5a,16);
            prepare(0x449b30);auto* a=reinterpret_cast<unsigned*>(S);a[0]=out;a[1]=out+4;a[2]=out+8;a[3]=out+12;a[4]=frames;a[5]=fb(frac);run();
            const auto t=race_frames_to_time_449b30(frames,frac);auto h16=[&](unsigned x){return *reinterpret_cast<std::uint16_t*>(x);};
            check(h16(out)==t.hours&&h16(out+4)==t.minutes&&h16(out+8)==t.seconds&&h16(out+12)==t.ms,"449B30 words");
            const std::uint16_t hh=std::uint16_t(R(3)),mm=std::uint16_t(R(60)),ss=std::uint16_t(R(60)),ms=std::uint16_t(R(1000));
            prepare(0x449c10);a[0]=hh|0xabcd0000u;a[1]=mm|0x12340000u;a[2]=ss;a[3]=ms;run();check(guest_call.out_eax==unsigned(race_time_ms_449c10(hh,mm,ss,ms)),"449C10");break;}
        case 13:{prepare(0x4503d0);run();check(guest_call.out_eax==race_pack_route_4503d0(env.st),"4503D0");
            const std::uint32_t p=rng();prepare(0x450490);reinterpret_cast<unsigned*>(S)[0]=p;run();race_unpack_route_450490(env.st,p);compare("450490");break;}
        case 14:{const std::int32_t lvl=R(6)==0?std::int32_t(14+R(3)):std::int32_t(R(14));const unsigned v=R(3);
            prepare(0x451140);auto* a=reinterpret_cast<unsigned*>(S);a[0]=unsigned(lvl);a[1]=v;run();
            if(!native([&]{race_set_route_451140(env.st,env.w,services,lvl,v);},"451140"))break;if(env.w.variant_780258==4&&lvl<14)++cov.save_route;compare("451140");break;}
        case 15:{const std::int32_t lvl=R(6)==0?std::int32_t(14+R(3)):std::int32_t(R(14));
            prepare(0x451350);reinterpret_cast<unsigned*>(S)[0]=unsigned(lvl);run();std::uint32_t r=0;
            if(!native([&]{r=race_get_route_451350(env.st,env.w,services,lvl);},"451350"))break;check(guest_call.out_eax==r,"451350 eax");compare("451350");break;}
        case 16:{prepare(0x456cf0);run();race_store_route_456cf0(env.st,services);compare("456CF0");break;}
        case 17:{const std::uint32_t key=R(4)==0?rng():R(20);prepare(0x44c940);reinterpret_cast<unsigned*>(S)[0]=key;run();
            check(guest_call.out_eax==race_stage_level_44c940(env.w,key),"44C940 eax");compare("44C940");break;}
        case 18:{prepare(0x44fe00);run();race_manager_display_44fe00(env.st);compare("44FE00");
            sync();prepare(0x44fe10);run();race_manager_destroy_44fe10(env.st,services);compare("44FE10");break;}
        default:{prepare(0x450790);run();if(!native([&]{race_manager_init_450790(env.st,env.w,services);},"450790"))break;++cov.inits;compare("450790");
            sync();prepare(0x4515b0);run();if(!native([&]{race_manager_control_4515b0(env.st,env.w,services);},"4515B0"))break;
            flip(i==23999);compare("4515B0 on random world");break;}
        }
    }

    // ---- 2. sequences: init, then control frames on an evolving race -------------------
    for(unsigned seq=0;seq<400;++seq){
        randomize_all();env.w.player_events_680ad4=std::int32_t(1+R(4));env.w.course_10=0;
        for(unsigned id=0;id<32;++id)env.w.event_open_79fb48[id]=std::uint8_t(R(5)?2u:0u);
        if(R(2))env.w.mode_78026c=0x10;
        const unsigned start_key=R(15);setv(0x49b2d0,90);setv(0x44b7b0,0);setv(0x44c2c0,start_key);sync();
        prepare(0x450790);run();if(!native([&]{race_manager_init_450790(env.st,env.w,services);},"seq init"))continue;++cov.inits;compare("seq init");
        auto* ew=env.nw[0].data();
        std::uint16_t pos=std::uint16_t(R(50));std::uint32_t lap=0;
        for(unsigned frame=0;frame<60;++frame){
            randomize_values();
            setv(0x49b2d0,frame<3?90u-frame:0u);setv(0x44c2c0,start_key);
            setv(0x44b7b0,R(12)==0?1u:0u);setv(0x456d60,R(3)==0?0u:2u);
            // car advances; lamps / branch bits change from time to time
            pos=std::uint16_t(pos+R(30));if(pos>650){pos=std::uint16_t(R(40));++lap;}
            std::memcpy(ew+0x64,&pos,2);const std::uint32_t five_c=R(10)==0?1u:0u;std::memcpy(ew+0x5c,&five_c,4);
            const std::uint16_t lamp=std::uint16_t(R(4)==0?(R(2)?0x10u:0x100u)|(R(2)?0x0400u:0x0800u)|(R(2)?0x80u:0u):R(0x100));std::memcpy(ew+0x25c,&lamp,2);
            const float speed=rf(0.f,350.f);std::memcpy(ew+0x1c4,&speed,4);
            if(R(8)==0)env.st.branch_state_7d38e8|=1u<<R(6);
            if(R(20)==0)env.st.time_7d394c=std::int32_t(R(3));
            sync();
            const bool last=seq==399&&frame==59;
            prepare(0x4515b0);run();if(!native([&]{race_manager_control_4515b0(env.st,env.w,services);},"seq control"))break;
            ++cov.frames;if(env.st.flags_7d39f0&2u)++cov.stage_clear;if(env.st.flags_7d39f0&4u)++cov.goal;if(env.st.flags_7d39f0&1u)++cov.timeup;
            if(env.st.sector_set_7d3998)++cov.sectors;if(env.w.ranking_80fb14)++cov.ranking;
            for(const auto& c:services.calls){if(c.pc==0x467e00)++cov.attack_restart;}
            flip(last);compare("seq control frame "+std::to_string(frame));
            if(R(3)==0){sync();prepare(0x44fe00);run();race_manager_display_44fe00(env.st);compare("seq display");}
        }
        sync();prepare(0x44fe10);run();race_manager_destroy_44fe10(env.st,services);compare("seq destroy");
    }

    // ---- 3. accessors -------------------------------------------------------------------
    for(unsigned i=0;i<6000;++i){
        randomize_all();sync();const std::uint32_t arg=R(3)==0?rng():R(15),arg2=R(5);
        auto call=[&](unsigned pc,std::initializer_list<unsigned> a){prepare(pc);unsigned k=0;for(auto x:a)reinterpret_cast<unsigned*>(S)[k++]=x;guest_call.st0=(pc==0x4505e0||pc==0x4505f0)?1u:0u;run();return guest_call.out_eax;};
        auto eq=[&](bool ok,const char* what){check(ok,what);};
        switch(i%30){
        case 0:eq(call(0x44fdf0,{})==race_countdown_frames_44fdf0(env.st),"44FDF0");break;
        case 1:call(0x44fe30,{arg});race_set_time_44fe30(env.st,std::int32_t(arg));compare("44FE30");break;
        case 2:eq(call(0x44fe40,{})==std::uint32_t(race_time_44fe40(env.st)),"44FE40");break;
        case 3:call(0x44fe50,{arg});race_set_flag0_44fe50(env.st,arg);compare("44FE50");break;
        case 4:eq(call(0x44fe70,{})==race_flag0_44fe70(env.st),"44FE70");break;
        case 5:call(0x44fef0,{arg});race_set_flag2_44fef0(env.st,arg);compare("44FEF0");break;
        case 6:eq(call(0x44ff10,{})==race_flag2_44ff10(env.st),"44FF10");break;
        case 7:call(0x450110,{arg});race_set_flag1_450110(env.st,arg);compare("450110");break;
        case 8:eq(call(0x450130,{})==race_flag1_450130(env.st),"450130");break;
        case 9:call(0x450140,{arg});race_set_flag3_450140(env.st,arg);compare("450140");break;
        case 10:eq(call(0x450160,{})==race_flag3_450160(env.st),"450160");break;
        case 11:call(0x450230,{arg});race_set_over_state_450230(env.st,arg);compare("450230");eq(call(0x450240,{})==race_over_state_450240(env.st),"450240");break;
        case 12:{const std::uint16_t preset=std::uint16_t(R(5));p32(0x78024c,0xabcd0000u|preset);const std::int32_t key=std::int32_t(R(20)),br=std::int32_t(R(3));
            eq(call(0x450250,{unsigned(key),unsigned(br)})==unsigned(race_next_stage_key_450250(env.w,preset,key,br)),"450250");compare("450250");break;}
        case 13:eq(call(0x4502c0,{})==race_flag5_4502c0(env.st),"4502C0");call(0x4502d0,{});race_clear_flag5_4502d0(env.st);compare("4502D0");break;
        case 14:call(0x4502e0,{arg2});race_request_lamp_4502e0(env.st,std::int32_t(arg2));compare("4502E0");break;
        case 15:eq(call(0x450300,{})==race_flag6_450300(env.st),"450300");call(0x450310,{});race_clear_flag6_450310(env.st);compare("450310");break;
        case 16:{const auto r=call(0x450320,{});eq(r==race_route_number_450320(env.st,services),"450320");compare("450320");break;}
        case 17:{const unsigned id=8+R(24);eq(call(0x450380,{id})==race_event_stage_key_450380(env.w,id),"450380");
            eq((call(0x4503a0,{id})&0xffffu)==race_event_position_4503a0(env.w,id),"4503A0");eq(call(0x4503c0,{})==race_leader_event_4503c0(env.st),"4503C0");break;}
        case 18:{const std::int32_t lvl=std::int32_t(R(21));eq(call(0x450560,{unsigned(lvl)})==race_stage_key_450560(env.st,lvl),"450560 (protected)");break;}
        case 19:eq(call(0x450570,{})==race_stage_frames_450570(env.st),"450570");eq(call(0x450580,{})==unsigned(race_split_450580(env.st)),"450580");
            eq(call(0x450590,{})==unsigned(race_sector_ms_450590(env.st)),"450590");break;
        case 20:{const std::int32_t lvl=std::int32_t(R(15));eq(call(0x4505a0,{unsigned(lvl)})==unsigned(race_stage_split_4505a0(env.st,lvl)),"4505A0");
            eq(call(0x4505b0,{})==race_sector_new_4505b0(env.st),"4505B0");eq(call(0x4505c0,{})==race_sector_set_4505c0(env.st),"4505C0");
            eq(call(0x4505d0,{})==race_total_frames_4505d0(env.st),"4505D0");break;}
        case 21:{call(0x4505e0,{});eq(guest_call.out_st0==fb(race_average_speed_4505e0(env.st)),"4505E0");
            const std::int32_t lvl=std::int32_t(R(15));call(0x4505f0,{unsigned(lvl)});eq(guest_call.out_st0==fb(race_stage_speed_4505f0(env.st,lvl)),"4505F0");break;}
        case 22:call(0x450600,{arg});race_set_time_adjust_450600(env.st,std::int32_t(arg));compare("450600");break;
        case 23:{const std::int32_t l=std::int32_t(R(15)),s2=std::int32_t(R(4));
            eq(call(0x450610,{unsigned(l),unsigned(s2)})==unsigned(race_sector_time_450610(env.st,l,s2)),"450610");
            eq(call(0x450630,{unsigned(l),unsigned(s2)})==unsigned(race_sector_stage_time_450630(env.st,l,s2)),"450630");break;}
        case 24:{p32(scratch,0x11111111);p32(scratch+4,0x22222222);call(0x450650,{scratch,scratch+4});std::int32_t l=0,s2=0;race_latest_sector_450650(env.st,l,s2);
            eq(g32(scratch)==unsigned(l)&&g32(scratch+4)==unsigned(s2),"450650");break;}
        case 25:eq(call(0x450670,{})==race_sector_banner_450670(env.st),"450670");eq(call(0x450680,{})==race_display_count_450680(env.st),"450680");
            eq(call(0x450690,{})==race_sector_goal_450690(env.st),"450690");eq(call(0x4506a0,{arg2})==race_stage_flag_4506a0(env.st,std::int32_t(arg2)),"4506A0");break;
        case 26:{const std::int32_t lvl=R(2)?std::int32_t(R(3)?14:4):std::int32_t(R(20));const auto r=call(0x450750,{unsigned(lvl)});
            eq(r==race_is_last_level_450750(services,lvl),"450750");compare("450750");break;}
        case 27:{const auto r=call(0x450780,{});eq(r==race_last_level_450780(services),"450780");compare("450780");break;}
        case 28:{const unsigned req=R(3)?1u:0u;const auto r=call(0x451180,{req});eq(r==race_time_451180(env.st,services,env.w.work(8),req),"451180");break;}
        default:{std::int32_t a=std::int32_t(rng());p32(0x7d2e80,unsigned(a));const unsigned pc=0x44b9c0+0x10*R(4);call(pc,{});
            if(pc==0x44b9c0)race_area_state_44b9c0(a);else if(pc==0x44b9d0)race_area_state_44b9d0(a);else if(pc==0x44b9e0)race_area_state_44b9e0(a);else race_area_state_44b9f0(a);
            eq(g32(0x7d2e80)==unsigned(a),"44B9C0..44B9F0");break;}
        }
    }
    // ---- 5. optional cross-architecture replay dump (OR2_RACE_MANAGER_REPLAY=<file>) --------
    // Original outputs of the float-sensitive paths (449B30 x87 extended +
    // truncation, 4506B0 SSE with inf/NaN/denormal operands, 450AC0 x87 ratio)
    // for tests/test_race_manager_replay.cpp on non-x86 hosts (qemu-aarch64).
    if(const char* dump=std::getenv("OR2_RACE_MANAGER_REPLAY")){
        std::vector<std::uint32_t> t1,t2,t3;
        const std::uint32_t special[]={0x00000000u,0x80000000u,0x7f800000u,0xff800000u,0x7fc00000u,0xffc00000u,0x7fa00000u,0x00000001u,0x80000001u,
            0x3f800000u,0xbf800000u,0x40000000u,0xc0000000u,0x3effffffu,0x3f7fffffu,0x4b000000u,0x5f000000u,0xdf000000u,0x7f7fffffu};
        auto pick=[&]()->std::uint32_t{if(R(4)==0)return special[R(sizeof special/4)];if(R(3)==0)return std::uint32_t(rng());return fb(rf(-2.f,2.f));};
        for(unsigned i=0;i<60000;++i){
            const std::uint32_t frames=R(3)==0?rng():(R(2)?R(0x57e40):R(400)),frac=pick();
            const unsigned out=scratch;prepare(0x449b30);auto* a=reinterpret_cast<unsigned*>(S);a[0]=out;a[1]=out+4;a[2]=out+8;a[3]=out+12;a[4]=frames;a[5]=frac;run();
            auto h16=[&](unsigned x){return unsigned(*reinterpret_cast<std::uint16_t*>(x));};
            t1.insert(t1.end(),{frames,frac,h16(out),h16(out+4),h16(out+8),h16(out+12)});
            const auto t=race_frames_to_time_449b30(frames,bf(frac));
            check(t.hours==h16(out)&&t.minutes==h16(out+4)&&t.seconds==h16(out+8)&&t.ms==h16(out+12),"449B30 replay case (host)");
        }
        randomize_all();sync();
        for(unsigned i=0;i<60000;++i){
            const std::uint32_t o3=pick(),o4=R(8)==0?o3:pick();setv(0x4a4440,o3,1);setv(0x4a4440,o4,2);p32(trace,0);services.calls.clear();
            prepare(0x4506b0);guest_call.esi=gw(8);run();const float r=race_sector_progress_4506b0(services,env.w.work(8));
            t2.insert(t2.end(),{o3,o4,guest_call.out_xmm0});check(fb(r)==guest_call.out_xmm0,"4506B0 replay case (host)");
        }
        for(unsigned i=0;i<60000;++i){
            randomize_all();env.w.mode_78026c=0x10;env.w.variant_780258=0;env.w.time_decrement_637911=1;env.st.flags_7d39f0=0;env.st.time_7d394c=1;
            setv(0x49b2d0,0);const std::int16_t pos=std::int16_t(R(4)==0?int(R(3)):int(R(70000))-35000);const std::uint16_t end=std::uint16_t(R(4)==0?R(3):R(65536));
            std::memcpy(env.nw[0].data()+0x64,&pos,2);setv(0x43d470,end);sync();
            prepare(0x450ac0);run();race_check_game_timer_450ac0(env.st,env.w,services);
            t3.insert(t3.end(),{std::uint32_t(std::uint16_t(pos)),end,g32(0x7d3878)});check(fb(env.st.goal_ratio_7d3878)==g32(0x7d3878),"450AC0 ratio replay case (host)");
        }
        std::ofstream f(dump,std::ios::binary);
        const std::uint32_t head[5]={0x50524d52u,x87_control,std::uint32_t(t1.size()/6),std::uint32_t(t2.size()/3),std::uint32_t(t3.size()/3)};
        f.write(reinterpret_cast<const char*>(head),20);
        for(const auto* v:{&t1,&t2,&t3})f.write(reinterpret_cast<const char*>(v->data()),std::streamsize(v->size()*4));
        std::cerr<<"replay dump written: "<<dump<<'\n';
    }
    // ---- 4. trivial foreign inits (original entries temporarily restored) -------------
    {   for(unsigned pc:{0x46fab0u,0x4f0dd0u,0x4f2ac0u})restore(pc,pc+5);
        for(unsigned page:{0x803000u,0x84c000u,0x84d000u})if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("foreign init pages");
        for(unsigned i=0;i<300;++i){
            std::vector<std::uint8_t> vis(kVisibility84cec8Size),score(kScore84def8Size);for(auto& b:vis)b=std::uint8_t(rng());for(auto& b:score)b=std::uint8_t(rng());
            std::uint16_t w=std::uint16_t(rng());
            std::memcpy(reinterpret_cast<void*>(0x84cec8),vis.data(),vis.size());std::memcpy(reinterpret_cast<void*>(0x84def8),score.data(),score.size());
            *reinterpret_cast<std::uint16_t*>(0x8037bc)=w;
            prepare(0x46fab0);run();prepare(0x4f0dd0);run();prepare(0x4f2ac0);run();
            race_reset_8037bc_46fab0(w);race_visibility_init_4f0dd0(vis.data());race_score_init_4f2ac0(score.data());
            if(inject&&i==299)vis[0x100]^=1;
            check(*reinterpret_cast<std::uint16_t*>(0x8037bc)==w,"46FAB0");
            check(std::memcmp(reinterpret_cast<void*>(0x84cec8),vis.data(),vis.size())==0,"4F0DD0 84CEC8..84D8F3");
            check(std::memcmp(reinterpret_cast<void*>(0x84def8),score.data(),score.size())==0,"4F2AC0 84DEF8..84DF38");
        }
    }
    unsigned never=0;for(const auto& e:stubs)if(!hits[e.pc]){++never;std::cerr<<"callee never reached: "<<std::hex<<e.pc<<std::dec<<'\n';}
    std::cout<<"{\"routine\":\"race_manager_probe\",\"coverage\":{\"callees_reached\":"<<stubs.size()-never<<",\"callees\":"<<stubs.size()<<",\"inits\":"<<cov.inits<<",\"control_frames\":"<<cov.frames<<",\"stage_clear\":"<<cov.stage_clear
             <<",\"goal\":"<<cov.goal<<",\"timeup\":"<<cov.timeup<<",\"extend\":"<<cov.extend<<",\"sector_set\":"<<cov.sectors<<",\"route_save\":"<<cov.save_route
             <<",\"ranking_frames\":"<<cov.ranking<<",\"attack_restart\":"<<cov.attack_restart<<"},\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}\n";
    return mismatches?1:0;
}

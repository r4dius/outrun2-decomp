// Included inside the compare_pc dispatcher; uses its private x86 harness.
if(only=="arcade_probe"){
    // OUTRUN2SP arcade frontend (arcade_attract.cpp): the event-4 callbacks of functions
    // 1..0xC against the original. External PC functions are capture stubs recording
    // {pc, ECX, stack args (+ the 4287B0 matrix)} and returning a scripted value; the module's
    // own helpers (4BEE60..4C4880, 4BFB20, 428440 -> 428460) and the protected heads run as
    // original code. Compared: the arcade blocks, the work 780440, the player car byte and the
    // ordered service log.
    using namespace outrun::platform;
    if(argc<=7||std::string(argv[7])=="-")throw std::runtime_error("arcade probe requires the pinned d3dx9_29.dll (D3DXMatrixTranslation)");
    wire_d3dx_imports(map_d3dx29(argv[7]));
    constexpr unsigned base=0x3bc00000u,stubs=base,trace=base+0x8000u,values=base+0x7000u,Obj=base+0x40000u,Player=base+0x41000u,Fix=base+0x43000u;
    constexpr std::uint32_t Work4=0x780440u;
    map_at(base,0x50000,PROT_READ|PROT_WRITE|PROT_EXEC);map_original_arithmetic_bridge(file);
    {   const std::size_t text_file=pe_file_offset(file,0x401000u);
        for(std::uint32_t a=0x401000u;a<0x401000u+0x194881u;a+=0x100u){
            const std::size_t n=std::min<std::size_t>(0x100u,0x401000u+0x194881u-a);
            if(std::memcmp(reinterpret_cast<void*>(a),file.data()+text_file+(a-0x401000u),n))r060_raw_write(a,file.data()+text_file+(a-0x401000u),n);
        }
    }
    for(std::uint32_t page=0x62f000u;page<0x98d000u;page+=0x1000u)mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE);
    auto emit32=[](std::vector<std::uint8_t>& b,std::uint32_t v){for(unsigned k=0;k<4;++k)b.push_back(std::uint8_t(v>>(k*8)));};
    // kind: 0 plain, 1 matrix at arg1, 2 x87 0.0 result, 3 thiscall ret argc*4
    struct Svc{std::uint32_t pc;unsigned argc,kind;};
    const std::vector<Svc> services{
        {0x401000,3,0},{0x401030,1,0},{0x4035f0,0,0},{0x408880,0,0},{0x40ec60,1,0},{0x417f70,0,0},{0x4239c0,2,0},{0x423bd0,1,0},
        {0x423cb0,4,0},{0x424940,1,0},{0x427aa0,2,2},{0x427b10,3,0},{0x428320,3,0},{0x428460,5,0},{0x4285a0,1,0},{0x4287b0,2,1},
        {0x428800,2,0},{0x428880,1,0},{0x4288c0,2,0},{0x429530,5,0},{0x4299c0,1,0},{0x42ccb0,1,0},{0x42d280,6,0},{0x42d5c0,5,0},
        {0x42dfb0,1,0},{0x42e020,3,0},{0x42ebf0,0,0},{0x43f8c0,1,0},{0x43f940,1,0},{0x43f950,1,0},{0x440110,2,0},{0x4401d0,1,0},
        {0x440330,2,0},{0x4406a0,2,0},{0x446a50,0,3},{0x446cf0,0,3},{0x446d90,8,3},{0x447000,2,3},{0x447090,0,3},{0x4470f0,0,3},
        {0x448960,1,0},{0x448980,0,0},{0x4489c0,0,0},{0x448ad0,2,0},{0x448fd0,2,0},{0x4493c0,0,0},{0x449fa0,1,0},{0x44a1a0,0,0},
        {0x44c0a0,1,0},{0x44c0b0,0,0},{0x44c0d0,0,0},{0x44c3d0,0,0},{0x44da00,2,0},{0x44fcc0,1,0},{0x44fd00,0,0},{0x452cd0,2,0},
        {0x452d20,2,0},{0x452ed0,2,0},{0x453320,0,0},{0x4536c0,1,0},{0x4536f0,1,0},{0x453720,1,0},{0x453750,1,0},{0x453780,1,0},
        {0x455660,1,0},{0x4556e0,1,0},{0x455ab0,0,0},{0x455ad0,0,0},{0x455ae0,1,0},{0x455b00,1,0},{0x455b10,0,0},{0x455b40,0,0},
        {0x455b60,1,0},{0x455bf0,1,0},{0x455c10,0,0},{0x455c20,0,0},{0x456d60,0,0},{0x457a60,0,0},{0x4591c0,1,0},{0x4591f0,1,0},
        {0x45aa50,0,0},{0x46bbe0,1,0},{0x47ef20,1,0},{0x47f110,0,0},{0x48b130,1,0},{0x48b150,1,0},{0x48b170,1,0},{0x48b190,1,0},
        {0x48b1b0,1,0},{0x48b1c0,0,0},{0x48b1d0,1,0},{0x48b200,1,0},{0x48f970,1,0},{0x4957d0,0,0},{0x495a10,1,0},{0x49b460,0,0},
        {0x49fa60,0,0},{0x4b6ee0,1,0},{0x4b88d0,0,0},{0x4b89b0,0,0},{0x4b89e0,0,0},{0x4b8c00,0,0},{0x4f21b0,0,0},{0x4f2210,0,0},
        {0x580f40,0,0}};
    std::map<std::uint32_t,unsigned> index_of;std::map<std::uint32_t,std::uint32_t> stub_at;unsigned stub_off=0;
    for(std::size_t k=0;k<services.size();++k){
        const auto& e=services[k];index_of[e.pc]=unsigned(k);
        std::vector<std::uint8_t> c{0x57,0x56,0x51,0x8b,0x3d};emit32(c,trace);
        c.insert(c.end(),{0x6b,0xff,0x60,0x81,0xc7});emit32(c,trace+64);
        c.insert(c.end(),{0xc7,0x07});emit32(c,e.pc);
        c.insert(c.end(),{0x89,0x4f,0x04});
        for(unsigned i=0;i<e.argc;++i)c.insert(c.end(),{0x8b,0x44,0x24,std::uint8_t(0x10+4*i),0x89,0x47,std::uint8_t(0x08+4*i)});
        if(e.kind==1)c.insert(c.end(),{0x8b,0x74,0x24,0x14,0x83,0xc7,0x20,0xb9,0x10,0,0,0,0xf3,0xa5});
        c.insert(c.end(),{0xff,0x05});emit32(c,trace);
        c.push_back(0xa1);emit32(c,values+4*unsigned(k));
        if(e.kind==2)c.insert(c.end(),{0xd9,0xee});
        c.insert(c.end(),{0x59,0x5e,0x5f});
        if(e.kind==3)c.insert(c.end(),{0xc2,std::uint8_t(e.argc*4u),0x00});else c.push_back(0xc3);
        std::memcpy(reinterpret_cast<void*>(stubs+stub_off),c.data(),c.size());
        stub_at[e.pc]=stubs+stub_off;r046_write_rel32(e.pc,0xe9,stubs+stub_off);stub_off=(stub_off+unsigned(c.size())+15u)&~15u;
    }
    std::mt19937 rng(0x4c3dc000u);
    auto rnd=[&](std::uint32_t n){return n?std::uint32_t(rng()%n):0u;};
    auto real=[&](float a,float b){return a+(b-a)*float(rng()%100000)/100000.f;};
    auto g32=[](std::uint32_t a){return *reinterpret_cast<std::uint32_t*>(a);};
    auto p32=[](std::uint32_t a,std::uint32_t v){*reinterpret_cast<std::uint32_t*>(a)=v;};
    auto p16=[](std::uint32_t a,std::uint16_t v){*reinterpret_cast<std::uint16_t*>(a)=v;};
    auto p8=[](std::uint32_t a,std::uint8_t v){*reinterpret_cast<std::uint8_t*>(a)=v;};
    auto pf=[](std::uint32_t a,float v){std::memcpy(reinterpret_cast<void*>(a),&v,4);};
    unsigned cases{},mismatches{};const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
    std::map<std::string,unsigned> coverage;
    auto fail=[&](const std::string& what){++mismatches;if(mismatches<20)std::cerr<<what<<"\n";};
    auto compare=[&](const void* native,const void* original,std::size_t n,const std::string& what){
        ++cases;if(!std::memcmp(native,original,n))return;
        const auto* a=static_cast<const std::uint8_t*>(native);const auto* b=static_cast<const std::uint8_t*>(original);
        for(std::size_t i=0;i<n;++i)if(a[i]!=b[i]){std::ostringstream s;s<<what<<" offset=0x"<<std::hex<<i<<" PC="<<unsigned(b[i])<<" native="<<unsigned(a[i]);fail(s.str());break;}
    };
    struct Mirror{std::uint32_t base,end;std::vector<std::uint8_t> bytes;};
    std::vector<Mirror> mirrors{{Work4,Work4+0x70u,{}},{0x844928u,0x844ac0u,{}},{0x84a208u,0x84aa00u,{}},{0x6898c0u,0x68a000u,{}},
        {0x65a7a0u,0x65a7c0u,{}},{0x780258u,0x78025cu,{}},{0x7c24bcu,0x7c24c0u,{}},{0x799c28u,0x799c2cu,{}},{0x799d18u,0x799d1cu,{}},{0x842820u,0x8428b8u,{}},
        {Obj,Obj+0x600u,{}},{Player,Player+0x40u,{}},{Fix,Fix+0x400u,{}}};
    std::vector<std::uint8_t> data_6898c0(0x740),data_65a7a0(0x20);
    std::memcpy(data_6898c0.data(),file.data()+pe_file_offset(file,0x6898c0u),data_6898c0.size());
    std::memcpy(data_65a7a0.data(),file.data()+pe_file_offset(file,0x65a7a0u),data_65a7a0.size());
    static constexpr std::uint32_t Pc[]{0x4bfe00,0x4c3510,0x4bf3f0,0x4bff80,0x4c3530,0x4c1fa0,0x4c2240,0x4c3960,0x4c2420,0x4c0060,
        0x4c0090,0x4c01e0,0x4c3dc0,0x4c22c0,0x4bf4d0,0x4c18c0,0x4c2d20,0x4c1e80,0x4bfdb0,0x4c1f10,0x4c30b0,0x4c02b0,0x4c02e0,
        0x4c04d0,0x4c4880,0x4c22e0,0x4c2330,0x4c26d0,0x4c05f0,0x4bf500,0x4bf510,0x4bfb90,0x4c2b70,0x4bee80,
        // the entry module services (arcade_entry_invoke): run as original code for these entries
        0x4b6ee0,0x4b88d0,0x4b89b0,0x4b89e0,0x4b8c00};
    const unsigned Entries=unsigned(sizeof Pc/sizeof Pc[0]),FirstModule=Entries-5u;
    static constexpr std::uint32_t ModulePc[]{0x4b6ee0,0x4b88d0,0x4b89b0,0x4b89e0,0x4b8c00};
    const std::size_t text_off=pe_file_offset(file,0x401000u);
    auto module_original=[&](bool original){
        for(std::uint32_t pc:ModulePc){
            if(original)r060_raw_write(pc,file.data()+text_off+(pc-0x401000u),5);
            else r046_write_rel32(pc,0xe9,stub_at.at(pc));
        }
    };
    for(unsigned iter=0;iter<count;++iter){
        unsigned entry=iter%Entries;
        if(const char* e=std::getenv("OR2_ARCADE_ENTRY"))entry=unsigned(std::stoul(e));
        std::memset(reinterpret_cast<void*>(trace),0,0x8000);
        std::vector<std::uint32_t> value(services.size());
        for(auto& v:value)v=rnd(3)?rnd(3):rng();
        auto setv=[&](std::uint32_t pc,std::uint32_t v){value[index_of.at(pc)]=v;};
        auto handle=[&]{return rnd(4)?rnd(1344):0xffffffffu;};
        setv(0x4035f0,Obj);setv(0x428320,handle());setv(0x428460,handle());setv(0x428880,1u+rnd(4));
        setv(0x4536f0,rnd(3)?0u:1u);setv(0x4536c0,rnd(3)?0u:1u);setv(0x453780,rnd(4)?0u:1u);
        setv(0x453720,std::uint32_t(std::int32_t(rnd(257))-128));setv(0x453750,std::uint32_t(std::int32_t(rnd(257))-128));
        setv(0x456d60,(rng()&0xffffff00u)|(rnd(3)?1u:rnd(5)));setv(0x455ad0,(rng()&0xffffff00u)|rnd(4));setv(0x455b00,(rng()&0xffffff00u)|rnd(4));
        setv(0x4591f0,(rng()&0xffffff00u)|rnd(12));setv(0x47f110,rnd(4)?0u:1u);setv(0x4493c0,rnd(3)?0u:1u);setv(0x49b460,rnd(5)?0u:1u);
        setv(0x427aa0,0u);setv(0x449fa0,Fix+0x380u+4u*rnd(8));setv(0x4239c0,rnd(2)?0u:0x1234u);setv(0x46bbe0,rnd(0x223));
        setv(0x448980,rnd(2));setv(0x448960,rnd(2));setv(0x4f21b0,rnd(2));setv(0x45aa50,rnd(2));setv(0x455b40,rnd(2));
        setv(0x455b60,rnd(2));setv(0x455bf0,rnd(3)?1u:0u);setv(0x48f970,(rng()&0xffffff00u)|(rnd(3)?0xffu:rnd(4)));
        setv(0x453320,std::uint32_t(std::int32_t(rnd(5))-1));setv(0x455c20,rnd(5));setv(0x455c10,rnd(4));setv(0x48b1c0,rnd(4));
        setv(0x580f40,rnd(0x8000));setv(0x417f70,rng());
        std::memcpy(reinterpret_cast<void*>(values),value.data(),value.size()*4);
        // globals: blocks random, then the fields that index tables kept in range
        for(auto& mi:mirrors)for(std::uint32_t a=mi.base;a<mi.end;a+=4)if(mi.end-a>=4)p32(a,rnd(3)?rnd(4):rng());else p8(a,std::uint8_t(rng()));
        std::memcpy(reinterpret_cast<void*>(0x6898c0u),data_6898c0.data(),data_6898c0.size());
        std::memcpy(reinterpret_cast<void*>(0x65a7a0u),data_65a7a0.data(),data_65a7a0.size());
        for(std::uint32_t a=0x65a7a4u;a<0x65a7b4u;++a)p8(a,std::uint8_t(rnd(4)?rnd(4):rng()));
        p32(0x689b24u,rnd(12));p32(0x689fb4u,rnd(2)?0x28u:0x5au);p32(0x689fb8u,rnd(2)?0xffffffd8u:0xffffffa6u);
        p32(0x780258u,rnd(3)?rnd(10):(3u+rnd(2)));p32(0x7c24bcu,rnd(2));p32(0x799c28u,Work4);p32(0x799d18u,Player);
        p32(Obj+0x518u,rnd(3)?0u:(rnd(2)?1u:0xffffffffu));
        // the event-4 work
        p32(Work4,0x5c8be0u);p16(Work4+8u,std::uint16_t(rng()));p8(Work4+0xau,std::uint8_t(rng()));
        p16(Work4+0x10u,std::uint16_t(rnd(3)?rnd(3):rnd(0x300)));p16(Work4+0x12u,std::uint16_t(rnd(3)?rnd(3):rnd(0x40)));
        p8(Work4+0x15u,std::uint8_t(rnd(12)));p8(Work4+0x18u,std::uint8_t(rnd(5)));
        p8(Work4+0x19u,std::uint8_t(rnd(5)));p8(Work4+0x1au,std::uint8_t(rnd(5)));p8(Work4+0x1bu,std::uint8_t(rnd(4)));
        p8(Work4+0x1du,std::uint8_t(rnd(5)));p8(Work4+0x1eu,std::uint8_t(rnd(6)));
        for(std::uint32_t o:{0x1fu,0x20u,0x22u,0x23u})p8(Work4+o,std::uint8_t(rnd(3)?rnd(4):0xffu));
        p8(Work4+0x27u,std::uint8_t(rnd(3)?rnd(4):0xffu));p8(Work4+0x24u,std::uint8_t(rnd(8)));
        for(std::uint32_t o:{0x25u,0x26u,0x21u})p8(Work4+o,std::uint8_t(rnd(4)));
        p8(Work4+0x3cu,std::uint8_t(rnd(3)));p8(Work4+0x3du,std::uint8_t(rnd(2)));p8(Work4+0x3eu,std::uint8_t(rnd(2)));
        if(entry==24u)p32(Work4+0x3cu,rnd(5));
        if(entry==28u)p8(Work4+0x20u,std::uint8_t(rnd(4)?rnd(8):0xfu));
        const bool module=entry>=FirstModule;
        std::uint32_t module_arg=0;
        if(module){   // offline-shaped counts: 0 or 2..4 players, the local slot inside them
            const std::uint32_t n=rnd(4)?2u+rnd(3):0u;setv(0x455c20,n);setv(0x455c10,n?1u+rnd(n):rnd(3));
            if(Pc[entry]==0x4b88d0u&&!n){setv(0x455c20,2u);setv(0x455c10,1u+rnd(2));}
            setv(0x455ab0,(rng()&0xffff0000u)|std::uint16_t(rnd(3)?rnd(0x1000):std::int16_t(-std::int16_t(rnd(100)))));
            setv(0x456d60,(rng()&0xffffff00u)|rnd(5));setv(0x455b00,rnd(4)?rnd(4):rng());
            p32(0x84284cu,std::uint32_t(std::int32_t(rnd(6))-1));p32(0x842894u,rnd(3)?rnd(200):rng());
            p32(0x842898u,rnd(3)?(rnd(2)?0x1fu:0xffffffffu):rng());p32(0x84289cu,rnd(2));
            for(std::uint32_t a=0x8428a0u;a<0x8428b0u;a+=4)p32(a,rnd(2));
            for(std::uint32_t a:{0x84288cu,0x842890u,0x8428b0u})p32(a,handle());
            module_arg=rnd(3)?(rng()&0xffffff00u)|rnd(6):rng();
        }
        p8(Work4+0x40u,std::uint8_t(rnd(2)));p8(Work4+0x41u,std::uint8_t(rnd(3)));
        p32(Work4+0x44u,rnd(3)?rnd(0x20):0x1eu);p8(Work4+0x48u,std::uint8_t(rnd(2)));p8(Work4+0x4au,std::uint8_t(rnd(2)));
        p8(Work4+0x4bu,std::uint8_t(rnd(3)));p8(Work4+0x4cu,std::uint8_t(rnd(3)?0u:rnd(4)));
        p32(Work4+0x50u,rnd(2));p32(Work4+0x54u,rnd(2));p32(Work4+0x58u,rnd(0x180));
        // arcade blocks
        for(std::uint32_t a=0x844960u;a<=0x8449d8u;a+=4)p32(a,handle());
        for(std::uint32_t a:{0x844964u,0x844968u,0x84497cu,0x844980u,0x844984u,0x84498cu,0x844990u,0x84499cu,0x8449a0u,0x8449a4u,0x8449a8u})p32(a,handle());
        p32(0x844928u,rnd(2));p32(0x844944u,rnd(2));p32(0x844948u,rnd(2));p32(0x844950u,rnd(2));p32(0x844954u,rnd(3));p32(0x844958u,rnd(3)?0u:1u);
        p8(0x844940u,std::uint8_t(rnd(2)));p8(0x84494eu,std::uint8_t(rnd(12)));p16(0x8449e4u,std::uint16_t(rnd(16)));p16(0x8449f8u,std::uint16_t(rnd(12)));
        p32(0x8449dcu,rnd(5));p32(0x8449e0u,std::uint32_t(std::int32_t(rnd(5))-1));p32(0x8449e8u,rnd(2));p32(0x8449ecu,rnd(2)?4u:rnd(10));
        p32(0x8449f4u,rnd(4));p32(0x8449fcu,rnd(3)?0x1eu:rnd(40));p32(0x844a00u,rnd(3)?0x1eu:(rnd(2)?0xffffffffu:rnd(40)));p32(0x844a04u,rnd(0x18));
        static constexpr std::uint32_t Tables[]{0x5c8ea8u};
        p32(0x84a208u,rnd(2)?Tables[0]:g32(0x5c9204u+rnd(5)*4u));
        p32(0x84a210u,rnd(3)?0u:1u);p32(0x84a214u,rnd(3)?0xcu:rnd(13));p32(0x84a218u,rnd(2));p32(0x84a21cu,1u+rnd(3));
        p32(0x84a238u,rnd(2));p8(0x84a248u,std::uint8_t(rnd(12)));p32(0x84a23cu,0x2e0022u+rnd(4));
        p32(0x84a240u,(g32(0x7c24bcu)?0x689c68u:0x689b28u)+0x20u*rnd(5));p32(0x84a244u,rnd(2));
        for(std::uint32_t a=0x84a24cu;a<0x84a25cu;a+=4)p32(a,0x2e0000u+rnd(0x60));
        p8(0x84a260u,std::uint8_t(rnd(5)));p8(0x84a261u,std::uint8_t(rnd(5)));
        pf(0x84a264u,real(-200,400));pf(0x84a268u,rnd(3)?0.0f:real(-60,60));pf(0x84a26cu,real(0,3));pf(0x84a270u,real(-200,400));
        pf(0x84a274u,real(0,40));pf(0x84a278u,real(20,80));pf(0x84a27cu,real(100,900));pf(0x84a280u,real(0,640));pf(0x84a284u,real(0,480));
        pf(0x84a288u,real(0,40));for(std::uint32_t a=0x84a28cu;a<0x84a291u;++a)p8(a,std::uint8_t(rnd(4)));
        p8(0x84a291u,std::uint8_t(3u+rnd(8)));p8(0x84a292u,std::uint8_t(rnd(3)?0u:rnd(10)));p8(0x84a293u,std::uint8_t(rnd(2)));
        pf(0x84a294u,real(-300,300));p32(0x84a298u,0x2e0000u+rnd(0x60));p32(0x84a29cu,rnd(2)?g32(0x84a298u):0x2e0000u+rnd(0x60));
        p32(0x84a2a0u,rnd(16));p8(0x84a2a4u,std::uint8_t(rnd(2)));p8(0x84a2a8u,std::uint8_t(rnd(2)));p32(0x84a2acu,rnd(2));
        p32(0x84a2b0u,rnd(3));p16(0x84a2b4u,std::uint16_t(rnd(16)));p32(0x84a2b8u,rnd(3)?0xffffffffu:rnd(0x180));p8(0x84a2bcu,std::uint8_t(rnd(3)));
        p16(0x84a2c0u,std::uint16_t(rnd(2)));p32(0x84a2c8u,rnd(2));p32(0x84a2d8u,rnd(2));p32(0x84a2dcu,rnd(2));p32(0x84a2e0u,rnd(4));p32(0x84a2e4u,rnd(4));
        for(std::uint32_t a=0x84a2e8u;a<0x84a310u;a+=4)p32(a,rnd(3));
        p32(0x84a310u,rnd(3)?0u:rnd(0x40));pf(0x84a314u,real(-40,40));p32(0x84a318u,rnd(2));
        p32(0x84a9acu,rnd(0x40));p32(0x84a9f4u,rnd(3)?rnd(0x60):0xffffu);p32(0x84a9f8u,rnd(0x40));p8(0x84a9fcu,std::uint8_t(rnd(40)));
        {   // the list screens keep their row count (4BEE90) and a row inside it
            std::uint8_t rows=0;
            if(entry==12u)rows=rnd(2)?3u:5u;else if(entry==24u)rows=8u;else if(entry==16u)rows=0xau;
            if(rows){p8(0x84a291u,rows);p8(Work4+0x19u,std::uint8_t(rnd(rows)));p8(Work4+0x1au,std::uint8_t(rnd(rows)));}
        }
        std::memcpy(reinterpret_cast<void*>(values),value.data(),value.size()*4);
        for(auto& mi:mirrors){mi.bytes.resize(mi.end-mi.base);std::memcpy(mi.bytes.data(),reinterpret_cast<void*>(mi.base),mi.bytes.size());}
        if(module)module_original(true);
        prepare(Pc[entry]);reinterpret_cast<unsigned*>(S)[0]=module?module_arg:Work4;
        run();
        if(module)module_original(false);
        // native
        PcRaceMemory mem;std::vector<std::vector<std::uint8_t>> native(mirrors.size());
        for(std::size_t i=0;i<EmbeddedExeRangeCount;++i)mem.map_const(EmbeddedExeRanges[i].base,EmbeddedExeRanges[i].data,EmbeddedExeRanges[i].size);
        for(std::size_t i=0;i<mirrors.size();++i){native[i]=mirrors[i].bytes;mem.map(mirrors[i].base,native[i].data(),native[i].size());}
        std::array<std::uint8_t,0x40> mb{};outrun::driving::PcMatrixStack stack{outrun::driving::Bytes(mb.data(),mb.size())};
        struct NCall{std::uint32_t pc,ecx;decltype(PcRaceCall::args) a;std::array<std::uint32_t,16> mat;};std::vector<NCall> calls;
        PcRaceContext ctx{mem,stack,[&](const PcRaceCall& k)->std::uint32_t{
            const auto it=index_of.find(k.pc);if(it==index_of.end()){std::ostringstream s;s<<"arcade probe: unexpected service "<<std::hex<<k.pc;throw std::runtime_error(s.str());}
            NCall n{k.pc,k.ecx,k.args,{}};
            if(services[it->second].kind==1)for(unsigned w=0;w<16;++w)n.mat[w]=mem.u32(k.args[1]+w*4);
            calls.push_back(n);return value[it->second];},nullptr};
        bool ok=true;std::string why;
        try{
            if(module){PcRaceCall k;k.pc=Pc[entry];k.argc=1;k.args[0]=module_arg;std::uint32_t r=0;
                if(!arcade_entry_invoke(ctx,k,r))throw std::runtime_error("entry service not ported");}
            else if(!arcade_event4_invoke(ctx,Pc[entry],Work4))throw std::runtime_error("callback not ported");}
        catch(const std::exception& e){ok=false;why=e.what();}
        std::ostringstream tg;tg<<"entry "<<entry<<" ("<<std::hex<<Pc[entry]<<std::dec<<") run "<<iter;const std::string tag=tg.str();
        ++cases;if(!ok){fail(tag+" native threw: "+why);continue;}
        if(inject&&iter==11)native[1][0x40]^=1;
        for(std::size_t i=0;i<mirrors.size();++i){std::ostringstream s;s<<tag<<" region "<<std::hex<<mirrors[i].base;
            compare(native[i].data(),reinterpret_cast<void*>(mirrors[i].base),native[i].size(),s.str());}
        const unsigned n=g32(trace);++cases;
        if(n!=calls.size()){std::ostringstream s;s<<tag<<" service count PC="<<n<<" native="<<calls.size()<<std::hex;
            for(unsigned i=0;i<n&&i<40;++i)s<<" "<<g32(trace+64+i*0x60);s<<" | native";for(auto& cl:calls)s<<" "<<cl.pc;fail(s.str());}
        else for(unsigned i=0;i<n;++i){const auto* g=reinterpret_cast<const std::uint32_t*>(trace+64+i*0x60);
            const auto& e=services[index_of[g[0]]];bool same=g[0]==calls[i].pc;
            if(e.kind==3)same=same&&g[1]==calls[i].ecx;
            for(unsigned k=0;k<e.argc;++k){
                if(e.kind==1&&k==1)continue;                               // 4287B0: the matrix, not its address
                // 452ED0 / 452CD0 / 452D20 take 16-bit pushes (movsx ax / cx): only the low word is defined.
                const bool word_arg=(g[0]==0x452ed0u||g[0]==0x452cd0u)||(g[0]==0x452d20u&&k==0);
                const bool byte_arg=g[0]==0x4406a0u&&k==1;   // 4BF340 pushes ebx (bl defined)
                // byte pushes (xor reg / mov dl or movzx of a byte) are full dwords; 48B1xx setters take al.
                same=same&&(word_arg?(g[2+k]&0xffffu)==(calls[i].a[k]&0xffffu):byte_arg?(g[2+k]&0xffu)==(calls[i].a[k]&0xffu):g[2+k]==calls[i].a[k]);
            }
            if(e.kind==1)for(unsigned w=0;w<16;++w)same=same&&g[8+w]==calls[i].mat[w];
            if(!same){std::ostringstream s;s<<tag<<" service #"<<i<<" "<<std::hex<<g[0]<<" ecx "<<g[1]<<"/"<<calls[i].ecx;
                for(unsigned k=0;k<e.argc;++k)s<<" PC "<<g[2+k]<<"/native "<<calls[i].a[k];fail(s.str());break;}}
        ++coverage["entry "+std::to_string(entry)];
        coverage["services"]+=unsigned(calls.size());
        if(std::getenv("OR2_ARCADE_HIST")){std::ostringstream h;h<<"e"<<entry<<" n"<<std::min<std::size_t>(calls.size(),40);++coverage[h.str()];
            for(auto& cl:calls){std::ostringstream q;q<<"e"<<entry<<" "<<std::hex<<cl.pc;++coverage[q.str()];}}
    }
    std::cout<<"{\"routine\":\""<<only<<"\",\"coverage\":{";bool first=true;
    for(const auto& [k,v]:coverage){std::cout<<(first?"":",")<<"\""<<k<<"\":"<<v;first=false;}
    std::cout<<"},\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}"<<std::endl;
    return mismatches?1:0;
}

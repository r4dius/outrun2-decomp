// Included inside the compare_pc dispatcher; uses its private x86 harness.
if(only=="race_end_probe"){
    // Race end modes (race_end_modes.cpp) against the original:
    //   0 49A1E0  1 49A280  2 49D1E0 (+49A4F0)  3 49A790  4 49A800  5 49A920
    //   6 49ADC0  7 49AEA0  8 49AED0 (bookkeeping 452xxx, 453370, 49AD00)
    //   9 49B170  10 49AF90 (all four stages)  11 4AE750  12 4AE960
    //   13 4AED20  14 4AED50  15 4AEEC0  16 4AEEE0  17 4B72F0(kind)
    //   18 4BFB20 (VM at 4BFB25)  19 45AEF0 (VM entry)  20 4B7630 (VM entry)
    // Boundary services are capture stubs recording {pc, ECX, stack args,
    // and for 4287B0 the 16 matrix words} and returning a scripted value
    // (4505E0/4505F0 also push 0.0 for the caller's fstp; 4EDCE0 is a
    // thiscall `ret 4`). The helpers 4B72F0, 4B7630, 4BFB20, 45AEF0,
    // 428440, 452xxx, 453370, 4AEF20, 498400, 47F110 and D3DXMatrixTranslation
    // run as original code. Compared: every mirrored region, the service log.
    using namespace outrun::platform;
    if(argc<=7||std::string(argv[7])=="-")throw std::runtime_error("race end probe requires the pinned d3dx9_29.dll (D3DXMatrixTranslation)");
    wire_d3dx_imports(map_d3dx29(argv[7]));
    constexpr unsigned base=0x3bc00000u,stubs=base,trace=base+0x8000u,values=base+0x7000u,Obj=base+0x40000u,Player=base+0x41000u,Fix=base+0x43000u;
    map_at(base,0x50000,PROT_READ|PROT_WRITE|PROT_EXEC);map_original_arithmetic_bridge(file);
    {   unsigned restored=0;const std::size_t text_file=pe_file_offset(file,0x401000u);
        for(std::uint32_t a=0x401000u;a<0x401000u+0x194881u;a+=0x100u){
            const std::size_t n=std::min<std::size_t>(0x100u,0x401000u+0x194881u-a);
            if(std::memcmp(reinterpret_cast<void*>(a),file.data()+text_file+(a-0x401000u),n)){r060_raw_write(a,file.data()+text_file+(a-0x401000u),n);++restored;}
        }
        std::cerr<<"restored "<<restored<<" patched .text blocks"<<char(10);
    }
    for(std::uint32_t page=0x62f000u;page<0x98d000u;page+=0x1000u)mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE);
    auto emit32=[](std::vector<std::uint8_t>& b,std::uint32_t v){for(unsigned k=0;k<4;++k)b.push_back(std::uint8_t(v>>(k*8)));};
    // kind: 0 plain, 1 matrix at arg1, 2 x87 0.0 result, 3 thiscall ret 4
    struct Svc{std::uint32_t pc;unsigned argc,kind;};
    const std::vector<Svc> services{
        {0x427630,0,0},{0x428600,0,0},{0x43f900,1,0},{0x440a10,2,0},{0x440a30,2,0},{0x440330,2,0},{0x4401d0,1,0},{0x440110,2,0},
        {0x440cd0,1,0},{0x43fa90,0,0},{0x4957f0,0,0},{0x55a930,0,0},{0x43f860,0,0},{0x43f8c0,1,0},{0x43f980,0,0},{0x43f990,1,0},
        {0x4505d0,0,0},{0x43f8e0,0,0},{0x4489c0,0,0},{0x44c3d0,0,0},{0x44a1a0,0,0},{0x43de50,1,0},{0x4f11b0,1,0},{0x4f0600,1,0},
        {0x46fc30,1,0},{0x4f2210,0,0},{0x42deb0,2,0},{0x429920,2,0},{0x401000,3,0},{0x401030,1,0},{0x47ef30,1,0},{0x42df90,0,0},
        {0x4299a0,0,0},{0x43f960,0,0},{0x450240,0,0},{0x42dfb0,1,0},{0x4299c0,1,0},{0x42e020,3,0},{0x455710,0,0},{0x4edce0,1,3},
        {0x48b1f0,0,0},{0x451350,1,0},{0x44c940,1,0},{0x4505a0,1,0},{0x4505e0,0,2},{0x4505f0,1,2},{0x44fd00,0,0},{0x42ebf0,0,0},
        {0x4489f0,0,0},{0x42dfd0,0,0},{0x429a10,0,0},{0x47ed40,0,0},{0x465fa0,0,0},{0x451a00,0,0},{0x4276b0,0,0},{0x4999d0,0,0},
        {0x496170,0,0},{0x4e85e0,0,0},{0x496180,0,0},{0x4e8620,0,0},{0x499890,0,0},{0x450230,1,0},{0x455730,0,0},{0x48b140,0,0},
        {0x48b160,0,0},{0x428320,3,0},{0x428460,5,0},{0x4285a0,1,0},{0x428770,2,0},{0x4287b0,2,1},{0x428800,2,0},{0x428880,1,0},
        {0x4506a0,1,0},{0x4035f0,0,0},{0x4536f0,1,0},
        {0x4278c0,1,0},{0x427700,1,0},{0x424940,1,0},{0x46c500,0,0},{0x48b310,0,0},{0x48b350,0,0},{0x496510,0,0},{0x495b20,0,0},
        {0x495b80,0,0},{0x4959e0,0,0},{0x495a60,0,0},{0x4249f0,1,0},{0x4999f0,0,0},{0x448990,1,0},{0x44dbb0,1,0},{0x44c500,0,0},
        {0x44c510,0,0},{0x4f12a0,4,0},{0x4f1a90,2,0},{0x4f1210,2,0},{0x580c38,1,0},{0x44ff10,0,0},{0x495890,0,0},{0x4b99d0,1,0},
        {0x45b820,0,0},
        {0x443ea0,0,0},{0x44b7b0,1,0},{0x450380,1,0},{0x495820,0,0},{0x4493c0,0,0},{0x44dc50,1,0},{0x457840,0,0},{0x456d70,0,0},
        {0x45a0b0,1,0},{0x455ad0,0,0},{0x456e10,0,0},{0x4872e0,0,0},{0x43f910,0,0},{0x495c10,0,0},{0x456d40,0,0},{0x451180,1,0},{0x450320,0,0},
        {0x48b180,0,0},{0x48b1a0,0,0},{0x4478f0,6,3},{0x447750,8,3},{0x447a60,8,3},{0x447c10,8,3},{0x495a20,0,0},{0x48b430,0,0},{0x4956c0,0,0},
        {0x467880,0,0},{0x467960,0,0},{0x47ef10,0,0},
        {0x401050,0,0},{0x49fa60,0,0},{0x45a840,0,0},{0x44da00,2,0},{0x4518c0,0,0},{0x46bbe0,1,0},{0x448ad0,2,0},{0x448980,0,0},
        {0x49ba50,0,0},{0x44c0b0,0,0},{0x49ba80,0,0},{0x440200,1,0},{0x45a920,0,0},{0x44fcc0,1,0},{0x43f8f0,0,0},{0x43f940,1,0},{0x4532e0,0,0},
        {0x429530,5,0},{0x4b8e20,0,0},{0x42ccb0,1,0},{0x42ca60,1,0},{0x42cc60,2,0},{0x42cca0,1,0},{0x42cc00,2,0},{0x42ccc0,1,0},{0x4294c0,3,7},{0x465f70,1,0},{0x466190,1,0},{0x4b8e30,1,0},{0x4bea50,0,0},{0x495830,0,0},{0x496440,0,0},{0x45bf30,1,0},{0x45b830,0,0},{0x45b840,0,0},{0x4b9c80,1,0},{0x46c220,0,0},{0x497579,0,9},{0x476710,1,0},{0x42cdd0,1,0},{0x43fa00,0,0},{0x4ef710,0,0},{0x47ee70,1,8},{0x42d280,6,0},{0x42d200,7,0},{0x4b0100,1,0},{0x450560,1,0},{0x44c8d0,1,0},{0x456d60,0,0},{0x455ae0,1,0},{0x45a2b0,1,0},{0x4591f0,1,0},{0x4591c0,1,0},{0x4bb7c0,0,0},{0x49a060,0,0},{0x46c440,2,0},{0x45bd30,1,0},{0x46c420,0,0},{0x497900,0,0},{0x48c5f0,0,0}};
    std::map<std::uint32_t,unsigned> index_of;unsigned stub_off=0;
    for(std::size_t k=0;k<services.size();++k){
        const auto& e=services[k];index_of[e.pc]=unsigned(k);
        std::vector<std::uint8_t> c{0x57,0x56,0x51,0x8b,0x3d};emit32(c,trace);            // push edi/esi/ecx; mov edi,[count]
        c.insert(c.end(),{0x6b,0xff,0x60,0x81,0xc7});emit32(c,trace+64);                    // imul edi,edi,0x60; add edi,entries
        c.insert(c.end(),{0xc7,0x07});emit32(c,e.pc);                                       // [edi]=pc
        c.insert(c.end(),{0x89,0x4f,0x04});                                                 // [edi+4]=ecx
        for(unsigned i=0;i<e.argc;++i)c.insert(c.end(),{0x8b,0x44,0x24,std::uint8_t(0x10+4*i),0x89,0x47,std::uint8_t(0x08+4*i)});
        if(e.kind==1)c.insert(c.end(),{0x8b,0x74,0x24,0x14,0x83,0xc7,0x20,0xb9,0x10,0,0,0,0xf3,0xa5});   // esi=arg1; edi+=0x20; rep movsd 16
        c.insert(c.end(),{0xff,0x05});emit32(c,trace);                                      // inc [count]
        c.push_back(0xa1);emit32(c,values+4*unsigned(k));                                   // eax=value
        if(e.kind==2)c.insert(c.end(),{0xd9,0xee});
        if(e.kind==7)c.insert(c.end(),{0x8b,0x74,0x24,0x14,0x89,0x06,0x8b,0x74,0x24,0x18,0x89,0x06});   // value -> [arg1], [arg2]
        if(e.kind==8)c.insert(c.end(),{0x8b,0x7c,0x24,0x10,0xb9,0x10,0,0,0,0xf3,0xab});      // edi=arg0; ecx=16; rep stosd value
        if(e.kind==9){c.insert(c.end(),{0x59,0x5e,0x5f,0xb8});emit32(c,0x4978f4u);c.insert(c.end(),{0xff,0xe0});}   // 497579: on to 497560's epilogue                                         // fldz
        if(e.kind!=9)c.insert(c.end(),{0x59,0x5e,0x5f});
        if(e.kind==9){}else if(e.kind==3)c.insert(c.end(),{0xc2,std::uint8_t(e.argc*4u),0x00});else c.push_back(0xc3);   // thiscall: ret argc*4
        std::memcpy(reinterpret_cast<void*>(stubs+stub_off),c.data(),c.size());
        r046_write_rel32(e.pc,0xe9,stubs+stub_off);stub_off=(stub_off+unsigned(c.size())+15u)&~15u;
    }
    std::mt19937 rng(0x49a1e000u);
    auto rnd=[&](std::uint32_t n){return n?std::uint32_t(rng()%n):0u;};
    auto real=[&](float a,float b){return a+(b-a)*float(rng()%100000)/100000.f;};
    auto g32=[](std::uint32_t a){return *reinterpret_cast<std::uint32_t*>(a);};
    auto p32=[](std::uint32_t a,std::uint32_t v){*reinterpret_cast<std::uint32_t*>(a)=v;};
    auto pf=[](std::uint32_t a,float v){std::memcpy(reinterpret_cast<void*>(a),&v,4);};
    unsigned cases{},mismatches{},undefined{};const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
    std::map<std::string,unsigned> coverage;
    auto fail=[&](const std::string& what){++mismatches;if(mismatches<20)std::cerr<<what<<"\n";};
    auto compare=[&](const void* native,const void* original,std::size_t n,const std::string& what){
        ++cases;if(!std::memcmp(native,original,n))return;
        const auto* a=static_cast<const std::uint8_t*>(native);const auto* b=static_cast<const std::uint8_t*>(original);
        for(std::size_t i=0;i<n;++i)if(a[i]!=b[i]){std::ostringstream s;s<<what<<" offset=0x"<<std::hex<<i<<" PC="<<unsigned(b[i])<<" native="<<unsigned(a[i]);fail(s.str());break;}
    };
    struct Mirror{std::uint32_t base,end;std::vector<std::uint8_t> bytes;};
    std::vector<Mirror> mirrors{{0x836ce0u,0x836ce2u,{}},{0x8367acu,0x8367b0u,{}},{0x8367f4u,0x8367f8u,{}},
        {0x842048u,0x842100u,{}},{0x687774u,0x68777cu,{}},{0x842888u,0x84288cu,{}},{0x7d3a90u,0x7d3e00u,{}},{0x7d6720u,0x7d6770u,{}},
        {0x7f1958u,0x7f1978u,{}},{0x67ee38u,0x67ee54u,{}},{0x65994cu,0x659950u,{}},{0x780258u,0x78025cu,{}},{0x78024cu,0x780250u,{}},
        {0x799d18u,0x799d1cu,{}},{0x7c23e0u,0x7c27ecu,{}},{0x6840ecu,0x6840f8u,{}},{0x8a8cdcu,0x8a8ce0u,{}},{Obj,Obj+0x600u,{}},{Player,Player+0x10f0u,{}},
        {0x836d18u,0x836d2cu,{}},{0x67f614u,0x67f618u,{}},{0x67f638u,0x67f644u,{}},{0x84b900u,0x84bd04u,{}},{0x8369b4u,0x836ce0u,{}},
        {0x8367c0u,0x8367ecu,{}},{Fix,Fix+0x400u,{}},
        {0x82e7c0u,0x82e7f0u,{}},{0x836630u,0x836718u,{}},{0x78026cu,0x780270u,{}},{0x651758u,0x653788u,{}},{0x7d68bcu,0x7d68d4u,{}},{0x836d08u,0x836d18u,{}},{0x635f34u,0x635f38u,{}},{0x836ce4u,0x836ce8u,{}},{0x8367f0u,0x8367f2u,{}},
        {0x67ef60u,0x67ef70u,{}},{0x7f2560u,0x7f26e0u,{}},
        {0x830360u,0x830390u,{}},{0x655b58u,0x655b90u,{}},{0x79fb48u,0x79fb50u,{}},{0x799c28u,0x799c2cu,{}},{0x842820u,0x842888u,{}}};
    const unsigned Entries=59u;
    for(unsigned iter=0;iter<count;++iter){
        unsigned entry=iter%Entries;
        if(const char* e=std::getenv("OR2_RACE_END_ENTRY"))entry=unsigned(std::stoul(e));
        std::memset(reinterpret_cast<void*>(trace),0,0x8000);
        std::vector<std::uint32_t> value(services.size());
        for(auto& v:value)v=rnd(3)?rnd(3):rng();
        auto setv=[&](std::uint32_t pc,std::uint32_t v){value[index_of.at(pc)]=v;};
        setv(0x43fa90,rnd(2));setv(0x43f980,rnd(6)?0u:1u);setv(0x450240,rnd(8));setv(0x4957f0,rnd(4)?(rng()&0xffffff00u):rng());
        setv(0x55a930,rnd(6)?0u:1u);setv(0x43f860,rnd(3)?(rng()&0xffffff00u):rng()|1u);
        {const std::uint32_t m[]{0x19u,0x1au,0x1bu,0x1cu,rng()};setv(0x43f8e0,m[rnd(5)]);}
        setv(0x42df90,rnd(3)?1u:0u);setv(0x4299a0,rnd(3)?1u:0u);setv(0x43f960,rnd(2));
        {const std::uint32_t r[]{0u,1u,2u,0u,1u,0xffffffffu};setv(0x451350,r[rnd(entry==8u||entry==26u?3u:6u)]);}   // 453370 indexes 5A7AB4 with it
        setv(0x44c940,rnd(7));setv(0x48b140,(rng()&0xffffff00u)|(rnd(8)?rnd(30):0xffu));setv(0x48b160,rnd(3)?(rng()&0xffffff00u)|1u:rng()&0xffffff00u);
        setv(0x428320,rnd(8)?rnd(1344):0xffffffffu);setv(0x428460,rnd(8)?rnd(1344):0xffffffffu);setv(0x428880,1u+rnd(4));
        setv(0x4035f0,Obj);setv(0x4536f0,rnd(3)?0u:1u<<rnd(8));setv(0x4506a0,rnd(4)?0u:1u);
        setv(0x496180,rnd(2)?rng()|1u:rng()&0xffffff00u);setv(0x4e8620,rnd(2)?rng()|1u:rng()&0xffffff00u);setv(0x499890,rnd(2));
        std::memcpy(reinterpret_cast<void*>(values),value.data(),value.size()*4);
        // globals
        for(auto& mi:mirrors)for(std::uint32_t a=mi.base;a<mi.end;a+=4)if(mi.end-a>=4)p32(a,rnd(3)?rnd(4):rng());else *reinterpret_cast<std::uint8_t*>(a)=std::uint8_t(rng());
        p32(0x780258u,rnd(10));p32(0x78024cu,rnd(4));p32(0x799d18u,Player);p32(Player+4,rng());
        p32(0x8a8cdcu,rnd(4)?0u:1u);p32(0x65994cu,rnd(2));p32(0x836d10u,rnd(5));p32(0x836d08u,rnd(4));
        p32(0x7f1964u,rnd(3)?0u:rng());
        p32(0x687774u,rnd(3)?0xffffffffu:rnd(1344));p32(0x687778u,rnd(3)?0xffffffffu:rnd(1344));p32(0x842888u,rnd(4));
        *reinterpret_cast<std::uint16_t*>(0x7d6728u)=std::uint16_t(rnd(12));
        p32(Obj+0x518u,rnd(3)?0u:(rnd(2)?1u:0xffffffffu));
        // route block
        p32(0x6840f0u,rnd(5)?rnd(15):0xffffffffu);p32(0x6840ecu,rnd(2));pf(0x6840f4u,rnd(8)?real(0,1):-1.0f);
        p32(0x8420fcu,rnd(5)?0u:1u);p32(0x8420e0u,rnd(10));p32(0x8420e8u,rnd(6)?0x28u:0x10u+rnd(0x30));
        p32(0x8420e4u,rnd(3)?g32(0x8420e8u)-0xeu:rnd(0x2a));
        for(unsigned k=0;k<24;++k)pf(0x842050u+k*4,rnd(8)?real(-250,250):-10000.0f);
        for(unsigned k=0;k<12;++k)p32(0x8420b0u+k*4,rnd(3));
        p32(0x8420f0u,0x340000u|rnd(0x20));p32(0x8420ecu,rnd(1344));p32(0x8420f4u,rnd(1344));p32(0x8420f8u,rnd(1344));
        // console flow / Miles
        p32(0x836d28u,rnd(3));p32(0x836d10u,rnd(4));
        for(unsigned d=0;d<8;++d)*reinterpret_cast<std::uint8_t*>(0x836d1cu+d)=std::uint8_t(rnd(4)?rnd(30):rng());
        *reinterpret_cast<std::uint8_t*>(0x836d18u)=std::uint8_t(rnd(2));
        p32(0x84bcf4u,rnd(3)?rnd(100000):rng());p32(0x84bcf8u,rnd(1000));*reinterpret_cast<std::uint8_t*>(0x84bcfcu)=std::uint8_t(rnd(2));
        pf(0x84bd00u,real(-5,50));p32(Player+0xe78u,rnd(3)?rnd(5000):rng());
        for(unsigned k=0;k<0x2c;++k)*reinterpret_cast<std::uint8_t*>(0x8367c0u+k)=std::uint8_t(rnd(3)==0&&k<0x1eu?1u:0u);
        for(std::uint32_t a=0x8369b4u;a<0x836ce0u;a+=4)p32(a,0);
        p32(0x8369b4u,0x8369c8u+rnd(8)*12u);
        for(unsigned k=0;k<0x100;++k)p32(Fix+k*4,(k%2==0&&k<0x40)?rnd(0xcf):rng());   // PRICES pairs {key < 0xCF, value} / SETUP words
        setv(0x4f1a90,Fix);setv(0x4f1210,rnd(0x20));setv(0x4f12a0,1);
        setv(0x427700,rnd(3)?0u:1u);if(entry==37u)setv(0x450320,rnd(0x20));/* 499960 bit index stays inside the license */setv(0x496510,rnd(8));setv(0x495890,rnd(8));setv(0x4b99d0,rnd(3)?rnd(0x1000000):rng());
        std::memcpy(reinterpret_cast<void*>(values),value.data(),value.size()*4);
        // goal
        setv(0x44dc50,rnd(6));setv(0x450380,0xau+rnd(5));setv(0x44b7b0,rnd(2)?(rng()&0xffffff00u)|1u:rng()&0xffffff00u);
        setv(0x495820,rnd(2));setv(0x4493c0,rnd(3)?0u:1u);setv(0x4872e0,rnd(3)?0u:1u);setv(0x43f910,rnd(2)?rng():rnd(10));
        setv(0x456d40,rnd(2)?0xfffffd00u+rnd(0x200):rng());setv(0x495b20,rnd(8));
        p32(0x635f34u,rnd(3)?0x42u:0x3eu+rnd(4));p32(0x8369b8u,rnd(3)?rnd(0x300):0x24eu);
        p32(0x836d14u,rnd(3)?3u:rng());for(std::uint32_t a=0x7d68bcu;a<0x7d68d4u;a+=4)p32(a,0);
        if(rnd(4)==0)*reinterpret_cast<std::uint8_t*>(0x7d68d2u)=1;
        for(std::uint32_t a=0x8369c8u;a<0x836ce0u;a+=4)p32(a,0);
        std::memcpy(reinterpret_cast<void*>(0x651758u),file.data()+pe_file_offset(file,0x651758u),0x2030);
        // GAD_PUB displays: banner handle, voice flag, Miles digit layout, game mode.
        p32(0x836644u,rnd(3)?0xffffffffu:rnd(1344));p32(0x67ee3cu,rnd(3)?1u:rnd(3));
        pf(0x67ee40u,real(0,640));pf(0x67ee44u,real(0,480));pf(0x67ee48u,real(10,40));
        p32(0x78026cu,rnd(4)?13u+rnd(23):rnd(64));p32(0x67f638u,rnd(5)?rnd(1344):0xffffffffu);
        for(unsigned d=0;d<8;++d)*reinterpret_cast<std::uint8_t*>(0x836d1cu+d)=std::uint8_t(rnd(4)?rnd(10):rng());   // camera scripts: the EXE .data
        // START display 499A30: frame counter, players, network player list.
        p32(0x836ce4u,rnd(3)?0x28u+rnd(0x30):rng());*reinterpret_cast<std::uint16_t*>(0x8367f0u)=std::uint16_t(rnd(4)?rnd(5):rng());
        setv(0x456d60,(rng()&0xffffff00u)|(rnd(4)?rnd(5):rng()&0xffu));setv(0x455ae0,rng());
        setv(0x4591f0,(rng()&0xffffff00u)|rnd(0x24));setv(0x4591c0,(rng()&0xffffff00u)|(1u+rnd(8)));
        // GOAL display 498670: variant, banner, steps, slides, tables in range.
        p32(0x780258u,rnd(6)?rnd(10):rnd(16));if((entry==43u||entry==44u)&&rnd(3))p32(0x780258u,(const std::uint32_t[]){0u,1u,7u,8u,9u}[rnd(5)]);
        p32(0x836694u,rnd(3)?0xffffffffu:rnd(1344));p32(0x836650u,rnd(3)?0xffffffffu:rnd(1344));
        *reinterpret_cast<std::uint8_t*>(0x836712u)=std::uint8_t(rnd(3));*reinterpret_cast<std::uint16_t*>(0x836714u)=std::uint16_t(rnd(1344));
        *reinterpret_cast<std::uint8_t*>(0x67ef6du)=std::uint8_t(rnd(3));setv(0x46c440,rnd(7));if(entry==44u||entry==41u)setv(0x44c940,1u+rnd(7));*reinterpret_cast<std::uint8_t*>(0x67ee3au)=std::uint8_t(rnd(2));
        *reinterpret_cast<std::uint16_t*>(0x836698u)=std::uint16_t(rnd(6));*reinterpret_cast<std::uint16_t*>(0x836630u)=std::uint16_t(rnd(0x200));
        *reinterpret_cast<std::uint16_t*>(0x836710u)=std::uint16_t(rnd(1344));p32(0x83669cu,0x5c1e00u);
        for(std::uint32_t q:{0x8366c4u,0x8366d4u}){pf(q,real(300,460));pf(q+4,real(0,30));pf(q+8,real(-25,25));p32(q+12,rnd(4));}
        *reinterpret_cast<std::uint8_t*>(0x67ef6cu)=std::uint8_t(rnd(3));
        for(std::uint32_t e=0x7f2588u;e<0x7f26dcu;e+=0x44u){const std::uint32_t d=rnd(4);*reinterpret_cast<std::uint8_t*>(e)=std::uint8_t(d);p32(e-0x24u,d*rnd(8));}
        setv(0x48b350,rnd(2)?rng()|1u:rng()&0xffffff00u);setv(0x465f70,rnd(2)?rng()|1u:rng()&0xffffff00u);setv(0x428880,rnd(3));
        setv(0x43f910,rnd(3)?rnd(0x420):rng());setv(0x4bea50,rnd(3)?0u:rng());setv(0x427700,rnd(3)?0u:1u+rnd(5));
        setv(0x4294c0,[&]{const float f=real(-50,50);std::uint32_t u;std::memcpy(&u,&f,4);return u;}());setv(0x43fa00,rnd(3));
        // 4979E0 stage table
        p32(0x8366e8u,rnd(4)?rnd(600):rng());p32(0x67ef64u,1u+rnd(6));p32(0x67ef68u,1u+rnd(0x40));p32(0x67ef60u,rnd(3)?0xffffffffu:rnd(16));
        pf(Player+0x102cu,real(0,1.2f));p32(Fix+0x3f0u,rnd(8));
        setv(0x4ef710,rnd(16));setv(0x4506a0,rnd(3)?1u:0u);setv(0x47ee70,rnd(2));setv(0x44c8d0,rnd(3)?0u:Fix+0x3f0u);
        for(std::uint32_t a=0x836658u;a<0x836698u;a+=4)if(rnd(2))p32(a,0xffffffffu);
        // 497560 ranking rows: racer record {+4 time, +14 value, +18 name} in Fix.
        p32(Fix+0x384u,rnd(4)?rnd(400000):rng());p32(Fix+0x394u,rng());p32(Fix+0x398u,rnd(2)?Fix+0x3a0u:0u);
        std::memcpy(reinterpret_cast<void*>(Fix+0x3a0u),"RIVAL 7",8);
        setv(0x476710,rnd(4)?Fix+0x380u:0u);
        setv(0x496510,rnd(8));setv(0x496440,rnd(9));setv(0x45bf30,rnd(8));setv(0x45b840,1u+rnd(20000));setv(0x495b20,rnd(8));
        std::memcpy(reinterpret_cast<void*>(values),value.data(),value.size()*4);
        // OUTRUN2SP modes: previous mode, attract state and the event-4 work (Fix + 0x200, sequence Fix + 0x300).
        setv(0x43f8f0,(const std::uint32_t[]){7u,12u,rnd(40)}[rnd(3)]);setv(0x4532e0,rnd(2)?2u:rnd(4));setv(0x448980,rnd(2));setv(0x45a920,rnd(2));
        setv(0x46bbe0,rnd(0x223));setv(0x43f980,rnd(6)?0u:1u);
        p32(0x830368u,rnd(4)?rnd(10):rng());p32(0x830380u,rnd(4)?0u:1u);p32(0x799c28u,Fix+0x200u);p32(Fix+0x200u,Fix+0x300u);
        *reinterpret_cast<std::uint8_t*>(Fix+0x215u)=std::uint8_t(rnd(8));for(unsigned k=0;k<16;++k)p32(Fix+0x300u+k*4,rnd(12));
        for(unsigned k=0;k<12;++k)p32(0x655b5cu+k*4,k+1u);
        if(entry==47u)*reinterpret_cast<std::uint8_t*>(0x82e7d4u)=std::uint8_t(rnd(2)?0xffu:rnd(0x24));   // 487240: a 653788 scene
        std::memcpy(reinterpret_cast<void*>(values),value.data(),value.size()*4);
        if(entry>=56u){
            p32(0x67ee4cu,rnd(9));p32(0x67ee50u,rnd(4));
            p32(0x8366acu,rnd(4)?rnd(201):0xffffffffu);
            for(unsigned a=0x8366b0u;a<0x8366c0u;a+=4)p32(a,rnd(2)?0xffffffffu:rnd(64));
            setv(0x428880,rnd(2)?4u:rnd(4));
            setv(0x427700,rnd(2));setv(0x4536f0,rnd(2)?4u:0u);
            std::memcpy(reinterpret_cast<void*>(values),value.data(),value.size()*4);
        }
        const std::uint32_t kind=entry==31u?rnd(11):rnd(4);
        for(auto& mi:mirrors){mi.bytes.resize(mi.end-mi.base);std::memcpy(mi.bytes.data(),reinterpret_cast<void*>(mi.base),mi.bytes.size());}
        static constexpr std::uint32_t Pc[]{0x49a1e0,0x49a280,0x49d1e0,0x49a790,0x49a800,0x49a920,0x49adc0,0x49aea0,0x49aed0,0x49b170,0x49af90,
            0x4ae750,0x4ae960,0x4aed20,0x4aed50,0x4aeec0,0x4aeee0,0x4b72f0,0x4bfb20,0x45aef0,0x4b7630,
            0x49b470,0x49b570,0x49b7c0,0x49b7e0,0x49d9c0,0x49dae0,0x49b190,0x49cdf0,0x499bb0,0x4ef280,0x4ef550,0x4ef510,
            0x49c9c0,0x49cc50,0x49d4a0,0x49d850,0x49cfd0,0x4b1680,0x497960,0x497fa0,0x4998c0,0x499a30,0x498670,0x499020,
            0x48ae40,0x49f360,0x49f370,0x48ae50,0x48b210,0x48ae90,0x48b070,0x48b0b0,0x48b0f0,0x48b120,0x48b2a0,
            0x4981a0,0x4981f0,0x4983d0};
        const std::uint32_t stack_route=rnd(4)?rnd(0x10):rng(),stack_level=rnd(7);
        prepare(Pc[entry]);reinterpret_cast<unsigned*>(S)[0]=entry==38u?stack_route:kind;reinterpret_cast<unsigned*>(S)[1]=stack_level;
        run();const std::uint32_t guest_eax=guest_call.out_eax;
        // native
        PcRaceMemory mem;std::vector<std::vector<std::uint8_t>> native(mirrors.size());
        for(std::size_t i=0;i<mirrors.size();++i){native[i]=mirrors[i].bytes;mem.map(mirrors[i].base,native[i].data(),native[i].size());}
        race_end_map_tables(mem);
        std::array<std::uint8_t,0x40> mb{};outrun::driving::PcMatrixStack stack{outrun::driving::Bytes(mb.data(),mb.size())};
        struct NCall{std::uint32_t pc,ecx;decltype(PcRaceCall::args) a;std::array<std::uint32_t,16> mat;};std::vector<NCall> calls;
        PcRaceContext ctx{mem,stack,[&](const PcRaceCall& k)->std::uint32_t{
            const auto it=index_of.find(k.pc);if(it==index_of.end()){std::ostringstream s;s<<"race end probe: unexpected service "<<std::hex<<k.pc;throw std::runtime_error(s.str());}
            NCall n{k.pc,k.ecx,k.args,{}};
            if(services[it->second].kind==1)for(unsigned w=0;w<16;++w)n.mat[w]=mem.u32(k.args[1]+w*4);
            if(services[it->second].kind==7){mem.put32(k.args[1],value[it->second]);mem.put32(k.args[2],value[it->second]);}
            if(services[it->second].kind==8)for(unsigned w=0;w<16;++w)mem.put32(k.args[0]+w*4,value[it->second]);
            calls.push_back(n);return value[it->second];},nullptr};
        std::uint32_t native_eax=0;bool ok=true,undef=false;std::string why;
        try{
            switch(entry){
            case 0:mode20_init_49a1e0(ctx);break;       case 1:mode20_control_49a280(ctx);break;
            case 2:mode20_exit_49d1e0(ctx);break;       case 3:mode25_init_49a790(ctx);break;
            case 4:mode25_control_49a800(ctx);break;    case 5:mode25_exit_49a920(ctx);break;
            case 6:mode27_init_49adc0(ctx);break;       case 7:mode27_control_49aea0(ctx);break;
            case 8:mode27_exit_49aed0(ctx);break;       case 9:mode28_init_49b170(ctx);break;
            case 10:mode28_control_49af90(ctx);break;   case 11:route_init_4ae750(ctx);break;
            case 12:route_control_4ae960(ctx);break;    case 13:route_destroy_4aed20(ctx);break;
            case 14:ta_result_init_4aed50(ctx);break;   case 15:ta_result_control_4aeec0(ctx);break;
            case 16:ta_result_destroy_4aeee0(ctx);break;case 17:common_anim_4b72f0(ctx,kind);break;
            case 18:native_eax=result_input_4bfb20(ctx);break;
            case 19:adv_release_45af40(ctx);break;      case 20:common_anim_release_4b7630(ctx);break;
            case 21:mode34_init_49b470(ctx);break;      case 22:mode34_control_49b570(ctx);break;
            case 23:mode34_exit_49b7c0(ctx);break;      case 24:mode35_init_49b7e0(ctx);break;
            case 25:mode35_control_49d9c0(ctx);break;   case 26:mode35_exit_49dae0(ctx);break;
            case 27:mode30_control_49b190(ctx);break;   case 28:race_close_49cdf0(ctx);break;
            case 29:model_release_499bb0(ctx);break;    case 30:miles_init_4ef280(ctx);break;
            case 31:miles_compute_4ef550(ctx,kind);break;case 32:miles_commit_4ef510(ctx);break;
            case 33:mode19_init_49c9c0(ctx);break;      case 34:mode19_control_49cc50(ctx);break;
            case 35:mode33_init_49d4a0(ctx);break;      case 36:mode33_control_49d850(ctx);break;
            case 37:mode19_exit_49cfd0(ctx);break;
            case 38:native_eax=goal_index_4b1680(stack_route,stack_level);break;
            case 39:gad_game_over_497960(ctx);break;
            case 40:miles_digits_497fa0(ctx);break;
            case 42:gad_start_499a30(ctx);break;
            case 43:gad_goal_498670(ctx);break;
            case 44:gad_time_over_499020(ctx);break;
            case 45:mode7_init_48ae40(ctx);break;       case 46:mode7_control_49f360(ctx);break;
            case 47:mode7_exit_49f370(ctx);break;       case 48:mode9_control_48ae50(ctx);break;
            case 49:mode10_init_48b210(ctx);break;      case 50:mode10_control_48ae90(ctx);break;
            case 51:mode10_exit_48b070(ctx);break;      case 52:mode11_control_48b0b0(ctx);break;
            case 53:mode12_init_48b0f0(ctx);break;      case 54:mode12_control_48b120(ctx);break;
            case 55:mode12_exit_48b2a0(ctx);break;
            case 56:mode36_init_4981a0(ctx);break;
            case 57:mode36_control_4981f0(ctx);break;
            case 58:mode36_exit_4983d0(ctx);break;
            default:gad_display_4998c0(ctx);break;
            }
        }catch(const PcRaceEndUndefined&){undef=true;}
        catch(const std::exception& e){ok=false;why=e.what();}
        const std::string tag="entry "+std::to_string(entry)+" run "+std::to_string(iter);
        if(undef){++undefined;++coverage["undefined stack read"];continue;}
        ++cases;if(!ok){fail(tag+" native threw: "+why);continue;}
        if(inject&&iter==11)for(std::size_t i=0;i<mirrors.size();++i)if(mirrors[i].base==0x842048u)native[i][0x98]^=1;
        for(std::size_t i=0;i<mirrors.size();++i){std::ostringstream s;s<<tag<<" region "<<std::hex<<mirrors[i].base;
            compare(native[i].data(),reinterpret_cast<void*>(mirrors[i].base),native[i].size(),s.str());}
        ++cases;if((entry==18||entry==38)&&native_eax!=guest_eax){std::ostringstream s;s<<tag<<" EAX PC="<<guest_eax<<" native="<<native_eax;fail(s.str());}
        const unsigned n=g32(trace);++cases;
        if(n!=calls.size()){std::ostringstream s;s<<tag<<" service count PC="<<n<<" native="<<calls.size();
            for(unsigned i=0;i<n&&i<40;++i)s<<" "<<std::hex<<g32(trace+64+i*0x60);s<<" | native";for(auto& cl:calls)s<<" "<<std::hex<<cl.pc;fail(s.str());}
        else for(unsigned i=0;i<n;++i){const auto* g=reinterpret_cast<const std::uint32_t*>(trace+64+i*0x60);
            const auto& e=services[index_of[g[0]]];bool same=g[0]==calls[i].pc;
            if(e.kind==3)same=same&&g[1]==calls[i].ecx;
            if(g[0]==0x55a930u||g[0]==0x443ea0u)same=same&&g[1]==calls[i].ecx;
            for(unsigned k=0;k<e.argc;++k){
                if(e.kind==1&&k==1)continue;                               // 4287B0: the matrix, not its stack address
                if(e.kind==7&&k>=1)continue;                               // 4294C0: output addresses
                if(e.kind==8&&k==0)continue;                               // 47EE70: its stack array
                // 49CFD0 pushes byte locals as dwords: only their low byte is defined.
                const bool byte_arg=(g[0]==0x4478f0u&&(k==0||k==3||k==4))||((g[0]==0x447750u||g[0]==0x447a60u||g[0]==0x447c10u)&&(k==0||k==4||k==5));
                same=same&&(byte_arg?(g[2+k]&0xffu)==(calls[i].a[k]&0xffu):g[2+k]==calls[i].a[k]);
            }
            if(e.kind==1)for(unsigned w=0;w<16;++w)same=same&&g[8+w]==calls[i].mat[w];
            if(!same){std::ostringstream s;s<<tag<<" service #"<<i<<" "<<std::hex<<g[0];for(unsigned k=0;k<e.argc;++k)s<<" PC "<<g[2+k]<<"/native "<<calls[i].a[k];fail(s.str());break;}}
        ++coverage["entry "+std::to_string(entry)];
        if(entry==10)++coverage["49AF90 stage "+std::to_string(std::min(g32(0x836d10u),4u))];
        if(entry==4)++coverage["49A800 stage "+std::to_string(std::min(g32(0x836d08u),3u))];
        if(entry==44)++coverage["499020 variant "+std::to_string(std::min(g32(0x780258u),10u))+(calls.size()>12?" long":"")];
        if(entry==43){std::uint32_t texts=0;for(auto& cl:calls)texts+=cl.pc==0x42ccc0u;
            ++coverage["498670 variant "+std::to_string(std::min(g32(0x780258u),10u))+(texts?" text":"")+(calls.size()>12?" long":"")];}
        if(entry==43||entry==41)for(auto& cl:calls){
            if(cl.pc==0x42d200u)++coverage["4979E0 bar"];if(cl.pc==0x4505a0u)++coverage["4979E0 split time"];
            if(cl.pc==0x429530u&&cl.a[0]==0x2c004du)++coverage["4979E0 marker"];if(cl.pc==0x47ee70u)++coverage["4979E0 47EE70"];
            if(cl.pc==0x428320u&&cl.a[0]==0x2c00bdu)++coverage["4979E0 new record"];if(cl.pc==0x4287b0u)++coverage["4287B0 matrix"];}
    }
    std::cout<<"{\"routine\":\""<<only<<"\",\"coverage\":{";bool first=true;
    for(const auto& [k,v]:coverage){std::cout<<(first?"":",")<<"\""<<k<<"\":"<<v;first=false;}
    std::cout<<"},\"total_cases\":"<<cases<<",\"undefined\":"<<undefined<<",\"total_mismatching_cases\":"<<mismatches<<"}"<<std::endl;
    return mismatches?1:0;
}

// Included inside the compare_pc dispatcher; uses its private x86 harness.
if(only=="race_objects_probe"){
    // Course objects (OSO, race_traffic_objects.inc) against the original: every event
    // callback of the event functions 0x3D..0x42, 0x4A..0x4F, 0x5C..0x65 and the controls
    // they switch to. The module (4A8A60..4ACE40, 4F0660, 43F3D0, 43E790, the rigid body
    // 516E10..519BD0, the motion sampler 5136C0) runs unmodified with its protected bridges
    // live (4A90B7, 4AA14F, 517A40, 4480D7); every callee outside it is a capture stub
    // recording {pc, ECX, stack arguments} and returning a scripted value: 43EB60 writes
    // [arg4] = value, the point's y, [arg2] = value3, [arg3] = value4; 43D1D0 / 502E20 write
    // their four vectors; 429780 its two words; the draw leaves (405360, 4044A0, 4044C0,
    // 4044E0, 4056D0) record the current matrix, 4289E0 / 428AC0 their matrix argument.
    // Compared: the object work and the mirrored regions (cars, records, shapes 6812D0..,
    // events, matrices), the matrix arena and the ordered service log.
    using namespace outrun::platform;
    if(argc<=7||std::string(argv[7])=="-")throw std::runtime_error("race objects probe requires the pinned d3dx9_29.dll");
    constexpr unsigned base=0x3a800000u,stubs=base,trace=base+0x8000u,values=base+0x18000u,values2=base+0x19000u,values3=base+0x1a000u,values4=base+0x1b000u;
    constexpr unsigned Objs=base+0x20000u,Recs=base+0x28000u,Handles=base+0x2b000u,Anim=base+0x2c000u,Cars=base+0x30000u,Camera=base+0x48000u,
        CornerVals=base+0x49000u,Light=base+0x49100u,LenTab=base+0x4a000u,MtxTrace=base+0x50000u;
    map_at(base,0x60000,PROT_READ|PROT_WRITE|PROT_EXEC);map_original_arithmetic_bridge(file);
    {   unsigned restored=0;const std::size_t text_file=pe_file_offset(file,0x401000u);
        for(std::uint32_t a=0x401000u;a<0x401000u+0x194881u;a+=0x100u){
            const std::size_t n=std::min<std::size_t>(0x100u,0x401000u+0x194881u-a);
            if(std::memcmp(reinterpret_cast<void*>(a),file.data()+text_file+(a-0x401000u),n)){r060_raw_write(a,file.data()+text_file+(a-0x401000u),n);++restored;}
        }
        std::cerr<<"restored "<<restored<<" patched .text blocks"<<char(10);
    }
    init_matrix_oracle_globals();wire_d3dx_imports(map_d3dx29(argv[7]));
    {   static const std::uint8_t finite_stub[]{0x8b,0x44,0x24,0x08,0x25,0x00,0x00,0xf0,0x7f,0x3d,0x00,0x00,0xf0,0x7f,0x0f,0x95,0xc0,0x0f,0xb6,0xc0,0xc3};
        std::memcpy(reinterpret_cast<void*>(base+0x7f00u),finite_stub,sizeof(finite_stub));
        auto* iat=reinterpret_cast<void*>(D3DX_BASE+0x1000u);
        if(mprotect(iat,4096,PROT_READ|PROT_WRITE))throw std::runtime_error("d3dx IAT page");
        *reinterpret_cast<std::uint32_t*>(D3DX_BASE+0x1270u)=base+0x7f00u;
        if(mprotect(iat,4096,PROT_READ))throw std::runtime_error("d3dx IAT restore");
        auto* page=reinterpret_cast<void*>(0x596000u);
        if(mprotect(page,4096,PROT_READ|PROT_WRITE))throw std::runtime_error("IAT inverse");
        *reinterpret_cast<std::uint32_t*>(0x5962dcu)=D3DX_BASE+(0x440e9cu-0x400000u);
        *reinterpret_cast<std::uint32_t*>(0x5962b4u)=D3DX_BASE+(0x441288u-0x400000u);   // D3DXMatrixScaling (40A360, generic)
        if(mprotect(page,4096,PROT_READ))throw std::runtime_error("IAT inverse");
    }
    for(std::uint32_t page=0x62f000u;page<0x98d000u;page+=0x1000u)mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE);
    auto emit32=[](std::vector<std::uint8_t>& b,std::uint32_t v){for(unsigned k=0;k<4;++k)b.push_back(std::uint8_t(v>>(k*8)));};
    // kind: 0 plain, 2 x87 float result, 10 43EB60, 11 43E570, 17 matrix [arg1], 18 429780,
    // 19 matrix [arg2], 20 current matrix, 21 four vectors [arg2..arg5]
    struct Svc{std::uint32_t pc;unsigned argc,kind;};
    const std::vector<Svc> services{
        {0x440a60,2,0},{0x440b80,0,0},{0x440b90,1,0},{0x440bd0,2,0},{0x440b20,0,0},{0x4401d0,1,0},
        {0x43eb60,5,10},{0x43e570,3,11},{0x43d1d0,6,21},{0x502e20,6,21},
        {0x424940,1,0},{0x429530,5,0},{0x429780,3,18},{0x4289e0,5,19},{0x428ac0,6,17},
        {0x405360,6,20},{0x4044a0,0,20},{0x4044c0,0,20},{0x4044e0,1,20},{0x4056d0,4,20},
        {0x4082b0,3,0},{0x410740,0,0},{0x4107a0,1,0},{0x4493e0,0,2},{0x55a930,0,0},{0x45c440,0,0},
        {0x44dc50,1,0},{0x44ddc0,2,0},{0x44bdd0,0,0},{0x580f33,1,0},{0x580f40,0,0},{0x417f70,0,0}};
    std::map<std::uint32_t,unsigned> index_of;unsigned stub_off=0;
    for(std::size_t k=0;k<services.size();++k){
        const auto& e=services[k];index_of[e.pc]=unsigned(k);
        std::vector<std::uint8_t> c{0x57,0x56,0x51,0x8b,0x3d};emit32(c,trace);            // push edi/esi/ecx; mov edi,[count]
        c.insert(c.end(),{0x6b,0xff,0x40,0x81,0xc7});emit32(c,trace+64);                    // imul edi,edi,0x40; add edi,entries
        c.insert(c.end(),{0xc7,0x07});emit32(c,e.pc);                                       // [edi]=pc
        c.insert(c.end(),{0x89,0x4f,0x04});                                                 // [edi+4]=ecx
        for(unsigned i=0;i<e.argc;++i)c.insert(c.end(),{0x8b,0x44,0x24,std::uint8_t(0x10+4*i),0x89,0x47,std::uint8_t(0x08+4*i)});
        if(e.kind==18){c.insert(c.end(),{0x8b,0x74,0x24,0x14,0x85,0xf6,0x74,0x08,0x8b,0x0d});emit32(c,values2+4*unsigned(k));c.insert(c.end(),{0x89,0x0e});
            c.insert(c.end(),{0x8b,0x74,0x24,0x18,0x85,0xf6,0x74,0x08,0x8b,0x0d});emit32(c,values3+4*unsigned(k));c.insert(c.end(),{0x89,0x0e});}
        if(e.kind==17||e.kind==19||e.kind==20){   // matrix -> MtxTrace + count*64
            if(e.kind==20){c.insert(c.end(),{0x8b,0x35});emit32(c,0x89b564u);}               // esi = current matrix
            else c.insert(c.end(),{0x8b,0x74,0x24,std::uint8_t(e.kind==17?0x14:0x18)});     // esi = [arg1] / [arg2]
            c.insert(c.end(),{0x8b,0x0d});emit32(c,trace);c.insert(c.end(),{0xc1,0xe1,0x06,0x81,0xc1});emit32(c,MtxTrace);
            c.insert(c.end(),{0x57,0x89,0xcf,0xb9,0x10,0,0,0,0xf3,0xa5,0x5f});}
        if(e.kind==21){for(unsigned j=0;j<4;++j){   // [arg2+j] = CornerVals + j*12 (3 dwords)
            c.insert(c.end(),{0x57,0x8b,0x7c,0x24,std::uint8_t(0x1c+4*j),0xbe});emit32(c,CornerVals+j*12u);c.insert(c.end(),{0xb9,3,0,0,0,0xf3,0xa5,0x5f});}}
        c.insert(c.end(),{0xff,0x05});emit32(c,trace);                                      // inc [count]
        c.push_back(0xa1);emit32(c,values+4*unsigned(k));                                   // eax=value
        if(e.kind==2){c.insert(c.end(),{0xd9,0x05});emit32(c,values+4*unsigned(k));}       // fld dword [value]
        if(e.kind==10){
            c.insert(c.end(),{0x8b,0x74,0x24,0x20,0x85,0xf6,0x74,0x02,0x89,0x06});          // esi=arg4; if(esi)[esi]=eax
            c.insert(c.end(),{0x8b,0x74,0x24,0x14,0x8b,0x0d});emit32(c,values2+4*unsigned(k));   // esi=arg1; ecx=value2
            c.insert(c.end(),{0x89,0x4e,0x04});                                             // [esi+4]=ecx
            c.insert(c.end(),{0x8b,0x74,0x24,0x18,0x85,0xf6,0x74,0x08,0x8b,0x0d});emit32(c,values3+4*unsigned(k));   // esi=arg2; if(esi)[esi]=value3
            c.insert(c.end(),{0x89,0x0e});
            c.insert(c.end(),{0x8b,0x74,0x24,0x1c,0x85,0xf6,0x74,0x08,0x8b,0x0d});emit32(c,values4+4*unsigned(k));   // esi=arg3; if(esi)[esi]=value4
            c.insert(c.end(),{0x89,0x0e});
        }
        if(e.kind==11){c.insert(c.end(),{0x8b,0x74,0x24,0x10,0x8b,0x0d});emit32(c,values2+4*unsigned(k));c.insert(c.end(),{0x89,0x4e,0x54,0x89,0x4e,0x08,0x8b,0x0d});
            emit32(c,values3+4*unsigned(k));c.insert(c.end(),{0x89,0x4e,0x0c,0x8b,0x0d});emit32(c,values4+4*unsigned(k));c.insert(c.end(),{0x89,0x4e,0x10});
            c.insert(c.end(),{0x57,0x8d,0x7e,0x24,0xbe});emit32(c,CornerVals+0x30u);c.insert(c.end(),{0xb9,0x0c,0,0,0,0xf3,0xa5,0x5f});}   // corners [arg0+24..53]
        c.insert(c.end(),{0x59,0x5e,0x5f,0xc3});
        std::memcpy(reinterpret_cast<void*>(stubs+stub_off),c.data(),c.size());
        r046_write_rel32(e.pc,0xe9,stubs+stub_off);stub_off=(stub_off+unsigned(c.size())+15u)&~15u;
    }
    std::mt19937 rng(0x4a8a9000u);
    auto rnd=[&](std::uint32_t n){return n?std::uint32_t(rng()%n):0u;};
    auto real=[&](float a,float b){return a+(b-a)*float(rng()%100000)/100000.f;};
    auto fb=[](float f){std::uint32_t u;std::memcpy(&u,&f,4);return u;};
    auto g32=[](std::uint32_t a){return *reinterpret_cast<std::uint32_t*>(a);};
    auto p32=[](std::uint32_t a,std::uint32_t v){*reinterpret_cast<std::uint32_t*>(a)=v;};
    auto p16=[](std::uint32_t a,std::uint32_t v){*reinterpret_cast<std::uint16_t*>(a)=std::uint16_t(v);};
    auto p8=[](std::uint32_t a,std::uint32_t v){*reinterpret_cast<std::uint8_t*>(a)=std::uint8_t(v);};
    auto pf=[](std::uint32_t a,float v){std::memcpy(reinterpret_cast<void*>(a),&v,4);};
    unsigned cases{},mismatches{};const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
    std::map<std::string,unsigned> coverage;
    auto fail=[&](const std::string& what){++mismatches;if(mismatches<20)std::cerr<<what<<"\n";};
    auto compare=[&](const void* native,const void* original,std::size_t n,const std::string& what){
        ++cases;if(!std::memcmp(native,original,n))return;
        const auto* a=static_cast<const std::uint8_t*>(native);const auto* b=static_cast<const std::uint8_t*>(original);
        for(std::size_t i=0;i<n;++i)if(a[i]!=b[i]){std::ostringstream s;s<<what<<" offset=0x"<<std::hex<<i<<" PC="<<unsigned(b[i])<<" native="<<unsigned(a[i]);
            const std::size_t o=i&~std::size_t(3);s<<" dwords:";const std::size_t lim=std::getenv("OR2_OBJECTS_ALL")?n:o+0x20;unsigned shown=0;
            for(std::size_t q=o;q<lim&&q+4<=n&&shown<48;q+=4){std::uint32_t pa,na;std::memcpy(&pa,b+q,4);std::memcpy(&na,a+q,4);if(pa!=na){s<<" +"<<q<<":"<<pa<<"/"<<na;++shown;}}
            fail(s.str());break;}
    };
    struct Mirror{std::uint32_t base,end;std::vector<std::uint8_t> bytes;};
    std::vector<Mirror> mirrors{{Objs,Objs+0x8000u,{}},{Recs,Recs+0x2800u,{}},{Handles,Handles+0x40u,{}},{Anim,Anim+0x4000u,{}},{Cars,Cars+0x18000u,{}},
        {Camera,Camera+0x400u,{}},{CornerVals,CornerVals+0x60u,{}},{Light,Light+0x40u,{}},{LenTab,LenTab+0x1000u,{}},
        {0x681200u,0x681e00u,{}},{0x84bd00u,0x84d900u,{}},{0x799b30u,0x79fb48u,{}},{0x79fb48u,0x79fd50u,{}},{0x680ad4u,0x680ae0u,{}},{0x780200u,0x780280u,{}},
        {0x7f9460u,0x7f9610u,{}},{0x7d2da0u,0x7d2de0u,{}},{0x7d3180u,0x7d31d0u,{}},{0x95d860u,0x95d960u,{}},
        {0x599400u,0x599500u,{}},{0x5a2900u,0x5a2a00u,{}},{0x5a9100u,0x5a9200u,{}},{0x5b0000u,0x5b0400u,{}},{0x5b3cc0u,0x5b4600u,{}},{0x5c1c60u,0x5c1d50u,{}},
        {0x5c3700u,0x5c4100u,{}},{0x5da040u,0x5da2e0u,{}},{0x5e0a80u,0x5e0ac0u,{}},{0x619a00u,0x619b00u,{}},{0x628000u,0x628400u,{}},{0x7162c0u,0x7162d0u,{}},
        {MatrixArena,MatrixArena+0x2000u,{}},{0x89b560u,0x89b570u,{}}};
    static constexpr std::uint32_t Pc[]{
        0x4abcd0,0x4acd50,0x4acbc0,0x4ab160,0x4ac790,0x4ab1f0,0x4ab760,0x4acb00,0x4ab960,0x4ab9b0,0x4acde0,
        0x4aa560,0x4abf20,0x4aa620,0x4aafc0,0x4aa7d0,0x4ac200,0x4ac8c0,0x4aa920,0x4ab2e0,0x4aadc0,0x4ac440,
        0x4a8a60,0x4a8a70,0x4a8b50,0x4a8ba0,0x4a8c20,0x4a8c60,0x4a9c70,0x4a9c80,0x4a9f40,0x4abe40,0x4a9f80,0x4aa070,
        0x4a8a90,0x4a9030,0x4a9df0,0x4a8d80,0x4aef50,0x4a9010,0x4a9c50,0x4aada0,0x4ab2b0,0x5170d0,0x4abce0,0x4a9410,0x516f20,0x516fa0,0x517770,0x5178f0,0x517a40,0x517d90,0x4a92e0,0x43f3d0};
    const unsigned Entries=unsigned(sizeof(Pc)/sizeof(Pc[0]));
    static const std::uint32_t models[]{0u,0x57001au,0x57001cu,0x570021u,0x570022u,0x570023u,0x570024u,0x570025u,0x57002au,0x57002bu,0x57002cu,0x570031u,0x570039u,0x1ee0001u,0x1ee0004u,0x1ee000du};
    static const std::uint32_t kinds[]{0xau,0xdu,0xeu,0x11u,0x16u,0x17u,0x22u,0x38u,0x39u,0x3cu,0x3u,0x3eu};
    for(unsigned iter=0;iter<count;++iter){
        unsigned entry=iter%Entries;
        if(const char* e=std::getenv("OR2_OBJECTS_ENTRY"))entry=unsigned(std::stoul(e));
        std::memset(reinterpret_cast<void*>(trace),0,0x10000);
        std::vector<std::uint32_t> value(services.size()),value2(services.size()),value3(services.size()),value4(services.size());
        for(auto& v:value)v=rnd(3)?rnd(3):rng();
        for(auto& v:value2)v=fb(real(-5,5));
        for(auto& v:value3)v=rnd(4)?rnd(0x200):0xffffffffu;
        for(auto& v:value4)v=rnd(3)?rnd(0x2000):0xffffffffu;
        auto setv=[&](std::uint32_t pc,std::uint32_t v){value[index_of.at(pc)]=v;};
        // ---- world ------------------------------------------------------------
        p32(0x78026cu,rnd(4)?0x10u:rnd(0x20));
        p32(0x680ad4u,rnd(24));
        p32(0x7f94c0u,rnd(8)?0u:1u);
        const std::uint32_t w=Objs+rnd(8)*0x800u;
        setv(0x440a60,w);
        const std::uint32_t lane=rnd(2);
        const std::uint32_t id=lane?0x11au+rnd(0x1a):0x20u+rnd(0xfa);
        setv(0x440b80,id);
        setv(0x4493e0,fb(real(0,1)));setv(0x45c440,rnd(3)?(rnd(2)?0xfu:(rnd(2)?0x10u:0x19u)):rnd(0x20));
        setv(0x44dc50,rnd(2)?0x1cu:rnd(0x30));setv(0x44ddc0,rnd(2)?0xffffffffu:0x4fbu);setv(0x580f40,rnd(0x8000));
        setv(0x4082b0,Light);setv(0x43eb60,rnd(4));
        for(std::uint32_t k=0;k<4;++k)p32(0x780228u+k*4u,LenTab+k*0x200u);
        for(std::uint32_t k=0;k<0x1000;k+=2)p16(LenTab+k,rnd(4)?rnd(300):rng());
setv(0x55a930,rnd(6)?0u:1u);
        for(std::uint32_t k=0;k<0x18;++k)pf(CornerVals+k*4u,real(-6,6));
        for(std::uint32_t k=0;k<0x40;k+=4)pf(Light+k,real(0,1));
        // event records / flags: cars 8..31, the object's own record points at the work
        for(std::uint32_t i=0;i<410u;++i)p32(0x799b38u+i*0x3cu,0);
        for(std::uint32_t i=0;i<24;++i)p32(0x799b38u+(8u+i)*0x3cu,Cars+i*0x1000u);
        p32(0x799d18u,Cars);
        for(std::uint32_t i=0;i<0x200;++i)p8(0x79fb48u+i,i>=8u&&i<32u?(rnd(3)?2u:rnd(4)):(rnd(2)?0u:(rnd(2)?1u:3u)));   // live events have works
        p8(0x79fb50u,2);
        // other live objects (4ABF20 counts taken hearts among events 0x20..0x119)
        for(std::uint32_t i=0;i<6;++i){const std::uint32_t oid=0x20u+rnd(0xfa),ow=Objs+0x4000u+i*0x600u;
            p32(0x799b38u+oid*0x3cu,ow);p8(0x79fb48u+oid,2);
            for(std::uint32_t k=0;k<0x600;k+=4)p32(ow+k,rnd(3)?0u:rnd(4));
            p32(ow+0x5c,rnd(2)?kinds[rnd(9)]:rnd(0x40));p32(ow+0xa4,rnd(2));p32(ow+0x94,rnd(2));}
        p32(0x799b38u+id*0x3cu,w);
        p32(0x84d8e0u,rnd(9));
        // cars
        for(std::uint32_t i=0;i<24;++i){
            const std::uint32_t car=Cars+i*0x1000u;
            for(std::uint32_t k=0;k<0x1000;k+=4)p32(car+k,rnd(3)?0u:rnd(8));
            p32(car+4,rnd(4)?(rnd(2)?1u:0x21u):rng());p32(car+8,rng());
            for(std::uint32_t k:{0x14u,0x18u,0x1cu})pf(car+k,real(-12,12));
            for(std::uint32_t k:{0x16cu,0x170u,0x174u}){float v;std::memcpy(&v,reinterpret_cast<void*>(car+k-0x158u),4);pf(car+k,v+real(-1.5f,1.5f));}
            for(std::uint32_t k:{0x20u,0x24u,0x28u})pf(car+k,real(-3,3));
            p16(car+0x2c,rng());p16(car+0x2e,rng());p16(car+0x30,rng());p16(car+0x160,rng());
            for(std::uint32_t k=0;k<16;++k)pf(car+0x70+k*4u,(k%5u)==0u?real(0.7f,1.0f):(k>=12u&&k<15u?real(-12,12):real(-0.5f,0.5f)));
            pf(car+0x7c,0.0f);pf(car+0x8c,0.0f);pf(car+0x9c,0.0f);pf(car+0xac,1.0f);
            for(std::uint32_t k:{0xd28u,0xd2cu,0xd30u})pf(car+k,real(-6,6));
            pf(car+0x338,real(0.5f,2.5f));pf(car+0x344,real(-2.5f,-0.5f));pf(car+0x340,real(0.5f,3.5f));pf(car+0x34c,real(-3.5f,-0.5f));
            pf(car+0x1c4,real(0,3));p8(car+0xd36,rnd(3));p32(car+0x5c,rnd(3)?0u:1u);p32(car+0x68,rnd(5));
        }
        // camera
        p32(0x79f574u,Camera);
        for(std::uint32_t k=0;k<0x400;k+=4)pf(Camera+k,real(-1,1));
        pf(Camera+0xb0,real(0.6f,1.4f));pf(Camera+0xb4,real(0.6f,1.4f));
        for(std::uint32_t k:{0xf8u,0xfcu,0x100u})pf(Camera+k,real(-20,20));
        for(std::uint32_t k=0;k<16;++k)pf(Camera+0x140+k*4u,(k%5u)==0u?real(0.8f,1.2f):real(-0.3f,0.3f));
        pf(Camera+0x178,real(-20,-5));
        // course / screen matrices
        for(std::uint32_t k=0;k<16;++k){pf(0x7d2da0u+k*4u,(k%5u)==0u?1.0f:(k>=12u&&k<15u?real(-5,5):real(-0.1f,0.1f)));
            pf(0x7d3190u+k*4u,(k%5u)==0u?1.0f:(k>=12u&&k<15u?real(-5,5):real(-0.1f,0.1f)));
            pf(0x95d920u+k*4u,(k%5u)==0u?real(0.8f,1.2f):real(-0.2f,0.2f));}
        pf(0x95d95cu,1.0f);
        pf(0x7d3184u,real(0.5f,1.5f));
        // AREA object records / lane tables / motion
        p32(0x84d6ccu,Handles);p32(0x84d6d0u,Handles+4u);p32(Handles,Recs);p32(Handles+4u,Recs);
        for(std::uint32_t r=0;r<0xfa;++r){const std::uint32_t q=Recs+r*0x28u;
            for(std::uint32_t k=0;k<0xc;k+=4)pf(q+k,real(-10,10));
            for(std::uint32_t k=0xc;k<0x18;k+=4)pf(q+k,real(-3,3));
            p32(q+0x18,rnd(2)?kinds[rnd(12)]:rnd(0x3f));p32(q+0x1c,rng());p32(q+0x20,rnd(13));p32(q+0x24,rnd(4));}
        p32(0x84d6c0u,rnd(6)?Anim:0u);p32(0x84d6c4u,rnd(6)?Anim:0u);
        for(std::uint32_t e=0;e<8;++e){p32(Anim+e*8u,Anim+0x100u+e*0x40u);pf(Anim+e*8u+4u,real(0.5f,6));
            const std::uint32_t data=Anim+0x100u+e*0x40u;
            for(std::uint32_t ch=0;ch<6;++ch){const std::uint32_t keys=Anim+0x400u+(e*6u+ch)*0x40u;const std::uint32_t n=1u+rnd(4);
                p16(data+ch*8u,n);p32(data+ch*8u+4u,keys);float t=real(0,0.5f);
                for(std::uint32_t q=0;q<n;++q){pf(keys+q*16u,t);pf(keys+q*16u+4u,real(-3,3));pf(keys+q*16u+8u,real(-1,1));pf(keys+q*16u+12u,real(-1,1));t+=real(0.3f,2);}}}
        // the object work
        for(std::uint32_t k=0;k<0x800;k+=4)p32(w+k,rnd(3)?fb(real(-3,3)):rnd(8));
        for(std::uint32_t k=0;k<16;++k)pf(w+k*4u,(k%5u)==0u?real(0.7f,1.0f):(k>=12u&&k<15u?real(-8,8):real(-0.4f,0.4f)));
        for(std::uint32_t k:{0x40u,0x44u,0x48u})pf(w+k,real(-8,8));
        if(rnd(4)==0)pf(w+0x44,0.0f);
        pf(w+0x50,real(0,2));p32(w+0x54,rnd(4)?1u:rnd(4));p32(w+0x58,lane);
        p32(w+0x5c,rnd(3)?kinds[rnd(12)]:rnd(0x40));p32(w+0x60,rnd(3)?0xffffffffu:rnd(0x40));p32(w+0x64,models[rnd(16)]);
        for(std::uint32_t k:{0x68u,0x6cu,0x70u})pf(w+k,real(-3,3));
        pf(w+0x74,real(-0.5f,3));p32(w+0x78,rnd(3)?rnd(0x200):0u);p32(w+0x7c,rnd(4));p32(w+0x80,rnd(2)?0xffffffffu:rnd(4));
        p32(w+0x84,rnd(2));p32(w+0x88,rnd(3)?0u:1u);p32(w+0x8c,rnd(2));pf(w+0x90,real(0.3f,3));p32(w+0x94,rnd(3)?1u:0u);
        if(entry==18||entry==19||entry==33)pf(w+0x94,rnd(2)?0.0f:real(-0.2f,0.2f));
        p32(w+0x98,rnd(12));p32(w+0x9c,rnd(12));p32(w+0xa0,rnd(14));p32(w+0xa4,rnd(2));p32(w+0xa8,rnd(12));p32(w+0xac,rnd(12));p32(w+0xb0,rnd(12));
        for(std::uint32_t k:{0xb4u,0xb8u,0xbcu,0xc0u})pf(w+k,real(-8,8));
        if(entry==7||entry==8||entry==21){p32(w+0xbc,rnd(2));p32(w+0xc0,rnd(2));p32(w+0xc4,rnd(2));p32(w+0xc8,rnd(2));p32(w+0xd0,rnd(2));p32(w+0xd4,rnd(40));}
        p16(w+0x90,rnd(2));p16(w+0x92,rnd(2));
        // rigid body at +80 and the contact shape at +170
        {   const std::uint32_t rb=w+0x80;
            p32(rb,rnd(2));
            for(std::uint32_t k=0;k<16;++k)pf(rb+0x10+k*4u,(k%5u)==0u?real(0.8f,1.0f):(k>=12u&&k<15u?real(-8,8):real(-0.3f,0.3f)));
            pf(rb+0x1c,0.0f);pf(rb+0x2c,0.0f);pf(rb+0x3c,0.0f);pf(rb+0x4c,1.0f);
            for(std::uint32_t k=0x50;k<0x98;k+=4)pf(rb+k,real(-0.5f,0.5f));
            const float mass=real(0.5f,5);pf(rb+0x98,mass);pf(rb+0x9c,1.0f/mass);
            for(std::uint32_t k=0;k<3;++k){const float in=real(0.3f,3);pf(rb+0xa0+k*4u,in);pf(rb+0xac+k*4u,1.0f/in);}
            pf(rb+0xb8,real(0,0.8f));pf(rb+0xbc,real(0,0.8f));
            for(std::uint32_t k=0xc0;k<0xe4;k+=4)pf(rb+k,real(-0.3f,0.3f));
            const std::uint32_t shape=w+0x170;const std::uint32_t n=1u+rnd(26);
            p32(shape,n);
            for(std::uint32_t k=0;k<26u*12u;k+=4){pf(shape+4+k,real(-1.5f,1.5f));pf(shape+0x13c+k,real(-8,8));}
        }
        // cars at the object (hits / touches): car +D28 near the work or body position
        for(std::uint32_t i=0;i<24;++i){
            if(rnd(2))continue;
            const std::uint32_t car=Cars+i*0x1000u,src=rnd(2)?w+0x40u:w+0xc0u;
            for(std::uint32_t k=0;k<12;k+=4){float v;std::memcpy(&v,reinterpret_cast<void*>(src+k),4);pf(car+0xd28+k,v+real(-1.2f,1.2f));}
            for(std::uint32_t k=0;k<16;++k)pf(car+0x70+k*4u,(k%5u)==0u?1.0f:(k==15u?1.0f:(k>=12u?0.0f:real(-0.05f,0.05f))));
        }
        std::memcpy(reinterpret_cast<void*>(values),value.data(),value.size()*4);
        std::memcpy(reinterpret_cast<void*>(values2),value2.data(),value2.size()*4);
        std::memcpy(reinterpret_cast<void*>(values3),value3.data(),value3.size()*4);
        std::memcpy(reinterpret_cast<void*>(values4),value4.data(),value4.size()*4);
        reset_matrix_oracle_globals();
        for(auto& mi:mirrors){mi.bytes.resize(mi.end-mi.base);std::memcpy(mi.bytes.data(),reinterpret_cast<void*>(mi.base),mi.bytes.size());}
        if(std::getenv("OR2_OBJECTS_TRACE"))std::cerr<<"iter "<<iter<<" entry "<<entry<<char(10);
        // native
        PcRaceMemory mem;std::vector<std::vector<std::uint8_t>> native(mirrors.size());
        for(std::size_t i=0;i<mirrors.size();++i){native[i]=mirrors[i].bytes;mem.map(mirrors[i].base,native[i].data(),native[i].size());}
        std::vector<std::uint8_t> scratch(TrafficStackSize);mem.map(TrafficStackBase,scratch.data(),scratch.size());
        std::vector<std::uint8_t>& arena=native[mirrors.size()-2];
        outrun::driving::PcMatrixStack stack{outrun::driving::Bytes(arena.data(),0x1000),0,0,64};
        struct NCall{std::uint32_t pc,ecx;decltype(PcRaceCall::args) a;};std::vector<NCall> calls;std::map<std::size_t,std::array<std::uint32_t,16>> nmtx;
        PcRaceContext ctx{mem,stack,[&](const PcRaceCall& k)->std::uint32_t{
            const auto it=index_of.find(k.pc);if(it==index_of.end()){std::ostringstream s;s<<"race objects probe: unexpected service "<<std::hex<<k.pc;throw std::runtime_error(s.str());}
            const auto& e=services[it->second];
            if(e.kind==17||e.kind==19){const std::uint32_t mp=e.kind==17?k.args[1]:k.args[2];std::array<std::uint32_t,16> q;for(unsigned j=0;j<16;++j)q[j]=mem.u32(mp+j*4u);nmtx[calls.size()]=q;}
            if(e.kind==20){const auto cur=stack.current();std::array<std::uint32_t,16> q;for(unsigned j=0;j<16;++j)q[j]=cur.u32(j*4u);nmtx[calls.size()]=q;}
            calls.push_back({k.pc,k.ecx,k.args});
            if(e.kind==10){if(k.args[4])mem.put32(k.args[4],value[it->second]);mem.put32(k.args[1]+4,value2[it->second]);
                if(k.args[2])mem.put32(k.args[2],value3[it->second]);if(k.args[3])mem.put32(k.args[3],value4[it->second]);}
            if(e.kind==11){mem.put32(k.args[0]+0x54,value2[it->second]);mem.put32(k.args[0]+8,value2[it->second]);mem.put32(k.args[0]+0xc,value3[it->second]);mem.put32(k.args[0]+0x10,value4[it->second]);
                for(unsigned j=0;j<12;++j)mem.put32(k.args[0]+0x24+j*4u,mem.u32(CornerVals+0x30u+j*4u));}
            if(e.kind==18){if(k.args[1])mem.put32(k.args[1],value2[it->second]);if(k.args[2])mem.put32(k.args[2],value3[it->second]);}
            if(e.kind==21)for(unsigned j=0;j<4;++j)for(unsigned q=0;q<3;++q)mem.put32(k.args[2+j]+q*4u,mem.u32(CornerVals+j*12u+q*4u));
            return value[it->second];},nullptr};
        bool ok=true;std::string why;
        const std::uint32_t arg=Pc[entry]==0x5170d0u?w+0x80u:w;
        // debug entries: stack arguments (rb, plane / force at Light, shape, mask / point, out)
        const std::uint32_t rb=w+0x80u,Pl=Light,Pt=CornerVals+0x30u,Out=CornerVals+0x48u;
        std::array<std::uint32_t,5> da{rb,Pl,w+0x170u,Out,0};
        switch(Pc[entry]){
        case 0x516f20u:da={rb,Pl,Pt,0,0};break;
        case 0x516fa0u:da={rb,Pl,Pl+0xc,Pt,0};break;
        case 0x517a40u:da={rb,Pl,w+0x170u,rng()&((1u<<26)-1u),Out};break;
        case 0x517d90u:da={rb,Pl,Pt,Out,0};break;
        case 0x4a92e0u:da={Pt,Pl,0,0,0};break;
        case 0x43f3d0u:da={Pt,0,0,0,0};break;
        default:break;
        }
        const bool debug_entry=entry>=46u;
        try{if(debug_entry)(void)traffic_object_debug(ctx,Pc[entry],da[0],da[1],da[2],da[3],da[4]);
            else if(!traffic_object_callback(ctx,Pc[entry],arg))throw std::runtime_error("callback not dispatched");}
        catch(const std::exception& e){ok=false;why=e.what();}
        prepare(Pc[entry]);p32(S,arg);p32(S+4,0);p32(S+8,0);
        if(debug_entry){for(unsigned q=0;q<5;++q)p32(S+q*4u,da[q]);if(Pc[entry]==0x4a92e0u){guest_call.esi=da[0];guest_call.eax=da[1];}}
        std::memset(reinterpret_cast<void*>(0x31001000u),0,S-0x31001000u);   // locals: a zeroed stack, as the native frames
        static unsigned trace_at;trace_at=trace;
        g_signal_dump=[]{const unsigned nc=*reinterpret_cast<std::uint32_t*>(trace_at);dprintf(2,"PC faulted after %u services:",nc);
            for(unsigned i=0;i<nc&&i<64;++i){const auto* g=reinterpret_cast<const std::uint32_t*>(trace_at+64+i*0x40);dprintf(2," %x(%x,%x)",g[0],g[2],g[3]);}
            dprintf(2,"%c",10);};
        run();g_signal_dump=nullptr;
        const std::string tag="entry "+std::to_string(entry)+" ("+[&]{std::ostringstream s;s<<std::hex<<Pc[entry];return s.str();}()+") run "+std::to_string(iter);
        auto stackish=[&](std::uint32_t pc_arg,std::uint32_t nat){return pc_arg>=S-0x20000u&&pc_arg<S+0x1000u&&nat>=TrafficStackBase&&nat<TrafficStackBase+TrafficStackSize;};
        ++cases;if(!ok){fail(tag+" native threw: "+why);continue;}
        if(inject&&iter==13)native[0][w-Objs]^=1;
        for(std::size_t i=0;i<mirrors.size();++i){std::ostringstream s;s<<tag<<" region "<<std::hex<<mirrors[i].base;
            compare(native[i].data(),reinterpret_cast<void*>(mirrors[i].base),native[i].size(),s.str());}
        const unsigned nc=g32(trace);++cases;
        if(nc!=calls.size()){std::ostringstream s;s<<tag<<" service count PC="<<nc<<" native="<<calls.size()<<" |";
            for(unsigned i=0;i<nc&&i<40;++i)s<<" "<<std::hex<<g32(trace+64+i*0x40);s<<" | native";for(auto& cl:calls)s<<" "<<std::hex<<cl.pc;fail(s.str());}
        else for(unsigned i=0;i<nc;++i){const auto* g=reinterpret_cast<const std::uint32_t*>(trace+64+i*0x40);
            const auto& e=services[index_of[g[0]]];bool same=g[0]==calls[i].pc;
            if(g[0]==0x55a930u)same=same&&g[1]==calls[i].ecx;
            for(unsigned k=0;k<e.argc;++k)same=same&&(g[2+k]==calls[i].a[k]||stackish(g[2+k],calls[i].a[k]));
            if(same&&(e.kind==17||e.kind==19||e.kind==20)){const auto it=nmtx.find(i);same=it!=nmtx.end()&&!std::memcmp(it->second.data(),reinterpret_cast<void*>(MtxTrace+i*64u),64);
                if(!same){std::ostringstream s;s<<tag<<" service #"<<i<<" "<<std::hex<<g[0]<<" matrix";if(it!=nmtx.end())for(unsigned j=0;j<16;++j)s<<" "<<g32(MtxTrace+i*64u+j*4u)<<"/"<<it->second[j];fail(s.str());break;}}
            if(!same){std::ostringstream s;s<<tag<<" service #"<<i<<" "<<std::hex<<g[0];for(unsigned k=0;k<e.argc;++k)s<<" PC "<<g[2+k]<<"/native "<<calls[i].a[k];fail(s.str());break;}}
        ++coverage["entry "+std::to_string(entry)];
        for(auto& cl:calls){
            if(cl.pc==0x440b90u)++coverage["control switched"];if(cl.pc==0x4401d0u)++coverage["closed"];if(cl.pc==0x424940u)++coverage["sound"];
            if(cl.pc==0x405360u||cl.pc==0x4056d0u)++coverage["model drawn"];if(cl.pc==0x429530u||cl.pc==0x4289e0u||cl.pc==0x428ac0u)++coverage["sprite drawn"];
            if(cl.pc==0x43d1d0u)++coverage["ground plane"];if(cl.pc==0x502e20u)++coverage["wall face"];}
        for(auto& mi:mirrors)if(mi.base==Objs){std::uint32_t was;std::memcpy(&was,mi.bytes.data()+(w+0x80u-Objs),4);if(!was&&g32(w+0x80u)==1u)++coverage["body activated "+std::to_string(entry)];}
    }
    std::cout<<"{\"routine\":\""<<only<<"\",\"coverage\":{";bool first=true;
    for(const auto& [k,v]:coverage){std::cout<<(first?"":",")<<"\""<<k<<"\":"<<v;first=false;}
    std::cout<<"},\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}"<<std::endl;
    return mismatches?1:0;
}

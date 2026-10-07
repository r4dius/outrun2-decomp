// Included inside the compare_pc dispatcher; uses its private x86 harness.
if(only=="race_traffic_probe"){
    // Rival racers / traffic (race_traffic.cpp) against the original:
    //   0 47DAC0 init   1 477510 ranks   2 477C90 order   3 47C8A0 appear (47BC90)
    //   4 47D8B0 spawn  5 4EF890 appear tables
    //   6 47EC00 control  7 47EBA0 traffic  8 4791B0 overtakes  9 479500 LOD/links  10 479580 gap
    //   11 47CA40 catch-up  12 477E00 leader  13 478B90 fixed appear  14 4787E0 random appear
    //   per-car control 47E780 parts: 15 47E6C0 AI  16 476260 hit  17 474D30 trim  18 470360 shadow
    //   19 470570 boxes  20 470760 body  21 472450 traffic speed  22 472FE0 bus  23 47A7C0 racer
    //   24 47E110 lanes  25 4739F0 weave  26 473F30 recover  27 47DC10 lane choice  28 47BC00 motion  29 47AD30 racer lanes
    // A part that reaches a routine still unported (47AD30, 47BC00) is counted, not compared.
    // The module runs unmodified with its protected bridges live (47BC90,
    // 477C9D, 470295, 470933, 46C6FF, 476CB1, 46FE79, 46F990, 4866C7, 478641)
    // and the matrix leaves over D3DX. Every callee outside the module is a
    // capture stub recording {pc, ECX, stack arguments} and returning a
    // scripted value; 4A6CF0 returns it on the x87 stack, 43EB60 writes
    // [arg4] = value and the probe point's y, 43E570 writes [arg0+54].
    // Compared: every mirrored region (module .bss, appear tables, racers and
    // their tables, car works, player, .data words), the matrix arena and the
    // ordered service log. Stack addresses passed to services compare equal
    // when both sides pass a frame address (native scratch stack / PC stack).
    using namespace outrun::platform;
    if(argc<=7||std::string(argv[7])=="-")throw std::runtime_error("race traffic probe requires the pinned d3dx9_29.dll");
    constexpr unsigned base=0x3b800000u,stubs=base,trace=base+0x8000u,values=base+0x18000u,values2=base+0x19000u,values3=base+0x1a000u,values4=base+0x1b000u;
    constexpr unsigned Racers=base+0x20000u,Table1c=base+0x21000u,Table20=base+0x21400u,Table30=base+0x21800u,Rec=base+0x21c00u,Rec2=base+0x21e00u;
    constexpr unsigned Cfg=base+0x22000u,Chain=base+0x22400u,ModelData=base+0x23000u,Player=base+0x24000u,Works=base+0x26000u;
    constexpr unsigned WorksEnd=Works+24u*0x10f0u,AppearPts=base+0x40000u,Area=base+0x44400u,Camera=base+0x44800u,Params=base+0x45000u,BusFix=base+0x47800u,TrimT=base+0x47900u,LenTab=base+0x48000u;
    constexpr unsigned Objs=base+0x50000u,Gates=base+0x54000u,Rob=base+0x57000u,MtxTrace=base+0x60000u,CornerVals=base+0x5f000u;   // MtxTrace: 4287B0 matrices (kind 17)   // Heart Attack: OSO object works, gate objects 0x134.., robots 362/363
    map_at(base,0x70000,PROT_READ|PROT_WRITE|PROT_EXEC);map_original_arithmetic_bridge(file);
    {   unsigned restored=0;const std::size_t text_file=pe_file_offset(file,0x401000u);
        for(std::uint32_t a=0x401000u;a<0x401000u+0x194881u;a+=0x100u){
            const std::size_t n=std::min<std::size_t>(0x100u,0x401000u+0x194881u-a);
            if(std::memcmp(reinterpret_cast<void*>(a),file.data()+text_file+(a-0x401000u),n)){r060_raw_write(a,file.data()+text_file+(a-0x401000u),n);++restored;}
        }
        std::cerr<<"restored "<<restored<<" patched .text blocks"<<char(10);
    }
    init_matrix_oracle_globals();wire_d3dx_imports(map_d3dx29(argv[7]));
    {   // 40A240 calls D3DXMatrixInverse through IAT 5962DC: the DLL's generic
        // routine 440E9C, whose msvcrt _finite import (IAT +1270) gets an exact
        // stand-in (as in camera_probe).
        static const std::uint8_t finite_stub[]{0x8b,0x44,0x24,0x08,0x25,0x00,0x00,0xf0,0x7f,0x3d,0x00,0x00,0xf0,0x7f,0x0f,0x95,0xc0,0x0f,0xb6,0xc0,0xc3};
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
    // kind: 0 plain, 2 x87 float result, 10 43EB60, 11 43E570
    struct Svc{std::uint32_t pc;unsigned argc,kind;};
    const std::vector<Svc> services{
        {0x44ff10,0,0},{0x4a6cf0,1,2},{0x456d60,0,0},{0x44c990,1,0},{0x499ba0,1,12},{0x414040,0,0},{0x406630,2,0},{0x4962a0,0,0},
        {0x4957f0,0,0},{0x495b00,0,0},{0x495b20,0,0},{0x4b00d0,0,0},{0x495800,0,0},{0x44fdf0,0,0},{0x49b2d0,0,0},{0x43d470,1,0},
        {0x44bdd0,0,0},{0x44be00,0,0},{0x44be10,0,0},{0x44c940,1,0},{0x451350,1,0},{0x451140,2,0},{0x44be50,1,0},{0x44bed0,1,0},
        {0x44bec0,0,0},{0x4efb90,1,0},{0x4f0030,2,0},{0x43eb60,5,10},{0x43e570,3,11},{0x440180,2,0},{0x440200,1,0},{0x4874f0,2,0},
        {0x455c10,0,0},{0x4f6e40,1,0},{0x4a4010,1,0},{0x5051d0,2,0},{0x487570,1,0},{0x4a20f0,1,0},{0x4a2120,0,0},{0x4960a0,1,0},
        {0x55a930,0,0},{0x43e6c0,3,11},{0x43f960,0,0},{0x440110,2,0},{0x4401d0,1,0},{0x440bb0,2,0},{0x44b750,1,0},{0x44c520,0,0},
        {0x44c8d0,1,0},{0x44dc50,1,0},{0x44f0f0,2,0},{0x450380,1,0},{0x4551c0,1,0},{0x45c440,0,0},{0x45c450,0,0},{0x45c460,0,0},
        {0x496390,0,0},{0x4963b0,0,0},{0x4b00e0,0,0},{0x4f9ea0,0,0},
        {0x4a6ea0,4,0},{0x424940,1,0},{0x44fe40,0,0},{0x4a2270,5,0},{0x502420,1,0},{0x45b510,0,0},{0x45d820,0,0},{0x450680,0,0},
        {0x440bd0,2,0},{0x483e90,1,13},{0x45aff0,0,0},{0x45b810,0,0},{0x4b01a0,2,2},{0x455150,1,0},
        {0x43d390,3,15},{0x43d440,2,0},{0x5041b0,3,0},{0x43f110,6,16},{0x46c780,3,0},{0x4a2650,1,0},{0x4a3f80,2,0},{0x495880,0,0},{0x44b7b0,1,0},{0x4505d0,0,0},
        {0x580f40,0,0},{0x428320,3,0},{0x428460,5,0},{0x4285a0,1,0},{0x4287b0,2,17},{0x428800,2,0},{0x428880,1,0},{0x428940,2,0},{0x428770,2,0},{0x4289e0,5,19},{0x429780,3,18},{0x44ddc0,2,0},{0x440370,1,0},
        {0x429530,5,0},{0x4294c0,3,18},{0x4295d0,9,0},{0x4bc990,4,23},{0x43fa00,0,0},{0x450250,2,0},
        {0x450570,0,0},{0x45a2b0,1,0},{0x417f70,0,0},{0x580f33,1,0},{0x405360,6,20},{0x409df0,1,20},{0x4044a0,0,20},{0x4044c0,0,20},{0x4044e0,1,20},{0x4052b0,0,20},{0x4052c0,0,20}};
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
        if(e.kind==19){c.insert(c.end(),{0x8b,0x74,0x24,0x18,0x8b,0x0d});emit32(c,trace);c.insert(c.end(),{0xc1,0xe1,0x06,0x81,0xc1});emit32(c,MtxTrace);
            c.insert(c.end(),{0x57,0x89,0xcf,0xb9,0x10,0,0,0,0xf3,0xa5,0x5f});}
        if(e.kind==20||e.kind==23){   // 20: current matrix, 23: 16 bytes of the string [arg3] -> MtxTrace + count*64
            if(e.kind==20){c.insert(c.end(),{0x8b,0x35});emit32(c,0x89b564u);}else c.insert(c.end(),{0x8b,0x74,0x24,0x1c});
            c.insert(c.end(),{0x8b,0x0d});emit32(c,trace);c.insert(c.end(),{0xc1,0xe1,0x06,0x81,0xc1});emit32(c,MtxTrace);
            c.insert(c.end(),{0x57,0x89,0xcf,0xb9,std::uint8_t(e.kind==20?0x10:4),0,0,0,0xf3,0xa5,0x5f});}
        if(e.kind==17){c.insert(c.end(),{0x8b,0x74,0x24,0x14,0x8b,0x0d});emit32(c,trace);c.insert(c.end(),{0xc1,0xe1,0x06,0x81,0xc1});emit32(c,MtxTrace);
            c.insert(c.end(),{0x57,0x89,0xcf,0xb9,0x10,0,0,0,0xf3,0xa5,0x5f});}   // [arg1] matrix -> MtxTrace + count*64
        c.insert(c.end(),{0xff,0x05});emit32(c,trace);                                      // inc [count]
        c.push_back(0xa1);emit32(c,values+4*unsigned(k));                                   // eax=value
        if(e.kind==2){c.insert(c.end(),{0xd9,0x05});emit32(c,values+4*unsigned(k));}       // fld dword [value]
        if(e.kind==10){
            c.insert(c.end(),{0x8b,0x74,0x24,0x20,0x85,0xf6,0x74,0x02,0x89,0x06});          // esi=arg4; if(esi)[esi]=eax
            c.insert(c.end(),{0x8b,0x74,0x24,0x14,0x8b,0x0d});emit32(c,values2+4*unsigned(k));   // esi=arg1; ecx=value2
            c.insert(c.end(),{0x89,0x4e,0x04});                                             // [esi+4]=ecx
            c.insert(c.end(),{0x8b,0x74,0x24,0x18,0x85,0xf6,0x74,0x08,0x8b,0x0d});emit32(c,values3+4*unsigned(k));   // esi=arg2; if(esi)[esi]=value3
            c.insert(c.end(),{0x89,0x0e});
        }
        if(e.kind==11){c.insert(c.end(),{0x8b,0x74,0x24,0x10,0x8b,0x0d});emit32(c,values2+4*unsigned(k));c.insert(c.end(),{0x89,0x4e,0x54,0x89,0x4e,0x08,0x8b,0x0d});
            emit32(c,values3+4*unsigned(k));c.insert(c.end(),{0x89,0x4e,0x0c,0x8b,0x0d});emit32(c,values4+4*unsigned(k));c.insert(c.end(),{0x89,0x4e,0x10});   // [arg0+54], point at +8
            c.insert(c.end(),{0x57,0x8d,0x7e,0x24,0xbe});emit32(c,CornerVals);c.insert(c.end(),{0xb9,0x0c,0,0,0,0xf3,0xa5,0x5f});}   // corners [arg0+24..53] = CornerVals
        if(e.kind==15){c.insert(c.end(),{0x8b,0x74,0x24,0x18,0x8b,0x0d});emit32(c,values2+4*unsigned(k));c.insert(c.end(),{0x89,0x0e,0x8b,0x0d});emit32(c,values3+4*unsigned(k));
            c.insert(c.end(),{0x89,0x4e,0x04,0x8b,0x0d});emit32(c,values4+4*unsigned(k));c.insert(c.end(),{0x89,0x4e,0x08});}   // [arg2] = 3 floats
        if(e.kind==13){c.insert(c.end(),{0x8b,0x74,0x24,0x10,0x8b,0x0d});emit32(c,values2+4*unsigned(k));c.insert(c.end(),{0x89,0x0e,0x8b,0x0d});emit32(c,values3+4*unsigned(k));
            c.insert(c.end(),{0x89,0x4e,0x04,0x8b,0x0d});emit32(c,values4+4*unsigned(k));c.insert(c.end(),{0x89,0x4e,0x08});}   // [arg0] = 3 floats
        if(e.kind==12){c.insert(c.end(),{0x8b,0x44,0x24,0x10,0x0f,0xb6,0x80});emit32(c,0x8367c0u);}   // eax = movzx byte [arg0+8367C0]
        if(e.kind==16){   // 43F110(.., .., point, &polygon, .., &kind): [arg5]=eax, [arg2+4]=value2, [arg3]=value3
            c.insert(c.end(),{0x8b,0x74,0x24,0x24,0x85,0xf6,0x74,0x02,0x89,0x06});
            c.insert(c.end(),{0x8b,0x74,0x24,0x18,0x8b,0x0d});emit32(c,values2+4*unsigned(k));c.insert(c.end(),{0x89,0x4e,0x04});
            c.insert(c.end(),{0x8b,0x74,0x24,0x1c,0x85,0xf6,0x74,0x08,0x8b,0x0d});emit32(c,values3+4*unsigned(k));c.insert(c.end(),{0x89,0x0e});
        }   // eax = movzx byte [arg0+8367C0]
        c.insert(c.end(),{0x59,0x5e,0x5f,0xc3});
        std::memcpy(reinterpret_cast<void*>(stubs+stub_off),c.data(),c.size());
        r046_write_rel32(e.pc,0xe9,stubs+stub_off);stub_off=(stub_off+unsigned(c.size())+15u)&~15u;
    }
    std::mt19937 rng(0x47dac000u);
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
            if(std::getenv("OR2_TRAFFIC_DWORDS")){const std::size_t o=i&~std::size_t(3);s<<" dwords:";for(std::size_t q=o;q<o+0x40&&q+4<=n;q+=4){std::uint32_t pa,na;std::memcpy(&pa,b+q,4);std::memcpy(&na,a+q,4);if(pa!=na)s<<" +"<<q<<":"<<pa<<"/"<<na;}}
            fail(s.str());break;}
    };
    struct Mirror{std::uint32_t base,end;std::vector<std::uint8_t> bytes;};
    std::vector<Mirror> mirrors{{0x800a00u,0x80fb50u,{}},{0x84bd00u,0x84d900u,{}},{0x64de00u,0x64e200u,{}},{0x6a4e2cu,0x6a4e60u,{}},
        {0x6a5df8u,0x6a6de8u,{}},{0x650500u,0x651100u,{}},{0x799b30u,0x79a300u,{}},{0x79fb48u,0x79fd50u,{}},{0x780200u,0x780280u,{}},{LenTab,LenTab+0x2000u,{}},
        {0x680ad4u,0x680ae0u,{}},{0x8367c0u,0x8367ecu,{}},{0x7f9460u,0x7f9610u,{}},{0x5c21e0u,0x5c2210u,{}},{0x599400u,0x599500u,{}},{0x5c3e00u,0x5c4100u,{}},{0x5c6fc0u,0x5c6fe0u,{}},{0x850ba0u,0x8514a0u,{}},{0x6ac880u,0x6ac9c0u,{}},{0x5e0980u,0x5e0c80u,{}},{0x5a9100u,0x5afe00u,{}},{0x7162c0u,0x7162d0u,{}},{0x5da2c0u,0x5da2e0u,{}},{0x79f570u,0x79f580u,{}},{AppearPts,AppearPts+0x4200u,{}},{Area,Area+0x200u,{}},{Camera,Camera+0x400u,{}},{Params,Params+0x2500u,{}},{BusFix,BusFix+0x100u,{}},{TrimT,TrimT+0x700u,{}},{0x5a2900u,0x5a2a00u,{}},{0x5a4600u,0x5a4700u,{}},{0x5b0000u,0x5b0400u,{}},
        {0x5b3a00u,0x5b4600u,{}},{0x619a00u,0x619b00u,{}},{0x628000u,0x628400u,{}},
        {Racers-0x100u,Racers+0x1000u,{}},{Table1c,Table1c+0x400u,{}},{Table20,Table20+0x400u,{}},{Table30,Table30+0x400u,{}},{Rec,Rec+0x200u,{}},
        {Rec2,Rec2+0x200u,{}},{Cfg,Cfg+0x400u,{}},{Chain,Chain+0xc00u,{}},{ModelData,ModelData+0x1000u,{}},{Player,Player+0x10f0u,{}},
        {Works,WorksEnd,{}},
        {0x7f1900u,0x7f2a00u,{}},{0x7f8980u,0x7f8e00u,{}},{0x842100u,0x842120u,{}},{0x84def8u,0x84df40u,{}},{0x842800u,0x844a00u,{}},{0x84bcf0u,0x84bd00u,{}},{0x686258u,0x686260u,{}},{0x5c68e0u,0x5c68f0u,{}},{0x7d3190u,0x7d31d0u,{}},{0x5da040u,0x5da2c0u,{}},{0x7d6810u,0x7d6880u,{}},{0x79a300u,0x79fb48u,{}},
        {0x82f5f0u,0x82fb10u,{}},{0x655b58u,0x655b5cu,{}},{0x63ee68u,0x64de00u,{}},{0x64e200u,0x650500u,{}},{0x7d3830u,0x7d3a00u,{}},{0x7d2698u,0x7d269cu,{}},{0x7d2da0u,0x7d2de0u,{}},{CornerVals,CornerVals+0x30u,{}},{Objs,Objs+0x4000u,{}},{Gates,Gates+0x2800u,{}},{Rob,Rob+0x200u,{}},
        {MatrixArena,MatrixArena+0x2000u,{}},{0x89b560u,0x89b570u,{}}};
    const unsigned Entries=70u;
    for(unsigned iter=0;iter<count;++iter){
        unsigned entry=iter%Entries;
        if(const char* e=std::getenv("OR2_TRAFFIC_ENTRY"))entry=unsigned(std::stoul(e));
        std::memset(reinterpret_cast<void*>(trace),0,0x10000);
        std::vector<std::uint32_t> value(services.size()),value2(services.size()),value3(services.size()),value4(services.size());
        for(auto& v:value)v=rnd(3)?rnd(3):rng();
        for(auto& v:value2)v=fb(real(-50,50));
        for(auto& v:value3)v=fb(real(-50,50));
        for(auto& v:value4)v=fb(real(-50,50));
        auto setv=[&](std::uint32_t pc,std::uint32_t v){value[index_of.at(pc)]=v;};
        auto setv2=[&](std::uint32_t pc,std::uint32_t v){value2[index_of.at(pc)]=v;};
        auto setv3=[&](std::uint32_t pc,std::uint32_t v){value3[index_of.at(pc)]=v;};
        // ---- world ------------------------------------------------------------
        const std::uint32_t players=1u+rnd(10),n=rnd(7);
        p32(0x680ad4u,players);p32(0x680ad8u,rnd(4));
        p32(0x78026cu,rnd(4)?0x10u:(rnd(2)?3u:rnd(0x24)));p32(0x780258u,rnd(3)?6u:rnd(10));p32(0x780248u,0);
        for(std::uint32_t a=0x800a00u;a<0x80fb50u;a+=4)p32(a,rnd(4)?0u:(rnd(2)?rnd(8):rng()));
        for(std::uint32_t k=0;k<6;++k)if(rnd(5))p16(0x804480u+k*0x1030u,0x4f53u);
        for(std::uint32_t k=0;k<12;++k)if(rnd(5))p16(0x80a670u+k*0x70cu,0x4f53u);
        for(std::uint32_t k=0;k<0x2d00;k+=4)p32(0x804484u+k,rng());   // road samples
        for(std::uint32_t k=0;k<0x1500;k+=4)p32(0x80a674u+k,rng());
        p16(0x804388u,rnd(3)?0u:20u+rnd(400));p32(0x800d98u,rnd(5)?0u:rnd(5));p32(0x80a5a0u,rnd(5)?0u:rnd(5));
        p32(0x8037b8u,rnd(4)?0u:1u);p8(0x800d38u,rnd(12));p8(0x803744u,rnd(256));p32(0x803710u,rnd(3)?600u+rnd(400):rng());
        for(std::uint32_t k=0;k<12;++k)p32(0x803718u+k*4u,rnd(6));
        for(std::uint32_t k=0;k<6;++k)p16(0x800d2cu+k*2u,rnd(4)?rnd(0x20):rnd(0x40));   // lane traffic weights (46CC10 sums)
        p8(0x80fb18u,rnd(2));p32(0x80fb14u,rnd(2));p32(0x80fb24u,0);
        p32(0x80fb00u,rnd(8)?Racers:0u);p32(0x80fb04u,n);p32(0x80fb1cu,rnd(8)?Table1c:0u);p32(0x80fb20u,Table20);p32(0x80fb30u,rnd(8)?Table30:0u);
        p32(0x80fb0cu,Cfg);                                          // 476F40 reads [80FB0C]+48 unguarded
        for(std::uint32_t k=0;k<0x400;k+=4)p32(Cfg+k,rng());
        p8(Cfg+0x30,rnd(32));p32(Cfg+0x44,rnd(3)?Cfg+0x100u:0u);p32(Cfg+0x48,rnd(3)?Cfg+0x140u:0u);
        for(std::uint32_t k=0;k<4;++k){pf(Cfg+0x100u+k*4u,rnd(3)?real(0.1f,4):0.0f);pf(Cfg+0x140u+k*4u,rnd(3)?real(0.1f,4):0.0f);}
        for(std::uint32_t k=0;k<0x400;k+=4){p32(Table1c+k,rng());p32(Table20+k,rng());p32(Table30+k,rng());}
        for(std::uint32_t k=0;k+0x14u<=0x400u;k+=0x14u)p32(Table1c+k+0x10u,rnd(0x1000u/0xa0u));   // rank -> racer index
        p32(0x64df64u,Cfg+0x200u);pf(Cfg+0x210u,real(0,2));pf(Cfg+0x214u,real(0,2));
        p32(0x64deecu,Cfg+0x240u);pf(Cfg+0x240u,real(0,0.5f));pf(Cfg+0x244u,real(0.3f,0.7f));pf(Cfg+0x248u,real(0.6f,0.99f));
        p32(0x64def0u,Cfg+0x280u);pf(Cfg+0x298u,real(-0.2f,0.2f));pf(Cfg+0x29cu,real(-0.2f,0.2f));
        for(std::uint32_t a=0x6a4e2cu;a<0x6a4e60u;a+=4)p32(a,rnd(0x10000));
        for(std::uint32_t m=0;m<0x2c;++m)p8(0x8367c0u+m,m<30u&&rnd(5)==0?1u:0u);
        // model records 650500 + m*0x44 -> chain -> model data
        for(std::uint32_t m=0;m<0x2d;++m){
            const std::uint32_t r=0x650500u+m*0x44u,P1=Chain+m*0x10u,P2=Chain+0x400u+m*0x10u,MD=ModelData+(m%32u)*0x80u;
            p32(r,P1);p32(P1,P2);p32(P2,MD);p32(P2+4,(entry==18u||rnd(2))?MD+0x40u:0u);   // +4: shadow data (470360)
            p32(r+0xc,rng());p32(r+0x10,rng());p32(r+0x14,rng());
            for(std::uint32_t k=0;k<0x80;k+=4)pf(MD+k,real(-3,3));
        }
        for(std::uint32_t k=0;k<0x200;k+=4){p32(Rec+k,rng());p32(Rec2+k,rng());}
        p16(Rec2+0x26,rnd(0x8000));
        // event records: works of 8.., flags
        for(std::uint32_t id=0;id<32;++id)p32(0x799b38u+id*0x3cu,id==8u?Player:(id>=9u?Works+(id-9u)*0x10f0u:0u));
        p32(0x799d18u,Player);
        for(std::uint32_t id=0;id<0x200;++id)p8(0x79fb48u+id,id>=9u&&id<32u?(rnd(2)?0u:(rnd(2)?2u:1u)|(rnd(4)<<2)):rnd(256));
        auto fill_car=[&](std::uint32_t w,std::uint32_t id){
            for(std::uint32_t k=0;k<0x10f0;k+=4)p32(w+k,rnd(3)?0u:rng());
            p32(w,id);p32(w+4,rng()&~1u);p32(w+8,rnd(4)?(rng()&~1u):(rng()|1u));p8(w+0x11,rnd(30));
            p16(w+0x260,rnd(280));p8(w+0xc36,rnd(8));p8(w+0xc37,rnd(8));
            p32(w+0x5c,rnd(2));p32(w+0x60,rnd(3)?0x64u+rnd(2):rnd(3));p16(w+0x64,rnd(280));p8(w+0x66,rnd(12));p32(w+0x68,rnd(4));
            pf(w+0x1c4,real(0,2));pf(w+0xbd0,real(-0.2f,1.2f));p16(w+0xb72,rnd(3)?0u:rnd(5));
            for(std::uint32_t k:{0x14u,0x18u,0x1cu})pf(w+k,real(-500,500));
            p16(w+0x2c,rng());p16(w+0x2e,rng());p16(w+0x30,rng());p16(w+0xc2c,rng());p32(w+0x314,0);p32(w+0x324,rnd(7));p8(w+0x328,rnd(5));
            p32(w+0xc60,rnd(2)?0x19au:9u+rnd(23));p8(w+0x335,rnd(3));   // 4F9450: the last-hit car, the box count
        };
        fill_car(Player,8);
        for(std::uint32_t id=9;id<33;++id)fill_car(Works+(id-9u)*0x10f0u,id);
        for(std::uint32_t i=0;i<0x1000/0xa0;++i){
            const std::uint32_t r=Racers+i*0xa0u;
            for(std::uint32_t k=0;k<0xa0;k+=4)p32(r+k,rnd(3)?0u:rng());
            p32(r+0x3c,rnd(3)?std::uint32_t(std::max<std::int32_t>(0,std::int32_t(*reinterpret_cast<std::uint16_t*>(Player+0x260))+std::int32_t(rnd(80))-15)):rnd(300));pf(r+0x40,real(-2,300));pf(r+0x44,real(-1,3));
            const std::uint32_t ev=9u+rnd(std::max(1u,players));
            p32(r+0x4c,rnd(3)?0xffffffffu:ev);p32(r+0x50,rnd(2));p32(r+0x54,rnd(4)?1u:0u);p32(r+0x6c,rnd(5)?0u:1u);
            p8(r+0x72,rnd(30));p8(r+0x73,rnd(8));p8(r+0x74,rnd(8));p8(r+0x76,rnd(8));p8(r+0x77,rnd(4));p32(r+0x60,rnd(2));
            p32(r+0x64,rnd(2));p32(r+0x68,rnd(2));pf(r+0x24,real(0,100));pf(r+0x28,real(-5,100));pf(r+0x30,real(-5,10));
            p32(r+0x98,rnd(20));p32(r+0x9c,rnd(200));pf(r+0x20,real(0,1));
        }
        {   // consistent ranks: a permutation of 0..n over the racers and the player
            std::vector<std::uint32_t> act;for(std::uint32_t k=0;k<n;++k)if(g32(Racers+k*0xa0u+0x54u))act.push_back(k);
            std::vector<std::uint32_t> perm(act.size()+1);for(std::uint32_t k=0;k<perm.size();++k)perm[k]=k;
            std::shuffle(perm.begin(),perm.end(),rng);
            for(std::uint32_t k=0;k<act.size();++k)p8(Racers+act[k]*0xa0u+0x74u,perm[k]);
            p8(Player+0xc36u,perm[act.size()]);
            std::uint32_t next=std::uint32_t(perm.size());
            for(std::uint32_t k=0;k<n;++k)if(!g32(Racers+k*0xa0u+0x54u))p8(Racers+k*0xa0u+0x74u,next++);
        }
        p32(0x64e08cu,rnd(4)?1u:0u);p32(0x79f574u,Camera);
        for(std::uint32_t k=0;k<0x200;k+=4)pf(Camera+k,real(-1,1));
        for(std::uint32_t k=0;k<16;++k)pf(Camera+0x140u+k*4u,(k%5u)==0u?1.0f:real(-0.3f,0.3f));
        for(std::uint32_t k:{0x170u,0x174u,0x178u})pf(Camera+k,real(-400,400));
        pf(Camera+0xb4u,real(100,600));pf(Camera+0xbcu,real(-5,5));pf(Camera+0xc0u,rnd(4)?real(100,3000):real(-5,50));   // 4F02D0 focal / depth range
        for(std::uint32_t k=0;k<0x200;k+=4)p32(Area+k,rnd(4)?0u:rng());
        p32(Area+0xc,rnd(3)?0u:(rnd(2)?0x200u:0x100u));
        {   // appear points per stage (6A6CE0 -> records of 0x14, terminated by 0x42)
            const std::uint32_t pl5c=g32(Player+0x5c);const std::uint16_t pl64=*reinterpret_cast<std::uint16_t*>(Player+0x64);
            for(std::uint32_t s=0;s<0x42;++s){
                const std::uint32_t b=AppearPts+s*0x100u;p32(0x6a6ce0u+s*4u,b);
                const std::uint32_t nrec=rnd(10);
                for(std::uint32_t r=0;r<nrec;++r){
                    const std::uint32_t e=b+r*0x14u;
                    p32(e,rnd(3)?s:rnd(0x42));p32(e+4,rnd(3)?pl5c:rnd(2));p16(e+8,rnd(3)?pl64:rnd(280));p8(e+0xa,rnd(8));p8(e+0xb,rng());
                    p32(e+0xc,rnd(3)?rnd(0x1e):0x1eu+rnd(0xe));p8(e+0x10,rnd(4));p8(e+0x11,rnd(4)?0u:1u);p16(e+0x12,rng());
                }
                p32(b+nrec*0x14u,rnd(8)?0x42u:0x43u+rnd(4));
            }
        }
        if(entry>=6u){p8(0x79fcceu,rnd(4)?2u:rnd(256));p32(0x80fb1cu,Table1c);p32(0x80fb30u,Table30);
            const std::uint32_t v=g32(0x780258u);if((entry==6u||entry==7u)&&(v==2u||v==4u))p32(0x780258u,6u);
            for(std::uint32_t r=0;r<8;++r){const std::uint32_t q=Rec+r*0x14u;   // appear records: count, gap, model weights 0..100
                for(std::uint32_t k=0;k<0x14;++k)p8(q+k,rnd(256));p8(q+4,rnd(8));p8(q+5,rnd(8));for(std::uint32_t k=6;k<11;++k)p8(q+k,rnd(3)?rnd(101):0u);}
            p16(0x800d50u,*reinterpret_cast<std::uint16_t*>(Player+0x64)-rnd(12));p8(0x800d52u,rnd(4));p8(0x800d28u,rnd(3));p32(0x800d9cu,rnd(5));for(std::uint32_t k=0;k<5;++k)p8(0x800d3cu+k,rnd(9));   // per-class pick counters (kept 0..8 by 46CE30)
           p32(0x800ab0u,rnd(4)?0u:1u);
            if(entry==6u||entry==7u)for(std::uint32_t id=8;id<32;++id)pf((id==8u?Player:Works+(id-9u)*0x10f0u)+0x2e8u,real(-1,1));   // 4F9000 yaw offset: finite radians in the game
        }
        std::uint32_t car=Works+rnd(23)*0x10f0u;   // a traffic car (event 9..31)
        if(entry>=15u){
            p8(0x79fb48u+g32(car),2);p32(0x80fb00u,Racers);
            for(std::uint32_t r=0;r<8;++r){std::uint32_t mk=0;if(rnd(2))mk|=1u<<rnd(6);if(!rnd(3))mk|=1u<<rnd(6);p8(Rec+r*0x14u+0xb,mk);}   // blocked lanes (never a whole side)
            for(std::uint32_t a=0x802af0u;a<0x803710u;a+=4)pf(a,rnd(4)?real(0,90):real(90,600));
            for(std::uint32_t k=0;k<8;++k)p32(0x803750u+k*4u,rnd(3)?Works+rnd(23)*0x10f0u:(rnd(2)?Player:0u));
            {   // speed record: moderate values
                p8(Rec2+2,rnd(9));p8(Rec2+3,rnd(9));p16(Rec2+4,5u+rnd(60));p16(Rec2+6,1u+rnd(20));p16(Rec2+8,5u+rnd(200));p16(Rec2+0xa,rnd(6));p16(Rec2+0xc,20u+rnd(200));
                for(std::uint32_t k=0;k<7;++k){p16(Rec2+0x0eu+k*2u,50u+rnd(250));p16(Rec2+0x1c+k*2u,50u+rnd(250));p16(Rec2+0x2a+k*2u,rnd(100));p16(Rec2+0x38+k*2u,rnd(100));}
            }
            for(std::uint32_t k=0;k<0x2500;k+=4)pf(Params+k,real(0.5f,3));
            for(std::uint32_t k=0;k<0x100;k+=4)p32(BusFix+k,rng());
            p16(BusFix+4,rnd(300));
            {   // 474990 trim list: cfg+44 -> {list} -> {stage, records}..., -1; records 0x20 {lo, hi, kind, 6 floats}, FFFF
                p32(TrimT,TrimT+0x10u);
                for(std::uint32_t k=0;k<3;++k){p32(TrimT+0x10u+k*8u,rnd(4));p32(TrimT+0x14u+k*8u,TrimT+0x100u+k*0x100u);
                    for(std::uint32_t r=0;r<4;++r){const std::uint32_t e=TrimT+0x100u+k*0x100u+r*0x20u;p16(e,rnd(150));p16(e+2,150u+rnd(150));p32(e+4,rnd(2));
                        for(std::uint32_t f=0;f<6;++f)pf(e+8u+f*4u,rnd(3)?real(0.2f,2):0.0f);}
                    p16(TrimT+0x100u+k*0x100u+0x80u,0xffffu);}
                p32(TrimT+0x28u,0xffffffffu);
                p32(Cfg+0x44,rnd(3)?TrimT:0u);
            }
            auto ai_car=[&](std::uint32_t w){
                p32(w+0x320,rnd(4));p32(w+0x10d4,0x5b3e08u+rnd(7)*0x14u);p8(w+0xb74,rnd(4));p8(w+0xc38,rnd(6));p8(w+0xc32,rnd(6));p32(w+0xdd4,8u+rnd(24));
                p32(w+0x2b4,Params);p8(w+0x335,rnd(3));p8(w+0xd20,0);p8(w+0x66,rnd(6));p8(w+0x280,rnd(6)?rnd(6):0xffu);
                for(std::uint32_t k:{0x264u,0x268u})pf(w+k,real(-6,6));
                pf(w+0x1c4,real(0,3));pf(w+0x10e0,rnd(2)?real(-1,2):0.0f);pf(w+0xd24,real(-1,1));pf(w+0x2c8,rnd(3)?0.0f:real(-1,1));pf(w+0x2e8,real(-1,1));
                for(std::uint32_t k=0xb00;k<0xc00;k+=4)pf(w+k,real(-2,2));
                pf(w+0xb80,real(50,200));pf(w+0xb90,real(50,200));pf(w+0xbc0,real(0.5f,1.5f));pf(w+0xb78,real(0.5f,1.5f));pf(w+0xb88,real(0.5f,1.5f));
                p32(w+0xb14,rnd(3)?0u:rnd(40));p32(w+0xb18,40u+rnd(100));p16(w+0xb50,rnd(50));p32(w+0xb58,rnd(4)?0u:0xbu+rnd(12));
                p16(w+0xb60,rnd(3)?0u:rnd(200));p16(w+0xb62,rnd(100));p16(w+0xb64,rnd(100));p16(w+0xb6e,rng());p16(w+0xb70,rnd(4)?rnd(3)*0xb4u:rnd(400));
                p8(w+0xb6c,rnd(4)?0u:1u);p8(w+0xb6d,rnd(8));p8(w+0xd22,rnd(5));p8(w+0xd23,rnd(5));p16(w+0xc26,rnd(4)?0u:rnd(100));p16(w+0xc2c,rnd(3)?0u:rng());
                p16(w+0x162,rng());p16(w+0x17e,rng());p16(w+0x32,rng());p32(w+0x208,rnd(4));p32(w+0x1d8,rnd(4));
                p32(w+0xc5c,rng()&0x7fff3ffeu);p32(w+0xc,rng());p32(w+0x34,rnd(0x200));
                for(std::uint32_t k=0;k<0x40;k+=4)pf(w+0xc90+k,real(-1,1));
                for(std::uint32_t k=0;k<0x18;k+=4)pf(w+0xc70+k,real(-3,3));p32(w+0xc48,0);
                for(std::uint32_t b=0;b<3;++b)for(std::uint32_t k=0;k<0x1c;k+=4)pf(w+0x338+b*0x1ecu+k,real(-4,4));
                pf(w+0x10d8+4,real(0,3));
                p32(w+0xc48,rnd(3)?0u:w+0xc70);p8(w+0xc38,rnd(6));p8(w+0xc30,rnd(6));p32(w+0xc3c,8u+rnd(24));p8(w+0xc42,rnd(2));
                p32(w+0x230,rnd(4)?rnd(0x400):0xffffffffu);p32(w+0x1c0,rnd(0x400));p32(w+0x248,rnd(3)?0u:rng());   // road polygon (780228 tables)
            };
            ai_car(Player);for(std::uint32_t id=9;id<32;++id)ai_car(Works+(id-9u)*0x10f0u);
            p8(Player+0x2f0,rnd(4)?0u:2u);
            {   // course order links +314 / +318 among a random subset
                std::vector<std::uint32_t> chain{Player};for(std::uint32_t id=9;id<32;++id)if(rnd(2))chain.push_back(Works+(id-9u)*0x10f0u);
                if(!std::count(chain.begin(),chain.end(),car))chain.push_back(car);
                std::shuffle(chain.begin(),chain.end(),rng);
                for(std::uint32_t id=8;id<32;++id){const std::uint32_t w=id==8u?Player:Works+(id-9u)*0x10f0u;p32(w+0x314,0);p32(w+0x318,0);}
                for(std::size_t i=0;i<chain.size();++i){p32(chain[i]+0x314,i?chain[i-1]:0u);p32(chain[i]+0x318,i+1<chain.size()?chain[i+1]:0u);}
            }
            pf(Cfg+0x210,real(0.5f,1.5f));pf(Cfg+0x214,real(0.5f,1.5f));pf(Cfg+0x218,real(0.5f,1.5f));
        }
        // services
        setv(0x44ff10,rnd(6)?0u:1u);setv(0x456d60,rnd(5));setv(0x44c990,rnd(300));setv(0x499ba0,rnd(4)?0u:1u);setv(0x414040,rng());
        setv(0x4962a0,rnd(3)?0u:1u);setv(0x4957f0,rnd(3)?1u:0u);setv(0x495b00,rnd(3)?0u:1u);setv(0x495b20,rnd(8));setv(0x4b00d0,rnd(3)?0u:1u);
        setv(0x495800,rnd(20));setv(0x44fdf0,rnd(3)?rnd(0x700):rng());setv(0x49b2d0,rnd(3)?rnd(0x60):rng());setv(0x43d470,rnd(8)?150u+rnd(150):1u+rnd(3));
        setv(0x44bdd0,rnd(16));setv(0x44be00,rnd(8));setv(0x44be10,rnd(16));setv(0x44c940,rnd(8));setv(0x451350,rnd(4)?rnd(2):rnd(3));
        setv(0x44be50,rnd(2));setv(0x44bed0,MatrixArena+0x1000u);setv(0x44bec0,MatrixArena+0x1040u);
        setv(0x4efb90,Rec+rnd(8)*0x14u);setv(0x4f0030,Rec2);setv(0x43eb60,rnd(4)?0u:1u);setv(0x455c10,1u+rnd(3));setv(0x4a20f0,rnd(3)?rnd(50):0xffffffffu-rnd(5));
        setv(0x4a2120,rnd(200));setv(0x4960a0,rnd(2));setv(0x55a930,rnd(4)?0u:1u);
        setv(0x43e6c0,rnd(8)?1u:0u);setv(0x43f960,rnd(2));setv(0x44b750,rng());setv(0x44c520,rnd(8000));setv(0x44c8d0,Area);
        setv(0x44dc50,rnd(0x42));setv(0x44f0f0,rnd(2));setv(0x450380,rnd(16));setv(0x4551c0,rnd(2));
        {const std::uint32_t z[]{0x16u,9u,2u,6u,0u};setv(0x45c440,rnd(3)?z[rnd(5)]:rnd(30));}
        setv(0x45c450,rnd(300));setv(0x45c460,rnd(300));setv(0x496390,rnd(2));setv(0x4963b0,rnd(2));setv(0x4b00e0,rnd(2));
        setv2(0x43e6c0,fb(real(2,30)));
        setv(0x44fe40,rng());setv(0x45b510,rnd(2));setv(0x45d820,rnd(2));setv(0x450680,rnd(2));setv(0x45aff0,BusFix);setv(0x45b810,rnd(3));
        setv(0x4b01a0,fb(real(0,3)));setv2(0x483e90,fb(real(-4,4)));
        if(entry>=15u&&rnd(3)){const std::uint32_t z[]{0x14u,0x16u,9u,0xeu,2u,6u,0xbu};setv(0x45c440,z[rnd(7)]);}
        if(entry>=15u)setv(0x44fdf0,rnd(4)?rnd(0x700):rnd(0x2000));
        if(entry==12u){   // 477E00: the leader reaching the branch
            const std::uint32_t z[]{4u,0xeu,rnd(16)};const std::uint32_t lvl=z[rnd(3)];
            setv(0x44c940,lvl);setv(0x44be00,rnd(4)?lvl:rnd(16));setv(0x451350,rnd(4)?2u:rnd(3));setv(0x43d470,rnd(2)?1u+rnd(60):150u+rnd(150));
            if(rnd(2))p32(Player+0x5c,0);p32(0x64e190u,rnd(4)?rnd(6):0xffffffffu);
        }
        setv(0x4a6cf0,fb(rnd(6)?real(-1,280):-1.0f));
        setv2(0x43eb60,fb(real(-20,20)));setv2(0x43e570,fb(real(0,30)));
        setv3(0x43eb60,rnd(4)?rnd(0x400):0xffffffffu);                        // 43EB60 [arg2]: the polygon index
        // 780228 + kind*4: per-kind polygon -> section word tables (47BA30)
        for(std::uint32_t kd=0;kd<8;++kd)p32(0x780228u+kd*4u,LenTab+(kd&3u)*0x800u);
        for(std::uint32_t q=0;q<0x2000u;q+=2)p16(LenTab+q,rnd(0xb4));
        if(entry==30u){   // 476760 ranking: goal bits, places near the player, lookups that resolve
            p16(Player+0x25cu,rnd(4)?(0x10u<<(4u*rnd(2)))|rnd(0x10):rnd(0x10000));
            for(std::uint32_t id=8;id<33;++id){const std::uint32_t w=id==8u?Player:Works+(id-9u)*0x10f0u;pf(w+0xbdcu,rnd(8)?real(0,1):real(-3,3));if(!rnd(4))p8(w+4u,*reinterpret_cast<std::uint8_t*>(w+4u)|1u);
                p16(w+0x260u,rnd(4)?std::uint32_t(std::max<std::int32_t>(0,std::int32_t(*reinterpret_cast<std::uint16_t*>(Player+0x260))+std::int32_t(rnd(9))-4)):rnd(280));}
            for(std::uint32_t k=0;k<0x1000/0xa0;++k){const std::uint32_t r=Racers+k*0xa0u;
                p32(r+0x4cu,rnd(6)?8u+k:(rnd(2)?0xffffffffu:rnd(40)));p8(r+0x70u,rnd(8)?k:rnd(25));
                if(rnd(2))pf(r+0x40u,real(0,1)+float(*reinterpret_cast<std::uint32_t*>(r+0x3cu)));}
            const std::uint32_t used=rnd(n+3);
            for(std::uint32_t i=0;i<0x400u/0x1cu;++i)p8(Table20+i*0x1cu+0x10u,i<used?(rnd(4)?rnd(9):rnd(256)):0xffu);
            p8(0x80fb24u,rnd(5)?0u:1u);setv(0x495b20,rnd(5));if(rnd(4))p32(Player+0x5cu,1u+rnd(3));setv(0x44b7b0,rnd(5)?1u:0u);
        }
        if(entry==32u){   // 4F9450 collisions: live cars packed around one point, with one or two boxes
            const float cx=real(-200,200),cz=real(-200,200),cy=real(-5,5);
            const std::uint16_t cs=std::uint16_t(10u+rnd(200));
            p32(0x78026cu,rnd(3)?0x10u:rnd(0x14));p32(0x78024cu,rnd(4));
            for(std::uint32_t id=8;id<33;++id){
                const std::uint32_t w=id==8u?Player:Works+(id-9u)*0x10f0u;
                if(id<32u)p8(0x79fb48u+id,rnd(3)?2u:(rnd(2)?1u:0u));
                p32(w+8,rnd(8)?(*reinterpret_cast<std::uint32_t*>(w+8)&~1u):(*reinterpret_cast<std::uint32_t*>(w+8)|1u));
                std::uint32_t f4=*reinterpret_cast<std::uint32_t*>(w+4)&~0x40u;if(!rnd(8))f4|=0x40u;if(rnd(2))f4&=~0x800000u;
                if(id==8u)f4|=1u;p32(w+4,f4);
                pf(w+0x300,rnd(8)?real(-120,1000):real(-500,-131));p8(w+0x329,rnd(8)?rnd(11):rnd(256));p16(w+0xb64,rnd(6)?0u:rnd(100));
                p32(w+0xd14,rnd(4)?0u:rnd(6));
                const float px=cx+real(-3,3),py=cy+real(-0.5f,0.5f),pz=cz+real(-4,4);
                pf(w+0x14,px);pf(w+0x18,py);pf(w+0x1c,pz);
                pf(w+0x16c,px+real(-0.6f,0.6f));pf(w+0x170,py);pf(w+0x174,pz+real(-0.8f,0.8f));
                for(std::uint32_t k:{0x20u,0x24u,0x28u})pf(w+k,real(-1.5f,1.5f));
                pf(w+0x1c4,real(0,2));pf(w+0xd24,rnd(3)?0.0f:real(0,1));pf(w+0x2c8,rnd(4)?real(-1,0.5f):real(-1,30));pf(w+0x2f8,rnd(4)?real(-1,0.5f):real(-1,30));
                pf(w+0x2dc,real(-1,0.5f));pf(w+0x2e8,rnd(3)?0.0f:real(-0.3f,0.3f));
                for(std::uint32_t k:{0x2cu,0x2eu,0x30u,0x17cu,0x17eu,0x180u,0xc2cu,0xc2eu})p16(w+k,rnd(2)?std::uint16_t(rnd(0x800)-0x400):std::uint16_t(rng()));
                p32(w+0x5c,rnd(5)?0u:1u);p16(w+0x64,std::uint16_t(cs+rnd(5)-2));p32(w+0x60,0x64u+rnd(2));p8(w+0x66,rnd(6));p32(w+0x68,rnd(4));
                p8(w+0x335,1u+rnd(2));p8(w+0x10,rnd(8));p32(w+0x324,rnd(7));p32(w+0x320,rnd(5));p32(w+0xb58,rnd(6)?0u:0x12u);p32(w+0xb5c,rnd(6)?0u:0x12u);
                p16(w+0x1030,rnd(3)?0u:rnd(80));p8(w+0xd22,rnd(3)?0u:rnd(80));p8(w+0xda4,rnd(3)?0u:rnd(200));p32(w+0xda0,rnd(3)?0x19au:9u+rnd(23));
                p32(w+0xc60,rnd(2)?0x19au:8u+rnd(24));p16(w+0xb4c,rnd(3)?0u:rnd(80));p16(w+0xb4e,rnd(3)?0u:rnd(80));p32(w+0xae8,rnd(2)?0x19au:9u);
                pf(w+0xdb4,real(0,1));p8(w+0x282,rnd(256));p32(w+0x2f0,rng());pf(w+0x26c,real(-3,3));p32(w+0x230,rnd(3)?rnd(0x400):0xffffffffu);
                for(std::uint32_t q=0;q<16;++q){pf(w+0xc90+q*4,(q%5)==0?1.0f:real(-0.05f,0.05f));pf(w+0xcd0+q*4,(q%5)==0?1.0f:real(-0.05f,0.05f));}
                for(std::uint32_t k=0;k<2;++k){const std::uint32_t b=w+0x338+k*0x1ecu;
                    const float hx=real(0.6f,1.2f),hz=real(1.2f,2.6f);
                    pf(b,hx+real(-0.2f,0.2f)*float(k));pf(b+4,real(0.5f,2));pf(b+8,hz);pf(b+0xc,-hx);pf(b+0x10,real(-0.5f,0));pf(b+0x14,-hz+real(0,0.5f)*float(k));
                    for(std::uint32_t q=0x18;q<0x34;q+=4)pf(b+q,real(-2,2));}
            }
            p32(Player,8);setv(0x45c440,rnd(4)?rnd(0x20):0x14u);setv3(0x43eb60,rnd(0x400));
        }
        if(entry==33u||entry==34u){   // Heart Attack: 46D250 pair records 800AF8, 46DB60 marker ring 8037C8
            p32(0x780258u,2u);p8(0x79fb48u+0x19au,0);
            for(std::uint32_t a=0x802af0u;a<0x803710u;a+=4)pf(a,rnd(4)?real(0,20):real(20,300));
            for(std::uint32_t r=0x800af8u;r<0x800d28u;r+=0x38u){
                auto id=[&]{return rnd(3)?9u+rnd(23):0x19au;};
                p32(r,id());p32(r+4,rnd(6)?id():0x19au);pf(r+0x30,real(-3,3));p8(r+0x34,rnd(8));
            }
            for(std::uint32_t id=8;id<32;++id){const std::uint32_t w=id==8u?Player:Works+(id-9u)*0x10f0u;p16(w+0x160,rng());p16(w+0x64,rnd(300));
                for(std::uint32_t k=0;k<16;++k)pf(w+0xb0u+k*4u,(k%5u)==0u?1.0f:real(-0.5f,0.5f));for(std::uint32_t k:{0xe0u,0xe4u,0xe8u})pf(w+k,real(-400,400));}
            pf(0x804384u,real(-10,10));p16(0x8037b0u,rng());
            setv(0x45c440,rnd(5)?(entry==33u?9u:0xbu):rnd(0x20));setv(0x45c450,rnd(150));setv(0x45c460,150u+rnd(150));
            p32(0x8037b4u,rnd(0x21));p32(0x804354u,rnd(0x21));p32(0x804348u,0);
            for(std::uint32_t k=0;k<32;++k){const std::uint32_t r=0x8037c8u+k*0x5cu;
                p32(r,rnd(2)?0x19au:9u+rnd(23));p32(r+0x4c,rnd(2));p32(r+0x50,rnd(2));for(std::uint32_t q=4;q<0x4c;q+=4)pf(r+q,real(-5,5));p16(r+0x58,rng());}
        }
        if(entry>=35u){   // Heart Attack requests (45E610..461940): NAVI quest block, robots, OSO objects, gates
            p32(0x780258u,2u);
            auto small=[&]{return rnd(3)?rnd(8):(rnd(2)?fb(real(-3,3)):rng());};
            for(std::uint32_t a=0x7f1900u;a<0x7f2a00u;a+=4)p32(a,small());
            for(std::uint32_t a=0x7f8980u;a<0x7f8d80u;a+=4)p32(a,small());
            p32(0x7f2428u,rnd(26));p32(0x7f1c64u,rnd(8));p32(0x7f1994u,rnd(8));pf(0x842110u,real(0,600));p32(0x7f8b40u,rnd(700));
            for(std::uint32_t k=0;k<8;++k){const std::uint32_t q=0x7f1998u+k*12u;p32(q,rnd(4)?0x100u+rnd(0x60):0xffffffffu);p32(q+4,rnd(200));p16(q+8,rnd(10));}
            p32(0x7f23c4u,rnd(100000));p32(0x7f24e8u,rnd(200000));p32(0x7f8aa0u,rnd(400000));p32(0x7f8a90u,rnd(3));p32(0x7f8a9cu,rnd(130));
            p16(0x7f8ab4u,rnd(50));p16(0x7f8ab6u,60u+rnd(300));p32(0x7f8ab0u,rnd(300));p32(0x7f8a98u,rnd(400));p16(0x7f8aacu,rnd(300));
            p16(0x7f1c50u,1u+rnd(20));p32(0x7f2988u,rnd(10));p32(0x7f8b38u,rnd(3));p32(0x7f23dcu,rnd(20));
            p16(0x7f23bcu,rnd(100));p16(0x7f23beu,120u+rnd(200));p32(0x7f23c0u,rnd(1000));
            p32(0x7f1c90u,rnd(2)?0xeu:6u);p16(0x7f1c8cu,1u+rnd(10));p32(0x7f1c74u,rnd(2)?0x10u:0xau);p8(0x7f1c70u,1u+rnd(6));p8(0x7f1ca0u,1u+rnd(6));
            p32(0x7f2538u,rnd(3)?1u:0xffffffffu);p16(0x7f253cu,rnd(3)?0xffffu:rnd(300));p16(0x7f2530u,rnd(50));p16(0x7f2532u,60u+rnd(200));
            p16(0x7f23d0u,1u+rnd(10));p16(0x7f23d2u,rnd(10));p16(0x7f1cb8u,1u+rnd(10));p16(0x7f1cbau,rnd(10));
            p8(0x7f8ad2u,rnd(5));pf(0x7f8abcu,rnd(3)?real(0,1.2f):0.0f);p16(0x7f8ad0u,rnd(3));p16(0x7f8accu,rnd(50));p16(0x7f8aceu,60u+rnd(200));p16(0x7f8ac8u,rnd(100));
            p16(0x7f198eu,1u+rnd(20));p32(0x7f1988u,rnd(100));p16(0x7f8adcu,rnd(6));p16(0x7f8adeu,1u+rnd(9));p32(0x7f8ae8u,rnd(3)?rnd(40):40u+rnd(4));p32(0x7f8af0u,rnd(4));
            p32(0x84df00u,rnd(20));p32(0x84df0cu,rnd(20));p32(0x7d6834u,rnd(2));
            for(std::uint32_t k=0;k<8;++k)p32(0x82f5f0u+k*0xa4u+8u,rng());
            const float px=*reinterpret_cast<float*>(Player+0x14),pz=*reinterpret_cast<float*>(Player+0x1c);
            p32(Player+0xdd4,rnd(2)?0x19au:9u+rnd(23));pf(Player+0x1c4,real(0,3));p8(Player+0xd36,rnd(3)?0u:rnd(5)-2u);
            p32(Player+0x2f0,rnd(3)?0u:rng());p32(Player+4,rnd(3)?(rng()&~0x4580000u):rng());
            for(std::uint32_t o:{0x24cu,0x250u,0x254u,0x258u})p32(Player+o,rnd(2)?0u:rng());
            for(std::uint32_t o:{0x264u,0x268u,0x26cu,0x2e8u})pf(Player+o,real(-2,2));
            {const std::uint32_t b=Player+0x338u;pf(b,1.0f);pf(b+4,1.5f);pf(b+8,2.2f);pf(b+0xc,-1.0f);pf(b+0x10,0.0f);pf(b+0x14,-2.2f);}
            for(std::uint32_t id=0x20u;id<0x19au;++id){p8(0x79fb48u+id,0);p32(0x799b38u+id*0x3cu,0);}
            p32(0x79f010u,Rob);p32(0x79f04cu,Rob+0x100u);p32(Rob,rnd(8));p32(Rob+0x100u,rnd(8));
            static constexpr std::uint32_t kinds[]{5u,6u,0xau,0xeu,0x10u,0x12u,0x16u,0x39u,0x22u};
            for(std::uint32_t k=0;k<16;++k){
                const std::uint32_t id=0x20u+k*3u,w=Objs+k*0x400u;
                p8(0x79fb48u+id,rnd(4)?2u:1u);p32(0x799b38u+id*0x3cu,w);
                for(std::uint32_t q=0;q<0x400;q+=4)p32(w+q,rnd(3)?0u:rng());
                p32(w+0x5c,rnd(5)?kinds[rnd(9)]:rnd(0x40));p32(w+0x3f8,rnd(2));p32(w+0x3fc,rnd(2)?8u:rnd(12));
                p32(w+0x7c,rnd(4));p32(w+0x80,rnd(2)?8u:rnd(12));p32(w+0xbc,rnd(2));p32(w+0xc0,rnd(3)?1u:0u);
                p16(w+0xcc,rnd(300));p16(w+0xce,rnd(12));
                pf(w+0x40,px+real(-20,20));pf(w+0x44,real(-1,6));pf(w+0x48,pz+real(-20,20));pf(w+0xa8,real(-1,4));pf(w+0x74,real(-1,1));
                for(std::uint32_t o:{0xa4u,0xacu,0xb0u,0xb4u,0xb8u})pf(w+o,real(-2,2));
                for(std::uint32_t o:{0x68u,0x6cu,0x70u})pf(w+o,real(-4,4));   // angles stay finite radians in the game
            }
            for(std::uint32_t i=0;i<40;++i){
                const std::uint32_t id=0x134u+i,w=Gates+i*0x100u;
                p8(0x79fb48u+id,rnd(3)?2u:0u);p32(0x799b38u+id*0x3cu,w);
                for(std::uint32_t q=0;q<0x100;q+=4)p32(w+q,rnd(3)?0u:rng());
                p32(w+0x5c,rnd(2)?0x12u:rnd(0x20));p32(w+0x7c,rnd(4));p32(w+0x80,rnd(2)?8u:rnd(12));
                pf(w+0x44,real(-1,6));pf(w+0xa8,real(-1,4));pf(w+0x74,real(-1,1));for(std::uint32_t o:{0x68u,0x6cu,0x70u})pf(w+o,real(-4,4));
            }
            p32(0x7f8a74u,rnd(9));pf(0x7f8a84u,px+real(-3,3));pf(0x7f8a8cu,pz+real(-3,3));
            for(std::uint32_t i=0;i<4;++i){const std::uint32_t e=0x7f1cf0u+i*0x2cu;
                p32(e-0x28u,Objs+rnd(16)*0x400u);p32(e-0x24u,Objs+rnd(16)*0x400u);
                pf(e-0x20u,px+real(-3,3));pf(e-0x1cu,pz+real(-3,3));pf(e-0x18u,px+real(-3,3));pf(e-0x14u,pz+real(-3,3));
                pf(e-0xcu,real(-1,1));pf(e-8u,real(-3,3));p32(e,rnd(3)?0u:(rnd(2)?0x80000001u:1u));}
            for(std::uint32_t r=0x800af8u;r<0x800d28u;r+=0x38u){
                p32(r,rnd(3)?9u+rnd(23):0x19au);p32(r+4,rnd(4)?9u+rnd(23):0x19au);
                pf(r+8,px+real(-4,4));pf(r+0xc,real(-1,2));pf(r+0x10,pz+real(-4,4));pf(r+0x14,px+real(-4,4));pf(r+0x18,real(-1,2));pf(r+0x1c,pz+real(-4,4));}
            for(std::uint32_t k=0;k<32;++k){const std::uint32_t r=0x8037c8u+k*0x5cu;
                p32(r,rnd(2)?0x19au:9u+rnd(23));p32(r+0x4c,rnd(2));p32(r+0x50,rnd(2));
                for(std::uint32_t j=0;j<2;++j){pf(r+0x28+j*0xc,px+real(-6,6));pf(r+0x2c+j*0xc,real(-1,2));pf(r+0x30+j*0xc,pz+real(-6,6));}}
            for(std::uint32_t id=9;id<32;++id){const std::uint32_t w=Works+(id-9u)*0x10f0u;
                p32(w+0xc5c,rng()&~0x40000u);p16(w+0x64,*reinterpret_cast<std::uint16_t*>(Player+0x64)+rnd(15)-5u);p8(w+0xb6c,rnd(4)?0u:1u);p32(w+0x2f4,rnd(2));pf(w+0x1c4,real(0,3));}   // no escorts (+C5C 40000): 45D830 reads 470240's speed output even when it fails (PC stack garbage)
            setv(0x44fe40,rng());
            p32(0x79f574u,Camera);setv(0x428880,rnd(4));p8(Camera+0x34a,rnd(3));pf(Camera+0xb0,real(200,400));p8(0x655b59u,rnd(3));
            p32(0x7f19f8u,rnd(4));p32(0x7f8afcu,rnd(4)?rnd(200000):0u-rnd(60000));p32(0x7f1c3cu,rnd(3)?rnd(200000):0u-rnd(60000));p32(0x7f8c70u,rnd(2));
            p32(0x7f8b20u,rnd(17));p32(0x7f8b24u,rnd(16));
            for(std::uint32_t k=0;k<16;++k){const std::uint32_t e=0x7f2038u+k*0x38u;p32(e,rnd(2));p32(e+4,rnd(7));p32(e+8,rnd(40));p32(e+0x28,rnd(5));p32(e+0x2c,rnd(8));pf(e+0x30,real(0.5f,2));p16(e+0x34,rnd(4));
                for(std::uint32_t q=0xc;q<0x24;q+=4)pf(e+q,real(-200,200));}
            for(std::uint32_t e=0x7f1a1cu;e<0x7f1c4cu;e+=0x1cu){p32(e-0x14u,rnd(2));p16(e+4,rnd(10));}
            p32(0x7f8b58u,rnd(4)?rnd(50):0xffffffffu);p32(0x7f8b50u,rnd(25));p32(0x7f8b4cu,rnd(2));p16(0x7f8b5cu,rnd(4)?rnd(10):0xffffu);p16(0x7f8b5eu,rnd(4)?rnd(10):0xffffu);
            p32(0x7f8b60u,rnd(8));p32(0x7f8b64u,rnd(8));p16(0x7f8bb4u,rnd(3));p16(0x7f8bb8u,rnd(40));p16(0x7f8bbau,rnd(3));pf(0x7f8b70u,real(-80,80));pf(0x7f8b6cu,real(0,200));
            // request manager: quest tables, stage start / goal bits, pointers that must stay valid
            setv(0x43eb60,rnd(4));setv(0x44c940,rnd(5));setv(0x44dc50,rnd(0x28));setv(0x44ddc0,rnd(4)?rnd(300):0xffffffffu);setv(0x440370,rnd(4)?0u:1u);
            setv(0x429780,0);setv2(0x429780,rnd(30));setv3(0x429780,rnd(30));
            for(std::uint32_t j=0;j<12;++j)pf(CornerVals+j*4u,real(-30,30));
            p32(0x7d39f0u,rnd(4)?0u:rng());for(std::uint32_t j=0;j<12;++j)pf(0x7d3830u+j*4u,real(0.5f,2));p32(0x7d2698u,rnd(5));
            for(std::uint32_t j=0;j<16;++j)pf(0x7d2da0u+j*4u,(j%5u)==0u?1.0f:real(-0.2f,0.2f));
            p32(0x7f2430u,0x5a94d8u+rnd(0x28)*0x140u+rnd(8)*10u);p32(0x7f24e4u,rnd(12));p32(0x7f2470u,rnd(50));p32(0x7f2478u,rnd(2));
            p32(0x7f24b8u,rnd(3)?0u:0x63ee68u+rnd(0x20)*0x320u);p32(0x7f24c0u,rnd(0x100));p32(0x7f24bcu,rnd(0x70));
            p32(0x7f2528u,rnd(3)?0u:0x461940u);p32(0x7f24d0u,0x7f8c80u);p32(0x7f24d4u,0x7f8cbcu);p32(0x7f24ccu,rnd(4));p32(0x7f24c8u,rnd(4));
            for(std::uint32_t l=0;l<5;++l){const std::uint32_t r=0x7f2548u+l*0x44u;p8(r,rnd(3)?rnd(4):0xffu);p8(r+0x40,rnd(4));for(std::uint32_t j=0;j<6;++j)p8(r+0x38+j,rnd(4)?0u:1u);}
            p32(0x7f2468u,rnd(10));p32(0x7f2460u,rnd(20)-10u);p32(0x7f2464u,rnd(20)-10u);for(std::uint32_t j=0;j<10;++j)p32(0x7f2438u+j*4u,rnd(30)-10u);
            p32(0x680ad8u,1u+rnd(8));
            // Heart Attack display (entries 55..62): result sheet timer, face panel, level grades
            p32(0x7f8b34u,rnd(0x300));p32(0x7f8b28u,rnd(200)-50u);p16(0x7f8b2cu,rnd(8));p32(0x7f24c8u,rnd(4));
            for(std::uint32_t l=0;l<5;++l){const std::uint32_t r=0x7f2548u+l*0x44u;p8(r+0x40,1u+rnd(4));for(std::uint32_t j=0;j<6;++j)p32(r+8u+j*4u,rnd(8));p32(r+0x1c,rnd(40));}
            for(std::uint32_t k=0;k<4;++k){const std::uint32_t r=0x7f8cf8u+k*0x38u;
                for(std::uint32_t q=4;q<0x38;q+=4)pf(r+q,real(-5,5));p32(r,rnd(4)?1u:0u);p32(r+0x2c,0x570000u+rnd(0x40));}
            p32(0x7f24d0u,rnd(4)?0x7f8cf8u:0u);p32(0x7f24d4u,rnd(5)?0x7f8d30u:0u);p32(0x7f24d8u,rnd(2)?0u:0x7f8d68u);p32(0x7f24dcu,rnd(2)?0u:0x7f8da0u);
            p32(0x7f2524u,rnd(2)?0u:0x45ca70u);
            setv(0x4294c0,rnd(4)?0u:0xffffffffu);setv(0x43fa00,rnd(4));
            if(entry>=64u){   // score control 4F2DF0 / 4F2B20 / 4F2860 / 4F2660 / 4F2CD0 (with 4B9770 / 4B9980)
                p32(0x780258u,rnd(4)?2u:(rnd(2)?9u:3u+rnd(2)));p32(0x78026cu,rnd(3)?0x10u:(rnd(2)?0x13u:0x1bu));
                setv(0x4957f0,rnd(5)?0u:1u);setv(0x4b00d0,rnd(2));setv(0x4b00e0,rnd(2));setv(0x44ff10,rnd(5)?0u:1u);p32(0x7d39f0u,rnd(4)?(rng()&~2u):rng());
                setv(0x450570,rnd(6000));setv(0x55a930,rnd(4)?0u:1u);setv(0x44c940,rnd(5));setv(0x49b2d0,rnd(120));setv(0x45a2b0,rnd(8));
                p8(0x79fb50u,rnd(5)?2u:rnd(4));p32(0x680ad4u,rnd(6));p32(0x680ad8u,rnd(18));p32(0x68625cu,rnd(10));
                p32(0x84df04u,rnd(3)?0u:1u);for(std::uint32_t a=0x84df1cu;a<=0x84df34u;a+=4)p32(a,rnd(2)?0u:rnd(80));p8(0x84df10u,rnd(8));
                for(std::uint32_t k=0;k<10;++k){const std::uint32_t r=0x8444c8u+k*0x24u;p32(r+0xc,rnd(3)?0u:(rnd(2)?rnd(0x120):0u-rnd(5)));}
                for(std::uint32_t k=0;k<0x20;++k)p32(0x8447dcu+k*4u,rnd(100000));p32(0x8446fcu,rnd(100000));pf(0x84bcf8u,0.0f);p32(0x84bcf8u,rnd(1000));
                const float px=*reinterpret_cast<float*>(Player+0x14),pz=*reinterpret_cast<float*>(Player+0x1c);
                const std::uint16_t p260=std::uint16_t(rnd(3000));
                p16(Player+0x260,p260);p16(Player+0x262,rnd(5));p32(Player+4,rng()&~0x4580000u);p32(Player+0xdd4,8u+rnd(24));p32(Player+0xc,rng());
                for(std::uint32_t o:{0x20u,0x24u,0x28u})pf(Player+o,real(-1,1));
                p32(Player+0x244,rnd(3)?0u:rng());pf(Player+0xe68,real(0,0.4f));p16(Player+0x162,rnd(3000)-1500u);pf(Player+0x1c4,real(0,3));pf(Player+0xe64,real(0,3));
                p16(Player+0x64,rnd(300));p16(Player+0x18c,rnd(300));p8(Player+0xc36,rnd(5));p32(Player+0x2f0,rng());p8(Player+6,rnd(2));p32(Player+0x68,0x100u+rnd(4));
                for(std::uint32_t id=9;id<32;++id){const std::uint32_t w=Works+(id-9u)*0x10f0u;
                    p8(0x79fb48u+id,rnd(4)?2u:1u);p32(0x799b38u+id*0x3cu,w);p32(w,id);
                    p16(w+0x260,std::uint16_t(p260+rnd(8)-4u));p16(w+0x262,rnd(5));p32(w+0x324,rnd(7));p32(w+0xc5c,rng()&(rnd(2)?0xff3fffbfu:0xffffffffu));p16(w+0xb72,rnd(3)?0u:rnd(100));
                    p8(w+0x11,rng());p8(w+0x12,rnd(5));pf(w+0x14,px+real(-20,20));pf(w+0x18,real(-2,2));pf(w+0x1c,pz+real(-20,20));}
            }
            if(entry==69u){   // 4F0E40: gate records (4F0BD0) set up by 4F0920 / 4F0C30, then object visibility
                setv(0x417f70,rng());setv(0x44bec0,0x7d3190u);
                for(std::uint32_t k=0;k<16;++k)pf(0x7d3190u+k*4u,(k%5u)==0u?1.0f:real(-0.2f,0.2f));
                const float px=*reinterpret_cast<float*>(Player+0x14),pz=*reinterpret_cast<float*>(Player+0x1c);
                auto record=[&](std::uint32_t st,bool gates){const std::uint32_t r=st-0x28u;
                    p32(st,rnd(3));p32(st+4,gates?(rnd(3)?0x61u:(rnd(2)?0x4bu:0x99u)):(rnd(4)?0x4bu:0x99u));p32(st+8,rnd(2));
                    pf(r,px+real(-30,30));pf(r+4,real(-2,4));pf(r+8,pz+real(-30,30));for(std::uint32_t o:{0xcu,0x10u,0x14u})pf(r+o,real(-3,3));
                    p32(r+0x18,rnd(0x30));p32(r+0x20,rnd(3));p32(r+0x24,rnd(4));};
                for(std::uint32_t k=0;k<0x28u;++k)record(0x84cec8u+k*0x34u,true);
                for(std::uint32_t k=0;k<0xau;++k)record(0x84d700u+k*0x34u,false);
                p32(0x84d8e0u,rnd(1000));p32(0x84d8f0u,rnd(2));
                for(std::uint32_t i=0;i<40;++i){const std::uint32_t w=Gates+i*0x100u;pf(w+0x4c,real(0.5f,3));pf(w+0x50,real(0.5f,2));
                    for(std::uint32_t o:{0x68u,0x6cu,0x70u})pf(w+o,real(-3,3));}
                for(std::uint32_t k=0;k<16;++k){const std::uint32_t w=Objs+k*0x400u;pf(w+0x4c,real(0.5f,3));pf(w+0x50,real(0.5f,2));}
            }
            if(entry==63u){   // 46ED20: state 9 pair markers
                setv(0x45c440,rnd(4)?9u:rnd(20));pf(0x804384u,real(-3,3));
                for(std::uint32_t r=0x800af8u;r<0x800d28u;r+=0x38u){pf(r+0x20,real(-3,3));pf(r+0x24,real(-3,3));pf(r+0x2c,real(0.5f,3));pf(r+0x30,real(-1,3));}
            }
            pf(0x7f1c38u,real(-3,3));pf(0x842110u,real(0,600));
            p32(0x7f8bc8u,rnd(2));p32(0x7f8bc4u,Player);
            p16(0x7f8a54u,rnd(12));for(std::uint32_t k=0;k<24;++k)p32(0x7f89a8u+k*4u,9u+rnd(23));
            for(std::uint32_t k=0;k<3;++k)p32(0x7f8a14u+k*4u,rnd(3)?9u+rnd(23):(rnd(2)?8u:0x19au));p16(0x7f8a56u,rnd(8));p16(0x7f8a5cu,rnd(3));p16(0x7f8a5eu,rnd(3));
        }
        if(entry==31u){   // 504E70 wall probes: the kinds 43EB60 / 43F110 report, the car's corner points and gates
            static constexpr std::uint32_t kinds[]{1u,0x40u,0x2000u,0x10000u,0x4u,0x20u,0x1000000u,0x10u};
            setv(0x43eb60,kinds[rnd(8)]);setv(0x43f110,kinds[rnd(8)]);setv3(0x43eb60,rnd(0x400));setv3(0x43f110,rnd(0x400));
            pf(car+0x268,real(-20,20));pf(car+0x264,real(-20,20));if(rnd(3))p32(car+0x5c,0);
            for(std::uint32_t k:{0x338u,0x33cu,0x340u,0x344u,0x348u,0x34cu})pf(car+k,real(-3,3));
            pf(car+0xd24,rnd(2)?real(0,1):0.0f);pf(car+0x2c8,rnd(2)?real(0,2):0.0f);pf(car+0x2e8,real(-3,3));
            for(std::uint32_t k:{0x20u,0x24u,0x28u})pf(car+k,real(-2,2));
            if(rnd(4)==0){p32(car+0x5c,1);p16(car+0x64,0x29u+rnd(16));p8(car+4,*reinterpret_cast<std::uint8_t*>(car+4)|0x20u);}
        }
        std::memcpy(reinterpret_cast<void*>(values),value.data(),value.size()*4);
        std::memcpy(reinterpret_cast<void*>(values2),value2.data(),value2.size()*4);
        std::memcpy(reinterpret_cast<void*>(values3),value3.data(),value3.size()*4);
        std::memcpy(reinterpret_cast<void*>(values4),value4.data(),value4.size()*4);
        reset_matrix_oracle_globals();
        for(std::uint32_t k=0;k<16;++k){pf(MatrixArena+0x1000u+k*4u,(k%5u)==0u?1.0f:real(-0.3f,0.3f));pf(MatrixArena+0x1040u+k*4u,(k%5u)==0u?1.0f:real(-0.3f,0.3f));}
        for(std::uint32_t k:{0x1030u,0x1034u,0x1038u,0x1070u,0x1074u,0x1078u})pf(MatrixArena+k,real(-400,400));
        for(auto& mi:mirrors){mi.bytes.resize(mi.end-mi.base);std::memcpy(mi.bytes.data(),reinterpret_cast<void*>(mi.base),mi.bytes.size());}
        static constexpr std::uint32_t Pc[]{0x47dac0,0x477510,0x477c90,0x47c8a0,0x47d8b0,0x4ef890,0x47ec00,0x47eba0,0x4791b0,0x479500,0x479580,0x47ca40,0x477e00,0x478b90,0x4787e0,
            0x47e6c0,0x476260,0x474d30,0x470360,0x470570,0x470760,0x472450,0x472fe0,0x47a7c0,0x47e110,0x4739f0,0x473f30,0x47dc10,0x47bc00,0x47ad30,0x476760,0x504e70,0x4f9450,0x46d250,0x46db60,
            0x45e610,0x45f3d0,0x45f540,0x45e8a0,0x45eeb0,0x45efb0,0x45f180,0x4617a0,0x461850,0x462410,0x4626e0,0x45cca0,0x461940,
            0x45fb80,0x45b950,0x460ad0,0x464d20,0x464350,0x45f7f0,0x45d2a0,
            0x462cd0,0x4603f0,0x45b3d0,0x45d190,0x45bdf0,0x45b2a0,0x45eca0,0x45e9b0,0x46ed20,
            0x4f2df0,0x4f2b20,0x4f2860,0x4f2660,0x4f2cd0,0x4f0e40};
        const std::uint32_t rec=Rec+rnd(4)*0x14u,other=rnd(2)?Player:Works+rnd(23)*0x10f0u;
        // PC registers / stack words and the native (a0, a1, a2) of each entry
        std::uint32_t r_eax=0,r_ecx=0,r_ebx=0,r_esi=0,r_edi=0;std::array<std::uint32_t,3> stk{Player,rec,0};
        std::uint32_t n0=Player,n1=rec,n2=Rec2;
        switch(entry){
        case 15:case 17:case 26:case 28:case 29:r_eax=car;n0=car;break;
        case 16:stk={other,car,0};n0=other;n1=car;break;
        case 18:stk={car,0,0};n0=car;break;
        case 19:r_ebx=car;n0=car;break;
        case 20:r_esi=car;n0=car;break;
        case 21:stk={car,rec,Rec2};n0=car;break;
        case 22:case 23:r_eax=car;stk={Rec2,0,0};n0=car;break;
        case 24:r_ecx=car;r_eax=rec;r_esi=Rec2;n0=car;break;
        case 25:r_eax=car;r_edi=Rec2;r_ebx=rec;n0=car;break;
        case 27:r_eax=car;stk={rec,Rec2,0};n0=car;break;
        case 30:stk={other,0,0};n0=other;break;
        case 31:stk={car,0,0};n0=car;break;
        case 65:{const std::uint32_t t=rnd(4)?rnd(120):rng();stk={t,0,0};n0=t;break;}
        default:break;
        }
        if(std::getenv("OR2_TRAFFIC_TRACE"))std::cerr<<"iter "<<iter<<" entry "<<entry<<char(10);
        if(std::getenv("OR2_TRAFFIC_DEBUG")){for(std::uint32_t s=0;s<4;++s){const std::uint32_t b=g32(0x6a6ce0u+s*4u);std::cerr<<std::hex<<"stage "<<s<<" @"<<b<<":";for(std::uint32_t k=0;k<0x30;k+=4)std::cerr<<" "<<g32(b+k);std::cerr<<char(10);}
            for(std::uint32_t m=0;m<0x2d;++m)std::cerr<<std::hex<<" m"<<m<<"="<<g32(0x650500u+m*0x44u);std::cerr<<std::dec<<char(10);}
        const unsigned from=std::getenv("OR2_TRAFFIC_FROM")?unsigned(std::stoul(std::getenv("OR2_TRAFFIC_FROM"))):0u;
        if(iter<from)continue;
        const bool native_first=from&&iter==from&&std::getenv("OR2_TRAFFIC_NATIVE");
        // native
        PcRaceMemory mem;std::vector<std::vector<std::uint8_t>> native(mirrors.size());
        for(std::size_t i=0;i<mirrors.size();++i){native[i]=mirrors[i].bytes;mem.map(mirrors[i].base,native[i].data(),native[i].size());}
        std::vector<std::uint8_t> scratch(TrafficStackSize);mem.map(TrafficStackBase,scratch.data(),scratch.size());
        std::vector<std::uint8_t>& arena=native[mirrors.size()-2];
        outrun::driving::PcMatrixStack stack{outrun::driving::Bytes(arena.data(),0x1000),0,0,64};
        struct NCall{std::uint32_t pc,ecx;decltype(PcRaceCall::args) a;};std::vector<NCall> calls;std::map<std::size_t,std::array<std::uint32_t,16>> nmtx;
        PcRaceContext ctx{mem,stack,[&](const PcRaceCall& k)->std::uint32_t{
            const auto it=index_of.find(k.pc);if(it==index_of.end()){std::ostringstream s;s<<"race traffic probe: unexpected service "<<std::hex<<k.pc;throw std::runtime_error(s.str());}
            const auto& e=services[it->second];
            if(k.pc==0x4287b0u||k.pc==0x4289e0u){const std::uint32_t mp=k.pc==0x4287b0u?k.args[1]:k.args[2];std::array<std::uint32_t,16> q;for(unsigned j=0;j<16;++j)q[j]=mem.u32(mp+j*4u);nmtx[calls.size()]=q;}
            if(e.kind==20){const auto cur=stack.current();std::array<std::uint32_t,16> q;for(unsigned j=0;j<16;++j)q[j]=cur.u32(j*4u);nmtx[calls.size()]=q;}
            if(e.kind==23){std::array<std::uint32_t,16> q{};for(unsigned j=0;j<4;++j)q[j]=mem.u32(k.args[3]+j*4u);for(unsigned j=4;j<16;++j)q[j]=*reinterpret_cast<std::uint32_t*>(MtxTrace+calls.size()*64u+j*4u);nmtx[calls.size()]=q;}
            calls.push_back({k.pc,k.ecx,k.args});
            if(e.kind==10){if(k.args[4])mem.put32(k.args[4],value[it->second]);mem.put32(k.args[1]+4,value2[it->second]);if(k.args[2])mem.put32(k.args[2],value3[it->second]);}
            if(e.kind==11){mem.put32(k.args[0]+0x54,value2[it->second]);mem.put32(k.args[0]+8,value2[it->second]);mem.put32(k.args[0]+0xc,value3[it->second]);mem.put32(k.args[0]+0x10,value4[it->second]);
                for(unsigned j=0;j<12;++j)mem.put32(k.args[0]+0x24+j*4u,mem.u32(CornerVals+j*4u));}
            if(e.kind==18){if(k.args[1])mem.put32(k.args[1],value2[it->second]);if(k.args[2])mem.put32(k.args[2],value3[it->second]);}
            if(e.kind==15){mem.put32(k.args[2],value2[it->second]);mem.put32(k.args[2]+4,value3[it->second]);mem.put32(k.args[2]+8,value4[it->second]);}
            if(e.kind==12)return std::uint32_t(mem.u8(0x8367c0u+k.args[0]));
            if(e.kind==16){if(k.args[5])mem.put32(k.args[5],value[it->second]);mem.put32(k.args[2]+4,value2[it->second]);if(k.args[3])mem.put32(k.args[3],value3[it->second]);}
            if(e.kind==13){mem.put32(k.args[0],value2[it->second]);mem.put32(k.args[0]+4,value3[it->second]);mem.put32(k.args[0]+8,value4[it->second]);}
            return value[it->second];},nullptr};
        bool ok=true;std::string why;std::uint32_t native_ret=0;
        try{
            switch(entry){
            case 0:traffic_init_47dac0(ctx);break;
            case 1:traffic_ranks_477510(ctx);break;
            case 2:traffic_order_477c90(ctx);break;
            case 3:traffic_appear_47c8a0(ctx);break;
            case 4:traffic_spawn_47d8b0(ctx);break;
            case 5:traffic_tables_4ef890(ctx);break;
            case 6:traffic_control_47ec00(ctx);break;
            default:native_ret=traffic_control_part(ctx,Pc[entry],n0,n1,n2)&0xffu;break;
            }
        }catch(const std::exception& e){ok=false;why=e.what();}
        if(native_first){std::cerr<<"native: "<<(ok?"ok":why)<<" calls:";for(auto& cl:calls){std::cerr<<" "<<std::hex<<cl.pc<<"(";for(unsigned k=0;k<5;++k)std::cerr<<cl.a[k]<<",";std::cerr<<")";}std::cerr<<std::dec<<char(10);continue;}
        std::uint32_t pc_ret=0;
        if(!native_first){
            // a case the PC would loop on forever (detected natively) is not run on the PC
            if(!ok&&why.find("endless")!=std::string::npos){++coverage["skipped: PC endless loop"];continue;}
           prepare(Pc[entry]);p32(S,stk[0]);p32(S+4,stk[1]);p32(S+8,stk[2]);
            guest_call.eax=r_eax;guest_call.ecx=r_ecx;guest_call.ebx=r_ebx;guest_call.esi=r_esi;guest_call.edi=r_edi;
            static unsigned trace_at;trace_at=trace;
            g_signal_dump=[]{const unsigned nc=*reinterpret_cast<std::uint32_t*>(trace_at);dprintf(2,"PC faulted after %u services:",nc);
                for(unsigned i=0;i<nc&&i<64;++i){const auto* g=reinterpret_cast<const std::uint32_t*>(trace_at+64+i*0x40);dprintf(2," %x(%x,%x)",g[0],g[2],g[3]);}
                dprintf(2,"%c",10);};
            if(entry==32u||entry==6u||entry==7u||entry==51u||entry==52u||entry>=55u)std::memset(reinterpret_cast<void*>(0x31001000u),0,S-0x31001000u);   // 4F9450 reads stale locals: a zeroed stack, as the native frames
            run();g_signal_dump=nullptr;pc_ret=guest_call.out_eax&0xffu;}
        const std::string tag="entry "+std::to_string(entry)+" run "+std::to_string(iter);
        auto stackish=[&](std::uint32_t pc_arg,std::uint32_t nat){return pc_arg>=S-0x20000u&&pc_arg<S+0x1000u&&nat>=TrafficStackBase&&nat<TrafficStackBase+TrafficStackSize;};
        ++cases;if(!ok){if(why.find(" not ported")!=std::string::npos){++coverage["reached unported "+why.substr(14,6)];continue;}fail(tag+" native threw: "+why);continue;}
        if(entry==13u&&pc_ret!=native_ret){fail(tag+" result PC="+std::to_string(pc_ret)+" native="+std::to_string(native_ret));continue;}
        if(inject&&iter==13)native[0][0x2e08u-0x0a00u]^=1;
        {   // 478641 bridge: [803748] = the place pointer (a stack address on both sides)
            std::uint32_t pcv=g32(0x803748u),nv;std::memcpy(&nv,native[0].data()+(0x803748u-0x800a00u),4);
            if(pcv!=nv&&stackish(pcv,nv))std::memcpy(native[0].data()+(0x803748u-0x800a00u),&pcv,4);
        }
        if(entry==32u||entry==6u||entry==7u){   // 4F9450 copies whole 0xD4 hit records from its stack: the fields of edges that did not hit are
            // uninitialised locals (here the harness's own stack contents) and are never read (readers test +4 first).
            for(std::size_t i=0;i<mirrors.size();++i){
                if(mirrors[i].base!=Works&&mirrors[i].base!=Player)continue;
                const std::uint32_t cars=mirrors[i].base==Works?23u:1u;
                for(std::uint32_t cidx=0;cidx<cars;++cidx)for(std::uint32_t k=0;k<2;++k)for(std::uint32_t r=0;r<2;++r)for(std::uint32_t e=0;e<4;++e){
                    const std::uint32_t off=cidx*0x10f0u+0x36cu+k*0x1ecu+r*0xd4u+4u+e*0x34u;
                    if(off+0x34u>native[i].size())continue;
                    const auto* pc=reinterpret_cast<const std::uint8_t*>(mirrors[i].base+off);
                    std::uint32_t hit;std::memcpy(&hit,pc+4,4);
                    if(hit)continue;
                    std::memcpy(native[i].data()+off,pc,4);std::memcpy(native[i].data()+off+0xc,pc+0xc,0x28);
                }
            }
        }
        for(std::size_t i=0;i<mirrors.size();++i){std::ostringstream s;s<<tag<<" region "<<std::hex<<mirrors[i].base;
            compare(native[i].data(),reinterpret_cast<void*>(mirrors[i].base),native[i].size(),s.str());}
        const unsigned nc=g32(trace);++cases;
        if(nc!=calls.size()){std::ostringstream s;s<<tag<<" service count PC="<<nc<<" native="<<calls.size()<<" |";
            for(unsigned i=0;i<nc&&i<40;++i)s<<" "<<std::hex<<g32(trace+64+i*0x40);s<<" | native";for(auto& cl:calls)s<<" "<<std::hex<<cl.pc;fail(s.str());}
        else for(unsigned i=0;i<nc;++i){const auto* g=reinterpret_cast<const std::uint32_t*>(trace+64+i*0x40);
            const auto& e=services[index_of[g[0]]];bool same=g[0]==calls[i].pc;
            if(g[0]==0x55a930u)same=same&&g[1]==calls[i].ecx;
            for(unsigned k=0;k<e.argc;++k)same=same&&(g[2+k]==calls[i].a[k]||stackish(g[2+k],calls[i].a[k]));
            if(e.kind==23){   // the string only: bytes after its NUL are stack contents
                auto cut=[](std::uint8_t* b){bool z=false;for(unsigned j=0;j<16;++j){if(z)b[j]=0;else if(!b[j])z=true;}};
                cut(reinterpret_cast<std::uint8_t*>(MtxTrace+i*64u));if(auto it=nmtx.find(i);it!=nmtx.end())cut(reinterpret_cast<std::uint8_t*>(it->second.data()));}
            if(same&&(e.kind==17||e.kind==19||e.kind==20||e.kind==23)){const auto it=nmtx.find(i);same=it!=nmtx.end()&&!std::memcmp(it->second.data(),reinterpret_cast<void*>(MtxTrace+i*64u),64);
                if(!same){std::ostringstream s;s<<tag<<" service #"<<i<<" 4287B0 matrix";if(it!=nmtx.end())for(unsigned j=0;j<16;++j)s<<" "<<std::hex<<g32(MtxTrace+i*64u+j*4u)<<"/"<<it->second[j];fail(s.str());break;}}
            if(!same){std::ostringstream s;s<<tag<<" service #"<<i<<" "<<std::hex<<g[0];for(unsigned k=0;k<e.argc;++k)s<<" PC "<<g[2+k]<<"/native "<<calls[i].a[k];fail(s.str());break;}}
        ++coverage["entry "+std::to_string(entry)];
        for(auto& mi:mirrors)if(mi.base==Works)for(std::uint32_t k=0;k<23;++k){const std::uint32_t o=k*0x10f0u+4u;
            const bool was=mi.bytes[o]&0x80u,now=*reinterpret_cast<std::uint8_t*>(Works+o)&0x80u;if(was!=now)++coverage[now?"view: became visible":"view: became hidden"];}
        if(std::getenv("OR2_TRAFFIC_LAST")){std::ostringstream s;s<<"e"<<entry<<" last "<<std::hex<<(calls.empty()?0u:calls.back().pc)<<" n"<<std::dec<<calls.size();++coverage[s.str()];}
        for(auto& cl:calls){if(cl.pc==0x440180u)++coverage["car opened"];if(cl.pc==0x440200u)++coverage["car released"];if(cl.pc==0x43e570u)++coverage["478430 lanes"];if(cl.pc==0x440110u)++coverage["traffic opened"];
            if(entry==29u){if(cl.pc==0x451350u)++coverage["47AD30 route"];if(cl.pc==0x495880u)++coverage["47AD30 495880"];if(cl.pc==0x44f0f0u)++coverage["47AD30 preload"];
                if(cl.pc==0x44bed0u)++coverage["47AD30 lane offsets"];if(cl.pc==0x44dc50u)++coverage["47AD30 sections"];}}
        if(entry==30u){for(auto& cl:calls){if(cl.pc==0x495800u)++coverage["476760 player entry"];if(cl.pc==0x495b20u)++coverage["476760 sorted"];if(cl.pc==0x4505d0u)++coverage["476760 ranked"];}
            unsigned added=0;for(auto& mi:mirrors)if(mi.base==Table20)for(std::uint32_t i=0;i<0x400u/0x1cu;++i)if(*reinterpret_cast<std::uint8_t*>(Table20+i*0x1cu+0x10u)!=mi.bytes[i*0x1cu+0x10u])++added;
            if(added)++coverage["476760 entries added"];if(added>2)++coverage["476760 3+ entries"];}
        if(entry==32u){for(auto& cl:calls){if(cl.pc==0x4a2650u)++coverage["4F9450 separated"];if(cl.pc==0x4a6ea0u)++coverage["4F9450 crash state"];
                if(cl.pc==0x424940u)++coverage["4F9450 SE"];if(cl.pc==0x46c780u)++coverage["4F9450 impact feedback"];if(cl.pc==0x43d470u)++coverage["4F9450 4F6FE0 tail"];}
            for(auto& mi:mirrors)if(mi.base==Works)for(std::uint32_t k=0;k<23;++k){const std::uint32_t o=k*0x10f0u+4u;
                const std::uint32_t was=*reinterpret_cast<const std::uint32_t*>(mi.bytes.data()+o),now=*reinterpret_cast<std::uint32_t*>(Works+o);
                if(!(was&0x4000000u)&&(now&0x4000000u))++coverage["4F9450 velocity exchanged"];if(!(was&0x10000u)&&(now&0x10000u))++coverage["4F9450 pushed"];}}
        if(entry==33u){unsigned live=0;for(std::uint32_t r=0x800af8u;r<0x800d28u;r+=0x38u)if(g32(r)!=0x19au)++live;++coverage["46D250 open pairs="+std::to_string(live)];
            for(auto& mi:mirrors)if(mi.base==0x800a00u)for(std::uint32_t r=0x800af8u;r<0x800d28u;r+=0x38u){std::uint32_t was;std::memcpy(&was,mi.bytes.data()+(r-0x800a00u),4);
                if(was==0x19au&&g32(r)!=0x19au)++coverage["46D250 new pair"];if(was!=0x19au&&g32(r)!=0x19au)++coverage["46D250 rescored"];}}
        if(entry==34u){for(auto& mi:mirrors)if(mi.base==0x800a00u)for(std::uint32_t r=0x8037c8u;r<0x804348u;r+=0x5cu){std::uint32_t was;std::memcpy(&was,mi.bytes.data()+(r-0x800a00u),4);
                if(was==0x19au&&g32(r)!=0x19au)++coverage["46DB60 new marker"];if(was!=0x19au&&g32(r)==0x19au)++coverage["46DB60 freed"];}}
        if(entry==69u){auto was=[&](std::uint32_t a){for(auto& mi:mirrors)if(a>=mi.base&&a+4<=mi.end){std::uint32_t v;std::memcpy(&v,mi.bytes.data()+(a-mi.base),4);return v;}return 0u;};
            for(std::uint32_t k=0;k<0x28u;++k){const std::uint32_t st=0x84cec8u+k*0x34u;if(was(st)==1u&&g32(st)==2u)++coverage[g32(st+4)==0x61u?"4F0920 hoop":"4F0920 set up"];if(was(st)==2u&&g32(st)==0u)++coverage["4F0920 closed"];}
            for(std::uint32_t k=0;k<16;++k){const std::uint32_t w=Objs+k*0x400u;if(g32(w+0x54)&1u)++coverage["visible"];else ++coverage["hidden"];}}
        if(entry>=64u){auto was=[&](std::uint32_t a){for(auto& mi:mirrors)if(a>=mi.base&&a+4<=mi.end){std::uint32_t v;std::memcpy(&v,mi.bytes.data()+(a-mi.base),4);return v;}return 0u;};
            if(g32(0x84df0cu)!=was(0x84df0cu))++coverage["score passes"];
            for(std::uint32_t k=0;k<10;++k)if(g32(0x8444d4u+k*0x24u)!=was(0x8444d4u+k*0x24u)){++coverage["score message"];break;}
            if(g32(0x8447fcu)!=was(0x8447fcu))++coverage["player score"];
            if(g32(0x84bcf8u)!=was(0x84bcf8u))++coverage["miles"];
            if(g32(0x84df34u)!=was(0x84df34u))++coverage["drift frames"];}
        if(entry>=35u){for(auto& cl:calls){if(cl.pc==0x424940u)++coverage["HA sound"];if(cl.pc==0x43eb60u)++coverage["HA 461940 pieces"];}
            for(auto& mi:mirrors)if(mi.base==0x7f8a00u){std::uint32_t was;std::memcpy(&was,mi.bytes.data()+(0x7f8afcu-0x7f8a00u),4);if(was!=g32(0x7f8afcu))++coverage["HA result "+std::to_string(entry)];}
            for(auto& mi:mirrors)if(mi.base==0x7f1900u){std::uint32_t was;std::memcpy(&was,mi.bytes.data()+(0x7f23c4u-0x7f1900u),4);if(was!=g32(0x7f23c4u))++coverage["HA hearts "+std::to_string(entry)];}}
        if(entry==31u){unsigned probes=0;bool wall=false;for(auto& cl:calls){if(cl.pc==0x43eb60u||cl.pc==0x43f110u)++probes;if(cl.pc==0x5041b0u)wall=true;if(cl.pc==0x43f110u)++coverage["504E70 43F110 branch"];}
            ++coverage["504E70 probes="+std::to_string(probes)];if(wall)++coverage["504E70 wall"];}
        if(entry==29u){const std::uint32_t c30=*reinterpret_cast<std::uint8_t*>(car+0xc30),c66=*reinterpret_cast<std::uint8_t*>(car+0x66);
            ++coverage["47AD30 c30="+std::to_string(c30)];if(c66!=0)++coverage["47AD30 c66 nonzero"];}
    }
    std::cout<<"{\"routine\":\""<<only<<"\",\"coverage\":{";bool first=true;
    for(const auto& [k,v]:coverage){std::cout<<(first?"":",")<<"\""<<k<<"\":"<<v;first=false;}
    std::cout<<"},\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}"<<std::endl;
    return mismatches?1:0;
}

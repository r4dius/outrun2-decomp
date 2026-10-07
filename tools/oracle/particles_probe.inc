// Included inside the compare_pc dispatcher; uses its private x86 harness.
if(only=="particles_probe"){
    // Original event 397 PART_EFC (41BBF0 init, 41BC60 control over several
    // frames, the effect requests and every particle function reached by
    // injected particles) against race_particles. The module code runs
    // unmodified with the protector sections mapped (VM entries 418420 and
    // 41FB17, relocated snippets 40EAAD/447CC9/448486/40E926 live); the matrix
    // leaves run the pinned D3DX generic routines. Callees outside the module
    // are capture stubs (428320, 424940, 43E3B0 writing a scripted road info,
    // IDirect3DTexture9::GetLevelDesc writing a scripted size); the CRT
    // thread data 585BEC is a private block (rand state +0x14). Compared:
    // the module .data/.bss, the glow colours, the tire-mark part, the car /
    // camera / tire fixtures, the pool records of the 427xxx module, the
    // matrix arena and pointer, the ordered service calls.
    using namespace outrun::platform;
    if(argc<=7||std::string(argv[7])=="-")throw std::runtime_error("particles oracle requires pinned d3dx9_29.dll");
    map_original_arithmetic_bridge(file);
    {   unsigned restored=0;
        const std::size_t text_file=pe_file_offset(file,0x401000u);
        for(std::uint32_t a=0x401000u;a<0x401000u+0x194881u;a+=0x100u){
            const std::size_t n=std::min<std::size_t>(0x100u,0x401000u+0x194881u-a);
            if(std::memcmp(reinterpret_cast<void*>(a),file.data()+text_file+(a-0x401000u),n)){r060_raw_write(a,file.data()+text_file+(a-0x401000u),n);++restored;}
        }
        std::cerr<<"restored "<<restored<<" patched .text blocks"<<char(10);
    }
    init_matrix_oracle_globals();wire_d3dx_imports(map_d3dx29(argv[7]));
    {   // D3DXVec4Transform (IAT 596284) -> generic 44067A.
        auto* page=reinterpret_cast<void*>(0x596000u);
        if(mprotect(page,4096,PROT_READ|PROT_WRITE))throw std::runtime_error("IAT vec4");
        *reinterpret_cast<std::uint32_t*>(0x596284u)=D3DX_BASE+(0x44067au-0x400000u);
        if(mprotect(page,4096,PROT_READ))throw std::runtime_error("IAT vec4");
    }
    fake_d3d9::install();
    {   // DrawPrimitiveUP (slot 83) and SetTransform (slot 44) also append
        // their vertex bytes / matrix to the data log.
        using namespace fake_d3d9;
        auto put=[&](unsigned slot,const Emit& e){const unsigned at=Stubs+0x10000u+slot*0x100u;write_stub(at,e);*reinterpret_cast<std::uint32_t*>(DeviceVt+slot*4)=at;};
        {Emit e;prologue(e,83,5);
         e.u8({0x8b,0x44,0x24,0x18,0x8b,0x4c,0x24,0x1c,0x83,0xf8,0x04,0x75,0x05,0x8d,0x0c,0x49,0xeb,0x08,0x83,0xf8,0x05,0x75,0x03,0x83,0xc1,0x02,
               0x0f,0xaf,0x4c,0x24,0x24,0x8b,0x74,0x24,0x20,0x8b,0x3d});e.u32(DataPtr);e.u8({0xf3,0xa4,0x89,0x3d});e.u32(DataPtr);e.u8({0x31,0xc0});
         epilogue(e,5);put(83,e);}
        {Emit e;prologue(e,44,3);
         e.u8({0x8b,0x74,0x24,0x1c,0xb9,0x10,0,0,0,0x8b,0x3d});e.u32(DataPtr);e.u8({0xf3,0xa5,0x89,0x3d});e.u32(DataPtr);e.u8({0x31,0xc0});
         epilogue(e,3);put(44,e);}
    }
    constexpr unsigned base=0x3a800000u,strace=base+0x10000u,values=base+0x30000u,tls=base+0x3f000u,ptd=base+0x3e000u;
    constexpr unsigned FIX=0x3a900000u,FIXSIZE=0x40000u;
    constexpr unsigned SvcStride=0x40u,SvcCap=0x3f0u;
    map_at(base,0x80000,PROT_READ|PROT_WRITE|PROT_EXEC);map_at(FIX,FIXSIZE,PROT_READ|PROT_WRITE);
    auto rw=[](std::uint32_t lo,std::uint32_t hi){for(std::uint32_t p=lo&~0xfffu;p<hi;p+=0x1000u)if(mprotect(reinterpret_cast<void*>(p),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("particles pages");};
    rw(0x638000u,0x639000u);rw(0x64f000u,0x651000u);rw(0x74e000u,0x751000u);rw(0x780000u,0x781000u);rw(0x799000u,0x79a000u);
    rw(0x79f000u,0x7a0000u);rw(0x7c2000u,0x7c7000u);rw(0x7d2000u,0x7d4000u);rw(0x7f9000u,0x7fa000u);rw(0x800000u,0x801000u);
    rw(0x819000u,0x82f000u);rw(0x899000u,0x89c000u);rw(0x8a8000u,0x956000u);rw(0x956000u,0x957000u);rw(0x95a000u,0x962000u);rw(0x98a000u,0x98b000u);rw(0x95d000u,0x95e000u);rw(0x89e000u,0x89f000u);rw(0x740000u,0x741000u);
    {   // CRT thread data 585BEC -> ptd (holdrand at +0x14)
        std::uint8_t code[]{0xb8,0,0,0,0,0xc3};std::memcpy(code+1,&ptd,4);r060_raw_write(0x585bec,code,sizeof(code));
    }
    fake_d3d9::remap_fs0(0x4862c0u,0x486450u,tls);
    auto emit32=[](std::vector<std::uint8_t>& b,std::uint32_t v){for(unsigned k=0;k<4;++k)b.push_back(std::uint8_t(v>>(k*8)));};
    unsigned stub_off=0;
    auto place=[&](const std::vector<std::uint8_t>& code){
        if(stub_off+code.size()>0x10000)throw std::runtime_error("particles stubs");
        const unsigned at=base+stub_off;std::memcpy(reinterpret_cast<void*>(at),code.data(),code.size());
        stub_off=(stub_off+unsigned(code.size())+15u)&~15u;return at;};
    // Logger: [count]++ record {pc, eax, ecx, args[6]} at strace+16+k*64, EAX = [values+slot*4].
    // extra: code inserted before the return (EDX saved), arg k at [esp+8+4k].
    struct Svc{std::uint32_t pc;unsigned argc;unsigned ret;};
    const std::vector<Svc> services{{0x428320u,3,0},{0x424940u,1,0},{0x43e3b0u,3,0},{PcParticleGetLevelDesc,3,0xc},
        {0x408880u,0,0},{0x411060u,0,0},{0x409df0u,1,0}};
    std::map<std::uint32_t,unsigned> slot_of;unsigned level_desc_stub=0;
    constexpr unsigned ROAD=FIX+0x1b800u,TEXOBJ=FIX+0x1b400u,TEXVT=FIX+0x1b500u,DESC_W=FIX+0x1b600u;
    for(std::size_t k=0;k<services.size();++k){
        const auto& e=services[k];slot_of[e.pc]=unsigned(k);
        std::vector<std::uint8_t> c{0x52,0x8b,0x15};emit32(c,strace);
        c.insert(c.end(),{0x6b,0xd2,0x40,0xc7,0x82});emit32(c,strace+16);emit32(c,e.pc);
        c.insert(c.end(),{0x89,0x82});emit32(c,strace+20);c.insert(c.end(),{0x89,0x8a});emit32(c,strace+24);
        for(unsigned i=0;i<6;++i){
            if(i<e.argc)c.insert(c.end(),{0x8b,0x4c,0x24,std::uint8_t(8+4*i)});else c.insert(c.end(),{0x31,0xc9});
            c.insert(c.end(),{0x89,0x8a});emit32(c,strace+28+4*i);}
        c.insert(c.end(),{0xff,0x05});emit32(c,strace);
        if(e.pc==0x43e3b0u){   // copy 0x19 dwords ROAD -> [arg0]
            c.insert(c.end(),{0x56,0x57,0x51,0x8b,0x7c,0x24,0x14,0xbe});emit32(c,ROAD);
            c.insert(c.end(),{0xb9,0x19,0,0,0,0xf3,0xa5,0x59,0x5f,0x5e});
        }
        if(e.pc==PcParticleGetLevelDesc){   // [arg2+0x18..0x20) = DESC_W[0..8)
            c.insert(c.end(),{0x8b,0x54,0x24,0x10,0x8b,0x0d});emit32(c,DESC_W);c.insert(c.end(),{0x89,0x4a,0x18,0x8b,0x0d});emit32(c,DESC_W+4);
            c.insert(c.end(),{0x89,0x4a,0x1c});
        }
        c.push_back(0xa1);emit32(c,values+4*unsigned(k));
        c.push_back(0x5a);
        if(e.ret)c.insert(c.end(),{0xc2,std::uint8_t(e.ret),0});else c.push_back(0xc3);
        const unsigned at=place(c);
        if(e.pc==PcParticleGetLevelDesc)level_desc_stub=at;
        else r046_write_rel32(e.pc,0xe9,at);
    }
    std::mt19937 rng(0x41bbf000u);
    auto rnd=[&](std::uint32_t n){return n?std::uint32_t(rng()%n):0u;};
    auto real=[&](float lo,float hi){return lo+(hi-lo)*(float(rng()&0xffffu)/65535.f);};
    auto g32=[](std::uint32_t a){return *reinterpret_cast<std::uint32_t*>(a);};
    auto p32=[](std::uint32_t a,std::uint32_t v){*reinterpret_cast<std::uint32_t*>(a)=v;};
    auto p16=[](std::uint32_t a,std::uint16_t v){*reinterpret_cast<std::uint16_t*>(a)=v;};
    auto p8=[](std::uint32_t a,std::uint8_t v){*reinterpret_cast<std::uint8_t*>(a)=v;};
    auto pf=[](std::uint32_t a,float v){*reinterpret_cast<float*>(a)=v;};
    unsigned cases{},mismatches{};const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
    std::map<std::string,unsigned> coverage;
    auto fail=[&](const std::string& what){++mismatches;if(mismatches<25)std::cerr<<what<<"\n";};
    auto compare=[&](const void* native,const void* original,std::size_t n,const std::string& what){
        ++cases;if(!std::memcmp(native,original,n))return true;
        const auto* a=static_cast<const std::uint8_t*>(native);const auto* b=static_cast<const std::uint8_t*>(original);
        for(std::size_t i=0;i<n;++i)if(a[i]!=b[i]){std::ostringstream s;s<<what<<" offset=0x"<<std::hex<<i<<" PC="<<unsigned(b[i])<<" native="<<unsigned(a[i]);fail(s.str());break;}
        return false;
    };
    {   // Native images against the pinned EXE.
        PcParticleState st;PcRaceMemory tables;particles_map_tables(tables);
        compare(st.data.data(),reinterpret_cast<void*>(0x74e8e0u),st.data.size(),"particles .data image 74E8E0");
        compare(st.tire_data.data(),reinterpret_cast<void*>(0x64fe1cu),st.tire_data.size(),"tire-mark .data image 64FE1C");
        compare(tables.at(0x622528u,0x16d8u),reinterpret_cast<void*>(0x622528u),0x16d8u,".rdata tables 622528");
    }
    // Pristine module images (restored before every case).
    std::vector<std::uint8_t> data_image(reinterpret_cast<std::uint8_t*>(0x74e8e0u),reinterpret_cast<std::uint8_t*>(0x750080u));
    std::vector<std::uint8_t> tire_image(reinterpret_cast<std::uint8_t*>(0x64fe1cu),reinterpret_cast<std::uint8_t*>(0x64fe58u));
    struct Mirror{std::uint32_t base,end;std::vector<std::uint8_t> bytes;};
    std::vector<Mirror> mirrors{{0x74e8e0u,0x750080u,{}},{0x8a8ce4u,0x955a30u,{}},{0x8a8c18u,0x8a8c5cu,{}},{0x95af4cu,0x95afc4u,{}},
        {0x64fe1cu,0x64fe58u,{}},{0x819638u,0x82e7c0u,{}},{FIX,FIX+FIXSIZE,{}},{0x95e000u,0x962000u,{}},{0x956500u,0x956600u,{}},
        {0x98abccu,0x98abd0u,{}},{0x82e7c0u,0x82e7dcu,{}},{0x82ea38u,0x82ea48u,{}},{0x82ee60u,0x82ee64u,{}},{0x95af0cu,0x95af10u,{}},
        {0x78024cu,0x780270u,{}},{0x780140u,0x780148u,{}},{0x780228u,0x780230u,{}},{0x799b38u,0x79a300u,{}},{0x79f574u,0x79f578u,{}},
        {0x79fb48u,0x79fb70u,{}},{0x7c2800u+0x57u*0x48u,0x7c2800u+0x58u*0x48u,{}},{0x7d2df4u,0x7d2df8u,{}},{0x7d3188u,0x7d318cu,{}},
        {0x7d33bcu,0x7d33c8u,{}},{0x7f94c0u,0x7f94c4u,{}},{0x800abcu,0x800ac0u,{}},{0x800ad0u,0x800ad4u,{}},{0x899b98u,0x89a4f8u,{}},
        {0x638e9cu,0x638ea0u,{}},{0x650500u,0x650500u+24u*0x44u,{}},{0x95d860u,0x95daa0u,{}},{0x89ede0u,0x89edecu,{}},{0x740c8cu,0x740c94u,{}},{0x79fcd4u,0x79fcd8u,{}},{0x7c2800u+0xd0u*0x48u,0x7c2800u+0xd1u*0x48u,{}}};
    // ---- fixture -----------------------------------------------------------
    constexpr unsigned PARAMS=FIX+0x18000u,CAM=FIX+0x1a000u,TIRE=FIX+0x1a400u,MODEL=FIX+0x1a800u,CHAIN=FIX+0x1a900u,
        HDR=FIX+0x1b000u,LIST=FIX+0x1b100u,ARRAY=FIX+0x1b200u,TABLE=FIX+0x1c000u,LENGTHS=FIX+0x1d000u;
    auto fill_matrix=[&](std::uint32_t a){for(unsigned k=0;k<16;++k)pf(a+k*4,k%5==0?real(0.6f,1.2f):(k==3||k==7||k==11?0.f:(k>=12?(k==15?1.f:real(-300,300)):real(-0.6f,0.6f))));};
    auto build=[&](unsigned run){
        std::memcpy(reinterpret_cast<void*>(0x74e8e0u),data_image.data(),data_image.size());
        std::memcpy(reinterpret_cast<void*>(0x64fe1cu),tire_image.data(),tire_image.size());
        std::memset(reinterpret_cast<void*>(0x8a8c18u),0,0x955a30u-0x8a8c18u);
        std::memset(reinterpret_cast<void*>(0x819638u),0,0x82e7c0u-0x819638u);
        std::memset(reinterpret_cast<void*>(0x95af4cu),0,0x95afc4u-0x95af4cu);
        for(unsigned k=0;k<FIXSIZE;k+=4)p32(FIX+k,rnd(4)?0u:rng());
        // cars: 0 = player, 1..23
        for(unsigned id=0;id<24;++id){
            const unsigned C=FIX+id*0x1000u;p32(0x799d18u+id*0x3cu,C);
            for(unsigned k=0;k<0xe00;k+=4)pf(C+k,real(-3,3));
            p32(C+4,(rng()&~0x70000000u)|(rnd(6)<<28));p8(C+0x11,std::uint8_t(rnd(4)));
            for(unsigned k=0;k<3;++k){pf(C+0x14+k*4,real(-400,400));pf(C+0x20+k*4,real(-2,2));pf(C+0xd0+k*4,real(-2,2));}
            fill_matrix(C+0xb0);fill_matrix(C+0xf0);
            for(unsigned k=0;k<12;++k)pf(C+0x130+k*4,real(-2,2));
            p32(C+0x58,fbits(real(0,1)));p32(C+0x5c,rnd(3)?0u:1u);p16(C+0x64,std::uint16_t(rnd(2)?0x80u+2u*rnd(0x40):rnd(0x300)));p32(C+0x68,rnd(3)?rnd(3):rnd(0x100));
            p16(C+0x162,std::uint16_t(rng()));p16(C+0x18c,std::uint16_t(rnd(2)?rnd(0x40):rnd(0x300)));p32(C+0x1c0,rnd(4)?rnd(0x40):0xffffffffu);
            pf(C+0x1c4,real(0,80));for(unsigned k=0;k<4;++k)p32(C+0x24c+k*4,rnd(3)?0u:rng());
            p8(C+0x296,std::uint8_t(rnd(3)));p32(C+0x2a8,rnd(3)?0u:rng());p32(C+0x2b4,PARAMS);pf(C+0x2ec,real(-7,7));
            p32(C+0x2f0,rng()&0xffu);p32(C+0x324,rnd(8));
            for(unsigned k=0;k<0x80;++k)p32(C+0x514+k*4,rnd(4)?0u:1u);
            p32(C+0xd18,rnd(3));p16(C+0xd34,std::uint16_t(rng()));p32(C+0xd9c,rnd(3)?0u:rnd(40));
            // 4866C0 model chain for byte +11
        }
        for(unsigned m=0;m<4;++m){p32(0x650500u+m*0x44u,CHAIN+m*0x10u);p32(CHAIN+m*0x10u,CHAIN+0x80u+m*0x10u);p32(CHAIN+0x80u+m*0x10u,MODEL);}
        for(unsigned k=0;k<0x100;k+=4)pf(MODEL+k,real(-2,2));
        pf(PARAMS+0x1644,real(0,120));pf(PARAMS+0xbe0,real(0.1f,0.4f));pf(PARAMS+0xc2c,real(0.1f,0.4f));
        for(unsigned k=0;k<0x400;k+=4)pf(CAM+k,real(-300,300));
        pf(CAM+0xb0,real(200,600));pf(CAM+0xb4,real(200,600));p8(CAM+0x34a,std::uint8_t(rnd(3)));fill_matrix(CAM+0x140);
        p32(0x79f574u,CAM);
        for(unsigned i=0;i<4;++i){const unsigned T=TIRE+i*0x100u;p32(0x82ea38u+i*4,rnd(5)?T:0u);for(unsigned k=0;k<0x100;k+=4)pf(T+k,real(-3,3));p16(T+0xee,std::uint16_t(rng()));}
        p32(0x82ee60u,rng());
        for(unsigned id=0;id<0x28;++id)p8(0x79fb48u+id,std::uint8_t(rnd(3)?2u:rnd(0x20)));
        p32(0x78026cu,run%7==0?0x18u:(run%7==1?0x13u:(run%11==2?0x10u:0x10u+rnd(2))));
        p32(0x780258u,rnd(4));p16(0x78024cu,std::uint16_t(rnd(4)));
        // camera override 82E7C0.. (4872E0/487300/487480), network, light table
        p32(0x82e7c0u,rnd(3));p32(0x82e7c8u,rnd(4));p32(0x82e7d8u,rnd(3)?0u:1u);
        p32(0x7f94c0u,rnd(3)?0u:1u);p32(0x800abcu,rnd(2));pf(0x800ad0u,real(-1,1));
        for(std::uint32_t a=0x899b98u;a<0x89a4f8u;a+=4)pf(a,real(0,1.3f));
        p32(0x638e9cu,rnd(12));
        // resource 0x57: header count, list -> array of handles (+0x50 the texture object)
        {const unsigned R=0x7c2800u+0x57u*0x48u;p32(R,HDR);p32(HDR+8,0x15u+rnd(0x20));p32(R+0x24,LIST);p32(LIST,ARRAY);
         for(unsigned k=0;k<0x40;++k)p32(ARRAY+k*4,0x60000000u+k);p32(ARRAY+0x50,TEXOBJ);p32(TEXOBJ,TEXVT);p32(TEXVT+0x44,level_desc_stub);
         p32(DESC_W,64+rnd(64));p32(DESC_W+4,64+rnd(64));}
        for(unsigned k=0;k<0x64;k+=4)pf(ROAD+k,real(-200,200));
        // course records for 44C8D0 / 44DC50, lengths for 43D470
        {const unsigned n=1+rnd(4);p32(0x7d33bcu,TABLE);p32(0x7d33c4u,n);
         for(unsigned i=0;i<n;++i){p32(TABLE+i*0x78u+4,rnd(3));p32(TABLE+i*0x78u+0x14,TABLE+0x800u+i*4u);p32(TABLE+0x800u+i*4u,rnd(3)?7u:rnd(0x42));}
         p32(0x7d2df4u,TABLE+0x900u);p32(TABLE+0x900u,rnd(0x42));p32(0x7d3188u,rnd(2)?TABLE+0xa00u:0u);p32(TABLE+0xa04u,rnd(3));
         for(unsigned k=0;k<2;++k){p32(0x780140u+k*4,rnd(4)?LENGTHS+k*0x100u:0u);p32(LENGTHS+k*0x100u+0xc,1+rnd(8));p32(0x780228u+k*4,LENGTHS+0x400u+k*0x100u);
            for(unsigned j=0;j<16;++j)p16(LENGTHS+0x400u+k*0x100u+j*2,std::uint16_t(0x40+rnd(0x300)));}}
        p32(0x98abccu,rnd(2));p32(0x95af0cu,rng());
        // pool of the 427xxx module
        for(std::uint32_t a=0x95e000u;a<0x962000u;a+=4)p32(a,rnd(3)?0u:rnd(3));
        for(std::uint32_t a=0x956500u;a<0x956600u;a+=4)p32(a,rnd(4));
        p32(ptd+0x14,rng());
    };
    // Particles injected into every source after the PC init (so that all
    // particle functions run in the control frames).
    struct Fn{std::uint32_t source;std::uint32_t pc;};
    const Fn ctrl_fns[]={{4,0x41d330u},{4,0x41d5c0u},{3,0x41dcc0u},{1,0x41e230u},{2,0x41e820u},{5,0x41ed80u},{6,0x41f2f0u},{7,0x41f9f0u},
                         {8,0x420180u},{9,0x4203e0u},{10,0x420820u}};
    auto inject_particles=[&]{
        for(unsigned s=1;s<11;++s){const unsigned src=0x8a8d18u+s*0x84u;
            if(!(s==8||s==9||s==10))p32(src+0x5c,0x934448u+rnd(4)*0x5cu);}
        for(const auto& f:ctrl_fns){
            const unsigned src=0x8a8d18u+f.source*0x84u;const unsigned buf=g32(src+8),n=g32(src+4),stride=g32(src+0x68);
            if(!buf||!n||!stride)continue;
            for(unsigned k=0;k<3;++k){
                const unsigned a=buf+rnd(std::min(n,64u))*stride;
                p32(a,0x80000000u);p32(a+0x1c,f.pc);
                {const unsigned car0=g32(0x799d18u);
                 for(unsigned w=1;w<7;++w)pf(a+w*4,w<4?(rnd(2)?real(-300,300):*reinterpret_cast<float*>(car0+0x10+w*4)+real(-4,4)):real(-0.5f,0.5f));}
                if(f.source>=8){p32(a+0x20,1+rnd(60));p16(a+0x24,std::uint16_t(1+rnd(6)));p16(a+0x26,std::uint16_t(rnd(8)));continue;}
                pf(a+0x20,real(0.1f,2));for(unsigned w=0;w<4;++w)p32(a+0x24+w*4,rng());
                p32(a+0x44,1+rnd(40));p32(a+0x48,1+rnd(40));p32(a+0x4c,rnd(12));
                if(f.pc==0x41d330u){p32(a+0x50,rnd(256));}
                if(f.pc==0x41f2f0u)pf(a+0x4c,real(0,100));
            }
        }
        // tire records 934448.. (player) and 9345B8.. (cars)
        for(std::uint32_t r=0x934448u;r<0x9345b8u+23u*0x170u;r+=4)pf(r,real(-2,2));
        for(std::uint32_t r=0x934448u;r<0x9345b8u+23u*0x170u;r+=0x5c){
            pf(r,real(0,0.5f));p32(r+0x40,rnd(3)?rng():0u);p32(r+0x54,rnd(6));p32(r+0x58,0x74fee4u+rnd(6)*0x10u);
            const float nx=real(-0.3f,0.3f),nz=real(-0.3f,0.3f);pf(r+0x10,nx);pf(r+0x14,1.f);pf(r+0x18,nz);
            if(rnd(2)){const unsigned car0=g32(0x799d18u);for(unsigned w=0;w<3;++w){const float c=*reinterpret_cast<float*>(car0+0x14+w*4);
                pf(r+4+w*4,c+real(-2,2));pf(r+0x1c+w*4,c+real(-2,2));}}}
    };
    const unsigned total_runs=count;
    for(unsigned run=0;run<total_runs;++run){
        build(run);
        const unsigned entry=run%6u;   // 0 init, 1 control frames, 2 control frames with injected particles, 3 display, 4 4208A0 smoke + control frames, 5 420560 + control frames
        std::vector<std::uint32_t> value(services.size());
        for(auto& v:value)v=rnd(3)?rnd(64):rng();
        value[slot_of[0x428320u]]=rnd(3)?rnd(0x40):0xffffffffu;   // pool handle (or failure)
        for(std::size_t k=0;k<services.size();++k)p32(values+4*unsigned(k),value[k]);
        if(entry>=1){prepare(0x41bbf0);run_original32();if(entry>=2)inject_particles();}
        if(entry==3){
            p32(strace,0);prepare(0x41bc60);run_original32();       // one PC control frame: live particles
            for(unsigned k=0;k<16;++k){pf(0x95d860u+k*4,k%5==0?real(0.5f,1.5f):real(-0.3f,0.3f));pf(0x95d8a0u+k*4,k%5==0?real(0.5f,1.5f):real(-0.3f,0.3f));}
            pf(0x95d8a0u+0x2c,-1.f);pf(0x95d8a0u+0x38,real(-2,-0.5f));pf(0x95d8a0u+0x3c,0.f);
            p32(0x74fe7cu,rnd(8)?1u:0u);p32(0x95af9cu,rng());
            fake_d3d9::reset_log();
            for(unsigned k=0;k<0x400;++k)p32(fake_d3d9::RenderStates+k*4,rnd(3)?0u:rnd(3));
            for(unsigned k=0;k<0x200;++k){p32(fake_d3d9::StageStates+k*4,rnd(3)?0u:rnd(5));p32(fake_d3d9::SamplerStates+k*4,rnd(3)?0u:rnd(4));}
            for(unsigned k=0;k<256;++k)p32(fake_d3d9::Textures+k*4,0);
        }
        if(entry==5){   // 420560: event 0x18D running (mostly), mode 24 (mostly), the camera's +1C0, the 0xD0 entry
            constexpr unsigned D0SYS=FIX+0x1e000u,D0TT=FIX+0x1e100u,D0SLOTS=FIX+0x1e200u;
            *reinterpret_cast<std::uint8_t*>(0x79fcd5u)=std::uint8_t(rnd(5)?2u:rnd(4));
            p32(0x78026cu,rnd(4)?0x18u:rnd(40));
            const unsigned cam=g32(0x79f574u);for(unsigned k=0;k<16;++k)pf(cam+0x1c0u+k*4,k%5==0?real(0.6f,1.2f):(k>=12&&k<15?real(-200,200):real(-0.6f,0.6f)));pf(cam+0x1fcu,1.f);
            const unsigned e=0x7c2800u+0xd0u*0x48u;p32(e,D0SYS);p32(e+0x24u,D0TT);p32(D0SYS+8u,rnd(4)?0x20u:rnd(0x10));p32(D0TT,D0SLOTS);
            for(unsigned k=0;k<0x20u;++k)p32(D0SLOTS+k*4,rng());
            if(rnd(2))for(unsigned a=0x9406d8u;a<=0x9406ecu;a+=4)p32(a,rnd(3)?rng():0u);else p32(0x9406d8u,0);
        }
        for(auto& m:mirrors){m.bytes.resize(m.end-m.base);std::memcpy(m.bytes.data(),reinterpret_cast<void*>(m.base),m.bytes.size());}
        std::uint32_t native_rand=g32(ptd+0x14);
        reset_matrix_oracle_globals();
        std::vector<std::uint8_t> arena(0x800);Bytes ar(arena.data(),arena.size());
        for(unsigned k=0;k<16;++k)ar.putf(k*4,k%5==0?1.f:real(-0.5f,0.5f));
        std::memcpy(reinterpret_cast<void*>(MatrixArena),arena.data(),arena.size());
        PcRaceMemory mem;
        std::vector<std::vector<std::uint8_t>> native(mirrors.size());
        for(std::size_t i=0;i<mirrors.size();++i){native[i]=mirrors[i].bytes;mem.map(mirrors[i].base,native[i].data(),native[i].size());}
        particles_map_tables(mem);
        PcMatrixStack stack{ar,0,0,64};
        struct NCall{std::uint32_t pc;decltype(PcRaceCall::args) a;unsigned argc;};
        std::vector<NCall> calls;
        PcRaceService service=[&](const PcRaceCall& k)->std::uint32_t{
            if(k.pc==PcParticleRenderGlobal){mem.put32(k.args[0],k.args[1]);return 0;}
            if(!slot_of.count(k.pc)){std::ostringstream e;e<<"native called an unstubbed service "<<std::hex<<k.pc;throw std::runtime_error(e.str());}
            calls.push_back({k.pc,k.args,k.argc});
            if(k.pc==0x43e3b0u)for(unsigned w=0;w<0x64;w+=4)mem.put32(k.args[0]+w,g32(ROAD+w));
            if(k.pc==PcParticleGetLevelDesc){mem.put32(k.args[2]+0x18,g32(DESC_W));mem.put32(k.args[2]+0x1c,g32(DESC_W+4));}
            if(k.pc==0x411060u){const auto cur=stack.current();for(unsigned w=0;w<64;w+=4)mem.put32(0x95d9e0u+w,cur.u32(w));}
            return value[slot_of.at(k.pc)];
        };
        struct LogDevice:PcParticleDevice{
            std::vector<std::array<std::uint32_t,12>> log;std::vector<std::uint8_t> data;
            std::vector<std::uint32_t> rs=std::vector<std::uint32_t>(0x400),stage=std::vector<std::uint32_t>(0x200),sampler=std::vector<std::uint32_t>(0x200),tex=std::vector<std::uint32_t>(256);
            void rec(std::uint32_t id,std::initializer_list<std::uint32_t> a){std::array<std::uint32_t,12> r{};r[0]=id;r[1]=std::uint32_t(a.size());unsigned k=2;for(auto v:a)r[k++]=v;log.push_back(r);}
            static unsigned si(std::uint32_t s,std::uint32_t t){return ((s&0xfu)<<5)+(t&0x1fu);}
            std::uint32_t get_render_state(std::uint32_t s)override{rec(58,{s,0});return rs[s&0x3ffu];}
            void set_render_state(std::uint32_t s,std::uint32_t v)override{rec(57,{s,v});rs[s&0x3ffu]=v;}
            std::uint32_t get_texture(std::uint32_t s)override{rec(64,{s,0});return tex[s&0xffu];}
            void set_texture(std::uint32_t s,std::uint32_t t)override{rec(65,{s,t});tex[s&0xffu]=t;}
            std::uint32_t get_texture_stage_state(std::uint32_t s,std::uint32_t t)override{rec(66,{s,t,0});return stage[si(s,t)];}
            void set_texture_stage_state(std::uint32_t s,std::uint32_t t,std::uint32_t v)override{rec(67,{s,t,v});stage[si(s,t)]=v;}
            std::uint32_t get_sampler_state(std::uint32_t s,std::uint32_t t)override{rec(68,{s,t,0});return sampler[si(s,t)];}
            void set_sampler_state(std::uint32_t s,std::uint32_t t,std::uint32_t v)override{rec(69,{s,t,v});sampler[si(s,t)]=v;}
            void set_transform(std::uint32_t s,const float* m)override{rec(44,{s,0});const auto* b=reinterpret_cast<const std::uint8_t*>(m);data.insert(data.end(),b,b+64);}
            void draw_primitive_up(std::uint32_t type,std::uint32_t count,const std::uint8_t* d,std::uint32_t stride)override{
                rec(83,{type,count,0,stride});const std::uint32_t v=type==4u?count*3u:(type==5u?count+2u:count);data.insert(data.end(),d,d+std::size_t(v)*stride);}
            void set_fvf(std::uint32_t f)override{rec(89,{f});}
            void set_vertex_shader(std::uint32_t v)override{rec(92,{v});}
            void set_pixel_shader(std::uint32_t v)override{rec(107,{v});}
            void release(std::uint32_t o)override{rec(0x4002,{o});}
        } device;
        if(entry==3){
            for(unsigned k=0;k<0x400;++k)device.rs[k]=g32(fake_d3d9::RenderStates+k*4);
            for(unsigned k=0;k<0x200;++k){device.stage[k]=g32(fake_d3d9::StageStates+k*4);device.sampler[k]=g32(fake_d3d9::SamplerStates+k*4);}
            for(unsigned k=0;k<256;++k)device.tex[k]=g32(fake_d3d9::Textures+k*4);
        }
        PcRaceContext ctx{mem,stack,service,nullptr};
        PcParticleContext pctx{ctx,native_rand,&device};
        const unsigned frames=(entry==0||entry==3)?1u:1u+rnd(3);
        p32(strace,0);
        std::string tag="particles entry "+std::to_string(entry)+" run "+std::to_string(run);
        bool threw=false;
        for(unsigned f=0;f<frames&&!threw;++f){
            const unsigned smoke_n=1u+rnd(12),smoke_colour=rng();constexpr unsigned SmokePos=FIX+0x1f000u;
            if(entry==5&&f==0){prepare(0x420560);run_original32();if(guest_call.out_sp!=S){fail(tag+" 420560 guest stack imbalance");threw=true;break;}}
            if(entry==4&&f==0){   // the AUTOSCENE smoke emitter, then the control frame runs its particles
                for(unsigned w=0;w<3;++w){pf(SmokePos+w*4,real(-50,50));Bytes(native[6].data()+(SmokePos-FIX)+w*4,4).put32(0,g32(SmokePos+w*4));}
                prepare(0x4208a0);auto* st=reinterpret_cast<unsigned*>(S);st[0]=smoke_n;st[1]=SmokePos;st[2]=smoke_colour;run_original32();
                if(guest_call.out_sp!=S){fail(tag+" 4208A0 guest stack imbalance");threw=true;break;}
            }
            if(entry==0)prepare(0x41bbf0);else if(entry==3)prepare(0x41bd10);else prepare(0x41bc60);
            run_original32();
            if(guest_call.out_sp!=S){fail(tag+" guest stack imbalance");threw=true;break;}
            try{
                if(entry==4&&f==0)particles_smoke_emit_4208a0(pctx,smoke_n,g32(SmokePos),g32(SmokePos+4),g32(SmokePos+8),smoke_colour);
                if(entry==5&&f==0)particles_flare_420560(pctx);
                if(entry==0)particles_init_41bbf0(pctx);else if(entry==3)particles_display_41bd10(pctx);else particles_control_41bc60(pctx);}
            catch(const std::exception& ex){++cases;fail(tag+" frame "+std::to_string(f)+" native threw: "+ex.what());threw=true;}
            if(f+1<frames){p32(0x95af0cu,g32(0x95af0cu)+1u);for(std::size_t i=0;i<mirrors.size();++i)if(mirrors[i].base==0x95af0cu)Bytes(native[i].data(),4).put32(0,g32(0x95af0cu));}
        }
        if(threw)continue;
        ++coverage["entry_"+std::to_string(entry)];coverage["frames"]+=frames;
        for(unsigned sidx=1;sidx<11;++sidx){const unsigned src=0x8a8d18u+sidx*0x84u;const unsigned buf=g32(src+8),n=g32(src+4),stride=g32(src+0x68);
            if(!buf||!stride||n>0x1000)continue;unsigned alive=0;for(unsigned k=0;k<n;++k)if(g32(buf+k*stride+0x1c))++alive;coverage["alive_src"+std::to_string(sidx)]+=alive;}
        if(inject&&run==total_runs-1){native[1][0x100]^=1;if(!device.data.empty())device.data[device.data.size()/2]^=1;}
        for(std::size_t i=0;i<mirrors.size();++i){
            std::ostringstream s;s<<tag<<" region "<<std::hex<<mirrors[i].base;
            compare(native[i].data(),reinterpret_cast<void*>(mirrors[i].base),native[i].size(),s.str());
        }
        {++cases;if(native_rand!=g32(ptd+0x14))fail(tag+" rand state");}
        if(entry==3){
            const unsigned nl=fake_d3d9::log_count();++cases;coverage["device_calls"]+=nl;
            if(nl!=device.log.size()){std::ostringstream e;e<<tag<<" device call count PC="<<nl<<" native="<<device.log.size();
                for(unsigned i=0;i<std::min<std::size_t>(nl,device.log.size());++i){const auto* r=fake_d3d9::log_record(i);if(r[0]!=device.log[i][0]){e<<" first differing call "<<i<<" PC id "<<r[0]<<" native "<<device.log[i][0];break;}}
                fail(e.str());}
            else for(unsigned i=0;i<nl;++i){
                const auto* r=fake_d3d9::log_record(i);const auto& n=device.log[i];
                // Pointer arguments (Get* outputs, the matrix and vertex pointers) are compared through the data log.
                const unsigned skip=(r[0]==58||r[0]==64||r[0]==44)?1u:((r[0]==66||r[0]==68||r[0]==83)?2u:99u);
                bool ok=r[0]==n[0];
                for(unsigned a=0;ok&&a<n[1]&&a<10&&n[0]!=0x4002u;++a)if(a!=skip&&r[2+a]!=n[2+a])ok=false;
                if(!ok){std::ostringstream e;e<<tag<<" device call "<<i<<" PC "<<std::hex<<r[0]<<"("<<r[2]<<","<<r[3]<<","<<r[4]<<","<<r[5]<<") native "<<n[0]<<"("<<n[2]<<","<<n[3]<<","<<n[4]<<","<<n[5]<<")";fail(e.str());break;}
            }
            const unsigned words=fake_d3d9::data_log_words();++cases;
            if(words*4u!=device.data.size()){std::ostringstream e;e<<tag<<" device data PC="<<words*4<<" native="<<device.data.size();fail(e.str());}
            else compare(device.data.data(),fake_d3d9::data_log(),device.data.size(),tag+" device data");
            coverage["display_bytes"]+=words*4u;
        }
        compare(arena.data(),reinterpret_cast<void*>(MatrixArena),arena.size(),tag+" matrix arena");
        {std::array<std::uint32_t,2> sp{MatrixArena+std::uint32_t(stack.current_offset),std::uint32_t(stack.depth)};
         compare(sp.data(),reinterpret_cast<void*>(0x89b564),8,tag+" matrix pointer/depth");}
        const unsigned n=g32(strace);++cases;coverage["service_calls"]+=n;
        if(n>SvcCap)throw std::runtime_error("service trace overflow");
        if(n!=calls.size()){std::ostringstream s;s<<tag<<" service count PC="<<n<<" native="<<calls.size();fail(s.str());}
        else for(unsigned i=0;i<n;++i){
            const unsigned at=strace+16+i*SvcStride;const auto& k=calls[i];
            bool ok=g32(at)==k.pc;
            for(unsigned a=0;ok&&a<k.argc;++a){
                const std::uint32_t ga=g32(at+12+4*a);
                const bool local=(k.pc==0x43e3b0u&&a<2)||(k.pc==PcParticleGetLevelDesc&&a==2);
                if(local)ok=(ga>=S-0x1000&&ga<S+0x40);else ok=ga==k.a[a];
            }
            if(!ok){std::ostringstream s;s<<tag<<" service "<<i<<" PC "<<std::hex<<g32(at)<<"("<<g32(at+12)<<","<<g32(at+16)<<","<<g32(at+20)<<") native "<<k.pc<<"("<<k.a[0]<<","<<k.a[1]<<","<<k.a[2]<<")";fail(s.str());break;}
            coverage["svc_"+std::to_string(slot_of[k.pc])]++;
        }
    }
    std::cerr<<"coverage:";for(const auto& [k,v]:coverage)std::cerr<<' '<<k<<'='<<v;std::cerr<<"\n";
    std::cout<<"{\"routine\":\""<<only<<"\",\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}"<<'\n';return mismatches?1:0;
}

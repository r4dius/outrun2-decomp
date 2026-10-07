// Included inside the compare_pc dispatcher; uses its private x86 harness.
if(only=="race_area_probe"||only=="race_sky_probe"){
    // Original race AREA (event 390: 44CB00 init, 44F7C0 control, 44F120
    // display, 44B7F0 destroy and their parts) and SKY (event 391: 451A30,
    // 451B40, 4521C0, 452080, 451DD0, 451E90) against race_area/race_sky.
    // The module code runs unmodified with its protected bridges live; every
    // callee outside the module is a capture stub recording (entry, ECX, stack
    // arguments) and returning a scripted per-case value that the native
    // service returns too. Render leaves are capture stubs recording their
    // arguments and the current matrix (the draw list). The matrix leaves run
    // the original code over D3DX. The Windows FS:0 SEH accesses of 44CF00 are
    // pointed at a private TLS dword. Every mapped region (area block, module
    // .data, globals, fixtures), the matrix arena/stack, the ordered service
    // calls, the draw list and the return values are compared.
    using namespace outrun::platform;
    if(argc<=7||std::string(argv[7])=="-")throw std::runtime_error("race area oracle requires pinned d3dx9_29.dll");
    constexpr unsigned base=0x3a800000u,strace=base+0x10000u,dtrace=base+0x20000u,values=base+0x30000u,tls=base+0x3f000u;
    constexpr unsigned fix=base+0x40000u,fix_size=0x30000u;
    constexpr unsigned stubs=base;            // 0x10000 of stub code
    constexpr unsigned SvcStride=0x40u,DrawStride=0x60u,SvcCap=0x3f0u,DrawCap=0x2a0u;
    map_at(base,0x80000,PROT_READ|PROT_WRITE|PROT_EXEC);map_original_arithmetic_bridge(file);
    {   // The dispatcher's start-up patches (r046 .. r062 call redirections)
        // are removed: the whole .text is restored to the pinned file bytes
        // before this probe installs its own stubs.
        unsigned restored=0;
        const std::size_t text_file=pe_file_offset(file,0x401000u);
        for(std::uint32_t a=0x401000u;a<0x401000u+0x194881u;a+=0x100u){
            const std::size_t n=std::min<std::size_t>(0x100u,0x401000u+0x194881u-a);
            if(std::memcmp(reinterpret_cast<void*>(a),file.data()+text_file+(a-0x401000u),n)){r060_raw_write(a,file.data()+text_file+(a-0x401000u),n);++restored;}
        }
        std::cerr<<"restored "<<restored<<" patched .text blocks"<<char(10);
    }
    init_matrix_oracle_globals();wire_d3dx_imports(map_d3dx29(argv[7]));
    {   // 40A360 (mxScale) jumps through the D3DXMatrixScaling import 5962B4:
        // wire it to the DLL's generic routine (export 44127D dispatches to 441288).
        auto* page=reinterpret_cast<void*>(0x596000u);
        if(mprotect(page,4096,PROT_READ|PROT_WRITE))throw std::runtime_error("IAT scaling");
        *reinterpret_cast<std::uint32_t*>(0x5962b4u)=D3DX_BASE+(0x441288u-0x400000u);
        if(mprotect(page,4096,PROT_READ))throw std::runtime_error("IAT scaling");
    }
    for(unsigned page:{0x635000u,0x636000u,0x638000u,0x7d2000u,0x7d3000u,0x780000u,0x799000u,0x79e000u,0x79f000u,0x7f9000u,0x7c2000u,0x7b1000u,0x95d000u,0x8a8000u,0x89b000u})
        if(mprotect(reinterpret_cast<void*>(page&~0xfffu),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("race area globals");
    auto emit32=[](std::vector<std::uint8_t>& b,std::uint32_t v){for(unsigned k=0;k<4;++k)b.push_back(std::uint8_t(v>>(k*8)));};
    // ---- service capture stubs -------------------------------------------
    struct Svc{std::uint32_t pc;unsigned argc;};
    const std::vector<Svc> services{
        {0x4103f0,2},{0x4afd90,1},{0x4af560,0},{0x4af570,0},{0x4afb40,0},{0x4afb50,0},{0x4afb70,0},{0x4afb80,0},{0x4af580,0},{0x4af590,0},
        {0x4249f0,1},{0x448820,1},{0x44fd80,2},{0x44f880,1},{0x44fc60,4},{0x455b80,1},{0x455ba0,1},{0x450630,2},{0x47fbd0,3},{0x450130,0},
        {0x4556f0,0},{0x455bd0,1},{0x46fc30,1},{0x48b310,0},{0x48b350,0},{0x451350,1},{0x46c500,0},{0x448990,1},{0x4502c0,0},
        {0x450380,1},{0x4502d0,0},{0x427700,1},{0x4af550,0},{0x43de50,1},{0x440cd0,1},{0x44a1a0,0},{0x4f11b0,1},{0x4f0600,1},{0x4ef860,0},
        {0x4ef850,0},{0x4299c0,1},{0x42dfb0,1},{0x46c4c0,0},{0x46c260,0},{0x46c2c0,1},{0x4f21c0,1},{0x4401d0,1},{0x46c240,0},{0x43dba0,2},
        {0x46fe50,2},{0x448ad0,2},{0x448980,0},{0x44aa80,3},{0x45dfb0,1},{0x45e000,1},{0x46c250,0},{0x45c470,1},{0x4f10d0,2},
        {0x46c4d0,0},{0x4f0430,2},{0x4f0d10,1},{0x42deb0,2},{0x429920,2},{0x46c380,1},{0x4f03a0,2},{0x4efb50,1},{0x4efaf0,1},{0x4efd20,2},
        {0x42df90,0},{0x4299a0,0},{0x4103a0,2},{0x440d10,1},{0x440d30,0},{0x4957f0,0},{0x495490,0},{0x45aee0,0},{0x450300,0},{0x450310,0},
        {0x46c360,1},{0x448960,1},{0x4f0910,1},{0x4f0cb0,3},{0x4278c0,1},{0x4872e0,0},{0x487320,0},{0x43d470,1},{0x40ecb0,0},{0x49b2d0,0},
        {0x44ff10,0},{0x43f960,0},{0x407c30,4},{0x407e40,6},{0x406780,2},
        // sky helpers are services of the AREA (installed only in race_area_probe)
        {0x451dd0,0},{0x451e90,0}};
    std::map<std::uint32_t,unsigned> slot_of;std::map<std::uint32_t,unsigned> argc_of;
    unsigned stub_off=0;
    auto install=[&](std::uint32_t pc,const std::vector<std::uint8_t>& code){
        if(stub_off+code.size()>0x10000)throw std::runtime_error("race area stubs");
        std::memcpy(reinterpret_cast<void*>(stubs+stub_off),code.data(),code.size());
        r046_write_rel32(pc,0xe9,stubs+stub_off);stub_off=(stub_off+unsigned(code.size())+15u)&~15u;
    };
    const bool sky=only=="race_sky_probe";
    for(std::size_t k=0;k<services.size();++k){
        const auto& e=services[k];
        if(sky&&(e.pc==0x451dd0u||e.pc==0x451e90u))continue;
        slot_of[e.pc]=unsigned(k);argc_of[e.pc]=e.argc;
        std::vector<std::uint8_t> c{0x52,0x8b,0x15};emit32(c,strace);                          // push edx; mov edx,[count]
        c.insert(c.end(),{0x6b,0xd2,0x40,0xc7,0x82});emit32(c,strace+16);emit32(c,e.pc);        // imul edx,64; mov [edx+t+16],pc
        c.insert(c.end(),{0x89,0x82});emit32(c,strace+20);c.insert(c.end(),{0x89,0x8a});emit32(c,strace+24); // eax, ecx
        for(unsigned i=0;i<6;++i){
            if(i<e.argc)c.insert(c.end(),{0x8b,0x4c,0x24,std::uint8_t(8+4*i)});else c.insert(c.end(),{0x31,0xc9});
            c.insert(c.end(),{0x89,0x8a});emit32(c,strace+28+4*i);}
        c.insert(c.end(),{0xff,0x05});emit32(c,strace);                                         // inc [count]
        c.push_back(0xa1);emit32(c,values+4*unsigned(k));                                        // mov eax,[value]
        c.insert(c.end(),{0x5a,0xc3});install(e.pc,c);
    }
    // ---- render leaf capture stubs (draw list) -----------------------------
    struct Leaf{std::uint32_t pc;unsigned argc;};
    const Leaf leaves[]={{0x405360u,6},{0x4052b0u,0},{0x4052c0u,0},{0x4044e0u,1},{0x4044f0u,5},{0x4056d0u,4},{0x404540u,0}};
    auto leaf_code=[&](std::uint32_t pc,unsigned n,unsigned argbase,unsigned ret){
        std::vector<std::uint8_t> a{0x56,0x57,0x51,0xa1};emit32(a,dtrace);                      // push esi/edi/ecx; mov eax,[count]
        a.insert(a.end(),{0x6b,0xc8,0x60,0x81,0xc1});emit32(a,dtrace+0x10);                     // imul ecx,eax,0x60; add ecx,records
        a.insert(a.end(),{0xc7,0x01});emit32(a,pc);                                             // mov [ecx],pc
        a.insert(a.end(),{0x8d,0x74,0x24,std::uint8_t(argbase),0x8d,0x79,0x04,0x51,0xb9});emit32(a,n); // lea esi,args; lea edi,[ecx+4]; push ecx; mov ecx,n
        a.insert(a.end(),{0xf3,0xa5,0x59,0x8d,0x79,0x20,0x8b,0x35,0x64,0xb5,0x89,0x00,0xb9,0x10,0,0,0,0xf3,0xa5,0x40,0xa3});
        emit32(a,dtrace);a.insert(a.end(),{0x59,0x5f,0x5e});
        if(ret)a.insert(a.end(),{0xc2,std::uint8_t(ret),0});else a.push_back(0xc3);
        return a;
    };
    for(const auto& l:leaves)install(l.pc,leaf_code(l.pc,l.argc,0x10,0));
    // Fake device 89BD60 for the sky SetRenderState(dev, state, value) (stdcall).
    const unsigned device=base+0x3e000u,vtable=base+0x3e100u,setrs=stubs+0xff00u;
    {   auto code=leaf_code(PcRaceSetRenderState,2,0x14,0xc);   // skip the this pointer
        std::memcpy(reinterpret_cast<void*>(setrs),code.data(),code.size());
        *reinterpret_cast<std::uint32_t*>(device)=vtable;*reinterpret_cast<std::uint32_t*>(vtable+0xe4)=setrs;
        *reinterpret_cast<std::uint32_t*>(0x89bd60u)=device;
    }
    // ---- 44CF00 SEH: FS:0 accesses -> private TLS dword ---------------------
    // FXT: 4D4B3A is the relocated epilogue reached from the bridge at 44D3B9; Steam: the
    // epilogue is in place at 44D3B9.
    for(auto site:{0x44cf07u,0x44cf0eu,0x44cf48u,g_steam_exe?0x44d3b9u:0x4d4b3au}){
        std::array<std::uint8_t,7> code{};const unsigned n=site==0x44cf07u?6u:7u;
        std::memcpy(code.data(),reinterpret_cast<void*>(site),n);
        if(code[0]!=0x64)throw std::runtime_error("44CF00 FS prefix changed");
        code[0]=0x90;std::memcpy(code.data()+n-4,&tls,4);r060_raw_write(site,code.data(),n);
    }
    // ---- helpers -------------------------------------------------------------
    std::mt19937 rng(sky?0x51a30000u:0x44cb0000u);
    auto rnd=[&](std::uint32_t n){return n?std::uint32_t(rng()%n):0u;};
    auto real=[&](float lo,float hi){return lo+(hi-lo)*(float(rng()&0xffffu)/65535.f);};
    auto g32=[](std::uint32_t a){return *reinterpret_cast<std::uint32_t*>(a);};
    auto p32=[](std::uint32_t a,std::uint32_t v){*reinterpret_cast<std::uint32_t*>(a)=v;};
    auto p16=[](std::uint32_t a,std::uint16_t v){*reinterpret_cast<std::uint16_t*>(a)=v;};
    auto p8=[](std::uint32_t a,std::uint8_t v){*reinterpret_cast<std::uint8_t*>(a)=v;};
    auto pf=[](std::uint32_t a,float v){*reinterpret_cast<float*>(a)=v;};
    unsigned cases{},mismatches{};const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
    std::map<std::string,unsigned> coverage;
    auto fail=[&](const std::string& what){++mismatches;if(mismatches<(std::getenv("OR2_ALL_FAILS")?100000u:20u))std::cerr<<what<<"\n";};
    auto compare=[&](const void* native,const void* original,std::size_t n,const std::string& what){
        ++cases;if(!std::memcmp(native,original,n))return;
        const auto* a=static_cast<const std::uint8_t*>(native);const auto* b=static_cast<const std::uint8_t*>(original);
        for(std::size_t i=0;i<n;++i)if(a[i]!=b[i]){std::ostringstream s;s<<what<<" offset=0x"<<std::hex<<i<<" PC="<<unsigned(b[i])<<" native="<<unsigned(a[i]);fail(s.str());break;}
    };
    // Regions mirrored natively: [base,end) copied from the guest before a case.
    struct Mirror{std::uint32_t base,end;std::vector<std::uint8_t> bytes;};
    std::vector<Mirror> mirrors{{0x7d2d80u,0x7d34c8u,{}},{0x635f2cu,0x636bc4u,{}},{0x780240u,0x780280u,{}},{0x799d18u,0x799d1cu,{}},
        {0x79ec00u,0x79fd00u,{}},{0x7f94c0u,0x7f94c4u,{}},{0x7f95a8u,0x7f95acu,{}},{0x7d3a64u,0x7d3a68u,{}},{0x638664u,0x638674u,{}},
        {0x8a8c18u,0x8a8c1cu,{}},{0x95dba0u,0x95dbe0u,{}},{fix,fix+fix_size,{}}};
    {   // Module images and EXE tables of the native state against the pinned
        // EXE as mapped (before any case writes the guest .data).
        PcRaceAreaState area_state;PcRaceSkyState sky_state;PcRaceMemory tables;race_area_map_tables(tables);
        compare(area_state.data.data(),reinterpret_cast<void*>(0x635f2cu),area_state.data.size(),"area .data image 635F2C");
        compare(sky_state.data.data(),reinterpret_cast<void*>(0x638664u),sky_state.data.size(),"sky .data image 638664");
        compare(tables.at(0x5d4dc8u,0x5d5e58u-0x5d4dc8u),reinterpret_cast<void*>(0x5d4dc8u),0x5d5e58u-0x5d4dc8u,"EXE tables 5D4DC8");
        std::vector<std::uint8_t> zero(0x748);compare(area_state.block.data(),zero.data(),zero.size(),"area block initial zero");
    }
    // ---- one case ------------------------------------------------------------
    struct Values{std::map<std::uint32_t,std::uint32_t> v;};
    const unsigned T=fix,DESC=fix+0x1000,ALT=fix+0x2000,LIST=fix+0x2800,SKYW=fix+0x2c00,GEO=fix+0x3000,CAR=fix+0x6000,OBJ=fix+0x8000,
        STR=fix+0x9000,DF4=fix+0x9100,WORK=fix+0xa000;
    auto fill_matrix=[&](std::uint32_t a){for(unsigned k=0;k<16;++k)pf(a+k*4,k==15?1.f:(k>=12?real(-200,200):real(-1.5f,1.5f)));};
    auto build=[&](unsigned run,std::map<std::uint32_t,std::uint32_t>& value){
        std::memset(reinterpret_cast<void*>(fix),0,fix_size);
        for(unsigned k=0;k<fix_size;k+=4)p32(fix+k,rng());
        // descriptors
        const std::uint32_t ids[]={0,1,2,0xb,0xf,0x1c,0x1e,5};
        for(unsigned d=0;d<8;++d){
            const unsigned D=DESC+d*0x100,A=ALT+d*0x80,L=LIST+d*0x20,W=SKYW+d*0x10;
            p32(D,run%3?ids[(d+run)%8]:rnd(0x28));p32(D+4,rnd(16));p32(D+8,L);
            for(unsigned k=0;k<7;++k)p32(L+k*4,k<rnd(6)?0x100000u+rnd(0x40000):0xffffffffu);p32(L+28,0xffffffffu);
            p32(D+0x18,A);p32(D+0x3c,rng());p32(D+0x58,rnd(3)?W:0u);p16(W,std::uint16_t(rnd(0x300)));p8(W+2,std::uint8_t(rng()));
            p16(D+0x5c,std::uint16_t(rnd(0x120)));p16(D+0x5e,std::uint16_t(0x80+rnd(0x180)));p16(D+0x7c,std::uint16_t(rnd(0x400)));
            for(unsigned k=0;k<3;++k)pf(D+0x80+k*4,real(-900,900));
            for(unsigned k=0;k<3;++k)pf(D+0x8c+k*4,real(-3,3));
            for(unsigned k=0;k<4;++k)p32(D+0x48+k*4,rnd(4)?0x200000u+rnd(0x100):0xffffffffu);
            p32(A+4,rnd(0x400));p32(D+0x60,rnd(16));                       // bank id (448810, 44CD30)
        }
        // course table
        const unsigned n=1+rnd(6);
        for(unsigned i=0;i<n;++i){
            const unsigned R=T+i*0x78;
            p32(R,rnd(3)?rnd(0x43):0x42u);p32(R+4,0x30+i);p32(R+8,rng()%64);
            p32(R+0x14,DESC+rnd(8)*0x100);p32(R+0x18,ALT+rnd(8)*0x80);
            p32(R+0x24,rnd(3)?rnd(n):0xffffffffu);p32(R+0x28,rnd(3)?rnd(n):0xffffffffu);
            p32(R+0x2c,rnd(3)?rnd(n):0xffffffffu);p32(R+0x30,rnd(3)?rnd(n):0xffffffffu);
            p32(R+0x64,rnd(2)?rng():0u);p32(R+0x68,rnd(2)?rng():0u);
        }
        // geometry: [G] = H; H flag and section/part offsets
        const unsigned H=GEO+0x100;p32(GEO,H);
        p32(H,rnd(2)?0u:1u);
        for(unsigned k=1;k<0x40;++k)p32(H+k*4,0x100+rnd(0x1000)*4);
        // car
        std::uint32_t car=CAR;
        p32(car+0x5c,rnd(3)==0);p16(car+0x64,std::uint16_t(rnd(4)?rnd(0x60):(rnd(2)?0x85+rnd(0x6a):rnd(0x220))));p32(car+0x68,rnd(4)?0x30+rnd(n+1):0u);
        p32(car+0x184,rnd(2));p16(car+0x4e,std::uint16_t(rng()));p32(car+0x1f4,rnd(200));p8(car+0x10,std::uint8_t(rnd(8)));
        for(unsigned k=0;k<6;++k)pf(car+0x14+k*4,real(-300,300));
        pf(car+0xdbc,real(-2,2));pf(car+0x1f8,real(0,1500));pf(car+0xb54,real(-600,600));pf(car+0x26c,real(-1,1));
        pf(car+0xe68,real(0,0.4f));if(rnd(2))p32(car+0x2a8,rng()&~0x10f407cu);if(rnd(2))p32(car+4,rng()&~0x80008u);p16(car+0x18c,std::uint16_t(rnd(0x60)));pf(car+0x2b0,real(-50,120));
        // stage objects, flags
        for(unsigned i=0;i<16;++i){p32(0x79ecc8u+i*0x3c,OBJ+i*0x100);p8(0x79fca4u+i,std::uint8_t(rng()));}
        p8(0x79fcceu,std::uint8_t(rng()));p32(0x79f6dcu,WORK);
        p32(0x780258u,rnd(7));p32(0x78024cu,rnd(4));p32(0x78026cu,rnd(3)?0x10u:(rnd(2)?3u:rnd(0x20)));
        p32(0x799d18u,car);p32(0x7f94c0u,rnd(2));p32(0x7f95a8u,rnd(2));
        // area block
        auto* block=reinterpret_cast<std::uint8_t*>(0x7d2d80u);
        for(unsigned k=0;k<0x748;k+=4)p32(0x7d2d80u+k,rnd(3)?rnd(4):rng());
        (void)block;
        fill_matrix(0x7d2da0u);fill_matrix(0x7d3130u);fill_matrix(0x7d3190u);
        const std::uint32_t states_e80[]={0x14,0x14,0x14,0x14,0x14,0x14,0x14,0x14,0x12,0x13,0x15,0x16,0x17,0x18,0x19,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,0x1a,0x30};
        p32(0x7d2e80u,states_e80[rnd(sizeof states_e80/4)]);
        {const std::uint32_t states_e88[]={0x14,1,3,4,7,8,0xb,0xe,0xf,0x12,0x13,0,2,5,0x15,0x30};
         p32(0x7d2e88u,states_e88[rnd(sizeof states_e88/4)]);}
        const std::uint32_t states_d80[]={0x14,1,2,14,15,0,3,0x14};
        p32(0x7d2d80u,states_d80[rnd(8)]);
        pf(0x7d3184u,rnd(4)?real(-0.2f,1.3f):1.f);pf(0x7d2e6cu,rnd(2)?-1.f:real(-10,100));
        p32(0x7d306cu,rnd(4)?GEO:0u);p32(0x7d3070u,rnd(2)?GEO:0u);
        p32(0x7d2e70u,rnd(4));p32(0x7d2e74u,rnd(4));
        p32(0x7d2e78u,rnd(0x300));p32(0x7d3064u,rnd(2));p8(0x7d33d0u,std::uint8_t(rnd(3)==0));p32(0x7d33b4u,rnd(3)==0);
        p32(0x7d2e5cu,rnd(3)?rng()%64:0u);p32(0x7d2e7cu,rnd(3));
        p32(0x7d3068u,rnd(2)?0xffffffffu:rnd(12));
        // event records: the EXE table or fixture records (other +04/+30 values)
        const unsigned FEV=fix+0xb000;
        const std::uint32_t callbacks[]={0x49a650,0x44ba50,0x44bcb0,0x44ba80,0x44baa0,0x44bac0,0x44bae0,0x44bb30,0x44bcc0,0x44bb90,0x44bbb0,0x44bcd0,0x44bbf0};
        for(unsigned r=0;r<4;++r){const unsigned E=FEV+r*0x24,I=FEV+0x200+r*0x100;
            p32(E,0x40000+rnd(0x100));p32(E+4,rnd(2)?0u:0x5a4540u);p32(E+8,rng());p32(E+0xc,1+rnd(3));
            p16(E+0x10,std::uint16_t(rnd(0x80)));p16(E+0x12,std::uint16_t(rnd(0x400)));pf(E+0x14,real(0,300));
            p32(E+0x18,callbacks[rnd(13)]);p32(E+0x1c,callbacks[rnd(13)]);p32(E+0x20,I);
            for(unsigned k=0;k<4;++k){p32(I+k*0x40+0x28,rng()&0xf);pf(I+k*0x40+0x2c,real(-5,5));p32(I+k*0x40+0x30,rnd(2));
                for(unsigned q=0;q<3;++q)pf(I+k*0x40+0x34+q*4,real(-200,200));}}
        auto event=[&]{return rnd(2)?0x6369c0u+rnd(12)*0x24:FEV+rnd(4)*0x24;};
        p32(0x7d349cu,rnd(3)?event():0u);p32(0x7d34a4u,event());p32(0x7d34a0u,rnd(0x400));
        for(std::uint32_t a=0x7d3074u;a<=0x7d30a4u;a+=4)p32(a,rnd(4)?rnd(3):rnd(0x400));
        if(rnd(3)==0){   // ending selection deep paths: clean run statistics
            for(std::uint32_t a:{0x7d3074u,0x7d3078u,0x7d307cu})p32(a,0);
            p32(0x7d3088u,rnd(3)?0u:rnd(8));p32(0x7d3094u,rnd(8)?0x40u+rnd(0x200):0u);p32(0x7d3090u,rnd(4)?rnd(0x80):0u);
            p32(0x7d308cu,0xf0+rnd(0x40));p32(0x7d3080u,rnd(3)==0);p32(0x7d3084u,rnd(3)?0u:rnd(0x80));p32(0x7d30a4u,rnd(0x300));}
        p32(0x7d2df4u,DF4);p32(DF4,rnd(2)?0x1cu:rng()%64);
        p32(0x7d33bcu,T);p32(0x7d33c4u,n);
        // 7D30A8 record
        const unsigned R0=0x7d30a8u;
        p32(R0+4,0x30+rnd(n+2));p32(R0+8,rng()%64);p32(R0+0x14,DESC+rnd(8)*0x100);p32(R0+0x18,ALT+rnd(8)*0x80);
        p32(R0+0x24,rnd(3)?rnd(n):0xffffffffu);p32(R0+0x28,rnd(3)?rnd(n):0xffffffffu);
        p32(R0+0x2c,rnd(2)?rnd(n):0xffffffffu);p32(R0+0x30,rnd(2)?rnd(n):0xffffffffu);p32(R0+0x64,rnd(2)?rng():0u);p32(R0+0x68,rnd(2)?rng():0u);
        p32(0x7d3188u,rnd(3)?R0:T+rnd(n)*0x78);p32(0x7d31dcu,rnd(4)?R0:T+rnd(n)*0x78);
        // module .data image
        std::memcpy(reinterpret_cast<void*>(0x635f2cu),RaceAreaDataImage,0xc98);
        if(rnd(2)){p32(0x635f2cu,0x30+rnd(n+1));p32(0x635f30u,rng()%64);}
        p32(0x636bb8u,rnd(4));if(rnd(8)==0)p32(0x6368c4u,rnd(14));
        // sky
        std::memcpy(reinterpret_cast<void*>(0x638664u),RaceSkyDataImage,0x10);
        if(rnd(2))p16(0x638664u,std::uint16_t(rnd(0x200)));
        p32(0x7d3a64u,rnd(4));pf(0x8a8c18u,real(0,4));fill_matrix(0x95dba0u);
        for(unsigned k=0;k<0x120;k+=4)p32(WORK+k,rnd(3)?rnd(3):rng());
        for(unsigned l=0;l<2;++l){const unsigned r=WORK+l*0x90;
            for(unsigned k=0;k<3;++k)p32(r+k*4,rnd(3)?0x300000u+rnd(0x40):0xffffffffu);
            for(unsigned k=0;k<4;++k)pf(r+0x10+k*4,real(-3,3));
            p16(r+0x28,std::uint16_t(rnd(40)));p16(r+0x2a,std::uint16_t(rnd(40)-5));p16(r+0x2c,std::uint16_t(rnd(40)));
            pf(r+0x30,real(-0.2f,1.2f));pf(r+0x34,real(-0.2f,1.2f));p32(r+0x38,rnd(2));p32(r+0x3c,rnd(2));pf(r+0x40,real(0,1));
            fill_matrix(r+0x50);}
        p16(WORK+0xb8,std::uint16_t(rnd(40)-5));p16(WORK+0xba,std::uint16_t(rnd(40)-5));p16(WORK+0xbc,std::uint16_t(rnd(40)-5));
        // bank object counts (448810 + 0xC)
        for(unsigned r=0;r<16;++r)p32(0x7c2800u+r*0x48+0xc,rnd(6));
        // service values
        value.clear();
        for(const auto& e:services)value[e.pc]=rnd(3)==0?1u:0u;
        for(std::uint32_t pc:{0x48b310u,0x48b350u,0x4957f0u,0x495490u,0x46c500u,0x44ff10u,0x43f960u,0x450130u,0x4556f0u,0x451e90u})value[pc]=rnd(4)==0?rng():0u;
        for(std::uint32_t pc:{0x448980u,0x42df90u,0x4299a0u,0x448960u,0x44f880u,0x46c4d0u,0x45aee0u,0x46c240u,0x4502c0u,0x450300u})value[pc]=rnd(4)?1u:0u;
        value[0x451350u]=rnd(4);value[0x450380u]=0x30+rnd(n+1);value[0x44fd80u]=0x5000+rnd(0x100);value[0x4872e0u]=rnd(3)?1u:rnd(3);
        value[0x487320u]=rnd(5)?rnd(16):0xffffu|(rng()&0xffff0000u);value[0x43d470u]=rnd(16);value[0x40ecb0u]=rng();
        value[0x49b2d0u]=rnd(3)?rnd(0x100):rng();value[0x455b80u]=rnd(4);value[0x455ba0u]=rnd(8);value[0x450630u]=rng()%64;value[0x47fbd0u]=rng()%64;
        value[0x455bd0u]=rng();value[0x406780u]=rnd(4);value[0x448820u]=rnd(3)?1u:rnd(3);value[0x440d30u]=rng();value[0x4f0910u]=rng();
        value[0x45c470u]=rnd(2)?STR:0u;
        {const char* s="stage_obj.sz";std::memcpy(reinterpret_cast<void*>(STR),s,std::strlen(s)+1);}
        // Online next-area path (7F94C0, 7F95A8, 46C500) indexes the table with
        // the current record's +24 unchecked: keep it valid there.
        if(g32(0x7f94c0u)&&g32(0x7f95a8u)&&value[0x46c500u]){
            p32(R0+0x24,rnd(n));for(unsigned i=0;i<n;++i)p32(T+i*0x78+0x24,rnd(n));}
        for(std::size_t k=0;k<services.size();++k)p32(values+4*unsigned(k),value[services[k].pc]);
    };
    const unsigned total_runs=count;
    for(unsigned run=0;run<total_runs;++run){
        std::map<std::uint32_t,std::uint32_t> value;build(run,value);
        // entry selection
        unsigned entry=sky?run%7u:run%12u;
        if(const char* only_entry=std::getenv("OR2_RACE_ENTRY"))entry=unsigned(std::stoul(only_entry));
        const std::uint32_t entry_ecx=rng();
        if(sky&&entry==1&&rnd(12)==0)p32(rnd(2)?0x7d3188u:0x7d31dcu,0);
        const std::uint32_t car=g32(0x799d18u);
        const std::uint32_t desc=g32(0x7d3188u)?g32(g32(0x7d3188u)+0x14):0u;
        // sky alpha / object for 452080
        const std::uint32_t sky_object=0x300000u+rnd(0x40);const float sky_alpha=rnd(3)?real(0.5f,1.1f):real(-0.1f,0.3f);
        for(auto& m:mirrors){m.bytes.resize(m.end-m.base);std::memcpy(m.bytes.data(),reinterpret_cast<void*>(m.base),m.bytes.size());}
        reset_matrix_oracle_globals();
        std::vector<std::uint8_t> arena(0x800);Bytes ar(arena.data(),arena.size());
        for(unsigned k=0;k<16;++k)ar.putf(k*4,k%5==0?1.f:real(-0.5f,0.5f));
        std::memcpy(reinterpret_cast<void*>(MatrixArena),arena.data(),arena.size());
        p32(strace,0);p32(dtrace,0);p32(tls,0x7fffabcdu);
        // original
        std::uint32_t original_eax=0;
        if(!sky){
            switch(entry){
            case 0:prepare(0x44cb00);break;
            case 1:prepare(0x44f7c0);guest_call.ecx=entry_ecx;break;
            case 2:prepare(0x44f120);break;
            case 3:prepare(0x44b7f0);break;
            case 4:prepare(0x44e590);guest_call.ecx=entry_ecx;break;
            case 5:prepare(0x44cf00);break;
            case 6:prepare(0x44f190);break;
            case 7:prepare(0x44de10);reinterpret_cast<unsigned*>(S)[0]=car;reinterpret_cast<unsigned*>(S)[1]=desc;break;
            case 8:prepare(0x44e290);guest_call.eax=car;reinterpret_cast<unsigned*>(S)[0]=desc;break;
            case 9:prepare(0x44d410);break;
            case 11:prepare(0x44cd30);break;
            default:prepare(0x44ede0);break;
            }
        }else{
            switch(entry){
            case 0:prepare(0x451a30);reinterpret_cast<unsigned*>(S)[0]=WORK;break;
            case 1:prepare(0x451b40);reinterpret_cast<unsigned*>(S)[0]=WORK;break;
            case 2:prepare(0x4521c0);reinterpret_cast<unsigned*>(S)[0]=WORK;break;
            case 3:prepare(0x452080);guest_call.esi=sky_object;{float a=sky_alpha;std::memcpy(reinterpret_cast<unsigned*>(S),&a,4);}break;
            case 4:prepare(0x451dd0);break;
            case 6:prepare(0x451c20);break;
            default:prepare(0x451e90);break;
            }
        }
        const bool sky_null=sky&&entry==1&&(g32(0x7d3188u)==0||g32(0x7d31dcu)==0);
        if(!sky_null){run_original32();original_eax=guest_call.out_eax;
            if(guest_call.out_sp!=S){fail("guest stack imbalance entry "+std::to_string(entry));++cases;continue;}}
        // native
        PcRaceMemory mem;
        std::vector<std::vector<std::uint8_t>> native(mirrors.size());
        for(std::size_t i=0;i<mirrors.size();++i){native[i]=mirrors[i].bytes;mem.map(mirrors[i].base,native[i].data(),native[i].size());}
        race_area_map_tables(mem);
        PcMatrixStack stack{ar,0,0,64};
        struct NCall{std::uint32_t pc,ecx;decltype(PcRaceCall::args) a;unsigned argc;};
        std::vector<NCall> calls;std::vector<PcVehicleDrawCall> draws;
        PcRaceService service=[&](const PcRaceCall& k)->std::uint32_t{
            if(k.pc==0x448810u)return g32(0x7c2800u+k.args[0]*0x48u+0xc);
            if(!slot_of.count(k.pc)){std::ostringstream e;e<<"native called an unstubbed service "<<std::hex<<k.pc;throw std::runtime_error(e.str());}
            calls.push_back({k.pc,k.ecx,k.args,k.argc});
            return value.at(k.pc);
        };
        PcRaceContext ctx{mem,stack,service,&draws};
        std::uint32_t native_eax=0;bool threw=false;
        try{
            if(!sky){
                switch(entry){
                case 0:race_area_init_44cb00(ctx);break;
                case 1:race_area_control_44f7c0(ctx,entry_ecx);break;
                case 2:race_area_display_44f120(ctx);break;
                case 3:race_area_destroy_44b7f0(ctx);break;
                case 4:race_area_control_44e590(ctx,entry_ecx);break;
                case 5:native_eax=race_area_stage_44cf00(ctx);break;
                case 6:race_area_objects_44f190(ctx);break;
                case 7:race_area_geometry_44de10(ctx,car,desc);break;
                case 8:race_area_stage_draw_44e290(ctx,car,desc);break;
                case 9:native_eax=race_area_ending_44d410(ctx);break;
                case 11:race_env_model_44cd30(ctx);break;
                default:race_area_stats_44ede0(ctx);break;
                }
            }else{
                switch(entry){
                case 0:race_sky_init_451a30(ctx,WORK);break;
                case 1:race_sky_control_451b40(ctx,WORK);break;
                case 2:race_sky_display_4521c0(ctx,WORK);break;
                case 3:race_sky_object_452080(ctx,sky_object,sky_alpha);break;
                case 4:race_sky_reset_451dd0(ctx);break;
                case 6:race_env_sky_451c20(ctx,g32(0x79f6dcu));break;
                default:native_eax=race_sky_step_451e90(ctx);break;
                }
            }
        }catch(const std::exception& ex){
            threw=true;
            if(!sky_null){++cases;fail("native threw entry "+std::to_string(entry)+" run "+std::to_string(run)+": "+ex.what());continue;}
        }
        if(sky_null){++cases;if(!threw)fail("451B40 null record: native did not report the unwritten PC stack words");++coverage["sky_null_reported"];continue;}
        const std::string tag=(sky?"sky entry ":"area entry ")+std::to_string(entry)+" run "+std::to_string(run);
        ++coverage[(sky?"sky_entry_":"area_entry_")+std::to_string(entry)];
        if(!sky&&(entry==5||entry==9)){++cases;if(native_eax!=original_eax)fail(tag+" EAX");}
        if(sky&&entry==5){++cases;if(native_eax!=original_eax)fail(tag+" EAX");}
        if(inject&&run==total_runs-1){if(!native.empty())native[0][0x100]^=1;}
        // regions
        for(std::size_t i=0;i<mirrors.size();++i){
            std::ostringstream s;s<<tag<<" region "<<std::hex<<mirrors[i].base;
            compare(native[i].data(),reinterpret_cast<void*>(mirrors[i].base),native[i].size(),s.str());
        }
        compare(arena.data(),reinterpret_cast<void*>(MatrixArena),arena.size(),tag+" matrix arena");
        {std::array<std::uint32_t,2> sp{MatrixArena+std::uint32_t(stack.current_offset),std::uint32_t(stack.depth)};
         compare(sp.data(),reinterpret_cast<void*>(0x89b564),8,tag+" matrix pointer/depth");}
        // services
        const unsigned n=g32(strace);++cases;
        if(n>SvcCap)throw std::runtime_error("service trace overflow");
        coverage["service_calls"]+=n;
        if(n!=calls.size()){std::ostringstream s;s<<tag<<" service count PC="<<n<<" native="<<calls.size();
            for(unsigned i=0;i<std::max<std::size_t>(n,calls.size())&&i<12;++i)s<<"\n   "<<std::hex<<(i<n?g32(strace+16+i*SvcStride):0)<<" / "<<(i<calls.size()?calls[i].pc:0)<<std::dec;
            fail(s.str());}
        else for(unsigned i=0;i<n;++i){
            const unsigned at=strace+16+i*SvcStride;const auto& k=calls[i];
            bool ok=g32(at)==k.pc&&k.argc==argc_of[k.pc];
            for(unsigned a=0;ok&&a<k.argc;++a){
                const std::uint32_t ga=g32(at+12+4*a);
                const bool local=(k.pc==0x46c380u&&a==0)||(k.pc==0x4f03a0u&&a==0)||(k.pc==0x44fc60u&&(a==1||a==2));
                if(local)ok=(ga>=S-0x400&&ga<S+0x40)&&(k.a[a]==PcRaceLocal44E590||k.a[a]==PcRaceLocal44C310A||k.a[a]==PcRaceLocal44C310B);
                else ok=ga==k.a[a];
            }
            if(!ok){std::ostringstream s;s<<tag<<" service "<<i<<" PC "<<std::hex<<g32(at)<<"("<<g32(at+12)<<","<<g32(at+16)<<","<<g32(at+20)<<") native "<<k.pc<<"("<<k.a[0]<<","<<k.a[1]<<","<<k.a[2]<<")";fail(s.str());break;}
        }
        // draw list
        const unsigned dn=g32(dtrace);++cases;coverage["draw_calls"]+=dn;
        if(dn>DrawCap)throw std::runtime_error("draw trace overflow");
        if(dn!=draws.size()){std::ostringstream s;s<<tag<<" draw count PC="<<dn<<" native="<<draws.size();
            for(unsigned i=0;i<std::max<std::size_t>(dn,draws.size())&&i<12;++i)s<<"\n   "<<std::hex<<(i<dn?g32(dtrace+0x10+i*DrawStride):0)<<"("<<(i<dn?g32(dtrace+0x14+i*DrawStride):0)<<") / "<<(i<draws.size()?draws[i].pc:0)<<"("<<(i<draws.size()?draws[i].args[0]:0)<<")"<<std::dec;
            fail(s.str());}
        else for(unsigned i=0;i<dn;++i){
            const unsigned at=dtrace+0x10+i*DrawStride;const auto& d=draws[i];
            std::vector<std::uint8_t> nt(DrawStride,0),ot(DrawStride,0);std::memcpy(ot.data(),reinterpret_cast<void*>(at),DrawStride);
            Bytes nb(nt.data(),nt.size());nb.put32(0,d.pc);for(unsigned a=0;a<d.argc;++a)nb.put32(4+a*4,d.args[a]);
            for(unsigned a=d.argc;a<7;++a)std::memcpy(nt.data()+4+a*4,ot.data()+4+a*4,4);   // unused slots: caller stack bytes
            std::memcpy(nt.data()+0x20,d.matrix.data(),64);
            compare(nt.data(),ot.data(),DrawStride,tag+" draw "+std::to_string(i));
        }
    }
    std::cerr<<"coverage:";for(const auto& [k,v]:coverage)std::cerr<<' '<<k<<'='<<v;std::cerr<<"\n";
    std::cout<<"{\"routine\":\""<<only<<"\",\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}"<<'\n';return mismatches?1:0;
}

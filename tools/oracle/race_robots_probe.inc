// Included inside the compare_pc dispatcher; uses its private x86 harness.
if(only=="race_robots_probe"){
    // Original race robots (Lindbergh RobDriver / RobFlagMan: events 362 ROB01
    // and 364 ROB03: 4884D0/488DD0/4879B0/487980 and 488710/488820/4879D0/
    // 487A00, plus the module parts 487810, 487C30, 487EE0, 487940, 487880,
    // 487A30, 487B70, 488450, 488AF0 and the CRT qsort 580CB0) against
    // race_robots. The module code runs unmodified with its protected bridges
    // live (4480ED, 103C50C, 40EADB snippets, the 488AF3 and 580CB4 VMs); the
    // natively ported leaves (math/matrix leaves over D3DX, RobMotion
    // accessors, getters) run as original code. Every other callee is a
    // capture stub recording (entry, ECX, stack arguments, current matrix)
    // and returning a scripted per-case value that the native service returns
    // too. Every mapped region, the matrix arena/stack, the ordered service
    // calls (with their matrices) and the return values are compared.
    using namespace outrun::platform;
    if(argc<=7||std::string(argv[7])=="-")throw std::runtime_error("race robots oracle requires pinned d3dx9_29.dll");
    constexpr unsigned base=0x3a800000u,strace=base+0x10000u,values=base+0x30000u;
    constexpr unsigned fix=base+0x40000u,fix_size=0x30000u;
    constexpr unsigned stubs=base;
    constexpr unsigned SvcStride=0x80u,SvcCap=0x3f0u;
    map_at(base,0x80000,PROT_READ|PROT_WRITE|PROT_EXEC);map_original_arithmetic_bridge(file);
    {   unsigned restored=0;
        const std::size_t text_file=pe_file_offset(file,0x401000u);
        for(std::uint32_t a=0x401000u;a<0x401000u+0x194881u;a+=0x100u){
            const std::size_t n=std::min<std::size_t>(0x100u,0x401000u+0x194881u-a);
            if(std::memcmp(reinterpret_cast<void*>(a),file.data()+text_file+(a-0x401000u),n)){r060_raw_write(a,file.data()+text_file+(a-0x401000u),n);++restored;}
        }
        std::cerr<<"restored "<<restored<<" patched .text blocks"<<char(10);
    }
    init_matrix_oracle_globals();wire_d3dx_imports(map_d3dx29(argv[7]));
    for(unsigned page:{0x654000u,0x780000u,0x799000u,0x79f000u,0x7a0000u,0x7c2000u,0x7d3000u,0x7dd000u,0x7df000u,0x7f9000u,0x82f000u,0x84d000u,0x85a000u,0x89b000u})
        if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("race robots globals");
    auto emit32=[](std::vector<std::uint8_t>& b,std::uint32_t v){for(unsigned k=0;k<4;++k)b.push_back(std::uint8_t(v>>(k*8)));};
    // ---- service capture stubs -------------------------------------------
    struct Svc{std::uint32_t pc;unsigned argc;bool thiscall;};
    const std::vector<Svc> services{
        {0x4f1cf0,2,true},{0x4f1d30,1,true},{0x4f1e60,1,true},{0x4f2260,0,true},{0x4f2280,1,true},{0x4f2320,3,true},{0x4f2520,0,true},
        {0x514bf0,1,false},{0x514f60,2,false},{0x5147d0,1,false},{0x515040,2,false},{0x5148d0,1,false},{0x5148f0,1,false},{0x514e60,1,false},{0x514e80,1,false},
        {0x402100,0,false},{0x402140,0,false},{0x402170,0,false},{0x4021a0,0,false},{0x4082b0,3,false},{0x40d840,1,false},{0x409e00,1,false},
        {0x49b2d0,0,false},{0x43f860,0,false},{0x440d50,1,false},{0x440d70,0,false},{0x44bce0,0,false},{0x44bd20,0,false},{0x44dc50,1,false},
        {0x44fe70,0,false},{0x44ff10,0,false},{0x450240,0,false},{0x450380,1,false},{0x451350,1,false},{0x45a2b0,1,false},{0x46bc30,4,false},
        {0x47fbd0,3,false},{0x417f70,0,false},{0x580f33,1,false},{0x580f40,0,false}};
    if(services.size()!=sizeof PcRaceRobotServices/sizeof PcRaceRobotServices[0])throw std::runtime_error("race robots service list");
    for(std::size_t k=0;k<services.size();++k)if(services[k].pc!=PcRaceRobotServices[k])throw std::runtime_error("race robots service order");
    std::map<std::uint32_t,unsigned> argc_of;
    unsigned stub_off=0;
    auto install=[&](std::uint32_t pc,const std::vector<std::uint8_t>& code){
        if(stub_off+code.size()>0x10000)throw std::runtime_error("race robots stubs");
        std::memcpy(reinterpret_cast<void*>(stubs+stub_off),code.data(),code.size());
        r046_write_rel32(pc,0xe9,stubs+stub_off);stub_off=(stub_off+unsigned(code.size())+15u)&~15u;
    };
    for(std::size_t k=0;k<services.size();++k){
        const auto& e=services[k];argc_of[e.pc]=e.argc;
        std::vector<std::uint8_t> c{0x52,0x8b,0x15};emit32(c,strace);                          // push edx; mov edx,[count]
        c.insert(c.end(),{0xc1,0xe2,0x07,0xc7,0x82});emit32(c,strace+16);emit32(c,e.pc);        // shl edx,7; mov [edx+t+16],pc
        c.insert(c.end(),{0x89,0x82});emit32(c,strace+20);c.insert(c.end(),{0x89,0x8a});emit32(c,strace+24); // eax, ecx
        for(unsigned i=0;i<6;++i){
            if(i<e.argc)c.insert(c.end(),{0x8b,0x4c,0x24,std::uint8_t(8+4*i)});else c.insert(c.end(),{0x31,0xc9});
            c.insert(c.end(),{0x89,0x8a});emit32(c,strace+28+4*i);}
        // current matrix: push esi/edi; mov esi,[89B564]; lea edi,[edx+t+52]; mov ecx,16; rep movsd; pop edi/esi
        c.insert(c.end(),{0x56,0x57,0x8b,0x35,0x64,0xb5,0x89,0x00,0x8d,0xba});emit32(c,strace+52);
        c.insert(c.end(),{0xb9,0x10,0,0,0,0xf3,0xa5,0x5f,0x5e});
        c.insert(c.end(),{0xff,0x05});emit32(c,strace);                                         // inc [count]
        c.push_back(0xa1);emit32(c,values+4*unsigned(k));                                        // mov eax,[value]
        c.push_back(0x5a);                                                                       // pop edx
        if(e.thiscall&&e.argc){c.insert(c.end(),{0xc2,std::uint8_t(e.argc*4),0});}else c.push_back(0xc3);
        install(e.pc,c);
    }
    // ---- helpers -------------------------------------------------------------
    std::mt19937 rng(0x4884d000u);
    auto rnd=[&](std::uint32_t n){return n?std::uint32_t(rng()%n):0u;};
    auto real=[&](float lo,float hi){return lo+(hi-lo)*(float(rng()&0xffffu)/65535.f);};
    auto g32=[](std::uint32_t a){return *reinterpret_cast<std::uint32_t*>(a);};
    auto p32=[](std::uint32_t a,std::uint32_t v){*reinterpret_cast<std::uint32_t*>(a)=v;};
    auto p16=[](std::uint32_t a,std::uint16_t v){*reinterpret_cast<std::uint16_t*>(a)=v;};
    auto p8=[](std::uint32_t a,std::uint8_t v){*reinterpret_cast<std::uint8_t*>(a)=v;};
    auto pf=[](std::uint32_t a,float v){*reinterpret_cast<float*>(a)=v;};
    unsigned cases{},mismatches{};const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
    std::map<std::string,unsigned> coverage;
    auto fail=[&](const std::string& what){++mismatches;if(mismatches<20)std::cerr<<what<<"\n";};
    auto compare=[&](const void* native,const void* original,std::size_t n,const std::string& what){
        ++cases;if(!std::memcmp(native,original,n))return;
        const auto* a=static_cast<const std::uint8_t*>(native);const auto* b=static_cast<const std::uint8_t*>(original);
        for(std::size_t i=0;i<n;++i)if(a[i]!=b[i]){std::ostringstream s;s<<what<<" offset=0x"<<std::hex<<i<<" PC="<<unsigned(b[i])<<" native="<<unsigned(a[i]);fail(s.str());break;}
    };
    {   // Embedded EXE bytes against the pinned EXE as mapped.
        PcRaceRobotState st;
        compare(st.data.data(),reinterpret_cast<void*>(PcRaceRobotState::DataBase),st.data.size(),"robot .data image 654568");
        for(const auto& t:RaceRobotTables)compare(t.data,reinterpret_cast<void*>(t.base),t.size,"robot EXE table");
    }
    std::vector<std::uint8_t> data_image(RaceRobotsDataImage,RaceRobotsDataImage+sizeof RaceRobotsDataImage);
    struct Mirror{std::uint32_t base,end;std::vector<std::uint8_t> bytes;};
    std::vector<Mirror> mirrors{{PcRaceRobotState::BlockBase,PcRaceRobotState::BlockEnd,{}},{PcRaceRobotState::WorkBase,PcRaceRobotState::WorkEnd,{}},
        {PcRaceRobotState::DataBase,PcRaceRobotState::DataEnd,{}},{0x780240u,0x780280u,{}},{0x799d18u,0x799d1cu,{}},{0x79f010u,0x79f014u,{}},
        {0x79f574u,0x79f578u,{}},{0x79fb48u,0x79fce8u,{}},{0x7c2400u,0x7c2404u,{}},{0x7d3874u,0x7d3878u,{}},{0x7d387cu,0x7d3880u,{}},
        {0x7d3944u,0x7d3948u,{}},{0x7d39f0u,0x7d39f4u,{}},{0x7dd138u,0x7dd13cu,{}},{0x7df108u,0x7df10cu,{}},{0x7f94c0u,0x7f94c4u,{}},
        {0x84d970u,0x84d978u,{}},{0x85a714u,0x85a718u,{}},{fix,fix+fix_size,{}}};
    // fixtures
    const unsigned CAR=fix,CAM=fix+0x4400,LIGHT=fix+0x4600,NAMETAB=fix+0x4700,LISTS=fix+0x4a00,STR=fix+0x8a00,BONES=fix+0x9000,MOTLIST=fix+0xf000;
    const char* names[]={"fm_wait","fm_flag","fm_run","dr_idle","a","b"};
    auto fill_matrix=[&](std::uint32_t a){for(unsigned k=0;k<16;++k)pf(a+k*4,k==15?1.f:(k>=12?real(-200,200):real(-1.5f,1.5f)));};
    auto build=[&](unsigned run,std::map<std::uint32_t,std::uint32_t>& value){
        (void)run;
        for(unsigned k=0;k<fix_size;k+=4)p32(fix+k,rng());
        // cars (4 * 0x10F0)
        for(unsigned i=0;i<4;++i){const unsigned C=CAR+i*0x10f0;
            p32(C+4,rng());p8(C+0x11,std::uint8_t(rnd(30)));
            for(unsigned k=0;k<3;++k)pf(C+0x14+k*4,real(-60,60));
            pf(C+0x58,real(0,2));p32(C+0x5c,rnd(3));
            fill_matrix(C+0xb0);fill_matrix(C+0xf0);
            p16(C+0x204,std::uint16_t(rng()));p32(C+0x2f0,rng());pf(C+0x2f8,rnd(2)?real(-1,1):0.f);p32(C+0xd18,rnd(4)?rnd(400)-200:rng());
            if(rnd(3)==0)p32(C+4,(rng()&~0xc000u)|0x4000u);}
        for(unsigned k=0;k<3;++k){pf(CAM+0xf8+k*4,real(-60,60));pf(CAM+0x104+k*4,real(-60,60));}
        for(unsigned k=0;k<8;++k)pf(LIGHT+k*4,real(-2,2));
        // strings and name table: [84D970]+(id>>16)*16+4 -> list; list[id&0xffff] -> string
        for(unsigned s=0;s<6;++s)std::memcpy(reinterpret_cast<void*>(STR+s*0x20),names[s],std::strlen(names[s])+1);
        for(unsigned g=0;g<0x30;++g)p32(NAMETAB+g*16+4,LISTS+(g%8)*0x200);
        for(unsigned l=0;l<8;++l)for(unsigned k=0;k<0x80;++k)p32(LISTS+l*0x200+k*4,STR+rnd(6)*0x20);
        for(unsigned k=0;k<0x20*0x350;k+=4)pf(BONES+k,real(-1.5f,1.5f));
        for(unsigned k=0;k<8;++k)p32(MOTLIST+k*4,0x60000u+rnd(0x80));
        // module block and motion objects
        for(std::uint32_t a=PcRaceRobotState::BlockBase;a<PcRaceRobotState::BlockEnd;a+=4)p32(a,rnd(3)?rnd(4):rng());
        for(unsigned i=0;i<8;++i){const unsigned M=PcRaceRobotState::MotionBase+i*PcRaceRobotState::MotionSize;
            const std::uint32_t tab[]={0x60036,0x250002,0x260012,0x60015};
            p32(M+4,rnd(2)?tab[rnd(4)]:0x60000u+rnd(0x80));p32(M+8,rng());p16(M+0x68,std::uint16_t(rnd(0x30)));
            p32(M+0x74,BONES);p32(M+0xa0,STR+rnd(6)*0x20);}
        p32(0x82f11cu,rnd(2)?0x654760u+rnd(5)*24u:MOTLIST);p32(0x82f5e8u,rnd(6));p32(0x82f5e4u,rnd(3)==0);
        {const std::uint32_t states[]={0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,1,2,4,5,9,13,12,8};p32(0x82f588u,states[rnd(sizeof states/4)]);}
        p32(0x82f590u,rnd(9));p32(0x82f594u,rnd(4)?rnd(3):(rnd(2)?0xffffffffu:rnd(400)));p16(0x82f58cu,std::uint16_t(0x70+rnd(16)));p32(0x82f59cu,rnd(2));
        p8(0x82f2f8u,std::uint8_t(rnd(2)));p8(0x82f2f9u,std::uint8_t(rnd(2)));
        {const std::uint32_t bases[]={0x5c0e9cu,0x5c0ebcu,0x5c0edcu};const std::uint32_t row=bases[rnd(3)]+rnd(4)*8u;p32(0x82f598u,row);
         // substates 2/3 read [[row]], 6/7 read [[row+4]] unchecked (only reached after 1/5 found them)
         const std::uint32_t sub=g32(0x82f590u);
         if((sub==2u||sub==3u)&&!g32(row))p32(0x82f590u,1);
         if((sub==6u||sub==7u)&&!g32(row+4))p32(0x82f590u,5);}
        for(unsigned k=0;k<3;++k){pf(0x82f2fcu+k*4,real(-60,60));pf(0x82f378u+k*4,real(-3.2f,3.2f));}
        // works
        for(std::uint32_t a=PcRaceRobotState::WorkBase;a<PcRaceRobotState::WorkEnd;a+=4)p32(a,rng());
        for(unsigned i=0;i<8;++i){const unsigned W=PcRaceRobotState::WorkBase+i*0x90;
            p32(W,rnd(8)?i:rnd(8));p32(W+4,rng()&(rnd(2)?0xfffeffffu:0xffffffffu));
            for(unsigned k=0;k<3;++k)pf(W+0x40+k*4,real(-60,60));
            {const std::uint32_t pend[]={0xffffffffu,0xfffffffeu,0x60011u,0x200004u,0xfffffff0u};p32(W+0x68,pend[rnd(5)]);}}
        // module .data (6546C0 sometimes shuffled: qsort input)
        std::memcpy(reinterpret_cast<void*>(PcRaceRobotState::DataBase),data_image.data(),data_image.size());
        if(rnd(2))for(unsigned k=0;k<0x22;++k)p32(0x6546c0u+k*4,rnd(3)?g32(0x6546c0u+rnd(0x22)*4):0x60000u+rnd(0x80));
        else if(rnd(2)){auto* t=reinterpret_cast<std::uint32_t*>(0x6546c0u);std::sort(t,t+0x22);}   // as after the first 4884D0
        // globals
        p32(0x799d18u,CAR);p32(0x79f010u,PcRaceRobotState::WorkBase);p32(0x79f574u,CAM);
        for(std::uint32_t a=0x79fb48u;a<0x79fce8u;++a)p8(a,std::uint8_t(rng()));
        if(rnd(2))p8(0x79fcafu,std::uint8_t((rng()&~3u)|2u));                 // observer (event 359) running
        // flag man seen from the camera: eye near the flag man, looking at it
        if(rnd(2)){const std::uint32_t W=PcRaceRobotState::WorkBase+(rnd(4)?2u:rnd(8))*0x90;
            for(unsigned k=0;k<3;++k){const float f=*reinterpret_cast<float*>(W+0x40+k*4);pf(CAM+0xf8+k*4,f+real(-20,20));pf(CAM+0x104+k*4,f+real(-5,5));}}
        // runaway: cars around the flag man
        if(rnd(2))for(unsigned i=0;i<4;++i){const unsigned C=CAR+i*0x10f0;for(unsigned k=0;k<3;++k)pf(C+0x14+k*4,*reinterpret_cast<float*>(0x82f2fcu+k*4)+real(-30,30));
            for(unsigned k=0;k<3;++k)pf(C+0xb0+0x30+k*4,*reinterpret_cast<float*>(0x82f2fcu+k*4)+real(-30,30));}
        if(rnd(3)==0){p32(PcRaceRobotState::WorkBase+0x68,0xffffffffu);pf(CAR+0x2f8,0.f);}   // driver: no pending/strain
        p32(0x780258u,rnd(9));p32(0x78024cu,rnd(5));p32(0x78026cu,rnd(3)?0x10u:3u);
        p32(0x7c2400u,rnd(6));p32(0x7d3874u,rng());p32(0x7d387cu,rnd(0x200));p32(0x7d3944u,rng());p32(0x7d39f0u,rng());
        p8(0x7dd138u,std::uint8_t(rng()));p32(0x7df108u,rng());p8(0x7df10bu,std::uint8_t(rnd(6)-1));
        p32(0x7f94c0u,rnd(2));p32(0x84d970u,NAMETAB);p32(0x84d974u,rng());p32(0x85a714u,rnd(2));
        // service values
        value.clear();
        for(const auto& e:services)value[e.pc]=rng();
        auto timer=[&]{const std::uint16_t t[]={0,1,0x77,0x78,0xffff,0x100};return (rng()&0xffff0000u)|(rnd(4)?t[rnd(6)]:std::uint16_t(rng()));};
        value[0x49b2d0u]=timer();value[0x4082b0u]=LIGHT;
        value[0x43f860u]=rnd(2)?(rng()&0xffffff00u):rng()|1u;
        for(std::uint32_t pc:{0x44bce0u,0x44fe70u,0x44ff10u})value[pc]=rnd(3)?0u:(rnd(2)?1u:rng());
        value[0x450240u]=rnd(3)?4u:rnd(8);value[0x44bd20u]=rnd(15)-1u;
        {const std::uint32_t v[]={0x1a,0x1b,0x1c,0};value[0x44dc50u]=v[rnd(4)];}
        value[0x451350u]=rnd(8);value[0x47fbd0u]=rnd(0x200);value[0x45a2b0u]=(rng()&0xffffff00u)|rnd(6);value[0x580f40u]=rnd(0x8000);
        for(std::size_t k=0;k<services.size();++k)p32(values+4*unsigned(k),value[services[k].pc]);
    };
    const unsigned total_runs=count;
    for(unsigned run=0;run<total_runs;++run){
        std::map<std::uint32_t,std::uint32_t> value;build(run,value);
        unsigned entry=run%13u;
        if(const char* only_entry=std::getenv("OR2_ROBOT_ENTRY"))entry=unsigned(std::stoul(only_entry));
        const std::uint32_t k=entry<4?(rnd(8)?0u:rnd(8)):entry<8?(rnd(8)?2u:rnd(8)):rnd(8);
        const std::uint32_t work=PcRaceRobotState::WorkBase+k*0x90u;
        const std::uint32_t chara=rng(),motion_id=rnd(2)?g32(0x6546c0u+rnd(0x22)*4):0x60000u+rnd(0x80),position=rnd(4),qn=rnd(0x23);
        std::vector<std::uint8_t> arena(0x800);Bytes ar(arena.data(),arena.size());
        for(unsigned q=0;q<16;++q)ar.putf(q*4,q%5==0?1.f:real(-0.5f,0.5f));
        for(auto& m:mirrors){m.bytes.resize(m.end-m.base);std::memcpy(m.bytes.data(),reinterpret_cast<void*>(m.base),m.bytes.size());}
        reset_matrix_oracle_globals();
        std::memcpy(reinterpret_cast<void*>(MatrixArena),arena.data(),arena.size());
        p32(strace,0);
        // original
        auto* st=reinterpret_cast<unsigned*>(S);
        switch(entry){
        case 0:prepare(0x4884d0);st[0]=work;break;
        case 1:prepare(0x488dd0);st[0]=work;break;
        case 2:prepare(0x4879b0);st[0]=work;break;
        case 3:prepare(0x487980);st[0]=work;break;
        case 4:prepare(0x488710);st[0]=work;break;
        case 5:prepare(0x488820);st[0]=work;break;
        case 6:prepare(0x4879d0);st[0]=work;break;
        case 7:prepare(0x487a00);st[0]=work;break;
        case 8:prepare(0x487a30);break;
        case 9:prepare(0x487c30);st[0]=work;st[1]=chara;break;
        case 10:prepare(0x487940);guest_call.esi=motion_id;break;
        case 11:prepare(0x488af0);guest_call.esi=CAR;st[0]=work;st[1]=position;break;
        default:prepare(0x580cb0);st[0]=0x6546c0u;st[1]=qn;st[2]=4;st[3]=0x487920u;break;
        }
        run_original32();const std::uint32_t original_eax=guest_call.out_eax;
        const std::string tag="robot entry "+std::to_string(entry)+" run "+std::to_string(run);
        if(guest_call.out_sp!=S){fail(tag+" guest stack imbalance");++cases;continue;}
        // native
        PcRaceMemory mem;
        std::vector<std::vector<std::uint8_t>> native(mirrors.size());
        for(std::size_t i=0;i<mirrors.size();++i){native[i]=mirrors[i].bytes;mem.map(mirrors[i].base,native[i].data(),native[i].size());}
        race_robots_map_tables(mem);
        PcMatrixStack stack{ar,0,0,64};
        struct NCall{std::uint32_t pc,ecx;decltype(PcRaceCall::args) a;unsigned argc;std::array<std::uint8_t,64> matrix;};
        std::vector<NCall> calls;
        PcRaceService service=[&](const PcRaceCall& c)->std::uint32_t{
            if(!argc_of.count(c.pc)){std::ostringstream e;e<<"native called an unstubbed service "<<std::hex<<c.pc;throw std::runtime_error(e.str());}
            NCall n{c.pc,c.ecx,c.args,c.argc,{}};const auto cur=stack.current();for(unsigned q=0;q<64;++q)n.matrix[q]=cur.u8(q);
            calls.push_back(n);
            return value.at(c.pc);
        };
        PcRaceContext ctx{mem,stack,service,nullptr};
        std::uint32_t native_eax=0;
        try{
            switch(entry){
            case 0:race_robot_driver_init_4884d0(ctx,work);break;
            case 1:race_robot_driver_control_488dd0(ctx,work);break;
            case 2:race_robot_driver_display_4879b0(ctx,work);break;
            case 3:race_robot_driver_destroy_487980(ctx,work);break;
            case 4:race_robot_flagman_init_488710(ctx,work);break;
            case 5:race_robot_flagman_control_488820(ctx,work);break;
            case 6:race_robot_flagman_display_4879d0(ctx,work);break;
            case 7:race_robot_flagman_destroy_487a00(ctx,work);break;
            case 8:native_eax=race_robot_flagman_runaway_487a30(ctx);break;
            case 9:native_eax=race_robot_set_chara_487c30(ctx,work,chara);break;
            case 10:native_eax=race_robot_hand_motion_487940(mem,motion_id)?1u:0u;break;
            case 11:race_robot_driver_matrix_488af0(ctx,work,position,CAR);break;
            default:pc_crt_qsort_580cb0(mem,0x6546c0u,qn,4);break;
            }
        }catch(const std::exception& ex){++cases;fail(tag+" native threw: "+ex.what());continue;}
        ++coverage["entry_"+std::to_string(entry)];
        if(entry==8||entry==9){++cases;if(native_eax!=original_eax)fail(tag+" EAX");}
        if(entry==10){++cases;if(native_eax!=(original_eax&0xffu))fail(tag+" AL");coverage[native_eax?"hand_gu":"hand_pa"]++;}
        if(entry==1)coverage["driver_state_"+std::to_string(g32(0x82f588u))]++;
        if(entry==5)coverage[std::string("flagman_")+((g32(work+4)&0x8000u)?"near":"far")]++;
        if(inject&&run==total_runs-1){if(!native.empty())native[0][0x4d4]^=1;}
        for(std::size_t i=0;i<mirrors.size();++i){
            std::ostringstream s;s<<tag<<" region "<<std::hex<<mirrors[i].base;
            compare(native[i].data(),reinterpret_cast<void*>(mirrors[i].base),native[i].size(),s.str());
        }
        compare(arena.data(),reinterpret_cast<void*>(MatrixArena),arena.size(),tag+" matrix arena");
        {std::array<std::uint32_t,2> sp{MatrixArena+std::uint32_t(stack.current_offset),std::uint32_t(stack.depth)};
         compare(sp.data(),reinterpret_cast<void*>(0x89b564),8,tag+" matrix pointer/depth");}
        const unsigned n=g32(strace);++cases;
        if(n>SvcCap)throw std::runtime_error("service trace overflow");
        coverage["service_calls"]+=n;
        if(n!=calls.size()){std::ostringstream s;s<<tag<<" service count PC="<<n<<" native="<<calls.size();
            for(unsigned i=0;i<std::max<std::size_t>(n,calls.size())&&i<16;++i)s<<"\n   "<<std::hex<<(i<n?g32(strace+16+i*SvcStride):0)<<" / "<<(i<calls.size()?calls[i].pc:0)<<std::dec;
            fail(s.str());}
        else for(unsigned i=0;i<n;++i){
            const unsigned at=strace+16+i*SvcStride;const auto& c=calls[i];
            const auto& svc=*std::find_if(services.begin(),services.end(),[&](const Svc& x){return x.pc==c.pc;});
            bool ok=g32(at)==c.pc&&c.argc==argc_of[c.pc];
            if(ok&&svc.thiscall)ok=g32(at+8)==c.ecx;
            for(unsigned a=0;ok&&a<c.argc;++a){
                const std::uint32_t ga=g32(at+12+4*a);
                if(c.pc==0x46bc30u&&(a==2||a==3))ok=(ga>=S-0x100&&ga<S)&&c.a[a]==(a==2?PcRobotLocal488dd0A:PcRobotLocal488dd0B);
                else ok=ga==c.a[a];
            }
            coverage["svc_"+[&]{std::ostringstream s;s<<std::hex<<c.pc;return s.str();}()]++;
            if(!ok){std::ostringstream s;s<<tag<<" service "<<i<<" PC "<<std::hex<<g32(at)<<" ecx "<<g32(at+8)<<" ("<<g32(at+12)<<","<<g32(at+16)<<","<<g32(at+20)<<","<<g32(at+24)<<") native "<<c.pc<<" ecx "<<c.ecx<<" ("<<c.a[0]<<","<<c.a[1]<<","<<c.a[2]<<","<<c.a[3]<<")";fail(s.str());break;}
            compare(c.matrix.data(),reinterpret_cast<void*>(at+36),64,tag+" service "+std::to_string(i)+" matrix");
        }
    }
    std::cerr<<"coverage:";for(const auto& [k,v]:coverage)std::cerr<<' '<<k<<'='<<v;std::cerr<<"\n";
    std::cout<<"{\"routine\":\""<<only<<"\",\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}"<<'\n';return mismatches?1:0;
}

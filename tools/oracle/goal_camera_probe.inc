// Included inside the compare_pc dispatcher; uses its private x86 harness.
if(only=="goal_camera_probe"){
    // Scripted race cameras (race_goal_camera.cpp) against the original:
    //   0 486730 script (authored scripts of every scene, and random records
    //     over all 31 opcodes)   1 486EF0 motion   2 4874A0 control
    //   3 475720 PasPlCar_Ctrl   4 4755C0 (VM at 4755C6)
    // Services are capture stubs recording {pc, ECX, stack args} and
    // returning a scripted value; 427AA0 also pushes 0.0 (x87 result),
    // 43EB60 stores its value through its fifth argument (the query kind),
    // 43D920 through its first two (the two polygon numbers). The matrix
    // leaves (409F90, 409F30, 40A7D0, 40A010, 40A240, 40A410, 40A0D0,
    // 449640, 40F180), 513650 and its keys run as original code. Compared:
    // every mirrored region, the matrix arena, the service log, EAX.
    using namespace outrun::platform;
    if(argc<=7||std::string(argv[7])=="-")throw std::runtime_error("goal camera probe requires the pinned d3dx9_29.dll (D3DX rotations/inverse)");
    init_matrix_oracle_globals();wire_d3dx_imports(map_d3dx29(argv[7]));
    constexpr unsigned base=0x3c800000u,stubs=base,trace=base+0x8000u,values=base+0x7000u,Cam=base+0x40000u,Poly=base+0x50000u,Wheels=base+0x60000u;
    map_at(base,0x80000,PROT_READ|PROT_WRITE|PROT_EXEC);map_original_arithmetic_bridge(file);
    {   // D3DXMatrixInverse (40A240) imports msvcrt _finite.
        static const std::uint8_t finite_stub[]{0x8b,0x44,0x24,0x08,0x25,0x00,0x00,0xf0,0x7f,0x3d,0x00,0x00,0xf0,0x7f,0x0f,0x95,0xc0,0x0f,0xb6,0xc0,0xc3};
        std::memcpy(reinterpret_cast<void*>(stubs+0x6f00u),finite_stub,sizeof(finite_stub));
        auto* iat=reinterpret_cast<void*>(D3DX_BASE+0x1000u);
        if(mprotect(iat,4096,PROT_READ|PROT_WRITE))throw std::runtime_error("d3dx IAT page");
        *reinterpret_cast<std::uint32_t*>(D3DX_BASE+0x1270u)=stubs+0x6f00u;
        if(mprotect(iat,4096,PROT_READ))throw std::runtime_error("d3dx IAT restore");
        auto* game_iat=reinterpret_cast<void*>(0x596000u);
        if(mprotect(game_iat,4096,PROT_READ|PROT_WRITE))throw std::runtime_error("game IAT");
        *reinterpret_cast<std::uint32_t*>(0x5962dcu)=D3DX_BASE+(0x440e9cu-0x400000u);   // D3DXMatrixInverse
        if(mprotect(game_iat,4096,PROT_READ))throw std::runtime_error("game IAT restore");
    }
    {   unsigned restored=0;const std::size_t text_file=pe_file_offset(file,0x401000u);
        for(std::uint32_t a=0x401000u;a<0x401000u+0x194881u;a+=0x100u){
            const std::size_t n=std::min<std::size_t>(0x100u,0x401000u+0x194881u-a);
            if(std::memcmp(reinterpret_cast<void*>(a),file.data()+text_file+(a-0x401000u),n)){r060_raw_write(a,file.data()+text_file+(a-0x401000u),n);++restored;}
        }
        std::cerr<<"restored "<<restored<<" patched .text blocks"<<char(10);
    }
    for(std::uint32_t page=0x62f000u;page<0x98d000u;page+=0x1000u)mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE);
    auto emit32=[](std::vector<std::uint8_t>& b,std::uint32_t v){for(unsigned k=0;k<4;++k)b.push_back(std::uint8_t(v>>(k*8)));};
    // kind: 0 plain, 2 x87 0.0 result, 4 value -> [arg4], 5 value -> [arg0] and [arg1]
    struct Svc{std::uint32_t pc;unsigned argc,kind;};
    const std::vector<Svc> services{
        {0x428320,3,0},{0x401000,3,0},{0x42e020,3,0},{0x401050,0,0},{0x401030,1,0},{0x48b1e0,0,0},{0x427aa0,1,2},{0x4493c0,0,0},
        {0x450240,0,0},{0x4249f0,1,0},{0x440bb0,2,0},{0x46c3f0,0,0},{0x4957f0,0,0},{0x495b10,0,0},{0x48b310,0,0},{0x48b320,0,0},
        {0x495490,0,0},{0x4954f0,0,0},{0x46c400,1,0},{0x43d920,4,5},{0x487b70,3,0},{0x41fb10,0,0},{0x49a650,0,0},{0x4532e0,0,0},
        {0x41c420,0,0},{0x4a7d70,2,0},{0x49f7c0,2,0},{0x49f7d0,2,0},{0x4502e0,1,0},{0x440a10,2,0},{0x440a30,2,0},{0x428460,5,0},
        {0x44ff10,0,0},{0x4a2650,1,0},{0x4a2ee0,1,0},{0x4a4710,1,0},{0x4a8330,1,0},{0x43eb60,5,4},{0x43d390,3,0}};
    std::map<std::uint32_t,unsigned> index_of;unsigned stub_off=0;
    for(std::size_t k=0;k<services.size();++k){
        const auto& e=services[k];index_of[e.pc]=unsigned(k);
        std::vector<std::uint8_t> c{0x57,0x56,0x51,0x8b,0x3d};emit32(c,trace);            // push edi/esi/ecx; mov edi,[count]
        c.insert(c.end(),{0x6b,0xff,0x30,0x81,0xc7});emit32(c,trace+64);                    // imul edi,edi,0x30; add edi,entries
        c.insert(c.end(),{0xc7,0x07});emit32(c,e.pc);                                       // [edi]=pc
        c.insert(c.end(),{0x89,0x4f,0x04});                                                 // [edi+4]=ecx
        for(unsigned i=0;i<e.argc;++i)c.insert(c.end(),{0x8b,0x44,0x24,std::uint8_t(0x10+4*i),0x89,0x47,std::uint8_t(0x08+4*i)});
        c.insert(c.end(),{0xff,0x05});emit32(c,trace);                                      // inc [count]
        c.push_back(0xa1);emit32(c,values+4*unsigned(k));                                   // eax=value
        if(e.kind==4)c.insert(c.end(),{0x8b,0x4c,0x24,0x20,0x89,0x01});                    // mov ecx,[arg4]; mov [ecx],eax
        if(e.kind==5)c.insert(c.end(),{0x8b,0x4c,0x24,0x10,0x89,0x01,0x8b,0x4c,0x24,0x14,0x89,0x01});
        if(e.kind==2)c.insert(c.end(),{0xd9,0xee});                                         // fldz
        c.insert(c.end(),{0x59,0x5e,0x5f,0xc3});
        std::memcpy(reinterpret_cast<void*>(stubs+stub_off),c.data(),c.size());
        r046_write_rel32(e.pc,0xe9,stubs+stub_off);stub_off=(stub_off+unsigned(c.size())+15u)&~15u;
    }
    std::mt19937 rng(0x4874a000u);
    auto rnd=[&](std::uint32_t n){return n?std::uint32_t(rng()%n):0u;};
    auto real=[&](float a,float b){return a+(b-a)*float(rng()%100000)/100000.f;};
    auto g32=[](std::uint32_t a){return *reinterpret_cast<std::uint32_t*>(a);};
    auto p32=[](std::uint32_t a,std::uint32_t v){*reinterpret_cast<std::uint32_t*>(a)=v;};
    auto p16=[](std::uint32_t a,std::uint16_t v){std::memcpy(reinterpret_cast<void*>(a),&v,2);};
    auto pf=[](std::uint32_t a,float v){std::memcpy(reinterpret_cast<void*>(a),&v,4);};
    unsigned cases{},mismatches{};const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
    std::map<std::string,unsigned> coverage;
    auto fail=[&](const std::string& what){++mismatches;if(mismatches<20)std::cerr<<what<<"\n";};
    auto compare=[&](const void* native,const void* original,std::size_t n,const std::string& what){
        ++cases;if(!std::memcmp(native,original,n))return;
        const auto* a=static_cast<const std::uint8_t*>(native);const auto* b=static_cast<const std::uint8_t*>(original);
        for(std::size_t i=0;i<n;++i)if(a[i]!=b[i]){std::ostringstream s;s<<what<<" offset=0x"<<std::hex<<i<<" PC="<<unsigned(b[i])<<" native="<<unsigned(a[i]);fail(s.str());break;}
    };
    constexpr std::uint32_t Player=0x7804b0u,CarBytes=0x10f0u;
    struct Mirror{std::uint32_t base,end;std::vector<std::uint8_t> bytes;};
    std::vector<Mirror> mirrors{{Player,Player+CarBytes,{}},{0x799d18u,0x799d1cu,{}},{0x79f574u,0x79f578u,{}},{Cam,Cam+0x400u,{}},
        {0x82e7c0u,0x82f0f0u,{}},{0x651758u,0x653788u,{}},{0x7d2da0u,0x7d2de0u,{}},{0x7d3190u,0x7d31d0u,{}},{0x780110u,0x780120u,{}},
        {Poly,Poly+0x2000u,{}},{0x78026cu,0x780270u,{}},{0x780258u,0x78025cu,{}},{0x799b30u,0x799b30u+410u*0x3cu,{}},{Wheels,Wheels+0x400u,{}}};
    const std::vector<std::uint8_t> scripts(file.data()+pe_file_offset(file,0x651758u),file.data()+pe_file_offset(file,0x651758u)+0x2030);
    static constexpr std::uint8_t MotionScenes[]{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,20,25,31,32,33,34,35};
    std::uint32_t original_op7=0;
    for(unsigned iter=0;iter<count;++iter){
        unsigned entry=iter%5u;
        if(const char* e=std::getenv("OR2_GOAL_CAMERA_ENTRY"))entry=unsigned(std::stoul(e));
        std::memset(reinterpret_cast<void*>(trace),0,0x8000);
        std::vector<std::uint32_t> value(services.size());
        for(auto& v:value)v=rnd(3)?rnd(3):rng();
        auto setv=[&](std::uint32_t pc,std::uint32_t v){value[index_of.at(pc)]=v;};
        setv(0x4493c0,rnd(3)?0u:1u);setv(0x450240,rnd(9));setv(0x46c3f0,rnd(4)?0u:1u);setv(0x4957f0,rnd(4)?(rng()&0xffffff00u):rng()|1u);
        setv(0x495b10,rnd(3));setv(0x48b310,rnd(4)?(rng()&0xffffff00u):rng()|1u);setv(0x48b320,rnd(0x60));setv(0x495490,rnd(4)?(rng()&0xffffff00u):rng()|1u);
        setv(0x4954f0,rnd(0x60));setv(0x43d920,rnd(0x80));setv(0x4532e0,rnd(3));setv(0x44ff10,rnd(2));
        {const std::uint32_t k[]{0u,1u,1u,2u,rng()};setv(0x43eb60,k[rnd(5)]);}
        // globals
        for(auto& mi:mirrors)for(std::uint32_t a=mi.base;a<mi.end;a+=4)p32(a,rnd(3)?rnd(4):rng());
        std::memcpy(reinterpret_cast<void*>(0x651758u),scripts.data(),scripts.size());
        p32(0x799d18u,Player);p32(0x79f574u,Cam);
        for(unsigned k=0;k<4;++k)p32(0x780110u+k*4,Poly);
        for(std::uint32_t a=Poly;a<Poly+0x2000u;a+=4)pf(a,real(-500,500));
        for(std::uint32_t a=Player;a<Player+CarBytes;a+=4)pf(a,real(-50,50));
        *reinterpret_cast<std::uint8_t*>(Player+0x11)=std::uint8_t(rnd(30));p32(Player+0x5c,rnd(4));p32(Player+4,rng());
        for(unsigned k=0;k<0x400;k+=4)pf(Cam+k,real(-10,10));
        for(std::uint32_t mat:{0x7d2da0u,0x7d3190u})for(unsigned k=0;k<16;++k)pf(mat+k*4,k==15?1.f:(k>=12?real(-900,900):(k%5==0?real(0.6f,1.4f):real(-0.6f,0.6f))));
        for(std::uint32_t a=0x82e7f0u;a<0x82f0f0u;a+=4)pf(a,real(-3,3));
        for(unsigned i=0;i<4;++i)p32(0x82ea38u+i*4,Wheels+i*0x100u);
        p32(0x78026cu,rnd(3)?0x10u:rnd(0x25));p32(0x780258u,rnd(10));
        for(std::uint32_t a=0x8572ecu;a<0x8573bcu;a+=4)p32(a,0);
        // override block 82E7C0
        const std::uint8_t scene=entry==1||entry==2?MotionScenes[rnd(sizeof MotionScenes)]:std::uint8_t(rnd(36));
        *reinterpret_cast<std::uint8_t*>(0x82e7d4u)=scene;
        p32(0x82e7d8u,entry==2&&rnd(4)==0?0u:1u);p32(0x82e7d0u,rnd(3)?1u:rnd(3));
        const float clock=real(0,650);pf(0x82e7e0u,clock);pf(0x82e7e8u,rnd(8)?real(0,11):real(20,40));
        p16(0x82e7c4u,std::uint16_t(rnd(8)?std::int16_t(rnd(700)):std::int16_t(-1)));
        // scripts: authored (flags +28 random) or random records over all opcodes
        {   std::uint32_t rec=g32(0x653788u+scene*4u);const bool synth=rnd(2)!=0;
            for(;*reinterpret_cast<std::int16_t*>(rec+0x24)>=0;rec+=0x2c){
                p32(rec+0x28,rnd(3)?1u:0u);
                if(!synth)continue;
                const std::uint32_t op=rnd(4)?rnd(0x1f):rnd(0x40);
                p32(rec,op);
                for(unsigned k=1;k<5;++k)p32(rec+k*4,rnd(3)?rnd(3):rng());
                pf(rec+0x14,real(-3000,3000));pf(rec+0x18,real(-3000,3000));
                if(op==13u)p32(rec+4,rnd(410));
                if(op==3u&&rnd(2))p32(rec+4,3);
                if(op==5u&&rnd(2))p32(rec+4,0x16a);
                p16(rec+0x24,std::uint16_t(rnd(700)));p32(rec+0x28,rnd(4)?1u:0u);
            }
        }
        for(auto& mi:mirrors){mi.bytes.resize(mi.end-mi.base);std::memcpy(mi.bytes.data(),reinterpret_cast<void*>(mi.base),mi.bytes.size());}
        std::memcpy(reinterpret_cast<void*>(values),value.data(),value.size()*4);
        reset_matrix_oracle_globals();
        std::vector<std::uint8_t> arena(0x800);outrun::driving::Bytes ar(arena.data(),arena.size());
        for(unsigned k=0;k<16;++k)ar.putf(k*4,k%5==0?1.f:real(-0.5f,0.5f));
        std::memcpy(reinterpret_cast<void*>(MatrixArena),arena.data(),arena.size());
        // original
        static constexpr std::uint32_t Pc[]{0x486730u,0x486ef0u,0x4874a0u,0x475720u,0x4755c0u};
        prepare(Pc[entry]);reinterpret_cast<unsigned*>(S)[0]=Player;
        if(entry==4)guest_call.eax=Player;
        run();const std::uint32_t guest_eax=guest_call.out_eax;
        // native
        PcRaceMemory mem;std::vector<std::vector<std::uint8_t>> native(mirrors.size());
        race_end_map_tables(mem);goal_camera_map_tables(mem);race_robots_map_tables(mem);
        for(std::size_t i=0;i<mirrors.size();++i){native[i]=mirrors[i].bytes;mem.map(mirrors[i].base,native[i].data(),native[i].size());}
        outrun::driving::PcMatrixStack stack{ar,0,0,64};
        struct NCall{std::uint32_t pc,ecx;decltype(PcRaceCall::args) a;};std::vector<NCall> calls;
        PcRaceContext ctx{mem,stack,[&](const PcRaceCall& k)->std::uint32_t{
            const auto it=index_of.find(k.pc);if(it==index_of.end()){std::ostringstream s;s<<"goal camera probe: unexpected service "<<std::hex<<k.pc;throw std::runtime_error(s.str());}
            calls.push_back({k.pc,k.ecx,k.args});
            const auto& e=services[it->second];const std::uint32_t v=value[it->second];
            if(e.kind==4)mem.put32(k.args[4],v);
            if(e.kind==5){mem.put32(k.args[0],v);mem.put32(k.args[1],v);}
            return e.kind==2?0u:v;},nullptr};
        std::uint32_t native_eax=0;bool ok=true;std::string why;
        try{
            switch(entry){
            case 0:goal_script_486730(ctx);break;
            case 1:native_eax=goal_motion_486ef0(ctx);break;
            case 2:camera_override_control_4874a0(ctx);break;
            case 3:pas_pl_car_475720(ctx,Player);break;
            default:pas_save_4755c0(mem,Player);break;
            }
        }catch(const std::exception& e){ok=false;why=e.what();}
        const std::string tag="entry "+std::to_string(entry)+" run "+std::to_string(iter)+" scene "+std::to_string(scene);
        ++cases;if(!ok){fail(tag+" native threw: "+why);continue;}
        if(inject&&iter==12)native[0][0x2d8]^=1;
        for(std::size_t i=0;i<mirrors.size();++i){std::ostringstream s;s<<tag<<" region "<<std::hex<<mirrors[i].base;
            compare(native[i].data(),reinterpret_cast<void*>(mirrors[i].base),native[i].size(),s.str());}
        compare(arena.data(),reinterpret_cast<void*>(MatrixArena),arena.size(),tag+" matrix arena");
        {std::array<std::uint32_t,2> sp{MatrixArena+std::uint32_t(stack.current_offset),std::uint32_t(stack.depth)};
         compare(sp.data(),reinterpret_cast<void*>(0x89b564),8,tag+" matrix pointer/depth");}
        ++cases;if(entry==1&&native_eax!=guest_eax){std::ostringstream s;s<<tag<<" EAX PC="<<guest_eax<<" native="<<native_eax;fail(s.str());}
        const unsigned n=g32(trace);++cases;
        if(n!=calls.size()){std::ostringstream s;s<<tag<<" service count PC="<<n<<" native="<<calls.size();
            for(unsigned i=0;i<n&&i<40;++i)s<<" "<<std::hex<<g32(trace+64+i*0x30);s<<" | native";for(auto& cl:calls)s<<" "<<std::hex<<cl.pc;fail(s.str());}
        else for(unsigned i=0;i<n;++i){const auto* g=reinterpret_cast<const std::uint32_t*>(trace+64+i*0x30);
            const auto& e=services[index_of[g[0]]];bool same=g[0]==calls[i].pc;
            for(unsigned k=0;k<e.argc;++k){
                // stack locals passed by address (43D920, 43EB60): their contents are compared instead
                if((g[0]==0x43d920u&&k<3)||(g[0]==0x43eb60u&&(k==1||k==4)))continue;
                same=same&&g[2+k]==calls[i].a[k];
            }
            if(!same){std::ostringstream s;s<<tag<<" service #"<<i<<" "<<std::hex<<g[0];for(unsigned k=0;k<e.argc;++k)s<<" PC "<<g[2+k]<<"/native "<<calls[i].a[k];fail(s.str());break;}}
        ++coverage["entry "+std::to_string(entry)];
        for(const auto& cl:calls)if(cl.pc==0x43d920u)++coverage["op 7 network placement"];
        for(const auto& cl:calls)if(cl.pc==0x43d390u)++coverage["475720 wheel on road"];
        if(entry==1)++coverage[guest_eax?"486EF0 sampled":"486EF0 none"];
        (void)original_op7;
    }
    std::cout<<"{\"routine\":\""<<only<<"\",\"coverage\":{";bool first=true;
    for(const auto& [k,v]:coverage){std::cout<<(first?"":",")<<"\""<<k<<"\":"<<v;first=false;}
    std::cout<<"},\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}"<<std::endl;
    return mismatches?1:0;
}

// Included inside the compare_pc dispatcher; uses its private x86 harness.
if(only=="ghost_draw_probe"){
    // Other-car / ghost-car display (race_ghost_car.cpp) against the original:
    //   0 4AE5F0 display (event flags 79FB48, 7D39F0, 7C24C9, +B62/+B64/+B53
    //     blink counters, the 0x40 ghost gate, the 4AE0F0 model tree for
    //     every LOD 0..4 and all 30 car models, 842038/84203C/842040 alpha)
    //   1 4ADAC0 shadow (4AD380)   2 4AE0F0 direct (+4 bit 2 fade path)
    // Draw leaves (405360, 4056D0, 4044F0, 4044A0, 4044C0, 4044E0, 4052B0,
    // 4052C0, 404540, 405350) are capture stubs recording {pc, 6 stack args,
    // current matrix [89B564]}; the native side records PcVehicleDrawCall.
    // Services 46F990, 49EED0, 4957F0, 495B00, 4962D0, 495860 return
    // scripted values. The matrix leaves run as original code on a matrix
    // arena. Compared: car works, 842038..842044, the draw list, the service
    // log, the matrix arena and 89B564/89B568.
    using namespace outrun::platform;
    if(argc<=7||std::string(argv[7])=="-")throw std::runtime_error("ghost draw probe requires the pinned d3dx9_29.dll (D3DX rotations)");
    init_matrix_oracle_globals();wire_d3dx_imports(map_d3dx29(argv[7]));
    // 40A360 uses D3DXMatrixScaling: its generic body follows the export's dispatch jump.
    if(mprotect(reinterpret_cast<void*>(0x596000u),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("IAT");
    *reinterpret_cast<std::uint32_t*>(0x5962b4u)=D3DX_BASE+(0x441288u-0x400000u);mprotect(reinterpret_cast<void*>(0x596000u),4096,PROT_READ);
    constexpr unsigned base=0x3bc00000u,stubs=base,strace=base+0x4000u,dtrace=base+0x8000u,values=base+0x2c000u;
    map_at(base,0x40000,PROT_READ|PROT_WRITE|PROT_EXEC);map_original_arithmetic_bridge(file);
    {   unsigned restored=0;const std::size_t text_file=pe_file_offset(file,0x401000u);
        for(std::uint32_t a=0x401000u;a<0x401000u+0x194881u;a+=0x100u){
            const std::size_t n=std::min<std::size_t>(0x100u,0x401000u+0x194881u-a);
            if(std::memcmp(reinterpret_cast<void*>(a),file.data()+text_file+(a-0x401000u),n)){r060_raw_write(a,file.data()+text_file+(a-0x401000u),n);++restored;}
        }
        std::cerr<<"restored "<<restored<<" patched .text blocks"<<char(10);
    }
    for(std::uint32_t page=0x62f000u;page<0x98d000u;page+=0x1000u)mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE);
    auto emit32=[](std::vector<std::uint8_t>& b,std::uint32_t v){for(unsigned k=0;k<4;++k)b.push_back(std::uint8_t(v>>(k*8)));};
    unsigned stub_off=0;
    auto install=[&](std::uint32_t pc,const std::vector<std::uint8_t>& c){
        std::memcpy(reinterpret_cast<void*>(stubs+stub_off),c.data(),c.size());
        r046_write_rel32(pc,0xe9,stubs+stub_off);stub_off=(stub_off+unsigned(c.size())+15u)&~15u;};
    const std::vector<std::uint32_t> leaves{0x405360,0x4056d0,0x4044f0,0x4044a0,0x4044c0,0x4044e0,0x4052b0,0x4052c0,0x404540,0x405350};
    for(auto pc:leaves){   // [dtrace] count; entries 0x80: pc, args[6], matrix 64 at +0x20
        std::vector<std::uint8_t> c{0x56,0x57,0x51,0x8b,0x3d};emit32(c,dtrace);
        c.insert(c.end(),{0xc1,0xe7,0x07,0x81,0xc7});emit32(c,dtrace+0x80);
        c.insert(c.end(),{0xc7,0x07});emit32(c,pc);
        for(unsigned i=0;i<6;++i)c.insert(c.end(),{0x8b,0x44,0x24,std::uint8_t(0x10+4*i),0x89,0x47,std::uint8_t(4+4*i)});
        c.insert(c.end(),{0x8b,0x35});emit32(c,0x89b564u);                                   // mov esi,[89B564]
        c.insert(c.end(),{0x83,0xc7,0x20,0xb9,0x40,0,0,0,0xf3,0xa4});                        // edi+=20; rep movsb 64
        c.insert(c.end(),{0xff,0x05});emit32(c,dtrace);
        c.insert(c.end(),{0x59,0x5f,0x5e,0x31,0xc0,0xc3});
        install(pc,c);
    }
    const std::vector<std::uint32_t> services{0x46f990,0x49eed0,0x4957f0,0x495b00,0x4962d0,0x495860};
    std::map<std::uint32_t,unsigned> svc_index;
    for(std::size_t k=0;k<services.size();++k){svc_index[services[k]]=unsigned(k);
        std::vector<std::uint8_t> c{0x57,0x8b,0x3d};emit32(c,strace);
        c.insert(c.end(),{0xc1,0xe7,0x04,0x81,0xc7});emit32(c,strace+64);
        c.insert(c.end(),{0xc7,0x07});emit32(c,services[k]);
        for(unsigned i=0;i<2;++i)c.insert(c.end(),{0x8b,0x44,0x24,std::uint8_t(8+4*i),0x89,0x47,std::uint8_t(4+4*i)});
        c.insert(c.end(),{0xff,0x05});emit32(c,strace);c.push_back(0x5f);
        c.push_back(0xa1);emit32(c,values+4*unsigned(k));c.push_back(0xc3);
        install(services[k],c);
    }
    std::mt19937 rng(0x4ae5f000u);
    auto rnd=[&](std::uint32_t n){return n?std::uint32_t(rng()%n):0u;};
    auto real=[&](float a,float b){return a+(b-a)*float(rng()%100000)/100000.f;};
    auto g32=[](std::uint32_t a){return *reinterpret_cast<std::uint32_t*>(a);};
    auto p32=[](std::uint32_t a,std::uint32_t v){*reinterpret_cast<std::uint32_t*>(a)=v;};
    auto pf=[](std::uint32_t a,float v){std::memcpy(reinterpret_cast<void*>(a),&v,4);};
    auto p16=[](std::uint32_t a,std::uint16_t v){std::memcpy(reinterpret_cast<void*>(a),&v,2);};
    unsigned cases{},mismatches{};const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
    std::map<std::string,unsigned> coverage;
    auto fail=[&](const std::string& what){++mismatches;if(mismatches<20)std::cerr<<what<<"\n";};
    auto compare=[&](const void* native,const void* original,std::size_t n,const std::string& what){
        ++cases;if(!std::memcmp(native,original,n))return;
        const auto* a=static_cast<const std::uint8_t*>(native);const auto* b=static_cast<const std::uint8_t*>(original);
        for(std::size_t i=0;i<n;++i)if(a[i]!=b[i]){std::ostringstream s;s<<what<<" offset=0x"<<std::hex<<i<<" PC="<<unsigned(b[i])<<" native="<<unsigned(a[i]);fail(s.str());break;}
    };
    constexpr std::uint32_t Car=0x7815a0u,CarBytes=0x10f0u,Player=0x7804b0u;
    struct Mirror{std::uint32_t base,end;std::vector<std::uint8_t> bytes;};
    std::vector<Mirror> mirrors{{Car,Car+CarBytes,{}},{Player,Player+CarBytes,{}},{0x842038u,0x842044u,{}},{0x78026cu,0x780270u,{}},
        {0x780258u,0x78025cu,{}},{0x799d18u,0x799d1cu,{}},{0x7d39f0u,0x7d39f4u,{}},{0x7c24c8u,0x7c24ccu,{}},{0x79fb48u,0x79fd48u,{}},
        {0x7f94c0u,0x7f94c4u,{}}};
    auto fill_matrix=[&](std::uint32_t a){for(unsigned k=0;k<16;++k)pf(a+k*4,k==15?1.f:(k>=12?real(-300,300):real(-1,1)));};
    bool injected=false;
    for(unsigned iter=0;iter<count;++iter){
        unsigned entry=iter%3u;
        if(const char* e=std::getenv("OR2_GHOST_DRAW_ENTRY"))entry=unsigned(std::stoul(e));
        std::memset(reinterpret_cast<void*>(strace),0,0x4000);p32(dtrace,0);
        std::array<std::uint32_t,6> value{};value[0]=rnd(3)?rnd(0x40)-3u:rng();value[1]=rnd(3)?4u:rnd(6);value[2]=rnd(2);value[3]=rnd(2);value[4]=rnd(2);value[5]=rnd(6);
        std::memcpy(reinterpret_cast<void*>(values),value.data(),sizeof value);
        for(std::uint32_t a=Car;a<Car+CarBytes;a+=4)p32(a,rnd(4)?0u:rng());
        const unsigned model=rnd(30);
        p32(Car,9+rnd(4));*reinterpret_cast<std::uint8_t*>(Car+0x11)=std::uint8_t(model);*reinterpret_cast<std::uint8_t*>(Car+0x12)=std::uint8_t(rnd(8));
        p32(Car+4,(rnd(3)?0xc0u:rng()&0xffu)|(rnd(4)?0u:0x100u)|(rnd(4)?0u:0x200u)|(rnd(8)?0u:0x20u)|(rnd(6)?0u:4u));
        p32(Car+8,rnd(8)?0u:1u);
        p32(Car+0xc5c,(rnd(4)?0x2000u:0u)|(rnd(3)?0u:0x200u)|(rnd(3)?0u:0x400u)|(rnd(10)?0u:0x80000u));
        *reinterpret_cast<std::uint8_t*>(Car+0x328)=std::uint8_t(rnd(8)?rnd(5):rnd(0x100));
        p16(Car+0xb50,std::uint16_t(rng()));p16(Car+0xb62,std::uint16_t(rnd(3)?0:rnd(4)));p16(Car+0xb64,std::uint16_t(rnd(3)?0:rnd(4)));
        *reinterpret_cast<std::uint8_t*>(Car+0xb53)=std::uint8_t(rnd(2));*reinterpret_cast<std::uint8_t*>(Car+0xb6c)=std::uint8_t(rnd(2));
        p32(Car+0x2f0,rnd(4));p32(Car+0x2f4,rnd(60));p32(Car+0x38,rnd(3)?0u:rnd(3)-1u);
        *reinterpret_cast<std::uint8_t*>(Car+0xc35)=std::uint8_t(rnd(3)?0xffu:rnd(10));
        pf(Car+0x2c8,rnd(3)?real(0,50):real(-1,0));pf(Car+0x2dc,real(-1,2));pf(Car+0xb68,rnd(4)?real(0.2f,0.8f):real(0.9f,1.2f));
        for(unsigned w:{0x32u,0x40u,0x44u})p16(Car+w,std::uint16_t(rng()));
        p16(Car+0x260,std::uint16_t(rnd(40)));
        fill_matrix(Car+0xb0);fill_matrix(Car+0xf0);fill_matrix(Car+0xc90);
        for(std::uint32_t a=Player;a<Player+CarBytes;a+=4)p32(a,0);p16(Player+0x260,std::uint16_t(rnd(40)));
        p32(0x799d18u,Player);
        p32(0x78026cu,rnd(5)?(rnd(2)?0x10u:0x12u):(rnd(2)?3u:rnd(0x20)));
        p32(0x780258u,rnd(4)?7u:rnd(10));
        p32(0x7d39f0u,rnd(8)?0u:4u);*reinterpret_cast<std::uint8_t*>(0x7c24c9u)=std::uint8_t(rnd(10)?0:1);
        for(std::uint32_t a=0x79fb48u;a<0x79fd48u;++a)*reinterpret_cast<std::uint8_t*>(a)=std::uint8_t(rnd(3)?0:0x10);
        p32(0x7f94c0u,0);
        p32(0x842038u,rng());p32(0x84203cu,rng());p32(0x842040u,rng());
        for(auto& mi:mirrors){mi.bytes.resize(mi.end-mi.base);std::memcpy(mi.bytes.data(),reinterpret_cast<void*>(mi.base),mi.bytes.size());}
        reset_matrix_oracle_globals();
        std::vector<std::uint8_t> arena(0x800);outrun::driving::Bytes ar(arena.data(),arena.size());
        for(unsigned k=0;k<16;++k)ar.putf(k*4,k%5==0?1.f:real(-0.5f,0.5f));
        std::memcpy(reinterpret_cast<void*>(MatrixArena),arena.data(),arena.size());
        switch(entry){
        case 0:prepare(0x4ae5f0u);reinterpret_cast<unsigned*>(S)[0]=Car;break;
        case 1:prepare(0x4adac0u);reinterpret_cast<unsigned*>(S)[0]=Car;break;
        default:prepare(0x4ae0f0u);guest_call.esi=Car;break;
        }
        run();
        PcRaceMemory mem;std::vector<std::vector<std::uint8_t>> native(mirrors.size());
        for(std::size_t i=0;i<mirrors.size();++i){native[i]=mirrors[i].bytes;mem.map(mirrors[i].base,native[i].data(),native[i].size());}
        ghost_map_car_tables(mem);
        outrun::driving::PcMatrixStack stack{ar,0,0,64};
        struct NCall{std::uint32_t pc;decltype(PcRaceCall::args) a;};std::vector<NCall> calls;std::vector<PcVehicleDrawCall> draws;
        PcRaceContext ctx{mem,stack,[&](const PcRaceCall& k)->std::uint32_t{
            const auto it=svc_index.find(k.pc);if(it==svc_index.end())throw std::runtime_error("ghost draw probe: unexpected service");
            calls.push_back({k.pc,k.args});return value[it->second];},&draws};
        bool ok=true;std::string why;
        try{
            switch(entry){
            case 0:ghost_car_display_4ae5f0(ctx,Car);break;
            case 1:ghost_car_shadow_4adac0(ctx,Car);break;
            default:other_car_draw_4ae0f0(ctx,Car);break;
            }
        }catch(const std::exception& e){ok=false;why=e.what();}
        const std::string tag="entry "+std::to_string(entry)+" run "+std::to_string(iter)+" model "+std::to_string(model);
        if(!ok&&why.find("Heart Attack +C5C 0x80000")!=std::string::npos&&g32(0x780258u)==2u&&(g32(Car+0xc5c)&0x80000u)){++coverage["latched 4AE0F0 Heart Attack 0x80000"];continue;}
        ++cases;if(!ok){fail(tag+" native threw: "+why);continue;}
        if(inject&&!injected&&!draws.empty()){draws[0].matrix[0]^=1;injected=true;}
        for(std::size_t i=0;i<mirrors.size();++i){std::ostringstream s;s<<tag<<" region "<<std::hex<<mirrors[i].base;
            compare(native[i].data(),reinterpret_cast<void*>(mirrors[i].base),native[i].size(),s.str());}
        compare(arena.data(),reinterpret_cast<void*>(MatrixArena),arena.size(),tag+" matrix arena");
        {std::array<std::uint32_t,2> sp{MatrixArena+std::uint32_t(stack.current_offset),std::uint32_t(stack.depth)};
         compare(sp.data(),reinterpret_cast<void*>(0x89b564),8,tag+" matrix pointer/depth");}
        const unsigned n=g32(dtrace);++cases;
        if(n!=draws.size()){fail(tag+" draw count PC="+std::to_string(n)+" native="+std::to_string(draws.size()));continue;}
        for(unsigned i=0;i<n;++i){
            const auto* g=reinterpret_cast<const std::uint32_t*>(dtrace+0x80+i*0x80);const auto& d=draws[i];
            bool same=g[0]==d.pc;for(unsigned k=0;k<d.argc;++k)same=same&&g[1+k]==d.args[k];
            same=same&&!std::memcmp(g+8,d.matrix.data(),64);
            ++cases;if(!same){std::ostringstream s;s<<tag<<" draw #"<<i<<" PC "<<std::hex<<g[0]<<"("<<g[1]<<","<<g[2]<<","<<g[3]<<") native "<<d.pc<<"("<<d.args[0]<<","<<d.args[1]<<","<<d.args[2]<<")"
                <<(std::memcmp(g+8,d.matrix.data(),64)?" matrix differs":"");fail(s.str());break;}
        }
        const unsigned sn=g32(strace);++cases;
        if(sn!=calls.size())fail(tag+" service count PC="+std::to_string(sn)+" native="+std::to_string(calls.size()));
        else for(unsigned i=0;i<sn;++i){const auto* g=reinterpret_cast<const std::uint32_t*>(strace+64+i*16);
            bool same=g[0]==calls[i].pc;if(g[0]==0x46f990u)same=same&&g[1]==calls[i].a[0]&&g[2]==calls[i].a[1];
            if(!same){fail(tag+" service #"+std::to_string(i));break;}}
        ++coverage["entry "+std::to_string(entry)];
        if(n){const int l=int(std::int8_t(mirrors[0].bytes[0x328]));++coverage[l>=0&&l<=4?"drew lod "+std::to_string(l):std::string("drew lod other")];}
    }
    std::cout<<"{\"routine\":\""<<only<<"\",\"coverage\":{";bool first=true;
    for(const auto& [k,v]:coverage){std::cout<<(first?"":",")<<"\""<<k<<"\":"<<v;first=false;}
    std::cout<<"},\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}"<<std::endl;
    return mismatches?1:0;
}

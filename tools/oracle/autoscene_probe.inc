// Included inside the compare_pc dispatcher; uses its private x86 harness.
if(only=="autoscene_probe"){
    // AUTOSCENE (race_autoscene.cpp) against the original over the 38 retail scene scripts
    // (OR2_AS_DIR: the inflated \AS\as_*_bin.sz files). Entry 0: 4B5150 init. Entry 1: 4B5F60
    // then F frames of 4B6CD0 control + 4B6A30 display, then (sometimes) 4B6690 dest.
    // External PC functions are capture stubs recording {pc, ECX, stack args} (405360: the
    // current matrix; 448CD0: its name) and returning scripted values; 580253 is a bump
    // allocator, 44FC60 writes the script pointer / block, 4AF520 pushes 0.0. The module, the
    // matrix stack (D3DX), 513650, 581000 and 51B740's measured result (stubbed 0) run as
    // original code. Compared: the work, the script copy, the allocations, the car, the robot
    // and event words, 84281C, the matrix arena and the ordered service log.
    using namespace outrun::platform;
    if(argc<=7||std::string(argv[7])=="-")throw std::runtime_error("autoscene probe requires the pinned d3dx9_29.dll");
    wire_d3dx_imports(map_d3dx29(argv[7]));
    const char* dir=std::getenv("OR2_AS_DIR");if(!dir)throw std::runtime_error("autoscene probe: OR2_AS_DIR not set");
    struct Script{std::uint32_t scene;std::vector<std::uint8_t> data;};
    std::vector<Script> scripts;
    for(std::uint32_t scene=0x13;scene<=0x38u;++scene){
        const std::uint32_t row=0x717d68u+scene*12u;const std::uint32_t path=*reinterpret_cast<const std::uint32_t*>(row+4);
        std::string p=reinterpret_cast<const char*>(path);                     // \AS\as_E_01_bin.sz
        std::string base_name=p.substr(p.find_last_of('\\')+1);base_name=base_name.substr(0,base_name.size()-3);
        for(auto& ch:base_name)ch=char(std::tolower(static_cast<unsigned char>(ch)));
        std::ifstream f(std::string(dir)+"/"+base_name,std::ios::binary);
        if(!f)throw std::runtime_error("autoscene probe: missing "+base_name);
        std::vector<std::uint8_t> raw((std::istreambuf_iterator<char>(f)),std::istreambuf_iterator<char>());
        scripts.push_back({scene,std::vector<std::uint8_t>(raw.begin()+4,raw.end())});   // the loader's data skips the size word
    }
    constexpr unsigned base=0x50000000u,stubs=base,values=base+0x7000u,trace=base+0x10000u,TraceMax=0x6000u;
    constexpr unsigned Work=base+0x400000u,Data=base+0x410000u,Heap=base+0x440000u,HeapSize=0x40000u,Bump=base+0x8000u;
    constexpr unsigned Car=base+0x480000u,RobotA=base+0x481000u,RobotB=base+0x482000u,RobotWorks=base+0x483000u,Block=base+0x484000u;
    map_at(base,0x490000,PROT_READ|PROT_WRITE|PROT_EXEC);map_original_arithmetic_bridge(file);
    {   const std::size_t text_file=pe_file_offset(file,0x401000u);
        for(std::uint32_t a=0x401000u;a<0x401000u+0x194881u;a+=0x100u){
            const std::size_t n=std::min<std::size_t>(0x100u,0x401000u+0x194881u-a);
            if(std::memcmp(reinterpret_cast<void*>(a),file.data()+text_file+(a-0x401000u),n))r060_raw_write(a,file.data()+text_file+(a-0x401000u),n);
        }
    }
    for(std::uint32_t page=0x62f000u;page<0x98d000u;page+=0x1000u)mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE);
    init_matrix_oracle_globals();
    auto emit32=[](std::vector<std::uint8_t>& b,std::uint32_t v){for(unsigned k=0;k<4;++k)b.push_back(std::uint8_t(v>>(k*8)));};
    // kind: 0 plain, 2 x87 0.0, 3 thiscall ret argc*4, 4 matrix of [89B564] (405360), 5 name at
    // arg0 (448CD0), 9 44FC60 ([arg1] = value, [arg3] = Block), 10 bump allocator (580253)
    struct Svc{std::uint32_t pc;unsigned argc,kind;};
    const std::vector<Svc> services{
        {0x440a60,2,0},{0x440cd0,1,0},{0x440b20,0,0},{0x440890,0,0},{0x440d10,1,0},{0x440d30,0,0},{0x440d50,1,0},{0x440d70,0,0},
        {0x580253,1,10},{0x44fd80,2,0},{0x44f880,1,0},{0x44fc60,4,9},{0x44fd00,0,0},{0x42ebf0,0,0},{0x4489f0,0,0},{0x51b740,2,0},
        {0x483e10,4,0},{0x4af520,1,2},{0x42e020,3,0},{0x448cd0,1,5},{0x4249f0,1,0},{0x405360,6,4},{0x4208a0,3,0},{0x420560,0,0},
        {0x49a650,1,0},{0x487c30,2,0},{0x487d10,2,0},{0x487d40,2,0},{0x514880,3,0},{0x514830,3,0},{0x514800,3,0},{0x514f60,2,0},
        {0x48f4e0,0,3},{0x4f2280,1,3},{0x4f1d30,1,3},{0x4f2520,0,3},{0x4f1e30,1,3},{0x4ed890,1,3},{0x4f1e60,1,3}};
    std::map<std::uint32_t,unsigned> index_of;unsigned stub_off=0;
    for(std::size_t k=0;k<services.size();++k){
        const auto& e=services[k];index_of[e.pc]=unsigned(k);
        std::vector<std::uint8_t> c{0x57,0x56,0x51,0x8b,0x3d};emit32(c,trace);            // push edi/esi/ecx; mov edi,[count]
        c.insert(c.end(),{0x81,0xff});emit32(c,TraceMax);c.insert(c.end(),{0x72,0x02,0x31,0xff});   // cap: wrap to 0
        c.insert(c.end(),{0x6b,0xff,0x60,0x81,0xc7});emit32(c,trace+64);
        c.insert(c.end(),{0xc7,0x07});emit32(c,e.pc);
        c.insert(c.end(),{0x89,0x4f,0x04});
        for(unsigned i=0;i<e.argc;++i)c.insert(c.end(),{0x8b,0x44,0x24,std::uint8_t(0x10+4*i),0x89,0x47,std::uint8_t(0x08+4*i)});
        if(e.kind==4)c.insert(c.end(),{0x8b,0x35,0x64,0xb5,0x89,0x00,0x83,0xc7,0x20,0xb9,0x10,0,0,0,0xf3,0xa5});   // esi=[89B564]; 64 bytes
        if(e.kind==5)c.insert(c.end(),{0x8b,0x74,0x24,0x10,0x83,0xc7,0x20,0xb9,0x10,0,0,0,0xf3,0xa5});           // the name at arg0
        c.insert(c.end(),{0xff,0x05});emit32(c,trace);
        c.push_back(0xa1);emit32(c,values+4*unsigned(k));
        if(e.kind==2)c.insert(c.end(),{0xd9,0xee});
        if(e.kind==9){c.insert(c.end(),{0x8b,0x74,0x24,0x14,0x89,0x06,0x8b,0x74,0x24,0x1c,0xc7,0x06});emit32(c,Block);}
        if(e.kind==10){c.push_back(0xa1);emit32(c,Bump);                                   // eax=[Bump]
            c.insert(c.end(),{0x8b,0x4c,0x24,0x10,0x83,0xc1,0x0f,0x83,0xe1,0xf0,0x01,0x0d});emit32(c,Bump);}   // [Bump] += (arg+15)&~15
        c.insert(c.end(),{0x59,0x5e,0x5f});
        if(e.kind==3)c.insert(c.end(),{0xc2,std::uint8_t(e.argc*4u),0x00});else c.push_back(0xc3);
        std::memcpy(reinterpret_cast<void*>(stubs+stub_off),c.data(),c.size());
        r046_write_rel32(e.pc,0xe9,stubs+stub_off);stub_off=(stub_off+unsigned(c.size())+15u)&~15u;
    }
    std::mt19937 rng(0x4b515000u);
    auto rnd=[&](std::uint32_t n){return n?std::uint32_t(rng()%n):0u;};
    auto real=[&](float a,float b){return a+(b-a)*float(rng()%100000)/100000.f;};
    auto g32=[](std::uint32_t a){return *reinterpret_cast<std::uint32_t*>(a);};
    auto p32=[](std::uint32_t a,std::uint32_t v){*reinterpret_cast<std::uint32_t*>(a)=v;};
    unsigned cases{},mismatches{};const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
    std::map<std::string,unsigned> coverage;
    auto fail=[&](const std::string& what){++mismatches;if(mismatches<20)std::cerr<<what<<"\n";};
    auto compare=[&](const void* native,const void* original,std::size_t n,const std::string& what){
        ++cases;if(!std::memcmp(native,original,n))return;
        const auto* a=static_cast<const std::uint8_t*>(native);const auto* b=static_cast<const std::uint8_t*>(original);
        for(std::size_t i=0;i<n;++i)if(a[i]!=b[i]){std::ostringstream s;s<<what<<" offset=0x"<<std::hex<<i<<" PC="<<unsigned(b[i])<<" native="<<unsigned(a[i]);fail(s.str());break;}
    };
    struct Mirror{std::uint32_t base,end;std::vector<std::uint8_t> bytes;};
    // the event records of the robot slots 0x16A..0x17F and 0x19A (440890 results)
    std::vector<Mirror> mirrors{{Work,Work+0x124u,{}},{Data,Data+0x30000u,{}},{Heap,Heap+HeapSize,{}},{Bump,Bump+4u,{}},{Car,Car+0x400u,{}},
        {RobotA,RobotA+0x40u,{}},{RobotB,RobotB+0x40u,{}},{RobotWorks,RobotWorks+0x400u,{}},
        {0x799b30u+0x16au*0x3cu,0x799b30u+0x180u*0x3cu,{}},{0x799b30u+0x19au*0x3cu,0x799b30u+0x19bu*0x3cu,{}},
        {0x799ca0u,0x799ca4u,{}},{0x79f010u,0x79f014u,{}},{0x79f04cu,0x79f050u,{}},{0x799d18u,0x799d1cu,{}},{0x84281cu,0x842820u,{}},
        {0x7d3a7cu,0x7d3a80u,{}},{0x78024cu,0x780250u,{}},{0x79fb4eu,0x79fb4fu,{}}};
    for(unsigned iter=0;iter<count;++iter){
        const unsigned entry=std::getenv("OR2_AS_ENTRY")?unsigned(std::stoul(std::getenv("OR2_AS_ENTRY"))):(iter%8u==0u?0u:1u);
        const Script& sc=scripts[rnd(std::uint32_t(scripts.size()))];
        if(sc.data.size()>0x30000u)throw std::runtime_error("autoscene probe: script larger than its region");
        std::memset(reinterpret_cast<void*>(trace),0,64+std::size_t(TraceMax)*0x60u);
        std::vector<std::uint32_t> value(services.size());
        for(auto& v:value)v=rnd(3)?rnd(3):rng();
        auto setv=[&](std::uint32_t pc,std::uint32_t v){value[index_of.at(pc)]=v;};
        setv(0x440a60,Work);setv(0x51b740,0);setv(0x44fd80,0x1234u);setv(0x44f880,rnd(3)?1u:0u);setv(0x44fc60,Data);
        setv(0x440890,0x16au+rnd(0x15));   // a free slot (0x19A, none free, leaves the record without a work: its motion events then crash the PC too)
        setv(0x448cd0,rnd(4)?0x10000u*(1u+rnd(0x40))+rnd(0x100):0xffffffffu);setv(0x48f4e0,rnd(3));
        std::memcpy(reinterpret_cast<void*>(values),value.data(),value.size()*4);
        // memory
        for(auto& mi:mirrors)for(std::uint32_t a=mi.base;a<mi.end;a+=4)if(mi.end-a>=4)p32(a,rnd(3)?rnd(4):rng());else *reinterpret_cast<std::uint8_t*>(a)=std::uint8_t(rng());
        std::memset(reinterpret_cast<void*>(Data),0,0x30000u);std::memcpy(reinterpret_cast<void*>(Data),sc.data.data(),sc.data.size());
        p32(Bump,Heap);
        p32(0x799ca0u,Work);p32(0x799d18u,Car);p32(0x79f010u,RobotA);p32(0x79f04cu,RobotB);
        p32(RobotA+8u,rnd(2)?0x10u:rnd(0x20));p32(RobotB+8u,rnd(2)?0x10u:rnd(0x20));
        for(std::uint32_t id=0x16au;id<0x180u;++id)p32(0x799b38u+id*0x3cu,RobotWorks+((id-0x16au)&7u)*0x80u);
        for(std::uint32_t k=0;k<8u;++k){p32(RobotWorks+k*0x80u,rnd(8));p32(RobotWorks+k*0x80u+4u,rng());p32(RobotWorks+k*0x80u+8u,rnd(3)?0x10u:(rnd(2)?0xfu:rnd(0x20)));}
        p32(0x7d3a7cu,rnd(5));p32(0x78024cu,rnd(4));*reinterpret_cast<std::uint8_t*>(0x79fb4eu)=std::uint8_t(rnd(4));
        // the work: free (entry 0 / 1 start it)
        for(std::uint32_t a=0;a<0x124u;a+=4)p32(Work+a,0);
        const unsigned frames=entry==1u?1u+rnd(rnd(4)?120u:1200u):0u;const bool dest=entry==1u&&rnd(4)==0u;
        for(auto& mi:mirrors){mi.bytes.resize(mi.end-mi.base);std::memcpy(mi.bytes.data(),reinterpret_cast<void*>(mi.base),mi.bytes.size());}
        reset_matrix_oracle_globals();
        std::vector<std::uint8_t> arena(0x10000);std::memcpy(arena.data(),reinterpret_cast<void*>(MatrixArena),arena.size());
        // original
        if(entry==0){prepare(0x4b5150u);reinterpret_cast<unsigned*>(S)[0]=0;run();}
        else{
            prepare(0x4b5f60u);reinterpret_cast<unsigned*>(S)[0]=sc.scene;run();
            for(unsigned f=0;f<frames;++f){
                prepare(0x4b6cd0u);reinterpret_cast<unsigned*>(S)[0]=Work;run();
                prepare(0x4b6a30u);reinterpret_cast<unsigned*>(S)[0]=Work;run();
            }
            if(dest){prepare(0x4b6690u);reinterpret_cast<unsigned*>(S)[0]=Work;run();}
        }
        // native
        PcRaceMemory mem;std::vector<std::vector<std::uint8_t>> native(mirrors.size());
        for(std::size_t i=0;i<EmbeddedExeRangeCount;++i)mem.map_const(EmbeddedExeRanges[i].base,EmbeddedExeRanges[i].data,EmbeddedExeRanges[i].size);
        for(std::size_t i=0;i<mirrors.size();++i){native[i]=mirrors[i].bytes;mem.map(mirrors[i].base,native[i].data(),native[i].size());}
        outrun::driving::PcMatrixStack stack{outrun::driving::Bytes(arena.data(),arena.size()),0,0,64};
        std::uint32_t bump=Heap;
        struct NCall{std::uint32_t pc,ecx;decltype(PcRaceCall::args) a;std::array<std::uint8_t,64> extra;};std::vector<NCall> calls;
        std::vector<PcVehicleDrawCall> draws;
        PcRaceContext ctx{mem,stack,[&](const PcRaceCall& k)->std::uint32_t{
            const auto it=index_of.find(k.pc);if(it==index_of.end()){std::ostringstream s;s<<"autoscene probe: unexpected service "<<std::hex<<k.pc;throw std::runtime_error(s.str());}
            NCall n{k.pc,k.ecx,k.args,{}};
            const auto kind=services[it->second].kind;
            if(kind==5)for(unsigned b=0;b<64;++b)n.extra[b]=mem.mapped(k.args[0]+b,1)?mem.u8(k.args[0]+b):0u;
            calls.push_back(n);
            if(kind==9){mem.put32(k.args[1],value[it->second]);mem.put32(k.args[3],Block);}
            if(kind==10){const std::uint32_t r=bump;bump+=(k.args[0]+15u)&~15u;mem.put32(Bump,bump);return r;}
            return value[it->second];},&draws};
        bool ok=true;std::string why;
        try{
            if(entry==0)autoscene_init_4b5150(ctx);
            else{
                autoscene_start_4b5f60(ctx,sc.scene);
                for(unsigned f=0;f<frames;++f){
                    const std::size_t before=draws.size();
                    autoscene_control_4b6cd0(ctx,Work);autoscene_display_4b6a30(ctx,Work);
                    // the draws are leaves of the PC log, in place
                    for(std::size_t d=before;d<draws.size();++d){NCall n{0x405360u,0,{},{}};for(unsigned a=0;a<6;++a)n.a[a]=draws[d].args[a];std::memcpy(n.extra.data(),draws[d].matrix.data(),64);calls.push_back(n);}
                }
                if(dest)autoscene_dest_4b6690(ctx,Work);
            }
        }catch(const PcAutosceneUnported& u){ok=false;std::ostringstream s;s<<"unported "<<std::hex<<u.pc;why=s.str();}
        catch(const std::exception& e){ok=false;why=e.what();}
        std::ostringstream tg;tg<<"entry "<<entry<<" scene "<<std::hex<<sc.scene<<std::dec<<" frames "<<frames<<" run "<<iter;const std::string tag=tg.str();
        ++cases;if(!ok){fail(tag+" native threw: "+why);continue;}
        if(inject&&iter==9)native[0][0x10]^=1;
        for(std::size_t i=0;i<mirrors.size();++i){std::ostringstream s;s<<tag<<" region "<<std::hex<<mirrors[i].base;
            compare(native[i].data(),reinterpret_cast<void*>(mirrors[i].base),native[i].size(),s.str());}
        compare(arena.data(),reinterpret_cast<void*>(MatrixArena),arena.size(),tag+" matrix arena");
        // the PC log: draws are logged where they happen (natively appended per frame: compare
        // the service subsequence and the draw subsequence separately)
        const unsigned n=g32(trace);++cases;
        if(n>=TraceMax){++coverage["trace overflow (log not compared)"];continue;}
        std::vector<const std::uint32_t*> pc_calls,pc_draws;
        for(unsigned i=0;i<n;++i){const auto* g=reinterpret_cast<const std::uint32_t*>(trace+64+i*0x60);(g[0]==0x405360u?pc_draws:pc_calls).push_back(g);}
        std::vector<const NCall*> nat_calls,nat_draws;for(auto& cl:calls)(cl.pc==0x405360u?nat_draws:nat_calls).push_back(&cl);
        if(pc_calls.size()!=nat_calls.size()||pc_draws.size()!=nat_draws.size()){std::ostringstream s;s<<tag<<" service count PC="<<pc_calls.size()<<"/"<<pc_draws.size()<<" native="<<nat_calls.size()<<"/"<<nat_draws.size()<<std::hex;
            for(std::size_t i=0;i<pc_calls.size()&&i<30;++i)s<<" "<<pc_calls[i][0];s<<" | native";for(std::size_t i=0;i<nat_calls.size()&&i<30;++i)s<<" "<<nat_calls[i]->pc;fail(s.str());continue;}
        bool same_log=true;
        for(std::size_t i=0;i<pc_calls.size()&&same_log;++i){const auto* g=pc_calls[i];const auto& nc=*nat_calls[i];const auto& e=services[index_of[g[0]]];
            bool same=g[0]==nc.pc;
            if(e.kind==3)same=same&&g[1]==nc.ecx;
            for(unsigned k=0;k<e.argc;++k){
                if(e.kind==5&&k==0)continue;                               // 448CD0: the name, not its address
                if(g[0]==0x44fc60u&&k==2)continue;                          // the size slot on the PC stack
                same=same&&g[2+k]==nc.a[k];
            }
            if(e.kind==5)same=same&&std::string(reinterpret_cast<const char*>(g+8),strnlen(reinterpret_cast<const char*>(g+8),64))==std::string(reinterpret_cast<const char*>(nc.extra.data()),strnlen(reinterpret_cast<const char*>(nc.extra.data()),64));
            if(!same){std::ostringstream s;s<<tag<<" service #"<<i<<" "<<std::hex<<g[0];for(unsigned k=0;k<e.argc;++k)s<<" PC "<<g[2+k]<<"/native "<<nc.a[k];fail(s.str());same_log=false;}}
        for(std::size_t i=0;i<pc_draws.size()&&same_log;++i){const auto* g=pc_draws[i];const auto& nc=*nat_draws[i];
            bool same=true;for(unsigned k=0;k<6;++k)same=same&&g[2+k]==nc.a[k];
            same=same&&!std::memcmp(g+8,nc.extra.data(),64);
            if(!same){std::ostringstream s;s<<tag<<" draw #"<<i;fail(s.str());same_log=false;}}
        ++coverage["entry "+std::to_string(entry)];
        if(entry==1){std::ostringstream s;s<<"scene "<<std::hex<<sc.scene<<" state "<<g32(Work+0x1cu);++coverage[s.str()];
            coverage["draws"]+=unsigned(pc_draws.size());for(auto* g:pc_calls){std::ostringstream q;q<<"svc "<<std::hex<<g[0];++coverage[q.str()];}}
    }
    std::cout<<"{\"routine\":\""<<only<<"\",\"coverage\":{";bool first=true;
    for(const auto& [k,v]:coverage){std::cout<<(first?"":",")<<"\""<<k<<"\":"<<v;first=false;}
    std::cout<<"},\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}"<<std::endl;
    return mismatches?1:0;
}

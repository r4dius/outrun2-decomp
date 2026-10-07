// Included inside the compare_pc dispatcher; uses its private x86 harness.
if(only=="ending_probe"){
    // The arcade goal ending (race_ending.cpp) against the original:
    //   0 452B10  1 49A660 (mode 24 init)  2 49A710 (control: countdown, 4527F0, 4524E0)
    //   3 4527F0  4 4524E0 (states 0..7)  5 4523E0  6 4524B0  7 4527A0  8 451D10  9 4522D0
    //   10 49F410  11 49F4E0
    // External PC functions are capture stubs recording {pc, stack args} and returning scripted
    // values (4162B0 pushes 0.0); the draw leaves 4044F0 / 4056D0 / 404540 and the device's
    // SetRenderState (a fake device at [89BD60], vtable +E4) record the current matrix. The
    // VM snippet 44794F, 40A060 and D3DXMatrixTranslation run as original code. Compared: the
    // ending words 7D3A74..7D3A8F, 638DF0..638EAF, 836848, 8367A0..8367A7, the car, the robots,
    // the matrix arena, the service log and the draw log.
    using namespace outrun::platform;
    if(argc<=7||std::string(argv[7])=="-")throw std::runtime_error("ending probe requires the pinned d3dx9_29.dll");
    wire_d3dx_imports(map_d3dx29(argv[7]));
    constexpr unsigned base=0x50000000u,stubs=base,values=base+0x7000u,trace=base+0x10000u;
    constexpr unsigned Car=base+0x80000u,RobotA=base+0x81000u,RobotB=base+0x82000u,Dev=base+0x83000u,Vtbl=base+0x83100u;
    map_at(base,0x90000,PROT_READ|PROT_WRITE|PROT_EXEC);map_original_arithmetic_bridge(file);
    {   const std::size_t text_file=pe_file_offset(file,0x401000u);
        for(std::uint32_t a=0x401000u;a<0x401000u+0x194881u;a+=0x100u){
            const std::size_t n=std::min<std::size_t>(0x100u,0x401000u+0x194881u-a);
            if(std::memcmp(reinterpret_cast<void*>(a),file.data()+text_file+(a-0x401000u),n))r060_raw_write(a,file.data()+text_file+(a-0x401000u),n);
        }
    }
    for(std::uint32_t page=0x62f000u;page<0x98d000u;page+=0x1000u)mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE);
    init_matrix_oracle_globals();
    auto emit32=[](std::vector<std::uint8_t>& b,std::uint32_t v){for(unsigned k=0;k<4;++k)b.push_back(std::uint8_t(v>>(k*8)));};
    // kind: 0 plain, 2 x87 0.0, 4 draw leaf (the matrix of [89B564]), 6 the device's SetRenderState (stdcall)
    struct Svc{std::uint32_t pc;unsigned argc,kind;};
    const std::vector<Svc> services{
        {0x44c0d0,0,0},{0x4af580,0,0},{0x40ec60,1,0},{0x448fd0,2,0},{0x428320,3,0},{0x429a10,0,0},{0x42dfd0,0,0},{0x4489c0,0,0},
        {0x44c3d0,0,0},{0x44a1a0,0,0},{0x43de50,1,0},{0x4f11b0,1,0},{0x4f0600,1,0},{0x46fc30,1,0},{0x4f2210,0,0},{0x42df90,0,0},
        {0x4299a0,0,0},{0x4285a0,1,0},{0x401030,1,0},{0x42dfb0,1,0},{0x4299c0,1,0},{0x47ef30,1,0},{0x4b1dd0,0,0},{0x401000,3,0},
        {0x450380,1,0},{0x45bf30,1,0},{0x44fcc0,1,0},{0x44fce0,1,0},{0x448ad0,2,0},{0x46bbe0,1,0},{0x4f2020,2,0},{0x49a650,1,0},
        {0x42deb0,2,0},{0x429920,2,0},{0x448b90,0,0},{0x448980,0,0},{0x4f21b0,0,0},{0x427700,1,0},{0x440110,2,0},{0x4103f0,2,0},
        {0x4b5f60,1,0},{0x4b5fd0,0,0},{0x4afba0,0,0},{0x4b5fc0,0,0},{0x428880,1,0},{0x4bfb20,0,0},{0x4401d0,1,0},{0x440330,2,0},
        {0x43f8c0,1,0},{0x428730,2,0},{0x407cf0,6,0},{0x407dd0,6,0},{0x408390,4,0},{0x4083c0,4,0},{0x407f40,5,0},{0x408030,4,0},
        {0x407d60,4,0},{0x451800,5,0},{0x407e40,6,0},{0x406780,2,0},{0x4162b0,3,2},{0x4f6e40,1,0},{0x46ba20,1,0},{0x46bbc0,1,0},
        {0x46bb20,1,0},{0x4044f0,5,4},{0x4056d0,4,4},{0x404540,0,4},{PcRaceSetRenderState,3,6},{0x448cd0,1,0},{0x4044e0,1,4},{0x405360,6,4}};
    std::map<std::uint32_t,unsigned> index_of;unsigned stub_off=0;std::uint32_t render_state_stub=0;
    for(std::size_t k=0;k<services.size();++k){
        const auto& e=services[k];index_of[e.pc]=unsigned(k);
        std::vector<std::uint8_t> c{0x57,0x56,0x51,0x8b,0x3d};emit32(c,trace);
        c.insert(c.end(),{0x6b,0xff,0x60,0x81,0xc7});emit32(c,trace+64);
        c.insert(c.end(),{0xc7,0x07});emit32(c,e.pc);
        c.insert(c.end(),{0x89,0x4f,0x04});
        for(unsigned i=0;i<e.argc;++i)c.insert(c.end(),{0x8b,0x44,0x24,std::uint8_t(0x10+4*i),0x89,0x47,std::uint8_t(0x08+4*i)});
        if(e.kind==4||e.kind==6)c.insert(c.end(),{0x8b,0x35,0x64,0xb5,0x89,0x00,0x83,0xc7,0x20,0xb9,0x10,0,0,0,0xf3,0xa5});
        c.insert(c.end(),{0xff,0x05});emit32(c,trace);
        c.push_back(0xa1);emit32(c,values+4*unsigned(k));
        if(e.kind==2)c.insert(c.end(),{0xd9,0xee});
        c.insert(c.end(),{0x59,0x5e,0x5f});
        if(e.kind==6)c.insert(c.end(),{0xc2,0x0c,0x00});else c.push_back(0xc3);
        std::memcpy(reinterpret_cast<void*>(stubs+stub_off),c.data(),c.size());
        if(e.kind==6)render_state_stub=stubs+stub_off;else r046_write_rel32(e.pc,0xe9,stubs+stub_off);
        stub_off=(stub_off+unsigned(c.size())+15u)&~15u;
    }
    std::mt19937 rng(0x4524e000u);
    auto rnd=[&](std::uint32_t n){return n?std::uint32_t(rng()%n):0u;};
    auto real=[&](float a,float b){return a+(b-a)*float(rng()%100000)/100000.f;};
    auto g32=[](std::uint32_t a){return *reinterpret_cast<std::uint32_t*>(a);};
    auto p32=[](std::uint32_t a,std::uint32_t v){*reinterpret_cast<std::uint32_t*>(a)=v;};
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
    std::vector<Mirror> mirrors{{0x7d3a74u,0x7d3a90u,{}},{0x638df0u,0x638f70u,{}},{0x836848u,0x83684cu,{}},{0x8367a0u,0x8367a8u,{}},
        {Car,Car+0x340u,{}},{RobotA,RobotA+0x10u,{}},{RobotB,RobotB+0x10u,{}},{0x799d18u,0x799d1cu,{}},{0x79f010u,0x79f014u,{}},
        {0x79f04cu,0x79f050u,{}},{0x780258u,0x78025cu,{}},{0x78024cu,0x780250u,{}}};
    std::vector<std::uint8_t> data_638df0(0x180);std::memcpy(data_638df0.data(),file.data()+pe_file_offset(file,0x638df0u),data_638df0.size());
    static constexpr std::uint32_t Pc[]{0x452b10,0x49a660,0x49a710,0x4527f0,0x4524e0,0x4523e0,0x4524b0,0x4527a0,0x451d10,0x4522d0,0x49f410,0x49f4e0,0x44b890};
    const unsigned Entries=13u;
    for(unsigned iter=0;iter<count;++iter){
        const unsigned entry=std::getenv("OR2_ENDING_ENTRY")?unsigned(std::stoul(std::getenv("OR2_ENDING_ENTRY"))):iter%Entries;
        std::memset(reinterpret_cast<void*>(trace),0,0x8000);
        std::vector<std::uint32_t> value(services.size());
        for(auto& v:value)v=rnd(3)?rnd(3):rng();
        auto setv=[&](std::uint32_t pc,std::uint32_t v){value[index_of.at(pc)]=v;};
        setv(0x450380,0xau+rnd(15));   // the course ids 0xA..0x18 (the ending columns 0..9)setv(0x45bf30,rnd(4)?3u+rnd(5):rng());
        setv(0x4b5fd0,rnd(3)?rnd(6):rng());setv(0x4b5fc0,rnd(2)?0x460u+rnd(3)-1u:rnd(0x500));setv(0x428880,rnd(5));
        setv(0x406780,rnd(3));setv(0x46bbe0,0x500000u+rnd(0x100));setv(0x428320,rnd(4)?rnd(1344):0xffffffffu);
        std::memcpy(reinterpret_cast<void*>(values),value.data(),value.size()*4);
        for(auto& mi:mirrors)for(std::uint32_t a=mi.base;a<mi.end;a+=4)if(mi.end-a>=4)p32(a,rnd(3)?rnd(4):rng());else *reinterpret_cast<std::uint8_t*>(a)=std::uint8_t(rng());
        std::memcpy(reinterpret_cast<void*>(0x638df0u),data_638df0.data(),data_638df0.size());   // .data 638DF0..638F6F (token table, light rows read past 638EA8)
        p32(0x799d18u,Car);p32(0x79f010u,RobotA);p32(0x79f04cu,RobotB);
        p32(0x780258u,rnd(4)?rnd(3):rnd(8));p32(0x78024cu,rnd(4));
        p32(0x638e98u,entry==0u&&rnd(3)==0u?0x26u+rnd(4):rnd(0x26));   // 452B10 clamps it below 0x26p32(0x638e9cu,std::uint32_t(std::int32_t(rnd(12))-1));
        p32(0x7d3a74u,rnd(8));p32(0x7d3a78u,rnd(5)?0u:1u);p32(0x7d3a7cu,rnd(4));p32(0x7d3a84u,rnd(5)?0u:1u);
        *reinterpret_cast<std::uint8_t*>(0x7d3a88u)=std::uint8_t(rnd(2));*reinterpret_cast<std::uint8_t*>(0x7d3a80u)=std::uint8_t(rnd(16));
        p32(0x836848u,rnd(3)?rnd(3):rnd(10));
        for(std::uint32_t a:{0x8367a0u,0x8367a4u})p32(a,rnd(3)?rnd(1344):0xffffffffu);
        pf(Car+0x58u,real(0,2));
        p32(0x89bd60u,Dev);p32(Dev,Vtbl);p32(Vtbl+0xe4u,render_state_stub);
        std::memcpy(reinterpret_cast<void*>(values),value.data(),value.size()*4);
        for(auto& mi:mirrors){mi.bytes.resize(mi.end-mi.base);std::memcpy(mi.bytes.data(),reinterpret_cast<void*>(mi.base),mi.bytes.size());}
        reset_matrix_oracle_globals();
        std::vector<std::uint8_t> arena(0x10000);
        for(unsigned k=0;k<16;++k)pf(MatrixArena+k*4,k%5==0?1.f:real(-0.5f,0.5f));
        std::memcpy(arena.data(),reinterpret_cast<void*>(MatrixArena),arena.size());
        const std::uint32_t a_ending=rnd(0x26),a_column=rnd(10);
        prepare(Pc[entry]);
        if(entry==5){guest_call.eax=a_ending;guest_call.ecx=a_column;}
        if(entry==10||entry==11)reinterpret_cast<unsigned*>(S)[0]=Car;
        run();const std::uint32_t guest_eax=guest_call.out_eax;
        // native
        PcRaceMemory mem;std::vector<std::vector<std::uint8_t>> native(mirrors.size());
        for(std::size_t i=0;i<EmbeddedExeRangeCount;++i)mem.map_const(EmbeddedExeRanges[i].base,EmbeddedExeRanges[i].data,EmbeddedExeRanges[i].size);
        for(std::size_t i=0;i<mirrors.size();++i){native[i]=mirrors[i].bytes;mem.map(mirrors[i].base,native[i].data(),native[i].size());}
        outrun::driving::PcMatrixStack stack{outrun::driving::Bytes(arena.data(),arena.size()),0,0,64};
        struct NCall{std::uint32_t pc;decltype(PcRaceCall::args) a;};std::vector<NCall> calls;std::vector<PcVehicleDrawCall> draws;
        PcRaceContext ctx{mem,stack,[&](const PcRaceCall& k)->std::uint32_t{
            const auto it=index_of.find(k.pc);if(it==index_of.end()){std::ostringstream s;s<<"ending probe: unexpected service "<<std::hex<<k.pc;throw std::runtime_error(s.str());}
            calls.push_back({k.pc,k.args});
            if(services[it->second].kind==2)return 0u;
            return value[it->second];},&draws};
        std::uint32_t native_eax=0;bool ok=true;std::string why;
        try{
            switch(entry){
            case 0:ending_reset_452b10(ctx);break;case 1:mode24_init_49a660(ctx);break;case 2:mode24_control_49a710(ctx);break;
            case 3:ending_choice_4527f0(ctx);break;case 4:ending_states_4524e0(ctx);break;case 5:ending_lights_4523e0(ctx,a_ending,a_column);break;
            case 6:native_eax=ending_record_4524b0(ctx);break;case 7:mode24_exit_4527a0(ctx);break;case 8:ending_lights_init_451d10(ctx);break;
            case 9:ending_background_4522d0(ctx);break;case 10:ending_car_init_49f410(ctx,Car);break;case 11:ending_car_dest_49f4e0(ctx,Car);break;
            default:ending_area_display_44b890(ctx);break;
            }
        }catch(const std::exception& e){ok=false;why=e.what();}
        const std::string tag="entry "+std::to_string(entry)+" run "+std::to_string(iter);
        ++cases;if(!ok){fail(tag+" native threw: "+why);continue;}
        if(inject&&iter==7)native[0][0]^=1;
        for(std::size_t i=0;i<mirrors.size();++i){std::ostringstream s;s<<tag<<" region "<<std::hex<<mirrors[i].base;
            compare(native[i].data(),reinterpret_cast<void*>(mirrors[i].base),native[i].size(),s.str());}
        compare(arena.data(),reinterpret_cast<void*>(MatrixArena),arena.size(),tag+" matrix arena");
        ++cases;if(entry==6&&(native_eax&0xffu)!=(guest_eax&0xffu)){fail(tag+" EAX");}
        const unsigned n=g32(trace);
        std::vector<const std::uint32_t*> pc_calls,pc_draws;
        auto is_draw=[&](std::uint32_t pc){const auto it=index_of.find(pc);return it!=index_of.end()&&(services[it->second].kind==4||services[it->second].kind==6);};
        for(unsigned i=0;i<n;++i){const auto* g=reinterpret_cast<const std::uint32_t*>(trace+64+i*0x60);(is_draw(g[0])?pc_draws:pc_calls).push_back(g);}
        ++cases;
        if(pc_calls.size()!=calls.size()||pc_draws.size()!=draws.size()){std::ostringstream s;s<<tag<<" count PC="<<pc_calls.size()<<"/"<<pc_draws.size()<<" native="<<calls.size()<<"/"<<draws.size()<<std::hex;
            for(std::size_t i=0;i<pc_calls.size()&&i<40;++i)s<<" "<<pc_calls[i][0];s<<" | native";for(auto& cl:calls)s<<" "<<cl.pc;fail(s.str());continue;}
        bool same_log=true;
        for(std::size_t i=0;i<calls.size()&&same_log;++i){const auto* g=pc_calls[i];const auto& e=services[index_of[g[0]]];
            bool same=g[0]==calls[i].pc;
            for(unsigned k=0;k<e.argc;++k)same=same&&g[2+k]==calls[i].a[k];
            if(!same){std::ostringstream s;s<<tag<<" service #"<<i<<" "<<std::hex<<g[0]<<"/"<<calls[i].pc;for(unsigned k=0;k<e.argc;++k)s<<" PC "<<g[2+k]<<"/native "<<calls[i].a[k];fail(s.str());same_log=false;}}
        for(std::size_t i=0;i<draws.size()&&same_log;++i){const auto* g=pc_draws[i];const auto& d=draws[i];
            const unsigned skip=d.pc==PcRaceSetRenderState?1u:0u;      // the device pointer
            bool same=g[0]==d.pc;for(unsigned k=0;k+skip<services[index_of[g[0]]].argc;++k)same=same&&g[2+k+skip]==d.args[k];
            same=same&&!std::memcmp(g+8,d.matrix.data(),64);
            if(!same){std::ostringstream s;s<<tag<<" draw #"<<i<<" "<<std::hex<<g[0]<<"/"<<d.pc;for(unsigned k=0;k<5;++k)s<<" "<<g[2+k]<<"/"<<d.args[k];
                s<<" m0 "<<g[8]<<"/"<<*reinterpret_cast<const std::uint32_t*>(d.matrix.data())<<" m12 "<<g[8+12]<<"/"<<*reinterpret_cast<const std::uint32_t*>(d.matrix.data()+48);fail(s.str());same_log=false;}}
        ++coverage["entry "+std::to_string(entry)];
        if(entry==4)++coverage["4524E0 state "+std::to_string(std::min(mirrors[0].bytes[0]|0u,7u))];
        if(entry==3)for(auto& cl:calls)if(cl.pc==0x448ad0u&&cl.a[1]==2u)++coverage["448AD0 mode 2"];
    }
    std::cout<<"{\"routine\":\""<<only<<"\",\"coverage\":{";bool first=true;
    for(const auto& [k,v]:coverage){std::cout<<(first?"":",")<<"\""<<k<<"\":"<<v;first=false;}
    std::cout<<"},\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}"<<std::endl;
    return mismatches?1:0;
}

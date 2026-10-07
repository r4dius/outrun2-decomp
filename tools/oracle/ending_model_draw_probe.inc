if(only=="ending_model_draw_probe"){
    // Original 469FF0 (the arcade ending's car draw list, 46C060) with its protected bridge live.
    // The render leaves are capture stubs: each records its PC, arguments and
    // the current matrix; the native draw list must match it exactly.
    if(argc<=7||std::string(argv[7])=="-")throw std::runtime_error("model draw oracle requires pinned d3dx9_29.dll");
    constexpr unsigned base=0x3a800000,car=base,trace=base+0x10000,stubs=base+0x1f000;
    map_at(base,0x20000,PROT_READ|PROT_WRITE|PROT_EXEC);map_original_arithmetic_bridge(file);
    init_matrix_oracle_globals();wire_d3dx_imports(map_d3dx29(argv[7]));
    for(unsigned page:{0x82e000u,0x82f000u,0x780000u,0x64d000u,0x897000u,0x864000u,0x899000u,0x79f000u})
        if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("model draw globals");
    auto emit32=[](std::vector<std::uint8_t>& b,std::uint32_t v){for(unsigned k=0;k<4;++k)b.push_back(std::uint8_t(v>>(k*8)));};
    struct Leaf{std::uint32_t pc;unsigned argc;};
    const Leaf leaves[]={{0x405360u,6},{0x4056d0u,4},{0x4044f0u,5},{0x404540u,0},{0x4052b0u,0},{0x4052c0u,0},{0x405350u,0},{0x422550u,2},{0x422740u,1}};
    unsigned slot=0;
    for(const auto& l:leaves){
        std::vector<std::uint8_t> a{0x56,0x57,0x51,0xa1};emit32(a,trace);        // push esi/edi/ecx; mov eax,[count]
        a.insert(a.end(),{0x6b,0xc8,0x60,0x81,0xc1});emit32(a,trace+0x10);       // imul ecx,eax,0x60; add ecx,records
        a.insert(a.end(),{0xc7,0x01});emit32(a,l.pc);                            // mov [ecx],pc
        a.insert(a.end(),{0x8d,0x74,0x24,0x10,0x8d,0x79,0x04,0x51,0xb9});emit32(a,l.argc); // lea esi,args; lea edi,[ecx+4]; push ecx; mov ecx,argc
        a.insert(a.end(),{0xf3,0xa5,0x59,0x8d,0x79,0x20,0x8b,0x35,0x64,0xb5,0x89,0x00,0xb9,0x10,0,0,0,0xf3,0xa5,0x40,0xa3});
        emit32(a,trace);a.insert(a.end(),{0x59,0x5f,0x5e,0xc3});
        std::memcpy(reinterpret_cast<void*>(stubs+slot*0x80),a.data(),a.size());
        r046_write_rel32(l.pc,0xe9,stubs+slot*0x80);++slot;
    }
    std::mt19937 rng(0x469ff000u);
    auto real=[&](float lo,float hi){return lo+(hi-lo)*(float(rng()&0xffffu)/65535.f);};
    // Comparison: native 46AE70 draw list against the original.
    unsigned cases{},mismatches{};const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
    auto compare=[&](const void* native,const void* original,std::size_t n,const char* what,unsigned model,unsigned run){
        ++cases;if(!std::memcmp(native,original,n))return;
        ++mismatches;if(mismatches<12){const auto* a=static_cast<const std::uint8_t*>(native);const auto* b=static_cast<const std::uint8_t*>(original);
            for(std::size_t i=0;i<n;++i)if(a[i]!=b[i]){std::cerr<<what<<" model="<<model<<" run="<<run<<" offset=0x"<<std::hex<<i<<" PC="<<unsigned(b[i])<<" native="<<unsigned(a[i])<<std::dec<<"\n";break;}}
    };
    std::array<std::uint8_t,0x1000> body{};
    unsigned total_calls=0,steer_runs=0,roof_runs=0,shadow449640=0;
    for(unsigned run=0;run<3000;++run){
        const unsigned model=run%30u;
        std::array<std::uint8_t,0x10f0> ev{};for(auto& b:ev)b=std::uint8_t(rng());
        Bytes e(ev.data(),ev.size());
        for(unsigned k=0;k<0x10f0;k+=4)e.putf(k,real(-2,2));
        e.put8(0x11,std::uint8_t(model));e.put8(0x12,std::uint8_t(rng()));
        std::uint32_t flags=rng();
        if(run%3u==0u)flags=(flags&~0xc000u)|0x4000u; // favour the full view path
        if(run%5u==1u)flags=(flags&~0xc000u)|0x8000u;
        e.put32(4,flags);
        e.put32(0x2bc,run%3u?rng():0u);e.put32(0x2c4,rng());e.put32(0x38,run%4u==0u?0u:std::uint32_t(rng()%400u));e.put32(0x1f4,run%5u==0u?0u:rng()%1000u);
        e.put16(0x204,std::uint16_t(rng()));e.put16(0x2e,std::uint16_t(rng()));
        for(unsigned w:{0x32u,0x40u,0x42u,0x44u,0x46u,0x164u,0x166u})e.put16(w,run%7u==0u?0u:std::uint16_t(rng()));
        if(run%11u==0u)e.put32(0x4,e.u32(4)|0x100u);
        e.putf(0x1c4,real(-3,3));e.putf(0x2fc,real(-0.5f,0.8f));e.putf(0x2c8,real(-1,1));
        e.putf(0x2dc,real(-1,2));e.putf(0xe9c,run%4u?real(0,1.5f):1.f);
        for(unsigned k=0;k<16;++k)e.putf(0xf0+k*4,(k%5u)==0u?1.f:real(-0.3f,0.3f));
        for(unsigned k=0;k<16;++k)e.putf(0xb0+k*4,real(-1,1));
        for(unsigned k=0;k<4;++k){e.putf(0x130+k*12,real(-1,1));e.putf(0x134+k*12,real(-1,1));e.putf(0x138+k*12,real(-2,2));}
        for(auto& b:body)b=std::uint8_t(rng());
        Bytes bw(body.data(),body.size());
        for(std::size_t w:{0x288u,0x37cu,0x470u,0x564u}){bw.put16(w,run%7u==0u?0u:std::uint16_t(rng()));bw.put16(w+2,run%6u==0u?0u:std::uint16_t(rng()));}
        const std::int32_t debug=run%29u==0u?std::int32_t(rng()%3u):-1;
        const std::int32_t mode=run%2u?32:std::int32_t(rng()%40u);
        const std::uint8_t scene=std::uint8_t(run%9u==0u?6u:(run%13u==0u?15u:rng()%20u));
        std::memcpy(reinterpret_cast<void*>(car),ev.data(),ev.size());
        std::memcpy(reinterpret_cast<void*>(0x82e7f0),body.data(),0x800);
        *reinterpret_cast<std::int32_t*>(0x64d198)=debug;*reinterpret_cast<std::int32_t*>(0x78026c)=mode;
        *reinterpret_cast<std::uint8_t*>(0x82e7d4)=scene;
        std::memset(reinterpret_cast<void*>(trace),0,0xf000);
        reset_matrix_oracle_globals();
        std::vector<std::uint8_t> arena(0x400);Bytes ar(arena.data(),arena.size());
        for(unsigned k=0;k<16;++k)ar.putf(k*4,real(-2,2));
        std::memcpy(reinterpret_cast<void*>(MatrixArena),arena.data(),arena.size());
        prepare(0x469ff0);auto* st=reinterpret_cast<unsigned*>(S);st[0]=car;st[1]=model;run_original32();
        PcMatrixStack stack{ar,0,0,64};std::vector<outrun::platform::PcVehicleDrawCall> calls;
        try{outrun::platform::vehicle_ending_model_draw_469ff0(e,std::int32_t(model),stack,calls);}
        catch(const std::exception& ex){++cases;++mismatches;if(mismatches<12)std::cerr<<"native 469FF0 threw model="<<model<<": "<<ex.what()<<"\n";continue;}
        total_calls+=unsigned(calls.size());
        if(((flags>>14)&3u)==1u)++steer_runs;
        if(model==5&&((flags>>14)&3u)==1u)++roof_runs;
        if(((flags>>6)&1u)==0u&&(flags&0x800000u)&&(scene==6||scene==15))++shadow449640;
        const unsigned count=*reinterpret_cast<unsigned*>(trace);
        std::vector<std::uint8_t> native_trace(0x10+calls.size()*0x60),original_trace(0x10+count*0x60);
        std::memcpy(original_trace.data(),reinterpret_cast<void*>(trace),original_trace.size());
        Bytes nt(native_trace.data(),native_trace.size());nt.put32(0,std::uint32_t(calls.size()));
        for(std::size_t k=0;k<calls.size();++k){
            const auto& c=calls[k];nt.put32(0x10+k*0x60,c.pc);
            for(unsigned a=0;a<c.argc;++a)nt.put32(0x14+k*0x60+a*4,c.args[a]);
            std::memcpy(native_trace.data()+0x30+k*0x60,c.matrix.data(),64);
            // Unused argument slots in the original capture hold caller stack bytes.
            for(unsigned a=c.argc;a<6;++a)if(k<count)std::memcpy(native_trace.data()+0x14+k*0x60+a*4,original_trace.data()+0x14+k*0x60+a*4,4);
            if(k<count)std::memcpy(native_trace.data()+0x2c+k*0x60,original_trace.data()+0x2c+k*0x60,4);
        }
        if(inject&&run==2999&&!calls.empty())native_trace[0x30]^=1;
        if(native_trace.size()!=original_trace.size()){++cases;++mismatches;if(mismatches<12)std::cerr<<"call count model="<<model<<" run="<<run<<" PC="<<count<<" native="<<calls.size()<<"\n";}
        else compare(native_trace.data(),original_trace.data(),native_trace.size(),"draw list",model,run);
        compare(ev.data(),reinterpret_cast<void*>(car),ev.size(),"car object",model,run);
        compare(arena.data(),reinterpret_cast<void*>(MatrixArena),arena.size(),"matrix arena",model,run);
        std::array<std::uint32_t,2> sp{MatrixArena+std::uint32_t(stack.current_offset),std::uint32_t(stack.depth)};
        compare(sp.data(),reinterpret_cast<void*>(0x89b564),8,"matrix pointer/depth",model,run);
    }
    std::cerr<<"ending model draw coverage: calls="<<total_calls<<" interior="<<steer_runs<<" roof="<<roof_runs<<" shadow449640="<<shadow449640<<"\n";
    std::cout<<"{\"routine\":\"ending_model_draw_probe\",\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}"<<'\n';return mismatches?1:0;
}

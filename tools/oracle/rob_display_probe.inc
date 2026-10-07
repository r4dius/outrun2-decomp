if(only=="rob_display_probe"){
    // Original rob_disp_disp 514E60 (514C30, 513D10 face, 514280 body with
    // the bridge 103CB43, 513E40 hands, 514B60 prop, the kind 3/4 disc)
    // against rob_disp_disp_514e60 over synthetic characters, display works
    // and bone matrices; the matrix leaves run on the pinned d3dx9_29. The
    // draw leaves 405360 405450 405580 406800 4044F0 404540 405350 4052B0
    // 4052C0 410670 and 448CD0 are capture stubs recording their arguments
    // and the current matrix (405580 also its 4 palette matrices; 448CD0
    // answers table[name & 0x7FF]). Compared: the ordered leaf calls, the
    // display block 8573D0..857900, the matrix arena and pointer/depth.
    using namespace outrun::platform;
    if(argc<=7||std::string(argv[7])=="-")throw std::runtime_error("rob display oracle requires pinned d3dx9_29.dll");
    const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
    map_original_arithmetic_bridge(file);
    init_matrix_oracle_globals();wire_d3dx_imports(map_d3dx29(argv[7]));
    for(std::uint32_t page=0x62f000u;page<0x98d000u;page+=0x1000u)mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE);
    {   auto* iat=reinterpret_cast<void*>(0x596000u);
        if(mprotect(iat,4096,PROT_READ|PROT_WRITE))throw std::runtime_error("display IAT");
        *reinterpret_cast<std::uint32_t*>(0x5962b4u)=D3DX_BASE+(0x441288u-0x400000u);   // D3DXMatrixScaling
        if(mprotect(iat,4096,PROT_READ))throw std::runtime_error("display IAT restore");}
    constexpr std::uint32_t Area=0x3a840000u,Stubs=0x3a850000u,Trace=0x3a860000u,Table=0x3a8a0000u,Stride=0x180u,Cap=0x100u;
    map_at(Area,0x10000,PROT_READ|PROT_WRITE);map_at(Stubs,0x4000,PROT_READ|PROT_WRITE|PROT_EXEC);
    map_at(Trace,Stride*Cap+0x100,PROT_READ|PROT_WRITE);map_at(Table,0x2000,PROT_READ|PROT_WRITE);
    auto g32=[](std::uint32_t a){return *reinterpret_cast<const std::uint32_t*>(a);};
    auto p32=[](std::uint32_t a,std::uint32_t v){*reinterpret_cast<std::uint32_t*>(a)=v;};
    auto e32=[](std::vector<std::uint8_t>& b,std::uint32_t v){for(unsigned k=0;k<4;++k)b.push_back(std::uint8_t(v>>(k*8)));};
    struct Leaf{std::uint32_t pc;unsigned argc;};
    const std::vector<Leaf> leaves{{0x405360,6},{0x405450,5},{0x405580,6},{0x406800,1},{0x4044f0,5},{0x404540,0},{0x405350,0},
        {0x4052b0,0},{0x4052c0,0},{0x410670,0},{0x448cd0,1}};
    std::map<std::uint32_t,unsigned> argc_of;unsigned off=0;
    for(const auto& l:leaves){
        argc_of[l.pc]=l.argc;
        // record = [Trace+0x10 + n*Stride]: pc, args[6], matrix (64), palette (256)
        std::vector<std::uint8_t> c{0x52,0x56,0x57,0x51,0x8b,0x15};e32(c,Trace);                          // push edx/esi/edi/ecx; mov edx,[count]
        c.insert(c.end(),{0x69,0xd2});e32(c,Stride);c.insert(c.end(),{0x81,0xc2});e32(c,Trace+0x10);      // imul edx,Stride; add edx,Trace+10
        c.insert(c.end(),{0xc7,0x02});e32(c,l.pc);                                                       // mov [edx],pc
        for(unsigned i=0;i<6;++i){
            if(i<l.argc)c.insert(c.end(),{0x8b,0x4c,0x24,std::uint8_t(0x14+4*i)});else c.insert(c.end(),{0x31,0xc9});
            c.insert(c.end(),{0x89,0x4a,std::uint8_t(4+4*i)});}
        c.insert(c.end(),{0x8b,0x35});e32(c,0x89b564u);c.insert(c.end(),{0x8d,0x7a,0x20,0xb9,0x10,0,0,0,0xf3,0xa5});   // matrix
        if(l.pc==0x405580u){c.insert(c.end(),{0x8b,0x74,0x24,0x24,0x8d,0x7a,0x60,0xb9,0x40,0,0,0,0xf3,0xa5});}          // palette = arg5
        c.insert(c.end(),{0xff,0x05});e32(c,Trace);
        c.insert(c.end(),{0x59,0x5f,0x5e,0x5a});
        if(l.pc==0x448cd0u){c.insert(c.end(),{0x8b,0x44,0x24,0x04,0x25,0xff,0x07,0,0,0x8b,0x04,0x85});e32(c,Table);}
        else c.insert(c.end(),{0x31,0xc0});
        c.push_back(0xc3);
        std::memcpy(reinterpret_cast<void*>(Stubs+off),c.data(),c.size());
        r046_write_rel32(l.pc,0xe9,Stubs+off);off=(off+unsigned(c.size())+15u)&~15u;
    }
    std::mt19937 rng(0x514e60u);
    auto real=[&](float lo,float hi){return lo+(hi-lo)*(float(rng()&0xffffu)/65535.f);};
    const unsigned cases=std::max(count,1u)*20u;
    unsigned mismatches=0,calls_total=0;bool injected=false;
    auto report=[&](const std::string& w){++mismatches;if(mismatches<=12)std::cerr<<"MISMATCH "<<w<<char(10);};
    std::vector<std::uint8_t> disp(PcRobDispState::End-PcRobDispState::Base),area(0x10000),pools(0x2000),t82(0x100),cells(0x10);
    std::vector<std::uint32_t> names(0x800);
    for(unsigned cs=0;cs<cases;++cs){
        const std::string tag=" case "+std::to_string(cs);
        std::fill(area.begin(),area.end(),0);
        auto w32=[&](std::uint32_t a,std::uint32_t v){std::memcpy(area.data()+(a-Area),&v,4);};
        auto w16=[&](std::uint32_t a,std::uint16_t v){std::memcpy(area.data()+(a-Area),&v,2);};
        auto wf=[&](std::uint32_t a,float v){std::memcpy(area.data()+(a-Area),&v,4);};
        auto matrix=[&](std::uint32_t a){for(unsigned k=0;k<16;++k)wf(a+k*4,k==15?1.f:(k>=12?real(-3,3):real(-1,1)));};
        const std::uint32_t Work=Area,Mot=0x82f5f0u,Chr=Area+0x100,Rigid=Area+0x200,Skin=Area+0x400,Remap=Area+0x600,Inv=Area+0x1000,Bones=Area+0x4000;
        const std::uint32_t index=rng()%3u,kind=rng()%20u;
        w32(Work,index);w32(Work+4,(rng()%8u==0u?0u:0x8000u)|(rng()&0xcu));w32(Work+8,kind);matrix(Work+0x10);
        const unsigned nbones=10u+rng()%20u;
        for(unsigned b=0;b<nbones;++b)matrix(Bones+b*0x350u);
        const std::uint16_t nrigid=std::uint16_t(rng()%6u),nskin=std::uint16_t(rng()%5u),nremap=std::uint16_t(nbones+rng()%4u);   // every bone id remapped (retail data)
        w16(Chr+2,nrigid);w16(Chr+4,nskin);w16(Chr+6,nremap);w32(Chr+8,Rigid);w32(Chr+0xc,Skin);w32(Chr+0x10,Remap);w32(Chr+0x18,Inv);
        for(unsigned i=0;i<nremap;++i)w16(Remap+i*2,std::uint16_t(i<nbones?i:(0x8000u|(rng()%nbones))));
        for(unsigned i=0;i<32;++i)matrix(Inv+i*0x40u);
        for(unsigned i=0;i<nrigid;++i){w32(Rigid+i*8,(rng()%3u==0u?0x8000u:0u)|(rng()%(nbones+4u)));w32(Rigid+i*8+4,rng()%6u==0u?0xffffffffu:rng());}
        for(unsigned i=0;i<nskin;++i){const std::uint32_t r=Skin+i*0x14u;const std::uint16_t n=std::uint16_t(rng()%5u);w16(r+2,n);
            for(unsigned j=0;j<4;++j)w16(r+4+j*2,std::uint16_t((rng()%3u==0u?0x8000u:0u)|(rng()%nbones)));w32(r+0x10,rng());}   // in range: a skipped slot keeps PC stack contents
        // remap table values (8573D0 and the frame table) must stay small: the PC indexes the inverse matrices with them
        for(auto& b:disp)b=std::uint8_t(rng());
        for(unsigned k=0;k<0x100;++k){const std::uint32_t v=rng()%32u;std::memcpy(disp.data()+k*4,&v,4);}
        {const std::uint32_t d=PcRobDispState::Works+index*0x60u-PcRobDispState::Base;
         for(unsigned k:{0x10u,0x14u,0x18u,0x1cu,0x58u,0x5cu}){const std::uint32_t v=rng()%4u==0u?0xffffffffu:(rng()%3u==0u?0xcd0000u|(rng()%8u):rng());std::memcpy(disp.data()+d+k,&v,4);}
         for(unsigned k:{0xcu,0x50u,0x54u}){const std::uint32_t v=rng()%(nbones+3u);std::memcpy(disp.data()+d+k,&v,4);}
         for(unsigned k=0x20;k<0x50;k+=4){const float v=real(-1,1);std::memcpy(disp.data()+d+k,&v,4);}
         const std::uint32_t a4=rng()%4u,a8=rng()%5u;std::memcpy(disp.data()+d+4,&a4,4);std::memcpy(disp.data()+d+8,&a8,4);}
        for(unsigned k=0;k<0x2000;k+=4){const float v=(k%64==60)?1.f:real(-1,1);std::memcpy(pools.data()+k,&v,4);}
        std::fill(t82.begin(),t82.end(),0);{const std::uint32_t v=Chr;std::memcpy(t82.data()+kind*4,&v,4);}
        std::fill(cells.begin(),cells.end(),0);{const std::uint32_t pr=rng()%4u;std::memcpy(cells.data(),&pr,4);}
        for(auto& v:names)v=rng()%5u==0u?0u:rng();
        std::vector<std::uint8_t> mot(0xa4,0);{const std::uint16_t nb=std::uint16_t(nbones);std::memcpy(mot.data()+0x68,&nb,2);std::memcpy(mot.data()+0x74,&Bones,4);
         std::uint16_t count=std::uint16_t(nbones);std::memcpy(mot.data()+0x58+0x10,&count,2);}
        std::memcpy(reinterpret_cast<void*>(Area),area.data(),area.size());
        std::memcpy(reinterpret_cast<void*>(PcRobDispState::Base),disp.data(),disp.size());
        std::memcpy(reinterpret_cast<void*>(0x858710u+index*0x800u),pools.data()+index*0x800u,0x800);
        std::vector<std::uint8_t> pool_native(0x2000);std::memcpy(pool_native.data(),pools.data(),pools.size());
        std::memcpy(reinterpret_cast<void*>(0x82f388u),t82.data(),t82.size());
        std::memcpy(reinterpret_cast<void*>(0x78024cu),cells.data(),4);
        std::memcpy(reinterpret_cast<void*>(Mot+index*0xa4u),mot.data(),mot.size());
        std::memcpy(reinterpret_cast<void*>(Table),names.data(),names.size()*4);
        std::vector<std::uint8_t> arena(0x1000);outrun::driving::Bytes ar(arena.data(),arena.size());
        for(unsigned q=0;q<16;++q)ar.putf(q*4,q==15?1.f:(q>=12?real(-5,5):real(-1,1)));
        reset_matrix_oracle_globals();std::memcpy(reinterpret_cast<void*>(MatrixArena),arena.data(),arena.size());
        p32(Trace,0);
        prepare(0x514e60u);reinterpret_cast<unsigned*>(S)[0]=Work;run_original32();
        // native
        PcRaceMemory m;
        m.map(Area,area.data(),area.size());m.map(PcRobDispState::Base,disp.data(),disp.size());
        m.map(0x858710u,pool_native.data(),0x1800);m.map(0x82f388u,t82.data(),t82.size());m.map(0x78024cu,cells.data(),4);
        m.map(Mot+index*0xa4u,mot.data(),mot.size());rob_disp_map_rdata(m);
        outrun::driving::PcMatrixStack stack{ar,0,0,64};
        struct NCall{std::uint32_t pc;decltype(PcRaceCall::args) a;std::array<std::uint8_t,64> mx;std::array<std::uint8_t,256> pal;};
        std::vector<NCall> calls;
        PcRaceContext ctx{m,stack,[&](const PcRaceCall& k)->std::uint32_t{
            if(!argc_of.count(k.pc)){std::ostringstream e;e<<"unexpected leaf "<<std::hex<<k.pc;throw std::runtime_error(e.str());}
            NCall n{k.pc,k.args,{},{}};const auto cur=stack.current();for(unsigned q=0;q<64;++q)n.mx[q]=cur.u8(q);
            if(k.pc==0x405580u)for(unsigned q=0;q<256;++q)n.pal[q]=m.mapped(k.args[4]+q,1)?m.u8(k.args[4]+q):0;
            calls.push_back(n);
            return k.pc==0x448cd0u?names[k.args[0]&0x7ffu]:0u;},nullptr};
        try{rob_disp_disp_514e60(ctx,Work);}catch(const std::exception& e){report(std::string("native threw: ")+e.what()+tag);continue;}
        if(inject&&!injected&&!calls.empty()){calls[0].mx[3]^=1u;injected=true;}
        const std::uint32_t n=g32(Trace);calls_total+=n;
        if(n!=calls.size()){report("call count "+std::to_string(n)+" vs "+std::to_string(calls.size())+tag);continue;}
        for(std::uint32_t i=0;i<n;++i){
            const std::uint32_t rec=Trace+0x10+i*Stride;const auto& c=calls[i];
            bool ok=g32(rec)==c.pc;
            const unsigned used=argc_of[c.pc];
            for(unsigned a=0;ok&&a<used;++a){
                if(c.pc==0x405580u&&a==4)continue;                        // palette pointer: stack address on the PC
                ok=g32(rec+4+a*4)==c.a[a];}
            if(ok)ok=!std::memcmp(reinterpret_cast<const void*>(rec+0x20),c.mx.data(),64);
            if(ok&&c.pc==0x405580u)ok=!std::memcmp(reinterpret_cast<const void*>(rec+0x60),c.pal.data(),std::min<std::uint32_t>(c.a[5],4u)*64u);
            if(!ok){std::ostringstream o;o<<"leaf "<<i<<" pc "<<std::hex<<g32(rec)<<"/"<<c.pc;
                for(unsigned a=0;a<used;++a)o<<" a"<<a<<"="<<g32(rec+4+a*4)<<"/"<<c.a[a];
                o<<" mx="<<!std::memcmp(reinterpret_cast<const void*>(rec+0x20),c.mx.data(),64);
                if(c.pc==0x405580u)for(unsigned q=0;q<4;++q)o<<" pal"<<q<<"="<<!std::memcmp(reinterpret_cast<const void*>(rec+0x60+q*64),c.pal.data()+q*64,64);
                report(o.str()+tag);break;}
        }
        if(std::memcmp(reinterpret_cast<const void*>(PcRobDispState::Base),disp.data(),disp.size()))report("display block"+tag);
        if(std::memcmp(reinterpret_cast<const void*>(MatrixArena),arena.data(),arena.size()))report("matrix arena"+tag);
        if(g32(0x89b564u)!=MatrixArena+std::uint32_t(stack.current_offset)||g32(0x89b568u)!=std::uint32_t(stack.depth))report("matrix pointer"+tag);
    }
    std::cerr<<"rob display: leaf calls="<<calls_total<<char(10);
    std::cout<<"{\"routine\":\"rob_display_probe\",\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}"<<'\n';return 0;
}

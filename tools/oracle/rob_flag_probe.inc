if(only=="rob_flag_probe"){
    // Original flagman flag cloth init 402100 (401080, 401830) and control
    // 402140 (401490/401530/4015E0/401720/401790, 401990/40F2C0) against
    // rob_flag, the D3DX transforms on the pinned d3dx9_29. Per case: init,
    // then random wind scale (+CAC), wind directions (+40), 95AEB8/95AEAC
    // and 1..6 control steps under random current matrices. Compared after
    // every call: the cloth 95B270..95BF20 (the stack contents of node
    // +34..+3C masked where the PC never writes them), the mesh 85FF20..,
    // the matrix arena.
    using namespace outrun::platform;
    if(argc<=7||std::string(argv[7])=="-")throw std::runtime_error("rob flag oracle requires pinned d3dx9_29.dll");
    const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
    map_original_arithmetic_bridge(file);
    init_matrix_oracle_globals();wire_d3dx_imports(map_d3dx29(argv[7]));
    for(std::uint32_t page=0x62f000u;page<0x98d000u;page+=0x1000u)mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE);
    std::mt19937 rng(0x402140u);
    auto real=[&](float lo,float hi){return lo+(hi-lo)*(float(rng()&0xffffu)/65535.f);};
    const unsigned cases=std::max(count,1u)*4u;
    unsigned mismatches=0,steps=0;bool injected=false;
    auto report=[&](const std::string& w){++mismatches;if(mismatches<=12)std::cerr<<"MISMATCH "<<w<<char(10);};
    for(unsigned cs=0;cs<cases;++cs){
        const std::string tag=" case "+std::to_string(cs);
        PcRobFlagState st;
        for(auto& b:st.cloth)b=std::uint8_t(rng());
        for(auto& b:st.mesh)b=std::uint8_t(rng());
        std::fill(st.wind.begin(),st.wind.end(),0);
        if(rng()%2u){for(unsigned k=0xc;k<0x18;k+=4){const float v=real(-0.01f,0.01f);std::memcpy(st.wind.data()+k,&v,4);}
                     const float b=real(0,0.01f);std::memcpy(st.wind.data(),&b,4);}
        std::memcpy(reinterpret_cast<void*>(PcRobFlagState::ClothBase),st.cloth.data(),st.cloth.size());
        std::memcpy(reinterpret_cast<void*>(PcRobFlagState::MeshBase),st.mesh.data(),st.mesh.size());
        std::memcpy(reinterpret_cast<void*>(PcRobFlagState::WindBase),st.wind.data(),st.wind.size());
        PcRaceMemory m;st.map(m);
        std::vector<std::uint8_t> arena(0x800);outrun::driving::Bytes ar(arena.data(),arena.size());
        reset_matrix_oracle_globals();
        outrun::driving::PcMatrixStack stack{ar,0,0,64};
        PcRaceContext ctx{m,stack,nullptr,nullptr};
        auto compare=[&](const std::string& what,bool row0_only){
            std::vector<std::uint8_t> g(st.cloth.size());std::memcpy(g.data(),reinterpret_cast<const void*>(PcRobFlagState::ClothBase),g.size());
            auto n=st.cloth;
            for(unsigned node=0;node<35;++node){
                if(row0_only&&node>=5)break;
                for(unsigned k=0x34;k<0x40;++k){g[0xc+node*0x5c+k]=0;n[0xc+node*0x5c+k]=0;}
            }
            if(inject&&!injected&&what=="ctrl"){n[0x10]^=1u;injected=true;}
            if(g!=n){for(std::size_t k=0;k<g.size();++k)if(g[k]!=n[k]){std::ostringstream o;o<<what<<" cloth +"<<std::hex<<k;report(o.str()+tag);break;}return;}
            if(std::memcmp(reinterpret_cast<const void*>(PcRobFlagState::MeshBase),st.mesh.data(),st.mesh.size())){
                for(std::size_t k=0;k<st.mesh.size();++k)if(reinterpret_cast<const std::uint8_t*>(PcRobFlagState::MeshBase)[k]!=st.mesh[k]){
                    std::ostringstream o;o<<what<<" mesh +"<<std::hex<<k;report(o.str()+tag);break;}}
        };
        prepare(0x402100u);run_original32();
        try{rob_flag_init_402100(ctx);}catch(const std::exception& e){report(std::string("init threw: ")+e.what()+tag);continue;}
        compare("init",false);
        // Wind scale and directions (the EXE leaves them 0; exercised here).
        if(rng()%2u){const float s=real(-2,2);std::memcpy(st.cloth.data()+0xcac,&s,4);std::memcpy(reinterpret_cast<void*>(PcRobFlagState::ClothBase+0xcac),&s,4);
            for(unsigned node=0;node<35;++node)for(unsigned k=0x40;k<0x4c;k+=4){const float v=real(-0.01f,0.01f);
                std::memcpy(st.cloth.data()+0xc+node*0x5c+k,&v,4);std::memcpy(reinterpret_cast<void*>(PcRobFlagState::ClothBase+0xc+node*0x5c+k),&v,4);}}
        const unsigned n=1u+rng()%6u;
        for(unsigned i=0;i<n;++i){
            for(unsigned q=0;q<16;++q)ar.putf(q*4,q==15?1.f:(q>=12?real(-2,2):real(-1,1)));
            std::memcpy(reinterpret_cast<void*>(MatrixArena),arena.data(),arena.size());
            prepare(0x402140u);run_original32();
            try{rob_flag_ctrl_402140(ctx);}catch(const std::exception& e){report(std::string("ctrl threw: ")+e.what()+tag);break;}
            ++steps;
            compare("ctrl",true);
            if(std::memcmp(reinterpret_cast<const void*>(MatrixArena),arena.data(),arena.size()))report("matrix arena"+tag);
        }
    }
    std::cerr<<"rob flag: control steps="<<steps<<char(10);
    std::cout<<"{\"routine\":\"rob_flag_probe\",\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}"<<'\n';return 0;
}

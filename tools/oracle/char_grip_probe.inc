if(only=="char_grip_probe"){
    // Original 46BC30 GetHandleGripPositionLocal against race_char_grip_46bc30
    // (EXE tables 5B2F68.., fcos/fsin, the matrix leaves over d3dx9_29).
    // Compared: both output vectors, the matrix arena, the pointer/depth.
    using namespace outrun::platform;
    if(argc<=7||std::string(argv[7])=="-")throw std::runtime_error("char grip oracle requires pinned d3dx9_29.dll");
    const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
    map_original_arithmetic_bridge(file);
    init_matrix_oracle_globals();wire_d3dx_imports(map_d3dx29(argv[7]));
    constexpr std::uint32_t Area=0x3a840000u;
    map_at(Area,0x10000,PROT_READ|PROT_WRITE);
    std::mt19937 rng(0x46bc30u);
    auto real=[&](float lo,float hi){return lo+(hi-lo)*(float(rng()&0xffffu)/65535.f);};
    const unsigned cases=std::max(count,1u)*40u;unsigned mismatches=0;
    auto report=[&](const std::string& w){++mismatches;if(mismatches<=12)std::cerr<<"MISMATCH "<<w<<char(10);};
    for(unsigned cs=0;cs<cases;++cs){
        std::vector<std::uint8_t> area(0x100,0);
        const std::uint32_t car=Area,A=Area+0x40,B=Area+0x60;
        area[0x11]=std::uint8_t(rng()%30u);
        const float angle=real(-3.2f,3.2f);
        std::memcpy(reinterpret_cast<void*>(Area),area.data(),area.size());
        std::vector<std::uint8_t> arena(0x800);outrun::driving::Bytes ar(arena.data(),arena.size());
        for(unsigned q=0;q<16;++q)ar.putf(q*4,q==15?1.f:(q>=12?real(-5,5):real(-1,1)));
        reset_matrix_oracle_globals();std::memcpy(reinterpret_cast<void*>(MatrixArena),arena.data(),arena.size());
        prepare(0x46bc30u);auto* st=reinterpret_cast<unsigned*>(S);st[0]=car;std::memcpy(&st[1],&angle,4);st[2]=A;st[3]=B;run_original32();
        PcRaceMemory m;m.map(Area,area.data(),area.size());race_robots_map_tables(m);
        outrun::driving::PcMatrixStack stack{ar,0,0,64};
        PcRaceContext ctx{m,stack,nullptr,nullptr};
        try{race_char_grip_46bc30(ctx,car,angle,A,B);}catch(const std::exception& e){report(std::string("native threw: ")+e.what());continue;}
        if(inject&&cs==5u)area[0x44]^=1u;
        if(std::memcmp(reinterpret_cast<const void*>(A),area.data()+0x40,0x2c))report("outputs case "+std::to_string(cs));
        if(std::memcmp(reinterpret_cast<const void*>(MatrixArena),arena.data(),arena.size()))report("matrix arena case "+std::to_string(cs));
        if(*reinterpret_cast<std::uint32_t*>(0x89b564u)!=MatrixArena+std::uint32_t(stack.current_offset))report("matrix pointer case "+std::to_string(cs));
    }
    std::cout<<"{\"routine\":\"char_grip_probe\",\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}"<<'\n';return 0;
}

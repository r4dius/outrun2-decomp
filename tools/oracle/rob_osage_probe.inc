if(only=="rob_osage_probe"){
    // Original rob_osage_init 514F60 (514E80, 5229A0, 522810, 522620 x20,
    // 522450, 5223B0, 522560, 401790, 5222B0) against rob_osage_init_514f60,
    // with the protected bridges live and the matrix leaves over the pinned
    // d3dx9_29. The 5229E7 malloc / 522299 free call sites go to guest stubs
    // (bump allocator = RobotHeap: 16-byte rounding, fresh bytes 0xCD; free
    // is a RET). Synthetic character data: osage records, parameter lists,
    // a motion object with random bone matrices. Compared: the osage block
    // 858710..85ADB8, the globals 85DE50..85DE64, the heap (node 0 +24/+28
    // are stack contents on the PC and are cleared on both sides), the
    // matrix arena and the stack pointer/depth.
    using namespace outrun::platform;
    if(argc<=7||std::string(argv[7])=="-")throw std::runtime_error("rob osage oracle requires pinned d3dx9_29.dll");
    const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
    map_original_arithmetic_bridge(file);
    init_matrix_oracle_globals();wire_d3dx_imports(map_d3dx29(argv[7]));
    for(std::uint32_t page=0x62f000u;page<0x98d000u;page+=0x1000u)mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE);
    constexpr std::uint32_t Heap=RobotHeap::Base,HeapSize=0x100000u,Files=0x3a840000u,Stubs=0x3a850000u,NextCell=0x3a851000u;
    map_at(Heap,HeapSize,PROT_READ|PROT_WRITE);map_at(Files,0x10000,PROT_READ|PROT_WRITE);map_at(Stubs,0x2000,PROT_READ|PROT_WRITE|PROT_EXEC);
    auto g32=[](std::uint32_t a){return *reinterpret_cast<const std::uint32_t*>(a);};
    auto p32=[](std::uint32_t a,std::uint32_t v){*reinterpret_cast<std::uint32_t*>(a)=v;};
    auto pf=[](std::uint32_t a,float v){*reinterpret_cast<float*>(a)=v;};
    {
        const std::uint8_t mal[]{0x8b,0x44,0x24,0x04,0x85,0xc0,0x75,0x05,0xb8,0x01,0,0,0, 0x83,0xc0,0x0f,0x83,0xe0,0xf0,
            0x57,0x89,0xc1,0xa1,0,0,0,0, 0x89,0xc7,0x01,0x0d,0,0,0,0, 0x50,0xb0,0xcd,0xf3,0xaa,0x58,0x5f,0xc3};
        std::vector<std::uint8_t> b(mal,mal+sizeof(mal));
        std::memcpy(b.data()+23,&NextCell,4);std::memcpy(b.data()+31,&NextCell,4);
        std::memcpy(reinterpret_cast<void*>(Stubs),b.data(),b.size());
        *reinterpret_cast<std::uint8_t*>(Stubs+0x100)=0xc3;
        r046_write_rel32(0x5229e7u,0xe8,Stubs);r046_write_rel32(0x522299u,0xe8,Stubs+0x100);
    }
    std::mt19937 rng(0x514f60u);
    auto real=[&](float lo,float hi){return lo+(hi-lo)*(float(rng()&0xffffu)/65535.f);};
    const unsigned cases=std::max(count,1u)*4u;
    unsigned mismatches=0,chains_total=0,nodes_total=0;
    auto report=[&](const std::string& what){++mismatches;if(mismatches<=12)std::cerr<<"MISMATCH "<<what<<char(10);};
    PcRaceMemory m;
    std::vector<std::uint8_t> files(0x10000),block(PcRobOsageState::ChainsEnd-PcRobOsageState::SphereBase),globals(0x14),table82(0x100);
    const std::uint32_t Work=Files,Chr=Files+0x100,Osage=Files+0x200,Recs=Files+0x300,Params=Files+0x800,Motion=Files+0xa00,Bones=Files+0x1000;
    for(unsigned cs=0;cs<cases;++cs){
        const std::string tag=" case "+std::to_string(cs);
        std::fill(files.begin(),files.end(),0);
        auto w32=[&](std::uint32_t a,std::uint32_t v){std::memcpy(files.data()+(a-Files),&v,4);};
        auto wf=[&](std::uint32_t a,float v){std::memcpy(files.data()+(a-Files),&v,4);};
        auto w16=[&](std::uint32_t a,std::uint16_t v){std::memcpy(files.data()+(a-Files),&v,2);};
        const std::uint32_t disp=rng()%3u,kind=rng()%20u;
        w32(Work,disp);w32(Work+8,kind);
        const bool no_osage=rng()%10u==0u;
        w32(Chr+0x1c,no_osage?0u:Osage);
        const std::uint32_t chains=1u+rng()%4u;
        w32(Osage,chains);w32(Osage+8,Recs);w32(Osage+0xc,Params);w32(Osage+0x10,Params+0x100);   // +10: sphere index list (515040)
        for(unsigned k=0;k<0x20;++k)w32(Params+0x100+k*4,rng()%12u);
        for(std::uint32_t i=0;i<chains;++i){
            const std::uint32_t r=Recs+i*0x24u;
            const std::uint32_t bones=16u;
            w32(r,rng()%6u==0u?(0x8000u|rng()%bones):(rng()%(bones+3u)));
            w32(r+8,rng()%7u);w32(r+4,rng()%8u);
            for(unsigned k=0;k<3;++k)wf(r+0xc+k*4,real(-0.3f,0.3f));
            for(unsigned k=0;k<3;++k)wf(r+0x18+k*4,real(-3.1f,3.1f));
        }
        for(unsigned k=0;k<0x40;++k)wf(Params+k*4,real(0.01f,0.2f));
        w16(Motion+0x68,16);w32(Motion+0x74,Bones);
        for(unsigned b=0;b<16;++b){const std::uint32_t M=Bones+b*0x350u;
            for(unsigned k=0;k<16;++k)wf(M+k*4,k==15?1.f:(k>=12?real(-2,2):real(-1.2f,1.2f)));}
        std::fill(table82.begin(),table82.end(),0);{const std::uint32_t v=Chr;std::memcpy(table82.data()+kind*4,&v,4);}
        for(auto& b:block)b=std::uint8_t(rng());
        // chains: zero (no previous nodes), words 85A710.. random
        for(std::uint32_t a=PcRobOsageState::Chains;a<PcRobOsageState::ChainsEnd;++a)block[a-PcRobOsageState::SphereBase]=0;
        for(auto& b:globals)b=std::uint8_t(rng());
        {const std::uint32_t z=0;std::memcpy(globals.data()+8,&z,4);std::memcpy(globals.data()+0xc,&z,4);}   // 85DE58/85DE5C: never written by the EXE
        std::memcpy(reinterpret_cast<void*>(Files),files.data(),files.size());
        std::memcpy(reinterpret_cast<void*>(PcRobOsageState::SphereBase),block.data(),block.size());
        std::memcpy(reinterpret_cast<void*>(PcRobOsageState::GlobalsBase),globals.data(),globals.size());
        std::memcpy(reinterpret_cast<void*>(0x82f388u),table82.data(),table82.size());
        std::memset(reinterpret_cast<void*>(Heap),0,HeapSize);p32(NextCell,Heap);
        std::vector<std::uint8_t> arena(0x800);outrun::driving::Bytes ar(arena.data(),arena.size());
        for(unsigned q=0;q<16;++q)ar.putf(q*4,q==15?1.f:(q>=12?real(-5,5):real(-1,1)));
        reset_matrix_oracle_globals();std::memcpy(reinterpret_cast<void*>(MatrixArena),arena.data(),arena.size());
        prepare(0x514f60u);reinterpret_cast<unsigned*>(S)[0]=Work;reinterpret_cast<unsigned*>(S)[1]=Motion;run_original32();
        // native
        RobotHeap heap;
        m.clear();m.map(Files,files.data(),files.size());m.map(PcRobOsageState::SphereBase,block.data(),block.size());
        m.map(PcRobOsageState::GlobalsBase,globals.data(),globals.size());m.map(0x82f388u,table82.data(),table82.size());
        outrun::driving::PcMatrixStack stack{ar,0,0,64};
        PcRaceContext ctx{m,stack,nullptr,nullptr};
        rob_disp_map_rdata(m);
        const unsigned controls=rng()%4u;
        try{
            rob_osage_init_514f60(ctx,heap,Work,Motion);
            for(unsigned q=0;q<controls;++q){
                prepare(0x515040u);reinterpret_cast<unsigned*>(S)[0]=Work;reinterpret_cast<unsigned*>(S)[1]=Motion;run_original32();
                rob_osage_ctrl_515040(ctx,Work,Motion);
            }
        }
        catch(const std::exception& e){report(std::string("native threw: ")+e.what()+tag);continue;}
        std::vector<std::uint8_t> native_heap(HeapSize,0);
        for(const auto& b:heap.blocks)std::memcpy(native_heap.data()+(b.base-Heap),b.bytes.data(),b.bytes.size());
        // node 0 +24/+28: PC stack contents
        for(std::uint32_t i=0;i<48;++i){const std::uint32_t ch=PcRobOsageState::Chains+i*0x20u;
            const std::uint32_t nodes=g32(ch+0xc);if(!nodes||nodes<Heap||nodes>=Heap+HeapSize)continue;
            ++chains_total;nodes_total+=g32(ch+8);
            p32(nodes+0x24,0);p32(nodes+0x28,0);
            std::memset(native_heap.data()+(nodes-Heap)+0x24,0,8);}
        if(inject&&cs==5u)block[PcRobOsageState::Chains-PcRobOsageState::SphereBase+disp*0x200+8]^=1u;
        if(std::memcmp(reinterpret_cast<const void*>(PcRobOsageState::SphereBase),block.data(),block.size())){
            for(std::size_t k=0;k<block.size();++k)if(reinterpret_cast<const std::uint8_t*>(PcRobOsageState::SphereBase)[k]!=block[k]){
                std::ostringstream o;o<<"osage block "<<std::hex<<PcRobOsageState::SphereBase+k;report(o.str()+tag);break;}}
        if(std::memcmp(reinterpret_cast<const void*>(PcRobOsageState::GlobalsBase),globals.data(),globals.size()))report("globals 85DE50"+tag);
        if(g32(NextCell)!=heap.next)report("heap next"+tag);
        const std::uint32_t used=heap.next-Heap;
        if(std::memcmp(reinterpret_cast<const void*>(Heap),native_heap.data(),used)){
            for(std::uint32_t k=0;k<used;++k)if(reinterpret_cast<const std::uint8_t*>(Heap)[k]!=native_heap[k]){
                std::ostringstream o;o<<"heap byte +"<<std::hex<<k;report(o.str()+tag);break;}}
        if(std::memcmp(reinterpret_cast<const void*>(MatrixArena),arena.data(),arena.size()))report("matrix arena"+tag);
        if(g32(0x89b564u)!=MatrixArena+std::uint32_t(stack.current_offset)||g32(0x89b568u)!=std::uint32_t(stack.depth))report("matrix pointer/depth"+tag);
    }
    std::cerr<<"osage: chains="<<chains_total<<" nodes="<<nodes_total<<char(10);
    std::cout<<"{\"routine\":\"rob_osage_probe\",\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}"<<'\n';return 0;
}

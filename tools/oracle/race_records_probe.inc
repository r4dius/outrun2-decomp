// Included inside the compare_pc dispatcher; uses its private x86 harness.
if(only=="race_records_probe"){
    // Record module of the arcade variant 0 (race_records.cpp) against the original:
    //   0 47ECA0 (tables allocated or reused, both cleared), 1 480AD0(bl level, kind),
    //   2 480FE0(a, b, c), 3 480130(buf, level, kind, half, media) (EBX index),
    //   4 480BC0(rec, index, level, kind, half) (its protected head live), 5 481230(a, b, c).
    // Capture stubs: 580253 malloc (scripted heap blocks in turn), 416700 (scripted slot), 416930
    // (copies a scripted 0xFD4-byte record into its destination), 4239C0 (the path string is
    // logged; scripted handle 0 / 1), 423F10 (0xFD4), 423CB0 (copies a scripted file record),
    // 423BD0, 416830. 449A80 runs as original code. Compared: the .bss words 80FB40..810453 /
    // 813730..81374F, the heap tables, the path buffer and the ordered service log.
    using namespace outrun::platform;
    constexpr unsigned base=0x3bd00000u,stubs=base,values=base+0x7000u,heap=base+0x10000u,trace=base+0x80000u;
    constexpr std::uint32_t Heap1=heap,Heap2=heap+0x10000u,Heap3=heap+0x30000u,Heap4=heap+0x50000u;   // up to 0x1DAD8 each
    constexpr std::uint32_t SaveRecord=base+0x90000u,FileRecord=base+0x91000u,PathBuffer=base+0x92000u;
    map_at(base,0x100000,PROT_READ|PROT_WRITE|PROT_EXEC);map_original_arithmetic_bridge(file);
    {   const std::size_t text_file=pe_file_offset(file,0x401000u);
        for(std::uint32_t a=0x401000u;a<0x401000u+0x194881u;a+=0x100u){
            const std::size_t n=std::min<std::size_t>(0x100u,0x401000u+0x194881u-a);
            if(std::memcmp(reinterpret_cast<void*>(a),file.data()+text_file+(a-0x401000u),n))r060_raw_write(a,file.data()+text_file+(a-0x401000u),n);
        }
    }
    for(std::uint32_t page=0x62f000u;page<0x98d000u;page+=0x1000u)mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE);
    auto emit32=[](std::vector<std::uint8_t>& b,std::uint32_t v){for(unsigned k=0;k<4;++k)b.push_back(std::uint8_t(v>>(k*8)));};
    struct Svc{std::uint32_t pc;unsigned argc;};
    const std::vector<Svc> services{{0x580253,1},{0x416700,7},{0x416930,9},{0x4239c0,2},{0x423f10,1},{0x423cb0,4},{0x423bd0,1},{0x416830,0}};
    const std::uint32_t malloc_counter=values+0x100u,malloc_list=values+0x104u;
    unsigned stub_off=0;
    for(std::size_t k=0;k<services.size();++k){
        const auto& e=services[k];
        // push edi/esi/ecx; edi = trace + 64 + [trace]*0x60; {pc, ecx, args}; [trace]++
        std::vector<std::uint8_t> c{0x57,0x56,0x51,0x8b,0x3d};emit32(c,trace);
        c.insert(c.end(),{0x6b,0xff,0x60,0x81,0xc7});emit32(c,trace+64);
        c.insert(c.end(),{0xc7,0x07});emit32(c,e.pc);
        c.insert(c.end(),{0x89,0x4f,0x04});
        for(unsigned i=0;i<e.argc;++i)c.insert(c.end(),{0x8b,0x44,0x24,std::uint8_t(0x10+4*i),0x89,0x47,std::uint8_t(0x08+4*i)});
        c.insert(c.end(),{0xff,0x05});emit32(c,trace);
        auto copy_record=[&](std::uint32_t src){   // push esi/edi; edi = arg0; esi = src; 0x3F5 dwords
            c.insert(c.end(),{0x56,0x57,0x8b,0x7c,0x24,0x18,0xbe});emit32(c,src);
            c.insert(c.end(),{0xb9,0xf5,0x03,0x00,0x00,0xf3,0xa5,0x5f,0x5e});};
        if(e.pc==0x416930u)copy_record(SaveRecord);
        if(e.pc==0x423cb0u)copy_record(FileRecord);
        if(e.pc==0x4239c0u)   // the path string (0x40 bytes) into record +0x20
            c.insert(c.end(),{0x56,0x57,0x8b,0x74,0x24,0x18,0x83,0xc7,0x20,0xb9,0x40,0x00,0x00,0x00,0xf3,0xa4,0x5f,0x5e});
        if(e.pc==0x580253u){   // eax = list[[counter]++]
            c.push_back(0xa1);emit32(c,malloc_counter);
            c.insert(c.end(),{0xff,0x05});emit32(c,malloc_counter);
            c.insert(c.end(),{0x8b,0x04,0x85});emit32(c,malloc_list);
        }else{c.push_back(0xa1);emit32(c,values+4u*unsigned(k));}
        c.insert(c.end(),{0x59,0x5e,0x5f,0xc3});
        std::memcpy(reinterpret_cast<void*>(stubs+stub_off),c.data(),c.size());
        r046_write_rel32(e.pc,0xe9,stubs+stub_off);stub_off=(stub_off+unsigned(c.size())+15u)&~15u;
    }
    std::mt19937 rng(0x480fe000u);
    auto rnd=[&](std::uint32_t n){return n?std::uint32_t(rng()%n):0u;};
    auto g32=[](std::uint32_t a){return *reinterpret_cast<std::uint32_t*>(a);};
    auto p32=[](std::uint32_t a,std::uint32_t v){*reinterpret_cast<std::uint32_t*>(a)=v;};
    unsigned cases{},mismatches{};const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
    std::map<std::string,unsigned> coverage;
    auto fail=[&](const std::string& what){++mismatches;if(mismatches<20)std::cerr<<what<<"\n";};
    // A record image: QHOT with a valid CRC, QHOT with a wrong CRC, or anything else.
    auto make_record=[&](std::vector<std::uint8_t>& r,unsigned kind){
        for(auto& b:r)b=std::uint8_t(rng());
        if(kind==2u)return;
        const std::uint32_t len=rnd(0xf94u);
        std::memcpy(r.data(),"QHOT",4);r[0x24]=0x32;r[0x25]=0x01;std::memcpy(r.data()+0xc,&len,4);
        PcRaceMemory m;ghost_map_tables(m);m.map(0x1000u,r.data(),r.size());
        m.put16(0x1026u,0xffffu);
        std::uint16_t crc=ghost_crc_449a80(m,0x1000u,std::int32_t(len+0x40u));
        if(rnd(2)){m.put16(0x1026u,0xffffu);crc=ghost_crc_449a80(m,0x1000u,std::int32_t(PcRecordBytes));}   // the file path's length
        if(kind==1u)crc^=0x0100u;
        m.put16(0x1026u,crc);
    };
    struct Mirror{std::uint32_t base,end;std::vector<std::uint8_t> bytes;};
    std::vector<Mirror> mirrors{{PcRecordState::BlockBase,PcRecordState::BlockEnd,{}},{PcRecordState::TailBase,PcRecordState::TailEnd,{}},
        {0x78024cu,0x780250u,{}},{Heap1,Heap1+PcRecordTable1Bytes,{}},{Heap2,Heap2+PcRecordTable2Bytes,{}},
        {Heap3,Heap3+PcRecordTable2Bytes,{}},{Heap4,Heap4+PcRecordTable2Bytes,{}},{PathBuffer,PathBuffer+0x100u,{}}};
    std::vector<std::uint8_t> save(PcRecordBytes),filerec(PcRecordBytes);
    for(unsigned iter=0;iter<count;++iter){
        const unsigned entry=iter%6u;
        std::memset(reinterpret_cast<void*>(trace),0,0x10000);
        const std::uint32_t slot=rnd(3)?rnd(0x200):0xffffffffu;
        const unsigned save_kind=rnd(3),file_kind=rnd(3);const bool file_open=rnd(3)!=0u;
        make_record(save,save_kind);make_record(filerec,file_kind);
        std::memcpy(reinterpret_cast<void*>(SaveRecord),save.data(),save.size());std::memcpy(reinterpret_cast<void*>(FileRecord),filerec.data(),filerec.size());
        p32(values+4u,slot);p32(values+12u,file_open?1u:0u);p32(values+16u,PcRecordBytes);
        p32(malloc_counter,0);p32(malloc_list,Heap3);p32(malloc_list+4u,Heap4);
        for(auto& mi:mirrors)for(std::uint32_t a=mi.base;a<mi.end;a+=4)p32(a,rnd(3)?rnd(4):rng());
        p32(0x78024cu,rnd(4)?rnd(4):rng());
        if(entry<3u){const unsigned t=rnd(4);p32(0x81373cu,t&1u?Heap1:0u);p32(0x813740u,t&2u?Heap2:0u);}
        else{p32(0x81373cu,Heap1);p32(0x813740u,Heap2);}
        // level 0..2 (nml / tn1 / tn2) and kind 0..1 (_a / _m) index the sprintf string tables; the
        // upper bytes of the pushes are left-overs (only bl / the low byte are read).
        const std::uint32_t a=rnd(3)?rnd(4):rng(),b=(rng()&0xffffff00u)|rnd(3),k=(rng()&0xffffff00u)|rnd(2);
        const std::uint32_t index=rnd(15),half=rnd(2),media=rnd(2);
        const std::uint32_t rec=Heap2+rnd(30)*PcRecordBytes;
        for(auto& mi:mirrors){mi.bytes.resize(mi.end-mi.base);std::memcpy(mi.bytes.data(),reinterpret_cast<void*>(mi.base),mi.bytes.size());}
        static constexpr std::uint32_t Pc[]{0x47eca0u,0x480ad0u,0x480fe0u,0x480130u,0x480bc0u,0x481230u};
        prepare(Pc[entry]);
        auto* S32=reinterpret_cast<unsigned*>(S);
        if(entry==1u){guest_call.ebx=b;S32[0]=k;}
        else if(entry==2u||entry==5u){S32[0]=a;S32[1]=b;S32[2]=k;}
        else if(entry==3u){guest_call.ebx=index;S32[0]=PathBuffer;S32[1]=std::uint32_t(std::int32_t(std::int8_t(b)));S32[2]=k;S32[3]=half;S32[4]=media;}
        else if(entry==4u){S32[0]=rec;S32[1]=index;S32[2]=b;S32[3]=k;S32[4]=half;}
        run();
        // native
        PcRaceMemory mem;std::vector<std::vector<std::uint8_t>> native(mirrors.size());
        for(std::size_t i=0;i<EmbeddedExeRangeCount;++i)mem.map_const(EmbeddedExeRanges[i].base,EmbeddedExeRanges[i].data,EmbeddedExeRanges[i].size);
        ghost_map_tables(mem);   // 449A80's CRC-16 words 5A27D8
        for(std::size_t i=0;i<mirrors.size();++i){native[i]=mirrors[i].bytes;mem.map(mirrors[i].base,native[i].data(),native[i].size());}
        std::array<std::uint8_t,0x40> mb{};outrun::driving::PcMatrixStack stack{outrun::driving::Bytes(mb.data(),mb.size())};
        struct NCall{std::uint32_t pc;decltype(PcRaceCall::args) a;std::array<std::uint8_t,0x40> text;};std::vector<NCall> calls;unsigned mallocs=0;
        PcRaceContext ctx{mem,stack,[&](const PcRaceCall& c)->std::uint32_t{
            NCall n{c.pc,c.args,{}};
            if(c.pc==0x4239c0u)for(unsigned i=0;i<0x40u;++i)n.text[i]=mem.u8(c.args[0]+i);
            calls.push_back(n);
            switch(c.pc){
            case 0x580253u:return mallocs++==0u?Heap3:Heap4;
            case 0x416700u:return slot;
            case 0x416930u:for(std::uint32_t i=0;i<PcRecordBytes;++i)mem.put8(c.args[0]+i,save[i]);return 0u;
            case 0x4239c0u:return file_open?1u:0u;
            case 0x423f10u:return PcRecordBytes;
            case 0x423cb0u:for(std::uint32_t i=0;i<PcRecordBytes;++i)mem.put8(c.args[0]+i,filerec[i]);return 0u;
            case 0x423bd0u:case 0x416830u:return 0u;
            default:throw std::runtime_error("race records probe: unexpected service");
            }},nullptr};
        std::string why;
        try{
            switch(entry){
            case 0:records_tables_47eca0(ctx);break;
            case 1:records_request_480ad0(ctx,std::uint8_t(b),k);break;
            case 2:records_init_480fe0(ctx,a,b,k);break;
            case 3:records_path_480130(ctx,PathBuffer,std::uint32_t(std::int32_t(std::int8_t(b))),k,half,media,index);break;
            case 4:records_load_480bc0(ctx,rec,index,b,k,half);break;
            default:records_load_481230(ctx,a,b,k);break;
            }
        }catch(const std::exception& e){why=e.what();}
        std::ostringstream tg;tg<<"entry "<<entry<<" run "<<iter;const std::string tag=tg.str();
        ++cases;if(!why.empty()){fail(tag+" native threw: "+why);continue;}
        if(inject&&iter==5u)native[0][0x11]^=1u;
        for(std::size_t i=0;i<mirrors.size();++i){
            ++cases;const auto* g=reinterpret_cast<const std::uint8_t*>(mirrors[i].base);
            for(std::size_t o=0;o<native[i].size();++o)if(g[o]!=native[i][o]){
                std::ostringstream s;s<<tag<<" region "<<std::hex<<mirrors[i].base<<" offset 0x"<<o<<" PC="<<unsigned(g[o])<<" native="<<unsigned(native[i][o]);fail(s.str());break;}
        }
        const unsigned n=g32(trace);++cases;
        bool same=n==calls.size();std::string first;
        for(unsigned i=0;same&&i<n;++i){
            const auto* g=reinterpret_cast<const std::uint32_t*>(trace+64+i*0x60);
            same=g[0]==calls[i].pc;
            unsigned argc=0;for(const auto& e:services)if(e.pc==g[0])argc=e.argc;
            for(unsigned j=0;same&&j<argc;++j){
                if(g[0]==0x4239c0u&&j==0u){   // the path: a PC stack buffer; its text is compared
                    same=std::memcmp(reinterpret_cast<const std::uint8_t*>(trace+64+i*0x60+0x20),calls[i].text.data(),
                        std::strlen(reinterpret_cast<const char*>(calls[i].text.data()))+1)==0;
                    continue;}
                same=g[2+j]==calls[i].a[j];
            }
            if(!same){std::ostringstream s;s<<std::hex<<" at #"<<i<<" pc "<<g[0];first=s.str();}
        }
        if(!same){std::ostringstream s;s<<tag<<" service log differs (PC "<<n<<" calls, native "<<calls.size()<<")"<<first;fail(s.str());}
        ++coverage["entry "+std::to_string(entry)];
        if(entry==4u)++coverage["480BC0 save "+std::to_string(save_kind)+" file "+(file_open?std::to_string(file_kind):std::string("absent"))];
    }
    std::cout<<"{\"routine\":\""<<only<<"\",\"coverage\":{";bool first=true;
    for(const auto& [k,v]:coverage){std::cout<<(first?"":",")<<"\""<<k<<"\":"<<v;first=false;}
    std::cout<<"},\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}"<<std::endl;
    return mismatches?1:0;
}

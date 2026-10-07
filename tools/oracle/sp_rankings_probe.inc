// Included inside the compare_pc dispatcher; uses its private x86 harness.
if(only=="sp_rankings_probe"){
    // OutRun2SP rankings (sp_rankings.cpp) against the original over random
    // books / tables / stats: 0 4F3CC0 book->tables, 1 4F3AD0 tables->book,
    // 2 4F3EA0 defaults, 3 453210 stats, 4 452B80 header, 5 453440 boot init.
    using namespace outrun::platform;
    map_original_arithmetic_bridge(file);
    for(std::uint32_t page=0x62f000u;page<0x98d000u;page+=0x1000u)mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE);
    std::mt19937 rng(0x5b0a0001u);
    unsigned cases{},mismatches{};const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
    struct Region{std::uint32_t base,size;};
    const Region regions[]{{SpBookBase,SpBookSize},{SpStatsBase,SpStatsSize},{SpTablesBase,SpTablesEnd-SpTablesBase}};
    std::map<std::string,unsigned> coverage;
    static constexpr std::uint32_t Pc[]{0x4f3cc0u,0x4f3ad0u,0x4f3ea0u,0x453210u,0x452b80u,0x453440u,0x452d20u,0x452cd0u,0x452ed0u};
    for(unsigned iter=0;iter<count;++iter){
        const unsigned entry=iter%9u;
        // word arguments with random upper halves (indexes kept inside the book)
        const std::uint32_t a0=(rng()&0xffff0000u)|(entry==8u?rng()&0xffffu:(entry==6u?rng()%0xafcu:rng()%12u));
        const std::uint32_t a1=entry==6u?(rng()%3u?rng()%100000u:rng()):entry==7u?(rng()&0xffff0000u)|(rng()%0xe0u):rng();
        std::vector<std::vector<std::uint8_t>> native;
        for(const auto& r:regions){
            std::vector<std::uint8_t> b(r.size);for(auto& x:b)x=std::uint8_t(rng());
            std::memcpy(reinterpret_cast<void*>(r.base),b.data(),b.size());native.push_back(std::move(b));
        }
        if(entry==6u&&rng()%3u){   // plausible counts / times as well as raw random ones
            const std::uint32_t n=rng()%4u?1u+rng()%500u:rng();std::memcpy(native[1].data()+0x3c,&n,4);
            const std::uint32_t t=rng()%2u?rng()%100000u:rng();std::memcpy(native[0].data()+0xa0+(a0&0xffffu)*4u,&t,4);
            std::memcpy(reinterpret_cast<void*>(regions[0].base),native[0].data(),native[0].size());
            std::memcpy(reinterpret_cast<void*>(regions[1].base),native[1].data(),native[1].size());
        }
        *reinterpret_cast<std::uint32_t*>(0x63960cu)=SpBookBase;
        PcRaceMemory mem;
        for(std::size_t i=0;i<native.size();++i)mem.map(regions[i].base,native[i].data(),native[i].size());
        std::uint32_t book_cell=SpBookBase;mem.map(0x63960cu,reinterpret_cast<std::uint8_t*>(&book_cell),4);
        mem.map_const(0x5da318u,reinterpret_cast<const std::uint8_t*>(0x5da318u),0x30);
        std::string why;
        try{
            switch(entry){
            case 0:sp_tables_from_book_4f3cc0(mem);break;
            case 1:sp_book_from_tables_4f3ad0(mem);break;
            case 2:sp_tables_defaults_4f3ea0(mem);break;
            case 3:sp_stats_from_book_453210(mem);break;
            case 4:sp_book_header_452b80(mem);break;
            case 5:sp_book_init_453440(mem);break;
            case 6:sp_stats_average_452d20(mem,a0,a1);break;
            case 7:sp_stats_count_452cd0(mem,a0,a1);break;
            default:sp_stats_set_452ed0(mem,a0,a1);break;
            }
        }catch(const std::exception& e){why=e.what();}
        prepare(Pc[entry]);reinterpret_cast<unsigned*>(S)[0]=a0;reinterpret_cast<unsigned*>(S)[1]=a1;run();
        ++cases;
        if(!why.empty()){++mismatches;if(mismatches<10)std::cerr<<"entry "<<entry<<" native threw: "<<why<<'\n';continue;}
        if(inject&&iter==7)native[0][0x123]^=1;
        for(std::size_t i=0;i<native.size();++i){
            ++cases;
            if(!std::memcmp(native[i].data(),reinterpret_cast<void*>(regions[i].base),native[i].size()))continue;
            ++mismatches;
            for(std::size_t o=0;o<native[i].size();++o){const auto pcb=*reinterpret_cast<std::uint8_t*>(regions[i].base+o);
                if(pcb!=native[i][o]){if(mismatches<10)std::cerr<<"entry "<<entry<<" run "<<iter<<" region "<<std::hex<<regions[i].base<<" offset 0x"<<o<<" PC="<<unsigned(pcb)<<" native="<<unsigned(native[i][o])<<std::dec<<'\n';break;}}
        }
        ++coverage["entry "+std::to_string(entry)];
    }
    std::cout<<"{\"routine\":\""<<only<<"\",\"coverage\":{";bool first=true;
    for(const auto& [k,v]:coverage){std::cout<<(first?"":",")<<"\""<<k<<"\":"<<v;first=false;}
    std::cout<<"},\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}"<<std::endl;
    return mismatches?1:0;
}

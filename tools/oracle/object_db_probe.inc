if(only=="object_db_probe"){
    // Original object name database 448B90 (build, state 1) and 448CD0
    // lookups against object_db_build_448b90 / object_db_find_448b10, over
    // the retail Common/object_db_bin.sz payload (OR2_OBJECT_DB = the
    // inflated file) and synthetic files. 44F880 returns 1, 44FC60 hands
    // out the file address, 580BC2 is a RET. Compared: 7CC1E0 table, count
    // 7D25E0, the flag 7D25E4, 7C27F0/7C27F4 and every lookup result.
    using namespace outrun::platform;
    const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
    map_original_arithmetic_bridge(file);   // 580CB0 qsort enters through jmp [1039C00]
    for(std::uint32_t page=0x62f000u;page<0x98d000u;page+=0x1000u)mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE);
    constexpr std::uint32_t File=0x3a800000u,Stubs=0x3a850000u,Name=0x3a851000u;
    map_at(File,0x40000,PROT_READ|PROT_WRITE);map_at(Stubs,0x2000,PROT_READ|PROT_WRITE|PROT_EXEC);
    auto g32=[](std::uint32_t a){return *reinterpret_cast<const std::uint32_t*>(a);};
    auto p32=[](std::uint32_t a,std::uint32_t v){*reinterpret_cast<std::uint32_t*>(a)=v;};
    {
        const std::uint8_t one[]{0xb8,1,0,0,0,0xc3};
        std::uint8_t take[]{0x8b,0x44,0x24,0x08,0xc7,0x00,0,0,0,0,0xc3};std::memcpy(take+6,&File,4);
        std::memcpy(reinterpret_cast<void*>(Stubs),one,sizeof one);std::memcpy(reinterpret_cast<void*>(Stubs+0x40),take,sizeof take);
        *reinterpret_cast<std::uint8_t*>(Stubs+0x80)=0xc3;
        // 5822D2 toupper in the "C" locale (the CRT per-thread data 585BEC is
        // not set up in the oracle): 'a'..'z' -> -0x20, other ints unchanged.
        const std::uint8_t up[]{0x8b,0x44,0x24,0x04,0x83,0xf8,0x61,0x7c,0x08,0x83,0xf8,0x7a,0x7f,0x03,0x83,0xe8,0x20,0xc3};
        std::memcpy(reinterpret_cast<void*>(Stubs+0xc0),up,sizeof up);r046_write_rel32(0x5822d2u,0xe9,Stubs+0xc0);
        r046_write_rel32(0x44f880u,0xe9,Stubs);r046_write_rel32(0x44fc60u,0xe9,Stubs+0x40);r046_write_rel32(0x580bc2u,0xe9,Stubs+0x80);
    }
    std::vector<std::uint8_t> retail;
    if(const char* path=std::getenv("OR2_OBJECT_DB")){std::ifstream f(path,std::ios::binary);retail.assign(std::istreambuf_iterator<char>(f),{});}
    if(retail.size()>4)retail.erase(retail.begin(),retail.begin()+4);
    std::mt19937 rng(0x448b90u);
    const unsigned cases=std::max(count,1u)*4u;
    unsigned mismatches=0,lookups=0,refused=0;
    auto report=[&](const std::string& what){++mismatches;if(mismatches<=12)std::cerr<<"MISMATCH "<<what<<char(10);};
    const std::string alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZ_0123456789abcxyz";
    for(unsigned cs=0;cs<cases;++cs){
        const std::string tag=" case "+std::to_string(cs);
        std::vector<std::uint8_t> file;std::vector<std::string> names;
        const bool use_retail=cs%4u==0u&&!retail.empty();
        if(use_retail){
            file=retail;
            for(unsigned g=0;g<43;++g){std::uint32_t e;std::memcpy(&e,file.data()+g*4,4);
                for(;;e+=8){std::uint32_t h,o;std::memcpy(&o,file.data()+e,4);std::memcpy(&h,file.data()+e+4,4);if(h==0xffffffffu)break;
                    names.emplace_back(reinterpret_cast<const char*>(file.data()+o));}}
        }else{
            file.assign(0x20000,0);std::uint32_t cur=0xb0;
            auto w=[&](std::uint32_t o,std::uint32_t v){std::memcpy(file.data()+o,&v,4);};
            std::uint32_t text=0x10000;
            for(unsigned g=0;g<43;++g){
                w(g*4,cur);const unsigned n=rng()%6u==0u?0u:rng()%20u;
                for(unsigned i=0;i<n;++i){
                    std::string s;const unsigned len=1u+rng()%14u;for(unsigned k=0;k<len;++k)s.push_back(alphabet[rng()%alphabet.size()]);
                    std::memcpy(file.data()+text,s.c_str(),s.size()+1);
                    w(cur,text);w(cur+4,(rng()%200u)<<16|(rng()%80u));cur+=8;text+=std::uint32_t(s.size())+1u;names.push_back(s);
                }
                w(cur,0);w(cur+4,0xffffffffu);cur+=8;
            }
        }
        // Guest: previous table (the PC appends) and state 1.
        PcObjectDb db;
        const std::uint32_t pre=cs%3u==2u?std::uint32_t(rng()%40u):0u;
        std::memset(reinterpret_cast<void*>(0x7cc1e0u),0,0x6400);
        for(std::uint32_t i=0;i<pre;++i){const std::uint32_t h=rng(),v=rng();p32(0x7cc1e0u+i*8,h);p32(0x7cc1e4u+i*8,v);db.table_7cc1e0[i]={std::int32_t(h),v};}
        p32(0x7d25e0u,pre);db.count_7d25e0=pre;*reinterpret_cast<std::uint8_t*>(0x7d25e4u)=1;db.sorted_7d25e4=true;
        p32(0x7c27f4u,1);p32(0x7c27f0u,0x1234);db.state_7c27f4=1;db.slot_7c27f0=0x1234;
        std::memcpy(reinterpret_cast<void*>(File),file.data(),file.size());
        prepare(0x448b90);run_original32();
        try{object_db_build_448b90(db,outrun::driving::Bytes(file.data(),file.size()));}
        catch(const std::exception& e){report(std::string("native build threw: ")+e.what()+tag);continue;}
        if(inject&&cs==5u)db.table_7cc1e0[1].handle^=1u;
        if(g32(0x7d25e0u)!=db.count_7d25e0)report("count"+tag);
        if(g32(0x7c27f4u)!=db.state_7c27f4||g32(0x7c27f0u)!=db.slot_7c27f0)report("loader state"+tag);
        if(*reinterpret_cast<std::uint8_t*>(0x7d25e4u)!=(db.sorted_7d25e4?1u:0u))report("sorted flag"+tag);
        for(std::uint32_t i=0;i<db.count_7d25e0;++i)if(std::int32_t(g32(0x7cc1e0u+i*8))!=db.table_7cc1e0[i].hash||g32(0x7cc1e4u+i*8)!=db.table_7cc1e0[i].handle){
            report("table entry "+std::to_string(i)+tag);break;}
        // Lookups: known names (some lower-cased) and random ones.
        for(unsigned q=0;q<60;++q){
            std::string s;
            if(!names.empty()&&q%3u!=2u){s=names[rng()%names.size()];if(q%5u==0u)for(auto& ch:s)ch=char(std::tolower(static_cast<unsigned char>(ch)));}
            else{const unsigned len=rng()%8u;for(unsigned k=0;k<len;++k)s.push_back(alphabet[rng()%alphabet.size()]);}
            std::memcpy(reinterpret_cast<void*>(Name),s.c_str(),s.size()+1);
            prepare(0x448cd0);reinterpret_cast<unsigned*>(S)[0]=Name;run_original32();const std::uint32_t guest=guest_call.out_eax;
            std::uint32_t native=0;
            try{native=object_db_find_448b10(db,s);}catch(const std::exception&){++refused;continue;}   // ambiguous hash: refused
            ++lookups;
            if(guest!=native){std::ostringstream o;o<<"lookup "<<s<<" guest "<<std::hex<<guest<<" native "<<native;report(o.str()+tag);break;}
        }
        if(*reinterpret_cast<std::uint8_t*>(0x7d25e4u)!=(db.sorted_7d25e4?1u:0u))report("sorted flag after lookups"+tag);
    }
    std::cerr<<"object db: lookups="<<lookups<<" refused="<<refused<<char(10);
    std::cout<<"{\"routine\":\"object_db_probe\",\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}"<<'\n';return 0;
}

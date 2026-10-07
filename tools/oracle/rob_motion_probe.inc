// Included inside the compare_pc dispatcher; uses its private x86 harness.
if(only=="rob_motion_probe"){
    // Original RobMotion system init 4F2470 (with the file loader 4F1F90, the
    // motion-table relocation 4F1F30, the empty 440D90 and the CRT strrchr
    // 581090 running as original code) against rob_motion_tables. The file
    // and heap services (4239C0 open, 423F10 size, 423CB0 read, 423BD0 close,
    // 580253 malloc) and the allocator selector stacks 440D10/440D30/440D50/
    // 440D70 are capture stubs recording (entry, stack arguments). open and
    // malloc return a scripted per-case value that advances by a per-case
    // stride on each call, size reads the word before the handle, read copies
    // the fixture file bytes (handle = source address) into the buffer. The
    // native services do the same. Compared: the .bss block 84D970..84DE74,
    // the fixture region (files and heap buffers), the ordered service calls
    // with their arguments and EAX (4F1F90, 581090).
    // Entries: 0 4F2470, 1 4F1F90 (including a failing open), 2 4F1F30,
    // 3 581090. With OR2_ROB_MOTION_RETAIL=<game dir> case 0 also loads the
    // retail Common\bone.bin / Common\motdata_table.bin.
    using namespace outrun::platform;
    constexpr unsigned base=0x3a800000u,strace=base+0x10000u,values=base+0x30000u;
    constexpr unsigned fix=base+0x40000u,fix_size=0x30000u,stubs=base;
    constexpr unsigned SvcStride=0x20u,SvcCap=0x1000u;
    map_at(base,0x80000,PROT_READ|PROT_WRITE|PROT_EXEC);map_original_arithmetic_bridge(file);
    {   unsigned restored=0;
        const std::size_t text_file=pe_file_offset(file,0x401000u);
        for(std::uint32_t a=0x401000u;a<0x401000u+0x194881u;a+=0x100u){
            const std::size_t n=std::min<std::size_t>(0x100u,0x401000u+0x194881u-a);
            if(std::memcmp(reinterpret_cast<void*>(a),file.data()+text_file+(a-0x401000u),n)){r060_raw_write(a,file.data()+text_file+(a-0x401000u),n);++restored;}
        }
        std::cerr<<"restored "<<restored<<" patched .text blocks"<<char(10);
    }
    if(mprotect(reinterpret_cast<void*>(0x84d000u),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("rob motion globals");
    auto emit32=[](std::vector<std::uint8_t>& b,std::uint32_t v){for(unsigned k=0;k<4;++k)b.push_back(std::uint8_t(v>>(k*8)));};
    struct Svc{std::uint32_t pc;unsigned argc;int kind;};   // kind 0 value, 1 value+stride, 2 size, 3 read
    const std::vector<Svc> services{{0x4239c0,2,1},{0x423f10,1,2},{0x440d10,1,0},{0x440d50,1,0},{0x580253,1,1},{0x440d30,0,0},{0x440d70,0,0},{0x423cb0,4,3},{0x423bd0,1,0}};
    if(services.size()!=sizeof PcRobMotionServices/sizeof PcRobMotionServices[0])throw std::runtime_error("rob motion service list");
    for(std::size_t k=0;k<services.size();++k)if(services[k].pc!=PcRobMotionServices[k])throw std::runtime_error("rob motion service order");
    std::map<std::uint32_t,unsigned> argc_of,index_of;
    unsigned stub_off=0;
    for(std::size_t k=0;k<services.size();++k){
        const auto& e=services[k];argc_of[e.pc]=e.argc;index_of[e.pc]=unsigned(k);
        const unsigned value=values+8u*unsigned(k),stride=value+4u;
        std::vector<std::uint8_t> c{0x52,0x8b,0x15};emit32(c,strace);                          // push edx; mov edx,[count]
        c.insert(c.end(),{0xc1,0xe2,0x05,0xc7,0x82});emit32(c,strace+16);emit32(c,e.pc);        // shl edx,5; mov [edx+t+16],pc
        for(unsigned i=0;i<4;++i){
            if(i<e.argc)c.insert(c.end(),{0x8b,0x4c,0x24,std::uint8_t(8+4*i)});else c.insert(c.end(),{0x31,0xc9});
            c.insert(c.end(),{0x89,0x8a});emit32(c,strace+20+4*i);}
        c.insert(c.end(),{0xff,0x05});emit32(c,strace);                                         // inc [count]
        c.push_back(0x5a);                                                                       // pop edx
        switch(e.kind){
        case 0:c.push_back(0xa1);emit32(c,value);break;                                          // mov eax,[value]
        case 1:c.push_back(0xa1);emit32(c,value);c.insert(c.end(),{0x8b,0x0d});emit32(c,stride); // mov ecx,[stride]
               c.insert(c.end(),{0x01,0x0d});emit32(c,value);break;                              // add [value],ecx
        case 2:c.insert(c.end(),{0x8b,0x44,0x24,0x04,0x8b,0x40,0xfc});break;                     // mov eax,[esp+4]; mov eax,[eax-4]
        default:c.insert(c.end(),{0x56,0x57,0x8b,0x7c,0x24,0x0c,0x8b,0x74,0x24,0x18,0x8b,0x4c,0x24,0x10,
                                  0xf3,0xa4,0x5f,0x5e,0x8b,0x44,0x24,0x0c});break;              // rep movsb buf<-handle (size bytes), eax=count
        }
        c.push_back(0xc3);
        if(stub_off+c.size()>0x10000)throw std::runtime_error("rob motion stubs");
        std::memcpy(reinterpret_cast<void*>(stubs+stub_off),c.data(),c.size());
        r046_write_rel32(e.pc,0xe9,stubs+stub_off);stub_off=(stub_off+unsigned(c.size())+15u)&~15u;
    }
    std::mt19937 rng(0x4f247000u);
    auto rnd=[&](std::uint32_t n){return n?std::uint32_t(rng()%n):0u;};
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
    for(const auto& t:RobMotionRdata)compare(t.data,reinterpret_cast<void*>(t.base),t.size,"rob motion .rdata string");
    // retail files (optional)
    std::vector<std::uint8_t> retail_bone,retail_table;
    if(const char* dir=std::getenv("OR2_ROB_MOTION_RETAIL")){
        auto slurp=[](const std::string& p){std::ifstream f(p,std::ios::binary);return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(f)),std::istreambuf_iterator<char>());};
        retail_bone=slurp(std::string(dir)+"/Common/bone.bin");retail_table=slurp(std::string(dir)+"/Common/motdata_table.bin");
        if(retail_bone.empty()||retail_table.empty())throw std::runtime_error("OR2_ROB_MOTION_RETAIL: files not found");
    }
    // Fixture layout: SRC0 (bone file) FIX+0x100 (<= 0xB000), SRC1 (table
    // file) FIX+0xC000 (<= 0x4000), heap FIX+0x10000.. (buffers 0xC000 apart).
    const unsigned SRC0=fix+0x100u,SRC1=fix+0xc000u,HEAP=fix+0x10000u,SCRATCH=fix+0x2c000u;
    // Synthetic motion table image (file offsets): n entries, terminator,
    // lists, names "mot_<k>_bin.gz" (some without '.').
    auto make_table=[&](unsigned n,bool terminated){
        std::vector<std::uint32_t> words;std::vector<std::uint8_t> img((n+1u)*16u,0);
        auto w32=[&](std::size_t o,std::uint32_t v){if(img.size()<o+4)img.resize(o+4);std::memcpy(img.data()+o,&v,4);};
        std::vector<std::uint32_t> list_off(n),name_off(n),count(n);
        for(unsigned e=0;e<n;++e){
            const unsigned kind=rnd(8);
            count[e]=kind==0?0u:kind==1?0xffffffffu:1u+rnd(9);
            if(kind==2)list_off[e]=0u;
            else{list_off[e]=std::uint32_t(img.size());const unsigned m=std::int32_t(count[e])>0?count[e]:rnd(3);
                for(unsigned k=0;k<m;++k)w32(img.size(),rng()&0xffffu);}
        }
        for(unsigned e=0;e<n;++e){
            name_off[e]=std::uint32_t(img.size());
            std::string s="mot_"+std::to_string(rng()%100000)+(rnd(6)?"_bin.gz":"_bin");
            if(rnd(10)==0)s+=".x.y";
            for(char ch:s)img.push_back(std::uint8_t(ch));img.push_back(0);
        }
        while(img.size()&3u)img.push_back(0);
        for(unsigned e=0;e<n;++e){w32(e*16u,name_off[e]);w32(e*16u+4,list_off[e]);w32(e*16u+8,count[e]);w32(e*16u+12,rng());}
        w32(n*16u,terminated?0u:name_off[0]);w32(n*16u+4,0);w32(n*16u+8,0);w32(n*16u+12,rng());
        if(n==0)w32(0,0);
        return img;
    };
    struct Mirror{std::uint32_t base,end;std::vector<std::uint8_t> bytes;};
    std::vector<Mirror> mirrors{{PcRobMotionTablesState::Base,PcRobMotionTablesState::End,{}},{fix,fix+fix_size,{}}};
    for(unsigned run=0;run<count;++run){
        unsigned entry=run%4u;
        if(const char* only_entry=std::getenv("OR2_ROB_MOTION_ENTRY"))entry=unsigned(std::stoul(only_entry));
        for(unsigned k=0;k<fix_size;k+=4)p32(fix+k,rng());
        for(std::uint32_t a=PcRobMotionTablesState::Base;a<PcRobMotionTablesState::End;a+=4)p32(a,rnd(2)?0u:rng());
        std::array<std::uint32_t,2*9> value{};for(auto& v:value)v=rng();
        auto set=[&](std::uint32_t pc,std::uint32_t v,std::uint32_t stride){value[2*index_of[pc]]=v;value[2*index_of[pc]+1]=stride;};
        const bool real=entry==0&&run==0&&!retail_bone.empty();
        std::vector<std::uint8_t> bone,table;
        if(real){bone=retail_bone;table=retail_table;}
        else{bone.resize(0x100u+rnd(0xa000u)&~3u);for(auto& b:bone)b=std::uint8_t(rng());table=make_table(63u+rnd(4),true);}
        const std::uint32_t heap=HEAP+rnd(0x100)*4u;
        std::uint32_t arg0{},arg1{};std::uint8_t ch{};
        switch(entry){
        case 0:   // files: bone at SRC0, table at SRC1
            std::memcpy(reinterpret_cast<void*>(SRC0),bone.data(),bone.size());p32(SRC0-4,std::uint32_t(bone.size()));
            std::memcpy(reinterpret_cast<void*>(SRC1),table.data(),table.size());p32(SRC1-4,std::uint32_t(table.size()));
            set(0x4239c0,SRC0,SRC1-SRC0);set(0x580253,heap,0xc000u);
            coverage[real?"init_retail":"init"]++;
            break;
        case 1:{  // one load; open fails in 1 of 4
            const bool first=rnd(2);arg0=first?0x5da304u:0x5da2e8u;
            arg1=rnd(2)?(first?0x84de68u:0x84de6cu):SCRATCH+rnd(0x100)*4u;
            std::memcpy(reinterpret_cast<void*>(SRC0),bone.data(),bone.size());p32(SRC0-4,std::uint32_t(bone.size()));
            set(0x4239c0,rnd(4)?SRC0:0u,SRC1-SRC0);set(0x580253,heap,0xc000u);
            coverage[value[2*index_of[0x4239c0]]?"load":"load_fail"]++;
            break;}
        case 2:{  // relocation of a table at a random base
            const unsigned n=rnd(8)==0?0u:1u+rnd(80);
            table=make_table(n,true);arg0=heap;
            std::memcpy(reinterpret_cast<void*>(heap),table.data(),table.size());
            coverage[n?"relocate":"relocate_empty"]++;
            break;}
        default:{ // strrchr on a NUL-terminated string
            const std::uint32_t s=SCRATCH+rnd(0x40);const unsigned len=rnd(40);
            for(unsigned k=0;k<len;++k)*reinterpret_cast<std::uint8_t*>(s+k)=std::uint8_t(rnd(4)?'a'+rnd(20):rnd(256)|1u);
            *reinterpret_cast<std::uint8_t*>(s+len)=0;
            {const std::uint8_t cs[]={'.',0,'a','z',0x80,0xff};ch=rnd(3)?cs[rnd(6)]:std::uint8_t(rng());}
            arg0=s;arg1=ch;coverage["strrchr"]++;
            break;}
        }
        for(std::size_t k=0;k<value.size();++k)p32(values+4u*unsigned(k),value[k]);
        for(auto& m:mirrors){m.bytes.resize(m.end-m.base);std::memcpy(m.bytes.data(),reinterpret_cast<void*>(m.base),m.bytes.size());}
        p32(strace,0);
        auto* st=reinterpret_cast<unsigned*>(S);
        switch(entry){
        case 0:prepare(0x4f2470);break;
        case 1:prepare(0x4f1f90);st[0]=arg0;st[1]=arg1;break;
        case 2:prepare(0x4f1f30);st[0]=arg0;break;
        default:prepare(0x581090);st[0]=arg0;st[1]=arg1;break;
        }
        run_original32();const std::uint32_t original_eax=guest_call.out_eax;
        const std::string tag="rob motion entry "+std::to_string(entry)+" run "+std::to_string(run);
        if(guest_call.out_sp!=S){fail(tag+" guest stack imbalance");++cases;continue;}
        // native
        PcRaceMemory mem;
        std::vector<std::vector<std::uint8_t>> native(mirrors.size());
        for(std::size_t i=0;i<mirrors.size();++i){native[i]=mirrors[i].bytes;mem.map(mirrors[i].base,native[i].data(),native[i].size());}
        rob_motion_map_rdata(mem);
        struct NCall{std::uint32_t pc;decltype(PcRaceCall::args) a;unsigned argc;};
        std::vector<NCall> calls;
        auto nvalue=value;
        PcRaceService service=[&](const PcRaceCall& c)->std::uint32_t{
            if(!argc_of.count(c.pc)){std::ostringstream e;e<<"native called an unstubbed service "<<std::hex<<c.pc;throw std::runtime_error(e.str());}
            calls.push_back({c.pc,c.args,c.argc});
            const unsigned k=index_of[c.pc];
            switch(services[k].kind){
            case 0:return nvalue[2*k];
            case 1:{const std::uint32_t v=nvalue[2*k];nvalue[2*k]+=nvalue[2*k+1];return v;}
            case 2:return mem.u32(c.args[0]-4u);
            default:for(std::uint32_t i=0;i<c.args[1];++i)mem.put8(c.args[0]+i,mem.u8(c.args[3]+i));return c.args[2];
            }
        };
        std::uint32_t native_eax=0;
        try{
            switch(entry){
            case 0:rob_motion_init_4f2470(mem,service);break;
            case 1:native_eax=rob_motion_load_file_4f1f90(mem,service,arg0,arg1);break;
            case 2:rob_motion_relocate_4f1f30(mem,arg0);break;
            default:native_eax=pc_crt_strrchr_581090(mem,arg0,ch);break;
            }
        }catch(const std::exception& ex){++cases;fail(tag+" native threw: "+ex.what());continue;}
        ++coverage["entry_"+std::to_string(entry)];
        if(entry==0)coverage["table_entries"]+=g32(0x84de70u);
        if(entry==3)coverage[original_eax?"strrchr_found":"strrchr_null"]++;
        if(entry==1||entry==3){++cases;if(native_eax!=original_eax){std::ostringstream s;s<<tag<<" EAX PC="<<std::hex<<original_eax<<" native="<<native_eax;fail(s.str());}}
        if(inject&&run==count-1){native[0][0x4]^=1;}
        for(std::size_t i=0;i<mirrors.size();++i){
            std::ostringstream s;s<<tag<<" region "<<std::hex<<mirrors[i].base;
            compare(native[i].data(),reinterpret_cast<void*>(mirrors[i].base),native[i].size(),s.str());
        }
        const unsigned n=g32(strace);++cases;
        if(n>SvcCap)throw std::runtime_error("service trace overflow");
        coverage["service_calls"]+=n;
        if(n!=calls.size()){std::ostringstream s;s<<tag<<" service count PC="<<n<<" native="<<calls.size();fail(s.str());}
        else for(unsigned i=0;i<n;++i){
            const unsigned at=strace+16+i*SvcStride;const auto& c=calls[i];
            bool ok=g32(at)==c.pc&&c.argc==argc_of[c.pc];
            for(unsigned a=0;ok&&a<c.argc;++a)ok=g32(at+4+4*a)==c.a[a];
            if(!ok){std::ostringstream s;s<<tag<<" service "<<i<<" PC "<<std::hex<<g32(at)<<" ("<<g32(at+4)<<","<<g32(at+8)<<","<<g32(at+12)<<","<<g32(at+16)<<") native "<<c.pc<<" ("<<c.a[0]<<","<<c.a[1]<<","<<c.a[2]<<","<<c.a[3]<<")";fail(s.str());break;}
        }
    }
    std::cerr<<"coverage:";for(const auto& [k,v]:coverage)std::cerr<<' '<<k<<'='<<v;std::cerr<<"\n";
    std::cout<<"{\"routine\":\""<<only<<"\",\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}"<<'\n';return mismatches?1:0;
}

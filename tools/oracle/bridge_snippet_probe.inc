if(only=="bridge_snippet_probe"){
    // Black-box observation of a protected snippet inside a function: the
    // original runs from the function entry OR2_ENTRY through its protected
    // bridge; the continuation OR2_CONT is patched to a capture stub that
    // records every register and EFLAGS, then returns to the harness.
    // OR2_SNIPPET selects the per-bridge input set below.
    const char* e=std::getenv("OR2_ENTRY");const char* k=std::getenv("OR2_CONT");const char* which=std::getenv("OR2_SNIPPET");
    if(!e||!k||!which)throw std::runtime_error("set OR2_ENTRY, OR2_CONT and OR2_SNIPPET");
    const std::uint32_t entry=std::uint32_t(std::strtoul(e,nullptr,16)),cont=std::uint32_t(std::strtoul(k,nullptr,16));
    const std::string snippet=which;
    constexpr unsigned base=0x3a800000,buf=base+0x100,stub=base+0x200,obj=base+0x1000;
    map_at(base,0x10000,PROT_READ|PROT_WRITE|PROT_EXEC);map_original_arithmetic_bridge(file);
    for(std::uint32_t page=0x62f000u;page<0x98d000u;page+=0x1000u)mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE);
    {   std::vector<std::uint8_t> c;auto imm=[&](std::uint32_t v){for(unsigned i=0;i<4;++i)c.push_back(std::uint8_t(v>>(i*8)));};
        c.push_back(0xa3);imm(buf);
        const std::uint8_t regs[]{0x0d,0x15,0x1d,0x25,0x2d,0x35,0x3d};
        for(unsigned i=0;i<7;++i){c.push_back(0x89);c.push_back(regs[i]);imm(buf+4+i*4);}
        c.push_back(0x9c);c.push_back(0x8f);c.push_back(0x05);imm(buf+32);
        c.push_back(0xbc);imm(S-4);c.push_back(0xc3);
        std::memcpy(reinterpret_cast<void*>(stub),c.data(),c.size());
    }
    std::array<std::uint8_t,5> saved{};std::memcpy(saved.data(),reinterpret_cast<void*>(cont),5);
    r046_write_rel32(cont,0xe9,stub);
    std::mt19937 rng(0x5e1f0000u);
    auto regs=[&]{return reinterpret_cast<const std::uint32_t*>(buf);};
    auto dump=[&](const std::string& label){const auto* r=regs();
        std::cerr<<label<<std::hex<<" eax="<<r[0]<<" ecx="<<r[1]<<" edx="<<r[2]<<" ebx="<<r[3]<<" esp="<<r[4]<<" ebp="<<r[5]<<" esi="<<r[6]<<" edi="<<r[7]<<" fl="<<(r[8]&0x8d5)<<std::dec<<char(10);};
    if(snippet=="diff"){
        // The .data / .bss words the bridge writes: two runs from different fills.
        for(unsigned pass=0;pass<2;++pass){
            for(std::uint32_t a=0x62f000u;a<0x98d000u;a+=4)if(pass)*reinterpret_cast<std::uint32_t*>(a)=a;
            std::vector<std::uint32_t> before((0x98d000u-0x62f000u)/4);std::memcpy(before.data(),reinterpret_cast<void*>(0x62f000u),before.size()*4);
            prepare(entry);run_original32();
            dump(pass?"datafill":"image");unsigned shown=0;
            for(std::size_t i=0;i<before.size();++i){const auto now=*reinterpret_cast<const std::uint32_t*>(0x62f000u+i*4);
                if(now!=before[i]&&shown++<40)std::cerr<<std::hex<<"  ["<<(0x62f000u+i*4)<<"] "<<before[i]<<" -> "<<now<<std::dec<<char(10);}
        }
    }else if(snippet=="44cd30"){
        // 44CD30 (cube face env model) bridge 44CD63..44CD69 (7 bytes, then jne exit): which word is compared
        // with 0x32. Binary search over .data/.bss: a range filled with 0x32, the rest 0 (the cells below set).
        auto g=[](std::uint32_t a)->std::uint32_t&{return *reinterpret_cast<std::uint32_t*>(a);};
        auto run_with=[&](std::uint32_t lo,std::uint32_t hi,std::uint32_t fill)->bool{
            for(std::uint32_t a=0x62f000u;a<0x98d000u;a+=4)g(a)=(a>=lo&&a<hi)?fill:0u;
            for(std::uint32_t a=obj;a<obj+0x6000u;a+=4)g(a)=(a>=lo&&a<hi)?fill:0u;
            g(0x799d18u)=obj;g(obj+0x5cu)=1u;g(0x7d3188u)=obj+0x200u;g(obj+0x214u)=obj+0x300u;g(0x7d2e60u)=0x50u;if(std::getenv("OR2_E88"))g(0x7d2e88u)=std::uint32_t(std::strtoul(std::getenv("OR2_E88"),nullptr,16));
            g(0x89b564u)=MatrixArena;g(0x89b56cu)=64u;
            for(unsigned i=0;i<9;++i)reinterpret_cast<std::uint32_t*>(buf)[i]=0xdeadbeefu;
            prepare(entry);run_original32();
            return reinterpret_cast<const std::uint32_t*>(buf)[0]!=0xdeadbeefu;};
        init_matrix_oracle_globals();
        for(std::uint32_t v:{0x31u,0x32u,0x33u,0x40u}){
            for(std::uint32_t a=0x62f000u;a<0x98d000u;a+=4)g(a)=0u;for(std::uint32_t a=obj;a<obj+0x6000u;a+=4)g(a)=0u;
            g(0x799d18u)=obj;g(obj+0x5cu)=v;g(0x7d3188u)=obj+0x200u;g(obj+0x214u)=obj+0x300u;g(0x7d2e60u)=0x50u;g(0x89b564u)=MatrixArena;g(0x89b56cu)=64u;
            for(unsigned i=0;i<9;++i)reinterpret_cast<std::uint32_t*>(buf)[i]=0xdeadbeefu;
            prepare(entry);run_original32();std::cerr<<"car+5C="<<std::hex<<v<<std::dec<<" reached="<<(reinterpret_cast<const std::uint32_t*>(buf)[0]!=0xdeadbeefu)<<char(10);
            if(reinterpret_cast<const std::uint32_t*>(buf)[0]!=0xdeadbeefu)dump("  regs");
        }
        {   // OR2_CONT=44cd6a: binary search of the word that clears CF (X >= 0x32) when filled with 0x7f7f7f7f
            auto cf=[&](std::uint32_t lo,std::uint32_t hi){run_with(lo,hi,0x7f7f7f7fu);return (reinterpret_cast<const std::uint32_t*>(buf)[8]&1u)==0u;};
            std::cerr<<"cf-clear data="<<cf(0x62f000u,0x98d000u)<<" obj="<<cf(obj,obj+0x6000u)<<char(10);
            std::uint32_t lo=0,hi=0;
            if(cf(0x62f000u,0x98d000u)){lo=0x62f000u;hi=0x98d000u;}else if(cf(obj,obj+0x6000u)){lo=obj;hi=obj+0x6000u;}
            if(hi){while(hi-lo>4u){const std::uint32_t mid=lo+((hi-lo)/8u)*4u;if(cf(lo,mid))hi=mid;else lo=mid;}std::cerr<<std::hex<<"compared word "<<lo<<std::dec<<char(10);}
        }
        std::cerr<<"none="<<run_with(0,0,0)<<" all32="<<run_with(0x62f000u,0x98d000u,0x32u)<<" car32="<<run_with(obj,obj+0x6000u,0x32u)<<" car32b="<<run_with(obj,obj+0x1000u,0x32323232u)<<char(10);
        for(std::uint32_t fill:{0x32u,0x32323232u}){
            std::uint32_t lo=0x62f000u,hi=0x98d000u;
            if(!run_with(lo,hi,fill)){lo=obj;hi=obj+0x6000u;if(!run_with(lo,hi,fill))continue;}
            while(hi-lo>4u){const std::uint32_t mid=lo+((hi-lo)/8u)*4u;if(run_with(lo,mid,fill))hi=mid;else lo=mid;}
            std::cerr<<std::hex<<"fill "<<fill<<" word "<<lo<<std::dec<<char(10);
            run_with(lo,hi,fill);dump("captured");
        }
    }else if(snippet=="484810"){
        // Camera 484810 (eax = camera): which car/table the bridge selects.
        for(unsigned run=0;run<8;++run){
            std::memset(reinterpret_cast<void*>(obj),0,0x2000);
            const std::uint8_t model=std::uint8_t(run*3u%30u);
            *reinterpret_cast<std::uint8_t*>(obj+0x11)=model;*reinterpret_cast<std::uint32_t*>(0x799d18u)=obj;
            *reinterpret_cast<std::uint8_t*>(obj+0x1800+0x34a)=std::uint8_t(run);
            *reinterpret_cast<std::uint32_t*>(0x819634u)=run%3u;
            prepare(entry);guest_call.eax=obj+0x1800;run_original32();
            dump("model "+std::to_string(model)+" 34a "+std::to_string(run)+" preset "+std::to_string(run%3u));
        }
        // Camera words filled with their own offsets: which one lands in ESI.
        for(unsigned k=0;k<0x400;k+=4)*reinterpret_cast<std::uint32_t*>(obj+0x1800+k)=0x70000000u+k;
        *reinterpret_cast<std::uint8_t*>(obj+0x1800+0x34a)=2;
        prepare(entry);guest_call.eax=obj+0x1800;run_original32();dump("camera words = 0x70000000+offset, 34a=2");
        for(unsigned v:{0u,1u,5u,0x15u,0x16u,0x7fu,0x80u,0xffu}){
            std::memset(reinterpret_cast<void*>(obj+0x1800),0,0x400);*reinterpret_cast<std::uint8_t*>(obj+0x1800+0x34a)=std::uint8_t(v);
            prepare(entry);guest_call.eax=obj+0x1800;run_original32();dump("34a="+std::to_string(v));
        }
        // Every .data dword holds its own address: a register loaded from
        // memory then names its source.
        std::vector<std::uint8_t> keep(0x98d000u-0x62f000u);std::memcpy(keep.data(),reinterpret_cast<void*>(0x62f000u),keep.size());
        for(std::uint32_t a=0x62f000u;a<0x98d000u;a+=4)*reinterpret_cast<std::uint32_t*>(a)=a;
        std::memset(reinterpret_cast<void*>(obj+0x1800),0,0x400);
        prepare(entry);guest_call.eax=obj+0x1800;guest_call.esi=0x12340000u;guest_call.edi=0x56780000u;guest_call.ecx=0x9abc0000u;run_original32();dump("data = own address, esi/edi/ecx markers");
        std::memcpy(reinterpret_cast<void*>(0x62f000u),keep.data(),keep.size());
    }else if(snippet=="485590"){
        // 485590(camera, entry, n) with EAX = car: what the bridge at 4855C8 leaves.
        for(unsigned v:{0u,1u,2u,0x15u,0x1bu,0x80u,0xffu}){
            std::memset(reinterpret_cast<void*>(obj),0,0x3000);
            for(unsigned k=0;k<0x400;k+=4)*reinterpret_cast<std::uint32_t*>(obj+0x1800+k)=0x70000000u+k;
            *reinterpret_cast<std::uint8_t*>(obj+0x1800+0x34a)=std::uint8_t(v);
            for(unsigned k=0;k<0x40;k+=4)*reinterpret_cast<std::uint32_t*>(obj+0x2800+k)=0x60000000u+k;
            prepare(entry);guest_call.eax=obj;guest_call.ebx=0x11110000u;guest_call.esi=0x22220000u;guest_call.edi=0x33330000u;
            auto* st=reinterpret_cast<std::uint32_t*>(S);st[0]=obj+0x1800;st[1]=obj+0x2800;st[2]=30;run_original32();
            dump("34a="+std::to_string(v));
            const auto* r=regs();const auto* sp=reinterpret_cast<const std::uint32_t*>(r[4]);
            std::cerr<<"  stack:"<<std::hex;for(unsigned q=0;q<16;++q)std::cerr<<' '<<sp[q];std::cerr<<std::dec<<char(10);
        }
    }else if(snippet=="at"){
        // Start AT the bridge jump (OR2_ENTRY = the jmp [1039xxx] address):
        // every register names itself, a 256 KB object window and the stack
        // hold their own addresses (OR2_FILL=k: address*k), so the captured
        // registers and the changed words show what the protected
        // instructions did.
        constexpr std::uint32_t big=0x4c000000u,size=0x40000u;
        map_at(big,size,PROT_READ|PROT_WRITE);
        const std::uint32_t mul=std::getenv("OR2_FILL")?std::uint32_t(std::strtoul(std::getenv("OR2_FILL"),nullptr,16)):1u;
        for(std::uint32_t a=big;a<big+size;a+=4)*reinterpret_cast<std::uint32_t*>(a)=a*mul;
        auto* st=reinterpret_cast<std::uint32_t*>(S);for(unsigned q=0;q<128;++q)st[q]=0x5a000000u+q*4;
        // OR2_ARGS="4c001000,5": stack words S[0], S[1].. (arguments when OR2_ENTRY is a function entry)
        if(const char* args=std::getenv("OR2_ARGS")){std::string list=args;std::size_t at=0;unsigned q=0;
            while(at<list.size()&&q<128){auto end=list.find(',',at);if(end==std::string::npos)end=list.size();
                st[q++]=std::uint32_t(std::strtoul(list.substr(at,end-at).c_str(),nullptr,16));at=end+1;}}
        // OR2_DATAFILL=1: every .data/.bss dword also holds its own address
        // (restored afterwards) so a global read names its source.
        std::vector<std::uint8_t> keep_data;const bool datafill=std::getenv("OR2_DATAFILL")!=nullptr;
        if(datafill){keep_data.resize(0x98d000u-0x62f000u);std::memcpy(keep_data.data(),reinterpret_cast<void*>(0x62f000u),keep_data.size());
            for(std::uint32_t a=0x62f000u;a<0x98d000u;a+=4)*reinterpret_cast<std::uint32_t*>(a)=a;}
        // OR2_POKE="4c001008=08,...": single bytes written after the fill.
        if(const char* poke=std::getenv("OR2_POKE")){std::string list=poke;std::size_t at=0;
            while(at<list.size()){auto end=list.find(',',at);if(end==std::string::npos)end=list.size();
                const auto item=list.substr(at,end-at);const auto eq=item.find('=');
                *reinterpret_cast<std::uint8_t*>(std::strtoul(item.substr(0,eq).c_str(),nullptr,16))=std::uint8_t(std::strtoul(item.c_str()+eq+1,nullptr,16));at=end+1;}}
        std::vector<std::uint32_t> before(size/4);std::memcpy(before.data(),reinterpret_cast<void*>(big),size);
        prepare(entry);guest_call.eax=big+0x400;guest_call.ecx=big+0x100;guest_call.edx=big+0x600;guest_call.ebx=big+0x500;
        guest_call.ebp=big+0x200;guest_call.esi=big;guest_call.edi=big+0x300;
        // OR2_SET="esi=4c000800,eax=7": override single registers to tell
        // apart candidate base registers or immediates.
        if(const char* set=std::getenv("OR2_SET")){std::string list=set;std::size_t at=0;
            while(at<list.size()){auto end=list.find(',',at);if(end==std::string::npos)end=list.size();
                const auto item=list.substr(at,end-at);const auto v=std::uint32_t(std::strtoul(item.c_str()+4,nullptr,16));const auto n=item.substr(0,3);
                if(n=="eax")guest_call.eax=v;else if(n=="ecx")guest_call.ecx=v;else if(n=="edx")guest_call.edx=v;else if(n=="ebx")guest_call.ebx=v;
                else if(n=="ebp")guest_call.ebp=v;else if(n=="esi")guest_call.esi=v;else if(n=="edi")guest_call.edi=v;else throw std::runtime_error("OR2_SET "+item);
                at=end+1;}}
        run_original32();dump("at "+std::string(e));
        const auto* r=regs();std::cerr<<"  esp delta="<<std::hex<<(std::int32_t(r[4])-std::int32_t(S))<<" stack:";
        const auto* sp=reinterpret_cast<const std::uint32_t*>(r[4]);for(unsigned q=0;q<8;++q)std::cerr<<' '<<sp[q];std::cerr<<char(10);
        for(unsigned q=0;q<size/4;++q){const auto v=reinterpret_cast<const std::uint32_t*>(big)[q];if(v!=before[q])std::cerr<<"  obj+"<<q*4<<" = "<<v<<char(10);}
        for(unsigned q=0;q<128;++q)if(st[q]!=0x5a000000u+q*4)std::cerr<<"  stack["<<q*4<<"] = "<<st[q]<<char(10);
        if(datafill){for(std::uint32_t a=0x62f000u;a<0x98d000u;a+=4){const auto v=*reinterpret_cast<std::uint32_t*>(a);if(v!=a)std::cerr<<"  ["<<a<<"] = "<<v<<char(10);}
            std::memcpy(reinterpret_cast<void*>(0x62f000u),keep_data.data(),keep_data.size());}
        std::cerr<<std::dec;
    }else if(snippet=="call"){
        // A whole protected function: run it from OR2_ENTRY to its return
        // (OR2_CONT names any spare address) with every .data/.bss dword
        // holding its own address, then list the dwords it changed and EAX.
        std::vector<std::uint8_t> keep(0x98d000u-0x62f000u);std::memcpy(keep.data(),reinterpret_cast<void*>(0x62f000u),keep.size());
        for(std::uint32_t a=0x62f000u;a<0x98d000u;a+=4)*reinterpret_cast<std::uint32_t*>(a)=a;
        // OR2_POKE32="780258=6,...": dwords written after the fill.
        if(const char* poke=std::getenv("OR2_POKE32")){std::string list=poke;std::size_t at=0;
            while(at<list.size()){auto end=list.find(',',at);if(end==std::string::npos)end=list.size();
                const auto item=list.substr(at,end-at);const auto eq=item.find('=');
                *reinterpret_cast<std::uint32_t*>(std::strtoul(item.substr(0,eq).c_str(),nullptr,16))=std::uint32_t(std::strtoul(item.c_str()+eq+1,nullptr,16));at=end+1;}}
        prepare(entry);auto* st=reinterpret_cast<std::uint32_t*>(S);for(unsigned q=0;q<8;++q)st[q]=0x5b000000u+q*4;
        guest_call.eax=0x11110000u;guest_call.ecx=0x22220000u;guest_call.edx=0x33330000u;run_original32();
        std::cerr<<std::hex<<"call "<<entry<<" eax="<<guest_call.out_eax<<"\n";
        for(std::uint32_t a=0x62f000u;a<0x98d000u;a+=4){const auto v=*reinterpret_cast<std::uint32_t*>(a);if(v!=a)std::cerr<<"  ["<<a<<"] = "<<v<<"\n";}
        std::cerr<<std::dec;
        std::memcpy(reinterpret_cast<void*>(0x62f000u),keep.data(),keep.size());
    }else if(snippet=="generic"){
        for(unsigned run=0;run<4;++run){prepare(entry);guest_call.eax=obj;guest_call.ecx=obj+0x100;*reinterpret_cast<std::uint32_t*>(S)=obj+0x200;run_original32();dump("run "+std::to_string(run));}
    }
    {auto* page=reinterpret_cast<void*>(cont&~0xfffu);mprotect(page,8192,PROT_READ|PROT_WRITE|PROT_EXEC);std::memcpy(reinterpret_cast<void*>(cont),saved.data(),5);}
    std::cout<<"{\"routine\":\"bridge_snippet_probe\",\"total_cases\":1,\"total_mismatching_cases\":0}"<<'\n';return 0;
}

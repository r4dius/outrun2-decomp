// Included inside the compare_pc dispatcher; uses its private x86 harness.
if(only=="name_entry_probe"){
    // OUTRUN2SP name entry (race_name_entry.cpp) against the original:
    //   0 4B2370 init  1 4B29E0 control (4B0970 slide, name filter, bad words, commit, states)
    //   2 4B4F20 display (4B46F0, 4B1F40, 4B22F0 (VM 4B22F1), 4B17E0, 4B1FA0, 4B4010, 4B4170)
    //   3 4B0950 destroy  4 49A960 mode 26 init  5 49AA60 control  6 49AAE0 exit
    // Boundary services are capture stubs recording {pc, ECX, stack args} (4287B0 / 4289B0:
    // the matrix; 42CCE0 "%s": the text) and returning a scripted value; 47EE70 copies a
    // scripted flag pattern to its argument; 428840 pushes 0.0. The module's own helpers, the
    // SP accessors 4F3010..4F3AD0, 449AC0, the CRT (strstr, _stricmp, sprintf), 4B72F0 /
    // 4B7630 and D3DXMatrixTranslation run as original code. The PC stack is zeroed before
    // each run (the init record's uninitialised words are taken as 0 by the port).
    // Compared: the module .bss, the SP tables and book, the license, the mode words, the
    // service log. Letter-input states (8427E4 / 8426D4 / 8423B0 set) are not generated.
    using namespace outrun::platform;
    if(argc<=7||std::string(argv[7])=="-")throw std::runtime_error("name entry probe requires the pinned d3dx9_29.dll (D3DXMatrixTranslation)");
    wire_d3dx_imports(map_d3dx29(argv[7]));
    constexpr unsigned base=0x3bc00000u,stubs=base,trace=base+0x8000u,values=base+0x7000u,Player=base+0x41000u,Fix=base+0x43000u;
    map_at(base,0x50000,PROT_READ|PROT_WRITE|PROT_EXEC);map_original_arithmetic_bridge(file);
    {   const std::size_t text_file=pe_file_offset(file,0x401000u);
        for(std::uint32_t a=0x401000u;a<0x401000u+0x194881u;a+=0x100u){
            const std::size_t n=std::min<std::size_t>(0x100u,0x401000u+0x194881u-a);
            if(std::memcmp(reinterpret_cast<void*>(a),file.data()+text_file+(a-0x401000u),n))r060_raw_write(a,file.data()+text_file+(a-0x401000u),n);
        }
    }
    for(std::uint32_t page=0x62f000u;page<0x98d000u;page+=0x1000u)mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE);
    auto emit32=[](std::vector<std::uint8_t>& b,std::uint32_t v){for(unsigned k=0;k<4;++k)b.push_back(std::uint8_t(v>>(k*8)));};
    {   // _stricmp (58DD1E) through 585BEC: a private thread block whose locale is the global
        // one ([735E5C]) with no LC_CTYPE handle (+14 = 0): __ascii_stricmp.
        const std::uint32_t ptd=base+0x48000u,loc=base+0x48800u;
        std::memset(reinterpret_cast<void*>(ptd),0,0x1000);
        *reinterpret_cast<std::uint32_t*>(ptd+0x64u)=loc;*reinterpret_cast<std::uint32_t*>(0x735e5cu)=loc;
        std::uint8_t code[]{0xb8,0,0,0,0,0xc3};std::memcpy(code+1,&ptd,4);r060_raw_write(0x585bec,code,sizeof(code));
    }
    // kind: 0 plain, 1 matrix at arg1, 2 x87 0.0 result, 4 matrix at arg2, 5 42CCE0 (text of a "%s"),
    // 6 47EE70 (16 pattern words to arg0)
    struct Svc{std::uint32_t pc;unsigned argc,kind;};
    const std::uint32_t Pattern=Fix+0x380u;
    const std::vector<Svc> services{
        {0x450320,0,0},{0x450560,1,0},{0x44dc50,1,0},{0x428320,3,0},{0x428460,5,0},{0x428880,1,0},{0x428840,1,2},{0x4287b0,2,1},
        {0x4289b0,4,4},{0x429530,5,0},{0x4b99d0,1,0},{0x48b140,0,0},{0x451180,1,0},{0x48b180,0,0},{0x48b1a0,0,0},{0x45b830,0,0},
        {0x45b840,0,0},{0x47ee70,1,6},{0x427700,1,0},{0x4493c0,0,0},{0x4249f0,1,0},{0x4165f0,0,0},{0x480d00,1,0},{0x481180,1,0},
        {0x47ef10,1,0},{0x4536f0,1,0},{0x42ccb0,1,0},{0x42ca60,1,0},{0x42cca0,1,0},{0x42cc60,2,0},{0x42cc00,2,0},{0x42cce0,2,5},
        {0x42d5c0,5,0},{0x465eb0,1,0},{0x49a650,1,0},{0x428600,0,0},
        {0x43f900,1,0},{0x42deb0,2,0},{0x429920,2,0},{0x42df90,0,0},{0x4299a0,0,0},{0x440110,2,0},{0x4401d0,1,0},{0x43fa90,0,0},
        {0x43f8c0,1,0},{0x43f980,0,0},{0x43f990,1,0},{0x427630,0,0},{0x42dfb0,1,0},{0x4299c0,1,0},
        {0x4035f0,0,0},{0x4285a0,1,0},{0x440cd0,1,0}};
    std::map<std::uint32_t,unsigned> index_of;unsigned stub_off=0;
    for(std::size_t k=0;k<services.size();++k){
        const auto& e=services[k];index_of[e.pc]=unsigned(k);
        std::vector<std::uint8_t> c{0x57,0x56,0x51,0x8b,0x3d};emit32(c,trace);            // push edi/esi/ecx; mov edi,[count]
        c.insert(c.end(),{0x6b,0xff,0x60,0x81,0xc7});emit32(c,trace+64);                    // imul edi,edi,0x60; add edi,entries
        c.insert(c.end(),{0xc7,0x07});emit32(c,e.pc);                                       // [edi]=pc
        c.insert(c.end(),{0x89,0x4f,0x04});                                                 // [edi+4]=ecx
        for(unsigned i=0;i<e.argc;++i)c.insert(c.end(),{0x8b,0x44,0x24,std::uint8_t(0x10+4*i),0x89,0x47,std::uint8_t(0x08+4*i)});
        if(e.kind==1)c.insert(c.end(),{0x8b,0x74,0x24,0x14,0x83,0xc7,0x20,0xb9,0x10,0,0,0,0xf3,0xa5});   // esi=arg1; edi+=0x20; rep movsd 16
        if(e.kind==4)c.insert(c.end(),{0x8b,0x74,0x24,0x18,0x83,0xc7,0x20,0xb9,0x10,0,0,0,0xf3,0xa5});   // esi=arg2
        if(e.kind==5)c.insert(c.end(),{0x8b,0x44,0x24,0x10,0x3d,0x68,0x64,0x62,0x00,0x75,0x0e,       // arg0 == "%s":
                                       0x8b,0x74,0x24,0x14,0x83,0xc7,0x20,0xb9,0x10,0,0,0,0xf3,0xa5});   // the 64 bytes at arg1
        c.insert(c.end(),{0xff,0x05});emit32(c,trace);                                      // inc [count]
        if(e.kind==6){c.insert(c.end(),{0x8b,0x7c,0x24,0x10,0xbe});emit32(c,Pattern);c.insert(c.end(),{0xb9,0x10,0,0,0,0xf3,0xa5});}
        c.push_back(0xa1);emit32(c,values+4*unsigned(k));                                   // eax=value
        if(e.kind==2)c.insert(c.end(),{0xd9,0xee});                                         // fldz
        c.insert(c.end(),{0x59,0x5e,0x5f,0xc3});
        std::memcpy(reinterpret_cast<void*>(stubs+stub_off),c.data(),c.size());
        r046_write_rel32(e.pc,0xe9,stubs+stub_off);stub_off=(stub_off+unsigned(c.size())+15u)&~15u;
    }
    std::mt19937 rng(0x4b237000u);
    auto rnd=[&](std::uint32_t n){return n?std::uint32_t(rng()%n):0u;};
    auto real=[&](float a,float b){return a+(b-a)*float(rng()%100000)/100000.f;};
    auto g32=[](std::uint32_t a){return *reinterpret_cast<std::uint32_t*>(a);};
    auto p32=[](std::uint32_t a,std::uint32_t v){*reinterpret_cast<std::uint32_t*>(a)=v;};
    auto p8=[](std::uint32_t a,std::uint8_t v){*reinterpret_cast<std::uint8_t*>(a)=v;};
    auto pf=[](std::uint32_t a,float v){std::memcpy(reinterpret_cast<void*>(a),&v,4);};
    unsigned cases{},mismatches{};const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
    std::map<std::string,unsigned> coverage;
    auto fail=[&](const std::string& what){++mismatches;if(mismatches<20)std::cerr<<what<<"\n";};
    auto compare=[&](const void* native,const void* original,std::size_t n,const std::string& what){
        ++cases;if(!std::memcmp(native,original,n))return;
        const auto* a=static_cast<const std::uint8_t*>(native);const auto* b=static_cast<const std::uint8_t*>(original);
        for(std::size_t i=0;i<n;++i)if(a[i]!=b[i]){std::ostringstream s;s<<what<<" offset=0x"<<std::hex<<i<<" PC="<<unsigned(b[i])<<" native="<<unsigned(a[i]);fail(s.str());break;}
    };
    struct Mirror{std::uint32_t base,end;std::vector<std::uint8_t> bytes;};
    std::vector<Mirror> mirrors{{0x842288u,0x842820u,{}},{0x842820u,0x84288cu,{}},{0x687774u,0x68777cu,{}},{0x84def8u,0x850b00u,{}},
        {0x7d3a90u,0x7d6720u,{}},{0x7c23e0u,0x7c27ecu,{}},{0x780258u,0x78025cu,{}},{0x78024cu,0x780250u,{}},{0x799d18u,0x799d1cu,{}},
        {0x836d08u,0x836d18u,{}},{Player,Player+0x40u,{}},{Fix,Fix+0x400u,{}}};
    // Names: letters, digits, the 5C6388 symbols, spaces, other bytes and the decoded bad words.
    std::vector<std::string> words;
    for(std::uint32_t i=0;g32(0x68694cu+i*4u);++i){std::string w;for(std::uint32_t p=g32(0x68694cu+i*4u);*reinterpret_cast<const char*>(p);++p)w+=*reinterpret_cast<const char*>(p);
        for(std::size_t k=0;k<4u&&k<w.size();++k)w[k]=char(w[k]-1);if(w.size()<=4u)words.push_back(w);}
    const std::string chars="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!%&+-./=?@\\ ~#*";
    static constexpr std::uint32_t Pc[]{0x4b2370,0x4b29e0,0x4b4f20,0x4b0950,0x49a960,0x49aa60,0x49aae0,0x4b1dd0};
    const unsigned Entries=8u;
    for(unsigned iter=0;iter<count;++iter){
        unsigned entry=iter%Entries;
        if(const char* e=std::getenv("OR2_NAME_ENTRY"))entry=unsigned(std::stoul(e));
        std::memset(reinterpret_cast<void*>(trace),0,0x8000);
        std::vector<std::uint32_t> value(services.size());
        for(auto& v:value)v=rnd(3)?rnd(3):rng();
        auto setv=[&](std::uint32_t pc,std::uint32_t v){value[index_of.at(pc)]=v;};
        auto handle=[&]{return rnd(4)?rnd(1344):0xffffffffu;};
        setv(0x450320,rnd(6)?rnd(16):(rnd(2)?16u+rnd(8):0xfffffff0u+rnd(16)));
        setv(0x44dc50,rnd(4)?10u+rnd(20):rnd(40));
        setv(0x428320,handle());setv(0x428460,handle());setv(0x428880,rnd(3)?3u:rnd(5));
        setv(0x4b99d0,rnd(3)?rnd(0x1000000):rng());setv(0x48b140,(rng()&0xffffff00u)|rnd(16));setv(0x451180,rnd(5)?rnd(1200000):0u);
        setv(0x48b180,(rng()&0xffffff00u)|(rnd(4)?rnd(2):0xffu));setv(0x48b1a0,(rng()&0xffffff00u)|(rnd(4)?rnd(2):0xffu));
        setv(0x45b830,rnd(3)?rnd(5000000):rng());setv(0x45b840,1u+rnd(20000));
        setv(0x427700,rnd(2));setv(0x4493c0,rnd(3)?0u:1u);setv(0x4536f0,rnd(3)?0u:1u);
        setv(0x465eb0,Fix+0x300u);setv(0x4035f0,Fix+0x200u);
        setv(0x42df90,rnd(3)?1u:0u);setv(0x4299a0,rnd(3)?1u:0u);setv(0x43fa90,rnd(2));setv(0x43f980,rnd(5)?0u:1u);
        for(unsigned k=0;k<16;++k)p32(Pattern+k*4,rnd(3)?0u:1u);
        std::memcpy(reinterpret_cast<void*>(values),value.data(),value.size()*4);
        // globals: blocks random, then the indexed fields kept in range
        for(auto& mi:mirrors)for(std::uint32_t a=mi.base;a<mi.end;a+=4)if(mi.end-a>=4)p32(a,rnd(3)?rnd(4):rng());else p8(a,std::uint8_t(rng()));
        // the SP modes' (variant, preset) pairs keep the 4F3xxx rows inside 84DF40..850B00
        const std::uint32_t variant=rnd(5)?rnd(3):3u,preset=variant==2u?0u:rnd(4);
        p32(0x780258u,variant);p32(0x78024cu,preset);p32(0x799d18u,Player);p32(0x63960cu,0x7d3a90u);
        p32(0x842888u,rnd(4));p32(0x687774u,rnd(3)?0xffffffffu:rnd(1344));p32(0x687778u,rnd(3)?0xffffffffu:rnd(1344));
        p32(0x836d0cu,rnd(4));p32(0x836d08u,rnd(10));
        // SP tables: records {score | nibble | choice, time, ratio | bits, name}
        for(std::uint32_t a=0x84df40u;a<0x850b00u;a+=16){p32(a,rng());p32(a+4,rnd(4)?rnd(1200000):rng());p32(a+8,rng());p32(a+12,rng());}
        // the license name
        {std::string n;if(rnd(4)==0&&!words.empty())n=words[rnd(std::uint32_t(words.size()))];
         else{const std::uint32_t len=rnd(8);for(std::uint32_t k=0;k<len;++k)n+=rnd(12)?chars[rnd(std::uint32_t(chars.size()))]:char(1+rnd(255));}
         if(rnd(6)==0)n=" "+n+"  ";
         for(std::size_t k=0;k<n.size()&&k<0x20u;++k)p8(0x7c23e0u+std::uint32_t(k),std::uint8_t(n[k]));p8(0x7c23e0u+std::uint32_t(std::min<std::size_t>(n.size(),0x20u)),0);}
        // the module block
        p32(0x8427e4u,0);p32(0x8426d4u,0);p32(0x8423b0u,0);
        p32(0x8426ecu,variant);p32(0x842708u,preset);p32(0x8427e0u,rnd(6));p32(0x8427e8u,rnd(2));p32(0x8427ecu,rnd(2));
        p32(0x84270cu,rnd(5));
        p32(0x8426d8u,rnd(5)?rnd(10):0xffffffffu);
        p32(0x8426f0u,rnd(2)?0u:5u);p32(0x8426f8u,std::uint32_t(std::int32_t(rnd(20))-2));p32(0x8426fcu,rnd(3)?0xffffffffu:rnd(6));
        p32(0x8427fcu,rnd(2));p32(0x842800u,rnd(16));p32(0x8426e4u,rnd(3)?rnd(4):rng());p8(0x84280cu,std::uint8_t(rnd(2)));
        p32(0x842804u,rnd(2));
        for(std::uint32_t a:{0x842710u,0x842714u,0x842718u,0x84271cu,0x842724u})p32(a,handle());
        {const std::int32_t l[]{-1,0,1,2,3,4,6};p32(0x842720u,std::uint32_t(l[rnd(7)]));}
        p32(0x8427f4u,std::uint32_t(std::int32_t(rnd(7))-1));
        for(std::uint32_t r=0;r<11u;++r){
            const std::uint32_t e=0x8423c0u+r*0x48u;
            for(std::uint32_t o=0x24u;o<0x3cu;o+=4)pf(e+o,real(-300,640));
            p32(e+8u,rnd(4)?rnd(0x1000000):rng());p32(e+0xcu,rnd(4)?rnd(1200000):rng());p32(e+0x10u,rnd(3)?rnd(5000):rng());
            for(std::uint32_t k=0;k<6u;++k)p8(e+0x14u+k,std::uint8_t(rnd(6)?chars[rnd(std::uint32_t(chars.size()))]:rnd(4)?0u:rng()));
            p8(e+0x1au,std::uint8_t(rnd(4)));p8(e+0x1bu,std::uint8_t(rnd(2)));p8(e+0x1cu,std::uint8_t(rnd(2)));
            p32(e+0x20u,rnd(16));p32(e+0x3cu,handle());p32(e+0x40u,handle());p32(e+0x44u,rnd(4));
        }
        std::memcpy(reinterpret_cast<void*>(Fix+0x300u),"NO ENTRY %d",12);
        for(auto& mi:mirrors){mi.bytes.resize(mi.end-mi.base);std::memcpy(mi.bytes.data(),reinterpret_cast<void*>(mi.base),mi.bytes.size());}
        prepare(Pc[entry]);std::memset(reinterpret_cast<void*>(S-0x2000u),0,0x2000u);
        run();
        // native
        PcRaceMemory mem;std::vector<std::vector<std::uint8_t>> native(mirrors.size());
        for(std::size_t i=0;i<EmbeddedExeRangeCount;++i)mem.map_const(EmbeddedExeRanges[i].base,EmbeddedExeRanges[i].data,EmbeddedExeRanges[i].size);
        for(std::size_t i=0;i<mirrors.size();++i){native[i]=mirrors[i].bytes;mem.map(mirrors[i].base,native[i].data(),native[i].size());}
        const std::uint32_t book_cell=0x7d3a90u;mem.map_const(0x63960cu,reinterpret_cast<const std::uint8_t*>(&book_cell),4);
        std::array<std::uint8_t,0x40> mb{};outrun::driving::PcMatrixStack stack{outrun::driving::Bytes(mb.data(),mb.size())};
        struct NCall{std::uint32_t pc,ecx;decltype(PcRaceCall::args) a;std::array<std::uint32_t,16> mat;};std::vector<NCall> calls;
        PcRaceContext ctx{mem,stack,[&](const PcRaceCall& k)->std::uint32_t{
            const auto it=index_of.find(k.pc);if(it==index_of.end()){std::ostringstream s;s<<"name entry probe: unexpected service "<<std::hex<<k.pc;throw std::runtime_error(s.str());}
            NCall n{k.pc,k.ecx,k.args,{}};
            const auto kind=services[it->second].kind;
            if(kind==1||kind==4)for(unsigned w=0;w<16;++w)n.mat[w]=mem.u32(k.args[kind==1?1:2]+w*4);
            if(kind==5&&k.args[0]==0x626468u)for(unsigned w=0;w<16;++w){std::uint32_t v=0;for(unsigned b=0;b<4;++b)v|=std::uint32_t(mem.mapped(k.args[1]+w*4+b,1)?mem.u8(k.args[1]+w*4+b):0u)<<(b*8);n.mat[w]=v;}
            if(kind==6)for(unsigned w=0;w<16;++w)mem.put32(k.args[0]+w*4,g32(Pattern+w*4));
            calls.push_back(n);return value[it->second];},nullptr};
        bool ok=true,unreachable=false;std::string why;
        try{
            switch(entry){
            case 0:name_entry_init_4b2370(ctx);break;   case 1:name_entry_control_4b29e0(ctx);break;
            case 2:name_entry_display_4b4f20(ctx);break;case 3:name_entry_destroy_4b0950(ctx);break;
            case 4:mode26_init_49a960(ctx);break;       case 5:mode26_control_49aa60(ctx);break;
            case 6:mode26_exit_49aae0(ctx);break;
            default:{const std::uint32_t r=name_entry_record_4b1dd0(ctx);++cases;if(r!=guest_call.out_eax)fail("4B1DD0 EAX PC "+std::to_string(guest_call.out_eax)+" native "+std::to_string(r));
                if(r)++coverage["4B1DD0 ranked"];break;}
            }
        }catch(const PcRaceEndUnreachable&){unreachable=true;}
        catch(const std::exception& e){ok=false;why=e.what();}
        const std::string tag="entry "+std::to_string(entry)+" run "+std::to_string(iter);
        if(unreachable){++coverage["unreachable"];continue;}
        ++cases;if(!ok){fail(tag+" native threw: "+why);continue;}
        if(inject&&iter==11)native[0][0x478]^=1;
        if(std::getenv("OR2_NAME_DEBUG")&&std::memcmp(native[0].data(),reinterpret_cast<void*>(0x842288u),native[0].size())){
            std::cerr<<tag<<" variant "<<variant<<" preset "<<preset<<" rank PC "<<std::int32_t(g32(0x8426d8u))<<" native "<<std::int32_t(mem.u32(0x8426d8u))<<" count "<<g32(0x8427e0u)<<char(10);
            for(std::uint32_t r=0;r<11u;++r){std::cerr<<" row "<<r<<std::hex;for(std::uint32_t o=0;o<0x48u;o+=4)std::cerr<<" "<<g32(0x8423c0u+r*0x48u+o)<<"/"<<mem.u32(0x8423c0u+r*0x48u+o);std::cerr<<std::dec<<char(10);}}
        for(std::size_t i=0;i<mirrors.size();++i){std::ostringstream s;s<<tag<<" region "<<std::hex<<mirrors[i].base;
            compare(native[i].data(),reinterpret_cast<void*>(mirrors[i].base),native[i].size(),s.str());}
        const unsigned n=g32(trace);++cases;
        if(n!=calls.size()){std::ostringstream s;s<<tag<<" service count PC="<<n<<" native="<<calls.size()<<std::hex;
            for(unsigned i=0;i<n&&i<60;++i)s<<" "<<g32(trace+64+i*0x60);s<<" | native";for(auto& cl:calls)s<<" "<<cl.pc;fail(s.str());}
        else for(unsigned i=0;i<n;++i){const auto* g=reinterpret_cast<const std::uint32_t*>(trace+64+i*0x60);
            const auto& e=services[index_of[g[0]]];bool same=g[0]==calls[i].pc;
            for(unsigned k=0;k<e.argc;++k){
                if(e.kind==1&&k==1)continue;                               // 4287B0: the matrix, not its stack address
                if(e.kind==4&&k==2)continue;                               // 4289B0
                if(e.kind==5&&k==1&&g[2]==0x626468u)continue;              // "%s": the text below
                if(e.kind==6&&k==0)continue;                               // 47EE70: the module block (same) / stack
                if(g[0]==0x42cce0u&&k==1&&g[2]==0x5c63a0u)continue;          // "9999999": no second argument pushed
                // byte pushes of the 48B1xx results / row bytes: only their low byte is defined
                same=same&&g[2+k]==calls[i].a[k];
            }
            if(e.kind==1||e.kind==4)for(unsigned w=0;w<16;++w)same=same&&g[8+w]==calls[i].mat[w];
            if(e.kind==5&&g[2]==0x626468u){
                const auto* pc_text=reinterpret_cast<const char*>(g+8);const auto* nat=reinterpret_cast<const char*>(calls[i].mat.data());
                same=same&&std::string(pc_text,strnlen(pc_text,64))==std::string(nat,strnlen(nat,64));}
            if(!same){std::ostringstream s;s<<tag<<" service #"<<i<<" "<<std::hex<<g[0];for(unsigned k=0;k<e.argc;++k)s<<" PC "<<g[2+k]<<"/native "<<calls[i].a[k];fail(s.str());break;}}
        ++coverage["entry "+std::to_string(entry)];
        if(entry==1){
            if(mirrors[0].bytes[0x8427fcu-0x842288u])++coverage["control commit variant "+std::to_string(variant)];
            else ++coverage["control state "+std::to_string(std::min(g32(0x842800u),14u))];
            for(std::uint32_t r=0;r<11u;++r)if(!std::memcmp(reinterpret_cast<void*>(0x8423c0u+r*0x48u+0x14u),"----",4))++coverage["bad word"];}
        if(entry==2)for(auto& cl:calls){if(cl.pc==0x4289b0u)++coverage["4289B0"];if(cl.pc==0x42cce0u)++coverage["42CCE0"];if(cl.pc==0x42d5c0u)++coverage["no entry banner"];}
        if(entry==0)++coverage["init rank "+std::to_string(std::int32_t(g32(0x8426d8u)))];
    }
    std::cout<<"{\"routine\":\""<<only<<"\",\"coverage\":{";bool first=true;
    for(const auto& [k,v]:coverage){std::cout<<(first?"":",")<<"\""<<k<<"\":"<<v;first=false;}
    std::cout<<"},\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}"<<std::endl;
    return mismatches?1:0;
}

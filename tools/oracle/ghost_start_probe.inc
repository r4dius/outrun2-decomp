// Included inside the compare_pc dispatcher; uses its private x86 harness.
if(only=="ghost_start_probe"){
    // Time Attack ghost module start (race_ghosts.cpp) against the original:
    //   0 416610 save layout (real and perturbed size words)
    //   1 467AC0 init (variants, preset/absent buffers, 672DF0 = code, the
    //     416700 VM entry and 423030 with a present/absent save file)
    //   2 4686C0 load (7F92C0 0/1): 467340 slot 1 through the save buffer
    //     (valid / corrupt / absent) or the retail default ghost (valid,
    //     CRC mismatch, open failure, empty file), 466460, 467260, 4662B0,
    //     416830
    //   3 467340 direct (slot 0..2, flag)   4 465F40   5 449A80 (VM at
    //     449A8A)   6 416870   7 416700 with a0 != 0 (record index formula)
    // Service leaves are capture stubs recording {pc, object register, EBX,
    // stack args, 64 bytes of the name}: 580253 malloc / 580BC2 free, 580C33
    // new / 580C38 delete, 4239C0 open / 423F10 size / 423CB0 read /
    // 423BD0 close (a fixture file: handle = its address, size word before
    // it), 406DB0 exists (ESI), 406E50 save read / 406C50 save write (ECX,
    // size in EBX; a successful read copies a fixture save image), 466340.
    // The native side maps a copy of every region at the same addresses and
    // answers the same services. Compared: 7F8D80..7F9420, 7457C8..746470,
    // 8A8C74..8A8C80, 64BFED, the fixture (heap, save object and buffer), the
    // ordered service log and EAX. OR2_GHOSTS_DIR=<game>/Ghosts uses the
    // retail gc_default_TA_%02d.rec files as file fixtures and save images.
    using namespace outrun::platform;
    constexpr unsigned base=0x3b400000u,stubs=base,trace=base+0x4000u,values=base+0x8000u,fix=base+0x10000u,fix_size=0x1f0000u;
    constexpr unsigned Obj=fix,SaveBuf=fix+0x1000u,FileData=fix+0x20000u,Heap=fix+0x30000u,NewBuf=fix+0x100000u,SaveSrc=fix+0x120000u;
    map_at(base,0x200000,PROT_READ|PROT_WRITE|PROT_EXEC);map_original_arithmetic_bridge(file);
    {   unsigned restored=0;const std::size_t text_file=pe_file_offset(file,0x401000u);
        for(std::uint32_t a=0x401000u;a<0x401000u+0x194881u;a+=0x100u){
            const std::size_t n=std::min<std::size_t>(0x100u,0x401000u+0x194881u-a);
            if(std::memcmp(reinterpret_cast<void*>(a),file.data()+text_file+(a-0x401000u),n)){r060_raw_write(a,file.data()+text_file+(a-0x401000u),n);++restored;}
        }
        std::cerr<<"restored "<<restored<<" patched .text blocks"<<char(10);
    }
    for(std::uint32_t page=0x62f000u;page<0x98d000u;page+=0x1000u)mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE);
    auto emit32=[](std::vector<std::uint8_t>& b,std::uint32_t v){for(unsigned k=0;k<4;++k)b.push_back(std::uint8_t(v>>(k*8)));};
    // kind: 0 value, 1 value+stride, 2 size [f-4], 3 read f->buf, 4 save read, 5 none (void)
    // obj: 0 none, 1 ECX, 2 ESI; str: 0 none, 1 arg0, 2 obj register
    struct Svc{std::uint32_t pc;unsigned argc,kind,obj,str;};
    const std::vector<Svc> services{{0x580253,1,1,0,0},{0x580bc2,1,5,0,0},{0x580c33,1,0,0,0},{0x580c38,1,5,0,0},
        {0x4239c0,2,0,0,1},{0x423f10,1,2,0,0},{0x423cb0,4,3,0,0},{0x423bd0,1,5,0,0},
        {0x406db0,0,0,2,2},{0x406e50,1,4,1,2},{0x406c50,1,0,1,2},{0x466340,2,0,0,0}};
    std::map<std::uint32_t,unsigned> index_of;
    unsigned stub_off=0;
    for(std::size_t k=0;k<services.size();++k){
        const auto& e=services[k];index_of[e.pc]=unsigned(k);
        const unsigned value=values+8u*unsigned(k),stride=value+4u;
        std::vector<std::uint8_t> c{0x56,0x57,0x51,0x8b,0x3d};emit32(c,trace);            // push esi/edi/ecx; mov edi,[count]
        c.insert(c.end(),{0xc1,0xe7,0x07,0x81,0xc7});emit32(c,trace+64);                   // shl edi,7; add edi,entries
        c.insert(c.end(),{0xc7,0x07});emit32(c,e.pc);                                       // [edi]=pc
        if(e.obj==1)c.insert(c.end(),{0x8b,0x04,0x24,0x89,0x47,0x04});                      // mov eax,[esp] (ecx); [edi+4]
        if(e.obj==2)c.insert(c.end(),{0x8b,0x44,0x24,0x08,0x89,0x47,0x04});                 // mov eax,[esp+8] (esi)
        c.insert(c.end(),{0x89,0x5f,0x08});                                                 // [edi+8]=ebx
        for(unsigned i=0;i<e.argc;++i)c.insert(c.end(),{0x8b,0x44,0x24,std::uint8_t(0x10+4*i),0x89,0x47,std::uint8_t(0x0c+4*i)});
        if(e.str){
            if(e.str==1)c.insert(c.end(),{0x8b,0x74,0x24,0x10});                            // esi=[esp+10] (arg0)
            else if(e.obj==1)c.insert(c.end(),{0x8b,0x34,0x24});                            // esi=ecx
            else c.insert(c.end(),{0x8b,0x74,0x24,0x08});                                   // esi=esi
            c.insert(c.end(),{0x83,0xc7,0x20,0xb9,0x40,0,0,0,0xf3,0xa4});                   // edi+=20; rep movsb 64
        }
        c.insert(c.end(),{0xff,0x05});emit32(c,trace);                                      // inc [count]
        switch(e.kind){
        case 0:c.push_back(0xa1);emit32(c,value);break;
        case 1:c.push_back(0xa1);emit32(c,value);c.insert(c.end(),{0x8b,0x0d});emit32(c,stride);c.insert(c.end(),{0x01,0x0d});emit32(c,value);break;
        case 2:c.insert(c.end(),{0x8b,0x44,0x24,0x10,0x8b,0x40,0xfc});break;
        case 3:c.insert(c.end(),{0x8b,0x7c,0x24,0x10,0x8b,0x74,0x24,0x1c,0x8b,0x4c,0x24,0x14,0xf3,0xa4,0x8b,0x44,0x24,0x18});break;
        case 4:{   // eax=[value]; if 0: copy ebx bytes [stride] -> arg0
            c.push_back(0xa1);emit32(c,value);c.insert(c.end(),{0x85,0xc0,0x75,0x0e,0x8b,0x7c,0x24,0x10,0x8b,0x35});emit32(c,stride);
            c.insert(c.end(),{0x89,0xd9,0xf3,0xa4});break;}
        default:c.insert(c.end(),{0x31,0xc0});break;
        }
        c.insert(c.end(),{0x59,0x5f,0x5e,0xc3});
        std::memcpy(reinterpret_cast<void*>(stubs+stub_off),c.data(),c.size());
        r046_write_rel32(e.pc,0xe9,stubs+stub_off);stub_off=(stub_off+unsigned(c.size())+15u)&~15u;
    }
    // fixture ghosts: retail files or synthetic records
    std::vector<std::vector<std::uint8_t>> retail;
    if(const char* dir=std::getenv("OR2_GHOSTS_DIR"))for(unsigned k=1;k<=0x54;++k){
        char name[64];std::snprintf(name,sizeof name,"/gc_default_TA_%02u.rec",k);
        std::ifstream f(std::string(dir)+name,std::ios::binary);
        retail.emplace_back((std::istreambuf_iterator<char>(f)),std::istreambuf_iterator<char>());
    }
    if(std::getenv("OR2_GHOST_4168D0")){   // black-box observation of the protected 4168D0(index, src, n)
        std::mt19937 r2(0x4168d0u);
        {   // the VM body calls kernel32 GetCurrentProcessId through the IAT slot 5960F8
            static const std::uint8_t pid[]{0xb8,0x34,0x12,0x00,0x00,0xc3};
            std::memcpy(reinterpret_cast<void*>(stubs+0x3f00u),pid,sizeof pid);
            const std::uint32_t open_value=std::getenv("OR2_OPENEVENT")?std::uint32_t(std::strtoul(std::getenv("OR2_OPENEVENT"),nullptr,16)):0u;
            std::vector<std::uint8_t> oe{0xb8};for(unsigned k=0;k<4;++k)oe.push_back(std::uint8_t(open_value>>(8*k)));
            oe.insert(oe.end(),{0xff,0x05});for(unsigned k=0;k<4;++k)oe.push_back(std::uint8_t((stubs+0x3ff0u)>>(8*k)));
            oe.insert(oe.end(),{0xc2,0x0c,0x00});    // OpenEventA(access, inherit, name): stdcall
            std::memcpy(reinterpret_cast<void*>(stubs+0x3f20u),oe.data(),oe.size());
            if(mprotect(reinterpret_cast<void*>(0x596000u),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("IAT");
            *reinterpret_cast<std::uint32_t*>(0x5960f8u)=stubs+0x3f00u;*reinterpret_cast<std::uint32_t*>(0x5960f4u)=stubs+0x3f20u;
            mprotect(reinterpret_cast<void*>(0x596000u),4096,PROT_READ);
        }
        for(unsigned t=0;t<12;++t){
            for(unsigned k=0;k<fix_size;k+=4)*reinterpret_cast<std::uint32_t*>(fix+k)=0;
            *reinterpret_cast<std::uint32_t*>(0x8a8c7cu)=Obj;
            const bool has=t%8!=3;*reinterpret_cast<std::uint32_t*>(Obj+0x130u)=has?SaveBuf:0u;
            const std::uint32_t size=0x2000u;*reinterpret_cast<std::uint32_t*>(Obj+0x134u)=size;
            *reinterpret_cast<std::uint8_t*>(Obj+0x138u)=std::uint8_t(0x55);
            const std::uint32_t idx=7u;
            const std::uint32_t offs[]{0x100u,0x100u,0x100u,0x100u,0x1f00u,0x80000000u,0xfffffff0u,0x1ff0u,0u,0x2000u,0x10u,0x10u};
            const std::uint32_t ns[]{0x1f00u,0x1f01u,0x1effu,0u,0x100u,0x10u,0x20u,0x10u,0x2000u,0u,0x7fffffffu,0x80000010u};
            *reinterpret_cast<std::uint32_t*>(0x7457c8u+idx*8)=offs[t];
            const std::uint32_t src=fix+0x40000u,n=ns[t];
            for(unsigned k=0;k<0x9cac;++k)*reinterpret_cast<std::uint8_t*>(src+k)=std::uint8_t(k*7+t+1);
            std::vector<std::uint8_t> before(reinterpret_cast<std::uint8_t*>(fix),reinterpret_cast<std::uint8_t*>(fix+fix_size));
            const std::uint32_t words0=*reinterpret_cast<std::uint32_t*>(0x8a8c78u);
            prepare(0x4168d0u);{auto* a=reinterpret_cast<std::uint32_t*>(S);a[0]=idx;a[1]=src;a[2]=n;}
            run();
            std::cerr<<std::hex<<"t="<<t<<" idx="<<idx<<" off="<<*reinterpret_cast<std::uint32_t*>(0x7457c8u+idx*8)<<" size="<<size<<" n="<<n<<" has="<<has
                <<" eax="<<guest_call.out_eax<<" 138="<<unsigned(*reinterpret_cast<std::uint8_t*>(Obj+0x138u))<<" 8a8c78 "<<words0<<"->"<<*reinterpret_cast<std::uint32_t*>(0x8a8c78u)<<" changed:";
            std::size_t first=~std::size_t(0),last=0,count=0;
            for(std::size_t k=0;k<before.size();++k)if(before[k]!=reinterpret_cast<std::uint8_t*>(fix)[k]){if(first==~std::size_t(0))first=k;last=k;++count;}
            if(count)std::cerr<<" fix+"<<first<<"..+"<<last<<" ("<<std::dec<<count<<" bytes)";
            std::cerr<<std::dec<<char(10);
        }
        return 0;
    }
    std::mt19937 rng(0x467ac000u);
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
    // A playback record image: a retail file, else random frames with a valid CRC.
    auto make_record=[&](unsigned code,bool valid)->std::vector<std::uint8_t>{
        std::vector<std::uint8_t> r;
        if(!retail.empty()&&!retail[code%retail.size()].empty())r=retail[code%retail.size()];
        else{
            r.assign(0x120u+0x14u+rnd(60)*0x10u,0);for(auto& b:r)b=std::uint8_t(rng());
            std::memcpy(r.data(),"NHOT",4);const std::uint32_t t=15001u+rnd(1000000);std::memcpy(r.data()+4,&t,4);
            const std::uint32_t frames=1u+rnd(40);std::memcpy(r.data()+0xf8,&frames,4);
            const std::uint32_t len=std::uint32_t(r.size());std::memcpy(r.data()+0x100,&len,4);
            PcRaceMemory tm;ghost_map_tables(tm);std::vector<std::uint8_t> copy=r;copy[0xfc]=0xff;copy[0xfd]=0xff;copy[0xfe]=0;copy[0xff]=0;
            tm.map(0x10000000u,copy.data(),copy.size());const std::uint32_t crc=ghost_crc_449a80(tm,0x10000000u,std::int32_t(len));
            std::memcpy(r.data()+0xfc,&crc,4);
        }
        if(!valid)r[0x130u+rnd(unsigned(r.size())-0x130u)]^=std::uint8_t(1u+rnd(255));
        return r;
    };
    struct Log{std::uint32_t pc,obj,ebx;std::array<std::uint32_t,5> args;std::string text;};
    const std::vector<std::uint8_t> table_image(reinterpret_cast<std::uint8_t*>(0x7457c8u),reinterpret_cast<std::uint8_t*>(0x746470u));
    for(unsigned iter=0;iter<count;++iter){
        unsigned entry=iter%8u;
        if(const char* e=std::getenv("OR2_GHOST_ENTRY"))entry=unsigned(std::stoul(e));
        // ---- random state -------------------------------------------------
        for(unsigned k=0;k<fix_size;k+=4)p32(fix+k,rng());
        std::memset(reinterpret_cast<void*>(trace),0,0x4000);
        std::array<std::uint32_t,2*12> value{};for(auto& v:value)v=rng();
        auto set=[&](std::uint32_t pc,std::uint32_t v,std::uint32_t s){value[2*index_of[pc]]=v;value[2*index_of[pc]+1]=s;};
        set(0x580253,Heap,0x20000u);set(0x580c33,NewBuf,0);set(0x406db0,rnd(2),0);set(0x406c50,rnd(3)?0u:1u+rnd(12),0);set(0x406e50,rnd(4)?0u:0xbu,SaveSrc);
        set(0x466340,0,0);
        std::memcpy(reinterpret_cast<void*>(0x7457c8u),table_image.data(),table_image.size());
        for(std::uint32_t a=0x7f8d80u;a<0x7f9420u;a+=4)p32(a,rnd(3)?0u:rng());
        const unsigned code=rnd(0x54);
        p32(0x656234u,rnd(4)?code:(rng()&~0xffu)|code);
        p32(0x780258u,rnd(3)?7u:rnd(11));
        p32(0x672df0u,rnd(8)?0x54u:std::uint32_t(std::int8_t(code)));
        *reinterpret_cast<std::uint8_t*>(0x64bfedu)=std::uint8_t(rng());
        p32(0x8a8c74u,rng());p32(0x8a8c78u,rng());p32(0x8a8c7cu,Obj);
        p32(Obj+0x130u,0);p32(Obj+0x134u,0);*reinterpret_cast<std::uint8_t*>(Obj+0x138u)=std::uint8_t(rng());
        // save image (a GHOST file) at SaveSrc; retail/synthetic file at FileData
        {   std::memset(reinterpret_cast<void*>(SaveSrc),0,0x17ff0);
            for(unsigned k=0;k<3;++k){const unsigned idx=rnd(0x54);const auto r=make_record(idx,rnd(3)!=0);
                const std::uint32_t off=*reinterpret_cast<std::uint32_t*>(0x7457c8u+idx*8u);
                if(off+r.size()<=0x17ff0u)std::memcpy(reinterpret_cast<void*>(SaveSrc+off),r.data(),r.size());}
        }
        std::uint32_t file_handle=FileData;
        {   const unsigned kind=rnd(6);
            auto r=make_record(code,kind!=1);
            if(kind==2)r.clear();
            std::memcpy(reinterpret_cast<void*>(FileData),r.data(),r.size());p32(FileData-4,std::uint32_t(r.size()));
            if(kind==3)file_handle=0;
            set(0x4239c0,file_handle,0);
            coverage[std::string("file ")+(kind==1?"crc":kind==2?"empty":kind==3?"absent":"valid")]++;
        }
        std::uint32_t arg0{},arg1{},arg2{};std::array<std::uint32_t,7> a7{};
        switch(entry){
        case 0:{if(rnd(2))for(unsigned e=0;e<0x194;++e)*reinterpret_cast<std::uint16_t*>(0x7457ceu+e*8u)=std::uint16_t(rnd(3)?rnd(0x9000):0xf000u+rnd(0x1000));break;}
        case 1:
            p32(0x7f9228u,rnd(2)?Heap+0x60000u:0u);
            p32(0x7f9224u,rnd(2)?Heap+0x80000u:0u);
            if(rnd(3)==0){p32(Obj+0x130u,SaveBuf);p32(Obj+0x134u,0x17ff0u);}
            break;
        case 2:case 3:
            p32(0x7f9224u,Heap+0x80000u);
            p32(0x7f92c0u,entry==3||rnd(5)!=0);p32(0x7f8d80u,rnd(8)?code:rnd(0x194));
            if(rnd(4)){p32(Obj+0x130u,SaveBuf);p32(Obj+0x134u,rnd(4)?0x17ff0u:0x800u+rnd(0x17000));
                std::memcpy(reinterpret_cast<void*>(SaveBuf),reinterpret_cast<void*>(SaveSrc),0x17ff0);}
            arg0=rnd(4)?code:rng();arg1=rnd(3);arg2=rnd(3)?0u:rnd(4);
            break;
        case 4:{p32(0x7f9224u,rnd(8)?Heap+0x80000u:0u);arg0=rnd(3);
            if(g32(0x7f9224u)){const auto r=make_record(code,true);std::memcpy(reinterpret_cast<void*>(Heap+0x80000u+arg0*0x9cac),r.data(),std::min<std::size_t>(r.size(),0x9cac));
                if(rnd(4)==0)*reinterpret_cast<std::uint8_t*>(Heap+0x80000u+arg0*0x9cac)^=1;}
            break;}
        case 5:arg0=fix+0x40000u;arg1=rnd(8)?rnd(0x9e00):(rnd(2)?0u:0x80000000u|rng());break;
        case 6:p32(Obj+0x130u,rnd(6)?SaveBuf:0u);p32(Obj+0x134u,rnd(3)?0x17ff0u:rnd(0x17ff0));
            arg0=rnd(0x194);arg1=Heap+0x40000u;arg2=rnd(4)?0x9cacu:rnd(0x20000);
            if(rnd(4)==0)p32(0x7457c8u+arg0*8u,0x80000000u|rng());
            if(std::int32_t(g32(0x7457c8u+arg0*8u))>=0&&g32(Obj+0x130u)){
                const std::uint32_t off=g32(0x7457c8u+arg0*8u),size=g32(Obj+0x134u);
                const std::int64_t n=std::int32_t(off+arg2)>std::int32_t(size)?std::int64_t(size)-off:arg2;
                if(n<0||off+n>0x17ff0u+0x10000u)p32(Obj+0x130u,0);   // the PC would fault: not a case
            }
            break;
        case 7:for(auto& a:a7)a=rng();a7[0]|=rnd(4)?1u:0u;
            {   // keep the index inside the table
                std::uint32_t edx=(a7[2]&0xff)+(std::uint8_t(a7[1])==0?2u:0u),eax=(a7[3]&0xff)+edx*2u;eax=(a7[6]&0xff)+eax*20u;eax=(a7[5]&0xff)+eax*2u+0x54u;if(std::uint8_t(a7[4]))eax+=10u;
                if(std::uint8_t(a7[0])&&eax>=0x194u){a7[1]=1;a7[2]=0;a7[3]=0;a7[5]&=~0xffu;a7[6]&=~0xffu;a7[6]|=rnd(8);}
                if(!std::uint8_t(a7[0])){a7[6]=(a7[6]&~0xffu)|rnd(0x54);}
            }
            if(rnd(3)==0){p32(Obj+0x130u,SaveBuf);p32(Obj+0x134u,0x17ff0u);}
            break;
        }
        std::memcpy(reinterpret_cast<void*>(values),value.data(),sizeof value);
        // ---- snapshot for the native side ---------------------------------
        PcGhostState st;
        std::memcpy(st.block.data(),reinterpret_cast<void*>(0x7f8d80u),st.block.size());
        std::memcpy(st.table.data(),reinterpret_cast<void*>(0x7457c8u),st.table.size());
        std::memcpy(st.words.data(),reinterpret_cast<void*>(0x8a8c74u),st.words.size());
        std::memcpy(st.bytes_64bfec.data(),reinterpret_cast<void*>(0x64bfecu),2);
        std::vector<std::uint8_t> nfix(reinterpret_cast<std::uint8_t*>(fix),reinterpret_cast<std::uint8_t*>(fix+fix_size));
        std::array<std::uint32_t,3> ngl{g32(0x780258u),g32(0x672df0u),g32(0x656234u)};
        // ---- original -----------------------------------------------------
        std::uint32_t guest_eax=0;
        const std::uint32_t pcs[]{0x416610u,0x467ac0u,0x4686c0u,0x467340u,0x465f40u,0x449a80u,0x416870u,0x416700u};
        prepare(pcs[entry]);
        {auto* s=reinterpret_cast<std::uint32_t*>(S);s[0]=arg0;s[1]=arg1;s[2]=arg2;if(entry==7)for(unsigned i=0;i<7;++i)s[i]=a7[i];}
        run();guest_eax=guest_call.out_eax;
        // ---- native -------------------------------------------------------
        PcRaceMemory m;st.map(m);ghost_map_tables(m);m.map(fix,nfix.data(),nfix.size());
        m.map(0x780258u,reinterpret_cast<std::uint8_t*>(&ngl[0]),4);m.map(0x672df0u,reinterpret_cast<std::uint8_t*>(&ngl[1]),4);
        std::vector<Log> nlog;std::array<std::uint32_t,2*12> nvalue=value;
        std::array<std::uint8_t,0x40> mstore{};outrun::driving::PcMatrixStack matrices{outrun::driving::Bytes(mstore.data(),mstore.size())};
        PcRaceContext ctx{m,matrices,[&](const PcRaceCall& k)->std::uint32_t{
            if(k.pc==0x48b320u)return ngl[2];
            const auto it=index_of.find(k.pc);if(it==index_of.end())throw std::runtime_error("ghost probe: unexpected service");
            const auto& e=services[it->second];Log l{k.pc,e.obj?k.ecx:0u,0,{},{}};
            std::uint32_t ebx=0;std::array<std::uint32_t,5> a{};
            if(k.pc==0x406e50u||k.pc==0x406c50u){a[0]=k.args[0];ebx=k.args[1];}else for(unsigned i=0;i<e.argc;++i)a[i]=k.args[i];
            l.ebx=ebx;l.args=a;
            if(e.str){const std::uint32_t p=e.str==1?a[0]:k.ecx;for(unsigned i=0;i<64;++i){const auto ch=m.u8(p+i);if(!ch)break;l.text.push_back(char(ch));}}
            nlog.push_back(l);
            std::uint32_t& v=nvalue[2*it->second];const std::uint32_t s=nvalue[2*it->second+1];
            switch(e.kind){
            case 0:return v;
            case 1:{const auto r=v;v+=s;return r;}
            case 2:return m.u32(a[0]-4);
            case 3:{const std::uint32_t n=a[1];if(n)std::memcpy(m.at(a[0],n,true),m.at(a[3],n),n);return a[2];}
            case 4:if(v)return v;if(ebx)std::memcpy(m.at(a[0],ebx,true),m.at(s,ebx),ebx);return 0;
            default:return 0;
            }},nullptr};
        std::uint32_t native_eax=0;bool native_ok=true;std::string why;
        try{
            switch(entry){
            case 0:ghost_save_layout_416610(m);break;
            case 1:ghost_init_467ac0(ctx);break;
            case 2:ghost_load_4686c0(ctx);break;
            case 3:ghost_load_record_467340(ctx,arg0,arg1,arg2);break;
            case 4:native_eax=std::uint8_t(ghost_car_465f40(m,arg0));guest_eax&=0xffu;break;
            case 5:native_eax=ghost_crc_449a80(m,arg0,std::int32_t(arg1));guest_eax&=0xffffu;break;
            case 6:native_eax=ghost_save_copy_416870(ctx,arg0,arg1,arg2)?1u:0u;guest_eax&=0xffu;break;
            case 7:native_eax=ghost_save_open_416700(ctx,a7);break;
            }
        }catch(const std::exception& e){native_ok=false;why=e.what();}
        if(entry==0||entry==2||entry==3)guest_eax=native_eax=0;
        if(entry==1)guest_eax=native_eax=0;
        ++cases;if(!native_ok){fail("entry "+std::to_string(entry)+" native threw: "+why);continue;}
        if(inject&&iter==9)st.block[0x100]^=1;
        const std::string tag="entry "+std::to_string(entry)+" run "+std::to_string(iter);
        compare(st.block.data(),reinterpret_cast<void*>(0x7f8d80u),st.block.size(),tag+" block 7F8D80");
        compare(st.table.data(),reinterpret_cast<void*>(0x7457c8u),st.table.size(),tag+" table 7457C8");
        compare(st.words.data(),reinterpret_cast<void*>(0x8a8c74u),st.words.size(),tag+" words 8A8C74");
        compare(st.bytes_64bfec.data(),reinterpret_cast<void*>(0x64bfecu),2,tag+" 64BFEC");
        compare(nfix.data(),reinterpret_cast<void*>(fix),nfix.size(),tag+" fixture");
        ++cases;if(native_eax!=guest_eax){std::ostringstream s;s<<tag<<" eax PC="<<std::hex<<guest_eax<<" native="<<native_eax;fail(s.str());}
        // service log
        const unsigned gcount=g32(trace);
        ++cases;
        if(gcount!=nlog.size()){fail(tag+" service count PC="+std::to_string(gcount)+" native="+std::to_string(nlog.size()));continue;}
        for(unsigned i=0;i<gcount;++i){
            const auto* g=reinterpret_cast<const std::uint32_t*>(trace+64+i*128);const auto& n=nlog[i];
            const auto& e=services[index_of[g[0]]];
            bool ok=g[0]==n.pc&&(e.obj==0||g[1]==n.obj);
            if(n.pc==0x406e50u||n.pc==0x406c50u)ok=ok&&g[2]==n.ebx;
            for(unsigned k=0;k<e.argc;++k)ok=ok&&(g[3+k]==n.args[k]||(n.pc==0x4239c0u&&k==0));   // name buffer: compared by text
            if(e.str){std::string t;const char* p=reinterpret_cast<const char*>(g+8);for(unsigned k=0;k<64&&p[k];++k)t.push_back(p[k]);ok=ok&&t==n.text;}
            if(!ok){std::ostringstream s;s<<tag<<" service #"<<i<<" "<<std::hex<<g[0]<<" native "<<n.pc<<" "<<n.text;fail(s.str());break;}
        }
        ++coverage["entry "+std::to_string(entry)];
        if(entry==2||entry==3)++coverage[std::string("slot state ")+std::to_string(g32(0x7f91f8u+4u*(entry==2?1u:std::uint32_t(std::int8_t(arg1)))))];
    }
    std::cout<<"{\"routine\":\""<<only<<"\",\"coverage\":{";bool first=true;
    for(const auto& [k,v]:coverage){std::cout<<(first?"":",")<<"\""<<k<<"\":"<<v;first=false;}
    std::cout<<"},\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}"<<std::endl;
    return mismatches?1:0;
}

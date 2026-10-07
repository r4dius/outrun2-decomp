// Included inside the compare_pc dispatcher; uses its private x86 harness.
if(only=="scene_owner_probe"){
    // Original START scene owner 49BA80 against scene_owner_49ba80. Every
    // external callee is a capture stub recording (entry, EAX, ECX, three
    // stack arguments) and returning a scripted value (+ its first argument
    // for the value getters); the native service returns the same values.
    // Runs cover variants 0..9, presets 0..3, the predicate combinations and
    // one blocking callee at a time; the ordered calls, 836718 and EAX are compared.
    using namespace outrun::platform;
    constexpr unsigned base=0x3a700000,trace=base+0x8000,values=base+0x6000;
    map_at(base,0x40000,PROT_READ|PROT_WRITE|PROT_EXEC);map_original_arithmetic_bridge(file);
    for(unsigned page:{0x836000u,0x780000u,0x7d3000u})
        if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("scene owner globals");
    struct Callee{unsigned pc;unsigned nargs;bool add_arg;};
    const std::vector<Callee> callees{
        {0x440d90,0,false},{0x48b140,0,false},{0x48b180,0,false},{0x48b1a0,0,false},{0x480fe0,3,false},{0x481230,3,false},
        {0x467ac0,0,false},{0x4686c0,0,false},{0x44fce0,1,false},{0x43db00,1,false},{0x4957f0,0,false},{0x48b310,0,false},
        {0x495490,0,false},{0x43dba0,2,false},{0x49b390,1,false},{0x49b3c0,1,false},{0x49b3f0,1,false},{0x49b420,1,false},
        {0x4b00d0,0,false},{0x55a930,0,false},{0x46c520,0,false},{0x4babe0,1,true},{0x44c2d0,0,false},{0x42df90,0,false},
        {0x4276b0,0,false},{0x4299a0,0,false},{0x448980,0,false},{0x427700,1,false},{0x488090,1,true},{0x4f21b0,0,false},
        {0x4999a0,0,false},{0x4962a0,0,false},{0x477210,0,false},{0x46bbe0,1,true},{0x49b870,0,false},{0x448ad0,2,false},
        {0x44dbb0,1,true},{0x44c220,1,true},{0x44a080,0,false},{0x44aa80,3,false},{0x44c2e0,0,false},{0x44c310,2,false},
        {0x4f0380,1,false},{0x4f0400,1,false},{0x46fc30,1,false},{0x4ef860,0,false},{0x4ef850,0,false},{0x44c2c0,0,false},
        {0x45c470,1,false},{0x4f10d0,2,false},{0x4f0430,2,false},{0x46fe50,3,false},{0x4efb50,2,false},{0x4efaf0,2,false},
        {0x4efd20,2,false},{0x44fcc0,1,false},{0x49a650,0,false},{0x47f110,0,false},{0x452e30,2,false},{0x452db0,2,false}};
    unsigned stub_index{};
    auto install=[&](unsigned pc,const std::vector<std::uint8_t>& code){const unsigned dst=base+0x10000+256*stub_index++;
        r060_raw_write(dst,code.data(),code.size());std::array<std::uint8_t,5> jump{{0xe9,0,0,0,0}};
        const auto delta=dst-pc-5;std::memcpy(jump.data()+1,&delta,4);r060_raw_write(pc,jump.data(),jump.size());};
    auto imm=[](std::vector<std::uint8_t>& c,unsigned v){for(unsigned j=0;j<4;++j)c.push_back(std::uint8_t(v>>(8*j)));};
    std::map<unsigned,unsigned> slot;
    for(std::size_t k=0;k<callees.size();++k){
        const auto& e=callees[k];slot[e.pc]=unsigned(k);
        std::vector<std::uint8_t> c{0x52,0x8b,0x15};imm(c,trace);                                  // push edx; mov edx,[trace]
        c.insert(c.end(),{0x6b,0xd2,0x18,0xc7,0x82});imm(c,trace+16);imm(c,e.pc);                  // imul edx,24; mov [edx+t+16],pc
        c.insert(c.end(),{0x89,0x82});imm(c,trace+20);c.insert(c.end(),{0x89,0x8a});imm(c,trace+24);// mov [..+20],eax; mov [..+24],ecx
        for(unsigned i=0;i<3;++i){
            if(i<e.nargs)c.insert(c.end(),{0x8b,0x4c,0x24,std::uint8_t(8+4*i)});else c.insert(c.end(),{0x31,0xc9});
            c.insert(c.end(),{0x89,0x8a});imm(c,trace+28+4*i);}
        c.insert(c.end(),{0xff,0x05});imm(c,trace);                                                // inc [trace]
        c.push_back(0xa1);imm(c,values+4*unsigned(k));                                              // mov eax,[value]
        if(e.add_arg)c.insert(c.end(),{0x03,0x44,0x24,0x08});                                      // add eax,[esp+8]
        c.insert(c.end(),{0x5a,0xc3});install(e.pc,c);                                             // pop edx; ret
    }
    unsigned cases{},mismatches{};const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
    auto check=[&](bool ok,const char* label){++cases;if(!ok){++mismatches;if(mismatches<15)std::cerr<<"scene owner "<<cases<<" "<<label<<"\n";}};
    auto g32=[](unsigned a){return *reinterpret_cast<std::uint32_t*>(a);};
    auto p32=[](unsigned a,std::uint32_t v){*reinterpret_cast<std::uint32_t*>(a)=v;};
    const unsigned course=base+0x7000;
    std::map<unsigned,unsigned> value;
    const std::vector<unsigned> blockers{0x42df90,0x4299a0,0x448980,0x4f21b0,0x427700,0x43dba0,0x44aa80,0x44c310,0x4f10d0,0x4f0430,0x46fe50,0x4efb50,0x4efaf0,0x47f110};
    unsigned ready_runs{},calls_total{};
    for(unsigned variant=0;variant<10;++variant)for(unsigned preset=0;preset<4;++preset)for(unsigned bits=0;bits<32;bits+=(preset==0?1:7))
    for(unsigned blocked=0;blocked<=blockers.size();blocked+=(bits%5==0?1:5)){
        value.clear();
        for(const auto& e:callees)value[e.pc]=0;
        for(unsigned pc:{0x42df90u,0x4299a0u,0x448980u,0x4f21b0u})value[pc]=1;
        value[0x48b140]=0x10+variant;value[0x48b180]=0x1180;value[0x48b1a0]=0x11a0;
        value[0x4957f0]=bits&1;value[0x48b310]=(bits>>1)&1;value[0x4b00d0]=(bits>>2)&1;value[0x55a930]=(bits>>3)&1;
        value[0x4962a0]=(bits>>4)&1;value[0x4999a0]=(bits>>2)&1;value[0x477210]=(bits>>3)&1;value[0x495490]=0;
        value[0x46c520]=bits%4;value[0x44c2d0]=bits&4?0x1c:5;value[0x4babe0]=0x20;value[0x46bbe0]=0x0b;value[0x488090]=0xbe;
        value[0x44dbb0]=0x100;value[0x44c220]=0x200;value[0x45c470]=bits&8?0x300:0;value[0x44c2c0]=0x44;
        if(blocked<blockers.size()){const auto pc=blockers[blocked];value[pc]=(pc==0x42df90||pc==0x4299a0||pc==0x448980||pc==0x4f21b0)?0:1;}
        for(std::size_t k=0;k<callees.size();++k)p32(values+4*unsigned(k),value[callees[k].pc]);
        SceneOwnerInputs in;in.variant_780258=variant;in.preset_78024c=preset;in.course_7d3188=true;
        in.course_18_18=0x1818;in.course_18_1c=0x181c;in.course_64=bits&16?0x6464:0;
        p32(0x780258,variant);p32(0x78024c,preset);
        p32(0x7d3188,course);p32(course+0x18,course+0x100);p32(course+0x118,in.course_18_18);p32(course+0x11c,in.course_18_1c);p32(course+0x64,in.course_64);
        std::uint32_t stage=5;
        struct Call{unsigned pc,eax,ecx;std::array<unsigned,3> a;};
        std::vector<Call> native;
        const SceneOwnerCall service=[&](std::uint32_t pc,std::uint32_t eax,const std::array<std::uint32_t,3>& a)->std::uint32_t{
            native.push_back({pc,eax,0,a});
            const auto& e=callees[slot.at(pc)];
            return value[pc]+(e.add_arg?a[0]:0u);
        };
        for(unsigned step=0;step<6;++step){
            p32(0x836718,stage);p32(trace,0);native.clear();
            prepare(0x49ba80);run_original32();const auto original=guest_call.out_eax;
            const auto result=scene_owner_49ba80(stage,in,service);
            if(inject&&variant==3&&preset==0&&bits==1&&blocked==0&&step==0)stage^=1;
            check(stage==g32(0x836718),"836718 stage");check(result==original,"49BA80 EAX");
            const unsigned n=g32(trace);check(n==native.size(),"call count");calls_total+=n;
            for(unsigned i=0;i<std::min<unsigned>(n,unsigned(native.size()));++i){
                const unsigned at=trace+16+24*i;const auto& c=native[i];const auto& e=callees[slot.at(c.pc)];
                bool ok=g32(at)==c.pc;
                const bool register_arg=c.pc==0x4999a0||c.pc==0x49b390||c.pc==0x49b3c0||c.pc==0x49b3f0||c.pc==0x49b420;
                if(register_arg)ok&=g32(at+4)==c.eax;
                if(c.pc==0x55a930)ok&=g32(at+8)==0x7f9460u;
                for(unsigned k=0;k<e.nargs;++k){
                    const unsigned guest_arg=g32(at+12+4*k);
                    ok&=(c.pc==0x480fe0||c.pc==0x481230)&&k==0?(guest_arg&0xffu)==c.a[k]:guest_arg==c.a[k];
                }
                if(!ok&&mismatches<15){std::cerr<<"  call "<<i<<" guest "<<std::hex<<g32(at)<<" eax "<<g32(at+4)<<" a "<<g32(at+12)<<","<<g32(at+16)
                    <<" native "<<c.pc<<" eax "<<c.eax<<" a "<<c.a[0]<<","<<c.a[1]<<std::dec<<" (variant "<<variant<<" stage "<<stage<<")\n";}
                check(ok,"ordered callee, EAX and arguments");
            }
            if(result==1u){++ready_runs;break;}
        }
    }
    std::cout<<"{\"routine\":\"scene_owner_probe\",\"coverage\":{\"ready_runs\":"<<ready_runs<<",\"calls\":"<<calls_total<<"},\"total_cases\":"<<cases
             <<",\"total_mismatching_cases\":"<<mismatches<<"}\n";return mismatches?1:0;
}

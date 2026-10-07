// Included inside the compare_pc dispatcher; uses its private x86 harness.
if(only=="ending_services_probe"){
    // race_ending_services.cpp against the original leaves: the light / fog setters 407CF0
    // 407D60 407DD0 407E40 407F40 (44A430, D3DX matrices live) 408030 4080A0 408390 4083C0
    // 451800, the camera 483E10, the robot leaves 487D10 / 487D40, ROBDISPWORK 514800 / 514880 /
    // 514830 (its 4066D0 / 4103F0 calls are capture stubs) and the RobMotion leaves 48F4E0 /
    // 4ED890 / 4F1E30 (thiscall). Random memory; the regions they touch, the matrix arena and
    // the stub log are compared.
    using namespace outrun::platform;
    if(argc<=7||std::string(argv[7])=="-")throw std::runtime_error("ending services probe requires the pinned d3dx9_29.dll");
    wire_d3dx_imports(map_d3dx29(argv[7]));
    constexpr unsigned base=0x50000000u,stubs=base,trace=base+0x1000u,Camera=base+0x2000u,Work=base+0x3000u,Motion=base+0x4000u,Vec=base+0x5000u;
    map_at(base,0x10000,PROT_READ|PROT_WRITE|PROT_EXEC);map_original_arithmetic_bridge(file);
    for(std::uint32_t page:{0x899000u,0x89a000u,0x7d3000u,0x79f000u,0x857000u,0x7f2000u})
        if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("ending services pages");
    init_matrix_oracle_globals();
    auto emit32=[](std::vector<std::uint8_t>& b,std::uint32_t v){for(unsigned k=0;k<4;++k)b.push_back(std::uint8_t(v>>(k*8)));};
    // 4066D0 / 4103F0 capture stubs: {pc, arg0, arg1} per call at trace+0x10, count at trace.
    unsigned stub_off=0;
    for(std::uint32_t pc:{0x4066d0u,0x4103f0u}){
        std::vector<std::uint8_t> c{0x51,0xa1};emit32(c,trace);                    // push ecx; mov eax,[count]
        c.insert(c.end(),{0x6b,0xc8,0x0c,0x81,0xc1});emit32(c,trace+0x10);         // imul ecx,eax,12; add ecx,log
        c.insert(c.end(),{0xc7,0x01});emit32(c,pc);                                // mov [ecx],pc
        c.insert(c.end(),{0x8b,0x44,0x24,0x08,0x89,0x41,0x04,0x8b,0x44,0x24,0x0c,0x89,0x41,0x08});   // args 0 / 1
        c.insert(c.end(),{0xff,0x05});emit32(c,trace);c.insert(c.end(),{0x31,0xc0,0x59,0xc3});   // count++; xor eax,eax; pop ecx; ret
        std::memcpy(reinterpret_cast<void*>(stubs+stub_off),c.data(),c.size());
        r046_write_rel32(pc,0xe9,stubs+stub_off);stub_off+=0x80u;
    }
    std::mt19937 rng(0x48e10000u);
    auto rnd=[&](std::uint32_t n){return n?std::uint32_t(rng()%n):0u;};
    auto real=[&](float a,float b){return a+(b-a)*float(rng()%100000)/100000.f;};
    auto fb=[](float f){std::uint32_t v;std::memcpy(&v,&f,4);return v;};
    unsigned cases{},mismatches{};const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
    std::map<std::string,unsigned> coverage;
    auto compare=[&](const void* native,const void* original,std::size_t n,const std::string& what){
        ++cases;if(!std::memcmp(native,original,n))return;
        ++mismatches;const auto* a=static_cast<const std::uint8_t*>(native);const auto* b=static_cast<const std::uint8_t*>(original);
        for(std::size_t i=0;i<n;++i)if(a[i]!=b[i]){if(mismatches<20)std::cerr<<what<<" offset=0x"<<std::hex<<i<<" PC="<<unsigned(b[i])<<" native="<<unsigned(a[i])<<std::dec<<"\n";break;}
    };
    struct Region{std::uint32_t base,size;};
    const Region regions[]={{0x899b98u,0x960u},{0x7d3a10u,0x54u},{0x79f574u,4u},{Camera,0x400u},{Work,0x100u},{0x8577d8u,0x300u},{Motion,0xa4u},{Vec,0x100u},{0x7f2560u,0x180u}};
    static const std::uint32_t Lights[]={0x407cf0u,0x407d60u,0x407dd0u,0x407e40u,0x407f40u,0x408030u,0x4080a0u,0x408390u,0x4083c0u,0x451800u};
    for(unsigned iter=0;iter<count*4000u;++iter){
        const unsigned kind=rnd(21);
        // memory
        for(const auto& r:regions)for(std::uint32_t a=0;a<r.size;a+=4)*reinterpret_cast<std::uint32_t*>(r.base+a)=rnd(3)?fb(real(-4,4)):std::uint32_t(rng());
        *reinterpret_cast<std::uint32_t*>(0x79f574u)=Camera;
        *reinterpret_cast<std::uint32_t*>(Work)=rnd(8);                                       // ROBDISPWORK index
        for(unsigned k=0;k<16;++k)*reinterpret_cast<float*>(Vec+0x40+k*4)=real(-2,2);
        std::memset(reinterpret_cast<void*>(trace),0,0x100);
        std::vector<std::vector<std::uint8_t>> native;
        for(const auto& r:regions)native.emplace_back(reinterpret_cast<const std::uint8_t*>(r.base),reinterpret_cast<const std::uint8_t*>(r.base)+r.size);
        reset_matrix_oracle_globals();
        std::vector<std::uint8_t> arena(0x400);for(unsigned k=0;k<16;++k){float f=real(-2,2);std::memcpy(arena.data()+k*4,&f,4);}
        std::memcpy(reinterpret_cast<void*>(MatrixArena),arena.data(),arena.size());
        PcRaceMemory mem;
        for(std::size_t i=0;i<EmbeddedExeRangeCount;++i)mem.map_const(EmbeddedExeRanges[i].base,EmbeddedExeRanges[i].data,EmbeddedExeRanges[i].size);
        for(std::size_t i=0;i<std::size(regions);++i)mem.map(regions[i].base,native[i].data(),native[i].size());
        outrun::driving::PcMatrixStack stack{outrun::driving::Bytes(arena.data(),arena.size()),0,0,64};
        std::vector<std::array<std::uint32_t,3>> log;
        PcRaceContext ctx{mem,stack,[&](const PcRaceCall& k)->std::uint32_t{log.push_back({k.pc,k.args[0],k.args[1]});return 0u;},nullptr};
        std::string tag;
        if(kind<10){
            PcRaceCall k;k.pc=Lights[kind];k.argc=6;
            k.args[0]=rnd(3);k.args[1]=rnd(3);k.args[2]=rnd(2);
            for(unsigned a=3;a<6;++a)k.args[a]=fb(real(-3.2f,3.2f));
            if(k.pc==0x408390u||k.pc==0x4083c0u||k.pc==0x451800u){k.args[0]=rnd(3);for(unsigned a=1;a<5;++a)k.args[a]=fb(real(-3,3));}
            prepare(k.pc);auto* st=reinterpret_cast<unsigned*>(S);for(unsigned a=0;a<6;++a)st[a]=k.args[a];run();
            (void)ending_light_service(mem,stack,k);
            tag="light "+std::to_string(kind);
        }else switch(kind){
        case 10:{const unsigned e=rnd(4)?Vec:0u,t=rnd(4)?Vec+0x10:0u,r=rnd(4)?Vec+0x20:0u,f=rnd(4)?Vec+0x30:0u;
            prepare(0x483e10u);auto* st=reinterpret_cast<unsigned*>(S);st[0]=e;st[1]=t;st[2]=r;st[3]=f;run();
            camera_set_483e10(mem,e,t,r,f);tag="483E10";break;}
        case 11:{const unsigned m=rnd(3)?Vec+0x40:0u;
            prepare(0x487d10u);auto* st=reinterpret_cast<unsigned*>(S);st[0]=Work;st[1]=m;run();
            robot_matrix_487d10(mem,stack,Work,m);tag=m?"487D10 copy":"487D10 current";break;}
        case 12:{const unsigned on=rnd(2);
            prepare(0x487d40u);auto* st=reinterpret_cast<unsigned*>(S);st[0]=Work;st[1]=on;run();
            robot_flag_487d40(mem,Work,on);tag="487D40";break;}
        case 13:{const unsigned v=rng(),w=rnd(2);
            prepare(0x514800u);auto* st=reinterpret_cast<unsigned*>(S);st[0]=Work;st[1]=v;st[2]=w;run();
            rob_disp_word_514800(mem,Work,v,w);tag="514800";break;}
        case 14:{const unsigned w=rnd(2);
            prepare(0x514880u);auto* st=reinterpret_cast<unsigned*>(S);st[0]=Work;st[1]=Vec;st[2]=w;run();
            rob_disp_vector_514880(mem,Work,Vec,w);tag="514880";break;}
        case 15:{   // the model: sometimes in the previous resource, sometimes -1
            const std::uint32_t d=*reinterpret_cast<std::uint32_t*>(Work)*0x60u+0x8577d8u;
            std::uint32_t model=rnd(4)==0?0xffffffffu:rng();
            if(rnd(2))model=(*reinterpret_cast<std::uint32_t*>(d+0x14)&0xffff0000u)|(model&0xffffu);
            const unsigned frame=rng();
            prepare(0x514830u);auto* st=reinterpret_cast<unsigned*>(S);st[0]=Work;st[1]=model;st[2]=frame;run();
            rob_disp_model_514830(ctx,Work,model,frame);tag="514830";break;}
        case 16:{prepare(0x48f4e0u);guest_call.ecx=Motion;run_original32();
            const std::uint32_t pc_eax=guest_call.out_eax;const std::uint32_t nat=rob_motion_word_48f4e0(mem,Motion);
            compare(&nat,&pc_eax,4,"48F4E0 result");tag="48F4E0";break;}
        case 17:{const unsigned v=fb(real(-2,2));
            prepare(0x4ed890u);guest_call.ecx=Motion;reinterpret_cast<unsigned*>(S)[0]=v;run_original32();
            rob_motion_speed_4ed890(mem,Motion,v);tag="4ED890";break;}
        case 20:{   // 45BF30: stage scores and counts (counts 0 / negative sometimes)
            for(unsigned i=0;i<5;++i){*reinterpret_cast<std::int32_t*>(0x7f2564u+i*0x44u)=std::int32_t(rnd(3)?rnd(200000):rng());
                *reinterpret_cast<std::uint8_t*>(0x7f2588u+i*0x44u)=std::uint8_t(rnd(4)?1u+rnd(5):rng());}
            native[8].assign(reinterpret_cast<const std::uint8_t*>(0x7f2560u),reinterpret_cast<const std::uint8_t*>(0x7f2560u)+0x180u);
            const unsigned n=rnd(4)?rnd(6):rng();
            prepare(0x45bf30u);reinterpret_cast<unsigned*>(S)[0]=n;run();
            const std::uint32_t pc_eax=guest_call.out_eax,nat=navi_score_mean_45bf30(mem,n);
            compare(&nat,&pc_eax,4,"45BF30 result");tag="45BF30";break;}
        default:{   // 4F1E30: around 0 and the end, NaN sometimes
            float f=rnd(8)==0?std::nanf(""):real(-2,6);
            *reinterpret_cast<float*>(Motion+0x14)=rnd(10)==0?std::nanf(""):real(0,4);native[6].assign(reinterpret_cast<const std::uint8_t*>(Motion),reinterpret_cast<const std::uint8_t*>(Motion)+0xa4);
            prepare(0x4f1e30u);guest_call.ecx=Motion;reinterpret_cast<unsigned*>(S)[0]=fb(f);run_original32();
            rob_motion_frame_end_4f1e30(mem,Motion,f);tag="4F1E30";break;}
        }
        ++coverage[tag];
        if(inject&&iter==77)native[0][0x10]^=1;
        for(std::size_t i=0;i<std::size(regions);++i){std::ostringstream s;s<<tag<<" region "<<std::hex<<regions[i].base;
            compare(native[i].data(),reinterpret_cast<const void*>(regions[i].base),native[i].size(),s.str());}
        compare(arena.data(),reinterpret_cast<const void*>(MatrixArena),arena.size(),tag+" matrix arena");
        // the 4066D0 / 4103F0 log
        const std::uint32_t n=*reinterpret_cast<const std::uint32_t*>(trace);
        std::vector<std::uint32_t> pcl(n*3),nl(log.size()*3);
        std::memcpy(pcl.data(),reinterpret_cast<const void*>(trace+0x10),pcl.size()*4);
        for(std::size_t i=0;i<log.size();++i){nl[i*3]=log[i][0];nl[i*3+1]=log[i][1];nl[i*3+2]=log[i][2];}
        ++cases;if(pcl!=nl){++mismatches;if(mismatches<20)std::cerr<<tag<<" service log PC="<<n<<" native="<<log.size()<<"\n";}
    }
    std::cout<<"{\"routine\":\""<<only<<"\",\"coverage\":{";bool first=true;
    for(const auto& [k,v]:coverage){std::cout<<(first?"":",")<<"\""<<k<<"\":"<<v;first=false;}
    std::cout<<"},\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}"<<std::endl;
    return mismatches?1:0;
}

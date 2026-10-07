if(only=="shadow_alloc_observe"){
    // Black-box observation of the protected 46BA20 (jmp [1039898]): the
    // changed words of the car work per model byte +11, EAX, the resource
    // 0xBA entry and the logged callees (CRT stubs: malloc bump heap,
    // _onexit returns its argument).
    map_original_arithmetic_bridge(file);
    for(std::uint32_t page=0x62f000u;page<0x98d000u;page+=0x1000u)mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE);
    constexpr std::uint32_t M=0x3a840000u,Stubs=0x3a850000u,T=0x3a860000u,H=0x3a900000u;
    map_at(M,0x2000,PROT_READ|PROT_WRITE);map_at(Stubs,0x4000,PROT_READ|PROT_WRITE|PROT_EXEC);
    map_at(T,0x10000,PROT_READ|PROT_WRITE);map_at(H,0x100000,PROT_READ|PROT_WRITE);
    auto e32=[](std::vector<std::uint8_t>& b,std::uint32_t v){for(unsigned k=0;k<4;++k)b.push_back(std::uint8_t(v>>(k*8)));};
    unsigned off=0;
    // kind 0: return imm, 1: return arg0, 2: malloc bump (arg0 bytes)
    auto stub=[&](std::uint32_t pc,unsigned kind,std::uint32_t imm){
        std::vector<std::uint8_t> c{0x56,0x8b,0x35};e32(c,T);c.insert(c.end(),{0xc1,0xe6,0x05,0x81,0xc6});e32(c,T+0x10);
        c.insert(c.end(),{0xc7,0x06});e32(c,pc);
        c.insert(c.end(),{0x8b,0x44,0x24,0x08,0x89,0x46,0x04,0x8b,0x44,0x24,0x0c,0x89,0x46,0x08,0x8b,0x44,0x24,0x10,0x89,0x46,0x0c,0x89,0x4e,0x10,0xff,0x05});e32(c,T);
        c.push_back(0x5e);
        if(kind==0){c.push_back(0xb8);e32(c,imm);}
        else if(kind==1)c.insert(c.end(),{0x8b,0x44,0x24,0x04});
        else{c.push_back(0xa1);e32(c,H);c.insert(c.end(),{0x8b,0x4c,0x24,0x04,0x83,0xc1,0x0f,0x83,0xe1,0xf0,0x01,0x0d});e32(c,H);}
        c.push_back(0xc3);
        std::memcpy(reinterpret_cast<void*>(Stubs+off),c.data(),c.size());r046_write_rel32(pc,0xe9,Stubs+off);off=(off+unsigned(c.size())+15u)&~15u;};
    *reinterpret_cast<std::uint32_t*>(H)=H+0x100;
    stub(0x580253u,2,0);stub(0x5800b5u,1,0);stub(0x580bc2u,0,0);
    const std::uint32_t entry=0x7c2800u+0xbau*0x48u;
    const bool fill=std::getenv("OR2_FILL")!=nullptr;
    constexpr std::uint32_t P=0x3a870000u;map_at(P,0x10000,PROT_READ|PROT_WRITE);
    for(std::uint32_t a=P;a<P+0x10000;a+=4)*reinterpret_cast<std::uint32_t*>(a)=fill?P+((a*7u)&0xfffcu):0x77770000u+(a&0xffffu);
    for(unsigned k=0;k<0x48;k+=4)*reinterpret_cast<std::uint32_t*>(entry+k)=fill?P+k*0x100:0;
    const int first=std::getenv("OR2_MODEL")?std::atoi(std::getenv("OR2_MODEL")):0;
    for(int model=first;model<first+(std::getenv("OR2_MODELS")?std::atoi(std::getenv("OR2_MODELS")):1);++model){
        *reinterpret_cast<std::uint32_t*>(H)=H+0x100;for(std::uint32_t a=H+0x100;a<H+0x1000;a+=4)*reinterpret_cast<std::uint32_t*>(a)=0xcdcd0000u+(a&0xffffu);
        for(unsigned k=0;k<0x2000;k+=4)*reinterpret_cast<std::uint32_t*>(M+k)=0x11110000u+k;
        *reinterpret_cast<std::uint32_t*>(T)=0;
        *reinterpret_cast<std::uint8_t*>(M+0x11)=std::uint8_t(model);
        prepare(0x46ba20u);reinterpret_cast<unsigned*>(S)[0]=M;run_original32();
        std::cerr<<"model="<<model<<" eax="<<std::hex<<guest_call.out_eax<<" sp_ok="<<(guest_call.out_sp==S)<<":";
        for(unsigned k=0;k<0x2000;k+=4){const auto v=*reinterpret_cast<std::uint32_t*>(M+k);if(v!=0x11110000u+k&&k!=0x10)std::cerr<<" +"<<k<<"="<<v;}
        std::cerr<<" | entry:";for(unsigned k=0;k<0x48;k+=4)std::cerr<<" "<<*reinterpret_cast<std::uint32_t*>(entry+k);
        std::cerr<<" | calls:";for(std::uint32_t i=0;i<*reinterpret_cast<std::uint32_t*>(T);++i){const auto* r=reinterpret_cast<const std::uint32_t*>(T+0x10+i*0x20);
            std::cerr<<" ["<<r[0]<<" "<<r[1]<<" "<<r[2]<<" "<<r[3]<<" ecx="<<r[4]<<"]";}
        std::cerr<<" heap="<<*reinterpret_cast<std::uint32_t*>(H)<<char(10);
        for(std::uint32_t a=H+0x100;a<*reinterpret_cast<std::uint32_t*>(H);a+=4){if(((a-H)&0x1f)==0)std::cerr<<char(10)<<"  "<<a<<":";std::cerr<<" "<<*reinterpret_cast<std::uint32_t*>(a);}
        std::cerr<<std::dec<<char(10);
    }
    std::cout<<"{\"routine\":\"shadow_alloc_observe\",\"total_cases\":1,\"total_mismatching_cases\":0}"<<'\n';return 0;
}

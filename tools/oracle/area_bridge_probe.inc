if(only=="area_bridge_probe"){
    // Black-box observation of a protected bridge reached by JMP from inside a
    // function: the harness enters at OR2_ENTRY (the bridge's JMP site) with
    // the registers of OR2_REGS (hex eax,ecx,edx,ebx,esi,edi,ebp), the
    // continuation OR2_CONT is patched to a capture stub recording every
    // register. OR2_OWN=1 fills 62F000..98D000 with each dword's own address
    // first, so a register loaded from memory names its source.
    // Measurement only: prints registers, compares nothing.
    const char* e=std::getenv("OR2_ENTRY");const char* k=std::getenv("OR2_CONT");const char* rg=std::getenv("OR2_REGS");
    if(!e||!k||!rg)throw std::runtime_error("set OR2_ENTRY, OR2_CONT and OR2_REGS");
    const std::uint32_t entry=std::uint32_t(std::strtoul(e,nullptr,16)),cont=std::uint32_t(std::strtoul(k,nullptr,16));
    std::uint32_t r[7]{};{std::string s=rg;std::size_t p=0;for(unsigned i=0;i<7&&p<s.size();++i){const auto q=s.find(',',p);r[i]=std::uint32_t(std::strtoul(s.substr(p,q-p).c_str(),nullptr,16));if(q==std::string::npos)break;p=q+1;}}
    constexpr unsigned base=0x3a800000,buf=base+0x100,stub=base+0x200;
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
    r046_write_rel32(cont,0xe9,stub);
    if(const char* own=std::getenv("OR2_OWN");own&&std::string(own)=="1")for(std::uint32_t a=0x62f000u;a<0x98d000u;a+=4)*reinterpret_cast<std::uint32_t*>(a)=a;
    if(const char* pk=std::getenv("OR2_POKE")){std::string s=pk;std::size_t p=0;while(p<s.size()){const auto q=s.find(',',p);const auto item=s.substr(p,q-p);const auto eq=item.find('=');
        *reinterpret_cast<std::uint32_t*>(std::uintptr_t(std::strtoul(item.substr(0,eq).c_str(),nullptr,16)))=std::uint32_t(std::strtoul(item.substr(eq+1).c_str(),nullptr,16));if(q==std::string::npos)break;p=q+1;}}
    prepare(entry);guest_call.eax=r[0];guest_call.ecx=r[1];guest_call.edx=r[2];guest_call.ebx=r[3];guest_call.esi=r[4];guest_call.edi=r[5];guest_call.ebp=r[6];
    run_original32();
    const auto* o=reinterpret_cast<const std::uint32_t*>(buf);
    std::cerr<<std::hex<<"eax="<<o[0]<<" ecx="<<o[1]<<" edx="<<o[2]<<" ebx="<<o[3]<<" esp="<<o[4]<<" ebp="<<o[5]<<" esi="<<o[6]<<" edi="<<o[7]<<" fl="<<(o[8]&0x8d5)<<" (S="<<S<<")"<<std::dec<<char(10);
    if(const char* pk=std::getenv("OR2_PEEK")){std::string s=pk;std::size_t p=0;while(p<s.size()){const auto q=s.find(',',p);const auto a=std::uint32_t(std::strtoul(s.substr(p,q-p).c_str(),nullptr,16));
        std::cerr<<std::hex<<"["<<a<<"]="<<*reinterpret_cast<const std::uint32_t*>(std::uintptr_t(a))<<std::dec<<char(10);if(q==std::string::npos)break;p=q+1;}}   // OR2_PEEK: dwords after the bridge
    std::cout<<"{\"routine\":\"area_bridge_probe\",\"total_cases\":1,\"total_mismatching_cases\":0}"<<'\n';return 0;
}

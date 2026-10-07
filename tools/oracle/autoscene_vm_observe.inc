// Included inside the compare_pc dispatcher; uses its private x86 harness.
if(only=="autoscene_vm_observe"){
    // Measures the protected compare at 4B576D (4B5750): the node names (EXE strings of 9+
    // characters) whose 10-byte compare matches, seen as a 40A290 call.
    {   const std::size_t text_file=pe_file_offset(file,0x401000u);
        for(std::uint32_t a=0x401000u;a<0x401000u+0x194881u;a+=0x100u){
            const std::size_t n=std::min<std::size_t>(0x100u,0x401000u+0x194881u-a);
            if(std::memcmp(reinterpret_cast<void*>(a),file.data()+text_file+(a-0x401000u),n))r060_raw_write(a,file.data()+text_file+(a-0x401000u),n);
        }
    }
    map_original_arithmetic_bridge(file);
    constexpr unsigned base=0x3bc00000u,hits=base+0x100u,node=base+0x1000u,name=base+0x2000u;
    map_at(base,0x10000,PROT_READ|PROT_WRITE|PROT_EXEC);
    for(std::uint32_t page=0x62f000u;page<0x98d000u;page+=0x1000u)mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE);
    // 40A290 counts in [hits]; the other leaves return.
    {std::uint8_t c[]{0xff,0x05,0,0,0,0,0xc3};std::memcpy(c+2,&hits,4);std::memcpy(reinterpret_cast<void*>(base),c,sizeof c);r046_write_rel32(0x40a290u,0xe9,base);}
    {std::uint8_t ret[]{0xc3};std::memcpy(reinterpret_cast<void*>(base+0x40),ret,1);
     for(std::uint32_t pc:{0x409ef0u,0x40a010u,0x40a4e0u,0x405360u,0x40a0d0u})r046_write_rel32(pc,0xe9,base+0x40);}
    {std::uint8_t c[]{0xff,0x05,0,0,0,0,0xc3};const std::uint32_t h2=hits+4;std::memcpy(c+2,&h2,4);std::memcpy(reinterpret_cast<void*>(base+0x60),c,sizeof c);r046_write_rel32(0x40a2d0u,0xe9,base+0x60);}
    {   // 4B5772 (mov ecx,0xA): record EDI (the compared string) in [hits+8], then resume at 4B5777.
        std::vector<std::uint8_t> c{0x89,0x3d};const std::uint32_t h3=hits+8;for(unsigned k=0;k<4;++k)c.push_back(std::uint8_t(h3>>(k*8)));
        c.insert(c.end(),{0xb9,0x0a,0,0,0,0xe9,0,0,0,0});
        const std::uint32_t at=base+0x80;std::memcpy(reinterpret_cast<void*>(at),c.data(),c.size());
        const std::int32_t rel=std::int32_t(0x4b5777u-(at+std::uint32_t(c.size())));std::memcpy(reinterpret_cast<void*>(at+c.size()-4),&rel,4);
        r046_write_rel32(0x4b5772u,0xe9,at);
    }
    unsigned found=0,tried=0;
    for(std::uint32_t a=0x596000u;a<0x5a0000u;){
        std::uint32_t e=a;while(e<0x1450000u&&*reinterpret_cast<const unsigned char*>(e)>=0x20&&*reinterpret_cast<const unsigned char*>(e)<0x7f)++e;
        if(e<0x1450000u&&*reinterpret_cast<const unsigned char*>(e)==0&&e-a>=2u){
            ++tried;
            std::memset(reinterpret_cast<void*>(name),0,0x100);std::memcpy(reinterpret_cast<void*>(name),reinterpret_cast<void*>(a),10);   // the 10 bytes compared, as stored
            std::memset(reinterpret_cast<void*>(node),0,0x44);
            *reinterpret_cast<std::uint32_t*>(node)=0x100u;*reinterpret_cast<std::uint32_t*>(node+8)=0x123456u;*reinterpret_cast<std::uint32_t*>(node+0xc)=name;
            *reinterpret_cast<std::uint32_t*>(hits)=0;*reinterpret_cast<std::uint32_t*>(hits+4)=0;
            prepare(0x4b5750u);reinterpret_cast<unsigned*>(S)[0]=base+0x3000u;reinterpret_cast<unsigned*>(S)[1]=node;run();
            if(*reinterpret_cast<std::uint32_t*>(hits)){++found;std::cout<<"match at "<<std::hex<<a<<std::dec<<": "<<reinterpret_cast<const char*>(name)<<std::endl;}
        }
        a=e+1;
    }
    {const std::uint32_t edi=*reinterpret_cast<std::uint32_t*>(hits+8);std::cout<<"EDI at the compare "<<std::hex<<edi<<std::dec;
     if(edi>=0x400000u&&edi<0x1450000u){std::cout<<" bytes:";for(unsigned k=0;k<16;++k)std::cout<<" "<<std::hex<<unsigned(*reinterpret_cast<const unsigned char*>(edi+k));
         std::cout<<" '"<<std::string(reinterpret_cast<const char*>(edi),strnlen(reinterpret_cast<const char*>(edi),16))<<"'";}
     std::cout<<std::dec<<std::endl;}
    std::cout<<"tried "<<tried<<" found "<<found<<" translate calls (last run) "<<*reinterpret_cast<std::uint32_t*>(hits+4)<<std::endl;
    return 0;
}

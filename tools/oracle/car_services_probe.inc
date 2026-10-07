// Included inside the compare_pc dispatcher; uses its private x86 harness.
// Car-control leaf services (GamePlCar_Ctrl 4A8330 / CommonPlCar 4A8100).
if(only=="car_services_measure"){
    // Black-box measurement of the protected pieces. The original bytes of
    // every routine touched here are restored from the pinned file first
    // (other harness setup code patches some of these sites at startup).
    const char* which_env=std::getenv("OR2_MEASURE");const std::string which=which_env?which_env:"all";
    constexpr unsigned base=0x3ac00000,buf=base+0x100,stub=base+0x200,obj=base+0x4000;
    map_at(base,0x20000,PROT_READ|PROT_WRITE|PROT_EXEC);map_original_arithmetic_bridge(file);
    for(std::uint32_t page=0x62f000u;page<0x98d000u;page+=0x1000u)mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE);
    auto restore=[&](std::uint32_t va,std::size_t n){std::vector<std::uint8_t> b(n);for(std::size_t i=0;i<n;++i)b[i]=file[pe_file_offset(file,va+std::uint32_t(i))];r060_raw_write(va,b.data(),n);};
    {   std::vector<std::uint8_t> c;auto imm=[&](std::uint32_t v){for(unsigned i=0;i<4;++i)c.push_back(std::uint8_t(v>>(i*8)));};
        c.push_back(0xa3);imm(buf);
        const std::uint8_t regs[]{0x0d,0x15,0x1d,0x25,0x2d,0x35,0x3d};
        for(unsigned i=0;i<7;++i){c.push_back(0x89);c.push_back(regs[i]);imm(buf+4+i*4);}
        c.push_back(0x9c);c.push_back(0x8f);c.push_back(0x05);imm(buf+32);
        c.push_back(0xbc);imm(S-4);c.push_back(0xc3);
        std::memcpy(reinterpret_cast<void*>(stub),c.data(),c.size());
    }
    auto regs=[&]{return reinterpret_cast<const std::uint32_t*>(buf);};
    auto dump=[&](const std::string& label){const auto* r=regs();
        std::cerr<<label<<std::hex<<" eax="<<r[0]<<" ecx="<<r[1]<<" edx="<<r[2]<<" ebx="<<r[3]<<" ebp="<<r[5]<<" esi="<<r[6]<<" edi="<<r[7]<<" fl="<<(r[8]&0x8d5)<<std::dec<<char(10);};
    auto capture_at=[&](std::uint32_t cont){r046_write_rel32(cont,0xe9,stub);};
    auto p32=[](std::uint32_t a,std::uint32_t v){*reinterpret_cast<std::uint32_t*>(a)=v;};
    std::vector<std::uint8_t> keep(0x98d000u-0x62f000u);std::memcpy(keep.data(),reinterpret_cast<void*>(0x62f000u),keep.size());
    auto own_address_data=[&]{for(std::uint32_t a=0x62f000u;a<0x98d000u;a+=4)p32(a,a);};
    auto restore_data=[&]{std::memcpy(reinterpret_cast<void*>(0x62f000u),keep.data(),keep.size());};
    if(which=="all"||which=="4a3f8a"){
        restore(0x4a3f80u,0x90);capture_at(0x4a3f90u);
        for(unsigned k=0;k<0x1100;k+=4)p32(obj+k,0x70000000u+k);
        for(std::uint32_t hint:{0u,5u,0xffffffffu}){prepare(0x4a3f80u);auto* st=reinterpret_cast<std::uint32_t*>(S);st[0]=obj;st[1]=hint;
            guest_call.esi=0x12340000u;guest_call.ebx=0x56780000u;run_original32();dump("4A3F8A event words=0x70000000+off hint="+std::to_string(hint));}
        for(std::int32_t v:{0,1,5,-1,-2,0x7fffffff}){p32(obj+0x10d0u,std::uint32_t(v));prepare(0x4a3f80u);auto* st=reinterpret_cast<std::uint32_t*>(S);st[0]=obj;st[1]=3;run_original32();dump("4A3F8A 10d0="+std::to_string(v));}
        own_address_data();prepare(0x4a3f80u);{auto* st=reinterpret_cast<std::uint32_t*>(S);st[0]=obj;st[1]=3;}run_original32();dump("4A3F8A data=own address");restore_data();
    }
    if(which=="all"||which=="4a45f9"){
        restore(0x4a45f0u,0x10);capture_at(0x4a45ffu);
        for(unsigned k=0;k<0x1100;k+=4)p32(obj+k,0x70000000u+k);
        prepare(0x4a45f0u);reinterpret_cast<std::uint32_t*>(S)[0]=obj;guest_call.edi=0x12340000u;run_original32();dump("4A45F9 event words=0x70000000+off");
        own_address_data();prepare(0x4a45f0u);reinterpret_cast<std::uint32_t*>(S)[0]=obj;guest_call.edi=0x12340000u;run_original32();dump("4A45F9 data=own address");restore_data();
        for(unsigned k=0;k<0x1100;k+=4)p32(obj+k,0);
        prepare(0x4a45f0u);reinterpret_cast<std::uint32_t*>(S)[0]=obj;guest_call.edi=0x12340000u;run_original32();dump("4A45F9 event zero");
    }
    if(which=="45a2b0"){
        // Two capture stubs: 45A2B9 (byte table path) marks 1, 45A2C4 (mode path) marks 2.
        restore(0x45a2b0u,0x60);
        auto marker_stub=[&](std::uint32_t at,std::uint32_t mark){std::vector<std::uint8_t> c{0xc7,0x05};for(unsigned i=0;i<4;++i)c.push_back(std::uint8_t((buf+0x40)>>(8*i)));for(unsigned i=0;i<4;++i)c.push_back(std::uint8_t(mark>>(8*i)));
            c.push_back(0xe9);const std::uint32_t rel=stub-(at+std::uint32_t(c.size())+4u);for(unsigned i=0;i<4;++i)c.push_back(std::uint8_t(rel>>(8*i)));std::memcpy(reinterpret_cast<void*>(at),c.data(),c.size());};
        marker_stub(base+0x400,1);marker_stub(base+0x440,2);
        r046_write_rel32(0x45a2b9u,0xe9,base+0x400);r046_write_rel32(0x45a2c4u,0xe9,base+0x440);
        auto run_id=[&](std::uint32_t id){p32(buf+0x40,0);prepare(0x45a2b0u);reinterpret_cast<std::uint32_t*>(S)[0]=id;run_original32();return *reinterpret_cast<std::uint32_t*>(buf+0x40);};
        for(std::uint32_t id:{0u,1u,5u,0xffu}){const auto m=run_id(id);dump("45A2B0 id="+std::to_string(id)+" path="+std::to_string(m));}
        const auto base_path=run_id(1);
        std::vector<std::uint8_t> saved(0x98d000u-0x62f000u);std::memcpy(saved.data(),reinterpret_cast<void*>(0x62f000u),saved.size());
        for(std::uint8_t fill:{0x00u,0x01u,0x55u,0xffu}){
            std::uint32_t lo=0x62f000u,hi=0x98d000u;bool found=true;
            std::memcpy(reinterpret_cast<void*>(0x62f000u),saved.data(),saved.size());std::memset(reinterpret_cast<void*>(lo),fill,hi-lo);
            const auto all=run_id(1);if(all==base_path){std::cerr<<"fill "<<unsigned(fill)<<": no change"<<char(10);continue;}
            for(unsigned depth=0;depth<24&&hi-lo>1;++depth){
                const auto mid=lo+(hi-lo)/2;
                std::memcpy(reinterpret_cast<void*>(0x62f000u),saved.data(),saved.size());std::memset(reinterpret_cast<void*>(lo),fill,mid-lo);
                if(run_id(1)!=base_path)hi=mid;else{
                    std::memcpy(reinterpret_cast<void*>(0x62f000u),saved.data(),saved.size());std::memset(reinterpret_cast<void*>(mid),fill,hi-mid);
                    if(run_id(1)!=base_path)lo=mid;else{std::cerr<<"no single region "<<std::hex<<lo<<"-"<<hi<<std::dec<<char(10);found=false;break;}
                }
            }
            if(found)std::cerr<<"fill "<<unsigned(fill)<<": sensitive byte "<<std::hex<<lo<<std::dec<<char(10);
            std::memcpy(reinterpret_cast<void*>(0x62f000u),saved.data(),saved.size());
            if(found){for(std::uint32_t v:{0u,1u,2u,3u,4u,0x100u,0xffffffffu}){p32(lo&~3u,v);for(std::uint32_t id:{0u,1u,3u,7u}){const auto m=run_id(id);std::cerr<<"  dword "<<std::hex<<(lo&~3u)<<"="<<v<<" id="<<id<<" path="<<m<<" eax="<<regs()[0]<<std::dec<<char(10);}}
                std::memcpy(reinterpret_cast<void*>(0x62f000u),saved.data(),saved.size());break;}
        }
        restore_data();
    }
    std::cout<<"{\"routine\":\"car_services_measure\",\"total_cases\":1,\"total_mismatching_cases\":0}"<<'\n';return 0;
}

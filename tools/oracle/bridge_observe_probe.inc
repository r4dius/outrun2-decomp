if(only=="bridge_4c50d0_probe"){
    // Black-box observation of the protected 4C50D0 (jmp [1039BF4]): runs the
    // original with each start value of the transition globals and reports
    // the words it changes (84A9FD active byte, 68A66C..68A674 floats).
    map_original_arithmetic_bridge(file);
    for(std::uint32_t page=0x62f000u;page<0x98d000u;page+=0x1000u)mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE);
    unsigned mismatches=0;
    for(unsigned start=0;start<3;++start){
        *reinterpret_cast<std::uint8_t*>(0x84a9fdu)=std::uint8_t(start==2?0x55:start);
        const std::uint32_t before[4]{*reinterpret_cast<std::uint32_t*>(0x68a66cu),*reinterpret_cast<std::uint32_t*>(0x68a670u),
            *reinterpret_cast<std::uint32_t*>(0x68a674u),*reinterpret_cast<std::uint32_t*>(0x84a9fcu)};
        prepare(0x4c50d0);run_original32();
        const std::uint32_t after[4]{*reinterpret_cast<std::uint32_t*>(0x68a66cu),*reinterpret_cast<std::uint32_t*>(0x68a670u),
            *reinterpret_cast<std::uint32_t*>(0x68a674u),*reinterpret_cast<std::uint32_t*>(0x84a9fcu)};
        std::cerr<<"start "<<start<<" eax "<<std::hex<<guest_call.out_eax;
        for(unsigned k=0;k<4;++k)std::cerr<<" ["<<before[k]<<"->"<<after[k]<<"]";
        std::cerr<<std::dec<<char(10);
    }
    std::cout<<"{\"routine\":\"bridge_4c50d0_probe\",\"total_cases\":3,\"total_mismatching_cases\":"<<mismatches<<"}"<<'\n';return 0;
}
if(only=="bridge_4b00d0_probe"){
    // Black-box observation of the protected 4B00D0 (jmp [1039988]) used by
    // 49BA80 stage 8/61: result against the 8421C0 scene state word.
    map_original_arithmetic_bridge(file);
    for(std::uint32_t page=0x62f000u;page<0x98d000u;page+=0x1000u)mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE);
    for(std::uint32_t value:{0u,1u,2u,3u,4u,5u,6u,0x100u,0xffffffffu}){
        *reinterpret_cast<std::uint32_t*>(0x8421c0u)=value;
        prepare(0x4b00d0);run_original32();
        std::cerr<<"8421C0="<<std::hex<<value<<" eax="<<guest_call.out_eax<<std::dec<<char(10);
    }
    std::cout<<"{\"routine\":\"bridge_4b00d0_probe\",\"total_cases\":9,\"total_mismatching_cases\":0}"<<'\n';return 0;
}
if(only=="bridge_43f860_probe"){
    // Black-box observation of the protected 43F860 (jmp 103BE8F): which data
    // bytes change its result. Bisects the writable data range by filling it.
    map_original_arithmetic_bridge(file);
    for(std::uint32_t page=0x62f000u;page<0x98d000u;page+=0x1000u)mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE);
    auto run=[&]{prepare(0x43f860);run_original32();return guest_call.out_eax&255u;};
    std::vector<std::uint8_t> saved(0x98d000u-0x62f000u);std::memcpy(saved.data(),reinterpret_cast<void*>(0x62f000u),saved.size());
    const auto base_value=run();std::cerr<<"baseline "<<base_value<<char(10);
    std::uint32_t lo=0x62f000u,hi=0x98d000u;
    for(unsigned depth=0;depth<24&&hi-lo>1;++depth){
        const auto mid=lo+(hi-lo)/2;
        std::memcpy(reinterpret_cast<void*>(0x62f000u),saved.data(),saved.size());
        std::memset(reinterpret_cast<void*>(lo),0x01,mid-lo);
        if(run()!=base_value)hi=mid;else{
            std::memcpy(reinterpret_cast<void*>(0x62f000u),saved.data(),saved.size());
            std::memset(reinterpret_cast<void*>(mid),0x01,hi-mid);
            if(run()!=base_value)lo=mid;else{std::cerr<<"no single region between "<<std::hex<<lo<<"-"<<hi<<std::dec<<char(10);break;}
        }
    }
    std::cerr<<"sensitive byte "<<std::hex<<lo<<std::dec<<char(10);
    std::memcpy(reinterpret_cast<void*>(0x62f000u),saved.data(),saved.size());
    for(std::uint32_t v:{0u,1u,2u,0x100u}){*reinterpret_cast<std::uint32_t*>(lo&~3u)=v;std::cerr<<"dword "<<std::hex<<(lo&~3u)<<"="<<v<<" -> "<<run()<<std::dec<<char(10);}
    std::cout<<"{\"routine\":\"bridge_43f860_probe\",\"total_cases\":1,\"total_mismatching_cases\":0}"<<'\n';return 0;
}
if(only=="bridge_getter_probe"){
    // Generic black-box observation of a protected getter (no arguments):
    // OR2_BRIDGE=<hex address>. Bisects the writable data range for the bytes
    // that change the returned EAX, then prints the value mapping.
    const char* at=std::getenv("OR2_BRIDGE");if(!at)throw std::runtime_error("set OR2_BRIDGE");
    const std::uint32_t entry=std::uint32_t(std::strtoul(at,nullptr,16));
    map_original_arithmetic_bridge(file);
    for(std::uint32_t page=0x62f000u;page<0x98d000u;page+=0x1000u)mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE);
    auto run=[&]{prepare(entry);run_original32();return guest_call.out_eax;};
    std::vector<std::uint8_t> saved(0x98d000u-0x62f000u);std::memcpy(saved.data(),reinterpret_cast<void*>(0x62f000u),saved.size());
    const auto base_value=run();std::cerr<<"baseline "<<std::hex<<base_value<<std::dec<<char(10);
    for(std::uint8_t fill:{0x01u,0x55u,0xffu}){
        std::uint32_t lo=0x62f000u,hi=0x98d000u;bool found=true;
        std::memcpy(reinterpret_cast<void*>(0x62f000u),saved.data(),saved.size());std::memset(reinterpret_cast<void*>(lo),fill,hi-lo);
        const auto all=run();if(all==base_value){std::cerr<<"fill "<<unsigned(fill)<<": no change"<<char(10);continue;}
        for(unsigned depth=0;depth<24&&hi-lo>1;++depth){
            const auto mid=lo+(hi-lo)/2;
            std::memcpy(reinterpret_cast<void*>(0x62f000u),saved.data(),saved.size());
            std::memset(reinterpret_cast<void*>(lo),fill,mid-lo);
            if(run()!=base_value)hi=mid;else{
                std::memcpy(reinterpret_cast<void*>(0x62f000u),saved.data(),saved.size());
                std::memset(reinterpret_cast<void*>(mid),fill,hi-mid);
                if(run()!=base_value)lo=mid;else{std::cerr<<"no single region between "<<std::hex<<lo<<"-"<<hi<<std::dec<<char(10);found=false;break;}
            }
        }
        if(found)std::cerr<<"fill "<<unsigned(fill)<<": sensitive byte "<<std::hex<<lo<<std::dec<<char(10);
        std::memcpy(reinterpret_cast<void*>(0x62f000u),saved.data(),saved.size());
        if(found)for(std::uint32_t v:{0u,1u,2u,0x7fu,0x100u,0x12345678u,0xffffffffu}){*reinterpret_cast<std::uint32_t*>(lo&~3u)=v;
            std::cerr<<"  dword "<<std::hex<<(lo&~3u)<<"="<<v<<" -> "<<run()<<std::dec<<char(10);}
        std::memcpy(reinterpret_cast<void*>(0x62f000u),saved.data(),saved.size());
        if(found)break;
    }
    std::cout<<"{\"routine\":\"bridge_getter_probe\",\"total_cases\":1,\"total_mismatching_cases\":0}"<<'\n';return 0;
}

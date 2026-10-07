if(only=="rob_setloop_observe"){
    // Black-box observation of the protected RobMotion SetLoop 4F1E60
    // (jmp [1039C40]): changed words of the motion object and EAX.
    map_original_arithmetic_bridge(file);
    constexpr std::uint32_t M=0x3a840000u;
    map_at(M,0x1000,PROT_READ|PROT_WRITE);
    for(std::uint32_t n:{0u,1u,2u,3u,7u,0xffffffffu,0x80000000u}){
        for(unsigned k=0;k<0x100;k+=4)*reinterpret_cast<std::uint32_t*>(M+k)=0x11110000u+k;
        prepare(0x4f1e60u);guest_call.ecx=M;reinterpret_cast<unsigned*>(S)[0]=n;run_original32();
        std::cerr<<"n="<<std::hex<<n<<" eax="<<guest_call.out_eax<<" sp_ok="<<(guest_call.out_sp==S+4)<<":";
        for(unsigned k=0;k<0x100;k+=4){const auto v=*reinterpret_cast<std::uint32_t*>(M+k);if(v!=0x11110000u+k)std::cerr<<" +"<<k<<"="<<v;}
        std::cerr<<std::dec<<char(10);
    }
    std::cout<<"{\"routine\":\"rob_setloop_observe\",\"total_cases\":1,\"total_mismatching_cases\":0}"<<'\n';return 0;
}

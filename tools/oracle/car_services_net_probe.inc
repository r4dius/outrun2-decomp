// Included inside the compare_pc dispatcher; uses its private x86 harness.
// car_services_net_probe: original 457770/457480, 4FB870, 4FC390, 4A4440,
// 4506B0, 449B30, 449C10 and 451180 against src/driving/pc_car_services_net.
// GetCsRoadInfoByCsLen 43E3B0 is replaced (call sites 457491 and 4A4455 only)
// by a capture stub that records (place, hint) and returns a scripted
// 0x64-byte road info + success flag; the native side receives the same
// scripted result. car_services_net_e2e_probe keeps the original 43E3B0 and
// runs it against the native pc_get_cs_road_info_by_cs_len on the world
// fixture (see below).
if(only=="car_services_net_probe"){
    using namespace outrun::driving;
    constexpr unsigned base=0x3ab00000,A=base+0x1000,E=base+0x4000,Wk=base+0x6000,P=base+0x8000,M=base+0x10000;
    map_at(base,0x20000,PROT_READ|PROT_WRITE|PROT_EXEC);
    for(unsigned page=0x7de000u;page<0x7e6000u;page+=0x1000u)
        if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("car net table");
    for(unsigned page:{0x656000u,0x780000u,0x799000u,0x7d3000u,0x7dd000u})
        if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("car net globals");
    {   // 43E3B0 capture stub (cdecl out,place,hint).
        std::vector<std::uint8_t> c;auto imm=[&](unsigned v){for(unsigned j=0;j<4;++j)c.push_back(std::uint8_t(v>>(8*j)));};
        c.push_back(0xa1);imm(A);c.insert(c.end(),{0x89,0xc1,0xff,0x05});imm(A);c.insert(c.end(),{0xc1,0xe1,0x03});
        c.insert(c.end(),{0x8b,0x54,0x24,0x08,0x89,0x91});imm(A+0x10);
        c.insert(c.end(),{0x8b,0x54,0x24,0x0c,0x89,0x91});imm(A+0x14);
        c.insert(c.end(),{0xc1,0xe0,0x07,0x56,0x57,0x8d,0xb0});imm(A+0x104);
        c.insert(c.end(),{0x8b,0x7c,0x24,0x0c,0xb9,0x19,0,0,0,0xf3,0xa5,0x8b,0x80});imm(A+0x100);
        c.insert(c.end(),{0x5f,0x5e,0xc3});r060_raw_write(base,c.data(),c.size());
    }
    for(unsigned site:{0x457491u,0x4a4455u}){
        std::int32_t rel;std::memcpy(&rel,reinterpret_cast<void*>(site+1u),4);
        if(*reinterpret_cast<std::uint8_t*>(site)!=0xe8||site+5u+unsigned(rel)!=0x43e3b0u)throw std::runtime_error("car net 43E3B0 call site");
        r046_write_rel32(site,0xe8,base);
    }
    const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
    unsigned cases{},mismatches{};std::map<std::string,unsigned> per,bad;
    auto check=[&](bool ok,const char* what,unsigned i){++cases;++per[what];if(!ok){++mismatches;++bad[what];if(bad[what]<4)std::cerr<<"car_services_net "<<what<<" case "<<i<<"\n";}};
    auto g32=[](std::uint32_t a){return *reinterpret_cast<const std::uint32_t*>(a);};
    auto p32=[](std::uint32_t a,std::uint32_t v){*reinterpret_cast<std::uint32_t*>(a)=v;};
    auto bits=[](float f){std::uint32_t u;std::memcpy(&u,&f,4);return u;};
    auto fb=[](std::uint32_t u){float f;std::memcpy(&f,&u,4);return f;};
    std::mt19937 rng(0x4fb870u);
    auto uni=[&](float lo,float hi){return lo+(hi-lo)*float(rng()%1000001u)/1000000.f;};
    auto edge=[&](){static const std::uint32_t e[]={0u,0x80000000u,0x7fc00000u,0xffc00000u,0x7f800000u,0xff800000u,0x00000001u,0x7f7fffffu,0xff7fffffu,0x3f800000u,0xbf800000u,0x38d1b717u,0x5f000000u,0xdf000000u,0x4f800000u,0x3f733333u,0x3f733334u};
        return fb(e[rng()%(sizeof(e)/4)]);};
    auto rf=[&](float lo,float hi,unsigned i){return (i%23u==0u)?edge():(i%97u==1u?fb(rng()):uni(lo,hi));};
    auto native_cw=[&](){unsigned short cw=static_cast<unsigned short>(x87_control);__asm__ volatile("fldcw %0"::"m"(cw));};
    auto restore_cw=[&](){unsigned short cw=0x037f;__asm__ volatile("fldcw %0"::"m"(cw));};
    // Scripted road info k (0..3): found flag + 0x64 bytes.
    auto script=[&](unsigned k,unsigned i,std::array<std::uint8_t,0x64>& info,bool& found)->void{
        Bytes b(info.data(),info.size());for(unsigned o=0;o<0x64;o+=4)b.put32(o,rng());
        found=(i%29u)!=3u;
        const float cx=uni(-3000.f,3000.f),cy=uni(-200.f,200.f),cz=uni(-3000.f,3000.f),w=uni(2.f,40.f),l=uni(1.f,30.f),yaw=uni(-3.2f,3.2f);
        const float sx=std::cos(yaw),sz=std::sin(yaw);
        const float qx[4]={-w,w,w,-w},qz[4]={-l,-l,l,l};
        const unsigned off[4]={0x24,0x30,0x48,0x3c};
        for(unsigned v=0;v<4;++v){b.putf(off[v],cx+qx[v]*sx-qz[v]*sz+uni(-0.5f,0.5f));b.putf(off[v]+4,cy+uni(-2.f,2.f));b.putf(off[v]+8,cz+qx[v]*sz+qz[v]*sx+uni(-0.5f,0.5f));}
        b.putf(0x18,uni(-0.3f,0.3f));b.putf(0x1c,uni(0.8f,1.0f));b.putf(0x20,uni(-0.3f,0.3f));
        if(i%41u==5u)for(unsigned o=0x24;o<0x54;o+=4)b.putf(o,b.f32(0x24+(o-0x24)%12u)); // degenerate: all vertices equal
        if(i%37u==7u)b.putf(0x18+4*(rng()%3u),0.0f);
        if(i%31u==9u)b.putf(0x18+4*(rng()%15u),edge());
        if(i%53u==11u){b.putf(0x18,0);b.putf(0x1c,0);b.putf(0x20,0);}
        std::memcpy(reinterpret_cast<void*>(A+0x104+k*0x80u),info.data(),0x64);p32(A+0x100+k*0x80u,found?(i%7u==0u?0xabc0u:1u):0u);
    };
    auto fill=[&](unsigned addr,std::size_t n){for(std::size_t o=0;o<n;o+=4)p32(addr+unsigned(o),rng());};
    auto set_pos=[&](unsigned off,const std::array<std::uint8_t,0x64>& info,unsigned i){
        Bytes b(const_cast<std::uint8_t*>(info.data()),info.size());Bytes e(reinterpret_cast<void*>(E),0x1000);
        const float ccx=(b.f32(0x24)+b.f32(0x48))*0.5f,ccz=(b.f32(0x2c)+b.f32(0x50))*0.5f;
        e.putf(off,std::isfinite(ccx)?ccx+uni(-30.f,30.f):uni(-10.f,10.f));e.putf(off+4,uni(-100.f,100.f));
        e.putf(off+8,std::isfinite(ccz)?ccz+uni(-30.f,30.f):uni(-10.f,10.f));
        if(i%43u==13u)e.putf(off+4*(rng()%3u),edge());
    };
    const unsigned N=2000u; // argv[2] (the harness case count) is not used by this probe
    // ---- 457480 (ST0 raw) and 457770 (table store) ----
    for(unsigned i=0;i<N*4u;++i){
        fill(E,0x1000);std::array<std::uint8_t,0x64> info{};bool found{};script(0,i,info,found);set_pos(0x14,info,i);
        Bytes e(reinterpret_cast<void*>(E),0x1000);if(i%11u==0u)e.put16(0x260,std::uint16_t(i%3u?0xffffu:0u));
        std::vector<std::uint8_t> ev(reinterpret_cast<std::uint8_t*>(E),reinterpret_cast<std::uint8_t*>(E)+0x1000);
        p32(A,0);
        if(i&1u){
            prepare(0x457480);guest_call.edi=E;guest_call.st0=1;run();
            native_cw();const long double nat=pc_route_progress_457480(Bytes(ev.data(),ev.size()),PcRoadInfoResult{found,Bytes(info.data(),info.size())});restore_cw();
            std::uint8_t nb[16]{};std::memcpy(nb,&nat,10);if(inject&&i==N*4u-1u)nb[0]^=1;
            check(std::memcmp(nb,guest_call.out_st0_raw,10)==0,"457480 st0",i);
        }else{
            const std::uint8_t slot=std::uint8_t(rng());*reinterpret_cast<std::uint8_t*>(0x7dd138u)=slot;p32(0x799d18u,E);
            fill(0x7de480u,0x6c00);std::vector<std::uint8_t> tab(reinterpret_cast<std::uint8_t*>(0x7de480u),reinterpret_cast<std::uint8_t*>(0x7de480u)+0x6c00);
            prepare(0x457770);run();
            native_cw();pc_route_state4_457770(Bytes(ev.data(),ev.size()),slot,Bytes(tab.data(),tab.size()),PcRoadInfoResult{found,Bytes(info.data(),info.size())});restore_cw();
            check(std::memcmp(tab.data(),reinterpret_cast<void*>(0x7de480u),tab.size())==0,"457770 table",i);
        }
        check(std::memcmp(ev.data(),reinterpret_cast<void*>(E),0x1000)==0&&g32(A)==1u&&g32(A+0x10)==E+0x5cu&&g32(A+0x14)==0xffffffffu,"457480 event/call",i);
    }
    // ---- 4FB870 forced cruise ----
    for(unsigned i=0;i<N*5u;++i){
        if(i%64u==0u){fill(M,0x7678);fill(E,0x1000);fill(Wk,0x800);fill(P,0x2000);}
        Bytes m(reinterpret_cast<void*>(M),0x7678),e(reinterpret_cast<void*>(E),0x1000),w(reinterpret_cast<void*>(Wk),0x800),p(reinterpret_cast<void*>(P),0x2000);
        m.putf(0x184,i%13u==0u?edge():uni(0.0f,0.05f));m.put32(0x7668,(i&1u)?rng()|1u:0u);
        m.puti(0x766c,int(rng()%7u)-2);m.putf(0x7670,rf(-0.5f,2.0f,i));m.putf(0x7674,rf(-0.5f,2.0f,i+5u));
        if(i%9u==4u)m.putf(0x7670,0.0f);if(i%9u==5u)m.put32(0x7674,m.u32(0x7670));
        e.putf(0x1c4,rf(-10.f,120.f,i+1u));e.putf(0x178,rf(-10.f,120.f,i+2u));e.put32(0x1f4,(i%3u)?0u:rng()%3u+1u);
        for(unsigned k=0;k<3;++k)e.putf(0x20+4*k,rf(-80.f,80.f,i+3u+k));
        if(i%17u==6u){e.putf(0x20,0);e.putf(0x24,0);e.putf(0x28,0);}
        if(i%19u==8u){e.putf(0x20,1e-5f);e.putf(0x24,0);e.putf(0x28,-1e-5f);}
        e.put16(0x2e,std::uint16_t(rng()));
        p.putf(0xb48,rf(0.2f,3.f,i+6u));p.putf(0xb94,rf(0.2f,3.f,i+7u));p.put32(0x10a0,rng());p.putf(0x1644,rf(-2000.f,9000.f,i+8u));
        if(i%29u==2u)p.putf(0x1644,fb(0x4f000000u^(rng()&0x80ffffffu)));
        std::vector<std::uint8_t> nm(reinterpret_cast<std::uint8_t*>(M),reinterpret_cast<std::uint8_t*>(M)+0x7678),ne(reinterpret_cast<std::uint8_t*>(E),reinterpret_cast<std::uint8_t*>(E)+0x1000),
            nw(reinterpret_cast<std::uint8_t*>(Wk),reinterpret_cast<std::uint8_t*>(Wk)+0x800),np(reinterpret_cast<std::uint8_t*>(P),reinterpret_cast<std::uint8_t*>(P)+0x2000);
        p32(E+0x2b4,P);ne[0x2b4]=std::uint8_t(P);ne[0x2b5]=std::uint8_t(P>>8);ne[0x2b6]=std::uint8_t(P>>16);ne[0x2b7]=std::uint8_t(P>>24);
        {const float z=0.f,a=m.f32(0x7670),b=m.f32(0x7674);
         if(z>=a)++per["cov 4FB870 early-exit"];else{if(b>a)++per["cov 4FB870 ramp-up"];else if(a>z)++per["cov 4FB870 ramp-down"];else ++per["cov 4FB870 hold"];
         if(e.u32(0x1f4))++per["cov 4FB870 vector"];else ++per["cov 4FB870 angle"];}}
        prepare(0x4fb870);guest_call.ecx=M;reinterpret_cast<unsigned*>(S)[0]=E;reinterpret_cast<unsigned*>(S)[1]=Wk;run_original32();
        native_cw();pc_manager_forced_cruise_4fb870(Bytes(nm.data(),nm.size()),Bytes(ne.data(),ne.size()),Bytes(nw.data(),nw.size()),Bytes(np.data(),np.size()));restore_cw();
        if(inject&&i==N*5u-1u)nw[0x32c]^=1;
        check(guest_call.out_sp==S+8u,"4FB870 ret 8",i);
        check(std::memcmp(nm.data(),reinterpret_cast<void*>(M),nm.size())==0,"4FB870 manager",i);
        check(std::memcmp(ne.data(),reinterpret_cast<void*>(E),ne.size())==0,"4FB870 event",i);
        check(std::memcmp(nw.data(),reinterpret_cast<void*>(Wk),nw.size())==0,"4FB870 work",i);
        check(std::memcmp(np.data(),reinterpret_cast<void*>(P),np.size())==0,"4FB870 params",i);
    }
    // ---- 4FC390 road mu ----
    {
        std::vector<std::uint32_t> flags{0,1,2,3,4,5,6,7,8,9,0xf,0x10,0x11,0x20,0xff,0x100,0x101,0x3ff,0x400,0x401,0x800,0x7fff,0x8000,0x8001,0xfffff,0x100000,0x100001,
            0x1fffff,0x200000,0x200001,0x3fffff,0x400000,0x400001,0x7fffff,0x800000,0x800001,0x1000000,0x80000000u,0xffffffffu};
        for(unsigned i=0;i<N*4u;++i){
            const std::uint32_t f=(i%5u==4u)?(1u<<(rng()%32u)):(i%7u==6u?rng():flags[i%flags.size()]);
            float mu=(i%3u==0u)?edge():uni(0.0f,1.5f);Bytes(reinterpret_cast<void*>(M),0x7678).putf(0x764c,mu);
            const std::uint32_t road=rng();
            prepare(0x4fc390);guest_call.ecx=M;guest_call.st0=1;reinterpret_cast<unsigned*>(S)[0]=road;reinterpret_cast<unsigned*>(S)[1]=f;run_original32();
            native_cw();const long double nat=pc_manager_road_mu_4fc390(mu,road,f);const long double via=pc_manager_road_mu_lookup(&mu,road,f);restore_cw();
            std::uint8_t nb[16]{},vb[16]{};std::memcpy(nb,&nat,10);std::memcpy(vb,&via,10);if(inject&&i==7u)nb[1]^=1;
            check(guest_call.out_sp==S+8u&&std::memcmp(nb,guest_call.out_st0_raw,10)==0&&std::memcmp(vb,nb,10)==0,"4FC390 st0",i);
        }
    }
    // ---- 4A4440 road side offsets (direct) ----
    for(unsigned i=0;i<N*3u;++i){
        fill(E,0x1000);std::array<std::uint8_t,0x64> info{};bool found{};script(0,i,info,found);set_pos(0x14,info,i);
        const std::int32_t hint=int(rng()%400u)-2;p32(A,0);p32(E+0x800,rng());p32(E+0x804,rng());
        prepare(0x4a4440);auto* s=reinterpret_cast<unsigned*>(S);s[0]=unsigned(hint);s[1]=E+0x5c;s[2]=E+0x14;s[3]=E+0x800;s[4]=E+0x804;run();
        native_cw();const auto r=pc_road_side_offsets_4a4440(PcRoadInfoResult{found,Bytes(info.data(),info.size())},
            {Bytes(reinterpret_cast<void*>(E),0x1000).f32(0x14),Bytes(reinterpret_cast<void*>(E),0x1000).f32(0x18),Bytes(reinterpret_cast<void*>(E),0x1000).f32(0x1c)});restore_cw();
        check(g32(E+0x800)==bits(r.a3)&&g32(E+0x804)==bits(r.a4)&&g32(A)==1u&&g32(A+0x10)==E+0x5cu&&g32(A+0x14)==unsigned(hint),"4A4440 outputs",i);
    }
    // ---- 4506B0 lane ratio (XMM0) ----
    for(unsigned i=0;i<N*3u;++i){
        fill(E,0x1000);std::array<std::uint8_t,0x64> i0{},i1{};bool f0{},f1{};script(0,i,i0,f0);script(1,i*7u+3u,i1,f1);
        set_pos(0x14,i0,i);set_pos(0x16c,(i%5u==0u)?i0:i1,i+1u);
        Bytes e(reinterpret_cast<void*>(E),0x1000);const std::int32_t hint=int(rng()%400u)-2;e.puti(0x1c0,hint);
        std::vector<std::uint8_t> ev(reinterpret_cast<std::uint8_t*>(E),reinterpret_cast<std::uint8_t*>(E)+0x1000);p32(A,0);
        prepare(0x4506b0);guest_call.esi=E;run();
        native_cw();const float r=pc_lane_ratio_4506b0(Bytes(ev.data(),ev.size()),PcRoadInfoResult{f0,Bytes(i0.data(),i0.size())},PcRoadInfoResult{f1,Bytes(i1.data(),i1.size())});restore_cw();
        check(guest_call.out_xmm0==bits(r),"4506B0 xmm0",i);
        check(g32(A)==2u&&g32(A+0x10)==E+0x5cu&&g32(A+0x14)==unsigned(hint)&&g32(A+0x18)==E+0x184u&&g32(A+0x1c)==unsigned(hint)&&
              std::memcmp(ev.data(),reinterpret_cast<void*>(E),0x1000)==0,"4506B0 calls/event",i);
    }
    // ---- 449B30 general / 449C10 ----
    for(unsigned i=0;i<N*5u;++i){
        std::uint32_t frames=(i%4u==0u)?rng():(i%4u==1u?rng()%400000u:(i%4u==2u?0xffffffffu-rng()%1000u:rng()%1000u));
        float extra=(i%6u==0u)?edge():(i%6u==1u?0.0f:uni(-2.5f,2.5f));
        if(i%50u==3u){frames=60u*60u*1000u;extra=-0.0f;}
        prepare(0x449b30);auto* s=reinterpret_cast<unsigned*>(S);const unsigned o=E+0x900;
        for(unsigned k=0;k<4;++k){s[k]=o+4*k;p32(o+4*k,0xdddd0000u|k);}s[4]=frames;s[5]=bits(extra);run();
        native_cw();const auto t=pc_time_split_449b30(frames,extra);restore_cw();
        bool ok=true;for(unsigned k=0;k<4;++k)ok=ok&&g32(o+4*k)==(0xdddd0000u|t[k]);
        check(ok,"449B30 split",i);
        const std::uint16_t h=std::uint16_t(rng()),mi=std::uint16_t(rng()),se=std::uint16_t(rng()),ms=std::uint16_t(rng());
        prepare(0x449c10);s[0]=h|0xabcd0000u;s[1]=mi|0x12340000u;s[2]=se;s[3]=ms|0xffff0000u;run();
        check(guest_call.out_eax==pc_time_join_449c10(h,mi,se,ms),"449C10 join",i);
    }
    // ---- 451180 allocate_result ----
    for(unsigned i=0;i<N*4u;++i){
        fill(E,0x1000);std::array<std::uint8_t,0x64> i0{},i1{};bool f0{},f1{};script(0,i,i0,f0);script(1,i*5u+1u,i1,f1);
        set_pos(0x14,i0,i);set_pos(0x16c,i1,i+2u);Bytes e(reinterpret_cast<void*>(E),0x1000);e.puti(0x1c0,int(rng()%300u)-1);
        PcAllocateResultState st{};
        static const std::int32_t counters[]={-2147483647-1,-1,0,1,58,59,60,61,1000,2147483647};
        st.counter_656234=(i%3u==2u)?counters[rng()%10u]:int(rng()%60u);
        st.frames_7d3948=(i%4u==0u)?rng():rng()%500000u;st.mode_78024c=rng()%6u;
        st.result_7d3738=(i%5u==0u)?0xffffffffu:rng();st.result_7d3698=(i%7u==0u)?0xffffffffu:rng();
        const std::uint32_t request=(i%4u==3u)?0u:(i%8u==1u?rng():1u);
        p32(0x656234u,unsigned(st.counter_656234));p32(0x799d18u,E);p32(0x7d3948u,st.frames_7d3948);p32(0x78024cu,st.mode_78024c);
        p32(0x7d3738u,st.result_7d3738);p32(0x7d3698u,st.result_7d3698);
        std::vector<std::uint8_t> ev(reinterpret_cast<std::uint8_t*>(E),reinterpret_cast<std::uint8_t*>(E)+0x1000);p32(A,0);
        prepare(0x451180);reinterpret_cast<unsigned*>(S)[0]=request;run();
        native_cw();auto r=pc_allocate_result_451180(request,st,Bytes(ev.data(),ev.size()),PcRoadInfoResult{f0,Bytes(i0.data(),i0.size())},PcRoadInfoResult{f1,Bytes(i1.data(),i1.size())});restore_cw();
        if(inject&&i==N*4u-1u)r^=1u;
        const bool live=request!=0u&&st.counter_656234<0x3c;
        check(guest_call.out_eax==r&&g32(A)==(live?2u:0u)&&std::memcmp(ev.data(),reinterpret_cast<void*>(E),0x1000)==0,"451180 result",i);
    }
    for(unsigned site:{0x457491u,0x4a4455u})r046_write_rel32(site,0xe8,0x43e3b0u);
    for(const auto& [k,v]:per){if(k.rfind("cov ",0)==0)std::cerr<<"  coverage "<<k.substr(4)<<": "<<v<<" inputs\n";else std::cerr<<"  "<<k<<": "<<v<<" cases, "<<bad[k]<<" mismatching\n";}
    std::cout<<"{\"routine\":\"car_services_net_probe\",\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}"<<'\n';return mismatches?1:0;
}
// car_services_net_e2e_probe: original 457770/457480, 4A4440, 4506B0 and
// 451180 with the ORIGINAL GetCsRoadInfoByCsLen 43E3B0 (no stub) on the r016
// world fixture (id 11 = the get_cs_road_info_by_cs_len configuration),
// against the native overloads taking PcRoadInfoContext, i.e. the verified
// native pc_get_cs_road_info_by_cs_len end-to-end. Besides the results, the
// complete world image (course tables, matrix stack arena including the
// 43E3B0 push/pop scratch), world globals and transforms are compared.
if(only=="car_services_net_e2e_probe"){
    using namespace outrun::driving;
    constexpr unsigned base=0x3ab00000,E=base+0x4000;
    map_at(base,0x20000,PROT_READ|PROT_WRITE|PROT_EXEC);
    init_spline_oracle();init_world_oracle(); // bridge + d3dx are mapped in main() for this probe name
    for(unsigned page=0x7de000u;page<0x7e6000u;page+=0x1000u)
        if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("car net table");
    for(unsigned page:{0x656000u,0x780000u,0x799000u,0x7d3000u,0x7dd000u})
        if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("car net globals");
    for(unsigned site:{0x457491u,0x4a4455u}){
        std::int32_t rel;std::memcpy(&rel,reinterpret_cast<void*>(site+1u),4);
        if(*reinterpret_cast<std::uint8_t*>(site)!=0xe8||site+5u+unsigned(rel)!=0x43e3b0u)throw std::runtime_error("car net e2e: 43E3B0 call site is patched");
    }
    const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
    unsigned cases{},mismatches{};std::map<std::string,unsigned> per,bad;
    auto check=[&](bool ok,const char* what,unsigned i){++cases;++per[what];if(!ok){++mismatches;++bad[what];if(bad[what]<4)std::cerr<<"car_services_net_e2e "<<what<<" case "<<i<<"\n";}};
    auto g32=[](std::uint32_t a){return *reinterpret_cast<const std::uint32_t*>(a);};
    auto p32=[](std::uint32_t a,std::uint32_t v){*reinterpret_cast<std::uint32_t*>(a)=v;};
    auto bits=[](float f){std::uint32_t u;std::memcpy(&u,&f,4);return u;};
    std::mt19937 rng(0x43e3b0u);
    auto uni=[&](float lo,float hi){return lo+(hi-lo)*float(rng()%1000001u)/1000000.f;};
    auto native_cw=[&](){unsigned short cw=static_cast<unsigned short>(x87_control);__asm__ volatile("fldcw %0"::"m"(cw));};
    auto restore_cw=[&](){unsigned short cw=0x037f;__asm__ volatile("fldcw %0"::"m"(cw));};
    const unsigned N=3000u;
    unsigned found_cases{},rejected{};
    for(unsigned i=0;i<N;++i){
        auto f=outrun::testing::make_course_world_fixture(i,11u);auto fb=f.bytes();
        const unsigned mode=i%5u;
        PcAllocateResultState st{};st.counter_656234=(i%10u==9u)?60:int(rng()%60u);st.frames_7d3948=rng()%500000u;
        st.mode_78024c=rng()%5u;st.result_7d3738=rng();st.result_7d3698=rng();const std::uint32_t request=(i%15u==14u)?0u:1u;
        p32(0x656234u,unsigned(st.counter_656234));p32(0x799d18u,E);p32(0x7d3948u,st.frames_7d3948);p32(0x78024cu,st.mode_78024c);
        p32(0x7d3738u,st.result_7d3738);p32(0x7d3698u,st.result_7d3698);
        const std::uint8_t slot=std::uint8_t(rng());*reinterpret_cast<std::uint8_t*>(0x7dd138u)=slot;
        for(unsigned o=0;o<0x6c00;o+=4)p32(0x7de480u+o,rng());
        WorldBinding bind(f);auto native_f=f;
        for(unsigned o=0;o<0x1000;o+=4)p32(E+o,rng());
        Bytes e(reinterpret_cast<void*>(E),0x1000);
        for(unsigned k=0;k<16;++k){e.put8(0x5c+k,fb.u8(0x4560+k));e.put8(0x184+k,fb.u8(0x4560+k));}
        {   // second place: a neighbouring course length from the same table
            const auto type=fb.u32(0x4560);auto q=fb.sub(type*4096u,4096);const auto idx=q.i32(0xf10);
            if(i%3u==1u&&idx>0)e.put16(0x184+8,std::uint16_t(q.i16(0x200+std::size_t(idx-1)*2u)));
        }
        e.puti(0x1c0,(i%4u==3u)?-1:fb.i32(0x4670));
        {   // Position near the road envelope (queried on a throw-away fixture copy).
            auto probe_f=f;auto tables=probe_f.tables();auto stack=probe_f.matrix();PcRoadInfoContext rc{tables,stack,{fb.f32(0x4638),fb.f32(0x463c)}};
            std::array<std::uint8_t,0x64> info{};native_cw();bool ok=false;
            try{ok=pc_get_cs_road_info_by_cs_len(Bytes(info.data(),info.size()),e.sub(0x5c,12),-1,rc);}catch(const std::exception& ex){restore_cw();std::cerr<<"prequery i="<<i<<" "<<ex.what()<<" type="<<fb.u32(0x4560)<<" len="<<fb.i16(0x4568)<<'\n';throw;}restore_cw();
            Bytes ib(info.data(),info.size());const float cx=ok?ib.f32(0x08):0.f,cy=ok?ib.f32(0x0c):0.f,cz=ok?ib.f32(0x10):0.f;found_cases+=ok;
            e.putf(0x14,cx+uni(-20.f,20.f));e.putf(0x18,cy+uni(-5.f,5.f));e.putf(0x1c,cz+uni(-20.f,20.f));
            e.putf(0x16c,cx+uni(-20.f,20.f));e.putf(0x170,cy+uni(-5.f,5.f));e.putf(0x174,cz+uni(-20.f,20.f));
        }
        std::vector<std::uint8_t> ev(reinterpret_cast<std::uint8_t*>(E),reinterpret_cast<std::uint8_t*>(E)+0x1000);
        std::vector<std::uint8_t> tab(reinterpret_cast<std::uint8_t*>(0x7de480u),reinterpret_cast<std::uint8_t*>(0x7de480u)+0x6c00);
        auto tables=native_f.tables();auto stack=native_f.matrix();PcRoadInfoContext road{tables,stack,{fb.f32(0x4638),fb.f32(0x463c)}};
        const Bytes nev(ev.data(),ev.size());
        try{if(mode==0u){
            prepare(0x457480);guest_call.edi=E;guest_call.st0=1;run();
            native_cw();const long double nat=pc_route_progress_457480(nev,road);restore_cw();
            std::uint8_t nb[16]{};std::memcpy(nb,&nat,10);if(inject&&i==N-5u)nb[0]^=1;
            check(std::memcmp(nb,guest_call.out_st0_raw,10)==0,"e2e 457480 st0",i);
        }else if(mode==1u){
            prepare(0x457770);run();
            native_cw();pc_route_state4_457770(nev,slot,Bytes(tab.data(),tab.size()),road);restore_cw();
            check(std::memcmp(tab.data(),reinterpret_cast<void*>(0x7de480u),tab.size())==0,"e2e 457770 table",i);
        }else if(mode==2u){
            prepare(0x4506b0);guest_call.esi=E;run();
            native_cw();const float r=pc_lane_ratio_4506b0(nev,road);restore_cw();
            check(guest_call.out_xmm0==bits(r),"e2e 4506B0 xmm0",i);
        }else if(mode==3u){
            prepare(0x451180);reinterpret_cast<unsigned*>(S)[0]=request;run();
            native_cw();auto r=pc_allocate_result_451180(request,st,nev,road);restore_cw();if(inject&&i==N-2u)r^=1u;
            check(guest_call.out_eax==r,"e2e 451180 eax",i);
        }else{
            const std::int32_t hint=e.i32(0x1c0);
            prepare(0x4a4440);auto* s=reinterpret_cast<unsigned*>(S);s[0]=unsigned(hint);s[1]=E+0x184;s[2]=E+0x16c;s[3]=E+0x800;s[4]=E+0x804;run();
            native_cw();std::array<std::uint8_t,0x64> info{};const bool ok=pc_get_cs_road_info_by_cs_len(Bytes(info.data(),info.size()),nev.sub(0x184,12),hint,road);
            const auto r=pc_road_side_offsets_4a4440(PcRoadInfoResult{ok,Bytes(info.data(),info.size())},{nev.f32(0x16c),nev.f32(0x170),nev.f32(0x174)});restore_cw();
            nev.putf(0x800,r.a3);nev.putf(0x804,r.a4);
            check(g32(E+0x800)==bits(r.a3)&&g32(E+0x804)==bits(r.a4),"e2e 4A4440 outputs",i);
        }}catch(const std::domain_error& ex){restore_cw();++rejected;if(rejected<6)std::cerr<<"native domain rejection i="<<i<<" mode="<<mode<<" "<<ex.what()<<" guest_eax="<<guest_call.out_eax<<std::endl;
            continue;}
        check(std::memcmp(ev.data(),reinterpret_cast<void*>(E),0x1000)==0,"e2e event (only 4A4440 outputs written)",i);
        native_f.save(stack,native_f.prediction());bind.capture_globals();
        std::uint64_t before{};for(const auto& [k,v]:stats)if(k.rfind("car_net_e2e",0)==0)before+=v.fail;
        bind.compare("car_net_e2e",native_f);
        std::uint64_t after{};for(const auto& [k,v]:stats)if(k.rfind("car_net_e2e",0)==0)after+=v.fail;
        check(after==before,"e2e world image/globals/matrix",i);
    }
    for(const auto& [k,v]:per)std::cerr<<"  "<<k<<": "<<v<<" cases, "<<bad[k]<<" mismatching\n";
    std::cerr<<"  road info found (native pre-query, hint -1): "<<found_cases<<"/"<<N<<"; native domain rejections (excluded; fixture matrix stack at capacity: the first 43E3B0 push overflows, its pop drifts the pointer onto unset arena bytes, the second query then sees a non-finite homogeneous w): "<<rejected<<"\n";
    std::cout<<"{\"routine\":\"car_services_net_e2e_probe\",\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}"<<'\n';return mismatches?1:0;
}

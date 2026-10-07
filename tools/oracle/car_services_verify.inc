// Included inside the compare_pc dispatcher (via car_services_probe.inc).
if(only=="car_services_probe"){
    // Whole-function oracle for the car-control leaf services: original x86
    // (with the protected VM bridges mapped) against src/driving/pc_car_services.
    // OR2_ROUTINE=<name> restricts to one family.
    const char* routine_env=std::getenv("OR2_ROUTINE");const std::string routine=routine_env?routine_env:"all";
    auto want=[&](const char* n){return routine=="all"||routine==n;};
    const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
    constexpr unsigned base=0x3ae00000,arena=base+0x10000,trace=base+0x8000,values=base+0x7000,obj=base+0x20000;
    map_at(base,0x40000,PROT_READ|PROT_WRITE|PROT_EXEC);if(!protected_bridge_mapped)map_original_arithmetic_bridge(file);
    for(std::uint32_t page=0x62f000u;page<0x98d000u;page+=0x1000u)mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE);
    auto restore=[&](std::uint32_t va,std::size_t n){std::vector<std::uint8_t> b(n);for(std::size_t i=0;i<n;++i)b[i]=file[pe_file_offset(file,va+std::uint32_t(i))];r060_raw_write(va,b.data(),n);};
    auto g32=[](std::uint32_t a){return *reinterpret_cast<const std::uint32_t*>(a);};
    auto p32=[](std::uint32_t a,std::uint32_t v){*reinterpret_cast<std::uint32_t*>(a)=v;};
    auto p16=[](std::uint32_t a,std::uint16_t v){*reinterpret_cast<std::uint16_t*>(a)=v;};
    auto p8=[](std::uint32_t a,std::uint8_t v){*reinterpret_cast<std::uint8_t*>(a)=v;};
    auto fb=[](float f){std::uint32_t u;std::memcpy(&u,&f,4);return u;};
    std::map<std::string,std::pair<unsigned,unsigned>> tally;unsigned cases{},mismatches{};
    auto check=[&](bool ok,const std::string& what,unsigned i){++cases;auto& t=tally[what];++t.first;if(!ok){++t.second;++mismatches;if(mismatches<20)std::cerr<<what<<" case "<<i<<"\n";}};
    std::mt19937 rng(0x4a8330u);
    auto rnd=[&](std::uint32_t n){return n?std::uint32_t(rng()%n):0u;};
    auto stack=[&]{return reinterpret_cast<std::uint32_t*>(S);};
    const unsigned N=count>1?count:4000u;
    (void)trace;(void)values;(void)p16;

    // ---- 0x43D130 lane rates --------------------------------------------
    if(want("43d130")){
        restore(0x43d130u,0xa0);
        for(unsigned i=0;i<N;++i){
            std::array<std::vector<std::uint8_t>,4> tabs;
            for(unsigned t=0;t<4;++t){tabs[t].resize(64);for(auto& b:tabs[t])b=std::uint8_t(rng());
                const bool present=rnd(5)!=0;std::memcpy(reinterpret_cast<void*>(arena+t*0x100),tabs[t].data(),64);p32(0x780160u+t*4,present?arena+t*0x100:0u);}
            const std::uint32_t type=rnd(4);const std::int32_t sel=rnd(9)==0?-1:std::int32_t(rnd(16));
            for(unsigned k=0;k<4;++k)p32(obj+k*4,0xdeadbeefu);
            prepare(0x43d130u);auto* st=stack();st[0]=std::uint32_t(sel);st[1]=type;for(unsigned k=0;k<4;++k)st[2+k]=obj+k*4;run();
            Bytes tv(tabs[type].data(),tabs[type].size());const bool present=g32(0x780160u+type*4)!=0u;
            auto r=pc_lane_rates_43d130(sel,present?&tv:nullptr);if(inject&&i==N-1)r[2]=-r[2];
            bool ok=true;for(unsigned k=0;k<4;++k)ok=ok&&g32(obj+k*4)==fb(r[k]);check(ok,"43D130 lane rates",i);
        }
    }
    // ---- 0x4963E0 race ready ----------------------------------------------
    if(want("4963e0")){
        for(unsigned i=0;i<N;++i){
            const std::uint32_t route=rnd(4)==0?std::uint32_t(rng()):rnd(8);const bool present=rnd(4)!=0;const std::uint32_t f10=rnd(3)==0?0u:std::uint32_t(rng());
            p32(0x780258u,route);p32(0x83637cu,present?obj:0u);p32(obj+0x10,f10);
            prepare(0x4963e0u);run();auto r=pc_race_ready_4963e0(route,present,f10)?1u:0u;if(inject&&i==N-1)r^=1u;
            check(guest_call.out_eax==r,"4963E0 race ready",i);
        }
    }
    // ---- 0x456BB0 reference course position (+0x4F53B0) -------------------
    if(want("456bb0")){
        for(unsigned i=0;i<N;++i){
            const std::uint32_t n=rnd(9);const std::uint32_t clock=rng();const std::uint8_t versus=std::uint8_t(rnd(3)==0?0u:std::uint32_t(rng()));
            p32(0x7de418u,n);p32(0x7f1938u,clock);p8(0x850b04u,versus);
            std::vector<std::array<std::uint8_t,16>> rec(n);std::vector<Bytes> views;
            for(std::uint32_t s=0;s<n;++s){
                const std::uint32_t sa=0x7de41cu+s*0x6cu;for(unsigned k=0;k<0x6c;k+=4)p32(sa+k,rng());
                const std::uint16_t key=std::uint16_t(rnd(6));const std::uint32_t kind=rnd(3)?3u+rnd(2):rnd(8);
                p32(sa+0,obj+s*0x20);p32(sa+4,rnd(5)==0?std::uint32_t(rng()):rnd(6));
                const std::uint32_t age=rnd(3)==0?std::uint32_t(rng()):rnd(0x120);p32(sa+0x54,clock-age);
                for(auto& b:rec[s])b=std::uint8_t(rng());rec[s][0]=std::uint8_t(key);rec[s][1]=0;std::memcpy(rec[s].data()+8,&kind,4);
                std::memcpy(reinterpret_cast<void*>(obj+s*0x20),rec[s].data(),16);
            }
            for(std::uint32_t s=0;s<n;++s)views.emplace_back(rec[s].data(),rec[s].size());
            std::vector<std::uint8_t> mgr(4+n*0x6c);std::memcpy(mgr.data(),reinterpret_cast<void*>(0x7de418u),mgr.size());
            prepare(0x456bb0u);guest_call.ecx=0x7de418u;run();
            auto r=pc_reference_course_position_456bb0(Bytes(mgr.data(),mgr.size()),views.data(),views.size(),clock,versus);if(inject&&i==N-1)r^=1u;
            check((guest_call.out_eax&0xffffu)==r,"456BB0 reference position",i);
        }
        for(unsigned i=0;i<256;++i){p8(0x850b04u,std::uint8_t(i));prepare(0x4f53b0u);guest_call.eax=rng();run();
            auto r=pc_versus_flag_4f53b0(std::uint8_t(i))?1u:0u;if(inject&&i==255)r^=1u;check((guest_call.out_eax&0xffu)==r,"4F53B0 versus flag",i);}
    }
    // ---- 0x450130 protected gate ----------------------------------------------
    if(want("450130")){
        restore(0x450130u,0x10);
        for(unsigned i=0;i<N;++i){const std::uint32_t f=rng();p32(0x7d39f0u,f);prepare(0x450130u);guest_call.ecx=rng();run();
            auto r=pc_protected_gate_450130(f);if(inject&&i==N-1)r^=1u;check(guest_call.out_eax==r,"450130 protected gate",i);}
    }
    // ---- 0x4B5FD0 owner mode -------------------------------------------------
    if(want("4b5fd0")){
        restore(0x4b5fd0u,0x20);
        for(unsigned i=0;i<N;++i){const std::uint8_t fl=std::uint8_t(rng());const std::uint32_t m=rng();p8(0x79fb4eu,fl);p32(0x799ca0u,obj);p32(obj+0x1c,m);
            prepare(0x4b5fd0u);guest_call.eax=rng();run();auto r=std::uint32_t(pc_owner_mode_4b5fd0(fl,std::int32_t(m)));if(inject&&i==N-1)r^=1u;
            check(guest_call.out_eax==r,"4B5FD0 owner mode",i);}
    }
    // ---- 0x44F0F0 stage gate (protected tail) ----------------------------------
    if(want("44f0f0")){
        restore(0x44f0f0u,0x30);restore(0x44ddc0u,0x50);
        for(unsigned i=0;i<N;++i){
            static constexpr std::int32_t stages[]={0x1c,0x3a,0,0x44,-1};
            const std::int32_t stage=rnd(6)==5?std::int32_t(rng()):stages[rnd(5)];
            const std::uint16_t pos=std::uint16_t(rng()),rolling=std::uint16_t(rng());
            p32(0x6a55c8u,obj);p16(obj+0x7e,rolling);
            prepare(0x44f0f0u);stack()[0]=std::uint32_t(stage);stack()[1]=pos;run();
            auto r=pc_road_stage_gate_44f0f0(stage,pos,rolling);if(inject&&i==N-1)r^=1u;
            check(guest_call.out_eax==r,"44F0F0 stage gate",i);
        }
    }
    // ---- 0x45A2B0 CommRace_GetRank ---------------------------------------------
    if(want("45a2b0")){
        restore(0x45a2b0u,0x60);
        for(unsigned i=0;i<N;++i){
            for(unsigned k=0;k<0x200;k+=4)p32(0x7df118u+k,rng());
            p32(0x780258u,rnd(6));for(std::uint32_t a:{0x7df10cu,0x7df110u,0x7de418u})p32(a,rng());
            const std::uint32_t id=rnd(4)==0?rnd(0x200):rnd(16);
            prepare(0x45a2b0u);stack()[0]=id;guest_call.eax=rng();guest_call.ecx=rng();guest_call.edx=rng();guest_call.ebx=rng();guest_call.esi=rng();guest_call.edi=rng();run();
            auto r=pc_comm_race_get_rank_45a2b0(id,Bytes(reinterpret_cast<void*>(0x7df118u),0x200),*reinterpret_cast<std::uint32_t*>(0x78026cu),*reinterpret_cast<std::uint32_t*>(0x780258u));if(inject&&i==N-1)r^=1u;
            check(guest_call.out_eax==r,"45A2B0 CommRace_GetRank",i);
        }
    }
    // ---- 0x4A3F80 cache refresh with the measured 4A3F8A selector -------------
    if(want("4a3f80")){
        restore(0x4a3f8au,6); // only the protected selector; 0x4A3FD2 keeps the r046 43E3B0 stub
        for(unsigned i=0;i<N;++i){
            std::array<std::uint8_t,0x1100> native{};for(auto& b:native)b=std::uint8_t(rng());Bytes ne(native.data(),native.size());
            const std::int32_t hint=std::int32_t(rnd(12));
            const std::int32_t cached=rnd(5)==0?-1:(rnd(6)==0?std::int32_t(rng()):std::int32_t(rnd(12)));ne.puti(0x10d0,cached);
            ne.puti(0x5c,int(rnd(3)));ne.put16(0x64,std::uint16_t(rng()));
            if(rnd(3)==0){ne.put32(0x10c0,ne.u32(0x5c));ne.put16(0x10c8,std::uint16_t(ne.i16(0x64)));ne.put32(0x10cc,ne.u32(0x68));if(rnd(2))ne.puti(0x10d0,hint);}
            PcRoadCacheRefreshInputs in{};in.selector_result=pc_road_cache_selector_4a3f8a(ne);in.query_success=rnd(4)!=0;for(auto& b:in.query_output)b=std::uint8_t(rng());
            set_r046_cache_inputs(in);std::memcpy(reinterpret_cast<void*>(obj),native.data(),native.size());
            prepare(0x4a3f80u);stack()[0]=obj;stack()[1]=std::uint32_t(hint);run();
            auto nr=refresh_cached_road_4a3f80(ne,hint,in)?1u:0u;if(inject&&i==N-1)nr^=1u;
            check(guest_call.out_eax==nr&&std::memcmp(reinterpret_cast<void*>(obj),native.data(),native.size())==0,"4A3F80 cache refresh (4A3F8A selector)",i);
        }
    }
    // ---- 0x4A45F0 CalcOfsLeftLane with measured 4A45F9 selector + real 43D130 --
    if(want("4a45f0")){
        restore(0x4a45f0u,0x10);restore(0x4a4659u,5);restore(0x43d130u,0xa0);
        for(unsigned i=0;i<N;++i){
            std::array<std::uint8_t,0x1100> native{};Bytes ne(native.data(),native.size());
            auto rf=[&](float lo,float hi){return lo+(hi-lo)*(float(rng()&0xffffu)/65535.0f);};
            ne.putf(0x14,rf(-150,150));ne.putf(0x18,rf(-20,40));ne.putf(0x1c,rf(-150,150));ne.putf(0x58,rf(-0.5f,1.5f));
            const std::uint32_t type=rnd(4);ne.put32(0x5c,type);ne.put32(0x1f4,rnd(7)==0?200u:rnd(401));
            ne.puti(0x230,rnd(8)==0?-1:std::int32_t(rnd(12)));
            std::array<std::vector<std::uint8_t>,4> tabs;
            for(unsigned t=0;t<4;++t){tabs[t].resize(64);for(auto& b:tabs[t])b=std::uint8_t(rng());const bool present=rnd(5)!=0;
                std::memcpy(reinterpret_cast<void*>(arena+t*0x100),tabs[t].data(),64);p32(0x780160u+t*4,present?arena+t*0x100:0u);}
            PcOfsLeftLaneInputs in{};in.selector=pc_ofs_left_lane_selector_4a45f9(ne);
            {Bytes tv(tabs[type].data(),64);in.rates=pc_lane_rates_43d130(in.selector,g32(0x780160u+type*4)?&tv:nullptr);}
            for(unsigned k=0;k<4;++k)in.points[k]={rf(-250,250),rf(-30,50),rf(-250,250)};
            set_ofs_left_lane_inputs(in);std::memcpy(reinterpret_cast<void*>(obj),native.data(),native.size());
            prepare(0x4a45f0u);stack()[0]=obj;run();
            calc_ofs_left_lane_4a45f0(ne,in);if(inject&&i==N-1)native[0x58]^=1u;
            check(std::memcmp(reinterpret_cast<void*>(obj),native.data(),native.size())==0,"4A45F0 CalcOfsLeftLane (4A45F9 selector, 43D130)",i);
        }
    }
    #include "car_services_roads.inc"
    std::cerr<<"car_services_probe per routine:\n";for(auto& kv:tally)std::cerr<<"  "<<kv.first<<": "<<kv.second.first<<" cases, "<<kv.second.second<<" mismatches\n";
    std::cout<<"{\"routine\":\"car_services_probe\",\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}"<<'\n';return mismatches?1:0;
}

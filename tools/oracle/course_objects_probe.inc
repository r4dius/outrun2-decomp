// Included inside the compare_pc dispatcher; uses its private x86 harness.
if(only=="course_objects_probe"){
    // Stage-object lane services of the race AREA runtime
    // (race_area_services.cpp) against the original code:
    //   4EFD20(e, lane)  table copy into 84BD08 (lane 0) / 6A5DC4 (lane != 0);
    //   4F0D10(lane)     protected prologue live, 440110 a logging stub:
    //                    the opened (event, function) pairs are compared;
    //   4F0430(lane)     state 1 (protected entry live), 44F880 stubbed as
    //                    finished and 44FC60 delivering a scripted payload:
    //                    the relocated payload is compared;
    //   4F0CB0(id, f, d) the 440110 log and the record 84D6D8 + (id-15C)*34;
    //   4F0910(v)        the 84D8F0 swap.
    using namespace outrun::platform;
    constexpr unsigned base=0x3ad00000u,stubs=base,logw=base+0xff00u,payload=base+0x20000u,records=base+0x40000u,table=base+0x60000u;
    map_at(base,0x100000,PROT_READ|PROT_WRITE|PROT_EXEC);map_original_arithmetic_bridge(file);
    {   // The dispatcher's start-up patches (other oracles' stubs) are undone first.
        const std::size_t text_file=pe_file_offset(file,0x401000u);
        for(std::uint32_t a=0x401000u;a<0x401000u+0x194881u;a+=0x100u){
            const std::size_t n=std::min<std::size_t>(0x100u,0x401000u+0x194881u-a);
            if(std::memcmp(reinterpret_cast<void*>(a),file.data()+text_file+(a-0x401000u),n))r060_raw_write(a,file.data()+text_file+(a-0x401000u),n);
        }
    }
    for(unsigned page:{0x84b000u,0x84c000u,0x84d000u,0x6a5000u})
        if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("course objects globals");
    auto emit32=[](std::vector<std::uint8_t>& b,std::uint32_t v){for(unsigned k=0;k<4;++k)b.push_back(std::uint8_t(v>>(k*8)));};
    unsigned off=0;
    auto stub_at=[&](const std::vector<std::uint8_t>& c){const auto at=stubs+off;std::memcpy(reinterpret_cast<void*>(at),c.data(),c.size());off=(off+unsigned(c.size())+15u)&~15u;return at;};
    {   // 440110(event, function): log {event, function} at [logw] (count), entries after it.
        std::vector<std::uint8_t> c{0x8b,0x0d};emit32(c,logw);                               // mov ecx,[logw]
        c.insert(c.end(),{0x8b,0x44,0x24,0x04,0x89,0x04,0xcd});emit32(c,logw+8);            // mov eax,[esp+4]; mov [ecx*8+logw+8],eax
        c.insert(c.end(),{0x8b,0x44,0x24,0x08,0x89,0x04,0xcd});emit32(c,logw+12);           // mov eax,[esp+8]; mov [ecx*8+logw+12],eax
        c.insert(c.end(),{0x41,0x89,0x0d});emit32(c,logw);c.push_back(0xc3);                 // inc ecx; mov [logw],ecx; ret
        {const auto log440110=stub_at(c);r046_write_rel32(0x4f0d4cu,0xe8,log440110);r046_write_rel32(0x4f0cc3u,0xe8,log440110);}
        c={0xb8};emit32(c,1u);c.push_back(0xc3);r046_write_rel32(0x4f046au,0xe8,stub_at(c));   // 44F880: finished
        c={0x8b,0x44,0x24,0x08,0xc7,0x00};emit32(c,payload);c.insert(c.end(),{0x31,0xc0,0xc3}); // 44FC60: [arg1] = payload
        r046_write_rel32(0x4f0493u,0xe8,stub_at(c));
        if(std::getenv("OR2_OBJ_TRACE")){ // DEBUG: record eax/ebx/flags at 4F0448
            c={0x9c,0xa3};emit32(c,logw+0x80);c.insert(c.end(),{0x89,0x1d});emit32(c,logw+0x84);
            c.insert(c.end(),{0x58,0xa3});emit32(c,logw+0x88);c.insert(c.end(),{0x50,0x9d});
            c.insert(c.end(),{0xa1});emit32(c,logw+0x80);
            c.push_back(0xbe);emit32(c,1u);c.push_back(0xe9);
            const auto at=stubs+off;const std::int32_t rel=std::int32_t(0x4f044du-(at+unsigned(c.size())+4u));emit32(c,std::uint32_t(rel));
            r046_write_rel32(0x4f0448u,0xe9,stub_at(c));}
    }
    std::mt19937 rng(0x4f0d1000u);
    auto rnd=[&](std::uint32_t n){return n?std::uint32_t(rng()%n):0u;};
    auto g32=[](std::uint32_t a){return *reinterpret_cast<std::uint32_t*>(a);};
    auto p32=[](std::uint32_t a,std::uint32_t v){*reinterpret_cast<std::uint32_t*>(a)=v;};
    auto pf=[](std::uint32_t a,float v){std::memcpy(reinterpret_cast<void*>(a),&v,4);};
    unsigned cases{},mismatches{};const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
    auto check=[&](bool ok,const std::string& what){++cases;if(!ok){++mismatches;if(mismatches<20)std::cerr<<what<<"\n";}};
    std::map<std::string,unsigned> coverage;
    // Native memory over copies at the same PC addresses.
    auto region=[](std::uint32_t a,std::uint32_t n){return std::vector<std::uint8_t>(reinterpret_cast<const std::uint8_t*>(std::uintptr_t(a)),reinterpret_cast<const std::uint8_t*>(std::uintptr_t(a))+n);};
    for(unsigned iter=0;iter<count*300u;++iter){
        const unsigned part=iter%4u;
        if(part==0){   // ---- 4EFD20 ----
            const std::uint32_t lane=rnd(3)==0?0u:rnd(3);
            const std::uint32_t n=rnd(10);
            for(std::uint32_t k=0;k<0x100;k+=4)p32(table+k,rng());
            for(std::uint32_t k=0;k<n;++k)if((*reinterpret_cast<std::uint16_t*>(table+k*12))==0xffffu)*reinterpret_cast<std::uint16_t*>(table+k*12)=0x1234;
            *reinterpret_cast<std::uint16_t*>(table+n*12)=0xffffu;
            for(std::uint32_t k=0;k<0x60;k+=4)p32(0x84bd08u+k,rng());
            for(std::uint32_t k=0;k<0x18;k+=4)p32(0x6a5dc4u+k,rng());
            auto t0=region(table,0x100),b0=region(0x84bd08u,0x60),d0=region(0x6a5dc4u,0x18);
            prepare(0x4efd20u);reinterpret_cast<unsigned*>(S)[0]=table;reinterpret_cast<unsigned*>(S)[1]=lane;run_original32();
            PcRaceMemory m;m.map(table,t0.data(),t0.size());m.map(0x84bd08u,b0.data(),b0.size());m.map(0x6a5dc4u,d0.data(),d0.size());
            race_objects_copy_4efd20(m,table,lane);
            if(inject&&iter==3u)b0[1]^=1u;
            check(region(0x84bd08u,0x60)==b0&&region(0x6a5dc4u,0x18)==d0,"4EFD20 lane "+std::to_string(lane)+" n "+std::to_string(n)+" iter "+std::to_string(iter));
            ++coverage[lane?"4EFD20 lane!=0":(n>=7?"4EFD20 lane0 full":"4EFD20 lane0")];
        }else if(part==1){   // ---- 4F0D10 ----
            const std::uint32_t lane=rnd(3);
            const std::uint32_t n=rnd(6);
            const std::uint32_t root_cell=records-0x10u;
            for(std::uint32_t k=0;k<0x400;k+=4)p32(records+k,rng());
            for(std::uint32_t k=0;k<16;++k){pf(records+k*0x28+4,rnd(4)==0?float(rnd(20000))-10000.f:float(rnd(5000)));p32(records+k*0x28+0x1c,rnd(8));}
            for(std::uint32_t k=0;k<n;++k)pf(records+k*0x28+4,float(rnd(20000))-9999.f);
            pf(records+n*0x28+4,rnd(2)?-10000.5f:-20000.f);
            const bool null_root=rnd(8)==0,null_record=rnd(8)==0,low_zero=rnd(10)==0;
            p32(root_cell,null_record?0u:records);
            const std::uint32_t root=null_root?0u:(low_zero?0x3adf0000u:root_cell);
            if(low_zero)p32(0x3adf0000u,records);
            p32(0x84d6ccu+lane*4u,root);p32(logw,0);
            prepare(0x4f0d10u);reinterpret_cast<unsigned*>(S)[0]=lane;run_original32();
            std::vector<std::pair<std::uint32_t,std::uint32_t>> glog,nlog;
            for(std::uint32_t k=0;k<g32(logw);++k)glog.push_back({g32(logw+8+k*8),g32(logw+12+k*8)});
            PcRaceMemory m;auto rr=region(records-0x10u,0x410);m.map(records-0x10u,rr.data(),rr.size());
            std::vector<std::uint8_t> lz(4);std::memcpy(lz.data(),&records,4);if(low_zero)m.map(0x3adf0000u,lz.data(),4);
            for(std::size_t i=0;i<EmbeddedExeRangeCount;++i)m.map_const(EmbeddedExeRanges[i].base,EmbeddedExeRanges[i].data,EmbeddedExeRanges[i].size);
            race_objects_open_4f0d10(m,lane,root,[](void* u,std::uint32_t e,std::uint32_t f){static_cast<std::vector<std::pair<std::uint32_t,std::uint32_t>>*>(u)->push_back({e,f});},&nlog);
            if(inject&&iter==4u)nlog.push_back({0,0});
            check(glog==nlog,"4F0D10 lane "+std::to_string(lane)+" root "+std::to_string(root)+" nullrec "+std::to_string(null_record)+" guest "+std::to_string(glog.size())+" native "+std::to_string(nlog.size())+" iter "+std::to_string(iter));
            ++coverage[null_root||low_zero?"4F0D10 no root":null_record?"4F0D10 no records":"4F0D10 opens "+std::to_string(std::min<std::size_t>(glog.size(),3))];
        }else if(part==3){   // ---- 4F0CB0 / 4F0910 ----
            const std::uint32_t id=0x15cu+rnd(10),fn=rnd(0x80),data=records+rnd(0x40)*4u;
            for(std::uint32_t k=0;k<0x200;k+=4)p32(records+k,rng());
            for(std::uint32_t a=0x84d6d8u;a<0x84d8f4u;a+=4)p32(a,rng());
            p32(logw,0);
            auto vis=region(0x84d6d8u,0x84d8f4u-0x84d6d8u),rec=region(records,0x200);
            prepare(0x4f0cb0u);reinterpret_cast<unsigned*>(S)[0]=id;reinterpret_cast<unsigned*>(S)[1]=fn;reinterpret_cast<unsigned*>(S)[2]=data;run_original32();
            std::vector<std::pair<std::uint32_t,std::uint32_t>> glog,nlog;
            for(std::uint32_t k=0;k<g32(logw);++k)glog.push_back({g32(logw+8+k*8),g32(logw+12+k*8)});
            PcRaceMemory m;m.map(0x84d6d8u,vis.data(),vis.size());m.map(records,rec.data(),rec.size());
            race_objects_dynamic_open_4f0cb0(m,id,fn,data,[](void* u,std::uint32_t e,std::uint32_t f){static_cast<std::vector<std::pair<std::uint32_t,std::uint32_t>>*>(u)->push_back({e,f});},&nlog);
            if(inject&&iter==7u)vis[0x30]^=1u;
            check(glog==nlog&&region(0x84d6d8u,0x84d8f4u-0x84d6d8u)==vis,"4F0CB0 id "+std::to_string(id)+" iter "+std::to_string(iter));
            const std::uint32_t v=rng();const std::uint32_t before=g32(0x84d8f0u);
            prepare(0x4f0910u);reinterpret_cast<unsigned*>(S)[0]=v;run_original32();
            const std::uint32_t ret=race_objects_frame_4f0910(m,v);
            check(guest_call.out_eax==ret&&g32(0x84d8f0u)==m.u32(0x84d8f0u)&&ret==before,"4F0910 iter "+std::to_string(iter));
            ++coverage["4F0CB0 / 4F0910"];
        }else{   // ---- 4F0430 state 1 ----
            const std::uint32_t lane=rnd(3);
            const std::uint32_t n=rnd(6);
            std::vector<std::uint8_t> img(0x2000,0);
            auto put=[&](std::uint32_t o,std::uint32_t v){std::memcpy(img.data()+o,&v,4);};
            const std::uint32_t objs=0x100;
            for(std::uint32_t k=0;k<n;++k){
                put(k*8,objs+k*0x60);put(k*8+4,rng());
                for(std::uint32_t f=0;f<0x60;f+=4)put(objs+k*0x60+f,rng());
                for(std::uint32_t f=4;f<=0x4c;f+=8)put(objs+k*0x60+f,rnd(3)?rnd(0x1000)&~3u:0u);
            }
            put(n*8,0);
            std::memcpy(reinterpret_cast<void*>(payload),img.data(),img.size());
            p32(0x84ce7cu+lane*4u,1);p32(0x84ce88u+lane*4u,0x55aa00u+lane);
            prepare(0x4f0430u);reinterpret_cast<unsigned*>(S)[0]=0x5a5a0000u|lane;reinterpret_cast<unsigned*>(S)[1]=lane;run_original32(); // 4F0430(token, lane)
            PcRaceMemory m;m.map(payload,img.data(),img.size());
            race_objects_relocate_4f0430(m,payload);
            if(inject&&iter==5u)img[0]^=1u;
            check(region(payload,0x2000)==img,"4F0430 relocation n "+std::to_string(n)+" iter "+std::to_string(iter));
            check(g32(0x84ce7cu+lane*4u)==2u&&g32(0x84ce88u+lane*4u)==0u,"4F0430 state words iter "+std::to_string(iter)+" lane "+std::to_string(lane)+" state "+std::to_string(g32(0x84ce7cu+lane*4u))+" slot "+std::to_string(g32(0x84ce88u+lane*4u))+" 84D6C0 "+std::to_string(g32(0x84d6c0u+lane*4u))+" eax "+std::to_string(guest_call.out_eax)+" vm eax/ebx/fl "+std::to_string(g32(logw+0x80))+"/"+std::to_string(g32(logw+0x84))+"/"+std::to_string(g32(logw+0x88)));
            ++coverage[n?"4F0430 relocate":"4F0430 empty"];
        }
    }
    for(const auto& [k,v]:coverage)std::cerr<<"coverage "<<k<<": "<<v<<"\n";
    std::cout<<"{\"routine\":\"course_objects_probe\",\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}"<<'\n';return 0;
}

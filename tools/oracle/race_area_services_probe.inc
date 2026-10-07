// Included inside the compare_pc dispatcher; uses its private x86 harness.
if(only=="race_area_services_probe"){
    // Services ported for the race AREA/SKY runtime (race_area_services.cpp)
    // against the original code:
    //   44F980  one step of the async loader on a slot (mode-7 transfer into
    //           a handle allocation), file I/O leaves stubbed: 4239C0 open,
    //           423F10 size, 423CF0 read (from a scripted stream: .sz =
    //           {u32 size, payload} as the "rbgx" stream delivers it, other
    //           files the raw bytes), 423E30 done, 423BD0 close, 580253 malloc;
    //           compared: the allocation (payload + {p, mode}), the slot
    //           words +04 +14 +1C +24, the malloc size.
    //   44FC60  (+ protected bridge 44FCA4 live) on a completed/empty slot.
    //   407C30 / 407E40 light words 899B98..89A4F8, 4103A0 (89EDBC..C4),
    //   406780 on a synthetic loaded bank (7C2800 entry +0C/+20), 43F960.
    using namespace outrun::platform;
    constexpr unsigned base=0x3ac00000u,stubs=base,stream=base+0x10000u,cursor=base+0xfff0u,sizew=base+0xfff4u,mallocw=base+0xfff8u;
    constexpr unsigned freew=base+0xffecu,heap=base+0x40000u,slotg=0x7d34d0u,path=base+0x30000u,bank=base+0x80000u;
    map_at(base,0x100000,PROT_READ|PROT_WRITE|PROT_EXEC);map_original_arithmetic_bridge(file);
    {   unsigned restored=0;const std::size_t text_file=pe_file_offset(file,0x401000u);
        for(std::uint32_t a=0x401000u;a<0x401000u+0x194881u;a+=0x100u){
            const std::size_t n=std::min<std::size_t>(0x100u,0x401000u+0x194881u-a);
            if(std::memcmp(reinterpret_cast<void*>(a),file.data()+text_file+(a-0x401000u),n)){r060_raw_write(a,file.data()+text_file+(a-0x401000u),n);++restored;}
        }
        std::cerr<<"restored "<<restored<<" patched .text blocks"<<char(10);
    }
    for(unsigned page:{0x7d3000u,0x899000u,0x89a000u,0x89e000u,0x7c2000u,0x7c3000u,0x7b1000u,0x780000u,0x62f000u,0x630000u})
        if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("services globals");
    auto emit32=[](std::vector<std::uint8_t>& b,std::uint32_t v){for(unsigned k=0;k<4;++k)b.push_back(std::uint8_t(v>>(k*8)));};
    unsigned off=0;
    auto install=[&](std::uint32_t pc,const std::vector<std::uint8_t>& c){
        std::memcpy(reinterpret_cast<void*>(stubs+off),c.data(),c.size());r046_write_rel32(pc,0xe9,stubs+off);off=(off+unsigned(c.size())+15u)&~15u;};
    {   // 4239C0 open -> 0x5100; 423F10 size -> [sizew]; 423E30 -> 1; 423BD0 -> ret
        std::vector<std::uint8_t> c{0xb8};emit32(c,0x5100u);c.push_back(0xc3);install(0x4239c0u,c);
        c={0xa1};emit32(c,sizew);c.push_back(0xc3);install(0x423f10u,c);
        c={0xb8};emit32(c,1u);c.push_back(0xc3);install(0x423e30u,c);
        c={0xc3};install(0x423bd0u,c);
        // 580BC2 free(p): [freew] = p
        c={0x8b,0x44,0x24,0x04,0xa3};emit32(c,freew);c.push_back(0xc3);install(0x580bc2u,c);
        // 580253 malloc(n): [mallocw] = n, eax = heap
        c={0x8b,0x44,0x24,0x04,0xa3};emit32(c,mallocw);c.push_back(0xb8);emit32(c,heap);c.push_back(0xc3);install(0x580253u,c);
        // 423CF0(dest, count, 0, file, 0): copy count bytes from [cursor], advance, eax = count
        c={0x56,0x57,0x51,0x8b,0x7c,0x24,0x10,0x8b,0x4c,0x24,0x14,0x8b,0x35};emit32(c,cursor);
        c.insert(c.end(),{0xf3,0xa4,0x89,0x35});emit32(c,cursor);
        c.insert(c.end(),{0x8b,0x44,0x24,0x14,0x59,0x5f,0x5e,0xc3});install(0x423cf0u,c);
    }
    std::mt19937 rng(0x44f98000u);
    auto rnd=[&](std::uint32_t n){return n?std::uint32_t(rng()%n):0u;};
    auto g32=[](std::uint32_t a){return *reinterpret_cast<std::uint32_t*>(a);};
    auto p32=[](std::uint32_t a,std::uint32_t v){*reinterpret_cast<std::uint32_t*>(a)=v;};
    unsigned cases{},mismatches{};const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
    auto check=[&](bool ok,const std::string& what){++cases;if(!ok){++mismatches;if(mismatches<20)std::cerr<<what<<"\n";}};
    std::map<std::string,unsigned> coverage;
    for(unsigned iter=0;iter<count;++iter){
        const unsigned part=iter%6u;
        if(part==0){   // ---- 44F980 loader step -----------------------------
            const bool sz=rnd(2)!=0;const std::uint32_t size=1u+rnd(0x3000);
            const std::uint32_t modes[]={1,2,3,4,5,7,9,0,6};const std::uint32_t mode=modes[rnd(9)];
            std::vector<std::uint8_t> payload(size);for(auto& b:payload)b=std::uint8_t(rng());
            std::vector<std::uint8_t> streamb;
            if(sz){for(unsigned k=0;k<4;++k)streamb.push_back(std::uint8_t(size>>(k*8)));}
            streamb.insert(streamb.end(),payload.begin(),payload.end());
            std::memcpy(reinterpret_cast<void*>(stream),streamb.data(),streamb.size());
            p32(cursor,stream);p32(sizew,sz?0xdeadu:size);p32(mallocw,0);
            std::memset(reinterpret_cast<void*>(heap),0xcd,size+16);
            std::memset(reinterpret_cast<void*>(slotg),0,0x30);
            p32(slotg,1);p32(slotg+0x10,path);p32(slotg+0x20,sz?0u:1u);p32(slotg+0x2c,mode);
            prepare(0x44f980u);guest_call.esi=slotg;run();
            std::vector<std::uint8_t> image;std::string error;
            const bool ok=race_area_loader_image(sz?streamb:payload,sz,heap,mode,image,error);
            if(inject&&iter==6)image[0]^=1u;
            check(ok,"native loader image failed: "+error);
            if(!ok)continue;
            check(g32(mallocw)==size+8u,"580253 size");
            check(!std::memcmp(image.data(),reinterpret_cast<void*>(heap),image.size()),"allocation payload + {p, mode}");
            check(g32(slotg+0x14)==heap+std::uint32_t(image.size()-8u),"slot +14 handle");
            check(g32(slotg+0x24)==heap,"slot +24 destination");
            check(g32(slotg+0x1c)==size,"slot +1C size");
            check(g32(slotg+4)==4u,"slot +04 state");
            ++coverage[sz?"44F980 .sz":"44F980 raw"];
        }else if(part==1){   // ---- 44FC60 + bridge 44FCA4 ---------------------
            std::memset(reinterpret_cast<void*>(slotg),0,0x30);
            PcRaceAsyncSlot s{};
            const unsigned kind=rnd(3);           // 0 completed, 1 empty (+0 == 0), 2 null slot
            const std::uint32_t H=base+0x22000u+rnd(0x100)*8u;p32(H,rng());p32(H+4,rnd(10));p32(freew,0);
            if(kind==0){s.word0=1u+rnd(0xff);s.state=4;s.handle_14=rnd(4)?H:0u;s.dest_24=rng();s.size_28=rng();
                p32(slotg,s.word0);p32(slotg+4,4);p32(slotg+0x14,s.handle_14);p32(slotg+0x24,s.dest_24);p32(slotg+0x28,s.size_28);}
            const std::uint32_t outs=base+0x20000u;
            const bool ua=rnd(4)!=0,ub=rnd(4)!=0,uo=rnd(4)!=0;
            p32(outs,0x11111111u);p32(outs+4,0x22222222u);p32(outs+8,0x33333333u);
            prepare(0x44fc60u);reinterpret_cast<unsigned*>(S)[0]=kind==2?0u:slotg;reinterpret_cast<unsigned*>(S)[1]=ua?outs:0u;
            reinterpret_cast<unsigned*>(S)[2]=ub?outs+4:0u;reinterpret_cast<unsigned*>(S)[3]=uo?outs+8:0u;run();
            std::uint32_t x=0x11111111u,y=0x22222222u,h=0x33333333u;
            const auto freed=race_async_take_44fc60(s,ua?&x:nullptr,ub?&y:nullptr,uo?&h:nullptr);
            check(g32(outs)==x&&g32(outs+4)==y&&g32(outs+8)==h,"44FC60 outputs");
            check(g32(freew)==(freed?g32(freed):0u),"44FC60 released handle (440CD0 -> 580BC2)");
            if(kind!=2){std::array<std::uint8_t,0x30> zero{};check(!std::memcmp(zero.data(),reinterpret_cast<void*>(slotg),0x30),"44FC60 slot released");}
            ++coverage["44FC60 kind "+std::to_string(kind)];
        }else if(part==2){   // ---- 407C30 / 407E40 ---------------------------
            std::vector<std::uint8_t> lights(0x960);for(auto& b:lights)b=std::uint8_t(rng());
            std::memcpy(reinterpret_cast<void*>(0x899b98u),lights.data(),lights.size());
            const std::uint32_t b=rnd(3),a=rnd(2),c=b==0?rnd(3):rnd(2);
            const bool e40=rnd(2)!=0;
            Bytes l(lights.data(),lights.size());
            if(!e40){const auto v=rng();prepare(0x407c30u);reinterpret_cast<unsigned*>(S)[0]=a;reinterpret_cast<unsigned*>(S)[1]=b;
                reinterpret_cast<unsigned*>(S)[2]=c;reinterpret_cast<unsigned*>(S)[3]=v;run();race_light_word_407c30(l,a,b,c,v);}
            else{const auto v0=rng(),v1=rng(),v2=rng();prepare(0x407e40u);unsigned* st=reinterpret_cast<unsigned*>(S);
                st[0]=a;st[1]=b;st[2]=c;st[3]=v0;st[4]=v1;st[5]=v2;run();race_light_words_407e40(l,a,b,c,v0,v1,v2);}
            check(!std::memcmp(lights.data(),reinterpret_cast<void*>(0x899b98u),lights.size()),e40?"407E40 light words":"407C30 light words");
            ++coverage[e40?"407E40":"407C30"];
        }else if(part==3){   // ---- 4103A0 --------------------------------------
            const std::uint32_t a0=rnd(3)?rnd(4):rng(),a1=rng();
            p32(0x89edbcu,rng());p32(0x89edc0u,rng());p32(0x89edc4u,rng());
            prepare(0x4103a0u);reinterpret_cast<unsigned*>(S)[0]=a0;reinterpret_cast<unsigned*>(S)[1]=a1;run();
            const auto w=race_shader_mode_4103a0(a0,a1);
            check(g32(0x89edbcu)==w[0]&&g32(0x89edc0u)==w[1]&&g32(0x89edc4u)==w[2],"4103A0 words");
            ++coverage["4103A0"];
        }else if(part==4){   // ---- 406780 on a synthetic bank ----------------
            // guest: entry 7C2800+id*48: +0C count, +20 -> [objects pointer];
            // objects = S0 + 0x18 + i*0x3C with absolute pointers (+18 range,
            // +1C meshes); native: the same system bytes with offsets.
            const std::uint32_t id=rnd(0x20),count=rnd(6);
            const std::uint32_t sys=bank,objects=sys+0x18u;
            std::vector<std::uint8_t> system(0x4000);for(auto& b:system)b=std::uint8_t(rng());
            Bytes ns(system.data(),system.size());
            for(std::uint32_t i=0;i<6;++i){
                const std::uint32_t rec=0x18u+i*0x3cu,range=0x400u+i*0x10u,meshes=0x800u+i*0x400u;
                const std::uint32_t n=rnd(10);const std::uint32_t start=rng()&0xffffu;
                ns.put32(rec+0x18,range);ns.put32(rec+0x1c,meshes);ns.put32(range,start);ns.put32(range+4,start+n*0x38u+(rnd(3)==0?rnd(0x38):0u));
            }
            std::memcpy(reinterpret_cast<void*>(sys),system.data(),system.size());
            for(std::uint32_t i=0;i<6;++i){const std::uint32_t rec=sys+0x18u+i*0x3cu;p32(rec+0x18,g32(rec+0x18)+sys);p32(rec+0x1c,g32(rec+0x1c)+sys);}
            const std::uint32_t holder=base+0x21000u;p32(holder,objects);   // [entry+20] -> holder -> object base (index*0x3C added by 406780)
            // native offsets are relative to the system base: object table at +18
            p32(0x7c2800u+id*0x48u+0xcu,count);p32(0x7c2800u+id*0x48u+0x20u,holder);
            const std::uint32_t object=rnd(8)==0?0xffffffffu:((id<<16)|rnd(8)),mask=rng();
            prepare(0x406780u);reinterpret_cast<unsigned*>(S)[0]=object;reinterpret_cast<unsigned*>(S)[1]=mask;run();
            const auto native=race_mesh_count_406780(ns,count,object,mask);
            check(guest_call.out_eax==native,"406780 count");
            ++coverage[object==0xffffffffu?"406780 none":((object&0xffffu)<count?"406780 object":"406780 out of range")];
        }else{   // ---- 43F960 ------------------------------------------------------
            const std::uint32_t v=rnd(3)?rnd(6):rng();p32(0x78024cu,v);
            prepare(0x43f960u);run();
            check(guest_call.out_eax==race_preset_43f960(v),"43F960");
            ++coverage["43F960"];
        }
    }
    for(const auto& [k,n]:coverage)std::cerr<<"  "<<k<<": "<<n<<"\n";
    std::cout<<"{\"routine\":\"race_area_services_probe\",\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}"<<'\n';
    return mismatches?1:0;
}

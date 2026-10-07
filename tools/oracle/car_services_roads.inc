// Included inside car_services_probe (car_services_verify.inc): course road
// sample tables 0x804480/0x800DA8/0x80A670, 46F7A0, 479D90 and the streamer
// 46FAC0/46FC40/46FDE0 (+44C680 filename).
{
    constexpr std::uint32_t Primary=0x804480u,Preload=0x800da8u,Secondary=0x80a670u;
    constexpr std::uint32_t PrimaryBytes=6u*0x1030u,PreloadBytes=2u*0x1030u,SecondaryBytes=12u*0x70cu;
    auto put_sample=[&](std::uint32_t at,std::int16_t x,std::int16_t z,std::uint16_t y15,bool off){
        p16(at,std::uint16_t(x));p16(at+2,std::uint16_t(z));p16(at+4,std::uint16_t((y15&0x7fffu)|(off?0x8000u:0u)));};
    // ---- 0x46F7A0 route flags ----------------------------------------------
    if(want("46f7a0")){
        restore(0x46f7a0u,0x180);
        for(unsigned i=0;i<N;++i){
            const std::uint16_t idx=std::uint16_t(rnd(689));
            std::array<std::array<std::uint16_t,3>,3> proto{};for(auto& p:proto)for(auto& w:p)w=std::uint16_t(rnd(4)==0?rng():rnd(3));
            for(unsigned b=0;b<6;++b){const auto& p=proto[rnd(3)];const std::uint32_t at=Primary+b*0x1030u+4u+idx*6u;
                p16(at,p[0]);p16(at+2,p[1]);p16(at+4,std::uint16_t(p[2]^(rnd(4)==0?0x8000u:0u)^(rnd(12)==0?1u:0u)));}
            std::array<std::uint8_t,0x70> ev{};for(auto& b:ev)b=std::uint8_t(rng());Bytes e(ev.data(),ev.size());
            e.put32(0x5c,rnd(5)==0?(rnd(2)?1u:std::uint32_t(rng())):0u);e.put16(0x64,idx);
            std::memcpy(reinterpret_cast<void*>(obj),ev.data(),ev.size());
            std::vector<std::uint8_t> prim(PrimaryBytes);std::memcpy(prim.data(),reinterpret_cast<void*>(Primary),prim.size());
            std::vector<std::uint8_t> pre(PreloadBytes),sec(SecondaryBytes);
            PcRoadTableArena t{Bytes(prim.data(),prim.size()),Bytes(pre.data(),pre.size()),Bytes(sec.data(),sec.size())};
            prepare(0x46f7a0u);stack()[0]=obj;run();
            auto r=pc_route_flags_46f7a0(e,t);if(inject&&i==N-1)r^=1u;
            check((guest_call.out_eax&0xffu)==r,"46F7A0 route flags",i);
        }
    }
    // ---- 0x479D90 route width ----------------------------------------------
    if(want("479d90")){
        restore(0x479d90u,0x1c0);
        for(unsigned i=0;i<N;++i){
            const std::uint32_t type=rnd(3)==0?(rnd(4)==0?std::uint32_t(rng())|1u:1u):0u;
            const unsigned blocks=type==0?6u:12u,block_bytes=type==0?0x1030u:0x70cu;const std::uint32_t region=type==0?Primary:Secondary;
            const std::uint16_t idx=std::uint16_t(rnd(type==0?689:299));
            const std::uint8_t sel_place=std::uint8_t(rnd(blocks)),sel_arg=std::uint8_t(rnd(blocks));
            for(unsigned b:{unsigned(sel_place),unsigned(sel_arg)}){
                p16(region+b*block_bytes,rnd(7)==0?std::uint16_t(rng()):std::uint16_t(0x4f53u));
                put_sample(region+b*block_bytes+4u+idx*6u,std::int16_t(rng()),std::int16_t(rng()),std::uint16_t(rng()),rnd(6)==0);
            }
            std::array<std::uint8_t,0x10> place{};for(auto& b:place)b=std::uint8_t(rng());Bytes pl(place.data(),place.size());
            pl.put32(0,type);pl.put16(8,idx);pl.put8(0x0a,sel_place);
            std::memcpy(reinterpret_cast<void*>(obj),place.data(),place.size());
            auto rf=[&](float lo,float hi){return lo+(hi-lo)*(float(rng()&0xffffu)/65535.0f);};
            for(std::uint32_t m:{0x7d2da0u,0x7d3190u})for(unsigned k=0;k<16;++k){
                float v=rf(-2.0f,2.0f);if(k==3||k==7||k==11)v=rnd(8)==0?rf(-0.01f,0.01f):0.0f;if(k==15)v=rnd(8)==0?rf(0.5f,2.0f):(rnd(40)==0?0.0f:1.0f);if(k>=12&&k<15)v=rf(-500.0f,500.0f);
                *reinterpret_cast<float*>(m+k*4)=v;}
            reset_matrix_oracle_globals();
            for(unsigned k=0;k<0x10000;k+=4)p32(MatrixArena+k,fb(rf(-1.0f,1.0f)));
            static constexpr std::int32_t depths[]={0,3,62,63,64};const std::int32_t depth=depths[rnd(5)];
            p32(0x89b564u,MatrixArena+0x800u);p32(0x89b568u,std::uint32_t(depth));p32(0x89b56cu,64u);
            const std::int32_t row=0;
            std::vector<std::uint8_t> prim(PrimaryBytes),pre(PreloadBytes),sec(SecondaryBytes),marena(0x10000);
            std::memcpy(prim.data(),reinterpret_cast<void*>(Primary),prim.size());std::memcpy(sec.data(),reinterpret_cast<void*>(Secondary),sec.size());
            std::memcpy(marena.data(),reinterpret_cast<void*>(MatrixArena),marena.size());
            std::array<std::uint8_t,64> m0{},m1{};std::memcpy(m0.data(),reinterpret_cast<void*>(0x7d2da0u),64);std::memcpy(m1.data(),reinterpret_cast<void*>(0x7d3190u),64);
            PcRoadTableArena t{Bytes(prim.data(),prim.size()),Bytes(pre.data(),pre.size()),Bytes(sec.data(),sec.size())};
            PcMatrixStack ms{Bytes(marena.data(),marena.size()),0x800,depth,64};
            prepare(0x479d90u);guest_call.st0=1;stack()[0]=obj;stack()[1]=std::uint32_t(row);stack()[2]=sel_arg;run();
            auto r=pc_route_width_479d90(pl,row,std::int8_t(sel_arg),t,Bytes(m0.data(),64),Bytes(m1.data(),64),ms);
            std::uint8_t nat[10];std::memcpy(nat,&r.value,10);if(inject&&i==N-1)nat[0]^=1u;
            const bool ok=std::memcmp(nat,guest_call.out_st0_raw,10)==0&&g32(0x89b564u)==std::uint32_t(MatrixArena+ms.current_offset)&&
                std::int32_t(g32(0x89b568u))==ms.depth&&std::memcmp(marena.data(),reinterpret_cast<void*>(MatrixArena),marena.size())==0;
            check(ok,r.used_matrices?"479D90 route width (matrix path)":"479D90 route width (FLT_MAX path)",i);
        }
    }
    // ---- 0x44F880 async slot poll (relocated snippet) ---------------------------
    if(want("44f880")||want("loader")){
        restore(0x44f880u,0x30);
        for(unsigned i=0;i<N;++i){
            const bool present=rnd(6)!=0;const std::uint32_t w0=rnd(3)==0?0u:std::uint32_t(rng());const std::int32_t state=rnd(4)==0?std::int32_t(rng()):std::int32_t(rnd(6))-1;
            p32(obj,w0);p32(obj+4,std::uint32_t(state));
            prepare(0x44f880u);stack()[0]=present?obj:0u;guest_call.eax=rng();run();
            auto r=pc_async_slot_finished_44f880(present,w0,state);if(inject&&i==N-1)r^=1u;
            check(guest_call.out_eax==r,"44F880 async slot poll",i);
        }
    }
    // ---- streamer 46FAC0 / 46FC40 / 46FDE0 (+44C680) --------------------------
    if(want("loader")){
        restore(0x46fac0u,0x3a0);restore(0x44c680u,0x1b0);
        struct Rec{std::uint32_t pc,arg;std::array<std::uint32_t,7> rq;};
        struct Io{std::uint32_t handle,finished;std::vector<Rec> calls;std::vector<PcRoadTableRequest> reqs;};
        // Capture stubs for the platform async-file routines (cdecl, one argument).
        auto emit_stub=[&](std::uint32_t at,std::uint32_t pc,unsigned value_slot,bool copy_request){
            std::vector<std::uint8_t> c{0x52,0x53,0x8b,0x15};auto imm=[&](std::uint32_t v){for(unsigned k=0;k<4;++k)c.push_back(std::uint8_t(v>>(8*k)));};
            imm(trace);c.insert(c.end(),{0xc1,0xe2,0x06,0xc7,0x82});imm(trace+0x10);imm(pc);
            c.insert(c.end(),{0x8b,0x4c,0x24,0x0c,0x89,0x8a});imm(trace+0x14);
            if(copy_request)for(unsigned k=0;k<7;++k){c.insert(c.end(),{0x8b,0x59,std::uint8_t(4*k),0x89,0x9a});imm(trace+0x18+4*k);}
            c.insert(c.end(),{0xff,0x05});imm(trace);c.push_back(0xa1);imm(values+4*value_slot);c.insert(c.end(),{0x5b,0x5a,0xc3});
            std::memcpy(reinterpret_cast<void*>(at),c.data(),c.size());
        };
        emit_stub(base+0x1000,0x44fd20u,0,true);emit_stub(base+0x1100,0x44f880u,1,false);emit_stub(base+0x1200,0x44fbe0u,2,false);
        r046_write_rel32(0x44fd20u,0xe9,base+0x1000);r046_write_rel32(0x44f880u,0xe9,base+0x1100);r046_write_rel32(0x44fbe0u,0xe9,base+0x1200);
        PcRoadTableFileService io{};
        io.submit=[](void* u,const PcRoadTableRequest& r)->std::uint32_t{auto* s=static_cast<Io*>(u);s->calls.push_back({0x44fd20u,0,{}});s->reqs.push_back(r);return s->handle;};
        io.finished=[](void* u,std::uint32_t h)->std::uint32_t{auto* s=static_cast<Io*>(u);s->calls.push_back({0x44f880u,h,{}});return s->finished;};
        io.release=[](void* u,std::uint32_t h){auto* s=static_cast<Io*>(u);s->calls.push_back({0x44fbe0u,h,{}});};
        static constexpr std::uint32_t statuses[]={0u,0u,0u,1u,1u,2u,3u,0xffffffffu};
        for(unsigned i=0;i<N;++i){
            for(unsigned k=0;k<24;++k){p32(0x804390u+k*4,statuses[rnd(8)]);p32(0x80a5a8u+k*4,rnd(4)==0?0u:std::uint32_t(rng()));}
            static constexpr std::int32_t stages[]={0,0,0,1,2};
            for(unsigned w=0;w<2;++w){p32(0x803730u+w*4,std::uint32_t(stages[rnd(5)]));p32(0x800d44u+w*4,rnd(14));}
            for(unsigned b=0;b<6;++b)p16(Primary+b*0x1030u,std::uint16_t(rng()));
            for(unsigned b=0;b<2;++b)p16(Preload+b*0x1030u,std::uint16_t(rng()));
            for(unsigned b=0;b<12;++b)p16(Secondary+b*0x70cu,std::uint16_t(rng()));
            const std::uint32_t selected=rnd(3);p32(0x7d33c8u,selected);
            Io s{};s.handle=rnd(5)==0?0u:0x5000u+i;s.finished=rnd(3)==0?0u:(rnd(2)?1u:std::uint32_t(rng())|1u);
            p32(values+0,s.handle);p32(values+4,s.finished);p32(values+8,0);p32(trace,0);
            const std::uint32_t which=rnd(2),kind=rnd(0x42);const bool reset=rnd(4)==0;
            // Native mirror of the guest state before the call.
            PcRoadTableLoaderState st{};
            for(unsigned k=0;k<24;++k){st.status[k]=g32(0x804390u+k*4);st.handle[k]=g32(0x80a5a8u+k*4);}
            for(unsigned w=0;w<2;++w){st.stage[w]=std::int32_t(g32(0x803730u+w*4));st.counter[w]=std::int32_t(g32(0x800d44u+w*4));}
            std::vector<std::uint8_t> prim(PrimaryBytes),pre(PreloadBytes),sec(SecondaryBytes);
            std::memcpy(prim.data(),reinterpret_cast<void*>(Primary),prim.size());std::memcpy(pre.data(),reinterpret_cast<void*>(Preload),pre.size());std::memcpy(sec.data(),reinterpret_cast<void*>(Secondary),sec.size());
            PcRoadTableArena t{Bytes(prim.data(),prim.size()),Bytes(pre.data(),pre.size()),Bytes(sec.data(),sec.size())};
            io.user=&s;std::memset(reinterpret_cast<void*>(0x800d58u),0,0x50);
            std::uint32_t nr=0;
            if(reset){prepare(0x46fc30u);stack()[0]=which;run();pc_road_table_reset_46fac0(st,t,which);}
            else{prepare(0x46fe50u);stack()[0]=which;stack()[1]=kind;run();nr=pc_road_table_step_46fde0(st,t,which,kind,selected,io);}
            if(inject&&i==N-1)st.counter[which]^=1;
            bool ok=reset||guest_call.out_eax==nr;
            for(unsigned k=0;k<24;++k)ok=ok&&g32(0x804390u+k*4)==st.status[k]&&g32(0x80a5a8u+k*4)==st.handle[k];
            for(unsigned w=0;w<2;++w)ok=ok&&std::int32_t(g32(0x803730u+w*4))==st.stage[w]&&std::int32_t(g32(0x800d44u+w*4))==st.counter[w];
            ok=ok&&std::memcmp(prim.data(),reinterpret_cast<void*>(Primary),prim.size())==0&&std::memcmp(pre.data(),reinterpret_cast<void*>(Preload),pre.size())==0&&
               std::memcmp(sec.data(),reinterpret_cast<void*>(Secondary),sec.size())==0;
            const std::uint32_t n=g32(trace);ok=ok&&n==s.calls.size();
            std::size_t rq_index=0;
            for(std::uint32_t k=0;ok&&k<n;++k){
                const std::uint32_t e=trace+0x10+k*64;const std::uint32_t pc=g32(e);ok=ok&&pc==s.calls[k].pc;
                if(pc==0x44fd20u){
                    const auto& r=s.reqs[rq_index++];const std::uint32_t rb=r.region==PcRoadTableRegion::Primary?Primary:(r.region==PcRoadTableRegion::Preload?Preload:Secondary);
                    ok=ok&&g32(e+8)==0x800d58u&&g32(e+0xc)==rb+r.offset&&g32(e+0x10)==r.size&&g32(e+0x14)==0u&&g32(e+0x18)==0u&&g32(e+0x1c)==0u&&g32(e+0x20)==r.flags;
                    ok=ok&&std::string(reinterpret_cast<const char*>(0x800d58u))==r.path&&r.path==st.path;
                }else ok=ok&&g32(e+4)==s.calls[k].arg;
            }
            check(ok,reset?"46FAC0 road table reset":(s.reqs.empty()?"46FDE0/46FC40 road table step":"46FDE0/46FC40/44C680 road table step+request"),i);
        }
    }
}

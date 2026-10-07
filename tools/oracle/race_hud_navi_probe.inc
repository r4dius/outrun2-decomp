// Included by race_hud_probe.inc (shared sprite pool/bank set-up, d3dx wired).
// Original event 388 NAVI_PUB: 4BC9E0 init (4B8E40, 45E0F0 with 45C010,
// 45E050/45C160/45C2C0 and its protected bridge, 4BA690, 4BC390, 4BC5B0,
// 4BA850), 4BCC40 control (450380, 44C940/44C8D0, 4B93E0, 4B9CD0, 4BA8C0,
// 477DC0, 46C840), 4BEB00 display (4B9B40, 4BD340, 4BD230, 4BC990, 4BA9D0
// with its bridge, 42CC30/42CC00, 4B9D80, 4BAD20 with 409F90/40A7D0/40A170/
// 449940/40A010 and the pinned D3DX, 4BCF10, 4B8F30, 4B9010, 4B9100, 4B9450,
// 4294C0, 4BA070, 4BD0A0, 4B9630, 4BDA30/4BD9B0, the protected 4BEDF0 test,
// 46ED20) and 4B8DF0 destroy (45E290), with the real sprite pool code.
// Capture stubs (draw submissions and foreign services): 429530, 429580,
// 4289B0 (with its matrix), 42D280, 42D5F0, 424940; scripted services:
// 45A2B0 rank gateway, 4BA0E0 line of sight, 45DB70 runs original (no OSO object live).
// The native side sees a copy of the whole .data image and the object
// blocks through PcMemory; after every call the whole .data image (except
// the sprite pool, compared structurally) and every object are compared.
{
    constexpr unsigned nb=0x3c400000,cars_at=nb,spec_at=nb+0x8000,camera_at=nb+0xa000,racers_at=nb+0xb000,rank_at=nb+0xb800,
        fb0c_at=nb+0xbc00,records_at=nb+0xc000,misc_at=nb+0xd000,mission_at=nb+0xd800,stubs=nb+0x10000,dtrace=nb+0x20000,
        strace=nb+0x60000,counter=nb+0x6f000;
    map_at(nb,0x70000,PROT_READ|PROT_WRITE|PROT_EXEC);
    // Variants 0 / 7: the record tables [81373C] (qt1, level * 0xFD4 + 0x2F) and [813740] (qt2,
    // QHOT records of 0xFD4) and the ghost block [7F9224] + 0x9CAC (read only, shared natively).
    constexpr unsigned qa=0x3c480000,qt1=qa+0x1000,ghost_at=qa+0x12000,ghost_base=ghost_at+0x100-0x9cac,qb=0x3c500000,qt2=qb;
    map_at(qa,0x13000,PROT_READ|PROT_WRITE);map_at(qb,0x210000,PROT_READ|PROT_WRITE);
    for(std::uint32_t page=0x62f000u;page<0x98d000u;page+=0x1000u)mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE);
    unsigned stub_at=stubs;
    auto install=[&](unsigned pc,const std::vector<std::uint8_t>& code){r060_raw_write(stub_at,code.data(),code.size());r046_write_rel32(pc,0xe9,stub_at);stub_at+=0x100;};
    auto imm=[](std::vector<std::uint8_t>& c,unsigned v){for(unsigned j=0;j<4;++j)c.push_back(std::uint8_t(v>>(8*j)));};
    // Draw recorder: dtrace = count, records of 128 bytes {pc, args[6], pad, matrix @+0x20}.
    auto drawer=[&](unsigned pc,unsigned nargs,bool matrix){std::vector<std::uint8_t> c{0x56,0x57,0x51,0x8b,0x15};imm(c,dtrace);
        c.insert(c.end(),{0x69,0xd2});imm(c,128);c.insert(c.end(),{0x8d,0x92});imm(c,dtrace+16);c.insert(c.end(),{0xc7,0x02});imm(c,pc);
        for(unsigned i=0;i<nargs;++i)c.insert(c.end(),{0x8b,0x44,0x24,std::uint8_t(0x10+4*i),0x89,0x42,std::uint8_t(4+4*i)});
        if(matrix)c.insert(c.end(),{0x8b,0x74,0x24,0x18,0x8d,0x7a,0x20,0xb9,0x10,0,0,0,0xf3,0xa5});
        c.insert(c.end(),{0xff,0x05});imm(c,dtrace);c.insert(c.end(),{0x59,0x5f,0x5e,0xc3});install(pc,c);};
    // 429530/429580/4289B0 run their original 428A10/428EA0 chain; only the
    // SPRANI renderer 429460 is captured (shared stub: trace records).
    drawer(0x42d280,6,false);drawer(0x42d5f0,6,false);
    // Variant 2: the text setters / monospace text 42CA60 42CC60 42CCB0 42CCA0 42CCE0 (42CC00 runs)
    // and the race_traffic Heart Attack routines 462CD0 (display) / 464D20 (control) are recorded.
    drawer(0x42ca60,1,false);drawer(0x42cc60,2,false);drawer(0x42ccb0,1,false);drawer(0x42cca0,1,false);drawer(0x42cce0,1,false);
    drawer(0x462cd0,0,false);drawer(0x464d20,0,false);drawer(0x46ed20,0,false);drawer(0x42ccc0,1,false);drawer(0x4249f0,1,false);
    {std::vector<std::uint8_t> c{0x8b,0x15};imm(c,strace);c.insert(c.end(),{0x8b,0x44,0x24,0x04,0x89,0x04,0x95});imm(c,strace+16);
     c.insert(c.end(),{0xff,0x05});imm(c,strace);c.push_back(0xc3);install(0x424940,c);}
    // 43EB60(mode, &p, 0, 0, &flags) course query: p.y -= 1.5 (x87), flags = bits(p.x) & 0x00100002.
    {std::vector<std::uint8_t> c{0x8b,0x44,0x24,0x08,0x8b,0x4c,0x24,0x14,0xd9,0x40,0x04,0xd8,0x25};imm(c,counter+8);
     c.insert(c.end(),{0xd9,0x58,0x04,0x8b,0x10,0x81,0xe2});imm(c,0x00100002u);c.insert(c.end(),{0x89,0x11,0xc3});install(0x43eb60,c);}
    {const float k=1.5f;std::memcpy(reinterpret_cast<void*>(counter+8),&k,4);}
    auto g8=[](unsigned a){return *reinterpret_cast<std::uint8_t*>(a);};
    auto p8=[](unsigned a,std::uint8_t v){*reinterpret_cast<std::uint8_t*>(a)=v;};
    auto p16=[](unsigned a,std::uint16_t v){*reinterpret_cast<std::uint16_t*>(a)=v;};
    auto pf=[](unsigned a,float v){std::memcpy(reinterpret_cast<void*>(a),&v,4);};
    auto rf=[&](float lo,float hi){return lo+(hi-lo)*float(rng()%100000)/100000.0f;};
    // Native copies: the .data image and the object block.
    constexpr unsigned data_lo=0x62f000,data_hi=0x98d000;
    std::vector<std::uint8_t> ndata(data_hi-data_lo),nobj(0x10000);
    auto to_native=[&]{std::memcpy(ndata.data(),reinterpret_cast<void*>(data_lo),ndata.size());std::memcpy(nobj.data(),reinterpret_cast<void*>(nb),nobj.size());};
    auto excluded=[&](unsigned a){return (a>=0x956500&&a<0x9565ac)||(a>=0x95e028&&a<0x986b28)||a==0x986b28||(a>=0x9564e0&&a<0x9564ec);};
    NaviPubGlobals fresh;
    check(!std::memcmp(fresh.g688b00.data(),reinterpret_cast<void*>(0x688b00),0x800),"NaviPubGlobals 688B00 image");
    unsigned calls{},draws_total{},sounds_total{},faults{},digit_draws{},marker_draws{},sprite3d{};std::map<std::uint64_t,unsigned> histogram;std::map<std::uint64_t,unsigned> hist07;
    auto compare_all=[&](const char* label,const std::vector<RaceHudDraw>& nd,const std::vector<std::uint32_t>& ns,unsigned objects_calls,unsigned native_objects){
        bool ok=true;unsigned first=0;
        for(unsigned a=data_lo;a<data_hi;a+=4){if(excluded(a))continue;
            if(std::memcmp(ndata.data()+(a-data_lo),reinterpret_cast<void*>(a),4)){if(ok)first=a;ok=false;}}
        if(!ok&&mismatches<14)std::cerr<<label<<" .data first diff "<<std::hex<<first<<" guest "<<g32(first)<<" native "<<Bytes(ndata.data()+(first-data_lo),4).u32(0)<<std::dec<<char(10);
        check(ok,".data image");
        check(!std::memcmp(nobj.data(),reinterpret_cast<void*>(nb),0xe000),"object block (cars, camera, racers, records)");
        std::vector<RaceHudDraw> gd;
        for(unsigned i=0;i<g32(dtrace);++i){const unsigned r=dtrace+16+128*i;RaceHudDraw d;d.pc=g32(r);for(unsigned k=0;k<6;++k)d.args[k]=g32(r+4+4*k);
            if(d.pc==0x4289b0){std::memcpy(d.matrix.data(),reinterpret_cast<void*>(r+0x20),64);d.args[2]=0;}
            gd.push_back(d);}
        std::vector<RaceHudDraw> nn;
        for(auto d:nd)switch(d.pc){
            case 0x42cce0:for(unsigned k=1;k<d.args.size();++k)d.args[k]=0;nn.push_back(d);break;   // the resolved text stays native
            case 0x42d280:case 0x42d5f0:case 0x42ca60:case 0x42cc60:case 0x42ccb0:case 0x42cca0:case 0x462cd0:case 0x464d20:case 0x46ed20:case 0x42ccc0:case 0x4249f0:nn.push_back(d);break;
            default:break;}
        bool same=gd.size()==nn.size();
        for(std::size_t i=0;same&&i<gd.size();++i)same=gd[i].pc==nn[i].pc&&gd[i].args==nn[i].args&&(gd[i].pc!=0x4289b0||!std::memcmp(gd[i].matrix.data(),nn[i].matrix.data(),64));
        if(!same&&mismatches<14){std::cerr<<label<<" draws guest "<<gd.size()<<" native "<<nn.size()<<" var "<<g32(0x780258)<<std::hex<<" 7f8ef0 "<<unsigned(g8(0x7f8ef0))<<" 7f9224 "<<g32(0x7f9224)
            <<" magic "<<(g32(0x7f9224)?g32(g32(0x7f9224)+0x9cac):0u)<<" 7f9208 "<<g32(0x7f9208)<<" 844894 "<<g32(0x844894)<<" 81373c "<<g32(0x81373c)<<" 84481c "<<g32(0x84481c)<<" "<<g32(0x844820)<<" "<<g32(0x84482f)<<" "<<g32(0x844833)<<std::dec<<char(10);
            for(std::size_t i=0;i<std::max(gd.size(),nn.size())&&i<40;++i){
                if(i<gd.size()){std::cerr<<"  G "<<std::hex<<gd[i].pc;for(auto v:gd[i].args)std::cerr<<" "<<v;std::cerr<<std::dec<<char(10);}
                if(i<nn.size()){std::cerr<<"  N "<<std::hex<<nn[i].pc;for(auto v:nn[i].args)std::cerr<<" "<<v;std::cerr<<std::dec<<char(10);}}}
        check(same,"draw list");draws_total+=unsigned(gd.size());for(auto& d:gd)++histogram[(std::uint64_t(d.pc)<<32)|d.args[0]];
        if(g32(0x780258)==7u||g32(0x780258)==0u)for(auto& d:nd)++hist07[(std::uint64_t(g32(0x780258))<<56)|(std::uint64_t(d.pc)<<32)|d.args[0]];
        for(auto& d:nd){digit_draws+=d.pc==0x42cc00;marker_draws+=g32(0x780258)==2u&&d.pc==0x429580&&(d.args[0]>>16)==0x2bu;}
        std::vector<std::uint32_t> gs;for(unsigned i=0;i<g32(strace);++i)gs.push_back(g32(strace+16+4*i));
        if(gs!=ns&&mismatches<14){std::cerr<<label<<" var "<<g32(0x780258)<<" sounds guest";for(auto v:gs)std::cerr<<" "<<std::hex<<v;std::cerr<<" native";for(auto v:ns)std::cerr<<" "<<v;std::cerr<<std::dec<<char(10);}
        check(gs==ns,"ordered 424940 sounds");sounds_total+=unsigned(gs.size());
        compare_pool(label,true);
    };
    // Scripted services identical on both sides.
    std::uint32_t native_objects{};
    std::vector<SpraniDraw> scene_draws;SpraniGlobals scene_globals;std::array<float,3> scene_blend{};unsigned scene_total{};
    struct GroundScript{std::uint32_t* objects;float delta;} ground{&native_objects,1.5f};
    auto services=[&](PcRaceMemory& mem,std::vector<RaceHudDraw>& draws,std::vector<std::uint32_t>& sounds,PcMatrixStack* st){
        NaviPubServices s;s.m=&mem;s.sprites=&pool;s.pause_95b214=g32(0x95b214);s.matrices=st;s.draws=&draws;s.sounds=&sounds;
        s.scene_draws=&scene_draws;s.sprani=&scene_globals;s.blend_9564e0=&scene_blend;
        s.heart_user=&draws;s.heart=[](void* u,std::uint32_t pc){RaceHudDraw d;d.pc=pc;static_cast<std::vector<RaceHudDraw>*>(u)->push_back(d);return true;};
        ground.delta=*reinterpret_cast<float*>(counter+8);s.user=&ground;
        s.ground_43eb60=[](void* u,std::uint32_t,outrun::driving::CourseProbe& p,std::uint32_t& flags){
            p.y=outrun::driving::x87_float(outrun::driving::X87(p.y)-outrun::driving::X87(static_cast<GroundScript*>(u)->delta));
            std::uint32_t xb;std::memcpy(&xb,&p.x,4);flags=xb&0x00100002u;return true;};
        return s;};
    auto run_both=[&](unsigned pc,const char* label,auto&& native,auto&& args){
        std::memset(reinterpret_cast<void*>(dtrace),0,0x40000);std::memset(reinterpret_cast<void*>(strace),0,0x1000);p32(counter,0);native_objects=0;
        p32(trace,0);p32(0x986b28,0x55);
        reset_matrix_oracle_globals();
        to_native();
        std::vector<std::uint8_t> arena(0x10000);std::memcpy(arena.data(),reinterpret_cast<void*>(MatrixArena),arena.size());
        PcMatrixStack stack{Bytes(arena.data(),arena.size()),0,0,64};
        prepare(pc);args();run();
        PcRaceMemory mem;mem.map(data_lo,ndata.data(),ndata.size());mem.map(nb,nobj.data(),nobj.size());
        mem.map_const(qa,reinterpret_cast<const std::uint8_t*>(qa),0x13000);mem.map_const(qb,reinterpret_cast<const std::uint8_t*>(qb),0x210000);
        std::vector<RaceHudDraw> draws;std::vector<std::uint32_t> sounds;
        auto s=services(mem,draws,sounds,&stack);
        scene_draws.clear();scene_globals.current_7551b4=std::int32_t(Bytes(ndata.data()+(0x7551b4-data_lo),4).u32(0));scene_globals.flag_986b28=0x55;
        {float v;std::memcpy(&v,reinterpret_cast<void*>(0x9564e0),4);scene_blend[0]=v;std::memcpy(&v,reinterpret_cast<void*>(0x9564e4),4);scene_blend[1]=v;
         std::memcpy(&v,reinterpret_cast<void*>(0x9564e8),4);scene_blend[2]=v;}
        const bool ok=native(s);++calls;
        if(!ok){++faults;if(mismatches<14)std::cerr<<label<<" native missing "<<std::hex<<s.missing<<" fault "<<s.fault<<std::dec<<char(10);}
        check(ok,label);
        if(inject&&!injected&&!scene_draws.empty()&&calls>40){scene_draws[0].frame+=1.0f;injected=true;}
        compare_all(label,draws,sounds,g32(counter),native_objects);
        {   // 428A10 -> 429460 submissions: root, frame, scale, layer, 7551B4 and the matrix top.
            bool same=g32(trace)==scene_draws.size();
            for(std::size_t i=0;same&&i<scene_draws.size();++i){
                const unsigned r=trace+16+128*unsigned(i);const auto& d=scene_draws[i];
                const unsigned bank=d.token>>16,index=d.token&0xffff;const unsigned scene=g32(g32(0x9568b8+bank*4)+index*4);
                const unsigned root=g32(scene+4)+(g32(scene)-1)*0x24;
                same=g32(r+4)==root&&g32(r+8)==bits(d.frame)&&g32(r+12)==bits(d.scale)&&g32(r+16)==d.layer&&std::int32_t(g32(r+20))==d.id_7551b4&&
                     g32(r+24)==(d.depth==1u?1u:0u)&&g32(r+28)==(d.depth==2u?1u:0u);
                sprite3d+=d.depth!=0u;
                for(unsigned k=0;same&&k<16;++k)same=g32(r+0x40+k*4)==bits(d.matrix[k]);
                if(!same&&mismatches<14){std::cerr<<label<<" scene draw "<<i<<" token "<<std::hex<<d.token<<" root "<<g32(r+4)<<"/"<<root<<" layer "<<g32(r+16)<<"/"<<d.layer<<" id "<<g32(r+20)<<"/"<<d.id_7551b4<<" frame "<<g32(r+8)<<"/"<<bits(d.frame)<<" scale "<<g32(r+12)<<"/"<<bits(d.scale);
                    for(unsigned k=0;k<16;++k)std::cerr<<" "<<g32(r+0x40+k*4)<<"/"<<bits(d.matrix[k]);std::cerr<<std::dec<<char(10);}
            }
            if(g32(trace)!=scene_draws.size()&&mismatches<14)std::cerr<<label<<" scene draws guest "<<g32(trace)<<" native "<<scene_draws.size()<<char(10);
            if(!same&&mismatches<14&&std::getenv("OR2_NAVI_SCENES")){
                for(unsigned i=0;i<std::max<unsigned>(g32(trace),unsigned(scene_draws.size()))&&i<60;++i){
                    std::cerr<<"  "<<i<<std::hex;
                    if(i<g32(trace))std::cerr<<" G root "<<g32(trace+16+128*i+4)<<" f "<<g32(trace+16+128*i+8)<<" x "<<g32(trace+16+128*i+0x70);
                    if(i<scene_draws.size())std::cerr<<" | N "<<scene_draws[i].token<<" f "<<bits(scene_draws[i].frame)<<" x "<<bits(scene_draws[i].matrix[12]);
                    std::cerr<<std::dec<<char(10);}}
            check(same,"428A10 -> 429460 scene draws");scene_total+=unsigned(scene_draws.size());
            check(g32(0x986b28)==scene_globals.flag_986b28&&g32(0x9564e0)==bits(scene_blend[0])&&g32(0x9564e4)==bits(scene_blend[1])&&
                  g32(0x9564e8)==bits(scene_blend[2]),"986B28 / 9564E0..E8");
        }
        check(!std::memcmp(arena.data(),reinterpret_cast<void*>(MatrixArena),64*4)&&g32(0x89b568)==std::uint32_t(stack.depth),"matrix stack");
    };
    // 4BA9D0 directly: every style (image and sprite digits), text-cursor chars.
    for(unsigned i=0;i<3000;++i){
        const unsigned style=rng()%15;const std::int32_t x=int(rng()%640)-50,y=int(rng()%480)-50;const unsigned layer=rng()%12;const float alpha=rf(0.0f,1.0f);
        std::string text;const unsigned len=1+rng()%9;const char set[]="0123456789 -:.";for(unsigned k=0;k<len;++k)text.push_back(set[rng()%14]);
        std::memcpy(reinterpret_cast<void*>(misc_at+0x100),text.c_str(),text.size()+1);p32(0x7551b4,rng());
        run_both(0x4ba9d0,"4BA9D0 digits",[&](NaviPubServices& s){return navi_digits_4ba9d0(s,style,x,y,text,layer,alpha);},
            [&]{auto* a=reinterpret_cast<unsigned*>(S);a[0]=style;a[1]=unsigned(x);a[2]=unsigned(y);a[3]=misc_at+0x100;a[4]=layer;std::memcpy(a+5,&alpha,4);});
    }
    // World set-up.
    std::vector<unsigned> cars;for(unsigned i=0;i<7;++i)cars.push_back(cars_at+i*0x1000/1);
    for(unsigned scenario=0;scenario<60;++scenario){
        pool.reset();std::memset(reinterpret_cast<void*>(pool_base),0,0x7c*FrontendSprites::Count);std::memset(reinterpret_cast<void*>(0x956500),0,0xac);
        p32(0x95b214,0);
        for(unsigned i=0;i<0xe000;++i)p8(nb+i,std::uint8_t(rng()));
        const unsigned ncars=7;
        for(unsigned i=0;i<ncars;++i){
            const unsigned c=cars_at+i*0x1000;
            p32(0x799b38+(8+i)*0x3c,c);p32(c,8+i);
            p8(c+0xc36,std::uint8_t(rng()%7));p16(c+0x260,std::uint16_t(rng()%3000));p32(c+0x5c,rng()%3);p16(c+0x64,std::uint16_t(rng()%200));
            p8(c+0x13,std::uint8_t(rng()%3?1:0));p8(c+6,std::uint8_t(rng()%4==0));p8(c+0x244,std::uint8_t(rng()%4?0:rng()));p8(c+0xda4,std::uint8_t(int(rng()%5)-2));
            pf(c+0x1f8,rf(0.0f,300.0f));pf(c+0x1c4,rf(0.0f,2.0f));pf(c+0xe64,rf(0.0f,2.0f));pf(c+0xe68,rf(0.0f,0.4f));p16(c+0x162,std::uint16_t(std::int16_t(int(rng()%4000)-2000)));
            p32(c+0x208,rng()%9);p32(c+0x20c,rng()%12000);p32(c+0x48,rng()%12000);p32(c+0xe80,rng()%2);p8(c+0x11,std::uint8_t(rng()%8));p8(c+0x12,std::uint8_t(rng()));
            p8(c+0x10,std::uint8_t(rng()%8));p32(c+0x68,0x100+rng()%4);p32(c+8,rng()%4==0?0x20000000u:0u);
            p32(c+0x2b4,spec_at+(i%2)*0x1400/2);
            for(unsigned k=0;k<16;++k)pf(c+0xb0+k*4,k==15?1.0f:(k%4==3?0.0f:(k>=12?rf(-300.0f,300.0f):rf(-1.2f,1.2f))));
        }
        p32(spec_at+0x10a0,rng()%7);p32(spec_at+0xa00+0x10a0,rng()%7);
        for(unsigned k=0;k<16;++k)pf(camera_at+0x140+k*4,k==15?1.0f:(k%4==3?0.0f:(k>=12?rf(-50.0f,50.0f):rf(-1.2f,1.2f))));
        pf(camera_at+0xb0,rf(200.0f,400.0f));pf(camera_at+0xb4,rf(-300.0f,300.0f));p32(0x79f574,camera_at);
        for(unsigned k=0;k<3;++k)pf(camera_at+0xd4+k*4,rf(-400.0f,400.0f));
        for(unsigned i=0;i<7;++i)for(unsigned k=0;k<3;++k)pf(cars_at+i*0x1000+0x14+k*4,rf(-400.0f,400.0f));
        for(unsigned i=0;i<9;++i)p8(0x79fb48+i,std::uint8_t(rng()));
        for(unsigned i=0x20;i<0x134;++i)p8(0x79fb48+i,std::uint8_t(rng()%4?0:(rng()%2?1:3)));   // 45DB70 runs live: no OSO object is live (flags != 2)
        for(unsigned i=0;i<0x100;++i)p8(0x7df118+i,std::uint8_t(rng()%8));      // 45A2B0 rank table
        {const float k=rng()%3?1.5f:-1.5f;std::memcpy(reinterpret_cast<void*>(counter+8),&k,4);}
        for(unsigned i=0;i<7;++i)p8(0x79fb51+i,std::uint8_t(rng()%3?2:rng()));
        p32(0x680ad4,rng()%4?7:rng()%8);
        // Racers and ranks.
        const unsigned nrac=1+rng()%6;p32(0x80fb00,racers_at);p32(0x80fb04,nrac);p32(0x80fb2c,1+rng()%6);p32(0x80fb1c,rank_at);
        for(unsigned i=0;i<8;++i)p32(rank_at+i*20+0x10,rng()%4==0?0xffffffffu:rng()%nrac);
        p32(0x64e190,rng()%2?0xffffffffu:rng()%nrac);p32(0x80fb0c,rng()%3?fb0c_at:0);p32(fb0c_at+0x18,rng()%4);
        p16(0x804388,std::uint16_t(1+rng()%5));p32(0x803710,rng()%8==0?0:1000+rng()%20000);
        // Course records.
        const unsigned nrec=1+rng()%4;p32(0x7d33bc,records_at);p32(0x7d33c4,nrec);
        for(unsigned i=0;i<nrec;++i){const unsigned r=records_at+i*0x78;p32(r+4,0x100+i);p32(r+8,rng()%6);p32(r+0x14,misc_at+i*4);p32(misc_at+i*4,rng()%0x38);p32(r+0x74,rng()%3);}
        p32(0x7d3188,records_at+(rng()%nrec)*0x78);p32(0x7d2df4,misc_at+0x40);p32(misc_at+0x40,rng()%0x38);
        p32(0x635f2c,rng()%3?0xffffffffu:0x100);p32(0x635f30,rng()%5);
        p32(0x7d33c0,rng()%16);
        // Mission.
        const unsigned types[]{0,1,2,3,6};p32(0x83637c,mission_at);p32(mission_at+0x20,types[rng()%5]);p32(mission_at+0x38,rng()%400);p32(mission_at+0x3c,400+rng()%400);
        p8(0x836374,std::uint8_t(rng()%5!=0));p32(0x836388,rng()%3000);
        const unsigned pick=rng()%5;const bool heart=pick==0,outrun=pick==1,timeattack=pick==2;
        p32(0x780258,heart?2u:outrun?1u:timeattack?(rng()%2?7u:0u):(rng()%6?6u:9u));
        if(timeattack){   // variants 0 / 7: sectors 4BA3A0, banners 4BE020, stages 4BE4B0, goal ahead 4BB970
            const bool rec=rng()%2;p32(0x81373c,rec?qt1:0u);p32(0x813740,rec?qt2:0u);
            std::memset(reinterpret_cast<void*>(qa),0,0x12000);std::memset(reinterpret_cast<void*>(qb),0,0x210000);
            for(unsigned l=0;l<16;++l)p8(qt1+l*0xfd4+0x2f,std::uint8_t(rng()%4));
            auto qhot=[&](unsigned idx){const unsigned r=qt2+idx*0xfd4;if(rng()%4==0)return;
                p32(r,0x544f4851u);p16(r+0x24,std::uint16_t(rng()%6?0x132:0x131));for(unsigned k=0;k<4;++k)p32(r+0x14+k*4,rng()%5?rng()%300000:rng()%4000000);
                p8(r+0x28,std::uint8_t(rng()%5?rng()%4:0xff));p8(r+0x31,std::uint8_t(rng()%4));
                for(unsigned k=0;k<5;++k)p8(r+0x2b+k,std::uint8_t(rng()%4?0x41+rng()%26:rng()));if(rng()%4==0)p8(r+0x2b,0);};
            for(unsigned idx=0;idx<8;++idx)qhot(idx);for(unsigned idx=0x200;idx<0x208;++idx)qhot(idx);
            p32(0x7f9224,ghost_base);p32(ghost_at+0x100,rng()%3?0x544f484eu:0u);
            p8(ghost_at+0x100+0x11b,std::uint8_t(rng()%5?rng()%4:0xff));p8(ghost_at+0x100+0x11c,std::uint8_t(rng()%4));
            for(unsigned k=0;k<5;++k)p8(ghost_at+0x100+0x104+k,std::uint8_t(rng()%4?0x41+rng()%26:rng()));
            p8(0x7f8ef0,std::uint8_t(rng()%2));p8(0x7f91d0,std::uint8_t(rng()%5?rng()%4:0xff));p8(0x7f9204,std::uint8_t(rng()%4));
            for(unsigned k=0;k<5;++k)p8(0x7f9208+k,std::uint8_t(rng()%4?0x61+rng()%26:rng()));
            for(unsigned k=0;k<4;++k)p32(0x7f91d4+k*4,rng()%4?rng()%300000:0u);
            for(unsigned a=0x7f8f00;a<0x7f9100;a+=4)p32(a,rng()%4?rng()%300000:0u);
            for(unsigned i=0;i<nrec;++i){p32(records_at+i*0x78+0x2c,rng()%2?0xffffffffu:rng()%3);p32(records_at+i*0x78+0x30,rng()%2?0xffffffffu:rng()%3);}
            for(unsigned k=0;k<0x38;++k)p32(0x6a54e0+k*4,misc_at+0x500+k*2);   // 44C990 stage records (+7E length)
            for(unsigned k=0;k<0x38;++k)p16(misc_at+0x57eu+k*2,std::uint16_t(rng()%250));
            p32(0x842a98,0xffffffffu);p8(0x83036d,std::uint8_t(rng()%5?rng()%3:rng()));p8(0x8361b4,std::uint8_t(rng()%2));
            for(unsigned k=0;k<16;++k)p32(0x7d39a0+k*4,rng()%3);
            for(unsigned k=0;k<0x10;++k){p32(0x844848+k*4,rng()%3?0u:rng()%300000);}
        }
        if(outrun){   // variant 1: rival kind 5 traffic (4BB640), the final stretch sprite (4BB890), split time (4B9320)
            for(unsigned id=15;id<0x20;++id)p8(0x79fb48+id,0);
            for(unsigned i=0;i<ncars;++i){const unsigned c=cars_at+i*0x1000;p32(c+0x324,rng()%2?5u:rng()%7);p32(c+0xc5c,rng()%4?0u:0x800000u);pf(c+0x102c,rf(0.6f,0.8f));}
            p32(cars_at+0x68,rng()%3);p32(0x8446ec,rng()%4?1u:0u);
            p32(0x80fb14,rng()%2);p32(0x8448a8,rng()%3?0xffffffffu:rng()%64);
            p32(0x689210,rng()%60);p32(0x68920c,rng()%30);p8(0x8448a5,std::uint8_t(rng()%2));
        }
        if(heart){
            for(unsigned i=0;i<nrec;++i)p32(records_at+i*0x78+8,rng()%5);   // levels 0..4 (4BB9E0 rows)
            p32(0x635f30,rng()%5);
            for(unsigned i=0;i<ncars;++i){const unsigned c=cars_at+i*0x1000;p16(c+0x25e,std::uint16_t(rng()%3));p32(c+4,rng()%3?0u:rng());}
            p32(0x688b34,rng()%3?0xffffffffu:rng()%64);p32(0x688b48,rng()%480);
        }p32(0x78024c,rng()%5);p32(0x78026c,16);p8(0x780248,0);p32(0x780278,1);
        p32(0x7c24bc,rng()%2);p32(0x7d2698,rng()%5);p8(0x7de784,0);p8(0x830394,std::uint8_t(rng()%2));p32(0x656234,rng()%100);
        p32(0x95af0c,rng());p32(0x7551b4,rng());p32(0x688b3c,rng()%4?0u:1u);p32(0x689200,rng()%2);p8(0x6891fe,std::uint8_t(rng()%2));pf(0x844924,rf(0.0f,1.0f));
        // Heart-Attack quest table entries before 45E0F0 resets them.
        for(unsigned e=0;e<15;++e)p8(0x7f2548+e*0x44,std::uint8_t(rng()%3==0?0xff:rng()%4));
        for(unsigned a=0x7f2580;a<0x7f2660;++a)p8(a,std::uint8_t(rng()%4==0?rng()%3:0));
        if(std::getenv("OR2_NAVI_DEBUG")){
            {std::vector<std::uint8_t> b(0x60);for(std::size_t i=0;i<b.size();++i)b[i]=file[pe_file_offset(file,0x45a2b0u+std::uint32_t(i))];
             std::cerr<<"45a2b0 bytes same as file: "<<!std::memcmp(b.data(),reinterpret_cast<void*>(0x45a2b0),0x60)<<" 1039ccc="<<std::hex<<g32(0x1039ccc)<<std::dec<<char(10);}
            for(unsigned id:{0u,1u,5u,7u}){prepare(0x45a2b0);reinterpret_cast<unsigned*>(S)[0]=id;run();std::cerr<<"45a2b0("<<id<<")="<<std::hex<<guest_call.out_eax<<" table "<<unsigned(g8(0x7df118+id))<<std::dec<<char(10);}
            const unsigned stage=g32(g32(0x799d18)+0x68);std::vector<std::uint8_t> keep(0x1000);std::memcpy(keep.data(),reinterpret_cast<void*>(0x635000),0x1000);
            prepare(0x44dc50);reinterpret_cast<unsigned*>(S)[0]=stage;run();const auto u=guest_call.out_eax;
            prepare(0x44c940);reinterpret_cast<unsigned*>(S)[0]=stage;run();const auto l=guest_call.out_eax;
            std::memcpy(reinterpret_cast<void*>(0x635000),keep.data(),0x1000);
            std::cerr<<"guest stage "<<std::hex<<stage<<" 44dc50 "<<u<<" level "<<l<<" entry "<<unsigned(g8(0x7f2548+l*0x44))<<std::dec<<char(10);}
        // 4295D0 directly: 3D sprites (Heart Attack car hearts) over the camera projection.
        pf(camera_at+0xbc,rf(-3.0f,3.0f));
        for(unsigned k=0;k<16;++k)pf(0x95d8a0+k*4,k%5==0?rf(0.5f,2.0f):rf(-0.5f,0.5f));
        for(unsigned i=0;i<20;++i){
            const unsigned token=0x2c0000u+rng()%0x120u;
            std::array<float,3> pos{};const unsigned src=cars_at+(rng()%7)*0x1000+0x14;
            for(unsigned k=0;k<3;++k){pos[k]=rng()%4?*reinterpret_cast<float*>(src+k*4)+rf(-3.0f,3.0f):rf(-400.0f,400.0f);pf(misc_at+0x200+k*4,pos[k]);}
            const std::int32_t mode=rng()%3?-1:int(rng()%3);const std::int32_t frame=int(rng()%40);
            const float scale=rf(0.2f,3.0f),zbias=rng()%2?-0.02f:rf(-1.0f,1.0f),angle=rng()%3?0.0f:rf(-3.0f,3.0f),minimum=rng()%2?0.0f:rf(0.0f,2.0f);
            const unsigned depth=rng()%3?0u:1u+rng()%3;p32(0x7551b4,rng()%3?0xffffffffu:rng());
            run_both(0x4295d0,"4295D0 3D sprite",[&](NaviPubServices& s){return navi_sprite3d_4295d0(s,token,pos,mode,frame,scale,zbias,angle,depth,minimum);},
                [&]{auto* a=reinterpret_cast<unsigned*>(S);a[0]=token;a[1]=misc_at+0x200;a[2]=unsigned(mode);a[3]=unsigned(frame);std::memcpy(a+4,&scale,4);
                    std::memcpy(a+5,&zbias,4);std::memcpy(a+6,&angle,4);a[7]=depth;std::memcpy(a+8,&minimum,4);});
        }
        // 428AC0 directly (course object hoops): 428AF0 with a given matrix.
        for(unsigned i=0;i<10;++i){
            const unsigned token=0x2c0000u+rng()%0x120u;std::array<float,16> mt{};
            for(unsigned k=0;k<16;++k){mt[k]=k%5==0?rf(0.5f,2.0f):(k>=12&&k<15?rf(-300.0f,300.0f):rf(-0.5f,0.5f));pf(misc_at+0x300+k*4,mt[k]);}
            const std::int32_t frame=int(rng()%40);const float sc=rf(0.2f,3.0f),angle=rng()%3?0.0f:rf(-3.0f,3.0f);const unsigned mode=rng()%2;
            p32(0x7551b4,rng()%3?0xffffffffu:rng());
            run_both(0x428ac0,"428AC0 3D sprite",[&](NaviPubServices& s){return navi_sprite_428ac0(s,token,mt,frame,sc,angle,mode);},
                [&]{auto* a=reinterpret_cast<unsigned*>(S);a[0]=token;a[1]=misc_at+0x300;a[2]=unsigned(frame);std::memcpy(a+3,&sc,4);std::memcpy(a+4,&angle,4);a[5]=mode;});
        }
        p32(0x78026c,rng()%2?16u:15u);
        run_both(0x4bc9e0,"4BC9E0 init",[&](NaviPubServices& s){return navi_pub_init_4bc9e0(s);},[]{});
        for(unsigned frame=0;frame<24;++frame){
            // Inputs that change between frames.
            for(unsigned i=0;i<ncars;++i){const unsigned c=cars_at+i*0x1000;if(rng()%3==0){p8(c+0xc36,std::uint8_t(rng()%7));p16(c+0x260,std::uint16_t(g32(c+0x260)+rng()%40));
                pf(c+0x1f8,rf(0.0f,300.0f));pf(c+0xe68,rf(0.0f,0.4f));p32(c+0x208,rng()%9);p32(c+0x20c,rng()%12000);p32(c+0x68,0x100+rng()%4);}}
            p32(0x7d394c,rng()%4==0?rng()%700:g32(0x7d394c));p32(0x7d38f0,rng()%10==0);p32(0x7d39f0,rng());
            if(rng()%6==0)p32(0x78026c,rng()%3?15:13);else p32(0x78026c,16);
            if(rng()%8==0)p8(0x84490c,1);if(rng()%10==0)p16(0x8447f4,std::uint16_t(rng()%3?0xb4:rng()%200));
            if(rng()%8==0)p16(0x844900,std::uint16_t(rng()%4));if(rng()%8==0)p16(0x844708,std::uint16_t(rng()%3?0x78:rng()%200));
            if(rng()%10==0)p16(0x8446e8,std::uint16_t(rng()%3));
            p32(0x836388,g32(0x836388)+rng()%20);if(rng()%10==0)p32(0x836388,9999990+rng()%20);
            if(g32(0x780258)==1u){p32(0x7d387c,rng()%4?rng()%700000:rng());p32(0x780278,1+rng()%3);
                if(rng()%3==0){const unsigned k=rng()%10,r=0x8444c8+k*0x24;   // a score message (4B9770's record)
                    p32(r+4,rng()%7);p32(r+8,rng()%20000);{const unsigned ages[]{5,0xf,0x19,0,6};p32(r+0xc,rng()%2?ages[rng()%5]:rng()%0x8d);}p32(r+0x10,rng()%300);pf(r+0x14,rf(0.0f,1.0f));
                    const unsigned styles[]{1,0xc,0xd,0xe,0xf,0x11};p32(r+0x18,styles[rng()%6]);p32(r+0x1c,rng()%5);p32(r+0x20,rng()%3);}
                p32(0x7f8b60,0);pf(0x8446c4,rf(0.0f,30.0f));p32(0x8446c0,rng()%2?g32(cars_at+0x260)&0xffffu:rng()%3000);
                for(unsigned i=0;i<ncars;++i)pf(cars_at+i*0x1000+0x102c,rf(0.6f,0.8f));}
            if(g32(0x780258)==2u){
                auto sv=[&]{return rng()%3?std::int32_t(rng()%400)-200:std::int32_t(rng()%20000)-10000;};   // the PC formats them into small stack buffers
                p32(0x7f2428,rng()%3?0xeu:(rng()%25+10u)%26u);   // never 9 (46ED20, the 3D pass, is not ported)
                for(unsigned k=0;k<3;++k){p32(0x7f8b00+k*8,rng()%4?cars_at+(1+rng()%6)*0x1000:0u);p32(0x7f8b04+k*8,rng()%10);}
                p32(0x7f1cac,rng()%2?0xffffffffu:rng()%3);p32(0x7f1c80,unsigned(sv())*(rng()%3?1000u:1u));
                for(unsigned k=0;k<7;++k){p32(0x7f2438+k*4,unsigned(sv()));p32(0x7f244c+k*4,unsigned(sv()));}
                p32(0x7f2464,unsigned(sv()));for(unsigned k=0;k<6;++k)p32(0x7f2538+k*0x44,rng()%3);
                p32(0x7d3994,rng()%2);p32(0x7d3998,rng()%2);p32(0x7d39f4,rng()%2);p32(0x7d393c,rng()%4?rng()%600000:rng());p32(0x7d39dc,rng()%4?rng()%40000:rng());
                p32(0x7d394c,rng()%8000);   // 4BD9B0 formats frames / 60.2 into an 8-byte buffer
                for(unsigned k=0;k<30;++k)pf(0x8448b0+k*4,rf(0.0f,30.0f));
                p16(0x8446e8,std::uint16_t(rng()%2));
            }
            if(timeattack){
                p32(0x7d3934,rng()%3==0?0x12cu:(rng()%3?rng()%400:0u));p32(0x7d3874,rng()%4);p32(0x7d3944,rng()%4);
                for(unsigned k=0;k<16;++k){p32(0x7d3650+k*4,rng()%4?rng()%600000:rng()%4000000);p32(0x7d3748+k*4,rng()%600000);}
                for(unsigned k=0;k<15;++k)p32(0x7d3954+k*4,rng()%4?rng()%600000:rng()%4000000);
                p32(0x7d3930,rng()%4?rng()%40000:rng()%400000);
                p8(0x8448a6,std::uint8_t(rng()%2));p8(0x81044c,std::uint8_t(rng()%3==0?1:rng()%3));p32(0x810438,cars_at+(rng()%7)*0x1000);pf(0x8448ac,rf(0.0f,1.0f));
                for(unsigned i=0;i<ncars;++i){const unsigned c=cars_at+i*0x1000;p32(c+0xc5c,rng()%2?0x2000u:0u);pf(c+0xb68,rf(0.0f,1.0f));}
                for(unsigned k=0;k<8;++k)p8(0x844895+k,std::uint8_t(rng()%2));
                if(rng()%3==0)p8(0x844894,std::uint8_t(rng()%8));
                if(rng()%4==0)p16(0x8446e8,std::uint16_t(0xd0+rng()%8));p32(0x780278,1+rng()%3);p32(0x810124,rng()%2);
                p32(0x7d387c,rng()%4?rng()%700000:rng());
            }
            run_both(0x4bcc40,"4BCC40 control",[&](NaviPubServices& s){return navi_pub_control_4bcc40(s);},[]{});
            run_both(0x4beb00,"4BEB00 display",[&](NaviPubServices& s){return navi_pub_display_4beb00(s);},[]{});
            if(rng()%5==0)pool.tick(sprani_clock_427f70(1,0,1,0)),(prepare(0x427f70),p32(0x740ca4,1),run());
        }
        run_both(0x4b8df0,"4B8DF0 destroy",[&](NaviPubServices& s){return navi_pub_destroy_4b8df0(s);},[]{});
    }
    std::cerr<<"scene draws "<<scene_total<<" ";
    std::cerr<<"labels "<<histogram[(std::uint64_t(0x42d280)<<32)|0x2b0095]+histogram[(std::uint64_t(0x429580)<<32)|0x2c00cb]<<" ";
    std::cerr<<"navi calls "<<calls<<" draws "<<draws_total<<" sounds "<<sounds_total<<" native faults "<<faults<<" text-pos "<<digit_draws<<" heart markers "<<marker_draws<<" 3D sprites "<<sprite3d<<char(10);
    coverage=draws_total;
    if(std::getenv("OR2_NAVI_HIST7"))for(auto& [k,v]:hist07)if(((k>>32)&0xffffffu)!=0x42ccc0u&&((k>>32)&0xffffffu)!=0x42cc00u)std::cerr<<"v"<<(k>>56)<<" "<<std::hex<<((k>>32)&0xffffffu)<<" "<<(k&0xffffffffu)<<std::dec<<" "<<v<<char(10);
    if(std::getenv("OR2_NAVI_HIST"))for(auto& [k,v]:histogram)std::cerr<<std::hex<<(k>>32)<<" "<<(k&0xffffffffu)<<std::dec<<" "<<v<<char(10);
}

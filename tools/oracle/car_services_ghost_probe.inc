if(only=="car_services_ghost_probe"){
    // Ghost recording: 47ED90 reset, 47F330 record writer, 466E50 packet
    // serializer with its codecs 466A20/466AF0(protected bridge)/466030/
    // 4667D0/4668F0/466720. Whole original functions execute; nothing inside
    // them is stubbed. The pinned d3dx9_29 generic routines back 40A240
    // (inverse), 40A7D0 (transform coord), 40A220 and the rotations.
    using namespace outrun::driving;
    if(argc<=7||std::string(argv[7])=="-")throw std::runtime_error("car_services_ghost_probe requires pinned d3dx9_29.dll");
    constexpr unsigned base=0x3a900000;
    constexpr unsigned stubs=base,capture=base+0x100,records=base+0x1000,car=base+0x11000,player=base+0x12000,
                       frame_a=base+0x13000,frame_b=base+0x13100,frame_c=base+0x13200,packet=base+0x14000,pbuf=base+0x20000;
    constexpr std::size_t RecordsBytes=15u*0xfd4u+0x40u,PacketBytes=0x9400u,ArenaBytes=0x1100u;
    map_at(base,0x30000,PROT_READ|PROT_WRITE|PROT_EXEC);map_original_arithmetic_bridge(file);
    init_matrix_oracle_globals();wire_d3dx_imports(map_d3dx29(argv[7]));
    {   static const std::uint8_t finite_stub[]{0x8b,0x44,0x24,0x08,0x25,0x00,0x00,0xf0,0x7f,0x3d,0x00,0x00,0xf0,0x7f,0x0f,0x95,0xc0,0x0f,0xb6,0xc0,0xc3};
        std::memcpy(reinterpret_cast<void*>(stubs),finite_stub,sizeof(finite_stub));
        auto* iat=reinterpret_cast<void*>(D3DX_BASE+0x1000u);
        if(mprotect(iat,4096,PROT_READ|PROT_WRITE))throw std::runtime_error("d3dx IAT page");
        *reinterpret_cast<std::uint32_t*>(D3DX_BASE+0x1270u)=stubs;
        if(mprotect(iat,4096,PROT_READ))throw std::runtime_error("d3dx IAT restore");
        auto* game_iat=reinterpret_cast<void*>(0x596000u);
        if(mprotect(game_iat,4096,PROT_READ|PROT_WRITE))throw std::runtime_error("game IAT");
        *reinterpret_cast<std::uint32_t*>(0x5962dcu)=D3DX_BASE+(0x440e9cu-0x400000u); // D3DXMatrixInverse
        if(mprotect(game_iat,4096,PROT_READ))throw std::runtime_error("game IAT restore");
    }
    for(std::uint32_t page=0x62f000u;page<0x98d000u;page+=0x1000u)
        if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("ghost .data pages");
    // Startup harness setup may have patched code these routines execute:
    // restore every byte of the executed ranges from the pinned file.
    unsigned restored=0;
    {   const std::pair<std::uint32_t,std::uint32_t> ranges[]{
            {0x47ed90u,0x47ee60u},{0x47f330u,0x47f778u},{0x466030u,0x46611au},{0x466720u,0x466bd8u},{0x466e50u,0x46718fu},
            {0x449330u,0x4497a0u},{0x450780u,0x450790u},{0x43f960u,0x43f978u},{0x48b1a0u,0x48b1a6u},{0x48b350u,0x48b35fu},
            {0x44bea0u,0x44bee3u},{0x409ef0u,0x40a2e0u},{0x40a3e0u,0x40a480u},{0x40a7d0u,0x40a7e6u},{0x40eeb0u,0x40ef00u},
            {0x439388u,0x4393acu},{0x448449u,0x448455u}};
        for(const auto& r:ranges)for(std::uint32_t a=r.first;a<r.second;++a){
            const auto want=file[pe_file_offset(file,a)];auto* q=reinterpret_cast<std::uint8_t*>(a);
            if(*q!=want){auto* page=reinterpret_cast<void*>(a&~0xfffu);mprotect(page,4096,PROT_READ|PROT_WRITE|PROT_EXEC);*q=want;
                mprotect(page,4096,PROT_READ|PROT_EXEC);++restored;}
        }
    }
    const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
    WorldNativeControl control(static_cast<unsigned short>(x87_control));
    std::map<std::string,unsigned> cov;
    unsigned cases{},mismatches{};std::map<std::string,std::array<unsigned,2>> per;
    auto compare=[&](const void* native,unsigned address,std::size_t n,const char* what,const char* section){
        ++cases;++per[section][0];if(!std::memcmp(native,reinterpret_cast<void*>(address),n))return;
        ++mismatches;++per[section][1];if(mismatches<12){const auto* a=static_cast<const std::uint8_t*>(native);
            for(std::size_t i=0;i<n;++i)if(a[i]!=reinterpret_cast<std::uint8_t*>(address)[i]){
                unsigned nw,pw;std::memcpy(&nw,a+(i&~3u),4);std::memcpy(&pw,reinterpret_cast<void*>(address+(i&~3u)),4);
                std::cerr<<section<<" "<<what<<" case="<<cases<<" offset="<<std::hex<<i<<" PC="<<pw<<" native="<<nw<<std::dec<<"\n";break;}}
    };
    std::mt19937 rng(0x47f330u);
    auto real=[&](float lo,float hi){return lo+(hi-lo)*(float(rng()&0xffffffu)/float(0xffffffu));};
    auto fill=[&](std::uint32_t a,std::size_t n){for(std::size_t k=0;k<n;++k)reinterpret_cast<std::uint8_t*>(a)[k]=std::uint8_t(rng());};
    auto g32=[](std::uint32_t a)->std::uint32_t&{return *reinterpret_cast<std::uint32_t*>(a);};
    auto gf=[](std::uint32_t a,float v){std::memcpy(reinterpret_cast<void*>(a),&v,4);};
    auto odd_float=[&]()->float{
        switch(rng()%12u){
        case 0:return 0.f;case 1:return -0.f;case 2:return std::numeric_limits<float>::quiet_NaN();
        case 3:return std::numeric_limits<float>::infinity();case 4:return 3e9f;case 5:return 1e-40f;
        default:{std::uint32_t u=rng();float f;std::memcpy(&f,&u,4);return f;}}
    };
    // Random rotation + translation (optionally singular / odd) area matrix.
    auto area_matrix=[&](std::uint32_t a,unsigned kind){
        float m[16]{};
        const float y=real(-3.2f,3.2f),x=real(-0.5f,0.5f);
        const float cy=std::cos(y),sy=std::sin(y),cx=std::cos(x),sx=std::sin(x);
        m[0]=cy;m[2]=-sy;m[4]=sy*sx;m[5]=cx;m[6]=cy*sx;m[8]=sy*cx;m[9]=-sx;m[10]=cy*cx;m[15]=1;
        m[12]=real(-3000,3000);m[13]=real(-200,200);m[14]=real(-3000,3000);
        if(kind==1)for(auto& v:m)v=real(-3,3);
        if(kind==2)for(auto& v:m)v=0;                 // singular: inverse leaves current untouched
        if(kind==3)for(unsigned k=0;k<4;++k)m[4+k]=m[k];
        std::memcpy(reinterpret_cast<void*>(a),m,64);
    };
    auto setup_arena=[&](std::vector<std::uint8_t>& arena,std::int32_t depth){
        reset_matrix_oracle_globals();arena.assign(ArenaBytes,0);
        for(auto& b:arena)b=std::uint8_t(rng());
        for(unsigned k=0;k<ArenaBytes;k+=4){float v=real(-2,2);std::memcpy(arena.data()+k,&v,4);}
        std::memcpy(reinterpret_cast<void*>(MatrixArena),arena.data(),arena.size());
        g32(0x89b564u)=MatrixArena+std::uint32_t(depth)*64u;g32(0x89b568u)=std::uint32_t(depth);
    };
    auto pick_depth=[&](unsigned i)->std::int32_t{const unsigned k=i%16u;return k==0?63:k==1?64:k==2?62:std::int32_t(rng()%8u);};
    auto compare_arena=[&](const std::vector<std::uint8_t>& arena,const PcMatrixStack& s,const char* section){
        compare(arena.data(),MatrixArena,arena.size(),"matrix arena",section);
        std::array<std::uint32_t,2> st{MatrixArena+std::uint32_t(s.current_offset),std::uint32_t(s.depth)};
        compare(st.data(),0x89b564u,8,"matrix pointer/depth",section);
    };

    // ------------------------------------------------ 47ED90 ghost reset
    for(unsigned i=0;i<4000;++i){
        const std::int32_t modes[]{0,1,2,3,4,-1,0x7fffffff,std::int32_t(rng())};
        const std::int32_t mode=modes[i%8u];
        fill(records,RecordsBytes);fill(0x81010cu,0x18);fill(player,0x20);
        *reinterpret_cast<std::uint8_t*>(0x810445u)=std::uint8_t(rng());g32(0x81043cu)=rng();
        *reinterpret_cast<std::int32_t*>(0x78024cu)=mode;*reinterpret_cast<std::uint8_t*>(0x83036du)=std::uint8_t(rng());
        g32(0x81373cu)=records;g32(0x799d18u)=player;
        std::vector<std::uint8_t> rec(RecordsBytes),rs(0x18);
        std::memcpy(rec.data(),reinterpret_cast<void*>(records),rec.size());std::memcpy(rs.data(),reinterpret_cast<void*>(0x81010cu),rs.size());
        PcGhostRecorderGlobals g{Bytes(rs.data(),rs.size()),*reinterpret_cast<std::uint8_t*>(0x810445u),g32(0x81043cu)};
        const auto* ev=reinterpret_cast<const std::uint8_t*>(player);
        PcGhostResetInputs in{mode,*reinterpret_cast<std::uint8_t*>(0x83036du),ev[0x11],ev[0x12],ev[0x13]};
        pc_ghost_record_reset_47ed90(g,Bytes(rec.data(),rec.size()),in);
        prepare(0x47ed90u);run();
        if(inject&&i==3999)rec[0x29]^=1;
        compare(rs.data(),0x81010cu,rs.size(),"recorder 81010C",  "47ED90");
        compare(&g.player_model_810445,0x810445u,1,"810445","47ED90");
        compare(&g.frame_81043c,0x81043cu,4,"81043C","47ED90");
        compare(rec.data(),records,rec.size(),"record slots","47ED90");
    }

    // ------------------------------------------------ 47F330 record writer
    auto random_car=[&](unsigned i){
        fill(car,0x300);
        auto* c=reinterpret_cast<std::uint8_t*>(car);
        const bool odd=i%13u==0;
        for(unsigned k=0;k<3;++k)gf(car+0x14+k*4,odd?odd_float():real(-3000,3000));
        for(unsigned o:{0x2cu,0x2eu,0x30u,0x32u,0x40u,0x44u,0x260u}){const std::uint16_t w=std::uint16_t(rng());std::memcpy(c+o,&w,2);}
        if(i%3u==0){const std::uint16_t w=std::uint16_t(rng()%0x2000u);std::memcpy(c+0x2c,&w,2);}
        g32(car+0x5c)=(i%4u==0)?0u:(i%4u==1?1u:rng());
        g32(car+4)=rng();g32(car+0x38)=(i%5u==0)?0u:rng();
        const float v=i%9u==0?odd_float():(i%3u==0?0.f:real(-60,120));gf(car+0x2c8,v);
        for(unsigned k=0;k<3;++k)gf(car+0x2d8+k*4,odd?odd_float():real(-8,8));
        for(unsigned k=0;k<3;++k)gf(car+0x2e4+k*4,odd?odd_float():real(-3.5f,3.5f));
    };
    const std::uint32_t slot=records+0xfd4u*2u;
    for(unsigned i=0;i<6000;++i){
        random_car(i);
        area_matrix(0x7d2da0u,i%11u==0?2u:(i%7u==0?1u:0u));area_matrix(0x7d3190u,i%17u==0?2u:(i%5u==0?3u:0u));
        fill(slot,0xfd4);fill(0x81010cu,0x18);
        const std::int32_t counts[]{0,1,std::int32_t(rng()%0xf60u),0xf5f,0xf60,0xf61,-1,std::int32_t(rng()%64u)};
        *reinterpret_cast<std::int32_t*>(0x810110u)=counts[i%8u];
        const std::uint32_t divisor=(i%6u==0)?1u+rng()%40u:0x10u;
        g32(0x81043cu)=(i%4u==0)?rng():(rng()%64u)*0x10u;
        std::vector<std::uint8_t> arena;const std::int32_t depth=pick_depth(i);
        // Make equality paths reachable: a native dry run supplies the values
        // the original will compute, then some of them seed the recorder.
        if(i%2u==0&&counts[i%8u]>=0&&counts[i%8u]<0xf60){
            setup_arena(arena,depth);
            std::vector<std::uint8_t> rs(0x18),sl(0xfd4),cr(0x300);
            std::memcpy(rs.data(),reinterpret_cast<void*>(0x81010cu),0x18);std::memcpy(sl.data(),reinterpret_cast<void*>(slot),0xfd4);
            std::memcpy(cr.data(),reinterpret_cast<void*>(car),0x300);
            PcMatrixStack s{Bytes(arena.data(),arena.size()),std::ptrdiff_t(depth)*64,depth,64};
            try{
                pc_ghost_record_write_47f330(Bytes(rs.data(),0x18),Bytes(sl.data(),0xfd4),Bytes(cr.data(),0x300),
                    {g32(0x81043cu),divisor,Bytes(reinterpret_cast<void*>(0x7d2da0u),64),Bytes(reinterpret_cast<void*>(0x7d3190u),64)},s);
                auto* live=reinterpret_cast<std::uint8_t*>(0x81010cu);
                for(unsigned k=0;k<3;++k)if(rng()%2u)std::memcpy(live+8+k*4,rs.data()+8+k*4,4);
                for(unsigned k=0;k<3;++k)if(rng()%2u)live[0x14+k]=rs[0x14+k];
                if(rng()%2u)live[0x17]=reinterpret_cast<std::uint8_t*>(car)[0x33];
            }catch(const std::exception&){}
        }
        setup_arena(arena,depth);
        std::vector<std::uint8_t> rs(0x18),sl(0xfd4),cr(0x300);
        std::memcpy(rs.data(),reinterpret_cast<void*>(0x81010cu),0x18);std::memcpy(sl.data(),reinterpret_cast<void*>(slot),0xfd4);
        std::memcpy(cr.data(),reinterpret_cast<void*>(car),0x300);
        PcMatrixStack s{Bytes(arena.data(),arena.size()),std::ptrdiff_t(depth)*64,depth,64};
        pc_ghost_record_write_47f330(Bytes(rs.data(),0x18),Bytes(sl.data(),0xfd4),Bytes(cr.data(),0x300),
            {g32(0x81043cu),divisor,Bytes(reinterpret_cast<void*>(0x7d2da0u),64),Bytes(reinterpret_cast<void*>(0x7d3190u),64)},s);
        {   const std::int32_t before=*reinterpret_cast<std::int32_t*>(0x810110u);const std::int32_t after=Bytes(rs.data(),0x18).i32(4);
            if(before>=0xf60)++cov["47F330 full"];else if(after==before)++cov["47F330 skipped (frame % divisor)"];
            else{++cov["47F330 written"];if(sl[0x34+before]==0x80)++cov["47F330 x marker 0x80"];
                if(after-before>=17)++cov["47F330 float block"];if(before<0)++cov["47F330 negative count"];}
        }
        prepare(0x47f330u);guest_call.esi=0x81010cu;guest_call.ecx=slot;
        reinterpret_cast<unsigned*>(S)[0]=car;reinterpret_cast<unsigned*>(S)[1]=divisor;run();
        if(inject&&i==5999)sl[0x34]^=1;
        compare(rs.data(),0x81010cu,0x18,"recorder","47F330");
        compare(sl.data(),slot,0xfd4,"record slot","47F330");
        compare(cr.data(),car,0x300,"car","47F330");
        compare_arena(arena,s,"47F330");
    }

    // ------------------------------------------------ 466AF0 bridge black box
    // Capture every register at the continuation 466AFA, reached only via the
    // protected jmp [0x10398E8]; also check the bridge leaves .data untouched.
    unsigned bridge_cases=0,bridge_bad=0;
    {
        std::vector<std::uint8_t> c;auto imm=[&](std::uint32_t v){for(unsigned k=0;k<4;++k)c.push_back(std::uint8_t(v>>(k*8)));};
        const std::uint32_t buf=capture+0x80;
        c.push_back(0xa3);imm(buf);
        const std::uint8_t regs[]{0x0d,0x15,0x1d,0x25,0x2d,0x35,0x3d};
        for(unsigned k=0;k<7;++k){c.push_back(0x89);c.push_back(regs[k]);imm(buf+4+k*4);}
        c.push_back(0xbc);imm(S-4);c.push_back(0xc3); // resume the harness return path
        std::memcpy(reinterpret_cast<void*>(capture),c.data(),c.size());
        std::array<std::uint8_t,5> saved{};std::memcpy(saved.data(),reinterpret_cast<void*>(0x466afau),5);
        r046_write_rel32(0x466afau,0xe9,capture);
        std::vector<std::uint8_t> before(0x98d000u-0x62f000u);
        for(unsigned i=0;i<20000;++i){
            const std::uint32_t v=i<64?(1u<<(i%32u))^(i>=32?0xffffffffu:0u):rng();
            g32(packet)=v;const bool snap=i%2000u==0;
            if(snap)std::memcpy(before.data(),reinterpret_cast<void*>(0x62f000u),before.size());
            prepare(0x466af0u);guest_call.eax=frame_a;guest_call.ecx=packet;guest_call.ebx=0x11110000u+i;guest_call.esi=0x22220000u+i;
            guest_call.edi=0x33330000u+i;guest_call.ebp=0x44440000u+i;guest_call.edx=0x55555555u;run_original32();
            const auto* r=reinterpret_cast<const std::uint32_t*>(buf);
            // eax ecx edx ebx esp ebp esi edi
            const std::uint32_t expect_edx=((v>>1)&0x3fffu)^((inject&&i==19999)?1u:0u);
            const bool ok=r[0]==frame_a&&r[1]==packet&&r[2]==expect_edx&&r[3]==0x11110000u+i&&r[4]==S-4u&&
                          r[5]==0x44440000u+i&&r[6]==0x22220000u+i&&r[7]==0x33330000u+i;
            ++bridge_cases;if(!ok){++bridge_bad;if(bridge_bad<6)std::cerr<<std::hex<<"bridge v="<<v<<" eax="<<r[0]<<" ecx="<<r[1]<<" edx="<<r[2]
                <<" ebx="<<r[3]<<" esp="<<r[4]<<" ebp="<<r[5]<<" esi="<<r[6]<<" edi="<<r[7]<<std::dec<<"\n";}
            if(snap&&std::memcmp(before.data(),reinterpret_cast<void*>(0x62f000u),before.size())){++bridge_bad;std::cerr<<"bridge wrote .data\n";}
        }
        auto* page=reinterpret_cast<void*>(0x466afau&~0xfffu);mprotect(page,8192,PROT_READ|PROT_WRITE|PROT_EXEC);
        std::memcpy(reinterpret_cast<void*>(0x466afau),saved.data(),5);mprotect(page,8192,PROT_READ|PROT_EXEC);
        cases+=bridge_cases;mismatches+=bridge_bad;per["466AF0 bridge"]={bridge_cases,bridge_bad};
    }

    // ------------------------------------------------ packet codecs (leaves)
    auto random_frame=[&](std::uint32_t a,unsigned i){
        fill(a,0x30);
        if(i%4u!=0)for(unsigned k=0;k<3;++k)gf(a+k*4,real(-100,100));
        if(i%4u==1)for(unsigned k=0;k<3;++k)gf(a+k*4,real(-0.2f,0.2f));
        if(i%5u!=0){gf(a+0x14,real(-8,8));gf(a+0x1c,real(-8,8));}
        if(i%7u==0){gf(a+0x14,0.f);gf(a+0x1c,0.f);for(unsigned o:{0x25u,0x27u,0x29u})reinterpret_cast<std::uint8_t*>(a)[o]=0;}
        if(i%6u==0){const std::uint16_t w=std::uint16_t(rng()%9u-1u);std::memcpy(reinterpret_cast<void*>(a+0x12),&w,2);}
    };
    for(unsigned i=0;i<30000;++i){
        const unsigned which=i%6u;
        random_frame(frame_a,i);random_frame(frame_b,i+1);fill(frame_c,0x30);fill(packet,0x20);
        if(which==1&&i%12u==1)g32(packet)&=~1u;
        if(which==4&&i%12u==4)g32(packet)&=~1u;
        std::vector<std::uint8_t> a(0x30),b(0x30),c(0x30),p(0x20);
        std::memcpy(a.data(),reinterpret_cast<void*>(frame_a),0x30);std::memcpy(b.data(),reinterpret_cast<void*>(frame_b),0x30);
        std::memcpy(c.data(),reinterpret_cast<void*>(frame_c),0x30);std::memcpy(p.data(),reinterpret_cast<void*>(packet),0x20);
        Bytes A(a.data(),0x30),B(b.data(),0x30),C(c.data(),0x30),Pk(p.data(),0x20);
        const char* name="";
        switch(which){
        case 0:name="466A20";pc_ghost_key_encode_466a20(Pk,A);prepare(0x466a20u);guest_call.eax=packet;guest_call.ecx=frame_a;break;
        case 1:name="466AF0";pc_ghost_key_decode_466af0(C,Pk);prepare(0x466af0u);guest_call.eax=frame_c;guest_call.ecx=packet;break;
        case 2:name="466030";pc_ghost_frame_delta_466030(C,A,B);prepare(0x466030u);guest_call.eax=frame_c;guest_call.ecx=frame_a;reinterpret_cast<unsigned*>(S)[0]=frame_b;break;
        case 3:name="4667D0";pc_ghost_delta_encode_4667d0(Pk,A);prepare(0x4667d0u);guest_call.eax=packet;guest_call.ecx=frame_a;break;
        case 4:name="4668F0";pc_ghost_delta_decode_4668f0(C,Pk);prepare(0x4668f0u);guest_call.eax=frame_c;guest_call.ecx=packet;break;
        default:name="466720";pc_ghost_frame_add_466720(C,A,B);prepare(0x466720u);guest_call.eax=frame_c;guest_call.ecx=frame_a;reinterpret_cast<unsigned*>(S)[0]=frame_b;break;
        }
        run();
        if(inject&&i>=29994)c[0x13]^=1,p[0x1f]^=1;
        compare(a.data(),frame_a,0x30,"frame a",name);compare(b.data(),frame_b,0x30,"frame b",name);
        compare(c.data(),frame_c,0x30,"frame c",name);compare(p.data(),packet,0x20,"packet",name);
    }

    // ------------------------------------------------ 466E50 serializer
    auto run_466e50=[&](unsigned i,bool evolve){
        if(!evolve)random_car(i);
        else{   // small per-frame motion so delta packets stay in range
            for(unsigned k=0;k<3;++k){float v;std::memcpy(&v,reinterpret_cast<void*>(car+0x14+k*4),4);gf(car+0x14+k*4,v+real(-2,2));}
            for(unsigned o:{0x2cu,0x2eu,0x30u}){std::int16_t w;std::memcpy(&w,reinterpret_cast<void*>(car+o),2);w=std::int16_t(w+std::int16_t(rng()%801u)-400);std::memcpy(reinterpret_cast<void*>(car+o),&w,2);}
            if(rng()%3u==0){std::uint16_t w=std::uint16_t(rng());std::memcpy(reinterpret_cast<void*>(car+0x260),&w,2);}
        }
        g32(player+0x5c)=(i%3u==0)?0u:1u;g32(0x799d18u)=player;
        *reinterpret_cast<std::int32_t*>(0x656234u)=(i%5u==0)?std::int32_t(0x3c+rng()%100u):(i%23u==0?std::int32_t(rng()):std::int32_t(rng()%0x3cu));
        if(i%29u==0)area_matrix(0x7d2da0u,1);
        std::vector<std::uint8_t> arena;const std::int32_t depth=pick_depth(i);setup_arena(arena,depth);
        std::vector<std::uint8_t> pb(PacketBytes),prev(0x30),mat(0x40),cr(0x300);
        std::memcpy(pb.data(),reinterpret_cast<void*>(pbuf),PacketBytes);std::memcpy(prev.data(),reinterpret_cast<void*>(0x7f92c8u),0x30);
        std::memcpy(mat.data(),reinterpret_cast<void*>(0x7f9300u),0x40);std::memcpy(cr.data(),reinterpret_cast<void*>(car),0x300);
        PcGhostPacketState st{g32(0x7f8ef8u),g32(0x7f8ef4u),g32(0x7f91f0u),g32(0x7f8d84u),Bytes(prev.data(),0x30),Bytes(mat.data(),0x40)};
        PcMatrixStack s{Bytes(arena.data(),arena.size()),std::ptrdiff_t(depth)*64,depth,64};
        pc_ghost_packet_write_466e50(st,{Bytes(pb.data(),pb.size()),pbuf},Bytes(cr.data(),0x300),
            {g32(player+0x5c),*reinterpret_cast<std::int32_t*>(0x656234u),Bytes(reinterpret_cast<void*>(0x7d2da0u),64)},s);
        prepare(0x466e50u);guest_call.eax=car;run();
        return std::make_tuple(std::move(pb),std::move(prev),std::move(mat),std::move(cr),std::move(arena),st,s);
    };
    unsigned seq_cases=0;
    for(unsigned seq=0;seq<400;++seq){
        area_matrix(0x7d2da0u,seq%9u==0?2u:0u);fill(0x7f9300u,0x40);fill(0x7f92c8u,0x30);fill(pbuf,PacketBytes);
        g32(0x7f8d84u)=pbuf;
        const unsigned mode=seq%10u;
        g32(0x7f91f0u)=mode==7?pbuf+0x9370u+rng()%0x20u:pbuf+(mode==9?rng()%0x9400u:(mode==8?0x9360u:0u));
        g32(0x7f8ef4u)=mode==6?0xf5eu+rng()%4u:(mode==5?1u+rng()%100u:0u);g32(0x7f8ef8u)=rng()%2u;
        random_car(seq);
        const unsigned frames=mode==9?4u:24u;
        for(unsigned f=0;f<frames;++f){
            auto [pb,prev,mat,cr,arena,st,s]=run_466e50(seq*64u+f,f>0);
            std::array<std::uint32_t,3> g{st.overflow_7f8ef8,st.count_7f8ef4,st.cursor_7f91f0};
            {   static std::uint32_t last_count;const auto d=st.cursor_7f91f0;(void)d;
                if(st.overflow_7f8ef8==1&&st.count_7f8ef4==last_count)++cov["466E50 overflow/no-op"];
                else if(st.count_7f8ef4==1)++cov["466E50 key frame"];else ++cov["466E50 delta frame"];
                if(*reinterpret_cast<std::int32_t*>(0x656234u)<0x3c)++cov["466E50 matrix path"];
                last_count=st.count_7f8ef4;}
            if(inject&&seq==399&&f==frames-1)g[1]^=1;
            compare(&g[0],0x7f8ef8u,4,"7F8EF8","466E50");compare(&g[1],0x7f8ef4u,4,"7F8EF4","466E50");
            compare(&g[2],0x7f91f0u,4,"7F91F0","466E50");compare(&st.base_7f8d84,0x7f8d84u,4,"7F8D84","466E50");
            compare(prev.data(),0x7f92c8u,0x30,"7F92C8 frame","466E50");compare(mat.data(),0x7f9300u,0x40,"7F9300 matrix","466E50");
            compare(pb.data(),pbuf,PacketBytes,"packet buffer","466E50");compare(cr.data(),car,0x300,"car","466E50");
            compare_arena(arena,s,"466E50");++seq_cases;
        }
    }
    for(const auto& [k,v]:cov)std::cerr<<"coverage "<<k<<": "<<v<<"\n";
    for(const auto& [k,v]:per)std::cerr<<"section "<<k<<": "<<v[0]<<" compares, "<<v[1]<<" mismatching\n";
    std::cerr<<"restored bytes: "<<restored<<", 466E50 frames: "<<seq_cases<<", x87 cw "<<std::hex<<x87_control<<std::dec<<"\n";
    std::cout<<"{\"routine\":\"car_services_ghost_probe\",\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}"<<'\n';
    return mismatches?1:0;
}

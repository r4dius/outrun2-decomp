// Included inside the compare_pc dispatcher; uses its private x86 harness.
if(only=="record_car_probe"){
    // Variant 0 record ghost cars (race_records.cpp 480220 + helpers, race_ghost_car.cpp
    // 4AD1A0 / 4AD1F0 / 4AD080) against the original:
    //   0 480220 playback (its protected head live: interpolated frames, stream frames, the
    //     finished / foreign-level / 47ED00 paths), 1 4AD080 control, 2 4AD1F0 init, 3 4AD1A0,
    //   4 47F930 sample decode, 5 47EF90 Bezier, 6 4800F0, 7 47F0E0, 8 47F140 / 47F1A0 / 47ED00,
    //   9 43F370 (43EB60 stubbed: its five arguments logged; OR2_RECORD_CAR_ENTRY=9 only). Measured:
    //     43EB60(0x100, &point copy, &arg slot (the hint is the pointer value), 0, 0).
    //   10 47F260, 11 47FF60, 12 47FD40, 13 480F80 (services 44C940, 451350, 450250, 450780,
    //      43F9C0 and 49B2D0 scripted), 14 47ED90, 15 47F330 encoder (D3DXMatrixInverse wired as
    //      in ghost_car_probe), 16 47F780 (44FF10, 450750, 4505A0, 450630, 451180, 48B1A0 scripted),
    //   17 47EE70 (its protected step live), 18 47EF30.
    // Capture stubs: 450130, 43F370, 49B2D0, 47F1A0, 480220, 47F140 (scripted values), 4401D0,
    // 4A2650, 440BD0, 440BA0, 43EB60. Matrix leaves run as original code on the matrix arena.
    // Compared: the record slots 80FB40..810453, 813730..81374F, the queue 841FF0..842037, the four
    // car works 7815A0.., the player work, the matrix arena and 89B564/89B568, the sample / output
    // buffers, the service log and EAX.
    using namespace outrun::platform;
    if(argc<=7||std::string(argv[7])=="-")throw std::runtime_error("record car probe requires the pinned d3dx9_29.dll (D3DX rotations)");
    init_matrix_oracle_globals();wire_d3dx_imports(map_d3dx29(argv[7]));
    constexpr unsigned base=0x3b900000u,stubs=base,trace=base+0x4000u,values=base+0x8000u,Records=base+0x10000u,Buffers=base+0x30000u;
    constexpr unsigned Table2=base+0x40000u,Table1=base+0x60000u;   // [813740] 30 records, [81373C] 15 records
    map_at(base,0x80000,PROT_READ|PROT_WRITE|PROT_EXEC);map_original_arithmetic_bridge(file);
    {   // 40A240 calls D3DXMatrixInverse through the game IAT; its generic body imports msvcrt _finite.
        static const std::uint8_t finite_stub[]{0x8b,0x44,0x24,0x08,0x25,0x00,0x00,0xf0,0x7f,0x3d,0x00,0x00,0xf0,0x7f,0x0f,0x95,0xc0,0x0f,0xb6,0xc0,0xc3};
        std::memcpy(reinterpret_cast<void*>(stubs+0x3f00u),finite_stub,sizeof(finite_stub));
        auto* iat=reinterpret_cast<void*>(D3DX_BASE+0x1000u);
        if(mprotect(iat,4096,PROT_READ|PROT_WRITE))throw std::runtime_error("d3dx IAT page");
        *reinterpret_cast<std::uint32_t*>(D3DX_BASE+0x1270u)=stubs+0x3f00u;
        if(mprotect(iat,4096,PROT_READ))throw std::runtime_error("d3dx IAT restore");
        auto* game_iat=reinterpret_cast<void*>(0x596000u);
        if(mprotect(game_iat,4096,PROT_READ|PROT_WRITE))throw std::runtime_error("game IAT");
        *reinterpret_cast<std::uint32_t*>(0x5962dcu)=D3DX_BASE+(0x440e9cu-0x400000u);   // D3DXMatrixInverse
        if(mprotect(game_iat,4096,PROT_READ))throw std::runtime_error("game IAT restore");
    }
    {   const std::size_t text_file=pe_file_offset(file,0x401000u);
        for(std::uint32_t a=0x401000u;a<0x401000u+0x194881u;a+=0x100u){
            const std::size_t n=std::min<std::size_t>(0x100u,0x401000u+0x194881u-a);
            if(std::memcmp(reinterpret_cast<void*>(a),file.data()+text_file+(a-0x401000u),n))r060_raw_write(a,file.data()+text_file+(a-0x401000u),n);
        }
    }
    for(std::uint32_t page=0x62f000u;page<0x98d000u;page+=0x1000u)mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE);
    auto emit32=[](std::vector<std::uint8_t>& b,std::uint32_t v){for(unsigned k=0;k<4;++k)b.push_back(std::uint8_t(v>>(k*8)));};
    struct Svc{std::uint32_t pc;unsigned argc;bool value;};
    const std::vector<Svc> services{{0x450130,0,true},{0x43f370,1,true},{0x49b2d0,0,true},{0x47f1a0,1,true},{0x480220,3,true},{0x47f140,1,true},
        {0x4401d0,1,false},{0x4a2650,1,false},{0x440bd0,2,false},{0x440ba0,1,false},{0x43eb60,5,true},
        {0x44c940,1,true},{0x451350,1,true},{0x450250,2,true},{0x450780,0,true},{0x43f9c0,0,true},
        {0x44ff10,0,true},{0x450750,1,true},{0x4505a0,1,true},{0x450630,2,true},{0x451180,1,true},{0x48b1a0,0,true}};
    std::map<std::uint32_t,unsigned> index_of;std::map<std::uint32_t,std::uint32_t> stub_at;unsigned stub_off=0;
    for(std::size_t k=0;k<services.size();++k){
        const auto& e=services[k];index_of[e.pc]=unsigned(k);
        std::vector<std::uint8_t> c{0x57,0x8b,0x3d};emit32(c,trace);                         // push edi; mov edi,[count]
        c.insert(c.end(),{0xc1,0xe7,0x05,0x81,0xc7});emit32(c,trace+64);                    // shl edi,5; add edi,entries
        c.insert(c.end(),{0xc7,0x07});emit32(c,e.pc);
        for(unsigned i=0;i<e.argc;++i)c.insert(c.end(),{0x8b,0x44,0x24,std::uint8_t(8+4*i),0x89,0x47,std::uint8_t(4+4*i)});
        if(e.pc==0x43eb60u){   // *arg3 (polygon) = [values+0x40]
            c.insert(c.end(),{0x8b,0x44,0x24,0x10,0x8b,0x3d});emit32(c,values+0x200u);c.insert(c.end(),{0x89,0x38});}
        c.insert(c.end(),{0xff,0x05});emit32(c,trace);c.push_back(0x5f);
        if(e.value){c.push_back(0xa1);emit32(c,values+4*unsigned(k));}else c.insert(c.end(),{0x31,0xc0});
        c.push_back(0xc3);
        std::memcpy(reinterpret_cast<void*>(stubs+stub_off),c.data(),c.size());
        stub_at[e.pc]=stubs+stub_off;stub_off=(stub_off+unsigned(c.size())+15u)&~15u;
    }
    // Stubs are patched per entry: the function under test runs as original code.
    auto patch=[&](std::uint32_t skip){
        const std::size_t text_file=pe_file_offset(file,0x401000u);
        for(const auto& e:services){
            if(e.pc==skip)r060_raw_write(e.pc,file.data()+text_file+(e.pc-0x401000u),5);
            else r046_write_rel32(e.pc,0xe9,stub_at.at(e.pc));
        }
    };
    std::mt19937 rng(0x480220u);
    auto rnd=[&](std::uint32_t n){return n?std::uint32_t(rng()%n):0u;};
    auto real=[&](float a,float b){return a+(b-a)*float(rng()%100000)/100000.f;};
    auto g32=[](std::uint32_t a){return *reinterpret_cast<std::uint32_t*>(a);};
    auto p32=[](std::uint32_t a,std::uint32_t v){*reinterpret_cast<std::uint32_t*>(a)=v;};
    auto pf=[](std::uint32_t a,float v){std::memcpy(reinterpret_cast<void*>(a),&v,4);};
    auto p16=[](std::uint32_t a,std::uint16_t v){std::memcpy(reinterpret_cast<void*>(a),&v,2);};
    auto p8=[](std::uint32_t a,std::uint8_t v){*reinterpret_cast<std::uint8_t*>(a)=v;};
    unsigned cases{},mismatches{};const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
    std::map<std::string,unsigned> coverage;
    auto fail=[&](const std::string& what){++mismatches;if(mismatches<20)std::cerr<<what<<"\n";};
    auto compare=[&](const void* native,const void* original,std::size_t n,const std::string& what){
        ++cases;if(!std::memcmp(native,original,n))return;
        const auto* a=static_cast<const std::uint8_t*>(native);const auto* b=static_cast<const std::uint8_t*>(original);
        for(std::size_t i=0;i<n;++i)if(a[i]!=b[i]){std::ostringstream s;s<<what<<" offset=0x"<<std::hex<<i<<" PC="<<unsigned(b[i])<<" native="<<unsigned(a[i]);fail(s.str());break;}
    };
    constexpr std::uint32_t Cars=0x7815a0u,CarBytes=0x10f0u,Player=0x7804b0u;
    struct Mirror{std::uint32_t base,end;std::vector<std::uint8_t> bytes;};
    std::vector<Mirror> mirrors{{PcRecordState::BlockBase,PcRecordState::BlockEnd,{}},{PcRecordState::TailBase,PcRecordState::TailEnd,{}},
        {0x841ff0u,0x842038u,{}},{Cars,Cars+4u*CarBytes,{}},{Player,Player+CarBytes,{}},{Records,Records+0x10000u,{}},{Buffers,Buffers+0x1000u,{}},
        {0x78026cu,0x780270u,{}},{0x780258u,0x78025cu,{}},{0x799d18u,0x799d1cu,{}},{0x7d2da0u,0x7d2de0u,{}},{0x7d3190u,0x7d31d0u,{}},
        {0x79fb48u,0x79fb60u,{}},{0x799b38u+9u*0x3cu,0x799b38u+13u*0x3cu,{}},
        {Table2,Table2+PcRecordTable2Bytes,{}},{Table1,Table1+PcRecordTable1Bytes,{}}};
    // A random QHOT record whose stream is any byte string (47F930 decodes every byte string).
    auto make_record=[&](std::uint32_t at,std::int32_t level){
        for(std::uint32_t k=0;k<0xfd4u;k+=4)p32(at+k,rng());
        std::memcpy(reinterpret_cast<void*>(at),"QHOT",4);
        p32(at+0xc,rnd(8)?0x100u+rnd(0xe00u):rnd(3)?0u:0x80000000u|rng());
        p16(at+0x24,rnd(10)?0x132u:std::uint16_t(rng()));
        p8(at+0x2f,std::uint8_t(level));p8(at+0x32,std::uint8_t(rnd(3)?0:1));
        p8(at+0x33,std::uint8_t(rnd(8)?1u+rnd(8):std::uint8_t(0x80u|rng())));
        // keep the floats of the stream finite: the 0x18 raw float field rarely NaN
        for(std::uint32_t k=0x34;k<0xfd4u;++k)if(rnd(16)==0)p8(at+k,0x80);
    };
    auto sample=[&](std::uint32_t s){   // a decoded-sample image
        for(unsigned w=0;w<3;++w)pf(s+w*4,real(-6000,6000));for(unsigned w=0;w<3;++w)pf(s+0xc+w*4,real(-8,8));
        pf(s+0x18,real(0,90));p32(s+0x1c,rnd(2));for(unsigned w=0;w<6;++w)p16(s+0x20+w*2,std::uint16_t(rng()));
        p16(s+0x2c,std::uint16_t(rng()));p8(s+0x2e,std::uint8_t(rnd(2)));p8(s+0x2f,std::uint8_t(rng()));
    };
    for(unsigned iter=0;iter<count;++iter){
        static constexpr unsigned Order[]{0,1,2,3,4,5,6,7,8,10,11,12,13,14,15,16,17,18};   // entry 9 (43F370 measurement) only on request
        unsigned entry=Order[iter%18u];
        if(const char* e=std::getenv("OR2_RECORD_CAR_ENTRY"))entry=unsigned(std::stoul(e));
        static constexpr std::uint32_t Pc[]{0x480220u,0x4ad080u,0x4ad1f0u,0x4ad1a0u,0x47f930u,0x47ef90u,0x4800f0u,0x47f0e0u,0x47f140u,0x43f370u,
            0x47f260u,0x47ff60u,0x47fd40u,0x480f80u,0x47ed90u,0x47f330u,0x47f780u,0x47ee70u,0x47ef30u};
        const unsigned pick8=rnd(3);   // entry 8: 47F140, 47F1A0 or 47ED00
        patch(entry==8u?(pick8==0u?0x47f140u:pick8==1u?0x47f1a0u:0u):Pc[entry]);
        std::memset(reinterpret_cast<void*>(trace),0,0x4000);
        std::array<std::uint32_t,32> value{};value[0]=rnd(3)?0u:1u;value[1]=rnd(0x4000);value[2]=rnd(4)?rnd(0x3c):(rnd(2)?0x3cu:0x3cu+rnd(100));
        value[3]=rnd(400);value[4]=rnd(2);value[5]=rnd(4);value[10]=rnd(4);
        value[11]=rnd(15);value[12]=rnd(4)?rnd(3):rng();value[13]=rnd(14);value[14]=rnd(4)?1u+rnd(14):(rnd(2)?0u:0xffffffffu);value[15]=rnd(4)?0u:1u;
        value[16]=rnd(4)?0u:1u;value[17]=rnd(2);value[18]=rnd(0x10000);value[19]=rnd(0x10000);value[20]=rnd(0x10000);value[21]=rnd(3);
        if(entry==16u){value[0]=rnd(4)?0u:1u;value[11]=1u+rnd(13);}   // 47F780: a stage with a previous record
        const std::uint32_t preset18=rnd(3)?2u:rnd(4);
        if(entry==18u)value[14]=preset18==2u?14u:4u+rnd(11);   // every flag 47EF30 reads is written
        if(entry==17u&&rnd(4)==0)value[14]=rnd(2)?0xffffffffu:rnd(15);
        std::memcpy(reinterpret_cast<void*>(values),value.data(),sizeof value);p32(values+0x200u,rnd(4)?rnd(0x500):0xffffffffu);
        const unsigned slot=rnd(4);const std::uint32_t car=Cars+slot*CarBytes;
        const std::int32_t level=std::int32_t(rnd(15));
        // record slots and records
        for(std::uint32_t a=PcRecordState::BlockBase;a<PcRecordState::BlockEnd;a+=4)p32(a,rng());
        for(std::uint32_t a=PcRecordState::TailBase;a<PcRecordState::TailEnd;a+=4)p32(a,rng());
        for(unsigned s=0;s<4;++s){
            const std::uint32_t S=0x80fb48u+s*0xa4u,rec=Records+s*0x1000u;
            make_record(rec,rnd(3)?level:std::int32_t(rnd(15)));
            sample(S);sample(S+0x30);sample(S+0x60);
            p32(S+0x90,rnd(10)?rec:(rnd(2)?0u:Buffers));
            p32(S+0x94,rnd(3)?0u:rnd(g32(rec+0xc)&0x7fffffffu?(g32(rec+0xc)&0xfffu):1u));
            p32(S+0x98,rnd(3)?rnd(0x200):rng());p32(S+0xa0,rnd(4)?0u:1u);
        }
        p8(0x81044cu,std::uint8_t(rnd(3)?1:rnd(3)));p32(0x81043cu,rnd(0x400));
        if(entry>=10u){   // the record tables, the five +32 players and the encoder state 81010C
            p8(0x81044cu,std::uint8_t(rnd(3)?0:rnd(3)));p32(0x810440u,rnd(4)?rnd(0x200):rng());p32(0x810124u,rnd(3)?0u:0x80fb48u+rnd(4)*0xa4u);
            p32(0x810128u,rnd(3)?2u:rnd(4));
            if(entry>=17u)for(unsigned r=0;r<15;++r){const std::uint32_t at=Table1+r*0xfd4u;   // stage records vs the loaded ones
                if(rnd(4)){std::memcpy(reinterpret_cast<void*>(at),"QHOT",4);p32(at+0xc,rnd(5)?1u+rnd(0x200):0u);
                    p8(at+0x2f,std::uint8_t(rnd(8)));p8(at+0x30,std::uint8_t(rnd(2)));p32(at+4,rnd(4)?rnd(0x20000):rng());}}
            p32(0x813740u,Table2);p32(0x81373cu,entry!=13u||rnd(10)?Table1:0u);   // only 480F80 tests [81373C] = 0
            for(std::uint32_t a=Table1;a<Table1+PcRecordTable1Bytes;a+=4)p32(a,rng());
            for(unsigned r=0;r<30;++r){const std::uint32_t at=Table2+r*0xfd4u;
                if(rnd(3)){make_record(at,rnd(3)?level:std::int32_t(rnd(15)));p32(at+0x10,rnd(0x300));p8(at+0x32,std::uint8_t(rnd(3)?1:0));}
                else for(std::uint32_t k=0;k<0xfd4u;k+=4)p32(at+k,rng());}
            for(unsigned k=0;k<4;++k)p32(0x80fb48u+k*0xa4u+0x90u,Table2+rnd(30)*0xfd4u);   // 810124 may name any slot

            p32(0x81010cu,rnd(3)?1u:rnd(3));p32(0x810110u,rnd(4)?rnd(0xf70):rng()%0x1000u);
            for(std::uint32_t a=0x810114u;a<0x810120u;a+=4)pf(a,rnd(3)?-99999.9921875f:float(int(rnd(2000))-1000)*0.01f);
            for(std::uint32_t a=0x810120u;a<0x810124u;++a)p8(a,std::uint8_t(rnd(3)?rnd(4):rng()));
            p32(0x81043cu,rnd(3)?16u*rnd(20):rnd(0x200));
            for(unsigned b=0;b<5;++b){const std::uint32_t B=0x81012cu+b*0x9cu;sample(B);sample(B+0x30);sample(B+0x60);
                p32(B+0x90,rnd(3)?rnd(0x40):rng());p32(B+0x94,rnd(3)?rnd(0x300):rng()%0x2000u);}
        }
        for(std::uint32_t a=0x841ff0u;a<0x842038u;a+=4)p32(a,rng());
        p8(0x841ff0u,std::uint8_t(rnd(4)));p8(0x841ff1u,std::uint8_t(rnd(4)));
        for(unsigned q=0;q<4;++q){p32(0x841ff8u+q*16,9+q);p8(0x842000u+q*16,std::uint8_t(rnd(30)));}
        // cars and player
        for(std::uint32_t a=Cars;a<Cars+4*CarBytes;a+=4)p32(a,rnd(4)?0u:rng());
        for(unsigned k=0;k<4;++k){const std::uint32_t w=Cars+k*CarBytes;p32(w,9+k);
            for(unsigned v=0;v<3;++v)pf(w+0x14+v*4,real(-3000,3000));pf(w+0xb68,rnd(4)?real(0,1):real(0.2f,0.8f));
            p16(w+0x260,std::uint16_t(rnd(40)));p8(w+0x11,std::uint8_t(rnd(30)));pf(w+0x2c8,real(0,90));
            for(unsigned v=0;v<6;++v)pf(w+0x2d8+v*4,real(-3,3));}
        for(std::uint32_t a=Player;a<Player+CarBytes;a+=4)p32(a,rnd(4)?0u:rng());
        p16(Player+0x260,std::uint16_t(rnd(40)));p32(Player+0x68,rnd(3)?std::uint32_t(level):rnd(15));p32(Player+4,rng());
        for(unsigned v=0;v<3;++v)pf(Player+0x14+v*4,real(-6000,6000));
        for(unsigned w:{0x2cu,0x2eu,0x30u})p16(Player+w,std::uint16_t(rng()));
        p8(Player+0x33,std::uint8_t(rnd(4)));p32(Player+0x5c,rnd(2));pf(Player+0x2c8,rnd(4)?real(0,90):real(-2,0));
        p32(Player+0x38,rnd(3)?0u:rnd(3)-1u);for(unsigned v=0;v<6;++v)pf(Player+0x2d8+v*4,real(-3,3));
        p8(Player+0x11,std::uint8_t(rnd(30)));p8(Player+0x12,std::uint8_t(rnd(8)));p8(Player+0x13,std::uint8_t(rnd(8)));
        p32(0x799d18u,Player);
        if(entry>=10u)if(const std::uint32_t named=g32(0x810124u)){   // as 47FD40 left it: a bound QHOT record, its event marked
            const std::uint32_t k=(named-0x80fb48u)/0xa4u,rec=Table2+(21u+2u*rnd(5))*0xfd4u;
            make_record(rec,level);p32(named+0x90u,rec);
            p32(Cars+k*CarBytes+0xc5cu,g32(Cars+k*CarBytes+0xc5cu)|0x100000u);
        }
        p32(0x78026cu,rnd(6)?0x10u:rnd(0x20));p32(0x780258u,rnd(6)?0u:rnd(10));
        for(unsigned k=0;k<16;++k){pf(0x7d2da0u+k*4,k==15?1.f:(k>=12?real(-500,500):real(-1,1)));pf(0x7d3190u+k*4,k==15?1.f:(k>=12?real(-500,500):real(-1,1)));}
        for(std::uint32_t a=0x79fb48u;a<0x79fb60u;++a)p8(a,std::uint8_t(rnd(4)));
        for(unsigned e=9;e<13;++e)p32(0x799b38u+e*0x3cu,Cars+(e-9)*CarBytes);
        for(std::uint32_t a=Buffers;a<Buffers+0x1000u;a+=4)p32(a,rnd(3)?0u:rng());
        // per entry arguments
        std::uint32_t frame=rnd(4)?rnd(0x400):(rnd(2)?0u:rng());
        std::uint32_t a0=0,a1=0,a2=0,a3=0;float t=real(0,1);
        const std::uint32_t Stream=Buffers+0x400u,Out=Buffers,Ref=Buffers+0x40u,Out2=Buffers+0x80u;
        if(entry==4){for(std::uint32_t k=0;k<0x100;++k)p8(Stream+k,std::uint8_t(rnd(6)?rng():0x80u));sample(Ref);a0=rnd(0x40);a1=rnd(3)?Ref:0u;}
        if(entry==5){sample(Buffers);sample(Buffers+0x40);sample(Buffers+0x80);if(rnd(4)==0)std::memcpy(reinterpret_cast<void*>(Buffers+0x80),reinterpret_cast<void*>(Buffers),12);}
        if(entry==6||entry==7){for(unsigned k=0;k<8;++k)p16(Buffers+k*2,std::uint16_t(rng()));a0=std::uint16_t(rng());a1=std::uint16_t(rng());}
        if(entry==8){a0=rnd(4);a1=pick8;}
        if(entry==9){pf(Buffers,real(-3000,3000));pf(Buffers+4,real(-50,50));pf(Buffers+8,real(-3000,3000));}
        for(auto& mi:mirrors){mi.bytes.resize(mi.end-mi.base);std::memcpy(mi.bytes.data(),reinterpret_cast<void*>(mi.base),mi.bytes.size());}
        reset_matrix_oracle_globals();
        std::vector<std::uint8_t> arena(0x800);outrun::driving::Bytes ar(arena.data(),arena.size());
        for(unsigned k=0;k<16;++k)ar.putf(k*4,k%5==0?1.f:real(-0.5f,0.5f));
        std::memcpy(reinterpret_cast<void*>(MatrixArena),arena.data(),arena.size());
        // original
        auto* S32=reinterpret_cast<unsigned*>(S);
        prepare(Pc[entry]);
        switch(entry){
        case 0:S32[0]=car;S32[1]=slot;S32[2]=frame;break;
        case 1:case 2:S32[0]=car;break;
        case 3:S32[0]=9+slot;S32[1]=rng();S32[2]=rng();S32[3]=rng();a0=S32[0];a1=S32[1];a2=S32[2];a3=S32[3];break;
        case 4:guest_call.eax=a0;guest_call.esi=Stream;guest_call.edi=Out;S32[0]=a1;break;
        case 5:guest_call.esi=Buffers;guest_call.edi=Buffers+0x40;guest_call.ebx=Buffers+0x80;std::memcpy(&S32[0],&t,4);S32[1]=Out2+0x40;break;
        case 6:guest_call.eax=Buffers+6;guest_call.ecx=Out2;guest_call.edx=Buffers;std::memcpy(&guest_call.xmm0,&t,4);break;
        case 7:S32[0]=a0;S32[1]=a1;std::memcpy(&S32[2],&t,4);S32[3]=Out2;break;
        case 8:if(a1==0){prepare(0x47f140u);S32[0]=a0;}else if(a1==1){prepare(0x47f1a0u);S32[0]=a0;}else{prepare(0x47ed00u);guest_call.eax=a0;}break;
        case 10:a0=9+rnd(4);a1=rnd(4);a2=rng();a3=Table2+rnd(30)*0xfd4u;S32[0]=a0;S32[1]=a1;S32[2]=a2;S32[3]=a3;break;
        case 11:case 12:case 13:case 14:break;
        case 15:a0=Table1+rnd(15)*0xfd4u;a1=rnd(4)?0x10u:1u+rnd(20);guest_call.ecx=a0;guest_call.esi=0x81010cu;S32[0]=Player;S32[1]=a1;break;
        case 16:S32[0]=Player;break;
        case 17:S32[0]=Buffers+0x200;break;
        case 18:S32[0]=preset18;break;
        default:S32[0]=Buffers;break;
        }
        run();const std::uint32_t guest_eax=guest_call.out_eax;
        // native
        PcRaceMemory mem;std::vector<std::vector<std::uint8_t>> native(mirrors.size());
        for(std::size_t i=0;i<EmbeddedExeRangeCount;++i)mem.map_const(EmbeddedExeRanges[i].base,EmbeddedExeRanges[i].data,EmbeddedExeRanges[i].size);
        ghost_map_tables(mem);ghost_map_car_tables(mem);   // 4866C0 model records 650500
        for(std::size_t i=0;i<mirrors.size();++i){native[i]=mirrors[i].bytes;mem.map(mirrors[i].base,native[i].data(),native[i].size());}
        outrun::driving::PcMatrixStack stack{ar,0,0,64};
        struct NCall{std::uint32_t pc;decltype(PcRaceCall::args) a;};std::vector<NCall> calls;
        PcRaceContext ctx{mem,stack,[&](const PcRaceCall& k)->std::uint32_t{
            const auto it=index_of.find(k.pc);if(it==index_of.end()){std::ostringstream s;s<<"record car probe: unexpected service "<<std::hex<<k.pc;throw std::runtime_error(s.str());}
            calls.push_back({k.pc,k.args});return services[it->second].value?value[it->second]:0u;},nullptr};
        std::uint32_t native_eax=guest_eax;bool ok=true;std::string why;
        try{
            switch(entry){
            case 0:native_eax=records_play_480220(ctx,car,slot,frame);break;
            case 1:record_car_control_4ad080(ctx,car);break;
            case 2:record_car_init_4ad1f0(ctx,car);break;
            case 3:record_car_queue_4ad1a0(mem,a0,std::uint8_t(a1),a2,std::uint8_t(a3));break;
            case 4:native_eax=records_sample_47f930(mem,a0,Stream,Out,a1);break;
            case 5:records_bezier_47ef90(mem,t,Out2+0x40,Buffers,Buffers+0x40,Buffers+0x80);break;
            case 6:records_words_4800f0(mem,Buffers+6,Out2,Buffers,t);break;
            case 7:records_word_47f0e0(mem,a0,a1,t,Out2);break;
            case 8:if(a1==0)native_eax=records_state_47f140(mem,a0);else if(a1==1)native_eax=records_frame_47f1a0(mem,a0);else records_drop_47ed00(mem,a0);break;
            case 10:records_bind_47f260(mem,a0,a1,a2,a3);break;
            case 11:records_schedule_47ff60(ctx);break;
            case 12:records_advance_47fd40(ctx);break;
            case 13:records_frame_480f80(ctx);break;
            case 14:records_restart_47ed90(ctx);break;
            case 15:records_encode_47f330(ctx,a0,0x81010cu,Player,a1);break;
            case 16:records_player_47f780(ctx,Player);break;
            case 17:records_stage_flags_47ee70(ctx,Buffers+0x200);break;
            case 18:native_eax=records_ranking_47ef30(ctx,preset18);break;
            default:break;
            }
        }catch(const std::exception& e){ok=false;why=e.what();}
        const std::string tag="entry "+std::to_string(entry)+" run "+std::to_string(iter);
        if(entry==9){   // measurement: the 43EB60 arguments 43F370 passes (its VM-pushed first argument)
            const auto* g=reinterpret_cast<const std::uint32_t*>(trace+64);
            if(iter<40){std::ostringstream s;s<<std::hex<<"43F370 -> 43EB60("<<g[1]<<", "<<g[2]<<", "<<g[3]<<", "<<g[4]<<", "<<g[5]<<") eax "<<guest_eax<<" poly "<<g32(values+0x200u)<<" type "<<value[10];std::cerr<<s.str()<<"\n";}
            ++coverage["entry 9"];continue;
        }
        ++cases;if(!ok){fail(tag+" native threw: "+why);continue;}
        if(inject&&iter==7)native[3][0x14]^=1;
        for(std::size_t i=0;i<mirrors.size();++i){std::ostringstream s;s<<tag<<" region "<<std::hex<<mirrors[i].base;
            compare(native[i].data(),reinterpret_cast<void*>(mirrors[i].base),native[i].size(),s.str());}
        compare(arena.data(),reinterpret_cast<void*>(MatrixArena),arena.size(),tag+" matrix arena");
        {std::array<std::uint32_t,2> sp{MatrixArena+std::uint32_t(stack.current_offset),std::uint32_t(stack.depth)};
         compare(sp.data(),reinterpret_cast<void*>(0x89b564),8,tag+" matrix pointer/depth");}
        ++cases;if((entry==0||entry==4||entry==8||entry==18)&&!(entry==8&&a1==2u)&&native_eax!=guest_eax){std::ostringstream s;s<<tag<<" EAX PC="<<guest_eax<<" native="<<native_eax;fail(s.str());}
        const unsigned n=g32(trace);++cases;
        if(n!=calls.size()){fail(tag+" service count PC="+std::to_string(n)+" native="+std::to_string(calls.size()));
            if(std::getenv("OR2_RECORD_CAR_LOG")){std::ostringstream s;s<<std::hex<<"  PC:";for(unsigned i=0;i<n;++i)s<<" "<<g32(trace+64+i*32);
                s<<" | native:";for(const auto& cl:calls)s<<" "<<cl.pc;std::cerr<<s.str()<<'\n';}}
        else for(unsigned i=0;i<n;++i){const auto* g=reinterpret_cast<const std::uint32_t*>(trace+64+i*32);
            const auto& e=services[index_of[g[0]]];bool same=g[0]==calls[i].pc;
            for(unsigned k=0;k<e.argc;++k)same=same&&g[1+k]==calls[i].a[k];
            if(!same){std::ostringstream s;s<<tag<<" service #"<<i<<" "<<std::hex<<g[0];fail(s.str());break;}}
        ++coverage["entry "+std::to_string(entry)];
        if(entry==12u||entry==13u){
            auto before=[&](std::uint32_t a){return *reinterpret_cast<const std::uint32_t*>(mirrors[0].bytes.data()+(a-PcRecordState::BlockBase));};
            if(!before(0x810124u)&&g32(0x810124u))++coverage["47FD40 bind"];
            bool decoded=false;for(unsigned b=0;b<5;++b)if(g32(0x8101c0u+b*0x9cu)!=before(0x8101c0u+b*0x9cu))decoded=true;
            if(decoded)++coverage["47FD40 decode"];
            const std::uint8_t f0=mirrors[0].bytes[0x81044cu-PcRecordState::BlockBase],f1=*reinterpret_cast<const std::uint8_t*>(0x81044cu);
            if(f0!=f1)++coverage["47FD40 81044C="+std::to_string(f1)];
        }
        if(entry==15u||entry==16u){
            auto before=[&](std::uint32_t a){return *reinterpret_cast<const std::uint32_t*>(mirrors[0].bytes.data()+(a-PcRecordState::BlockBase));};
            const std::uint32_t c0=before(0x810110u),c1=g32(0x810110u);
            const float spd=*reinterpret_cast<const float*>(Player+0x2c8);
            std::string k=c1==0u&&c0!=0u?"reset":c1>c0?(spd==0.0f?"sample idle":"sample driving"):"none";
            ++coverage[std::string(entry==15u?"47F330 ":"47F780 ")+k];
        }
        if(entry==11u&&g32(0x810128u)!=*reinterpret_cast<const std::uint32_t*>(mirrors[0].bytes.data()+(0x810128u-PcRecordState::BlockBase)))++coverage["47FF60 route change"];
        if(entry==0){
            const std::uint32_t S0=0x80fb48u+slot*0xa4u,rec=*reinterpret_cast<const std::uint32_t*>(mirrors[0].bytes.data()+(S0+0x90u-PcRecordState::BlockBase));
            std::string path="other";
            if(rec==Records+slot*0x1000u&&g32(rec)==0x544f4851u){
                const std::uint32_t nn=std::uint32_t(std::int32_t(*reinterpret_cast<const std::int8_t*>(rec+0x33)));
                const bool same_level=std::int32_t(*reinterpret_cast<const std::int8_t*>(rec+0x2f))==std::int32_t(*reinterpret_cast<const std::uint32_t*>(mirrors[4].bytes.data()+0x68));
                path=same_level&&nn&&frame%nn?"interpolated":"stream";
            }
            ++coverage["480220 "+path];
        }
    }
    std::cout<<"{\"routine\":\""<<only<<"\",\"coverage\":{";bool first=true;
    for(const auto& [k,v]:coverage){std::cout<<(first?"":",")<<"\""<<k<<"\":"<<v;first=false;}
    std::cout<<"},\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}"<<std::endl;
    return mismatches?1:0;
}

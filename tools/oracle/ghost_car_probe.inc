// Included inside the compare_pc dispatcher; uses its private x86 harness.
if(only=="ghost_car_probe"){
    // Time Attack ghost cars (race_ghost_car.cpp) against the original:
    //   0 467E30 playback (every branch: game modes, variants, countdown,
    //     key/interpolated frames, spline fits, the area-matrix transform)
    //   1 4ACE40 control (467E30 + 466250 + the +B68 alpha ramp)
    //   2 4AD000 init (4874F0, 46F350 with 4866C0, the 841FA8 queue)
    //   3 466250   4 4F6290 + 454FC0 on random samples   5 4ACFC0 queue
    // Leaves: 49B2D0 countdown and 4962A0 return scripted values; 4401D0,
    // 4A2650, 440BD0, 440BA0 are capture stubs recording {pc, args}. The
    // matrix leaves (409EF0, 40A0A0, 40A3E0/410/440, 40A0D0, 40A010,
    // 409F90, 40A7D0, 40A220, 449640) run as original code on a matrix arena
    // (the native side runs the ported ones). Playback records: retail
    // gc_default_TA_%02d.rec (OR2_GHOSTS_DIR), else synthetic packets.
    // Compared: 7F8D80..7F9420, 841FA8..841FF0, 64BFEC/64BFED, the four car
    // works 7815A0.., the matrix arena and 89B564/89B568, the service log, EAX.
    using namespace outrun::platform;
    if(argc<=7||std::string(argv[7])=="-")throw std::runtime_error("ghost car probe requires the pinned d3dx9_29.dll (D3DX rotations)");
    init_matrix_oracle_globals();wire_d3dx_imports(map_d3dx29(argv[7]));
    constexpr unsigned base=0x3b800000u,stubs=base,trace=base+0x4000u,values=base+0x8000u,Ghosts=base+0x10000u;
    map_at(base,0x80000,PROT_READ|PROT_WRITE|PROT_EXEC);map_original_arithmetic_bridge(file);
    {   // 466E50 (4671D0) uses D3DXMatrixInverse; its generic body imports msvcrt _finite.
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
    {   unsigned restored=0;const std::size_t text_file=pe_file_offset(file,0x401000u);
        for(std::uint32_t a=0x401000u;a<0x401000u+0x194881u;a+=0x100u){
            const std::size_t n=std::min<std::size_t>(0x100u,0x401000u+0x194881u-a);
            if(std::memcmp(reinterpret_cast<void*>(a),file.data()+text_file+(a-0x401000u),n)){r060_raw_write(a,file.data()+text_file+(a-0x401000u),n);++restored;}
        }
        std::cerr<<"restored "<<restored<<" patched .text blocks"<<char(10);
    }
    for(std::uint32_t page=0x62f000u;page<0x98d000u;page+=0x1000u)mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE);
    auto emit32=[](std::vector<std::uint8_t>& b,std::uint32_t v){for(unsigned k=0;k<4;++k)b.push_back(std::uint8_t(v>>(k*8)));};
    struct Svc{std::uint32_t pc;unsigned argc;bool value;};
    const std::vector<Svc> services{{0x49b2d0,0,true},{0x4962a0,0,true},{0x4401d0,1,false},{0x4a2650,1,false},{0x440bd0,2,false},{0x440ba0,1,false},
        {0x48b310,0,true},{0x4505d0,0,true},{0x451180,1,true},{0x450610,2,true},{0x450320,0,true},{0x424940,1,false},{0x440200,1,false},{0x440180,2,false},
        {0x580253,1,true},{0x580bc2,1,false}};
    std::map<std::uint32_t,unsigned> index_of;unsigned stub_off=0;
    for(std::size_t k=0;k<services.size();++k){
        const auto& e=services[k];index_of[e.pc]=unsigned(k);
        std::vector<std::uint8_t> c{0x57,0x8b,0x3d};emit32(c,trace);                         // push edi; mov edi,[count]
        c.insert(c.end(),{0xc1,0xe7,0x04,0x81,0xc7});emit32(c,trace+64);                    // shl edi,4; add edi,entries
        c.insert(c.end(),{0xc7,0x07});emit32(c,e.pc);
        for(unsigned i=0;i<e.argc;++i)c.insert(c.end(),{0x8b,0x44,0x24,std::uint8_t(8+4*i),0x89,0x47,std::uint8_t(4+4*i)});
        c.insert(c.end(),{0xff,0x05});emit32(c,trace);c.push_back(0x5f);
        if(e.value){c.push_back(0xa1);emit32(c,values+4*unsigned(k));}else c.insert(c.end(),{0x31,0xc0});
        c.push_back(0xc3);
        std::memcpy(reinterpret_cast<void*>(stubs+stub_off),c.data(),c.size());
        r046_write_rel32(e.pc,0xe9,stubs+stub_off);stub_off=(stub_off+unsigned(c.size())+15u)&~15u;
    }
    std::vector<std::vector<std::uint8_t>> retail;
    if(const char* dir=std::getenv("OR2_GHOSTS_DIR"))for(unsigned k=1;k<=0x54;++k){
        char name[64];std::snprintf(name,sizeof name,"/gc_default_TA_%02u.rec",k);
        std::ifstream f(std::string(dir)+name,std::ios::binary);
        retail.emplace_back((std::istreambuf_iterator<char>(f)),std::istreambuf_iterator<char>());
    }
    std::mt19937 rng(0x4ace4000u);
    auto rnd=[&](std::uint32_t n){return n?std::uint32_t(rng()%n):0u;};
    auto real=[&](float a,float b){return a+(b-a)*float(rng()%100000)/100000.f;};
    auto g32=[](std::uint32_t a){return *reinterpret_cast<std::uint32_t*>(a);};
    auto p32=[](std::uint32_t a,std::uint32_t v){*reinterpret_cast<std::uint32_t*>(a)=v;};
    auto pf=[](std::uint32_t a,float v){std::memcpy(reinterpret_cast<void*>(a),&v,4);};
    auto p16=[](std::uint32_t a,std::uint16_t v){std::memcpy(reinterpret_cast<void*>(a),&v,2);};
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
    std::vector<Mirror> mirrors{{0x7f8d80u,0x7f9420u,{}},{0x841fa8u,0x841ff0u,{}},{0x64bfecu,0x64bfeeu,{}},{Cars,Cars+4u*CarBytes,{}},
        {Player,Player+CarBytes,{}},{Ghosts,Ghosts+3u*0x9cacu,{}},{0x78026cu,0x780270u,{}},{0x780258u,0x78025cu,{}},{0x656234u,0x656238u,{}},
        {0x799d18u,0x799d1cu,{}},{0x7d2da0u,0x7d2de0u,{}},{0x799d54u,0x799d58u,{}},{base+0x30000u,base+0x30000u+0x9cacu,{}},{base+0x60000u,base+0x69e00u,{}},{0x7c23e0u,0x7c2400u,{}},{0x79fb48u,0x79fb60u,{}}};
    auto make_record=[&](unsigned code)->std::vector<std::uint8_t>{
        if(!retail.empty()&&!retail[code%retail.size()].empty())return retail[code%retail.size()];
        std::vector<std::uint8_t> r(0x120u+0x1cu+40u*0x10u,0);for(auto& b:r)b=std::uint8_t(rng());
        std::memcpy(r.data(),"NHOT",4);const std::uint32_t frames=41;std::memcpy(r.data()+0xf8,&frames,4);
        return r;
    };
    for(unsigned iter=0;iter<count;++iter){
        unsigned entry=iter%12u;
        if(const char* e=std::getenv("OR2_GHOST_CAR_ENTRY"))entry=unsigned(std::stoul(e));
        std::memset(reinterpret_cast<void*>(trace),0,0x4000);
        std::array<std::uint32_t,16> value{};value[0]=rnd(4)?rnd(0x3c):0x3cu+rnd(200);value[1]=rnd(2);
        value[6]=rnd(2);value[7]=rnd(4)?0x259u+rnd(0x10000):rnd(0x300);value[8]=rnd(2000000);value[9]=rng();value[10]=rng();value[14]=base+0x60000u;
        std::memcpy(reinterpret_cast<void*>(values),value.data(),sizeof value);
        // ghost records
        std::memset(reinterpret_cast<void*>(Ghosts),0,3*0x9cac);
        for(unsigned s=0;s<3;++s)if(rnd(6)){const auto r=make_record(rnd(0x54));std::memcpy(reinterpret_cast<void*>(Ghosts+s*0x9cac),r.data(),std::min<std::size_t>(r.size(),0x9cac));}
        // module block
        for(std::uint32_t a=0x7f8d80u;a<0x7f9420u;a+=4)p32(a,rnd(3)?0u:rng());
        p32(0x7f9224u,(entry==3&&rnd(10)==0)?0u:Ghosts);   // 467E30 has no null check (467AC0 always allocates)
        p32(0x7f9220u,rnd(3)?(rnd(2)?rnd(16):16u*rnd(4)):rnd(0x80));
        for(unsigned s=0;s<3;++s){
            const std::uint32_t g=Ghosts+s*0x9cac,count=g32(g+0xf8);
            p32(0x7f91e4u+s*4,rnd(8)?(count?rnd(count+1):0u):rng()%0x1000u);
            for(unsigned k=0;k<3;++k){const std::uint32_t S=0x7f8d88u+s*0x78u+k*0x28u;
                for(unsigned w=0;w<4;++w)pf(S+w*4,real(-100,100));
                p32(S+0x10,rnd(40));p32(S+0x14,rnd(3)?rnd(8):0u);pf(S+0x18,real(-2000,2000));pf(S+0x1c,real(-2000,2000));p32(S+0x20,rnd(40));p32(S+0x24,rnd(40));
                pf(S+0xc,real(-2000,2000));}
            const std::uint32_t pv=0x7f9390u+s*0x30u;
            for(unsigned w=0;w<3;++w)pf(pv+w*4,real(-3000,3000));
            for(unsigned w=0;w<5;++w)p16(pv+0xc+w*2,std::uint16_t(rng()));
            pf(pv+0x14,real(0,90));for(unsigned w=0;w<3;++w)pf(pv+0x18+w*4,real(-2,2));
            for(unsigned w=0;w<6;++w)p16(pv+0x24+w*2,std::uint16_t(rng()));
        }
        for(std::uint32_t a=0x841fa8u;a<0x841ff0u;a+=4)p32(a,rng());
        *reinterpret_cast<std::uint8_t*>(0x841fa8u)=std::uint8_t(rnd(4));*reinterpret_cast<std::uint8_t*>(0x841fa9u)=std::uint8_t(rnd(4));
        for(unsigned q=0;q<4;++q){p32(0x841fb0u+q*16,9+q);*reinterpret_cast<std::uint8_t*>(0x841fb8u+q*16)=std::uint8_t(rnd(30));}
        *reinterpret_cast<std::uint16_t*>(0x64bfecu)=std::uint16_t(rng());
        // cars
        for(std::uint32_t a=Cars;a<Cars+4*CarBytes;a+=4)p32(a,rnd(4)?0u:rng());
        for(unsigned k=0;k<4;++k){const std::uint32_t car=Cars+k*CarBytes;p32(car,9+k);
            for(unsigned w=0;w<3;++w)pf(car+0x14+w*4,real(-3000,3000));pf(car+0xb68,rnd(4)?real(0,1):real(0.2f,0.8f));
            p16(car+0x260,std::uint16_t(rnd(40)));*reinterpret_cast<std::uint8_t*>(car+0x11)=std::uint8_t(rnd(30));}
        for(std::uint32_t a=Player;a<Player+CarBytes;a+=4)p32(a,rnd(4)?0u:rng());
        p16(Player+0x260,std::uint16_t(rnd(40)));p32(Player+0x5c,rnd(2));
        p32(0x799d18u,Player);
        p32(0x78026cu,rnd(6)?(rnd(2)?0x10u:0x12u):rnd(0x20));
        p32(0x780258u,rnd(5)?7u:rnd(10));
        p32(0x656234u,rnd(3)?rnd(0x3c):0x3cu+rnd(0x18));
        for(unsigned k=0;k<16;++k)pf(0x7d2da0u+k*4,k==15?1.f:(k>=12?real(-500,500):real(-1,1)));
        const unsigned slot=entry==1||entry==2?rnd(4):rnd(4);
        const std::uint32_t car=Cars+slot*CarBytes;
        std::uint32_t a0=0,a1=0;float fv=0;
        if(entry==4){a0=0x7f8d88u+rnd(3)*0x78u;fv=real(-3000,3000);a1=rnd(3)?rnd(40):rng();}
        if(entry==5){a0=9+rnd(4);a1=rng();}
        for(auto& mi:mirrors){mi.bytes.resize(mi.end-mi.base);std::memcpy(mi.bytes.data(),reinterpret_cast<void*>(mi.base),mi.bytes.size());}
        reset_matrix_oracle_globals();
        std::vector<std::uint8_t> arena(0x800);outrun::driving::Bytes ar(arena.data(),arena.size());
        for(unsigned k=0;k<16;++k)ar.putf(k*4,k%5==0?1.f:real(-0.5f,0.5f));
        std::memcpy(reinterpret_cast<void*>(MatrixArena),arena.data(),arena.size());
        // original
        std::uint32_t guest_eax=0;float guest_st0=0;
        switch(entry){
        case 0:prepare(0x467e30u);reinterpret_cast<unsigned*>(S)[0]=car;reinterpret_cast<unsigned*>(S)[1]=slot;break;
        case 1:prepare(0x4ace40u);reinterpret_cast<unsigned*>(S)[0]=car;break;
        case 2:prepare(0x4ad000u);reinterpret_cast<unsigned*>(S)[0]=car;break;
        case 3:prepare(0x466250u);reinterpret_cast<unsigned*>(S)[0]=slot;break;
        case 4:if(rnd(2)){prepare(0x4f6290u);reinterpret_cast<unsigned*>(S)[0]=a0;std::memcpy(reinterpret_cast<unsigned*>(S)+1,&fv,4);reinterpret_cast<unsigned*>(S)[2]=a1;}
               else{prepare(0x454fc0u);reinterpret_cast<unsigned*>(S)[0]=a0;reinterpret_cast<unsigned*>(S)[1]=a1;a0|=0x80000000u;guest_call.st0=1;}
               break;
        case 6:prepare(0x4666a0u);p32(0x799d54u,Cars+rnd(4)*CarBytes);*reinterpret_cast<std::uint8_t*>(0x64bfecu)=std::uint8_t(rnd(3)?1:0);
            *reinterpret_cast<std::uint8_t*>(0x7f922cu)=std::uint8_t(rnd(4)?0:1);if(rnd(3)==0){std::memcpy(reinterpret_cast<void*>(Ghosts+0x9cac),reinterpret_cast<void*>(Ghosts),0x120);}
            for(auto& mi:mirrors)std::memcpy(mi.bytes.data(),reinterpret_cast<void*>(mi.base),mi.bytes.size());break;
        case 7:{prepare(0x4671d0u);reinterpret_cast<unsigned*>(S)[0]=Player;
            const std::uint32_t work=Ghosts;p32(0x7f9228u,work);   // work record (reuses the ghost buffer region)
            const std::uint32_t cur=work+0x120u+rnd(0x40)*8u;p32(0x7f8d84u,work+0x120u);p32(0x7f91f0u,cur);p32(0x7f8ef4u,rnd(4)?rnd(0x40):0u);p32(0x7f8ef8u,0);
            for(unsigned w=0;w<3;++w)pf(Player+0x14+w*4,real(-3000,3000));for(unsigned w:{0x2cu,0x2eu,0x30u,0x260u,0x40u,0x32u,0x44u})p16(Player+w,std::uint16_t(rng()));
            pf(Player+0x2c8,real(0,90));for(unsigned w=0;w<6;++w)pf(Player+0x2d8+w*4,real(-2,2));
            for(auto& mi:mirrors)std::memcpy(mi.bytes.data(),reinterpret_cast<void*>(mi.base),mi.bytes.size());break;}
        case 8:case 9:case 10:{
            constexpr std::uint32_t Work=base+0x30000u;
            p32(0x7f9228u,Work);
            {const auto r=make_record(rnd(0x54));std::memset(reinterpret_cast<void*>(Work),0,0x9cac);std::memcpy(reinterpret_cast<void*>(Work),r.data(),std::min<std::size_t>(r.size(),0x9cac));}
            p32(0x7f8ef4u,rnd(8)?*reinterpret_cast<std::uint32_t*>(Work+0xf8):0u);p32(0x7f8ef8u,rnd(8)?0u:1u);p32(0x7f92b8u,rnd(2));
            p32(0x7f91f4u,rnd(3)?0u:1u);p32(0x7f91f0u,Work+0x120u+rnd(0x20)*8u);p32(0x7f8d84u,Work+0x120u);
            for(std::uint32_t a=0x7c23e0u;a<0x7c2400u;++a)*reinterpret_cast<std::uint8_t*>(a)=std::uint8_t(0x41+rnd(26));
            *reinterpret_cast<std::uint16_t*>(0x656234u)=std::uint16_t(rnd(3)?rnd(0x54):0x8000u|rng());
            for(unsigned w=0;w<3;++w)pf(Player+0x14+w*4,real(-3000,3000));for(unsigned w:{0x2cu,0x2eu,0x30u,0x260u,0x40u,0x32u,0x44u})p16(Player+w,std::uint16_t(rng()));
            for(std::uint32_t a=0x79fb48u+9u;a<0x79fb48u+13u;++a)*reinterpret_cast<std::uint8_t*>(a)=std::uint8_t(rnd(4));
            for(auto& mi:mirrors)std::memcpy(mi.bytes.data(),reinterpret_cast<void*>(mi.base),mi.bytes.size());
            prepare(entry==8?0x467190u:entry==9?0x466120u:0x467e00u);break;}
        case 11:p32(0x7f9224u,rnd(3)?Ghosts:0u);p32(0x7f9228u,rnd(3)?Ghosts+0x9cacu:0u);
            for(auto& mi:mirrors)std::memcpy(mi.bytes.data(),reinterpret_cast<void*>(mi.base),mi.bytes.size());prepare(0x465fa0u);break;
        default:prepare(0x4acfc0u);{auto* s=reinterpret_cast<unsigned*>(S);s[0]=a0;s[1]=a1;s[2]=rng();s[3]=rng();a1=s[1];}break;
        }
        const std::array<std::uint32_t,4> stack_args{reinterpret_cast<unsigned*>(S)[0],reinterpret_cast<unsigned*>(S)[1],reinterpret_cast<unsigned*>(S)[2],reinterpret_cast<unsigned*>(S)[3]};
        run();guest_eax=guest_call.out_eax;std::memcpy(&guest_st0,&guest_call.out_st0,4);
        // native
        PcRaceMemory mem;std::vector<std::vector<std::uint8_t>> native(mirrors.size());
        for(std::size_t i=0;i<mirrors.size();++i){native[i]=mirrors[i].bytes;mem.map(mirrors[i].base,native[i].data(),native[i].size());}
        ghost_map_tables(mem);ghost_map_car_tables(mem);outrun::driving::PcMatrixStack stack{ar,0,0,64};   // 46F350 reads 650500
        struct NCall{std::uint32_t pc;decltype(PcRaceCall::args) a;};std::vector<NCall> calls;
        PcRaceContext ctx{mem,stack,[&](const PcRaceCall& k)->std::uint32_t{
            if(k.pc==0x48b320u)return mem.u32(0x656234u);                  // original 48B320 in the guest
            const auto it=index_of.find(k.pc);if(it==index_of.end())throw std::runtime_error("ghost car probe: unexpected service");
            calls.push_back({k.pc,k.args});return services[it->second].value?value[it->second]:0u;},nullptr};
        std::uint32_t native_eax=0;float native_st0=0;bool ok=true;std::string why;
        try{
            switch(entry){
            case 0:native_eax=ghost_play_467e30(ctx,car,slot);break;
            case 1:ghost_car_control_4ace40(ctx,car);guest_eax=0;break;
            case 2:ghost_car_init_4ad000(ctx,car);guest_eax=0;break;
            case 3:native_eax=ghost_state_466250(mem,slot);break;
            case 4:if(!(a0&0x80000000u)){ghost_spline_fit_4f6290(mem,a0,fv,a1);guest_eax=0;}
                   else{native_st0=ghost_spline_454fc0(mem,a0&0x7fffffffu,a1);guest_eax=0;++cases;
                        if(std::memcmp(&native_st0,&guest_st0,4)){std::ostringstream s;s<<"454FC0 run "<<iter<<" PC="<<guest_st0<<" native="<<native_st0;fail(s.str());}}
                   break;
            case 6:ghost_frame_4666a0(ctx);guest_eax=0;break;
            case 7:ghost_record_4671d0(ctx,Player);guest_eax=0;break;
            case 8:ghost_goal_467190(ctx);guest_eax=0;break;
            case 9:ghost_record_reset_466120(mem);guest_eax=0;break;
            case 10:ghost_restart_467e00(ctx);guest_eax=0;break;
            case 11:ghost_release_465fa0(ctx);guest_eax=0;break;
            default:ghost_car_queue_4acfc0(mem,stack_args[0],std::uint8_t(stack_args[1]),stack_args[2],std::uint8_t(stack_args[3]));guest_eax=0;break;
            }
        }catch(const std::exception& e){ok=false;why=e.what();}
        const std::string tag="entry "+std::to_string(entry)+" run "+std::to_string(iter);
        ++cases;if(!ok){fail(tag+" native threw: "+why);continue;}
        if(inject&&iter==7)native[3][0x14]^=1;
        for(std::size_t i=0;i<mirrors.size();++i){std::ostringstream s;s<<tag<<" region "<<std::hex<<mirrors[i].base;
            compare(native[i].data(),reinterpret_cast<void*>(mirrors[i].base),native[i].size(),s.str());}
        compare(arena.data(),reinterpret_cast<void*>(MatrixArena),arena.size(),tag+" matrix arena");
        {std::array<std::uint32_t,2> sp{MatrixArena+std::uint32_t(stack.current_offset),std::uint32_t(stack.depth)};
         compare(sp.data(),reinterpret_cast<void*>(0x89b564),8,tag+" matrix pointer/depth");}
        ++cases;if(entry==0||entry==3)if(native_eax!=guest_eax){std::ostringstream s;s<<tag<<" EAX PC="<<guest_eax<<" native="<<native_eax;fail(s.str());}
        const unsigned n=g32(trace);++cases;
        if(n!=calls.size())fail(tag+" service count PC="+std::to_string(n)+" native="+std::to_string(calls.size()));
        else for(unsigned i=0;i<n;++i){const auto* g=reinterpret_cast<const std::uint32_t*>(trace+64+i*16);
            const auto& e=services[index_of[g[0]]];bool same=g[0]==calls[i].pc;
            for(unsigned k=0;k<e.argc;++k)same=same&&g[1+k]==calls[i].a[k];
            if(!same){std::ostringstream s;s<<tag<<" service #"<<i<<" "<<std::hex<<g[0];fail(s.str());break;}}
        ++coverage["entry "+std::to_string(entry)];
        if(entry==0&&value[0]<=0x3c&&(g32(0x78026cu)==0x10u||g32(0x78026cu)==0x12u)){const std::uint32_t S0=0x7f8d88u+slot*0x78u;
            if(slot<3)++coverage[std::string("467E30 splines ")+(std::int32_t(*reinterpret_cast<std::uint32_t*>(mirrors[0].bytes.data()+(S0+0x14-0x7f8d80u)))>3?"eval":"quadratic")];
            ++coverage[std::string("467E30 matrix ")+(std::int32_t(g32(0x656234u))<0x3c?"on":"off")];}
        if(entry==0)++coverage[std::string("467E30 ")+(g32(0x78026cu)!=0x10u&&g32(0x78026cu)!=0x12u?"mode":value[0]>0x3c?"countdown":(g32(0x7f9220u)&0xf)?"interpolated":"key")];
    }
    std::cout<<"{\"routine\":\""<<only<<"\",\"coverage\":{";bool first=true;
    for(const auto& [k,v]:coverage){std::cout<<(first?"":",")<<"\""<<k<<"\":"<<v;first=false;}
    std::cout<<"},\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}"<<std::endl;
    return mismatches?1:0;
}

// Included inside the compare_pc dispatcher; uses its private x86 harness.
if(only=="scene_effects_probe"){
    // Original event 387 SCN_EFC (4AFBB0 init, 4AFDC0 control, 4AFC70 destroy,
    // parts 4AF5B0/4AF630/4AFCC0, the AREA/player-car setters 4AF550..4AFD90)
    // against race_scene_effects, and the three D3DX generic paths it needs
    // (D3DXMatrixScaling 441288, PerspectiveFovRH 441E96, LookAtRH 441AF6)
    // against pc_d3dx. The module code runs unmodified (the 4AF580 protected
    // snippet 4480BF live with the protector sections mapped); the matrix
    // leaves run the original code over the pinned D3DX generic routines.
    // Every mirrored region (work 780280, 79F5EC, parameters 842130, light
    // table 899B98, 799D18 and the car fixture), the matrix arena and the
    // stack pointer/depth are compared.
    using namespace outrun::platform;
    if(argc<=7||std::string(argv[7])=="-")throw std::runtime_error("scene effects oracle requires pinned d3dx9_29.dll");
    map_original_arithmetic_bridge(file);
    {   unsigned restored=0;
        const std::size_t text_file=pe_file_offset(file,0x401000u);
        for(std::uint32_t a=0x401000u;a<0x401000u+0x194881u;a+=0x100u){
            const std::size_t n=std::min<std::size_t>(0x100u,0x401000u+0x194881u-a);
            if(std::memcmp(reinterpret_cast<void*>(a),file.data()+text_file+(a-0x401000u),n)){r060_raw_write(a,file.data()+text_file+(a-0x401000u),n);++restored;}
        }
        std::cerr<<"restored "<<restored<<" patched .text blocks"<<char(10);
    }
    init_matrix_oracle_globals();wire_d3dx_imports(map_d3dx29(argv[7]));
    {   auto* page=reinterpret_cast<void*>(0x596000u);
        if(mprotect(page,4096,PROT_READ|PROT_WRITE))throw std::runtime_error("IAT scene effects");
        *reinterpret_cast<std::uint32_t*>(0x5962b4u)=D3DX_BASE+(0x441288u-0x400000u);   // D3DXMatrixScaling
        *reinterpret_cast<std::uint32_t*>(0x5962b8u)=D3DX_BASE+(0x441af6u-0x400000u);   // D3DXMatrixLookAtRH
        *reinterpret_cast<std::uint32_t*>(0x5962d0u)=D3DX_BASE+(0x441e96u-0x400000u);   // D3DXMatrixPerspectiveFovRH
        if(mprotect(page,4096,PROT_READ))throw std::runtime_error("IAT scene effects");
    }
    constexpr unsigned base=0x3a800000u,fix=base+0x40000u;
    map_at(base,0x80000,PROT_READ|PROT_WRITE|PROT_EXEC);
    for(unsigned page:{0x780000u,0x799000u,0x79f000u,0x842000u,0x899000u,0x89a000u,0x89b000u})
        if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("scene effects globals");
    std::mt19937 rng(0x4afbb000u);
    auto rnd=[&](std::uint32_t n){return n?std::uint32_t(rng()%n):0u;};
    auto real=[&](float lo,float hi){return lo+(hi-lo)*(float(rng()&0xffffu)/65535.f);};
    auto g32=[](std::uint32_t a){return *reinterpret_cast<std::uint32_t*>(a);};
    auto p32=[](std::uint32_t a,std::uint32_t v){*reinterpret_cast<std::uint32_t*>(a)=v;};
    auto p16=[](std::uint32_t a,std::uint16_t v){*reinterpret_cast<std::uint16_t*>(a)=v;};
    auto pf=[](std::uint32_t a,float v){*reinterpret_cast<float*>(a)=v;};
    unsigned cases{},mismatches{};const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
    std::map<std::string,unsigned> coverage;
    auto fail=[&](const std::string& what){++mismatches;if(mismatches<20)std::cerr<<what<<"\n";};
    auto compare=[&](const void* native,const void* original,std::size_t n,const std::string& what){
        ++cases;if(!std::memcmp(native,original,n))return;
        const auto* a=static_cast<const std::uint8_t*>(native);const auto* b=static_cast<const std::uint8_t*>(original);
        for(std::size_t i=0;i<n;++i)if(a[i]!=b[i]){std::ostringstream s;s<<what<<" offset=0x"<<std::hex<<i<<" PC="<<unsigned(b[i])<<" native="<<unsigned(a[i]);fail(s.str());break;}
    };
    // ---- EXE parameter table 5C4EA0 ------------------------------------------
    {   std::vector<std::uint32_t> exe,nat;
        for(std::uint32_t a=0x5c4ea0u;g32(a);a+=16){exe.push_back(g32(a+4));exe.push_back(g32(a+8));}
        for(const auto& r:SceneEffectsParameters5c4ea0){nat.push_back(r.target);nat.push_back(r.value);}
        ++cases;if(exe!=nat)fail("5C4EA0 parameter table differs from the EXE");
    }
    // ---- D3DX generic paths ----------------------------------------------------
    const unsigned OUT=fix+0x8000u,VEC=fix+0x8100u;
    auto special=[&](float v){switch(rnd(24)){case 0:return 0.f;case 1:return -0.f;case 2:return 1e-30f;case 3:return 3e37f;
        case 4:return std::numeric_limits<float>::quiet_NaN();case 5:return std::numeric_limits<float>::infinity();default:return v;}};
    for(unsigned i=0;i<count;++i){
        const unsigned kind=i%4u;
        std::array<std::uint8_t,64> nat{};for(auto& b:nat)b=std::uint8_t(rng());
        std::memcpy(reinterpret_cast<void*>(OUT),nat.data(),64);
        Bytes out(nat.data(),64);
        if(kind==0){
            const float sx=special(real(-50,50)),sy=special(real(-50,50)),sz=special(real(-50,50));
            prepare(D3DX_BASE+(0x441288u-0x400000u));
            auto* a=reinterpret_cast<unsigned*>(S);a[0]=OUT;std::memcpy(&a[1],&sx,4);std::memcpy(&a[2],&sy,4);std::memcpy(&a[3],&sz,4);
            run_original32();pc_d3dx_matrix_scaling(out,sx,sy,sz);
            if(guest_call.out_sp!=S+16){fail("441288 stack");++cases;continue;}
        }else if(kind==1){
            float fovy=rnd(4)?real(0.05f,3.1f):special(real(-7,7)),aspect=rnd(8)?real(0.3f,4.f):special(real(-3,3));
            float zn=rnd(8)?real(0.01f,20.f):special(real(-5,5)),zf=rnd(8)?real(20.f,90000.f):special(zn);
#if OR2_X87_FAST
            {   // The fast x87 model follows SSE when both operands of one
                // operation are NaN (first operand) where the x87 returns the
                // larger-significand NaN: keep at most one NaN source.
                const bool nf=std::isnan(fovy)||!std::isfinite(fovy)||std::fabs(fovy)>=0x1p63f;
                if(nf+std::isnan(aspect)+std::isnan(zn)+std::isnan(zf)>1){fovy=real(0.05f,3.1f);aspect=real(0.3f,4.f);zn=real(0.01f,20.f);zf=real(20.f,90000.f);}
            }
#endif
            prepare(D3DX_BASE+(0x441e96u-0x400000u));
            auto* a=reinterpret_cast<unsigned*>(S);a[0]=OUT;std::memcpy(&a[1],&fovy,4);std::memcpy(&a[2],&aspect,4);std::memcpy(&a[3],&zn,4);std::memcpy(&a[4],&zf,4);
            run_original32();pc_d3dx_perspective_fov_rh(out,fovy,aspect,zn,zf);
            if(std::getenv("OR2_SCN_DEBUG")&&std::memcmp(nat.data(),reinterpret_cast<void*>(OUT),64)){char b[160];std::snprintf(b,160,"FovRH case %u fovy=%a aspect=%a zn=%a zf=%a",i,fovy,aspect,zn,zf);std::cerr<<b<<std::endl;
                for(unsigned k=0;k<16;++k){float f;std::memcpy(&f,reinterpret_cast<void*>(OUT+k*4),4);std::snprintf(b,160," %u PC %a native %a",k,f,reinterpret_cast<float*>(nat.data())[k]);std::cerr<<b<<std::endl;}}
            if(guest_call.out_sp!=S+20){fail("441E96 stack");++cases;continue;}
        }else if(kind==3){
            std::array<float,16> ma{},mb{};
            for(auto* m:{&ma,&mb})for(auto& f:*m){const std::uint32_t u=rnd(3)?0u:rng();std::memcpy(&f,&u,4);if(!u)f=rnd(2)?real(-900,900):real(-1,1)*std::ldexp(1.f,int(rnd(200))-150);
#if OR2_X87_FAST
                if(std::isnan(f))f=real(-900,900);   // dual-NaN operands: see the FovRH note
#endif
            }
            std::memcpy(reinterpret_cast<void*>(VEC),ma.data(),64);std::memcpy(reinterpret_cast<void*>(VEC+64),mb.data(),64);
            prepare(D3DX_BASE+(0x440943u-0x400000u));
            auto* a=reinterpret_cast<unsigned*>(S);a[0]=OUT;a[1]=VEC;a[2]=VEC+64;
            run_original32();const auto r=pc_d3dx_matrix_multiply(ma,mb);std::memcpy(nat.data(),r.data(),64);
            if(guest_call.out_sp!=S+12){fail("440943 stack");++cases;continue;}
        }else{
            PcVec3 e{real(-900,900),real(-900,900),real(-900,900)},t{real(-900,900),real(-900,900),real(-900,900)},u{real(-1,1),real(-1,1),real(-1,1)};
            if(rnd(6)==0)u={0.f,1.f,0.f};
            if(rnd(20)==0)t=e;                                     // degenerate z axis
            if(rnd(20)==0){u={t[0]-e[0],t[1]-e[1],t[2]-e[2]};}      // up parallel to z
            if(rnd(30)==0)e[rnd(3)]=special(e[0]);
            std::memcpy(reinterpret_cast<void*>(VEC),e.data(),12);std::memcpy(reinterpret_cast<void*>(VEC+16),t.data(),12);std::memcpy(reinterpret_cast<void*>(VEC+32),u.data(),12);
            prepare(D3DX_BASE+(0x441af6u-0x400000u));
            auto* a=reinterpret_cast<unsigned*>(S);a[0]=OUT;a[1]=VEC;a[2]=VEC+16;a[3]=VEC+32;
            run_original32();pc_d3dx_look_at_rh(out,e,t,u);
            if(guest_call.out_sp!=S+16){fail("441AF6 stack");++cases;continue;}
        }
        ++coverage[kind==0?"d3dx_scaling":kind==1?"d3dx_fov_rh":kind==2?"d3dx_look_at_rh":"d3dx_multiply"];
        if(inject&&i==count-1)nat[5]^=1;
        compare(nat.data(),reinterpret_cast<void*>(OUT),64,std::string("D3DX ")+(kind==0?"441288":kind==1?"441E96":kind==2?"441AF6":"440943")+" case "+std::to_string(i));
    }
    // ---- SCN_EFC ---------------------------------------------------------------
    const unsigned CAR=fix;
    struct Mirror{std::uint32_t base,end;std::vector<std::uint8_t> bytes;};
    std::vector<Mirror> mirrors{{0x780280u,0x780434u,{}},{0x79f5ecu,0x79f5f0u,{}},{0x842130u,0x842198u,{}},{0x899b98u,0x89a4f8u,{}},
        {0x799d18u,0x799d1cu,{}},{CAR,CAR+0x100u,{}}};
    const unsigned W=0x780280u;
    auto fill_matrix=[&](std::uint32_t a){for(unsigned k=0;k<16;++k)pf(a+k*4,k==15?1.f:(k>=12?real(-500,500):real(-1.2f,1.2f)));};
    for(unsigned run=0;run<count;++run){
        const unsigned entry=run%20u;
        for(unsigned k=0;k<0x1b4;k+=4)p32(W+k,rnd(3)?rnd(4):rng());
        p32(W,rng()&(rnd(4)?0x1fu:0xffffffffu));
        for(unsigned k=0;k<3;++k){pf(W+0x10+k*4,real(-600,600));pf(W+0x1c+k*4,real(-100,100));pf(W+0x28+k*4,real(-1,1));}
        {const std::uint32_t targets[]={0,1,2,0,1};p32(W+0x94,targets[rnd(5)]);p32(W+0x98,rnd(4)?targets[rnd(5)]:rng());
         pf(W+0x9c,real(-0.5f,1.5f));
         const std::int16_t timers[]={-1,0,1,2,0x77,0x78,0x78,0x79,0x40,-5};p16(W+0xa4,std::uint16_t(rnd(3)?timers[rnd(10)]:std::int16_t(rng())));}
        p32(0x79f5ecu,W);
        for(std::uint32_t a=0x842130u;a<0x842198u;a+=4)pf(a,real(-10,10));
        for(std::uint32_t a:{0x84216cu,0x84217cu,0x842138u})pf(a,real(-400,400));
        for(std::uint32_t a:{0x842168u,0x842150u,0x842180u,0x842154u})pf(a,rnd(4)?real(0,200):real(-400,400));
        for(std::uint32_t a:{0x842130u,0x842194u,0x84214cu,0x842134u})pf(a,rnd(3)?real(-0.05f,0.05f):(rnd(2)?0.f:(rnd(2)?1e-8f:-1e-9f)));
        for(std::uint32_t a=0x899b98u;a<0x89a4f8u;a+=4)p32(a,rng());
        for(unsigned l=0;l<15;++l){const std::uint32_t L=0x899b98u+l*0xa0u;pf(L+0x64,rnd(4)?real(0,3.5f):real(-3,3));pf(L+0x68,rnd(4)?real(0,3.5f):(rnd(2)?-1.f:std::numeric_limits<float>::quiet_NaN()));}
        for(unsigned k=0;k<0x100;k+=4)p32(CAR+k,rng());
        p32(CAR+0x5c,rnd(4)?1u:0u);fill_matrix(CAR+0xb0);
        if(rnd(40)==0)pf(0x842160u,std::numeric_limits<float>::quiet_NaN());
        p32(0x799d18u,CAR);
        for(auto& m:mirrors){m.bytes.resize(m.end-m.base);std::memcpy(m.bytes.data(),reinterpret_cast<void*>(m.base),m.bytes.size());}
        reset_matrix_oracle_globals();
        std::vector<std::uint8_t> arena(0x800);Bytes ar(arena.data(),arena.size());
        for(unsigned k=0;k<16;++k)ar.putf(k*4,k%5==0?1.f:real(-0.5f,0.5f));
        std::memcpy(reinterpret_cast<void*>(MatrixArena),arena.data(),arena.size());
        const std::uint32_t arg=rnd(4);
        static const std::uint32_t setters[]={0x4af550u,0x4af560u,0x4af570u,0x4af580u,0x4af590u,0x4afb40u,0x4afb50u,0x4afb60u,0x4afb70u,0x4afb80u,0x4afd90u,0x4af5a0u,0x4afb90u,0x4afba0u};
        switch(entry){
        case 0:prepare(0x4afbb0);reinterpret_cast<unsigned*>(S)[0]=W;break;
        case 1:case 2:case 3:prepare(0x4afdc0);reinterpret_cast<unsigned*>(S)[0]=W;break;
        case 4:prepare(0x4afc70);reinterpret_cast<unsigned*>(S)[0]=W;break;
        case 5:prepare(0x4afcc0);guest_call.eax=W+0x90;break;
        default:prepare(setters[entry-6]);reinterpret_cast<unsigned*>(S)[0]=arg;break;
        }
        run_original32();
        const std::uint32_t original_eax=guest_call.out_eax;
        if(guest_call.out_sp!=S){fail("guest stack imbalance entry "+std::to_string(entry));++cases;continue;}
        PcRaceMemory mem;
        std::vector<std::vector<std::uint8_t>> native(mirrors.size());
        for(std::size_t i=0;i<mirrors.size();++i){native[i]=mirrors[i].bytes;mem.map(mirrors[i].base,native[i].data(),native[i].size());}
        PcMatrixStack stack{ar,0,0,64};
        PcRaceService service=[&](const PcRaceCall& k)->std::uint32_t{std::ostringstream e;e<<"SCN_EFC native called a service "<<std::hex<<k.pc;throw std::runtime_error(e.str());};
        PcRaceContext ctx{mem,stack,service,nullptr};
        std::uint32_t native_eax=0;bool eax_checked=false;
        const std::string tag="scn_efc entry "+std::to_string(entry)+" run "+std::to_string(run);
        try{
            switch(entry){
            case 0:scene_effects_init_4afbb0(ctx,W);break;
            case 1:case 2:case 3:scene_effects_control_4afdc0(ctx,W);break;
            case 4:scene_effects_destroy_4afc70(mem,W);break;
            case 5:scene_effects_transition_4afcc0(mem,W+0x90);break;
            default:{PcRaceCall k{};k.pc=setters[entry-6];k.argc=k.pc==0x4afd90u?1u:0u;k.args[0]=arg;
                if(!scene_effects_setter(mem,k,native_eax))throw std::runtime_error("setter dispatch");eax_checked=true;}
            }
        }catch(const std::exception& ex){++cases;fail(tag+" native threw: "+ex.what());continue;}
        ++coverage["entry_"+std::to_string(entry)];
        if(entry>=1&&entry<=3){const auto f=mirrors[0].bytes[0];if(f&1)++coverage["ctrl_rain"];if(f&2)++coverage["ctrl_transition"];if(f&8)++coverage["ctrl_headlights"];}
        if(eax_checked){++cases;if(native_eax!=original_eax)fail(tag+" EAX");}
        if(inject&&run==count-1)native[0][0x40]^=1;
        for(std::size_t i=0;i<mirrors.size();++i){
            std::ostringstream s;s<<tag<<" region "<<std::hex<<mirrors[i].base;
            compare(native[i].data(),reinterpret_cast<void*>(mirrors[i].base),native[i].size(),s.str());
        }
        compare(arena.data(),reinterpret_cast<void*>(MatrixArena),arena.size(),tag+" matrix arena");
        {std::array<std::uint32_t,2> sp{MatrixArena+std::uint32_t(stack.current_offset),std::uint32_t(stack.depth)};
         compare(sp.data(),reinterpret_cast<void*>(0x89b564),8,tag+" matrix pointer/depth");}
    }
    std::cerr<<"coverage:";for(const auto& [k,v]:coverage)std::cerr<<' '<<k<<'='<<v;std::cerr<<"\n";
    std::cout<<"{\"routine\":\""<<only<<"\",\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}"<<'\n';return mismatches?1:0;
}

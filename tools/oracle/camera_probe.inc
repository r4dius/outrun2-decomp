if(only=="camera_probe"){
    // Event181/function0D camera. Stage 1: the pinned d3dx9_29 generic
    // routines it depends on, executed directly from the mapped DLL.
    if(argc<=7||std::string(argv[7])=="-")throw std::runtime_error("camera oracle requires pinned d3dx9_29.dll");
    constexpr unsigned base=0x3a800000;
    map_at(base,0x10000,PROT_READ|PROT_WRITE|PROT_EXEC);map_original_arithmetic_bridge(file);
    init_matrix_oracle_globals();wire_d3dx_imports(map_d3dx29(argv[7]));
    {   // The DLL's msvcrt _finite import is not loaded in this harness. Supply
        // an exact equivalent (finite <=> exponent not all ones) at its IAT slot.
        static const std::uint8_t finite_stub[]{0x8b,0x44,0x24,0x08,0x25,0x00,0x00,0xf0,0x7f,0x3d,0x00,0x00,0xf0,0x7f,0x0f,0x95,0xc0,0x0f,0xb6,0xc0,0xc3};
        std::memcpy(reinterpret_cast<void*>(base+0x7000),finite_stub,sizeof(finite_stub));
        auto* iat=reinterpret_cast<void*>(D3DX_BASE+0x1000u);
        if(mprotect(iat,4096,PROT_READ|PROT_WRITE))throw std::runtime_error("d3dx IAT page");
        *reinterpret_cast<std::uint32_t*>(D3DX_BASE+0x1270u)=base+0x7000u;
        if(mprotect(iat,4096,PROT_READ))throw std::runtime_error("d3dx IAT restore");
    }
    constexpr unsigned inverse_va=D3DX_BASE+(0x440e9cu-0x400000u),perspective_va=D3DX_BASE+(0x441fccu-0x400000u);
    unsigned cases{},mismatches{};const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
    auto compare=[&](const void* native,unsigned address,std::size_t n,const char* what){
        ++cases;if(!std::memcmp(native,reinterpret_cast<void*>(address),n))return;
        ++mismatches;if(mismatches<9){const auto* a=static_cast<const std::uint8_t*>(native);
            for(std::size_t i=0;i<n;++i)if(a[i]!=reinterpret_cast<std::uint8_t*>(address)[i]){
                unsigned nw,pw;std::memcpy(&nw,a+(i&~3u),4);std::memcpy(&pw,reinterpret_cast<void*>(address+(i&~3u)),4);
                std::cerr<<what<<" case="<<cases<<" offset="<<std::hex<<i<<" PC="<<pw<<" native="<<nw<<std::dec<<"\n";break;}}
    };
    std::mt19937 rng(0x4f523181u);
    auto real=[&](float lo,float hi){return lo+(hi-lo)*(float(rng()&0xffffffu)/float(0xffffffu));};
    WorldNativeControl control(static_cast<unsigned short>(x87_control));
    for(unsigned i=0;i<20000;++i){
        std::array<float,16> m{};
        const unsigned kind=i%8u;
        for(auto& v:m)v=real(-4,4);
        if(kind==1){for(auto& v:m)v=0;m[0]=m[5]=m[10]=m[15]=1;m[12]=real(-900,900);m[13]=real(-50,50);m[14]=real(-900,900);}
        if(kind==2){const float a=real(-3.2f,3.2f);for(auto& v:m)v=0;m[0]=std::cos(a);m[2]=-std::sin(a);m[8]=std::sin(a);m[10]=std::cos(a);m[5]=1;m[15]=1;m[12]=real(-100,100);}
        if(kind==3)for(unsigned k=0;k<4;++k)m[4+k]=m[k]*2.f;           // singular rows
        if(kind==4){for(auto& v:m)v=real(-1e-12f,1e-12f);}               // tiny determinant
        if(kind==5){m[3]=std::numeric_limits<float>::infinity();}
        if(kind==6){for(auto& v:m)v=float(int(rng()%5)-2);}               // exact zeros/cancellation
        std::array<std::uint8_t,64> in{},out{};
        std::memcpy(in.data(),m.data(),64);
        for(unsigned k=0;k<64;++k)out[k]=std::uint8_t(0xa0+k+i);
        const bool in_place=i%3u==0u;const bool with_det=i%2u==0u;
        std::memcpy(reinterpret_cast<void*>(base+0x100),in_place?in.data():out.data(),64);
        std::memcpy(reinterpret_cast<void*>(base+0x200),in.data(),64);
        *reinterpret_cast<std::uint32_t*>(base+0x300)=0xdeadbeefu;
        prepare(inverse_va);auto* st=reinterpret_cast<unsigned*>(S);
        st[0]=base+0x100;st[1]=with_det?base+0x300:0u;st[2]=in_place?base+0x100:base+0x200;run_original32();
        const unsigned pc_result=guest_call.out_eax;
        float det=0;std::uint32_t det_bits=0xdeadbeefu;
        auto& target=in_place?in:out;
        const bool ok=in_place?pc_d3dx_matrix_inverse(Bytes(in.data(),64),with_det?&det:nullptr,Bytes(in.data(),64))
                              :pc_d3dx_matrix_inverse(Bytes(out.data(),64),with_det?&det:nullptr,Bytes(in.data(),64));
        if(with_det)std::memcpy(&det_bits,&det,4);
        if(inject&&i==19999)target[5]^=1;
        ++cases;if((pc_result!=0)!=ok){++mismatches;if(mismatches<9)std::cerr<<"inverse result case="<<i<<"\n";}
        compare(in_place?in.data():out.data(),base+0x100,64,"inverse matrix");
        compare(&det_bits,base+0x300,4,"inverse determinant");
    }
    for(unsigned i=0;i<20000;++i){
        const float l=real(-3,0),r=l+(i%97u==0u?0.f:real(0.001f,4)),b=real(-2,0),t=b+real(0.001f,3);
        const float zn=real(0.01f,5),zf=zn+(i%89u==0u?0.f:real(1,20000));
        std::array<std::uint8_t,64> out{};for(unsigned k=0;k<64;++k)out[k]=std::uint8_t(k*3+i);
        std::memcpy(reinterpret_cast<void*>(base+0x100),out.data(),64);
        prepare(perspective_va);auto* st=reinterpret_cast<unsigned*>(S);
        st[0]=base+0x100;std::memcpy(st+1,&l,4);std::memcpy(st+2,&r,4);std::memcpy(st+3,&b,4);std::memcpy(st+4,&t,4);std::memcpy(st+5,&zn,4);std::memcpy(st+6,&zf,4);
        run_original32();
        pc_d3dx_perspective_off_center_rh(Bytes(out.data(),64),l,r,b,t,zn,zf);
        compare(out.data(),base+0x100,64,"perspective off-center RH");
    }
    // Stage 2: original 484BD0 -> 483C10, 40A980/404310 (perspective, frustum,
    // D3DXPlaneFromPoints), 482F20 (4493E0/4B5FD0 blend, 40F480/449580, 40F180,
    // rotations, 40A310), 40A240 inverse. Only the Direct3D-facing 409DF0/409E00
    // are replaced by capture stubs; their stores are compared to native.
    {
        auto* iat=reinterpret_cast<void*>(0x596000u);
        if(mprotect(iat,4096,PROT_READ|PROT_WRITE))throw std::runtime_error("camera IAT");
        *reinterpret_cast<std::uint32_t*>(0x5962dcu)=inverse_va;
        *reinterpret_cast<std::uint32_t*>(0x5962e8u)=perspective_va;
        *reinterpret_cast<std::uint32_t*>(0x5962e0u)=D3DX_BASE+(0x442f1bu-0x400000u);
        if(mprotect(iat,4096,PROT_READ))throw std::runtime_error("camera IAT restore");
    }
    constexpr unsigned store_trace=base+0x8000,transform_trace=base+0x9000,camera=base+0x4000,owner=base+0x5000;
    {
        auto emit32=[](std::vector<std::uint8_t>& b,std::uint32_t v){for(unsigned k=0;k<4;++k)b.push_back(std::uint8_t(v>>(k*8)));};
        std::vector<std::uint8_t> a{0x56,0x57,0x51,0xa1};emit32(a,store_trace);
        a.insert(a.end(),{0x6b,0xc8,0x44,0x81,0xc1});emit32(a,store_trace+0x10);
        a.insert(a.end(),{0x8b,0x54,0x24,0x10,0x89,0x11,0x8d,0x79,0x04,0x8b,0x35,0x64,0xb5,0x89,0x00,0xb9,0x10,0,0,0,0xf3,0xa5,0x40,0xa3});
        emit32(a,store_trace);a.insert(a.end(),{0x59,0x5f,0x5e,0xc3});
        std::memcpy(reinterpret_cast<void*>(base+0x7100),a.data(),a.size());
        std::vector<std::uint8_t> t{0xa1};emit32(t,transform_trace);
        t.insert(t.end(),{0x8b,0x54,0x24,0x04,0x89,0x14,0x85});emit32(t,transform_trace+0x10);
        t.insert(t.end(),{0x40,0xa3});emit32(t,transform_trace);t.push_back(0xc3);
        std::memcpy(reinterpret_cast<void*>(base+0x7180),t.data(),t.size());
        r046_write_rel32(0x409df0u,0xe9,base+0x7100);r046_write_rel32(0x409e00u,0xe9,base+0x7180);
    }
    for(unsigned page:{0x740000u,0x95b000u,0x82e000u,0x79f000u,0x799000u,0x634000u})
        if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("camera globals");
    unsigned resets=0,wraps=0;
    for(unsigned i=0;i<4000;++i){
        std::array<std::uint8_t,0x400> cam{};
        Bytes c(cam.data(),cam.size());
        for(unsigned k=0;k<0x400;k+=4)c.putf(k,real(-3,3));
        c.putf(0xac,real(0.3f,1.6f));c.putf(0xb8,real(0.5f,2.2f));c.putf(0xbc,real(0.05f,5));c.putf(0xc0,real(100,20000));
        c.putf(0xc4,i%3u?0.f:real(-0.5f,0.5f));c.putf(0xc8,i%5u?0.f:real(-0.5f,0.5f));
        for(unsigned k=0;k<3;++k){c.putf(0xf8+k*4,real(-500,500));c.putf(0x104+k*4,c.f32(0xf8+k*4)+real(-20,20));
            c.putf(0x128+k*4,i%4u==0u?real(-12,12):real(-3.1f,3.1f));
            const bool far_jump=i%7u==0u;
            c.putf(0x390+k*4,c.f32(0xf8+k*4)+(far_jump?real(-40,40):real(-3,3)));
            c.putf(0x39c+k*4,c.f32(0x104+k*4)+(i%11u==0u?real(-40,40):real(-3,3)));
            c.putf(0x384+k*4,c.f32(0x128+k*4)+real(-6.5f,6.5f));}
        const PcCameraScreen screen{i%2u?640.f:1280.f,i%2u?480.f:720.f};
        PcCameraBlend blend{i%9u==0u?1:0,std::uint8_t(i%4u==2u?2u:(rng()&0xfdu)),std::int32_t(i%6u==0u?3u:rng()%5u),real(0,1)};
        *reinterpret_cast<float*>(0x740c8c)=screen.width_740c8c;*reinterpret_cast<float*>(0x740c90)=screen.height_740c90;
        *reinterpret_cast<std::int32_t*>(0x82e7d8)=blend.interpolation_override_82e7d8;
        *reinterpret_cast<std::uint8_t*>(0x79fb4e)=blend.owner_flags_79fb4e;
        *reinterpret_cast<std::uint32_t*>(0x799ca0)=owner;*reinterpret_cast<std::int32_t*>(owner+0x1c)=blend.owner_mode_799ca0_1c;
        *reinterpret_cast<float*>(0x634b34)=blend.frame_blend_634b34;
        *reinterpret_cast<std::uint32_t*>(0x79f574)=camera;
        std::memcpy(reinterpret_cast<void*>(camera),cam.data(),cam.size());
        std::memset(reinterpret_cast<void*>(store_trace),0,0x1000);std::memset(reinterpret_cast<void*>(transform_trace),0,0x100);
        std::array<std::uint8_t,0x58> frustum{};for(unsigned k=0;k<frustum.size();++k)frustum[k]=std::uint8_t(0x33+k);
        std::memcpy(reinterpret_cast<void*>(0x95bf40),frustum.data(),frustum.size());
        reset_matrix_oracle_globals();
        std::vector<std::uint8_t> arena(0x400);Bytes ar(arena.data(),arena.size());
        for(unsigned k=0;k<16;++k)ar.putf(k*4,real(-2,2));
        std::memcpy(reinterpret_cast<void*>(MatrixArena),arena.data(),arena.size());
        PcMatrixStack stack{ar,0,0,64};
        prepare(0x484bd0);*reinterpret_cast<unsigned*>(S)=camera;run_original32();
        const float d1=float(course_vec3_distance({c.f32(0xf8),c.f32(0xfc),c.f32(0x100)},{c.f32(0x390),c.f32(0x394),c.f32(0x398)}));
        if(d1>10.f)++resets;
        for(unsigned k=0;k<3;++k)if(std::fabs(c.f32(0x128+k*4))>3.1415927f)++wraps;
        PcCameraDevice device;
        camera_project_484bd0(c,stack,device,screen,blend);
        Bytes fr(frustum.data(),frustum.size());
        for(unsigned k=0;k<6;++k)fr.putf(k*4,device.frustum_95bf40[k]);
        for(unsigned p=0;p<4;++p)for(unsigned k=0;k<4;++k)fr.putf(0x18+p*16+k*4,device.planes_95bf58[p][k]);
        std::vector<std::uint8_t> stores(0x10+device.stores.size()*0x44);Bytes st(stores.data(),stores.size());
        st.put32(0,std::uint32_t(device.stores.size()));
        for(std::size_t k=0;k<device.stores.size();++k){st.put32(0x10+k*0x44,device.stores[k].slot);std::memcpy(stores.data()+0x14+k*0x44,device.stores[k].matrix.data(),64);}
        std::vector<std::uint8_t> transforms(0x10+device.transforms.size()*4);Bytes tt(transforms.data(),transforms.size());
        tt.put32(0,std::uint32_t(device.transforms.size()));for(std::size_t k=0;k<device.transforms.size();++k)tt.put32(0x10+k*4,device.transforms[k]);
        if(inject&&i==3999)cam[0x1c4]^=1;
        compare(cam.data(),camera,cam.size(),"camera object");
        compare(frustum.data(),0x95bf40,frustum.size(),"frustum globals and planes");
        compare(stores.data(),store_trace,stores.size(),"409DF0 slot stores");
        compare(transforms.data(),transform_trace,transforms.size(),"409E00 transforms");
        compare(arena.data(),MatrixArena,arena.size(),"matrix stack arena");
        std::array<std::uint32_t,2> state{MatrixArena+std::uint32_t(stack.current_offset),std::uint32_t(stack.depth)};
        compare(state.data(),0x89b564,8,"matrix stack pointer/depth");
    }
    // Stage 3: complete original 484EE0 init (table copies, 483C10, 484BD0)
    // against native with the generated embedded camera tables.
    for(std::uint32_t page=0x813000u;page<0x81a000u;page+=0x1000u)
        if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("camera table pages");
    std::vector<std::uint8_t> live_fe8(0x64c),live_bb0(0x438),live_750(0x5460);
    for(unsigned i=0;i<2000;++i){
        std::array<std::uint8_t,0x400> cam{};Bytes c(cam.data(),cam.size());
        for(unsigned k=0;k<0x400;k+=4)c.putf(k,real(-3,3));
        for(unsigned k=0;k<3;++k){c.putf(0xf8+k*4,real(-500,500));c.putf(0x104+k*4,c.f32(0xf8+k*4)+real(-20,20));c.putf(0xd4+k*4,real(-9,9));
            c.putf(0x128+k*4,real(-9,9));c.putf(0x390+k*4,c.f32(0xf8+k*4)+real(-15,15));c.putf(0x39c+k*4,c.f32(0x104+k*4)+real(-15,15));}
        const PcCameraScreen screen{i%2u?640.f:1280.f,i%2u?480.f:720.f};
        PcCameraBlend blend{i%3u==0u?1:0,std::uint8_t(i%4u==2u?2u:0u),std::int32_t(i%5u),real(0,1)};
        const std::int32_t steering=i%6u<3u?1:0;
        *reinterpret_cast<float*>(0x740c8c)=screen.width_740c8c;*reinterpret_cast<float*>(0x740c90)=screen.height_740c90;
        *reinterpret_cast<std::int32_t*>(0x82e7d8)=blend.interpolation_override_82e7d8;
        *reinterpret_cast<std::int32_t*>(0x82e7ec)=steering;
        *reinterpret_cast<std::uint8_t*>(0x79fb4e)=blend.owner_flags_79fb4e;
        *reinterpret_cast<std::uint32_t*>(0x799ca0)=owner;*reinterpret_cast<std::int32_t*>(owner+0x1c)=blend.owner_mode_799ca0_1c;
        *reinterpret_cast<float*>(0x634b34)=blend.frame_blend_634b34;
        *reinterpret_cast<std::uint32_t*>(0x79f574)=camera;
        std::memcpy(reinterpret_cast<void*>(camera),cam.data(),cam.size());
        for(auto* v:{&live_fe8,&live_bb0,&live_750})for(auto& b:*v)b=std::uint8_t(rng());
        std::memcpy(reinterpret_cast<void*>(0x818fe8),live_fe8.data(),live_fe8.size());
        std::memcpy(reinterpret_cast<void*>(0x818bb0),live_bb0.data(),live_bb0.size());
        std::memcpy(reinterpret_cast<void*>(0x813750),live_750.data(),live_750.size());
        std::memset(reinterpret_cast<void*>(store_trace),0,0x1000);std::memset(reinterpret_cast<void*>(transform_trace),0,0x100);
        reset_matrix_oracle_globals();
        std::vector<std::uint8_t> arena(0x400);Bytes ar(arena.data(),arena.size());
        for(unsigned k=0;k<16;++k)ar.putf(k*4,real(-2,2));
        std::memcpy(reinterpret_cast<void*>(MatrixArena),arena.data(),arena.size());
        PcMatrixStack stack{ar,0,0,64};
        prepare(0x484ee0);*reinterpret_cast<unsigned*>(S)=camera;run_original32();
        PcCameraDevice device;
        PcCameraTables tables{Bytes(const_cast<std::uint8_t*>(outrun::platform::EmbeddedCameraTable5b4a30),0x64c),
            Bytes(const_cast<std::uint8_t*>(outrun::platform::EmbeddedCameraTable5ba4e0),0x438),
            Bytes(const_cast<std::uint8_t*>(outrun::platform::EmbeddedCameraTable5b5080),0x5460),
            Bytes(live_fe8.data(),live_fe8.size()),Bytes(live_bb0.data(),live_bb0.size()),Bytes(live_750.data(),live_750.size())};
        camera_init_484ee0(c,stack,device,screen,blend,steering,tables);
        std::vector<std::uint8_t> stores(0x10+device.stores.size()*0x44);Bytes st(stores.data(),stores.size());
        st.put32(0,std::uint32_t(device.stores.size()));
        for(std::size_t k=0;k<device.stores.size();++k){st.put32(0x10+k*0x44,device.stores[k].slot);std::memcpy(stores.data()+0x14+k*0x44,device.stores[k].matrix.data(),64);}
        if(inject&&i==1999)cam[0x34c]^=1;
        compare(cam.data(),camera,cam.size(),"init camera object");
        compare(live_fe8.data(),0x818fe8,live_fe8.size(),"init table 818FE8");
        compare(live_bb0.data(),0x818bb0,live_bb0.size(),"init table 818BB0");
        compare(live_750.data(),0x813750,live_750.size(),"init table 813750");
        compare(stores.data(),store_trace,stores.size(),"init 409DF0 stores");
        compare(arena.data(),MatrixArena,arena.size(),"init matrix arena");
    }
    // Stage 4: 485FE0 head, 482E80 in-car test, 4A2BA0 flags over all game
    // modes, and the 78026C == 32 frontend preset branch.
    if(mprotect(reinterpret_cast<void*>(0x780000u),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("camera mode page");
    constexpr unsigned car_at=base+0x6000;
    unsigned in_car=0,faded=0,control_runs=0;
    for(unsigned i=0;i<6000;++i){
        std::array<std::uint8_t,0x400> cam{};Bytes c(cam.data(),cam.size());
        for(unsigned k=0;k<0x400;k+=4)c.putf(k,real(-3,3));
        std::array<std::uint8_t,0xea4> car{};Bytes v(car.data(),car.size());
        for(auto& b:car)b=std::uint8_t(rng());
        v.put8(0x11,std::uint8_t(rng()%30u));v.put16(0xea0,std::uint16_t(int(rng()%24u)-4));
        c.put8(0x34a,std::uint8_t(int(rng()%6u)-1));c.put16(0x33c,std::uint16_t(int(rng()%12u)-3));c.put16(0x368,std::uint16_t(rng()%2u));
        const std::uint8_t display=std::uint8_t(rng());
        const std::int32_t mode=i%3u==0u?32:std::int32_t(i%3u==1u?(3u+rng()%38u):(12u+rng()%20u));   // modes 3..40 (24: 485470, 10: 48608E, idle 4861D5)
        // Place the camera inside the model box about half the time.
        const unsigned model=v.u8(0x11);
        Bytes box(const_cast<std::uint8_t*>(outrun::platform::EmbeddedCameraTable5ba4e0)+0xc+model*0x24,0x18);
        for(unsigned k=0;k<3;++k){const float lo=box.f32(0xc+k*4),hi=box.f32(k*4);
            c.putf(0xe0+k*4,i%2u?lo+(hi-lo)*real(0,1)+(k==2?1.f:0.f):real(-10,10));}
        const bool direct=i%4u==1u;
        *reinterpret_cast<std::uint8_t*>(0x79fcc9)=display;*reinterpret_cast<std::int32_t*>(0x78026c)=mode;
        *reinterpret_cast<std::uint32_t*>(0x799d18)=car_at;*reinterpret_cast<std::uint32_t*>(0x79f574)=camera;
        std::memcpy(reinterpret_cast<void*>(car_at),car.data(),car.size());
        std::memcpy(reinterpret_cast<void*>(camera),cam.data(),cam.size());
        if(direct){
            prepare(0x4a2ba0);*reinterpret_cast<unsigned*>(S)=car_at;run_original32();
            camera_car_flags_4a2ba0(v,c,display,mode);
            if((v.u32(4)&0xc000u)==0x8000u)++faded;
            compare(car.data(),car_at,car.size(),"4A2BA0 car");compare(cam.data(),camera,cam.size(),"4A2BA0 camera unchanged");
            continue;
        }
        const std::uint8_t pause=std::uint8_t(i%5u==0u?1u:0u);
        const std::int32_t preset=std::int32_t(rng()%2u);
        for(unsigned o:{0x94u,0x98u,0x9cu})c.put32(o,i%7u==3u?1u:0u);
        std::memcpy(reinterpret_cast<void*>(camera),cam.data(),cam.size());
        *reinterpret_cast<std::uint8_t*>(0x780248)=pause;*reinterpret_cast<std::int32_t*>(0x819634)=preset;
        const PcCameraControl control{pause,mode,preset,display,v,Bytes(const_cast<std::uint8_t*>(outrun::platform::EmbeddedCameraTable5ba4e0),0x438)};
        const PcCameraScreen screen{640.f,480.f};
        PcCameraBlend blend{0,0,0,real(0,1)};
        *reinterpret_cast<std::int32_t*>(0x82e7d8)=0;*reinterpret_cast<std::uint8_t*>(0x79fb4e)=0;*reinterpret_cast<float*>(0x634b34)=blend.frame_blend_634b34;
        *reinterpret_cast<float*>(0x740c8c)=640.f;*reinterpret_cast<float*>(0x740c90)=480.f;
        reset_matrix_oracle_globals();
        std::vector<std::uint8_t> arena(0x400);Bytes ar(arena.data(),arena.size());
        for(unsigned k=0;k<16;++k)ar.putf(k*4,real(-2,2));
        std::memcpy(reinterpret_cast<void*>(MatrixArena),arena.data(),arena.size());
        PcMatrixStack stack{ar,0,0,64};PcCameraDevice device;
        const auto before_cam=cam;const auto before_car=car;
        const bool native=camera_control_485fe0(c,stack,device,screen,blend,control);
        if(!native){ // unported controller: the native side must not have changed anything
            ++cases;if(cam!=before_cam||car!=before_car){++mismatches;std::cerr<<"unsupported 485FE0 path mutated state\n";}
            continue;
        }
        ++control_runs;if(c.u32(0x354)==1)++in_car;
        std::memset(reinterpret_cast<void*>(store_trace),0,0x1000);
        prepare(0x485fe0);*reinterpret_cast<unsigned*>(S)=camera;run_original32();
        std::vector<std::uint8_t> stores(0x10+device.stores.size()*0x44);Bytes st(stores.data(),stores.size());
        st.put32(0,std::uint32_t(device.stores.size()));
        for(std::size_t k=0;k<device.stores.size();++k){st.put32(0x10+k*0x44,device.stores[k].slot);std::memcpy(stores.data()+0x14+k*0x44,device.stores[k].matrix.data(),64);}
        if(inject&&control_runs==1700)cam[0xf8]^=1;
        compare(cam.data(),camera,cam.size(),"485FE0 camera");compare(car.data(),car_at,car.size(),"485FE0 car");
        compare(stores.data(),store_trace,stores.size(),"485FE0 stores");compare(arena.data(),MatrixArena,arena.size(),"485FE0 matrix arena");
    }
    // Stage 5: 485FE0 in a race (78026C == 16): 485250 start view and the
    // goal views 4853D0 (78026C 0x13/0x1B/0x22/0x23), the
    // offline 4858D0 race view (484810, 483110, 4832F0, 484A40, 485590 with
    // 483CE0/483600/4834A0, 484DF0, 484BD0). 43EB60 is a stub that writes a
    // per-case ground height; the live tables are the 484EE0 copies of the
    // authored ROM tables.
    {
        constexpr unsigned ground_y=base+0x7300;
        std::vector<std::uint8_t> g{0x8b,0x44,0x24,0x08,0x8b,0x0d};
        for(unsigned k=0;k<4;++k)g.push_back(std::uint8_t(ground_y>>(k*8)));
        g.insert(g.end(),{0x89,0x48,0x04,0x31,0xc0,0xc3});
        std::memcpy(reinterpret_cast<void*>(base+0x7280),g.data(),g.size());
        r046_write_rel32(0x43eb60u,0xe9,base+0x7280);
        for(std::uint32_t page:{0x836000u,0x7c2000u,0x7d6000u,0x7f9000u,0x780000u})
            if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("race camera globals");
        std::memcpy(live_fe8.data(),outrun::platform::EmbeddedCameraTable5b4a30,live_fe8.size());
        std::memcpy(live_bb0.data(),outrun::platform::EmbeddedCameraTable5ba4e0,live_bb0.size());
        std::memcpy(live_750.data(),outrun::platform::EmbeddedCameraTable5b5080,live_750.size());
        unsigned starts=0,races=0,blends=0,snaps=0,switches=0,goals=0;
        for(unsigned i=0;i<8000;++i){
            std::array<std::uint8_t,0x400> cam{};Bytes c(cam.data(),cam.size());
            for(unsigned k=0;k<0x400;k+=4)c.putf(k,real(-3,3));
            std::array<std::uint8_t,0xea4> car{};Bytes v(car.data(),car.size());
            for(unsigned k=0;k<car.size();k+=4)v.putf(k,real(-2,2));
            v.put8(0x11,std::uint8_t(rng()%30u));
            for(std::size_t o:{0x2cu,0x2eu,0x30u,0x162u,0x1fcu,0x1feu,0x200u,0xd8cu})v.put16(o,std::uint16_t(rng()));
            for(unsigned k=0;k<3;++k){v.putf(0x14+k*4,real(-800,800));v.putf(0x2d8+k*4,real(-1,1));}
            v.putf(0x1c4,i%3u==0u?real(0.003f,0.011f):real(-0.02f,0.05f));v.putf(0x2c8,real(-1,1));
            v.put32(0x2f0,rng());v.put16(0xd68,std::uint16_t(rng()%6u));v.put32(0xd18,rng()%3u);
            v.put16(0xea0,std::uint16_t(int(rng()%24u)-4));
            const std::uint8_t views[]{0,1,2,5,9,0x15,0x1b,0x1c,0x1d};
            c.put8(0x34a,views[rng()%9u]);c.put8(0x34b,views[rng()%9u]);
            c.put16(0x33c,std::uint16_t(i%4u==0u?0u:rng()%31u));c.put16(0x368,std::uint16_t(rng()%3u));
            c.put16(0x340,std::uint16_t(i%3u==0u?0u:rng()%40u));c.put16(0x342,std::uint16_t(i%5u==0u?std::uint16_t(c.i16(0x340)):rng()%40u));
            c.put16(0x344,std::uint16_t(i%4u==0u?0u:rng()%4u));c.put16(0x346,std::uint16_t(int(rng()%40u)-5));c.put16(0x348,std::uint16_t(1u+rng()%60u));
            c.put8(0x360,std::uint8_t(i%7u==0u?0xeu:rng()%6u));c.put32(0x350,rng()%2u);
            for(unsigned o:{0x94u,0x98u,0x9cu})c.put32(o,0u);
            c.putf(0xac,real(0.3f,1.6f));c.putf(0xb8,real(0.5f,2.2f));c.putf(0xbc,real(0.05f,5));c.putf(0xc0,real(100,20000));
            for(unsigned k=0;k<3;++k){c.putf(0xe0+k*4,real(-5,5));c.putf(0xec+k*4,real(-5,5));c.putf(0x110+k*4,real(-5,5));c.putf(0x11c+k*4,real(-5,5));
                c.putf(0x390+k*4,real(-900,900));c.putf(0x39c+k*4,real(-900,900));c.putf(0x384+k*4,real(-3,3));}
            for(unsigned k=0;k<16;++k){c.putf(0x200+k*4,(k%5u)==0u?1.f:real(-0.4f,0.4f));c.putf(0x240+k*4,(k%5u)==0u?1.f:real(-0.4f,0.4f));}
            const std::uint16_t timers[]{0x12b,0xb5,0xb4,0,1,200,250,90};
            const std::uint16_t timer=timers[i%8u];
            const std::int32_t variant=std::int32_t(i%5u==0u?0u:(rng()%2u?6u:1u));
            const std::uint32_t steering=i%6u==0u?1u:0u,mask=i%3u==1u?0x40000u:(rng()&~0x40000u);
            const std::uint8_t b780270=std::uint8_t(i%11u==0u?1u:0u),b7c24b8=std::uint8_t(rng()%3u);
            const float ground=real(-900,900);
            *reinterpret_cast<std::uint16_t*>(0x8367bc)=timer;*reinterpret_cast<std::uint8_t*>(0x7c24b8)=b7c24b8;
            *reinterpret_cast<std::uint32_t*>(0x7f94c0)=0;*reinterpret_cast<std::int32_t*>(0x780258)=variant;
            *reinterpret_cast<std::uint32_t*>(0x82e7ec)=steering;*reinterpret_cast<std::uint32_t*>(0x7d6778)=mask;
            *reinterpret_cast<std::uint8_t*>(0x780270)=b780270;*reinterpret_cast<float*>(ground_y)=ground;
            const std::uint8_t display=std::uint8_t(rng());
            const std::int32_t goal_modes[]{0x13,0x1b,0x22,0x23},start_modes[]{13,14,17,18};
            const std::int32_t mode=i%4u==3u?goal_modes[rng()%4u]:i%4u==2u?start_modes[rng()%4u]:16;   // 48618D: 13/14/16/17/18
            *reinterpret_cast<std::uint8_t*>(0x79fcc9)=display;*reinterpret_cast<std::int32_t*>(0x78026c)=mode;
            *reinterpret_cast<std::uint8_t*>(0x780248)=0;
            *reinterpret_cast<std::uint32_t*>(0x799d18)=car_at;*reinterpret_cast<std::uint32_t*>(0x79f574)=camera;
            std::memcpy(reinterpret_cast<void*>(car_at),car.data(),car.size());
            std::memcpy(reinterpret_cast<void*>(camera),cam.data(),cam.size());
            std::memcpy(reinterpret_cast<void*>(0x818fe8),live_fe8.data(),live_fe8.size());
            std::memcpy(reinterpret_cast<void*>(0x818bb0),live_bb0.data(),live_bb0.size());
            std::memcpy(reinterpret_cast<void*>(0x813750),live_750.data(),live_750.size());
            const PcCameraScreen screen{640.f,480.f};
            PcCameraBlend blend{0,0,0,real(0,1)};
            *reinterpret_cast<std::int32_t*>(0x82e7d8)=0;*reinterpret_cast<std::uint8_t*>(0x79fb4e)=0;*reinterpret_cast<float*>(0x634b34)=blend.frame_blend_634b34;
            *reinterpret_cast<float*>(0x740c8c)=640.f;*reinterpret_cast<float*>(0x740c90)=480.f;
            reset_matrix_oracle_globals();
            std::vector<std::uint8_t> arena(0x400);Bytes ar(arena.data(),arena.size());
            for(unsigned k=0;k<16;++k)ar.putf(k*4,real(-2,2));
            std::memcpy(reinterpret_cast<void*>(MatrixArena),arena.data(),arena.size());
            PcMatrixStack stack{ar,0,0,64};PcCameraDevice device;
            std::vector<std::uint8_t> fe8=live_fe8,bb0=live_bb0,t750=live_750;
            PcRaceCameraInputs race{v,Bytes(fe8.data(),fe8.size()),Bytes(bb0.data(),bb0.size()),Bytes(t750.data(),t750.size()),
                timer,b7c24b8,0u,variant,steering,mask,b780270,[ground](CourseProbe& p){p.y=ground;}};
            PcCameraControl control{0,mode,0,display,v,Bytes(const_cast<std::uint8_t*>(outrun::platform::EmbeddedCameraTable5ba4e0),0x438)};
            control.race=&race;
            if(!race_camera_mode_48618d(mode))++goals;else if(std::int16_t(timer)>0xb4)++starts;else{++races;if(c.i16(0x33c)!=0)++blends;else ++snaps;if(c.i16(0x340)!=0)++switches;}
            bool native=false;
            try{native=camera_control_485fe0(c,stack,device,screen,blend,control);}
            catch(const std::exception& ex){++cases;++mismatches;if(mismatches<9)std::cerr<<"native race camera threw: "<<ex.what()<<"\n";continue;}
            if(!native){++cases;++mismatches;if(mismatches<9)std::cerr<<"native race camera declined a supported case\n";continue;}
            std::memset(reinterpret_cast<void*>(store_trace),0,0x1000);std::memset(reinterpret_cast<void*>(transform_trace),0,0x100);
            prepare(0x485fe0);*reinterpret_cast<unsigned*>(S)=camera;run_original32();
            std::vector<std::uint8_t> stores(0x10+device.stores.size()*0x44);Bytes st(stores.data(),stores.size());
            st.put32(0,std::uint32_t(device.stores.size()));
            for(std::size_t k=0;k<device.stores.size();++k){st.put32(0x10+k*0x44,device.stores[k].slot);std::memcpy(stores.data()+0x14+k*0x44,device.stores[k].matrix.data(),64);}
            if(inject&&i==7999)cam[0xfc]^=1;
            compare(cam.data(),camera,cam.size(),"race 485FE0 camera");compare(car.data(),car_at,car.size(),"race 485FE0 car");
            compare(fe8.data(),0x818fe8,fe8.size(),"race 485FE0 table 818FE8");
            compare(stores.data(),store_trace,stores.size(),"race 485FE0 stores");compare(arena.data(),MatrixArena,arena.size(),"race 485FE0 matrix arena");
        }
        std::cerr<<"race camera coverage: goal="<<goals<<" start="<<starts<<" race="<<races<<" blend="<<blends<<" snap="<<snaps<<" switch timers="<<switches<<"\n";
    }
    // Stage 6: the 485FE0 paths of the LAN and attract modes: 485300 (variants
    // 3/4 before the start), the 4858D0 shake (55A930 != 0: 46C480 push and the
    // rand() jitter of +36C..+380) and mode 3 (48616A: 485470 or 485B40 with no
    // demo record, record kind 1 or kind 0). rand() 580F40 runs on a stub state
    // (the CRT keeps it per thread); 43EB60 is still the stage-5 ground stub.
    {
        constexpr unsigned rand_stub=base+0x7400,rand_state=base+0x7480,records=base+0x7500,ground_y=base+0x7300;
        auto emit32=[](std::vector<std::uint8_t>& b,std::uint32_t v){for(unsigned k=0;k<4;++k)b.push_back(std::uint8_t(v>>(k*8)));};
        std::vector<std::uint8_t> r{0xa1};emit32(r,rand_state);                       // mov eax,[state]
        r.insert(r.end(),{0x69,0xc0,0xfd,0x43,0x03,0x00,0x05,0xc3,0x9e,0x26,0x00,0xa3}); // imul, add, mov [state],eax
        emit32(r,rand_state);r.insert(r.end(),{0xc1,0xe8,0x10,0x25,0xff,0x7f,0x00,0x00,0xc3});
        std::memcpy(reinterpret_cast<void*>(rand_stub),r.data(),r.size());
        std::array<std::uint8_t,5> rand_entry{};std::memcpy(rand_entry.data(),reinterpret_cast<void*>(0x580f40u),5);
        r046_write_rel32(0x580f40u,0xe9,rand_stub);
        for(std::uint32_t page:{0x800000u,0x83d000u,0x842000u,0x7f1000u,0x7d2000u,0x7d3000u,0x82e000u})
            if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("LAN/demo camera globals");
        unsigned lan_starts=0,shakes=0,shake_steps=0,endings=0,demo_none=0,demo_kind0=0,demo_kind1=0,demo_other=0;
        for(unsigned i=0;i<9000;++i){
            std::array<std::uint8_t,0x400> cam{};Bytes c(cam.data(),cam.size());
            for(unsigned k=0;k<0x400;k+=4)c.putf(k,real(-3,3));
            std::array<std::uint8_t,0xea4> car{};Bytes v(car.data(),car.size());
            for(unsigned k=0;k<car.size();k+=4)v.putf(k,real(-2,2));
            v.put8(0x11,std::uint8_t(rng()%30u));
            for(std::size_t o:{0x2cu,0x2eu,0x30u,0x162u,0x1fcu,0x1feu,0x200u,0xd8cu})v.put16(o,std::uint16_t(rng()));
            for(unsigned k=0;k<3;++k){v.putf(0x14+k*4,real(-800,800));v.putf(0x2d8+k*4,real(-1,1));}
            v.putf(0x1c4,i%3u==0u?real(0.003f,0.011f):real(-0.02f,0.05f));v.putf(0x2c8,real(-1,1));
            v.put32(0x2f0,rng());v.put16(0xd68,std::uint16_t(rng()%6u));v.put32(0xd18,rng()%3u);
            v.put16(0xea0,std::uint16_t(int(rng()%24u)-4));v.put32(0x5c,rng()%2u);
            const std::uint8_t views[]{0,1,2,5,9,0x15,0x1b,0x1c,0x1d};
            c.put8(0x34a,views[rng()%9u]);c.put8(0x34b,views[rng()%9u]);
            c.put16(0x33c,std::uint16_t(i%4u==0u?0u:rng()%31u));c.put16(0x368,std::uint16_t(rng()%3u));
            c.put16(0x340,std::uint16_t(i%3u==0u?0u:rng()%40u));c.put16(0x342,std::uint16_t(i%5u==0u?std::uint16_t(c.i16(0x340)):rng()%40u));
            c.put16(0x344,std::uint16_t(i%4u==0u?0u:rng()%4u));c.put16(0x346,std::uint16_t(int(rng()%40u)-5));c.put16(0x348,std::uint16_t(1u+rng()%60u));
            c.put8(0x360,std::uint8_t(i%7u==0u?0xeu:rng()%6u));c.put32(0x350,rng()%2u);
            for(unsigned o:{0x94u,0x98u,0x9cu})c.put32(o,0u);
            c.putf(0xac,real(0.3f,1.6f));c.putf(0xb8,real(0.5f,2.2f));c.putf(0xbc,real(0.05f,5));c.putf(0xc0,real(100,20000));
            for(unsigned k=0;k<3;++k){c.putf(0xe0+k*4,real(-5,5));c.putf(0xec+k*4,real(-5,5));c.putf(0x110+k*4,real(-5,5));c.putf(0x11c+k*4,real(-5,5));
                c.putf(0x390+k*4,real(-900,900));c.putf(0x39c+k*4,real(-900,900));c.putf(0x384+k*4,real(-3,3));}
            for(unsigned k=0;k<16;++k){c.putf(0x200+k*4,(k%5u)==0u?1.f:real(-0.4f,0.4f));c.putf(0x240+k*4,(k%5u)==0u?1.f:real(-0.4f,0.4f));}
            // Shake state: +354 gate, +378 active, +37C frames left (never 0: IDIV), +380 amount.
            c.put32(0x354,i%5u==0u?1u:0u);c.put32(0x378,rng()%3u?1u:0u);c.put32(0x37c,1u+rng()%8u);c.putf(0x380,real(0,2));
            const unsigned path=i%3u;
            std::int32_t mode=16,variant=1,v83daf4=0,override_82e7d8=0;std::uint16_t timer=0;std::uint32_t network=0;
            float push=0.f;
            if(path==0){                                                        // 485300
                const std::uint16_t t[]{0x167,0xb5,200,0x170,0x181,0x12b};timer=t[rng()%6u];variant=3+std::int32_t(rng()%2u);
                const std::int32_t modes[]{13,14,16,17,18};mode=modes[rng()%5u];network=rng()%2u;++lan_starts;
            }else if(path==1){                                                  // 4858D0 + shake
                const std::uint16_t t[]{0xb4,0,1,90,0x50};timer=t[rng()%5u];variant=std::int32_t(rng()%6u);
                network=1u+rng()%3u;push=rng()%4u?real(-0.5f,3):0.f;++shakes;if(c.u32(0x354)==0u&&c.u32(0x378)!=0u)++shake_steps;
            }else{                                                              // mode 3
                mode=3;v83daf4=std::int32_t(rng()%6u);override_82e7d8=std::int32_t(rng()%2u);network=rng()%2u;
                timer=std::uint16_t(rng()%400u);variant=std::int32_t(rng()%6u);
            }
            // Demo records ([7F1958] + [842860] * 0x44).
            std::array<std::uint8_t,0x44*4> rec{};Bytes rb(rec.data(),rec.size());
            for(unsigned k=0;k<rec.size();k+=4)rb.putf(k,real(-5,5));
            const unsigned index=rng()%4u;Bytes one=rb.sub(index*0x44,0x44);
            const std::uint32_t kinds[]{0,1,2};const std::uint32_t kind=kinds[rng()%3u];one.put32(4,kind);
            for(unsigned k=0;k<3;++k){one.putf(8+k*4,real(-300,300));one.putf(0x14+k*4,real(-3,3));one.putf(0x34+k*4,real(-1,2));}
            one.putf(0x20,rng()%3u?real(0,4):real(-1,0));one.putf(0x24,real(20,90));one.putf(0x28,real(20,90));one.putf(0x2c,real(0,400));
            one.put8(0x30,std::uint8_t(views[rng()%9u]+1u));one.putf(0x40,real(-1,3));
            const bool has_record=path==2&&rng()%4u!=0u;
            if(path==2){
                if(v83daf4>2&&v83daf4<=4&&override_82e7d8!=0)++endings;
                else if(!has_record)++demo_none;else if(kind==0)++demo_kind0;else if(kind==1)++demo_kind1;else ++demo_other;
            }
            std::array<std::uint8_t,64> m2da0{},m3190{};
            for(unsigned k=0;k<16;++k){Bytes(m2da0.data(),64).putf(k*4,k%5u==0u?1.f:real(-0.5f,0.5f));Bytes(m3190.data(),64).putf(k*4,k%5u==0u?1.f:real(-0.5f,0.5f));}
            for(unsigned k=0;k<3;++k){Bytes(m2da0.data(),64).putf(0x30+k*4,real(-500,500));Bytes(m3190.data(),64).putf(0x30+k*4,real(-500,500));}
            std::uint32_t seed=rng();
            const std::uint32_t steering=i%6u==0u?1u:0u,mask=i%3u==1u?0x40000u:(rng()&~0x40000u);
            const std::uint8_t b780270=std::uint8_t(i%11u==0u?1u:0u),b7c24b8=std::uint8_t(rng()%3u);
            const float ground=real(-900,900);
            *reinterpret_cast<std::uint16_t*>(0x8367bc)=timer;*reinterpret_cast<std::uint8_t*>(0x7c24b8)=b7c24b8;
            *reinterpret_cast<std::uint32_t*>(0x7f94c0)=network;*reinterpret_cast<std::int32_t*>(0x780258)=variant;
            *reinterpret_cast<float*>(0x800ad0)=push;*reinterpret_cast<std::int32_t*>(0x83daf4)=v83daf4;
            *reinterpret_cast<std::uint32_t*>(0x7f1958)=has_record?records:0u;*reinterpret_cast<std::uint32_t*>(0x842860)=index;
            std::memcpy(reinterpret_cast<void*>(records),rec.data(),rec.size());
            std::memcpy(reinterpret_cast<void*>(0x7d2da0),m2da0.data(),64);std::memcpy(reinterpret_cast<void*>(0x7d3190),m3190.data(),64);
            *reinterpret_cast<std::uint32_t*>(rand_state)=seed;
            *reinterpret_cast<std::uint32_t*>(0x82e7ec)=steering;*reinterpret_cast<std::uint32_t*>(0x7d6778)=mask;
            *reinterpret_cast<std::uint8_t*>(0x780270)=b780270;*reinterpret_cast<float*>(ground_y)=ground;
            const std::uint8_t display=std::uint8_t(rng());
            *reinterpret_cast<std::uint8_t*>(0x79fcc9)=display;*reinterpret_cast<std::int32_t*>(0x78026c)=mode;
            *reinterpret_cast<std::uint8_t*>(0x780248)=0;
            *reinterpret_cast<std::uint32_t*>(0x799d18)=car_at;*reinterpret_cast<std::uint32_t*>(0x79f574)=camera;
            std::memcpy(reinterpret_cast<void*>(car_at),car.data(),car.size());
            std::memcpy(reinterpret_cast<void*>(camera),cam.data(),cam.size());
            std::memcpy(reinterpret_cast<void*>(0x818fe8),live_fe8.data(),live_fe8.size());
            std::memcpy(reinterpret_cast<void*>(0x818bb0),live_bb0.data(),live_bb0.size());
            std::memcpy(reinterpret_cast<void*>(0x813750),live_750.data(),live_750.size());
            const PcCameraScreen screen{640.f,480.f};
            PcCameraBlend blend{override_82e7d8,0,0,real(0,1)};
            *reinterpret_cast<std::int32_t*>(0x82e7d8)=override_82e7d8;*reinterpret_cast<std::uint8_t*>(0x79fb4e)=0;*reinterpret_cast<float*>(0x634b34)=blend.frame_blend_634b34;
            *reinterpret_cast<float*>(0x740c8c)=640.f;*reinterpret_cast<float*>(0x740c90)=480.f;
            reset_matrix_oracle_globals();
            std::vector<std::uint8_t> arena(0x400);Bytes ar(arena.data(),arena.size());
            for(unsigned k=0;k<16;++k)ar.putf(k*4,real(-2,2));
            std::memcpy(reinterpret_cast<void*>(MatrixArena),arena.data(),arena.size());
            PcMatrixStack stack{ar,0,0,64};PcCameraDevice device;
            std::vector<std::uint8_t> fe8=live_fe8,bb0=live_bb0,t750=live_750;
            PcRaceCameraInputs race{v,Bytes(fe8.data(),fe8.size()),Bytes(bb0.data(),bb0.size()),Bytes(t750.data(),t750.size()),
                timer,b7c24b8,network,variant,steering,mask,b780270,[ground](CourseProbe& p){p.y=ground;}};
            race.network_shake_800ad0=push;race.crt_random_580f40=&seed;race.demo_round_83daf4=v83daf4;
            if(has_record)race.demo_record_4b6f40=one;
            race.area_matrix_7d2da0=Bytes(m2da0.data(),64);race.area_matrix_7d3190=Bytes(m3190.data(),64);
            PcCameraControl control{0,mode,0,display,v,Bytes(const_cast<std::uint8_t*>(outrun::platform::EmbeddedCameraTable5ba4e0),0x438)};
            control.race=&race;
            bool native=false;
            try{native=camera_control_485fe0(c,stack,device,screen,blend,control);}
            catch(const std::exception& ex){++cases;++mismatches;if(mismatches<9)std::cerr<<"native LAN/demo camera threw: "<<ex.what()<<"\n";continue;}
            if(!native){++cases;++mismatches;if(mismatches<9)std::cerr<<"native LAN/demo camera declined path "<<path<<"\n";continue;}
            std::memset(reinterpret_cast<void*>(store_trace),0,0x1000);std::memset(reinterpret_cast<void*>(transform_trace),0,0x100);
            prepare(0x485fe0);*reinterpret_cast<unsigned*>(S)=camera;run_original32();
            std::vector<std::uint8_t> stores(0x10+device.stores.size()*0x44);Bytes st(stores.data(),stores.size());
            st.put32(0,std::uint32_t(device.stores.size()));
            for(std::size_t k=0;k<device.stores.size();++k){st.put32(0x10+k*0x44,device.stores[k].slot);std::memcpy(stores.data()+0x14+k*0x44,device.stores[k].matrix.data(),64);}
            if(inject&&i==8999)cam[0xf8]^=1;
            compare(cam.data(),camera,cam.size(),"LAN/demo 485FE0 camera");compare(car.data(),car_at,car.size(),"LAN/demo 485FE0 car");
            compare(fe8.data(),0x818fe8,fe8.size(),"LAN/demo 485FE0 table 818FE8");
            compare(stores.data(),store_trace,stores.size(),"LAN/demo 485FE0 stores");compare(arena.data(),MatrixArena,arena.size(),"LAN/demo 485FE0 matrix arena");
            compare(&seed,rand_state,4,"LAN/demo 485FE0 rand state");
        }
        if(mprotect(reinterpret_cast<void*>(0x580000u),4096,PROT_READ|PROT_WRITE|PROT_EXEC))throw std::runtime_error("580F40 restore");
        std::memcpy(reinterpret_cast<void*>(0x580f40u),rand_entry.data(),5);
        if(mprotect(reinterpret_cast<void*>(0x580000u),4096,PROT_READ|PROT_EXEC))throw std::runtime_error("580F40 restore protection");
        std::cerr<<"LAN/demo camera coverage: lan starts="<<lan_starts<<" shakes="<<shakes<<" shake steps="<<shake_steps<<" mode-3 endings="<<endings
                 <<" demo none="<<demo_none<<" kind0="<<demo_kind0<<" kind1="<<demo_kind1<<" other kinds="<<demo_other<<"\n";
    }
    std::cerr<<"camera control coverage: 485FE0 runs="<<control_runs<<" in-car="<<in_car<<" faded="<<faded<<"\n";
    std::cerr<<"camera coverage: view resets="<<resets<<" wrapped angles="<<wraps<<"\n";
    std::cout<<"{\"routine\":\"camera_probe\",\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}\n";return mismatches?1:0;
}

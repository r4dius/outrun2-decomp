if(only=="car_reflection_probe"){
    // Original car reflection cube code (413B30 init, 414340 frame leaf with
    // 414050 / 413FC0, 40A6D0 alignment) against pc_car_reflection on the
    // logging fake IDirect3DDevice9. The renderer callees (409EA0, 404310,
    // 410FF0, 411060, 410740, 4107A0, 404540, 40A580, 40EEB0, D3DX) run the
    // original code; 44CD30, 451C20, 405830 and 4056D0 are capture stubs
    // (arguments, current matrix, stack depth), 44B7B0 is `mov al,[7D33D0]`
    // (its protected body) and 449DF0 returns scripted times. Compared: every
    // device call (handles canonicalised: back buffer, depth, cube, cube face
    // surfaces), every renderer global word (8A89F0.., 95D860.., lights, pass
    // records, frustum), the capture records and the matrix arena, pointer
    // and depth (start depths 0..65, including the pointer/depth desync).
    using namespace outrun::platform;
    if(argc<=7||std::string(argv[7])=="-")throw std::runtime_error("car reflection oracle requires pinned d3dx9_29.dll");
    const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
    constexpr unsigned base=0x3a800000u,stubs=base,trace=base+0x10000u,times=base+0x20000u,fix=base+0x30000u;
    constexpr unsigned Stride=0x80u,TraceCap=0x100u;
    constexpr unsigned CAR=fix,CAM=fix+0x2000u,SCN=fix+0x3000u;
    map_at(base,0x40000,PROT_READ|PROT_WRITE|PROT_EXEC);map_original_arithmetic_bridge(file);
    {   unsigned restored=0;                         // remove the dispatcher's start-up .text patches
        const std::size_t text_file=pe_file_offset(file,0x401000u);
        for(std::uint32_t a=0x401000u;a<0x401000u+0x194881u;a+=0x100u){
            const std::size_t n=std::min<std::size_t>(0x100u,0x401000u+0x194881u-a);
            if(std::memcmp(reinterpret_cast<void*>(a),file.data()+text_file+(a-0x401000u),n)){r060_raw_write(a,file.data()+text_file+(a-0x401000u),n);++restored;}
        }
        std::cerr<<"restored "<<restored<<" patched .text blocks"<<char(10);
    }
    for(std::uint32_t page=0x62f000u;page<0x98d000u;page+=0x1000u)mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE);
    init_matrix_oracle_globals();wire_d3dx_imports(map_d3dx29(argv[7]));
    {   auto* page=reinterpret_cast<void*>(0x596000u);
        if(mprotect(page,4096,PROT_READ|PROT_WRITE))throw std::runtime_error("IAT scaling");
        *reinterpret_cast<std::uint32_t*>(0x5962b4u)=D3DX_BASE+(0x441288u-0x400000u);
        *reinterpret_cast<std::uint32_t*>(0x5962dcu)=D3DX_BASE+(0x440e9cu-0x400000u);   // inverse (410FF0, 411060)
        *reinterpret_cast<std::uint32_t*>(0x5962e8u)=D3DX_BASE+(0x441fccu-0x400000u);   // perspective off-center RH (404310)
        *reinterpret_cast<std::uint32_t*>(0x5962e0u)=D3DX_BASE+(0x442f1bu-0x400000u);   // plane from points (404310)
        *reinterpret_cast<std::uint32_t*>(0x5962b0u)=D3DX_BASE+(0x440ba3u-0x400000u);   // transpose (411060 constants)
        if(mprotect(page,4096,PROT_READ))throw std::runtime_error("IAT scaling");
    }
    {   // The DLL's msvcrt _finite import (D3DXMatrixInverse): finite <=> exponent not all ones (as camera_probe).
        static const std::uint8_t finite_stub[]{0x8b,0x44,0x24,0x08,0x25,0x00,0x00,0xf0,0x7f,0x3d,0x00,0x00,0xf0,0x7f,0x0f,0x95,0xc0,0x0f,0xb6,0xc0,0xc3};
        std::memcpy(reinterpret_cast<void*>(base+0x3f000u),finite_stub,sizeof(finite_stub));
        auto* iat=reinterpret_cast<void*>(D3DX_BASE+0x1000u);
        if(mprotect(iat,4096,PROT_READ|PROT_WRITE))throw std::runtime_error("d3dx IAT page");
        *reinterpret_cast<std::uint32_t*>(D3DX_BASE+0x1270u)=base+0x3f000u;
        if(mprotect(iat,4096,PROT_READ))throw std::runtime_error("d3dx IAT restore");
    }
    fake_d3d9::install();
    auto g32=[](std::uint32_t a){return *reinterpret_cast<const std::uint32_t*>(a);};
    auto p32=[](std::uint32_t a,std::uint32_t v){*reinterpret_cast<std::uint32_t*>(a)=v;};
    auto pf=[](std::uint32_t a,float v){*reinterpret_cast<float*>(a)=v;};
    auto emit32=[](std::vector<std::uint8_t>& b,std::uint32_t v){for(unsigned k=0;k<4;++k)b.push_back(std::uint8_t(v>>(k*8)));};
    // ---- stubs ---------------------------------------------------------------
    unsigned stub_off=0;
    auto install=[&](std::uint32_t pc,const std::vector<std::uint8_t>& code){
        std::memcpy(reinterpret_cast<void*>(stubs+stub_off),code.data(),code.size());
        r046_write_rel32(pc,0xe9,stubs+stub_off);stub_off=(stub_off+unsigned(code.size())+15u)&~15u;
    };
    auto capture=[&](std::uint32_t pc,unsigned n){
        std::vector<std::uint8_t> c{0x56,0x57,0x51,0x52,0xa1};emit32(c,trace);           // push esi/edi/ecx/edx; mov eax,[count]
        c.insert(c.end(),{0x69,0xd0});emit32(c,Stride);c.insert(c.end(),{0x81,0xc2});emit32(c,trace+0x10);
        c.insert(c.end(),{0xc7,0x02});emit32(c,pc);                                        // mov [edx],pc
        c.insert(c.end(),{0x8d,0x74,0x24,0x14,0x8d,0x7a,0x04,0xb9});emit32(c,n);c.insert(c.end(),{0xf3,0xa5});
        c.insert(c.end(),{0x8b,0x35,0x64,0xb5,0x89,0x00,0x8d,0x7a,0x20,0xb9,0x10,0,0,0,0xf3,0xa5});
        c.insert(c.end(),{0x8b,0x0d,0x68,0xb5,0x89,0x00,0x89,0x4a,0x60});                 // depth -> +60
        c.insert(c.end(),{0x40,0xa3});emit32(c,trace);c.insert(c.end(),{0x5a,0x59,0x5f,0x5e,0xc3});
        install(pc,c);
    };
    capture(0x44cd30u,0);capture(0x451c20u,0);capture(0x405830u,1);capture(0x4056d0u,4);
    {std::vector<std::uint8_t> c{0xa0};emit32(c,0x7d33d0u);c.push_back(0xc3);install(0x44b7b0u,c);}
    {std::vector<std::uint8_t> c{0xa1};emit32(c,times);c.insert(c.end(),{0xd9,0x04,0x85});emit32(c,times+0x10);
     c.insert(c.end(),{0x40,0xa3});emit32(c,times);c.push_back(0xc3);install(0x449df0u,c);}
    // ---- native device -------------------------------------------------------
    constexpr std::uint32_t NCube=0xc0be0001u,NDepth=0xd5000001u,NBack=0xb0000001u,NDs=0xb0000002u,NFace=0x5f000000u;
    struct NativeDevice final:PcD3D9Device{
        std::vector<std::vector<std::uint32_t>> calls;
        [[noreturn]] static void unused(){throw std::runtime_error("unexpected device call in the cube code");}
        std::uint32_t create_vertex_buffer(std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t)override{unused();}
        std::uint32_t create_index_buffer(std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t)override{unused();}
        std::uint8_t* lock(std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t)override{unused();}
        void unlock(std::uint32_t)override{unused();}
        std::uint32_t create_vertex_declaration(const PcVertexElement*)override{unused();}
        std::uint32_t create_vertex_shader(const std::uint32_t*)override{unused();}
        void set_vertex_shader_constant_f(std::uint32_t start,const float* d,std::uint32_t n)override{
            std::vector<std::uint32_t> c{94,start,n};for(std::uint32_t k=0;k<n*4;++k){std::uint32_t u;std::memcpy(&u,d+k,4);c.push_back(u);}calls.push_back(c);}
        std::uint32_t create_texture_from_file(const PcTextureFileRequest&)override{unused();}
        void release(std::uint32_t h)override{calls.push_back({(h>>24)==0x5fu?0x7002u:h==NCube?0x6002u:0x4002u});}
        std::uint32_t get_render_state(std::uint32_t)override{unused();}
        void set_render_state(std::uint32_t,std::uint32_t)override{unused();}
        std::uint32_t get_texture(std::uint32_t)override{unused();}
        void set_texture(std::uint32_t,std::uint32_t)override{unused();}
        std::uint32_t get_texture_stage_state(std::uint32_t,std::uint32_t)override{unused();}
        void set_texture_stage_state(std::uint32_t,std::uint32_t,std::uint32_t)override{unused();}
        std::uint32_t get_sampler_state(std::uint32_t,std::uint32_t)override{unused();}
        void set_sampler_state(std::uint32_t,std::uint32_t,std::uint32_t)override{unused();}
        std::uint32_t draw_indexed_primitive(std::uint32_t,std::int32_t,std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t)override{unused();}
        void set_vertex_declaration(std::uint32_t)override{unused();}
        void set_vertex_shader(std::uint32_t)override{unused();}
        void set_stream_source(std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t)override{unused();}
        void set_indices(std::uint32_t)override{unused();}
        std::uint32_t create_pixel_shader(const std::uint32_t*)override{unused();}
        void set_pixel_shader(std::uint32_t)override{unused();}
        void set_pixel_shader_constant_f(std::uint32_t,const float*,std::uint32_t)override{unused();}
        std::uint32_t get_declaration(std::uint32_t,PcVertexElement*)override{unused();}
        void set_light(std::uint32_t i,const std::uint32_t* l)override{std::vector<std::uint32_t> c{51,i};c.insert(c.end(),l,l+26);calls.push_back(c);}
        void light_enable(std::uint32_t i,std::uint32_t e)override{calls.push_back({53,i,e});}
        std::uint32_t get_render_target(std::uint32_t i)override{calls.push_back({38,i});return NBack;}
        void set_render_target(std::uint32_t i,std::uint32_t s)override{calls.push_back({37,i,s});}
        std::uint32_t get_depth_stencil_surface()override{calls.push_back({40});return NDs;}
        void set_depth_stencil_surface(std::uint32_t s)override{calls.push_back({39,s});}
        void clear(std::uint32_t f,std::uint32_t c,float z,std::uint32_t st)override{std::uint32_t u;std::memcpy(&u,&z,4);calls.push_back({43,0,0,f,c,u,st});}
        std::uint32_t create_cube_texture(std::uint32_t e,std::uint32_t l,std::uint32_t u,std::uint32_t f,std::uint32_t p)override{calls.push_back({25,e,l,u,f,p});return NCube;}
        std::uint32_t create_depth_stencil_surface(std::uint32_t w,std::uint32_t h,std::uint32_t f,std::uint32_t m,std::uint32_t q,std::uint32_t d)override{calls.push_back({29,w,h,f,m,q,d});return NDepth;}
        std::uint32_t get_cube_map_surface(std::uint32_t c,std::uint32_t face,std::uint32_t level)override{
            if(c!=NCube)throw std::runtime_error("GetCubeMapSurface on another object");calls.push_back({0x6012,face,level});return NFace+face;}
    } native;
    // Guest pointers -> the native handle names.
    auto canon=[&](std::uint32_t p)->std::uint32_t{
        if(p==0)return 0;
        if(p==fake_d3d9::BackBuffer)return NBack;if(p==fake_d3d9::DepthSurface)return NDs;
        if(p==g32(0x8a89f4u))return NCube;if(p==g32(0x8a8a00u))return NDepth;
        if(g32(p)==fake_d3d9::SurfaceVt)return NFace+g32(p+4);
        return 0xbad00000u|(p&0xfffffu);};
    auto guest_calls=[&]{
        std::vector<std::vector<std::uint32_t>> out;std::uint32_t data=0;const auto* df=fake_d3d9::data_log();
        for(std::uint32_t k=0;k<fake_d3d9::log_count();++k){
            const auto* r=fake_d3d9::log_record(k);const auto id=r[0];const auto* a=r+2;
            switch(id){
            case 25:out.push_back({25,a[0],a[1],a[2],a[3],a[4]});break;
            case 29:out.push_back({29,a[0],a[1],a[2],a[3],a[4],a[5]});break;
            case 38:out.push_back({38,a[0]});break;
            case 40:out.push_back({40});break;
            case 37:out.push_back({37,a[0],canon(a[1])});break;
            case 39:out.push_back({39,canon(a[0])});break;
            case 43:out.push_back({43,a[0],a[1],a[2],a[3],a[4],a[5]});break;
            case 53:out.push_back({53,a[0],a[1]});break;
            case 0x6012:out.push_back({0x6012,a[0],a[1]});break;
            case 0x4002:case 0x6002:case 0x7002:out.push_back({id});break;
            case 51:{std::vector<std::uint32_t> c{51,a[0]};for(unsigned w=0;w<26;++w){std::uint32_t u;std::memcpy(&u,df+data+w,4);c.push_back(u);}data+=26;out.push_back(c);break;}
            case 94:{std::vector<std::uint32_t> c{94,a[0],a[2]};
                for(std::uint32_t q=0;q<a[2]*4u;++q){std::uint32_t u;std::memcpy(&u,df+data+q,4);c.push_back(u);}data+=a[2]*4u;
                if(a[0]==0x30u||a[0]==0x31u)c[6]=0;               // 4109C0 cone w: uninitialised PC stack slot
                out.push_back(c);break;}
            case 47:data+=6;out.push_back({47});break;
            default:out.push_back({0xdead0000u|id});break;
            }
        }
        return out;
    };
    // ---- the renderer globals ------------------------------------------------
    PcRenderGlobals rg;
    PcFlushContext fc{native,rg,[](std::uint32_t)->PcPmtResources*{return nullptr;},pc_colour_list_alt_408c80,1};
    const std::set<std::uint32_t> handle_words{0x8a89f4u,0x8a89f8u,0x8a89fcu,0x8a8a00u};
    auto seed=[&]{for(auto& r:rg.ranges)if(r.base!=0x740c98u)for(std::uint32_t k=0;k<r.size;k+=4){
        const auto a=r.base+k;r.words[k/4]=handle_words.count(a)?canon(g32(a)):g32(a);}};
    // ---- one-time init: both sides create their objects ----------------------
    reset_matrix_oracle_globals();
    fake_d3d9::reset_log();native.calls.clear();
    seed();
    std::vector<std::uint8_t> arena(0x10000);Bytes ar(arena.data(),arena.size());
    std::memcpy(arena.data(),reinterpret_cast<void*>(MatrixArena),arena.size());
    prepare(0x413b30);run_original32();
    unsigned mismatches=0,cases=0;std::map<std::string,unsigned> coverage;
    auto report=[&](const std::string& what){++mismatches;if(mismatches<=16)std::cerr<<"MISMATCH "<<what<<char(10);};
    auto compare_all=[&](const std::string& tag,PcMatrixStack& st){
        auto gl=guest_calls();const auto& nl=native.calls;
        coverage["device_calls"]+=unsigned(gl.size());
        if(gl.size()!=nl.size()){std::ostringstream o;o<<"call count guest "<<gl.size()<<" native "<<nl.size()<<tag;
            for(std::size_t k=0;k<std::max(gl.size(),nl.size())&&k<14;++k)o<<"\n   "<<std::hex<<(k<gl.size()?gl[k][0]:0)<<" / "<<(k<nl.size()?nl[k][0]:0)<<std::dec;report(o.str());}
        else for(std::size_t k=0;k<gl.size();++k)if(gl[k]!=nl[k]){
            std::ostringstream o;o<<"call "<<k<<" guest";for(auto v:gl[k])o<<" "<<std::hex<<v;o<<" | native";for(auto v:nl[k])o<<" "<<std::hex<<v;
            report(o.str()+tag);break;}
        for(auto& r:rg.ranges){if(r.base==0x740c98u)continue;bool ok=true;
            for(std::uint32_t k=0;k<r.size&&ok;k+=4){const auto a=r.base+k;const auto gv=handle_words.count(a)?canon(g32(a)):g32(a);
                if(gv!=r.words[k/4]){std::ostringstream o;o<<"global 0x"<<std::hex<<a<<" guest "<<gv<<" native "<<r.words[k/4]<<tag;report(o.str());ok=false;}}}
        if(std::memcmp(arena.data(),reinterpret_cast<void*>(MatrixArena),arena.size())){
            for(std::size_t k=0;k<arena.size();k+=4)if(std::memcmp(arena.data()+k,reinterpret_cast<void*>(MatrixArena+k),4)){
                std::ostringstream o;o<<"matrix arena +0x"<<std::hex<<k<<tag;report(o.str());break;}}
        if(g32(0x89b564u)!=MatrixArena+std::uint32_t(st.current_offset)||std::int32_t(g32(0x89b568u))!=st.depth){
            std::ostringstream o;o<<"matrix pointer/depth guest "<<std::hex<<g32(0x89b564u)-MatrixArena<<"/"<<std::dec<<std::int32_t(g32(0x89b568u))
                <<" native "<<std::hex<<st.current_offset<<"/"<<std::dec<<st.depth<<tag;report(o.str());}
    };
    {   PcMatrixStack st{ar,0,0,64};
        car_reflection_init_413b30(fc,st);
        ++cases;compare_all(" init 413B30",st);
        // Both sides' arenas agree now; resynchronise the globals to the guest (the handles stay canonical).
        seed();
    }
    const std::uint32_t heap_mark=fake_d3d9::heap_top();    // the cube and depth objects; per-case surfaces above
    // ---- cases -----------------------------------------------------------------
    std::mt19937 rng(0x414340u);
    auto rnd=[&](std::uint32_t n){return n?std::uint32_t(rng()%n):0u;};
    auto real=[&](float lo,float hi){return lo+(hi-lo)*(float(rng()&0xffffu)/65535.f);};
    auto fill_matrix=[&](std::uint32_t a){
        // A rotation (yaw / pitch) with a translation, as the car and view matrices are.
        const float y=real(-3.2f,3.2f),p=real(-0.3f,0.3f);const float cy=std::cos(y),sy=std::sin(y),cp=std::cos(p),sp=std::sin(p);
        const float m[16]{cy,0,-sy,0, sy*sp,cp,cy*sp,0, sy*cp,-sp,cy*cp,0, real(-3000,3000),real(-50,300),real(-3000,3000),1};
        for(unsigned k=0;k<16;++k)pf(a+k*4,m[k]);};
    const unsigned total=std::max(count,1u)*400u;
    for(unsigned run=0;run<total;++run){
        unsigned entry=run%3u;   // 0: 414340, 1: 414050 alone, 2: 40A6D0
        if(const char* e=std::getenv("OR2_CUBE_ENTRY"))entry=unsigned(std::stoul(e));
        // inputs
        p32(0x799d18u,CAR);p32(0x79f574u,CAM);p32(0x79f5ecu,SCN);
        *reinterpret_cast<std::uint8_t*>(0x79fb50u)=std::uint8_t(rnd(6)?(2u|(rng()&0xfcu)):rnd(4));
        *reinterpret_cast<std::uint8_t*>(0x7d33d0u)=std::uint8_t(rnd(4)==0);
        p32(CAR+4,rnd(3)==0?((rng()&~0xc000u)|0x8000u):rng());p32(CAR+0x5c,rnd(3)==0);fill_matrix(CAR+0xb0);
        pf(CAM+0xa0,real(0.4f,1.3f));pf(CAM+0xb8,rnd(2)?1.3333334f:real(1.f,2.f));pf(CAM+0xbc,real(0.1f,1.f));pf(CAM+0xc0,real(800.f,6000.f));
        pf(CAM+0xc4,rnd(3)?0.f:real(-0.2f,0.2f));pf(CAM+0xc8,rnd(3)?0.f:real(-0.2f,0.2f));
        for(unsigned k=0;k<0xa0;k+=4)pf(0x899c38u+k,real(-1.5f,1.5f));
        p32(0x899c38u,rnd(4)?1u:rnd(3));p32(0x899c3cu,1u+rnd(3));
        {   // light direction +44: unit, sometimes exactly (0,0,+-1) or with tiny x/y (the degenerate 40A6D0 path)
            const unsigned kind=rnd(5);float x=real(-1,1),y=real(-1,1),z=real(-1,1);
            if(kind==0){x=0;y=0;z=rnd(2)?1.f:-1.f;}else if(kind==1){x=real(-1e-5f,1e-5f);y=real(-1e-5f,1e-5f);z=rnd(2)?1.f:-1.f;}
            const float n=std::sqrt(x*x+y*y+z*z);if(n>0&&kind>1){x/=n;y/=n;z/=n;}
            pf(0x899c38u+0x44,x);pf(0x899c38u+0x48,y);pf(0x899c38u+0x4c,z);}
        p32(SCN,rnd(2)?2u|(rng()&~2u):rng()&~2u);p32(SCN+0x9c,rnd(2)?rng():std::uint32_t(0x3f800000u));
        p32(0x8a8bd0u,rnd(6));
        {const unsigned k=rnd(6);pf(0x8a8bf8u,k==0?1.4f:k==1?std::nanf(""):k==2?1.3999999f:real(0.f,3.f));}
        pf(0x8a8bf4u,real(0.f,2.f));
        p32(0x7d25f0u,rnd(3)?0u:rnd(9));
        // the device-side camera matrices, light/eye state
        for(std::uint32_t a=0x95d860u;a<0x95da60u;a+=0x40u)fill_matrix(a);
        for(unsigned k=0;k<3;++k)pf(0x95dc50u+k*4,real(-100,100));
        for(unsigned k=0;k<5;++k)p32(0x95dc68u+k*4,rnd(3)==0);
        for(unsigned k=0;k<4;++k)p32(0x95def4u+k*4,0);
        // scripted times (ms), increasing
        p32(times,0);{float t=real(0,100);for(unsigned k=0;k<16;++k){t+=rnd(4)?real(0.f,1.f):real(0.f,0.1f);pf(times+0x10+k*4,t);}}
        // the matrix stack: depth 0..65, sometimes with the pointer out of step
        static const std::int32_t depths[]{1,1,1,2,3,0,5,40,61,62,63,64,65};
        std::int32_t depth=depths[rnd(sizeof depths/sizeof depths[0])];
        if(const char* e=std::getenv("OR2_CUBE_DEPTH"))depth=std::stoi(e);
        const std::int32_t slot=std::max(std::min(depth,63),3)+(rnd(5)==0?std::int32_t(rnd(5)):0);   // pointer never behind the pops (PC: before the arena)
        reset_matrix_oracle_globals();
        for(unsigned k=0;k<72*16;++k)pf(MatrixArena+k*4,real(-2,2));
        p32(0x89b564u,MatrixArena+std::uint32_t(slot)*64u);p32(0x89b568u,std::uint32_t(depth));
        std::memcpy(arena.data(),reinterpret_cast<void*>(MatrixArena),arena.size());
        PcMatrixStack st{ar,std::ptrdiff_t(slot)*64,depth,64};
        p32(trace,0);
        seed();
        fake_d3d9::reset_log();native.calls.clear();
        *reinterpret_cast<std::uint32_t*>(fake_d3d9::Heap)=heap_mark;
        // native services: the same capture records
        std::vector<std::array<std::uint32_t,Stride/4>> records;
        auto record=[&](std::uint32_t pc,std::initializer_list<std::uint32_t> args){
            std::array<std::uint32_t,Stride/4> r{};r[0]=pc;unsigned i=1;for(auto a:args)r[i++]=a;
            const auto m=st.current();for(unsigned k=0;k<16;++k)r[8+k]=m.u32(k*4);r[0x18]=std::uint32_t(st.depth);records.push_back(r);};
        std::uint32_t time_index=0;
        PcCarReflectionServices sv;
        sv.env_model_44cd30=[&]{record(0x44cd30u,{});};
        sv.env_sky_451c20=[&]{record(0x451c20u,{});};
        sv.flush_alpha_405830=[&]{record(0x405830u,{0});};
        sv.draw_4056d0=[&](std::uint32_t o,std::uint32_t v,std::uint32_t c,std::uint32_t b){record(0x4056d0u,{o,v,c,b});};
        sv.elapsed_449df0=[&]{const float t=*reinterpret_cast<const float*>(times+0x10+time_index*4);++time_index;return t;};
        PcCarReflectionInputs in;
        in.player_flags_79fb50=*reinterpret_cast<const std::uint8_t*>(0x79fb50u);
        in.final_stage_7d33d0=*reinterpret_cast<const std::uint8_t*>(0x7d33d0u);
        in.car=Bytes(reinterpret_cast<std::uint8_t*>(CAR),0x100);
        in.camera=Bytes(reinterpret_cast<std::uint8_t*>(CAM),0x100);
        in.light_899c38=Bytes(reinterpret_cast<std::uint8_t*>(0x899c38u),0xa0);
        in.scn_efc=Bytes(reinterpret_cast<std::uint8_t*>(SCN),0x1b4);
        in.layer_7d25f0=g32(0x7d25f0u);
        PcRenderView view{};PcCarReflectionStats cstats;
        CourseProbe va{real(-1,1),real(-1,1),real(-1,1)},vb{real(-1,1),real(-1,1),real(-1,1)};
        if(entry==2){const unsigned k=rnd(4);if(k==0)va=vb;else if(k==1){va={-vb.x,-vb.y,-vb.z};}else if(k==2){vb={0,0,1};va={real(-1e-4f,1e-4f),real(-1e-4f,1e-4f),rnd(2)?1.f:-1.f};}}
        std::string tag;
        try{
            if(entry==0){
                if(std::getenv("OR2_CUBE_TRACE"))std::cerr<<"run "<<run<<" flags "<<unsigned(in.player_flags_79fb50)<<" depth "<<depth<<" slot "<<slot<<" face "<<g32(0x8a8bd0u)<<" cube "<<std::hex<<g32(0x8a89f4u)<<std::dec<<std::endl;
                if(const char* e=std::getenv("OR2_CUBE_SKIPCALL")){const std::uint8_t c[5]{0x83,0xc4,0x0c,0x90,0x90};r060_raw_write(std::uint32_t(std::stoul(e,nullptr,16)),c,5);}
                if(const char* e=std::getenv("OR2_CUBE_RET")){const std::uint8_t c[1]{0xc3};r060_raw_write(std::uint32_t(std::stoul(e,nullptr,16)),c,1);}
                if(const char* e=std::getenv("OR2_CUBE_STOP")){const std::uint8_t c[4]{0x8b,0xe5,0x5d,0xc3};r060_raw_write(std::uint32_t(std::stoul(e,nullptr,16)),c,4);}
                prepare(0x414340);run_original32();
                if(std::getenv("OR2_CUBE_STOP")&&(in.player_flags_79fb50&3u)==2u){std::cerr<<"stopped fine"<<std::endl;return 0;}
                car_reflection_frame_414340(fc,st,view,in,sv,cstats);tag=" 414340 run "+std::to_string(run);
                coverage[cstats.frames?"414340 active":"414340 skipped"]++;coverage["faces"]+=cstats.faces;
            }else if(entry==1){
                prepare(0x414050);run_original32();
                car_reflection_scene_414050(fc,st,in,sv,cstats);tag=" 414050 run "+std::to_string(run);
                coverage[(g32(SCN)&2u)?"414050 sun":"414050 no sun"]++;
            }else{
                // 40A6D0(eax = a, ecx = b): register arguments through a guest thunk.
                const std::uint32_t A=fix+0x5000u,B=fix+0x5010u,thunk=fix+0x5100u;
                pf(A,va.x);pf(A+4,va.y);pf(A+8,va.z);pf(B,vb.x);pf(B+4,vb.y);pf(B+8,vb.z);
                std::vector<std::uint8_t> c{0xb8};emit32(c,A);c.push_back(0xb9);emit32(c,B);c.push_back(0xe8);
                emit32(c,0x40a6d0u-(thunk+10u+5u));c.push_back(0xc3);std::memcpy(reinterpret_cast<void*>(thunk),c.data(),c.size());
                prepare(thunk);run_original32();
                car_reflection_align_40a6d0(st,va,vb);tag=" 40A6D0 run "+std::to_string(run);++coverage["40A6D0"];
            }
        }catch(const std::exception& e){++cases;report(std::string("native threw: ")+e.what()+tag);continue;}
        if(guest_call.out_sp!=S){++cases;report("guest stack imbalance"+tag);continue;}
        ++cases;
        if(inject&&run==total-1)rg.w(0x8a8bd0)^=1u;
        compare_all(tag,st);
        const unsigned gn=g32(trace);
        if(gn>TraceCap)throw std::runtime_error("capture overflow");
        if(gn!=records.size())report("capture count guest "+std::to_string(gn)+" native "+std::to_string(records.size())+tag);
        else for(unsigned i=0;i<gn;++i){
            const auto* r=reinterpret_cast<const std::uint32_t*>(trace+0x10+i*Stride);const auto& n=records[i];
            unsigned args=r[0]==0x4056d0u?4u:r[0]==0x405830u?1u:0u;bool ok=r[0]==n[0]&&r[0x18]==n[0x18];
            for(unsigned k=0;k<args;++k)ok=ok&&r[1+k]==n[1+k];
            for(unsigned k=0;k<16;++k)ok=ok&&r[8+k]==n[8+k];
            if(!ok){std::ostringstream o;o<<"capture "<<i<<" guest "<<std::hex<<r[0]<<" depth "<<r[0x18]<<" native "<<n[0]<<" depth "<<n[0x18]<<tag;report(o.str());break;}
        }
        coverage["captures"]+=gn;
    }
    std::cerr<<"coverage:";for(const auto& [k,v]:coverage)std::cerr<<' '<<k<<'='<<v;std::cerr<<char(10);
    std::cout<<"{\"routine\":\"car_reflection_probe\",\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}"<<'\n';return mismatches?1:0;
}

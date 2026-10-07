if(only=="sprite_2d_sprani_probe"||only=="sprite_2d_pool_probe"){
    // sprite_2d_pool_probe: the instance pool display 428170 (429010 canvas
    // matrices, 429460 per visible instance, mode-4 releases) over random
    // pools of those scenes (guest pool 95E028 / counts 956558, native
    // FrontendSprites), same comparisons plus 7551B4 and the pool state.
    const bool pool_mode=only=="sprite_2d_pool_probe";
    // Original SPRANI scene renderer 429460 (48BCF0/48BCB0 windows, 429350
    // layer walk, 429150/4291D0 push/pop, 429220 layer matrices with
    // 48BC00/48B970 channels, 428BD0 footage records with 48BBA0, 42A030,
    // 42DDF0, the 986B28 mask stash, 42D0C0/42DD50 queueing) against
    // pc_sprani_render_429460, over the retail SPRANI animations (every
    // ani_*.bin of OR2_SPRANI_DIR, the inflated ani_*.sz files, one bank
    // each, relocated as 429AC0/429B00 do; the same bytes are placed at the
    // same PC addresses in guest memory). Random scene, frame, scale, list
    // layer, matrix-stack top, blend stack, mask state and queue cursor.
    // Compared: the queue nodes (the uninitialised record +0 stack word
    // excepted), heads, 956BF4, 95B220, the mask stash and its state, the
    // blend stack, the matrix stack and the animation bytes (48BBA0 writes
    // the current frame's texture token into it).
    using namespace outrun::platform;
    const char* dir=std::getenv("OR2_SPRANI_DIR");
    if(!dir)throw std::runtime_error("sprani oracle requires OR2_SPRANI_DIR (inflated ani_*.bin files)");
    const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
    if(argc<=7||std::string(argv[7])=="-")throw std::runtime_error("sprani oracle requires pinned d3dx9_29.dll");
    map_original_arithmetic_bridge(file);
    init_matrix_oracle_globals();wire_d3dx_imports(map_d3dx29(argv[7]));
    {   // D3DXVec4Transform (IAT 596284) -> generic 44067A; the rotation-Z dispatch slot 6100CC -> generic 441521.
        auto* page=reinterpret_cast<void*>(0x596000u);
        if(mprotect(page,4096,PROT_READ|PROT_WRITE))throw std::runtime_error("IAT sprani");
        *reinterpret_cast<std::uint32_t*>(0x596284u)=D3DX_BASE+(0x44067au-0x400000u);
        *reinterpret_cast<std::uint32_t*>(0x5962b4u)=D3DX_BASE+(0x441288u-0x400000u);   // D3DXMatrixScaling (generic)
        if(mprotect(page,4096,PROT_READ))throw std::runtime_error("IAT sprani");
        auto* slot=reinterpret_cast<std::uint32_t*>(D3DX_BASE+(0x6100ccu-0x400000u));
        auto* spage=reinterpret_cast<void*>(std::uintptr_t(slot)&~std::uintptr_t(0xfff));
        if(mprotect(spage,4096,PROT_READ|PROT_WRITE))throw std::runtime_error("d3dx dispatch page");
        *slot=D3DX_BASE+(0x441521u-0x400000u);
    }
    for(std::uint32_t page=0x62f000u;page<0x98d000u;page+=0x1000u)mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE);
    fake_d3d9::install();
    map_at(Pc2dNodeBase,0x30000,PROT_READ|PROT_WRITE);
    auto g32=[](std::uint32_t a){return *reinterpret_cast<const std::uint32_t*>(a);};
    auto p32=[](std::uint32_t a,std::uint32_t v){*reinterpret_cast<std::uint32_t*>(a)=v;};
    auto fb=[](float f){std::uint32_t u;std::memcpy(&u,&f,4);return u;};
    // Banks.
    std::vector<std::string> names;
    for(const auto& e:std::filesystem::directory_iterator(dir)){const auto n=e.path().filename().string();if(n.rfind("ani_",0)==0&&e.path().extension()==".bin")names.push_back(e.path().string());}
    std::sort(names.begin(),names.end());
    if(names.empty()||names.size()>PcSpriteBankCount)throw std::runtime_error("sprani oracle: bad OR2_SPRANI_DIR");
    PcSprite2dState pristine;
    struct Root{std::uint32_t bank,root;};std::vector<Root> roots;
    for(std::uint32_t b=0;b<names.size();++b){
        std::string err;if(!pc_sprite_bank_load_animation(pristine.banks[b],b,load_any(names[b].c_str()),err)){std::cerr<<"skipped "<<names[b]<<": "<<err<<char(10);continue;}
        pristine.banks[b].state=2;
        {const auto& a=pristine.banks[b].animation;
         for(std::uint32_t i=0;i*4u+4u<=a.size();++i){std::uint32_t v;std::memcpy(&v,a.data()+i*4,4);if(!v)break;
            if(const auto r=pc_sprani_scene_root(pristine,(b<<16)|i))roots.push_back({b,r});}}
        const auto base=PcSpraniAnimationBase(b);map_at(base,(std::uint32_t(pristine.banks[b].animation.size())+0xfffu)&~0xfffu,PROT_READ|PROT_WRITE);
        p32(0x9568b8+b*4,base);
    }
    // Textures: 256 fake objects shared by every bank (42A030 returns them, 42DDF0 sizes them).
    struct NativeDevice final:PcD3D9Device{
        std::map<std::uint32_t,std::pair<std::uint32_t,std::uint32_t>> sizes;
        [[noreturn]] static void unused(){throw std::runtime_error("unexpected device call in 429460");}
        std::uint32_t create_vertex_buffer(std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t)override{unused();}
        std::uint32_t create_index_buffer(std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t)override{unused();}
        std::uint8_t* lock(std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t)override{unused();}
        void unlock(std::uint32_t)override{unused();}
        std::uint32_t create_vertex_declaration(const PcVertexElement*)override{unused();}
        std::uint32_t create_vertex_shader(const std::uint32_t*)override{unused();}
        void set_vertex_shader_constant_f(std::uint32_t,const float*,std::uint32_t)override{unused();}
        std::uint32_t create_texture_from_file(const PcTextureFileRequest&)override{unused();}
        void release(std::uint32_t)override{unused();}
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
        bool texture_level_size(std::uint32_t t,std::uint32_t,std::uint32_t& w,std::uint32_t& h)override{
            const auto it=sizes.find(t);if(it==sizes.end())return false;w=it->second.first;h=it->second.second;return true;}
    } native;
    std::mt19937 rng(0x429460u);
    constexpr std::uint32_t TexArray=0x3a810000u;map_at(TexArray,0x1000,PROT_READ|PROT_WRITE);
    std::vector<std::uint32_t> tex;
    for(unsigned k=0;k<256;++k){
        const auto t=fake_d3d9::heap_top();p32(fake_d3d9::Heap,t+0x40);
        const std::uint32_t w=8u<<(rng()%7u),h=8u<<(rng()%7u);p32(t,fake_d3d9::TextureVt);p32(t+4,w);p32(t+8,h);
        native.sizes[t]={w,h};tex.push_back(t);p32(TexArray+k*4,t);
    }
    for(std::uint32_t b=0;b<PcSpriteBankCount;++b){
        const bool loaded=b<names.size()&&!pristine.banks[b].animation.empty();
        p32(0x956d88+b*0x2c,loaded?2u:0u);p32(0x956d88+b*0x2c+8,loaded?TexArray:0u);
        if(loaded)pristine.banks[b].textures=tex;
    }
    constexpr std::uint32_t PoolPtr=0x3a811000u;map_at(PoolPtr,0x1000,PROT_READ|PROT_WRITE);p32(PoolPtr,Pc2dNodeBase);p32(0x956bec,PoolPtr);
    auto uni=[&](float lo,float hi){return lo+(hi-lo)*float(rng()%100000u)/100000.0f;};
    const unsigned cases=std::max(count,1u)*40u;
    unsigned mismatches=0,queued=0,masks=0,faults=0,pool_draws_total=0;
    auto report=[&](const std::string& what){++mismatches;if(mismatches<=12)std::cerr<<"MISMATCH "<<what<<char(10);};
    for(unsigned cs=0;cs<cases;++cs){
        const std::string tag=" case "+std::to_string(cs);
        PcSprite2dState s;
        for(std::uint32_t b=0;b<names.size();++b){if(pristine.banks[b].animation.empty())continue;s.banks[b]=pristine.banks[b];
            std::memcpy(reinterpret_cast<void*>(std::uintptr_t(PcSpraniAnimationBase(b))),s.banks[b].animation.data(),s.banks[b].animation.size());}
        const auto& pick=roots[rng()%roots.size()];
        const float frame=uni(-5.f,300.f),scale=uni(0.f,1.3f);const std::uint32_t layer=rng()%0x16u;
        // Matrix stack top.
        reset_matrix_oracle_globals();
        std::vector<std::uint8_t> arena(0x800);Bytes ar(arena.data(),arena.size());
        {const float a=uni(0.5f,2.f),b=uni(-0.5f,0.5f),c=uni(-0.5f,0.5f),d=uni(0.5f,2.f);
         const float m[16]{a,b,0,0, c,d,0,0, 0,0,1,0, uni(-100.f,700.f),uni(-100.f,500.f),uni(0.f,1.f),1};
         for(unsigned k=0;k<16;++k)ar.putf(k*4,m[k]);}
        std::memcpy(reinterpret_cast<void*>(MatrixArena),arena.data(),arena.size());
        PcMatrixStack stack{ar,0,0,64};
        // Blend stack, flags, mask state, id, queue.
        s.blend_9564e0={uni(0.f,1.f),uni(0.f,1.f),uni(0.f,1.f)};
        for(unsigned k=0;k<3;++k)p32(0x9564e0+k*4,fb(s.blend_9564e0[k]));
        s.depth_9564ec=std::int32_t(rng()%4u);p32(0x9564ec,std::uint32_t(s.depth_9564ec));
        for(unsigned k=0;k<9;++k){s.stack_956468[k]=uni(0.f,1.f);p32(0x956468+k*4,fb(s.stack_956468[k]));}
        for(unsigned k=0;k<18;++k){s.stack_956490[k]=fb(uni(0.f,1.f));p32(0x956490+k*4,s.stack_956490[k]);}
        s.flag_9564f4=rng()%4u==0u;s.flag_9564f8=rng()%4u==0u;p32(0x9564f4,s.flag_9564f4);p32(0x9564f8,s.flag_9564f8);
        s.mask_986b28=rng()%5u==0u;s.mask_986b2c=fb(uni(0.f,1.f));s.mask_986b30=rng()%Pc2dMaskCount;
        p32(0x986b28,s.mask_986b28);p32(0x986b2c,s.mask_986b2c);p32(0x986b30,s.mask_986b30);
        std::memset(reinterpret_cast<void*>(Pc2dMaskBase),0,Pc2dMaskCount*Pc2dRecordBytes);
        s.id_7551b4=std::int32_t(rng());p32(0x7551b4,std::uint32_t(s.id_7551b4));
        s.cursor_956bf4=rng()%Pc2dNodeCount;s.count_95b220=rng()%0x40u;p32(0x956bf4,s.cursor_956bf4);p32(0x95b220,s.count_95b220);
        s.update_index_8a8cdc=rng()%16u==0u?1u:0u;p32(0x8a8cdc,s.update_index_8a8cdc);
        for(std::uint32_t li=0;li<Pc2dLayerCount;++li)p32(0x956c00+li*4,0);
        std::memset(reinterpret_cast<void*>(Pc2dNodeBase),0,Pc2dNodeCount*Pc2dNodeBytes);
        FrontendSprites pool;
        if(!pool_mode){
            prepare(0x429460);guest_call.edi=pick.root;
            reinterpret_cast<unsigned*>(S)[0]=fb(frame);reinterpret_cast<unsigned*>(S)[1]=fb(scale);reinterpret_cast<unsigned*>(S)[2]=layer;
            run_original32();
            pc_sprani_render_429460(s,native,stack,pick.root,frame,scale,layer);
        }else{
            // Pool: a few instances per used layer (same slots on both sides).
            std::memset(reinterpret_cast<void*>(0x95e028),0,21u*0x1f00u);
            for(std::uint32_t l=0;l<21u;++l){p32(0x956558+l*4,0);p32(0x956500+l*4,0);}
            for(std::uint32_t b=0x20;b<PcSpriteBankCount;++b){
                std::vector<FrontendSpriteTiming> t(0x400,FrontendSpriteTiming{100.0f,1.0f,640.0f,480.0f});pool.bind_timing(b,t);}
            for(std::uint32_t l=0;l<21u;++l){
                if(rng()%3u)continue;
                const unsigned n=1u+rng()%4u;
                for(unsigned k=0;k<n;++k){
                    const auto& r=roots[rng()%roots.size()];
                    if(r.bank<0x20u)continue;
                    // the scene index of this root
                    std::uint32_t index=0;{const auto& a=pristine.banks[r.bank].animation;
                        for(std::uint32_t i=0;i*4u+4u<=a.size();++i){std::uint32_t v;std::memcpy(&v,a.data()+i*4,4);if(!v)break;if(pc_sprani_scene_root(pristine,(r.bank<<16)|i)==r.root){index=i;break;}}}
                    const std::uint32_t token=(r.bank<<16)|index;
                    const std::uint32_t mode=rng()%5u;
                    const auto h=pool.create(token,l,mode);if(h==~0u)continue;
                    auto* in=pool.get_mutable(h);
                    in->visible=rng()%5u!=0u;in->frame=uni(-5.f,200.f);in->id_2c=std::int32_t(rng());
                    {const float a=uni(0.5f,1.5f),b=uni(-0.3f,0.3f),c=uni(-0.3f,0.3f),d=uni(0.5f,1.5f);
                     in->matrix={a,b,0,0, c,d,0,0, 0,0,1,0, uni(-100.f,100.f),uni(-100.f,100.f),0,1};}
                    const std::uint32_t g=0x95e028u+(h/64u)*0x1f00u+(h%64u)*0x7cu;
                    p32(g,1);p32(g+4,in->visible?1u:0u);p32(g+0xc,fb(in->frame));p32(g+0x10,mode);p32(g+0x14,l);p32(g+0x2c,std::uint32_t(in->id_2c));
                    for(unsigned m=0;m<16;++m)p32(g+0x30+m*4,fb(in->matrix[m]));
                    std::uint32_t scene;std::memcpy(&scene,pristine.banks[r.bank].animation.data()+index*4,4);
                    p32(g+0x70,scene);p32(g+0x74,r.root);
                    p32(0x956558+l*4,g32(0x956558+l*4)+1u);p32(0x956500+l*4,pool.next(l));
                }
            }
            prepare(0x428170);run_original32();
            pc_sprite_pool_display_428170(s,native,stack,pool);pool_draws_total+=s.pool_draws;
            if(g32(0x7551b4)!=std::uint32_t(s.id_7551b4))report("7551B4"+tag);
            for(std::uint32_t l=0;l<21u;++l){
                if(g32(0x956558+l*4)!=pool.used(l)){report("pool count "+std::to_string(l)+tag);break;}
                for(unsigned k=0;k<64u;++k){const auto g=0x95e028u+l*0x1f00u+k*0x7cu;
                    if((g32(g)!=0u)!=pool.instances()[l*64u+k].allocated){report("pool allocated "+std::to_string(l*64u+k)+tag);l=21u;break;}}}
        }
        if(s.missing){++faults;report("native fault pc "+std::to_string(s.missing)+" address "+std::to_string(s.fault)+tag);}
        if(inject&&cs==7u)s.count_95b220^=1u;
        queued+=s.sprite_nodes;
        // Queue.
        for(std::uint32_t li=0;li<Pc2dLayerCount;++li)if(g32(0x956c00+li*4)!=s.heads_956c00[li]){report("head "+std::to_string(li)+tag);break;}
        if(g32(0x956bf4)!=s.cursor_956bf4)report("956BF4"+tag);
        if(g32(0x95b220)!=s.count_95b220)report("95B220"+tag);
        {const auto* g=reinterpret_cast<const std::uint8_t*>(Pc2dNodeBase);
         for(std::uint32_t n=0;n<Pc2dNodeCount;++n){const std::uint32_t o=n*Pc2dNodeBytes;
            for(std::uint32_t k=0;k<Pc2dNodeBytes;k+=4){if(k==0x58u)continue;
                std::uint32_t gv,nv;std::memcpy(&gv,g+o+k,4);std::memcpy(&nv,s.nodes.data()+o+k,4);
                if(gv!=nv){std::ostringstream m;m<<"node "<<n<<" +"<<std::hex<<k<<" guest "<<gv<<" native "<<nv;report(m.str()+tag);n=Pc2dNodeCount;break;}}}}
        if(g32(0x986b28)!=s.mask_986b28||g32(0x986b2c)!=s.mask_986b2c||g32(0x986b30)!=s.mask_986b30)report("mask state"+tag);
        if(s.mask_986b28)++masks;
        {const auto* g=reinterpret_cast<const std::uint8_t*>(Pc2dMaskBase);
         for(std::uint32_t e=0;e<Pc2dMaskCount;++e)for(std::uint32_t k=4;k<Pc2dRecordBytes;k+=4){
            std::uint32_t gv,nv;std::memcpy(&gv,g+e*Pc2dRecordBytes+k,4);std::memcpy(&nv,s.masks_986b34.data()+e*Pc2dRecordBytes+k,4);
            if(gv!=nv){std::ostringstream m;m<<"mask stash "<<e<<" +"<<std::hex<<k;report(m.str()+tag);e=Pc2dMaskCount;break;}}}
        for(unsigned k=0;k<3;++k)if(g32(0x9564e0+k*4)!=fb(s.blend_9564e0[k])){report("blend 9564E0"+tag);break;}
        if(std::int32_t(g32(0x9564ec))!=s.depth_9564ec)report("9564EC"+tag);
        if(std::memcmp(reinterpret_cast<const void*>(MatrixArena),arena.data(),arena.size()))report("matrix arena"+tag);
        if(g32(0x89b564)!=MatrixArena+std::uint32_t(stack.current_offset)||std::int32_t(g32(0x89b568))!=stack.depth)report("matrix pointer/depth"+tag);
        {const auto& a=s.banks[pick.bank].animation;
         if(std::memcmp(reinterpret_cast<const void*>(std::uintptr_t(PcSpraniAnimationBase(pick.bank))),a.data(),a.size()))report("animation bytes"+tag);}
    }
    std::cerr<<"sprani 429460: roots="<<roots.size()<<" banks="<<names.size()<<" queued sprite nodes="<<queued<<" pending masks="<<masks<<" native faults="<<faults<<char(10);
    if(pool_mode)std::cerr<<"pool draws="<<pool_draws_total<<char(10);
    std::cout<<"{\"routine\":\""<<only<<"\",\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}"<<'\n';return 0;
}

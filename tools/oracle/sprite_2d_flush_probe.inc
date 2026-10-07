if(only=="sprite_2d_flush_probe"){
    // Original 2D queue flush 42D710 (429C60 with 408880, 42A2B0 blend, the
    // 100000/200000 render-state save/restore into the 8606F0 shadow, 42A3A0
    // sprite strips, 42A800 masked multi-texture strips with 1 or 2 chained
    // 986B34 mask records, 42A0A0 with [95B218] = 0, 429F60) on the logging
    // fake IDirect3DDevice9 against pc_2d_flush_42d710. Random queues: nodes
    // at Pc2dNodeBase (same addresses on both sides), random records,
    // textures (fake objects with a width/height), matrices, render / stage /
    // sampler states and renderer globals. D3DXVec4Transform and
    // D3DXMatrixInverse are the pinned d3dx9_29 exports. Compared: the device
    // call sequence (DrawPrimitiveUP with its vertex bytes), the queue words
    // (heads, 95B220), the blend caches, 956BFC and the 8606F0 shadow cells
    // the flush writes. Chains of three masks (42A800's protected bridge
    // 4484BF) and text nodes (42C0F0) are not generated.
    using namespace outrun::platform;
    if(argc<=7||std::string(argv[7])=="-")throw std::runtime_error("sprite 2d oracle requires pinned d3dx9_29.dll");
    const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
    map_original_arithmetic_bridge(file);
    init_matrix_oracle_globals();wire_d3dx_imports(map_d3dx29(argv[7]));
    {   // D3DXVec4Transform (IAT 596284) -> generic 44067A, D3DXMatrixInverse (IAT 5962DC) -> generic 440E9C.
        auto* page=reinterpret_cast<void*>(0x596000u);
        if(mprotect(page,4096,PROT_READ|PROT_WRITE))throw std::runtime_error("IAT 2d");
        *reinterpret_cast<std::uint32_t*>(0x596284u)=D3DX_BASE+(0x44067au-0x400000u);
        *reinterpret_cast<std::uint32_t*>(0x5962dcu)=D3DX_BASE+(0x440e9cu-0x400000u);
        *reinterpret_cast<std::uint32_t*>(0x596294u)=D3DX_BASE+(0x444b56u-0x400000u);   // D3DXMatrixTransformation2D (generic)
        if(mprotect(page,4096,PROT_READ))throw std::runtime_error("IAT 2d");
        auto* slot=reinterpret_cast<std::uint32_t*>(D3DX_BASE+(0x6100ccu-0x400000u));        // rotation-Z dispatch -> generic 441521
        if(mprotect(reinterpret_cast<void*>(std::uintptr_t(slot)&~std::uintptr_t(0xfff)),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("d3dx dispatch page");
        *slot=D3DX_BASE+(0x441521u-0x400000u);
    }
    {   // The generic inverse calls _finite through the DLL's IAT (D3DX+0x1270): stub it (as frame_render_probe).
        constexpr std::uint32_t stub=0x3a807000u;map_at(0x3a800000u,0x10000,PROT_READ|PROT_WRITE|PROT_EXEC);
        static const std::uint8_t finite_stub[]{0x8b,0x44,0x24,0x08,0x25,0x00,0x00,0xf0,0x7f,0x3d,0x00,0x00,0xf0,0x7f,0x0f,0x95,0xc0,0x0f,0xb6,0xc0,0xc3};
        std::memcpy(reinterpret_cast<void*>(stub),finite_stub,sizeof(finite_stub));
        auto* iat=reinterpret_cast<void*>(D3DX_BASE+0x1000u);
        if(mprotect(iat,4096,PROT_READ|PROT_WRITE))throw std::runtime_error("d3dx IAT page");
        *reinterpret_cast<std::uint32_t*>(D3DX_BASE+0x1270u)=stub;
        if(mprotect(iat,4096,PROT_READ))throw std::runtime_error("d3dx IAT restore");}
    for(std::uint32_t page=0x62f000u;page<0x98d000u;page+=0x1000u)mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE);
    fake_d3d9::install();
    // Fake ID3DXSprite ([95B218]): logging stubs 0x8000+slot; SetTransform
    // appends the 16 matrix words, Draw the rect/center/position words of its
    // non-null pointers to the data log.
    constexpr std::uint32_t SpriteObj=fake_d3d9::Base+0x9200u,SpriteVt=fake_d3d9::Base+0x9300u;
    {   constexpr unsigned SpriteArgc[14]={3,1,1,2,2,2,3,3,2,6,1,1,1,1};
        for(unsigned slot=0;slot<14;++slot){
            fake_d3d9::Emit e;fake_d3d9::prologue(e,0x8000u+slot,SpriteArgc[slot]);
            auto copy=[&](unsigned off,unsigned n,bool optional){
                e.u8({0x8b,0x74,0x24});e.b.push_back(std::uint8_t(off));
                if(optional){e.u8({0x85,0xf6,0x74,0x13});}
                e.u8({0x8b,0x3d});e.u32(fake_d3d9::DataPtr);e.u8({0xb9});e.u32(n);e.u8({0xf3,0xa5,0x89,0x3d});e.u32(fake_d3d9::DataPtr);};
            if(slot==5)copy(0x18,16,false);
            if(slot==9){copy(0x1c,4,true);copy(0x20,3,true);copy(0x24,3,true);}
            e.u8({0x31,0xc0});fake_d3d9::epilogue(e,SpriteArgc[slot]);
            const std::uint32_t at=fake_d3d9::Base+0x60000u+slot*0x100u;fake_d3d9::write_stub(at,e);
            *reinterpret_cast<std::uint32_t*>(SpriteVt+slot*4)=at;
        }
        *reinterpret_cast<std::uint32_t*>(SpriteObj)=SpriteVt;
    }
    map_at(Pc2dNodeBase,0x30000,PROT_READ|PROT_WRITE);
    map_at(0x3a812000u,0x1000,PROT_READ|PROT_WRITE);
    auto g32=[](std::uint32_t a){return *reinterpret_cast<const std::uint32_t*>(a);};
    auto p32=[](std::uint32_t a,std::uint32_t v){*reinterpret_cast<std::uint32_t*>(a)=v;};
    auto fbits_=[](float f){std::uint32_t u;std::memcpy(&u,&f,4);return u;};
    struct NativeDevice final:PcD3D9Device{
        std::vector<std::vector<std::uint32_t>> calls;
        std::array<std::uint32_t,0x400> render{};std::array<std::array<std::uint32_t,32>,16> stage{},sampler{};
        std::map<std::uint32_t,std::pair<std::uint32_t,std::uint32_t>> sizes;
        [[noreturn]] static void unused(){throw std::runtime_error("unexpected device call in the 2D flush");}
        void log(std::vector<std::uint32_t> c){calls.push_back(std::move(c));}
        std::uint32_t create_vertex_buffer(std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t)override{unused();}
        std::uint32_t create_index_buffer(std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t)override{unused();}
        std::uint8_t* lock(std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t)override{unused();}
        void unlock(std::uint32_t)override{unused();}
        std::uint32_t create_vertex_declaration(const PcVertexElement*)override{unused();}
        std::uint32_t create_vertex_shader(const std::uint32_t*)override{unused();}
        void set_vertex_shader_constant_f(std::uint32_t start,const float* d,std::uint32_t n)override{
            std::vector<std::uint32_t> c{94,start,n};for(std::uint32_t k=0;k<n*4;++k){std::uint32_t u;std::memcpy(&u,d+k,4);c.push_back(u);}log(c);}
        std::uint32_t create_texture_from_file(const PcTextureFileRequest&)override{unused();}
        void release(std::uint32_t)override{log({2});}
        std::uint32_t get_render_state(std::uint32_t s)override{log({58,s});return render[s&0x3ffu];}
        void set_render_state(std::uint32_t s,std::uint32_t v)override{log({57,s,v});render[s&0x3ffu]=v;}
        std::uint32_t get_texture(std::uint32_t s)override{log({64,s});return 0;}
        void set_texture(std::uint32_t s,std::uint32_t t)override{log({65,s,t});}
        std::uint32_t get_texture_stage_state(std::uint32_t s,std::uint32_t t)override{log({66,s,t});return stage[s&15u][t&31u];}
        void set_texture_stage_state(std::uint32_t s,std::uint32_t t,std::uint32_t v)override{log({67,s,t,v});stage[s&15u][t&31u]=v;}
        std::uint32_t get_sampler_state(std::uint32_t s,std::uint32_t t)override{log({68,s,t});return sampler[s&15u][t&31u];}
        void set_sampler_state(std::uint32_t s,std::uint32_t t,std::uint32_t v)override{log({69,s,t,v});sampler[s&15u][t&31u]=v;}
        std::uint32_t draw_indexed_primitive(std::uint32_t,std::int32_t,std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t)override{unused();}
        void set_vertex_declaration(std::uint32_t)override{unused();}
        void set_vertex_shader(std::uint32_t s)override{log({92,s!=0u});}
        void set_stream_source(std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t)override{unused();}
        void set_indices(std::uint32_t)override{unused();}
        std::uint32_t create_pixel_shader(const std::uint32_t*)override{unused();}
        void set_pixel_shader(std::uint32_t s)override{log({107,s!=0u});}
        void set_pixel_shader_constant_f(std::uint32_t start,const float* d,std::uint32_t n)override{
            std::vector<std::uint32_t> c{109,start,n};for(std::uint32_t k=0;k<n*4;++k){std::uint32_t u;std::memcpy(&u,d+k,4);c.push_back(u);}log(c);}
        std::uint32_t get_declaration(std::uint32_t,PcVertexElement*)override{unused();}
        void draw_primitive_up(std::uint32_t type,std::uint32_t count,const void* data,std::uint32_t stride)override{
            std::vector<std::uint32_t> c{83,type,count,stride};const std::size_t n=std::size_t(count+2u)*stride/4u;
            for(std::size_t k=0;k<n;++k){std::uint32_t u;std::memcpy(&u,static_cast<const std::uint8_t*>(data)+k*4,4);c.push_back(u);}log(c);}
        void set_fvf(std::uint32_t f)override{log({89,f});}
        struct Sprite final:PcD3DXSprite{
            NativeDevice* d{};
            void set_transform(const float m[16])override{std::vector<std::uint32_t> c{0x8005};for(unsigned k=0;k<16;++k){std::uint32_t u;std::memcpy(&u,m+k,4);c.push_back(u);}d->log(c);}
            void begin(std::uint32_t f)override{d->log({0x8008,f});}
            void draw(std::uint32_t t,const std::int32_t* r,const float* ce,const float* po,std::uint32_t col)override{
                std::vector<std::uint32_t> c{0x8009,t,r!=nullptr,ce!=nullptr,po!=nullptr,col};
                if(r)for(unsigned k=0;k<4;++k)c.push_back(std::uint32_t(r[k]));
                if(ce)for(unsigned k=0;k<3;++k){std::uint32_t u;std::memcpy(&u,ce+k,4);c.push_back(u);}
                if(po)for(unsigned k=0;k<3;++k){std::uint32_t u;std::memcpy(&u,po+k,4);c.push_back(u);}
                d->log(c);}
            void end()override{d->log({0x800b});}
        } sprite_obj;
        bool texture_level_size(std::uint32_t t,std::uint32_t level,std::uint32_t& w,std::uint32_t& h)override{
            log({0x4011,level});const auto it=sizes.find(t);if(it==sizes.end())return false;w=it->second.first;h=it->second.second;return true;}
    } native;
    auto guest_calls=[&]{
        std::vector<std::vector<std::uint32_t>> out;std::uint32_t data=0;const auto* df=fake_d3d9::data_log();
        auto words=[&](std::vector<std::uint32_t>& c,std::uint32_t n){for(std::uint32_t w=0;w<n;++w){std::uint32_t u;std::memcpy(&u,df+data+w,4);c.push_back(u);}data+=n;};
        const auto n=fake_d3d9::log_count();
        for(std::uint32_t k=0;k<n;++k){
            const auto* r=fake_d3d9::log_record(k);const auto id=r[0];const auto* a=r+2;
            switch(id){
            case 57:out.push_back({57,a[0],a[1]});break;
            case 58:out.push_back({58,a[0]});break;
            case 64:out.push_back({64,a[0]});break;
            case 65:out.push_back({65,a[0],a[1]});break;
            case 66:out.push_back({66,a[0],a[1]});break;
            case 67:out.push_back({67,a[0],a[1],a[2]});break;
            case 68:out.push_back({68,a[0],a[1]});break;
            case 69:out.push_back({69,a[0],a[1],a[2]});break;
            case 92:out.push_back({92,a[0]!=0u});break;
            case 107:out.push_back({107,a[0]!=0u});break;
            case 89:out.push_back({89,a[0]});break;
            case 94:case 109:{std::vector<std::uint32_t> c{id,a[0],a[2]};words(c,a[2]*4u);out.push_back(c);break;}
            case 83:{std::vector<std::uint32_t> c{83,a[0],a[1],a[3]};words(c,(a[1]+2u)*a[3]/4u);out.push_back(c);break;}
            case 0x4011:out.push_back({0x4011,a[0]});break;
            case 0x8005:{std::vector<std::uint32_t> c{0x8005};words(c,16);out.push_back(c);break;}
            case 0x8008:out.push_back({0x8008,a[0]});break;
            case 0x8009:{std::vector<std::uint32_t> c{0x8009,a[0],a[1]!=0u,a[2]!=0u,a[3]!=0u,a[4]};
                if(a[1])words(c,4);if(a[2])words(c,3);if(a[3])words(c,3);out.push_back(c);break;}
            case 0x800b:out.push_back({0x800b});break;
            case 0x4002:out.push_back({2});break;
            default:out.push_back({0xdead0000u|id});break;
            }
        }
        return out;
    };
    PcRenderGlobals rg;
    PcFlushContext fc{native,rg,[](std::uint32_t)->PcPmtResources*{return nullptr;},pc_colour_list_alt_408c80,1};
    std::mt19937 rng(0x42d710u);
    auto uni=[&](float lo,float hi){return lo+(hi-lo)*float(rng()%100000u)/100000.0f;};
    const unsigned cases=std::max(count,1u)*40u;
    unsigned mismatches=0,calls_total=0,draws_total=0,masked_total=0,masked_strips[2]{},sprite_draws=0;
    auto report=[&](const std::string& what){++mismatches;if(mismatches<=12)std::cerr<<"MISMATCH "<<what<<char(10);};
    static constexpr std::uint32_t Words8606f0[]{0x1c,0x38,0x3c,0x5c,0x60,0x64,0x594,0x598,0x5a0,0x35c,0x360,0x358,0x3d4,0x3e0,0x3e4,0x3d0,0x3dc,0x3f8,0x47c};
    static constexpr std::uint32_t Bytes8606f0[]{0x63f,0x646,0x647,0x64f,0x650,0x651,0x79d,0x79e,0x7a0,0x70f,0x710,0x70e,0x72d,0x730,0x731,0x72c,0x72f,0x736,0x757};
    for(unsigned cs=0;cs<cases;++cs){
        const std::string tag=" case "+std::to_string(cs);
        PcSprite2dState s;
        for(auto& r:rg.ranges){if(r.base==0x740c98u)continue;
            for(std::uint32_t k=0;k<r.size;k+=4){const std::uint32_t v=(rng()%3u)==0u?rng():rng()%3u;p32(r.base+k,v);r.words[k/4]=v;}}
        for(auto& r:rg.ranges)for(std::uint32_t k=0;k<r.size;k+=4)if(r.base+k>=0x8606f0u&&r.base+k<0x860ef0u){p32(r.base+k,0);r.words[k/4]=0;}
        const std::uint32_t layer=rng()%4u;p32(0x7d25f0,layer);
        p32(0x7b11bc,0);p32(0x7b11c0,0);
        for(std::uint32_t st=0;st<0x400;++st){const std::uint32_t v=rng()%9u;p32(fake_d3d9::RenderStates+st*4,v);native.render[st]=v;}
        for(std::uint32_t st=0;st<16;++st)for(std::uint32_t t=0;t<32;++t){const std::uint32_t v=rng()%7u;
            p32(fake_d3d9::StageStates+(st*32+t)*4,v);native.stage[st][t]=v;
            const std::uint32_t w=rng()%5u;p32(fake_d3d9::SamplerStates+(st*32+t)*4,w);native.sampler[st][t]=w;}
        for(std::uint32_t st=0;st<16;++st)p32(fake_d3d9::Textures+st*4,0);
        // Screen scale 740C94/740C98, no sprite, the 8606F0 shadow.
        s.screen_740c94=uni(0.8f,2.2f);s.screen_740c98=uni(0.8f,2.2f);
        p32(0x740c94,fbits_(s.screen_740c94));p32(0x740c98,fbits_(s.screen_740c98));
        const bool with_sprite=rng()%3u!=0u;native.sprite_obj.d=&native;
        p32(0x95b218,with_sprite?SpriteObj:0u);s.sprite_95b218=with_sprite;
        // Image-bank textures (42A0A0 with a +38 texture of 0): banks 5 and 6.
        p32(0x893d20,0x8606f0);
        std::memset(reinterpret_cast<void*>(0x8606f0),0,0x800);
        for(auto& r:rg.ranges)for(std::uint32_t k=0;k<r.size;k+=4)if(r.base+k>=0x8606f0u&&r.base+k<0x860ef0u)r.words[k/4]=0;
        // Textures: fake objects with a width and height.
        const auto heap0=fake_d3d9::heap_top();native.sizes.clear();
        std::vector<std::uint32_t> tex;
        constexpr std::uint32_t TexArray=0x3a812000u;
        for(unsigned k=0;k<8;++k)p32(TexArray+k*4,0);
        for(unsigned k=0;k<6;++k){
            const auto t=fake_d3d9::heap_top();p32(fake_d3d9::Heap,t+0x40);
            const std::uint32_t w=8u<<(rng()%7u),h=8u<<(rng()%7u);p32(t,fake_d3d9::TextureVt);p32(t+4,w);p32(t+8,h);
            native.sizes[t]={w,h};tex.push_back(t);p32(TexArray+k*4,t);
        }
        // A sprite record (0xB8 bytes) at `at` in guest memory.
        auto record=[&](std::uint8_t* r,std::uint32_t flags,std::uint32_t chain){
            std::memset(r,0,0xb8);
            auto w=[&](std::uint32_t o,std::uint32_t v){std::memcpy(r+o,&v,4);};
            auto wf=[&](std::uint32_t o,float v){w(o,fbits_(v));};
            w(4,rng());w(8,flags);w(0xc,tex[rng()%tex.size()]);w(0x10,1);
            const float a=uni(0.5f,2.0f),b=uni(-0.4f,0.4f),c=uni(-0.4f,0.4f),d=uni(0.5f,2.0f);
            const float m[16]{a,b,0,0, c,d,0,0, 0,0,1,0, uni(-50.f,600.f),uni(-50.f,450.f),uni(0.f,1.f),1};
            for(unsigned k=0;k<16;++k)wf(0x14+k*4,m[k]);
            const float W=uni(4.f,256.f),H=uni(4.f,256.f);
            wf(0x64,H);wf(0x6c,W);wf(0x78,W);wf(0x7c,H);
            if(rng()%4u==0u)for(unsigned k=0;k<4;++k)wf(0x5c+k*12,uni(0.f,1.f));
            for(unsigned k=0;k<8;++k)wf(0x84+k*4,uni(0.f,1.f));
            w(0xa4,rng());w(0xa8,rng());w(0xac,1u+rng()%4u);w(0xb0,1u+rng()%4u);w(0xb4,chain);
        };
        // Mask stash 986B34 (0x30 entries).
        unsigned next_mask=0;
        auto mask_entry=[&](unsigned depth)->std::uint32_t{
            const unsigned i=next_mask++%Pc2dMaskCount;const std::uint32_t at=Pc2dMaskBase+i*Pc2dRecordBytes;
            const std::uint32_t chain=depth>1u?0u:0u;(void)chain;
            record(reinterpret_cast<std::uint8_t*>(std::uintptr_t(at)),0x41u,0);return at;
        };
        std::memset(reinterpret_cast<void*>(Pc2dMaskBase),0,Pc2dMaskCount*Pc2dRecordBytes);
        std::memset(reinterpret_cast<void*>(Pc2dNodeBase),0,Pc2dNodeCount*Pc2dNodeBytes);
        std::array<std::uint32_t,Pc2dLayerCount> heads{};std::uint32_t used=0,live=0;
        const std::uint32_t first=rng()%4u,last=first+1u+rng()%4u;
        for(std::uint32_t li=0;li<Pc2dLayerCount;++li){
            if(rng()%3u==0u)continue;
            const std::uint32_t head=Pc2dNodeBase+(used++)*Pc2dNodeBytes;heads[li]=head;++live;
            std::uint32_t prev=head;const unsigned n=1u+rng()%4u;
            for(unsigned k=0;k<n;++k){
                const std::uint32_t node=Pc2dNodeBase+(used++)*Pc2dNodeBytes;++live;
                p32(prev,node);p32(node+4,prev);p32(node+8,head);prev=node;
                auto* r=reinterpret_cast<std::uint8_t*>(std::uintptr_t(node));
                if(rng()%5u==0u&&!std::getenv("OR2_2D_NOIMG")){                                         // image node (42A0A0)
                    p32(node+0xc,0);const std::uint32_t r=node+0x10;
                    const std::uint32_t bank=5u+rng()%2u;p32(r,(bank<<16)|(rng()%8u));
                    for(unsigned k=0;k<4;++k)p32(r+4+k*4,rng()%3u==0u?0u:rng()%300u);
                    p32(r+0x14,fbits_(uni(-2.f,2.f)));p32(r+0x18,fbits_(uni(-2.f,2.f)));p32(r+0x1c,fbits_(uni(0.f,64.f)));p32(r+0x20,fbits_(uni(0.f,64.f)));
                    p32(r+0x24,fbits_(uni(-50.f,650.f)));p32(r+0x28,fbits_(uni(-50.f,500.f)));p32(r+0x2c,rng());p32(r+0x30,fbits_(rng()%3u==0u?uni(-3.f,3.f):0.f));
                    p32(r+0x34,(rng()%16u)|((rng()%2u)?0x8000u:0u));p32(r+0x38,rng()%3u==0u?0u:tex[rng()%tex.size()]);
                    continue;
                }
                p32(node+0xc,1);
                std::uint32_t flags=((rng()%2u)?0x45u:0x41u)|((rng()%4u)<<15);
                const unsigned pick=rng()%4u;if(pick==1u)flags|=0x100000u;else if(pick==2u)flags|=0x200000u;
                std::uint32_t chain=0;
                if(rng()%2u&&!std::getenv("OR2_2D_NOMASK")){
                    ++masked_total;flags|=0x40000u;if(rng()%4u)flags|=0x80000u;
                    chain=mask_entry(1);
                    if(rng()%3u==0u){const auto second=mask_entry(2);p32(chain+0xb4,second);}
                }
                record(r+0x58,flags,chain);
            }
            p32(head+4,prev);p32(prev,0);
        }
        for(std::uint32_t li=0;li<Pc2dLayerCount;++li){p32(0x956c00+li*4,heads[li]);s.heads_956c00[li]=heads[li];}
        const std::uint32_t count0=live+rng()%8u;p32(0x95b220,count0);s.count_95b220=count0;
        const std::uint32_t bc[4]{rng()%3u==0u?0xffffffffu:rng()%16u,rng()%16u,rng()%8u,rng()%0x100u};
        p32(0x956bf8,bc[0]);p32(0x956b94,bc[1]);p32(0x956b98,bc[2]);p32(0x956bfc,bc[3]);
        s.blend_956bf8=bc[0];s.blend_956b94=bc[1];s.blend_956b98=bc[2];s.w956bfc=bc[3];
        std::memcpy(s.nodes.data(),reinterpret_cast<const void*>(Pc2dNodeBase),s.nodes.size());
        std::memcpy(s.masks_986b34.data(),reinterpret_cast<const void*>(Pc2dMaskBase),s.masks_986b34.size());
        fake_d3d9::reset_log();native.calls.clear();
        prepare(0x42d710);reinterpret_cast<unsigned*>(S)[0]=first;reinterpret_cast<unsigned*>(S)[1]=last;run_original32();
        for(std::uint32_t b:{5u,6u}){p32(0x956d88+b*0x2c,2);p32(0x956d88+b*0x2c+8,TexArray);s.banks[b].state=2;s.banks[b].textures.assign(tex.begin(),tex.end());s.banks[b].textures.resize(8,0u);}
        pc_2d_flush_42d710(s,fc,with_sprite?&native.sprite_obj:nullptr,first,last,layer);
        if(s.missing)report("native latched missing "+std::to_string(s.missing)+tag);
        if(inject&&cs==7u)s.count_95b220^=1u;
        auto gl=guest_calls();const auto& nl=native.calls;calls_total+=unsigned(gl.size());
        for(const auto& c:gl){if(c[0]==0x8009u)++sprite_draws;}for(const auto& c:gl)if(c[0]==83u){++draws_total;if(c[3]==0x24u)++masked_strips[0];else if(c[3]==0x2cu)++masked_strips[1];}
        if(gl.size()!=nl.size())report("device call count guest "+std::to_string(gl.size())+" native "+std::to_string(nl.size())+tag);
        for(std::size_t k=0;k<std::min(gl.size(),nl.size());++k)if(gl[k]!=nl[k]){
            std::ostringstream o;o<<"device call "<<k<<" guest";for(std::size_t j=0;j<gl[k].size()&&j<12;++j)o<<" "<<std::hex<<gl[k][j];
            o<<" | native";for(std::size_t j=0;j<nl[k].size()&&j<12;++j)o<<" "<<std::hex<<nl[k][j];
            if(gl[k].size()==nl[k].size())for(std::size_t j=0;j<gl[k].size();++j)if(gl[k][j]!=nl[k][j]){o<<" (word "<<std::dec<<j<<std::hex<<": "<<gl[k][j]<<" vs "<<nl[k][j]<<")";break;}
            report(o.str()+tag);break;}
        // Queue and caches.
        for(std::uint32_t li=0;li<Pc2dLayerCount;++li)if(g32(0x956c00+li*4)!=s.heads_956c00[li])report("head "+std::to_string(li)+tag);
        if(g32(0x95b220)!=s.count_95b220)report("95B220 guest "+std::to_string(g32(0x95b220))+" native "+std::to_string(s.count_95b220)+tag);
        if(g32(0x956bf8)!=s.blend_956bf8||g32(0x956b94)!=s.blend_956b94||g32(0x956b98)!=s.blend_956b98)report("blend caches"+tag);
        if(g32(0x956bfc)!=s.w956bfc)report("956BFC"+tag);
        // 8606F0 shadow cells.
        const std::map<std::uint32_t,std::uint32_t> named_w{{0x1c,s.shadow_1c},{0x38,s.shadow_38},{0x3c,s.shadow_3c},{0x5c,s.shadow_5c},{0x60,s.shadow_60},{0x64,s.shadow_64}};
        const std::map<std::uint32_t,std::uint8_t> named_b{{0x63f,s.shadow_63f},{0x646,s.shadow_646},{0x647,s.shadow_647},{0x64f,s.shadow_64f},{0x650,s.shadow_650},{0x651,s.shadow_651}};
        for(auto o:Words8606f0){std::uint32_t nv;const auto it=named_w.find(o);if(it!=named_w.end())nv=it->second;else std::memcpy(&nv,s.shadow_8606f0.data()+o,4);
            if(g32(0x8606f0+o)!=nv){std::ostringstream m;m<<"shadow word +"<<std::hex<<o<<" guest "<<g32(0x8606f0+o)<<" native "<<nv;report(m.str()+tag);break;}}
        for(auto o:Bytes8606f0){const auto it=named_b.find(o);const std::uint8_t nv=it!=named_b.end()?it->second:s.shadow_8606f0[o];
            if(*reinterpret_cast<const std::uint8_t*>(0x8606f0+o)!=nv){std::ostringstream m;m<<"shadow byte +"<<std::hex<<o;report(m.str()+tag);break;}}
        p32(fake_d3d9::Heap,heap0);
    }
    std::cerr<<"sprite 2d flush: device calls="<<calls_total<<" strips="<<draws_total<<" masked records="<<masked_total<<" 2-stage strips="<<masked_strips[0]<<" 3-stage strips="<<masked_strips[1]<<" ID3DXSprite draws="<<sprite_draws<<char(10);
    std::cout<<"{\"routine\":\"sprite_2d_flush_probe\",\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}"<<'\n';return 0;
}

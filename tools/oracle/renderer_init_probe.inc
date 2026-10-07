if(only=="renderer_init_probe"){
    // Original renderer initialisation 404250 (4041E0, 404540, 404600,
    // 40F6B0, 405160, 4083F0 with 40AC60/40AF40/420B00, 420B80 normalisation
    // cube map, 408880 (404540, 404600, 408F90, 409430), 4227C0) on the
    // logging fake IDirect3DDevice9 against renderer_init_404250. Platform
    // leaves replaced on the guest side: malloc 580253 (fixed buffer), free
    // 580BC2 (ret), D3DXCreateTexture (logging stub creating a texture);
    // D3DXVec3Normalize is the pinned d3dx9_29 export. Compared: the device
    // call sequence (object handles by presence, shader tokens, constants),
    // the renderer globals (handles by presence), the six cube faces and
    // 8999A0.
    using namespace outrun::platform;
    if(argc<=7||std::string(argv[7])=="-")throw std::runtime_error("renderer init oracle requires pinned d3dx9_29.dll");
    const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
    constexpr unsigned base=0x3a800000,Stubs=base+0x1000,Buffer=base+0x10000;
    map_at(base,0x40000,PROT_READ|PROT_WRITE|PROT_EXEC);
    map_original_arithmetic_bridge(file);
    init_matrix_oracle_globals();wire_d3dx_imports(map_d3dx29(argv[7]));
    for(std::uint32_t page=0x62f000u;page<0x98d000u;page+=0x1000u)mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE);
    fake_d3d9::install();
    auto g32=[](std::uint32_t a){return *reinterpret_cast<const std::uint32_t*>(a);};
    auto p32=[](std::uint32_t a,std::uint32_t v){*reinterpret_cast<std::uint32_t*>(a)=v;};
    unsigned next_stub=Stubs;
    auto put=[&](const fake_d3d9::Emit& e){std::memcpy(reinterpret_cast<void*>(next_stub),e.b.data(),e.b.size());const auto at=next_stub;next_stub+=0x80;return at;};
    {   fake_d3d9::Emit e;e.u8({0xb8});e.u32(Buffer);e.u8({0xc3});r046_write_rel32(0x420bcf,0xe8,put(e));}   // malloc
    {   fake_d3d9::Emit e;e.u8({0xc3});r046_write_rel32(0x440cfa,0xe8,put(e));}                            // free
    {   // D3DXCreateTexture(pDevice,W,H,Mip,Usage,Format,Pool,ppTexture): stdcall 8.
        fake_d3d9::Emit e;fake_d3d9::prologue(e,0x5003,9);fake_d3d9::alloc_fixed(e,0x40,fake_d3d9::TextureVt);
        e.u8({0x8b,0x54,0x24,0x30,0x89,0x02,0x31,0xc0,0x5a,0x59,0x5f,0x5e,0xc2,0x20,0x00});
        const auto at=put(e);auto* iat=reinterpret_cast<void*>(0x596000u);
        if(mprotect(iat,4096,PROT_READ|PROT_WRITE))throw std::runtime_error("init IAT");
        p32(0x59629cu,at);
        if(mprotect(iat,4096,PROT_READ))throw std::runtime_error("init IAT restore");}
    // ---- native logging device ----
    struct NativeDevice final:PcD3D9Device{
        std::vector<std::vector<std::uint32_t>> calls;
        std::array<std::uint32_t,0x400> render{};std::array<std::array<std::uint32_t,32>,16> stage{},sampler{};std::array<std::uint32_t,16> textures{};
        std::uint32_t next{0x100};
        std::map<std::uint32_t,std::vector<std::uint8_t>> surfaces;std::vector<std::uint32_t> surface_order;
        [[noreturn]] static void unused(){throw std::runtime_error("unexpected device call in the renderer init");}
        void log(std::vector<std::uint32_t> c){calls.push_back(std::move(c));}
        static std::uint32_t hash(const std::uint32_t* t){std::uint32_t h=0x811c9dc5u;for(;;++t){h=(h^*t)*0x01000193u;if(*t==0xffffu)return h;}}
        std::uint32_t create_vertex_buffer(std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t)override{unused();}
        std::uint32_t create_index_buffer(std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t)override{unused();}
        std::uint8_t* lock(std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t)override{unused();}
        void unlock(std::uint32_t)override{unused();}
        std::uint32_t create_vertex_declaration(const PcVertexElement*)override{unused();}
        std::uint32_t create_vertex_shader(const std::uint32_t*)override{unused();}
        void set_vertex_shader_constant_f(std::uint32_t start,const float* d,std::uint32_t n)override{
            std::vector<std::uint32_t> c{94,start,n};for(std::uint32_t k=0;k<n*4;++k){std::uint32_t u;std::memcpy(&u,d+k,4);c.push_back(u);}log(c);}
        std::uint32_t create_texture_from_file(const PcTextureFileRequest&)override{unused();}
        void release(std::uint32_t h)override{log({surfaces.count(h)?0x7002u:2u});}
        std::uint32_t get_render_state(std::uint32_t s)override{log({58,s});return render[s&0x3ffu];}
        void set_render_state(std::uint32_t s,std::uint32_t v)override{log({57,s,v});render[s&0x3ffu]=v;}
        std::uint32_t get_texture(std::uint32_t s)override{log({64,s});return textures[s&15u];}
        void set_texture(std::uint32_t s,std::uint32_t t)override{log({65,s,t!=0u});}
        std::uint32_t get_texture_stage_state(std::uint32_t s,std::uint32_t t)override{log({66,s,t});return stage[s&15u][t&31u];}
        void set_texture_stage_state(std::uint32_t s,std::uint32_t t,std::uint32_t v)override{log({67,s,t,v});stage[s&15u][t&31u]=v;}
        std::uint32_t get_sampler_state(std::uint32_t s,std::uint32_t t)override{log({68,s,t});return sampler[s&15u][t&31u];}
        void set_sampler_state(std::uint32_t s,std::uint32_t t,std::uint32_t v)override{log({69,s,t,v});sampler[s&15u][t&31u]=v;}
        std::uint32_t draw_indexed_primitive(std::uint32_t,std::int32_t,std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t)override{unused();}
        void set_vertex_declaration(std::uint32_t)override{unused();}
        void set_vertex_shader(std::uint32_t)override{unused();}
        void set_stream_source(std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t)override{unused();}
        void set_indices(std::uint32_t)override{unused();}
        std::uint32_t create_pixel_shader(const std::uint32_t* t)override{log({106,hash(t)});return next++;}
        void set_pixel_shader(std::uint32_t s)override{log({107,s!=0u});}
        void set_pixel_shader_constant_f(std::uint32_t start,const float* d,std::uint32_t n)override{
            std::vector<std::uint32_t> c{109,start,n};for(std::uint32_t k=0;k<n*4;++k){std::uint32_t u;std::memcpy(&u,d+k,4);c.push_back(u);}log(c);}
        std::uint32_t get_declaration(std::uint32_t,PcVertexElement*)override{unused();}
        std::uint32_t create_cube_texture(std::uint32_t e,std::uint32_t l,std::uint32_t u,std::uint32_t f,std::uint32_t p)override{log({25,e,l,u,f,p});return next++;}
        std::uint32_t create_texture(std::uint32_t w,std::uint32_t h,std::uint32_t l,std::uint32_t u,std::uint32_t f,std::uint32_t p)override{log({0x5003,w,h,l,u,f,p});return next++;}
        std::uint32_t create_depth_stencil_surface(std::uint32_t w,std::uint32_t h,std::uint32_t f,std::uint32_t m,std::uint32_t q,std::uint32_t d)override{log({29,w,h,f,m,q,d});return next++;}
        std::uint32_t get_cube_map_surface(std::uint32_t,std::uint32_t face,std::uint32_t level)override{
            log({0x6012,face,level});const auto h=next++;surfaces[h].assign(0x4000,0);surface_order.push_back(h);return h;}
        std::uint32_t get_surface_level(std::uint32_t,std::uint32_t level)override{log({0x4012,level});return next++;}
        std::uint8_t* lock_rect(std::uint32_t s,std::uint32_t& pitch)override{log({0x700d,0,0});pitch=0x100;return surfaces.at(s).data();}
        void unlock_rect(std::uint32_t)override{log({0x700e});}
    } native;
    auto guest_calls=[&]{
        std::vector<std::vector<std::uint32_t>> out;std::uint32_t data=0;const auto* df=fake_d3d9::data_log();
        auto words=[&](std::vector<std::uint32_t>& c,unsigned n){for(unsigned w=0;w<n;++w){std::uint32_t u;std::memcpy(&u,df+data+w,4);c.push_back(u);}data+=n;};
        const auto n=fake_d3d9::log_count();
        for(std::uint32_t k=0;k<n;++k){
            const auto* r=fake_d3d9::log_record(k);const auto id=r[0];const auto* a=r+2;
            switch(id){
            case 57:out.push_back({57,a[0],a[1]});break;
            case 58:out.push_back({58,a[0]});break;
            case 65:out.push_back({65,a[0],a[1]!=0u});break;
            case 64:out.push_back({64,a[0]});break;
            case 66:out.push_back({66,a[0],a[1]});break;
            case 68:out.push_back({68,a[0],a[1]});break;
            case 69:out.push_back({69,a[0],a[1],a[2]});break;
            case 67:out.push_back({67,a[0],a[1],a[2]});break;
            case 94:case 109:{std::vector<std::uint32_t> c{id,a[0],a[2]};words(c,a[2]*4u);out.push_back(c);break;}
            case 106:out.push_back({106,NativeDevice::hash(reinterpret_cast<const std::uint32_t*>(std::uintptr_t(a[0])))});break;
            case 107:out.push_back({107,a[0]!=0u});break;
            case 25:out.push_back({25,a[0],a[1],a[2],a[3],a[4]});break;
            case 29:out.push_back({29,a[0],a[1],a[2],a[3],a[4],a[5]});break;
            case 0x5003:out.push_back({0x5003,a[0],a[1],a[2],a[3],a[4],a[5]});break;
            case 0x6012:out.push_back({0x6012,a[0],a[1]});break;
            case 0x4012:out.push_back({0x4012,a[0]});break;
            case 0x700d:out.push_back({0x700d,a[1],a[2]});break;
            case 0x700e:out.push_back({0x700e});break;
            case 0x7002:out.push_back({0x7002});break;
            case 0x4002:case 0x6002:out.push_back({2});break;
            default:out.push_back({0xdead0000u|id});break;
            }
        }
        return out;
    };
    // Handle words: compared by presence (fake pointers vs native handles).
    auto handle_word=[](std::uint32_t a){return (a>=0x89b5b8u&&a<=0x89b5e0u)||a==0x89a548u||(a>=0x95afc4u&&a<=0x95afccu);};
    PcRenderGlobals rg;
    PcFlushContext fc{native,rg,[](std::uint32_t)->PcPmtResources*{return nullptr;},pc_colour_list_alt_408c80,1};
    PcRenderQueue opaque,alpha;PcRenderView view{};std::vector<std::uint8_t> arena(64*64);
    outrun::driving::PcMatrixStack ms{Bytes(arena.data(),arena.size()),0,0,64};
    PcRenderContext queue{opaque,alpha,view,ms,[](std::uint32_t)->const PcModelBank*{return nullptr;},7};
    std::mt19937 rng(0x404250u);
    const unsigned cases=std::max(count,1u)*40u;
    unsigned mismatches=0,calls_total=0,faces_checked=0;
    auto report=[&](const std::string& what){++mismatches;if(mismatches<=12)std::cerr<<"MISMATCH "<<what<<char(10);};
    for(unsigned c=0;c<cases;++c){
        const std::string tag=" case "+std::to_string(c);
        // Renderer globals: random words (0/1-heavy where branches read flags), same on both sides.
        for(auto& r:rg.ranges){if(r.base==0x740c98u)continue;
            for(std::uint32_t k=0;k<r.size;k+=4){const std::uint32_t v=(rng()%3u)==0u?rng():rng()%3u;p32(r.base+k,v);r.words[k/4]=v;}}
        for(std::uint32_t a:{0x89bd2cu,0x89bc4cu,0x89bc50u}){if(rng()%2u){const std::uint32_t v=a==0x89bd2cu?0x1ffu:a==0x89bc4cu?0x130c0305u:0x1c80u;p32(a,v);rg.w(a)=v;}}
        const std::uint32_t layer=rng()%4u;p32(0x7d25f0,layer);
        p32(0x8999a0,rng());queue.flag_depth_8999a0=std::int32_t(g32(0x8999a0));
        p32(0x7b11bc,0);p32(0x7b11c0,0);
        for(std::uint32_t s=0;s<0x400;++s){const std::uint32_t v=rng()%9u;p32(fake_d3d9::RenderStates+s*4,v);native.render[s]=v;}
        for(std::uint32_t s=0;s<16;++s)for(std::uint32_t t=0;t<32;++t){const std::uint32_t v=(rng()%3u)?rng()%7u:0x3f800000u;
            p32(fake_d3d9::StageStates+(s*32+t)*4,v);native.stage[s][t]=v;
            const std::uint32_t w=rng()%5u;p32(fake_d3d9::SamplerStates+(s*32+t)*4,w);native.sampler[s][t]=w;}
        for(std::uint32_t s=0;s<16;++s){p32(fake_d3d9::Textures+s*4,0);native.textures[s]=0;}
        fake_d3d9::reset_log();native.calls.clear();native.surfaces.clear();native.surface_order.clear();
        const auto heap0=fake_d3d9::heap_top();
        prepare(0x404250);run_original32();
        renderer_init_404250(fc,queue,layer);
        if(inject&&c==7u)rg.w(0x89bd20)^=1u;
        // Device calls.
        auto gl=guest_calls();const auto& nl=native.calls;calls_total+=unsigned(gl.size());
        if(gl.size()!=nl.size())report("device call count guest "+std::to_string(gl.size())+" native "+std::to_string(nl.size())+tag);
        for(std::size_t k=0;k<std::min(gl.size(),nl.size());++k)if(gl[k]!=nl[k]){
            std::ostringstream o;o<<"device call "<<k<<" guest";for(auto v:gl[k])o<<" "<<std::hex<<v;o<<" | native";for(auto v:nl[k])o<<" "<<std::hex<<v;
            report(o.str()+tag);break;}
        // Cube faces: the guest surfaces in allocation order.
        std::vector<std::uint32_t> gsurf;
        fake_d3d9::for_each_object(heap0,[&](std::uint32_t p,std::uint32_t vt){if(vt==fake_d3d9::SurfaceVt)gsurf.push_back(p);});
        if(gsurf.size()!=native.surface_order.size())report("cube surfaces guest "+std::to_string(gsurf.size())+" native "+std::to_string(native.surface_order.size())+tag);
        for(std::size_t k=0;k<std::min(gsurf.size(),native.surface_order.size());++k){
            ++faces_checked;
            if(std::memcmp(reinterpret_cast<const void*>(std::uintptr_t(gsurf[k]+0x10)),native.surfaces.at(native.surface_order[k]).data(),0x4000)){
                const auto* gp=reinterpret_cast<const std::uint32_t*>(std::uintptr_t(gsurf[k]+0x10));
                const auto* np=reinterpret_cast<const std::uint32_t*>(native.surfaces.at(native.surface_order[k]).data());
                for(unsigned t=0;t<0x1000;++t)if(gp[t]!=np[t]){std::ostringstream o;o<<"cube face "<<k<<" texel "<<t<<std::hex<<" guest "<<gp[t]<<" native "<<np[t];report(o.str()+tag);break;}
            }
        }
        // Globals.
        bool ok=true;
        for(auto& r:rg.ranges){if(r.base==0x740c98u)continue;
            for(std::uint32_t k=0;k<r.size&&ok;k+=4){
                const auto a=r.base+k;const auto gv=g32(a),nv=r.words[k/4];
                if(handle_word(a)?((gv!=0u)!=(nv!=0u)):(gv!=nv)){
                    std::ostringstream o;o<<"global 0x"<<std::hex<<a<<" guest "<<gv<<" native "<<nv<<tag;report(o.str());ok=false;}}}
        if(std::uint32_t(queue.flag_depth_8999a0)!=g32(0x8999a0))report("8999A0"+tag);
        p32(fake_d3d9::Heap,heap0);                                     // the case's fake objects are no longer referenced
    }
    std::cerr<<"renderer init: device calls="<<calls_total<<" cube faces="<<faces_checked<<char(10);
    std::cout<<"{\"routine\":\"renderer_init_probe\",\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}"<<'\n';return 0;
}

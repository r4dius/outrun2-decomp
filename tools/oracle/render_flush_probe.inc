if(only=="render_flush_probe"){
    // Original renderer flush (40F6B0 init, 410F90 camera matrices, 4044F0
    // pass records, 405360 queueing, 405890 opaque flush, 405840 alpha sort +
    // flush) against the logging fake IDirect3DDevice9, on banks loaded by the
    // original loader (colour bank COMMON/obj_pc_color + each retail car), and
    // the native port (pc_render_flush, pc_pmt_loader, pc_render_queue) against
    // a logging native device. Every device call of every flush is compared
    // (handles by creation ordinal, shader constants and shader tokens by
    // value), plus the renderer globals after each frame.
    using namespace outrun::platform;
    if(argc<=7||std::string(argv[7])=="-")throw std::runtime_error("render flush oracle requires pinned d3dx9_29.dll");
    const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
    const char* cars=std::getenv("OR2_CARS_DIR");
    if(!cars)throw std::runtime_error("set OR2_CARS_DIR to the retail Cars directory");
    const std::string common=std::string(cars)+"/../COMMON/obj_pc_color_pmt.sz";
    constexpr unsigned base=0x3a800000,car_window=0x5c000000,colour_window=0x58000000;
    map_at(base,0x10000,PROT_READ|PROT_WRITE|PROT_EXEC);map_at(car_window,0x2000000,PROT_READ|PROT_WRITE);
    map_at(colour_window,0x2000000,PROT_READ|PROT_WRITE);
    map_original_arithmetic_bridge(file);
    init_matrix_oracle_globals();wire_d3dx_imports(map_d3dx29(argv[7]));
    {   auto* iat=reinterpret_cast<void*>(0x596000u);
        if(mprotect(iat,4096,PROT_READ|PROT_WRITE))throw std::runtime_error("flush IAT");
        auto* dll=reinterpret_cast<std::uint32_t*>(0x596000u);(void)dll;
        *reinterpret_cast<std::uint32_t*>(0x5962a0u)=D3DX_BASE+(0x4524b8u-0x400000u); // D3DXDeclaratorFromFVF
        *reinterpret_cast<std::uint32_t*>(0x5962b0u)=D3DX_BASE+(0x440ba3u-0x400000u); // D3DXMatrixTranspose (generic)
        *reinterpret_cast<std::uint32_t*>(0x5962dcu)=D3DX_BASE+(0x440e9cu-0x400000u); // D3DXMatrixInverse (generic)
        if(mprotect(iat,4096,PROT_READ))throw std::runtime_error("flush IAT restore");}
    {   // D3DX msvcrt _finite import (used by the generic inverse), as in camera_probe.
        static const std::uint8_t finite_stub[]{0x8b,0x44,0x24,0x08,0x25,0x00,0x00,0xf0,0x7f,0x3d,0x00,0x00,0xf0,0x7f,0x0f,0x95,0xc0,0x0f,0xb6,0xc0,0xc3};
        std::memcpy(reinterpret_cast<void*>(base+0x7000),finite_stub,sizeof(finite_stub));
        auto* iat=reinterpret_cast<void*>(D3DX_BASE+0x1000u);
        if(mprotect(iat,4096,PROT_READ|PROT_WRITE))throw std::runtime_error("d3dx IAT page");
        *reinterpret_cast<std::uint32_t*>(D3DX_BASE+0x1270u)=base+0x7000u;
        if(mprotect(iat,4096,PROT_READ))throw std::runtime_error("d3dx IAT restore");
    }
    {   // operator new (5802CF): the CRT heap is not initialised here; a bump
        // allocator (16-byte aligned) serves the 40B200 pixel shader cache nodes.
        map_at(0x57000000u,0x100000,PROT_READ|PROT_WRITE);
        *reinterpret_cast<std::uint32_t*>(base+0x7100)=0x57000000u;
        std::vector<std::uint8_t> st{0xa1};for(unsigned k=0;k<4;++k)st.push_back(std::uint8_t((base+0x7100)>>(k*8)));
        const std::uint8_t tail[]{0x8b,0x4c,0x24,0x04,0x83,0xc1,0x0f,0x83,0xe1,0xf0,0x01,0x0d};
        st.insert(st.end(),tail,tail+sizeof(tail));for(unsigned k=0;k<4;++k)st.push_back(std::uint8_t((base+0x7100)>>(k*8)));
        st.push_back(0xc3);
        std::memcpy(reinterpret_cast<void*>(base+0x7200),st.data(),st.size());
        r046_write_rel32(0x5802cfu,0xe9,base+0x7200);
    }
    fake_d3d9::install();
    unsigned shadow_leaves=0;
    {   // 46BD30: the shadow volume leaves 422550/422740 are not ported yet; a
        // bare RET here, reported by the native side instead of drawn.
        for(std::uint32_t a:{0x422550u,0x422740u}){auto* pg=reinterpret_cast<void*>(a&~0xfffu);
            if(mprotect(pg,4096,PROT_READ|PROT_WRITE|PROT_EXEC))throw std::runtime_error("shadow leaf patch");
            *reinterpret_cast<std::uint8_t*>(a)=0xc3;mprotect(pg,4096,PROT_READ|PROT_EXEC);}
    }
    if(fake_d3d9::remap_fs0(0x40f4e0u,0x40f700u,base+0xfff0u)!=3)throw std::runtime_error("40F4E0 fs:[0] patch count");
    for(std::uint32_t page=0x62f000u;page<0x98d000u;page+=0x1000u)mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE);
    auto g32=[](std::uint32_t a){return *reinterpret_cast<const std::uint32_t*>(a);};
    auto p32=[](std::uint32_t a,std::uint32_t v){*reinterpret_cast<std::uint32_t*>(a)=v;};
    // ---- native logging device (state tables like the fake) ----
    struct NativeDevice final:PcD3D9Device{
        struct Object{std::uint32_t kind{};std::vector<std::uint8_t> bytes;std::vector<std::uint32_t> words;};
        std::vector<Object> objects;std::vector<std::vector<std::uint32_t>> calls;
        std::array<std::uint32_t,0x400> render{};std::array<std::uint32_t,16*32> stage{},sampler{};std::array<std::uint32_t,256> textures{};
        bool loading=false;
        std::uint32_t add(Object o){objects.push_back(std::move(o));return std::uint32_t(objects.size());}
        void log(std::vector<std::uint32_t> c){if(!loading)calls.push_back(std::move(c));}
        std::uint32_t create_vertex_buffer(std::uint32_t n,std::uint32_t,std::uint32_t,std::uint32_t)override{return add({26,std::vector<std::uint8_t>(n),{}});}
        std::uint32_t create_index_buffer(std::uint32_t n,std::uint32_t,std::uint32_t,std::uint32_t)override{return add({27,std::vector<std::uint8_t>(n),{}});}
        std::uint8_t* lock(std::uint32_t h,std::uint32_t off,std::uint32_t,std::uint32_t)override{return objects.at(h-1).bytes.data()+off;}
        void unlock(std::uint32_t)override{}
        std::uint32_t create_vertex_declaration(const PcVertexElement* e)override{
            std::vector<std::uint32_t> w;
            for(;;++e){w.push_back(e->stream|(std::uint32_t(e->offset)<<16));
                w.push_back(e->type|(std::uint32_t(e->method)<<8)|(std::uint32_t(e->usage)<<16)|(std::uint32_t(e->usage_index)<<24));
                if(e->stream==PcDeclEndStream)break;}
            return add({86,{},w});}
        std::uint32_t create_vertex_shader(const std::uint32_t*)override{return add({91,{},{}});}
        void set_vertex_shader_constant_f(std::uint32_t start,const float* d,std::uint32_t n)override{
            std::vector<std::uint32_t> c{94,start,n};for(std::uint32_t k=0;k<n*4;++k){std::uint32_t u;std::memcpy(&u,d+k,4);c.push_back(u);}log(c);}
        std::uint32_t create_texture_from_file(const PcTextureFileRequest& q)override{return add({q.cube?0x5002u:0x5001u,{},{}});}
        void release(std::uint32_t)override{log({2});}
        std::uint32_t get_render_state(std::uint32_t s)override{log({58,s});return render[s&0x3ffu];}
        void set_render_state(std::uint32_t s,std::uint32_t v)override{log({57,s,v});render[s&0x3ffu]=v;}
        std::uint32_t get_texture(std::uint32_t s)override{log({64,s});return textures[s&0xffu];}
        void set_texture(std::uint32_t s,std::uint32_t t)override{log({65,s,t});textures[s&0xffu]=t;}
        std::uint32_t get_texture_stage_state(std::uint32_t s,std::uint32_t t)override{log({66,s,t});return stage[((s&0xfu)<<5)+(t&0x1fu)];}
        void set_texture_stage_state(std::uint32_t s,std::uint32_t t,std::uint32_t v)override{log({67,s,t,v});stage[((s&0xfu)<<5)+(t&0x1fu)]=v;}
        std::uint32_t get_sampler_state(std::uint32_t s,std::uint32_t t)override{log({68,s,t});return sampler[((s&0xfu)<<5)+(t&0x1fu)];}
        void set_sampler_state(std::uint32_t s,std::uint32_t t,std::uint32_t v)override{log({69,s,t,v});sampler[((s&0xfu)<<5)+(t&0x1fu)]=v;}
        std::uint32_t draw_indexed_primitive(std::uint32_t t,std::int32_t b,std::uint32_t mi,std::uint32_t n,std::uint32_t si,std::uint32_t p)override{
            log({82,t,std::uint32_t(b),mi,n,si,p});return 0;}
        void set_vertex_declaration(std::uint32_t d)override{log({87,d});}
        void set_vertex_shader(std::uint32_t s)override{log({92,s});}
        void set_stream_source(std::uint32_t s,std::uint32_t b,std::uint32_t o,std::uint32_t st)override{log({100,s,b,o,st});}
        void set_indices(std::uint32_t b)override{log({104,b});}
        std::uint32_t create_pixel_shader(const std::uint32_t* t)override{
            std::vector<std::uint32_t> c{106};for(;;++t){c.push_back(*t);if(*t==0xffffu)break;}log(c);return add({106,{},{}});}
        void set_pixel_shader(std::uint32_t s)override{log({107,s});}
        void set_pixel_shader_constant_f(std::uint32_t start,const float* d,std::uint32_t n)override{
            std::vector<std::uint32_t> c{109,start,n};for(std::uint32_t k=0;k<n*4;++k){std::uint32_t u;std::memcpy(&u,d+k,4);c.push_back(u);}log(c);}
        void set_light(std::uint32_t i,const std::uint32_t* l)override{std::vector<std::uint32_t> c{51,i};c.insert(c.end(),l,l+26);log(c);}
        void light_enable(std::uint32_t i,std::uint32_t e)override{log({53,i,e});}
        std::uint32_t get_declaration(std::uint32_t d,PcVertexElement* out)override{
            log({0x2004});const auto& w=objects.at(d-1).words;const auto n=std::uint32_t(w.size()/2);
            if(out)for(std::uint32_t k=0;k<n;++k)out[k]=PcVertexElement{std::uint16_t(w[2*k]),std::uint16_t(w[2*k]>>16),
                std::uint8_t(w[2*k+1]),std::uint8_t(w[2*k+1]>>8),std::uint8_t(w[2*k+1]>>16),std::uint8_t(w[2*k+1]>>24)};
            return n;}
    } native;
    // ---- guest object ordinals (heap walk) ----
    std::map<std::uint32_t,std::uint32_t> ordinal;std::uint32_t objects_seen=0,heap_mark=fake_d3d9::heap_top();
    auto sync_ordinals=[&]{fake_d3d9::for_each_object(heap_mark,[&](std::uint32_t p,std::uint32_t){ordinal[p]=++objects_seen;});heap_mark=fake_d3d9::heap_top();};
    auto gid=[&](std::uint32_t p)->std::uint32_t{if(!p)return 0;auto it=ordinal.find(p);if(it==ordinal.end())return 0xbad00000u|(p&0xfffffu);return it->second;};
    // ---- banks ----
    struct Loaded{std::vector<std::uint8_t> pmt;PcPmtResources r;std::uint32_t system{},video{},bank{};};
    auto inflate=[&](const std::string& path){
        std::ifstream in(path,std::ios::binary);if(!in)throw std::runtime_error("missing "+path);
        std::vector<std::uint8_t> packed((std::istreambuf_iterator<char>(in)),std::istreambuf_iterator<char>());
        std::vector<std::uint8_t> pmt(64u*1024u*1024u);uLongf size=uLongf(pmt.size());
        if(uncompress(pmt.data(),&size,packed.data(),uLong(packed.size()))!=Z_OK)throw std::runtime_error("inflate "+path);
        pmt.resize(size);return pmt;};
    PcShaderCache cache{};PcShaderGlobals sg{};sg.generated_key_1039ec0=g32(0x1039ec0);
    auto load=[&](Loaded& l,const std::string& path,std::uint32_t resource,std::uint32_t window){
        l.pmt=inflate(path);Bytes pb(l.pmt.data(),l.pmt.size());
        const auto ss=pb.u32(8),vs=pb.u32(12);
        if(ss+vs>0x1f00000u)throw std::runtime_error("PMT larger than the guest window");
        l.system=window;l.video=window+((ss+0xfffu)&~0xfffu);
        std::memcpy(reinterpret_cast<void*>(l.system),l.pmt.data()+16,ss);
        std::memcpy(reinterpret_cast<void*>(l.video),l.pmt.data()+16+ss,vs);
        l.bank=0x7c2800u+resource*0x48u;std::memset(reinterpret_cast<void*>(l.bank),0,0x48);
        const std::uint32_t cell=base+0x200+(resource&0xffu)*8u;
        p32(cell,l.system);p32(cell+4,7);p32(l.bank+0x28,cell);p32(l.bank+0x08,l.video);p32(l.bank+0x30,7);
        sg.mode_89edbc=g32(0x89edbc);sg.override_89edd4=g32(0x89edd4);
        fake_d3d9::reset_log();
        prepare(0x42e490);guest_call.eax=l.bank;run_original32();
        unsigned steps=0;do{prepare(0x42e4f0);*reinterpret_cast<unsigned*>(S)=l.bank;run_original32();++steps;}while((guest_call.out_eax&0xffu)==0u&&steps<4096u);
        prepare(0x42e850);guest_call.eax=l.bank;run_original32();
        steps=0;do{prepare(0x42e8c0);guest_call.edi=l.bank;run_original32();++steps;}while((guest_call.out_eax&0xffu)==0u&&steps<4096u);
        sync_ordinals();
        l.r=PcPmtResources{};l.r.system.assign(l.pmt.begin()+16,l.pmt.begin()+16+ss);l.r.video=l.pmt.data()+16+ss;l.r.video_size=vs;
        PcPmtLoadCursor cur{};native.loading=true;
        pmt_objects_begin_42e490(l.r,cur);while(!pmt_objects_step_42e4f0(l.r,cur,native,cache,sg)){}
        pmt_textures_begin_42e850(l.r,cur);while(!pmt_textures_step_42e8c0(l.r,cur,native)){}
        native.loading=false;
        if(native.objects.size()!=objects_seen)throw std::runtime_error("native/guest object counts differ after loading "+path);
    };
    Loaded colour;load(colour,common,0xbb,colour_window);
    PcRenderGlobals rg;
    // Seed the native globals from guest memory (file .data values) once.
    for(auto& r:rg.ranges)if(r.base!=0x740c98u)for(std::uint32_t k=0;k<r.size;k+=4)r.words[k/4]=g32(r.base+k);
    std::map<std::uint32_t,Loaded*> resident{{0xbbu,&colour}};
    PcFlushContext fc{native,rg,[&](std::uint32_t r)->PcPmtResources*{auto it=resident.find(r);return it==resident.end()?nullptr:&it->second->r;},
        pc_colour_list_alt_408c80,1};
    std::mt19937 rng(0x4f52464cu);
    auto real=[&](float lo,float hi){return lo+(hi-lo)*(float(rng()&0xffffu)/65535.f);};
    auto both32=[&](std::uint32_t a,std::uint32_t v){p32(a,v);rg.w(a)=v;};
    auto bothf=[&](std::uint32_t a,float f){std::uint32_t u;std::memcpy(&u,&f,4);both32(a,u);};
    unsigned cases=0,mismatches=0,calls_compared=0,draws=0,pixel_shaders=0,overrides=0,displays=0,morphs=0;
    auto report=[&](const std::string& what){++mismatches;if(mismatches<=12)std::cerr<<"MISMATCH "<<what<<char(10);};
    // Guest log -> normalised calls.
    auto guest_calls=[&]{
        std::vector<std::vector<std::uint32_t>> out;std::uint32_t data=0;const auto* df=fake_d3d9::data_log();
        auto datum=[&](std::uint32_t n,std::vector<std::uint32_t>& c){for(std::uint32_t k=0;k<n*4u;++k){std::uint32_t u;std::memcpy(&u,df+data+k,4);c.push_back(u);}data+=n*4u;};
        const auto n=fake_d3d9::log_count();
        for(std::uint32_t k=0;k<n;++k){
            const auto* r=fake_d3d9::log_record(k);const auto id=r[0];const auto* a=r+2;
            switch(id){
            case 57:out.push_back({57,a[0],a[1]});break;
            case 58:out.push_back({58,a[0]});break;
            case 64:out.push_back({64,a[0]});break;
            case 65:out.push_back({65,a[0],gid(a[1])});break;
            case 66:case 68:out.push_back({id,a[0],a[1]});break;
            case 67:case 69:out.push_back({id,a[0],a[1],a[2]});break;
            case 82:out.push_back({82,a[0],a[1],a[2],a[3],a[4],a[5]});break;
            case 87:case 92:case 104:case 107:out.push_back({id,gid(a[0])});break;
            case 94:case 109:{std::vector<std::uint32_t> c{id,a[0],a[2]};datum(a[2],c);
                // 4109C0 cone constants 0x30/0x31: w is an uninitialised stack slot on the PC.
                if(id==94&&(a[0]==0x30u||a[0]==0x31u)&&c.size()==7)c[6]=0;
                out.push_back(c);break;}
            case 100:out.push_back({100,a[0],gid(a[1]),a[2],a[3]});break;
            case 106:{ // tokens from the created object (copied at creation; a[0] is a stack buffer)
                std::vector<std::uint32_t> c{106};const auto obj=g32(a[1]);
                for(std::uint32_t t=obj+0x20;;t+=4){c.push_back(g32(t));if(g32(t)==0xffffu)break;}out.push_back(c);break;}
            case 53:out.push_back({53,a[0],a[1]});break;
            case 51:{std::vector<std::uint32_t> c{51,a[0]};for(unsigned w=0;w<26;++w){std::uint32_t u;std::memcpy(&u,df+data+w,4);c.push_back(u);}data+=26;out.push_back(c);break;}
            case 0x2004:out.push_back({0x2004});break;
            case 0x4002:case 0x1002:case 0x2002:case 0x3002:out.push_back({2});break;
            default:out.push_back({0xdead0000u|id});break;
            }
        }
        return out;
    };
    auto compare_calls=[&](const std::string& what){
        sync_ordinals();auto g=guest_calls();
        // IDirect3DVertexDeclaration9::GetDeclaration (0x2004) is a pure query:
        // the port caches each declaration's vertex size (404700) instead of
        // asking for every draw, so it is left out of the comparison.
        auto queries=[](const std::vector<std::uint32_t>& c){return c.size()==1&&c[0]==0x2004u;};
        g.erase(std::remove_if(g.begin(),g.end(),queries),g.end());
        native.calls.erase(std::remove_if(native.calls.begin(),native.calls.end(),queries),native.calls.end());
        // Pixel shaders created during the flush: remap guest ordinals created now.
        const auto& n=native.calls;
        ++cases;calls_compared+=unsigned(g.size());
        for(const auto& c:n)if(c[0]==82)++draws;else if(c[0]==106)++pixel_shaders;
        if(g.size()!=n.size()){report(what+" call count guest="+std::to_string(g.size())+" native="+std::to_string(n.size()));}
        for(std::size_t k=0;k<std::min(g.size(),n.size());++k)if(g[k]!=n[k]){
            std::ostringstream o;o<<what<<" call "<<k<<" guest";for(auto v:g[k])o<<" "<<std::hex<<v;o<<" | native";for(auto v:n[k])o<<" "<<std::hex<<v;
            report(o.str());break;}
        native.calls.clear();fake_d3d9::reset_log();
    };
    // Renderer init (40F6B0) on both.
    fake_d3d9::reset_log();native.calls.clear();
    prepare(0x40f6b0);run_original32();render_state_init_40f6b0(fc);
    compare_calls("40F6B0");
    // Environment/lighting inputs shared by every frame.
    auto randomise_inputs=[&]{
        // Lights through the light API (410710 ambient, 410740 reset, 4107A0 per light).
        fake_d3d9::reset_log();native.calls.clear();
        for(std::uint32_t o=0;o<0xc;o+=4)bothf(0x95dc20+o,real(0.f,1.f));
        {   std::array<float,4> amb{real(0.f,0.6f),real(0.f,0.6f),real(0.f,0.6f),1.f};
            std::memcpy(reinterpret_cast<void*>(base+0x2200),amb.data(),16);
            prepare(0x410710);guest_call.eax=base+0x2200;run_original32();render_light_ambient_410710(fc,amb);}
        prepare(0x410740);run_original32();render_lights_reset_410740(fc);
        const unsigned lights=1u+rng()%5u;
        for(unsigned k=0;k<lights;++k){
            std::array<std::uint8_t,0x94> desc{};Bytes db(desc.data(),desc.size());
            for(std::uint32_t o=8;o<0x94;o+=4)db.putf(o,real(-1.5f,1.5f));
            db.put32(0,rng()%4u!=0u);db.put32(4,k==0?3u:1u+rng()%4u);
            db.putf(0x90,real(0.5f,2.5f));db.putf(0x64,real(0.2f,1.5f));db.putf(0x68,real(0.1f,1.2f));
            std::memcpy(reinterpret_cast<void*>(base+0x2300),desc.data(),desc.size());
            prepare(0x4107a0);*reinterpret_cast<unsigned*>(S)=base+0x2300;run_original32();
            render_light_add_4107a0(fc,db);
        }
        compare_calls("light API");
        for(std::uint32_t o=0;o<0x40;o+=4)bothf(0x95c018+o,real(-1.f,1.f));
        bothf(0x8a8c1c,real(0.25f,2.f));
        for(std::uint32_t o=0;o<0x40;o+=4)bothf(0x8a8a04+o,real(-1.f,1.f));
        auto byte=[&](std::uint32_t a,std::uint8_t v){auto& w=*reinterpret_cast<std::uint8_t*>(a);w=v;rg.put_byte(a,v);};
        byte(0x95af0a,std::uint8_t(rng()&1u));byte(0x95af0b,std::uint8_t(rng()&1u));
        // Environment texture 89A548: texture 0 of the colour bank on both sides.
        const auto gt=g32(g32(colour.bank+0x24)),nt=colour.r.view().u32(colour.r.view().u32(colour.r.texture_table_24));
        p32(0x89a548,g32(gt));rg.w(0x89a548)=nt;
    };
    for(unsigned car=0;car<30;++car){
        const unsigned resource=VehicleResourceIds[car];
        Loaded vehicle;load(vehicle,std::string(cars)+"/"+(VehicleResourcePaths[car]+5),resource,car_window);
        resident[resource]=&vehicle;
        // Guest queue-building relocation is done by the loader (42E4F0).
        const std::uint32_t objects=g32(vehicle.bank+0x0c);
        // Frames 0-2 queue then flush with 405890/405840; frame 3 runs in the
        // 8999B0 immediate mode (every 405360 flushes through 4052C0) and
        // frame 4 queues and ends with 4052C0; frames 3-4 mix 4056D0 colour
        // overrides (undone by 4052C0) with 405360.
        for(unsigned frame=0;frame<5;++frame){
            randomise_inputs();
            // Camera: projection (slot 1) then view (slot 0) through 410F90 on both.
            PcCameraDevice cam;std::array<std::uint8_t,4*64> ca{};PcMatrixStack cs{Bytes(ca.data(),ca.size()),0,0,4};
            outrun::driving::pc_matrix_identity(cs);
            camera_perspective_404310(cs,real(0.6f,1.2f),1.333f,real(0.1f,1.f),real(200,3000),0.f,0.f,cam);
            std::array<float,16> proj{};
            for(unsigned k=0;k<16;++k)proj[k]=cs.current().f32(k*4);
            PcRenderView view{};
            const float yaw=real(-3.1f,3.1f),dist=real(3,frame==2?60.f:12.f);
            view.eye={std::sin(yaw)*dist,real(0.5f,4),std::cos(yaw)*dist};view.target={0.f,0.5f,0.f};
            std::array<float,16> vm{};
            {   std::array<std::uint8_t,2*64> a{};PcMatrixStack vs{Bytes(a.data(),a.size()),0,0,2};outrun::driving::pc_matrix_identity(vs);
                outrun::driving::pc_matrix_rotate_x(vs,real(-0.3f,0.1f));outrun::driving::pc_matrix_rotate_y(vs,-yaw);
                outrun::driving::pc_matrix_translate_vector(vs,{-view.eye[0],-view.eye[1],-view.eye[2]});
                for(unsigned k=0;k<16;++k)vm[k]=vs.current().f32(k*4);
                for(unsigned k=0;k<64;++k)view.view_95d860[k]=vs.current().u8(k);}
            view.frustum_95bf40=cam.frustum_95bf40;view.planes_95bf58=cam.planes_95bf58;
            std::memcpy(reinterpret_cast<void*>(0x95bf40),view.frustum_95bf40.data(),24);
            std::memcpy(reinterpret_cast<void*>(0x95bf58),view.planes_95bf58.data(),64);
            for(unsigned k=0;k<6;++k)rg.putf(0x95bf40+k*4,view.frustum_95bf40[k]);
            for(unsigned k=0;k<16;++k)rg.putf(0x95bf58+k*4,view.planes_95bf58[k/4][k%4]);
            fake_d3d9::reset_log();native.calls.clear();
            std::memcpy(reinterpret_cast<void*>(base+0x2000),proj.data(),64);std::memcpy(reinterpret_cast<void*>(base+0x2040),vm.data(),64);
            prepare(0x410f90);guest_call.eax=base+0x2000;guest_call.edx=1;run_original32();
            render_set_matrix_410f90(fc,proj,1);
            prepare(0x410f90);guest_call.eax=base+0x2040;guest_call.edx=0;run_original32();render_set_matrix_410f90(fc,vm,0);
            compare_calls("410F90 camera car "+std::to_string(car));
            // Pass records as the car draw list sets them (46AE70 states/alt states).
            const bool alt=(rng()%3u)==0u;
            auto pass=[&](std::uint32_t p,std::uint32_t a1,std::uint32_t a2,std::uint32_t a3,std::uint32_t a4){
                prepare(0x4044f0);auto* st=reinterpret_cast<unsigned*>(S);st[0]=p;st[1]=a1;st[2]=a2;st[3]=a3;st[4]=a4;run_original32();
                render_pass_record_4044f0(rg,p,a1,a2,a3,a4);};
            if(!alt){pass(0,0,0,0x80,0x1000000f);pass(1,1,0,8,0xf);}
            else{pass(0,1,0,8,7);pass(1,1,0,8,7);}
            // Queues: original and native 405360 on the same objects.
            *reinterpret_cast<std::uint32_t*>(0x79f574)=base+0x3000;
            std::memcpy(reinterpret_cast<void*>(base+0x3000+0xf8),view.eye.data(),12);
            std::memcpy(reinterpret_cast<void*>(base+0x3000+0x104),view.target.data(),12);
            std::memcpy(reinterpret_cast<void*>(0x95d860),view.view_95d860.data(),64);
            prepare(0x405160);run_original32();p32(0x8999b0,0);
            PcRenderQueue opaque,alpha;render_queues_reset_405160(opaque,alpha);
            reset_matrix_oracle_globals();
            std::vector<std::uint8_t> arena(0x1000);Bytes ar(arena.data(),arena.size());
            for(unsigned k=0;k<16;++k)ar.putf(k*4,(k%5u)==0u?1.f:0.f);
            std::memcpy(reinterpret_cast<void*>(MatrixArena),arena.data(),arena.size());
            PcMatrixStack stack{ar,0,0,64};
            const std::int32_t lod=std::int32_t(rng()%12u);
            p32(0x8999b8,std::uint32_t(lod));p32(0x8999a0,0);
            PcModelBank qbank{resource,vehicle.r.view(),objects};
            PcRenderContext qc{opaque,alpha,view,stack,[&](std::uint32_t id)->const PcModelBank*{return id==resource?&qbank:nullptr;},lod};
            const std::uint32_t list=EmbeddedVehicleColourIds[car];
            const std::int32_t cb=std::int32_t(rng()%9u)-1;
            if(frame>=3){p32(0x8999b0,frame==3?1u:0u);qc.immediate_8999b0=frame==3?1u:0u;p32(0x864f14,0);}
            qc.flush_4052c0=[&]{render_queue_flush_4052c0(fc,qc);};
            if(frame>=3){fake_d3d9::reset_log();native.calls.clear();}
            for(unsigned object=0;object<objects;++object){
                const std::uint32_t id=(resource<<16)|object;
                const std::int32_t to_opaque=std::int32_t(rng()%3u!=0u);
                if(frame>=3&&(rng()%3u)==0u){
                    const std::uint32_t value=rng();
                    prepare(0x4056d0);auto* st=reinterpret_cast<unsigned*>(S);
                    st[0]=id;st[1]=value;st[2]=list;st[3]=unsigned(cb);run_original32();
                    render_object_override_4056d0(qc,id,value,list,cb);++overrides;
                    continue;
                }
                if(frame<3&&(rng()%4u)==0u){
                    // 405450 morph on itself (+2C target, +30 weight 95AECC): 404700 stream 1
                    // and the 405890 constant 7.
                    const float wt=real(0,1);std::uint32_t wu;std::memcpy(&wu,&wt,4);
                    p32(0x95aecc,wu);qc.morph_weight_95aecc=wu;
                    prepare(0x405450);auto* st=reinterpret_cast<unsigned*>(S);
                    st[0]=id;st[1]=id;st[2]=unsigned(to_opaque);st[3]=list;st[4]=unsigned(cb);run_original32();
                    render_object_morph_405450(qc,id,id,to_opaque,list,cb);++morphs;
                    continue;
                }
                prepare(0x405360);auto* st=reinterpret_cast<unsigned*>(S);
                st[0]=id;st[1]=unsigned(to_opaque);st[2]=0;st[3]=list;st[4]=unsigned(cb);st[5]=0;run_original32();
                render_object_405360(qc,id,to_opaque,Bytes(nullptr,0),list,cb,0);
            }
            if(g32(0x864ef8)!=opaque.count||g32(0x8612d8)!=alpha.count||g32(0x864f14)!=opaque.undo.size()||g32(0x8999b0)!=qc.immediate_8999b0)
                report("queue counts car "+std::to_string(car)+" frame "+std::to_string(frame));
            // Opaque flush.
            if(frame<3){fake_d3d9::reset_log();native.calls.clear();}
            if(frame==4){prepare(0x4052c0);run_original32();render_queue_flush_4052c0(fc,qc);}
            else if(frame<3){
            prepare(0x405890);*reinterpret_cast<unsigned*>(S)=0x864ef8;run_original32();
            {std::vector<std::uint32_t> order(opaque.count);for(std::uint32_t k=0;k<opaque.count;++k)order[k]=k;
             render_queue_flush_405890(fc,opaque,order);}}
            if(g32(0x864ef8)!=opaque.count||g32(0x864f14)!=opaque.undo.size()||g32(0x8999b0)!=qc.immediate_8999b0)
                report("4052C0 state car "+std::to_string(car)+" frame "+std::to_string(frame));
            if(inject&&car==29&&frame==0&&!native.calls.empty())native.calls.back().push_back(1);
            compare_calls("opaque flush car "+std::to_string(car)+" frame "+std::to_string(frame));
            // Alpha: 405840 sorts, flushes and clears.
            prepare(0x405840);run_original32();
            {std::vector<std::uint32_t> order(alpha.count);for(std::uint32_t k=0;k<alpha.count;++k)order[k]=k;
             if(alpha.count)render_queue_sort_4499e0(alpha,order,0,std::int32_t(alpha.count)-1);
             render_queue_flush_405890(fc,alpha,order);}
            compare_calls("alpha flush car "+std::to_string(car)+" frame "+std::to_string(frame));
            // Renderer globals (pointer words compared through the object maps).
            for(const auto& r:rg.ranges){
                if(r.base==0x740c98u)continue;
                for(std::uint32_t k=0;k<r.size;k+=4){
                    const auto a=r.base+k;auto gv=g32(a),nv=r.words[k/4];
                    if(a==0x89a4f8u||a==0x95aeecu||a==0x95aef0u)continue;
                    if(a==0x89a51cu||a==0x89a548u){gv=gid(gv);}
                    if(a==0x95aee8u){gv=gid(gv);}
                    ++cases;
                    if(gv!=nv){std::ostringstream o;o<<"global 0x"<<std::hex<<a<<" guest "<<gv<<" native "<<nv<<" car "<<std::dec<<car;report(o.str());break;}
                }
            }
        }
        // 46C140 end to end: original (4082B0 light, 40D840 environment,
        // 409F90, 46AE70 with its bridges, 405360/4056D0/4052C0 immediate
        // draws, 40A010) against vehicle_display_46c140, twice per car.
        for(unsigned run=0;run<2;++run){
            randomise_inputs();
            constexpr unsigned car_va=base+0xa000;
            std::vector<std::uint8_t> ev(0x10f0);Bytes e(ev.data(),ev.size());
            for(unsigned k=0;k<0x10f0;k+=4)e.putf(k,real(-2,2));
            e.put8(0x11,std::uint8_t(car));e.put8(0x12,std::uint8_t(rng()%8u));
            std::uint32_t flags=rng();if(run==0)flags=(flags&~0xc000u)|0x4000u;
            e.put32(4,flags);e.put32(0x38,std::uint32_t(rng()%400u));e.put32(0x1f4,rng()%1000u);
            e.put16(0x204,std::uint16_t(rng()));e.put16(0x2e,std::uint16_t(rng()));
            e.putf(0x1c4,real(-3,3));e.putf(0x2fc,real(-0.5f,0.8f));e.putf(0x2c8,real(-1,1));e.putf(0x2dc,real(-1,2));e.putf(0xb68,real(0,1));
            e.putf(0x58,real(0.3f,1.5f));
            for(unsigned k=0;k<16;++k)e.putf(0xf0+k*4,(k%5u)==0u?1.f:0.f);
            for(unsigned k=0;k<16;++k)e.putf(0xb0+k*4,(k%5u)==0u?1.f:0.f);
            e.putf(0xb0+0x38,real(-1,1));
            std::vector<std::uint8_t> body(0x800);for(auto& b:body)b=std::uint8_t(rng());
            std::memcpy(reinterpret_cast<void*>(car_va),ev.data(),ev.size());
            std::memcpy(reinterpret_cast<void*>(0x82e7f0),body.data(),body.size());
            const std::int32_t mode=32;const std::uint8_t scene=std::uint8_t(rng()%20u);
            *reinterpret_cast<std::int32_t*>(0x64d198)=-1;*reinterpret_cast<std::int32_t*>(0x78026c)=mode;*reinterpret_cast<std::uint8_t*>(0x82e7d4)=scene;
            // Environment records and fog, flags 79FCCA (&3 == 2 mostly).
            std::vector<std::uint8_t> lights(0x960),fog(0x54);
            for(unsigned r=0;r<15;++r){Bytes l(lights.data()+r*0xa0,0xa0);
                for(unsigned k=0;k<0xa0;k+=4)l.putf(k,real(-1.f,1.f));
                l.put32(0,rng()%4u!=0u);l.put32(4,r==2?3u:1u+rng()%3u);
                for(unsigned k=0x08;k<0x38;k+=4)l.putf(k,real(0.f,1.2f));
                l.putf(0x50,real(1.f,500.f));l.putf(0x64,real(0.2f,1.5f));l.putf(0x68,real(0.1f,1.2f));l.putf(0x90,real(0.5f,2.5f));}
            for(unsigned r=0;r<3;++r){Bytes f(fog.data()+r*0x1c,0x1c);for(unsigned k=0;k<0x1c;k+=4)f.put32(k,rng());
                f.put32(0,1);f.put32(4,rng()%4u);f.putf(8,real(0,100));f.putf(0xc,real(200,3000));f.putf(0x10,real(0.0005f,0.02f));}
            std::memcpy(reinterpret_cast<void*>(0x899b98u),lights.data(),lights.size());
            std::memcpy(reinterpret_cast<void*>(0x7d3a10u),fog.data(),fog.size());
            const std::uint8_t envflags=std::uint8_t((rng()%5u)?2u:rng()),pixel=std::uint8_t(rng()&1u);
            *reinterpret_cast<std::uint8_t*>(0x79fccau)=envflags;*reinterpret_cast<std::uint8_t*>(0x740c88u)=pixel;
            const std::uint32_t layer=run?6u:3u;p32(0x7d25f0,layer);
            // Queues in immediate mode (the default outside 4052B0 sections).
            prepare(0x405160);run_original32();p32(0x8999b0,1);p32(0x864f14,0);
            PcRenderQueue opaque,alpha;render_queues_reset_405160(opaque,alpha);
            reset_matrix_oracle_globals();
            std::vector<std::uint8_t> arena(0x1000);Bytes ar(arena.data(),arena.size());
            for(unsigned k=0;k<16;++k)ar.putf(k*4,(k%5u)==0u?1.f:0.f);
            std::memcpy(reinterpret_cast<void*>(MatrixArena),arena.data(),arena.size());
            PcMatrixStack stack{ar,0,0,64};
            p32(0x8999b8,7);p32(0x8999a0,0);
            PcRenderView view{};
            for(unsigned k=0;k<64;++k)view.view_95d860[k]=*reinterpret_cast<const std::uint8_t*>(0x95d860+k);
            std::memcpy(view.frustum_95bf40.data(),reinterpret_cast<const void*>(0x95bf40),24);
            std::memcpy(view.planes_95bf58.data(),reinterpret_cast<const void*>(0x95bf58),64);
            std::memcpy(view.eye.data(),reinterpret_cast<const void*>(base+0x3000+0xf8),12);
            std::memcpy(view.target.data(),reinterpret_cast<const void*>(base+0x3000+0x104),12);
            PcModelBank qbank{resource,vehicle.r.view(),objects};
            PcRenderContext qc{opaque,alpha,view,stack,[&](std::uint32_t id)->const PcModelBank*{return id==resource?&qbank:nullptr;},7};
            qc.immediate_8999b0=1;qc.flush_4052c0=[&]{render_queue_flush_4052c0(fc,qc);};
            fake_d3d9::reset_log();native.calls.clear();
            prepare(0x46c140);*reinterpret_cast<unsigned*>(S)=car_va;run_original32();
            PcVehicleDisplayContext vc{fc,qc,PcEnvironmentRenderTables{Bytes(lights.data(),lights.size()),Bytes(fog.data(),fog.size()),pixel},
                envflags,layer,stack,Bytes(body.data(),body.size()),mode,scene};
            vehicle_display_46c140(vc,e);
            ++displays;
            if(inject&&car==28&&run==1&&!native.calls.empty())native.calls.front().push_back(1);
            compare_calls("46C140 car "+std::to_string(car)+" run "+std::to_string(run));
            if(std::memcmp(reinterpret_cast<const void*>(0x899b98u),lights.data(),lights.size()))report("46C140 light table car "+std::to_string(car));
            if(std::memcmp(reinterpret_cast<const void*>(car_va),ev.data(),ev.size()))report("46C140 car object car "+std::to_string(car));
            if(g32(0x864ef8)!=opaque.count||g32(0x8612d8)!=alpha.count||g32(0x864f14)!=opaque.undo.size()||g32(0x8999b0)!=qc.immediate_8999b0)
                report("46C140 queue state car "+std::to_string(car));
            for(const auto& r:rg.ranges){
                if(r.base==0x740c98u)continue;
                for(std::uint32_t k=0;k<r.size;k+=4){
                    const auto a=r.base+k;auto gv=g32(a),nv=r.words[k/4];
                    if(a==0x89a4f8u||a==0x95aeecu||a==0x95aef0u)continue;
                    if(a==0x89a51cu||a==0x89a548u||a==0x95aee8u)gv=gid(gv);
                    ++cases;
                    if(gv!=nv){std::ostringstream o;o<<"46C140 global 0x"<<std::hex<<a<<" guest "<<gv<<" native "<<nv<<" car "<<std::dec<<car;report(o.str());break;}
                }
            }
            // Leave the alpha queue empty for the next car.
            prepare(0x405840);run_original32();
            {std::vector<std::uint32_t> order(alpha.count);for(std::uint32_t k=0;k<alpha.count;++k)order[k]=k;
             if(alpha.count)render_queue_sort_4499e0(alpha,order,0,std::int32_t(alpha.count)-1);
             render_queue_flush_405890(fc,alpha,order);}
            compare_calls("46C140 alpha car "+std::to_string(car));
        }
        for(unsigned run=0;run<4;++run){
            randomise_inputs();
            constexpr unsigned car_va=base+0xa000;
            std::vector<std::uint8_t> ev(0x10f0);Bytes e(ev.data(),ev.size());
            for(unsigned k=0;k<0x10f0;k+=4)e.putf(k,real(-2,2));
            e.put8(0x11,std::uint8_t(car));e.put8(0x12,std::uint8_t(rng()%8u));
            std::uint32_t flags=rng();if(run==0)flags=(flags&~0xc000u)|0x4000u;
            e.put32(4,flags);e.put32(0x38,std::uint32_t(rng()%400u));e.put32(0x1f4,rng()%1000u);
            e.put16(0x204,std::uint16_t(rng()));e.put16(0x2e,std::uint16_t(rng()));
            e.putf(0x1c4,real(-3,3));e.putf(0x2fc,real(-0.5f,0.8f));e.putf(0x2c8,real(-1,1));e.putf(0x2dc,real(-1,2));e.putf(0xb68,real(0,1));
            e.putf(0x58,real(0.3f,1.5f));
            e.putf(0xe9c,real(0,1.5f));e.put32(0x2bc,0);e.put32(0x2c4,0);
            e.put32(0xd10,run>=2&&(rng()&1u)?1u:rng()%3u);e.put32(0xd18,std::uint32_t(std::int32_t(rng()%7u)-2));
            if(run==1)flags=(flags&~0xc000u)|0x8000u;e.put32(4,flags);
            for(unsigned k=0;k<16;++k)e.putf(0xf0+k*4,(k%5u)==0u?1.f:0.f);
            for(unsigned k=0;k<16;++k)e.putf(0xb0+k*4,(k%5u)==0u?1.f:0.f);
            e.putf(0xb0+0x38,real(-1,1));
            std::vector<std::uint8_t> body(0x800);for(auto& b:body)b=std::uint8_t(rng());
            std::memcpy(reinterpret_cast<void*>(car_va),ev.data(),ev.size());
            std::memcpy(reinterpret_cast<void*>(0x82e7f0),body.data(),body.size());
            const std::int32_t mode=(rng()&1u)?16:32;const std::uint8_t scene=std::uint8_t(rng()%20u);
            *reinterpret_cast<std::int32_t*>(0x64d198)=-1;*reinterpret_cast<std::int32_t*>(0x78026c)=mode;*reinterpret_cast<std::uint8_t*>(0x82e7d4)=scene;
            // Environment records and fog, flags 79FCCA (&3 == 2 mostly).
            std::vector<std::uint8_t> lights(0x960),fog(0x54);
            for(unsigned r=0;r<15;++r){Bytes l(lights.data()+r*0xa0,0xa0);
                for(unsigned k=0;k<0xa0;k+=4)l.putf(k,real(-1.f,1.f));
                l.put32(0,rng()%4u!=0u);l.put32(4,r==2?3u:1u+rng()%3u);
                for(unsigned k=0x08;k<0x38;k+=4)l.putf(k,real(0.f,1.2f));
                l.putf(0x50,real(1.f,500.f));l.putf(0x64,real(0.2f,1.5f));l.putf(0x68,real(0.1f,1.2f));l.putf(0x90,real(0.5f,2.5f));}
            for(unsigned r=0;r<3;++r){Bytes f(fog.data()+r*0x1c,0x1c);for(unsigned k=0;k<0x1c;k+=4)f.put32(k,rng());
                f.put32(0,1);f.put32(4,rng()%4u);f.putf(8,real(0,100));f.putf(0xc,real(200,3000));f.putf(0x10,real(0.0005f,0.02f));}
            std::memcpy(reinterpret_cast<void*>(0x899b98u),lights.data(),lights.size());
            std::memcpy(reinterpret_cast<void*>(0x7d3a10u),fog.data(),fog.size());
            const std::uint8_t envflags=std::uint8_t((rng()%5u)?2u:rng()),pixel=std::uint8_t(rng()&1u);
            *reinterpret_cast<std::uint8_t*>(0x79fccau)=envflags;*reinterpret_cast<std::uint8_t*>(0x740c88u)=pixel;
            const std::uint32_t layer=run?6u:3u;p32(0x7d25f0,layer);
            // Queues in immediate mode (the default outside 4052B0 sections).
            prepare(0x405160);run_original32();p32(0x8999b0,1);p32(0x864f14,0);
            PcRenderQueue opaque,alpha;render_queues_reset_405160(opaque,alpha);
            reset_matrix_oracle_globals();
            std::vector<std::uint8_t> arena(0x1000);Bytes ar(arena.data(),arena.size());
            for(unsigned k=0;k<16;++k)ar.putf(k*4,(k%5u)==0u?1.f:0.f);
            std::memcpy(reinterpret_cast<void*>(MatrixArena),arena.data(),arena.size());
            PcMatrixStack stack{ar,0,0,64};
            p32(0x8999b8,7);p32(0x8999a0,0);
            PcRenderView view{};
            for(unsigned k=0;k<64;++k)view.view_95d860[k]=*reinterpret_cast<const std::uint8_t*>(0x95d860+k);
            std::memcpy(view.frustum_95bf40.data(),reinterpret_cast<const void*>(0x95bf40),24);
            std::memcpy(view.planes_95bf58.data(),reinterpret_cast<const void*>(0x95bf58),64);
            std::memcpy(view.eye.data(),reinterpret_cast<const void*>(base+0x3000+0xf8),12);
            std::memcpy(view.target.data(),reinterpret_cast<const void*>(base+0x3000+0x104),12);
            PcModelBank qbank{resource,vehicle.r.view(),objects};
            PcRenderContext qc{opaque,alpha,view,stack,[&](std::uint32_t id)->const PcModelBank*{return id==resource?&qbank:nullptr;},7};
            qc.immediate_8999b0=1;qc.flush_4052c0=[&]{render_queue_flush_4052c0(fc,qc);};
            fake_d3d9::reset_log();native.calls.clear();
            const std::uint32_t net=run>=2?1u:0u,net2=rng()&1u;const std::uint8_t pause=std::uint8_t(rng()&1u);
            p32(0x7f94c0,net);p32(0x800abc,net2);*reinterpret_cast<std::uint8_t*>(0x780248u)=pause;
            const float ph0=real(-3,3),ph1=real(-3,3);std::memcpy(reinterpret_cast<void*>(0x7f9424u),&ph0,4);std::memcpy(reinterpret_cast<void*>(0x7f9428u),&ph1,4);
            PcRaceCarDisplayState rst{};rst.phase_7f9424=ph0;rst.phase_7f9428=ph1;
            prepare(0x46bd30);*reinterpret_cast<unsigned*>(S)=car_va;run_original32();
            PcVehicleDisplayContext vc{fc,qc,PcEnvironmentRenderTables{Bytes(lights.data(),lights.size()),Bytes(fog.data(),fog.size()),pixel},
                envflags,layer,stack,Bytes(body.data(),body.size()),mode,scene};
            std::vector<std::uint32_t> skipped;
            vehicle_race_display_46bd30(vc,rst,PcRaceCarDisplayInputs{net,net2,pause},e,[&](std::uint32_t pc){skipped.push_back(pc);});
            shadow_leaves+=unsigned(skipped.size());
            {float g0,g1;std::memcpy(&g0,reinterpret_cast<void*>(0x7f9424u),4);std::memcpy(&g1,reinterpret_cast<void*>(0x7f9428u),4);++cases;
             if(std::memcmp(&g0,&rst.phase_7f9424,4)||std::memcmp(&g1,&rst.phase_7f9428,4))report("46BD30 phases car "+std::to_string(car));}
            ++displays;
            if(inject&&car==28&&run==3&&!native.calls.empty())native.calls.front().push_back(1);
            compare_calls("46BD30 car "+std::to_string(car)+" run "+std::to_string(run));
            if(std::memcmp(reinterpret_cast<const void*>(0x899b98u),lights.data(),lights.size()))report("46BD30 light table car "+std::to_string(car));
            if(std::memcmp(reinterpret_cast<const void*>(car_va),ev.data(),ev.size()))report("46BD30 car object car "+std::to_string(car));
            if(g32(0x864ef8)!=opaque.count||g32(0x8612d8)!=alpha.count||g32(0x864f14)!=opaque.undo.size()||g32(0x8999b0)!=qc.immediate_8999b0)
                report("46BD30 queue state car "+std::to_string(car));
            for(const auto& r:rg.ranges){
                if(r.base==0x740c98u)continue;
                for(std::uint32_t k=0;k<r.size;k+=4){
                    const auto a=r.base+k;auto gv=g32(a),nv=r.words[k/4];
                    if(a==0x89a4f8u||a==0x95aeecu||a==0x95aef0u)continue;
                    if(a==0x89a51cu||a==0x89a548u||a==0x95aee8u)gv=gid(gv);
                    ++cases;
                    if(gv!=nv){std::ostringstream o;o<<"46BD30 global 0x"<<std::hex<<a<<" guest "<<gv<<" native "<<nv<<" car "<<std::dec<<car;report(o.str());break;}
                }
            }
            // Leave the alpha queue empty for the next car.
            prepare(0x405840);run_original32();
            {std::vector<std::uint32_t> order(alpha.count);for(std::uint32_t k=0;k<alpha.count;++k)order[k]=k;
             if(alpha.count)render_queue_sort_4499e0(alpha,order,0,std::int32_t(alpha.count)-1);
             render_queue_flush_405890(fc,alpha,order);}
            compare_calls("46BD30 alpha car "+std::to_string(car));
        }
        resident.erase(resource);
        std::cerr<<"car "<<car<<" draws so far "<<draws<<" pixel shaders "<<pixel_shaders<<" calls "<<calls_compared<<char(10);
    }
    std::cerr<<"46BD30 shadow volume leaves reported "<<shadow_leaves<<char(10);
    std::cerr<<"render flush coverage: calls="<<calls_compared<<" draws="<<draws<<" pixel shaders="<<pixel_shaders<<" 4056D0="<<overrides<<" 46C140="<<displays<<" 405450="<<morphs<<char(10);
    std::cout<<"{\"routine\":\"render_flush_probe\",\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}"<<'\n';return 0;
}

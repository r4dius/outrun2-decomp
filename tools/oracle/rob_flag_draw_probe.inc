if(only=="rob_flag_draw_probe"){
    // Original flagman_cloth_draw 401B80 (448810 texture, SetMaterial, 41B5C0
    // save, the sampler / stage / render states, FVF 1D2, DrawIndexedPrimitiveUP,
    // 41B7A0 restore) on the fake device against rob_flag_draw_401b80 on a
    // logging native device with the same state tables. Per case: random
    // preset 78024C, texture resource (count, handle slots), device states,
    // bound stage textures, columns / rows. Compared: the call sequence with
    // arguments (the material words, the DIPUP pointers as PC addresses), the
    // particle bss 8A8CE4..955A30 and 89EDE0/89EDE8.
    using namespace outrun::platform;
    const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
    fake_d3d9::install();
    for(std::uint32_t page=0x62f000u;page<0x98d000u;page+=0x1000u)mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE);
    auto g32=[](std::uint32_t a){return *reinterpret_cast<const std::uint32_t*>(a);};
    auto p32=[](std::uint32_t a,std::uint32_t v){*reinterpret_cast<std::uint32_t*>(a)=v;};
    constexpr std::uint32_t Sys=0x3a900000u,TexObjs=fake_d3d9::Base+0x80000u;
    map_at(Sys,0x10000,PROT_READ|PROT_WRITE);
    for(unsigned k=0;k<32;++k){p32(TexObjs+k*0x40,fake_d3d9::TextureVt);}
    struct LogDevice final:PcD3D9Device{
        std::vector<std::vector<std::uint32_t>> calls;
        std::array<std::uint32_t,0x400> rs{};std::array<std::uint32_t,0x200> stage{},sampler{};std::array<std::uint32_t,256> tex{};
        static unsigned si(std::uint32_t s,std::uint32_t t){return ((s&0xfu)<<5)+(t&0x1fu);}
        std::uint32_t create_vertex_buffer(std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t)override{return 0;}
        std::uint32_t create_index_buffer(std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t)override{return 0;}
        std::uint8_t* lock(std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t)override{return nullptr;}
        void unlock(std::uint32_t)override{}
        std::uint32_t create_vertex_declaration(const PcVertexElement*)override{return 0;}
        std::uint32_t create_vertex_shader(const std::uint32_t*)override{return 0;}
        void set_vertex_shader_constant_f(std::uint32_t,const float*,std::uint32_t)override{calls.push_back({94});}
        std::uint32_t create_texture_from_file(const PcTextureFileRequest&)override{return 0;}
        void release(std::uint32_t)override{calls.push_back({0x4002});}
        std::uint32_t get_render_state(std::uint32_t s)override{calls.push_back({58,s});return rs[s&0x3ffu];}
        void set_render_state(std::uint32_t s,std::uint32_t v)override{calls.push_back({57,s,v});rs[s&0x3ffu]=v;}
        std::uint32_t get_texture(std::uint32_t s)override{calls.push_back({64,s});return tex[s&0xffu];}
        void set_texture(std::uint32_t s,std::uint32_t t)override{calls.push_back({65,s,t});tex[s&0xffu]=t;}
        std::uint32_t get_texture_stage_state(std::uint32_t s,std::uint32_t t)override{calls.push_back({66,s,t});return stage[si(s,t)];}
        void set_texture_stage_state(std::uint32_t s,std::uint32_t t,std::uint32_t v)override{calls.push_back({67,s,t,v});stage[si(s,t)]=v;}
        std::uint32_t get_sampler_state(std::uint32_t s,std::uint32_t t)override{calls.push_back({68,s,t});return sampler[si(s,t)];}
        void set_sampler_state(std::uint32_t s,std::uint32_t t,std::uint32_t v)override{calls.push_back({69,s,t,v});sampler[si(s,t)]=v;}
        std::uint32_t draw_indexed_primitive(std::uint32_t,std::int32_t,std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t)override{calls.push_back({82});return 0;}
        void set_vertex_declaration(std::uint32_t)override{calls.push_back({87});}
        void set_vertex_shader(std::uint32_t s)override{calls.push_back({92,s});}
        void set_stream_source(std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t)override{calls.push_back({100});}
        void set_indices(std::uint32_t)override{calls.push_back({104});}
        std::uint32_t create_pixel_shader(const std::uint32_t*)override{return 0;}
        void set_pixel_shader(std::uint32_t s)override{calls.push_back({107,s});}
        void set_pixel_shader_constant_f(std::uint32_t,const float*,std::uint32_t)override{calls.push_back({109});}
        std::uint32_t get_declaration(std::uint32_t,PcVertexElement*)override{return 0;}
        void set_fvf(std::uint32_t f)override{calls.push_back({89,f});}
        void set_material(const float m[17])override{std::vector<std::uint32_t> c{49};for(unsigned k=0;k<17;++k){std::uint32_t u;std::memcpy(&u,m+k,4);c.push_back(u);}calls.push_back(c);}
        void draw_indexed_primitive_up(std::uint32_t type,std::uint32_t mv,std::uint32_t nv,std::uint32_t count,const void* idx,
            std::uint32_t fmt,const void* data,std::uint32_t stride)override{
            calls.push_back({84,type,mv,nv,count,std::uint32_t(reinterpret_cast<std::uintptr_t>(idx)),fmt,std::uint32_t(reinterpret_cast<std::uintptr_t>(data)),stride});}
    };
    std::mt19937 rng(0x401b80u);
    const unsigned cases=std::max(count,1u)*8u;
    unsigned mismatches=0,draws=0;bool injected=false;
    auto report=[&](const std::string& w){++mismatches;if(mismatches<=12)std::cerr<<"MISMATCH "<<w<<char(10);};
    constexpr std::uint32_t BssBase=PcParticleState::BssBase,BssEnd=PcParticleState::BssEnd;
    for(unsigned cs=0;cs<cases;++cs){
        const std::string tag=" case "+std::to_string(cs);
        const std::uint32_t preset=rng()%5u;p32(0x78024cu,preset);
        const std::uint32_t resource=rob_flag_texture_resource(preset);
        // Texture resource: system at Sys (+8 count), +24 -> table header whose word is the handle slots.
        std::memset(reinterpret_cast<void*>(Sys),0,0x1000);
        const std::uint32_t n_tex=rng()%4u;p32(Sys+8,n_tex);
        const std::uint32_t slots_off=0x100u+(rng()%16u)*4u;
        for(unsigned k=0;k<4;++k)p32(Sys+slots_off+k*4,rng()%3u?TexObjs+(rng()%32u)*0x40u:0u);
        const std::uint32_t header=Sys+0x800u;p32(header,Sys+slots_off);
        const std::uint32_t entry=0x7c2800u+resource*0x48u;std::memset(reinterpret_cast<void*>(entry),0,0x48);
        p32(entry,Sys);p32(entry+0x24,header);
        PcPmtResources r{};r.system.assign(reinterpret_cast<const std::uint8_t*>(Sys),reinterpret_cast<const std::uint8_t*>(Sys)+0x1000);
        r.texture_table_24=0x800u;
        // Native handle slots are system offsets: rewrite the header word.
        {std::uint32_t o=slots_off;std::memcpy(r.system.data()+0x800,&o,4);}
        // Device states: random tables for the entries 401B80 / 41B5C0 touch.
        LogDevice dev;
        auto both_rs=[&](std::uint32_t s,std::uint32_t v){p32(fake_d3d9::RenderStates+(s&0x3ffu)*4,v);dev.rs[s&0x3ffu]=v;};
        for(const auto& s:ParticleSavedStates41b5c0)both_rs(s[0],rng()%3u);
        for(std::uint32_t s:{0x1bu,0xabu,0x13u,0x14u,0x07u,0x0eu,0x0fu,0x16u,0x89u,0x1cu,0x1du})both_rs(s,rng()%4u);
        for(std::uint32_t t:{1u,2u,5u,6u,7u}){const std::uint32_t v=rng()%4u;p32(fake_d3d9::SamplerStates+LogDevice::si(0,t)*4,v);dev.sampler[LogDevice::si(0,t)]=v;}
        for(std::uint32_t t:{0xbu,0x18u}){const std::uint32_t v=rng()%2u;p32(fake_d3d9::StageStates+LogDevice::si(0,t)*4,v);dev.stage[LogDevice::si(0,t)]=v;}
        for(unsigned k=0;k<4;++k){const std::uint32_t t=rng()%2u?TexObjs+(rng()%32u)*0x40u:0u;p32(fake_d3d9::Textures+k*4,t);dev.tex[k]=t;}
        const std::uint32_t colour_saved=g32(0x737728u);
        // Mesh: cols x rows strip from 401830, random vertices.
        const std::uint32_t cols=2u+rng()%4u,rows=2u+rng()%6u;
        std::vector<std::uint8_t> bss(BssEnd-BssBase);
        for(auto& b:bss)b=std::uint8_t(rng());
        std::memcpy(reinterpret_cast<void*>(BssBase),bss.data(),bss.size());
        PcRaceMemory mesh;
        mesh.map(PcRobFlagState::MeshBase,reinterpret_cast<std::uint8_t*>(PcRobFlagState::MeshBase),PcRobFlagState::MeshEnd-PcRobFlagState::MeshBase);
        rob_flag_mesh_401830(mesh,PcRobFlagState::Indices,PcRobFlagState::Vertices,cols,rows);
        for(std::uint32_t a=PcRobFlagState::Vertices;a<PcRobFlagState::MeshEnd;a+=4){const float f=float(std::int32_t(rng()%2001u)-1000)/100.f;std::memcpy(reinterpret_cast<void*>(a),&f,4);}
        p32(0x89ede0u,0x11u);p32(0x89ede8u,0x22u);
        // Guest.
        fake_d3d9::reset_log();
        prepare(0x401b80u);{auto* st=reinterpret_cast<unsigned*>(S);st[0]=PcRobFlagState::Vertices;st[1]=PcRobFlagState::Indices;st[2]=cols;st[3]=rows;}run_original32();
        std::vector<std::uint8_t> guest_bss(bss.size());std::memcpy(guest_bss.data(),reinterpret_cast<const void*>(BssBase),bss.size());
        const std::uint32_t ge0=g32(0x89ede0u),ge8=g32(0x89ede8u);
        std::vector<std::vector<std::uint32_t>> g;std::uint32_t data=0;const auto* df=fake_d3d9::data_log();
        for(std::uint32_t k=0;k<fake_d3d9::log_count();++k){
            const auto* rec=fake_d3d9::log_record(k);const auto id=rec[0];const auto* a=rec+2;
            switch(id){
            case 57:case 67:case 69:g.push_back(id==57?std::vector<std::uint32_t>{57,a[0],a[1]}:std::vector<std::uint32_t>{id,a[0],a[1],a[2]});break;
            case 58:case 64:g.push_back({id,a[0]});break;
            case 66:case 68:g.push_back({id,a[0],a[1]});break;
            case 65:g.push_back({65,a[0],a[1]});break;
            case 89:case 92:case 107:g.push_back({id,a[0]});break;
            case 49:{std::vector<std::uint32_t> c{49};for(unsigned w=0;w<17;++w){std::uint32_t u;std::memcpy(&u,df+data+w,4);c.push_back(u);}data+=17;g.push_back(c);break;}
            case 84:g.push_back({84,a[0],a[1],a[2],a[3],a[4],a[5],a[6],a[7]});break;
            case 0x4002:g.push_back({0x4002});break;
            default:g.push_back({0xdead0000u|id});break;
            }
        }
        // Native (fresh bss copy, same starting bytes).
        std::memcpy(reinterpret_cast<void*>(BssBase),bss.data(),bss.size());
        std::vector<std::uint8_t> nbss=bss;PcRaceMemory save;save.map(BssBase,nbss.data(),nbss.size());
        std::uint32_t ne0=0x11u,ne8=0x22u;
        PcRobFlagDraw draw{mesh,save,dev,rob_flag_texture_401b80(r),[&](std::uint32_t a,std::uint32_t v){if(a==0x89ede0u)ne0=v;else if(a==0x89ede8u)ne8=v;}};
        try{rob_flag_draw_401b80(draw,PcRobFlagState::Vertices,PcRobFlagState::Indices,cols,rows);}
        catch(const std::exception& e){report(std::string("native threw: ")+e.what()+tag);continue;}
        if(inject&&!injected&&!dev.calls.empty()){dev.calls[0].push_back(1);injected=true;}
        auto& n=dev.calls;
        for(const auto& c:n)if(c[0]==84)++draws;
        if(g.size()!=n.size())report("call count guest="+std::to_string(g.size())+" native="+std::to_string(n.size())+tag);
        for(std::size_t k=0;k<std::min(g.size(),n.size());++k)if(g[k]!=n[k]){
            std::ostringstream o;o<<"call "<<k<<" guest";for(auto v:g[k])o<<" "<<std::hex<<v;o<<" | native";for(auto v:n[k])o<<" "<<std::hex<<v;report(o.str()+tag);break;}
        if(guest_bss!=nbss){for(std::size_t k=0;k<nbss.size();++k)if(guest_bss[k]!=nbss[k]){std::ostringstream o;o<<"bss "<<std::hex<<(BssBase+k);report(o.str()+tag);break;}}
        if(ge0!=ne0||ge8!=ne8)report("89EDE0/89EDE8"+tag);
        if(g32(0x737728u)!=colour_saved)report("737728 written"+tag);
    }
    std::cerr<<"rob flag draw: draws="<<draws<<char(10);
    std::cout<<"{\"routine\":\"rob_flag_draw_probe\",\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}"<<'\n';return 0;
}

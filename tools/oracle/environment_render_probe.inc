if(only=="environment_render_probe"){
    // Original 40D840 (40D870 fog, 40DD70 light colours, 40DB80 lights with
    // 410740/4107A0) against render_environment_40d840, on the logging fake
    // IDirect3DDevice9. Light tables 899B98..89A4F8, fog records 7D3A10,
    // flags 79FCCA / 740C88, the environment index and the initial render
    // states are randomised. Every device call (SetLight compared by the
    // D3DLIGHT9 contents copied at the call) and every renderer global word is compared.
    using namespace outrun::platform;
    const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
    constexpr unsigned base=0x3a800000;
    map_at(base,0x10000,PROT_READ|PROT_WRITE|PROT_EXEC);
    map_original_arithmetic_bridge(file);
    for(std::uint32_t page=0x62f000u;page<0x98d000u;page+=0x1000u)mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE);
    fake_d3d9::install();
    auto g32=[](std::uint32_t a){return *reinterpret_cast<const std::uint32_t*>(a);};
    auto p32=[](std::uint32_t a,std::uint32_t v){*reinterpret_cast<std::uint32_t*>(a)=v;};
    struct NativeDevice final:PcD3D9Device{
        std::vector<std::vector<std::uint32_t>> calls;std::array<std::uint32_t,0x400> render{};
        [[noreturn]] static void unused(){throw std::runtime_error("unexpected device call in 40D840");}
        std::uint32_t create_vertex_buffer(std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t)override{unused();}
        std::uint32_t create_index_buffer(std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t)override{unused();}
        std::uint8_t* lock(std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t)override{unused();}
        void unlock(std::uint32_t)override{unused();}
        std::uint32_t create_vertex_declaration(const PcVertexElement*)override{unused();}
        std::uint32_t create_vertex_shader(const std::uint32_t*)override{unused();}
        void set_vertex_shader_constant_f(std::uint32_t start,const float* d,std::uint32_t n)override{
            std::vector<std::uint32_t> c{94,start,n};for(std::uint32_t k=0;k<n*4;++k){std::uint32_t u;std::memcpy(&u,d+k,4);c.push_back(u);}calls.push_back(c);}
        std::uint32_t create_texture_from_file(const PcTextureFileRequest&)override{unused();}
        void release(std::uint32_t)override{unused();}
        std::uint32_t get_render_state(std::uint32_t s)override{calls.push_back({58,s});return render[s&0x3ffu];}
        void set_render_state(std::uint32_t s,std::uint32_t v)override{calls.push_back({57,s,v});render[s&0x3ffu]=v;}
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
    } native;
    PcRenderGlobals rg;
    for(auto& r:rg.ranges)if(r.base!=0x740c98u)for(std::uint32_t k=0;k<r.size;k+=4)r.words[k/4]=g32(r.base+k);
    PcFlushContext fc{native,rg,[](std::uint32_t)->PcPmtResources*{return nullptr;},pc_colour_list_alt_408c80,1};
    auto guest_calls=[&]{
        std::vector<std::vector<std::uint32_t>> out;std::uint32_t data=0;const auto* df=fake_d3d9::data_log();
        const auto n=fake_d3d9::log_count();
        for(std::uint32_t k=0;k<n;++k){
            const auto* r=fake_d3d9::log_record(k);const auto id=r[0];const auto* a=r+2;
            switch(id){
            case 57:out.push_back({57,a[0],a[1]});break;
            case 58:out.push_back({58,a[0]});break;
            case 53:out.push_back({53,a[0],a[1]});break;
            case 51:{std::vector<std::uint32_t> c{51,a[0]};for(unsigned w=0;w<26;++w){std::uint32_t u;std::memcpy(&u,df+data+w,4);c.push_back(u);}data+=26;out.push_back(c);break;}
            case 94:{std::vector<std::uint32_t> c{94,a[0],a[2]};
                for(std::uint32_t q=0;q<a[2]*4u;++q){std::uint32_t u;std::memcpy(&u,df+data+q,4);c.push_back(u);}data+=a[2]*4u;
                // 4109C0 cone constants 0x30/0x31: w is an uninitialised stack slot on the PC.
                if(a[0]==0x30u||a[0]==0x31u)c[6]=0;
                out.push_back(c);break;}
            default:out.push_back({0xdead0000u|id});break;
            }
        }
        return out;
    };
    std::mt19937 rng(0x40d840u);
    auto real=[&](float lo,float hi){return lo+(hi-lo)*(float(rng()&0xffffu)/65535.f);};
    auto fbits=[](float f){std::uint32_t u;std::memcpy(&u,&f,4);return u;};
    const unsigned cases=std::max(count,1u)*4000u;
    unsigned mismatches=0,calls_compared=0,lights_set=0,fog_cases=0;
    auto report=[&](const std::string& what){++mismatches;if(mismatches<=12)std::cerr<<"MISMATCH "<<what<<char(10);};
    std::vector<std::uint8_t> lights(0x960),fog(0x54);
    for(unsigned c=0;c<cases;++c){
        for(unsigned r=0;r<15;++r){
            Bytes l(lights.data()+r*0xa0,0xa0);
            for(unsigned k=0;k<0xa0;k+=4)l.put32(k,fbits(real(-1.f,1.f)));
            static const std::uint32_t en[]{0,1,1,1,2,0xffffffffu};l.put32(0,en[rng()%6u]);
            static const std::uint32_t ty[]{1,2,3,3,1,2,0,4};l.put32(4,ty[rng()%8u]);
            for(unsigned k=0x08;k<0x38;k+=4)l.put32(k,fbits(real(0.f,1.5f)));
            l.putf(0x50,real(1.f,500.f));l.putf(0x54,real(0.f,2.f));
            l.putf(0x58,real(0.f,1.f));l.putf(0x5c,real(0.f,0.1f));l.putf(0x60,real(0.f,0.01f));
            l.putf(0x64,real(0.f,3.1f));l.putf(0x68,real(0.f,3.1f));
            for(unsigned k=0x6c;k<0x84;k+=4)l.putf(k,real(0.f,1.f));
            l.putf(0x90,(rng()%8u)?real(0.5f,2.5f):std::nanf(""));
        }
        for(unsigned r=0;r<3;++r){
            Bytes f(fog.data()+r*0x1c,0x1c);
            for(unsigned k=0;k<0x1c;k+=4)f.put32(k,rng());
            f.put32(0,(rng()%5u)?1u:0u);f.put32(4,rng()%5u);
            f.putf(8,real(-10.f,500.f));f.putf(0xc,real(10.f,3000.f));
            switch(rng()%5u){case 0:f.putf(0x10,0.f);break;case 1:f.putf(0x10,-real(0.f,1.f));break;case 2:f.putf(0x10,real(0.00001f,0.001f));break;default:f.putf(0x10,real(0.0005f,0.05f));}
        }
        std::memcpy(reinterpret_cast<void*>(0x899b98u),lights.data(),lights.size());
        std::memcpy(reinterpret_cast<void*>(0x7d3a10u),fog.data(),fog.size());
        const std::uint8_t pixel=(rng()%3u)==0u?1u:0u,flags=std::uint8_t((rng()%4u)?(0x2u|(rng()&0xfcu)):rng());
        *reinterpret_cast<std::uint8_t*>(0x740c88u)=pixel;*reinterpret_cast<std::uint8_t*>(0x79fccau)=flags;
        static const std::int32_t envs[]{0,1,2,2,-1};const std::int32_t env=envs[rng()%5u];
        for(std::uint32_t s:{0x1cu,0x22u,0x23u,0x24u,0x25u,0x26u}){
            const std::uint32_t v=(rng()%3u)?rng()%4u:rng();
            p32(fake_d3d9::RenderStates+s*4,v);native.render[s]=v;}
        fake_d3d9::reset_log();native.calls.clear();
        prepare(0x40d840);*reinterpret_cast<unsigned*>(S)=std::uint32_t(env);run_original32();
        PcEnvironmentRenderTables t{Bytes(lights.data(),lights.size()),Bytes(fog.data(),fog.size()),pixel};
        render_environment_40d840(fc,env,flags,t);
        if(inject&&c==11u)rg.w(0x95dc20)^=1u;
        auto gl=guest_calls();const auto& nl=native.calls;
        calls_compared+=unsigned(gl.size());for(const auto& x:nl){if(x[0]==51)++lights_set;if(x[0]==57&&x[1]==0x23)++fog_cases;}
        const std::string tag=" case "+std::to_string(c)+" env "+std::to_string(env);
        if(gl.size()!=nl.size())report("call count guest "+std::to_string(gl.size())+" native "+std::to_string(nl.size())+tag);
        for(std::size_t k=0;k<std::min(gl.size(),nl.size());++k)if(gl[k]!=nl[k]){
            std::ostringstream o;o<<"call "<<k<<" guest";for(auto v:gl[k])o<<" "<<std::hex<<v;o<<" | native";for(auto v:nl[k])o<<" "<<std::hex<<v;
            report(o.str()+tag);break;}
        bool globals_ok=true;
        for(auto& r:rg.ranges){if(r.base==0x740c98u)continue;
            for(std::uint32_t k=0;k<r.size&&globals_ok;k+=4)if(g32(r.base+k)!=r.words[k/4]){
                std::ostringstream o;o<<"global 0x"<<std::hex<<r.base+k<<" guest "<<g32(r.base+k)<<" native "<<r.words[k/4]<<tag;report(o.str());globals_ok=false;}}
        if(!globals_ok)for(auto& r:rg.ranges)if(r.base!=0x740c98u)for(std::uint32_t k=0;k<r.size;k+=4)r.words[k/4]=g32(r.base+k);
    }
    std::cerr<<"environment render: device calls="<<calls_compared<<" SetLight="<<lights_set<<" fog table modes set="<<fog_cases<<char(10);
    std::cout<<"{\"routine\":\"environment_render_probe\",\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}"<<'\n';return 0;
}

if(only=="pmt_loader_probe"){
    // Runs the original PMT processing steps (42E490, 42E4F0*, 42E850, 42E8C0*)
    // for the 30 retail car files against the logging fake IDirect3DDevice9,
    // runs the native port (platform/pc_pmt_loader) against a logging native
    // device, and compares: the step counts, the device call sequence (with
    // declaration elements, shader tokens, buffer contents and texture
    // loader arguments), the loaded system section with every pointer
    // normalised (system offsets, device objects by creation ordinal), the
    // bank/cursor globals and the shader cache 95C058.
    using namespace outrun::platform;
    if(argc<=7||std::string(argv[7])=="-")throw std::runtime_error("PMT loader oracle requires pinned d3dx9_29.dll");
    const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
    const char* cars=std::getenv("OR2_CARS_DIR");
    if(!cars)throw std::runtime_error("set OR2_CARS_DIR to the retail Cars directory");
    constexpr unsigned base=0x3a800000,window=0x5c000000;
    map_at(base,0x10000,PROT_READ|PROT_WRITE|PROT_EXEC);map_at(window,0x2000000,PROT_READ|PROT_WRITE);
    map_original_arithmetic_bridge(file);
    init_matrix_oracle_globals();wire_d3dx_imports(map_d3dx29(argv[7]));
    {   auto* iat=reinterpret_cast<void*>(0x596000u);
        if(mprotect(iat,4096,PROT_READ|PROT_WRITE))throw std::runtime_error("loader IAT");
        *reinterpret_cast<std::uint32_t*>(0x5962a0u)=D3DX_BASE+(0x4524b8u-0x400000u); // D3DXDeclaratorFromFVF
        if(mprotect(iat,4096,PROT_READ))throw std::runtime_error("loader IAT restore");}
    fake_d3d9::install();
    if(fake_d3d9::remap_fs0(0x40f4e0u,0x40f700u,base+0xfff0u)!=3)throw std::runtime_error("40F4E0 fs:[0] patch count");
    for(std::uint32_t page=0x7c2000u;page<0x7cd000u;page+=0x1000u)
        if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("bank pages");
    for(unsigned page:{0x957000u,0x95b000u,0x95c000u,0x95d000u,0x95e000u,0x98a000u,0x98b000u,0x89e000u,0x742000u})
        if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("loader globals");
    auto g32=[](std::uint32_t a){return *reinterpret_cast<const std::uint32_t*>(a);};
    // Native logging device: handles are global creation ordinals + 1.
    struct NativeDevice final:PcD3D9Device{
        struct Object{std::uint32_t kind{};std::vector<std::uint8_t> bytes;std::vector<std::uint32_t> words;};
        std::vector<Object> objects;std::vector<std::vector<std::uint32_t>> calls;const std::uint8_t* video{};
        std::uint32_t add(Object o){objects.push_back(std::move(o));return std::uint32_t(objects.size());}
        std::uint32_t create_vertex_buffer(std::uint32_t n,std::uint32_t u,std::uint32_t f,std::uint32_t p)override{
            calls.push_back({26,n,u,f,p});return add({26,std::vector<std::uint8_t>(n),{}});}
        std::uint32_t create_index_buffer(std::uint32_t n,std::uint32_t u,std::uint32_t f,std::uint32_t p)override{
            calls.push_back({27,n,u,f,p});return add({27,std::vector<std::uint8_t>(n),{}});}
        std::uint8_t* lock(std::uint32_t h,std::uint32_t off,std::uint32_t size,std::uint32_t flags)override{
            calls.push_back({0x100b,off,size,flags});auto& b=objects.at(h-1).bytes;
            if(off>b.size()||size>b.size()-off)return nullptr;return b.data()+off;}
        void unlock(std::uint32_t)override{calls.push_back({0x100c});}
        std::uint32_t create_vertex_declaration(const PcVertexElement* e)override{
            std::vector<std::uint32_t> w;
            for(;;++e){w.push_back(e->stream|(std::uint32_t(e->offset)<<16));
                w.push_back(e->type|(std::uint32_t(e->method)<<8)|(std::uint32_t(e->usage)<<16)|(std::uint32_t(e->usage_index)<<24));
                if(e->stream==PcDeclEndStream)break;}
            std::vector<std::uint32_t> c{86};c.insert(c.end(),w.begin(),w.end());calls.push_back(c);return add({86,{},w});}
        std::uint32_t create_vertex_shader(const std::uint32_t* t)override{
            std::vector<std::uint32_t> w;for(;;++t){w.push_back(*t);if(*t==0xffffu)break;}
            std::vector<std::uint32_t> c{91};c.insert(c.end(),w.begin(),w.end());calls.push_back(c);return add({91,{},w});}
        void set_vertex_shader_constant_f(std::uint32_t start,const float* d,std::uint32_t n)override{
            std::vector<std::uint32_t> c{94,start,n};for(std::uint32_t k=0;k<n*4;++k){std::uint32_t u;std::memcpy(&u,d+k,4);c.push_back(u);}calls.push_back(c);}
        std::uint32_t create_texture_from_file(const PcTextureFileRequest& q)override{
            const std::uint32_t off=std::uint32_t(q.data-video);
            if(q.cube)calls.push_back({0x5002,off,q.size,q.width,q.mip_levels,q.usage,q.format,q.pool,q.filter,q.mip_filter,q.colour_key});
            else calls.push_back({0x5001,off,q.size,q.width,q.height,q.mip_levels,q.usage,q.format,q.pool,q.filter,q.mip_filter});
            return add({q.cube?0x5002u:0x5001u,{},{}});}
        void release(std::uint32_t)override{calls.push_back({2});}
        [[noreturn]] static void unused(){throw std::logic_error("pipeline call during loading");}
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
        void set_vertex_shader(std::uint32_t h)override{calls.push_back({92,h});}   // 4103F0
        void set_stream_source(std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t)override{unused();}
        void set_indices(std::uint32_t)override{unused();}
        std::uint32_t create_pixel_shader(const std::uint32_t*)override{unused();}
        void set_pixel_shader(std::uint32_t)override{unused();}
        void set_pixel_shader_constant_f(std::uint32_t,const float*,std::uint32_t)override{unused();}
        std::uint32_t get_declaration(std::uint32_t,PcVertexElement*)override{unused();}
    } native;
    PcShaderCache cache{};
    PcShaderGlobals globals{};globals.mode_89edbc=g32(0x89edbc);globals.override_89edd4=g32(0x89edd4);
    globals.generated_key_1039ec0=g32(0x1039ec0);
    std::map<std::uint32_t,std::uint32_t> guest_ordinal;std::uint32_t guest_objects=0,heap_mark=fake_d3d9::heap_top();
    auto guest_id=[&](std::uint32_t p)->std::uint32_t{if(!p)return 0;auto it=guest_ordinal.find(p);
        if(it==guest_ordinal.end())throw std::runtime_error("guest pointer is not a fake device object");return it->second;};
    // Loaded system section with pointers normalised.
    auto normalise=[&](const std::uint8_t* sys,std::size_t size,std::uint32_t reloc,std::uint32_t header,
                       const std::function<std::uint32_t(std::uint32_t)>& id){
        std::vector<std::uint8_t> out(sys,sys+size);Bytes s(out.data(),out.size());
        s.put32(0x14,s.u32(0x14)-reloc);
        const auto count=s.u32(4);
        for(std::uint32_t i=0;i<count;++i){
            const auto rec=0x18u+i*0x3cu;
            for(std::uint32_t f=4;f<=0x38;f+=4)s.put32(rec+f,s.u32(rec+f)-reloc);
            const auto info=s.u32(rec+0x18);const auto groups=s.i32(info+0x24),mats=s.i32(info+0x2c);
            for(std::int32_t g=0;g<groups;++g){
                const auto group=s.u32(rec+0x30)+std::uint32_t(g)*0x2cu;
                for(std::int32_t k=0;k<s.i32(group);++k){const auto slot=s.u32(rec+8)+std::uint32_t(g)*16u+std::uint32_t(k)*4u;s.put32(slot,id(s.u32(slot)));}
                const auto ib=s.u32(rec+4)+std::uint32_t(g)*4u;s.put32(ib,id(s.u32(ib)));
                for(std::int32_t k=0;k<mats;++k){const auto m=s.u32(rec+0xc)+std::uint32_t(mats*g+k)*0x18u;
                    s.put32(m,id(s.u32(m)));s.put32(m+4,id(s.u32(m+4)));}
            }
        }
        s.put32(header,s.u32(header)-reloc);
        for(std::uint32_t t=0;t<s.u32(8);++t)s.put32(header+4+t*4,id(s.u32(header+4+t*4)));
        return out;
    };
    unsigned cases=0,mismatches=0;std::size_t native_call_mark=0;std::uint32_t native_object_mark=0;
    auto report=[&](unsigned car,const std::string& what){++mismatches;if(mismatches<=20)std::cerr<<"car "<<car<<" MISMATCH "<<what<<"\n";};
    {   // D3DXDeclaratorFromFVF sweep against the pinned d3dx9_29.dll.
        unsigned fvf_cases=0,fvf_bad=0;
        const std::uint32_t out=base+0x1000u;
        for(std::uint32_t pos:{0x0u,0x2u,0x4u,0x6u,0x8u,0xau,0xcu,0xeu,0x4002u})
        for(std::uint32_t bits=0;bits<16;++bits)
        for(std::uint32_t tex=0;tex<=8;++tex)
        for(std::uint32_t beta:{0x0u,0x1000u,0x8000u})
        for(std::uint32_t fmt:{0x0u,0x55555555u,0xaaaaaaaau,0xffffffffu,0x1b1b1b1bu}){
            const std::uint32_t mask=tex?(((1u<<(2u*tex))-1u)<<16):0u;
            const std::uint32_t fvf=pos|(bits<<4)|(tex<<8)|beta|((fmt<<16)&mask);
            std::memset(reinterpret_cast<void*>(out),0xcc,PcMaxFvfDeclSize*8u);
            prepare(D3DX_BASE+(0x4524b8u-0x400000u));
            reinterpret_cast<std::uint32_t*>(S)[0]=fvf;reinterpret_cast<std::uint32_t*>(S)[1]=out;run_original32();
            const bool gok=guest_call.out_eax==0u;
            PcVertexDeclaration nd{};const bool nok=d3dx_declarator_from_fvf(fvf,nd);
            ++fvf_cases;bool same=gok==nok;
            if(same&&gok)for(unsigned k=0;k<PcMaxFvfDeclSize;++k){
                const auto w0=g32(out+k*8u),w1=g32(out+k*8u+4u);const auto& e=nd[k];
                if(w0!=(e.stream|(std::uint32_t(e.offset)<<16))||
                   w1!=(e.type|(std::uint32_t(e.method)<<8)|(std::uint32_t(e.usage)<<16)|(std::uint32_t(e.usage_index)<<24))){same=false;break;}
                if(e.stream==PcDeclEndStream)break;
            }
            if(!same){++fvf_bad;if(fvf_bad<=8)std::cerr<<"FVF MISMATCH 0x"<<std::hex<<fvf<<std::dec<<" guest_ok="<<gok<<" native_ok="<<nok<<char(10);}
        }
        std::cerr<<"D3DXDeclaratorFromFVF sweep: "<<fvf_cases<<" FVFs, "<<fvf_bad<<" differences"<<char(10);
        cases+=fvf_cases;mismatches+=fvf_bad;
    }
    {   // 40E140 shader linker and 40DE10 configuration word sweeps.
        unsigned link_cases=0,link_bad=0,config_cases=0,config_bad=0;
        std::uint32_t rng=0x4f523035u;auto next=[&]{rng=rng*1664525u+1013904223u;return rng;};
        const std::uint32_t word=base+0x2000u,output=base+0x4000u;
        auto link_check=[&](std::uint32_t config){
            *reinterpret_cast<std::uint32_t*>(word)=config;
            prepare(0x40e140u);guest_call.eax=word;reinterpret_cast<std::uint32_t*>(S)[0]=output;run_original32();
            const auto native_tokens=link_vertex_shader_40e140(config);
            ++link_cases;bool same=true;
            for(std::size_t k=0;k<native_tokens.size();++k)if(g32(output+std::uint32_t(k)*4u)!=native_tokens[k]){same=false;break;}
            if(!same){++link_bad;if(link_bad<=8)std::cerr<<"40E140 MISMATCH config 0x"<<std::hex<<config<<std::dec<<char(10);}
        };
        for(std::uint32_t low=0;low<(1u<<9);++low)if((low&7u)<=4u&&((low>>5)&0xfu)<=7u)link_check(low);
        for(unsigned k=0;k<60000u;++k){
            std::uint32_t c=(next()%5u)|((next()%4u)<<3)|((next()%8u)<<5);
            for(unsigned layer=0;layer<3;++layer){const auto r=next();
                if(r%3u)c|=((next()%13u)|((next()%4u)<<4))<<(9u+6u*layer);}
            link_check(c);
        }
        const std::uint32_t group=base+0x2100u,material=base+0x2200u,elements=base+0x3000u,out=base+0x2400u;
        static constexpr std::uint32_t fvfs[]={0x112,0x152,0x212,0x252,0x102,0x142,0x312,0x412,0x116,0x118,0x11a,0x156,0x15a,0x2,0x42,0x0};
        for(unsigned k=0;k<60000u;++k){
            Bytes gg(reinterpret_cast<void*>(group),0x2c),mm(reinterpret_cast<void*>(material),0x58);
            for(unsigned o=0;o<0x2c;o+=4)gg.put32(o,next());
            for(unsigned o=0;o<0x58;o+=4)mm.put32(o,next());
            gg.put32(0x28,next()%5u);gg.put32(0x20,fvfs[next()%16u]);
            if(next()%2u)mm.put32(4,next()&0x180000ffu);
            for(unsigned layer=0;layer<4;++layer){const auto o=8u+layer*0x14u;
                std::uint32_t w=(next()%6u)<<28;const auto sel=next()%6u;
                if(sel==1)w|=0x80u;else if(sel==2)w|=0x2u;else if(sel==3)w|=0x8u;else if(sel==4)w|=0x4u;else if(sel==5)w|=next()&0x3ffu;
                if(next()%2u)w|=1u;mm.put32(o,w);if(layer<3)mm.puti(o+0x10,std::int32_t(next()%3u)-1);}
            PcVertexDeclaration decl{};const bool with=next()%8u!=0u;
            if(with&&!d3dx_declarator_from_fvf(fvfs[next()%16u],decl))throw std::runtime_error("sweep FVF");
            if(with)for(unsigned e=0;e<PcMaxFvfDeclSize;++e){const auto& d=decl[e];
                *reinterpret_cast<std::uint32_t*>(elements+e*8u)=d.stream|(std::uint32_t(d.offset)<<16);
                *reinterpret_cast<std::uint32_t*>(elements+e*8u+4u)=d.type|(std::uint32_t(d.method)<<8)|(std::uint32_t(d.usage)<<16)|(std::uint32_t(d.usage_index)<<24);
                if(d.stream==PcDeclEndStream)break;}
            *reinterpret_cast<std::uint32_t*>(out)=0xdeadbeefu;
            prepare(0x40de10u);guest_call.eax=group;guest_call.ecx=material;guest_call.ebx=out;
            reinterpret_cast<std::uint32_t*>(S)[0]=with?elements:0u;run_original32();
            const auto native_config=material_config_40de10(gg,mm,with?decl.data():nullptr);
            ++config_cases;
            if(g32(out)!=native_config){++config_bad;if(config_bad<=8)std::cerr<<"40DE10 MISMATCH guest 0x"<<std::hex<<g32(out)<<" native 0x"<<native_config<<std::dec<<char(10);}
        }
        std::cerr<<"40E140 link sweep: "<<link_cases<<" configs, "<<link_bad<<" differences; 40DE10 sweep: "<<config_cases<<" cases, "<<config_bad<<" differences"<<char(10);
        cases+=link_cases+config_cases;mismatches+=link_bad+config_bad;
    }
    for(unsigned car=0;car<30u;++car){
        const unsigned resource=VehicleResourceIds[car];
        std::string path=std::string(cars)+"/"+(VehicleResourcePaths[car]+5);
        std::ifstream in(path,std::ios::binary);if(!in)throw std::runtime_error("missing "+path);
        std::vector<std::uint8_t> packed((std::istreambuf_iterator<char>(in)),std::istreambuf_iterator<char>());
        std::vector<std::uint8_t> pmt(64u*1024u*1024u);uLongf size=uLongf(pmt.size());
        if(uncompress(pmt.data(),&size,packed.data(),uLong(packed.size()))!=Z_OK)throw std::runtime_error("inflate "+path);
        pmt.resize(size);Bytes pb(pmt.data(),pmt.size());
        const auto ss=pb.u32(8),vs=pb.u32(12);
        if(ss+vs>0x1f00000u)throw std::runtime_error("PMT larger than the guest window");
        const std::uint32_t system=window,video=window+((ss+0xfffu)&~0xfffu);
        std::memcpy(reinterpret_cast<void*>(system),pmt.data()+16,ss);
        std::memcpy(reinterpret_cast<void*>(video),pmt.data()+16+ss,vs);
        const std::uint32_t bank=0x7c2800u+resource*0x48u;std::memset(reinterpret_cast<void*>(bank),0,0x48);
        *reinterpret_cast<std::uint32_t*>(base+0x200)=system;*reinterpret_cast<std::uint32_t*>(base+0x204)=7;
        *reinterpret_cast<std::uint32_t*>(bank+0x28)=base+0x200;*reinterpret_cast<std::uint32_t*>(bank+0x08)=video;
        *reinterpret_cast<std::uint32_t*>(bank+0x30)=7;
        fake_d3d9::reset_log();
        prepare(0x42e490);guest_call.eax=bank;run_original32();
        unsigned steps=0;
        do{prepare(0x42e4f0);*reinterpret_cast<unsigned*>(S)=bank;run_original32();++steps;}while((guest_call.out_eax&0xffu)==0u&&steps<4096u);
        prepare(0x42e850);guest_call.eax=bank;run_original32();
        unsigned tex_steps=0;
        do{prepare(0x42e8c0);guest_call.edi=bank;run_original32();++tex_steps;}while((guest_call.out_eax&0xffu)==0u&&tex_steps<4096u);
        // Native run on a private copy.
        PcPmtResources r{};r.system.assign(pmt.begin()+16,pmt.begin()+16+ss);r.video=pmt.data()+16+ss;r.video_size=vs;
        native.video=r.video;PcPmtLoadCursor cur{};
        pmt_objects_begin_42e490(r,cur);
        unsigned nsteps=0;while(nsteps<4096u){++nsteps;if(pmt_objects_step_42e4f0(r,cur,native,cache,globals))break;}
        pmt_textures_begin_42e850(r,cur);
        unsigned ntex=0;while(ntex<4096u){++ntex;if(pmt_textures_step_42e8c0(r,cur,native))break;}
        // 4103F0 (material shader records for a kind, 4104D0) on every object
        // and one index past the count; kinds 0..11.
        unsigned rebinds=0;
        for(std::uint32_t i=0;i<=r.object_count_0c;++i){
            const std::uint32_t kind=1u+(car*7u+i*5u)%11u;   // kind 0: null token table entry (the PC faults)
            prepare(0x4103f0u);reinterpret_cast<unsigned*>(S)[0]=(resource<<16)|i;reinterpret_cast<unsigned*>(S)[1]=kind;run_original32();
            const auto guest_result=guest_call.out_eax;
            std::uint32_t native_result=0;
            try{native_result=pmt_object_shaders_4103f0(r,i,kind,native,cache,globals);}
            catch(const std::exception& e){report(car,std::string("4103F0 native threw: ")+e.what());break;}
            if(guest_result!=native_result)report(car,"4103F0 result object "+std::to_string(i));
            ++rebinds;
        }
        ++cases;
        if(steps!=nsteps||tex_steps!=ntex)report(car,"step counts "+std::to_string(steps)+"/"+std::to_string(nsteps)+" "+std::to_string(tex_steps)+"/"+std::to_string(ntex));
        // Guest objects of this car in creation order.
        std::vector<std::pair<std::uint32_t,std::uint32_t>> created;
        fake_d3d9::for_each_object(heap_mark,[&](std::uint32_t p,std::uint32_t vt){created.push_back({p,vt});guest_ordinal[p]=++guest_objects;});
        heap_mark=fake_d3d9::heap_top();
        // Guest calls, normalised like the native log.
        std::vector<std::vector<std::uint32_t>> gcalls;std::vector<std::vector<std::uint8_t>> gbuffers;std::size_t next=0;
        auto take=[&](std::uint32_t vt){while(next<created.size()&&created[next].second!=vt)++next;
            if(next>=created.size())throw std::runtime_error("guest object order");return created[next++].first;};
        const auto n=fake_d3d9::log_count();
        for(std::uint32_t k=0;k<n;++k){
            const auto* rec=fake_d3d9::log_record(k);const auto id=rec[0];const auto* a=rec+2;
            if(id==26||id==27){gcalls.push_back({id,a[0],a[1],a[2],a[3]});
                const auto obj=g32(a[4]);(void)obj;}
            else if(id==0x100b)gcalls.push_back({id,a[0],a[1],a[3]});
            else if(id==0x100c)gcalls.push_back({id});
            else if(id==92)gcalls.push_back({92,guest_id(a[0])});
            else if(id==86||id==91){
                std::vector<std::uint32_t> c{id};
                // objects are matched by creation order below
                gcalls.push_back(c);}
            else if(id==0x5001||id==0x5002){std::vector<std::uint32_t> c{id,a[0]-video};for(unsigned j=1;j<10;++j)c.push_back(a[j]);gcalls.push_back(c);}
            else gcalls.push_back({0xdead0000u|id});
        }
        // Fill declaration/shader payloads and buffer contents from the objects.
        for(auto& c:gcalls){
            if(c[0]==86){const auto p=take(fake_d3d9::DeclVt);
                for(std::uint32_t e=p+0x20;;e+=8){c.push_back(g32(e));c.push_back(g32(e+4));if((g32(e)&0xffffu)==0xffu)break;}}
            else if(c[0]==91){const auto p=take(fake_d3d9::ShaderVt);
                for(std::uint32_t t=p+0x20;;t+=4){c.push_back(g32(t));if(g32(t)==0xffffu)break;}}
        }
        for(auto& [p,vt]:created)if(vt==fake_d3d9::BufferVt){const auto len=g32(p+4);
            gbuffers.emplace_back(reinterpret_cast<const std::uint8_t*>(p+0x40),reinterpret_cast<const std::uint8_t*>(p+0x40+len));}
        std::vector<std::vector<std::uint32_t>> ncalls(native.calls.begin()+std::ptrdiff_t(native_call_mark),native.calls.end());
        native_call_mark=native.calls.size();
        std::vector<std::vector<std::uint8_t>> nbuffers;
        for(std::uint32_t h=native_object_mark;h<native.objects.size();++h)
            if(native.objects[h].kind==26||native.objects[h].kind==27)nbuffers.push_back(native.objects[h].bytes);
        if(std::getenv("OR2_PMT_INJECT_BUFFER")&&car==29&&nbuffers.size()>1)nbuffers[1].back()^=1u;
        if(native.objects.size()-native_object_mark!=created.size())report(car,"object count "+std::to_string(created.size())+"/"+std::to_string(native.objects.size()-native_object_mark));
        native_object_mark=std::uint32_t(native.objects.size());
        if(gcalls.size()!=ncalls.size())report(car,"call count "+std::to_string(gcalls.size())+"/"+std::to_string(ncalls.size()));
        for(std::size_t k=0;k<std::min(gcalls.size(),ncalls.size());++k)if(gcalls[k]!=ncalls[k]){
            std::ostringstream o;o<<"call "<<k<<" guest";for(auto v:gcalls[k])o<<" "<<std::hex<<v;o<<" native";for(auto v:ncalls[k])o<<" "<<std::hex<<v;
            report(car,o.str());break;}
        if(gbuffers!=nbuffers){std::size_t k=0;while(k<gbuffers.size()&&k<nbuffers.size()&&gbuffers[k]==nbuffers[k])++k;
            std::size_t b=0;if(k<gbuffers.size()&&k<nbuffers.size())while(b<gbuffers[k].size()&&b<nbuffers[k].size()&&gbuffers[k][b]==nbuffers[k][b])++b;
            report(car,"buffer contents: buffer "+std::to_string(k)+"/"+std::to_string(gbuffers.size())+" byte "+std::to_string(b)+
                " sizes "+std::to_string(k<gbuffers.size()?gbuffers[k].size():0)+"/"+std::to_string(k<nbuffers.size()?nbuffers[k].size():0));
            if(k<gbuffers.size()&&k<nbuffers.size()&&std::getenv("OR2_PMT_DEBUG")){std::cerr<<" guest";for(unsigned j=0;j<16;++j)std::cerr<<" "<<unsigned(gbuffers[k][j]);
                std::cerr<<char(10)<<" native";for(unsigned j=0;j<16;++j)std::cerr<<" "<<unsigned(nbuffers[k][j]);std::cerr<<char(10);
                std::size_t diffs=0;for(std::size_t j=0;j<gbuffers[k].size();++j)diffs+=gbuffers[k][j]!=nbuffers[k][j];std::cerr<<" differing bytes "<<diffs<<char(10);}}
        // System section, bank and cursors.
        const std::uint32_t header=g32(bank+0x24)-system;
        auto gsys=normalise(reinterpret_cast<const std::uint8_t*>(system),ss,system,header,guest_id);
        auto nsys=normalise(r.system.data(),r.system.size(),0,r.texture_table_24,[](std::uint32_t h){return h;});
        if(inject&&car==29){Bytes s(nsys.data(),nsys.size());const auto rec=0x18u+(s.u32(4)-1u)*0x3cu;const auto m=s.u32(rec+0xc);s.put32(m+0xc,s.u32(m+0xc)^1u);}
        if(gsys!=nsys){std::size_t k=0;while(gsys[k]==nsys[k])++k;report(car,"system section at +"+std::to_string(k));}
        const std::array<std::uint32_t,9> gb{g32(bank+0)-system,g32(bank+4)-system,g32(bank+0xc),g32(bank+0x20)-system,header,
            g32(0x95b230),g32(0x95b234),g32(0x95b22c),g32(0x957be0)-system};
        const std::array<std::uint32_t,9> nb{0,r.texture_records_04,r.object_count_0c,r.object_table_20,r.texture_table_24,
            cur.object_95b230,cur.offset_95b234,cur.texture_95b22c,cur.texture_record_957be0};
        if(gb!=nb)report(car,"bank/cursor fields");
        // Shader cache 95C058.
        if(g32(0x95c058+0x1800)!=cache.count||g32(0x95c058+0x1804)!=cache.high_water)report(car,"shader cache counts");
        for(std::uint32_t k=0;k<cache.count&&k<256u;++k){
            const auto e=0x95c058u+k*0x18u;const auto& ne=cache.entries[k];
            if(g32(e)!=ne.hash||g32(e+4)!=ne.key||g32(e+8)!=ne.key3||guest_id(g32(e+0xc))!=ne.shader||
               guest_id(g32(e+0x10))!=ne.declaration||g32(e+0x14)!=ne.references){report(car,"shader cache entry "+std::to_string(k));break;}
        }
        std::cerr<<"car "<<car<<" steps="<<steps<<"/"<<tex_steps<<" calls="<<gcalls.size()<<" objects="<<created.size()
                 <<" buffers="<<gbuffers.size()<<" cache="<<cache.count<<"\n";
    }
    std::cout<<"{\"routine\":\"pmt_loader_probe\",\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}"<<'\n';return 0;
}

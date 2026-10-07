if(only=="shadow_volume_probe"){
    // Original car shadow volumes against pc_shadow_volume on each retail car
    // bank loaded by the original loader (guest) and the native loader:
    // 46BA20 (shadow objects, 420E50 blocks and quads, 4211F0/421680 edges)
    // then frames of 422550 (421740/421850 silhouette, 421A90 stencil passes)
    // and 422740 (4220F0 darkening) under random light directions and
    // current matrices. Guest leaves: malloc 580253 (bump heap, mirrored
    // natively at the same addresses). Compared: the car words +2B8..+2C4,
    // the whole shadow heap, the range types of the model (6 -> 5), every
    // device call (Lock calls excluded: the native side locks through the
    // service), the matrix arena and pointer/depth.
    using namespace outrun::platform;
    if(argc<=7||std::string(argv[7])=="-")throw std::runtime_error("shadow volume oracle requires pinned d3dx9_29.dll");
    const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
    const char* cars=std::getenv("OR2_CARS_DIR");
    if(!cars)throw std::runtime_error("set OR2_CARS_DIR to the retail Cars directory");
    constexpr unsigned base=0x3a800000,car_window=0x5c000000,Heap=0x4c000000,HeapSize=0x800000;
    map_at(base,0x10000,PROT_READ|PROT_WRITE|PROT_EXEC);map_at(car_window,0x2000000,PROT_READ|PROT_WRITE);
    map_at(Heap,HeapSize,PROT_READ|PROT_WRITE);
    map_original_arithmetic_bridge(file);
    init_matrix_oracle_globals();wire_d3dx_imports(map_d3dx29(argv[7]));
    {   auto* iat=reinterpret_cast<void*>(0x596000u);
        if(mprotect(iat,4096,PROT_READ|PROT_WRITE))throw std::runtime_error("shadow IAT");
        *reinterpret_cast<std::uint32_t*>(0x5962a0u)=D3DX_BASE+(0x4524b8u-0x400000u); // D3DXDeclaratorFromFVF
        *reinterpret_cast<std::uint32_t*>(0x5962b0u)=D3DX_BASE+(0x440ba3u-0x400000u); // D3DXMatrixTranspose (generic)
        *reinterpret_cast<std::uint32_t*>(0x5962dcu)=D3DX_BASE+(0x440e9cu-0x400000u); // D3DXMatrixInverse (generic)
        if(mprotect(iat,4096,PROT_READ))throw std::runtime_error("shadow IAT restore");}
    {   static const std::uint8_t finite_stub[]{0x8b,0x44,0x24,0x08,0x25,0x00,0x00,0xf0,0x7f,0x3d,0x00,0x00,0xf0,0x7f,0x0f,0x95,0xc0,0x0f,0xb6,0xc0,0xc3};
        std::memcpy(reinterpret_cast<void*>(base+0x7000),finite_stub,sizeof(finite_stub));
        auto* iat=reinterpret_cast<void*>(D3DX_BASE+0x1000u);
        if(mprotect(iat,4096,PROT_READ|PROT_WRITE))throw std::runtime_error("d3dx IAT page");
        *reinterpret_cast<std::uint32_t*>(D3DX_BASE+0x1270u)=base+0x7000u;
        if(mprotect(iat,4096,PROT_READ))throw std::runtime_error("d3dx IAT restore");
    }
    {   // operator new (5802CF) for the loader's shader cache nodes and malloc (580253) for the shadow blocks:
        // bump allocators, 16-byte aligned ([base+0x7100] / [base+0x7104] the next addresses).
        map_at(0x57000000u,0x100000,PROT_READ|PROT_WRITE);
        *reinterpret_cast<std::uint32_t*>(base+0x7100)=0x57000000u;
        *reinterpret_cast<std::uint32_t*>(base+0x7104)=Heap;
        auto bump=[&](std::uint32_t cell,std::uint32_t at,std::uint32_t site){
            std::vector<std::uint8_t> st{0xa1};for(unsigned k=0;k<4;++k)st.push_back(std::uint8_t(cell>>(k*8)));
            const std::uint8_t tail[]{0x8b,0x4c,0x24,0x04,0x83,0xc1,0x0f,0x83,0xe1,0xf0,0x01,0x0d};
            st.insert(st.end(),tail,tail+sizeof(tail));for(unsigned k=0;k<4;++k)st.push_back(std::uint8_t(cell>>(k*8)));
            st.push_back(0xc3);
            std::memcpy(reinterpret_cast<void*>(at),st.data(),st.size());
            r046_write_rel32(site,0xe9,at);};
        bump(base+0x7100,base+0x7200,0x5802cfu);
        bump(base+0x7104,base+0x7240,0x580253u);
    }
    fake_d3d9::install();
    if(fake_d3d9::remap_fs0(0x40f4e0u,0x40f700u,base+0xfff0u)!=3)throw std::runtime_error("40F4E0 fs:[0] patch count");
    for(std::uint32_t page=0x62f000u;page<0x98d000u;page+=0x1000u)mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE);
    auto g32=[](std::uint32_t a){return *reinterpret_cast<const std::uint32_t*>(a);};
    auto p32=[](std::uint32_t a,std::uint32_t v){*reinterpret_cast<std::uint32_t*>(a)=v;};
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
        void draw_primitive_up(std::uint32_t type,std::uint32_t count,const void* d,std::uint32_t stride)override{
            // Data compared like the fake: (count + 2) * stride bytes.
            std::vector<std::uint32_t> c{83,type,count,stride};const auto n=(std::size_t(count)+2u)*stride/4u;
            for(std::size_t k=0;k<n;++k){std::uint32_t u;std::memcpy(&u,static_cast<const std::uint8_t*>(d)+k*4,4);c.push_back(u);}log(c);}
        void set_fvf(std::uint32_t f)override{log({89,f});}
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
    std::map<std::uint32_t,std::uint32_t> ordinal;std::uint32_t objects_seen=0,heap_mark=fake_d3d9::heap_top();
    auto sync_ordinals=[&]{fake_d3d9::for_each_object(heap_mark,[&](std::uint32_t p,std::uint32_t){ordinal[p]=++objects_seen;});heap_mark=fake_d3d9::heap_top();};
    auto gid=[&](std::uint32_t p)->std::uint32_t{if(!p)return 0;auto it=ordinal.find(p);if(it==ordinal.end())return 0xbad00000u|(p&0xfffffu);return it->second;};
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
    PcRenderGlobals rg;
    for(auto& r:rg.ranges)if(r.base!=0x740c98u)for(std::uint32_t k=0;k<r.size;k+=4)r.words[k/4]=g32(r.base+k);
    std::map<std::uint32_t,Loaded*> resident;
    PcFlushContext fc{native,rg,[&](std::uint32_t r)->PcPmtResources*{auto it=resident.find(r);return it==resident.end()?nullptr:&it->second->r;},
        pc_colour_list_alt_408c80,1};
    std::mt19937 rng(0x422550u);
    auto real=[&](float lo,float hi){return lo+(hi-lo)*(float(rng()&0xffffu)/65535.f);};
    unsigned cases=0,mismatches=0,frames=0,quads=0,calls_compared=0;bool injected=false;
    auto report=[&](const std::string& what){++mismatches;if(mismatches<=12)std::cerr<<"MISMATCH "<<what<<char(10);};
    auto guest_calls=[&]{
        std::vector<std::vector<std::uint32_t>> out;std::uint32_t data=0;const auto* df=fake_d3d9::data_log();
        auto datum=[&](std::uint32_t n,std::vector<std::uint32_t>& c){for(std::uint32_t k=0;k<n;++k){std::uint32_t u;std::memcpy(&u,df+data+k,4);c.push_back(u);}data+=n;};
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
            case 83:{std::vector<std::uint32_t> c{83,a[0],a[1],a[3]};datum(std::uint32_t((std::size_t(a[1])+2u)*a[3]/4u),c);out.push_back(c);break;}
            case 89:out.push_back({89,a[0]});break;
            case 87:case 92:case 104:case 107:out.push_back({id,gid(a[0])});break;
            case 94:case 109:{std::vector<std::uint32_t> c{id,a[0],a[2]};datum(a[2]*4u,c);out.push_back(c);break;}
            case 100:out.push_back({100,a[0],gid(a[1]),a[2],a[3]});break;
            case 53:out.push_back({53,a[0],a[1]});break;
            case 51:{std::vector<std::uint32_t> c{51,a[0]};datum(26,c);out.push_back(c);break;}
            case 0x2004:out.push_back({0x2004});break;
            case 0x100b:case 0x100c:break;                     // buffer Lock/Unlock: the native side locks through the service
            case 0x4002:case 0x1002:case 0x2002:case 0x3002:out.push_back({2});break;
            default:out.push_back({0xdead0000u|id});break;
            }
        }
        return out;
    };
    auto compare_calls=[&](const std::string& what){
        sync_ordinals();auto g=guest_calls();
        auto& n=native.calls;
        // Native texture handles are loader ordinals; guest ones are mapped by gid.
        ++cases;calls_compared+=unsigned(g.size());
        if(inject&&!injected&&what.rfind("422550",0)==0&&!n.empty()){n.back().push_back(1);injected=true;}
        if(g.size()!=n.size())report(what+" call count guest="+std::to_string(g.size())+" native="+std::to_string(n.size()));
        for(std::size_t k=0;k<std::min(g.size(),n.size());++k)if(g[k]!=n[k]){
            std::ostringstream o;o<<what<<" call "<<k<<" guest";for(std::size_t q=0;q<std::min<std::size_t>(g[k].size(),8);++q)o<<" "<<std::hex<<g[k][q];
            o<<" | native";for(std::size_t q=0;q<std::min<std::size_t>(n[k].size(),8);++q)o<<" "<<std::hex<<n[k][q];report(o.str());break;}
        n.clear();fake_d3d9::reset_log();
    };
    fake_d3d9::reset_log();native.calls.clear();
    prepare(0x40f6b0);run_original32();render_state_init_40f6b0(fc);
    compare_calls("40F6B0");
    // Native shadow memory: the same heap window and car/light cells at the guest addresses.
    std::vector<std::uint8_t> nheap(HeapSize),ncar(0x1000),nlight(0x10);
    constexpr std::uint32_t Car=base+0x4000,Light=base+0x5000;
    const unsigned first_car=std::getenv("OR2_CAR")?unsigned(std::atoi(std::getenv("OR2_CAR"))):0u;
    for(unsigned car=first_car;car<30;++car){
        const std::uint32_t layout_id=[&]{std::uint32_t v;std::memcpy(&v,EmbeddedVehicleLayouts+car*0x128u+8u,4);return v;}();
        const std::uint32_t resource=layout_id>>16;
        Loaded vehicle;load(vehicle,std::string(cars)+"/"+(VehicleResourcePaths[car]+5),resource,car_window);
        resident[resource]=&vehicle;
        // Guest heap and native heap: same pattern, same next address.
        for(std::uint32_t k=0;k<HeapSize;k+=4){const std::uint32_t v=0xcdcd0000u+(k&0xfffcu);p32(Heap+k,v);std::memcpy(nheap.data()+k,&v,4);}
        p32(base+0x7104,Heap);std::uint32_t nnext=Heap;
        std::memset(reinterpret_cast<void*>(Car),0,0x1000);*reinterpret_cast<std::uint8_t*>(Car+0x11)=std::uint8_t(car);
        std::fill(ncar.begin(),ncar.end(),0);ncar[0x11]=std::uint8_t(car);
        PcRaceMemory m;m.map(Heap,nheap.data(),nheap.size());m.map(Car,ncar.data(),ncar.size());m.map(Light,nlight.data(),nlight.size());
        m.map(vehicle.system,vehicle.r.system.data(),vehicle.r.system.size());
        const std::uint32_t sys=vehicle.system;
        PcShadowServices sv{m,native,
            [&](std::uint32_t n){const std::uint32_t at=nnext;nnext+=(n+15u)&~15u;return at;},
            [&](std::uint32_t){},
            [&](std::uint32_t r,std::uint32_t& count,std::uint32_t& table){
                if(r!=resource)return false;count=vehicle.r.object_count_0c;table=sys+vehicle.r.view().u32(vehicle.r.object_table_20);return true;},
            [&](std::uint32_t field){return field>=sys&&field<sys+vehicle.r.system.size()?sys:0u;},
            [&](std::uint32_t h,std::uint32_t,std::uint32_t& size)->const std::uint8_t*{
                if(!h||h>native.objects.size())return nullptr;auto& o=native.objects[h-1];size=std::uint32_t(o.bytes.size());return o.bytes.data();},
            1.f,1.f};
        const std::string tag=" car "+std::to_string(car);
        // 46BA20.
        fake_d3d9::reset_log();native.calls.clear();
        prepare(0x46ba20u);*reinterpret_cast<unsigned*>(S)=Car;run_original32();
        try{shadow_alloc_46ba20(sv,Car);}catch(const std::exception& e){report(std::string("46BA20 threw: ")+e.what()+tag);resident.erase(resource);continue;}
        compare_calls("46BA20"+tag);
        if(std::memcmp(reinterpret_cast<const void*>(Car),ncar.data(),0x1000))report("46BA20 car work"+tag);
        if(g32(base+0x7104)!=nnext)report("46BA20 heap size"+tag);
        {const std::uint32_t used=g32(base+0x7104)-Heap;
         if(std::memcmp(reinterpret_cast<const void*>(Heap),nheap.data(),used))
             for(std::uint32_t k=0;k<used;++k)if(reinterpret_cast<const std::uint8_t*>(Heap)[k]!=nheap[k]){std::ostringstream o;o<<"46BA20 heap +"<<std::hex<<k;report(o.str()+tag);break;}}
        ++cases;
        // Range types of the model (6 -> 5 in 420E50): compare the whole system section's range words.
        for(const std::uint32_t object:{ncar[0x2bc]|ncar[0x2bd]<<8|ncar[0x2be]<<16|std::uint32_t(ncar[0x2bf])<<24}){
            if(!object)continue;
            const std::uint32_t o=m.u32(object+0x1c);if(!o)continue;
            const std::uint32_t n=m.u32(m.u32(o+0x1c)+sys+0x28),entry=m.u32(o+0x20)+sys+n*8u;
            bool bad=false;
            for(std::int32_t k=0;k<m.i32(entry+4)&&!bad;++k){
                const std::uint32_t prim=m.u32(o+0x24)+sys+(m.u32(entry)+std::uint32_t(k))*0x14u;
                const std::uint32_t draw=m.u32(o+0x28)+sys+m.u32(prim+4)*0x20u;
                for(std::int32_t r=0;r<m.i32(draw+0xc);++r){const std::uint32_t rr=m.u32(o+0x2c)+sys+(m.u32(draw+8)+std::uint32_t(r))*0x10u;
                    if(g32(rr)!=m.u32(rr)){bad=true;break;}}
            }
            if(bad)report("range type"+tag);
        }
        // Frames.
        const std::uint32_t s0=g32(Car+0x2bc),s1=g32(Car+0x2c4);
        const unsigned nframes=1u+rng()%4u;
        for(unsigned f=0;f<nframes;++f){
            for(unsigned k=0;k<3;++k){const float v=real(-1,1);std::memcpy(reinterpret_cast<void*>(Light+k*4),&v,4);std::memcpy(nlight.data()+k*4,&v,4);}
            reset_matrix_oracle_globals();
            std::vector<std::uint8_t> arena(0x1000);Bytes ar(arena.data(),arena.size());
            for(unsigned k=0;k<16;++k)ar.putf(k*4,k==15?1.f:(k>=12?real(-3,3):real(-1,1)));
            std::memcpy(reinterpret_cast<void*>(MatrixArena),arena.data(),arena.size());
            PcMatrixStack stack{ar,0,0,64};
            PcShadowFrame fr{fc,stack,g32(0x7d25f0)};
            for(const std::uint32_t object:{s0,s1}){
                if(!object)continue;
                fake_d3d9::reset_log();native.calls.clear();
                prepare(0x422550u);{auto* st=reinterpret_cast<unsigned*>(S);st[0]=object;st[1]=Light;}run_original32();
                try{shadow_draw_422550(sv,fr,object,Light);}catch(const std::exception& e){report(std::string("422550 threw: ")+e.what()+tag);break;}
                compare_calls("422550"+tag+" frame "+std::to_string(f));
                quads+=m.u32(object+0x18)/4u;
            }
            if(s0){
                fake_d3d9::reset_log();native.calls.clear();
                prepare(0x422740u);*reinterpret_cast<unsigned*>(S)=s0;run_original32();
                try{shadow_darken_422740(sv,fr,s0);}catch(const std::exception& e){report(std::string("422740 threw: ")+e.what()+tag);}
                compare_calls("422740"+tag);
            }
            ++frames;
            const std::uint32_t used=g32(base+0x7104)-Heap;
            if(std::memcmp(reinterpret_cast<const void*>(Heap),nheap.data(),used))
                for(std::uint32_t k=0;k<used;++k)if(reinterpret_cast<const std::uint8_t*>(Heap)[k]!=nheap[k]){std::ostringstream o;o<<"frame heap +"<<std::hex<<k;report(o.str()+tag);break;}
            if(std::memcmp(reinterpret_cast<const void*>(MatrixArena),arena.data(),arena.size()))report("matrix arena"+tag);
            if(g32(0x89b568)!=std::uint32_t(stack.depth)||g32(0x89b564)!=MatrixArena+std::uint32_t(stack.current_offset))report("matrix pointer/depth"+tag);
        }
        resident.erase(resource);
    }
    std::cerr<<"shadow volumes: frames="<<frames<<" quads="<<quads<<" calls="<<calls_compared<<char(10);
    std::cout<<"{\"routine\":\"shadow_volume_probe\",\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}"<<'\n';return 0;
}

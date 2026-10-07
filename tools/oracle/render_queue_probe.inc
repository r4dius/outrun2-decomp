if(only=="render_queue_probe"){
    // Original 405360 -> 4050F0 -> 404D10 (protected entry bridge live) ->
    // 404B00/404A80/40A580 over real retail car PMT system sections, relocated
    // like 42E4F0. Queues, node stack, LOD threshold, matrices are compared.
    if(argc<=7||std::string(argv[7])=="-")throw std::runtime_error("render queue oracle requires pinned d3dx9_29.dll");
    const char* cars=std::getenv("OR2_CARS_DIR");
    if(!cars)throw std::runtime_error("set OR2_CARS_DIR to the retail Cars directory");
    constexpr unsigned base=0x3a800000,guest_pmt=0x5c000000;
    map_at(base,0x10000,PROT_READ|PROT_WRITE|PROT_EXEC);map_at(guest_pmt,0x800000,PROT_READ|PROT_WRITE);
    map_original_arithmetic_bridge(file);
    init_matrix_oracle_globals();wire_d3dx_imports(map_d3dx29(argv[7]));
    for(std::uint32_t page=0x7c2000u;page<0x7cd000u;page+=0x1000u)
        if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("bank pages");
    for(std::uint32_t page=0x860000u;page<0x89b000u;page+=0x1000u)
        if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("queue pages");
    for(unsigned page:{0x95a000u,0x95b000u,0x95d000u,0x79f000u})
        if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("view pages");
    unsigned cases{},mismatches{};const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
    auto compare=[&](const void* native,const void* original,std::size_t n,const char* what,unsigned car,unsigned object){
        ++cases;if(!std::memcmp(native,original,n))return;
        ++mismatches;if(mismatches<12){const auto* a=static_cast<const std::uint8_t*>(native);const auto* b=static_cast<const std::uint8_t*>(original);
            for(std::size_t i=0;i<n;++i)if(a[i]!=b[i]){unsigned nw=0,pw=0;std::memcpy(&nw,a+(i&~std::size_t(3)),4);std::memcpy(&pw,b+(i&~std::size_t(3)),4);
                std::cerr<<what<<" car="<<car<<" object="<<object<<" offset=0x"<<std::hex<<i<<" PC="<<pw<<" native="<<nw<<std::dec<<"\n";break;}}
    };
    std::mt19937 rng(0x4f523d10u);
    auto real=[&](float lo,float hi){return lo+(hi-lo)*(float(rng()&0xffffu)/65535.f);};
    unsigned queued=0,billboards=0,visible_calls=0;bool injected=false;
    for(unsigned car=0;car<30;++car){
        const unsigned resource=outrun::platform::VehicleResourceIds[car];
        std::string path=std::string(cars)+"/"+(outrun::platform::VehicleResourcePaths[car]+5);
        std::ifstream in(path,std::ios::binary);if(!in)throw std::runtime_error("missing "+path);
        std::vector<std::uint8_t> packed((std::istreambuf_iterator<char>(in)),std::istreambuf_iterator<char>());
        std::vector<std::uint8_t> pmt(64u*1024u*1024u);uLongf size=uLongf(pmt.size());
        if(uncompress(pmt.data(),&size,packed.data(),uLong(packed.size()))!=Z_OK)throw std::runtime_error("inflate "+path);
        pmt.resize(size);Bytes pb(pmt.data(),pmt.size());
        const auto objects=pb.u32(0),ss=pb.u32(8);
        if(ss>0x7f0000u)throw std::runtime_error("system section too large for the guest window");
        std::vector<std::uint8_t> system(pmt.begin()+16,pmt.begin()+16+ss);
        // Guest copy relocated exactly like 42E4F0 (+04..+38 of each object).
        std::memcpy(reinterpret_cast<void*>(guest_pmt),system.data(),system.size());
        for(unsigned o=0;o<objects;++o)for(unsigned f=4;f<=0x38;f+=4)
            *reinterpret_cast<std::uint32_t*>(guest_pmt+0x18+o*0x3c+f)+=guest_pmt;
        const std::uint32_t bank=0x7c2800u+resource*0x48u;
        *reinterpret_cast<std::uint32_t*>(bank+0x0c)=objects;
        *reinterpret_cast<std::uint32_t*>(base+0x100)=guest_pmt+0x18;
        *reinterpret_cast<std::uint32_t*>(bank+0x20)=base+0x100;
        outrun::platform::PcModelBank native_bank{resource,Bytes(system.data(),system.size()),objects};
        for(unsigned trial=0;trial<4;++trial){
            // View: camera behind and above the car, random yaw; real perspective leaf.
            PcCameraDevice cam_device;std::array<std::uint8_t,4*64> cam_arena{};
            PcMatrixStack cam_stack{Bytes(cam_arena.data(),cam_arena.size()),0,0,4};
            outrun::driving::pc_matrix_identity(cam_stack);
            camera_perspective_404310(cam_stack,real(0.6f,1.2f),1.333f,real(0.1f,1.f),real(200,3000),0.f,0.f,cam_device);
            outrun::platform::PcRenderView view{};
            const float yaw=real(-3.1f,3.1f),dist=real(3,trial==3?400.f:25.f);
            view.eye={std::sin(yaw)*dist,real(0.5f,4),std::cos(yaw)*dist};view.target={0.f,0.5f,0.f};
            {   std::array<std::uint8_t,64> v{};Bytes vb(v.data(),64);
                std::array<std::uint8_t,2*64> a{};PcMatrixStack vs{Bytes(a.data(),a.size()),0,0,2};outrun::driving::pc_matrix_identity(vs);
                outrun::driving::pc_matrix_rotate_x(vs,real(-0.3f,0.1f));outrun::driving::pc_matrix_rotate_y(vs,-yaw);
                outrun::driving::pc_matrix_translate_vector(vs,{-view.eye[0],-view.eye[1],-view.eye[2]});
                for(unsigned k=0;k<64;++k)view.view_95d860[k]=vs.current().u8(k);(void)vb;}
            view.frustum_95bf40=cam_device.frustum_95bf40;view.planes_95bf58=cam_device.planes_95bf58;
            std::memcpy(reinterpret_cast<void*>(0x95d860),view.view_95d860.data(),64);
            std::memcpy(reinterpret_cast<void*>(0x95bf40),view.frustum_95bf40.data(),24);
            std::memcpy(reinterpret_cast<void*>(0x95bf58),view.planes_95bf58.data(),64);
            *reinterpret_cast<std::uint32_t*>(0x79f574)=base+0x3000;
            std::memcpy(reinterpret_cast<void*>(base+0x3000+0xf8),view.eye.data(),12);
            std::memcpy(reinterpret_cast<void*>(base+0x3000+0x104),view.target.data(),12);
            prepare(0x405160);run_original32();*reinterpret_cast<std::uint32_t*>(0x8999b0)=0;
            outrun::platform::PcRenderQueue opaque,alpha;outrun::platform::render_queues_reset_405160(opaque,alpha);
            reset_matrix_oracle_globals();
            std::vector<std::uint8_t> arena(0x1000);Bytes ar(arena.data(),arena.size());
            for(unsigned k=0;k<16;++k)ar.putf(k*4,(k%5u)==0u?1.f:0.f);
            ar.putf(0x30,real(-1,1));ar.putf(0x38,real(-1,1));
            std::memcpy(reinterpret_cast<void*>(MatrixArena),arena.data(),arena.size());
            PcMatrixStack stack{ar,0,0,64};
            const std::int32_t lod=std::int32_t(rng()%24u)-4;
            *reinterpret_cast<std::int32_t*>(0x8999b8)=lod;*reinterpret_cast<std::int32_t*>(0x8999a0)=0;
            outrun::platform::PcRenderContext ctx{opaque,alpha,view,stack,
                [&](std::uint32_t id)->const outrun::platform::PcModelBank*{return id==resource?&native_bank:nullptr;},lod};
            for(unsigned object=0;object<objects;++object){
                const std::uint32_t id=(resource<<16)|object;
                const std::int32_t to_opaque=std::int32_t(rng()%3u!=0u),cb=std::int32_t(rng()%4u)-1,flag=std::int32_t(rng()&1u);
                const std::uint32_t colour=0x64cf00u+(rng()%64u)*4u;
                // 0: 405360, 1: 405450 morph (target = another object of the car), 2: 405580 palette
                const unsigned entry=rng()%3u;
                const std::uint32_t target=(resource<<16)|(rng()%objects);
                const std::uint32_t weight=rng();*reinterpret_cast<std::uint32_t*>(0x95aecc)=weight;ctx.morph_weight_95aecc=weight;
                const std::uint32_t palette_count=1u+rng()%4u;std::array<std::uint8_t,256> palette{};
                for(unsigned k=0;k<64;++k){const float v=(k%16==15)?1.f:real(-1.5f,1.5f);std::memcpy(palette.data()+k*4,&v,4);}
                std::memcpy(reinterpret_cast<void*>(base+0x2000),palette.data(),palette.size());
                auto* st=reinterpret_cast<unsigned*>(S);
                if(entry==0){prepare(0x405360);st[0]=id;st[1]=unsigned(to_opaque);st[2]=0;st[3]=colour;st[4]=unsigned(cb);st[5]=unsigned(flag);}
                else if(entry==1){prepare(0x405450);st[0]=id;st[1]=target;st[2]=unsigned(to_opaque);st[3]=colour;st[4]=unsigned(cb);}
                else{prepare(0x405580);st[0]=id;st[1]=unsigned(to_opaque);st[2]=colour;st[3]=unsigned(cb);st[4]=base+0x2000u;st[5]=palette_count;}
                run_original32();
                try{
                    if(entry==0)outrun::platform::render_object_405360(ctx,id,to_opaque,Bytes(nullptr,0),colour,cb,flag);
                    else if(entry==1)outrun::platform::render_object_morph_405450(ctx,id,target,to_opaque,colour,cb);
                    else outrun::platform::render_object_palette_405580(ctx,id,to_opaque,colour,cb,palette.data(),palette_count);
                }
                catch(const std::exception& e){++cases;++mismatches;if(mismatches<12)std::cerr<<"native threw car="<<car<<" object="<<object<<": "<<e.what()<<"\n";continue;}
                ++visible_calls;
                for(unsigned which=0;which<2;++which){
                    const auto& q=which?alpha:opaque;const std::uint32_t qa=which?0x8612d8u:0x864ef8u;
                    const auto* g=reinterpret_cast<const std::uint32_t*>(qa);
                    std::array<std::uint32_t,3> hdr_n{q.count,q.matrix_count,0},hdr_p{g[0],g[2],0};
                    compare(hdr_n.data(),hdr_p.data(),8,"queue counts",car,object);
                    const std::uint32_t storage=g[5],pool=g[6],ptrs=g[4];
                    for(std::uint32_t k=0;k<std::min(q.count,g[0]);++k){
                        const auto* e=reinterpret_cast<const std::uint32_t*>(storage+k*0x3c);
                        std::array<std::uint32_t,15> pc{};for(unsigned f=0;f<15;++f)pc[f]=e[f];
                        pc[1]=(pc[1]-pool)/0x40u;pc[4]=(pc[4]-0x7c2800u)/0x48u;
                        pc[5]-=guest_pmt;pc[6]-=guest_pmt;pc[7]-=guest_pmt;
                        if(pc[11])pc[11]-=guest_pmt;                        // +2C: morph target object
                        auto nat=q.entries[k];nat[8]=pc[8];
                        if(!nat[11])nat[12]=pc[12];                         // +20, +30 without a morph: PC stack contents
                        if(inject&&car==29&&!injected){nat[0]^=1;injected=true;}
                        compare(nat.data(),pc.data(),60,"queue entry",car,object);
                        const auto ptr=reinterpret_cast<const std::uint32_t*>(ptrs)[k];
                        std::array<std::uint32_t,1> pn{storage+k*0x3c},pp{ptr};compare(pn.data(),pp.data(),4,"queue pointer",car,object);
                    }
                    for(std::uint32_t k=0;k<std::min(q.matrix_count,g[2])&&k<q.matrices.size();++k)
                        compare(q.matrices[k].data(),reinterpret_cast<void*>(pool+k*0x40),64,"matrix pool",car,object);
                }
                std::array<std::int32_t,2> sn{ctx.lod_threshold_8999b8,ctx.flag_depth_8999a0},sp{*reinterpret_cast<std::int32_t*>(0x8999b8),*reinterpret_cast<std::int32_t*>(0x8999a0)};
                compare(sn.data(),sp.data(),8,"LOD threshold/node depth",car,object);
                compare(arena.data(),reinterpret_cast<void*>(MatrixArena),arena.size(),"matrix arena",car,object);
                std::array<std::uint32_t,2> msn{MatrixArena+std::uint32_t(stack.current_offset),std::uint32_t(stack.depth)};
                compare(msn.data(),reinterpret_cast<void*>(0x89b564),8,"matrix pointer/depth",car,object);
            }
            queued+=opaque.count+alpha.count;
        }
        // Coverage: billboard nodes (flag 0x800) present in this car's objects.
        for(unsigned o=0;o<objects;++o){
            const Bytes sb(system.data(),system.size());
            const auto h=sb.u32(0x18+o*0x3c+0x18);const auto count=sb.u32(h+0x24);const auto nodes=sb.u32(0x18+o*0x3c+0x1c);
            for(unsigned n=0;n<count&&nodes+n*0x38+4<=system.size();++n)if(sb.u32(nodes+n*0x38)&0x800u)++billboards;
        }
    }
    std::cerr<<"render queue coverage: object calls="<<visible_calls<<" queued entries="<<queued<<" billboard nodes="<<billboards<<"\n";
    std::cout<<"{\"routine\":\"render_queue_probe\",\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}"<<'\n';return mismatches?1:0;
}

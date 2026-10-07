if(only=="rob_motion_engine_probe"){
    // Original RobMotion engine (4F2280 SetMotion with its bridge 1039A80,
    // 4F1D30 SetFrame, 4F1E60 SetLoop, 4F2320 SetMotionConnect, 4F2520 Calc
    // and everything below: 520750/522F70 keys, 522C20 Hermite, 51B420 bone
    // tree, 51A770 IK, 519DD0 blend with its bridge 1039C90, 51B260 pose
    // save) against rob_motion_engine over the retail data
    // (OR2_GAME_DIR: Common/bone.bin, Common/motdata_table.bin, Anims/*.sz),
    // loaded identically for both sides at 64000000. Bone sets come from
    // SetBone 4F1CF0 (malloc/free call sites 52067E/52072B -> guest bump
    // allocator = RobotHeap). Each case runs a random sequence of calls on
    // one motion object and compares after every call: the motion objects
    // 82F5F0..82FB10, the bone buffers, the engine globals 85B2B8..85B2EC,
    // the matrix arena and the stack pointer/depth, EAX.
    using namespace outrun::platform;
    if(argc<=7||std::string(argv[7])=="-")throw std::runtime_error("rob motion engine oracle requires pinned d3dx9_29.dll");
    const char* game=std::getenv("OR2_GAME_DIR");
    if(!game)throw std::runtime_error("set OR2_GAME_DIR to the retail game directory");
    const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
    map_original_arithmetic_bridge(file);
    init_matrix_oracle_globals();wire_d3dx_imports(map_d3dx29(argv[7]));
    for(std::uint32_t page=0x62f000u;page<0x98d000u;page+=0x1000u)mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE);
    constexpr std::uint32_t Data=0x64000000u,DataSize=0x4000000u,Heap=RobotHeap::Base,HeapSize=0x400000u,Stubs=0x3a850000u,NextCell=0x3a851000u;
    map_at(Data,DataSize,PROT_READ|PROT_WRITE);map_at(Heap,HeapSize,PROT_READ|PROT_WRITE);map_at(Stubs,0x2000,PROT_READ|PROT_WRITE|PROT_EXEC);
    auto g32=[](std::uint32_t a){return *reinterpret_cast<const std::uint32_t*>(a);};
    auto p32=[](std::uint32_t a,std::uint32_t v){*reinterpret_cast<std::uint32_t*>(a)=v;};
    auto pf=[](std::uint32_t a,float v){*reinterpret_cast<float*>(a)=v;};
    {
        const std::uint8_t mal[]{0x8b,0x44,0x24,0x04,0x85,0xc0,0x75,0x05,0xb8,0x01,0,0,0, 0x83,0xc0,0x0f,0x83,0xe0,0xf0,
            0x57,0x89,0xc1,0xa1,0,0,0,0, 0x89,0xc7,0x01,0x0d,0,0,0,0, 0x50,0xb0,0xcd,0xf3,0xaa,0x58,0x5f,0xc3};
        std::vector<std::uint8_t> b(mal,mal+sizeof(mal));
        std::memcpy(b.data()+23,&NextCell,4);std::memcpy(b.data()+31,&NextCell,4);
        std::memcpy(reinterpret_cast<void*>(Stubs),b.data(),b.size());
        *reinterpret_cast<std::uint8_t*>(Stubs+0x100)=0xc3;
        r046_write_rel32(0x52067eu,0xe8,Stubs);r046_write_rel32(0x52072bu,0xe8,Stubs+0x100);
    }
    {   auto* iat=reinterpret_cast<void*>(0x596000u);
        if(mprotect(iat,4096,PROT_READ|PROT_WRITE))throw std::runtime_error("engine IAT");
        *reinterpret_cast<std::uint32_t*>(0x5962dcu)=D3DX_BASE+(0x440e9cu-0x400000u);   // D3DXMatrixInverse (generic)
        *reinterpret_cast<std::uint32_t*>(0x5962b4u)=D3DX_BASE+(0x441288u-0x400000u);   // D3DXMatrixScaling
        if(mprotect(iat,4096,PROT_READ))throw std::runtime_error("engine IAT restore");}
    {   static const std::uint8_t finite_stub[]{0x8b,0x44,0x24,0x08,0x25,0x00,0x00,0xf0,0x7f,0x3d,0x00,0x00,0xf0,0x7f,0x0f,0x95,0xc0,0x0f,0xb6,0xc0,0xc3};
        std::memcpy(reinterpret_cast<void*>(Stubs+0x200),finite_stub,sizeof(finite_stub));
        auto* iat=reinterpret_cast<void*>(D3DX_BASE+0x1000u);
        if(mprotect(iat,4096,PROT_READ|PROT_WRITE))throw std::runtime_error("d3dx IAT page");
        *reinterpret_cast<std::uint32_t*>(D3DX_BASE+0x1270u)=Stubs+0x200u;
        if(mprotect(iat,4096,PROT_READ))throw std::runtime_error("d3dx IAT restore");}
    auto read_file=[&](const std::string& path){std::ifstream in(path,std::ios::binary);if(!in)throw std::runtime_error("missing "+path);
        return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(in)),std::istreambuf_iterator<char>());};
    // ---- data at 70000000: bone.bin, the relocated motion table, the groups -----
    std::vector<std::uint8_t> data(DataSize,0);
    const std::uint32_t Bones=Data,Table=Data+0x10000u;
    {const auto b=read_file(std::string(game)+"/Common/bone.bin");std::memcpy(data.data(),b.data(),b.size());}
    {const auto t=read_file(std::string(game)+"/Common/motdata_table.bin");std::memcpy(data.data()+0x10000,t.data(),t.size());}
    PcRaceMemory dm;dm.map(Data,data.data(),data.size());
    std::array<std::uint8_t,0x504> tables{};dm.map(0x84d970u,tables.data(),tables.size());
    rob_motion_relocate_4f1f30(dm,Table);
    const std::uint32_t groups=dm.u32(0x84de70u);
    std::vector<std::uint32_t> group_base(0x3f,0),group_count(0x3f,0);
    std::uint32_t cursor=0x100000u;
    for(std::uint32_t g=0;g<groups&&g<0x3fu;++g){
        std::string name;for(std::uint32_t p=dm.u32(Table+g*16);;++p){const auto ch=dm.u8(p);if(!ch)break;name.push_back(char(std::tolower(ch)));}
        const auto dot=name.rfind('.');if(dot!=std::string::npos)name[dot+1]='s';
        const auto packed=read_file(std::string(game)+"/Anims/"+name);
        std::vector<std::uint8_t> raw(8u*1024u*1024u);uLongf size=uLongf(raw.size());
        if(uncompress(raw.data(),&size,packed.data(),uLong(packed.size()))!=Z_OK)throw std::runtime_error("inflate "+name);
        if(cursor+size>DataSize)throw std::runtime_error("motion data window");
        std::memcpy(data.data()+cursor,raw.data()+4,size-4);
        group_base[g]=Data+cursor;group_count[g]=dm.u32(Table+g*16+8);
        cursor=(cursor+std::uint32_t(size)+0xfffu)&~0xfffu;
    }
    std::memcpy(reinterpret_cast<void*>(Data),data.data(),data.size());
    p32(0x84d970u,Table);p32(0x84d974u,Bones);
    std::vector<std::uint32_t> group_ids;
    for(std::uint32_t g=0;g<0x3fu;++g){p32(0x84dd68u+g*4u,group_base[g]);if(group_base[g]&&group_count[g])group_ids.push_back(g);}
    for(std::uint32_t g=0;g<0x3fu;++g)std::memcpy(tables.data()+(0x84dd68u-0x84d970u)+g*4u,&group_base[g],4);
    {const std::uint32_t t=Table,b=Bones;std::memcpy(tables.data(),&t,4);std::memcpy(tables.data()+4,&b,4);}
    std::cerr<<"motion groups loaded: "<<group_ids.size()<<char(10);
    // ---- cases --------------------------------------------------------------------
    std::mt19937 rng(0x4f2520u);
    auto real=[&](float lo,float hi){return lo+(hi-lo)*(float(rng()&0xffffu)/65535.f);};
    const unsigned cases=std::max(count,1u);
    unsigned mismatches=0,calls=0,calcs=0,degenerate=0;bool skip_case=false;
    auto report=[&](const std::string& what){++mismatches;if(mismatches<=16)std::cerr<<"MISMATCH "<<what<<char(10);};
    std::vector<std::uint8_t> motions(PcRaceRobotState::BlockEnd-0x82f5f0u),engine(PcRobMotionEngineState::End-PcRobMotionEngineState::Base);
    std::array<std::uint8_t,4> step{};
    for(unsigned cs=0;cs<cases;++cs){
        const std::string tag=" case "+std::to_string(cs);
        // fresh motion objects and heap; SetBone on both sides
        std::fill(motions.begin(),motions.end(),0);
        for(auto& b:engine)b=0;
        std::memcpy(reinterpret_cast<void*>(0x82f5f0u),motions.data(),motions.size());
        std::memcpy(reinterpret_cast<void*>(PcRobMotionEngineState::Base),engine.data(),engine.size());
        std::memset(reinterpret_cast<void*>(Heap),0,HeapSize);p32(NextCell,Heap);
        RobotHeap heap;PcAllocatorStateStacks stacks{};
        const unsigned which=rng()%3u;const std::uint32_t mot=0x82f5f0u+which*0xa4u;   // the robots' motion objects (display indexes 0..2)
        const std::uint32_t entry=Bones+(rng()%9u)*16u;
        PcRaceMemory m;
        auto remap=[&]{m.clear();m.map(Data,data.data(),data.size());m.map(0x84d970u,tables.data(),tables.size());
            m.map(0x82f5f0u,motions.data(),motions.size());m.map(PcRobMotionEngineState::Base,engine.data(),engine.size());
            m.map(0x842114u,step.data(),4);heap.map(m);};
        remap();
        prepare(0x4f1cf0);guest_call.ecx=mot;reinterpret_cast<unsigned*>(S)[0]=entry;reinterpret_cast<unsigned*>(S)[1]=Bones;run_original32();
        rob_set_bone_4f1cf0(m,heap,stacks,mot,entry,Bones);
        remap();
        std::vector<std::uint8_t> arena(0x2000);outrun::driving::Bytes ar(arena.data(),arena.size());
        for(unsigned q=0;q<16;++q)ar.putf(q*4,q==15?1.f:(q>=12?real(-5,5):real(-1,1)));
        reset_matrix_oracle_globals();std::memcpy(reinterpret_cast<void*>(MatrixArena),arena.data(),arena.size());
        outrun::driving::PcMatrixStack stack{ar,0,0,64};
        PcRaceContext ctx{m,stack,nullptr,nullptr};
        auto compare=[&](const std::string& what){
            bool ok=true;
            auto diff=[&](const void* guest,const void* native,std::size_t n,const std::string& region,std::uint32_t base){
                if(!std::memcmp(guest,native,n))return;
                for(std::size_t k=0;k<n;++k)if(static_cast<const std::uint8_t*>(guest)[k]!=static_cast<const std::uint8_t*>(native)[k]){
                    std::ostringstream o;o<<what<<" "<<region<<" "<<std::hex<<base+k<<" PC "<<unsigned(static_cast<const std::uint8_t*>(guest)[k])<<" native "<<unsigned(static_cast<const std::uint8_t*>(native)[k]);
                    if(ok)report(o.str()+tag);ok=false;
                    if(std::getenv("OR2_ENGINE_DEBUG")&&region=="bones"){
                        const std::uint32_t off=std::uint32_t(base+k-heap.blocks[0].base);const std::uint32_t bi=off/0x350u,bo=off%0x350u-0x40u,B=heap.blocks[0].base+bi*0x350u;
                        std::cerr<<"  bone "<<std::dec<<bi<<" +"<<std::hex<<bo+0x40u<<" type "<<unsigned(*reinterpret_cast<std::uint8_t*>(B+0x44))<<" ik "<<unsigned(*reinterpret_cast<std::uint8_t*>(B+0x45))
                                 <<" idx "<<g32(B+0x338)<<char(10);
                        auto dump=[&](const char* nm,std::uint32_t o){std::cerr<<"   "<<nm<<" PC "<<std::hex<<g32(B+o)<<" "<<g32(B+o+4)<<" "<<g32(B+o+8)<<" native ";
                            for(unsigned q=0;q<3;++q){std::uint32_t v;std::memcpy(&v,heap.blocks[0].bytes.data()+(B+o+q*4-heap.blocks[0].base),4);std::cerr<<v<<" ";}std::cerr<<char(10);};
                        {const std::uint32_t buf=heap.blocks[0].base;const std::int32_t nb=std::int16_t(*reinterpret_cast<std::uint16_t*>(mot+0x58+0x10));
                         for(std::int32_t q=0;q<nb;++q){const std::uint32_t Q=buf+std::uint32_t(q)*0x350u;const unsigned nc=*reinterpret_cast<std::int8_t*>(Q+0x47);
                            std::uint32_t c0=nc?(g32(g32(Q+0x330))-buf)/0x350u:0xffffu;std::uint32_t nv;std::memcpy(&nv,heap.blocks[0].bytes.data()+(Q+0x318-buf),4);
                            std::cerr<<"   b"<<std::dec<<q<<" t"<<unsigned(*reinterpret_cast<std::uint8_t*>(Q+0x44))<<" k"<<unsigned(*reinterpret_cast<std::uint8_t*>(Q+0x45))<<" nc"<<nc<<" c0="<<c0
                                     <<" tk"<<*reinterpret_cast<std::uint16_t*>(Q+0x6c)<<"/"<<*reinterpret_cast<std::uint16_t*>(Q+0xb4)<<"/"<<*reinterpret_cast<std::uint16_t*>(Q+0xfc)
                                     <<" pc318="<<std::hex<<g32(Q+0x318)<<" n318="<<nv<<std::dec<<char(10);}}
                        dump("m0",0x0);dump("m4",0x10);dump("m8",0x20);dump("m12",0x30);dump("2d0",0x2d0);dump("2dc",0x2dc);dump("2e8",0x2e8);dump("2f4",0x2f4);dump("300",0x300);dump("30c",0x30c);dump("318",0x318);dump("324",0x324);}
                    break;}};
            diff(reinterpret_cast<const void*>(0x82f5f0u),motions.data(),motions.size(),"motion",0x82f5f0u);
            diff(reinterpret_cast<const void*>(PcRobMotionEngineState::Base),engine.data(),engine.size(),"engine",PcRobMotionEngineState::Base);
            for(const auto& b:heap.blocks)diff(reinterpret_cast<const void*>(b.base),b.bytes.data(),b.bytes.size(),"bones",b.base);
            diff(reinterpret_cast<const void*>(MatrixArena),arena.data(),arena.size(),"matrix arena",MatrixArena);
            if(g32(0x89b564u)!=MatrixArena+std::uint32_t(stack.current_offset)||g32(0x89b568u)!=std::uint32_t(stack.depth)){if(ok)report(what+" matrix pointer/depth"+tag);ok=false;}
            if(!ok){   // resynchronise the native side on the guest
                std::memcpy(motions.data(),reinterpret_cast<const void*>(0x82f5f0u),motions.size());
                std::memcpy(engine.data(),reinterpret_cast<const void*>(PcRobMotionEngineState::Base),engine.size());
                for(auto& b:heap.blocks)std::memcpy(b.bytes.data(),reinterpret_cast<const void*>(b.base),b.bytes.size());
                std::memcpy(arena.data(),reinterpret_cast<const void*>(MatrixArena),arena.size());
                stack.current_offset=std::ptrdiff_t(g32(0x89b564u)-MatrixArena);stack.depth=std::int32_t(g32(0x89b568u));
            }
            return ok;};
        if(!compare("SetBone"))continue;
        auto pick_id=[&]{const std::uint32_t g=group_ids[rng()%group_ids.size()];return (g<<16)|(rng()%group_count[g]);};
        const unsigned ops=4u+rng()%24u;
        for(unsigned op=0;op<ops;++op){
            const unsigned kind=op==0?0u:rng()%10u;
            std::string what;std::uint32_t guest_eax=0,native_eax=0;bool has_eax=false;
            const std::uint32_t degenerate_before=rob_motion_degenerate_blends();
            try{
                switch(kind){
                case 0:{const std::uint32_t id=rng()%12u==0u?(0x3eu<<16):pick_id();what="SetMotion "+std::to_string(id);
                    prepare(0x4f2280u);guest_call.ecx=mot;reinterpret_cast<unsigned*>(S)[0]=id;run_original32();guest_eax=guest_call.out_eax;
                    native_eax=rob_motion_set_motion_4f2280(ctx,mot,id);has_eax=true;break;}
                case 1:{const float fr=rng()%4u==0u?real(-40,-1):real(0,120);what="SetFrame";
                    prepare(0x4f1d30u);guest_call.ecx=mot;reinterpret_cast<float*>(S)[0]=fr;run_original32();
                    rob_motion_set_frame_4f1d30(ctx,mot,fr);break;}
                case 2:{const std::uint32_t n=rng()%4u;what="SetLoop";
                    prepare(0x4f1e60u);guest_call.ecx=mot;reinterpret_cast<unsigned*>(S)[0]=n;run_original32();guest_eax=guest_call.out_eax;
                    native_eax=rob_motion_set_loop_4f1e60(m,mot,n);has_eax=true;break;}
                case 3:{const std::uint32_t id=pick_id();const float fr=real(0,30),len=rng()%5u==0u?real(0,1):real(1,20);what="Connect";
                    prepare(0x4f2320u);guest_call.ecx=mot;auto* st=reinterpret_cast<unsigned*>(S);st[0]=id;std::memcpy(&st[1],&fr,4);std::memcpy(&st[2],&len,4);run_original32();
                    guest_eax=guest_call.out_eax;native_eax=rob_motion_connect_4f2320(ctx,mot,id,fr,len);has_eax=true;break;}
                default:{const float s=rng()%3u==0u?real(0.5f,2.5f):1.0f;what="Calc";++calcs;
                    pf(0x842114u,s);std::memcpy(step.data(),&s,4);
                    if(rng()%4u==0u){const float sp=real(0.2f,2.0f);pf(mot+0x18,sp);std::memcpy(motions.data()+(mot-0x82f5f0u)+0x18,&sp,4);}
                    if(!g32(mot+0x54)){p32(mot+0x54,entry);std::memcpy(motions.data()+(mot-0x82f5f0u)+0x54,&entry,4);}
                    prepare(0x4f2520u);guest_call.ecx=mot;run_original32();
                    rob_motion_calc_4f2520(ctx,mot);break;}
                }
            }catch(const std::exception& e){report(what+" native threw: "+e.what()+tag);break;}
            ++calls;
            if(inject&&cs==3u&&op==ops-1u)motions[(mot-0x82f5f0u)+0x10]^=1u;
            if(has_eax&&guest_eax!=native_eax){std::ostringstream o;o<<what<<" EAX PC "<<std::hex<<guest_eax<<" native "<<native_eax;report(o.str()+tag);}
            if(rob_motion_degenerate_blends()!=degenerate_before){++degenerate;skip_case=true;break;}   // PC reads stack garbage
            (void)compare(what);
        }
    }
    std::cerr<<"rob motion engine: calls="<<calls<<" calcs="<<calcs<<" degenerate blends (case stopped)="<<degenerate<<char(10);(void)skip_case;
    std::cout<<"{\"routine\":\"rob_motion_engine_probe\",\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}"<<'\n';return 0;
}

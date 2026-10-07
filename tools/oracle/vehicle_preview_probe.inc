// Direct PC preview bodies plus their original shared three-slot scheduler.
if(only=="frontend_vehicle_preview_probe"){
    using namespace outrun::platform;
    constexpr unsigned base=0x3a800000,car=base+0x1000,trace=base+0x2000,ready_table=base+0x5000;
    map_at(base,0x20000,PROT_READ|PROT_WRITE|PROT_EXEC);map_original_arithmetic_bridge(file);
    for(unsigned page:{0x799000u,0x830000u,0x7d2000u,0x841000u,0x780000u,0x655000u,0x7dd000u,0x7de000u})
        if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("preview globals");
    *reinterpret_cast<unsigned*>(0x799d18)=car;
    *reinterpret_cast<unsigned*>(0x780258)=1;*reinterpret_cast<std::uint8_t*>(0x7de418)=0;
    unsigned stub_index{};
    auto capture=[&](unsigned pc,unsigned nargs,bool ready=false){
        std::vector<std::uint8_t> code{0x8b,0x15};
        auto imm=[&](unsigned v){for(unsigned j=0;j<4;++j)code.push_back(std::uint8_t(v>>(8*j)));};
        imm(trace);code.insert(code.end(),{0x6b,0xd2,20,0xc7,0x82});imm(trace+16);imm(pc);
        for(unsigned i=0;i<4;++i){
            if(i<nargs)code.insert(code.end(),{0x8b,0x44,0x24,std::uint8_t(4+4*i)});else code.insert(code.end(),{0x31,0xc0});
            if(pc==0x4406f0&&i==2)code.insert(code.end(),{0x25,255,0,0,0});
            code.insert(code.end(),{0x89,0x82});imm(trace+20+4*i);
        }
        code.insert(code.end(),{0xff,0x05});imm(trace);
        if(ready){code.insert(code.end(),{0x8b,0x54,0x24,4,0x8b,0x04,0x95});imm(ready_table);}
        code.push_back(0xc3);const unsigned dst=base+0x10000+512*stub_index++;
        r060_raw_write(dst,code.data(),code.size());std::array<std::uint8_t,5> jump{0xe9,0,0,0,0};
        const unsigned delta=dst-pc-5;std::memcpy(jump.data()+1,&delta,4);r060_raw_write(pc,jump.data(),jump.size());
    };
    for(unsigned pc:{0x49a650u,0x44c0d0u,0x44c3d0u,0x44a1a0u,0x4f2210u})capture(pc,0);
    for(unsigned pc:{0x40ec60u,0x44c0a0u,0x4401d0u,0x448990u})capture(pc,1);
    for(unsigned pc:{0x440110u,0x440330u,0x448ad0u})capture(pc,2);
    capture(0x448960,1,true);
    unsigned cases{},mismatches{};const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
    auto check=[&](bool ok,const char* what){++cases;if(!ok){++mismatches;if(mismatches<12)std::cerr<<"preview "<<cases<<" "<<what<<"\n";}};
    struct Trace {std::vector<std::array<unsigned,5>> rows;} log;
    auto external=[](void* p,unsigned pc,const unsigned* a,std::size_t n){std::array<unsigned,5> row{pc,0,0,0,0};
        if(n>4)return false;for(std::size_t i=0;i<n;++i)row[i+1]=a[i];static_cast<Trace*>(p)->rows.push_back(row);return true;};
    auto ready=[](void* p,unsigned id,bool& result){static_cast<Trace*>(p)->rows.push_back({0x448960,id,0,0,0});result=*reinterpret_cast<unsigned*>(ready_table+4*id)!=0;return true;};
    FrontendVehicleLoaderServices io{&log,
        [](void* p,unsigned id,unsigned mode){static_cast<Trace*>(p)->rows.push_back({0x448ad0,id,mode,0,0});return true;},ready,
        [](void* p,unsigned id){static_cast<Trace*>(p)->rows.push_back({0x448990,id,0,0,0});return true;}};
    auto reset_trace=[&]{*reinterpret_cast<unsigned*>(trace)=0;log.rows.clear();};
    for(unsigned start=0;start<30;++start){
        std::array<std::uint8_t,0x94> object;FrontendVehicleLoader loader;VehicleCreationQueue creation;
        std::array<std::uint8_t,0x100> vehicle;std::array<std::uint8_t,12> lists;
        for(unsigned i=0;i<object.size();++i)object[i]=std::uint8_t(start*17+i*13);
        for(unsigned i=0;i<vehicle.size();++i)vehicle[i]=std::uint8_t(start*31+i);
        lists.fill(0xcd);loader.object.fill(0xab);
        Bytes b(object.data(),object.size());FrontendVehiclePreviewServices s{Bytes(loader.object.data(),loader.object.size()),Bytes(vehicle.data(),vehicle.size()),Bytes(lists.data(),lists.size()),&log,external,ready};
        s.creation=&creation;creation.object.fill(0xa5);
        std::memcpy(reinterpret_cast<void*>(base),object.data(),object.size());std::memcpy(reinterpret_cast<void*>(car),vehicle.data(),vehicle.size());
        std::memcpy(reinterpret_cast<void*>(0x83039c),loader.object.data(),loader.object.size());std::memcpy(reinterpret_cast<void*>(0x7d26a8),lists.data(),lists.size());
        std::memcpy(reinterpret_cast<void*>(0x841ac0),creation.object.data(),creation.object.size());
        auto compare=[&]{
            check(!std::memcmp(object.data(),reinterpret_cast<void*>(base),object.size()),"entire preview object");
            check(!std::memcmp(loader.object.data(),reinterpret_cast<void*>(0x83039c),loader.object.size()),"entire shared loader");
            check(!std::memcmp(vehicle.data(),reinterpret_cast<void*>(car),vehicle.size()),"entire shared car");
            check(!std::memcmp(lists.data(),reinterpret_cast<void*>(0x7d26a8),lists.size()),"shared scene lists");
            check(!std::memcmp(creation.object.data(),reinterpret_cast<void*>(0x841ac0),creation.object.size()),"whole creation queue incl untouched records");
            check(log.rows.size()==*reinterpret_cast<unsigned*>(trace)&&!std::memcmp(log.rows.data(),reinterpret_cast<void*>(trace+16),log.rows.size()*20),"ordered external calls");
        };
        auto original=[&](unsigned pc,unsigned self,std::initializer_list<unsigned> args={}){reset_trace();prepare(pc);guest_call.ecx=self;unsigned n=0;for(unsigned a:args)reinterpret_cast<unsigned*>(S)[n++]=a;run_original32();};
        original(0x48bf00,base);check(frontend_vehicle_preview_construct_48bf00(b),"construct");compare();
        original(0x48bf20,0x83039c);check(frontend_vehicle_loader_init_48bf20(loader,io),"loader init");compare();
        for(unsigned cycle=0;cycle<4;++cycle){
            unsigned selected=(start+cycle*7)%30;
            original(0x48c170,base,{selected,cycle&1});check(frontend_vehicle_preview_init_48c170(b,selected,(cycle&1)!=0,s),"init");compare();
            for(unsigned frame=0;frame<90;++frame){
                for(unsigned id=0;id<512;++id)*reinterpret_cast<unsigned*>(ready_table+4*id)=frame%7!=0;
                *reinterpret_cast<unsigned*>(ready_table+4*0xba)=frame>2;
                *reinterpret_cast<unsigned*>(ready_table+4*0xbb)=frame>5;
                original(0x48bfe0,0x83039c);check(frontend_vehicle_loader_tick_48bfe0(loader,io),"shared loader tick");compare();
                if(frame%9==8){
                    if(frame%18==17)selected=(selected+1+cycle)%30;
                    const unsigned colour=1+frame%8,unlocked=(frame/9)%2;
                    original(0x48c290,base,{selected,colour,unlocked});check(frontend_vehicle_preview_select_48c290(b,selected,colour,unlocked!=0,s),"select");compare();
                }
                original(0x48c3f0,base);check(frontend_vehicle_preview_tick_48c3f0(b,s),"preview tick");compare();
                if(frame==70){original(0x48c260,base);check(frontend_vehicle_preview_close_48c260(b,s),"close");compare();}
            }
            original(0x48c450,base);check(frontend_vehicle_preview_suspend_48c450(b,s),"suspend");compare();
            for(unsigned frame=0;frame<12;++frame){
                for(unsigned id=0;id<512;++id)*reinterpret_cast<unsigned*>(ready_table+4*id)=1;
                original(0x48bfe0,0x83039c);check(frontend_vehicle_loader_tick_48bfe0(loader,io),"flush tick");compare();
            }
            check(frontend_vehicle_loader_empty_48bfc0(loader),"flushed");
        }
        VehicleCreationServices create_api{&log,[](void* p,unsigned event,unsigned function){static_cast<Trace*>(p)->rows.push_back({0x440110,event,function,0,0});return true;}};
        for(unsigned colour=0;colour<256;++colour)for(unsigned hidden:{0u,1u,2u,255u}){
            original(0x49fa60,0);check(vehicle_creation_reset_49fa60(creation),"queue reset");compare();
            original(0x4406f0,0,{start,colour,hidden,0x3e800000});
            check(vehicle_creation_preview_4406f0(creation,start,colour,hidden,create_api),"queue preview signed colour");compare();
        }
        for(unsigned colour=0;colour<256;++colour)for(unsigned lane=0;lane<8;++lane){
            original(0x49fa60,0);check(vehicle_creation_reset_49fa60(creation),"queue reset offline");compare();
            *reinterpret_cast<std::uint8_t*>(0x655b59)=std::uint8_t(start);*reinterpret_cast<std::uint8_t*>(0x655b5a)=std::uint8_t(colour);
            *reinterpret_cast<std::uint8_t*>(0x7dd138)=std::uint8_t(lane);
            original(0x440380,0);check(vehicle_creation_offline_440380(creation,start,colour,lane,create_api),"offline player descriptor");compare();
        }
        original(0x49fa60,0);check(vehicle_creation_reset_49fa60(creation),"queue reset for appends");compare();
        for(unsigned lane=0;lane<16;++lane){
            original(0x49fa80,0,{8+lane,lane,start,1+lane%8,0x10203040+lane});
            check(vehicle_creation_append_49fa80(creation,8+lane,lane,start,1+lane%8,0x10203040+lane),"queue append");compare();
        }
        if(inject&&start==0){vehicle[4]^=0x40;compare();}
    }
    std::cout<<"{\"routine\":\"frontend_vehicle_preview_probe\",\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}\n";return mismatches?1:0;
}

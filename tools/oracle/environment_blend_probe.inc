if(only=="environment_blend_probe"){
    if(argc<=7||std::string(argv[7])=="-")throw std::runtime_error("environment oracle requires pinned D3DX29");
    constexpr unsigned base=0x3a800000;
    map_at(base,0x10000,PROT_READ|PROT_WRITE|PROT_EXEC);map_original_arithmetic_bridge(file);
    init_matrix_oracle_globals();wire_d3dx_imports(map_d3dx29(argv[7]));init_spline_oracle();init_world_oracle();
    if(mprotect(reinterpret_cast<void*>(0x899000),8192,PROT_READ|PROT_WRITE))throw std::runtime_error("environment light pages");
    unsigned cases=0,mismatches=0;
    const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
    auto compare=[&](const auto& native,unsigned address,const char* what){
        ++cases;if(!std::memcmp(native.data(),reinterpret_cast<void*>(address),native.size()))return;
        ++mismatches;if(mismatches<9)for(unsigned i=0;i<native.size();++i)if(native[i]!=*reinterpret_cast<std::uint8_t*>(address+i)){
            unsigned n;std::memcpy(&n,native.data()+(i&~3u),4);
            std::cerr<<what<<" case="<<cases<<" offset="<<std::hex<<i<<" PC="<<*reinterpret_cast<unsigned*>(address+(i&~3u))<<" native="<<n<<std::dec<<"\n";break;}
    };
    for(unsigned pattern=0;pattern<32;++pattern)for(unsigned lane=0;lane<3;++lane){
        outrun::testing::CourseWorldFixture world;outrun::testing::configure_world_boundary(world,0,0);world.rebuild_grids();
        auto wb=world.bytes();auto transform=wb.sub(0x4400,64);PcMatrixStack temp{transform,0,0,1};
        pc_matrix_rotate_y(temp,(int(pattern)-16)*0.17f);
        WorldBinding binding(world);auto matrices=world.matrix();
        std::array<std::uint8_t,0x400> environment{};
        std::array<std::uint8_t,0x1e0+16> sun{};std::array<std::uint8_t,0x54+16> fog{};
        std::array<std::array<std::uint8_t,0xb0>,3> data{};
        for(unsigned i=0;i<environment.size();++i)environment[i]=std::uint8_t(i+pattern);
        for(unsigned i=0;i<sun.size();++i)sun[i]=std::uint8_t(i*7+pattern);
        for(unsigned i=0;i<fog.size();++i)fog[i]=std::uint8_t(i*9+pattern);
        Bytes env(environment.data(),environment.size()),live_sun(sun.data(),sun.size()),live_fog(fog.data(),fog.size());
        for(unsigned k=0;k<0x1e0/4;++k)live_sun.putf(k*4,(int(k%17)-8)*0.07f);
        for(unsigned slot=0;slot<3;++slot){
            for(unsigned k=0;k<3;++k)live_fog.putf(slot*28+8+k*4,(int(pattern)-int(k))*0.4f);
            Bytes p(data[slot].data(),data[slot].size());
            for(unsigned k=0;k<data[slot].size()/4;++k)p.putf(k*4,(int(k%13)-6)*0.13f+slot);
            p.putf(0x60,(int(pattern)-16)*12.31f);p.putf(0x64,(int(pattern)-7)*19.77f);
            for(unsigned k=0;k<3;++k)p.put8(0x48+k,std::uint8_t(pattern*13+slot*79+k*31));
            // Color bytes are floats in a sun record: retain bounded values.
            if(lane==1)for(unsigned k=0;k<3;++k)p.putf(0x48+k*4,(k+slot)*0.29f);
            std::memcpy(reinterpret_cast<void*>(base+0x2000+slot*0x200),data[slot].data(),data[slot].size());
        }
        std::array<Bytes,3> records{Bytes(nullptr,0),Bytes(nullptr,0),Bytes(nullptr,0)};
        for(unsigned k=0;k<3;++k){bool present=bool(pattern&(1u<<k));if(present)records[k]=Bytes(data[k].data(),data[k].size());
            *reinterpret_cast<unsigned*>(base+0x1000+k*4)=present?base+0x2000+k*0x200:0;}
        env.putf(0x2d8,30);env.put16(0x334,0);env.put32(0x2c8,0);
        std::memcpy(reinterpret_cast<void*>(0x7d2600),environment.data(),environment.size());
        std::memcpy(reinterpret_cast<void*>(0x899b98),sun.data(),sun.size());std::memcpy(reinterpret_cast<void*>(0x7d3a10),fog.data(),fog.size());
        for(unsigned frame=0;frame<40;++frame){
            const unsigned phase=frame==0?0:frame==39?1:2;
            const auto time=std::int16_t(int(frame)-3);
            env.put32(0x2c8,phase);env.put16(0x334,std::uint16_t(time));
            *reinterpret_cast<unsigned*>(0x7d28c8)=phase;*reinterpret_cast<std::int16_t*>(0x7d2934)=time;
            PcEnvironmentBlendContext c{phase,time,30,env.sub(0xd0,0x1e0),env.sub(0x2e0,0x54),live_sun,live_fog,transform,matrices};
            prepare(0x44ab10);auto* stack=reinterpret_cast<unsigned*>(S);stack[0]=base+0x1000;stack[1]=lane;run_original32();
            course_environment_blend_44ab10(records,lane,c);
            compare(environment,0x7d2600,"environment retained state");compare(sun,0x899b98,"live sun");compare(fog,0x7d3a10,"live fog");
            world.save(matrices,world.prediction());binding.capture_globals();compare(world.image,WorldArena,"matrices");
        }
    }
    for(unsigned pattern=0;pattern<64;++pattern){
        std::array<std::uint8_t,0x110> camera{};
        std::array<std::uint8_t,24> nearest{};
        std::array<std::uint8_t,0x3c0+16> lights{};
        std::array<std::uint8_t,33*0x2c> list{};
        Bytes cam(camera.data(),camera.size()),near(nearest.data(),nearest.size()),light(lights.data(),lights.size()),source(list.data(),list.size());
        for(unsigned i=0;i<lights.size();++i)lights[i]=std::uint8_t(i*13+pattern);
        for(unsigned i=0;i<nearest.size();++i)nearest[i]=std::uint8_t(i*7+pattern);
        for(unsigned index=0;index<32;++index){auto rec=source.sub(index*0x2c,0x2c);
            for(unsigned k=0;k<3;++k){rec.putf(k*4,(index+k)*.032f);rec.putf(0x10+k*4,(int((index*23+k*53+pattern*71)%777)-388)*1.23f);rec.putf(0x20+k*4,(int((index+k)%5)-2)*.25f);}
            rec.putf(0xc,index%7==pattern%7?-100.f:32.f);rec.putf(0x1c,pattern*7.f);
        }
        source.putf(32*0x2c+0xc,-999.9f);
        std::memcpy(reinterpret_cast<void*>(base+0x4000),list.data(),list.size());
        std::memcpy(reinterpret_cast<void*>(0x7d2d58),nearest.data(),nearest.size());
        std::memcpy(reinterpret_cast<void*>(0x899d78),lights.data(),lights.size());
        *reinterpret_cast<unsigned*>(0x79f574)=base+0x3000;
        for(unsigned frame=0;frame<32;++frame){
            for(unsigned k=0;k<3;++k){cam.putf(0xf8+k*4,(int(frame)-16)*(int(k)-1)*7.93f);cam.putf(0x104+k*4,cam.f32(0xf8+k*4)+(int((pattern+k)%5)-2)*4.3f);}
            const bool present=frame!=31&&pattern!=0;
            *reinterpret_cast<unsigned*>(0x7d26b4)=present?base+0x4000:0;
            std::memcpy(reinterpret_cast<void*>(base+0x3000),camera.data(),camera.size());
            prepare(0x44a1d0);run_original32();
            course_environment_lights_44a1d0(present?source:Bytes(nullptr,0),cam,near,light);
            if(inject&&pattern==63&&frame==31)lights[0]^=1;
            compare(nearest,0x7d2d58,"nearest retained state");compare(lights,0x899d78,"local light state");
            compare(list,base+0x4000,"source unchanged");compare(camera,base+0x3000,"camera unchanged");
        }
    }
    // Phase-3 course progression and the complete 44A8DF frame sequence:
    // original 4517D0 -> 449F50 (protected 799D18 bridge) -> 44A000 (protected
    // 44C640 bridge), 44B020/44A520/43E570 over the primary fixture course.
    unsigned progression_frames=0,prepared_records=0,interpolated_frames=0;
    for(unsigned pattern=0;pattern<48;++pattern){
        auto world=outrun::testing::make_course_world_fixture(256u+pattern*5u,5);
        WorldBinding binding(world);auto matrices=world.matrix();
        const auto tables=world.tables();const auto& primary=tables.courses[0];
        constexpr unsigned lists_at=0x8000,car_at=0x5000,chain_at=0x6000;
        std::mt19937 rng(0x4f523044u^(pattern*1664525u));
        auto real=[&](float lo,float hi){return lo+(hi-lo)*(float(rng()&0xffffu)/65535.f);};
        std::vector<std::uint8_t> arena(0x8000);Bytes la(arena.data(),arena.size());
        for(auto& b:arena)b=std::uint8_t(rng());
        std::array<std::uint32_t,6> list_offset{};unsigned cursor=0;
        for(unsigned list=0;list<6;++list){
            const bool sun=list>=3;list_offset[list]=cursor;
            const unsigned count=1u+rng()%5u;
            for(unsigned r=0;r<count;++r){
                auto e=la.sub(cursor,0xb0);cursor+=0xb0;
                if(rng()%7u==0u){e.put16(0,0xfffe);continue;}
                const std::uint16_t first=std::uint16_t(rng()%3u);
                const std::uint16_t last=std::uint16_t(first+(rng()%3u==0u?0u:rng()%3u));
                e.put16(0,first);e.put16(0x10,last);
                for(unsigned k=0x20;k<0xb0;k+=4)e.putf(k,real(-2,2));
                if(rng()%4u)e.put32(8,0x7f7fffffu); // 44A940 initialization marker
                else{for(unsigned k=0;k<3;++k){e.putf(4+k*4,real(-10,40));e.putf(0x24+k*4,real(-1,1));}e.putf(0x20,real(0,0.2f));}
                if(!sun){for(unsigned k=0;k<3;++k){e.put8(0x48+k,std::uint8_t(rng()));e.put8(0x64+k,std::uint8_t(rng()));}}
                else{e.putf(0x60,real(-80,80));e.putf(0x64,real(-180,180));e.putf(0xa0,real(-80,80));e.putf(0xa4,real(-180,180));}
            }
            la.put16(cursor,0xffff);cursor+=0xb0;
        }
        std::vector<std::uint8_t> native_arena=arena;
        std::memcpy(reinterpret_cast<void*>(base+lists_at),arena.data(),arena.size());
        auto native_list=[&](unsigned list){return Bytes(native_arena.data()+list_offset[list],native_arena.size()-list_offset[list]);};
        const unsigned absent=rng()%8u; // bit k: fog/sun slot k absent
        PcEnvironmentFrame frame{};
        for(unsigned slot=0;slot<3;++slot){
            const bool gone=(absent>>slot)&1u;
            *reinterpret_cast<unsigned*>(0x7d3a00+slot*4)=gone?0:base+lists_at+list_offset[slot];
            *reinterpret_cast<unsigned*>(0x7d26a8+slot*4)=gone?0:base+lists_at+list_offset[3+slot];
            frame.fog_lists[slot]=gone?Bytes(nullptr,0):native_list(slot);
            frame.sun_lists[slot]=gone?Bytes(nullptr,0):native_list(3+slot);
        }
        std::array<std::uint8_t,17*0x2c> light_list{};Bytes ll(light_list.data(),light_list.size());
        for(unsigned index=0;index<16;++index){auto rec=ll.sub(index*0x2c,0x2c);
            for(unsigned k=0;k<3;++k){rec.putf(k*4,real(0,1));rec.putf(0x10+k*4,real(-60,60));rec.putf(0x20+k*4,real(-1,1));}
            rec.putf(0xc,index%5==pattern%5?-100.f:real(1,50));rec.putf(0x1c,real(0,40));}
        ll.putf(16*0x2c+0xc,-999.9f);
        std::memcpy(reinterpret_cast<void*>(base+0x4000),light_list.data(),light_list.size());
        *reinterpret_cast<unsigned*>(0x7d26b4)=pattern%11u?base+0x4000:0;
        frame.light_list=pattern%11u?ll:Bytes(nullptr,0);
        std::array<std::uint8_t,0x70> car{};Bytes cv(car.data(),car.size());
        for(unsigned i=0;i<car.size();++i)car[i]=std::uint8_t(rng());
        *reinterpret_cast<unsigned*>(0x799d18)=base+car_at;
        std::array<std::uint8_t,0x110> camera{};Bytes cam(camera.data(),camera.size());
        *reinterpret_cast<unsigned*>(0x79f574)=base+0x3000;
        // 44C610/44C640 course record chain [[[7D3188]+14]+58].
        std::array<std::uint8_t,4> transition_record{};Bytes tr(transition_record.data(),4);
        tr.put16(0,std::uint16_t(rng()%3u));tr.put8(2,std::uint8_t(pattern%6u==5u?0xffu:rng()%2u));tr.put8(3,0x5a);
        const bool record_present=pattern%3u!=0u;
        const std::uint8_t mode=std::uint8_t(pattern%4u==0u?1u:(2u|(rng()&0xfcu)));
        *reinterpret_cast<std::uint8_t*>(0x79fcce)=mode;*reinterpret_cast<unsigned*>(0x7d3188)=base+chain_at;
        *reinterpret_cast<unsigned*>(base+chain_at+0x14)=base+chain_at+0x100;
        *reinterpret_cast<unsigned*>(base+chain_at+0x100+0x58)=record_present?base+chain_at+0x200:0;
        std::memcpy(reinterpret_cast<void*>(base+chain_at+0x200),transition_record.data(),4);
        PcEnvironmentTransition transition{mode,record_present?tr:Bytes(nullptr,0)};
        std::array<std::uint8_t,0x400> environment{};Bytes env(environment.data(),environment.size());
        for(unsigned i=0;i<environment.size();++i)environment[i]=std::uint8_t(i*3+pattern);
        std::array<std::uint8_t,0x54> fog{};std::array<std::uint8_t,9*0xa0> lights{};std::array<std::uint8_t,24> nearest{};
        for(unsigned i=0;i<fog.size();++i)fog[i]=std::uint8_t(i*5+pattern);
        Bytes lf(fog.data(),fog.size()),lsn(lights.data(),lights.size());
        for(unsigned k=0;k<lights.size()/4;++k)lsn.putf(k*4,(int(k%19)-9)*0.05f);
        for(unsigned slot=0;slot<3;++slot)for(unsigned k=0;k<3;++k)lf.putf(slot*28+8+k*4,real(-5,5));
        for(unsigned k=0;k<6;++k)env.put32(0x2b0+k*4,rng()%3u==0u?1u:0u);
        const unsigned start_phase=pattern%4u==1u?0u:3u;
        env.put32(0x2c8,start_phase);env.put16(0x334,std::uint16_t(rng()%5u));env.putf(0x2d8,real(1,90));
        // 7D26A8 sun list roots and 7D26B4 light root live inside this block.
        for(unsigned k=0;k<4;++k)env.put32(0xa8+k*4,*reinterpret_cast<unsigned*>(0x7d26a8+k*4));
        std::memcpy(reinterpret_cast<void*>(0x7d2600),environment.data(),environment.size());
        std::memcpy(reinterpret_cast<void*>(0x7d3a10),fog.data(),fog.size());
        std::memcpy(reinterpret_cast<void*>(0x899b98),lights.data(),lights.size());
        std::memcpy(reinterpret_cast<void*>(0x7d2d58),nearest.data(),nearest.size());
        frame.flags_7d28b0=env.sub(0x2b0,24);frame.nearest_7d2d58=Bytes(nearest.data(),nearest.size());
        frame.lights_899d78=lsn.sub(3*0xa0,6*0xa0);frame.vehicle=cv;frame.camera=cam;
        for(unsigned step=0;step<150;++step){
            const float along=float(step)/150.f;
            cv.putf(0x14,real(-2,32));cv.putf(0x18,real(-1,3));cv.putf(0x1c,-along*48.f+real(-3,3));
            cv.put32(0x5c,rng()%13u==0u?1u:0u);
            cv.put16(0x64,std::uint16_t(std::int16_t(int(along*4.f)-(step%37u==0u?5:0))));
            for(unsigned k=0;k<3;++k){cam.putf(0xf8+k*4,real(-40,40));cam.putf(0x104+k*4,cam.f32(0xf8+k*4)+real(-5,5));}
            if(step%29u==7u)for(unsigned k=0;k<6;++k)env.put32(0x2b0+k*4,rng()&1u);
            std::memcpy(reinterpret_cast<void*>(0x7d28b0),environment.data()+0x2b0,24);
            std::memcpy(reinterpret_cast<void*>(base+car_at),car.data(),car.size());
            std::memcpy(reinterpret_cast<void*>(base+0x3000),camera.data(),camera.size());
            for(unsigned entry:{0x4517d0u,0x449f50u,0x44a000u}){prepare(entry);run_original32();}
            {
                WorldNativeControl control(static_cast<unsigned short>(x87_control));
                PcEnvironmentBlendContext c{env.u32(0x2c8),env.i16(0x334),env.f32(0x2d8),env.sub(0xd0,0x1e0),env.sub(0x2e0,0x54),
                    lsn.sub(0,3*0xa0),lf,tables.transforms[0],matrices};
                if(c.phase_7d28c8==3&&cv.u32(0x5c)==0)++progression_frames;
                if(c.phase_7d28c8==2)++interpolated_frames;
                try{course_environment_update_44a8df(frame,primary,transition,c);}
                catch(const std::exception& e){++cases;++mismatches;if(mismatches<9)std::cerr<<"progression native rejected pattern="<<pattern<<" step="<<step<<": "<<e.what()<<"\n";}
                env.put32(0x2c8,c.phase_7d28c8);env.put16(0x334,std::uint16_t(c.time_7d2934));env.putf(0x2d8,c.duration_7d28d8);
            }
            if(step==149)for(unsigned at=0;at+0xb0<=cursor;at+=0xb0)if(la.u32(at+8)==0x7f7fffffu&&Bytes(native_arena.data(),native_arena.size()).u32(at+8)!=0x7f7fffffu)++prepared_records;
            if(inject&&pattern==47&&step==149)native_arena[list_offset[3]+0x14]^=1;
            compare(environment,0x7d2600,"progression retained state");compare(fog,0x7d3a10,"progression fog");
            compare(lights,0x899b98,"progression lights");compare(nearest,0x7d2d58,"progression nearest");
            compare(native_arena,base+lists_at,"progression prepared records");compare(car,base+car_at,"vehicle unchanged");
            world.save(matrices,world.prediction());binding.capture_globals();compare(world.image,WorldArena,"progression matrices");
        }
    }
    std::cerr<<"progression coverage: frames="<<progression_frames<<" interpolation_frames="<<interpolated_frames<<" records_prepared_by_44a520="<<prepared_records<<'\n';
    // 43E570 / 43E6C0 (out, request, hint) called directly over the fixture courses.
    unsigned run_queries=0,run_found=0;
    for(unsigned pattern=0;pattern<48;++pattern){
        auto world=outrun::testing::make_course_world_fixture(512u+pattern*7u,5);
        WorldBinding binding(world);
        const auto tables=world.tables();
        std::mt19937 rng(0x43e6c0u^(pattern*2654435761u));
        constexpr unsigned req_at=0x5800,out_at=0x5900;
        for(unsigned step=0;step<64;++step){
            const std::uint32_t entry=step&1u?0x43e6c0u:0x43e570u;
            std::array<std::uint8_t,12> request{};std::array<std::uint8_t,0x58> out{};
            Bytes rq(request.data(),request.size()),ov(out.data(),out.size());
            const std::uint32_t kind=rng()%4u==0u?1u:0u;
            rq.put32(0,kind);rq.put32(4,rng()%6u);rq.put16(8,std::uint16_t(rng()%3u==0u?rng()%40u:rng()%8u));rq.put16(10,std::uint16_t(rng()));
            for(auto& b:out)b=std::uint8_t(rng());
            ov.put32(0,rng()%3u==0u?0xffffffffu:rng()%8u);ov.put32(4,rng()%3u==0u?0xffffffffu:rng()%8u);
            for(unsigned k=0x24;k<0x54;k+=4)ov.putf(k,float(int(rng()%2001u)-1000)*0.05f);
            const std::int32_t hint=rng()%3u==0u?-1:std::int32_t(rng()%8u);
            std::memcpy(reinterpret_cast<void*>(base+req_at),request.data(),request.size());
            std::memcpy(reinterpret_cast<void*>(base+out_at),out.data(),out.size());
            prepare(entry);auto* stack=reinterpret_cast<unsigned*>(S);stack[0]=base+out_at;stack[1]=base+req_at;stack[2]=std::uint32_t(hint);run_original32();
            const std::uint32_t pc_ret=guest_call.out_eax;
            std::uint32_t native_ret=0;
            try{
                WorldNativeControl control(static_cast<unsigned short>(x87_control));
                native_ret=(entry==0x43e570u?course_run_geometry_43e570(tables.courses[kind],rq,ov,hint):course_run_length_43e6c0(tables.courses[kind],rq,ov,hint))?1u:0u;
            }catch(const std::exception& e){++cases;++mismatches;if(mismatches<9)std::cerr<<"run query native rejected pattern="<<pattern<<" step="<<step<<": "<<e.what()<<"\n";continue;}
            ++run_queries;if(pc_ret)++run_found;
            if(inject&&pattern==40&&step==33)out[0x54]^=1;
            ++cases;if(pc_ret!=native_ret){++mismatches;if(mismatches<9)std::cerr<<"run query result "<<std::hex<<entry<<" PC="<<pc_ret<<" native="<<native_ret<<std::dec<<"\n";}
            compare(out,base+out_at,entry==0x43e570u?"43E570 output":"43E6C0 output");compare(request,base+req_at,"run query request");
        }
    }
    std::cerr<<"run query coverage: queries="<<run_queries<<" found="<<run_found<<'\n';
    std::cout<<"{\"routine\":\"environment_blend_probe\",\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}\n";return mismatches?1:0;
}

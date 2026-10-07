// Direct original PC ground collision and retained vehicle chain.
std::ofstream ground_golden;std::uint32_t ground_golden_count=0;
void ground_u32(std::uint32_t n){char b[4];for(unsigned k=0;k<4;++k)b[k]=char(n>>(8*k));ground_golden.write(b,4);}
void begin_ground_golden(const char* path){
    ground_golden.open(path,std::ios::binary|std::ios::trunc);if(!ground_golden)throw std::runtime_error("ground corpus open");
    ground_golden.write("OR2G016\0",8);ground_u32(1);ground_u32(x87_control);ground_u32(0);
}
void finish_ground_golden(){if(ground_golden.is_open()){
    ground_golden.seekp(16);ground_u32(ground_golden_count);ground_golden.flush();
    if(!ground_golden)throw std::runtime_error("ground corpus finalization");
    ground_golden.close();
}}
void ground_write(const void* src,std::size_t n){ground_golden.write(static_cast<const char*>(src),static_cast<std::streamsize>(n));if(!ground_golden)throw std::runtime_error("ground corpus write");}
std::map<std::string,std::uint64_t> ground_coverage;
bool ground_enabled(const std::string& only){return only=="all"||only=="ground_all"||only=="calc_ground_coli_face"||only=="ground_suspension_chain";}
void run_ground_cases(unsigned i,const std::string& only){
    if(!ground_enabled(only))return;
    WorldNativeControl control(static_cast<unsigned short>(x87_control));
    for(unsigned which=0;which<2;++which){
        const std::string name=which?"ground_suspension_chain":"calc_ground_coli_face";
        if(only!="all"&&only!="ground_all"&&only!=name)continue;
        auto f=outrun::testing::make_ground_fixture(i,which==0u);WorldBinding bind(f.world);
        std::memcpy(reinterpret_cast<void*>(E),f.car.data(),event_size);
        std::memcpy(reinterpret_cast<void*>(W),f.car.data()+event_size,work_size);
        std::memcpy(reinterpret_cast<void*>(P),f.car.data()+event_size+work_size,parameter_size);
        const std::uint32_t yaw0=*reinterpret_cast<std::uint32_t*>(0x7d3128),yaw1=*reinterpret_cast<std::uint32_t*>(0x7d317c);
        *reinterpret_cast<std::uint32_t*>(0x7d3128)=f.world.bytes().u32(0x4700);
        *reinterpret_cast<std::uint32_t*>(0x7d317c)=f.world.bytes().u32(0x4704);
        constexpr std::uint32_t entries[]={0x519500,0x518fa0,0x519170,0x4a1b70,0x4a1a90};
        const auto steps=which?10u:1u;
        const bool capture=ground_golden.is_open()&&(i<168u||i==511u||i==1023u||i==9999u||i==19999u);
        if(capture){ground_u32(which);ground_u32(i);ground_u32(steps);
            ground_write(f.world.image.data(),f.world.image.size());ground_write(f.car.data(),f.car.size());++ground_golden_count;}
        for(unsigned stage=0;stage<steps;++stage){
            const auto routine=stage%5u;
            prepare(entries[routine]);Bytes st(reinterpret_cast<void*>(S),64);st.put32(0,E);st.put32(4,W);
            run();bind.capture_globals();
            if(capture){ground_write(reinterpret_cast<void*>(WorldArena),f.world.image.size());
                ground_write(reinterpret_cast<void*>(E),event_size);ground_write(reinterpret_cast<void*>(W),work_size);ground_write(reinterpret_cast<void*>(P),parameter_size);}
            if(routine==0u){
                Bytes original_w(reinterpret_cast<void*>(W),work_size),original_e(reinterpret_cast<void*>(E),event_size);
                unsigned hitmask=0;for(unsigned k=0;k<4;++k)if(!(original_w.u32(embedded_wheel_offsets[k]+0x14)&1u))hitmask|=1u<<k;
                ++ground_coverage["query_wheel_hitmask_"+std::to_string(hitmask)];
                ++ground_coverage["selected_type_"+std::to_string(original_e.u32(0x5c))];
                ++ground_coverage["special_equals_current_"+std::to_string(original_e.u32(0x230)==original_e.u32(0x1c0))];
                const auto nx=original_w.f32(0x628),ny=original_w.f32(0x62c),nz=original_w.f32(0x630);
                ++ground_coverage[(nx==0&&ny==0&&nz==0)?"zero_normal":(nx!=0||nz!=0)?"sloped_normal":"vertical_normal"];
            }
            try{outrun::testing::run_ground_native(f,routine);}
            catch(const std::exception& ex){throw std::runtime_error(name+" case="+std::to_string(i)+" stage="+std::to_string(stage)+": "+ex.what());}
            const auto tag=name+(which?"_"+std::to_string(stage+1u):"");
            compare_world_bytes(tag+"_event",reinterpret_cast<void*>(E),f.car.data(),event_size);
            compare_world_bytes(tag+"_work",reinterpret_cast<void*>(W),f.car.data()+event_size,work_size);
            compare_world_bytes(tag+"_parameters",reinterpret_cast<void*>(P),f.car.data()+event_size+work_size,parameter_size);
            bind.compare(tag,f.world);
            compare_u32(tag+"_yaw_primary",*reinterpret_cast<std::uint32_t*>(0x7d3128),f.world.bytes().u32(0x4700));
            compare_u32(tag+"_yaw_secondary",*reinterpret_cast<std::uint32_t*>(0x7d317c),f.world.bytes().u32(0x4704));
        }
        *reinterpret_cast<std::uint32_t*>(0x7d3128)=yaw0;*reinterpret_cast<std::uint32_t*>(0x7d317c)=yaw1;
    }
}

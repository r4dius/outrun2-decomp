if(only=="race_events_probe"){
    // Small race events: 4AF420/4AF4A0 game clock, 49ACE0/49ACF0 frame
    // counter, 4866D0/4874A0 camera override (82E7D8 == 0 path).
    using namespace outrun::platform;
    for(std::uint32_t page:{0x842000u,0x836000u,0x82e000u})
        if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("race events globals");
    const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
    unsigned cases{},mismatches{};
    auto check=[&](bool ok,const char* what,unsigned i){++cases;if(!ok){++mismatches;if(mismatches<9)std::cerr<<what<<" case "<<i<<"\n";}};
    auto g32=[](std::uint32_t a){return *reinterpret_cast<const std::uint32_t*>(a);};
    auto bits=[](float f){std::uint32_t u;std::memcpy(&u,&f,4);return u;};
    std::mt19937 rng(0x4af420u);
    for(unsigned i=0;i<20000;++i){
        RaceGameClock c{};
        if(i%100u==0u){for(std::uint32_t a=0x84210cu;a<=0x84212cu;a+=4)*reinterpret_cast<std::uint32_t*>(a)=rng();
            prepare(0x4af420);run_original32();race_clock_init_4af420(c);}
        else{
            const float t=i%7u?float(int(rng()%200000u))*0.37f:float(std::int32_t(rng()));
            const float st=i%5u?float(rng()%400u)*0.01f:-float(rng()%3u);
            const float nx=i%3u?-1.f:float(rng()%300u)*0.01f;
            c.time_842110=t;c.step_842114=st;c.next_step_842118=nx;
            std::memcpy(reinterpret_cast<void*>(0x842110u),&t,4);std::memcpy(reinterpret_cast<void*>(0x842114u),&st,4);
            std::memcpy(reinterpret_cast<void*>(0x842118u),&nx,4);
            prepare(0x4af4a0);run_original32();race_clock_control_4af4a0(c);
        }
        if(inject&&i==19999)c.frames_84211c^=1;
        check(g32(0x842110u)==bits(c.time_842110)&&g32(0x842114u)==bits(c.step_842114)&&g32(0x842118u)==bits(c.next_step_842118)&&
              g32(0x84211cu)==std::uint32_t(c.frames_84211c),"4AF420/4AF4A0 clock",i);
        if(i%100u==0u)check(g32(0x84210cu)==0u&&g32(0x842120u)==bits(c.f842120)&&g32(0x842124u)==bits(c.f842124)&&g32(0x842128u)==0u&&g32(0x84212cu)==0u,"4AF420 tail",i);
    }
    for(unsigned i=0;i<200;++i){
        std::uint32_t n=rng();*reinterpret_cast<std::uint32_t*>(0x8367f4u)=n;
        if(i%2u){prepare(0x49acf0);run_original32();race_frame_counter_control_49acf0(n);}
        else{prepare(0x49ace0);run_original32();race_frame_counter_init_49ace0(n);}
        check(g32(0x8367f4u)==n,"49ACE0/49ACF0",i);
    }
    for(unsigned i=0;i<200;++i){
        for(std::uint32_t a=0x82e7c0u;a<=0x82e7ecu;a+=4)*reinterpret_cast<std::uint32_t*>(a)=rng();
        RaceCameraOverride o{};
        prepare(0x4866d0);run_original32();race_camera_override_init_4866d0(o);
        prepare(0x4874a0);run_original32();const auto missing=race_camera_override_control_4874a0(o);
        check(missing==0u,"4874A0 path",i);
        check(g32(0x82e7c0u)==o.w82e7c0&&std::int16_t(g32(0x82e7c4u))==o.w82e7c4&&g32(0x82e7c8u)==o.w82e7c8&&
              std::int16_t(g32(0x82e7ccu))==o.w82e7cc&&g32(0x82e7d0u)==o.w82e7d0&&std::uint8_t(g32(0x82e7d4u))==o.scene_82e7d4&&
              std::int32_t(g32(0x82e7d8u))==o.override_82e7d8&&std::int16_t(g32(0x82e7dcu))==o.w82e7dc&&g32(0x82e7e0u)==bits(o.time_82e7e0)&&
              g32(0x82e7e4u)==o.w82e7e4&&g32(0x82e7ecu)==o.steering_82e7ec,"4866D0 globals",i);
    }
    std::cout<<"{\"routine\":\"race_events_probe\",\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}"<<'\n';return mismatches?1:0;
}

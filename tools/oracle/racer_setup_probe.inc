// Included inside the compare_pc dispatcher; uses its private x86 harness.
if(only=="racer_setup_probe"){
    // Original 47CF40 (with 4774B0, 476550, 496300, 4957F0, 495B20, 4961F0,
    // 4B02A0, 505360, 505390, 582194 and the CRT sprintf) against
    // racer_setup_47cf40. The retail Races.bin is relocated at a guest base
    // exactly as 4F12A0 does; the native reads the same addresses through
    // PcAddressView. malloc is a bump allocator, 44B820 and 465EB0 are stubs.
    using namespace outrun::platform;
    constexpr unsigned base=0x3a700000,blob=base+0x20000,heap=base+0x100000,trace=base+0x8000,misc=base+0xc000;
    map_at(base,0x200000,PROT_READ|PROT_WRITE|PROT_EXEC);map_original_arithmetic_bridge(file);
    for(unsigned page:{0x80f000u,0x6a4000u,0x64d000u,0x64e000u,0x680000u,0x803000u,0x804000u,0x836000u,0x851000u,0x686000u,0x799000u,0x780000u})
        if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("racer globals");
    auto stub=[](unsigned pc,unsigned value,unsigned short pop=0){std::array<std::uint8_t,8> code{0xb8,0,0,0,0,0xc2,0,0};
        std::memcpy(code.data()+1,&value,4);std::memcpy(code.data()+6,&pop,2);r060_raw_write(pc,code.data(),code.size());};
    unsigned stub_index{};
    auto install=[&](unsigned pc,const std::vector<std::uint8_t>& code){const unsigned dst=base+0x10000+512*stub_index++;
        r060_raw_write(dst,code.data(),code.size());std::array<std::uint8_t,5> jump{{0xe9,0,0,0,0}};
        const auto delta=dst-pc-5;std::memcpy(jump.data()+1,&delta,4);r060_raw_write(pc,jump.data(),jump.size());};
    auto imm=[](std::vector<std::uint8_t>& c,unsigned v){for(unsigned j=0;j<4;++j)c.push_back(std::uint8_t(v>>(8*j)));};
    // malloc(n): record n at trace[16+4k], return [misc] and advance it by (n+15)&~15.
    {std::vector<std::uint8_t> c{0x8b,0x15};imm(c,trace);                                   // mov edx,[trace]
     c.insert(c.end(),{0x8b,0x44,0x24,0x04,0x89,0x04,0x95});imm(c,trace+16);                   // mov eax,[esp+4]; mov [edx*4+trace+16],eax
     c.insert(c.end(),{0xff,0x05});imm(c,trace);                                              // inc [trace]
     c.insert(c.end(),{0x83,0xc0,0x0f,0x83,0xe0,0xf0});                                       // add eax,15; and eax,-16
     c.insert(c.end(),{0x8b,0x0d});imm(c,misc);c.insert(c.end(),{0x01,0x05});imm(c,misc);      // mov ecx,[misc]; add [misc],eax
     c.insert(c.end(),{0x89,0xc8,0xc3});install(0x580253,c);}                                 // mov eax,ecx; ret
    std::uint32_t length_result{};
    stub(0x44b820,0);                                                                      // patched per case below
    const unsigned text=misc+0x100;std::strcpy(reinterpret_cast<char*>(text),"RIVAL");
    stub(0x465eb0,text);
    RaceAssetPack races;std::string error;
    if(!load_race_asset_pack_file("../OutRun2006 Coast 2 Coast (FXT)/Scripts/bin/Races.bin",races,&error))throw std::runtime_error(error);
    std::vector<std::uint8_t> relocated;
    if(!pc_relocate_blob(races.bytes,blob,relocated))throw std::runtime_error("races relocation");
    std::memcpy(reinterpret_cast<void*>(blob),relocated.data(),relocated.size());
    PcAddressView view;view.add(blob,relocated.data(),relocated.size());view.add_exe();
    unsigned cases{},mismatches{};const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
    auto check=[&](bool ok,const char* label){++cases;if(!ok){++mismatches;if(mismatches<15)std::cerr<<"racer setup "<<cases<<" "<<label<<"\n";}};
    auto g32=[](unsigned a){return *reinterpret_cast<std::uint32_t*>(a);};
    auto p32=[](unsigned a,std::uint32_t v){*reinterpret_cast<std::uint32_t*>(a)=v;};
    const unsigned car=misc+0x400,mission=misc+0x800;
    std::uint32_t seed=11;auto rnd=[&]{seed=seed*1103515245u+12345u;return seed>>8;};
    std::vector<std::uint32_t> configs{0u};
    for(unsigned i=0;i<races.race_count;++i){const auto rec=blob+unsigned(races.races_offset)+i*0x44;
        const auto cfg=g32(rec+0x1c);if(cfg)configs.push_back(cfg);}
    std::sort(configs.begin(),configs.end());configs.erase(std::unique(configs.begin(),configs.end()),configs.end());
    std::cerr<<"racer configs "<<configs.size()<<"\n";
    unsigned specials{},boss{};
    for(std::size_t ci=0;ci<configs.size();++ci)for(unsigned variant=0;variant<8;++variant){
        const auto cfg=configs[ci];const bool flag=variant==7;
        RacerSetupState st;RacerSetupServices s;s.view=&view;
        s.car_present=true;s.car_byte11=std::int8_t(variant&1?15:3+variant);s.car_word25e=std::uint16_t(rnd());
        s.network_686258=rnd()&0xfff;s.selection_836374=(variant&2)!=0;s.mission_type_495b20=variant&4?6u:1u;
        float seconds=float(rnd()%500)+0.25f;s.time_4961f0=std::int32_t(seconds*60.0f);
        const unsigned presets=variant%4;
        for(unsigned i=0;i<presets;++i)s.preset_8361c0.push_back({std::uint8_t(rnd()%20),std::uint8_t(rnd()%8),std::uint8_t(rnd()),std::uint8_t(rnd())});
        for(auto& f:st.rival_8514a4)f=float(rnd()%1000)/1000.0f;
        s.text_3d7="RIVAL";
        length_result=1000u+rnd()%60000u;
        s.course_length_44b820=[&](std::int32_t,std::uint32_t& l){l=length_result;return true;};
        // Guest state.
        p32(misc,heap);p32(trace,0);std::memset(reinterpret_cast<void*>(heap),0,0x80000); // malloc contents are unspecified; both sides start from zero
        stub(0x44b820,length_result);
        std::memset(reinterpret_cast<void*>(car),0,0x300);*reinterpret_cast<std::int8_t*>(car+0x11)=s.car_byte11;
        *reinterpret_cast<std::uint16_t*>(car+0x25e)=s.car_word25e;p32(0x799d18,car);p32(0x686258,s.network_686258);
        *reinterpret_cast<std::uint8_t*>(0x836374)=s.selection_836374;p32(0x83637c,mission);p32(mission+0x20,s.mission_type_495b20);
        std::memcpy(reinterpret_cast<void*>(mission+0x28),&seconds,4);p32(0x780258,1);
        p32(0x83639c,presets);for(unsigned i=0;i<presets;++i)std::memcpy(reinterpret_cast<void*>(0x8361c0+4*i),s.preset_8361c0[i].data(),4);
        std::memcpy(reinterpret_cast<void*>(0x8514a4),st.rival_8514a4.data(),24);
        p32(0x6a4e2c,0x1234);p32(0x80fb28,7);
        if(std::getenv("OR2_RACER_TRACE"))std::cerr<<"case "<<ci<<"/"<<variant<<" cfg "<<std::hex<<cfg<<std::dec<<char(10);
        prepare(0x47cf40);{Bytes a(reinterpret_cast<void*>(S),8);a.put32(0,cfg);a.put32(4,flag);}run_original32();
        if(!racer_setup_47cf40(st,s,cfg,flag)){check(false,"native 47CF40");std::cerr<<"  native missing "<<std::hex<<s.missing<<" cfg "<<cfg<<" count "<<g32(cfg)<<std::dec<<char(10);}
        if(inject&&ci==2&&variant==3&&!st.racers_80fb00.empty())st.racers_80fb00[0x10]^=1;
        const unsigned racers=g32(0x80fb00),count=g32(0x80fb04);
        check(count==st.count_80fb04&&g32(0x80fb0c)==st.config_80fb0c&&g32(0x80fb28)==st.special_80fb28,"80FB04 / 80FB0C / 80FB28");
        check(!std::memcmp(reinterpret_cast<void*>(racers),st.racers_80fb00.data(),st.racers_80fb00.size()),"racer records 80FB00");
        if(std::memcmp(reinterpret_cast<void*>(racers),st.racers_80fb00.data(),st.racers_80fb00.size())&&mismatches<6)
            for(unsigned k=0;k<st.racers_80fb00.size();k+=4)if(std::memcmp(reinterpret_cast<void*>(racers+k),st.racers_80fb00.data()+k,4))
                {std::cerr<<"  cfg "<<std::hex<<cfg<<" off "<<k<<" guest "<<g32(racers+k)<<" native "<<*reinterpret_cast<std::uint32_t*>(st.racers_80fb00.data()+k)<<std::dec<<"\n";if(k>0x200)break;}
        check(*reinterpret_cast<std::uint8_t*>(0x80fb08)==st.flag_80fb08&&*reinterpret_cast<std::uint8_t*>(0x80fb09)==st.flag_80fb09&&
              *reinterpret_cast<std::uint8_t*>(0x80fb18)==st.v80fb18&&*reinterpret_cast<std::uint8_t*>(0x80fb24)==st.v80fb24&&g32(0x80fb2c)==st.v80fb2c,"80FB08/09/18/24/2C");
        check(g32(0x6a4e2c)==st.seed_6a4e2c&&g32(0x64deec)==st.names_64deec&&g32(0x64def0)==st.names_64def0,"6A4E2C seed / 64DEEC names");
        check(g32(0x64e190)==std::uint32_t(st.v64e190)&&g32(0x64e194)==std::uint32_t(st.v64e194)&&g32(0x8037b8)==st.v8037b8,"64E190/94 / 8037B8");
        check(g32(0x680ad4)==st.v680ad4&&g32(0x680ad8)==st.v680ad8&&g32(0x803710)==st.length_803710&&
              *reinterpret_cast<std::int16_t*>(0x804388)==st.per_lap_804388&&g32(0x64df64)==st.profile_64df64,"680AD4/D8, 803710, 804388, 64DF64");
        // Allocation order/sizes: 80FB00, 80FB1C, 80FB30, 80FB20.
        check(g32(trace)==4&&g32(trace+16)==st.racers_80fb00.size()&&g32(trace+20)==st.table_80fb1c.size()&&
              g32(trace+24)==st.table_80fb30.size()&&g32(trace+28)==st.table_80fb20.size(),"allocation sizes");
        const unsigned t20=g32(0x80fb20);
        bool marks=true;for(unsigned i=0;i<=count;++i)marks&=*reinterpret_cast<std::uint8_t*>(t20+i*0x1c+0x10)==st.table_80fb20[i*0x1c+0x10];
        check(marks,"80FB20 +0x10 markers");
        specials+=st.special_80fb28!=0;boss+=st.flag_80fb08;
    }
    std::cout<<"{\"routine\":\"racer_setup_probe\",\"coverage\":{\"configs\":"<<configs.size()<<",\"special_racers\":"<<specials<<",\"flag_80fb08\":"<<boss
             <<"},\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}\n";return mismatches?1:0;
}

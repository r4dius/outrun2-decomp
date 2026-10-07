// Included inside the compare_pc dispatcher; uses its private x86 harness.
if(only=="mission_manager_probe"){
    // Original event-function 0x20 pieces: 495B90 init, 4964D0 destroy and
    // the variant!=4 path of 4965A0 (race record lookup, 4961F0 time, course
    // apply and traffic set-up calls) against mission_manager. The retail
    // Races.bin records are copied into guest memory; 4F12A0 (async load),
    // 4F1A90/4F1BA0 (category accessors), 4F1210 (course list count), 44D720
    // and 47CF40 are capture stubs with scripted results.
    using namespace outrun::platform;
    constexpr unsigned base=0x3a700000,rows=base+0x2000,trace=base+0x8000,config=base+0xc000;
    map_at(base,0x20000,PROT_READ|PROT_WRITE|PROT_EXEC);map_original_arithmetic_bridge(file);
    for(unsigned page:{0x836000u,0x67e000u,0x688000u,0x7c2000u,0x780000u})
        if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("mission manager globals");
    auto stub=[](unsigned pc,unsigned value,unsigned short pop=0){std::array<std::uint8_t,8> code{0xb8,0,0,0,0,0xc2,0,0};
        std::memcpy(code.data()+1,&value,4);std::memcpy(code.data()+6,&pop,2);r060_raw_write(pc,code.data(),code.size());};
    unsigned stub_index{};
    auto install=[&](unsigned pc,const std::vector<std::uint8_t>& code){const unsigned dst=base+0x10000+512*stub_index++;
        r060_raw_write(dst,code.data(),code.size());std::array<std::uint8_t,5> jump{{0xe9,0,0,0,0}};
        const auto delta=dst-pc-5;std::memcpy(jump.data()+1,&delta,4);r060_raw_write(pc,jump.data(),jump.size());};
    // Capture up to 10 stack args into trace[16+48*n], return dword at result_slot.
    auto capture=[&](unsigned pc,unsigned nargs,unsigned result_slot){std::vector<std::uint8_t> code{0x8b,0x15};
        auto imm=[&](unsigned v){for(unsigned j=0;j<4;++j)code.push_back(std::uint8_t(v>>(8*j)));};
        imm(trace);code.insert(code.end(),{0x6b,0xd2,0x30,0xc7,0x82});imm(trace+16);imm(pc);
        for(unsigned i=0;i<10;++i){if(i<nargs)code.insert(code.end(),{0x8b,0x44,0x24,std::uint8_t(4+4*i)});else code.insert(code.end(),{0x31,0xc0});
            code.insert(code.end(),{0x89,0x82});imm(trace+20+4*i);}
        code.insert(code.end(),{0xff,0x05});imm(trace);code.push_back(0xa1);imm(result_slot);code.push_back(0xc3);install(pc,code);};
    RaceAssetPack races;std::string error;
    if(!load_race_asset_pack_file("../OutRun2006 Coast 2 Coast (FXT)/Scripts/bin/Races.bin",races,&error))throw std::runtime_error(error);
    std::memcpy(reinterpret_cast<void*>(rows),races.bytes.data()+races.races_offset,races.race_count*0x44);
    stub(0x4f1a90,rows);stub(0x4f1ba0,races.race_count);stub(0x580c38,0); // CRT free of the (unallocated) 836354 buffer
    capture(0x4f12a0,4,config+0);capture(0x4f1210,2,config+4);capture(0x44d720,10,config+8);capture(0x47cf40,2,config+12);
    unsigned cases{},mismatches{};const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
    auto check=[&](bool ok,const char* label){++cases;if(!ok){++mismatches;if(mismatches<15)std::cerr<<"mission manager "<<cases<<" "<<label<<"\n";}};
    auto g32=[](unsigned a){return *reinterpret_cast<std::uint32_t*>(a);};
    auto p32=[](unsigned a,std::uint32_t v){*reinterpret_cast<std::uint32_t*>(a)=v;};
    struct Call{unsigned pc;std::array<unsigned,10> a;};
    auto sync=[&](const MissionManagerState& m,std::uint32_t key,std::uint32_t sub,std::uint32_t variant,const PcLicense& profile){
        p32(0x836350,m.loader_836350);p32(0x836354,m.buffer_836354);p32(0x836358,m.status_836358);p32(0x836378,m.v836378);
        p32(0x83637c,m.record_83637c<0?0:rows+unsigned(m.record_83637c)*0x44);p32(0x836380,m.v836380);p32(0x836384,m.stage_836384);
        p32(0x836388,m.timer_836388);p32(0x83638c,m.v83638c);p32(0x836390,m.countdown_836390);p32(0x836394,m.v836394);
        *reinterpret_cast<std::uint8_t*>(0x836398)=m.v836398;p32(0x83639c,m.v83639c);p32(0x8363a0,m.v8363a0);
        *reinterpret_cast<std::uint8_t*>(0x8363a4)=m.v8363a4;p32(0x8363a8,m.v8363a8);*reinterpret_cast<std::uint8_t*>(0x67e6ac)=m.flag_67e6ac;
        p32(0x67e6b0,m.v67e6b0);p32(0x67e6b4,m.v67e6b4);p32(0x67e6b8,std::uint32_t(m.v67e6b8));p32(0x688b3c,m.v688b3c);
        *reinterpret_cast<std::uint8_t*>(0x836374)=m.selection_836374;
        p32(0x67e6a4,key);p32(0x67e6a8,sub);p32(0x780258,variant);std::memcpy(reinterpret_cast<void*>(0x7c23e0),profile.data(),profile.size());
        std::memset(reinterpret_cast<void*>(trace),0,0x4000);};
    auto compare=[&](const MissionManagerState& m,const std::vector<Call>& calls){
        check(g32(0x836350)==m.loader_836350&&g32(0x836354)==m.buffer_836354&&g32(0x836358)==m.status_836358,"836350 loader words");
        check(g32(0x836384)==m.stage_836384&&g32(0x836388)==m.timer_836388&&g32(0x83639c)==m.v83639c,"836384 stage / 836388 time / 83639C");
        check(g32(0x83637c)==(m.record_83637c<0?0:rows+unsigned(m.record_83637c)*0x44),"83637C selected record");
        check(g32(0x836378)==m.v836378&&g32(0x836380)==m.v836380&&g32(0x83638c)==m.v83638c&&g32(0x836390)==m.countdown_836390&&
              g32(0x836394)==m.v836394&&*reinterpret_cast<std::uint8_t*>(0x836398)==m.v836398&&g32(0x8363a0)==m.v8363a0&&
              *reinterpret_cast<std::uint8_t*>(0x8363a4)==m.v8363a4&&g32(0x8363a8)==m.v8363a8,"836378..8363A8");
        check(*reinterpret_cast<std::uint8_t*>(0x67e6ac)==m.flag_67e6ac&&g32(0x67e6b0)==m.v67e6b0&&g32(0x67e6b4)==m.v67e6b4&&
              g32(0x67e6b8)==std::uint32_t(m.v67e6b8)&&g32(0x688b3c)==m.v688b3c,"67E6AC..67E6B8 / 688B3C");
        check(*reinterpret_cast<std::uint8_t*>(0x836374)==m.selection_836374,"836374");
        // Only the calls with observable arguments beyond the stubs' data are compared.
        std::vector<Call> guest;for(unsigned i=0;i<g32(trace);++i){Call c{g32(trace+16+48*i),{}};for(unsigned k=0;k<10;++k)c.a[k]=g32(trace+20+48*i+4*k);
            if(c.pc==0x44d720||c.pc==0x47cf40)guest.push_back(c);}
        check(guest.size()==calls.size(),"ordered 44D720/47CF40 calls");
        for(std::size_t i=0;i<std::min(guest.size(),calls.size());++i){
            check(guest[i].pc==calls[i].pc,"call entry");
            if(calls[i].pc==0x44d720){
                const auto& a=guest[i].a;
                check(std::string(reinterpret_cast<const char*>(std::uintptr_t(a[0])))=="Race_Courses"&&a[1]==0x6255d5&&a[2]==0x836350&&
                      a[3]==0&&a[4]==0&&a[5]==1&&a[9]==0,"44D720 fixed arguments");
                check(a[6]==calls[i].a[6]&&a[8]==calls[i].a[8],"44D720 kind-4 flag and course count");
            }else check(guest[i].a[0]==calls[i].a[0]&&guest[i].a[1]==0,"47CF40 record +1C");
        }
    };
    std::uint32_t seed=7;auto rnd=[&]{seed=seed*1103515245u+12345u;return seed>>8;};
    for(unsigned rec=0;rec<races.race_count+4;++rec)for(unsigned variant:{1u,6u,2u})for(unsigned ready:{0u,1u})for(unsigned apply:{0u,1u}){
        PcLicense profile{};for(auto& b:profile)b=std::uint8_t(rnd());
        const auto* r=races.bytes.data()+races.races_offset+std::min(rec,races.race_count-1)*0x44;
        std::uint32_t key,sub;std::memcpy(&key,r,4);std::memcpy(&sub,r+4,4);
        if(rec>=races.race_count){key=0x7777+rec;sub=rec;}                        // missing record
        MissionManagerState m;
        // init, then stage 0, then stage 3 (x2), then stage 4, then destroy.
        sync(m,key,sub,variant,profile);prepare(0x495b90);run_original32();mission_manager_init_495b90(m);compare(m,{});
        for(unsigned step=0;step<4;++step){
            std::vector<Call> calls;
            MissionManagerServices s;s.races=&races;s.profile=&profile;s.race_key_67e6a4=key;s.race_sub_67e6a8=sub;s.variant_780258=variant;
            s.apply_44d720=[&](RaceCourseSelection& sel,bool kind4){Call c{0x44d720,{}};c.a[6]=kind4;c.a[8]=sel.course_count;calls.push_back(c);return apply!=0;};
            s.traffic_47cf40=[&](std::uint32_t index){Call c{0x47cf40,{}};std::uint32_t v;std::memcpy(&v,races.bytes.data()+races.races_offset+index*0x44+0x1c,4);c.a[0]=v;calls.push_back(c);return true;};
            // The stage-0 Races load is synchronous natively; script the guest's
            // 4F12A0 accordingly (the async not-ready frame is covered by ready=0
            // only on the first step, where the native load has not run yet).
            const bool guest_ready=ready||step>0;
            sync(m,key,sub,variant,profile);p32(config+0,guest_ready);p32(config+8,apply);p32(config+12,1);
            {RaceCourseSelection sel;if(m.record_83637c>=0){const auto* rr=races.bytes.data()+races.races_offset+m.record_83637c*0x44;
                std::uint32_t k,sb;std::memcpy(&k,rr,4);std::memcpy(&sb,rr+4,4);if(race_course_select_4965a0(races,k,sb,sel))p32(config+4,sel.course_count);}}
            prepare(0x4965a0);run_original32();const bool original=(guest_call.out_eax&255)!=0;
            bool result{};
            if(guest_ready){check(mission_manager_select_4965a0(m,s,result),"native 4965A0");}
            else result=false;                                                      // PC async frame: nothing changes
            if(inject&&rec==3&&step==1)m.timer_836388^=1;
            compare(m,calls);check(result==original,"4965A0 result");
        }
        sync(m,key,sub,variant,profile);prepare(0x4964d0);run_original32();mission_manager_destroy_4964d0(m);compare(m,{});
    }
    // 44B820 on the course records that the native 44D720 builds for every
    // Races record (tokens are the EXE descriptor addresses, readable here).
    {
        if(mprotect(reinterpret_cast<void*>(0x7d3000),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("course globals");
        CourseAssetPack course;{std::vector<std::uint8_t> csc,pack;std::FILE* f=std::fopen("../OutRun2006 Coast 2 Coast (FXT)/Scripts/bin/csc_data_cvt.bin","rb");if(!f)throw std::runtime_error("csc_data_cvt.bin");csc.resize(CourseCvtBlobBytes);const auto got=std::fread(csc.data(),1,csc.size(),f);std::fclose(f);if(got!=csc.size())throw std::runtime_error("csc_data_cvt.bin size");// descriptors from the player's EXE image (OR2_EXE), course records from the retail file
        if(!build_course_asset_pack(csc,pack,&error)||!parse_course_asset_pack(pack.data(),pack.size(),course,&error))throw std::runtime_error(error);}
        const auto tables=outrun::driving::course_descriptor_tables_r078(course.descriptors);
        const unsigned records_guest=base+0x14000;
        unsigned lengths{};
        for(unsigned rec=0;rec<races.race_count;++rec){
            const auto* r=races.bytes.data()+races.races_offset+rec*0x44;std::uint32_t key,sub;std::memcpy(&key,r,4);std::memcpy(&sub,r+4,4);
            RaceCourseSelection sel;if(!race_course_select_4965a0(races,key,sub,sel))continue;
            outrun::driving::PcCourseRuntimeState44d720 state{};std::array<std::uint8_t,128> stack{};
            outrun::driving::PcMatrixStack matrices{outrun::driving::Bytes(stack.data(),stack.size()),0,0,2};
            outrun::driving::PcCourseApplyRequest44d720 request{};request.data_name="Race_Courses";request.category_name="";request.loader={true,3};
            request.direct_records=sel.course_records.data();request.direct_bytes=sel.course_records.size();request.direct_count=std::int32_t(sel.course_count);
            check(outrun::driving::runtime_apply_course_data_44d720(request,tables,state,matrices,{}),"native 44D720 for 44B820");
            std::memcpy(reinterpret_cast<void*>(records_guest),sel.course_records.data(),sel.course_records.size());
            p32(0x7d33bc,records_guest+unsigned(state.selected_index)*0x78);
            std::memcpy(reinterpret_cast<void*>(0x7d30a8),state.selected_7d30a8.data(),state.selected_7d30a8.size());
            for(std::int32_t laps:{-1,0,1,2,3,7}){
                prepare(0x44b820);Bytes(reinterpret_cast<void*>(S),4).put32(0,std::uint32_t(laps));run_original32();
                std::uint32_t length{};
                const bool ok=course_length_44b820(state.selected_7d30a8.data(),sel.course_records.data()+std::size_t(state.selected_index)*0x78,
                    sel.course_records.size()-std::size_t(state.selected_index)*0x78,course.descriptors,laps,length);
                check(ok&&length==guest_call.out_eax,"44B820 course length");++lengths;
            }
        }
        std::cerr<<"44B820 lengths compared "<<lengths<<char(10);
    }
    std::cout<<"{\"routine\":\"mission_manager_probe\",\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}\n";return mismatches?1:0;
}

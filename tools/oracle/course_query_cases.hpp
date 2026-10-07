// Included by compare_pc.cpp after the spline fixture comparator.
constexpr const char* course_query_names[]={"find_secondary_course_run","coli_get_forward_polygon_number",
    "coli_get_back_polygon_number","coli_get_left_polygon_number","coli_get_right_polygon_number","get_road_cond","course_query_chain"};
bool course_query_enabled(const std::string& only){
    if(only=="all"||only=="course_query_all")return true;
    for(auto n:course_query_names)if(only==n)return true;
    return false;
}
std::ofstream query_golden;std::uint32_t query_golden_count=0;
void query_write_u32(std::uint32_t v){char b[4];for(unsigned k=0;k<4;++k)b[k]=char(v>>(k*8));query_golden.write(b,4);}
void begin_query_golden(const char* path){
    query_golden.open(path,std::ios::binary|std::ios::trunc);if(!query_golden)throw std::runtime_error("cannot open query snapshots");
    query_golden.write("OR2Q015\0",8);query_write_u32(1);query_write_u32(x87_control);query_write_u32(0);
}
void finish_query_golden(){if(query_golden.is_open()){query_golden.seekp(16);query_write_u32(query_golden_count);query_golden.close();}}
void run_course_query_cases(unsigned i,const std::string& only){
    if(!course_query_enabled(only))return;
    for(unsigned id=0;id<7;++id){
        if(only!="all"&&only!="course_query_all"&&only!=course_query_names[id])continue;
        auto f=outrun::testing::make_course_query_fixture(i,id>=5u,id==0u);auto m=f.bytes();
        const auto type=m.u32(0xf00),options=m.u32(0xf04);
        // Save/restore the full globals page; explicitly check that all roots,
        // unrelated globals and tuning words are unchanged by each query.
        std::array<std::uint8_t,4096> old_globals{},configured_globals{};
        std::memcpy(old_globals.data(),reinterpret_cast<void*>(0x780000),4096);
        auto root=[&](std::uint32_t at,std::uint32_t value){*reinterpret_cast<std::uint32_t*>(at+type*4u)=value;};
        root(0x780100,E+0x180);root(0x780110,options&4u?E+0x400:0);root(0x780120,E+0x800);
        root(0x780140,options&2u?E+0x140:0);root(0x780228,E+0x200);root(0x780218,E+0x280);root(0x7801f8,E+0xc00);
        *reinterpret_cast<std::int32_t*>(0x780238)=m.i32(0xf08);*reinterpret_cast<std::uint8_t*>(0x780190)=options&1u?0x80:0;
        std::memcpy(configured_globals.data(),reinterpret_cast<void*>(0x780000),4096);
        const auto old_lat=*reinterpret_cast<std::uint32_t*>(0x67d94c),old_long=*reinterpret_cast<std::uint32_t*>(0x67d950);
        *reinterpret_cast<std::uint32_t*>(0x67d94c)=m.u32(0xf20);*reinterpret_cast<std::uint32_t*>(0x67d950)=m.u32(0xf24);
        std::memcpy(reinterpret_cast<void*>(E),f.image.data(),4096);
        constexpr std::uint32_t entry[]={0x43d6e0,0x43df20,0x43e0d0,0x43e250,0x43e300,0x43e7e0};
        Bytes st(reinterpret_cast<void*>(S),64);
        if(id==6u){
            const auto t=f.tables();const auto index=m.i32(0xf10);
            bool guest_hits[4]{},native_hits[4]{};
            const std::uint32_t entries[]={0x43df20,0x43e0d0,0x43e250,0x43e300};
            using Fn=bool(*)(const CourseCollisionTables&,std::int32_t,std::int32_t&);
            const Fn fn[]={coli_get_forward_polygon_number,coli_get_back_polygon_number,coli_get_left_polygon_number,coli_get_right_polygon_number};
            for(unsigned stage=0;stage<4;++stage){
                prepare(entries[stage]);st.put32(0,E+4u+stage*4u);st.puti(4,index);st.put32(8,type);run();
                guest_hits[stage]=guest_call.out_eax!=0;
                auto output=m.i32(4u+stage*4u);native_hits[stage]=fn[stage](t,index,output);m.puti(4u+stage*4u,output);
                compare_u32(std::string("course_query_chain_")+std::to_string(stage+1)+"_return",guest_hits[stage],native_hits[stage]);
                compare_spline_image(std::string("course_query_chain_")+std::to_string(stage+1)+"_memory",f.image);
            }
            // Each side consumes ITS OWN retained neighbor outputs. No guest
            // output or memory is copied into the native state between stages.
            prepare(0x43cd50);guest_call.esi=E+0x400u+std::uint32_t(index)*0x40u;guest_call.st0=1;
            st.putf(0,m.f32(0xe00));st.putf(4,m.f32(0xe08));st.put32(8,E+0x800u+std::uint32_t(index)*0x30u);
            Bytes gm(reinterpret_cast<void*>(E),4096);const unsigned slot[]={1,0,2,3};
            for(unsigned k=0;k<4;++k){const unsigned q=slot[k];st.put32(12+k*4,guest_hits[q]?E+0x400u+gm.u32(4+q*4)*0x40u:0);}
            run();const auto original=guest_call.out_st0;
            CourseQuad quads[4]{};const CourseQuad* pointers[4]{};
            for(unsigned k=0;k<4;++k){const unsigned q=slot[k];if(native_hits[q]){quads[k]=course_quad_vertices(t.polygons.sub(std::size_t(m.u32(4+q*4))*0x40u,0x40));pointers[k]=&quads[k];}}
            const auto poly=course_quad_vertices(t.polygons.sub(std::size_t(index)*0x40u,0x40));
            const auto normal=course_quad_vertices(t.normals.sub(std::size_t(index)*0x30u,0x30));
            const auto y=calc_y_pos_spl(m.f32(0xe00),m.f32(0xe08),poly,normal,{pointers[0],pointers[1],pointers[2],pointers[3]},f.tuning());
            gm.put32(0x3c,original);m.putf(0x3c,y);
            compare_u32("course_query_chain_5_height",original,fbits(y));
            compare_spline_image("course_query_chain_5_memory",f.image);
            compare_u32("course_query_chain_globals",std::memcmp(configured_globals.data(),reinterpret_cast<void*>(0x780000),4096)==0,1);
            compare_u32("course_query_chain_tuning_lateral",*reinterpret_cast<std::uint32_t*>(0x67d94c),m.u32(0xf20));
            compare_u32("course_query_chain_tuning_longitudinal",*reinterpret_cast<std::uint32_t*>(0x67d950),m.u32(0xf24));
            *reinterpret_cast<std::uint32_t*>(0x67d94c)=old_lat;*reinterpret_cast<std::uint32_t*>(0x67d950)=old_long;
            std::memcpy(reinterpret_cast<void*>(0x780000),old_globals.data(),4096);
            continue;
        }
        prepare(entry[id]);
        if(id==0){guest_call.eax=E+0x100;st.put32(0,E+0x20);st.put32(4,E+0x24);}
        else if(id<5){st.put32(0,E+id*4);st.puti(4,m.i32(0xf10));st.put32(8,type);}
        else {guest_call.eax=static_cast<std::uint16_t>(m.i16(0xf18));st.put32(0,m.u32(0xf14));st.putf(4,m.f32(0xe00));
            st.put32(8,E+0x30);st.putf(12,m.f32(0xe08));st.put32(16,E+0x34);st.put32(20,E+0x38);st.put32(24,type);}
        run();const auto original_result=id==5u?0u:guest_call.out_eax;
        if(query_golden.is_open()&&i<256u){
            query_write_u32(id);query_write_u32(i);query_write_u32(original_result);
            query_golden.write(reinterpret_cast<const char*>(f.image.data()),4096);
            query_golden.write(reinterpret_cast<const char*>(E),4096);++query_golden_count;
            if(!query_golden)throw std::runtime_error("query snapshot write failed");
        }
        std::uint32_t native=0;
        try { native=outrun::testing::run_course_query_native(f,id); }
        catch(const std::exception& ex){
            throw std::runtime_error(std::string(course_query_names[id])+" case="+std::to_string(i)+" type="+std::to_string(type)+": "+ex.what());
        }
        compare_u32(std::string(course_query_names[id])+"_return",original_result,native);
        compare_spline_image(std::string(course_query_names[id])+"_memory",f.image);
        const auto intact=std::memcmp(configured_globals.data(),reinterpret_cast<void*>(0x780000),4096)==0;
        compare_u32(std::string(course_query_names[id])+"_globals",intact?1u:0u,1u);
        compare_u32("course_query_tuning_lateral",*reinterpret_cast<std::uint32_t*>(0x67d94c),m.u32(0xf20));
        compare_u32("course_query_tuning_longitudinal",*reinterpret_cast<std::uint32_t*>(0x67d950),m.u32(0xf24));
        *reinterpret_cast<std::uint32_t*>(0x67d94c)=old_lat;*reinterpret_cast<std::uint32_t*>(0x67d950)=old_long;
        std::memcpy(reinterpret_cast<void*>(0x780000),old_globals.data(),4096);
    }
}

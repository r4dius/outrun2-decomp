// r017: no body-wall or wrecker stubs; only closed geometric original entries.
constexpr const char* wall_names[]={"matrix_load_rotation","matrix_inverse_vector","cop_coli_point","calc_coli_wall_face","push_outpos_mat_obsolete","coli_set_obsolete","wall_geometry_chain"};
constexpr std::uint32_t WallArena=0x34400000u;
bool wall_enabled(const std::string& only){
    if(only=="all"||only=="wall_geometry_all")return true;
    for(auto n:wall_names)if(only==n)return true;
    return false;
}
void init_wall_oracle(){map_at(WallArena,0x10000,PROT_READ|PROT_WRITE);}
struct WallBinding {
    std::array<std::uint8_t,4096> globals{},matrices{},expected_globals{},expected_matrices{};
    std::array<std::uint8_t,128> transforms{};
    explicit WallBinding(outrun::testing::WallGeometryFixture& f){
        auto b=f.bytes();std::memcpy(globals.data(),reinterpret_cast<void*>(0x780000),4096);
        std::memcpy(matrices.data(),reinterpret_cast<void*>(0x89b000),4096);
        std::memcpy(transforms.data(),reinterpret_cast<void*>(0x7d2da0),64);std::memcpy(transforms.data()+64,reinterpret_cast<void*>(0x7d3190),64);
        std::memcpy(reinterpret_cast<void*>(WallArena),f.image.data(),f.image.size());
        for(unsigned t=0;t<4;++t)*reinterpret_cast<std::uint32_t*>(0x780110+t*4)=WallArena;
        *reinterpret_cast<std::uint32_t*>(0x89b564)=WallArena+0xf00u+b.u32(0x1300);
        *reinterpret_cast<std::uint32_t*>(0x89b568)=b.u32(0x1304);*reinterpret_cast<std::uint32_t*>(0x89b56c)=b.u32(0x1308);
        std::memcpy(reinterpret_cast<void*>(0x7d2da0),f.image.data()+0x400,64);std::memcpy(reinterpret_cast<void*>(0x7d3190),f.image.data()+0x440,64);
        std::memcpy(expected_globals.data(),reinterpret_cast<void*>(0x780000),4096);std::memcpy(expected_matrices.data(),reinterpret_cast<void*>(0x89b000),4096);
    }
    void capture(){
        Bytes b(reinterpret_cast<void*>(WallArena),outrun::testing::wall_image_size);
        b.put32(0x1300,*reinterpret_cast<std::uint32_t*>(0x89b564)-(WallArena+0xf00u));
        b.put32(0x1304,*reinterpret_cast<std::uint32_t*>(0x89b568));b.put32(0x1308,*reinterpret_cast<std::uint32_t*>(0x89b56c));
    }
    void compare(const std::string& name,outrun::testing::WallGeometryFixture& f){
        auto b=f.bytes();compare_world_bytes(name+"_memory",reinterpret_cast<void*>(WallArena),f.image.data(),f.image.size());
        Bytes m(expected_matrices.data(),4096);m.put32(0x564,WallArena+0xf00u+b.u32(0x1300));m.put32(0x568,b.u32(0x1304));m.put32(0x56c,b.u32(0x1308));
        compare_world_bytes(name+"_globals",reinterpret_cast<void*>(0x780000),expected_globals.data(),4096);
        compare_world_bytes(name+"_matrix_globals",reinterpret_cast<void*>(0x89b000),expected_matrices.data(),4096);
        compare_world_bytes(name+"_primary_transform",reinterpret_cast<void*>(0x7d2da0),f.image.data()+0x400,64);
        compare_world_bytes(name+"_secondary_transform",reinterpret_cast<void*>(0x7d3190),f.image.data()+0x440,64);
    }
    ~WallBinding(){
        std::memcpy(reinterpret_cast<void*>(0x780000),globals.data(),4096);std::memcpy(reinterpret_cast<void*>(0x89b000),matrices.data(),4096);
        std::memcpy(reinterpret_cast<void*>(0x7d2da0),transforms.data(),64);std::memcpy(reinterpret_cast<void*>(0x7d3190),transforms.data()+64,64);
    }
};
std::uint32_t run_wall_original(unsigned id){
    constexpr std::uint32_t entry[]={0x40a190,0x40a8a0,0x43d1d0,0x502e20,0x502f80,0x5030f0,0x40a170};
    prepare(entry[id]);Bytes b(reinterpret_cast<void*>(WallArena),outrun::testing::wall_image_size),st(reinterpret_cast<void*>(S),64);
    if(id==0u)st.put32(0,WallArena+b.u32(0x1340));
    else if(id==1u){st.put32(0,WallArena+0x1370);st.put32(4,WallArena+0x1360);}
    else if(id==2u){
        st.put32(0,b.u32(0x1314));st.put32(4,b.u32(0x1310));
        for(unsigned k=0;k<4;++k)st.put32(8+k*4,WallArena+b.u32(0x1380+k*4));
    }else if(id==3u){
        st.put32(0,b.u32(0x1318));st.put32(4,b.u32(0x131c));
        for(unsigned k=0;k<4;++k)st.put32(8+k*4,WallArena+b.u32(0x1380+k*4));
    }else if(id==4u){
        guest_call.esi=WallArena+0x4c0;guest_call.ecx=WallArena+0x500;
        st.put32(0,WallArena+0x700);st.put32(4,WallArena+0x1324);st.put32(8,WallArena+0x1328);
    }else if(id==5u){
        guest_call.eax=WallArena+0x500;st.put32(0,WallArena+0x700);st.put32(4,WallArena+0x4c0);
        st.put32(8,b.u32(0x1324));st.put32(12,WallArena+0x132c);
    }else if(id==6u){outrun::testing::wall_prepare_response(b);st.put32(0,WallArena+0x710);}
    run();return id==5u?guest_call.out_eax:0;
}
std::ofstream wall_golden;std::uint32_t wall_golden_count=0;
void wall_u32(std::uint32_t v){char b[4];for(unsigned k=0;k<4;++k)b[k]=char(v>>(8*k));wall_golden.write(b,4);}
void begin_wall_golden(const char* path){
    wall_golden.open(path,std::ios::binary|std::ios::trunc);if(!wall_golden)throw std::runtime_error("wall snapshot open");
    wall_golden.write("OR2L017\0",8);wall_u32(1);wall_u32(x87_control);wall_u32(0);wall_u32(outrun::testing::wall_image_size);
}
void finish_wall_golden(){if(wall_golden.is_open()){
    wall_golden.seekp(16);wall_u32(wall_golden_count);wall_golden.flush();if(!wall_golden)throw std::runtime_error("wall snapshot finalization");wall_golden.close();}}
std::map<std::string,std::uint64_t> wall_coverage;
void run_wall_cases(unsigned i,const std::string& only){
    if(!wall_enabled(only))return;
    WorldNativeControl control(static_cast<unsigned short>(x87_control));
    for(unsigned id=0;id<7;++id){
        if(only!="all"&&only!="wall_geometry_all"&&only!=wall_names[id])continue;
        auto f=outrun::testing::make_wall_geometry_fixture(i,id==6u?7u:id);WallBinding binding(f);
        const auto before=f.image;const unsigned stages=id==6u?8u:1u;
        const bool capture=wall_golden.is_open()&&i<128u;
        if(capture){wall_u32(id);wall_u32(i);wall_u32(stages);wall_golden.write(reinterpret_cast<const char*>(before.data()),before.size());++wall_golden_count;}
        for(unsigned stage=0;stage<stages;++stage){
            const auto actual=id==6u?outrun::testing::wall_sequence[stage]:id;
            const auto original=run_wall_original(actual);binding.capture();
            if(capture){wall_u32(original);wall_golden.write(reinterpret_cast<const char*>(WallArena),before.size());if(!wall_golden)throw std::runtime_error("wall snapshot write");}
            std::uint32_t native=0;
            try{native=outrun::testing::run_wall_geometry_native(f,actual);}catch(const std::exception& ex){throw std::runtime_error(std::string(wall_names[id])+" case="+std::to_string(i)+" stage="+std::to_string(stage)+": "+ex.what());}
            const auto name=std::string(wall_names[id])+(id==6u?"_"+std::to_string(stage+1):"");
            compare_u32(name+"_return",original,native);binding.compare(name,f);
            auto b=f.bytes();
            if(actual==4u){++wall_coverage[b.u32(0x1324)?"push_nonzero_mask":"push_zero_mask"];++wall_coverage[b.f32(0x1328)>0?"push_positive_distance":"push_zero_distance"];}
            if(actual==5u)++wall_coverage[original?"weighted_contact":"no_weighted_contact"];
            if(actual==2u)++wall_coverage["course_type_"+std::to_string(b.u32(0x1310))];
        }
    }
}

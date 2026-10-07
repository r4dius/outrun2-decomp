// r018: original stage/entrapment/matrix/friction code, no response stubs.
constexpr auto ResponseArena=outrun::testing::response_guest_base;
bool response_enabled(const std::string& only){
    if(only=="all"||only=="wall_response_all")return true;
    for(auto n:outrun::testing::response_names)if(only==n)return true;
    return false;
}
constexpr std::uint32_t response_pages[]={0x7d2000,0x7d3000,0x7de000,0x89b000,0x5e0000,0x5e1000,0x5e2000,0x5e3000};
void init_response_oracle(){
    map_at(ResponseArena,0x10000,PROT_READ|PROT_WRITE);
    for(auto p:response_pages)if(mprotect(reinterpret_cast<void*>(p),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("response globals mprotect");
}
struct ResponseBinding {
    std::array<std::array<std::uint8_t,4096>,8> saved{},expected{};
    explicit ResponseBinding(outrun::testing::WallResponseFixture& f){
        for(unsigned k=0;k<8;++k)std::memcpy(saved[k].data(),reinterpret_cast<void*>(response_pages[k]),4096);
        auto b=f.bytes();std::memcpy(reinterpret_cast<void*>(ResponseArena),f.image.data(),f.image.size());
        *reinterpret_cast<std::uint32_t*>(0x7d33bc)=ResponseArena+0x1c00;
        *reinterpret_cast<std::uint32_t*>(0x7d33c4)=b.u32(0x400c);
        *reinterpret_cast<std::uint32_t*>(0x7d2df4)=ResponseArena+0x1e40;
        *reinterpret_cast<std::uint8_t*>(0x7de418)=b.u8(0x401c);
        *reinterpret_cast<std::uint32_t*>(0x89b564)=ResponseArena+0x1800+b.u32(0x4000);
        *reinterpret_cast<std::uint32_t*>(0x89b568)=b.u32(0x4004);*reinterpret_cast<std::uint32_t*>(0x89b56c)=b.u32(0x4008);
        *reinterpret_cast<std::uint32_t*>(0x5e302c)=b.u32(0x4024);*reinterpret_cast<std::uint32_t*>(0x5e3030)=b.u32(0x4028);
        std::memcpy(reinterpret_cast<void*>(0x5e0f20),f.image.data()+0x2000,0x1000);
        std::memcpy(reinterpret_cast<void*>(0x5e1fa0),f.image.data()+0x3000,0x1000);
        for(unsigned k=0;k<8;++k)std::memcpy(expected[k].data(),reinterpret_cast<void*>(response_pages[k]),4096);
    }
    void capture(){
        Bytes b(reinterpret_cast<void*>(ResponseArena),outrun::testing::response_image_size);
        b.put32(0x4000,*reinterpret_cast<std::uint32_t*>(0x89b564)-(ResponseArena+0x1800));
        b.put32(0x4004,*reinterpret_cast<std::uint32_t*>(0x89b568));b.put32(0x4008,*reinterpret_cast<std::uint32_t*>(0x89b56c));
    }
    void compare(const std::string& name,outrun::testing::WallResponseFixture& f){
        compare_world_bytes(name+"_memory",reinterpret_cast<void*>(ResponseArena),f.image.data(),f.image.size());
        for(unsigned k=0;k<8;++k)compare_world_bytes(name+"_globals_"+std::to_string(k),reinterpret_cast<void*>(response_pages[k]),expected[k].data(),4096);
    }
    ~ResponseBinding(){for(unsigned k=0;k<8;++k)std::memcpy(reinterpret_cast<void*>(response_pages[k]),saved[k].data(),4096);}
};
std::uint32_t run_response_original(unsigned id){
    constexpr std::uint32_t entries[]={0x44c8d0,0x44dc50,0x5036c0,0x503720,0x4a47f0,0x40a020,0x40a100,0x40a220,0x40a410,0x503bf0,0x503380};
    prepare(entries[id]);Bytes b(reinterpret_cast<void*>(ResponseArena),outrun::testing::response_image_size),st(reinterpret_cast<void*>(S),64);
    if(id<2)st.put32(0,b.u32(0x4010));
    else if(id<4){guest_call.eax=ResponseArena;guest_call.ebx=b.u32(0x4014);}
    else if(id==4){st.put32(0,ResponseArena);st.put32(4,b.u32(0x4018));}
    else if(id==6)st.put32(0,ResponseArena+b.u32(0x402c));
    else if(id==7)st.put32(0,ResponseArena+b.u32(0x4030));
    else if(id==8)st.put32(0,b.u32(0x4020));
    else if(id==9){guest_call.esi=ResponseArena;guest_call.eax=ResponseArena+0x1000;st.put32(0,b.u32(0x4020));}
    else if(id==10){guest_call.esi=ResponseArena;guest_call.eax=std::uint32_t(b.i16(0x4034));}
    run();
    if(id==0)return guest_call.out_eax?(guest_call.out_eax-ResponseArena-0x1c00)/0x78:0xffffffffu;
    if(id<4)return guest_call.out_eax;
    return 0;
}
std::ofstream response_golden;std::uint32_t response_golden_count=0;
void response_u32(std::uint32_t v){char b[4];for(unsigned k=0;k<4;++k)b[k]=char(v>>(8*k));response_golden.write(b,4);}
void begin_response_golden(const char* path){
    response_golden.open(path,std::ios::binary|std::ios::trunc);if(!response_golden)throw std::runtime_error("response snapshot open");
    response_golden.write("OR2R018\0",8);response_u32(1);response_u32(x87_control);response_u32(0);response_u32(outrun::testing::response_image_size);
}
void finish_response_golden(){if(response_golden.is_open()){
    response_golden.seekp(16);response_u32(response_golden_count);response_golden.flush();if(!response_golden)throw std::runtime_error("response snapshot finalization");response_golden.close();}}
std::map<std::string,std::uint64_t> response_coverage;
void run_response_cases(unsigned i,const std::string& only){
    if(!response_enabled(only))return;
    WorldNativeControl control(static_cast<unsigned short>(x87_control));
    for(unsigned id=0;id<outrun::testing::response_selector_count;++id){
        if(only!="all"&&only!="wall_response_all"&&only!=outrun::testing::response_names[id])continue;
        auto f=outrun::testing::make_wall_response_fixture(i,id);ResponseBinding binding(f);
        const auto before=f.image;const unsigned stages=id==11?3u:1u;
        const bool capture=response_golden.is_open()&&i<128u;
        if(capture){response_u32(id);response_u32(i);response_u32(stages);response_golden.write(reinterpret_cast<const char*>(before.data()),before.size());++response_golden_count;}
        for(unsigned stage=0;stage<stages;++stage){
            const auto actual=id==11?9u:id;const auto old=f.image;
            const auto original=run_response_original(actual);binding.capture();
            if(capture){response_u32(original);response_golden.write(reinterpret_cast<const char*>(ResponseArena),before.size());if(!response_golden)throw std::runtime_error("response snapshot write");}
            std::uint32_t native=0;
            try{native=outrun::testing::run_wall_response_native(f,actual);}catch(const std::exception& ex){throw std::runtime_error(std::string(outrun::testing::response_names[id])+" case="+std::to_string(i)+" stage="+std::to_string(stage)+": "+ex.what());}
            const auto name=std::string(outrun::testing::response_names[id])+(id==11?"_"+std::to_string(stage+1):"");
            if(original!=native)std::cerr<<"response return mismatch id="<<id<<" case="<<i<<" stage="<<stage<<" lookup_stage="<<outrun::driving::pc_stage_number(f.context().stages,f.bytes().u32(0x68))<<" position="<<f.bytes().u32(0x4014)<<"\n";
            compare_u32(name+"_return",original,native);binding.compare(name,f);
            auto b=f.bytes();
            if(actual==2||actual==3)++response_coverage[std::string(actual==2?"crush_":"friction_")+(original?"in_range":"out_of_range")];
            if(actual==9){
                const auto c=f.context();const bool a=outrun::driving::check_crush_entrapment_length(b.sub(0,0x1000),std::uint16_t(b.i16(0x64)),c),d=outrun::driving::check_friction_entrapment_length(b.sub(0,0x1000),std::uint16_t(b.i16(0x64)),c);
                ++response_coverage["response_zone_"+std::to_string(unsigned(a)+2u*unsigned(d))];
                ++response_coverage[std::memcmp(old.data()+0x105c,f.image.data()+0x105c,12)?"velocity_adopted":"velocity_preserved"];
                ++response_coverage[std::memcmp(old.data()+0xdec,f.image.data()+0xdec,4)?"timer_raised":"timer_preserved"];
                ++response_coverage["side_"+std::to_string(b.u8(0x281))];
            }
        }
    }
}

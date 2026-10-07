// r019 original public entries with all their callees: no sound/route stubs.
constexpr auto ReboundArena=outrun::testing::rebound_guest_base;
bool rebound_enabled(const std::string& only){
    if(only=="all"||only=="wall_rebound_all")return true;
    for(auto name:outrun::testing::rebound_names)if(only==name)return true;
    return false;
}
constexpr std::uint32_t rebound_pages[]={0x7d2000,0x7d3000,0x7de000,0x89b000,0x5e0000,0x5e1000,0x5e2000,0x5e3000,0x635000,0x7dd000,0x7df000,0x780000,0x7f1000,0x956000,0x79f000,0x836000,0x830000};
constexpr unsigned rebound_page_count=sizeof(rebound_pages)/sizeof(rebound_pages[0]);
void init_rebound_oracle(){
    map_at(ReboundArena,0x10000,PROT_READ|PROT_WRITE);
    for(auto p:rebound_pages)if(mprotect(reinterpret_cast<void*>(p),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("rebound globals mprotect");
}
// Every changed global has an explicit native representation; every byte in
// the other parts of these pages must stay equal to its bound pre-state.
constexpr std::uint32_t rebound_mutable[][3]={{0x635f2c,0x5000,8},{0x7d39a0,0x5100,256},{0x7de418,0x5800,0x1000},{0x9563e8,0x6900,128},{0x9560c0,0x6980,4},{0x956124,0x6984,4}};
struct ReboundBinding {
    std::array<std::array<std::uint8_t,4096>,rebound_page_count> saved{},expected{};
    explicit ReboundBinding(outrun::testing::WallReboundFixture& f){
        for(unsigned k=0;k<rebound_page_count;++k)std::memcpy(saved[k].data(),reinterpret_cast<void*>(rebound_pages[k]),4096);
        auto b=f.bytes();std::memcpy(reinterpret_cast<void*>(ReboundArena),f.image.data(),f.image.size());
        auto put=[](std::uint32_t va,std::uint32_t v){*reinterpret_cast<std::uint32_t*>(va)=v;};
        put(0x7d33bc,ReboundArena+0x1c00);put(0x7d33c4,b.u32(0x400c));put(0x7d2df4,ReboundArena+0x1e40);
        put(0x89b564,ReboundArena+0x1800+b.u32(0x4000));put(0x89b568,b.u32(0x4004));put(0x89b56c,b.u32(0x4008));
        put(0x5e302c,b.u32(0x4024));put(0x5e3030,b.u32(0x4028));
        std::memcpy(reinterpret_cast<void*>(0x5e0f20),f.image.data()+0x2000,0x1000);std::memcpy(reinterpret_cast<void*>(0x5e1fa0),f.image.data()+0x3000,0x1000);
        for(const auto& m:rebound_mutable)std::memcpy(reinterpret_cast<void*>(m[0]),f.image.data()+m[1],m[2]);
        put(0x7d3188,ReboundArena+0x5300);put(0x780258,b.u32(0x6a04));put(0x7f1938,b.u32(0x6a08));
        *reinterpret_cast<std::uint8_t*>(0x7dd138)=b.u8(0x6a00);
        *reinterpret_cast<std::uint8_t*>(0x836374)=b.u8(0x6a0c);*reinterpret_cast<std::uint8_t*>(0x830394)=b.u8(0x6a0d);*reinterpret_cast<std::uint8_t*>(0x8361b4)=b.u8(0x6a0e);
        *reinterpret_cast<std::uint8_t*>(0x79fcc7)=b.u8(0x6988);
        for(unsigned k=0;k<rebound_page_count;++k)std::memcpy(expected[k].data(),reinterpret_cast<void*>(rebound_pages[k]),4096);
    }
    void capture(){
        Bytes b(reinterpret_cast<void*>(ReboundArena),outrun::testing::rebound_image_size);
        b.put32(0x4000,*reinterpret_cast<std::uint32_t*>(0x89b564)-(ReboundArena+0x1800));b.put32(0x4004,*reinterpret_cast<std::uint32_t*>(0x89b568));b.put32(0x4008,*reinterpret_cast<std::uint32_t*>(0x89b56c));
        for(const auto& m:rebound_mutable)std::memcpy(reinterpret_cast<void*>(ReboundArena+m[1]),reinterpret_cast<void*>(m[0]),m[2]);
    }
    void compare(const std::string& name,outrun::testing::WallReboundFixture& f){
        compare_world_bytes(name+"_memory",reinterpret_cast<void*>(ReboundArena),f.image.data(),f.image.size());
        auto put_expected=[&](std::uint32_t va,const void* src,std::size_t n){
            auto p=static_cast<const std::uint8_t*>(src);
            std::size_t off=0;
            while(off<n){
                const auto addr=va+std::uint32_t(off), page=addr&~4095u;
                unsigned k=0;while(k<rebound_page_count&&rebound_pages[k]!=page)++k;
                if(k==rebound_page_count)throw std::runtime_error("unmapped expected global");
                const auto chunk=std::min<std::size_t>(n-off,4096u-(addr&4095u));
                std::memcpy(expected[k].data()+(addr&4095u),p+off,chunk);off+=chunk;
            }
        };
        for(const auto& m:rebound_mutable)put_expected(m[0],f.image.data()+m[1],m[2]);
        auto b=f.bytes();const std::uint32_t mat[]={ReboundArena+0x1800+b.u32(0x4000),b.u32(0x4004),b.u32(0x4008)};put_expected(0x89b564,mat,12);
        for(unsigned k=0;k<rebound_page_count;++k)compare_world_bytes(name+"_globals_"+std::to_string(k),reinterpret_cast<void*>(rebound_pages[k]),expected[k].data(),4096);
    }
    ~ReboundBinding(){for(unsigned k=0;k<rebound_page_count;++k)std::memcpy(reinterpret_cast<void*>(rebound_pages[k]),saved[k].data(),4096);}
};
std::uint32_t run_rebound_original(unsigned id){
    const std::uint32_t entries[]={0x44c940,0x4503d0,0x450490,0x456820,0x451140,0x451350,0x424940,0x5033f0,0x487790,0x503a20};
    prepare(id==12?0x503bf0:entries[id]);Bytes b(reinterpret_cast<void*>(ReboundArena),outrun::testing::rebound_image_size),st(reinterpret_cast<void*>(S),64);
    switch(id){
      case 0:st.put32(0,b.u32(0x68));break;
      case 1:break;
      case 2:st.put32(0,b.u32(0x6b18));break;
      case 3:guest_call.ecx=0x7de418;st.put32(0,b.u32(0x6b18));st.put32(4,b.u32(0x6a08));break;
      case 4:st.put32(0,b.u32(0x6b10));st.put32(4,b.u32(0x6b14));break;
      case 5:st.put32(0,b.u32(0x6b10));break;
      case 6:st.put32(0,b.u32(0x6b1c));break;
      case 7:guest_call.esi=ReboundArena;st.put32(0,std::uint32_t(b.i16(0x4034)));break;
      case 8:for(unsigned k=0;k<4;++k)st.put32(k*4,b.u32(0x6b20+4*k));st.put32(16,ReboundArena+b.u32(0x6b30));st.put32(20,ReboundArena+b.u32(0x6b34));break;
      case 9:guest_call.ecx=ReboundArena;guest_call.eax=ReboundArena+0x6b00;st.put32(0,ReboundArena+0x1000);break;
      case 12:guest_call.esi=ReboundArena;guest_call.eax=ReboundArena+0x1000;st.put32(0,b.u32(0x4020));break;
    }
    if(id==3){run_original32();if(guest_call.out_sp!=S+8)throw std::runtime_error("route saver stdcall imbalance");}else run();
    return id==0||id==1||id==5?guest_call.out_eax:0u;
}
std::ofstream rebound_golden;std::uint32_t rebound_golden_count=0;
void rebound_u32(std::uint32_t v){char b[4];for(unsigned k=0;k<4;++k)b[k]=char(v>>(8*k));rebound_golden.write(b,4);}
void begin_rebound_golden(const char* path){rebound_golden.open(path,std::ios::binary|std::ios::trunc);if(!rebound_golden)throw std::runtime_error("rebound snapshot open");rebound_golden.write("OR2R019\0",8);rebound_u32(1);rebound_u32(x87_control);rebound_u32(0);rebound_u32(outrun::testing::rebound_image_size);}
void finish_rebound_golden(){if(rebound_golden.is_open()){rebound_golden.seekp(16);rebound_u32(rebound_golden_count);rebound_golden.flush();if(!rebound_golden)throw std::runtime_error("rebound snapshot write");rebound_golden.close();}}
std::map<std::string,std::uint64_t> rebound_coverage;
void run_rebound_cases(unsigned i,const std::string& only){
    if(!rebound_enabled(only))return;
    WorldNativeControl control(static_cast<unsigned short>(x87_control));
    for(unsigned id=0;id<outrun::testing::rebound_selector_count;++id){
        if(only!="all"&&only!="wall_rebound_all"&&only!=outrun::testing::rebound_names[id])continue;
        auto f=outrun::testing::make_wall_rebound_fixture(i,id);ReboundBinding binding(f);const auto before=f.image;
        const unsigned stages=outrun::testing::rebound_stage_count(id);const bool capture=rebound_golden.is_open()&&i<128u;
        if(capture){rebound_u32(id);rebound_u32(i);rebound_u32(stages);rebound_golden.write(reinterpret_cast<const char*>(before.data()),before.size());++rebound_golden_count;}
        for(unsigned stage=0;stage<stages;++stage){
            const auto actual=outrun::testing::rebound_stage_id(id,stage);const auto old=f.image;
            const auto original=run_rebound_original(actual);binding.capture();
            if(capture){rebound_u32(original);rebound_golden.write(reinterpret_cast<const char*>(ReboundArena),before.size());}
            const auto native=outrun::testing::run_wall_rebound_native(f,actual);
            const auto name=std::string(outrun::testing::rebound_names[id])+(stages>1?"_"+std::to_string(stage+1):"");
            const auto failed_before=stats[name+"_memory"].fail;
            compare_u32(name+"_return",original,native);binding.compare(name,f);
            if(stats[name+"_memory"].fail!=failed_before)std::cerr<<"rebound case="<<i<<" selector="<<name<<"\n";
            auto b=f.bytes();
            if(actual==9){++rebound_coverage["heading_mode_"+std::to_string(b.u32(0x290))];++rebound_coverage[std::memcmp(old.data()+0x105c,f.image.data()+0x105c,12)?"velocity_scaled":"velocity_preserved"];
                ++rebound_coverage[std::memcmp(old.data()+0x6900,f.image.data()+0x6900,128)?"sound_queued":"sound_not_queued"];
                ++rebound_coverage[std::memcmp(old.data()+0x5000,f.image.data()+0x5000,8)?"cache_updated":"cache_preserved"];
            }
            if(actual==4||actual==5||actual==9)++rebound_coverage[std::memcmp(old.data()+0x5800,f.image.data()+0x5800,0x1000)?"save_changed":"save_preserved"];
        }
    }
}

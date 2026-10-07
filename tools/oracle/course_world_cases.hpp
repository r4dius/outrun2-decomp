// r016: direct original PC entries, real world queries, no lower-query stub.
constexpr const char* world_names[]={"matrix_push","matrix_pop","matrix_load","matrix_inverse_point","matrix_point","get_y_position_prog","get_y_position_spl_chk_closed","course_world_chain",
    "matrix_push_load","matrix_vector","course_collision_world_normal","get_cs_road_info_by_cs_len","pl_wrecker_sub","advance_on_road_place","pl_wrecker_delayed",
    "matrix_push_unit","matrix_unit_rotation","matrix_get","matrix_rotate_x","matrix_rotate_z","matrix_translate_vector","reconstruct_posture_matrix_and_face_work","calc_disp_matrix","pl_wrecker_immediate","crash_entry_wrecker_chain_r025","cw_crush_status_r026","cbw_coli_wall_r027","get_y_position_prog_bk_r028","car_body_wall_coli_check_r028","coli_car_r028","action_force2_r029","matrix_rotate_axis_r029","set_force_obsolete_r029","make_force_work_sus_r029",
    "calc_game_resist_r030","force_work_49fed0_r030","ass_cancel_incline_resistance_r030","ass_press_down_against_road_r030","make_force_work_r030","rear_grip_curve_r031","rear_grip_ctrl_r031","slip_angle_ctrl_r031","cornering_ctrl_r031","calc_pl_body_force_limit_r031","make_force_work_tire_4a0000_r032","assist_wanderer_4a5470_r032","reb_pl_body_base_4a0200_r032","reb_pl_body_sub3_4a08f0_r033","reb_pl_body_sub2_4a0320_r033","reb_pl_body_4a62e0_r033","wrec_pl_body_4a0c70_r034","ass_compulsive_move_5184b0_r035","calc_pl_body_2nd_4a7ec0_r035","check_night_and_tunnel_4a25f0_r036","calc_disp_steering_angle_46eb40_r036","get_y_position_branch_43f110"};
constexpr std::uint32_t WorldArena=0x34000000u,WorldGrids=0x34100000u,WreckerArena=0x34200000u;
bool world_enabled(const std::string& only){
    if(only=="all"||only=="course_world_all"||only=="wrecker_r022_all"||only=="matrix_primitives_r023_all"||only=="posture_primitives_r024_all")return true;
    for(auto n:world_names)if(only==n)return true;
    return false;
}
void init_world_oracle(){
    map_at(WorldArena,0x10000,PROT_READ|PROT_WRITE);map_at(WorldGrids,0x80000,PROT_READ|PROT_WRITE);map_at(WreckerArena,0x10000,PROT_READ|PROT_WRITE);
    if(mprotect(reinterpret_cast<void*>(0x7d2000),0x2000,PROT_READ|PROT_WRITE))throw std::runtime_error("world transforms protection");
    for(auto page:{0x5a4000u,0x5c2000u,0x5c4000u,0x5e0000u,0x5e1000u,0x5e2000u,0x5e3000u,0x634000u,0x635000u,0x79f000u,0x7d3000u,0x7dd000u,0x7de000u,0x7df000u,0x7f1000u,0x7f9000u,0x800000u,0x82e000u,0x836000u,0x830000u,0x956000u,0x799000u})
        if(mprotect(reinterpret_cast<void*>(page),0x1000,PROT_READ|PROT_WRITE))throw std::runtime_error("wrecker advance globals protection");
}
void compare_world_bytes(const std::string& name,const void* original,const void* native,std::size_t n){
    auto& s=stats[name];++s.cases;s.bytes+=n;
    if(std::memcmp(original,native,n)==0){++s.exact;return;}
    ++s.fail;
    if(s.fail<=5){
        const auto a=static_cast<const std::uint8_t*>(original),b=static_cast<const std::uint8_t*>(native);
        for(std::size_t k=0;k<n;++k)if(a[k]!=b[k]){
            std::cerr<<name<<" case="<<s.cases<<" offset=0x"<<std::hex<<k<<" original="<<unsigned(a[k])<<" native="<<unsigned(b[k]);
            std::cerr<<std::dec<<'\n';break;
        }
    }
}
struct WorldNativeControl {
    unsigned short old{},selected{};
    explicit WorldNativeControl(unsigned short cw):selected(cw){
        __asm__ volatile("fnstcw %0":"=m"(old));__asm__ volatile("fldcw %0"::"m"(selected));
    }
    ~WorldNativeControl(){__asm__ volatile("fldcw %0"::"m"(old));}
};
struct WorldBinding {
    std::array<std::uint8_t,4096> globals{},matrix_globals{},configured{},configured_matrix{};
    std::array<std::uint8_t,128> transforms{};
    std::uint32_t lateral{},longitudinal{},primary_yaw{},secondary_yaw{};
    explicit WorldBinding(outrun::testing::CourseWorldFixture& f){
        auto b=f.bytes();std::memcpy(globals.data(),reinterpret_cast<void*>(0x780000),4096);
        std::memcpy(matrix_globals.data(),reinterpret_cast<void*>(0x89b000),4096);
        std::memcpy(transforms.data(),reinterpret_cast<void*>(0x7d2da0),64);
        std::memcpy(transforms.data()+64,reinterpret_cast<void*>(0x7d3190),64);
        lateral=*reinterpret_cast<std::uint32_t*>(0x67d94c);longitudinal=*reinterpret_cast<std::uint32_t*>(0x67d950);
        primary_yaw=*reinterpret_cast<std::uint32_t*>(0x7d3128);secondary_yaw=*reinterpret_cast<std::uint32_t*>(0x7d317c);
        std::memcpy(reinterpret_cast<void*>(WorldArena),f.image.data(),f.image.size());
        for(unsigned t=0;t<4;++t){
            const auto base=WorldArena+t*4096u;
            auto root=[&](std::uint32_t a,std::uint32_t p){*reinterpret_cast<std::uint32_t*>(a+t*4u)=p;};
            root(0x780100,base+0x180);root(0x780110,base+0x400);root(0x780120,base+0x800);root(0x780140,base+0x140);
            root(0x780228,base+0x200);root(0x780218,base+0x280);root(0x7801f8,base+0xc00);
            root(0x7801e8,(b.u32(0x4648)&(1u<<t))?WorldGrids+t*0x20000u:0u);
            std::memcpy(reinterpret_cast<void*>(WorldGrids+t*0x20000u),f.grids[t].data(),0x20000);
        }
        *reinterpret_cast<std::int32_t*>(0x780238)=b.i32(0xf08);
        *reinterpret_cast<std::uint8_t*>(0x780190)=b.u32(0xf04)&1u?0x80:0;
        for(unsigned k=0;k<16;++k)*reinterpret_cast<std::uint32_t*>(0x7801a8+k*4)=b.u32(0x4500+k*4);
        *reinterpret_cast<std::uint32_t*>(0x780240)=b.u32(0x4540);*reinterpret_cast<std::uint32_t*>(0x78023c)=b.u32(0x4544);
        *reinterpret_cast<std::uint32_t*>(0x89b564)=WorldArena+0x4000u+b.u32(0x4550);
        *reinterpret_cast<std::uint32_t*>(0x89b568)=b.u32(0x4554);*reinterpret_cast<std::uint32_t*>(0x89b56c)=b.u32(0x4558);
        std::memcpy(reinterpret_cast<void*>(0x7d2da0),f.image.data()+0x4400,64);
        std::memcpy(reinterpret_cast<void*>(0x7d3190),f.image.data()+0x4440,64);
        *reinterpret_cast<std::uint32_t*>(0x67d94c)=b.u32(0xf20);*reinterpret_cast<std::uint32_t*>(0x67d950)=b.u32(0xf24);
        *reinterpret_cast<float*>(0x7d3128)=b.f32(0x4638);*reinterpret_cast<float*>(0x7d317c)=b.f32(0x463c);
        std::memcpy(configured.data(),reinterpret_cast<void*>(0x780000),4096);
        std::memcpy(configured_matrix.data(),reinterpret_cast<void*>(0x89b000),4096);
    }
    void capture_globals(){
        Bytes b(reinterpret_cast<void*>(WorldArena),outrun::testing::world_image_size);
        for(unsigned k=0;k<16;++k)b.put32(0x4500+k*4,*reinterpret_cast<std::uint32_t*>(0x7801a8+k*4));
        b.put32(0x4540,*reinterpret_cast<std::uint32_t*>(0x780240));b.put32(0x4544,*reinterpret_cast<std::uint32_t*>(0x78023c));
        b.put32(0x4550,*reinterpret_cast<std::uint32_t*>(0x89b564)-(WorldArena+0x4000u));
        b.put32(0x4554,*reinterpret_cast<std::uint32_t*>(0x89b568));b.put32(0x4558,*reinterpret_cast<std::uint32_t*>(0x89b56c));
    }
    void compare(const std::string& name,outrun::testing::CourseWorldFixture& f){
        auto b=f.bytes();compare_world_bytes(name+"_memory",reinterpret_cast<void*>(WorldArena),f.image.data(),f.image.size());
        Bytes g(configured.data(),configured.size()),s(configured_matrix.data(),configured_matrix.size());
        for(unsigned k=0;k<16;++k)g.put32(0x1a8+k*4,b.u32(0x4500+k*4));
        g.put32(0x240,b.u32(0x4540));g.put32(0x23c,b.u32(0x4544));
        s.put32(0x564,WorldArena+0x4000u+b.u32(0x4550));s.put32(0x568,b.u32(0x4554));s.put32(0x56c,b.u32(0x4558));
        compare_world_bytes(name+"_globals",reinterpret_cast<void*>(0x780000),configured.data(),4096);
        compare_world_bytes(name+"_matrix_globals",reinterpret_cast<void*>(0x89b000),configured_matrix.data(),4096);
        for(unsigned t=0;t<4;++t)compare_world_bytes(name+"_grid",reinterpret_cast<void*>(WorldGrids+t*0x20000u),f.grids[t].data(),0x20000);
        compare_world_bytes(name+"_primary_transform",reinterpret_cast<void*>(0x7d2da0),f.image.data()+0x4400,64);
        compare_world_bytes(name+"_secondary_transform",reinterpret_cast<void*>(0x7d3190),f.image.data()+0x4440,64);
        compare_u32(name+"_lateral",*reinterpret_cast<std::uint32_t*>(0x67d94c),b.u32(0xf20));
        compare_u32(name+"_longitudinal",*reinterpret_cast<std::uint32_t*>(0x67d950),b.u32(0xf24));
    }
    ~WorldBinding(){
        std::memcpy(reinterpret_cast<void*>(0x780000),globals.data(),4096);
        std::memcpy(reinterpret_cast<void*>(0x89b000),matrix_globals.data(),4096);
        std::memcpy(reinterpret_cast<void*>(0x7d2da0),transforms.data(),64);
        std::memcpy(reinterpret_cast<void*>(0x7d3190),transforms.data()+64,64);
        *reinterpret_cast<std::uint32_t*>(0x67d94c)=lateral;*reinterpret_cast<std::uint32_t*>(0x67d950)=longitudinal;
        *reinterpret_cast<std::uint32_t*>(0x7d3128)=primary_yaw;*reinterpret_cast<std::uint32_t*>(0x7d317c)=secondary_yaw;
    }
};
std::uint32_t run_world_original(unsigned id){
    constexpr std::uint32_t entries[]={0x409ef0,0x40a010,0x40a170,0x40a840,0x40a7d0,0x43eb60,0x518e10,0,0x409f90,0x40a820,0x43d390,0x43e3b0,0,0,0,0x409f30,0x40a0a0,0x40a0d0,0x40a3e0,0x40a440,0x40a2d0};
    prepare(entries[id]);Bytes b(reinterpret_cast<void*>(WorldArena),outrun::testing::world_image_size),st(reinterpret_cast<void*>(S),64);
    if(id==2u||id==8u)st.put32(0,WorldArena+b.u32(0x4660));
    else if(id==3u||id==4u||id==9u){st.put32(0,WorldArena+0x4610);st.put32(4,WorldArena+0x4600);}
    else if(id==10u){st.put32(0,b.u32(0x4634));st.put32(4,b.u32(0x4630));st.put32(8,WorldArena+0x4610);}
    else if(id==11u){st.put32(0,WorldArena+0x4700);st.put32(4,WorldArena+0x4560);st.put32(8,b.u32(0x4670));}
    else if(id==17u){st.put32(0,WorldArena+0x4700);}
    else if(id==18u||id==19u){st.put32(0,b.u32(0x4674));}
    else if(id==20u){st.put32(0,WorldArena+0x4610);}
    else if(id>=5u&&id<=6u){
        unsigned offset=0;if(id==5u){st.put32(0,b.u32(0x4640));offset=4;}
        st.put32(offset,WorldArena+0x4600);const auto opts=b.u32(0x4644);
        for(unsigned k=0;k<3;++k)st.put32(offset+4+k*4,(opts&(1u<<k))?WorldArena+0x4620+k*4:0u);
    }
    run();return (id>=5u&&id<=6u)||id==11u?guest_call.out_eax:0u;
}
std::ofstream world_golden;std::uint32_t world_golden_count=0;
void world_u32(std::uint32_t v){char b[4];for(unsigned k=0;k<4;++k)b[k]=char(v>>(8*k));world_golden.write(b,4);}
void begin_world_golden(const char* path){
    world_golden.open(path,std::ios::binary|std::ios::trunc);if(!world_golden)throw std::runtime_error("world snapshot open");
    world_golden.write("OR2W016\0",8);world_u32(1);world_u32(x87_control);world_u32(0);
}
void finish_world_golden(){
    if(world_golden.is_open()){
        world_golden.seekp(16);world_u32(world_golden_count);world_golden.flush();
        if(!world_golden)throw std::runtime_error("world snapshot finalisation failed");
        world_golden.close();
    }
}
void run_world_cases(unsigned i,const std::string& only){
    if(!world_enabled(only))return;
    WorldNativeControl control(static_cast<unsigned short>(x87_control));
    for(unsigned id=0;id<56;++id){
        if(only=="wrecker_r022_all"){if(id<8u||id>14u)continue;}
        else if(only=="matrix_primitives_r023_all"){if(id<15u||id>17u)continue;}
        else if(only=="posture_primitives_r024_all"){if(id<18u||id>23u)continue;}
        else if(only!="all"&&only!="course_world_all"&&only!=world_names[id])continue;
        auto f=outrun::testing::make_course_world_fixture(i,id);WorldBinding bind(f);
        if(id==54u){
            std::array<std::uint8_t,0x400> event{},native_event{};
            std::mt19937 rng(0x46eb4036u^(i*1664525u));for(auto& v:event)v=std::uint8_t(rng());
            Bytes ee(event.data(),event.size());
            static constexpr float values[]={-1025.75f,-257.9f,-255.9f,-24.1f,-23.9f,-20.0f,-4.9f,-4.0f,-3.9f,-1.0f,-0.0f,0.0f,1.0f,3.9f,4.0f,4.1f,19.9f,20.0f,23.9f,24.0f,24.1f,127.9f,128.0f,255.9f,256.0f,1024.75f};
            ee.putf(0x268,values[i%(sizeof(values)/sizeof(values[0]))]);ee.put32(0x27c,(i%7u)==0u?1u:0u);native_event=event;
            std::memcpy(reinterpret_cast<void*>(WreckerArena),event.data(),event.size());prepare(0x46eb40u);Bytes st(reinterpret_cast<void*>(S),16);st.put32(0,WreckerArena);run();
            calc_disp_steering_angle_46eb40(Bytes(native_event.data(),native_event.size()));
            const std::string name=world_names[id];compare_world_bytes(name+"_event",reinterpret_cast<void*>(WreckerArena),native_event.data(),native_event.size());bind.compare(name,f);continue;
        }
        if(id==53u){
            std::array<std::uint8_t,0x400> event{},native_event{};
            std::mt19937 rng(0x4a25f036u^(i*1664525u));for(auto& v:event)v=std::uint8_t(rng());Bytes ee(event.data(),event.size());
            PcNightTunnelInputs inputs{(i&1u)!=0u,(i&2u)!=0u,static_cast<std::uint8_t>((i*37u)^((i>>3u)*11u))};native_event=event;
            set_night_tunnel_inputs(inputs);std::memcpy(reinterpret_cast<void*>(WreckerArena),event.data(),event.size());prepare(0x4a25f0u);Bytes st(reinterpret_cast<void*>(S),16);st.put32(0,WreckerArena);run();
            check_night_and_tunnel_4a25f0(Bytes(native_event.data(),native_event.size()),inputs);
            const std::string name=world_names[id];compare_world_bytes(name+"_event",reinterpret_cast<void*>(WreckerArena),native_event.data(),native_event.size());bind.compare(name,f);continue;
        }
        if(id==52u){
            auto native_f=f;auto fb=f.bytes();
            std::array<std::uint8_t,0x1800> event{},native_event{};
            std::array<std::uint8_t,0x800> work{},native_work{};
            std::array<std::uint8_t,0x2400> params{},native_params{};
            std::array<std::uint8_t,4*0xf4> wheels{},native_wheels{};
            std::mt19937 rng(0x4a7ec035u^(i*1664525u));
            for(auto& v:event)v=std::uint8_t(rng());for(auto& v:work)v=std::uint8_t(rng());for(auto& v:params)v=std::uint8_t(rng());for(auto& v:wheels)v=std::uint8_t(rng());
            Bytes ee(event.data(),event.size()),ww(work.data(),work.size()),pp(params.data(),params.size()),wh(wheels.data(),wheels.size());auto rf=[&](float q){return float(int(rng()%257u)-128)*q;};
            ee.put32(0x2b4,WreckerArena+0x3000u);ww.put32(0x248,WreckerArena+0x6000u);ww.put32(0x24c,WreckerArena+0x6000u+0xf4u);ww.put32(0x250,WreckerArena+0x6000u+2u*0xf4u);ww.put32(0x254,WreckerArena+0x6000u+3u*0xf4u);
            // Valid course place for the already-closed assCompulsiveMove child.
            for(unsigned k=0;k<16;++k)ee.put8(0x5c+k,fb.u8(0x4560+k));ee.put32(0x5c,0u);ee.puti(0x60,0);ee.put16(0x64,std::uint16_t(i%5u));ee.put16(0x66,0u);ee.puti(0x1c0,0);
            static constexpr std::int16_t angles[]={-0x6000,-0x3000,-0x1000,-1,0,1,0x1000,0x3000,0x6000};ee.put16(0x4e,std::uint16_t(angles[i%9u]));ee.put16(0x160,std::uint16_t(angles[(i/3u)%9u]));ee.put16(0xd46,std::uint16_t(angles[(i/7u)%9u]));ee.put16(0xd4a,std::uint16_t(angles[(i/11u)%9u]));
            ee.put32(0x1f4,(i&1u)?1u:0u);ee.putf(0xd54,float(int((i/2u)%9u)-4)*0.25f);static constexpr std::int8_t timers[]={-1,0,1,2,6,12,20};ee.put8(0xd36,std::uint8_t(timers[(i/5u)%7u]));
            pp.putf(0xf24,0.5f+float(i%7u)*0.125f);pp.putf(0xed8,0.25f+float((i/7u)%7u)*0.125f);ee.putf(0xdc4,0.25f+float((i/49u)%9u)*0.125f);
            static constexpr float move_gates[]={-1.0f,0.0f,0.00005f,0.0001f,0.0002f,0.25f};ee.putf(0x2c8,move_gates[(i/13u)%6u]);const std::uint32_t states[]={0u,2u,3u,5u,6u,7u};ee.put32(0x2f0,(ee.u32(0x2f0)&~0x7cu)|(states[(i/17u)%6u]<<2u));static constexpr float speed_states[]={-0.25f,0.0f,0.20f,0.23071244f,0.35f,0.691f,0.6921373f,0.8f};ee.putf(0x1c4,speed_states[(i/19u)%8u]);ee.putf(0xdbc,0.25f+float((i/23u)%9u)*0.25f);
            // Disable the already independently closed assist/rebound/wrecker dispatchers in this parent-domain oracle.
            ee.put32(0xe90,1u);ee.put8(0x283,0u);ee.put8(0x284,0u);ee.put16(0xd68,0u);
            // Inputs used by assCompulsiveMove.
            static constexpr float d0[]={-2.0f,-0.25f,0.0f,0.5f,2.0f};ee.putf(0x270,d0[i%5u]);float den=(i%17u==0u)?0.00005f:(i%17u==1u)?0.0001f:(0.25f+float((i/17u)%9u)*0.125f);ee.putf(0x274,ee.f32(0x270)+den);const bool equal=(i%7u)==0u;ee.putf(0x264,-2.0f+float((i/7u)%9u)*0.375f);ee.putf(0x268,equal?ee.f32(0x264):(1.5f+float((i/63u)%7u)*0.25f));ee.putf(0x26c,-2.5f+float((i/11u)%21u)*0.25f);ee.putf(0xd58,float((i/29u)%9u)*0.1f);
            pp.putf(0x20f4,0.2f+float(i%7u)*0.05f);pp.putf(0x2140,0.35f+float((i/7u)%7u)*0.04f);pp.putf(0x218c,0.45f+float((i/49u)%7u)*0.04f);pp.putf(0x21d8,0.5f+float((i/343u)%7u)*0.1f);pp.putf(0x2224,0.6f+float((i/13u)%7u)*0.08f);pp.putf(0x2270,0.7f+float((i/17u)%7u)*0.07f);
            // Stable rigid-body state for ActionForce2, while keeping non-zero forces and angular velocity.
            for(unsigned k=0;k<16;++k)ww.putf(0x10u+k*4u,0.0f);ww.putf(0x10,1.0f);ww.putf(0x24,1.0f);ww.putf(0x38,1.0f);ww.putf(0x4c,1.0f);ww.put32(0,1u);
            ww.putf(0x40,rf(0.125f));ww.putf(0x44,rf(0.0625f));ww.putf(0x48,rf(0.125f));ww.putf(0x50,rf(0.015625f));ww.putf(0x54,rf(0.015625f));ww.putf(0x58,rf(0.015625f));ww.putf(0x5c,0.5f+rf(0.03125f));ww.putf(0x60,rf(0.015625f));ww.putf(0x64,2.0f+rf(0.03125f));
            ww.putf(0x98,0.5f);ww.putf(0x9c,0.75f);ww.putf(0xa0,0.4f);ww.putf(0xa4,0.45f);ww.putf(0xa8,0.5f);ww.putf(0xac,0.6f);ww.putf(0xb0,0.65f);ww.putf(0xb4,0.7f);for(auto o:{0x68u,0x6cu,0x70u,0x74u,0x78u,0x7cu,0xc0u,0xc4u,0xc8u,0xccu,0xd0u,0xd4u})ww.putf(o,rf(0.03125f));
            ww.putf(0x14c,0.5f+rf(0.03125f));ww.putf(0x150,rf(0.015625f));ww.putf(0x154,2.0f+rf(0.03125f));float nx=float(int((i/31u)%7u)-3)*0.02f,nz=float(int((i/217u)%7u)-3)*0.02f,ny=1.0f,nl=std::sqrt(nx*nx+ny*ny+nz*nz);ww.putf(0x628,nx/nl);ww.putf(0x62c,ny/nl);ww.putf(0x630,nz/nl);for(unsigned k=0;k<3;++k){ww.putf(0x22cu+k*4u,rf(0.03125f));ww.putf(0x238u+k*4u,rf(0.03125f));}
            // Four valid tire records; no mode-dependent tire table lookup is needed.
            ee.put32(0x208,0u);ww.put8(0x440,0u);ww.put8(0x534,0u);for(unsigned t=0;t<4;++t){const std::size_t b=t*0xf4u;wh.put8(b,0u);wh.putf(b+0x04,float(t<2?-1:1)*(0.5f+0.1f*t));wh.putf(b+0x0c,float(t&1?-1:1)*(0.8f+0.05f*t));wh.putf(b+0x2c,-0.25f);for(auto o:{0x7cu,0x88u,0x94u,0xa0u}){wh.putf(b+o,rf(0.01f));wh.putf(b+o+4,rf(0.01f));wh.putf(b+o+8,rf(0.01f));}}
            native_event=event;native_work=work;native_params=params;native_wheels=wheels;
            const bool primary=(i%3u)!=0u,secondary=((i/3u)%2u)!=0u;const auto saved0=*reinterpret_cast<std::uint32_t*>(0x7d2e80u),saved1=*reinterpret_cast<std::uint32_t*>(0x7d2e88u);auto restore=[&]{*reinterpret_cast<std::uint32_t*>(0x7d2e80u)=saved0;*reinterpret_cast<std::uint32_t*>(0x7d2e88u)=saved1;};
            try{
                *reinterpret_cast<std::uint32_t*>(0x7d2e80u)=primary?10u:9u;*reinterpret_cast<std::uint32_t*>(0x7d2e88u)=secondary?20u:19u;std::memcpy(reinterpret_cast<void*>(WreckerArena),event.data(),event.size());std::memcpy(reinterpret_cast<void*>(WreckerArena+0x2000u),work.data(),work.size());std::memcpy(reinterpret_cast<void*>(WreckerArena+0x3000u),params.data(),params.size());std::memcpy(reinterpret_cast<void*>(WreckerArena+0x6000u),wheels.data(),wheels.size());
                prepare(0x4a7ec0u);guest_call.ecx=WreckerArena;guest_call.eax=WreckerArena+0x2000u;run();bind.capture_globals();
                auto tables=native_f.tables();auto stack=native_f.matrix();auto prediction=native_f.prediction();CourseWorldQuery query{tables,stack,prediction};PcRoadInfoContext road{tables,stack,{fb.f32(0x4638),fb.f32(0x463c)}};std::array<std::uint8_t,4> sd{},fd{};std::vector<Bytes> descriptors{Bytes(sd.data(),sd.size())};PcStageViews stages{Bytes(sd.data(),sd.size()),0,descriptors,Bytes(fd.data(),fd.size())};std::array<PcCourseEndView,4> ends={{{std::nullopt,tables.courses[0].runs.lengths},{std::nullopt,tables.courses[1].runs.lengths},{std::nullopt,tables.courses[2].runs.lengths},{std::nullopt,tables.courses[3].runs.lengths}}};for(unsigned t=0;t<4;++t)if(tables.courses[t].runs.present)ends[t].header=tables.courses[t].runs.header;std::array<std::uint8_t,8> cache{};std::array<std::uint8_t,56> choices{};std::array<std::uint8_t,0x34> selection{};std::array<std::uint8_t,0x400> save{};PcRouteContext route{Bytes(choices.data(),choices.size()),Bytes(selection.data(),selection.size()),Bytes(save.data(),save.size()),0,0,0,{0,0,0}};PcCourseAdvanceContext advance{ends,stages,Bytes(cache.data(),cache.size()),route,0,0,primary};PcAssCompulsiveMoveContext ass{advance,road,primary,secondary};PcDispMatrixContext display{stack,1.0f,3u};PcPlWreckerContext pl{advance,road,query,display};PcCalcPlBody2Context ctx{ass,road,stages,pl};std::array<Bytes,4> tires={Bytes(native_wheels.data(),0xf4u),Bytes(native_wheels.data()+0xf4u,0xf4u),Bytes(native_wheels.data()+2u*0xf4u,0xf4u),Bytes(native_wheels.data()+3u*0xf4u,0xf4u)};
                calc_pl_body_2nd_4a7ec0(Bytes(native_event.data(),native_event.size()),Bytes(native_work.data(),native_work.size()),Bytes(native_params.data(),native_params.size()),tires,Bytes(native_wheels.data(),native_wheels.size()),ctx);native_f.save(stack,prediction);
                const std::string name=world_names[id];compare_world_bytes(name+"_event",reinterpret_cast<void*>(WreckerArena),native_event.data(),native_event.size());compare_world_bytes(name+"_work",reinterpret_cast<void*>(WreckerArena+0x2000u),native_work.data(),native_work.size());compare_world_bytes(name+"_params",reinterpret_cast<void*>(WreckerArena+0x3000u),native_params.data(),native_params.size());compare_world_bytes(name+"_wheels",reinterpret_cast<void*>(WreckerArena+0x6000u),native_wheels.data(),native_wheels.size());bind.compare(name,native_f);
            }catch(...){restore();throw;}restore();continue;
        }
        if(id==51u){
            auto native_f=f;auto fb=f.bytes();
            std::array<std::uint8_t,0x1800> event{},native_event{};std::array<std::uint8_t,0x800> work{},native_work{};std::array<std::uint8_t,0x2400> params{},native_params{};
            std::mt19937 rng(0x5184b035u^(i*1664525u));for(auto& v:event)v=std::uint8_t(rng());for(auto& v:work)v=std::uint8_t(rng());for(auto& v:params)v=std::uint8_t(rng());Bytes ee(event.data(),event.size()),ww(work.data(),work.size()),pp(params.data(),params.size());auto rf=[&](float q){return float(int(rng()%257u)-128)*q;};
            ee.put32(0x2b4,WreckerArena+0x4000u);for(unsigned k=0;k<16;++k)ee.put8(0x5c+k,fb.u8(0x4560+k));ee.put32(0x5c,0u);ee.puti(0x60,0);ee.put16(0x64,std::uint16_t(i%5u));ee.put16(0x66,0u);ee.puti(0x1c0,0);
            // Progress/interpolation inputs, including the epsilon side of the ratio gate.
            static constexpr float d0[]={-2.0f,-0.25f,0.0f,0.5f,2.0f};ee.putf(0x270,d0[i%5u]);float den=(i%17u==0u)?0.00005f:(i%17u==1u)?0.0001f:(0.25f+float((i/17u)%9u)*0.125f);ee.putf(0x274,ee.f32(0x270)+den);
            const bool equal=(i%7u)==0u;ee.putf(0x264,-2.0f+float((i/7u)%9u)*0.375f);ee.putf(0x268,equal?ee.f32(0x264):(1.5f+float((i/63u)%7u)*0.25f));ee.putf(0x26c,-2.5f+float((i/11u)%21u)*0.25f);
            static constexpr std::int16_t ang[]={-0x6000,-0x4000,-0x1800,-0x400,-1,0,1,0x400,0x1800,0x4000,0x6000};ee.put16(0x160,std::uint16_t(ang[(i/3u)%11u]));ee.put16(0x4e,std::uint16_t(ang[(i/5u)%11u]/2));ee.put16(0xd46,std::uint16_t(ang[(i/9u)%11u]));ee.put16(0xd4a,std::uint16_t(ang[(i/13u)%11u]));ee.putf(0xd58,float((i/19u)%9u)*0.1f);static constexpr std::int8_t timers[]={0,1,2,6,12,20,-1};ee.put8(0xd36,std::uint8_t(timers[(i/23u)%7u]));
            // Chassis vectors: nonzero velocity, a unit road normal and modest force.
            ww.putf(0x14c,0.5f+rf(0.03125f));ww.putf(0x150,rf(0.015625f));ww.putf(0x154,2.0f+rf(0.03125f));ww.putf(0xc0,rf(0.0625f));ww.putf(0xc4,rf(0.03125f));ww.putf(0xc8,rf(0.0625f));float nx=float(int((i/29u)%7u)-3)*0.02f,nz=float(int((i/203u)%7u)-3)*0.02f,ny=1.0f,nl=std::sqrt(nx*nx+ny*ny+nz*nz);ww.putf(0x628,nx/nl);ww.putf(0x62c,ny/nl);ww.putf(0x630,nz/nl);
            pp.putf(0x20f4,0.2f+float(i%7u)*0.05f);pp.putf(0x2140,0.35f+float((i/7u)%7u)*0.04f);pp.putf(0x218c,0.45f+float((i/49u)%7u)*0.04f);pp.putf(0x21d8,0.5f+float((i/343u)%7u)*0.1f);pp.putf(0x2224,0.6f+float((i/13u)%7u)*0.08f);pp.putf(0x2270,0.7f+float((i/17u)%7u)*0.07f);
            native_event=event;native_work=work;native_params=params;
            const bool primary=(i%3u)!=0u,secondary=((i/3u)%2u)!=0u;const auto saved0=*reinterpret_cast<std::uint32_t*>(0x7d2e80u),saved1=*reinterpret_cast<std::uint32_t*>(0x7d2e88u);auto restore=[&]{*reinterpret_cast<std::uint32_t*>(0x7d2e80u)=saved0;*reinterpret_cast<std::uint32_t*>(0x7d2e88u)=saved1;};
            try{
                *reinterpret_cast<std::uint32_t*>(0x7d2e80u)=primary?10u:9u;*reinterpret_cast<std::uint32_t*>(0x7d2e88u)=secondary?20u:19u;std::memcpy(reinterpret_cast<void*>(WreckerArena),event.data(),event.size());std::memcpy(reinterpret_cast<void*>(WreckerArena+0x2000u),work.data(),work.size());std::memcpy(reinterpret_cast<void*>(WreckerArena+0x4000u),params.data(),params.size());
                prepare(0x5184b0u);Bytes st(reinterpret_cast<void*>(S),16);st.put32(0,WreckerArena);st.put32(4,WreckerArena+0x2000u);run();bind.capture_globals();
                auto tables=native_f.tables();auto stack=native_f.matrix();auto prediction=native_f.prediction();PcRoadInfoContext road{tables,stack,{fb.f32(0x4638),fb.f32(0x463c)}};std::array<std::uint8_t,4> sd{},fd{};std::vector<Bytes> descriptors{Bytes(sd.data(),sd.size())};PcStageViews stages{Bytes(sd.data(),sd.size()),0,descriptors,Bytes(fd.data(),fd.size())};std::array<PcCourseEndView,4> ends={{{std::nullopt,tables.courses[0].runs.lengths},{std::nullopt,tables.courses[1].runs.lengths},{std::nullopt,tables.courses[2].runs.lengths},{std::nullopt,tables.courses[3].runs.lengths}}};for(unsigned t=0;t<4;++t)if(tables.courses[t].runs.present)ends[t].header=tables.courses[t].runs.header;std::array<std::uint8_t,8> cache{};std::array<std::uint8_t,56> choices{};std::array<std::uint8_t,0x34> selection{};std::array<std::uint8_t,0x400> save{};PcRouteContext route{Bytes(choices.data(),choices.size()),Bytes(selection.data(),selection.size()),Bytes(save.data(),save.size()),0,0,0,{0,0,0}};PcCourseAdvanceContext advance{ends,stages,Bytes(cache.data(),cache.size()),route,0,0,primary};PcAssCompulsiveMoveContext ctx{advance,road,primary,secondary};
                ass_compulsive_move_5184b0(Bytes(native_event.data(),native_event.size()),Bytes(native_work.data(),native_work.size()),Bytes(native_params.data(),native_params.size()),ctx);native_f.save(stack,prediction);
                const std::string name=world_names[id];compare_world_bytes(name+"_event",reinterpret_cast<void*>(WreckerArena),native_event.data(),native_event.size());compare_world_bytes(name+"_work",reinterpret_cast<void*>(WreckerArena+0x2000u),native_work.data(),native_work.size());compare_world_bytes(name+"_params",reinterpret_cast<void*>(WreckerArena+0x4000u),native_params.data(),native_params.size());bind.compare(name,native_f);
            }catch(...){restore();throw;}restore();continue;
        }
        if(id==50u){
            auto native_f=f;auto fb=f.bytes();
            std::array<std::uint8_t,0x1800> event{},native_event{};
            std::array<std::uint8_t,0x800> work{},native_work{};
            std::array<std::uint8_t,0x1600> params{},native_params{};
            std::array<std::uint8_t,4*0xf4> wheels{},native_wheels{};
            std::mt19937 rng(0x4a0c7034u^(i*1664525u));for(auto& v:event)v=std::uint8_t(rng());for(auto& v:work)v=std::uint8_t(rng());for(auto& v:params)v=std::uint8_t(rng());for(auto& v:wheels)v=std::uint8_t(rng());
            Bytes ee(event.data(),event.size()),ww(work.data(),work.size()),pp(params.data(),params.size()),wh(wheels.data(),wheels.size());auto rf=[&](float q){return float(int(rng()%257u)-128)*q;};
            // Keep the immediate PlWrecker child valid when countdown reaches zero.
            ee.put32(0x2b4,WreckerArena+0x3000u);ww.put32(0x248,WreckerArena+0x5000u);ww.put32(0x24c,WreckerArena+0x5000u+0xf4u);ww.put32(0x250,WreckerArena+0x5000u+2u*0xf4u);ww.put32(0x254,WreckerArena+0x5000u+3u*0xf4u);
            pp.putf(0x260,float(int(i%11u)-5)*0.015625f);pp.putf(0xb48,0.35f+float(i%5u)*0.03125f);pp.putf(0xb94,0.45f+float((i/5u)%5u)*0.03125f);pp.put32(0x15f8,0xa5000000u^(i*2654435761u));ww.putf(0x224,float(int((i/25u)%9u)-4)*0.0078125f);for(unsigned k=0;k<4;++k)wh.putf(std::size_t(k)*0xf4u+0x08,0.75f+0.125f*k+float(i%7u)*0.0078125f);
            const unsigned family=i%8u;
            if(family==0u)ee.put16(0xd68,0u);else if(family==1u)ee.put16(0xd68,1u);else ee.put16(0xd68,std::uint16_t(2u+(i%23u)));
            ee.put16(0xd6a,std::uint16_t(12u+((i/7u)%32u)));ee.put16(0xd50,std::uint16_t(rng()));ee.put16(0xd44,std::uint16_t(rng()));ee.put16(0xd46,std::uint16_t(rng()));
            ee.put16(0x2e,std::uint16_t(std::int16_t(int(i*131u)%65536-32768)));ee.put16(0xd78,std::uint16_t(std::int16_t(int(i*173u)%65536-32768)));
            ee.putf(0x14,float(int(i%29u)-14)*1.5f);ee.putf(0x18,float(int((i/29u)%13u)-6)*0.5f);ee.putf(0x1c,float(int((i/377u)%23u)-11)*2.0f);
            ee.putf(0xd6c,ee.f32(0x14)+rf(0.25f));ee.putf(0xd70,ee.f32(0x18)+rf(0.125f));ee.putf(0xd74,ee.f32(0x1c)+rf(0.25f));
            // Valid course place used by the stage-special endpoint chooser.
            for(unsigned k=0;k<16;++k)ee.put8(0xd7c+k,fb.u8(0x4560+k));ee.puti(0x1c0,fb.i32(0x4670));
            const std::uint32_t states[]={0u,2u,5u,7u};ee.put32(0x2f0,(ee.u32(0x2f0)&~0x7cu)|(states[(i/3u)%4u]<<2u));
            for(auto o:{0x50u,0x54u,0x5cu,0x60u,0x64u,0x68u,0x6cu,0x70u,0x74u,0x78u,0x7cu,0xc0u,0xc4u,0xc8u,0xccu,0xd0u,0xd4u})ww.putf(o,rf(0.0625f));
            // Special stage branch coverage plus ordinary-stage controls.
            const auto stage_key=(family==1u)?0u:(0x60000000u+(i%31u));ee.put32(0x68,stage_key);const unsigned sm=(i/8u)%4u;const std::int32_t stage_number=sm==0u?0x1c:sm==1u?0x12:7+std::int32_t(sm);
            if(family==1u){ee.put32(0x5c,0u);ee.puti(0x60,0);ee.put16(0x64,0u);ee.put16(0x66,0u);ee.put32(0x68,0u);ee.puti(0x1c0,0);}
            else if(sm<2u){ee.put32(0x5c,0u);ee.put32(0x04,ee.u32(0x04)|0x00020000u);}else{ee.put32(0x04,ee.u32(0x04)&~0x00020000u);}
            // PlWrecker display branches.
            const std::uint32_t display_flags[]={0u,0x00800000u,0x80000000u,0x80800000u};ee.put32(4,(ee.u32(4)&~0x80800000u)|display_flags[(i/4u)%4u]);ee.putf(0x1c4,0.05f+float(i%37u)*0.03125f);ee.putf(0xdb4,float((i/37u)%9u)*0.125f);ee.putf(0xd24,(i%4u==0u)?0.0f:(i%4u==1u)?1.1920928955078125e-7f:(i%4u==2u)?0.125f:-0.25f);
            auto vec=[&](std::size_t o,float k){ee.putf(o,k);ee.putf(o+4,-k*0.5f);ee.putf(o+8,k*0.25f);};vec(0x2d8,float(int(i%9u)-4)*0.03125f);vec(0x1040,float(int((i/9u)%9u)-4)*0.025f);vec(0x1034,float(int((i/81u)%9u)-4)*0.02f);vec(0x2e4,float(int((i/729u)%9u)-4)*0.018f);
            native_event=event;native_work=work;native_params=params;native_wheels=wheels;
            std::array<std::uint8_t,0x78> stage_records{};std::array<std::uint8_t,4> stage_desc{},fallback_desc{};Bytes sr(stage_records.data(),stage_records.size()),sd(stage_desc.data(),stage_desc.size()),fd(fallback_desc.data(),fallback_desc.size());sr.put32(4,stage_key);sd.puti(0,stage_number);fd.puti(0,33);sr.put32(0x14,WreckerArena+0x6100u);
            std::array<std::uint8_t,4096> g7d3{};std::memcpy(g7d3.data(),reinterpret_cast<void*>(0x7d3000),4096);const auto saved_mode=*reinterpret_cast<std::uint32_t*>(0x82e7d8u),saved_blend=*reinterpret_cast<std::uint32_t*>(0x634b34u);const auto saved_scene=*reinterpret_cast<std::uint8_t*>(0x82e7d4u);auto restore=[&]{std::memcpy(reinterpret_cast<void*>(0x7d3000),g7d3.data(),4096);*reinterpret_cast<std::uint32_t*>(0x82e7d8u)=saved_mode;*reinterpret_cast<std::uint32_t*>(0x634b34u)=saved_blend;*reinterpret_cast<std::uint8_t*>(0x82e7d4u)=saved_scene;};
            try{
                std::memcpy(reinterpret_cast<void*>(WreckerArena),event.data(),event.size());std::memcpy(reinterpret_cast<void*>(WreckerArena+0x2000u),work.data(),work.size());std::memcpy(reinterpret_cast<void*>(WreckerArena+0x3000u),params.data(),params.size());std::memcpy(reinterpret_cast<void*>(WreckerArena+0x5000u),wheels.data(),wheels.size());std::memcpy(reinterpret_cast<void*>(WreckerArena+0x6000u),stage_records.data(),stage_records.size());std::memcpy(reinterpret_cast<void*>(WreckerArena+0x6100u),stage_desc.data(),stage_desc.size());std::memcpy(reinterpret_cast<void*>(WreckerArena+0x6200u),fallback_desc.data(),fallback_desc.size());*reinterpret_cast<std::uint32_t*>(0x7d33bc)=WreckerArena+0x6000u;*reinterpret_cast<std::uint32_t*>(0x7d33c4)=1u;*reinterpret_cast<std::uint32_t*>(0x7d2df4)=WreckerArena+0x6200u;
                *reinterpret_cast<std::uint32_t*>(0x82e7d8u)=(i%5u==0u)?1u:0u;*reinterpret_cast<std::uint8_t*>(0x82e7d4u)=static_cast<std::uint8_t>((i%6u==0u)?6u:(i%6u==1u)?15u:(i%6u==2u)?17u:3u);static constexpr float blends[]={0.0f,0.125f,0.25f,0.5f,0.75f,0.875f,1.0f};*reinterpret_cast<float*>(0x634b34u)=blends[(i/5u)%7u];prepare(0x4493e0u);guest_call.st0=1;run();const float blend=ffrom(guest_call.out_st0);const auto scene=*reinterpret_cast<std::uint8_t*>(0x82e7d4u);
                prepare(0x4a0c70u);guest_call.esi=WreckerArena;guest_call.edi=WreckerArena+0x2000u;run();bind.capture_globals();
                auto tables=native_f.tables();auto stack=native_f.matrix();auto prediction=native_f.prediction();CourseWorldQuery query{tables,stack,prediction};PcRoadInfoContext road{tables,stack,{fb.f32(0x4638),fb.f32(0x463c)}};PcDispMatrixContext display{stack,blend,scene};std::vector<Bytes> descriptors{Bytes(stage_desc.data(),stage_desc.size())};PcStageViews stages{Bytes(stage_records.data(),stage_records.size()),1,descriptors,Bytes(fallback_desc.data(),fallback_desc.size())};std::array<PcCourseEndView,4> ends={{{std::nullopt,tables.courses[0].runs.lengths},{std::nullopt,tables.courses[1].runs.lengths},{std::nullopt,tables.courses[2].runs.lengths},{std::nullopt,tables.courses[3].runs.lengths}}};for(unsigned t=0;t<4;++t)if(tables.courses[t].runs.present)ends[t].header=tables.courses[t].runs.header;std::array<std::uint8_t,8> cache{};std::array<std::uint8_t,56> choices{};std::array<std::uint8_t,0x34> selection{};std::array<std::uint8_t,0x400> save{};PcRouteContext route{Bytes(choices.data(),choices.size()),Bytes(selection.data(),selection.size()),Bytes(save.data(),save.size()),0,0,0,{0,0,0}};PcCourseAdvanceContext advance{ends,stages,Bytes(cache.data(),cache.size()),route,0,0,false};PcPlWreckerContext pl{advance,road,query,display};
                wrec_pl_body_4a0c70(Bytes(native_event.data(),native_event.size()),Bytes(native_work.data(),native_work.size()),Bytes(native_params.data(),native_params.size()),Bytes(native_wheels.data(),native_wheels.size()),pl);native_f.save(stack,prediction);
                const std::string name=world_names[id];compare_world_bytes(name+"_event",reinterpret_cast<void*>(WreckerArena),native_event.data(),native_event.size());compare_world_bytes(name+"_work",reinterpret_cast<void*>(WreckerArena+0x2000u),native_work.data(),native_work.size());compare_world_bytes(name+"_params",reinterpret_cast<void*>(WreckerArena+0x3000u),native_params.data(),native_params.size());compare_world_bytes(name+"_wheels",reinterpret_cast<void*>(WreckerArena+0x5000u),native_wheels.data(),native_wheels.size());bind.compare(name,native_f);
            }catch(...){restore();throw;}restore();continue;
        }
        if(id==49u){
            auto native_f=f;auto fb=f.bytes();std::array<std::uint8_t,0x1000> event{},native_event{};std::array<std::uint8_t,0x800> work{},native_work{};
            std::mt19937 rng(0x4a62e033u^(i*1664525u));for(auto& v:event)v=std::uint8_t(rng());for(auto& v:work)v=std::uint8_t(rng());Bytes ee(event.data(),event.size()),ww(work.data(),work.size());auto rf=[&](float q){return float(int(rng()%257u)-128)*q;};
            for(unsigned k=0;k<16;++k)ww.putf(0x10u+k*4u,0.0f);ww.putf(0x10,1.0f);ww.putf(0x24,1.0f);ww.putf(0x38,1.0f);ww.putf(0x4c,1.0f);const float yaw=float(int(i%37u)-18)*0.01f,c=std::cos(yaw),sn=std::sin(yaw);ww.putf(0x10,c);ww.putf(0x18,-sn);ww.putf(0x30,sn);ww.putf(0x38,c);
            float nx=float(int((i/37u)%7u)-3)*0.03125f,nz=float(int((i/259u)%7u)-3)*0.03125f,ny=1.0f;const float nl=std::sqrt(nx*nx+ny*ny+nz*nz);ww.putf(0x628,nx/nl);ww.putf(0x62c,ny/nl);ww.putf(0x630,nz/nl);for(auto o:{0x5cu,0x60u,0x64u,0x68u,0x6cu,0x70u,0xc0u,0xc4u,0xc8u,0xccu,0xd0u,0xd4u})ww.putf(o,rf(0.0625f));
            static constexpr std::int16_t ang[]={-0x6000,-0x3000,-0x800,-1,0,1,0x800,0x3000,0x6000};ee.put16(0xd4c,std::uint16_t(ang[i%9u]));ee.put16(0xd4e,std::uint16_t(ang[(i/3u)%9u]));ee.put16(0x2e,std::uint16_t(ang[(i/9u)%9u]));ee.put16(0x286,std::uint16_t(ang[(i/27u)%9u]));ee.put16(0x288,std::uint16_t(ang[(i/81u)%9u]));
            const unsigned family=i%8u;if(family==0u){ee.put8(0x283,0u);ee.put8(0x284,std::uint8_t((i/8u)%5u));}else{ee.put8(0x283,std::uint8_t(1u+(i%23u)));ee.put8(0x284,3u);}ee.put32(0x290,family==1u?1u:family==2u?2u:family==3u?0u:family==4u?3u:1u);
            ee.putf(0x26c,4.0f+float((i/11u)%9u)*0.5f);ee.putf(0x264,-2.0f+float((i/13u)%7u)*0.5f);ee.putf(0x268,2.0f+float((i/17u)%7u)*0.5f);ee.putf(0x28c,0.25f+float((i/19u)%17u)*0.25f);ee.putf(0x14,float(int(i%19u)-9)*2.0f);ee.putf(0x18,float(int((i/19u)%11u)-5));ee.putf(0x1c,float(int((i/209u)%17u)-8)*3.0f);ee.putf(0xd5c,ee.f32(0x14)+rf(0.125f));ee.putf(0xd60,ee.f32(0x18)+rf(0.0625f));ee.putf(0xd64,ee.f32(0x1c)+rf(0.125f));ee.puti(0x1c0,fb.i32(0x4670));
            ee.put8(0x06,std::uint8_t(ee.u8(0x06)&~1u));if((i/8u)&1u){for(unsigned k=0;k<16;++k)ee.put8(0x5c+k,fb.u8(0x4560+k));ee.put16(0x64,0u);ee.puti(0x1c0,fb.i32(0x4670));}else{ee.put32(0x5c,0u);ee.put16(0x64,60u);}
            const auto stage_key=ee.u32(0x68);std::array<std::uint8_t,0x78> stage_records{};std::array<std::uint8_t,4> stage_desc{},fallback_desc{};Bytes sr(stage_records.data(),stage_records.size()),sd(stage_desc.data(),stage_desc.size()),fd(fallback_desc.data(),fallback_desc.size());sr.put32(4,stage_key);sd.puti(0,9);fd.puti(0,33);native_event=event;native_work=work;
            std::array<std::uint8_t,4096> g7d3{};std::memcpy(g7d3.data(),reinterpret_cast<void*>(0x7d3000),4096);auto restore=[&]{std::memcpy(reinterpret_cast<void*>(0x7d3000),g7d3.data(),4096);};
            try{sr.put32(0x14,WreckerArena+0x5100u);std::memcpy(reinterpret_cast<void*>(WreckerArena),event.data(),event.size());std::memcpy(reinterpret_cast<void*>(WreckerArena+0x2000u),work.data(),work.size());std::memcpy(reinterpret_cast<void*>(WreckerArena+0x5000u),stage_records.data(),stage_records.size());std::memcpy(reinterpret_cast<void*>(WreckerArena+0x5100u),stage_desc.data(),stage_desc.size());std::memcpy(reinterpret_cast<void*>(WreckerArena+0x5200u),fallback_desc.data(),fallback_desc.size());*reinterpret_cast<std::uint32_t*>(0x7d33bc)=WreckerArena+0x5000u;*reinterpret_cast<std::uint32_t*>(0x7d33c4)=1u;*reinterpret_cast<std::uint32_t*>(0x7d2df4)=WreckerArena+0x5200u;
                prepare(0x4a62e0u);guest_call.eax=WreckerArena+0x2000u;guest_call.ecx=WreckerArena;run();auto stack=native_f.matrix();auto tables=native_f.tables();PcRoadInfoContext road{tables,stack,{fb.f32(0x4638),fb.f32(0x463c)}};std::vector<Bytes> descriptors{Bytes(stage_desc.data(),stage_desc.size())};PcStageViews stages{Bytes(stage_records.data(),stage_records.size()),1,descriptors,Bytes(fallback_desc.data(),fallback_desc.size())};reb_pl_body_4a62e0(Bytes(native_event.data(),native_event.size()),Bytes(native_work.data(),native_work.size()),road,stages);native_f.save(stack,native_f.prediction());bind.capture_globals();const std::string name=world_names[id];compare_world_bytes(name+"_event",reinterpret_cast<void*>(WreckerArena),native_event.data(),native_event.size());compare_world_bytes(name+"_work",reinterpret_cast<void*>(WreckerArena+0x2000u),native_work.data(),native_work.size());bind.compare(name,native_f);
            }catch(...){restore();throw;}restore();continue;
        }
        if(id==48u){
            auto native_f=f;auto fb=f.bytes();
            std::array<std::uint8_t,0x1000> event{},native_event{};
            std::array<std::uint8_t,0x800> work{},native_work{};
            std::mt19937 rng(0x4a032033u^(i*1664525u));for(auto& v:event)v=std::uint8_t(rng());for(auto& v:work)v=std::uint8_t(rng());
            Bytes ee(event.data(),event.size()),ww(work.data(),work.size());auto rf=[&](float q){return float(int(rng()%257u)-128)*q;};
            for(unsigned k=0;k<16;++k)ww.putf(0x10u+k*4u,0.0f);ww.putf(0x10,1.0f);ww.putf(0x24,1.0f);ww.putf(0x38,1.0f);ww.putf(0x4c,1.0f);
            const float yaw=float(int(i%41u)-20)*0.009375f,c=std::cos(yaw),sn=std::sin(yaw);ww.putf(0x10,c);ww.putf(0x18,-sn);ww.putf(0x30,sn);ww.putf(0x38,c);
            float nx=float(int((i/41u)%9u)-4)*0.03125f,nz=float(int((i/369u)%9u)-4)*0.03125f,ny=1.0f;const float nl=std::sqrt(nx*nx+ny*ny+nz*nz);nx/=nl;ny/=nl;nz/=nl;
            ww.putf(0x628,nx);ww.putf(0x62c,ny);ww.putf(0x630,nz);
            ww.putf(0x5c,rf(0.0625f));ww.putf(0x60,rf(0.03125f));ww.putf(0x64,rf(0.0625f));
            static constexpr std::int16_t ang[]={-0x7000,-0x4000,-0x1000,-1,0,1,0x1000,0x4000,0x7000};
            ee.put16(0xd4c,std::uint16_t(ang[i%9u]));ee.put16(0xd4e,std::uint16_t(ang[(i/3u)%9u]));ee.put16(0x2e,std::uint16_t(ang[(i/9u)%9u]));ee.put16(0x286,std::uint16_t(ang[(i/27u)%9u]));
            const std::uint8_t count=std::uint8_t(1u+(i%31u));ee.put8(0x283,count);ee.put32(0x290,2u);
            static constexpr float denoms[]={-8.0f,-4.0f,-0.5f,0.0f,0.5f,4.0f,8.0f};ee.putf(0x26c,denoms[(i/5u)%7u]);ee.putf(0x264,-3.0f+float((i/13u)%13u)*0.5f);ee.putf(0x268,3.0f+float((i/17u)%13u)*0.5f);
            ee.putf(0x14,float(int(i%19u)-9)*2.0f);ee.putf(0x18,float(int((i/19u)%11u)-5));ee.putf(0x1c,float(int((i/209u)%17u)-8)*3.0f);
            ee.putf(0xd5c,ee.f32(0x14)+rf(0.125f));ee.putf(0xd60,ee.f32(0x18)+rf(0.0625f));ee.putf(0xd64,ee.f32(0x1c)+rf(0.125f));ee.puti(0x1c0,fb.i32(0x4670));
            const bool base=(i%11u)==0u;ee.put8(0x06,std::uint8_t((ee.u8(0x06)&~1u)|(base?1u:0u)));
            if(!base && ((i/11u)&1u)){for(unsigned k=0;k<16;++k)ee.put8(0x5c+k,fb.u8(0x4560+k));ee.put16(0x64,0u);ee.puti(0x1c0,fb.i32(0x4670));}
            else if(!base){ee.put32(0x5c,0u);ee.put16(0x64,60u);}
            const auto stage_key=ee.u32(0x68);const int stage_mode=int((i/37u)%4u);const std::int32_t stage_number=stage_mode==0?0x1c:stage_mode==1?0x12:7+stage_mode;
            if(stage_mode<2){ee.put32(0x04,ee.u32(0x04)|0x00020000u);if((i/148u)&1u)ee.put32(0x5c,0u);}else ee.put32(0x04,ee.u32(0x04)&~0x00020000u);
            native_event=event;native_work=work;
            std::array<std::uint8_t,0x78> stage_records{};std::array<std::uint8_t,4> stage_desc{},fallback_desc{};Bytes sr(stage_records.data(),stage_records.size()),sd(stage_desc.data(),stage_desc.size()),fd(fallback_desc.data(),fallback_desc.size());sr.put32(4,stage_key);sd.puti(0,stage_number);fd.puti(0,33);
            std::array<std::uint8_t,4096> g7d3{};std::memcpy(g7d3.data(),reinterpret_cast<void*>(0x7d3000),4096);auto restore=[&]{std::memcpy(reinterpret_cast<void*>(0x7d3000),g7d3.data(),4096);};
            try{
                sr.put32(0x14,WreckerArena+0x5100u);std::memcpy(reinterpret_cast<void*>(WreckerArena),event.data(),event.size());std::memcpy(reinterpret_cast<void*>(WreckerArena+0x2000u),work.data(),work.size());std::memcpy(reinterpret_cast<void*>(WreckerArena+0x5000u),stage_records.data(),stage_records.size());std::memcpy(reinterpret_cast<void*>(WreckerArena+0x5100u),stage_desc.data(),stage_desc.size());std::memcpy(reinterpret_cast<void*>(WreckerArena+0x5200u),fallback_desc.data(),fallback_desc.size());
                *reinterpret_cast<std::uint32_t*>(0x7d33bc)=WreckerArena+0x5000u;*reinterpret_cast<std::uint32_t*>(0x7d33c4)=1u;*reinterpret_cast<std::uint32_t*>(0x7d2df4)=WreckerArena+0x5200u;
                prepare(0x4a0320u);guest_call.eax=WreckerArena;Bytes st(reinterpret_cast<void*>(S),16);st.put32(0,WreckerArena+0x2000u);run();
                auto stack=native_f.matrix();auto tables=native_f.tables();PcRoadInfoContext road{tables,stack,{fb.f32(0x4638),fb.f32(0x463c)}};std::vector<Bytes> descriptors{Bytes(stage_desc.data(),stage_desc.size())};PcStageViews stages{Bytes(stage_records.data(),stage_records.size()),1,descriptors,Bytes(fallback_desc.data(),fallback_desc.size())};
                try{reb_pl_body_sub2_4a0320(Bytes(native_event.data(),native_event.size()),Bytes(native_work.data(),native_work.size()),road,stages);}catch(const std::exception& ex){throw std::runtime_error(std::string("reb_pl_body_sub2 case=")+std::to_string(i)+": "+ex.what());}
                native_f.save(stack,native_f.prediction());bind.capture_globals();const std::string name=world_names[id];compare_world_bytes(name+"_event",reinterpret_cast<void*>(WreckerArena),native_event.data(),native_event.size());compare_world_bytes(name+"_work",reinterpret_cast<void*>(WreckerArena+0x2000u),native_work.data(),native_work.size());bind.compare(name,native_f);
            }catch(...){restore();throw;}restore();continue;
        }
        if(id==47u){
            auto native_f=f;auto fb=f.bytes();
            std::array<std::uint8_t,0x1000> event{},native_event{};
            std::array<std::uint8_t,0x800> work{},native_work{};
            std::mt19937 rng(0x4a08f033u^(i*1664525u));for(auto& v:event)v=std::uint8_t(rng());for(auto& v:work)v=std::uint8_t(rng());
            Bytes ee(event.data(),event.size()),ww(work.data(),work.size());auto rf=[&](float q){return float(int(rng()%257u)-128)*q;};
            // Stable pose and a unit-ish road normal.  Varying the normal
            // keeps the two projection stages live without feeding the PC
            // impossible geometry.
            for(unsigned k=0;k<16;++k)ww.putf(0x10u+k*4u,0.0f);ww.putf(0x10,1.0f);ww.putf(0x24,1.0f);ww.putf(0x38,1.0f);ww.putf(0x4c,1.0f);
            const float yaw=float(int(i%41u)-20)*0.009375f,c=std::cos(yaw),sn=std::sin(yaw);ww.putf(0x10,c);ww.putf(0x18,-sn);ww.putf(0x30,sn);ww.putf(0x38,c);
            float nx=float(int((i/41u)%9u)-4)*0.03125f,nz=float(int((i/369u)%9u)-4)*0.03125f,ny=1.0f;const float nl=std::sqrt(nx*nx+ny*ny+nz*nz);nx/=nl;ny/=nl;nz/=nl;
            ww.putf(0x628,nx);ww.putf(0x62c,ny);ww.putf(0x630,nz);
            ww.putf(0x5c,rf(0.0625f));ww.putf(0x60,rf(0.03125f));ww.putf(0x64,rf(0.0625f));
            static constexpr std::int16_t ang[]={-0x7000,-0x4000,-0x1000,-1,0,1,0x1000,0x4000,0x7000};
            ee.put16(0xd4c,std::uint16_t(ang[i%9u]));ee.put16(0xd4e,std::uint16_t(ang[(i/3u)%9u]));ee.put16(0x2e,std::uint16_t(ang[(i/9u)%9u]));
            ee.put16(0x286,std::uint16_t(ang[(i/27u)%9u]));ee.put16(0x288,std::uint16_t(ang[(i/81u)%9u]));
            const std::uint8_t count=std::uint8_t(1u+(i%31u));ee.put8(0x283,count);ee.put32(0x290,2u);
            static constexpr float speeds[]={0.0f,0.03125f,0.25f,1.0f,4.0f,16.0f,64.0f};ee.putf(0x28c,speeds[(i/5u)%7u]);
            ee.putf(0x14,float(int(i%19u)-9)*2.0f);ee.putf(0x18,float(int((i/19u)%11u)-5));ee.putf(0x1c,float(int((i/209u)%17u)-8)*3.0f);
            ee.putf(0xd5c,ee.f32(0x14)+rf(0.125f));ee.putf(0xd60,ee.f32(0x18)+rf(0.0625f));ee.putf(0xd64,ee.f32(0x1c)+rf(0.125f));
            ee.puti(0x1c0,fb.i32(0x4670));
            // Roughly one seventh exercises Sub3's explicit fallback to the
            // already-closed base helper.  The remaining cases alternate
            // direct target deltas and the real 100/101 road-centre chooser.
            const bool base=(i%7u)==0u;ee.put8(0x06,std::uint8_t((ee.u8(0x06)&~1u)|(base?1u:0u)));
            if(!base && ((i/7u)&1u)){
                for(unsigned k=0;k<16;++k)ee.put8(0x5c+k,fb.u8(0x4560+k));
                ee.put16(0x64,0u);ee.puti(0x1c0,fb.i32(0x4670));
            }else if(!base){ee.put32(0x5c,0u);ee.put16(0x64,60u);}
            native_event=event;native_work=work;
            std::memcpy(reinterpret_cast<void*>(WreckerArena),event.data(),event.size());std::memcpy(reinterpret_cast<void*>(WreckerArena+0x2000u),work.data(),work.size());
            prepare(0x4a08f0u);guest_call.eax=WreckerArena+0x2000u;guest_call.ecx=WreckerArena;run();
            auto stack=native_f.matrix();auto tables=native_f.tables();PcRoadInfoContext road{tables,stack,{fb.f32(0x4638),fb.f32(0x463c)}};
            try{reb_pl_body_sub3_4a08f0(Bytes(native_event.data(),native_event.size()),Bytes(native_work.data(),native_work.size()),road);}
            catch(const std::exception& ex){throw std::runtime_error(std::string("reb_pl_body_sub3 case=")+std::to_string(i)+": "+ex.what());}
            native_f.save(stack,native_f.prediction());bind.capture_globals();const std::string name=world_names[id];
            compare_world_bytes(name+"_event",reinterpret_cast<void*>(WreckerArena),native_event.data(),native_event.size());compare_world_bytes(name+"_work",reinterpret_cast<void*>(WreckerArena+0x2000u),native_work.data(),native_work.size());bind.compare(name,native_f);continue;
        }
        if(id==46u){
            auto native_f=f;
            std::array<std::uint8_t,0x1000> event{},native_event{};
            std::array<std::uint8_t,0x800> work{},native_work{};
            std::mt19937 rng(0x4a020032u^(i*1664525u));for(auto& v:event)v=std::uint8_t(rng());for(auto& v:work)v=std::uint8_t(rng());
            Bytes ee(event.data(),event.size()),ww(work.data(),work.size());auto rf=[&](float q){return float(int(rng()%257u)-128)*q;};
            for(unsigned k=0;k<16;++k)ww.putf(0x10u+k*4u,0.0f);ww.putf(0x10,1.0f);ww.putf(0x24,1.0f);ww.putf(0x38,1.0f);ww.putf(0x4c,1.0f);
            const float yaw=float(int(i%37u)-18)*0.01f,c=std::cos(yaw),sn=std::sin(yaw);ww.putf(0x10,c);ww.putf(0x18,-sn);ww.putf(0x30,sn);ww.putf(0x38,c);
            ww.putf(0x5c,rf(0.0625f));ww.putf(0x60,rf(0.03125f));ww.putf(0x64,rf(0.0625f));
            static constexpr std::int16_t a[]={-0x7000,-0x3000,-0x800,-1,0,1,0x800,0x3000,0x7000};
            ee.put16(0x286,std::uint16_t(a[i%9u]));ee.put16(0x288,std::uint16_t(a[(i/3u)%9u]));ee.put16(0x2e,std::uint16_t(a[(i/9u)%9u]));ee.put16(0x294,std::uint16_t(a[(i/27u)%9u]));
            static constexpr std::uint32_t flags[]={0u,2u,0x100000u,0x200000u,0xf00002u};ee.put32(0x244,flags[(i/81u)%5u]);
            native_event=event;native_work=work;std::memcpy(reinterpret_cast<void*>(WreckerArena),event.data(),event.size());std::memcpy(reinterpret_cast<void*>(WreckerArena+0x2000u),work.data(),work.size());
            prepare(0x4a0200u);guest_call.ebx=WreckerArena+0x2000u;guest_call.edi=WreckerArena;run();
            auto stack=native_f.matrix();reb_pl_body_base_4a0200(Bytes(native_event.data(),native_event.size()),Bytes(native_work.data(),native_work.size()),stack);native_f.save(stack,native_f.prediction());
            bind.capture_globals();const std::string name=world_names[id];compare_world_bytes(name+"_event",reinterpret_cast<void*>(WreckerArena),native_event.data(),native_event.size());compare_world_bytes(name+"_work",reinterpret_cast<void*>(WreckerArena+0x2000u),native_work.data(),native_work.size());bind.compare(name,native_f);continue;
        }
        if(id==45u){
            auto native_f=f;
            std::array<std::uint8_t,0x1000> event{},native_event{};std::array<std::uint8_t,0x800> work{},native_work{};
            std::mt19937 rng(0x4a547032u^(i*1664525u));for(auto& v:event)v=std::uint8_t(rng());for(auto& v:work)v=std::uint8_t(rng());Bytes ee(event.data(),event.size()),ww(work.data(),work.size());auto rf=[&](float q){return float(int(rng()%257u)-128)*q;};
            for(unsigned k=0;k<16;++k)ww.putf(0x10u+k*4u,0.0f);ww.putf(0x10,1.0f);ww.putf(0x24,1.0f);ww.putf(0x38,1.0f);ww.putf(0x4c,1.0f);
            const float yaw=float(int(i%31u)-15)*0.0125f,c=std::cos(yaw),sn=std::sin(yaw);ww.putf(0x10,c);ww.putf(0x18,-sn);ww.putf(0x30,sn);ww.putf(0x38,c);
            ww.putf(0x5c,rf(0.0625f));ww.putf(0x60,rf(0.03125f));ww.putf(0x64,rf(0.0625f));ww.putf(0x628,0.0f);ww.putf(0x62c,1.0f);ww.putf(0x630,0.0f);
            static constexpr std::int32_t dur[]={10,29,30,31,60,61,120,121,300};const auto d=dur[i%9u];ee.puti(0xe8c,d);const int half=(d-(d>>31))/2;ee.puti(0xe88,(i%13u)==0u?0:std::max(1,(half>0?int((i/9u)%unsigned(half*2+1)):1)));ee.put32(0xe90,(i%17u)==0u?1u:0u);
            ee.putf(0xe98,0.25f+float((i/17u)%17u)*0.0625f);ee.putf(0xe94,((i/7u)&1u)?1.0f:-1.0f);
            static constexpr std::int16_t ang[]={-0x6000,-0x3000,-0x800,-1,0,1,0x800,0x3000,0x6000};ee.put16(0xd4c,std::uint16_t(ang[(i/5u)%9u]));ee.put16(0xd4e,std::uint16_t(ang[(i/11u)%9u]));ee.put16(0x160,std::uint16_t(ang[(i/23u)%9u]));ee.put16(0x2e,std::uint16_t(ang[(i/47u)%9u]));
            native_event=event;native_work=work;std::memcpy(reinterpret_cast<void*>(WreckerArena),event.data(),event.size());std::memcpy(reinterpret_cast<void*>(WreckerArena+0x2000u),work.data(),work.size());
            prepare(0x4a5470u);guest_call.ebx=WreckerArena;Bytes st(reinterpret_cast<void*>(S),16);st.put32(0,WreckerArena+0x2000u);run();auto stack=native_f.matrix();assist_wanderer_4a5470(Bytes(native_event.data(),native_event.size()),Bytes(native_work.data(),native_work.size()),stack);native_f.save(stack,native_f.prediction());
            bind.capture_globals();const std::string name=world_names[id];compare_world_bytes(name+"_event",reinterpret_cast<void*>(WreckerArena),native_event.data(),native_event.size());compare_world_bytes(name+"_work",reinterpret_cast<void*>(WreckerArena+0x2000u),native_work.data(),native_work.size());bind.compare(name,native_f);continue;
        }
        if(id==44u){
            auto native_f=f;
            std::array<std::uint8_t,0x1000> event{},native_event{};std::array<std::uint8_t,0x800> work{},native_work{};std::array<std::uint8_t,0x2600> params{},native_params{};std::array<std::uint8_t,4*0xb0> tires{},native_tires{};
            std::mt19937 rng(0x4a000032u^(i*1664525u));for(auto& v:event)v=std::uint8_t(rng());for(auto& v:work)v=std::uint8_t(rng());for(auto& v:params)v=std::uint8_t(rng());for(auto& v:tires)v=std::uint8_t(rng());Bytes ee(event.data(),event.size()),ww(work.data(),work.size()),pp(params.data(),params.size()),tt(tires.data(),tires.size());auto rf=[&](float q){return float(int(rng()%257u)-128)*q;};
            ee.put32(0x2b4,WreckerArena+0x6000u);static constexpr float gates[]={-1.0f,0.0f,0.5f,2.0f};ee.putf(0x2c8,gates[i%4u]);static constexpr std::uint32_t sts[]={0u,3u<<2,6u<<2,7u<<2,8u<<2};ee.put32(0x2f0,sts[(i/4u)%5u]);
            const std::uint32_t mode=(i/20u)%4u;ee.put32(0x208,mode);ee.put8(0x11,(i%7u)==0u?15u:14u);ee.putf(0x2a0,(i%11u)==0u?0.5f:1.0f);ee.putf(0xdc0,0.5f+float(i%13u)*0.125f);pp.put32(0x10a0,(i%5u)==0u?mode:99u);for(unsigned m=1;m<5;++m)pp.putf(std::size_t(m+0x7bu)*0x4cu,0.5f+float((i+m)%17u)*0.125f);
            for(unsigned k=0;k<16;++k)ww.putf(0x10u+k*4u,0.0f);ww.putf(0x10,1.0f);ww.putf(0x24,1.0f);ww.putf(0x38,1.0f);ww.putf(0x4c,1.0f);ww.putf(0x40,rf(0.125f));ww.putf(0x44,rf(0.0625f));ww.putf(0x48,rf(0.125f));ww.putf(0x5c,rf(0.0625f));ww.putf(0x60,rf(0.03125f));ww.putf(0x64,rf(0.0625f));for(auto o:{0x68u,0x6cu,0x70u,0x74u,0x78u,0x7cu})ww.putf(o,rf(0.03125f));ww.put8(0x440,(i%29u)==0u?1u:0u);ww.put8(0x534,(i%31u)==0u?1u:0u);
            for(unsigned k=0;k<4;++k){auto q=tt.sub(k*0xb0u,0xb0u);q.put8(0,(i+k)%19u==0u?1u:0u);q.putf(0x04,rf(0.125f));q.putf(0x0c,rf(0.125f));q.putf(0x2c,rf(0.125f));for(auto o:{0x7cu,0x80u,0x84u,0x88u,0x8cu,0x90u,0x94u,0x98u,0x9cu,0xa0u,0xa4u,0xa8u})q.putf(o,rf(0.03125f));ww.put32(0x248u+k*4u,WreckerArena+0x4000u+k*0xb0u);}
            native_event=event;native_work=work;native_params=params;native_tires=tires;std::memcpy(reinterpret_cast<void*>(WreckerArena),event.data(),event.size());std::memcpy(reinterpret_cast<void*>(WreckerArena+0x2000u),work.data(),work.size());std::memcpy(reinterpret_cast<void*>(WreckerArena+0x4000u),tires.data(),tires.size());std::memcpy(reinterpret_cast<void*>(WreckerArena+0x6000u),params.data(),params.size());
            prepare(0x4a0000u);guest_call.ebx=WreckerArena;Bytes st(reinterpret_cast<void*>(S),16);st.put32(0,WreckerArena+0x2000u);run();auto stack=native_f.matrix();std::array<Bytes,4> tv{{Bytes(native_tires.data()+0*0xb0u,0xb0u),Bytes(native_tires.data()+1*0xb0u,0xb0u),Bytes(native_tires.data()+2*0xb0u,0xb0u),Bytes(native_tires.data()+3*0xb0u,0xb0u)}};make_force_work_tire_4a0000(Bytes(native_event.data(),native_event.size()),Bytes(native_work.data(),native_work.size()),Bytes(native_params.data(),native_params.size()),tv,stack);native_f.save(stack,native_f.prediction());
            bind.capture_globals();const std::string name=world_names[id];compare_world_bytes(name+"_event",reinterpret_cast<void*>(WreckerArena),native_event.data(),native_event.size());compare_world_bytes(name+"_work",reinterpret_cast<void*>(WreckerArena+0x2000u),native_work.data(),native_work.size());compare_world_bytes(name+"_params",reinterpret_cast<void*>(WreckerArena+0x6000u),native_params.data(),native_params.size());compare_world_bytes(name+"_tires",reinterpret_cast<void*>(WreckerArena+0x4000u),native_tires.data(),native_tires.size());bind.compare(name,native_f);continue;
        }
        if(id==43u){
            auto native_f=f;
            std::array<std::uint8_t,0x800> work{},native_work{};
            std::mt19937 rng(0x4a3c6031u^(i*1664525u));
            for(auto& v:work)v=std::uint8_t(rng());
            Bytes ww(work.data(),work.size());
            auto rf=[&](float q){return float(int(rng()%257u)-128)*q;};
            CourseProbe velocity{rf(0.03125f),rf(0.03125f),rf(0.03125f)};
            if(i%31u==0u)velocity={0.0f,0.0f,0.0f};
            else if(i%31u==1u)velocity={0.00000003125f,-0.000000015625f,0.0000000078125f};
            else if(i%31u==2u)velocity={12.0f,-2.0f,18.0f};
            outrun::testing::world_put_probe(ww,0x5c,velocity);
            CourseProbe force{rf(0.25f),rf(0.25f),rf(0.25f)};
            CourseProbe current{rf(0.0625f),rf(0.0625f),rf(0.0625f)};
            outrun::testing::world_put_probe(ww,0xc0,force);
            outrun::testing::world_put_probe(ww,0x14c,current);
            static constexpr float thresholds[]={0.0f,0.00001f,0.01f,0.125f,0.5f,1.0f,3.0f,10.0f,50.0f};
            const float threshold=thresholds[i%(sizeof(thresholds)/sizeof(thresholds[0]))];
            native_work=work;
            std::memcpy(reinterpret_cast<void*>(WreckerArena+0x2000u),work.data(),work.size());
            prepare(0x4a3c60u);guest_call.eax=WreckerArena+0x2000u;Bytes st(reinterpret_cast<void*>(S),16);st.putf(0,threshold);run();
            calc_pl_body_force_limit_4a3c60(Bytes(native_work.data(),native_work.size()),threshold);
            bind.capture_globals();const std::string name=world_names[id];compare_world_bytes(name+"_work",reinterpret_cast<void*>(WreckerArena+0x2000u),native_work.data(),native_work.size());bind.compare(name,native_f);continue;
        }
        if(id==42u){
            auto native_f=f;
            std::array<std::uint8_t,0x1000> event{},native_event{};
            std::array<std::uint8_t,0x800> work{},native_work{};
            std::array<std::uint8_t,0x400> params{},native_params{};
            std::mt19937 rng(0x4a395031u^(i*1664525u));
            for(auto& v:event)v=std::uint8_t(rng());for(auto& v:work)v=std::uint8_t(rng());for(auto& v:params)v=std::uint8_t(rng());
            Bytes ee(event.data(),event.size()),ww(work.data(),work.size()),pp(params.data(),params.size());
            ee.put32(0x2b4,WreckerArena+0x6000u);
            static constexpr std::int16_t ang[]={-0x5000,-0x4000,-0x3000,-0x800,-1,0,1,0x800,0x3000,0x4000,0x5000};
            ee.put16(0x4e,std::uint16_t(ang[i%11u]));ee.put32(0x1f4,(i%13u)==0u?0u:1u);
            ee.put16(0xd44,std::uint16_t(ang[(i/3u)%11u]/2));ee.put16(0x32,std::uint16_t(ang[(i/7u)%11u]/3));ee.put16(0xd46,std::uint16_t(ang[(i/11u)%11u]/2));
            ee.putf(0xd40,float(int((i/5u)%21u)-10)*0.015625f);
            pp.putf(0x130,0.75f+float(i%17u)*0.0625f);pp.putf(0x214,0.5f+float((i/17u)%13u)*0.125f);
            auto ident=[&](std::size_t o){for(unsigned k=0;k<16;++k)ww.putf(o+k*4u,0.0f);ww.putf(o+0x00,1.0f);ww.putf(o+0x14,1.0f);ww.putf(o+0x28,1.0f);ww.putf(o+0x3c,1.0f);};
            ident(0x100);ident(0x1e0);
            const float yaw=float(int(i%17u)-8)*0.0125f,cy=std::cos(yaw),sy=std::sin(yaw);
            ww.putf(0x100+0x00,cy);ww.putf(0x100+0x08,-sy);ww.putf(0x100+0x20,sy);ww.putf(0x100+0x28,cy);
            ww.putf(0x14c,float(int(rng()%257u)-128)*0.03125f);ww.putf(0x150,float(int(rng()%65u)-32)*0.015625f);ww.putf(0x154,1.0f+float(rng()%97u)*0.03125f);
            ww.putf(0x144,float(int(rng()%129u)-64)*0.03125f);
            ww.putf(0x628,0.0f);ww.putf(0x62c,1.0f);ww.putf(0x630,0.0f);
            native_event=event;native_work=work;native_params=params;
            std::memcpy(reinterpret_cast<void*>(WreckerArena),event.data(),event.size());std::memcpy(reinterpret_cast<void*>(WreckerArena+0x2000u),work.data(),work.size());std::memcpy(reinterpret_cast<void*>(WreckerArena+0x6000u),params.data(),params.size());
            prepare(0x4a3950u);guest_call.ecx=WreckerArena;guest_call.eax=WreckerArena+0x2000u;run();
            auto native_stack=native_f.matrix();
            cornering_ctrl(Bytes(native_event.data(),native_event.size()),Bytes(native_work.data(),native_work.size()),Bytes(native_params.data(),native_params.size()),native_stack);
            {auto nb=native_f.bytes();nb.puti(0x4550,static_cast<std::int32_t>(native_stack.current_offset));nb.puti(0x4554,native_stack.depth);nb.puti(0x4558,native_stack.capacity);}
            bind.capture_globals();const std::string name=world_names[id];compare_world_bytes(name+"_event",reinterpret_cast<void*>(WreckerArena),native_event.data(),native_event.size());compare_world_bytes(name+"_work",reinterpret_cast<void*>(WreckerArena+0x2000u),native_work.data(),native_work.size());compare_world_bytes(name+"_params",reinterpret_cast<void*>(WreckerArena+0x6000u),native_params.data(),native_params.size());bind.compare(name,native_f);continue;
        }
        if(id==41u){
            auto native_f=f;
            std::array<std::uint8_t,0x1000> event{},native_event{};
            std::array<std::uint8_t,0x800> work{},native_work{};
            std::array<std::uint8_t,0x2600> params{},native_params{};
            std::mt19937 rng(0x4a331031u^(i*1664525u));
            for(auto& v:event)v=std::uint8_t(rng());for(auto& v:work)v=std::uint8_t(rng());for(auto& v:params)v=std::uint8_t(rng());
            Bytes ee(event.data(),event.size()),ww(work.data(),work.size()),pp(params.data(),params.size());
            ee.put32(0x2b4,WreckerArena+0x6000u);
            auto rf=[&](float q){return float(int(rng()%257u)-128)*q;};
            ee.putf(0x1c4,0.75f+float(i%61u)*0.025f);ee.put8(0xd36,std::uint8_t(i%24u));
            ee.putf(0xd38,0.25f+float((i/3u)%13u)*0.05f);ee.putf(0xd3c,float((i/7u)%11u)*0.05f);
            static constexpr std::int16_t angles[]={-0x3000,-0x1800,-0x400,-1,0,1,0x400,0x1800,0x3000};
            ee.put16(0xd46,std::uint16_t(angles[i%9u]));ee.put16(0xd48,std::uint16_t(angles[(i/3u)%9u]/8));
            ee.put16(0xd4a,std::uint16_t(angles[(i/9u)%9u]));ee.put16(0x32,std::uint16_t(angles[(i/27u)%9u]));
            ee.put16(0x4e,std::uint16_t(angles[(i/81u)%9u]));ee.put16(0x202,std::uint16_t(angles[(i/243u)%9u]));
            ee.put8(0x2f0,(i%17u)==0u?2u:0u);ee.put8(0x283,(i%31u)==0u?1u:0u);ee.puti(0xd94,std::int32_t(i%241u)-20);
            for(unsigned k=0;k<3;++k){ww.putf(0x50u+k*4u,rf(0.03125f));ww.putf(0x628u+k*4u,k==1?1.0f+rf(0.001f):rf(0.015625f));}
            pp.putf(0,1.0f+float(i%13u)*0.0625f);pp.putf(0x130,0.75f+float((i/13u)%11u)*0.0625f);pp.putf(0x214,1.0f+float((i/143u)%9u)*0.125f);
            pp.putf(0x2010,0.25f+float(i%17u)*0.03125f);pp.putf(0x205c,256.0f+float((i/17u)%17u)*64.0f);
            pp.putf(0x22bc,0.05f+float(i%9u)*0.01f);pp.putf(0x2308,0.15f+float((i/9u)%11u)*0.015625f);pp.putf(0x2354,0.02f+float((i/99u)%7u)*0.01f);pp.putf(0x23a0,0.5f+float(i%11u)*0.0625f);pp.putf(0x23ec,0.08f+float((i/11u)%7u)*0.01f);
            for(auto off:{0x1db0u,0x1e48u,0x1dfcu,0x1e94u,0x1ee0u,0x1f78u,0x1f2cu,0x1fc4u})pp.putf(off,0.25f+float((off/4u+i)%13u)*0.03125f);
            for(unsigned idx=0;idx<2;++idx){pp.putf(std::size_t(idx+0x32u)*0x4cu,0.25f+float((i+idx)%11u)*0.0625f);pp.putf(std::size_t(idx+0x36u)*0x4cu,0.5f+float((i+idx)%13u)*0.0625f);}
            SlipAngleInputs inputs{(i&3u)==0u?1u:0u,(i%5u)==0u?0.5f:1.0f};
            native_event=event;native_work=work;native_params=params;
            std::memcpy(reinterpret_cast<void*>(WreckerArena),event.data(),event.size());std::memcpy(reinterpret_cast<void*>(WreckerArena+0x2000u),work.data(),work.size());std::memcpy(reinterpret_cast<void*>(WreckerArena+0x6000u),params.data(),params.size());
            *reinterpret_cast<std::uint32_t*>(0x7f94c0u)=inputs.assist_gate;*reinterpret_cast<float*>(0x800aacu)=inputs.assist_state;
            prepare(0x4a3310u);guest_call.esi=WreckerArena;guest_call.ecx=WreckerArena+0x2000u;run();
            slip_angle_ctrl(Bytes(native_event.data(),native_event.size()),Bytes(native_work.data(),native_work.size()),Bytes(native_params.data(),native_params.size()),inputs);
            bind.capture_globals();const std::string name=world_names[id];compare_world_bytes(name+"_event",reinterpret_cast<void*>(WreckerArena),native_event.data(),native_event.size());compare_world_bytes(name+"_work",reinterpret_cast<void*>(WreckerArena+0x2000u),native_work.data(),native_work.size());compare_world_bytes(name+"_params",reinterpret_cast<void*>(WreckerArena+0x6000u),native_params.data(),native_params.size());bind.compare(name,native_f);continue;
        }
        if(id==39u||id==40u){
            auto native_f=f;
            std::array<std::uint8_t,0x1000> event{},native_event{};
            std::array<std::uint8_t,0x800> work{},native_work{};
            std::array<std::uint8_t,0x2000> params{},native_params{};
            std::mt19937 rng((0x4a2fa031u+id*0x101u)^(i*1664525u));
            for(auto& v:event)v=std::uint8_t(rng());for(auto& v:work)v=std::uint8_t(rng());for(auto& v:params)v=std::uint8_t(rng());
            Bytes ee(event.data(),event.size()),ww(work.data(),work.size()),pp(params.data(),params.size());ee.put32(0x2b4,WreckerArena+0x6000u);
            auto sf=[&](unsigned k,float q){return float(int((rng()>>(k%13u))%257u)-128)*q;};
            ee.put8(0x13,(i%5u)==0u?1u:0u);ee.put8(0xd36,std::uint8_t((i%9u)==0u?0u:1u+(i%45u)));
            ee.puti(0xd94,std::int32_t(i%241u)-20);ee.puti(0xd98,std::int32_t((i/3u)%101u)-10);ee.puti(0xd9c,std::int32_t((i/7u)%71u)-5);ee.put32(0x1f4,(i%6u)==0u?0u:1u);
            static constexpr std::int16_t steer_cases[]={0,0x2ff,0x300,0x601,0x3800,0x3801,-0x300,-0x601,-0x3801,0x7fff,-0x7fff};
            ee.put16(0x4e,std::uint16_t(steer_cases[i%(sizeof(steer_cases)/sizeof(steer_cases[0]))]));ee.put16(0x16a,std::uint16_t(std::int16_t(int(i%9u)-4)*0x200));
            ee.put16(0x202,std::uint16_t(std::int16_t((int((i/11u)%17u)-8)*0x1000)));ee.put16(0xd46,std::uint16_t(std::int16_t((int((i/5u)%29u)-14)*0x600)));
            ee.putf(0x26c,sf(2,0.015625f));ww.put8(0x244,(i&1u)?8u:0u);
            for(unsigned idx=0;idx<2;++idx){pp.putf(std::size_t(idx+0x32u)*0x4cu,0.25f+float((i+idx)%17u)*0.0625f);pp.putf(std::size_t(idx+0x36u)*0x4cu,0.5f+float((i+idx*3u)%19u)*0.09375f);}
            pp.putf(0x1c34,0.35f+float(i%7u)*0.05f);pp.putf(0x1c80,0.45f+float((i/7u)%7u)*0.05f);pp.putf(0x1ccc,0.55f+float((i/49u)%7u)*0.05f);pp.putf(0x1d18,0.05f+float(i%9u)*0.015625f);pp.putf(0x1d64,0.35f+float((i/9u)%11u)*0.03125f);
            RearGripInputs inputs{std::int32_t(i%321u),std::int32_t((i*7u)%321u),std::int32_t((i*13u)%321u),std::int32_t((i*17u)%321u)};if(i%23u==0u)inputs={249,11,250,10};if(i%23u==1u)inputs={249,11,249,11};
            native_event=event;native_work=work;native_params=params;std::memcpy(reinterpret_cast<void*>(WreckerArena),event.data(),event.size());std::memcpy(reinterpret_cast<void*>(WreckerArena+0x2000u),work.data(),work.size());std::memcpy(reinterpret_cast<void*>(WreckerArena+0x6000u),params.data(),params.size());
            if(id==39u){const std::int32_t index=std::int32_t(i&1u);prepare(0x4a3260u);guest_call.eax=WreckerArena;guest_call.edx=std::uint32_t(index);run();compare_u32("rear_grip_curve_r031_return",guest_call.out_xmm0,fbits(rear_grip_curve_4a3260(Bytes(native_event.data(),native_event.size()),Bytes(native_params.data(),native_params.size()),index)));}
            else {set_rear_grip_inputs(inputs);prepare(0x4a2fa0u);guest_call.esi=WreckerArena;Bytes st(reinterpret_cast<void*>(S),16);st.put32(0,WreckerArena+0x2000u);run();rear_grip_ctrl(Bytes(native_event.data(),native_event.size()),Bytes(native_work.data(),native_work.size()),Bytes(native_params.data(),native_params.size()),inputs);}
            bind.capture_globals();const std::string name=world_names[id];compare_world_bytes(name+"_event",reinterpret_cast<void*>(WreckerArena),native_event.data(),native_event.size());compare_world_bytes(name+"_work",reinterpret_cast<void*>(WreckerArena+0x2000u),native_work.data(),native_work.size());compare_world_bytes(name+"_params",reinterpret_cast<void*>(WreckerArena+0x6000u),native_params.data(),native_params.size());bind.compare(name,native_f);continue;
        }
        if(id>=34u&&id<=38u){
            // r030 force-work cluster.  Original and native worlds begin from
            // independent event/work/parameter/suspension images.  Guest
            // pointers are installed only for the x86 side; native APIs use
            // explicit bounded views and never dereference a guest address.
            auto native_f=f;
            std::array<std::uint8_t,0x1000> event{},native_event{};
            std::array<std::uint8_t,0x800> work{},native_work{};
            std::array<std::uint8_t,0x2000> params{},native_params{};
            std::array<std::uint8_t,4*0xf4> susp{},native_susp{};
            std::mt19937 rng((0x4a61f030u+id*0x10001u)^(i*1664525u));
            for(auto& v:event)v=std::uint8_t(rng());for(auto& v:work)v=std::uint8_t(rng());
            for(auto& v:params)v=std::uint8_t(rng());for(auto& v:susp)v=std::uint8_t(rng());
            Bytes ee(event.data(),event.size()),ww(work.data(),work.size()),pp(params.data(),params.size()),ss(susp.data(),susp.size());
            auto identity=[&](std::size_t off){for(unsigned k=0;k<16;++k)ww.putf(off+k*4u,0.0f);ww.putf(off+0x00,1.0f);ww.putf(off+0x14,1.0f);ww.putf(off+0x28,1.0f);ww.putf(off+0x3c,1.0f);};
            identity(0x10);identity(0x1e0);
            const float yaw=float(int(i%61u)-30)*0.00625f,c=std::cos(yaw),sn=std::sin(yaw);
            ww.putf(0x10,c);ww.putf(0x18,-sn);ww.putf(0x30,sn);ww.putf(0x38,c);
            ww.putf(0x40,float(int(i%47u)-23)*0.125f);ww.putf(0x44,float(int((i/47u)%29u)-14)*0.0625f);ww.putf(0x48,float(int((i/1363u)%41u)-20)*0.125f);
            auto rf=[&](float q){return float(int(rng()%257u)-128)*q;};
            CourseProbe velocity{rf(0.03125f),rf(0.015625f),rf(0.03125f)};
            if(i%31u==0u)velocity={0.0f,0.0f,0.0f};
            else if(i%31u==1u)velocity={0.00003125f,-0.000015625f,0.0000078125f};
            else if(i%31u==2u)velocity={12.0f,-2.0f,18.0f};
            outrun::testing::world_put_probe(ww,0x5c,velocity);
            for(auto off:{0x68u,0x6cu,0x70u,0x74u,0x78u,0x7cu})ww.putf(off,rf(0.03125f));
            ww.putf(0x98,0.5f+float(i%17u)*0.125f);
            const CourseProbe road{float(int(i%13u)-6)*0.0078125f,1.0f+float(int((i/13u)%11u)-5)*0.00390625f,float(int((i/143u)%13u)-6)*0.0078125f};
            outrun::testing::world_put_probe(ww,0x628,road);
            ee.putf(0xe6c,[] (unsigned n){static constexpr float v[]={-0.25f,0.0f,0.25f,0.75f,0.999f,1.0f,1.25f};return v[n%7u];}(i));
            ee.put8(0x282,std::uint8_t(i%9u));ee.put8(0x283,std::uint8_t((i/9u)%4u));ee.put32(0x2b4,WreckerArena+0x6000u);
            pp.putf(0,0.125f+float((i/7u)%17u)*0.0625f);
            pp.putf(0x1988,0.25f+float((i/17u)%13u)*0.0625f);pp.putf(0x19d4,0.5f+float((i/29u)%11u)*0.125f);
            for(unsigned k=0;k<4;++k){
                auto q=ss.sub(k*0xf4u,0xf4u);q.putf(0x04,rf(0.125f));q.putf(0x08,rf(0.125f));q.putf(0x0c,rf(0.125f));
                float fy=rf(0.0625f);if((i+k)%19u==0u)fy=0.0f;q.putf(0x24,fy);
                ww.put32(0x248u+k*4u,WreckerArena+0x4000u+k*0xf4u);
            }
            native_event=event;native_work=work;native_params=params;native_susp=susp;
            std::memcpy(reinterpret_cast<void*>(WreckerArena),event.data(),event.size());
            std::memcpy(reinterpret_cast<void*>(WreckerArena+0x2000u),work.data(),work.size());
            std::memcpy(reinterpret_cast<void*>(WreckerArena+0x4000u),susp.data(),susp.size());
            std::memcpy(reinterpret_cast<void*>(WreckerArena+0x6000u),params.data(),params.size());
            Bytes st(reinterpret_cast<void*>(S),32);
            if(id==34u){prepare(0x49fe00u);guest_call.esi=WreckerArena;guest_call.edi=WreckerArena+0x2000u;run();}
            else if(id==35u){prepare(0x49fed0u);guest_call.edx=WreckerArena;guest_call.esi=WreckerArena+0x2000u;run();}
            else if(id==36u){prepare(0x518010u);st.put32(0,WreckerArena+0x2000u);run();}
            else if(id==37u){prepare(0x5180b0u);st.put32(0,WreckerArena+0x2000u);run();}
            else {prepare(0x4a61f0u);guest_call.eax=WreckerArena+0x2000u;st.put32(0,WreckerArena);run();}
            bind.capture_globals();
            auto stack=native_f.matrix();auto prediction=native_f.prediction();
            std::array<Bytes,4> native_susp_views{{Bytes(native_susp.data()+0*0xf4u,0xf4u),Bytes(native_susp.data()+1*0xf4u,0xf4u),Bytes(native_susp.data()+2*0xf4u,0xf4u),Bytes(native_susp.data()+3*0xf4u,0xf4u)}};
            try{
                Bytes ne(native_event.data(),native_event.size()),nw(native_work.data(),native_work.size()),np(native_params.data(),native_params.size());
                if(id==34u)calc_game_resist(ne,nw,np,stack);
                else if(id==35u)force_work_49fed0(ne,nw,np,stack);
                else if(id==36u)ass_cancel_incline_resistance(nw,stack);
                else if(id==37u)ass_press_down_against_road(nw,stack);
                else make_force_work(ne,nw,np,native_susp_views,stack);
            }catch(const std::exception& ex){throw std::runtime_error(std::string(world_names[id])+" case="+std::to_string(i)+": "+ex.what());}
            native_f.save(stack,prediction);
            const std::string name=world_names[id];
            compare_world_bytes(name+"_event",reinterpret_cast<void*>(WreckerArena),native_event.data(),native_event.size());
            compare_world_bytes(name+"_work",reinterpret_cast<void*>(WreckerArena+0x2000u),native_work.data(),native_work.size());
            compare_world_bytes(name+"_suspensions",reinterpret_cast<void*>(WreckerArena+0x4000u),native_susp.data(),native_susp.size());
            compare_world_bytes(name+"_params",reinterpret_cast<void*>(WreckerArena+0x6000u),native_params.data(),native_params.size());
            bind.compare(name,native_f);continue;
        }
        if(id==33u){
            // PC 0x49FD50 MakeForceWorkSus: four suspension force/point pairs
            // transformed through body*road matrix then accumulated by SetForce.
            auto native_f=f;
            std::array<std::uint8_t,0x1000> event{},native_event{};
            std::array<std::uint8_t,0x800> work{},native_work{};
            std::array<std::uint8_t,4*0xf4> susp{},native_susp{};
            std::mt19937 rng(0x49fd5029u^(i*1664525u));
            for(auto& v:event)v=std::uint8_t(rng());for(auto& v:work)v=std::uint8_t(rng());for(auto& v:susp)v=std::uint8_t(rng());
            Bytes ee(event.data(),event.size()),ww(work.data(),work.size()),ss(susp.data(),susp.size());
            auto identity=[&](std::size_t off){for(unsigned k=0;k<16;++k)ww.putf(off+k*4u,0.0f);ww.putf(off+0x00,1.0f);ww.putf(off+0x14,1.0f);ww.putf(off+0x28,1.0f);ww.putf(off+0x3c,1.0f);};
            identity(0x10);identity(0x1e0);
            const float a=float(int(i%41u)-20)*0.01f,c=std::cos(a),sn=std::sin(a);
            ww.putf(0x10,c);ww.putf(0x18,-sn);ww.putf(0x30,sn);ww.putf(0x38,c);
            ww.putf(0x40,float(int(i%17u)-8)*0.25f);ww.putf(0x44,float(int((i/17u)%13u)-6)*0.125f);ww.putf(0x48,float(int((i/221u)%19u)-9)*0.25f);
            const float b=float(int((i/5u)%37u)-18)*0.0078125f,cb=std::cos(b),sb=std::sin(b);
            ww.putf(0x1e0,cb);ww.putf(0x1e8,-sb);ww.putf(0x200,sb);ww.putf(0x208,cb);
            ww.putf(0x210,float(int(i%11u)-5)*0.125f);ww.putf(0x214,float(int((i/11u)%9u)-4)*0.0625f);ww.putf(0x218,float(int((i/99u)%15u)-7)*0.125f);
            auto rf=[&](float q){return float(int(rng()%257u)-128)*q;};
            for(auto off:{0x68u,0x6cu,0x70u,0x74u,0x78u,0x7cu})ww.putf(off,rf(0.03125f));
            ee.put8(0x283,(i%4u)==0u?0u:std::uint8_t(1u+(i%4u)));
            for(unsigned k=0;k<4;++k){
                auto q=ss.sub(k*0xf4u,0xf4u);q.putf(0x04,rf(0.125f));q.putf(0x08,rf(0.125f));q.putf(0x0c,rf(0.125f));
                float fy=rf(0.0625f);if((i+k)%9u==0u)fy=0.0f;q.putf(0x24,fy);
                ww.put32(0x248u+k*4u,WreckerArena+0x4000u+k*0xf4u);
            }
            native_event=event;native_work=work;native_susp=susp;
            std::memcpy(reinterpret_cast<void*>(WreckerArena),event.data(),event.size());
            std::memcpy(reinterpret_cast<void*>(WreckerArena+0x2000u),work.data(),work.size());
            std::memcpy(reinterpret_cast<void*>(WreckerArena+0x4000u),susp.data(),susp.size());
            prepare(0x49fd50u);guest_call.ebx=WreckerArena+0x2000u;Bytes st(reinterpret_cast<void*>(S),16);st.put32(0,WreckerArena);run();bind.capture_globals();
            auto stack=native_f.matrix();auto prediction=native_f.prediction();
            std::array<Bytes,4> native_susp_views{{Bytes(native_susp.data()+0*0xf4u,0xf4u),Bytes(native_susp.data()+1*0xf4u,0xf4u),Bytes(native_susp.data()+2*0xf4u,0xf4u),Bytes(native_susp.data()+3*0xf4u,0xf4u)}};
            try{make_force_work_sus(Bytes(native_event.data(),native_event.size()),Bytes(native_work.data(),native_work.size()),native_susp_views,stack);}
            catch(const std::exception& ex){throw std::runtime_error(std::string("MakeForceWorkSus r029 case=")+std::to_string(i)+": "+ex.what());}
            native_f.save(stack,prediction);
            compare_world_bytes("make_force_work_sus_r029_event",reinterpret_cast<void*>(WreckerArena),native_event.data(),native_event.size());
            compare_world_bytes("make_force_work_sus_r029_work",reinterpret_cast<void*>(WreckerArena+0x2000u),native_work.data(),native_work.size());
            compare_world_bytes("make_force_work_sus_r029_suspensions",reinterpret_cast<void*>(WreckerArena+0x4000u),native_susp.data(),native_susp.size());
            bind.compare("make_force_work_sus_r029",native_f);continue;
        }
        if(id==32u){
            // r029 SetForce_obsolete 0x517010: linear force accumulation and
            // body-local torque from point x force, with the real matrix stack.
            auto native_f=f;
            std::array<std::uint8_t,0x100> body{},native_body{};
            std::mt19937 rng(0x51701029u^(i*1103515245u));for(auto& v:body)v=std::uint8_t(rng());
            Bytes b(body.data(),body.size());b.put32(0,1u);
            for(unsigned k=0;k<16;++k)b.putf(0x10u+k*4u,0.0f);
            const float yaw=float(int(i%41u)-20)*0.01f,c=std::cos(yaw),sn=std::sin(yaw);
            b.putf(0x10,c);b.putf(0x18,-sn);b.putf(0x24,1.0f);b.putf(0x30,sn);b.putf(0x38,c);b.putf(0x4c,1.0f);
            b.putf(0x40,float(int(i%23u)-11)*0.25f);b.putf(0x44,float(int((i/23u)%19u)-9)*0.125f);b.putf(0x48,float(int((i/437u)%29u)-14)*0.25f);
            auto rf=[&](float q){return float(int(rng()%257u)-128)*q;};
            for(auto off:{0x68u,0x6cu,0x70u,0x74u,0x78u,0x7cu})b.putf(off,rf(0.03125f));
            CourseProbe force{rf(0.125f),rf(0.125f),rf(0.125f)};
            CourseProbe point{b.f32(0x40)+rf(0.25f),b.f32(0x44)+rf(0.25f),b.f32(0x48)+rf(0.25f)};
            const std::uint32_t flags=i%4u;native_body=body;
            std::array<std::uint8_t,32> args{};Bytes a(args.data(),args.size());
            a.putf(0,force.x);a.putf(4,force.y);a.putf(8,force.z);a.putf(16,point.x);a.putf(20,point.y);a.putf(24,point.z);
            std::memcpy(reinterpret_cast<void*>(WreckerArena+0x8000u),body.data(),body.size());
            std::memcpy(reinterpret_cast<void*>(WreckerArena+0x9000u),args.data(),args.size());
            prepare(0x517010u);Bytes st(reinterpret_cast<void*>(S),24);st.put32(0,WreckerArena+0x8000u);st.put32(4,WreckerArena+0x9000u);st.put32(8,WreckerArena+0x9010u);st.put32(12,flags);run();bind.capture_globals();
            auto stack=native_f.matrix();auto prediction=native_f.prediction();
            try{set_force_obsolete(Bytes(native_body.data(),native_body.size()),force,point,flags,stack);}
            catch(const std::exception& ex){throw std::runtime_error(std::string("SetForce obsolete r029 case=")+std::to_string(i)+": "+ex.what());}
            native_f.save(stack,prediction);
            compare_world_bytes("set_force_obsolete_r029_body",reinterpret_cast<void*>(WreckerArena+0x8000u),native_body.data(),native_body.size());
            bind.compare("set_force_obsolete_r029",native_f);continue;
        }
        if(id==31u){
            // Direct closure for mxRotateAxe 0x40A550, which ActionForce2 uses.
            auto native_f=f;
            static constexpr CourseProbe axes[]={{1,0,0},{0,1,0},{0,0,1},{0.25f,-0.5f,0.75f},{-2.0f,1.0f,0.5f},{0.001f,0.002f,-0.003f}};
            CourseProbe axis=axes[i%(sizeof(axes)/sizeof(axes[0]))];
            const float angle=float(int((i/6u)%257u)-128)*0.00390625f;
            std::array<std::uint8_t,16> arg{};Bytes a(arg.data(),arg.size());a.putf(0,axis.x);a.putf(4,axis.y);a.putf(8,axis.z);
            std::memcpy(reinterpret_cast<void*>(WreckerArena+0x9000u),arg.data(),arg.size());
            prepare(0x40a550u);Bytes st(reinterpret_cast<void*>(S),16);st.put32(0,WreckerArena+0x9000u);st.putf(4,angle);run();bind.capture_globals();
            auto stack=native_f.matrix();auto prediction=native_f.prediction();
            try{pc_matrix_rotate_axis(stack,axis,angle);}
            catch(const std::exception& ex){throw std::runtime_error(std::string("mxRotateAxe r029 case=")+std::to_string(i)+": "+ex.what());}
            native_f.save(stack,prediction);bind.compare("matrix_rotate_axis_r029",native_f);continue;
        }
        if(id==30u){
            // r029 closes ActionForce2 0x517410.  The original and native
            // sides start from independent body + matrix-stack images.
            auto native_f=f;
            std::array<std::uint8_t,0x100> body{},native_body{};
            Bytes b(body.data(),body.size());
            std::mt19937 rng(0x51741029u^(i*1664525u));
            for(auto& v:body)v=std::uint8_t(rng());
            // Keep all arithmetic finite while exercising enabled/disabled,
            // below-epsilon angular speed and active rotation paths.
            b.put32(0,(i%17u)==0u?0u:1u);
            for(unsigned k=0;k<16;++k)b.putf(0x10u+k*4u,0.0f);
            const float yaw=float(int(i%33u)-16)*0.0125f;
            const float c=std::cos(yaw),sn=std::sin(yaw);
            b.putf(0x10,c);b.putf(0x18,-sn);b.putf(0x24,1.0f);b.putf(0x30,sn);b.putf(0x38,c);b.putf(0x4c,1.0f);
            b.putf(0x40,float(int(i%41u)-20)*0.125f);b.putf(0x44,float(int((i/41u)%29u)-14)*0.0625f);b.putf(0x48,float(int((i/1189u)%37u)-18)*0.125f);
            auto sf=[&](unsigned salt,float scale){return float(int((rng()>>salt)%257u)-128)*scale;};
            b.putf(0x5c,sf(0,0.03125f));b.putf(0x60,sf(1,0.03125f));b.putf(0x64,sf(2,0.03125f));
            b.putf(0x68,sf(3,0.015625f));b.putf(0x6c,sf(4,0.015625f));b.putf(0x70,sf(5,0.015625f));
            b.putf(0x74,sf(6,0.125f));b.putf(0x78,sf(7,0.125f));b.putf(0x7c,sf(8,0.125f));
            b.putf(0x98,0.25f+float(i%11u)*0.0625f);b.putf(0x9c,0.5f+float((i/11u)%9u)*0.125f);
            b.putf(0xa0,0.75f+float(i%7u)*0.125f);b.putf(0xa4,0.875f+float((i/7u)%7u)*0.125f);b.putf(0xa8,1.0f+float((i/49u)%7u)*0.125f);
            b.putf(0xac,0.125f+float(i%5u)*0.0625f);b.putf(0xb0,0.1875f+float((i/5u)%5u)*0.0625f);b.putf(0xb4,0.25f+float((i/25u)%5u)*0.0625f);
            b.putf(0xc0,sf(9,0.0625f));b.putf(0xc4,sf(10,0.0625f));b.putf(0xc8,sf(11,0.0625f));
            b.putf(0xcc,sf(12,0.03125f));b.putf(0xd0,sf(13,0.03125f));b.putf(0xd4,sf(14,0.03125f));
            if(i%13u==0u){b.putf(0x50,0.0f);b.putf(0x54,0.0f);b.putf(0x58,0.0f);b.putf(0x68,0.0f);b.putf(0x6c,0.0f);b.putf(0x70,0.0f);b.putf(0xcc,0.0f);b.putf(0xd0,0.0f);b.putf(0xd4,0.0f);}
            else {b.putf(0x50,sf(15,0.0078125f));b.putf(0x54,sf(16,0.0078125f));b.putf(0x58,sf(17,0.0078125f));}
            static constexpr float scales[]={0.0f,0.25f,0.75f,1.0f,1.25f,1.5f,2.0f,-0.5f};
            const float step_scale=scales[(i/3u)%8u];
            native_body=body;
            std::memcpy(reinterpret_cast<void*>(WreckerArena+0x8000u),body.data(),body.size());
            prepare(0x517410u);Bytes st(reinterpret_cast<void*>(S),16);st.put32(0,WreckerArena+0x8000u);st.putf(4,step_scale);run();bind.capture_globals();

            auto stack=native_f.matrix();auto prediction=native_f.prediction();
            try{action_force2(Bytes(native_body.data(),native_body.size()),step_scale,stack);}
            catch(const std::exception& ex){throw std::runtime_error(std::string("ActionForce2 r029 case=")+std::to_string(i)+": "+ex.what());}
            native_f.save(stack,prediction);
            compare_world_bytes("action_force2_r029_body",reinterpret_cast<void*>(WreckerArena+0x8000u),native_body.data(),native_body.size());
            bind.compare("action_force2_r029",native_f);
            continue;
        }
        if(id==29u){
            // r028 closes ColiCar 0x519830 over the now-closed four-stage chain:
            // ground face -> suspension contact -> bump push -> body wall.
            // One shared native matrix stack mirrors the original outer Push/Load.
            auto native_f=f;
            std::array<std::uint8_t,0x1800> event{},native_event{};
            std::array<std::uint8_t,0x800> work{},native_work{};
            std::array<std::uint8_t,0x1600> params{},native_params{};
            std::array<std::uint8_t,4*0xf4> wheels{},native_wheels{};
            std::array<std::uint8_t,0x100> contacts{},native_contacts{};
            std::array<std::uint8_t,20*8> primary{};
            std::array<std::uint8_t,20*12> recovery{};
            std::array<std::uint8_t,0x78> stage_records{},native_stage_records{};
            std::array<std::uint8_t,16> stage_desc{},native_stage_desc{},fallback_desc{},native_fallback_desc{};
            std::array<std::uint8_t,4*64> reroute{},native_reroute{},crush_ranges{},native_crush_ranges{},friction_ranges{},native_friction_ranges{};
            std::array<std::uint8_t,32> material_masks{},native_material_masks{};
            std::array<std::uint8_t,8*64> material_commands{},native_material_commands{};
            std::array<std::uint8_t,64> cw_commands{},native_cw_commands{};
            std::array<std::uint8_t,128> sound_entries{},native_sound_entries{};
            std::array<std::uint8_t,8> sound_state{},native_sound_state{},stage_cache{},native_stage_cache{};
            std::array<std::array<std::uint8_t,0x100>,4> surfaces{},native_surfaces{};
            std::array<std::uint8_t,16> material_modes{{0,1,1,0xc2,0,1,1,0xc2,1,0x42,2,3,1,0x42,2,3}};
            Bytes ee(event.data(),event.size()),ww(work.data(),work.size()),pp(params.data(),params.size()),wh(wheels.data(),wheels.size());
            Bytes ct(contacts.data(),contacts.size()),pri(primary.data(),primary.size()),rec(recovery.data(),recovery.size());
            Bytes sr(stage_records.data(),stage_records.size()),sd(stage_desc.data(),stage_desc.size()),fd(fallback_desc.data(),fallback_desc.size());
            Bytes rr(reroute.data(),reroute.size()),cr(crush_ranges.data(),crush_ranges.size()),fr(friction_ranges.data(),friction_ranges.size());
            Bytes mm(material_masks.data(),material_masks.size()),mc(material_commands.data(),material_commands.size()),cc(cw_commands.data(),cw_commands.size());
            Bytes sq(sound_entries.data(),sound_entries.size()),ss(sound_state.data(),sound_state.size()),sc(stage_cache.data(),stage_cache.size());

            const unsigned route_case=i%6u;
            const unsigned geometry=(i/6u)%5u;
            const unsigned scenario=(i/30u)%9u;
            static constexpr std::uint8_t geometry_material[]={10,9,3,11,0};
            const auto material=geometry_material[geometry];
            const unsigned type=(route_case>=3u)?(((i/6u)&1u)?3u:2u):0u;
            for(auto& a:surfaces)a.fill(material);
            for(unsigned k=0;k<16;++k){rr.put16(k*4u,0xffffu);cr.put16(k*4u,0xffffu);fr.put16(k*4u,0xffffu);}
            for(unsigned k=0;k<20;++k){pri.put32(k*8u,0u);pri.putf(k*8u+4u,0.01f);rec.putf(k*12u,0.25f+float(k)*0.03125f);}
            for(unsigned k=0;k<16;++k)cc.put32(k*4u,2000u+k*7u);
            for(unsigned k=0;k<8;++k){mm.put32(k*4u,1u<<k);for(unsigned st=0;st<16;++st)mc.put32(k*64u+st*4u,3000u+k*64u+st);}
            for(unsigned k=0;k<32;++k)sq.put32(k*4u,1000u+k);ss.put32(0,31u);ss.put32(4,0u);
            sc.put32(0,0xffffffffu);sc.put32(4,0u);
            sr.put32(4,100u);sr.put32(8,(scenario==4u||scenario==5u)?1u:0u);sr.put32(0x14,WreckerArena+0x5600u);sd.puti(0,0);fd.puti(0,0);

            // The same body-shape points are transformed into world probes and
            // are then consumed again by Cbw if the aggregate kind is a wall.
            const std::int32_t contact_count=1+std::int32_t((i/270u)%4u);ww.puti(0x68c,contact_count);
            const bool miss=(scenario==6u);
            for(std::int32_t k=0;k<contact_count;++k){
                const float magnitude=0.55f+0.15f*float(k)+float((i+k)%11u)*0.0078125f;
                outrun::testing::world_put_probe(ww,0x690u+std::size_t(k)*12u,{miss?magnitude:-magnitude,0.0f,float((k&1)?-1:1)*0.20f});
            }
            for(unsigned k=0;k<16;++k)ww.putf(0x10u+k*4u,0.0f);
            ww.putf(0x10,1.0f);ww.putf(0x24,1.0f);ww.putf(0x38,1.0f);ww.putf(0x4c,1.0f);
            outrun::testing::world_put_probe(ww,0x40,{0,0,0});outrun::testing::world_put_probe(ww,0x628,{0,1,0});
            ww.put32(0x244,(scenario>=2u&&scenario<=6u)?8u:0u);
            ww.put32(4,0u);
            ww.put32(0x248,WreckerArena+0x4000u);ww.put32(0x24c,WreckerArena+0x4000u+0xf4u);
            ww.put32(0x250,WreckerArena+0x4000u+2*0xf4u);ww.put32(0x254,WreckerArena+0x4000u+3*0xf4u);
            ww.putf(0x224,float(int(i%9u)-4)*0.0078125f);

            const std::uint32_t yaw_flag=(i%3u)==0u?0x80000000u:0u;
            ee.put32(0,8u);ee.put32(4,(route_case>=3u?1u:0u)|yaw_flag);ee.put32(8,0u);
            ee.putf(0x2e8,(i&1u)?0.03125f:-0.03125f);
            ee.put32(0x5c,0u);ee.puti(0x60,0);ee.put16(0x64,0u);ee.put16(0x66,0u);ee.put32(0x68,100u);
            ee.put32(0x1f4,200u+std::uint32_t(i%17u));ee.put32(0x2a8,0x20u);
            ee.put32(0xdf8,scenario==5u?1u:0u);ee.putf(0x2c8,0.0f);ee.putf(0x2f8,0.0f);ee.putf(0xdb4,0.0f);ee.putf(0xe64,1.0f);
            ee.put8(0xd23,1u);ee.put8(0x282,0u);ee.put8(0x284,0u);ee.puti(0xdec,0);
            ee.putf(0x1c4,1.0f);ee.putf(0xdbc,1.0f);ee.put16(0x160,0u);ee.put16(0x2e,0u);ee.put16(0x286,0u);
            ee.put16(0xd4c,0u);ee.put16(0xd4e,(scenario==4u||scenario==5u)?4000u:scenario==8u?2000u:0u);
            ee.putf(0x26c,scenario==8u?-1.0f:0.25f);
            const float jitter=float(int((i/45u)%5u)-2)*0.25f;
            const float degrees=scenario==1u?1.0f+0.25f*jitter:scenario==2u?30.0f+jitter:scenario==3u?60.0f+jitter:(scenario==4u||scenario==5u)?21.0f+0.25f*jitter:scenario==7u?12.0f+jitter:scenario==8u?15.0f+jitter:0.0f;
            const float radians=degrees*0x1.1df46ap-6f;
            outrun::testing::world_put_probe(ww,0x5c,{std::sin(radians),0.0f,std::cos(radians)});
            outrun::testing::world_put_probe(ee,0x14,{5.0f,9.0f,5.0f});ee.put32(0x2b4,WreckerArena+0x2000u);
            pp.putf(0x260,float(int(i%11u)-5)*0.015625f);pp.putf(0xb48,0.35f+float(i%5u)*0.03125f);pp.putf(0xb94,0.45f+float((i/5u)%5u)*0.03125f);pp.put32(0x15f8,0xa5000000u^(i*2654435761u));
            for(unsigned k=0;k<4;++k){
                const auto o=std::size_t(k)*0xf4u;wh.putf(o+0x04,(k&1u)?0.8f:-0.8f);wh.putf(o+0x08,0.5f);
                wh.putf(o+0x0c,k<2u?-1.0f:1.0f);wh.putf(o+0x28,0.25f);wh.putf(o+0x3c,-9876.5f);
                wh.put32(o+0x10,0u);wh.put32(o+0x14,1u);
            }
            for(unsigned axle=0;axle<2;++axle){pp.putf((axle+0x26u)*0x4cu,0.25f);pp.putf((axle+0x10u)*0x4cu,0.10f);}

            native_event=event;native_work=work;native_params=params;native_wheels=wheels;native_contacts=contacts;
            native_stage_records=stage_records;native_stage_desc=stage_desc;native_fallback_desc=fallback_desc;native_reroute=reroute;
            native_crush_ranges=crush_ranges;native_friction_ranges=friction_ranges;native_material_masks=material_masks;
            native_material_commands=material_commands;native_cw_commands=cw_commands;native_sound_entries=sound_entries;native_sound_state=sound_state;
            native_stage_cache=stage_cache;native_surfaces=surfaces;

            constexpr std::array<std::uint32_t,16> pages{{0x5a4000u,0x5c2000u,0x5c4000u,0x5e0000u,0x5e1000u,0x5e2000u,0x5e3000u,0x634000u,0x635000u,0x79f000u,0x7d2000u,0x7d3000u,0x7dd000u,0x7de000u,0x82e000u,0x956000u}};
            std::array<std::array<std::uint8_t,4096>,pages.size()> saved{};
            for(std::size_t k=0;k<pages.size();++k)std::memcpy(saved[k].data(),reinterpret_cast<void*>(pages[k]),4096);
            auto restore=[&]{for(std::size_t k=0;k<pages.size();++k)std::memcpy(reinterpret_cast<void*>(pages[k]),saved[k].data(),4096);};
            try{
                // Response thresholds and mutable gameplay tables used by the
                // original parent.  Values are kept explicit so prior oracle
                // selectors cannot leak state into this integrated case.
                *reinterpret_cast<float*>(0x5e3044u)=0x1.657186p-4f;*reinterpret_cast<float*>(0x5e3020u)=0x1.657186p-3f;
                *reinterpret_cast<float*>(0x5e3040u)=0x1.becde6p-2f;*reinterpret_cast<float*>(0x5c4020u)=0x1.921fb6p-1f;
                *reinterpret_cast<float*>(0x5e303cu)=0x1.657186p-2f;*reinterpret_cast<float*>(0x5e3038u)=0x1.41b2f8p-2f;
                *reinterpret_cast<float*>(0x5e302cu)=2608.0f;*reinterpret_cast<float*>(0x5e3030u)=-2608.0f;
                std::memcpy(reinterpret_cast<void*>(0x5e0de0u),material_modes.data(),material_modes.size());
                std::memcpy(reinterpret_cast<void*>(0x5e0f20u),crush_ranges.data(),crush_ranges.size());std::memcpy(reinterpret_cast<void*>(0x5e1fa0u),friction_ranges.data(),friction_ranges.size());
                std::memcpy(reinterpret_cast<void*>(0x635f2cu),stage_cache.data(),stage_cache.size());*reinterpret_cast<std::uint8_t*>(0x7de418u)=0u;

                // Closed CwCrash/PlWrecker dependencies reached by high-impact
                // response branches.
                *reinterpret_cast<float*>(0x5e3028u)=-170.0f;*reinterpret_cast<float*>(0x5e0ac0u)=-130.0f;
                *reinterpret_cast<float*>(0x5a460cu)=216.720001220703125f;*reinterpret_cast<float*>(0x5c20e0u)=-20.0f;*reinterpret_cast<float*>(0x5e3024u)=-33.0f;
                std::memcpy(reinterpret_cast<void*>(0x5e08e8u),primary.data(),primary.size());std::memcpy(reinterpret_cast<void*>(0x5e0988u),recovery.data(),recovery.size());
                std::memcpy(reinterpret_cast<void*>(0x5c2570u),reroute.data(),reroute.size());std::memcpy(reinterpret_cast<void*>(0x5e0f00u),material_masks.data(),material_masks.size());std::memcpy(reinterpret_cast<void*>(0x5e0df0u),cw_commands.data(),cw_commands.size());
                std::memcpy(reinterpret_cast<void*>(WreckerArena+0x5000u),material_commands.data(),material_commands.size());for(unsigned k=0;k<8;++k)*reinterpret_cast<std::uint32_t*>(0x5e0ee0u+k*4u)=WreckerArena+0x5000u+k*64u;
                std::memcpy(reinterpret_cast<void*>(WreckerArena+0x5400u),stage_records.data(),stage_records.size());std::memcpy(reinterpret_cast<void*>(WreckerArena+0x5600u),stage_desc.data(),stage_desc.size());std::memcpy(reinterpret_cast<void*>(WreckerArena+0x5640u),fallback_desc.data(),fallback_desc.size());
                *reinterpret_cast<std::uint32_t*>(0x7d33bcu)=WreckerArena+0x5400u;*reinterpret_cast<std::int32_t*>(0x7d33c4u)=1;*reinterpret_cast<std::uint32_t*>(0x7d2df4u)=WreckerArena+0x5640u;
                std::memcpy(reinterpret_cast<void*>(0x9563e8u),sound_entries.data(),sound_entries.size());*reinterpret_cast<std::uint32_t*>(0x9560c0u)=ss.u32(0);*reinterpret_cast<std::uint32_t*>(0x956124u)=ss.u32(4);*reinterpret_cast<std::uint8_t*>(0x79fcc7u)=0u;
                *reinterpret_cast<std::uint32_t*>(0x82e7d8u)=0u;*reinterpret_cast<std::uint8_t*>(0x82e7d4u)=3u;*reinterpret_cast<float*>(0x634b34u)=0.5f;
                const std::uint32_t mode=route_case>=3u?3u:0u,variant=0u;*reinterpret_cast<std::uint32_t*>(0x780258u)=mode;*reinterpret_cast<std::uint32_t*>(0x78024cu)=variant;
                Bytes configured(bind.configured.data(),bind.configured.size());configured.put32(0x258,mode);configured.put32(0x24c,variant);
                for(unsigned t=0;t<4;++t){const auto guest=WreckerArena+0x6000u+t*0x100u;std::memcpy(reinterpret_cast<void*>(guest),surfaces[t].data(),surfaces[t].size());*reinterpret_cast<std::uint32_t*>(0x780170u+t*4u)=guest;configured.put32(0x170u+t*4u,guest);}
                prepare(0x4493e0u);guest_call.st0=1;run();const float blend=ffrom(guest_call.out_st0);const auto scene=*reinterpret_cast<std::uint8_t*>(0x82e7d4u);

                std::memcpy(reinterpret_cast<void*>(WreckerArena),event.data(),event.size());std::memcpy(reinterpret_cast<void*>(0x82e7f0u),work.data(),work.size());
                std::memcpy(reinterpret_cast<void*>(WreckerArena+0x2000u),params.data(),params.size());std::memcpy(reinterpret_cast<void*>(WreckerArena+0x4000u),wheels.data(),wheels.size());
                set_bk_selector_inputs(type==3u?1u:0u,false,false,false);
                prepare(0x519830u);Bytes st(reinterpret_cast<void*>(S),32);st.put32(0,WreckerArena);st.put32(4,0x82e7f0u);run();bind.capture_globals();

                auto tables=native_f.tables();auto stack=native_f.matrix();auto prediction=native_f.prediction();CourseWorldQuery query{tables,stack,prediction};
                std::array<PcCourseEndView,4> ends={{{std::nullopt,tables.courses[0].runs.lengths},{std::nullopt,tables.courses[1].runs.lengths},{std::nullopt,tables.courses[2].runs.lengths},{std::nullopt,tables.courses[3].runs.lengths}}};
                for(unsigned t=0;t<4;++t)if(tables.courses[t].runs.present)ends[t].header=tables.courses[t].runs.header;
                std::vector<Bytes> descriptors{Bytes(native_stage_desc.data(),native_stage_desc.size())};PcStageViews stages{Bytes(native_stage_records.data(),native_stage_records.size()),1,descriptors,Bytes(native_fallback_desc.data(),native_fallback_desc.size())};
                std::array<std::uint8_t,56> choices{};std::array<std::uint8_t,0x34> selection{};std::array<std::uint8_t,0x400> route_save{};PcRouteContext route{Bytes(choices.data(),choices.size()),Bytes(selection.data(),selection.size()),Bytes(route_save.data(),route_save.size()),0,0,0,{0,0,0}};
                WallResponseContext response{stages,Bytes(native_crush_ranges.data(),native_crush_ranges.size()),Bytes(native_friction_ranges.data(),native_friction_ranges.size()),0,2608.0f,-2608.0f};
                PcSoundQueue queue{Bytes(native_sound_entries.data(),native_sound_entries.size()),Bytes(native_sound_state.data(),native_sound_state.size()),0u};WallReboundContext rebound{response,Bytes(native_stage_cache.data(),native_stage_cache.size()),route,queue};
                PcCourseAdvanceContext advance{ends,stages,Bytes(native_stage_cache.data(),native_stage_cache.size()),route,100u,0,false};auto nb=native_f.bytes();PcRoadInfoContext road{tables,stack,{nb.f32(0x4638),nb.f32(0x463c)}};PcDispMatrixContext display{stack,blend,scene};PcPlWreckerContext pl{advance,road,query,display};
                PcCrashWreckerContext wrecker{Bytes(native_work.data(),native_work.size()),Bytes(native_params.data(),native_params.size()),Bytes(native_wheels.data(),native_wheels.size()),&pl};PcCrashTables crash_tables{Bytes(primary.data(),primary.size()),Bytes(recovery.data(),recovery.size()),{}};PcImpactFeedback feedback{Bytes(nullptr,0),Bytes(nullptr,0),false};PcCrashEntryContext crash{crash_tables,stages,Bytes(native_reroute.data(),native_reroute.size()),feedback,&wrecker};
                PcCrushSelection crush{-170.0f,-130.0f,216.720001220703125f,-20.0f,-33.0f,0x1.657186p-3f,Bytes(native_cw_commands.data(),native_cw_commands.size())};
                std::array<Bytes,8> material_views={Bytes(native_material_commands.data()+0*64u,64u),Bytes(native_material_commands.data()+1*64u,64u),Bytes(native_material_commands.data()+2*64u,64u),Bytes(native_material_commands.data()+3*64u,64u),Bytes(native_material_commands.data()+4*64u,64u),Bytes(native_material_commands.data()+5*64u,64u),Bytes(native_material_commands.data()+6*64u,64u),Bytes(native_material_commands.data()+7*64u,64u)};PcMaterialSounds materials{Bytes(native_material_masks.data(),native_material_masks.size()),material_views};
                std::array<Bytes,4> surface_views={Bytes(native_surfaces[0].data(),native_surfaces[0].size()),Bytes(native_surfaces[1].data(),native_surfaces[1].size()),Bytes(native_surfaces[2].data(),native_surfaces[2].size()),Bytes(native_surfaces[3].data(),native_surfaces[3].size())};
                PcWallControllerContext controller{tables,surface_views,Bytes(material_modes.data(),material_modes.size()),rebound,crash,crush,materials,ends,mode,variant};
                PcBkQueryContext bk{mode,type==3u?1u:0u,1u,false,false,false};
                PcBodyWallContext body{query,bk,controller,Bytes(native_params.data(),native_params.size()),Bytes(native_wheels.data(),native_wheels.size()),pl};
                std::array<Bytes,4> wheel_views={Bytes(native_wheels.data()+0*0xf4u,0xf4u),Bytes(native_wheels.data()+1*0xf4u,0xf4u),Bytes(native_wheels.data()+2*0xf4u,0xf4u),Bytes(native_wheels.data()+3*0xf4u,0xf4u)};
                GroundCollisionContext ground{query,{nb.f32(0x4638u),nb.f32(0x463cu)}};
                PcColiCarContext coli{ground,body,Bytes(native_params.data(),native_params.size()),wheel_views};
                try{coli_car(Bytes(native_event.data(),native_event.size()),Bytes(native_work.data(),native_work.size()),coli);}
                catch(const std::exception& ex){throw std::runtime_error(std::string("ColiCar r028 case=")+std::to_string(i)+": "+ex.what());}
                native_f.save(stack,prediction);

                const std::string name=std::string("coli_car_r")+std::to_string(route_case)+"_g"+std::to_string(geometry)+"_s"+std::to_string(scenario);
                compare_world_bytes(name+"_event",reinterpret_cast<void*>(WreckerArena),native_event.data(),native_event.size());compare_world_bytes(name+"_work",reinterpret_cast<void*>(0x82e7f0u),native_work.data(),native_work.size());
                compare_world_bytes(name+"_params",reinterpret_cast<void*>(WreckerArena+0x2000u),native_params.data(),native_params.size());compare_world_bytes(name+"_wheels",reinterpret_cast<void*>(WreckerArena+0x4000u),native_wheels.data(),native_wheels.size());
                compare_world_bytes(name+"_stage_cache",reinterpret_cast<void*>(0x635f2cu),native_stage_cache.data(),native_stage_cache.size());compare_world_bytes(name+"_sound_entries",reinterpret_cast<void*>(0x9563e8u),native_sound_entries.data(),native_sound_entries.size());
                std::array<std::uint8_t,8> original_sound_state{};Bytes oss(original_sound_state.data(),original_sound_state.size());oss.put32(0,*reinterpret_cast<std::uint32_t*>(0x9560c0u));oss.put32(4,*reinterpret_cast<std::uint32_t*>(0x956124u));compare_world_bytes(name+"_sound_state",original_sound_state.data(),native_sound_state.data(),native_sound_state.size());bind.compare(name,native_f);
            }catch(...){restore();throw;}
            restore();continue;
        }
        if(id==28u){
            // r028 closes CarBodyWallColiCheck 0x504CE0.  The wrapper itself
            // performs every body probe, chooses normal/BK world lookup, builds
            // its private contact records and dispatches no-wall, tow or Cbw.
            // Both worlds start independent; no x86 result is resynchronised.
            auto native_f=f;
            std::array<std::uint8_t,0x1800> event{},native_event{};
            std::array<std::uint8_t,0x800> work{},native_work{};
            std::array<std::uint8_t,0x1600> params{},native_params{};
            std::array<std::uint8_t,4*0xf4> wheels{},native_wheels{};
            std::array<std::uint8_t,0x100> contacts{},native_contacts{};
            std::array<std::uint8_t,20*8> primary{};
            std::array<std::uint8_t,20*12> recovery{};
            std::array<std::uint8_t,0x78> stage_records{},native_stage_records{};
            std::array<std::uint8_t,16> stage_desc{},native_stage_desc{},fallback_desc{},native_fallback_desc{};
            std::array<std::uint8_t,4*64> reroute{},native_reroute{},crush_ranges{},native_crush_ranges{},friction_ranges{},native_friction_ranges{};
            std::array<std::uint8_t,32> material_masks{},native_material_masks{};
            std::array<std::uint8_t,8*64> material_commands{},native_material_commands{};
            std::array<std::uint8_t,64> cw_commands{},native_cw_commands{};
            std::array<std::uint8_t,128> sound_entries{},native_sound_entries{};
            std::array<std::uint8_t,8> sound_state{},native_sound_state{},stage_cache{},native_stage_cache{};
            std::array<std::array<std::uint8_t,0x100>,4> surfaces{},native_surfaces{};
            std::array<std::uint8_t,16> material_modes{{0,1,1,0xc2,0,1,1,0xc2,1,0x42,2,3,1,0x42,2,3}};
            Bytes ee(event.data(),event.size()),ww(work.data(),work.size()),pp(params.data(),params.size()),wh(wheels.data(),wheels.size());
            Bytes ct(contacts.data(),contacts.size()),pri(primary.data(),primary.size()),rec(recovery.data(),recovery.size());
            Bytes sr(stage_records.data(),stage_records.size()),sd(stage_desc.data(),stage_desc.size()),fd(fallback_desc.data(),fallback_desc.size());
            Bytes rr(reroute.data(),reroute.size()),cr(crush_ranges.data(),crush_ranges.size()),fr(friction_ranges.data(),friction_ranges.size());
            Bytes mm(material_masks.data(),material_masks.size()),mc(material_commands.data(),material_commands.size()),cc(cw_commands.data(),cw_commands.size());
            Bytes sq(sound_entries.data(),sound_entries.size()),ss(sound_state.data(),sound_state.size()),sc(stage_cache.data(),stage_cache.size());

            const unsigned route_case=i%6u;
            const unsigned geometry=(i/6u)%5u;
            const unsigned scenario=(i/30u)%9u;
            static constexpr std::uint8_t geometry_material[]={10,9,3,11,0};
            const auto material=geometry_material[geometry];
            const unsigned type=(route_case>=3u)?(((i/6u)&1u)?3u:2u):0u;
            for(auto& a:surfaces)a.fill(material);
            for(unsigned k=0;k<16;++k){rr.put16(k*4u,0xffffu);cr.put16(k*4u,0xffffu);fr.put16(k*4u,0xffffu);}
            for(unsigned k=0;k<20;++k){pri.put32(k*8u,0u);pri.putf(k*8u+4u,0.01f);rec.putf(k*12u,0.25f+float(k)*0.03125f);}
            for(unsigned k=0;k<16;++k)cc.put32(k*4u,2000u+k*7u);
            for(unsigned k=0;k<8;++k){mm.put32(k*4u,1u<<k);for(unsigned st=0;st<16;++st)mc.put32(k*64u+st*4u,3000u+k*64u+st);}
            for(unsigned k=0;k<32;++k)sq.put32(k*4u,1000u+k);ss.put32(0,31u);ss.put32(4,0u);
            sc.put32(0,0xffffffffu);sc.put32(4,0u);
            sr.put32(4,100u);sr.put32(8,(scenario==4u||scenario==5u)?1u:0u);sr.put32(0x14,WreckerArena+0x5600u);sd.puti(0,0);fd.puti(0,0);

            // The same body-shape points are transformed into world probes and
            // are then consumed again by Cbw if the aggregate kind is a wall.
            const std::int32_t contact_count=1+std::int32_t((i/270u)%4u);ww.puti(0x68c,contact_count);
            const bool miss=(scenario==6u);
            for(std::int32_t k=0;k<contact_count;++k){
                const float magnitude=0.55f+0.15f*float(k)+float((i+k)%11u)*0.0078125f;
                outrun::testing::world_put_probe(ww,0x690u+std::size_t(k)*12u,{miss?magnitude:-magnitude,0.0f,float((k&1)?-1:1)*0.20f});
            }
            for(unsigned k=0;k<16;++k)ww.putf(0x10u+k*4u,0.0f);
            ww.putf(0x10,1.0f);ww.putf(0x24,1.0f);ww.putf(0x38,1.0f);ww.putf(0x4c,1.0f);
            outrun::testing::world_put_probe(ww,0x40,{0,0,0});outrun::testing::world_put_probe(ww,0x628,{0,1,0});
            ww.put32(0x244,(scenario>=2u&&scenario<=6u)?8u:0u);
            ww.put32(4,0u);
            ww.put32(0x248,WreckerArena+0x4000u);ww.put32(0x24c,WreckerArena+0x4000u+0xf4u);
            ww.put32(0x250,WreckerArena+0x4000u+2*0xf4u);ww.put32(0x254,WreckerArena+0x4000u+3*0xf4u);
            ww.putf(0x224,float(int(i%9u)-4)*0.0078125f);

            const std::uint32_t yaw_flag=(i%3u)==0u?0x80000000u:0u;
            ee.put32(0,8u);ee.put32(4,(route_case>=3u?1u:0u)|yaw_flag);ee.put32(8,0u);
            ee.putf(0x2e8,(i&1u)?0.03125f:-0.03125f);
            ee.put32(0x5c,0u);ee.puti(0x60,0);ee.put16(0x64,0u);ee.put16(0x66,0u);ee.put32(0x68,100u);
            ee.put32(0x1f4,200u+std::uint32_t(i%17u));ee.put32(0x2a8,0x20u);
            ee.put32(0xdf8,scenario==5u?1u:0u);ee.putf(0x2c8,0.0f);ee.putf(0x2f8,0.0f);ee.putf(0xdb4,0.0f);ee.putf(0xe64,1.0f);
            ee.put8(0xd23,1u);ee.put8(0x282,0u);ee.put8(0x284,0u);ee.puti(0xdec,0);
            ee.putf(0x1c4,1.0f);ee.putf(0xdbc,1.0f);ee.put16(0x160,0u);ee.put16(0x2e,0u);ee.put16(0x286,0u);
            ee.put16(0xd4c,0u);ee.put16(0xd4e,(scenario==4u||scenario==5u)?4000u:scenario==8u?2000u:0u);
            ee.putf(0x26c,scenario==8u?-1.0f:0.25f);
            const float jitter=float(int((i/45u)%5u)-2)*0.25f;
            const float degrees=scenario==1u?1.0f+0.25f*jitter:scenario==2u?30.0f+jitter:scenario==3u?60.0f+jitter:(scenario==4u||scenario==5u)?21.0f+0.25f*jitter:scenario==7u?12.0f+jitter:scenario==8u?15.0f+jitter:0.0f;
            const float radians=degrees*0x1.1df46ap-6f;
            outrun::testing::world_put_probe(ww,0x5c,{std::sin(radians),0.0f,std::cos(radians)});
            outrun::testing::world_put_probe(ee,0x14,{5.0f,9.0f,5.0f});ee.put32(0x2b4,WreckerArena+0x2000u);
            pp.putf(0x260,float(int(i%11u)-5)*0.015625f);pp.putf(0xb48,0.35f+float(i%5u)*0.03125f);pp.putf(0xb94,0.45f+float((i/5u)%5u)*0.03125f);pp.put32(0x15f8,0xa5000000u^(i*2654435761u));
            for(unsigned k=0;k<4;++k)wh.putf(std::size_t(k)*0xf4u+0x08,0.75f+0.125f*k+float(i%7u)*0.0078125f);

            native_event=event;native_work=work;native_params=params;native_wheels=wheels;native_contacts=contacts;
            native_stage_records=stage_records;native_stage_desc=stage_desc;native_fallback_desc=fallback_desc;native_reroute=reroute;
            native_crush_ranges=crush_ranges;native_friction_ranges=friction_ranges;native_material_masks=material_masks;
            native_material_commands=material_commands;native_cw_commands=cw_commands;native_sound_entries=sound_entries;native_sound_state=sound_state;
            native_stage_cache=stage_cache;native_surfaces=surfaces;

            constexpr std::array<std::uint32_t,16> pages{{0x5a4000u,0x5c2000u,0x5c4000u,0x5e0000u,0x5e1000u,0x5e2000u,0x5e3000u,0x634000u,0x635000u,0x79f000u,0x7d2000u,0x7d3000u,0x7dd000u,0x7de000u,0x82e000u,0x956000u}};
            std::array<std::array<std::uint8_t,4096>,pages.size()> saved{};
            for(std::size_t k=0;k<pages.size();++k)std::memcpy(saved[k].data(),reinterpret_cast<void*>(pages[k]),4096);
            auto restore=[&]{for(std::size_t k=0;k<pages.size();++k)std::memcpy(reinterpret_cast<void*>(pages[k]),saved[k].data(),4096);};
            try{
                // Response thresholds and mutable gameplay tables used by the
                // original parent.  Values are kept explicit so prior oracle
                // selectors cannot leak state into this integrated case.
                *reinterpret_cast<float*>(0x5e3044u)=0x1.657186p-4f;*reinterpret_cast<float*>(0x5e3020u)=0x1.657186p-3f;
                *reinterpret_cast<float*>(0x5e3040u)=0x1.becde6p-2f;*reinterpret_cast<float*>(0x5c4020u)=0x1.921fb6p-1f;
                *reinterpret_cast<float*>(0x5e303cu)=0x1.657186p-2f;*reinterpret_cast<float*>(0x5e3038u)=0x1.41b2f8p-2f;
                *reinterpret_cast<float*>(0x5e302cu)=2608.0f;*reinterpret_cast<float*>(0x5e3030u)=-2608.0f;
                std::memcpy(reinterpret_cast<void*>(0x5e0de0u),material_modes.data(),material_modes.size());
                std::memcpy(reinterpret_cast<void*>(0x5e0f20u),crush_ranges.data(),crush_ranges.size());std::memcpy(reinterpret_cast<void*>(0x5e1fa0u),friction_ranges.data(),friction_ranges.size());
                std::memcpy(reinterpret_cast<void*>(0x635f2cu),stage_cache.data(),stage_cache.size());*reinterpret_cast<std::uint8_t*>(0x7de418u)=0u;

                // Closed CwCrash/PlWrecker dependencies reached by high-impact
                // response branches.
                *reinterpret_cast<float*>(0x5e3028u)=-170.0f;*reinterpret_cast<float*>(0x5e0ac0u)=-130.0f;
                *reinterpret_cast<float*>(0x5a460cu)=216.720001220703125f;*reinterpret_cast<float*>(0x5c20e0u)=-20.0f;*reinterpret_cast<float*>(0x5e3024u)=-33.0f;
                std::memcpy(reinterpret_cast<void*>(0x5e08e8u),primary.data(),primary.size());std::memcpy(reinterpret_cast<void*>(0x5e0988u),recovery.data(),recovery.size());
                std::memcpy(reinterpret_cast<void*>(0x5c2570u),reroute.data(),reroute.size());std::memcpy(reinterpret_cast<void*>(0x5e0f00u),material_masks.data(),material_masks.size());std::memcpy(reinterpret_cast<void*>(0x5e0df0u),cw_commands.data(),cw_commands.size());
                std::memcpy(reinterpret_cast<void*>(WreckerArena+0x5000u),material_commands.data(),material_commands.size());for(unsigned k=0;k<8;++k)*reinterpret_cast<std::uint32_t*>(0x5e0ee0u+k*4u)=WreckerArena+0x5000u+k*64u;
                std::memcpy(reinterpret_cast<void*>(WreckerArena+0x5400u),stage_records.data(),stage_records.size());std::memcpy(reinterpret_cast<void*>(WreckerArena+0x5600u),stage_desc.data(),stage_desc.size());std::memcpy(reinterpret_cast<void*>(WreckerArena+0x5640u),fallback_desc.data(),fallback_desc.size());
                *reinterpret_cast<std::uint32_t*>(0x7d33bcu)=WreckerArena+0x5400u;*reinterpret_cast<std::int32_t*>(0x7d33c4u)=1;*reinterpret_cast<std::uint32_t*>(0x7d2df4u)=WreckerArena+0x5640u;
                std::memcpy(reinterpret_cast<void*>(0x9563e8u),sound_entries.data(),sound_entries.size());*reinterpret_cast<std::uint32_t*>(0x9560c0u)=ss.u32(0);*reinterpret_cast<std::uint32_t*>(0x956124u)=ss.u32(4);*reinterpret_cast<std::uint8_t*>(0x79fcc7u)=0u;
                *reinterpret_cast<std::uint32_t*>(0x82e7d8u)=0u;*reinterpret_cast<std::uint8_t*>(0x82e7d4u)=3u;*reinterpret_cast<float*>(0x634b34u)=0.5f;
                const std::uint32_t mode=route_case>=3u?3u:0u,variant=0u;*reinterpret_cast<std::uint32_t*>(0x780258u)=mode;*reinterpret_cast<std::uint32_t*>(0x78024cu)=variant;
                Bytes configured(bind.configured.data(),bind.configured.size());configured.put32(0x258,mode);configured.put32(0x24c,variant);
                for(unsigned t=0;t<4;++t){const auto guest=WreckerArena+0x6000u+t*0x100u;std::memcpy(reinterpret_cast<void*>(guest),surfaces[t].data(),surfaces[t].size());*reinterpret_cast<std::uint32_t*>(0x780170u+t*4u)=guest;configured.put32(0x170u+t*4u,guest);}
                prepare(0x4493e0u);guest_call.st0=1;run();const float blend=ffrom(guest_call.out_st0);const auto scene=*reinterpret_cast<std::uint8_t*>(0x82e7d4u);

                std::memcpy(reinterpret_cast<void*>(WreckerArena),event.data(),event.size());std::memcpy(reinterpret_cast<void*>(0x82e7f0u),work.data(),work.size());
                std::memcpy(reinterpret_cast<void*>(WreckerArena+0x2000u),params.data(),params.size());std::memcpy(reinterpret_cast<void*>(WreckerArena+0x4000u),wheels.data(),wheels.size());
                set_bk_selector_inputs(type==3u?1u:0u,false,false,false);
                prepare(0x504ce0u);Bytes st(reinterpret_cast<void*>(S),32);st.put32(0,WreckerArena);st.put32(4,0x82e7f0u);run();bind.capture_globals();

                auto tables=native_f.tables();auto stack=native_f.matrix();auto prediction=native_f.prediction();CourseWorldQuery query{tables,stack,prediction};
                std::array<PcCourseEndView,4> ends={{{std::nullopt,tables.courses[0].runs.lengths},{std::nullopt,tables.courses[1].runs.lengths},{std::nullopt,tables.courses[2].runs.lengths},{std::nullopt,tables.courses[3].runs.lengths}}};
                for(unsigned t=0;t<4;++t)if(tables.courses[t].runs.present)ends[t].header=tables.courses[t].runs.header;
                std::vector<Bytes> descriptors{Bytes(native_stage_desc.data(),native_stage_desc.size())};PcStageViews stages{Bytes(native_stage_records.data(),native_stage_records.size()),1,descriptors,Bytes(native_fallback_desc.data(),native_fallback_desc.size())};
                std::array<std::uint8_t,56> choices{};std::array<std::uint8_t,0x34> selection{};std::array<std::uint8_t,0x400> route_save{};PcRouteContext route{Bytes(choices.data(),choices.size()),Bytes(selection.data(),selection.size()),Bytes(route_save.data(),route_save.size()),0,0,0,{0,0,0}};
                WallResponseContext response{stages,Bytes(native_crush_ranges.data(),native_crush_ranges.size()),Bytes(native_friction_ranges.data(),native_friction_ranges.size()),0,2608.0f,-2608.0f};
                PcSoundQueue queue{Bytes(native_sound_entries.data(),native_sound_entries.size()),Bytes(native_sound_state.data(),native_sound_state.size()),0u};WallReboundContext rebound{response,Bytes(native_stage_cache.data(),native_stage_cache.size()),route,queue};
                PcCourseAdvanceContext advance{ends,stages,Bytes(native_stage_cache.data(),native_stage_cache.size()),route,100u,0,false};auto nb=native_f.bytes();PcRoadInfoContext road{tables,stack,{nb.f32(0x4638),nb.f32(0x463c)}};PcDispMatrixContext display{stack,blend,scene};PcPlWreckerContext pl{advance,road,query,display};
                PcCrashWreckerContext wrecker{Bytes(native_work.data(),native_work.size()),Bytes(native_params.data(),native_params.size()),Bytes(native_wheels.data(),native_wheels.size()),&pl};PcCrashTables crash_tables{Bytes(primary.data(),primary.size()),Bytes(recovery.data(),recovery.size()),{}};PcImpactFeedback feedback{Bytes(nullptr,0),Bytes(nullptr,0),false};PcCrashEntryContext crash{crash_tables,stages,Bytes(native_reroute.data(),native_reroute.size()),feedback,&wrecker};
                PcCrushSelection crush{-170.0f,-130.0f,216.720001220703125f,-20.0f,-33.0f,0x1.657186p-3f,Bytes(native_cw_commands.data(),native_cw_commands.size())};
                std::array<Bytes,8> material_views={Bytes(native_material_commands.data()+0*64u,64u),Bytes(native_material_commands.data()+1*64u,64u),Bytes(native_material_commands.data()+2*64u,64u),Bytes(native_material_commands.data()+3*64u,64u),Bytes(native_material_commands.data()+4*64u,64u),Bytes(native_material_commands.data()+5*64u,64u),Bytes(native_material_commands.data()+6*64u,64u),Bytes(native_material_commands.data()+7*64u,64u)};PcMaterialSounds materials{Bytes(native_material_masks.data(),native_material_masks.size()),material_views};
                std::array<Bytes,4> surface_views={Bytes(native_surfaces[0].data(),native_surfaces[0].size()),Bytes(native_surfaces[1].data(),native_surfaces[1].size()),Bytes(native_surfaces[2].data(),native_surfaces[2].size()),Bytes(native_surfaces[3].data(),native_surfaces[3].size())};
                PcWallControllerContext controller{tables,surface_views,Bytes(material_modes.data(),material_modes.size()),rebound,crash,crush,materials,ends,mode,variant};
                PcBkQueryContext bk{mode,type==3u?1u:0u,1u,false,false,false};
                PcBodyWallContext body{query,bk,controller,Bytes(native_params.data(),native_params.size()),Bytes(native_wheels.data(),native_wheels.size()),pl};
                try{car_body_wall_coli_check(Bytes(native_event.data(),native_event.size()),Bytes(native_work.data(),native_work.size()),body);}
                catch(const std::exception& ex){throw std::runtime_error(std::string("CarBodyWallColiCheck r028 case=")+std::to_string(i)+": "+ex.what());}
                native_f.save(stack,prediction);

                const std::string name=std::string("car_body_wall_r")+std::to_string(route_case)+"_g"+std::to_string(geometry)+"_s"+std::to_string(scenario);
                compare_world_bytes(name+"_event",reinterpret_cast<void*>(WreckerArena),native_event.data(),native_event.size());compare_world_bytes(name+"_work",reinterpret_cast<void*>(0x82e7f0u),native_work.data(),native_work.size());
                compare_world_bytes(name+"_params",reinterpret_cast<void*>(WreckerArena+0x2000u),native_params.data(),native_params.size());compare_world_bytes(name+"_wheels",reinterpret_cast<void*>(WreckerArena+0x4000u),native_wheels.data(),native_wheels.size());
                compare_world_bytes(name+"_stage_cache",reinterpret_cast<void*>(0x635f2cu),native_stage_cache.data(),native_stage_cache.size());compare_world_bytes(name+"_sound_entries",reinterpret_cast<void*>(0x9563e8u),native_sound_entries.data(),native_sound_entries.size());
                std::array<std::uint8_t,8> original_sound_state{};Bytes oss(original_sound_state.data(),original_sound_state.size());oss.put32(0,*reinterpret_cast<std::uint32_t*>(0x9560c0u));oss.put32(4,*reinterpret_cast<std::uint32_t*>(0x956124u));compare_world_bytes(name+"_sound_state",original_sound_state.data(),native_sound_state.data(),native_sound_state.size());bind.compare(name,native_f);
            }catch(...){restore();throw;}
            restore();continue;
        }
        if(id==55u){
            // 43F110: the racer-branch course query (504E70): branch kind 2 -> 43EB60,
            // else types 3 (kind 1) / 2 with the type-0 retry. Argument 0 is never read.
            auto native_f=f;auto fb=f.bytes();auto nb=native_f.bytes();
            const std::uint32_t branch=i%4u;                         // 0, 1, 2, 3 (3: as 0, type 2)
            set_bk_selector_inputs(branch,false,false,false);
            prepare(0x43f110u);Bytes st(reinterpret_cast<void*>(S),32);
            st.put32(0,0x1234u+i);st.put32(4,fb.u32(0x4640));st.put32(8,WorldArena+0x4600u);
            const auto opts=fb.u32(0x4644);
            st.put32(12,opts&1u?WorldArena+0x4620u:0u);
            st.put32(16,opts&2u?WorldArena+0x4624u:0u);
            st.put32(20,opts&4u?WorldArena+0x4628u:0u);
            run();const auto original=guest_call.out_eax;bind.capture_globals();
            auto tables=native_f.tables();auto stack=native_f.matrix();auto prediction=native_f.prediction();
            CourseWorldQuery query{tables,stack,prediction};auto point=outrun::testing::world_probe(nb,0x4600u);
            auto polygon=nb.u32(0x4620u),special=nb.u32(0x4624u),kind=nb.u32(0x4628u);
            const auto native=get_y_position_branch_43f110(query,branch,nb.u32(0x4640u),point,
                opts&1u?&polygon:nullptr,opts&2u?&special:nullptr,opts&4u?&kind:nullptr);
            outrun::testing::world_put_probe(nb,0x4600u,point);nb.put32(0x4620u,polygon);nb.put32(0x4624u,special);nb.put32(0x4628u,kind);
            native_f.save(stack,prediction);
            const std::string name=std::string("get_y_position_branch_43f110_b")+std::to_string(branch);
            compare_u32(name+"_return",original,inject_mismatch&&i==1u?native^1u:native);bind.compare(name,native_f);
            continue;
        }
        if(id==27u){
            // r028 closes the PC-only GetYPositionProg_BK 0x43EEE0 selector
            // over both ordinary fallbacks and its type-2/type-3 special path.
            // Platform getters are deterministic oracle inputs; the original
            // x86 matrix/grid/GetRoadCond implementation remains untouched.
            auto native_f=f;auto fb=f.bytes();auto nb=native_f.bytes();
            const unsigned selector=i%8u;
            std::uint32_t game_mode=3u,branch=0u,mode4_gate=1u;
            bool gate0=false,gate1=false,gate2=false;
            switch(selector){
                case 0: game_mode=3u;branch=1u;break;                    // easy=0 -> ordinary fallback
                case 1: game_mode=3u;branch=2u;break;                    // branch record 2 -> ordinary fallback
                case 2: game_mode=3u;branch=1u;break;                    // active type 3
                case 3: game_mode=3u;branch=0u;break;                    // active type 2
                case 4: game_mode=4u;branch=1u;mode4_gate=1u;break;      // mode-4 explicit gate, type 3
                case 5: game_mode=4u;branch=0u;mode4_gate=0u;break;      // mode-4 gate off -> ordinary
                case 6: game_mode=0u;branch=0u;break;                    // external gates all false -> ordinary
                default:game_mode=0u;branch=(i&8u)?1u:0u;                // one external gate -> special
                        if((i/8u)%3u==0u)gate0=true;else if((i/8u)%3u==1u)gate1=true;else gate2=true;break;
            }
            set_bk_selector_inputs(branch,gate0,gate1,gate2);
            const auto saved_mode=*reinterpret_cast<std::uint32_t*>(0x780258u);
            const auto saved_mode4_ptr=*reinterpret_cast<std::uint32_t*>(0x799d18u);
            *reinterpret_cast<std::uint32_t*>(0x780258u)=game_mode;
            *reinterpret_cast<std::uint32_t*>(0x799d18u)=BkStateBase+0x100u;
            *reinterpret_cast<std::uint32_t*>(BkStateBase+0x15cu)=mode4_gate;
            Bytes configured(bind.configured.data(),bind.configured.size());configured.put32(0x258,game_mode);
            const auto restore=[&]{*reinterpret_cast<std::uint32_t*>(0x780258u)=saved_mode;*reinterpret_cast<std::uint32_t*>(0x799d18u)=saved_mode4_ptr;};
            try{
                prepare(0x43eee0u);Bytes st(reinterpret_cast<void*>(S),32);
                st.put32(0,fb.u32(0x4640));st.put32(4,WorldArena+0x4600u);
                const auto opts=fb.u32(0x4644);
                st.put32(8, opts&1u?WorldArena+0x4620u:0u);
                st.put32(12,opts&2u?WorldArena+0x4624u:0u);
                st.put32(16,opts&4u?WorldArena+0x4628u:0u);
                run();const auto original=guest_call.out_eax;bind.capture_globals();

                auto tables=native_f.tables();auto stack=native_f.matrix();auto prediction=native_f.prediction();
                CourseWorldQuery query{tables,stack,prediction};auto point=outrun::testing::world_probe(nb,0x4600u);
                auto polygon=nb.u32(0x4620u),special=nb.u32(0x4624u),kind=nb.u32(0x4628u);
                PcBkQueryContext ctx{game_mode,branch,mode4_gate,gate0,gate1,gate2};
                const auto native=get_y_position_prog_bk(query,ctx,nb.u32(0x4640u),point,
                    opts&1u?&polygon:nullptr,opts&2u?&special:nullptr,opts&4u?&kind:nullptr);
                outrun::testing::world_put_probe(nb,0x4600u,point);nb.put32(0x4620u,polygon);nb.put32(0x4624u,special);nb.put32(0x4628u,kind);
                native_f.save(stack,prediction);
                const std::string name=std::string("get_y_position_prog_bk_s")+std::to_string(selector);
                compare_u32(name+"_return",original,native);bind.compare(name,native_f);
            }catch(...){restore();throw;}
            restore();continue;
        }
        if(id==26u){
            // r027 closes the complete CbwColiWall 0x5041B0 controller.  The
            // x86 and native paths start from independent copies and exercise
            // geometry selection, push-out/ColiSet, friction, rebound and the
            // already closed crush->tow chain without result resynchronisation.
            auto native_f=f;
            std::array<std::uint8_t,0x1800> event{},native_event{};
            std::array<std::uint8_t,0x800> work{},native_work{};
            std::array<std::uint8_t,0x1600> params{},native_params{};
            std::array<std::uint8_t,4*0xf4> wheels{},native_wheels{};
            std::array<std::uint8_t,0x100> contacts{},native_contacts{};
            std::array<std::uint8_t,20*8> primary{};
            std::array<std::uint8_t,20*12> recovery{};
            std::array<std::uint8_t,0x78> stage_records{},native_stage_records{};
            std::array<std::uint8_t,16> stage_desc{},native_stage_desc{},fallback_desc{},native_fallback_desc{};
            std::array<std::uint8_t,4*64> reroute{},native_reroute{},crush_ranges{},native_crush_ranges{},friction_ranges{},native_friction_ranges{};
            std::array<std::uint8_t,32> material_masks{},native_material_masks{};
            std::array<std::uint8_t,8*64> material_commands{},native_material_commands{};
            std::array<std::uint8_t,64> cw_commands{},native_cw_commands{};
            std::array<std::uint8_t,128> sound_entries{},native_sound_entries{};
            std::array<std::uint8_t,8> sound_state{},native_sound_state{},stage_cache{},native_stage_cache{};
            std::array<std::array<std::uint8_t,0x100>,4> surfaces{},native_surfaces{};
            std::array<std::uint8_t,16> material_modes{{0,1,1,0xc2,0,1,1,0xc2,1,0x42,2,3,1,0x42,2,3}};
            Bytes ee(event.data(),event.size()),ww(work.data(),work.size()),pp(params.data(),params.size()),wh(wheels.data(),wheels.size());
            Bytes ct(contacts.data(),contacts.size()),pri(primary.data(),primary.size()),rec(recovery.data(),recovery.size());
            Bytes sr(stage_records.data(),stage_records.size()),sd(stage_desc.data(),stage_desc.size()),fd(fallback_desc.data(),fallback_desc.size());
            Bytes rr(reroute.data(),reroute.size()),cr(crush_ranges.data(),crush_ranges.size()),fr(friction_ranges.data(),friction_ranges.size());
            Bytes mm(material_masks.data(),material_masks.size()),mc(material_commands.data(),material_commands.size()),cc(cw_commands.data(),cw_commands.size());
            Bytes sq(sound_entries.data(),sound_entries.size()),ss(sound_state.data(),sound_state.size()),sc(stage_cache.data(),stage_cache.size());

            const unsigned geometry=i%5u;
            const unsigned scenario=(i/5u)%9u;
            static constexpr std::uint8_t geometry_material[]={10,9,3,11,0};
            const auto material=geometry_material[geometry];
            const unsigned type=(i/45u)%4u;
            for(auto& a:surfaces)a.fill(material);
            for(unsigned k=0;k<16;++k){rr.put16(k*4u,0xffffu);cr.put16(k*4u,0xffffu);fr.put16(k*4u,0xffffu);}
            for(unsigned k=0;k<20;++k){pri.put32(k*8u,0u);pri.putf(k*8u+4u,0.01f);rec.putf(k*12u,0.25f+float(k)*0.03125f);}
            for(unsigned k=0;k<16;++k)cc.put32(k*4u,2000u+k*7u);
            for(unsigned k=0;k<8;++k){mm.put32(k*4u,1u<<k);for(unsigned st=0;st<16;++st)mc.put32(k*64u+st*4u,3000u+k*64u+st);}
            for(unsigned k=0;k<32;++k)sq.put32(k*4u,1000u+k);ss.put32(0,31u);ss.put32(4,0u);
            sc.put32(0,0xffffffffu);sc.put32(4,0u);
            sr.put32(4,100u);sr.put32(8,(scenario==4u||scenario==5u)?1u:0u);sr.put32(0x14,WreckerArena+0x5600u);sd.puti(0,0);fd.puti(0,0);

            // Cbw's same count drives both contact records and the face-shape
            // points at work+0x690.  A positive-X shape in scenario 6 makes
            // ColiSet return false and covers the parent's immediate exit.
            const std::int32_t contact_count=1+std::int32_t((i/180u)%4u);ww.puti(0x68c,contact_count);
            const bool miss=(scenario==6u);
            for(std::int32_t k=0;k<contact_count;++k){
                const float magnitude=0.55f+0.15f*float(k)+float((i+k)%11u)*0.0078125f;
                outrun::testing::world_put_probe(ww,0x690u+std::size_t(k)*12u,{miss?magnitude:-magnitude,0.0f,float((k&1)?-1:1)*0.20f});
            }
            for(unsigned k=0;k<16;++k)ww.putf(0x10u+k*4u,0.0f);
            ww.putf(0x10,1.0f);ww.putf(0x24,1.0f);ww.putf(0x38,1.0f);ww.putf(0x4c,1.0f);
            outrun::testing::world_put_probe(ww,0x40,{0,0,0});outrun::testing::world_put_probe(ww,0x628,{0,1,0});
            ww.put32(0x244,(scenario>=2u&&scenario<=6u)?8u:0u);
            ww.put32(4,0u);
            ww.put32(0x248,WreckerArena+0x4000u);ww.put32(0x24c,WreckerArena+0x4000u+0xf4u);
            ww.put32(0x250,WreckerArena+0x4000u+2*0xf4u);ww.put32(0x254,WreckerArena+0x4000u+3*0xf4u);
            ww.putf(0x224,float(int(i%9u)-4)*0.0078125f);

            // Two contact records; every second case makes record 1 a skip so
            // both loop paths participate while the first qualifying face is
            // always defined.
            for(std::int32_t k=0;k<contact_count;++k){
                const auto o=std::size_t(k)*16u;ct.put32(o,1u);ct.put32(o+12u,type);
                bool qualifying=(k==0)||(((i+unsigned(k))&1u)==0u);
                if(contact_count>1&&(i%7u)==0u){qualifying=(k==1);}
                ct.put32(o+8u,qualifying?0x20u:0u);
            }

            const std::uint32_t yaw_flag=(i%3u)==0u?0x80000000u:0u;
            ee.put32(0,8u);ee.put32(4,(scenario==0u?0u:1u)|yaw_flag);ee.put32(8,0u);
            ee.putf(0x2e8,(i&1u)?0.03125f:-0.03125f);
            ee.put32(0x5c,0u);ee.puti(0x60,0);ee.put16(0x64,0u);ee.put16(0x66,0u);ee.put32(0x68,100u);
            ee.put32(0x1f4,200u+std::uint32_t(i%17u));ee.put32(0x2a8,0x20u);
            ee.put32(0xdf8,scenario==5u?1u:0u);ee.putf(0x2c8,0.0f);ee.putf(0x2f8,0.0f);ee.putf(0xdb4,0.0f);ee.putf(0xe64,1.0f);
            ee.put8(0xd23,1u);ee.put8(0x282,0u);ee.put8(0x284,0u);ee.puti(0xdec,0);
            ee.putf(0x1c4,1.0f);ee.putf(0xdbc,1.0f);ee.put16(0x160,0u);ee.put16(0x2e,0u);ee.put16(0x286,0u);
            ee.put16(0xd4c,0u);ee.put16(0xd4e,(scenario==4u||scenario==5u)?4000u:scenario==8u?2000u:0u);
            ee.putf(0x26c,scenario==8u?-1.0f:0.25f);
            const float jitter=float(int((i/45u)%5u)-2)*0.25f;
            const float degrees=scenario==1u?1.0f+0.25f*jitter:scenario==2u?30.0f+jitter:scenario==3u?60.0f+jitter:(scenario==4u||scenario==5u)?21.0f+0.25f*jitter:scenario==7u?12.0f+jitter:scenario==8u?15.0f+jitter:0.0f;
            const float radians=degrees*0x1.1df46ap-6f;
            outrun::testing::world_put_probe(ww,0x5c,{std::sin(radians),0.0f,std::cos(radians)});
            outrun::testing::world_put_probe(ee,0x14,{5.0f,9.0f,5.0f});ee.put32(0x2b4,WreckerArena+0x2000u);
            pp.putf(0x260,float(int(i%11u)-5)*0.015625f);pp.putf(0xb48,0.35f+float(i%5u)*0.03125f);pp.putf(0xb94,0.45f+float((i/5u)%5u)*0.03125f);pp.put32(0x15f8,0xa5000000u^(i*2654435761u));
            for(unsigned k=0;k<4;++k)wh.putf(std::size_t(k)*0xf4u+0x08,0.75f+0.125f*k+float(i%7u)*0.0078125f);

            native_event=event;native_work=work;native_params=params;native_wheels=wheels;native_contacts=contacts;
            native_stage_records=stage_records;native_stage_desc=stage_desc;native_fallback_desc=fallback_desc;native_reroute=reroute;
            native_crush_ranges=crush_ranges;native_friction_ranges=friction_ranges;native_material_masks=material_masks;
            native_material_commands=material_commands;native_cw_commands=cw_commands;native_sound_entries=sound_entries;native_sound_state=sound_state;
            native_stage_cache=stage_cache;native_surfaces=surfaces;

            constexpr std::array<std::uint32_t,16> pages{{0x5a4000u,0x5c2000u,0x5c4000u,0x5e0000u,0x5e1000u,0x5e2000u,0x5e3000u,0x634000u,0x635000u,0x79f000u,0x7d2000u,0x7d3000u,0x7dd000u,0x7de000u,0x82e000u,0x956000u}};
            std::array<std::array<std::uint8_t,4096>,pages.size()> saved{};
            for(std::size_t k=0;k<pages.size();++k)std::memcpy(saved[k].data(),reinterpret_cast<void*>(pages[k]),4096);
            auto restore=[&]{for(std::size_t k=0;k<pages.size();++k)std::memcpy(reinterpret_cast<void*>(pages[k]),saved[k].data(),4096);};
            try{
                // Response thresholds and mutable gameplay tables used by the
                // original parent.  Values are kept explicit so prior oracle
                // selectors cannot leak state into this integrated case.
                *reinterpret_cast<float*>(0x5e3044u)=0x1.657186p-4f;*reinterpret_cast<float*>(0x5e3020u)=0x1.657186p-3f;
                *reinterpret_cast<float*>(0x5e3040u)=0x1.becde6p-2f;*reinterpret_cast<float*>(0x5c4020u)=0x1.921fb6p-1f;
                *reinterpret_cast<float*>(0x5e303cu)=0x1.657186p-2f;*reinterpret_cast<float*>(0x5e3038u)=0x1.41b2f8p-2f;
                *reinterpret_cast<float*>(0x5e302cu)=2608.0f;*reinterpret_cast<float*>(0x5e3030u)=-2608.0f;
                std::memcpy(reinterpret_cast<void*>(0x5e0de0u),material_modes.data(),material_modes.size());
                std::memcpy(reinterpret_cast<void*>(0x5e0f20u),crush_ranges.data(),crush_ranges.size());std::memcpy(reinterpret_cast<void*>(0x5e1fa0u),friction_ranges.data(),friction_ranges.size());
                std::memcpy(reinterpret_cast<void*>(0x635f2cu),stage_cache.data(),stage_cache.size());*reinterpret_cast<std::uint8_t*>(0x7de418u)=0u;

                // Closed CwCrash/PlWrecker dependencies reached by high-impact
                // response branches.
                *reinterpret_cast<float*>(0x5e3028u)=-170.0f;*reinterpret_cast<float*>(0x5e0ac0u)=-130.0f;
                *reinterpret_cast<float*>(0x5a460cu)=216.720001220703125f;*reinterpret_cast<float*>(0x5c20e0u)=-20.0f;*reinterpret_cast<float*>(0x5e3024u)=-33.0f;
                std::memcpy(reinterpret_cast<void*>(0x5e08e8u),primary.data(),primary.size());std::memcpy(reinterpret_cast<void*>(0x5e0988u),recovery.data(),recovery.size());
                std::memcpy(reinterpret_cast<void*>(0x5c2570u),reroute.data(),reroute.size());std::memcpy(reinterpret_cast<void*>(0x5e0f00u),material_masks.data(),material_masks.size());std::memcpy(reinterpret_cast<void*>(0x5e0df0u),cw_commands.data(),cw_commands.size());
                std::memcpy(reinterpret_cast<void*>(WreckerArena+0x5000u),material_commands.data(),material_commands.size());for(unsigned k=0;k<8;++k)*reinterpret_cast<std::uint32_t*>(0x5e0ee0u+k*4u)=WreckerArena+0x5000u+k*64u;
                std::memcpy(reinterpret_cast<void*>(WreckerArena+0x5400u),stage_records.data(),stage_records.size());std::memcpy(reinterpret_cast<void*>(WreckerArena+0x5600u),stage_desc.data(),stage_desc.size());std::memcpy(reinterpret_cast<void*>(WreckerArena+0x5640u),fallback_desc.data(),fallback_desc.size());
                *reinterpret_cast<std::uint32_t*>(0x7d33bcu)=WreckerArena+0x5400u;*reinterpret_cast<std::int32_t*>(0x7d33c4u)=1;*reinterpret_cast<std::uint32_t*>(0x7d2df4u)=WreckerArena+0x5640u;
                std::memcpy(reinterpret_cast<void*>(0x9563e8u),sound_entries.data(),sound_entries.size());*reinterpret_cast<std::uint32_t*>(0x9560c0u)=ss.u32(0);*reinterpret_cast<std::uint32_t*>(0x956124u)=ss.u32(4);*reinterpret_cast<std::uint8_t*>(0x79fcc7u)=0u;
                *reinterpret_cast<std::uint32_t*>(0x82e7d8u)=0u;*reinterpret_cast<std::uint8_t*>(0x82e7d4u)=3u;*reinterpret_cast<float*>(0x634b34u)=0.5f;
                const std::uint32_t mode=0u,variant=0u;*reinterpret_cast<std::uint32_t*>(0x780258u)=mode;*reinterpret_cast<std::uint32_t*>(0x78024cu)=variant;
                Bytes configured(bind.configured.data(),bind.configured.size());configured.put32(0x258,mode);configured.put32(0x24c,variant);
                for(unsigned t=0;t<4;++t){const auto guest=WreckerArena+0x6000u+t*0x100u;std::memcpy(reinterpret_cast<void*>(guest),surfaces[t].data(),surfaces[t].size());*reinterpret_cast<std::uint32_t*>(0x780170u+t*4u)=guest;configured.put32(0x170u+t*4u,guest);}
                prepare(0x4493e0u);guest_call.st0=1;run();const float blend=ffrom(guest_call.out_st0);const auto scene=*reinterpret_cast<std::uint8_t*>(0x82e7d4u);

                std::memcpy(reinterpret_cast<void*>(WreckerArena),event.data(),event.size());std::memcpy(reinterpret_cast<void*>(0x82e7f0u),work.data(),work.size());
                std::memcpy(reinterpret_cast<void*>(WreckerArena+0x1800u),contacts.data(),contacts.size());std::memcpy(reinterpret_cast<void*>(WreckerArena+0x2000u),params.data(),params.size());std::memcpy(reinterpret_cast<void*>(WreckerArena+0x4000u),wheels.data(),wheels.size());
                prepare(0x5041b0u);Bytes st(reinterpret_cast<void*>(S),32);st.put32(0,WreckerArena);st.put32(4,0x82e7f0u);st.put32(8,WreckerArena+0x1800u);run();bind.capture_globals();

                auto tables=native_f.tables();auto stack=native_f.matrix();auto prediction=native_f.prediction();CourseWorldQuery query{tables,stack,prediction};
                std::array<PcCourseEndView,4> ends={{{std::nullopt,tables.courses[0].runs.lengths},{std::nullopt,tables.courses[1].runs.lengths},{std::nullopt,tables.courses[2].runs.lengths},{std::nullopt,tables.courses[3].runs.lengths}}};
                for(unsigned t=0;t<4;++t)if(tables.courses[t].runs.present)ends[t].header=tables.courses[t].runs.header;
                std::vector<Bytes> descriptors{Bytes(native_stage_desc.data(),native_stage_desc.size())};PcStageViews stages{Bytes(native_stage_records.data(),native_stage_records.size()),1,descriptors,Bytes(native_fallback_desc.data(),native_fallback_desc.size())};
                std::array<std::uint8_t,56> choices{};std::array<std::uint8_t,0x34> selection{};std::array<std::uint8_t,0x400> route_save{};PcRouteContext route{Bytes(choices.data(),choices.size()),Bytes(selection.data(),selection.size()),Bytes(route_save.data(),route_save.size()),0,0,0,{0,0,0}};
                WallResponseContext response{stages,Bytes(native_crush_ranges.data(),native_crush_ranges.size()),Bytes(native_friction_ranges.data(),native_friction_ranges.size()),0,2608.0f,-2608.0f};
                PcSoundQueue queue{Bytes(native_sound_entries.data(),native_sound_entries.size()),Bytes(native_sound_state.data(),native_sound_state.size()),0u};WallReboundContext rebound{response,Bytes(native_stage_cache.data(),native_stage_cache.size()),route,queue};
                PcCourseAdvanceContext advance{ends,stages,Bytes(native_stage_cache.data(),native_stage_cache.size()),route,100u,0,false};auto nb=native_f.bytes();PcRoadInfoContext road{tables,stack,{nb.f32(0x4638),nb.f32(0x463c)}};PcDispMatrixContext display{stack,blend,scene};PcPlWreckerContext pl{advance,road,query,display};
                PcCrashWreckerContext wrecker{Bytes(native_work.data(),native_work.size()),Bytes(native_params.data(),native_params.size()),Bytes(native_wheels.data(),native_wheels.size()),&pl};PcCrashTables crash_tables{Bytes(primary.data(),primary.size()),Bytes(recovery.data(),recovery.size()),{}};PcImpactFeedback feedback{Bytes(nullptr,0),Bytes(nullptr,0),false};PcCrashEntryContext crash{crash_tables,stages,Bytes(native_reroute.data(),native_reroute.size()),feedback,&wrecker};
                PcCrushSelection crush{-170.0f,-130.0f,216.720001220703125f,-20.0f,-33.0f,0x1.657186p-3f,Bytes(native_cw_commands.data(),native_cw_commands.size())};
                std::array<Bytes,8> material_views={Bytes(native_material_commands.data()+0*64u,64u),Bytes(native_material_commands.data()+1*64u,64u),Bytes(native_material_commands.data()+2*64u,64u),Bytes(native_material_commands.data()+3*64u,64u),Bytes(native_material_commands.data()+4*64u,64u),Bytes(native_material_commands.data()+5*64u,64u),Bytes(native_material_commands.data()+6*64u,64u),Bytes(native_material_commands.data()+7*64u,64u)};PcMaterialSounds materials{Bytes(native_material_masks.data(),native_material_masks.size()),material_views};
                std::array<Bytes,4> surface_views={Bytes(native_surfaces[0].data(),native_surfaces[0].size()),Bytes(native_surfaces[1].data(),native_surfaces[1].size()),Bytes(native_surfaces[2].data(),native_surfaces[2].size()),Bytes(native_surfaces[3].data(),native_surfaces[3].size())};
                PcWallControllerContext controller{tables,surface_views,Bytes(material_modes.data(),material_modes.size()),rebound,crash,crush,materials,ends,mode,variant};
                try{cbw_coli_wall(Bytes(native_event.data(),native_event.size()),Bytes(native_work.data(),native_work.size()),Bytes(native_contacts.data(),native_contacts.size()),stack,controller);}catch(const std::exception& ex){throw std::runtime_error(std::string("CbwColiWall r027 case=")+std::to_string(i)+": "+ex.what());}
                native_f.save(stack,prediction);

                const std::string name=std::string("cbw_coli_wall_g")+std::to_string(geometry)+"_s"+std::to_string(scenario);
                compare_world_bytes(name+"_event",reinterpret_cast<void*>(WreckerArena),native_event.data(),native_event.size());compare_world_bytes(name+"_work",reinterpret_cast<void*>(0x82e7f0u),native_work.data(),native_work.size());
                compare_world_bytes(name+"_params",reinterpret_cast<void*>(WreckerArena+0x2000u),native_params.data(),native_params.size());compare_world_bytes(name+"_wheels",reinterpret_cast<void*>(WreckerArena+0x4000u),native_wheels.data(),native_wheels.size());compare_world_bytes(name+"_contacts",reinterpret_cast<void*>(WreckerArena+0x1800u),native_contacts.data(),native_contacts.size());
                compare_world_bytes(name+"_stage_cache",reinterpret_cast<void*>(0x635f2cu),native_stage_cache.data(),native_stage_cache.size());compare_world_bytes(name+"_sound_entries",reinterpret_cast<void*>(0x9563e8u),native_sound_entries.data(),native_sound_entries.size());
                std::array<std::uint8_t,8> original_sound_state{};Bytes oss(original_sound_state.data(),original_sound_state.size());oss.put32(0,*reinterpret_cast<std::uint32_t*>(0x9560c0u));oss.put32(4,*reinterpret_cast<std::uint32_t*>(0x956124u));compare_world_bytes(name+"_sound_state",original_sound_state.data(),native_sound_state.data(),native_sound_state.size());bind.compare(name,native_f);
            }catch(...){restore();throw;}
            restore();continue;
        }
        if(id==25u){
            // r026 closes CwCrushStatus 0x5038D0 over an active tow domain.
            // Original and native start from independent event/work/world/sound
            // copies; no x86 result is copied into the native state.
            auto native_f=f;
            std::array<std::uint8_t,0x1800> event{},native_event{};
            std::array<std::uint8_t,0x800> work{},native_work{};
            std::array<std::uint8_t,0x1600> params{},native_params{};
            std::array<std::uint8_t,4*0xf4> wheels{},native_wheels{};
            std::array<std::uint8_t,0x100> contacts{},native_contacts{};
            std::array<std::uint8_t,20*8> primary{};
            std::array<std::uint8_t,20*12> recovery{};
            std::array<std::uint8_t,0x78> stage_records{},native_stage_records{};
            std::array<std::uint8_t,16> stage_desc{},native_stage_desc{},fallback_desc{},native_fallback_desc{};
            std::array<std::uint8_t,4*64> reroute{},native_reroute{},crush_ranges{},native_crush_ranges{};
            std::array<std::uint8_t,32> material_masks{},native_material_masks{};
            std::array<std::uint8_t,8*64> material_commands{},native_material_commands{};
            std::array<std::uint8_t,64> cw_commands{},native_cw_commands{};
            std::array<std::uint8_t,128> sound_entries{},native_sound_entries{};
            std::array<std::uint8_t,8> sound_state{},native_sound_state{};
            std::mt19937 rng(0x5038d026u^(i*1664525u));
            for(auto& v:event)v=std::uint8_t(rng());for(auto& v:work)v=std::uint8_t(rng());
            for(auto& v:params)v=std::uint8_t(rng());for(auto& v:wheels)v=std::uint8_t(rng());
            for(auto& v:contacts)v=std::uint8_t(rng());
            Bytes ee(event.data(),event.size()),ww(work.data(),work.size()),pp(params.data(),params.size()),wh(wheels.data(),wheels.size());
            Bytes ct(contacts.data(),contacts.size()),pri(primary.data(),primary.size()),rec(recovery.data(),recovery.size());
            Bytes sr(stage_records.data(),stage_records.size()),sd(stage_desc.data(),stage_desc.size()),fd(fallback_desc.data(),fallback_desc.size());
            Bytes rr(reroute.data(),reroute.size()),cr(crush_ranges.data(),crush_ranges.size());
            Bytes mm(material_masks.data(),material_masks.size()),mc(material_commands.data(),material_commands.size());
            Bytes cc(cw_commands.data(),cw_commands.size()),sq(sound_entries.data(),sound_entries.size()),ss(sound_state.data(),sound_state.size());

            static constexpr float durations[]={0.01f,0.025f,0.05f,0.25f};
            const float duration=durations[i%4u];
            const std::int32_t expected_phase=duration==0.01f?0:duration==0.025f?1:duration==0.05f?3:15;
            const unsigned desired=(i/4u)%3u; // 0=state1, 1=state5, 2=state2 before optional reroute.
            const bool trapped=expected_phase<=1&&((i/12u)&1u);
            const bool reroute_state1=desired==0u&&expected_phase<=1&&((i/24u)&1u);
            const float threshold=0x1.657186p-3f;

            // Valid primary OnRoadPlace. Delayed phases mirror r025: -10 + 10
            // lands on the synthetic course boundary without route transition.
            ee.put32(0,8u);ee.put32(0x5c,0u);ee.puti(0x60,0);ee.put16(0x64,std::uint16_t(expected_phase>1?-10:0));
            ee.put16(0x66,0);ee.put32(0x68,100u);ee.puti(0x1c0,0);ee.puti(0xdec,int(i%241u)-40);
            ee.putf(0x2c8,0.0f);ee.putf(0x2f8,0.0f);ee.put32(0xdf8,0u);ee.putf(0xdb4,0.0f);ee.putf(0xe64,1.0f);
            ee.put32(0x1f4,desired==0u?(trapped?351u:197u):(trapped?250u:170u));
            const float angle=desired==1u?threshold:std::nextafter(threshold,-INFINITY);
            ee.put16(0x1fe,static_cast<std::uint16_t>(0x1400u+(i&0x1ffu)));
            ee.putf(0x1c4,0.05f+float(i%37u)*0.03125f);
            auto put_angle=[&](std::size_t o,std::int32_t v){ee.put16(o,static_cast<std::uint16_t>(static_cast<std::int16_t>(v)));};
            put_angle(0x2c,int(i*31u)%1025-512);put_angle(0x30,int(i*47u)%1025-512);
            put_angle(0xc2c,int(i*13u)%513-256);put_angle(0xc2e,int(i*17u)%513-256);
            const std::uint32_t display_flags[]={0u,0x00800000u,0x80000000u,0x80800000u};
            // bit0 activates the real PlWrecker edge; bit5 stays clear so this
            // selector isolates Cw's own two sound dispatches from feedback.
            ee.put32(4,(ee.u32(4)&~0x80800021u)|1u|display_flags[(i/48u)%4u]);
            ee.put8(0xd23,0);ee.put8(0xc36,0);
            auto vec=[&](std::size_t o,float k){ee.putf(o,k);ee.putf(o+4,-k*0.5f);ee.putf(o+8,k*0.25f);};
            vec(0x2d8,float(int(i%9u)-4)*0.03125f);vec(0x1040,float(int((i/9u)%9u)-4)*0.025f);
            vec(0x1034,float(int((i/81u)%9u)-4)*0.02f);vec(0x2e4,float(int((i/729u)%9u)-4)*0.018f);
            const float offsets[]={0.0f,1.1920928955078125e-7f,0.125f,-0.25f};ee.putf(0xd24,offsets[(i/64u)%4u]);
            outrun::testing::world_put_probe(ee,0x14,{5.0f,9.0f,5.0f});
            ee.put32(0x2b4,WreckerArena+0x2000);
            ww.put32(0x248,WreckerArena+0x4000);ww.put32(0x24c,WreckerArena+0x4000+0xf4);
            ww.put32(0x250,WreckerArena+0x4000+2*0xf4);ww.put32(0x254,WreckerArena+0x4000+3*0xf4);
            ww.put32(0x670,(i/96u)&1u?0x20000000u:0u);ww.puti(0x68c,2);
            pp.putf(0x260,float(int(i%11u)-5)*0.015625f);pp.putf(0xb48,0.35f+float(i%5u)*0.03125f);pp.putf(0xb94,0.45f+float((i/5u)%5u)*0.03125f);
            pp.put32(0x15f8,0xa5000000u^(i*2654435761u));ww.putf(0x224,float(int((i/25u)%9u)-4)*0.0078125f);
            for(unsigned k=0;k<4;++k)wh.putf(std::size_t(k)*0xf4u+0x08,0.75f+0.125f*k+float(i%7u)*0.0078125f);

            // One explicit stage. Serialized descriptor pointers are only for
            // the x86 side; native pc_stage_number uses the supplied view.
            sr.put32(4,100u);sr.put32(0x14,WreckerArena+0x5600);sd.puti(0,0);fd.puti(0,0);
            for(unsigned k=0;k<16;++k){rr.put16(k*4,0xffffu);cr.put16(k*4,0xffffu);}
            if(trapped){cr.put16(0,0);cr.put16(2,0);cr.put16(4,0xffffu);}
            if(reroute_state1){rr.put16(0,0);rr.put16(2,0);rr.put16(4,0xffffu);}

            // Crash durations control the public PlWrecker phase. Every state
            // gets the same duration so a valid 1->5 reroute retains the phase.
            for(unsigned k=0;k<20;++k){pri.put32(k*8u,0);pri.putf(k*8u+4u,duration);rec.putf(k*12u,0.25f+float(k)*0.03125f);}
            for(unsigned k=0;k<16;++k)cc.put32(k*4u,2000u+k*7u);
            for(unsigned k=0;k<8;++k){mm.put32(k*4u,1u<<k);for(unsigned st=0;st<16;++st)mc.put32(k*64u+st*4u,3000u+k*64u+st);}
            const unsigned material=(i/192u)%8u;ct.put32(8,1u<<material);ct.put32(24,0u);
            for(unsigned k=0;k<32;++k)sq.put32(k*4u,1000u+k);ss.put32(0,31u);ss.put32(4,0u);

            native_event=event;native_work=work;native_params=params;native_wheels=wheels;native_contacts=contacts;
            native_stage_records=stage_records;native_stage_desc=stage_desc;native_fallback_desc=fallback_desc;
            native_reroute=reroute;native_crush_ranges=crush_ranges;native_material_masks=material_masks;
            native_material_commands=material_commands;native_cw_commands=cw_commands;
            native_sound_entries=sound_entries;native_sound_state=sound_state;

            std::array<std::uint8_t,4096> saved5a{},saved5c{},saved5e{},saved5e3{},saved79f{},saved7d2{},saved7d3{},saved82e{},saved956{};
            std::memcpy(saved5a.data(),reinterpret_cast<void*>(0x5a4000),4096);std::memcpy(saved5c.data(),reinterpret_cast<void*>(0x5c2000),4096);
            std::memcpy(saved5e.data(),reinterpret_cast<void*>(0x5e0000),4096);std::memcpy(saved5e3.data(),reinterpret_cast<void*>(0x5e3000),4096);
            std::memcpy(saved79f.data(),reinterpret_cast<void*>(0x79f000),4096);std::memcpy(saved7d2.data(),reinterpret_cast<void*>(0x7d2000),4096);
            std::memcpy(saved7d3.data(),reinterpret_cast<void*>(0x7d3000),4096);std::memcpy(saved82e.data(),reinterpret_cast<void*>(0x82e000),4096);
            std::memcpy(saved956.data(),reinterpret_cast<void*>(0x956000),4096);const auto saved_blend=*reinterpret_cast<std::uint32_t*>(0x634b34u);
            auto restore=[&]{
                std::memcpy(reinterpret_cast<void*>(0x5a4000),saved5a.data(),4096);std::memcpy(reinterpret_cast<void*>(0x5c2000),saved5c.data(),4096);
                std::memcpy(reinterpret_cast<void*>(0x5e0000),saved5e.data(),4096);std::memcpy(reinterpret_cast<void*>(0x5e3000),saved5e3.data(),4096);
                std::memcpy(reinterpret_cast<void*>(0x79f000),saved79f.data(),4096);std::memcpy(reinterpret_cast<void*>(0x7d2000),saved7d2.data(),4096);
                std::memcpy(reinterpret_cast<void*>(0x7d3000),saved7d3.data(),4096);std::memcpy(reinterpret_cast<void*>(0x82e000),saved82e.data(),4096);
                std::memcpy(reinterpret_cast<void*>(0x956000),saved956.data(),4096);*reinterpret_cast<std::uint32_t*>(0x634b34u)=saved_blend;
            };
            try{
                // Original fixed globals/tables.
                *reinterpret_cast<float*>(0x5e3028u)=-170.0f;*reinterpret_cast<float*>(0x5e0ac0u)=-130.0f;
                *reinterpret_cast<float*>(0x5a460cu)=216.720001220703125f;*reinterpret_cast<float*>(0x5c20e0u)=-20.0f;
                *reinterpret_cast<float*>(0x5e3024u)=-33.0f;*reinterpret_cast<float*>(0x5e3020u)=threshold;
                std::memcpy(reinterpret_cast<void*>(0x5e08e8),primary.data(),primary.size());std::memcpy(reinterpret_cast<void*>(0x5e0988),recovery.data(),recovery.size());
                std::memcpy(reinterpret_cast<void*>(0x5c2570),reroute.data(),reroute.size());std::memcpy(reinterpret_cast<void*>(0x5e0f20),crush_ranges.data(),64u);
                std::memcpy(reinterpret_cast<void*>(0x5e0f00),material_masks.data(),material_masks.size());std::memcpy(reinterpret_cast<void*>(0x5e0df0),cw_commands.data(),cw_commands.size());
                std::memcpy(reinterpret_cast<void*>(WreckerArena+0x5000),material_commands.data(),material_commands.size());
                for(unsigned k=0;k<8;++k)*reinterpret_cast<std::uint32_t*>(0x5e0ee0u+k*4u)=WreckerArena+0x5000u+k*64u;
                std::memcpy(reinterpret_cast<void*>(WreckerArena+0x5400),stage_records.data(),stage_records.size());
                std::memcpy(reinterpret_cast<void*>(WreckerArena+0x5600),stage_desc.data(),stage_desc.size());std::memcpy(reinterpret_cast<void*>(WreckerArena+0x5640),fallback_desc.data(),fallback_desc.size());
                *reinterpret_cast<std::uint32_t*>(0x7d33bcu)=WreckerArena+0x5400;*reinterpret_cast<std::int32_t*>(0x7d33c4u)=1;*reinterpret_cast<std::uint32_t*>(0x7d2df4u)=WreckerArena+0x5640;
                std::memcpy(reinterpret_cast<void*>(0x9563e8),sound_entries.data(),sound_entries.size());*reinterpret_cast<std::uint32_t*>(0x9560c0u)=ss.u32(0);*reinterpret_cast<std::uint32_t*>(0x956124u)=ss.u32(4);*reinterpret_cast<std::uint8_t*>(0x79fcc7u)=2u;
                *reinterpret_cast<std::uint32_t*>(0x82e7d8u)=(i%5u==0u)?1u:0u;*reinterpret_cast<std::uint8_t*>(0x82e7d4u)=static_cast<std::uint8_t>((i%6u==0u)?6u:(i%6u==1u)?15u:(i%6u==2u)?17u:3u);
                static constexpr float blends[]={0.0f,0.125f,0.25f,0.5f,0.75f,0.875f,1.0f};*reinterpret_cast<float*>(0x634b34u)=blends[(i/5u)%7u];
                const std::uint32_t mode=(i/7u)%6u,variant=(i/11u)%4u;*reinterpret_cast<std::uint32_t*>(0x780258u)=mode;*reinterpret_cast<std::uint32_t*>(0x78024cu)=variant;
                Bytes configured(bind.configured.data(),bind.configured.size());configured.put32(0x258,mode);configured.put32(0x24c,variant);
                prepare(0x4493e0u);guest_call.st0=1;run();const float blend=ffrom(guest_call.out_st0);const auto scene=*reinterpret_cast<std::uint8_t*>(0x82e7d4u);

                std::memcpy(reinterpret_cast<void*>(WreckerArena),event.data(),event.size());std::memcpy(reinterpret_cast<void*>(0x82e7f0),work.data(),work.size());
                std::memcpy(reinterpret_cast<void*>(WreckerArena+0x1800),contacts.data(),contacts.size());std::memcpy(reinterpret_cast<void*>(WreckerArena+0x2000),params.data(),params.size());
                std::memcpy(reinterpret_cast<void*>(WreckerArena+0x4000),wheels.data(),wheels.size());
                prepare(0x5038d0u);Bytes st(reinterpret_cast<void*>(S),32);guest_call.esi=WreckerArena;st.put32(0,0x82e7f0u);st.put32(4,fbits(angle));st.put32(8,WreckerArena+0x1800);run();const auto original=guest_call.out_eax;
                bind.capture_globals();

                auto tables=native_f.tables();auto stack=native_f.matrix();auto prediction=native_f.prediction();CourseWorldQuery query{tables,stack,prediction};
                std::array<PcCourseEndView,4> ends={{{std::nullopt,tables.courses[0].runs.lengths},{std::nullopt,tables.courses[1].runs.lengths},{std::nullopt,tables.courses[2].runs.lengths},{std::nullopt,tables.courses[3].runs.lengths}}};
                for(unsigned t=0;t<4;++t)if(tables.courses[t].runs.present)ends[t].header=tables.courses[t].runs.header;
                std::vector<Bytes> descriptors{Bytes(native_stage_desc.data(),native_stage_desc.size())};
                PcStageViews stages{Bytes(native_stage_records.data(),native_stage_records.size()),1,descriptors,Bytes(native_fallback_desc.data(),native_fallback_desc.size())};
                std::array<std::uint8_t,8> cache{};std::array<std::uint8_t,56> choices{};std::array<std::uint8_t,0x34> selection{};std::array<std::uint8_t,0x400> save{};
                PcRouteContext route{Bytes(choices.data(),choices.size()),Bytes(selection.data(),selection.size()),Bytes(save.data(),save.size()),0,0,0,{0,0,0}};
                PcCourseAdvanceContext advance{ends,stages,Bytes(cache.data(),cache.size()),route,100u,0,false};
                auto nb=native_f.bytes();PcRoadInfoContext road{tables,stack,{nb.f32(0x4638),nb.f32(0x463c)}};PcDispMatrixContext display{stack,blend,scene};PcPlWreckerContext pl{advance,road,query,display};
                PcCrashWreckerContext wrecker{Bytes(native_work.data(),native_work.size()),Bytes(native_params.data(),native_params.size()),Bytes(native_wheels.data(),native_wheels.size()),&pl};
                PcCrashTables crash{Bytes(primary.data(),primary.size()),Bytes(recovery.data(),recovery.size()),{}};PcImpactFeedback feedback{Bytes(nullptr,0),Bytes(nullptr,0),false};
                PcCrashEntryContext entry{crash,stages,Bytes(native_reroute.data(),native_reroute.size()),feedback,&wrecker};
                WallResponseContext wall{stages,Bytes(native_crush_ranges.data(),native_crush_ranges.size()),Bytes(native_crush_ranges.data(),native_crush_ranges.size()),0};
                PcCrushSelection tuning{-170.0f,-130.0f,216.720001220703125f,-20.0f,-33.0f,threshold,Bytes(native_cw_commands.data(),native_cw_commands.size())};
                std::array<Bytes,8> material_views={
                    Bytes(native_material_commands.data()+0*64u,64u),Bytes(native_material_commands.data()+1*64u,64u),
                    Bytes(native_material_commands.data()+2*64u,64u),Bytes(native_material_commands.data()+3*64u,64u),
                    Bytes(native_material_commands.data()+4*64u,64u),Bytes(native_material_commands.data()+5*64u,64u),
                    Bytes(native_material_commands.data()+6*64u,64u),Bytes(native_material_commands.data()+7*64u,64u)};
                PcMaterialSounds materials{Bytes(native_material_masks.data(),native_material_masks.size()),material_views};
                PcSoundQueue queue{Bytes(native_sound_entries.data(),native_sound_entries.size()),Bytes(native_sound_state.data(),native_sound_state.size()),2u};
                std::uint32_t native=0;
                try{native=pc_cw_crush_status(Bytes(native_event.data(),native_event.size()),Bytes(native_work.data(),native_work.size()),angle,Bytes(native_contacts.data(),native_contacts.size()),entry,wall,tuning,materials,queue,ends,mode,variant)?1u:0u;}
                catch(const std::exception& ex){throw std::runtime_error(std::string("CwCrushStatus r026 case=")+std::to_string(i)+": "+ex.what());}
                native_f.save(stack,prediction);

                const std::string name=std::string("cw_crush_status_")+(desired==0u?"state1":desired==1u?"state5":"state2")+(reroute_state1?"_reroute5":"")+(expected_phase<=1?"_immediate":"_delayed")+(trapped?"_trapped":"");
                compare_u32(name+"_return",original,native);compare_world_bytes(name+"_event",reinterpret_cast<void*>(WreckerArena),native_event.data(),native_event.size());
                compare_world_bytes(name+"_work",reinterpret_cast<void*>(0x82e7f0),native_work.data(),native_work.size());compare_world_bytes(name+"_params",reinterpret_cast<void*>(WreckerArena+0x2000),native_params.data(),native_params.size());
                compare_world_bytes(name+"_wheels",reinterpret_cast<void*>(WreckerArena+0x4000),native_wheels.data(),native_wheels.size());compare_world_bytes(name+"_contacts",reinterpret_cast<void*>(WreckerArena+0x1800),native_contacts.data(),native_contacts.size());
                compare_world_bytes(name+"_sound_entries",reinterpret_cast<void*>(0x9563e8),native_sound_entries.data(),native_sound_entries.size());
                std::array<std::uint8_t,8> original_sound_state{};Bytes oss(original_sound_state.data(),original_sound_state.size());oss.put32(0,*reinterpret_cast<std::uint32_t*>(0x9560c0u));oss.put32(4,*reinterpret_cast<std::uint32_t*>(0x956124u));
                compare_world_bytes(name+"_sound_state",original_sound_state.data(),native_sound_state.data(),native_sound_state.size());bind.compare(name,native_f);
            }catch(...){restore();throw;}
            restore();continue;
        }
        if(id==24u){
            // r025 parent-chain oracle: start from two independent copies of the
            // same world fixture.  Unlike older integrated cases, native state
            // is never resynchronised from the x86 result between entry,
            // PlWrecker, and the optional crash-state update.
            auto native_f=f;
            std::array<std::uint8_t,0x1800> event{},native_event{};
            std::array<std::uint8_t,0x800> work{},native_work{};
            std::array<std::uint8_t,0x1600> params{},native_params{};
            std::array<std::uint8_t,4*0xf4> wheels{},native_wheels{};
            std::array<std::uint8_t,20*8> primary{};
            std::array<std::uint8_t,20*12> recovery{};
            std::mt19937 rng(0x4a227025u^(i*1664525u));
            for(auto& v:event)v=std::uint8_t(rng());for(auto& v:work)v=std::uint8_t(rng());
            for(auto& v:params)v=std::uint8_t(rng());for(auto& v:wheels)v=std::uint8_t(rng());
            Bytes ee(event.data(),event.size()),ww(work.data(),work.size()),pp(params.data(),params.size()),wh(wheels.data(),wheels.size());
            Bytes pri(primary.data(),primary.size()),rec(recovery.data(),recovery.size());

            static constexpr float durations[]={0.01f,0.025f,0.05f,0.25f};
            const float duration=durations[i%4u];
            const bool start_wrapper=((i/4u)&1u)!=0;
            const std::uint32_t state=((i/8u)&1u)?5u:2u;
            const std::uint32_t reverse=(i/16u)&7u;
            const float rate=0.25f+float((i/128u)%13u)*0.125f;
            const std::int32_t expected_phase=duration==0.01f?0:duration==0.025f?1:duration==0.05f?3:15;

            // Valid OnRoadPlace.  Delayed phases advance by ten; starting at
            // -10 lands exactly on the synthetic course end and avoids route
            // callbacks while still exercising the real delayed public path.
            ee.put32(0x5c,0);ee.puti(0x60,0);ee.put16(0x64,std::uint16_t(expected_phase>1?-10:0));
            ee.put16(0x66,0);ee.put32(0x68,0);ee.puti(0x1c0,0);
            ee.put32(0x2f0,(ee.u32(0x2f0)&0xfffffc03u)|(state<<2u));
            ee.put16(0x1fe,static_cast<std::uint16_t>(0x1200u+(i&0x3ffu)));
            ee.putf(0x1c4,0.05f+float(i%37u)*0.03125f);ee.putf(0xdb4,float((i/37u)%9u)*0.125f);
            auto put_angle=[&](std::size_t o,std::int32_t v){ee.put16(o,static_cast<std::uint16_t>(static_cast<std::int16_t>(v)));};
            put_angle(0x2c,int(i*31u)%1025-512);put_angle(0x30,int(i*47u)%1025-512);
            put_angle(0xc2c,int(i*13u)%513-256);put_angle(0xc2e,int(i*17u)%513-256);
            const std::uint32_t display_flags[]={0u,0x00800000u,0x80000000u,0x80800000u};
            // bit0 requests the real tow path; bit5 is cleared so this corpus
            // isolates entry/wrecker ordering from already-closed feedback.
            ee.put32(4,(ee.u32(4)&~0x80800021u)|1u|display_flags[(i/32u)%4u]);
            auto vec=[&](std::size_t o,float k){ee.putf(o,k);ee.putf(o+4,-k*0.5f);ee.putf(o+8,k*0.25f);};
            vec(0x2d8,float(int(i%9u)-4)*0.03125f);vec(0x1040,float(int((i/9u)%9u)-4)*0.025f);
            vec(0x1034,float(int((i/81u)%9u)-4)*0.02f);vec(0x2e4,float(int((i/729u)%9u)-4)*0.018f);
            const float offsets[]={0.0f,1.1920928955078125e-7f,0.125f,-0.25f};ee.putf(0xd24,offsets[(i/64u)%4u]);
            outrun::testing::world_put_probe(ee,0x14,{5.0f,9.0f,5.0f});
            ee.put32(0x2b4,WreckerArena+0x3000);
            ww.put32(0x248,WreckerArena+0x5000);ww.put32(0x24c,WreckerArena+0x5000+0xf4);
            ww.put32(0x250,WreckerArena+0x5000+2*0xf4);ww.put32(0x254,WreckerArena+0x5000+3*0xf4);
            pp.putf(0x260,float(int(i%11u)-5)*0.015625f);pp.putf(0xb48,0.35f+float(i%5u)*0.03125f);pp.putf(0xb94,0.45f+float((i/5u)%5u)*0.03125f);
            pp.put32(0x15f8,0xa5000000u^(i*2654435761u));ww.putf(0x224,float(int((i/25u)%9u)-4)*0.0078125f);
            for(unsigned k=0;k<4;++k)wh.putf(std::size_t(k)*0xf4u+0x08,0.75f+0.125f*k+float(i%7u)*0.0078125f);
            for(unsigned k=0;k<20;++k){pri.put32(k*8u,0);pri.putf(k*8u+4u,duration);rec.putf(k*12u,0.25f+float(k)*0.03125f);}
            native_event=event;native_work=work;native_params=params;native_wheels=wheels;

            std::array<std::uint8_t,4096> saved82e{},saved5e{};
            std::memcpy(saved82e.data(),reinterpret_cast<void*>(0x82e000),4096);
            std::memcpy(saved5e.data(),reinterpret_cast<void*>(0x5e0000),4096);
            const auto saved_blend=*reinterpret_cast<std::uint32_t*>(0x634b34u);
            auto restore=[&]{
                std::memcpy(reinterpret_cast<void*>(0x82e000),saved82e.data(),4096);
                std::memcpy(reinterpret_cast<void*>(0x5e0000),saved5e.data(),4096);
                *reinterpret_cast<std::uint32_t*>(0x634b34u)=saved_blend;
            };
            try{
                std::memcpy(reinterpret_cast<void*>(0x5e08e8),primary.data(),primary.size());
                std::memcpy(reinterpret_cast<void*>(0x5e0988),recovery.data(),recovery.size());
                *reinterpret_cast<std::uint32_t*>(0x82e7d8u)=(i%5u==0u)?1u:0u;
                *reinterpret_cast<std::uint8_t*>(0x82e7d4u)=static_cast<std::uint8_t>((i%6u==0u)?6u:(i%6u==1u)?15u:(i%6u==2u)?17u:3u);
                static constexpr float blends[]={0.0f,0.125f,0.25f,0.5f,0.75f,0.875f,1.0f};
                *reinterpret_cast<float*>(0x634b34u)=blends[(i/5u)%7u];
                prepare(0x4493e0u);guest_call.st0=1;run();const float blend=ffrom(guest_call.out_st0);
                const auto scene=*reinterpret_cast<std::uint8_t*>(0x82e7d4u);
                std::memcpy(reinterpret_cast<void*>(WreckerArena),event.data(),event.size());
                std::memcpy(reinterpret_cast<void*>(0x82e7f0),work.data(),work.size());
                std::memcpy(reinterpret_cast<void*>(WreckerArena+0x3000),params.data(),params.size());
                std::memcpy(reinterpret_cast<void*>(WreckerArena+0x5000),wheels.data(),wheels.size());

                prepare(start_wrapper?0x4a6ea0u:0x4a2270u);Bytes st(reinterpret_cast<void*>(S),32);
                st.put32(0,WreckerArena);st.put32(4,state);st.put32(8,reverse);st.put32(12,1u);st.put32(16,fbits(rate));run();
                if(expected_phase==0){prepare(0x4a2400u);Bytes up(reinterpret_cast<void*>(S),16);up.put32(0,WreckerArena);run();}
                // Mirror the original mutable service globals into the original
                // WorldArena image only for final comparison. native_f was copied
                // before original execution, so this is not a native resync.
                bind.capture_globals();

                auto tables=native_f.tables();auto stack=native_f.matrix();auto prediction=native_f.prediction();CourseWorldQuery query{tables,stack,prediction};
                std::array<PcCourseEndView,4> ends={{{std::nullopt,tables.courses[0].runs.lengths},{std::nullopt,tables.courses[1].runs.lengths},{std::nullopt,tables.courses[2].runs.lengths},{std::nullopt,tables.courses[3].runs.lengths}}};
                for(unsigned t=0;t<4;++t)if(tables.courses[t].runs.present)ends[t].header=tables.courses[t].runs.header;
                std::array<std::uint8_t,1> empty{};PcStageViews stages{Bytes(empty.data(),0),0,{},Bytes(empty.data(),0)};
                std::array<std::uint8_t,8> cache{};std::array<std::uint8_t,56> choices{};std::array<std::uint8_t,0x34> selection{};std::array<std::uint8_t,0x400> save{};
                PcRouteContext route{Bytes(choices.data(),choices.size()),Bytes(selection.data(),selection.size()),Bytes(save.data(),save.size()),0,0,0,{0,0,0}};
                PcCourseAdvanceContext advance{ends,stages,Bytes(cache.data(),cache.size()),route,0,0,false};
                auto nb=native_f.bytes();PcRoadInfoContext road{tables,stack,{nb.f32(0x4638),nb.f32(0x463c)}};PcDispMatrixContext display{stack,blend,scene};
                PcPlWreckerContext pl{advance,road,query,display};
                PcCrashWreckerContext wrecker{Bytes(native_work.data(),native_work.size()),Bytes(native_params.data(),native_params.size()),Bytes(native_wheels.data(),native_wheels.size()),&pl};
                PcCrashTables crash{Bytes(primary.data(),primary.size()),Bytes(recovery.data(),recovery.size()),{}};
                PcImpactFeedback feedback{Bytes(empty.data(),0),Bytes(empty.data(),0),false};
                PcCrashEntryContext entry{crash,stages,Bytes(empty.data(),0),feedback,&wrecker};
                try{
                    if(start_wrapper)pc_start_crash(Bytes(native_event.data(),native_event.size()),state,reverse,true,entry);
                    else pc_enter_crash(Bytes(native_event.data(),native_event.size()),state,reverse,true,rate,entry);
                    if(expected_phase==0)pc_advance_crash_state(Bytes(native_event.data(),native_event.size()),crash);
                }catch(const std::exception& ex){throw std::runtime_error(std::string("crash entry+wrecker case=")+std::to_string(i)+": "+ex.what());}
                native_f.save(stack,prediction);

                const auto name=std::string("crash_entry_wrecker_chain_")+(start_wrapper?"start_":"enter_")+(expected_phase<=1?"immediate":"delayed")+(expected_phase==0?"_update":"");
                compare_world_bytes(name+"_event",reinterpret_cast<void*>(WreckerArena),native_event.data(),native_event.size());
                compare_world_bytes(name+"_work",reinterpret_cast<void*>(0x82e7f0),native_work.data(),native_work.size());
                compare_world_bytes(name+"_params",reinterpret_cast<void*>(WreckerArena+0x3000),native_params.data(),native_params.size());
                compare_world_bytes(name+"_wheels",reinterpret_cast<void*>(WreckerArena+0x5000),native_wheels.data(),native_wheels.size());
                bind.compare(name,native_f);
            }catch(...){restore();throw;}
            restore();continue;
        }
        if(id==23u){
            std::array<std::uint8_t,0x1800> event{},native_event{};
            std::array<std::uint8_t,0x800> work{},native_work{};
            std::array<std::uint8_t,0x1600> params{},native_params{};
            std::array<std::uint8_t,4*0xf4> wheels{},native_wheels{};
            std::mt19937 rng(0x50490024u^(i*1664525u));
            for(auto& v:event)v=std::uint8_t(rng());for(auto& v:work)v=std::uint8_t(rng());
            for(auto& v:params)v=std::uint8_t(rng());for(auto& v:wheels)v=std::uint8_t(rng());
            Bytes ee(event.data(),event.size()),ww(work.data(),work.size()),pp(params.data(),params.size()),wh(wheels.data(),wheels.size());
            // Valid primary OnRoadPlace over the dedicated one-polygon boundary fixture.
            ee.put32(0x5c,0);ee.puti(0x60,0);ee.put16(0x64,0);ee.put16(0x66,0);ee.put32(0x68,0);ee.puti(0x1c0,0);
            const std::uint32_t states[]={0u,2u,5u,7u};ee.put32(0x2f0,(ee.u32(0x2f0)&~0x7cu)|(states[i%4u]<<2u));
            ee.putf(0x1c4,0.05f+float(i%37u)*0.03125f);ee.putf(0xdb4,float((i/37u)%9u)*0.125f);
            auto put_angle=[&](std::size_t o,std::int32_t v){ee.put16(o,static_cast<std::uint16_t>(static_cast<std::int16_t>(v)));};
            put_angle(0x2c,int(i*31u)%1025-512);put_angle(0x30,int(i*47u)%1025-512);
            put_angle(0xc2c,int(i*13u)%513-256);put_angle(0xc2e,int(i*17u)%513-256);
            // CalcDispMatrix auxiliary branches remain live after the reset assigns main/alternate pose.
            const std::uint32_t display_flags[]={0u,0x00800000u,0x80000000u,0x80800000u};
            ee.put32(4,(ee.u32(4)&~0x80800000u)|display_flags[(i/4u)%4u]);
            auto vec=[&](std::size_t o,float k){ee.putf(o,k);ee.putf(o+4,-k*0.5f);ee.putf(o+8,k*0.25f);};
            vec(0x2d8,float(int(i%9u)-4)*0.03125f);vec(0x1040,float(int((i/9u)%9u)-4)*0.025f);
            vec(0x1034,float(int((i/81u)%9u)-4)*0.02f);vec(0x2e4,float(int((i/729u)%9u)-4)*0.018f);
            const float offsets[]={0.0f,1.1920928955078125e-7f,0.125f,-0.25f};ee.putf(0xd24,offsets[(i/16u)%4u]);
            ee.put32(0x2b4,WreckerArena+0x3000);
            ww.put32(0x248,WreckerArena+0x5000);ww.put32(0x24c,WreckerArena+0x5000+0xf4);
            ww.put32(0x250,WreckerArena+0x5000+2*0xf4);ww.put32(0x254,WreckerArena+0x5000+3*0xf4);
            pp.putf(0x260,float(int(i%11u)-5)*0.015625f);pp.putf(0xb48,0.35f+float(i%5u)*0.03125f);pp.putf(0xb94,0.45f+float((i/5u)%5u)*0.03125f);
            pp.put32(0x15f8,0xa5000000u^(i*2654435761u));ww.putf(0x224,float(int((i/25u)%9u)-4)*0.0078125f);
            for(unsigned k=0;k<4;++k)wh.putf(std::size_t(k)*0xf4u+0x08,0.75f+0.125f*k+float(i%7u)*0.0078125f);
            native_event=event;native_work=work;native_params=params;native_wheels=wheels;
            const auto saved_mode=*reinterpret_cast<std::uint32_t*>(0x82e7d8u),saved_blend=*reinterpret_cast<std::uint32_t*>(0x634b34u);
            const auto saved_scene=*reinterpret_cast<std::uint8_t*>(0x82e7d4u);
            *reinterpret_cast<std::uint32_t*>(0x82e7d8u)=(i%5u==0u)?1u:0u;
            *reinterpret_cast<std::uint8_t*>(0x82e7d4u)=static_cast<std::uint8_t>((i%6u==0u)?6u:(i%6u==1u)?15u:(i%6u==2u)?17u:3u);
            static constexpr float blends[]={0.0f,0.125f,0.25f,0.5f,0.75f,0.875f,1.0f};*reinterpret_cast<float*>(0x634b34u)=blends[(i/5u)%7u];
            prepare(0x4493e0u);guest_call.st0=1;run();const float blend=ffrom(guest_call.out_st0);const auto scene=*reinterpret_cast<std::uint8_t*>(0x82e7d4u);
            std::memcpy(reinterpret_cast<void*>(WreckerArena),event.data(),event.size());std::memcpy(reinterpret_cast<void*>(WreckerArena+0x2000),work.data(),work.size());
            std::memcpy(reinterpret_cast<void*>(WreckerArena+0x3000),params.data(),params.size());std::memcpy(reinterpret_cast<void*>(WreckerArena+0x5000),wheels.data(),wheels.size());
            const auto phase=static_cast<std::int32_t>(i&1u);
            prepare(0x504900u);Bytes st(reinterpret_cast<void*>(S),32);st.put32(0,WreckerArena);st.put32(4,WreckerArena+0x2000);st.puti(8,phase);run();bind.capture_globals();
            auto stack=f.matrix();auto prediction=f.prediction();auto tables=f.tables();CourseWorldQuery q{tables,stack,prediction};
            PcRoadInfoContext road{tables,stack,{f.bytes().f32(0x4638),f.bytes().f32(0x463c)}};PcDispMatrixContext disp{stack,blend,scene};
            try{pc_pl_wrecker_immediate(Bytes(native_event.data(),native_event.size()),Bytes(native_work.data(),native_work.size()),Bytes(native_params.data(),native_params.size()),Bytes(native_wheels.data(),native_wheels.size()),phase,road,q,disp);}
            catch(const std::exception& ex){throw std::runtime_error(std::string("PlWrecker immediate case=")+std::to_string(i)+": "+ex.what());}
            f.save(stack,prediction);
            compare_world_bytes("pl_wrecker_immediate_event",reinterpret_cast<void*>(WreckerArena),native_event.data(),native_event.size());
            compare_world_bytes("pl_wrecker_immediate_work",reinterpret_cast<void*>(WreckerArena+0x2000),native_work.data(),native_work.size());
            compare_world_bytes("pl_wrecker_immediate_params",reinterpret_cast<void*>(WreckerArena+0x3000),native_params.data(),native_params.size());
            compare_world_bytes("pl_wrecker_immediate_wheels",reinterpret_cast<void*>(WreckerArena+0x5000),native_wheels.data(),native_wheels.size());bind.compare("pl_wrecker_immediate",f);
            *reinterpret_cast<std::uint32_t*>(0x82e7d8u)=saved_mode;*reinterpret_cast<std::uint8_t*>(0x82e7d4u)=saved_scene;*reinterpret_cast<std::uint32_t*>(0x634b34u)=saved_blend;
            continue;
        }
        if(id==22u){
            std::array<std::uint8_t,0x1200> event{},native_event{};
            std::mt19937 erng(0x4a265024u^(i*1664525u));for(auto& v:event)v=std::uint8_t(erng());
            Bytes ee(event.data(),event.size());
            auto probe=[&](std::size_t o,float x,float y,float z){ee.putf(o,x);ee.putf(o+4,y);ee.putf(o+8,z);};
            auto rf=[&](int span){return float(int(erng()%unsigned(span*2+1))-span)/32.0f;};
            probe(0x14,rf(64),rf(32),rf(64));probe(0x16c,rf(64),rf(32),rf(64));
            probe(0x2d8,rf(16),rf(16),rf(16));probe(0x1040,rf(16),rf(16),rf(16));
            probe(0x1034,rf(64),rf(64),rf(64));probe(0x2e4,rf(64),rf(64),rf(64));
            const std::uint32_t flag_modes[]={0u,0x00800000u,0x80000000u,0x80800000u};
            ee.put32(4,(ee.u32(4)&~0x80800000u)|flag_modes[i%4u]);
            const float offsets[]={0.0f,1.1920928955078125e-7f,0.25f,-0.5f};ee.putf(0xd24,offsets[(i/4u)%4u]);
            auto put_angle=[&](std::size_t o){ee.put16(o,static_cast<std::uint16_t>(erng()));};
            for(auto o:{0x2cu,0x2eu,0x30u,0x17cu,0x17eu,0x180u,0xc2cu,0xc2eu})put_angle(o);
            native_event=event;
            const auto saved_mode=*reinterpret_cast<std::uint32_t*>(0x82e7d8u);
            const auto saved_scene=*reinterpret_cast<std::uint8_t*>(0x82e7d4u);
            const auto saved_blend=*reinterpret_cast<std::uint32_t*>(0x634b34u);
            *reinterpret_cast<std::uint32_t*>(0x82e7d8u)=(i%5u==0u)?1u:0u;
            *reinterpret_cast<std::uint8_t*>(0x82e7d4u)=static_cast<std::uint8_t>((i%6u==0u)?6u:(i%6u==1u)?15u:(i%6u==2u)?17u:3u);
            static constexpr float blends[]={0.0f,0.125f,0.25f,0.5f,0.75f,0.875f,1.0f};
            *reinterpret_cast<float*>(0x634b34u)=blends[(i/5u)%7u];
            prepare(0x4493e0u);guest_call.st0=1;run();const float blend=ffrom(guest_call.out_st0);
            const auto scene=*reinterpret_cast<std::uint8_t*>(0x82e7d4u);
            std::memcpy(reinterpret_cast<void*>(WreckerArena),event.data(),event.size());
            prepare(0x4a2650u);Bytes st(reinterpret_cast<void*>(S),16);st.put32(0,WreckerArena);run();bind.capture_globals();
            auto stack=f.matrix();PcDispMatrixContext dc{stack,blend,scene};pc_calc_disp_matrix(Bytes(native_event.data(),native_event.size()),dc);f.save(stack,f.prediction());
            compare_world_bytes("calc_disp_event",reinterpret_cast<void*>(WreckerArena),native_event.data(),native_event.size());bind.compare("calc_disp",f);
            *reinterpret_cast<std::uint32_t*>(0x82e7d8u)=saved_mode;*reinterpret_cast<std::uint8_t*>(0x82e7d4u)=saved_scene;*reinterpret_cast<std::uint32_t*>(0x634b34u)=saved_blend;
            continue;
        }
        if(id==21u){
            std::array<std::uint8_t,0x1000> event{},native_event{};
            std::array<std::uint8_t,0x800> work{},native_work{};
            std::array<std::uint8_t,0xc00> params{},native_params{};
            std::array<std::uint8_t,4*0xf4> wheels{},native_wheels{};
            std::mt19937 wrng(0x503dd024u^(i*1664525u));
            for(auto& v:event)v=std::uint8_t(wrng());
            for(auto& v:work)v=std::uint8_t(wrng());
            for(auto& v:params)v=std::uint8_t(wrng());
            for(auto& v:wheels)v=std::uint8_t(wrng());
            Bytes ee(event.data(),event.size()),ww(work.data(),work.size()),pp(params.data(),params.size()),wh(wheels.data(),wheels.size());
            outrun::testing::world_put_probe(ee,0x14,{5.0f,9.0f,5.0f});
            auto put_angle=[&](std::size_t o,std::int32_t v){ee.put16(o,static_cast<std::uint16_t>(static_cast<std::int16_t>(v)));};
            put_angle(0x2c,int(i*37u)%129-64);put_angle(0x2e,int(i*53u)%129-64);
            put_angle(0x30,int(i*71u)%129-64);put_angle(0x160,int(i*97u)%16385-8192);
            ee.put32(0x2b4,WreckerArena+0x2000);ww.put32(0x248,WreckerArena+0x3000);
            pp.putf(0x260,float(int(i%17u)-8)*0.03125f);
            pp.putf(0xb48,0.35f+float(i%7u)*0.015625f);pp.putf(0xb94,0.45f+float((i/7u)%7u)*0.015625f);
            ww.putf(0x224,float(int((i/49u)%9u)-4)*0.0078125f);
            for(unsigned k=0;k<4;++k)wh.putf(std::size_t(k)*0xf4u+0x08,0.75f+0.125f*k+float(i%5u)*0.015625f);
            native_event=event;native_work=work;native_params=params;native_wheels=wheels;
            std::memcpy(reinterpret_cast<void*>(WreckerArena),event.data(),event.size());
            std::memcpy(reinterpret_cast<void*>(WreckerArena+0x1000),work.data(),work.size());
            std::memcpy(reinterpret_cast<void*>(WreckerArena+0x2000),params.data(),params.size());
            std::memcpy(reinterpret_cast<void*>(WreckerArena+0x3000),wheels.data(),wheels.size());
            prepare(0x503dd0);Bytes st(reinterpret_cast<void*>(S),16);st.put32(0,WreckerArena);st.put32(4,WreckerArena+0x1000);run();bind.capture_globals();
            auto stack=f.matrix();auto prediction=f.prediction();auto tables=f.tables();CourseWorldQuery q{tables,stack,prediction};
            try{pc_reconstruct_posture_matrix_and_face_work(Bytes(native_event.data(),native_event.size()),Bytes(native_work.data(),native_work.size()),
                    Bytes(native_params.data(),native_params.size()),Bytes(native_wheels.data(),native_wheels.size()),q);}
            catch(const std::exception& ex){throw std::runtime_error(std::string("reconstruct_posture case=")+std::to_string(i)+": "+ex.what());}
            f.save(stack,prediction);
            compare_world_bytes("reconstruct_posture_event",reinterpret_cast<void*>(WreckerArena),native_event.data(),native_event.size());
            compare_world_bytes("reconstruct_posture_work",reinterpret_cast<void*>(WreckerArena+0x1000),native_work.data(),native_work.size());
            compare_world_bytes("reconstruct_posture_params",reinterpret_cast<void*>(WreckerArena+0x2000),native_params.data(),native_params.size());
            compare_world_bytes("reconstruct_posture_wheels",reinterpret_cast<void*>(WreckerArena+0x3000),native_wheels.data(),native_wheels.size());
            bind.compare("reconstruct_posture",f);continue;
        }
        if(id>=15u){
            const auto original=run_world_original(id);bind.capture_globals();
            std::uint32_t native=0;
            try{native=outrun::testing::run_course_world_native(f,id);}
            catch(const std::exception& ex){throw std::runtime_error(std::string(world_names[id])+" case="+std::to_string(i)+": "+ex.what());}
            compare_u32(std::string(world_names[id])+"_return",original,native);bind.compare(world_names[id],f);continue;
        }
        if(id==14u){
            auto fb=f.bytes();
            std::array<std::uint8_t,0xe00> event{},native_event{};
            std::array<std::uint8_t,0x800> work{},native_work{};
            std::mt19937 wrng(0x504c5752u^(i*1103515245u));
            for(auto& v:event)v=std::uint8_t(wrng());for(auto& v:work)v=std::uint8_t(wrng());
            Bytes ee(event.data(),event.size());
            auto tables=f.tables();const auto type=std::uint32_t((i/5u)%4u);
            ee.put32(0x5c,type);ee.puti(0x60,std::int32_t((i%9u)*7u));
            // PlWrecker advances by ten. Keep the integrated road query on
            // valid synthetic rows 0/1/2; crossing itself is exercised by the
            // dedicated advance_on_road_place corpus above.
            const std::int32_t starts[]={-10,-9,-8};
            ee.put16(0x64,std::uint16_t(starts[(i/7u)%3u]));ee.put8(0x66,std::uint8_t(i%12u));
            const auto old_key=0x5100u+std::uint32_t(i%13u),current_key=0x6100u+std::uint32_t(i%17u);ee.put32(0x68,old_key);
            ee.puti(0x1c0,fb.i32(0x4670));
            ee.putf(0x14,float(int(i%19u)-9)*2.0f);ee.putf(0x18,float(int((i/19u)%11u)-5));ee.putf(0x1c,float(int((i/209u)%17u)-8)*3.0f);
            const auto phase=2+std::int32_t(i%6u);
            native_event=event;native_work=work;

            const auto current_property=std::uint32_t((i*3u+1u)%14u),old_property=std::uint32_t((i*5u+3u)%14u);
            std::array<std::uint8_t,56> choices{},native_choices{};
            for(unsigned k=0;k<14;++k){const auto v=(k==old_property)?std::uint32_t((i/3u)&1u):2u;std::memcpy(choices.data()+k*4,&v,4);}native_choices=choices;
            std::array<std::uint8_t,0x34> selection{};std::array<std::uint8_t,0x400> save{};
            std::array<std::uint8_t,8> cache{},native_cache{};Bytes cache_b(cache.data(),cache.size());cache_b.put32(0,0xffffffffu);cache_b.put32(4,0x5a5a5a5au);native_cache=cache;
            std::array<std::uint8_t,2*0x78> stage_records{};Bytes sr(stage_records.data(),stage_records.size());
            sr.put32(0x04,current_key);sr.put32(0x08,current_property);sr.put32(0x78+0x04,old_key);sr.put32(0x78+0x08,old_property);
            const auto stage_limit=std::uint8_t((i/11u)%16u);

            std::array<std::uint8_t,4096> g635{},g7d3{},g836{},g830{};
            std::memcpy(g635.data(),reinterpret_cast<void*>(0x635000),4096);std::memcpy(g7d3.data(),reinterpret_cast<void*>(0x7d3000),4096);
            std::memcpy(g836.data(),reinterpret_cast<void*>(0x836000),4096);std::memcpy(g830.data(),reinterpret_cast<void*>(0x830000),4096);
            auto restore=[&]{std::memcpy(reinterpret_cast<void*>(0x635000),g635.data(),4096);std::memcpy(reinterpret_cast<void*>(0x7d3000),g7d3.data(),4096);std::memcpy(reinterpret_cast<void*>(0x836000),g836.data(),4096);std::memcpy(reinterpret_cast<void*>(0x830000),g830.data(),4096);};
            try{
                std::memcpy(reinterpret_cast<void*>(WreckerArena),event.data(),event.size());std::memcpy(reinterpret_cast<void*>(WreckerArena+0x2000),work.data(),work.size());
                std::memcpy(reinterpret_cast<void*>(0x7d39a0),choices.data(),choices.size());std::memcpy(reinterpret_cast<void*>(0x635f2c),cache.data(),cache.size());
                std::memcpy(reinterpret_cast<void*>(WreckerArena+0x3000),stage_records.data(),stage_records.size());
                *reinterpret_cast<std::uint32_t*>(0x7d33bc)=WreckerArena+0x3000;*reinterpret_cast<std::uint32_t*>(0x7d33c4)=2;
                *reinterpret_cast<std::uint32_t*>(0x7d30ac)=current_key;*reinterpret_cast<std::uint32_t*>(0x7d31dc)=0;*reinterpret_cast<std::uint32_t*>(0x7d2e80)=0;*reinterpret_cast<std::uint32_t*>(0x7d33ac)=stage_limit;
                *reinterpret_cast<std::uint8_t*>(0x836374)=0;*reinterpret_cast<std::uint8_t*>(0x830394)=0;*reinterpret_cast<std::uint8_t*>(0x8361b4)=0;
                prepare(0x504900);Bytes st(reinterpret_cast<void*>(S),32);st.put32(0,WreckerArena);st.put32(4,WreckerArena+0x2000);st.puti(8,phase);run();bind.capture_globals();

                std::array<PcCourseEndView,4> ends={{{std::nullopt,tables.courses[0].runs.lengths},{std::nullopt,tables.courses[1].runs.lengths},{std::nullopt,tables.courses[2].runs.lengths},{std::nullopt,tables.courses[3].runs.lengths}}};
                for(unsigned t=0;t<4;++t){auto& q=tables.courses[t];if(q.runs.present)ends[t].header=q.runs.header;}
                std::array<std::uint8_t,1> no_descriptor{};PcStageViews stages{Bytes(stage_records.data(),stage_records.size()),2,{},Bytes(no_descriptor.data(),0)};
                PcRouteContext route{Bytes(native_choices.data(),native_choices.size()),Bytes(selection.data(),selection.size()),Bytes(save.data(),save.size()),0,0,0,{0,0,0}};
                PcCourseAdvanceContext advance{ends,stages,Bytes(native_cache.data(),native_cache.size()),route,current_key,stage_limit,false};
                auto stack=f.matrix();PcRoadInfoContext road{tables,stack,{fb.f32(0x4638),fb.f32(0x463c)}};
                try{pc_pl_wrecker_delayed(Bytes(native_event.data(),native_event.size()),phase,advance,road);}
                catch(const std::exception& ex){
                    auto ne=Bytes(native_event.data(),native_event.size());const auto nt=ne.u32(0xd7c);
                    const auto& tm=tables.transforms[nt==0u?0u:1u];auto cm=stack.current();
                    std::cerr<<"case="<<i<<" initial="<<type<<" advanced="<<nt<<" stack_off="<<stack.current_offset<<" depth="<<stack.depth
                             <<" transform_w="<<tm.f32(0x0c)<<","<<tm.f32(0x1c)<<","<<tm.f32(0x2c)<<","<<tm.f32(0x3c)
                             <<" current_w="<<cm.f32(0x0c)<<","<<cm.f32(0x1c)<<","<<cm.f32(0x2c)<<","<<cm.f32(0x3c)<<"\n";
                    throw std::runtime_error(std::string("pl_wrecker_delayed case=")+std::to_string(i)+" type="+std::to_string(type)+" phase="+std::to_string(phase)+": "+ex.what());}
                f.save(stack,f.prediction());
                compare_world_bytes("pl_wrecker_delayed_event",reinterpret_cast<void*>(WreckerArena),native_event.data(),native_event.size());
                compare_world_bytes("pl_wrecker_delayed_work",reinterpret_cast<void*>(WreckerArena+0x2000),native_work.data(),native_work.size());
                compare_world_bytes("pl_wrecker_delayed_cache",reinterpret_cast<void*>(0x635f2c),native_cache.data(),native_cache.size());
                compare_world_bytes("pl_wrecker_delayed_choices",reinterpret_cast<void*>(0x7d39a0),native_choices.data(),native_choices.size());
                bind.compare("pl_wrecker_delayed",f);
            }catch(...){restore();throw;}restore();continue;
        }
        if(id==13u){
            auto fb=f.bytes();
            std::array<std::uint8_t,16> place{},native_place{};
            for(unsigned k=0;k<16;++k)place[k]=fb.u8(0x4560+k);
            Bytes pp(place.data(),place.size());
            const auto type=std::uint32_t((i/5u)%4u);pp.put32(0,type);
            // Keep early-return, clamp, exact-end and cross-end paths all live.
            auto tables=f.tables();const auto& ct=tables.courses[type];
            const auto count=ct.runs.header.u32(0x0c);
            const auto end=count?std::uint16_t(ct.runs.lengths.i16(std::size_t(count-1u)*2u)):0u;
            static constexpr std::int32_t steps[]={-13,-1,0,1,2,10,37};
            const auto step=steps[i%7u];
            std::int32_t start=0;
            switch((i/7u)%6u){case 0:start=-2;break;case 1:start=0;break;case 2:start=int(end)-1;break;case 3:start=int(end);break;case 4:start=int(end)+1;break;default:start=int(end)-int(step)+1;break;}
            pp.put16(8,std::uint16_t(start));pp.put8(0x0a,std::uint8_t(i%12u));
            const auto old_key=0x3000u+std::uint32_t(i%13u);const auto current_key=0x4000u+std::uint32_t(i%17u);pp.put32(0x0c,old_key);
            const auto current_property=std::uint32_t(i%14u);
            const auto old_property=std::uint32_t((i*5u+3u)%14u);
            native_place=place;

            std::array<std::uint8_t,56> choices{},native_choices{};
            for(unsigned k=0;k<14;++k){const auto v=(k==old_property)?std::uint32_t((i/3u)&1u):2u;std::memcpy(choices.data()+k*4,&v,4);}
            native_choices=choices;
            std::array<std::uint8_t,0x34> selection{};std::array<std::uint8_t,0x400> save{};
            // Force misses so both the original and native implementations use
            // the explicit PC-layout record table rather than a primed cache.
            std::array<std::uint8_t,8> cache{},native_cache{};Bytes cache_b(cache.data(),cache.size());cache_b.put32(0,0xffffffffu);cache_b.put32(4,0xa5a5a5a5u);native_cache=cache;
            std::array<std::uint8_t,2*0x78> stage_records{};Bytes sr(stage_records.data(),stage_records.size());
            sr.put32(0x04,current_key);sr.put32(0x08,current_property);
            sr.put32(0x78+0x04,old_key);sr.put32(0x78+0x08,old_property);
            const auto stage_limit=std::uint8_t((i/11u)%16u);

            // Save/restore the extra PC globals touched by 44BDD0/44C940/451350.
            std::array<std::uint8_t,4096> g635{},g7d3{},g836{},g830{};
            std::memcpy(g635.data(),reinterpret_cast<void*>(0x635000),4096);std::memcpy(g7d3.data(),reinterpret_cast<void*>(0x7d3000),4096);
            std::memcpy(g836.data(),reinterpret_cast<void*>(0x836000),4096);std::memcpy(g830.data(),reinterpret_cast<void*>(0x830000),4096);
            auto restore=[&]{std::memcpy(reinterpret_cast<void*>(0x635000),g635.data(),4096);std::memcpy(reinterpret_cast<void*>(0x7d3000),g7d3.data(),4096);std::memcpy(reinterpret_cast<void*>(0x836000),g836.data(),4096);std::memcpy(reinterpret_cast<void*>(0x830000),g830.data(),4096);};
            try{
                std::memcpy(reinterpret_cast<void*>(WreckerArena+0x400),place.data(),place.size());
                std::memcpy(reinterpret_cast<void*>(0x7d39a0),choices.data(),choices.size());
                std::memcpy(reinterpret_cast<void*>(0x635f2c),cache.data(),cache.size());
                std::memcpy(reinterpret_cast<void*>(WreckerArena+0x800),stage_records.data(),stage_records.size());
                *reinterpret_cast<std::uint32_t*>(0x7d33bc)=WreckerArena+0x800;*reinterpret_cast<std::uint32_t*>(0x7d33c4)=2;
                *reinterpret_cast<std::uint32_t*>(0x7d30ac)=current_key;*reinterpret_cast<std::uint32_t*>(0x7d31dc)=0;
                *reinterpret_cast<std::uint32_t*>(0x7d2e80)=0;*reinterpret_cast<std::uint32_t*>(0x7d33ac)=stage_limit;
                *reinterpret_cast<std::uint8_t*>(0x836374)=0;*reinterpret_cast<std::uint8_t*>(0x830394)=0;*reinterpret_cast<std::uint8_t*>(0x8361b4)=0;
                prepare(0x46fe70);Bytes st(reinterpret_cast<void*>(S),16);st.put32(0,WreckerArena+0x400);st.puti(4,step);run();
                const auto original=guest_call.out_eax;

                std::array<PcCourseEndView,4> ends={{
                    {std::nullopt,tables.courses[0].runs.lengths},{std::nullopt,tables.courses[1].runs.lengths},
                    {std::nullopt,tables.courses[2].runs.lengths},{std::nullopt,tables.courses[3].runs.lengths}}};
                for(unsigned t=0;t<4;++t){auto& q=tables.courses[t];if(q.runs.present)ends[t].header=q.runs.header;}
                std::array<std::uint8_t,1> no_descriptor{};PcStageViews stages{Bytes(stage_records.data(),stage_records.size()),2,{},Bytes(no_descriptor.data(),0)};
                PcRouteContext route{Bytes(native_choices.data(),native_choices.size()),Bytes(selection.data(),selection.size()),Bytes(save.data(),save.size()),0,0,0,{0,0,0}};
                PcCourseAdvanceContext ctx{ends,stages,Bytes(native_cache.data(),native_cache.size()),route,current_key,stage_limit,false};
                const auto native=pc_advance_on_road_place(Bytes(native_place.data(),native_place.size()),step,ctx)?1u:0u;
                compare_u32("advance_on_road_place_return",original,native);
                compare_world_bytes("advance_on_road_place_memory",reinterpret_cast<void*>(WreckerArena+0x400),native_place.data(),native_place.size());
                compare_world_bytes("advance_on_road_place_cache",reinterpret_cast<void*>(0x635f2c),native_cache.data(),native_cache.size());
                compare_world_bytes("advance_on_road_place_choices",reinterpret_cast<void*>(0x7d39a0),native_choices.data(),native_choices.size());
            }catch(...){restore();throw;}
            restore();continue;
        }
        if(id==12u){
            auto fb=f.bytes();std::array<std::uint8_t,0x400> event{},native_event{};
            std::array<std::uint8_t,16> place{},native_place{};std::array<std::uint8_t,0x64> output{},native_output{};
            std::mt19937 wrng(0x57524543u^(i*1103515245u));
            for(auto& v:event)v=std::uint8_t(wrng());for(auto& v:output)v=0xcdu;
            for(unsigned k=0;k<16;++k)place[k]=fb.u8(0x4560+k);
            Bytes ee(event.data(),event.size()),pp(place.data(),place.size());
            const auto type=pp.u32(0);ee.put32(0x5c,type);ee.puti(0x1c0,fb.i32(0x4670));
            // Position around the selected road center so both 100/101 branches
            // get chosen across the deterministic corpus.
            ee.putf(0x14,float(int(i%19u)-9)*2.0f);ee.putf(0x18,float(int((i/19u)%11u)-5));ee.putf(0x1c,float(int((i/209u)%17u)-8)*3.0f);
            native_event=event;native_place=place;native_output=output;
            std::memcpy(reinterpret_cast<void*>(WreckerArena),event.data(),event.size());
            std::memcpy(reinterpret_cast<void*>(WreckerArena+0x400),place.data(),place.size());
            std::memcpy(reinterpret_cast<void*>(WreckerArena+0x500),output.data(),output.size());
            prepare(0x503780);guest_call.eax=WreckerArena;guest_call.ebx=WreckerArena+0x400;Bytes st(reinterpret_cast<void*>(S),64);st.put32(0,WreckerArena+0x500);run();bind.capture_globals();
            auto stack=f.matrix();auto tables=f.tables();PcRoadInfoContext rc{tables,stack,{fb.f32(0x4638),fb.f32(0x463c)}};
            try{pc_pl_wrecker_sub(Bytes(native_event.data(),native_event.size()),Bytes(native_place.data(),native_place.size()),Bytes(native_output.data(),native_output.size()),rc);}
            catch(const std::exception& ex){throw std::runtime_error(std::string("pl_wrecker_sub case=")+std::to_string(i)+" type="+std::to_string(type)+": "+ex.what());}
            f.save(stack,f.prediction());
            compare_world_bytes("pl_wrecker_sub_event",reinterpret_cast<void*>(WreckerArena),native_event.data(),native_event.size());
            compare_world_bytes("pl_wrecker_sub_place",reinterpret_cast<void*>(WreckerArena+0x400),native_place.data(),native_place.size());
            compare_world_bytes("pl_wrecker_sub_defined",reinterpret_cast<void*>(WreckerArena+0x500),native_output.data(),0x58);
            bool tail=true;for(unsigned k=0x58;k<0x64;++k)tail=tail&&native_output[k]==0xcdu;compare_u32("pl_wrecker_sub_native_padding_untouched",tail?1u:0u,1u);
            bind.compare("pl_wrecker_sub",f);continue;
        }
        const unsigned steps=id==7u?8u:1u;
        for(unsigned stage=0;stage<steps;++stage){
            const unsigned actual_id=id==7u?(stage&1u?6u:5u):id;
            const auto before=f.image;const auto original=run_world_original(actual_id);bind.capture_globals();
            if(world_golden.is_open()&&i<256u&&id<7u){
                world_u32(id);world_u32(i);world_u32(original);
                world_golden.write(reinterpret_cast<const char*>(before.data()),before.size());
                world_golden.write(reinterpret_cast<const char*>(WorldArena),before.size());
                if(!world_golden)throw std::runtime_error("world snapshot write failed");
                ++world_golden_count;
            }
            std::uint32_t native=0;
            try{native=outrun::testing::run_course_world_native(f,actual_id);}
            catch(const std::exception& ex){throw std::runtime_error(std::string(world_names[id])+" case="+std::to_string(i)+" stage="+std::to_string(stage)+": "+ex.what());}
            const auto name=std::string(world_names[id])+(id==7u?"_"+std::to_string(stage+1u):"");
            compare_u32(name+"_return",original,native);bind.compare(name,f);
        }
    }
}

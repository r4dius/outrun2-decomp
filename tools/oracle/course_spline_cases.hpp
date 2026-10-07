// Included inside compare_pc.cpp's private namespace after common comparators.
// Guest/native input images are independently owned. Full 4-KiB comparison
// checks outputs, immutable inputs, and canaries, not only the returned height.
constexpr const char* spline_routines[]={"course_vec3_distance","course_vec3_length_squared",
    "calc_normal_to_delta","calc_hermite_direction_vector2","calc_hermite_tangent",
    "calc_hermite_tangent2","calc_hermite_coefficients","calc_carry_variable",
    "solve_hermite","calc_hermite2","calc_y_pos_spl","course_spline_chain","find_primary_course_run"};
bool spline_enabled(const std::string& only){
    if(only=="all"||only=="course_spline_all")return true;
    for(auto s:spline_routines)if(only==s)return true;
    return false;
}
void init_spline_oracle(){
    if(mprotect(reinterpret_cast<void*>(0x780000u),4096,PROT_READ|PROT_WRITE))
        throw std::runtime_error("topology globals mprotect failed");
    if(mprotect(reinterpret_cast<void*>(0x67d000u),4096,PROT_READ|PROT_WRITE))
        throw std::runtime_error("spline tuning page mprotect failed");
}
void compare_spline_image(const std::string& name,const std::array<std::uint8_t,4096>& expected){
    auto& s=stats[name];++s.cases;s.bytes+=expected.size();
    auto* actual=reinterpret_cast<const std::uint8_t*>(E);
    for(std::size_t k=0;k<expected.size();++k)if(actual[k]!=expected[k]){
        ++s.fail;if(s.fail<=4){float a=0,b=0;const auto base=k&~std::size_t(3);std::memcpy(&a,actual+base,4);std::memcpy(&b,expected.data()+base,4);
            std::cerr<<name<<" case="<<s.cases<<" offset=0x"<<std::hex<<k<<std::dec<<" actual="<<std::setprecision(10)<<a<<" native="<<b<<"\n";}return;
    }
    ++s.exact;
}
struct SplineFixture {
    std::array<std::uint8_t,4096> image{};
    CourseQuad p{},normals{},forward{},back{},left{},right{};
    CourseVec3 point{};CourseTangents tangents{};std::array<float,3> coefficients{};
    CourseSplineTuning tuning{};float u{},v{},value{};unsigned mask{};
    template<class T> void put(std::size_t o,const T& x){static_assert(std::is_trivially_copyable_v<T>);std::memcpy(image.data()+o,&x,sizeof(x));}
    void guest() const{std::memcpy(reinterpret_cast<void*>(E),image.data(),image.size());}
    CourseSplineNeighbors neighbors() const{return {mask&1u?&forward:nullptr,mask&2u?&back:nullptr,mask&4u?&left:nullptr,mask&8u?&right:nullptr};}
    void neighbor_args(Bytes stack,unsigned start) const{
        for(unsigned j=0;j<4;++j)stack.put32(start+j*4,(mask&(1u<<j))?E+0x180u+j*0x40u:0u);
    }
};
// Self-contained synthetic snapshots: no commercial tables/code/assets.
std::ofstream spline_golden;
std::uint32_t spline_golden_count=0;
void spline_write_u32(std::uint32_t v){char b[4];for(unsigned k=0;k<4;++k)b[k]=char((v>>(k*8))&255);spline_golden.write(b,4);}
void begin_spline_golden(const char* path){
    spline_golden.open(path,std::ios::binary|std::ios::trunc);
    if(!spline_golden)throw std::runtime_error("cannot open spline snapshots");
    spline_golden.write("OR2S014\0",8);spline_write_u32(1);spline_write_u32(x87_control);spline_write_u32(0);
}
void capture_spline_golden(unsigned i,const SplineFixture& f){
    if(!spline_golden.is_open()||i>=128u)return;
    const std::uint32_t entries[]={0x40f140,0x40f110,0x494b90,0x494a70,0x495340,0x4951d0,0x494e20,0x494d30,0x494ee0,0x4953f0,0x43cd50};
    unsigned id=0;for(;id<11u;++id)if(entries[id]==guest_call.entry)break;
    if(id==11u)return;
    spline_write_u32(id);spline_write_u32(i);
    spline_write_u32(guest_call.entry==0x494d30?guest_call.out_xmm0:guest_call.out_st0);
    spline_write_u32(fbits(f.tuning.lateral));spline_write_u32(fbits(f.tuning.longitudinal));
    spline_write_u32(fbits(f.u));spline_write_u32(fbits(f.v));spline_write_u32(fbits(f.value));spline_write_u32(f.mask);
    spline_golden.write(reinterpret_cast<const char*>(f.image.data()),0x380);
    // Expected bytes come ONLY from the just-executed original, before native reconstruction.
    spline_golden.write(reinterpret_cast<const char*>(E),0x380);++spline_golden_count;
    if(!spline_golden)throw std::runtime_error("spline snapshot write failed");
}
void finish_spline_golden(){if(spline_golden.is_open()){
    spline_golden.seekp(16);spline_write_u32(spline_golden_count);spline_golden.close();
    if(!spline_golden)throw std::runtime_error("spline snapshots finalization failed");
}}
SplineFixture make_spline_fixture(unsigned i){
    SplineFixture f;std::mt19937 gen(0x4f523014u^(i*1664525u));
    auto sample=[&](float lo,float hi){return lo+(hi-lo)*(float(gen()&0xffffffu)/float(0xffffffu));};
    for(auto& b:f.image)b=std::uint8_t(gen());
    const float x=sample(-400,400),z=sample(-400,400),len=sample(4,40),width=sample(2,20);
    const float skew=(i%4u)?sample(-0.3f,0.3f):0.0f;
    f.p={CourseVec3{x,sample(-10,10),z},CourseVec3{x+skew*width,sample(-10,10),z+width},
         CourseVec3{x+len+skew*width,sample(-10,10),z+width+skew*len},CourseVec3{x+len,sample(-10,10),z+skew*len}};
    // Trapezoidal and oblique quads exercise both linear and quadratic inversion.
    if(i%4u>=2u){f.p[2].x+=sample(-1,1);f.p[2].z+=sample(-0.5f,0.5f);}
    for(unsigned j=0;j<4;++j){
        f.normals[j]={sample(-0.6f,0.6f),sample(0.4f,1.2f),sample(-0.6f,0.6f)};
        f.forward[j]={f.p[j].x+len,f.p[j].y+sample(-2,2),f.p[j].z+skew*len};
        f.back[j]={f.p[j].x-len,f.p[j].y+sample(-2,2),f.p[j].z-skew*len};
        f.left[j]={f.p[j].x-skew*width,f.p[j].y+sample(-2,2),f.p[j].z-width};
        f.right[j]={f.p[j].x+skew*width,f.p[j].y+sample(-2,2),f.p[j].z+width};
    }
    static constexpr float edges[]={0,1,0.5f,-0.001f,1.001f,-0.01f,1.01f,0x1p-23f};
    f.u=i%3u?sample(0,1):edges[(i/3u)%8u];f.v=i%5u?sample(0,1):edges[(i/5u)%8u];
    const float tx=f.v,tz=f.u;
    f.point={x+len*tx+skew*width*tz,sample(-100,100),z+width*tz+skew*len*tx};
    if(i%29u==0)f.point=f.p[(i/29u)%4u];
    // Rotate the complete geometry/normals to cover both dominant axes and signs.
    auto rotate=[&](CourseVec3& a){const auto x0=a.x,z0=a.z;switch((i>>2u)&3u){
        case 1:a.x=z0;a.z=-x0;break;case 2:a.x=-x0;a.z=-z0;break;case 3:a.x=-z0;a.z=x0;break;default:break;}};
    for(auto* q:{&f.p,&f.normals,&f.forward,&f.back,&f.left,&f.right})for(auto& a:*q)rotate(a);
    rotate(f.point);
    for(auto& t:f.tangents)t={sample(-8,8),sample(-8,8),sample(-8,8)};
    f.coefficients={sample(-4,4),sample(-4,4),sample(-4,4)};
    if(i%13u==0)f.coefficients[0]=0.0f;
    if(i%13u==1)f.coefficients={-2.0f,3.0f,0.0f};
    if(i%13u==2)f.coefficients={1.0f,-1.5f,1.5f};
    f.value=i%2u?sample(-0.1f,1.1f):f.u;
    static constexpr float tune[]={1.0f,0.5f,1.25f,2.0f,-0.75f,0.0f};
    f.tuning={tune[(i/16u)%6u],tune[(i/96u)%6u]};f.mask=i&15u;
    f.put(0x100,f.p);f.put(0x140,f.normals);f.put(0x180,f.forward);f.put(0x1c0,f.back);
    f.put(0x200,f.left);f.put(0x240,f.right);f.put(0x280,f.point);f.put(0x2a0,f.coefficients);f.put(0x300,f.tangents);
    return f;
}
void run_course_spline_cases(unsigned i,const std::string& only){
    if(!spline_enabled(only))return;
    auto enabled=[&](const char* n){return only=="all"||only=="course_spline_all"||only==n;};
    auto f=make_spline_fixture(i);
    *reinterpret_cast<float*>(0x67d94cu)=f.tuning.lateral;*reinterpret_cast<float*>(0x67d950u)=f.tuning.longitudinal;
    auto setup=[&](std::uint32_t entry){f.guest();prepare(entry);return Bytes(reinterpret_cast<void*>(S),80);};
    if(enabled("course_vec3_distance")){
        auto st=setup(0x40f140u);st.put32(0,E+0x100);st.put32(4,E+0x118);guest_call.st0=1;run();capture_spline_golden(i,f);
        compare_float("course_vec3_distance",ffrom(guest_call.out_st0),float(course_vec3_distance(f.p[0],f.p[2])));
        compare_spline_image("course_vec3_distance_memory",f.image);
    }
    if(enabled("course_vec3_length_squared")){
        auto st=setup(0x40f110u);st.put32(0,E+0x100);guest_call.st0=1;run();capture_spline_golden(i,f);
        compare_float("course_vec3_length_squared",ffrom(guest_call.out_st0),float(course_vec3_length_squared(f.p[0])));
        compare_spline_image("course_vec3_length_squared_memory",f.image);
    }
    if(enabled("calc_normal_to_delta")){
        auto st=setup(0x494b90);st.put32(0,E+0x100);st.put32(4,E+0x10c);st.put32(8,E+0x140);st.put32(12,E+0x14c);
        st.putf(16,f.tuning.lateral);st.put32(20,E+0x20);st.put32(24,E+0x2c);run();capture_spline_golden(i,f);
        CourseVec3 a{},b{};calc_normal_to_delta(f.p[0],f.p[1],f.normals[0],f.normals[1],f.tuning.lateral,a,b);
        auto exp=f;exp.put(0x20,a);exp.put(0x2c,b);compare_spline_image("calc_normal_to_delta",exp.image);
    }
    if(enabled("calc_hermite_direction_vector2")){
        auto st=setup(0x494a70);st.put32(0,E+0x20);st.put32(4,E+0x50);st.put32(8,E+0x100);f.neighbor_args(st,12);run();capture_spline_golden(i,f);
        CourseQuad u{},v{};calc_hermite_direction_vector2(u,v,f.p,f.neighbors());auto exp=f;exp.put(0x20,u);exp.put(0x50,v);
        compare_spline_image("calc_hermite_direction_vector2",exp.image);
    }
    if(enabled("calc_hermite_tangent")){
        auto st=setup(0x495340);st.put32(0,E+0x20);st.put32(4,E+0x100);st.put32(8,E+0x140);run();capture_spline_golden(i,f);
        CourseTangents t{};calc_hermite_tangent(t,f.p,f.normals,f.tuning);auto exp=f;exp.put(0x20,t);
        compare_spline_image("calc_hermite_tangent",exp.image);
    }
    if(enabled("calc_hermite_tangent2")){
        auto st=setup(0x4951d0);st.put32(0,E+0x20);st.put32(4,E+0x100);f.neighbor_args(st,8);run();capture_spline_golden(i,f);
        CourseTangents t{};std::memcpy(&t,f.image.data()+0x20,sizeof(t));calc_hermite_tangent2(t,f.p,f.neighbors(),f.tuning);
        auto exp=f;exp.put(0x20,t);compare_spline_image("calc_hermite_tangent2",exp.image);
    }
    if(enabled("calc_hermite_coefficients")){
        auto st=setup(0x494e20);st.put32(0,E+0x20);guest_call.ecx=E+0x100;guest_call.eax=E+0x124;guest_call.edi=E+0x300;guest_call.esi=E+0x30c;run();capture_spline_golden(i,f);
        auto exp=f;exp.put(0x20,calc_hermite_coefficients(f.p[0],f.p[3],f.tangents[0],f.tangents[1]));
        compare_spline_image("calc_hermite_coefficients",exp.image);
    }
    if(enabled("calc_carry_variable")){
        setup(0x494d30);guest_call.eax=E+0x2a0;guest_call.xmm0=fbits(f.value);run();capture_spline_golden(i,f);
        compare_float("calc_carry_variable",ffrom(guest_call.out_xmm0),calc_carry_variable(f.value,f.coefficients));
        compare_spline_image("calc_carry_variable_memory",f.image);
    }
    if(enabled("solve_hermite")){
        auto st=setup(0x494ee0);st.put32(0,E+0x20);st.put32(4,E+0x280);st.putf(8,f.u);st.putf(12,f.v);st.put32(16,E+0x100);st.put32(20,E+0x300);run();capture_spline_golden(i,f);
        CourseVec3 out{};solve_hermite(out,f.point,f.u,f.v,f.p,f.tangents);auto exp=f;exp.put(0x20,out);
        compare_spline_image("solve_hermite",exp.image);
    }
    if(enabled("calc_hermite2")){
        auto st=setup(0x4953f0);st.put32(0,E+0x280);st.putf(4,f.u);st.putf(8,f.v);st.put32(12,E+0x100);st.put32(16,E+0x140);f.neighbor_args(st,20);guest_call.st0=1;run();capture_spline_golden(i,f);
        compare_float("calc_hermite2",ffrom(guest_call.out_st0),calc_hermite2(f.point,f.u,f.v,f.p,f.normals,f.neighbors(),f.tuning));
        compare_spline_image("calc_hermite2_memory",f.image);
    }
    if(enabled("calc_y_pos_spl")){
        auto st=setup(0x43cd50);guest_call.esi=E+0x100;st.putf(0,f.point.x);st.putf(4,f.point.z);st.put32(8,E+0x140);f.neighbor_args(st,12);guest_call.st0=1;run();capture_spline_golden(i,f);
        compare_float("calc_y_pos_spl",ffrom(guest_call.out_st0),calc_y_pos_spl(f.point.x,f.point.z,f.p,f.normals,f.neighbors(),f.tuning));
        compare_spline_image("calc_y_pos_spl_memory",f.image);
    }
    if(enabled("course_spline_chain")){
        // Keep independent native/guest tangent state between both stages.
        auto st=setup(0x4951d0);st.put32(0,E+0x300);st.put32(4,E+0x100);f.neighbor_args(st,8);run();
        auto exp=f;calc_hermite_tangent2(exp.tangents,exp.p,exp.neighbors(),exp.tuning);exp.put(0x300,exp.tangents);
        compare_spline_image("course_spline_chain_tangents",exp.image);
        prepare(0x494ee0);st.put32(0,E+0x20);st.put32(4,E+0x280);st.putf(8,f.u);st.putf(12,f.v);st.put32(16,E+0x100);st.put32(20,E+0x300);run();
        CourseVec3 out{};solve_hermite(out,exp.point,exp.u,exp.v,exp.p,exp.tangents);exp.put(0x20,out);
        compare_spline_image("course_spline_chain_solve",exp.image);
    }
    if(enabled("find_primary_course_run")){
        // All original roots resolve only inside this diagnostic guest arena;
        // native code gets bounded selected views, with no guest pointers.
        auto exp=f;Bytes im(exp.image.data(),exp.image.size());
        const unsigned type=(i>>4)&1u;
        const int total=1+int(i%27u),count=1+int((i*19u)%80u);
        const bool present=i%17u!=0u,force=i%7u==0u;
        const std::int16_t target=std::int16_t(i%35u);
        std::int32_t hint=int((i*23u)%unsigned(count+4))-1;
        if(i%11u==0u)hint=count+20;
        im.put32(0x100,type);im.put16(0x108,std::uint16_t(target));
        im.puti(0x50c,count);
        for(int j=0;j<count;++j)im.put16(0x600u+unsigned(j)*2u,std::uint16_t((i%3u)?j/3:(j*7)%total));
        for(int j=0;j<total;++j){im.put16(0x800u+unsigned(j)*4u,std::uint16_t(j*3));im.put16(0x802u+unsigned(j)*4u,std::uint16_t(j*3+2));}
        const auto before=exp.image;
        exp.guest();prepare(0x43d4d0);guest_call.ecx=E+0x100;guest_call.eax=std::uint32_t(hint);
        Bytes st(reinterpret_cast<void*>(S),16);st.put32(0,E+0x20);st.put32(4,E+0x24);
        const std::uint32_t roots[]={0x780140u+type*4u,0x780228u+type*4u,0x780218u+type*4u,0x780238u};
        std::uint32_t saved[4];for(unsigned j=0;j<4;++j)saved[j]=*reinterpret_cast<std::uint32_t*>(roots[j]);
        const auto saved_force=*reinterpret_cast<std::uint8_t*>(0x780190u);
        *reinterpret_cast<std::uint32_t*>(roots[0])=present?E+0x500:0;
        *reinterpret_cast<std::uint32_t*>(roots[1])=E+0x600;
        *reinterpret_cast<std::uint32_t*>(roots[2])=E+0x800;
        *reinterpret_cast<std::uint32_t*>(roots[3])=std::uint32_t(total);
        *reinterpret_cast<std::uint8_t*>(0x780190u)=force?0x80:0;
        run();
        auto a=im.i32(0x20),b=im.i32(0x24);
        CourseRunTables tables{im.sub(0x500,0x10),im.sub(0x600,unsigned(count)*2u),im.sub(0x800,unsigned(total)*4u),total,force,present};
        const auto result=find_primary_course_run(tables,target,hint,a,b);
        im.puti(0x20,a);im.puti(0x24,b);
        compare_u32("find_primary_course_run_return",guest_call.out_eax,std::uint32_t(result));
        compare_spline_image("find_primary_course_run_memory",exp.image);
        for(unsigned j=0;j<4;++j)*reinterpret_cast<std::uint32_t*>(roots[j])=saved[j];
        *reinterpret_cast<std::uint8_t*>(0x780190u)=saved_force;
        (void)before;
    }
    compare_u32("course_spline_tuning_preserved_lateral",*reinterpret_cast<std::uint32_t*>(0x67d94c),fbits(f.tuning.lateral));
    compare_u32("course_spline_tuning_preserved_longitudinal",*reinterpret_cast<std::uint32_t*>(0x67d950),fbits(f.tuning.longitudinal));
}

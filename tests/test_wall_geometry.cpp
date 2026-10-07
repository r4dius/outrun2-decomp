#include "support/wall_geometry_fixture.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace outrun::testing;
namespace {
unsigned checks=0;
void check(bool yes,const char* what){++checks;if(!yes)throw std::runtime_error(what);}
template<class F>void rejected_unchanged(WallGeometryFixture& f,F action){
    const auto before=f.image;bool failed=false;
    try{action();}catch(const std::exception&){failed=true;}
    check(failed,"invalid native view was not rejected");check(before==f.image,"rejection wrote state");
}
void identity_stack(WallGeometryFixture& f){auto b=f.bytes();wall_matrix(b.sub(0xf80,64),0,0,0,0);wall_matrix(b.sub(0x710,64),0,0,0,0);}
}
int main(){try{
    // Load rotation: all four translation words survive, zero fourth column.
    for(unsigned i=0;i<3;++i){
        auto f=make_wall_geometry_fixture(i,0),expected=f;auto b=expected.bytes();
        const auto source=b.u32(0x1340);
        for(unsigned row=0;row<3;++row){for(unsigned col=0;col<3;++col)b.put32(0xf80+row*16+col*4,b.u32(source+row*16+col*4));b.put32(0xf80+row*16+12,0);}
        run_wall_geometry_native(f,0);check(f.image==expected.image,"rotation-only load/overlap order");
    }
    {auto f=make_wall_geometry_fixture(0,1);auto b=f.bytes();identity_stack(f);
     auto m=b.sub(0xf80,64);m.putf(0,2);m.putf(4,3);m.putf(8,4);m.putf(0x30,999);
     auto s=f.matrix();auto q=pc_matrix_inverse_vector(s,{1,2,3});check(q.x==20&&q.y==2&&q.z==3,"inverse vector must not subtract translation");}
    // CopColiPoint: four course selectors and persistent temporary stack bytes.
    for(unsigned t=0;t<4;++t){
        auto f=make_wall_geometry_fixture(t,2);auto b=f.bytes();b.puti(0x1308,15);auto w=f.tables();auto s=f.matrix();
        auto expected=pc_transform_point(w.transforms[t?1:0],wall_read(b,(t%16)*64));
        const auto original_slot=b.sub(0xf80,64);std::array<std::uint32_t,16> old{};for(unsigned k=0;k<16;++k)old[k]=original_slot.u32(k*4);
        cop_coli_point(w,t,t,s,{b.sub(0x480,12),b.sub(0x48c,12),b.sub(0x498,12),b.sub(0x4a4,12)});
        check(b.f32(0x480)==expected.x&&b.f32(0x484)==expected.y&&b.f32(0x488)==expected.z,"Cop world vertices");
        check(s.depth==2&&s.current_offset==128,"Cop stack balanced");
        for(unsigned k=0;k<16;++k){check(b.u32(0xf80+k*4)==old[k],"Cop changed parent slot");check(b.u32(0xfc0+k*4)==w.transforms[t?1:0].u32(k*4),"Cop lost temporary slot");}
    }
    {auto f=make_wall_geometry_fixture(0,2);auto b=f.bytes();auto s=f.matrix();auto w=f.tables();
     cop_coli_point(w,0,0,s,{b.sub(0x480,12),b.sub(0x48c,12),b.sub(0x498,12),b.sub(0x4a4,12)});
     check(s.depth==2&&s.current_offset==64,"original overflow Push/Pop asymmetry");}
    // Input/output alias of the polygon itself is supported by the original snapshot.
    {auto f=make_wall_geometry_fixture(1,2);auto b=f.bytes();auto s=f.matrix();auto w=f.tables();
     std::array<CourseProbe,4> expected{};for(unsigned k=0;k<4;++k)expected[k]=pc_transform_point(w.transforms[1],wall_read(b,k*12));
     cop_coli_point(w,0,1,s,{b.sub(12,12),b.sub(0,12),b.sub(36,12),b.sub(24,12)});
     const unsigned offsets[]={12,0,36,24};for(unsigned k=0;k<4;++k)check(b.f32(offsets[k])==expected[k].x&&b.f32(offsets[k]+8)==expected[k].z,"Cop did not snapshot all input vertices");}
    // Scalar side boundary: exact equality selects the >= side.
    for(unsigned i: {0u,1u,2u}){
        auto f=make_wall_geometry_fixture(i,3);run_wall_geometry_native(f,3);auto b=f.bytes();
        check(b.f32(0x480)==(i<2?8.f:0.f),"wall side equality");
        check(b.f32(0x48c)==(i<2?-1.f:1.f),"wall normal sign");
        check(b.u32(0x49c)==0u,"offset plane Y must be exactly zero");
    }
    {auto f=make_wall_geometry_fixture(126,3);run_wall_geometry_native(f,3);check(std::isnan(f.bytes().f32(0x48c)),"degenerate face must not invent fallback normal");}
    {auto f=make_wall_geometry_fixture(0,4);identity_stack(f);auto b=f.bytes();b.puti(0x500,4);
     for(unsigned k=0;k<4;++k)wall_write(b,0x504+k*12,{k==0?-1.f:k==1?0.f:float(k),0,0});
     b.putf(0x740,5);auto s=f.matrix();auto result=push_outpos_mat_obsolete(b.sub(0x700,0x800),b.sub(0x4c0,36),b.sub(0x500,388),s);
     check(result.mask==0xc0000000u&&result.distance==1.f,"push threshold/mask orientation");
     check(b.f32(0x740)==6&&pc_matrix_translation(s).x==0,"push must move work, not current stack");
     result=push_outpos_mat_obsolete(b.sub(0x700,0x800),b.sub(0x4c0,36),b.sub(0x500,388),s);
     check(b.f32(0x740)==7&&result.distance==1.f,"retained push must see unchanged current matrix");
     b.puti(0x500,32);for(unsigned k=0;k<32;++k)b.putf(0x504+k*12,k==31?-1.f:2.f);
     result=push_outpos_mat_obsolete(b.sub(0x700,0x800),b.sub(0x4c0,36),b.sub(0x500,388),s);
     check(result.mask==1u,"32nd point corresponds to least significant mask bit");
     b.puti(0x500,-9);b.putf(0x504,-2);
     result=push_outpos_mat_obsolete(b.sub(0x700,0x800),b.sub(0x4c0,36),b.sub(0x500,388),s);
     check(result.mask==0x80000000u,"nonpositive shape count clamps to one");}
    {auto f=make_wall_geometry_fixture(0,5);identity_stack(f);auto b=f.bytes();b.puti(0x500,2);b.put32(0x1324,0xc0000000u);
     wall_write(b,0x504,{-1,1,2});wall_write(b,0x510,{-2,3,4});auto s=f.matrix();s.current().putf(0x30,99);
     bool hit=coli_set_obsolete(b.sub(0x700,0x800),b.sub(0x4c0,36),b.sub(0x500,388),b.u32(0x1324),s,b.sub(0x132c,12));
     check(hit&&b.f32(0x132c)==(1.f/-3.f)*5.f,"weighted point arithmetic");
     check(b.f32(0xfc0+0x30)==99&&s.current_offset==128&&s.depth==2,"rotation load must retain pushed translation");
     hit=coli_set_obsolete(b.sub(0x700,0x800),b.sub(0x4c0,36),b.sub(0x500,388),0,s,b.sub(0x132c,12));
     check(!hit&&b.u32(0x132c)==0&&b.u32(0x1330)==0&&b.u32(0x1334)==0,"no hit clears all coordinates");}
    {auto f=make_wall_geometry_fixture(124,5);check(run_wall_geometry_native(f,5)==1,"negative extended projection rounded to zero must be retained");}
    {auto f=make_wall_geometry_fixture(125,5);check(run_wall_geometry_native(f,5)==1,"twelve selected projections");}
    for(unsigned bad=0;bad<9;++bad){auto f=make_wall_geometry_fixture(0,2);auto b=f.bytes();b.puti(0x1308,15);auto s=f.matrix();auto w=f.tables();
        std::array<Bytes,4> out={b.sub(0x480,12),b.sub(0x48c,12),b.sub(0x498,12),b.sub(0x4a4,12)};
        if(bad==0)w.courses[0].polygons=Bytes(nullptr,0);
        if(bad==1)w.transforms[0]=b.sub(0x400,63);
        if(bad==2)out[3]=b.sub(0x4a4,11);
        if(bad==3)w.courses[0].polygons_present=false;
        if(bad==4)s.current_offset=1024;
        if(bad==5)s.current_offset=960;
        if(bad==6)s.current_offset=-1;
        rejected_unchanged(f,[&]{cop_coli_point(w,bad==7?0xffffffffu:0u,bad==8?4u:0u,s,out);});
    }
    {auto f=make_wall_geometry_fixture(125,5);auto b=f.bytes();auto s=f.matrix();b.putf(0x504+12*12,-13);b.put32(0x1324,0xfff80000u);
     rejected_unchanged(f,[&]{coli_set_obsolete(b.sub(0x700,0x800),b.sub(0x4c0,36),b.sub(0x500,388),b.u32(0x1324),s,b.sub(0x132c,12));});
     b.put32(0x1324,0xfff00000u);
     rejected_unchanged(f,[&]{coli_set_obsolete(b.sub(0x700,0x800),b.sub(0x4c0,36),b.sub(0x500,15),b.u32(0x1324),s,b.sub(0x132c,12));});
     rejected_unchanged(f,[&]{coli_set_obsolete(b.sub(0x700,0x800),b.sub(0x4c0,36),b.sub(0x500,388),b.u32(0x1324),s,b.sub(0x132c,11));});
     rejected_unchanged(f,[&]{push_outpos_mat_obsolete(b.sub(0x700,0x4b),b.sub(0x4c0,36),b.sub(0x500,388),s);});
     rejected_unchanged(f,[&]{pc_matrix_load_rotation(s,b.sub(0x710,43));});}
    // Every native fixture uses poisonous serialized guest wheel pointers.
    for(unsigned i=0;i<256;++i){auto f=make_wall_geometry_fixture(i,7);for(auto id:wall_sequence)run_wall_geometry_native(f,id);
     for(unsigned k=0;k<4;++k)check(f.bytes().u32(0x700+0x248+k*4)==0xf0bad000u+k*0x100u,"native dereferenced or rewrote a guest pointer");}
    std::cout<<checks<<" wall geometry boundary/view checks passed; no commercial resources\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

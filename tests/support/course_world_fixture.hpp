#pragma once
// Synthetic PC-layout course data; no commercial asset or code bytes.
#include "course_query_fixture.hpp"
#include "driving/pc_course_world.hpp"
#include "driving/pc_wrecker.hpp"
#include <cstring>
#include <limits>
namespace outrun::testing {
using namespace outrun::driving;
constexpr std::size_t world_image_size=0x4800;
struct CourseWorldFixture {
    std::array<std::uint8_t,world_image_size> image{};
    std::array<std::array<std::uint16_t,65536>,4> grids{};
    Bytes bytes(){return {image.data(),image.size()};}
    void rebuild_grids(){
        auto b=bytes();
        for(unsigned t=0;t<4;++t){
            grids[t].fill(static_cast<std::uint16_t>(b.i16(0x4680+t*16)));
            const auto cell=b.u32(0x4684+t*16);
            if(cell<65536u)grids[t][cell]=static_cast<std::uint16_t>(b.i16(0x4688+t*16));
        }
    }
    PcMatrixStack matrix(){auto b=bytes();return {b.sub(0x4000,0x400),b.i32(0x4550),b.i32(0x4554),b.i32(0x4558)};}
    EasyLctPredictionState prediction(){
        auto b=bytes();EasyLctPredictionState p;
        for(unsigned k=0;k<16;++k)p.recent[k]=b.u32(0x4500+k*4);
        p.cursor=b.u32(0x4540);p.easy=b.u32(0x4544);return p;
    }
    CourseWorldTables tables(){
        auto b=bytes();CourseWorldTables w;
        for(unsigned t=0;t<4;++t){
            auto m=b.sub(t*4096u,4096);const auto options=m.u32(0xf04);
            w.courses[t]={{m.sub(0x140,0x10),m.sub(0x200,0x80),m.sub(0x280,0x100),m.i32(0xf08),bool(options&1),bool(options&2)},
                m.sub(0x180,0x20),m.sub(0x400,0x400),m.sub(0x800,0x300),m.sub(0xc00,0x200),t,bool(options&4)};
            w.grids[t]=Bytes(grids[t].data(),grids[t].size()*2u);
            w.grids_present[t]=(b.u32(0x4648)&(1u<<t))!=0;
        }
        w.transforms={b.sub(0x4400,64),b.sub(0x4440,64)};
        w.tuning={b.f32(0xf20),b.f32(0xf24)};return w;
    }
    void save(const PcMatrixStack& s,const EasyLctPredictionState& p){
        auto b=bytes();b.puti(0x4550,static_cast<std::int32_t>(s.current_offset));b.puti(0x4554,s.depth);b.puti(0x4558,s.capacity);
        for(unsigned k=0;k<16;++k)b.put32(0x4500+k*4,p.recent[k]);
        b.put32(0x4540,p.cursor);b.put32(0x4544,p.easy);
    }
};
inline void world_put_probe(Bytes b,std::size_t o,const CourseProbe& p){b.putf(o,p.x);b.putf(o+4,p.y);b.putf(o+8,p.z);}
inline CourseProbe world_probe(Bytes b,std::size_t o){return {b.f32(o),b.f32(o+4),b.f32(o+8)};}
inline void configure_world_boundary(CourseWorldFixture& f,unsigned scenario,unsigned options){
    auto m=f.bytes();m.put32(0x4644,options);m.put32(0x4640,0x400);m.put32(0x4648,3);
    m.put32(0x4540,14);m.put32(0x4544,0);
    for(unsigned k=0;k<16;++k)m.put32(0x4500+k*4,k<7u?1u:0u);
    m.puti(0x4550,128);m.puti(0x4554,2);m.puti(0x4558,15);
    for(unsigned t=0;t<2;++t){
        auto a=m.sub(0x4400+t*64,64);
        for(unsigned k=0;k<16;++k)a.putf(k*4,0);
        a.putf(0,1);a.putf(0x14,1);a.putf(0x28,1);a.putf(0x3c,1);a.putf(0x34,t?-7.f:3.f);
    }
    for(unsigned t=0;t<4;++t){
        auto b=m.sub(t*4096u,4096);b.put32(0xf04,6);b.puti(0xf08,1);b.putf(0xf20,1);b.putf(0xf24,1);
        b.puti(0x148,1);b.puti(0x14c,1);b.put16(0x200,0);b.put16(0x280,0);b.put16(0x282,0);b.put8(0x180,1);
        const CourseProbe vertices[]={{0,0,0},{10,0,0},{10,0,10},{0,0,10}};
        for(unsigned v=0;v<4;++v){world_put_probe(b,0x400+v*12,vertices[v]);world_put_probe(b,0x800+v*12,{0,1,0});}
        b.put16(0x43c,2);b.put16(0xc00,1);b.put16(0xc02,0);b.put16(0xc20,1);b.put16(0xc22,0);b.put16(0xc00+500,0);
        m.put16(0x4680+t*16,16);m.put32(0x4684+t*16,0xffffffffu);m.put16(0x4688+t*16,250);
    }
    world_put_probe(m,0x4600,{5,9,5});m.put32(0x4620,0xdeadbeef);m.put32(0x4624,0xcafebabe);m.put32(0x4628,0xabcddcba);
    switch(scenario){
    case 0:break; // Primary hit.
    case 1:m.put32(0x4648,2);break; // Missing primary, secondary hit.
    case 2:m.put16(0x4680,250);break; // Empty primary list, secondary hit.
    case 3:m.put16(0x4680,0);break; // Nonempty zero-offset list is not accepted.
    case 4:m.put32(0x4648,0);break; // Both absent, prediction unchanged.
    case 5:m.put16(0x4680,0);m.put16(0x4690,250);break;
    case 6:m.putf(0x4600,0);m.putf(0x4608,0);break; // Exact corner included.
    case 7:m.putf(0x4600,-0.0005f);m.putf(0x4608,5);break;
    case 8:m.putf(0x4600,5);m.putf(0x4608,-0.0005f);break;
    case 9:m.putf(0x4600,-0.0005f);m.putf(0x4608,-0.0005f);break;
    case 10:m.putf(0x4600,10.0005f);break; // +X cannot recover.
    case 11:for(unsigned t=0;t<4;++t)m.put8(t*4096u+0x180,32);break; // Modulo32 kind, four retries.
    case 12:for(unsigned t=0;t<4;++t)m.put8(t*4096u+0x180,8);break; // Do not write special index.
    case 13:m.put32(0x4544,3);m.put32(0x4648,15);break;
    case 14:m.puti(0x4558,3);break; // Overflowing push, asymmetric pop.
    case 15:for(unsigned t=0;t<4;++t){m.put32(0x4684+t*16,0x8080);m.put16(0x4688+t*16,250);}break;
    }
}
inline CourseWorldFixture make_course_world_fixture(unsigned i,unsigned id){
    CourseWorldFixture f;std::mt19937 rng(0x4f523016u^(i*1664525u));
    for(auto& b:f.image)b=std::uint8_t(rng());
    auto m=f.bytes();
    for(unsigned t=0;t<4;++t){
        auto c=make_course_query_fixture(i,true,false);std::memcpy(f.image.data()+t*4096,c.image.data(),4096);
        auto q=m.sub(t*4096u,4096);q.put32(0xf00,t);q.put16(0xc00,0);q.put16(0xc00+250*2,0);
        // The standalone secondary-run corpus deliberately exercises counts
        // that can exceed the synthetic polygon allocation.  World/road-info
        // integration must keep every returned run index backed by real
        // geometry; otherwise the original follows random fixture bytes and
        // may manufacture NaNs rather than testing the reconstructed service.
        if(t!=0u){
            q.puti(0x14c,q.i32(0x148));
            // Secondary mode 101/103 follows 0x400 spline runs until a
            // 0x800 marker.  Give every synthetic spline record a valid marker
            // so an integrated road query never walks past its polygon table.
            const auto valid=q.i32(0x148);
            for(std::int32_t j=0;j<valid;++j){
                const auto off=0x400u+std::size_t(j)*0x40u+0x3cu;
                const auto fl=static_cast<std::uint16_t>(q.i16(off));if(fl&0x400u)q.put16(off,std::uint16_t(fl|0x800u));
            }
        }
        if(i%23u==0u){q.put16(0xc00,1);q.put16(0xc02,std::uint16_t(q.i32(0xf10)));}
        const auto list=static_cast<std::uint16_t>(q.i16(0xf18));
        m.put16(0x4680+t*16,(i%23u==0u&&t==0u)?0:list);m.put32(0x4684+t*16,0xffffffffu);m.put16(0x4688+t*16,250);
    }
    auto sample=[&](float a,float b){return a+(b-a)*(float(rng()&0xffffu)/65535.f);};
    for(unsigned t=0;t<2;++t){
        auto a=m.sub(0x4400+t*64,64);
        for(unsigned j=0;j<16;++j)a.putf(j*4,0);
        a.putf(0,1);a.putf(0x14,1);a.putf(0x28,1);a.putf(0x3c,1);
        if(i>=128u){
            const float angle=sample(-0.08f,0.08f),c=std::cos(angle),s=std::sin(angle);
            a.putf(0,c);a.putf(8,-s);a.putf(0x20,s);a.putf(0x28,c);
            if((i/9u)%3u==1u){a.putf(0x14,c);a.putf(0x18,s);a.putf(0x24,-s);a.putf(0x28,c);}
            a.putf(0x30,sample(-2,2));a.putf(0x38,sample(-2,2));
        }
        a.putf(0x34,t?32.f:-8.f);
    }
    const int off=int((i%8u)+2u)*64;m.puti(0x4550,off);m.puti(0x4554,int(i%4u));m.puti(0x4558,15);
    for(unsigned j=0;j<16;++j)m.put32(0x4000+unsigned(off)+j*4,m.u32(0x4400+j*4));
    if(id<5u||id==15u){
        m.puti(0x4554,int(i%12u)-2);m.puti(0x4558,int((i/3u)%8u)+1);
        if(i%127u==0u)m.puti(0x4554,std::numeric_limits<std::int32_t>::max());
        if(i%131u==0u)m.puti(0x4554,std::numeric_limits<std::int32_t>::min());
        if(id==4u){
            auto a=m.sub(0x4000+unsigned(off),64);
            static constexpr float w[]={1.f,0.5f,2.f,-2.f,1.00000011920928955078125f,0.99999988079071044921875f};
            a.putf(0x3c,w[i%6u]);
        }
    }
    m.put32(0x4630,i%4u);
    m.puti(0x4634,m.sub((i%4u)*4096u,4096).i32(0xf10));
    m.putf(0x4638,sample(-0.25f,0.25f));m.putf(0x463c,sample(-0.25f,0.25f));
    {const auto type=m.u32(0x4630);auto q=m.sub(type*4096u,4096);const auto idx=q.i32(0xf10);
      m.put32(0x4560,type);m.puti(0x4564,type?int((i%4u)+100u):0);
      m.put16(0x4568,std::uint16_t(q.i16(0x200+std::size_t(idx)*2u)));m.put32(0x456c,0xa5a5a5a5u);
      m.puti(0x4670,idx);}
    m.put32(0x4660,0x4400);
    if(id==2u&&i%3u!=0u)m.put32(0x4660,0x4000u+unsigned(off)+(i%3u==1u?4u:0xfffffffcu));
    for(unsigned k=0;k<16;++k)m.put32(0x4500+k*4,k<(i%16u)?1u:0u);
    m.put32(0x4540,(i/2u)%16u);m.put32(0x4544,(i/16u)%4u);
    m.put32(0x4640,m.u32(0xf14));m.put32(0x4644,i%8u);m.put32(0x4648,(i/4u)%16u);
    if(i>=128u&&i%7u)m.put32(0x4648,15u);
    world_put_probe(m,0x4600,{m.f32(0xe00),100.f,m.f32(0xe08)});
    if(id<5u)world_put_probe(m,0x4600,{sample(-200,200),sample(-200,200),sample(-200,200)});
    if(id>=5u&&i%17u==0u){
        static constexpr float bounds[]={-4000,-3072,-3048,0,24,3048,3072,4000};
        m.putf(0x4600,bounds[(i/17u)%8u]);m.putf(0x4608,bounds[(i/136u)%8u]);
    }
    if(id>=5u&&i%11u==0u)for(unsigned t=0;t<4;++t){m.put32(0x4684+t*16,0x7f80);m.put16(0x4688+t*16,250);}
    if(id>=5u&&id<12u&&i<128u)configure_world_boundary(f,i/8u,i%8u);
    if(id==18u||id==19u)m.putf(0x4674,sample(-3.0f,3.0f));
    if(id==20u)world_put_probe(m,0x4610,{sample(-20,20),sample(-20,20),sample(-20,20)});
    if(id==21u||id==23u||id==24u||id==25u||id==26u||id==27u||id==28u||id==29u||id==50u||id==51u||id==52u){
        // ReconstructPostureMatrixAndFaceWork performs a real world->course
        // query after preserving the caller translation.  Keep the dedicated
        // integration corpus on a small, valid square and start from a clean
        // caller matrix so the query cannot accidentally walk random fixture
        // geometry.  The parent itself supplies the rotations under test.
        configure_world_boundary(f,(i/64u)%3u,7u);
        m=f.bytes();
        const auto current=static_cast<std::size_t>(m.i32(0x4550));
        auto cur=m.sub(0x4000u+current,64u);
        for(unsigned k=0;k<16;++k)cur.putf(k*4u,0.0f);
        cur.putf(0x00,1.0f);cur.putf(0x14,1.0f);cur.putf(0x28,1.0f);cur.putf(0x3c,1.0f);
        if(id==28u||id==29u){
            // r028 CarBodyWallColiCheck/ColiCar drives the query and Cbw controller
            // from the same polygon.  Use a deterministic convex XZ square
            // that is simultaneously a valid mode-0 GetRoadCond polygon and
            // a non-degenerate generic wall face.
            m.put32(0x4648,15u);m.put32(0x4544,1u);m.put32(0x4644,7u);m.put32(0x4640,0u);
            const unsigned route=i%6u;const std::uint8_t kind=(route==1u||route==4u)?0u:(route==2u||route==5u)?5u:1u;
            const CourseProbe v[4]={{-2,0,-2},{2,0,-2},{2,0,2},{-2,0,2}};
            for(unsigned t=0;t<4;++t){
                auto q=m.sub(t*4096u,4096u);q.put8(0x180u,kind);
                for(unsigned k=0;k<4;++k)world_put_probe(q,0x400u+k*12u,v[k]);
            }
        }
        if(id==27u){
            // r028 GetYPositionProg_BK needs all four explicit course tables so
            // the special selector can hit type 2/3 instead of immediately
            // falling back to primary type 0.
            m.put32(0x4648,15u);
            m.put32(0x4544,(i%8u)==0u?0u:1u);
            m.put32(0x4644,i%8u);
            static constexpr std::uint32_t modes[]{0u,0x100u,0x400u,0x800u};
            m.put32(0x4640,modes[(i/8u)%4u]);
        }
        if(id==26u){
            // CbwColiWall r027 needs a deterministic vertical wall.  Polygon 1
            // deliberately has p2==p3 so the material-mode special paths can
            // be selected without making CopColiPoint itself synthetic.
            const unsigned geometry=i%5u;
            for(unsigned t=0;t<4;++t){
                auto q=m.sub(t*4096u,4096u);
                CourseProbe v[4]{};
                if(geometry==0u){ // material 10, low-mode 2 regular special
                    v[0]={0,0,0};v[1]={0,0,1};v[2]={2,0,2};v[3]=v[2];
                }else if(geometry==1u){ // material 9, 0x42 reorder
                    v[0]={0,0,1};v[1]={2,0,2};v[2]={0,0,0};v[3]=v[2];
                }else if(geometry==2u){ // material 3, 0xC2 reorder
                    v[0]={2,0,2};v[1]={0,0,0};v[2]={0,0,1};v[3]=v[2];
                }else if(geometry==3u){ // material 11, alternate special
                    v[0]={1,0,0};v[1]={1,0,1};v[2]={0,0,2};v[3]=v[2];
                }else{ // material 0, generic CalcColiWallFace path
                    v[0]={0,0,1};v[1]={0,0,0};v[2]={0,0,1};v[3]={1,0,2};
                }
                for(unsigned k=0;k<4;++k)world_put_probe(q,0x440u+k*12u,v[k]);
            }
        }
        if(id==51u||id==52u){
            // assCompulsiveMove advances the local OnRoadPlace by 1,1,14,1.
            // Keep those advances inside a 32-position primary course while
            // mapping every position to one of sixteen backed polygons.
            auto q=m.sub(0,4096u);q.put32(0xf04,(q.u32(0xf04)|1u));q.puti(0xf08,32);q.puti(0x14c,32);
            for(unsigned k=0;k<32;++k){q.put16(0x200u+k*2u,std::uint16_t(k));q.put16(0x280u+k*4u,std::uint16_t(std::min(k,15u)));q.put16(0x282u+k*4u,std::uint16_t(std::min(k,15u)));}
            for(unsigned k=0;k<16;++k){
                const float yaw=(float(int(k)-7))*0.0125f,c=std::cos(yaw),sn=std::sin(yaw);const float cx=float(k)*1.25f,cz=float(k)*3.0f;
                const CourseProbe local[4]={{-1,0,-1},{1,0,-1},{1,0,1},{-1,0,1}};
                for(unsigned v=0;v<4;++v){const float x=local[v].x*c-local[v].z*sn+cx,z=local[v].x*sn+local[v].z*c+cz;world_put_probe(q,0x400u+k*0x40u+v*12u,{x,0,z});}
                q.put16(0x400u+k*0x40u+0x3cu,0u);
            }
            m.put32(0x4560,0u);m.puti(0x4564,0);m.put16(0x4568,0u);m.put32(0x456c,0u);m.puti(0x4670,0);
        }
    }
    // Wrecker direct-branch coverage needs a >40 course length that still maps
    // to a valid synthetic polygon, rather than deliberately walking garbage.
    if(id==12u&&i%17u==0u){
        const auto type=m.u32(0x4630);auto q=m.sub(type*4096u,4096);const auto idx=q.i32(0xf10);
        q.puti(0xf08,64);q.put32(0xf04,q.u32(0xf04)&~1u);
        q.put16(0x200+std::size_t(idx)*2u,41);m.put16(0x4568,41);
    }
    // Overflow itself is checked directly; repeated calls do not walk past
    // the explicit arena, where original raw pointer behavior is undefined.
    if(id==7u)m.puti(0x4558,15);
    f.rebuild_grids();return f;
}
inline std::uint32_t run_course_world_native(CourseWorldFixture& f,unsigned id){
    auto b=f.bytes();auto s=f.matrix();auto p=f.prediction();std::uint32_t ret=0;
    if(id==0u)pc_matrix_push(s);
    else if(id==1u)pc_matrix_pop(s);
    else if(id==2u)pc_matrix_load(s,b.sub(b.u32(0x4660),64));
    else if(id==3u)world_put_probe(b,0x4610,pc_matrix_inverse_point(s,world_probe(b,0x4600)));
    else if(id==4u)world_put_probe(b,0x4610,pc_matrix_point(s,world_probe(b,0x4600)));
    else if(id==8u)pc_matrix_push_load(s,b.sub(b.u32(0x4660),64));
    else if(id==9u)world_put_probe(b,0x4610,pc_matrix_vector(s,world_probe(b,0x4600)));
    else if(id==10u){
        auto tables=f.tables();const auto type=b.u32(0x4630);
        if(type>=4u)throw std::out_of_range("world-normal fixture type");
        world_put_probe(b,0x4610,course_collision_world_normal(tables.courses[type],b.i32(0x4634),s,tables.transforms[type==0u?0u:1u]));
    }
    else if(id==11u){
        auto tables=f.tables();PcRoadInfoContext rc{tables,s,{b.f32(0x4638),b.f32(0x463c)}};
        ret=pc_get_cs_road_info_by_cs_len(b.sub(0x4700,0x64),b.sub(0x4560,0x10),b.i32(0x4670),rc)?1u:0u;
    }
    else if(id==15u)pc_matrix_push_unit(s);
    else if(id==16u)pc_matrix_unit_rotation(s);
    else if(id==17u)pc_matrix_get(s,b.sub(0x4700,64));
    else if(id==18u)pc_matrix_rotate_x(s,b.f32(0x4674));
    else if(id==19u)pc_matrix_rotate_z(s,b.f32(0x4674));
    else if(id==20u)pc_matrix_translate_vector(s,world_probe(b,0x4610));
    else{
        auto tables=f.tables();CourseWorldQuery q{tables,s,p};auto point=world_probe(b,0x4600);
        std::uint32_t index=b.u32(0x4620),special=b.u32(0x4624),kind=b.u32(0x4628);const auto opts=b.u32(0x4644);
        if(id==5u)ret=get_y_position_prog(q,b.u32(0x4640),point,opts&1u?&index:nullptr,opts&2u?&special:nullptr,opts&4u?&kind:nullptr);
        else if(id==6u)ret=get_y_position_spl_chk(q,point,opts&1u?&index:nullptr,opts&2u?&special:nullptr,opts&4u?&kind:nullptr);
        else throw std::invalid_argument("unknown world native fixture id");
        world_put_probe(b,0x4600,point);b.put32(0x4620,index);b.put32(0x4624,special);b.put32(0x4628,kind);
    }
    f.save(s,p);return ret;
}
}

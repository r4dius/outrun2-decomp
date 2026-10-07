#include "driving/pc_collision.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>
using namespace outrun::driving;
static void pf(Bytes b,std::size_t o,float v){b.putf(o,v);}
namespace {
void stage0(Bytes,Bytes w,void* u){static_cast<std::vector<int>*>(u)->push_back(0);w.put32(0x200,w.u32(0x200)+1);}
void stage1(Bytes,Bytes w,void* u){static_cast<std::vector<int>*>(u)->push_back(1);w.put32(0x200,w.u32(0x200)+1);}
void stage2(Bytes,Bytes w,void* u){static_cast<std::vector<int>*>(u)->push_back(2);w.put32(0x200,w.u32(0x200)+1);}
void stage3(Bytes,Bytes w,void* u){static_cast<std::vector<int>*>(u)->push_back(3);w.put32(0x200,w.u32(0x200)+1);}

struct QueryScript { std::array<CourseQueryResult,4> r{}; unsigned calls{}; std::array<CourseProbe,4> seen{}; };
CourseQueryResult scripted_query(std::uint32_t mask,const CourseProbe& p,void* u){
    auto& q=*static_cast<QueryScript*>(u); if(mask!=0x400u) return {}; q.seen[q.calls]=p; return q.r[q.calls++];
}
}
int main(){
    std::vector<std::uint8_t> e(event_size),w(work_size),p(parameter_size);
    Bytes E(e.data(),e.size()),W(w.data(),w.size()),P(p.data(),p.size());
    for(unsigned i=0;i<16;++i)pf(W,0x10+i*4,0.0f);
    pf(W,0x10,1);pf(W,0x10+5*4,1);pf(W,0x10+10*4,1);pf(W,0x10+15*4,1);
    pf(W,0x628,0);pf(W,0x62c,1);pf(W,0x630,0);
    pf(W,0x640,2);pf(W,0x644,3);pf(W,0x648,4);
    pf(W,0x40,2);pf(W,0x44,3);pf(W,0x48,4);
    constexpr std::array<std::size_t,4> off{0x258,0x34c,0x440,0x534};
    std::array<Bytes,4> q{W.sub(off[0],0xf4),W.sub(off[1],0xf4),W.sub(off[2],0xf4),W.sub(off[3],0xf4)};
    for(unsigned axle=0;axle<2;++axle){pf(P,(axle+0x26u)*0x4cu,0.25f);pf(P,(axle+0x10u)*0x4cu,0.25f);}

    // Exhaust all four-wheel contact masks and both previous "none in contact" states.
    // +0x08 is deliberately poisoned and must not influence CarSusColiCheck.
    for(unsigned old_none=0;old_none<2;++old_none)for(unsigned mask=0;mask<16;++mask){
        pf(W,0x40,0);pf(W,0x44,0);pf(W,0x48,0);
        for(unsigned i=0;i<4;++i){q[i].put32(0,0xa5a50000u | (i<<8) | 1u);pf(q[i],0x04,float(i));pf(q[i],0x08,77.0f+float(i));pf(q[i],0x0c,0.0f);pf(q[i],0x28,0.75f);pf(q[i],0x3c,((mask>>i)&1u)?0.5f:std::nextafter(0.5f,-1.0f));pf(q[i],0x70,9);pf(q[i],0x74,9);pf(q[i],0x78,9);}
        W.put32(0x244,0x55aa5500u|(old_none?4u:0u));
        E.put32(0x2b4,0xdead0001u); // invalid guest pointer: native must use explicit P view
        for(unsigned i=0;i<4;++i)W.put32(0x248+i*4,0xdeadc000u+i*4u); // invalid guest wheel pointers
        car_sus_coli_check(E,W,P,q);
        const bool all_contact=mask==15u, none_contact=mask==0u;
        std::uint32_t expect=(0x55aa5500u|(old_none?4u:0u))&0xffffff79u;
        if(all_contact)expect|=2u;if(none_contact)expect|=4u;if(none_contact&&!old_none)expect|=0x80u;
        if(W.u32(0x244)!=expect){std::cerr<<"aggregate flags mismatch mask="<<mask<<" old="<<old_none<<"\n";return 1;}
        for(unsigned i=0;i<4;++i){const bool contact=((mask>>i)&1u)!=0;
            if(((q[i].u32(0)&1u)==0)!=contact){std::cerr<<"wheel contact flag mismatch\n";return 1;}
            if(std::fabs(q[i].f32(0x2c)-0.5f)>1e-6f){std::cerr<<"wheel +0x2c mismatch\n";return 1;}
            if(q[i].f32(0x70)!=0||q[i].f32(0x74)!=1||q[i].f32(0x78)!=0){std::cerr<<"contact normal mismatch\n";return 1;}
        }
    }
    // Bit 7 is transition-only: first no-contact call sets it, second clears it while bit 2 remains set.
    for(unsigned i=0;i<4;++i){q[i].put32(0,0);pf(q[i],0x28,0.75f);pf(q[i],0x3c,0.49f);}W.put32(0x244,0);
    car_sus_coli_check(E,W,P,q);if((W.u32(0x244)&0x84u)!=0x84u){std::cerr<<"missing no-contact transition\n";return 1;}
    car_sus_coli_check(E,W,P,q);if((W.u32(0x244)&0x84u)!=0x04u){std::cerr<<"transition bit not transient\n";return 1;}

    // Bump leaf baseline. Highest penetration is wheel 2: local y 0.1 - 0.5 = -0.4 => push +0.4.
    pf(W,0x40,2);pf(W,0x44,3);pf(W,0x48,4);
    const float ys[4]={1.0f,0.5f,0.1f,0.8f};
    for(unsigned i=0;i<4;++i){pf(q[i],0x04,float(i));pf(q[i],0x08,ys[i]);pf(q[i],0x0c,0.0f);}
    car_sus_bump_push(E,W,P,q);
    if(std::fabs(W.f32(0x40)-2.0f)>1e-6f||std::fabs(W.f32(0x44)-3.4f)>1e-6f||std::fabs(W.f32(0x48)-4.0f)>1e-6f){std::cerr<<"unexpected bump push\n";return 1;}
    if(std::fabs(W.f32(0x10+13*4)-3.4f)>1e-6f){std::cerr<<"body translation overlap mismatch\n";return 1;}

    std::vector<int> order;CollisionStages stages{stage0,stage1,stage2,stage3,&order};W.put32(0x200,0);
    if(!coli_car(E,W,stages)||order!=std::vector<int>({0,1,2,3})||W.u32(0x200)!=4){std::cerr<<"ColiCar stage order mismatch\n";return 1;}
    // Preflight must reject missing dependency before stage 0: no callback and no state change.
    for(unsigned missing=0;missing<4;++missing){order.clear();W.put32(0x200,0x12345678u);auto bad=stages;
        if(missing==0)bad.ground_face=nullptr;else if(missing==1)bad.suspension_check=nullptr;else if(missing==2)bad.suspension_bump_push=nullptr;else bad.body_wall=nullptr;
        if(coli_car(E,W,bad)||!order.empty()||W.u32(0x200)!=0x12345678u){std::cerr<<"ColiCar missing-dependency preflight failed\n";return 1;}
    }

    // Closed course-query dependencies reconstructed from the PC binary.
    if(calc_collision_area(-3072.0f,-3072.0f)!=0u ||
       calc_collision_area(-3048.0f,-3072.0f)!=1u ||
       calc_collision_area(-3072.0f,-3048.0f)!=0x100u ||
       calc_collision_area(3048.0f,3048.0f)!=0xffffu ||
       calc_collision_area(1.0e8f,-1.0e8f)!=0x00ffu){std::cerr<<"CalcCollisionArea mismatch\n";return 1;}

    std::array<std::uint8_t,0x40> poly{};Bytes POLY(poly.data(),poly.size());
    auto putv=[&](std::size_t o,float x,float y,float z){POLY.putf(o,x);POLY.putf(o+4,y);POLY.putf(o+8,z);};
    putv(0x00,0.0f,5.0f,0.0f);putv(0x0c,0.0f,8.0f,1.0f);putv(0x18,1.0f,10.0f,1.0f);putv(0x24,1.0f,7.0f,0.0f);
    if(course_triangle_plane_y(POLY,0.25f,0.75f)!=7.75f ||
       course_quad_plane_y(POLY,0.25f,0.75f)!=7.75f ||
       course_quad_plane_y(POLY,0.75f,0.25f)!=7.25f){std::cerr<<"course polygon plane-y mismatch\n";return 1;}

    EasyLctPredictionState pred{};pred.cursor=14u;
    update_easy_lct_prediction_table(pred,7u);
    if(pred.cursor!=15u||pred.recent[15]!=7u||pred.easy!=0u){std::cerr<<"prediction slot15 mismatch\n";return 1;}
    pred={};for(unsigned i=0;i<7;++i)pred.recent[i]=1u;pred.cursor=7u;
    update_easy_lct_prediction_table(pred,2u);
    if(pred.cursor!=8u||pred.recent[8]!=2u||pred.easy!=1u){std::cerr<<"prediction threshold mismatch\n";return 1;}
    pred={};for(unsigned i=0;i<7;++i)pred.recent[i]=1u;pred.recent[15]=9u;pred.cursor=14u;
    update_easy_lct_prediction_table(pred,3u);
    if(pred.easy!=0u){std::cerr<<"prediction slot15 was incorrectly counted\n";return 1;}

    std::array<std::uint8_t,0x100> ct{};Bytes CT(ct.data(),ct.size());CT.put16(0x40u+0x3cu,0xa55au);
    CT.put16(0x40u+0x3eu,static_cast<std::uint16_t>(static_cast<std::int16_t>(-123)));
    if(course_collision_ext_flags(CT,1)!=0xa55au||course_collision_ext_flags(CT,-1)!=0||course_collision_ext_flags(CT,1,false)!=0){std::cerr<<"course collision ext flags mismatch\n";return 1;}
    if(course_collision_offset_direction(CT,1,0.1f)!=920 ||
       course_collision_offset_direction(CT,1,-0.1f)!=-1166 ||
       course_collision_offset_direction(CT,1,0.1f,false)!=0){std::cerr<<"CopGetOfsDir mismatch\n";return 1;}

    std::array<std::uint8_t,16> lengths{};Bytes LT(lengths.data(),lengths.size());
    LT.put16(0,0x1234u);LT.put16(6,0xbeefu);
    if(course_length(LT,0)!=0x1234u||course_length(LT,3)!=0xbeefu||
       course_length(LT,-1)!=0u||course_length(LT,3,false)!=0u){std::cerr<<"GetCourseLength mismatch\n";return 1;}

    // GetYPositionSplChk retries the exact PC probe pattern: base, +X epsilon, +Z epsilon, +X/+Z.
    QueryScript qs{};for(unsigned i=0;i<4;++i){qs.r[i].y=10.0f+float(i);qs.r[i].course_or_return=20+i;qs.r[i].collision_index=30+i;qs.r[i].special_index=40+i;qs.r[i].flags=(i<3)?1u:0x00200002u;}
    CourseProbe cp{1.25f,2.5f,-3.75f};std::uint32_t ci=0,si=0,fl=0,ret=0;CourseQuery cq{scripted_query,&qs};
    if(!get_y_position_spl_chk(cp,&ci,&si,&fl,cq,&ret)||qs.calls!=4||cp.y!=13.0f||ci!=33||si!=33||fl!=0x00200002u||ret!=23){std::cerr<<"GetYPositionSplChk result mismatch\n";return 1;}
    const float eps=0.001f;
    if(qs.seen[0].x!=1.25f||qs.seen[0].z!=-3.75f||qs.seen[1].x!=float(1.25f+eps)||qs.seen[1].z!=-3.75f||qs.seen[2].x!=1.25f||qs.seen[2].z!=float(-3.75f+eps)||qs.seen[3].x!=float(1.25f+eps)||qs.seen[3].z!=float(-3.75f+eps)){std::cerr<<"GetYPositionSplChk retry probes mismatch\n";return 1;}
    QueryScript q1{};q1.r[0]={7.0f,9u,11u,12u,0u};cp={4,5,6};ci=si=fl=ret=0xdeadbeefu;cq.user=&q1;
    if(!get_y_position_spl_chk(cp,&ci,&si,&fl,cq,&ret)||q1.calls!=1||cp.y!=7||ci!=11||si!=0xdeadbeefu||fl!=0||ret!=9){std::cerr<<"GetYPositionSplChk first-hit mismatch\n";return 1;}
    CourseQuery missing{};if(get_y_position_spl_chk(cp,&ci,&si,&fl,missing,&ret)){std::cerr<<"GetYPositionSplChk missing dependency accepted\n";return 1;}
    std::cout<<"collision tests passed\n";return 0;
}

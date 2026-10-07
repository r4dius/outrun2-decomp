// PART_EFC effect requests and particle functions: spark (source 3,
// 41D810/41DC60/41DCC0), gravel (source 1, 41DF00/41E190/41E230), grass
// (source 2, 41E440/41E780/41E820), water (source 5, 41E9F0/41ED00/41ED80),
// misc (source 6, 41EF00/41F180/41F2F0), backfire (source 7, 41F5F0,
// 41F7F0/41F870/41F8F0/41F970, 41F9F0), tire marks (41FB50/41FDD0/4862C0),
// sources 8..10 (420110/420180, 420350/4203E0, 4207B0/420820) and the glow
// colour interpolation 4160F0.
#include "platform/race_particles_port.hpp"
#include "driving/pc_d3dx.hpp"
namespace outrun::platform::particles_detail {
bool tire_smoke_test_41d180(P&,std::uint32_t);
void smoke_born_41d1e0(P&,std::uint32_t);
void smoke_born_41d270(P&,std::uint32_t);
void smoke_ctrl_41d330(P&,std::uint32_t,std::uint32_t);
void smoke_ctrl_41d5c0(P&,std::uint32_t);
namespace {
using driving::CourseProbe;
X hyp(X a,X b,X c){return driving::x87_sqrt((a*a+b*b)+c*c);}
CourseProbe vec(const P& p,std::uint32_t a){return {p.f(a),p.f(a+4),p.f(a+8)};}
void putv(const P& p,std::uint32_t a,const CourseProbe& v){p.putf(a,v.x);p.putf(a+4,v.y);p.putf(a+8,v.z);}
void born(P& p,std::uint32_t id){const std::uint32_t w=particles_get_work_418420(p.pc,id);particles_born_418350(p.pc,w);}
// Gravity step subtracted from the falling particles' y speed.
const float Gravity=fb(0x3b315723u);
// A particle cleared (as 4182B0).
void kill(Particle& q){q.state=0;q.function=0;q.position[2]=0;q.position[1]=0;q.position[0]=0;}
// Particle motion shared by the functions: position += velocity.
void advance(Particle& q){for(unsigned k=0;k<3;++k)q.position[k]=st(X(q.position[k])+X(q.velocity[k]));}
// The road record the source bounces on: a point (+04) and its normal (+10).
const RoadPlane& road(const P& p,const ParticleSource& src){return p.at<RoadPlane>(src.plane);}
// Signed distance to the plane: ((p - o) . n) summed z, y, then x.
X plane(const Particle& q,const RoadPlane& r){
    const X dx=X(q.position[0])-X(r.point[0]),dy=X(q.position[1])-X(r.point[1]),dz=X(q.position[2])-X(r.point[2]);
    return (dz*X(r.normal[2])+dy*X(r.normal[1]))+dx*X(r.normal[0]);
}
// Velocity reflected on the plane: v += 2 (-v . n) n.
void bounce(Particle& q,const RoadPlane& r){
    const X m1=X(-1.0f);
    const X vx=X(q.velocity[0])*m1,vy=X(q.velocity[1])*m1,vz=X(q.velocity[2])*m1;
    X k=(vz*X(r.normal[2])+vy*X(r.normal[1]))+vx*X(r.normal[0]);
    k=k+k;
    const float ky=st(k*X(r.normal[1])),kz=st(k*X(r.normal[2]));
    q.velocity[0]=st(k*X(r.normal[0])+X(q.velocity[0]));
    q.velocity[1]=st(X(ky)+X(q.velocity[1]));
    q.velocity[2]=st(X(kz)+X(q.velocity[2]));
}
// Distance from a world point (three floats at PC address a) to the particle.
X distance_to(const P& p,std::uint32_t a,const Particle& q){
    const X dx=p.x(a)-X(q.position[0]),dy=p.x(a+4)-X(q.position[1]),dz=p.x(a+8)-X(q.position[2]);
    return hyp(dz,dy,dx);
}
// Sun light 1 diffuse (899C40) clamped to 1, as 41FF70 factors.
std::uint32_t sun_scaled(P& p,std::uint32_t colour){
    float r=p.f(0x899c40u),g=p.f(0x899c44u),b=p.f(0x899c48u);
    if(!c0c3(X(r),X(1.0f)))r=1.0f;
    if(!c0c3(X(g),X(1.0f)))g=1.0f;
    if(!c0c3(X(b),X(1.0f)))b=1.0f;
    return particles_colour_scale_41ff70(p.pc,colour,1.0f,r,g,b);
}
// rand()*k1*k2 truncated (the common born-time random).
std::uint32_t rand_ftol(P& p,std::uint32_t k1,std::uint32_t k2){
    const std::uint32_t r=p.rand();
    return ftol(X(std::int32_t(r))*X(fb(k1))*X(fb(k2)));
}
const std::uint32_t RandUnit=0x38000100u;   // 1/32767 (rand() to [0, 1])
// The quarter-texture cell of a new debris particle.
void random_cell(P& p,DebrisParticle& q){
    const X a=X(std::int32_t((p.rand()>>13)&3u))*X(0.25f);
    q.uv_min[0]=st(a);
    const X b=X(std::int32_t((p.rand()>>13)&3u))*X(0.25f);
    q.uv_min[1]=st(b);
    q.uv_max[0]=st(X(q.uv_min[0])+X(0.25f));
    q.uv_max[1]=st(b+X(0.25f));
}
void billboard_uv(Billboard& q){q.uv_max[1]=1.0f;q.uv_max[0]=1.0f;q.uv_min[1]=0;q.uv_min[0]=0;}
// Behind the camera plane: (p - eye) . (look - eye) < 0.
bool behind_camera(const P& p,std::uint32_t cam,const Particle& q){
    const X a=p.x(cam+0x104)-p.x(cam+0xf8),b=p.x(cam+0x108)-p.x(cam+0xfc),c=p.x(cam+0x10c)-p.x(cam+0x100);
    const X px=X(q.position[0])-p.x(cam+0xf8),py=X(q.position[1])-p.x(cam+0xfc),pz=X(q.position[2])-p.x(cam+0x100);
    return !jp5((pz*c+py*b)+px*a,X(0.0f));
}
// The alpha byte of the four corner colours.
void set_alpha(Billboard& q,std::uint32_t alpha24){for(auto& c:q.colour)c=(c&0xffffffu)|alpha24;}
// Rival texture handle pattern: [[r]+8] > limit ? [[r+24]]+off : 0.
std::uint32_t tex(P& p,std::uint32_t limit,std::uint32_t off){return p.texture(0x57,limit,off);}
// 449940 (render utility) with the current matrix: transform, then the
// perspective scale (SSE). Returns ST0 (unused by the callers).
void project_449940(P& p,float sx,float sy,std::uint32_t in,std::uint32_t out){
    const auto l=driving::pc_matrix_point(p.c.matrices,vec(p,in));
    if(!c0c3(X(fb(0x34000000u)),driving::x87_abs(X(l.z)))){   // eps > |z|
        p.putf(out+8,0.f);p.putf(out+4,0.f);p.putf(out,0.f);return;
    }
    const float inv=1.0f/(0.0f-l.z);
    p.putf(out,(inv*sx)*l.x);
    const float k=inv*sy;
    p.putf(out+4,l.y*k);
    p.putf(out+8,l.z);
}
// 4287B0(handle, matrix): the 427xxx object pool record 95E058 + handle*0x7C.
// The pool is the runtime's native sprite pool: its accesses go through the
// pool services (4287B0 matrix, 428880 status +24, 4285A0 release, inlined by 41D810).
void pool_matrix_4287b0(P& p,std::uint32_t handle,std::uint32_t matrix){
    if(std::int32_t(handle)<0)return;
    p.call(0x4287b0u,{handle,matrix});
}
}
// ---- spark (source 3, 8A8EA4) ------------------------------------------------
void spark_req_41d810(P& p){
    const std::uint32_t car=p.u(0x799d18u),cam=p.u(0x79f574u);
    const float k=fb(0x3c23d70au);
    std::uint32_t rec=0x93444cu;
    for(std::uint32_t ebx=0x92e81cu,ebp=0;ebx<0x92e84cu;ebx+=0xc,ebp+=4,rec+=0x5c){
        std::uint32_t edi=0;
        bool timer=false;
        if(p.u(rec+0x48)&&(p.u(car+0x2a8)&0x10f0020u)){
            const std::uint32_t c64=p.u(0x74ff64u);
            p.put(ebx-4,c64);p.put(ebx+4,c64);p.putx(ebx,p.x(0x74ff64u)*X(3.0f));
            edi=1;timer=true;
        }else{
            const std::uint32_t idx=p.u(ebp+0x74ff70u);
            if(p.u(car+idx*4u+0x514)||p.u(car+idx*4u+0x700)){
                const X v=p.x(0x74ff64u)*X(3.0f);
                p.put(ebx,p.u(0x74ff64u));p.putx(ebx-4,v);p.putx(ebx+4,v);
                timer=true;
            }
        }
        if(timer)p.put(ebp+0x9366c8u,p.u(0x74ff6cu));
        const std::int32_t h=p.i(ebp+0x8fa32cu);
        if(h>=0){
            // inlined: instance +24 (status) != 1 -> 4285A0 release (+0 = 0, layer count - 1, cursor = slot)
            if(p.call(0x428880u,{std::uint32_t(h)})!=1u){
                p.call(0x4285a0u,{std::uint32_t(h)});
                p.put(ebp+0x8fa32cu,0xffffffffu);
            }
        }
        if(!edi||p.i(ebp+0x8fa32cu)>=0)continue;
        if(c0c3(p.x(rec-4),X(k)))continue;
        if(p.u8(cam+0x34a)!=2u)continue;
        p.push_load(cam+0x140);
        // local vector (rec+0.. copy), y raised by 0.5 (front) / 0.75 (rear)
        std::array<std::uint8_t,12> lv{};
        const std::uint32_t L=0x7fff0200u;
        const std::size_t mark=p.m.mark();p.m.map(L,lv.data(),lv.size());
        try{
            p.put(L,p.u(rec));p.put(L+4,p.u(rec+4));p.put(L+8,p.u(rec+8));
            p.putx(L+4,p.x(L+4)+(ebx>=0x92e834u?X(fb(0x3f400000u)):X(0.5f)));
            project_449940(p,p.f(cam+0xb0),p.f(cam+0xb4),L,L);
            p.pop();
            if(!jp5(p.x(L+8),X(0.0f))){
                std::array<float,16> t{1,0,0,0,0,1,0,0,0,0,1,0,p.f(L),st(-p.x(L+4)),0,1};   // D3DXMatrixTranslation(x, -y, 0)
                std::array<std::uint8_t,64> tb{};std::memcpy(tb.data(),t.data(),64);
                const std::uint32_t T=0x7fff0240u;const std::size_t mk2=p.m.mark();p.m.map(T,tb.data(),64);
                try{
                    const std::uint32_t handle=p.call(0x428320u,{0x2c0001u,5,1});
                    p.put(ebp+0x8fa32cu,handle);
                    pool_matrix_4287b0(p,handle,T);
                }catch(...){p.m.release(mk2);throw;}
                p.m.release(mk2);
            }
        }catch(...){p.m.release(mark);throw;}
        p.m.release(mark);
    }
    std::int32_t n=p.i(0x74ff68u);
    std::uint32_t esi=0x934468u,ebp=0x92e81cu;
    for(std::uint32_t edi=0x9366c8u;edi<0x9366d8u;edi+=4,ebp+=0xc,esi+=0x5c){
        std::int32_t t=p.i(edi);
        if(t<=0)continue;
        --t;p.put(edi,std::uint32_t(t));if(t<0)p.put(edi,0);
        if(!jp5(p.x(esi-0x20),X(fb(0x3c23d70au))))continue;
        const std::uint32_t car2=car;
        const X c60=p.x(0x74ff60u);
        const X a28=c60*p.x(car2+0x28);
        const X a24=c60*p.x(car2+0x24)+p.x(0x74ff80u);
        const X a20=c60*p.x(car2+0x20);
        p.put(0x8a8edcu,p.u(ebp-4));p.putx(0x8a8ec4u,a20);
        p.put(0x8a8ee0u,p.u(ebp));p.putx(0x8a8ec8u,a24);
        p.put(0x8a8ee4u,p.u(ebp+4));p.putx(0x8a8eccu,a28);
        float l20=p.f(esi);
        const X d0=p.x(esi-0x1c)-p.x(esi-4);
        p.put(0x8a8f00u,esi-0x20);
        float l1c=p.f(esi-4);
        const X d1=p.x(esi-0x18)-p.x(esi);
        const X d2=p.x(esi-0x14)-p.x(esi+4);
        p.put(0x8a8ef0u,0xffffffffu);
        float l24=p.f(esi+4);
        float l30=st(d2);
        const float inv=st(X(1.0f)/X(p.i(0x74ff68u)));
        const float l28=st(d0*X(inv)),l2c=st(d1*X(inv));
        l30=st(X(l30)*X(inv));
        l20=st(X(l20)+X(fb(0x3f333333u)));
        if(n<=0)continue;
        for(std::int32_t ebx=0;;){
            p.putf(0x8a8eb8u,l1c);p.putf(0x8a8ebcu,l20);p.putf(0x8a8ec0u,l24);
            p.putf(0x8a8ed0u,k);p.putf(0x8a8ed4u,k);p.putf(0x8a8ed8u,k);
            born(p,3);
            l1c=st(X(l1c)+X(l28));
            n=p.i(0x74ff68u);++ebx;
            l20=st(X(l2c)+X(l20));
            l24=st(X(l24)+X(l30));
            if(!(ebx<n))break;
        }
    }
}
// 41DC60: a spark lives [74FF84, 74FF88) frames.
void spark_born_41dc60(P& p,std::uint32_t at,std::uint32_t source_at){
    auto& q=p.at<SparkParticle>(at);
    q.state=std::int32_t(0x80000000u);q.function=0x41dcc0u;
    q.colour=p.at<ParticleSource>(source_at).colour;q.scale=1.0f;
    const std::int32_t r=std::int32_t(p.rand());
    const std::int32_t lo=p.i(0x74ff84u),hi=p.i(0x74ff88u);
    q.life=std::int32_t(std::uint32_t(r)*std::uint32_t(hi-lo))/0x7fff+lo;   // the PC divides by 32767 with a magic multiply
}
// 41DCC0: drag on x/z, bounce on the road, gravity; the source texture
// blinks between two frames with the life parity.
void spark_ctrl_41dcc0(P& p,std::uint32_t at,std::uint32_t source_at){
    auto& q=p.at<SparkParticle>(at);
    const auto& r=road(p,p.at<ParticleSource>(source_at));
    advance(q);
    const X drag=p.x(0x74ff8cu);
    q.velocity[0]=st(drag*X(q.velocity[0]));
    q.velocity[2]=st(drag*X(q.velocity[2]));
    if(!jp5(plane(q,r),X(0.0f)))bounce(q,r);
    const std::int32_t life=q.life-1;
    q.life=life;
    q.velocity[1]=st(X(q.velocity[1])-X(Gravity));
    if(life<=0)kill(q);
    auto& spark=source(p,SourceSpark);
    spark.texture_list=0;spark.texture_count=0;
    spark.texture=(life%2)!=0?tex(p,0x13,0x4c):tex(p,0x14,0x50);
}
// ---- gravel (source 1, 8A8D9C) -----------------------------------------------
void gravel_req_41df00(P& p){
    p.put(0x8a8e6cu,sun_scaled(p,0xffffffffu));
    for(std::int32_t ebx=0;ebx<p.i(0x74ffa8u);++ebx){
        for(std::uint32_t esi=0x934508u;esi<0x9345c0u;esi+=0x5c){
            if(!(p.u(esi+0x38)&0x200010u))continue;
            const std::uint32_t rec=esi-8;
            float px=p.f(esi-4),py=p.f(esi),pz=p.f(esi+4);
            if(!jp5(p.x(rec),X(0.01)))continue;
            p.put(0x8a8df8u,rec);
            const X nx=p.x(esi+0x20)*X(-0.5f),ny=p.x(esi+0x24)*X(-0.5f);
            const float nz=st(p.x(esi+0x28)*X(-0.5f));
            px=st(X(px)+nx);p.putf(0x8a8db0u,px);
            const X yy=ny+X(py);
            pz=st(X(pz)+X(nz));
            py=st(yy+X(fb(0x3dcccccdu)));
            p.putf(0x8a8db4u,py);
            const X dx=p.x(esi-4)-p.x(esi+0x14);
            p.putf(0x8a8db8u,pz);
            const X dy=p.x(esi)-p.x(esi+0x18);
            p.put(0x8a8dc8u,0x3e19999au);
            const X dz=p.x(esi+4)-p.x(esi+0x1c);
            p.put(0x8a8dccu,0x3c23d70au);
            const float l30=st(dz);
            p.put(0x8a8dd0u,0x3e19999au);
            const float K=st(p.x(0x74ff90u)-p.x(0x74ff94u)*p.x(rec));
            p.putf(0x8a8dbcu,st(dx*X(K)));
            p.putf(0x8a8dc0u,st(dy*X(K)));
            p.putf(0x8a8dc4u,st(X(l30)*X(K)));
            const X w=(p.x(0x74ffa4u)*p.x(rec))*p.x(0x74ff9cu);
            p.putf(0x8a8dd4u,st(w*p.x(esi+0x20)));
            p.putf(0x8a8ddcu,st(w*p.x(esi+0x28)));
            p.putf(0x8a8dd8u,st(p.x(0x74ffa0u)*p.x(rec)+p.x(0x74ffa4u)));
            born(p,1);
        }
    }
}
// 41E190: gravel lives [74FFAC] + 30 - rand*30 frames.
void gravel_born_41e190(P& p,std::uint32_t at,std::uint32_t source_at){
    auto& q=p.at<DebrisParticle>(at);
    q.state=std::int32_t(0x80000000u);q.function=0x41e230u;
    q.colour=p.at<ParticleSource>(source_at).colour;q.scale=1.0f;
    q.life=std::int32_t(p.u(0x74ffacu)-rand_ftol(p,RandUnit,0xc1f00000u)+0x1eu);
    random_cell(p,q);
}
// 41E230: bounce, gravity; dies after its life or 15 m from the car.
void gravel_ctrl_41e230(P& p,std::uint32_t at,std::uint32_t source_at){
    auto& q=p.at<DebrisParticle>(at);
    const auto& r=road(p,p.at<ParticleSource>(source_at));
    const std::uint32_t car=p.u(PlayerCar);
    advance(q);
    if(!jp5(plane(q,r),X(0.0f)))bounce(q,r);
    q.velocity[1]=st(X(q.velocity[1])-X(Gravity));
    const std::int32_t life=q.life-1;
    q.life=life;
    if(life<=0){kill(q);return;}
    if(!c0c3(distance_to(p,car+0x14,q),X(15.0f)))kill(q);
}
// ---- grass (source 2, 8A8E20) ------------------------------------------------
void grass_req_41e440(P& p){
    const std::uint32_t car=p.u(0x799d18u);
    p.put(0x8a8e6cu,sun_scaled(p,0xffffffffu));
    const std::uint32_t c44=0x3cf5c28fu,c48=0,c4c=0x3cf5c28fu,c38=0x3cf5c28fu,c3c=0x3ca3d70au,c40=0x3cf5c28fu;
    (void)c38;(void)c3c;(void)c40;
    p.push_load(car+0xb0);
    std::uint32_t special=0;
    {const std::uint32_t f=p.u(car+0x2f0);
     if(f&2u){const std::uint32_t k=(f>>2)&0x1fu;
        if(k==2u||k==5u||k==0xfu||k==0xcu||k==0x11u||k==0x12u||k==0x13u||k==0xdu)special=1;}}
    for(std::int32_t n=0;n<p.i(0x74ffc4u);++n){
        for(std::uint32_t esi=0x934478u;esi<0x9345e8u;esi+=0x5c){
            const std::uint32_t rec=esi-0x30;
            if(!jp5(p.x(rec),X(0.01)))continue;
            if(!(p.u8(esi+0x10)&4u))continue;
            float l2c=p.f(esi-0x2c),l30=p.f(esi-0x28),l34=p.f(esi-0x24);
            const X y1=X(l30)+X(fb(0x3dcccccdu));
            X h=p.x(rec)+X(0.5f);
            p.put(0x8a8e4cu,c44);p.put(0x8a8e50u,c48);
            h=-h;
            const X hx=h*p.x(esi-8);
            const float hy=st(h*p.x(esi-4));
            const float hz=st(h*p.x(esi));
            l2c=st(X(l2c)+hx);p.putf(0x8a8e34u,l2c);
            l30=st(X(hy)+y1);p.putf(0x8a8e38u,l30);
            l34=st(X(hz)+X(l34));
            const X d0=p.x(esi-0x2c)-p.x(esi-0x14);
            const X d1=p.x(esi-0x28)-p.x(esi-0x10);
            p.putf(0x8a8e3cu,l34);
            const X d2=p.x(esi-0x24)-p.x(esi-0xc);
            p.put(0x8a8e54u,c4c);
            const float l28=st(d2);
            const float l20=st(d0*p.x(0x74ffb0u));
            const X m1=d1*p.x(0x74ffb0u);
            const float l28b=st(X(l28)*p.x(0x74ffb0u));
            X base=special?p.x(0x74ffc0u):(p.x(0x74ffb8u)*p.x(rec)+p.x(0x74ffbcu));
            p.putf(0x8a8e40u,l20);
            const float l24=st(base+m1);
            p.putf(0x8a8e44u,l24);
            p.putf(0x8a8e48u,l28b);
            const X q=p.x(0x74ffb4u)*p.x(rec);
            p.put(0x8a8e7cu,rec);
            const float l40=st(q);
            p.putf(0x8a8e58u,l40);
            p.putf(0x8a8e60u,l40);
            p.putf(0x8a8e5cu,st(p.x(0x74ffb4u)*X(l24)));
            born(p,2);
        }
    }
    p.pop();
}
// 41E780: grass lives 90 - rand*30 frames.
void grass_born_41e780(P& p,std::uint32_t at,std::uint32_t source_at){
    auto& q=p.at<DebrisParticle>(at);
    q.state=std::int32_t(0x80000000u);q.function=0x41e820u;
    q.colour=p.at<ParticleSource>(source_at).colour;q.scale=1.0f;
    q.life=std::int32_t(0x5au-rand_ftol(p,RandUnit,0xc1f00000u));
    random_cell(p,q);
}
// 41E820: damped motion; below the car it is pushed back up; dies under
// the road or 10 m from the car.
void grass_ctrl_41e820(P& p,std::uint32_t at,std::uint32_t source_at){
    auto& q=p.at<DebrisParticle>(at);
    const auto& r=road(p,p.at<ParticleSource>(source_at));
    const std::uint32_t car=p.u(PlayerCar);
    advance(q);
    const X k=p.x(0x74ffc8u);
    q.velocity[0]=st(k*X(q.velocity[0]));
    const X vy=(X(q.velocity[1])-X(Gravity))*k;
    q.velocity[1]=st(vy);
    q.velocity[2]=st(k*X(q.velocity[2]));
    if(!jp5(X(q.position[1]),p.x(car+0x18)))q.velocity[1]=st(driving::x87_abs(vy)-X(Gravity));
    if(!jp5(plane(q,r),X(0.0f))){kill(q);return;}
    if(!c0c3(distance_to(p,car+0x14,q),X(10.0f)))kill(q);
}
// ---- water (source 5, 8A8FAC) ------------------------------------------------
void water_req_41e9f0(P& p){
    const std::uint32_t car=p.u(0x799d18u);
    const std::uint32_t mask=0x400080u,v=p.u(0x74ffe8u);
    const float k=fb(0x3c23d70au);
    if(p.u(0x934488u)&mask)p.put(0x9154c8u,v);
    if(p.u(0x9344e4u)&mask)p.put(0x9154ccu,v);
    if(p.u(0x934540u)&mask)p.put(0x9154d0u,v);
    if(p.u(0x93459cu)&mask)p.put(0x9154d4u,v);
    p.put(0x8a8ff8u,p.u(0x74ffccu));
    p.put(0x8a9008u,0x934500u);
    if(p.u8(0x95af0cu)&3u)return;
    std::int32_t ecx=p.i(0x74ffe4u);
    for(std::uint32_t edx=2,ebp=0x934520u;edx<4;++edx,ebp+=0x5c){
        if(!jp5(p.x(ebp-0x20),X(0.01)))continue;
        if(p.i(edx*4u+0x9154c8u)>0){
            const float a20=st(p.x(0x74ffdcu)*p.x(car+0x20));
            const float a24=st(p.x(0x74ffdcu)*p.x(car+0x24)+p.x(0x95afb4u));
            const X w=p.x(0x74ffdcu)*p.x(car+0x28);
            float d0=st(p.x(ebp-0x1c)-p.x(ebp-4)),d1=st(p.x(ebp-0x18)-p.x(ebp)),d2=st(p.x(ebp-0x14)-p.x(ebp+4));
            const float c0=st(X(d1)*p.x(ebp-8)-X(d2)*p.x(ebp-0xc));
            const float c1=st(X(d2)*p.x(ebp-0x10)-X(d0)*p.x(ebp-8));
            const float c2=st(X(d0)*p.x(ebp-0xc)-X(d1)*p.x(ebp-0x10));
            X x0=X(c0),x2=X(c2);float l28=c1;
            const X len=driving::x87_sqrt((X(c0)*X(c0)+X(c1)*X(c1))+X(c2)*X(c2));
            if(!c0c3(len,X(1e-4))){
                const X f=p.x(0x74ffecu)/len;
                x0=x0*f;l28=st(X(l28)*f);x2=x2*f;
            }
            if(!(edx&1u)){x0=x0*X(-1.0f);l28=st(X(l28)*X(-1.0f));x2=x2*X(-1.0f);}
            const X s0=x0+X(a20);
            const X s1=X(l28)+X(a24);
            p.putf(0x8a8fd4u,st(x2+w));
            p.putx(0x8a8fccu,s0);
            p.put(0x8a8fe4u,p.u(0x74ffe0u));
            p.putx(0x8a8fd0u,s1);
            p.put(0x8a8fe8u,p.u(0x74ffe0u));p.put(0x8a8fecu,p.u(0x74ffe0u));
            float l30=p.f(ebp-4),l34=p.f(ebp),l38=p.f(ebp+4);
            const X inv=X(1.0f)/X(p.i(0x74ffe4u));
            d0=st(X(d0)*inv);d1=st(X(d1)*inv);d2=st(X(d2)*inv);
            l34=st(X(l34)+p.x(0x74fff0u));
            if(ecx>0){
                for(std::int32_t esi=0;;){
                    p.putf(0x8a8fc0u,l30);p.putf(0x8a8fc4u,l34);p.putf(0x8a8fc8u,l38);
                    p.putf(0x8a8fd8u,k);p.putf(0x8a8fdcu,k);p.putf(0x8a8fe0u,k);
                    born(p,5);
                    l30=st(X(l30)+X(d0));
                    ecx=p.i(0x74ffe4u);++esi;
                    l34=st(X(d1)+X(l34));
                    l38=st(X(l38)+X(d2));
                    if(!(esi<ecx))break;
                }
            }
        }
        const std::int32_t t=p.i(edx*4u+0x9154c8u)-1;
        p.put(edx*4u+0x9154c8u,std::uint32_t(t));
        if(t<0)p.put(edx*4u+0x9154c8u,0);
    }
}
// 41ED00: a water drop with the four colours 74FFF4.. (opaque) and life [750004] - rand*30.
void water_born_41ed00(P& p,std::uint32_t at){
    auto& q=p.at<WaterParticle>(at);
    q.state=std::int32_t(0x80000000u);q.function=0x41ed80u;
    for(unsigned k=0;k<4;++k)q.colour[k]=p.u(0x74fff4u+k*4)|0xff000000u;
    q.size=p.f(0x74ffd0u);
    const std::uint32_t v=rand_ftol(p,RandUnit,0xc1f00000u);
    q.uv_max[1]=1.0f;q.uv_max[0]=1.0f;
    q.life=std::int32_t(p.u(0x750004u)-v);
    q.uv_min[1]=0;q.uv_min[0]=0;
}
// 41ED80: drag, cleared under the road, gravity, grows with the car speed.
void water_ctrl_41ed80(P& p,std::uint32_t at,std::uint32_t source_at){
    auto& q=p.at<WaterParticle>(at);
    const auto& r=road(p,p.at<ParticleSource>(source_at));
    const std::uint32_t car=p.u(PlayerCar);
    advance(q);
    q.velocity[0]=st(p.x(0x750008u)*X(q.velocity[0]));
    q.velocity[2]=st(p.x(0x750008u)*X(q.velocity[2]));
    if(!jp5(plane(q,r),X(0.0f)))kill(q);
    q.velocity[1]=st(X(q.velocity[1])-X(Gravity));
    const X growth=p.x(0x74ffd4u)*p.x(car+0x1c4);
    const std::int32_t life=q.life-1;
    q.size=st(growth+X(q.size));
    q.life=life;
    if(life<=0)kill(q);
}
// ---- misc (source 6, 8A9030) -------------------------------------------------
namespace {
// 40F2C0 (EAX = a, ESI = b, EDX = c, ECX = out): normalized (c-a) x (a-b).
std::uint32_t normal_40f2c0(P& p,std::uint32_t a,std::uint32_t b,std::uint32_t c,std::uint32_t out){
    const X a0=p.x(a)-p.x(b),a1=p.x(a+4)-p.x(b+4),a2=p.x(a+8)-p.x(b+8);
    const float b0=st(p.x(c)-p.x(a)),b1=st(p.x(c+4)-p.x(a+4));
    const X b2=p.x(c+8)-p.x(a+8);
    p.putx(out,b2*a1-X(b1)*a2);
    p.putx(out+4,X(b0)*a2-b2*a0);
    p.putx(out+8,X(b1)*a0-X(b0)*a1);
    const X x=p.x(out),y=p.x(out+4),z=p.x(out+8);
    const X s=(x*x+y*y)+z*z;
    if(s==X(0.0f))return 0;                                   // FUCOMPP equal (ordered)
    const X inv=X(1.0f)/driving::x87_sqrt(s);
    p.putx(out,inv*p.x(out));p.putx(out+4,inv*p.x(out+4));p.putx(out+8,inv*p.x(out+8));
    return 1;
}
}
void misc_req_41ef00(P& p){
    const std::uint32_t cam=p.u(0x79f574u),car=p.u(0x799d18u);
    std::array<std::uint8_t,0xd0> frame{};                     // the PC frame F+0x00..F+0xC4
    const std::uint32_t F=0x7fff1000u;
    const std::size_t mark=p.m.mark();p.m.map(F,frame.data(),frame.size());
    try{
        p.put(F+0x54,0);p.put(F+0x58,0);p.put(F+0x5c,0);
        std::uint32_t esi=p.u16(p.u(0x799b38u+8u*0x3cu)+0x64);          // 4503A0(8)
        const std::uint32_t ebp=p.u(car+0x1c0);
        const std::uint16_t own=p.u16(car+0x18c);
        if(!p.u(0x750054u)){p.m.release(mark);return;}
        const std::uint32_t course=p.u(p.u(0x799b38u+8u*0x3cu)+0x68);
        std::uint32_t z;{const std::uint32_t r=race_area_record_44c8d0(p.m,course);z=r?p.u(p.u(r+0x14)):p.u(p.u(0x7d2df4u));}
        if(z!=7u){p.m.release(mark);return;}
        const std::int32_t div=p.i(0x750038u);
        if(std::int32_t(esi&0xffffu)%div!=0){p.m.release(mark);return;}
        if(std::uint16_t(esi)<=own){p.m.release(mark);return;}
        if(p.u(car+0x5c)){p.m.release(mark);return;}
        p.put(F+0x44,0);p.put(F+0x48,0);
        esi+=p.u(0x750034u);
        std::uint16_t length=0;                                 // 43D470(0)
        if(const std::uint32_t t=p.u(0x780140u)){const std::uint32_t n=p.u(t+0xc)-1u;if(n!=0xffffffffu)length=p.u16(p.u(0x780228u)+n*2u);}
        if(std::uint16_t(esi)>=length){p.m.release(mark);return;}
        p.put16(F+0x4c,std::uint16_t(esi));
        p.call(0x43e3b0u,{F+0x60,F+0x44,ebp});
        p.put(F+0x14,p.u(F+0x6c));
        const float y=st(p.x(0x750040u)+p.x(F+0x14));
        p.put(F+0x2c,p.u(F+0x68));p.put(F+0x10,p.u(F+0x68));
        p.putf(F+0x14,y);
        p.put(F+0x30,p.u(F+0x6c));
        --esi;
        p.put(F+0x34,p.u(F+0x70));p.put(F+0x18,p.u(F+0x70));
        p.put16(F+0x4c,std::uint16_t(esi));
        p.call(0x43e3b0u,{F+0x60,F+0x44,ebp});
        p.put(F+0x20,p.u(F+0x68));p.put(F+0x24,p.u(F+0x6c));p.put(F+0x28,p.u(F+0x70));
        normal_40f2c0(p,F+0x10,F+0x2c,F+0x20,F+0x38);
        const std::uint32_t r=p.rand()&0x7fffu;
        const float s=st(((X(std::int32_t(r))*X(fb(0x38000000u)))-X(0.5f))*p.x(0x75003cu));
        for(unsigned k=0;k<3;++k)p.putx(F+0x38+k*4,X(s)*p.x(F+0x38+k*4));   // 40F050
        p.putx(F+0x10,p.x(F+0x38)+p.x(F+0x10));
        p.putx(F+0x14,p.x(F+0x3c)+p.x(F+0x14));
        p.putx(F+0x18,p.x(F+0x40)+p.x(F+0x18));
        for(unsigned k=0;k<3;++k)p.putx(F+0x20+k*4,p.x(cam+0x104+k*4)-p.x(cam+0xf8+k*4));   // 40EFA0
        for(unsigned k=0;k<3;++k)p.putx(F+0x2c+k*4,p.x(F+0x10+k*4)-p.x(cam+0xf8+k*4));
        const X d=(p.x(F+0x34)*p.x(F+0x28)+p.x(F+0x30)*p.x(F+0x24))+p.x(F+0x2c)*p.x(F+0x20);
        if(!jp5(d,X(0.0f))){p.m.release(mark);return;}
        {   const X dx=p.x(cam+0xd4)-p.x(F+0x10),dy=p.x(cam+0xd8)-p.x(F+0x14),dz=p.x(cam+0xdc)-p.x(F+0x18);  // 40F140
            if(!c0c3(hyp(dz,dx,dy),p.x(0x75002cu))){p.m.release(mark);return;}}
        const std::uint32_t w=particles_get_work_418420(p.pc,6);
        if(!w){p.m.release(mark);return;}
        p.copy(0x8a8d2cu+6u*0x84u,F+0x10,3);p.copy(0x8a8d44u+6u*0x84u,F+0x54,3);    // 418230
        p.copy(0x8a8d38u+6u*0x84u,0x95afb8u,3);p.copy(0x8a8d50u+6u*0x84u,0x75001cu,3);  // 418270
        p.put(0x8a903cu,0x41f180u);
        particles_born_418350(p.pc,w);
    }catch(...){p.m.release(mark);throw;}
    p.m.release(mark);
}
// 41F180: a sunlit cloud keeping its birth position and camera distance.
void misc_born_41f180(P& p,std::uint32_t at){
    auto& q=p.at<MiscParticle>(at);
    const std::uint32_t cam=p.u(Camera);
    q.state=std::int32_t(0x80000000u);q.function=0x41f2f0u;q.size=p.f(0x750044u);
    const std::uint32_t n=p.u(0x750030u)-rand_ftol(p,RandUnit,0xc2b40000u);
    q.life=n;q.life_start=n;
    for(unsigned k=0;k<4;++k)q.colour[k]=sun_scaled(p,p.u(0x75000cu+k*4))|0xff000000u;
    billboard_uv(q);
    for(unsigned k=0;k<3;++k)q.start[k]=q.position[k];
    const X dx=p.x(cam+0xf8)-X(q.position[0]),dy=p.x(cam+0xfc)-X(q.position[1]),dz=p.x(cam+0x100)-X(q.position[2]);
    q.start_distance=st(hyp(dz,dx,dy));
}
// 41F2F0: grows; cleared behind the camera, too far or at the end of its
// life; fades in with the distance travelled towards the camera.
void misc_ctrl_41f2f0(P& p,std::uint32_t at){
    auto& q=p.at<MiscParticle>(at);
    const std::uint32_t cam=p.u(Camera);
    q.size=st(p.x(0x750028u)+X(q.size));
    advance(q);
    if(behind_camera(p,cam,q))kill(q);
    if(!c0c3(distance_to(p,cam+0xd4,q),p.x(0x75002cu)))kill(q);
    if(!--q.life)kill(q);
    const X dx=p.x(cam+0xf8)-X(q.position[0]),dy=p.x(cam+0xfc)-X(q.position[1]),dz=p.x(cam+0x100)-X(q.position[2]);
    const X d=hyp(dz,dy,dx);
    const X d0=X(q.start_distance);
    X v;
    if(jp5(d,p.x(0x750050u)))v=(d0-d)/(d0-p.x(0x750050u));
    else v=d/p.x(0x750050u);
    v=v*p.x(0x75004cu);
    if(!c0c3(v,p.x(0x750048u)))v=p.x(0x750048u);
    if(!c0c3(v,X(1.0f)))v=X(1.0f);
    set_alpha(q,ftol(v*X(255.0f))<<24);
}
// ---- backfire (source 7, 8A90B4) ---------------------------------------------
void backfire_req_41f5f0(P& p){
    const std::uint32_t car=p.u(0x799d18u);
    p.put(0x8a9100u,p.u(0x750058u));
    for(std::uint32_t a:{0x8a90d4u,0x8a90d8u,0x8a90dcu,0x8a90ecu,0x8a90f0u,0x8a90f4u})p.put(a,0);
    p.push_load(car+0xb0);
    bool done=false;
    if(p.u8(0x915240u)==1u){
        const std::uint32_t v=ftol(p.x(p.u(car+0x2b4)+0x1644)*X(fb(0xc118c9ebu)));
        const std::uint32_t limit=0xfffffc18u-v;
        if(p.u(0x91523cu)>limit&&p.u(0x915238u)>=0xffu){
            for(unsigned n=0;n<2;++n)
                for(std::uint32_t fn:{0x41f7f0u,0x41f8f0u,0x41f870u,0x41f970u}){p.put(0x8a90c0u,fn);born(p,7);}
            p.call(0x424940u,{0x51});
            done=true;
        }
    }
    if(!done&&p.u(0x7f9460u+0x60u)&&!c0c3(p.x(0x800ad0u),X(0.0f))&&(p.rand()&3u)){
        if(p.rand()&1u){p.put(0x8a90c0u,0x41f970u);born(p,7);p.put(0x8a90c0u,0x41f8f0u);}
        else{p.put(0x8a90c0u,0x41f8f0u);born(p,7);p.put(0x8a90c0u,0x41f970u);}
        born(p,7);
    }
    p.pop();
}
// 41F7F0 / 41F870 / 41F8F0 / 41F970: a backfire flame of exhaust `side`,
// mirrored or not, living 8 - rand*10 frames.
void backfire_born(P& p,std::uint32_t at,std::uint32_t side,std::uint32_t flip){
    auto& q=p.at<BackfireParticle>(at);
    q.state=std::int32_t(0x80000000u);q.function=0x41f9f0u;
    for(unsigned k=0;k<4;++k)q.colour[k]=p.u(0x750060u+k*4);
    q.size=p.f(0x75005cu);
    q.side=side;q.flip=flip;
    const std::uint32_t r=p.rand()&0x7fffu;
    q.life=std::int32_t(8u-ftol(X(std::int32_t(r))*X(fb(0x38000000u))*X(-10.0f)));
}
// 41F9F0: placed at the exhaust (table 6238C8 by side and car model) under
// the car matrix, hidden while life > 8, scrolling through the texture.
void backfire_ctrl_41f9f0(P& p,std::uint32_t at){
    auto& q=p.at<BackfireParticle>(at);
    const std::uint32_t car=p.u(PlayerCar);
    const std::uint32_t entry=((q.side+std::uint32_t(std::int32_t(std::int8_t(p.u8(car+0x11))))*2u)*3u)*4u+0x6238c8u;
    CourseProbe exhaust{p.f(entry),p.f(entry+4),p.f(entry+8)};
    q.state=q.life>8?(q.state&0x7fffffff):std::int32_t(std::uint32_t(q.state)|0x80000000u);
    p.push_load(car+0xb0);
    driving::pc_matrix_multiply_current(p.c.matrices,p.m.bytes(car+0xf0,64));
    if(q.flip)exhaust.x=st(-X(exhaust.x));
    const auto w=driving::pc_matrix_point(p.c.matrices,exhaust);
    q.position[0]=w.x;q.position[1]=w.y;q.position[2]=w.z;
    p.pop();
    const std::int32_t n=q.life;
    q.uv_max[0]=st(X(n)*X(0.125f));
    q.uv_max[1]=1.0f;q.uv_min[1]=0;q.life=n-1;
    q.uv_min[0]=st(X(n-1)*X(0.125f));
    if(n-1<=0)kill(q);
}
// ---- tire marks (41FB50, 41FDD0, 4862C0) -------------------------------------
namespace {
// 4862C0(vertices, kind): appends 6 vertices (0x90 bytes) to the ring of
// tire-mark kind at 64FE1C + kind*0x14 (+0 index, +8 count, +C buffer). The
// function-local static 82E7B8 = 40ECB0() % 3 ([95AF0C], measured) is
// initialized on the first call (guard bit 1 of 82E7BC).
void tire_mark_add_4862c0(P& p,std::uint32_t vertices,std::uint32_t kind){
    if(!(p.u8(0x82e7bcu)&1u)){p.put(0x82e7bcu,p.u(0x82e7bcu)|1u);p.put(0x82e7b8u,p.u(0x95af0cu)%3u);}
    const std::uint32_t e=0x64fe1cu+kind*0x14u;
    const std::uint32_t at=p.u(e)*0x90u+p.u(e+0xc);
    p.copy(at,vertices,0x24);
    const std::uint32_t n=p.u(e)+1u;p.put(e,n);
    if(std::int32_t(n)>=p.i(e+8))p.put(e,0);
}
// 41FDD0 (EDI = new edge (2 points), ESI = previous edge): one quad; returns
// the fractional texture coordinate in ST0.
X tire_mark_quad_41fdd0(P& p,std::uint32_t edi,std::uint32_t esi,float a0,float a1,float a2,std::uint32_t kind){
    const X d0=p.x(edi)-p.x(esi),d1=p.x(edi+4)-p.x(esi+4),d2=p.x(edi+8)-p.x(esi+8);
    const X len=driving::x87_sqrt((d0*d0+d2*d2)+d1*d1);
    const X u=len*X(0.5f)+X(a0);
    const float uf=st(u);
    const std::uint32_t c1=(ftol(X(a1)*X(255.0f))<<24)|0xffffffu;
    const std::uint32_t c2=(ftol(X(a2)*X(255.0f))<<24)|0xffffffu;
    const X h=p.x(0x750078u);
    std::array<std::uint8_t,0x90> v{};
    const std::uint32_t V=0x7fff0400u;const std::size_t mark=p.m.mark();p.m.map(V,v.data(),v.size());
    try{
        const float nx=p.f(edi),ny=st(h+p.x(edi+4)),nz=p.f(edi+8);
        const float n2x=p.f(edi+0xc),n2y=st(h+p.x(edi+0x10)),n2z=p.f(edi+0x14);
        const float o2x=p.f(esi+0xc),o2y=st(h+p.x(esi+0x10)),o2z=p.f(esi+0x14);
        const float ox=p.f(esi),oy=st(h+p.x(esi+4)),oz=p.f(esi+8);
        auto vert=[&](unsigned k,float x,float y,float z,std::uint32_t c,float s,float t){
            const std::uint32_t a=V+k*0x18u;p.putf(a,x);p.putf(a+4,y);p.putf(a+8,z);p.put(a+0xc,c);p.putf(a+0x10,s);p.putf(a+0x14,t);};
        vert(0,nx,ny,nz,c1,0.f,a0);
        vert(1,n2x,n2y,n2z,c1,1.f,a0);
        vert(2,o2x,o2y,o2z,c2,1.f,uf);
        vert(3,nx,ny,nz,c1,0.f,a0);
        vert(4,o2x,o2y,o2z,c2,1.f,uf);
        vert(5,ox,oy,oz,c2,0.f,uf);
        tire_mark_add_4862c0(p,V,kind);
    }catch(...){p.m.release(mark);throw;}
    p.m.release(mark);
    const X uu=X(uf);
    return uu-X(std::int32_t(ftol(uu)));
}
}
// 41FB50: the player's tire marks. Each tire's new edge is the contact point
// +- half the tire width across the direction of travel; a strip opens, grows
// (41FDD0 quad) or closes with the surface kind of the tire.
void tire_mark_req_41fb50(P& p){
    const std::uint32_t car=p.u(0x799d18u);
    TireRecord* tires=p.array<TireRecord>(0x934448u,4);
    TireMark* marks=p.array<TireMark>(0x9174d8u,4);
    for(std::uint32_t tire=0;tire<4;++tire){
        const auto& r=tires[tire];
        auto& mk=marks[tire];
        const std::uint32_t edge=0x9174d8u+tire*0x28u+0xcu;              // &mk.left (41FDD0 argument)
        const X a0=X(mk.left[0])-X(r.pos[0]),a1=X(mk.left[1])-X(r.pos[1]),a2=X(mk.left[2])-X(r.pos[2]);
        const X moved=driving::x87_sqrt((a1*a1+a2*a2)+a0*a0);
        const X b0=X(r.pos[0])-X(r.prev_pos[0]),b1=X(r.pos[1])-X(r.prev_pos[1]),b2=X(r.pos[2])-X(r.prev_pos[2]);
        // across = travel x normal
        const float c0=st(b1*X(r.normal[2])-b2*X(r.normal[1]));
        const float c1=st(b2*X(r.normal[0])-b0*X(r.normal[2]));
        const float c2=st(b0*X(r.normal[1])-b1*X(r.normal[0]));
        float l24=c2;
        X x0=X(c0),x1=X(c1);
        const X lc=driving::x87_sqrt((X(c2)*X(c2)+X(c1)*X(c1))+X(c0)*X(c0));
        if(!c0c3(lc,X(1e-4))){
            const X f=(p.x(p.u(car+0x2b4)+std::uint32_t((std::int32_t(tire)>>1)+0x28)*0x4cu)*X(0.5f))/lc;   // half the tire width
            x0=x0*f;x1=x1*f;l24=st(X(l24)*f);
        }
        std::array<float,6> pts{st(x0+X(r.pos[0])),st(x1+X(r.pos[1])),st(X(l24)+X(r.pos[2])),
                                st(X(r.pos[0])-x0),st(X(r.pos[1])-x1),st(X(r.pos[2])-X(l24))};
        if(c0c3(moved,p.x(0x750070u)))continue;
        const std::uint32_t f=r.state;
        std::uint32_t laying=0,kind=0;
        if(f&0xc02u)laying=(1u<<tire)&p.u(0x95afa8u);
        else if(f&0x8004u){laying=1;kind=1;}
        else if(f&0x18u){kind=2;laying=1;}
        std::array<std::uint8_t,0x18> np{};std::memcpy(np.data(),pts.data(),0x18);
        const std::uint32_t N=0x7fff0500u;const std::size_t mark=p.m.mark();p.m.map(N,np.data(),np.size());
        X out=moved;
        try{
            if((p.u(car+4)&0xc000u)&&laying&&kind==mk.kind){
                if(mk.active){
                    out=tire_mark_quad_41fdd0(p,N,edge,mk.length,p.f(0x750074u),mk.v,mk.kind);
                    {const std::uint32_t v=p.u(0x750074u);std::memcpy(&mk.v,&v,4);}
                }else{mk.active=1;mk.v=0.0f;}
            }else{
                if(mk.active)out=tire_mark_quad_41fdd0(p,N,edge,mk.length,0.f,mk.v,mk.kind);
                mk.active=0;mk.v=0.0f;
            }
        }catch(...){p.m.release(mark);throw;}
        p.m.release(mark);
        mk.length=st(out);
        for(unsigned k=0;k<3;++k){mk.left[k]=pts[k];mk.right[k]=pts[3+k];}
        mk.kind=kind;
    }
    p.put(0x95afa8u,0);
}
// ---- sources 8..10 (texture-change particles) --------------------------------
// 420110: life [75007C] + 30 - rand*30, period 1 - rand/512 (0 or 1).
void texchg_born_420110(P& p,std::uint32_t at){
    auto& q=p.at<SpriteParticle>(at);
    q.state=std::int32_t(0x80000000u);q.function=0x420180u;
    q.life=std::int32_t(p.u(0x75007cu)-rand_ftol(p,RandUnit,0xc1f00000u)+0x1eu);
    const std::uint32_t r=p.rand();
    const std::uint32_t v=ftol(X(std::int32_t(r))*X(fb(0xb9800100u)));
    q.frame=0;q.period=std::int16_t(1u-v);
}
// 420180: next texture frame every `period` frames (back to 0 at 6); dies
// after its life or below the reference height (source +5C record, +04).
void texchg_ctrl_420180(P& p,std::uint32_t at,std::uint32_t source_at){
    auto& q=p.at<SpriteParticle>(at);
    const std::uint32_t reference=p.at<ParticleSource>(source_at).plane;
    advance(q);
    const std::int32_t life=q.life,period=q.period;
    if(!period)throw std::domain_error("particles: 420180 divide by zero (the PC faults)");
    if(life%period==0){q.frame=std::int16_t(q.frame+1);if(q.frame>=6)q.frame=0;}
    q.life=life-1;
    if(life-1<=0||!jp5(X(q.position[1]),p.x(reference+4)))kill(q);
}
// 420350: life 130 - rand*30, period 4 - rand/512, a random first frame.
void texchg_born_420350(P& p,std::uint32_t at){
    auto& q=p.at<SpriteParticle>(at);
    q.state=std::int32_t(0x80000000u);q.function=0x4203e0u;
    q.life=std::int32_t(0x82u-rand_ftol(p,RandUnit,0xc1f00000u));
    {const std::uint32_t r=p.rand();q.period=std::int16_t(4u-ftol(X(std::int32_t(r))*X(fb(0xb9800100u))));}
    const std::uint32_t r=p.rand();
    const std::int16_t first=std::int16_t(ftol(X(std::int32_t(r))*X(fb(0x3a900120u))));
    q.frame=first;
    if(first>=0x18)q.frame=std::int16_t(std::int32_t(first)%6);
}
// 4203E0: as 420180, the frame stepping back by 6 when it reaches a multiple of 6.
void texchg_ctrl_4203e0(P& p,std::uint32_t at,std::uint32_t source_at){
    auto& q=p.at<SpriteParticle>(at);
    const std::uint32_t reference=p.at<ParticleSource>(source_at).plane;
    advance(q);
    const std::int32_t life=q.life,period=q.period;
    if(!period)throw std::domain_error("particles: 4203E0 divide by zero (the PC faults)");
    if(life%period==0){
        const std::int16_t next=std::int16_t(q.frame+1);q.frame=next;
        if(std::int32_t(next)%6==0)q.frame=std::int16_t(next-6);
    }
    q.life=life-1;
    if(life-1<=0||!jp5(X(q.position[1]),p.x(reference+4)))kill(q);
}
// 4207B0 (the arcade ending pieces): life 130 - rand*30, period 1 - rand/256.
void texchg_born_4207b0(P& p,std::uint32_t at){
    auto& q=p.at<SpriteParticle>(at);
    q.state=std::int32_t(0x80000000u);q.function=0x420820u;
    q.life=std::int32_t(0x82u-rand_ftol(p,RandUnit,0xc1f00000u));
    const std::uint32_t r=p.rand();
    const std::uint32_t v=ftol(X(std::int32_t(r))*X(fb(0xb8800100u)));
    q.frame=0;q.period=std::int16_t(1u-v);
}
// 420820: the frame advances by `period` every control, modulo 6.
void texchg_ctrl_420820(P& p,std::uint32_t at,std::uint32_t source_at){
    auto& q=p.at<SpriteParticle>(at);
    const std::uint32_t reference=p.at<ParticleSource>(source_at).plane;
    advance(q);
    std::int16_t frame=std::int16_t(q.frame+q.period);q.frame=frame;
    if(frame>=6){do{frame=std::int16_t(frame-6);}while(frame>=6);q.frame=frame;}
    const std::int32_t life=q.life-1;q.life=life;
    if(life<=0||!jp5(X(q.position[1]),p.x(reference+4)))kill(q);
}
// ---- 4208A0 the AUTOSCENE smoke (node token BD0020) ------------------------------
// 4209D0 born: life 5 + rand * [62827C] * [74FE80], alpha scale [74FED7] * [6280BC], colours
// 74FED4..74FEE0, +34 / +38 = 1.0.
void smoke_born_4209d0(P& p,std::uint32_t at){
    auto& q=p.at<SmokeParticle>(at);
    q.state=std::int32_t(0x80000000u);q.function=0x420a70u;q.size=p.f(0x74fe88u);
    const std::uint32_t r=p.rand();
    const std::uint32_t life=ftol(X(std::int32_t(r))*X(fb(RandUnit))*p.x(0x74fe80u))+5u;
    q.life=life;q.life_start=life;
    q.alpha=bf(st(X(std::int32_t(p.u8(0x74fed7u)))*X(fb(0x3b808081u))));     // byte / 255 as a float
    for(unsigned k=0;k<4;++k)q.colour[k]=p.u(0x74fed4u+k*4);
    billboard_uv(q);
}
// 420A70 ctrl: move, life - 1 (0: the particle is cleared), alpha = min(life * +50 / +48, 1) *
// [5A91B8] into the four colour words.
void smoke_ctrl_420a70(P& p,std::uint32_t at){
    auto& q=p.at<SmokeParticle>(at);
    advance(q);
    const std::int32_t life=std::int32_t(q.life)-1;q.life=std::uint32_t(life);
    if(!(life>0))kill(q);
    X a=X(life)*X(fb(q.alpha))/X(std::int32_t(q.life_start));
    if(a>X(1.0f))a=X(1.0f);                                      // 62806C; fcom / test ah,41: unordered keeps it
    set_alpha(q,ftol(a*X(255.0f))<<24);
}
// ---- 4160F0 CtrlGlowColor ----------------------------------------------------
void glow_colour_4160f0(P& p){
    const std::uint32_t mode=p.u(0x78026cu),car=p.u(0x799d18u);
    auto table=[](std::uint32_t v){return std::uint32_t(std::int32_t(v)%0x42)*0x44u+0x622528u;};
    auto course_value=[&](std::uint32_t id){const std::uint32_t r=race_area_record_44c8d0(p.m,id);return r?p.u(p.u(r+0x14)):p.u(p.u(0x7d2df4u));};
    std::uint32_t src;X t;
    if(mode==0x18u){
        const std::int32_t s=p.i(0x638e9cu);const std::int32_t e=s>4?s:-1;
        t=X(1.0f);
        if(e<=-1||e>=0xa)src=0x622924u;
        else{static constexpr std::uint32_t Pick[5]={0x1d,0x19,0x1b,0x1c,0x1a};
            const std::uint32_t k=std::uint32_t(e-5);src=table(k>4u?0xfu:Pick[k]);}
    }else{
        const std::uint32_t route=p.u(car+0x5c);
        if(route==0u){src=table(course_value(p.u(p.u(0x799b38u+8u*0x3cu)+0x68)));t=X(1.0f);}
        else if(route==1u){
            std::uint16_t length=0;
            if(const std::uint32_t q=p.u(0x780144u)){const std::uint32_t n=p.u(q+0xc)-1u;if(n!=0xffffffffu)length=p.u16(p.u(0x78022cu)+n*2u);}
            const float half=st(X(std::int32_t(length))*X(0.5f));
            const float pos=st(X(std::int32_t(p.u16(p.u(0x799b38u+8u*0x3cu)+0x64))));
            if(jp5(X(pos),X(half))){
                const std::uint32_t area=p.u(0x7d3188u)?p.u(p.u(0x7d3188u)+4):0xfu;       // 44C830
                src=table(course_value(area));
                t=(X(pos)-X(half))/X(half);
            }else{
                src=table(course_value(p.u(p.u(0x799b38u+8u*0x3cu)+0x68)));
                t=X(1.0f)-X(pos)/X(half);
            }
        }else throw std::logic_error("particles: 4160F0 reads an uninitialized PC stack word (car+5C not 0/1)");
    }
    p.copy(0x8a8c18u,src,0x11);                    // relocated snippet 448486: ECX = [1039EA8] = 0x11
    p.putx(0x8a8c30u,(p.x(0x8a8c30u)-X(4.0f))*t+X(4.0f));
    p.putx(0x8a8c20u,(p.x(0x8a8c20u)-X(4.0f))*t+X(4.0f));
}
// ---- function pointer dispatch -------------------------------------------------
bool effect_callback(P& p,std::uint32_t pc,std::uint32_t a0,std::uint32_t a1){
    switch(pc){
    case 0x41d1e0u:smoke_born_41d1e0(p,a0);return true;
    case 0x41d270u:smoke_born_41d270(p,a0);return true;
    case 0x41d330u:smoke_ctrl_41d330(p,a0,a1);return true;
    case 0x41d5c0u:smoke_ctrl_41d5c0(p,a0);return true;
    case 0x41dc60u:spark_born_41dc60(p,a0,a1);return true;
    case 0x41dcc0u:spark_ctrl_41dcc0(p,a0,a1);return true;
    case 0x41e190u:gravel_born_41e190(p,a0,a1);return true;
    case 0x41e230u:gravel_ctrl_41e230(p,a0,a1);return true;
    case 0x41e780u:grass_born_41e780(p,a0,a1);return true;
    case 0x41e820u:grass_ctrl_41e820(p,a0,a1);return true;
    case 0x41ed00u:water_born_41ed00(p,a0);return true;
    case 0x41ed80u:water_ctrl_41ed80(p,a0,a1);return true;
    case 0x41f180u:misc_born_41f180(p,a0);return true;
    case 0x41f2f0u:misc_ctrl_41f2f0(p,a0);return true;
    case 0x41f7f0u:backfire_born(p,a0,0,0);return true;
    case 0x41f870u:backfire_born(p,a0,0,1);return true;
    case 0x41f8f0u:backfire_born(p,a0,1,0);return true;
    case 0x41f970u:backfire_born(p,a0,1,1);return true;
    case 0x41f9f0u:backfire_ctrl_41f9f0(p,a0);return true;
    case 0x420110u:texchg_born_420110(p,a0);return true;
    case 0x420180u:texchg_ctrl_420180(p,a0,a1);return true;
    case 0x420350u:texchg_born_420350(p,a0);return true;
    case 0x4203e0u:texchg_ctrl_4203e0(p,a0,a1);return true;
    case 0x4207b0u:texchg_born_4207b0(p,a0);return true;
    case 0x420820u:texchg_ctrl_420820(p,a0,a1);return true;
    case 0x4209d0u:smoke_born_4209d0(p,a0);return true;
    case 0x420a70u:smoke_ctrl_420a70(p,a0);return true;
    default:return false;
    }
}
}
namespace outrun::platform {
// 4208A0(count, &pos, colour): colour words 74FED4..74FEE0, the emitter globals 8A8F34.. (born
// 4209D0, spreads 0.01 / 0.04, life 7FFF, the time [74FE88]) and count particles of type 4.
void particles_smoke_emit_4208a0(PcParticleContext& pc,std::uint32_t count,std::uint32_t x,std::uint32_t y,std::uint32_t z,std::uint32_t colour){
    using namespace particles_detail;
    P p(pc);
    for(std::uint32_t k=0;k<0x10u;k+=4)p.put(0x74fed4u+k,colour);
    const std::uint32_t t=p.u(0x74fe88u);p.put(0x8a8f6cu,t);p.put(0x8a8f70u,t);
    p.put(0x8a8f48u,0);p.put(0x8a8f4cu,0x3c23d70au);p.put(0x8a8f50u,0);p.put(0x8a8f60u,0x3d23d70au);
    p.put(0x8a8f64u,0x3c23d70au);p.put(0x8a8f68u,0x3d23d70au);p.put(0x8a8f38u,0x7fffu);
    p.put(0x8a8f3cu,x);p.put(0x8a8f40u,y);p.put(0x8a8f44u,z);
    p.put(0x8a8f54u,0x3c23d70au);p.put(0x8a8f58u,0x3c23d70au);p.put(0x8a8f5cu,0x3c23d70au);
    p.put(0x8a8f34u,0x4209d0u);
    for(std::int32_t n=std::int32_t(count);n>0;--n)particles_detail::born(p,4);
}
}
namespace outrun::platform {
// 420560 (the arcade ending's falling pieces, AUTOSCENE event rows): with event 0x18D running,
// outside mode 24 the six texture words 9406D8.. are cleared; in mode 24 they are resolved once
// from 623B98 (bank 0xD0 objects 0xB..0x10, 448810: handle or 0), the camera's inverse view +1C0
// is pushed and twelve type-10 particles are born at (0, 1.5 - rand * [62827C] * 2.5, -3)
// (D3DXVec3TransformCoord) moving (0, -0.005, 0) (D3DXVec3TransformNormal).
void particles_flare_420560(PcParticleContext& pc){
    using namespace particles_detail;
    P p(pc);
    if((p.u8(0x79fcd5u)&3u)!=2u)return;
    const std::uint32_t cam=p.u(0x79f574u);
    if(p.u(0x78026cu)!=0x18u){
        if(p.u(0x9406d8u)==0u)return;
        for(std::uint32_t a=0x9406d8u;a<=0x9406ecu;a+=4)p.put(a,0);
        return;
    }
    if(p.u(0x9406d8u)==0u)
        for(std::uint32_t i=0;i<0x18u;i+=4){
            const std::uint32_t token=p.u(0x623b98u+i),index=token&0xffffu,e=P::resource(token>>16);
            std::uint32_t h=0;
            if(index<p.u(p.u(e)+8u))h=p.u(p.u(p.u(e+0x24u))+index*4u);
            p.put(0x9406d8u+i,h);
        }
    p.push_load(cam+0x1c0u);
    p.put(0x8a9290u,0);p.put(0x8a9294u,0x9406d8u);p.put(0x8a9298u,6u);
    auto matrix=[&]{driving::PcMatrix16 m{};const auto top=p.current();for(unsigned k=0;k<16;++k)m[k]=top.f32(k*4);return m;};
    for(int n=12;n>0;--n){
        const std::uint32_t r=p.rand();
        const float y=st(X(fb(0x3fc00000u))-X(std::int32_t(r))*X(fb(0x38000100u))*X(fb(0x40200000u)));   // 5B005C - r * 62827C * 6281EC
        const auto at=driving::pc_d3dx_vec3_transform_coord(driving::PcVec3{0.0f,y,fb(0xc0400000u)},matrix());
        p.putf(0x8a9254u,at[0]);p.putf(0x8a9258u,at[1]);p.putf(0x8a925cu,at[2]);
        p.put(0x8a926cu,0x3f800000u);p.put(0x8a9270u,0);p.put(0x8a9274u,0x3f800000u);
        const auto v=driving::pc_d3dx_vec3_transform_normal(driving::PcVec3{0.0f,fb(0xbba3d70au),0.0f},matrix());
        p.putf(0x8a9260u,v[0]);p.putf(0x8a9264u,v[1]);p.putf(0x8a9268u,v[2]);
        p.put(0x8a9278u,0x3b449ba6u);p.put(0x8a927cu,0);p.put(0x8a9280u,0x3b449ba6u);
        particles_detail::born(p,0xa);
    }
    p.pop();
}
}

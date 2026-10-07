#include "platform/rob_osage.hpp"
#include "driving/pc_matrix_stack.hpp"
#include "driving/pc_x87.hpp"
#include <array>
#include <stdexcept>
namespace outrun::platform {
namespace {
using driving::CourseProbe;
using driving::X87;
using driving::x87_float;
constexpr float One62806c=1.0f,Gravity73771c=0.0027222000062465668f,Damping7134ec=0.0020000000949949026f;
constexpr std::uint32_t WarmUp722d40=20u;
constexpr std::uint32_t Pool85de60=0x85de60u,Mask85a710=0x85a710u;
struct Osage {
    PcRaceContext& c;
    PcRaceMemory& m;
    explicit Osage(PcRaceContext& cc):c(cc),m(cc.m){}
    CourseProbe vec(std::uint32_t a){return {m.f32(a),m.f32(a+4),m.f32(a+8)};}
    void put_vec(std::uint32_t a,const CourseProbe& v){m.putf(a,v.x);m.putf(a+4,v.y);m.putf(a+8,v.z);}
    void zero_vec(std::uint32_t a){m.putf(a+8,0.0f);m.putf(a+4,0.0f);m.putf(a,0.0f);}
    // 40EF10(a, b): a += b (x87 per lane, spilled).
    static CourseProbe add(const CourseProbe& a,const CourseProbe& b){
        return {x87_float(X87(a.x)+X87(b.x)),x87_float(X87(b.y)+X87(a.y)),x87_float(X87(b.z)+X87(a.z))};}
    // 40EFA0(out, a, b): a - b.
    static CourseProbe sub(const CourseProbe& a,const CourseProbe& b){
        return {x87_float(X87(a.x)-X87(b.x)),x87_float(X87(a.y)-X87(b.y)),x87_float(X87(a.z)-X87(b.z))};}
    // 40F050(out, v, s): v * s.
    static CourseProbe scale(const CourseProbe& v,float s){
        return {x87_float(X87(s)*X87(v.x)),x87_float(X87(s)*X87(v.y)),x87_float(X87(s)*X87(v.z))};}
    // 40F080(out, v, length): out = v * (length / |v|) when |v| > 1e-4 (the
    // double literal 6282A8); else out is left as it was.
    static void set_length(CourseProbe& out,const CourseProbe& v,float length){
        const X87 x(v.x),y(v.y),z(v.z);
        const X87 len=driving::x87_sqrt((x*x+y*y)+z*z);
        if(!(len>X87(0.0001)))return;
        const X87 k=X87(length)/len;
        out={x87_float(k*x),x87_float(k*y),x87_float(k*z)};
    }
    // ---- matrix leaves on the 89B564 stack ----------------------------------
    void push(){driving::pc_matrix_push(c.matrices);}
    void pop(){driving::pc_matrix_pop(c.matrices);}
    void rotations(std::uint32_t rec){          // 40A440 z, 40A410 y, 40A3E0 x
        driving::pc_matrix_rotate_z(c.matrices,m.f32(rec+0x20));
        driving::pc_matrix_rotate_y(c.matrices,m.f32(rec+0x1c));
        driving::pc_matrix_rotate_x(c.matrices,m.f32(rec+0x18));
    }
    std::uint32_t node(std::uint32_t chain,std::uint32_t k){return m.u32(chain+0xc)+k*PcRobOsageState::NodeSize;}
    // 5223B0(A, B, out): T = unit(A - B); out = (0, -g, 0) - T * (T.y * g).
    CourseProbe droop_5223b0(std::uint32_t a,std::uint32_t b){
        CourseProbe t=sub(vec(a),vec(b));
        (void)driving::pc_unit_vector_40eeb0(t);
        const float s=t.y*Gravity73771c;                       // MULSS
        const CourseProbe u=scale(t,s);
        return sub(CourseProbe{0.0f,0.0f-Gravity73771c,0.0f},u);
    }
    // 522560(list, position): push the position out of the type-1 spheres
    // {type, centre, radius} (0x1C records, type 0 ends).
    void collide_522560(std::uint32_t list,std::uint32_t pos){
        for(std::uint32_t r=list;m.u32(r)!=0u;r+=0x1cu){
            if(m.u32(r)!=1u)continue;
            const float dy=m.f32(pos+4)-m.f32(r+8),dx=m.f32(pos)-m.f32(r+4),dz=m.f32(pos+8)-m.f32(r+0xc),radius=m.f32(r+0x10);
            const float d2=((dz*dz)+(dy*dy))+(dx*dx),r2=radius*radius;
            if(!(r2>d2))continue;
            const X87 k=X87(radius)/driving::x87_sqrt(X87(d2));
            m.putf(pos,x87_float(X87(dx)*k+X87(m.f32(r+4))));
            m.putf(pos+4,x87_float(X87(dy)*k+X87(m.f32(r+8))));
            m.putf(pos+8,x87_float(k*X87(dz)+X87(m.f32(r+0xc))));
        }
    }
    // 401790(a, b, length): a = b + (a - b) * (length / |a - b|) when longer.
    void limit_401790(std::uint32_t a,std::uint32_t b,float length){pc_limit_401790(m,a,b,length);}
    // 522450(chain): node matrices into the pool.
    void matrices_522450(std::uint32_t chain){
        const std::uint32_t rec=m.u32(chain);
        if(!rec||!m.u32(chain+4))return;
        driving::pc_matrix_translate_vector(c.matrices,vec(rec+0xc));
        rotations(rec);
        for(std::uint32_t k=1;k<m.u32(chain+8);++k){
            const std::uint32_t n=node(chain,k);
            const CourseProbe v=driving::pc_matrix_inverse_point(c.matrices,vec(n+4));
            const float z=x87_float(driving::x87_atan2(X87(v.y),X87(v.x)));
            const float r=x87_float(driving::x87_sqrt(X87(v.y)*X87(v.y)+X87(v.x)*X87(v.x)));
            const float y=x87_float(driving::x87_atan2(X87(0.0f-v.z),X87(r)));
            driving::pc_matrix_rotate_z(c.matrices,z);
            driving::pc_matrix_rotate_y(c.matrices,y);
            driving::pc_matrix_get(c.matrices,m.bytes(m.u32(n+0x20),64));
            driving::pc_matrix_translate_vector(c.matrices,CourseProbe{m.f32(n),0.0f,0.0f});
        }
    }
    void step_522620(std::uint32_t chain){
        const std::uint32_t rec=m.u32(chain);
        if(!rec||!m.u32(chain+4))return;
        push();
        put_vec(node(chain,0)+4,driving::pc_matrix_point(c.matrices,vec(rec+0xc)));
        rotations(rec);
        const CourseProbe x1=driving::pc_matrix_vector(c.matrices,CourseProbe{One62806c,0.0f,0.0f});
        for(std::uint32_t k=1;k<m.u32(chain+8);++k){
            const std::uint32_t n=node(chain,k);
            const CourseProbe old=vec(n+4);
            CourseProbe f{0.0f,0.0f,0.0f};
            if(k!=m.u32(chain+8)-1u)f=droop_5223b0(n+4,n+0x30);
            const CourseProbe g=droop_5223b0(n+4,n-0x28);
            f=add(f,g);
            f=add(f,vec(chain+0x14));
            put_vec(n+0x10,add(vec(n+0x10),f));
            put_vec(n+0x10,add(vec(n+0x10),scale(x1,m.f32(n+0x24))));
            put_vec(n+4,add(vec(n+4),vec(n+0x10)));
            // 85DE58 (-> 722C60) and 85DE5C are never written by the EXE.
            std::uint32_t list=0;
            if(m.u32(0x85de58u))list=0x722c60u;
            else if(m.u32(0x85de5cu))list=m.u32(0x85de5cu);
            else list=m.u32(n+0x28);
            if(list){
                if(list==0x722c60u)throw std::logic_error("rob osage: 722C60 sphere list (85DE58 is never set by the EXE)");
                collide_522560(list,n+4);
            }
            limit_401790(n+4,n-0x28,m.f32(n));
            put_vec(n+0x10,sub(vec(n+4),old));
        }
        pop();
        matrices_522450(chain);
    }
    // 522810(chain): nodes laid out along the record's direction, then the
    // warm-up steps.
    void place_522810(std::uint32_t chain){
        const std::uint32_t rec=m.u32(chain);
        if(!rec||!m.u32(chain+4))return;
        m.put32(0x85de50u,m.u32(chain+0xc));m.put32(0x85de54u,rec);
        push();
        CourseProbe p=driving::pc_matrix_point(c.matrices,vec(rec+0xc));
        put_vec(node(chain,0)+4,p);zero_vec(node(chain,0)+0x10);
        rotations(rec);
        CourseProbe d=driving::pc_matrix_vector(c.matrices,CourseProbe{One62806c,0.0f,0.0f});
        d.y=d.y-Gravity73771c;                                       // SUBSS
        CourseProbe s{0.0f,0.0f,0.0f};                               // PC: stack contents until the first 40F080
        for(std::uint32_t k=1;k<m.u32(chain+8);++k){
            const std::uint32_t n=node(chain,k);
            set_length(s,d,m.f32(n));
            p=add(p,s);
            put_vec(n+4,p);zero_vec(n+0x10);
        }
        pop();
        for(std::uint32_t i=0;i<WarmUp722d40;++i){push();step_522620(chain);pop();}
    }
    // 5229A0(chain, record, parameters).
    void build_5229a0(RobotHeap& heap,std::uint32_t chain,std::uint32_t rec,std::uint32_t param){
        m.put32(chain+4,param);m.put32(chain,rec);
        m.putf(chain+0x1c,0.0f);m.putf(chain+0x18,0.0f);m.putf(chain+0x14,0.0f);
        // Node template (the PC stack block copied by REP MOVSD): +00, +1C,
        // +20 and +28 are set; the other words are stack contents (0 here),
        // all later overwritten except node 0's +24/+28.
        std::array<std::uint32_t,11> t{};
        t[7]=m.u32(param+4);
        const std::int32_t count=std::int32_t(m.u32(rec+8));
        const std::uint32_t buf=heap.malloc(std::uint32_t(count+1)*PcRobOsageState::NodeSize);
        if(!buf)throw std::runtime_error("rob osage: 580253 returned NULL (the PC writes through it)");
        {auto& b=heap.blocks.back();m.map(b.base,b.bytes.data(),b.bytes.size());}
        m.put32(chain+8,0);m.put32(chain+0xc,buf);
        for(unsigned w=0;w<11;++w)m.put32(buf+w*4u,t[w]);
        m.put32(chain+8,1);
        std::uint32_t cursor=param+0xc;
        for(std::int32_t k=0;k<std::int32_t(m.u32(rec+8));++k,cursor+=8u){
            t[0]=m.u32(cursor-8u);
            if(k!=std::int32_t(m.u32(rec+8))-1)t[7]=m.u32(cursor);
            const std::uint32_t pool=m.u32(Pool85de60);
            t[8]=pool;t[10]=0;
            const std::uint32_t dst=m.u32(chain+0xc)+m.u32(chain+8)*PcRobOsageState::NodeSize;
            for(unsigned w=0;w<11;++w)m.put32(dst+w*4u,t[w]);
            m.put32(Pool85de60,pool+0x40u);
            m.put32(chain+8,m.u32(chain+8)+1u);
        }
        place_522810(chain);
    }
};
std::uint32_t chain_of(std::uint32_t disp,std::uint32_t i){return PcRobOsageState::Chains+((disp<<4)+i)*PcRobOsageState::ChainSize;}
}
void rob_osage_reset_514e80(PcRaceMemory& m,RobotHeap& heap,std::uint32_t work){
    for(std::uint32_t i=0;i<16u;++i){
        const std::uint32_t ch=chain_of(m.u32(work),i);                 // 522280
        m.put32(ch,0);m.put32(ch+4,0);m.put32(ch+0x10,0);m.put32(ch+8,0);
        if(const auto p=m.u32(ch+0xc)){heap.free(p);m.put32(ch+0xc,0);}
    }
}
void rob_osage_step_522620(PcRaceContext& c,std::uint32_t chain){Osage(c).step_522620(chain);}
void pc_limit_401790(PcRaceMemory& m,std::uint32_t a,std::uint32_t b,float length){
    const X87 dx=X87(m.f32(a))-X87(m.f32(b));
    const float dy=x87_float(X87(m.f32(a+4))-X87(m.f32(b+4)));   // spilled to the argument slot
    const X87 dz=X87(m.f32(a+8))-X87(m.f32(b+8));
    const X87 d2=(dz*dz+X87(dy)*X87(dy))+dx*dx;
    const X87 l2=X87(length)*X87(length);
    if(!(d2>l2))return;
    const X87 k=X87(length)/driving::x87_sqrt(d2);
    m.putf(a,x87_float(k*dx+X87(m.f32(b))));
    m.putf(a+4,x87_float(X87(dy)*k+X87(m.f32(b+4))));
    m.putf(a+8,x87_float(k*dz+X87(m.f32(b+8))));
}
namespace {
std::uint32_t bone_matrix_4f1ec0(PcRaceMemory& m,std::uint32_t motion,std::uint32_t id){
    const std::int32_t bones=std::int16_t(m.u16(motion+0x68));
    if(bones>std::int32_t(id&0x7fffu)&&!(id&0x8000u))return m.u32(motion+0x74)+(id&0x7fffu)*0x350u;
    return 0u;
}
// 514EC0(motion, definitions, out): world spheres {type, centre, radius,
// +14, +18} (0x1C) from 0x20-byte definitions {bone | -1, type, radius,
// centre, +18, +1C}; stops at a type 0 (or a bone without matrix); out ends
// with a 0 word.
void spheres_514ec0(PcRaceContext& c,std::uint32_t motion,std::uint32_t def,std::uint32_t out){
    auto& m=c.m;
    if(!def){m.put32(out,0);return;}
    if(m.u32(def+4)){
        for(;;){
            const std::uint32_t id=m.u32(def);
            if(id==0xffffffffu){m.put32(out+4,m.u32(def+0xc));m.put32(out+8,m.u32(def+0x10));m.put32(out+0xc,m.u32(def+0x14));}
            else{
                const std::uint32_t bm=bone_matrix_4f1ec0(m,motion,id);
                if(!bm)break;
                driving::pc_matrix_load(c.matrices,m.bytes(bm,64));
                const driving::CourseProbe p=driving::pc_matrix_point(c.matrices,{m.f32(def+0xc),m.f32(def+0x10),m.f32(def+0x14)});
                m.putf(out+4,p.x);m.putf(out+8,p.y);m.putf(out+0xc,p.z);
            }
            m.put32(out,m.u32(def+4));m.put32(out+0x10,m.u32(def+8));m.put32(out+0x14,m.u32(def+0x18));m.put32(out+0x18,m.u32(def+0x1c));
            def+=0x20u;out+=0x1cu;
            if(!m.u32(def+4))break;
        }
    }
    m.put32(out,0);
}
}
void rob_osage_ctrl_515040(PcRaceContext& c,std::uint32_t work,std::uint32_t motion){
    auto& m=c.m;
    const std::uint32_t kind=m.u32(work+8);
    const std::uint32_t osage=m.u32(m.u32(0x82f388u+kind*4u)+0x1c);
    if(!osage)return;
    std::uint32_t rec=m.u32(osage+8),defs=0,damping=0,slot=0;
    // 515298 byte table / 515288 jump table over kind 5..15.
    static constexpr std::uint8_t Case[11]={0,0,0,0,3,3,3,3,1,2,2};
    if(kind-5u<=10u){
        switch(Case[kind-5u]){
        case 0:defs=0x5e6740u;break;
        case 1:defs=0x5e6900u;damping=0x7134f0u;break;
        case 2:defs=0x5e6ae0u;damping=0x713570u;break;
        default:break;
        }
    }
    if(const std::uint32_t only=m.u32(0x85a714u)){
        defs=0x7147a8u;damping=0x85a730u;
        if(only!=m.u32(work)+1u)return;
    }
    Osage o(c);
    o.push();
    const std::uint32_t disp=m.u32(work);
    for(std::int32_t i=0;i<std::int32_t(m.u32(osage));++i,rec+=0x24u){
        const std::uint32_t bm=bone_matrix_4f1ec0(m,motion,m.u32(rec));
        if(!bm)continue;
        const std::uint32_t ch=chain_of(disp,std::uint32_t(i));
        if(defs){
            std::uint32_t idx=m.u32(osage+0x10)+m.u32(rec+4)*4u;
            if(damping){   // 5222F0(list): nodes 1.. +24
                std::uint32_t list=damping+m.u32(idx)*4u;
                for(std::uint32_t k=1;k<m.u32(ch+8);++k){
                    if(list){m.put32(o.node(ch,k)+0x24,m.u32(list));list+=4u;}
                    else m.putf(o.node(ch,k)+0x24,0.0f);
                }
            }
            const std::uint32_t first=slot;
            for(std::int32_t k=0;k<std::int32_t(m.u32(rec+8));++k,idx+=4u,++slot)
                spheres_514ec0(c,motion,m.u32(defs+m.u32(idx)*4u),PcRobOsageState::SphereBase+slot*0x70u);
            // 522360(7136D0 + first*4): nodes 1.. +28 = the storage pointers
            std::uint32_t list=0x7136d0u+first*4u;
            for(std::uint32_t k=1;k<m.u32(ch+8);++k){
                if(list){m.put32(o.node(ch,k)+0x28,m.u32(list));list+=4u;}
                else m.put32(o.node(ch,k)+0x28,0);
            }
        }else{
            for(std::uint32_t k=1;k<m.u32(ch+8);++k)m.put32(o.node(ch,k)+0x28,0);   // 522360(0)
        }
        driving::pc_matrix_load(c.matrices,m.bytes(bm,64));
        if(m.u32(Mask85a710)&(1u<<(disp&31u)))o.place_522810(ch);
        m.put32(ch+0x14,m.u32(0x85a720u));m.put32(ch+0x18,m.u32(0x85a724u));m.put32(ch+0x1c,m.u32(0x85a728u));   // 522340
        o.step_522620(ch);
    }
    m.put32(Mask85a710,m.u32(Mask85a710)&~(1u<<(disp&31u)));
    o.pop();
}
void rob_osage_init_514f60(PcRaceContext& c,RobotHeap& heap,std::uint32_t work,std::uint32_t motion){
    auto& m=c.m;
    const std::uint32_t chr=m.u32(0x82f388u+m.u32(work+8)*4u);
    const std::uint32_t osage=m.u32(chr+0x1c);
    if(!osage)return;
    const std::uint32_t param=m.u32(osage+0xc);
    std::uint32_t rec=m.u32(osage+8);
    rob_osage_reset_514e80(m,heap,work);
    const std::uint32_t disp=m.u32(work);
    m.put32(Pool85de60,0x858710u+(disp<<11));                          // 522270
    Osage o(c);
    o.push();
    for(std::int32_t i=0;i<std::int32_t(m.u32(osage));++i,rec+=0x24u){
        // 4F1EC0 GetMatrix(motion, bone id)
        const std::uint32_t id=m.u32(rec);
        const std::int32_t bones=std::int16_t(m.u16(motion+0x68));
        std::uint32_t bm=0;
        if(bones>std::int32_t(id&0x7fffu)&&!(id&0x8000u))bm=m.u32(motion+0x74)+(id&0x7fffu)*0x350u;
        if(bm)driving::pc_matrix_load(c.matrices,m.bytes(bm,64));
        else driving::pc_matrix_identity(c.matrices);
        const std::uint32_t ch=chain_of(m.u32(work),std::uint32_t(i));
        o.build_5229a0(heap,ch,rec,param);
        // 5222B0(damping 7134EC): nodes 1.. +24
        for(std::uint32_t k=1;k<m.u32(ch+8);++k)m.putf(o.node(ch,k)+0x24,Damping7134ec);
    }
    o.pop();
    m.put32(Mask85a710,m.u32(Mask85a710)|(1u<<(disp&31u)));
}
}

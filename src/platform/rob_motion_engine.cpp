#include "platform/rob_motion_engine.hpp"
#include "driving/pc_d3dx.hpp"
#include "driving/pc_matrix_stack.hpp"
#include "driving/pc_x87.hpp"
#include <array>
#include <cstring>
#include <stdexcept>
namespace outrun::platform {
namespace {
using driving::Bytes;
using driving::CourseProbe;
using driving::X87;
using driving::x87_float;
// EXE constants (read-only): 62806C 1.0, 6280E8 1/60, 6281F0 FLT_EPSILON,
// 6282A4 3.0, 6280B0 2.0, 62813C 0.1, 6280C4 -1.0, 6280C8 pi, 6281C0 0.01,
// 62825C -pi/2, 628260 pi/2, 5A29D8 -pi, 5A29DC 2pi, 5E0AB8 1e-8, 5E5884
// 2/pi, 5E6BE4/5E6BE8 -+3.1415827, 5E6BEC 20pi, 619A34 0.0.
float bits(std::uint32_t u){float f;std::memcpy(&f,&u,4);return f;}
const float One=1.0f,Sixtieth=bits(0x3c888889u),Epsilon=bits(0x34000000u),Three=3.0f,Two=2.0f,Tenth=bits(0x3dcccccdu),
    MinusOne=-1.0f,Pi=bits(0x40490fdbu),Hundredth=bits(0x3c23d70au),MinusHalfPi=bits(0xbfc90fdbu),HalfPi=bits(0x3fc90fdbu),
    MinusPi=bits(0xc0490fdbu),TwoPi=bits(0x40c90fdbu),Tiny=bits(0x322bcc77u),TwoOverPi=bits(0x3f22f983u),
    MinusPiEdge=bits(0xc0490fb1u),PiEdge=bits(0x40490fb1u),TwentyPi=bits(0x427b53d2u),Zero619a34=0.0f;
constexpr std::uint32_t Flags85b2e8=0x85b2e8u,Factor85b2bc=0x85b2bcu,Bone85b2b8=0x85b2b8u,Bone85b2c0=0x85b2c0u,
    Angle85b2c4=0x85b2c4u,Offset85b2d4=0x85b2d4u,Bone85b2e0=0x85b2e0u,Bone85b2e4=0x85b2e4u;
// ---- records (may_alias views over the robot heap) ------------------------------
#define OR2_PC_RECORD struct __attribute__((may_alias))
OR2_PC_RECORD MotionKey { float time,tangent_in,tangent_out,value; };
// Key channel (0x48): the two keys around the current time and the stream.
OR2_PC_RECORD MotionChannel {
    MotionKey key[2];            // +00 / +10
    std::uint32_t unknown20;
    std::uint16_t keys;          // +24 key count (0: the default below)
    std::uint16_t left;          // +26 keys still in the stream
    std::uint32_t unknown28[4];
    std::uint32_t stream;        // +38 first key in the motion data
    std::uint32_t cursor;        // +3C next key
    float rest;                  // +40 default value
    std::uint32_t unknown44;
};
// Bone (0x350, bone buffer + i*0x350).
OR2_PC_RECORD MotionBone {
    float matrix[16];            // +00 world matrix (51B420 get / kind 3 load)
    std::uint32_t file_record;   // +40
    std::int8_t kind;            // +44 0/1 keys, 2 keys + IK, 3 fixed matrix, 4 rotation keys
    std::int8_t ik;              // +45 IK kind (2 two-bone, 1 aim); on the middle joint: the bend side
    std::uint8_t unit_rotation;  // +46
    std::int8_t children;        // +47
    MotionChannel channels[9];   // +48 translation xyz, rotation xyz, scale xyz
    float ik_target[3];          // +2D0
    float translation[3];        // +2DC
    float rotation[3];           // +2E8
    float scale[3];              // +2F4
    float saved_ik_target[3];    // +300
    float saved_translation[3];  // +30C
    float saved_rotation[3];     // +318
    float saved_scale[3];        // +324
    std::uint32_t child_list;    // +330 array of child bone addresses
    std::uint32_t unknown334;
    std::uint32_t id;            // +338 bone index
    std::uint32_t target_mode;   // +33C IK override: 1 = +340, 2 = keys with +344 as the y limit
    float target_override[3];    // +340
    std::uint32_t unknown34c;
};
static_assert(sizeof(MotionChannel)==0x48&&sizeof(MotionBone)==0x350,"RobMotion record layouts");
std::uint32_t& rob_motion_degenerate_blends_counter(){static std::uint32_t n=0;return n;}
struct Engine {
    PcRaceContext& c;
    PcRaceMemory& m;
    explicit Engine(PcRaceContext& cc):c(cc),m(cc.m){}
    float f(std::uint32_t a){return m.f32(a);}
    void pf(std::uint32_t a,float v){m.putf(a,v);}
    CourseProbe vec(std::uint32_t a){return {f(a),f(a+4),f(a+8)};}
    void put_vec(std::uint32_t a,const CourseProbe& v){pf(a,v.x);pf(a+4,v.y);pf(a+8,v.z);}
    void copy3(std::uint32_t to,std::uint32_t from){m.put32(to,m.u32(from));m.put32(to+4,m.u32(from+4));m.put32(to+8,m.u32(from+8));}
    std::uint32_t flags(){return m.u32(Flags85b2e8);}
    float factor(){return f(Factor85b2bc);}
    // ---- matrix leaves ------------------------------------------------------
    void push(){driving::pc_matrix_push(c.matrices);}
    void push_unit(){driving::pc_matrix_push_unit(c.matrices);}
    void pop(){driving::pc_matrix_pop(c.matrices);}
    void load(std::uint32_t a){driving::pc_matrix_load(c.matrices,m.bytes(a,64));}
    void get(std::uint32_t a){driving::pc_matrix_get(c.matrices,m.bytes(a,64));}
    void translate(const CourseProbe& v){driving::pc_matrix_translate_vector(c.matrices,v);}   // 40A2D0 / 40A290
    void unit_rotation(){driving::pc_matrix_unit_rotation(c.matrices);}                        // 40A0A0
    void rotate_zyx(float x,float y,float z){                                                 // 40A470(x, y, z)
        driving::pc_matrix_rotate_z(c.matrices,z);driving::pc_matrix_rotate_y(c.matrices,y);driving::pc_matrix_rotate_x(c.matrices,x);}
    void rotate_y(float a){driving::pc_matrix_rotate_y(c.matrices,a);}                          // 40A410
    void multiply(const std::array<float,16>& left){                                           // 439394(cur, left, cur)
        std::array<float,16> l=left;driving::pc_matrix_multiply_current(c.matrices,Bytes(l.data(),64));}
    void scale(float x,float y,float z){                                                       // 40A360: D3DXMatrixScaling
        multiply({x,0,0,0, 0,y,0,0, 0,0,z,0, 0,0,0,1});}
    void rotation_z_cs(float s,float co){multiply({co,s,0,0, -s,co,0,0, 0,0,1,0, 0,0,0,1});}  // 40AA50(sin, cos)
    void rotation_y_cs(float s,float co){multiply({co,0,-s,0, 0,1,0,0, s,0,co,0, 0,0,0,1});}  // 40A9B0(sin, cos)
    void invert(){                                                                             // 40A240: D3DXMatrixInverse(cur, NULL, cur)
        auto cur=c.matrices.current();(void)driving::pc_d3dx_matrix_inverse(cur,nullptr,cur);}
    CourseProbe point(const CourseProbe& v){return driving::pc_matrix_point(c.matrices,v);}    // 40A7D0
    // ---- keys -----------------------------------------------------------------
    // 522AA0: half float (sign, 5-bit exponent biased +112, 10-bit mantissa); 0 -> 0.
    static float half(std::uint16_t h){
        if(h==0u)return 0.0f;
        const std::uint32_t v=((std::uint32_t(h&0x7fffu)+0x1c000u)<<13)|(std::uint32_t(h&0x8000u)<<16);
        return bits(v);
    }
    // ---- typed views (one lookup per record) ------------------------------------
    MotionBone& bone(std::uint32_t a){return *reinterpret_cast<MotionBone*>(m.at(a,sizeof(MotionBone),true));}
    MotionChannel& channel(std::uint32_t a){return *reinterpret_cast<MotionChannel*>(m.at(a,sizeof(MotionChannel),true));}
    static CourseProbe vec(const float* v){return {v[0],v[1],v[2]};}
    static void put_vec(float* o,const CourseProbe& v){o[0]=v.x;o[1]=v.y;o[2]=v.z;}
    static void copy3(float* to,const float* from){std::memcpy(to,from,12);}
    // ---- keys -----------------------------------------------------------------
    // 522AE0(key, cursor): {time, in, out, value} from the key stream.
    void read_key_522ae0(MotionKey& key,std::uint32_t& cursor){
        auto next=[&]{const std::uint16_t v=m.u16(cursor);cursor+=2u;return v;};
        const std::uint16_t w=next();
        key.time=float(std::int32_t(w&0x1fffu))*Sixtieth;
        switch(w>>14){
        case 0:key.value=0.0f;key.tangent_out=0.0f;key.tangent_in=0.0f;return;
        case 1:key.value=half(next());key.tangent_out=0.0f;key.tangent_in=0.0f;return;
        case 2:{key.value=half(next());const float t=half(next());key.tangent_out=t;key.tangent_in=t;return;}
        default:key.value=half(next());key.tangent_in=half(next());key.tangent_out=half(next());return;
        }
    }
    // 522BE0(channel): rewind to the first two keys.
    void rewind_522be0(MotionChannel& ch){
        if(!ch.keys)return;
        ch.left=std::uint16_t(ch.keys-2u);
        ch.cursor=ch.stream;
        ch.key[0].time=0.0f;
        read_key_522ae0(ch.key[0],ch.cursor);
        read_key_522ae0(ch.key[1],ch.cursor);
    }
    // 522EE0(flag, set, channel): counts the channel's keys in the stream.
    void channel_522ee0(std::uint32_t flag,std::uint32_t set,MotionChannel& ch){
        std::uint32_t n=0;
        if(!flag){ch.keys=0;return;}
        ch.stream=m.u32(set+0x40);
        for(;;){
            std::uint32_t p=m.u32(set+0x40);
            const std::uint32_t w=m.u16(p);
            const bool init=(w&0x2000u)!=0u;
            if(init&&(w&0x1fffu)==1u)break;
            if(n!=0u&&(w&0x1fffu)==0u)break;
            p+=2u;++n;m.put32(set+0x40,p);
            if(init&&(w&0x1fffu)==0u){
                const std::uint16_t v=m.u16(p);m.put32(set+0x40,p+2u);
                ch.key[0].value=half(v);
                break;
            }
            p+=(w>>14)*2u;m.put32(set+0x40,p);
        }
        ch.keys=std::uint16_t(n);
        if(n>=2u)rewind_522be0(ch);
    }
    // 522F70(set, bones, -): per bone its channel mask from the 8/16-bit table.
    void channels_522f70(std::uint32_t set,std::uint32_t bones){
        const std::int32_t n=std::int16_t(m.u16(set+0x10));
        const bool wide=(m.u8(set+0x16)&2u)!=0u;
        std::uint32_t c16=wide?m.u32(set+0x38):0u,c8=wide?0u:m.u32(set+0x3c);
        static constexpr std::uint32_t Mask[9]={0x20,0x10,0x8,0x4,0x2,0x1,0x100,0x80,0x40};
        for(std::int32_t i=0;i<n;++i){
            std::uint32_t mask=0;
            if(wide){if(std::uint32_t(i)==m.u16(c16)){mask=m.u16(c16+2);c16+=4u;}}
            else{if(std::uint32_t(i)==m.u8(c8)){mask=m.u8(c8+1);c8+=2u;}}
            MotionBone& b=bone(bones+std::uint32_t(i)*sizeof(MotionBone));
            for(unsigned k=0;k<9;++k)channel_522ee0(mask&Mask[k],set,b.channels[k]);
        }
    }
    // 520750(set, entry, base).
    void keys_520750(std::uint32_t set,std::uint32_t entry,std::uint32_t base){
        pf(set+0x30,Tenth);
        m.put16(set+0x16,m.u16(entry+2));m.put16(set+0x34,m.u16(entry+4));m.put16(set+0x12,m.u16(entry+4));
        m.put16(set+0x2c,0x3c);m.put32(set+0x20,0);
        m.put32(set+0x24,std::uint32_t(m.u16(entry+4))-1u);
        pf(set+0x28,One);
        m.put32(set+8,m.u16(entry));
        if(m.u8(set+0x16)&2u)m.put32(set+0x38,m.u32(entry+0xc)+base);
        else m.put32(set+0x3c,m.u32(entry+0xc)+base);
        m.put32(set+0x40,m.u32(entry+0x14)+base);
        channels_522f70(set,m.u32(set+0x1c));
    }
    // 522C20(channel, t): Hermite interpolation (x87 result).
    X87 evaluate_522c20(MotionChannel& ch,float t){
        const MotionKey& k0=ch.key[0];
        const MotionKey& k1=ch.key[1];
        if(ch.keys==1u)return X87(k0.value);
        if(k0.time>t){
            if(std::uint32_t(ch.left)==std::uint32_t(ch.keys)-2u)return X87(k0.value);
            rewind_522be0(ch);
            if(k0.time>t)return X87(k0.value);
        }else if(X87(Epsilon)>driving::x87_abs(X87(k0.time)-X87(t)))return X87(k0.value);
        while(!(k1.time>t)){
            if(X87(Epsilon)>driving::x87_abs(X87(k1.time)-X87(t)))return X87(k1.value);
            if(!ch.left)return X87(k1.value);
            --ch.left;
            ch.key[0]=ch.key[1];
            read_key_522ae0(ch.key[1],ch.cursor);
        }
        const float u=t-k0.time,dt=k1.time-k0.time;
        const float inv=One/dt;
        const float u2=u*u,inv2=inv*inv;
        const float s2=u2*inv2;
        const float ha=s2*Three;
        const float s2u=s2*u;
        const float u2inv=u2*inv;
        const float a=s2u-u2inv;
        const float b=(s2u*inv)*Two;
        const X87 p=((X87(a)-X87(u2inv))+X87(u))*X87(k0.tangent_out);
        const X87 q=((X87(b)-X87(ha))+X87(One))*X87(k0.value);
        const X87 r=(X87(ha)-X87(b))*X87(k1.value);
        const X87 s=X87(a)*X87(k1.tangent_in);
        return ((p+q)+r)+s;
    }
    // 519920(channels, out, t): a channel without keys keeps its default.
    void evaluate3_519920(MotionChannel* chs,float* out,float t){
        for(unsigned k=0;k<3;++k){
            if(chs[k].keys==0u)std::memcpy(&out[k],&chs[k].rest,4);
            else out[k]=x87_float(evaluate_522c20(chs[k],t));
        }
    }
    // ---- blend helpers ---------------------------------------------------------
    void blend3(float* v,const float* saved){                // v = (v - saved) * f + saved (SSE)
        const float fa=factor();
        for(unsigned k=0;k<3;++k)v[k]=(v[k]-saved[k])*fa+saved[k];
    }
    static bool rescale_needed(const float* s){              // |1 - s| > 0.01 on any axis
        for(unsigned k=0;k<3;++k)
            if(driving::x87_abs(X87(One)-X87(s[k]))>X87(Hundredth))return true;
        return false;
    }
    void scale_if_needed(const float* s){if(rescale_needed(s))scale(s[0],s[1],s[2]);}
    // 51A3A0 case 1: translation blend (the Y lane twice when the bone is 85B2E0).
    void blend_translation_51a3a0(MotionBone& b){
        const float fa=factor();
        if(b.id!=m.u32(Bone85b2e0)){blend3(b.translation,b.saved_translation);return;}
        const float* s=b.saved_translation;float* v=b.translation;
        const float y=(v[1]-s[1])*fa+s[1];
        v[1]=y;
        v[0]=(v[0]-s[0])*fa+s[0];
        v[1]=(y-s[1])*fa+s[1];
        v[2]=(v[2]-s[2])*fa+s[2];
    }
    // 519970(a, b, t, out): out = a*t + b*(1-t) row-wise, rows re-orthonormalised.
    // Returns false when a row is left unwritten (length <= 0.01): the PC then
    // reads its uninitialised stack matrix (519DD0 +80), not modelled.
    static bool lerp_519970(const std::array<float,16>& a,const std::array<float,16>& b,float t,std::array<float,16>& o){
        bool full=true;
        const float s=One-t;
        const float x0=a[0]*t+b[0]*s,x1=a[4]*t+b[4]*s,x2=a[8]*t+b[8]*s;
        o[0]=x0;o[4]=x1;o[8]=x2;
        {
            const float l=x87_float(driving::x87_sqrt((X87(x2)*X87(x2)+X87(x1)*X87(x1))+X87(x0)*X87(x0)));
            if(l>Hundredth){const float k=One/l;o[0]=x0*k;o[4]=x1*k;o[8]=x2*k;}
        }
        const float y0=b[1]*s+a[1]*t,y1=b[5]*s+a[5]*t,y2=b[9]*s+a[9]*t;
        // z = x x y (from the unnormalised x), then y = z x x.
        const float z0=y2*x1-y1*x2,z1=y0*x2-y2*x0,z2=y1*x0-y0*x1;
        const float w0=z1*x2-z2*x1,w1=z2*x0-z0*x2,w2=z0*x1-z1*x0;
        {
            const float l=x87_float(driving::x87_sqrt((X87(w2)*X87(w2)+X87(w1)*X87(w1))+X87(w0)*X87(w0)));
            if(l>Hundredth){const float k=One/l;o[1]=w0*k;o[5]=w1*k;o[9]=w2*k;}else full=false;
        }
        {
            const float l=x87_float(driving::x87_sqrt((X87(z2)*X87(z2)+X87(z1)*X87(z1))+X87(z0)*X87(z0)));
            if(l>Hundredth){const float k=One/l;o[2]=z0*k;o[6]=z1*k;o[10]=z2*k;}else full=false;
        }
        return full;
    }
    // 519CE0(out, matrix | 0 = current): angles of a rotation matrix.
    static void angles_519ce0(float* out,const std::array<float,16>& mx){
        using driving::x87_atan2;
        if(driving::x87_abs(X87(mx[10]))>X87(Tiny)){
            const X87 v=(X87(One)/X87(mx[10]))*X87(mx[6]);
            const X87 ax=x87_atan2(v,X87(1.0f));
            out[0]=x87_float(ax);
            const X87 y=x87_atan2(-X87(mx[2]),(X87(One)/driving::x87_cos(ax))*X87(mx[10]));
            out[1]=x87_float(y);
            if(X87(HalfPi)>driving::x87_abs(y))out[2]=x87_float(x87_atan2(X87(mx[1]),X87(mx[0])));
            else out[2]=x87_float(x87_atan2(-X87(mx[1]),-X87(mx[0])));
            return;
        }
        if(driving::x87_abs(X87(mx[6]))>X87(Tiny)){
            out[0]=HalfPi;
            out[1]=x87_float(x87_atan2(-X87(mx[2]),X87(mx[6])));
            out[2]=x87_float(x87_atan2(X87(mx[8]),-X87(mx[9])));
            return;
        }
        out[0]=0.0f;
        out[2]=x87_float(x87_atan2(-X87(mx[4]),X87(mx[5])));
        out[1]=mx[2]>0.0f?MinusHalfPi:HalfPi;
    }
    std::array<float,16> current(){std::array<float,16> a{};auto cur=c.matrices.current();for(unsigned k=0;k<16;++k)a[k]=cur.f32(k*4u);return a;}
    // 519DD0(esi = angles, edi = saved angles, factor): blend through matrices.
    void blend_rotation_519dd0(float* angles,const float* saved,float fa){
        push_unit();rotate_zyx(angles[0],angles[1],angles[2]);const auto m1=current();
        push_unit();rotate_zyx(saved[0],saved[1],saved[2]);const auto m2=current();
        pop();pop();
        std::array<float,16> o{};
        if(!lerp_519970(m1,m2,fa,o))++rob_motion_degenerate_blends_counter();
        angles_519ce0(angles,o);
    }
    // 519C30(a): angle into (-pi, pi], +-3.1415827 edges -> pi.
    static float wrap_519c30(float a){
        if(driving::x87_abs(X87(a))>X87(TwentyPi)){
            const float x=a*TwoOverPi;
            a=float(driving::x87_ftol32(X87(x)))*TwoPi+a;     // CVTTSS2SI (truncation)
        }else{
            if(MinusPi>a){do{a=a+TwoPi;}while(MinusPi>a);}
            if(a>Pi){do{a=a-TwoPi;}while(a>Pi);}
        }
        if(a>=PiEdge||MinusPiEdge>=a)a=Pi;                     // unordered: kept
        return a;
    }
    // ---- bone tree ---------------------------------------------------------------
    void get(MotionBone& b){driving::pc_matrix_get(c.matrices,Bytes(reinterpret_cast<std::uint8_t*>(b.matrix),64));}
    MotionBone& child(const MotionBone& b,std::uint32_t i){return bone(m.u32(b.child_list+i*4u));}
    // 51A5F0(bone, t): TRS from its keys.
    void trs_51a5f0(MotionBone& b,float t){
        evaluate3_519920(b.channels+0,b.translation,t);
        evaluate3_519920(b.channels+3,b.rotation,t);
        evaluate3_519920(b.channels+6,b.scale,t);
        if(flags()&2u){
            blend_translation_51a3a0(b);
            blend_rotation_519dd0(b.rotation,b.saved_rotation,factor());
            blend3(b.scale,b.saved_scale);
        }
        translate(vec(b.translation));
        if(b.unit_rotation)unit_rotation();
        rotate_zyx(b.rotation[0],b.rotation[1],b.rotation[2]);
        scale_if_needed(b.scale);
    }
    static bool no_translation_keys(const MotionBone& b){return b.channels[0].keys==0u&&b.channels[1].keys==0u&&b.channels[2].keys==0u;}
    void default_translation(MotionBone& b){
        for(unsigned k=0;k<3;++k)std::memcpy(&b.translation[k],&b.channels[k].rest,4);
        translate(vec(b.translation));
    }
    // Scale keys, blend and the rescale of a joint after its IK rotation.
    void joint_scale(MotionBone& j,float t){
        evaluate3_519920(j.channels+6,j.scale,t);
        if(flags()&2u)blend3(j.scale,j.saved_scale);
        scale_if_needed(j.scale);
    }
    // 519870(v): x axis aimed at v (40AA50 then 40A9B0).
    void aim_519870(const CourseProbe& v){
        const float a=v.x,b=v.y,cz=v.z;
        const float s=b*b+a*a;
        const float inv=x87_float(X87(One)/driving::x87_sqrt(X87(s)));
        rotation_z_cs(inv*b,inv*a);
        const float inv2=x87_float(X87(One)/driving::x87_sqrt(X87(cz)*X87(cz)+X87(s)));
        const float co=(inv*s)*inv2;
        const float sn=0.0f-inv2*cz;
        rotation_y_cs(sn,co);
    }
    // IK target of the effector in the joint's space (51A88D..51AAA0 /
    // 51AF2F..51B13C): V = the effector keys (or override), world when flag 1.
    CourseProbe ik_target(MotionBone& eff,const CourseProbe& keys,std::uint32_t flag_bits){
        CourseProbe v=keys;
        if(flag_bits&1u){
            put_vec(eff.ik_target,point(v));
            if(flags()&2u)blend3(eff.ik_target,eff.saved_ik_target);
        }else{
            put_vec(eff.ik_target,v);
            push_unit();(void)point(vec(eff.ik_target));pop();     // 40A7D0 on a unit matrix, result dropped by the pop
            if(flags()&2u)blend3(eff.ik_target,eff.saved_ik_target);
        }
        v=vec(eff.ik_target);
        push();invert();v=point(v);pop();
        return v;
    }
    // 519920 into a local vector (the PC passes a stack address).
    CourseProbe evaluate3_to(MotionChannel* chs,float t){
        float o[3];
        evaluate3_519920(chs,o,t);
        return {o[0],o[1],o[2]};
    }
    // 51A770(bone, t, flags): two-bone IK (kind 2) / aim (kind 1).
    void ik_51a770(MotionBone& b,float t,std::uint32_t flag_bits){
        if(b.ik==2){
            MotionBone& j0=child(b,0);
            MotionBone& j1=child(j0,0);
            MotionBone& eff=child(j1,0);
            if(no_translation_keys(eff)){
                trs_51a5f0(j0,t);
                get(j0);
                trs_51a5f0(j1,t);get(j1);return;
            }
            default_translation(j0);
            CourseProbe keys{};
            if(eff.target_mode==1u)keys=vec(eff.target_override);
            else{
                keys=evaluate3_to(eff.channels,t);
                if(eff.target_mode==2u&&keys.y>=eff.target_override[1])keys.y=eff.target_override[1];
            }
            eff.target_mode=0;
            const CourseProbe v=ik_target(eff,keys,flag_bits);
            const float len=x87_float(driving::x87_sqrt((X87(v.z)*X87(v.z)+X87(v.y)*X87(v.y))+X87(v.x)*X87(v.x)));
            push_unit();
            aim_519870(v);
            const float l1=j1.channels[0].rest,l2=eff.channels[0].rest;   // bone lengths: the default x translation
            const std::int32_t bend=j1.ik;
            float ca=((l1*l1+len*len)-l2*l2)/((l1*len)*Two);
            if(ca>One)ca=One;else if(MinusOne>ca)ca=MinusOne;
            X87 sa=driving::x87_sqrt(X87(One)-X87(ca)*X87(ca));
            if(bend==0)sa=-sa;
            rotation_z_cs(x87_float(sa),ca);
            std::array<float,16> local=current();
            pop();
            driving::pc_matrix_multiply_current(c.matrices,Bytes(reinterpret_cast<std::uint8_t*>(local.data()),64));
            joint_scale(j0,t);
            get(j0);
            default_translation(j1);
            push_unit();
            float cb=((l1*l1+l2*l2)-len*len)/((l1*l2)*Two);
            if(cb>One)cb=One;else if(MinusOne>cb)cb=MinusOne;
            X87 sb=driving::x87_sqrt(X87(One)-X87(cb)*X87(cb));
            if(1-bend==0)sb=-sb;
            const float sbf=x87_float(sb);
            pop();
            rotation_z_cs(sbf,0.0f-cb);
            joint_scale(j1,t);
            get(j1);
            return;
        }
        if(b.ik==1){
            MotionBone& j0=child(b,0);
            MotionBone& eff=child(j0,0);
            if(no_translation_keys(eff)){trs_51a5f0(j0,t);get(j0);return;}
            default_translation(j0);
            const CourseProbe v=ik_target(eff,evaluate3_to(eff.channels,t),flag_bits);
            push_unit();aim_519870(v);pop();
            aim_519870(v);
            joint_scale(j0,t);
            get(j0);
        }
    }
    // 51B420(bone, t, flags).
    void bone_51b420(MotionBone& b,float t,std::uint32_t flag_bits){
        push();
        switch(b.kind){
        case 0:case 1:trs_51a5f0(b,t);break;
        case 2:trs_51a5f0(b,t);push();ik_51a770(b,t,flag_bits);pop();break;
        case 3:driving::pc_matrix_load(c.matrices,Bytes(reinterpret_cast<std::uint8_t*>(b.matrix),64));break;
        case 4:
            default_translation(b);
            if(b.unit_rotation)unit_rotation();
            evaluate3_519920(b.channels+3,b.rotation,t);
            if(flags()&2u)blend_rotation_519dd0(b.rotation,b.saved_rotation,factor());
            rotate_zyx(b.rotation[0],b.rotation[1],b.rotation[2]);
            joint_scale(b,t);
            break;
        default:break;
        }
        get(b);
        for(std::int32_t i=0;i<b.children;++i)bone_51b420(child(b,std::uint32_t(i)),t,flag_bits);
        pop();
    }
    // 51B6D0 / 51B650.
    void calc_bones(std::uint32_t set,float frame,bool blend,float fa){
        m.put32(Flags85b2e8,blend?2u:0u);
        if(blend)pf(Factor85b2bc,fa);
        if(set){
            pf(Offset85b2d4,0.0f);pf(Offset85b2d4+4,0.0f);pf(Offset85b2d4+8,0.0f);
            push_unit();
            bone_51b420(bone(m.u32(set+0x1c)),frame*Sixtieth,m.u16(set+0x16));
            pop();
        }
        if(blend)m.put32(Flags85b2e8,0);
    }
    // ---- pose save (51B3F0) ------------------------------------------------------
    CourseProbe offset(){return {f(Offset85b2d4),f(Offset85b2d4+4),f(Offset85b2d4+8)};}
    // 519E60(esi = bone).
    void save_519e60(MotionBone& b){
        if(flags()&4u){
            if(b.id!=m.u32(Bone85b2e0)&&b.id==m.u32(Bone85b2c0)){
                const CourseProbe o=offset();
                push_unit();
                translate({0.0f-o.x,0.0f-o.y,0.0f-o.z});
                rotate_y(0.0f-f(Angle85b2c4));
                translate(vec(b.translation));
                put_vec(b.saved_translation,driving::pc_matrix_translation(c.matrices));
                pop();
            }else copy3(b.saved_translation,b.translation);
        }
        if(flags()&4u)save_rotation(b);
        if(flags()&4u)copy3(b.saved_scale,b.scale);
    }
    // 519F2D.. / 51B295..: rotation save into +318.
    void save_rotation(MotionBone& b){
        float* out=b.saved_rotation;
        if(b.id==m.u32(Bone85b2e0))copy3(out,b.rotation);
        else if(b.id==m.u32(Bone85b2e4)){
            std::memcpy(&out[0],&b.rotation[0],4);
            out[1]=wrap_519c30(b.rotation[1]-f(Angle85b2c4));
            std::memcpy(&out[2],&b.rotation[2],4);
        }else if(b.id==m.u32(Bone85b2b8)){
            push_unit();
            rotate_y(0.0f-f(Angle85b2c4));
            rotate_zyx(b.rotation[0],b.rotation[1],b.rotation[2]);
            angles_519ce0(out,current());
            pop();
        }else copy3(out,b.rotation);
    }
    // IK target save (51A0A2.. / 51A264..) into eff +300.
    void save_ik_target(MotionBone& eff){
        const float a=f(Angle85b2c4);
        const bool equal=a==Zero619a34;                         // UCOMISS / LAHF / TEST AH,44h / JNP
        const CourseProbe o=offset();
        const float* p=eff.ik_target;
        if(!equal&&eff.id!=m.u32(Bone85b2c0)&&eff.id!=m.u32(Bone85b2b8)){
            push_unit();
            rotate_y(0.0f-a);
            translate({p[0]-o.x,p[1]-o.y,p[2]-o.z});
            put_vec(eff.saved_ik_target,driving::pc_matrix_translation(c.matrices));
            pop();
        }else{
            eff.saved_ik_target[0]=p[0]-o.x;
            eff.saved_ik_target[1]=p[1]-o.y;
            eff.saved_ik_target[2]=p[2]-o.z;
        }
    }
    // 51A050(eax = bone).
    void save_ik_51a050(MotionBone& b){
        if(b.ik==2){
            MotionBone& j0=child(b,0);
            MotionBone& j1=child(j0,0);
            MotionBone& eff=child(j1,0);
            if(no_translation_keys(eff)){save_519e60(j0);save_519e60(j1);return;}
            if(flags()&4u){
                save_ik_target(eff);
                if(flags()&4u)copy3(j0.saved_scale,j0.scale);
            }
            if(flags()&4u)copy3(j1.saved_scale,j1.scale);
            return;
        }
        if(b.ik==1){
            MotionBone& j0=child(b,0);
            MotionBone& eff=child(j0,0);
            if(no_translation_keys(eff)){save_519e60(j0);return;}          // 51A262 -> 51A1EE (after the MOV ESI,EBX): ESI = j0
            if(!(flags()&4u))return;
            save_ik_target(eff);
            if(flags()&4u)copy3(j0.saved_scale,j0.scale);
        }
    }
    // 51B260(bone).
    void save_51b260(MotionBone& b){
        switch(b.kind){
        case 0:case 1:save_519e60(b);break;
        case 2:save_519e60(b);save_ik_51a050(b);break;
        case 4:
            if(flags()&4u){
                save_rotation(b);
                if(flags()&4u)copy3(b.saved_scale,b.scale);
            }
            break;
        default:break;
        }
        for(std::int32_t i=0;i<b.children;++i)save_51b260(child(b,std::uint32_t(i)));
    }
    // ---- motion object ---------------------------------------------------------------
    std::uint32_t name_of(std::uint32_t id){
        const std::uint32_t list=m.u32(m.u32(0x84d970u)+((id>>16)<<4)+4u);
        return m.u32(list+(id&0xffffu)*4u);
    }
    std::uint32_t set_motion(std::uint32_t mot,std::uint32_t id){
        // Bridge 1039A80: ECX = &84DD68[id >> 16] (group data; 0 = not loaded).
        const std::uint32_t slot=0x84dd68u+std::uint32_t(std::int32_t(id)>>16)*4u;
        const std::uint32_t base=m.u32(slot);
        if(!base)return 0u;
        m.put32(mot+4,id);
        keys_520750(mot+0x58,base+(id&0x7fffu)*0x18u,base);
        m.put32(mot+0xa0,name_of(id));
        const std::int32_t frames=std::int16_t(m.u16(mot+0x6a));
        m.put32(mot+8,m.u32(mot+8)&0xffffffc2u);
        pf(mot+0xc,0.0f);pf(mot+0x10,0.0f);
        pf(mot+0x14,float(frames));pf(mot+0x1c,float(frames));
        m.put32(mot+0x9c,0);
        return 1u;
    }
    void set_frame(std::uint32_t mot,float fr){
        std::uint32_t fl=m.u32(mot+8)&0xfffffff7u;
        const std::uint32_t loops=m.u32(mot+0x9c);
        pf(mot+0x10,fr);m.put32(mot+8,fl);
        const std::uint32_t looping=(loops!=0u&&loops!=1u)?1u:0u;
        fl=((fl^looping)&1u)^fl;m.put32(mot+8,fl);
        if(fl&1u){
            if(fr>f(mot+0x14)){
                do{
                    const float v=f(mot+0x10)-f(mot+0x14);
                    m.put32(mot+8,m.u32(mot+8)|8u);pf(mot+0x10,v);
                }while(f(mot+0x10)>f(mot+0x14));
            }
            if(0.0f>f(mot+0x10)){
                float v=f(mot+0x10);std::uint32_t g=m.u32(mot+8);
                do{v=f(mot+0x14)+v;g|=8u;}while(0.0f>v);
                m.put32(mot+8,g);pf(mot+0x10,v);
            }
        }else{
            if(fr>f(mot+0x14)){
                do{m.put32(mot+0x10,m.u32(mot+0x14));m.put32(mot+8,m.u32(mot+8)|0xcu);}while(f(mot+0x10)>f(mot+0x14));
            }
            if(0.0f>f(mot+0x10)){
                std::uint32_t g=m.u32(mot+8);float v;
                do{v=0.0f;g|=0xcu;}while(0.0f>v);
                m.put32(mot+8,g);pf(mot+0x10,v);
            }
        }
        if((m.u32(mot+8)&8u)&&std::int32_t(loops)>0)m.put32(mot+0x9c,loops-1u);
    }
    void connect_step_4f2410(std::uint32_t mot,float t){
        const std::uint32_t loops=m.u32(mot+0x9c);
        std::uint32_t fl=(m.u32(mot+8)&0xfffffff6u)|(loops!=0u?1u:0u);
        pf(mot+0x2c,t);
        m.put32(mot+8,fl);
        if(t>=f(mot+0x38)){                                      // COMISS / JB (unordered: skip)
            m.put32(mot+8,fl&0xffffffefu);
            (void)set_motion(mot,m.u32(mot+0x24));
            set_frame(mot,f(mot+0x30));
            m.put32(mot+0x9c,loops);
        }
    }
    std::uint32_t connect(std::uint32_t mot,std::uint32_t id,float fr,float length){
        if(One>length)return set_motion(mot,id);
        const std::uint32_t set=mot+0x58;
        m.put32(mot+0x20,m.u32(mot+4));m.put32(mot+0x24,id);
        // 51B3F0: pose save with 85B2E8 = 4.
        m.put32(Flags85b2e8,4u);save_51b260(bone(m.u32(set+0x1c)));m.put32(Flags85b2e8,0u);
        const std::uint32_t base=m.u32(0x84dd68u+std::uint32_t(std::int32_t(id)>>16)*4u);
        keys_520750(set,base+(id&0x7fffu)*0x18u,base);
        const std::uint32_t name=name_of(id);
        std::uint32_t fl=m.u32(mot+8);
        m.put32(mot+4,id);
        fl=(fl&0xffffffd2u)|0x10u;
        pf(mot+0xc,fr);pf(mot+0x10,fr);pf(mot+0x30,fr);
        m.put32(mot+0xa0,name);
        pf(mot+0x28,0.0f);pf(mot+0x2c,0.0f);
        m.put32(mot+8,fl);
        pf(mot+0x38,length);
        m.put32(mot+0x34,m.u32(mot+0x18));
        for(std::uint32_t k=0x3c;k<=0x50;k+=4)pf(mot+k,0.0f);
        return fl;
    }
    void calc(std::uint32_t mot,float step){
        if(!m.u32(mot+0x54))return;
        const float d=x87_float(X87(step)*X87(f(mot+0x18)));
        if(!(m.u32(mot+8)&0x10u)){
            if(f(mot+0x10)>f(mot+0x1c))m.put32(mot+0x10,m.u32(mot+0x1c));
            const float fr=f(mot+0x10);
            pf(mot+0xc,fr);
            calc_bones(mot+0x58,fr,false,0.0f);
            set_frame(mot,f(mot+0xc)+d);
        }else{
            const float t=f(mot+0x2c);
            pf(mot+0x28,t);
            calc_bones(mot+0x58,f(mot+0xc),true,t/f(mot+0x38));
            connect_step_4f2410(mot,f(mot+0x28)+d);
        }
        m.put32(mot+8,m.u32(mot+8)|0x20u);
    }
};
}
std::uint32_t rob_motion_set_motion_4f2280(PcRaceContext& c,std::uint32_t motion,std::uint32_t id){return Engine(c).set_motion(motion,id);}
void rob_motion_set_frame_4f1d30(PcRaceContext& c,std::uint32_t motion,float frame){Engine(c).set_frame(motion,frame);}
std::uint32_t rob_motion_set_loop_4f1e60(PcRaceMemory& m,std::uint32_t motion,std::uint32_t n){m.put32(motion+0x9c,n);return n;}   // bridge 1039C40 (measured)
std::uint32_t rob_motion_connect_4f2320(PcRaceContext& c,std::uint32_t motion,std::uint32_t id,float frame,float length){return Engine(c).connect(motion,id,frame,length);}
void rob_motion_calc_4f2520(PcRaceContext& c,std::uint32_t motion){Engine(c).calc(motion,c.m.f32(0x842114u));}
std::uint32_t rob_motion_degenerate_blends(){return rob_motion_degenerate_blends_counter();}
}

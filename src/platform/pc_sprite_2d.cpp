#include "platform/pc_sprite_2d.hpp"
#include "driving/pc_d3dx.hpp"
#include "driving/pc_x87.hpp"
#include <cmath>
#include <cstring>
#include <stdexcept>
namespace outrun::platform {
namespace {
using driving::X87;
using driving::x87_float;
using M16=std::array<float,16>;
struct Fault { std::uint32_t pc,address; };
float fbits(std::uint32_t u){float f;std::memcpy(&f,&u,4);return f;}
std::uint32_t ubits(float f){std::uint32_t u;std::memcpy(&u,&f,4);return u;}
std::uint32_t rd32(const std::uint8_t* p){std::uint32_t v;std::memcpy(&v,p,4);return v;}
void wr32(std::uint8_t* p,std::uint32_t v){std::memcpy(p,&v,4);}
// fld m32 / fstp m32 of a value: exact, except that an SNaN is quieted (masked invalid).
float fld_fstp(float v){
    auto u=ubits(v);
    if((u&0x7f800000u)==0x7f800000u&&(u&0x7fffffu)&&!(u&0x400000u))u|=0x400000u;
    return fbits(u);
}
// _ftol2 (582194): 64-bit truncation, EAX = low dword.
std::uint32_t ftol2(X87 v){return std::uint32_t(std::uint64_t(driving::x87_ftol64(v)));}
// fild of an unsigned dword as the PC does it (signed load, + 2^32 when negative: 628070).
X87 fild_u(std::uint32_t v){X87 r{std::int32_t(v)};if(std::int32_t(v)<0)r+=X87(4294967296.0f);return r;}
// EXE constants (read from OR2006C2C.EXE).
constexpr float K1=1.0f;                       // 62806C
constexpr float K_frame_62818c=0.000244140625f; // 62818C (1/4096)
constexpr float K_percent_6281c0=0.00999999977648258209228515625f; // 6281C0
constexpr float K_radians_6281c4=0.01745329238474369049072265625f; // 6281C4
constexpr float K_255_5a91b8=255.0f;            // 5A91B8
constexpr float K_half_628064=0.5f;             // 628064
constexpr float K_texel_6281bc=0.509999990463256835937500f;      // 6281BC
constexpr float K_round_628204=0.001000000047497451305389404296875f; // 628204
constexpr float K3_6282a4=3.0f,K2_6280b0=2.0f;  // 6282A4, 6280B0
// Blend tables 626494 (D3DBLEND, indices 0..15: the .rdata words that follow
// the 11 entries) and 626480 (D3DBLENDOP, 0..7).
constexpr std::uint32_t Blend626494[16]{1,2,3,4,5,6,7,8,9,10,11,0x68747572u,0x6f667265u,0x75696472u,0x6du,0x7277616cu};
constexpr std::uint32_t BlendOp626480[8]{1,2,3,4,5,1,2,3};
M16 translation(float x,float y){M16 m{};m[0]=m[5]=m[10]=m[15]=1.0f;m[12]=x;m[13]=y;m[14]=0.0f;return m;}
M16 scaling(float x,float y,float z){M16 m{};m[0]=x;m[5]=y;m[10]=z;m[15]=1.0f;return m;}
void left_multiply(driving::PcMatrixStack& st,M16 m){driving::pc_matrix_multiply_current(st,driving::Bytes(m.data(),64));}
struct Ctx {
    PcSprite2dState& s;
    PcD3D9Device* d;                                       // null for the image producers
    driving::PcMatrixStack* st;
    std::uint32_t pc{};                                    // current PC function (fault report)
    // ---- SPRANI animation memory ----
    std::uint8_t* ani(std::uint32_t a,std::uint32_t n){
        if(a>=0x40000000u&&a<0x40000000u+(PcSpriteBankCount<<22)){
            auto& b=s.banks[(a-0x40000000u)>>22];const auto off=(a-0x40000000u)&0x3fffffu;
            if(!b.animation.empty()&&off<=b.animation.size()&&n<=b.animation.size()-off)return b.animation.data()+off;
        }
        throw Fault{pc,a};
    }
    std::uint32_t u32(std::uint32_t a){return rd32(ani(a,4));}
    std::int32_t i32(std::uint32_t a){return std::int32_t(u32(a));}
    std::uint16_t u16(std::uint32_t a){std::uint16_t v;std::memcpy(&v,ani(a,2),2);return v;}
    std::int16_t i16(std::uint32_t a){return std::int16_t(u16(a));}
    std::uint8_t u8(std::uint32_t a){return *ani(a,1);}
    float f32(std::uint32_t a){return fbits(u32(a));}
    void put16(std::uint32_t a,std::uint16_t v){std::memcpy(ani(a,2),&v,2);}
    void putf(std::uint32_t a,float v){wr32(ani(a,4),ubits(v));}
    // ---- queue nodes ----
    std::uint8_t* node(std::uint32_t a,std::uint32_t n){
        if(a>=Pc2dNodeBase&&a-Pc2dNodeBase<=s.nodes.size()&&n<=s.nodes.size()-(a-Pc2dNodeBase))return s.nodes.data()+(a-Pc2dNodeBase);
        throw Fault{pc,a};
    }
    std::uint32_t n32(std::uint32_t a){return rd32(node(a,4));}
    // A sprite record: a queue node's (+0x58) or a 986B34 mask stash entry.
    std::uint32_t r32(std::uint32_t a){
        if(a>=Pc2dMaskBase&&a-Pc2dMaskBase<=s.masks_986b34.size()&&4u<=s.masks_986b34.size()-(a-Pc2dMaskBase))
            return rd32(s.masks_986b34.data()+(a-Pc2dMaskBase));
        return n32(a);
    }
    float rf(std::uint32_t a){return fbits(r32(a));}
    void nput(std::uint32_t a,std::uint32_t v){wr32(node(a,4),v);}
    // ---- device state helpers (Get, compare, Set: the inline PC pattern) ----
    void rs(std::uint32_t state,std::uint32_t value){if(d->get_render_state(state)!=value)d->set_render_state(state,value);}
    void samp(std::uint32_t stage,std::uint32_t type,std::uint32_t value){if(d->get_sampler_state(stage,type)!=value)d->set_sampler_state(stage,type,value);}
    // ---- banks ----
    std::uint32_t texture_42a030(std::uint32_t token){
        const auto bank=(token>>16)&0xffffu;
        if(bank>=PcSpriteBankCount)return 0;
        const auto& b=s.banks[bank];
        if(b.state!=2u)return 0;
        const auto index=token&0xffffu;
        if(index>=b.textures.size())throw Fault{0x42a052u,token};      // the PC reads past its texture array
        return b.textures[index];
    }
};
// ------------------------------------------------------------------ producers
// 48BCF0: [c+4] <= frame < [c+8] (comiss: unordered passes).
bool window_48bcf0(Ctx& c,std::uint32_t comp,float frame){
    if(!comp)return false;
    if(c.f32(comp+4)>frame)return false;
    if(frame>=c.f32(comp+8))return false;
    return true;
}
// 48BCB0: layer flag 2 and first <= frame < last.
bool window_48bcb0(Ctx& c,std::uint32_t layer,float frame){
    if(!layer)return false;
    if(!(c.u8(layer+0x48)&2u))return false;
    if(float(c.i16(layer))>frame)return false;
    if(frame>=float(c.i16(layer+2)))return false;
    return true;
}
// 48B970: channel value at a frame (keys of 4 halves {t, v, in, out} from
// +6, cursor +2, count +0, shifts +4/+5; Hermite between keys).
X87 channel_48b970(Ctx& c,std::uint32_t ch,float frame){
    const auto saved=c.pc;c.pc=0x48b970u;
    const std::int32_t n4=std::int32_t(c.i16(ch))*4;
    auto time=[&](std::int32_t k){return c.i16(ch+6u+std::uint32_t(k*2));};
    if(float(time(c.i16(ch+2)))>frame){
        for(;;){
            auto u=c.u16(ch+2);
            if(std::int16_t(u)<=0)break;
            u=std::uint16_t(u+0xfffcu);c.put16(ch+2,u);
            if(!(float(time(std::int16_t(u)))>frame))break;
        }
    }
    if(frame>float(time(c.i16(ch+2)))){
        for(;;){
            auto u=c.u16(ch+2);
            if(std::int32_t(std::int16_t(u))>=n4)break;
            u=std::uint16_t(u+4u);c.put16(ch+2,u);
            if(!(frame>float(time(std::int16_t(u)))))break;
        }
    }
    const std::int32_t p1=std::int32_t(1u<<(c.u8(ch+4)&31u));
    const float scale=K1/float(p1);                                           // divss
    const auto u=c.u16(ch+2);const std::int32_t cu=std::int16_t(u);
    X87 result;
    if(cu>=n4){
        const auto nu=std::uint16_t(std::uint32_t(u)-4u);c.put16(ch+2,nu);
        result=X87(std::int32_t(c.i16(ch+8u+std::uint32_t(std::int32_t(std::int16_t(nu))*2))))*X87(scale);
    }else if(std::int16_t(u)<=0){
        c.put16(ch+2,0);
        result=X87(std::int32_t(c.i16(ch+8)))*X87(scale);
    }else{
        const std::uint32_t k=ch+std::uint32_t(cu*2);
        const std::int16_t pv=c.i16(k),cv=c.i16(k+8),ca=c.i16(k+0xa),pb=c.i16(k+4);
        const float pvs=float(pv)*scale;
        if(pv==cv&&pb==ca)result=X87(pvs);
        else{
            const std::int32_t pt=c.i16(k-2),ct=c.i16(k+6);
            const std::int32_t p2=std::int32_t(1u<<(c.u8(ch+5)&31u));
            const float dt=float(ct)-float(pt);
            const float uu=frame-float(pt);
            const float inv=K1/dt;
            const X87 T=X87(K1)/X87(p2);
            const float u2=uu*uu,inv2=inv*inv,s2=u2*inv2,k3s2=s2*K3_6282a4,s2u=s2*uu,u2inv=u2*inv;
            const float A=s2u-u2inv;
            const float k2s3=(s2u*inv)*K2_6280b0;
            const X87 H1=(X87(A)-X87(u2inv))+X87(uu);
            const X87 X1=H1*(X87(std::int32_t(pb))*T);
            const X87 cvs=X87(std::int32_t(cv))*X87(scale);
            const X87 h01=X87(k3s2)-X87(k2s3);
            const X87 S=X1+cvs*h01;
            const X87 S2=S+(X87(std::int32_t(ca))*T)*X87(A);
            const X87 h00=(X87(k2s3)-X87(k3s2))+X87(K1);
            result=S2+h00*X87(pvs);
        }
    }
    c.pc=saved;return result;
}
// 48BC00: evaluated channels of a layer (+18..+2C from the +30..+44 channels).
void channels_48bc00(Ctx& c,std::uint32_t layer,float frame){
    if(!layer)return;
    if(!c.u8(layer+0x49))return;
    for(std::uint32_t k=0;k<6;++k)
        if(c.u8(layer+0x49)&(1u<<k))c.putf(layer+0x18+k*4,x87_float(channel_48b970(c,c.u32(layer+0x30+k*4),frame)));
}
// 429150 / 4291D0: blend (9564E0..E8) and matrix push/pop.
void push_429150(Ctx& c){
    auto& s=c.s;
    if(s.depth_9564ec>=9)return;
    driving::pc_matrix_push(*c.st);
    const auto d=std::uint32_t(s.depth_9564ec);
    s.stack_956468[d]=fld_fstp(s.blend_9564e0[0]);
    s.stack_956490[d*2]=ubits(s.blend_9564e0[1]);s.stack_956490[d*2+1]=ubits(s.blend_9564e0[2]);
    ++s.depth_9564ec;
}
void pop_4291d0(Ctx& c){
    auto& s=c.s;
    if(s.depth_9564ec<=0)return;
    const auto d=std::uint32_t(--s.depth_9564ec);
    s.blend_9564e0[0]=fld_fstp(s.stack_956468[d]);
    s.blend_9564e0[1]=fbits(s.stack_956490[d*2]);s.blend_9564e0[2]=fbits(s.stack_956490[d*2+1]);
    driving::pc_matrix_pop(*c.st);
}
// 429220: layer matrix chain (parents first), top = T(-anchor) S Rz T(pos) top.
void matrix_429220(Ctx& c,std::uint32_t layer,float frame){
    if(!layer)return;
    matrix_429220(c,c.u32(layer+0xc),frame);
    channels_48bc00(c,layer,frame);
    const float rot=x87_float(X87(c.f32(layer+0x28))*X87(K_radians_6281c4));
    left_multiply(*c.st,translation(c.f32(layer+0x18),c.f32(layer+0x1c)));
    left_multiply(*c.st,pc_d3dx_rotation_z(rot));
    const float sy=x87_float(X87(c.f32(layer+0x24))*X87(K_percent_6281c0));
    const float sx=x87_float(X87(c.f32(layer+0x20))*X87(K_percent_6281c0));
    left_multiply(*c.st,scaling(sx,sy,1.0f));
    left_multiply(*c.st,translation(-c.f32(layer+0x10),-c.f32(layer+0x14)));
    auto& b=c.s.blend_9564e0;
    b[0]=x87_float(X87(b[0])+X87(rot));
    b[1]=x87_float(X87(b[1])*X87(c.f32(layer+0x20))*X87(K_percent_6281c0));
    b[2]=x87_float(X87(b[2])*X87(c.f32(layer+0x24))*X87(K_percent_6281c0));
}
// 48BBA0: current frame record of a footage (+14 = its texture token).
void frame_48bba0(Ctx& c,std::uint32_t f,float frame){
    if(!f)return;
    const auto n=c.i32(f+0xc);
    if(n<1){c.ani(f+0x14,4);wr32(c.ani(f+0x14,4),0xffffffffu);return;}
    if(n==1){wr32(c.ani(f+0x14,4),c.u32(c.u32(f+0x10)));return;}
    const float v=float(c.i32(f+8))*frame+K_round_628204;                    // cvtsi2ss, mulss, addss
    std::int32_t idx;
    if(!(v>=-2147483648.0f&&v<2147483648.0f))idx=std::int32_t(0x80000000u); // cvttss2si indefinite
    else idx=std::int32_t(v);
    if(idx<0)throw Fault{0x48bbdeu,std::uint32_t(idx)};                        // the PC indexes before the frame table
    const auto frames=c.u32(f+0x10);
    wr32(c.ani(f+0x14,4),idx<n?c.u32(frames+std::uint32_t(idx)*20u):c.u32(frames+std::uint32_t(n)*20u-20u));
}
// 42DD50 / the inline copies in 42CFE0/42D0C0: allocation from the pool.
std::uint32_t allocate_node(Ctx& c){
    auto& s=c.s;
    const auto node=Pc2dNodeBase+s.cursor_956bf4*Pc2dNodeBytes;
    ++s.cursor_956bf4;++s.count_95b220;
    if(s.cursor_956bf4>=Pc2dNodeCount)s.cursor_956bf4=0;
    c.nput(node,0);c.nput(node+4,0);c.nput(node+8,0);
    return node;
}
std::uint32_t node_42dd50(Ctx& c,std::uint32_t head){
    auto& s=c.s;
    if(s.update_index_8a8cdc!=0u||std::int32_t(s.count_95b220)>=0x230)return 0;
    const auto node=allocate_node(c);
    if(head){
        if(!c.n32(head))c.nput(head,node);
        c.nput(node+4,c.n32(head+4));c.nput(node,0);c.nput(node+8,head);
        if(const auto last=c.n32(head+4))c.nput(last,node);
        c.nput(head+4,node);
    }else{c.nput(node+4,0);c.nput(node,0);c.nput(node+8,node);}
    return node;
}
std::uint32_t list_head(Ctx& c,float layer){
    auto& s=c.s;
    const auto idx=std::int32_t(ftol2(X87(layer)));
    const std::uint32_t li=idx<0?0u:(idx>=0x14?0x14u:std::uint32_t(idx));
    auto head=s.heads_956c00[li];
    if(!head){
        if(s.update_index_8a8cdc!=0u||std::int32_t(s.count_95b220)>=0x230)return 0;
        head=allocate_node(c);
        c.nput(head+4,0);c.nput(head,0);c.nput(head+8,head);
        s.heads_956c00[li]=head;
    }
    return head;
}
// 42D0C0: sprite record (0xB8 bytes) into its layer list.
void queue_42d0c0(Ctx& c,const std::uint8_t* record,float layer){
    const auto saved=c.pc;c.pc=0x42d0c0u;
    const auto head=list_head(c,layer);
    if(head){
        if(const auto n=node_42dd50(c,head)){
            c.nput(n+0xc,1);
            std::memcpy(c.node(n+0x58,0xb8),record,0xb8);
            std::memset(c.node(n+0x10,0x48),0,0x48);
            ++c.s.sprite_nodes;
        }
    }
    c.pc=saved;
}
// 42CFE0: image record (0x48 bytes) into its layer list.
void queue_42cfe0(Ctx& c,const std::uint8_t* record,float layer){
    const auto saved=c.pc;c.pc=0x42cfe0u;
    const auto head=list_head(c,layer);
    if(head){
        if(const auto n=node_42dd50(c,head)){
            c.nput(n+0xc,0);
            std::memcpy(c.node(n+0x10,0x48),record,0x48);
            std::memset(c.node(n+0x58,0xb8),0,0xb8);
            ++c.s.image_nodes;
        }
    }
    c.pc=saved;
}
// 42DDF0: texture size (GetLevelDesc(0) Width/Height, fild unsigned).
void size_42ddf0(Ctx& c,std::uint32_t token,float& w,float& h){
    const auto tex=c.texture_42a030(token);
    std::uint32_t tw=0,th=0;
    if(!tex||!c.d||!c.d->texture_level_size(tex,0,tw,th))throw Fault{0x42de2bu,token};   // the PC calls through the texture object
    w=x87_float(fild_u(tw));h=x87_float(fild_u(th));
}
// 428BD0: footage record of a layer.
void footage_428bd0(Ctx& c,std::uint32_t f,std::uint32_t layer,float frame,float alpha,std::uint32_t list_layer){
    if(!layer||!f)return;
    const auto saved=c.pc;c.pc=0x428bd0u;
    auto& s=c.s;
    std::array<std::uint8_t,64> mat{};
    {auto top=c.st->current();for(unsigned k=0;k<64;k+=4)wr32(mat.data()+k,top.u32(k));}
    frame_48bba0(c,f,frame);
    const auto token=c.u32(f+0x14);
    if(token==0xffffffffu||c.i32(f+0xc)<=0){c.pc=saved;return;}
    std::array<std::uint8_t,Pc2dRecordBytes> r{};              // +0: uninitialised stack word on the PC (never read)
    auto put=[&](std::uint32_t o,std::uint32_t v){wr32(r.data()+o,v);};
    auto putf=[&](std::uint32_t o,float v){wr32(r.data()+o,ubits(v));};
    put(0xac,3);put(0xb0,3);
    std::memcpy(r.data()+0x14,mat.data(),64);
    put(0x10,1);
    put(4,(ftol2(X87(alpha)*X87(K_255_5a91b8))<<24)|(std::uint32_t(s.id_7551b4)&0xffffffu));
    const auto tex=c.texture_42a030(token);
    if(!tex){c.pc=saved;return;}
    put(0xc,tex);
    float w{},h{};size_42ddf0(c,token,w,h);
    const auto fr=c.u32(f+0x10);
    const X87 A=X87(K1)/X87(w);const float iw=x87_float(A);
    const X87 Ac=A*X87(c.f32(fr+0xc));
    putf(0x84,x87_float(Ac));
    const X87 B=X87(K1)/X87(h);const float ih=x87_float(B);
    const X87 Q=X87(K1)-B*X87(c.f32(fr+8));
    putf(0x88,x87_float(Q));
    putf(0x8c,x87_float(Ac));
    const X87 P=X87(K1)-X87(ih)*X87(c.f32(fr+0x10));
    putf(0x90,x87_float(P));
    const X87 R=X87(iw)*X87(c.f32(fr+4));
    put(0xa4,ubits(s.blend_9564e0[1]));put(0xa8,ubits(s.blend_9564e0[2]));
    const auto lf=c.u8(layer+0x48);
    std::uint32_t flags=(std::uint32_t(std::uint8_t(~lf))&4u)|0x41u;
    putf(0x94,x87_float(R));putf(0x98,x87_float(P));putf(0x9c,x87_float(R));putf(0xa0,x87_float(Q));
    putf(0x64,float(c.i32(f+4)));putf(0x6c,float(c.i32(f)));putf(0x78,float(c.i32(f)));putf(0x7c,float(c.i32(f+4)));
    if(s.flag_9564f4)flags|=0x100000u;else if(s.flag_9564f8)flags|=0x200000u;
    put(8,flags);
    const float lf32=float(std::int32_t(list_layer));
    if(lf&8u){
        if(!s.mask_986b28){
            std::memcpy(s.masks_986b34.data()+s.mask_986b30*Pc2dRecordBytes,r.data(),Pc2dRecordBytes);
            s.mask_986b28=1;s.mask_986b2c=ubits(alpha);c.pc=saved;return;
        }
    }
    if(s.mask_986b28){
        put(8,flags|0xc0000u);put(0xb4,Pc2dMaskBase+s.mask_986b30*Pc2dRecordBytes);
        queue_42d0c0(c,r.data(),lf32);
        ++s.mask_986b30;s.mask_986b28=0;if(s.mask_986b30>=0x30u)s.mask_986b30=0;
    }else queue_42d0c0(c,r.data(),lf32);
    c.pc=saved;
}
// 429350: one layer (footage or child component) under its matrix/blend.
void layer_429350(Ctx& c,std::uint32_t layer,float frame,float scale,std::uint32_t list_layer){
    if(!layer||!c.u32(layer+8))return;
    push_429150(c);
    matrix_429220(c,layer,frame);
    if(!window_48bcb0(c,layer,frame)&&!c.s.mask_986b28){pop_4291d0(c);return;}
    const std::int32_t e6=c.i16(layer+6),c0=c.i16(layer),d4=c.i16(layer+4);
    const bool footage=(c.u8(layer+0x48)&1u)!=0u;
    const float local=x87_float(((X87(frame)-X87(c0))*(X87(e6)*X87(K_frame_62818c)))+X87(d4));
    const float alpha=x87_float(X87(c.f32(layer+0x2c))*X87(K_percent_6281c0)*X87(scale));
    if(footage){footage_428bd0(c,c.u32(layer+8),layer,local,alpha,list_layer);pop_4291d0(c);return;}
    const auto comp=c.u32(layer+8);
    if(window_48bcf0(c,comp,local)){
        const auto count=c.i32(comp+0x1c);
        if(count-1>=0){
            std::uint32_t off=std::uint32_t(count-1)*0x4cu;
            for(std::uint32_t n=std::uint32_t(count);n;--n,off-=0x4cu)layer_429350(c,c.u32(comp+0x20)+off,local,alpha,list_layer);
        }
    }
    pop_4291d0(c);
}
// ---------------------------------------------------------------------- flush
void blend_42a2b0(Ctx& c,std::uint32_t mode){
    auto& s=c.s;
    const auto dst=mode&0xfu,src=(mode>>4)&0xfu,op=(mode>>8)&7u;
    if(dst!=s.blend_956bf8){s.blend_956bf8=dst;c.rs(0x14,Blend626494[dst]);}
    if(src!=s.blend_956b94){s.blend_956b94=src;c.rs(0x13,Blend626494[src]);}
    if(op!=s.blend_956b98){s.blend_956b98=op;c.rs(0xab,BlendOp626480[op]);}
}
void begin_429c60(Ctx& c,PcFlushContext& f,PcD3DXSprite* sprite,std::uint32_t layer_7d25f0){
    auto& s=c.s;
    s.blend_956bf8=s.blend_956b94=s.blend_956b98=0xffffffffu;
    render_reset_states_408880(f,layer_7d25f0);
    if(sprite){c.rs(0x18,0);sprite->begin(0);}
    c.samp(0,5,2);c.samp(0,6,2);c.samp(0,1,1);c.samp(0,2,1);c.samp(0,3,1);c.samp(0,7,0);
    c.d->set_vertex_shader(0);
    c.d->set_fvf(0x144);
    c.rs(0x16,1);c.rs(0x1b,1);c.rs(0x18,0);c.rs(0xa8,7);c.rs(0x17,8);c.rs(7,0);c.rs(0x34,0);
    s.w956bfc=0xff;
}
void end_429f60(Ctx& c,PcD3DXSprite* sprite){
    if(sprite){c.rs(0x14,6);c.rs(0x13,5);sprite->end();}
    c.rs(0x17,4);c.rs(7,1);
}
// 42A3A0: one sprite record as a DrawPrimitiveUP strip.
void draw_42a3a0(Ctx& c,std::uint32_t rec){
    auto& s=c.s;
    const auto tex=c.n32(rec+0xc);
    if(!tex)return;
    const auto colour=c.n32(rec+4),flags=c.n32(rec+8);
    blend_42a2b0(c,flags);
    c.d->set_texture(0,tex);
    std::uint32_t tw=0,th=0;
    if(!c.d->texture_level_size(tex,0,tw,th))throw Fault{0x42a3dcu,tex};
    const float hu=x87_float(X87(K_texel_6281bc)/fild_u(tw));
    const float hv=x87_float(X87(K_texel_6281bc)/fild_u(th));
    float nhu=-hu;const float nhv=-hv;
    if(flags&0x18000u){
        std::uint32_t w2=0,h2=0;
        if(!c.d->texture_level_size(tex,0,w2,h2))throw Fault{0x42a444u,tex};
        nhu=x87_float(fild_u(h2));                                         // [esp+0x28] = Height (as the PC stores it)
        c.samp(0,1,3);c.samp(0,2,3);c.samp(0,3,3);
    }else{
        c.samp(0,1,c.n32(rec+0xac));c.samp(0,2,c.n32(rec+0xb0));
    }
    M16 m{};for(unsigned k=0;k<16;++k)m[k]=fbits(c.n32(rec+0x14+k*4));
    auto& v=s.vertices_98b868;
    auto vput=[&](std::uint32_t o,float x){wr32(v.data()+o,ubits(x));};
    auto corner=[&](std::uint32_t o){return pc_d3dx_vec4_transform({fbits(c.n32(rec+o)),fbits(c.n32(rec+o+4)),fbits(c.n32(rec+o+8)),1.0f},m);};
    const X87 kx(s.screen_740c94),ky(s.screen_740c98),half(K_half_628064);
    auto p=corner(0x54);
    vput(0x08,p[2]);vput(0x00,x87_float(kx*X87(p[0])+half));vput(0x04,x87_float(ky*X87(p[1])+half));
    vput(0x14,x87_float(X87(hu)+X87(fbits(c.n32(rec+0x9c)))));vput(0x18,x87_float(X87(nhv)+X87(fbits(c.n32(rec+0xa0)))));
    vput(0x0c,1.0f);wr32(v.data()+0x10,colour);
    p=corner(0x60);
    vput(0x24,p[2]);vput(0x1c,x87_float(kx*X87(p[0])+half));vput(0x20,x87_float(ky*X87(p[1])+half));
    vput(0x30,x87_float(X87(hu)+X87(fbits(c.n32(rec+0x94)))));vput(0x34,x87_float(X87(hv)+X87(fbits(c.n32(rec+0x98)))));
    vput(0x28,1.0f);wr32(v.data()+0x2c,colour);
    p=corner(0x6c);
    vput(0x40,p[2]);vput(0x38,x87_float(kx*X87(p[0])+half));vput(0x3c,x87_float(ky*X87(p[1])+half));
    vput(0x4c,x87_float(X87(nhu)+X87(fbits(c.n32(rec+0x84)))));vput(0x50,x87_float(X87(nhv)+X87(fbits(c.n32(rec+0x88)))));
    vput(0x44,1.0f);wr32(v.data()+0x48,colour);
    p=corner(0x78);
    vput(0x5c,p[2]);
    const float px=x87_float(kx*X87(p[0])),py=x87_float(ky*X87(p[1]));   // stored, then + 0.5
    vput(0x54,x87_float(X87(px)+half));vput(0x58,x87_float(X87(py)+half));
    vput(0x68,x87_float(X87(nhu)+X87(fbits(c.n32(rec+0x8c)))));vput(0x6c,x87_float(X87(hv)+X87(fbits(c.n32(rec+0x90)))));
    vput(0x60,1.0f);wr32(v.data()+0x64,colour);
    c.d->draw_primitive_up(5,2,v.data(),0x1c);
    ++s.sprite_draws;++s.draws;
}
// 42A800: a masked sprite record (flag 0x40000) as one multi-texture
// DrawPrimitiveUP strip. Stage s (0 = the sprite, 1.. = the mask records
// chained through +0xB4) samples its record's texture; a mask stage's texture
// coordinates are the sprite corners taken into that mask's footage space
// (D3DXMatrixInverse of its matrix, then its crop scale/offset). The colour
// is the AND of the chained records' colours. With flag 0x80000 stage 1's
// alpha multiplies the sprite's (border-addressed, border colour 0), the
// states being saved in / restored from the 8606F0 shadow.
void latch(PcSprite2dState& s,std::uint32_t pc,std::uint32_t address);
void mask_42a800(Ctx& c,std::uint32_t rec){
    auto& s=c.s;auto& d=*c.d;
    const std::uint32_t tex=c.r32(rec+0xc);
    if(!tex)return;
    const auto saved=c.pc;c.pc=0x42a800u;
    std::uint32_t colour=c.r32(rec+4);const std::uint32_t flags=c.r32(rec+8);
    blend_42a2b0(c,flags);
    M16 m{};for(unsigned k=0;k<16;++k)m[k]=c.rf(rec+0x14+k*4);
    std::array<std::array<float,4>,4> P{};
    for(unsigned k=0;k<4;++k){const auto a=rec+0x54+k*12;P[k]=pc_d3dx_vec4_transform({c.rf(a),c.rf(a+4),c.rf(a+8),1.0f},m);}
    float HU[4]{},HV[4]{},NHU[4]{},NHV[4]{};
    float UV[4][4][2]{};                                    // [stage][corner slot][u,v] (esp+0x140)
    static constexpr unsigned Slot626470[4]{3,2,0,1};
    std::uint32_t count=0;
    for(std::uint32_t r=rec;r;r=c.r32(r+0xb4)){
        if(count>=4)throw Fault{0x42a8a0u,r};                // the PC's stack arrays hold 4 stages
        const auto t=c.r32(r+0xc);
        d.set_texture(count,t);
        std::uint32_t w=0,h=0;
        if(!d.texture_level_size(t,0,w,h))throw Fault{0x42a8c7u,t};
        const X87 a=X87(K_texel_6281bc)/fild_u(w),b=X87(K_texel_6281bc)/fild_u(h);
        HU[count]=x87_float(a);HV[count]=x87_float(b);NHU[count]=x87_float(-a);NHV[count]=x87_float(-b);
        if(count>=1){
            const float u0=c.rf(r+0x9c),v0=c.rf(r+0x88);
            const float su=x87_float((X87(c.rf(r+0x8c))-X87(u0))/(X87(c.rf(r+0x6c))-X87(c.rf(r+0x54))));
            const float sv=x87_float((X87(c.rf(r+0x90))-X87(v0))/(X87(c.rf(r+0x64))-X87(c.rf(r+0x58))));
            M16 mm{};for(unsigned k=0;k<16;++k)mm[k]=c.rf(r+0x14+k*4);
            driving::PcMatrix16 inv{};
            if(!driving::pc_d3dx_matrix_inverse(inv,mm))throw Fault{0x42a976u,r};   // the PC would use uninitialised stack
            M16 im{};for(unsigned k=0;k<16;++k)im[k]=inv[k];
            for(unsigned k=0;k<4;++k){
                const auto q=pc_d3dx_vec4_transform({P[k][0],P[k][1],P[k][2],1.0f},im);
                auto& uv=UV[count][Slot626470[k]];
                uv[0]=x87_float(X87(q[0])*X87(su)+X87(u0));
                uv[1]=x87_float(X87(q[1])*X87(sv)+X87(v0));
            }
        }else{
            for(unsigned k=0;k<4;++k){UV[0][k][0]=c.rf(r+0x84+k*8);UV[0][k][1]=c.rf(r+0x88+k*8);}
        }
        colour&=c.r32(r+4);
        ++count;
    }
    if(flags&0x18000u){
        std::uint32_t w=0,h=0;(void)d.texture_level_size(tex,0,w,h);           // GetLevelDesc, result unused
        c.samp(0,1,3);c.samp(0,2,3);c.samp(0,3,3);
    }else{
        c.samp(0,1,c.r32(rec+0xac));c.samp(0,2,c.r32(rec+0xb0));
    }
    auto& sh=s.shadow_8606f0;
    auto save_w=[&](std::uint32_t off,std::uint32_t v){std::memcpy(sh.data()+off,&v,4);};
    auto load_w=[&](std::uint32_t off){std::uint32_t v;std::memcpy(&v,sh.data()+off,4);return v;};
    auto samp_save=[&](std::uint32_t st,std::uint32_t type,std::uint32_t want,std::uint32_t off,std::uint32_t dirty){
        const auto v=d.get_sampler_state(st,type);if(v!=want)d.set_sampler_state(st,type,want);save_w(off,v);sh[dirty]=1;};
    auto tss_save=[&](std::uint32_t st,std::uint32_t type,std::uint32_t want,std::uint32_t off,std::uint32_t dirty){
        const auto v=d.get_texture_stage_state(st,type);if(v!=want)d.set_texture_stage_state(st,type,want);save_w(off,v);sh[dirty]=1;};
    auto samp_restore=[&](std::uint32_t st,std::uint32_t type,std::uint32_t off,std::uint32_t dirty){
        const auto v=load_w(off);if(d.get_sampler_state(st,type)!=v)d.set_sampler_state(st,type,v);sh[dirty]=0;};
    auto tss_restore=[&](std::uint32_t st,std::uint32_t type,std::uint32_t off,std::uint32_t dirty){
        const auto v=load_w(off);if(d.get_texture_stage_state(st,type)!=v)d.set_texture_stage_state(st,type,v);sh[dirty]=0;};
    const bool alpha_mask=(flags&0x80000u)!=0u;
    if(alpha_mask){
        samp_save(1,1,4,0x594,0x79d);samp_save(1,2,4,0x598,0x79e);samp_save(1,4,0,0x5a0,0x7a0);
        tss_save(0,5,1,0x35c,0x70f);tss_save(0,6,2,0x360,0x710);tss_save(0,4,4,0x358,0x70e);
        tss_save(1,2,2,0x3d4,0x72d);tss_save(1,5,1,0x3e0,0x730);tss_save(1,6,2,0x3e4,0x731);
        tss_save(1,1,2,0x3d0,0x72c);tss_save(1,4,4,0x3dc,0x72f);
    }
    if(count>=2&&count<=4){
        // FVF XYZRHW|DIFFUSE|TEXn: x,y,z,rhw,colour, then (u,v) per stage. Four stages
        // (three chained masks, 42BA3E..42BB4E on the Steam build) take the same path
        // with FVF 0x444 and a 0x34-byte vertex.
        const std::uint32_t stride=20u+8u*count;
        std::array<std::uint8_t,4*0x34> v{};
        const X87 kx(s.screen_740c94),ky(s.screen_740c98),half(K_half_628064);
        // Vertex k: corner k; stage uv = +-half texel + the corner's slot.
        static constexpr unsigned Slot[4]{3,2,0,1};
        for(unsigned k=0;k<4;++k){
            auto* q=v.data()+k*stride;
            wr32(q+0,ubits(x87_float((X87(P[k][0])+half)*kx)));
            wr32(q+4,ubits(x87_float((X87(P[k][1])+half)*ky)));
            wr32(q+8,ubits(P[k][2]));wr32(q+12,ubits(1.0f));wr32(q+16,colour);
            for(unsigned st=0;st<count;++st){
                const bool hu_pos=k<2,hv_pos=(k&1u)!=0u;               // V0 (+u,-v) V1 (+u,+v) V2 (-u,-v) V3 (-u,+v)
                const auto& uv=UV[st][Slot[k]];
                wr32(q+20+st*8,ubits(x87_float(X87(hu_pos?HU[st]:NHU[st])+X87(uv[0]))));
                wr32(q+24+st*8,ubits(x87_float(X87(hv_pos?HV[st]:NHV[st])+X87(uv[1]))));
            }
        }
        // Stage 1..n-1 take their own coordinate set (TEXCOORDINDEX = stage).
        static constexpr std::uint32_t Tci[4][2]{{0,0},{0x3f8,0x736},{0x47c,0x757},{0x500,0x778}};
        for(std::uint32_t st=1;st<count;++st)tss_save(st,0xb,st,Tci[st][0],Tci[st][1]);
        d.set_vertex_shader(0);
        d.set_fvf(count==2u?0x244u:count==3u?0x344u:0x444u);
        d.draw_primitive_up(5,2,v.data(),stride);
        ++s.draws;++s.sprite_draws;
        for(std::uint32_t st=1;st<count;++st)tss_restore(st,0xb,Tci[st][0],Tci[st][1]);
    }
    if(alpha_mask){
        samp_restore(1,1,0x594,0x79d);samp_restore(1,2,0x598,0x79e);samp_restore(1,4,0x5a0,0x7a0);
        tss_restore(0,5,0x35c,0x70f);tss_restore(0,6,0x360,0x710);tss_restore(0,4,0x358,0x70e);
        tss_restore(1,2,0x3d4,0x72d);tss_restore(1,5,0x3e0,0x730);tss_restore(1,6,0x3e4,0x731);
        tss_restore(1,1,0x3d0,0x72c);tss_restore(1,4,0x3dc,0x72f);
    }
    for(std::uint32_t st=0;st<count;++st)d.set_texture(st,0);
    c.pc=saved;
}
// 42BE60 (ESI = record, EDI = four XYZRHW|DIFFUSE|TEX1 vertices of 0x1C): the
// record's rectangle (+4..+10) around its centre (flag 4: 0, flag 8: half the
// size, else +1C/+20), scaled (+14/+18), rotated (+30), moved (+24/+28) and
// shifted by half a pixel; UV from the texture's level-0 size (flag 0x400: the
// whole texture), flags 2 / 1 swap the V / U ends.
void quad_42be60(Ctx& c,std::uint32_t rec,std::array<std::uint8_t,4*0x1c>& v){
    const auto tex=c.n32(rec+0x38);
    std::uint32_t W=0,H=0;
    if(!c.d->texture_level_size(tex,0,W,H))throw Fault{0x42be72u,tex};
    const auto r=[&](std::uint32_t o){return fbits(c.n32(rec+o));};
    const float fw=x87_float(X87(std::int32_t(c.n32(rec+0xc)-c.n32(rec+4))));
    const X87 fh=X87(std::int32_t(c.n32(rec+0x10)-c.n32(rec+8)));
    const auto flags=c.n32(rec+0x34);
    X87 cx,cy;
    if(flags&4u){cx=X87(0.0f);cy=X87(0.0f);}
    else if(flags&8u){cx=X87(fw)*X87(K_half_628064);cy=fh*X87(K_half_628064);}
    else{cx=X87(r(0x1c));cy=X87(r(0x20));}
    const float sn=x87_float(driving::x87_sin(X87(r(0x30)))),cs=x87_float(driving::x87_cos(X87(r(0x30))));
    const X87 sx(r(0x14)),sy(r(0x18)),tx(r(0x24)),ty(r(0x28)),half(K_half_628064);
    const std::uint32_t colour=c.n32(rec+0x2c);
    auto put=[&](unsigned k,X87 x,X87 y){
        auto* q=v.data()+k*0x1c;
        wr32(q,ubits(x87_float(((x*X87(cs)-y*X87(sn))+tx)-half)));
        wr32(q+4,ubits(x87_float(((y*X87(cs)+x*X87(sn))+ty)-half)));
        wr32(q+8,0u);wr32(q+0xc,ubits(1.0f));wr32(q+0x10,colour);};
    const float ncx=x87_float(-cx);
    const X87 wcx=X87(fw)-cx;const float wcx_f=x87_float(wcx);
    const X87 y01=-cy*sy;
    put(0,-cx*sx,y01);
    put(1,X87(x87_float(wcx*sx)),y01);
    const X87 hcy=fh-cy,y23=hcy*sy;
    put(2,X87(x87_float(X87(wcx_f)*sx)),y23);
    put(3,X87(ncx)*sx,y23);
    X87 iw,ih;
    if(flags&0x400u){iw=X87(1.0f);ih=X87(1.0f);}
    else{
        X87 a=X87(std::int32_t(W));if(std::int32_t(W)<0)a=a+X87(4294967296.0f);iw=X87(1.0f)/a;
        X87 b=X87(std::int32_t(H));if(std::int32_t(H)<0)b=b+X87(4294967296.0f);ih=X87(1.0f)/b;
    }
    float v0=x87_float(X87(std::int32_t(c.n32(rec+8)))*ih),v1=x87_float(X87(std::int32_t(c.n32(rec+0x10)))*ih);
    float u0=x87_float(X87(std::int32_t(c.n32(rec+4)))*iw);
    X87 u1=X87(std::int32_t(c.n32(rec+0xc)))*iw;
    if(flags&2u)std::swap(v0,v1);
    if(flags&1u){const float t=x87_float(u1);u1=X87(u0);u0=t;}
    const float u1f=x87_float(u1);
    auto uv=[&](unsigned k,float u,float vv){auto* q=v.data()+k*0x1c;wr32(q+0x14,ubits(u));wr32(q+0x18,ubits(vv));};
    uv(0,u0,v0);uv(1,u1f,v0);uv(2,u1f,v1);uv(3,u0,v1);
}
// 42C0F0 (EAX = record of flags 0x100): stage-0 addressing forced to 3 (the
// saved values are not restored), the 42BE60 quad drawn with the record's
// texture, or with its four textures (+38..+44) when flag 0x10000 is set.
void multi_42c0f0(Ctx& c,std::uint32_t rec){
    auto& d=*c.d;
    std::uint32_t saved[3];
    for(std::uint32_t t=1;t<=3;++t)saved[t-1]=d.get_sampler_state(0,t);
    for(std::uint32_t t=1;t<=3;++t)
        if(saved[t-1]!=3u&&d.get_sampler_state(0,t)!=3u)d.set_sampler_state(0,t,3);
    std::array<std::uint8_t,4*0x1c> v{};
    quad_42be60(c,rec,v);
    const bool four=(c.n32(rec+0x34)&0x10000u)!=0u;
    const std::uint32_t n=four?4u:1u;
    for(std::uint32_t st=0;st<n;++st)d.set_texture(st,c.n32(rec+0x38+st*4));
    d.set_vertex_shader(0);d.set_fvf(0x144);
    d.draw_primitive_up(5,2,v.data(),0x1c);
    ++c.s.draws;
    for(std::uint32_t st=0;st<n;++st)d.set_texture(st,0);
}
// 42A0A0: one image record through the ID3DXSprite.
void image_42a0a0(Ctx& c,PcD3DXSprite* sprite,std::uint32_t rec){
    if(!sprite)return;
    auto& s=c.s;
    auto tex=c.n32(rec+0x38);
    if(!tex){
        const auto token=c.n32(rec);
        const auto bank=(token>>16)&0xffffu;
        if(bank>=PcSpriteBankCount||s.banks[bank].state!=2u)return;
        const auto index=token&0xffffu;
        if(index>=s.banks[bank].textures.size())throw Fault{0x42a0f3u,token};
        tex=s.banks[bank].textures[index];
        if(!tex)return;
    }
    const auto flags=c.n32(rec+0x34);
    std::uint32_t W=0,H=0;
    if(!c.d->texture_level_size(tex,0,W,H))throw Fault{0x42a116u,tex};
    const std::int32_t left=std::int32_t(c.n32(rec+4)),top=std::int32_t(c.n32(rec+8));
    std::int32_t right=std::int32_t(c.n32(rec+0xc)),bottom=std::int32_t(c.n32(rec+0x10));
    if(left==0&&right==0)right=std::int32_t(W);
    if(top==0&&bottom==0)bottom=std::int32_t(H);
    const std::int32_t rect_top=std::int32_t(H)-bottom,rect_bottom=std::int32_t(H)-top;
    if(right-left>2)--right;
    const std::int32_t rect[4]{left,rect_top,right,rect_bottom};
    const std::int32_t rw=right-left,rh=rect_bottom-rect_top;
    const std::uint32_t e=flags^2u;
    const float sx=fbits(c.n32(rec+0x14)),sy_rec=fbits(c.n32(rec+0x18));
    float sy=sy_rec;
    if(e&2u)sy=-sy_rec;
    const X87 ssx=(e&1u)?-X87(sx):X87(sx);
    X87 cx,cy;
    if(flags&4u){cx=X87(0.0f);cy=X87(0.0f);}                                 // 619A34
    else if(flags&8u){cx=X87(rw)*X87(K_half_628064);cy=X87(rh)*X87(K_half_628064);}
    else{cx=X87(fbits(c.n32(rec+0x1c)));cy=X87(fbits(c.n32(rec+0x20)));}
    const float tx=x87_float(X87(fbits(c.n32(rec+0x24)))-cx*X87(sx));
    X87 ty=X87(fbits(c.n32(rec+0x28)))-cy*X87(sy_rec);
    if(e&2u)ty=ty+X87(rh)*X87(sy_rec);
    const X87 TX=(e&1u)?X87(rw)*X87(sx)+X87(tx):X87(tx);
    const X87 k94(s.screen_740c94),k98(s.screen_740c98);
    const float scal[2]{x87_float(k94*ssx),x87_float(k98*X87(sy))};
    const float trans[2]{x87_float(k94*TX),x87_float(k98*ty)};
    const auto m=pc_d3dx_transformation_2d(scal,fbits(c.n32(rec+0x30)),trans);
    sprite->set_transform(m.data());
    sprite->draw(tex,rect,nullptr,nullptr,c.n32(rec+0x2c));
    ++s.draws;
}
}
// ------------------------------------------------------------------ API
PcSprite2dState::PcSprite2dState():nodes(std::size_t(Pc2dNodeCount)*Pc2dNodeBytes,0),masks_986b34(std::size_t(Pc2dMaskCount)*Pc2dRecordBytes,0){
    blend_9564e0={0.0f,1.0f,1.0f};
}
std::uint8_t* PcSprite2dState::node(std::uint32_t a){
    if(a<Pc2dNodeBase||a-Pc2dNodeBase>=nodes.size())return nullptr;
    return nodes.data()+(a-Pc2dNodeBase);
}
bool PcSprite2dState::anim_ok(std::uint32_t a,std::uint32_t n){
    if(a<0x40000000u||a>=0x40000000u+(PcSpriteBankCount<<22))return false;
    const auto& b=banks[(a-0x40000000u)>>22];const auto off=(a-0x40000000u)&0x3fffffu;
    return !b.animation.empty()&&off<=b.animation.size()&&n<=b.animation.size()-off;
}
std::uint32_t pc_sprani_scene_root(const PcSprite2dState& s,std::uint32_t token){
    const auto bank=token>>16,index=token&0xffffu;
    if(bank>=PcSpriteBankCount)return 0;
    const auto& a=s.banks[bank].animation;
    if(a.empty()||std::size_t(index)*4+4>a.size())return 0;
    const auto base=PcSpraniAnimationBase(bank);
    const auto scene=rd32(a.data()+index*4);
    if(!scene||scene<base||scene-base+8>a.size())return 0;
    const auto count=rd32(a.data()+(scene-base)),comps=rd32(a.data()+(scene-base)+4);
    if(!count)return 0;
    return comps+(count-1)*0x24u;
}
void pc_sprani_render_429460(PcSprite2dState& s,PcD3D9Device& d,driving::PcMatrixStack& st,std::uint32_t root,float frame,float scale,std::uint32_t layer){
    Ctx c{s,&d,&st,0x429460u};
    const auto depth=st.depth;const auto offset=st.current_offset;const auto blend=s.blend_9564e0;const auto bdepth=s.depth_9564ec;
    try{
        if(!window_48bcf0(c,root,frame))return;
        const auto count=c.i32(root+0x1c);
        if(count-1<0)return;
        std::uint32_t off=std::uint32_t(count-1)*0x4cu;
        for(std::uint32_t n=std::uint32_t(count);n;--n,off-=0x4cu)layer_429350(c,c.u32(root+0x20)+off,frame,scale,layer);
    }catch(const Fault& f){
        ++s.missing_counts[f.pc];if(!s.missing){s.missing=f.pc;s.fault=f.address;}
        st.depth=depth;st.current_offset=offset;s.blend_9564e0=blend;s.depth_9564ec=bdepth;   // aborted: the caller's stack as entered
    }catch(const std::exception&){
        ++s.missing_counts[0x429460u];if(!s.missing){s.missing=0x429460u;s.fault=root;}
        st.depth=depth;st.current_offset=offset;s.blend_9564e0=blend;s.depth_9564ec=bdepth;
    }
}
// 429010 (see the header).
void pc_sprite_canvas_429010(PcSprite2dState& s,driving::PcMatrixStack& st,std::uint32_t root,const std::array<float,16>& matrix){
    Ctx c{s,nullptr,&st,0x429010u};
    const float A=x87_float(X87(std::int32_t(c.i16(root)))*X87(K_half_628064));
    const float B=x87_float(X87(std::int32_t(c.i16(root+2)))*X87(K_half_628064));
    auto top=st.current();
    for(unsigned k=0;k<16;++k)top.put32(k*4,(k%5u)==0u?0x3f800000u:0u);
    auto translate=[&](float x,float y){M16 t{};t[0]=t[5]=t[10]=t[15]=1.0f;t[12]=x;t[13]=y;t[14]=0.0f;left_multiply(st,t);};
    const float y1=x87_float(X87(240.0f)-X87(B)),x1=x87_float(X87(320.0f)-X87(A));   // 6281CC / 6281C8
    translate(x1,y1);
    translate(A,B);
    {M16 m=matrix;left_multiply(st,m);}
    translate(x87_float(-X87(A)),x87_float(-X87(B)));
    s.blend_9564e0={0.0f,1.0f,1.0f};
}
// 428170 (see the header).
void pc_sprite_pool_display_428170(PcSprite2dState& s,PcD3D9Device& d,driving::PcMatrixStack& st,FrontendSprites& pool){
    const auto& inst=pool.instances();
    for(unsigned layer=0;layer<FrontendSprites::Layers;++layer){
        const unsigned count=pool.used(layer);
        for(unsigned slot=0,seen=0;seen<count&&slot<FrontendSprites::Slots;++slot){
            const auto h=layer*FrontendSprites::Slots+slot;
            const auto& in=inst[h];
            if(!in.allocated||!in.visible)continue;
            ++seen;
            s.id_7551b4=in.id_2c;
            const auto root=pc_sprani_scene_root(s,in.token);
            if(!root){++s.pool_missing_roots;++s.missing_counts[0x4281ceu];continue;}
            s.mask_986b28=0;
            driving::pc_matrix_push(st);
            try{pc_sprite_canvas_429010(s,st,root,in.matrix);}
            catch(const Fault& f){++s.missing_counts[f.pc];if(!s.missing){s.missing=f.pc;s.fault=f.address;}driving::pc_matrix_pop(st);continue;}
            pc_sprani_render_429460(s,d,st,root,in.frame,1.0f,layer);
            driving::pc_matrix_pop(st);
            ++s.pool_draws;
            pool.drawn(h);                                        // mode 4: released after one draw
        }
    }
}
namespace {
void latch(PcSprite2dState& s,std::uint32_t pc,std::uint32_t address){
    ++s.missing_counts[pc];if(!s.missing){s.missing=pc;s.fault=address;}
}
// 42C2F0 (bridge 40EAF0: bank = (token >> 16) & 0xFFFF): image table entry
// into rec +0 (texture token), +4/+8/+C/+10 (left, top, right, bottom).
void image_42c2f0(PcSprite2dState& s,std::uint8_t* rec,std::uint32_t token){
    const auto bank=(token>>16)&0xffffu;
    if(bank>=PcSpriteBankCount)return;
    const auto& b=s.banks[bank];
    if(b.header.size()<0x20)return;                                          // [956D8C + bank*0x2C] == 0
    const auto index=token&0xffffu;
    if(rd32(b.header.data()+0x18)<=index)return;
    const auto at=std::size_t(rd32(b.header.data()+0x1c))+std::size_t(index)*0x1c;
    if(at+0x1c>b.header.size())throw Fault{0x42c324u,token};
    const auto* e=b.header.data()+at;
    auto h16=[&](unsigned o){return std::uint32_t(std::uint16_t(e[o]|(e[o+1]<<8)));};
    wr32(rec,(bank<<16)|rd32(e));wr32(rec+8,h16(0x16));wr32(rec+4,h16(0x14));wr32(rec+0x10,h16(0x1a));wr32(rec+0xc,h16(0x18));
}
}
void pc_image_42d280(PcSprite2dState& s,std::uint32_t token,std::int32_t x,std::int32_t y,std::uint32_t flip,float layer,std::uint32_t colour){
    Ctx c{s,nullptr,nullptr,0x42d280u};
    try{
        std::array<std::uint8_t,0x48> r{};                                      // bridge 448102: ecx = [1039E80] = 0x12 dwords zeroed
        wr32(r.data()+0x14,ubits(1.0f));wr32(r.data()+0x18,ubits(1.0f));
        image_42c2f0(s,r.data(),token);
        wr32(r.data()+0x24,ubits(float(x)));wr32(r.data()+0x2c,colour);wr32(r.data()+0x28,ubits(float(y)));
        if(flip&1u)wr32(r.data()+0x34,rd32(r.data()+0x34)|1u);
        if(flip&2u)wr32(r.data()+0x34,rd32(r.data()+0x34)|2u);
        queue_42cfe0(c,r.data(),layer);
    }catch(const Fault& f){latch(s,f.pc,f.address);}
}
void pc_image_42d200(PcSprite2dState& s,std::uint32_t token,std::int32_t x,std::int32_t y,float scale_x,std::uint32_t flip,float layer,std::uint32_t colour){
    Ctx c{s,nullptr,nullptr,0x42d200u};
    try{
        std::array<std::uint8_t,0x48> r{};                                      // rep stos 0x12 dwords
        wr32(r.data()+0x18,ubits(1.0f));
        image_42c2f0(s,r.data(),token);
        wr32(r.data()+0x2c,colour);wr32(r.data()+0x24,ubits(float(x)));wr32(r.data()+0x14,ubits(scale_x));wr32(r.data()+0x28,ubits(float(y)));
        if(flip&1u)wr32(r.data()+0x34,rd32(r.data()+0x34)|1u);
        if(flip&2u)wr32(r.data()+0x34,rd32(r.data()+0x34)|2u);
        queue_42cfe0(c,r.data(),layer);
    }catch(const Fault& f){latch(s,f.pc,f.address);}
}
void pc_image_42d300(PcSprite2dState& s,std::uint32_t mode,std::uint32_t token,std::int32_t x,std::int32_t y,float width,float height,float layer,std::uint32_t colour){
    Ctx c{s,nullptr,nullptr,0x42d300u};
    try{
        std::array<std::uint8_t,0x48> t{};image_42c2f0(s,t.data(),token);
        std::int32_t left=std::int32_t(rd32(t.data()+4)),top=std::int32_t(rd32(t.data()+8));
        const std::int32_t right=std::int32_t(rd32(t.data()+0xc)),bottom=std::int32_t(rd32(t.data()+0x10));
        std::array<std::uint8_t,Pc2dRecordBytes> r{};
        wr32(r.data(),rd32(t.data()));wr32(r.data()+4,colour);wr32(r.data()+8,0x45u);
        {   const auto bank=(rd32(t.data())>>16)&0xffffu;std::uint32_t tex=0;
            if(bank<PcSpriteBankCount&&s.banks[bank].state==2u){
                const auto index=rd32(t.data())&0xffffu;
                if(index>=s.banks[bank].textures.size())throw Fault{0x42d3f9u,rd32(t.data())};   // the PC reads past its texture array
                tex=s.banks[bank].textures[index];}
            wr32(r.data()+0xc,tex);}
        wr32(r.data()+0x10,1u);
        for(unsigned k=0;k<16;++k)wr32(r.data()+0x14+k*4,(k%5u)==0u?0x3f800000u:0u);
        wr32(r.data()+0xac,3u);wr32(r.data()+0xb0,3u);wr32(r.data()+0xb4,0u);
        std::int32_t A=right,B=bottom;                                          // jump table 42D5B0
        switch(mode){
        case 0:A=right;B=bottom;break;
        case 1:A=left;B=bottom;break;
        case 2:top=bottom;A=right;B=bottom;break;
        case 3:left+=3;top+=3;A=left;B=top;break;
        default:A=right;B=bottom;break;
        }
        constexpr float KU=0x1.fe01fep-8f,KV=0x1.fc07fp-7f;                   // 6281B4 (1/128.5), 6281B0 (1/64.5)
        const float uA=x87_float(X87(A)*X87(KU)),vT=x87_float(X87(K1)-X87(top)*X87(KV)),vB=x87_float(X87(K1)-X87(B)*X87(KV)),uL=x87_float(X87(left)*X87(KU));
        auto putf=[&](std::uint32_t o,float v){wr32(r.data()+o,ubits(v));};
        putf(0x84,uA);putf(0x88,vT);putf(0x8c,uA);putf(0x90,vB);putf(0x94,uL);putf(0x98,vB);putf(0x9c,uL);putf(0xa0,vT);
        const float fx=float(x),fy=float(y);
        const float yh=x87_float(X87(height)+X87(fy)),xw=x87_float(X87(fx)+X87(width));
        putf(0x54,fx);putf(0x58,fy);putf(0x60,fx);putf(0x64,yh);putf(0x6c,xw);putf(0x70,fy);putf(0x78,xw);putf(0x7c,yh);
        queue_42d0c0(c,r.data(),layer);
    }catch(const Fault& f){latch(s,f.pc,f.address);}
}
void pc_image_entry_42c2f0(PcSprite2dState& s,std::uint8_t* rec,std::uint32_t token){image_42c2f0(s,rec,token);}
void pc_image_record_42cfe0(PcSprite2dState& s,const std::array<std::uint8_t,0x48>& record,float layer){
    Ctx c{s,nullptr,nullptr,0x42cfe0u};
    try{queue_42cfe0(c,record.data(),layer);}catch(const Fault& f){latch(s,f.pc,f.address);}
}
void pc_image_group_42d5f0(PcSprite2dState& s,std::uint32_t token,std::int32_t x,std::int32_t y,float layer,std::uint32_t colour,std::uint32_t flags){
    Ctx c{s,nullptr,nullptr,0x42d5f0u};
    const auto bank=(token>>16)&0xffffu;
    if(bank>=0x4bu)return;
    const auto& b=s.banks[bank];
    if(b.header.size()<0x20)return;
    const auto index=token&0xffffu;
    if(rd32(b.header.data()+0x10)<=index)return;
    try{
        const auto group=std::size_t(rd32(b.header.data()+0x14))+std::size_t(index)*8;
        if(group+8>b.header.size())throw Fault{0x42d634u,token};
        std::uint32_t count=rd32(b.header.data()+group);
        std::size_t entry=rd32(b.header.data()+group+4);
        std::array<std::uint8_t,0x48> r{};
        wr32(r.data()+0x14,ubits(1.0f));wr32(r.data()+0x18,ubits(1.0f));wr32(r.data()+0x2c,0xffffffffu);
        for(;count;--count,entry+=0x10){
            if(entry+0x10>b.header.size())throw Fault{0x42d680u,token};
            const auto* e=b.header.data()+entry;
            image_42c2f0(s,r.data(),rd32(e)|(bank<<16));
            const auto dx=std::uint32_t(std::uint16_t(e[4]|(e[5]<<8))),dy=std::uint32_t(std::uint16_t(e[6]|(e[7]<<8)));
            const auto fl=std::uint16_t(e[0xa]|(e[0xb]<<8));
            wr32(r.data()+0x24,ubits(float(std::int32_t(dx+std::uint32_t(x)))));
            wr32(r.data()+0x2c,colour);
            std::uint32_t f=flags;
            wr32(r.data()+0x34,f);
            wr32(r.data()+0x28,ubits(float(std::int32_t(dy+std::uint32_t(y)))));
            if(fl&1u){f|=1u;wr32(r.data()+0x34,f);}
            if(fl&2u){f|=2u;wr32(r.data()+0x34,f);}
            queue_42cfe0(c,r.data(),layer);
        }
    }catch(const Fault& f){latch(s,f.pc,f.address);}
}
void pc_2d_flush_42d710(PcSprite2dState& s,PcFlushContext& f,PcD3DXSprite* sprite,std::uint32_t first,std::uint32_t last,std::uint32_t layer_7d25f0){
    Ctx c{s,&f.device,nullptr,0x42d710u};
    if(first>=last)return;
    if(!s.sprite_95b218)sprite=nullptr;
    for(std::uint32_t li=first;li<last;++li){
        try{
            begin_429c60(c,f,sprite,layer_7d25f0);
            if(li<Pc2dLayerCount&&s.heads_956c00[li]){
                const auto head=s.heads_956c00[li];
                --s.count_95b220;
                for(auto n=c.n32(head);n;n=c.n32(n)){
                    --s.count_95b220;
                    if(c.n32(n+0xc)==0u){
                        const auto rec=n+0x10;const auto flags=c.n32(rec+0x34);
                        if(flags&0x100u){blend_42a2b0(c,0x45);multi_42c0f0(c,rec);}     // 42C0F0 rotated / multi-texture quad
                        else{blend_42a2b0(c,(flags&0x8000u)?0x41u:0x45u);image_42a0a0(c,sprite,rec);}
                        continue;
                    }
                    auto& d=*c.d;
                    auto save=[&](std::uint32_t state,std::uint32_t want,std::uint32_t& word,std::uint8_t& dirty){
                        const auto v=d.get_render_state(state);if(v!=want){d.set_render_state(state,want);}word=v;dirty=1;};
                    auto restore=[&](std::uint32_t state,std::uint32_t word,std::uint8_t& dirty){
                        if(d.get_render_state(state)!=word){d.set_render_state(state,word);}dirty=0;};
                    const auto flags=c.n32(n+0x60);
                    if(flags&0x100000u){
                        save(7,1,s.shadow_1c,s.shadow_63f);save(0x17,4,s.shadow_5c,s.shadow_64f);save(0xe,0,s.shadow_38,s.shadow_646);
                    }else if(flags&0x200000u){
                        save(7,1,s.shadow_1c,s.shadow_63f);save(0x17,4,s.shadow_5c,s.shadow_64f);save(0xe,1,s.shadow_38,s.shadow_646);
                        save(0xf,1,s.shadow_3c,s.shadow_647);save(0x19,7,s.shadow_64,s.shadow_651);save(0x18,0x80,s.shadow_60,s.shadow_650);
                    }
                    if(!(c.n32(n+0x60)&0x40000u))draw_42a3a0(c,n+0x58);
                    else mask_42a800(c,n+0x58);
                    const auto after=c.n32(n+0x60);
                    if(after&0x100000u){
                        restore(7,s.shadow_1c,s.shadow_63f);restore(0x17,s.shadow_5c,s.shadow_64f);restore(0xe,s.shadow_38,s.shadow_646);
                    }else if(after&0x200000u){
                        restore(7,s.shadow_1c,s.shadow_63f);restore(0x17,s.shadow_5c,s.shadow_64f);restore(0xe,s.shadow_38,s.shadow_646);
                        restore(0xf,s.shadow_3c,s.shadow_647);restore(0x19,s.shadow_64,s.shadow_651);restore(0x18,s.shadow_60,s.shadow_650);
                    }
                }
                s.heads_956c00[li]=0;
            }
            end_429f60(c,sprite);
        }catch(const Fault& e){
            latch(s,e.pc,e.address);
            if(li<Pc2dLayerCount)s.heads_956c00[li]=0;                          // the list is dropped (not drawn further)
        }catch(const std::exception&){
            latch(s,0x42d710u,li);
            if(li<Pc2dLayerCount)s.heads_956c00[li]=0;
        }
    }
}
// ---------------------------------------------------------------------- D3DX
std::array<float,4> pc_d3dx_vec4_transform(const std::array<float,4>& v,const std::array<float,16>& m){
    const X87 x(v[0]),y(v[1]),z(v[2]),w(v[3]);
    auto e=[&](unsigned k){return X87(m[k]);};
    return {x87_float(((e(12)*w+e(4)*y)+e(8)*z)+x*e(0)),
            x87_float(((e(13)*w+e(5)*y)+e(1)*x)+e(9)*z),
            x87_float(((e(14)*w+e(6)*y)+e(2)*x)+e(10)*z),
            x87_float(((e(15)*w+e(7)*y)+e(3)*x)+e(11)*z)};
}
std::array<float,16> pc_d3dx_rotation_z(float angle){
    float sn,cs;
#if defined(__i386__) || defined(__x86_64__)
    __asm__ volatile("flds %2\n\tfsincos\n\tfstps %1\n\tfstps %0" : "=m"(sn),"=m"(cs) : "m"(angle) : "st");
#else
    sn=float(std::sin(double(angle)));cs=float(std::cos(double(angle)));
#endif
    return {cs,sn,0.0f,0.0f,-sn,cs,0.0f,0.0f,0.0f,0.0f,1.0f,0.0f,0.0f,0.0f,0.0f,1.0f};
}
std::array<float,16> pc_d3dx_transformation_2d(const float scaling_xy[2],float rotation,const float translation_xy[2]){
    // 444B56 with ScalingRotation == 0 and pScalingCenter == NULL (444CD1 -> 444D38):
    // diag(Sx, Sy, 1, 1) (Sx/Sy copied as dwords); rotation != 0 (fucompp,
    // unordered included): M = M * Rz (pRotationCenter == NULL: 444E2D);
    // then M[3][0..1] += translation.
    std::array<float,16> m{};
    std::uint32_t sx,sy;std::memcpy(&sx,&scaling_xy[0],4);std::memcpy(&sy,&scaling_xy[1],4);
    m[0]=fbits(sx);m[5]=fbits(sy);m[10]=1.0f;m[15]=1.0f;
    if(!(rotation==0.0f))m=driving::pc_d3dx_matrix_multiply(m,pc_d3dx_rotation_z(rotation));
    m[12]=x87_float(X87(translation_xy[0])+X87(m[12]));
    m[13]=x87_float(X87(translation_xy[1])+X87(m[13]));
    return m;
}
// ---------------------------------------------------------------------- loaders
namespace {
std::size_t dds_bytes(const std::uint8_t* p,std::size_t n){
    if(n<128||std::memcmp(p,"DDS ",4)||rd32(p+4)!=124u)return 0;
    const auto h=rd32(p+12),w=rd32(p+16),mips=std::max<std::uint32_t>(1u,rd32(p+28)),pf=rd32(p+80),fourcc=rd32(p+84),bits=rd32(p+88);
    const bool cube=(rd32(p+112)&0x200u)!=0u;
    std::size_t total=0;
    for(unsigned f=0;f<(cube?6u:1u);++f){
        std::uint32_t lw=w,lh=h;
        for(std::uint32_t l=0;l<mips;++l){
            if(pf&4u){const unsigned block=fourcc==0x31545844u?8u:16u;total+=std::size_t(std::max(1u,(lw+3)/4))*std::max(1u,(lh+3)/4)*block;}
            else total+=std::size_t(lw)*lh*(bits/8u);
            lw=std::max(1u,lw/2);lh=std::max(1u,lh/2);
        }
    }
    return 128+total;
}
}
bool pc_sprite_bank_load_xst(PcSpriteBank& b,PcD3D9Device& device,const std::vector<std::uint8_t>& d,std::string& error){
    if(d.size()<40){error="XST too small";return false;}
    const auto meta=rd32(d.data()),pixels=rd32(d.data()+4);
    if(std::uint64_t(meta)+pixels+8u!=d.size()||meta<32){error="XST sections";return false;}
    b.header.assign(d.begin()+8,d.begin()+8+meta);
    // 430090 / 430120: the entry table at meta + 0xC + [meta+4], [meta+8] entries of
    // 0x14 bytes (+4 offset into the pixel section, +0xD type, +0x10 size). Each entry
    // with a type and a size is a file for D3DXCreateTextureFromFileInMemory (DDS in
    // the PC packs); type 0 or size 0 (the Xbox-layout raw blocks of spr_warning /
    // spr_title / spr_select) creates no texture, as on the PC.
    const auto count=rd32(b.header.data()+8);
    const std::size_t table=0xcu+std::size_t(rd32(b.header.data()+4));
    if(table+std::size_t(count)*0x14u>meta){error="XST entry table";return false;}
    const std::size_t pixels_at=8u+meta;
    b.textures.clear();
    for(std::uint32_t k=0;k<count;++k){
        const auto* e=b.header.data()+table+std::size_t(k)*0x14u;
        const auto offset=rd32(e+4),size=rd32(e+0x10);const std::uint8_t type=e[0xd];
        if(type==0u||size==0u){b.textures.push_back(0u);continue;}
        if(std::uint64_t(offset)+size>pixels){error="XST texture "+std::to_string(k)+" outside the pixel section";return false;}
        if(!dds_bytes(d.data()+pixels_at+offset,size)){error="XST texture "+std::to_string(k)+" DDS";return false;}
        PcTextureFileRequest q{};
        q.data=d.data()+pixels_at+offset;q.size=size;
        q.width=q.height=0xffffffffu;q.mip_levels=1;q.usage=0;q.format=0;q.pool=1;q.filter=1;q.mip_filter=1;q.colour_key=0;
        b.textures.push_back(device.create_texture_from_file(q));
    }
    b.state=2;return true;
}
bool pc_sprite_bank_load_animation(PcSpriteBank& b,std::uint32_t bank,const std::vector<std::uint8_t>& d,std::string& error){
    if(d.size()<8||rd32(d.data())+4u!=d.size()){error="ani wrapper";return false;}
    if(d.size()-4u>(1u<<22)){error="ani larger than its window";return false;}
    b.animation.assign(d.begin()+4,d.end());
    auto& a=b.animation;const auto base=PcSpraniAnimationBase(bank);
    auto in=[&](std::uint32_t off,std::uint32_t n){return std::size_t(off)+n<=a.size();};
    auto rel=[&](std::uint32_t off){if(!in(off,4))return;const auto v=rd32(a.data()+off);if(v)wr32(a.data()+off,v+base);};
    auto at=[&](std::uint32_t pc)->std::uint32_t{return pc-base;};
    auto fail=[&](const char* why){error=why;a.clear();return false;};
    // 429AC0: scene pointer table (terminated by 0).
    for(std::uint32_t t=0;;t+=4){
        if(!in(t,4))return fail("ani scene table");
        if(!rd32(a.data()+t))break;
        wr32(a.data()+t,rd32(a.data()+t)+base);
        const auto scene=at(rd32(a.data()+t));
        // 48BDD0(scene, base)
        if(!in(scene,0x10))return fail("ani scene");
        rel(scene+4);rel(scene+0xc);
        for(std::uint32_t i=0;std::int32_t(i)<std::int32_t(rd32(a.data()+scene));++i){
            const auto comp=at(rd32(a.data()+scene+4))+i*0x24u;
            if(!in(comp,0x24))return fail("ani component");
            rel(comp+0x20);
            for(std::uint32_t j=0;std::int32_t(j)<std::int32_t(rd32(a.data()+comp+0x1c));++j){
                const auto layer=at(rd32(a.data()+comp+0x20))+j*0x4cu;
                if(!in(layer,0x4c))return fail("ani layer");
                rel(layer+8);rel(layer+0xc);
                const auto mask=a[layer+0x49];
                for(unsigned k=0;k<6;++k)if(mask&(1u<<k))rel(layer+0x30+k*4);
            }
        }
        for(std::uint32_t i=0;std::int32_t(i)<std::int32_t(rd32(a.data()+scene+8));++i){
            const auto fo=at(rd32(a.data()+scene+0xc))+i*0x18u;
            if(!in(fo,0x18))return fail("ani footage");
            rel(fo+0x10);
        }
        // 429B00(bank): bank << 16 into every footage frame's texture word.
        for(std::uint32_t i=0;std::int32_t(i)<std::int32_t(rd32(a.data()+scene+8));++i){
            const auto fo=at(rd32(a.data()+scene+0xc))+i*0x18u;
            for(std::uint32_t j=0;std::int32_t(j)<std::int32_t(rd32(a.data()+fo+0xc));++j){
                const auto fr=at(rd32(a.data()+fo+0x10))+j*0x14u;
                if(!in(fr,4))return fail("ani frame");
                wr32(a.data()+fr,rd32(a.data()+fr)|(bank<<16));
            }
        }
    }
    return true;
}
}

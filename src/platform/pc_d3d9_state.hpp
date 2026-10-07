#pragma once
// Direct3D 9 pipeline state as the PC device keeps it (Get* returns what was
// Set, starting from the documented D3D9 defaults), shared by the device
// backends (software reference renderer, deko3d).
#include "platform/pc_d3d9.hpp"
#include <array>
#include <cstdio>
#include <algorithm>
#include <string>
#include <map>
#include <cmath>
#include <stdexcept>
#include <vector>
#include <cstring>
namespace outrun::platform {
namespace d3d9 {
enum : std::uint32_t {
    RS_ZENABLE=7,RS_FILLMODE=8,RS_SHADEMODE=9,RS_ZWRITEENABLE=14,RS_ALPHATESTENABLE=15,RS_LASTPIXEL=16,
    RS_SRCBLEND=19,RS_DESTBLEND=20,RS_CULLMODE=22,RS_ZFUNC=23,RS_ALPHAREF=24,RS_ALPHAFUNC=25,RS_DITHERENABLE=26,
    RS_ALPHABLENDENABLE=27,RS_FOGENABLE=28,RS_SPECULARENABLE=29,RS_FOGCOLOR=34,RS_FOGTABLEMODE=35,RS_FOGSTART=36,
    RS_FOGEND=37,RS_FOGDENSITY=38,RS_RANGEFOGENABLE=48,RS_STENCILENABLE=52,RS_TEXTUREFACTOR=60,RS_CLIPPING=136,
    RS_LIGHTING=137,RS_FOGVERTEXMODE=140,RS_COLORVERTEX=141,RS_LOCALVIEWER=142,RS_COLORWRITEENABLE=168,
    RS_BLENDOP=171,RS_SCISSORTESTENABLE=174,RS_SLOPESCALEDEPTHBIAS=175,RS_DEPTHBIAS=195,
    RS_SEPARATEALPHABLENDENABLE=206,RS_SRCBLENDALPHA=207,RS_DESTBLENDALPHA=208,RS_BLENDOPALPHA=209,
    SAMP_ADDRESSU=1,SAMP_ADDRESSV=2,SAMP_ADDRESSW=3,SAMP_BORDERCOLOR=4,SAMP_MAGFILTER=5,SAMP_MINFILTER=6,
    SAMP_MIPFILTER=7,SAMP_MIPMAPLODBIAS=8,SAMP_MAXMIPLEVEL=9,SAMP_MAXANISOTROPY=10,
    TSS_BUMPENVMAT00=7,TSS_BUMPENVMAT01=8,TSS_BUMPENVMAT10=9,TSS_BUMPENVMAT11=10,TSS_BUMPENVLSCALE=22,TSS_BUMPENVLOFFSET=23,
    CMP_NEVER=1,CMP_LESS=2,CMP_EQUAL=3,CMP_LESSEQUAL=4,CMP_GREATER=5,CMP_NOTEQUAL=6,CMP_GREATEREQUAL=7,CMP_ALWAYS=8,
    CULL_NONE=1,CULL_CW=2,CULL_CCW=3,
};
inline float f(std::uint32_t u){float v;std::memcpy(&v,&u,4);return v;}
inline std::uint32_t u(float v){std::uint32_t r;std::memcpy(&r,&v,4);return r;}
}
// State tracking part of a device: every Get/Set of the interface.
class PcD3D9StateDevice:public PcD3D9Device {
public:
    PcD3D9StateDevice(){reset_state();}
    void reset_state(){
        render.fill(0);
        auto rs=[&](std::uint32_t s,std::uint32_t v){render[s]=v;};
        using namespace d3d9;
        rs(RS_ZENABLE,1);rs(RS_FILLMODE,3);rs(RS_SHADEMODE,2);rs(RS_ZWRITEENABLE,1);rs(RS_LASTPIXEL,1);
        rs(RS_SRCBLEND,2);rs(RS_DESTBLEND,1);rs(RS_CULLMODE,CULL_CCW);rs(RS_ZFUNC,CMP_LESSEQUAL);rs(RS_ALPHAFUNC,CMP_ALWAYS);
        rs(RS_FOGEND,u(1.f));rs(RS_FOGDENSITY,u(1.f));rs(RS_TEXTUREFACTOR,0xffffffffu);rs(RS_CLIPPING,1);rs(RS_LIGHTING,1);
        rs(RS_COLORVERTEX,1);rs(RS_LOCALVIEWER,1);rs(RS_COLORWRITEENABLE,0xf);rs(RS_BLENDOP,1);
        rs(RS_SRCBLENDALPHA,2);rs(RS_DESTBLENDALPHA,1);rs(RS_BLENDOPALPHA,1);
        rs(58,0xffffffffu);/*STENCILMASK*/rs(59,0xffffffffu);/*STENCILWRITEMASK*/rs(56,CMP_ALWAYS);/*STENCILFUNC*/
        rs(53,1);rs(54,1);rs(55,1); // STENCILFAIL/ZFAIL/PASS = KEEP
        rs(190,0xf);rs(191,0xf);rs(192,0xf);rs(193,0xffffffffu); // COLORWRITEENABLE1..3, BLENDFACTOR
        rs(145,1);rs(146,2);                                    // DIFFUSE/SPECULARMATERIALSOURCE = COLOR1/COLOR2
        lights={};light_on.fill(0);material.fill(0.f);
        for(auto& s:sampler){s.fill(0);s[SAMP_ADDRESSU]=1;s[SAMP_ADDRESSV]=1;s[SAMP_ADDRESSW]=1;
            s[SAMP_MAGFILTER]=1;s[SAMP_MINFILTER]=1;s[SAMP_MAXANISOTROPY]=1;}
        for(unsigned k=0;k<stage.size();++k){stage[k].fill(0);stage[k][1]=k==0?4:1;stage[k][4]=k==0?4:1;stage[k][11]=k;
            stage[k][2]=2;stage[k][3]=1;stage[k][5]=2;stage[k][6]=1;} // COLOROP/ARG1/ARG2, ALPHAOP/ARG1/ARG2, TEXCOORDINDEX
        textures.fill(0);streams={};indices=0;declaration=0;vertex_shader=0;pixel_shader=0;
        for(auto& c:vs_constants)c.fill(0.f);for(auto& c:ps_constants)c.fill(0.f);
        vs_dirty_lo=0u;vs_dirty_hi=256u;
    }
    std::uint32_t get_render_state(std::uint32_t s)override{return s<render.size()?render[s]:0u;}
    void set_render_state(std::uint32_t s,std::uint32_t v)override{if(s<render.size())render[s]=v;}
    std::uint32_t get_texture(std::uint32_t s)override{if(s>=textures.size())return 0;if(textures[s])add_ref(textures[s]);return textures[s];}
    void set_texture(std::uint32_t s,std::uint32_t t)override{if(s<textures.size())textures[s]=t;}
    std::uint32_t get_texture_stage_state(std::uint32_t s,std::uint32_t t)override{return s<stage.size()&&t<stage[0].size()?stage[s][t]:0u;}
    void set_texture_stage_state(std::uint32_t s,std::uint32_t t,std::uint32_t v)override{if(s<stage.size()&&t<stage[0].size())stage[s][t]=v;}
    std::uint32_t get_sampler_state(std::uint32_t s,std::uint32_t t)override{return s<sampler.size()&&t<sampler[0].size()?sampler[s][t]:0u;}
    void set_sampler_state(std::uint32_t s,std::uint32_t t,std::uint32_t v)override{if(s<sampler.size()&&t<sampler[0].size())sampler[s][t]=v;}
    void set_vertex_declaration(std::uint32_t d)override{declaration=d;}
    void set_vertex_shader(std::uint32_t s)override{vertex_shader=s;}
    void set_pixel_shader(std::uint32_t s)override{pixel_shader=s;}
    void set_stream_source(std::uint32_t s,std::uint32_t b,std::uint32_t o,std::uint32_t st)override{if(s<streams.size())streams[s]={b,o,st};}
    void set_indices(std::uint32_t b)override{indices=b;}
    void set_vertex_shader_constant_f(std::uint32_t start,const float* d,std::uint32_t n)override{
        for(std::uint32_t k=0;k<n&&start+k<vs_constants.size();++k)std::memcpy(vs_constants[start+k].data(),d+k*4,16);
        if(start<vs_constants.size()&&n){vs_dirty_lo=std::min(vs_dirty_lo,start);
            vs_dirty_hi=std::max(vs_dirty_hi,std::min<std::uint32_t>(start+n,std::uint32_t(vs_constants.size())));}}
    void set_pixel_shader_constant_f(std::uint32_t start,const float* d,std::uint32_t n)override{
        for(std::uint32_t k=0;k<n&&start+k<ps_constants.size();++k)std::memcpy(ps_constants[start+k].data(),d+k*4,16);}
    // Fixed-function lighting state (SetLight / LightEnable / SetMaterial).
    void set_light(std::uint32_t i,const std::uint32_t* l)override{if(i<lights.size())for(unsigned k=0;k<26;++k)lights[i][k]=l[k];}
    void light_enable(std::uint32_t i,std::uint32_t on)override{if(i<light_on.size())light_on[i]=on;}
    void set_material(const float m[17])override{for(unsigned k=0;k<17;++k)material[k]=m[k];}
    std::array<std::array<std::uint32_t,26>,8> lights{};
    std::array<std::uint32_t,8> light_on{};
    std::array<float,17> material{};
    // Direct3D 9 fixed-function vertex lighting (camera space, directional /
    // point / spot lights, material sources from D3DRS 145..148 with
    // COLORVERTEX, global ambient D3DRS 139, specular with LOCALVIEWER):
    // the lit diffuse and specular of one vertex, each clamped to [0, 1].
    void ffp_light(const float pos[3],const float normal[3],const float* vertex_diffuse,const float* vertex_specular,
        float diffuse_out[4],float specular_out[4])const{
        using namespace d3d9;
        const auto wv=[&]{std::array<float,16> r{};for(unsigned i=0;i<4;++i)for(unsigned j=0;j<4;++j){float s=0;
            for(unsigned k=0;k<4;++k)s+=transform_world[i*4+k]*transform_view[k*4+j];r[i*4+j]=s;}return r;}();
        float p[3],n[3];
        for(unsigned j=0;j<3;++j){p[j]=pos[0]*wv[j]+pos[1]*wv[4+j]+pos[2]*wv[8+j]+wv[12+j];n[j]=normal[0]*wv[j]+normal[1]*wv[4+j]+normal[2]*wv[8+j];}
        if(render[143]){const float l=std::sqrt(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]);if(l>0.f)for(float& c:n)c/=l;}  // NORMALIZENORMALS
        const bool colour_vertex=render[RS_COLORVERTEX]!=0u;
        auto source=[&](std::uint32_t state,unsigned mat)->std::array<float,4>{
            const std::uint32_t s=render[state];
            if(colour_vertex&&s==1u&&vertex_diffuse)return {vertex_diffuse[0],vertex_diffuse[1],vertex_diffuse[2],vertex_diffuse[3]};
            if(colour_vertex&&s==2u&&vertex_specular)return {vertex_specular[0],vertex_specular[1],vertex_specular[2],vertex_specular[3]};
            return {material[mat],material[mat+1],material[mat+2],material[mat+3]};};
        const auto md=source(145,0),ms=source(146,8),ma=source(147,4),me=source(148,12);
        const std::uint32_t ga=render[139];                       // AMBIENT (D3DCOLOR)
        const float gar=float((ga>>16)&0xffu)/255.f,gag=float((ga>>8)&0xffu)/255.f,gab=float(ga&0xffu)/255.f;
        float dr=me[0]+ma[0]*gar,dg=me[1]+ma[1]*gag,db=me[2]+ma[2]*gab;
        float sr=0,sg=0,sb=0;
        for(unsigned i=0;i<lights.size();++i){
            if(!light_on[i])continue;
            const auto& l=lights[i];
            auto lf=[&](unsigned k){return f(l[k]);};
            float L[3],atten=1.f,spot=1.f;
            if(l[0]==3u){                                         // directional
                float d[3];for(unsigned j=0;j<3;++j)d[j]=lf(16)*transform_view[j]+lf(17)*transform_view[4+j]+lf(18)*transform_view[8+j];
                const float len=std::sqrt(d[0]*d[0]+d[1]*d[1]+d[2]*d[2]);
                for(unsigned j=0;j<3;++j)L[j]=len>0.f?-d[j]/len:0.f;
            }else{                                                // point / spot
                float lp[3];for(unsigned j=0;j<3;++j)lp[j]=lf(13)*transform_view[j]+lf(14)*transform_view[4+j]+lf(15)*transform_view[8+j]+transform_view[12+j];
                float d[3]{lp[0]-p[0],lp[1]-p[1],lp[2]-p[2]};
                const float dist=std::sqrt(d[0]*d[0]+d[1]*d[1]+d[2]*d[2]);
                if(dist>lf(19))continue;                          // Range
                for(unsigned j=0;j<3;++j)L[j]=dist>0.f?d[j]/dist:0.f;
                const float den=lf(21)+lf(22)*dist+lf(23)*dist*dist;
                atten=den>0.f?1.f/den:1.f;
                if(l[0]==2u){                                     // spot
                    float sd[3];for(unsigned j=0;j<3;++j)sd[j]=lf(16)*transform_view[j]+lf(17)*transform_view[4+j]+lf(18)*transform_view[8+j];
                    const float sl=std::sqrt(sd[0]*sd[0]+sd[1]*sd[1]+sd[2]*sd[2]);
                    const float rho=sl>0.f?-(sd[0]*L[0]+sd[1]*L[1]+sd[2]*L[2])/sl:0.f;
                    const float ct=std::cos(lf(24)*0.5f),cp=std::cos(lf(25)*0.5f);
                    if(rho<=cp)spot=0.f;else if(rho<ct)spot=std::pow((rho-cp)/(ct-cp),lf(20));
                }
            }
            const float k=atten*spot;
            dr+=ma[0]*lf(9)*k;dg+=ma[1]*lf(10)*k;db+=ma[2]*lf(11)*k;
            const float ndl=n[0]*L[0]+n[1]*L[1]+n[2]*L[2];
            if(ndl>0.f){dr+=md[0]*lf(1)*ndl*k;dg+=md[1]*lf(2)*ndl*k;db+=md[2]*lf(3)*ndl*k;}
            if(render[RS_SPECULARENABLE]&&ndl>0.f){
                float v[3]{0.f,0.f,-1.f};
                if(render[RS_LOCALVIEWER]){const float pl=std::sqrt(p[0]*p[0]+p[1]*p[1]+p[2]*p[2]);if(pl>0.f)for(unsigned j=0;j<3;++j)v[j]=-p[j]/pl;}
                float h[3]{L[0]+v[0],L[1]+v[1],L[2]+v[2]};
                const float hl=std::sqrt(h[0]*h[0]+h[1]*h[1]+h[2]*h[2]);
                if(hl>0.f){const float ndh=(n[0]*h[0]+n[1]*h[1]+n[2]*h[2])/hl;
                    if(ndh>0.f){const float s=std::pow(ndh,material[16])*k;sr+=ms[0]*lf(5)*s;sg+=ms[1]*lf(6)*s;sb+=ms[2]*lf(7)*s;}}
            }
        }
        auto sat=[](float x){return x<0.f?0.f:x>1.f?1.f:x;};
        diffuse_out[0]=sat(dr);diffuse_out[1]=sat(dg);diffuse_out[2]=sat(db);diffuse_out[3]=sat(md[3]);
        specular_out[0]=sat(sr);specular_out[1]=sat(sg);specular_out[2]=sat(sb);
        specular_out[3]=vertex_specular?vertex_specular[3]:1.f;           // fog factor passes through
    }
    // DrawIndexedPrimitiveUP by expansion: the indexed vertices are lit
    // (D3DRS_LIGHTING with a NORMAL in the FVF) into an FVF without the
    // normal and drawn with draw_primitive_up (lists and strips).
    void draw_indexed_primitive_up(std::uint32_t type,std::uint32_t,std::uint32_t,std::uint32_t count,const void* indices,
        std::uint32_t index_format,const void* data,std::uint32_t stride)override{
        if(!count||!indices||!data)return;
        std::size_t total=0;
        switch(type){case 4:total=std::size_t(count)*3;break;case 5:case 6:total=std::size_t(count)+2;break;
            default:throw std::runtime_error("DrawIndexedPrimitiveUP: unsupported primitive type");}
        const std::uint32_t fvf=fvf_;
        if((fvf&0x400eu)!=0x002u)throw std::runtime_error("DrawIndexedPrimitiveUP: only the XYZ fixed-function FVFs are supported");
        const bool has_normal=(fvf&0x010u)!=0u,lit=has_normal&&render[d3d9::RS_LIGHTING]!=0u;
        std::size_t off=12;const std::size_t normal_at=has_normal?off:0;if(has_normal)off+=12;if(fvf&0x020u)off+=4;
        const std::size_t diffuse_at=off;if(fvf&0x040u)off+=4;
        const std::size_t specular_at=off;if(fvf&0x080u)off+=4;
        const std::size_t tex_bytes=stride>off?stride-off:0;
        const std::uint32_t out_fvf=(fvf&~0x030u)|0x0c0u;          // no normal / psize, diffuse + specular present
        const std::size_t out_stride=12+8+tex_bytes;
        std::vector<std::uint8_t> out(total*out_stride);
        const auto* v=static_cast<const std::uint8_t*>(data);
        auto rd=[](const std::uint8_t* q){float x;std::memcpy(&x,q,4);return x;};
        auto colour=[](const std::uint8_t* q,float c[4]){c[0]=q[2]/255.f;c[1]=q[1]/255.f;c[2]=q[0]/255.f;c[3]=q[3]/255.f;};
        auto pack=[](const float c[4]){auto b=[](float x){x=x<0.f?0.f:x>1.f?1.f:x;return std::uint32_t(x*255.f+0.5f);};
            return (b(c[3])<<24)|(b(c[0])<<16)|(b(c[1])<<8)|b(c[2]);};
        for(std::size_t i=0;i<total;++i){
            const std::uint32_t index=index_format==0x66u?static_cast<const std::uint32_t*>(indices)[i]:static_cast<const std::uint16_t*>(indices)[i];
            const std::uint8_t* q=v+std::size_t(index)*stride;
            std::uint8_t* o=out.data()+i*out_stride;
            std::memcpy(o,q,12);
            float d[4]{1,1,1,1},s[4]{0,0,0,1};
            if(fvf&0x040u)colour(q+diffuse_at,d);
            if(fvf&0x080u)colour(q+specular_at,s);
            if(lit){
                const float pos[3]{rd(q),rd(q+4),rd(q+8)},nrm[3]{rd(q+normal_at),rd(q+normal_at+4),rd(q+normal_at+8)};
                float ld[4],ls[4];ffp_light(pos,nrm,(fvf&0x040u)?d:nullptr,(fvf&0x080u)?s:nullptr,ld,ls);
                std::memcpy(d,ld,sizeof d);std::memcpy(s,ls,sizeof s);
            }
            const std::uint32_t dc=pack(d),sc=pack(s);
            std::memcpy(o+12,&dc,4);std::memcpy(o+16,&sc,4);
            std::memcpy(o+20,q+off,tex_bytes);
        }
        const std::uint32_t saved=fvf_;
        set_fvf(out_fvf);
        const std::uint32_t lighting=render[d3d9::RS_LIGHTING];
        render[d3d9::RS_LIGHTING]=0;
        draw_primitive_up(type,count,out.data(),std::uint32_t(out_stride));
        render[d3d9::RS_LIGHTING]=lighting;
        set_fvf(saved);
    }
    void set_fvf(std::uint32_t v)override{fvf_=v;}
    std::uint32_t fvf_{};
    // Diagnostics: DrawIndexedPrimitive primitive counts per render-state
    // combination (cull, z enable/write/func, alpha test func/ref, blend
    // src/dst, colour write, stencil).
    // Keyed by the raw state words (formatted only in the summary); the
    // previous draw's entry is reused while the state does not change.
    using DipKey=std::array<std::uint32_t,12>;
    std::map<DipKey,std::uint64_t> dip_states;
    DipKey dip_last_key_{};std::uint64_t* dip_last_count_{};
    void note_dip_state(std::uint32_t prims){
        const DipKey k{render[22],render[7],render[14],render[23],render[15],render[25],render[24]&0xffu,render[27],render[19],render[20],render[168],render[52]};
        if(!dip_last_count_||k!=dip_last_key_){dip_last_key_=k;dip_last_count_=&dip_states[k];}
        *dip_last_count_+=prims;}
    std::string dip_state_summary(unsigned top=10)const{
        std::vector<std::pair<std::uint64_t,std::string>> v;
        for(const auto& [k,n]:dip_states){char t[96];
            std::snprintf(t,sizeof t,"cull%u z%u%u f%u at%u/%u/%02x bl%u:%u/%u cw%x st%u",k[0],k[1],k[2],k[3],k[4],k[5],k[6],k[7],k[8],k[9],k[10],k[11]);
            v.push_back({n,t});}
        std::sort(v.begin(),v.end(),[](const auto& a,const auto& b){return a.first>b.first;});
        std::string out;for(unsigned i=0;i<v.size()&&i<top;++i)out+=" {"+v[i].second+" p="+std::to_string(v[i].first)+"}";
        return out;}
    // Reference counting of textures handed out by GetTexture.
    void add_ref(std::uint32_t)override{}
    struct Stream { std::uint32_t buffer{},offset{},stride{}; };
    std::array<std::uint32_t,256> render{};
    std::array<std::array<std::uint32_t,14>,16> sampler{};
    std::array<std::array<std::uint32_t,33>,8> stage{};
    // Fixed-function transforms (SetTransform): world, view, projection.
    std::array<float,16> transform_world{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1},transform_view=transform_world,transform_proj=transform_world;
    void set_transform(std::uint32_t state,const float m[16])override{
        auto* t=state==0x100u?&transform_world:state==2u?&transform_view:state==3u?&transform_proj:nullptr;
        if(t)for(unsigned k=0;k<16;++k)(*t)[k]=m[k];}
    // world * view * projection (row vectors), the fixed-function vertex transform.
    std::array<float,16> transform_wvp()const{
        auto mul=[](const std::array<float,16>& a,const std::array<float,16>& b){std::array<float,16> r{};
            for(unsigned i=0;i<4;++i)for(unsigned j=0;j<4;++j){float s=0;for(unsigned k=0;k<4;++k)s+=a[i*4+k]*b[k*4+j];r[i*4+j]=s;}return r;};
        return mul(mul(transform_world,transform_view),transform_proj);}
    std::array<std::uint32_t,16> textures{};
    std::array<Stream,4> streams{};
    std::uint32_t indices{},declaration{},vertex_shader{},pixel_shader{};
    std::array<std::array<float,4>,256> vs_constants{};
    // Registers [lo, hi) set since the backend last uploaded them.
    std::uint32_t vs_dirty_lo{0u},vs_dirty_hi{256u};
    std::array<std::array<float,4>,8> ps_constants{};
};
}

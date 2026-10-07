// Checks the generated shader blob code (tools/generate_pc_shaders.py) against
// an independent SM1 interpreter written here from the D3D9 instruction
// definitions, for every blob of the EXE and random register states; checks
// that every blob and every shader linked by 40E140 splits back into blobs.
#include "platform/pc_shader_programs.hpp"
#include "platform/pc_vertex_shader_setup.hpp"
#include <cstdio>
#include <cstdlib>
#include <random>
using namespace outrun::platform;
using namespace outrun::platform::pc_shader;
static unsigned checks;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"shader programs line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace {
// ---- independent interpreter ----
struct Machine {
    unsigned kind{}; // 0 vs, 1 ps11, 2 ps14
    V4 r[12]{},v[16]{},c[256]{},o_pos{},o_fog{1,1,1,1},o_pts{},o_d[2]{},o_t[8]{},t[6]{},tc[4]{},vc[2]{};
    float m3[4]{};
    const PsSampling* io{};
    V4 defs[8]{};bool defined[8]{};
    V4& reg(std::uint32_t p){
        const unsigned type=((p>>28)&7u)|((p>>8)&0x18u),n=p&0x7ffu;
        if(kind==0){
            switch(type){case 0:return r[n];case 1:return v[n];case 2:return c[n];case 4:return n==0?o_pos:(n==1?o_fog:o_pts);
                case 5:return o_d[n];case 6:return o_t[n];}
        }else{
            switch(type){case 0:return r[n];case 1:return vc[n];case 2:return defined[n]?defs[n]:c[n];case 3:return t[n];}
        }
        std::fprintf(stderr,"register type %u\n",type);std::exit(1);
    }
    V4 read(std::uint32_t p){
        const V4 x=reg(p);V4 s{};
        for(unsigned i=0;i<4;++i)s[i]=x[(p>>(16+2*i))&3u];
        switch((p>>24)&0xfu){
        case 0:return s;
        case 1:return {-s.x,-s.y,-s.z,-s.w};
        case 2:return {s.x-0.5f,s.y-0.5f,s.z-0.5f,s.w-0.5f};
        case 3:return {-(s.x-0.5f),-(s.y-0.5f),-(s.z-0.5f),-(s.w-0.5f)};
        case 4:return {s.x*2.f-1.f,s.y*2.f-1.f,s.z*2.f-1.f,s.w*2.f-1.f};
        case 5:return {-(s.x*2.f-1.f),-(s.y*2.f-1.f),-(s.z*2.f-1.f),-(s.w*2.f-1.f)};
        case 6:return {1.f-s.x,1.f-s.y,1.f-s.z,1.f-s.w};
        case 7:return {s.x*2.f,s.y*2.f,s.z*2.f,s.w*2.f};
        case 8:return {-(s.x*2.f),-(s.y*2.f),-(s.z*2.f),-(s.w*2.f)};
        }
        return s;
    }
    struct Pending { std::uint32_t dst; V4 value; unsigned mask_limit; };
    void store(const Pending& w){
        const auto d=w.dst;V4 v=w.value;
        int shift=int((d>>24)&0xfu);if(shift>=8)shift-=16;
        if(shift){const float f=std::ldexp(1.f,shift);for(unsigned i=0;i<4;++i)v[i]*=f;}
        if((d>>20)&1u)for(unsigned i=0;i<4;++i)v[i]=std::fmin(std::fmax(v[i],0.f),1.f);
        V4& target=reg(d);const unsigned m=((d>>16)&0xfu)&w.mask_limit;
        for(unsigned i=0;i<4;++i)if(m>>i&1u)target[i]=v[i];
    }
    void run(const std::uint32_t* tok,std::size_t n){
        // def first (shader-wide).
        for(std::size_t p=0;p<n;){const auto op=tok[p]&0xffffu;std::size_t q=p+1;
            if(op==0x51u){const auto k=tok[p+1]&0x7ffu;defined[k]=true;std::memcpy(&defs[k],tok+p+2,16);q=p+6;}
            else while(q<n&&(tok[q]&0x80000000u))++q;
            p=q;}
        std::vector<Pending> pending;
        for(std::size_t p=0;p<n;){
            const auto op=tok[p]&0xffffu;const bool co=(tok[p]&0x40000000u)!=0;
            std::size_t q=p+1;
            if(op==0x51u)q=p+6;else while(q<n&&(tok[q]&0x80000000u))++q;
            const std::uint32_t* a=tok+p+1;const std::size_t argc=q-p-1;
            std::size_t next=q;bool next_co=false;
            if(next<n)next_co=(tok[next]&0x40000000u)!=0;
            if(!co){for(auto& w:pending)store(w);pending.clear();}
            auto s=[&](unsigned i){return read(a[i]);};
            Pending w{a[0],{},0xfu};bool arithmetic=true;
            switch(op){
            case 0:case 31:case 0x51:arithmetic=false;break;
            case 1:w.value=s(1);break;
            case 2:{auto x=s(1),y=s(2);w.value={x.x+y.x,x.y+y.y,x.z+y.z,x.w+y.w};break;}
            case 3:{auto x=s(1),y=s(2);w.value={x.x-y.x,x.y-y.y,x.z-y.z,x.w-y.w};break;}
            case 4:{auto x=s(1),y=s(2),z=s(3);for(unsigned i=0;i<4;++i)w.value[i]=x[i]*y[i]+z[i];break;}
            case 5:{auto x=s(1),y=s(2);for(unsigned i=0;i<4;++i)w.value[i]=x[i]*y[i];break;}
            case 6:{const float f=s(1).w;const float r=f==1.f?1.f:(f==0.f?INFINITY:1.f/f);w.value={r,r,r,r};break;}
            case 7:{const float f=std::fabs(s(1).w);const float r=f==1.f?1.f:(f==0.f?INFINITY:1.f/std::sqrt(f));w.value={r,r,r,r};break;}
            case 8:{auto x=s(1),y=s(2);const float d=x.x*y.x+x.y*y.y+x.z*y.z;w.value={d,d,d,d};break;}
            case 9:{auto x=s(1),y=s(2);const float d=x.x*y.x+x.y*y.y+x.z*y.z+x.w*y.w;w.value={d,d,d,d};break;}
            case 10:{auto x=s(1),y=s(2);for(unsigned i=0;i<4;++i)w.value[i]=std::fmin(x[i],y[i]);break;}
            case 11:{auto x=s(1),y=s(2);for(unsigned i=0;i<4;++i)w.value[i]=std::fmax(x[i],y[i]);break;}
            case 12:{auto x=s(1),y=s(2);for(unsigned i=0;i<4;++i)w.value[i]=x[i]<y[i]?1.f:0.f;break;}
            case 13:{auto x=s(1),y=s(2);for(unsigned i=0;i<4;++i)w.value[i]=x[i]>=y[i]?1.f:0.f;break;}
            case 16:{auto x=s(1);V4 d{1,0,0,1};const float pw=std::fmin(std::fmax(x.w,-127.9961f),127.9961f);
                if(x.x>0.f){d.y=x.x;if(x.y>0.f)d.z=std::pow(x.y,pw);}w.value=d;break;}
            case 17:{auto x=s(1),y=s(2);w.value={1.f,x.y*y.y,x.z,y.w};break;}
            case 20:case 21:case 22:case 23:case 24:{
                const unsigned cols=(op==20||op==21)?4u:3u,rows=op==20?4u:op==21?3u:op==22?4u:op==23?3u:2u;
                const auto x=s(1);
                for(unsigned rr=0;rr<rows;++rr){
                    const std::uint32_t row=(a[2]&~0x7ffu)|((a[2]&0x7ffu)+rr);const auto y=read(row);
                    float d=0.f;for(unsigned i=0;i<cols;++i)d+=x[i]*y[i];w.value[rr]=d;}
                w.mask_limit=(1u<<rows)-1u;break;}
            case 0x50:{auto x=s(1),y=s(2),z=s(3);for(unsigned i=0;i<4;++i)w.value[i]=x[i]>0.5f?y[i]:z[i];break;}
            case 0x42:{
                arithmetic=false;const auto dn=a[0]&0x7ffu;
                if(kind==2){V4 cc=read(a[1]&0xf0ffffffu);const auto m=(a[1]>>24)&0xfu;
                    if(m==9)cc={cc.x/cc.z,cc.y/cc.z,0,1};else if(m==10)cc={cc.x/cc.w,cc.y/cc.w,0,1};
                    store({a[0],io->sample(int(dn),cc),0xfu});}
                else t[dn]=io->sample(int(dn),tc[dn]);
                break;}
            case 0x44:{arithmetic=false;const auto dn=a[0]&0x7ffu,sn=a[1]&0x7ffu;const auto m=io->bump(int(dn));const auto l=io->luminance(int(dn));
                V4 col=io->sample(int(dn),{tc[dn].x+m[0]*t[sn].x+m[2]*t[sn].y,tc[dn].y+m[1]*t[sn].x+m[3]*t[sn].y,0,1});
                const float k=std::fmin(std::fmax(t[sn].z*l[0]+l[1],0.f),1.f);col.x*=k;col.y*=k;col.z*=k;t[dn]=col;break;}
            case 0x49:{arithmetic=false;const auto dn=a[0]&0x7ffu,sn=a[1]&0x7ffu;m3[dn]=tc[dn].x*t[sn].x+tc[dn].y*t[sn].y+tc[dn].z*t[sn].z;break;}
            case 0x4d:{arithmetic=false;const auto dn=a[0]&0x7ffu,sn=a[1]&0x7ffu;
                const float nx=m3[dn-2],ny=m3[dn-1],nz=tc[dn].x*t[sn].x+tc[dn].y*t[sn].y+tc[dn].z*t[sn].z;
                const float ex=tc[dn-2].w,ey=tc[dn-1].w,ez=tc[dn].w;const float k=2.f*(nx*ex+ny*ey+nz*ez)/(nx*nx+ny*ny+nz*nz);
                t[dn]=io->sample(int(dn),{nx*k-ex,ny*k-ey,nz*k-ez,1});break;}
            default:std::fprintf(stderr,"opcode %u\n",op);std::exit(1);
            }
            if(arithmetic){
                if(co||next_co)pending.push_back(w);else store(w);
                if(co){for(auto& x:pending)store(x);pending.clear();}
            }
            p=q;
        }
        for(auto& x:pending)store(x);
    }
};
float rnd(std::mt19937& g){return float(int(g()%2001u)-1000)/250.f;}
V4 rv(std::mt19937& g){return {rnd(g),rnd(g),rnd(g),rnd(g)};}
bool same(const V4& a,const V4& b){
    for(unsigned i=0;i<4;++i){const float x=a[i],y=b[i];
        if(std::isnan(x)&&std::isnan(y))continue;
        if(x==y)continue;
        if(std::fabs(x-y)<=1e-4f*std::fmax(1.f,std::fmax(std::fabs(x),std::fabs(y))))continue;
        return false;}
    return true;
}
}
int main(){
    std::mt19937 g(0x5348u);
    PsSampling io;
    io.sample=[](int n,const V4& c){return V4{c.x*0.25f+float(n),c.y*0.5f,c.z+c.x,0.75f};};
    io.bump=[](int n){return std::array<float,4>{0.5f,float(n)*0.1f,-0.25f,1.f};};
    io.luminance=[](int n){return std::array<float,2>{0.75f,float(n)*0.05f};};
    unsigned blobs_checked=0;
    for(std::size_t b=0;b<PcShaderBlobCount;++b){
        const auto& blob=PcShaderBlobs[b];
        const std::uint32_t* body=PcShaderBlobTokens+blob.token_offset;
        // Whole-blob shader splits back to itself (or to a sequence with the same tokens).
        std::vector<std::uint32_t> shader{blob.version};shader.insert(shader.end(),body,body+blob.token_count);shader.push_back(0xffff);
        PcShaderProgram prog;CHECK(pc_shader_split(shader.data(),prog));
        for(unsigned trial=0;trial<64;++trial){
            Machine m;m.io=&io;
            if(blob.version==0xfffe0101u){
                m.kind=0;VsState s;std::array<V4,256> c{};
                for(auto& x:c)x=rv(g);for(unsigned k=0;k<12;++k)s.R[k]=m.r[k]=rv(g);for(unsigned k=0;k<16;++k)s.V[k]=m.v[k]=rv(g);
                for(unsigned k=0;k<256;++k)m.c[k]=c[k];s.C=c.data();
                PcShaderBlobFns_vs[blob.index](s);m.run(body,blob.token_count);
                for(unsigned k=0;k<12;++k)CHECK(same(s.R[k],m.r[k]));
                CHECK(same(s.oPos,m.o_pos));CHECK(same(s.oD[0],m.o_d[0]));CHECK(same(s.oD[1],m.o_d[1]));
                for(unsigned k=0;k<8;++k)CHECK(same(s.oT[k],m.o_t[k]));
            }else if(blob.version==0xffff0104u){
                m.kind=2;Ps14State s;s.io=&io;std::array<V4,8> c{};for(auto& x:c)x=rv(g);s.PC=c.data();
                for(unsigned k=0;k<8;++k)m.c[k]=c[k];
                for(unsigned k=0;k<6;++k){s.R[k]=m.r[k]=rv(g);s.T[k]=m.t[k]=rv(g);}
                for(unsigned k=0;k<2;++k)s.VC[k]=m.vc[k]=rv(g);
                PcShaderBlobFns_ps14[blob.index](s);m.run(body,blob.token_count);
                for(unsigned k=0;k<6;++k)CHECK(same(s.R[k],m.r[k]));
            }else{
                m.kind=1;Ps11State s;s.io=&io;std::array<V4,8> c{};for(auto& x:c)x=rv(g);s.PC=c.data();
                for(unsigned k=0;k<8;++k)m.c[k]=c[k];
                for(unsigned k=0;k<2;++k){s.R[k]=m.r[k]=rv(g);s.VC[k]=m.vc[k]=rv(g);}
                for(unsigned k=0;k<4;++k){s.T[k]=m.t[k]=rv(g);s.TC[k]=m.tc[k]=rv(g);}
                PcShaderBlobFns_ps11[blob.index](s);m.run(body,blob.token_count);
                for(unsigned k=0;k<2;++k)CHECK(same(s.R[k],m.r[k]));
                for(unsigned k=0;k<4;++k)CHECK(same(s.T[k],m.t[k]));
            }
        }
        ++blobs_checked;
    }
    // Every vertex shader 40E140 can link from the configuration fields splits.
    unsigned linked=0;
    for(std::uint32_t low=0;low<(1u<<9);++low){
        if((low&7u)>4u||((low>>5)&0xfu)>7u)continue;
        for(std::uint32_t layer=0;layer<13;++layer){
            const std::uint32_t c=low|((layer)<<9)|((layer%4u)<<13);
            const auto tokens=link_vertex_shader_40e140(c);
            PcShaderProgram p;CHECK(pc_shader_split(tokens.data(),p));++linked;
            CHECK(!p.inputs.empty());
        }
    }
    std::printf("pc shader programs: %u blobs, %u linked vertex shaders, %u checks\n",blobs_checked,linked,checks);
    return 0;
}

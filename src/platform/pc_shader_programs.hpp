#pragma once
// PC shader bytecode as generated code (tools/generate_pc_shaders.py): every
// D3D9 shader blob of the EXE as a C++ function over a register state (the
// same translation is emitted as GLSL for the Switch backend), the blob table,
// and the split of a created shader into blob ids.
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <vector>
namespace outrun::platform {
namespace pc_shader {
struct V4 { float x{},y{},z{},w{}; float& operator[](unsigned i){return (&x)[i];} float operator[](unsigned i)const{return (&x)[i];} };
inline V4 v4(float f){return {f,f,f,f};}
inline V4 v4(float a,float b,float c,float d){return {a,b,c,d};}
inline V4 swz(const V4& v,unsigned a,unsigned b,unsigned c,unsigned d){return {v[a],v[b],v[c],v[d]};}
inline V4 neg(const V4& v){return {-v.x,-v.y,-v.z,-v.w};}
inline V4 add(const V4& a,const V4& b){return {a.x+b.x,a.y+b.y,a.z+b.z,a.w+b.w};}
inline V4 sub(const V4& a,const V4& b){return {a.x-b.x,a.y-b.y,a.z-b.z,a.w-b.w};}
inline V4 mul(const V4& a,const V4& b){return {a.x*b.x,a.y*b.y,a.z*b.z,a.w*b.w};}
inline V4 mad(const V4& a,const V4& b,const V4& c){return {a.x*b.x+c.x,a.y*b.y+c.y,a.z*b.z+c.z,a.w*b.w+c.w};}
inline V4 min(const V4& a,const V4& b){return {std::fmin(a.x,b.x),std::fmin(a.y,b.y),std::fmin(a.z,b.z),std::fmin(a.w,b.w)};}
inline V4 max(const V4& a,const V4& b){return {std::fmax(a.x,b.x),std::fmax(a.y,b.y),std::fmax(a.z,b.z),std::fmax(a.w,b.w)};}
inline V4 dp3(const V4& a,const V4& b){return v4(a.x*b.x+a.y*b.y+a.z*b.z);}
inline V4 dp4(const V4& a,const V4& b){return v4(a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w);}
inline V4 slt(const V4& a,const V4& b){return {a.x<b.x?1.f:0.f,a.y<b.y?1.f:0.f,a.z<b.z?1.f:0.f,a.w<b.w?1.f:0.f};}
inline V4 sge(const V4& a,const V4& b){return {a.x>=b.x?1.f:0.f,a.y>=b.y?1.f:0.f,a.z>=b.z?1.f:0.f,a.w>=b.w?1.f:0.f};}
inline V4 d3d_rcp(float v){return v4(v==1.f?1.f:(v==0.f?INFINITY:1.f/v));}
inline V4 d3d_rsq(float v){v=std::fabs(v);return v4(v==1.f?1.f:(v==0.f?INFINITY:1.f/std::sqrt(v)));}
inline V4 d3d_lit(const V4& s){V4 d{1.f,0.f,0.f,1.f};const float p=std::fmin(std::fmax(s.w,-127.9961f),127.9961f);
    if(s.x>0.f){d.y=s.x;if(s.y>0.f)d.z=std::pow(s.y,p);}return d;}
inline V4 d3d_dst(const V4& a,const V4& b){return {1.f,a.y*b.y,a.z,b.w};}
inline V4 lrp(const V4& t,const V4& a,const V4& b){return {b.x+(a.x-b.x)*t.x,b.y+(a.y-b.y)*t.y,b.z+(a.z-b.z)*t.z,b.w+(a.w-b.w)*t.w};}
inline V4 cnd(const V4& c,const V4& a,const V4& b){return {c.x>0.5f?a.x:b.x,c.y>0.5f?a.y:b.y,c.z>0.5f?a.z:b.z,c.w>0.5f?a.w:b.w};}
inline V4 cmp(const V4& c,const V4& a,const V4& b){return {c.x>=0.f?a.x:b.x,c.y>=0.f?a.y:b.y,c.z>=0.f?a.z:b.z,c.w>=0.f?a.w:b.w};}
inline float clamp01(float v){return std::fmin(std::fmax(v,0.f),1.f);}
inline V4 sat(const V4& v){return {clamp01(v.x),clamp01(v.y),clamp01(v.z),clamp01(v.w)};}
inline void wmask(V4& d,const V4& v,unsigned m){if(m&1)d.x=v.x;if(m&2)d.y=v.y;if(m&4)d.z=v.z;if(m&8)d.w=v.w;}
inline V4 proj_z(const V4& c){return {c.x/c.z,c.y/c.z,0.f,1.f};}
inline V4 proj_w(const V4& c){return {c.x/c.w,c.y/c.w,0.f,1.f};}
struct VsState { V4 R[12]{},V[16]{}; const V4* C{}; V4 oPos{},oFog{1,1,1,1},oPts{},oD[2]{},oT[8]{}; };
// Texture reads of the pixel stages: sample(stage, coordinate) (2D uses xy,
// cubes xyz); bump(stage) = BUMPENVMAT00/01/10/11, luminance(stage) = LSCALE/LOFFSET.
struct PsSampling {
    std::function<V4(int,const V4&)> sample;
    std::function<std::array<float,4>(int)> bump;
    std::function<std::array<float,2>(int)> luminance;
};
struct Ps14State { V4 R[6]{},T[6]{},VC[2]{}; const V4* PC{}; const PsSampling* io{};
    V4 sample(int n,const V4& c){return io->sample(n,c);} };
struct Ps11State { V4 R[2]{},T[4]{},TC[4]{},VC[2]{}; float M3[4]{}; const V4* PC{}; const PsSampling* io{};
    V4 sample(int n,const V4& c){return io->sample(n,c);}
    V4 sample_bump(int n,const V4& tc,const V4& t){
        const auto m=io->bump(n);const auto l=io->luminance(n);
        V4 c=io->sample(n,{tc.x+m[0]*t.x+m[2]*t.y,tc.y+m[1]*t.x+m[3]*t.y,0.f,1.f});
        const float k=clamp01(t.z*l[0]+l[1]);c.x*=k;c.y*=k;c.z*=k;return c;}
    V4 sample_vspec(int n,float a,float b,float c,const V4& eye){
        const float nx=a,ny=b,nz=c;const float ne=nx*eye.x+ny*eye.y+nz*eye.z,nn=nx*nx+ny*ny+nz*nz;
        const float k=2.f*ne/nn;return io->sample(n,{nx*k-eye.x,ny*k-eye.y,nz*k-eye.z,1.f});}
};
}
template<class State> using PcShaderBlobFn=void(*)(State&);
struct PcShaderBlob { std::uint32_t va,version,index,token_offset,token_count; };
extern const PcShaderBlob PcShaderBlobs[];
extern const std::size_t PcShaderBlobCount;
extern const std::size_t PcShaderBlobCounts[3];
extern std::uint32_t PcShaderBlobTokens[];   // from the player's EXE image
extern const PcShaderBlobFn<pc_shader::VsState> PcShaderBlobFns_vs[];
extern const PcShaderBlobFn<pc_shader::Ps14State> PcShaderBlobFns_ps14[];
extern const PcShaderBlobFn<pc_shader::Ps11State> PcShaderBlobFns_ps11[];
// A created shader as a program of blob indices (per version table), plus the
// vs input declarations (usage, usage index, v register).
struct PcShaderProgram {
    std::uint32_t version{};
    std::vector<std::uint32_t> blobs;
    struct Input { std::uint8_t usage{},usage_index{},reg{}; };
    std::vector<Input> inputs;
};
// Splits tokens (version ... 0x0000FFFF) into EXE blob bodies. False when the
// token stream is not a concatenation of known blobs of its version.
bool pc_shader_split(const std::uint32_t* tokens,PcShaderProgram& out);
void pc_shader_run_vs(const PcShaderProgram&,pc_shader::VsState&);
void pc_shader_run_ps14(const PcShaderProgram&,pc_shader::Ps14State&);
void pc_shader_run_ps11(const PcShaderProgram&,pc_shader::Ps11State&);
}

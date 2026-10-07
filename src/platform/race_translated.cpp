#include <cstdint>
#include "system/exe_image.hpp"
#include <algorithm>
#include <map>
#include "platform/race_translated.hpp"
#include "platform/pc_d3d9.hpp"
#include "platform/pc_sprite_2d.hpp"
#include "driving/pc_d3dx.hpp"
#include "driving/pc_camera.hpp"
#include "driving/pc_x87.hpp"
#include "platform/frontend_profiles.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
namespace outrun::platform {
namespace {
using driving::X87;using driving::x87_float;
struct Session {
    PcRaceMemory* memory{};
    const PcRaceService* service{};
    const TranslatedModule* module{};
    std::uint8_t* matrix_words{};            // 89B564 / 89B568 / 89B56C
};
constexpr std::uint32_t ReturnSentinel=0xfffffff0u;
constexpr std::uint32_t DeviceSlots=128u;
std::string hex(std::uint32_t v){char t[16];std::snprintf(t,sizeof t,"%X",v);return t;}
or2x86::Fn find(const TranslatedModule& mod,std::uint32_t pc){
    for(const auto* f=mod.functions;f&&f->fn;++f)if(f->pc==pc)return f->fn;
    return nullptr;
}
std::uint32_t pops(const TranslatedModule& mod,std::uint32_t pc){
    for(const auto* p=mod.pops;p&&p->pc;++p)if(p->pc==pc)return p->bytes;
    return 0u;
}
thread_local const TranslatedModule* lookup_module=nullptr;
or2x86::Fn lookup(std::uint32_t pc){return lookup_module?find(*lookup_module,pc):nullptr;}
// Native stack <-> guest words, both ways (the native code between two
// translated instructions may push or pop).
void matrices_out(Session& s){
    auto& st=*s.module->matrices;
    const std::uint32_t w[3]{TranslatedMatrixBase+std::uint32_t(st.current_offset),std::uint32_t(st.depth),std::uint32_t(st.capacity)};
    std::memcpy(s.matrix_words,w,12);
}
void matrices_in(Session& s){
    auto& st=*s.module->matrices;
    std::uint32_t w[3];std::memcpy(w,s.matrix_words,12);
    st.current_offset=std::ptrdiff_t(w[0]-TranslatedMatrixBase);st.depth=std::int32_t(w[1]);
}
// Vertices read by DrawPrimitiveUP for `count` primitives of `type`.
std::uint32_t up_vertices(std::uint32_t type,std::uint32_t count){
    switch(type){
    case 1:return count;case 2:return count*2u;case 3:return count+1u;
    case 4:return count*3u;case 5:case 6:return count+2u;default:return 0u;
    }
}
// COM objects the device hands to the translated code (GetRenderTarget /
// GetDepthStencilSurface surfaces, the occlusion query 89F680): guest object k
// at TranslatedObjectBase + 0x40 + k*8 = {vtable, handle}; the vtable (at
// TranslatedObjectBase) holds thunks TranslatedObjectThunks + offset. They
// persist across calls (the PC keeps them in globals between passes).
constexpr std::uint32_t ObjectVtableWords=64u,ObjectSlots=64u;
thread_local std::array<std::uint32_t,ObjectVtableWords+ObjectSlots*2u> object_words{};
std::uint32_t object_slot(std::uint32_t guest){
    const std::uint32_t first=TranslatedObjectBase+ObjectVtableWords*4u;
    if(guest<first||guest>=first+ObjectSlots*8u||(guest-first)%8u)return ObjectSlots;
    return (guest-first)/8u;
}
// A surface argument: the handle of a proxy, else the value itself (handles
// the native code stored, e.g. 95AFC4 / 95AFCC).
std::uint32_t object_handle(std::uint32_t v){
    const auto k=object_slot(v);
    return k<ObjectSlots&&object_words[ObjectVtableWords+k*2u]?object_words[ObjectVtableWords+k*2u+1u]:v;
}
// IUnknown / IDirect3DQuery9 methods of a proxy (stdcall, `this` first).
std::uint32_t object_call(Session& s,or2x86::Cpu& c,std::uint32_t offset,std::uint32_t& popped){
    const std::uint32_t self=translated_arg(c,0),k=object_slot(self);
    if(k>=ObjectSlots||!object_words[ObjectVtableWords+k*2u])throw std::runtime_error("translated code: COM call +"+hex(offset)+" on an unknown object "+hex(self));
    const std::uint32_t h=object_words[ObjectVtableWords+k*2u+1u];
    if(s.module->com){std::uint32_t eax=0;if(s.module->com(c,h,offset,eax,popped))return eax;}
    if(!s.module->device)throw std::runtime_error("translated code: COM method +"+hex(offset)+" of "+hex(h)+" is not bridged");
    auto& d=*s.module->device;
    switch(offset){
    case 0x04:popped=4;return 1u;                                                         // AddRef
    case 0x08:popped=4;d.release(h);object_words[ObjectVtableWords+k*2u]=0;object_words[ObjectVtableWords+k*2u+1u]=0;return 0u;
    case 0x18:popped=8;d.query_issue(h,translated_arg(c,1));return 0u;                     // IDirect3DQuery9::Issue
    case 0x1c:{popped=16;std::uint32_t n=0;                                                // IDirect3DQuery9::GetData
        const std::uint32_t r=d.query_get_data(h,n);
        if(r==0u&&translated_arg(c,1)&&translated_arg(c,2)>=4u)s.memory->put32(translated_arg(c,1),n);
        return r;}
    default:throw std::runtime_error("translated code: COM method +"+hex(offset)+" is not bridged");
    }
}
// One IDirect3DDevice9 method: the vtable offset, the stack arguments after
// `this`; returns the HRESULT and the argument bytes popped (with `this`).
std::uint32_t device_call(Session& s,or2x86::Cpu& c,std::uint32_t offset,std::uint32_t& popped){
    auto& d=*s.module->device;auto& m=*s.memory;
    auto a=[&](unsigned n){return translated_arg(c,n+1u);};
    auto out=[&](std::uint32_t ptr,std::uint32_t v){m.put32(ptr,v);};
    auto args=[&](unsigned n){popped=4u*(n+1u);};
    switch(offset){
    case 0x08:args(0);return 0u;                                                          // Release (the device stays)
    case 0x94:args(2);d.set_render_target(a(0),object_handle(a(1)));return 0u;          // SetRenderTarget
    case 0x98:args(2);out(a(1),translated_object(d.get_render_target(a(0))));return 0u;  // GetRenderTarget
    case 0x9c:args(1);d.set_depth_stencil_surface(object_handle(a(0)));return 0u;     // SetDepthStencilSurface
    case 0xa0:args(1);out(a(0),translated_object(d.get_depth_stencil_surface()));return 0u;
    case 0xac:{args(6);                                                                    // Clear(count, rects, flags, colour, z, stencil)
        if(a(0)||a(1))throw std::runtime_error("translated code: Clear with rectangles");
        float z;const std::uint32_t zb=a(4);std::memcpy(&z,&zb,4);d.clear(a(2),a(3),z,a(5));return 0u;}
    case 0xbc:{args(1);std::uint32_t v[6];for(unsigned k=0;k<6;++k)v[k]=m.u32(a(0)+k*4);d.set_viewport(v);return 0u;}
    case 0xc0:{args(1);std::uint32_t v[6];d.get_viewport(v);for(unsigned k=0;k<6;++k)out(a(0)+k*4,v[k]);return 0u;}
    case 0x178:{args(3);std::vector<float> v(std::size_t(a(2))*4u);                        // SetVertexShaderConstantF
        for(std::size_t k=0;k<v.size();++k)v[k]=m.f32(a(1)+std::uint32_t(k)*4u);d.set_vertex_shader_constant_f(a(0),v.data(),a(2));return 0u;}
    case 0x1b4:{args(3);std::vector<float> v(std::size_t(a(2))*4u);                        // SetPixelShaderConstantF
        for(std::size_t k=0;k<v.size();++k)v[k]=m.f32(a(1)+std::uint32_t(k)*4u);d.set_pixel_shader_constant_f(a(0),v.data(),a(2));return 0u;}
    case 0xb0:{args(2);std::array<float,16> t{};for(unsigned k=0;k<16;++k)t[k]=m.f32(a(1)+k*4);d.set_transform(a(0),t.data());return 0u;}
    case 0xe4:args(2);d.set_render_state(a(0),a(1));return 0u;
    case 0xe8:args(2);out(a(1),d.get_render_state(a(0)));return 0u;
    case 0x100:args(2);out(a(1),d.get_texture(a(0)));return 0u;
    case 0x104:args(2);d.set_texture(a(0),a(1));return 0u;
    case 0x108:args(3);out(a(2),d.get_texture_stage_state(a(0),a(1)));return 0u;
    case 0x10c:args(3);d.set_texture_stage_state(a(0),a(1),a(2));return 0u;
    case 0x110:args(3);out(a(2),d.get_sampler_state(a(0),a(1)));return 0u;
    case 0x114:args(3);d.set_sampler_state(a(0),a(1),a(2));return 0u;
    case 0x148:args(6);(void)d.draw_indexed_primitive(a(0),std::int32_t(a(1)),a(2),a(3),a(4),a(5));return 0u;
    case 0x14c:{args(4);const std::uint32_t n=up_vertices(a(0),a(1));
        d.draw_primitive_up(a(0),a(1),m.at(a(2),std::size_t(n)*a(3)),a(3));return 0u;}
    case 0x164:args(1);d.set_fvf(a(0));return 0u;
    case 0x170:args(1);d.set_vertex_shader(a(0));return 0u;
    case 0x190:args(4);d.set_stream_source(a(0),a(1),a(2),a(3));return 0u;
    case 0x1a0:args(1);d.set_indices(a(0));return 0u;
    case 0x1ac:args(1);d.set_pixel_shader(a(0));return 0u;
    default:throw std::runtime_error("translated code: IDirect3DDevice9 method +"+hex(offset)+" is not bridged");
    }
}
// D3DX imports (stdcall, 439xxx thunks into the d3dx9_29 IAT) with the port's
// implementations (the generic x87 paths the PC takes).
bool d3dx_import(Session& s,or2x86::Cpu& c,std::uint32_t target){
    auto& m=*s.memory;
    auto mat=[&](std::uint32_t a){driving::PcMatrix16 r{};for(unsigned k=0;k<16;++k)r[k]=m.f32(a+k*4);return r;};
    switch(target){
    case 0x439394u:{                                                     // D3DXMatrixMultiply(out, a, b)
        const auto out=translated_arg(c,0);
        const auto r=driving::pc_d3dx_matrix_multiply(mat(translated_arg(c,1)),mat(translated_arg(c,2)));
        for(unsigned k=0;k<16;++k)m.putf(out+k*4,r[k]);
        translated_return(c,out,12);return true;}
    case 0x4393b2u:{                                                     // D3DXVec4Transform(out, v, m)
        const auto out=translated_arg(c,0),v=translated_arg(c,1);
        const auto r=pc_d3dx_vec4_transform({m.f32(v),m.f32(v+4),m.f32(v+8),m.f32(v+12)},mat(translated_arg(c,2)));
        for(unsigned k=0;k<4;++k)m.putf(out+k*4,r[k]);
        translated_return(c,out,12);return true;}
    case 0x43939au:{                                                     // D3DXVec3TransformCoord(out, v, m)
        const auto out=translated_arg(c,0),v=translated_arg(c,1);
        const auto r=driving::pc_d3dx_vec3_transform_coord({m.f32(v),m.f32(v+4),m.f32(v+8)},mat(translated_arg(c,2)));
        m.putf(out,r[0]);m.putf(out+4,r[1]);m.putf(out+8,r[2]);
        translated_return(c,out,12);return true;}
    case 0x4393e2u:{                                                     // D3DXVec3TransformNormal(out, v, m)
        const auto out=translated_arg(c,0),v=translated_arg(c,1);
        const auto r=driving::pc_d3dx_vec3_transform_normal({m.f32(v),m.f32(v+4),m.f32(v+8)},mat(translated_arg(c,2)));
        m.putf(out,r[0]);m.putf(out+4,r[1]);m.putf(out+8,r[2]);
        translated_return(c,out,12);return true;}
    case 0x4393e8u:{                                                     // D3DXVec3Normalize(out, v)
        const auto out=translated_arg(c,0),v=translated_arg(c,1);
        const auto r=driving::pc_d3dx_vec3_normalize({m.f32(v),m.f32(v+4),m.f32(v+8)});
        m.putf(out,r[0]);m.putf(out+4,r[1]);m.putf(out+8,r[2]);
        translated_return(c,out,8);return true;}
    case 0x4393d0u:{                                                     // D3DXMatrixTranspose(out, m)
        const auto out=translated_arg(c,0);
        const auto r=driving::pc_d3dx_matrix_transpose(mat(translated_arg(c,1)));
        for(unsigned k=0;k<16;++k)m.putf(out+k*4,r[k]);
        translated_return(c,out,8);return true;}
    case 0x43938eu:{                                                     // D3DXMatrixInverse(out, det, m)
        const auto out=translated_arg(c,0),det=translated_arg(c,1),in=translated_arg(c,2);
        float dv=0.0f;
        const driving::Bytes src(m.at(in,64),64);
        std::array<std::uint8_t,64> copy{};std::memcpy(copy.data(),src.data(),64);   // in place: the PC passes out == in
        const bool ok=driving::pc_d3dx_matrix_inverse(driving::Bytes(m.at(out,64,true),64),det?&dv:nullptr,driving::Bytes(copy.data(),64));
        if(det)m.putf(det,dv);
        translated_return(c,ok?out:0u,12);return true;}
    case 0x4393a6u:{                                                     // D3DXMatrixTranslation(out, x, y, z)
        const auto out=translated_arg(c,0);
        for(unsigned k=0;k<16;++k)m.put32(out+k*4,(k%5u==0u)?0x3f800000u:0u);
        m.put32(out+0x30,translated_arg(c,1));m.put32(out+0x34,translated_arg(c,2));m.put32(out+0x38,translated_arg(c,3));
        translated_return(c,out,16);return true;}
    case 0x4393cau:{                                                     // D3DXMatrixScaling(out, sx, sy, sz)
        const auto out=translated_arg(c,0);
        driving::pc_d3dx_matrix_scaling(driving::Bytes(m.at(out,64,true),64),m.f32(c.esp+8),m.f32(c.esp+12),m.f32(c.esp+16));
        translated_return(c,out,16);return true;}
    case 0x4393b8u:case 0x4393beu:case 0x4393dcu:{                       // D3DXMatrixRotationZ / X / Y(out, angle)
        const auto out=translated_arg(c,0);
        const X87 angle(m.f32(c.esp+8));
        const float sn=x87_float(driving::x87_sin(angle)),cs=x87_float(driving::x87_cos(angle));
        driving::PcMatrix16 r{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
        if(target==0x4393b8u){r[0]=cs;r[1]=sn;r[4]=-sn;r[5]=cs;}
        else if(target==0x4393beu){r[5]=cs;r[6]=sn;r[9]=-sn;r[10]=cs;}
        else{r[0]=cs;r[2]=-sn;r[8]=sn;r[10]=cs;}
        for(unsigned k=0;k<16;++k)m.putf(out+k*4,r[k]);
        translated_return(c,out,8);return true;}
    case 0x4393acu:{                                                     // D3DXMatrixOrthoOffCenterRH(out, l, r, b, t, zn, zf)
        const auto out=translated_arg(c,0);
        auto f=[&](unsigned n){return m.f32(c.esp+4u+4u*n);};
        driving::pc_d3dx_ortho_off_center_rh(driving::Bytes(m.at(out,64,true),64),f(1),f(2),f(3),f(4),f(5),f(6));
        translated_return(c,out,28);return true;}
    case 0x43937cu:{                                                     // D3DXMatrixPerspectiveOffCenterRH(out, l, r, b, t, zn, zf)
        const auto out=translated_arg(c,0);
        auto f=[&](unsigned n){return m.f32(c.esp+4u+4u*n);};
        driving::pc_d3dx_perspective_off_center_rh(driving::Bytes(m.at(out,64,true),64),f(1),f(2),f(3),f(4),f(5),f(6));
        translated_return(c,out,28);return true;}
    case 0x4393a0u:{                                                     // D3DXMatrixPerspectiveFovRH(out, fovy, aspect, zn, zf)
        const auto out=translated_arg(c,0);
        auto f=[&](unsigned n){return m.f32(c.esp+4u+4u*n);};
        driving::pc_d3dx_perspective_fov_rh(driving::Bytes(m.at(out,64,true),64),f(1),f(2),f(3),f(4));
        translated_return(c,out,20);return true;}
    case 0x4393c4u:{                                                     // D3DXMatrixLookAtRH(out, eye, at, up)
        const auto out=translated_arg(c,0);
        auto v=[&](unsigned n){const auto p=translated_arg(c,n);return driving::PcVec3{m.f32(p),m.f32(p+4),m.f32(p+8)};};
        driving::pc_d3dx_look_at_rh(driving::Bytes(m.at(out,64,true),64),v(1),v(2),v(3));
        translated_return(c,out,16);return true;}
    case 0x439388u:{                                                     // D3DXPlaneFromPoints(out, a, b, c)
        const auto out=translated_arg(c,0);
        auto p=[&](unsigned n){const auto q=translated_arg(c,n);return driving::CourseProbe{m.f32(q),m.f32(q+4),m.f32(q+8)};};
        const auto r=driving::pc_d3dx_plane_from_points(p(1),p(2),p(3));
        for(unsigned k=0;k<4;++k)m.putf(out+k*4,r[k]);
        translated_return(c,out,16);return true;}
    case 0x58f7bau:{                                                     // CRT _CIfmod: ST1 mod ST0 (FPREM until complete), ST0 popped
        auto& x=c.fpu;
        std::swap(x.st(0),x.st(1));x.prem(false);x.st(1)=x.st(0);x.pop();   // fxch, fprem, fstp st(1)
        c.esp+=4u;return true;}
    case 0x582bb3u:                                                      // __security_check_cookie (ECX): the cookie is never broken here
        c.esp+=4u;return true;
    case 0x580f40u:                                                      // CRT rand()
        if(!s.module->crt_random)return false;
        c.eax=frontend_crt_random_580f40(*s.module->crt_random);c.esp+=4u;return true;
    default:return false;
    }
}
}
std::uint32_t translated_object(std::uint32_t handle){
    if(!handle)return 0u;
    for(std::uint32_t k=0;k<ObjectSlots;++k){
        auto& w=object_words[ObjectVtableWords+k*2u];
        if(w)continue;
        w=TranslatedObjectBase;object_words[ObjectVtableWords+k*2u+1u]=handle;
        return TranslatedObjectBase+ObjectVtableWords*4u+k*8u;
    }
    throw std::runtime_error("translated code: no free COM object proxy");
}
std::uint32_t translated_arg(or2x86::Cpu& c,unsigned n){
    const std::uint32_t a=c.esp+4u+4u*n;
    if(a+4u>TranslatedStackBase+TranslatedStackSize)return 0u;   // past the outermost frame
    return or2x86::ld32(c,a);
}
void translated_return(or2x86::Cpu& c,std::uint32_t eax,std::uint32_t popped){
    c.eax=eax;c.esp+=4u+popped;
}
std::uint32_t translated_call(PcRaceMemory& m,const PcRaceService& service,const TranslatedModule& module,
                              std::uint32_t pc,std::uint32_t ecx,std::initializer_list<std::uint32_t> args){
    return translated_call_registers(m,service,module,pc,TranslatedRegisters{0,ecx,0,0,0,0},args);
}
std::uint32_t translated_call_registers(PcRaceMemory& m,const PcRaceService& service,const TranslatedModule& module,
                              std::uint32_t pc,const TranslatedRegisters& regs,std::initializer_list<std::uint32_t> args){
    return translated_call_list(m,service,module,pc,regs,std::vector<std::uint32_t>(args));
}
bool translated_has(const TranslatedModule& module,std::uint32_t pc){return find(module,pc)!=nullptr;}
static std::map<std::uint32_t,std::uint64_t>* g_trcount=nullptr;
static void trcount_dump(){if(!g_trcount)return;std::fprintf(stderr,"[trcount]");for(auto& [k,v]:*g_trcount)std::fprintf(stderr," %x=%llu",k,(unsigned long long)v);std::fputc(10,stderr);}

std::uint32_t translated_call_list(PcRaceMemory& m,const PcRaceService& service,const TranslatedModule& module,
                              std::uint32_t pc,const TranslatedRegisters& regs,const std::vector<std::uint32_t>& list){
    const or2x86::Fn fn=find(module,pc);
    if(!fn)throw std::logic_error("translated call: "+hex(pc)+" is not translated");
    {static const bool on=std::getenv("OR2_TR_COUNT")!=nullptr;if(on){if(!g_trcount){g_trcount=new std::map<std::uint32_t,std::uint64_t>();std::atexit(trcount_dump);}++(*g_trcount)[pc];}}
    // One guest stack for every translated call: a nested call (a service that runs
    // translated code again) starts below the frame of the call in progress.
    static thread_local std::array<std::uint8_t,TranslatedStackSize> stack{};
    static thread_local std::vector<or2x86::Cpu*> active;
    const auto mark=m.mark();
    if(!m.mapped(TranslatedStackBase,4))m.map(TranslatedStackBase,stack.data(),stack.size());
    for(const auto* d=module.code_data;d&&d->size;++d)m.map_const(d->base,d->bytes?d->bytes:exe_image_bytes(d->base,d->size),d->size);
    Session s{&m,&service,&module};
    // The device object: [89BD60] -> object -> vtable -> thunk addresses.
    std::array<std::uint32_t,2+DeviceSlots> device_words{};
    std::uint32_t device_pointer=TranslatedDeviceBase;
    if(module.device){
        device_words[0]=TranslatedDeviceBase+8u;
        for(std::uint32_t k=0;k<DeviceSlots;++k)device_words[2+k]=TranslatedDeviceThunks+k*4u;
        m.map(TranslatedDeviceBase,reinterpret_cast<std::uint8_t*>(device_words.data()),sizeof device_words);
        m.map(0x89bd60u,reinterpret_cast<std::uint8_t*>(&device_pointer),4);
    }
    if(module.device||module.com){
        for(std::uint32_t k=0;k<ObjectVtableWords;++k)object_words[k]=TranslatedObjectThunks+k*4u;
        m.map(TranslatedObjectBase,reinterpret_cast<std::uint8_t*>(object_words.data()),sizeof object_words);
    }
    std::array<std::uint8_t,12> matrix_words{};
    if(module.matrices){
        auto& st=*module.matrices;
        m.map(TranslatedMatrixBase,st.storage.data(),st.storage.size());
        m.map(0x89b564u,matrix_words.data(),matrix_words.size());
        s.matrix_words=matrix_words.data();
        matrices_out(s);
    }
    or2x86::Cpu c{};
    c.user=&s;
    c.mem_user=&m;
    c.mem_at=[](void* u,std::uint32_t a,std::uint32_t n,bool w)->std::uint8_t*{return static_cast<PcRaceMemory*>(u)->at(a,n,w);};
    const TranslatedModule* previous_lookup=lookup_module;
    lookup_module=&module;
    c.lookup=lookup;
    c.trap=[](or2x86::Cpu&,std::uint32_t at,const char* why){throw std::runtime_error("translated code at "+hex(at)+": "+why);};
    c.external=[](or2x86::Cpu& c,std::uint32_t target){
        auto& s=*static_cast<Session*>(c.user);
        if(const auto g=find(*s.module,target)){g(c);return;}
        for(const auto* n=s.module->next;n;n=n->next)if(const auto g=find(*n,target)){g(c);return;}
        if(target==0x582920u){   // _chkstk (VS2005, EAX = frame size) outside the module: ESP = call-site ESP - EAX, EAX = return address
            const std::uint32_t ret=or2x86::ld32(c,c.esp);c.esp=c.esp+4u-c.eax;c.eax=ret;return;}
        if(s.module->device&&target>=TranslatedDeviceThunks&&target<TranslatedDeviceThunks+DeviceSlots*4u){
            std::uint32_t popped=0;
            const std::uint32_t eax=device_call(s,c,target-TranslatedDeviceThunks,popped);
            translated_return(c,eax,popped);
            return;
        }
        if((s.module->device||s.module->com)&&target>=TranslatedObjectThunks&&target<TranslatedObjectThunks+ObjectVtableWords*4u){
            std::uint32_t popped=0;
            const std::uint32_t eax=object_call(s,c,target-TranslatedObjectThunks,popped);
            translated_return(c,eax,popped);
            return;
        }
        if(d3dx_import(s,c,target))return;
        if(s.module->matrices)matrices_in(s);
        if(s.module->special&&s.module->special(c,target)){if(s.module->matrices)matrices_out(s);return;}
        PcRaceCall k{};k.pc=target;k.eax=c.eax;k.ecx=c.ecx;k.argc=std::uint32_t(k.args.size());
        for(unsigned i=0;i<k.args.size();++i)k.args[i]=translated_arg(c,i);
        const std::uint32_t eax=(*s.service)(k);
        if(s.module->matrices)matrices_out(s);
        translated_return(c,eax,pops(*s.module,target));
    };
    std::uint32_t esp=active.empty()?TranslatedStackBase+TranslatedStackSize-0x20u:active.back()->esp-0x40u;
    if(esp<TranslatedStackBase+0x1000u)throw std::runtime_error("translated call "+hex(pc)+": guest stack exhausted");
    for(auto it=list.rbegin();it!=list.rend();++it){esp-=4u;or2x86::st32(c,esp,*it);}
    esp-=4u;or2x86::st32(c,esp,ReturnSentinel);
    c.fs_base=module.fs_base;
    c.esp=esp;c.eax=regs.eax;c.ecx=regs.ecx;c.edx=regs.edx;c.ebx=regs.ebx;c.esi=regs.esi;c.edi=regs.edi;
    active.push_back(&c);
    auto finish=[&]{
        if(module.matrices)matrices_in(s);
        active.pop_back();lookup_module=previous_lookup;m.release(mark);};
    try{fn(c);}
    catch(...){
        // OR2_TR_STACK: the code addresses on the guest stack at the fault (the call chain).
        if(std::getenv("OR2_TR_STACK")&&active.size()==1u){
            std::fprintf(stderr,"[tr] fault in %s, eip-ish stack:",hex(pc).c_str());
            for(std::uint32_t a=c.esp;a+4u<=TranslatedStackBase+TranslatedStackSize;a+=4u){
                const auto v=or2x86::ld32(c,a);if(v>=0x401000u&&v<0x590000u)std::fprintf(stderr," %06X",v);}
            std::fprintf(stderr,"\n");
        }
        finish();throw;}
    finish();
    if(c.esp!=esp+4u&&c.esp!=esp+4u+4u*std::uint32_t(list.size()))throw std::runtime_error("translated call "+hex(pc)+": stack not balanced");   // cdecl, or stdcall popping its arguments
    return c.eax;
}
namespace {
// The visible part of a writable region (later mappings shadow earlier ones) and
// its bytes; `live` false once a service dropped or remapped the region's
// backing store (the piece is then neither written back nor compared).
struct DiffSaved {
    std::uint32_t base; std::uint8_t* data; std::size_t size; std::vector<std::uint8_t> bytes;
    PcRaceMemory::Region region; bool live{true};
};
void diff_check_live(const PcRaceMemory& m,std::vector<DiffSaved>& saved){
    for(auto& s:saved){
        if(!s.live)continue;
        bool found=false;
        for(const auto& r:m.regions())if(r.data==s.region.data&&r.size==s.region.size&&r.base==s.region.base){found=true;break;}
        s.live=found;
    }
}
std::vector<DiffSaved> diff_visible(const PcRaceMemory& m){
    std::vector<DiffSaved> out;
    std::vector<std::pair<std::uint64_t,std::uint64_t>> covered;   // sorted, disjoint [lo, hi)
    const auto& regions=m.regions();
    for(auto it=regions.rbegin();it!=regions.rend();++it){
        const auto& r=*it;
        if(!r.size)continue;
        const std::uint64_t lo=r.base,hi=lo+r.size;
        if(r.writable){
            std::uint64_t at=lo;
            for(const auto& [clo,chi]:covered){
                if(chi<=at)continue;
                if(clo>=hi)break;
                if(clo>at)out.push_back({std::uint32_t(at),r.data+(at-lo),std::size_t(clo-at),{},r});
                at=std::max(at,chi);
                if(at>=hi)break;
            }
            if(at<hi)out.push_back({std::uint32_t(at),r.data+(at-lo),std::size_t(hi-at),{},r});
        }
        covered.push_back({lo,hi});
        std::sort(covered.begin(),covered.end());
        std::vector<std::pair<std::uint64_t,std::uint64_t>> merged;
        for(const auto& x:covered){if(!merged.empty()&&x.first<=merged.back().second)merged.back().second=std::max(merged.back().second,x.second);else merged.push_back(x);}
        covered.swap(merged);
    }
    for(auto& s:out)s.bytes.assign(s.data,s.data+s.size);
    return out;
}
struct DiffRun {
    // A changed byte run of a service call (region index, offset, bytes); region
    // SIZE_MAX is the matrix stack storage.
    struct Delta { std::size_t region,offset; std::vector<std::uint8_t> bytes; };
    struct Call {
        std::uint32_t pc,ecx,argc; std::array<std::uint32_t,12> args; std::uint32_t result;
        std::vector<Delta> deltas; std::ptrdiff_t matrix_offset; std::int32_t matrix_depth;
        std::string thrown;   // the service threw this (replayed as a throw)
        std::vector<std::uint8_t> pointee;   // RaceDiffOptions::pointees bytes
        std::vector<std::uint8_t> pointee_after;   // the same after the call (the service's output)
    };
    std::vector<Call> calls;
    std::string error;
    std::vector<std::string> replay_errors;
};
void diff_deltas(std::vector<DiffRun::Delta>& out,std::size_t region,const std::uint8_t* before,const std::uint8_t* now,std::size_t n){
    for(std::size_t i=0;i<n;){
        if(before[i]==now[i]){++i;continue;}
        std::size_t j=i;while(j<n&&before[j]!=now[j])++j;
        out.push_back({region,i,std::vector<std::uint8_t>(now+i,now+j)});i=j;
    }
}
const RaceDiffPointee* diff_pointee(const RaceDiffOptions& o,std::uint32_t pc){
    for(const auto& p:o.pointees)if(p.pc==pc)return &p;
    return nullptr;
}
std::vector<std::uint8_t> diff_pointee_bytes(const PcRaceMemory& m,const RaceDiffOptions& o,const PcRaceCall& k){
    const auto* p=diff_pointee(o,k.pc);
    if(!p||!m.mapped(k.args[p->arg]+p->offset,p->size))return {};
    const auto* b=m.at(k.args[p->arg]+p->offset,p->size);
    return std::vector<std::uint8_t>(b,b+p->size);
}
DiffRun diff_record(PcRaceContext& c,const std::function<void(PcRaceContext&)>& body,const RaceDiffOptions& o,
                    const std::vector<DiffSaved>& saved,const DiffRun* replay){
    DiffRun run;
    std::size_t next=0;
    auto ignored=[&](std::uint32_t pc){return std::find(o.ignored.begin(),o.ignored.end(),pc)!=o.ignored.end();};
    auto apply=[&](const DiffRun::Call& r){
        for(const auto& d:r.deltas){
            if(d.region!=SIZE_MAX&&!saved[d.region].live)continue;
            std::uint8_t* p=d.region==SIZE_MAX?c.matrices.storage.data():saved[d.region].data;
            std::memcpy(p+d.offset,d.bytes.data(),d.bytes.size());
        }
        c.matrices.current_offset=r.matrix_offset;c.matrices.depth=r.matrix_depth;
    };
    PcRaceService recorder=[&](const PcRaceCall& k)->std::uint32_t{
        if(replay){
            // Skip the reference's calls the candidate leaves out (ignored pcs), applying their effects.
            while(next<replay->calls.size()&&replay->calls[next].pc!=k.pc&&ignored(replay->calls[next].pc))apply(replay->calls[next++]);
            if(next>=replay->calls.size()||replay->calls[next].pc!=k.pc){
                char t[96];std::snprintf(t,sizeof t,"call %zu: candidate %08X where the reference has %08X",next,k.pc,
                    next<replay->calls.size()?replay->calls[next].pc:0u);
                run.replay_errors.push_back(t);
                throw std::runtime_error("diff replay: call sequence differs");
            }
            const auto& r=replay->calls[next++];
            run.calls.push_back({k.pc,k.ecx,k.argc,k.args,r.result,{},0,0,{},diff_pointee_bytes(c.m,o,k),{}});
            apply(r);
            if(const auto* p=diff_pointee(o,k.pc);p&&!r.pointee_after.empty()&&c.m.mapped(k.args[p->arg]+p->offset,p->size))
                std::memcpy(c.m.at(k.args[p->arg]+p->offset,p->size,true),r.pointee_after.data(),p->size);
            if(!r.thrown.empty())throw std::runtime_error(r.thrown);
            return r.result;
        }
        std::vector<std::vector<std::uint8_t>> before;
        std::vector<std::uint8_t> stack;
        if(o.replay){
            before.reserve(saved.size());for(const auto& s:saved)before.emplace_back(s.data,s.data+s.size);
            stack.assign(c.matrices.storage.data(),c.matrices.storage.data()+c.matrices.storage.size());
        }
        std::uint32_t r=0;std::string thrown;
        auto pointee=diff_pointee_bytes(c.m,o,k);
        try{r=c.service(k);}catch(const std::exception& e){if(!o.replay)throw;thrown=e.what();}
        auto pointee_after=diff_pointee_bytes(c.m,o,k);
        if(o.replay||!ignored(k.pc)){
            DiffRun::Call call{k.pc,k.ecx,k.argc,k.args,r,{},c.matrices.current_offset,c.matrices.depth,thrown,std::move(pointee),std::move(pointee_after)};
            if(o.replay){
                diff_check_live(c.m,const_cast<std::vector<DiffSaved>&>(saved));
                for(std::size_t i=0;i<saved.size();++i)if(saved[i].live)diff_deltas(call.deltas,i,before[i].data(),saved[i].data,saved[i].size);
                diff_deltas(call.deltas,SIZE_MAX,stack.data(),c.matrices.storage.data(),stack.size());
            }
            run.calls.push_back(std::move(call));
        }
        if(!thrown.empty())throw std::runtime_error(thrown);
        return r;};
    PcRaceContext inner{c.m,c.matrices,recorder,c.draws};
    try{body(inner);}catch(const std::exception& e){run.error=e.what();}
    if(replay&&run.replay_errors.empty()){
        while(next<replay->calls.size()&&ignored(replay->calls[next].pc))apply(replay->calls[next++]);
        if(next<replay->calls.size()){
            char t[96];std::snprintf(t,sizeof t,"call %zu: reference %08X not made by the candidate",next,replay->calls[next].pc);
            run.replay_errors.push_back(t);
        }
    }
    return run;
}
}
bool race_diff_run(PcRaceContext& c,const char* name,const std::function<void(PcRaceContext&)>& reference,
                   const std::function<void(PcRaceContext&)>& candidate,const RaceDiffOptions& o){
    // The writable regions (each backing store once) and the matrix stack.
    std::vector<DiffSaved> saved=diff_visible(c.m);
    if(o.pure){
        static std::map<std::string,std::pair<std::uint64_t,std::uint64_t>> counts;
        auto& [runs,bad]=counts[name];++runs;
        const std::vector<std::uint8_t> stack0(c.matrices.storage.data(),c.matrices.storage.data()+c.matrices.storage.size());
        const auto offset0=c.matrices.current_offset;const auto depth0=c.matrices.depth;
        auto run_mock=[&](const std::function<void(PcRaceContext&)>& body,DiffRun& r){
            PcRaceService mock=[&](const PcRaceCall& k)->std::uint32_t{
                const std::uint32_t v=o.mock?o.mock(k):0u;
                r.calls.push_back({k.pc,k.ecx,k.argc,k.args,v,{},0,0,{},diff_pointee_bytes(c.m,o,k),{}});return v;};
            PcRaceContext inner{c.m,c.matrices,mock,c.draws};
            try{body(inner);}catch(const std::exception& e){r.error=e.what();}
        };
        auto put_back=[&]{
            for(auto& s:saved)std::memcpy(s.data,s.bytes.data(),s.size);
            std::memcpy(c.matrices.storage.data(),stack0.data(),stack0.size());c.matrices.current_offset=offset0;c.matrices.depth=depth0;
        };
        DiffRun a,b;
        run_mock(reference,a);
        std::vector<std::vector<std::uint8_t>> after;for(const auto& s:saved)after.emplace_back(s.data,s.data+s.size);
        put_back();
        run_mock(candidate,b);
        unsigned found=0;char t[200];
        auto report=[&](const std::string& what){if(found++<4)std::fprintf(stderr,"[diff %s] run %llu: %s\n",name,(unsigned long long)runs,what.c_str());};
        if(a.error!=b.error)report("error: reference \""+a.error+"\" candidate \""+b.error+"\"");
        for(std::size_t k=0;k<saved.size();++k)for(std::size_t i=0;i<saved[k].size;++i)if(saved[k].data[i]!=after[k][i]){
            std::snprintf(t,sizeof t,"byte %08X: reference %02X candidate %02X (before %02X)",unsigned(saved[k].base+i),after[k][i],saved[k].data[i],saved[k].bytes[i]);report(t);break;}
        for(std::size_t i=0;i<std::max(a.calls.size(),b.calls.size());++i){
            if(i>=a.calls.size()||i>=b.calls.size()){
                std::snprintf(t,sizeof t,"call %zu: %s only: %08X",i,i<a.calls.size()?"reference":"candidate",(i<a.calls.size()?a.calls[i]:b.calls[i]).pc);report(t);break;}
            const auto& x=a.calls[i];const auto& y=b.calls[i];
            bool same=x.pc==y.pc&&(y.ecx==0u||x.ecx==y.ecx);
            const bool by_record=diff_pointee(o,y.pc)!=nullptr;
            for(std::uint32_t n=0;same&&n<y.argc&&n<12u;++n)if(!by_record||n!=diff_pointee(o,y.pc)->arg)same=x.args[n]==y.args[n];
            if(by_record)same=same&&x.pointee==y.pointee;
            if(!same){std::snprintf(t,sizeof t,"call %zu: reference %08X(%08X %08X %08X), candidate %08X(%08X %08X %08X)",
                i,x.pc,x.args[0],x.args[1],x.args[2],y.pc,y.args[0],y.args[1],y.args[2]);report(t);break;}
        }
        put_back();
        if(found)++bad;
        if(runs%1000u==0u)std::fprintf(stderr,"[diff %s] %llu runs, %llu with differences\n",name,(unsigned long long)runs,(unsigned long long)bad);
        return found==0;
    }
    if(o.calls_only){
        static std::map<std::string,std::pair<std::uint64_t,std::uint64_t>> counts;
        auto& [runs,bad]=counts[name];++runs;
        DiffRun b;
        PcRaceService mock=[&](const PcRaceCall& k)->std::uint32_t{const std::uint32_t r=o.mock?o.mock(k):0u;
            b.calls.push_back({k.pc,k.ecx,k.argc,k.args,r,{},0,0,{},diff_pointee_bytes(c.m,o,k),{}});return r;};
        {PcRaceContext inner{c.m,c.matrices,mock,c.draws};try{candidate(inner);}catch(const std::exception& e){b.error=e.what();}}
        struct Write { std::uint32_t at; std::uint8_t value; };
        std::vector<Write> writes;
        for(auto& s:saved)for(std::size_t i=0;i<s.size;++i)if(s.data[i]!=s.bytes[i]){writes.push_back({std::uint32_t(s.base+i),s.data[i]});s.data[i]=s.bytes[i];}
        const DiffRun a=diff_record(c,reference,RaceDiffOptions{o.ignored},saved,nullptr);
        unsigned found=0;char t[200];
        auto report=[&](const std::string& what){if(found++<4)std::fprintf(stderr,"[diff %s] run %llu: %s\n",name,(unsigned long long)runs,what.c_str());};
        if(a.error!=b.error)report("error: reference \""+a.error+"\" candidate \""+b.error+"\"");
        std::size_t j=0;
        for(const auto& y:b.calls){
            while(j<a.calls.size()&&a.calls[j].pc!=y.pc&&std::find(o.ignored.begin(),o.ignored.end(),a.calls[j].pc)!=o.ignored.end())++j;
            if(j>=a.calls.size()){std::snprintf(t,sizeof t,"candidate call %08X not made by the reference",y.pc);report(t);break;}
            const auto& x=a.calls[j++];
            bool same=x.pc==y.pc&&(y.ecx==0u||x.ecx==y.ecx);
            for(std::uint32_t n=0;same&&n<y.argc&&n<12u;++n)same=x.args[n]==y.args[n];
            if(!same){std::snprintf(t,sizeof t,"call %zu: reference %08X(%08X %08X %08X), candidate %08X(%08X %08X %08X)",
                j-1,x.pc,x.args[0],x.args[1],x.args[2],y.pc,y.args[0],y.args[1],y.args[2]);report(t);break;}
        }
        if(j<a.calls.size()&&!found){std::snprintf(t,sizeof t,"reference call %08X not made by the candidate",a.calls[j].pc);report(t);}
        for(const auto& w:writes)if(c.m.mapped(w.at,1)&&c.m.u8(w.at)!=w.value){
            std::snprintf(t,sizeof t,"byte %08X: reference %02X candidate %02X",unsigned(w.at),c.m.u8(w.at),w.value);report(t);break;}
        if(found)++bad;
        if(runs%1000u==0u)std::fprintf(stderr,"[diff %s] %llu runs, %llu with differences\n",name,(unsigned long long)runs,(unsigned long long)bad);
        return found==0;
    }
    const std::vector<std::uint8_t> stack(c.matrices.storage.data(),c.matrices.storage.data()+c.matrices.storage.size());
    const auto offset=c.matrices.current_offset;const auto depth=c.matrices.depth;
    const std::size_t draws=c.draws?c.draws->size():0u;
    std::vector<std::vector<std::uint8_t>> state;
    for(const auto& x:o.state)state.emplace_back(static_cast<std::uint8_t*>(x.data),static_cast<std::uint8_t*>(x.data)+x.size);
    auto restore=[&](bool memory_only){
        diff_check_live(c.m,saved);
        for(const auto& s:saved)if(s.live)std::memcpy(s.data,s.bytes.data(),s.size);
        std::memcpy(c.matrices.storage.data(),stack.data(),stack.size());c.matrices.current_offset=offset;c.matrices.depth=depth;
        if(c.draws)c.draws->resize(draws);
        if(memory_only)return;
        {std::size_t k=0;for(const auto& x:o.state){std::memcpy(x.data,state[k].data(),x.size);++k;}}
        if(o.rewind)o.rewind();
    };
    const DiffRun a=diff_record(c,reference,o,saved,nullptr);
    diff_check_live(c.m,saved);
    std::vector<std::vector<std::uint8_t>> after;after.reserve(saved.size());
    for(const auto& s:saved)if(s.live)after.emplace_back(s.data,s.data+s.size);else after.emplace_back();
    const std::vector<std::uint8_t> stack_a(c.matrices.storage.data(),c.matrices.storage.data()+c.matrices.storage.size());
    const auto offset_a=c.matrices.current_offset;const auto depth_a=c.matrices.depth;
    const std::size_t draws_a=c.draws?c.draws->size():0u;
    // In replay mode the services' outside state stays as the reference left it.
    std::vector<std::vector<std::uint8_t>> state_a;
    for(const auto& x:o.state)state_a.emplace_back(static_cast<std::uint8_t*>(x.data),static_cast<std::uint8_t*>(x.data)+x.size);
    restore(o.replay);
    const DiffRun b=diff_record(c,candidate,o,saved,o.replay?&a:nullptr);
    static std::map<std::string,std::pair<std::uint64_t,std::uint64_t>> counts;
    auto& [runs,bad]=counts[name];++runs;
    unsigned found=0;
    auto report=[&](const std::string& what){if(found++<4)std::fprintf(stderr,"[diff %s] run %llu: %s\n",name,(unsigned long long)runs,what.c_str());};
    char t[200];
    for(const auto& e:b.replay_errors)report(e);
    if(a.error!=b.error&&b.replay_errors.empty())report("error: reference \""+a.error+"\" candidate \""+b.error+"\"");
    diff_check_live(c.m,saved);
    for(std::size_t k=0;k<saved.size();++k){
        const auto& s=saved[k];
        if(!s.live||after[k].size()!=s.size)continue;
        for(std::size_t i=0;i<s.size;++i)if(s.data[i]!=after[k][i]){
            std::snprintf(t,sizeof t,"byte %08X: reference %02X candidate %02X (before %02X)",unsigned(s.base+i),after[k][i],s.data[i],s.bytes[i]);report(t);break;}
    }
    // The matrix stack up to its current matrix (the slots above are scratch).
    const std::size_t live=std::min<std::size_t>(c.matrices.storage.size(),std::size_t(std::max<std::ptrdiff_t>(0,offset_a))+64u);
    if(offset_a!=c.matrices.current_offset||depth_a!=c.matrices.depth||std::memcmp(stack_a.data(),c.matrices.storage.data(),live)!=0)
        report("matrix stack differs");
    if(!o.replay&&draws_a!=(c.draws?c.draws->size():0u)){std::snprintf(t,sizeof t,"draws: reference %zu candidate %zu",draws_a,c.draws?c.draws->size():0u);report(t);}
    {std::size_t k=0;for(const auto& x:o.state){if(std::memcmp(x.data,state_a[k].data(),x.size)!=0){std::snprintf(t,sizeof t,"service state %zu differs",k);report(t);}++k;}}
    if(!o.replay)for(std::size_t i=0;i<std::max(a.calls.size(),b.calls.size());++i){
        if(i>=a.calls.size()||i>=b.calls.size()){
            const auto& x=i<a.calls.size()?a.calls[i]:b.calls[i];
            std::snprintf(t,sizeof t,"call %zu: %s only: %08X",i,i<a.calls.size()?"reference":"candidate",x.pc);report(t);break;}
        const auto& x=a.calls[i];const auto& y=b.calls[i];
        bool same=x.pc==y.pc&&x.result==y.result&&(y.ecx==0u||x.ecx==y.ecx);
        const bool by_record=diff_pointee(o,y.pc)!=nullptr;
        for(std::uint32_t n=0;same&&n<y.argc&&n<12u;++n)if(!by_record||n!=diff_pointee(o,y.pc)->arg)same=x.args[n]==y.args[n];
        if(by_record)same=same&&x.pointee==y.pointee;
        if(!same){std::snprintf(t,sizeof t,"call %zu: reference %08X(ecx %08X, %08X %08X %08X) = %08X, candidate %08X(ecx %08X, %08X %08X %08X) = %08X",
            i,x.pc,x.ecx,x.args[0],x.args[1],x.args[2],x.result,y.pc,y.ecx,y.args[0],y.args[1],y.args[2],y.result);report(t);break;}
    }else{
        // The arguments of the replayed calls (the reference log less the ignored pcs).
        std::size_t i=0;
        for(const auto& x:a.calls){
            if(i>=b.calls.size())break;
            const auto& y=b.calls[i];
            if(x.pc!=y.pc)continue;   // an ignored reference call
            bool same=y.ecx==0u||x.ecx==y.ecx;
            const bool by_record=diff_pointee(o,y.pc)!=nullptr;
            for(std::uint32_t n=0;same&&n<y.argc&&n<12u;++n)if(!by_record||n!=diff_pointee(o,y.pc)->arg)same=x.args[n]==y.args[n];
            if(by_record)same=same&&x.pointee==y.pointee;
            if(!same){std::snprintf(t,sizeof t,"call %zu %08X: reference (ecx %08X, %08X %08X %08X), candidate (ecx %08X, %08X %08X %08X)",
                i,x.pc,x.ecx,x.args[0],x.args[1],x.args[2],y.ecx,y.args[0],y.args[1],y.args[2]);report(t);break;}
            ++i;
        }
    }
    if(found)++bad;
    // OR2_DIFF_VERBOSE: both call logs of a run that differs.
    if(found&&std::getenv("OR2_DIFF_VERBOSE")){
        for(const auto* r:{&a,&b}){
            std::fprintf(stderr,"[diff %s] %s (error \"%s\"):",name,r==&a?"reference":"candidate",r->error.c_str());
            for(const auto& x:r->calls)std::fprintf(stderr," %X(%X,%X)%s",x.pc,x.args[0],x.args[1],x.deltas.empty()?"":"*");
            std::fputc(10,stderr);
        }
    }
    if(!o.keep)restore(false);
    if(runs%1000u==0u)std::fprintf(stderr,"[diff %s] %llu runs, %llu with differences\n",name,(unsigned long long)runs,(unsigned long long)bad);
    return found==0;
}
}

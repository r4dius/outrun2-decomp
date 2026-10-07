#include "d3d11_d3d9.hpp"
#include "pc_hlsl_shaders.hpp"
#include "fxaa_hlsl.hpp"
#include <cmath>
#include <d3dcompiler.h>
#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
namespace outrun::xbox_runtime {
using namespace platform::d3d9;
namespace {
constexpr unsigned KTexture=5,KSurface=6;
void check(HRESULT hr,const char* what){if(FAILED(hr))throw std::runtime_error(std::string(what)+" failed (HRESULT "+std::to_string(unsigned(hr))+")");}
ComPtr<ID3DBlob> compile(const std::string& source,const char* entry,const char* target){
    ComPtr<ID3DBlob> code,errors;
    const HRESULT hr=D3DCompile(source.data(),source.size(),"pc_shader",nullptr,nullptr,entry,target,D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&code,&errors);
    if(FAILED(hr))throw std::runtime_error(std::string("HLSL compile: ")+(errors?static_cast<const char*>(errors->GetBufferPointer()):"unknown error"));
    return code;
}
// D3D9 and D3D11 share the numbering of comparison functions, stencil
// operations, blend factors 1..11, blend operations and address modes.
D3D11_COMPARISON_FUNC comparison(unsigned f){return D3D11_COMPARISON_FUNC(std::clamp(f,1u,8u));}
D3D11_STENCIL_OP stencil_op(unsigned v){return D3D11_STENCIL_OP(v>=1&&v<=8?v:1u);}
D3D11_BLEND blend_factor(unsigned v){return v==0?D3D11_BLEND_ONE:D3D11_BLEND(std::min(v,11u));}
// The alpha channel only takes alpha factors; colour factors read the alpha.
D3D11_BLEND alpha_factor(unsigned v){
    switch(blend_factor(v)){case D3D11_BLEND_SRC_COLOR:return D3D11_BLEND_SRC_ALPHA;case D3D11_BLEND_INV_SRC_COLOR:return D3D11_BLEND_INV_SRC_ALPHA;
    case D3D11_BLEND_DEST_COLOR:return D3D11_BLEND_DEST_ALPHA;case D3D11_BLEND_INV_DEST_COLOR:return D3D11_BLEND_INV_DEST_ALPHA;default:return blend_factor(v);}
}
D3D11_BLEND_OP blend_op(unsigned v){return D3D11_BLEND_OP(v>=1&&v<=5?v:1u);}
D3D11_TEXTURE_ADDRESS_MODE address(unsigned v){return D3D11_TEXTURE_ADDRESS_MODE(v>=1&&v<=5?v:1u);}
template<class T> std::string key(const T& d){return std::string(reinterpret_cast<const char*>(&d),sizeof d);}
void colour(std::uint32_t c,float* out){out[0]=float((c>>16)&255)/255;out[1]=float((c>>8)&255)/255;out[2]=float(c&255)/255;out[3]=float(c>>24)/255;}
}
D3D11D3D9Device::D3D11D3D9Device(ID3D11Device* device,ID3D11DeviceContext* context,unsigned w,unsigned h)
    :PcSoftD3D9Device(w,h),device_(device),context_(context){
    const auto vs=compile(hlsl::ffp,"vs_main","vs_4_1");
    check(device_->CreateVertexShader(vs->GetBufferPointer(),vs->GetBufferSize(),nullptr,&vertex_),"CreateVertexShader");
    const auto ps=compile(hlsl::ffp,"ps_main","ps_4_1");
    check(device_->CreatePixelShader(ps->GetBufferPointer(),ps->GetBufferSize(),nullptr,&ffp_),"CreatePixelShader");
    D3D11_INPUT_ELEMENT_DESC elements[12]{};
    const char* names[3]{"POSITION","COLOR","TEXCOORD"};
    for(unsigned a=0;a<12;++a){auto& e=elements[a];e.SemanticName=a==0?names[0]:a<3?names[1]:names[2];
        e.SemanticIndex=a==0?0:a<3?a-1:a-3;e.Format=a==11?DXGI_FORMAT_R32_FLOAT:DXGI_FORMAT_R32G32B32A32_FLOAT;
        e.AlignedByteOffset=UINT(a*4*sizeof(float));e.InputSlotClass=D3D11_INPUT_PER_VERTEX_DATA;}
    static_assert(offsetof(Vertex,fog)==11*4*sizeof(float),"vertex layout");
    check(device_->CreateInputLayout(elements,12,vs->GetBufferPointer(),vs->GetBufferSize(),&layout_),"CreateInputLayout");
    const UINT sizes[3]{8*16,512,48};
    for(unsigned k=0;k<3;++k){D3D11_BUFFER_DESC d{};d.ByteWidth=sizes[k];d.Usage=D3D11_USAGE_DYNAMIC;d.BindFlags=D3D11_BIND_CONSTANT_BUFFER;d.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
        check(device_->CreateBuffer(&d,nullptr,&ubo_[k]),"CreateBuffer(constants)");}
    allocate_back(w,h);
    {   const auto fvs=compile(hlsl::fxaa,"vs_main","vs_4_1"),fps=compile(hlsl::fxaa,"ps_main","ps_4_1");
        check(device_->CreateVertexShader(fvs->GetBufferPointer(),fvs->GetBufferSize(),nullptr,&fxaa_vertex_),"CreateVertexShader(FXAA)");
        check(device_->CreatePixelShader(fps->GetBufferPointer(),fps->GetBufferSize(),nullptr,&fxaa_pixel_),"CreatePixelShader(FXAA)");
        D3D11_SAMPLER_DESC sd{};sd.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;sd.AddressU=sd.AddressV=sd.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;sd.MaxLOD=D3D11_FLOAT32_MAX;
        check(device_->CreateSamplerState(&sd,&fxaa_sampler_),"CreateSamplerState(FXAA)");
        D3D11_RASTERIZER_DESC rd{};rd.FillMode=D3D11_FILL_SOLID;rd.CullMode=D3D11_CULL_NONE;rd.DepthClipEnable=TRUE;
        check(device_->CreateRasterizerState(&rd,&fxaa_rasterizer_),"CreateRasterizerState(FXAA)");
        D3D11_DEPTH_STENCIL_DESC dd{};check(device_->CreateDepthStencilState(&dd,&fxaa_depth_),"CreateDepthStencilState(FXAA)");
        D3D11_BLEND_DESC bd{};bd.RenderTarget[0].RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;check(device_->CreateBlendState(&bd,&fxaa_blend_),"CreateBlendState(FXAA)");
    }
    D3D11_TEXTURE2D_DESC t{};t.MipLevels=1;t.ArraySize=1;t.SampleDesc.Count=1;t.Usage=D3D11_USAGE_DEFAULT;
    const std::uint32_t black[6]{};D3D11_SUBRESOURCE_DATA init[6];for(auto& i:init){i.pSysMem=black;i.SysMemPitch=4;i.SysMemSlicePitch=4;}
    t.Width=t.Height=1;t.Format=DXGI_FORMAT_R8G8B8A8_UNORM;t.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    check(device_->CreateTexture2D(&t,init,&dummy2d_),"CreateTexture2D(dummy)");
    check(device_->CreateShaderResourceView(dummy2d_.Get(),nullptr,&dummy2d_srv_),"CreateShaderResourceView(dummy)");
    t.ArraySize=6;t.MiscFlags=D3D11_RESOURCE_MISC_TEXTURECUBE;
    check(device_->CreateTexture2D(&t,init,&dummycube_),"CreateTexture2D(dummy cube)");
    check(device_->CreateShaderResourceView(dummycube_.Get(),nullptr,&dummycube_srv_),"CreateShaderResourceView(dummy cube)");
    D3D11_QUERY_DESC q{};q.Query=D3D11_QUERY_OCCLUSION;check(device_->CreateQuery(&q,&query_),"CreateQuery");
}
D3D11D3D9Device::~D3D11D3D9Device(){if(query_live_)context_->End(query_.Get());}
void D3D11D3D9Device::allocate_back(unsigned w,unsigned h){
    if(!w||!h||w>16384||h>16384)throw std::runtime_error("invalid GPU back buffer size");
    back_srv_.Reset();back_rtv_.Reset();back_dsv_.Reset();back_colour_tex_.Reset();back_depth_tex_.Reset();
    msaa_rtv_.Reset();msaa_dsv_.Reset();msaa_colour_tex_.Reset();msaa_depth_tex_.Reset();msaa_samples_=0;
    fxaa_copy_srv_.Reset();fxaa_copy_tex_.Reset();
    D3D11_TEXTURE2D_DESC t{};t.Width=w;t.Height=h;t.MipLevels=1;t.ArraySize=1;t.Format=DXGI_FORMAT_R8G8B8A8_UNORM;t.SampleDesc.Count=1;
    t.Usage=D3D11_USAGE_DEFAULT;t.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
    check(device_->CreateTexture2D(&t,nullptr,&back_colour_tex_),"CreateTexture2D(back)");
    check(device_->CreateRenderTargetView(back_colour_tex_.Get(),nullptr,&back_rtv_),"CreateRenderTargetView(back)");
    check(device_->CreateShaderResourceView(back_colour_tex_.Get(),nullptr,&back_srv_),"CreateShaderResourceView(back)");
    t.Format=DXGI_FORMAT_D24_UNORM_S8_UINT;t.BindFlags=D3D11_BIND_DEPTH_STENCIL;
    check(device_->CreateTexture2D(&t,nullptr,&back_depth_tex_),"CreateTexture2D(depth)");
    check(device_->CreateDepthStencilView(back_depth_tex_.Get(),nullptr,&back_dsv_),"CreateDepthStencilView(back)");
    back_w_=w;back_h_=h;
}
void D3D11D3D9Device::resize_back_buffer(unsigned w,unsigned h){if(w!=back_w_||h!=back_h_)allocate_back(w,h);}
void D3D11D3D9Device::ensure_msaa(){
    if(msaa_samples_==msaa_&&msaa_rtv_)return;
    UINT quality=0;check(device_->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM,msaa_,&quality),"CheckMultisampleQualityLevels");
    if(!quality)throw std::runtime_error("GPU lacks this MSAA sample count");
    msaa_rtv_.Reset();msaa_dsv_.Reset();msaa_colour_tex_.Reset();msaa_depth_tex_.Reset();
    D3D11_TEXTURE2D_DESC t{};t.Width=back_w_;t.Height=back_h_;t.MipLevels=1;t.ArraySize=1;t.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    t.SampleDesc.Count=msaa_;t.Usage=D3D11_USAGE_DEFAULT;t.BindFlags=D3D11_BIND_RENDER_TARGET;
    check(device_->CreateTexture2D(&t,nullptr,&msaa_colour_tex_),"CreateTexture2D(MSAA)");
    check(device_->CreateRenderTargetView(msaa_colour_tex_.Get(),nullptr,&msaa_rtv_),"CreateRenderTargetView(MSAA)");
    t.Format=DXGI_FORMAT_D24_UNORM_S8_UINT;t.BindFlags=D3D11_BIND_DEPTH_STENCIL;
    check(device_->CreateTexture2D(&t,nullptr,&msaa_depth_tex_),"CreateTexture2D(MSAA depth)");
    check(device_->CreateDepthStencilView(msaa_depth_tex_.Get(),nullptr,&msaa_dsv_),"CreateDepthStencilView(MSAA)");
    msaa_samples_=msaa_;
}
void D3D11D3D9Device::finish_scene(){
    if(scene_finished_||count_only||colour_target_!=BackBuffer)return;
    if(msaa_live_){   // resolve into the back buffer, whose depth/stencil is cleared for the 2D layer
        msaa_live_=false;scene_finished_=true;
        context_->ResolveSubresource(back_colour_tex_.Get(),0,msaa_colour_tex_.Get(),0,DXGI_FORMAT_R8G8B8A8_UNORM);
        context_->ClearDepthStencilView(back_dsv_.Get(),D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL,1.f,0);
        ++msaa_resolves;return;
    }
    if(!fxaa_)return;
    scene_finished_=true;
    if(!fxaa_copy_tex_){
        D3D11_TEXTURE2D_DESC t{};back_colour_tex_->GetDesc(&t);t.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        check(device_->CreateTexture2D(&t,nullptr,&fxaa_copy_tex_),"CreateTexture2D(FXAA)");
        check(device_->CreateShaderResourceView(fxaa_copy_tex_.Get(),nullptr,&fxaa_copy_srv_),"CreateShaderResourceView(FXAA)");
    }
    ID3D11ShaderResourceView* none[12]{};context_->PSSetShaderResources(0,12,none);
    context_->CopyResource(fxaa_copy_tex_.Get(),back_colour_tex_.Get());
    ID3D11RenderTargetView* rtv=back_rtv_.Get();context_->OMSetRenderTargets(1,&rtv,nullptr);
    const D3D11_VIEWPORT vp{0,0,float(back_w_),float(back_h_),0,1};context_->RSSetViewports(1,&vp);
    context_->RSSetState(fxaa_rasterizer_.Get());context_->OMSetDepthStencilState(fxaa_depth_.Get(),0);context_->OMSetBlendState(fxaa_blend_.Get(),nullptr,0xffffffffu);
    context_->IASetInputLayout(nullptr);context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context_->VSSetShader(fxaa_vertex_.Get(),nullptr,0);context_->PSSetShader(fxaa_pixel_.Get(),nullptr,0);
    ID3D11ShaderResourceView* srv=fxaa_copy_srv_.Get();context_->PSSetShaderResources(0,1,&srv);
    ID3D11SamplerState* sampler=fxaa_sampler_.Get();context_->PSSetSamplers(0,1,&sampler);
    context_->Draw(3,0);
    ++fxaa_passes;
}
void D3D11D3D9Device::begin_frame(){
    scene_finished_=false;msaa_live_=msaa_>1&&!count_only;
    if(msaa_live_)ensure_msaa();
    if(!count_only){context_->Begin(query_.Get());query_live_=true;}
}
void D3D11D3D9Device::end_frame(){
    if(msaa_live_){const auto target=colour_target_;colour_target_=BackBuffer;finish_scene();colour_target_=target;}   // no 2D layer this frame
    if(!query_live_)return;context_->End(query_.Get());query_live_=false;
    UINT64 passed=0;while(context_->GetData(query_.Get(),&passed,sizeof passed,0)==S_FALSE){}
    pixels_shaded+=passed;
    // Development: OR2_XBOX_DRAW_STATS=1 logs the draws of every 60th frame.
    static const bool stats=std::getenv("OR2_XBOX_DRAW_STATS")!=nullptr;static unsigned frame=0;
    if(stats&&(frame++%60u)==0u)std::fprintf(stdout,"D3D11 frame %u: draws=%u vertices=%u pixels=%llu viewport=%.0fx%.0f\n",
        frame,frame_draws_,frame_vertices_,(unsigned long long)passed,soft_viewport_.w,soft_viewport_.h);
    frame_draws_=frame_vertices_=0;
}
D3D11D3D9Device::Texture* D3D11D3D9Device::texture(std::uint32_t handle){
    auto* o=object(handle);if(!o||o->kind!=KTexture)return nullptr;
    if(auto it=textures_.find(handle);it!=textures_.end())return &it->second;
    Texture t{};t.cube=o->texture.cube;t.levels=std::max(1u,o->texture.levels);
    const auto& top=o->texture.faces[0][0];
    D3D11_TEXTURE2D_DESC d{};d.Width=top.width;d.Height=top.height;d.MipLevels=t.levels;d.ArraySize=t.cube?6:1;d.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    d.SampleDesc.Count=1;d.Usage=D3D11_USAGE_DEFAULT;d.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET;d.MiscFlags=t.cube?D3D11_RESOURCE_MISC_TEXTURECUBE:0;
    check(device_->CreateTexture2D(&d,nullptr,&t.texture),"CreateTexture2D");
    for(unsigned face=0;face<o->rgba.size();++face)for(unsigned level=0;level<o->rgba[face].size()&&level<t.levels;++level){
        const auto& rgba=o->rgba[face][level];if(rgba.empty())continue;
        context_->UpdateSubresource(t.texture.Get(),D3D11CalcSubresource(level,face,t.levels),nullptr,rgba.data(),o->texture.faces[face][level].width*4,0);
    }
    check(device_->CreateShaderResourceView(t.texture.Get(),nullptr,&t.srv),"CreateShaderResourceView");
    return &(textures_[handle]=std::move(t));
}
ID3D11RenderTargetView* D3D11D3D9Device::colour_view(){
    if(colour_target_==BackBuffer)return msaa_live_?msaa_rtv_.Get():back_rtv_.Get();   // the multisampled scene (port enhancement)
    auto* s=object(colour_target_);if(!s||!s->parent)throw std::runtime_error("invalid GPU colour surface");
    auto* t=texture(s->parent);if(!t)throw std::runtime_error("GPU surface has no parent");
    const std::uint64_t k=std::uint64_t(s->parent)<<32|std::uint64_t(s->face)<<16|s->level;
    if(auto it=colour_views_.find(k);it!=colour_views_.end())return it->second.Get();
    D3D11_RENDER_TARGET_VIEW_DESC d{};d.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    if(t->cube){d.ViewDimension=D3D11_RTV_DIMENSION_TEXTURE2DARRAY;d.Texture2DArray.MipSlice=s->level;d.Texture2DArray.FirstArraySlice=s->face;d.Texture2DArray.ArraySize=1;}
    else{d.ViewDimension=D3D11_RTV_DIMENSION_TEXTURE2D;d.Texture2D.MipSlice=s->level;}
    ComPtr<ID3D11RenderTargetView> view;check(device_->CreateRenderTargetView(t->texture.Get(),&d,&view),"CreateRenderTargetView");
    return (colour_views_[k]=view).Get();
}
ID3D11DepthStencilView* D3D11D3D9Device::depth_view(std::uint32_t handle){
    if(handle==DepthBuffer)return colour_target_==BackBuffer&&msaa_live_?msaa_dsv_.Get():back_dsv_.Get();if(!handle)return nullptr;
    if(auto it=depth_views_.find(handle);it!=depth_views_.end())return it->second.Get();
    auto* o=object(handle);if(!o||o->kind!=KSurface||o->parent)throw std::runtime_error("invalid GPU depth surface");
    D3D11_TEXTURE2D_DESC d{};d.Width=o->width;d.Height=o->height;d.MipLevels=1;d.ArraySize=1;d.Format=DXGI_FORMAT_D24_UNORM_S8_UINT;
    d.SampleDesc.Count=1;d.Usage=D3D11_USAGE_DEFAULT;d.BindFlags=D3D11_BIND_DEPTH_STENCIL;
    ComPtr<ID3D11Texture2D> tex;check(device_->CreateTexture2D(&d,nullptr,&tex),"CreateTexture2D(depth surface)");
    ComPtr<ID3D11DepthStencilView> view;check(device_->CreateDepthStencilView(tex.Get(),nullptr,&view),"CreateDepthStencilView");
    return (depth_views_[handle]=view).Get();
}
void D3D11D3D9Device::bind_target(){
    // A texture bound for sampling cannot be the target at the same time.
    ID3D11ShaderResourceView* none[12]{};context_->PSSetShaderResources(0,12,none);
    if(colour_target_==BackBuffer&&msaa_live_&&depth_target_&&depth_target_!=DepthBuffer)throw std::runtime_error("MSAA scene with a separate depth surface");
    ID3D11RenderTargetView* rtv=colour_view();context_->OMSetRenderTargets(1,&rtv,depth_view(depth_target_));
}
void D3D11D3D9Device::release(std::uint32_t handle){
    PcSoftD3D9Device::release(handle);if(object(handle))return;
    textures_.erase(handle);depth_views_.erase(handle);shader_programs_.erase(handle);
    for(auto it=colour_views_.begin();it!=colour_views_.end();)it=std::uint32_t(it->first>>32)==handle?colour_views_.erase(it):std::next(it);
}
void D3D11D3D9Device::unlock_rect(std::uint32_t handle){
    auto* s=object(handle);const auto parent=s?s->parent:0;PcSoftD3D9Device::unlock_rect(handle);
    if(!parent||!textures_.count(parent))return;
    s=object(handle);auto* p=object(parent);auto& t=textures_.at(parent);
    context_->UpdateSubresource(t.texture.Get(),D3D11CalcSubresource(s->level,s->face,t.levels),nullptr,p->rgba[s->face][s->level].data(),s->width*4,0);
}
void D3D11D3D9Device::clear(std::uint32_t flags,std::uint32_t c,float z,std::uint32_t stencil){
    if(count_only)return;bind_target();
    if(flags&1){float rgba[4];colour(c,rgba);context_->ClearRenderTargetView(colour_view(),rgba);}
    if(auto* dsv=depth_view(depth_target_);dsv&&(flags&6))
        context_->ClearDepthStencilView(dsv,(flags&2?D3D11_CLEAR_DEPTH:0)|(flags&4?D3D11_CLEAR_STENCIL:0),z,UINT8(stencil));
}
void D3D11D3D9Device::raster(const Vertex* vertices,unsigned count){
    if(count<3)return;
    batch_rhw_=pretransformed_;
    auto push=[&](const Vertex& source){auto v=source;
        if(pretransformed_){const float w=1.f/v.pos[3];v.pos[0]=(2*(v.pos[0]+.5f-soft_viewport_.x)/soft_viewport_.w-1)*w;
            v.pos[1]=(1-2*(v.pos[1]+.5f-soft_viewport_.y)/soft_viewport_.h)*w;v.pos[2]=v.pos[2]*w;v.pos[3]=w;}
        else{v.pos[0]+=v.pos[3]/soft_viewport_.w;v.pos[1]-=v.pos[3]/soft_viewport_.h;}
        batch_.push_back(v);};
    for(unsigned k=1;k+1<count;++k){push(vertices[0]);push(vertices[k]);push(vertices[k+1]);++triangles_drawn;}
}
std::uint32_t D3D11D3D9Device::draw_indexed_primitive(std::uint32_t type,std::int32_t base,std::uint32_t min,std::uint32_t vertices,std::uint32_t start,std::uint32_t count){
    batch_.clear();const auto result=PcSoftD3D9Device::draw_indexed_primitive(type,base,min,vertices,start,count);flush();return result;
}
void D3D11D3D9Device::draw_primitive_up(std::uint32_t type,std::uint32_t count,const void* data,std::uint32_t stride){
    batch_.clear();PcSoftD3D9Device::draw_primitive_up(type,count,data,stride);flush();
}
void D3D11D3D9Device::uniform(unsigned slot,const void* data,std::size_t bytes){
    D3D11_MAPPED_SUBRESOURCE m{};check(context_->Map(ubo_[slot].Get(),0,D3D11_MAP_WRITE_DISCARD,0,&m),"Map(constants)");
    std::memcpy(m.pData,data,bytes);context_->Unmap(ubo_[slot].Get(),0);
}
void D3D11D3D9Device::apply_state(){
    // The logical PC target stretched to the back buffer's pixels; with the UI
    // rectangle, pretransformed (2D) draws keep a centred 4:3 area.
    const bool back=colour_target_==BackBuffer;
    double sx=back?double(back_w_)/back_width_:1,sy=back?double(back_h_)/back_height_:1,ox=0;
    if(back&&ui_rect_&&batch_rhw_){const double w=double(back_h_)*4/3;ox=std::floor((back_w_-w)/2);sx=w/back_width_;}
    const float vx=float(ox+soft_viewport_.x*sx),vy=float(soft_viewport_.y*sy),vw=float(soft_viewport_.w*sx),vh=float(soft_viewport_.h*sy);
    D3D11_VIEWPORT vp{vx,vy,vw,vh,soft_viewport_.min_z,soft_viewport_.max_z};context_->RSSetViewports(1,&vp);
    const D3D11_RECT scissor{LONG(std::lround(vx)),LONG(std::lround(vy)),LONG(std::lround(vx+vw)),LONG(std::lround(vy+vh))};context_->RSSetScissorRects(1,&scissor);
    D3D11_RASTERIZER_DESC r{};r.FillMode=D3D11_FILL_SOLID;r.FrontCounterClockwise=FALSE;r.DepthClipEnable=TRUE;r.ScissorEnable=TRUE;
    r.MultisampleEnable=back&&msaa_live_?TRUE:FALSE;
    r.CullMode=render[RS_CULLMODE]==CULL_NONE?D3D11_CULL_NONE:render[RS_CULLMODE]==CULL_CCW?D3D11_CULL_BACK:D3D11_CULL_FRONT;
    const float bias=f(render[RS_DEPTHBIAS]),slope=f(render[RS_SLOPESCALEDEPTHBIAS]);r.DepthBias=INT(bias*16777215.f);r.SlopeScaledDepthBias=slope;
    auto& rs=rasterizer_states_[key(r)];if(!rs)check(device_->CreateRasterizerState(&r,&rs),"CreateRasterizerState");context_->RSSetState(rs.Get());
    D3D11_DEPTH_STENCIL_DESC d{};d.DepthEnable=render[RS_ZENABLE]!=0;d.DepthWriteMask=render[RS_ZWRITEENABLE]?D3D11_DEPTH_WRITE_MASK_ALL:D3D11_DEPTH_WRITE_MASK_ZERO;
    d.DepthFunc=comparison(render[RS_ZFUNC]);d.StencilEnable=render[52]!=0;d.StencilReadMask=UINT8(render[58]);d.StencilWriteMask=UINT8(render[59]);
    d.FrontFace={stencil_op(render[53]),stencil_op(render[54]),stencil_op(render[55]),comparison(render[56])};d.BackFace=d.FrontFace;
    auto& ds=depth_states_[key(d)];if(!ds)check(device_->CreateDepthStencilState(&d,&ds),"CreateDepthStencilState");context_->OMSetDepthStencilState(ds.Get(),render[57]&255);
    D3D11_BLEND_DESC b{};auto& t=b.RenderTarget[0];const bool separate=render[RS_SEPARATEALPHABLENDENABLE]!=0;
    t.BlendEnable=render[RS_ALPHABLENDENABLE]!=0;t.SrcBlend=blend_factor(render[RS_SRCBLEND]);t.DestBlend=blend_factor(render[RS_DESTBLEND]);t.BlendOp=blend_op(render[RS_BLENDOP]);
    t.SrcBlendAlpha=alpha_factor(render[separate?RS_SRCBLENDALPHA:RS_SRCBLEND]);t.DestBlendAlpha=alpha_factor(render[separate?RS_DESTBLENDALPHA:RS_DESTBLEND]);
    t.BlendOpAlpha=blend_op(render[separate?RS_BLENDOPALPHA:RS_BLENDOP]);t.RenderTargetWriteMask=UINT8(render[RS_COLORWRITEENABLE]&15);
    auto& bs=blend_states_[key(b)];if(!bs)check(device_->CreateBlendState(&b,&bs),"CreateBlendState");context_->OMSetBlendState(bs.Get(),nullptr,0xffffffffu);
}
void D3D11D3D9Device::bind_textures(){
    ID3D11ShaderResourceView* views[12];ID3D11SamplerState* samplers[6];
    for(unsigned s=0;s<6;++s){auto* o=object(textures[s]);auto* t=texture(textures[s]);
        views[s]=t&&!t->cube?t->srv.Get():dummy2d_srv_.Get();views[6+s]=t&&t->cube?t->srv.Get():dummycube_srv_.Get();
        const auto& p=sampler[s];const bool linear=p[SAMP_MINFILTER]!=1,mag=p[SAMP_MAGFILTER]!=1;const auto mip=p[SAMP_MIPFILTER];
        D3D11_SAMPLER_DESC d{};
        d.Filter=D3D11_FILTER((linear?D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT:0)|(mag?D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT:0)|(mip==2?D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR:0));
        d.AddressU=address(p[SAMP_ADDRESSU]);d.AddressV=address(p[SAMP_ADDRESSV]);d.AddressW=address(p[SAMP_ADDRESSW]);
        d.MipLODBias=f(p[SAMP_MIPMAPLODBIAS]);d.ComparisonFunc=D3D11_COMPARISON_NEVER;colour(p[SAMP_BORDERCOLOR],d.BorderColor);
        d.MinLOD=o&&o->kind==KTexture?float(std::min(p[SAMP_MAXMIPLEVEL],std::max(1u,o->texture.levels)-1)):0.f;d.MaxLOD=mip==0?d.MinLOD:FLT_MAX;
        auto& ss=sampler_states_[key(d)];if(!ss)check(device_->CreateSamplerState(&d,&ss),"CreateSamplerState");samplers[s]=ss.Get();
    }
    context_->PSSetShaderResources(0,12,views);context_->PSSetSamplers(0,6,samplers);
}
ID3D11PixelShader* D3D11D3D9Device::pixel_program(unsigned kind,const platform::PcShaderProgram* program){
    auto found=shader_programs_.find(pixel_shader);if(found!=shader_programs_.end())return found->second.Get();
    // Specialize the recovered instruction sequence, leaving material,
    // constants and textures dynamic (as the PS5 GL device does).
    if(program->blobs.size()>16)throw std::runtime_error("PS program exceeds 16 blobs");
    std::string source=kind==1?hlsl::ps11:hlsl::ps14;
    const std::string declaration="int4 program[4];int4 info;";
    const auto at=source.find(declaration);if(at==std::string::npos)throw std::runtime_error("PS constant layout mismatch");
    source.replace(at,declaration.size(),"int4 unused_program[4];int4 unused_info;");
    const auto insert=source.find('\n',source.find("cbuffer PsProgram"))+1;
    std::string fixed="static const int4 program[4]={";
    for(unsigned row=0;row<4;++row){if(row)fixed+=",";fixed+="int4(";
        for(unsigned col=0;col<4;++col){if(col)fixed+=",";const auto k=row*4+col;fixed+=k<program->blobs.size()?std::to_string(program->blobs[k]):"-1";}fixed+=")";}
    fixed+="};static const int4 info=int4("+std::to_string(program->blobs.size())+",0,0,0);\n";
    source.insert(insert,fixed);
    const auto code=compile(source,"ps_main","ps_4_1");ComPtr<ID3D11PixelShader> shader;
    check(device_->CreatePixelShader(code->GetBufferPointer(),code->GetBufferSize(),nullptr,&shader),"CreatePixelShader");
    return (shader_programs_[pixel_shader]=shader).Get();
}
void D3D11D3D9Device::flush(){
    if(batch_.empty()||count_only)return;bind_target();apply_state();bind_textures();
    auto* ps=object(pixel_shader);const unsigned kind=ps&&ps->kind==4?(ps->program.version==0xffff0101u?1:2):0;
    context_->PSSetShader(kind?pixel_program(kind,&ps->program):ffp_.Get(),nullptr,0);
    uniform(0,ps_constants.data(),8*4*sizeof(float));
    if(kind){struct {std::int32_t program[16],info[4],kinds[8];float bump[8][4],lum[2][4];} p{};
        std::fill(std::begin(p.program),std::end(p.program),-1);
        for(unsigned k=0;k<ps->program.blobs.size();++k)p.program[k]=int(ps->program.blobs[k]);p.info[0]=int(ps->program.blobs.size());
        for(unsigned s=0;s<8;++s){auto* t=object(textures[s]);p.kinds[s]=t&&t->kind==KTexture&&t->texture.cube?1:0;for(unsigned k=0;k<4;++k)p.bump[s][k]=f(stage[s][7+k]);}
        for(unsigned s=0;s<4;++s){p.lum[s>>1][(s&1)*2]=f(stage[s][22]);p.lum[s>>1][(s&1)*2+1]=f(stage[s][23]);}uniform(1,&p,sizeof p);
    }else{struct {std::int32_t op[4][4],arg[4][4],misc[4][4];float konst[4][4],tfactor[4];std::int32_t flags[4];} p{};
        for(unsigned s=0;s<4;++s){const auto& t=stage[s];p.op[s][0]=int(t[1]);p.op[s][1]=int(t[2]);p.op[s][2]=int(t[3]);p.op[s][3]=int(t[4]);
            p.arg[s][0]=int(t[5]);p.arg[s][1]=int(t[6]);p.arg[s][2]=int(t[26]);p.arg[s][3]=int(t[27]);auto* tx=object(textures[s]);
            p.misc[s][0]=int(t[28]);p.misc[s][1]=int(t[11]&3);p.misc[s][2]=tx&&tx->kind==KTexture&&!tx->texture.cube;p.misc[s][3]=tx&&tx->kind==KTexture&&tx->texture.cube;
            colour(t[32],p.konst[s]);}
        colour(render[RS_TEXTUREFACTOR],p.tfactor);p.flags[0]=render[RS_SPECULARENABLE]!=0;uniform(1,&p,sizeof p);}
    float fixed[12]{};colour(render[RS_FOGCOLOR],fixed);fixed[3]=f(render[RS_FOGEND]);
    if(render[RS_FOGENABLE])fixed[4]=render[RS_FOGTABLEMODE]?float(render[RS_FOGTABLEMODE]):4.f;
    fixed[5]=f(render[RS_FOGSTART]);fixed[6]=f(render[RS_FOGDENSITY]);fixed[7]=1;fixed[8]=render[RS_ALPHATESTENABLE]?1.f:0.f;fixed[9]=float(render[RS_ALPHAREF]&255);fixed[10]=float(render[RS_ALPHAFUNC]);uniform(2,fixed,sizeof fixed);
    ID3D11Buffer* constants[3]{ubo_[0].Get(),ubo_[1].Get(),ubo_[2].Get()};context_->PSSetConstantBuffers(0,3,constants);
    const std::size_t bytes=batch_.size()*sizeof(Vertex);
    if(bytes>vbo_bytes_){vbo_bytes_=std::max<std::size_t>(bytes,std::size_t(1)<<20);vbo_.Reset();
        D3D11_BUFFER_DESC d{};d.ByteWidth=UINT(vbo_bytes_);d.Usage=D3D11_USAGE_DYNAMIC;d.BindFlags=D3D11_BIND_VERTEX_BUFFER;d.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
        check(device_->CreateBuffer(&d,nullptr,&vbo_),"CreateBuffer(vertices)");}
    D3D11_MAPPED_SUBRESOURCE m{};check(context_->Map(vbo_.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&m),"Map(vertices)");std::memcpy(m.pData,batch_.data(),bytes);context_->Unmap(vbo_.Get(),0);
    const UINT stride=sizeof(Vertex),offset=0;ID3D11Buffer* vb=vbo_.Get();context_->IASetVertexBuffers(0,1,&vb,&stride,&offset);
    context_->IASetInputLayout(layout_.Get());context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);context_->VSSetShader(vertex_.Get(),nullptr,0);
    context_->Draw(UINT(batch_.size()),0);++frame_draws_;frame_vertices_+=unsigned(batch_.size());batch_.clear();batch_rhw_=false;
}
}

#include "d3d11_display.hpp"
#include "native_window.hpp"
#include "platform/pc_dds.hpp"
#include <d3dcompiler.h>
#include <dxgi1_2.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
namespace outrun::xbox_runtime {
namespace {
constexpr const char* Shader=R"HLSL(
struct VsIn{float2 p:POSITION;float4 c:COLOR0;float2 uv:TEXCOORD0;};
struct Varying{float4 p:SV_Position;float4 c:COLOR0;float2 uv:TEXCOORD0;};
Texture2D image:register(t0);SamplerState smp:register(s0);
Varying vs_main(VsIn v){Varying o;o.p=float4(v.p,0,1);o.c=v.c;o.uv=v.uv;return o;}
float4 ps_main(Varying v):SV_Target{return image.Sample(smp,v.uv)*v.c;}
)HLSL";
void check(HRESULT hr,const char* what){if(FAILED(hr))throw std::runtime_error(std::string(what)+" failed (HRESULT "+std::to_string(unsigned(hr))+")");}
// The process' device and swap chain (one window).
struct Graphics {
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
    ComPtr<IDXGISwapChain1> swap_chain;ComPtr<ID3D11RenderTargetView> target;
    unsigned width{},height{};
};
Graphics& graphics(){
    static Graphics g;
    if(g.device)return g;
    // Xbox One UWP games get feature level 10.1 under D3D11; the PC build
    // asks for the same so both run the same shaders.
    const D3D_FEATURE_LEVEL levels[]{D3D_FEATURE_LEVEL_11_0,D3D_FEATURE_LEVEL_10_1};
    UINT flags=D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    check(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,flags,levels,2,D3D11_SDK_VERSION,&g.device,nullptr,&g.context),"D3D11CreateDevice");
    ComPtr<IDXGIDevice> dxgi;check(g.device.As(&dxgi),"IDXGIDevice");
    ComPtr<IDXGIAdapter> adapter;check(dxgi->GetAdapter(&adapter),"GetAdapter");
    ComPtr<IDXGIFactory2> factory;check(adapter->GetParent(IID_PPV_ARGS(&factory)),"IDXGIFactory2");
    auto& w=native_window();
    DXGI_SWAP_CHAIN_DESC1 d{};d.Width=w.width;d.Height=w.height;d.Format=DXGI_FORMAT_R8G8B8A8_UNORM;d.SampleDesc.Count=1;
    d.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;d.BufferCount=2;d.SwapEffect=DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;d.Scaling=DXGI_SCALING_STRETCH;
    if(w.core_window)check(factory->CreateSwapChainForCoreWindow(g.device.Get(),static_cast<IUnknown*>(w.core_window),&d,nullptr,&g.swap_chain),"CreateSwapChainForCoreWindow");
    else check(factory->CreateSwapChainForHwnd(g.device.Get(),static_cast<HWND>(w.hwnd),&d,nullptr,nullptr,&g.swap_chain),"CreateSwapChainForHwnd");
    ComPtr<ID3D11Texture2D> back;check(g.swap_chain->GetBuffer(0,IID_PPV_ARGS(&back)),"GetBuffer");
    check(g.device->CreateRenderTargetView(back.Get(),nullptr,&g.target),"CreateRenderTargetView(swap chain)");
    g.width=w.width;g.height=w.height;
    std::fprintf(stdout,"Xbox display: Direct3D 11 feature level %x, swap chain %ux%u\n",unsigned(g.device->GetFeatureLevel()),g.width,g.height);
    return g;
}
ComPtr<ID3DBlob> compile(const char* entry,const char* target){
    ComPtr<ID3DBlob> code,errors;
    if(FAILED(D3DCompile(Shader,std::strlen(Shader),"display",nullptr,nullptr,entry,target,0,0,&code,&errors)))
        throw std::runtime_error(std::string("display shader: ")+(errors?static_cast<const char*>(errors->GetBufferPointer()):"unknown error"));
    return code;
}
struct QuadVertex {float p[2],c[4],uv[2];};
}
ID3D11Device* D3D11Display::device()const{return graphics().device.Get();}
ID3D11DeviceContext* D3D11Display::context()const{return graphics().context.Get();}
D3D11Display::~D3D11Display(){if(graphics().context)graphics().context->ClearState();}
bool D3D11Display::open(unsigned w,unsigned h,std::string& error){
    try{
        width_=w;height_=h;auto* dev=graphics().device.Get();
        const auto vs=compile("vs_main","vs_4_1"),ps=compile("ps_main","ps_4_1");
        check(dev->CreateVertexShader(vs->GetBufferPointer(),vs->GetBufferSize(),nullptr,&vertex_),"CreateVertexShader(display)");
        check(dev->CreatePixelShader(ps->GetBufferPointer(),ps->GetBufferSize(),nullptr,&pixel_),"CreatePixelShader(display)");
        const D3D11_INPUT_ELEMENT_DESC elements[]{
            {"POSITION",0,DXGI_FORMAT_R32G32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},
            {"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,8,D3D11_INPUT_PER_VERTEX_DATA,0},
            {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,24,D3D11_INPUT_PER_VERTEX_DATA,0}};
        check(dev->CreateInputLayout(elements,3,vs->GetBufferPointer(),vs->GetBufferSize(),&layout_),"CreateInputLayout(display)");
        D3D11_BUFFER_DESC b{};b.ByteWidth=6*sizeof(QuadVertex);b.Usage=D3D11_USAGE_DYNAMIC;b.BindFlags=D3D11_BIND_VERTEX_BUFFER;b.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
        check(dev->CreateBuffer(&b,nullptr,&vbo_),"CreateBuffer(display)");
        D3D11_BLEND_DESC blend{};auto& t=blend.RenderTarget[0];t.BlendEnable=TRUE;t.SrcBlend=D3D11_BLEND_SRC_ALPHA;t.DestBlend=D3D11_BLEND_INV_SRC_ALPHA;t.BlendOp=D3D11_BLEND_OP_ADD;
        t.SrcBlendAlpha=D3D11_BLEND_ONE;t.DestBlendAlpha=D3D11_BLEND_INV_SRC_ALPHA;t.BlendOpAlpha=D3D11_BLEND_OP_ADD;t.RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;
        check(dev->CreateBlendState(&blend,&blend_),"CreateBlendState(display)");
        t.BlendEnable=FALSE;check(dev->CreateBlendState(&blend,&opaque_),"CreateBlendState(display opaque)");
        D3D11_SAMPLER_DESC s{};s.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;s.AddressU=s.AddressV=s.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;s.MaxLOD=D3D11_FLOAT32_MAX;
        check(dev->CreateSamplerState(&s,&sampler_),"CreateSamplerState(display)");
        D3D11_RASTERIZER_DESC r{};r.FillMode=D3D11_FILL_SOLID;r.CullMode=D3D11_CULL_NONE;r.DepthClipEnable=TRUE;
        check(dev->CreateRasterizerState(&r,&raster_),"CreateRasterizerState(display)");
        D3D11_DEPTH_STENCIL_DESC ds{};check(dev->CreateDepthStencilState(&ds,&no_depth_),"CreateDepthStencilState(display)");
        return true;
    }catch(const std::exception& e){error=e.what();return false;}
}
void D3D11Display::clear(){
    auto& g=graphics();auto* c=g.context.Get();
    ID3D11RenderTargetView* target=g.target.Get();c->OMSetRenderTargets(1,&target,nullptr);
    const float black[4]{0,0,0,1};c->ClearRenderTargetView(target,black);
    const D3D11_VIEWPORT vp{0,0,float(g.width),float(g.height),0,1};c->RSSetViewports(1,&vp);
    c->RSSetState(raster_.Get());c->OMSetDepthStencilState(no_depth_.Get(),0);c->OMSetBlendState(blend_.Get(),nullptr,0xffffffffu);
    c->IASetInputLayout(layout_.Get());c->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    c->VSSetShader(vertex_.Get(),nullptr,0);c->PSSetShader(pixel_.Get(),nullptr,0);
    ID3D11SamplerState* s=sampler_.Get();c->PSSetSamplers(0,1,&s);
    const UINT stride=sizeof(QuadVertex),offset=0;ID3D11Buffer* vb=vbo_.Get();c->IASetVertexBuffers(0,1,&vb,&stride,&offset);
}
ID3D11ShaderResourceView* D3D11Display::upload(const void* key,const std::uint8_t* pixels,unsigned w,unsigned h,bool update){
    if(!pixels||!w||!h)throw std::runtime_error("invalid display texture");
    auto& i=images_[key];const bool fresh=!i.texture||i.width!=w||i.height!=h;
    if(fresh){
        D3D11_TEXTURE2D_DESC d{};d.Width=w;d.Height=h;d.MipLevels=1;d.ArraySize=1;d.Format=DXGI_FORMAT_R8G8B8A8_UNORM;d.SampleDesc.Count=1;
        d.Usage=D3D11_USAGE_DEFAULT;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        const D3D11_SUBRESOURCE_DATA init{pixels,w*4,0};
        i=Image{};check(device()->CreateTexture2D(&d,&init,&i.texture),"CreateTexture2D(display)");
        check(device()->CreateShaderResourceView(i.texture.Get(),nullptr,&i.view),"CreateShaderResourceView(display)");i.width=w;i.height=h;
    }else if(update)context()->UpdateSubresource(i.texture.Get(),0,nullptr,pixels,w*4,0);
    return i.view.Get();
}
ID3D11ShaderResourceView* D3D11Display::texels(const void* key,platform::MeshPreviewTextureFormat f,unsigned w,unsigned h,const std::vector<std::uint8_t>& bytes){
    if(auto it=images_.find(key);it!=images_.end())return it->second.view.Get();
    platform::PcDdsTexture t{};t.width=w;t.height=h;t.levels=1;
    t.format=f==platform::MeshPreviewTextureFormat::bc1?platform::PcDdsTexture::Format::bc1:
        f==platform::MeshPreviewTextureFormat::bc2?platform::PcDdsTexture::Format::bc2:
        f==platform::MeshPreviewTextureFormat::bc3?platform::PcDdsTexture::Format::bc3:platform::PcDdsTexture::Format::rgba8;
    t.faces.resize(1);t.faces[0].push_back({w,h,bytes});const auto rgba=platform::pc_dds_rgba(t,0,0);
    if(rgba.size()!=std::size_t(w)*h*4)throw std::runtime_error("display DDS size mismatch");return upload(key,rgba.data(),w,h,false);
}
void D3D11Display::quad(const std::array<platform::MeshPreviewVertex,4>& q,ID3D11ShaderResourceView* image,unsigned,unsigned){
    QuadVertex vertices[6];const unsigned order[]{0,1,2,2,3,0};
    for(unsigned k=0;k<6;++k){const auto& v=q[order[k]];vertices[k]={{v.position[0],v.position[1]},{v.color[0],v.color[1],v.color[2],v.color[3]},{v.uv[0],v.uv[1]}};}
    auto* c=context();D3D11_MAPPED_SUBRESOURCE m{};check(c->Map(vbo_.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&m),"Map(display)");
    std::memcpy(m.pData,vertices,sizeof vertices);c->Unmap(vbo_.Get(),0);
    c->PSSetShaderResources(0,1,&image);c->Draw(6,0);
}
void D3D11Display::rectangle(ID3D11ShaderResourceView* texture,int x,int y,int w,int h){
    std::array<platform::MeshPreviewVertex,4> q{};
    const float px[]{float(x),float(x+w),float(x+w),float(x)},py[]{float(y),float(y),float(y+h),float(y+h)};
    for(unsigned k=0;k<4;++k){q[k].position={2*px[k]/width_-1,1-2*py[k]/height_,0};q[k].color={1,1,1,1};q[k].uv={k==1||k==2?1.f:0.f,k>=2?1.f:0.f};}
    quad(q,texture,unsigned(w),unsigned(h));
}
void D3D11Display::rgba(const void* key,const std::uint8_t* bytes,unsigned w,unsigned h,int x,int y,int ow,int oh){rectangle(upload(key,bytes,w,h,true),x,y,ow,oh);}
void D3D11Display::pc_frame(ID3D11ShaderResourceView* texture,int x,int y,int w,int h){
    // The PC device drew into its own target: draw it opaque, then restore the display state.
    {auto& g=graphics();ID3D11RenderTargetView* target=g.target.Get();g.context->OMSetRenderTargets(1,&target,nullptr);}
    context()->OMSetBlendState(opaque_.Get(),nullptr,0xffffffffu);rectangle(texture,x,y,w,h);
    context()->OMSetBlendState(blend_.Get(),nullptr,0xffffffffu);
}
// Development: OR2_XBOX_SCREENSHOT_DIR=<folder> writes every 60th presented frame as frame-NNNN.ppm.
void screenshot(Graphics& g){
    static const char* dir=std::getenv("OR2_XBOX_SCREENSHOT_DIR");static unsigned frame=0;
    if(!dir||(frame++%60u)!=0u)return;
    ComPtr<ID3D11Texture2D> back;if(FAILED(g.swap_chain->GetBuffer(0,IID_PPV_ARGS(&back))))return;
    D3D11_TEXTURE2D_DESC d{};back->GetDesc(&d);d.Usage=D3D11_USAGE_STAGING;d.BindFlags=0;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;d.MiscFlags=0;
    ComPtr<ID3D11Texture2D> staging;if(FAILED(g.device->CreateTexture2D(&d,nullptr,&staging)))return;
    g.context->CopyResource(staging.Get(),back.Get());
    D3D11_MAPPED_SUBRESOURCE m{};if(FAILED(g.context->Map(staging.Get(),0,D3D11_MAP_READ,0,&m)))return;
    char name[64];std::snprintf(name,sizeof name,"/frame-%04u.ppm",frame/60u);
    if(std::FILE* f=std::fopen((std::string(dir)+name).c_str(),"wb")){
        std::fprintf(f,"P6\n%u %u\n255\n",d.Width,d.Height);
        for(unsigned y=0;y<d.Height;++y){const auto* row=static_cast<const std::uint8_t*>(m.pData)+std::size_t(y)*m.RowPitch;
            for(unsigned x=0;x<d.Width;++x)std::fwrite(row+x*4,3,1,f);}
        std::fclose(f);}
    g.context->Unmap(staging.Get(),0);
}
// FRAME RATE (enhancements/frame_rate): the output is put at 120 Hz once,
// when it offers it; 60 / 120 fps are then paced by the present interval,
// so a change in Settings applies at once without another mode switch.
namespace { unsigned g_refresh=0,g_interval=1; }
unsigned output_refresh(){
    if(!g_refresh){
        // Development: OR2_XBOX_REFRESH_HZ=N stands for the output's rate (120 Hz checks on a 60 Hz monitor).
        const char* forced=std::getenv("OR2_XBOX_REFRESH_HZ");
        g_refresh=forced?unsigned(std::strtoul(forced,nullptr,10)):select_refresh_rate(120u);
        if(!g_refresh)g_refresh=60u;
    }
    return g_refresh;
}
void pace_frames(unsigned fps){
    g_interval=std::max(1u,(output_refresh()+fps/2u)/std::max(1u,fps));
    std::fprintf(stdout,"Xbox display: %u fps on a %u Hz output (present interval %u)\n",fps,output_refresh(),g_interval);
}
bool D3D11Display::present(std::string& error){
    auto& g=graphics();screenshot(g);
    const HRESULT hr=g.swap_chain->Present(g_interval,0);
    if(FAILED(hr)){error="Present failed (HRESULT "+std::to_string(unsigned(hr))+")";return false;}
    if(!pump_window_events()){error="window closed";return false;}
    return true;
}
}

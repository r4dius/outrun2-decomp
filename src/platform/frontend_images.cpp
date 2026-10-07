#include "frontend_images.hpp"
#include "driving/pc_x87.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>
namespace outrun::platform {
namespace {
unsigned word(const std::uint8_t* p){return unsigned(p[0])|(unsigned(p[1])<<8)|(unsigned(p[2])<<16)|(unsigned(p[3])<<24);}
unsigned half(const std::uint8_t* p){return unsigned(p[0])|(unsigned(p[1])<<8);}
bool fail(std::string* e,const char* s){if(e)*e=s;return false;}
}
bool parse_frontend_image_bank(const std::uint8_t* p,std::size_t n,FrontendImageBank& out,std::string* e){
    if(!p||n<40||n>16*1024*1024)return fail(e,"raw image XST size");
    const auto meta=word(p),pixels=word(p+4);if(meta<32||std::uint64_t(meta)+pixels+8!=n)return fail(e,"raw image XST sections");
    const auto* h=p+8;const unsigned tex=word(h+8),count=word(h+24),offset=word(h+28);
    if(word(h)!=0||!tex||tex>256||!count||count>65536||offset<32||offset>meta||count>(meta-offset)/28)return fail(e,"raw image XST table");
    FrontendImageBank bank;std::size_t cursor=8+meta;
    for(unsigned i=0;i<tex;++i){
        if(n-cursor<128||std::memcmp(p+cursor,"DDS ",4)||word(p+cursor+4)!=124)return fail(e,"raw image DDS header");
        const auto* d=p+cursor;const unsigned width=word(d+16),height=word(d+12),fourcc=word(d+84),levels=word(d+28);
        unsigned format=fourcc==0x31545844?1:fourcc==0x33545844?2:fourcc==0x35545844?3:0;
        if(!format||!width||!height||width>4096||height>4096||levels>1)return fail(e,"raw image DDS format");
        const std::size_t bytes=std::size_t((width+3)/4)*((height+3)/4)*(format==1?8:16);
        if(bytes>n-cursor-128)return fail(e,"raw image DDS truncated");
        bank.textures.push_back({width,height,MeshPreviewTextureFormat(format),{d+128,d+128+bytes}});cursor+=128+bytes;
    }
    if(cursor!=n)return fail(e,"raw image trailing payload");
    for(unsigned i=0;i<count;++i){const auto* r=h+offset+i*28;FrontendImageRegion region;region.texture=word(r);
        for(unsigned c=0;c<4;++c)region.crop[c]=half(r+20+c*2);
        if(region.texture>=tex)return fail(e,"raw image region texture");
        const auto& t=bank.textures[region.texture];const auto& c=region.crop;
        if(c[2]<c[0]||c[3]<c[1]||c[2]>t.width||c[3]>t.height)return fail(e,"raw image region crop");
        bank.regions.push_back(region);
    }
    out=std::move(bank);return true;
}
bool load_frontend_image_bank(const char* path,FrontendImageBank& out,std::string* e){
    auto* f=std::fopen(path,"rb");if(!f)return fail(e,"raw image bank unavailable");
    if(std::fseek(f,0,SEEK_END)){std::fclose(f);return fail(e,"raw image bank seek");}const auto n=std::ftell(f);
    if(n<0||n>16*1024*1024||std::fseek(f,0,SEEK_SET)){std::fclose(f);return fail(e,"raw image bank size");}
    std::vector<std::uint8_t> data(std::size_t(n),0);const bool ok=std::fread(data.data(),1,data.size(),f)==data.size();std::fclose(f);
    return ok?parse_frontend_image_bank(data.data(),data.size(),out,e):fail(e,"raw image bank read");
}
bool frontend_image_quad(const FrontendListImage& image,const FrontendImageBank& bank,FrontendGlyphQuad& out,unsigned& texture){
    const unsigned index=image.token&0xffff;
    if(image.token>>16!=bank.bank||index>=bank.regions.size()||!std::isfinite(image.layer))return false;
    const auto& region=bank.regions[index];if(region.texture>=bank.textures.size())return false;
    texture=region.texture;const auto& c=region.crop;
    if(image.pc==0x42d280){
        FrontendGlyph g;g.token=texture;g.left=int(c[0]);g.top=int(c[1]);g.right=int(c[2]);g.bottom=int(c[3]);
        g.x=float(image.x);g.y=float(image.y);g.color=image.color;
        if(!frontend_glyph_quad(g,bank.textures[texture],out))return false;
        // The argument formerly named frame is the original horizontal /
        // vertical flip mask here, not a sprite animation frame.
        if(image.frame&1){std::swap(out[0].uv[0],out[1].uv[0]);std::swap(out[3].uv[0],out[2].uv[0]);}
        if(image.frame&2){std::swap(out[0].uv[1],out[3].uv[1]);std::swap(out[1].uv[1],out[2].uv[1]);}
        return true;
    }
    if(image.pc!=0x42d300||!std::isfinite(image.width)||!std::isfinite(image.height))return false;
    unsigned left=c[0],top=c[1],right=c[2],bottom=c[3];
    switch(image.mode){case 1:right=left;break;case 2:top=bottom;break;case 3:left+=3;right=left;top+=3;bottom=top;break;default:break;}
    // Original 42D300 constants are 1/128.5 and 1/64.5, NOT atlas reciprocal
    // dimensions. Its material UV order differs from its position order.
    // The actual 42A3A0 consumer reorders UVs and applies 0.51-texel offsets.
    float sx,sy;unsigned bx=0x3bff00ff,by=0x3c7e03f8;std::memcpy(&sx,&bx,4);std::memcpy(&sy,&by,4);
    // 42D300: fild; fmul; (fsubr 1.0); fst float. Each fmul/fsubr/fadd rounds
    // to the x87 precision control before the float store.
    using driving::X87;using driving::x87_float;
    auto fi=[](unsigned v){return X87(std::int32_t(v));};
    const float ul=x87_float(fi(right)*X87(sx)),ur=x87_float(fi(left)*X87(sx));
    const float vt=x87_float(X87(1.0f)-fi(top)*X87(sy)),vb=x87_float(X87(1.0f)-fi(bottom)*X87(sy));
    const float x=float(image.x)+.5f,y=float(image.y)+.5f,xr=x87_float(X87(float(image.x))+X87(image.width))+.5f,yb=x87_float(X87(image.y)+X87(image.height))+.5f;
    if(!std::isfinite(xr)||!std::isfinite(yb))return false;
    const auto& t=bank.textures[texture];if(!t.width||!t.height)return false;
    const float half_u=.51f/t.width,half_v=.51f/t.height;
    constexpr float hy=.9f,hx=.9f*(640.f/480.f)/(1280.f/720.f);
    for(unsigned i=0;i<4;++i){const bool r=i==1||i==2,b=i>=2;auto& v=out[i];
        v.position={{-hx+(r?xr:x)*(2*hx/640),hy-(b?yb:y)*(2*hy/480),0}};
        v.normal={{.401422f,.752666f,.521849f}};v.uv={{r?ul-half_u:ur+half_u,b?vb+half_v:vt-half_v}};
        v.color={{float((image.color>>16)&255)/255,float((image.color>>8)&255)/255,float(image.color&255)/255,float(image.color>>24)/255}};v.transform_index=8;
    }return true;
}
}

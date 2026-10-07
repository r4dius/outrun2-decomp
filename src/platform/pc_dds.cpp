#include "platform/pc_dds.hpp"
#include <algorithm>
#include <cstring>
namespace outrun::platform {
namespace {
std::uint32_t le32(const std::uint8_t* p){return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|(std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);}
std::uint32_t level_bytes(PcDdsTexture::Format f,std::uint32_t w,std::uint32_t h){
    const std::uint32_t bw=std::max(1u,(w+3)/4),bh=std::max(1u,(h+3)/4);
    switch(f){case PcDdsTexture::Format::bc1:return bw*bh*8;case PcDdsTexture::Format::bc2:case PcDdsTexture::Format::bc3:return bw*bh*16;
        default:return w*h*4;}
}
void rgb565(std::uint16_t c,std::uint8_t* o){
    const unsigned r=(c>>11)&31,g=(c>>5)&63,b=c&31;o[0]=std::uint8_t((r<<3)|(r>>2));o[1]=std::uint8_t((g<<2)|(g>>4));o[2]=std::uint8_t((b<<3)|(b>>2));}
// Colour block of DXT1 (with 1-bit alpha when c0<=c1) / DXT3/5 (always 4 colours).
void colour_block(const std::uint8_t* b,bool dxt1,std::uint8_t out[16][4]){
    const std::uint16_t c0=std::uint16_t(b[0]|(b[1]<<8)),c1=std::uint16_t(b[2]|(b[3]<<8));
    std::uint8_t p[4][4]{};rgb565(c0,p[0]);rgb565(c1,p[1]);p[0][3]=p[1][3]=255;
    if(!dxt1||c0>c1){
        for(unsigned k=0;k<3;++k){p[2][k]=std::uint8_t((2*p[0][k]+p[1][k])/3);p[3][k]=std::uint8_t((p[0][k]+2*p[1][k])/3);}
        p[2][3]=p[3][3]=255;
    }else{
        for(unsigned k=0;k<3;++k)p[2][k]=std::uint8_t((p[0][k]+p[1][k])/2);
        p[2][3]=255;p[3][0]=p[3][1]=p[3][2]=0;p[3][3]=0;
    }
    const std::uint32_t bits=le32(b+4);
    for(unsigned i=0;i<16;++i)std::memcpy(out[i],p[(bits>>(2*i))&3u],4);
}
}
std::vector<std::uint8_t> pc_dds_rgba(const PcDdsTexture& t,unsigned face,unsigned level){
    const auto& l=t.faces.at(face).at(level);
    if(t.format==PcDdsTexture::Format::rgba8)return l.data;
    std::vector<std::uint8_t> out(std::size_t(l.width)*l.height*4);
    const std::uint32_t bw=std::max(1u,(l.width+3)/4),bh=std::max(1u,(l.height+3)/4);
    const unsigned stride=t.format==PcDdsTexture::Format::bc1?8u:16u;
    for(std::uint32_t by=0;by<bh;++by)for(std::uint32_t bx=0;bx<bw;++bx){
        const std::uint8_t* b=l.data.data()+(by*bw+bx)*stride;
        std::uint8_t px[16][4];
        if(t.format==PcDdsTexture::Format::bc1)colour_block(b,true,px);
        else{
            colour_block(b+8,false,px);
            if(t.format==PcDdsTexture::Format::bc2){
                for(unsigned i=0;i<16;++i){const unsigned a=(b[i/2]>>((i&1)*4))&15u;px[i][3]=std::uint8_t(a*17);}
            }else{
                const unsigned a0=b[0],a1=b[1];unsigned a[8]{a0,a1};
                if(a0>a1)for(unsigned k=1;k<7;++k)a[k+1]=((7-k)*a0+k*a1)/7;
                else{for(unsigned k=1;k<5;++k)a[k+1]=((5-k)*a0+k*a1)/5;a[6]=0;a[7]=255;}
                std::uint64_t bits=0;for(unsigned k=0;k<6;++k)bits|=std::uint64_t(b[2+k])<<(8*k);
                for(unsigned i=0;i<16;++i)px[i][3]=std::uint8_t(a[(bits>>(3*i))&7u]);
            }
        }
        for(unsigned i=0;i<16;++i){const auto x=bx*4+(i&3),y=by*4+(i>>2);
            if(x<l.width&&y<l.height)std::memcpy(out.data()+(std::size_t(y)*l.width+x)*4,px[i],4);}
    }
    return out;
}
bool pc_d3dx_texture_from_file(const PcTextureFileRequest& q,PcDdsTexture& t,std::string& error){
    t=PcDdsTexture{};
    if(!q.data||q.size<0x80u||std::memcmp(q.data,"DDS ",4)!=0){error="not a DDS file";return false;}
    const std::uint8_t* h=q.data+4;
    const std::uint32_t height=le32(h+8),width=le32(h+12),file_levels=std::max(1u,le32(h+24));
    const std::uint32_t pf_flags=le32(h+76),fourcc=le32(h+80),bits=le32(h+84);
    const std::uint32_t rmask=le32(h+88),gmask=le32(h+92),bmask=le32(h+96),amask=le32(h+100);
    const std::uint32_t caps2=le32(h+108);
    if(pf_flags&4u){
        if(fourcc==0x31545844u)t.format=PcDdsTexture::Format::bc1;
        else if(fourcc==0x33545844u)t.format=PcDdsTexture::Format::bc2;
        else if(fourcc==0x35545844u)t.format=PcDdsTexture::Format::bc3;
        else{error="unsupported DDS FourCC";return false;}
    }else if(bits==32&&rmask==0xff0000u&&gmask==0xff00u&&bmask==0xffu)t.format=PcDdsTexture::Format::rgba8;
    else{error="unsupported DDS pixel format";return false;}
    const bool has_alpha=(pf_flags&1u)!=0&&amask==0xff000000u;
    t.cube=q.cube;
    if(t.cube!=(caps2!=0u)){error="DDS cube flag differs from the request";return false;}
    t.width=width;t.height=height;
    std::uint32_t chain=1;{std::uint32_t w=width,hh=height;while(w>1||hh>1){w=std::max(1u,w/2);hh=std::max(1u,hh/2);++chain;}}
    std::uint32_t wanted=(q.mip_levels==0u||q.mip_levels==0xffffffffu)?chain:std::min(q.mip_levels,chain);
    t.levels=wanted;
    const unsigned faces=t.cube?6u:1u;
    std::size_t at=0x80;
    t.faces.resize(faces);
    for(unsigned f=0;f<faces;++f){
        std::uint32_t w=width,hh=height;
        for(std::uint32_t l=0;l<file_levels;++l){
            const auto n=level_bytes(t.format,w,hh);
            if(at+n>q.size){error="DDS file shorter than its levels";return false;}
            if(l<wanted){
                PcDdsTexture::Level lv{w,hh,std::vector<std::uint8_t>(q.data+at,q.data+at+n)};
                if(t.format==PcDdsTexture::Format::rgba8)for(std::size_t k=0;k<lv.data.size();k+=4){ // B,G,R,A -> R,G,B,A
                    std::swap(lv.data[k],lv.data[k+2]);if(!has_alpha)lv.data[k+3]=255;}
                t.faces[f].push_back(std::move(lv));
            }
            at+=n;w=std::max(1u,w/2);hh=std::max(1u,hh/2);
        }
    }
    if(file_levels<wanted){
        // Generate the missing levels (D3DX mip filter): decode to RGBA8 first.
        if(t.format!=PcDdsTexture::Format::rgba8){
            for(unsigned f=0;f<faces;++f)for(auto& lv:t.faces[f]){PcDdsTexture tmp=t;lv.data=pc_dds_rgba(tmp,f,unsigned(&lv-t.faces[f].data()));}
            t.format=PcDdsTexture::Format::rgba8;
        }
        const auto filter=q.mip_filter&0xffu;
        for(unsigned f=0;f<faces;++f)while(t.faces[f].size()<wanted){
            const auto& src=t.faces[f].back();
            PcDdsTexture::Level lv{std::max(1u,src.width/2),std::max(1u,src.height/2),{}};
            lv.data.resize(std::size_t(lv.width)*lv.height*4);
            for(std::uint32_t y=0;y<lv.height;++y)for(std::uint32_t x=0;x<lv.width;++x)for(unsigned c=0;c<4;++c){
                unsigned v;
                if(filter==1u)v=src.data[(std::size_t(y)*src.width+x)*4+c]; // NONE: no scaling, top-left part
                else if(filter==2u)v=src.data[(std::size_t(std::min(y*2,src.height-1))*src.width+std::min(x*2,src.width-1))*4+c];
                else{unsigned s=0;for(unsigned dy=0;dy<2;++dy)for(unsigned dx=0;dx<2;++dx)
                        s+=src.data[(std::size_t(std::min(y*2+dy,src.height-1))*src.width+std::min(x*2+dx,src.width-1))*4+c];v=(s+2)/4;}
                lv.data[(std::size_t(y)*lv.width+x)*4+c]=std::uint8_t(v);
            }
            t.faces[f].push_back(std::move(lv));
        }
    }
    return true;
}
}

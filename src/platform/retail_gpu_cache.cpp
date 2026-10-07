#include "platform/retail_gpu_cache.hpp"
#include "pc_pmt.hpp"
#include "platform/mesh_preview_pack.hpp"
#include "platform/vehicle_visual.hpp"
#include "platform/frontend_text.hpp"
#include "platform/frontend_images.hpp"
#include "platform/embedded_exe_data.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <functional>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <zlib.h>

namespace outrun::platform {
namespace {
constexpr std::size_t MaxCacheBytes=128u*1024u*1024u;
constexpr std::uint32_t FrontendTextures=129u,FrontendScenes=224u;
constexpr std::size_t FepHeader=224u,FepTexRec=32u,FepSceneRec=24u,FepDrawRec=64u;

void fail(std::string* error,const std::string& text){if(error)*error=text;}
std::uint32_t rd32(const std::uint8_t* p){return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8u)|(std::uint32_t(p[2])<<16u)|(std::uint32_t(p[3])<<24u);}
float rdf(const std::uint8_t* p){auto bits=rd32(p);float v{};std::memcpy(&v,&bits,4);return v;}
void put32(std::vector<std::uint8_t>& out,std::size_t off,std::uint32_t v){if(off+4>out.size())out.resize(off+4);for(unsigned i=0;i<4;++i)out[off+i]=std::uint8_t(v>>(8u*i));}
void puti32(std::vector<std::uint8_t>& out,std::size_t off,std::int32_t v){put32(out,off,static_cast<std::uint32_t>(v));}
void putf(std::vector<std::uint8_t>& out,std::size_t off,float v){std::uint32_t bits{};std::memcpy(&bits,&v,4);put32(out,off,bits);}
std::uint32_t crc(const std::uint8_t* p,std::size_t n){return static_cast<std::uint32_t>(::crc32(0L,reinterpret_cast<const Bytef*>(p),static_cast<uInt>(n)));}
std::uint32_t crc(const std::vector<std::uint8_t>& v){return crc(v.data(),v.size());}

std::uint32_t dds_format(std::uint32_t fourcc){if(fourcc==0x31545844u)return 1u;if(fourcc==0x33545844u)return 2u;if(fourcc==0x35545844u)return 3u;return 0u;}
std::size_t bc_level_bytes(std::uint32_t w,std::uint32_t h,std::uint32_t fmt){return std::size_t(std::max(1u,(w+3u)/4u))*std::max(1u,(h+3u)/4u)*(fmt==1u?8u:16u);}

struct DdsEntry{std::uint32_t source{},width{},height{},format{};std::vector<std::uint8_t> payload;};
bool scan_dds(const std::vector<std::uint8_t>& raw,std::uint32_t expected,std::vector<DdsEntry>& out,std::string* error){
    std::vector<std::size_t> starts;for(std::size_t i=0;i+4<=raw.size();++i)if(std::memcmp(raw.data()+i,"DDS ",4)==0){starts.push_back(i);i+=3;}if(starts.size()!=expected){fail(error,"unexpected SprAni DDS count");return false;}out.clear();
    for(std::size_t n=0;n<starts.size();++n){const auto s=starts[n];if(s+128>raw.size()||rd32(raw.data()+s+4)!=124u){fail(error,"invalid SprAni DDS header");return false;}const auto h=rd32(raw.data()+s+12),w=rd32(raw.data()+s+16),fmt=dds_format(rd32(raw.data()+s+84));if(!fmt){fail(error,"unsupported SprAni DDS format");return false;}const auto count=bc_level_bytes(w,h,fmt),end=s+128u+count,next=n+1<starts.size()?starts[n+1]:raw.size();if(end!=next){fail(error,"SprAni DDS pages are not contiguous top-level images");return false;}DdsEntry e{};e.source=static_cast<std::uint32_t>(n);e.width=w;e.height=h;e.format=fmt;e.payload.assign(raw.data()+s+128,raw.data()+end);out.push_back(std::move(e));}return true;
}

bool build_fonts(RetailAssetStore& store,std::vector<std::uint8_t>& out,std::string* error){
    std::vector<std::uint8_t> raw,metrics;
    if(!retail_asset_read_relative_inflated(store,"Sprite/spr_font_xst.sz",raw,MaxCacheBytes,error))return false;
    std::vector<DdsEntry> atlas;if(!scan_dds(raw,10,atlas,error))return false;
    std::uint32_t dims[10][3];for(unsigned i=0;i<10;++i){dims[i][0]=atlas[i].width;dims[i][1]=atlas[i].height;dims[i][2]=atlas[i].format;}
    if(!build_font_metrics(dims,metrics,error))return false;
    out.assign(512,0);std::memcpy(out.data(),"OR2FNT1\0",8);std::memcpy(out.data()+32,metrics.data(),480);
    for(unsigned i=0;i<10;++i){const auto r=32+i*48;const auto token=rd32(out.data()+r);
        const auto count=rd32(out.data()+r+16),kern=rd32(out.data()+r+32),offset=rd32(out.data()+r+28);
        const auto length=std::size_t(count)*2+kern;
        if(token>=10||offset>metrics.size()-480||length>metrics.size()-480-offset){fail(error,"font metrics bounds");return false;}
        const auto& t=atlas[token];
        if(t.width!=rd32(out.data()+r+36)||t.height!=rd32(out.data()+r+40)||t.format!=rd32(out.data()+r+44)){
            fail(error,"retail FONT atlas differs from PC font descriptor");return false;
        }
        put32(out,r+28,unsigned(out.size()-512));
        out.insert(out.end(),metrics.begin()+480+offset,metrics.begin()+480+offset+length);
        out.insert(out.end(),t.payload.begin(),t.payload.end());
    }
    put32(out,8,1);put32(out,12,10);put32(out,16,48);put32(out,20,512);put32(out,24,unsigned(out.size()));
    put32(out,28,crc(out.data()+32,out.size()-32));
    FrontendFontPack check;return parse_frontend_font_pack(out.data(),out.size(),check,error);
}

struct Affine{float a=1,b=0,c=0,d=1,x=0,y=0;};
Affine compose(const Affine&p,const Affine&l){return {p.a*l.a+p.c*l.b,p.b*l.a+p.d*l.b,p.a*l.c+p.c*l.d,p.b*l.c+p.d*l.d,p.a*l.x+p.c*l.y+p.x,p.b*l.x+p.d*l.y+p.y};}
struct FDraw{std::uint32_t source{};std::array<std::uint32_t,4> crop{};std::array<float,8> corners{};};struct FScene{std::uint32_t token{},width{},height{};std::vector<FDraw> draws;};

bool decode_frontend_scenes(const std::vector<std::uint8_t>& ani,const std::vector<std::uint32_t>& tokens,bool first_frame,std::vector<FScene>& scenes,std::uint32_t& source_scenes,std::string* error){
    if(ani.size()<8||rd32(ani.data())+4u!=ani.size()){fail(error,"unexpected SprAni animation wrapper");return false;}const auto* d=ani.data()+4;const std::size_t n=ani.size()-4;const auto table=rd32(d);if(table<8||table%4||table>n){fail(error,"invalid SprAni scene pointer table");return false;}source_scenes=table/4u-1u;if(rd32(d+table-4)!=0u){fail(error,"SprAni scene terminator mismatch");return false;}scenes.clear();
    for(auto token:tokens){const auto si=token&0xffffu;if(si>=source_scenes){fail(error,"SprAni scene token out of range");return false;}const auto sp=rd32(d+si*4u);if(sp+20>n){fail(error,"SprAni scene pointer out of range");return false;}const auto cc=rd32(d+sp),cp=rd32(d+sp+4),fc=rd32(d+sp+8),fp=rd32(d+sp+12),packed=rd32(d+sp+16);FScene scene{};scene.token=token;scene.width=packed&0xffffu;scene.height=packed>>16u;if(!cc||!scene.width||scene.width>4096u||!scene.height||scene.height>4096u||std::size_t(cp)+std::size_t(cc)*36u>n||std::size_t(fp)+std::size_t(fc)*24u>n){fail(error,"invalid SprAni scene");return false;}
        std::function<bool(std::uint32_t,const Affine&,unsigned)> visit=[&](std::uint32_t off,const Affine& parent,unsigned depth){if(depth>64u||off<cp||off>=cp+cc*36u||(off-cp)%36u||off+36>n){fail(error,"SprAni component pointer invalid");return false;}const auto lc=rd32(d+off+28),lp=rd32(d+off+32);if(std::size_t(lp)+std::size_t(lc)*76u>n){fail(error,"SprAni layer table invalid");return false;}for(std::uint32_t rem=lc;rem;--rem){const auto layer=lp+(rem-1u)*76u;const auto source=rd32(d+layer+8);float ax=rdf(d+layer+16),ay=rdf(d+layer+20),px=rdf(d+layer+24),py=rdf(d+layer+28),sx=rdf(d+layer+32)*.01f,sy=rdf(d+layer+36)*.01f,rad=rdf(d+layer+40)*0.01745329251994329577f;const float a=std::cos(rad)*sx,b=std::sin(rad)*sx,c=-std::sin(rad)*sy,dd=std::cos(rad)*sy;const auto matrix=compose(parent,{a,b,c,dd,px-a*ax-c*ay,py-b*ax-dd*ay});if(source>=cp&&source<cp+cc*36u&&(source-cp)%36u==0u){if(!visit(source,matrix,depth+1))return false;continue;}if(source<fp||source>=fp+fc*24u||(source-fp)%24u||source+24>n){fail(error,"SprAni layer source invalid");return false;}const auto fw=rd32(d+source),fh=rd32(d+source+4),frames=rd32(d+source+12),fr=rd32(d+source+16);if(frames!=1u&&!first_frame){fail(error,"unexpected animated footage in static menu");return false;}if(frames==0u){if(first_frame)continue;fail(error,"SprAni footage has no frame");return false;}if(fr+20>n){fail(error,"SprAni frame pointer invalid");return false;}FDraw draw{};draw.source=rd32(d+fr);for(unsigned i=0;i<4;++i){const float v=rdf(d+fr+4+i*4);if(!std::isfinite(v)||v<0||v>4096){fail(error,"SprAni crop invalid");return false;}draw.crop[i]=static_cast<std::uint32_t>(std::lround(v));}const float xy[4][2]={{0,0},{float(fw),0},{float(fw),float(fh)},{0,float(fh)}};for(unsigned i=0;i<4;++i){draw.corners[i*2]=matrix.a*xy[i][0]+matrix.c*xy[i][1]+matrix.x;draw.corners[i*2+1]=matrix.b*xy[i][0]+matrix.d*xy[i][1]+matrix.y;}scene.draws.push_back(draw);}return true;};
        if(!visit(cp+(cc-1u)*36u,{},0u))return false;scenes.push_back(std::move(scene));
    }return true;
}

bool build_frontend(RetailAssetStore& store,bool loading,std::vector<std::uint8_t>& bytes,std::string* error){
    const char* xst=loading?"Sprite/spr_SPRANI_LOADING_CVT_Exst.sz":"Sprite/spr_SPRANI_SUMO_FE_CVT_Exst.sz";const char* ani_path=loading?"Sprani/ani_SPRANI_LOADING_CVT.sz":"Sprani/ani_SPRANI_SUMO_FE_CVT.sz";const std::uint32_t source_tex=loading?5u:129u;std::vector<std::uint8_t> xc,ac,x,a;if(!retail_asset_read_relative(store,xst,xc,MaxCacheBytes,error)||!retail_asset_read_relative(store,ani_path,ac,MaxCacheBytes,error)||!retail_asset_read_relative_inflated(store,xst,x,MaxCacheBytes,error)||!retail_asset_read_relative_inflated(store,ani_path,a,MaxCacheBytes,error))return false;std::vector<DdsEntry> entries;if(!scan_dds(x,source_tex,entries,error))return false;
    std::vector<std::uint32_t> tokens;if(loading){for(std::uint32_t i=0;i<9;++i)tokens.push_back(0x002f0000u|i);}else{if(a.size()<8||rd32(a.data())+4u!=a.size()){fail(error,"SUMO_FE wrapper invalid");return false;}const auto*d=a.data()+4;const auto table=rd32(d);const auto count=table/4u-1u;for(std::uint32_t i=0;i<count;++i){const auto p=rd32(d+i*4u);if(p+20<=a.size()-4u)tokens.push_back(0x00440000u|i);}}
    std::vector<FScene> scenes;std::uint32_t source_scenes=0;if(!decode_frontend_scenes(a,tokens,true,scenes,source_scenes,error))return false;std::vector<std::uint32_t> used;for(const auto&s:scenes)for(const auto&d:s.draws)if(std::find(used.begin(),used.end(),d.source)==used.end())used.push_back(d.source);if(!loading){used.clear();for(std::uint32_t i=0;i<source_tex;++i)used.push_back(i);}for(auto i:used)if(i>=entries.size()){fail(error,"SprAni draw texture outside XST");return false;}std::size_t draws=0;for(const auto&s:scenes)draws+=s.draws.size();const std::size_t tro=FepHeader,sro=tro+used.size()*FepTexRec,dro=sro+scenes.size()*FepSceneRec,po=dro+draws*FepDrawRec;std::size_t tex_end=po;for(auto i:used)tex_end+=entries[i].payload.size();const auto animation=std::vector<std::uint8_t>(a.begin()+4,a.end());const std::size_t size=tex_end+animation.size();if(size>24u*1024u*1024u){fail(error,"generated frontend cache exceeds limit");return false;}bytes.assign(size,0);std::memcpy(bytes.data(),loading?"OR2LDP2\0":"OR2FEP6\0",8);put32(bytes,8,loading?2u:6u);put32(bytes,12,224u);put32(bytes,16,static_cast<std::uint32_t>(used.size()));put32(bytes,20,32u);put32(bytes,24,static_cast<std::uint32_t>(scenes.size()));put32(bytes,28,24u);put32(bytes,32,static_cast<std::uint32_t>(draws));put32(bytes,36,64u);put32(bytes,40,static_cast<std::uint32_t>(tro));put32(bytes,44,static_cast<std::uint32_t>(sro));put32(bytes,48,static_cast<std::uint32_t>(dro));put32(bytes,52,static_cast<std::uint32_t>(po));put32(bytes,56,static_cast<std::uint32_t>(size));put32(bytes,60,source_tex);put32(bytes,64,source_scenes);put32(bytes,68,static_cast<std::uint32_t>(xc.size()));put32(bytes,72,static_cast<std::uint32_t>(x.size()));put32(bytes,76,static_cast<std::uint32_t>(ac.size()));put32(bytes,80,static_cast<std::uint32_t>(a.size()));put32(bytes,84,static_cast<std::uint32_t>(tex_end));put32(bytes,88,static_cast<std::uint32_t>(animation.size()));put32(bytes,92,crc(animation));
    std::size_t running=po;for(std::size_t n=0;n<used.size();++n){const auto& e=entries[used[n]];const auto o=tro+n*32u;put32(bytes,o,e.source);put32(bytes,o+4,e.width);put32(bytes,o+8,e.height);put32(bytes,o+12,e.format);put32(bytes,o+16,static_cast<std::uint32_t>(running));put32(bytes,o+20,static_cast<std::uint32_t>(e.payload.size()));put32(bytes,o+24,crc(e.payload));running+=e.payload.size();}
    std::size_t first=0,di=0;for(std::size_t si=0;si<scenes.size();++si){const auto& s=scenes[si];const auto o=sro+si*24u;put32(bytes,o,s.token);put32(bytes,o+4,s.width);put32(bytes,o+8,s.height);put32(bytes,o+12,static_cast<std::uint32_t>(first));put32(bytes,o+16,static_cast<std::uint32_t>(s.draws.size()));for(std::size_t li=0;li<s.draws.size();++li,++di){const auto&d=s.draws[li];const auto q=dro+di*64u;put32(bytes,q,d.source);for(unsigned c=0;c<4;++c)put32(bytes,q+4+c*4,d.crop[c]);for(unsigned c=0;c<8;++c)putf(bytes,q+20+c*4,d.corners[c]);put32(bytes,q+52,static_cast<std::uint32_t>(li));}first+=s.draws.size();}
    running=po;for(auto i:used){const auto&e=entries[i];std::memcpy(bytes.data()+running,e.payload.data(),e.payload.size());running+=e.payload.size();}std::memcpy(bytes.data()+tex_end,animation.data(),animation.size());return true;
}

}

bool build_retail_start_loading(RetailAssetStore& store,std::vector<std::uint8_t>& pack,std::string* error){
    if(error)error->clear();
    return build_frontend(store,true,pack,error);
}
bool load_retail_sprani(RetailAssetStore& store,const std::string& bank,std::uint32_t scenes,GameUiPack& pack,std::string* error){
    std::vector<std::uint8_t> a;
    if(!retail_asset_read_relative_inflated(store,"Sprani/ani_SPRANI_"+bank+"_CVT.sz",a,MaxCacheBytes,error))return false;
    if(a.size()<8||rd32(a.data())+4u!=a.size()){fail(error,bank+" animation wrapper invalid");return false;}
    pack=GameUiPack{};pack.scene_count=scenes;pack.animation.assign(a.begin()+4,a.end());
    GameUiScene last{};if(!game_ui_scene(pack,scenes-1u,last)){fail(error,bank+" animation scenes invalid");return false;}
    return true;
}
bool build_retail_font_pack(RetailAssetStore& store,FrontendFontPack& pack,std::string* error){
    std::vector<std::uint8_t> bytes;
    return build_fonts(store,bytes,error)&&parse_frontend_font_pack(bytes.data(),bytes.size(),pack,error);
}
}

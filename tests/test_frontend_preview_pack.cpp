#include "platform/frontend_preview_pack.hpp"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
using namespace outrun::platform;
namespace {
unsigned checks{};
void require(bool value,const char* message){++checks;if(!value){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
bool changed(const std::vector<GameUiDraw>& a,const std::vector<GameUiDraw>& b){
    if(a.size()!=b.size())return true;
    for(std::size_t i=0;i<a.size();++i){
        if(a[i].visible!=b[i].visible||
           std::fabs(a[i].opacity-b[i].opacity)>0.001f)return true;
        for(std::size_t c=0;c<4u;++c)for(std::size_t axis=0;axis<2u;++axis)
            if(std::fabs(a[i].corners[c][axis]-b[i].corners[c][axis])>0.001f)
                return true;
    }
    return false;
}
void put32(std::vector<std::uint8_t>& b,std::size_t o,std::uint32_t v){for(unsigned i=0;i<4u;++i)b[o+i]=std::uint8_t(v>>(i*8u));}
void putf(std::vector<std::uint8_t>& b,std::size_t o,float v){std::uint32_t bits{};std::memcpy(&bits,&v,4u);put32(b,o,bits);}
std::uint32_t crc32(const std::uint8_t* data,std::size_t size){std::uint32_t crc=0xffffffffu;for(std::size_t i=0;i<size;++i){crc^=data[i];for(unsigned bit=0;bit<8u;++bit)crc=(crc>>1u)^(0xedb88320u&std::uint32_t(0u-(crc&1u)));}return ~crc;}
std::vector<std::uint8_t> synthetic(){
    constexpr std::array<std::uint32_t,FrontendPreviewPackExpectedScenes> scene_draws{{3u,3u,3u,2u,4u,2u,3u,3u,3u}};
    constexpr std::size_t texture_offset=FrontendPreviewPackHeaderSize;
    constexpr std::size_t scene_offset=texture_offset+FrontendPreviewPackExpectedTextures*FrontendPreviewPackRecordSize;
    constexpr std::size_t draw_offset=scene_offset+FrontendPreviewPackExpectedScenes*FrontendPreviewSceneRecordSize;
    constexpr std::size_t payload_offset=draw_offset+FrontendPreviewPackExpectedDraws*FrontendPreviewDrawRecordSize;
    constexpr std::size_t payload_bytes=FrontendPreviewPackExpectedTextures*8u;
    std::vector<std::uint8_t> out(payload_offset+payload_bytes);
    const std::uint8_t magic[8]={'O','R','2','F','E','P','3',0};std::memcpy(out.data(),magic,8u);
    put32(out,8u,FrontendPreviewPackVersion);put32(out,12u,FrontendPreviewPackHeaderSize);
    put32(out,16u,FrontendPreviewPackExpectedTextures);put32(out,20u,FrontendPreviewPackRecordSize);
    put32(out,24u,FrontendPreviewPackExpectedScenes);put32(out,28u,FrontendPreviewSceneRecordSize);
    put32(out,32u,FrontendPreviewPackExpectedDraws);put32(out,36u,FrontendPreviewDrawRecordSize);
    put32(out,40u,texture_offset);put32(out,44u,scene_offset);put32(out,48u,draw_offset);
    put32(out,52u,payload_offset);put32(out,56u,std::uint32_t(out.size()));
    put32(out,60u,FrontendPreviewSourceTextureCount);put32(out,64u,FrontendPreviewSourceSceneCount);
    put32(out,68u,123u);put32(out,72u,456u);put32(out,76u,789u);put32(out,80u,1011u);
    std::size_t running=payload_offset;
    for(std::size_t i=0;i<FrontendPreviewPackExpectedTextures;++i){const auto record=texture_offset+i*FrontendPreviewPackRecordSize;put32(out,record,FrontendPreviewSourceIndices[i]);put32(out,record+4u,4u);put32(out,record+8u,4u);put32(out,record+12u,1u);put32(out,record+16u,std::uint32_t(running));put32(out,record+20u,8u);for(std::size_t j=0;j<8u;++j)out[running+j]=std::uint8_t(i*8u+j);put32(out,record+24u,crc32(out.data()+running,8u));running+=8u;}
    std::uint32_t first_draw=0u;
    for(std::size_t scene=0;scene<scene_draws.size();++scene){const auto record=scene_offset+scene*FrontendPreviewSceneRecordSize;put32(out,record,FrontendPreviewTokens[scene]);put32(out,record+4u,FrontendPreviewSceneWidth);put32(out,record+8u,FrontendPreviewSceneHeight);put32(out,record+12u,first_draw);put32(out,record+16u,scene_draws[scene]);for(std::uint32_t local=0;local<scene_draws[scene];++local){const auto index=first_draw+local;const auto draw=draw_offset+index*FrontendPreviewDrawRecordSize;put32(out,draw,FrontendPreviewSourceIndices[index%FrontendPreviewSourceIndices.size()]);put32(out,draw+12u,4u);put32(out,draw+16u,4u);const float x=float(local*4u);const float corners[8]={x,0.0f,x+4.0f,0.0f,x+4.0f,4.0f,x,4.0f};for(std::size_t c=0;c<8u;++c)putf(out,draw+20u+c*4u,corners[c]);put32(out,draw+52u,local);}first_draw+=scene_draws[scene];}
    return out;
}
}
int main(int argc,char** argv){
    auto bytes=synthetic();FrontendPreviewPack pack{};std::string error;
    require(parse_frontend_preview_pack(bytes.data(),bytes.size(),pack,&error),"valid synthetic OR2FEP3");
    require(pack.textures.size()==10u&&pack.scenes.size()==9u&&pack.draws.size()==26u,"complete frontend menu retained");
    require(pack.scenes[0].token==FrontendPreviewInitialToken&&pack.scenes[4].draw_count==4u&&pack.scenes[8].first_draw==23u,"scene ranges retained");
    require(find_frontend_preview_scene(pack,0x0044008bu)==&pack.scenes[4]&&find_frontend_preview_scene(pack,0xdeadbeefu)==nullptr,"token lookup");
    auto bad=bytes;bad[0]='X';require(!parse_frontend_preview_pack(bad.data(),bad.size(),pack,&error),"bad magic rejected");
    bad=bytes;put32(bad,FrontendPreviewPackHeaderSize,72u);require(!parse_frontend_preview_pack(bad.data(),bad.size(),pack,&error),"wrong source index rejected");
    bad=bytes;put32(bad,FrontendPreviewPackHeaderSize+FrontendPreviewPackExpectedTextures*FrontendPreviewPackRecordSize,0x44u);require(!parse_frontend_preview_pack(bad.data(),bad.size(),pack,&error),"wrong scene token rejected");
    bad=bytes;bad.back()^=1u;require(!parse_frontend_preview_pack(bad.data(),bad.size(),pack,&error),"bad payload crc rejected");
    if(argc>=2){FrontendPreviewPack real{};require(load_frontend_preview_pack_file(argv[1],real,&error),"generated SUMO_FE pack loads");require(real.source_xst_compressed_bytes==3727034u&&real.source_xst_decompressed_bytes==20220900u&&real.source_animation_compressed_bytes==159163u&&real.source_animation_decompressed_bytes==1104424u,"real SUMO_FE sources identified");require(find_frontend_preview_scene(real,FrontendPreviewInitialToken)!=nullptr,"original initial scene retained");if(real.scenes.size()==9u){require(real.draws.size()==26u&&real.textures.size()==10u,"legacy menu preview retained");}else{require(real.scenes.size()==124u&&real.draws.size()==2428u&&real.textures.size()==82u,"full-screen original archive retained");require(find_frontend_preview_scene(real,0x004400deu)!=nullptr,"late original full-screen scene retained");require(real.animation.size()==1104420u,"authored SUMO_FE animation retained");GameUiPack animation{};require(make_frontend_animation_view(real,animation,&error),error.c_str());for(const auto& scene:real.scenes){std::vector<GameUiDraw> draws;require(game_ui_scene_frame_draws(animation,scene.token&0xffffu,30.0f,draws,&error),error.c_str());require(draws.size()==scene.draw_count,"SUMO_FE animated layer count stable");}std::vector<GameUiDraw> first,later;require(game_ui_scene_frame_draws(animation,0u,0.0f,first,&error)&&game_ui_scene_frame_draws(animation,0u,60.0f,later,&error)&&changed(first,later),"authored SUMO_FE scene 0 visibly animates");}}
    if(argc>=3){FrontendPreviewPack loading{};require(load_frontend_preview_pack_file(argv[2],loading,&error),error.c_str());require(loading.source_texture_count==5u&&loading.source_scene_count==9u,"retail START archive source layout");require(loading.textures.size()==5u&&loading.scenes.size()==9u&&loading.draws.size()==171u,"all START loading scenes and sprites retained");require(find_frontend_preview_scene(loading,0x002f0000u)!=nullptr&&find_frontend_preview_scene(loading,0x002f0008u)!=nullptr,"START loading scene range retained");GameUiPack animation{};require(make_frontend_animation_view(loading,animation,&error),error.c_str());for(const auto& scene:loading.scenes){std::vector<GameUiDraw> draws;require(game_ui_scene_frame_draws(animation,scene.token&0xffffu,60.0f,draws,&error),error.c_str());require(draws.size()==scene.draw_count,"START animated layer count stable");}std::vector<GameUiDraw> first,later;require(game_ui_scene_frame_draws(animation,2u,0.0f,first,&error)&&game_ui_scene_frame_draws(animation,2u,60.0f,later,&error)&&changed(first,later),"authored START scene 2 visibly animates");}
    std::printf("frontend_preview_pack: %u checks passed\n",checks);return 0;
}

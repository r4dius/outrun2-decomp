#include "platform/frontend_preview_pack.hpp"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <utility>

namespace outrun::platform {
namespace {
constexpr std::array<std::uint8_t,8> Magic{{'O','R','2','F','E','P','3',0}};
constexpr std::array<std::uint8_t,8> FullMagic{{'O','R','2','F','E','P','4',0}};
constexpr std::array<std::uint8_t,8> AnimatedMagic{{'O','R','2','F','E','P','5',0}};
constexpr std::array<std::uint8_t,8> CompleteMagic{{'O','R','2','F','E','P','6',0}};
constexpr std::array<std::uint8_t,8> LoadingMagic{{'O','R','2','L','D','P','1',0}};
constexpr std::array<std::uint8_t,8> AnimatedLoadingMagic{{'O','R','2','L','D','P','2',0}};
constexpr std::size_t MaxPackBytes=24u*1024u*1024u;
void fail(std::string* error,const char* message){if(error)*error=message;}
std::uint32_t u32(const std::uint8_t* p){return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8u)|(std::uint32_t(p[2])<<16u)|(std::uint32_t(p[3])<<24u);}
float f32(const std::uint8_t* p){const auto bits=u32(p);float value{};std::memcpy(&value,&bits,sizeof(value));return value;}
std::uint32_t crc32(const std::uint8_t* data,std::size_t size){
    std::uint32_t crc=0xffffffffu;
    for(std::size_t i=0;i<size;++i){crc^=data[i];for(unsigned bit=0;bit<8u;++bit)crc=(crc>>1u)^(0xedb88320u&std::uint32_t(0u-(crc&1u)));}
    return ~crc;
}
bool valid_format(std::uint32_t value){return value>=1u&&value<=3u;}
std::size_t bc_bytes(std::uint32_t width,std::uint32_t height,std::uint32_t format){
    if(width==0u||height==0u||width>2048u||height>2048u||!valid_format(format))return 0u;
    const auto blocks_x=(std::size_t(width)+3u)/4u,blocks_y=(std::size_t(height)+3u)/4u;
    if(blocks_x>std::numeric_limits<std::size_t>::max()/blocks_y)return 0u;
    return blocks_x*blocks_y*(format==1u?8u:16u);
}
const FrontendPreviewTexture* find_texture(const FrontendPreviewPack& pack,std::uint32_t source){
    for(const auto& texture:pack.textures)if(texture.source_index==source)return &texture;
    return nullptr;
}
}

const FrontendPreviewScene* find_frontend_preview_scene(
    const FrontendPreviewPack& pack,std::uint32_t token){
    for(const auto& scene:pack.scenes)if(scene.token==token)return &scene;
    return nullptr;
}

bool parse_frontend_preview_pack(const std::uint8_t* data,std::size_t size,
                                 FrontendPreviewPack& pack,std::string* error){
    if(error)error->clear();
    if((!data&&size!=0u)||size<FrontendPreviewPackHeaderSize||size>MaxPackBytes){fail(error,"frontend pack size outside bounds");return false;}
    const bool animated_loading=std::memcmp(data,AnimatedLoadingMagic.data(),AnimatedLoadingMagic.size())==0;
    const bool complete=std::memcmp(data,CompleteMagic.data(),CompleteMagic.size())==0;
    const bool animated=complete||animated_loading||std::memcmp(data,AnimatedMagic.data(),AnimatedMagic.size())==0;
    const bool full=animated||std::memcmp(data,FullMagic.data(),FullMagic.size())==0;
    const bool loading=animated_loading||std::memcmp(data,LoadingMagic.data(),LoadingMagic.size())==0;
    if(!full&&!loading&&std::memcmp(data,Magic.data(),Magic.size())!=0){fail(error,"frontend pack magic mismatch");return false;}
    const auto texture_count=u32(data+16u),texture_record_size=u32(data+20u);
    const auto scene_count=u32(data+24u),scene_record_size=u32(data+28u);
    const auto draw_count=u32(data+32u),draw_record_size=u32(data+36u);
    const auto texture_record_offset=u32(data+40u),scene_record_offset=u32(data+44u);
    const auto draw_record_offset=u32(data+48u),payload_offset=u32(data+52u),file_size=u32(data+56u);
    const auto source_texture_count=loading?5u:FrontendPreviewSourceTextureCount;
    const auto source_scene_count=loading?9u:FrontendPreviewSourceSceneCount;
    const auto animation_offset=u32(data+84u),animation_bytes=u32(data+88u);
    if(u32(data+8u)!=(complete?6u:animated_loading?2u:loading?1u:animated?5u:full?4u:FrontendPreviewPackVersion)||u32(data+12u)!=FrontendPreviewPackHeaderSize||
       texture_count==0u||texture_count>source_texture_count||
       (!full&&!loading&&texture_count!=FrontendPreviewPackExpectedTextures)||
       texture_record_size!=FrontendPreviewPackRecordSize||
       scene_count==0u||scene_count>source_scene_count||
       (loading&&scene_count!=source_scene_count)||
       (!full&&!loading&&scene_count!=FrontendPreviewPackExpectedScenes)||
       scene_record_size!=FrontendPreviewSceneRecordSize||
       draw_count==0u||draw_count>10000u||(!full&&!loading&&draw_count!=FrontendPreviewPackExpectedDraws)||
       draw_record_size!=FrontendPreviewDrawRecordSize||
       texture_record_offset!=FrontendPreviewPackHeaderSize||
       scene_record_offset!=texture_record_offset+texture_count*texture_record_size||
       draw_record_offset!=scene_record_offset+scene_count*scene_record_size||
       payload_offset!=draw_record_offset+draw_count*draw_record_size||payload_offset>size||file_size!=size||
       u32(data+60u)!=source_texture_count||u32(data+64u)!=source_scene_count||
       (!animated&&(animation_offset!=0u||animation_bytes!=0u||u32(data+92u)!=0u))||
       (animated&&(animation_bytes<4u*(source_scene_count+1u)||
                   animation_offset>size||animation_bytes!=size-animation_offset||
                   crc32(data+animation_offset,animation_bytes)!=u32(data+92u)))){
        fail(error,"frontend pack header mismatch");return false;
    }
    for(std::size_t off=68u;off<=80u;off+=4u)if(u32(data+off)==0u){fail(error,"frontend source sizes missing");return false;}

    FrontendPreviewPack next{};
    next.source_texture_count=u32(data+60u);next.source_scene_count=u32(data+64u);
    next.source_xst_compressed_bytes=u32(data+68u);next.source_xst_decompressed_bytes=u32(data+72u);
    next.source_animation_compressed_bytes=u32(data+76u);next.source_animation_decompressed_bytes=u32(data+80u);
    std::memcpy(next.source_xst_compressed_sha256.data(),data+96u,32u);
    std::memcpy(next.source_xst_decompressed_sha256.data(),data+128u,32u);
    std::memcpy(next.source_animation_compressed_sha256.data(),data+160u,32u);
    std::memcpy(next.source_animation_decompressed_sha256.data(),data+192u,32u);
    next.textures.reserve(texture_count);
    std::size_t running=payload_offset;
    for(std::size_t i=0;i<texture_count;++i){
        const auto* record=data+texture_record_offset+i*FrontendPreviewPackRecordSize;
        const auto source_index=u32(record),width=u32(record+4u),height=u32(record+8u),format=u32(record+12u);
        const auto offset=u32(record+16u),bytes=u32(record+20u),expected_crc=u32(record+24u);
        const auto expected_bytes=bc_bytes(width,height,format);
        if(source_index>=source_texture_count||
           (!full&&!loading&&source_index!=FrontendPreviewSourceIndices[i])||
           find_texture(next,source_index)||offset!=running||expected_bytes==0u||
           bytes!=expected_bytes||bytes>size-running||u32(record+28u)!=0u||crc32(data+offset,bytes)!=expected_crc){fail(error,"frontend texture record mismatch");return false;}
        FrontendPreviewTexture texture{};texture.source_index=source_index;texture.width=width;texture.height=height;
        texture.format=static_cast<MeshPreviewTextureFormat>(format);texture.bytes.assign(data+offset,data+offset+bytes);
        next.textures.push_back(std::move(texture));running+=bytes;
    }
    next.scenes.reserve(scene_count);std::uint32_t expected_first_draw=0u;
    for(std::size_t i=0;i<scene_count;++i){
        const auto* record=data+scene_record_offset+i*FrontendPreviewSceneRecordSize;
        FrontendPreviewScene scene{u32(record),u32(record+4u),u32(record+8u),u32(record+12u),u32(record+16u)};
        if((loading?scene.token!=0x002f0000u+i:
             full?(scene.token<0x00440000u||
                   scene.token>=0x00440000u+FrontendPreviewSourceSceneCount||
                   (i!=0u&&scene.token<=next.scenes.back().token)):
                  scene.token!=FrontendPreviewTokens[i])||
           (complete?(scene.width==0u||scene.width>4096u||scene.height==0u||scene.height>4096u):
              (scene.width!=FrontendPreviewSceneWidth||scene.height!=FrontendPreviewSceneHeight))||
           scene.first_draw!=expected_first_draw||
           (!complete&&scene.draw_count==0u)||scene.first_draw>draw_count||scene.draw_count>draw_count-scene.first_draw||
           u32(record+20u)!=0u){fail(error,"frontend scene record mismatch");return false;}
        expected_first_draw+=scene.draw_count;next.scenes.push_back(scene);
    }
    if(expected_first_draw!=draw_count){fail(error,"frontend scene draw coverage mismatch");return false;}
    next.draws.reserve(draw_count);std::size_t scene_index=0u;
    for(std::size_t i=0;i<draw_count;++i){
        while(scene_index+1u<next.scenes.size()&&i>=next.scenes[scene_index+1u].first_draw)++scene_index;
        const auto* record=data+draw_record_offset+i*FrontendPreviewDrawRecordSize;
        FrontendPreviewDraw draw{};draw.source_texture=u32(record);
        for(std::size_t c=0;c<4u;++c)draw.crop[c]=u32(record+4u+c*4u);
        for(std::size_t c=0;c<4u;++c){draw.corners[c][0]=f32(record+20u+c*8u);draw.corners[c][1]=f32(record+24u+c*8u);}
        draw.layer_order=u32(record+52u);draw.frame_index=u32(record+56u);
        const auto* texture=find_texture(next,draw.source_texture);
        const auto local_order=static_cast<std::uint32_t>(i)-next.scenes[scene_index].first_draw;
        if(!texture||draw.crop[0]>=draw.crop[2]||draw.crop[1]>=draw.crop[3]||
           draw.crop[2]>texture->width||draw.crop[3]>texture->height||draw.layer_order!=local_order||
           draw.frame_index!=0u||u32(record+60u)!=0u){fail(error,"frontend draw record mismatch");return false;}
        for(const auto& corner:draw.corners)for(float value:corner)
            // Full archive includes long off-canvas text strips (retail max
            // 4660px); scissoring, not truncating their geometry, clips them.
            if(!std::isfinite(value)||std::fabs(value)>(complete?8192.0f:4096.0f)){fail(error,"frontend draw coordinate outside bounds");return false;}
        next.draws.push_back(draw);
    }
    if(running!=(animated?animation_offset:size)){
        fail(error,"frontend pack trailing bytes");return false;
    }
    if(animated){
        const auto table=4u*(source_scene_count+1u);
        if(u32(data+animation_offset)!=table||
           u32(data+animation_offset+table-4u)!=0u){
            fail(error,"SUMO_FE animation pointer table mismatch");return false;
        }
        next.animation.assign(data+animation_offset,data+size);
    }
    pack=std::move(next);return true;
}

bool load_frontend_preview_pack_file(const char* path,FrontendPreviewPack& pack,
                                     std::string* error){
    if(error)error->clear();
    if(!path){fail(error,"null frontend pack path");return false;}
    std::FILE* file=std::fopen(path,"rb");if(!file){fail(error,"cannot open frontend pack");return false;}
    if(std::fseek(file,0,SEEK_END)!=0){std::fclose(file);fail(error,"cannot size frontend pack");return false;}
    const auto length=std::ftell(file);if(length<0||static_cast<std::uint64_t>(length)>MaxPackBytes){std::fclose(file);fail(error,"frontend pack file outside bounds");return false;}
    std::rewind(file);std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    const auto read=bytes.empty()?0u:std::fread(bytes.data(),1,bytes.size(),file);const int extra=std::fgetc(file);std::fclose(file);
    if(read!=bytes.size()||extra!=EOF){fail(error,"frontend pack read mismatch");return false;}
    return parse_frontend_preview_pack(bytes.data(),bytes.size(),pack,error);
}

bool make_frontend_animation_view(const FrontendPreviewPack& source,
                                  GameUiPack& animation,std::string* error){
    if(error)error->clear();
    if(source.animation.empty()||source.source_scene_count==0u||
       source.source_texture_count==0u){
        fail(error,"frontend animation absent");return false;
    }
    GameUiPack next{};
    next.scene_count=source.source_scene_count;
    next.animation=source.animation;
    next.textures.resize(source.source_texture_count);
    for(const auto& texture:source.textures){
        auto& slot=next.textures[texture.source_index];
        slot.width=texture.width;slot.height=texture.height;
        slot.format=texture.format;
    }
    for(const auto& scene:source.scenes){
        std::vector<GameUiDraw> draws;
        const auto index=scene.token&0xffffu;
        if(!game_ui_scene_frame_draws(next,index,0.0f,draws,error)||
           draws.size()!=scene.draw_count){
            if(error&&error->empty())*error="SUMO_FE animated draw count mismatch";
            return false;
        }
        for(std::size_t i=0;i<draws.size();++i){
            const auto& static_draw=source.draws[scene.first_draw+i];
            if(draws[i].texture!=static_draw.source_texture||
               draws[i].crop!=static_draw.crop){
                fail(error,"SUMO_FE animated layer order mismatch");return false;
            }
        }
    }
    animation=std::move(next);return true;
}

} // namespace outrun::platform

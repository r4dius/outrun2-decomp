#include "platform/game_ui_pack.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <limits>
#include <utility>

namespace outrun::platform {
namespace {
constexpr std::array<std::uint8_t,8> Magic{{'O','R','2','G','U','I','1',0}};
constexpr std::size_t HeaderBytes=64u,RecordBytes=24u,MaxBytes=4u*1024u*1024u;
void fail(std::string* error,const char* message){if(error)*error=message;}
std::uint32_t u32(const std::uint8_t* p){return std::uint32_t(p[0])|
    (std::uint32_t(p[1])<<8u)|(std::uint32_t(p[2])<<16u)|(std::uint32_t(p[3])<<24u);}
float f32(const std::uint8_t* p){const auto value=u32(p);float result{};
    std::memcpy(&result,&value,sizeof(result));return result;}
std::uint32_t crc32(const std::uint8_t* data,std::size_t size){
    std::uint32_t crc=0xffffffffu;
    for(std::size_t i=0;i<size;++i){crc^=data[i];for(unsigned b=0;b<8u;++b)
        crc=(crc>>1u)^(0xedb88320u&std::uint32_t(0u-(crc&1u)));}
    return ~crc;
}
bool sample_track(const std::uint8_t* data,std::size_t size,std::uint32_t offset,
                  std::uint32_t scene_count,float frame,float& result,
                  std::string* error){
    if(offset<4u*(scene_count+1u)||offset>size||size-offset<6u){
        fail(error,"GAME keyframe pointer outside animation");return false;
    }
    const auto count=u32(data+offset);
    if(count==0u||count>1024u||count>(size-offset-6u)/8u){
        fail(error,"GAME keyframe table outside animation");return false;
    }
    // The header stores fixed-point exponent bytes for tangent scaling.
    // Their difference matches the straight-line slopes of 11,011/11,155
    // retail segments; the others encode authored easing.
    const auto fraction_bits=data[offset+4u];
    const auto tangent_bits=data[offset+5u];
    // SUMO_FE scene 1 also contains the valid exponent pair (7,6).
    // A negative difference multiplies the slope; it is not a corrupt track.
    const int tangent_exponent=int(tangent_bits)-int(fraction_bits);
    if(fraction_bits>16u||tangent_bits>16u||
       tangent_exponent < -16||tangent_exponent>16){
        fail(error,"GAME keyframe tangent scale invalid");return false;
    }
    const float tangent_divisor=std::ldexp(1.0f,
        tangent_exponent);
    const auto first=data+offset+6u;
    std::uint32_t previous_frame=0u;
    for(std::uint32_t i=0u;i<count;++i){
        const auto knot=first+i*8u;
        const auto knot_frame=std::uint32_t(knot[0])|(std::uint32_t(knot[1])<<8u);
        if(i!=0u&&knot_frame<=previous_frame){
            fail(error,"GAME keyframe times are not increasing");return false;
        }
        previous_frame=knot_frame;
    }
    const auto value=[](const std::uint8_t* knot){
        const auto raw=std::uint16_t(knot[2])|(std::uint16_t(knot[3])<<8u);
        return float(static_cast<std::int16_t>(raw))/256.0f;
    };
    const auto tangent=[&](const std::uint8_t* knot,std::size_t at){
        const auto raw=std::uint16_t(knot[at])|
            (std::uint16_t(knot[at+1u])<<8u);
        return float(static_cast<std::int16_t>(raw))/256.0f/tangent_divisor;
    };
    const auto first_frame=std::uint32_t(first[0])|(std::uint32_t(first[1])<<8u);
    if(frame<=float(first_frame)){result=value(first);return true;}
    for(std::uint32_t i=1u;i<count;++i){
        const auto knot=first+i*8u;
        const auto next_frame=std::uint32_t(knot[0])|(std::uint32_t(knot[1])<<8u);
        if(frame<=float(next_frame)){
            const auto previous=first+(i-1u)*8u;
            const auto prior_frame=std::uint32_t(previous[0])|
                (std::uint32_t(previous[1])<<8u);
            const float duration=float(next_frame-prior_frame);
            const float t=(frame-float(prior_frame))/duration;
            const float t2=t*t,t3=t2*t;
            result=(2.0f*t3-3.0f*t2+1.0f)*value(previous)+
                (t3-2.0f*t2+t)*duration*tangent(previous,6u)+
                (-2.0f*t3+3.0f*t2)*value(knot)+
                (t3-t2)*duration*tangent(knot,4u);
            return true;
        }
    }
    result=value(first+(count-1u)*8u);return true;
}
}

bool game_ui_scene(const GameUiPack& pack,std::uint32_t index,GameUiScene& scene){
    if(index>=pack.scene_count||pack.animation.size()<(pack.scene_count+1u)*4u)return false;
    const auto* data=pack.animation.data();const auto size=pack.animation.size();
    const auto offset=u32(data+index*4u);
    if(offset>size||size-offset<20u)return false;
    const auto components=u32(data+offset),component_ptr=u32(data+offset+4u);
    const auto footage=u32(data+offset+8u),footage_ptr=u32(data+offset+12u);
    const auto packed_size=u32(data+offset+16u);
    if(components==0u||components>1024u||footage>4096u||
       component_ptr>size||components>(size-component_ptr)/36u||
       footage_ptr>size||footage>(size-footage_ptr)/24u||
       (packed_size&0xffffu)==0u||(packed_size>>16u)==0u)return false;
    scene={packed_size&0xffffu,packed_size>>16u,components,footage};return true;
}

static bool game_ui_scene_draws_impl(const GameUiPack& pack,std::uint32_t index,
                                    float frame,bool animate,
                                    std::vector<GameUiDraw>& draws,std::string* error){
    if(error)error->clear();
    draws.clear();
    if(animate&&!std::isfinite(frame)){
        fail(error,"GAME animation frame is not finite");return false;
    }
    GameUiScene scene{};
    if(!game_ui_scene(pack,index,scene)){
        fail(error,"GAME scene header outside animation table");return false;
    }
    const auto* data=pack.animation.data();const auto size=pack.animation.size();
    const auto scene_offset=u32(data+index*4u);
    const auto component_base=u32(data+scene_offset+4u);
    const auto footage_base=u32(data+scene_offset+12u);
    const auto in_bounds=[&](std::uint32_t offset,std::size_t bytes){
        return offset<=size&&bytes<=size-offset;
    };
    struct Affine {float a,b,c,d,x,y;};
    const auto compose=[](const Affine& p,const Affine& q){
        return Affine{p.a*q.a+p.c*q.b,p.b*q.a+p.d*q.b,
                      p.a*q.c+p.c*q.d,p.b*q.c+p.d*q.d,
                      p.a*q.x+p.c*q.y+p.x,p.b*q.x+p.d*q.y+p.y};
    };
    std::array<std::uint32_t,16> active{};
    std::function<bool(std::uint32_t,const Affine&,unsigned,bool,float)> visit;
    visit=[&](std::uint32_t component,const Affine& parent,unsigned depth,
              bool parent_visible,float parent_opacity){
        if(depth>=active.size()||component<component_base||
           (component-component_base)%36u!=0u||
           (component-component_base)/36u>=scene.components||
           !in_bounds(component,36u)){
            fail(error,"GAME component reference/depth invalid");return false;
        }
        for(unsigned i=0;i<depth;++i)if(active[i]==component){
            fail(error,"GAME component graph has a cycle");return false;
        }
        active[depth]=component;
        const auto layer_count=u32(data+component+28u);
        const auto layer_base=u32(data+component+32u);
        if(layer_count>4096u||!in_bounds(layer_base,std::size_t(layer_count)*76u)){
            fail(error,"GAME layer table outside animation");return false;
        }
        // PC painter order is reverse layer-index order, as in SUMO_FE.
        for(std::uint32_t remaining=layer_count;remaining!=0u;--remaining){
            const auto layer=layer_base+(remaining-1u)*76u;
            const auto source=u32(data+layer+8u);
            const float anchor_x=f32(data+layer+16u),anchor_y=f32(data+layer+20u);
            float pose[6]={f32(data+layer+24u),f32(data+layer+28u),
                           f32(data+layer+32u),f32(data+layer+36u),
                           f32(data+layer+40u),f32(data+layer+44u)};
            if(animate)for(unsigned channel=0u;channel<6u;++channel){
                const auto pointer=u32(data+layer+48u+channel*4u);
                if(pointer!=0u&&!sample_track(data,size,pointer,
                    pack.scene_count,frame,pose[channel],error))
                    return false;
            }
            const float position_x=pose[0],position_y=pose[1];
            const float scale_x=pose[2],scale_y=pose[3];
            const float degrees=pose[4];
            const float values[]{anchor_x,anchor_y,position_x,position_y,
                                 scale_x,scale_y,degrees};
            for(float value:values)if(!std::isfinite(value)||std::fabs(value)>100000.0f){
                fail(error,"GAME layer has nonfinite/out-of-range base pose");return false;
            }
            const float radians=degrees*0.01745329251994329577f;
            const float sx=scale_x*0.01f,sy=scale_y*0.01f;
            const float a=std::cos(radians)*sx,b=std::sin(radians)*sx;
            const float c=-std::sin(radians)*sy,d=std::cos(radians)*sy;
            const Affine local{a,b,c,d,position_x-a*anchor_x-c*anchor_y,
                               position_y-b*anchor_x-d*anchor_y};
            const auto matrix=compose(parent,local);
            const auto interval=u32(data+layer);
            const auto first_frame=static_cast<std::int16_t>(interval&0xffffu);
            const auto last_frame=static_cast<std::int16_t>(interval>>16u);
            const bool layer_visible=!animate||
                (frame>=float(first_frame)&&frame<=float(last_frame));
            const float opacity=animate?std::clamp(pose[5]*0.01f,0.0f,1.0f):1.0f;
            if(source>=component_base&&source-component_base<scene.components*36u&&
               (source-component_base)%36u==0u){
                if(!visit(source,matrix,depth+1u,parent_visible&&layer_visible,
                          parent_opacity*opacity))return false;
                continue;
            }
            if(source<footage_base||source-footage_base>=scene.footage*24u||
               (source-footage_base)%24u!=0u||!in_bounds(source,24u)){
                fail(error,"GAME layer source is neither component nor footage");return false;
            }
            const auto width=u32(data+source),height=u32(data+source+4u);
            const auto frame_count=u32(data+source+12u),frame_ptr=u32(data+source+16u);
            if(width==0u||height==0u||width>2048u||height>2048u||
               frame_count>1u||(frame_count!=0u&&
                                  !in_bounds(frame_ptr,std::size_t(frame_count)*20u))){
                fail(error,"GAME footage frame layout invalid");return false;
            }
            if(frame_count==0u)continue; // original component placeholder
            const auto texture=u32(data+frame_ptr);
            if(texture>=pack.textures.size()){
                fail(error,"GAME footage references missing atlas");return false;
            }
            const float crop_f[]{f32(data+frame_ptr+4u),f32(data+frame_ptr+8u),
                                 f32(data+frame_ptr+12u),f32(data+frame_ptr+16u)};
            std::array<std::uint32_t,4> crop{};
            for(unsigned i=0;i<4u;++i){
                if(!std::isfinite(crop_f[i])||crop_f[i]<0.0f||crop_f[i]>2048.0f||
                   std::fabs(crop_f[i]-std::round(crop_f[i]))>0.01f){
                    fail(error,"GAME footage crop is not integral/in bounds");return false;
                }
                crop[i]=static_cast<std::uint32_t>(std::lround(crop_f[i]));
            }
            const auto& atlas=pack.textures[texture];
            if(crop[2]<=crop[0]||crop[3]<=crop[1]||crop[2]>atlas.width||
               crop[3]>atlas.height){
                fail(error,"GAME footage crop outside atlas");return false;
            }
            if(draws.size()>=4096u){fail(error,"GAME scene exceeds draw cap");return false;}
            GameUiDraw draw{};draw.texture=texture;draw.crop=crop;
            draw.first_frame=first_frame;
            draw.last_frame=last_frame;
            if(animate){
                draw.visible=parent_visible&&layer_visible;
                draw.opacity=parent_opacity*opacity;
                if(draw.opacity<0.05f)draw.visible=false;
            }
            for(unsigned i=0;i<6u;++i){
                const auto key=u32(data+layer+48u+i*4u);
                if(key>=4u*(pack.scene_count+1u)&&key<size)draw.has_keyframes=true;
            }
            constexpr float corners[4][2]={{0.0f,0.0f},{1.0f,0.0f},
                                           {1.0f,1.0f},{0.0f,1.0f}};
            for(unsigned i=0;i<4u;++i){
                const auto x=corners[i][0]*float(width),y=corners[i][1]*float(height);
                draw.corners[i]={{matrix.a*x+matrix.c*y+matrix.x,
                                  matrix.b*x+matrix.d*y+matrix.y}};
            }
            draws.push_back(draw);
        }
        return true;
    };
    return visit(component_base+(scene.components-1u)*36u,
                 {1.0f,0.0f,0.0f,1.0f,0.0f,0.0f},0u,true,1.0f);
}

bool game_ui_scene_base_draws(const GameUiPack& pack,std::uint32_t index,
                              std::vector<GameUiDraw>& draws,std::string* error){
    return game_ui_scene_draws_impl(pack,index,0.0f,false,draws,error);
}

bool game_ui_scene_frame_draws(const GameUiPack& pack,std::uint32_t index,
                              float frame,std::vector<GameUiDraw>& draws,
                              std::string* error){
    return game_ui_scene_draws_impl(pack,index,frame,true,draws,error);
}

static bool parse_ui_pack(const std::uint8_t* data,std::size_t size,
                         GameUiPack& pack,std::string* error,bool shared){
    if(error)error->clear();
    if((!data&&size!=0u)||size<HeaderBytes||size>MaxBytes||
       std::memcmp(data,Magic.data(),Magic.size())!=0){fail(error,"GAME UI pack magic/size mismatch");return false;}
    const auto version=u32(data+8u),header=u32(data+12u),textures=u32(data+16u);
    const auto scenes=u32(data+20u),animation_offset=u32(data+24u);
    const auto animation_size=u32(data+28u),record_offset=u32(data+32u);
    const auto payload_offset=u32(data+36u),file_size=u32(data+40u);
    const auto source_sprite_bytes=u32(data+44u),source_animation_bytes=u32(data+48u);
    const auto stride=u32(data+52u),reserved=u32(data+56u),expected_crc=u32(data+60u);
    // Do not let ETC's 319 scenes enter GAME renderers with 108-slot arrays.
    if(version!=1u||header!=HeaderBytes||textures!=(shared?11u:7u)||scenes!=(shared?319u:108u)||
       record_offset!=HeaderBytes||stride!=RecordBytes||
       animation_offset!=HeaderBytes+textures*RecordBytes||
       animation_size<4u*(scenes+1u)||animation_size>MaxBytes||
       animation_offset>size||animation_size>size-animation_offset||
       payload_offset!=animation_offset+animation_size||payload_offset>size||
       file_size!=size||source_sprite_bytes<1000u||
       source_animation_bytes!=animation_size+4u||reserved!=0u||
       crc32(data+HeaderBytes,size-HeaderBytes)!=expected_crc){
        fail(error,"GAME UI pack header/CRC mismatch");return false;
    }
    GameUiPack next{};next.scene_count=scenes;
    next.animation.assign(data+animation_offset,data+payload_offset);
    if(u32(next.animation.data())!=4u*(scenes+1u)||
       u32(next.animation.data()+4u*scenes)!=0u){
        fail(error,"GAME UI animation pointer table mismatch");return false;
    }
    next.textures.reserve(textures);
    std::size_t running=payload_offset;
    for(std::size_t index=0;index<textures;++index){
        const auto* record=data+record_offset+index*RecordBytes;
        const auto source_index=u32(record),width=u32(record+4u),height=u32(record+8u);
        const auto format=u32(record+12u),offset=u32(record+16u),bytes=u32(record+20u);
        if(source_index!=index||width==0u||height==0u||width>2048u||height>2048u||
           (format!=1u&&format!=2u&&format!=3u)||offset!=running||
           running>size||bytes>size-running){fail(error,"GAME UI texture record mismatch");return false;}
        const auto expected=((std::size_t(width)+3u)/4u)*((std::size_t(height)+3u)/4u)*
            (format==1u?8u:16u);
        if(bytes!=expected){fail(error,"GAME UI DDS byte count mismatch");return false;}
        GameUiTexture texture{};texture.width=width;texture.height=height;
        texture.format=static_cast<MeshPreviewTextureFormat>(format);
        texture.bytes.assign(data+running,data+running+bytes);
        next.textures.push_back(std::move(texture));running+=bytes;
    }
    if(running!=size){fail(error,"GAME UI pack trailing data");return false;}
    for(std::uint32_t index=0;index<scenes;++index){
        GameUiScene scene{};if(!game_ui_scene(next,index,scene)){
            fail(error,"GAME UI scene structure mismatch");return false;
        }
        std::vector<GameUiDraw> draws;
        if(!game_ui_scene_base_draws(next,index,draws,error))return false;
        if(!game_ui_scene_frame_draws(next,index,0.0f,draws,error))return false;
    }
    pack=std::move(next);return true;
}

bool parse_game_ui_pack(const std::uint8_t* p,std::size_t n,GameUiPack& pack,std::string* error){return parse_ui_pack(p,n,pack,error,false);}
bool parse_shared_ui_pack(const std::uint8_t* p,std::size_t n,GameUiPack& pack,std::string* error){return parse_ui_pack(p,n,pack,error,true);}
static bool load_ui_pack_file(const char* path,GameUiPack& pack,std::string* error,bool shared){
    if(error)error->clear();
    if(!path){fail(error,"null GAME UI pack path");return false;}
    std::FILE* file=std::fopen(path,"rb");if(!file){fail(error,"cannot open GAME UI pack");return false;}
    if(std::fseek(file,0,SEEK_END)!=0){std::fclose(file);fail(error,"cannot size GAME UI pack");return false;}
    const auto length=std::ftell(file);std::rewind(file);
    if(length<0||static_cast<std::uint64_t>(length)>MaxBytes){std::fclose(file);fail(error,"GAME UI pack outside bounds");return false;}
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    const auto read=std::fread(bytes.data(),1,bytes.size(),file);const int extra=std::fgetc(file);std::fclose(file);
    if(read!=bytes.size()||extra!=EOF){fail(error,"GAME UI pack read mismatch");return false;}
    return parse_ui_pack(bytes.data(),bytes.size(),pack,error,shared);
}
bool load_game_ui_pack_file(const char* p,GameUiPack& pack,std::string* error){return load_ui_pack_file(p,pack,error,false);}
bool load_shared_ui_pack_file(const char* p,GameUiPack& pack,std::string* error){return load_ui_pack_file(p,pack,error,true);}
}

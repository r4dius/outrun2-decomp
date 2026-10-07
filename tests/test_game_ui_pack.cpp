#include "platform/game_ui_pack.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
using namespace outrun::platform;
namespace {
unsigned checks{};
void require(bool ok,const char* message){++checks;if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
void put32(std::vector<std::uint8_t>& b,std::size_t offset,std::uint32_t value){
    for(unsigned i=0;i<4u;++i)b[offset+i]=std::uint8_t(value>>(i*8u));
}
void put16(std::vector<std::uint8_t>& b,std::size_t offset,std::uint16_t value){
    b[offset]=std::uint8_t(value);b[offset+1u]=std::uint8_t(value>>8u);
}
void putf(std::vector<std::uint8_t>& b,std::size_t offset,float value){
    std::uint32_t bits{};std::memcpy(&bits,&value,sizeof(bits));put32(b,offset,bits);
}
std::uint32_t crc32(const std::uint8_t* data,std::size_t size){
    std::uint32_t crc=0xffffffffu;
    for(std::size_t i=0;i<size;++i){crc^=data[i];for(unsigned bit=0;bit<8u;++bit)
        crc=(crc>>1u)^(0xedb88320u&std::uint32_t(0u-(crc&1u)));}
    return ~crc;
}
std::vector<std::uint8_t> synthetic(){
    constexpr std::size_t records=64u,animation=64u+7u*24u,animation_size=516u;
    constexpr std::size_t payload=animation+animation_size;
    std::vector<std::uint8_t> b(payload+7u*8u);
    std::memcpy(b.data(),"OR2GUI1",7u);
    put32(b,8u,1u);put32(b,12u,64u);put32(b,16u,7u);put32(b,20u,108u);
    put32(b,24u,animation);put32(b,28u,animation_size);put32(b,32u,records);
    put32(b,36u,payload);put32(b,40u,std::uint32_t(b.size()));
    put32(b,44u,1024u);put32(b,48u,animation_size+4u);put32(b,52u,24u);
    for(std::size_t i=0;i<7u;++i){const auto record=records+i*24u;
        put32(b,record,std::uint32_t(i));put32(b,record+4u,4u);
        put32(b,record+8u,4u);put32(b,record+12u,1u);
        put32(b,record+16u,std::uint32_t(payload+i*8u));put32(b,record+20u,8u);
    }
    for(std::size_t i=0;i<108u;++i)put32(b,animation+i*4u,436u);
    put32(b,animation,436u);put32(b,animation+436u,1u);
    put32(b,animation+440u,456u);put32(b,animation+444u,1u);
    put32(b,animation+448u,492u);put32(b,animation+452u,(480u<<16u)|640u);
    put32(b,60u,crc32(b.data()+64u,b.size()-64u));return b;
}
}
int main(int argc,char** argv){
    constexpr std::size_t animation=64u+7u*24u;
    auto bytes=synthetic();GameUiPack pack{};std::string error;
    require(parse_game_ui_pack(bytes.data(),bytes.size(),pack,&error),"valid GAME pack parses");
    require(pack.scene_count==108u&&pack.textures.size()==7u&&pack.animation.size()==516u,"all UI resources retained");
    GameUiScene scene{};require(game_ui_scene(pack,0u,scene)&&scene.width==640u&&scene.height==480u,"scene zero decoded");
    require(game_ui_scene(pack,107u,scene)&&!game_ui_scene(pack,108u,scene),"scene table bounded");
    std::vector<GameUiDraw> empty_draws;
    require(game_ui_scene_base_draws(pack,0u,empty_draws,&error)&&empty_draws.empty(),
            "empty synthetic component resolves without fabricated sprites");
    // One authored layer and a deliberately curved two-knot scale track:
    // the encoded tangents produce 81.25% at frame five, not linear 75%.
    auto curved=pack;
    curved.animation.resize(672u);
    auto& ani=curved.animation;
    put32(ani,456u+28u,1u);put32(ani,456u+32u,516u);
    put32(ani,492u,4u);put32(ani,492u+4u,4u);
    put32(ani,492u+12u,1u);put32(ani,492u+16u,600u);
    put32(ani,516u,(10u<<16u));put32(ani,516u+8u,492u);
    putf(ani,516u+32u,-0.0013270393f);
    putf(ani,516u+36u,100.0f);putf(ani,516u+44u,100.0f);
    put32(ani,516u+56u,640u);
    put32(ani,600u,0u);
    putf(ani,604u,0.0f);putf(ani,608u,0.0f);
    putf(ani,612u,4.0f);putf(ani,616u,4.0f);
    put32(ani,640u,2u);put16(ani,644u,0x0b07u);
    put16(ani,646u,0u);put16(ani,648u,0x6400u);
    put16(ani,650u,0u);put16(ani,652u,0u);
    put16(ani,654u,10u);put16(ani,656u,0x3200u);
    put16(ani,658u,0xb000u);put16(ani,660u,0u);
    std::vector<GameUiDraw> curved_draws;
    require(game_ui_scene_frame_draws(curved,0u,5.0f,curved_draws,&error)&&
            curved_draws.size()==1u,"curved synthetic GAME layer resolves");
    require(std::fabs(curved_draws[0].corners[1][0]-3.25f)<0.001f,
            "GAME tangent-scaled cubic evaluation differs from linear interpolation");
    put16(ani,644u,0x0607u); // retail SUMO_FE includes a negative exponent difference
    put16(ani,658u,0xff80u); // -0.5 * 2 = -1 incoming slope
    require(game_ui_scene_frame_draws(curved,0u,5.0f,curved_draws,&error)&&
            std::fabs(curved_draws[0].corners[1][0]-3.05f)<0.001f,
            "negative tangent exponent scales instead of rejecting retail track");
    put16(ani,644u,0xff07u);
    require(!game_ui_scene_frame_draws(curved,0u,5.0f,curved_draws,&error),
            "unbounded tangent exponent rejected");
    require(!game_ui_scene_base_draws(pack,108u,empty_draws,&error),
            "sprite traversal rejects scene index past 108");
    auto bad=bytes;bad[0]='X';require(!parse_game_ui_pack(bad.data(),bad.size(),pack,&error),"bad magic rejected");
    bad=bytes;bad.back()^=1u;require(!parse_game_ui_pack(bad.data(),bad.size(),pack,&error),"payload CRC enforced");
    bad=bytes;put32(bad,animation+0u,0xffffffffu);put32(bad,60u,crc32(bad.data()+64u,bad.size()-64u));
    require(!parse_game_ui_pack(bad.data(),bad.size(),pack,&error),"bad scene pointer rejected");
    if(argc==2){GameUiPack real{};
        require(load_game_ui_pack_file(argv[1],real,&error),"retail GAME pack loads");
        require(real.scene_count==108u&&real.textures.size()==7u,"retail GAME inventory");
        require(game_ui_scene(real,78u,scene)&&scene.width==640u&&scene.height==480u,"retail scene 78");
        std::size_t leaf_draws=0u,keyframed_draws=0u;
        for(std::uint32_t index=0;index<real.scene_count;++index){
            std::vector<GameUiDraw> draws;
            const bool resolved=game_ui_scene_base_draws(real,index,draws,&error);
            if(!resolved)std::fprintf(stderr,"scene %u: %s\n",index,error.c_str());
            require(resolved,"retail GAME component/layer/footage graph resolves");
            leaf_draws+=draws.size();
            for(const auto& draw:draws){
                require(draw.texture<real.textures.size(),"resolved atlas index bounded");
                if(draw.has_keyframes)++keyframed_draws;
            }
        }
        require(leaf_draws>300u&&keyframed_draws>100u,
                "retail GAME scene set contains visible and animated layers");
        std::size_t changed_scenes=0u,visible_zero=0u,visible_sixty=0u;
        for(std::uint32_t index=0;index<real.scene_count;++index){
            std::vector<GameUiDraw> first,later;
            require(game_ui_scene_frame_draws(real,index,0.0f,first,&error),
                    "retail GAME scene evaluates frame zero");
            require(game_ui_scene_frame_draws(real,index,60.0f,later,&error),
                    "retail GAME scene evaluates frame sixty");
            require(first.size()==later.size(),"animation preserves authored GPU draw count");
            bool changed=false;
            for(std::size_t i=0;i<first.size();++i){
                visible_zero+=first[i].visible?1u:0u;
                visible_sixty+=later[i].visible?1u:0u;
                require(first[i].texture==later[i].texture&&first[i].crop==later[i].crop,
                        "animation preserves atlas command identity");
                if(first[i].visible!=later[i].visible||
                   std::fabs(first[i].opacity-later[i].opacity)>0.001f||
                   std::fabs(first[i].corners[0][0]-later[i].corners[0][0])>0.001f||
                   std::fabs(first[i].corners[0][1]-later[i].corners[0][1])>0.001f)
                    changed=true;
            }
            if(changed)++changed_scenes;
        }
        require(changed_scenes>10u,"retail GAME animation advances multiple authored scenes");
        require(visible_zero>200u&&visible_sixty>200u,
                "retail GAME animation keeps authored sprites visible");
        std::vector<GameUiDraw> invalid_frame;
        require(!game_ui_scene_frame_draws(real,0u,NAN,invalid_frame,&error),
                "nonfinite animation frame rejected");
        std::vector<GameUiDraw> hud_draws;
        require(game_ui_scene_base_draws(real,78u,hud_draws,&error),
                "retail GAME scene 78 resolves for GPU composition");
        std::size_t visible_at_zero=0u,on_screen=0u;
        for(const auto& draw:hud_draws){
            if(draw.first_frame>0||draw.last_frame<0)continue;
            ++visible_at_zero;
            float min_x=draw.corners[0][0],max_x=min_x;
            float min_y=draw.corners[0][1],max_y=min_y;
            for(const auto& corner:draw.corners){
                if(corner[0]<min_x)min_x=corner[0];
                if(corner[0]>max_x)max_x=corner[0];
                if(corner[1]<min_y)min_y=corner[1];
                if(corner[1]>max_y)max_y=corner[1];
            }
            if(max_x>0.0f&&min_x<640.0f&&max_y>0.0f&&min_y<480.0f&&
               (max_x-min_x)*(max_y-min_y)>1.0f)++on_screen;
        }
        require(visible_at_zero>=3u,"GAME scene 78 has visible frame-zero layers");
        require(on_screen>=2u,"GAME scene 78 has nondegenerate frame-zero artwork");
        std::printf("retail GAME base draws=%zu keyframed=%zu\n",leaf_draws,keyframed_draws);
        std::printf("retail GAME scenes changed by frame 60=%zu\n",changed_scenes);
        std::printf("retail GAME visible draws frame 0/60=%zu/%zu\n",visible_zero,visible_sixty);
        std::printf("retail GAME scene 78 frame-zero draws=%zu\n",visible_at_zero);
    }
    std::printf("game_ui_pack: %u checks passed\n",checks);return 0;
}

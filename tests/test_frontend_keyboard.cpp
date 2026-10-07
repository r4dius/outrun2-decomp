#include "platform/frontend_keyboard.hpp"
#include <cstdio>
#include <cstdlib>
using namespace outrun::platform;
using outrun::driving::Bytes;
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"%d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
int main(int argc,char** argv){
    CHECK(argc==2);GameUiPack pack;std::string error;
    CHECK(!load_game_ui_pack_file(argv[1],pack,&error));
    if(!load_shared_ui_pack_file(argv[1],pack,&error)){std::fprintf(stderr,"%s\n",error.c_str());return 1;}
    CHECK(pack.scene_count==319&&pack.textures.size()==11);
    FrontendSprites pool;CHECK(pool.bind(0x2c,pack));FrontendUiResources ui{pool};
    struct Context{int input=-1;std::vector<unsigned> sounds;bool focus=false;} ctx;
    FrontendKeyboardServices services{&ctx,
        [](void* p,Bytes,int& input){input=static_cast<Context*>(p)->input;return true;},
        [](void* p,unsigned sound){static_cast<Context*>(p)->sounds.push_back(sound);return true;},
        [](void* p,bool f){static_cast<Context*>(p)->focus=f;return true;}};
    std::array<std::uint8_t,PcKeyboardBytes> data;data.fill(0xa5);Bytes b(data.data(),data.size());
    unsigned repeat{},result{};CHECK(keyboard_construct_468e40(b,repeat));CHECK(repeat==12);CHECK(b.u32(0x700)==0xa5a5a5a5);
    CHECK(keyboard_init_468880(b,ui,services));CHECK(ctx.focus&&services.selected==0);CHECK(b.u32(0xec)==1);
    keyboard_limits_468780(b,1,16);CHECK(keyboard_name_468710(b,"OR2C2C"));
    CHECK(keyboard_position_4687c0(b,ui,0,60));
    CHECK(keyboard_tick_469130(b,ui,services,result));CHECK(result==0&&pool.used(12)==1);
    for(unsigned i=0;i<60;++i)pool.tick();
    CHECK(keyboard_tick_469130(b,ui,services,result));CHECK(pool.used(12)==2);
    // Every cursor position resolves a real authored ETC animation.
    for(unsigned flags:{0u,2u})for(unsigned cell=0;cell<44;++cell)for(unsigned dir=0;dir<4;++dir){
        b.put32(0x708,flags);services.selected=cell;CHECK(keyboard_move_468c00(b,ui,services,dir));
        CHECK(services.selected<44);const auto* sprite=pool.get(b.u32(0x55c));CHECK(sprite);
        std::vector<GameUiDraw> draws;CHECK(game_ui_scene_frame_draws(pack,sprite->token&0xffff,0,draws,&error));
    }
    b.put32(0x708,0);ctx.input=0;
    for(unsigned alphabet=1;alphabet<=5;++alphabet){
        b.put32(0xec,alphabet);services.selected=10;CHECK(keyboard_name_468710(b,""));
        const auto ch=keyboard_character_468d40(b,10);CHECK(keyboard_tick_469130(b,ui,services,result));
        CHECK(b.u8(0x5f4)==ch);CHECK(b.u32(0xec)==(alphabet==3?1:alphabet));
    }
    services.selected=38;CHECK(keyboard_tick_469130(b,ui,services,result));CHECK(keyboard_name_468770(b).empty());
    CHECK(keyboard_tick_469130(b,ui,services,result));CHECK(ctx.sounds.back()==3);
    for(unsigned cell=39;cell<43;++cell){
        services.selected=cell;b.put32(0xec,1);b.put32(0x708,1u<<(cell-38));
        CHECK(keyboard_tick_469130(b,ui,services,result));CHECK(b.u32(0xec)==1);
        b.put32(0x708,0);CHECK(keyboard_tick_469130(b,ui,services,result));CHECK(b.u32(0xec)==cell-37);
        CHECK(keyboard_tick_469130(b,ui,services,result));CHECK(b.u32(0xec)==1);
    }
    services.selected=0;for(unsigned i=0;i<25;++i)CHECK(keyboard_tick_469130(b,ui,services,result));
    CHECK(keyboard_name_468770(b).size()==15);CHECK(ctx.sounds.back()==3);
    services.selected=43;CHECK(keyboard_tick_469130(b,ui,services,result));CHECK(result==0&&b.u8(0x6fc));
    ctx.input=-1;CHECK(keyboard_tick_469130(b,ui,services,result));CHECK(result==0);
    for(unsigned i=0;i<60;++i)pool.tick();
    CHECK(keyboard_tick_469130(b,ui,services,result));CHECK(result==1&&!ctx.focus&&pool.used(12)==0);
    CHECK(keyboard_init_468880(b,ui,services));keyboard_limits_468780(b,1,16);
    CHECK(keyboard_finish_469030(b,ui,services,true));CHECK(!b.u8(0x6fc));
    CHECK(keyboard_finish_469030(b,ui,services,false));for(unsigned i=0;i<60;++i)pool.tick();
    CHECK(keyboard_tick_469130(b,ui,services,result));CHECK(result==2&&!ctx.focus);
    CHECK(!keyboard_name_468710(b,std::string(257,'x')));
    FrontendSprites absent;FrontendUiResources missing_ui{absent};
    CHECK(keyboard_construct_468e40(b,repeat));
    CHECK(!keyboard_init_468880(b,missing_ui,services));CHECK(!ctx.focus);
    std::puts("original keyboard: retail ETC animations, alphabets, navigation, limit, commit/cancel and delayed release pass");
}

#include "platform/title_owner.hpp"
#include "driving/pc_driving.hpp"
#include "driving/pc_x87.hpp"

#include <array>
#include <cmath>
#include <cstring>

namespace outrun::platform {
namespace {
std::uint32_t get32(const std::uint8_t* object,std::size_t offset){
    std::uint32_t value{};
    std::memcpy(&value,object+offset,sizeof(value));
    return value;
}
void put32(std::uint8_t* object,std::size_t offset,std::uint32_t value){
    std::memcpy(object+offset,&value,sizeof(value));
}
}

bool title_base_construct_48f480(std::uint8_t* object,std::size_t size,
                                 std::uint32_t& global_6591e4){
    if(!object||size<0x34u)return false;
    put32(object,0u,0x5c18c8u);
    for(std::size_t i=0;i<4u;++i){
        std::memset(object+0x0cu+i*2u,0,2u);
        std::memset(object+0x14u+i*2u,0,2u);
        put32(object,0x1cu+i*4u,0u);
        object[0x2cu+i*2u]=0u;
        object[0x2du+i*2u]=0u;
    }
    global_6591e4=0x0cu;
    return true;
}

bool title_ui_resource_construct_465160(std::uint8_t* object,std::size_t size){
    if(!object||size<0xa0u)return false;
    for(const std::size_t off:{0x08u,0x0cu,0x10u})put32(object,off,0xffffffffu);
    for(const std::size_t off:{0x40u,0x44u,0x48u,0x4cu,0x50u,0x54u})
        put32(object,off,0u);
    for(const std::size_t off:{0x64u,0x68u,0x6cu,0x70u,0x74u,0x78u})
        put32(object,off,0x3f800000u);
    for(const std::size_t off:{0x00u,0x04u,0x1cu,0x20u,0x24u,0x28u,
                               0x88u,0x8cu,0x90u,0x94u,0x98u,0x9cu})
        put32(object,off,0u);
    put32(object,0x18u,1u);
    return true;
}

bool frontend_input_axes_48f4f0(std::uint8_t* object,std::size_t size,
                               const FrontendInputSnapshot& input){
    if(!object||size<0x34)return false;
    std::memcpy(object+0x14,object+0x0c,8);
    std::memcpy(object+0x0c,input.axes.data(),8);
    for(std::size_t i=0;i<4;++i){
        std::int32_t counter;std::memcpy(&counter,object+0x1c+4*i,4);
        const auto axis=std::int32_t(input.axes[i]);
        object[0x2c+2*i]=object[0x2d+2*i]=0;
        if(counter>=0)counter=(std::abs(axis)<=64)?-1:counter-1;
        else if(std::abs(axis)>64){object[0x2c+2*i+(axis<0)]=1;counter=20;}
        std::memcpy(object+0x1c+4*i,&counter,4);
    }
    return true;
}

bool frontend_input_action_48f5f0(std::uint8_t* object,std::size_t size,
                                 const FrontendInputSnapshot& input,std::int32_t argument,
                                 std::uint32_t& previous,void* user,
                                 FrontendInputFeedback feedback,std::uint32_t& result){
    if(!frontend_input_axes_48f4f0(object,size,input))return false;
    const auto keys=input.feature_mask;
    result=12;
    if(keys&5)result=0;
    else if(keys&8)result=1;
    else if((keys&0x400)||object[0x2f])result=2;
    else if((keys&0x1000)||object[0x2d])result=3;
    else if((keys&0x800)||object[0x2e])result=4;
    else if((keys&0x2000)||object[0x2c])result=5;
    else if(input.device_held&0x1000)result=10;
    else if(input.device_held&0x2000)result=11;
    else if(keys&0x10)result=6;
    else if(keys&0x20)result=7;
    else if(keys&0x8000)result=8;
    else if(keys&0x4000)result=9;
    const auto old=previous;previous=result;
    // The original protected comparison is action == shared 6591E4.
    // Even -1 is forwarded to 440ED0 when the action changes.
    constexpr std::uint32_t feedback_keys[]{4,8,0x400,0x1000,0x800,0x2000,0x10,0x20,0x8000,0x4000};
    return result>=10||result==old||(feedback&&feedback(user,feedback_keys[result],argument));
}

bool title_list_construct_4ed950(std::uint8_t* object,std::size_t size){
    if(!object||size<0x3cu)return false;
    for(const std::size_t off:{0x04u,0x08u,0x0cu,0x18u,0x20u,0x24u,
                               0x2cu,0x34u,0x38u})put32(object,off,0u);
    put32(object,0u,0xffffffffu);
    object[0x10u]=1u;
    for(const std::size_t off:{0x11u,0x12u,0x13u,0x14u,0x15u,0x30u})
        object[off]=0u;
    put32(object,0x1cu,0x41800000u);
    put32(object,0x28u,0x0cu);
    return true;
}

bool title_transform_construct_48e310(std::uint8_t* object,std::size_t size){
    if(!object||size<0x478u)return false;
    // Inline 0x48D570 first, with its this pointer at object + 0x30.
    object[0x30u]=0u;
    object[0x44cu]=1u;
    put32(object,0x430u,9u);
    put32(object,0x434u,4u);
    put32(object,0x438u,0x400u);
    put32(object,0x450u,0xff3f474au);
    for(const auto off:{0x454u,0x458u,0x45cu,0x460u})
        put32(object,off,0x3f800000u);
    put32(object,0x440u,0u);
    put32(object,0x448u,0x280u);
    put32(object,0x43cu,0u);
    put32(object,0x444u,0x1e0u);
    put32(object,0x464u,1u);

    for(const auto off:{0x04u,0x08u,0x0cu,0x24u,0x28u,
                        0x468u,0x470u})put32(object,off,0u);
    for(const auto off:{0u,0x10u,0x14u})put32(object,off,0xffffffffu);
    for(const auto off:{0x18u,0x1au,0x1bu,0x1cu,0x1du,0x46cu})
        object[off]=0u;
    object[0x19u]=1u;
    put32(object,0x20u,0x41800000u);
    put32(object,0x2cu,0x0cu);
    put32(object,0x474u,0x60u);
    return true;
}

bool title_widget_construct_48e590(std::uint8_t* object,std::size_t size,
                                   std::uint32_t& global_6591e4){
    if(!object||size<0x48cu)return false;
    if(!title_base_construct_48f480(object,size,global_6591e4))return false;
    put32(object,0u,0x5c1888u); // 0x48E4D0 base
    for(const auto off:{0x34u,0x38u,0x3cu,0x40u})put32(object,off,0u);
    object[0x4du]=0u;
    object[0x4eu]=0u;
    for(const auto off:{0x458u,0x460u,0x464u,0x468u,0x46cu,0x488u})
        put32(object,off,0u);
    put32(object,0u,0x5c18a0u);
    put32(object,0x450u,9u);
    put32(object,0x454u,13u);
    put32(object,0x45cu,0x400u);
    object[0x470u]=1u;
    put32(object,0x474u,0xffddddddu);
    put32(object,0x478u,0x3f800000u);
    put32(object,0x47cu,0x3f800000u);
    put32(object,0x480u,1u);
    object[0x4cu]=1u;
    put32(object,0x484u,14u);
    return true;
}

bool title_controller_construct_48c490(std::uint8_t* object,std::size_t size,
                                       std::uint32_t& global_6591e4){
    if(!object||size<0x12a6u)return false;
    if(!title_base_construct_48f480(object,size,global_6591e4))return false;
    put32(object,0u,0x5c1838u);
    for(const auto off:{0x48u,0x4d4u,0x960u,0xdecu})
        if(!title_widget_construct_48e590(object+off,size-off,global_6591e4))
            return false;
    object[0x12a5u]=0u;
    put32(object,0x08u,0x2du);
    return true;
}

bool title_controller_init_48c5b0(std::uint8_t* object,std::size_t size){
    if(!object||size<0x12a6u)return false;
    std::memset(object+0x34u,0,6u);
    put32(object,0x3cu,0u);
    for(const auto off:{0x41u,0x42u,0x43u,0x44u,0x12a4u,0x12a5u})
        object[off]=0u;
    put32(object,0x1278u,0u);
    put32(object,0x127cu,0u);
    object[0x40u]=1u;
    return true;
}

bool title_controller_move_48cb00(std::uint8_t* object,std::size_t size,float x,float y,float duration){
    if(!object||size<0x12a5||!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(duration))return false;
    driving::Bytes b(object,size);b.put8(0x12a4,1);b.putf(0x12a0,0);b.putf(0x1294,0);b.putf(0x128c,x);b.putf(0x1290,y);
    if(duration==0){b.putf(0x1280,x);b.putf(0x1284,y);b.putf(0x1288,0);b.putf(0x1298,0);b.putf(0x129c,0);}
    else {const float inverse=1.f/duration;b.putf(0x1298,(x-b.f32(0x1280))*inverse);b.putf(0x129c,(y-b.f32(0x1284))*inverse);}
    return true;
}
bool title_controller_rect_48d4e0(std::uint8_t* object,std::size_t size,float x,float y,float width,float height){
    if(!object||size<0x12a5||!std::isfinite(width)||!std::isfinite(height)||
       double(width)>=2147483648.||double(height)>=2147483648.||double(height)<-2147483648.)return false;
    if(!title_controller_move_48cb00(object,size,x,y,0))return false;
    driving::Bytes b(object,size);b.put16(0x36,std::uint16_t(int(width<116.f?116.f:width)));b.put16(0x38,std::uint16_t(int(height)));return true;
}
bool title_controller_motion_48cc00(std::uint8_t* object,std::size_t size,float delta){
    if(!object||size<0x12a5||!std::isfinite(delta))return false;
    driving::Bytes b(object,size);
    if(b.u8(0x12a4)){
        bool reached[2]{b.f32(0x1298)==0,b.f32(0x129c)==0};
        for(unsigned axis=0;axis<3;++axis){const auto off=axis*4;
            // PC stores 40F050's x87 multiply into a float before 40EF10 adds.
            // Both fmul and fadd round to the x87 precision control.
            const float step=driving::x87_float(driving::X87(delta)*driving::X87(b.f32(0x1298+off)));
            b.putf(0x1280+off,driving::x87_float(driving::X87(b.f32(0x1280+off))+driving::X87(step)));
        }
        for(unsigned axis=0;axis<2;++axis){const auto off=axis*4;const float v=b.f32(0x1298+off),p=b.f32(0x1280+off),t=b.f32(0x128c+off);
            if((v<0&&t>=p)||(v>0&&p>=t))reached[axis]=true;
        }
        // Neither axis snaps early. The original snaps both only when both
        // have reached their target, including a stationary axis.
        if(reached[0]&&reached[1]){for(unsigned axis=0;axis<3;++axis)b.put32(0x1280+axis*4,b.u32(0x128c+axis*4));b.put8(0x12a4,0);}
    }
    const float x=b.f32(0x1280),y=b.f32(0x1284),w=float(b.i16(0x36)),h=float(b.i16(0x38));
    b.putf(0x48+0x34,x+6);b.putf(0x48+0x38,y+5);
    b.putf(0x4d4+0x34,x+14);b.putf(0x4d4+0x38,y+38);
    b.putf(0x960+0x34,w+x-45);b.putf(0x960+0x38,h+y-38);
    b.putf(0xdec+0x34,x+45);b.putf(0xdec+0x38,h+y-38);
    return true;
}

bool title_controller_tick_48d420(std::uint8_t* object,std::size_t size,
                                  const TitleControllerServices& services,
                                  std::uint32_t& result){
    if(!object||size<0x12a6u||!services.call)return false;
    result=0u;
    if(object[0x34u]==0u)return true;
    const auto call=[&](std::uint32_t pc,std::size_t offset,
                        std::int32_t argument,std::uint32_t& value){
        return services.call(services.user,pc,object,offset,argument,value);
    };
    std::uint32_t ignored{};
    if(!call(0x48cc00u,0u,0,ignored))return false;
    std::uint32_t action=12u;
    if(object[0x40u]==0u){
        const bool feedback_enabled=services.root_state==2u&&object[0x42u]!=1u&&
                          object[0x41u]!=1u&&object[0x12a5u]==0u;
        if(!call(0x48f5f0u,0u,feedback_enabled?1:-1,action))return false;
    }
    object[0x40u]=0u;
    if(action==0u&&object[0x42u]!=0u){
        object[0x44u]=1u;
        if(!call(0x4249f0u,0u,0x40,ignored))return false;
        result=1u;
        return true;
    }
    if(action==1u&&object[0x41u]!=0u){
        object[0x44u]=0u;
        if(!call(0x4249f0u,0u,0,ignored))return false;
        result=2u;
        return true;
    }
    if(!call(0x47f110u,0x48u,0,ignored))return false;
    if(!call(0x47f110u,0x4d4u,0,ignored))return false;
    return true;
}

TitleOwnerInitialUiState4d5e40 title_menu_records(const TitleMenuGlobals& globals){
    TitleOwnerInitialUiState4d5e40 state;state.root_state=globals.root_state;
    if(globals.root_state==3){
        state.entries[state.entry_count++]=8;
        if(globals.variant!=4)state.entries[state.entry_count++]=7;
        else if(globals.manager_present&&globals.manager_child_key!=7)state.entries[state.entry_count++]=9;
        state.entries[state.entry_count++]=6;
    }
    for(unsigned entry:{10u,11u,13u})state.entries[state.entry_count++]=entry;
    if(globals.root_state==2)state.entries[state.entry_count++]=14;
    return state;
}
bool title_owner_initial_ui_4d5e40(std::uint8_t* object,std::size_t size,
                                   std::uint32_t root_state,
                                   TitleOwnerInitialUiState4d5e40& state,
                                   std::uint32_t variant,bool manager_present,
                                   std::uint32_t manager_child_key){
    if(!object||size<TitleOwnerPcSize)return false;
    // This is the stage-0 body selected by 0x4D8C50. Refuse to manufacture a
    // later-stage transition when a caller enters it out of sequence.
    if(get32(object,0x9acu)!=0u)return false;

    state=title_menu_records({root_state,variant,manager_present,manager_child_key});

    // 0x48C5B0 leaves the child disabled after initialization. 0x4D5E40 is
    // the first owner body that makes the controller live before advancing
    // the title stage. Keep only this observed parent-owned activation here;
    // widget/list helper internals remain explicit future work.
    object[0x9bcu+0x34u]=1u;
    put32(object,0x9acu,1u);
    return true;
}

bool title_menu_control_4d7300(std::uint8_t* object,std::size_t size,TitleMenuGlobals& globals,
                              const TitleControllerServices& services,std::uint32_t& result){
    result=0;if(!object||size<TitleOwnerPcSize||!services.call)return false;
    unsigned ignored{};
    if(!services.call(services.user,0x48dda0,object,0x34,0,ignored))return false;
    std::int32_t selected;std::memcpy(&selected,object+0x34,4);
    if(globals.root_state==3&&globals.variant==4&&selected>=1&&
       globals.manager_present&&globals.manager_child_key==7)++selected;
    if(globals.delay_692c9c>0)globals.delay_692c9c-=1.f;
    if(!(globals.delay_692c9c<=0))return true; // unordered COMISS also waits
    unsigned action{};if(!services.call(services.user,0x48f5f0,object,0,1,action))return false;
    if(action==0){
        if(globals.feature_mask&1){put32(object,0x9ac,get32(object,0x9ac)+1);put32(object,0x9b0,2);return true;}
        constexpr unsigned pause_targets[]{~0u,12,15,3,6,9};
        constexpr unsigned menu_targets[]{3,6,9,18};
        const unsigned count=globals.root_state==2?4:6;
        if(selected<0||unsigned(selected)>=count)return false;
        const auto target=globals.root_state==2?menu_targets[selected]:pause_targets[selected];
        if(target!=~0u)put32(object,0x9b4,target);
        put32(object,0x9ac,get32(object,0x9ac)+1);put32(object,0x9b0,target==~0u?2:1);
    }else if(action==1){
        put32(object,0x9b4,18);put32(object,0x9ac,get32(object,0x9ac)+1);put32(object,0x9b0,1);
    }else if(action==2||action==4){
        if(!services.call(services.user,action==2?0x48dc10:0x48dc60,object,0x34,1,ignored))return false;
    }
    return true;
}

bool title_menu_close_4d6090(std::uint8_t* object,std::size_t size,const TitleControllerServices& services){
    if(!object||size<TitleOwnerPcSize||!services.call)return false;unsigned ignored{};
    if(!services.call(services.user,0x48e440,object,0x34,0,ignored)||
       !services.call(services.user,16,object,0x9bc,0,ignored))return false;
    object[0x9b8]=1;return true;
}

namespace {
struct FullTitleConstructor {
    std::uint8_t* base;
    std::size_t size;
    std::uint32_t* global;
    bool ok{true};
    static void child(void* user,std::uint32_t pc,std::uint8_t* ptr){
        auto& ctx=*static_cast<FullTitleConstructor*>(user);
        const auto offset=std::size_t(ptr-ctx.base);
        if(offset>=ctx.size){ctx.ok=false;return;}
        const auto remaining=ctx.size-offset;
        switch(pc){
        case 0x48f480u: ctx.ok=title_base_construct_48f480(ptr,remaining,*ctx.global)&&ctx.ok;break;
        case 0x48e310u: ctx.ok=title_transform_construct_48e310(ptr,remaining)&&ctx.ok;break;
        case 0x465160u: ctx.ok=title_ui_resource_construct_465160(ptr,remaining)&&ctx.ok;break;
        case 0x48c490u: ctx.ok=title_controller_construct_48c490(ptr,remaining,*ctx.global)&&ctx.ok;break;
        case 0x4ed950u: ctx.ok=title_list_construct_4ed950(ptr,remaining)&&ctx.ok;break;
        default: ctx.ok=false;break;
        }
    }
    static void array(void* user,std::uint32_t pc,std::uint8_t* ptr,
                      std::uint32_t elem,std::uint32_t count,
                      std::uint32_t ctor,std::uint32_t dtor){
        auto& ctx=*static_cast<FullTitleConstructor*>(user);
        ctx.ok=(pc==0x5816bdu&&ptr==ctx.base+0x4acu&&elem==0x8cu&&
                count==4u&&ctor==0x570ac0u&&dtor==0x49a650u)&&ctx.ok;
        // The original 0x570AC0 element constructor returns this unchanged.
    }
};
}

bool title_owner_construct_complete_4d7140(std::uint8_t* object,std::size_t size,
                                            std::uint8_t& game_state_780270,
                                            std::uint8_t& title_flag_95b250,
                                            std::uint32_t& global_6591e4){
    if(!object||size<TitleOwnerPcSize)return false;
    FullTitleConstructor ctx{object,size,&global_6591e4};
    return title_owner_construct_4d7140(object,size,game_state_780270,
                                        title_flag_95b250,
                                        {&ctx,FullTitleConstructor::child,
                                         FullTitleConstructor::array})&&ctx.ok;
}

bool title_owner_construct_4d7140(std::uint8_t* object,std::size_t size,
                                  std::uint8_t& game_state_780270,
                                  std::uint8_t& title_flag_95b250,
                                  const TitleOwnerConstructorServices& services){
    if(!object||size<TitleOwnerPcSize||!services.child||!services.array)
        return false;
    const auto child=[&](std::uint32_t pc,std::size_t offset){
        services.child(services.user,pc,object+offset);
    };
    child(0x48f480u,0u);
    put32(object,0u,0x5ccbb4u);
    child(0x48e310u,0x34u);
    services.array(services.user,0x5816bdu,object+0x4acu,0x8cu,4u,
                   0x570ac0u,0x49a650u);
    child(0x465160u,0x740u);
    child(0x465160u,0x800u);
    child(0x465160u,0x8c0u);
    child(0x48c490u,0x9bcu);
    child(0x4ed950u,0x1c6cu);
    child(0x465160u,0x1cacu);
    put32(object,0x08u,0x2cu);
    put32(object,0x9acu,0u);
    game_state_780270=1u; // 0x43F9E0(1)
    title_flag_95b250=1u; // 0x42F320(1)
    return true;
}

bool title_owner_init_4d5d00(std::uint8_t* object,std::size_t size,
                              std::uint32_t singleton_state,std::uint32_t mode,
                              TitleOwnerGlobals& globals,
                              const TitleOwnerServices& services){
    if(!object||size<TitleOwnerPcSize||!services.child_init||
       (singleton_state==3u&&mode!=4u&&!services.pause))return false;
    // 0x48E440 releases the list at +0x34. Do not drop live PC-list nodes
    // until their ownership/destructors have been ported.
    if(get32(object,0x3cu)!=0u)return false;
    put32(object,0x9acu,0u);
    put32(object,0x9b4u,0u);
    put32(object,0x3cu,0u);
    put32(object,0x9b0u,0u);
    object[0x9b8u]=0u;
    const std::uint32_t base=singleton_state==2u?8u:14u;
    for(std::uint32_t i=0u;i<3u;++i)globals.scene_ids[i]=base+i;
    services.child_init(services.user);
    if(singleton_state==3u&&mode!=4u){
        services.pause(services.user);
        globals.pause_flag=1u;
    }
    return true;
}

std::uint32_t title_owner_control_target_4d7260(std::uint32_t owner_state){
    // 0x4D72E0: 22 state bytes, each selecting one of eight jump entries.
    constexpr std::array<std::uint8_t,22> selector{{
        0,7,7,1,7,7,2,7,7,3,7,7,4,7,7,5,7,7,0,7,7,6}};
    constexpr std::array<std::uint32_t,8> target{{
        0u,0x4d60c0u,0x4d62a0u,0x4d6370u,
        0x4d6510u,0x4d6510u,0x4d6a60u,0u}};
    if(owner_state==0u||owner_state>selector.size())return 0u;
    return target[selector[owner_state-1u]];
}

bool title_owner_tick_4d8c50(std::uint8_t* object,std::size_t size,
                             const TitleOwnerTickServices& services,
                             std::uint32_t& result){
    if(!object||size<TitleOwnerPcSize||!services.call)return false;
    const auto call=[&](std::uint32_t pc,std::size_t offset,
                        std::uint32_t& value){
        return services.call(services.user,pc,object,offset,value);
    };
    result=0u;
    if(object[0x9b8u]!=0u){
        const auto action=get32(object,0x9b0u);
        put32(object,0x9b0u,0u);
        if(action==2u||action==4u){
            if(services.root_state==3u){
                std::uint32_t active{};
                if(!call(0x55a930u,0u,active))return false;
                if(active!=0u){
                    std::uint32_t ignored{};
                    if(!call(0x46c270u,0u,ignored))return false;
                }
            }
            put32(object,0x9b0u,1u);
            result=action;
            return true;
        }
        if(action==1u){
            put32(object,0x9acu,get32(object,0x9b4u));
            object[0x9b8u]=0u;
        }
    }
    std::uint32_t ignored{};
    if(!call(0x48d420u,0x9bcu,ignored))return false;
    const auto stage=get32(object,0x9acu);
    // Original 24-entry jump table at 0x4D8E10, including its shared tails.
    switch(stage){
    case 0u:return call(0x4d5e40u,0u,ignored);
    case 1u:{
        std::uint32_t value{};
        if(!call(0x4d7300u,0u,value))return false;
        return value!=1u||call(0x4d6090u,0u,ignored);
    }
    case 2u:return call(0x4d6090u,0u,ignored);
    case 3u:return call(0x4d7440u,0u,ignored);
    case 4u:{
        std::uint32_t value{};
        if(!call(0x4d86f0u,0u,value))return false;
        return value!=1u||call(0x4d6090u,0u,ignored);
    }
    case 5u:return call(0x4d6090u,0u,ignored);
    case 6u:return call(0x4d7780u,0u,ignored);
    case 7u:{
        std::uint32_t value{};
        if(!call(0x4d8890u,0u,value))return false;
        return value!=1u||call(0x4d6340u,0u,ignored);
    }
    case 8u:return call(0x4d6340u,0u,ignored);
    case 9u:return call(0x4d78c0u,0u,ignored);
    case 10u:{
        std::uint32_t value{};
        if(!call(0x4d89b0u,0u,value))return false;
        return value!=1u||call(0x4d63f0u,0u,ignored);
    }
    case 11u:return call(0x4d63f0u,0u,ignored);
    case 12u:return call(0x4d6430u,0u,ignored);
    case 13u:{
        std::uint32_t value{};
        if(!call(0x4d7b20u,0u,value))return false;
        return value!=2u||call(0x4d7b80u,0u,ignored);
    }
    case 14u:return call(0x4d7b80u,0u,ignored);
    case 15u:return call(0x4d6530u,0u,ignored);
    case 16u:{
        std::uint32_t value{};
        if(!call(0x4d7c70u,0u,value))return false;
        return value!=2u||call(0x4d6630u,0u,ignored);
    }
    case 17u:return call(0x4d6630u,0u,ignored);
    case 18u:return call(0x4d66d0u,0u,ignored);
    case 19u:{
        std::uint32_t value{};
        if(!call(0x4d7da0u,0u,value))return false;
        if(value==1u||value==5u)object[0x9b8u]=1u;
        return true;
    }
    case 20u:object[0x9b8u]=1u;return true;
    case 21u:return call(0x4d7e00u,0u,ignored);
    case 22u:{
        std::uint32_t value{};
        if(!call(0x4d7fb0u,0u,value))return false;
        return value!=5u||call(0x4d6e50u,0u,ignored);
    }
    case 23u:return call(0x4d6e50u,0u,ignored);
    default:return true;
    }
}

} // namespace outrun::platform

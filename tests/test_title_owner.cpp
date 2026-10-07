#include "platform/title_owner.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <utility>
#include <vector>

using namespace outrun::platform;
namespace {
void require(bool condition,const char* message){
    if(!condition){std::cerr<<message<<'\n';std::exit(1);}
}
struct Calls{std::uint32_t child{};std::uint32_t pause{};};
void child(void* user){++static_cast<Calls*>(user)->child;}
void pause(void* user){++static_cast<Calls*>(user)->pause;}
std::uint32_t read32(const std::array<std::uint8_t,TitleOwnerPcSize>& object,std::size_t offset){
    std::uint32_t value{};std::memcpy(&value,object.data()+offset,4);return value;
}
struct ConstructorTrace {
    std::uint8_t* base{};
    std::vector<std::uint32_t> pc;
    std::vector<std::size_t> offsets;
    std::array<std::uint32_t,4> array_args{};
};
void construct_child(void* user,std::uint32_t pc,std::uint8_t* child){
    auto& trace=*static_cast<ConstructorTrace*>(user);
    trace.pc.push_back(pc);trace.offsets.push_back(std::size_t(child-trace.base));
}
void construct_array(void* user,std::uint32_t pc,std::uint8_t* first,
                     std::uint32_t size,std::uint32_t count,
                     std::uint32_t ctor,std::uint32_t dtor){
    auto& trace=*static_cast<ConstructorTrace*>(user);
    trace.pc.push_back(pc);trace.offsets.push_back(std::size_t(first-trace.base));
    trace.array_args={{size,count,ctor,dtor}};
}
struct TickTrace {
    std::vector<std::uint32_t> pc;
    std::vector<std::size_t> offsets;
    std::uint32_t answer{};
    std::uint32_t fail_pc{};
};
bool tick_call(void* user,std::uint32_t pc,std::uint8_t*,std::size_t offset,
               std::uint32_t& result){
    auto& trace=*static_cast<TickTrace*>(user);
    trace.pc.push_back(pc);trace.offsets.push_back(offset);
    result=trace.answer;
    return pc!=trace.fail_pc;
}
struct ControllerTrace {
    std::vector<std::uint32_t> pc;
    std::vector<std::size_t> offsets;
    std::vector<std::int32_t> args;
    std::uint32_t answer{};
    std::uint32_t fail_pc{};
};
bool controller_call(void* user,std::uint32_t pc,std::uint8_t*,
                     std::size_t offset,std::int32_t argument,
                     std::uint32_t& result){
    auto& trace=*static_cast<ControllerTrace*>(user);
    trace.pc.push_back(pc);trace.offsets.push_back(offset);
    trace.args.push_back(argument);result=trace.answer;
    return pc!=trace.fail_pc;
}
void write32(std::array<std::uint8_t,TitleOwnerPcSize>& object,
             std::size_t offset,std::uint32_t value){
    std::memcpy(object.data()+offset,&value,4);
}
}
int main(){
    std::uint32_t checks{};
    {
        std::array<std::uint8_t,0x34> object{};unsigned previous{},action{};
        require(title_base_construct_48f480(object.data(),object.size(),previous),"input constructor");
        FrontendInputSnapshot in;in.axes[0]=127;
        struct Feedback {unsigned count{},key{};int argument{};} trace;
        const auto feedback=[](void* p,unsigned key,int arg){auto& t=*static_cast<Feedback*>(p);++t.count;t.key=key;t.argument=arg;return true;};
        auto tick=[&]{return frontend_input_action_48f5f0(object.data(),object.size(),in,-1,previous,&trace,feedback,action);};
        require(tick()&&action==12,"initial held stick first decrements counter");
        require(tick()&&action==5&&trace.count==1&&trace.key==0x2000&&trace.argument==-1,"negative feedback argument does not suppress input");
        for(unsigned i=0;i<21;++i)require(tick()&&action==12,"original 20 to zero repeat countdown");
        require(tick()&&action==5&&trace.count==2,"stick repeat pulse");
        in.axes[0]=64;require(tick()&&action==12,"inclusive neutral threshold");
        in.axes[0]=-65;require(tick()&&action==3,"immediate pulse after neutral");
        in.feature_mask=0xffff;require(tick()&&action==0&&trace.key==4,"confirm priority");
        const auto before=trace.count;require(tick()&&action==0&&trace.count==before,"shared action suppresses feedback not action");
        previous=12;require(tick()&&trace.count==before+1,"another object's shared previous action is observed");
        previous=12;require(!frontend_input_action_48f5f0(object.data(),object.size(),in,1,previous,nullptr,nullptr,action),"required feedback cannot silently succeed");
        require(!frontend_input_axes_48f4f0(object.data(),object.size()-1,in),"bounded object");
        checks+=32;
    }
    for(std::uint32_t state=0;state<=8;++state)
    for(std::uint32_t mode: {1u,4u}){
        std::array<std::uint8_t,TitleOwnerPcSize> object{};
        object[0x9ac]=7;object[0x9b0]=7;object[0x9b4]=7;object[0x9b8]=7;
        TitleOwnerGlobals globals{};Calls calls{};
        require(title_owner_init_4d5d00(object.data(),object.size(),state,mode,
                    globals,{&calls,child,pause}),"title init");
        require(read32(object,0x9ac)==0&&read32(object,0x9b0)==0&&
                read32(object,0x9b4)==0&&object[0x9b8]==0,"title fields");
        const auto base=state==2?8u:14u;
        require(globals.scene_ids[0]==base&&globals.scene_ids[1]==base+1&&
                globals.scene_ids[2]==base+2,"title scene IDs");
        const auto wanted_pause=state==3u&&mode!=4u?1u:0u;
        require(globals.pause_flag==wanted_pause&&calls.pause==wanted_pause&&
                calls.child==1u,"title service order/count");
        checks+=4;
    }
    constexpr std::array<std::uint32_t,25> target{{
        0,0,0,0,0x4d60c0,0,0,0x4d62a0,0,0,0x4d6370,0,0,
        0x4d6510,0,0,0x4d6510,0,0,0,0,0,0x4d6a60,0,0}};
    for(std::uint32_t state=0;state<target.size();++state){
        require(title_owner_control_target_4d7260(state)==target[state],"title dispatch");
        ++checks;
    }
    std::array<std::uint8_t,TitleOwnerPcSize> object{};
    TitleOwnerGlobals globals{};Calls calls{};
    object[0x3c]=1;
    require(!title_owner_init_4d5d00(object.data(),object.size(),2,1,globals,
                {&calls,child,pause})&&calls.child==0,"live list guard");++checks;
    object[0x3c]=0;
    require(!title_owner_init_4d5d00(object.data(),object.size()-1,2,1,globals,
                {&calls,child,pause})&&calls.child==0,"size guard");++checks;
    ConstructorTrace trace{};trace.base=object.data();
    std::uint8_t game_state{},title_flag{};
    require(title_owner_construct_4d7140(object.data(),object.size(),
                game_state,title_flag,{&trace,construct_child,construct_array}),
            "title constructor");++checks;
    const std::vector<std::uint32_t> expected_pc{
        0x48f480u,0x48e310u,0x5816bdu,0x465160u,0x465160u,
        0x465160u,0x48c490u,0x4ed950u,0x465160u};
    const std::vector<std::size_t> expected_offsets{
        0u,0x34u,0x4acu,0x740u,0x800u,0x8c0u,0x9bcu,0x1c6cu,0x1cacu};
    require(trace.pc==expected_pc&&trace.offsets==expected_offsets,
            "constructor call order");++checks;
    require(trace.array_args==std::array<std::uint32_t,4>{{0x8cu,4u,0x570ac0u,0x49a650u}},
            "constructor array signature");++checks;
    require(read32(object,0u)==0x5ccbb4u&&read32(object,8u)==0x2cu&&
            read32(object,0x9acu)==0u&&game_state==1u&&title_flag==1u,
            "constructor parent fields");++checks;
    require(!title_owner_construct_4d7140(object.data(),object.size()-1,
                game_state,title_flag,{&trace,construct_child,construct_array}),
            "constructor size guard");++checks;
    std::array<std::uint8_t,0xa0u> base{};base.fill(0xa5u);
    std::uint32_t base_global{};
    require(title_base_construct_48f480(base.data(),base.size(),base_global)&&
            read32(object,0u)==0x5ccbb4u&&base_global==12u,
            "base constructor");++checks;
    require(!title_base_construct_48f480(base.data(),0x33u,base_global),
            "base constructor bound");++checks;
    base.fill(0xa5u);
    require(title_ui_resource_construct_465160(base.data(),base.size())&&
            base[0x08u]==0xffu&&base[0x18u]==1u&&
            base[0x40u]==0u&&base[0x64u]==0u&&base[0x66u]==0x80u&&
            base[0x9cu]==0u,"UI resource constructor");++checks;
    require(!title_ui_resource_construct_465160(base.data(),0x9fu),
            "UI constructor bound");++checks;
    base.fill(0xa5u);
    require(title_list_construct_4ed950(base.data(),base.size())&&
            base[0u]==0xffu&&base[0x10u]==1u&&
            base[0x1eu]==0x80u&&base[0x1fu]==0x41u&&
            base[0x28u]==0x0cu,"list constructor");++checks;
    require(!title_list_construct_4ed950(base.data(),0x3bu),
            "list constructor bound");++checks;
    object.fill(0xa5u);
    game_state=0u;title_flag=0u;base_global=0u;
    require(title_owner_construct_complete_4d7140(
                object.data(),object.size(),game_state,title_flag,base_global),
            "complete title constructor");++checks;
    require(read32(object,0u)==0x5ccbb4u&&read32(object,0x08u)==44u&&
            read32(object,0x34u+0x20u)==0x41800000u&&
            read32(object,0x9bcu)==0x5c1838u&&
            read32(object,0x9bcu+0x48u)==0x5c18a0u&&
            object[0x4acu]==0xa5u&&base_global==12u&&
            game_state==1u&&title_flag==1u,
            "complete title child graph and CRT no-op array");++checks;
    require(title_controller_init_48c5b0(object.data()+0x9bcu,
                                         object.size()-0x9bcu)&&
            object[0x9bcu+0x40u]==1u&&
            read32(object,0x9bcu+0x1278u)==0u,
            "title controller virtual initializer");++checks;
    require(!title_controller_init_48c5b0(object.data()+0x9bcu,0x12a5u),
            "title controller initializer bound");++checks;
    object.fill(0u);
    TitleOwnerInitialUiState4d5e40 initial_ui{};
    require(title_owner_initial_ui_4d5e40(object.data(),object.size(),3u,initial_ui)&&
            read32(object,0x9acu)==1u&&object[0x9bcu+0x34u]==1u&&
            initial_ui.entry_count==6u&&
            initial_ui.entries==std::array<std::uint32_t,7>{{8u,7u,6u,10u,11u,13u,0u}}&&
            initial_ui.text_ids==std::array<std::uint32_t,2>{{0x295u,0x296u}},
            "title initial UI stage 0 source ordering");++checks;
    require(!title_owner_initial_ui_4d5e40(object.data(),object.size(),3u,initial_ui),
            "title initial UI cannot replay after stage advance");++checks;
    object.fill(0u);
    require(title_owner_initial_ui_4d5e40(object.data(),object.size(),2u,initial_ui)&&
            initial_ui.entry_count==4u&&initial_ui.entries[3]==14u,
            "title initial UI optional record 14 follows root state");++checks;
    std::array<std::uint8_t,0x12a6u> controller{};
    ControllerTrace ctl{};std::uint32_t ctl_result=99u;
    require(title_controller_tick_48d420(controller.data(),controller.size(),
                {&ctl,controller_call,2u},ctl_result)&&ctl_result==0u&&
            ctl.pc.empty(),"inactive title controller");++checks;
    controller[0x34u]=1u;controller[0x42u]=1u;
    ctl.answer=0u;
    require(title_controller_tick_48d420(controller.data(),controller.size(),
                {&ctl,controller_call,2u},ctl_result)&&ctl_result==1u&&
            controller[0x44u]==1u&&ctl.pc==
                std::vector<std::uint32_t>{0x48cc00u,0x48f5f0u,0x4249f0u}&&
            ctl.args[1]==-1&&ctl.args[2]==0x40,
            "title controller opening action");++checks;
    controller[0x42u]=0u;controller[0x41u]=1u;
    ctl={};ctl.answer=1u;
    require(title_controller_tick_48d420(controller.data(),controller.size(),
                {&ctl,controller_call,2u},ctl_result)&&ctl_result==2u&&
            controller[0x44u]==0u&&ctl.args[1]==-1&&ctl.args[2]==0,
            "title controller closing action");++checks;
    controller[0x41u]=0u;ctl={};ctl.answer=12u;
    require(title_controller_tick_48d420(controller.data(),controller.size(),
                {&ctl,controller_call,2u},ctl_result)&&ctl_result==0u&&
            ctl.pc==std::vector<std::uint32_t>{
                0x48cc00u,0x48f5f0u,0x47f110u,0x47f110u}&&
            ctl.offsets[2]==0x48u&&ctl.offsets[3]==0x4d4u&&ctl.args[1]==1,
            "title controller widget path");++checks;
    // Every PC jump-table entry must invoke its actual source handler, not
    // the neighboring case. The child tick always precedes state dispatch.
    constexpr std::array<std::uint32_t,24> first_handler{{
        0x4d5e40u,0x4d7300u,0x4d6090u,0x4d7440u,
        0x4d86f0u,0x4d6090u,0x4d7780u,0x4d8890u,
        0x4d6340u,0x4d78c0u,0x4d89b0u,0x4d63f0u,
        0x4d6430u,0x4d7b20u,0x4d7b80u,0x4d6530u,
        0x4d7c70u,0x4d6630u,0x4d66d0u,0x4d7da0u,
        0u,0x4d7e00u,0x4d7fb0u,0x4d6e50u}};
    for(std::uint32_t stage=0;stage<first_handler.size();++stage){
        object.fill(0u);write32(object,0x9acu,stage);
        TickTrace t{};std::uint32_t ret=99u;
        require(title_owner_tick_4d8c50(object.data(),object.size(),
                    {&t,tick_call,2u},ret)&&ret==0u,
                "title tick dispatch result");
        require(t.pc.front()==0x48d420u&&t.offsets.front()==0x9bcu&&
                (first_handler[stage]==0u?t.pc.size()==1u:
                 t.pc.size()==2u&&t.pc[1]==first_handler[stage]),
                "title tick exact PC jump target");
        if(stage==20u)require(object[0x9b8u]==1u,"title pending state");
        checks+=2;
    }
    for(const auto pair: {std::pair<std::uint32_t,std::uint32_t>{1u,0x4d6090u},
                          {4u,0x4d6090u},{7u,0x4d6340u},
                          {10u,0x4d63f0u},{13u,0x4d7b80u},
                          {16u,0x4d6630u},{22u,0x4d6e50u}}){
        object.fill(0u);write32(object,0x9acu,pair.first);
        TickTrace t{};t.answer=(pair.first==13u||pair.first==16u)?2u:
                              pair.first==22u?5u:1u;
        std::uint32_t ret{};
        require(title_owner_tick_4d8c50(object.data(),object.size(),
                    {&t,tick_call,2u},ret)&&t.pc.size()==3u&&
                t.pc.back()==pair.second,
                "title conditional tail");++checks;
    }
    object.fill(0u);object[0x9b8u]=1u;
    write32(object,0x9b0u,2u);
    TickTrace exit{};exit.answer=1u;
    std::uint32_t ret{};
    require(title_owner_tick_4d8c50(object.data(),object.size(),
                {&exit,tick_call,3u},ret)&&ret==2u&&
            read32(object,0x9b0u)==1u&&exit.pc==
                std::vector<std::uint32_t>{0x55a930u,0x46c270u},
            "title owner exit action and sound gate");++checks;
    object.fill(0u);object[0x9b8u]=1u;
    write32(object,0x9b0u,1u);write32(object,0x9b4u,14u);
    TickTrace switch_state{};
    require(title_owner_tick_4d8c50(object.data(),object.size(),
                {&switch_state,tick_call,2u},ret)&&
            read32(object,0x9acu)==14u&&object[0x9b8u]==0u&&
            switch_state.pc==std::vector<std::uint32_t>{0x48d420u,0x4d7b80u},
            "title deferred state transition");++checks;
    switch_state.fail_pc=0x48d420u;
    require(!title_owner_tick_4d8c50(object.data(),object.size(),
                {&switch_state,tick_call,2u},ret),
            "title missing child remains gated");++checks;
    // Fail closed at each newly ported title-menu service boundary. In
    // particular an absent display/input must not commit a menu choice.
    for(unsigned failure:{0x48dda0u,0x48f5f0u,0x48dc10u,0x48dc60u}){
        object.fill(0);write32(object,0x9ac,1);write32(object,0x9b4,99);
        ControllerTrace trace;trace.fail_pc=failure;
        trace.answer=failure==0x48dc10?2:failure==0x48dc60?4:0;
        TitleMenuGlobals globals;globals.root_state=2;
        require(!title_menu_control_4d7300(object.data(),object.size(),globals,
                    {&trace,controller_call,2},ret)&&read32(object,0x9ac)==1&&
                read32(object,0x9b4)==99&&trace.pc.back()==failure,
                "title service failure cannot become a successful selection");++checks;
    }
    for(unsigned failure:{0x48e440u,16u}){
        object.fill(0);ControllerTrace trace;trace.fail_pc=failure;
        require(!title_menu_close_4d6090(object.data(),object.size(),
                    {&trace,controller_call,2})&&!object[0x9b8]&&trace.pc.back()==failure,
                "title close cannot acknowledge uncompleted cleanup");++checks;
    }
    object.fill(0);write32(object,0x34,~0u);write32(object,0x9ac,1);
    ControllerTrace invalid;TitleMenuGlobals invalid_globals;invalid_globals.root_state=2;
    require(!title_menu_control_4d7300(object.data(),object.size(),invalid_globals,
                {&invalid,controller_call,2},ret)&&read32(object,0x9ac)==1,
            "invalid title selection rejected without indexing outside retail table");++checks;
    std::cout<<checks<<" title-owner checks passed\n";
}

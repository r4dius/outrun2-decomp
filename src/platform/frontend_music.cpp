#include "platform/frontend_music.hpp"
namespace outrun::platform {
using driving::Bytes;
namespace {
constexpr std::int8_t Counts68f36c[3]{9,7,12};
// 5C9AB0: unlock bit per track (tracks 0..6 are always available).
constexpr unsigned Unlocks5c9ab0[28]{174,174,174,174,174,174,174,172,173,174,175,176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,192};
struct Rows {std::array<FrontendCarouselDescriptor,24> carousel{};std::array<FrontendCarouselDescriptor,12> titles{};};
// 5C9B20/5C9BF8/5C9CA0: 2*count carousel ranges of 0x28 frames (hold, move).
// 5C9DC0/5C9E30/5C9E88: one 0x6E-frame title range per track.
const std::array<Rows,3>& rows(){
    static const auto out=[]{std::array<Rows,3> r{};constexpr unsigned carousel[3]{0x440020,0x4400a6,0x44001f},titles[3]{0x44001b,0x44001a,0x440019};
        for(unsigned l=0;l<3;++l){
            for(unsigned i=0;i<2u*unsigned(Counts68f36c[l]);++i){const unsigned k=i/2;
                r[l].carousel[i]=i&1?FrontendCarouselDescriptor{carousel[l],0x14+0x28*k,0x14+0x28*k}:
                                     FrontendCarouselDescriptor{carousel[l],k?0x28*k-0x14:0,0x14+0x28*k};}
            for(unsigned i=0;i<unsigned(Counts68f36c[l]);++i)r[l].titles[i]={titles[l],0x6e*i,0x6e*(i+1)};
        }
        return r;}();
    return out;
}
constexpr unsigned TableAddress[3]{0x5c9b20,0x5c9bf8,0x5c9ca0};
constexpr unsigned Labels5c9f18[3][3]{{0x4400da,1,1},{0x4400da,2,2},{0x4400da,0,0}};
constexpr unsigned One=0x3f800000;
Bytes object(FrontendMusic& c){return Bytes(c.object.data(),c.object.size());}
Bytes carousel(FrontendMusic& c){return object(c).sub(0x35c,0x118);}
bool fail(FrontendMusic& c,FrontendMusicServices& s,unsigned pc){if(!c.fault)c.fault=pc;s.missing=pc;return false;}
bool resource(FrontendMusic& c,FrontendMusicServices& s,unsigned off,unsigned pc,const unsigned* a=nullptr,unsigned n=0,unsigned* out=nullptr){
    unsigned result{};
    if(!s.ui.call(pc,object(c).sub(off,0xa0),a,n,result)||s.ui.missing_pc)return fail(c,s,s.ui.missing_pc?s.ui.missing_pc:pc);
    if(out)*out=result;return true;
}
bool configure(FrontendMusic& c,FrontendMusicServices& s,unsigned off,std::initializer_list<unsigned> args){
    return resource(c,s,off,0x465860,args.begin(),unsigned(args.size()))&&resource(c,s,off,0x465970);
}
bool list_valid(const FrontendMusicServices& s){
    const int l=s.music.list_84b0f4;if(l<0||l>2)return false;
    const int k=s.music.cursors_84b0ec[unsigned(l)];return k>=0&&k<Counts68f36c[l];
}
// 4C9750: flat track index of the current list cursor.
int track_4c9750(const FrontendMusicServices& s){
    const auto& m=s.music;
    switch(m.list_84b0f4){
    case 1:return Counts68f36c[0]+m.cursors_84b0ec[1];
    case 2:return Counts68f36c[1]+m.cursors_84b0ec[2]+Counts68f36c[0];
    default:return m.cursors_84b0ec[0];
    }
}
// 4474E0 on 7C23E0, only asked above track 6.
bool unlocked(const FrontendMusicServices& s,int track){
    if(track<=6)return true;
    const unsigned bit=Unlocks5c9ab0[unsigned(track)];
    return (s.profile[0x28+bit/8]&(1u<<(bit%8)))!=0;
}
FrontendCarouselTable table(const FrontendMusicServices& s){
    const unsigned l=unsigned(s.music.list_84b0f4);return {rows()[l].carousel.data(),2u*unsigned(Counts68f36c[l]),TableAddress[l]};
}
bool title(FrontendMusic& c,FrontendMusicServices& s){
    const auto& r=rows()[unsigned(s.music.list_84b0f4)].titles[unsigned(s.music.cursors_84b0ec[unsigned(s.music.list_84b0f4)])];
    return configure(c,s,0x3c,{r.token,r.first,r.last,6,0,0,0,One,One,One,0});
}
bool label(FrontendMusic& c,FrontendMusicServices& s){
    const auto* r=Labels5c9f18[unsigned(s.music.list_84b0f4)];
    return configure(c,s,0x2bc,{r[0],r[1],r[2],6,0,0,0,One,One,One,0});
}
bool lock(FrontendMusic& c,FrontendMusicServices& s){return configure(c,s,0x21c,{0x440047,5,5,6,0,0,0,One,One,One,0});}
// 4C99D0/4C9A30: preview the current track on channel 0.
bool play(FrontendMusic& c,FrontendMusicServices& s,unsigned track,unsigned loop=1){
    return (s.play_401000&&s.play_401000(s.user,0,track,loop))||fail(c,s,0x401000);
}
bool play_current(FrontendMusic& c,FrontendMusicServices& s){return play(c,s,unsigned(track_4c9750(s))+0x21);}
bool sound(FrontendMusic& c,FrontendMusicServices& s,unsigned id){
    return (s.ui.effect_4249f0&&s.ui.effect_4249f0(s.ui.effect_user,id))||fail(c,s,0x4249f0);
}
// 4C9940: hide help/header/label, slide the header out, stop the preview,
// and on back restart the menu music (track 0x1E).
bool leave_4c9940(FrontendMusic& c,FrontendMusicServices& s){
    if(!resource(c,s,0x17c,0x465250)||!resource(c,s,0xdc,0x465250)||!resource(c,s,0x2bc,0x465250))return false;
    if(!configure(c,s,0xdc,{0x440088,0x19,0,4,1,0,0,One,One,0xc0000000,0})||!resource(c,s,0xdc,0x4659f0))return false;
    if(!s.stop_401030||!s.stop_401030(s.user,0))return fail(c,s,0x401030);
    if(object(c).u32(0x38)==2u)return play(c,s,0x1e);
    return true;
}
// Lock icon follows the availability of the newly selected track.
bool relock(FrontendMusic& c,FrontendMusicServices& s,bool before,bool after){
    if(before==after)return true;
    return after?resource(c,s,0x21c,0x465250):lock(c,s);
}
}
bool frontend_music_construct_4c9a90(FrontendMusic& c,unsigned& repeat){
    auto* p=c.object.data();const auto size=c.object.size();c.fault=0;c.constructed=false;
    if(!title_base_construct_48f480(p,size,repeat))return false;
    object(c).put32(0,0x5c9f3c);
    for(unsigned off:{0x3cu,0xdcu,0x17cu,0x21cu,0x2bcu,0x360u})if(!title_ui_resource_construct_465160(p+off,size-off))return false;
    object(c).put32(8,4);c.constructed=true;return true;
}
bool frontend_music_init_4c9790(FrontendMusic& c,FrontendMusicServices& s){
    if(!c.constructed||!list_valid(s))return fail(c,s,0x4c9790);
    auto b=object(c);b.put8(0x34,0);b.put8(0x35,1);b.put32(0x38,0);
    FrontendCarouselServices api{s.ui,s.timer};auto w=carousel(c);
    const unsigned l=unsigned(s.music.list_84b0f4);
    if(!frontend_carousel_init_51b7b0(w,table(s),0,2u*unsigned(Counts68f36c[l])-1,api))return fail(c,s,api.missing);
    frontend_carousel_select_51bdb0(w,s.music.cursors_84b0ec[l]);
    if(!unlocked(s,track_4c9750(s))&&!lock(c,s))return false;
    if(!configure(c,s,0xdc,{0x440088,0,0x19,5,1,0,0,One,One,0x40000000,0}))return false;
    driving::object_store_depth_pair_442f20(s.root,0,0);
    const unsigned labels[]{4,0x295,0x10,0x29c,~0u,~0u,8,0x296};unsigned result{};
    if(!s.ui.commands(0x440ea0,s.root.sub(0x51c,s.root.size()-0x51c),labels,8,s.globals,result))
        return fail(c,s,s.ui.missing_pc?s.ui.missing_pc:0x440ea0);
    return true;
}
bool frontend_music_control_4c9c20(FrontendMusic& c,FrontendMusicServices& s,unsigned& result){
    result=0;if(!c.constructed||!list_valid(s))return fail(c,s,0x4c9c20);
    unsigned action{};
    if(!frontend_input_action_48f5f0(c.object.data(),c.object.size(),s.input,0,s.repeat,&s,
        [](void* p,unsigned key,int arg){auto& x=*static_cast<FrontendMusicServices*>(p);return x.ui.input_feedback(x.root,key,arg);},action))
        return fail(c,s,s.ui.missing_pc?s.ui.missing_pc:0x48f5f0);
    if(!resource(c,s,0x3c,0x4659f0)||!resource(c,s,0xdc,0x4659f0))return false;
    auto b=object(c);auto w=carousel(c);FrontendCarouselServices api{s.ui,s.timer};
    if(!frontend_carousel_tick_51be30(w,table(s),api))return fail(c,s,api.missing);
    auto& m=s.music;
    if(b.u8(0x35)){
        unsigned ready{};
        if(!resource(c,s,0xdc,0x4652e0,nullptr,0,&ready))return false;
        if(!ready)return true;
        if(!configure(c,s,0x17c,{0x440087,0,0x23,6,0,0,0,One,One,One,0})||!title(c,s)||!label(c,s))return false;
        b.put8(0x35,0);return play_current(c,s);
    }
    if(b.u8(0x34)){result=b.u32(0x38);return true;}
    if(s.random_held_4536c0){
        b.put32(0x38,5);b.put8(0x34,1);
        int track{};
        for(unsigned tries=1;;++tries){
            track=int((frontend_crt_random_580f40(s.crt_random)&0x1f)*0x1b)>>5;
            if(tries==100||track>=28){track=0;break;}
            if(unlocked(s,track))break;
        }
        m.track_830364=std::uint8_t(track);                         // 48B1D0(68F370[track])
        return leave_4c9940(c,s);
    }
    if(s.input.device_held&0x2000){
        if(!resource(c,s,0x3c,0x465250))return false;
        if(!frontend_carousel_release_51bc30(w,api))return fail(c,s,api.missing);
        if(!resource(c,s,0x2bc,0x465250))return false;
        const bool before=unlocked(s,track_4c9750(s));
        m.list_84b0f4=std::int16_t(m.list_84b0f4+1);if(m.list_84b0f4>=3)m.list_84b0f4=0;
        if(!list_valid(s))return fail(c,s,0x4c9e6f);
        const bool after=unlocked(s,track_4c9750(s));
        const unsigned l=unsigned(m.list_84b0f4);
        if(!title(c,s))return false;
        if(!frontend_carousel_init_51b7b0(w,table(s),0,2u*unsigned(Counts68f36c[l])-1,api))return fail(c,s,api.missing);
        frontend_carousel_select_51bdb0(w,m.cursors_84b0ec[l]);
        if(!label(c,s)||!play_current(c,s)||!relock(c,s,before,after))return false;
    }
    const unsigned l=unsigned(m.list_84b0f4);auto& cursor=m.cursors_84b0ec[l];
    switch(action){
    case 0:{
        const int track=track_4c9750(s);
        if(!unlocked(s,track))return sound(c,s,3);
        b.put32(0x38,5);b.put8(0x34,1);cursor=b.i16(0x410);
        if(!list_valid(s))return fail(c,s,0x4ca057);
        m.track_830364=std::uint8_t(track);
        return sound(c,s,0x40)&&leave_4c9940(c,s);
    }
    case 1:
        b.put8(0x34,1);b.put32(0x38,2);cursor=b.i16(0x410);
        if(!list_valid(s))return fail(c,s,0x4ca09b);
        return leave_4c9940(c,s);
    case 3:case 5:{
        const bool forward=action==5;
        if(forward?2*cursor>=2*Counts68f36c[l]-2:cursor<=0)return true;
        if(!frontend_carousel_move(w,table(s),forward,api))return fail(c,s,api.missing);   // 51BB10 / 51BBA0
        if(!resource(c,s,0x3c,0x465250))return false;
        const bool before=unlocked(s,track_4c9750(s));
        cursor=std::int16_t(cursor+(forward?1:-1));
        const bool after=unlocked(s,track_4c9750(s));
        return title(c,s)&&play_current(c,s)&&relock(c,s,before,after);
    }
    default:return true;
    }
}
bool frontend_music_suspend_4c9910(FrontendMusic& c,FrontendMusicServices& s){
    FrontendCarouselServices api{s.ui,s.timer};
    if(!frontend_carousel_release_51bc30(carousel(c),api))return fail(c,s,api.missing);
    if(!resource(c,s,0x3c,0x465250))return false;
    if(object(c).u32(0x224)!=~0u)return resource(c,s,0x21c,0x465250);
    return true;
}
}

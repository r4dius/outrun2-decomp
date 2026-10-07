#include "frontend_requests.hpp"
#include <cstring>
namespace outrun::platform {
using driving::Bytes;
namespace {
// Unmodified PC tables 5CE738..5CEC1C and 69EF24 / 69EF64.
struct Descriptor {unsigned token,first,last;};
struct Point {float x,y;};
constexpr unsigned MapToken=0x4400dc;                                       // [69EF64]
constexpr unsigned results_5ce954[15]{1,1,1,2,2,2,3,3,3,3,4,4,4,4,4};
constexpr int nav_5ce850[16][4]{                                             // up, down, right, left
    {-1,-1,1,-1},{-1,-1,2,0},{-1,3,4,1},{2,-1,5,1},{-1,5,7,2},{4,6,8,3},{5,-1,9,3},{-1,8,11,4},
    {7,9,12,5},{8,10,13,6},{9,-1,14,6},{-1,12,-1,7},{11,13,-1,8},{12,14,-1,9},{13,15,-1,10},{14,-1,-1,10}};
constexpr Point cursor_5ce738[16]{{-255,-39},{-189,-39},{-114,-66},{-114,-15},{-39,-89},{-39,-39},{-39,10},{40,-116},
    {40,-66},{40,-15},{40,35},{114,-139},{114,-89},{114,-39},{114,10},{114,60}};
constexpr Point stages_5ce7c0[15]{{-172,-39},{-97,-66},{-97,-15},{-22,-89},{-22,-39},{-22,10},{57,-116},{57,-66},
    {57,-15},{57,35},{131,-139},{131,-89},{131,-39},{131,10},{131,60}};
constexpr Point captions_5ce838[3]{{310,318},{310,334},{310,350}};
constexpr Descriptor lists_5ce990[6]{{0x4400ba,0,29},{0x4400ba,30,59},{0x4400ba,60,89},{0x4400ba,90,119},{0x4400af,6,6},{0x4400af,5,5}};
constexpr Descriptor grades_5ce9c0[7]{{0x4400af,6,6},{0x4400af,5,5},{0x4400af,4,4},{0x4400af,3,3},{0x4400af,2,2},{0x4400af,1,1},{0x4400af,0,0}};
constexpr unsigned icons_5cea18[30]{31,17,24,7,33,15,14,20,16,18,35,28,8,12,23,34,10,36,13,22,11,9,25,27,30,26,32,19,29,21};
constexpr unsigned courses_5cea90[30]{12,6,10,0,13,4,3,8,5,7,14,11,1,2,9,28,16,29,18,21,17,15,22,24,26,23,27,19,25,20};
constexpr Descriptor footers_5ceb08[3]{{0x4400ca,0,0},{0x4400ca,3,3},{0x4400ca,1,1}};
constexpr Descriptor titles_5ceb38[3]{{0x4400a7,71,71},{0x4400a7,70,70},{0x4400a7,72,72}};
constexpr Descriptor list_titles_5ceb68[4]{{0x4400a7,74,74},{0x4400a7,75,75},{0x4400a7,76,76},{0x4400a7,77,77}};
constexpr unsigned captions_5ceb98[33]{0x354,0x355,0x356,0x357,0x358,0x359,0x35a,0x35b,0x35c,0x35d,0x35f,0x35e,0x360,
    0x361,0x362,0x363,0x364,0x365,0x366,0x367,0x368,0x369,0x36a,0x36b,0x36c,0x36d,0x36e,0x36f,0x370,0x371,0x372,0x373,0x42f};
constexpr const char* lists_64f080[4]{"REQUEST_NORMAL","REQUEST_SPECIAL1","REQUEST_SPECIAL2","REQUEST_SPECIAL3"};
constexpr Point slots_69ef24[8]{{84,342},{128,342},{172,342},{216,342},{78,358},{122,358},{166,358},{210,358}};

bool fail(FrontendRequestServices& s,unsigned pc){s.missing=pc;return false;}
Bytes object(FrontendRequestState& c){return {c.object.data(),c.object.size()};}
unsigned bits(float f){unsigned r;std::memcpy(&r,&f,4);return r;}
bool resource(FrontendRequestState& c,FrontendRequestServices& s,unsigned off,unsigned pc,const unsigned* a=nullptr,unsigned n=0){
    unsigned r{};
    if(!s.ui.call(pc,object(c).sub(off,0xa0),a,n,r)||s.ui.missing_pc)return fail(s,s.ui.missing_pc?s.ui.missing_pc:pc);
    return true;
}
bool release(FrontendRequestState& c,FrontendRequestServices& s,unsigned off){return resource(c,s,off,0x465250);}
// 465250 / 465860(token, first, last, layer, mode, x, y, 1, 1, 1, 0) / 465970.
bool configure(FrontendRequestState& c,FrontendRequestServices& s,unsigned off,Descriptor d,unsigned layer,float x=0,float y=0){
    const unsigned a[]{d.token,d.first,d.last,layer,0,bits(x),bits(y),0x3f800000,0x3f800000,0x3f800000,0};
    return release(c,s,off)&&resource(c,s,off,0x465860,a,11)&&resource(c,s,off,0x465970);
}
bool sound(FrontendRequestServices& s,unsigned id){
    return (s.ui.effect_4249f0&&s.ui.effect_4249f0(s.ui.effect_user,id))||fail(s,0x4249f0);
}
// [535EF0(4035F0()) + 208] >> 8 & 7: the categories group (4..6), its 15 course types.
bool group(FrontendRequestServices& s,unsigned& g){
    g=(s.root.u32(0x20c)>>8)&7;
    if(g<4||g>6||!s.progress.championship_present[g-4])return fail(s,0x4ea3c0);
    return true;
}
unsigned type_of(FrontendRequestServices& s,unsigned g,unsigned i){return s.progress.championship_types[g-4][i];}
// The four 4-bit results of a course type (licence +36E word, 7 = none): low byte low / high
// nibble, then the high byte's (4E9DE0's order).
unsigned result(FrontendRequestServices& s,unsigned type,unsigned j){
    const unsigned w=s.profile[0x36eu + type*2]|unsigned(s.profile[0x36fu + type*2])<<8;
    return j==1?(w>>4)&15:j==2?(w>>8)&15:j==3?w>>12:w&15;
}
// 4EA030(stage, type): one past the last result of the stage's results.
unsigned done_4ea030(FrontendRequestServices& s,unsigned stage,unsigned type){
    unsigned n=0;
    for(unsigned j=0;j<results_5ce954[stage];++j)if(result(s,type,j)!=7)n=j+1;
    return n;
}
unsigned stage_index(FrontendRequestServices& s){
    const auto sel=s.categories.selection_84b7f8;
    return sel>0?sel-1:0;
}
// 4EA210: the cursor (+17C): a map node, or the result slot +38 with the list's picture (+21C).
bool selection_4ea210(FrontendRequestState& c,FrontendRequestServices& s){
    const auto b=object(c);
    if(!release(c,s,0x21c))return false;
    if(!b.u8(0x34)){
        const auto sel=s.categories.selection_84b7f8;
        if(sel>=16)return fail(s,0x4ea210);
        return configure(c,s,0x17c,{MapToken,0,0x34},4,cursor_5ce738[sel].x,cursor_5ce738[sel].y);
    }
    const auto cursor=b.u32(0x38);
    if(cursor>=4)return fail(s,0x4ea210);
    if(!configure(c,s,0x21c,lists_5ce990[cursor],4))return false;
    return configure(c,s,0x17c,{MapToken,0,0x34},5,slots_69ef24[cursor].x-320.f,slots_69ef24[cursor].y-240.f);
}
// 4F12A0's relocation: the target of the pointer stored at a file offset.
bool pointer(const driving::PcRelocCategoryBlobR077& blob,std::size_t at,std::uint32_t& target){
    const Bytes b(blob.data,blob.size);
    for(std::uint32_t i=0;i<blob.relocation_count;++i)
        if(b.u32(8u+i*8u)==at){target=b.u32(8u+i*8u+4u);return target+8u<=blob.size;}
    return false;
}
}
bool frontend_request_caption_4ea070(const driving::PcRelocCategoryBlobR077& blob,unsigned list,unsigned i,unsigned& caption){
    if(list>=4)return false;
    auto* records=static_cast<std::uint8_t*>(driving::runtime_category_records_4f1a90(lists_64f080[list],blob));
    if(!records)return false;
    const std::size_t at=std::size_t(records-static_cast<std::uint8_t*>(blob.data))+i*0x74u+0x58u;
    std::uint32_t target{};if(!pointer(blob,at,target))return false;
    const Bytes b(blob.data,blob.size);
    const unsigned kind=b.u32(target),sub=b.u32(target+4);
    switch(kind){
    case 1:caption=0xd;break;
    case 2:case 9:caption=1;break;
    case 3:caption=sub==1?6:0;break;
    case 4:caption=2;break;
    case 5:caption=sub==0?9:sub==2?0xb:0xa;break;
    case 6:caption=3;break;
    case 7:caption=4;break;
    case 8:caption=5;break;
    case 10:caption=0x12;break;
    case 11:caption=sub==1?7:0x16;break;
    case 12:caption=0xe;break;
    case 13:caption=0x13;break;
    case 14:caption=0x19;break;
    case 15:caption=0x10;break;
    case 16:{constexpr unsigned v[7]{0x1b,0x1f,0x1c,0x1d,0x1a,0x1e,0x20};caption=sub<=6?v[sub]:0;break;}
    case 17:caption=0xc;break;
    case 18:caption=0xf;break;
    case 19:caption=0x15;break;
    case 20:caption=8;break;
    case 21:caption=0x18;break;
    case 22:caption=0x11;break;
    case 23:caption=0x14;break;
    case 24:caption=0x17;break;
    default:caption=0;break;
    }
    return true;
}
bool frontend_requests_construct_4e9e50(FrontendRequestState& c,unsigned& repeat){
    c.fault=0;c.constructed=false;c.glyphs.clear();
    if(!title_base_construct_48f480(c.object.data(),c.object.size(),repeat))return false;
    for(unsigned off=0x3c;off<0x1f7c;off+=0xa0)
        if(!title_ui_resource_construct_465160(c.object.data()+off,c.object.size()-off))return false;
    if(!title_widget_construct_48e590(c.object.data()+0x1f7c,PcTextWidgetBytes,repeat))return false;
    object(c).put32(0,0x5cec1c);object(c).put32(8,0x3c);c.constructed=true;return true;
}
bool frontend_requests_init_4ea330(FrontendRequestState& c,FrontendRequestServices& s){
    object(c).put8(0x34,0);object(c).put32(0x38,0);
    if(!selection_4ea210(c,s))return false;
    auto w=object(c).sub(0x1f7c,PcTextWidgetBytes);
    w.put32(0x450,9);w.put32(0x454,5);w.put32(0x474,0xff3f474a);w.put32(0x480,1);
    w.putf(0x34,300);w.putf(0x38,330);
    const unsigned rect[]{300,330,570,370};
    if(!frontend_text_setter(0x48eee0,w,rect,4))return fail(s,0x48eee0);
    w.put8(0x470,1);return true;
}
bool frontend_requests_control_4ea3c0(FrontendRequestState& c,FrontendRequestServices& s,unsigned& result){
    result=0;unsigned g{},action{};if(!group(s,g))return false;
    auto& sel=s.categories.selection_84b7f8;
    if(sel>=16)return fail(s,0x4ea3c0);
    const unsigned stage=stage_index(s),type=type_of(s,g,stage),done=done_4ea030(s,stage,type);
    if(!frontend_input_action_48f5f0(c.object.data(),c.object.size(),s.input,0,s.repeat,&s,
        [](void* p,unsigned key,int arg){auto& v=*static_cast<FrontendRequestServices*>(p);return v.ui.input_feedback(v.root,key,arg);},action))
        return fail(s,s.ui.missing_pc?s.ui.missing_pc:0x48f5f0);
    auto b=object(c);const bool detail=b.u8(0x34)!=0;const auto cursor=b.u32(0x38);
    auto move=[&]{return selection_4ea210(c,s)&&sound(s,1);};
    auto go=[&](int next){if(next<0)return true;sel=unsigned(next);return move();};
    switch(action){
    case 0:
        if(sel&&!done)return sound(s,3);
        if(!sound(s,0x40))return false;
        if(sel&&!detail){b.put8(0x34,1);b.put32(0x38,0);return selection_4ea210(c,s);}
        s.root.put32(0x20c,(s.root.u32(0x20c)&~0x3f000u)|((sel<<12)&0x3f000u));
        s.root.put32(0x20c,(s.root.u32(0x20c)&~0x1c0000u)|((cursor<<18)&0x1c0000u));
        b.put32(4,0xa);result=1;return true;
    case 1:
        if(detail){b.put8(0x34,0);b.put32(0x38,0);return selection_4ea210(c,s);}
        s.root.put32(0x20c,s.root.u32(0x20c)&0xfffc0fffu);
        s.root.put32(0x20c,s.root.u32(0x20c)&0xffe3ffffu);
        result=2;return true;
    case 2:return detail||go(nav_5ce850[sel][0]);
    case 4:return detail||go(nav_5ce850[sel][1]);
    case 5:
        if(!detail)return go(nav_5ce850[sel][2]);
        if(cursor>=done-1u)return true;
        b.put32(0x38,cursor+1);return move();
    case 3:
        if(!detail)return go(nav_5ce850[sel][3]);
        if(std::int32_t(cursor)<=0)return true;
        b.put32(0x38,cursor-1);return move();
    default:return true;
    }
}
bool frontend_requests_display_4ea610(FrontendRequestState& c,FrontendRequestServices& s){
    c.glyphs.clear();unsigned g{};if(!group(s,g))return false;
    const auto sel=s.categories.selection_84b7f8;
    if(sel>=16)return fail(s,0x4ea610);
    const unsigned stage=stage_index(s),type=type_of(s,g,stage),done=done_4ea030(s,stage,type);
    unsigned icon=type;                                                    // [esp+14]
    if(type>=30&&type<60)icon=type-30;else if(type==60)icon=0;else if(type==61)icon=15;
    if(icon>=30)return fail(s,0x4ea610);
    const unsigned on=sel>0?1:0;
    if(!configure(c,s,0x3c,{0x4400b2,on,on},1)||!configure(c,s,0xdc,{0x4400b3,on,on},3))return false;
    // The 15 stages: a node where a result exists, its grade (mean of the scores - 1).
    for(unsigned i=0;i<15;++i){
        const unsigned node=0xa3c+i*0xa0,mark=0x139c+i*0xa0,t=type_of(s,g,i),n=results_5ce954[i];
        if(!release(c,s,node))return false;
        unsigned last=0,score=0;
        for(unsigned j=0;j<n;++j){const auto r=result(s,t,j);if(r!=7){last=j+1;score+=r+1;}}
        if(!last)continue;
        if(!configure(c,s,node,{0x4400ae,0,0},2,stages_5ce7c0[i].x,stages_5ce7c0[i].y))return false;
        const std::int32_t q=std::int32_t(score/n)-1;
        const unsigned grade=q>=0?unsigned(q):score?0u:7u;
        if(grade>7)return fail(s,0x4ea610);                                   // past the 5CE9C0 table
        if(!release(c,s,mark))return false;
        if(grade!=7&&!configure(c,s,mark,grades_5ce9c0[grade],3,stages_5ce7c0[i].x+2.f,stages_5ce7c0[i].y))return false;
    }
    for(unsigned k=0;k<4;++k)for(unsigned off:{0x2bcu,0x53cu,0x7bcu})if(!release(c,s,off+k*0xa0))return false;
    // The selected stage's result slots: done ones with the course icon and their grade.
    const unsigned slots=sel?results_5ce954[sel-1]:0;
    for(unsigned i=0;i<slots;++i){
        const float x=slots_69ef24[i].x-320.f,y=slots_69ef24[i].y-240.f;
        if(i<done){
            if(!configure(c,s,0x2bc+i*0xa0,{0x4400bb,3,3},3,x,y)||
               !configure(c,s,0x53c+i*0xa0,{0x4400b9,icons_5cea18[icon],icons_5cea18[icon]},2,x,y))return false;
            const auto r=result(s,type,i);
            if(r>7)return fail(s,0x4ea610);                                   // past the 5CE9C0 table
            if(r!=7&&!configure(c,s,0x7bc+i*0xa0,grades_5ce9c0[r],4,slots_69ef24[i+4].x-320.f,slots_69ef24[i+4].y-240.f))return false;
        }else if(!configure(c,s,0x2bc+i*0xa0,{0x4400bb,5,5},2,x,y))return false;
    }
    if(!release(c,s,0x1cfc)||!release(c,s,0x1d9c))return false;
    if(!configure(c,s,0x1cfc,{0x4400b8,3,3},2))return false;
    const auto b=object(c);const bool detail=b.u8(0x34)!=0;const auto cursor=b.u32(0x38);
    if(detail&&cursor>=4)return fail(s,0x4ea610);
    const Descriptor title=detail?list_titles_5ceb68[cursor]:sel?Descriptor{0x4400a7,courses_5cea90[icon],courses_5cea90[icon]}:titles_5ceb38[g-4];
    if(!configure(c,s,0x1d9c,title,2,-83.f,148.f)||!configure(c,s,0x1edc,footers_5ceb08[g-4],1))return false;
    if(!release(c,s,0x1e3c))return false;
    if(detail){
        // The request list of the result slot (42CA60(9), scale 1, mode 5, colour, 42C360(1)).
        driving::PcRelocCategoryBlobR077 blob{};
        if(!s.script||!s.script(type,blob))return fail(s,0x4f12a0);
        const auto count=driving::runtime_category_count_4f1ba0(lists_64f080[cursor],blob);
        if(count<0||count>3)return fail(s,0x4f1ba0);
        if(count&&!configure(c,s,0x1e3c,{0x4400bc,unsigned(count-1),unsigned(count-1)},3))return false;
        for(std::int32_t i=0;i<count;++i){
            unsigned k{};if(!frontend_request_caption_4ea070(blob,cursor,unsigned(i),k)||k>=33)return fail(s,0x4ea070);
            const auto* text=s.text?s.text->get(captions_5ceb98[k]):nullptr;
            if(!text)return fail(s,0x465eb0);
            const auto x=short(captions_5ce838[i].x),y=short(captions_5ce838[i].y);
            if(!s.fonts||!frontend_text_draw(s.fonts->fonts[9],{1,1,0xff3f474a,5,1},{x,x,y},*text,c.glyphs))return fail(s,0x42cdd0);
        }
        return true;
    }
    // The caption box: 0x462 on the start node, else 0x463 when the stage has a result, 0x381.
    unsigned id=0x462;
    if(sel){
        const unsigned n=results_5ce954[slots<15?slots:14];                 // the PC indexes 5CE954 by the slot count
        unsigned last=0;for(unsigned j=0;j<n;++j)if(result(s,type,j)!=7)last=j+1;
        id=last?0x463:0x381;
    }
    const auto* caption=s.text?s.text->get(id):nullptr;
    if(!caption)return fail(s,0x465eb0);
    auto w=object(c).sub(0x1f7c,PcTextWidgetBytes);
    if(!frontend_text_set_48f280(w,*caption,w.u32(0x450),w.u32(0x474)))return fail(s,0x48ee80);
    return (s.fonts&&frontend_text_display_48f3c0(w,*s.fonts,c.lines,c.glyphs))||fail(s,0x48f3c0);
}
bool frontend_requests_suspend_4eaeb0(FrontendRequestState& c,FrontendRequestServices& s){
    c.glyphs.clear();if(!c.constructed)return true;
    if(!release(c,s,0x3c)||!release(c,s,0xdc))return false;
    for(unsigned k=0;k<4;++k)for(unsigned off:{0x2bcu,0x53cu,0x7bcu})if(!release(c,s,off+k*0xa0))return false;
    for(unsigned k=0;k<15;++k)if(!release(c,s,0xa3c+k*0xa0)||!release(c,s,0x139c+k*0xa0))return false;
    for(unsigned off:{0x17cu,0x21cu,0x1cfcu,0x1d9cu,0x1e3cu,0x1edcu})if(!release(c,s,off))return false;
    return true;
}
}

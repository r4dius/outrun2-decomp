#include "platform/frontend_showroom.hpp"
#include "platform/frontend_showroom_tables.hpp"
#include "platform/frontend_vehicle_data.hpp"
#include "platform/title_owner.hpp"
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
namespace outrun::platform {
using driving::Bytes;
namespace {
struct Fault{unsigned pc;};
[[noreturn]] void fault(unsigned pc){throw Fault{pc};}
unsigned bits(float v){unsigned r;std::memcpy(&r,&v,4);return r;}
const std::vector<std::array<std::uint32_t,3>>& sprites(){
    static const std::vector<std::array<std::uint32_t,3>> t(ShowroomSprites5cae00.begin(),ShowroomSprites5cae00.end());return t;}
struct Run {
    FrontendShowroom& c;FrontendShowroomServices& s;Bytes o;
    Run(FrontendShowroom& c_,FrontendShowroomServices& s_):c(c_),s(s_),o(c_.object.data(),c_.object.size()){}
    FrontendUiResources& ui(){
        if(!c.ui&&c.pool)c.ui=std::make_unique<FrontendUiResources>(FrontendUiResources{*c.pool});
        if(!c.ui)fault(0x465160);return *c.ui;}
    FrontendChoiceList& list(){
        if(!c.list)c.list=std::make_unique<FrontendChoiceList>(o.sub(0x380,PcFrontendChoiceListBytes),ui(),s.fonts);
        return *c.list;}
    // ---- 465xxx UI resources -------------------------------------------
    unsigned res(unsigned off,unsigned pc,const unsigned* a=nullptr,unsigned n=0){
        unsigned r{};auto& u=ui();u.missing_pc=0;
        if(!u.call(pc,o.sub(off,0xa0),a,n,r)||u.missing_pc)fault(u.missing_pc?u.missing_pc:pc);return r;}
    void release(unsigned off){res(off,0x465250);}
    void commit(unsigned off){res(off,0x465970);}
    void tick(unsigned off){res(off,0x4659f0);}
    bool finished(unsigned off){return res(off,0x4652e0)!=0;}
    // 465860(token, first, last, layer, mode, x 0, y 0, 1, 1, speed, 0).
    void play(unsigned off,unsigned token,unsigned first,unsigned last,unsigned layer,unsigned mode,float speed=1){
        const unsigned a[]{token,first,last,layer,mode,0,0,bits(1),bits(1),bits(speed),0};res(off,0x465860,a,11);}
    void row(unsigned off,unsigned index,unsigned layer,unsigned mode,float speed=1){
        if(index>=ShowroomSprites5cae00.size())fault(0x465860);
        const auto& t=ShowroomSprites5cae00[index];play(off,t[0],t[1],t[2],layer,mode,speed);}
    // ---- PC leaves -------------------------------------------------------
    void sound(unsigned id){auto& u=ui();if(!u.effect_4249f0||!u.effect_4249f0(u.effect_user,id))fault(0x4249f0);}
    void external(unsigned pc,std::initializer_list<unsigned> a={}){
        if(!s.external||!s.external(s.user,pc,a.begin(),a.size()))fault(pc);}
    unsigned input(int arg){
        unsigned action{};
        if(!frontend_input_action_48f5f0(c.object.data(),c.object.size(),s.input,arg,s.repeat,this,feedback,action))
            fault(ui().missing_pc?ui().missing_pc:0x48f5f0);
        return action;}
    static bool feedback(void* p,unsigned key,int a){auto& r=*static_cast<Run*>(p);return r.ui().input_feedback(r.s.root,key,a);}
    float points()const{float f;std::memcpy(&f,s.profile.data()+0x24,4);return f;}
    static int ftol(float f){return std::isfinite(f)&&f>-2147483648.f&&f<2147483648.f?int(f):INT32_MIN;}   // cvttss2si
    bool unlocked(int bit)const{                                                  // 4474E0
        int q=bit;if(q<0)q+=7;const int byte=q>>3;int b=bit%8;if(b<0)b+=8;
        if(byte<0||0x28+byte>=int(PcLicenseBytes))fault(0x4474e0);
        return (s.profile[std::size_t(0x28+byte)]&(1u<<b))!=0;}
    unsigned price(int item)const{                                                // 4EF4D0
        if(item<0||item>=0xcf||!s.prices)fault(0x4ef4d0);
        std::uint32_t v;std::memcpy(&v,s.prices+item*4,4);return v;}
    // 447460: buy when not owned and the truncated Miles reach the price.
    bool buy(int bit,unsigned cost){
        if(unlocked(bit))return false;
        const float p=points();const std::int64_t whole=std::isfinite(p)?std::int64_t(p):0;
        if(std::uint32_t(whole)<cost)return false;
        int q=bit;if(q<0)q+=7;int b=bit%8;if(b<0)b+=8;
        s.profile[std::size_t(0x28+(q>>3))]|=std::uint8_t(1u<<b);
        const float left=float(double(p)-double(cost));std::memcpy(s.profile.data()+0x24,&left,4);
        return true;}
    // ---- records ----------------------------------------------------------
    Bytes rec(unsigned i){if(i>=30)fault(0x4d4d00);return o.sub(0x34+i*0x1c,0x1c);}
    int selected(){return o.i32(0x380);}
    // 4D4200 / inline: the item of the selected row.
    int item(){
        const int base=o.i32(0xbe0);if(base==0xcf)return 0;
        const int sel=selected();if(base>=0)return sel+base;
        if(sel<0||sel>=30)fault(0x4d4200);return rec(unsigned(sel)).i32(8);}
    // ---- preview (48C170..48C450) ----------------------------------------
    Bytes preview_bytes(){return o.sub(0xaa4,0x94);}
    void preview(bool ok,unsigned pc){if(!ok)fault(s.preview&&s.preview->fault?s.preview->fault:pc);}
    FrontendVehiclePreviewServices& pv(){if(!s.preview)fault(0x48c170);return *s.preview;}
    void preview_select(Bytes r){preview(frontend_vehicle_preview_select_48c290(preview_bytes(),r.u32(0xc),r.u8(0x11),true,pv()),0x48c290);}
    void camera(int v){s.camera_preset_819634=v;}                                 // 482E70
    // ---- 446710 icon strip at +9D8 (+18 resource, +B8 token, +BC step, +C0 base)
    Bytes strip(){return o.sub(0x9d8,0xc8);}
    void strip_play(unsigned first,unsigned last,unsigned mode){
        auto w=strip();play(0x9d8+0x18,w.u32(0xb8),first,last,4,mode);commit(0x9d8+0x18);}
    void strip_init(unsigned token,int count,int step,int base,std::uint8_t wrap){
        auto w=strip();w.puti(0xbc,step);w.puti(4,count);w.puti(0xc,count);w.put32(0xb8,token);w.puti(0xc0,base);
        w.put8(0xc4,0);w.put8(0xc5,1);w.put32(0,0);w.put32(8,0);w.put8(0x10,wrap);w.put32(0x14,0xf);
        play(0x9d8+0x18,token,1,unsigned(base),4,1);commit(0x9d8+0x18);}
    unsigned frame(int index){auto w=strip();return unsigned(w.i32(0xbc)*index+w.i32(0xc0));}
    void strip_leave(){auto w=strip();const int i=w.i32(8);strip_play(frame(i),unsigned(w.i32(0xbc)*(i+1)),3);}
    void strip_tick(){                                                             // 4467A0
        auto w=strip();if(!w.u8(0xc5))return;
        tick(0x9d8+0x18);
        if(finished(0x9d8+0x18)&&w.u32(0x18+8)!=~0u){const auto f=frame(w.i32(8));strip_play(f,f,0);return;}
        if(finished(0x9d8+0x18)&&w.u32(0x18+8)==~0u){                             // 4462A0(0)
            const int k=w.i32(0xbc)*w.i32(8);strip_play(unsigned(k+1),unsigned(w.i32(0xc0)+k),1);}
    }
    void strip_release(){strip_leave();release(0x9d8+0x18);strip().put8(0xc5,0);}  // 446830
    void strip_next(){                                                             // 4468A0
        strip_leave();auto w=strip();const int i=w.i32(8);
        if(i<w.i32(0xc)+w.i32(0)){w.puti(8,i+1);sound(1);}else if(w.u8(0x10)){w.puti(8,w.i32(0));sound(1);}}
    void strip_prev(){                                                             // 446930
        strip_leave();auto w=strip();const int i=w.i32(8);
        if(i>w.i32(0)){w.puti(8,i-1);sound(1);}else if(w.u8(0x10)){w.puti(8,w.i32(4));sound(1);}}
    // ---- choice list +380 -------------------------------------------------
    void list_init(int scroll,int limit,unsigned first,unsigned last){
        auto& l=list();if(!l.clear_48e440())fault(0x48e440);
        if(!l.initialize_48d970(0x5cae00,sprites(),1,2,1,std::uint8_t(scroll),limit,nullptr,0))fault(0x48d970);
        auto h=o.sub(0x380,PcFrontendChoiceListBytes);
        h.putf(0x20,19.f);h.putf(0x24,-180.f);h.putf(0x28,-50.f);h.put32(0x2c,1);   // 48E250 / 48E260 / 48E2A0
        for(unsigned r=first;r<=last;++r){unsigned index{};if(!l.add_sprite_48e390(r,s.repeat,index))fault(0x48e390);}
    }
    void list_display(){if(!list().display_48dda0(s.timer,c.lines,c.glyphs))fault(0x48dda0);}
    void list_move(bool forward){bool snd{};if(!list().move(forward,snd))fault(forward?0x48dc60:0x48dc10);}   // argument 0: no 4249F0
    void list_clear(){if(c.list&&!c.list->clear_48e440())fault(0x48e440);}
    // ---- titles ------------------------------------------------------------
    void title_in(unsigned r){                                                     // 4D3DE0
        release(0x898);play(0x898,0x44009a,0x1e,0,0,1,-1);commit(0x898);
        release(0x938);row(0x938,r,0,1);commit(0x938);}
    void title_hold(unsigned r){release(0x938);row(0x938,r,0,0);commit(0x938);}   // 4D3E80
    void title_out(unsigned r){                                                    // 4D3EE0
        release(0x938);row(0x938,r,0,1,-1);commit(0x938);
        release(0x898);play(0x898,0x44009a,0,0x1e,0,1);commit(0x898);}
    // ---- price / owned icon (+B3C) and the texts ----------------------------
    void text_init(unsigned off){frontend_text_init_48e640(o.sub(off,PcTextWidgetBytes));o.put32(off+0x450,9);}
    void text_set(unsigned off,const std::string& v,unsigned font,unsigned colour){
        if(!frontend_text_set_48f280(o.sub(off,PcTextWidgetBytes),v,font,colour))fault(0x48f280);}
    void owned_show(){                                                             // 4D4430
        release(0xb3c);
        if(unlocked(item())){play(0xb3c,0x440033,0x14,0x23,0,0);o.put32(0xb38,1);}
        else{play(0xb3c,0x440033,0,0xf,0,0);o.put32(0xb38,0);}
        if(o.i32(0xbe0)!=0xcf)commit(0xb3c);
        text_init(0x1fe0);text_set(0x1fe0,"0",0,~0u);o.putf(0x1fe0+0x34,480.f);o.putf(0x1fe0+0x38,368.f);
    }
    void owned_tick(){                                                             // 4D4520
        tick(0xb3c);
        if(!finished(0xb3c)&&!o.u8(0xbdc))return;
        release(0xb3c);unsigned r;
        if(o.u8(0xbdc)){if(unlocked(item())){o.put32(0xb38,1);r=0xb;}else{o.put32(0xb38,0);r=8;}}
        else switch(o.u32(0xb38)){case 0:r=8;break;case 1:r=0xb;break;case 2:r=0xe;break;default:fault(0x4d4520);}
        row(0xb3c,r,0,0);
        if(o.i32(0xbe0)!=0xcf)commit(0xb3c);
        o.put8(0xbdc,0);
    }
    void owned_hide(){                                                             // 4D4630
        release(0xb3c);
        if(unlocked(item())){play(0xb3c,0x440033,0x23,0x14,0,3);o.put32(0xb38,1);}
        else{play(0xb3c,0x440033,0xf,0,0,3);o.put32(0xb38,0);}
        if(o.i32(0xbe0)!=0xcf)commit(0xb3c);
    }
    void affordable(){                                                             // 4D43F0
        const int miles=ftol(points());
        for(unsigned i=0;i<30;++i){auto r=rec(i);r.put8(0x10,r.u8(0)?1:std::uint8_t(miles>=int(price(r.i32(8)))));}
    }
    void state(unsigned v){o.put32(0x37c,v);}
    void next_state(){o.put32(0x37c,o.u32(0x37c)+1);}
    // ---- purchase dialog -----------------------------------------------------
    Bytes window(){return o.sub(0xbec,PcFrontendWindowBytes);}
    void purchase(int it,unsigned back){                                           // 4D46E0
        o.put32(0xbe8,back);o.puti(0xbe4,it);
        if(unlocked(it)){state(back);return;}
        if(preview_bytes().u32(0))preview(frontend_vehicle_preview_close_48c260(preview_bytes(),pv()),0x48c260);
        const auto* t=s.text.get(0x3a5);if(!t)fault(0x465eb0);
        if(!frontend_window_init_48d0c0(window(),s.fonts,s.text,c.lines,*t,"",0,330,180,239,130,0xc,0))fault(0x48d0c0);
        const bool short_of=int(price(it))>ftol(points());
        release(0x1e9c);play(0x1e9c,0x4400d1,short_of?4:0,short_of?4:0,0xd,0);commit(0x1e9c);
        o.put8(0x28f8,short_of?0:1);sound(short_of?3:0x40);
        o.put32(0x1f3c,3);state(0x16);
    }
    void window_tick(){                                                            // vtable +8 (48D420)
        struct Ctx{Run* r;} ctx{this};
        TitleControllerServices svc;svc.user=&ctx;svc.root_state=s.root_state;
        svc.call=[](void* p,unsigned pc,std::uint8_t* object,std::size_t offset,int arg,unsigned& result){
            auto& r=*static_cast<Ctx*>(p)->r;result=0;
            if(pc==0x48cc00)return title_controller_motion_48cc00(object,PcFrontendWindowBytes,r.s.owner_delta);
            if(pc==0x47f110)return true;
            if(pc==0x4249f0){auto& u=r.ui();return u.effect_4249f0&&u.effect_4249f0(u.effect_user,unsigned(arg));}
            if(pc==0x48f5f0)return frontend_input_action_48f5f0(object+offset,0x34,r.s.input,arg,r.s.repeat,&r,feedback,result);
            return false;};
        unsigned result{};auto w=window();
        if(!title_controller_tick_48d420(w.data(),PcFrontendWindowBytes,svc,result))fault(0x48d420);
    }
    void dialog_icon(unsigned frame,unsigned choice){
        release(0x1e9c);play(0x1e9c,0x4400d1,frame,frame,0xd,0);commit(0x1e9c);o.put32(0x1f3c,choice);}
    unsigned dialog_control(){                                                     // 4D4840
        window_tick();
        if(!frontend_window_display_48c5f0(window(),s.fonts,c.lines,c.glyphs,c.images,c.icons))fault(0x48c5f0);
        if(o.u8(0x28f8)){
            switch(input(1)){
            case 0:
                if(o.u32(0x1f3c)==3){
                    const int it=o.i32(0xbe4);const bool ok=buy(it,price(it));
                    o.put8(0xbdc,1);if(ok)s.profile[0x3f4]|=2;next_state();
                }else if(o.u32(0x1f3c)==5)next_state();
                break;
            case 1:next_state();break;
            case 3:if(o.u32(0x1f3c)!=3){dialog_icon(0,3);return 2;}break;
            case 5:if(o.u32(0x1f3c)!=5){dialog_icon(1,5);return 2;}break;
            default:break;
            }
        }else{const unsigned a=input(1);if(a<=1)next_state();}
        return 2;
    }
    void dialog_close(){                                                           // 4D49C0
        frontend_window_suspend_48ca30(window());release(0x1e9c);
        if(o.u32(0x1f3c)==3&&(s.profile[0x3f4]&2))external(0x416420);
        const unsigned back=o.u32(0xbe8);
        if(back>=4&&back<=0x13&&(back-4)%3==0)state(back);
    }
    // ---- pages -----------------------------------------------------------------
    void main_init(){                                                              // 4D4C50
        o.puti(0xbe0,0xcf);release(0xb3c);owned_show();
        list_init(0,0x60,0x28,0x2c);strip_init(0x44005e,4,0x14,0xa,1);next_state();}
    unsigned main_control(){                                                       // 4D3F80
        list_display();strip_tick();
        switch(input(1)){
        case 0:{static constexpr unsigned to[]{3,6,0xf,9,0xc};const int sel=selected();   // jump table 4D407C
            if(sel>=0&&sel<=4)state(to[sel]);return 1;}
        case 1:next_state();return 2;
        case 2:list_move(false);strip_prev();return 0;
        case 4:list_move(true);strip_next();return 0;
        default:return 0;
        }
    }
    void leave_main(){external(0x401030,{0});release(0xb3c);list_clear();strip_release();}   // 4D4090
    void records(bool colours){
        for(unsigned i=0;i<30;++i){
            auto r=rec(i);const unsigned model=colours?ShowroomColourModels5cbe70[i]:ShowroomCars5cbd80[i][0];
            unsigned index=0;for(unsigned k=0;k<30;++k)if(VehicleMenuModels[k]==model){index=k;break;}
            r.put32(0xc,index);r.put8(0,std::uint8_t(vehicle_unlocked_4c8f90(s.profile,index)));
            for(unsigned k=0;k<8;++k)r.put8(0x12+k,vehicle_colour_unlocked_4c8fc0(s.profile,index,k)?std::uint8_t(k+1):0xff);
            r.put8(0x1a,0);
            if(colours){r.put8(0x11,1);for(unsigned k=0;k<8;++k)if(r.u8(0x12+k)!=0xff){r.put8(0x11,r.u8(0x12+k));break;}
                r.put32(4,model);}
            else{if(model>=ShowroomModelColours5b39f4.size())fault(0x46bd20);
                r.put8(0x11,std::uint8_t(ShowroomModelColours5b39f4[model]));r.put32(4,model);r.put32(8,ShowroomCars5cbd80[i][1]);}
        }
    }
    void cars_init(){                                                              // 4D4D00
        camera(1);o.puti(0xbe0,-1);records(false);affordable();
        auto r0=rec(0);
        preview(frontend_vehicle_preview_init_48c170(preview_bytes(),r0.u32(0xc),r0.u8(0)!=0,pv()),0x48c170);
        preview_select(r0);
        list_init(1,7,0x2d,0x48);strip_init(0x44005f,0x1b,0xa,0xa,1);title_in(0x13);next_state();owned_show();
    }
    void cars_control(){                                                           // 4D4E70
        preview(frontend_vehicle_preview_tick_48c3f0(preview_bytes(),pv()),0x48c3f0);
        list_display();strip_tick();if(finished(0x938))title_hold(0x14);
        switch(input(0)){
        case 0:purchase(item(),4);break;
        case 1:next_state();break;
        case 2:list_move(false);strip_prev();preview_select(rec(unsigned(selected())));o.put8(0xbdc,1);break;
        case 4:list_move(true);strip_next();preview_select(rec(unsigned(selected())));o.put8(0xbdc,1);break;
        default:break;
        }
        const int sel=selected();if(sel<0||sel>=30)fault(0x4d4f4e);
        auto r=rec(unsigned(sel));
        if(std::uint8_t(vehicle_unlocked_4c8f90(s.profile,r.u32(0xc)))!=r.u8(0)){r.put8(0,1);affordable();preview_select(r);}
    }
    void simple_init(int base,unsigned first,unsigned last,unsigned token,int count,unsigned title){   // 4D4FB0 / 4D55E0
        o.puti(0xbe0,base);list_init(1,7,first,last);strip_init(token,count,0xa,9,1);title_in(title);next_state();owned_show();}
    void simple_control(unsigned hold,unsigned back){                              // 4D5060 / 4D5690
        list_display();strip_tick();if(finished(0x938))title_hold(hold);
        switch(input(0)){
        case 0:purchase(o.i32(0xbe0)+selected(),back);break;
        case 1:next_state();break;
        case 2:list_move(false);strip_prev();o.put8(0xbdc,1);break;
        case 4:list_move(true);strip_next();o.put8(0xbdc,1);break;
        default:break;
        }
    }
    void track(unsigned i){if(i>=ShowroomTracks68f38c.size())fault(0x4d5210);external(0x401000,{0,ShowroomTracks68f38c[i]+0x21,1});}
    void music_init(){                                                             // 4D5130
        o.put8(0xaa1,0);o.put8(0xaa0,0);track(0);external(0x42e020,{0,bits(1),1});
        simple_init(0xac,0xd9,0xed,0x44008a,0x14,0x1c);}
    void music_control(){                                                          // 4D5210
        list_display();strip_tick();if(finished(0x938))title_hold(0x1d);
        switch(input(0)){
        case 0:purchase(item(),10);break;
        case 1:next_state();break;
        case 2:{const auto v=o.u8(0xaa1);o.put8(0xaa1,v>0?std::uint8_t(v-1):0x14);track(o.u8(0xaa1));
            list_move(false);strip_prev();o.put8(0xbdc,1);break;}
        case 4:{const auto v=o.u8(0xaa1);o.put8(0xaa1,v<0x14?std::uint8_t(v+1):0);track(o.u8(0xaa1));
            list_move(true);strip_next();o.put8(0xbdc,1);break;}
        default:break;
        }
    }
    void colours_init(){                                                           // 4D5350
        o.puti(0xbe0,0x1c);camera(1);records(true);
        auto r0=rec(0),r1=rec(1);
        preview(frontend_vehicle_preview_init_48c170(preview_bytes(),r0.u32(0xc),r0.u8(0)!=0,pv()),0x48c170);
        preview(frontend_vehicle_preview_select_48c290(preview_bytes(),r1.u32(0xc),4,true,pv()),0x48c290);
        o.put8(0xbdc,1);list_init(1,7,0x49,0xb6);strip_init(0x440061,0x6d,0xa,9,1);title_in(0x1f);next_state();owned_show();
    }
    void colours_control(){                                                        // 4D54E0
        preview(frontend_vehicle_preview_tick_48c3f0(preview_bytes(),pv()),0x48c3f0);
        list_display();strip_tick();if(finished(0x938))title_hold(0x20);
        const unsigned a=input(0);
        if(a==0){purchase(item(),0xd);return;}
        if(a==1){next_state();return;}
        if(a!=2&&a!=4)return;
        list_move(a==4);if(a==4)strip_next();else strip_prev();
        const int sel=selected();if(sel<0||sel>=int(ShowroomColourRows5cba10.size()))fault(0x4d5591);
        const auto& e=ShowroomColourRows5cba10[unsigned(sel)];
        preview(frontend_vehicle_preview_select_48c290(preview_bytes(),rec(e[0]).u32(0xc),e[1],true,pv()),0x48c290);
        o.put8(0xbdc,1);
    }
    void back(unsigned title,bool cars,bool music){                                // 4D40C0 / 4D4110 / 4D4140 / 4D4180 / 4D41D0
        if(cars){camera(0);preview(frontend_vehicle_preview_suspend_48c450(preview_bytes(),pv()),0x48c450);}
        if(music)external(0x401030,{0});
        list_clear();strip_release();title_out(title);state(0);
    }
};
template<class F> bool guarded(FrontendShowroom& c,FrontendShowroomServices& s,F&& body){
    if(c.fault&&!s.missing)s.missing=c.fault;
    try{Run r(c,s);body(r);return true;}
    catch(const Fault& f){if(!c.fault)c.fault=f.pc;s.missing=f.pc;}
    catch(const std::exception&){if(!c.fault)c.fault=0x4d5880;s.missing=c.fault;}
    return false;
}
}
bool frontend_showroom_construct_4d4230(FrontendShowroom& c,FrontendSprites& pool,unsigned& repeat){
    c.fault=0;c.constructed=false;c.list.reset();c.glyphs.clear();c.images.clear();c.icons.clear();
    std::fill(c.object.begin(),c.object.end(),0);
    auto* p=c.object.data();const auto size=c.object.size();Bytes o(p,size);
    if(!title_base_construct_48f480(p,size,repeat))return false;
    o.put32(0,0x5cbee8);
    if(!title_transform_construct_48e310(p+0x380,size-0x380))return false;
    for(unsigned off:{0x7f8u,0x898u,0x938u,0x9f0u,0xb3cu,0x1e9cu,0x1f40u})
        if(!title_ui_resource_construct_465160(p+off,size-off))return false;
    if(!frontend_vehicle_preview_construct_48bf00(o.sub(0xaa4,0x94)))return false;
    if(!title_controller_construct_48c490(p+0xbec,size-0xbec,repeat))return false;
    for(unsigned off:{0x1fe0u,0x246cu})if(!title_widget_construct_48e590(p+off,size-off,repeat))return false;
    o.put32(0xbe0,0);o.put32(0x37c,0);o.put8(0xbdc,0);o.put32(8,0x2b);
    c.pool=&pool;c.ui=std::make_unique<FrontendUiResources>(FrontendUiResources{pool});
    c.constructed=true;return true;
}
bool frontend_showroom_init_4d4a30(FrontendShowroom& c,FrontendShowroomServices& s){
    return guarded(c,s,[&](Run& r){
        auto& o=r.o;o.put8(0xbdc,0);o.puti(0xbe0,0xcf);
        r.release(0x1f40);r.play(0x1f40,0x440035,0,0x14,0,1);
        r.release(0x898);r.play(0x898,0x44009a,0,0x1e,0,1);r.commit(0x898);
        r.text_init(0x246c);
        char miles[64];std::snprintf(miles,sizeof miles,"%f",double(r.points()));
        r.text_set(0x246c,miles,0,0xff3f474a);o.putf(0x246c+0x34,480.f);o.putf(0x246c+0x38,87.f);
        r.owned_show();
        const unsigned labels[]{4,0x295,~0u,~0u,0x4000,0x29a,8,0x296};unsigned result{};
        if(!r.ui().commands(0x440ea0,s.root.sub(0x51c,s.root.size()-0x51c),labels,8,s.globals,result))fault(r.ui().missing_pc?r.ui().missing_pc:0x440ea0);
        r.state(0);
    });
}
bool frontend_showroom_control_4d5880(FrontendShowroom& c,FrontendShowroomServices& s,unsigned& result){
    result=0;c.glyphs.clear();c.images.clear();c.icons.clear();
    return guarded(c,s,[&](Run& r){
        auto& o=r.o;
        driving::object_store_depth_pair_442f20(s.root,0,1);
        r.owned_tick();
        r.tick(0x1f40);
        r.external(0x4c50d0);r.external(0x4c50f0,{0});r.external(0x4c5100,{bits(1)});
        if(r.finished(0x1f40)){r.release(0x1f40);r.play(0x1f40,0x440035,0x14,0x14,0,0);r.commit(0x1f40);}
        if(r.finished(0x898)&&o.u32(0x37c)==1){r.release(0x898);r.play(0x898,0x44009a,0x1e,0x1e,0,0);r.commit(0x898);}
        switch(o.u32(0x37c)){
        case 0:r.main_init();break;
        case 1:if(r.main_control()==1)r.leave_main();break;
        case 2:r.leave_main();result=2;break;
        case 3:r.cars_init();break;
        case 4:r.cars_control();break;
        case 5:r.back(0x15,true,false);break;
        case 6:r.simple_init(0x8a,0xb7,0xd8,0x440060,0x21,0x16);break;
        case 7:r.simple_control(0x17,7);break;
        case 8:r.back(0x18,false,false);break;
        case 9:r.music_init();break;
        case 10:r.music_control();break;
        case 11:r.back(0x1e,false,true);break;
        case 12:r.colours_init();break;
        case 13:r.colours_control();break;
        case 14:r.back(0x21,true,false);break;
        case 15:r.simple_init(0xc1,0xee,0xfb,0x4400a4,0xd,0x19);break;
        case 16:r.simple_control(0x1a,0x10);break;
        case 17:r.back(0x1b,false,false);break;
        case 21:case 22:if(r.dialog_control()==0)r.state(0x16);break;
        case 23:r.dialog_close();break;
        default:break;
        }
    });
}
bool frontend_showroom_display_4d4350(FrontendShowroom& c,FrontendShowroomServices& s){
    return guarded(c,s,[&](Run& r){
        auto& o=r.o;
        // 48EE80("%d") then 48EF30(-1): +458 is the format length, as on the PC.
        const auto show=[&](unsigned off,int value,bool draw){
            auto w=o.sub(off,PcTextWidgetBytes);const auto v=std::to_string(value);
            for(std::size_t i=0;i<v.size();++i)w.put8(0x4e + i,std::uint8_t(v[i]));
            w.put8(0x4e + v.size(),0);w.put32(0x458,2);w.put32(0x474,~0u);
            if(draw&&!frontend_text_display_48f3c0(w,s.fonts,c.lines,c.glyphs))fault(0x48f3c0);
        };
        show(0x246c,Run::ftol(r.points()),true);
        show(0x1fe0,int(r.price(r.item())),o.u32(0xb38)!=1&&o.i32(0xbe0)!=0xcf);
    });
}
bool frontend_showroom_suspend_4d4b70(FrontendShowroom& c,FrontendShowroomServices& s){
    if(!c.constructed)return true;
    return guarded(c,s,[&](Run& r){
        auto& o=r.o;
        r.camera(0);r.state(2);r.list_clear();
        if(r.preview_bytes().u32(0))r.preview(frontend_vehicle_preview_suspend_48c450(r.preview_bytes(),r.pv()),0x48c450);
        r.owned_hide();
        for(unsigned off:{0x898u,0x938u,0x7f8u,0xb3cu,0x1f40u,0x1e9cu})r.release(off);
        r.external(0x401030,{0});r.release(0xb3c);r.list_clear();r.strip_release();
        o.put8(0x1fe0+0x4c,0);o.put8(0x246c+0x4c,0);   // text vtable +10 (48EE60)
        r.external(0x4c5110);
    });
}
bool frontend_showroom_destroy_4d5760(FrontendShowroom& c,FrontendShowroomServices& s){
    if(!c.constructed)return true;
    Bytes(c.object.data(),c.object.size()).put32(0x37c,2);
    const bool ok=frontend_showroom_suspend_4d4b70(c,s);
    frontend_window_suspend_48ca30(Bytes(c.object.data(),c.object.size()).sub(0xbec,PcFrontendWindowBytes));
    c.list.reset();c.constructed=false;c.glyphs.clear();c.images.clear();c.icons.clear();
    return ok;
}
}

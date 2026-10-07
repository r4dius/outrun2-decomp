// Game modes 1/2/3/4/5 (race_modes.hpp): the boot logos (1, 2), the arcade demo
// rounds (3, 4: the attract loop's four steps), the SEGA logo (5). Network
// checks read the LAN session (7DF34C manager, 7DF100.. records): offline they
// all answer "no session".
#include "platform/race_modes.hpp"
#include "driving/pc_x87.hpp"
#include <array>
#include <cmath>
#include <cstring>
#include <stdexcept>
namespace outrun::platform {
namespace {
// Mode state words (.bss 83DAxx).
constexpr std::uint32_t Logo1Step=0x83db1cu,Logo2Step=0x83db20u,DemoStep=0x83db24u,Demo4Step=0x83db28u,SegaStep=0x83db2cu,
    SegaNext=0x83db04u,                      // the mode after the SEGA logo
    DemoRound=0x83daf4u,                     // 1..4, the attract loop step
    DemoCourse=0x83daf8u,DemoCar=0x83db00u,DemoMusic=0x83daebu,   // rotating picks (10 / 6 / 5)
    DemoCourseId=0x83dae9u,DemoCarId=0x83db01u,DemoMusicId=0x83daecu,
    DemoGhost=0x83dafcu,DemoSkip=0x83db10u,
    NetManager=0x7df34cu,NetPlayers=0x7df10au,NetSelf=0x7df10bu,NetReady=0x7df108u,NetCount=0x7df10eu;
struct Modes {
    PcRaceContext& c;
    PcRaceMemory& m;
    explicit Modes(PcRaceContext& cc):c(cc),m(cc.m){}
    std::uint32_t call(std::uint32_t pc,std::initializer_list<std::uint32_t> args={}){
        PcRaceCall k{};k.pc=pc;k.argc=std::uint32_t(args.size());
        unsigned i=0;for(auto a:args)k.args[i++]=a;
        if(!c.service)throw std::logic_error("modes: no service for PC call");
        return c.service(k);
    }
    void next_mode(std::uint32_t mode){call(0x43f8c0u,{mode});}
    void timer(std::uint32_t frames){call(0x43f900u,{frames});}
    void event_open(std::uint32_t event,std::uint32_t function){call(0x440110u,{event,function});}
    void event_close(std::uint32_t event){call(0x4401d0u,{event});}
    bool quit_asked(){return m.u32(0x7d6738u)||call(0x4bfb20u);}   // 453310: [7D6738]
    // Leaves for the title (Start pressed / quit): mode 7, the BGM fade, the attract restart.
    void to_title(std::uint32_t step_word,bool reset){
        next_mode(7);call(0x4249f0u,{5});
        if(reset)m.put32(step_word,0);
        call(0x4b72f0u,{1});
    }
    // Sound banks: 0x20 (licence 1) or 0x21, 0x22, 0x32 loaded and opened.
    void banks(){
        const std::uint32_t first=call(0x4532e0u)==1u?0x20u:0x21u;
        for(std::uint32_t b:{first,0x22u,0x32u}){call(0x42deb0u,{b,9});call(0x429920u,{b,9});}
    }
    // ---- LAN session checks (offline: no manager) ------------------------------
    bool net(){return m.u32(NetManager)!=0u;}
    std::uint32_t player(std::int32_t i){return call(0x42e030u,{std::uint32_t(i)});}
    std::int32_t players(){return m.i8(NetPlayers);}
    // 4552F0: a player has a positive +10 word: it is the opponent (7F1930).
    bool opponent_4552f0(){
        if(!net())return false;
        for(std::int32_t i=0;i<players();++i){
            const std::uint32_t p=player(i);
            if(m.i16(p+0x10)>0){m.put8(m.u32(0x7f1888u)+0x15,std::uint8_t(i));m.put32(0x7f1930u,p);return true;}
        }
        return false;
    }
    // 458C10: the count of players outside the ready states 1..5 (7DF10E); 1 when it drops to 0.
    bool all_left_458c10(){
        if(!net()||m.u8(NetReady))return false;
        const std::uint32_t before=m.u8(NetCount);
        std::uint32_t out=0;
        for(std::int32_t i=0;i<players();++i){
            const std::uint32_t s=m.u8(player(i)+0x28);
            if(!(s>0u&&s<=5u))++out;
        }
        m.put8(NetCount,std::uint8_t(out));
        return out==0u&&before!=0u;
    }
    std::uint32_t self_index(std::int32_t i){return i==m.i8(NetSelf)?0xffffffffu:std::uint32_t(i);}
    // 49EF40 / 49EF70: the demo round as 5 bytes (round, car, course, music, 842880).
    void save_round_49ef40(std::array<std::uint8_t,5>& r){
        r={std::uint8_t(m.u32(DemoRound)),m.u8(DemoCar),m.u8(DemoCourse),m.u8(DemoMusic),std::uint8_t(m.u32(0x842880u))};
    }
    void load_round_49ef70(const std::array<std::uint8_t,5>& r){
        m.put32(DemoRound,r[0]);m.put8(DemoCar,r[1]);m.put8(DemoCourse,r[2]);m.put8(DemoMusic,r[3]);
        m.put32(0x842880u,r[4]);
        set_picks();
    }
    void set_picks(){
        m.put8(DemoCourseId,m.u8(0x5c2438u+m.u8(DemoCourse)));
        m.put8(DemoCarId,m.u8(0x5c2450u+m.u8(DemoCar)));
        m.put8(DemoMusicId,m.u8(0x5c2480u+m.u8(DemoMusic)));
    }
    // 455880: every ready player (+28 == 2) shows the same demo round as ours; the
    // last one read becomes ours.
    bool same_round_455880(){
        if(!net()||m.u8(NetReady))return true;
        std::array<std::uint8_t,5> mine{},seen{};
        save_round_49ef40(mine);seen=mine;
        std::uint32_t ready=0,same=0;
        for(std::int32_t i=players()-1;i>=0;--i){
            const std::uint32_t p=player(std::int32_t(self_index(i)));
            if(m.u8(p+0x28)!=2u)continue;
            for(unsigned k=0;k<5;++k)seen[k]=m.u8(p+0x58+k);
            ++ready;
            if(seen==mine)++same;
        }
        load_round_49ef70(seen);
        return ready==same;
    }
    // 458C80: we are in the attract (+29 = 4) and so is everyone.
    bool all_attract_458c80(){
        if(!net()||m.u8(NetReady))return true;
        if(!same_round_455880())return false;
        m.put8(m.u32(0x7f1888u)+0x29,4);
        std::uint32_t other=0;
        for(std::int32_t i=0;i<players();++i)if(m.u8(player(std::int32_t(self_index(i)))+0x29)!=4u)++other;
        return other==m.u8(NetCount);
    }
    std::uint32_t net_waiting_455b20(){return net()?m.u8(NetCount):0u;}
    // 452C60(t): the session clock's high water mark (7D676C, added to [63960C]+38).
    void session_time_452c60(std::uint32_t t){
        const std::int32_t d=std::int32_t(t-m.u32(0x7d676cu));
        if(d>0){m.put32(0x7d676cu,t);const std::uint32_t s=m.u32(0x63960cu)+0x38;m.put32(s,m.u32(s)+std::uint32_t(d));}
    }
    // 452BE0: the ones' complement sum of n bytes (the packet checksum).
    std::uint16_t checksum_452be0(std::uint32_t p,std::uint32_t n){
        std::uint32_t sum=0;
        for(;n>1u;n-=2u,p+=2u){
            sum+=m.u16(p);
            if(std::int32_t(sum)<0)sum=(sum>>16)+(sum&0xffffu);
        }
        if(n==1u)sum+=m.u8(p);
        while(sum>>16)sum=(sum&0xffffu)+(sum>>16);
        return std::uint16_t(sum==0xffffu?sum:~sum);
    }
    // 453330: the session record's checksum (+C4 over +30..+C8); the original also
    // formats a debug line (4021B0) it never shows.
    void session_checksum_453330(){
        if(!call(0x47f110u))return;
        const std::uint32_t s=m.u32(0x63960cu);
        m.put16(s+0xc4,0);
        m.put16(s+0xc4,checksum_452be0(s+0x30,0x98));
    }
    // 45AEF0: the comm state reset (7F1958..7F196C).
    void comm_reset_45aef0(){
        if(m.u32(0x7f1964u)){call(0x440cd0u,{0x7f1964u});m.put32(0x7f1958u,0);}
        m.put32(0x7f195cu,0);m.put32(0x7f1960u,2);m.put32(0x7f196cu,0xf);
    }
    // ---- mode 1: the first logo -----------------------------------------------
    void mode1_init_49e790(){
        call(0x42deb0u,{0x13,9});call(0x455710u);
        if(call(0x4556f0u))call(0x45a840u);
    }
    void mode1_control_49e7b0(){
        switch(m.u32(Logo1Step)){
        case 0:
            if(call(0x42df70u,{0x13})){timer(0x258);event_open(3,0x2e);m.put32(Logo1Step,2);}
            return;
        case 2:
            if(call(0x43fa90u)){next_mode(0x20);m.put32(Logo1Step,0);}
            return;
        default:return;
        }
    }
    void mode1_exit_49e810(){call(0x42dfb0u,{0x13});event_close(3);}
    // ---- mode 2: the second logo, then the network lobby check ----------------------
    void mode2_init_49e830(){
        banks();
        session_time_452c60(call(0x40ecb0u));
        session_checksum_453330();
        call(0x455730u);call(0x4b6f80u);call(0x401050u);
        m.put32(DemoRound,1);
    }
    void mode2_control_49e8b0(){
        switch(m.u32(Logo2Step)){
        case 0:
            if(quit_asked()){next_mode(7);call(0x4249f0u,{5});call(0x4b7630u);call(0x4b72f0u,{1});return;}
            if(call(0x42df90u)&&call(0x4299a0u)){timer(0x4b0);event_open(3,0x2f);m.put32(Logo2Step,1);call(0x4b7630u);}
            return;
        case 1:
            call(0x45a840u);
            if(opponent_4552f0()){next_mode(6);m.put32(Logo2Step,0);call(0x4b72f0u,{1});return;}
            if(!call(0x45a920u)){call(0x42cc00u,{0x10,0x14});call(0x42cce0u,{0x5c2488u});return;}
            m.put32(Logo2Step,2);
            [[fallthrough]];
        case 2:
            if(quit_asked()){to_title(Logo2Step,true);return;}
            if(all_left_458c10()){next_mode(2);m.put32(Logo2Step,0);return;}
            if(std::int32_t(call(0x43f910u))<0xb4&&all_attract_458c80())timer(0);
            if(call(0x43fa90u)){next_mode(3);m.put32(Logo2Step,0);}
            return;
        default:return;
        }
    }
    void close_event3_49ea10(){event_close(3);}
    // ---- mode 3: the demo rounds ------------------------------------------------------
    void mode3_init_49ea20(){
        const std::uint32_t round=m.u32(DemoRound);
        m.put32(DemoGhost,0xffffffffu);
        if(round==1u){banks();return;}
        if(round<1u||round>4u)return;
        call(0x43f950u,{0});call(0x44da00u,{0,0});
        banks();
        call(0x429920u,{0x3d,9});call(0x42deb0u,{0x3d,9});
        call(0x42deb0u,{0x36,9});call(0x429920u,{0x36,9});
        call(0x42deb0u,{0x14,9});call(0x42deb0u,{0x15,9});
        call(0x49a650u);call(0x4276b0u);
        call(0x4b72f0u,{m.u32(DemoRound)==4u?2u:1u});
    }
    void mode3_control_49eb80(){
        switch(m.u32(DemoStep)){
        case 0:{
            if(quit_asked()){next_mode(7);call(0x4249f0u,{5});call(0x4b72f0u,{1});return;}
            if(!call(0x42df90u)||!call(0x4299a0u))return;
            switch(m.u32(DemoRound)){
            case 1:timer(0);break;
            case 2:timer(0xa8c);event_open(3,0x12);m.put32(DemoSkip,0);break;   // the demo race
            case 3:case 4:timer(0xa8c);event_open(3,0x13);m.put32(DemoSkip,0);break;   // the ranking / course preview
            default:break;
            }
            m.put32(DemoStep,2);
            return;}
        case 2:{
            if(quit_asked()){to_title(DemoStep,true);return;}
            if(opponent_4552f0()){next_mode(6);m.put32(DemoStep,0);call(0x4b72f0u,{1});return;}
            if(all_left_458c10()){next_mode(2);m.put32(DemoStep,0);return;}
            if(std::int32_t(call(0x43f910u))>0)return;
            switch(m.u32(DemoRound)){
            case 1:next_mode(4);break;
            case 2:case 3:next_mode(3);break;
            case 4:next_mode(2);break;
            default:break;
            }
            m.put32(DemoStep,0);
            return;}
        default:return;
        }
    }
    void mode3_exit_49ed10(){
        event_close(3);
        const std::uint32_t round=m.u32(DemoRound);
        if(round==1u){call(0x429a10u);call(0x42dfd0u);}
        else if(round>=2u&&round<=4u){
            if(round!=2u){
                call(0x401030u,{0});
                std::uint8_t course=std::uint8_t(m.u8(DemoCourse)+1u);if(course>=10u)course=0;m.put8(DemoCourse,course);
                std::uint8_t car=std::uint8_t(m.u8(DemoCar)+1u);if(car>=6u)car=0;m.put8(DemoCar,car);
                m.put8(DemoCourseId,m.u8(0x5c2438u+course));m.put8(DemoCarId,m.u8(0x5c2450u+car));
            }
            // The demo race's events and owners are closed.
            call(0x440330u,{8,0x18});call(0x440330u,{0x16a,0x15});
            for(std::uint32_t e:{0x167u,0x181u,0x186u,0x187u,0x18du,0x183u})event_close(e);
            call(0x44fd00u);call(0x42ebf0u);call(0x4489f0u);call(0x44fcc0u,{0});call(0x4489c0u);call(0x44c3d0u);call(0x44a1a0u);
            for(std::uint32_t k=0;k<4;++k)call(0x43de50u,{k});
            for(std::uint32_t k=0;k<3;++k)call(0x4f11b0u,{k});
            for(std::uint32_t k=0;k<3;++k)call(0x4f0600u,{k});
            call(0x46fc30u,{0});call(0x46fc30u,{1});
            call(0x4f2210u);call(0x429a10u);call(0x42dfd0u);
            std::uint8_t music=std::uint8_t(m.u8(DemoMusic)+1u);if(music>=5u)music=0;m.put8(DemoMusic,music);
            m.put8(DemoMusicId,m.u8(0x5c2480u+music));
        }
        const std::uint32_t next=m.u32(DemoRound)+1u;
        m.put32(DemoRound,std::int32_t(next)>=5?1u:next);
    }
    void mode4_init_49efe0(){banks();}
    // ---- mode 4: the demo's second leg (the countdown voices) ---------------------------
    void mode4_control_49f040(){
        switch(m.u32(Demo4Step)){
        case 0:
            if(quit_asked()){next_mode(7);call(0x4249f0u,{5});call(0x4b72f0u,{1});return;}
            if(call(0x42df90u)&&call(0x4299a0u)){m.put32(Demo4Step,2);call(0x4b72f0u,{1});}
            return;
        case 2:{
            if(quit_asked()){to_title(Demo4Step,true);return;}
            if(opponent_4552f0()){next_mode(1);m.put32(Demo4Step,0);call(0x4b72f0u,{1});return;}
            if(all_left_458c10()){next_mode(2);m.put32(Demo4Step,0);return;}
            const std::int32_t left=0x384-std::int32_t(call(0x43f910u));
            if(m.u32(0x7d6734u)&&net_waiting_455b20()==0u){
                // 15 s and 1 s before the end: a voice at the bank's volume.
                if(left==0x3c){call(0x401000u,{1,0x20,0});call(0x42e020u,{1,call(0x427aa0u,{0x20,0}),0});}
                else if(left==0x1e0){call(0x401000u,{0,2,1});call(0x42e020u,{0,call(0x427aa0u,{2,0}),0});}
            }
            if(call(0x43fa90u)){next_mode(3);m.put32(Demo4Step,0);}
            return;}
        default:return;
        }
    }
    void mode4_exit_49f1b0(){
        if(m.u8(0x79fb4au)&3u)call(0x49a650u);
        event_close(2);event_close(3);
    }
    // ---- mode 5: the SEGA logo -----------------------------------------------------------
    void mode5_init_49f1d0(){call(0x42deb0u,{0x13,9});}
    void mode5_control_49f1e0(){
        switch(m.u32(SegaStep)){
        case 0:
            if(call(0x42df70u,{0x13})){timer(0xb4);event_open(3,0x31);m.put32(SegaStep,2);}
            return;
        case 2:
            if(quit_asked()){next_mode(7);call(0x4249f0u,{5});m.put32(SegaStep,0);call(0x4b72f0u,{1});return;}
            if(opponent_4552f0()){next_mode(1);m.put32(SegaStep,0);call(0x4b72f0u,{1});return;}
            if(all_left_458c10()){m.put32(SegaNext,2);timer(0xb4);return;}
            if(call(0x43fa90u)){next_mode(m.u32(SegaNext));m.put32(SegaStep,0);}
            return;
        default:return;
        }
    }
    void mode5_exit_49f2d0(){m.put32(SegaNext,2);call(0x42dfb0u,{0x13});event_close(3);}
    // ---- the demo route's exit to the game (mode 8 prepare) -------------------------------
    void demo_exit_49f2f0(){
        if(m.u8(0x79fb4au)&3u)call(0x49a650u);
        call(0x428600u);call(0x44fd00u);call(0x42ebf0u);call(0x4489f0u);call(0x440240u);call(0x4489c0u);
        call(0x42dfd0u);call(0x429a10u);
        comm_reset_45aef0();
        call(0x487240u);call(0x401050u);call(0x427630u);call(0x40ec60u,{0});
        event_close(3);event_open(3,0x49);
    }
    // ---- event 3 functions 0x2E / 0x2F / 0x30 / 0x31 / 0x49: the logos -------------------
    // Locals the logo draws pass to the 2D services by pointer (mapped while they run).
    static constexpr std::uint32_t LogoLocal=0x7fff2000u;
    // 42CB90(x, y): the text cell (956BB4..956BBA): the font cell (956BBC / 956BBE)
    // scaled by 956BC4 / 956BC8, times x / y.
    void text_cell_42cb90(std::uint32_t x,std::uint32_t y){
        using driving::X87;
        const std::uint32_t w=std::uint16_t(driving::x87_ftol64(X87(std::int32_t(m.i16(0x956bbcu)))*X87(m.f32(0x956bc4u))))*x;
        m.put16(0x956bb8u,std::uint16_t(w));m.put16(0x956bb4u,std::uint16_t(w));
        const std::uint32_t h=std::uint32_t(driving::x87_ftol64(X87(std::int32_t(m.i16(0x956bbeu)))*X87(m.f32(0x956bc8u))))*y;
        m.put16(0x956bbau,std::uint16_t(h));m.put16(0x956bb6u,std::uint16_t(h));
    }
    // 42D1A0(token, x, y, scale): a sprite image record (42CFE0) at (x, y).
    void image_42d1a0(std::uint32_t token,std::int32_t x,std::int32_t y,float scale){
        std::array<std::uint8_t,0x48> rec{};
        auto put=[&](std::size_t at,std::uint32_t v){std::memcpy(rec.data()+at,&v,4);};
        auto putf=[&](std::size_t at,float v){std::memcpy(rec.data()+at,&v,4);};
        put(0,token);putf(0x14,1.0f);putf(0x18,1.0f);putf(0x24,float(x));putf(0x28,float(y));put(0x2c,0xffffffffu);
        const auto mark=m.mark();m.map(LogoLocal,rec.data(),rec.size());
        std::uint32_t bits;std::memcpy(&bits,&scale,4);
        try{call(0x42cfe0u,{LogoLocal,bits});}catch(...){m.release(mark);throw;}
        m.release(mark);
    }
    // The fade value of logos A / C: a float work allocated at init.
    void fade_init(){call(0x40ec60u,{0xffffffffu});m.putf(call(0x440a60u,{4,0}),0.0f);}
    // 4AEF80: the fade of logo A by the mode timer (in 0x492.., out 0x3DE.., in 0x3A2.., ...)
    // into the screen fade alpha 842108 while a logo is fully shown.
    void logo_a_control_4aef80(std::uint32_t work){
        const std::int32_t t=std::int32_t(call(0x43f910u));
        const float in=m.f32(0x5c4e94u),out=m.f32(0x5a91b8u),hold=m.f32(0x5c4e90u);
        float step=0.0f;
        if(t>=0x492)step=in;
        else if(t>=0x3de)step=out;
        else if(t>=0x3c0)step=hold;
        else if(t>=0x3a2)step=in;
        else if(t>=0x2ee)step=out;
        else if(t>=0x2d0)step=hold;
        else if(t>=0x2b2)step=in;
        else if(t>=0x1fe)step=out;
        else if(t>=0x1e0)step=hold;
        else if(t>=0x1c2)step=in;
        else if(t>=0x10e)step=out;
        else if(t>=0xf0)step=hold;
        const float v=m.f32(work)+step,top=m.f32(0x5c4e8cu);
        if(!(std::isunordered(v,top)||v<top))m.putf(work,top);   // COMISS / JB: below or unordered
        else if(0.0f>v)m.putf(work,0.0f);
        else m.putf(work,v);
        if(t>=0x492||(t<0x2ee&&t>=0x2d0))
            m.put32(0x842108u,std::uint32_t(driving::x87_ftol64(driving::X87(m.f32(work))))<<24);
    }
    void logo_b_init_4af090(){
        fade_init();
        m.put32(0x68479cu,call(0x428320u,{call(0x4532e0u)==1u?0x200000u:0x210000u,0,1}));
    }
    void logo_b_destroy_4af0e0(){
        call(0x4285a0u,{m.u32(0x68479cu)});m.put32(0x68479cu,0xffffffffu);
        call(0x440b20u);
    }
    void logo_c_init_4af120(){
        fade_init();
        m.put32(0x842100u,2);
        call(0x49a650u,{m.u32(0x6847a8u),1});
    }
    // 4AF160: logo C's sprite (6847A8 table, entry 842100) until 6847B0 frames are left.
    void logo_c_control_4af160(){
        const std::int32_t left=0x384-std::int32_t(call(0x43f910u));
        const std::int32_t at=m.i32(0x6847b0u);
        const std::uint32_t sprite=m.u32(0x6847a8u+m.u32(0x842100u)*4u);
        if(at>left)call(0x4289b0u,{sprite,1,0,0});
        else if(at==left)call(0x428320u,{sprite,1,1});
        else if(left==0xf0)call(0x428320u,{0x220002u,1,1});
    }
    void logo_c_destroy_4af1d0(){
        call(0x401030u,{1});call(0x49a650u);call(0x428600u);call(0x440b20u);call(0x4b7630u);call(0x4b72f0u,{1});
    }
    // 4AF200: back to mode 8 when a mode is asked for.
    void logo_mode_4af200(){
        if(m.u32(0x78026cu)!=8u&&call(0x43f8e0u))next_mode(8);
    }
    // 4AF220: the caption of logo D (5C4C00 strings, entry 842104), centred on 25 columns.
    void logo_d_display_4af220(){
        call(0x42ca60u,{0});call(0x42ccb0u,{0xe});
        const std::uint32_t text=m.u32(0x5c4c00u+m.u32(0x842104u)*4u);
        std::uint32_t n=0;while(m.u8(text+n))++n;
        std::int32_t x=0x19-std::int32_t(n>>1);if(x<3)x=3;
        text_cell_42cb90(std::uint32_t(x),0x14);
        call(0x42cce0u,{0x6270ecu,m.u32(0x5c4c00u+m.u32(0x842104u)*4u)});
    }
    // 4AF280: logo E, a full-screen image (42D0C0) through an identity transform.
    void logo_e_display_4af280(){
        std::array<std::uint8_t,0xb8> rec{};
        auto put=[&](std::size_t at,std::uint32_t v){std::memcpy(rec.data()+at,&v,4);};
        auto putf=[&](std::size_t at,float v){std::memcpy(rec.data()+at,&v,4);};
        const float one=m.f32(0x62806cu),half=m.f32(0x5c4e98u),k=m.f32(0x6280dcu);
        put(4,0xffffffffu);put(8,0x18010u);
        const std::uint32_t image=call(0x47f110u);
        if(!image)return;
        put(0xc,image);put(0x10,1);
        for(unsigned i=0;i<4;++i)putf(0x14+i*0x14,one);                     // the identity matrix +14..+53
        for(std::size_t at:{0x84u,0x8cu,0x90u,0x98u})putf(at,one);
        putf(0x64,k);putf(0x6c,half);putf(0x78,half);putf(0x7c,k);
        const auto mark=m.mark();m.map(LogoLocal,rec.data(),rec.size());
        try{call(0x42d0c0u,{LogoLocal,0});}catch(...){m.release(mark);throw;}
        m.release(mark);
    }
};
}
bool race_modes_callback(PcRaceContext& c,std::uint32_t callback){
    Modes x(c);
    switch(callback){
    case 0x49e790u:x.mode1_init_49e790();return true;
    case 0x49e7b0u:x.mode1_control_49e7b0();return true;
    case 0x49e810u:x.mode1_exit_49e810();return true;
    case 0x49e830u:x.mode2_init_49e830();return true;
    case 0x49e8b0u:x.mode2_control_49e8b0();return true;
    case 0x49ea10u:x.close_event3_49ea10();return true;
    case 0x49ea20u:x.mode3_init_49ea20();return true;
    case 0x49eb80u:x.mode3_control_49eb80();return true;
    case 0x49ed10u:x.mode3_exit_49ed10();return true;
    case 0x49efe0u:x.mode4_init_49efe0();return true;
    case 0x49f040u:x.mode4_control_49f040();return true;
    case 0x49f1b0u:x.mode4_exit_49f1b0();return true;
    case 0x49f1d0u:x.mode5_init_49f1d0();return true;
    case 0x49f1e0u:x.mode5_control_49f1e0();return true;
    case 0x49f2d0u:x.mode5_exit_49f2d0();return true;
    case 0x49f2f0u:x.demo_exit_49f2f0();return true;
    default:return false;
    }
}
bool race_logos_callback(PcRaceContext& c,std::uint32_t callback,std::uint32_t work){
    Modes x(c);
    switch(callback){
    case 0x4aef30u:case 0x4af090u:case 0x4af120u:
        if(callback==0x4aef30u)x.fade_init();else if(callback==0x4af090u)x.logo_b_init_4af090();else x.logo_c_init_4af120();
        return true;
    case 0x4aef50u:x.call(0x440b20u);return true;
    case 0x4aef60u:x.image_42d1a0(0x130001u,-192,-16,4.0f);return true;
    case 0x4aef80u:x.logo_a_control_4aef80(work);return true;
    case 0x4af0e0u:x.logo_b_destroy_4af0e0();return true;
    case 0x4af100u:x.image_42d1a0(0x130000u,-192,-16,4.0f);return true;
    case 0x4af160u:x.logo_c_control_4af160();return true;
    case 0x4af1d0u:x.logo_c_destroy_4af1d0();return true;
    case 0x4af200u:x.logo_mode_4af200();return true;
    case 0x4af220u:x.logo_d_display_4af220();return true;
    case 0x4af280u:x.logo_e_display_4af280();return true;
    default:return false;
    }
}
}

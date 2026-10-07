    // ==== the session object (7D68AC): +4 host-only flag, +5 active, +7 host, +C the
    // listener chain (next at +10), +14 the player count, +18.. the player objects
    // (7D6958 + k * 0x870: +0 word id, +8 state, +18 its weight, +240 ready,
    // +244 / +248 / +24C its address, port and translated address) ===============
    std::uint32_t player_at(std::uint32_t ses,std::uint32_t i){return u32(ses+0x18u+i*4u);}
    std::int32_t player_count(std::uint32_t ses){return i32(ses+0x14u);}
    // 453C10(ecx = session, listener): appended to the listener chain.
    void add_listener_453c10(std::uint32_t ses,std::uint32_t l){
        std::uint32_t at=ses+0xcu;
        while(u32(at))at=u32(at)+0x10u;
        p32(at,l);
    }
    // 453C40(ecx = session): every listener's vtable +4 (its reset).
    void reset_listeners_453c40(std::uint32_t ses){
        for(std::uint32_t o=u32(ses+0xcu);o;o=u32(o+0x10u))vcall(o,4);
    }
    // 453CD0(ecx = session): a host-only session: 830BF0 when it is set; else the first
    // listener (the lobby one 830BEC only while active) with a pending +4: its +4.
    std::uint32_t pending_453cd0(std::uint32_t ses){
        if(u8(ses+4u))return u32(0x830bf0u)?0x830bf0u:0u;
        for(std::uint32_t o=u32(ses+0xcu);o;o=u32(o+0x10u)){
            if(o==0x830becu&&!u8(ses+5u))continue;
            if(u32(o+4u))return o+4u;
        }
        return 0;
    }
    // 453DF0(ecx = session): the first of the ten player objects not in the session, set up
    // (4F4BD0) and added; 0 when all are in use.
    std::uint32_t new_player_453df0(std::uint32_t ses){
        const std::int32_t n=player_count(ses);
        for(std::uint32_t k=0;k<10u;++k){
            const std::uint32_t p=0x7d6958u+k*0x870u;
            std::int32_t i=0;
            while(i<player_count(ses)&&player_at(ses,std::uint32_t(i))!=p)++i;
            if(i!=n)continue;
            method(0x4f4bd0u,p);
            p32(ses+0x18u+u32(ses+0x14u)*4u,p);
            p32(ses+0x14u,u32(ses+0x14u)+1u);
            return p;
        }
        return 0;
    }
    // 453EE0(ecx = session, address): the player with the address's id word (+6), else the
    // one at that address / port (+244, or +24C when +244 is the local 7D6890); 0 if none.
    std::uint32_t find_player_453ee0(std::uint32_t ses,std::uint32_t addr){
        const std::uint16_t id=u16(addr+6u);
        if(id)for(std::int32_t i=0;i<player_count(ses);++i)
            if(u16(player_at(ses,std::uint32_t(i)))==id)return player_at(ses,std::uint32_t(i));
        for(std::int32_t i=0;i<player_count(ses);++i){
            const std::uint32_t p=player_at(ses,std::uint32_t(i));
            const std::uint32_t local=u32(0x7d6890u);
            const std::uint32_t at=(local&&local==u32(p+0x244u))?u32(p+0x24cu):u32(p+0x244u);
            if(at==u32(addr)&&u16(p+0x248u)==u16(addr+4u))return p;
        }
        return 0;
    }
    // 453F80(ecx = session, &used, &free): the weights (+18) of the players in states other
    // than 0 / 7, and [65A7A7] + 2 - that.
    void weights_453f80(std::uint32_t ses,std::uint32_t used,std::uint32_t free){
        p32(used,0);
        for(std::int32_t i=0;i<player_count(ses);++i){
            const std::uint32_t p=player_at(ses,std::uint32_t(i));
            const std::uint32_t st=u32(p+8u);
            if(st!=0u&&st!=7u)p32(used,u32(used)+u32(p+0x18u));
        }
        p32(free,std::uint32_t(std::int32_t(i8(0x65a7a7u))-i32(used)+2));
    }
    // 453FE0(&id): the next player id (14 bits, never 0, times 4) not used in the session.
    void new_id_453fe0(std::uint32_t out){
        for(;;){
            std::uint32_t id;
            do{id=(u32(0x7d68b8u)+1u)&0x3fffu;p32(0x7d68b8u,id);}while(!id);
            const std::uint32_t ses=u32(0x7d68acu);
            bool used=false;
            if(ses)for(std::int32_t i=0;i<player_count(ses);++i)
                if(u16(player_at(ses,std::uint32_t(i)))==id*4u){used=player_at(ses,std::uint32_t(i))!=0u;break;}
            if(!used){p16(out,id*4u);return;}
        }
    }
    // 454060(ecx = player): its colour +90 from the 63D9C8 table by +60 (entry 0 outside
    // the network screens' flag 1).
    void colour_454060(std::uint32_t p){
        p32(p+0x90u,(net_flags()&1u)?u32(0x63d9c8u+u32(p+0x60u)*4u):u32(0x63d9c8u));
    }
    // 454100(k, x): the LAN menu: 0 closes the lobby (430630 on 988DC8, the session's
    // vtable +8), 1 opens the list (431B10(1, x)).
    void lan_menu_454100(const std::vector<std::uint32_t>& a){
        const std::uint32_t k=a.empty()?0u:a[0];
        if(k==0u){method(0x430630u,0x988dc8u);p8(0x988e74u,0);vcall(u32(0x7d68acu),8);return;}
        if(k==1u){std::vector<std::uint32_t> b(a);b[0]=1;call_list(0x431b10u,b);}
    }
    // 454140(k, x): 0 restarts the lobby (430630 / 4306D0(0) / 4306A0 on 988DC8), 1 435AE0(x).
    void lan_menu_454140(std::uint32_t k,std::uint32_t x){
        if(k==0u){method(0x430630u,0x988dc8u);method(0x4306d0u,0x988dc8u,{0u});method(0x4306a0u,0x988dc8u);return;}
        if(k==1u)call(0x435ae0u,{x});
    }
    // 454180(k, x): the selected entry [659934] (when [659930] allows) to the session's
    // vtable +10 with x; for 0 also the 830C08 object's +C and 430B00 on 988E8C.
    void lan_menu_454180(std::uint32_t k,std::uint32_t x){
        const std::int32_t has=i32(0x659930u);const std::uint32_t sel=u32(0x659934u);
        if(k==0u){
            if(!has)return;
            vcall(u32(0x7d68acu),0x10,{sel,x});
            vcall(u32(0x830c08u),0xc);
            method(0x430b00u,0x988e8cu);
            return;
        }
        if(k==1u&&(has>0||sel==0xffffffffu))vcall(u32(0x7d68acu),0x10,{sel,x});
    }
    // ==== the LAN session 988DC8 (vtable 627BC8) ===========================================
    // 4F58F0(ecx = record): a race record 7D68E0 reset (ids 0, times -1).
    void record_reset_4f58f0(std::uint32_t r){p16(r,0);p16(r+2u,0);p32(r+4u,0xffffffffu);p32(r+8u,0xffffffffu);p8(r+0xcu,0);p32(r+0x10u,0);}
    // 454C60(ecx = session, host): a session starts (the online one is not handled here):
    // players, race records, the seven host writers, the start counters cleared; colour,
    // timers, active; this machine's player made (453DF0, 4F4590) at the local address;
    // a host also hosts (id 453FE0, state 2).
    void start_session_454c60(std::uint32_t s,std::uint32_t host){
        p32(s+0x14u,0);p32(s+0x54u,0xffffffffu);p32(s+0x58u,0);p32(s+0x5cu,0);p32(s+0x60u,0);p8(s+0x64u,0);
        for(std::uint32_t r=0x7d68e0u;r<0x7d6958u;r+=0x14u)record_reset_4f58f0(r);
        for(std::uint32_t k=0;k<7u;++k){
            const std::uint32_t w=0x63afb0u+k*0x530u;
            p32(s+0x68u+k*4u,0);p16(w+0x14u,0);p8(w+0x16u,0);p32(w+0x10u,0);p32(w+8u,0);
        }
        p32(0x7d68a4u,0);p32(0x850b30u,0);p32(0x850b34u,0);p32(0x7d68a8u,0);p32(0x7d688cu,0);p32(0x63aa7cu,0);
        p16(0x63d414u,0);p8(0x63d416u,0);p32(0x63d400u+0x10u,0);p32(0x63d400u+8u,0);
        p8(s+0x84u,0);p8(s+0x85u,0);p32(s+0x86u,0);p32(s+0x8au,0);
        colour_454060(s);
        p8(s+7u,host);
        p32(s+0x94u,0xe10);p32(s+0x98u,0xe10);p32(s+0x9cu,0);p32(s+0xa0u,2);p32(s+0xa4u,0xffffffffu);
        p8(s+5u,1);p8(s+6u,0);
        const std::uint32_t p=new_player_453df0(s);
        p32(s+0x58u,p);
        own_player_4f4590(p);
        address_4917a0(p+0x244u,0x7d6894u,0xa1ef,0,0x7d6894u);
        if(host&0xffu){
            p32(s+0x5cu,u32(s+0x58u));
            Frame L(*this,0x10);
            new_id_453fe0(L(0));
            set_id_4f46a0(u32(s+0x58u),u16(L(0)));
            p32(u32(s+0x58u)+8u,2);
        }
    }
    // 430630(ecx = session): made current (its five listeners added once), host flag clear,
    // the listeners reset (453C40).
    void make_current_430630(std::uint32_t s){
        if(!u32(s+0xcu))for(std::uint32_t l:{0x988db4u,0x988e78u,0x988e8cu,0x988eb4u,0x830becu})add_listener_453c10(s,l);
        p32(0x7d68acu,s);p8(s+4u,0);
        reset_listeners_453c40(s);
    }
    // 430690 / 4306A0 / 430700 / 4306D0 / 430740: the session's vtable +8 (host: 454C60(1)),
    // +C (the search 988E90 restarts, 60 frames), +10(i, -) (join slot i: its address
    // 7DBDD8 + i * 0x90 to 7D689C, 988EB4 started), +28(ip) (the 988EA8 address), +2C(addr)
    // (join that address).
    void host_430690(){start_session_454c60(0x988dc8u,1);}
    void search_4306a0(){p8(0x988ea4u,0);p32(0x659930u,0);p32(0x988ea0u,0x3c);clear_event_48f8e0(0x988e90u);p32(0x988e90u,1);}
    void join_slot_430700(std::uint32_t i){
        p32(0x988ed0u,i);
        p32(0x7d689cu,u32(0x7dbdd8u+i*0x90u));
        p32(0x7d6898u,0xa1ef);
        p32(0x830c08u,0x988eb4u);vcall(0x988eb4u,0xc);
    }
    void address_4306d0(std::uint32_t ip){Frame L(*this,0x10);p32(L(0),ip);address_4917a0(0x988ea8u,L(0),0xa1ef,0,0);}
    void join_address_430740(std::uint32_t a){
        p32(0x988ed0u,0xffffffffu);
        p32(0x7d689cu,u32(a));p32(0x7d6898u,u16(a+4u));
        p32(0x830c08u,0x988eb4u);vcall(0x988eb4u,0xc);
    }
    // 453D20(ecx = session, flag): [7D68BF] = flag, then vtable +14 (active) or +18.
    void leave_453d20(std::uint32_t s,std::uint32_t flag){p8(0x7d68bfu,(flag&0xffu)?1u:0u);vcall(s,u8(s+5u)?0x14u:0x18u);}
    // 453DA0(ecx = session): host-only (+4), 830BF0 pending event 1, [830C00] set.
    void host_only_453da0(std::uint32_t s){p8(s+4u,1);p8(0x830c00u,1);clear_event_48f8e0(0x830bf0u);p32(0x830bf0u,1);}
    // 453D50(ecx = session): closed: (with [7D68BF]) the listeners reset; no current session,
    // the CommRace player count 0 (456CE0).
    void close_453d50(std::uint32_t s){
        if(u8(0x7d68bfu)){
            for(std::uint32_t o=u32(u32(0x7d68acu)+0xcu);o;o=u32(o+0x10u))vcall(o,4);
            p8(0x7d68bfu,0);
        }
        p8(s+4u,0);p32(0x7d68acu,0);p32(0x7de418u,0);
    }
    // 453DC0(ecx = session): every player's +294 = 0x80000000 (4F4580).
    void mark_players_453dc0(std::uint32_t s){for(std::int32_t i=0;i<i32(s+0x14u);++i)p32(player_at(s,std::uint32_t(i))+0x294u,0x80000000u);}
    // 4305B0(ecx = 988DB4): after 60 frames, in the lobby screens (state 2) its event cleared.
    void lobby_tick_4305b0(std::uint32_t o){
        const std::uint32_t n=u32(0x98abdcu);p32(0x98abdcu,n+1u);
        if(std::int32_t(n)<=0x3c)return;
        if(net_flags()==2u)clear_event_48f8e0(o+4u);
        p32(0x98abdcu,0);
    }

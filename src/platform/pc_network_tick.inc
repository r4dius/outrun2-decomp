    // ==== the frame tick 454670 and the session / service state machines ===================
    // Globals: [7D68AC] / [7D68B0] the sessions, 83612C the service object (+4 its pending
    // event, +14 its state), 659930 the frontend network state (+14 its flags), 98A6FC the
    // online (Demonware) client, 98AB04 the lobby request.
    std::uint32_t vcall(std::uint32_t obj,std::uint32_t slot,std::initializer_list<std::uint32_t> args={}){
        return method(u32(u32(obj)+slot),obj,args);
    }
    std::uint32_t owner(){return call(0x4035f0u);}                       // the frontend owner 7B17E8
    std::uint32_t net_flags(){return u32(0x659930u+0x14u);}             // 56EE70 on 659930
    // 48F8E0(ecx = event): +0 = 0, +4 = 0, +8 = -1 (the event slot is free).
    void clear_event_48f8e0(std::uint32_t e){p32(e,0);p32(e+4,0);p32(e+8,0xffffffffu);}
    // (flags & 3) and not (flags & 4): the network screens are running.
    bool screens_on(){const std::uint32_t f=net_flags();return (f&3u)&&!(f&4u);}
    // 4F1F00(ecx = client): [+A0], the client's state.
    std::uint32_t client_state(std::uint32_t cl){return u32(cl+0xa0u);}
    // 454670: the network frame: the service (494790), the online client's update, the
    // sessions' listeners (453C60), the race start (4F5680), then the pending events of the
    // service (4942E0) and of the sessions (454260).
    void tick_454670(){
        method(0x494790u,0x83612cu);
        if(u32(0x98a6fcu)&&client_state(u32(u32(0x98a6fcu)+8u))==2u){
            method(0x531720u,u32(u32(0x98a6fcu)+8u));
            if(u8(0x7d68bcu)&&u32(u32(0x98a6fcu)+0x8cu)){
                const std::uint32_t r=method(0x526d10u,u32(u32(0x98a6fcu)+0x8cu));
                p32(0x7d68c0u,r);
                if(r==0u||r==4u){
                    if(net_flags()==2u&&u32(owner()+0x218u)!=2u)p8(0x7d68d2u,1);
                    else{
                        const std::uint32_t lobby=u32(0x98ab04u);
                        if((lobby==0u||lobby==1u)&&!u8(0x7d68d2u)&&i32(owner()+0x518u)<=0&&!u8(0x7d68bfu)){
                            p32(0x98ab1cu,0);
                            clear_event_48f8e0(0x98ab04u);
                            p32(0x98ab04u,5);
                            p32(0x98ab08u,(method(0x433c40u,0x988f40u,{1u})&0xffu)?0x3du:0x3cu);
                        }
                    }
                }
            }
        }
        if(const std::uint32_t a=u32(0x7d68acu))listeners_453c60(a);
        if(const std::uint32_t b=u32(0x7d68b0u);b&&b!=u32(0x7d68acu))listeners_453c60(b);
        if(u32(0x98a6fcu)&&client_state(u32(u32(0x98a6fcu)+8u))==2u)method(0x52fae0u,u32(u32(0x98a6fcu)+8u));
        if(u32(0x7d68acu))race_start_4f5680();
        if(!(service_event_4942e0(0x83612cu)&0xffu))return;
        std::uint32_t a=u32(0x7d68acu);
        if(a){if(!session_event_454260(a))return;a=u32(0x7d68acu);}
        if(const std::uint32_t b=u32(0x7d68b0u);b&&b!=a)(void)session_event_454260(b);
    }
    // 453C60(ecx = session): each listener (+C chain, next at +10) gets its update (vtable
    // +8), the lobby one (830BEC) only while the session is active (+5); [7D68B4] counts.
    void listeners_453c60(std::uint32_t ses){
        for(std::uint32_t o=u32(ses+0xcu);o;o=u32(o+0x10u))
            if(o!=0x830becu||u8(ses+5u))vcall(o,8);
        p32(0x7d68b4u,u32(0x7d68b4u)+1u);
    }
    // 4942E0(ecx = service): its pending event (+4): 2 (lost: 7D68CB unless 7D68D0), 4 (an
    // error: message 0x46 with the code +8); 1 when there is none.
    std::uint32_t service_event_4942e0(std::uint32_t svc){
        if(!screens_on())return 0;
        const std::uint32_t e=u32(svc+4u);
        if(!e)return 1;
        if(e==2u){clear_event_48f8e0(svc+4u);if(!u8(0x7d68d0u))p8(0x7d68cbu,1);}
        else if(e==4u){p32(0x7d68ccu,u32(svc+8u));call(0x49e4d0u,{0x46u});clear_event_48f8e0(svc+4u);}
        return 0;
    }
    // 454260(ecx = session): the session's pending event (vtable +24): 2 the host left (the
    // 830D48 state to 7D68D3), 3 / 5 messages 0x46 / 0x49, 4 an error, 6 / 7 the session
    // ended (in a race the lobby resumes; else back to the title, mode 0x3D). 1 when none.
    bool session_event_454260(std::uint32_t ses){
        if(!screens_on())return false;
        const std::uint32_t e=vcall(ses,0x24);
        if(!e)return true;
        auto done=[&]{if(!u8(0x7d68d0u))p8(0x7d68cbu,1);return false;};
        auto racing=[&]{return net_flags()==2u&&u32(owner()+0x218u)==2u;};
        switch(u32(e)){
        case 2:p8(0x7d68d3u,method(0x4920c0u,0x830d48u));p32(0x7d68ccu,u32(e+4u));clear_event_48f8e0(e);return done();
        case 3:p32(0x7d68ccu,u32(e+4u));p8(0x659951u,1);call(0x49e4d0u,{0x46u});return false;
        case 4:p32(0x7d68ccu,u32(e+4u));call(0x49e4d0u,{0x46u});clear_event_48f8e0(e);return false;
        case 5:p32(0x7d68ccu,u32(e+4u));p8(0x659951u,1);call(0x49e4d0u,{0x49u});return false;
        case 6:
            if(racing()){p32(0x7d68ccu,u32(e+4u));return done();}
            p8(0x7d68cbu,0);clear_event_48f8e0(e);return false;
        case 7:
            if(racing()&&!u8(0x7d68d2u)){
                p32(0x7d68ccu,u32(e+4u));
                if(u8(ses+5u)){vcall(ses,0x14);return done();}
                clear_event_48f8e0(e);
                if(ses==0x98a650u){
                    if(u8(0x98a648u)){method(0x431380u,0x988ed4u);method(0x4355f0u,0x98a628u);}
                    else{method(0x431380u,0x988ed4u);method(0x4355f0u,0x98a628u);}
                }else if(ses==0x988dc8u){method(0x48f900u,0x988e78u);method(0x491cc0u,0x988eb4u);}
                return done();
            }
            p8(0x7d68cbu,0);
            if(!u8(0x7d68d2u)){const std::uint32_t o=owner();if(method(0x4412c0u,o)!=0x3du)method(0x444780u,o,{0x3du});}
            clear_event_48f8e0(e);
            return false;
        default:return false;
        }
    }
    // 4F5680: the race start of a LAN session ([850B10] state): 2 the countdown armed (host:
    // 0x78 frames with every player unready, guest: 0x3C000), 10 waits for every player
    // (850B1C list) to be ready, 11 / 7 the countdown (43FA00 frames), 9 the host's 2048-
    // scaled one (4F55D0 every 6th frame); at 0 the race starts (455D30, [850B04] = 1).
    void race_start_4f5680(){
        std::uint32_t v=u32(0x850b10u);
        const std::uint32_t ses=u32(0x7d68acu);
        if(v!=0u&&v!=1u&&(!ses||!u32(ses+0x5cu))){v=1;p32(0x850b10u,1);p8(0x850b2cu,0);p8(0x850b24u,0);}
        auto go=[&](std::uint32_t left){
            p8(0x850b04u,1);p32(0x850b10u,0);p32(0x850b18u,left);
            call(0x455d30u,{left});
        };
        auto count7=[&]{
            const std::int32_t t=i32(0x850b14u)-std::int32_t(call(0x43fa00u));
            p32(0x850b14u,std::uint32_t(t));
            if(t>0)return;
            const std::uint32_t left=std::uint32_t(1-t);
            p32(0x850b18u,left);call(0x455d30u,{left});p32(0x850b10u,0);p8(0x850b04u,1);
        };
        switch(v){
        case 2:
            if(u32(u32(ses+0x5cu)+8u)==7u){p32(0x850b14u,0x3c000);p32(0x850b10u,0xb);return;}
            p8(0x850b2cu,0);
            if(u8(ses+7u)){
                p32(0x850b10u,0xa);p32(0x850b14u,0x78);
                for(std::int32_t i=0;i<i32(u32(0x7d68acu)+0x14u);++i)p8(u32(u32(0x7d68acu)+0x18u+std::uint32_t(i)*4u)+0x240u,0);
                p8(u32(u32(0x7d68acu)+0x18u)+0x240u,1);
                return;
            }
            p8(0x850b24u,0);p32(0x850b14u,0x3c000);p32(0x850b10u,0xb);return;
        case 7:count7();return;
        case 9:{
            p32(0x6aa334u,u32(0x6aa334u)-1u);
            if(u32(0x850b18u)%6u==0u)call(0x4f55d0u);
            const std::int32_t t=i32(0x850b14u)+std::int32_t(std::uint32_t(0u-call(0x43fa00u))<<11);
            p32(0x850b14u,std::uint32_t(t));
            if(t>=0x400)return;
            go(std::uint32_t(1-t/2048));
            return;}
        case 10:{
            const std::int32_t n=i32(ses+0x14u);
            std::uint32_t rec=u32(0x850b1cu);
            for(std::int32_t k=0;k<i32(0x850b34u);++k,rec+=0x20u){
                const std::uint32_t id=u32(rec);
                std::int32_t i=0;
                while(i<n&&u16(u32(ses+0x18u+std::uint32_t(i)*4u))!=id)++i;
                if(i!=n&&!u8(u32(ses+0x18u+std::uint32_t(i)*4u)+0x240u))return;
            }
            call(0x516440u);
            p32(0x850b10u,7);return;}
        case 11:
            call(0x516550u);
            if(u32(u32(u32(0x7d68acu)+0x5cu)+8u)==7u||u8(0x850b24u)){p32(0x850b10u,9);count7();}
            return;
        default:return;
        }
    }
    // 494790(ecx = service): the service's state (+14): 1 the start (sockets 491990(0) /
    // (1), the interface list 516D10 / 516DC0 into 7D6894 and the 836100 service list,
    // 830C30 ready); 2..6 / 7 / 8 the online client's sign-in; others: stopped.
    void service_494790(std::uint32_t svc){
        const std::uint32_t list=service_list_494730();
        const std::uint32_t st=u32(svc+0x14u);
        if(st==0u||st==9u)return;
        auto stopped=[&](bool all){
            if(all){p8(0x830c30u,0);p8(0x7d68c7u,0);p8(0x7d68c8u,0);}
            p8(0x7d68c9u,0);p8(0x7d68cau,0);
            clear_event_48f8e0(svc+4u);p32(svc+0x14u,0);
            method(0x48fc70u,0x659930u,{1u});
        };
        switch(st){
        case 1:{
            open_socket_491990(0);open_socket_491990(1);
            (void)interfaces_516d10(0x85b2b0u,u32(0x65b51cu));
            const std::uint32_t r=local_address_516dc0(0x85b2b0u);
            p32(0x7d6894u,r);
            p32(array_at_494370(list+0x18u,0),r);   // bdInAddr(r) (524E00 / 524CF0) into the address list
            p8(0x7d68c7u,0);p8(0x7d68c8u,1);p8(0x830c30u,1);
            clear_event_48f8e0(svc+4u);p32(svc+0x14u,0);
            return;}
        case 2:case 3:case 4:case 5:case 6:{
            method(0x435b90u,0x98a650u);
            p32(u32(0x98a6fcu)+8u,call(0x45ac10u));
            p8(list,1);
            if(method(0x531300u,u32(u32(0x98a6fcu)+8u),{list})&0xffu){p32(svc+0x14u,7);return;}
            stopped(false);return;}
        case 7:
            if(client_state(u32(u32(0x98a6fcu)+8u))==1u){method(0x5308f0u,u32(u32(0x98a6fcu)+8u));return;}
            p32(svc+0x14u,8);return;
        case 8:{
            const bool up=client_state(u32(u32(0x98a6fcu)+8u))==2u;
            p8(svc+0x19u,up?1u:0u);
            if(up){
                Frame L(*this,0x20);
                const std::uint32_t r=method(0x5300e0u,u32(u32(0x98a6fcu)+8u),{L(4)});
                p32(0x659a54u,method(0x564d90u,u32(r)));
                method(0x55e5d0u,L(4));
                p8(0x7d68c8u,1);p8(0x7d68cau,1);
                clear_event_48f8e0(svc+4u);p32(svc+0x14u,0);
                return;
            }
            p8(0x7d68c7u,0);p8(0x7d68c9u,0);p8(0x7d68cau,0);p32(svc+0x14u,0);
            p32(0x7d68ccu,0x23b);
            clear_event_48f8e0(svc+4u);p32(svc+4u,4);
            return;}
        default:stopped(true);return;
        }
    }
    // 494370(ecx = array, i): &array[i] (+0 the data, +8 the size; bdArray's bound check).
    std::uint32_t array_at_494370(std::uint32_t arr,std::uint32_t i){
        if(i>=u32(arr+8u))throw std::runtime_error("network: bdArray index out of range (494370)");
        return u32(arr)+i*4u;
    }
    // 516D10(ecx = list, socket): the interface list (SIO_GET_INTERFACE_LIST into 85AFB8,
    // 0x2F8 bytes, 0x4C per entry): the count to +0; 0 when there is none.
    std::uint32_t interfaces_516d10(std::uint32_t list,std::uint32_t sock){
        if(sock==0xffffffffu){p32(list,0);return 0;}
        Frame L(*this,0x10);
        if(std::int32_t(ws(WsaIoctl,{sock,0x4004747fu,0,0,0x85afb8u,0x2f8u,L(0),0,0}))<0){(void)ws(WsaGetLastError,{});return 0;}
        const std::uint32_t n=u32(L(0))/0x4cu;
        p32(list,n);
        return n?1u:0u;
    }
    // 516DC0(ecx = list): the first interface address outside 10.x / 192.168.x / 127.x,
    // else the first that is not loopback; 0 when none.
    std::uint32_t local_address_516dc0(std::uint32_t list){
        const std::int32_t n=i32(list);
        for(std::int32_t i=0;i<n;++i){
            const std::uint32_t a=u32(0x85afc0u+std::uint32_t(i)*0x4cu);
            const std::uint8_t b0=std::uint8_t(a),b1=std::uint8_t(a>>8);
            if(b0==10u||(b0==0xc0u&&b1==0xa8u)||b0==0x7fu)continue;
            return a;
        }
        for(std::int32_t i=0;i<n;++i){
            const std::uint32_t a=u32(0x85afc0u+std::uint32_t(i)*0x4cu);
            if(std::uint8_t(a)!=0x7fu)return a;
        }
        return 0;
    }
    // 491990(kind): the UDP socket of kind 0 / 1 / 2 ([65B51C] / [65B520] / [65B524], ports
    // 41455 / 41456 / 41457; kind 1 broadcast), non-blocking; -1 when it cannot be opened.
    std::uint32_t open_socket_491990(std::uint32_t kind){
        const std::uint32_t slot=kind==1u?0x65b520u:(kind==2u?0x65b524u:0x65b51cu);
        if(u32(slot)!=0xffffffffu)return u32(slot);
        Frame L(*this,0x20);
        const std::uint32_t sock=ws(Socket,{2u,2u,0x11u});
        p32(slot,sock);
        if(sock==0xffffffffu)return 0xffffffffu;
        auto fail=[&]{ws(CloseSocket,{u32(slot)});p32(slot,0xffffffffu);return 0xffffffffu;};
        if(kind==1u){p32(L(0x10),1);if(ws(SetSockOpt,{sock,0xffffu,0x20u,L(0x10),4u}))return fail();}
        p16(L(0),2);
        p16(L(2),ws(Htons,{kind==1u?0xa1f0u:(kind==2u?0xa1f1u:0xa1efu)}));
        p32(L(4),0);
        if(std::int32_t(ws(Bind,{u32(slot),L(0),0x10u}))<0)return fail();
        p32(L(0x14),1);
        if(std::int32_t(ws(IoctlSocket,{u32(slot),0x8004667eu,L(0x14)}))<0)return fail();
        return u32(slot);
    }

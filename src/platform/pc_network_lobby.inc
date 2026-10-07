    // ==== the LAN listener 830BEC (vtable 5C1BE8: +4 490020 reset, +8 490940 update) ======
    // The session's +58 is this machine's player, +5C the host's; a player's +C is its
    // vote state (0 idle, 1 asking, 2 counting), +10 its ready byte.
    // A stream (vtable 627C48: +0 write(data, n)) on a local buffer (490670: +4 data = +14,
    // +C capacity 2, +8 / +10 positions).
    void stream_490670(std::uint32_t st){p32(st+4u,st+0x14u);p32(st+0xcu,2);p32(st+0x10u,0);p32(st+8u,0);p32(st,0x627c48u);}
    void write_byte(std::uint32_t st,std::uint32_t v){
        Frame L(*this,0x10);
        p8(L(0),v);
        vcall(st,0,{L(0),1u});
    }
    // 490940(ecx = listener): the lobby frame of an active session (the listener idle):
    // packets in (490690), the names (490810), this player's update (4F4590); a guest
    // runs the vote (+C 2: wait for every lower id, then ask; 1: count down +A8 and accept
    // through the host 4F4CD0), the host sends the votes (0x13 messages every 6 frames);
    // then the race records (4F51E0), the counters, the timeouts (host 5163C0 / 5165F0 /
    // 454BB0, guest 515800), the start check (490290 / 4904E0 -> 4541F0), the 0x19 state
    // message to everyone and the players' flushes (4F4D80).
    void update_490940(std::uint32_t self){
        std::uint32_t s=u32(0x7d68acu);
        if(!s||!u8(s+5u)||u8(self+0x14u)||(u32(self+4u)!=0u&&u32(self+4u)!=1u)){
            method(0x490690u,self,{1u});
            if(u8(self+0x14u))method(0x490530u,self,{1u});
            p32(0x830be8u,0);
            return;
        }
        method(0x454060u,s);
        method(0x490690u,self,{0u});
        method(0x490810u,self);
        method(0x4f4590u,u32(u32(0x7d68acu)+0x58u));
        s=u32(0x7d68acu);
        auto frames=[&]{return call(0x43fa00u);};
        if(u8(s+7u))p32(u32(s+0x58u)+0xcu,0);
        else{
            const std::uint32_t me=u32(s+0x58u);
            switch(u32(me+0xcu)){
            case 2:{
                if(u32(s+0x54u)==0xffffffffu)break;
                bool wait=false;
                for(std::int32_t k=0;k<i32(s+0x54u)&&!wait;++k){
                    const std::uint16_t id=u16(s+0x40u+std::uint32_t(k)*2u);
                    if(id>u16(me))continue;
                    std::int32_t i=0;
                    for(;i<player_count(s);++i){const std::uint32_t p=player_at(s,std::uint32_t(i));if(u16(p)==id&&u32(p+8u))break;}
                    if(i==player_count(s))wait=true;
                }
                if(wait)break;
                for(std::int32_t i=0;i<player_count(s);++i){
                    const std::uint32_t p=player_at(s,std::uint32_t(i));
                    if(p==me){p32(me+0xcu,1);p32(u32(0x7d68acu)+0xa8u,0x2d0);break;}
                    const std::uint32_t v=u32(p+0xcu);
                    if(v==1u||v==2u)break;
                }
                break;}
            case 1:{
                std::uint32_t t=u32(0x830c04u)+frames();
                if(t>6u)t%=6u;
                p32(0x830c04u,t);
                s=u32(0x7d68acu);
                const std::uint32_t m2=u32(s+0x58u);
                const std::int32_t id=u16(m2);
                bool other=false;
                for(std::int32_t i=0;i<player_count(s);++i){
                    const std::uint32_t p=player_at(s,std::uint32_t(i));
                    if(p==u32(s+0x5cu))continue;
                    if(id>std::int32_t(u16(p))&&!u8(p+0x10u)){other=true;break;}
                }
                if(!other){p32(m2+0xcu,0);break;}
                p32(s+0xa8u,u32(s+0xa8u)-frames());
                if(i32(u32(0x7d68acu)+0xa8u)<=0)method(0x4f4cd0u,u32(u32(0x7d68acu)+0x5cu),{0u});
                break;}
            case 0:{
                const std::uint32_t t=u32(0x830c04u)+frames();
                p32(0x830c04u,t);
                if(t<=6u)break;
                for(std::int32_t i=0;i<player_count(u32(0x7d68acu));++i){
                    const std::uint32_t p=player_at(u32(0x7d68acu),std::uint32_t(i));
                    if(u32(p+0xcu)!=1u)continue;
                    Frame L(*this,0x30);
                    stream_490670(L(0));
                    write_byte(L(0),0x13);
                    write_byte(L(0),u8(p+0x10u)?1u:0u);
                    method(0x4f4ed0u,p,{L(0)});
                }
                p32(0x830c04u,u32(0x830c04u)%6u);
                break;}
            default:break;
            }
        }
        s=u32(0x7d68acu);
        if((net_flags()&1u)){const std::uint32_t st=u32(u32(s+0x58u)+8u);if(st&&st!=7u)call(0x4f51e0u);}
        p32(u32(0x7d68acu)+0x94u,u32(u32(0x7d68acu)+0x94u)+1u);
        p32(0x850b18u,u32(0x850b18u)+1u);
        for(std::int32_t i=0;i<player_count(u32(0x7d68acu));++i){
            const std::uint32_t p=player_at(u32(0x7d68acu),std::uint32_t(i));
            for(std::uint32_t o:{0x288u,0x28cu,0x290u})p32(p+o,u32(p+o)+1u);
        }
        s=u32(0x7d68acu);
        {const std::uint32_t me=u32(s+0x58u);
            if(u32(s+0x9cu)==u32(me+8u)&&u32(s+0xa0u)==u32(me+0xcu))p32(s+0x98u,u32(s+0x98u)+1u);
            else{p32(s+0x9cu,u32(me+8u));p32(s+0xa0u,u32(me+0xcu));p32(s+0x98u,0);}}
        s=u32(0x7d68acu);
        if(u8(s+7u)){
            call(0x5163c0u);
            std::int32_t i=0;
            while(i<i32(u32(0x7d68acu)+0x14u)){
                const std::uint32_t ses=u32(0x7d68acu);
                const std::uint32_t p=player_at(ses,std::uint32_t(i));
                if(p==u32(ses+0x58u)){++i;continue;}
                if(call(0x5165f0u,{p,std::uint32_t(i)})&0xffu){++i;continue;}
                method(0x454bb0u,ses,{p});
            }
        }else if(!(call(0x515800u)&0xffu)){
            if(s==0x98a650u)method(0x438510u,0x98ab00u);else method(0x4305b0u,0x988db4u);
            const std::uint32_t ev=self+4u;
            if(u32(ev)==0u||u32(ev)==1u){
                clear_event_48f8e0(ev);p32(ev,7);
                const std::uint32_t st=u32(u32(u32(0x7d68acu)+0x58u)+8u);
                p32(self+8u,st==0u?9u:(st!=7u?10u:8u));
            }
            return;
        }
        if(net_flags()&2u){
            if((method(0x490290u,0x659930u)&0xffu)||(i32(0x7d68a4u)>i32(0x7d68a8u)&&(method(0x4904e0u,0x659930u)&0xffu)))
                method(0x4541f0u,u32(0x7d68acu));
        }
        p16(0x63d944u,u16(0x63d938u));p8(0x63d946u,0);
        p32(0x63d930u+0x10u,0);p32(0x63d930u+8u,0);                    // 491890: the stream rewound
        write_byte(0x63d930u,0x19);
        method(0x4f4790u,u32(u32(0x7d68acu)+0x58u),{0x63d930u});
        if(u8(0x63d946u)||u32(0x63d938u)!=std::uint32_t(std::int32_t(i16(0x63d944u)))){
            for(std::int32_t i=0;i<player_count(u32(0x7d68acu));++i){
                const std::uint32_t ses=u32(0x7d68acu);
                const std::uint32_t p=player_at(ses,std::uint32_t(i));
                if(p!=u32(ses+0x58u))method(0x4f4ed0u,p,{0x63d930u});
            }
        }
        for(std::int32_t i=0;i<player_count(u32(0x7d68acu));++i){
            const std::uint32_t p=player_at(u32(0x7d68acu),std::uint32_t(i));
            if(u32(p+0x2a0u)||(u32(p+0x858u)&&i32(p+0x28cu)>7))method(0x4f4d80u,p);
        }
        p32(0x830be8u,u32(0x830be8u)+1u);
    }
    // ---- packets: a reader (+0 data, +4 length, +8 capacity, +C position, +10 the sender
    // address: +0 ip, +4 port, +6 id, +8 translated ip) and a writer (vtable 627C48 /
    // 491940: +4 data, +8 length, +C capacity, +10 position, +16 changed) --------------
    // 491860(ecx = reader, out, n): n bytes read.
    void read_491860(std::uint32_t r,std::uint32_t out,std::uint32_t n){
        for(std::uint32_t k=0;k<n;++k){p8(out+k,u8(u32(r)+u32(r+0xcu)));p32(r+0xcu,u32(r+0xcu)+1u);}
    }
    // 491900(ecx = writer, data, n): n bytes written (the length follows the position).
    void write_491900(std::uint32_t w,std::uint32_t data,std::uint32_t n){
        for(std::uint32_t k=0;k<n;++k){p8(u32(w+4u)+u32(w+0x10u),u8(data+k));p32(w+0x10u,u32(w+0x10u)+1u);}
        if(i32(w+0x10u)>i32(w+8u))p32(w+8u,u32(w+0x10u));
    }
    // 491940(ecx = writer, data, n): the same, +16 set when a byte differs.
    void write_491940(std::uint32_t w,std::uint32_t data,std::uint32_t n){
        for(std::uint32_t k=0;k<n;++k){
            const std::uint32_t at=u32(w+0x10u)+u32(w+4u);
            if(u8(data+k)!=u8(at))p8(w+0x16u,1);
            p8(at,u8(data+k));p32(w+0x10u,u32(w+0x10u)+1u);
        }
        if(i32(w+0x10u)>i32(w+8u))p32(w+8u,u32(w+0x10u));
    }
    // 491840(ecx = reader, from, n): the byte sum of n bytes.
    std::uint32_t checksum_491840(std::uint32_t r,std::uint32_t from,std::uint32_t n){
        std::uint8_t sum=0;for(std::uint32_t k=0;k<n;++k)sum=std::uint8_t(sum+u8(u32(r)+from+k));return sum;
    }
    // 4917A0(ecx = address, &ip, port, id, &translated): set.
    void address_4917a0(std::uint32_t a,std::uint32_t ip,std::uint32_t port,std::uint32_t id,std::uint32_t tr){
        p32(a,u32(ip));p16(a+4u,port);p16(a+6u,id);if(tr)p32(a+8u,u32(tr));
    }
    // 4917D0(ecx = address, writer) / 491800(ecx = address, reader): an address (4 + 2 + 4
    // bytes: ip, port, translated) written / read.
    void write_address_4917d0(std::uint32_t a,std::uint32_t w){vcall(w,0,{a,4u});vcall(w,0,{a+4u,2u});vcall(w,0,{a+8u,4u});}
    void read_address_491800(std::uint32_t a,std::uint32_t r){read_491860(r,a,4);read_491860(r,a+4u,2);read_491860(r,a+8u,4);}
    // 491A90(ecx = reader, raw, &from, -): a datagram from the socket (raw: the broadcast
    // one) into the reader; a game packet's 2-byte header (the sender id) is replaced by its
    // length. 1 when one came.
    std::uint32_t receive_491a90(std::uint32_t r,std::uint32_t raw,std::uint32_t from){
        const std::uint32_t sock=open_socket_491990((raw&0xffu)?1u:0u);
        if(sock==0xffffffffu)return 0;
        Frame L(*this,0x20);
        const std::uint32_t sa=L(0),len=L(0x10);
        p32(len,0x10);
        const std::int32_t n=std::int32_t(ws(RecvFrom,{sock,u32(r),u32(r+8u),0,sa,len}));
        if(n<=0)return 0;
        std::uint32_t id=0;
        if(!(raw&0xffu)){
            const std::uint32_t b=u32(r);
            id=(std::uint32_t(u8(b))<<8)+u8(b+1u);
            p16(b,std::uint32_t(n-2));
        }
        const std::uint32_t port=ws(Ntohs,{u16(sa+2u)});
        if(from)address_4917a0(from,sa+4u,port,id,0);
        address_4917a0(r+0x10u,sa+4u,ws(Ntohs,{u16(sa+2u)}),id,0);
        p32(r+4u,std::uint32_t(n));p32(r+0xcu,0);
        return 1;
    }
    // 491B60(ecx = writer, to, raw): the writer's bytes sent (to 0: broadcast); a game
    // packet's first two bytes carry this player's id while it is sent, then its length.
    std::uint32_t send_491b60(std::uint32_t w,std::uint32_t to,std::uint32_t raw){
        const bool r=to?((raw&0xffu)!=0u):true;
        const std::uint32_t sock=open_socket_491990(r?1u:0u);
        if(sock==0xffffffffu)return 0;
        Frame L(*this,0x10);
        const std::uint32_t sa=L(0);
        p16(sa,2);
        std::uint32_t port=0xa1ef;
        if(!to)p32(sa+4u,0xffffffffu);
        else{
            const std::uint32_t local=u32(0x7d6890u);
            if(local&&u32(to)==local)p32(sa+4u,u32(to+8u));
            else{p32(sa+4u,u32(to));port=u16(to+4u);}
        }
        p16(sa+2u,ws(Htons,{port}));
        if(!r){
            const std::uint32_t s=u32(0x7d68acu);
            const std::uint32_t id=(s&&u32(s+0x58u))?u16(u32(s+0x58u)):0u;
            p8(u32(w+4u),std::uint32_t(std::int32_t(id)>>8));p8(u32(w+4u)+1u,id);
        }
        const std::uint32_t sent=ws(SendTo,{sock,u32(w+4u),u32(w+8u),0,sa,0x10u});
        if(!r)p16(u32(w+4u),u16(w+8u)-2u);
        return sent==u32(w+8u)?1u:0u;
    }
    // 490690(ecx = listener, keep): every waiting packet: the host's discovery replies
    // (430770 on 988DC8, read past the length), else (keep = 0) the sender player marked
    // alive and the packet parsed (490030); anything else that asks for an answer (+3 set,
    // not 0x11) gets the 0x11 0 reply.
    void receive_all_490690(std::uint32_t self,std::uint32_t keep){
        Frame L(*this,0x880);
        const std::uint32_t R=L(0),A=L(0x30),W=L(0x40),two=L(0x60),buf=L(0x80);
        p32(R,buf);p32(R+4u,0);p32(R+8u,0x800);p32(R+0xcu,0);p16(R+0x14u,0xa1ef);p16(A+4u,0xa1ef);
        while(method(0x491a90u,R,{0u,A,0u})&0xffu){
            const std::uint32_t s=u32(0x7d68acu);
            if(s==0x988dc8u&&u8(s+5u)&&u8(s+7u)){
                p32(R+0xcu,2);                                    // past the length
                if(method(0x430770u,0x988dc8u,{R})&0xffu)continue;
                p32(R+0xcu,0);
            }
            if(!(keep&0xffu)){
                std::uint32_t p=method(0x453ee0u,u32(0x7d68acu),{A});
                if(p==u32(u32(0x7d68acu)+0x58u))p=0;
                else if(p){p32(p+0x254u,0);p8(p+0x10u,1);}
                if(method(0x490030u,self,{R,p})&0xffu)continue;
            }
            const std::uint32_t b=u32(R);
            if(u8(b+2u)==0x11u||!u8(b+3u))continue;
            p32(W,0x627c48u);p32(W+4u,two+0x10u);p32(W+8u,0);p32(W+0xcu,4);p32(W+0x10u,0);
            p32(two,2);
            write_491900(W,two,2);
            {Frame B(*this,0x10);p8(B(0),0x11);write_491900(W,B(0),1);p8(B(0),0);write_491900(W,B(0),1);}
            method(0x491b60u,W,{A,0u,0u});
        }
    }
    // 490030(ecx = listener, reader, player): a game packet: a 2-byte length, then messages
    // (a type byte; bit 5 / 6: the message size is in the last 1 / 2 bytes of the packet,
    // taken from its end; else the rest): 0x19 state (4F4B80), 0x17 / 0x18 / 0x1C / 0x1D
    // race (4F5910 / 4F6020 / 4F5970 / 4F59F0), 0x16 a reset (515700), 0x13 a vote, others
    // to the host (516C10, or 516CF0 / 516AC0 from strangers) or from it (515C30). 0 when
    // the packet is refused.
    std::uint32_t parse_490030(std::uint32_t self,std::uint32_t r,std::uint32_t p){
        (void)self;
        Frame L(*this,0x40);
        const std::uint32_t M=L(0),tmp=L(0x30);
        read_491860(r,tmp,2);
        const std::int32_t total0=std::int32_t(u16(tmp))+2;
        std::int32_t total=total0;
        if(i32(r+0xcu)<total){
            do{
                const std::uint32_t at=u32(r)+u32(r+0xcu);
                const std::uint32_t hdr=u8(at);
                p8(at,hdr&0x9fu);
                std::int32_t size;
                if((hdr&0x60u)==0x20u){size=u8(u32(r)+std::uint32_t(total)-1u);--total;}
                else if((hdr&0x60u)==0x40u){size=std::int32_t((std::uint32_t(u8(u32(r)+std::uint32_t(total)-1u))<<8)|u8(u32(r)+std::uint32_t(total)-2u));total-=2;}
                else size=total-i32(r+0xcu);
                const std::uint32_t type=hdr&0xffffff9fu;
                p32(M,at);p32(M+4u,std::uint32_t(size));p32(M+8u,std::uint32_t(size));p32(M+0xcu,1);p16(M+0x14u,0xa1ef);
                switch(type){
                case 0x15:return 0;
                case 0x19:if(p)method(0x4f4950u,p,{M});break;
                case 0x17:if(!p)return 0;call(0x4f5910u,{M,p});break;
                case 0x18:if(p)call(0x4f6020u,{M,p});break;
                case 0x1c:if(p)call(0x4f5970u,{M,p});break;
                case 0x1d:if(p)call(0x4f59f0u,{M,p});break;
                case 0x16:if(p)call(0x515700u,{M});return 1;
                case 0x13:
                    read_491860(M,tmp,1);
                    if(u8(tmp)==1u&&p)p8(p+0x11u,1);
                    break;
                default:{
                    const std::uint32_t s=u32(0x7d68acu);
                    if(u8(s+7u)){
                        if(p){if(!(call(0x516c10u,{type,M,p,r+0x10u})&0xffu))return 0;}
                        else if(!(call(0x516cf0u,{type,M,r+0x10u})&0xffu)&&!(call(0x516ac0u,{type,M,r+0x10u})&0xffu))return 0;
                    }else if(p==u32(s+0x5cu)){if(!(call(0x515c30u,{type,M})&0xffu))return 0;}
                    break;}
                }
                p32(r+0xcu,u32(r+0xcu)+std::uint32_t(size));
            }while(i32(r+0xcu)<total);
        }
        return i32(r+4u)>total0?0u:1u;
    }
    // ---- the player objects (7D6958 + k * 0x870): +8 state, +C vote, +18 info count, +20..
    // the infos (0x88 each), +244 address, +250 colour, the outgoing packet writer +298
    // (+29C data, +2A0 length, +2A4 capacity, +2A8 position), +7C4 the current message,
    // +7C8.. the sizes of the previous ones (count +7E8), +854 / +858 / +7EC a raw tail ----
    // 490810: one log line per new name (the slot names 7DCBE0, then the players' infos):
    // only the "shown" bytes (+80 / +A0) remain of it (the text went to a debug buffer).
    void names_490810(){
        if(!(net_flags()&2u)&&!(net_flags()&1u))return;
        for(std::int32_t i=i32(0x7dd130u)-1;i>=0;--i){
            const std::uint32_t e=0x7dcbe0u+std::uint32_t(i)*0x88u;
            if(!u8(e+0x80u)){p8(e+0x80u,1);return;}
        }
        const std::uint32_t s=u32(0x7d68acu);
        for(std::int32_t i=0;i<player_count(s);++i){
            const std::uint32_t p=player_at(s,std::uint32_t(i));
            if(p==u32(s+0x58u)||p==u32(s+0x5cu))continue;
            const std::uint32_t st=u32(p+8u);
            if(!st||st==7u)continue;
            for(std::int32_t j=0;j<i32(p+0x18u);++j)
                if(!u8(p+0xa0u+std::uint32_t(j)*0x88u)){p8(p+0xa0u+std::uint32_t(j)*0x88u,1);return;}
        }
    }
    // 4F4590(ecx = player): this machine's player: its state from the screens (2 racing /
    // 1 lobby, 4, 3, 5, 6), one info (520840) with the colour [659A54].
    void own_player_4f4590(std::uint32_t p){
        const std::uint32_t st=u32(p+8u);
        if(st&&st!=7u){
            const std::uint32_t f=net_flags();
            if(f&2u){
                const bool racing=(method(0x4412f0u,owner())&0xffu)||method(0x443040u,owner())==0x15u;
                p32(p+8u,racing?2u:1u);
            }else if(f&4u)p32(p+8u,4);
            else if(f&1u)p32(p+8u,3);
            else if(f&8u)p32(p+8u,5);
            else if(f&0x10u)p32(p+8u,6);
        }
        p32(p+0x18u,1);
        p32(p+0x250u,u32(0x659a54u));
        for(std::uint32_t i=0;std::int32_t(i)<i32(p+0x18u);++i)player_info_520840(p+0x20u+i*0x88u,i);
    }
    // 447400(ecx = license): the license rating (in mode 0x20: the seven 632E6C ranks by
    // 4E82A0, times [59DCE0]); 0 elsewhere.
    float rating_447400(std::uint32_t lic){
        if(u32(0x78026cu)!=0x20u)return f32(0x619a34u);
        float sum=0.0f;
        for(std::uint32_t i=0;i<7u;++i)sum=f32(0x632e6cu+call(0x4e82a0u,{i,lic})*4u)+sum;
        return driving::x87_float(driving::X87(sum)*driving::X87(f32(0x59dce0u)));
    }
    // 520840(ecx = info, i): this player's info: the name 830C10, its id (+28, or the
    // online one), the license name, rating, average (+44 = [7C24E4] / [7C24E8] * [6282CC]),
    // records and the car / colour / transmission choices.
    void player_info_520840(std::uint32_t info,std::uint32_t){
        for(std::uint32_t k=0;;++k){const std::uint8_t ch=u8(0x830c10u+k);p8(info+k,ch);if(!ch)break;}
        if(u32(0x7d68acu)==0x98a650u){p32(info+0x18u,u32(0x98ab20u));p32(info+0x1cu,u32(0x98ab24u));}
        else{p32(info+0x18u,u16(info+0x28u));p32(info+0x1cu,0);}
        p32(info+0x20u,0);p8(info+0x2au,0);p8(info+0x2bu,0);
        for(std::uint32_t k=0;;++k){const std::uint8_t ch=u8(0x7c23e0u+k);p8(info+0x4cu+k,ch);if(!ch)break;}
        if(net_flags()==2u)pf(info+0x40u,rating_447400(0x7c23e0u));
        {const float d=f32(0x7c24e8u);float v=0.0f;
            if(!(d==0.0f))v=(f32(0x7c24e4u)/d)*f32(0x6282ccu);
            pf(info+0x44u,v);}
        p32(info+0x48u,u32(0x7c2404u));p32(info+0x60u,u32(0x7c24f0u));p32(info+0x64u,u32(0x7c24ecu));
        p32(info+0x68u,u32(0x7c23fcu));p32(info+0x6cu,u32(0x7c2400u));p32(info+0x70u,u32(0x7c23f8u));
        p32(info+0x74u,std::uint32_t(std::int32_t(i8(0x655b59u))));
        p32(info+0x78u,std::uint32_t(std::int32_t(i8(0x655b5au))));
        p32(info+0x7cu,std::uint32_t(std::int32_t(i8(0x830374u))));
    }
    // 4F4790(ecx = player, writer): the player's state message body: id, info count,
    // address, colour, vote, then per info: name, id words, flag, rating, average, record,
    // the time words, the license name and the six choice bytes.
    void write_state_4f4790(std::uint32_t p,std::uint32_t w){
        auto put=[&](std::uint32_t a,std::uint32_t n){vcall(w,0,{a,n});};
        put(p,2);put(p+0x18u,1);
        write_address_4917d0(p+0x244u,w);
        put(p+0x250u,1);put(p+0xcu,1);
        for(std::uint32_t i=0;std::int32_t(i)<i32(p+0x18u);++i){
            const std::uint32_t f=p+0x20u+i*0x88u;
            put(f,0x10);put(f+0x18u,0x10);put(f+0x2au,1);
            put(f+0x40u,4);put(f+0x44u,4);put(f+0x48u,4);put(f+0x60u,4);put(f+0x64u,4);
            put(f+0x4cu,0x10);
            for(std::uint32_t o:{0x68u,0x6cu,0x70u,0x74u,0x78u,0x7cu})put(f+o,1);
        }
    }
    // 4F4ED0(ecx = player, writer): a message queued in the player's packet (flushed first
    // when it would not fit or eight are waiting; dropped when it can never fit); the
    // previous message's header byte gets its size kind (bit 5: 1 byte, bit 6: 2 bytes).
    void queue_4f4ed0(std::uint32_t p,std::uint32_t w){
        const std::uint32_t n=u32(w+8u),cap=u32(p+0x2a4u);
        const std::int32_t need=std::int32_t((u32(p+0x858u)?1u:0u)+u32(p+0x7e8u)*2u+2u+u32(p+0x2a0u)+n+u32(p+0x858u));
        if(need>std::int32_t(cap)||i32(p+0x7e8u)>=8){
            if(std::int32_t(n+2u)>std::int32_t(cap))return;
            flush_4f4d80(p);
        }
        if(!u32(p+0x2a0u)){Frame L(*this,0x10);vcall(p+0x298u,0,{L(0),2u});}
        else{
            const std::uint32_t prev=u32(p+0x7c4u);
            const std::int32_t len=i32(p+0x2a0u)-std::int32_t(prev);
            const std::uint32_t at=prev+u32(p+0x29cu);
            p8(at,u8(at)|(len<0x100?0x20u:0x40u));
            p32(p+0x7c8u+u32(p+0x7e8u)*4u,std::uint32_t(len));
            p32(p+0x7e8u,u32(p+0x7e8u)+1u);
        }
        p32(p+0x7c4u,u32(p+0x2a0u));
        for(std::uint32_t k=0;k<u32(w+8u);++k)p8(u32(p+0x29cu)+u32(p+0x2a0u)+k,u8(u32(w+4u)+k));
        p32(p+0x2a0u,u32(p+0x2a0u)+u32(w+8u));
    }
    // 4F4D80(ecx = player): the packet sent: the message sizes appended (last first, 1 or 2
    // bytes), the 2-byte length at the start, the raw tail (+854 / +7EC) when it fits; then
    // sent to the player (491B60) and emptied.
    void flush_4f4d80(std::uint32_t p){
        const std::uint32_t W=p+0x298u;
        p32(p+0x2a8u,u32(p+0x2a0u));
        while(u32(p+0x7e8u)){
            const std::uint32_t v=u32(p+0x7c4u+u32(p+0x7e8u)*4u);
            Frame L(*this,0x10);p32(L(0),v);
            vcall(W,0,{L(0),std::int32_t(v)<0x100?1u:2u});
            p32(p+0x7e8u,u32(p+0x7e8u)-1u);
        }
        {const std::int32_t h=i32(p+0x2a0u)-2;
            p32(p+0x2a8u,0);
            Frame L(*this,0x10);p32(L(0),std::uint32_t(h<=0?0:h));
            vcall(W,0,{L(0),2u});}
        if(const std::uint32_t tail=u32(p+0x858u);tail&&std::int32_t(tail+u32(p+0x2a0u)+1u)<=i32(p+0x2a4u)){
            p8(u32(p+0x29cu)+u32(p+0x2a0u),u8(p+0x854u));
            p32(p+0x2a0u,u32(p+0x2a0u)+1u);
            for(std::uint32_t k=0;k<tail;++k)p8(u32(p+0x29cu)+u32(p+0x2a0u)+k,u8(p+0x7ecu+k));
            p32(p+0x2a0u,u32(p+0x2a0u)+u32(p+0x858u));
            p32(p+0x858u,0);
        }
        send_491b60(W,p+0x244u,0);
        p32(W+0x10u,0);p32(W+8u,0);
        p32(p+0x28cu,0);
    }
    // 4F4950(ecx = player, reader) / 4F4B80(reader, player): a player's state message (see
    // 4F4790): its id (4F46A0), infos, address, colour and vote; the infos' +2C cleared and
    // the lobby told (4549F0).
    void read_state_4f4950(std::uint32_t p,std::uint32_t r){
        Frame L(*this,0x20);
        const std::uint32_t b=L(0);
        read_491860(r,b,2);
        method(0x4f46a0u,p,{u16(b)});
        read_491860(r,b,1);p32(p+0x18u,u8(b));
        read_address_491800(p+0x244u,r);
        read_491860(r,b,1);p32(p+0x250u,u8(b));
        read_491860(r,b,1);p32(p+0xcu,u8(b));
        for(std::uint32_t i=0;std::int32_t(i)<i32(p+0x18u);++i){
            const std::uint32_t f=p+0x20u+i*0x88u;
            read_491860(r,f,0x10);
            read_491860(r,f+0x18u,0x10);
            p8(f+0x10u,0);
            read_491860(r,b,1);p8(f+0x2au,u8(b)?1u:0u);
            for(std::uint32_t o:{0x40u,0x44u,0x48u,0x60u,0x64u}){read_491860(r,b,4);p32(f+o,u32(b));}
            read_491860(r,f+0x4cu,0x10);
            p8(f+0x5cu,0);
            for(std::uint32_t o:{0x68u,0x6cu,0x70u,0x74u,0x78u,0x7cu}){read_491860(r,b,1);p32(f+o,u8(b));}
        }
        for(std::uint32_t i=0;std::int32_t(i)<i32(p+0x18u);++i)p32(p+0x4cu+i*0x88u,0);
        if(u32(0x7d68acu))call(0x4549f0u,{p});
    }
    // 4F51E0: the host (every 60 frames once the race is set up, [850B00]) or a guest sends
    // its own race records 7D68E0 (0x14 each, count +60) to their owners (4F4FC0).
    void race_records_4f51e0(){
        std::uint32_t s=u32(0x7d68acu);
        if(u8(s+7u)&&!u32(0x850b00u)&&i32(0x7d68b4u)%0x3c!=0)return;
        for(std::int32_t i=0;i<i32(s+0x60u);++i){
            const std::uint32_t rec=0x7d68e4u+std::uint32_t(i)*0x14u;
            const std::uint16_t me=u16(u32(s+0x58u));
            if(std::uint16_t(u16(rec-2u)&0xfffcu)==me&&u8(s+7u)&&i32(rec)>=0){
                const bool own=std::uint16_t(u16(rec-4u)&0xfffcu)==me;
                call(0x4f4fc0u,{std::uint32_t(i),u32(rec),own?1u:0u});
                s=u32(0x7d68acu);
            }
        }
    }
    // 490020(ecx = listener): reset (+14 = 0, [830BE8] = 0, its event cleared).
    void listener_reset_490020(std::uint32_t self){p8(self+0x14u,0);p32(0x830be8u,0);event_clear_48f900(self);}
    // 490530(ecx = listener, leaving): the session closes: the others told (4F4CD0 4 from the
    // host, 0 to the host) when leaving, the lobby objects reset, every player removed
    // (454BB0), the session ended (454200), the listener reset.
    void close_490530(std::uint32_t self,std::uint32_t leaving){
        std::uint32_t s=u32(0x7d68acu);
        if(!s||!u8(s+5u)){vcall(self,4);return;}
        if(leaving&0xffu){
            if(u8(s+7u)){
                for(std::int32_t i=0;i<i32(u32(0x7d68acu)+0x14u);++i){
                    const std::uint32_t ses=u32(0x7d68acu),p=player_at(ses,std::uint32_t(i));
                    if(p!=u32(ses+0x58u)&&u32(p+8u)!=7u)method(0x4f4cd0u,p,{4u});
                }
            }else if(u32(u32(s+0x5cu)+8u)!=7u)method(0x4f4cd0u,u32(s+0x5cu),{0u});
            s=u32(0x7d68acu);
        }
        if(s==0x98a650u){
            if(u8(s+7u)){method(0x431380u,0x988ed4u);if(leaving&0xffu)method(0x432c60u,0x988efcu);}
            else method(0x4355f0u,0x98a628u);
            s=u32(0x7d68acu);
        }else if(s==0x988dc8u){
            if(u8(s+7u))event_clear_48f900(0x988e78u);else method(0x491cc0u,0x988eb4u);
            s=u32(0x7d68acu);
        }
        while(u32(s+0x14u)){method(0x454bb0u,s,{u32(s+0x18u)});s=u32(0x7d68acu);}
        p32(s+0x58u,0);p32(u32(0x7d68acu)+0x5cu,0);
        method(0x454200u,u32(0x7d68acu));
        vcall(self,4);
    }
    // 4F46A0(ecx = player, id): the player's id (+0) and its four info ids (id & ~3 | k at
    // +48 / +D0 / +158 / +1E0); the session's player list sorted again (qsort, 4F4680).
    void set_id_4f46a0(std::uint32_t p,std::uint32_t id){
        p16(p,id);
        const std::uint32_t base=id&0xfffcu;
        p16(p+0xd0u,base|1u);p16(p+0x48u,base);p16(p+0x1e0u,base|3u);p16(p+0x158u,base|2u);
        const std::uint32_t s=u32(0x7d68acu);
        call(0x580cb0u,{s+0x18u,u32(s+0x14u),4u,0x4f4680u});
    }
    // 4549F0(player): the lobby slots 7DCBE0 (count [7DD130]) of the player's infos
    // (matching +18 / +1C ids) removed (4544F0).
    void drop_slots_4549f0(std::uint32_t p){
        std::uint32_t k=0;
        while(std::int32_t(k)<i32(0x7dd130u)){
            const std::uint32_t e=0x7dcbe0u+k*0x88u;
            std::int32_t j=0;
            for(;j<i32(p+0x18u);++j){
                const std::uint32_t f=p+0x38u+std::uint32_t(j)*0x88u;
                if(u32(e+0x18u)==u32(f)&&u32(e+0x1cu)==u32(f+4u))break;
            }
            if(j<i32(p+0x18u))method(0x4544f0u,0x7dcbe0u,{k});
            else ++k;
        }
    }
    // 454BB0(ecx = session, player): the player removed (454A90, its online counts, reset by
    // 4F4BD0, unset as this machine / host, taken out of the list).
    void remove_player_454bb0(std::uint32_t s,std::uint32_t p){
        call(0x454a90u,{p});
        if(s==0x98a650u&&u8(s+7u)){
            p32(0x77ff9cu,u32(0x77ff9cu)-u32(p+0x14u));
            p32(0x77ff98u,u32(0x77ff98u)+(u32(p+0x14u)-u32(p+0x18u)));
        }
        method(0x4f4bd0u,p);
        if(u32(s+0x58u)==p)p32(s+0x58u,0);
        if(u32(s+0x5cu)==p)p32(s+0x5cu,0);
        std::int32_t i=0;
        while(i<i32(s+0x14u)&&player_at(s,std::uint32_t(i))!=p)++i;
        if(i>=i32(s+0x14u))return;
        for(;i<i32(s+0x14u)-1;++i)p32(s+0x18u+std::uint32_t(i)*4u,u32(s+0x1cu+std::uint32_t(i)*4u));
        p32(s+0x14u,u32(s+0x14u)-1u);
    }
    // 454200(ecx = session): ended ([659930] = 0, active / host / started cleared).
    void end_session_454200(std::uint32_t s){p32(0x659930u,0);p8(s+5u,0);p8(s+7u,0);p8(s+8u,0);}
    // 4F4CD0(ecx = player, code): (code 5: no message) the 0x11 code message sent six times
    // to the player, which is marked gone (+8 = 7).
    void goodbye_4f4cd0(std::uint32_t p,std::uint32_t code){
        if(code!=5u){
            Frame L(*this,0x40);
            const std::uint32_t W=L(0),buf=L(0x20),tmp=L(0x30);
            p32(W,0x627c48u);p32(W+4u,buf);p32(W+8u,0);p32(W+0xcu,4);p32(W+0x10u,0);
            p32(tmp,2);write_491900(W,tmp,2);
            p8(tmp,0x11);write_491900(W,tmp,1);
            p8(tmp,code);write_491900(W,tmp,1);
            for(int k=0;k<6;++k)send_491b60(W,p+0x244u,0);
        }
        p32(p+8u,7);
    }
    // ---- the host's shared state: five message writers 63AFB0 + k * 0x530 (vtable 491940,
    // +16 changed, +14 the previous length) with their sequence numbers [session + 68 + k*4] ----
    // 4F4700(ecx = player): its state byte: state (bits 0..3), the infos' +2B flags (bit 4..),
    // the vote + 1 (bits 5..6).
    std::uint32_t state_byte_4f4700(std::uint32_t p){
        std::uint32_t f=u32(p+8u)&0xfu;
        for(std::uint32_t i=0;std::int32_t(i)<i32(p+0x18u);++i)if(u8(p+0x4bu+i*0x88u))f|=0x10u<<i;
        return (((u32(p+0xcu)+1u)<<5)&0x60u)|f;
    }
    void put(std::uint32_t w,std::uint32_t a,std::uint32_t n){vcall(w,0,{a,n});}
    void put_value(std::uint32_t w,std::uint32_t v,std::uint32_t n){Frame L(*this,0x10);p32(L(0),v);put(w,L(0),n);}
    // 515EC0 / 515EE0 / 515EF0 / 515F70 / 5162B0(writer): the five host messages: the online
    // counters 77FF90 (online only), the course setup 65A7A4, every player's id and state
    // byte, the online infos' ids (online only), the race setup (516040).
    void host_counters_515ec0(std::uint32_t w){if(u32(0x7d68acu)==0x98a650u)put(w,0x77ff90u,0x14);}
    void host_setup_515ee0(std::uint32_t w){put(w,0x65a7a4u,0x28);}
    void host_players_515ef0(std::uint32_t w){
        std::uint32_t s=u32(0x7d68acu);
        for(std::int32_t i=0;i<i32(s+0x14u);++i){
            const std::uint32_t p=player_at(s,std::uint32_t(i));
            const std::uint32_t st=u32(p+8u);
            if(!st||st==7u)continue;
            put_value(w,u16(p),2);
            put_value(w,state_byte_4f4700(p),1);
            s=u32(0x7d68acu);
        }
    }
    void host_online_515f70(std::uint32_t w){
        std::uint32_t s=u32(0x7d68acu);
        if(s!=0x98a650u)return;
        for(std::int32_t i=0;i<i32(s+0x14u);++i){
            const std::uint32_t p=player_at(s,std::uint32_t(i));
            const std::uint32_t st=u32(p+8u);
            if(!st||st==7u)continue;
            for(std::uint32_t j=0;std::int32_t(j)<i32(p+0x18u);++j){
                put_value(w,u16(p+0x48u+j*0x88u),2);
                put_value(w,u32(p+0x58u+j*0x88u),4);
            }
            s=u32(0x7d68acu);
        }
    }
    // 516040(esi = writer, page): the race setup: the start counters 7D68A4 / 7D688C, the
    // course setup, the race start records (850B1C, rebuilt from the players when 7D68A4
    // changed: index, id, name, choices, rating), six records 7D68E0 of the page.
    void race_setup_516040(std::uint32_t w,std::uint32_t page){
        put_value(w,u32(0x7d68a4u),4);
        put_value(w,u32(0x7d688cu),4);
        put(w,0x65a7a4u,0x28);
        std::uint32_t s=u32(0x7d68acu);
        if(u32(0x7d68a4u)!=u32(0x850b30u)){
            p32(0x850b30u,u32(0x7d68a4u));
            p32(0x850b34u,u32(s+0x14u));
            for(std::uint32_t i=0;std::int32_t(i)<i32(s+0x14u);++i){
                const std::uint32_t p=player_at(s,i);
                const std::uint32_t rec=u32(0x850b1cu)+i*0x20u;
                p32(rec+4u,i);p32(rec,u16(p));
                p8(rec+0x18u,u8(p+0x94u));p8(rec+0x19u,u8(p+0x98u));p32(rec+0x1cu,u32(p+0x80u));
                for(std::uint32_t k=0;k<0x10u;k+=4)p32(rec+8u+k,u32(p+0x20u+k));
                s=u32(0x7d68acu);
            }
        }
        const std::int32_t first=std::int32_t(page)*6,end=first+6;
        {const std::int32_t n=std::min(i32(s+0x60u),end);put_value(w,std::uint32_t(std::uint8_t(n)-std::uint8_t(page*6u)),1);}
        for(std::int32_t j=first;j<std::min(i32(u32(0x7d68acu)+0x60u),end);++j)put_value(w,u16(0x7d68e0u+std::uint32_t(j)*0x14u),2);
        put_value(w,u8(0x850b34u),1);
        for(std::uint32_t k=0;std::int32_t(k)<i32(0x850b34u);++k){
            const std::uint32_t rec=u32(0x850b1cu)+k*0x20u;
            put(w,rec+4u,1);put(w,rec,4);put(w,rec+0x18u,1);put(w,rec+0x19u,1);put(w,rec+0x1cu,4);put(w,rec+8u,0x10);
        }
    }
    // 5162F0(fn, ebx = k): host message k rebuilt by fn after its kind byte and sequence;
    // when it changed, the sequence advances and every other player is marked to get it.
    void host_message_5162f0(std::uint32_t k){
        const std::uint32_t s=u32(0x7d68acu);
        const std::uint32_t w=0x63afb0u+k*0x530u,seq=s+0x68u+k*4u;
        p16(w+0x14u,u16(w+8u));p8(w+0x16u,0);
        p32(w+0x10u,0);p32(w+8u,0);
        put_value(w,k,1);
        put_value(w,u32(seq),4);
        switch(k){
        case 0:host_counters_515ec0(w);break;
        case 1:host_setup_515ee0(w);break;
        case 2:host_players_515ef0(w);break;
        case 3:host_online_515f70(w);break;
        case 4:race_setup_516040(w,0);break;
        case 6:put_value(w,u8(0x7d68a4u),1);break;                 // 5162D0
        default:throw std::runtime_error("network: host message 5 has no writer");
        }
        if(!u8(w+0x16u)&&u32(w+8u)==std::uint32_t(std::int32_t(i16(w+0x14u))))return;
        p32(seq,u32(seq)+1u);
        p32(w+0x10u,1);
        put_value(w,u32(seq),4);
        const std::uint32_t ses=u32(0x7d68acu);
        for(std::int32_t i=0;i<i32(ses+0x14u);++i){
            const std::uint32_t p=player_at(ses,std::uint32_t(i));
            if(p!=u32(ses+0x58u))p8(p+k+0x258u,1);
        }
    }
    // 5163C0: the host's messages 0..3 (and 4 once the start counter runs); [session + A4]
    // = 0x64 in the lobby screens (flag 1), else -1.
    void host_messages_5163c0(){
        for(std::uint32_t k=0;k<4u;++k)host_message_5162f0(k);
        if(u32(0x7d68a4u))host_message_5162f0(4);
        p32(u32(0x7d68acu)+0xa4u,(net_flags()&1u)?0x64u:0xffffffffu);
    }
    // 4F4BD0(ecx = player): the player object reset (no id, four empty infos, no address,
    // timers 0xE10, its packet emptied, vote 2).
    void player_reset_4f4bd0(std::uint32_t p){
        p16(p,0);p32(p+4u,0);p32(p+8u,0);p32(p+0x18u,0);p32(p+0x14u,0);
        for(std::uint32_t i=0;i<4u;++i)info_reset_5207f0(p+0x20u+i*0x88u);
        p32(p+0x244u,0);p32(p+0x248u,0);p32(p+0x24cu,0);
        p32(p+0x254u,0);p32(p+0x258u,0);p16(p+0x25cu,0);p8(p+0x25eu,0);
        p32(p+0x264u,0);p32(p+0x268u,0);
        p32(p+0x270u,0xffffffffu);p32(p+0x27cu,0xffffffffu);
        p8(p+0x26cu,0);p8(p+0x26du,0);p8(p+0x26eu,0);
        p32(p+0x274u,0);p32(p+0x278u,0xe10);p32(p+0x280u,0);
        for(std::uint32_t o:{0x284u,0x288u,0x28cu,0x290u})p32(p+o,0xe10);
        p32(p+0x298u+0x10u,0);p32(p+0x298u+8u,0);
        p32(p+0x7c4u,0);p32(p+0x7e8u,0);p32(p+0x854u,0);p32(p+0x858u,0);
        p8(p+0x240u,0);p8(p+0x85cu,0);p8(p+0x10u,0);p8(p+0x11u,0);
        p32(p+0xcu,2);
    }
    // 5207F0(ecx = info): an empty info.
    void info_reset_5207f0(std::uint32_t f){
        p8(f,0);p32(f+0x18u,0);p32(f+0x1cu,0);p32(f+0x20u,0);p32(f+0x24u,0);
        p16(f+0x28u,0);p8(f+0x2au,0);p8(f+0x2bu,0);p32(f+0x2cu,0);p32(f+0x34u,0);
        p32(f+0x38u,0xffffffffu);p32(f+0x3cu,0xffffffffu);p8(f+0x80u,0);
        p32(f+0x68u,0);p32(f+0x74u,0);p32(f+0x78u,0);p32(f+0x7cu,0);
    }
    // 454490(ecx = slots, at, entry) / 4544F0(ecx = slots, at): a 0x88 lobby slot inserted
    // at / removed from position at (count +550).
    void slot_insert_454490(std::uint32_t o,std::uint32_t at,std::uint32_t entry){
        const std::uint32_t n=u32(o+0x550u)+1u;p32(o+0x550u,n);
        for(std::int32_t i=std::int32_t(n)-1;i>std::int32_t(at);--i)
            for(std::uint32_t k=0;k<0x88u;k+=4)p32(o+std::uint32_t(i)*0x88u+k,u32(o+std::uint32_t(i-1)*0x88u+k));
        for(std::uint32_t k=0;k<0x88u;k+=4)p32(o+at*0x88u+k,u32(entry+k));
    }
    void slot_remove_4544f0(std::uint32_t o,std::uint32_t at){
        const std::uint32_t n=u32(o+0x550u)-1u;p32(o+0x550u,n);
        for(std::int32_t i=std::int32_t(at);i<i32(o+0x550u);++i)
            for(std::uint32_t k=0;k<0x88u;k+=4)p32(o+std::uint32_t(i)*0x88u+k,u32(o+std::uint32_t(i+1)*0x88u+k));
    }
    // 454A90(player): its lobby slots removed, and (guests of a non-LAN session) its infos
    // kept as the first slots (at most ten).
    void player_slots_454a90(std::uint32_t p){
        drop_slots_4549f0(p);
        const std::uint32_t s=u32(0x7d68acu);
        for(std::uint32_t j=0;std::int32_t(j)<i32(p+0x18u);++j){
            const std::uint32_t f=p+0x20u+j*0x88u;
            if(!u8(f)||p==u32(s+0x58u)||s==0x988dc8u)continue;
            if(i32(0x7dd130u)>=0xa)p32(0x7dd130u,u32(0x7dd130u)-1u);
            slot_insert_454490(0x7dcbe0u,0,f);
            p8(0x7dcc60u,0);
        }
    }
    // ==== the race-side network frame (45A280 / 45A0C0) and the race messages ===========
    // The CommRace block 7DE418 (see race_traffic_network.inc): this machine's entry is
    // [7DD138]; the clock [7F1938] counts the network frames.
    std::uint32_t own_entry(){return 0x7de418u+std::uint32_t(u8(0x7dd138u))*0x6cu;}
    // 4F53B0 / 456AE0(ecx = block, i): entry i is a connected player (see 456670).
    bool versus_4f53b0(){return u8(0x850b04u)!=0u;}
    bool connected_456ae0(std::uint32_t block,std::uint32_t i){
        const std::uint32_t e=block+4u+i*0x6cu;
        std::int32_t age=std::int32_t(u32(0x7f1938u)-u32(block+i*0x6cu+0x58u));
        if(!versus_4f53b0())age=0;
        const std::uint32_t pl=u32(e);
        if(u16(pl)!=u32(e+4u))return false;
        const std::uint32_t st=u32(pl+8u);
        return (st==3u||st==4u)&&age<0xff;
    }
    // 4575B0(id): the player id has a connected CommRace entry.
    std::uint32_t racing_4575b0(std::uint32_t id){
        const std::uint32_t n=u32(0x7de418u);
        for(std::uint32_t i=0;i<n;++i)
            if(u32(0x7de420u+i*0x6cu)==id)return connected_456ae0(0x7de418u,i)?1u:0u;
        return 0;
    }
    // 45A280: the race frame of a running session player (state not 0 / 7): 45A0C0.
    void race_frame_45a280(){
        const std::uint32_t s=u32(0x7d68acu);
        if(!s||!u32(s+0x58u))return;
        const std::uint32_t st=u32(u32(s+0x58u)+8u);
        if(st==7u||!st)return;
        race_frame_45a0c0();
    }
    // 45A0C0: outside the race the entry's times follow the clock (4F5B90 every 4th frame);
    // in the race (mode 0x10 / 0x12) the lost players are dropped (459F60), the entry gets
    // the course word, mission timer and speed byte, the state history (4F5340 into 7DF350
    // every 4th frame) and the progress message (4F5C70 every 32nd); the clock advances.
    void race_frame_45a0c0(){
        const std::uint32_t mode=u32(0x78026cu);
        if(mode!=0x10u&&mode!=0x12u){
            if(mode==0x20u){p32(0x7f1938u,u32(0x7f1938u)+1u);return;}
            const std::uint32_t e=own_entry(),clk=u32(0x7f1938u);
            p32(e+0x2cu,clk);p32(e+0x28u,clk);p32(e+0x58u,clk);
            if(clk&3u){p32(0x7f1938u,clk+1u);return;}
            ping_4f5b90();
            p32(0x7f1938u,u32(0x7f1938u)+1u);
            return;
        }
        drop_lost_459f60(0x7de418u);
        const std::uint32_t car=u32(0x799d18u);
        {const std::uint32_t e=own_entry(),clk=u32(0x7f1938u);
            p32(e+0x2cu,clk);p32(e+0x28u,clk);p32(e+0x58u,clk);
            p32(e+0x44u,u16(car+0x260u));}
        {const std::uint32_t t=call(0x495800u);p32(own_entry()+0x54u,t);}
        const float v=float(std::int32_t(call(0x44c940u,{u32(car+0x68u)})));
        std::uint8_t speed=0;
        switch(u32(0x78024cu)){
        case 0:case 1:case 4:speed=std::uint8_t(driving::x87_ftol64((driving::X87(v)+driving::X87(f32(car+0x102cu)))*driving::X87(f32(0x59dcccu))));break;
        case 2:case 3:speed=std::uint8_t(driving::x87_ftol64((driving::X87(v)+driving::X87(f32(car+0x102cu)))*driving::X87(f32(0x6281e8u))));break;
        default:{std::uint32_t b;std::memcpy(&b,&v,4);speed=std::uint8_t(b>>24);break;}   // (never: presets 0..3) a stack byte
        }
        p8(own_entry()+0x64u,speed);
        p16(own_entry()+0x5eu,u16(car+0x260u));
        std::uint32_t clk=u32(0x7f1938u);
        if(!(clk&3u)){
            call(0x4f5340u,{0x7df350u+((clk-4u)&0x3fu)*0x24u,0x90u,0,0});
            clk=u32(0x7f1938u);
        }
        if(!(clk&0x1fu)){
            const float pos=f32(car+0x102cu)+float(std::int32_t(call(0x44c940u,{u32(car+0x68u)})));
            std::uint32_t b;std::memcpy(&b,&pos,4);
            progress_4f5c70(b,u8(0x7dd138u));
            clk=u32(0x7f1938u);
        }
        p32(0x7f1938u,clk+1u);
    }
    // 459F60(ecx = block): each player whose connection dropped (marked +C 1): the lost
    // message (459AA0: 0x3CC / 0x481 by its state), the host's goodbye to it (4F4CD0 0)
    // or the guest's to the host, and its car closed (4401D0).
    void drop_lost_459f60(std::uint32_t block){
        for(std::uint32_t i=0;i<u32(block);++i){
            const std::uint32_t r=block+i*0x6cu;
            if(connected_456ae0(block,i))continue;
            if(u8(r+0xcu)!=1u)continue;
            {const std::uint32_t st=u32(u32(r+4u)+8u);
                method(0x459aa0u,0x7ee070u,{st==6u?0x3ccu:0x481u,r+0xfu});}
            if(u32(0x7d68acu)){
                std::int32_t age=std::int32_t(u32(0x7f1938u)-u32(r+0x58u));
                if(versus_4f53b0()&&age>=0xff){
                    if(i==0u)goodbye_4f4cd0(u32(u32(0x7d68acu)+0x5cu),0);
                    else if(u8(u32(0x7d68acu)+7u)&&u16(u32(r+4u))==u32(r+8u))goodbye_4f4cd0(u32(r+4u),0);
                }
            }
            for(std::uint32_t k=0;k<u32(block);++k){
                if(u32(u32(0x799d18u+k*0x3cu)+0x1054u)!=i)continue;
                if(k)call(0x4401d0u,{k+8u});
                break;
            }
            p8(r+0xcu,0);
        }
    }
    // 4F5B90: the clock message (0x18 0, the clock 455D40) to the racing players.
    void ping_4f5b90(){
        const std::uint32_t s=u32(0x7d68acu);
        if(!s)return;
        Frame L(*this,0x140);
        const std::uint32_t W=L(0),buf=L(0x20);
        p32(W,0x627c48u);p32(W+4u,buf);p32(W+8u,0);p32(W+0xcu,0x100);p32(W+0x10u,0);
        put_value(W,0x18,1);put_value(W,0,1);put_value(W,u32(0x7f1938u),4);
        for(std::int32_t i=0;i<i32(u32(0x7d68acu)+0x14u);++i){
            const std::uint32_t p=player_at(u32(0x7d68acu),std::uint32_t(i));
            const std::uint32_t st=u32(p+8u);
            if((st==3u||st==4u)&&racing_4575b0(u16(p)))queue_4f4ed0(p,W);
        }
    }
    // 4F5C70(progress, slot): the progress message (0x18 1, this player's id, the progress,
    // the slot | 0x80 on presets 2 / 3) to the first other player not racing.
    void progress_4f5c70(std::uint32_t progress,std::uint32_t slot){
        const std::uint32_t s=u32(0x7d68acu);
        if(!s)return;
        for(std::int32_t i=0;i<i32(u32(0x7d68acu)+0x14u);++i){
            const std::uint32_t p=player_at(u32(0x7d68acu),std::uint32_t(i));
            const std::uint32_t st=u32(p+8u);
            if(st==1u||st==2u||racing_4575b0(u16(p))==1u)continue;
            Frame L(*this,0x140);
            const std::uint32_t W=L(0),buf=L(0x20);
            p32(W,0x627c48u);p32(W+4u,buf);p32(W+8u,0);p32(W+0xcu,0x100);p32(W+0x10u,0);
            put_value(W,0x18,1);put_value(W,1,1);
            put_value(W,u16(u32(u32(0x7d68acu)+0x58u)),4);
            put_value(W,progress,4);
            const std::uint32_t pr=u32(0x78024cu);
            put_value(W,(pr==2u||pr==3u)?(slot|0x80u):slot,1);
            queue_4f4ed0(p,W);
            return;
        }
    }
    // A float as the 16-bit value 456E40 sends: 10 mantissa bits, a 5-bit exponent from -25,
    // the sign in bit 15; 0 below, 0xFFFF above 2^6.
    std::uint32_t half(float f){
        std::uint32_t b;std::memcpy(&b,&f,4);
        std::int32_t e=std::int32_t((b>>23)&0xffu)-0x7f;
        const std::uint32_t sign=b&0x80000000u;
        std::uint32_t m=b&0x7fffffu;
        if(e!=-0x7f)m|=0x800000u;
        if(e<-0x19)return 0;
        if(e>6)return 0xffff;
        std::uint32_t v=((m>>13)&0x3ffu)|(std::uint32_t(e+0x19)<<10);
        if(sign)v|=0x8000u;
        return v;
    }
    // 456E40(eax = out, state): the four wheel / history records of a car state (two halves,
    // a truncated word, a 25-bit word) and its two floats and word: the length.
    std::uint32_t pack_state_456e40(std::uint32_t out,std::uint32_t st){
        std::uint32_t o=out;
        auto b16=[&](std::uint32_t v){p8(o,v);p8(o+1u,v>>8);o+=2;};
        auto b32=[&](std::uint32_t v){p8(o,v);p8(o+1u,v>>8);p8(o+2u,v>>16);p8(o+3u,v>>24);o+=4;};
        for(std::uint32_t k=0;k<4u;++k){
            const std::uint32_t r=st+0x10u+k*0x24u;
            b16(half(f32(r-4u)));
            b16(half(f32(r)));
            b16(std::uint32_t(driving::x87_ftol64(driving::X87(f32(r+8u)))));
            b32(((u32(r+0xcu)&1u)<<24)|u32(r+0x10u));
        }
        b32(u32(st+0x70u));b32(u32(st+0x74u));
        b16(std::uint32_t(driving::x87_ftol64(driving::X87(f32(st+0x80u)))));
        return o-out;
    }
    // 457600(id, out, state, -): the player's race state packet: the clock (4 bytes), the
    // car state (456E40), the clock delta of its last packet, the progress (24 bits of
    // [7DE480] * [5A9214]), the speed byte, then (flag 1) its finish time and (flag 2) its
    // finish rank when recent; the flag byte last. Returns the length.
    std::uint32_t pack_race_457600(std::uint32_t id,std::uint32_t out,std::uint32_t st){
        std::int32_t idx=-1;
        for(std::uint32_t i=0;i<u32(0x7de418u);++i)if(u32(0x7de420u+i*0x6cu)==id){idx=std::int32_t(i);break;}
        const std::uint32_t clk=u32(0x7f1938u);
        p8(out,clk);p8(out+1u,std::uint32_t(std::int32_t(clk)>>8));
        p8(out+2u,u16(0x7f193au));p8(out+3u,u16(0x7f193au)>>8);
        std::uint32_t flags=0;
        std::uint32_t o=out+4u;
        o+=pack_state_456e40(o,st);
        const std::uint32_t rec=0x7de418u+std::uint32_t(idx)*0x6cu,own=own_entry();
        const std::uint32_t now=u32(0x7f1938u);
        p8(o,now-u8(rec+0x28u));++o;
        {const std::uint32_t v=std::uint32_t(cvtt_float(f32(own+0x68u)*f32(0x5a9214u)));
            p8(o,v);p8(o+1u,std::uint32_t(std::int32_t(v)>>8));p8(o+2u,std::uint32_t(std::int32_t(v)>>16));o+=3;}
        p8(o,u8(own+0x64u));++o;
        {const std::uint32_t t=u32(own+0x30u);
            if(u32(rec+0x2cu)-2u<=t&&now-t<0x100u){
                const std::uint32_t v=u32(own+0x34u);
                p8(o,now-t);p8(o+1u,v);p8(o+2u,v>>8);p8(o+3u,v>>16);p8(o+4u,v>>24);
                flags=1;o+=5;
            }}
        {const std::uint32_t t=u32(own+0x50u);
            if(u32(rec+0x2cu)-2u<=t&&now-t<0x100u){
                p8(o,now-t);p8(o+1u,u8(own+0x48u));p8(o+2u,u8(own+0x4cu));
                flags|=2u;o+=3;
            }}
        p8(o,flags);
        return o-out+1u;
    }
    static std::int32_t cvtt_float(float f){
        if(!(f>-2147483904.0f&&f<2147483648.0f))return std::int32_t(0x80000000u);
        return std::int32_t(f);
    }
    // 4F5A60(state, n) / 4F5340: the race state packet (0x17, 457600, its checksum) to every
    // other racing player; their +94 timers restart.
    void race_state_4f5a60(std::uint32_t st,std::uint32_t n){
        const std::uint32_t s=u32(0x7d68acu);
        if(!s)return;
        for(std::int32_t i=0;i<i32(u32(0x7d68acu)+0x14u);++i){
            const std::uint32_t p=player_at(u32(0x7d68acu),std::uint32_t(i));
            if(!racing_4575b0(u16(p))||p==u32(u32(0x7d68acu)+0x58u))continue;
            p32(u32(0x7d68acu)+0x94u,0);
            Frame L(*this,0x240);
            const std::uint32_t W=L(0),buf=L(0x20),pk=L(0x120);
            p32(W,0x627c48u);p32(W+4u,buf);p32(W+8u,0);p32(W+0xcu,0x100);p32(W+0x10u,0);
            put_value(W,0x17,1);
            const std::uint32_t len=pack_race_457600(u16(p),pk,st);
            write_491900(W,pk,len);
            put_value(W,checksum_491840(W+4u,0,u32(W+8u)),1);
            queue_4f4ed0(p,W);
            (void)n;
        }
    }
    // ---- race messages (0x17 state, 0x18 control, 0x1C / 0x1D results) received -------
    std::int32_t commrace_index(std::uint32_t id){
        for(std::uint32_t i=0;i<u32(0x7de418u);++i)if(u32(0x7de420u+i*0x6cu)==id)return std::int32_t(i);
        return -1;
    }
    std::uint32_t le32(std::uint32_t a){return u8(a)|(std::uint32_t(u8(a+1u))<<8)|(std::uint32_t(u8(a+2u))<<16)|(std::uint32_t(u8(a+3u))<<24);}
    // The float of a 456E40 half (0 stays 0; an odd mantissa fills the low 13 bits).
    static float unhalf(std::uint32_t h){
        h&=0xffffu;
        if(!h)return 0.0f;
        std::uint32_t b=(((h&0xffff8000u)<<3)|(h&0x3ffu))<<13;
        b|=(((h>>10)&0x1fu)+0x66u)<<23;
        if(h&1u)b|=0x1fffu;
        float f;std::memcpy(&f,&b,4);return f;
    }
    // 457010(eax = in, records, clock): the four car states (clock - 4 + k, the halves, the
    // word, the flag / 28-bit word) and the last one's position / yaw; the earlier positions
    // and yaws rebuilt backwards from the deltas. Returns the bytes read.
    std::uint32_t unpack_state_457010(std::uint32_t in,std::uint32_t st,std::uint32_t clock){
        std::uint32_t i=in;
        auto w16=[&]{const std::uint32_t v=u8(i)|(std::uint32_t(u8(i+1u))<<8);i+=2;return v;};
        for(std::uint32_t k=0;k<4u;++k){
            const std::uint32_t r=st+k*0x24u;
            p32(r,clock+k-4u);
            pf(r+0xcu,unhalf(w16()));
            pf(r+0x10u,unhalf(w16()));
            pf(r+0x18u,float(std::int32_t(w16())));
            const std::uint32_t v=le32(i);i+=4;
            p32(r+0x1cu,(v>>24)&1u);p32(r+0x20u,v&0xefffffffu);
        }
        p32(st+0x70u,le32(i));i+=4;
        p32(st+0x74u,le32(i));i+=4;
        pf(st+0x80u,float(std::int32_t(w16())));
        for(std::uint32_t c=st+0x78u,n=0;n<3u;++n,c-=0x24u){
            pf(c-0x2cu,f32(c-8u)-f32(c));
            pf(c-0x28u,f32(c-4u)-f32(c+4u));
            const float a=float(std::int32_t(std::int16_t(cvtt_float(f32(c+8u)))));
            const float b=float(std::int32_t(std::int16_t(cvtt_float(f32(c+0xcu)))));
            pf(c-0x1cu,a-b);
        }
        return i-in;
    }
    // 4567D0(ecx = block, i, word, time): player i's packed word / index (+34 / +38) when
    // the time is not older than its +30.
    void packed_4567d0(std::uint32_t block,std::uint32_t i,std::uint32_t v,std::uint32_t t){
        const std::uint32_t r=block+i*0x6cu;
        if(t<u32(r+0x30u))return;
        p32(r+0x34u,v);p32(r+0x38u,first_two(v));p32(r+0x30u,t);
    }
    static std::uint32_t first_two(std::uint32_t v){std::uint32_t i=0;while(i<0xeu&&(v&3u)!=2u){v>>=2;++i;}return i;}
    // 457210(data, length, id): a race state packet (see 457600) of player id: its car states
    // into the history (7EE240, index [7F1954]), then (when newer than its +28) its clock,
    // progress, course word, speed, and the finish time (4567D0) / rank it carries.
    void unpack_race_457210(std::uint32_t d,std::uint32_t len,std::uint32_t id){
        const std::int32_t idx=commrace_index(id);
        if(idx<0)return;
        const std::uint32_t flags=u8(d+len-1u);
        const std::uint32_t clock=le32(d);
        const std::uint32_t hist=u32(0x7f1954u);
        std::uint32_t s=d+4u;
        s+=unpack_state_457010(s,0x7ee240u+((std::uint32_t(idx)<<6)+hist)*0x24u,clock);
        p32(0x7df334u+std::uint32_t(idx)*4u,3);
        const std::uint32_t sent=clock-std::uint32_t(std::int32_t(i8(s)));
        p32(0x7f1954u,(hist+4u)&0x3fu);
        const std::uint32_t prog=u8(s+1u)|(std::uint32_t(u8(s+2u))<<8)|(std::uint32_t(u8(s+3u))<<16);
        const float progress=driving::x87_float(driving::X87(std::int32_t(prog))*driving::X87(f32(0x628130u)));
        const std::uint32_t speed=u8(s+4u);
        s+=5;
        std::uint32_t ftime=0,fword=0;
        if(flags&1u){ftime=clock-std::uint32_t(std::int32_t(i8(s)));fword=le32(s+1u);s+=5;}
        std::uint32_t rtime=0,r1=0,r2=0;
        if(flags&2u){rtime=clock-std::uint32_t(std::int32_t(i8(s)));r1=u8(s+1u);r2=u8(s+2u);}
        const std::uint32_t r=0x7de418u+std::uint32_t(idx)*0x6cu;
        if(clock<=u32(r+0x28u))return;
        p32(r+0x2cu,sent);p32(r+0x58u,u32(0x7f1938u));p8(r+0x64u,speed);p32(r+0x28u,clock);
        p32(r+0x54u,0);pf(r+0x6cu,progress);p16(r+0x5eu,prog>>8);
        if(flags&1u)packed_4567d0(0x7de418u,std::uint32_t(idx),fword,ftime);
        if(flags&2u){p32(r+0x48u,r1);p32(r+0x4cu,r2);p32(r+0x50u,rtime);}
    }
    // 457410(data, id): player id's clock (the 32-bit word) with no packet time.
    void clock_457410(std::uint32_t d,std::uint32_t id){
        const std::uint32_t r=0x7de418u+std::uint32_t(commrace_index(id))*0x6cu;
        p32(r+0x2cu,0);p32(r+0x58u,u32(0x7f1938u));p32(r+0x28u,le32(d));
    }
    // 4F5910(reader, player): a race state message with a valid checksum (its last byte).
    void race_state_in_4f5910(std::uint32_t m,std::uint32_t p){
        const std::uint32_t pos=u32(m+0xcu);
        p32(m+0xcu,u32(m+4u)-1u);
        Frame L(*this,0x10);
        read_491860(m,L(0),1);
        if(u8(L(0))!=checksum_491840(m,0,u32(m+0xcu)-1u))return;
        p32(m+0xcu,pos);
        unpack_race_457210(u32(m)+pos,u32(m+4u)-pos-1u,u16(p));
    }
    // 4F53D0(data, player): the race start sync: the host answers a guest's (0x18 2) with its
    // own counter / frame; a guest takes the host's frame and countdown (2048-scaled, the
    // latency averaged in [850B20]).
    void start_sync_4f53d0(std::uint32_t d,std::uint32_t p){
        const std::uint32_t s=u32(0x7d68acu);
        if(u8(s+7u)){
            if(u32(0x850b10u)!=7u||le32(d)!=u32(0x7d68a4u))return;
            Frame L(*this,0x140);
            const std::uint32_t W=L(0),buf=L(0x20);
            p32(W,0x627c48u);p32(W+4u,buf);p32(W+8u,0);p32(W+0xcu,0x100);p32(W+0x10u,0);
            put_value(W,0x18,1);put_value(W,2,1);
            put_value(W,le32(d),4);put_value(W,le32(d+4u),4);put_value(W,u32(0x850b14u),2);
            queue_4f4ed0(p,W);
            return;
        }
        if(p!=u32(s+0x5cu)||u32(0x850b10u)!=9u||le32(d)!=u32(0x7d68a4u))return;
        const std::uint32_t t=le32(d+4u);
        if(t<=u32(0x850b28u))return;
        std::int32_t lag=std::int32_t((u32(0x850b18u)-t)<<10);
        p32(0x850b28u,t);
        const std::int32_t w=std::int32_t(u8(d+8u)|(std::uint32_t(u8(d+9u))<<8));
        const std::int32_t c=i32(0x850b20u);
        if(c!=-1){
            const std::int32_t avg=(lag+c)/2;
            p32(0x850b20u,std::uint32_t(avg));
            p32(0x850b14u,std::uint32_t(((w<<11)-avg+i32(0x850b14u))/2));
        }else{
            p32(0x850b20u,std::uint32_t(lag));
            p32(0x850b14u,std::uint32_t((w<<11)-lag));
        }
    }
    // 4F55D0: a guest asks the host for the start sync (0x18 2, counter, frame).
    void start_ask_4f55d0(){
        Frame L(*this,0x140);
        const std::uint32_t W=L(0),buf=L(0x20);
        p32(W,0x627c48u);p32(W+4u,buf);p32(W+8u,0);p32(W+0xcu,0x100);p32(W+0x10u,0);
        put_value(W,0x18,1);put_value(W,2,1);put_value(W,u32(0x7d68a4u),4);put_value(W,u32(0x850b18u),4);
        queue_4f4ed0(u32(u32(0x7d68acu)+0x5cu),W);
    }
    // 4F6020(reader, player): a 0x18 message: 0 a clock (457410), 2 the start sync, 1 a
    // player's progress (+85C.. of that player, relayed by its owner to the lobby players).
    void control_in_4f6020(std::uint32_t m,std::uint32_t p){
        if(!u32(0x7d68acu))return;
        Frame L(*this,0x20);
        read_491860(m,L(0),1);
        const std::uint32_t kind=u8(L(0));
        if(kind==0u){clock_457410(u32(m)+u32(m+0xcu),u16(p));return;}
        if(kind==2u){start_sync_4f53d0(u32(m)+u32(m+0xcu),p);return;}
        if(kind!=1u)return;
        read_491860(m,L(4),4);
        const std::uint32_t s=u32(0x7d68acu),id=u32(L(4));
        std::uint32_t q=0;
        for(std::int32_t i=0;i<i32(s+0x14u);++i)if(u16(player_at(s,std::uint32_t(i)))==id){q=player_at(s,std::uint32_t(i));break;}
        if(!q)return;
        read_491860(m,L(8),4);read_491860(m,L(0xc),1);
        const std::uint32_t b=u8(L(0xc));
        p8(q+0x864u,b>>7);p8(q+0x85cu,1);p32(q+0x860u,u32(L(8)));p32(q+0x868u,b&3u);
        if(q!=p)return;
        for(std::int32_t i=0;i<i32(u32(0x7d68acu)+0x14u);++i){
            const std::uint32_t ses=u32(0x7d68acu),r=player_at(ses,std::uint32_t(i));
            if(r==u32(ses+0x58u))continue;
            const std::uint32_t st=u32(r+8u);
            if(st!=1u&&st!=2u)continue;
            Frame M(*this,0x140);
            const std::uint32_t W=M(0),buf=M(0x20);
            p32(W,0x627c48u);p32(W+4u,buf);p32(W+8u,0);p32(W+0xcu,0x100);p32(W+0x10u,0);
            put_value(W,0x18,1);put_value(W,1,1);put_value(W,id,4);put(W,L(8),4);put_value(W,b,1);
            queue_4f4ed0(r,W);
        }
    }
    // 457790(time, flag, word, -, id, word2) / 4578D0(time, word, -, id, word2): player id's
    // result (finished +5C, its time / words), its car closed (457790 only; [7DE784] on a flag).
    void result_457790(std::uint32_t t,std::uint32_t f,std::uint32_t w,std::uint32_t id,std::uint32_t w2){
        const std::int32_t idx=commrace_index(id);
        if(f)p8(0x7de784u,1);
        const std::uint32_t r=0x7de418u+std::uint32_t(idx)*0x6cu;
        p8(r+0x5cu,1);p32(r+0x3cu,f);p32(r+0x28u,t);p32(r+0x40u,w2);p32(r+0x44u,w);p32(r+0x58u,u32(0x7f1938u));
        for(std::uint32_t k=0;std::int32_t(k)<i32(0x7de418u);++k)
            if(u32(u32(0x799d18u+k*0x3cu)+0x1054u)==std::uint32_t(idx)){call(0x4401d0u,{k+8u});break;}
    }
    void result_4578d0(std::uint32_t t,std::uint32_t w,std::uint32_t id,std::uint32_t w2){
        const std::uint32_t r=0x7de418u+std::uint32_t(commrace_index(id))*0x6cu;
        p32(r+0x28u,t);p32(r+0x40u,w2);p8(r+0x5cu,1);p32(r+0x3cu,0);p32(r+0x44u,w);p32(r+0x58u,u32(0x7f1938u));
    }
    // 4F5970 / 4F59F0(reader, player): the result messages.
    void result_in_4f5970(std::uint32_t m,std::uint32_t p){
        Frame L(*this,0x20);
        read_491860(m,L(0),2);read_491860(m,L(4),2);read_491860(m,L(8),2);read_491860(m,L(0xc),4);read_491860(m,L(0x10),4);
        result_457790(u16(L(0)),u16(L(4)),u16(L(8)),u16(p),u32(L(0x10)));
    }
    void result_in_4f59f0(std::uint32_t m,std::uint32_t p){
        Frame L(*this,0x20);
        read_491860(m,L(0),2);read_491860(m,L(4),2);read_491860(m,L(8),4);read_491860(m,L(0xc),4);
        result_4578d0(u16(L(0)),u16(L(4)),u16(p),u32(L(0xc)));
    }
    // ---- the CommRace block set-up (race start) -----------------------------------------
    // 456560(ecx = entry, record): a player entry from a race start record: its session
    // player (by id; else the session's first), choices, rating and name from the player
    // (or the record), the clock; +8 set.
    void entry_456560(std::uint32_t e,std::uint32_t rec){
        for(std::uint32_t k=0;k<0x6cu;k+=4)p32(e+k,0);
        p32(e+4u,u32(rec));p32(e,0);
        const std::uint32_t s=u32(0x7d68acu);
        std::uint32_t pl=0;
        if(s)for(std::int32_t i=0;i<i32(s+0x14u);++i)if(u16(player_at(s,std::uint32_t(i)))==u32(rec)){pl=player_at(s,std::uint32_t(i));break;}
        p32(e,pl);
        if(pl){
            p8(e+9u,u8(pl+0x94u));p8(e+0xau,u8(pl+0x98u));p32(e+0x1cu,u32(pl+0x80u));
            for(std::uint32_t k=0;k<0x10u;k+=4)p32(e+0xbu+k,u32(pl+0x20u+k));
        }else{
            p32(e,u32(u32(0x7d68acu)+0x18u));
            p8(e+9u,u8(rec+0x18u));p8(e+0xau,u8(rec+0x19u));p32(e+0x1cu,u32(rec+0x1cu));
            for(std::uint32_t k=0;k<0x10u;k+=4)p32(e+0xbu+k,u32(rec+8u+k));
        }
        p8(e+0x1bu,0);p32(e+0x54u,u32(0x7f1938u));p8(e+8u,1);
    }
    // 4566C0(ecx = block): the block's entries from the race start records (850B1C, count
    // [850B34]); its packed word state cleared.
    void block_4566c0(std::uint32_t b){
        const std::uint32_t n=u32(0x850b34u);
        p32(b,n);
        for(std::uint32_t i=0;i<u32(b);++i)entry_456560(b+4u+i*0x6cu,u32(0x850b1cu)+i*0x20u);
        p8(b+0x36cu,0);p32(b+0x370u,0);p32(b+0x374u,0);
    }
    // 456CB0: the CommRace block built, the 7EE1F0 counters reset.
    void commrace_456cb0(){block_4566c0(0x7de418u);p32(0x7ee1f0u,0);p32(0x7ee1f4u,0);p32(0x7ee1f8u,0xf0);p32(0x7ee204u,0);}
    // 456870 / 456B40(ecx = block): no connected player still racing / the connected count.
    bool all_done_456870(std::uint32_t b){
        for(std::uint32_t k=0;k<u32(b);++k){
            if(!connected_456ae0(b,k))continue;
            if(u8(b+k*0x6cu+0x5cu)!=1u)return false;
        }
        return true;
    }
    std::uint32_t connected_456b40(std::uint32_t b){std::uint32_t n=0;for(std::uint32_t k=0;k<u32(b);++k)if(connected_456ae0(b,k))++n;return n;}
    // 455C30(esi = car slot): a remote car's state (7DFC58 + k * 0x330) cleared.
    void car_slot_455c30(std::uint32_t o){
        for(std::uint32_t off:{0xcu,0x34u,0x5cu,0x84u,0xacu,0xd4u,0x124u,0x174u,0x19cu,0x1c4u,0x228u,0x250u,0x278u,0x2a0u,0x2f0u})
            for(std::uint32_t k=0;k<0x28u;k+=4)p32(o+off+k,0);
        for(std::uint32_t k=0;k<0x10u;k+=4)p32(o+0x214u+k,0);
        p16(o+0x224u,0);p8(o+0x226u,0);p8(o+0x227u,0);p32(o+0x318u,0);
        p32(o,0);p32(o+4u,0);p32(o+8u,0xffffffffu);
        p32(o+0x320u,0);p32(o+0x324u,0);p32(o+0x328u,0);p16(o+0x32cu,0);
    }
    // 455EF0: the state histories (7EE240 / 7DF350) emptied, the clock 0.
    void histories_455ef0(){
        for(std::uint32_t c=0;c<0x900u;c+=0x24u){
            for(std::uint32_t k=0;k<6u;++k)p32(0x7ee240u+c+k*0x900u,0x7fff);
            p32(0x7df350u+c,0x7fff);
        }
        for(std::uint32_t k=0;k<6u;++k)p32(0x7df334u+k*4u,0);
        p32(0x7f1938u,0);
    }
    // 459210: the race network state reset for a LAN race: eight car slots, the result
    // tables, the race ids 7EE010 from the start records, the histories.
    void race_reset_459210(){
        const std::uint32_t n=u32(u32(0x7d68acu)+0x14u),recs=u32(0x850b1cu);
        for(std::uint32_t k=0;k<8u;++k){
            const std::uint32_t a=0x7df1d0u+k*0x2cu,b=k*0xcu,d=k*4u;
            p32(0x7de3b0u+b,0);p32(0x7de3b4u+b,0);p32(0x7de3b8u+b,0);
            p32(0x7debfcu+d,0x7fffffffu);p32(0x7ee030u+d,1);p32(0x7ee210u+d,0);
            pf(a+0x18u,0.0f);p32(a,0);pf(a+0x1cu,0.0f);
            pf(0x7edfb0u+b,0.0f);pf(0x7edfb4u+b,0.0f);pf(0x7edfb8u+b,0.0f);
            p32(0x7ee050u+d,0);
            car_slot_455c30(0x7dfc58u+k*0x330u);
            p32(0x7e15d8u+d,1);p32(0x7de390u+d,0);p32(0x7f184cu+d,0);p32(0x7e16c8u+d,0);p32(0x7ee010u+d,0);
        }
        for(std::uint32_t k=0;k<0x108u;++k)p32(0x7dece8u+k*4u,0);
        for(std::uint32_t a:{0x7df1ccu,0x7de410u,0x7df19cu,0x7e16ecu,0x7df330u})p32(a,0);
        p32(0x7de388u,0xffffffffu);p32(0x7f1848u,6);
        for(std::uint32_t a:{0x7de790u,0x7ee23cu,0x7debf4u,0x7debf8u,0x7f1880u,0x7f187cu,0x7e16e8u,0x7f1874u})p32(a,0);
        p16(0x7f186cu,0xffff);p32(0x7f1870u,0);p32(0x7f1878u,0);
        for(std::uint32_t k=0;k<8u;++k)p32(0x7dec34u+k*0x18u,0);
        pf(0x7ee208u,0.0f);p32(0x7f1840u,0);
        for(std::uint32_t i=0;std::int32_t(i)<std::int32_t(n);++i)p32(0x7ee010u+i*4u,u32(recs+i*0x20u));
        histories_455ef0();
    }
    // ---- the guest side of the host messages (515C30) ------------------------------------
    // 453E70(ecx = session, id): the player with that id; 0 if none.
    std::uint32_t player_by_id_453e70(std::uint32_t s,std::uint32_t id){
        for(std::int32_t i=0;i<i32(s+0x14u);++i)if(u16(player_at(s,std::uint32_t(i)))==id)return player_at(s,std::uint32_t(i));
        return 0;
    }
    // 4F4740(ecx = player, byte): its state, vote and info flags from a state byte (4F4700).
    void set_state_byte_4f4740(std::uint32_t p,std::uint32_t b){
        p32(p+8u,b&0xfu);
        if(b&0x60u)p32(p+0xcu,((b&0x60u)>>5)-1u);
        for(std::uint32_t i=0;std::int32_t(i)<i32(p+0x18u);++i)p8(p+0x4bu+i*0x88u,(b&(0x10u<<i))?1u:0u);
    }
    // 5153E0 / 515410 / 515440 / 5155A0 / 515660 / 5156A0 / 515780(reader): host messages
    // 0..6: the online counters, the course setup, the players (ids and state bytes: unknown
    // ones queued in +86, missing ones removed), the online ids, the raw lobby data 63AA78,
    // the race start records, the start counter's ready flag.
    std::uint32_t counters_in_5153e0(std::uint32_t m){
        if(u32(0x7d68acu)==0x98a650u){read_491860(m,0x77ff90u,0x14);method(0x434eb0u,0x77ff90u);}
        return 1;
    }
    std::uint32_t setup_in_515410(std::uint32_t m){
        read_491860(m,0x65a7a4u,0x28);
        if(u32(0x7d68acu)==0x98a650u)method(0x434eb0u,0x77ff90u);
        return 1;
    }
    std::uint32_t players_in_515440(std::uint32_t m){
        p32(u32(0x7d68acu)+0x54u,0);
        std::uint32_t ok=1;std::int32_t i=0;
        auto keep=[&](std::uint32_t s,std::uint32_t p){return p==u32(s+0x58u)||p==u32(s+0x5cu);};
        Frame L(*this,0x10);
        while(i32(m+0xcu)<i32(m+4u)){
            read_491860(m,L(0),2);read_491860(m,L(4),1);
            const std::uint16_t id=u16(L(0));const std::uint32_t b=u8(L(4));
            {const std::uint32_t s=u32(0x7d68acu);p16(s+0x40u+u32(s+0x54u)*2u,id);p32(s+0x54u,u32(s+0x54u)+1u);}
            std::uint32_t s=u32(0x7d68acu);
            while(i<i32(s+0x14u)){
                const std::uint32_t p=player_at(s,std::uint32_t(i));
                if(id<=u16(p))break;
                if(keep(s,p))++i;else{remove_player_454bb0(s,p);s=u32(0x7d68acu);}
            }
            if(i<i32(s+0x14u)&&u16(player_at(s,std::uint32_t(i)))==id){
                const std::uint32_t p=player_at(s,std::uint32_t(i));
                if(p!=u32(s+0x58u))set_state_byte_4f4740(p,b);
                ++i;continue;
            }
            if(u16(u32(s+0x58u))==id||u16(u32(s+0x5cu))==id)continue;
            for(std::uint32_t k=0;k<4u;++k)if(!u16(s+0x86u+k*2u)){p16(s+0x86u+k*2u,id);break;}
            ok=0;
        }
        std::uint32_t s=u32(0x7d68acu);
        while(i<i32(s+0x14u)){
            const std::uint32_t p=player_at(s,std::uint32_t(i));
            if(keep(s,p))++i;else{remove_player_454bb0(s,p);s=u32(0x7d68acu);}
        }
        return ok;
    }
    std::uint32_t online_ids_in_5155a0(std::uint32_t m){
        std::uint32_t ok=1;std::int32_t i=0;
        Frame L(*this,0x10);
        while(i32(m+0xcu)<i32(m+4u)){
            read_491860(m,L(0),2);read_491860(m,L(4),4);
            const std::uint32_t s=u32(0x7d68acu);
            const std::uint32_t w=u16(L(0));const std::uint16_t base=std::uint16_t(w&0xfffcu);
            while(i<i32(s+0x14u)&&base>u16(player_at(s,std::uint32_t(i))))++i;
            if(i<i32(s+0x14u)&&u16(player_at(s,std::uint32_t(i)))==base){
                const std::uint32_t p=player_at(s,std::uint32_t(i));
                if(p!=u32(s+0x58u))p32(p+(w&3u)*0x88u+0x58u,u32(L(4)));
            }else ok=0;
        }
        return ok;
    }
    std::uint32_t lobby_in_515660(std::uint32_t m){
        p32(0x63aa7cu,u32(m+4u));p32(0x63aa84u,0);
        for(std::uint32_t k=0;k<u32(m+4u);++k)p8(u32(0x63aa78u)+k,u8(u32(m)+k));
        return 1;
    }
    std::uint32_t starts_in_5156a0(std::uint32_t m){
        Frame L(*this,0x10);
        read_491860(m,L(0),1);
        const std::uint32_t n=u32(m+4u)-u32(m+0xcu);
        read_491860(m,u32(0x850b1cu),n);
        p32(0x850b34u,n>>5);p32(0x850b38u,n>>5);p8(0x850b2cu,1);p32(0x850b30u,u8(L(0)));
        return 1;
    }
    std::uint32_t ready_in_515780(std::uint32_t m){
        Frame L(*this,0x10);
        read_491860(m,L(0),1);
        if(u8(L(0))==u32(0x7d68a4u))p8(0x850b24u,1);
        return 1;
    }
    // 5157B0(fn, esi = reader, ebx = k): host message k with a newer sequence than ours
    // (while this player has a state) is read by fn; its sequence kept when fn accepts it.
    void sequenced_5157b0(std::uint32_t m,std::uint32_t k){
        Frame L(*this,0x10);
        read_491860(m,L(0),4);
        const std::uint32_t s=u32(0x7d68acu);
        if(!u32(u32(s+0x58u)+8u))return;
        const std::int32_t seq=i32(L(0));
        if(i32(s+0x68u+k*4u)>=seq)return;
        std::uint32_t ok=0;
        switch(k){
        case 0:ok=counters_in_5153e0(m);break;
        case 1:ok=setup_in_515410(m);break;
        case 2:ok=players_in_515440(m);break;
        case 3:ok=online_ids_in_5155a0(m);break;
        case 4:ok=lobby_in_515660(m);break;
        case 5:ok=starts_in_5156a0(m);break;
        default:ok=ready_in_515780(m);break;
        }
        if(ok&0xffu)p32(u32(0x7d68acu)+0x68u+k*4u,std::uint32_t(seq));
    }
    // 515B60(esi = reader): the host's welcome: this player's id, its address port as seen by
    // the host, then this player's state (0x0C) sent back.
    void welcome_515b60(std::uint32_t m){
        Frame L(*this,0x560);
        read_491860(m,L(0),2);
        const std::uint32_t me=u32(u32(0x7d68acu)+0x58u);
        set_id_4f46a0(me,u16(L(0)));
        own_player_4f4590(u32(u32(0x7d68acu)+0x58u));
        const std::uint32_t A=L(0x10);
        p16(A+4u,0xa1ef);
        read_address_491800(A,m);
        {const std::uint32_t p=u32(u32(0x7d68acu)+0x58u);if(u16(p+0x248u)!=u16(A+4u))p16(p+0x248u,u16(A+4u));}
        const std::uint32_t W=L(0x20),buf=L(0x40);
        p32(W,0x627c48u);p32(W+4u,buf);p32(W+8u,0);p32(W+0xcu,0x518);p32(W+0x10u,0);
        put_value(W,0xc,1);
        write_state_4f4790(u32(u32(0x7d68acu)+0x58u),W);
        queue_4f4ed0(u32(u32(0x7d68acu)+0x5cu),W);
    }
    // 5152B0(esi = reader): the start counter, the host's state; this player starts (2).
    void start_in_5152b0(std::uint32_t m){
        Frame L(*this,0x10);
        read_491860(m,L(0),4);
        p32(0x7d68a8u,u32(L(0)));
        read_state_4f4950(u32(u32(0x7d68acu)+0x5cu),m);
        const std::uint32_t s=u32(0x7d68acu),me=u32(s+0x58u);
        if(!u32(me+8u)){p32(me+8u,2);p8(u32(0x7d68acu)+6u,1);}
        else p8(s+6u,1);
    }
    // 515300(esi = reader): another player's state (made when new).
    void other_in_515300(std::uint32_t m){
        const std::uint32_t pos=u32(m+0xcu);
        Frame L(*this,0x10);
        read_491860(m,L(0),2);
        p32(m+0xcu,pos);
        std::uint32_t p=player_by_id_453e70(u32(0x7d68acu),u16(L(0)));
        if(!p)p=new_player_453df0(u32(0x7d68acu));
        if(p)read_state_4f4950(p,m);
    }
    // 515350(eax = reader): the host's frame / timeout: [+A4] (-1 without), and +84 set
    // when the host's sequence total is ahead of ours.
    void sync_in_515350(std::uint32_t m){
        Frame L(*this,0x10);
        read_491860(m,L(0),4);
        if(i32(m+0xcu)>=i32(m+4u))p32(u32(0x7d68acu)+0xa4u,0xffffffffu);
        else{read_491860(m,L(4),2);p32(u32(0x7d68acu)+0xa4u,u16(L(4)));}
        const std::uint32_t s=u32(0x7d68acu);
        std::uint32_t sum=0;for(std::uint32_t k=0;k<7u;++k)sum+=u32(s+0x68u+k*4u);
        if(i32(L(0))>std::int32_t(sum))p8(s+0x84u,1);
    }
    // 515C30(type, reader): a host message to a guest (both not gone): 0..6 sequenced, 11
    // welcome, 13 start, 14 another player, 15 sync, 17 the host's goodbye (codes 1..4 end
    // the session with reason 5..8), 20 nothing. 0 for other types.
    std::uint32_t host_in_515c30(std::uint32_t type,std::uint32_t m){
        const std::uint32_t s=u32(0x7d68acu);
        if(u32(u32(s+0x58u)+8u)==7u||u32(u32(s+0x5cu)+8u)==7u)return 0;
        if(type<=6u){sequenced_5157b0(m,type);return 1;}
        auto bye=[&](std::uint32_t reason,bool set){
            goodbye_4f4cd0(u32(u32(0x7d68acu)+0x5cu),5);
            if(!set)return;
            clear_event_48f8e0(0x830bf0u);
            p32(0x830bf0u,7);p32(0x830bf4u,reason);
        };
        switch(type){
        case 11:welcome_515b60(m);return 1;
        case 13:start_in_5152b0(m);return 1;
        case 14:other_in_515300(m);return 1;
        case 15:sync_in_515350(m);return 1;
        case 17:{
            Frame L(*this,0x10);read_491860(m,L(0),1);
            switch(u8(L(0))){
            case 0:bye(0,false);break;
            case 1:bye(5,true);break;
            case 2:bye(6,true);break;
            case 3:bye(7,true);break;
            case 4:bye(8,true);break;
            default:break;
            }
            return 1;}
        case 20:return 1;
        default:return 0;
        }
    }
    // ---- the host side of the guests' messages (516C10) ----------------------------------
    void new_writer(std::uint32_t W,std::uint32_t buf,std::uint32_t cap){p32(W,0x627c48u);p32(W+4u,buf);p32(W+8u,0);p32(W+0xcu,cap);p32(W+0x10u,0);}
    // 516B10(esi = address, reader): a stranger asking where the host is: the reply 0x12
    // with the host's address, sent to the address the request carries.
    void where_516b10(std::uint32_t addr,std::uint32_t m){
        Frame L(*this,0x1100);
        const std::uint32_t T1=L(0x20),T2=L(0x890),W=L(0),buf=L(0x14);
        player_433270(T1);
        read_address_491800(T1+0x244u,m);
        if(addr&&u16(T1+0x248u)!=u16(addr+4u))p16(T1+0x248u,u16(addr+4u));
        new_writer(W,buf,0x14);
        player_433270(T2);
        address_4917a0(T2+0x244u,u32(u32(0x7d68acu)+0x5cu)+0x244u,0xa1ef,0,0);
        put_value(W,0xb,2);put_value(W,0x12,1);
        write_address_4917d0(T2+0x244u,W);
        send_491b60(W,T1+0x244u,0);
    }
    // 516850(ecx = reader, eax = key, esi = player, address): a join request: the key must
    // match the player's +4; the wanted info count must fit the free places (online: the
    // shared counters); the player gets an id (453FE0) and the 0x0B welcome (id, address).
    void join_516850(std::uint32_t m,std::uint32_t key,std::uint32_t p,std::uint32_t addr){
        if(key!=u32(p+4u)){goodbye_4f4cd0(p,5);return;}
        Frame L(*this,0x560);
        read_491860(m,L(0),1);read_491860(m,L(4),1);
        const std::uint32_t want=u8(L(0)),c=u8(L(4));
        read_address_491800(p+0x244u,m);
        if(addr&&u16(p+0x248u)!=u16(addr+4u))p16(p+0x248u,u16(addr+4u));
        if(!u32(p+0x18u)){
            const std::uint32_t s=u32(0x7d68acu);
            if(s==0x98a650u){
                const std::int32_t a=i32(0x77ff90u),b=i32(0x77ff94u),cc=i32(0x77ff98u),d=i32(0x77ff9cu);
                const std::int32_t room=c?(a-d-cc):(a-cc-b);
                if(std::int32_t(want)>room){goodbye_4f4cd0(p,1);return;}
                std::int32_t take=0;
                if(c)take=std::min(std::int32_t(want),b-d);
                p32(0x77ff98u,std::uint32_t(cc+std::int32_t(want)-take));
                p32(0x77ff9cu,std::uint32_t(d+take));
                p32(p+0x14u,std::uint32_t(take));
            }else{
                weights_453f80(s,L(8),L(0xc));
                if(std::int32_t(want)>i32(L(0xc))){goodbye_4f4cd0(p,1);return;}
                p32(p+0x14u,0);
            }
            p32(p+0x18u,want);
            new_id_453fe0(L(0x10));
            set_id_4f46a0(p,u16(L(0x10)));
        }else if(u32(p+0x18u)!=want){goodbye_4f4cd0(p,5);return;}
        const std::uint32_t W=L(0x20),buf=L(0x40);
        new_writer(W,buf,0x518);
        put_value(W,0xb,1);put_value(W,u16(p),2);
        write_address_4917d0(p+0x244u,W);
        queue_4f4ed0(p,W);
    }
    // 516A10(eax = reader, esi = player): a guest's state; it starts (2); the 0x0D answer
    // (the start counter, less one in the lobby screens, and the host's state).
    void guest_state_516a10(std::uint32_t m,std::uint32_t p){
        read_state_4f4950(p,m);
        if(!u32(p+8u))p32(p+8u,2);
        Frame L(*this,0x560);
        const std::uint32_t W=L(0),buf=L(0x20);
        new_writer(W,buf,0x518);
        put_value(W,0xd,1);
        put_value(W,u32(0x7d68a4u)-((net_flags()&5u)?1u:0u),4);
        write_state_4f4790(u32(u32(0x7d68acu)+0x5cu),W);
        queue_4f4ed0(p,W);
    }
    // 516460(esi = reader, player): the guest's sequences (the host messages it lacks marked
    // +258..) and its four pending player ids (+264).
    void guest_seq_516460(std::uint32_t m,std::uint32_t p){
        Frame L(*this,0x10);
        for(std::uint32_t k=0;k<7u;++k){
            read_491860(m,L(0),4);
            if(i32(L(0))<i32(u32(0x7d68acu)+0x68u+k*4u))p8(p+0x258u+k,1);
        }
        std::uint32_t i=0;
        while(i32(m+0xcu)<i32(m+4u)){read_491860(m,L(0),2);p16(p+0x264u+i*2u,u16(L(0)));++i;}
        for(;i<4u;++i)p16(p+0x264u+i*2u,0);
    }
    // 491C50(ecx = reader, &kind, out): a tagged record (6-bit kind, 1..4 bytes).
    void tagged_491c50(std::uint32_t m,std::uint32_t kind,std::uint32_t out){
        const std::uint32_t h=u8(u32(m)+u32(m+0xcu));p32(m+0xcu,u32(m+0xcu)+1u);
        p32(kind,h&0x3fu);
        read_491860(m,out,(h>>6)+1u);
    }
    // 516500(esi = reader, player): the guest's state byte, then its tagged records (read).
    void guest_flags_516500(std::uint32_t m,std::uint32_t p){
        Frame L(*this,0x10);
        read_491860(m,L(0),1);
        set_state_byte_4f4740(p,u8(L(0)));
        while(i32(m+0xcu)<i32(m+4u))tagged_491c50(m,L(4),L(8));
    }
    // 516550: a guest's start answer (0x16: the counter, its first info id) to the host.
    void start_answer_516550(){
        Frame L(*this,0x540);
        const std::uint32_t W=L(0),buf=L(0x20);
        new_writer(W,buf,0x518);
        put_value(W,0x16,1);put_value(W,u8(0x7d68a4u),1);put_value(W,u8(u32(u32(0x7d68acu)+0x58u)+0x48u),1);
        queue_4f4ed0(u32(u32(0x7d68acu)+0x5cu),W);
    }
    // 516C10(type, reader, player, address): a guest's message to the host (not from a gone
    // player): 8 sequences, 9 a ping, 10 a join, 12 its state, 16 flags, 17 a goodbye (0:
    // answered), 18 where-is-the-host. 0 for others.
    std::uint32_t guest_in_516c10(std::uint32_t type,std::uint32_t m,std::uint32_t p,std::uint32_t addr){
        const std::uint32_t st=u32(p+8u);
        if(st==7u)return 0;
        switch(type){
        case 8:if(!st)return 0;guest_seq_516460(m,p);return 1;
        case 9:return st?1u:0u;
        case 10:{Frame L(*this,0x10);read_491860(m,L(0),4);join_516850(m,u32(L(0)),p,addr);return 1;}
        case 12:guest_state_516a10(m,p);return 1;
        case 16:if(!st)return 0;guest_flags_516500(m,p);return 1;
        case 17:{Frame L(*this,0x10);read_491860(m,L(0),1);if(!u8(L(0)))goodbye_4f4cd0(p,5);return 1;}
        case 18:where_516b10(addr,m);return 1;
        default:return 0;
        }
    }
    // 516CF0 / 516AC0(type, reader, address): from a stranger: 0x12 where-is-the-host; 0x0A
    // a join (a new player with the key).
    std::uint32_t stranger_where_516cf0(std::uint32_t type,std::uint32_t m,std::uint32_t addr){
        if(type!=0x12u)return 0;
        where_516b10(addr,m);return 1;
    }
    std::uint32_t stranger_join_516ac0(std::uint32_t type,std::uint32_t m,std::uint32_t addr){
        if(type!=0xau)return 0;
        const std::uint32_t p=new_player_453df0(u32(0x7d68acu));
        if(!p)return 0;
        Frame L(*this,0x10);
        read_491860(m,L(0),4);
        p32(p+4u,u32(L(0)));
        join_516850(m,u32(L(0)),p,addr);
        return 1;
    }
    // ---- the per-player keep-alive messages (host 5165F0, guest 515800) -----------------
    // 4F4B90(ecx = player): its packet may go now (something queued, its +254 timer within
    // the session's colour period [+90], or a raw tail once 7 frames passed).
    bool may_send_4f4b90(std::uint32_t p){
        if(u32(p+0x2a0u))return true;
        if(i32(p+0x254u)>i32(u32(0x7d68acu)+0x90u))return true;
        return i32(p+0x28cu)>7&&u32(p+0x858u);
    }
    // 4918A0(ecx = writer, kind, value): a tagged record (kind | (bytes - 1) << 6, then the
    // value's 1..4 low bytes).
    void tagged_4918a0(std::uint32_t w,std::uint32_t kind,std::uint32_t v){
        const std::uint32_t n=v<0x100u?1u:(v<0x10000u?2u:(v<0x1000000u?3u:4u));
        put_value(w,((n-1u)<<6)|(kind&0xffu),1);
        put_value(w,v,n);
    }
    // 5165F0(player, index): the host's frame for a guest: its goodbye when asked (+26C);
    // the timeout (0xF0 frames in a race, else 0x168: 0); every 7 frames when sendable a
    // pending player's state (0x0E), the host messages it lacks (and message 2 while the
    // host's vote is fresh), every 10 the sequence total (0x0F, the frame +A4 in the lobby).
    std::uint32_t host_frame_5165f0(std::uint32_t p,std::uint32_t){
        const std::uint32_t st=u32(p+8u);
        if(st==7u)return 0;
        if(u8(p+0x26cu)){goodbye_4f4cd0(p,u8(p+0x26du)?3u:2u);return 1;}
        if(!st)return 1;
        const std::uint32_t limit=(u32(u32(u32(0x7d68acu)+0x5cu)+8u)==3u&&st==3u)?0xf0u:0x168u;
        p32(p+0x254u,u32(p+0x254u)+call(0x43fa00u));
        if(i32(p+0x254u)>=std::int32_t(limit))return 0;
        const bool c=may_send_4f4b90(p);
        if(c&&i32(p+0x288u)>7){
            std::uint32_t i=0;
            while(i<4u&&!u16(p+0x264u+i*2u))++i;
            if(i<4u){
                const std::uint32_t q=player_by_id_453e70(u32(0x7d68acu),u16(p+0x264u+i*2u));
                if(q&&q!=p&&q!=u32(u32(0x7d68acu)+0x5cu)){
                    Frame L(*this,0x540);
                    const std::uint32_t W=L(0);
                    new_writer(W,W+0x14u,0x518);
                    put_value(W,0xe,1);
                    write_state_4f4790(q,W);
                    queue_4f4ed0(p,W);
                }
                p16(p+0x264u+i*2u,0);p32(p+0x288u,0);
            }
        }
        std::uint32_t sum=0;
        for(std::uint32_t k=0;k<7u;++k){
            const std::uint32_t s=u32(0x7d68acu);
            if((c&&u8(p+0x258u+k)&&i32(p+0x288u)>7)||(k==2u&&i32(s+0x98u)<6)){
                queue_4f4ed0(p,0x63afb0u+k*0x530u);
                p8(p+0x258u+k,0);p32(p+0x288u,0);
            }
            sum+=u32(u32(0x7d68acu)+0x68u+k*4u);
        }
        if(c&&i32(p+0x290u)>10){
            Frame L(*this,0x540);
            const std::uint32_t W=L(0),buf=L(0x20);
            new_writer(W,buf,0x518);
            put_value(W,0xf,1);put_value(W,sum,4);
            if(st==1u||st==2u)put_value(W,u16(u32(0x7d68acu)+0xa4u),2);
            queue_4f4ed0(p,W);
            p32(p+0x290u,0);
        }
        return 1;
    }
    // 515800: a guest's frame for its host: (not joined) the join request 0x0A every 4 frames
    // (key 7D6888, one info, the online flag, its address); else the host timeout (0x12C:
    // goodbye 0), its sequences and pending ids (0x08) every 7 frames, and its state (0x10:
    // state byte, race records as tagged values) every 10 frames or while the vote is fresh.
    std::uint32_t guest_frame_515800(){
        const std::uint32_t host=u32(u32(0x7d68acu)+0x5cu);
        if(u32(host+8u)==7u)return 0;
        Frame L(*this,0x540);
        const std::uint32_t W=L(0),buf=L(0x20);
        if(!u32(u32(u32(0x7d68acu)+0x58u)+8u)){
            if(i32(host+0x290u)<=4)return 1;
            new_writer(W,buf,0x518);
            put_value(W,0xa,1);put_value(W,u32(0x7d6888u),4);put_value(W,1,1);
            put_value(W,u32(0x7d68acu)==0x98a650u?(i32(0x98a644u)<0?1u:0u):0u,1);
            write_address_4917d0(u32(u32(0x7d68acu)+0x58u)+0x244u,W);
            queue_4f4ed0(host,W);
            p32(host+0x290u,0);
            return 1;
        }
        p32(host+0x254u,u32(host+0x254u)+call(0x43fa00u));
        if(i32(host+0x254u)>=0x12c){goodbye_4f4cd0(host,0);return 0;}
        const bool c=may_send_4f4b90(host);
        {const std::uint32_t s=u32(0x7d68acu);
            const bool pending=u8(s+0x84u)||u16(s+0x86u)||u16(s+0x88u)||u16(s+0x8au)||u16(s+0x8cu);
            if(pending&&c&&i32(host+0x288u)>7){
                new_writer(W,W+0x14u,0x518);
                put_value(W,8,1);
                for(std::uint32_t k=0;k<7u;++k)put_value(W,u32(u32(0x7d68acu)+0x68u+k*4u),4);
                for(std::uint32_t k=0;k<4u;++k){
                    const std::uint32_t a=u32(0x7d68acu)+0x86u+k*2u;
                    if(u16(a)){put_value(W,u16(a),2);p16(u32(0x7d68acu)+0x86u+k*2u,0);}
                }
                queue_4f4ed0(host,W);
                p8(u32(0x7d68acu)+0x84u,0);p32(host+0x288u,0);
            }}
        if(!((c&&i32(host+0x290u)>10)||i32(u32(0x7d68acu)+0x98u)<6))return 1;
        new_writer(W,buf,0x518);
        put_value(W,0x10,1);
        put_value(W,state_byte_4f4700(u32(u32(0x7d68acu)+0x58u)),1);
        for(std::int32_t i=0;i<i32(u32(0x7d68acu)+0x60u);++i){
            const std::uint32_t rec=0x7d68e4u+std::uint32_t(i)*0x14u;
            if(i32(rec)<0)continue;
            const std::uint16_t me=u16(u32(u32(0x7d68acu)+0x58u));
            if(std::uint16_t(u16(rec-4u)&0xfffcu)!=me&&std::uint16_t(u16(rec-2u)&0xfffcu)!=me)continue;
            const std::int32_t rem=i32(0x7d68a4u)%3;
            tagged_4918a0(W,std::uint32_t(i+rem*20),u32(rec));
        }
        queue_4f4ed0(host,W);
        p32(host+0x290u,0);
        return 1;
    }
    // ---- the race start of a LAN session (4F5270) -----------------------------------------
    std::uint32_t crt_rand(){return frontend_crt_random_580f40(c.event_function36.pc_crt_random_state);}   // 580F40
    // 490290: the host's race start (the lobby data 63AA78 from message 4, at least 13
    // bytes): its counter and sequence (not newer than ours); outside the lobby screens the
    // course setup, the race records (7D68E0) and the start records (850B1C) are taken.
    std::uint32_t take_start_490290(){
        if(!u32(0x7d68acu))return 0;
        const std::uint32_t R=0x63aa78u;
        if(i32(R+4u)<0xd)return 0;
        p32(R+0xcu,5);
        Frame L(*this,0x20);
        read_491860(R,L(0),4);
        const std::uint32_t counter=u32(L(0));
        read_491860(R,L(4),4);
        if(u32(L(4))>u32(u32(0x7d68acu)+0x6cu))return 0;
        if(net_flags()&1u)return 1;
        read_491860(R,0x65a7a4u,0x28);
        if(u32(0x7d68acu)==0x98a650u)method(0x434eb0u,0x77ff90u);
        p32(0x7d68a4u,counter);
        p32(u32(0x7d68acu)+0x60u,0);
        p32(0x850b30u,u32(0x7d68a4u));
        read_491860(R,L(8),1);
        for(std::uint32_t n=u8(L(8));n;--n){
            const std::uint32_t rec=0x7d68e0u+u32(u32(0x7d68acu)+0x60u)*0x14u;
            read_491860(R,L(0xc),2);
            p16(rec+2u,u16(L(0xc)));p16(rec,u16(L(0xc)));
            p32(u32(0x7d68acu)+0x60u,u32(u32(0x7d68acu)+0x60u)+1u);
        }
        read_491860(R,L(8),1);
        const std::uint32_t n2=u8(L(8));
        p32(0x850b34u,n2);
        for(std::uint32_t k=0;k<n2;++k){
            const std::uint32_t rec=u32(0x850b1cu)+k*0x20u;
            read_491860(R,L(0xc),1);p32(rec+4u,u8(L(0xc)));
            read_491860(R,L(0xc),4);p32(rec,u32(L(0xc)));
            read_491860(R,L(0xc),1);p8(rec+0x18u,u8(L(0xc)));
            read_491860(R,L(0xc),1);p8(rec+0x19u,u8(L(0xc)));
            read_491860(R,L(0xc),4);p32(rec+0x1cu,u32(L(0xc)));
            read_491860(R,rec+8u,0x10);
        }
        p32(0x63aa7cu,0);
        return 0;
    }
    // 4904E0: this player has a race record; else the start counter is taken as done.
    std::uint32_t in_race_4904e0(){
        const std::uint32_t s=u32(0x7d68acu);
        const std::uint16_t me=u16(u32(s+0x58u));
        for(std::int32_t i=0;i<i32(s+0x60u);++i)
            if(std::uint16_t(u16(0x7d68e0u+std::uint32_t(i)*0x14u)&0xfffcu)==me)return 1;
        p32(0x7d68a8u,u32(0x7d68a4u));
        return 0;
    }
    // 4F5320 / 4F5360 / 4F53A0: the race start state 850B10 reset / armed (2) / idle (1).
    void start_reset_4f5320(){p8(0x850b04u,0);p8(0x850b05u,0);p8(0x850b2cu,0);p32(0x850b10u,0);}
    void start_arm_4f5360(){if(!u32(0x850b10u)){p8(0x850b04u,0);p32(0x850b10u,2);}}
    // 4F5150: the event-4 work [799C28] for a LAN race: this player's slot / transmission
    // bytes (4591F0 / 4591C0), a random colour, the arcade setup (48B190 variant, 4C05F0).
    void race_work_4f5150(){
        const std::uint32_t w=u32(0x799c28u);
        p8(w+0x1cu,0xff);
        p8(w+0x1du,u32(0x7d68acu)?u8(0x7de425u+std::uint32_t(u8(0x7dd138u))*0x6cu):0u);   // 4591F0
        p8(w+0x20u,crt_rand()&3u);
        p8(w+0x1fu,1);
        {std::uint32_t v=0;   // 4591C0
            if(u32(0x7d68acu)){v=u8(0x7de426u+std::uint32_t(u8(0x7dd138u))*0x6cu);v=v<1u?1u:(v>8u?8u:v);}
            p8(w+0x1eu,v);}
        p8(w+0x24u,0xff);
        bool one;
        if(u8(0x65a7acu)==1u)one=(u32(0x65a7a8u)&0xff000000u)==0x1000000u;
        else one=i8(w+0x1du)>=0xf;
        p8(0x83036du,one?1u:0u);                                       // 48B190
        call(0x4c05f0u,{w});
    }
    // 4F5270(ecx = session): the race starts: the host's start taken (490290), the CommRace
    // block (456CB0), the start state reset, the course flag, the screens' state 4, the race
    // variant flag (49B450), the event-4 work; one more game counted ([7C24E8]); started.
    void race_begin_4f5270(){
        if(!u32(0x7d68acu))return;
        (void)take_start_490290();
        commrace_456cb0();
        start_reset_4f5320();
        p32(0x850b00u,std::uint32_t(std::int32_t(i8(0x65a7a6u))));
        p32(0x7d68a8u,u32(0x7d68a4u));
        p32(0x659930u+0x14u,4);                                        // 4EDCE0
        p32(0x8369c0u,1);                                              // 49B450
        race_work_4f5150();
        pf(0x7c24e8u,f32(0x7c24e8u)+f32(0x62806cu));
        p8(u32(0x7d68acu)+8u,1);
    }
    // 4F50D0: the host's new race: the race records from the players, the counter advanced.
    void new_race_4f50d0(){
        const std::uint32_t s=u32(0x7d68acu);
        if(!s||!u8(s+7u))return;
        p32(s+0x60u,u32(s+0x14u));
        for(std::int32_t i=0;i<i32(s+0x60u);++i){
            const std::uint32_t rec=0x7d68e0u+std::uint32_t(i)*0x14u;
            const std::uint16_t id=u16(player_at(s,std::uint32_t(i)));
            p16(rec+2u,id);p16(rec,id);p32(rec+4u,0xffffffffu);p32(rec+8u,0xffffffffu);p8(rec+0xcu,0);p32(rec+0x10u,0);
        }
        p32(0x7d68a4u,u32(0x7d68a4u)+1u);
        p32(0x7d688cu,u32(s+0x6cu));
        p8(s+0x64u,0);
    }
    // 4F5F00(id, time, total): this player's race finish (message 0x1D: the clock 7F1938, the
    // id, the time as float bits, the total) to every racing player but this one.
    void finish_send_4f5f00(std::uint32_t id,std::uint32_t time,std::uint32_t total){
        if(!u32(0x7d68acu))return;
        Frame L(*this,0x1020);
        const std::uint32_t W=L(0);
        new_writer(W,L(0x20),0x1000);
        put_value(W,0x1d,1);
        put_value(W,u32(0x7f1938u),2);                                 // 455D40
        put_value(W,id,2);
        put_value(W,time,4);
        put_value(W,total,4);
        for(std::int32_t i=0;i<player_count(u32(0x7d68acu));++i){
            const std::uint32_t s=u32(0x7d68acu);
            const std::uint32_t p=player_at(s,std::uint32_t(i));
            if(u32(p+8u)!=3u||!racing_4575b0(u16(p))||p==u32(s+0x58u))continue;
            queue_4f4ed0(p,W);
        }
    }
    // ---- LAN discovery: the search listener (430B40) asks with 0x1A broadcasts, the host
    // answers 0x1B with its session (4307B0), the replies fill the found list 7DBDB8
    // (0x90 each, at most 25; [659930] the count) ---------------------------------------
    // 4307B0(ecx = reader): the host's answer to a 0x1A search: its address, the free / used
    // weights (453F80), the names 7C23E0 / 65A7B8, the lobby flag (56EE70 & 5), the 13
    // course bytes 65A7A4.., sent back to the asker. 1 when answered.
    // vtable call by slot index (vcall takes the byte offset)
    std::uint32_t vslot(std::uint32_t o,std::uint32_t i,std::initializer_list<std::uint32_t> a={}){return vcall(o,i*4u,a);}
    std::uint32_t reply_4307b0(std::uint32_t r){
        const std::uint32_t s=u32(0x7d68acu);
        if(s!=0x988dc8u||!u8(s+5u)||!u8(s+7u))return 0;
        Frame L(*this,0x560);
        const std::uint32_t tmp=L(0),used=L(4),free=L(8),W=L(0x10),buf=L(0x30);
        read_491860(r,tmp,1);
        if(u8(tmp)!=0x1au)return 0;
        new_writer(W,buf,0x518);
        put_value(W,0,2);
        put_value(W,0x1b,1);
        write_address_4917d0(u32(u32(0x7d68acu)+0x58u)+0x244u,W);
        weights_453f80(u32(0x7d68acu),used,free);
        put_value(W,u8(free),1);
        put_value(W,u8(used),1);
        write_491900(W,0x7c23e0u,0x11);
        write_491900(W,0x65a7b8u,0x11);
        put_value(W,(u32(0x659930u+0x14u)&5u)?1u:0u,1);                // 56EE70
        for(std::uint32_t k=0;k<13u;++k)put_value(W,u8(0x65a7a4u+k),1);
        const std::int32_t n=i32(W+8u)-2;
        p32(W+0x10u,0);
        put_value(W,n>0?std::uint32_t(n):0u,2);
        method(0x491b60u,W,{r+0x10u,0u,0u});
        return 1;
    }
    // 430CB0(eax = listener, edi = reader, &from): a 0x1B answer: a new host (by its ip)
    // appended to the found list (the list restarts on the first answer of a search).
    void found_430cb0(std::uint32_t self,std::uint32_t r,std::uint32_t from){
        Frame L(*this,0x20);
        const std::uint32_t A=L(0),tmp=L(0x10);
        p16(A+4u,0xa1ef);
        read_address_491800(A,r);
        const std::uint32_t ip=u32(from);
        if(!u8(self+0x18u)){p32(0x659930u,0);p8(self+0x18u,1);}
        const std::int32_t n=i32(0x659930u);
        for(std::int32_t k=0;k<n;++k)if(u32(0x7dbdd8u+std::uint32_t(k)*0x90u)==ip)return;
        if(n>=0x19)return;
        const std::uint32_t e=0x7dbdb8u+std::uint32_t(n)*0x90u;
        p32(0x659930u,std::uint32_t(n+1));
        p32(e+0x20u,ip);
        auto byte=[&]{read_491860(r,tmp,1);return std::uint32_t(u8(tmp));};
        p32(e+0x24u,byte());p32(e+0x28u,0);
        p32(e+0x2cu,byte());
        read_491860(r,e+0x30u,0x11);p8(e+0x40u,0);
        read_491860(r,e+0x41u,0x11);p8(e+0x51u,0);
        p8(e+0x52u,byte()?1u:0u);
        for(std::uint32_t k=0;k<13u;++k)p32(e+0x54u+k*4u,byte());
    }
    // 430B40(ecx = search listener): every 15 frames of its countdown (+14) a 0x1A ask
    // (to +1C, or broadcast), then one answer read (game socket, else the raw one); the
    // countdown's end closes it (an empty list when nothing answered).
    void search_430b40(std::uint32_t self){
        if(u32(self+0x14u)){
            Frame L(*this,0xb00);
            const std::uint32_t tmp=L(0),A=L(4),R=L(0x10),buf=L(0x30),W=L(0x550),wbuf=L(0x570);
            if(i32(self+0x14u)%15==0){
                new_writer(W,wbuf,0x518);
                put_value(W,1,2);
                put_value(W,0x1a,1);
                method(0x491b60u,W,{u32(self+0x1cu)?self+0x1cu:0u,0u,0u});
            }
            p32(R,buf);p32(R+4u,0);p32(R+8u,0x518);p32(R+0xcu,0);p16(R+0x14u,0xa1ef);p16(A+4u,0xa1ef);
            if((method(0x491a90u,R,{0u,A,0u})&0xffu)||(method(0x491a90u,R,{1u,A,0u})&0xffu)){
                read_491860(R,tmp,2);
                read_491860(R,tmp,1);
                if(u8(tmp)==0x1bu)found_430cb0(self,R,A);
            }
            p32(self+0x14u,u32(self+0x14u)-1u);
            if(u32(self+0x14u))return;
            if(!u8(self+0x18u))p32(0x659930u,0);
        }
        clear_event_48f8e0(self+4u);
    }
    // 430B00(ecx = search listener): reset (no countdown, the target address cleared).
    void search_reset_430b00(std::uint32_t self){
        p32(self+0x14u,0);
        event_clear_48f900(self);
        Frame L(*this,0x10);
        p32(L(0),0);
        address_4917a0(self+0x1cu,L(0),0xa1ef,0,0);
    }
    // 430EE0(ecx = listener): the event armed (1), state 5.
    void arm_430ee0(std::uint32_t self){clear_event_48f8e0(self+4u);p32(self+4u,1);p32(self+0x14u,5);}
    // 48FF00(state): the lobby screen state [7D68D8]; the LAN session (or a guest of the
    // online one) is left (454220(1)), then the session's +4 method runs.
    void lobby_state_48ff00(std::uint32_t v){
        p32(0x7d68d8u,v);
        std::uint32_t s=u32(0x7d68acu);
        if(!s){p8(0x7d68d4u,1);return;}
        if(s!=0x98a650u||!u8(s+7u)){p8(0x7d68d4u,1);s=u32(0x7d68acu);}
        vslot(s,1,{0u});
    }
    // 491CD0(ecx = join listener): +14 5: the join starts (session +1C method, the host
    // player 453DF0 at the address 7D689C:[7D6898]); 6: wait for this player's state (7:
    // done) for 0x258 frames, then on the LAN menu (owner page 7) the next found host is
    // tried (48FF00 0x21) or the list is done (0x22); elsewhere the menu event 830BF0 = 7 / 9.
    void join_frame_491cd0(std::uint32_t self){
        const std::uint32_t st=u32(self+0x14u);
        if(st==5u){
            vslot(u32(0x7d68acu),7,{0u});
            const std::uint32_t h=new_player_453df0(u32(0x7d68acu));
            p32(u32(0x7d68acu)+0x5cu,h);
            address_4917a0(u32(u32(0x7d68acu)+0x5cu)+0x244u,0x7d689cu,u16(0x7d6898u),0,0);
            p32(self+0x14u,6);p32(self+0x18u,0);
            return;
        }
        if(st!=6u)return;
        const std::uint32_t me=u32(u32(0x7d68acu)+0x58u);
        if(me&&u32(me+8u)){p32(self+0x14u,7);clear_event_48f8e0(self+4u);return;}
        const std::uint32_t ev=u32(0x830bf0u);
        if(ev==0u||ev==1u){
            p32(self+0x18u,u32(self+0x18u)+1u);
            if(i32(self+0x18u)<=0x258)return;
            if(method(0x442ef0u,call(0x4035f0u))==7u&&!u32(0x68c81cu)){
                p32(self+0x14u,0);clear_event_48f8e0(self+4u);
                std::int32_t k=i32(0x659934u)+1;
                const std::int32_t n=i32(0x659930u);
                while(k<n&&!u32(0x7dbddcu+std::uint32_t(k)*0x90u))++k;
                p32(0x659934u,std::uint32_t(k));
                if(k<n){p32(0x659938u,std::uint32_t(n));p32(0x65993cu,std::uint32_t(k));lobby_state_48ff00(0x21);}
                else{p32(0x659938u,0);p32(0x65993cu,0);lobby_state_48ff00(0x22);}
                return;
            }
            clear_event_48f8e0(0x830bf0u);p32(0x830bf0u,7);p32(0x830bf4u,9);
        }
        p32(self+0x14u,0);clear_event_48f8e0(self+4u);
    }
    // 431BA0(dst, src, n): src copied into dst, cut at n - 1 bytes, terminated.
    void copy_431ba0(std::uint32_t dst,std::uint32_t src,std::uint32_t n){
        if(!n)return;
        std::uint32_t len=0;while(u8(src+len))++len;
        if(len>n-1u)len=n-1u;
        for(std::uint32_t k=0;k<len;++k)p8(dst+k,u8(src+k));
        p8(dst+len,0);
    }
    // 435FF0(ecx = online query): reset: its six handles released, its task (+30) deleted.
    void query_reset_435ff0(std::uint32_t o){
        p8(o+0x395u,0);p8(o+0x394u,0);p8(o+0x396u,0);p32(o+0x38cu,0);p32(o+0x390u,0);
        for(std::uint32_t off:{0x24u,0x28u,0x2cu,0x18u,0x1cu,0x20u})if(u32(o+off)){release_525f80(u32(o+off));p32(o+off,0);}
        if(u32(o+0x30u)){vslot(u32(o+0x30u),0,{1u});p32(o+0x30u,0);}
        p32(o+0x14u,9);
        event_clear_48f900(o);
    }
    // 4940D0(ecx = service 83612C): the online query 98A708 reset, the request lists cleared.
    void service_reset_4940d0(std::uint32_t o){
        query_reset_435ff0(0x98a708u);
        p32(o+0x13d20u,u32(o+0x13d1cu));p32(o+0x13d1cu,0);p32(o+0x13d24u,0);p32(o+0x1db20u,0);p8(o+0x13d34u,0);
        for(std::uint32_t i=0;i<15u;++i){
            const std::uint32_t a=o+0x1d9f8u+i*0x14u,c=o+0x30u+i*0x40u;
            p32(a-4u,0);p32(a,0);p8(a+4u,1);p32(c-0x1cu,0xffffffffu);p32(c,0);p32(c+4u,0);
        }
    }
    // 515700(reader): message 0x16 while the start state is 10: the player with that +48
    // id is marked (+240). 1 when taken.
    std::uint32_t reset_in_515700(std::uint32_t m){
        Frame L(*this,0x10);
        read_491860(m,L(0),1);
        read_491860(m,L(0),1);
        const std::uint32_t id=u8(L(0));
        if(u32(0x850b10u)!=0xau)return 0;
        const std::uint32_t s=u32(0x7d68acu);
        for(std::int32_t i=0;i<player_count(s);++i){
            const std::uint32_t p=player_at(s,std::uint32_t(i));
            if(u16(p+0x48u)==id){p8(p+0x240u,1);break;}
        }
        return 1;
    }
    // 51C000(ecx): a zero vector pair, +2C 0xFF8F268F, the other words cleared.
    void vector_51c000(std::uint32_t o){
        for(std::uint32_t k=0;k<0x18u;k+=4u)p32(o+k,0);
        p32(o+0x28u,0);p32(o+0x2cu,0xff8f268fu);p32(o+0x34u,0);p8(o+0x24u,0);p32(o+0x38u,0);
    }
    // 459BB0 / 459D10 / 45A0B0 (the network module's copies; see race_traffic_network.inc).
    std::uint32_t oldest_459bb0(std::uint32_t s){
        std::uint32_t r=u32(0x7f1938u);
        for(std::uint32_t k=0;k<u32(s);++k)
            if(connected_456ae0(s,k)&&u32(s+4u+k*0x6cu+0x24u)<r)r=u32(s+4u+k*0x6cu+0x24u);
        return r;
    }
    std::uint32_t rank_459d10(std::uint32_t s,std::uint32_t idx){
        const std::uint32_t r=s+idx*0x6cu;
        const std::uint32_t fin=u32(r+0x3cu),gave=u32(r+0x44u);
        std::uint32_t n=0;
        if(fin>oldest_459bb0(s))return 0xffffffffu;
        if(fin){
            const std::uint32_t t=u32(r+0x40u);
            for(std::uint32_t k=0;k<u32(s);++k){
                const std::uint32_t o=s+k*0x6cu;
                if(!u32(o+0x3cu))continue;
                const std::uint32_t x=u32(o+0x40u);
                if(t>x||(t==x&&std::int32_t(idx)>std::int32_t(k)))++n;
            }
            return n;
        }
        if(u8(r+0x5cu)){
            for(std::uint32_t k=0;k<u32(s);++k){
                const std::uint32_t o=s+k*0x6cu;
                if(u32(o+0x3cu)){++n;continue;}
                const std::uint32_t x=u32(o+0x44u);
                if(gave<x||(gave==x&&std::int32_t(idx)>std::int32_t(k)))++n;
            }
            return n;
        }
        if(connected_456ae0(s,idx))return 0xffffffffu;
        for(std::uint32_t k=0;k<u32(s);++k)
            if(connected_456ae0(s,k)||std::int32_t(idx)>std::int32_t(k))++n;
        return n;
    }
    // ---- profile ratings shown in the lobby (the profile 7C23E0: +1DD the per-course
    // grades, 5 per category; +28 the unlock bits) ----------------------------------------
    // 495930(category): the number of course records (5C1D6C, 0x44 each) of that category's
    // key ([836370] + category * 8), when the data 836358 is ready (3). The frontend counts
    // them once (frontend_license_progress_tables): its category_counts, 0 before.
    std::uint32_t course_count_495930(std::uint32_t cat){
        const auto& t=c.event_function36.frontend_category_progress;
        if(!t.categories_ready||cat>=t.category_counts.size())return 0;
        return t.category_counts[cat];
    }
    // 4E9FD0(stage, course type, profile): the stage's score, the sum of its first
    // [5CE954 + stage * 4] results (nibbles of the profile's word +36E + type * 2: low byte
    // low / high nibble, then the high byte's), each + 1 unless 7 (none).
    std::uint32_t stage_score_4e9fd0(std::uint32_t stage,std::uint32_t type,std::uint32_t profile){
        if(!profile)profile=0x7c23e0u;
        static constexpr std::uint32_t results_5ce954[15]={1,1,1,2,2,2,3,3,3,3,4,4,4,4,4};
        if(stage>=15u)throw std::runtime_error("4E9FD0: stage outside the 5CE954 table");
        const std::uint32_t w=u16(profile+0x36eu+type*2u);
        std::uint32_t sum=0;
        for(std::uint32_t j=0;j<results_5ce954[stage];++j){
            const std::uint32_t g=j==1u?(w>>4)&0xfu:j==2u?(w>>8)&0xfu:j==3u?w>>12:w&0xfu;   // 4E9DE0
            if(g!=7u)sum+=g+1u;
        }
        return sum;
    }
    // 4E81D0(category, profile): the mean grade of the category's courses (7: none or
    // one course without a grade).
    std::uint32_t grade_4e81d0(std::uint32_t cat,std::uint32_t profile){
        const std::uint32_t n=course_count_495930(cat)&0xffu;
        std::int32_t sum=0;
        for(std::uint32_t k=0;k<n;++k){
            const std::uint32_t v=u8(profile+cat*5u+0x1ddu+k);
            if(v==7u)return 7;
            sum+=std::int32_t(v);
        }
        if(!n)return 7;
        return std::uint32_t(sum/std::int32_t(n));
    }
    // 4E8230(kind, profile): the mean of the 15 stage scores (4E9FD0) of the mission set
    // [84B7E0 + kind * 4], as a grade (sum / 41 - 1; 7 when negative or no set). The sets
    // are the frontend's (4E8620 loads them): the types +1C of their first 15 records.
    std::uint32_t mission_grade_4e8230(std::uint32_t kind,std::uint32_t profile){
        const auto& t=c.event_function36.frontend_category_progress;
        if(kind>=t.championship_present.size()||!t.championship_present[kind])return 7;
        std::int32_t sum=0;
        for(std::uint32_t i=0;i<15u;++i)
            sum+=std::int32_t(stage_score_4e9fd0(i,t.championship_types[kind][i],profile));
        const std::int32_t g=sum/41-1;
        return g>=0?std::uint32_t(g):7u;
    }
    // 4E82A0(kind, profile): the grade of a group (0: categories 0..4, 1: 5..13, 2: 14..24,
    // 3: 25..39, 4..6: mission sets 0..2), profile 0 = 7C23E0; 7 when none.
    std::uint32_t group_grade_4e82a0(std::uint32_t kind,std::uint32_t profile){
        if(!profile)profile=0x7c23e0u;
        if(kind>6u)return 7;
        if(kind>=4u)return mission_grade_4e8230(kind-4u,profile);
        static constexpr std::uint32_t from[4]={0,5,14,25},to[4]={5,14,25,40};
        std::int32_t sum=0;
        for(std::uint32_t c=from[kind];c<to[kind];++c){
            const std::uint32_t g=grade_4e81d0(c,profile);
            if(g!=7u)sum+=std::int32_t(g)+1;
        }
        const std::int32_t q=sum/std::int32_t(to[kind]-from[kind])-1;
        return q>=0?std::uint32_t(q):7u;
    }
    // 4474E0(ecx = profile, bit): an unlock bit (+28).
    std::uint32_t bit_4474e0(std::uint32_t o,std::uint32_t bit){
        const std::int32_t b=std::int32_t(bit);
        return (u8(o+0x28u+std::uint32_t(b/8))&(1u<<(b%8)))?1u:0u;
    }
    // 4C8FC0(row, column): the unlock of a table cell 68EAC8 (-2 locked, -1 open, else a
    // profile bit); 447730(index, column): row [632E8C + index], column - 1.
    std::uint32_t unlocked_4c8fc0(std::uint32_t row,std::uint32_t col){
        const std::int32_t v=i16(0x68eac8u+(col+row*8u)*2u);
        if(v==-2)return 0;
        if(v==-1)return 1;
        return bit_4474e0(0x7c23e0u,std::uint32_t(v));
    }
    // ==== lobby widgets: the text box (vtable 5C18A0 over 5C1888 over 5C18C8) and the item
    // list (48D970 / 48DDA0); the text lines are wrapped into 8303E8 (16 lines of 0x80) ====
    static std::uint32_t fb(float v){std::uint32_t b;std::memcpy(&b,&v,4);return b;}
    std::int32_t ftrunc(float v){return std::int32_t(v);}                               // cvttss2si
    std::uint32_t slen(std::uint32_t a){std::uint32_t n=0;while(u8(a+n))++n;return n;}
    static constexpr std::uint32_t Lines=0x8303e8u,LinesEnd=0x830be8u;
    // (the widget base 48F480 / 48F4D0 and its input 48F5F0 are the frontend's: called out)
    // 48E4D0 / 48E500: the positioned widget (5C1888: +34 / +38 position, +3C / +40).
    void placed_48e4d0(std::uint32_t o){method(0x48f480u,o);p32(o,0x5c1888u);for(std::uint32_t k=0x34u;k<=0x40u;k+=4u)pf(o+k,0.0f);}
    // 48E640: the text box's defaults (font 9, colour FFDDDDDD, size D, scale 1, 0x400 max).
    void text_reset_48e640(std::uint32_t o){
        const float one=f32(0x62806cu);
        p8(o+0x4eu,0);p32(o+0x450u,9);p32(o+0x458u,0);p8(o+0x470u,1);p32(o+0x45cu,0x400);
        p32(o+0x474u,0xffddddddu);p32(o+0x454u,0xd);pf(o+0x478u,one);pf(o+0x47cu,one);
        p32(o+0x46cu,0);p32(o+0x464u,0);p32(o+0x460u,0);p32(o+0x468u,0);
        p32(o+0x480u,1);p8(o+0x4cu,1);p8(o+0x4du,0);p32(o+0x484u,0xe);p32(o+0x488u,0);
    }
    // 48E590: the text box constructed.
    void text_48e590(std::uint32_t o){placed_48e4d0(o);text_reset_48e640(o);p32(o,0x5c18a0u);}
    // 48EE80(ecx-less: box, format, ...): the box text printed (vsnprintf 0x400), its length.
    void text_print_48ee80(std::uint32_t o,std::uint32_t fmt,const std::vector<std::uint32_t>& va){
        Frame L(*this,0x40);
        for(std::size_t k=0;k<va.size()&&k<16u;++k)p32(L(std::uint32_t(k)*4u),va[k]);
        call(0x580265u,{o+0x4eu,0x400u,fmt,L(0)});
        p32(o+0x458u,slen(o+0x4eu));
    }
    // 48F2D0(ecx = box, style): the text and the style (48D570 layout) taken.
    void text_style_48f2d0(std::uint32_t o,std::uint32_t st){
        text_print_48ee80(o,st,{});
        p32(o+0x45cu,u32(st+0x408u));
        for(std::uint32_t k=0;k<16u;k+=4u)p32(o+0x460u+k,u32(st+0x40cu+k));
        if(i32(o+0x468u)-i32(o+0x460u)>0&&i32(o+0x46cu)-i32(o+0x464u)>0)p8(o+0x4du,1);
        p32(o+0x450u,u32(st+0x400u));p32(o+0x474u,u32(st+0x420u));p32(o+0x454u,u32(st+0x404u));
        p32(o+0x47cu,u32(st+0x428u));p32(o+0x478u,u32(st+0x424u));p8(o+0x470u,u8(st+0x41cu));
        p32(o+0x34u,u32(st+0x42cu));p32(o+0x38u,u32(st+0x430u));                       // 48E510
        p32(o+0x480u,u32(st+0x434u));
    }
    // 48D570(ecx = style): the default style (font 9, size 4, 0x400 chars, colour FF3F474A,
    // scale 1, the 640 x 480 box, no text).
    void style_48d570(std::uint32_t st){
        const float one=f32(0x62806cu);
        p8(st+0x41cu,1);p32(st+0x434u,1);p32(st+0x400u,9);p32(st+0x404u,4);p32(st+0x408u,0x400);p32(st+0x420u,0xff3f474au);
        p8(st,0);pf(st+0x424u,one);pf(st+0x428u,one);pf(st+0x42cu,one);pf(st+0x430u,one);
        p32(st+0x410u,0);p32(st+0x418u,0x280);p32(st+0x40cu,0);p32(st+0x414u,0x1e0);
    }
    // 48EB60(ecx = box): the text word-wrapped into the lines (the width +468 - +460 by
    // 42C370), +488 lines skipped; the unused lines cleared.
    void wrap_48eb60(std::uint32_t o){
        const std::int32_t W=i32(o+0x468u)-i32(o+0x460u);
        call(0x42c360u,{0x100u});
        call(0x42c370u,{0x62646cu});
        call(0x42c370u,{0x5c18c4u});
        Frame L(*this,0x90);
        const std::uint32_t B=L(0);
        p8(B,0);
        const std::uint32_t t=o+0x4eu;
        const std::int32_t n=std::int32_t(slen(t))+1;
        std::int32_t line=-i32(o+0x488u),col=0,linew=0,wlen=0;
        auto lineat=[&]{return Lines+std::uint32_t(line)*0x80u;};
        auto visible=[&]{return line>=0&&line<16;};
        for(std::int32_t i=0;i<n;++i){
            const std::uint32_t c=u8(t+std::uint32_t(i));
            if(c!=0x20u&&c!=0x2du&&c!=0xau&&c!=0u&&col+wlen!=0x7f){
                p8(B+std::uint32_t(wlen),c);++wlen;p8(B+std::uint32_t(wlen),0);
                if(std::int32_t(call(0x42c370u,{B}))+linew<=W)continue;
                if(col==0){
                    --i;--wlen;p8(B+std::uint32_t(wlen),0);
                    if(visible()){for(std::int32_t k=0;k<wlen;++k)p8(lineat()+std::uint32_t(k),u8(B+std::uint32_t(k)));p8(lineat()+std::uint32_t(wlen),0);}
                    ++line;
                    if(line==16)break;
                    wlen=0;col=0;
                    continue;
                }
                if(visible())p8(lineat()+std::uint32_t(col),0);
                ++line;
                if(line==16)break;
                col=0;linew=0;
                continue;
            }
            if(visible())for(std::int32_t k=0;k<wlen;++k)p8(lineat()+std::uint32_t(col+k),u8(B+std::uint32_t(k)));
            col+=wlen;
            if(c==0xau||c==0u||col==0x7f){
                wlen=0;
                if(visible())p8(lineat()+std::uint32_t(col),0);
                ++line;
                if(line==16)break;
                linew=0;col=0;
                continue;
            }
            p8(B+std::uint32_t(wlen),c);p8(B+std::uint32_t(wlen)+1u,0);
            linew+=std::int32_t(call(0x42c370u,{B}));
            wlen=0;
            if(visible())p8(lineat()+std::uint32_t(col),c);
            ++col;
        }
        if(line<16)for(std::int32_t k=line<0?0:line;k<16;++k)p8(Lines+std::uint32_t(k)*0x80u,0);
    }
    // 48EA50(ecx = box, text): the text cut into lines of at most +45C - 1 chars (0x80 max)
    // at spaces or at a "<newline>" marker.
    void lines_48ea50(std::uint32_t o,std::uint32_t text){
        const std::uint32_t end=text+slen(text);
        if(i32(o+0x45cu)>0x80)p32(o+0x45cu,0x80);
        for(std::uint32_t line=Lines;line<LinesEnd;line+=0x80u){
            for(std::uint32_t k=0;k<0x80u;k+=4u)p32(line+k,0);
            if(text>=end)continue;
            std::uint32_t take=u32(o+0x45cu)-1u,skip=0;
            const std::uint32_t mark=strstr_581000(text,0x5c18b8u);
            if(mark&&mark-text<0x1fu){skip=9;take=mark-text;}
            else{
                const std::int32_t max=i32(o+0x45cu)-1;
                std::int32_t a=std::int32_t(end-text);
                if(max<a)a=max;
                if(a>=max&&a>0){
                    while(a>0){if(u8(text+std::uint32_t(a))==0x20u){skip=1;take=std::uint32_t(a);break;}--a;}
                }
            }
            for(std::uint32_t k=0;k<take;++k)p8(line+k,u8(text+k));
            text+=skip+take;
        }
    }
    // 581000: strstr.
    std::uint32_t strstr_581000(std::uint32_t h,std::uint32_t nd){
        const std::uint32_t n=slen(nd);
        if(!n)return h;
        for(std::uint32_t p=h;u8(p);++p){
            std::uint32_t k=0;while(k<n&&u8(p+k)==u8(nd+k))++k;
            if(k==n)return p;
        }
        return 0;
    }
    // 48F0B0(ecx = box): the wrapped lines drawn (+480 flags 0x1000 / 0x2000: centred /
    // bottom-aligned on the last line), +484 the line height.
    void draw_wrapped_48f0b0(std::uint32_t o){
        wrap_48eb60(o);
        call(0x42c360u,{u32(o+0x480u)});
        std::int32_t off=0;
        const std::uint32_t fl=u32(o+0x480u);
        if(fl&0x3000u){
            std::int32_t h=0;
            for(std::uint32_t k=1;k<=16u;++k)if(slen(Lines+(k-1u)*0x80u))h=i32(o+0x484u)*std::int32_t(k);
            if(fl&0x1000u)off=h/2;else off=h;
        }
        for(std::uint32_t k=0;k<16u;++k){
            const std::uint32_t line=Lines+k*0x80u;
            if(!slen(line))continue;
            call(0x42c360u,{u32(o+0x480u)});
            call(0x42cc00u,{std::uint32_t(ftrunc(f32(o+0x34u))),std::uint32_t(i32(o+0x484u)*std::int32_t(k)+ftrunc(f32(o+0x38u))-off)});
            call(0x42cdd0u,{line});
        }
    }
    // 48F040(ecx = box): the text cut by 48EA50, drawn line by line.
    void draw_lines_48f040(std::uint32_t o){
        lines_48ea50(o,o+0x4eu);
        for(std::uint32_t k=0;k<16u;++k){
            const std::uint32_t line=Lines+k*0x80u;
            if(!slen(line))continue;
            call(0x42cc00u,{std::uint32_t(ftrunc(f32(o+0x34u))),std::uint32_t(i32(o+0x484u)*std::int32_t(k)+ftrunc(f32(o+0x38u)))});
            call(0x42cdd0u,{line});
        }
    }
    // 48F3C0(ecx = box): its display (vtable +C): font, scale, size, colour, flags, then
    // wrapped (+4D), one line ("%s") or cut lines (longer than +45C).
    void text_draw_48f3c0(std::uint32_t o){
        if(!u8(o+0x4cu))return;
        call(0x42ca60u,{u32(o+0x450u)});
        call(0x42cc60u,{u32(o+0x478u),u32(o+0x47cu)});
        call(0x42ccb0u,{u32(o+0x454u)});
        call(0x42cca0u,{u32(o+0x474u)});
        call(0x42c360u,{u32(o+0x480u)});
        if(u8(o+0x4du)){draw_wrapped_48f0b0(o);return;}
        const std::int32_t n=i32(o+0x458u),mx=i32(o+0x45cu);
        if(n>0&&(!mx||n<mx)){
            call(0x42cc00u,{std::uint32_t(ftrunc(f32(o+0x34u))),std::uint32_t(ftrunc(f32(o+0x38u)))});
            call(0x42cdd0u,{0x626468u,o+0x4eu});
            return;
        }
        if(n>mx&&mx>1)draw_lines_48f040(o);
    }
    // ---- the item list: +0 selected, +4 count, +8 first item (0x5E0 each, +5DC next),
    // +C the sprite table (12-byte entries), +10 / +14 the selected / normal frame,
    // +18 text mode, +19 vertical, +1A hidden, +1B no highlight, +1C texts, +1D arrows,
    // +20 step, +24 / +28 origin, +2C depth, +30 the style, +46C scrolls, +470 / +474 the
    // visible window ----------------------------------------------------------------------
    // 48D970(ecx = list, table, vertical, frame, normal, scrolls, window, style, texts).
    void list_init_48d970(std::uint32_t o,const std::uint32_t* a){
        p32(o+0xcu,a[0]);
        p32(o+4u,0);p32(o,0xffffffffu);p32(o+0x10u,a[2]);p32(o+0x14u,a[3]);
        p8(o+0x18u,(a[2]==0xffffffffu||a[3]==0xffffffffu)?0u:1u);
        p8(o+0x19u,a[1]);
        p8(o+0x46cu,a[4]);
        pf(o+0x20u,f32(0x5b018cu));
        p8(o+0x1au,0);p8(o+0x1bu,0);pf(o+0x24u,0.0f);pf(o+0x28u,0.0f);p32(o+0x2cu,0xc);
        p32(o+0x468u,0);p32(o+0x470u,0);p32(o+0x474u,a[5]);p32(o+8u,0);p8(o+0x1cu,0);
        if(a[6]){p8(o+0x1cu,a[7]);for(std::uint32_t k=0;k<0x438u;k+=4u)p32(o+0x30u+k,u32(a[6]+k));}
        p8(o+0x1du,0);
    }
    // 48E310(ecx = list): constructed empty.
    void list_48e310(std::uint32_t o){
        style_48d570(o+0x30u);
        pf(o+0x20u,f32(0x5b018cu));
        p32(o+4u,0);p32(o+0xcu,0);p8(o+0x18u,0);p8(o+0x1au,0);p8(o+0x1bu,0);p32(o+0x468u,0);p8(o+0x46cu,0);p32(o+0x470u,0);
        p32(o+8u,0);p8(o+0x1cu,0);p8(o+0x1du,0);p32(o,0xffffffffu);p32(o+0x10u,0xffffffffu);p32(o+0x14u,0xffffffffu);
        p8(o+0x19u,1);pf(o+0x24u,0.0f);pf(o+0x28u,0.0f);p32(o+0x2cu,0xc);p32(o+0x474u,0x60);
    }
    // 48DAD0 / 48DA20(ecx = item): an item (two sprites +10 / +B0, a text box +150).
    void item_48dad0(std::uint32_t it){method(0x465160u,it+0x10u);method(0x465160u,it+0xb0u);text_48e590(it+0x150u);}
    void item_free_48da20(std::uint32_t it){
        p32(it+0x150u,0x5c18a0u);p32(it+0x150u,0x5c1888u);p32(it+0x150u,0x5c18c8u);   // 48E630 / 48E500 / 48F4D0
        method(0x465250u,it+0xb0u);method(0x465250u,it+0x10u);
    }
    // 48DA80(ecx = list, item): appended.
    void list_append_48da80(std::uint32_t o,std::uint32_t it){
        p32(it+0x5dcu,0);
        if(!u32(o+8u)){p32(o+8u,it);return;}
        std::uint32_t p=u32(o+8u);
        while(u32(p+0x5dcu))p=u32(p+0x5dcu);
        p32(p+0x5dcu,it);
    }
    // 48DB30(ecx = list, text): a text item added (the list's style); its index.
    std::uint32_t list_add_48db30(std::uint32_t o,std::uint32_t text){
        push_440d50(1);
        std::uint32_t it=call(0x5802cfu,{0x5e0u});
        if(it)item_48dad0(it);
        p32(it,u32(o+4u));p32(it+8u,1);p32(it+4u,1);p32(it+0xcu,0xffffffffu);
        const std::uint32_t box=it+0x150u;
        vslot(box,1);
        text_style_48f2d0(box,o+0x30u);
        text_print_48ee80(box,text,{});
        list_append_48da80(o,it);
        p32(o+4u,u32(o+4u)+1u);
        if(u32(o+4u)==1u)p32(o,0);
        (void)pop_440d70();
        return u32(o+4u)-1u;
    }
    // 48DC60(ecx = list, sound): the selection moved to the next enabled (+8) item; the
    // cursor sound (4249F0 1) when asked.
    void list_next_48dc60(std::uint32_t o,std::uint32_t sound){
        for(;;){
            p32(o,u32(o)+1u);
            if(i32(o)>=i32(o+4u))p32(o,0);
            std::uint32_t it=u32(o+8u),k=0;
            while(it&&k!=u32(o)){it=u32(it+0x5dcu);++k;}
            if(u32(it+8u))break;
        }
        if(sound)call(0x4249f0u,{1u});
    }
    // 48E440(ecx = list): every item deleted.
    void list_clear_48e440(std::uint32_t o){
        std::uint32_t it=u32(o+8u);
        while(it){
            method(0x465250u,it+0x10u);method(0x465250u,it+0xb0u);
            const std::uint32_t next=u32(it+0x5dcu);
            item_free_48da20(it);
            call(0x5801a7u,{it});
            it=next;
        }
        p32(o+8u,0);
    }
    // 48DDA0(ecx = list): its display: the visible window follows the selection, the
    // blinking arrows (sprite 3004B), each visible enabled item's sprite / text and frame.
    void list_draw_48dda0(std::uint32_t o){
        float x=f32(o+0x24u),y=f32(o+0x28u);
        if(u8(o+0x46cu)){
            const std::int32_t first=i32(o+0x470u),sel=i32(o);
            if(first+i32(o+0x474u)<=sel){if(sel<i32(o+4u))p32(o+0x470u,std::uint32_t(first+1));}
            else if(sel<first)p32(o+0x470u,std::uint32_t(first-1));
            else if(first<0)p32(o+0x470u,0);
        }else{p32(o+0x474u,0x60);p32(o+0x470u,0);}
        float ax=x-f32(0x628138u);
        std::int32_t t=ftrunc(f32(0x842110u))*4;                                  // 4AF500 / ftol
        t%=256;
        t-=0x80;if(t<0)t=-t;
        const std::uint32_t alpha=std::uint32_t(t+0x7f);
        const float lift=f32(o+0x20u)*f32(0x628064u);
        const std::uint32_t colour=(alpha<<24)|0xffffffu;
        const std::uint32_t tbl=u32(o+0xcu);
        auto entry=[&](std::uint32_t e,std::uint32_t k){return u32(tbl+e*12u+k*4u);};
        if(u8(o+0x1du)){
            Frame L(*this,0x10);
            bool have=false;
            if(tbl){
                for(int tries=0;tries<2&&!have;++tries)
                    have=call(0x4294c0u,{entry(u32(o+0x10u),0),L(0),L(4)})!=0xffffffffu;
            }
            const float w=have?f32(L(0)):0.0f;
            ax=ax-w+f32(0x6281c8u);
            if(u32(o+0x470u)&&!u8(o+0x1au))
                call(0x42d280u,{0x3004bu,std::uint32_t(ftrunc(ax)),std::uint32_t(ftrunc(y+f32(0x6281ccu)-lift-f32(0x628138u))),0u,
                    fb(float(i32(o+0x2cu))+f32(0x62806cu)),colour});
        }
        std::int32_t idx=0;
        for(std::uint32_t it=u32(o+8u);it;){
            const std::uint32_t next=u32(it+0x5dcu);
            const std::uint32_t s1=it+0x10u,s2=it+0xb0u;
            method(0x465250u,s1);method(0x465250u,s2);
            const std::int32_t first=i32(o+0x470u);
            if(idx<first||idx>=first+i32(o+0x474u)){++idx;it=next;continue;}
            if(!u32(it+4u)||u8(o+0x1au)){++idx;it=next;continue;}
            const std::uint32_t depth=u32(o+0x2cu);
            if(u8(o+0x18u)){
                if(u32(it+0xcu)==0xffffffffu&&u8(o+0x1cu)){
                    const std::uint32_t box=it+0x150u;
                    const float ty=f32(o+0x460u)+f32(0x6281ccu)+y;
                    const float tx=f32(o+0x45cu)+f32(0x6281c8u)+x-float(i32(o+0x468u));
                    pf(box+0x34u,tx);pf(box+0x38u,ty);                                       // 48E530
                    p32(box+0x454u,depth+1u);                                                 // 48EF40
                    vslot(box,3);
                }else{
                    const std::uint32_t e=u32(it+0xcu);
                    call(0x465860u,{entry(e,0),entry(e,1),entry(e,2),depth+1u,1u,fb(x-float(i32(o+0x468u))),fb(y),
                        0x3f800000u,0x3f800000u,0x3f800000u,0u},s1);
                    method(0x465970u,s1);
                }
                const std::uint32_t e=(std::uint32_t(idx)==u32(o)&&!u8(o+0x1bu))?u32(o+0x10u):u32(o+0x14u);
                call(0x465860u,{entry(e,0),entry(e,1),entry(e,2),depth,1u,fb(x),fb(y),0x3f800000u,0x3f800000u,0x3f800000u,0u},s2);
                method(0x465970u,s2);
            }else{
                const std::uint32_t e=u32(it+0xcu);
                const std::uint32_t frame=(std::uint32_t(idx)==u32(o))?2u:1u;
                call(0x465860u,{entry(e,0),frame,frame,depth+1u,1u,fb(x),fb(y),0x3f800000u,0x3f800000u,0x3f800000u,0u},s1);
                method(0x465970u,s1);
            }
            if(u8(o+0x19u))y+=f32(o+0x20u);else x+=f32(o+0x20u);
            ++idx;it=next;
        }
        if(u8(o+0x1du)&&idx>i32(o+0x470u)+i32(o+0x474u)&&!u8(o+0x1au))
            call(0x42d280u,{0x3004bu,std::uint32_t(ftrunc(ax)),std::uint32_t(ftrunc(y+f32(0x6281ccu)-lift)),2u,
                fb(float(i32(o+0x2cu))+f32(0x62806cu)),colour});
    }
    // 440D50 / 440D70: the 4-deep value stack 7B1194 ([7B11C0] its depth).
    void push_440d50(std::uint32_t v){const std::uint32_t d=u32(0x7b11c0u);if(d<4u){p32(0x7b1194u+d*4u,v);p32(0x7b11c0u,d+1u);}}
    std::uint32_t pop_440d70(){const std::uint32_t d=u32(0x7b11c0u);const std::uint32_t v=u32(0x7b1194u+d*4u);p32(0x7b11c0u,d-1u);return v;}
    // 48C490(ecx): the lobby info panel (5C1838): four text boxes (+48, +4D4, +960, +DEC).
    void panel_48c490(std::uint32_t o){
        method(0x48f480u,o);
        p32(o,0x5c1838u);
        for(std::uint32_t off:{0x48u,0x4d4u,0x960u,0xdecu})text_48e590(o+off);
        p8(o+0x12a5u,0);p32(o+8u,0x2d);
    }
    // ---- the message panel (5C1838): a frame of 2D parts around four text boxes (+48 the
    // title, +4D4 the body, +960 / +DEC the two answers), +1280 its position moving to
    // +128C at +1298 per frame ----------------------------------------------------------
    // 40F050 / 40EF10: vector scale / add.
    void vscale_40f050(std::uint32_t out,std::uint32_t v,float s){for(std::uint32_t k=0;k<12u;k+=4u)pf(out+k,s*f32(v+k));}
    void vadd_40ef10(std::uint32_t a,std::uint32_t b){for(std::uint32_t k=0;k<12u;k+=4u)pf(a+k,f32(a+k)+f32(b+k));}
    std::uint32_t panel_reset_48c5b0(std::uint32_t o){
        p8(o+0x34u,0);p8(o+0x35u,0);p16(o+0x36u,0);p16(o+0x38u,0);p32(o+0x3cu,0);
        p8(o+0x41u,0);p8(o+0x42u,0);p8(o+0x43u,0);p8(o+0x44u,0);p32(o+0x1278u,0);p32(o+0x127cu,0);
        p8(o+0x12a4u,0);p8(o+0x12a5u,0);p8(o+0x40u,1);
        return 1;
    }
    void panel_free_48c520(std::uint32_t o){
        p32(o,0x5c1838u);
        for(std::uint32_t off:{0xdecu,0x960u,0x4d4u,0x48u}){p32(o+off,0x5c18a0u);p32(o+off,0x5c1888u);p32(o+off,0x5c18c8u);}
        p32(o,0x5c18c8u);
    }
    // the four boxes placed relative to the panel (48CC00 / 48CEF0 tails)
    void panel_place(std::uint32_t o){
        const float x=f32(o+0x1280u),y=f32(o+0x1284u);
        const float w=float(i16(o+0x36u)),h=float(i16(o+0x38u));
        pf(o+0x48u+0x34u,x+f32(0x5b4438u));pf(o+0x48u+0x38u,y+f32(0x628138u));
        pf(o+0x4d4u+0x34u,x+f32(0x5c1854u));pf(o+0x4d4u+0x38u,y+f32(0x5c1858u));
        pf(o+0x960u+0x34u,w+x-f32(0x628264u));pf(o+0x960u+0x38u,h+y-f32(0x5c1858u));
        pf(o+0xdecu+0x34u,x+f32(0x628264u));pf(o+0xdecu+0x38u,h+y-f32(0x5c1858u));
    }
    // 48CC00(ecx = panel): the move toward the target (velocity +1298 * frame time
    // owner+DA0), stopped past the target; the boxes follow.
    void panel_move_48cc00(std::uint32_t o){
        if(u8(o+0x12a4u)){
            const std::uint32_t vel=o+0x1298u,pos=o+0x1280u;
            bool dx=f32(vel)==0.0f,dy=f32(o+0x129cu)==0.0f;
            Frame L(*this,0x10);
            vscale_40f050(L(0),vel,f32(call(0x4035f0u)+0xda0u));
            vadd_40ef10(pos,L(0));
            const float vx=f32(vel),px=f32(pos),tx=f32(o+0x128cu);
            if((0.0f>vx&&tx>=px)||(vx>0.0f&&px>=tx))dx=true;
            const float vy=f32(o+0x129cu),py=f32(o+0x1284u),ty=f32(o+0x1290u);
            if((0.0f>vy&&ty>=py)||(vy>0.0f&&py>=ty))dy=true;
            if(dx&&dy){for(std::uint32_t k=0;k<12u;k+=4u)p32(pos+k,u32(o+0x128cu+k));p8(o+0x12a4u,0);}
        }
        panel_place(o);
    }
    // 48EF90(ecx = box): the height of its text (the last non-empty wrapped line * +484).
    std::uint32_t text_height_48ef90(std::uint32_t o){
        const std::int32_t n=i32(o+0x458u),mx=i32(o+0x45cu);
        if(!u8(o+0x4du)&&n>0&&(!mx||n<mx))return u32(o+0x484u);
        if(u8(o+0x4du)){                                   // else the lines already wrapped are measured
            call(0x42ca60u,{u32(o+0x450u)});
            call(0x42cc60u,{u32(o+0x478u),u32(o+0x47cu)});
            wrap_48eb60(o);
        }
        const std::uint32_t lh=u32(o+0x484u);
        std::uint32_t h=lh,y=lh;
        for(std::uint32_t k=0;k<16u;++k,y+=lh)if(slen(Lines+k*0x80u))h=y;
        return h;
    }
    // 48CEF0(ecx = panel, grow, extra): the height from the body text (+ 0x1E with
    // answers, + extra + 0x4C); grown around its centre, or kept.
    void panel_fit_48cef0(std::uint32_t o,std::uint32_t grow,std::uint32_t extra){
        p8(o+0x12a6u,grow);p32(o+0x12a8u,extra);
        const std::uint32_t add=(u8(o+0x42u)==1u||u8(o+0x41u)==1u)?0x1eu:0u;
        const std::int32_t old=i16(o+0x38u);
        const std::uint32_t body=text_height_48ef90(o+0x4d4u);
        const std::uint32_t h=std::uint32_t(std::uint16_t(u16(o+0x12a8u)+add))+body+0x4cu;
        p16(o+0x38u,h);
        if(u8(o+0x12a6u)){
            const std::int32_t nh=std::int16_t(h);
            if(old!=nh){
                const float d=float((old-nh)/2);
                pf(o+0x1284u,d+f32(o+0x1284u));
                pf(o+0x1290u,d+f32(o+0x1290u));
                panel_place(o);
            }
        }else p16(o+0x38u,std::uint32_t(old));
        p32(o+0x12acu,std::uint32_t(ftrunc(float(std::int32_t(body))+f32(o+0x1284u)+f32(0x5c1858u))));
    }
    // 48F280(box, format, font, colour, ...): text printed with a font and colour.
    void text_set_48f280(std::uint32_t o,std::uint32_t fmt,std::uint32_t font,std::uint32_t colour,const std::vector<std::uint32_t>& va){
        Frame L(*this,0x40);
        for(std::size_t k=0;k<va.size()&&k<16u;++k)p32(L(std::uint32_t(k)*4u),va[k]);
        call(0x580265u,{o+0x4eu,0x400u,fmt,L(0)});
        p32(o+0x458u,slen(o+0x4eu));p32(o+0x450u,font);p32(o+0x474u,colour);
    }
    // 48F1B0(box, format, max, x0, y0, x1, y1, centred, font, size, colour, sx, sy, ...):
    // a boxed (wrapped) text; +458 is the format's length.
    void text_box_48f1b0(std::uint32_t o,const std::uint32_t* a,const std::vector<std::uint32_t>& va){
        Frame L(*this,0x40);
        for(std::size_t k=0;k<va.size()&&k<16u;++k)p32(L(std::uint32_t(k)*4u),va[k]);
        call(0x580265u,{o+0x4eu,0x400u,a[0],L(0)});
        p32(o+0x45cu,a[1]);p32(o+0x458u,slen(a[0]));
        for(std::uint32_t k=0;k<4u;++k)p32(o+0x460u+k*4u,a[2+k]);
        if(i32(o+0x468u)-i32(o+0x460u)>0&&i32(o+0x46cu)-i32(o+0x464u)>0)p8(o+0x4du,1);
        p32(o+0x450u,a[7]);p32(o+0x474u,a[9]);p32(o+0x454u,a[8]);p32(o+0x478u,a[10]);p32(o+0x47cu,a[11]);p8(o+0x470u,a[6]);
    }
    // 48D0C0(ecx = panel, title, body, flags, x, y, w, h, depth, wide): opened (flags 1 / 4:
    // one answer box, 2: two; 1: the "no" sprite 5, else 9).
    std::uint32_t panel_open_48d0c0(std::uint32_t o,const std::uint32_t* a){
        float x,y,w,hh;std::memcpy(&x,&a[3],4);std::memcpy(&y,&a[4],4);std::memcpy(&w,&a[5],4);std::memcpy(&hh,&a[6],4);
        p8(o+0x34u,1);p8(o+0x40u,1);
        for(std::uint32_t k=0;k<0x24u;k+=4u)p32(o+0x1280u+k,0);
        pf(o+0x1280u,x);pf(o+0x1284u,y);pf(o+0x128cu,x);pf(o+0x1290u,y);
        p8(o+0x12a4u,1);
        if(f32(0x5c1864u)>w)w=f32(0x5c1864u);
        p32(o+0x3cu,a[7]);
        p16(o+0x36u,std::uint32_t(ftrunc(w)));
        p16(o+0x38u,std::uint32_t(ftrunc(hh)));
        if(a[2]&7u)p16(o+0x38u,std::uint32_t(ftrunc(hh)+0x20));
        const std::uint32_t depth=u32(o+0x3cu);
        std::uint32_t box=o+0x48u;
        vslot(box,1);
        text_set_48f280(box,a[0],9,0xffffffffu,{});
        pf(box+0x34u,x+f32(0x5b4438u));pf(box+0x38u,y+f32(0x628138u));p32(box+0x454u,depth);
        box=o+0x4d4u;
        vslot(box,1);
        const std::int32_t bx=ftrunc(x+f32(0x5c1854u)),by=ftrunc(y+f32(0x5c1858u));
        const std::int32_t bw=ftrunc(w-f32(0x5c1860u)),bh=ftrunc(hh-f32(0x5c185cu));
        const std::uint32_t one=fb(f32(0x62806cu));
        const std::uint32_t args[12]={a[1],0xffu,std::uint32_t(bx),std::uint32_t(by),std::uint32_t(bw+bx),std::uint32_t(bh+by),1u,9u,depth,0xff000000u,one,one};
        text_box_48f1b0(box,args,{});
        pf(box+0x34u,float(bx));pf(box+0x38u,float(by));p32(box+0x454u,depth);
        box=o+0x960u;
        vslot(box,1);
        text_set_48f280(box,call(0x465eb0u,{0x1f2u}),9,0xffffffffu,{});
        pf(box+0x34u,float(i16(o+0x36u))+x-f32(0x628264u));pf(box+0x38u,float(i16(o+0x38u))+y-f32(0x5c1858u));p32(box+0x454u,depth);
        box=o+0xdecu;
        p32(o+0xde0u,2);
        vslot(box,1);
        text_set_48f280(box,call(0x465eb0u,{0x1f1u}),9,0xffffffffu,{});
        pf(box+0x34u,x+f32(0x628264u));pf(box+0x38u,float(i16(o+0x38u))+y-f32(0x5c1858u));p32(box+0x454u,depth);
        const std::uint32_t fl=a[2]&0xffu;
        p8(o+0x41u,0);p8(o+0x42u,0);p32(o+0x127cu,5);p32(o+0x1278u,1);
        if(fl&5u){p8(o+0x42u,1);p32(o+0x127cu,(fl&1u)?5u:9u);}
        if(fl&2u){p32(o+0x1278u,1);p8(o+0x41u,1);}
        p8(o+0x43u,a[8]);
        panel_fit_48cef0(o,0,0);
        return 1;
    }
    // 48D420(ecx = panel): its frame: moved, then the answer pressed (1: first, 2: second).
    std::uint32_t panel_update_48d420(std::uint32_t o){
        if(!u8(o+0x34u))return 0;
        panel_move_48cc00(o);
        std::uint32_t r=0xc;
        if(!u8(o+0x40u)){
            (void)call(0x4035f0u);
            const std::uint32_t owner=call(0x4035f0u);
            if(u32(owner+0x218u)==2u&&u8(o+0x42u)!=1u&&u8(o+0x41u)!=1u&&!u8(o+0x12a5u))r=vslot(o,5,{1u});
            else r=vslot(o,5,{0xffffffffu});
        }
        p8(o+0x40u,0);
        if(r==0u&&u8(o+0x42u)){p8(o+0x44u,1);call(0x4249f0u,{0x40u});return 1;}
        if(r==1u&&u8(o+0x41u)){p8(o+0x44u,0);call(0x4249f0u,{0u});return 2;}
        vslot(o+0x48u,2);vslot(o+0x4d4u,2);
        return 0;
    }
    // 48C5F0(ecx = panel): its display: the frame parts (42D300), the boxes and the
    // answer buttons (429530).
    void panel_draw_48c5f0(std::uint32_t o){
        if(!u8(o+0x34u))return;
        const std::uint32_t df=fb(float(i32(o+0x3cu)));
        const std::int32_t w=i16(o+0x36u),h=i16(o+0x38u);
        const std::int32_t x=ftrunc(f32(o+0x1280u)),y=ftrunc(f32(o+0x1284u));
        auto part=[&](std::uint32_t kind,std::uint32_t id,std::int32_t px,std::int32_t py,std::uint32_t sw,std::uint32_t sh){
            call(0x42d300u,{kind,id,std::uint32_t(px),std::uint32_t(py),sw,sh,0u,df,0xffffffffu});};
        const std::int32_t top=y+0x1a;
        part(3,0x3004d,x+3,top,fb(float(w-9)),fb(float(h-0x2a)));
        part(1,0x3004c,x+3,y+0x20,fb(float(w-9)),0x40c00000u);
        if(u8(o+0x42u)==1u||u8(o+0x41u)==1u)part(1,0x30040,x+3,h+y-0x2e,fb(float(w-9)),0x41f00000u);
        if(u8(o+0x43u)==1u){
            part(1,0x30040,x+3,top,fb(float(w-9)),0x41f00000u);
            part(1,0x30040,x+3,h+y-0x2e,fb(float(w-9)),0x41f00000u);
        }
        part(1,0x30043,x+9,y,fb(float(w-0x73)),0x41d00000u);
        part(1,0x30046,x+9,h+y-0x10,fb(float(w-0x14)),0x41800000u);
        part(0,0x30042,x,y,0x41100000u,0x41d00000u);
        part(0,0x30041,w+x-0x6a,y,0x42d40000u,0x41d00000u);
        part(0,0x30045,x,h+y-0x10,0x41100000u,0x41800000u);
        part(0,0x30044,w+x-0xb,h+y-0x10,0x41300000u,0x41800000u);
        part(2,0x3004f,x,top,0x40400000u,fb(float(h-0x2a)));
        part(2,0x3004e,w+x-6,top,0x40c00000u,fb(float(h-0x2a)));
        vslot(o+0x48u,3);vslot(o+0x4d4u,3);
        const float by=float(h)+f32(o+0x1284u)-f32(0x628100u)-f32(0x6281ccu);
        if(u8(o+0x41u)){
            vslot(o+0x960u,3);
            call(0x429530u,{0x2c013au,fb(float(w)+f32(o+0x1280u)-f32(0x5c1850u)-f32(0x6281c8u)),fb(by),u32(o+0x3cu)+1u,0x19u});
        }
        if(u8(o+0x42u)){
            vslot(o+0xdecu,3);
            call(0x429530u,{u32(0x5c17a8u+u32(o+0x127cu)*12u),fb(f32(o+0x1280u)+f32(0x5c1850u)-f32(0x6281c8u)),fb(by),u32(o+0x3cu)+1u,0x19u});
        }
    }
    // 48E490(ecx = list): its items freed (no sprite reset).
    void list_free_48e490(std::uint32_t o){
        std::uint32_t it=u32(o+8u);
        while(it){const std::uint32_t next=u32(it+0x5dcu);item_free_48da20(it);call(0x5801a7u,{it});it=next;}
        p32(o+8u,0);
    }
    // ==== the lobby screens' base (vtable 5CED04): +38 the backdrop sprite, +D8 seven
    // setting labels (0x8C each), +4AC.. the shown settings, +4C8 the course picture, +574
    // a list, +9EC a sprite, +A8C done / +A90 its result ====================================
    // ---- labels (0x8C): +0 depth, +4 / +8 position, +C scroll, +10 text width, +14 box
    // width, +18 length, +24 the text, +88 colour ------------------------------------------
    void label_4edc80(std::uint32_t l){
        for(std::uint32_t k=0;k<0x24u;k+=4u)p32(l+k,0);
        p32(l+0x14u,0x280);
        for(std::uint32_t k=0;k<0x64u;k+=4u)p32(l+0x24u+k,0);
        p32(l+0x88u,0xffffffffu);
    }
    // 4EDCF0 / 4EDD50(ecx = label, text): its text (measured); 4EDCF0 also restarts it.
    void label_text_4edd50(std::uint32_t l,std::uint32_t text){
        call(0x42c360u,{0x100u});
        p32(l+0x10u,call(0x42c370u,{text}));
        p32(l+0x18u,slen(text));
        call(0x581780u,{l+0x24u,text,0x64u});
        p8(l+0x87u,0);
    }
    void label_text_4edcf0(std::uint32_t l,std::uint32_t text){
        call(0x42c360u,{0x100u});
        p32(l+0x10u,call(0x42c370u,{text}));
        p32(l+0x18u,slen(text));
        p32(l+0x20u,0);p32(l+0xcu,0);
        call(0x581780u,{l+0x24u,text,0x64u});
        p8(l+0x87u,0);
    }
    // 4EDAF0(ecx = label, text, x, y, width, colour).
    void label_init_4edaf0(std::uint32_t l,std::uint32_t text,std::uint32_t x,std::uint32_t y,std::uint32_t w,std::uint32_t colour){
        p32(l,0xe);p32(l+4u,x);p32(l+8u,y);p32(l+0xcu,0);p32(l+0x20u,0);p32(l+0x1cu,0);p32(l+0x10u,0);p32(l+0x14u,0x280);p32(l+0x18u,0);
        call(0x42ca60u,{9u});
        call(0x42c360u,{0x100u});
        p32(l+0x10u,call(0x42c370u,{text}));
        p32(l+0x14u,w);
        p32(l+0x18u,slen(text));
        call(0x581780u,{l+0x24u,text,0x64u});
        p8(l+0x87u,0);p32(l+0x88u,colour);
    }
    // 4EDB90(ecx = label): drawn; a text wider than its box scrolls (twice, 0x20 apart).
    void label_draw_4edb90(std::uint32_t l){
        call(0x42ccb0u,{u32(l)});
        call(0x42ca60u,{9u});
        call(0x42cca0u,{u32(l+0x88u)});
        call(0x42c360u,{0x100u});
        const std::int32_t tw=i32(l+0x10u),bw=i32(l+0x14u);
        if(tw<=bw){
            call(0x42c360u,{1u});
            call(0x42cc00u,{u32(l+4u),u32(l+8u)});
            call(0x42cdd0u,{0x626468u,l+0x24u});
            return;
        }
        call(0x42c360u,{0x100u});
        const std::int32_t gap=bw<0x20?bw:0x20;
        std::int32_t off=i32(l+0xcu)-1;
        p32(l+0xcu,std::uint32_t(off));
        if(off<=-tw){off+=tw+gap;p32(l+0xcu,std::uint32_t(off));}
        call(0x42c360u,{1u});
        const std::int32_t x=i32(l+4u);
        call(0x42c820u,{std::uint32_t(x),std::uint32_t(bw+x)});
        const std::int32_t x0=x+i32(l+0xcu);
        call(0x42cc00u,{std::uint32_t(x0),u32(l+8u)});
        call(0x42cdd0u,{0x626468u,l+0x24u});
        call(0x42cc00u,{std::uint32_t(i32(l+0x10u)+x0+gap),u32(l+8u)});
        call(0x42cdd0u,{0x626468u,l+0x24u});
        call(0x42c840u);
    }
    // ---- the highlight bar (51F560..): +0.. a mover (445E60), +28 width, +2C depth,
    // +30 shown, +31 lit ---------------------------------------------------------------
    // 445E60(ecx = mover, x, y, frames): the target; reached at once when frames is 0.
    void mover_445e60(std::uint32_t o,float x,float y,float t){
        p8(o+0x24u,1);pf(o+0x20u,0.0f);pf(o+0x14u,0.0f);pf(o+0x10u,y);pf(o+0xcu,x);
        if(t==0.0f){pf(o,x);pf(o+4u,y);pf(o+8u,0.0f);pf(o+0x18u,0.0f);pf(o+0x1cu,0.0f);return;}
        const float inv=f32(0x62806cu)/t;
        pf(o+0x18u,(x-f32(o))*inv);pf(o+0x1cu,(y-f32(o+4u))*inv);
    }
    void bar_51f580(std::uint32_t o,std::uint32_t x,std::uint32_t y,std::uint32_t w,std::uint32_t depth){
        float fx,fy;std::memcpy(&fx,&x,4);std::memcpy(&fy,&y,4);
        mover_445e60(o,fx,fy,0.0f);
        p32(o+0x28u,w);p32(o+0x2cu,depth);p8(o+0x31u,0);p8(o+0x30u,1);
    }
    void bar_draw_51f5b0(std::uint32_t o){
        if(!u8(o+0x30u))return;
        const std::uint32_t colour=u8(o+0x31u)?0xffffce0au:0xff8b9091u;
        const std::uint32_t z=fb(float(i32(o+0x2cu)));
        const std::uint32_t y=std::uint32_t(ftrunc(f32(o+4u)));
        call(0x42d300u,{0u,0x30048u,std::uint32_t(ftrunc(f32(o))),y,0x41600000u,0x41700000u,0u,z,colour});
        call(0x42d300u,{1u,0x30049u,std::uint32_t(ftrunc(f32(o)+f32(0x5c1854u))),y,fb(float(i32(o+0x28u)-0x1c)),0x41700000u,0u,z,colour});
        call(0x42d300u,{0u,0x30047u,std::uint32_t(ftrunc(float(i32(o+0x28u)-0x1c)+f32(o)+f32(0x5c1854u))),y,0x41600000u,0x41700000u,0u,z,colour});
    }
    // ---- the menu list (4ED950): +0 selected, +4 count, +8 first item (0x178: +0 index,
    // +4 indent, +8 shown, +C enabled, +10 sprite, +14 sprite object, +B4 bar, +E8 label,
    // +174 next), +C table, +10 text mode, +11 vertical, +12 hidden, +13 unlit, +15 arrows,
    // +18 width, +1C step, +20 / +24 origin, +28 depth, +2C, +30 scrolls, +34 / +38 window -
    void menu_4ed950(std::uint32_t o){
        p32(o+4u,0);p32(o,0xffffffffu);p8(o+0x10u,1);p32(o+0xcu,0);
        p8(o+0x14u,0);p8(o+0x15u,0);p8(o+0x11u,0);p8(o+0x12u,0);p8(o+0x13u,0);
        pf(o+0x18u,0.0f);pf(o+0x1cu,f32(0x5b018cu));pf(o+0x20u,0.0f);pf(o+0x24u,0.0f);
        p32(o+0x28u,0xc);p32(o+0x2cu,0);p8(o+0x30u,0);p32(o+0x34u,0);p32(o+0x38u,0);p32(o+8u,0);
    }
    // 4ECFB0(ecx = menu, table, width, vertical, scrolls, window).
    void menu_init_4ecfb0(std::uint32_t o,std::uint32_t tbl,std::uint32_t width,std::uint32_t vertical,std::uint32_t scrolls,std::uint32_t window){
        p32(o+0xcu,tbl);p8(o+0x11u,vertical);p32(o+0x18u,width);p8(o+0x30u,scrolls);
        pf(o+0x1cu,f32(0x5b018cu));
        p32(o+4u,0);p32(o,0xffffffffu);p8(o+0x10u,1);p8(o+0x14u,0);p8(o+0x15u,0);p8(o+0x12u,0);p8(o+0x13u,0);
        pf(o+0x20u,0.0f);pf(o+0x24u,0.0f);p32(o+0x28u,0xc);p32(o+0x2cu,0);p32(o+0x34u,0);p32(o+0x38u,window);p32(o+8u,0);
    }
    void menu_item_4ed100(std::uint32_t it){method(0x465160u,it+0x14u);p8(it+0xb4u+0x30u,0);p8(it+0xb4u+0x31u,0);}
    void menu_item_free_4ed020(std::uint32_t it){p8(it+0xb4u+0x30u,0);p8(it+0xb4u+0x31u,0);method(0x465250u,it+0x14u);}
    void menu_append_4ed080(std::uint32_t o,std::uint32_t it){
        p32(it+0x174u,0);
        if(!u32(o+8u)){p32(o+8u,it);return;}
        std::uint32_t p=u32(o+8u);
        while(u32(p+0x174u))p=u32(p+0x174u);
        p32(p+0x174u,it);
    }
    // 4ED160(ecx = menu, text, indent): an item (labelled when text is set); its index.
    std::uint32_t menu_add_4ed160(std::uint32_t o,std::uint32_t text,std::uint32_t indent){
        push_440d50(1);
        const std::uint32_t it=call(0x5802cfu,{0x178u});
        if(it)menu_item_4ed100(it);
        p32(it,u32(o+4u));p32(it+4u,indent);p32(it+0xcu,1);p32(it+8u,1);p32(it+0x10u,0xffffffffu);
        if(text){
            const float y=float(i32(o+4u))*f32(o+0x1cu)+f32(o+0x20u);
            label_init_4edaf0(it+0xe8u,text,std::uint32_t(ftrunc(f32(o+0x20u))),std::uint32_t(ftrunc(y)),std::uint32_t(ftrunc(f32(o+0x18u))),0xff3f474au);
        }
        menu_append_4ed080(o,it);
        p32(o+4u,u32(o+4u)+1u);
        if(u32(o+4u)==1u)p32(o,0);
        (void)pop_440d70();
        return u32(o+4u)-1u;
    }
    void menu_clear_4eda60(std::uint32_t o){
        std::uint32_t it=u32(o+8u);
        while(it){
            method(0x465250u,it+0x14u);
            const std::uint32_t next=u32(it+0x174u);p32(it+0x174u,0);
            menu_item_free_4ed020(it);call(0x5801a7u,{it});
            it=next;
        }
        p32(o+8u,0);p32(o+4u,0);
    }
    void menu_free_4edab0(std::uint32_t o){
        std::uint32_t it=u32(o+8u);
        while(it){const std::uint32_t next=u32(it+0x174u);menu_item_free_4ed020(it);call(0x5801a7u,{it});it=next;}
        p32(o+8u,0);
    }
    std::uint32_t menu_item_at(std::uint32_t o,std::uint32_t idx){
        std::uint32_t it=u32(o+8u),k=0;
        while(it&&k!=idx){it=u32(it+0x174u);++k;}
        return it;
    }
    // 4ED250 / 4ED2A0(ecx = menu): the previous / next enabled item (the cursor sound
    // with more than one item).
    void menu_prev_4ed250(std::uint32_t o){
        do{p32(o,u32(o)-1u);if(i32(o)<0)p32(o,u32(o+4u)-1u);}while(!u32(menu_item_at(o,u32(o))+0xcu));
        if(i32(o+4u)>1)call(0x4249f0u,{1u});
    }
    void menu_next_4ed2a0(std::uint32_t o){
        const std::int32_t n=i32(o+4u);
        do{p32(o,u32(o)+1u);if(i32(o)>=n)p32(o,0);}while(!u32(menu_item_at(o,u32(o))+0xcu));
        if(n>1)call(0x4249f0u,{1u});
    }
    // 48DC10(ecx = list, sound): the previous enabled list item.
    void list_prev_48dc10(std::uint32_t o,std::uint32_t sound){
        for(;;){
            p32(o,u32(o)-1u);
            if(i32(o)<0)p32(o,u32(o+4u)-1u);
            std::uint32_t it=u32(o+8u),k=0;
            while(it&&k!=u32(o)){it=u32(it+0x5dcu);++k;}
            if(u32(it+8u))break;
        }
        if(sound)call(0x4249f0u,{1u});
    }
    // 4ED3E0(ecx = menu): its display (as 48DDA0, with labels and highlight bars).
    void menu_draw_4ed3e0(std::uint32_t o){
        float x=f32(o+0x20u),y=f32(o+0x24u);
        if(u8(o+0x30u)){
            const std::int32_t first=i32(o+0x34u),sel=i32(o);
            if(first+i32(o+0x38u)<=sel){if(sel<i32(o+4u))p32(o+0x34u,std::uint32_t(first+1));}
            else if(sel<first)p32(o+0x34u,std::uint32_t(first-1));
            else if(first<0)p32(o+0x34u,0);
        }else{p32(o+0x38u,0x60);p32(o+0x34u,0);}
        const float ax=x-f32(0x628244u);
        std::int32_t vis=0,skip=0;
        std::int32_t t=ftrunc(f32(0x842110u))*4;
        t%=256;t-=0x80;if(t<0)t=-t;
        const std::uint32_t colour=(std::uint32_t(t+0x7f)<<24)|0xffffffu;
        const std::uint32_t z=fb(float(i32(o+0x28u))+f32(0x62806cu));
        if(u8(o+0x15u)&&u32(o+0x34u)&&!u8(o+0x12u))
            call(0x42d280u,{0x3004bu,std::uint32_t(ftrunc(ax)),std::uint32_t(ftrunc(y+f32(0x6280b0u))),0u,z,colour});
        const std::uint32_t tbl=u32(o+0xcu);
        auto entry=[&](std::uint32_t e,std::uint32_t k){return u32(tbl+e*12u+k*4u);};
        std::int32_t idx=0;
        for(std::uint32_t it=u32(o+8u);it;++idx){
            const std::uint32_t next=u32(it+0x174u);
            const std::uint32_t spr=it+0x14u;
            method(0x465250u,spr);
            const std::int32_t first=i32(o+0x34u);
            if(idx<first||idx>=first+i32(o+0x38u)){
                if(!u32(it+8u)||u8(o+0x12u))++skip;
                it=next;continue;
            }
            if(!u32(it+8u)||u8(o+0x12u)){++skip;it=next;continue;}
            ++vis;
            const std::uint32_t depth=u32(o+0x28u);
            if(u8(o+0x10u)){
                if(u32(it+0x10u)==0xffffffffu){
                    const std::uint32_t l=it+0xe8u;
                    p32(l+4u,std::uint32_t(ftrunc(float(i32(it+4u))+x+f32(0x5b4434u))));p32(l+8u,std::uint32_t(ftrunc(y)));
                    p32(l,depth+1u);
                    p32(l+0x14u,std::uint32_t(ftrunc(f32(o+0x18u)-f32(0x5c1854u))));
                    label_draw_4edb90(l);
                }else{
                    const std::uint32_t e=u32(it+0x10u);
                    const float sx=f32(o+0x18u)*f32(0x628064u)+x-f32(0x6281c8u)-float(i32(o+0x2cu))+float(i32(it+4u));
                    const float sy=y+f32(0x5b4434u)-f32(0x6281ccu);
                    call(0x465860u,{entry(e,0),entry(e,1),entry(e,2),depth+1u,1u,fb(sx),fb(sy),0x3f800000u,0x3f800000u,0x3f800000u,0u},spr);
                    method(0x465970u,spr);
                }
                const std::uint32_t bar=it+0xb4u;
                const std::int32_t ind=i32(it+4u);
                bar_51f580(bar,fb(float(ftrunc(x)+ind)),fb(float(ftrunc(y))),std::uint32_t(ftrunc(f32(o+0x18u)-float(ind))),depth);
                p8(bar+0x31u,(std::uint32_t(idx)==u32(o)&&!u8(o+0x13u))?1u:0u);
                bar_draw_51f5b0(bar);
            }else{
                const std::uint32_t e=u32(it+0x10u);
                const std::uint32_t frame=(std::uint32_t(idx)==u32(o))?2u:1u;
                call(0x465860u,{entry(e,0),frame,frame,depth+1u,1u,fb(float(i32(it+4u))+x),fb(y),0x3f800000u,0x3f800000u,0x3f800000u,0u},spr);
                method(0x465970u,spr);
            }
            if(u8(o+0x11u))y+=f32(o+0x1cu);else x+=f32(o+0x1cu);
            it=next;
        }
        if(u8(o+0x15u)&&idx-skip>i32(o+0x34u)+vis&&!u8(o+0x12u))
            call(0x42d280u,{0x3004bu,std::uint32_t(ftrunc(ax)),std::uint32_t(ftrunc(y)),2u,z,colour});
    }
    // ---- course lists for the lobby settings ----------------------------------------
    // 4EE390(course): the course is allowed by the lobby rules (65A7AB group, 65A7B4 bits).
    std::uint32_t course_allowed_4ee390(std::uint32_t c){
        const std::int32_t m=i8(0x65a7abu);
        if(m){
            const std::uint32_t g=u32(0x5e3104u+c*4u);
            switch(m){
            case 1:if(g!=0xeu)return 0;break;
            case 2:if(g!=0xfu&&g!=0x10u)return 0;break;
            case 3:if(g!=0x11u&&g!=0x12u)return 0;break;
            case 4:if(std::int32_t(c)<0xf)return 0;break;
            default:break;
            }
        }
        if(c>0x1du)return 0;
        static constexpr std::int8_t bit[30]={4,-1,8,24,-1,28,10,6,26,22,12,14,16,18,20,5,1,9,25,3,29,11,7,27,23,13,15,17,19,21};
        if(bit[c]<0)return 1;
        return (u32(0x65a7b4u)>>bit[c])&1u;
    }
    // 4EE720: the allowed courses into 84B800 ([84B878] the count; 0..9 for type 1).
    void courses_4ee720(){
        p32(0x84b878u,0);
        if(u8(0x65a7acu)==1u){for(std::uint32_t k=0;k<10u;++k)p32(0x84b800u+k*4u,k);p32(0x84b878u,10);return;}
        for(std::uint32_t c=0;c<0x1eu;++c)
            if(course_allowed_4ee390(c)&0xffu){p32(0x84b800u+u32(0x84b878u)*4u,c);p32(0x84b878u,u32(0x84b878u)+1u);}
    }
    // 4EE7D0(course): its available cars 1..8 into 84B880 ([84B8F8] the count).
    void cars_4ee7d0(std::uint32_t course){
        p32(0x84b8f8u,0);
        const bool type1=u8(0x65a7acu)==1u;
        for(std::uint32_t s=1;s<9u;++s){
            const bool ok=type1?call(0x4bfa70u,{course,s})!=0u:(call(0x447730u,{course,s},0x7c23e0u)&0xffu)!=0u;
            if(ok){p32(0x84b880u+u32(0x84b8f8u)*4u,s);p32(0x84b8f8u,u32(0x84b8f8u)+1u);}
        }
    }
    // 4EE930: the setup's course / car / transmission kept valid for the lobby rules.
    std::uint32_t setup_check_4ee930(){
        courses_4ee720();
        const std::int32_t cur=std::int8_t(u8(0x655b59u));
        const std::int32_t n=i32(0x84b878u);
        std::int32_t i=0;
        while(i<n&&i32(0x84b800u+std::uint32_t(i)*4u)!=cur)++i;
        std::uint32_t at=0;
        if(i<n){at=std::uint32_t(i);p8(0x655b59u,std::uint32_t(std::int32_t(std::int8_t(u8(0x84b800u+at*4u)))));}
        else{p8(0x655b59u,u8(0x84b800u));p8(0x655b5au,1);}
        bool auto_=false;
        if(u32(0x84b390u)==1u)auto_=u32(0x84b394u)==1u;
        else auto_=std::int8_t(u8(0x655b59u))>=0xf;
        p8(0x83036du,auto_?1u:0u);                                              // 48B190
        cars_4ee7d0(u32(0x84b800u+at*4u));
        const std::int32_t car=std::int8_t(u8(0x655b5au));
        const std::int32_t m=i32(0x84b8f8u);
        std::int32_t j=0;
        while(j<m&&i32(0x84b880u+std::uint32_t(j)*4u)!=car)++j;
        if(j<m)p8(0x655b5au,std::uint32_t(std::int32_t(std::int8_t(u8(0x84b880u+std::uint32_t(j)*4u)))));
        else if(u8(0x65a7acu)==1u)p8(0x655b5au,call(0x46bd20u,{at}));
        else p8(0x655b5au,u8(0x84b880u));
        const std::int32_t gear=std::int8_t(u8(0x830374u));
        if(gear!=1&&gear!=0)p8(0x830374u,0);
        return 0;
    }
    // ---- the base screen -------------------------------------------------------------
    void screen_4ee5d0(std::uint32_t o){
        method(0x48f480u,o);
        p32(o,0x5ced04u);
        method(0x465160u,o+0x38u);
        method(0x465160u,o+0x4c8u);
        list_48e310(o+0x574u);
        method(0x465160u,o+0x9ecu);
        p8(o+0x570u,0);p8(o+0xa8cu,0);p32(o+0x56cu,0);
        std::int32_t v=i32(0x7c24b0u);
        if(v>10||v<0){v=0;p32(0x7c24b0u,0);}
        call(0x42fc90u,{fb(float(v)*f32(0x6280ecu))});
    }
    void screen_close_4ee6c0(std::uint32_t o){
        method(0x465250u,o+0x38u);method(0x465250u,o+0x4c8u);
        p32(o+0x56cu,0);p8(o+0x570u,0);
        list_clear_48e440(o+0x574u);
        method(0x465250u,o+0x9ecu);method(0x465250u,o+0x4c8u);
        p8(o+0xa8cu,0);p32(o+0xa90u,0);
    }
    void screen_free_4ee850(std::uint32_t o){
        p32(o,0x5ced04u);
        std::int32_t v=i32(0x7c24b0u);
        if(v>10||v<0){v=0;p32(0x7c24b0u,0);}
        call(0x42fc90u,{fb(float(v)*f32(0x62813cu))});
        screen_close_4ee6c0(o);
        method(0x465250u,o+0x9ecu);
        list_free_48e490(o+0x574u);
        method(0x465250u,o+0x4c8u);
        method(0x465250u,o+0x38u);
        p32(o,0x5c18c8u);
    }
    // 4EE2E0(ecx = screen): the seven labels laid out (empty), the settings unknown (-2).
    void labels_4ee2e0(std::uint32_t o){
        for(std::uint32_t k=0;k<7u;++k){
            const std::uint32_t l=o+0xd8u+k*0x8cu;
            const float y=float(std::int32_t(k))*f32(0x5c1c44u)+f32(0x6281ccu);
            label_init_4edaf0(l,0x6255d5u,0x1d3,std::uint32_t(ftrunc(y)),u32(0x6a3328u),0xffffffffu);
            p32(l,1);
            p32(o+0x4acu+k*4u,0xfffffffeu);
        }
        method(0x465250u,o+0x4c8u);
        p32(o+0x568u,0);p8(o+0x34u,1);
    }
    // 4EDE20(ecx = screen, k): the text id of setting k.
    std::uint32_t setting_text_4ede20(std::uint32_t o,std::uint32_t k){
        const std::int32_t i=std::int8_t(std::uint8_t(k));
        const std::uint32_t v=u32(o+0x4acu+std::uint32_t(i)*4u);
        switch(i){
        case 0:return v+0x63u;
        case 1:switch(u32(o+0x4acu)){case 0:return v+0x69u;case 1:return v+0x6cu;case 2:return v+0xbeu;case 3:return v+0xc3u;default:return v;}
        case 2:return v+0xd3u;
        case 3:return v+0xd6u;
        case 4:return v+0xdcu;
        default:return v;
        }
    }
    // 4EDF00(ecx = screen): the settings from the lobby setup 65A7A8.. (and the player
    // count); +34 set when one changed.
    void settings_4edf00(std::uint32_t o){
        bool ch=false;
        auto take=[&](std::uint32_t off,std::uint32_t v){if(u32(o+off)!=v){p32(o+off,v);ch=true;}};
        take(0x4acu,std::uint32_t(std::int32_t(i8(0x65a7a8u))));
        take(0x4b0u,std::uint32_t(std::int32_t(i8(0x65a7a9u))));
        take(0x4b4u,std::uint32_t(std::int32_t(i8(0x65a7acu))));
        take(0x4b8u,std::uint32_t(std::int32_t(i8(0x65a7adu))));
        take(0x4bcu,std::uint32_t(std::int32_t(i8(0x65a7afu))));
        take(0x4c0u,std::uint32_t(std::int32_t(i8(0x65a7b1u))));
        if(u32(0x7d68acu))take(0x568u,u32(u32(0x7d68acu)+0x14u));
        p8(o+0x34u,ch?1u:0u);
    }
    // 4EDFC0(ecx = screen): the labels' texts refreshed when a setting changed (label 5:
    // "players / max", label 6 kept).
    void labels_refresh_4edfc0(std::uint32_t o,bool own_count){
        vslot(o,8);
        if(!u8(o+0x34u))return;
        Frame L(*this,0x110);
        for(std::uint32_t k=0;k<7u;++k){
            if(k==6u)continue;
            const std::uint32_t l=o+0xd8u+k*0x8cu;
            if(k==5u){
                if(own_count)call(0x5802ddu,{L(0),0x5c9828u,u32(o+0x4c0u),u32(o+0x4c4u)});
                else if(u32(0x7d68acu))call(0x5802ddu,{L(0),0x5c9828u,u32(u32(0x7d68acu)+0x14u),u32(o+0x4c0u)});
                else call(0x5802ddu,{L(0),0x5c2130u,u32(o+0x4c0u)});
                label_text_4edcf0(l,L(0));
                continue;
            }
            label_text_4edcf0(l,call(0x465eb0u,{setting_text_4ede20(o,k)}));
        }
        p8(o+0x34u,0);
    }
    // 4EE090(ecx = screen): the course picture (frame by the settings) and the labels.
    void screen_draw_4ee090(std::uint32_t o){
        std::uint32_t f=0;
        const std::uint32_t s=u32(o+0x4b0u);
        switch(u32(o+0x4acu)){
        case 0:f=s==0u?0x3fu:(s==1u?0x40u:0u);break;
        case 1:{static constexpr std::uint8_t t[20]={0xa,0xb,0xc,0xe,0xd,0x1d,0x19,0x1b,0x1c,0x1a,0x28,0x29,0x2a,0x2c,0x2b,0x3b,0x37,0x39,0x3a,0x38};
            f=s<20u?t[s]:0u;break;}
        case 2:{static constexpr std::uint8_t t[4]={0xd,0x1a,0x1e,0x2d};f=s<4u?t[s]:0u;break;}
        case 3:f=s==0u?0x3cu:(s==1u?0x3du:(s==2u?0x3eu:0u));break;
        default:break;
        }
        const std::uint32_t pic=o+0x4c8u;
        method(0x4659f0u,pic);
        if(u32(o+0x4d0u)==0xffffffffu||method(0x4652e0u,pic)){
            method(0x465250u,pic);
            call(0x465860u,{0x440068u,f,f,1u,3u,fb(float(i32(0x6a3320u))),fb(float(i32(0x6a3324u))),0x3f800000u,0x3f800000u,0x3f800000u,0u},pic);
            method(0x465970u,pic);
        }
        for(std::uint32_t k=0;k<7u;++k)label_draw_4edb90(o+0xd8u+k*0x8cu);
    }
    // ==== the LAN host list screen (vtable 5C97FC, 0x2200 bytes, factory 441690): +A94 a
    // message panel, +1D44 its answer menu, +1D80 the found hosts list, +21F8 its state ====
    void hosts_4c6300(std::uint32_t o){
        screen_4ee5d0(o);
        p32(o,0x5c97fcu);
        panel_48c490(o+0xa94u);
        menu_4ed950(o+0x1d44u);
        list_48e310(o+0x1d80u);
        p8(o+0xa8cu,0);p8(o+0x21fdu,0);p32(o+8u,0xb);
    }
    void hosts_free_4c69c0(std::uint32_t o){
        p32(o,0x5c97fcu);
        screen_close_4ee6c0(o);
        method(0x465250u,o+0x38u);
        p8(o+0xa8cu,0);
        list_clear_48e440(o+0x1d80u);
        menu_clear_4eda60(o+0x1d44u);
        vslot(o+0xa94u,4);
        p8(0x84aa00u,0);p8(o+0x21fdu,0);
        list_free_48e490(o+0x1d80u);
        menu_free_4edab0(o+0x1d44u);
        panel_free_48c520(o+0xa94u);
        screen_free_4ee850(o);
    }
    // 4C6860(ecx = screen): the hosts list filled from the found list (7DBDE8 names), or
    // the "none" text; the entry count.
    std::uint32_t hosts_fill_4c6860(std::uint32_t o){
        const std::uint32_t list=o+0x1d80u;
        list_clear_48e440(list);
        Frame L(*this,0x440);
        const std::uint32_t st=L(0);
        style_48d570(st);
        for(std::uint32_t k=0;k<0x400u;k+=4u)p32(st+k,0);
        pf(st+0x42cu,float(i32(0x68d584u)));
        p32(st+0x400u,9);p32(st+0x404u,2);p32(st+0x420u,0xffffffffu);
        pf(st+0x430u,float(i32(0x68d588u)));
        const std::uint32_t a[8]={0x5c97c0u,1u,3u,2u,1u,7u,st,1u};
        list_init_48d970(list,a);
        pf(list+0x24u,float(i32(0x68d58cu)));pf(list+0x28u,float(i32(0x68d590u)));
        pf(list+0x45cu,float(i32(0x68d584u)));pf(list+0x460u,float(i32(0x68d588u)));
        pf(list+0x20u,float(i32(0x68d594u)));
        p8(o+0x1d9bu,0);
        p32(list+0x2cu,2);
        p8(o+0x1d9du,1);
        std::int32_t n=i32(0x659930u);
        if(!n)n=1;
        for(std::int32_t k=0;k<n;++k)
            list_add_48db30(list,i32(0x659930u)?0x7dbde8u+std::uint32_t(k)*0x90u:call(0x465eb0u,{0x1efu}));
        return std::uint32_t(n);
    }
    // 4C6A90(ecx = screen): opened: the backdrop in, the list and labels, the help bar.
    std::uint32_t hosts_open_4c6a90(std::uint32_t o){
        const std::uint32_t bg=o+0x38u;
        p8(o+0xa8cu,0);p8(o+0x21fdu,0);p8(o+0x21fcu,0);p32(o+4u,0x53);p32(o+0x21f8u,0);
        method(0x465250u,bg);
        call(0x465860u,{0x440001u,0u,0x17u,0u,1u,0u,0u,0x3f800000u,0x3f800000u,0x3f800000u,0u},bg);
        method(0x465970u,bg);
        hosts_fill_4c6860(o);
        labels_4ee2e0(o);
        method(0x442f20u,call(0x4035f0u),{1u,0u});
        method(0x440ea0u,call(0x4035f0u),{4u,0x295u,0x8000u,0x297u,0x20u,0x2b0u,8u,0x296u});
        return 1;
    }
    // 4C6390(ecx = screen): its display by state (1: list, 3: with the message panel).
    void hosts_draw_4c6390(std::uint32_t o){
        const std::uint32_t st=u32(o+0x21f8u);
        if(st==1u){list_draw_48dda0(o+0x1d80u);screen_draw_4ee090(o);return;}
        if(st==3u){list_draw_48dda0(o+0x1d80u);screen_draw_4ee090(o);menu_draw_4ed3e0(o+0x1d44u);vslot(o+0xa94u,3);}
    }
    void hosts_close_4c63f0(std::uint32_t o){
        screen_close_4ee6c0(o);
        method(0x465250u,o+0x38u);
        p8(o+0xa8cu,0);
        list_clear_48e440(o+0x1d80u);
        menu_clear_4eda60(o+0x1d44u);
        vslot(o+0xa94u,4);
        p8(0x84aa00u,0);p8(o+0x21fdu,0);
    }
    // 4C6670(ecx = screen): leaving: everything cleared, the backdrop out (state 5).
    void hosts_leave_4c6670(std::uint32_t o){
        const std::uint32_t bg=o+0x38u;
        method(0x465250u,bg);
        list_clear_48e440(o+0x1d80u);
        menu_clear_4eda60(o+0x1d44u);
        vslot(o+0xa94u,4);
        method(0x465250u,o+0x4c8u);                                                      // 4EE380
        method(0x465250u,bg);
        call(0x465860u,{0x440001u,0x17u,0u,0u,1u,0u,0u,0x3f800000u,0x3f800000u,0xbf800000u,0u},bg);
        method(0x465970u,bg);
        p8(o+0xa8cu,1);p32(o+0x21f8u,5);
    }
    // 4C6700(ecx = screen): the shown settings from the selected host's entry.
    void hosts_settings_4c6700(std::uint32_t o){
        const std::uint32_t e=u32(o+0x1d80u)*0x90u;
        bool ch=false;
        auto take=[&](std::uint32_t off,std::uint32_t v){if(u32(o+off)!=v){p32(o+off,v);ch=true;}};
        take(0x4acu,u32(0x7dbe1cu+e));take(0x4b0u,u32(0x7dbe20u+e));take(0x4b4u,u32(0x7dbe2cu+e));
        take(0x4b8u,u32(0x7dbe30u+e));take(0x4bcu,u32(0x7dbe38u+e));take(0x4c0u,u32(0x7dbde4u+e));
        take(0x4c4u,u32(0x7dbe18u+e)+2u);
        p8(o+0x34u,ch?1u:0u);
    }
    // 4C6440(ecx = screen): the "join?" panel (state 3) with its two answers.
    void hosts_ask_4c6440(std::uint32_t o){
        const std::uint32_t panel=o+0xa94u,menu=o+0x1d44u;
        p32(o+0x21f8u,3);
        vslot(panel,1);
        const std::uint32_t body=call(0x465eb0u,{0x1f0u});
        const std::uint32_t title=call(0x465eb0u,{0x1efu});
        const std::uint32_t a[9]={title,body,0u,u32(0x68d598u),u32(0x68d59cu),u32(0x68d5a0u),u32(0x68d5a4u),4u,0u};
        panel_open_48d0c0(panel,a);
        menu_init_4ecfb0(menu,0x68d56cu,fb(float(i16(panel+0x36u)-0x25)),1u,0u,0u);
        p8(o+0x1d57u,0);
        const float y=float(double(f32(panel+0x1284u))+double(f32(0x5c1858u))+double(f32(0x5b4354u)));
        const float x=float(double(f32(panel+0x1280u))+double(f32(0x628074u)));
        pf(menu+0x20u,x);pf(menu+0x24u,y);
        p32(menu+0x1cu,0x41880000u);
        p32(menu+0x28u,6);
        for(std::uint32_t i=0;i<2u;++i)menu_add_4ed160(menu,call(0x465eb0u,{0x1f1u+i}),0);
    }
    // 4C6540(ecx = screen, button): the panel's answers: join (454100; 454220 first on the
    // owner's page 8) or back to the list (state 4).
    std::uint32_t hosts_answer_4c6540(std::uint32_t o,std::uint32_t b){
        vslot(o+0xa94u,2);
        switch(b){
        case 0:
            if(u32(o+0x1d44u)==0u){
                const std::uint32_t page=method(0x442ec0u,call(0x4035f0u));
                p32(0x7d68d8u,0x16);
                if(page==8u){p8(0x7d68d4u,1);call(0x454100u,{0u,1u});}
                else call(0x454100u,{1u});
                p32(o+0xa90u,2);
                vslot(o,10);
            }else if(u32(o+0x1d44u)==1u)p32(o+0x21f8u,4);
            return 0;
        case 1:p32(o+0x21f8u,4);return 0;
        case 2:menu_prev_4ed250(o+0x1d44u);return 0;
        case 4:menu_next_4ed2a0(o+0x1d44u);return 0;
        default:return 0;
        }
    }
    // 4C6610(ecx = screen): back from the panel (state 1).
    void hosts_back_4c6610(std::uint32_t o){p32(o+0x21f8u,1);menu_clear_4eda60(o+0x1d44u);vslot(o+0xa94u,4);}
    // 4C6B40(ecx = screen, button): the list: pick a host (the join panel when hosts were
    // found: [659934] the pick, 454180), back, up / down, search again (454140).
    std::uint32_t hosts_input_4c6b40(std::uint32_t o,std::uint32_t b){
        if(u8(o+0x21fdu)&&u32(0x7d68d8u)!=0x17u){p8(o+0x21fdu,0);hosts_fill_4c6860(o);}
        switch(b){
        case 0:
            if(i32(0x659930u)>0){
                p32(0x659934u,u32(o+0x1d80u));
                const std::uint32_t page=method(0x442ec0u,call(0x4035f0u));
                call(0x454180u,{page==8u?0u:1u,0u});
                p32(0x7d68d8u,0x16);p32(o+4u,0xd);
                return 0;
            }
            if(!u8(0x84aa00u)){p32(o+0x21f8u,2);return 0;}
            p32(o+0xa90u,2);vslot(o,10);return 0;
        case 1:p32(o+0xa90u,2);vslot(o,10);return 0;
        case 2:vslot(o,7);return 0;
        case 4:vslot(o,6);return 0;
        case 7:case 9:{
            p32(0x7d68d8u,0x17);p8(o+0x21fdu,1);
            const std::uint32_t flag=u8(0x84aa00u)?0u:1u;
            const std::uint32_t v=method(0x564c90u,call(0x4035f0u),{flag});
            call(0x454140u,{v,flag});
            return 0;}
        default:return 0;
        }
    }
    // 4C6CB0(ecx = screen): its frame by state (0 opening, 1 list, 2 ask, 3 panel, 4 back,
    // 5 leaving); the result once done.
    std::uint32_t hosts_update_4c6cb0(std::uint32_t o){
        const std::uint32_t bg=o+0x38u;
        method(0x4659f0u,bg);
        const std::uint32_t b=vslot(o,5,{1u});
        switch(u32(o+0x21f8u)){
        case 0:
            if(method(0x4652e0u,bg)){
                p32(o+0x21f8u,1);
                method(0x465250u,bg);
                call(0x465860u,{0x440001u,0x17u,0x17u,0u,0u,0u,0u,0x3f800000u,0x3f800000u,0x3f800000u,0u},bg);
                method(0x465970u,bg);
            }
            return 0;
        case 1:vslot(o,9);return hosts_input_4c6b40(o,b);
        case 2:hosts_ask_4c6440(o);return 0;
        case 3:vslot(o,9);return hosts_answer_4c6540(o,b);
        case 4:hosts_back_4c6610(o);return 0;
        case 5:if(method(0x4652e0u,bg))p32(o+0x21f8u,6);return 0;
        default:
            if(!u8(o+0xa8cu))return 0;
            p8(o+0xa8cu,0);
            return u32(o+0xa90u);
        }
    }
    // ==== the lobby room screen (vtable 5C9920, 0x31C8 bytes, factory 4416F0): one row per
    // player (+A94 / +E54 / +1214 its three sprites, 0xA0 apart; +15D4 a number, 0x3C;
    // +173C a name label), +574 the menu list, +1A84 the row cursor, +1A88 the lobby name,
    // +1F14 a message panel (+31C4 shown, +31C5 asked) =======================================
    // ---- the lobby setup 65A7A4 (packed) and its unpacked globals 84B370.. --------------
    void setup_unpack_490f50(std::uint32_t p){
        auto sx=[&](std::uint32_t a){return std::uint32_t(std::int32_t(i8(a)));};
        p32(0x84b370u,sx(p));p32(0x84b374u,sx(p+1u));p32(0x84b384u,sx(p+2u));p32(0x84b37cu,sx(p+3u));
        p32(0x84b388u,sx(p+4u));p32(0x84b38cu,sx(p+5u));p32(0x84b394u,sx(p+7u));p32(0x84b390u,sx(p+8u));
        p32(0x84b398u,sx(p+9u));p32(0x84b39cu,sx(p+0xbu));p32(0x84b380u,sx(p+0xeu));
        call(0x581780u,{0x84b3a0u,p+0x14u,0x10u});
    }
    static constexpr std::int8_t CourseBit[30]={4,-1,8,24,-1,28,10,6,26,22,12,14,16,18,20,5,1,9,25,3,29,11,7,27,23,13,15,17,19,21};
    // 447520(ecx = profile, course): the course is unlocked.
    std::uint32_t course_open_447520(std::uint32_t profile,std::uint32_t c){
        static constexpr std::int8_t bit[30]={0,-1,2,7,-1,9,3,1,8,6,4,5,0xa,0xb,0xc,0xf,0xd,0x11,0x16,0xe,0x18,0x12,0x10,0x17,0x15,0x13,0x14,0x19,0x1a,0x1b};
        if(c>0x1du)return 0;
        if(bit[c]<0)return 1;
        return bit_4474e0(profile,std::uint32_t(bit[c]));
    }
    // 491090(ecx = setup, course): the setup allows the course (+10 bits).
    std::uint32_t setup_course_491090(std::uint32_t p,std::uint32_t c){
        if(c>0x1du)return 0;
        if(CourseBit[c]<0)return 1;
        return (u32(p+0x10u)>>CourseBit[c])&1u;
    }
    // 491280(ecx = setup): the course group (+7) kept to one with an allowed course.
    void setup_groups_491280(std::uint32_t p){
        if(u8(p+8u))return;
        const std::int32_t m=i8(p+7u);
        if(!m)return;
        bool e=false,f=false,g=false,late=false;
        for(std::uint32_t c=0;c<0xfu;++c){
            if(!(setup_course_491090(p,c)&0xffu))continue;
            const std::uint32_t grp=u32(0x5e3104u+c*4u);
            if(grp==0xeu)e=true;
            if(grp==0xfu||grp==0x10u)f=true;
            if(grp==0x11u||grp==0x12u)g=true;
        }
        for(std::uint32_t c=0xf;c<0x1eu;++c)if(setup_course_491090(p,c)&0xffu)late=true;
        if(m==4&&!late)p8(p+7u,3);
        const std::uint32_t top=u32(p+4u)&0xff000000u;
        if(top==0x3000000u&&!g)p8(p+7u,2);
        if(top==0x2000000u&&!f)p8(p+7u,1);
        if(top==0x1000000u&&!e)p8(p+7u,0);
    }
    // 491370(ecx = setup): packed from the globals, the course bits from the profile.
    void setup_pack_491370(std::uint32_t p){
        p8(p,u8(0x84b370u));p8(p+1u,u8(0x84b374u));p8(p+2u,u8(0x84b384u));p8(p+3u,u8(0x84b37cu));
        p8(p+4u,u8(0x84b388u));p8(p+5u,u8(0x84b38cu));p8(p+7u,u8(0x84b394u));p8(p+8u,u8(0x84b390u));
        p8(p+9u,u8(0x84b398u));p8(p+0xbu,u8(0x84b39cu));p8(p+0xcu,u8(0x659a54u));p8(p+0xdu,u8(p+3u)+2u);
        p8(p+0xeu,u8(0x84b380u));
        call(0x581780u,{p+0x14u,0x84b3a0u,0x10u});
        for(std::uint32_t c=0;c<0x1eu;++c){
            if(CourseBit[c]<0)continue;
            const std::uint32_t m=1u<<CourseBit[c];
            const std::uint32_t v=(course_open_447520(0x7c23e0u,c)&0xffu)?m:0u;
            p32(p+0x10u,(u32(p+0x10u)&~m)|v);
        }
        setup_groups_491280(p);
    }
    // 490FD0(ecx = setup): a random race setup (seeded by [850B18]).
    void setup_random_490fd0(std::uint32_t p){
        c.event_function36.pc_crt_random_state=u32(0x850b18u);                       // 580F33 srand
        auto r=[&]{return std::int32_t(crt_rand());};
        std::int8_t k=std::int8_t(r()%4);
        if(k==3)k=0;
        p8(p+4u,std::uint32_t(k));
        switch(k){
        case 0:p8(p+5u,std::uint32_t(r()%2));break;
        case 1:p8(p+5u,std::uint32_t(r()%20));break;
        case 2:p8(p+5u,std::uint32_t(r()%4));break;
        default:break;
        }
        p8(p+8u,std::uint32_t(r()%2));
        p8(p+9u,r()%100>=50?1u:0u);
        p8(p+0xbu,r()%100>=50?1u:0u);
    }
    // 48FD90(ecx = name, edi = out, n): the name through the character map [659928]
    // ('.' dropped, one space kept); its length.
    std::uint32_t name_map_48fd90(std::uint32_t src,std::uint32_t out,std::uint32_t n){
        std::uint32_t k=0;bool space=false;
        if(u8(src)){
            const std::uint32_t map=u32(0x659928u);
            for(std::uint32_t s=src;;){
                if(std::int32_t(k)>=std::int32_t(n))break;
                const std::uint32_t ch=u8(s);
                if(ch>=0x20u){
                    const std::uint32_t m=u8(map+ch-0x20u);
                    if(m!=0x2eu&&!(m==0x20u&&space)){
                        if(m==0x20u)space=true;
                        p8(out+k,m);++k;
                    }
                }
                ++s;
                if(!u8(s))break;
            }
        }
        p8(out+k,0);
        return k;
    }
    // 48FDF0(name): the lower-cased name holds a word of the list [659890] (stored +1).
    std::uint32_t name_banned_48fdf0(std::uint32_t name){
        if(!u32(0x659890u))return 0;
        std::string lower;
        for(std::uint32_t a=name;u8(a);++a){char ch=char(u8(a));if(ch>='A'&&ch<='Z')ch=char(ch+0x20);lower.push_back(ch);}   // 58F5E4
        for(std::uint32_t e=0x659890u;u32(e);e+=4u){
            const std::uint32_t w=u32(e);
            if(!slen(w))continue;
            std::string word;
            for(std::uint32_t a=w;u8(a);++a)word.push_back(char(u8(a)-1u));
            if(lower.find(word)!=std::string::npos)return 1;
        }
        return 0;
    }
    // 48FEA0(name): the lobby name is allowed.
    std::uint32_t name_ok_48fea0(std::uint32_t name){
        Frame L(*this,0x20);
        name_map_48fd90(name,L(0),0x11);
        if(name_banned_48fdf0(L(0)))return 0;
        name_map_48fd90(name,L(0),0x11);
        return name_banned_48fdf0(L(0))?0u:1u;
    }
    // 492690(kind, text, a, b): the pending message 65D008.. (text 65D00C, 0x1000).
    std::uint32_t message_492690(std::uint32_t kind,std::uint32_t text,std::uint32_t a,std::uint32_t b){
        p32(0x65d008u,a);
        call(0x581780u,{0x65d00cu,text,0x1000u});
        p32(0x65e00cu,kind);p32(0x65e010u,b);
        return 1;
    }
    // ---- the number widget (51BF10.., 0x3C): +0 position, +28 sprite, +2C colour,
    // +30 depth, +34 kind ------------------------------------------------------------
    void number_51bf50(std::uint32_t o,std::uint32_t id,std::uint32_t depth,std::uint32_t kind,std::uint32_t x,std::uint32_t y,std::uint32_t colour){
        p32(o+0x30u,depth);p32(o,x);p32(o+0x28u,id);p32(o+0x34u,kind);p32(o+4u,y);p32(o+0x2cu,colour);
        pf(o+8u,0.0f);for(std::uint32_t k=0;k<12u;k+=4u)p32(o+0xcu+k,u32(o+k));p8(o+0x24u,0);
    }
    void number_draw_51bfd0(std::uint32_t o){
        call(0x42d280u,{u32(o+0x28u),std::uint32_t(ftrunc(f32(o))),std::uint32_t(ftrunc(f32(o+4u))),0u,fb(float(i32(o+0x30u))),u32(o+0x2cu)});
    }
    // 4465E0(x, y, depth): the ready marks of the racing players (their +85C / +860..).
    void ready_marks_4465e0(std::uint32_t x,std::uint32_t y,std::uint32_t depth){
        const std::uint32_t s=u32(0x7d68acu);
        if(!s)return;
        bool first=false;
        const std::uint32_t fx=fb(float(std::int32_t(x))),fy=fb(float(std::int32_t(y)));
        for(std::int32_t i=0;i<i32(u32(0x7d68acu)+0x14u);++i){
            const std::uint32_t p=u32(u32(0x7d68acu)+0x18u+std::uint32_t(i)*4u);
            if(!p||u32(p+8u)!=3u||!u8(p+0x85cu))continue;
            if(!first){first=true;call(0x429530u,{u8(p+0x864u)?0x4400c2u:0x4400c3u,fx,fy,depth,0u});}
            const float v=f32(p+0x860u)*(u8(p+0x864u)?f32(0x6281e8u):f32(0x59dcccu));
            call(0x429530u,{u32(0x59dcb4u+u32(p+0x868u)*4u),fx,fy,depth,std::uint32_t(ftrunc(v))});
        }
    }
    // ---- the room screen ---------------------------------------------------------------
    void room_4c6fd0(std::uint32_t o){
        screen_4ee5d0(o);
        p32(o,0x5c9920u);
        for(std::uint32_t k=0;k<6u;++k){method(0x465160u,o+0xa94u+k*0xa0u);}
        for(std::uint32_t k=0;k<6u;++k){method(0x465160u,o+0xe54u+k*0xa0u);}
        for(std::uint32_t k=0;k<6u;++k){method(0x465160u,o+0x1214u+k*0xa0u);}
        for(std::uint32_t k=0;k<6u;++k)vector_51c000(o+0x15d4u+k*0x3cu);
        text_48e590(o+0x1a88u);
        panel_48c490(o+0x1f14u);
        p8(o+0x570u,0);p8(o+0xa8cu,0);p32(o+0x1a84u,0);p32(o+8u,0xc);
    }
    void room_rows_reset_4c7360(std::uint32_t o){
        for(std::uint32_t k=0;k<6u;++k){
            const std::uint32_t s=o+0xe54u+k*0xa0u;
            method(0x465250u,s-0x3c0u);method(0x465250u,s);method(0x465250u,s+0x3c0u);
        }
    }
    // 4C7AB0(ecx = room): closed.
    void room_close_4c7ab0(std::uint32_t o){
        screen_close_4ee6c0(o);
        method(0x465250u,o+0x38u);
        list_clear_48e440(o+0x574u);
        method(0x465250u,o+0x9ecu);
        vslot(o+0x1a88u,4);
        p8(o+0xa8cu,0);p32(o+0xa90u,0);
        room_rows_reset_4c7360(o);
        for(std::uint32_t k=0;k<6u;++k)label_4edc80(o+0x173cu+k*0x8cu);
    }
    void room_free_4c7e30(std::uint32_t o){
        p32(o,0x5c9920u);
        room_close_4c7ab0(o);
        panel_free_48c520(o+0x1f14u);
        p32(o+0x1a88u,0x5c18a0u);p32(o+0x1a88u,0x5c1888u);p32(o+0x1a88u,0x5c18c8u);                // 48E630
        for(std::uint32_t k=6;k-->0;)vector_51c000(o+0x15d4u+k*0x3cu);                             // 51BF10
        for(std::uint32_t k=6;k-->0;)method(0x465250u,o+0x1214u+k*0xa0u);
        for(std::uint32_t k=6;k-->0;)method(0x465250u,o+0xe54u+k*0xa0u);
        for(std::uint32_t k=6;k-->0;)method(0x465250u,o+0xa94u+k*0xa0u);
        screen_free_4ee850(o);
    }
    // 4C70F0(ecx = room): filled: the setup checked (a banned lobby name cleared), the
    // backdrop, the menu (texts 2A, 2C..2E), the side sprite, the lobby name, the labels.
    void room_fill_4c70f0(std::uint32_t o){
        p32(0x659940u,0);
        setup_unpack_490f50(0x65a7a4u);
        if(slen(0x84b3a0u)&&!(name_ok_48fea0(0x84b3a0u)&0xffu)){p32(0x84b3a0u,0);p32(0x84b3a4u,0);p32(0x84b3a8u,0);p32(0x84b3acu,0);p8(0x84b3b0u,0);}
        setup_pack_491370(0x65a7a4u);
        const std::uint32_t bg=o+0x38u;
        method(0x465250u,bg);
        call(0x465860u,{0x440001u,0x52u,0x74u,1u,1u,0u,0u,0x3f800000u,0x3f800000u,0x3f800000u,0u},bg);
        method(0x465970u,bg);
        Frame L(*this,0x440);
        const std::uint32_t st=L(0);
        style_48d570(st);
        for(std::uint32_t k=0;k<0x400u;k+=4u)p32(st+k,0);
        p32(st+0x42cu,u32(0x5b4394u));
        p32(st+0x400u,9);p32(st+0x404u,0xd);p32(st+0x420u,0xffffffffu);
        p32(st+0x430u,u32(0x5c1c70u));
        const std::uint32_t list=o+0x574u;
        const std::uint32_t a[8]={0x68dc4cu,0u,2u,1u,0u,0x60u,st,1u};
        list_init_48d970(list,a);
        p8(o+0x58fu,0);
        p32(list+0x24u,0xc3430000u);p32(list+0x28u,0x43070000u);
        p32(list+0x45cu,0xc2480000u);p32(list+0x460u,0xc1000000u);
        p32(list+0x20u,0x43010000u);
        p32(list+0x2cu,2);
        p8(o+0x58fu,1);
        for(std::uint32_t t=0x2a;t<=0x2eu;++t)if(t!=0x2bu)list_add_48db30(list,call(0x465eb0u,{t}));
        const std::uint32_t side=o+0x9ecu;
        call(0x465860u,{0x440064u,0u,0x2du,1u,1u,0u,0u,0x3f800000u,0x3f800000u,0x3f800000u,0u},side);
        call(0x4653c0u,{0u,0x43070000u,0u,0u},side);
        method(0x465970u,side);
        const std::uint32_t box=o+0x1a88u;
        vslot(box,1);
        p32(st+0x420u,0xff3f474au);
        text_style_48f2d0(box,st);
        p32(box+0x34u,0x428c0000u);p32(box+0x38u,0x42c00000u);
        p32(box+0x454u,2);
        text_print_48ee80(box,0x65a7b8u,{});
        labels_4ee2e0(o);
        vslot(o,9);
        setup_check_4ee930();
        const std::uint32_t v=u8(0x830364u);
        if(v==0xffu||std::int8_t(v)>=0x43)p8(0x830364u,u8(0x68f370u));
    }
    // 4C76A0(ecx = room): opened.
    std::uint32_t room_open_4c76a0(std::uint32_t o){
        p8(o+0x570u,0);p8(o+0xa8cu,0);p8(o+0x31c4u,0);p32(o+0x1a84u,0);
        for(std::uint32_t k=0;k<6u;++k)label_init_4edaf0(o+0x173cu+k*0x8cu,0x6255d5u,0,0,0x98,0xffffffffu);
        p32(o+4u,0x53);
        room_fill_4c70f0(o);
        method(0x442f20u,call(0x4035f0u),{1u,0u});
        method(0x440ea0u,call(0x4035f0u),{4u,0x295u,0x8000u,0x297u,0xffffffffu,0xffffffffu,8u,0x296u});
        p32(call(0x4035f0u)+4u,1);                                                        // 440DF0
        const std::uint32_t fl=call(0x4035f0u)+4u+0x208u;                                 // 535EF0
        p32(fl,(u32(fl)&0xffffffe7u)|4u);
        p8(o+0x31c5u,0);
        return 1;
    }
    // 4C7B20(ecx = room, row, lit): a player's row: frame, rank / car sprites, the state
    // number and the name.
    void room_row_4c7b20(std::uint32_t o,std::uint32_t idx,std::uint32_t lit){room_row(o,idx,lit,0x68dc84u);}
    // (4C8880 is the same with the row offset 68E360)
    void room_row(std::uint32_t o,std::uint32_t idx,std::uint32_t lit,std::uint32_t yoff){
        const std::uint32_t row=o+idx*0xa0u;
        const std::uint32_t a=row+0xa94u,b=row+0xe54u,cs=row+0x1214u;
        method(0x465250u,a);method(0x465250u,b);method(0x465250u,cs);
        const float y0=float(std::int32_t(idx))*f32(0x628100u)-f32(0x6282ccu);
        const float yrow=y0-f32(yoff)+f32(0x6281ccu);
        const std::uint32_t s=u32(0x7d68acu);
        bool hl=(lit&0xffu)!=0u;
        if(s&&u32(u32(s+0x18u+idx*4u)+0xcu))hl=!hl;
        const std::uint32_t label=o+0x173cu+idx*0x8cu;
        const std::uint32_t fr=hl?1u:0u;
        p32(label+0x88u,hl?0xff3f474au:0xffffffffu);
        call(0x465860u,{0x440063u,fr,fr,3u,0u,0u,0u,0x3f800000u,0x3f800000u,0x3f800000u,0u},a);
        call(0x4653c0u,{0xc2f40000u,fb(y0),0u,0u},a);
        method(0x465970u,a);
        std::uint32_t colour=0xffffffffu;
        if(!s||u32(u32(s+0x18u+idx*4u)+0xcu)){
            std::int32_t t=ftrunc(f32(0x842110u))*4;
            t%=256;t-=0x80;if(t<0)t=-t;
            const std::uint32_t al=std::uint32_t(t+0x7f);
            colour=((((al-0x7fu)<<8)|al)<<16)|0x4040u;
        }
        call(0x465770u,{colour},a);
        if(s){
            const std::uint32_t p=u32(s+0x18u+idx*4u);
            const std::uint32_t c1=u32(p+0x98u)-1u,e94=u32(p+0x94u),e9c=u32(p+0x9cu),st=u32(p+8u);
            const std::uint32_t icon=st<=1u?0x10u:(st==2u?0xfu:(st<=4u?0xdu:3u));
            const bool late=std::int32_t(e94)>=0xf;
            const std::uint32_t pic=late?0x4400cdu:0x440065u;
            const std::uint32_t k=late?e94-0xfu:e94;
            const std::uint32_t fr2=c1+k*8u;
            call(0x465860u,{pic,fr2,fr2,4u,0u,0xc1b00000u,fb(y0),0x3f800000u,0x3f800000u,0x3f800000u,0u},b);
            call(0x465860u,{0x440062u,e9c,e9c,4u,0u,0xc2640000u,fb(y0),0x3f800000u,0x3f800000u,0x3f800000u,0u},cs);
            const std::uint32_t num=o+0x15d4u+idx*0x3cu;
            number_51bf50(num,u32(0x6a3290u+icon*4u),4,0,0x43640000u,fb(yrow),0xffffffffu);
            method(0x465970u,b);method(0x465970u,cs);
            number_draw_51bfd0(num);
        }
        label_text_4edd50(label,u32(u32(0x7d68acu)+0x18u+idx*4u)+0x20u);
        p32(label+4u,0x47);p32(label+8u,std::uint32_t(ftrunc(y0-f32(0x628248u)+f32(0x6281ccu))));
        p32(label,4);
        label_draw_4edb90(label);
    }
    // 4C7F30(ecx = room): the players' rows (at most 6).
    void room_rows_4c7f30(std::uint32_t o){
        if(!u32(0x7d68acu))return;
        room_rows_reset_4c7360(o);
        for(std::int32_t i=0;i<i32(u32(0x7d68acu)+0x14u)&&i<6;++i)
            room_row_4c7b20(o,std::uint32_t(i),(std::uint32_t(i)==u32(o+0x1a84u)&&!u8(o+0x570u))?1u:0u);
    }
    // 4C7FB0(ecx = room): its display.
    void room_draw_4c7fb0(std::uint32_t o){
        if(!method(0x4652e0u,o+0x38u)||u8(o+0xa8cu))return;
        const std::uint32_t s=u32(0x7d68acu);
        if(s&&u8(s+5u))screen_draw_4ee090(o);
        list_draw_48dda0(o+0x574u);
        vslot(o+0x1a88u,3);
        room_rows_4c7f30(o);
        ready_marks_4465e0(0,0,2);
        if(u8(o+0x31c4u))vslot(o+0x1f14u,3);
    }
    void room_next_4c73a0(std::uint32_t o){
        const std::uint32_t s=u32(0x7d68acu);
        if(!s||i32(s+0x14u)<=i32(o+0x1a84u))return;
        p32(o+0x1a84u,u32(o+0x1a84u)+1u);p32(0x659940u,u32(0x659940u)+1u);call(0x4249f0u,{1u});
    }
    void room_prev_4c73e0(std::uint32_t o){
        if(i32(o+0x1a84u)<=0)return;
        p32(o+0x1a84u,u32(o+0x1a84u)-1u);p32(0x659940u,u32(0x659940u)-1u);call(0x4249f0u,{1u});
    }
    // 4C7410(ecx = room): leaving (backdrop out).
    void room_leave_4c7410(std::uint32_t o){
        const std::uint32_t bg=o+0x38u;
        method(0x465250u,bg);
        list_clear_48e440(o+0x574u);
        method(0x465250u,o+0x9ecu);
        vslot(o+0x1a88u,4);
        method(0x465250u,o+0x4c8u);
        room_rows_reset_4c7360(o);
        for(std::uint32_t k=0;k<6u;++k)label_4edc80(o+0x173cu+k*0x8cu);
        method(0x465250u,bg);
        call(0x465860u,{0x440001u,0x74u,0x52u,1u,1u,0u,0u,0x3f800000u,0x3f800000u,0xbf800000u,0u},bg);
        method(0x465970u,bg);
        p8(o+0xa8cu,1);
    }
    // 4C74C0(ecx = room): the host may start (2+ players, none joining or busy): the new
    // race (4F50D0); else the reason text [68DC70] 1F4 busy / 1F5 joining / 1F6 alone.
    std::uint32_t room_can_start_4c74c0(std::uint32_t){
        const std::uint32_t s=u32(0x7d68acu);
        const std::int32_t n=i32(s+0x14u);
        if(n<2){p32(0x68dc70u,0x1f6);return 0;}
        bool busy=false,joining=false;
        for(std::int32_t i=0;i<n;++i){
            const std::uint32_t p=u32(s+0x18u+std::uint32_t(i)*4u);
            if(p==u32(s+0x58u))continue;
            const std::uint32_t st=u32(p+8u);
            if(st==2u)joining=true;
            else if(st>=3u&&st<=6u)busy=true;
        }
        if(busy){p32(0x68dc70u,0x1f4);return 0;}
        if(joining){p32(0x68dc70u,0x1f5);return 0;}
        new_race_4f50d0();
        return 1;
    }
    // 4C7570(ecx = room): the "cannot start" panel.
    void room_panel_4c7570(std::uint32_t o){
        const std::uint32_t panel=o+0x1f14u;
        p8(o+0x31c4u,1);
        vslot(panel,1);
        const std::uint32_t body=call(0x465eb0u,{0x346u});
        const std::uint32_t title=call(0x465eb0u,{0x345u});
        const std::uint32_t a[9]={title,body,1u,u32(0x68dc88u),u32(0x68dc8cu),u32(0x68dc90u),u32(0x68dc94u),6u,0u};
        panel_open_48d0c0(panel,a);
        text_print_48ee80(panel+0xdecu,call(0x465eb0u,{0x210u}),{});                     // 48CBC0
        method(0x446fc0u,call(0x4035f0u)+0x51cu);
        method(0x440e10u,call(0x4035f0u));
        method(0x440ea0u,call(0x4035f0u),{0xffffffffu,0xffffffffu,0xffffffffu,0xffffffffu,0xffffffffu,0xffffffffu,0xffffffffu,0xffffffffu});
        method(0x442f20u,call(0x4035f0u),{0u,0u});
    }
    void room_panel_close_4c7630(std::uint32_t o){
        if(!u8(o+0x31c4u))return;
        p8(o+0x31c4u,0);
        vslot(o+0x1f14u,4);
        method(0x4464f0u,call(0x4035f0u)+0x51cu);
        method(0x440e60u,call(0x4035f0u));
    }
    std::uint32_t room_panel_frame_4c7670(std::uint32_t o){
        if(u8(o+0x31c4u)&&vslot(o+0x1f14u,2))room_panel_close_4c7630(o);
        return 0;
    }
    // 4C7770(ecx = room): its frame: the menu (start, leave, random setup, settings...),
    // the rows cursor, the panel; the result once done.
    std::uint32_t room_update_4c7770(std::uint32_t o){
        method(0x442f20u,call(0x4035f0u),{1u,0u});
        method(0x440ea0u,call(0x4035f0u),{4u,0x295u,0x8000u,0x297u,0xffffffffu,0xffffffffu,8u,0x296u});
        method(0x4659f0u,o+0x38u);method(0x4659f0u,o+0x9ecu);
        if(!method(0x4652e0u,o+0x38u))return 0;
        if(u8(o+0xa8cu)){p8(o+0xa8cu,0);return u32(o+0xa90u);}
        text_print_48ee80(o+0x1a88u,0x65a7b8u,{});
        std::uint32_t s=u32(0x7d68acu);
        if(s){
            if(u8(s+5u)){vslot(o,9);setup_check_4ee930();s=u32(0x7d68acu);}
            if(s&&i32(o+0x1a84u)>=i32(s+0x14u)){p32(o+0x1a84u,u32(s+0x14u)-1u);p32(0x659940u,u32(u32(0x7d68acu)+0x14u)-1u);}
        }
        if(u8(o+0x31c4u))return room_panel_frame_4c7670(o);
        if(u8(o+0x31c5u)){
            p8(o+0x31c5u,0);
            s=u32(0x7d68acu);
            if(s&&i32(s+0x14u)<2){room_panel_4c7570(o);return 0;}
        }
        const std::uint32_t b=vslot(o,5,{1u});
        auto finish=[&]()->std::uint32_t{
            if(u32(o+0xa90u)==4u)return 4;
            if(i32(u32(0x7d68acu)+0x14u)<=1)return 0;
            vslot(o,10);
            return 0;
        };
        switch(b){
        case 0:{
            if(!u8(o+0x570u)){p32(0x659940u,u32(o+0x1a84u));p32(o+4u,0x22);p32(o+0xa90u,4);p8(o+0xa8cu,1);return 0;}
            s=u32(0x7d68acu);
            const bool host=s&&u8(s+7u);
            switch(u32(o+0x574u)){
            case 0:
                if(host){p32(o+4u,0x19);p32(o+0xa90u,4);}
                else{p32(o+4u,0x53);p32(o+0xa90u,2);}
                return finish();
            case 1:p32(o+4u,0x11);return 4;
            case 2:
                if(host){setup_random_490fd0(0x65a7a4u);p32(o+0xa90u,0);return 0;}
                p32(o+4u,0x53);p32(o+0xa90u,2);return finish();
            case 3:
                if(!s)return finish();
                if(!u8(s+7u)||i32(s+0x14u)<=1){room_panel_4c7570(o);return 0;}
                if(room_can_start_4c74c0(o))return finish();
                p32(o+4u,0x13);p8(o+0x31c5u,1);return 4;
            default:return finish();
            }}
        case 1:
            message_492690(0xe,call(0x465eb0u,{0x429u}),0xffffffffu,0xa);
            method(0x4459f0u,call(0x4035f0u),{0u});
            return 0;
        case 2:
            if(u8(o+0x570u)){p8(o+0x570u,0);p8(o+0x58fu,1);call(0x4249f0u,{1u});return 0;}
            vslot(o,7);return 0;
        case 3:if(u8(o+0x570u))list_prev_48dc10(o+0x574u,1);return 0;
        case 4:
            if(u8(o+0x570u))return 0;
            s=u32(0x7d68acu);
            if(!s)return 0;
            if(u32(o+0x1a84u)==u32(s+0x14u)-1u){p8(o+0x570u,1);p8(o+0x58fu,0);call(0x4249f0u,{1u});return 0;}
            vslot(o,6);return 0;
        case 5:if(u8(o+0x570u))list_next_48dc60(o+0x574u,1);return 0;
        default:return 0;
        }
    }
    // ==== the guest room screen (vtable 5C9A28, 0x1F10 bytes, factory 441750): the room's
    // layout without the panel; +56C the row cursor, +1A84 the lobby name =================
    void guest_room_4c8030(std::uint32_t o){
        screen_4ee5d0(o);
        p32(o,0x5c9a28u);
        for(std::uint32_t k=0;k<6u;++k){method(0x465160u,o+0xa94u+k*0xa0u);}
        for(std::uint32_t k=0;k<6u;++k){method(0x465160u,o+0xe54u+k*0xa0u);}
        for(std::uint32_t k=0;k<6u;++k){method(0x465160u,o+0x1214u+k*0xa0u);}
        for(std::uint32_t k=0;k<6u;++k)vector_51c000(o+0x15d4u+k*0x3cu);
        text_48e590(o+0x1a84u);
        p8(o+0x570u,0);p8(o+0xa8cu,0);p32(o+0x56cu,0);p32(o+8u,0xd);
    }
    void guest_room_close_4c87f0(std::uint32_t o){
        screen_close_4ee6c0(o);
        method(0x465250u,o+0x38u);
        list_clear_48e440(o+0x574u);
        method(0x465250u,o+0x9ecu);
        vslot(o+0x1a84u,4);
        p8(o+0xa8cu,0);p32(o+0xa90u,0);
        room_rows_reset_4c7360(o);
        for(std::uint32_t k=0;k<6u;++k)label_4edc80(o+0x173cu+k*0x8cu);
    }
    void guest_room_free_4c8b90(std::uint32_t o){
        p32(o,0x5c9a28u);
        guest_room_close_4c87f0(o);
        p32(o+0x1a84u,0x5c18a0u);p32(o+0x1a84u,0x5c1888u);p32(o+0x1a84u,0x5c18c8u);                // 48E630
        for(std::uint32_t k=6;k-->0;)vector_51c000(o+0x15d4u+k*0x3cu);
        for(std::uint32_t k=6;k-->0;)method(0x465250u,o+0x1214u+k*0xa0u);
        for(std::uint32_t k=6;k-->0;)method(0x465250u,o+0xe54u+k*0xa0u);
        for(std::uint32_t k=6;k-->0;)method(0x465250u,o+0xa94u+k*0xa0u);
        screen_free_4ee850(o);
    }
    // 4C8140(ecx = guest room): filled (menu texts 2B, 2C).
    void guest_room_fill_4c8140(std::uint32_t o){
        p32(0x659940u,0);
        const std::uint32_t bg=o+0x38u;
        method(0x465250u,bg);
        call(0x465860u,{0x440001u,0x52u,0x74u,1u,1u,0u,0u,0x3f800000u,0x3f800000u,0x3f800000u,0u},bg);
        method(0x465970u,bg);
        Frame L(*this,0x440);
        const std::uint32_t st=L(0);
        style_48d570(st);
        for(std::uint32_t k=0;k<0x400u;k+=4u)p32(st+k,0);
        p32(st+0x42cu,u32(0x5b4394u));
        p32(st+0x400u,9);p32(st+0x404u,0xd);p32(st+0x420u,0xffffffffu);
        p32(st+0x430u,u32(0x5c1c70u));
        const std::uint32_t list=o+0x574u;
        const std::uint32_t a[8]={0x68e33cu,0u,2u,1u,0u,0x60u,st,1u};
        list_init_48d970(list,a);
        p8(o+0x58fu,0);
        p32(list+0x24u,0xc3430000u);p32(list+0x28u,0x43070000u);
        p32(list+0x45cu,0xc2480000u);p32(list+0x460u,0xc1000000u);
        p32(list+0x20u,0x43010000u);
        p32(list+0x2cu,3);
        p8(o+0x58fu,1);
        for(std::uint32_t t=0x2a;t<=0x2eu;++t)if(t==0x2bu||t==0x2cu)list_add_48db30(list,call(0x465eb0u,{t}));
        const std::uint32_t side=o+0x9ecu;
        call(0x465860u,{0x440064u,0u,0x2du,1u,1u,0u,0u,0x3f800000u,0x3f800000u,0x3f800000u,0u},side);
        call(0x4653c0u,{0u,0x43070000u,0u,0u},side);
        method(0x465970u,side);
        const std::uint32_t box=o+0x1a84u;
        vslot(box,1);
        p32(st+0x420u,0xff3f474au);
        text_style_48f2d0(box,st);
        p32(box+0x34u,0x428c0000u);p32(box+0x38u,0x42c00000u);
        p32(box+0x454u,2);
        text_print_48ee80(box,0x65a7b8u,{});
        labels_4ee2e0(o);
        vslot(o,9);
        setup_check_4ee930();
        const std::uint32_t v=u8(0x830364u);
        if(v==0xffu||std::int8_t(v)>=0x43)p8(0x830364u,u8(0x68f370u));
    }
    std::uint32_t guest_room_open_4c8710(std::uint32_t o){
        p8(o+0x570u,0);p8(o+0xa8cu,0);p32(o+0x56cu,0);
        for(std::uint32_t k=0;k<6u;++k)label_init_4edaf0(o+0x173cu+k*0x8cu,0x6255d5u,0,0,0x98,0xffffffffu);
        p32(o+4u,0x53);
        guest_room_fill_4c8140(o);
        method(0x442f20u,call(0x4035f0u),{1u,0u});
        method(0x440ea0u,call(0x4035f0u),{4u,0x295u,0x8000u,0x297u,0xffffffffu,0xffffffffu,8u,0x296u});
        p32(call(0x4035f0u)+4u,1);                                                        // 440DF0
        const std::uint32_t fl=call(0x4035f0u)+4u+0x208u;                                 // 535EF0
        p32(fl,(u32(fl)&0xffffffe7u)|4u);
        service_reset_4940d0(0x65f0c0u);
        return 1;
    }
    void guest_room_rows_4c8c80(std::uint32_t o){
        if(!u32(0x7d68acu))return;
        room_rows_reset_4c7360(o);
        for(std::int32_t i=0;i<i32(u32(0x7d68acu)+0x14u)&&i<6;++i)
            room_row(o,std::uint32_t(i),(std::uint32_t(i)==u32(o+0x56cu)&&!u8(o+0x570u))?1u:0u,0x68e360u);
    }
    void guest_room_draw_4c8d00(std::uint32_t o){
        if(!method(0x4652e0u,o+0x38u)||u8(o+0xa8cu))return;
        const std::uint32_t s=u32(0x7d68acu);
        if(s&&u8(s+5u))screen_draw_4ee090(o);
        list_draw_48dda0(o+0x574u);
        vslot(o+0x1a84u,3);
        guest_room_rows_4c8c80(o);
        ready_marks_4465e0(0,0,2);
    }
    void guest_room_next_4c85f0(std::uint32_t o){
        const std::uint32_t s=u32(0x7d68acu);
        if(!s||i32(s+0x14u)<=i32(o+0x56cu))return;
        p32(o+0x56cu,u32(o+0x56cu)+1u);p32(0x659940u,u32(0x659940u)+1u);call(0x4249f0u,{1u});
    }
    void guest_room_prev_4c8630(std::uint32_t o){
        if(i32(o+0x56cu)<=0)return;
        p32(o+0x56cu,u32(o+0x56cu)-1u);p32(0x659940u,u32(0x659940u)-1u);call(0x4249f0u,{1u});
    }
    void guest_room_leave_4c8660(std::uint32_t o){
        const std::uint32_t bg=o+0x38u;
        method(0x465250u,bg);
        list_clear_48e440(o+0x574u);
        method(0x465250u,o+0x9ecu);
        vslot(o+0x1a84u,4);
        method(0x465250u,o+0x4c8u);
        room_rows_reset_4c7360(o);
        for(std::uint32_t k=0;k<6u;++k)label_4edc80(o+0x173cu+k*0x8cu);
        method(0x465250u,bg);
        call(0x465860u,{0x440001u,0x74u,0x52u,1u,1u,0u,0u,0x3f800000u,0x3f800000u,0xbf800000u,0u},bg);
        method(0x465970u,bg);
        p8(o+0xa8cu,1);
    }
    // 4C8360(ecx = guest room): its frame (ready / settings, leave the session, rows).
    std::uint32_t guest_room_update_4c8360(std::uint32_t o){
        method(0x442f20u,call(0x4035f0u),{1u,0u});
        method(0x440ea0u,call(0x4035f0u),{4u,0x295u,0x8000u,0x297u,0xffffffffu,0xffffffffu,8u,0x296u});
        method(0x4659f0u,o+0x38u);method(0x4659f0u,o+0x9ecu);
        if(!method(0x4652e0u,o+0x38u))return 0;
        if(u8(o+0xa8cu)){p8(o+0xa8cu,0);return u32(o+0xa90u);}
        std::uint32_t s=u32(0x7d68acu);
        if(s){
            if(u8(s+5u)){vslot(o,9);setup_check_4ee930();s=u32(0x7d68acu);}
            if(s&&i32(o+0x56cu)>=i32(s+0x14u)){p32(o+0x56cu,u32(s+0x14u)-1u);p32(0x659940u,u32(u32(0x7d68acu)+0x14u)-1u);}
        }
        text_print_48ee80(o+0x1a84u,0x65a7b8u,{});
        const std::uint32_t b=vslot(o,5,{1u});
        auto finish=[&]()->std::uint32_t{
            if(u32(o+0xa90u)==4u)return 4;
            if(i32(u32(0x7d68acu)+0x14u)<=1)return 0;
            vslot(o,10);
            return 0;
        };
        switch(b){
        case 0:
            if(!u8(o+0x570u)){p32(0x659940u,u32(o+0x56cu));p32(o+4u,0x22);p32(o+0xa90u,4);p8(o+0xa8cu,1);return 0;}
            switch(u32(o+0x574u)){
            case 0:p32(o+4u,0x19);p32(o+0xa90u,4);return finish();
            case 1:p32(o+4u,0x11);return 4;
            default:return finish();
            }
        case 1:
            p32(0x7d68d8u,0x12);
            s=u32(0x7d68acu);
            if(!s){p8(0x7d68d4u,1);return 0;}
            if(s!=0x98a650u||!u8(s+7u)){p8(0x7d68d4u,1);s=u32(0x7d68acu);}
            vslot(s,1,{0u});
            return 0;
        case 2:
            if(u8(o+0x570u)){p8(o+0x570u,0);p8(o+0x58fu,1);call(0x4249f0u,{1u});return 0;}
            vslot(o,7);return 0;
        case 3:if(u8(o+0x570u))list_prev_48dc10(o+0x574u,1);return 0;
        case 4:
            if(u8(o+0x570u))return 0;
            s=u32(0x7d68acu);
            if(!s)return 0;
            if(u32(o+0x56cu)==u32(s+0x14u)-1u){p8(o+0x570u,1);p8(o+0x58fu,0);call(0x4249f0u,{1u});return 0;}
            vslot(o,6);return 0;
        case 5:if(u8(o+0x570u))list_next_48dc60(o+0x574u,1);return 0;
        default:return 0;
        }
    }
    // ==== the network state screen (vtable 5CD834, 0xD8 bytes, factory 441D50): a spinner
    // (+38) while the session state [7D68D8] runs; +4 the next page, the result 1..4 ==========
    void netwait_4dc080(std::uint32_t o){method(0x48f480u,o);p32(o,0x5cd834u);method(0x465160u,o+0x38u);p32(o+8u,0xe);}
    void netwait_free_4dc0e0(std::uint32_t o){p32(o,0x5cd834u);p8(o+0x34u,0);method(0x465250u,o+0x38u);p32(o,0x5c18c8u);}
    std::uint32_t netwait_open_4dc140(std::uint32_t o){
        p8(o+0x34u,0);p32(o+4u,0x53);
        method(0x440f70u,call(0x4035f0u),{1u,0x1eu,7u});
        const std::uint32_t spr=o+0x38u;
        method(0x465250u,spr);
        call(0x465860u,{0x2c00feu,0u,0x14u,0xfu,3u,0u,0u,0x3f800000u,0x3f800000u,0x3f800000u,0u},spr);
        method(0x465970u,spr);
        return 1;
    }
    void netwait_close_4dcbc0(std::uint32_t o){method(0x441130u,call(0x4035f0u));method(0x465250u,o+0x38u);p8(o+0x34u,0);}
    bool listening(std::uint32_t s){
        if(!s)return false;
        for(std::uint32_t l=u32(s+0xcu);l;l=u32(l+0x10u))
            if((l!=0x830becu||u8(s+5u))&&u32(l+4u)==1u)return true;                                // 453CA0
        return false;
    }
    // 4DC1A0(ecx = screen): the session state machine's page: 1 a page in +4, 2 back, 3 a
    // page after a reset, 4 a message (or a new screen), 0 stay.
    std::uint32_t netwait_update_4dc1a0(std::uint32_t o){
        const std::uint32_t spr=o+0x38u;
        method(0x4659f0u,spr);
        if(u32(o+0x40u)==0xffffffffu||method(0x4652e0u,spr)){
            call(0x465860u,{0x2c00feu,0x14u,0x50u,0xfu,0u,0u,0u,0x3f800000u,0x3f800000u,0x3f800000u,0u},spr);
            method(0x465970u,spr);
            p8(o+0x34u,1);
        }
        if(!u8(o+0x34u))return 0;
        auto drop_flag=[&]{if(u8(0x7d68d0u))p8(0x7d68d0u,0);};
        auto leave=[&]{const std::uint32_t s=u32(0x7d68acu);if(s)vcall(s,0x18);};
        auto message=[&]()->std::uint32_t{
            message_492690(0x14,call(0x465eb0u,{0x1f0u}),0xffffffffu,0xa);
            p32(o+4u,0x2e);return 4;
        };
        if(u8(0x7d68bdu)&&u8(0x7d68bcu)){
            const std::uint32_t st=u32(0x7d68d8u);
            p8(0x7d68bdu,0);
            if(st==6u){p32(o+4u,1);return 1;}
            if(st==7u){p32(o+4u,7);return 1;}
            if(st==8u){p32(o+4u,0x21);return 4;}
            return 2;
        }
        if(u8(0x7d68bfu)){
            if(u8(0x7d68beu)&&!u8(0x830c00u)&&!listening(u32(0x7d68acu))){
                if(u32(0x7d68d8u)!=0x13u){
                    p8(0x7d68bfu,0);p8(0x7d68bcu,0);p8(0x7d68bdu,0);
                    p8(0x7d68d4u,0);                                                     // 454220(0)
                    if(u32(0x63aa70u)!=0xffffffffu){call(0x454b00u,{u32(0x63aa70u),0u});p32(0x63aa70u,0xffffffffu);}
                    if(u8(0x7d68d1u)){
                        p8(0x7d68d1u,0);
                        if(method(0x494410u,0x83612cu)&0xffu)return 0;
                        if(u8(0x7d68d2u)){p8(0x7d68d2u,0);return 0;}
                        const std::uint32_t page=method(0x443040u,call(0x4035f0u));
                        std::uint32_t r=0;
                        if(page)r=method(0x442ec0u,call(0x4035f0u));
                        if(page==7u||page==8u||page==0xbu||page==0xcu||page==0xdu||(page==0x15u&&(r==0xcu||r==0xdu))){p32(o+4u,6);return 3;}
                        p32(o+4u,page);return 3;
                    }
                }
            }else{
                const std::uint32_t st=u32(0x7d68d8u);
                if(st!=0x13u&&st!=0x14u&&st!=0xau&&st!=0xbu)return 0;
            }
        }
        const std::uint32_t st=u32(0x7d68d8u);
        if(u8(0x7d68c9u)&&u8(0x7d68cau)){p8(0x7d68c9u,0);return 2;}
        if(u8(0x7d68c7u)&&u8(0x7d68c8u)){
            p8(0x7d68c7u,0);
            if(st==2u)return 2;
            if(st==3u){p32(0x7d68d8u,0x16);p8(0x7d68d4u,1);call(0x454100u,{0u});return 0;}
            if(st==4u){p32(0x7d68d8u,0x16);call(0x454140u,{0u,1u});return 0;}
        }
        if(listening(u32(0x7d68acu))||listening(u32(0x7d68b0u))){
            if(!u8(0x7d68d4u))return 0;
        }
        p8(0x7d68d4u,0);
        const std::uint32_t state=u32(0x7d68d8u);
        static constexpr std::uint8_t map[0x23]={0,1,2,2,2,1,1,1,1,0x11,3,3,0x11,4,0x11,5,6,7,8,8,9,9,0xa,0xb,0xc,1,1,1,0xd,0xe,1,0x11,1,0xf,0x10};
        if(state>0x22u)return 0;
        std::uint32_t s=u32(0x7d68acu);
        auto busy=[&](bool both){
            if(u8(0x830c00u))return true;
            if(listening(u32(0x7d68acu)))return true;
            return both&&listening(u32(0x7d68b0u));
        };
        switch(map[state]){
        case 1:return 2;
        case 2:return (u8(0x7d68c7u)||u8(0x7d68c9u))?0u:2u;
        case 3:
            if(u8(0x830c00u)){p8(0x7d68d4u,1);return 0;}
            drop_flag();return 2;
        case 4:p32(o+4u,0);return 1;
        case 5:drop_flag();leave();p32(o+4u,6);return 3;
        case 6:drop_flag();leave();p32(o+4u,8);return 3;
        case 7:drop_flag();leave();p32(o+4u,7);return 3;
        case 8:{
            if(busy(false)){p8(0x7d68d4u,1);return 0;}
            s=u32(0x7d68acu);
            if(state==0x13u){p32(0x7d68d8u,0);p8(0x7d68d1u,1);method(0x4946e0u,0x83612cu);return 0;}   // 454240
            drop_flag();
            if(s)vcall(s,0x18);
            const std::uint32_t now=u32(0x7d68d8u);
            method(0x444b20u,call(0x4035f0u),{now==0x12u?0u:1u,0u,1u});
            return 2;}
        case 9:
            if(busy(true)){p8(0x7d68d4u,1);return 0;}
            drop_flag();leave();return 2;
        case 0xa:
        case 0xb:
            if(map[state]==0xbu){p32(0x7d68d8u,0);if(!s||!u8(s+5u))return 2;}
            if(!s)return 2;
            if(u8(s+5u)){p32(o+4u,u8(s+7u)?0xcu:0xdu);return 1;}
            if(method(0x442ef0u,call(0x4035f0u))==7u&&!u32(0x68c81cu)){
                const std::int32_t n=i32(0x659930u);
                if(n<=0)return message();
                std::int32_t k=0;
                p32(0x659934u,0);
                while(k<n&&!u32(0x7dbddcu+std::uint32_t(k)*0x90u))++k;
                p32(0x659934u,std::uint32_t(k));
                if(k>=n)return message();
                call(0x454180u,{1u,0u});
                return 0;
            }
            p32(o+4u,0xb);return 1;
        case 0xc:
            if(busy(false)){p8(0x7d68d4u,1);return 0;}
            leave();
            p32(0x7d68d8u,0x16);
            method(0x440e00u,call(0x4035f0u),{1u});
            method(0x434e00u,0x988f40u);
            return 0;
        case 0xd:p32(0x7d68d8u,0);return 2;
        case 0xe:{
            std::int32_t v=i32(0x672df0u);
            const std::uint32_t fl=call(0x4035f0u)+4u+0x208u;                                  // 535EF0
            auto field=[&](std::int32_t x){p32(fl,(u32(fl)&~0x3f000u)|((std::uint32_t(x)<<12)&0x3f000u));};
            auto bump=[&](std::uint32_t d){const std::uint32_t f=u32(fl);p32(fl,(f&~0x3f000u)|(((f&0xfffff000u)+d)&0x3f000u));};
            if(v>=0x50){
                p32(fl,u32(fl)|0x800u);
                if(v!=0x51&&v!=0x53)p32(fl,(u32(fl)&0xfffc5fffu)|0x5000u);
                else p32(fl,(u32(fl)&0xfffcbfffu)|0xb000u);
                p32(o+4u,0xa);return 1;
            }
            bool hi=false;
            if(v>=0x3c){
                if(v>=0x46)hi=true;
                p32(fl,u32(fl)|0x800u);
                if(u32(fl)&3u){
                    if(hi){field(v-0xb);bump(0x6000);}else field(v-1);
                }else{
                    if(hi){field(v-6);bump(0x6000);}else field(v+4);
                }
                p32(o+4u,0xa);return 1;
            }
            if(v>=0x1e)hi=true;
            if(u32(fl)&3u){
                if(hi){field(v+0x13);bump(0xf000);}else field(v-0xf);
            }else{
                if(hi){field(v-0x1e);bump(0xf000);}else field(v);
            }
            p32(fl,u32(fl)&0xfffff7ffu);
            p32(o+4u,0xa);return 1;}
        case 0xf:
            if(busy(true)){p8(0x7d68d4u,1);return 0;}
            drop_flag();leave();
            p32(0x659930u,u32(0x659938u));p32(0x659934u,u32(0x65993cu));
            p32(0x7d68acu,0x98a650u);p32(0x7d68d8u,0x16);
            call(0x454180u,{1u,0u});
            return 0;
        case 0x10:
            if(busy(true)){p8(0x7d68d4u,1);return 0;}
            drop_flag();leave();
            p32(0x7d68d8u,0);
            return message();
        default:return 0;
        }
    }
    // 4DCCC0(ecx = message screen): the error panel (text [7D68CC] + 238).
    std::uint32_t neterror_4dccc0(std::uint32_t o){
        p32(o+4u,6);
        Frame L(*this,0x110);
        call(0x5802ddu,{L(0),call(0x465eb0u,{u32(0x7d68ccu)+0x238u})});
        const std::uint32_t panel=o+0x38u;
        vslot(panel,1);
        const std::uint32_t a[9]={0x6255d5u,L(0),1u,u32(0x694decu),u32(0x694df0u),u32(0x694df4u),u32(0x694df8u),0xeu,0u};
        panel_open_48d0c0(panel,a);
        text_print_48ee80(panel+0xdecu,call(0x465eb0u,{0x210u}),{});                     // 48CBC0
        method(0x441300u,call(0x4035f0u),{0x28u});
        return 1;
    }
    // ==== the network error screen (vtable 5CD84C): a message panel (+38) ==================
    void neterror_4dcc00(std::uint32_t o){method(0x48f480u,o);p32(o,0x5cd84cu);panel_48c490(o+0x38u);p32(o+8u,0xf);}
    void neterror_free_4dcc60(std::uint32_t o){p32(o,0x5cd84cu);panel_free_48c520(o+0x38u);p32(o,0x5c18c8u);}
    // 4DCD60(ecx = error screen): its frame: once confirmed (or the state moves on) the
    // session is left and the next page chosen from the state [7D68D8] / owner page.
    std::uint32_t neterror_update_4dcd60(std::uint32_t o){
        bool confirm=false;
        if(listening(u32(0x7d68acu))||listening(u32(0x7d68b0u))){if(!u8(0x7d68d4u))confirm=true;}
        if(!confirm&&u8(0x7d68d0u))confirm=true;
        if(!confirm){
            p8(0x7d68d4u,0);
            if(u8(o+0x34u)){const std::uint32_t s=u32(0x7d68acu);if(s)vcall(s,0x18);p8(0x98a5f4u,0);}
            const std::uint32_t i=u32(0x7d68d8u)-2u;
            static constexpr std::uint8_t map[0x1d]={0,9,9,1,2,3,4,5,9,9,9,5,1,6,7,8,9,9,9,9,9,9,9,1,9,9,1,1,1};
            switch(i<0x1du?map[i]:9u){
            case 0:method(0x4946e0u,0x83612cu);return 2;
            case 1:return 2;
            case 2:p32(0x63aa6cu,0);p32(o+4u,0x16);return 4;
            case 3:p32(0x63aa6cu,1);p32(o+4u,0x16);return 4;
            case 4:p32(0x63aa6cu,2);p32(o+4u,0x16);return 4;
            case 5:p32(o+4u,0);return 1;
            case 6:p32(o+4u,6);return 3;
            case 7:method(0x444b20u,call(0x4035f0u),{0u,0u,1u});p32(o+4u,8);return 2;
            case 8:method(0x444b20u,call(0x4035f0u),{0u,0u,1u});p32(o+4u,7);return 2;
            default:p32(o+4u,method(0x442ef0u,call(0x4035f0u)));return 3;
            }
        }
        if(vslot(o,5,{1u}))return 0;
        p8(o+0x34u,1);
        auto page=[&]{return method(0x443040u,call(0x4035f0u));};
        if(u32(0x7d68d8u)!=2u){
            const bool early=(method(0x4412f0u,call(0x4035f0u))&0xffu)||method(0x442ef0u,call(0x4035f0u))==0x2cu;
            std::uint32_t st=u32(0x7d68d8u);
            bool reset=false;
            if(early&&u8(0x7d68d3u)&&st!=0x1fu)reset=true;
            if(!reset){
                if(u32(0x7d68ccu)==9u){
                    p8(o+0x34u,0);
                    if(u32(0x7d68acu))vcall(u32(0x7d68acu),4,{0u});
                    st=u32(0x7d68d8u);
                }
                if(u8(0x7d68d3u)&&st!=0x1fu)reset=true;
            }
            if(reset){p8(0x7d68d3u,0);p32(0x7d68d8u,0x19);p8(o+0x34u,0);}
            else if(st==0x1eu)p8(o+0x34u,0);
            else if(st==0x1fu){
                const std::uint32_t pg=page();
                if(pg==0xdu||pg==0xcu||pg==0xbu||pg==0x15u||pg==7u)p32(o+4u,6);else p32(o+4u,pg);
            }else if(st==0x1cu||st==0x1du){p8(o+0x34u,0);query_reset_435ff0(0x98a708u);}
            else{
                const std::uint32_t pg=page();
                if(pg==1u)p32(0x7d68d8u,0xe);
                else if(pg==0xdu||pg==0xcu||pg==0xbu||pg==0x15u)p32(0x7d68d8u,method(0x564c90u,call(0x4035f0u))?0x11u:0x10u);
                else if(pg==0u)p32(0x7d68d8u,6);
                else if(pg==6u)p32(0x7d68d8u,7);
                else if(pg==8u)p32(0x7d68d8u,0x10);
                else if(pg==7u)p32(0x7d68d8u,0x11);
            }
        }
        if(u8(0x7d68d0u))p8(0x7d68d0u,0);
        p8(0x7d68d4u,1);
        if(u8(o+0x34u)){
            const std::uint32_t s2=u32(0x7d68b0u);
            if(s2&&!u32(0x7d68acu))method(0x453c40u,s2);
            const std::uint32_t s=u32(0x7d68acu);
            if(s)vcall(s,4,{0u});
        }
        return 0;
    }
    // ==== Demonware core: the allocator setup (525340) and the logger (bdLogger 85DEA8:
    // vtable 5EA73C, +4 the root channel; a channel (0x28): +0 its name, +4 a hashmap of
    // sub-channels, +1C.. its subscribers) ==================================================
    // 525340(set): the allocators registered once (523170..5231C0); twice: a warning.
    void core_init_525340(std::uint32_t set){
        if(!u8(0x85deb4u)){
            if(set&0xffu){
                p32(0x85de8cu,0x525150u);p32(0x85de98u,0x525240u);p32(0x85de90u,0x525170u);
                p32(0x85de9cu,0x525270u);p32(0x85de94u,0x5251e0u);p32(0x85dea0u,0x5252d0u);
            }
            p8(0x85deb4u,1);
            return;
        }
        Frame L(*this,0x10);
        log_header_523e40(L(0),0x5eacfcu,0x5ead0cu,0x3b,0x5ea6b8u);
        log_523fb0(L(0),0x5ead34u,0x5ead3cu,{});
    }
    void log_header_523e40(std::uint32_t h,std::uint32_t file,std::uint32_t func,std::uint32_t line,std::uint32_t chan){p32(h,file);p32(h+4u,func);p32(h+8u,line);p32(h+0xcu,chan);}
    // 523F40: the logger singleton (its destructor 523E60 registered at the first use).
    std::uint32_t logger_523f40(){
        if(u32(0x85dea8u))return u32(0x85dea8u);
        std::uint32_t o=call(0x5231d0u,{8u});
        if(o){p32(o,0x5ea73cu);p32(o+4u,0);}
        p32(0x85dea8u,o);
        const std::uint32_t reg=registry_438c60();
        if(!u8(reg+0x10u)){
            const std::uint32_t list=reg+4u;
            if(u32(list+8u)==u32(list+4u))ptr_grow_567c10(list,1);
            p32(u32(list)+u32(list+8u)*4u,0x523e60u);
            p32(list+8u,u32(list+8u)+1u);
        }
        return u32(0x85dea8u);
    }
    // 5255A0(ecx = map, size, load): an empty bdHashMap (a power-of-two table).
    void hashmap_5255a0(std::uint32_t m,std::uint32_t n,float lf){
        if(lf<=0.0f||lf>f32(0x62806cu)){
            Frame L(*this,0x10);
            log_header_523e40(L(0),0x5eae9cu,0x5eaec8u,0x1dc,0x5ea6b8u);
            log_523fb0(L(0),0x5eaf48u,0x5eaf50u,{});
            p32(m+8u,0x3f400000u);
        }
        p32(m,0);
        const std::uint32_t cap=pow2_5253d0(n);
        p32(m+4u,cap);pf(m+8u,lf);
        double c=double(std::int32_t(cap));
        if(std::int32_t(cap)<0)c+=4294967296.0;
        p32(m+0xcu,std::uint32_t(std::int64_t(c*double(lf))));                                    // 582194
        const std::uint32_t t=call(0x5231d0u,{cap*4u});
        p32(m+0x10u,t);
        for(std::uint32_t k=0;k<cap;++k)p32(t+k*4u,0);
    }
    // 525860(ecx = channel, name).
    void channel_525860(std::uint32_t c,std::uint32_t name){
        string_523d60(c,name);
        hashmap_5255a0(c+4u,4,0.75f);
        p32(c+0x1cu,0);p32(c+0x20u,0);p32(c+0x24u,0);
    }
    // 523E80(ecx = logger, name): a new root channel (the old one freed).
    void logger_root_523e80(std::uint32_t lg,std::uint32_t name){
        const std::uint32_t old=u32(lg+4u);
        if(old){method(0x5258c0u,old);call(0x523200u,{old});}
        const std::uint32_t c=call(0x5231d0u,{0x28u});
        if(c)channel_525860(c,name);
        p32(lg+4u,c);
    }
    // bdString helpers: its length (523D50) and equality (523D00).
    std::uint32_t string_hash_5254a0(std::uint32_t key){
        const std::uint32_t s=u32(key),n=u32(u32(key)-8u);
        std::uint32_t h=0;
        for(std::uint32_t k=0;k<n;++k)h=h*0x1fu+std::uint32_t(std::int32_t(i8(s+k)));
        return h;
    }
    bool string_equal_523d00(std::uint32_t a,std::uint32_t b){
        std::uint32_t x=u32(a),y=u32(b);
        for(;;){const std::uint32_t c=u8(x);if(c!=u8(y))return false;if(!c)return true;++x;++y;}
    }
    // 5254E0(ecx = map, &key): the node of the key (+0 the value, +4 the key, +8 next).
    std::uint32_t hashmap_find_5254e0(std::uint32_t m,std::uint32_t key){
        if(!u32(m))return 0;
        const std::uint32_t h=string_hash_5254a0(key);
        for(std::uint32_t nd=u32(u32(m+0x10u)+((u32(m+4u)-1u)&h)*4u);nd;nd=u32(nd+8u))
            if(string_equal_523d00(nd+4u,key))return nd;
        return 0;
    }
    // 525670(ecx = channel, file, function, line, path, message): to its subscribers, then
    // down the path's first part.
    void channel_log_525670(std::uint32_t c,std::uint32_t file,std::uint32_t func,std::uint32_t line,std::uint32_t path,std::uint32_t msg){
        for(std::uint32_t i=0;i<u32(c+0x24u);++i){
            const std::uint32_t sub=u32(u32(c+0x1cu)+i*4u);                                       // 525540
            vcall(sub,4,{u32(c),file,func,line,msg});
        }
        std::uint32_t len=0;
        while(u8(path+len)&&u8(path+len)!=0x2fu)++len;
        const bool slash=u8(path+len)==0x2fu;
        if(!len)return;
        Frame L(*this,0x80);
        const std::uint32_t part=L(0x10),S=L(0);
        for(std::uint32_t k=0;k<len;++k)p8(part+k,u8(path+k));
        p8(part+len,0);
        string_523d60(S,part);
        const std::uint32_t nd=hashmap_find_5254e0(c+4u,S);
        if(nd)channel_log_525670(u32(nd),file,func,line,path+len+(slash?1u:0u),msg);
        string_free_523df0(S);
    }
    // 523FB0(header, channel, format, ...): a log line ("dw/<level>/<channel>").
    void log_523fb0(std::uint32_t h,std::uint32_t chan,std::uint32_t fmt,const std::vector<std::uint32_t>& va){
        Frame L(*this,0x4c0);
        const std::uint32_t B=L(0),M=L(0xa0),V=L(0x4a0);
        std::uint32_t n=slen(u32(h+0xcu));
        if(n>0x95u)n=0x95u;
        for(std::uint32_t k=0;k<n;++k)p8(B+k,u8(u32(h+0xcu)+k));
        p8(B+n,0);
        n=slen(B);
        if(n<0x95u)copy_431ba0(B+n,chan,0x96u-n);
        for(std::size_t k=0;k<va.size()&&k<8u;++k)p32(V+std::uint32_t(k)*4u,va[k]);
        call(0x580265u,{M,0x400u,fmt,V});
        p8(M+0x3ffu,0);
        const std::uint32_t lg=logger_523f40();
        if(!u32(lg+4u))logger_root_523e80(lg,0x5ea740u);
        channel_log_525670(u32(lg+4u),u32(h),u32(h+4u),u32(h+8u),B+3u,M);
    }
    // 456C30(ecx = CommRace block) / 456E30: the players finished (+58) or still connected.
    std::uint32_t racing_count_456c30(std::uint32_t block){
        std::uint32_t n=0;
        for(std::uint32_t i=0;i<u32(block);++i)
            if(u8(block+i*0x6cu+0x5cu)||connected_456ae0(block,i))++n;
        return n;
    }
    // 48E6D0(ecx = box, text, left, top, right, bottom, first): a long text word-wrapped
    // into the box's rectangle, drawn from line `first` (lines above top or below bottom are
    // not drawn); the number of lines past the bottom.
    std::uint32_t text_scroll_48e6d0(std::uint32_t box,std::uint32_t text,std::int32_t left,std::int32_t top,std::int32_t right,std::int32_t bottom,std::int32_t first){
        const std::int32_t W=right-left;
        call(0x42ca60u,{u32(box+0x450u)});
        call(0x42cc60u,{u32(box+0x478u),u32(box+0x47cu)});
        call(0x42ccb0u,{u32(box+0x454u)});
        call(0x42cca0u,{u32(box+0x474u)});
        call(0x42c360u,{u32(box+0x480u)|0x100u});
        const std::int32_t lh=i32(box+0x484u);
        std::int32_t y=top-lh*first;
        const std::int32_t space=std::int32_t(call(0x42c370u,{0x62646cu}));
        Frame F(*this,0x220);
        const std::uint32_t L=F(0),B=F(0x110);
        p8(B,0);
        const std::int32_t n=std::int32_t(slen(text))+1;
        std::int32_t lineLen=0,linew=0,wlen=0,over=0;
        auto emit=[&]{
            if(y>=top&&y<=bottom-lh){
                call(0x42cc00u,{std::uint32_t(left),std::uint32_t(y)});
                call(0x42cdd0u,{0x626468u,L});
                call(0x42c360u,{u32(box+0x480u)|0x100u});
            }else if(y>bottom-lh)++over;
            y+=lh;
        };
        for(std::int32_t i=0;i<n;++i){
            const std::uint32_t ch=u8(text+std::uint32_t(i));
            if(ch!=0x20u&&ch!=0xau&&ch!=0u&&wlen+lineLen!=0xff){
                p8(B+std::uint32_t(wlen),ch);++wlen;p8(B+std::uint32_t(wlen),0);
                if(std::int32_t(call(0x42c370u,{B}))+linew<=W)continue;
                if(lineLen==0){
                    --i;--wlen;p8(B+std::uint32_t(wlen),0);
                    for(std::int32_t k=0;k<wlen;++k)p8(L+std::uint32_t(k),u8(B+std::uint32_t(k)));
                    p8(L+std::uint32_t(wlen),0);
                    emit();
                    wlen=0;lineLen=0;
                    continue;
                }
                p8(L+std::uint32_t(lineLen),0);
                emit();
                linew=0;lineLen=0;
                continue;
            }
            for(std::int32_t k=0;k<wlen;++k)p8(L+std::uint32_t(lineLen+k),u8(B+std::uint32_t(k)));
            lineLen+=wlen;
            wlen=0;
            const std::int32_t w=linew+std::int32_t(call(0x42c370u,{B}));
            if(ch!=0xau&&ch!=0u&&lineLen!=0xff){
                p8(L+std::uint32_t(lineLen),0x20);++lineLen;
                linew=w+space;
                continue;
            }
            p8(L+std::uint32_t(lineLen),0);
            emit();
            linew=0;lineLen=0;
        }
        return std::uint32_t(over);
    }
    // ---- network functions the bulk audit found reachable (work/decomp/bulk_reach_all.py) ----
    // 491FE0(ecx = message box 830D48, action, arg): the network side of a message box:
    // 0 the session's chosen player [659940] marked (453EB0: +26C = 1, +26D = 0); 1 / 4 / 2
    // a discovery 988F40 request (434BC0 kind 1 from [box+8] +18 / +1C, 434C10 kind 2 from
    // [box] +10 / +14, 434CD0 kind 5 from [830D48] +10 / +14); 8 / 9 / 10 the answer
    // 434C60(1 / 0 / 2); the other actions nothing.
    void box_answer_491fe0(std::uint32_t box,std::uint32_t action){
        auto request=[&](std::uint32_t kind,std::uint32_t a0,std::uint32_t a1){
            const std::uint32_t q=0x988f40u;
            p32(q+0x16b8u,a0);p32(q+0x16d0u,kind);p8(q+0x16cdu,0);p32(q+0x16bcu,a1);p32(q+0x16dcu,0);
            clear_event_48f8e0(q+4u);p32(q+4u,1);
        };
        switch(action){
        case 0:{
            const std::uint32_t s=u32(0x7d68acu);
            if(!s)return;
            const std::uint32_t pl=player_by_id_453e70(s,u16(u32(s+u32(0x659940u)*4u+0x18u)));
            if(pl){p8(pl+0x26cu,1);p8(pl+0x26du,0);}
            return;}
        case 1:{const std::uint32_t r=u32(box+8u);request(1,u32(r+0x18u),u32(r+0x1cu));return;}
        case 2:{const std::uint32_t r=u32(0x830d48u);request(5,u32(r+0x10u),u32(r+0x14u));return;}
        case 4:{const std::uint32_t r=u32(box);request(2,u32(r+0x10u),u32(r+0x14u));return;}
        case 8:query_answer_434c60(0x988f40u,1);return;
        case 9:query_answer_434c60(0x988f40u,0);return;
        case 10:query_answer_434c60(0x988f40u,2);return;
        default:return;
        }
    }
    // 42C370(text) after 42CA60(9) / 42C360(0x100): the width of a text in font 9,
    // proportional, spaces 0.3 of a cell.
    std::uint32_t text_width_42c370(const std::string& t){
        const auto* fonts=c.event_function36.frontend_fonts;
        if(!fonts||fonts->fonts[9].token!=9u)throw std::runtime_error("network: 42C370 font 9 not loaded");
        FrontendTextStyle style{};style.flags=0x100u;
        return std::uint32_t(frontend_text_width_42c480(fonts->fonts[9],style,t,0.3f));
    }
    // 459AA0(ecx = queue 7EE070, text id, name): a LAN race message (16 entries of 0x18:
    // +0 the text id, +4 the name, 16 bytes and a 0). A full queue (+194 = 16) drops the shown
    // entry +180; the new one goes at +184; +18C = the width of its name, +190 the width of
    // the shown entry's text (drawn by 456540); +188 = 0xF0 frames when it was empty.
    void message_459aa0(std::uint32_t q,std::uint32_t text,std::uint32_t name){
        if(u32(q+0x194u)==0x10u){p32(q+0x180u,(u32(q+0x180u)+1u)&0xfu);p32(q+0x188u,0xf0);p32(q+0x194u,0xf);}
        const std::uint32_t e=q+u32(q+0x184u)*0x18u;
        p32(e,text);
        for(std::uint32_t k=0;k<16u;++k)p8(e+4u+k,u8(name+k));
        p8(e+0x14u,0);
        std::string who;for(std::uint32_t k=0;k<16u&&u8(e+4u+k);++k)who+=char(u8(e+4u+k));
        p32(q+0x18cu,text_width_42c370(who));
        const auto* table=c.event_function36.frontend_text;
        const std::uint32_t id=u32(q+u32(q+0x180u)*0x18u);
        const auto* shown=table?table->get(id):nullptr;
        if(!shown)throw std::runtime_error("network: 459AA0 text "+std::to_string(id));
        p32(q+0x190u,text_width_42c370(*shown));
        p32(q+0x184u,(u32(q+0x184u)+1u)&0xfu);
        const std::uint32_t n=u32(q+0x194u);
        if(!n)p32(q+0x188u,0xf0);
        p32(q+0x194u,n+1u);
    }
    // 431380(ecx = host listener 988ED4): closed: +24 = 0, its two references +14 / +18
    // released, +20 / +1C = 0, then 48F900.
    void listener_close_431380(std::uint32_t o){
        if(u8(o+0x24u))p8(o+0x24u,0);
        if(u32(o+0x14u)){release_525f80(u32(o+0x14u));p32(o+0x14u,0);}
        if(u32(o+0x18u)){release_525f80(u32(o+0x18u));p32(o+0x18u,0);}
        p32(o+0x20u,0);p32(o+0x1cu,0);
        method(0x48f900u,o);
    }
    // 432C60(ecx = 988EFC): +18 = -1, its event (+4) cleared and set to 1.
    void request_432c60(std::uint32_t o){p32(o+0x18u,0xffffffffu);clear_event_48f8e0(o+4u);p32(o+4u,1);}
    // 4355F0(ecx = guest 98A628): +20 = 0, then 491CC0.
    void guest_close_4355f0(std::uint32_t o){p8(o+0x20u,0);method(0x491cc0u,o);}
    // 49E4D0(page): the frontend owner's 4440F0(page).
    std::uint32_t owner_page_49e4d0(std::uint32_t page){return method(0x4440f0u,owner(),{page});}
    // 4BFA70(course, car): the arcade course's car list (5C9204 [course's 689FA8 index],
    // 12-byte entries ending at a 0 one) holds car (+8).
    std::uint32_t arcade_car_4bfa70(std::uint32_t course,std::uint32_t car){
        const std::uint32_t t=u32(0x5c9204u+std::uint32_t(std::int32_t(i8(0x689fa8u+course)))*4u);
        for(std::uint32_t k=0;;){
            if(u32(t+k*12u+8u)==car)return 1;
            k=std::uint32_t(std::int32_t(std::int8_t(std::uint8_t(k+1u))));
            if(!u32(t+k*12u))return 0;
        }
    }
    // 434EB0(ecx = settings): the lobby settings 65A7A4.. (signed bytes) into +14..+44,
    // 65A7B1 / 65A7B2 into +0 / +4, the name 65A7B8 (strncpy 16) into +48.
    void settings_434eb0(std::uint32_t o){
        for(std::uint32_t k=0;k<13u;++k)p32(o+0x14u+k*4u,std::uint32_t(std::int32_t(i8(0x65a7a4u+k))));
        p32(o,std::uint32_t(std::int32_t(i8(0x65a7b1u))));
        p32(o+4u,std::uint32_t(std::int32_t(i8(0x65a7b2u))));
        bool end=false;
        for(std::uint32_t k=0;k<16u;++k){const std::uint8_t ch=end?0:u8(0x65a7b8u+k);if(!ch)end=true;p8(o+0x48u+k,ch);}
    }
    // 431B10(keep): the LAN host session 98A650 current; 77FF90 the settings (6 players and
    // slot 0 unless keep), at least 2 players and the slot below the count; 988ED8 asked (1).
    void host_open_431b10(std::uint32_t keep){
        p32(0x7d68acu,0x98a650u);
        p32(0x77ff98u,0);p32(0x77ff9cu,0);p8(0x77ffa0u,0);
        if(!(keep&0xffu)){p32(0x77ff90u,6);p32(0x77ff94u,0);}
        settings_434eb0(0x77ff90u);
        std::int32_t n=i32(0x77ff90u);
        if(n<=2){n=2;p32(0x77ff90u,2);}
        if(i32(0x77ff94u)>=n-1)p32(0x77ff94u,std::uint32_t(n-1));
        clear_event_48f8e0(0x988ed8u);p32(0x988ef4u,0);p32(0x988ed8u,1);
    }
    // 435EF0(ecx = session): the session current, the guest join asked (988F20 = 1, +14 = 0,
    // +1C = 1).
    void join_435ef0(std::uint32_t s){
        p32(0x7d68acu,s);p32(0x988f34u,0);p32(0x988f3cu,1);clear_event_48f8e0(0x988f20u);p32(0x988f20u,1);
    }
    // 435AE0(reset): the LAN session 98A650 current, 77FFEC = 1, its 13 slots 77FFF0.. = -1
    // when reset, then its vtable +C (435EF0).
    void guest_open_435ae0(std::uint32_t reset){
        p32(0x7d68acu,0x98a650u);p32(0x77ffecu,1);
        if(reset&0xffu)for(std::uint32_t k=0;k<13u;++k)p32(0x77fff0u+k*4u,0xffffffffu);
        vcall(0x98a650u,0xc,{reset});
    }
    // 48FC70(ecx = 659930, flag): the notice of the network error [830C34]: 6 goes back to
    // the frontend page 0x46 (49E4D0, 659951 = 0, returns 0); 1..5 / 7 show their text in
    // the message box (492690 kind 0x18) and reopen the owner (4459F0(0)), returning 1.
    std::uint32_t error_notice_48fc70(std::uint32_t flag){
        const std::uint32_t code=u32(0x830c34u);
        if(code==6u){p8(0x659951u,0);owner_page_49e4d0(0x46u);return 0;}
        std::uint32_t id=0;
        switch(code){
        case 1:id=0x441u;break;
        case 2:id=(flag&0xffu)?0x46bu:0x26cu;break;
        case 3:id=call(0x47f110u)==1u?0x435u:0x26bu;break;
        case 4:id=call(0x47f110u)==1u?0x437u:0x436u;break;
        case 5:id=0x483u;break;
        case 7:id=0x479u;break;
        default:throw std::runtime_error("network: 48FC70 error "+std::to_string(code)+" has no text (the PC passes its flag as one)");
        }
        message_492690(0x18,call(0x465eb0u,{id}),0xffffffffu,0xa);
        method(0x4459f0u,owner(),{0u});
        return 1;
    }
    // 494410(ecx = service): the online client's shutdown (Demonware 5318A0 / 5308F0 until
    // idle, then freed); always 0. Without an online client (98A6FC, made only by the
    // sign-in states of 494790) there is nothing to do.
    bool service_shutdown_494410(std::uint32_t& eax){
        if(u32(0x98a6fcu))return false;   // online client: the translated original
        eax=0;return true;
    }

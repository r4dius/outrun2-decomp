    // ==== Heart Attack request objects and cars (463950 pre-actions, escorts, gates) ==========
    // 46E4A0(v): the protected entry swaps [64E08C] (traffic enabled) with v, returning the old value.
    std::uint32_t traffic_gate_46e4a0(std::uint32_t v){const std::uint32_t old=u32(0x64e08cu);p32(0x64e08cu,v);return old;}
    std::uint32_t language_4493c0(){return u32(0x7d2698u);}
    // 45B000(eax record, ecx hint): place a record on the road at its section word (+4):
    // +8 the run point raised by [6280C0], +14 the Euler angles of the run frame.
    void road_frame_45b000(std::uint32_t rec,std::uint32_t hint){
        Frame f(*this,0x120);
        const std::uint32_t P=f(0x30),O=f(0xb8),m1=f(0x40),m2=f(0x4c),m3=f(0x58),m4=f(0x64),d1=f(0xc),d2=f(0x24),cr=f(0x18),M=f(0x70);
        p32(P,0);p32(P+4,0);p16(P+8,u16(rec+4));
        call(0x43e570u,{O,P,hint});
        p32(rec+8,u32(O+8));p32(rec+0xc,u32(O+0xc));p32(rec+0x10,u32(O+0x10));
        pf(rec+0xc,sadd(f32(rec+0xc),K(0x6280c0u)));
        blend_40f180(m1,O+0x24,0.5f,O+0x3c,0.5f);
        blend_40f180(m2,O+0x30,0.5f,O+0x48,0.5f);
        blend_40f180(m3,O+0x24,0.5f,O+0x30,0.5f);
        blend_40f180(m4,O+0x3c,0.5f,O+0x48,0.5f);
        sub_40efa0(d1,m2,m1);sub_40efa0(d2,m4,m3);
        (void)unit_40eeb0(d1);(void)unit_40eeb0(d2);
        cross_40eff0(cr,d2,d1);
        p32(M,u32(d1));p32(M+4,u32(d1+4));p32(M+8,u32(d1+8));
        p32(M+0x10,u32(cr));p32(M+0x14,u32(cr+4));p32(M+0x18,u32(cr+8));
        p32(M+0x20,u32(d2));p32(M+0x24,u32(d2+4));p32(M+0x28,u32(d2+8));
        euler_449640(rec+0x14,M);
    }
    // 45C880(eax kind): close the gate objects 0x134.. of a request (17: [7F8A74]/2, 19: 40).
    void gates_close_45c880(std::uint32_t kind){
        if(kind==0x11u){for(std::int32_t i=0;i<i32(0x7f8a74u)/2;++i)call(0x4401d0u,{0x134u+std::uint32_t(i)});}
        else if(kind==0x13u){for(std::uint32_t i=0;i<0x28u;++i)call(0x4401d0u,{0x134u+i});}
    }
    // 461080(eax kind): open the gate objects (17: event 0x4B per gate pair, 19: 40 x 0x61).
    void gates_open_461080(std::uint32_t kind){
        Frame f(*this,0x30);
        const std::uint32_t L=f(0);
        const float k=K(0x5b0060u);
        pf(L,k);pf(L+4,k);pf(L+8,k);pf(L+0xc,0.0f);pf(L+0x10,0.0f);pf(L+0x14,0.0f);
        if(kind==0x13u){
            p32(L+0x18,0x12);p32(L+0x1c,8);p32(L+0x20,7);p32(L+0x24,0);
            for(std::uint32_t i=0;i<0x28u;++i)gate_open_4f0bd0(0x134u+i,0x61,L);
        }else if(kind==0x11u){
            p32(L+0x18,0x15);p32(L+0x1c,9);p32(L+0x20,0);p32(L+0x24,0);
            const std::uint32_t lo=u16(u32(0x7f24d0u)+4),hi=u16(u32(0x7f24d4u)+4);
            const std::int32_t a=std::int8_t(count_45c8e0(lo,hi,0xe));
            const std::int32_t b=std::int8_t(count_45c8e0(lo,hi,0x16));
            const std::int32_t n=(a+b)/2;
            for(std::int32_t i=0;i<n;++i)gate_open_4f0bd0(0x134u+std::uint32_t(i),0x4b,L);
        }
    }
    // 4611D0(eax kind; table): 17 pairs the posts (0E / 16) into gates 7F1CC8 and places the gate
    // objects; 16 spaces the cones (0B) from table 5AFB58 + n*16 (start, mid, end, split).
    void gates_place_4611d0(std::uint32_t kind,std::uint16_t tab){
        const std::uint32_t pl=player();
        if(kind==0x11u){
            Frame f(*this,0x40);
            for(std::uint32_t a=0x7f1cc8u;a<0x7f2038u;a+=4)p32(a,0);
            std::uint32_t half=0,pairs=0,slot=0;p32(0x7f8a74u,0);
            for(std::uint32_t k=0;k<0x114u;++k){
                if(!oso_live(k))continue;
                const std::uint32_t w=oso_work(k);
                const std::int16_t s=std::int16_t(section_43f370(w+0x40));
                const std::uint32_t c=u32(w+0x5c);
                if(c!=0xeu&&c!=0x16u)continue;
                if(s<i16(u32(0x7f24d0u)+4)||s>i16(u32(0x7f24d4u)+4))continue;
                p32(0x7f8a74u,u32(0x7f8a74u)+1u);
                p32(0x7f1cc8u+(half+slot)*4u,w);
                if(half==1u){++pairs;slot+=0xb;}
                half^=1u;
            }
            for(std::uint32_t i=0;i<pairs;++i){
                const std::uint32_t e=0x7f1cc8u+i*0x2cu;
                pf(e+0x18,driving::x87_float(dist_40f140(u32(e)+0x40,u32(e+4)+0x40)));
                sub_40efa0(f(0),u32(e)+0x40,u32(e+4)+0x40);
                angles_449800(e+0x1c,f(0));
                pf(e+0x24,0.0f);
                p32(e+8,u32(u32(e)+0x40));p32(e+0xc,u32(u32(e)+0x48));
                p32(e+0x10,u32(u32(e+4)+0x40));p32(e+0x14,u32(u32(e+4)+0x48));
            }
            for(std::uint32_t i=0;i<pairs;++i){
                const std::uint32_t e=0x7f1ce8u+i*0x2cu,id=0x134u+i;
                if((u8(0x79fb48u+id)&3u)!=2u)continue;
                const std::uint32_t w=work(id),v=f(0x10);
                blend_40f180(v,u32(e-0x20u)+0x40,0.5f,u32(e-0x1cu)+0x40,0.5f);
                pf(v+4,sadd(f32(v+4),K(0x5b005cu)));
                push_unit();translate_40a2d0(v);rotate_y(sadd(f32(e),K(0x628260u)));rotate_x(f32(e-4u));get_40a0d0(w);pop();
                p32(w+0x40,u32(v));p32(w+0x44,u32(v+4));p32(w+0x48,u32(v+8));
            }
            return;
        }
        if(kind!=0x10u)return;
        Frame f(*this,0x500);
        const std::uint32_t T=f(0x54),O1=f(0x24),V1=f(0x40);
        const std::uint32_t tb=0x5afb58u+std::uint32_t(tab)*16u;
        const std::uint32_t lo=u16(u32(0x7f24d0u)+4),hi=u16(u32(0x7f24d4u)+4);
        std::int32_t ebx=std::int8_t(count_45c8e0(lo,hi,0xb));
        const std::uint32_t split=call(0x44f0f0u,{call(0x44dc50u,{u32(pl+0x68)}),u32(0x7f24d0u)?u16(u32(0x7f24d0u)+4):0u});
        const std::int32_t total=ebx;
        if(split)ebx=ebx/2;
        std::int32_t ebp=0;
        for(std::uint32_t k=0;k<0x114u;++k){
            if(!oso_live(k))continue;
            const std::uint32_t w=oso_work(k);
            if(u32(w+0x5c)!=0xbu)continue;
            if(ebp==0){
                const std::int32_t n1=cvtt(smul(float(ebx),f32(tb+0xc)));
                pf(w+0x50,float(i32(w+0xa4)));
                const float st1=sdiv(ssub(f32(tb+4),f32(tb)),float(n1));
                for(std::int32_t i=0;i<n1;++i)pf(T+std::uint32_t(i)*4u,smul(sadd(smul(float(i),st1),f32(tb)),f32(w+0x50)));
                const std::int32_t n2=ebx-n1;
                const float st2=sdiv(ssub(f32(tb+8),f32(tb+4)),float(n2));
                for(std::int32_t i=0;i<n2;++i)pf(T+std::uint32_t(n1+i)*4u,smul(sadd(smul(float(i),st2),f32(tb+4)),f32(w+0x50)));
            }
            pf(w+0x50,f32(T+std::uint32_t(ebp)*4u));
            if(gt(K(0x6281ecu),smul(f32(w+0x50),K(0x6282a4u))))pf(w+0x50,K(0x5b02bcu));
            const float half=smul(f32(w+0x50),K(0x6282a4u));
            push_unit();translate_40a2d0(w+0x40);rotate_z(f32(w+0x70));rotate_y(f32(w+0x6c));rotate_x(f32(w+0x68));
            pf(V1,half);pf(V1+8,0.0f);pf(V1+4,0.0f);
            point_40a7d0(O1,V1);
            pf(w+0xb4,f32(O1));pf(w+0xb8,f32(O1+8));
            pf(V1,ssub(0.0f,half));
            point_40a7d0(O1,V1);
            pf(w+0xbc,f32(O1));pf(w+0xc0,f32(O1+8));
            pop();
            ++ebp;
            if(split&&ebp==ebx)ebp=0;
            if(ebp==total)return;
        }
    }
    // 45B5B0(eax table, n section): open the escort cars of 64AAD8 + table*0x78 (8 x 0xC) as
    // traffic events 0x59 (+324 = 6, +C5C 40000) at the player's section + n.
    void escorts_45b5b0(std::uint32_t table,std::uint32_t n){
        const std::uint32_t pl=player();
        Frame f(*this,0x40);
        const std::uint32_t P=f(0x30),pos=f(0x24),ang=f(0x18),spd=f(0x14);
        std::uint32_t e=0x64aad8u+table*0x78u;
        std::uint32_t id=u32(f(0x10));
        for(std::uint32_t j=0;j<8u;++j,e+=0xcu){
            p32(P,u32(pl+0x5c));p32(P+4,u32(pl+0x60));p32(P+8,u32(pl+0x64));p32(P+0xc,u32(pl+0x68));
            p16(P+8,std::uint16_t(u16(e)+n));p8(P+0xa,u8(e+2));
            if(i16(e)<0){p32(0x7f8a50u,0);return;}
            std::int32_t i=1;const std::int32_t cnt=i32(0x680ad8u);
            if(cnt>1){
                for(;i<cnt;++i){id=u32(0x680ad4u)+std::uint32_t(i)+8u;if(!(u8(0x79fb48u+id)&3u))break;}
            }
            if(i==cnt){p32(0x7f8a50u,0);return;}
            if(!place_pose_470240(P,0,pos,ang,spd)){p32(0x7f8a50u,0);return;}
            call(0x440110u,{id,0x59});
            const std::uint32_t cs=std::uint16_t(u16(pl+0x25e)+u16(P+8));
            const std::uint32_t w=work(id);
            set_work_46e830(w,id,std::int8_t(u32(e+4)),u8(e+8),pos,ang,f32(spd),P,0,cs,1);
            const std::uint32_t c=u32(w+0xc5c)&0xfffffffeu;
            p32(w+0x324,6);p16(w+0x25e,u16(pl+0x25e));p32(w+0xc5c,c|0x40000u);p16(w+0xb64,0);
            p8(w+0xb74,u8(e+9));p32(w+0xb58,0x12);p32(w+0xb5c,0x12);pf(w+0x1c4,0.0f);p32(w+0x1f4,0);
        }
        p32(0x7f8a50u,0);
    }
    // 45D470(edi record {kind, ., section word, lane byte}): open one Heart Attack car at the
    // player's section + record section, lane + 1. 1 = done or skipped, 0 = no free event.
    std::uint32_t spawn_45d470(std::uint32_t rec){
        const std::uint32_t pl=player();
        Frame f(*this,0x40);
        const std::uint32_t P=f(0x2c),pos=f(0x20),ang=f(0x14),spd=f(0x10),col=f(8);
        p32(P,u32(pl+0x5c));p32(P+4,u32(pl+0x60));p32(P+8,u32(pl+0x64));
        p16(P+8,u16(rec+4));p32(P+0xc,u32(pl+0x68));p8(P+0xa,u8(rec+6)+1u);
        std::int32_t i=0;std::uint32_t id=0x19au;
        const std::int32_t cnt=i32(0x680ad8u);
        if(cnt<=0)return 0;
        for(;i<cnt;++i){
            id=u32(0x680ad4u)+std::uint32_t(i)+8u;
            if(!(u8(0x79fb48u+id)&3u)&&!call(0x440370u,{id}))break;
        }
        if(i==i32(0x680ad8u)||id==0x19au)return 0;
        if(!place_pose_470240(P,0,pos,ang,spd)){p32(0x7f24bcu,u32(0x7f24bcu)+1u);return 1;}
        call(0x440110u,{id,0x59});
        p32(0x7f24c4u,u32(0x7f24c4u)+1u);
        const std::uint32_t w=work(id);
        std::uint32_t model;
        const std::uint32_t kind=u32(rec);
        if(kind<=1u)model=0x2b;
        else if(kind==2u)model=0x1f;
        else if(kind<=4u){
            if(call(0x44dc50u,{u32(pl+0x68)})==0x1au)model=u32(0x5e0c50u+std::uint32_t(std::int32_t(id)%7)*4u);
            else{const std::int32_t dv=i32(0x78024cu)==1?6:7;model=u32(0x5e0c34u+std::uint32_t(std::int32_t(id)%dv)*4u);}
        }else model=u32(spd);
        std::uint8_t colour;
        const std::int32_t lb=std::int8_t(std::uint8_t(id));
        if(model==0x26u)colour=std::uint8_t(lb%2);
        else if(model==0x2bu)colour=i8(0x7f24c0u)<0?2u:std::uint8_t(std::int32_t(id)%2);
        else colour=std::uint8_t(lb%3);
        p8(col,colour);
        const std::uint32_t cs=std::uint16_t(u16(pl+0x25e)+u16(rec+4));
        set_work_46e830(w,id,std::int8_t(model),colour,pos,ang,f32(spd),P,0,cs,1);
        const std::uint32_t c=u32(w+0xc5c)|0x40000u;
        p32(w+0xc5c,c);p32(w+0x324,u32(rec));p16(w+0xb62,0x3c);p16(w+0xb70,std::uint16_t(id*0x25u));
        p32(w+0xb58,0x12);p32(w+0xb5c,0x12);pf(w+0x1c4,0.0f);p32(w+0x1f4,0);
        if(u8(0x7f24c0u)&0x40u)p32(w+0xc5c,c|0x80000u);
        return 1;
    }
    // 45D710(ecx): select spawn table ecx & 0x3F (63EE68 / 644C28 with 78024C == 1); a negative
    // byte spawns the whole table at once.
    void spawn_table_45d710(std::uint32_t v){
        const std::uint32_t base=(i32(0x78024cu)==1?0x644c28u:0x63ee68u)+(v&0x3fu)*0x320u;
        p32(0x7f24b8u,base);p32(0x7f24c0u,v);p32(0x7f24bcu,0);p32(0x7f24c4u,0);p32(0x7f250cu,0);
        if(!(std::int8_t(std::uint8_t(v))<0))return;
        for(std::uint32_t o=0;o<0x320u;o+=8){
            const std::uint32_t r=u32(0x7f24b8u)+o;
            if(i16(r+4)<0||i8(r+6)<0)break;
            if(!spawn_45d470(r))break;
        }
    }
    // 45D7A0: spawn the table's cars as the player reaches 30..50 sections before them.
    void spawn_step_45d7a0(){
        const std::uint32_t pl=player();
        if(!u32(0x7f24b8u)||i8(0x7f24c0u)<0||i32(0x7f24bcu)>=0x64)return;
        for(std::uint32_t o=u32(0x7f24bcu)*8u;o<0x320u;o+=8){
            const std::uint32_t r=u32(0x7f24b8u)+o;
            if(i16(r+4)<0||i8(r+6)<0)return;
            const std::int32_t s=i16(r+4),c=i16(pl+0x64);
            if(s>=c+0x1e){
                if(s>c+0x32)return;
                if(!spawn_45d470(r))return;
            }
            p32(0x7f24bcu,u32(0x7f24bcu)+1u);
        }
    }
    // 4628D0 (mode 16): request statistics: objects 4 touched by speed, distance in the lead
    // (7F1C5C), distance on the throttle (7F1C60).
    void stats_4628d0(){
        if(u32(0x78026cu)!=0x10u)return;
        const std::uint32_t pl=player();
        if(u8(pl+4)&8u)return;
        for(std::uint32_t k=0;k<0x114u;++k){
            if(!oso_live(k))continue;
            const std::uint32_t w=oso_work(k);
            if(u32(w+0x5c)!=4u||!u32(w+0x3f8)||u32(w+0x3fc)!=8u)continue;
            p16(0x7f1c58u,u16(0x7f1c58u)+1u);
            const float s=f32(pl+0x1f8);
            std::uint32_t add;
            if(!jb(s,K(0x5b0304u)))add=10;else if(!jb(s,K(0x5b0074u)))add=8;else add=(s>=K(0x5afd30u))?4u:2u;
            p32(0x7f1c54u,u32(0x7f1c54u)+add);
        }
        if(u32(pl+0x5c)!=1u)return;
        if(i16(0x7f1c5au)<0)p16(0x7f1c5au,u16(pl+0x260));
        if(i8(pl+0xd36)>0){
            const std::int16_t cx=i16(0x7f1c5eu);
            if(cx>0){
                const std::uint16_t ax=u16(pl+0x260);
                if(std::int32_t(ax)>std::int32_t(cx)){p16(0x7f1c5cu,std::uint16_t(u16(0x7f1c5cu)+std::uint16_t(ax-std::uint16_t(cx))));p16(0x7f1c5eu,ax);}
            }else p16(0x7f1c5eu,u16(pl+0x260));
        }else p16(0x7f1c5eu,0xffffu);
        if(!(u32(pl+4)&0x80000u)){
            const std::uint32_t mm=u32(pl+0x258)|u32(pl+0x254)|u32(pl+0x250)|u32(pl+0x24c);
            if(((mm>>8)&0x3bu)&&!(u32(pl+0x2a8)&0x10f407cu)){
                const std::int16_t cx=i16(0x7f1c62u);
                if(cx>0){
                    const std::uint16_t ax=u16(pl+0x260);
                    if(std::int32_t(ax)>std::int32_t(cx)){p16(0x7f1c60u,std::uint16_t(u16(0x7f1c60u)+std::uint16_t(ax-std::uint16_t(cx))));p16(0x7f1c62u,ax);}
                }else p16(0x7f1c62u,u16(pl+0x260));
                return;
            }
        }
        p16(0x7f1c62u,0xffffu);
    }
    // 463950(eax pre-action record {section, -code, params}, current section): the actions that
    // run before a request (voices, banners, gates, escorts, traffic on / off). 1 when done.
    std::uint32_t pre_action_463950(std::uint32_t r,std::int16_t cur){
        const std::int16_t sec=i16(r);
        if(sec==0||sec>cur)return 0;
        const std::int16_t code=i16(r+2);
        if(code>=0)return 0;
        const std::uint32_t idx=std::uint32_t(std::int32_t(code)+0x13);
        const std::int32_t w4=i16(r+4);
        switch(idx){
        case 0:p8(0x7f8a64u,u8(r+4));break;
        case 1:escort_speed_45b520();break;
        case 2:escort_land_45d830();break;
        case 3:p32(0x7f2528u,0);p32(0x7f2524u,0);break;
        case 4:setup_462e20(u16(r+8),u16(r+6),std::uint32_t(w4));break;
        case 5:spawn_table_45d710(std::uint32_t(w4));break;
        case 6:gates_close_45c880(std::uint32_t(w4));break;
        case 7:escorts_45b5b0(std::uint32_t(w4),u16(r+6));break;
        case 8:gates_place_4611d0(std::uint32_t(w4),u16(r+6));break;
        case 9:gates_open_461080(std::uint32_t(w4));break;
        case 13:{
            static constexpr std::uint8_t B[26]{0,13,1,13,13,13,2,3,4,5,13,6,13,13,7,8,9,13,13,10,11,13,12,4,3,9};
            static constexpr std::uint32_t V[13]{0xe5,0xe7,0xe6,0xe9,0,0xde,0xdd,0xe2,0xe0,0xe3,0xe1,0xe4,0xdf};
            const std::uint32_t b=std::uint32_t(w4)<=0x19u?B[w4]:13u;
            if(b==4u)voice_45bd30(i16(r+6)==1?0xebu:0xeau,0x3c,0xa);
            else if(b<13u)voice_45bd30(V[b],0x3c,0xa);
            break;}
        case 14:{
            const std::uint32_t stage=call(0x44dc50u,{call(0x450380u,{8})});
            p32(0x7f8cacu,u32(0x64a9e8u+language_4493c0()*4u));p16(0x7f8c84u,u16(r+4));
            p32(0x7f8ce8u,u32(0x64aa00u+language_4493c0()*4u));p16(0x7f8cc0u,u16(r+6));
            p32(0x7f24d0u,0x7f8c80u);p32(0x7f24d4u,0x7f8cbcu);
            road_frame_45b000(0x7f8c80u,0xffffffffu);road_frame_45b000(u32(0x7f24d4u),0xffffffffu);
            p32(0x7f24d8u,0);p32(0x7f24dcu,0);
            const std::uint32_t s1=call(0x44ddc0u,{stage,u16(0x7f8c84u)});
            if(s1!=0xffffffffu){
                p32(0x7f8d24u,u32(0x64a9e8u+language_4493c0()*4u));p16(0x7f8cfcu,u16(0x7f8c84u));p32(0x7f8cf8u,1);p32(0x7f24d8u,0x7f8cf8u);
                road_frame_45b000(0x7f8cf8u,s1);
            }
            const std::uint32_t s2=call(0x44ddc0u,{stage,u16(0x7f8cc0u)});
            if(s2!=0xffffffffu){
                p32(0x7f8d60u,u32(0x64aa00u+language_4493c0()*4u));p16(0x7f8d38u,u16(0x7f8cc0u));p32(0x7f8d34u,1);p32(0x7f24dcu,0x7f8d34u);
                road_frame_45b000(0x7f8d34u,s2);
            }
            break;}
        case 15:banner_open_460970(1,std::uint32_t(w4),2,0xffffffffu,0x78);break;
        case 16:banner_open_460970(0,std::uint32_t(w4),2,0x1e,0xffffffffu);break;
        case 17:traffic_gate_46e4a0(1);break;
        case 18:traffic_gate_46e4a0(0);break;
        default:break;
        }
        return 1;
    }

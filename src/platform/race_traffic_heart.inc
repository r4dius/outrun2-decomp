    // ==== Heart Attack (variant 2) mission logic, PC module 45AFB0..4650E3 ====================
    // Included inside struct T (race_traffic.cpp). NAVI_PUB control 4BCC40 runs 464D20 for
    // variant 2 in mode 16. State: the NAVI quest block 7F1900..7F2A00 / 7F8A00..7F8C00
    // (7F2428 request kind, 7F23C4 hearts score, 7F24E8 request bonus, 7F8AFC request result,
    // 7F2548 + level*0x44 per-level records, 7F1994.. voice queue), the OSO objects (events
    // 32..307: flags 79FB68+k, works 79A2B8+k*0x3C) and the robots 362/363 (works [79F010+i*0x3C],
    // the driver and the girl whose reactions the requests trigger).
    // 46C530: the network session's player word, 0 offline.
    std::uint32_t net_46c530(){return call(0x55a930u,{},0,0x7f9460u)?u32(0x7f95c4u):0u;}
    std::uint32_t robot_work(std::uint32_t id){return u32(0x79f010u+id*0x3cu);}
    // 487D60(robot, motion): play a reaction (+68 motion, +78 = 2); the girl (1) only offline.
    void rob_anim_487d60(std::uint32_t id,std::uint32_t v){
        if(net_46c530()&&id==1u)return;
        const std::uint32_t w=robot_work(id);p32(w+0x68,v);p32(w+0x78,2);
    }
    // 487D90(robot, face): +70 face, +78 = 0, +7C = 1.
    void rob_face_487d90(std::uint32_t id,std::uint32_t v){
        if(net_46c530()&&id==1u)return;
        const std::uint32_t w=robot_work(id);p32(w+0x70,v);p32(w+0x78,0);p32(w+0x7c,1);
    }
    void rob_stop_487dd0(std::uint32_t id){p32(robot_work(id)+0x7c,0);}
    // 487DF0(robot) -> 4F1E70 IsMotionEnd of the robot's RobMotion 82F5F0 + [work]*0xA4.
    std::uint32_t rob_end_487df0(std::uint32_t id){return (u32(0x82f5f0u+u32(robot_work(id))*0xa4u+8u)>>2)&1u;}
    // 45B380(model): 1 for the models whose byte in 45B3AC is 0 (2..24).
    std::uint32_t model_flag_45b380(std::int8_t model){
        static constexpr std::uint8_t B[23]{0,0,1,0,1,1,0,0,1,1,0,1,1,0,1,0,0,1,1,1,1,1,0};
        const std::uint32_t k=std::uint32_t(std::int32_t(model)-2);
        return k<=0x16u&&B[k]==0u?1u:0u;
    }
    // 45BD30(id, delay, priority): the voice queue 7F1998 (8 entries {id, delay, priority word}),
    // head 7F1C64, tail 7F1994, last start time 7F8B40 (time = ftol [842110]).
    std::uint32_t voice_45bd30(std::uint32_t id,std::uint32_t delay,std::uint32_t prio){
        if(net_46c530())return 0;
        const std::uint32_t t=ftol(x(0x842110u));
        const std::uint32_t e=0x7f1998u+u32(0x7f1c64u)*12u;
        if(!(t-u32(0x7f8b40u)<=u32(e+4))){
            p32(0x7f8b40u,t);p32(e+4,std::uint32_t(std::int32_t(std::int16_t(delay))));p16(e+8,prio);p32(e,id);
            if(id!=0xffffffffu)call(0x424940u,{id});
            return 1;
        }
        std::uint32_t tail=u32(0x7f1994u);
        if(i16(0x7f19a0u+tail*12u)>=std::int16_t(prio))return 0;
        tail=(tail+1u)&7u;p32(0x7f1994u,tail);
        const std::uint32_t q=0x7f1998u+tail*12u;
        p16(q+8,prio);p32(q,id);p32(q+4,std::uint32_t(std::int32_t(std::int16_t(delay))));
        return 1;
    }
    std::uint32_t ha_level(){return call(0x44c940u,{u32(player()+0x68)});}
    // 45AFB0(n): n hearts lost: the level record +34 and 7F23C4 (10000 a heart).
    void hearts_lost_45afb0(std::uint32_t n){
        const std::uint32_t r=0x7f2548u+ha_level()*0x44u;
        p32(r+0x34,u32(r+0x34)-n);
        p32(0x7f23c4u,u32(0x7f23c4u)+n*0xffffd8f0u);
    }
    void score_take(std::uint32_t v){const std::uint32_t c=u32(0x7f23c4u);p32(0x7f1c80u,c);p32(0x7f23c4u,c-v);}
    void inc32(std::uint32_t a){p32(a,u32(a)+1u);}
    // 45E2F0(table esi): the request's common scoring (table: +0 crash, +4 overtaken, +8 slip,
    // +C wreck, +10 per clean wheel, +14 spin). The protected entry loads EDI = [799D18].
    std::uint32_t events_45e2f0(std::uint32_t tbl){
        const std::uint32_t pl=player();
        std::uint32_t eax=0,ecx;
        const std::uint32_t f4=u32(pl+4);
        if(!(f4&0x80000u)&&!(u8(pl+0x2f0)&1u)){
            if(f4&0x4000000u){
                const std::uint32_t v=u32(tbl+0xc);score_take(v);
                ecx=u32(0x7f24e8u)+v;inc32(0x7f24ecu);p32(0x7f24e8u,ecx);inc32(0x7f24f4u);eax=1;
            }else if(f4&0x500000u){
                const std::uint32_t v=u32(tbl+0x14);score_take(v);
                ecx=u32(0x7f24e8u)+v;inc32(0x7f2504u);p32(0x7f24e8u,ecx);eax=1;
            }else ecx=u32(0x7f24e8u);
        }else{
            const std::uint32_t k=(u32(pl+0x2f0)>>2)&0x1fu;
            static constexpr std::uint8_t B[14]{0,0,1,1,0,0,1,2,2,2,2,0,0,0};
            const std::uint32_t sel=k-1u<=0xdu?B[k-1u]:2u;
            if(sel==0u){
                const std::uint32_t v=u32(tbl);score_take(v);
                p32(0x7f1c64u,u32(0x7f1994u));
                voice_45bd30(k==1u?0x14du:0x14eu,0x3c,5);
                ecx=u32(0x7f24e8u)+u32(tbl);inc32(0x7f24ecu);inc32(0x7f24fcu);p32(0x7f24e8u,ecx);eax=1;
            }else if(sel==1u){
                const std::uint32_t ref=u32(pl+0xdd4);
                if(ref!=0x19au){
                    const float a=f32(pl+0x1c4),b=f32(work(ref)+0x1c4);
                    if((u32(pl+4)&0x4000000u)?gt(b,a):gt(a,b)){
                        const std::uint32_t v=u32(tbl+4);score_take(v);
                        p32(0x7f24e8u,u32(0x7f24e8u)+v);p32(0x7f1c64u,u32(0x7f1994u));
                        voice_45bd30(0x149,0x3c,5);inc32(0x7f24f8u);
                    }
                    inc32(0x7f24ecu);
                }
                eax=1;ecx=u32(0x7f24e8u);
            }else ecx=u32(0x7f24e8u);
        }
        const std::uint32_t mask=0x2f03f82u;
        for(std::uint32_t o:{0x24cu,0x250u,0x254u,0x258u})
            if(!(u32(pl+o)&mask)){ecx+=u32(tbl+0x10);inc32(0x7f2500u);p32(0x7f24e8u,ecx);}
        if(u32(pl+4)&0x20000u){ecx+=u32(tbl+8);p32(0x7f24e8u,ecx);inc32(0x7f2508u);}
        return eax;
    }
    void result_8afc(std::int32_t v){p32(0x7f8afcu,std::uint32_t(v)-u32(0x7f24e8u));}
    float pctK(){return K(0x5afd24u);}
    // 45E610 (request 0): keep the girl's heart up with speed; 7F8A90 2 = boost phase.
    void request_45e610(){
        const std::uint32_t pl=player();
        const std::uint32_t spd=ftol(x(pl+0x1c4)*X87(K(0x5b0170u)));
        call(0x44c940u,{call(0x450380u,{8})});
        const std::int8_t al=i8(pl+0xd36);
        if(u32(0x7f8a90u)==2u){
            if(al>0){const std::int32_t v=i32(0x7f8a9cu)-1;p32(0x7f8a9cu,std::uint32_t(v));if(v<0)p32(0x7f8a9cu,0);}
            else if(i32(0x7f8a9cu)<0x78)p32(0x7f8a9cu,u32(0x7f8a9cu)+2u);
            else{p32(0x7f8a90u,1);p16(0x7f8aacu,0);rob_stop_487dd0(1);}
        }else if(al>0){
            const std::uint16_t si=u16(pl+0x64);
            p32(0x7f8a90u,2);p32(0x7f8a9cu,0);
            voice_45bd30(0xc6,0x3c,0);
            p32(0x7f8aa4u,u32(pl+0x1f4));
            const std::int32_t q=std::int32_t(u32(0x7f8aa0u)-u32(0x7f24e8u))/80000;
            p16(0x7f8aacu,si);
            if(std::uint32_t(q)>5u)rob_face_487d90(1,4);
            else rob_face_487d90(1,q<=2?0u:(q<=4?5u:3u));
        }
        events_45e2f0(0x5b0158u);
        if(u32(0x7f8a90u)==2u){
            const std::int32_t span=std::int32_t(std::int16_t(std::uint16_t(u16(0x7f8ab6u)-u16(0x7f8ab4u))))*0x5a;
            const std::uint32_t esi=u32(0x7f8a98u)+1u;
            const std::int16_t di=i16(pl+0x64);
            const float ratio=sdiv(float(i32(0x7f8ab0u)),float(span/100));
            const std::int32_t ecx=cvtt(smul(ratio,pctK()));
            p32(0x7f8a98u,esi);
            const std::int16_t ax=i16(0x7f8aacu);
            if(di>ax){p32(0x7f8ab0u,u32(0x7f8ab0u)+std::uint32_t(std::int32_t(di)-std::int32_t(ax)));p16(0x7f8aacu,std::uint16_t(di));}
            if(ecx>i32(0x7f8aa0u))p32(0x7f8aa0u,std::uint32_t(ecx));
            const std::int32_t s=std::int32_t(esi);
            bool talk=false;
            if(spd>0xf0u)talk=true;
            else if(spd>0xc8u&&s%0x3d==0)talk=true;
            else if(spd>0x96u&&s%0xbd==0)talk=true;
            if(talk){
                const std::int32_t r=s%5;
                static constexpr std::uint32_t V[5]{0xcb,0xc7,0xc8,0xc9,0xca};
                if(std::uint32_t(r)<=4u)voice_45bd30(V[r],0x3c,0);
            }
        }
        result_8afc(i32(0x7f8aa0u));
    }
    // 45F3D0 (requests 2 / 22): rank changes (4F2B10 + 4F2B00 = [84DF0C] + [84DF00]).
    void request_45f3d0(){
        const std::uint32_t pl=player();
        const std::uint32_t edi=u32(0x84df0cu)+u32(0x84df00u);
        events_45e2f0(u32(0x7f2428u)==0x16u?0x5b01a8u:0x5b0190u);
        const float r=sdiv(float(std::int32_t(edi-u32(0x7f2988u))),float(std::int32_t(i16(0x7f1c50u))));
        const std::int32_t eax=cvtt(smul(r,pctK()));
        p32(0x7f1c4cu,std::uint32_t(eax));result_8afc(eax);
        if(u32(0x7f8b38u)!=edi){
            p32(0x7f8b38u,edi);
            if(u32(0x7f2428u)==0x16u)call(0x424940u,{0xd8});
            call(0x424940u,{0x50});
            const std::int32_t r5=i32(0x7f23dcu)%5;
            p32(0x7f8b30u,10);
            static constexpr std::uint32_t V[5]{0xc6,0xc7,0xc8,0xc9,0xca};
            if(std::uint32_t(r5)<=4u)voice_45bd30(V[r5],0x3c,0);
            rob_anim_487d60(1,girl_cheer(pl,u32(0x7f2428u)==0x16u?0x6003du:0x6003fu));
        }
        inc32(0x7f23dcu);
    }
    // The girl's reaction for model 9 (6003D) or by rand(): >= 0.5 -> 6003E, else `other`.
    std::uint32_t girl_cheer(std::uint32_t pl,std::uint32_t other){
        if(i8(pl+0x11)==9)return 0x6003du;
        const float f=smul(float(std::int32_t(call(0x580f40u,{}))),K(0x62827cu));
        return jb(f,K(0x628064u))?other:0x6003eu;
    }
    // 45F540 (request 6): distance along the stage.
    void request_45f540(){
        const std::uint32_t pl=player();
        const float a=float(std::int32_t(i16(pl+0x64))-std::int32_t(i16(0x7f23bcu)));
        const float b=float(std::int32_t(std::int16_t(std::uint16_t(u16(0x7f23beu)-u16(0x7f23bcu)))));
        const std::int32_t edi=cvtt(smul(sdiv(a,b),pctK()));
        if(!events_45e2f0(0x5b01c0u)){rob_face_487d90(1,0);rob_stop_487dd0(1);}
        else voice_45bd30(0x14f,0x3c,6);
        std::int32_t eax=i32(0x7f23c0u);
        if(edi>eax){eax=edi;p32(0x7f23c0u,std::uint32_t(eax));}
        result_8afc(eax);
    }
    bool oso_live(std::uint32_t k){return (u8(0x79fb68u+k)&3u)==2u;}
    std::uint32_t oso_work(std::uint32_t k){return u32(0x79a2b8u+k*0x3cu);}
    // 45E8A0 (requests 7 / 24): objects of kind [7F1C90] hit (+3F8, +3FC == 8), minus kind 6 hits.
    void request_45e8a0(){
        const std::uint32_t pl=player();
        for(std::uint32_t k=0;k<0x114u;++k){
            if(!oso_live(k))continue;
            const std::uint32_t w=oso_work(k),c=u32(w+0x5c);
            if(c==u32(0x7f1c90u)){
                if(!u32(w+0x3f8)||u32(w+0x3fc)!=8u)continue;
                if(u32(0x7f2428u)==0x18u&&i8(pl+0xd36)<=0)continue;
                p8(0x7f1c8eu,u8(0x7f1c8eu)+1u);rob_face_487d90(1,3);rob_stop_487dd0(1);
            }else if(c==6u&&u32(w+0x3f8)&&u32(w+0x3fc)==8u)p8(0x7f1c8fu,u8(0x7f1c8fu)+1u);
        }
        const std::int32_t got=std::int32_t(i8(0x7f1c8eu))-std::int32_t(i8(0x7f1c8fu))*3;
        const std::int32_t need=i16(0x7f1c8cu);
        if(got>=need){p32(0x7f8afcu,800000u-u32(0x7f24e8u));return;}
        result_8afc(cvtt(smul(sdiv(pctK(),float(need)),float(got))));
    }
    // 45EEB0 (requests 8 / 23): slide distance (7F2540) over the section range.
    void request_45eeb0(){
        const std::uint32_t pl=player();
        events_45e2f0(0x5b0174u);
        const float a=smul(float(i32(0x7f2538u)),f32(pl+0x26c));
        const float b=smul(f32(pl+0x268),f32(pl+0x264));
        bool ok=true;if(u32(0x7f2428u)==0x17u&&i8(pl+0xd36)<=0)ok=false;
        if(gt(a,0.0f)&&gt(0.0f,b)&&ok){
            std::int16_t ax=i16(0x7f253cu);const std::int16_t di=i16(pl+0x64);
            if(ax<0){ax=std::int16_t(di-1);p16(0x7f253cu,std::uint16_t(ax));}
            if(di>ax){p32(0x7f2540u,u32(0x7f2540u)+std::uint32_t(std::int32_t(di)-std::int32_t(ax)));p16(0x7f253cu,std::uint16_t(di));}
            rob_face_487d90(1,5);rob_stop_487dd0(1);
        }else p16(0x7f253cu,0xffffu);
        const float span=smul(float(std::int32_t(std::int16_t(std::uint16_t(u16(0x7f2532u)-u16(0x7f2530u))))),K(0x6280f0u));
        const std::int32_t ecx=cvtt(smul(sdiv(float(i32(0x7f2540u)),span),pctK()));
        std::int32_t eax=i32(0x7f2534u);
        if(ecx>eax){eax=ecx;p32(0x7f2534u,std::uint32_t(eax));}
        result_8afc(eax);
    }
    // 46EE60(i): release Heart Attack pair i (800AF8 + i*0x38): its cars get +C5C 10000.
    void pair_release_46ee60(std::uint8_t i){
        if(i>=10u)return;
        const std::uint32_t r=0x800af8u+std::uint32_t(i)*0x38u;
        auto mark=[&](std::uint32_t id){const std::uint32_t w=work(id);p32(w+0xc5c,(u32(w+0xc5c)&0xffff7fffu)|0x10000u);};
        const std::uint32_t a=u32(r);if((u8(0x79fb48u+a)&3u)==2u)mark(a);
        const std::uint32_t b=u32(r+4);const bool lb=(u8(0x79fb48u+b)&3u)==2u;
        p32(r,0x19au);if(lb)mark(b);
        p32(r+4,0x19au);
    }
    // 45EFB0 (request 9): drive between the cars of a Heart Attack pair (46D250 records).
    void request_45efb0(){
        const std::uint32_t pl=player();
        Frame f(*this,0x20);
        events_45e2f0(0x5b0174u);
        push_unit();
        rotate_y(ssub(0.0f,f32(pl+0x2e8)));
        rotate_z(driving::x87_float(-(X87(std::int32_t(i16(pl+0x30)))*X87(K(0x628254u)))));
        rotate_x(driving::x87_float(-(X87(std::int32_t(i16(pl+0x2c)))*X87(K(0x628254u)))));
        rotate_y(driving::x87_float(-(X87(std::int32_t(i16(pl+0x2e)))*X87(K(0x628254u)))));
        untranslate_40a310(pl+0x14);
        for(std::uint8_t i=0;i<10u;++i){
            const std::uint32_t r=0x800af8u+std::uint32_t(i)*0x38u;
            if(u32(r)==0x19au||u32(r+4)==0x19au)continue;
            point_40a7d0(f(0),r+8);point_40a7d0(f(0xc),r+0x14);
            if(!segment_box_4f6bd0(f(0),pl+0x338))continue;
            pair_release_46ee60(i);
            rob_anim_487d60(1,girl_cheer(pl,0x6003fu));
            const std::int16_t ax=std::int16_t(i16(0x7f23d2u)+1),cx=i16(0x7f23d0u);
            p16(0x7f23d2u,std::uint16_t(ax));
            if(ax>cx){call(0x424940u,{0x52});p32(0x7f23d8u,800000u);}
            else p32(0x7f23d8u,std::uint32_t(cvtt(smul(sdiv(float(ax),float(cx)),pctK()))));
        }
        pop();
        result_8afc(i32(0x7f23d8u));
    }
    // 45F180 (request 11): pass through the markers of the 46DB60 ring (8037C8).
    void request_45f180(){
        const std::uint32_t pl=player();
        Frame f(*this,0x10);
        const std::uint32_t quiet=model_flag_45b380(i8(pl+0x11));
        events_45e2f0(0x5b0174u);
        for(std::uint32_t k=0;k<0x20u;++k){
            const std::uint32_t rec=0x8037c8u+k*0x5cu;
            if(u32(rec)==0x19au)continue;
            for(std::uint32_t j=0;j<2u;++j){
                const std::uint32_t flag=rec+0x4cu+j*4u,pt=rec+0x28u+j*0xcu;
                if(!u32(flag))continue;
                sub_40efa0(f(0),pt,pl+0x14);
                const float d=sadd(smul(f32(f(8)),f32(f(8))),smul(f32(f(0)),f32(f(0))));
                if(jb(K(0x5b018cu),d))continue;
                p32(flag,0);
                if(!quiet)rob_anim_487d60(1,0x260003);
                const std::int16_t ax=std::int16_t(i16(0x7f1cbau)+1);p16(0x7f1cbau,std::uint16_t(ax));
                if(ax>i16(0x7f1cb8u))call(0x424940u,{0x52});
            }
        }
        const std::int16_t cx=i16(0x7f1cbau),ax=i16(0x7f1cb8u);
        const std::int32_t v=cx>ax?800000:cvtt(smul(sdiv(float(cx),float(ax)),pctK()));
        p32(0x7f1cc0u,std::uint32_t(v));result_8afc(v);
    }
    // 4617A0 (request 15): objects 0A / 39 touched (+7C, +80 == 8).
    void request_4617a0(){
        events_45e2f0(0x5b0174u);
        for(std::uint32_t k=0;k<0x114u;++k){
            if(!oso_live(k))continue;
            const std::uint32_t w=oso_work(k);
            if(!u32(w+0x7c)||u32(w+0x80)!=8u)continue;
            const std::uint32_t c=u32(w+0x5c);
            if(c==0xau)p8(0x7f1ca3u,u8(0x7f1ca3u)+1u);else if(c==0x39u)p8(0x7f1ca1u,u8(0x7f1ca1u)+1u);
            rob_face_487d90(1,4);rob_stop_487dd0(1);
        }
        const std::int32_t got=std::int32_t(i8(0x7f1ca3u))+std::int32_t(i8(0x7f1ca1u));
        result_8afc(cvtt(smul(float(got),sdiv(pctK(),float(std::int32_t(i8(0x7f1ca0u)))))));
    }
    // 461850 (requests 16 / 25): objects of kind [7F1C74] in state 3.
    void request_461850(){
        const std::uint32_t pl=player();
        for(std::uint32_t k=0;k<0x114u;++k){
            if(!oso_live(k))continue;
            const std::uint32_t w=oso_work(k);
            if(u32(w+0x5c)!=u32(0x7f1c74u))continue;
            if(u32(0x7f2428u)==0x19u&&i8(pl+0xd36)<=0)continue;
            if(u32(w+0x7c)!=3u)continue;
            p8(0x7f1c71u,u8(0x7f1c71u)+1u);rob_face_487d90(1,4);rob_stop_487dd0(1);
        }
        const float a=float(std::int32_t(i8(0x7f1c71u))),b=sdiv(pctK(),float(std::int32_t(i8(0x7f1c70u))));
        events_45e2f0(0x5b0174u);
        const float v=u8(0x7f1c71u)==u8(0x7f1c70u)?pctK():smul(b,a);
        p32(0x7f8afcu,std::uint32_t(cvtt(ssub(v,float(i32(0x7f24e8u))))));
    }
    // 462410 (request 18): the 7F8AD2 / 7F8ABC phase (input 2 = 453720) and the follow score.
    void request_462410(){
        const std::uint32_t pl=player();
        const std::uint32_t lazy=(u32(pl+4)>>17)&1u;
        const std::int32_t st=i8(0x7f8ad2u);
        enum{Check,Scan,Done};int go=Check;
        switch(std::uint32_t(st)){
        case 0:pf(0x7f8abcu,0.0f);go=Scan;break;
        case 1:{const float v=sadd(f32(0x7f8abcu),K(0x62813cu));pf(0x7f8abcu,v);
            if(!jb(v,K(0x62806cu))){pf(0x7f8abcu,K(0x62806cu));p16(0x7f8ad0u,0x78);p8(0x7f8ad2u,2);go=Done;
                if((u32(pl+4)&0x80000u)||(u8(pl+0x2f0)&1u))p8(0x7f8ad2u,3);
                else if(u32(0x7d6814u+2u*16u))p8(0x7f8ad2u,3);}
            break;}
        case 2:{const std::int16_t ax=i16(0x7f8ad0u);p16(0x7f8ad0u,std::uint16_t(ax-1));if(ax<=0)p8(0x7f8ad2u,3);break;}
        case 3:{const float v=ssub(f32(0x7f8abcu),K(0x6280ecu));pf(0x7f8abcu,v);
            if(!jb(0.0f,v)){pf(0x7f8abcu,0.0f);p8(0x7f8ad2u,0);go=Scan;}
            break;}
        default:break;
        }
        if(go==Check){
            if(gt(f32(0x7f8abcu),0.0f)){
                if((u32(pl+4)&0x80000u)||(u8(pl+0x2f0)&1u))p8(0x7f8ad2u,3);
                else if(u32(0x7d6814u+2u*16u))p8(0x7f8ad2u,3);   // 453720(2): [7D6810 + 2*16 + 4] (47F110 = 0)
                go=Done;
            }else go=Scan;
        }
        if(go==Scan){
            for(std::uint32_t k=0;k<0xfau;++k){
                if(!oso_live(k))continue;
                const std::uint32_t w=oso_work(k);
                if(u32(w+0x5c)!=0x10u)continue;
                if(X87(K(0x6280b0u))>dist_40f140(w+0x40,pl+0x14))p8(0x7f8ad2u,1);
            }
        }
        std::uint32_t edx;
        if(gt(f32(0x7f8abcu),K(0x628064u))){
            const std::int32_t a=i16(0x7f8accu),c=i16(0x7f8aceu),d=i16(0x7f8ac8u);
            float v=sdiv(float(d),smul(float(c-a),K(0x62821cu)));
            if(gt(v,K(0x62806cu)))v=K(0x62806cu);
            edx=std::uint32_t(cvtt(smul(v,K(0x5b0070u))));p32(0x7f8ac0u,edx);
            const std::int16_t ax=i16(pl+0x64),cx=i16(0x7f8acau);
            if(ax>cx)p16(0x7f8ac8u,std::uint16_t(u16(0x7f8ac8u)+std::uint16_t(ax-cx)));
        }else edx=u32(0x7f8ac0u);
        p16(0x7f8acau,u16(pl+0x64));
        if(lazy)p32(0x7f8ac4u,u32(0x7f8ac4u)+0x9c4u);
        else if((u32(pl+4)&0x80000u)||(u8(pl+0x2f0)&1u))p32(0x7f8ac4u,u32(0x7f8ac4u)+0x186a0u);
        else{
            const std::uint32_t ref=u32(pl+0xdd4);
            if(ref!=0x19au){
                const float a=f32(pl+0x1c4),b=f32(work(ref)+0x1c4);
                if(!(u32(pl+4)&0x4000000u)){if(gt(a,b))p32(0x7f8ac4u,u32(0x7f8ac4u)+0xd05u);}
                else if(gt(b,a)){
                    p32(0x7f8ac4u,u32(0x7f8ac4u)+0x7530u);p32(0x7f23c4u,u32(0x7f23c4u)-0x186a0u);
                    voice_45bd30(0x14a,0x3c,5);
                    p32(0x7f8afcu,u32(0x7f8ac0u)-u32(0x7f8ac4u));return;
                }
            }
        }
        p32(0x7f8afcu,edx-u32(0x7f8ac4u));
    }
    // 4626E0 (request 20): overtakes (+C5C 80000 cars 7 sections ahead, +B6C hits).
    void request_4626e0(){
        const std::uint32_t pl=player();
        const std::uint32_t quiet=model_flag_45b380(i8(pl+0x11));
        std::uint32_t hit=0,ahead=0,behind=0;
        events_45e2f0(0x5b02ecu);
        std::uint32_t ecx=u32(0x7f1988u);
        for(std::uint32_t i=1;i<0x18u;++i){
            if((u8(0x79fb50u+i)&3u)!=2u)continue;
            const std::uint32_t w=u32(0x799d18u+i*0x3cu);
            if(!(u32(w+0xc5c)&0x80000u))continue;
            if(std::int32_t(i16(w+0x64))-std::int32_t(i16(pl+0x64))==7){
                if(std::int32_t(ecx)>=0x3c){
                    const std::int32_t r=std::int32_t(call(0x44fe40u,{}))%3;
                    call(0x424940u,{u32(0x5b02e0u+std::uint32_t(r)*4u)});
                    ecx=0;p32(0x7f1988u,0);
                }
                ahead=1;
            }
            if(u8(w+0xb6c)&&!u32(w+0x2f4)){
                const std::int16_t ax=std::int16_t(i16(0x7f198cu)+1);p16(0x7f198cu,std::uint16_t(ax));
                const std::int32_t d=cvtt(smul(sdiv(float(std::int32_t(ax)),float(std::int32_t(i16(0x7f198eu)))),K(0x5b02dcu)));
                p32(0x7f8afcu,0u-u32(0x7f24e8u)-std::uint32_t(d));
                hit=1;
            }else if(i16(w+0x64)<i16(pl+0x64))behind=1;
        }
        p32(0x7f1988u,ecx+1u);
        if(hit&&!quiet){rob_anim_487d60(1,0x260007);return;}
        if(ahead&&rob_end_487df0(1)&&!quiet){rob_anim_487d60(1,0x60025);return;}
        if(behind&&rob_end_487df0(1))rob_anim_487d60(1,0x250003);   // no model test here
    }
    // 449C40(point ecx, segment eax): side of the point (x, z) against the segment line.
    std::int32_t side_449c40(std::uint32_t p,std::uint32_t s){
        const float px=f32(p),pz=f32(p+4),x0=f32(s),z0=f32(s+4),x1=f32(s+8),z1=f32(s+0xc);
        float v=smul(ssub(z1,pz),x0);
        v=sadd(v,smul(ssub(pz,z0),x1));
        v=sadd(v,smul(ssub(z0,z1),px));
        if(gt(v,0.0f))return 1;
        if(gt(0.0f,v))return -1;
        return 0;
    }
    // 449D00(a, b): the 2D segments a, b {x0, z0, x1, z1} intersect.
    std::uint32_t segments_449d00(std::uint32_t a,std::uint32_t b){
        auto lo=[&](std::uint32_t s,std::uint32_t o0,std::uint32_t o1){return gt(f32(s+o1),f32(s+o0))?f32(s+o0):f32(s+o1);};
        auto hi=[&](std::uint32_t s,std::uint32_t o0,std::uint32_t o1){return gt(f32(s+o0),f32(s+o1))?f32(s+o0):f32(s+o1);};
        if(gt(lo(a,0,8),hi(b,0,8)))return 0;
        if(gt(lo(a,4,0xc),hi(b,4,0xc)))return 0;
        if(gt(lo(b,0,8),hi(a,0,8)))return 0;
        if(gt(lo(b,4,0xc),hi(a,4,0xc)))return 0;
        if(!(side_449c40(b,a)*side_449c40(b+8,a)<=0))return 0;
        if(!(side_449c40(a,b)*side_449c40(a+8,b)<=0))return 0;
        return 1;
    }
    // 45CCA0 (request 17): gates 7F1CF0 (+0x2C each, [7F8A74]/2) crossed by the player's XZ step;
    // objects 0E / 16 hit; gate objects (events 0x134+i) placed between their two posts.
    void request_45cca0(){
        const std::uint32_t pl=player();
        Frame f(*this,0x20);
        pf(f(0),f32(0x7f8a84u));pf(f(4),f32(0x7f8a8cu));pf(f(8),f32(pl+0x14));pf(f(0xc),f32(pl+0x1c));
        p32(0x7f8a84u,u32(pl+0x14));p32(0x7f8a88u,u32(pl+0x18));p32(0x7f8a8cu,u32(pl+0x1c));
        auto half=[&]{return i32(0x7f8a74u)/2;};
        for(std::int32_t i=0;i<half();++i){
            const std::uint32_t e=0x7f1cf0u+std::uint32_t(i)*0x2cu;
            if(u32(e))continue;
            if(segments_449d00(f(0),e-0x20u)){p8(0x7f8a7cu,u8(0x7f8a7cu)+1u);p32(e,0x80000002u);}
        }
        for(std::uint32_t k=0;k<0x114u;++k){
            if(!oso_live(k))continue;
            const std::uint32_t w=oso_work(k),c=u32(w+0x5c);
            if((c==0xeu||c==0x16u)&&u32(w+0x3f8)&&u32(w+0x3fc)==8u){p8(0x7f8a7du,u8(0x7f8a7du)+1u);p32(w+0x3f0,0x30c);}
        }
        p32(0x7f8afcu,std::uint32_t(cvtt(smul(ssub(float(std::int32_t(i8(0x7f8a7cu))),smul(float(std::int32_t(i8(0x7f8a7du))),K(0x628064u))),K(0x5b0064u)))));
        for(std::int32_t i=0;i<half();++i){
            const std::uint32_t e=0x7f1cf0u+std::uint32_t(i)*0x2cu,id=0x134u+std::uint32_t(i);
            if(!u32(e)&&(u32(u32(e-0x28u)+0x3f8)||u32(u32(e-0x24u)+0x3f8)))p32(e,0x80000001u);
            if((u8(0x79fb48u+id)&3u)==2u){
                const std::uint32_t w=work(id),v=f(0x10);
                if(!u32(e)){blend_40f180(v,u32(e-0x28u)+0x40,0.5f,u32(e-0x24u)+0x40,0.5f);pf(v+4,sadd(f32(v+4),K(0x5b005cu)));}
                else{const float k=K(0x5b0060u);pf(v,k);pf(v+4,k);pf(v+8,k);}
                push_unit();translate_40a2d0(v);
                rotate_y(sadd(f32(e-8u),K(0x628260u)));rotate_x(f32(e-0xcu));
                get_40a0d0(w);pop();
                if(i32(e)<0)pf(w+0x74,0.0f);
            }
            if(i32(e)<0)p32(e,u32(e)^0x80000000u);
        }
    }
    // 4F6BD0(segment s {p0, p1}, box b {+x, y, +z, -x, y, -z}): the XZ segment crosses one of
    // the four box edges (planes x = b+0 / b+C, z = b+8 / b+14).
    std::uint32_t segment_box_4f6bd0(std::uint32_t s,std::uint32_t b){
        const float eps=K(0x5e0ab8u),neg=K(0x5e0abcu),zero=K(0x619a34u),one=K(0x62806cu);
        auto t_of=[&](float plane,float p0,float d,float& t){
            if(gt(eps,std::fabs(d)))return false;
            t=sdiv(ssub(plane,p0),d);
            return !jb(t,zero)&&!jb(one,t);};
        auto zat=[&](float t){return sadd(smul(ssub(one,t),f32(s+8)),smul(f32(s+0x14),t));};
        auto xat=[&](float t){return sadd(smul(ssub(one,t),f32(s)),smul(t,f32(s+0xc)));};
        float t;
        const float dx=ssub(f32(s+0xc),f32(s));
        if(!(dx>=eps)&&t_of(f32(b),f32(s),dx,t)){const float z=zat(t);if(!gt(f32(b+0x14),z)&&jbe(z,f32(b+8)))return 1;}
        if(!(neg>=dx)&&t_of(f32(b+0xc),f32(s),dx,t)){const float z=zat(t);if(!gt(f32(b+0x14),z)&&jbe(z,f32(b+8)))return 1;}
        const float dz=ssub(f32(s+0x14),f32(s+8));
        if(!(dz>=eps)&&t_of(f32(b+8),f32(s+8),dz,t)){const float xx=xat(t);if(!gt(f32(b+0xc),xx)&&jbe(xx,f32(b)))return 1;}
        if(!(neg>=dz)&&t_of(f32(b+0x14),f32(s+8),dz,t)){const float xx=xat(t);if(!gt(f32(b+0xc),xx)&&!gt(xx,f32(b)))return 1;}
        return 0;
    }
    void scale_40a360(float sx,float sy,float sz){
        std::array<float,16> k{sx,0,0,0, 0,sy,0,0, 0,0,sz,0, 0,0,0,1};
        driving::pc_matrix_multiply_current(c.matrices,driving::Bytes(reinterpret_cast<std::uint8_t*>(k.data()),64));
    }
    // Falling object step (+44 y down to the rest height +A8, +74 fall speed).
    bool oso_fall(std::uint32_t w){
        const float v=f32(w+0x74);
        const float y=ssub(sadd(sadd(smul(v,K(0x6282d4u)),f32(w+0xa8)),K(0x628128u)),smul(smul(v,v),K(0x5b02c0u)));
        pf(w+0x44,y);
        if(!jb(f32(w+0xa8),y)){p32(w+0x44,u32(w+0xa8));pf(w+0x74,0.0f);return true;}
        pf(w+0x74,sadd(f32(w+0x74),K(0x5afc4cu)));
        return false;
    }
    // 461940 (request 19): falling crates (objects 12): a landed crate spawns the five pieces of
    // table [64AAA8 + +CE*4] as gate objects 0x134.. (440BB0 -> 4AC440); 7F8AE2 broken, 7F8AE4 held.
    void request_461940(){
        const std::uint32_t pl=player();
        events_45e2f0(0x5b02c4u);
        for(std::uint32_t k=0;k<0xfau;++k){
            const std::uint32_t id=0x20u+k;
            if((u8(0x79fb48u+id)&3u)!=2u)continue;
            const std::uint32_t w=work(id);
            if(u32(w+0x5c)!=0x12u)continue;
            if(!u32(w+0xc0)){
                if(i32(0x7f8af0u)<std::int32_t(i16(0x7f8adcu))&&std::int32_t(i16(pl+0x64))+0x28>=std::int32_t(i16(w+0xcc))){
                    if(!u32(0x7f8af0u))rob_anim_487d60(1,0x220011);
                    p32(w+0xc0,1);inc32(0x7f8af0u);
                }
                continue;
            }
            if(u32(w+0xbc)){
                if(gt(f32(w+0x44),f32(w+0xa8))){
                    add_40ef10(w+0x40,w+0xb0);
                    if(!jb(f32(w+0xa8),f32(w+0x44))){
                        p32(w+0x40,u32(w+0xa4));p32(w+0x44,u32(w+0xa8));p32(w+0x48,u32(w+0xac));
                        pf(w+0x44,sadd(f32(w+0x44),K(0x628128u)));p32(w+0xbc,0);pf(w+0x74,0.0f);
                    }
                }
            }else if(u32(w+0x7c)!=2u&&gt(f32(w+0x44),f32(w+0xa8))){
                const float v=f32(w+0x74);
                const float y=ssub(sadd(sadd(smul(v,K(0x6282d4u)),f32(w+0xa8)),K(0x628128u)),smul(smul(v,v),K(0x5b02c0u)));
                pf(w+0x44,y);
                if(jb(f32(w+0xa8),y))pf(w+0x74,sadd(f32(w+0x74),K(0x5afc4cu)));
                else{
                    p32(w+0x44,u32(w+0xa8));pf(w+0x74,0.0f);
                    std::uint32_t tb=u32(0x64aaa8u+std::uint32_t(std::int32_t(i16(w+0xce)))*4u);
                    std::uint32_t base=u32(0x7f8ae8u);
                    if(std::int32_t(base)>=0x28){base=0;p32(0x7f8ae8u,0);}
                    tb+=0x18u;
                    for(std::uint32_t j=0;j<5u;++j,tb+=0x1cu){
                        if((u8(0x79fc7cu+base+j)&3u)!=2u)continue;
                        const std::uint32_t o=u32(0x79e368u+(base+j)*0x3cu);
                        push_unit();translate_40a2d0(w+0x40);translate_40a2d0(tb-0x18u);
                        rotate_y(f32(tb-8u));rotate_x(f32(tb-0xcu));rotate_z(f32(tb-4u));
                        scale_40a360(f32(tb),f32(tb),f32(tb));get_40a0d0(o);
                        p32(o+0x40,u32(o+0x30));p32(o+0x44,u32(o+0x34));p32(o+0x48,u32(o+0x38));
                        p32(o+0x68,u32(tb-0xcu));p32(o+0x6c,u32(tb-8u));p32(o+0x70,u32(tb-4u));
                        p32(o+0x50,u32(tb));
                        call(0x43eb60u,{0x100,o+0x40,0,0,0});
                        p32(o+0xa4,u32(o+0x40));p32(o+0xa8,u32(o+0x44));p32(o+0xac,u32(o+0x48));
                        pf(o+0x44,sadd(f32(o+0x44),K(0x628128u)));
                        p32(o+0x94,1);p32(o+0xbc,0);p32(o+0xc0,1);p32(o+0xc4,0);pf(o+0x74,0.0f);p32(o+0x7c,0);p32(o+0x80,0xffffffffu);
                        pop();
                        inc32(0x7f8ae8u);
                        call(0x440bb0u,{j+base+0x134u,0x4ac440u});
                    }
                }
            }
            if(u32(w+0x7c)==1u&&u32(w+0x80)==8u){p16(0x7f8ae2u,u16(0x7f8ae2u)+1u);rob_anim_487d60(1,0x260009);}
        }
        for(std::uint32_t id=0x134u;id<0x15cu;++id){
            if((u8(0x79fb48u+id)&3u)!=2u)continue;
            const std::uint32_t w=work(id),st=u32(w+0x7c);
            if(st!=2u&&gt(f32(w+0x44),f32(w+0xa8)))(void)oso_fall(w);
            if(u32(w+0x5c)==0x12u&&st==1u&&u32(w+0x80)==8u)p8(0x7f8ae4u,u8(0x7f8ae4u)+1u);
        }
        const float v=smul(sdiv(pctK(),float(std::int32_t(i16(0x7f8adeu)))),float(std::int32_t(i16(0x7f8ae2u))));
        p32(0x7f8afcu,std::uint32_t(cvtt(ssub(v,float(i32(0x7f24e8u))))));
    }

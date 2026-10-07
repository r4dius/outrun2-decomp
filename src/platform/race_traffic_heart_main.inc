    // ==== Heart Attack request manager (464350) and NAVI control step (464D20) =================
    // 45C0E0(eax start section, ecx request kind): start a request (3 = none: next request slot).
    void request_start_45c0e0(std::uint32_t sec,std::uint32_t kind){
        for(std::uint32_t o:{0x7f24ecu,0x7f24f0u,0x7f24f4u,0x7f24f8u,0x7f24fcu,0x7f2500u})p32(o,0);
        p32(0x7f246cu,sec);p32(0x7f2504u,0);p32(0x7f2428u,kind);p32(0x7f2434u,0xffffffffu);
        p32(0x7f2470u,0);p32(0x7f2474u,0);p32(0x7f24e8u,0);pf(0x7f24e0u,K(0x6282bcu));p32(0x7f2508u,0);
        if(kind==3u){p32(0x7f24c8u,u32(0x7f24c8u)+1u);p32(0x7f24d0u,0);p32(0x7f24d4u,0);}
    }
    // 45C160(bl level, esi stage): the quest of a stage by its route level.
    std::uint32_t quest_table_45c160(std::uint8_t level,std::uint32_t stage){
        std::uint32_t eax=call(0x44dc50u,{stage});
        if(std::int32_t(eax)>0xe)eax-=0xf;
        if(level==1u){const std::uint32_t i=stage-3u;if(i<=0xbu)return i<7u?0xfu+i:0x1au+(i-7u);return eax;}
        if(level==2u){const std::uint32_t i=stage-6u;if(i<=8u)return i<4u?0x16u+i:0x1fu+(i-4u);return eax;}
        if(level==3u){const std::uint32_t i=stage-0xau;if(i<=4u)return 0x24u+i;return eax;}
        return eax;
    }
    // 45C2C0(eax stage): the special quest of the stage's level entry (protected entry: EDI += 7F2548).
    std::uint32_t special_quest_45c2c0(std::uint32_t stage){
        const std::uint32_t level=call(0x44c940u,{stage}),e=0x7f2548u+level*0x44u;
        auto finish=[&]{const std::uint32_t v=quest_table_45c160(u8(e),stage);p32(e+4,v);return v;};
        p8(e,0);
        if(level==2u){
            for(std::uint32_t i=0;i<=4;++i)if(u8(0x7f2580u+i)||u8(0x7f25c4u+i))return finish();
            p8(e,1);return finish();
        }
        if(level==3u){
            auto one=[&]{p8(e,1);for(std::uint32_t i=0;i<=3;++i)if(u8(0x7f2608u+i)){p8(e,0);return finish();}return finish();};
            if(i8(0x7f25d0u)<=0)return one();
            for(std::uint32_t i=0;i<=4;++i)if(u8(0x7f2608u+i))return one();
            p8(e,2);return finish();
        }
        if(level==4u){
            const std::int8_t state=i8(0x7f2614u);
            const std::int32_t c=i8(0x7f264cu),d=i8(0x7f264du),b=i8(0x7f264eu),f=i8(0x7f264fu),g=i8(0x7f2650u);
            bool two;
            if(state==2){
                if(c+d+b+f+g+i8(0x7f2651u)==0){p8(e,3);return finish();}
                two=(c+d+b+f+g)==0;
            }else if(state<=0)two=false;
            else two=(c+d+b+f+g)==0;
            if(two){p8(e,2);return finish();}
            p8(e,1);
            for(std::uint32_t i=0;i<=3;++i)if(u8(0x7f264cu+i)){p8(e,0);return finish();}
            return finish();
        }
        return finish();
    }
    std::uint32_t stage_quest(std::uint32_t stage){
        const std::uint32_t e=0x7f2548u+call(0x44c940u,{stage})*0x44u;
        if(u8(e)!=0xffu){const std::uint32_t q=quest_table_45c160(u8(e),stage);p32(e+4,q);return q;}
        return special_quest_45c2c0(stage);
    }
    std::uint32_t quest_records(std::uint32_t quest){return (u32(0x78024cu)==0u?0x5a94d8u:0x5ac818u)+quest*0x140u;}
    // 45E050: count the requests of the player's stage quest (7F24CC).
    void quest_count_45e050(){
        const std::uint32_t t=quest_records(stage_quest(u32(player()+0x68)));
        std::uint32_t count=0;p32(0x7f24ccu,0);
        if(i16(t)>0){
            std::uint32_t p=4;
            for(std::uint32_t i=0;i<0x20u;){
                if(i16(t+p-2)>0){const std::int16_t k=i16(t+p);if(k>=0&&k<0x1a)p32(0x7f24ccu,++count);}
                p+=0xa;++i;
                if(!(i16(t+p-4)>0))break;
            }
        }
    }
    // 43F2F0(point): the course height under the point (43EB60 mode 0x100).
    float ground_43f2f0(std::uint32_t p){
        Frame f(*this,0x10);
        p32(f(0),u32(p));p32(f(4),u32(p+4));p32(f(8),u32(p+8));
        call(0x43eb60u,{0x100,f(0),0,0,0});
        return f32(f(4));
    }
    // 43F330(point): the course polygon under the point.
    std::uint32_t polygon_43f330(std::uint32_t p){
        Frame f(*this,0x10);
        p32(f(0),u32(p));p32(f(4),u32(p+4));p32(f(8),u32(p+8));p32(f(0xc),p);
        call(0x43eb60u,{0x100,f(0),f(0xc),0,0});
        return u32(f(0xc));
    }
    // 45D950(pos, angles out): the road orientation under pos (43F330 polygon, 43E570 run)
    // in the area frame 7D2DA0.
    void road_angles_45d950(std::uint32_t pos,std::uint32_t out){
        Frame f(*this,0x130);
        const std::uint32_t P=f(0x30),O=f(0xb8),m1=f(0x40),m2=f(0x4c),m3=f(0x58),m4=f(0x64),d1=f(0),d2=f(0x18),cr=f(0xc),M=f(0x70),A=f(0x24);
        p32(P,0);p32(P+4,0);
        const std::uint32_t poly=polygon_43f330(pos);
        const std::uint16_t sec=poly==0xffffffffu?0:u16(u32(0x780228u+u32(P)*4u)+poly*2u);
        p16(P+8,sec);
        call(0x43e570u,{O,P,poly});
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
        euler_449640(A,M);
        push_load(0x7d2da0u);
        rotate_y(f32(A+4));rotate_x(f32(A));rotate_z(f32(A+8));
        {Frame g(*this,0x40);get_40a0d0(g(0));euler_449640(out,g(0));}
        pop();
    }
    // 45DB70 (78024C == 0): the OSO objects' heights (0A / 22 / 39) and matrices (5..9 with the
    // 5B0078 offset, 0B / 0E / 16 on the road frame).
    void course_objects_45db70(){
        if(u32(0x78024cu))return;
        static constexpr std::uint8_t B[0x12]{0,0,0,0,0,2,1,2,2,1,2,2,2,2,2,2,2,1};
        for(std::uint32_t k=0;k<0x114u;++k){
            if(!oso_live(k))continue;
            const std::uint32_t w=oso_work(k);
            const std::uint32_t kind=u32(w+0x5c);
            if(kind==0xau||kind==0x22u||kind==0x39u){
                Frame f(*this,0x10);
                p32(f(0),u32(w+0x40));pf(f(4),K(0x5b0084u));p32(f(8),u32(w+0x48));
                pf(w+0x44,ground_43f2f0(f(0)));
            }
            const std::uint32_t c=u32(w+0x5c)-5u;
            if(c>0x11u||B[c]==2u)continue;
            road_angles_45d950(w+0x40,w+0x68);
            push_unit();translate_40a2d0(w+0x40);
            rotate_y(f32(w+0x6c));rotate_x(f32(w+0x68));rotate_z(f32(w+0x70));
            if(B[c]==1u)get_40a0d0(w);
            else{translate_40a2d0(0x5b0078u);get_40a0d0(w);point_40a7d0(w+0x40,0x5b0078u);}
            pop();
        }
    }
    // 45BCD0: play the next queued voice once the current one's delay has run.
    void voice_pump_45bcd0(){
        if(u32(0x7f1c64u)==u32(0x7f1994u))return;
        const std::uint32_t t=ftol(x(0x842110u));
        const std::uint32_t c=u32(0x7f1c64u);
        if(t-u32(0x7f8b40u)<=u32(0x7f199cu+c*12u))return;
        p32(0x7f8b40u,t);
        const std::uint32_t n=(c+1u)&7u;p32(0x7f1c64u,n);
        const std::uint32_t id=u32(0x7f1998u+n*12u);
        if(id!=0xffffffffu)call(0x424940u,{id});
    }
    // 464350: the request manager: stage start / goal bookkeeping, the stage quest's
    // pre-actions (463950), request start (45C0E0 + set-up) and end (evaluation, 462F10).
    void requests_464350(){
        const std::uint32_t q=u32(0x7f2428u),pl=player();
        const std::int16_t sec=i16(pl+0x64);
        const std::uint32_t k5c=u32(pl+0x5c),stage=u32(pl+0x68);
        const std::uint32_t lvl=call(0x44c940u,{stage});
        if((u32(0x7d39f0u)>>1)&1u){
            const std::uint32_t prev=0x7f2504u+lvl*0x44u;
            p32(0x7f247cu+lvl*4u,stage);
            for(std::uint32_t o:{0x7f24c8u,0x7f24d0u,0x7f24d4u,0x7f8b34u,0x7f24e4u,0x7f1c94u})p32(o,0);
            rob_stop_487dd0(1);traffic_gate_46e4a0(1);quest_count_45e050();
            std::int32_t edi=i32(0x7f23c4u)/10000-i32(0x7f2468u);
            p32(0x7f2448u+lvl*4u,std::uint32_t(edi));
            std::int32_t b=std::int32_t(level_bonus_45d360(lvl-1u));
            if(b>0){edi+=b;b*=10000;}
            const std::uint32_t h=u32(0x7f23c4u)+std::uint32_t(b);
            p32(0x7f2434u+lvl*4u,std::uint32_t(edi));
            p32(0x7f23c4u,h);p32(0x7f1c80u,h);p32(prev+0x34,std::uint32_t(edi));
            for(std::int32_t i=0;i<std::int32_t(i8(prev+0x40));++i)p32(prev+0x34,u32(prev+0x34)-u32(prev+0x20+std::uint32_t(i)*4u));
            p32(0x7f1c54u,0);p32(0x7f1c58u,0);p32(0x7f1c5cu,0);
            p32(0x7f2468u,std::uint32_t(std::int32_t(h)/10000));p32(0x7f2464u,0);p32(0x7f1c60u,0);p16(0x7f1c5au,0xffffu);
        }else if(call(0x44ff10u,{})&&!u32(0x7f2478u)){
            const std::uint32_t rec=0x7f2548u+lvl*0x44u;
            std::int32_t esi=i32(0x7f23c4u)/10000-i32(0x7f2468u);
            std::int32_t b=std::int32_t(level_bonus_45d360(lvl));
            if(b>0){esi+=b;b*=10000;}
            const std::uint32_t h=u32(0x7f23c4u)+std::uint32_t(b);
            p32(0x7f23c4u,h);p32(0x7f1c80u,h);
            p32(0x7f2464u,std::uint32_t(esi));p32(rec+0x34,std::uint32_t(esi));
            for(std::int32_t i=0;i<std::int32_t(i8(rec+0x40));++i)p32(rec+0x34,u32(rec+0x34)-u32(rec+0x20+std::uint32_t(i)*4u));
            p32(0x7f2478u,1);
        }
        p32(0x7f2460u,std::uint32_t(i32(0x7f1c80u)/10000-i32(0x7f2468u)));
        if(!k5c){
            const std::uint32_t quest=stage_quest(stage);
            const std::uint32_t r=quest_records(quest)+u32(0x7f24e4u)*10u;
            if(pre_action_463950(r,sec)){p32(0x7f24e4u,u32(0x7f24e4u)+1u);}
            else if(q==3u){
                const std::int16_t s0=i16(r);
                if(s0==0||s0>sec||i16(r+2)<=0){p32(0x7f2470u,u32(0x7f2470u)+1u);return;}
                request_start_45c0e0(std::uint32_t(std::int32_t(sec)),std::uint32_t(std::int32_t(i16(r+4))));
                p32(0x7f24e4u,u32(0x7f24e4u)+1u);p32(0x7f2430u,r);p32(0x7f242cu,quest);
                p32(0x7f2510u,0);p32(0x7f2520u,0);pf(0x7f2514u,f32(pl+0x1f8));pf(0x7f251cu,0.0f);
                voice_45bd30(0xe8,0x3c,9);
                request_setup(r);
                p16(0x7f8bb8u,0);p16(0x7f8bb4u,0xa);p16(0x7f8bbau,1);
                p32(0x7f2470u,u32(0x7f2470u)+1u);
                return;
            }
        }
        if(q==3u||i16(u32(0x7f2430u)+2)>sec){p32(0x7f2470u,u32(0x7f2470u)+1u);return;}
        escort_release_45d8e0();
        request_eval(q,lvl);
        p32(0x7f2470u,u32(0x7f2470u)+1u);
    }
    // The set-up of the request just started (464350 table 464CB8 / 464CF8 by 7F2428).
    void request_setup(std::uint32_t r){
        static constexpr std::uint8_t B[26]{0,15,1,15,15,15,2,3,4,5,15,6,15,15,7,8,9,10,11,12,13,14,1,4,3,9};
        const std::uint32_t q=u32(0x7f2428u);
        const std::uint32_t b=q<=0x19u?B[q]:15u;
        const std::uint16_t w0=u16(r),w2=u16(r+2),w6=u16(r+6),w8=u16(r+8);
        if(b!=0u&&b!=1u&&b!=7u&&b!=15u)reset_result();
        switch(b){
        case 0:setup_45e5b0(w0,w2);break;
        case 1:setup_45f310(w0,w2,w6,w8);break;
        case 2:p16(0x7f23bcu,w0);p16(0x7f23beu,w2);p32(0x7f23c0u,0);p32(0x7f8b3cu,0);break;
        case 3:p16(0x7f1c8cu,0x14);p16(0x7f1c88u,w0);p16(0x7f1c8au,w2);p32(0x7f1c90u,std::uint32_t(std::int32_t(std::int16_t(w6))));
            p8(0x7f1c8eu,0);p8(0x7f1c8fu,0);p16(0x7f1c8cu,w8);break;
        case 4:p16(0x7f2530u,w0);p16(0x7f2532u,w2);p32(0x7f2538u,std::uint32_t(std::int32_t(std::int16_t(w6))));
            p16(0x7f253cu,0xffffu);p32(0x7f2540u,0);p32(0x7f2534u,0);break;
        case 5:setup_45b300(w0,w2,w6,w8);break;
        case 6:p16(0x7f1cb4u,w0);p16(0x7f1cb6u,w2);p32(0x7f1cc0u,0);p16(0x7f1cb8u,w6);p16(0x7f1cbcu,w8);p16(0x7f1cbau,0);ring_clear_46ef10();break;
        case 7:setup_45f5d0(w0,w2);break;
        case 8:setup_45c950(w0,w2,u8(r+8));break;
        case 9:p16(0x7f1c6cu,w0);p16(0x7f1c6eu,w2);p32(0x7f1c74u,std::uint32_t(std::int32_t(std::int16_t(w6))));p8(0x7f1c71u,0);p8(0x7f1c70u,u8(r+8));break;
        case 10:setup_45ca20(w0,w2,std::uint32_t(std::int32_t(std::int16_t(w6))));break;
        case 11:setup_45d0a0(w0,w2);break;
        case 12:setup_45d040(w0,w2,w8,w6);break;
        case 13:setup_45d0f0(w0,w2,w6,w8);break;
        case 14:p16(0x7f8a68u,w0);p16(0x7f8a6au,w2);
            p8(0x7f8a6cu,count_45c8e0(std::uint32_t(std::int32_t(std::int16_t(w0))),w2,5));p8(0x7f8a6eu,0);p8(0x7f8a6du,0);break;
        default:break;
        }
        if(b!=15u)p32(0x7f19f8u,1);
    }
    // The evaluation of the running request (464350 table 464C5C / 464C9C by 7F2428).
    void request_eval(std::uint32_t q,std::uint32_t lvl){
        static constexpr std::uint8_t B[26]{0,15,1,15,15,15,2,3,4,5,15,6,15,15,7,8,9,10,11,12,13,14,1,4,3,9};
        switch(q<=0x19u?B[q]:15u){
        case 0:eval_462ff0(lvl);break;
        case 1:eval_4635c0(lvl);break;
        case 2:eval_4636f0(lvl);break;
        case 3:eval_463150(lvl);break;
        case 4:eval_463250(0x7f23e0u,lvl);break;
        case 5:eval_passes(lvl,0x7f23d2u,0x7f23d0u,0x7f23d4u,false);break;
        case 6:eval_passes(lvl,0x7f1cbau,0x7f1cb8u,0x7f1cbcu,true);break;
        case 7:eval_463840(lvl);break;
        case 8:eval_463df0(lvl);break;
        case 9:eval_463f00(0x7f23e0u,lvl);break;
        case 10:eval_464000(lvl);break;
        case 11:eval_464160(lvl);break;
        case 12:eval_464040(lvl);break;
        case 13:eval_4641d0(lvl);break;
        case 14:eval_4642c0(lvl);break;
        default:request_end_462f10(0,lvl,0);break;
        }
    }
    // The running request's update (464D20 table 4650E4 / 465128 by 7F2428).
    void request_update(){
        static constexpr std::uint8_t B[26]{0,1,2,16,16,16,3,4,5,6,16,7,16,16,8,9,10,11,12,13,14,15,2,5,4,10};
        const std::uint32_t q=u32(0x7f2428u);
        switch(q<=0x19u?B[q]:16u){
        case 0:request_45e610();break;
        case 1:break;
        case 2:request_45f3d0();break;
        case 3:request_45f540();break;
        case 4:request_45e8a0();break;
        case 5:request_45eeb0();break;
        case 6:request_45efb0();break;
        case 7:request_45f180();break;
        case 8:request_45f7f0();break;
        case 9:request_4617a0();break;
        case 10:request_461850();break;
        case 11:request_45cca0();break;
        case 12:request_462410();break;
        case 13:request_461940();break;
        case 14:request_4626e0();break;
        case 15:request_45d2a0();break;
        default:{
            const std::uint32_t pl=player();
            const std::uint32_t f4=u32(pl+4);
            if((f4&0x80000u)||(u8(pl+0x2f0)&1u)){
                static constexpr std::uint8_t T[14]{0,0,1,1,0,0,1,2,2,2,2,0,0,0};
                const std::uint32_t k=((u32(pl+0x2f0)>>2)&0x1fu)-1u;
                const std::uint32_t t=k<=0xdu?T[k]:2u;
                if(t==0u){hearts_lost_45afb0(0x32);voice_45bd30(0x150,0x3c,5);}
                else if(t==1u){
                    const std::uint32_t ref=u32(pl+0xdd4);
                    if(ref!=0x19au){
                        const float a=f32(pl+0x1c4),b=f32(work(ref)+0x1c4);
                        if((f4&0x4000000u)?gt(b,a):gt(a,b)){hearts_lost_45afb0(0x14);voice_45bd30(0x14c,0x3c,5);}
                    }
                }
            }
            const std::uint32_t ref=u32(pl+0xdd4);
            if(ref!=0x19au&&(u32(pl+4)&0x4000000u)&&gt(f32(work(ref)+0x1c4),f32(pl+0x1c4))){hearts_lost_45afb0(0xa);voice_45bd30(0x14b,0x3c,5);}
            const std::uint32_t cb=u32(0x7f2528u);
            if(cb==0x461940u)request_461940();
            else if(cb)throw std::logic_error("race traffic: Heart Attack callback [7F2528] not ported");
            break;}
        }
    }
    // 45F7F0 (request 14): the escort race: the ranking 7F89E8 of the player (8) among the race
    // cars of 7F89A8 by distance (+260), 7F8A56 rank (held for 7F8A5E frames), 7F8B00 rivals.
    void request_45f7f0(){
        const std::uint32_t pl=player();
        const std::uint32_t quiet=model_flag_45b380(i8(pl+0x11));
        events_45e2f0(0x5b01d8u);
        p16(0x7f8a58u,u16(0x7f8a56u));p32(0x7f8a40u,0);p32(0x7f89e8u,8);
        std::int16_t cnt=1;
        for(std::int32_t i=0;i<std::int32_t(i16(0x7f8a54u));++i){
            const std::uint32_t id=u32(0x7f89a8u+std::uint32_t(i)*4u);
            if((u8(0x79fb48u+id)&3u)!=2u){p32(0x7f8a40u,u32(0x7f8a40u)+1u);continue;}
            const std::uint32_t w=work(id);
            if(u32(w+0x324)!=6u)continue;
            if(u32(w+0x68)!=u32(pl+0x68)){p32(0x7f8a40u,u32(0x7f8a40u)+1u);continue;}
            p32(0x7f89e8u+std::uint32_t(std::int32_t(cnt))*4u,u32(w));++cnt;
        }
        const std::int32_t n=cnt;
        for(std::int32_t a=1;a<n;++a){
            const std::uint32_t pa=0x7f89e8u+std::uint32_t(a-1)*4u;
            for(std::int32_t b=a;b<n;++b){
                const std::uint32_t pb=0x7f89e8u+std::uint32_t(b)*4u;
                const std::uint32_t ea=u32(pa),eb=u32(pb);
                if(!(u16(work(ea)+0x260)>=u16(work(eb)+0x260))){p32(pb,ea);p32(pa,eb);}
            }
        }
        std::int32_t pos=0;
        if(n>0)while(u32(0x7f89e8u+std::uint32_t(pos)*4u)!=8u&&++pos<n){}
        std::int16_t bx=std::int16_t(i32(0x7f8a40u)+pos+1),bp=i16(0x7f8a58u);
        p16(0x7f8a56u,std::uint16_t(bx));
        if(i16(0x7f8a5eu)>0){
            if(bx>bp){
                const std::int32_t t=std::int32_t(bp)-i32(0x7f8a40u)-1;
                p16(0x7f8a56u,std::uint16_t(bp));
                for(std::int32_t k=n-1;k>t;--k)p32(0x7f89e8u+std::uint32_t(k)*4u,u32(0x7f89e8u+std::uint32_t(k-1)*4u));
                p32(0x7f89e8u+std::uint32_t(t)*4u,8);
                bx=i16(0x7f8a56u);bp=i16(0x7f8a58u);
            }
            p16(0x7f8a5eu,u16(0x7f8a5eu)-1u);
        }
        if(i16(0x7f8a5cu)<=0){
            p32(0x7f8a14u,n>0?u32(0x7f89e8u):0x19au);
            p32(0x7f8a18u,n>1?u32(0x7f89ecu):0x19au);
            p32(0x7f8a1cu,n>2?u32(0x7f89f0u):0x19au);
            p16(0x7f8a5cu,0x14);
        }else p16(0x7f8a5cu,u16(0x7f8a5cu)-1u);
        std::int32_t s=i32(0x7f8a40u);
        for(std::uint32_t k=0;k<3u;++k,++s){
            const std::uint32_t id=u32(0x7f8a14u+k*4u),o=0x7f8b00u+k*8u;
            p32(o,0);
            if(id==0x19au||s>=3)break;
            if(id!=8u){p32(o,work(id));p32(o+4,std::uint32_t(s));}
        }
        if(bx<bp){call(0x424940u,{0x5c});bx=i16(0x7f8a56u);bp=i16(0x7f8a58u);p16(0x7f8a5eu,0xf);}
        p32(0x7f8a44u,u32(0x5afc70u+std::uint32_t(std::int32_t(bx))*4u));
        p32(0x7f8afcu,u32(0x7f8a44u)-u32(0x7f24e8u));
        if(bx<bp){
            if(quiet||!rob_end_487df0(1))return;
            const float f=smul(float(std::int32_t(call(0x580f40u,{}))),K(0x62827cu));
            rob_anim_487d60(1,jb(f,K(0x628064u))?0x220003u:0x220000u);
        }else{
            if(quiet||!rob_end_487df0(1)||i16(0x7f8a56u)==1)return;
            rob_anim_487d60(1,0x220011);
        }
    }
    // 45D2A0 (request 21): objects 5 between 7F8A68..7F8A6A passed (7F8A6D) and hit (7F8A6E).
    void request_45d2a0(){
        const std::uint32_t pl=player();
        p8(0x7f8a6du,0);
        for(std::uint32_t k=0;k<0xfau;++k){
            if(!oso_live(k))continue;
            const std::uint32_t w=oso_work(k);
            if(u32(w+0x5c)!=5u)continue;
            const std::int32_t s=std::int32_t(section_43f370(w+0x40));
            if(s<std::int32_t(i16(0x7f8a68u))||s>std::int32_t(i16(0x7f8a6au)))continue;
            if(s<std::int32_t(i16(pl+0x64)))p8(0x7f8a6du,u8(0x7f8a6du)+1u);
            if(u32(w+0x3f8))p8(0x7f8a6eu,u8(0x7f8a6eu)+1u);
        }
        const float v=sdiv(float(std::int32_t(i8(0x7f8a6du))-std::int32_t(i8(0x7f8a6eu))),float(std::int32_t(i8(0x7f8a6cu))));
        p32(0x7f8afcu,std::uint32_t(cvtt(smul(v,K(0x5b0070u)))));
    }
    // 464D20: the Heart Attack step of NAVI_PUB control (variant 2, mode 16).
    void heart_control_464d20(){
        const std::uint32_t pl=player();
        const std::uint32_t lvl=call(0x44c940u,{u32(pl+0x68)});
        pf(0x7f1c38u,driving::x87_float(x(0x7f1c38u)+X87(K(0x5b0320u))));
        pf(0x7f1c38u,wrap_449470(f32(0x7f1c38u)));
        pf(0x7f1c40u,driving::x87_float(driving::x87_sin(x(0x7f1c38u))));
        if((u32(0x7d39f0u)>>1)&1u)course_objects_45db70();
        if(u32(pl+0x5c))p32(0x7f8b34u,u32(0x7f8b34u)+1u);
        request_update();
        const std::int32_t edi=i32(0x7f23c4u);
        std::int32_t esi=i32(0x7f1c80u);
        p32(0x7f1cacu,edi==esi?0u:(edi>esi?1u:0xffffffffu));
        const std::int32_t d=std::int32_t(std::uint32_t(edi)-std::uint32_t(esi))/32;
        if((d<0?-d:d)*32<20000)esi=edi;else esi+=d;
        p32(0x7f1c80u,std::uint32_t(esi));
        if(esi<0){p32(0x7f23c4u,0);p32(0x7f1c80u,0);p32(0x7f1cacu,0xffffffffu);}
        voice_pump_45bcd0();
        heart_meter_45fb80();
        effects_45b950();
        if(i32(0x7f8b30u)>0){if(i32(0x7f8b30u)%10==0)call(0x424940u,{0xd5});p32(0x7f8b30u,u32(0x7f8b30u)-1u);}
        requests_464350();
        banner_460ad0();
        if(u32(0x7f2428u)!=3u){
            const std::uint32_t c=u32(0x7f2520u)+1u;p32(0x7f2520u,c);
            const float s=f32(pl+0x1f8);
            const float acc=sadd(s,f32(0x7f251cu));pf(0x7f251cu,acc);
            pf(0x7f2518u,sdiv(acc,float(std::int32_t(c))));
            if(gt(f32(0x7f24e0u),s))pf(0x7f24e0u,s);
        }
        stats_4628d0();
        spawn_step_45d7a0();
        {
            const std::int32_t c=i32(0x7f2460u),e=i32(0x7f2464u);
            if(c!=e){
                const std::int32_t s=c>=0?1:-1;
                const std::int32_t v=e+s*3;
                p32(0x7f2464u,std::uint32_t(v));
                if((v<0?-v:v)>(c<0?-c:c))p32(0x7f2464u,std::uint32_t(c));
            }
        }
        for(std::int32_t i=0;i<std::int32_t(lvl);++i){
            const std::uint32_t a=0x7f2438u+std::uint32_t(i)*4u;
            const std::int32_t c=i32(a);
            if(!c||i32(a+0x14)==c)continue;
            const std::int32_t s=c>=0?1:-1;
            const std::int32_t v=i32(a+0x14)+s*3;
            p32(a+0x14,std::uint32_t(v));
            if((v<0?-v:v)>(c<0?-c:c))p32(a+0x14,std::uint32_t(c));
        }
    }

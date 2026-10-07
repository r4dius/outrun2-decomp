    // ==== Score control (race manager 4F2DF0 per frame, 4F2B20 stage bonus) ===================
    // The score block 84DEF8.. (4F2AC0) and the NAVI words 8428B8 / 8444C8 / 8446FC / 8447DC are
    // mapped by the HUD owner (native_race_hud_map_heart); 84BCF8 is the race end's.
    // 4B9770(car, other, points, kind): the car's score [8447DC + id * 4] += points (protected
    // 1039C1C: ecx = that word); kinds 1..4 / 6 queue a NAVI message in the first free one of the
    // ten 8444C8 slots (+C <= 0), with the passed car's model / colour for kinds 1..3.
    void score_4b9770(std::uint32_t car,std::uint32_t other,std::int32_t points,std::int32_t kind){
        const std::uint32_t id=u32(car);
        p32(0x8447dcu+id*4u,u32(0x8447dcu+id*4u)+std::uint32_t(points));
        if(kind<=0)return;
        if(kind>4&&kind!=6)return;
        std::uint32_t slot=0;
        while(i32(0x8444d4u+slot*0x24u)>0)if(++slot==10u)return;
        std::int32_t live=0,old=0;
        for(std::uint32_t k=0;k<10;++k){const std::int32_t v=i32(0x8444d4u+k*0x24u);if(v>0){++live;if(v>=0x8c)++old;}}
        const std::uint32_t r=0x8444c8u+slot*0x24u;
        p32(r,std::uint32_t(live));p32(r+0x10,std::uint32_t((live+1)*0x1a));p32(r+4,std::uint32_t(kind));p32(r+8,std::uint32_t(points));
        p32(r+0xc,std::uint32_t(old+0x8c));pf(r+0x14,0.0f);p32(r+0x18,1);
        if(other&&kind>=1&&kind<=3){
            p32(r+0x1c,std::uint32_t(std::int32_t(i8(other+0x11))));
            const std::uint8_t c=u8(other+0x12);p32(r+0x20,c>2u?0u:c);
            if(u32(other+0xc5c)&0x400000u)p32(r+0x18,kind==3?0xfu:0xdu);
            else if(i16(other+0xb72)>0)p32(r+0x18,kind==3?0xeu:0xcu);
            else p32(r+0x18,kind==3?0x11u:1u);
            return;
        }
        p32(r+0x1c,0);p32(r+0x20,0);
    }
    // 4B9980(car): the stage share 8428B8[(id - 8) * 15 + level (- 1 outside mode 0x13)] of the
    // car's score since the last call (8446FC).
    void score_stage_4b9980(std::uint32_t car){
        const std::uint32_t i=u32(car)-8u;
        std::uint32_t lvl=call(0x44c940u,{u32(car+0x68)});
        if(u32(0x78026cu)!=0x13u)--lvl;
        const std::uint32_t s=u32(0x8447fcu+i*4u);
        p32(0x8428b8u+(i*0xfu+lvl)*4u,s-u32(0x8446fcu));
        p32(0x8446fcu,s);
    }
    // 4F2660 (rivals, ids 8..) / 4F2860 (traffic, ids after them): a car the player has just got
    // ahead of (course gap, then the car-to-player direction against the player's heading)
    // counts once (+C5C 0x40) in 84DF0C and scores through 4B9770.
    void pass_cars(bool traffic){
        if((u8(0x79fb50u)&3u)!=2u)return;
        const std::uint32_t pl=player();
        p32(pl+0xc,u32(pl+0xc)&~3u);
        if(i32(traffic?0x680ad8u:0x680ad4u)<=0)return;
        Frame L(*this,0x20);
        for(std::int32_t n=0;;){
            const std::uint32_t id=std::uint32_t(n)+(traffic?u32(0x680ad4u):0u)+8u;
            if((u8(0x79fb48u+id)&3u)==2u){
                const std::uint32_t car=u32(0x799b38u+id*0x3cu);
                std::uint32_t f=u32(car+0xc5c);
                if(!(f&0x40u)){
                    const std::uint32_t pf4=u32(pl+4);
                    const bool w=((pf4>>26)&1u)!=0u,hit=(pf4&0x10000u)&&u32(pl+0xdd4)==id;
                    if(w){if(hit){f|=0x800000u;p32(car+0xc5c,f);}}
                    else if(hit){f|=0x400000u;p32(car+0xc5c,f);}
                    if(std::int32_t(u16(pl+0x260))-std::int32_t(u16(car+0x260))>=-2){
                        bool ok=true;
                        sub_40efa0(L(0x10),car+0x14,pl+0x14);
                        pf(L(0),f32(pl+0x20));pf(L(4),f32(pl+0x24));pf(L(8),f32(pl+0x28));
                        normalize_40ef00(L(0x10));normalize_40ef00(L(0));
                        if(u32(car+0x324)==5u){
                            if(std::int32_t(u16(pl+0x262))-std::int32_t(u16(car+0x262))>=2)ok=false;
                            else if(std::int32_t(u16(pl+0x260))-std::int32_t(u16(car+0x260))<2)ok=false;
                        }
                        const X87 d=dot_40efd0(L(0x10),L(0));
                        if(!(d>X87(0.0f))&&ok){
                            std::uint32_t c=u32(pl+0xc)|1u;p32(pl+0xc,c);
                            p32(pl+0xc,((u32(car+0x324)<<4)^c)&0xf0u^c);
                            p32(0x84df0cu,u32(0x84df0cu)+1u);
                            f=u32(car+0xc5c)|0x40u;p32(car+0xc5c,f);
                            if(!(f&0x800000u)){
                                if(!traffic||u32(car+0x324)==5u){
                                    std::int32_t pts;
                                    if(!traffic)pts=(f&0x400000u)?0x186a:(i16(car+0xb72)>0?0x61a8:0x30d4);
                                    else pts=(f&0x400000u)?0x30d4:(i16(car+0xb72)>0?0xc350:0x61a8);
                                    score_4b9770(pl,car,pts,3);
                                }else{
                                    std::int32_t kind=1,pts=0x2ee;
                                    if(std::int32_t(u32(car+0x324))<3){kind=2;pts=0x1d4c;}
                                    if(f&0x400000u)pts>>=1;
                                    else if(i16(car+0xb72)>0)pts<<=1;
                                    score_4b9770(pl,car,pts,kind);
                                }
                            }
                        }
                    }
                }
            }
            ++n;
            if(!(std::int32_t(std::int16_t(n))<i32(traffic?0x680ad8u:0x680ad4u)))break;
        }
    }
    // 4F25E0(id) (network variants 3 / 4): the car's rank change flags +4 0x1000 / 0x2000.
    void rank_flags_4f25e0(std::uint32_t id){
        const std::uint32_t car=u32(0x799b38u+id*0x3cu);
        p32(car+4,u32(car+4)&0xffffcfffu);
        if(std::int16_t(call(0x49b2d0u,{}))>0x3c){
            const std::uint8_t r=std::uint8_t(call(0x45a2b0u,{u8(car+0x10)}));
            p8(0x84df10u,r);p8(0x84df38u,r);return;
        }
        const std::uint8_t was=u8(0x84df10u);p8(0x84df38u,was);
        const std::uint8_t r=std::uint8_t(call(0x45a2b0u,{u8(car+0x10)}));
        p8(0x84df10u,r);
        if(was>r)p32(car+4,u32(car+4)|0x1000u);
        else if(was<r)p32(car+4,u32(car+4)|0x2000u);
    }
    // 4F2CD0 (mode 16): the stage driving statistics 84DF1C..84DF34 (reset by 450130 / car +4 8).
    void stats_4f2cd0(){
        if(u32(0x78026cu)!=0x10u)return;
        const std::uint32_t pl=player();
        std::uint32_t drift;
        if(((u32(0x7d39f0u)>>1)&1u)||(u8(pl+4)&8u)){   // 450130 (protected 4484A7: [7D39F0])
            drift=0;for(std::uint32_t a=0x84df1cu;a<=0x84df34u;a+=4)p32(a,0);
        }else drift=u32(0x84df34u);
        if(!(u8(pl+0x244)&2u))p32(0x84df28u,u32(0x84df28u)+1u);
        if((u32(pl+4)&0x80000u)||(u8(pl+0x2f0)&1u)){
            const std::uint32_t k=((u32(pl+0x2f0)>>2)&0x1fu)-1u;
            static constexpr std::uint8_t B[13]{0,1,2,2,1,1,2,2,2,2,2,1,1};
            if(k<=0xcu){if(B[k]==0)p32(0x84df1cu,u32(0x84df1cu)+1u);else if(B[k]==1)p32(0x84df20u,u32(0x84df20u)+1u);}
        }
        if(u8(pl+6)&1u)p32(0x84df2cu,u32(0x84df2cu)+1u);
        if(u32(pl+4)&0x20000u)p32(0x84df24u,u32(0x84df24u)+1u);
        if(u32(pl+4)&0x400000u)p32(0x84df30u,u32(0x84df30u)+1u);
        if(f32(pl+0xe68)>K(0x628304u)&&std::abs(std::int32_t(i16(pl+0x162)))<0x5dc)p32(0x84df34u,drift+1u);
    }
    // 4F2B20(seconds): the stage bonus (time under 60 s, clean-driving statistics, drift frames,
    // variant 9 grade), stats 4EF460 / 4EF440 offline, then 4B9770 kind 4 and 4B9980.
    void stage_bonus_4f2b20(std::int32_t t){
        const std::uint32_t pl=player();
        std::int32_t s=t<0x3c?(0x3c-t)*1000:0;
        if(!u32(0x84df1cu))s+=0x9c4;
        if(!u32(0x84df20u))s+=0x9c4;
        if(!u32(0x84df24u))s+=0x1388;
        if(!u32(0x84df28u))s+=0x2710;
        if(!u32(0x84df2cu))s+=0x9c4;
        if(!u32(0x84df30u))s+=0x2710;
        if(const std::uint32_t d=u32(0x84df34u))s+=std::int32_t(d*1000u)/60;
        if(!call(0x55a930u,{},0,0x7f9460u)){
            // 4EF460(clean flags) and 4EF440(seconds left): the miles score 84BCF8 (+0x19 / +0xA /
            // +0xA / +0x19 / +0xF / +0xF per clean statistic, then 40 per second under 60).
            static constexpr std::uint32_t Add[6]{0x19,0xa,0xa,0x19,0xf,0xf};
            const std::uint32_t clean[6]{0x84df28u,0x84df1cu,0x84df20u,0x84df24u,0x84df2cu,0x84df30u};
            for(unsigned k=0;k<6;++k)if(!u32(clean[k]))p32(0x84bcf8u,u32(0x84bcf8u)+Add[k]);
            const std::int32_t left=std::int32_t(0x3cu-std::uint32_t(t));
            p32(0x84bcf8u,u32(0x84bcf8u)+(left<0?0u:std::uint32_t(left))*40u);
        }
        if(u32(0x780258u)==9u){
            static constexpr std::int32_t G[8]{0x186a0,0xc350,0x61a8,0x30d4,0x30d4,0x30d4,0x30d4,0x2ee};
            const std::uint32_t g=u32(0x68625cu);   // 4B00F0
            if(g<=7u)s+=G[g];
        }
        if(!(call(0x4957f0u,{})&0xffu))score_4b9770(pl,0,s-s%10+10,4);
        score_stage_4b9980(pl);
    }
    // 4F2DF0: per-frame score control outside a mission selection.
    void score_4f2df0(){
        if(call(0x4957f0u,{})&0xffu)return;
        if(call(0x4b00d0u,{})&0xffu)pass_cars(false);
        if((u8(0x79fb50u)&3u)==2u){const std::uint32_t v=u32(0x780258u);if(v==3u||v==4u)rank_flags_4f25e0(8);}
        pass_cars(true);
        (void)call(0x44fe40u,{});
        std::uint32_t md=u32(0x78026cu);
        if(md==0x13u){
            if(!u32(0x84df04u)){
                p32(0x84df04u,1);
                stage_bonus_4f2b20(cvtt(smul(float(std::int32_t(call(0x450570u,{}))),K(0x62812cu))));
                md=u32(0x78026cu);
                if(md!=0x13u&&md==0x1bu&&!u32(0x84df04u))p32(0x84df04u,1);
            }
        }else if(md==0x1bu&&!u32(0x84df04u))p32(0x84df04u,1);
        stats_4f2cd0();
        if((u8(0x79fb50u)&3u)!=2u)return;
        if(call(0x44ff10u,{}))return;
        if(u32(0x78026cu)!=0x10u)return;
        const std::uint32_t pl=u32(0x799d18u);
        const std::int32_t gap=std::abs(std::int32_t(i16(pl+0x64))-std::int32_t(i16(pl+0x18c)));
        float mul=1.0f;
        if(u32(0x780258u)==9u){
            if(call(0x4b00e0u,{})&0xffu){
                const std::uint8_t r=u8(pl+0xc36);
                if(r==0)mul=K(0x6280c0u);else if(r==1)mul=K(0x6282a4u);else if(r==2)mul=K(0x6280b0u);
            }else mul=K(0x628064u);
        }
        if(u32(pl+0x244)&0x9cu)return;
        const float g=float(std::int32_t(std::int16_t(gap)));
        const bool drifting=f32(pl+0xe68)>K(0x628304u)&&std::abs(std::int32_t(i16(pl+0x162)))<0x5dc&&f32(pl+0x1c4)>f32(pl+0xe64);
        score_4b9770(pl,0,cvtt(smul(smul(g,K(drifting?0x5c68e0u:0x628244u)),mul)),0);
    }

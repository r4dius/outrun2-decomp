    // ==== Heart Attack request evaluation (the 464350 request manager's end actions) =========
    std::int32_t score_hearts(){const std::int32_t s=i32(0x7f8afcu);return (s<0?0:s)/10000;}
    std::uint32_t score_clamped(){const std::int32_t s=i32(0x7f8afcu);return std::uint32_t(s<0?0:s);}
    // 45C990(eax hearts): the request grade 1..7, capped by the penalties of the request.
    std::int16_t grade_45c990(std::int32_t h){
        std::int32_t r;
        if(h<0xc)r=1;else if(h<0x14)r=2;else if(h<0x1e)r=3;else if(h<0x2d)r=4;else if(h<0x3c)r=5;else r=h>=0x50?7:6;
        std::int32_t c=u32(0x7f24f4u)?5:7;
        if(u32(0x7f24f8u))c-=3;
        if(u32(0x7f24fcu))c-=4;
        std::int16_t g=std::int16_t(r);
        if(g>std::int16_t(c))g=std::int16_t(c);
        if(g<0)g=0;
        return g;
    }
    // 4505F0(level): the level time factor [7D3838 + level*4].
    float level_rate_4505f0(std::uint32_t level){return f32(0x7d3838u+level*4u);}
    std::int32_t meter_a(std::int32_t e){return ((e+e/2)*10)/7;}
    std::int32_t meter_b(std::int32_t e){return (e*20)/7;}
    void result_close(std::int16_t res,std::uint32_t edi,std::uint32_t level,std::int32_t meter){
        const std::uint32_t h=result_hearts_45dce0(edi,res);
        p32(0x7f8b30u,std::uint32_t(meter));p32(0x7f8b28u,0x78);
        request_end_462f10(res,level,std::uint32_t(std::int32_t(h)/10000));
    }
    // A finish-speed bonus: 1 below [628244] km/h lost, scaled down to [628100].
    float speed_keep(float d){
        if(gt(K(0x628244u),d))return K(0x62806cu);
        if(!gt(K(0x628100u),d))return 0.0f;
        float v=smul(ssub(K(0x628100u),d),K(0x628128u));
        if(gt(v,K(0x62806cu)))v=K(0x62806cu);
        return v;
    }
    // The 463150 / 463F00 ladder: 700000 when the speed held fully, else scaled.
    std::uint32_t speed_bonus(float d){
        if(gt(K(0x628244u),d))return 0xaae60u;
        if(!gt(K(0x628100u),d))return std::uint32_t(cvtt(smul(0.0f,K(0x5b0310u))));
        const float v=smul(ssub(K(0x628100u),d),K(0x628128u));
        if(gt(v,K(0x62806cu))||v==K(0x62806cu))return 0xaae60u;
        return std::uint32_t(cvtt(smul(v,K(0x5b0310u))));
    }
    // 462FF0(level): request 0 (boost) result.
    void eval_462ff0(std::uint32_t level){
        const std::uint32_t pl=player();
        std::uint32_t edi=score_clamped();
        const std::int32_t esi=std::int32_t(edi)/10000;
        const float q=sdiv(float(i32(0x7f8ab0u)),float(std::int32_t(std::int16_t(std::uint16_t(u16(0x7f8ab6u)-u16(0x7f8ab4u))))));
        const std::int16_t res=grade_45c990(esi);
        if(res==7){
            std::uint32_t v=0;
            if(!jb(q,K(0x6280f0u)))v=std::uint32_t(cvtt(smul(smul(ssub(q,K(0x6280f0u)),K(0x628244u)),K(0x5b030cu))));
            const float keep=speed_keep(ssub(f32(0x7f2514u),f32(pl+0x1f8)));
            edi=v-std::uint32_t(cvtt(smul(keep,K(0x5b0308u))))+800000u;
        }
        result_close(res,edi,level,meter_a(esi));
    }
    // 463150(level): request 6 (distance) result.
    void eval_463150(std::uint32_t level){
        const std::uint32_t pl=player();
        std::uint32_t edi=score_clamped();
        const std::int32_t esi=std::int32_t(edi)/10000;
        const std::int16_t res=grade_45c990(esi);
        if(res==7)edi+=speed_bonus(ssub(f32(0x7f2514u),f32(pl+0x1f8)));
        result_close(res,edi,level,meter_b(esi));
    }
    // 463250(record, level): requests 8 / 23 (slide) result.
    void eval_463250(std::uint32_t rec,std::uint32_t level){
        const std::uint32_t pl=player();
        std::int32_t esi=score_hearts();
        std::uint32_t ebp=0;
        const std::int16_t res=grade_45c990(esi);
        if(res==7){
            float v=smul(sdiv(float(i32(0x7f2540u)),float(std::int32_t(i16(0x7f2532u))-std::int32_t(i16(0x7f2530u)))),K(0x628244u));
            if(gt(v,K(0x62806cu)))v=K(0x62806cu);
            esi=0x50;
            const float d=ssub(f32(rec+0x134),f32(pl+0x1f8));
            ebp=std::uint32_t(cvtt(smul(v,K(0x5b030cu))));
            ebp+=std::uint32_t(cvtt(smul(speed_keep(d),K(0x5b0314u))));
        }
        result_close(res,std::uint32_t(esi)*10000u+ebp,level,meter_a(esi));
    }
    // 4633A0(level) request 9 / 4634B0(level) request 11: counted passes (7F23D0.. / 7F1CB8..).
    void eval_passes(std::uint32_t level,std::uint32_t got,std::uint32_t need,std::uint32_t full,bool add){
        std::int32_t esi=score_hearts();
        std::uint32_t ebp=0;
        const std::int16_t res=grade_45c990(esi);
        if(res==7){
            esi=0x50;
            if(!u32(0x7f2504u)&&!u32(0x7f24ecu))ebp=400000u;
            const std::int16_t cx=i16(got),dx=i16(need);
            if(cx>dx){
                const std::int16_t ax=i16(full);
                if(cx>=ax)ebp+=std::uint32_t((std::int32_t(cx)-std::int32_t(ax)+6)*50000);
                else{
                    const std::uint32_t v=std::uint32_t(cvtt(smul(sdiv(float(std::int32_t(cx)-std::int32_t(ax)),float(std::int32_t(ax)-std::int32_t(dx))),K(0x5b030cu))));
                    ebp=add?ebp+v:v;
                }
            }
        }
        result_close(res,std::uint32_t(esi)*10000u+ebp,level,meter_a(esi));
    }
    // 4635C0(level): requests 2 / 22 (overtakes) result.
    void eval_4635c0(std::uint32_t level){
        std::int32_t esi=score_hearts();
        std::int32_t edi=std::int32_t(u32(0x84df0cu)-u32(0x7f2988u)+u32(0x84df00u));
        std::int32_t ebp=0;
        const std::int16_t res=grade_45c990(esi);
        if(res==7){
            esi=0x50;
            if(!u32(0x7f2500u)&&!u32(0x7f2508u))ebp=0x19;
            if(!u32(0x7f2504u)&&!u32(0x7f24f4u))ebp+=0x19;
            const std::int32_t need=i16(0x7f1c52u);
            if(edi>=need){edi=edi-need+4;ebp+=edi*5;edi=ebp+0x50;}
            else{const std::int32_t base=i16(0x7f1c50u);
                ebp+=cvtt(sdiv(smul(float(edi-base),K(0x6282d4u)),float(need-base)));edi=ebp+0x50;}
        }else edi=esi;
        result_close(res,std::uint32_t(edi)*10000u,level,meter_a(esi));
    }
    // 4636F0(level): request 14 (escort race) result.
    void eval_4636f0(std::uint32_t level){
        const std::uint32_t sc=score_clamped();
        std::int32_t esi=std::int32_t(sc)/10000;
        std::uint32_t edi=sc;
        const std::int16_t res=grade_45c990(esi);
        if(res==7){
            std::uint32_t ebp=0;
            if(!u32(0x7f2504u)&&!u32(0x7f2500u)&&!u32(0x7f2508u))ebp=400000u;
            const float keep=speed_keep(ssub(f32(0x7f2514u),f32(0x7f2518u)));
            edi=ebp-std::uint32_t(cvtt(smul(keep,K(0x5b0318u))))+800000u;
        }else{
            const std::int32_t cap=i32(0x5b0038u+std::uint32_t(std::int32_t(res))*4u);
            if(esi>cap){esi=cap;edi=std::uint32_t(cap)*10000u;}
        }
        result_close(res,edi,level,meter_a(esi));
    }
    // 463840(level): request 17 (gates) result.
    void eval_463840(std::uint32_t level){
        std::int32_t esi=score_hearts();
        std::uint32_t eax=0;
        const std::int16_t res=grade_45c990(esi);
        if(res==7){
            esi=0x50;
            if(!u32(0x7f24ecu))eax=200000u;
            const float k=K(0x5afc4cu);
            const std::int32_t a=i32(0x7f2504u);
            if(a<0x3c)eax+=std::uint32_t(cvtt(smul(smul(float(0x3c-a),k),K(0x5b031cu))));
            const std::int32_t b=i32(0x7f2500u);
            if(b<0x3c)eax+=std::uint32_t(cvtt(smul(smul(float(0x3c-b),k),K(0x5b030cu))));
        }
        result_close(res,std::uint32_t(esi)*10000u+eax,level,meter_a(esi));
    }
    // 463DF0(level): request 15 (objects) result.
    void eval_463df0(std::uint32_t level){
        std::int32_t esi=score_hearts();
        std::uint32_t edi=0;
        const std::int16_t res=grade_45c990(esi);
        if(esi>0x50)esi=0x50;
        const std::uint32_t base=std::uint32_t(esi)*10000u;
        if(res==7){
            const std::int8_t cl=i8(0x7f1ca3u),dl=i8(0x7f1ca2u);
            const std::int32_t ebp=std::int32_t(i8(0x7f1ca1u))+std::int32_t(cl);
            if(cl==dl)edi=0xaae60u;
            else edi=std::uint32_t(cvtt(smul(sdiv(float(std::int32_t(cl)),float(std::int32_t(dl))),K(0x5b0310u))));
            const std::int32_t c=i8(0x7f1ca0u);
            if(ebp>c)edi+=std::uint32_t(cvtt(smul(float(ebp-c),K(0x628064u))))*10000u;
        }
        result_close(res,edi+base,level,meter_b(esi));
    }
    // 463F00(record, level): request 16 / 25 result.
    void eval_463f00(std::uint32_t rec,std::uint32_t level){
        const std::uint32_t pl=player();
        std::uint32_t edi=score_clamped();
        const std::int32_t esi=std::int32_t(edi)/10000;
        const std::int16_t res=grade_45c990(esi);
        if(res==7)edi+=speed_bonus(ssub(f32(rec+0x134),f32(pl+0x1f8)));
        result_close(res,edi,level,meter_b(esi));
    }
    // 464000(level): a failed request.
    void eval_464000(std::uint32_t level){
        const std::uint32_t h=result_hearts_45dce0(0,0);
        p32(0x7f8b28u,0x78);p32(0x7f8b30u,0);
        request_end_462f10(0,level,std::uint32_t(std::int32_t(h)/10000));
    }
    // 464040(level): request 19 (crates) result.
    void eval_464040(std::uint32_t level){
        std::uint32_t edi=score_clamped();
        std::int32_t esi=std::int32_t(edi)/10000;
        const std::int16_t res=grade_45c990(esi);
        if(res==7){
            const std::int16_t dx=i16(0x7f8ae0u),cx=i16(0x7f8ae2u);
            esi=0x50;
            if(cx>=dx)edi=std::uint32_t((std::int32_t(cx)-std::int32_t(dx))*10000+0x46);
            else{const std::int32_t b=i16(0x7f8adeu);
                edi=std::uint32_t(cvtt(smul(sdiv(K(0x6280f4u),float(std::int32_t(dx)-b)),float(std::int32_t(cx)-b))));}
            edi=edi-std::uint32_t(cvtt(smul(float(std::int32_t(i8(0x7f8ae4u))*10000),K(0x599440u))))+800000u;
        }else{
            const std::int32_t cap=i32(0x5b0038u+std::uint32_t(std::int32_t(res))*4u);
            if(esi>cap){esi=cap;edi=std::uint32_t(esi)*10000u;}
        }
        result_close(res,edi,level,meter_b(esi));
    }
    // 4641D0(level): request 20 result.
    void eval_4641d0(std::uint32_t level){
        std::int32_t esi=score_hearts();
        std::uint32_t eax=0;
        const std::int16_t res=grade_45c990(esi);
        if(res==7){
            const std::int16_t cx=i16(0x7f198cu),di=i16(0x7f198eu);
            esi=0x50;
            if(cx>di){
                const std::int16_t dx=i16(0x7f1990u);
                if(cx>=dx)eax=std::uint32_t((std::int32_t(cx)-std::int32_t(dx)+0xe)*50000);
                else eax=std::uint32_t(cvtt(sdiv(smul(float(std::int32_t(cx)-std::int32_t(di)),K(0x5b0310u)),float(std::int32_t(dx)-std::int32_t(di)))));
            }
        }
        result_close(res,std::uint32_t(esi)*10000u+eax,level,meter_b(esi));
    }
    // 464160(level): result = hearts / 8, no bonus.
    void eval_464160(std::uint32_t level){
        p32(0x7f8b28u,0x78);
        const std::int32_t c=score_hearts();
        p32(0x7f8b30u,std::uint32_t(meter_b(c)));
        request_end_462f10(std::int16_t(std::int32_t(std::int16_t(c))/8),level,0);
    }
    // 4642C0(level): result = hearts / 8 with the 45DCE0 hearts.
    void eval_4642c0(std::uint32_t level){
        const std::int32_t edi=score_hearts();
        const std::int32_t esi=std::int32_t(std::int16_t(edi))/8;
        const std::uint32_t h=result_hearts_45dce0(std::uint32_t(edi),std::int16_t(esi));
        p32(0x7f8b30u,std::uint32_t(meter_b(edi)));p32(0x7f8b28u,0x78);
        request_end_462f10(std::int16_t(esi),level,std::uint32_t(std::int32_t(h)/10000));
    }
    // 45CA70 (request 17 callback 7F2524): the gate markers on screen (429780 frame counts,
    // 449940 projection, 4289E0 draw of the player marker 2C0004).
    void gates_draw_45ca70(){
        const std::uint32_t pl=player(),cam=u32(0x79f574u);
        Frame f(*this,0x80);
        const std::uint32_t L0=f(0),L20x=f(0x20),Lp=f(0x24),M=f(0x30);
        push_load(cam+0x140);
        std::uint32_t tok=pl;float scale=f32(f(0x10));
        for(std::int32_t i=0;i<i32(0x7f8a74u)/2;++i){
            const std::uint32_t e=0x7f1cf0u+std::uint32_t(i)*0x2cu,id=0x134u+std::uint32_t(i);
            const std::uint32_t st=u32(e);
            if(st&1u)continue;
            if((u8(0x79fb48u+id)&3u)!=2u)continue;
            const std::uint32_t w=work(id);
            if(!(u32(w+0x54)&1u)&&st==0u)continue;
            std::int32_t frame=cvtt(f32(w+0x74));
            std::uint32_t loop=0;
            if(st==0u){p32(Lp,u32(w+0x40));p32(Lp+4,u32(w+0x44));p32(Lp+8,u32(w+0x48));tok=0x2c0003u;loop=1;scale=K(0x5b0058u);}
            else if(st&2u){p32(Lp,u32(pl+0x14));p32(Lp+4,u32(pl+0x18));p32(Lp+8,u32(pl+0x1c));
                pf(Lp+4,sadd(f32(Lp+4),K(0x5b005cu)));tok=0x2c0004u;scale=K(0x5b0058u);}
            call(0x429780u,{tok,L20x,L0});
            if(!loop&&frame==i32(L0))continue;
            (void)project_449940(f32(cam+0xb0),f32(cam+0xb4),Lp,Lp);
            float s;
            if(X87(K(0x6281f0u))>X87(std::fabs(f32(Lp+8))))s=0.0f;
            else s=smul(sdiv(smul(f32(cam+0xb0),scale),ssub(0.0f,f32(Lp+8))),K(0x6282d8u));
            translation_4393a6(M,f32(Lp),ssub(0.0f,f32(Lp+4)),0.0f);
            if(tok==0x2c0004u)call(0x4289e0u,{tok,5,M,std::uint32_t(frame),fbits(s)});
            ++frame;
            if(loop){if(frame>=i32(L0))frame=0;}
            else if(frame>=i32(L0))frame=i32(L0);
            pf(w+0x74,float(frame));
        }
        pop();
    }

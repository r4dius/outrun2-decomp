    // ==== Heart Attack request set-up (the 464350 request manager's start actions) ============
    // 43F370(point): section word of the course polygon under the point (43EB60 mode 0x100 through
    // the protected snippet 103BE05, polygon written over the argument slot); 0 off the course.
    std::uint32_t section_43f370(std::uint32_t p){
        Frame f(*this,0x10);
        p32(f(0),u32(p));p32(f(4),u32(p+4));p32(f(8),u32(p+8));p32(f(0xc),p);
        const std::uint32_t kind=call(0x43eb60u,{0x100,f(0),f(0xc),0,0});
        const std::uint32_t poly=u32(f(0xc));
        if(poly==0xffffffffu)return 0;
        return u16(u32(0x780228u+kind*4u)+poly*2u);
    }
    // 45C8E0(lo, hi, kind): live objects of a kind (crates 12 measured at +A4) in sections lo..hi.
    std::uint8_t count_45c8e0(std::uint32_t lo,std::uint32_t hi,std::uint32_t kind){
        std::uint8_t n=0;
        for(std::uint32_t k=0;k<0xfau;++k){
            if(!oso_live(k))continue;
            const std::uint32_t w=oso_work(k);
            if(u32(w+0x5c)!=kind)continue;
            const std::uint32_t s=section_43f370(kind==0x12u?w+0xa4:w+0x40);
            if(std::int32_t(s)<std::int32_t(std::int16_t(lo))||std::int32_t(s)>std::int32_t(std::int16_t(hi)))continue;
            ++n;
        }
        return n;
    }
    // 4EF870(v): [84CC18] = v.
    void appear_override_4ef870(std::uint32_t v){p32(0x84cc18u,v);}
    // 4F0BD0(id, function, data): open gate event id (440110) with its 0x28-byte record in 84CEA0.
    void gate_open_4f0bd0(std::uint32_t id,std::uint32_t fn,std::uint32_t data){
        call(0x440110u,{id,fn});
        const std::uint32_t r=0x84cea0u+(id-0x134u)*0x34u;
        p32(r+0x28,1);
        for(std::uint32_t k=0;k<0x28u;k+=4)p32(r+k,u32(data+k));
        p32(r+0x2c,fn);p32(r+0x30,u32(0x84d8f0u));
    }
    void reset_result(){p32(0x7f1978u,0);p32(0x7f8afcu,0);p32(0x7f1c3cu,0);}
    // 45E5B0(cx, dx) request 0 set-up.
    void setup_45e5b0(std::uint16_t a,std::uint16_t b){
        p32(0x7f8a90u,1);for(std::uint32_t o:{0x7f8a94u,0x7f8a98u,0x7f8a9cu,0x7f8aa0u,0x7f8aa4u,0x7f8aa8u})p32(o,0);
        p16(0x7f8ab4u,a);p16(0x7f8ab6u,b);p32(0x7f8ab0u,0);p16(0x7f8aacu,0);reset_result();
    }
    // 45F310(a, b, c, d) requests 2 / 22: rank goal; 22 also takes the 4F0030 appear record.
    void setup_45f310(std::uint16_t a,std::uint16_t b,std::uint16_t c,std::uint16_t d){
        const std::uint32_t pl=player();
        const std::uint32_t rk=u32(0x84df0cu)+u32(0x84df00u);
        p16(0x7f1c48u,a);p16(0x7f1c50u,c);p16(0x7f1c4au,b);
        p32(0x7f2988u,rk);p32(0x7f8b38u,rk);reset_result();p32(0x7f1c4cu,0);p16(0x7f1c52u,d);
        if(u32(0x7f2428u)==0x16u)appear_record_copy(call(0x4f0030u,{pl+0x5c,0}),0x12c012cu);
    }
    // Copy the 0x46-byte appear record to 7F23E0, force its four model weights, install it (4EF870).
    void appear_record_copy(std::uint32_t src,std::uint32_t w){
        for(std::uint32_t k=0;k<0x44u;k+=4)p32(0x7f23e0u+k,u32(src+k));
        p16(0x7f2424u,u16(src+0x44));
        p32(0x7f23eeu,w);p32(0x7f23f2u,w);p32(0x7f23f6u,w);p16(0x7f23fau,std::uint16_t(w));
        appear_override_4ef870(0x7f23e0u);
    }
    // 45B300(cx, dx; a, b) request 9 set-up (pairs to drive between).
    void setup_45b300(std::uint16_t cx,std::uint16_t dx,std::uint16_t a,std::uint16_t b){
        const std::uint32_t pl=player();
        p16(0x7f23ceu,dx);p16(0x7f23ccu,cx);p16(0x7f23d0u,a);
        p32(0x7f23d8u,0);p16(0x7f23d4u,b);p16(0x7f23d2u,0);
        appear_record_copy(call(0x4f0030u,{pl+0x5c,0}),0x1180118u);
    }
    // 46EF10: free the 46DB60 marker ring.
    void ring_clear_46ef10(){
        for(std::uint32_t r=0x8037c8u;r<0x804348u;r+=0x5cu)p32(r,0x19au);
        p32(0x804354u,0);p32(0x8037b4u,0);
    }
    // 45B520: the race cars (+324 == 6) follow the player's speed (capped at [5AFD28]).
    void escort_speed_45b520(){
        const std::uint32_t pl=player();
        p32(0x7f8a50u,1);p32(0x7f250cu,1);
        for(std::uint32_t i=0;i<0x18u;++i){
            if((u8(0x79fb51u+i)&3u)!=2u)continue;
            const std::uint32_t w=u32(0x799d54u+i*0x3cu);
            if(u32(w+0x324)!=6u)continue;
            const float v=f32(pl+0x1c4),cap=K(0x5afd28u);
            pf(w+0x1c4,v);if(gt(cap,v))pf(w+0x1c4,cap);
            p32(w+0x1f4,ftol(x(w+0x1c4)*X87(K(0x5a460cu))));p32(w+0xb58,0);
        }
    }
    // 45F5D0(cx, dx) request 14 set-up: the race cars (+324 == 6) of events 9..32 in 7F89A8.
    void setup_45f5d0(std::uint16_t a,std::uint16_t b){
        const std::uint32_t pl=player();
        p16(0x7f8a4cu,a);p16(0x7f8a4eu,b);p16(0x7f8a48u,0);p16(0x7f8a4au,u16(pl+0x64));p16(0x7f8a54u,0);
        for(std::uint32_t id=9;id<0x21u;++id){
            if((u8(0x79fb48u+id)&3u)!=2u)continue;
            if(u32(work(id)+0x324)!=6u)continue;
            p32(0x7f89a8u+std::uint32_t(std::int32_t(i16(0x7f8a54u)))*4u,id);
            p16(0x7f8a54u,u16(0x7f8a54u)+1u);
        }
        const std::uint16_t n=std::uint16_t(u16(0x7f8a54u)+1u);
        p16(0x7f8a58u,n);p16(0x7f8a56u,n);p16(0x7f8a5au,0);
        p16(0x7f8a60u,u8(pl+0x32a));p16(0x7f8a62u,n);p16(0x7f8a5cu,0);p16(0x7f8a5eu,0);
        escort_speed_45b520();
        reset_result();
    }
    // 45C950(cx, ax; count) request 15 set-up.
    void setup_45c950(std::uint16_t cx,std::uint16_t ax,std::uint8_t count){
        p16(0x7f1c9cu,cx);p16(0x7f1c9eu,ax);p8(0x7f1ca1u,0);p8(0x7f1ca3u,0);
        p8(0x7f1ca2u,count_45c8e0(cx,ax,0xa));
        p8(0x7f1ca0u,count);
    }
    // 45CA20(ax, cx, edx) request 17 set-up (also 462E20 for kind 0x11).
    void setup_45ca20(std::uint16_t ax,std::uint16_t cx,std::uint32_t edx){
        const std::uint32_t pl=player();
        p16(0x7f8a78u,ax);p16(0x7f8a7au,cx);p32(0x7f8a80u,edx);p8(0x7f8a7cu,0);
        p32(0x7f8a84u,u32(pl+0x14));p32(0x7f8a88u,u32(pl+0x18));pf(0x7f8b44u,0.0f);p32(0x7f8a8cu,u32(pl+0x1c));
    }
    // 45D040(cx, ax, dx; n) request 19 set-up.
    void setup_45d040(std::uint16_t cx,std::uint16_t ax,std::uint16_t dx,std::uint16_t n){
        p16(0x7f8ae0u,dx);p16(0x7f8ae2u,0);p32(0x7f8af0u,0);
        p16(0x7f8ad8u,cx);p16(0x7f8adau,ax);p32(0x7f8af4u,0xffffffffu);p32(0x7f8af8u,1);p16(0x7f8adeu,n);
        p16(0x7f8adcu,std::uint16_t(std::int16_t(std::int8_t(count_45c8e0(cx,ax,0x12)))));
    }
    // 45D0A0(ax, cx) request 18 set-up.
    void setup_45d0a0(std::uint16_t ax,std::uint16_t cx){
        p16(0x7f8accu,ax);p16(0x7f8aceu,cx);p16(0x7f8acau,0);p16(0x7f8ac8u,0);p8(0x7f8ad2u,0);
        p32(0x7f8ac4u,0);p32(0x7f8ac0u,0);reset_result();
    }
    // 45D0F0(di; bp, a, b) request 20 set-up: 429780 banner size, cars in di..bp get +B66 = 0.
    void setup_45d0f0(std::uint16_t di,std::uint16_t bp,std::uint16_t a,std::uint16_t b){
        p16(0x7f197cu,di);p16(0x7f197eu,bp);p16(0x7f198cu,0);
        call(0x429780u,{0x2c00e5u,0x7f1980u,0x7f1984u});
        p16(0x7f1990u,b);p16(0x7f198eu,a);p32(0x7f1988u,0);
        for(std::uint32_t i=1;i<0x18u;++i){
            if((u8(0x79fb50u+i)&3u)!=2u)continue;
            const std::uint32_t w=u32(0x799d18u+i*0x3cu);
            if(u8(w+4)&1u)continue;
            const std::int16_t s=i16(w+0x64);
            if(s<std::int16_t(di)||s>std::int16_t(bp))continue;
            p16(w+0xb66,0);
        }
    }
    // 45D360(level): bonus hearts of a cleared level (0 when a request of it failed).
    std::uint32_t level_bonus_45d360(std::uint32_t level){
        const std::uint32_t pl=player(),r=0x7f2548u+level*0x44u;
        for(std::uint32_t k=0;k<6u;++k)if(u8(r+0x38+k))return 0;
        const float k1=K(0x5b0068u);
        const float t=float(std::int32_t(std::uint32_t(u16(pl+0x260))-std::uint32_t(std::int32_t(i16(0x7f1c5au)))));
        std::int32_t a=cvtt(sdiv(float(std::int32_t(i16(0x7f1c5cu))),smul(smul(t,K(0x5c403cu)),k1)));
        if(a>5)a=5;
        std::int32_t b=cvtt(sdiv(float(std::int32_t(i16(0x7f1c60u))),smul(smul(t,K(0x62821cu)),k1)));
        if(b>5)b=5;
        if(a>b)b=a;
        p32(0x7f1c54u,u32(0x7f1c54u)+std::uint32_t(b)*10u);
        const float rate=level_rate_4505f0(level);
        const float v=smul(f32(u32(pl+0x2b4)+0x2438),rate);
        if(gt(v,K(0x5b0074u)))p32(0x7f1c54u,u32(0x7f1c54u)+std::uint32_t(cvtt(ssub(v,K(0x5b0074u))))*2u);
        return u32(0x7f1c54u);
    }
    // 45D830: the escorts (+C5C 40000) take their road speed (470240) after a request.
    void escort_land_45d830(){
        Frame f(*this,0x20);
        for(std::int32_t i=0;i<i32(0x680ad8u);++i){
            const std::uint32_t id=u32(0x680ad4u)+std::uint32_t(i)+8u;
            if((u8(0x79fb48u+id)&3u)!=2u)continue;
            const std::uint32_t w=work(id);
            if(!(u32(w+0xc5c)&0x40000u))continue;
            place_pose_470240(w+0x5c,0,f(0x10),f(4),f(0));
            pf(w+0x1c4,f32(f(0)));
            p32(w+0x1f4,ftol(x(f(0))*X87(K(0x5a460cu))));p32(w+0xb58,0);
        }
        p32(0x7f250cu,1);
    }
    // 45D8E0: release the escorts (+C5C 40000 -> closing) and the 45D710 spawn table.
    void escort_release_45d8e0(){
        for(std::int32_t i=0;i<i32(0x680ad8u);++i){
            const std::uint32_t id=u32(0x680ad4u)+std::uint32_t(i)+8u;
            if((u8(0x79fb48u+id)&3u)!=2u)continue;
            const std::uint32_t w=work(id),c=u32(w+0xc5c);
            if(!(c&0x40000u))continue;
            p16(w+0xb64,0x3c);p32(w+0xc5c,(c&0xfffbffffu)|1u);
        }
        p32(0x7f24b8u,0);p32(0x7f24bcu,0);
    }
    // 45DCE0(edi base; delay) result hearts of a request (protected entry: ESI = [799D18]).
    std::uint32_t result_hearts_45dce0(std::uint32_t edi,std::int16_t delay){
        const std::uint32_t pl=player();
        const std::uint32_t r=0x7f2548u+call(0x44c940u,{u32(pl+0x68)})*0x44u;
        std::uint32_t ecx=0;
        if(delay>=5){
            static constexpr std::uint8_t B[13]{0,4,0,1,4,0,1,2,4,0,1,2,3};
            static constexpr std::uint32_t V[5]{0xa,0x14,0x1e,0x32,0};
            const std::uint32_t k=u32(pl+0x68)-2u;
            const std::uint32_t add=k<=0xcu?V[B[k]]:0u;
            ecx=(u32(0x5b0088u+std::uint32_t(std::int32_t(i8(r)))*4u)+add)*0x7530u;
        }
        const std::int32_t c=std::int32_t(ecx);
        switch(u32(0x7f24ccu)){
        case 1:return ecx+edi*3u;
        case 2:return std::uint32_t((std::int32_t(edi*3u)/2)+(c/3)*2);
        case 3:return std::uint32_t(c/3)+edi;
        default:return edi;
        }
    }
    // 45DDC0(eax hearts, cx kind): the girl's voice for a result.
    void result_voice_45ddc0(std::uint32_t h,std::int16_t kind){
        static constexpr std::uint8_t B[26]{0,8,1,8,8,8,2,3,3,1,8,2,8,8,4,5,6,8,8,7,5,8,1,3,3,6};
        static constexpr std::uint32_t T[9]{0x5b00f8,0x5b00b8,0x5b00d8,0x5b0098,0x5b00d8,0x5b0098,0x5b0098,0x5b0098,0x5b00d8};
        const std::uint32_t c=std::uint32_t(std::int32_t(kind));
        const std::uint32_t tab=c<=0x19u?T[B[c]]:0x5b00d8u;
        voice_45bd30(u32(tab+h*4u),0x3c,9);
    }
    // 45DED0(edi hearts): the girl's reaction (model 9 and 5 hearts: none).
    void result_girl_45ded0(std::uint32_t h){
        if(!(u8(0x7f8bc8u)&1u)){p32(0x7f8bc8u,u32(0x7f8bc8u)|1u);p32(0x7f8bc4u,player());}
        const std::int8_t model=i8(u32(0x7f8bc4u)+0x11);
        if(model==9&&h==5u)return;
        const std::uint32_t tab=model_flag_45b380(model)?0x5b0118u:0x5b0138u;
        rob_stop_487dd0(1);
        if(std::int32_t(h)>0)rob_anim_487d60(1,u32(tab+h*4u));
    }
    // 462D80(eax result): open the result banner (460970 with text / frame indices).
    void result_banner_462d80(std::uint32_t r){
        switch(r){
        case 0:case 1:banner_open_460970(1,0xb,1,0xffffffffu,0x78);break;
        case 2:banner_open_460970(1,0xa,1,0xffffffffu,0x78);break;
        case 3:banner_open_460970(1,9,0,0xffffffffu,0x78);break;
        case 4:banner_open_460970(1,8,0,0xffffffffu,0x78);break;
        case 5:banner_open_460970(1,7,0,0xffffffffu,0x78);break;
        case 6:banner_open_460970(1,6,0,0xffffffffu,0x78);break;
        default:banner_open_460970(1,5,0,0xffffffffu,0x78);break;
        }
    }
    // 460970(eax mode, edi text, ebx frame; open delay, close delay): open the request banner
    // (protected entry: sub esp, 0x84).
    void banner_open_460970(std::uint32_t mode,std::uint32_t text,std::uint32_t frame,std::uint32_t open,std::uint32_t close){
        if(!text)return;
        Frame f(*this,0x90);
        const std::uint32_t M0=f(0x10),T1=f(0x50);
        const std::uint32_t tok=mode?u32(0x5a93f0u+text*4u):0x2b003eu;
        const std::uint32_t frm=u32(0x5a9484u+frame*4u);
        banner_close_45bec0();
        p32(0x7f8b54u,spr_create(frm,2,0));p32(0x7f8b58u,spr_create(tok,3,2));
        p32(0x7f8b68u,0xffffffffu);call(0x428940u,{u32(0x7f8b54u),0xffffffffu});
        p32(0x7f8b48u,mode);p32(0x7f8b4cu,mode);
        pf(0x7f8b6cu,K(0x5b02a4u));pf(0x7f8bbcu,K(0x5b02a4u));
        p32(0x7f8b50u,0);p32(0x7f8b60u,text);p16(0x7f8b5cu,std::uint16_t(open));p16(0x7f8b5eu,std::uint16_t(close));
        p32(0x7f8b64u,frame);p16(0x7f8bb4u,0);pf(0x7f8b70u,K(0x5b02a0u));pf(0x7f8bc0u,K(0x5b029cu));
        translation_4393a6(M0,from_bits(0x43300000u),from_bits(0x429c0000u),0.0f);
        scaling_4393ca(T1,0.0f,0.0f,1.0f);
        mul_439394(M0,T1,M0);
        spr_matrix(u32(0x7f8b54u),M0);spr_matrix(u32(0x7f8b58u),M0);
    }
    // 462E20(eax, ecx, edx kind): set-up of the gate requests 17 (callback 45CA70) / 19 (461940).
    void setup_462e20(std::uint16_t ax,std::uint16_t cx,std::uint32_t kind){
        if(kind==0x11u){
            const std::uint32_t pl=player();
            p16(0x7f8a7au,ax);p16(0x7f8a78u,cx);p32(0x7f8a80u,0xe);p8(0x7f8a7cu,0);
            p32(0x7f8a84u,u32(pl+0x14));p32(0x7f8a88u,u32(pl+0x18));pf(0x7f8b44u,0.0f);p32(0x7f8a8cu,u32(pl+0x1c));
            p32(0x7f2524u,0x45ca70u);
        }else if(kind==0x13u){
            setup_45d040(std::uint16_t(cx),std::uint16_t(ax),0x32,0x28);
            p32(0x7f2528u,0x461940u);
        }
    }
    // 462EA0(eax hearts, ecx level, di result): book a request result in the level record.
    void result_book_462ea0(std::uint32_t hearts,std::uint32_t level,std::int16_t res){
        const std::uint32_t r=0x7f2548u+level*0x44u;
        const std::uint32_t d=std::uint32_t(std::int32_t(res));
        p8(r+0x38+d,u8(r+0x38+d)+1u);
        p32(r+u32(0x7f24c8u)*4u+8u,d);
        p32(r+0x1c,u32(r+0x1c)+d);
        p32(r+std::uint32_t(std::int32_t(i8(r+0x40)))*4u+0x20u,hearts);
        p32(0x7f23c4u,u32(0x7f23c4u)+hearts*10000u);
        p8(r+0x40,u8(r+0x40)+1u);
        result_banner_462d80(d);
        p16(0x7f8b2cu,std::uint16_t(res));
    }
    // 462F10(eax result; level, hearts): close the running request with its result.
    void request_end_462f10(std::int16_t res,std::uint32_t level,std::uint32_t hearts){
        banner_close_45bec0();
        std::int16_t si=res;
        if(si>7)si=7;
        else if(si<0)si=0;
        result_voice_45ddc0(std::uint32_t(std::int32_t(si)),std::int16_t(u32(0x7f2428u)));
        result_girl_45ded0(std::uint32_t(std::int32_t(si)));
        result_book_462ea0(hearts,level,si);
        for(std::uint32_t o:{0x7f24ecu,0x7f24f0u,0x7f24f4u,0x7f24f8u,0x7f24fcu,0x7f2500u,0x7f2504u})p32(o,0);
        p32(0x7f2428u,3);p32(0x7f2434u,0xffffffffu);p32(0x7f2470u,0);p32(0x7f2474u,0);p32(0x7f24e8u,0);
        pf(0x7f24e0u,K(0x6282bcu));p32(0x7f2508u,0);p32(0x7f24c8u,u32(0x7f24c8u)+1u);
        p32(0x7f24d0u,0);p32(0x7f24d4u,0);p32(0x7f19f8u,2);
        appear_override_4ef870(0);
    }

    // ==== Heart Attack HUD pieces run from the NAVI control step (45B860, 45FB80, ...) =========
    // 449940(a, b, in, out): perspective of a point through the current matrix (out.z = view z);
    // returns b / -z, or 0 with out = 0 when |z| < [6281F0].
    X87 project_449940(float a,float b,std::uint32_t in,std::uint32_t out){
        Frame f(*this,0x10);point_40a7d0(f(0),in);
        const float px=f32(f(0)),py=f32(f(4)),pz=f32(f(8));
        if(X87(K(0x6281f0u))>X87(std::fabs(pz))){pf(out+8,0.0f);pf(out+4,0.0f);pf(out,0.0f);return X87(K(0x619a34u));}
        const float r=sdiv(K(0x62806cu),ssub(0.0f,pz));
        pf(out,smul(smul(r,a),px));const float s=smul(r,b);pf(out+4,smul(py,s));pf(out+8,pz);
        return X87(s);
    }
    // 4393A6 D3DXMatrixTranslation(out, x, y, z).
    void translation_4393a6(std::uint32_t o,float tx,float ty,float tz){
        for(std::uint32_t k=0;k<16;++k)pf(o+k*4u,(k%5u)==0u?1.0f:0.0f);
        pf(o+0x30,tx);pf(o+0x34,ty);pf(o+0x38,tz);
    }
    // 45B860(esi vel, edi pos; kind, frame, scale, life): a HUD effect in the 7F2038 list (16 x 0x38).
    void effect_45b860(std::uint32_t vel,std::uint32_t pos,std::uint32_t kind,std::uint32_t frame,float scale,std::uint16_t life){
        std::uint32_t c=u32(0x7f8b24u);
        for(std::uint32_t n=0;n<0x10u;++n){
            const std::uint32_t e=0x7f2038u+c*0x38u;
            ++c;if(c==0x10u)c=0;
            if(u32(e))continue;
            inc32(0x7f8b20u);p32(0x7f8b24u,c);
            const float x0=f32(pos),x1=f32(pos+4),x2=f32(vel),x3=f32(vel+4);
            p32(e,1);p32(e+4,0);p32(e+8,0);pf(e+0xc,x0);pf(e+0x10,x1);
            if(!kind){pf(e+0x1c,K(0x5afd2cu));pf(e+0x20,0.0f);}else{pf(e+0x1c,x0);pf(e+0x20,x1);}
            pf(e+0x14,x2);pf(e+0x18,x3);p32(e+0x24,0xffffffffu);p32(e+0x28,kind);pf(e+0x30,scale);
            p32(e+0x2c,std::int32_t(frame)<7?frame:6u);p16(e+0x34,life);
            return;
        }
        p32(0x7f8b24u,c);
    }
    // Sprite pool services (PC 428320 create, 428460 create range, 4285A0 release, 4287B0 matrix,
    // 428800 speed, 428880 state, 428940 colour).
    std::uint32_t fbits(float v){std::uint32_t u;std::memcpy(&u,&v,4);return u;}
    std::uint32_t spr_create(std::uint32_t tok,std::uint32_t layer,std::uint32_t mode){return call(0x428320u,{tok,layer,mode});}
    std::uint32_t spr_range(std::uint32_t tok,std::uint32_t layer,std::uint32_t mode,std::uint32_t a,std::uint32_t b){return call(0x428460u,{tok,layer,mode,a,b});}
    void spr_release(std::uint32_t h){call(0x4285a0u,{h});}
    void spr_matrix(std::uint32_t h,std::uint32_t mtx){call(0x4287b0u,{h,mtx});}
    void spr_speed(std::uint32_t h,float v){call(0x428800u,{h,fbits(v)});}
    // 45FB80: the heart meter (7F19F8 1 active / 2 closing): its anchor over the car (camera
    // +34A > 1) or fixed, the beating heart 7F1A1C, the 19 heart sprites 7F1A38.. on the arc
    // 7F8BCC.. (7F8BBC/7F8BC0 centre), colour 7F8B68 when the request score is negative.
    void heart_meter_45fb80(){
        const std::uint32_t cam=u32(0x79f574u),pl=player();
        Frame f(*this,0x80);
        const std::uint32_t Lv=f(0x30),Lm=f(0x40);
        if(i8(cam+0x34a)>1){
            const std::uint32_t v=0x5e0ac8u+std::uint32_t(std::int32_t(i8(0x655b59u)))*12u;
            p32(Lv,u32(v));p32(Lv+4,u32(v+4));p32(Lv+8,u32(v+8));
            push_load(pl+0xb0);point_40a7d0(Lv,Lv);load(cam+0x140);
            (void)project_449940(f32(cam+0xb0),f32(cam+0xb4),Lv,Lv);
            pf(0x7f19fcu,f32(Lv));pf(0x7f1a00u,ssub(0.0f,f32(Lv+4)));
            pop();
        }else{pf(0x7f19fcu,K(0x5b0210u));pf(0x7f1a00u,K(0x5b020cu));}
        const std::uint32_t st=u32(0x7f19f8u);
        if(st==2u){
            for(std::uint32_t e=0x7f1a1cu,k=0;e<0x7f1c4cu;e+=0x1cu,++k){
                if(!u32(e-0x14u))continue;
                p32(e-0x14u,0);
                const std::uint32_t kind=u8(0x5a94d0u+k%3u);
                spr_release(u32(e));p32(e,0xffffffffu);
                if(i16(0x7f1ca8u)>6){
                    const float s=sadd(smul(ssub(K(0x6282d4u),float(std::int32_t(k))),K(0x6280e4u)),K(0x628064u));
                    effect_45b860(0x5b01f0u,e-8u,kind,std::uint32_t(std::int32_t(i16(e+4))),s,0);
                }
                p16(e+4,0xffffu);
            }
            p16(0x7f1ca8u,0);p32(0x7f1c3cu,0);p32(0x7f19f8u,0);
        }else if(st!=1u){
            p16(0x7f1ca8u,0);p32(0x7f1c3cu,0);p16(0x7f8c78u,0);p32(0x7f8b28u,u32(0x7f8b28u)-1u);return;
        }else{
            p32(0x7f8c74u,u32(0x7f8c74u)-1u);
            if(i32(0x7f2510u)>0)p32(0x7f2510u,u32(0x7f2510u)-1u);
            const std::int32_t sc=i32(0x7f8afcu),prev=i32(0x7f1c3cu);
            const std::int32_t hearts=sc/10000+1;
            const std::int32_t delta=std::int32_t(std::uint32_t(sc)-std::uint32_t(prev));
            const std::int32_t gain=std::int32_t(std::uint32_t(hearts)-std::uint32_t(prev/10000)-1u);
            if(sc<0){
                const float t=smul(float(sc),K(0x6280c4u));
                const float a=jb(t,K(0x5b0208u))?ssub(K(0x62806cu),smul(t,K(0x5b0204u))):0.0f;
                const std::uint32_t b=ftol(X87(a)*X87(K(0x5a91b8u)))&0xffu;
                const std::uint32_t col=(((b<<8)|b)<<8)|0xff0000ffu;
                p32(0x7f8b68u,col);call(0x428940u,{u32(0x7f8b54u),col});
            }else{p32(0x7f8b68u,0xffffffffu);call(0x428940u,{u32(0x7f8b54u),0xffffffffu});}
            p32(0x7f1c3cu,std::uint32_t(sc));
            const float lx=f32(0x7f19fcu),ly=f32(0x7f1a00u);
            if(!u32(0x7f1a08u)){
                p32(0x7f1a08u,1);pf(0x7f1a0cu,0.0f);pf(0x7f1a10u,0.0f);p16(0x7f1a20u,0);
                p32(0x7f1a1cu,spr_range(0x2b000fu,1,0,0x1f,0x3c));
            }
            pf(0x7f1a14u,lx);pf(0x7f1a18u,ly);
            translation_4393a6(Lm,lx,ly,0.0f);spr_matrix(u32(0x7f1a1cu),Lm);
            if(gain<0||delta<0){
                if(!u16(0x7f8c78u)){const std::uint32_t h=spr_create(0x2b000eu,4,3);spr_speed(h,2.5f);spr_matrix(h,Lm);p16(0x7f8c78u,0x14);}
                p16(0x7f1a20u,std::uint16_t((hearts<0?0:hearts)%7));
                spr_release(u32(0x7f1a1cu));
                const std::uint32_t h=spr_range(u32(0x5a9468u+std::uint32_t(std::int32_t(i16(0x7f1a20u)))*4u),1,0,0x1f,0x3c);
                p32(0x7f1a1cu,h);spr_matrix(h,Lm);
                p32(0x7f1978u,0);
            }else if(gain>0){
                static constexpr std::uint8_t B[24]{0,1,1,1,1,1,0,1,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0};
                p16(0x7f1a20u,std::uint16_t((hearts<0?0:hearts)%7));
                const std::uint32_t q=u32(0x7f2428u);
                if(!(q<=0x17u&&B[q]==0u&&i32(0x7f2510u)>0)){
                    spr_release(u32(0x7f1a1cu));p32(0x7f2510u,0x14);
                    const std::uint32_t h=spr_range(u32(0x5a9468u+std::uint32_t(std::int32_t(i16(0x7f1a20u)))*4u),1,0,0x1f,0x3c);
                    p32(0x7f1a1cu,h);spr_matrix(h,Lm);
                    pf(Lv,f32(0x7f8bbcu));pf(Lv+4,ssub(0.0f,f32(0x7f8bc0u)));
                    if(i32(0x7f8c74u)<=0){
                        call(0x424940u,{0x52});p32(0x7f8c74u,10);
                        effect_45b860(Lv,0x7f19fcu,2,hearts<0?1u:5u,1.0f,0);
                    }
                }
            }
            const std::int32_t shown=hearts<0?0:hearts;
            const float cx=f32(0x7f8bbcu),cy=f32(0x7f8bc0u);
            std::int32_t lim=cvtt(smul(float(shown),K(0x628088u)));
            std::uint32_t chime=0;
            if(lim>0x13)lim=0x13;
            if(i32(0x7f24ecu)>0){
                std::int32_t a=u32(0x7f24f0u)?6:7;
                if(u32(0x7f24f4u))a-=2;
                if(u32(0x7f24f8u))a-=3;
                if(u32(0x7f24fcu))a-=4;
                if(a<0)a=0;
                const std::int32_t cap=cvtt(smul(float(a),K(0x5b0200u)));
                if(lim>cap)lim=cap;
                if(lim>0x13)lim=0x13;
            }
            if(!u32(0x7f8c70u)){
                p32(0x7f8c70u,1);
                for(std::uint32_t i=0;i<0x13u;++i){
                    const float t=driving::x87_float(X87(std::int32_t(i))*X87(K(0x5b01fcu)));
                    pf(0x7f8bd0u+i*8u,driving::x87_float(driving::x87_cos(X87(t))*X87(K(0x6281d4u))));
                    pf(0x7f8bd4u+i*8u,driving::x87_float(driving::x87_sin(X87(t))*X87(K(0x5b01f8u))));
                }
            }
            std::int32_t idx=1;
            for(std::uint32_t e=0x7f1a38u,a=0x7f8bd4u;e<0x7f1c4cu;e+=0x1cu,a+=8u,++idx){
                const std::uint32_t layer=e<0x7f1b34u?4u:5u;
                if(u32(e-0x14u)){
                    if(idx>lim){
                        p32(e-0x14u,0);spr_release(u32(e));p32(e,0xffffffffu);
                        const std::uint32_t h=spr_create(0x2b000eu,layer,3);spr_speed(h,2.5f);
                        const float y=ssub(0.0f,ssub(cy,f32(a)));
                        translation_4393a6(Lm,sadd(f32(a-4u),cx),y,0.0f);spr_matrix(h,Lm);
                    }else{
                        if(call(0x428880u,{u32(e)})==3u){spr_release(u32(e));p32(e,spr_range(0x2b0015u,layer,0,1,0x3b));}
                        const float x=sadd(f32(a-4u),cx);pf(e-8u,x);
                        const float y=ssub(f32(a),cy);pf(e-4u,y);
                        translation_4393a6(Lm,x,y,0.0f);spr_matrix(u32(e),Lm);
                    }
                }else{
                    if(idx>lim)break;
                    p32(e-0x14u,1);p16(e+4,7);
                    const std::uint32_t h=spr_range(0x2b0015u,layer,1,0x3d,0x69);
                    const float x=sadd(f32(a-4u),cx);pf(e-8u,x);
                    const float y=ssub(ssub(0.0f,cy),f32(a));
                    p32(e,h);pf(e-4u,y);
                    translation_4393a6(Lm,x,y,0.0f);spr_matrix(u32(e),Lm);
                    chime=1;
                }
            }
            if(chime)call(0x424940u,{0xd7});
            p16(0x7f1ca8u,std::uint16_t(shown));
        }
        if(i16(0x7f8c78u)>0)p16(0x7f8c78u,u16(0x7f8c78u)-1u);
        p32(0x7f8b28u,u32(0x7f8b28u)-1u);
    }
    // 45B950: the HUD effects of the 7F2038 list (state +4: 0 wait +34, 1, 2 open 4C..5A,
    // 3 run then open 5B..69, 4 open 5B..69, 5 close; motion kind +28: 0 arc, 1 x stepped / y eased,
    // 2 stepped, 3 x eased / y stepped).
    void effects_45b950(){
        Frame f(*this,0x40);
        const std::uint32_t Lm=f(0);
        const std::int32_t n=i32(0x7f8b20u);
        if(n<=0)return;
        std::int32_t done=0;std::uint32_t idx=0;
        for(std::uint32_t s=0x7f2044u;done<n;s+=0x38u,++idx){
            if(s>=0x7f23c4u)break;
            if(!u32(s-0xcu))continue;
            const std::uint32_t st=u32(s-8u);
            ++done;
            if(st>5u)continue;
            auto open=[&](std::uint32_t a,std::uint32_t b){
                const std::uint32_t h=spr_range(u32(0x5a9468u+u32(s+0x20)*4u),3,1,a,b);
                p32(s+0x18,h);call(0x428800u,{h,u32(s+0x24)});
                translation_4393a6(Lm,f32(s+0x10),f32(s+0x14),0.0f);spr_matrix(u32(s+0x18),Lm);
                p32(s-8u,u32(s-8u)+1u);};
            switch(st){
            case 0:p16(s+0x28,u16(s+0x28)-1u);if(i16(s+0x28)<0)p32(s-8u,st+1u);break;
            case 1:p32(s-8u,st+1u);break;
            case 2:open(0x4c,0x5a);break;
            case 3:
                if(call(0x428880u,{u32(s+0x18)})==3u){
                    spr_release(u32(s+0x18));p32(s+0x18,0xffffffffu);p32(s-8u,u32(s-8u)+1u);
                    open(0x5b,0x69);break;
                }
                {
                    const float sc=f32(s+0x24),k1=K(0x5a9254u),k2=K(0x5afd38u);
                    const float cnt=float(i32(s-4u));
                    switch(u32(s+0x1c)){
                    case 0:{const float sv=smul(smul(sc,k1),cnt);
                        const float y=sadd(float(cvtt(smul(sv,ssub(f32(s+0xc),f32(s+4))))),f32(s+4));pf(s+0x14,y);
                        const float t=ssub(y,K(0x6282ccu));
                        pf(s+0x10,sadd(smul(smul(t,t),K(0x5afd34u)),K(0x5afd30u)));break;}
                    case 1:{const float a=smul(smul(cnt,smul(sc,k1)),ssub(f32(s+8),f32(s)));const float b=smul(sc,k2);
                        pf(s+0x10,sadd(float(cvtt(a)),f32(s)));
                        pf(s+0x14,sadd(smul(ssub(f32(s+0xc),f32(s+0x14)),b),f32(s+0x14)));break;}
                    case 2:{const float sv=smul(smul(sc,k1),cnt);
                        pf(s+0x10,sadd(float(cvtt(smul(ssub(f32(s+8),f32(s)),sv))),f32(s)));
                        pf(s+0x14,sadd(float(cvtt(smul(ssub(f32(s+0xc),f32(s+4)),sv))),f32(s+4)));break;}
                    case 3:{pf(s+0x10,sadd(smul(ssub(f32(s+8),f32(s+0x10)),smul(sc,k2)),f32(s+0x10)));
                        const float v=smul(smul(cnt,smul(sc,k1)),ssub(f32(s+0xc),f32(s+4)));
                        pf(s+0x14,sadd(float(cvtt(v)),f32(s+4)));break;}
                    default:break;
                    }
                    translation_4393a6(Lm,f32(s+0x10),f32(s+0x14),0.0f);spr_matrix(u32(s+0x18),Lm);
                    p32(s-4u,u32(s-4u)+1u);
                }
                break;
            case 4:open(0x5b,0x69);break;
            case 5:
                if(call(0x428880u,{u32(s+0x18)})==3u){
                    p32(0x7f8b24u,idx);spr_release(u32(s+0x18));
                    p32(s+0x18,0xffffffffu);p32(s-0xcu,0);p32(0x7f8b20u,u32(0x7f8b20u)-1u);
                }
                break;
            }
        }
    }
    // 45BEC0: close the request banner (sprites 7F8B54 / 7F8B58) and reset its state.
    void banner_close_45bec0(){
        spr_release(u32(0x7f8b54u));spr_release(u32(0x7f8b58u));
        p32(0x7f8b4cu,0);p32(0x7f8b50u,0);p32(0x7f8b54u,0xffffffffu);p32(0x7f8b58u,0xffffffffu);
        p16(0x7f8b5cu,0);p32(0x7f8b60u,0);p32(0x7f8b64u,0);pf(0x7f8b6cu,0.0f);pf(0x7f8b70u,0.0f);
        p16(0x7f8bb4u,0);p16(0x7f8bb6u,0);p32(0x7f8b68u,0xffffffffu);
    }
    std::array<float,16> mtx(std::uint32_t a){std::array<float,16> r;for(std::uint32_t k=0;k<16;++k)r[k]=f32(a+k*4u);return r;}
    void mul_439394(std::uint32_t o,std::uint32_t a,std::uint32_t b){
        const auto r=driving::pc_d3dx_matrix_multiply(mtx(a),mtx(b));for(std::uint32_t k=0;k<16;++k)pf(o+k*4u,r[k]);
    }
    void scaling_4393ca(std::uint32_t o,float sx,float sy,float sz){driving::pc_d3dx_matrix_scaling(m.bytes(o,64),sx,sy,sz);}
    // 460AD0: the request banner (7F8B54 frame / 7F8B58 text, 7F8B50 frame counter): slide-in
    // squash 0..10, then the bob of 7F8BB4 frames (7F8BB8 phase, step 7F8BBA); 7F8B4C 0 closed.
    void banner_460ad0(){
        if(u32(0x7f8b58u)==0xffffffffu)return;
        const std::uint32_t pl=player();
        Frame f(*this,0x120);
        const std::uint32_t M0=f(0x14),T1=f(0x54),S=f(0x94),T2=f(0xd4);
        std::int32_t n=i32(0x7f8b50u);
        auto squash=[&](bool second){
            const float a=ssub(K(0x6281d4u),f32(0x7f8b70u));
            const float t=smul(ssub(K(0x5b02b8u),a),K(0x5b02b4u));
            for(std::uint32_t k=0;k<16;++k)pf(M0+k*4u,(k%5u)==0u?K(0x62806cu):0.0f);
            if(!second){
                const float s=smul(float(n),K(0x5b0068u));
                translation_4393a6(T1,0.0f,from_bits(0xc2800000u),0.0f);
                scaling_4393ca(S,smul(ssub(K(0x62806cu),s),t),smul(t,s),1.0f);
                translation_4393a6(T2,f32(0x7f8b6cu),ssub(0.0f,f32(0x7f8b70u)),0.0f);
            }else{
                const float s=smul(ssub(float(n),K(0x628138u)),K(0x5b0068u));
                translation_4393a6(T1,0.0f,from_bits(0x42800000u),0.0f);
                scaling_4393ca(S,s,sadd(smul(ssub(K(0x62806cu),t),s),t),1.0f);
                translation_4393a6(T2,f32(0x7f8b6cu),from_bits(0xc3200000u),0.0f);
            }
            mul_439394(M0,S,T2);mul_439394(M0,T1,M0);
            spr_matrix(u32(0x7f8b54u),M0);spr_matrix(u32(0x7f8b58u),M0);
            for(std::uint32_t k=0;k<0x40u;k+=4)p32(0x7f8b74u+k,u32(M0+k));
        };
        bool bob=true;
        if(n<=10){
            if(n<=5)squash(false);
            else{
                squash(true);
                if(i32(0x7f8b50u)==10){call(0x428770u,{u32(0x7f8b58u),0});pf(0x7f8b70u,K(0x5b02a8u));}
            }
            p32(0x7f8b50u,u32(0x7f8b50u)+1u);
        }else if(!u32(0x7f8b4cu)){
            const std::int16_t cx=i16(0x7f8b5cu);
            if(cx<0||n-0x14<std::int32_t(cx))p32(0x7f8b50u,std::uint32_t(n+1));
            else{
                const std::uint32_t text=u32(0x5a93f0u+u32(0x7f8b60u)*4u),frame=u32(0x5a9484u+u32(0x7f8b64u)*4u),keep=u32(0x7f8b60u);
                banner_close_45bec0();p32(0x7f8b60u,keep);
                p32(0x7f8b54u,spr_create(frame,2,0));p32(0x7f8b58u,spr_create(text,3,2));
                call(0x428940u,{u32(0x7f8b54u),u32(0x7f8b68u)});
                pf(0x7f8b6cu,K(0x5b02a4u));p32(0x7f8b50u,0);p32(0x7f8b4cu,1);pf(0x7f8b70u,K(0x5b02a0u));
                translation_4393a6(M0,from_bits(0x43300000u),from_bits(0x429c0000u),0.0f);
                scaling_4393ca(T1,0.0f,0.0f,1.0f);
                mul_439394(M0,T1,M0);
                spr_matrix(u32(0x7f8b54u),M0);spr_matrix(u32(0x7f8b58u),M0);
                rob_anim_487d60(1,0x25000c);p16(0x7f8bb4u,0);
            }
        }else{
            const std::int16_t cx=i16(0x7f8b5eu);
            if(cx<0||n-10<std::int32_t(cx))p32(0x7f8b50u,std::uint32_t(n+1));
            else banner_close_45bec0();
        }
        if(!bob)return;
        if(u32(pl+0x5c))return;
        if(!u16(0x7f8bb4u))return;
        const float a=float(std::int32_t(i16(0x7f8bb8u)));
        pf(0x7f8b70u,smul(a,K(0x5b02b0u)));pf(0x7f8b6cu,smul(a,K(0x5b02acu)));
        for(std::uint32_t k=0;k<0x40u;k+=4)p32(M0+k,u32(0x7f8b74u+k));
        translation_4393a6(T1,f32(0x7f8b6cu),f32(0x7f8b70u),0.0f);
        mul_439394(M0,M0,T1);
        spr_matrix(u32(0x7f8b58u),M0);
        pf(0x7f8bbcu,sadd(f32(0x7f8b6cu),K(0x5b02a4u)));pf(0x7f8bc0u,ssub(K(0x5b029cu),f32(0x7f8b70u)));
        spr_matrix(u32(0x7f8b54u),M0);
        p16(0x7f8bb8u,u16(0x7f8bb8u)+u16(0x7f8bbau));
        p16(0x7f8bb4u,u16(0x7f8bb4u)-1u);
    }

    // ==== Heart Attack display (NAVI_PUB display 4BEB00, variant 2) =============================
    // 2D sprites (429530 / 4294C0 / 4295D0) and text (4BC990) are services; models are draw
    // leaves (draw_leaf: the display's draw list, the oracle's services).
    void sprite_429530(std::uint32_t tok,float x,float y,std::uint32_t layer,std::uint32_t frame){call(0x429530u,{tok,bits(x),bits(y),layer,frame});}
    // 5802DD sprintf of "%3d" / "%4d" into buf.
    void format_int(std::uint32_t buf,std::uint32_t fmt,std::int32_t v){
        const std::uint32_t width=u8(fmt+1)-'0';
        char t[32];std::snprintf(t,sizeof t,"%*d",int(width),v);
        for(std::uint32_t k=0;;++k){p8(buf+k,std::uint8_t(t[k]));if(!t[k])break;}
    }
    // 45B3D0: the 32 markers of the 8037C8 ring (two posts each, +4C / +50) as 57002F models
    // bobbing with sin(+58 * 0x444) and squashed by its inverse.
    void markers_45b3d0(){
        draw_leaf(0x4044a0u,{});
        draw_leaf(0x4044e0u,{9});
        push_409ef0();
        draw_leaf(0x4052b0u,{});
        for(std::uint32_t k=0;k<32;++k){
            const std::uint32_t m=0x8037c8u+k*0x5cu;
            if(u32(m)==0x19au)continue;
            const float a=driving::x87_float(X87(std::int32_t(i16(m+0x58))*0x444)*X87(K(0x628254u)));
            const float s=driving::x87_float(driving::x87_sin(X87(a))*X87(K(0x628064u))+X87(K(0x62806cu)));
            const float inv=driving::x87_float(X87(K(0x62806cu))/X87(s));
            for(std::uint32_t j=0;j<2;++j){
                const std::uint32_t p=m+0x28u+j*12u;
                if(!u32(m+0x4cu+j*4u))continue;
                identity_40a020();
                translate3_40a290(f32(p),sadd(f32(p+4),s),f32(p+8));
                rotate_y(a);
                scale_40a360(inv,1.0f,inv);
                draw_leaf(0x405360u,{0x57002fu,0,0,0,0xffffffffu,0});
            }
        }
        draw_leaf(0x4052c0u,{});
        pop();
        draw_leaf(0x4044c0u,{});
    }
    // 45B7D0: the request counter sprites 2B0005 / 2B0006 (frame [7F8A56] - 1).
    void counter_45b7d0(){
        sprite_429530(0x2b0005u,0.0f,30.0f,1,0);
        sprite_429530(0x2b0006u,0.0f,30.0f,1,std::uint32_t(std::int32_t(i16(0x7f8a56u))-1));
    }
    // 45D190: the heart bubbles 2C00E5 (4295D0, 3D sprite at car +14) over the near cars
    // 9..31 still worth a heart (+2C8 == 0, +B6C clear), animated by +B66.
    void car_hearts_45d190(){
        const std::uint32_t pl=player();
        for(std::uint32_t i=1;i<0x18u;++i){
            if((u8(0x79fb50u+i)&3u)!=2u)continue;
            const std::uint32_t car=u32(0x799d54u+(i-1u)*0x3cu);
            if(u8(car+4)&1u)continue;
            if(u8(car+0xb6c))continue;
            if(f32(car+0x2c8)!=K(0x619a34u))continue;
            if(!(std::int32_t(i16(car+0x64))-std::int32_t(i16(pl+0x64))<10))continue;
            const std::int16_t f=i16(car+0xb66);
            const float sc=f<=0x19?K(0x5b006cu):ssub(K(0x5b006cu),smul(float(std::int32_t(f)-0x19),K(0x5b0068u)));
            call(0x4295d0u,{0x2c00e5u,car+0x14,0xffffffffu,std::uint32_t(std::int32_t(f)),bits(sc),0xbca3d70au,0,0,0});
            p16(car+0xb66,u16(car+0xb66)+1u);
            if(!(std::int32_t(i16(car+0xb66))<i32(0x7f1984u)))p16(car+0xb66,std::uint16_t(u32(0x7f1984u)-1u));
        }
    }
    // 45BDF0: the voice-face panel [5AFD40 + [7F8B2C] * 4] sliding in while [7F8B28] > 0.
    void face_45bdf0(){
        const std::int32_t t=i32(0x7f8b28u);
        if(t<=0)return;
        std::int32_t x;
        if(t>0x5a){const float v=smul(ssub(K(0x62806cu),smul(float(t-0x5a),K(0x5afd64u))),K(0x5afd60u));x=-0x9c-cvtt(v);}
        else x=0xe4;
        Frame L(*this,8);
        const std::uint32_t tok=u32(0x5afd40u+std::uint32_t(std::int32_t(i16(0x7f8b2cu)))*4u);
        call(0x4294c0u,{tok,L(4),L(0)});
        const float y=ssub(f32(L(0)),K(0x5afd3cu));
        const float xf=sadd(ssub(float(x),K(0x6281c8u)),f32(L(4)));
        sprite_429530(u32(0x5afd40u+std::uint32_t(std::int32_t(i16(0x7f8b2cu)))*4u),xf,y,9,0);
    }
    // 45B200(esi record): the road prop model (+2C) at its road frame (45B000 once) raised by
    // +20..+28 and [7F1C40].
    void road_prop_45b200(std::uint32_t r){
        if(!u32(r)){p32(r,1);road_frame_45b000(r,0xffffffffu);pf(r+0x20,0.0f);pf(r+0x24,0.0f);pf(r+0x28,0.0f);}
        load_40a170(0x7d2da0u);
        translate3_40a290(f32(r+8),f32(r+0xc),f32(r+0x10));
        rotate_y(f32(r+0x18));rotate_x(f32(r+0x14));rotate_z(f32(r+0x1c));
        translate3_40a290(f32(r+0x20),sadd(f32(r+0x24),K(0x7f1c40u)),f32(r+0x28));
        draw_leaf(0x405360u,{u32(r+0x2c),0,0,0,0xffffffffu,0});
    }
    // 45B2A0: the escort / goal props [7F24D0..7F24DC].
    void road_props_45b2a0(){
        if(!u32(0x7f24d0u)||!u32(0x7f24d4u))return;
        draw_leaf(0x4052b0u,{});
        push_409ef0();
        road_prop_45b200(u32(0x7f24d0u));
        road_prop_45b200(u32(0x7f24d4u));
        if(const std::uint32_t r=u32(0x7f24d8u))road_prop_45b200(r);
        if(const std::uint32_t r=u32(0x7f24dcu))road_prop_45b200(r);
        pop();
        draw_leaf(0x4052c0u,{});
    }
    // 45C7E0(stage): the class of its quest (1: 0x0F..0x15 / 0x1A..0x1E, 2: 0x16..0x19 /
    // 0x1F..0x23, 3: 0x24..0x28, else 0).
    std::uint32_t quest_class_45c7e0(std::uint32_t stage){
        (void)call(0x44c940u,{stage});
        const std::uint32_t q=stage_quest(stage);
        if(std::int32_t(q)<0xf||q-0xfu>0x19u)return 0;
        static constexpr std::uint8_t T[26]{0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,1,1,1,1,1,2,2,2,2,2};
        return T[q-0xfu]+1u;
    }
    // 4603F0: the stage result sheet ([7F8B34] frames): request rows with their results and
    // values, the level grade with its voice / girl animation, then the stage class banner.
    void result_sheet_4603f0(){
        const std::uint32_t pl=player();
        if(!u32(pl+0x5c))return;
        Frame E(*this,0x40);
        const std::uint32_t stage=u32(pl+0x68);
        std::uint32_t lvl=call(0x44c940u,{stage});
        const std::uint32_t ent=0x7f2548u+lvl*0x44u;
        if(lvl==4u)return;
        const std::uint32_t t=u32(0x7f8b34u);
        if(t>=0x21cu){
            const std::uint32_t r=call(0x451350u,{lvl});
            if(r==2u)return;
            const std::uint32_t v=call(0x450250u,{stage,r});
            if(t-0x276u<call(0x43fa00u,{})){
                const std::uint32_t k=quest_class_45c7e0(v);
                if(k>=1u&&k<=3u){call(0x424940u,{0xdb});call(0x428320u,{0x2b0040u+(k-1u),7,3});}
            }
            if(u32(0x7f8b34u)-0x294u<call(0x43fa00u,{})){
                const std::uint32_t k=quest_class_45c7e0(v);
                if(k==1u)call(0x424940u,{0x1cc});
                else if(k==2u){call(0x424940u,{0x1cd});call(0x424940u,{0xd9});}
                else if(k==3u){call(0x424940u,{0x1ce});call(0x424940u,{0xda});}
            }
            return;
        }
        if(t<0x78u)return;
        if(t-0x78u<call(0x43fa00u,{}))call(0x424940u,{0xa9});
        if(u32(0x7f8b34u)<0x168u){
            const std::uint32_t tok=u32(0x5b0288u+lvl*4u);
            pf(E(4),0.0f);pf(E(0),0.0f);
            call(0x4294c0u,{tok,E(0),E(4)});
            sprite_429530(u32(0x5b0288u+lvl*4u),ssub(f32(E(0)),K(0x6281c8u)),ssub(f32(E(4)),K(0x6281ccu)),9,0);
            const std::uint32_t c=u32(0x7f24c8u);
            if(c>=1u&&c<=3u)sprite_429530(0x2c0096u+(c-1u),0.0f,0.0f,9,0);
            for(std::uint32_t i=0;std::int32_t(i)<i32(0x7f24c8u);++i){
                const std::uint32_t lim=0x78u+i*0x3cu;
                const std::uint32_t t2=u32(0x7f8b34u);
                const std::int32_t yi=cvtt(K(0x5b0274u+i*4u));
                p32(E(0x18),std::uint32_t(yi));
                if(t2>=lim+0x3cu){
                    const std::uint32_t res=u32(ent+8u+i*4u);
                    if(t2-lim-0x3cu<call(0x43fa00u,{}))call(0x424940u,{u32(0x5afc54u+res*4u)});
                    pf(E(0x14),0.0f);pf(E(0xc),0.0f);
                    call(0x4294c0u,{u32(0x5c21e8u+res*4u),E(0xc),E(0x14)});
                    sprite_429530(u32(0x5c21e8u+res*4u),ssub(f32(E(0xc)),K(0x5b0270u)),float(i32(E(0x18))),9,0);
                }else if(t2>=lim){
                    pf(E(0x1c),0.0f);pf(E(0x10),0.0f);
                    call(0x4294c0u,{u32(0x5c21e8u+((t2>>2)&7u)*4u),E(0x10),E(0x1c)});
                    const std::uint32_t t3=u32(0x7f8b34u);
                    sprite_429530(u32(0x5c21e8u+((t3>>2)&7u)*4u),ssub(f32(E(0x10)),K(0x5b0270u)),float(yi),9,0);
                }
                format_int(E(0x28),0x5b026cu,i32(ent+0x20u+i*4u));
                call(0x4bc990u,{0xa,0x19d,u32(0x5b0258u+i*4u),E(0x28)});
            }
        }
        const std::uint32_t t3=u32(0x7f8b34u);
        if(t3>=0x1a4u){
            std::int32_t g=i32(ent+0x1c)/std::int32_t(std::int8_t(u8(ent+0x40)));
            if(g<0)g=0;else if(g>7)g=7;
            if(t3-0x1a4u<call(0x43fa00u,{})){
                call(0x424940u,{u32(0x5afc54u+std::uint32_t(g)*4u)});
                call(0x428320u,{u32(0x5b0238u+std::uint32_t(g)*4u),9,3});
                const std::uint32_t tbl=model_flag_45b380(u8(pl+0x11))?0x5a94b0u:0x5a9490u;
                if(g<1)g=1;
                rob_anim_487d60(1,u32(tbl+std::uint32_t(g)*4u));
            }
            pf(E(0x10),0.0f);pf(E(0xc),0.0f);
            call(0x4294c0u,{0x2c0087u,E(0xc),E(0x10)});
            sprite_429530(0x2c0087u,ssub(f32(E(0xc)),K(0x6281c8u)),ssub(f32(E(0x10)),K(0x5b0234u)),9,0);
            return;
        }
        if(t3<0x168u)return;
        pf(E(0),0.0f);pf(E(4),0.0f);
        call(0x4294c0u,{0x2c0087u,E(4),E(0)});
        sprite_429530(0x2c0087u,ssub(f32(E(4)),K(0x6281c8u)),ssub(f32(E(0)),K(0x5b0234u)),9,0);
        call(0x4294c0u,{u32(0x5b0214u+((u32(0x7f8b34u)>>2)&7u)*4u),E(4),E(0)});
        const std::uint32_t t4=u32(0x7f8b34u);
        sprite_429530(u32(0x5b0214u+((t4>>2)&7u)*4u),ssub(f32(E(4)),K(0x6281c8u)),ssub(f32(E(0)),K(0x5b0234u)),9,0);
    }
    // 45ECA0 / 45E9B0: the quest's goal models over the course frame (7D2DA0, raised 0.1).
    void goal_models(bool alt){
        const std::uint32_t q=stage_quest(u32(player()+0x68));
        if(alt){
            static const std::uint32_t Q[]{0xf,0x11,0x13,0x15,0x1f,0x1b,0x25,0x1c,0x26,0x1d,0x22,0x1e,0x23,0x28};
            bool any=false;for(auto v:Q)any=any||q==v;
            if(!any)return;
        }
        draw_leaf(0x4052b0u,{});
        push_409ef0();
        load_40a170(0x7d2da0u);
        translate3_40a290(0.0f,from_bits(0x3dcccccdu),0.0f);
        auto obj=[&](std::uint32_t id){draw_leaf(0x405360u,{id,0,0,0,0xffffffffu,0});};
        if(!alt){
            if(std::int32_t(q)>=0xf){
                const std::uint32_t c=q-0xfu;
                static constexpr std::uint8_t B[18]{0,1,2,3,10,4,10,10,10,10,10,5,10,6,7,10,8,9};
                static constexpr std::uint32_t M[10]{0xfb0001u,0xf50001u,0xf40002u,0xf80001u,0xfc0001u,0x1020001u,0x100000au,0,0x1020002u,0xfa0002u};
                if(c<=0x11u&&B[c]!=10u){
                    const std::uint32_t b=B[c];
                    if(b==7u){obj(0x1010003u);obj(0x1010004u);}
                    else obj(M[b]);
                }
            }else{
                const std::uint32_t s=call(0x44dc50u,{u32(player()+0x68)});
                if(s==0x14u)obj(0xf40001u);else if(s==0x19u)obj(0xfa0001u);else if(s==0x1du)obj(0x1020003u);
            }
        }else{
            const std::uint32_t c=q-0xfu;
            static constexpr std::uint8_t B[26]{0,12,1,12,2,12,3,12,12,12,12,12,4,5,6,7,8,12,12,9,10,12,11,5,12,7};
            switch(c<=0x19u?B[c]:12u){
            case 0:obj(0x5e0004u);obj(0x5e0003u);break;   // second id: the protected push 45EA91 (oracle)
            case 1:obj(0x610003u);obj(0x610004u);break;
            case 2:obj(0x5a0004u);obj(0x5a0003u);break;
            case 3:obj(0x5f0003u);break;
            case 4:obj(0x630004u);break;
            case 5:obj(0x640003u);break;
            case 6:obj(0x660003u);obj(0x660004u);break;
            case 7:obj(0x650006u);obj(0x650007u);break;
            case 8:obj(0x620005u);break;
            case 9:obj(0x660004u);break;
            case 10:obj(0x650006u);obj(0x650008u);break;
            case 11:obj(0x630005u);break;
            default:break;
            }
        }
        pop();
        draw_leaf(0x4052c0u,{});
    }
    // 46ED20 (end of every NAVI display): in state 9 (45C440), the 3D markers of the pair records
    // 800AF8: a 570038 post scaled (1, 2, +2C) at +8 turned by +24 / +20, and a 57002F heart at the
    // pair's midpoint (raised by [5B005C]) scaled by (+30 + 1) * [628088], turned by [804384].
    // World slot 6 = the pushed identity (409DF0 -> 410F90). Protected 44786F: esi = 800B1C.
    void pairs_46ed20(){
        if(call(0x45c440u,{})!=9u)return;                 // 45C440 = [7F2428]
        push_unit();
        draw_leaf(0x4044a0u,{});
        draw_leaf(0x4044e0u,{9});
        draw_leaf(0x409df0u,{6});
        draw_leaf(0x4052b0u,{});
        for(std::uint32_t r=0x800af8u;r<0x800d28u;r+=0x38u){
            if(u32(r)==0x19au||u32(r+4)==0x19au)continue;
            identity_40a020();
            translate_40a2d0(r+8);
            rotate_y(f32(r+0x24));rotate_x(f32(r+0x20));
            scale_40a360(1.0f,2.0f,f32(r+0x2c));
            draw_leaf(0x405360u,{0x570038u,0,0,0,0xffffffffu,0});
            Frame L(*this,0x10);
            blend_40f180(L(0),r+8,0.5f,r+0x14,0.5f);
            const float y=sadd(f32(L(4)),K(0x5b005cu));
            const float f=driving::x87_float((X87(f32(r+0x30))+X87(K(0x62806cu)))*X87(K(0x628088u)));
            pf(L(4),y);
            identity_40a020();
            translate_40a2d0(L(0));
            scale_40a360(f,f,f);
            rotate_y(f32(0x804384u));
            draw_leaf(0x405360u,{0x57002fu,0,0,0,0xffffffffu,0});
        }
        draw_leaf(0x4052c0u,{});
        draw_leaf(0x4044c0u,{});
        pop();
    }
    // 462CD0: the Heart Attack part of the race display by state [7F2428].
    void heart_display_462cd0(){
        (void)call(0x450380u,{8});
        const std::uint32_t s=u32(0x7f2428u)-2u;
        static constexpr std::uint8_t B[24]{0,1,6,6,0,0,6,0,6,2,6,6,3,0,0,4,6,0,5,6,0,6,0,0};
        switch(s<=0x17u?B[s]:0u){
        case 1:if(const std::uint32_t cb=u32(0x7f2524u)){if(cb!=0x45ca70u)throw std::logic_error("462CD0: display callback not ported");gates_draw_45ca70();}break;
        case 2:markers_45b3d0();break;
        case 3:counter_45b7d0();break;
        case 4:gates_draw_45ca70();break;
        case 5:car_hearts_45d190();break;
        default:break;
        }
        if(u32(0x78026cu)==0x10u){face_45bdf0();result_sheet_4603f0();road_props_45b2a0();}
        goal_models(u32(0x78024cu)!=0u);
    }

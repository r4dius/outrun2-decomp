    // ==== the network message dialog, screen 46 (441C90 -> 4926F0, vtable 5C1C2C) ==========
    // A message panel (+70, 48C490) over the text 65D00C, a menu (+34, 4ED950) of its answers
    // (yes / no, or one OK); the message code [65E00C] decides what an answer does (the
    // session left, a page change, a retry). +1320 a close pending, +1321 its kind, +1322 the
    // text too long for the panel (shown as a scrolled box, 65E018 its first line, 65E014 more
    // below). Included inside struct N.
    std::uint32_t message_new_441c90(){
        const std::uint32_t o=call(0x5802cfu,{0x1324u});      // operator new
        return o?message_4926f0(o):0u;
    }
    std::uint32_t message_4926f0(std::uint32_t o){
        method(0x48f480u,o);p32(o,0x5c1c2cu);menu_4ed950(o+0x34u);panel_48c490(o+0x70u);p32(o+8u,0x2eu);
        return o;
    }
    void message_free_492750(std::uint32_t o){
        p32(o,0x5c1c2cu);panel_free_48c520(o+0x70u);menu_free_4edab0(o+0x34u);method(0x48f4d0u,o);
    }
    std::uint32_t message_delete_492b70(std::uint32_t o,std::uint32_t flags){
        message_free_492750(o);if(flags&1u)call(0x5801a7u,{o});return o;
    }
    std::uint32_t message_suspend_492b50(std::uint32_t o){menu_clear_4eda60(o+0x34u);return vcall(o+0x70u,0x10);}
    // 434C60(ecx = 988F40, answer): the pending query answered: +16D0 = 3 (0) / 4 (1), the
    // request words +3E0 / +3E4 copied to +16B8 / +16BC, +16C4 = answer, its event (+4) raised.
    void query_answer_434c60(std::uint32_t o,std::uint32_t v){
        if(v==0u)p32(o+0x16d0u,3);else if(v==1u)p32(o+0x16d0u,4);
        p32(o+0x16b8u,u32(o+0x3e0u));p32(o+0x16bcu,u32(o+0x3e4u));
        p8(o+0x16cdu,0);p32(o+0x16c4u,v);p32(o+0x16dcu,0);
        clear_event_48f8e0(o+4u);p32(o+4u,1);
    }
    // 434E00: the session's leave (slot 1 with 0), or a new session (98A650) opened by 454180(1, 0).
    void query_leave_434e00(){
        const std::uint32_t s=u32(0x7d68acu);
        if((s&&u8(s+5u))||(net_flags()&1u)){vcall(u32(0x7d68acu),4,{0u});return;}
        if(!u32(0x7d68acu))p32(0x7d68acu,0x98a650u);
        p32(0x659934u,0xffffffffu);call(0x454180u,{1u,0u});
    }
    // 4927B0 (init): the panel opened on the message (the fixed scroll box when it is 0x400
    // bytes or more), the answers menu under its text.
    std::uint32_t message_init_4927b0(std::uint32_t o){
        using driving::X87;
        const std::uint32_t panel=o+0x70u,menu=o+0x34u;
        p8(o+0x1320u,0);p8(o+0x1321u,0);p32(o+4u,u32(0x65d008u));
        vslot(panel,1);                                                  // 48C5B0
        std::uint32_t len=0;while(u8(0x65d00cu+len))++len;
        if(len>=0x400u){
            const std::uint32_t a[9]={0x6255d5u,0x6255d5u,0u,fb(80.0f),fb(129.0f),fb(480.0f),fb(222.0f),u32(0x65e010u),0u};
            panel_open_48d0c0(panel,a);
            p8(o+0x1322u,1);p8(0x65e014u,1);p32(0x65e018u,0);
        }else{
            p8(o+0x1322u,0);
            const std::uint32_t a[9]={0x6255d5u,0x65d00cu,0u,u32(0x65cff4u),u32(0x65cff8u),u32(0x65cffcu),u32(0x65d000u),u32(0x65e010u),0u};
            panel_open_48d0c0(panel,a);
        }
        const float width=float(std::int32_t(i16(panel+0x36u)-0x25));  // 48CA90
        menu_init_4ecfb0(menu,0x65cfc4u,fb(width),1u,0u,0x60u);
        p8(o+0x47u,0);
        const bool scrolled=u8(o+0x1322u)!=0;
        const std::uint32_t extra=std::int32_t(u32(0x65e00cu))>=0x18?0xcu:0x1eu;
        float y;
        if(!scrolled){
            panel_fit_48cef0(panel,1u,extra);
            y=driving::x87_float(X87(float(i32(panel+0x12acu)))+X87(f32(0x5c1c44u)));               // 48CAF0 + 13
        }else{
            panel_fit_48cef0(panel,0u,extra);
            std::uint16_t h=u16(panel+0x38u);if(u8(panel+0x42u)||u8(panel+0x41u))h=std::uint16_t(h-0x20u);
            const float t=float(std::int32_t(std::int16_t(h))-0x50);                                      // 48CAA0
            y=driving::x87_float(X87(f32(panel+0x1284u))+X87(f32(0x5c1858u))+X87(t)
                                 -X87(f32(extra==0xcu?0x6281e8u:0x628074u)));                            // 48CAE0 + t - 12 / 17
        }
        const float x=driving::x87_float(X87(f32(panel+0x1280u))+X87(f32(0x628074u)));                // 48CAD0
        p32(menu+0x20u,fb(x));p32(menu+0x24u,fb(y));                                                      // 4ED8A0
        p32(menu+0x1cu,0x41880000u);                                                                      // 4ED880(17)
        p32(menu+0x28u,u32(0x65e010u)+1u);                                                                // 4ED8C0
        const std::int32_t code=i32(0x65e00cu);
        auto text=[&](std::uint32_t id){return call(0x465eb0u,{id});};
        if(code>=0x1e){menu_add_4ed160(menu,text(0x20fu),0);return 1;}
        if(code>=0x18){menu_add_4ed160(menu,text(0x1f3u),0);return 1;}
        menu_add_4ed160(menu,text(0x1f1u),0);menu_add_4ed160(menu,text(0x1f2u),0);
        if(code==3||code==0xd){                                                                           // 4ED930(1): "no" selected
            if(i32(menu)<i32(menu+4u)&&i32(menu)>-1)p32(menu,1);
        }
        return 1;
    }
    // 492A00: 5 when the close may go on (not a LAN variant and no close kind, or the
    // network not busy [830C00] and the owner idle 441260), else 0.
    std::uint32_t message_close_ready_492a00(std::uint32_t o){
        const std::uint32_t v=u32(0x780258u);
        if(v==3u||v==4u||u8(o+0x1321u)){
            if(u8(0x830c00u))return 0;
            if(method(0x441260u,owner())&0xffu)return 0;
        }
        return 5;
    }
    // 492A40: back to the title: 450230(7), the mode 0x22 (2SP) or 0x1B, 4409C0, 4EDCE0(8) on 659930.
    void message_title_492a40(){
        call(0x450230u,{7u});
        call(0x43f8c0u,{(call(0x43f860u)&0xffu)?0x22u:0x1bu});
        call(0x4409c0u);
        method(0x4edce0u,0x659930u,{8u});
    }
    // 492A80: the "no" / cancel answer by message code.
    std::uint32_t message_cancel_492a80(std::uint32_t o){
        const std::uint32_t code=u32(0x65e00cu);
        if(code>0x1eu)return 2;
        static constexpr std::uint8_t Kind[31]{0,1,2,3,7,7,7,7,7,7,7,2,7,7,2,4,7,7,2,5,2,2,2,2,2,2,2,2,2,6,2};
        switch(Kind[code]){
        case 0:p32(0x7d68d8u,0x19);p8(0x98a5f4u,0);method(0x434c60u,0x988f40u,{0u});return 2;
        case 1:p8(0x98a5f4u,0);return 2;
        case 3:method(0x442f20u,owner(),{1u,1u});return 2;
        case 4:case 5:p32(o+4u,0x21);return 4;
        case 6:p32(o+4u,0x20);return 4;
        default:return 2;
        }
    }
    // 492B90 (control): a pending close first, then the input (48F5F0): A answers (yes by
    // message code, or 492A80 on the second item), B cancels, up / down move the menu or
    // scroll the long text.
    std::uint32_t message_control_492b90(std::uint32_t o){
        const std::uint32_t action=vslot(o,5,{0xffffffffu});
        if(u8(o+0x1320u)){
            if(u32(0x65994cu)){
                if(message_close_ready_492a00(o)==5u){
                    if(u8(o+0x1321u)){p8(o+0x1321u,0);p32(0x65994cu,1);p8(o+0x1320u,0);return 2;}
                    message_title_492a40();p8(o+0x1320u,0);return 2;
                }
            }else{
                if(method(0x48f970u,0x659930u)&0xffu){
                    method(0x440e00u,owner(),{1u});
                    for(std::uint32_t k=0;;++k){const auto ch=u8(0x830c20u+k);p8(0x830c10u+k,ch);if(!ch)break;}
                }
                p8(o+0x1320u,0);return 2;
            }
        }
        const std::uint32_t menu=o+0x34u;
        switch(action){
        case 0:{
            call(0x4249f0u,{0x40u});
            const std::uint32_t item=u32(menu);
            if(item==1u)return message_cancel_492a80(o);
            if(item!=0u)return 0;
            const std::uint32_t code=u32(0x65e00cu);
            if(code>0x1eu)return 0;
            static constexpr std::uint8_t Kind[31]{0,1,2,3,11,11,11,11,11,11,11,11,11,11,4,11,11,11,11,5,6,11,7,8,9,9,9,9,9,10,9};
            switch(Kind[code]){
            case 0:p32(0x7d68d8u,0x19);p8(0x98a5f4u,0);method(0x434c60u,0x988f40u,{1u});return 2;
            case 1:{
                const std::uint32_t s=u32(0x7d68acu);p8(0x98a5f4u,0);
                if(s&&u8(s+5u)){
                    if(s==0x98a650u&&!u8(s+7u)&&(method(0x524100u,0x7dcbc8u,{0x989328u})&0xffu)){p32(0x7d68d8u,1);return 2;}
                    p32(0x7d68d8u,0x18);
                }else p32(0x7d68d8u,0x16);
                method(0x440e00u,owner(),{1u});call(0x454220u,{1u});method(0x434e00u,0x988f40u);
                p32(o+4u,0xd);return 2;}
            case 2:p8(0x672df7u,1);return 2;
            case 3:{
                method(0x442f20u,owner(),{1u,1u});
                for(std::uint32_t k=0;;++k){const auto ch=u8(0x7c23e0u+k);p8(0x830c10u+k,ch);if(!ch)break;}
                const std::uint32_t s=u32(0x7d68acu);
                if(!s||!u8(s+5u)){call(0x454240u);return 2;}
                const bool host_page=s==0x98a650u&&u32(owner()+0x218u)==3u;
                call(0x454220u,{1u});p32(0x7d68d8u,host_page?0x15u:0x13u);vcall(u32(0x7d68acu),4,{1u});
                return 2;}
            case 4:method(0x48ff00u,0x659930u,{0x12u});return 2;
            case 5:p32(0x7d68d8u,0x19);method(0x491fe0u,0x830d48u,{4u,0u});return 2;
            case 6:{
                const std::uint32_t page=method(0x442ec0u,owner());
                p32(0x7d68d8u,0x16);
                if(page==8u){call(0x454220u,{1u});call(0x454100u,{0u});}
                else call(0x454100u,{1u});
                return 2;}
            case 7:method(0x491fe0u,0x830d48u,{0u,0u});return 2;
            case 8:p32(0x8a8cacu,1);return 2;
            case 9:return 2;
            case 10:p32(o+4u,0x20);return 4;
            default:return 0;
            }
        }
        case 1:call(0x4249f0u,{0u});return message_cancel_492a80(o);
        case 2:
            if(u8(o+0x1322u)&&u32(menu)==0u){const std::int32_t t=i32(0x65e018u);if(t>0)p32(0x65e018u,std::uint32_t(t-1));return 0;}
            menu_prev_4ed250(menu);return 0;
        case 4:
            if(u8(o+0x1322u)&&u8(0x65e014u)){p32(0x65e018u,u32(0x65e018u)+1u);return 0;}
            menu_next_4ed2a0(menu);return 0;
        default:return 0;
        }
    }
    // 492FB0 (display): the menu, the panel (48D420 then 48C5F0) and the scrolled text
    // (48CE20) when the message is long; nothing while the owner holds a page over a long text.
    void message_display_492fb0(std::uint32_t o){
        if(i32(owner()+0x518u)>0&&u8(o+0x1322u))return;
        menu_draw_4ed3e0(o+0x34u);
        const std::uint32_t panel=o+0x70u;
        vslot(panel,2);vslot(panel,3);
        if(u8(o+0x1322u))p8(0x65e014u,message_scroll_48ce20(panel,0x65d00cu,u32(0x65e018u),1u)?1u:0u);
    }
    // 48CE20(text, first line, narrow): the panel body as a box (48E6D0) from the panel's
    // inner corner; narrow keeps 12 pixels for the scroll marks.
    std::uint32_t message_scroll_48ce20(std::uint32_t panel,std::uint32_t text,std::uint32_t first,std::uint32_t narrow){
        auto cvtt=[](float f){return (f>-2147483904.0f&&f<2147483648.0f)?std::int32_t(f):std::int32_t(0x80000000u);};
        const std::int32_t left=cvtt(f32(panel+0x1280u)+f32(0x628074u));
        const std::int32_t top=cvtt(f32(panel+0x1284u)+f32(0x5c1858u));
        std::int32_t right=std::int32_t(i16(panel+0x36u))+left-0x25;
        std::uint16_t h=u16(panel+0x38u);if(u8(panel+0x42u)||u8(panel+0x41u))h=std::uint16_t(h-0x20u);
        const std::int32_t bottom=std::int32_t(std::int16_t(h))-i32(panel+0x12a8u)+top-0x50;
        if(narrow&0xffu)right-=0xc;
        p32(panel+0x954u,1);
        return method(0x48e6d0u,panel+0x4d4u,{text,std::uint32_t(left),std::uint32_t(top),std::uint32_t(right),std::uint32_t(bottom),first});
    }

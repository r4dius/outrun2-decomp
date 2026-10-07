    // ==== the network objects' static constructors (the __xc entries 594ED0..5954B0) ======
    void atexit(std::uint32_t fn){call(0x580195u,{fn});}
    // 48F910(ecx = object): the event base: vtable 627C30, +4 / +8 = 0, +C = -1, +10 = 0.
    void event_base_48f910(std::uint32_t o){p32(o,0x627c30u);p32(o+4u,0);p32(o+8u,0);p32(o+0xcu,0xffffffffu);p32(o+0x10u,0);}
    // 48F900(ecx = object): its event (+4..+C) cleared.
    void event_clear_48f900(std::uint32_t o){p32(o+4u,0);p32(o+8u,0);p32(o+0xcu,0xffffffffu);}
    // 525F80(ecx = handle): a reference (+4 its counted object) released.
    void release_525f80(std::uint32_t h){
        p32(h,0);
        const std::uint32_t r=u32(h+4u);
        if(r){p32(r+4u,u32(r+4u)-1u);if(!u32(r+4u)&&u32(h+4u))vcall(u32(h+4u),0,{1u});}
        p32(h+4u,0);
    }
    void release_slot(std::uint32_t a){if(u32(a)){release_525f80(u32(a));p32(a,0);}}
    void delete_slot(std::uint32_t a){if(const std::uint32_t o=u32(a)){vcall(o,0,{1u});p32(a,0);}}
    // 4545D0(ecx = session): the session base (vtable 627C08): names and flags cleared,
    // +54 = -1, [7D68D8] / [7DD130] = 0.
    void session_base_4545d0(std::uint32_t o){
        p32(o,0x627c08u);
        for(std::uint32_t k=0;k<0x16u;k+=2)p16(o+0x40u+k,0);
        for(std::uint32_t k=0;k<8u;k+=2)p16(o+0x86u+k,0);
        p32(o+0xcu,0);p8(o+9u,0);p8(o+4u,0);p8(o+5u,0);p8(o+7u,0);p32(o+0x14u,0);p32(o+0x58u,0);p32(o+0x5cu,0);
        p32(0x7d68d8u,0);p32(o+0x10u,0);p8(o+6u,0);p32(o+0x54u,0xffffffffu);p32(0x7dd130u,0);
    }
    // 491CA0(ecx = object): an event object with vtable 5C1C18, +14 = 0.
    void event_object_491ca0(std::uint32_t o){event_base_48f910(o);p32(o,0x5c1c18u);p32(o+0x14u,0);}
    // 433270(ecx = player): the player object 7D6958 + k * 0x870: ids, port 0xA1EF, the
    // packet buffer (+29C = +2AC, 0x518 bytes, stream 627C48).
    void player_433270(std::uint32_t o){
        p16(o,0);p16(o+0x48u,0);
        p16(o+0x20u+0xb0u,0);p16(o+0xa8u+0xb0u,0);p16(o+0xa8u+0x138u,0);
        p16(o+0x248u,0xa1ef);
        for(std::uint32_t k=0;k<8u;k+=2)p16(o+0x264u+k,0);
        p32(o+0x2a4u,0x518);p32(o+0x29cu,o+0x2acu);p32(o+0x2a8u,0);p32(o+0x2a0u,0);p32(o+0x298u,0x627c48u);
        p32(o+0x7e8u,0);
    }
    // 454550(ecx = 7DCBE0): the name words of the player slots cleared.
    void names_454550(std::uint32_t o){
        p16(o+0x28u,0);
        for(std::uint32_t k=0;k<6u;++k)p16(o+0xb0u+k*0x88u,0);
        p16(o+0x28u+7u*0x88u,0);p16(o+0x28u+8u*0x88u,0);p16(o+0xb0u+8u*0x88u,0);
    }
    // 5240C0 / 524120(ecx = address): 8 / 16 bytes of 01.
    void ones(std::uint32_t o,std::uint32_t n){for(std::uint32_t k=0;k<n;k+=4)p32(o+k,0x01010101u);}
    // 522240(ecx = entry): a lobby list entry (+18 = 1.0, +14 = -1).
    void list_entry_522240(std::uint32_t o){
        pf(o+0x18u,f32(0x62806cu));p32(o+0xcu,0);p32(o+0x10u,0);p32(o+0x14u,0xffffffffu);
        p32(o+0x1cu,0);p32(o+0x20u,0);pf(o+0x24u,0.0f);p32(o+0x28u,0);
    }
    // 433B80(ecx = lobby): its search state cleared, the four handles released (+16D4,
    // +3D8, +16D8 references, +3DC an object), its event cleared.
    void lobby_reset_433b80(std::uint32_t o){
        p32(o+0x16d0u,0);p8(o+0x16ccu,0);p8(o+0x16cdu,0);p8(o+0x16ceu,0);p8(o+0x16b4u,0);p8(o+0x16e4u,0);p8(o+0x16cfu,0);
        for(std::uint32_t k:{0x16b0u,0x16dcu,0x16e0u,0x16b8u,0x16bcu,0x16c4u,0x16c0u,0x16c8u})p32(o+k,0);
        release_slot(o+0x16d4u);release_slot(o+0x3d8u);release_slot(o+0x16d8u);delete_slot(o+0x3dcu);
        event_clear_48f900(o);
    }
    // 433AD0(lobby): the lobby 988F40 (vtable 627E18): twenty 0x30 entries, 433B80.
    void lobby_433ad0(std::uint32_t o){
        event_base_48f910(o);p32(o,0x627e18u);
        for(std::uint32_t k=0;k<0x14u;++k)ones(o+0x39u+k*0x30u,8);
        ones(o+0x3e8u,8);
        for(std::uint32_t k=0;k<0x14u;++k){const std::uint32_t e=o+0x20u+k*0x30u;p32(e-8u,0);p32(e,0);p32(e+4u,0);p8(e+0x21u,1);}
        p32(o+0x16d4u,0);p32(o+0x3d8u,0);p32(o+0x16d8u,0);p32(o+0x3dcu,0);p8(o+0x16b5u,0);
        lobby_reset_433b80(o);
    }
    // 435FF0(ecx = request): a record request reset (state 9, handles released).
    void request_reset_435ff0(std::uint32_t o){
        p8(o+0x395u,0);p8(o+0x394u,0);p8(o+0x396u,0);p32(o+0x38cu,0);p32(o+0x390u,0);
        for(std::uint32_t k:{0x24u,0x28u,0x2cu,0x18u,0x1cu,0x20u})release_slot(o+k);
        delete_slot(o+0x30u);
        p32(o+0x14u,9);
        event_clear_48f900(o);
    }
    // 437380(ecx = query): its handles released.
    void query_reset_437380(std::uint32_t o){
        for(std::uint32_t k:{0x20u,0x24u,0x28u})release_slot(o+k);
        delete_slot(o+0x2cu);delete_slot(o+0x30u);
        p32(o+0x38u,0);
        event_clear_48f900(o);
    }
    void init_objects(){
        // 595140..5954B0: the players, the slot names, the 25 lobby slots, 830BEC, the
        // service 83612C, the 250 list entries 85B358, the crypto provider 85DED8
        for(std::uint32_t k=0;k<10u;++k)player_433270(0x7d6958u+k*0x870u);
        names_454550(0x7dcbe0u);p32(0x7dd130u,0);
        for(std::uint32_t k=0;k<0x19u;++k){ones(0x7dbdc8u+k*0x90u-8u,8);ones(0x7dbdc8u+k*0x90u,16);}
        event_base_48f910(0x830becu);p32(0x830becu,0x5c1be8u);p8(0x830becu+0x14u,0);atexit(0x595650u);
        event_base_48f910(0x83612cu);p32(0x83612cu+0x14u,0);p8(0x83612cu+0x1au,0);p8(0x83612cu+0x18u,0);p32(0x83612cu+0x24u,0);
            p32(0x83612cu,0x5c1c78u);atexit(0x595670u);
        for(std::uint32_t k=0;k<0xfau;++k)list_entry_522240(0x85b358u+k*0x2cu);
        p32(0x85ded8u,1);atexit(0x595870u);   // CryptAcquireContext(&85DED8, 0, 0, 1, 0xF0000000): the one fake provider
        // 594ED0..5950C0: the LAN lobby objects 988DB4..98AB70
        event_base_48f910(0x988db4u);p32(0x988db4u,0x627bfcu);atexit(0x595520u);
        session_base_4545d0(0x988dc8u);p32(0x988dc8u,0x627bc8u);atexit(0x595530u);
        event_base_48f910(0x988e78u);p32(0x988e78u,0x627c3cu);atexit(0x595540u);
        event_base_48f910(0x988e8cu);p32(0x988e8cu,0x627c4cu);p16(0x988e8cu+0x20u,0xa1ef);p32(0x988e8cu+0x14u,0);atexit(0x595550u);
        event_object_491ca0(0x988eb4u);p32(0x988eb4u,0x627c58u);atexit(0x595560u);
        event_base_48f910(0x988ed4u);for(std::uint32_t k:{0x14u,0x18u,0x20u,0x1cu})p32(0x988ed4u+k,0);p8(0x988ed4u+0x24u,0);p32(0x988ed4u,0x627c6cu);atexit(0x595570u);
        event_base_48f910(0x988efcu);p32(0x988efcu,0x627d40u);p32(0x988efcu+0x14u,0);atexit(0x595580u);
        event_base_48f910(0x988f1cu);for(std::uint32_t k:{0x14u,0x18u,0x1cu,0x20u})p32(0x988f1cu+k,0);p32(0x988f1cu,0x627d50u);atexit(0x595590u);
        lobby_433ad0(0x988f40u);atexit(0x5955a0u);
        event_object_491ca0(0x98a628u);p32(0x98a628u+0x24u,0);p8(0x98a628u+0x20u,0);p32(0x98a628u,0x627ea4u);atexit(0x5955b0u);
        session_base_4545d0(0x98a650u);p32(0x98a650u,0x627eb8u);p32(0x98a650u+0xacu,0);atexit(0x5955c0u);
        {const std::uint32_t o=0x98a708u;event_base_48f910(o);p32(o,0x627ee0u);for(std::uint32_t k=0x18u;k<=0x30u;k+=4)p32(o+k,0);request_reset_435ff0(o);atexit(0x5955d0u);}
        {const std::uint32_t o=0x98aaa0u;event_base_48f910(o);p32(o,0x627efcu);for(std::uint32_t k=0x20u;k<=0x30u;k+=4)p32(o+k,0);query_reset_437380(o);atexit(0x5955e0u);}
        event_base_48f910(0x98aae0u);p32(0x98aae0u,0x627fccu);atexit(0x5955f0u);
        {const std::uint32_t o=0x98ab00u;event_base_48f910(o);p32(o+0x14u,0x628004u);p32(o,0x627fecu);p32(o+0x14u,0x627fd8u);
            p32(o+0x2cu,0);p8(o+0x6au,0);p32(o+0x30u,0);p32(o+0x58u,0xffffffffu);atexit(0x595600u);}
        {const std::uint32_t o=0x98ab70u;event_base_48f910(o);p32(o,0x628058u);p32(o+0x48u,0);p32(o+0x50u,0);p8(o+0x54u,0);atexit(0x595610u);}
    }

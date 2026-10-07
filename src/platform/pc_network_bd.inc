    // ==== Demonware (bd) basics the LAN code uses: bdAlloc / bdFree, bdString (a
    // counted buffer: -C references, -8 length, -4 capacity), bdInAddr, bdArray (+0 data,
    // +4 capacity, +8 size) ===============================================================
    std::uint32_t bd_alloc(std::uint32_t n){return call(0x5231d0u,{n});}
    void bd_free(std::uint32_t p){call(0x523200u,{p});}
    // 523D60(ecx = string, text): a new string (capacity rounded up to 64 with the 0).
    void string_523d60(std::uint32_t s,std::uint32_t text){
        std::uint32_t len=0;while(u8(text+len))++len;
        if(!len){p8(u32(0x7246b4u),0);p32(s,0x7246b0u);p32(0x7246a4u,u32(0x7246a4u)+1u);return;}
        const std::uint32_t cap=((len+1u)>>6)+(((len+1u)&0x3fu)?1u:0u);
        const std::uint32_t b=bd_alloc((cap<<6)+0xcu);
        p32(b+8u,cap<<6);p32(b+4u,len);p32(b,1);
        for(std::uint32_t i=0;i<=len;++i)p8(b+0xcu+i,u8(text+i));
        p32(s,b+0xcu);
    }
    // 523CF0(ecx = string, from): a shared copy.
    void string_copy_523cf0(std::uint32_t s,std::uint32_t from){const std::uint32_t d=u32(from);p32(s,d);p32(d-0xcu,u32(d-0xcu)+1u);}
    // 523DF0(ecx = string): one reference dropped, the buffer freed at 0.
    void string_free_523df0(std::uint32_t s){
        const std::uint32_t b=u32(s)-0xcu;
        p32(b,u32(b)-1u);
        if(!u32(b))bd_free(b);
    }
    // 45AAE0 / 45AB50(ecx = array, n): room for n more (the capacity grows by max(n,
    // capacity)); the elements copied (strings 523CF0 / addresses) and the old ones freed.
    void array_grow(std::uint32_t arr,std::uint32_t n,bool strings){
        const std::uint32_t cap=u32(arr+4u);
        const std::uint32_t grown=cap+(n<=cap?cap:n);
        const std::uint32_t data=bd_alloc(grown*4u);
        const std::uint32_t old=u32(arr),size=u32(arr+8u);
        for(std::uint32_t i=0;i<size;++i){
            if(strings)string_copy_523cf0(data+i*4u,old+i*4u);
            else{p32(data+i*4u,0xff00ff00u);p32(data+i*4u,u32(old+i*4u));}
        }
        for(std::uint32_t i=0;i<size;++i){
            if(strings)string_free_523df0(old+i*4u);
            else p32(old+i*4u,0xdeadbeefu);
        }
        bd_free(old);
        p32(arr,data);p32(arr+4u,grown);
    }
    // 45AAA0(ecx = array of strings): every string freed, then the data; emptied.
    void strings_clear_45aaa0(std::uint32_t arr){
        const std::uint32_t d=u32(arr);
        for(std::uint32_t i=0;i<u32(arr+8u);++i)string_free_523df0(d+i*4u);
        bd_free(u32(arr));
        p32(arr,0);p32(arr+8u,0);p32(arr+4u,0);
    }
    void push_address(std::uint32_t arr,std::uint32_t v){
        if(u32(arr+8u)==u32(arr+4u))array_grow(arr,1,false);
        const std::uint32_t at=u32(arr)+u32(arr+8u)*4u;
        if(at){p32(at,0xff00ff00u);p32(at,v);}                        // 4586A0: 524D00 copy
        p32(arr+8u,u32(arr+8u)+1u);
    }
    void push_string(std::uint32_t arr,std::uint32_t text){
        Frame L(*this,0x10);
        string_523d60(L(0),text);
        if(u32(arr+8u)==u32(arr+4u))array_grow(arr,1,true);
        const std::uint32_t at=u32(arr)+u32(arr+8u)*4u;
        if(at)string_copy_523cf0(at,L(0));                            // 458650: 523CF0 copy
        p32(arr+8u,u32(arr+8u)+1u);
        string_free_523df0(L(0));
    }
    // 4F52E0(count): the race start records (850B1C: count * 0x20 bytes) and state.
    void race_start_init_4f52e0(std::uint32_t count){
        p32(0x850b1cu,call(0x580c33u,{count<<5}));
        p32(0x850b0cu,0);p8(0x850b04u,0);p8(0x850b05u,0);p8(0x850b2cu,0);p32(0x850b10u,0);
        p32(0x850b18u,1);
    }
    // 494270(ecx = service, x): the service starts (state 1, event 1, [7D68D8] = 2).
    void service_start_494270(std::uint32_t svc){
        p32(svc+0x14u,1);p32(0x7d68d8u,2);p8(0x7d68c7u,1);p8(0x7d68c8u,0);
        clear_event_48f8e0(svc+4u);p32(svc+4u,1);
    }
    // 45ACB0: the network start (417740): the race start records, bd memory (525340), the
    // service list (494730: no address, the two Demonware STUN names), the Demonware
    // client (45AC10), the service started (494270) and the LAN globals cleared.
    void startup_45acb0(){
        race_start_init_4f52e0(6);
        call(0x525340u,{1u});
        const std::uint32_t list=service_list_494730();
        push_address(list+0x18u,0xff00ff00u);                          // 524CD0: an empty bdInAddr
        strings_clear_45aaa0(service_list_494730()+8u);
        push_string(service_list_494730()+8u,0x5a9278u);                    // "outrun2.stun.eu.demonware.net"
        push_string(service_list_494730()+8u,0x5a9258u);                    // "outrun2.stun.us.demonware.net"
        client_45ac10();
        service_start_494270(0x83612cu);
        p8(0x7df108u,0);p8(0x7df10fu,0);p8(0x7df10du,0);p32(0x7ee230u,0);p32(0x7df34cu,0);p32(0x7f1844u,1);
    }
    // 494730: the service list 836100, built once (guard bit 0 of [836128]; atexit 595680).
    std::uint32_t service_list_494730(){
        if(!(u32(0x836128u)&1u)){
            p32(0x836128u,u32(0x836128u)|1u);
            service_list_4944a0(0x836100u);
            atexit(0x595680u);
        }
        return 0x836100u;
    }
    // 4944A0(ecx = list): the Demonware service list: ports 0xC02, the four server names
    // (5C1D28 / 5C1D10 / 5C1CF8 / 5C1CE0), no address, +24 from [628244].
    void service_list_4944a0(std::uint32_t o){
        p8(o,0);p16(o+2u,0xc02);p32(o+4u,0);
        p32(o+8u,0);p32(o+0xcu,0);p32(o+0x10u,0);
        p32(o+0x18u,0);p32(o+0x1cu,0);p32(o+0x20u,0);
        for(std::uint32_t name:{0x5c1d28u,0x5c1d10u,0x5c1cf8u,0x5c1ce0u})push_string(o+8u,name);
        p16(o+0x14u,0xc02);pf(o+0x24u,f32(0x628244u));
    }
    // 5253D0(n): the power of two at or above n.
    static std::uint32_t pow2_5253d0(std::uint32_t n){
        std::uint32_t a=n;a|=a>>1;a|=a>>2;a|=a>>4;a|=a>>8;a|=a>>16;
        a&=~(a>>1);
        return a==n?a:a<<1;
    }
    // 52FD20 / 531C10(ecx = map, n, load): a bdHashMap: +4 buckets (power of two), +8 the
    // load factor, +C the resize threshold (buckets * load, _ftol2), +10 the zeroed buckets.
    void hashmap(std::uint32_t o,std::uint32_t n,float load){
        p32(o,0);
        const std::uint32_t size=pow2_5253d0(n);
        p32(o+4u,size);pf(o+8u,load);
        driving::X87 v=driving::X87(std::int32_t(size));
        if(std::int32_t(size)<0)v=v+driving::X87(f32(0x628070u));
        p32(o+0xcu,std::uint32_t(driving::x87_ftol64(v*driving::X87(load))));
        const std::uint32_t b=bd_alloc(size*4u);
        p32(o+0x10u,b);
        for(std::uint32_t k=0;k<size*4u;++k)p8(b+k,0);
    }
    // 567C10 / 534090: a pointer array grown (capacity + max(n, capacity), raw copy) / pushed.
    void ptr_grow_567c10(std::uint32_t arr,std::uint32_t n){
        const std::uint32_t cap=u32(arr+4u);
        const std::uint32_t grown=cap+(n>cap?n:cap);
        const std::uint32_t d=bd_alloc(grown*4u);
        for(std::uint32_t i=0;i<u32(arr+8u);++i)p32(d+i*4u,u32(u32(arr)+i*4u));
        bd_free(u32(arr));
        p32(arr+4u,grown);p32(arr,d);
    }
    void ptr_push_534090(std::uint32_t arr,std::uint32_t v){
        if(u32(arr+8u)==u32(arr+4u))ptr_grow_567c10(arr,1);
        p32(u32(arr)+u32(arr+8u)*4u,v);p32(arr+8u,u32(arr+8u)+1u);
    }
    // 438C60: the bd cleanup registry 85DEB0 (vtable 627FFC, +4 its function array, +10
    // closed), made once and listing its own cleanup 438CF0.
    std::uint32_t registry_438c60(){
        if(u32(0x85deb0u))return u32(0x85deb0u);
        const std::uint32_t r=bd_alloc(0x14);
        if(r){p32(r,0x627ffcu);p32(r+4u,0);p32(r+8u,0);p32(r+0xcu,0);p8(r+0x10u,0);}
        p32(0x85deb0u,r);
        const std::uint32_t reg=registry_438c60();
        if(!u8(reg+0x10u))ptr_push_534090(reg+4u,0x438cf0u);
        return u32(0x85deb0u);
    }
    // 533510(ecx = object, owner array): a bd task object (vtables 5ECD44 / 5ECD3C / 5ECD28)
    // with its hash map (+24), registered in the owner's array.
    void task_533510(std::uint32_t o,std::uint32_t arr){
        p32(o,0x5ecf20u);p32(o+4u,0x5ec984u);p32(o+8u,0x628004u);
        p32(o,0x5ecd44u);p32(o+4u,0x5ecd3cu);p32(o+8u,0x5ecd28u);
        for(std::uint32_t k=0xcu;k<=0x20u;k+=4)p32(o+k,0);
        hashmap(o+0x24u,4,0.75f);
        p8(o+0x3cu,1);p32(o+0x40u,arr);
        ptr_push_534090(arr,o);
    }
    // 531060(ecx = client): the Demonware client (vtable 5EC878): its service list (+4), a
    // hash map (+34), the task array (+50) and one task (+5C), the rest zero.
    void client_531060(std::uint32_t o){
        p32(o,0x5ec878u);
        service_list_4944a0(o+4u);
        p32(o+0x2cu,0);p32(o+0x30u,0);
        hashmap(o+0x34u,4,0.75f);
        p32(o+0x4cu,0);
        p32(o+0x50u,0);p32(o+0x54u,0);p32(o+0x58u,0);
        task_533510(o+0x5cu,o+0x50u);
        for(std::uint32_t k=0xa0u;k<=0xc0u;k+=4)p32(o+k,0);
    }
    // 45AC10: the client 85DED0, made once (its cleanup 4586F0 registered).
    std::uint32_t client_45ac10(){
        if(u32(0x85ded0u))return u32(0x85ded0u);
        const std::uint32_t o=bd_alloc(0xc4);
        if(o)client_531060(o);
        p32(0x85ded0u,o);
        const std::uint32_t reg=registry_438c60();
        if(!u8(reg+0x10u))ptr_push_534090(reg+4u,0x4586f0u);
        return u32(0x85ded0u);
    }

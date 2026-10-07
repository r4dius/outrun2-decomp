// Included inside the compare_pc dispatcher; uses its private x86 harness.
if(only=="frontend_rankings_probe"||only=="frontend_ghosts_probe"||only=="frontend_courses_probe"){
    // Same harness for key 37 (4CEBF0 Time Attack board): list at +298.
    // Key 38 (4D05E0 Time Attack course board): list at +3DC.
    const bool ghosts=only=="frontend_ghosts_probe",courses=only=="frontend_courses_probe";
    const unsigned LB=courses?0x3dcu:ghosts?0x298u:0x33cu,Leave=courses?0x3d4u:ghosts?0x292u:0x334u,Result=courses?0x3d8u:ghosts?0x294u:0x338u,Focus=(courses||ghosts)?0x34u:0x331u;
    const unsigned EInit=courses?0x4d01a0u:ghosts?0x4cece0u:0x4caf80u,EControl=courses?0x4d06d0u:ghosts?0x4cef30u:0x4cb480u,
        EDisplay=courses?0x4cfb10u:ghosts?0x4ce3b0u:0x4cc390u,ESuspend=courses?0x4cfb40u:ghosts?0x4ce3e0u:0x4ca6f0u;
    // Original key-36 owner (course select + local ranking board): 4CAF80
    // init, 4CB480 control, 4CC390 display, 4CA6F0 suspend with the whole
    // 51B9F0..51F460 list, 4CA740/4CAA10/4CAB60/4CAD70/4CB270/4CA9B0, the
    // record fetch 51D0F0 over a random common save (7B17F8), 4474E0 unlock
    // bits, the real carousel 51B7B0/51BDB0/51BE30/51BB10/51BBA0/51BC30, the
    // CRT sprintf/vsnprintf/strncpy, 4287B0 matrices and 442F20. Stubs record
    // every UI resource call (465250/465860/465970/4659F0 with the resource
    // offset), 440EA0 labels, 4249F0 sounds and, while displaying, 48F3C0 /
    // 4EDB90 / 42D280; the native side records the same stream.
    // Not compared: the title-base input axes (+0C..+34, 48F4F0 is stubbed),
    // the internals of UI resource blocks (fake guest handles; only +8
    // validity and the drawn matrices), two bytes (list +2E/+2F) that 51D0F0
    // leaves uninitialised, and while displaying the scroll texts' +0C/+10
    // (42C370 widths need the font atlas, stubbed to 0 in the guest).
    using namespace outrun::platform;
    constexpr unsigned base=0x4d000000,root=base+0x30000,trace=base+0x34000,strings=base+0x5c000,code=0x4d060000;
    if(argc<=7||std::string(argv[7])=="-")throw std::runtime_error("rankings probe requires the pinned d3dx9_29.dll (D3DXMatrixTranslation 4393A6)");
    init_matrix_oracle_globals();wire_d3dx_imports(map_d3dx29(argv[7]));
    map_at(base,0x60000,PROT_READ|PROT_WRITE);map_at(code,0x10000,PROT_READ|PROT_WRITE|PROT_EXEC);map_original_arithmetic_bridge(file);
    for(std::uint32_t page=0x62f000u;page<0x98d000u;page+=0x1000u)mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE);
    unsigned stub_index{};
    auto install=[&](unsigned pc,const std::vector<std::uint8_t>& c){const unsigned dst=code+256*stub_index++;
        r060_raw_write(dst,c.data(),c.size());std::array<std::uint8_t,5> jump{{0xe9,0,0,0,0}};
        const auto delta=dst-pc-5;std::memcpy(jump.data()+1,&delta,4);r060_raw_write(pc,jump.data(),jump.size());};
    auto imm=[](std::vector<std::uint8_t>& c,unsigned v){for(unsigned j=0;j<4;++j)c.push_back(std::uint8_t(v>>(8*j)));};
    auto ret=[](std::vector<std::uint8_t>& c,unsigned pop){if(pop){c.push_back(0xc2);c.push_back(std::uint8_t(pop));c.push_back(0);}else c.push_back(0xc3);};
    // kind: 0 plain, 1 this-relative; side: 0 none, 1 release (+8=-1), 2 configure (+8=next handle).
    auto logger=[&](unsigned pc,unsigned nargs,unsigned pop,unsigned kind,unsigned side){
        std::vector<std::uint8_t> log{0x8b,0x15};imm(log,trace);log.insert(log.end(),{0xc1,0xe2,6,0xc7,0x82});imm(log,trace+64);imm(log,pc);
        if(kind){log.insert(log.end(),{0x89,0xc8,0x2d});imm(log,base);}else log.insert(log.end(),{0x31,0xc0});
        log.insert(log.end(),{0x89,0x82});imm(log,trace+68);
        for(unsigned i=0;i<nargs;++i){log.insert(log.end(),{0x8b,0x44,0x24,std::uint8_t(4+4*i),0x89,0x82});imm(log,trace+72+4*i);}
        log.insert(log.end(),{0xff,0x05});imm(log,trace);
        std::vector<std::uint8_t> c;
        if(kind){c.insert(c.end(),{0x81,0xf9});imm(c,base+0x3c);c.insert(c.end(),{0x0f,0x84});imm(c,unsigned(log.size()));}   // carousel: native bypasses the hook
        c.insert(c.end(),log.begin(),log.end());
        if(side==1)c.insert(c.end(),{0xc7,0x41,8,0xff,0xff,0xff,0xff});
        if(side==2){c.push_back(0xa1);imm(c,trace+4);c.insert(c.end(),{0x89,0x41,8,0xff,0x05});imm(c,trace+4);}
        ret(c,pop);install(pc,c);};
    auto value=[&](unsigned pc,unsigned v,unsigned pop){std::vector<std::uint8_t> c{0xb8};imm(c,v);ret(c,pop);install(pc,c);};
    logger(0x465250,0,0,1,1);logger(0x465860,11,0x2c,1,2);logger(0x465970,0,0,1,0);logger(0x4659f0,0,0,1,0);
    logger(0x440ea0,8,0x20,0,0);logger(0x4249f0,1,0,0,0);
    {std::vector<std::uint8_t> c{0xa1};imm(c,trace+8);c.push_back(0xc3);install(0x4652e0,c);}
    value(0x4035f0,root,0);value(0x465eb0,strings,0);value(0x42c370,0,0);value(0x42ca60,0,0);
    std::strcpy(reinterpret_cast<char*>(strings),"Total Players: %d");
    // 48F5F0 is re-installed per case (scripted action).
    unsigned stub_48f5f0=stub_index++;
    auto action_stub=[&](unsigned a){std::vector<std::uint8_t> c{0xb8};imm(c,a);ret(c,4);const unsigned dst=code+256*stub_48f5f0;
        r060_raw_write(dst,c.data(),c.size());std::array<std::uint8_t,5> jump{{0xe9,0,0,0,0}};const auto delta=dst-0x48f5f0u-5;std::memcpy(jump.data()+1,&delta,4);r060_raw_write(0x48f5f0,jump.data(),5);};
    auto display_stubs=[&]{logger(0x48f3c0,0,0,1,0);logger(0x4edb90,0,0,1,0);logger(0x42d280,6,0,0,0);};
    display_stubs();
    unsigned cases{},mismatches{};const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
    auto check=[&](bool ok,const std::string& label){++cases;if(!ok){++mismatches;if(mismatches<25)std::cerr<<"rankings "<<cases<<" "<<label<<char(10);}};
    // Native fixtures.
    std::vector<std::array<std::uint32_t,14>> events;
    auto effect=[](void* p,unsigned id){static_cast<std::vector<std::array<std::uint32_t,14>>*>(p)->push_back({0x4249f0,0,id});return true;};
    FrontendFontPack fonts{};fonts.fonts[9].token=9;fonts.fonts[9].width=16;fonts.fonts[9].height=16;fonts.fonts[9].first=0;
    FrontendTextTable text;{
        std::vector<std::uint8_t> t;auto put=[&](unsigned v){for(unsigned j=0;j<4;++j)t.push_back(std::uint8_t(v>>(8*j)));};
        const unsigned entries=0x344,first=8+(entries+1)*4;const char* word="Total Players: %d";const unsigned bytes=first+2*18;
        put(0x74657874);put(bytes);for(unsigned i=0;i<entries;++i)put(first);put(0);
        for(const char* p=word;;++p){t.push_back(std::uint8_t(*p));t.push_back(0);if(!*p)break;}
        if(!text.parse(t.data(),t.size()))throw std::runtime_error("rankings text table");
    }
    std::mt19937 rng(0x36363636u);
    auto rnd=[&](unsigned n){return unsigned(rng()%n);};
    // Common save bytes 0..15: every record field the offline board reads
    // stays in the ranges of its 51C710/5E70F8 lookups (car = flags >> 2,
    // class < 16, column < 0x70), names have no '%' and times stay positive.
    // Every name slot of the nine tables (natural index ranges) keeps a NUL
    // in its 16 bytes: 51D0F0 copies the name without terminating it and
    // 48F280 would read 51D0F0's uninitialised tail otherwise.
    auto fill_common=[&](std::array<std::uint8_t,PcCommonSaveBytes>& c){for(auto& b:c)b=std::uint8_t(rng()&15);
        struct T{unsigned name,stride,count;};
        constexpr T tables[]{{4,0x20,100},{0xc84,0x20,20},{0xf04,0x20,100},{0x1b84,0x20,100},{0x2804,0x30,200},
            {0x4d84,0x1c,620},{0x9154,0x58,40},{0x9f14,0x30,200},{0xc494,0x1c,650}};
        for(const auto& t:tables)for(unsigned e=0;e<t.count;++e){const unsigned at=t.name+e*t.stride+rnd(16);if(at<c.size())c[at]=0;}};
    struct Guest {unsigned g68fa84{};std::array<std::uint8_t,0x20> area{};};
    const std::vector<unsigned> Resources=courses?std::vector<unsigned>{0x3c,0x150,0x1f0,0x290,0x330,LB+0x26120}:ghosts?std::vector<unsigned>{0x3c,0x150,0x1f0,LB+0x26120}:std::vector<unsigned>{0x3c,0x150,0x1f0,0x290,LB+0x26120};
    auto is_resource=[&](unsigned off){
        for(unsigned r:Resources)if(off>=r&&off<r+0xa0)return true;
        if(off>=LB+0x30&&off<LB+0x30+30*0x144c){const unsigned k=(off-LB-0x30)%0x144c;if(k>=0xe0c)return true;}
        return false;};
    auto run_case=[&](const char* what,FrontendRankings& c,FrontendSprites& sprites,std::array<std::uint8_t,0xe00>& owner,
                      FrontendRankingGlobals& g,unsigned entry,unsigned native_result,unsigned mask_extra){
        const auto guest_eax=guest_call.out_eax;(void)entry;
        Bytes nb(c.object.data(),c.object.size()),gb(reinterpret_cast<void*>(base),c.object.size());
        bool same=true;unsigned first_diff=~0u;
        for(unsigned off=0;off<c.object.size();++off){
            if(off>=0xc&&off<0x34)continue;
            if(off==LB+0x2e||off==LB+0x2f)continue;
            if(is_resource(off)){const unsigned r=off;(void)r;continue;}
            if(mask_extra){const unsigned k=off-(LB+0x26670);if(off>=LB+0x26670&&off<LB+0x26670+15*0x8c&&((k%0x8c)>=0xc&&(k%0x8c)<0x14))continue;}
            if(nb.u8(off)!=gb.u8(off)){same=false;if(first_diff==~0u)first_diff=off;}
        }
        std::ostringstream o;o<<what<<" object"<<std::hex<<" first diff +"<<first_diff<<" native "<<(first_diff!=~0u?unsigned(nb.u8(first_diff)):0)<<" guest "<<(first_diff!=~0u?unsigned(gb.u8(first_diff)):0);
        check(same,o.str());
        // Resource handle validity.
        auto valid=[&](Bytes b,unsigned off){return b.i32(off+8)!=-1;};
        bool handles=true;unsigned bad=0;
        for(unsigned r:Resources)if(valid(nb,r)!=valid(gb,r)){handles=false;bad=r;}
        for(unsigned i=0;i<30;++i)for(unsigned k=0;k<10;++k){const unsigned off=LB+0x30+i*0x144c+0xe0c+k*0xa0;if(off==LB+0x30+i*0x144c+0xeac)continue;if(valid(nb,off)!=valid(gb,off)){handles=false;bad=off;}}
        {std::ostringstream h;h<<what<<" resource handle validity +"<<std::hex<<bad;check(handles,h.str());}
        // Drawn matrices of displayed records.
        bool matrices=true;
        // Only records below the count are ticked (51C160); a record shown past
        // the count keeps whatever matrix its sprite had (not modelled by the stub).
        for(unsigned i=0;i<15&&int(i)<nb.i32(LB);++i)for(unsigned k:{0xe0cu,0xf4cu,0xfecu,0x108cu,0x112cu,0x11ccu,0x126cu,0x130cu,0x13acu}){
            const unsigned off=LB+0x130a4+i*0x144c+k;const int nh=nb.i32(off+8),gh=gb.i32(off+8);if(nh<0||gh<0)continue;
            const auto* s=sprites.get(unsigned(nh));if(!s){matrices=false;continue;}
            if(std::memcmp(s->matrix.data(),reinterpret_cast<void*>(0x95e058u+unsigned(gh)*0x7cu),64)){
                if(matrices&&std::getenv("OR2_RK_VERBOSE")){const auto* gm=reinterpret_cast<const float*>(0x95e058u+unsigned(gh)*0x7cu);
                    std::cerr<<"  matrix rec "<<i<<std::hex<<" +"<<k<<std::dec<<" nh "<<nh<<" gh "<<gh<<" native "<<s->matrix[12]<<","<<s->matrix[13]<<","<<s->matrix[0]<<" guest "<<gm[12]<<","<<gm[13]<<","<<gm[0]<<char(10);}
                matrices=false;}}
        if(!matrices&&std::getenv("OR2_RK_VERBOSE")){
            std::cerr<<"  count "<<nb.i32(LB)<<" B[5]+4 "<<nb.i32(LB+0x130a4+5*0x144c+4)<<" y "<<nb.f32(LB+0x130a4+5*0x144c+0x10)<<" guest y "<<gb.f32(LB+0x130a4+5*0x144c+0x10)<<char(10);
            const unsigned n=*reinterpret_cast<unsigned*>(trace);
            for(unsigned i=0;i<n;++i){const auto* e=reinterpret_cast<const std::uint32_t*>(trace+64+64*i);
                if(e[1]>=LB+0x130a4+5*0x144c+0xe0c&&e[1]<LB+0x130a4+5*0x144c+0xeac)std::cerr<<"   guest #"<<i<<std::hex<<" "<<e[0]<<std::dec<<char(10);}
        }
        check(matrices,std::string(what)+" drawn matrices (4287B0)");
        check(!std::memcmp(owner.data(),reinterpret_cast<void*>(root),0x51c),std::string(what)+" owner object");
        check(!std::memcmp(g.area_85b308.data(),reinterpret_cast<void*>(0x85b308),0x20),std::string(what)+" 85B308..85B327");
        check(g.cursor_68fa84==*reinterpret_cast<std::int32_t*>(0x68fa84),std::string(what)+" 68FA84");
        check(g.cursor_690e7c==*reinterpret_cast<std::int32_t*>(0x690e7c)&&g.cursor_691534==*reinterpret_cast<std::int32_t*>(0x691534),std::string(what)+" 690E7C/691534");
        check(g.variant_84b0f8==*reinterpret_cast<std::uint8_t*>(0x84b0f8),std::string(what)+" 84B0F8");
        const unsigned count=*reinterpret_cast<unsigned*>(trace);
        bool trace_ok=count==events.size();unsigned at=~0u;
        for(unsigned i=0;trace_ok&&i<count;++i){const auto* e=reinterpret_cast<const std::uint32_t*>(trace+64+64*i);
            if(std::memcmp(e,events[i].data(),14*4)){trace_ok=false;at=i;}}
        {std::ostringstream t;t<<what<<" call trace ("<<count<<" guest, "<<events.size()<<" native";
            if(at!=~0u){const auto* e=reinterpret_cast<const std::uint32_t*>(trace+64+64*at);t<<std::hex<<"; #"<<std::dec<<at<<std::hex<<" guest "<<e[0]<<"@"<<e[1]<<" "<<e[2]<<" "<<e[3]<<" native "<<events[at][0]<<"@"<<events[at][1]<<" "<<events[at][2]<<" "<<events[at][3];}
            t<<")";check(trace_ok,t.str());}
        check(guest_eax==native_result,std::string(what)+" result");
    };
    unsigned latched{},confirmed{},backed{},scrolled{},moved{},filters{},rows_seen{},images_seen{};
    for(unsigned run=0;run<48;++run){
        // Mode bits: modes 1..6 (0x20C >> 5), low bits 0..3, with and without 0x800.
        const unsigned m=courses?(run%4==3?5:4):ghosts?4+run%2:1+run%6,low=m==1?(run/6)%4:(run/6)%2;
        std::array<std::uint8_t,0xe00> owner{};Bytes rb(owner.data(),owner.size());
        for(unsigned k=0;k<0x51c;k+=4)rb.put32(k,run*0x01010101u+k);
        rb.put32(0x20c,(m<<5)|low|((run&8)?0x800:0)|(courses?((run*7)%12)<<12:0u)|(rb.u32(0x20c)&~(courses?0x3ffffu:0xfffu)));
        PcLicense profile{};for(auto& b:profile)b=std::uint8_t(rng());
        auto common=std::make_unique<std::array<std::uint8_t,PcCommonSaveBytes>>();fill_common(*common);
        FrontendSprites sprites;sprites.bind_timing(0x44,std::vector<FrontendSpriteTiming>(224,{400,60}));
        FrontendUiResources ui{sprites};ui.effect_user=&events;ui.effect_4249f0=effect;PcUiNotifyGlobals globals;
        FrontendInputSnapshot input;unsigned repeat{},result{};
        ui.commands(0x442ac0,rb.sub(0x51c,owner.size()-0x51c),nullptr,0,globals,result);
        FrontendRankingGlobals g{};for(auto& b:g.area_85b308)b=std::uint8_t(rng());g.cursor_68fa84=int(rnd(6));g.cursor_690e7c=int(rnd(6));g.cursor_691534=int(rnd(6));g.variant_84b0f8=std::uint8_t(rnd(2));
        // Key 38 course bits as its confirm writes them: column + 6 for the second set.
        if(courses)rb.put32(0x20c,(rb.u32(0x20c)&~0x3f000u)|((rnd(6)+(g.variant_84b0f8?6u:0u))<<12));
        FrontendRankings c;unsigned guest_counter=100;
        FrontendRankingServices s{ui,rb,globals,profile,*common,input,repeat,g,&fonts,&text,3.5f};
        s.trace=&events;s.trace_base=c.object.data();
        auto sync_globals=[&]{
            std::memcpy(reinterpret_cast<void*>(root),owner.data(),owner.size());
            std::memcpy(reinterpret_cast<void*>(0x7c23e0),profile.data(),profile.size());
            std::memcpy(reinterpret_cast<void*>(0x7b17f8),common->data(),common->size());
            std::memcpy(reinterpret_cast<void*>(0x85b308),g.area_85b308.data(),0x20);
            *reinterpret_cast<std::int32_t*>(0x68fa84)=g.cursor_68fa84;*reinterpret_cast<std::int32_t*>(0x690e7c)=g.cursor_690e7c;
            *reinterpret_cast<std::int32_t*>(0x691534)=g.cursor_691534;*reinterpret_cast<std::uint8_t*>(0x84b0f8)=g.variant_84b0f8;
            *reinterpret_cast<std::uint8_t*>(0x84b0f6)=g.ghost_84b0f6;*reinterpret_cast<std::uint8_t*>(0x7d68bc)=g.online_7d68bc;
            *reinterpret_cast<std::uint8_t*>(0x7d68d2)=g.online_7d68d2;*reinterpret_cast<float*>(0x842110)=s.timer;
            std::memset(reinterpret_cast<void*>(trace),0,0x28000);*reinterpret_cast<unsigned*>(trace+4)=1345;   // guest handles 1345.. never collide with native ones
            (void)guest_counter;events.clear();};
        // The guest continues from the native object, so its drawn matrices
        // start from the native sprite matrices of the same handles.
        auto sync_object=[&]{std::memcpy(reinterpret_cast<void*>(base),c.object.data(),c.object.size());
            Bytes nb(c.object.data(),c.object.size());
            for(unsigned i=0;i<15;++i)for(unsigned k=0xe0c;k<0x144c;k+=0xa0){const int h=nb.i32(LB+0x130a4+i*0x144c+k+8);
                if(h<0||h>=int(FrontendSprites::Count))continue;std::memcpy(reinterpret_cast<void*>(0x95e058u+unsigned(h)*0x7cu),sprites.get(unsigned(h))->matrix.data(),64);}};
        check(courses?frontend_courses_construct_4d05e0(c,s):ghosts?frontend_ghosts_construct_4cebf0(c,s):frontend_rankings_construct_4cb300(c,s),"native construct "+std::to_string(s.missing));
        // Carousel readiness (4652E0 after the 4659F0 tick) as the native side will see it.
        auto ready=[&]{FrontendSprites copy=sprites;auto obj=c.object;FrontendUiResources u{copy};Bytes r(obj.data()+0x3c,0xa0);unsigned out{},x{};
            u.call(0x4659f0,r,nullptr,0,x);u.call(0x4652e0,r,nullptr,0,out);return out;};
        sync_globals();sync_object();events.clear();
        if(const char* callpc=std::getenv("OR2_RK_CALL")){   // debugging: one guest call on the constructed owner
            prepare(std::uint32_t(std::strtoul(callpc,nullptr,16)));guest_call.ecx=base+std::uint32_t(std::strtoul(std::getenv("OR2_RK_ECX")?std::getenv("OR2_RK_ECX"):"0",nullptr,16));
            if(const char* a=std::getenv("OR2_RK_ARGS")){auto* st=reinterpret_cast<std::uint32_t*>(S);unsigned k=0;std::string list=a;std::size_t at=0;
                while(at<list.size()){auto e=list.find(',',at);if(e==std::string::npos)e=list.size();st[k++]=std::uint32_t(std::strtoul(list.substr(at,e-at).c_str(),nullptr,16));at=e+1;}}
            run_original32();std::cerr<<"call ok eax="<<std::hex<<guest_call.out_eax<<" trace "<<*reinterpret_cast<unsigned*>(trace)<<std::dec<<char(10);return 0;}
        if(const char* stop=std::getenv("OR2_RK_STOP")){   // debugging: return to the harness at an address
            std::vector<std::uint8_t> c{0xbc};imm(c,S-4);c.push_back(0xc3);const unsigned dst=code+0xff00;r060_raw_write(dst,c.data(),c.size());
            const unsigned at=std::uint32_t(std::strtoul(stop,nullptr,16));std::array<std::uint8_t,5> jump{{0xe9,0,0,0,0}};const auto delta=dst-at-5;std::memcpy(jump.data()+1,&delta,4);r060_raw_write(at,jump.data(),5);
            prepare(EInit);guest_call.ecx=base;run_original32();std::cerr<<"reached "<<stop<<" trace "<<*reinterpret_cast<unsigned*>(trace)<<char(10);return 0;}
        if(courses&&m!=4){   // key 38 is only opened with mode 4; other modes pass 51B7B0 no table and must latch
            ++latched;check(!frontend_courses_init_4d01a0(c,s)&&s.missing==0x4cfce6,"course init outside mode 4 not latched");continue;}
        prepare(EInit);guest_call.ecx=base;run_original32();
        guest_counter=*reinterpret_cast<unsigned*>(trace+4);
        const bool init_ok=courses?frontend_courses_init_4d01a0(c,s):ghosts?frontend_ghosts_init_4cece0(c,s):frontend_rankings_init_4caf80(c,s);
        check(init_ok,"native init fault "+std::to_string(s.missing));
        if(inject&&run==(courses?2u:3u))c.object[LB+0x130a4+0x10]^=1;
        guest_call.out_eax&=0xff;
        run_case("init",c,sprites,owner,g,EInit,1,0);
        if(!init_ok)continue;
        // Random walk over the input actions, comparing after every step.
        constexpr unsigned masks[]{4,8,0x400,0x1000,0x800,0x2000,0x10,0x20,0x8000,0};
        for(unsigned step=0;step<40;++step){
            const unsigned pick=rnd(20);const unsigned action=pick<10?pick:(pick<13?2+2*rnd(2):pick<16?0:pick<18?3+2*rnd(2):12);
            const unsigned a=action>=10?12:action;
            unsigned b=a;if(courses&&pick==19)b=11;
            repeat=b;input.feature_mask=b<9?masks[b]:0;input.device_held=b==11?0x2000u:0u;
            s.timer=3.5f+0.0625f*float(step+1);
            sync_globals();sync_object();action_stub(b);*reinterpret_cast<unsigned*>(trace+8)=ready();events.clear();
            prepare(EControl);guest_call.ecx=base;run_original32();guest_counter=*reinterpret_cast<unsigned*>(trace+4);
            s.ui.missing_pc=0;const bool ok=courses?frontend_courses_control_4d06d0(c,s,result):ghosts?frontend_ghosts_control_4cef30(c,s,result):frontend_rankings_control_4cb480(c,s,result);
            check(ok,"native control fault "+std::to_string(s.missing)+" action "+std::to_string(a));
            run_case(("control "+std::to_string(b)).c_str(),c,sprites,owner,g,EControl,result,0);
            if(!ok)break;
            confirmed+=c.object[Leave]&&Bytes(c.object.data(),c.object.size()).u32(Result)==1;backed+=c.object[Leave]&&Bytes(c.object.data(),c.object.size()).u32(Result)==2;scrolled+=a==0&&c.object[Focus]!=0;moved+=a==3||a==5;filters+=a==6||a==8||b==11;
            rows_seen+=unsigned(Bytes(c.object.data(),c.object.size()).i32(LB));
            if(c.object[Leave]){c.object[Leave]=0;}     // keep walking after a confirm/back
            // Display every few steps.
            if(step%4==3){
                sync_globals();sync_object();events.clear();
                prepare(EDisplay);guest_call.ecx=base;run_original32();
                const bool shown=courses?frontend_courses_display_4cfb10(c,s):ghosts?frontend_ghosts_display_4ce3b0(c,s):frontend_rankings_display_4cc390(c,s);
                check(shown,"native display fault "+std::to_string(s.missing));
                run_case("display",c,sprites,owner,g,EDisplay,guest_call.out_eax,1);
                images_seen+=unsigned(c.images.size());
            }
        }
        sync_globals();sync_object();events.clear();
        prepare(ESuspend);guest_call.ecx=base;run_original32();
        check(courses?frontend_courses_suspend_4cfb40(c,s):ghosts?frontend_ghosts_suspend_4ce3e0(c,s):frontend_rankings_suspend_4ca6f0(c,s),"native suspend");
        run_case("suspend",c,sprites,owner,g,ESuspend,guest_call.out_eax,0);
    }
    std::cout<<"{\"routine\":\""<<only<<"\",\"coverage\":{\"latched_other_modes\":"<<latched<<",\"confirms\":"<<confirmed<<",\"backs\":"<<backed<<",\"focus_scrolls\":"<<scrolled
             <<",\"carousel_moves\":"<<moved<<",\"filter_steps\":"<<filters<<",\"rows_listed\":"<<rows_seen<<",\"score_digit_images\":"<<images_seen
             <<"},\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}\n";return mismatches?1:0;
}

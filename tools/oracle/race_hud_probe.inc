// Included inside the compare_pc dispatcher; uses its private x86 harness.
// Race HUD events (race_hud.hpp): 398 SPRANI, 389 GAD_PUB, 388 NAVI_PUB.
if(only=="race_hud_sprani_probe"||only=="race_hud_gad_pub_probe"||only=="race_hud_mission_probe"||only=="race_hud_navi_probe"){
    // Original sprite pool code (427E30 init, 428460 allocation, 427F70
    // control, 428170 display with 429010 and the pinned D3DX, 4285A0/428880,
    // 4970F0/498590/4998C0/497210) against FrontendSprites + race_hud. The
    // only substituted routine is the SPRANI renderer 429460, a capture stub
    // recording (EDI root component, frame, scale, layer) and the matrix top.
    using namespace outrun::platform;
    if(argc<=7||std::string(argv[7])=="-")throw std::runtime_error("race HUD sprite probe requires the pinned d3dx9_29.dll path");
    // Restore the original call sites the harness redirects for other oracles by default.
    set_r046_road_patches(false);set_r046_common_parent_patches(false);set_r048_parent_patches(false);
    map_original_arithmetic_bridge(file);map_original_read_page(file,0x01039000u);init_matrix_oracle_globals();wire_d3dx_imports(map_d3dx29(argv[7]));
    // 4656B0 also uses D3DXMatrixScaling: its generic body follows the export's dispatch jump.
    if(mprotect(reinterpret_cast<void*>(0x596000u),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("IAT");
    *reinterpret_cast<std::uint32_t*>(0x5962b4u)=D3DX_BASE+(0x441288u-0x400000u);mprotect(reinterpret_cast<void*>(0x596000u),4096,PROT_READ);
    constexpr unsigned base=0x3a700000,scenes=base+0x1000,trace=base+0x20000,pool_base=0x95e028u;
    map_at(base,0x60000,PROT_READ|PROT_WRITE|PROT_EXEC);
    for(auto page:{0x956000u,0x740000u,0x8a8000u,0x755000u,0x836000u,0x67e000u,0x84b000u})
        if(mprotect(reinterpret_cast<void*>(page),page==0x956000u?0x31000u:0x1000u,PROT_READ|PROT_WRITE))throw std::runtime_error("sprite globals protection");
    const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
    unsigned cases{},mismatches{},coverage{};bool injected{};
    auto check=[&](bool ok,const char* label){++cases;if(!ok||(inject&&cases==777&&only!="race_hud_sprani_probe")){++mismatches;if(mismatches<15)std::cerr<<only<<" case "<<cases<<" "<<label<<char(10);}};
    auto g32=[](unsigned a){return *reinterpret_cast<std::uint32_t*>(a);};
    auto p32=[](unsigned a,std::uint32_t v){*reinterpret_cast<std::uint32_t*>(a)=v;};
    auto bits=[](float f){std::uint32_t u;std::memcpy(&u,&f,4);return u;};
    std::mt19937 rng(0x428170u);
    // Guest banks: 9568B8[bank] -> scene pointers; scene {count 1, components};
    // root component {int16 w, int16 h, ?, float frames, float rate}.
    const unsigned banks[]{0x2bu,0x2cu,0x44u,0x48u,0x49u};
    FrontendSprites pool;unsigned cursor=scenes+0x400;
    for(unsigned bank:banks){
        std::vector<FrontendSpriteTiming> timing;const unsigned count=(only=="race_hud_mission_probe"||only=="race_hud_navi_probe")?(bank==0x2bu?0x70u:bank==0x2cu?0x120u:4u):6+rng()%10;
        const unsigned table=cursor;cursor+=count*4;
        for(unsigned i=0;i<count;++i){
            const unsigned scene=cursor;cursor+=0x40;
            const int w=int(rng()%700)+1,h=int(rng()%500)+1;const float frames=float(2+rng()%90),rate=float(rng()%3?30+rng()%40:rng()%5);
            p32(table+i*4,scene);p32(scene,1);p32(scene+4,scene+8);
            *reinterpret_cast<std::int16_t*>(scene+8)=std::int16_t(w);*reinterpret_cast<std::int16_t*>(scene+10)=std::int16_t(h);
            std::memcpy(reinterpret_cast<void*>(scene+16),&frames,4);std::memcpy(reinterpret_cast<void*>(scene+20),&rate,4);
            timing.push_back({frames,rate,float(w),float(h)});
        }
        p32(0x9568b8u+bank*4,table);
        if(!pool.bind_timing(bank,timing))throw std::runtime_error("bank timing");
    }
    // 429460 capture: records[trace+16+128*n] = {pc, edi, frame, scale, layer, ..., matrix @+0x40}.
    {
        std::vector<std::uint8_t> c;auto imm=[&](unsigned v){for(unsigned j=0;j<4;++j)c.push_back(std::uint8_t(v>>(8*j)));};
        c.insert(c.end(),{0x56,0x57,0x51,0x8b,0x15});imm(trace);c.insert(c.end(),{0x69,0xd2});imm(128);
        c.insert(c.end(),{0x8d,0x92});imm(trace+16);c.insert(c.end(),{0xc7,0x02});imm(0x429460);
        c.insert(c.end(),{0x89,0x7a,0x04});
        for(unsigned i=0;i<3;++i)c.insert(c.end(),{0x8b,0x44,0x24,std::uint8_t(0x10+4*i),0x89,0x42,std::uint8_t(8+4*i)});
        c.insert(c.end(),{0xa1});imm(0x7551b4);c.insert(c.end(),{0x89,0x42,0x14});
        c.insert(c.end(),{0xa1});imm(0x9564f4);c.insert(c.end(),{0x89,0x42,0x18});   // 428AF0 depth flags
        c.insert(c.end(),{0xa1});imm(0x9564f8);c.insert(c.end(),{0x89,0x42,0x1c});
        c.insert(c.end(),{0x8b,0x35});imm(0x89b564);c.insert(c.end(),{0x8d,0x7a,0x40,0xb9,0x10,0,0,0,0xf3,0xa5,0xff,0x05});imm(trace);
        c.insert(c.end(),{0x59,0x5f,0x5e,0xc3});
        const unsigned dst=base+0x30000;r060_raw_write(dst,c.data(),c.size());r046_write_rel32(0x429460,0xe9,dst);
    }
    auto instance=[&](unsigned h){return Bytes(reinterpret_cast<void*>(pool_base+h*0x7c),0x7c);};
    auto compare_pool=[&](const char* label,bool stale){
        bool ok=true;
        for(unsigned l=0;l<FrontendSprites::Layers;++l)if(!(g32(0x956558u+l*4)==pool.used(l)&&g32(0x956500u+l*4)==pool.next(l))){
            if(ok&&mismatches<6)std::cerr<<"layer "<<l<<" used "<<g32(0x956558u+l*4)<<"/"<<pool.used(l)<<" next "<<g32(0x956500u+l*4)<<"/"<<pool.next(l)<<char(10);ok=false;}
        check(ok,label);ok=true;
        for(unsigned h=0;h<FrontendSprites::Count;++h){
            const auto& s=pool.instances()[h];auto g=instance(h);
            if(g.u32(0)!=unsigned(s.allocated)){if(ok&&mismatches<6)std::cerr<<"handle "<<h<<" alloc "<<g.u32(0)<<"/"<<s.allocated<<char(10);ok=false;continue;}
            if(!s.allocated&&!stale)continue;
            if(!s.allocated&&stale){if(g.u32(0x24)!=s.status)ok=false;continue;}
            const auto root=g.u32(0x74);
            if(g.u32(4)!=unsigned(s.visible)||g.u32(8)!=s.token||g.u32(0xc)!=bits(s.frame)||g.u32(0x10)!=s.mode||g.u32(0x14)!=h/64||
               g.u32(0x18)!=bits(s.first)||g.u32(0x1c)!=bits(s.last)||g.u32(0x20)!=bits(s.speed)||g.u32(0x24)!=s.status||
               std::int32_t(g.u32(0x2c))!=s.id_2c||g.u32(0x78)!=s.pause_domain||
               *reinterpret_cast<std::int16_t*>(root)!=std::int16_t(s.canvas_width)||*reinterpret_cast<std::int16_t*>(root+2)!=std::int16_t(s.canvas_height)||
               g32(root+12)!=bits(s.rate)){if(ok&&mismatches<6)std::cerr<<"handle "<<h<<" frame "<<g.f32(0xc)<<"/"<<s.frame<<" status "<<g.u32(0x24)<<"/"<<s.status<<" first "<<g.f32(0x18)<<"/"<<s.first<<" last "<<g.f32(0x1c)<<"/"<<s.last<<" mode "<<s.mode<<char(10);ok=false;}
            for(unsigned k=0;k<16;++k)if(g.u32(0x30+k*4)!=bits(s.matrix[k]))ok=false;
        }
        check(ok,label);
    };
    auto create=[&](SpraniGlobals& gl){
        const unsigned bank=banks[rng()%5];FrontendSpriteTiming t{};unsigned index=rng()%16;
        if(!pool.bank_scene((bank<<16)|index,t))index=0;
        const unsigned token=(bank<<16)|index,layer=rng()%(FrontendSprites::Layers),mode=rng()%6;
        const int first=rng()%3?-1:int(rng()%8),last=rng()%3?-1:int(rng()%40);
        gl.paused_95b214=rng()%4==0;p32(0x95b214,gl.paused_95b214);
        prepare(0x428460);auto* a=reinterpret_cast<unsigned*>(S);a[0]=token;a[1]=layer;a[2]=mode;a[3]=unsigned(first);a[4]=unsigned(last);run();
        const auto h=pool.create(token,layer,mode,first,last,true,gl.paused_95b214);
        if(guest_call.out_eax!=h&&mismatches<6)std::cerr<<"alloc "<<std::hex<<token<<std::dec<<" layer "<<layer<<" guest "<<guest_call.out_eax<<" native "<<h<<char(10);
        check(guest_call.out_eax==h,"428460 handle");
        if(h==~0u)return;
        auto* s=pool.get_mutable(h);auto g=instance(h);
        if(rng()%4==0){s->visible=false;g.put32(4,0);}
        if(rng()%3==0){s->speed=float(int(rng()%9)-4)*0.75f;g.putf(0x20,s->speed);}
        if(rng()%2){for(unsigned k=0;k<16;++k){const float v=k%5==0?float(rng()%300)*0.01f:(rng()%3?0.0f:float(int(rng()%2000)-1000)*0.125f);s->matrix[k]=v;g.putf(0x30+k*4,v);}}
        if(rng()%5==0){s->id_2c=std::int32_t(rng()%1000);g.put32(0x2c,std::uint32_t(s->id_2c));}
        if(rng()%4==0){s->frame=float(int(rng()%120)-10);g.putf(0xc,s->frame);}
    };
    std::memset(reinterpret_cast<void*>(pool_base),0,0x7c*FrontendSprites::Count);std::memset(reinterpret_cast<void*>(0x956500),0,0xac);
    if(only=="race_hud_sprani_probe"){
        for(unsigned run_index=0;run_index<120;++run_index){
            SpraniGlobals gl;gl.current_7551b4=std::int32_t(rng());p32(0x7551b4,std::uint32_t(gl.current_7551b4));
            // Stale pool, then 427E30.
            const unsigned n=rng()%200;for(unsigned i=0;i<n;++i)create(gl);
            p32(0x95b214,1);gl.paused_95b214=1;prepare(0x427e30);run();sprani_init_427e30(pool,gl);
            compare_pool("427E30 pool",true);check(std::int32_t(g32(0x7551b4))==gl.current_7551b4&&g32(0x95b214)==gl.paused_95b214,"427E30 7551B4/95B214");
            for(unsigned step=0;step<12;++step){
                const unsigned what=rng()%4;
                if(what==0){const unsigned k=1+rng()%40;for(unsigned i=0;i<k;++i)create(gl);compare_pool("428460 pool",false);}
                else if(what==1){
                    const unsigned sixty=rng()%2,frame=rng()%3,limit=1,paused=rng()%3==0;
                    p32(0x740ca4,sixty);p32(0x8a8cdc,frame);p32(0x8a8ca8,limit);p32(0x95b214,paused);gl.paused_95b214=paused;
                    prepare(0x427f70);run();pool.tick(sprani_clock_427f70(sixty,std::int32_t(frame),std::int32_t(limit),paused));
                    compare_pool("427F70 pool",true);
                }else if(what==2){
                    for(unsigned i=0;i<8;++i){const unsigned h=rng()%FrontendSprites::Count;
                        prepare(0x4285a0);reinterpret_cast<unsigned*>(S)[0]=h;run();pool.release(h);}
                    compare_pool("4285A0 pool",true);
                }else{
                    reset_matrix_oracle_globals();
                    const unsigned depth=rng()%3;p32(0x89b568,depth==2?62:depth);p32(0x89b564,MatrixArena+64*(depth==2?62:depth));
                    for(unsigned k=0;k<64*66;k+=4)p32(MatrixArena+k,bits(float(int(rng()%200)-100)*0.5f));
                    std::vector<std::uint8_t> arena(0x10000);std::memcpy(arena.data(),reinterpret_cast<void*>(MatrixArena),arena.size());
                    PcMatrixStack stack{Bytes(arena.data(),arena.size()),std::ptrdiff_t(g32(0x89b564)-MatrixArena),std::int32_t(g32(0x89b568)),std::int32_t(g32(0x89b56c))};
                    p32(trace,0);p32(0x986b28,0x55);gl.flag_986b28=0x55;
                    std::array<float,3> blend{};p32(0x9564e0,7);p32(0x9564e4,7);p32(0x9564e8,7);std::uint32_t blend_bits[3]{7,7,7};
                    prepare(0x428170);run();
                    std::vector<SpraniDraw> draws;sprani_display_428170(pool,gl,stack,draws,blend);
                    if(!draws.empty())for(unsigned k=0;k<3;++k)blend_bits[k]=bits(blend[k]);
                    if(inject&&!injected&&!draws.empty()){draws[0].matrix[12]+=1.0f;injected=true;}
                    check(g32(trace)==draws.size(),"429460 draw count");coverage+=unsigned(draws.size());
                    for(std::size_t i=0;i<std::min<std::size_t>(g32(trace),draws.size());++i){
                        const unsigned r=trace+16+128*unsigned(i);const auto& d=draws[i];
                        const auto* s=pool.get(d.handle);(void)s;
                        const unsigned root=g32(r+4);
                        bool ok=g32(r+8)==bits(d.frame)&&g32(r+12)==0x3f800000u&&g32(r+16)==d.layer&&std::int32_t(g32(r+20))==d.id_7551b4&&
                            root==g32(pool_base+d.handle*0x7c+0x74);
                        for(unsigned k=0;k<16;++k)ok=ok&&g32(r+0x40+k*4)==bits(d.matrix[k]);
                        if(!ok&&mismatches<4){std::cerr<<"draw "<<i<<" frame "<<g32(r+8)<<"/"<<bits(d.frame)<<" layer "<<g32(r+16)<<"/"<<d.layer<<" id "<<g32(r+20)<<"/"<<d.id_7551b4<<std::hex;
                            for(unsigned k=0;k<16;++k)std::cerr<<" "<<g32(r+0x40+k*4)<<"/"<<bits(d.matrix[k]);std::cerr<<std::dec<<char(10);}
                        check(ok,"429460 draw record");
                    }
                    check(std::int32_t(g32(0x7551b4))==gl.current_7551b4&&g32(0x986b28)==gl.flag_986b28,"428170 7551B4/986B28");
                    check(g32(0x9564e0)==blend_bits[0]&&g32(0x9564e4)==blend_bits[1]&&g32(0x9564e8)==blend_bits[2],"429010 9564E0..E8");
                    check(g32(0x89b564)==MatrixArena+unsigned(stack.current_offset)&&std::int32_t(g32(0x89b568))==stack.depth,"matrix stack pointer/depth");
                    check(!std::memcmp(arena.data(),reinterpret_cast<void*>(MatrixArena),64*66),"matrix stack arena");
                    compare_pool("428170 pool",true);
                }
            }
        }
    }else if(only=="race_hud_mission_probe"){
        #include "race_hud_mission_probe.inc"
    }else if(only=="race_hud_navi_probe"){
        #include "race_hud_navi_probe.inc"
    }else{
        auto gad_sync=[&](const GadPubState& s){
            std::memcpy(reinterpret_cast<void*>(GadPubState::Base),s.block.data(),s.block.size());
            std::memcpy(reinterpret_cast<void*>(GadPubState::DataBase),s.data.data(),s.data.size());};
        auto gad_compare=[&](const GadPubState& s,const char* label){
            check(!std::memcmp(s.block.data(),reinterpret_cast<void*>(GadPubState::Base),s.block.size())&&
                  !std::memcmp(s.data.data(),reinterpret_cast<void*>(GadPubState::DataBase),s.data.size()),label);};
        SpraniGlobals gl;
        for(unsigned run_index=0;run_index<400;++run_index){
            // 4970F0 over random words: the native state starts as the guest bytes.
            GadPubState s;
            for(unsigned a=GadPubState::Base;a<GadPubState::Base+GadPubState::Size;a+=4)p32(a,rng());
            for(unsigned a=GadPubState::DataBase;a<GadPubState::DataBase+GadPubState::DataSize;a+=4)p32(a,rng());
            std::memcpy(s.block.data(),reinterpret_cast<void*>(GadPubState::Base),s.block.size());
            std::memcpy(s.data.data(),reinterpret_cast<void*>(GadPubState::DataBase),s.data.size());
            // Words the state does not name must be left alone: compare the whole block.
            std::array<std::uint8_t,0xe0> before;std::memcpy(before.data(),reinterpret_cast<void*>(0x836630),before.size());
            gad_compare(s,"state mirror");
            prepare(0x4970f0);run();gad_pub_init_4970f0(s);gad_compare(s,"4970F0 globals");
            // 498590 mode 16 on live sprites with scripted statuses.
            pool.clear_allocations();std::memset(reinterpret_cast<void*>(0x956500),0,0xac);
            for(unsigned h=0;h<FrontendSprites::Count;++h){instance(h).put32(0,0);instance(h).put32(0x24,pool.instances()[h].status);}
            for(unsigned i=0;i<5;++i){
                const unsigned kind=rng()%5;
                const unsigned slot=0x836640+4*i;
                if(kind==0)s.put32(slot,0xffffffffu);
                else if(kind==1)s.put32(slot,rng()%FrontendSprites::Count);
                else{create(gl);s.put32(slot,guest_call.out_eax);}
                if(s.i32(slot)>=0&&unsigned(s.i32(slot))<FrontendSprites::Count&&rng()%2){
                    const unsigned st=rng()%5;pool.get_mutable(unsigned(s.i32(slot)))->status=st;instance(unsigned(s.i32(slot))).put32(0x24,st);}
            }
            gad_sync(s);p32(0x78026c,16);p32(0x84bd00,0x12345678);float v84bd00;std::memcpy(&v84bd00,reinterpret_cast<void*>(0x84bd00),4);
            prepare(0x498590);run();std::uint32_t missing{};
            check(gad_pub_control_498590(s,pool,16,0,v84bd00,missing),"native 498590 mode 16");
            gad_compare(s,"498590 handles");check(g32(0x84bd00)==bits(v84bd00),"4EF410 84BD00");compare_pool("498590 pool",true);
            // 4998C0 in mode 16 is the RET: no global changes.
            std::memcpy(before.data(),reinterpret_cast<void*>(0x836630),before.size());
            prepare(0x4998c0);run();check(gad_pub_display_4998c0(16,missing)&&!std::memcmp(before.data(),reinterpret_cast<void*>(0x836630),before.size()),"4998C0 mode 16");
            // 497210 -> 428600.
            p32(0x7551b4,0x1234);p32(0x95b214,1);prepare(0x497210);run();gad_pub_destroy_497210(pool);compare_pool("497210 pool",true);
            check(g32(0x7551b4)==0x1234&&g32(0x95b214)==1,"428600 leaves 7551B4/95B214");
            if(inject&&run_index==5)s.put32(0x836694,s.u32(0x836694)^1u);
            gad_compare(s,"497210 globals");
        }
    }
    std::cout<<"{\"routine\":\""<<only<<"\",\"coverage\":"<<coverage<<",\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}\n";return mismatches?1:0;
}
if(only=="race_hud_snippet_probe"){
    // Resolves relocated load snippets: OR2_SNIPPETS="entry:cont,entry:cont".
    // Every .data dword holds its own address, so the captured EAX names the
    // global the snippet loads (EAX = [X] -> X).
    const char* list=std::getenv("OR2_SNIPPETS");if(!list)throw std::runtime_error("set OR2_SNIPPETS");
    constexpr unsigned base=0x3a800000,buf=base+0x100,stub=base+0x200;
    map_at(base,0x10000,PROT_READ|PROT_WRITE|PROT_EXEC);map_original_arithmetic_bridge(file);
    for(std::uint32_t page=0x62f000u;page<0x98d000u;page+=0x1000u)mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE);
    {   std::vector<std::uint8_t> c;auto imm=[&](std::uint32_t v){for(unsigned i=0;i<4;++i)c.push_back(std::uint8_t(v>>(i*8)));};
        c.push_back(0xa3);imm(buf);
        const std::uint8_t regs[]{0x0d,0x15,0x1d,0x25,0x2d,0x35,0x3d};
        for(unsigned i=0;i<7;++i){c.push_back(0x89);c.push_back(regs[i]);imm(buf+4+i*4);}
        c.push_back(0x9c);c.push_back(0x8f);c.push_back(0x05);imm(buf+32);
        c.push_back(0xbc);imm(S-4);c.push_back(0xc3);
        std::memcpy(reinterpret_cast<void*>(stub),c.data(),c.size());
    }
    const bool own=std::getenv("OR2_OWN")!=nullptr;
    std::vector<std::uint8_t> keep(0x98d000u-0x62f000u);std::memcpy(keep.data(),reinterpret_cast<void*>(0x62f000u),keep.size());
    std::string s=list;std::size_t at=0;
    while(at<s.size()){
        auto comma=s.find(',',at);if(comma==std::string::npos)comma=s.size();
        const auto item=s.substr(at,comma-at);at=comma+1;const auto colon=item.find(':');
        const auto entry=std::uint32_t(std::stoul(item.substr(0,colon),nullptr,16)),cont=std::uint32_t(std::stoul(item.substr(colon+1),nullptr,16));
        if(own)for(std::uint32_t a=0x62f000u;a<0x98d000u;a+=4)*reinterpret_cast<std::uint32_t*>(a)=a;
        auto pin=[&]{if(const char* set=std::getenv("OR2_SET")){std::string t=set;std::size_t q=0;
            while(q<t.size()){auto c2=t.find(',',q);if(c2==std::string::npos)c2=t.size();const auto it=t.substr(q,c2-q);q=c2+1;
                const auto eq=it.find('=');*reinterpret_cast<std::uint32_t*>(std::stoul(it.substr(0,eq),nullptr,16))=std::uint32_t(std::stoul(it.substr(eq+1),nullptr,16));}}};
        pin();
        std::array<std::uint8_t,5> saved{};std::memcpy(saved.data(),reinterpret_cast<void*>(cont),5);r046_write_rel32(cont,0xe9,stub);
        prepare(entry);guest_call.eax=0x11110000u;guest_call.ecx=0x22220000u;guest_call.edx=0x33330000u;guest_call.ebx=0x44440000u;
        guest_call.esi=0x55550000u;guest_call.edi=0x66660000u;
        if(const char* e2=std::getenv("OR2_EAX"))guest_call.eax=std::uint32_t(std::stoul(e2,nullptr,16));
        if(const char* e2=std::getenv("OR2_ECX"))guest_call.ecx=std::uint32_t(std::stoul(e2,nullptr,16));
        for(unsigned k=0;k<16;++k)reinterpret_cast<std::uint32_t*>(S)[k]=0x77000000u+k*4;
        if(const char* st=std::getenv("OR2_STACK")){std::string t=st;std::size_t q=0;unsigned k=0;
            while(q<t.size()&&k<16){auto c2=t.find(',',q);if(c2==std::string::npos)c2=t.size();reinterpret_cast<std::uint32_t*>(S)[k++]=std::uint32_t(std::stoul(t.substr(q,c2-q),nullptr,16));q=c2+1;}}
        run_original32();
        const auto* r=reinterpret_cast<const std::uint32_t*>(buf);
        if(std::getenv("OR2_BISECT")){
            // Which .data bytes change the captured EAX (fills of 0x01/0x55).
            auto again=[&]{pin();prepare(entry);guest_call.eax=0x11110000u;guest_call.ecx=0x22220000u;guest_call.edx=0x33330000u;guest_call.ebx=0x44440000u;
                guest_call.esi=0x55550000u;guest_call.edi=0x66660000u;for(unsigned k=0;k<16;++k)reinterpret_cast<std::uint32_t*>(S)[k]=0x77000000u+k*4;run_original32();return std::getenv("OR2_BISECT_FLAGS")?(r[8]&0x8d5u):r[0];};
            std::vector<std::uint8_t> base_image(0x98d000u-0x62f000u);std::memcpy(base_image.data(),reinterpret_cast<void*>(0x62f000u),base_image.size());
            const auto base_value=again();std::cout<<"baseline eax "<<std::hex<<base_value<<std::dec<<char(10);
            for(std::uint8_t fill:{0x01u,0x55u}){std::uint32_t lo=0x62f000u,hi=0x98d000u;bool found=true;
                for(unsigned depth=0;depth<24&&hi-lo>1;++depth){const auto mid=lo+(hi-lo)/2;
                    std::memcpy(reinterpret_cast<void*>(0x62f000u),base_image.data(),base_image.size());std::memset(reinterpret_cast<void*>(lo),fill,mid-lo);
                    if(again()!=base_value)hi=mid;else{std::memcpy(reinterpret_cast<void*>(0x62f000u),base_image.data(),base_image.size());std::memset(reinterpret_cast<void*>(mid),fill,hi-mid);
                        if(again()!=base_value)lo=mid;else{found=false;break;}}}
                std::memcpy(reinterpret_cast<void*>(0x62f000u),base_image.data(),base_image.size());
                std::cout<<"fill "<<unsigned(fill)<<(found?" sensitive ":" none/multi ")<<std::hex<<lo<<std::dec<<char(10);
                if(found)for(std::uint32_t v:{0u,1u,2u,3u,4u,5u,0x100u,0xffffffffu}){*reinterpret_cast<std::uint32_t*>(lo&~3u)=v;std::cout<<"  "<<std::hex<<(lo&~3u)<<"="<<v<<" -> "<<again()<<std::dec<<char(10);}
                std::memcpy(reinterpret_cast<void*>(0x62f000u),base_image.data(),base_image.size());if(found)break;}
        }
        std::cout<<std::hex<<entry<<"->"<<cont<<" eax="<<r[0]<<" ecx="<<r[1]<<" edx="<<r[2]<<" ebx="<<r[3]<<" ebp="<<r[5]<<" esi="<<r[6]<<" edi="<<r[7]<<" fl="<<(r[8]&0x8d5)<<std::dec<<char(10);
        {auto* page=reinterpret_cast<void*>(cont&~0xfffu);mprotect(page,8192,PROT_READ|PROT_WRITE|PROT_EXEC);std::memcpy(reinterpret_cast<void*>(cont),saved.data(),5);}
        std::memcpy(reinterpret_cast<void*>(0x62f000u),keep.data(),keep.size());
    }
    return 0;
}

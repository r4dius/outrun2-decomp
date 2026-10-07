if(only=="rob_disp_probe"){
    // Original rob_disp_init 514BF0 (514910, 513840, 514680) against
    // rob_disp_init_514bf0. The entries of 448CD0, 4066D0, 4103F0 and 406730
    // jump to guest stubs that log {id, args} (and 448CD0 answers
    // table[name & 0x7FF] from a random per-case table, -1 entries included);
    // the native service does the same. Compared: the display works
    // 8577D8..857900, the service log.
    using namespace outrun::platform;
    const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
    map_original_arithmetic_bridge(file);   // 5148D0 enters 513A30 through the bridge 1039BE8
    for(std::uint32_t page=0x62f000u;page<0x98d000u;page+=0x1000u)mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE);
    constexpr std::uint32_t Area=0x3a840000u,Stubs=0x3a850000u,LogPtr=0x3a851000u,Log=0x3a852000u,Table=0x3a860000u;
    map_at(Area,0x10000,PROT_READ|PROT_WRITE);map_at(Stubs,0x2000,PROT_READ|PROT_WRITE|PROT_EXEC);
    map_at(Log,0xe000,PROT_READ|PROT_WRITE);map_at(Table,0x2000,PROT_READ|PROT_WRITE);
    auto g32=[](std::uint32_t a){return *reinterpret_cast<const std::uint32_t*>(a);};
    auto p32=[](std::uint32_t a,std::uint32_t v){*reinterpret_cast<std::uint32_t*>(a)=v;};
    auto stub=[&](std::uint32_t at,std::uint32_t id,bool lookup){
        std::vector<std::uint8_t> b{0x8b,0x0d};auto d32=[&](std::uint32_t v){for(int k=0;k<4;++k)b.push_back(std::uint8_t(v>>(8*k)));};
        d32(LogPtr);b.insert(b.end(),{0xc7,0x01});d32(id);
        b.insert(b.end(),{0x8b,0x44,0x24,0x04,0x89,0x41,0x04,0x8b,0x44,0x24,0x08,0x89,0x41,0x08,0x8b,0x44,0x24,0x0c,0x89,0x41,0x0c,0x83,0xc1,0x10,0x89,0x0d});
        d32(LogPtr);
        if(lookup){b.insert(b.end(),{0x8b,0x44,0x24,0x04,0x25,0xff,0x07,0,0,0x8b,0x04,0x85});d32(Table);}
        else b.insert(b.end(),{0x31,0xc0});
        b.push_back(0xc3);
        std::memcpy(reinterpret_cast<void*>(at),b.data(),b.size());
    };
    stub(Stubs,0x448cd0u,true);stub(Stubs+0x100,0x4066d0u,false);stub(Stubs+0x200,0x4103f0u,false);stub(Stubs+0x300,0x406730u,false);
    r046_write_rel32(0x448cd0u,0xe9,Stubs);r046_write_rel32(0x4066d0u,0xe9,Stubs+0x100);
    r046_write_rel32(0x4103f0u,0xe9,Stubs+0x200);r046_write_rel32(0x406730u,0xe9,Stubs+0x300);
    std::mt19937 rng(0x514bf0u);
    auto m32=[](const std::vector<std::uint8_t>& v,std::uint32_t o){std::uint32_t x;std::memcpy(&x,v.data()+o,4);return x;};
    const unsigned cases=std::max(count,1u)*40u;
    unsigned mismatches=0,calls_total=0;
    auto report=[&](const std::string& what){++mismatches;if(mismatches<=12)std::cerr<<"MISMATCH "<<what<<char(10);};
    PcRaceMemory m;
    std::vector<std::uint8_t> disp(PcRobDispState::End-PcRobDispState::Base),area(0x10000),table82(0x100);
    std::vector<std::uint32_t> names(0x800);
    for(unsigned cs=0;cs<cases;++cs){
        const std::string tag=" case "+std::to_string(cs);
        for(auto& b:disp)b=std::uint8_t(rng());
        for(auto& v:names)v=rng()%5u==0u?0xffffffffu:rng();
        std::fill(area.begin(),area.end(),0);
        // Work at Area: +0 display index 0..2, +8 kind 0..22. Lists at Area+0x100.. (82F388[kind]).
        const std::uint32_t work=Area,kind=rng()%23u,index=rng()%3u;
        auto a32=[&](std::uint32_t o,std::uint32_t v){std::memcpy(area.data()+o,&v,4);};
        auto a16=[&](std::uint32_t o,std::uint16_t v){std::memcpy(area.data()+o,&v,2);};
        a32(0,index);a32(8,kind);
        for(unsigned k=0;k<23;++k){
            const std::uint32_t list=0x100u+k*0x400u;
            {const std::uint32_t v=Area+list;std::memcpy(table82.data()+k*4,&v,4);}
            const std::uint16_t n1=std::uint16_t(rng()%6u),n2=std::uint16_t(rng()%6u);
            a16(list+2,rng()%9u==0u?std::uint16_t(0xfffe):n1);a16(list+4,n2);
            a32(list+8,Area+list+0x20);a32(list+0xc,Area+list+0x100);
            for(unsigned i=0;i<8;++i)a32(list+0x20+i*8+4,rng());
            for(unsigned i=0;i<8;++i)a32(list+0x100+i*0x14+0x10,rng());
        }
        std::memcpy(reinterpret_cast<void*>(PcRobDispState::Base),disp.data(),disp.size());
        std::memcpy(reinterpret_cast<void*>(Area),area.data(),area.size());
        std::memcpy(reinterpret_cast<void*>(0x82f388u),table82.data(),23*4);
        std::memcpy(reinterpret_cast<void*>(Table),names.data(),names.size()*4);
        p32(LogPtr,Log);
        const unsigned entry_fn=rng()%3u;   // 514BF0 init, 5148D0 hand gu, 5148F0 hand pa
        prepare(entry_fn==0u?0x514bf0u:entry_fn==1u?0x5148d0u:0x5148f0u);reinterpret_cast<unsigned*>(S)[0]=work;run_original32();
        // native
        std::vector<std::uint32_t> log;
        m.clear();m.map(PcRobDispState::Base,disp.data(),disp.size());m.map(Area,area.data(),area.size());
        m.map(0x82f388u,table82.data(),23*4);rob_disp_map_rdata(m);
        std::vector<std::uint8_t> arena(0x100);outrun::driving::PcMatrixStack mats{outrun::driving::Bytes(arena.data(),arena.size()),0,0,4};
        PcRaceContext ctx{m,mats,[&](const PcRaceCall& k)->std::uint32_t{
            log.push_back(k.pc);for(unsigned i=0;i<3;++i)log.push_back(k.args[i]);
            if(k.pc==0x448cd0u)return names[k.args[0]&0x7ffu];
            return 0u;},nullptr};
        try{if(entry_fn==0u)rob_disp_init_514bf0(ctx,work);else if(entry_fn==1u)rob_disp_hand_gu_5148d0(ctx,work);else rob_disp_hand_pa_5148f0(ctx,work);}
        catch(const std::exception& e){report(std::string("native threw: ")+e.what()+tag);continue;}
        if(inject&&cs==7u)disp[0x8+index*0x60+0x50]^=1u;
        const std::uint32_t logged=(g32(LogPtr)-Log)/16u;calls_total+=logged;
        // args beyond the callee's count are stack contents on the PC: compare the used ones only
        if(logged*4u!=log.size())report("call count "+std::to_string(logged)+" vs "+std::to_string(log.size()/4)+tag);
        else for(std::uint32_t i=0;i<logged;++i){
            const auto id=g32(Log+i*16);const unsigned used=id==0x448cd0u?1u:id==0x406730u?3u:2u;
            bool ok=id==log[i*4];
            for(unsigned k=0;k<used&&ok;++k)ok=g32(Log+i*16+4+k*4)==log[i*4+1+k];
            if(!ok){std::ostringstream o;o<<"call "<<i<<" id "<<std::hex<<id;report(o.str()+tag);break;}
        }
        if(std::memcmp(reinterpret_cast<const void*>(PcRobDispState::Base),disp.data(),disp.size())){
            for(std::size_t k=0;k<disp.size();++k)if(reinterpret_cast<const std::uint8_t*>(PcRobDispState::Base)[k]!=disp[k]){
                std::ostringstream o;o<<"disp byte +"<<std::hex<<k;report(o.str()+tag);break;}}
    }
    {   // 487E10 character data relocation (eax = data, ecx = resource).
        const std::uint32_t D=Area;
        for(unsigned cs=0;cs<cases;++cs){
            std::vector<std::uint8_t> blob(0x2000);for(auto& x:blob)x=std::uint8_t(rng());
            auto b32=[&](std::uint32_t o,std::uint32_t v){std::memcpy(blob.data()+o,&v,4);};
            auto b16=[&](std::uint32_t o,std::uint16_t v){std::memcpy(blob.data()+o,&v,2);};
            b16(2,std::uint16_t(rng()%9u==0u?0x8001u:rng()%12u));b16(4,std::uint16_t(rng()%12u));
            for(std::uint32_t f=8;f<=0x1c;f+=4)b32(f,rng()%4u==0u?0u:0x100u+(rng()%0x40u)*0x10u);
            b32(8,0x400);b32(0xc,0x800);
            if(m32(blob,0x1c)){b32(0x1c,0x1000);}
            const std::uint32_t res=rng()&0xffffu;
            std::memcpy(reinterpret_cast<void*>(D),blob.data(),blob.size());
            prepare(0x487e10u);guest_call.eax=D;guest_call.ecx=res;run_original32();
            m.clear();m.map(D,blob.data(),blob.size());
            try{rob_chr_relocate_487e10(m,D,res);}catch(const std::exception& e){report(std::string("487E10 threw: ")+e.what());continue;}
            if(inject&&cs==3u)blob[0x404]^=1u;
            if(std::memcmp(reinterpret_cast<const void*>(D),blob.data(),blob.size())){
                for(std::size_t k=0;k<blob.size();++k)if(reinterpret_cast<const std::uint8_t*>(D)[k]!=blob[k]){
                    std::ostringstream o;o<<"487E10 byte +"<<std::hex<<k<<" case "<<std::dec<<cs;report(o.str());break;}}
        }
    }
    std::cerr<<"rob_disp: service calls="<<calls_total<<char(10);
    std::cout<<"{\"routine\":\"rob_disp_probe\",\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}"<<'\n';return 0;
}

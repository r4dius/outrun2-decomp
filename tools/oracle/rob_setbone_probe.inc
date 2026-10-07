if(only=="rob_setbone_probe"){
    // Original RobMotion SetBone 4F1CF0 (5206E0 release, 520620 build, 522DB0,
    // 522DD0 bone records) against rob_set_bone_4f1cf0, over synthetic bone
    // files and motion objects at 82F5F0 (same addresses on both sides). The
    // 580253 malloc / 580BC2 free call sites of 520620 / 5206E0 go to guest
    // stubs: malloc is the same bump allocator as RobotHeap (16-byte rounding,
    // fresh bytes 0xCD, range 72000000..), free is a RET. Two SetBone calls per
    // case (the second releases the first set). Compared: the motion object,
    // the heap bytes and the next heap address; the allocator stacks (7B11A8).
    using namespace outrun::platform;
    const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
    for(std::uint32_t page=0x62f000u;page<0x98d000u;page+=0x1000u)mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE);
    constexpr std::uint32_t Heap=RobotHeap::Base,HeapSize=0x100000u,Files=0x3a840000u,Stubs=0x3a850000u,NextCell=0x3a851000u;
    map_at(Heap,HeapSize,PROT_READ|PROT_WRITE);map_at(Files,0x10000,PROT_READ|PROT_WRITE);map_at(Stubs,0x2000,PROT_READ|PROT_WRITE|PROT_EXEC);
    auto g32=[](std::uint32_t a){return *reinterpret_cast<const std::uint32_t*>(a);};
    auto p32=[](std::uint32_t a,std::uint32_t v){*reinterpret_cast<std::uint32_t*>(a)=v;};
    {   // malloc(n): eax = [next]; next += max(n,1) rounded to 16; the block is filled with 0xCD.
        const std::uint8_t mal[]{0x8b,0x44,0x24,0x04,0x85,0xc0,0x75,0x05,0xb8,0x01,0,0,0, 0x83,0xc0,0x0f,0x83,0xe0,0xf0,
            0x57,0x89,0xc1,0xa1,0,0,0,0, 0x89,0xc7,0x01,0x0d,0,0,0,0, 0x50,0xb0,0xcd,0xf3,0xaa,0x58,0x5f,0xc3};
        std::vector<std::uint8_t> b(mal,mal+sizeof(mal));
        std::memcpy(b.data()+23,&NextCell,4);std::memcpy(b.data()+31,&NextCell,4);
        std::memcpy(reinterpret_cast<void*>(Stubs),b.data(),b.size());
        *reinterpret_cast<std::uint8_t*>(Stubs+0x100)=0xc3;            // free: ret
        r046_write_rel32(0x52067e,0xe8,Stubs);r046_write_rel32(0x52072b,0xe8,Stubs+0x100);
    }
    std::mt19937 rng(0x4f1cf0u);
    const unsigned cases=std::max(count,1u)*40u;
    unsigned mismatches=0,bones_total=0;
    auto report=[&](const std::string& what){++mismatches;if(mismatches<=12)std::cerr<<"MISMATCH "<<what<<char(10);};
    PcRaceMemory m;
    std::vector<std::uint8_t> motion_block(8u*0xa4u),files(0x10000);
    for(unsigned cs=0;cs<cases;++cs){
        const std::string tag=" case "+std::to_string(cs);
        // Bone file: 6 entries (16 bytes: u16 ?, u16 count, 0, records offset), records (0x38), extra index lists.
        std::fill(files.begin(),files.end(),0);
        auto w32=[&](std::uint32_t o,std::uint32_t v){std::memcpy(files.data()+o,&v,4);};
        auto w16=[&](std::uint32_t o,std::uint16_t v){std::memcpy(files.data()+o,&v,2);};
        std::uint32_t cursor=0x100;
        for(unsigned e=0;e<6;++e){
            const std::uint16_t n=std::uint16_t(1u+rng()%24u);
            w16(e*16,std::uint16_t(rng()));w16(e*16+2,n);w32(e*16+8,cursor);
            const std::uint32_t recs=cursor;cursor+=n*0x38u;
            for(std::uint32_t i=0;i<n;++i){const auto r=recs+i*0x38u;
                for(std::uint32_t k=0;k<0x30u;k+=4)w32(r+k,rng());
                w32(r,rng()%0x400u);
                const std::uint32_t links=rng()%3u==0u?0u:rng()%5u;
                w32(r+0x30,links);
                if(links){w32(r+0x34,cursor);for(std::uint32_t j=0;j<links;++j)w16(cursor+j*2,std::uint16_t(rng()%n));cursor+=((links*2u)+3u)&~3u;}
            }
            if(cursor>0xf000u)break;
        }
        std::memcpy(reinterpret_cast<void*>(Files),files.data(),files.size());
        // Motion objects 82F5F0 (random bytes, +0 id), heap reset.
        for(auto& b:motion_block)b=std::uint8_t(rng());
        const unsigned which=rng()%8u;const std::uint32_t motion=PcRaceRobotState::MotionBase+which*0xa4u;
        std::memcpy(motion_block.data()+which*0xa4u+0x58,std::array<std::uint8_t,0x44>{}.data(),0x44);   // no previous set (+1C = 0)
        std::memcpy(reinterpret_cast<void*>(PcRaceRobotState::MotionBase),motion_block.data(),motion_block.size());
        std::memset(reinterpret_cast<void*>(Heap),0,HeapSize);p32(NextCell,Heap);
        RobotHeap heap;
        PcAllocatorStateStacks stacks{};
        m.clear();m.map(PcRaceRobotState::MotionBase,motion_block.data(),motion_block.size());m.map(Files,files.data(),files.size());
        std::vector<std::uint8_t> native_heap(HeapSize,0);
        for(unsigned call=0;call<2;++call){
            const std::uint32_t entry=rng()%8u==0u?0u:Files+(rng()%6u)*16u;
            prepare(0x4f1cf0);guest_call.ecx=motion;reinterpret_cast<unsigned*>(S)[0]=entry;reinterpret_cast<unsigned*>(S)[1]=Files;run_original32();
            try{rob_set_bone_4f1cf0(m,heap,stacks,motion,entry,Files);}
            catch(const std::exception& e){report(std::string("native threw: ")+e.what()+tag);break;}
            if(entry)bones_total+=std::uint32_t(std::int16_t(m.u16(motion+0x68)));
        }
        if(inject&&cs==7u)motion_block[which*0xa4u+0x60]^=1u;
        for(const auto& b:heap.blocks)std::memcpy(native_heap.data()+(b.base-Heap),b.bytes.data(),b.bytes.size());
        if(std::memcmp(reinterpret_cast<const void*>(PcRaceRobotState::MotionBase),motion_block.data(),motion_block.size())){
            for(std::size_t k=0;k<motion_block.size();++k)if(reinterpret_cast<const std::uint8_t*>(PcRaceRobotState::MotionBase)[k]!=motion_block[k]){
                std::ostringstream o;o<<"motion byte +"<<std::hex<<k;report(o.str()+tag);break;}}
        if(g32(NextCell)!=heap.next)report("heap next"+tag);
        const std::uint32_t used=heap.next-Heap;
        if(std::memcmp(reinterpret_cast<const void*>(Heap),native_heap.data(),used)){
            for(std::uint32_t k=0;k<used;++k)if(reinterpret_cast<const std::uint8_t*>(Heap)[k]!=native_heap[k]){
                std::ostringstream o;o<<"heap byte +"<<std::hex<<k;report(o.str()+tag);break;}}
    }
    std::cerr<<"SetBone: bones built="<<bones_total<<char(10);
    std::cout<<"{\"routine\":\"rob_setbone_probe\",\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}"<<'\n';return 0;
}

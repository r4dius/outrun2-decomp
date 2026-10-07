// Included inside the compare_pc dispatcher; uses its private x86 harness.
if(only=="autoscene_table_observe"){
    // Measures the protected 51B740(scene, k) (VM entry): the AUTOSCENE per-scene callbacks
    // (k 0 control, 1 display, 2 the third slot) for scene ids 0..0x80.
    {   const std::size_t text_file=pe_file_offset(file,0x401000u);
        for(std::uint32_t a=0x401000u;a<0x401000u+0x194881u;a+=0x100u){
            const std::size_t n=std::min<std::size_t>(0x100u,0x401000u+0x194881u-a);
            if(std::memcmp(reinterpret_cast<void*>(a),file.data()+text_file+(a-0x401000u),n))r060_raw_write(a,file.data()+text_file+(a-0x401000u),n);
        }
    }
    map_original_arithmetic_bridge(file);
    for(std::uint32_t id=0;id<0x80u;++id){
        std::ostringstream s;s<<std::hex<<"scene "<<id<<":";bool any=false;
        for(std::uint32_t k=0;k<3u;++k){prepare(0x51b740u);reinterpret_cast<unsigned*>(S)[0]=id;reinterpret_cast<unsigned*>(S)[1]=k;run();
            s<<" "<<guest_call.out_eax;any=any||guest_call.out_eax;}
        if(any)std::cout<<s.str()<<std::endl;
    }
    return 0;
}

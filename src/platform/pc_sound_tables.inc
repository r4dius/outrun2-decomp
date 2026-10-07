// Static tables of the PC sound driver (42F0D0 / 427700 / 42EFF0 / 42F330),
// built from the player's EXE image when it is installed (formerly generated
// by tools/extract_sound_tables.py).
// Sound ids (7763E0 + id*20): rate word +8, volume category word +C;
// voices 773868 / 774170 (ICS buffers) [0 = 1]; ICS base rate word 7790FE + id*8.
PcSoundIdInfo kSoundIds[577]{};
// Banks 6242E8 + bank*12: file name and the ids 427700 maps to the slot ({count, ids...}).
PcSoundBankInfo kSoundBanks[169]{};
std::uint16_t kSoundBankIds[2213]{};
// ICS definitions 74D2F0 (count at 74D2EC; {id, ?, layer count, ?, layers}) and their 0xCC-byte layers.
PcIcsDefinition kIcsDefinitions[25]{};
std::uint8_t kIcsLayers[130][0xcc]{};
void build_sound_tables(){
    const auto in_image=[](std::uint32_t a){return a>=ExeImageBase&&a-ExeImageBase<ExeImageSize;};
    const auto u16=[&](std::uint32_t a)->std::uint16_t{if(!in_image(a))return 0;std::uint16_t v;std::memcpy(&v,exe_image_bytes(a,2),2);return v;};
    const auto u32=[&](std::uint32_t a)->std::uint32_t{return in_image(a)?exe_image_u32(a):0u;};
    for(std::uint32_t i=0;i<577u;++i)
        kSoundIds[i]={u16(0x7763e8u+i*20u),u16(0x7763ecu+i*20u),u32(0x773868u+i*4u),u32(0x774170u+i*4u),u16(0x7790feu+i*8u)};
    std::uint32_t ids=0;
    for(std::uint32_t b=0;b<169u;++b){
        const auto name=u32(0x6242e8u+b*12u),list=u32(0x6242e8u+b*12u+4u);
        std::uint32_t n=0;while(exe_image_bytes(name+n,1)[0])++n;
        std::uint32_t count=0;
        if(in_image(list)&&list<0x8a0000u){
            const auto c=std::int16_t(u16(list));
            if(c>0)count=std::uint32_t(c);
            if(ids+count>2213u)throw std::runtime_error("sound bank id lists exceed 2213 entries");
            for(std::uint32_t k=0;k<count;++k)kSoundBankIds[ids+k]=u16(list+2u+2u*k);
        }
        kSoundBanks[b]={reinterpret_cast<const char*>(exe_image_bytes(name,n+1)),std::uint16_t(ids),std::uint16_t(count)};
        ids+=count;
    }
    if(ids!=2213u||u32(0x74d2ecu)!=25u)throw std::runtime_error("unexpected PC sound tables");
    std::uint32_t layers=0;
    for(std::uint32_t k=0;k<25u;++k){
        const auto a=0x74d2f0u+k*20u,id=u32(a),count=u32(a+8u),list=u32(a+16u);
        if(layers+count>130u)throw std::runtime_error("ICS layers exceed 130");
        kIcsDefinitions[k]={id,std::uint16_t(layers),std::uint16_t(count)};
        for(std::uint32_t j=0;j<count;++j)std::memcpy(kIcsLayers[layers+j],exe_image_bytes(list+j*0xccu,0xccu),0xcc);
        layers+=count;
    }
    if(layers!=130u)throw std::runtime_error("unexpected ICS layer count");
}
OR2_EXE_BIND(build_sound_tables);

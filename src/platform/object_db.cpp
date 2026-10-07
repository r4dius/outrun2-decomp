#include "platform/object_db.hpp"
#include <algorithm>
#include <stdexcept>
namespace outrun::platform {
void object_db_reset_448ab0(PcObjectDb& d){d.word_7c27f8=0;d.slot_7c27f0=0;d.state_7c27f4=0;}
std::int32_t object_db_hash_448a70(const std::string& name){
    std::uint32_t h=0;
    for(const unsigned char ch:name){
        if(ch>0x7fu)throw std::runtime_error("object db: name byte outside ASCII (5822D2 toupper of a negative char)");
        const std::uint32_t up=(ch>='a'&&ch<='z')?ch-0x20u:ch;
        h=h*0x83u+up;
    }
    return std::int32_t(h);
}
void object_db_build_448b90(PcObjectDb& d,driving::Bytes p){
    for(std::uint32_t g=0;g<0xacu;g+=4){
        std::uint32_t e=p.u32(g);                                // [x+g] += x: offsets stay relative here
        for(;;e+=8u){
            const std::uint32_t handle=p.u32(e+4);
            if(handle==0xffffffffu)break;
            std::string name;
            for(std::uint32_t k=p.u32(e);;++k){const auto ch=p.u8(k);if(!ch)break;name.push_back(char(ch));}
            if(d.count_7d25e0>=PcObjectDb::Capacity)throw std::length_error("object db: table 7CC1E0 full (the PC would overwrite 7D25E0)");
            d.table_7cc1e0[d.count_7d25e0++]={object_db_hash_448a70(name),handle};
            d.sorted_7d25e4=false;
        }
    }
    d.slot_7c27f0=0;d.state_7c27f4=2;d.built=true;
}
std::uint32_t object_db_find_448b10(PcObjectDb& d,const std::string& name){
    auto* b=d.table_7cc1e0.data();auto* end=b+d.count_7d25e0;
    if(!d.sorted_7d25e4){
        // 580CB0 qsort by hash (448A50): the order of equal hashes is not
        // modelled; the lookup below refuses an ambiguous result.
        std::stable_sort(b,end,[](const PcObjectDb::Entry& x,const PcObjectDb::Entry& y){return x.hash<y.hash;});
        d.sorted_7d25e4=true;
    }
    const auto key=object_db_hash_448a70(name);
    auto it=std::lower_bound(b,end,key,[](const PcObjectDb::Entry& x,std::int32_t k){return x.hash<k;});
    // The PC compares the entry at the search end even past the count
    // (448B77: the .bss entry after the last one).
    if(it==end&&d.count_7d25e0>=PcObjectDb::Capacity)throw std::runtime_error("object db: lookup past a full table (reads 7D25E0)");
    if(it->hash!=key)return 0xffffffffu;
    if(it==end)return it->handle;
    for(auto j=it+1;j!=end&&j->hash==key;++j)
        if(j->handle!=it->handle)throw std::runtime_error("object db: hash of "+name+" shared by different handles (qsort order)");
    return it->handle;
}
}

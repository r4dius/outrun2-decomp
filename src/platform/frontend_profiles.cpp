#include "frontend_profiles.hpp"
#include "driving/pc_x87.hpp"
#include <cstdio>
#include <algorithm>
#include <cerrno>
#include <cstring>
namespace outrun::platform {
namespace {
std::uint32_t u32(const std::uint8_t* p){return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8u)|(std::uint32_t(p[2])<<16u)|(std::uint32_t(p[3])<<24u);}
void put32(std::uint8_t* p,std::uint32_t v){for(unsigned i=0;i<4;++i)p[i]=std::uint8_t(v>>(8*i));}
template<std::size_t N> int write_checked(const std::string& path,const std::array<std::uint8_t,N>& data){
    const auto temporary=path+".new",backup=path+".bak";
    // Do not clobber an unfinished save or its recovery copy.
    for(const auto& name:{temporary,backup})if(auto* old=std::fopen(name.c_str(),"rb")){
        std::fclose(old);return EEXIST;
    }
    auto* file=std::fopen(temporary.c_str(),"wb");if(!file)return errno?errno:EIO;
    std::uint8_t header[4];put32(header,N);
    bool ok=std::fwrite(header,1,4,file)==4&&std::fwrite(data.data(),1,N,file)==N;
    if(std::fflush(file)!=0)ok=false;
    if(std::fclose(file)!=0)ok=false;
    if(!ok){std::remove(temporary.c_str());return EIO;}
    bool had_original=false;
    if(std::rename(path.c_str(),backup.c_str())==0)had_original=true;
    else if(errno!=ENOENT){const int error=errno;std::remove(temporary.c_str());return error;}
    if(std::rename(temporary.c_str(),path.c_str())!=0){
        const int error=errno?errno:EIO;
        if(had_original)std::rename(backup.c_str(),path.c_str());
        return error; // Keep .new if restoring failed; never discard either copy.
    }
    if(had_original&&std::remove(backup.c_str())!=0)return errno?errno:EIO;
    return 0;
}
enum class Read {missing,valid,invalid};
template<std::size_t N> Read read(const std::string& path,std::array<std::uint8_t,N>& out){
    auto* f=std::fopen(path.c_str(),"rb");if(!f)return errno==ENOENT?Read::missing:Read::invalid;
    std::uint8_t header[4]{};std::array<std::uint8_t,N> candidate{};
    const bool valid=std::fread(header,1,4,f)==4u&&u32(header)==N&&std::fread(candidate.data(),1,N,f)==N;
    std::fclose(f);if(!valid)return Read::invalid;out=candidate;return Read::valid;
}
}
void frontend_profiles_load(FrontendProfiles& p,const std::string& dir){
    p={};frontend_profiles_reload_416380(p,dir);
}
std::uint32_t frontend_crt_random_580f40(std::uint32_t& state){
    state=state*0x343fdu+0x269ec3u;return (state>>16u)&0x7fffu;
}
void frontend_profiles_initialize_bank_4162d0(FrontendProfiles& p,
    const std::array<std::uint32_t,4>& random_values){
    for(std::size_t i=0;i<p.licenses.size();++i)
        frontend_license_reset_4471a0(p.licenses[i],random_values[i]);
}
void frontend_profiles_reload_416380(FrontendProfiles& p,const std::string& dir){
    p.queried=true;p.loaded_count=0;p.invalid_files=0;p.loaded.fill(false);
    const auto common=read(dir+"/common.dat",p.common);
    p.common_loaded=common==Read::valid;p.invalid_files+=common==Read::invalid;
    if(p.common_loaded)p.selected=u32(p.common.data());
    for(unsigned i=0;i<4u;++i){
        const auto result=read(dir+"/License"+std::to_string(i+1u)+".dat",p.licenses[i]);
        p.loaded[i]=result==Read::valid;p.loaded_count+=p.loaded[i];p.invalid_files+=result==Read::invalid;
    }
    // 448520 copies the selected 0x40C record. Reject corrupt indices safely.
    if(p.loaded_count&&p.selected<4u){
        p.active=p.licenses[p.selected];p.active_loaded=true;
        for(auto offset:{0xd0u,0xd4u})if(u32(p.active.data()+offset)>10u)
            std::fill_n(p.active.data()+offset,4u,0u);
    }
}
std::uint32_t frontend_profiles_next_key(const FrontendProfiles& p){return p.loaded_count?1u:21u;}
int frontend_profiles_free_slot(const FrontendProfiles& p){for(unsigned i=0;i<4u;++i)if((p.licenses[i][0x3f4u]&1u)==0u)return int(i);return -1;}
void frontend_license_reset_4471a0(PcLicense& p,std::uint32_t random_value){
    const auto d=[&](unsigned o,std::uint32_t v){put32(p.data()+o,v);};
    for(auto o:{0x14u,0x1cu,0x20u,0x24u,0xc4u,0xc8u,0xdcu,0xe0u,0xf0u,0xf8u,
                0x100u,0x104u,0x108u,0x10cu,0x114u,0x11cu})d(o,0);
    for(auto o:{0x18u,0xccu,0xd0u,0xd4u})d(o,10);
    d(0xc0,0x3f000000);d(0xd8,2);d(0xe4,1);d(0xf4,1);
    for(auto o:{0xe8u,0xe9u,0xeau,0xedu,0xeeu})p[o]=0;
    p[0xeb]=4;p[0xec]=1;p[0]=0;
    // 4EF260 -> 4EF150(9): table[9] - 40.909092f, then bucket 9.
    const std::uint32_t a=0x44ba5d17,b=0x4223a2e9;float x,y;
    std::memcpy(&x,&a,4);std::memcpy(&y,&b,4);x-=y;
    std::uint32_t rank;std::memcpy(&rank,&x,4);d(0x110,rank);d(0x118,9);
    std::fill_n(p.begin()+0x28,0x96,0);
    for(auto o:{0x125u,0x177u})std::fill_n(p.begin()+o,0x52,7);
    std::fill_n(p.begin()+0x1dd,0xc8,7);
    std::fill_n(p.begin()+0x2a5,0xc8,0);
    std::fill_n(p.begin()+0x1c9,20,0);
    std::fill_n(p.begin()+0x36e,0x84,0x77);
    p[0x3f4]=(p[0x3f4]&0xfcu)|2u;
    std::memcpy(p.data()+0x3fc,"051013153129",12);d(0x408,random_value);
}
void frontend_license_unlock_447360(PcLicense& p){
    std::fill_n(p.begin()+0x28,0x96,0xff);
    for(auto o:{0x125u,0x177u})std::fill_n(p.begin()+o,0x52,6);
    std::fill_n(p.begin()+0x1dd,0x190,6);
    std::fill_n(p.begin()+0x1c9,20,0);
    std::fill_n(p.begin()+0x36e,0x84,0x66);
}
void frontend_license_name_4dd590(PcLicense& p,const std::array<std::uint8_t,16>& name){
    p[0x3f4]|=2;
    std::copy(name.begin(),name.end(),p.begin());
}
bool frontend_profiles_select_448520(FrontendProfiles& p,std::uint32_t slot){
    if(slot!=~0u&&slot>=4)return false;
    p.selected=slot;put32(p.common.data(),slot);
    if(slot==~0u)return true; // PC does not clear the active object here.
    p.active=p.licenses[slot];p.active_loaded=true;
    for(auto o:{0xd0u,0xd4u})if(u32(p.active.data()+o)>10u)put32(p.active.data()+o,0);
    return true;
}
void frontend_profiles_sync_active(FrontendProfiles& p){
    if(p.selected<4&&p.active_loaded)p.licenses[p.selected]=p.active;
}
FrontendProfileList frontend_profiles_list_4e1c00(FrontendProfiles& p){
    frontend_profiles_sync_active(p);FrontendProfileList list;
    for(unsigned i=0;i<4;++i)if(p.licenses[i][0x3f4]&1u)list.slots[list.occupied++]=i;
    const int free=frontend_profiles_free_slot(p);
    if(free>=0){list.new_index=int(list.occupied);list.slots[list.occupied]=unsigned(free);}
    return list;
}
bool frontend_category_grade_4e81d0(const PcLicense& p,const LicenseProgressTables& tables,unsigned category,int& grade){
        if(!tables.categories_ready||category>=40)return false;
        const unsigned count=tables.category_counts[category];
        if(count>5)return false;
        if(!count){grade=7;return true;}
        unsigned total=0;
        for(unsigned i=0;i<count;++i){const unsigned g=p[0x1dd+category*5+i];
            if(g==7){grade=7;return true;}if(g>6)return false;total+=g;}
        grade=int(total/count);return true;
}
bool frontend_group_grade_4e82a0(const PcLicense& p,const LicenseProgressTables& tables,unsigned group,int& grade){
    if(group>=7)return false;
    constexpr unsigned boundaries[]={0,5,14,25,40};
        grade=7;
        if(group<4){int sum=0;
            for(unsigned i=boundaries[group];i<boundaries[group+1];++i){int g;
                if(!frontend_category_grade_4e81d0(p,tables,i,g))return false;
                if(g!=7)sum+=g+1;}
            grade=sum/int(boundaries[group+1]-boundaries[group])-1;if(grade<0)grade=7;
        }else if(tables.championship_present[group-4]){
            constexpr unsigned count[]={1,1,1,2,2,2,3,3,3,3,4,4,4,4,4};
            int sum=0;
            for(unsigned race=0;race<15;++race){const auto type=tables.championship_types[group-4][race];
                if(type>=66)return false;
                const unsigned packed=p[0x36e + type*2]|unsigned(p[0x36f+type*2])<<8;
                for(unsigned i=0;i<count[race];++i){const unsigned g=(packed>>(4*i))&15;
                    if(g>7)return false;
                    if(g!=7)sum+=int(g)+1;}
            }
            grade=sum/41-1;if(grade<0)grade=7;
        }
        if(grade<0||grade>7)return false;
    return true;
}
bool frontend_category_unlocked_4e8410(const PcLicense& p,const LicenseProgressTables& tables,unsigned group,bool& unlocked){
    unlocked=false;if(group>=7)return true;
    if(group==0||group==4){unlocked=true;return true;}
    int grade{};if(!frontend_group_grade_4e82a0(p,tables,group-1,grade))return false;
    unlocked=grade!=7&&grade>=4;return true;
}
std::int32_t frontend_unlock_notice_499730(PcLicense& p,const std::array<int,7>& grades){
    int sum=0;for(int grade:grades)if(grade!=7)sum+=grade+1;
    std::int32_t notice=-1;
    if(sum/7==7)notice=8;
    else for(int group=6;group>=0;--group)
        if(grades[unsigned(group)]!=7&&grades[unsigned(group)]>=4){notice=group;break;}
    // PC 49985C masks the shift count to five bits even for EBX == -1.
    const std::uint32_t bit=1u<<(std::uint32_t(notice)&31u),seen=u32(p.data()+0x11c);
    if(seen&bit)return -1;
    put32(p.data()+0x11c,seen|bit);p[0x3f4]|=2u;return notice;
}
bool frontend_category_picture_4e8b20(const PcLicense& p,const LicenseProgressTables& tables,unsigned selected,unsigned& result){
    if(selected>=7)return false;
    result=0;for(int group=2;group>=0;--group){int grade{};
        if(!frontend_group_grade_4e82a0(p,tables,unsigned(group),grade))return false;
        if(grade!=7&&grade>=4){result=unsigned(group+1)*4;break;}}
    if(selected<=3)result+=selected;return true;
}
bool frontend_license_completion_447400(const PcLicense& p,const LicenseProgressTables& tables,double& result){
    result=0;if(tables.mode!=32)return true; // original non-SUMO_FE branch
    if(!tables.categories_ready)return false;
    constexpr float weights[]={10,20,30,40,60,80,100,0};
    float total=0;
    for(unsigned group=0;group<7;++group){int grade{};
        if(!frontend_group_grade_4e82a0(p,tables,group,grade))return false;
        total+=weights[grade];
    }
    // 447400: fld sum; fmul 1/7 (rounds to the x87 precision control); the
    // ST0 result is returned and the caller stores it as a double.
    const std::uint32_t bits=0x3e124925;float seventh;std::memcpy(&seventh,&bits,4);
    result=driving::x87_double(driving::X87(total)*driving::X87(seventh));return true;
}
FrontendProfileSaveResult frontend_profiles_save_common_4164d0(FrontendProfiles& p,const std::string& dir){
    put32(p.common.data(),p.selected);FrontendProfileSaveResult result;
    result.error=result.common_error=write_checked(dir+"/common.dat",p.common);
    result.common_written=result.error==0;return result;
}
FrontendProfileSaveResult frontend_profiles_save_416420(FrontendProfiles& p,const std::string& dir,bool enabled){
    if(!enabled)return {};
    auto result=frontend_profiles_save_common_4164d0(p,dir);
    if(p.selected==~0u)return result;
    if(p.selected>=4||!p.active_loaded){result.license_error=EINVAL;result.error=EINVAL;result.pc_result=2;return result;}
    frontend_profiles_sync_active(p);
    // Original does not branch on the common write's return code.
    result.license_error=write_checked(dir+"/License"+std::to_string(p.selected+1)+".dat",p.active);
    if(!result.error)result.error=result.license_error;
    result.pc_result=1u+(result.license_error!=0);
    result.license_written=result.license_error==0;
    if(result.license_written){if(!p.loaded[p.selected])++p.loaded_count;p.loaded[p.selected]=true;}
    return result;
}
bool frontend_profiles_delete_416500(FrontendProfiles& p,std::uint32_t slot,
                                    const std::string& dir,std::uint32_t slot_random,
                                    std::uint32_t active_random,int& error){
    error=0;if(slot>=4)return true; // PC accepts out-of-range indices as no-op.
    frontend_license_reset_4471a0(p.licenses[slot],slot_random);
    if(slot==p.selected)frontend_license_reset_4471a0(p.active,active_random);
    const auto path=dir+"/License"+std::to_string(slot+1)+".dat";
    if(std::remove(path.c_str())!=0&&errno!=ENOENT){error=errno?errno:EIO;return false;}
    if(p.loaded[slot]&&p.loaded_count)--p.loaded_count;
    p.loaded[slot]=false;return true;
}
}

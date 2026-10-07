#include "platform/frontend_profiles.hpp"
#include <filesystem>
#include <fstream>
#include <chrono>
#include <stdexcept>
#include <cstdio>
using namespace outrun::platform;
namespace fs=std::filesystem;
namespace {
unsigned checks{};void req(bool v,const char*m){++checks;if(!v)throw std::runtime_error(m);}
template<class T> void save(const fs::path& path,const T& payload,unsigned declared=0){
    std::ofstream f(path,std::ios::binary);const unsigned n=declared?declared:unsigned(payload.size());
    for(unsigned i=0;i<4u;++i)f.put(char(n>>(i*8u)));
    f.write(reinterpret_cast<const char*>(payload.data()),payload.size());req(bool(f),"fixture write");
}
struct Fixture {fs::path root=fs::temp_directory_path()/("or2-profiles-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Fixture(){fs::create_directory(root);}~Fixture(){fs::remove_all(root);}};
}
int main(){try{
    {
        Fixture cold;FrontendProfiles boot;std::uint32_t rng=1;
        std::array<std::uint32_t,4> random{};for(auto& value:random)value=frontend_crt_random_580f40(rng);
        req(random==std::array<std::uint32_t,4>{{41,18467,6334,26500}},"PC CRT stream and four ordered constructor calls");
        frontend_profiles_initialize_bank_4162d0(boot,random);const auto initialized=boot.licenses;
        frontend_profiles_reload_416380(boot,cold.root.string());
        req(boot.licenses==initialized&&boot.selected==~0u&&frontend_profiles_next_key(boot)==21,
            "cold read preserves four original constructors");
        req(boot.licenses[0][0x18]==10&&boot.licenses[0][0x1dd]==7&&boot.licenses[0][0x36e]==0x77,
            "new editor receives original choice and locked progress, not zero profile");
        auto existing=initialized[2];existing[0]='X';existing[0x3f4]|=1;
        save(cold.root/"License3.dat",existing);
        std::array<std::uint8_t,PcCommonSaveBytes> common{};common[0]=1;
        save(cold.root/"common.dat",common);
        frontend_profiles_reload_416380(boot,cold.root.string());
        req(boot.loaded_count==1&&boot.licenses[2]==existing&&boot.licenses[1]==initialized[1],
            "sparse reads overlay only existing slots");
        req(boot.active_loaded&&boot.selected==1&&boot.active==initialized[1],
            "original common selection can refer to constructed slot without a file");
        save(cold.root/"License2.dat",existing,123);
        frontend_profiles_reload_416380(boot,cold.root.string());
        req(boot.invalid_files==1&&boot.licenses[1]==initialized[1],"corrupt overlay preserves initialized slot");
        req(frontend_profiles_select_448520(boot,0),"select actual cold slot");
        boot.active[0x3f4]|=1;boot.active[0]='N';
        const auto saved=frontend_profiles_save_416420(boot,cold.root.string(),true);
        req(bool(saved)&&saved.license_written,"new initialized profile saved to private fixture");
        FrontendProfiles restarted;frontend_profiles_initialize_bank_4162d0(restarted,random);
        frontend_profiles_reload_416380(restarted,cold.root.string());
        req(restarted.active==boot.active&&restarted.active[0x36e]==0x77,"cold create/save/restart preserves locked progression");
    }
    Fixture fixture;FrontendProfiles p;
    frontend_profiles_load(p,fixture.root.string());
    req(p.queried&&p.loaded_count==0&&p.invalid_files==0,"new installation");
    req(frontend_profiles_next_key(p)==21u&&frontend_profiles_free_slot(p)==0,"original new-license route");
    std::array<std::uint8_t,PcLicenseBytes> license{};license[0x3f4]=1;license[0x10]=0xab;
    license[0xd0]=11;license[0xd4]=7;
    save(fixture.root/"License1.dat",license);license[0x10]=0xcd;save(fixture.root/"License4.dat",license);
    std::array<std::uint8_t,PcCommonSaveBytes> common{};common[0]=3;save(fixture.root/"common.dat",common);
    frontend_profiles_load(p,fixture.root.string());
    req(p.loaded_count==2&&p.loaded[0]&&p.loaded[3]&&!p.loaded[1],"sparse original slots");
    req(p.selected==3&&p.active_loaded&&p.active[0x10]==0xcd,"common selected record copied");
    req(p.active[0xd0]==0&&p.active[0xd4]==7&&p.licenses[3][0xd0]==11,"448520 volume clamp on active copy only");
    req(frontend_profiles_next_key(p)==1&&frontend_profiles_free_slot(p)==1,"existing license route and first unused flag");
    save(fixture.root/"License2.dat",license,0x123); // Wrong original envelope length.
    {std::ofstream f(fixture.root/"License3.dat",std::ios::binary);f.put(12);}
    frontend_profiles_load(p,fixture.root.string());
    req(p.invalid_files==2&&p.loaded_count==2,"corrupt records not published");
    req(fs::file_size(fixture.root/"License3.dat")==1,"loading did not repair/overwrite save");
    common[0]=255;save(fixture.root/"common.dat",common);frontend_profiles_load(p,fixture.root.string());
    req(!p.active_loaded&&p.selected==255,"out-of-bounds common index safely rejected");
    for(auto& record:p.licenses)record[0x3f4]=1;
    req(frontend_profiles_free_slot(p)==-1,"four occupied records");
    PcLicense blank;blank.fill(0xaa);frontend_license_reset_4471a0(blank,12345);
    req(blank[0x18]==10&&blank[0xd0]==10&&blank[0xeb]==4&&blank[0x3f4]==0xaa,"PC new license defaults and preserved high flag bits");
    req(blank[1]==0xaa&&blank[0x120]==0xaa&&blank[0x36d]==0xaa,"constructor preserves untouched bytes");
    req(blank[0x125]==7&&blank[0x36e]==0x77&&blank[0x2a5]==0,"locked progression defaults");
    req(std::string(reinterpret_cast<char*>(blank.data()+0x3fc),12)=="051013153129","original license signature");
    frontend_license_unlock_447360(blank);req(blank[0x28]==255&&blank[0x125]==6&&blank[0x2a5]==6&&blank[0x36e]==0x66,"original unlock branch");
    Fixture writable;p={};
    for(auto& r:p.licenses)frontend_license_reset_4471a0(r,42);
    auto list=frontend_profiles_list_4e1c00(p);req(list.occupied==0&&list.new_index==0&&list.slots[0]==0,"new slot follows occupied entries");
    req(frontend_profiles_select_448520(p,2),"select unused slot for editor");p.active[0x3f4]|=1;p.active[0]='A';
    list=frontend_profiles_list_4e1c00(p);req(list.occupied==1&&list.slots[0]==2&&list.slots[1]==0,"active sync and sparse menu mapping");
    auto result=frontend_profiles_save_416420(p,writable.root.string(),false);
    req(bool(result)&&!result.common_written&&!fs::exists(writable.root/"common.dat"),"disabled saves do not write");
    result=frontend_profiles_save_416420(p,writable.root.string(),true);
    req(bool(result)&&result.common_written&&result.license_written,"common and active save");
    req(fs::file_size(writable.root/"License3.dat")==PcLicenseBytes+4,"original save envelope size");
    FrontendProfiles restored;frontend_profiles_load(restored,writable.root.string());
    req(restored.selected==2&&restored.active==p.active,"save/reload active exact bytes");
    req(result.pc_result==1,"original successful save result");
    {std::ofstream f(writable.root/"common.dat.bak");f<<"recovery";}
    p.active[0]='D';result=frontend_profiles_save_416420(p,writable.root.string(),true);
    req(!result&&result.common_error&&result.license_written&&result.pc_result==1,"PC continues license write after failed common file");
    frontend_profiles_load(restored,writable.root.string());req(restored.active[0]=='D',"license payload published despite common error");
    fs::remove(writable.root/"common.dat.bak");
    p.active[0]='B';result=frontend_profiles_save_416420(p,writable.root.string(),true);
    req(bool(result)&&!fs::exists(writable.root/"License3.dat.bak"),"checked save replacement");
    {std::ofstream f(writable.root/"License3.dat.bak");f<<"recovery";}
    p.active[0]='C';result=frontend_profiles_save_416420(p,writable.root.string(),true);
    req(!result&&!result.license_written&&result.pc_result==2&&result.license_error,"do not clobber recovery copy; PC failure result");
    frontend_profiles_load(restored,writable.root.string());req(restored.active[0]=='B',"failed save preserves previous record");
    req(frontend_profiles_select_448520(p,~0u)&&p.active[0]=='C',"PC deselect does not erase active record");
    req(!frontend_profiles_select_448520(p,4)&&p.selected==~0u,"invalid slot cannot index bank");
    int delete_error=999;
    frontend_profiles_select_448520(restored,2);
    req(frontend_profiles_delete_416500(restored,2,writable.root.string(),77,88,delete_error)&&delete_error==0,"explicit original delete operation");
    req(!fs::exists(writable.root/"License3.dat")&&fs::exists(writable.root/"common.dat"),"delete targets only selected license file");
    req(!(restored.active[0x3f4]&1)&&restored.selected==2&&restored.active[0x408]==88&&restored.licenses[2][0x408]==77,"delete resets both records but does not change selected index");
    req(frontend_profiles_delete_416500(restored,~0u,writable.root.string(),1,2,delete_error),"invalid delete is PC no-op");
    std::printf("frontend_profiles: %u checks; PC envelopes, selection, flags, corruption and read-only behavior\n",checks);return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}

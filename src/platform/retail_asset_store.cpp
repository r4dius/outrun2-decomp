#include "platform/retail_asset_store.hpp"
#include "platform/frontend_vehicle_data.hpp"
#include <array>
#include <cstdio>
#include <cstring>
#include <limits>
#include <utility>
#include <zlib.h>
#include <map>
#ifndef _WIN32
#include <strings.h>
#endif
#include "system/files.hpp"

namespace outrun::platform {
namespace {
struct ResourcePath { std::uint32_t id,mode; const char* path; bool inflate_for_owner; };

// Mirrors the original resource table entries exercised by the current native
// frontend/START/GAME route.  Paths are retail paths, not generated pack names.
constexpr ResourcePath Fixed[] = {
 {0xBA,2,"Common/obj_vshadow_pmt.sz",false},{0xBB,2,"Common/obj_pc_color_pmt.sz",false},
 {0x2C,8,"Sprite/spr_SPRANI_ETC_CVT_Exst.sz",false},{0x33,8,"Sprite/spr_sprani_common_cvt_xst.sz",false},
 {0x48,8,"Sprite/spr_SPRANI_SUMO_LOADING_Exst.sz",false},{0x44,9,"Sprite/spr_SPRANI_SUMO_FE_CVT_Exst.sz",false},
 {LoaderAssetSelectTableId,0,"Common/sel_dl_edit0.tgt",false},
 {0x2F,2,"Sprite/spr_SPRANI_LOADING_CVT_Exst.sz",false},{0x49,2,"Sprite/spr_SPRANI_SUMO_VSLOAD_Exst.sz",false},
 {0x2B,8,"Sprite/spr_SPRANI_GAME_CVT_Exst.sz",false},{0x2D,8,"Sprite/spr_sprani_fight_Exst.sz",false},
 {0x3E,8,"Sprite/spr_sprani_FLAG_RANK_Exst.sz",false},{0x3F,8,"Sprite/spr_sprani_CLAR_RANK_Exst.sz",false},
 {0x40,8,"Sprite/spr_sprani_JENN_RANK_Exst.sz",false},{0x41,8,"Sprite/spr_sprani_HOLL_RANK_Exst.sz",false},
 {0x46,8,"Sprite/spr_sprani_fruity_cvt_Exst.sz",false},{0x4A,8,"Sprite/spr_SPRANI_TAEXTRA_CVT_xst.sz",false},
 {0x23,8,"Sprite/spr_sprani_meter_246gts_xst.sz",false},{0x24,8,"Sprite/spr_sprani_meter_288gts_xst.sz",false},
 {0x25,8,"Sprite/spr_sprani_meter_360_xst.sz",false},{0x26,8,"Sprite/spr_sprani_meter_365gts_xst.sz",false},
 {0x27,8,"Sprite/spr_sprani_meter_f40_xst.sz",false},{0x28,8,"Sprite/spr_sprani_meter_f50_xst.sz",false},
 {0x29,8,"Sprite/spr_sprani_meter_fx_xst.sz",false},{0x2A,8,"Sprite/spr_sprani_meter_testa_xst.sz",false},
 {0x30,8,"Sprite/spr_sprani_meter_512bb_xst.sz",false},{0x31,8,"Sprite/spr_sprani_meter_250gto_xst.sz",false},
 {0x3C,9,"Sprite/spr_sprani_efct_flor_xst.sz",false},
 {0xBA,8,"Common/obj_vshadow_pmt.sz",false},{0x57,8,"Common/obj_course_obj_common_pmt.sz",false},
 {0x1EF,8,"Common/obj_course_obj_gates_pmt.sz",false},{0xBB,8,"Common/obj_pc_color_pmt.sz",false},
 {0xBE,8,"CHR/obj_chr_dr_m00_pmt.sz",false},{LoaderAssetMotionTableId,0,"Common/motdata_table.bin",false},
 {LoaderAssetBoneTableId,0,"Common/bone.bin",false},{LoaderAssetMotionGroup6Id,0,"Anims/mot_driver_bin.sz",true},
 {LoaderAssetMotionGroup32Id,0,"Anims/mot_OR2SP_DRIVER_bin.sz",true},
 {0xBC,8,"DRIVER/obj_driver_rival_pmt.sz",false},{0xC8,8,"DRIVER/obj_driver_special_pmt.sz",false},
 {0x30022,0,"Anims/mot_OR2SP_FAL_bin.sz",true},{0x30023,0,"Anims/mot_P51_AA_bin.sz",true},
 {0x30024,0,"Anims/mot_P51_AB_bin.sz",true},{0x30025,0,"Anims/mot_P51_FA_bin.sz",true},
 {0x30026,0,"Anims/mot_P51_FB_bin.sz",true},{0x30027,0,"Anims/mot_P51_HT_bin.sz",true},
 {0x30017,0,"Anims/mot_ETC_bin.sz",true},{0x30021,0,"Anims/mot_OR2SP_ETC_bin.sz",true},
 {0xC3,9,"CHR/CHR_AUT04_CVT.bin",false},
 {0x162,9,"Stage/CLOU_R/cs_CS_CLOU_R_pmt.sz",false},{0x19E,9,"Stage/CLOU_R/obj_COURSE_OBJ_CS_CLOU_R_pmt.sz",false},
 {0x180,9,"Stage/CLOU_R/cs_ENV_CLOU_R_pmt.sz",false},
 {0xDB,9,"Stage/BEAC/cs_CS_BEAC_pmt.sz",false},{0xFE,9,"Stage/BEAC/obj_course_obj_cs_beac_pmt.sz",false},
 {0x126,9,"Stage/BEAC/cs_ENV_BEAC_pmt.sz",false},
 // 0x44DBB0(10), primary +0x44 for BEAC.
 {0x117,10,"Stage/BEAC/obj_course_obj_sky_beac_pmt.sz",false},
};

constexpr const char* FrontendScripts[LoaderAssetFrontendScriptCount] = {
 "Request_Course_SP","Request_Course_OR2","Req_Course_MIX_PS2","Req_Course_MIX_PSP",
 "Req_Palm_Beach","Req_Deep_Lake","Req_Industrial_Complex","Req_Alpine","Req_Snow_Mountain",
 "Req_Cloudy_Highland","Req_Castle_Wall","Req_Ghost_Forest","Req_Coniferous_Forest","Req_Desert",
 "Req_Tulip_Garden","Req_Metropolis","Req_Ancient_Ruins","Req_Cape_Way","Req_Imperial_Avenue",
 "Req_Beach","Req_Sequoia","Req_Niagara","Req_Las_Vegas","Req_Alaska","Req_Grand_Canyon",
 "Req_San_Francisco","Req_Amazon","Req_MachuPicchu","Req_Yose","Req_Maya","Req_New_York",
 "Req_Prince_Edward","Req_Florida","Req_Easter_Island","Req_Palm_Beach_r","Req_Deep_Lake_r",
 "Req_Industrial_Complex_r","Req_Alpine_r","Req_Snow_Mountain_r","Req_Cloudy_Highland_r",
 "Req_Castle_Wall_r","Req_Ghost_Forest_r","Req_Coniferous_Forest_r","Req_Desert_r",
 "Req_Tulip_Garden_r","Req_Metropolis_r","Req_Ancient_Ruins_r","Req_Cape_Way_r",
 "Req_Imperial_Avenue_r","Req_Beach_r","Req_Sequoia_r","Req_Niagara_r","Req_Las_Vegas_r",
 "Req_Alaska_r","Req_Grand_Canyon_r","Req_San_Francisco_r","Req_Amazon_r","Req_MachuPicchu_r",
 "Req_Yose_r","Req_Maya_r","Req_New_York_r","Req_Prince_Edward_r","Req_Florida_r","Req_Easter_Island_r"
};

void fail(std::string* error,const char* message){ if(error)*error=message; }
std::uint64_t key(std::uint32_t id,std::uint32_t mode){return (std::uint64_t(mode)<<32u)|id;}

bool file_exists(const std::string& path){
    std::FILE* f=std::fopen(path.c_str(),"rb");if(!f)return false;std::fclose(f);return true;
}

std::string normalize_relative(std::string path){
    while(!path.empty()&&(path.front()=='\\'||path.front()=='/'))path.erase(path.begin());
    for(char& c:path)if(c=='\\')c='/';
    if(path.find("..")!=std::string::npos)return {};
    return path;
}

bool inflate_exact(const std::vector<std::uint8_t>& input,std::vector<std::uint8_t>& output,
                   std::size_t max_bytes,std::string* error){
    output.clear();
    if(input.empty()){fail(error,"empty zlib stream");return false;}
    z_stream stream{};
    stream.next_in=const_cast<Bytef*>(reinterpret_cast<const Bytef*>(input.data()));
    if(input.size()>std::numeric_limits<uInt>::max()){fail(error,"compressed retail asset too large");return false;}
    stream.avail_in=static_cast<uInt>(input.size());
    if(inflateInit(&stream)!=Z_OK){fail(error,"zlib inflateInit failed");return false;}
    std::array<std::uint8_t,64u*1024u> chunk{};
    int rc=Z_OK;
    while(rc==Z_OK){
        stream.next_out=reinterpret_cast<Bytef*>(chunk.data());
        stream.avail_out=static_cast<uInt>(chunk.size());
        rc=inflate(&stream,Z_NO_FLUSH);
        const auto produced=chunk.size()-stream.avail_out;
        if(produced>max_bytes-output.size()){
            inflateEnd(&stream);output.clear();fail(error,"inflated retail asset exceeds limit");return false;
        }
        output.insert(output.end(),chunk.data(),chunk.data()+produced);
    }
    const bool ok=rc==Z_STREAM_END&&stream.avail_in==0u;
    inflateEnd(&stream);
    if(!ok){output.clear();fail(error,"retail .sz is not one complete zlib stream");return false;}
    return true;
}

const ResourcePath* fixed_path(std::uint32_t id,std::uint32_t mode){
    for(const auto& item:Fixed)if(item.id==id&&item.mode==mode)return &item;
    return nullptr;
}
}

bool retail_asset_inflate_sz(const std::vector<std::uint8_t>& input,std::vector<std::uint8_t>& output,std::string* error){
    return inflate_exact(input,output,128u*1024u*1024u,error);
}
namespace {
#ifndef _WIN32
// The game names its files in several cases ("CARS/" for the Cars folder...).
// FAT/exFAT/NTFS ignore case; a case-sensitive file system (a PS5 package,
// Linux) needs each path component matched against the real directory names.
std::string case_resolved(const std::string& root,const std::string& rel){
    static std::map<std::string,std::string> resolved;
    if(const auto it=resolved.find(root+"|"+rel);it!=resolved.end())return it->second;
    std::string dir=root.empty()?".":root,out;
    for(std::size_t start=0;start<=rel.size();){
        const auto slash=rel.find('/',start);
        const auto part=rel.substr(start,slash==std::string::npos?std::string::npos:slash-start);
        std::string match=part;
        for(const auto& name:directory_names(dir))if(strcasecmp(name.c_str(),part.c_str())==0){match=name;break;}
        out+=(out.empty()?"":"/")+match;dir+="/"+match;
        if(slash==std::string::npos)break;
        start=slash+1;
    }
    resolved.emplace(root+"|"+rel,out);
    return out;
}
#endif
}
std::string retail_asset_path(const RetailAssetStore& store,const std::string& relative){
    auto rel=normalize_relative(relative);if(rel.empty())return {};
    if(store.root.empty())return rel;
    const std::string root=(store.root.back()=='/'||store.root.back()=='\\')?store.root.substr(0,store.root.size()-1):store.root;
#ifndef _WIN32
    if(!file_exists(root+"/"+rel))rel=case_resolved(root,rel);
#endif
    return root+"/"+rel;
}

bool retail_asset_store_is_game_root(const std::string& root){
    RetailAssetStore s{};s.root=root;
    return file_exists(retail_asset_path(s,"Scripts/bin/Races.bin"))&&
           file_exists(retail_asset_path(s,"Scripts/bin/csc_data_cvt.bin"))&&
           file_exists(retail_asset_path(s,"Stage/BEAC/coli_CS_BEAC_bin.sz"))&&
           file_exists(retail_asset_path(s,"CARS/obj_plcar_250gto_pmt.sz"))&&
           file_exists(retail_asset_path(s,"Sprite/spr_SPRANI_SUMO_FE_CVT_Exst.sz"));
}

bool retail_asset_store_open(RetailAssetStore& store,const std::string& root,std::string* error){
    if(error)error->clear();
    if(root.empty()||!retail_asset_store_is_game_root(root)){
        fail(error,"path is not an OutRun 2006 retail data root");return false;
    }
    store={};store.root=root;return true;
}

bool retail_asset_read_relative(RetailAssetStore& store,const std::string& relative,
                                std::vector<std::uint8_t>& bytes,std::size_t max_bytes,
                                std::string* error){
    if(error)error->clear();
    const auto path=retail_asset_path(store,relative);
    if(path.empty()){fail(error,"invalid retail relative path");return false;}
    std::FILE* file=std::fopen(path.c_str(),"rb");if(!file){fail(error,"retail file missing");return false;}
    if(std::fseek(file,0,SEEK_END)!=0){std::fclose(file);fail(error,"cannot size retail file");return false;}
    const auto length=std::ftell(file);if(length<0||static_cast<std::uint64_t>(length)>max_bytes){std::fclose(file);fail(error,"retail file exceeds size limit");return false;}
    std::rewind(file);bytes.resize(static_cast<std::size_t>(length));
    const auto got=bytes.empty()?0u:std::fread(bytes.data(),1,bytes.size(),file);const int extra=std::fgetc(file);std::fclose(file);
    if(got!=bytes.size()||extra!=EOF){bytes.clear();fail(error,"retail file read mismatch");return false;}
    ++store.file_reads;store.bytes_read+=bytes.size();return true;
}

bool retail_asset_read_relative_inflated(RetailAssetStore& store,const std::string& relative,
                                         std::vector<std::uint8_t>& bytes,std::size_t max_bytes,
                                         std::string* error){
    std::vector<std::uint8_t> compressed;
    if(!retail_asset_read_relative(store,relative,compressed,max_bytes,error))return false;
    return inflate_exact(compressed,bytes,max_bytes,error);
}

const LoaderAssetRecord* retail_asset_lookup(RetailAssetStore& store,
                                              std::uint32_t resource_id,
                                              std::uint32_t request_mode,
                                              std::string* error){
    if(error)error->clear();
    const auto k=key(resource_id,request_mode);
    if(auto it=store.loader_cache.find(k);it!=store.loader_cache.end()){
        ++store.cache_hits;return &it->second;
    }
    std::string relative;bool inflate_owner=false;
    if(resource_id>=LoaderAssetFrontendScriptBaseId&&
       resource_id<LoaderAssetFrontendScriptBaseId+LoaderAssetFrontendScriptCount&&request_mode==0u){
        const auto lane=resource_id-LoaderAssetFrontendScriptBaseId;
        relative="Scripts/bin/"+std::string(FrontendScripts[lane])+".bin";
    }else{
        // 48BFE0 preloads models in mode 11; 49BA80 uses mode 8.
        // Both resolve the same PC PMT source, with distinct ownership keys.
        if(request_mode==8u||request_mode==11u){
            for(std::size_t i=0;i<VehicleResourceIds.size();++i)
                if(VehicleResourceIds[i]==resource_id){relative=VehicleResourcePaths[i];break;}
        }
        if(relative.empty()){
            const auto* item=fixed_path(resource_id,request_mode);
            if(item){relative=item->path;inflate_owner=item->inflate_for_owner;}
            else if(request_mode==0u){
                // 4BF2E0 (the OUTRUN2SP course screen) requests the model in mode 0: the same
                // 633558 PMT path, its own ownership key.
                for(std::size_t i=0;i<VehicleResourceIds.size();++i)
                    if(VehicleResourceIds[i]==resource_id){relative=VehicleResourcePaths[i];break;}
            }
            if(relative.empty())return nullptr;
        }
    }
    LoaderAssetRecord record{};record.resource_id=resource_id;record.request_mode=request_mode;
    const bool ok=inflate_owner?
        retail_asset_read_relative_inflated(store,relative,record.bytes,128u*1024u*1024u,error):
        retail_asset_read_relative(store,relative,record.bytes,128u*1024u*1024u,error);
    if(!ok)return nullptr;
    return &store.loader_cache.emplace(k,std::move(record)).first->second;
}

const std::vector<std::uint8_t>* retail_asset_guest_path(
    RetailAssetStore& store,const std::string& guest_path,bool inflate_sz,
    std::string* error){
    if(error)error->clear();
    const auto relative=normalize_relative(guest_path);
    if(relative.empty()){fail(error,"invalid guest asset path");return nullptr;}
    const std::string cache_key=(inflate_sz?"I:":"R:")+relative;
    if(auto it=store.path_cache.find(cache_key);it!=store.path_cache.end()){
        ++store.cache_hits;return &it->second;
    }
    std::vector<std::uint8_t> bytes;
    const bool ok=inflate_sz?
        retail_asset_read_relative_inflated(store,relative,bytes,128u*1024u*1024u,error):
        retail_asset_read_relative(store,relative,bytes,128u*1024u*1024u,error);
    if(!ok)return nullptr;
    return &store.path_cache.emplace(cache_key,std::move(bytes)).first->second;
}

} // namespace outrun::platform

#include "platform/retail_gpu_cache.hpp"
#include "platform/loader_asset_pack.hpp"
#include "platform/frontend_preview_pack.hpp"
#include "platform/game_ui_pack.hpp"
#include "switch_renderer.hpp"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>
#include <zlib.h>
using namespace outrun::platform;
namespace fs=std::filesystem;
namespace {
unsigned checks{};void req(bool v,const char*m){++checks;if(!v)throw std::runtime_error(m);}
void put(const fs::path&p,const std::vector<std::uint8_t>&b){fs::create_directories(p.parent_path());std::ofstream f(p,std::ios::binary);f.write(reinterpret_cast<const char*>(b.data()),std::streamsize(b.size()));if(!f)throw std::runtime_error("fixture write");}
void copy_file_fixture(const fs::path&a,const fs::path&b){fs::create_directories(b.parent_path());fs::copy_file(a,b,fs::copy_options::overwrite_existing);}
const LoaderAssetRecord& rec(const LoaderAssetPack&p,std::uint32_t id,std::uint32_t mode){const auto*r=find_loader_asset(p,id,mode);if(!r)throw std::runtime_error("loader record missing");return *r;}
std::vector<std::uint8_t> zip(const std::vector<std::uint8_t>&raw){uLongf n=compressBound(raw.size());std::vector<std::uint8_t> out(n);if(compress2(out.data(),&n,raw.data(),raw.size(),9)!=Z_OK)throw std::runtime_error("zlib fixture");out.resize(n);return out;}
std::vector<std::uint8_t> wrap(const std::vector<std::uint8_t>&ani){std::vector<std::uint8_t> raw(4+ani.size());const auto n=std::uint32_t(ani.size());for(unsigned i=0;i<4;++i)raw[i]=std::uint8_t(n>>(i*8));std::copy(ani.begin(),ani.end(),raw.begin()+4);return zip(raw);}
}
int main(int argc,char**argv){try{
 req(argc==4,"usage LOADER FRONTEND LOADING");std::string e;LoaderAssetPack loader;FrontendPreviewPack fe,loading;
 req(load_loader_asset_pack_file(argv[1],loader,&e),"loader fixture");req(load_frontend_preview_pack_file(argv[2],fe,&e),"frontend fixture");req(load_frontend_preview_pack_file(argv[3],loading,&e),"loading fixture");
 const auto root=fs::temp_directory_path()/"outrun-r152-auto-cache";fs::remove_all(root);
 // A game folder holds the 250 GTO model (retail_asset_store_is_game_root).
 put(root/"CARS/obj_plcar_250gto_pmt.sz",rec(loader,0x0b,8).bytes);
 put(root/"Sprite/spr_SPRANI_SUMO_FE_CVT_Exst.sz",rec(loader,0x44,9).bytes);
 put(root/"Sprite/spr_SPRANI_LOADING_CVT_Exst.sz",rec(loader,0x2f,2).bytes);
 put(root/"Sprani/ani_SPRANI_SUMO_FE_CVT.sz",wrap(fe.animation));
 put(root/"Sprani/ani_SPRANI_LOADING_CVT.sz",wrap(loading.animation));
 put(root/"Scripts/bin/Races.bin",{1});put(root/"Scripts/bin/csc_data_cvt.bin",{1});put(root/"Stage/BEAC/coli_CS_BEAC_bin.sz",{1});
 RetailAssetStore store{};req(retail_asset_store_open(store,root.string(),&e),"retail root");std::vector<std::uint8_t> pack;
 {const bool ok=build_retail_start_loading(store,pack,&e);if(!ok)throw std::runtime_error(e);++checks;}
 req(!fs::exists(root/"Cache")&&!fs::exists(root/"start_loading_en.ldp2"),"nothing written next to the game files");
 FrontendPreviewPack load2;{const bool parsed=parse_frontend_preview_pack(pack.data(),pack.size(),load2,&e);req(parsed,e.c_str());}req(!load2.scenes.empty()&&!load2.textures.empty()&&load2.animation.size()==loading.animation.size(),"START loading pack");
 outrun::switch_runtime::SwitchRenderer renderer;
 using namespace outrun::switch_runtime;
 {const bool ok=switch_renderer_initialize(renderer,nullptr,e,&pack,nullptr,nullptr,nullptr);req(ok,e.c_str());}
 req(switch_renderer_set_frontend_token(renderer,0x44008bu),"no SUMO_FE pack: the PC frontend token selects nothing");
 req(switch_renderer_set_start_loading_scene(renderer,0u)&&switch_renderer_set_start_loading_visible(renderer,true),"show START loading");
 req(switch_renderer_draw(renderer)&&switch_renderer_stats(renderer).loading_frames==1u,"START loading drawn");
 switch_renderer_shutdown(renderer);
 fs::remove_all(root);std::printf("retail_gpu_cache_r153: %u checks passed; the START loading picture is built in memory from the game files\n",checks);return 0;
 }catch(const std::exception&x){std::fprintf(stderr,"FAILED: %s\n",x.what());return 1;}}

#include "platform/loader_asset_pack.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>
#include <vector>
using namespace outrun::platform;
namespace {
unsigned checks{};
void require(bool value,const char* message){++checks;if(!value){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
void put32(std::vector<std::uint8_t>& b,std::size_t o,std::uint32_t v){for(unsigned i=0;i<4u;++i)b[o+i]=std::uint8_t(v>>(i*8u));}
std::uint32_t crc32(const std::uint8_t* data,std::size_t size){std::uint32_t crc=0xffffffffu;for(std::size_t i=0;i<size;++i){crc^=data[i];for(unsigned bit=0;bit<8u;++bit)crc=(crc>>1u)^(0xedb88320u&std::uint32_t(0u-(crc&1u)));}return ~crc;}
std::vector<std::uint8_t> synthetic(unsigned version=LoaderAssetPackVersion){
    struct Item{std::uint32_t id,mode;std::vector<std::uint8_t> data;};
    std::vector<Item> items{
        {0xbau,2u,{0x78u,0xdau,1u,2u,3u}},
        {0xbbu,2u,{0x78u,0xdau,4u,5u,6u,7u}},
        {0x2cu,8u,{0x78u,0xdau,8u}},
        {0x33u,8u,{0x78u,0xdau,9u,10u}},
        {0x48u,8u,{0x78u,0xdau,11u,12u,13u}},
        {0x44u,9u,{0x78u,0xdau,14u,15u,16u,17u}},
        {LoaderAssetSelectTableId,0u,std::vector<std::uint8_t>(LoaderAssetSelectTableBytes,0x5au)},
    };
    if(version>=5u){
        items.push_back({LoaderAssetStartLoadingId,2u,{0x78u,0xdau,18u,19u}});
        items.push_back({LoaderAssetStartVersusId,2u,{0x78u,0xdau,20u,21u}});
    }
    if(version>=6u)for(const auto id:{0x2bu,0x2du,0x3eu,0x3fu,0x40u,0x41u,0x46u,0x4au})
        items.push_back({id,8u,{0x78u,0xdau,std::uint8_t(id)}});
    if(version>=7u){
        for(const auto id:{0x23u,0x24u,0x25u,0x26u,0x27u,0x28u,0x29u,0x2au,0x30u,0x31u})
            items.push_back({id,8u,{0x78u,0xdau,std::uint8_t(id)}});
        items.push_back({0x3cu,9u,{0x78u,0xdau,0x3cu}});
    }
    if(version>=8u)for(const auto id:{0xbau,0x57u,0x1efu,0xbbu,0xbeu})
        items.push_back({id,8u,{0x78u,0xdau,std::uint8_t(id)}});
    if(version>=9u){
        items.push_back({LoaderAssetMotionTableId,0u,std::vector<std::uint8_t>(15360u)});
        items.push_back({LoaderAssetBoneTableId,0u,std::vector<std::uint8_t>(42496u)});
        std::vector<std::uint8_t> group6(125444u),group32(52740u);
        put32(group6,0u,125440u);put32(group32,0u,52736u);
        items.push_back({LoaderAssetMotionGroup6Id,0u,std::move(group6)});
        items.push_back({LoaderAssetMotionGroup32Id,0u,std::move(group32)});
        for(const auto id:{0xbcu,0xc8u,0x0bu})
            items.push_back({id,8u,{0x78u,0xdau,std::uint8_t(id)}});
        for(const auto group:{34u,35u,36u,37u,38u,39u,23u,33u}){
            std::vector<std::uint8_t> data(8u);
            put32(data,0u,4u);
            items.push_back({loader_motion_group_id(group),0u,std::move(data)});
        }
    }
    if(version>=10u)items.push_back({0xc3u,9u,std::vector<std::uint8_t>(1712u,0x5au)});
    if(version>=11u)for(const auto id:{0x162u,0x19eu,0x180u})
        items.push_back({id,9u,{0x78u,0xdau,std::uint8_t(id)}});
    if(version>=12u)for(const auto id:{0xdbu,0xfeu,0x126u})
        items.push_back({id,9u,{0x78u,0xdau,std::uint8_t(id)}});
    for(std::uint32_t i=0u;i<LoaderAssetFrontendScriptCount;++i)
        items.push_back({LoaderAssetFrontendScriptBaseId+i,0u,
            {std::uint8_t(i+1u),std::uint8_t(0xa5u^i)}});
    const auto payload=LoaderAssetPackHeaderSize+items.size()*LoaderAssetPackRecordSize;
    std::size_t total=payload;for(const auto& item:items)total+=item.data.size();
    std::vector<std::uint8_t> out(total);
    if(version>=10u){const std::uint8_t magic[8]={'O','R','2','L','D','R','1',std::uint8_t('0'+version-10u)};std::memcpy(out.data(),magic,8);}
    else{const std::uint8_t magic[8]={'O','R','2','L','D','R',std::uint8_t('0'+version),0};std::memcpy(out.data(),magic,8);}
    put32(out,8u,version);put32(out,12u,LoaderAssetPackHeaderSize);put32(out,16u,std::uint32_t(items.size()));put32(out,20u,LoaderAssetPackRecordSize);put32(out,24u,LoaderAssetPackHeaderSize);put32(out,28u,std::uint32_t(payload));put32(out,32u,std::uint32_t(out.size()));
    std::size_t running=payload;
    for(std::size_t i=0;i<items.size();++i){const auto& item=items[i];const auto r=LoaderAssetPackHeaderSize+i*LoaderAssetPackRecordSize;put32(out,r,item.id);put32(out,r+4u,item.mode);put32(out,r+8u,std::uint32_t(running));put32(out,r+12u,std::uint32_t(item.data.size()));put32(out,r+16u,crc32(item.data.data(),item.data.size()));std::memcpy(out.data()+running,item.data.data(),item.data.size());running+=item.data.size();}
    return out;
}
}
int main(int argc,char** argv){
    auto bytes=synthetic();LoaderAssetPack pack{};std::string error;
    require(parse_loader_asset_pack(bytes.data(),bytes.size(),pack,&error),"valid synthetic OR2LDR12");
    require(pack.records.size()==LoaderAssetPackExpectedRecords&&pack.records[0].resource_id==0xbau&&pack.records[5].resource_id==0x44u&&pack.records[6].resource_id==LoaderAssetSelectTableId&&pack.records.back().resource_id==LoaderAssetFrontendScriptBaseId+63u,"loader records retained");
    require(find_loader_asset(pack,0xbau,2u)&&find_loader_asset(pack,0xbau,8u)&&find_loader_asset(pack,0x57u,8u)&&find_loader_asset(pack,0x1efu,8u)&&find_loader_asset(pack,0xbbu,8u)&&find_loader_asset(pack,0xbeu,8u)&&find_loader_asset(pack,0x2cu,8u)&&find_loader_asset(pack,0x48u,8u)&&find_loader_asset(pack,0x44u,9u)&&find_loader_asset(pack,LoaderAssetSelectTableId,0u)&&find_loader_asset(pack,LoaderAssetStartLoadingId,2u)&&find_loader_asset(pack,LoaderAssetStartVersusId,2u)&&find_loader_asset(pack,LoaderAssetGameSpritesId,8u)&&find_loader_asset(pack,0x4au,8u)&&find_loader_asset(pack,0x23u,8u)&&find_loader_asset(pack,0x31u,8u)&&find_loader_asset(pack,0x3cu,9u)&&find_loader_asset(pack,LoaderAssetFrontendScriptBaseId+63u,0u)&&!find_loader_asset(pack,0x48u,2u),"loader lookup id/mode");
    require(find_loader_asset(pack,LoaderAssetMotionTableId,0u)&&
            find_loader_asset(pack,LoaderAssetBoneTableId,0u)&&
            find_loader_asset(pack,LoaderAssetMotionGroup6Id,0u)&&
            find_loader_asset(pack,LoaderAssetMotionGroup32Id,0u)&&
            !find_loader_asset(pack,LoaderAssetMotionGroup6Id,8u),
            "motion graph is distinct from object loader modes");
    require(find_loader_asset(pack,0xbcu,8u)&&find_loader_asset(pack,0xc8u,8u)&&
            find_loader_asset(pack,0x0bu,8u),
            "START driver and selected 250 GTO source archives present");
    require(find_loader_asset(pack,loader_motion_group_id(34u),0u)&&
            find_loader_asset(pack,loader_motion_group_id(39u),0u)&&
            find_loader_asset(pack,loader_motion_group_id(23u),0u)&&
            find_loader_asset(pack,loader_motion_group_id(33u),0u),
            "START follow-on motion graph groups retained");
    require(find_loader_asset(pack,0xc3u,9u)&&
            find_loader_asset(pack,0xc3u,9u)->bytes.size()==1712u&&
            !find_loader_asset(pack,0xc3u,8u),
            "stage-48 direct CHR asset has its own request mode");
    require(find_loader_asset(pack,0x162u,9u)&&find_loader_asset(pack,0x19eu,9u)&&
            find_loader_asset(pack,0x180u,9u),
            "stage-51 PC route-world archives retain resource IDs and mode");
    require(find_loader_asset(pack,0xdbu,9u)&&find_loader_asset(pack,0xfeu,9u)&&
            find_loader_asset(pack,0x126u,9u),
            "first ordinary race BEAC archives retain descriptor IDs and mode");
    const auto old11=synthetic(11u);
    require(parse_loader_asset_pack(old11.data(),old11.size(),pack,&error)&&
            pack.records.size()==52u+LoaderAssetFrontendScriptCount&&
            !find_loader_asset(pack,0xdbu,9u),
            "OR2LDR11 remains readable without claiming ordinary race archives");
    const auto old10=synthetic(10u);
    require(parse_loader_asset_pack(old10.data(),old10.size(),pack,&error)&&
            pack.records.size()==49u+LoaderAssetFrontendScriptCount&&
            !find_loader_asset(pack,0x162u,9u),
            "OR2LDR10 remains readable without claiming world archives");
    const auto old9=synthetic(9u);
    require(parse_loader_asset_pack(old9.data(),old9.size(),pack,&error)&&
            pack.records.size()==48u+LoaderAssetFrontendScriptCount&&
            !find_loader_asset(pack,0xc3u,9u),
            "OR2LDR9 remains readable but cannot satisfy stage 48");
    const auto old4=synthetic(4u);
    require(parse_loader_asset_pack(old4.data(),old4.size(),pack,&error)&&pack.records.size()==71u&&!find_loader_asset(pack,LoaderAssetStartLoadingId,2u),"OR2LDR4 remains readable without claiming START resources");
    const auto old5=synthetic(5u);
    require(parse_loader_asset_pack(old5.data(),old5.size(),pack,&error)&&pack.records.size()==73u&&!find_loader_asset(pack,LoaderAssetGameSpritesId,8u),"OR2LDR5 remains readable without claiming GAME owner resources");
    const auto old6=synthetic(6u);
    require(parse_loader_asset_pack(old6.data(),old6.size(),pack,&error)&&pack.records.size()==81u&&!find_loader_asset(pack,0x31u,8u),"OR2LDR6 remains readable without new meter resources");
    const auto old7=synthetic(7u);
    require(parse_loader_asset_pack(old7.data(),old7.size(),pack,&error)&&pack.records.size()==92u&&!find_loader_asset(pack,0xbeu,8u),"OR2LDR7 remains readable without the stage-23 resources");
    const auto old8=synthetic(8u);
    require(parse_loader_asset_pack(old8.data(),old8.size(),pack,&error)&&pack.records.size()==97u&&!find_loader_asset(pack,LoaderAssetMotionGroup6Id,0u),"OR2LDR8 remains readable without the motion graph");
    auto bad=bytes;bad[0]='X';require(!parse_loader_asset_pack(bad.data(),bad.size(),pack,&error),"loader bad magic rejected");
    bad=bytes;bad.back()^=1u;require(!parse_loader_asset_pack(bad.data(),bad.size(),pack,&error),"loader corrupt payload rejected");
    bad=bytes;put32(bad,64u,0xbbu);require(!parse_loader_asset_pack(bad.data(),bad.size(),pack,&error),"loader wrong record order rejected");
    if(argc==2){LoaderAssetPack real{};require(load_loader_asset_pack_file(argv[1],real,&error),"generated OR2LDR12 loads");require(real.records.size()==LoaderAssetPackExpectedRecords&&real.records[0].bytes.size()==46468u&&real.records[1].bytes.size()==11671u&&real.records[2].bytes.size()==491981u&&real.records[3].bytes.size()==79361u&&real.records[4].bytes.size()==74717u&&real.records[5].bytes.size()==3727034u&&real.records[6].bytes.size()==LoaderAssetSelectTableBytes&&real.records[7].bytes.size()==172301u&&real.records[8].bytes.size()==432758u&&real.records[9].bytes.size()==725647u&&real.records[16].bytes.size()==92583u&&real.records[17].bytes.size()==4095u&&real.records[26].bytes.size()==4040u&&real.records[27].bytes.size()==28079u&&real.records[29].bytes.size()==844991u&&real.records[30].bytes.size()==65300u&&real.records[32].bytes.size()==178116u&&real.records[33].bytes.size()==15360u&&real.records[34].bytes.size()==42496u&&real.records[35].bytes.size()==125444u&&real.records[36].bytes.size()==52740u&&real.records[37].bytes.size()==1138698u&&real.records[38].bytes.size()==89028u&&real.records[39].bytes.size()==353787u&&real.records[48].bytes.size()==1712u&&real.records[49].bytes.size()==3145883u&&real.records[50].bytes.size()==54488u&&real.records[51].bytes.size()==767150u&&find_loader_asset(real,0xdbu,9u)&&find_loader_asset(real,0xfeu,9u)&&find_loader_asset(real,0x126u,9u),"generated FXT loader, START, GAME, world and decoded motion asset sizes");std::size_t frontend_bytes=0u;for(std::size_t i=55u;i<real.records.size();++i)frontend_bytes+=real.records[i].bytes.size();require(frontend_bytes==LoaderAssetFrontendScriptBytes,"generated frontend script total");}
    std::printf("loader_asset_pack: %u checks passed\n",checks);return 0;
}

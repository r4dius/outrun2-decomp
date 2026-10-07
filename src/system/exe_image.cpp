#include "system/exe_image.hpp"
#include "ports/pecompact_memory.hpp"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <zlib.h>
#ifdef _WIN32
#include <io.h>
#else
#include "system/files.hpp"
#endif
namespace outrun::platform {
const ExeImageRange ExeImageRanges[]={
#include "system/exe_image_ranges.inc"
};
const std::size_t ExeImageRangeCount=sizeof(ExeImageRanges)/sizeof(ExeImageRanges[0]);
namespace {
constexpr char CacheMagic[8]={'O','R','2','I','M','G','1','\0'};
constexpr std::uint32_t CacheVersion=1u;
constexpr std::size_t CacheHeader=64u;
constexpr std::uint32_t ImportTableVa=0x00596000u,ImportTableSize=0x300u;   // ADVAPI32 596000 .. ole32 5962F0 + terminator
std::vector<std::uint8_t>& image_storage(){static std::vector<std::uint8_t> image;return image;}
struct CopyEntry{std::uint8_t* destination;std::uint32_t va,size;};
std::vector<CopyEntry>& copies(){static std::vector<CopyEntry> list;return list;}
std::vector<void (*)()>& binders(){static std::vector<void (*)()> list;return list;}
struct Trace{
    std::FILE* file=nullptr;
    std::vector<std::pair<std::uint32_t,std::uint32_t>> seen;
    Trace(){if(const char* p=std::getenv("OR2_EXE_TRACE"))file=std::fopen(p,"ab");}
    ~Trace(){if(!file)return;std::sort(seen.begin(),seen.end());seen.erase(std::unique(seen.begin(),seen.end()),seen.end());
             for(const auto& r:seen)std::fprintf(file,"%08X %X\n",r.first,r.second);std::fclose(file);}
};
Trace& trace(){static Trace t;return t;}
bool fail(std::string* error,const std::string& text){if(error)*error=text;return false;}
std::uint32_t rd32(const std::uint8_t* p){std::uint32_t v;std::memcpy(&v,p,4);return v;}
std::uint16_t rd16(const std::uint8_t* p){std::uint16_t v;std::memcpy(&v,p,2);return v;}
void wr32(std::uint8_t* p,std::uint32_t v){std::memcpy(p,&v,4);}
std::uint32_t crc(const std::vector<std::uint8_t>& v){
    uLong c=::crc32(0L,Z_NULL,0);std::size_t done=0;
    while(done<v.size()){const auto n=static_cast<uInt>(std::min<std::size_t>(v.size()-done,1u<<30));c=::crc32(c,v.data()+done,n);done+=n;}
    return static_cast<std::uint32_t>(c);
}
bool read_file(const std::string& path,std::vector<std::uint8_t>& out,std::size_t limit,std::string* error){
    std::FILE* f=std::fopen(path.c_str(),"rb");if(!f)return fail(error,"cannot open "+path);
    if(std::fseek(f,0,SEEK_END)!=0){std::fclose(f);return fail(error,"cannot size "+path);}
    const long n=std::ftell(f);
    if(n<0||static_cast<unsigned long>(n)>limit){std::fclose(f);return fail(error,"unexpected size: "+path);}
    std::rewind(f);out.resize(static_cast<std::size_t>(n));
    const auto got=out.empty()?0u:std::fread(out.data(),1,out.size(),f);std::fclose(f);
    if(got!=out.size())return fail(error,"cannot read "+path);
    return true;
}
std::vector<std::string> list_files(const std::string& dir){
    std::vector<std::string> names;
#ifdef _WIN32
    _finddata_t d{};const auto h=_findfirst((dir+"/*").c_str(),&d);
    if(h!=-1){do{if(!(d.attrib&_A_SUBDIR))names.push_back(d.name);}while(_findnext(h,&d)==0);_findclose(h);}
#else
    names=directory_names(dir);
#endif
    std::sort(names.begin(),names.end());
    return names;
}
std::string lower(std::string s){for(auto& c:s)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));return s;}
}

Sha256Digest exe_image_range_set_id(){
    Sha256 s;
    for(std::size_t i=0;i<ExeImageRangeCount;++i){
        std::uint8_t row[8];wr32(row,ExeImageRanges[i].va);wr32(row+4,ExeImageRanges[i].size);
        s.update(row,8);s.update(ExeImageRanges[i].sha256.data(),32);
    }
    return s.finish();
}

bool exe_image_loaded(){return image_storage().size()==ExeImageSize;}

const std::uint8_t* exe_image_bytes(std::uint32_t va,std::uint32_t size){
    if(!exe_image_loaded()&&!exe_image_autoload())throw std::runtime_error("OR2006C2C.EXE image not loaded");
    if(va<ExeImageBase||va-ExeImageBase>ExeImageSize||size>ExeImageSize-(va-ExeImageBase)){
        char t[96];std::snprintf(t,sizeof t,"PC range %08X+%X is outside OR2006C2C.EXE",va,size);throw std::runtime_error(t);
    }
    if(trace().file)trace().seen.emplace_back(va,size);
    return image_storage().data()+(va-ExeImageBase);
}
std::uint32_t exe_image_u32(std::uint32_t va){return rd32(exe_image_bytes(va,4));}

bool exe_image_map_pe(const std::vector<std::uint8_t>& file,std::vector<std::uint8_t>& image,std::string* error){
    // PEC2 is stored in the first section's relocation-pointer field.
    if(file.size()>=0x40u){
        const std::size_t pe=rd32(file.data()+0x3C);
        if(pe<=file.size()&&file.size()-pe>=24u){
            const std::size_t optional=rd16(file.data()+pe+20);
            const std::size_t table=pe+24u+optional;
            if(rd16(file.data()+pe+6)>0&&table<=file.size()&&file.size()-table>=40u&&
               std::memcmp(file.data()+table+24,"PEC2",4)==0)
            {   // an unpacked copy can keep the PEC2 field: then it maps as a plain PE below
                std::string why;if(outrun_pecompact::decompress(file.data(),file.size(),image,&why))return true;
                image.clear();}
        }
    }

    if(file.size()<0x40u||file[0]!='M'||file[1]!='Z')return fail(error,"not a Windows executable");
    const auto pe=rd32(file.data()+0x3C);
    if(pe>file.size()||file.size()-pe<24u+0xE0u||std::memcmp(file.data()+pe,"PE\0\0",4)!=0)return fail(error,"not a Windows executable");
    const auto count=rd16(file.data()+pe+6),optional=rd16(file.data()+pe+20);
    const auto* opt=file.data()+pe+24;
    if(rd16(opt)!=0x10Bu||rd32(opt+28)!=ExeImageBase||rd32(opt+56)!=ExeImageSize)return fail(error,"not OutRun 2006 Coast 2 Coast (OR2006C2C.EXE)");
    const std::size_t table=pe+24u+optional;
    if(table>file.size()||(file.size()-table)/40u<count)return fail(error,"damaged executable header");
    image.assign(ExeImageSize,0);
    for(unsigned i=0;i<count;++i){
        const auto* s=file.data()+table+i*40u;
        const auto rva=rd32(s+12),raw_size=rd32(s+16),raw=rd32(s+20);
        if(rva>=ExeImageSize)continue;
        const auto n=std::min<std::size_t>({raw_size,ExeImageSize-rva,raw<file.size()?file.size()-raw:0u});
        if(n)std::memcpy(image.data()+rva,file.data()+raw,n);
    }
    // The import address table holds whatever the decompressing tool left
    // there (thunks, resolved addresses or zeros); the port never reads it.
    std::memset(image.data()+(ImportTableVa-ExeImageBase),0,ImportTableSize);
    return true;
}

bool exe_image_verify(const std::vector<std::uint8_t>& image,std::string* error,const ExeImageProgress& progress,std::size_t* bad){
    if(image.size()!=ExeImageSize)return fail(error,"image size mismatch");
    std::uint64_t total=0,done=0;
    for(std::size_t i=0;i<ExeImageRangeCount;++i)total+=ExeImageRanges[i].size;
    for(std::size_t i=0;i<ExeImageRangeCount;++i){
        const auto& r=ExeImageRanges[i];
        if(r.va<ExeImageBase||r.va-ExeImageBase>ExeImageSize||r.size>ExeImageSize-(r.va-ExeImageBase)){if(bad)*bad=i;return fail(error,"range table outside the image");}
        if(sha256(image.data()+(r.va-ExeImageBase),r.size)!=r.sha256){
            if(bad)*bad=i;
            char t[128];std::snprintf(t,sizeof t,"OR2006C2C.EXE data at %08X (%u bytes) differs from the supported Steam build",r.va,r.size);
            return fail(error,t);
        }
        done+=r.size;if(progress)progress(done,total);
    }
    return true;
}

void exe_image_install(std::vector<std::uint8_t>&& image){
    image_storage()=std::move(image);
    for(const auto& c:copies())std::memcpy(c.destination,exe_image_bytes(c.va,c.size),c.size);
    for(auto fn:binders())fn();
}


ExeCopy::ExeCopy(std::uint8_t* destination,std::uint32_t va,std::uint32_t size){
    copies().push_back({destination,va,size});
    if(exe_image_loaded())std::memcpy(destination,exe_image_bytes(va,size),size);
}

ExeBind::ExeBind(void (*fn)()){
    binders().push_back(fn);
    if(exe_image_loaded())fn();
}

bool exe_image_write_cache(const std::string& path,const std::vector<std::uint8_t>& image,std::string* error){
    if(image.size()!=ExeImageSize)return fail(error,"image size mismatch");
    uLongf packed=compressBound(static_cast<uLong>(image.size()));
    std::vector<std::uint8_t> out(CacheHeader+packed,0);
    if(compress2(out.data()+CacheHeader,&packed,image.data(),static_cast<uLong>(image.size()),6)!=Z_OK)return fail(error,"cannot compress the cache");
    out.resize(CacheHeader+packed);
    std::memcpy(out.data(),CacheMagic,8);wr32(out.data()+8,CacheVersion);wr32(out.data()+12,ExeImageBase);
    wr32(out.data()+16,ExeImageSize);wr32(out.data()+20,static_cast<std::uint32_t>(packed));wr32(out.data()+24,crc(image));
    const auto id=exe_image_range_set_id();std::memcpy(out.data()+32,id.data(),32);
    const std::string temp=path+".tmp";
    std::FILE* f=std::fopen(temp.c_str(),"wb");if(!f)return fail(error,"cannot create "+temp);
    const bool ok=std::fwrite(out.data(),1,out.size(),f)==out.size();
    if(std::fclose(f)!=0||!ok){std::remove(temp.c_str());return fail(error,"cannot write "+temp);}
    std::remove(path.c_str());
    if(std::rename(temp.c_str(),path.c_str())!=0){std::remove(temp.c_str());return fail(error,"cannot write "+path);}
    return true;
}

bool exe_image_load_cache(const std::string& path,std::string* error){
    std::vector<std::uint8_t> file;
    if(!read_file(path,file,64u<<20,error))return false;
    if(file.size()<CacheHeader||std::memcmp(file.data(),CacheMagic,8)!=0||rd32(file.data()+8)!=CacheVersion||
       rd32(file.data()+12)!=ExeImageBase||rd32(file.data()+16)!=ExeImageSize||rd32(file.data()+20)!=file.size()-CacheHeader)
        return fail(error,"cache file from another version or damaged");
    std::vector<std::uint8_t> image(ExeImageSize);uLongf size=ExeImageSize;
    if(uncompress(image.data(),&size,file.data()+CacheHeader,static_cast<uLong>(file.size()-CacheHeader))!=Z_OK||size!=ExeImageSize||crc(image)!=rd32(file.data()+24))
        return fail(error,"cache file damaged");
    const auto id=exe_image_range_set_id();
    if(std::memcmp(file.data()+32,id.data(),32)!=0){
        // Built for another range table: check again from the cached image.
        if(!exe_image_verify(image,error))return false;
        std::string ignored;exe_image_write_cache(path,image,&ignored);
    }
    exe_image_install(std::move(image));
    return true;
}

bool exe_image_find_source(const std::string& dir,std::vector<std::uint8_t>& image,std::string& path,
                           ExeSource* found,std::string* error,const ExeImageProgress& progress){
    // SHA-256 of the Steam OR2006C2C.EXE as sold (PECompact-compressed).
    static const Sha256Digest SteamPacked={0x48,0x76,0x71,0x2f,0x50,0x16,0x40,0x95,0x1d,0x89,0x20,0x69,0xdb,0x82,0x6e,0x43,
                                           0x1a,0xed,0xe3,0xe4,0xab,0x87,0x68,0x6f,0xc1,0xd5,0x56,0xe2,0x6d,0xf9,0xfe,0xc1};
    if(found)*found=ExeSource::Missing;
    std::string compressed,other;
    for(const auto& name:list_files(dir)){
        const auto l=lower(name);
        if(l.size()<4||l.compare(l.size()-4,4,".exe")!=0)continue;
        std::vector<std::uint8_t> file;std::string e;
        if(!read_file(dir+"/"+name,file,64u<<20,&e))continue;
        const bool steam=sha256(file.data(),file.size())==SteamPacked;
        if(steam){
            if(compressed.empty())compressed=name;
        }
        std::vector<std::uint8_t> mapped;
        if(!exe_image_map_pe(file,mapped,&e)){
            // Another program (Config.exe...), or an OR2006C2C.EXE of another edition.
            if(!steam&&l=="or2006c2c.exe"&&other.empty())other=name+": not the Steam version";
            continue;
        }
        if(!exe_image_verify(mapped,&e,progress)){if(!steam&&other.empty())other=name+": "+e;continue;}
        image=std::move(mapped);path=dir+"/"+name;
        if(found)*found=ExeSource::Found;
        return true;
    }
    if(!other.empty()){if(found)*found=ExeSource::OtherVersion;return fail(error,other);}
    if(!compressed.empty()){if(found)*found=ExeSource::Compressed;return fail(error,compressed+" is the compressed Steam file");}
    return fail(error,"no OR2006C2C.EXE in "+dir);
}

bool exe_image_autoload(){
    if(exe_image_loaded())return true;
    static bool tried=false;if(tried)return false;tried=true;
    const char* path=std::getenv("OR2_EXE");if(!path||!*path)return false;
    std::string error;
    if(exe_image_load_cache(path,&error))return true;
    std::vector<std::uint8_t> file,image;
    if(!read_file(path,file,64u<<20,&error)||!exe_image_map_pe(file,image,&error)||!exe_image_verify(image,&error)){
        std::fprintf(stderr,"OR2_EXE=%s: %s\n",path,error.c_str());return false;
    }
    exe_image_install(std::move(image));
    return true;
}
namespace {
// Development builds and tests name the image with OR2_EXE; the games load
// their cache explicitly at startup (system/setup_screen.hpp).
[[maybe_unused]] const bool autoloaded=exe_image_autoload();
}
}

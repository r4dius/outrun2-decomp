#include "enhancements/video.hpp"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace outrun::enhancements {
namespace {
VideoBackend* g_backend{};
std::string trim(std::string s){
    while(!s.empty()&&(s.back()=='\r'||s.back()=='\n'||s.back()==' '||s.back()=='\t'))s.pop_back();
    std::size_t i=0;while(i<s.size()&&(s[i]==' '||s[i]=='\t'))++i;
    return s.substr(i);
}
}
void set_video_backend(VideoBackend* backend){g_backend=backend;}
VideoBackend* video_backend(){return g_backend;}

Resolution resolution_at(std::uint32_t height,bool widescreen){
    const std::uint32_t width=widescreen?(height*16u+8u)/9u:height*4u/3u;
    return {(width+1u)&~1u,height};
}
std::string resolution_label(const Resolution& r){return std::to_string(r.width)+"x"+std::to_string(r.height);}
const char* antialiasing_label(Antialiasing a){
    switch(a){
    case Antialiasing::Msaa2x:return "MSAA 2x";
    case Antialiasing::Msaa4x:return "MSAA 4x";
    case Antialiasing::Fxaa:return "FXAA";
    default:return "Off";
    }
}
const char* antialiasing_key(Antialiasing a){
    switch(a){
    case Antialiasing::Msaa2x:return "msaa2";
    case Antialiasing::Msaa4x:return "msaa4";
    case Antialiasing::Fxaa:return "fxaa";
    default:return "off";
    }
}
bool parse_antialiasing(const std::string& value,Antialiasing& out){
    const auto v=trim(value);
    if(v=="off"||v=="0"){out=Antialiasing::Off;return true;}
    if(v=="msaa2"||v=="2"){out=Antialiasing::Msaa2x;return true;}
    if(v=="msaa4"||v=="4"){out=Antialiasing::Msaa4x;return true;}
    if(v=="fxaa"){out=Antialiasing::Fxaa;return true;}
    return false;
}
bool parse_resolution(const std::string& value,Resolution& out){
    const auto v=trim(value);const auto x=v.find('x');
    if(x==std::string::npos)return false;
    char* end{};const auto w=std::strtoul(v.c_str(),&end,10);
    if(end!=v.c_str()+x)return false;
    const auto h=std::strtoul(v.c_str()+x+1,&end,10);
    if(*end||w<320||h<240||w>8192||h>8192)return false;
    out={std::uint32_t(w),std::uint32_t(h)};return true;
}
std::string resolution_key(const Resolution& r){return std::to_string(r.width)+"x"+std::to_string(r.height);}
std::uint32_t nearest_height(const std::vector<std::uint32_t>& list,std::uint32_t want){
    if(list.empty())return want;
    std::uint32_t best=list.front();
    for(const auto h:list)if(std::llabs(std::int64_t(h)-want)<std::llabs(std::int64_t(best)-want))best=h;
    return best;
}

bool write_options(const std::string& path,const std::vector<std::pair<std::string,std::string>>& values){
    std::vector<std::string> lines;
    {std::ifstream in(path,std::ios::binary);std::string l;while(std::getline(in,l)){if(!l.empty()&&l.back()=='\r')l.pop_back();lines.push_back(l);}}
    std::vector<bool> written(values.size());
    for(auto& l:lines){
        const auto eq=l.find('=');if(l.empty()||l[0]=='#'||eq==std::string::npos)continue;
        const auto key=trim(l.substr(0,eq));
        for(std::size_t i=0;i<values.size();++i)if(values[i].first==key){l=key+"="+values[i].second;written[i]=true;}
    }
    for(std::size_t i=0;i<values.size();++i)if(!written[i])lines.push_back(values[i].first+"="+values[i].second);
    std::ostringstream out;for(const auto& l:lines)out<<l<<'\n';
    const auto temp=path+".tmp";
    {std::ofstream f(temp,std::ios::binary|std::ios::trunc);if(!f)return false;f<<out.str();if(!f.flush())return false;}
    if(std::rename(temp.c_str(),path.c_str())!=0){std::remove(temp.c_str());return false;}
    return true;
}
}

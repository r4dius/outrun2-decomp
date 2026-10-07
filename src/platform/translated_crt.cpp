// Guest C runtime for translated modules (translated_crt.hpp).
#include "platform/translated_crt.hpp"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
namespace outrun::platform {
GuestHeap::GuestHeap(std::uint32_t base,std::uint32_t size):base_(base),size_(size),bytes_(size){free_[base]=size;}
void GuestHeap::map(PcRaceMemory& m){m.map(base_,bytes_.data(),bytes_.size());}
std::uint32_t GuestHeap::alloc(std::uint32_t n){
    const std::uint32_t need=std::max<std::uint32_t>(16u,(n+15u)&~15u);
    for(auto it=free_.begin();it!=free_.end();++it){
        if(it->second<need)continue;
        const std::uint32_t a=it->first,rest=it->second-need;
        free_.erase(it);
        if(rest)free_[a+need]=rest;
        used_[a]=need;used_bytes_+=need;
        std::memset(bytes_.data()+(a-base_),0,need);
        return a;
    }
    return 0;
}
void GuestHeap::free(std::uint32_t a){
    const auto it=used_.find(a);
    if(it==used_.end())return;
    std::uint32_t start=a,size=it->second;used_bytes_-=size;used_.erase(it);
    auto next=free_.lower_bound(start);
    if(next!=free_.end()&&next->first==start+size){size+=next->second;next=free_.erase(next);}
    if(next!=free_.begin()){auto prev=std::prev(next);if(prev->first+prev->second==start){start=prev->first;size+=prev->second;free_.erase(prev);}}
    free_[start]=size;
}
std::uint32_t GuestHeap::size_of(std::uint32_t a)const{const auto it=used_.find(a);return it==used_.end()?0u:it->second;}
std::uint32_t GuestHeap::realloc(PcRaceMemory& m,std::uint32_t a,std::uint32_t n){
    if(!a)return alloc(n);
    if(!n){free(a);return 0;}
    const auto old=size_of(a);
    if(old>=n)return a;
    const auto b=alloc(n);
    if(!b)return 0;
    std::memcpy(m.at(b,old,true),m.at(a,old),old);
    free(a);return b;
}
std::string guest_string(PcRaceMemory& m,std::uint32_t a){std::string v;if(!a)return v;for(;m.u8(a);++a)v+=char(m.u8(a));return v;}
void guest_put_string(PcRaceMemory& m,std::uint32_t a,const std::string& s){
    for(std::size_t i=0;i<s.size();++i)m.put8(a+std::uint32_t(i),std::uint8_t(s[i]));
    m.put8(a+std::uint32_t(s.size()),0);
}
std::string guest_format(PcRaceMemory& m,std::uint32_t format,const std::function<std::uint32_t()>& arg){
    std::string out;
    for(std::uint32_t f=format;;++f){
        const char ch=char(m.u8(f));
        if(!ch)break;
        if(ch!='%'){out+=ch;continue;}
        std::string spec="%";
        for(;;){const char x=char(m.u8(++f));spec+=x;if(std::strchr("-+ #0",x)==nullptr||x==0)break;}
        while(std::isdigit(std::uint8_t(spec.back())))spec+=char(m.u8(++f));
        if(spec.back()=='.'){spec+=char(m.u8(++f));while(std::isdigit(std::uint8_t(spec.back())))spec+=char(m.u8(++f));}
        if(spec.back()=='l'||spec.back()=='h'){spec.pop_back();spec+=char(m.u8(++f));}   // the 32-bit target: long = int
        const char conv=spec.back();char buf[96];
        switch(conv){
        case '%':out+='%';continue;
        case 'd':case 'i':std::snprintf(buf,sizeof buf,spec.c_str(),std::int32_t(arg()));break;
        case 'u':case 'x':case 'X':case 'o':std::snprintf(buf,sizeof buf,spec.c_str(),arg());break;
        case 'c':std::snprintf(buf,sizeof buf,spec.c_str(),int(std::uint8_t(arg())));break;
        case 's':{const auto v=guest_string(m,arg());
            std::vector<char> b(v.size()+64);std::snprintf(b.data(),b.size(),spec.c_str(),v.c_str());out+=b.data();continue;}
        default:throw std::runtime_error("sprintf conversion "+spec);
        }
        out+=buf;
    }
    return out;
}
namespace {
// sscanf: %d %i %u %x %s %c (width, '*' suppression), literal characters and white space.
std::uint32_t guest_scan(PcRaceMemory& m,std::uint32_t input,std::uint32_t format,const std::function<std::uint32_t()>& arg){
    std::uint32_t in=input,count=0;
    for(std::uint32_t f=format;;++f){
        char ch=char(m.u8(f));
        if(!ch)break;
        if(std::isspace(std::uint8_t(ch))){while(std::isspace(m.u8(in)))++in;continue;}
        if(ch!='%'){if(char(m.u8(in))!=ch)break;++in;continue;}
        ch=char(m.u8(++f));
        bool skip=false;if(ch=='*'){skip=true;ch=char(m.u8(++f));}
        int width=0;while(std::isdigit(std::uint8_t(ch))){width=width*10+(ch-'0');ch=char(m.u8(++f));}
        if(ch=='l'||ch=='h')ch=char(m.u8(++f));
        if(ch=='%'){if(m.u8(in)!='%')break;++in;continue;}
        if(ch!='c')while(std::isspace(m.u8(in)))++in;
        if(!m.u8(in))break;
        if(ch=='d'||ch=='i'||ch=='u'||ch=='x'||ch=='X'){
            std::string t;const int limit=width?width:64;
            if((m.u8(in)=='-'||m.u8(in)=='+')&&int(t.size())<limit)t+=char(m.u8(in++));
            const bool hex=ch=='x'||ch=='X';
            while(int(t.size())<limit&&(hex?std::isxdigit(m.u8(in)):std::isdigit(m.u8(in))))t+=char(m.u8(in++));
            if(t.empty()||t=="-"||t=="+")break;
            const long v=std::strtol(t.c_str(),nullptr,hex?16:10);
            if(!skip){m.put32(arg(),std::uint32_t(v));++count;}
        }else if(ch=='s'){
            std::string t;const int limit=width?width:0x7fffffff;
            while(int(t.size())<limit&&m.u8(in)&&!std::isspace(m.u8(in)))t+=char(m.u8(in++));
            if(!skip){guest_put_string(m,arg(),t);++count;}
        }else if(ch=='c'){
            const int n=width?width:1;const std::uint32_t out=skip?0u:arg();
            for(int i=0;i<n&&m.u8(in);++i,++in)if(!skip)m.put8(out+std::uint32_t(i),m.u8(in));
            if(!skip)++count;
        }else throw std::runtime_error(std::string("sscanf conversion %")+ch);
    }
    return count;
}
}
bool translated_crt_call(PcRaceMemory& m,GuestHeap& heap,const PcRaceCall& k,std::uint32_t& eax){
    const auto& a=k.args;
    switch(k.pc){
    case 0x580253u:case 0x5802cfu:case 0x580c33u:                    // malloc / operator new (5802CF, 580C33)
        eax=heap.alloc(a[0]);
        if(!eax&&k.pc!=0x580253u)throw std::runtime_error("guest heap full (operator new)");
        return true;
    case 0x580bc2u:case 0x5801a7u:heap.free(a[0]);eax=0;return true; // free / operator delete
    case 0x580195u:eax=0;return true;                               // _callnewh: no new handler
    case 0x581b19u:eax=heap.realloc(m,a[0],a[1]);return true;       // realloc
    case 0x580340u:                                                 // memmove / memcpy
        if(a[2]){std::vector<std::uint8_t> t(a[2]);std::memcpy(t.data(),m.at(a[1],a[2]),a[2]);std::memcpy(m.at(a[0],a[2],true),t.data(),a[2]);}
        eax=a[0];return true;
    case 0x5810d0u:{                                                // strchr
        const auto c=std::uint8_t(a[1]);std::uint32_t p=a[0];
        for(;;++p){const auto v=m.u8(p);if(v==c){eax=p;return true;}if(!v){eax=0;return true;}}}
    case 0x581780u:{                                                // strncpy
        std::uint32_t i=0;for(;i<a[2]&&m.u8(a[1]+i);++i)m.put8(a[0]+i,m.u8(a[1]+i));
        for(;i<a[2];++i)m.put8(a[0]+i,0);eax=a[0];return true;}
    case 0x5802ddu:{                                                // sprintf(out, format, ...)
        std::uint32_t next=2;const auto out=guest_format(m,a[1],[&]{return a.at(next++);});
        guest_put_string(m,a[0],out);eax=std::uint32_t(out.size());return true;}
    case 0x580265u:{                                                // _vsnprintf(out, size, format, va_list)
        std::uint32_t list=a[3];const auto out=guest_format(m,a[2],[&]{const auto v=m.u32(list);list+=4;return v;});
        const std::size_t n=std::min<std::size_t>(out.size(),a[1]);
        for(std::size_t i=0;i<n;++i)m.put8(a[0]+std::uint32_t(i),std::uint8_t(out[i]));
        if(n<a[1])m.put8(a[0]+std::uint32_t(n),0);
        eax=out.size()<=a[1]?std::uint32_t(out.size()):0xffffffffu;return true;}
    case 0x580f62u:{                                                // sscanf(input, format, ...)
        std::uint32_t next=2;eax=guest_scan(m,a[0],a[1],[&]{return a.at(next++);});return true;}
    case 0x5822d2u:eax=a[0]>='a'&&a[0]<='z'?a[0]-0x20u:a[0];return true;   // toupper (C locale: a..z only)
    case 0x580f92u:{                                                // printf (the debug console): formatted, not shown
        std::uint32_t next=1;const auto out=guest_format(m,a[0],[&]{return a.at(next++);});
        if(std::getenv("OR2_CRT_PRINTF"))std::fputs(out.c_str(),stderr);
        eax=std::uint32_t(out.size());return true;}
    }
    return false;
}
}

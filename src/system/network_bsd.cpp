// BSD socket platform for the LAN layer (network_bsd.hpp): any system with
// BSD sockets; interface discovery and the host test LAN where getifaddrs and
// Unix sockets exist (Linux, macOS).
#include "system/network_bsd.hpp"
#if __has_include(<sys/socket.h>)
#define OR2_NET_BSD 1
#endif
#if defined(OR2_NET_BSD)&&__has_include(<ifaddrs.h>)
#define OR2_NET_VLAN 1
#include <ifaddrs.h>
#include <net/if.h>
#include <dirent.h>
#include <sys/un.h>
#include <cstdio>
#include <ctime>
#include <cstdlib>
#include <cstring>
#include <string>
#endif
#if defined(OR2_NET_BSD)
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
namespace outrun::platform {
#if defined(OR2_NET_BSD)
namespace {
std::vector<PcNetworkInterface> interfaces(){
    std::vector<PcNetworkInterface> out;
#if defined(OR2_NET_VLAN)
    ifaddrs* list=nullptr;
    if(getifaddrs(&list)!=0)return out;
    for(auto* i=list;i;i=i->ifa_next){
        if(!i->ifa_addr||i->ifa_addr->sa_family!=AF_INET||!(i->ifa_flags&IFF_UP)||(i->ifa_flags&IFF_LOOPBACK))continue;
        const auto address=reinterpret_cast<sockaddr_in*>(i->ifa_addr)->sin_addr.s_addr;
        const auto mask=i->ifa_netmask?reinterpret_cast<sockaddr_in*>(i->ifa_netmask)->sin_addr.s_addr:htonl(0xffffff00u);
        out.push_back({address,(address&mask)|~mask,mask});
    }
    freeifaddrs(list);
#endif
    return out;
}
}
#if defined(OR2_NET_VLAN)
// Host test LAN (OR2_NET_VLAN=<directory>:<a.b.c.d>): every instance on the machine
// is one address of a /24; a UDP port is a Unix datagram socket <directory>/<ip>:<port>;
// a datagram to x.y.z.255 goes to every other instance's socket on that port.
PcNetworkPlatform vlan_platform(const std::string& spec){
    const auto colon=spec.rfind(':');
    const std::string dir=spec.substr(0,colon),ip=spec.substr(colon+1);
    in_addr own{};inet_pton(AF_INET,ip.c_str(),&own);
    const std::uint32_t self=own.s_addr,mask=htonl(0xffffff00u);
    auto path=[dir](std::uint32_t address,std::uint16_t port){
        in_addr a{};a.s_addr=address;char text[INET_ADDRSTRLEN]{};inet_ntop(AF_INET,&a,text,sizeof text);
        return dir+"/"+text+":"+std::to_string(port);};
    PcNetworkPlatform p;
    p.open_udp=[self,path](std::uint16_t port,bool)->int{
        const int fd=socket(AF_UNIX,SOCK_DGRAM,0);if(fd<0)return -1;
        sockaddr_un a{};a.sun_family=AF_UNIX;const auto name=path(self,port);
        if(name.size()>=sizeof a.sun_path){close(fd);return -1;}
        std::strcpy(a.sun_path,name.c_str());unlink(a.sun_path);
        if(bind(fd,reinterpret_cast<sockaddr*>(&a),sizeof a)<0){close(fd);return -1;}
        const int flags=fcntl(fd,F_GETFL,0);(void)fcntl(fd,F_SETFL,(flags<0?0:flags)|O_NONBLOCK);
        return fd;};
    p.close=[](int fd){if(fd>=0)close(fd);};
    p.send_to=[self,mask,dir,path](int fd,const std::uint8_t* data,std::uint32_t size,std::uint32_t address,std::uint16_t port)->int{
        const std::uint16_t host_port=ntohs(port);
        auto one=[&](const std::string& name){
            sockaddr_un a{};a.sun_family=AF_UNIX;if(name.size()>=sizeof a.sun_path)return;
            std::strcpy(a.sun_path,name.c_str());
            const auto sent=sendto(fd,data,size,0,reinterpret_cast<sockaddr*>(&a),sizeof a);
            if(std::getenv("OR2_NET_TRACE"))std::fprintf(stderr,"[vlan] %s: %d (errno %d) t=%ld\n",name.c_str(),int(sent),sent<0?errno:0,long(std::time(nullptr)));};
        if(address==0xffffffffu||address==((self&mask)|~mask)){
            const std::string suffix=":"+std::to_string(host_port);
            if(DIR* d=opendir(dir.c_str())){
                while(dirent* e=readdir(d)){
                    const std::string n=e->d_name;
                    if(n.size()<=suffix.size()||n.compare(n.size()-suffix.size(),suffix.size(),suffix)!=0)continue;
                    const std::string full=dir+"/"+n;
                    if(full!=path(self,host_port))one(full);
                }
                closedir(d);
            }
        }else one(path(address,host_port));
        return int(size);};
    p.receive_from=[](int fd,std::uint8_t* data,std::uint32_t size,std::uint32_t& address,std::uint16_t& port)->int{
        sockaddr_un a{};socklen_t len=sizeof a;
        const auto n=recvfrom(fd,data,size,0,reinterpret_cast<sockaddr*>(&a),&len);
        if(std::getenv("OR2_NET_TRACE")){static unsigned calls=0;
            if(n>=0||++calls%30u==1u)std::fprintf(stderr,"[vlan] recv fd %d: %d (errno %d, size %u) t=%ld\n",fd,int(n),n<0?errno:0,size,long(std::time(nullptr)));}
        if(n<0)return errno==EWOULDBLOCK||errno==EAGAIN?0:-1;
        const std::string name=a.sun_path;const auto slash=name.rfind('/'),colon=name.rfind(':');
        in_addr from{};
        if(slash!=std::string::npos&&colon!=std::string::npos&&colon>slash){
            inet_pton(AF_INET,name.substr(slash+1,colon-slash-1).c_str(),&from);
            port=htons(std::uint16_t(std::atoi(name.c_str()+colon+1)));
        }
        address=from.s_addr;
        return int(n);};
    p.interfaces=[self,mask]{return std::vector<PcNetworkInterface>{{self,(self&mask)|~mask,mask}};};
    return p;
}
#endif
PcNetworkPlatform pc_network_bsd_platform(){
#if defined(OR2_NET_VLAN)
    if(const char* spec=std::getenv("OR2_NET_VLAN");spec&&std::strchr(spec,':'))return vlan_platform(spec);
#endif
    PcNetworkPlatform p;
    p.open_udp=[](std::uint16_t port,bool broadcast)->int{
        const int fd=socket(AF_INET,SOCK_DGRAM,0);
        if(fd<0)return -1;
        const int one=1;
        (void)setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&one,sizeof one);
        if(broadcast)(void)setsockopt(fd,SOL_SOCKET,SO_BROADCAST,&one,sizeof one);
        sockaddr_in a{};a.sin_family=AF_INET;a.sin_port=htons(port);a.sin_addr.s_addr=htonl(INADDR_ANY);
        if(bind(fd,reinterpret_cast<sockaddr*>(&a),sizeof a)<0){close(fd);return -1;}
        const int flags=fcntl(fd,F_GETFL,0);
        (void)fcntl(fd,F_SETFL,(flags<0?0:flags)|O_NONBLOCK);
        return fd;};
    p.close=[](int fd){if(fd>=0)close(fd);};
    p.send_to=[](int fd,const std::uint8_t* data,std::uint32_t size,std::uint32_t address,std::uint16_t port)->int{
        sockaddr_in a{};a.sin_family=AF_INET;a.sin_port=port;a.sin_addr.s_addr=address;
        const auto n=sendto(fd,data,size,0,reinterpret_cast<sockaddr*>(&a),sizeof a);
        return n<0?-1:int(n);};
    p.receive_from=[](int fd,std::uint8_t* data,std::uint32_t size,std::uint32_t& address,std::uint16_t& port)->int{
        sockaddr_in a{};socklen_t len=sizeof a;
        const auto n=recvfrom(fd,data,size,0,reinterpret_cast<sockaddr*>(&a),&len);
        if(n<0)return errno==EWOULDBLOCK||errno==EAGAIN?0:-1;
        address=a.sin_addr.s_addr;port=a.sin_port;
        return n==0?0:int(n);};
    p.interfaces=interfaces;
    return p;
}
#else
PcNetworkPlatform pc_network_bsd_platform(){return {};}
#endif
}

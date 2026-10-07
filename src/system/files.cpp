#include "system/files.hpp"
#include <cerrno>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#else
#include <dirent.h>
#include <unistd.h>
#endif
#ifdef __PROSPERO__
#include <fcntl.h>
#endif
namespace outrun::platform {
bool make_directory(const std::string& path){
#ifdef _WIN32
    const int rc=_mkdir(path.c_str());
#else
    const int rc=mkdir(path.c_str(),0777);
#endif
    return rc==0||errno==EEXIST;
}
bool is_directory(const std::string& path){
#ifdef __PROSPERO__
    // stat kills a PS5 title on firmware 13.60 (SYSTEM_ILLEGAL_FUNCTION_CALL).
    const int fd=open(path.c_str(),O_RDONLY|O_DIRECTORY);
    if(fd<0)return false;
    close(fd);return true;
#else
    struct stat st{};
    return stat(path.c_str(),&st)==0&&(st.st_mode&S_IFMT)==S_IFDIR;
#endif
}
bool remove_directory(const std::string& path){
    if(!is_directory(path))return false;   // rmdir of a missing folder takes the Eden emulator down
#ifdef _WIN32
    return _rmdir(path.c_str())==0;
#else
    return rmdir(path.c_str())==0;
#endif
}
std::vector<std::string> directory_names(const std::string& dir,bool* opened){
    std::vector<std::string> names;bool ok=false;
#ifdef _WIN32
    _finddata_t d{};const auto h=_findfirst((dir+"/*").c_str(),&d);
    if(h!=-1){ok=true;do{names.push_back(d.name);}while(_findnext(h,&d)==0);_findclose(h);}
#else
    if(DIR* d=opendir(dir.c_str())){
        ok=true;
        while(const dirent* e=readdir(d))names.push_back(e->d_name);
        closedir(d);
    }
#ifdef __PROSPERO__
    // A PS5 title's sandbox refuses opendir on /app0; open + getdents still list it.
    else if(const int fd=open(dir.c_str(),O_RDONLY|O_DIRECTORY);fd>=0){
        ok=true;
        std::vector<char> buffer(65536);   // at least one file system block (64 KiB on the PS5)
        for(int n;(n=getdents(fd,buffer.data(),int(buffer.size())))>0;)
            for(int offset=0;offset<n;){
                const auto* e=reinterpret_cast<const dirent*>(buffer.data()+offset);
                if(e->d_reclen==0)break;
                names.push_back(e->d_name);
                offset+=e->d_reclen;
            }
        close(fd);
    }
#endif
#endif
    if(opened)*opened=ok;
    return names;
}
}

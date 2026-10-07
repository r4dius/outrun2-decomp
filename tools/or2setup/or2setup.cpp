// or2setup: prepares the OR2006C2C.EXE image cache ahead of time, on a PC or
// a Mac, so the console or the Mac app starts without its first-launch step.
//
//   or2setup <game folder> [<output folder>]
//
// The game folder holds your own OutRun 2006 Coast 2 Coast installation with
// a readable copy of OR2006C2C.EXE (any name ending in .exe). The image is
// written as <game folder>/OR2006C2C.cache (or in the output folder); copy
// the whole game folder (with it) to the device afterwards.
#include "system/exe_image.hpp"
#include "system/files.hpp"
#include <cstdio>
#include <string>
#include <vector>
using namespace outrun::platform;
namespace {
void bar(const char* label,std::uint64_t done,std::uint64_t total){
    const int width=40,filled=total?int(width*done/total):width;
    std::printf("\r  %-24s [%s%s] %3d%%",label,std::string(std::size_t(filled),'#').c_str(),std::string(std::size_t(width-filled),'.').c_str(),
                total?int(100*done/total):100);
    std::fflush(stdout);
}
}
int main(int argc,char** argv){
    if(argc<2||argc>3){
        std::fprintf(stderr,"usage: or2setup <game folder> [<output folder>]\n");
        return 2;
    }
    const std::string game=argv[1],cache_dir=argc>2?argv[2]:game,cache=cache_dir+"/"+ExeImageCacheName;
    std::printf("OutRun 2006 Coast 2 Coast - preparing %s\n\n",game.c_str());
    std::vector<std::uint8_t> image;std::string path,error;ExeSource source=ExeSource::Missing;bool checking=false;
    std::printf("  Finding your OR2006C2C.EXE...\n");
    const bool found=exe_image_find_source(game,image,path,&source,&error,[&](std::uint64_t d,std::uint64_t t){checking=true;bar("Checking the game data",d,t);});
    if(checking)std::printf("\n");
    if(!found){
        const char* title="OR2006C2C.EXE not found";
        const char* sentence="This file is required from your own OutRun 2006 Coast 2 Coast installation (Steam version).";
        if(source==ExeSource::Compressed){
            title="OR2006C2C.EXE cannot be used yet";
            sentence="This file was found in your own game installation, but this build cannot read it yet.";
        }else if(source==ExeSource::OtherVersion)title="OR2006C2C.EXE is another version";
        std::printf("\n  %s\n  %s\n  Folder: %s\n",title,sentence,game.c_str());
        return 1;
    }
    std::printf("  Using %s\n",path.c_str());
    if(!make_directory(cache_dir)||!exe_image_write_cache(cache,image,&error)){
        std::printf("\n  Cannot save the prepared data\n  This folder must be writable, with 10 MB free.\n  Folder: %s\n",cache_dir.c_str());
        return 1;
    }
    std::printf("  Saved %s\n\nDone: copy the game folder to your device and start the game.\n",cache.c_str());
    return 0;
}

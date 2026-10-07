#include "system/setup_screen.hpp"
#include "system/exe_image.hpp"
#include "system/files.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <thread>
namespace outrun::platform {
namespace {
#include "system/setup_font.inc"
constexpr std::uint32_t rgb(int r,int g,int b){return std::uint32_t(r)|std::uint32_t(g)<<8|std::uint32_t(b)<<16|0xff000000u;}
std::uint32_t mix(std::uint32_t a,std::uint32_t b,double t){
    t=std::clamp(t,0.0,1.0);
    const auto c=[&](int s){return int(std::lround(double((a>>s)&255u)*(1.0-t)+double((b>>s)&255u)*t))<<s;};
    return std::uint32_t(c(0)|c(8)|c(16))|0xff000000u;
}
std::uint32_t blend(std::uint32_t dst,std::uint32_t src,int alpha){return mix(dst,src,alpha/255.0);}
const std::uint32_t White=rgb(235,235,235),Grey=rgb(140,140,140),Red=rgb(255,90,90);
std::vector<std::string> wrap(const std::string& s,std::size_t columns){
    std::vector<std::string> out;std::string line,word;
    const auto flush_word=[&]{
        if(word.empty())return;
        if(!line.empty()&&line.size()+1+word.size()>columns){out.push_back(line);line.clear();}
        while(word.size()>columns){out.push_back(word.substr(0,columns));word.erase(0,columns);}
        if(!line.empty())line+=' ';
        line+=word;word.clear();};
    for(char c:s){if(c==' ')flush_word();else word+=c;}
    flush_word();if(!line.empty()||out.empty())out.push_back(line);
    return out;
}
}

SetupScreen::SetupScreen(int width,int height):width_(width),height_(height),pixels_(std::size_t(width)*height){}
void SetupScreen::set_steps(const std::vector<std::string>& labels){steps_.clear();for(const auto& l:labels)steps_.push_back({l,State::Pending});}
void SetupScreen::set_step(std::size_t index,State state){if(index<steps_.size())steps_[index].state=state;}
void SetupScreen::set_progress(double fraction){progress_=fraction;}
void SetupScreen::set_detail(const std::string& text){detail_=text;}
void SetupScreen::set_footer(const std::string& text){footer_=text;}
void SetupScreen::set_message(const std::string& text){message_=text;}
void SetupScreen::set_error(const std::string& title,const std::vector<std::string>& lines){error_title_=title;error_lines_=lines;}

void SetupScreen::fill(int x,int y,int w,int h,std::uint32_t colour,int alpha){
    const int x0=std::max(0,x),y0=std::max(0,y),x1=std::min(width_,x+w),y1=std::min(height_,y+h);
    for(int j=y0;j<y1;++j)for(int i=x0;i<x1;++i){auto& p=pixels_[std::size_t(j)*width_+i];p=alpha>=255?colour:blend(p,colour,alpha);}
}
int SetupScreen::text_width(const std::string& s,int scale) const{return int(s.size())*6*scale-scale;}
void SetupScreen::text(int x,int y,const std::string& s,int scale,std::uint32_t colour,bool shadow){
    if(shadow)text(x+std::max(1,scale/2),y+std::max(1,scale/2),s,scale,rgb(0,0,0),false);
    for(std::size_t k=0;k<s.size();++k){
        const unsigned char c=static_cast<unsigned char>(s[k]);
        const auto* g=SetupFont[(c>=32&&c<127)?c-32:'?'-32];
        const int drop=(c=='g'||c=='p'||c=='q'||c=='y')?2:0;
        for(int row=0;row<7;++row)for(int col=0;col<5;++col)
            if(g[row]&(0x10>>col))fill(x+int(k)*6*scale+col*scale,y+(row+drop)*scale,scale,scale,colour,255);
    }
}

// Plain text on black: what is being done and how far it has got.
void SetupScreen::render(double seconds){
    (void)seconds;
    std::fill(pixels_.begin(),pixels_.end(),rgb(0,0,0));
    const int s=std::max(2,height_/240),line=s*12,left=width_/10;
    const std::size_t columns=std::size_t((width_-2*left)/(6*s));
    int y=height_/8;
    if(!error_title_.empty()){
        for(const auto& w:wrap(error_title_,columns)){text(left,y,w,s,Red,false);y+=line;}
        y+=line/2;
        for(const auto& l:error_lines_)for(const auto& w:wrap(l,columns)){text(left,y,w,s,White,false);y+=line;}
        return;
    }
    text(left,y,"OutRun 2006 Coast 2 Coast - first launch",s,White,false);y+=line*2;
    for(const auto& st:steps_){
        const char* mark=st.state==State::Done?"[done] ":st.state==State::Running?"[....] ":st.state==State::Failed?"[fail] ":"[    ] ";
        text(left,y,mark+st.label,s,st.state==State::Pending?Grey:st.state==State::Failed?Red:White,false);
        y+=line;
    }
    if(!message_.empty()){y+=line;text(left,y,message_,s,White,false);y+=line;}   // after one blank line ("Enjoy!")
    if(progress_>=0.0){
        char pct[16];std::snprintf(pct,sizeof pct,"%d%%",int(std::lround(std::clamp(progress_,0.0,1.0)*100)));
        y+=line/2;text(left,y,detail_.empty()?std::string(pct):detail_+"  "+pct,s,White,false);
    }
    if(!footer_.empty()){
        y+=line*2;
        for(const auto& w:wrap(footer_,columns)){text(left,y,w,s,Grey,false);y+=line;}
    }
}

namespace {
std::string without_slash(const std::string& dir){return (!dir.empty()&&(dir.back()=='/'||dir.back()=='\\'))?dir.substr(0,dir.size()-1):dir;}
bool file_exists(const std::string& path){std::FILE* f=std::fopen(path.c_str(),"rb");if(!f)return false;std::fclose(f);return true;}
// Earlier builds kept the image (exe_image.bin, then OR2006C2C.cache) and
// converted data in a folder of dir: the image moves to cache (or fallback),
// the rest is deleted with the folder once it is empty.
void adopt_former_cache(const std::string& dir,const std::string& cache,const std::string& fallback){
    for(const char* folder:{"/Cache","/.outrun-nx-cache","/.outrun-cache"}){
        const std::string old=dir+folder;
        if(!is_directory(old))continue;
        for(const char* name:{ExeImageCacheName,"exe_image.bin"}){
            const std::string from=old+"/"+name;
            if(!file_exists(from))continue;
            if(!file_exists(cache)&&std::rename(from.c_str(),cache.c_str())==0)continue;
            if(!fallback.empty()&&!file_exists(fallback)&&std::rename(from.c_str(),fallback.c_str())==0)continue;
            if(file_exists(cache)||(!fallback.empty()&&file_exists(fallback)))std::remove(from.c_str());
        }
        for(const char* name:{"start_loading_en.ldp2","sumo_fe.fep6","game_ui.gui1","shared_ui.gui1","fonts.fnt1","menu_images.xst",
                              "beac_scene.msh3","250gto.msh3","meter_250gto_0.dds","meter_250gto_1.dds"})
            std::remove((old+"/"+name).c_str());
        (void)remove_directory(old);
    }
}
}

bool exe_setup_prepare(const std::string& game_dir,const std::string& fallback_dir,const SetupPlatform& platform,std::string* error){
    const std::string root=without_slash(game_dir),cache=root+"/"+ExeImageCacheName;
    const std::string spare=without_slash(fallback_dir),fallback=spare.empty()?std::string():spare+"/"+ExeImageCacheName;
    adopt_former_cache(root,cache,fallback);
    if(!spare.empty()&&spare!=root)adopt_former_cache(spare,fallback,{});
    std::string e;
    if(exe_image_loaded())return true;   // development: OR2_EXE
    // Next to the game files, possibly prepared ahead of time (tools/or2setup,
    // tools/ps5_package.py) on read-only storage such as a package.
    if(exe_image_load_cache(cache,&e))return true;
    if(!fallback.empty()&&exe_image_load_cache(fallback,&e))return true;
    using Clock=std::chrono::steady_clock;
    const auto start=Clock::now();auto last=start-std::chrono::seconds(1);
    SetupScreen screen;
    screen.set_steps({"Find your OR2006C2C.EXE","Check the game data","Save the prepared data"});
    const auto show=[&](bool force){
        const auto now=Clock::now();
        if(!force&&now-last<std::chrono::milliseconds(50))return;
        last=now;
        screen.render(std::chrono::duration<double>(now-start).count());
        if(platform.present)platform.present(screen);
    };
    const auto fail=[&](const std::string& title,const std::vector<std::string>& lines,const std::string& why){
        auto all=lines;if(!platform.exit_hint.empty()){all.push_back("");all.push_back(platform.exit_hint);}
        screen.set_error(title,all);show(true);
        if(platform.wait_for_exit)platform.wait_for_exit();
        if(error)*error=why;return false;
    };
    screen.set_step(0,SetupScreen::State::Running);show(true);
    std::vector<std::uint8_t> image;std::string path;ExeSource found=ExeSource::Missing;
    bool checking=false;
    const auto progress=[&](std::uint64_t done,std::uint64_t total){
        if(!checking){checking=true;screen.set_step(0,SetupScreen::State::Done);screen.set_step(1,SetupScreen::State::Running);}
        screen.set_progress(total?double(done)/double(total):0.0);show(false);
    };
    if(!exe_image_find_source(game_dir,image,path,&found,&e,progress)){
        screen.set_step(checking?1:0,SetupScreen::State::Failed);screen.set_progress(-1.0);
        // One family of messages: what is wrong, then what this file must be.
        switch(found){
            case ExeSource::Compressed:
                return fail("OR2006C2C.EXE cannot be used yet",
                    {"This file was found in your own game installation, but this build cannot read it yet.",
                     "Folder: "+game_dir},e);
            case ExeSource::OtherVersion:
                return fail("OR2006C2C.EXE is another version",
                    {"This file is required from your own OutRun 2006 Coast 2 Coast installation (Steam version).",
                     "Folder: "+game_dir},e);
            default:
                return fail("OR2006C2C.EXE not found",
                    {"This file is required from your own OutRun 2006 Coast 2 Coast installation (Steam version).",
                     "Folder: "+game_dir},e);
        }
    }
    screen.set_step(0,SetupScreen::State::Done);screen.set_step(1,SetupScreen::State::Done);
    screen.set_step(2,SetupScreen::State::Running);screen.set_progress(-1.0);
    screen.set_detail("");show(true);
    // The game folder first; the platform's own folder when the game folder is read-only.
    if(!exe_image_write_cache(cache,image,&e)&&
       (fallback.empty()||!make_directory(spare)||!exe_image_write_cache(fallback,image,&e))){
        screen.set_step(2,SetupScreen::State::Failed);
        return fail("Cannot save the prepared data",{"This folder must be writable, with 10 MB free.","Folder: "+root},e);
    }
    exe_image_install(std::move(image));
    screen.set_step(2,SetupScreen::State::Done);
    screen.set_progress(-1.0);screen.set_message("Enjoy!");
    const auto until=Clock::now()+std::chrono::seconds(1);
    while(Clock::now()<until&&(!platform.keep_running||platform.keep_running())){
        show(true);std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
    return true;
}
}

#include "platform/frontend_text.hpp"
#include "driving/pc_x87.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

namespace outrun::platform {
namespace {
using driving::Bytes;
using driving::X87;
bool fail(std::string* e,const char* s){if(e)*e=s;return false;}
std::uint32_t u32(const std::uint8_t* p){return unsigned(p[0])|(unsigned(p[1])<<8)|(unsigned(p[2])<<16)|(unsigned(p[3])<<24);}
std::uint32_t crc(const std::uint8_t* p,std::size_t n){
    unsigned c=~0u;for(std::size_t i=0;i<n;++i){c^=p[i];for(unsigned k=0;k<8;++k)c=(c>>1)^(0xedb88320u&(0u-(c&1)));}return ~c;
}
bool read(const char* path,std::vector<std::uint8_t>& b,std::string* e){
    auto* f=std::fopen(path,"rb");if(!f)return fail(e,"text/font file unavailable");
    const bool seek=std::fseek(f,0,SEEK_END)==0;const auto n=seek?std::ftell(f):-1;
    if(n<0||n>8*1024*1024||std::fseek(f,0,SEEK_SET)){std::fclose(f);return fail(e,"text/font file size invalid");}
    b.resize(std::size_t(n));const bool ok=std::fread(b.data(),1,b.size(),f)==b.size();std::fclose(f);
    return ok||fail(e,"text/font short read");
}
// x87/SSE truncate-to-int behavior; avoid undefined host casts on malformed data.
int trunc(float f){return std::isfinite(f)&&double(f)>=-2147483648.&&double(f)<2147483648.?int(f):std::numeric_limits<int>::min();}
// _ftol2 on an x87 register value (every fadd/fiadd/fmul before it rounded to
// the current precision control).
int trunc(X87 f){return driving::x87_ftol32(f);}
std::int16_t wrap16(int v){const auto u=std::uint16_t(v);std::int16_t r;std::memcpy(&r,&u,2);return r;}
int pair_width(const FrontendFont& f,const FrontendTextStyle& s,unsigned c,unsigned next){
    if(c<f.first||c-f.first>=f.metrics.size())throw std::out_of_range("font character outside original metrics");
    const auto index=c-f.first;const int width=f.metrics[index][1];
    if(!f.kerning.empty()&&next){
        // The PC indexes the contiguous pair table, including a next newline
        // below first. Such a pair reads the preceding row, not a fallback.
        const auto pair=std::int64_t(index)*std::int64_t(f.metrics.size())+next-f.first;
        if(pair<0||std::size_t(pair)>=f.kerning.size())throw std::out_of_range("font pair outside original kerning");
        return trunc(X87(width-f.kerning[std::size_t(pair)])*X87(s.scale_x));          // 42C443
    }
    return trunc((X87(width)+X87(f.spacing_x))*X87(s.scale_x));                         // 42C463
}
std::string_view terminated(std::string_view v){return v.substr(0,v.find('\0'));}
FrontendTextStyle style(Bytes b){FrontendTextStyle s;s.scale_x=b.f32(0x478);s.scale_y=b.f32(0x47c);
    s.color=b.u32(0x474);s.mode=b.u32(0x454);s.flags=b.u32(0x480);return s;}
}
bool parse_frontend_font_pack(const std::uint8_t* p,std::size_t n,FrontendFontPack& out,std::string* e){
    if(!p||n<512||n>8*1024*1024||std::memcmp(p,"OR2FNT1\0",8)||u32(p+8)!=1||u32(p+12)!=10||
       u32(p+16)!=48||u32(p+20)!=512||u32(p+24)!=n||crc(p+32,n-32)!=u32(p+28))return fail(e,"font header/checksum invalid");
    FrontendFontPack pack;std::array<bool,10> seen{};std::size_t cursor=512;
    for(unsigned i=0;i<10;++i){
        Bytes b(const_cast<std::uint8_t*>(p+32+48*i),48);const auto id=b.u32(0),count=b.u32(16),kern=b.u32(32),w=b.u32(36),h=b.u32(40),fmt=b.u32(44);
        if(id>=10||seen[id]||b.u32(4)==0||b.u32(4)>512||b.u32(8)==0||b.u32(8)>512||
           count==0||count>256||b.u32(12)>256-count||(kern!=0&&kern!=count*count)||
           w==0||h==0||w>2048||h>2048||(fmt!=2&&fmt!=3)||512ull+b.u32(28)!=cursor)return fail(e,"font record invalid");
        const std::size_t pixels=std::size_t((w+3)/4)*((h+3)/4)*16,bytes=count*2+kern+pixels;
        if(bytes>n-cursor)return fail(e,"font payload truncated");
        seen[id]=true;
        auto& f=pack.fonts[id];f.token=id;f.width=std::int16_t(b.u32(4));f.height=std::int16_t(b.u32(8));f.first=b.u32(12);
        f.spacing_x=float(b.i32(20));f.spacing_y=float(b.i32(24));f.metrics.resize(count);f.kerning.resize(kern);
        std::memcpy(f.metrics.data(),p+cursor,count*2);cursor+=count*2;
        if(kern)std::memcpy(f.kerning.data(),p+cursor,kern);
        cursor+=kern;
        auto& t=pack.textures[id];t.width=w;t.height=h;t.format=MeshPreviewTextureFormat(fmt);
        t.bytes.assign(p+cursor,p+cursor+pixels);cursor+=pixels;
    }
    if(cursor!=n)return fail(e,"font trailing bytes");
    out=std::move(pack);return true;
}
bool load_frontend_font_pack(const char* p,FrontendFontPack& f,std::string* e){std::vector<std::uint8_t>b;return read(p,b,e)&&parse_frontend_font_pack(b.data(),b.size(),f,e);}
bool FrontendTextTable::parse(const std::uint8_t* p,std::size_t n,std::string* e){
    if(!p||n<16||n>8*1024*1024||u32(p)!=0x74657874||u32(p+4)!=n)return fail(e,"text header invalid");
    const auto first=u32(p+8);if(first<16||first>n||first%4)return fail(e,"text offset table invalid");
    std::vector<std::string> strings;bool end=false;
    for(std::size_t i=8;i<first;i+=4){
        auto off=u32(p+i);if(!off){end=true;break;}
        if(off<first||off>=n||off%2)return fail(e,"text string outside payload");
        std::string s;bool nul=false;
        for(;off+1<n;off+=2){if(p[off]==0&&p[off+1]==0){nul=true;break;}s.push_back(char(p[off]));}
        if(!nul)return fail(e,"text unterminated string");
        strings.push_back(std::move(s));
    }
    if(!end)return fail(e,"text table missing sentinel");
    strings_=std::move(strings);return true;
}
bool FrontendTextTable::load(const char* p,std::string* e){std::vector<std::uint8_t>b;return read(p,b,e)&&parse(b.data(),b.size(),e);}
const std::string* FrontendTextTable::get(unsigned i)const{return i<strings_.size()?&strings_[i]:nullptr;}
int frontend_text_width_42c480(const FrontendFont& f,const FrontendTextStyle& s,std::string_view text,float space){
    text=terminated(text);int width=0;const float cell=driving::x87_float(X87(int(f.width))*X87(s.scale_x));
    for(std::size_t i=0;i<text.size();++i){const unsigned c=std::uint8_t(text[i]),next=i+1<text.size()?std::uint8_t(text[i+1]):0;
        if(c==10)continue;
        if(c==32){width+=trunc(!f.metrics.empty()&&(s.flags&0x100)?X87(cell)*X87(space):X87(cell));continue;}
        if(!f.metrics.empty()&&(s.flags&0x100)){
            const auto advance=next==32?pair_width(f,s,c,0):pair_width(f,s,c,next);
            width=trunc(X87(advance)+X87(width)+X87(f.spacing_x));                          // 42C557 fild; fiadd; fadd
        }else width+=trunc(X87(f.spacing_x)+X87(cell));
    }return width;
}
void frontend_text_advance_42c610(const FrontendFont& f,const FrontendTextStyle& s,FrontendTextCursor& cur,std::uint8_t c,std::uint8_t next){
    if(c==10){cur.x=cur.origin_x;cur.y=wrap16(cur.y+f.height+trunc(f.spacing_y));return;}
    const float cell=driving::x87_float(X87(int(f.width))*X87(s.scale_x));
    if(c==32){cur.x=wrap16(cur.x+trunc(X87(cell)*X87(0.3f)));return;}
    if(!f.metrics.empty())cur.x=wrap16(trunc(X87(pair_width(f,s,c,next==32?0:next))+X87(int(cur.x))+X87(f.spacing_x)));
    else cur.x=wrap16(cur.x+trunc(X87(f.spacing_x)+X87(cell)));
}
bool frontend_text_glyph_42c860(const FrontendFont& f,const FrontendTextStyle& s,const FrontendTextCursor& cur,std::uint8_t c,FrontendGlyph& g){
    if(c==10||c==32)return false;
    const int index=int(c)-int(f.first);
    if(index<0||(!f.metrics.empty()&&std::size_t(index)>=f.metrics.size()))throw std::out_of_range("font glyph outside original metrics");
    const int bearing=f.metrics.empty()?0:trunc(X87(-int(f.metrics[index][0]))*X87(s.scale_x));
    g={};g.token=f.token;g.left=(index&15)*f.width;g.right=g.left+f.width;g.top=((index>>4)&15)*f.height;g.bottom=g.top+f.height;
    g.scale_x=s.scale_x;g.scale_y=s.scale_y;g.x=float(cur.x+bearing);g.y=float(cur.y);g.color=s.color;g.mode=s.mode;
    // Preserve PC clipping of the unscaled source-width span (42C860), including
    // its unusual scaled-font behavior; don't substitute a new clip algorithm.
    const int right=trunc(X87(int(f.width))+X87(g.x));
    if(g.x<s.clip_left){if(right<=s.clip_left)return false;g.left+=trunc(X87(s.clip_left)-X87(g.x));g.x=float(s.clip_left);}
    if(right>s.clip_right){if(g.x>=s.clip_right)return false;g.right+=s.clip_right-right;}
    return true;
}
bool frontend_text_glyph_42c720(const FrontendFont& f,const FrontendTextStyle& s,const FrontendTextCursor& cur,std::uint8_t c,FrontendGlyph& g){
    if(std::int8_t(c)<0||c==10||c==9||c==32)return false;
    const int index=int(c)-int(f.first);                                    // movsx al - 956BD4
    g={};g.token=f.token;                                                   // 42A070 record, then the cell
    g.left=(index&15)*f.width;g.top=((index>>4)&15)*f.height;g.right=f.width+g.left;g.bottom=f.height+g.top;
    g.scale_x=s.scale_x;g.scale_y=s.scale_y;g.x=float(cur.x);g.y=float(cur.y);g.color=s.color;g.mode=s.mode;
    return true;
}
void frontend_text_advance_42c5a0(const FrontendFont& f,const FrontendTextStyle& s,FrontendTextCursor& cur,std::uint8_t c){
    const X87 cell=X87(int(f.width))*X87(s.scale_x);                         // fild 956BBC; fmul 956BC4
    if(c==10){cur.x=cur.origin_x;cur.y=wrap16(cur.y+f.height+trunc(f.spacing_y));return;}
    if(c==32){cur.x=wrap16(cur.x+trunc(cell));return;}
    cur.x=wrap16(cur.x+trunc(X87(f.spacing_x)+cell));                       // fld 956BDC; fadd st,st(1)
}
bool frontend_text_draw(const FrontendFont& f,FrontendTextStyle s,FrontendTextCursor cur,std::string_view text,std::vector<FrontendGlyph>& out){
    text=terminated(text);s.flags|=0x100;
    try{
        if(!(s.flags&1)){const int width=frontend_text_width_42c480(f,s,text);cur.x=wrap16(cur.x-((s.flags&4)?width/2:width));}
        if(s.flags&8)cur.y=wrap16(cur.y+trunc(X87(int(f.height))*X87(-0.5f)));
        else if(s.flags&16)cur.y=wrap16(cur.y-f.height);
        std::vector<FrontendGlyph> draws;
        for(std::size_t i=0;i<text.size();++i){FrontendGlyph g;const auto c=std::uint8_t(text[i]);
            if(frontend_text_glyph_42c860(f,s,cur,c,g))draws.push_back(g);
            frontend_text_advance_42c610(f,s,cur,c,i+1<text.size()?std::uint8_t(text[i+1]):0);
        }out.insert(out.end(),draws.begin(),draws.end());return true;
    }catch(const std::out_of_range&){return false;}
}
void frontend_text_init_48e640(Bytes b){
    b.check(0,PcTextWidgetBytes);b.put8(0x4e,0);b.put32(0x450,9);b.put32(0x458,0);b.put8(0x470,1);
    b.put32(0x45c,0x400);b.put32(0x474,0xffdddddd);b.put32(0x454,13);b.putf(0x478,1);b.putf(0x47c,1);
    for(auto o:{0x460,0x464,0x468,0x46c,0x488})b.put32(o,0);
    b.put32(0x480,1);b.put8(0x4c,1);b.put8(0x4d,0);b.put32(0x484,14);
}
bool frontend_text_set_48f280(Bytes b,std::string_view text,unsigned font,unsigned color){
    b.check(0,PcTextWidgetBytes);text=terminated(text);if(text.size()>=0x400)return false;
    for(std::size_t i=0;i<text.size();++i)b.put8(0x4e + i,std::uint8_t(text[i]));
    b.put8(0x4e + text.size(),0);
    b.put32(0x458,unsigned(text.size()));b.put32(0x450,font);b.put32(0x474,color);return true;
}
std::string frontend_text_value_48eec0(Bytes b){b.check(0,PcTextWidgetBytes);std::string s;
    for(unsigned i=0;i<0x400;++i){if(!b.u8(0x4e + i))return s;s+=char(b.u8(0x4e + i));}throw std::out_of_range("unterminated text widget");}
bool frontend_text_setter(unsigned pc,Bytes b,const unsigned* a,std::size_t n){
    b.check(0,PcTextWidgetBytes);if(n&&!a)return false;
    switch(pc){
    case 0x48e640:if(n)return false;frontend_text_init_48e640(b);return true;
    case 0x48ee60:if(n)return false;b.put8(0x4c,0);return true;
    case 0x48e530:case 0x48ef50:if(n!=2)return false;
        b.put32(pc==0x48e530?0x34:0x478,a[0]);b.put32(pc==0x48e530?0x38:0x47c,a[1]);return true;
    case 0x48eed0:case 0x48ef30:case 0x48ef40:if(n!=1)return false;
        b.put32(pc==0x48eed0?0x45c:pc==0x48ef30?0x474:0x454,a[0]);return true;
    case 0x48eee0:if(n!=4)return false;
        for(unsigned i=0;i<4;++i)b.put32(0x460+4*i,a[i]);
        if(b.i32(0x468)>b.i32(0x460)&&b.i32(0x46c)>b.i32(0x464))b.put8(0x4d,1);
        return true;
    default:return false;
    }
}
bool frontend_text_split_48ea50(Bytes b,std::string_view text,FrontendTextLines& out){
    text=terminated(text);const int limit=std::min(b.i32(0x45c),128);if(limit<=1)return false;b.puti(0x45c,limit);
    std::size_t pos=0;for(auto& line:out.lines){line.fill(0);if(pos>=text.size())continue;
        std::size_t count=std::size_t(limit-1),skip=0;const auto rest=text.substr(pos);const auto newline=rest.find("<newline>");
        if(newline!=std::string_view::npos&&newline<31){count=newline;skip=9;}
        else if(rest.size()>=count){auto at=count;while(at>0&&rest[at]!=' ')--at;if(at>0){count=at;skip=1;}}
        std::memcpy(line.data(),rest.data(),std::min(count,rest.size()));pos+=count+skip;
    }return true;
}
bool frontend_text_wrap_48eb60(Bytes b,const FrontendFont& f,FrontendTextLines& out){
    const auto text=frontend_text_value_48eec0(b);const auto width=std::int64_t(b.i32(0x468))-b.i32(0x460);
    auto s=style(b);s.flags=0x100;int line=-b.i32(0x488),committed=0,used_width=0;std::string word;
    if(line< -1024||line>=16||width<=0)return false;
    auto write=[&](int offset,std::string_view data){if(line>=0&&line<16&&offset>=0&&std::size_t(offset)+data.size()<=128)
        std::memcpy(out.lines[std::size_t(line)].data()+offset,data.data(),data.size());};
    auto end=[&](){write(committed,std::string_view("\0",1));++line;committed=used_width=0;};
    try{for(std::size_t i=0;i<=text.size()&&line<16;++i){const char c=i<text.size()?text[i]:0;
        if(c!=' '&&c!='-'&&c!='\n'&&c!=0&&committed+int(word.size())!=127){
            word+=c;
            if(used_width+frontend_text_width_42c480(f,s,word)<=width)continue;
            if(committed==0){word.pop_back();write(0,word);committed=int(word.size());end();word.clear();--i;}
            else end();
        }else{
            write(committed,word);committed+=int(word.size());
            if(c=='\n'||!c||committed==127){end();word.clear();}
            else {word+=c;used_width+=frontend_text_width_42c480(f,s,word);write(committed,std::string_view(&c,1));++committed;word.clear();}
        }
    }}catch(const std::out_of_range&){return false;}
    for(int i=std::max(line,0);i<16;++i)out.lines[std::size_t(i)][0]=0;
    return true;
}
bool frontend_text_height_48ef90(Bytes b,const FrontendFontPack& pack,FrontendTextLines& lines,int& result){
    b.check(0,PcTextWidgetBytes);result=b.i32(0x484);
    if(b.u8(0x4d)){
        const auto id=b.u32(0x450);if(id>=pack.fonts.size()||!frontend_text_wrap_48eb60(b,pack.fonts[id],lines))return false;
    }else if(b.i32(0x458)>0&&(b.i32(0x45c)==0||b.i32(0x458)<b.i32(0x45c)))return true;
    for(unsigned i=0;i<16;++i)if(lines.lines[i][0])result=static_cast<int>(unsigned(i+1)*b.u32(0x484));
    return true;
}
bool frontend_text_box_48f1b0(Bytes b,std::string_view text,int limit,const std::array<int,4>& rect,
    std::uint8_t enabled,unsigned font,unsigned layer,unsigned color,float sx,float sy){
    if(!std::isfinite(sx)||!std::isfinite(sy)||!frontend_text_set_48f280(b,text,font,color))return false;
    b.puti(0x45c,limit);for(unsigned i=0;i<4;++i)b.puti(0x460+4*i,rect[i]);
    if(rect[2]>rect[0]&&rect[3]>rect[1])b.put8(0x4d,1);
    b.put8(0x470,enabled);b.put32(0x454,layer);b.putf(0x478,sx);b.putf(0x47c,sy);return true;
}
bool frontend_text_display_48f3c0(Bytes b,const FrontendFontPack& pack,FrontendTextLines& lines,std::vector<FrontendGlyph>& out){
    if(!b.u8(0x4c))return true;
    const auto id=b.u32(0x450);if(id>=pack.fonts.size())return false;
    const auto& font=pack.fonts[id];if(font.width<=0||font.height<=0)return false;auto s=style(b);
    const auto text=frontend_text_value_48eec0(b);const int length=b.i32(0x458),max=b.i32(0x45c);
    const int x=trunc(b.f32(0x34)),y=trunc(b.f32(0x38));
    if(!b.u8(0x4d)&&length>0&&(max==0||length<max))return frontend_text_draw(font,s,{wrap16(x),wrap16(x),wrap16(y)},text,out);
    bool wrapped=b.u8(0x4d)!=0;
    if(wrapped){if(!frontend_text_wrap_48eb60(b,font,lines))return false;}
    else {if(length<=max||max<=1)return true;if(!frontend_text_split_48ea50(b,text,lines))return false;}
    int vertical=0;if(wrapped&&(s.flags&0x3000)){for(unsigned i=0;i<16;++i)if(lines.lines[i][0])vertical=int(i+1)*b.i32(0x484);if(s.flags&0x1000)vertical/=2;}
    for(unsigned i=0;i<16;++i){const auto& l=lines.lines[i];const auto end=std::find(l.begin(),l.end(),'\0');
        if(end==l.end())return false;
        if(end==l.begin())continue;
        if(!frontend_text_draw(font,s,{wrap16(x),wrap16(x),wrap16(y+int(i)*b.i32(0x484)-vertical)},std::string_view(l.data(),std::size_t(end-l.begin())),out))return false;
        if(!wrapped)s.flags=1; // 42CDD0 resets flags after each line.
    }return true;
}
}

#include "platform/rob_disp.hpp"
#include "platform/rob_motion_tables.hpp"
#include "driving/pc_matrix_stack.hpp"
#include <array>
#include <cstring>
#include <stdexcept>
namespace outrun::platform {
namespace {
constexpr std::uint32_t None=0xffffffffu;
std::uint32_t call(PcRaceContext& c,std::uint32_t pc,std::initializer_list<std::uint32_t> args){
    PcRaceCall k{};k.pc=pc;k.argc=std::uint32_t(args.size());
    unsigned i=0;for(auto a:args)k.args[i++]=a;
    if(!c.service)throw std::logic_error("rob disp: no service for PC call");
    return c.service(k);
}
std::uint32_t rdata32(std::uint32_t a){
    if(a<RobDispRdataBase||a+4u>RobDispRdataBase+sizeof RobDispRdata)throw std::out_of_range("rob disp: .rdata outside 5E62F0..5E6B40");
    const auto* p=RobDispRdata+(a-RobDispRdataBase);
    return std::uint32_t(p[0])|std::uint32_t(p[1])<<8|std::uint32_t(p[2])<<16|std::uint32_t(p[3])<<24;
}
std::string rdata_string(std::uint32_t a){
    std::string s;
    for(;;++a){
        if(a<RobDispRdataBase||a>=RobDispRdataBase+sizeof RobDispRdata)throw std::out_of_range("rob disp: name outside 5E62F0..5E6B40");
        const char ch=char(RobDispRdata[a-RobDispRdataBase]);if(!ch)return s;s.push_back(ch);
    }
}
// Face / eye / vector copy from a face table (5E62F0 / 5E6330 / 5E6370 / 5E63B0).
void face_from_table(PcRaceContext& c,std::uint32_t disp,std::uint32_t t){
    auto& m=c.m;
    m.put32(disp+0xc,m.u32(t));
    const auto face=call(c,0x448cd0u,{m.u32(t+4)});
    m.put32(disp+0x14,face);m.put32(disp+0x10,face);
    m.put32(disp+0x18,call(c,0x448cd0u,{m.u32(t+8)}));
    m.put32(disp+0x1c,call(c,0x448cd0u,{m.u32(t+0xc)}));
    for(std::uint32_t k=0;k<0x30u;k+=4)m.put32(disp+0x20+k,m.u32(t+0x10+k));
}
}
void rob_disp_map_rdata(PcRaceMemory& m){
    m.map_const(RobDispRdataBase,RobDispRdata,sizeof RobDispRdata);
    m.map_const(RobChrPathsBase,RobChrPaths,sizeof RobChrPaths);
    m.map_const(RobOsageDataBase,RobOsageData,sizeof RobOsageData);
}
void rob_chr_relocate_487e10(PcRaceMemory& m,std::uint32_t d,std::uint32_t resource){
    for(std::uint32_t f=8;f<=0x18u;f+=4)if(const auto v=m.u32(d+f))m.put32(d+f,v+d);
    if(const auto v=m.u32(d+0x1c)){
        const auto x=v+d;m.put32(d+0x1c,x);
        m.put32(x+8,m.u32(x+8)+d);m.put32(x+0xc,m.u32(x+0xc)+d);m.put32(x+0x10,m.u32(x+0x10)+d);
    }
    for(std::int32_t i=0;i<std::int32_t(std::int16_t(m.u16(d+2)));++i){
        const auto a=m.u32(d+8)+std::uint32_t(i)*8u+4u;m.put32(a,(m.u32(a)&0xffffu)|(resource<<16));}
    for(std::int32_t i=0;i<std::int32_t(std::int16_t(m.u16(d+4)));++i){
        const auto a=m.u32(d+0xc)+std::uint32_t(i)*0x14u+0x10u;m.put32(a,(m.u32(a)&0xffffu)|(resource<<16));}
}
void rob_chr_load_488b80(PcRaceMemory& m,const PcRaceService& s){
    if(m.u32(0x654860u)==0u)return;
    for(std::uint32_t rec=0x654860u,i=0;;rec+=0x10u,i+=4u){
        if(rob_motion_load_file_4f1f90(m,s,m.u32(rec),0x82f488u+i)){
            const auto data=m.u32(m.u32(0x82f488u+i));
            m.put32(0x82f388u+i,data);
            rob_chr_relocate_487e10(m,data,m.u32(rec+8));
        }
        if(m.u32(rec+0x10)==0u)break;
    }
}
void PcRobDispState::construct_595430(PcObjectDb& db){
    auto w=[&](std::uint32_t a,std::uint32_t v){for(unsigned b=0;b<4;++b)block[a-Base+b]=std::uint8_t(v>>(8*b));};
    for(std::uint32_t n=0;n<WorkCount;++n){
        const std::uint32_t d=Works+n*WorkSize;
        w(d+0xc,0x19u);
        const auto face=object_db_find_448b10(db,rdata_string(rdata32(0x5e62f4u)));
        w(d+0x14,face);w(d+0x10,face);
        w(d+0x18,object_db_find_448b10(db,rdata_string(rdata32(0x5e62f8u))));
        w(d+0x1c,object_db_find_448b10(db,rdata_string(rdata32(0x5e62fcu))));
        for(std::uint32_t k=0;k<0x30u;k+=4)w(d+0x20+k,rdata32(0x5e6300u+k));
        w(d+0x50,0x27u);w(d+0x54,0x38u);
        w(d+0x58,object_db_find_448b10(db,"HAND_MAL_DEFORUT_L"));   // 5E64C0
        w(d+0x5c,object_db_find_448b10(db,"HAND_MAL_DEFORUT_R"));   // 5E64AC
        w(d+0x8,0);w(d+0x4,0);
    }
}
void rob_disp_hands_513840(PcRaceContext& c,std::uint32_t disp,std::uint32_t kind){
    auto& m=c.m;
    auto names=[&](std::uint32_t l,std::uint32_t r){
        m.put32(disp+0x58,call(c,0x448cd0u,{l}));m.put32(disp+0x5c,call(c,0x448cd0u,{r}));};
    auto fixed=[&](std::uint32_t l,std::uint32_t r){m.put32(disp+0x58,l);m.put32(disp+0x5c,r);};
    switch(kind){                                                    // jump table 5139F0 (kind 5..19)
    case 5:fixed(0xc40020u,0xc40022u);return;
    case 6:fixed(0xca0021u,0xca0023u);return;
    case 7:names(0x5e650cu,0x5e64fcu);return;
    case 8:names(0x5e64e8u,0x5e64d4u);return;
    case 9:fixed(0xbf001bu,0xbf001du);return;
    case 10:names(0x5e652cu,0x5e651cu);return;
    case 11:fixed(0xbe0016u,0xbe0018u);return;
    case 12:names(0x5e654cu,0x5e653cu);return;
    case 13:names(0x5e65c8u,0x5e65b4u);return;
    case 14:names(0x5e65a0u,0x5e658cu);return;
    case 15:names(0x5e6574u,0x5e655cu);return;
    case 16:names(0x5e64c0u,0x5e64acu);return;
    case 17:fixed(0x1eb0017u,0x1eb0019u);return;
    case 18:fixed(0x1ec001eu,0x1ec0020u);return;
    case 19:fixed(0x1ed001eu,0x1ed001fu);return;
    default:fixed(None,None);return;
    }
}
namespace {
struct HandEntry { bool named; std::uint32_t l,r; };
// Jump tables 513B5C (513A30) and 513CCC (513BA0), kinds 5..19: fixed
// handles, or 448CD0 lookups of the names at l/r.
constexpr HandEntry HandsGu513a30[15]={{false,0xc4001fu,0xc40021u},{false,0xca0020u,0xca0022u},{true,0x5e6614u,0x5e6604u},{true,0x5e65f0u,0x5e65dcu},{false,0xbf001au,0xbf001cu},{true,0x5e6634u,0x5e6624u},{false,0xbe0017u,0xbe0019u},{true,0x5e6654u,0x5e6644u},{false,0xffffffffu,0xffffffffu},{false,0xffffffffu,0xffffffffu},{false,0xffffffffu,0xffffffffu},{false,0xffffffffu,0xffffffffu},{false,0x1eb0018u,0x1eb001au},{false,0x1ec001fu,0x1ec0021u},{false,0x1ed001cu,0x1ed001du}};
constexpr HandEntry HandsPa513ba0[15]={{false,0xc40020u,0xc40022u},{false,0xca0021u,0xca0023u},{true,0x5e650cu,0x5e64fcu},{true,0x5e64e8u,0x5e64d4u},{false,0xbf001bu,0xbf001du},{true,0x5e652cu,0x5e651cu},{false,0xbe0016u,0xbe0018u},{true,0x5e654cu,0x5e653cu},{false,0xffffffffu,0xffffffffu},{false,0xffffffffu,0xffffffffu},{false,0xffffffffu,0xffffffffu},{false,0xffffffffu,0xffffffffu},{false,0x1eb0017u,0x1eb0019u},{false,0x1ec001eu,0x1ec0020u},{false,0x1ed001eu,0x1ed001fu}};
void hands(PcRaceContext& c,std::uint32_t work,const HandEntry* table){
    auto& m=c.m;
    const std::uint32_t disp=PcRobDispState::Works+m.u32(work)*PcRobDispState::WorkSize,kind=m.u32(work+8)-5u;
    if(kind>0xeu){m.put32(disp+0x58,None);m.put32(disp+0x5c,None);return;}
    const auto& e=table[kind];
    if(!e.named){m.put32(disp+0x58,e.l);m.put32(disp+0x5c,e.r);return;}
    m.put32(disp+0x58,call(c,0x448cd0u,{e.l}));m.put32(disp+0x5c,call(c,0x448cd0u,{e.r}));
}
}
void rob_disp_ctrl_5147d0(PcRaceMemory& m,std::uint32_t work){
    const std::uint32_t d=PcRobDispState::Works+m.u32(work)*PcRobDispState::WorkSize;
    const std::int32_t n=std::int32_t(m.u32(d+8))-1;
    m.put32(d+8,n>0?std::uint32_t(n):0u);
}
void rob_disp_hand_gu_5148d0(PcRaceContext& c,std::uint32_t work){hands(c,work,HandsGu513a30);}
void rob_disp_hand_pa_5148f0(PcRaceContext& c,std::uint32_t work){hands(c,work,HandsPa513ba0);}
void rob_disp_mesh_flags_514680(PcRaceContext& c,std::uint32_t list){
    auto& m=c.m;
    const auto a=m.u32(list+8),b=m.u32(list+0xc);
    for(std::int32_t i=0;i<std::int32_t(std::int16_t(m.u16(list+2)));++i)
        call(c,0x406730u,{m.u32(a+4+std::uint32_t(i)*8u),0xfffffdffu,0});
    for(std::int32_t i=0;i<std::int32_t(std::int16_t(m.u16(list+4)));++i)
        call(c,0x406730u,{m.u32(b+0x10+std::uint32_t(i)*0x14u),0xfffffdffu,0});
}
void rob_disp_init_514bf0(PcRaceContext& c,std::uint32_t work){
    auto& m=c.m;
    const std::uint32_t disp=PcRobDispState::Works+m.u32(work)*PcRobDispState::WorkSize;
    m.put32(disp,0);
    const std::uint32_t kind=m.u32(work+8);
    std::uint32_t table=0;
    // 514AAC byte table over kind 5..19, jump table 514A94.
    static constexpr std::uint8_t Case[15]{0,0,0,0,0,0,1,1,2,3,4,5,1,1,0};
    const std::uint32_t index=kind-5u;
    if(index<=0xeu){
        switch(Case[index]){
        case 0:m.put32(disp+0x50,0x2du);m.put32(disp+0x54,0x1du);m.put32(disp,1);break;
        case 1:m.put32(disp+0x50,0x15u);m.put32(disp+0x54,0x20u);m.put32(disp,1);break;
        case 2:table=0x5e6330u;m.put32(disp+0x50,0x28u);m.put32(disp+0x54,0x38u);break;
        case 3:table=0x5e6370u;m.put32(disp+0x50,0x28u);m.put32(disp+0x54,0x38u);break;
        case 4:table=0x5e63b0u;m.put32(disp+0x50,0x28u);m.put32(disp+0x54,0x38u);break;
        default:table=0x5e62f0u;m.put32(disp+0x50,0x27u);m.put32(disp+0x54,0x38u);break;
        }
    }else m.put32(disp,1);
    rob_disp_hands_513840(c,disp,kind);
    if(table){
        face_from_table(c,disp,table);
        m.put32(disp+8,0);m.put32(disp+4,0);
        const auto face=m.u32(disp+0x10);
        if(face!=None){call(c,0x4066d0u,{face,2});call(c,0x4103f0u,{m.u32(disp+0x10),1});}
    }
    rob_disp_mesh_flags_514680(c,m.u32(0x82f388u+m.u32(work+8)*4u));
}
namespace {
using driving::CourseProbe;
constexpr std::uint32_t Mode712e30=1u,Mode712e34=1u,Flags712e38=3u;
struct Draw {
    PcRaceContext& c;
    PcRaceMemory& m;
    explicit Draw(PcRaceContext& cc):c(cc),m(cc.m){}
    std::uint32_t leaf(std::uint32_t pc,std::initializer_list<std::uint32_t> a){return call(c,pc,a);}
    void push(){driving::pc_matrix_push(c.matrices);}
    void pop(){driving::pc_matrix_pop(c.matrices);}
    void identity(){driving::pc_matrix_identity(c.matrices);}
    void push_load(std::uint32_t a){driving::pc_matrix_push_load(c.matrices,m.bytes(a,64));}
    void load(std::uint32_t a){driving::pc_matrix_load(c.matrices,m.bytes(a,64));}
    void get(std::uint32_t a){driving::pc_matrix_get(c.matrices,m.bytes(a,64));}
    void multiply(std::uint32_t a){driving::pc_matrix_multiply_current(c.matrices,m.bytes(a,64));}
    CourseProbe vec(std::uint32_t a){return {m.f32(a),m.f32(a+4),m.f32(a+8)};}
    void translate(const CourseProbe& v){driving::pc_matrix_translate_vector(c.matrices,v);}
    std::uint32_t bone(std::uint32_t mot,std::uint32_t id){           // 4F1EC0
        const std::int32_t n=std::int16_t(m.u16(mot+0x68));
        if(n>std::int32_t(id&0x7fffu)&&!(id&0x8000u))return m.u32(mot+0x74)+(id&0x7fffu)*0x350u;
        return 0u;
    }
    void object(std::uint32_t obj){leaf(0x405360u,{obj,1,0,0,None,0});}
    // 513D10 (this = display work, motion).
    void face_513d10(std::uint32_t d,std::uint32_t mot){
        const std::uint32_t bm=bone(mot,m.u32(d+0xc));
        if(!bm)return;
        push();multiply(bm);
        if(!((m.u32(d+0x10)^m.u32(d+0x14))&0xffff0000u)&&m.u32(d+4)){
            const float w=1.0f-float(std::int32_t(m.u32(d+8)))/float(std::int32_t(m.u32(d+4)));
            std::uint32_t wb;std::memcpy(&wb,&w,4);leaf(0x406800u,{wb});
            const std::uint32_t a=m.u32(d+0x10);
            if(a!=None){const std::uint32_t b=m.u32(d+0x14);if(b!=None)leaf(0x405450u,{a,b,1,0,None});}
        }else{
            leaf(0x406800u,{0x3f800000u});
            const std::uint32_t a=m.u32(d+0x10);
            if(a!=None)leaf(0x405450u,{a,a,1,0,None});
        }
        if(m.u32(d+0x18)!=None){
            push();translate(vec(d+0x20));
            driving::pc_matrix_rotate_z(c.matrices,m.f32(d+0x40));driving::pc_matrix_rotate_x(c.matrices,m.f32(d+0x38));
            object(m.u32(d+0x18));pop();
        }
        if(m.u32(d+0x1c)!=None){
            push();translate(vec(d+0x2c));
            driving::pc_matrix_rotate_z(c.matrices,m.f32(d+0x4c));driving::pc_matrix_rotate_x(c.matrices,m.f32(d+0x44));
            object(m.u32(d+0x1c));pop();
        }
        pop();
    }
    // 514280(motion, character, pool, colour, colour byte). The PC frame
    // (0x564 bytes) holds the saved matrix (+30), the palette (+70, four
    // matrices) and the bone remap table (+170): one block, so an oversized
    // palette overwrites the table as on the PC.
    void body_514280(std::uint32_t mot,std::uint32_t chr,std::uint32_t pool,std::uint32_t colour,std::int8_t colour_byte){
        std::array<std::uint8_t,0x560> frame{};
        const std::uint32_t Frame=0x7ffe0000u;                      // symbolic stack address of the frame
        const std::size_t mark=m.mark();m.map(Frame,frame.data(),frame.size());
        try{
            const std::uint32_t set=mot+0x58;
            const std::uint32_t inv=m.u32(chr+0x18),rigid=m.u32(chr+8),skinned=m.u32(chr+0xc);
            const std::int32_t bones=std::int16_t(m.u16(set+0x10));
            for(std::int32_t i=0;i<std::int32_t(std::int16_t(m.u16(chr+6)));++i){
                const std::int32_t id=std::int16_t(m.u16(m.u32(chr+0x10)+std::uint32_t(i)*2u));
                if(id&0x8000)m.put32(0x8573d0u+std::uint32_t(id&0x7fff)*4u,std::uint32_t(i));
                else m.put32(Frame+0x170u+std::uint32_t(id)*4u,std::uint32_t(i));
            }
            const std::uint32_t Saved=Frame+0x30u,Palette=Frame+0x70u;
            if(Flags712e38&1u){
                get(Saved);
                std::uint32_t e=rigid;
                for(std::int32_t n=std::int16_t(m.u16(chr+2));n>0;--n,e+=8u){
                    const std::uint32_t w=m.u32(e),id=w&0x7fffu;
                    if(!(bones>std::int32_t(id)))continue;
                    if(pool){
                        if(w&0x8000u){load(Saved);multiply(pool+id*0x40u);}
                        else{const std::uint32_t bm=bone(mot,w);if(bm){load(Saved);multiply(bm);}else identity();}
                    }else{
                        const std::uint32_t bm=(w&0x8000u)?0u:bone(mot,w);
                        if(bm){load(Saved);multiply(bm);}else identity();
                    }
                    leaf(0x405360u,{m.u32(e+4),1,0,colour,std::uint32_t(std::int32_t(colour_byte)),0});
                }
                load(Saved);
            }
            if(Flags712e38&2u){
                get(Saved);
                std::uint32_t r=skinned;
                for(std::int32_t n=std::int16_t(m.u16(chr+4));n>0;--n,r+=0x14u){
                    const std::int32_t count=std::int16_t(m.u16(r+2));
                    for(std::int32_t j=0;j<count;++j){
                        const std::uint32_t w=m.u16(r+4+std::uint32_t(j)*2u);
                        if(!(bones>std::int32_t(w&0x7fffu)))continue;       // bridge 103CB43: w & 0x7FFF
                        load(Saved);
                        std::uint32_t idx;
                        if(w&0x8000u){
                            idx=m.u32(0x8573d0u+(w&0x7fffu)*4u);
                            if(pool)multiply(pool+(w&0x7fffu)*0x40u);
                        }else{
                            idx=m.u32(Frame+0x170u+std::uint32_t(std::int32_t(std::int16_t(w)))*4u);
                            const std::uint32_t bm=bone(mot,std::uint32_t(std::int32_t(std::int16_t(w))));
                            if(bm)multiply(bm);
                        }
                        multiply(inv+idx*0x40u);
                        get(Palette+std::uint32_t(j)*0x40u);
                    }
                    load(Saved);
                    leaf(0x405580u,{m.u32(r+0x10),1,colour,std::uint32_t(std::int32_t(colour_byte)),Palette,std::uint32_t(count)});
                }
            }
        }catch(...){m.release(mark);throw;}
        m.release(mark);
    }
    void hand_513e40(std::uint32_t mot,std::uint32_t b,std::uint32_t obj){
        const std::uint32_t bm=bone(mot,b);
        if(!bm||obj==None)return;
        push();multiply(bm);object(obj);pop();
    }
    void prop_514b60(std::uint32_t work,std::uint32_t mot){
        std::uint32_t at=0,obj=0;
        const std::uint32_t kind=m.u32(work+8);
        if(kind==9u||kind==10u){at=0x624bf8u;obj=leaf(0x448cd0u,{kind==9u?0x5e6664u:0x5e6670u});}
        const std::uint32_t bm=bone(mot,0x22);
        if(!bm||!at||!obj)return;
        push_load(work+0x10);multiply(bm);translate({0.0f,0.0f,0.0f});   // 624BF8 = (0, 0, 0)
        object(obj);pop();
    }
    void disp_514c30(std::uint32_t d,std::uint32_t work){
        const std::uint32_t mot=0x82f5f0u+m.u32(work)*0xa4u;
        const std::uint32_t f=m.u32(work+4);
        if(!(f&0x8000u))return;
        if((f&8u)&&!(f&4u))return;
        leaf(0x4052b0u,{});
        push_load(work+0x10);
        push();
        const std::uint32_t kind=m.u32(work+8);
        const bool face=kind==0x10u||kind==0xdu||kind==0xeu||kind==0xfu;
        if(face)face_513d10(d,mot);
        if(Mode712e30==1u){
            if(m.u32(d)==0u&&Mode712e34==0u)throw std::logic_error("rob disp: 513E90 (712E34 is 1 in the EXE)");
            const std::uint32_t pool=0x858710u+(m.u32(work)<<11);   // 514EB0
            body_514280(mot,m.u32(0x82f388u+kind*4u),pool,0,-1);
            leaf(0x4044f0u,{0,0,0,0x80,0});
            leaf(0x4044f0u,{1,0,0,0x80,0x10000007u});
            leaf(0x405350u,{});
            leaf(0x404540u,{});
            leaf(0x4044f0u,{1,0,0,0x80,0x1000000fu});
        }
        pop();
        leaf(0x410670u,{});
        if(kind==3u||kind==4u){
            const std::uint32_t bm=bone(mot,0);
            if(!bm)throw std::runtime_error("rob disp: 409F90(NULL) on the ground disc (no bone 0)");
            push();push_load(bm);
            const CourseProbe t=driving::pc_matrix_translation(c.matrices);
            pop();
            translate({t.x,0.02f,t.z});                                  // 40A290(x, 3CA3D70A, z)
            {std::array<float,16> sc{20.0f,0,0,0, 0,1.0f,0,0, 0,0,20.0f,0, 0,0,0,1};
             driving::pc_matrix_multiply_current(c.matrices,driving::Bytes(sc.data(),64));}   // 40A360(20, 1, 20)
            const std::uint32_t preset=m.u32(0x78024cu);
            object((preset==1u||preset==3u)?0xc30023u:0x12b0023u);
            pop();
        }
        push();
        hand_513e40(mot,m.u32(d+0x50),m.u32(d+0x58));
        hand_513e40(mot,m.u32(d+0x54),m.u32(d+0x5c));
        if(!face)prop_514b60(work,mot);
        pop();
        pop();
        leaf(0x4052c0u,{});
        leaf(0x404540u,{});
    }
};
}
void rob_disp_disp_514e60(PcRaceContext& c,std::uint32_t work){
    Draw(c).disp_514c30(PcRobDispState::Works+c.m.u32(work)*PcRobDispState::WorkSize,work);
}
}

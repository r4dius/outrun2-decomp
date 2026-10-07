// PC input device layer (pc_input_devices.hpp): the device objects in their PC
// layout over this layer's memory, with the fake DirectInput they talk to.
#include "platform/pc_input_devices.hpp"
#include "platform/native_runtime.hpp"
#include "platform/race_translated.hpp"
#include "platform/pc_address_view.hpp"
#include "platform/frontend_title_widgets.hpp"
#include "driving/pc_x87.hpp"
#include <algorithm>
#include <cctype>
#include <functional>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <vector>
namespace outrun::platform {
namespace {
// Fake DirectInput objects: the COM pointers the devices keep (never dereferenced).
constexpr std::uint32_t DirectInputHandle=0xd1000001u,JoystickHandle=0xd1000010u,KeyboardHandle=0xd1000020u;
// Guest heap of operator new (5802CF): the device objects live until shutdown.
constexpr std::uint32_t HeapBase=0x0f400000u,HeapSize=0x40000u;
// Scratch for the structures the fake DirectInput hands to callbacks.
constexpr std::uint32_t ScratchBase=0x0f480000u,ScratchSize=0x1000u;
// The pad's instance GUID (stable: 4040F0 matches saved records against it).
constexpr std::uint8_t PadGuid[16]{'O','R','2','0','0','6','S','W','I','T','C','H','P','A','D','1'};
constexpr std::uint32_t RecordBase=0x8999c0u,RecordSize=0x1d4u;   // +1D0: the 453BB9 table selector (never written)
// The title owner (Options window) of the configuration screen, and the text pool.
constexpr std::uint32_t OwnerBase=0x0f600000u,StringBase=0x0f490000u;
constexpr std::uint32_t CommonBase=0x7b17f8u;          // 4164D0 common save image
constexpr std::uint32_t DevicesBase=0x7c211cu,DevicesEnd=0x7c23dcu;   // 4040F0 / 4D8547 records
struct State {
    PcRaceMemory memory;
    bool mapped{},created{};
    std::array<std::uint8_t,0x120> globals_8605e0{};   // 8605E0..860700: 8605E0/E4, devices 8606D4[5], DirectInput 8606E8
    std::uint32_t current_7398d4{0xffffffffu},count_95aec4{},retries_98abc8{};
    std::uint32_t hwnd_8a8c88{0x11u},instance_8a8cd8{0x22u};
    std::uint32_t mode_78026c{},cookie_735a00{0xbb40e64eu};
    std::array<std::uint8_t,RecordSize> record{};
    std::array<std::uint8_t,0x40> map_738b48{};          // .data default joystick map (copied from the EXE)
    std::vector<std::uint8_t> heap=std::vector<std::uint8_t>(HeapSize);
    std::uint32_t heap_used{};
    std::array<std::uint8_t,ScratchSize> scratch{};
    std::array<std::uint8_t,0x10> owner_4035f0{};        // the object 4035F0 returns (443060 reads nothing of it here)
    PcDirectInputPad pad{};
    // Options > Controls > Configuration: the .data layout block 692B00..692CC0
    // (rows, timer 692C98) and the text pool 465EB0 strings are copied to.
    std::array<std::uint8_t,0x1c0> layout_692b00{};
    bool layout_loaded{};
    std::array<std::uint8_t,0x4000> strings{};
    std::uint32_t strings_used{};
    std::array<std::uint8_t,0x200> text_84b100{};          // 84B100 input name buffer (4D6790 / 4D6900)
    std::array<std::uint32_t,3> iat_5961b4{0x0fa00008u,0x0fa00000u,0x0fa00004u};   // ToAscii, GetKeyboardLayout, MapVirtualKeyExA
    std::array<std::uint8_t,0x800> config_text{};          // texts the configuration screen formats (on the PC stack)
};
State& state(){static State s;return s;}
NativePcInputStats stats_;
void put32(std::uint8_t* p,std::uint32_t v){std::memcpy(p,&v,4);}
bool exe_bytes(std::uint32_t a,std::uint8_t* out,std::size_t n){
    for(std::size_t i=0;i<EmbeddedExeRangeCount;++i){const auto& e=EmbeddedExeRanges[i];
        if(a>=e.base&&a+n<=e.base+e.size){std::memcpy(out,e.data+(a-e.base),n);return true;}}
    return false;
}
void map_state(NativeRuntimeContext& c,State& s){
    auto& m=s.memory;
    if(!s.mapped){
        s.mapped=true;
        for(std::size_t i=0;i<EmbeddedExeRangeCount;++i)m.map_const(EmbeddedExeRanges[i].base,EmbeddedExeRanges[i].data,EmbeddedExeRanges[i].size);
        if(!exe_bytes(0x738b48u,s.map_738b48.data(),s.map_738b48.size()))throw std::runtime_error("738B48 joystick map not in the EXE ranges");
        m.map(0x8605e0u,s.globals_8605e0.data(),s.globals_8605e0.size());
        m.map(0x7398d4u,reinterpret_cast<std::uint8_t*>(&s.current_7398d4),4);
        m.map(0x95aec4u,reinterpret_cast<std::uint8_t*>(&s.count_95aec4),4);
        m.map(0x98abc8u,reinterpret_cast<std::uint8_t*>(&s.retries_98abc8),4);
        m.map(0x8a8c88u,reinterpret_cast<std::uint8_t*>(&s.hwnd_8a8c88),4);
        m.map(0x8a8cd8u,reinterpret_cast<std::uint8_t*>(&s.instance_8a8cd8),4);
        m.map(0x735a00u,reinterpret_cast<std::uint8_t*>(&s.cookie_735a00),4);   // __security_cookie (582BB3 never fails here)
        m.map(0x78026cu,reinterpret_cast<std::uint8_t*>(&s.mode_78026c),4);
        m.map(RecordBase,s.record.data(),s.record.size());
        m.map(0x738b48u,s.map_738b48.data(),s.map_738b48.size());
        m.map(HeapBase,s.heap.data(),s.heap.size());
        m.map(ScratchBase,s.scratch.data(),s.scratch.size());
        m.map(0x0fb00000u,s.owner_4035f0.data(),s.owner_4035f0.size());
        s.layout_loaded=exe_bytes(0x692b00u,s.layout_692b00.data(),s.layout_692b00.size());
        if(s.layout_loaded)m.map(0x692b00u,s.layout_692b00.data(),s.layout_692b00.size());
        m.map(StringBase,s.strings.data(),s.strings.size());
        m.map(0x84b100u,s.text_84b100.data(),s.text_84b100.size());
        m.map(0x5961b4u,reinterpret_cast<std::uint8_t*>(s.iat_5961b4.data()),12);
        m.map(0x0f4a0000u,s.config_text.data(),s.config_text.size());
        // The device records of the common save (the save owner keeps the bytes).
        auto& common=c.event_function36.frontend_profiles.common;
        m.map(DevicesBase,common.data()+(DevicesBase-CommonBase),DevicesEnd-DevicesBase);
        // The active license 7C23E0 (4D85CF marks +3F4 dirty after saving the assignments).
        auto& license=c.event_function36.frontend_profiles.active;
        m.map(0x7c23e0u,license.data(),license.size());
    }
    s.mode_78026c=c.mode_state.current;
}
// DIDEVICEINSTANCEA (0x244 bytes) of the pad.
void device_instance(std::uint8_t* p,bool keyboard){
    std::memset(p,0,0x244);put32(p,0x244);
    if(keyboard){exe_bytes(0x59672cu,p+4,16);exe_bytes(0x59672cu,p+0x14,16);put32(p+0x24,0x13u);   // DI8DEVTYPE_KEYBOARD
        std::memcpy(p+0x28,"Keyboard",9);std::memcpy(p+0x12c,"Keyboard",9);return;}
    std::memcpy(p+4,PadGuid,16);std::memcpy(p+0x14,PadGuid,16);
    put32(p+0x24,0x0115u);                                  // DI8DEVTYPE_GAMEPAD, standard subtype
    static const char name[]="Nintendo Switch Controller";
    std::memcpy(p+0x28,name,sizeof name);std::memcpy(p+0x12c,name,sizeof name);
}
// 5802DD sprintf / 580265 _vsnprintf over guest memory: the integer, character
// and string conversions with flags, width and precision.
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
        const char conv=spec.back();char buf[96];
        switch(conv){
        case '%':out+='%';continue;
        case 'd':case 'i':std::snprintf(buf,sizeof buf,spec.c_str(),std::int32_t(arg()));break;
        case 'u':case 'x':case 'X':case 'o':std::snprintf(buf,sizeof buf,spec.c_str(),arg());break;
        case 'c':std::snprintf(buf,sizeof buf,spec.c_str(),int(std::uint8_t(arg())));break;
        case 's':{std::string v;for(std::uint32_t p=arg();m.u8(p);++p)v+=char(m.u8(p));
            std::vector<char> b(v.size()+64);std::snprintf(b.data(),b.size(),spec.c_str(),v.c_str());out+=b.data();continue;}
        default:throw std::runtime_error("sprintf conversion "+spec);
        }
        out+=buf;
    }
    return out;
}
std::string guest_string(PcRaceMemory& m,std::uint32_t a){std::string v;if(!a)return v;for(;m.u8(a);++a)v+=char(m.u8(a));return v;}
// MapVirtualKeyExA(DIK scan code, MAPVK_VSC_TO_VK_EX): US layout, the keys the
// configuration can name (4D6900); 0 for the others.
std::uint32_t scan_to_vk(std::uint32_t scan){
    static const char* rows[]{"1234567890","QWERTYUIOP","ASDFGHJKL","ZXCVBNM"};
    static constexpr std::uint32_t starts[]{0x02,0x10,0x1e,0x2c};
    for(unsigned r=0;r<4;++r){const auto n=std::strlen(rows[r]);if(scan>=starts[r]&&scan<starts[r]+n)return std::uint32_t(std::uint8_t(rows[r][scan-starts[r]]));}
    switch(scan){
    case 0x01:return 0x1b;case 0x0e:return 0x08;case 0x0f:return 0x09;case 0x1c:return 0x0d;case 0x1d:return 0xa2;case 0x2a:return 0xa0;
    case 0x36:return 0xa1;case 0x38:return 0xa4;case 0x39:return 0x20;case 0x9d:return 0xa3;case 0xb8:return 0xa5;
    case 0xc7:return 0x24;case 0xc8:return 0x26;case 0xc9:return 0x21;case 0xcb:return 0x25;case 0xcd:return 0x27;
    case 0xcf:return 0x23;case 0xd0:return 0x28;case 0xd1:return 0x22;case 0xd2:return 0x2d;case 0xd3:return 0x2e;
    }
    return 0;
}
// Axis objects of the pad (EnumObjects DIDFT_AXIS): lX lY lZ lRx lRy.
constexpr std::uint32_t PadAxes=5u,PadButtons=10u;
}
NativePcInputStats& native_pc_input_stats(){return stats_;}
void native_pc_input_set_pad(NativeRuntimeContext&,const PcDirectInputPad& pad){state().pad=pad;}
const std::uint8_t* native_pc_input_record(NativeRuntimeContext&){return state().record.data();}
namespace {
// The fake DirectInput device state (GetDeviceState, COM +24).
std::uint32_t device_state(State& s,bool keyboard,std::uint8_t* p,std::uint32_t size){
    std::memset(p,0,size);
    if(keyboard)return 0u;
    if(size<0x50u)return 0x80070057u;                       // E_INVALIDARG
    for(unsigned k=0;k<8;++k)put32(p+k*4,std::uint32_t(s.pad.axes[k]));
    put32(p+0x20,s.pad.pov);put32(p+0x24,0xffffffffu);put32(p+0x28,0xffffffffu);put32(p+0x2c,0xffffffffu);
    std::memcpy(p+0x30,s.pad.buttons.data(),32);
    return 0u;
}
}
namespace {
// Owner calls of the configuration screen (FrontendTitleWidgets::config_call).
bool owner_callee(std::uint32_t pc){
    switch(pc){
    case 0x48f5f0u:case 0x4249f0u:case 0x4536f0u:case 0x48d0c0u:case 0x48ee80u:case 0x446340u:case 0x4469c0u:case 0x465250u:
    case 0x42d280u:case 0x42ca60u:case 0x42ccb0u:case 0x42cca0u:case 0x42cc00u:case 0x42ce70u:
    case 0x4ecfb0u:case 0x4ed160u:case 0x4ed360u:case 0x4ed390u:case 0x4ed250u:case 0x4ed2a0u:case 0x4ed3e0u:case 0x4eda60u:
    case 0x4ed880u:case 0x4ed8a0u:case 0x4ed8c0u:case 0x48ca30u:return true;   // membership list, not an answer
    }
    return false;
}
// The configuration screen's memory (the owner mapped at OwnerBase) and services
// (operator new, the text pool, sprintf / _vsnprintf, the owner's methods) around `body`.
using NativeBody=std::function<std::uint32_t(const PcRaceService&)>;
std::uint32_t run(NativeRuntimeContext& c,FrontendTitleWidgets* owner,const NativeBody& body){
    auto& s=state();
    map_state(c,s);
    auto& m=s.memory;
    const auto mark=m.mark();
    struct Restore{PcRaceMemory& m;std::size_t mark;~Restore(){m.release(mark);}} restore{m,mark};
    driving::Bytes object(nullptr,0);
    if(owner){object=owner->owner_bytes();m.map(OwnerBase,object.data(),object.size());}
    PcRaceService service;
    service=[&](const PcRaceCall& k)->std::uint32_t{
        switch(k.pc){
        case 0x5802cfu:{                                   // operator new
            const std::uint32_t n=(k.args[0]+7u)&~7u;
            if(s.heap_used+n>HeapSize)throw std::runtime_error("input device heap full");
            const std::uint32_t a=HeapBase+s.heap_used;s.heap_used+=n;return a;}
        case 0x5801a7u:return 0u;                          // operator delete: the objects are not reused
        case 0x4035f0u:return 0x0fb00000u;                 // title owner (only passed to 443060)
        case 0x443060u:return 0u;                          // 443060: no text entry window open
        case 0x465eb0u:{                                   // text table entry: copied to the pool (ring)
            if(!owner)break;
            const auto* t=owner->text(k.args[0]);if(!t)throw std::runtime_error("465EB0 text "+std::to_string(k.args[0]));
            const auto n=std::uint32_t(t->size()+1);
            if(s.strings_used+n>s.strings.size())s.strings_used=0;
            std::memcpy(s.strings.data()+s.strings_used,t->c_str(),n);
            const std::uint32_t a=StringBase+s.strings_used;s.strings_used+=(n+3u)&~3u;return a;}
        case 0x5802ddu:{                                   // sprintf(out, format, ...)
            std::uint32_t next=2;const auto out=guest_format(m,k.args[1],[&]{return k.args.at(next++);});
            for(std::size_t i=0;i<out.size();++i)m.put8(k.args[0]+std::uint32_t(i),std::uint8_t(out[i]));
            m.put8(k.args[0]+std::uint32_t(out.size()),0);return std::uint32_t(out.size());}
        case 0x580265u:{                                   // _vsnprintf(out, size, format, va_list)
            std::uint32_t list=k.args[3];const auto out=guest_format(m,k.args[2],[&]{const auto v=m.u32(list);list+=4;return v;});
            const std::size_t n=std::min<std::size_t>(out.size(),k.args[1]);
            for(std::size_t i=0;i<n;++i)m.put8(k.args[0]+std::uint32_t(i),std::uint8_t(out[i]));
            if(n<k.args[1])m.put8(k.args[0]+std::uint32_t(n),0);
            return out.size()<=k.args[1]?std::uint32_t(out.size()):0xffffffffu;}
        }
        if(owner&&owner_callee(k.pc)){
            std::size_t offset=0;std::vector<std::string> strings;
            auto in_owner=[&](std::uint32_t a){return a>=OwnerBase&&a<OwnerBase+object.size();};
            switch(k.pc){
            case 0x48ee80u:if(!in_owner(k.args[0]))throw std::runtime_error("48EE80 outside the owner");
                offset=k.args[0]-OwnerBase;strings={"",guest_string(m,k.args[1])};break;
            case 0x42ca60u:case 0x42ccb0u:case 0x42cca0u:case 0x42cc00u:case 0x42d280u:case 0x4536f0u:case 0x4249f0u:break;
            case 0x42ce70u:{std::uint32_t next=2;strings={"",guest_format(m,k.args[1],[&]{return k.args.at(next++);})};break;}
            default:
                if(!in_owner(k.ecx)){char t[64];std::snprintf(t,sizeof t,"%08X with ECX %08X outside the owner",k.pc,k.ecx);throw std::runtime_error(t);}
                offset=k.ecx-OwnerBase;
                if(k.pc==0x48d0c0u)strings={guest_string(m,k.args[0]),guest_string(m,k.args[1])};
                if(k.pc==0x4ed160u)strings={guest_string(m,k.args[0])};
                break;
            }
            std::uint32_t result=0;
            if(!owner->config_call(k.pc,offset,k.args.data(),k.args.size(),strings,result)){
                char t[64];std::snprintf(t,sizeof t,"configuration: owner call %08X failed",k.pc);throw std::runtime_error(t);}
            return result;
        }
        char t[64];std::snprintf(t,sizeof t,"input devices: PC callee %08X not bridged",k.pc);throw std::runtime_error(t);};
    return body(service);
}
// ---- device creation (403DE0) and saved assignments (4040F0), native ----------
// The fake DirectInput succeeds on every call (HRESULT 0 in +08).
namespace {
constexpr std::uint32_t KeyboardVtable=0x624b88u,JoystickVtable=0x624b30u,DeviceBaseVtable=0x624ad8u;
struct InputInit {
    State& s;PcRaceMemory& m;
    explicit InputInit(State& ss):s(ss),m(ss.memory){}
    std::uint32_t slot(std::uint32_t i)const{return 0x8606d4u+i*4u;}
    std::uint32_t operator_new(std::uint32_t size){                        // 5802CF
        const std::uint32_t n=(size+7u)&~7u;
        if(s.heap_used+n>HeapSize)throw std::runtime_error("input device heap full");
        const std::uint32_t a=HeapBase+s.heap_used;s.heap_used+=n;return a;
    }
    void put_guid(std::uint32_t at,const std::uint8_t* g){for(unsigned k=0;k<16;++k)m.put8(at+k,g[k]);}
    void copy_words(std::uint32_t to,std::uint32_t from,unsigned n){for(unsigned k=0;k<n;++k)m.put32(to+k*4u,m.u32(from+k*4u));}
    void zero_words(std::uint32_t at,unsigned n){for(unsigned k=0;k<n;++k)m.put32(at+k*4u,0u);}
    // 402220: the base device (vtable 624AD8).
    void base_402220(std::uint32_t o){
        m.put32(o,DeviceBaseVtable);m.put32(o+8,0);m.put8(o+4,0);m.put8(o+5,1);m.put32(o+0xc,0);m.put32(o+0x4c,0);m.put32(o+0x50,0);
        for(unsigned k=0;k<16;++k){m.put32(o+0x70+k*4,k);m.put32(o+0xb0+k*4,0);m.put32(o+0xf0+k*4,0);m.put32(o+0x130+k*4,0);}
        zero_words(o+0x1b0,8);
        for(unsigned k=0;k<7;++k)m.put32(o+0x54+k*4,8u);
    }
    // 402640: the joystick (0x2C4 bytes, vtable 624B30).
    void joystick_402640(std::uint32_t o){
        base_402220(o);
        static constexpr std::uint32_t Axes[7]{8,8,2,0,4,1,3};
        for(unsigned k=0;k<7;++k)m.put32(o+0x54+k*4,Axes[k]);
        m.put32(o,JoystickVtable);
        copy_words(o+0x70,0x738b48u,16);                                   // the .data default map
        zero_words(o+0x1d4,0x14);zero_words(o+0x224,0x14);zero_words(o+0x274,0x14);
        zero_words(0x8605e0u,6);
        for(unsigned k=0;k<8;++k){m.put32(o+0x174+k*8,0x7fffu);m.put32(o+0x170+k*8,0xffff8000u);}
        m.put8(o+0x1d0,1);
    }
    // 403660: the keyboard (0x410 bytes, vtable 624B88).
    void keyboard_403660(std::uint32_t o){
        base_402220(o);m.put32(o,KeyboardVtable);
        zero_words(o+0x1d0,0x40);zero_words(o+0x2d0,0x40);
    }
    // 402E40 (ESI device, GUID): the joystick's DirectInput device; 402F80 sets the
    // range of each axis EnumObjects lists (SetProperty, no state).
    bool joystick_create_402e40(std::uint32_t o,const std::uint8_t* guid){
        if(m.u32(o+0xc)){m.put32(o+8,0);m.put32(o+8,0);m.put32(o+0xc,0);}   // Unacquire, Release
        if(!m.u32(0x8606e8u))return false;
        if(std::memcmp(guid,PadGuid,16)!=0){m.put32(o+8,0x80040154u);return false;}   // CreateDevice: REGDB_E_CLASSNOTREG
        m.put32(o+0xc,JoystickHandle);m.put32(o+8,0);
        put_guid(o+0x3c,guid);
        m.put32(o+8,0);                                                    // GetDeviceInfo (a local copy)
        m.put32(o+8,0);m.put32(o+8,0);                                     // SetDataFormat, SetCooperativeLevel
        auto* caps=m.at(o+0x10,0x2c,true);std::memset(caps,0,0x2c);       // GetCapabilities
        put32(caps,0x2c);put32(caps+4,1u);put32(caps+8,0x0115u);put32(caps+0xc,PadAxes);put32(caps+0x10,PadButtons);put32(caps+0x14,1u);
        m.put32(o+8,0);
        for(std::uint32_t k=0;k<PadAxes;++k){                              // EnumObjects(402F80, device, DIDFT_AXIS)
            auto* p=s.scratch.data();std::memset(p,0,0x13c);put32(p,0x13c);put32(p+0x14,k*4u);put32(p+0x18,0x2u|(k<<8));
            static const char* names[]{"X Axis","Y Axis","Z Axis","X Rotation","Y Rotation"};
            std::strcpy(reinterpret_cast<char*>(p+0x20),names[k]);
        }
        m.put32(o+8,0);
        return true;
    }
    // 403990 (ESI keyboard): GUID_SysKeyboard (59672C), data format 596714, level 6.
    bool keyboard_create_403990(std::uint32_t o){
        m.put8(o+4,0);
        if(!m.u32(0x8a8c88u)||!m.u32(0x8606e8u))return false;
        m.put32(o+0xc,KeyboardHandle);m.put32(o+8,0);
        m.put32(o+8,0);m.put32(o+8,0);
        m.put8(o+4,1);return true;
    }
    // 403EF0: EnumDevices callback (the instance at `instance`).
    bool enum_403ef0(std::uint32_t instance){
        const std::uint32_t at=slot(s.count_95aec4);
        if(!m.u32(at)){const auto o=operator_new(0x2c4);joystick_402640(o);m.put32(at,o);}
        const std::uint32_t o=m.u32(at);
        std::uint8_t guid[16];std::memcpy(guid,m.at(instance+4,16),16);
        if(m.u32(o)!=JoystickVtable)throw std::logic_error("403EF0: the device slot is not a joystick");
        if(!joystick_create_402e40(o,guid)){                               // 402820 with no maps
            // 402760(1): 402780 then the base destructor (vtable 624AD8), the heap block kept.
            if(m.u32(0x8606e8u)&&m.u32(o+0xc)){m.put32(o+8,0);m.put32(o+0xc,0);}
            m.put8(o+4,0);m.put32(o,DeviceBaseVtable);
            m.put32(at,0);return true;
        }
        ++s.count_95aec4;return s.count_95aec4<4u;
    }
    // 403DE0.
    bool create_403de0(){
        m.put32(0x8606e8u,DirectInputHandle);            // DirectInput8Create
        device_instance(s.scratch.data(),false);                           // EnumDevices(game controllers): the pad
        (void)enum_403ef0(ScratchBase);
        if(std::int32_t(s.count_95aec4)>0)s.current_7398d4=0;
        const std::uint32_t n=s.count_95aec4;
        if(m.u32(slot(n))!=0u||n>=5u)return true;
        const std::uint32_t o=operator_new(0x410);keyboard_403660(o);m.put32(slot(n),o);
        // 403750(GUID_SysKeyboard, 61E678, 0): keys cleared, GUID, the device, the map in +70 and +3D0.
        zero_words(o+0x1d0,0x40);zero_words(o+0x2d0,0x40);
        std::uint8_t guid[16];std::memcpy(guid,m.at(0x59672cu,16),16);put_guid(o+0x3c,guid);
        if(keyboard_create_403990(o)){
            for(unsigned k=0;k<16;++k){const std::uint32_t v=m.u32(0x61e678u+k*4u);m.put32(o+0x70+k*4,v);m.put32(o+0x3d0+k*4,v);}
        }
        copy_words(o+0x3d0,0x61e6b8u,16);                                  // 403960(61E6B8, 0)
        ++s.count_95aec4;
        return true;
    }
    // +14 of the devices: 402420 (joystick: map +70, axes +54) / 403960 (keyboard: race map +3D0, axes +54).
    void assign(std::uint32_t o,std::uint32_t map,std::uint32_t axes){
        const bool kb=m.u32(o)==KeyboardVtable;
        if(map)copy_words(o+(kb?0x3d0u:0x70u),map,16);
        if(axes)copy_words(o+0x54,axes,7);
    }
    // 4040F0: the saved device records of the common save (7C211C, 4 x 0xB0).
    void profile_4040f0(){
        for(std::uint32_t d=0;d<s.count_95aec4;++d){
            const std::uint32_t o=m.u32(slot(d));
            std::int32_t found=-1;
            for(std::uint32_t r=0;r<4u;++r){
                const std::uint32_t rec=DevicesBase+r*0xb0u;
                if(m.u8(rec)&&std::memcmp(m.at(rec+4,16),m.at(o+0x3c,16),16)==0)found=std::int32_t(r);
            }
            if(found<0)continue;
            const std::uint32_t rec=DevicesBase+std::uint32_t(found)*0xb0u;
            assign(o,rec+0x30,rec+0x14);
            if(m.u32(o)==JoystickVtable)                                    // 403590 (keyboard 403C60: no ranges)
                for(std::uint32_t e=0;e<8u;++e){m.put32(o+0x170+e*8,m.u32(rec+0x70+e*8));m.put32(o+0x174+e*8,m.u32(rec+0x74+e*8));}
        }
    }
};
}
// ---- per-frame update (406FA0), native --------------------------------------
#define OR2_PC_RECORD struct __attribute__((may_alias))
// The input record 8999C0 (RecordSize) the game reads its buttons from.
OR2_PC_RECORD InputRecord {
    std::uint32_t unknown00;
    std::uint32_t buttons;           // +04 game buttons this frame (407070)
    std::uint32_t pressed;           // +08 set since the last frame
    std::uint32_t released;          // +0C
    std::uint32_t toggled;           // +10 flips on every press
    std::uint32_t history[4];        // +14 the buttons of this and the last 3 frames
    std::uint32_t held_frames[28];   // +24 frames held (bits 1..4)
    std::uint16_t axes[28];          // +94 analog words (407430)
    std::uint8_t unknown_cc[0x38];
    std::uint32_t double_tap;        // +104 pressed again within 15 frames of a release
    std::uint32_t repeat;            // +108 auto-repeat pulses
    std::int32_t tap_frames[28];     // +10C frames since the release
    std::int8_t tap_armed[28];       // +17C
    std::int8_t repeat_frames[28];   // +198
    std::int8_t repeat_state[28];    // +1B4 0 idle, 1 first delay, 2 repeating
    std::uint32_t table_1d0;         // +1D0
};
static_assert(sizeof(InputRecord)==RecordSize,"input record layout");
// The device objects (operator new heap; keyboard and joystick classes).
OR2_PC_RECORD InputDevice {
    std::uint32_t vtable;
    std::uint8_t created;            // +04 the DirectInput device exists
    std::uint8_t enabled;            // +05 polled every frame (4023E0)
    std::uint8_t unknown06[2];
    std::uint32_t result;            // +08 last HRESULT
    std::uint32_t com;               // +0C IDirectInputDevice8
    std::uint8_t unknown10[0x3c];
    std::uint32_t buttons;           // +4C one bit per logical button
    std::uint32_t previous;          // +50
    std::uint32_t axis_field[7];     // +54 DIJOYSTATE field of each analog input (7 or more: none)
    std::uint32_t source[16];        // +70 key / pad button of each logical button
    std::int32_t value[16];          // +B0 0, FF or the analog 0..255
    std::int32_t previous_value[16]; // +F0
    std::uint8_t unknown130[0x40];
    std::int32_t range[8][2];        // +170 min / max of each DIJOYSTATE field
    std::int32_t axis[4];            // +1B0 the four analog inputs, -128..127 (402FE0)
    float unit[4];                   // +1C0 the same past the dead zone, -1..1
};
static_assert(sizeof(InputDevice)==0x1d0,"input device layout");
OR2_PC_RECORD KeyboardDevice : InputDevice {
    std::uint8_t keys[0x100];        // +1D0 DIK key states (bit 7: down)
    std::uint8_t previous_keys[0x100];
    std::uint32_t race_source[16];   // +3D0 the keys of the logical buttons in the race (mode 16)
};
OR2_PC_RECORD JoystickDevice : InputDevice {
    std::uint8_t unknown1d0[0x54];
    std::uint8_t state[0x50];        // +224 the last DIJOYSTATE
};
static_assert(sizeof(KeyboardDevice)==0x410&&sizeof(JoystickDevice)==0x274,"device layouts");
// Float constants of the EXE the device code reads (from the EXE ranges).
constexpr std::uint32_t AxisOffset=0x628064u,AxisUnit=0x628174u,DeadZone=0x628170u,DeadZoneScale=0x62816cu,Zero=0x619a34u,
    PlusOne=0x62806cu,MinusOne=0x6280c4u,AxisRange=0x628168u,AxisCenter=0x628164u,
    AxisOnHeld=0x6280d8u,AxisOn=0x6280d4u,AxisOffHeld=0x6280d0u,AxisOff=0x6280ccu,
    TriggerPlus=0x62827cu,TriggerMinus=0x628154u,TriggerScale=0x5a91b8u;
// v*255/32786 as the device code divides (0FFDC051 reciprocal, toward zero).
constexpr std::int32_t scale_255(std::int32_t t){
    const std::int32_t q=std::int32_t((std::int64_t(t)*0x0ffdc051ll)>>32)>>11;
    return q+std::int32_t(std::uint32_t(q)>>31);
}
struct InputFrame {
    NativeRuntimeContext& c;State& s;PcRaceMemory& m;InputRecord& rec;
    InputFrame(NativeRuntimeContext& cc,State& ss):c(cc),s(ss),m(ss.memory),rec(*reinterpret_cast<InputRecord*>(ss.record.data())){}
    float k(std::uint32_t a)const{return m.f32(a);}
    template<class T=InputDevice> T& object(std::uint32_t a){return *reinterpret_cast<T*>(m.at(a,sizeof(T),true));}
    std::uint32_t device(std::uint32_t i){return m.u32(0x8606d4u+i*4u);}
    // A virtual call of a device object: the per-frame methods and the device
    // creation after a loss (+48).
    bool vcall(std::uint32_t obj,std::uint32_t slot,std::initializer_list<std::uint32_t> args={}){
        const std::uint32_t pc=m.u32(m.u32(obj)+slot);
        switch(pc){
        case 0x4023e0u:return update(obj);
        case 0x402900u:return poll_joystick(obj);
        case 0x403800u:return poll_keyboard(obj);
        case 0x402380u:return acquire(obj);
        case 0x4028d0u:{std::uint8_t g[16];std::memcpy(g,m.at(obj+0x3c,16),16);return InputInit(s).joystick_create_402e40(obj,g);}   // +48
        case 0x4037d0u:return InputInit(s).keyboard_create_403990(obj);
        }
        (void)args;
        char t[64];std::snprintf(t,sizeof t,"input device method %08X not ported",pc);throw std::logic_error(t);
    }
    // 402380(flag): Acquire (retried while 8007001E) or Unacquire; the fake device accepts both.
    bool acquire(std::uint32_t obj){return object(obj).com!=0u;}
    // 4023E0 (vtable +10): poll an enabled device (+50), clear a disabled one.
    bool update(std::uint32_t obj){
        auto& d=object(obj);
        if(d.enabled)return vcall(obj,0x50);
        d.previous=d.buttons;d.buttons=0;
        std::memset(d.value,0,sizeof d.value);
        return false;
    }
    static void begin_poll(InputDevice& d){
        std::memcpy(d.previous_value,d.value,sizeof d.value);std::memset(d.value,0,sizeof d.value);
        d.previous=d.buttons;d.buttons=0;
    }
    static void set_button(InputDevice& d,unsigned i,std::uint32_t v){
        d.value[i]=std::int32_t(v);
        if(v){d.value[i]=0xff;d.buttons|=1u<<i;}
    }
    // 403800 (keyboard +50): the 256 key states, logical buttons from the key
    // table (in the race, mode 16, its own table; 443060: no text entry here).
    bool poll_keyboard(std::uint32_t obj){
        auto& d=object<KeyboardDevice>(obj);
        if(!d.created&&(!vcall(obj,0x48)||!d.created))return false;
        std::memcpy(d.previous_keys,d.keys,sizeof d.keys);
        d.result=device_state(s,true,d.keys,sizeof d.keys);
        if(std::int32_t(d.result)<0)throw std::logic_error("403800: GetDeviceState failed (the fake keyboard never fails)");
        begin_poll(d);
        const auto* table=s.mode_78026c==16u?d.race_source:d.source;
        for(unsigned i=0;i<16;++i)set_button(d,i,d.created?d.keys[table[i]&0xffu]>>7:0u);
        return true;
    }
    // 402FE0: DIJOYSTATE field axis_field[index] from its range to -128..127.
    std::int32_t axis(const InputDevice& d,const std::uint8_t* state,unsigned index){
        using driving::X87;
        const std::uint32_t field=d.axis_field[index];
        std::int32_t raw=0;
        if(field<=7u)std::memcpy(&raw,state+field*4u,4);
        X87 v=X87(k(AxisOffset));
        if(raw){
            const X87 lo=X87(float(d.range[field][0]));
            v=(X87(raw)-lo)/(X87(d.range[field][1])-lo);
        }
        return std::int32_t(driving::x87_ftol64(v*X87(k(AxisRange))-X87(k(AxisCenter))));
    }
    // 402900 (joystick +50): Poll, the DIJOYSTATE, 16 buttons, the POV as the
    // D-pad buttons 8..11, four axes with their dead-zone units, and the
    // analog buttons 6 / 7 of the trigger and stick fields.
    bool poll_joystick(std::uint32_t obj){
        using driving::X87;
        if(!object(obj).com&&!vcall(obj,0x48))return false;
        if(!vcall(obj,0xc,{1}))return false;                  // then Poll: the fake device succeeds
        std::uint8_t js[0x50];
        if(std::int32_t(device_state(s,false,js,sizeof js))<0)throw std::logic_error("402900: GetDeviceState failed (the fake pad never fails)");
        auto& d=object<JoystickDevice>(obj);
        begin_poll(d);
        std::memcpy(d.state,js,sizeof js);
        for(unsigned i=0;i<16;++i){
            if(d.source[i]>=0x20u)throw std::logic_error("402900: button source past rgbButtons");
            set_button(d,i,js[0x30u+d.source[i]]);
        }
        std::uint32_t pov;std::memcpy(&pov,js+0x20,4);
        // D-pad: 8 up (+D0), 9 down (+D4), 10 left (+D8), 11 right (+DC).
        struct Pov{std::uint32_t angle,bits;bool up,down,left,right;};
        static constexpr Pov Povs[8]{{0,0x100,1,0,0,0},{4500,0x900,1,0,0,1},{9000,0x800,0,0,0,1},{13500,0xa00,0,1,0,1},
                                     {18000,0x200,0,1,0,0},{22500,0x600,0,1,1,0},{27000,0x400,0,0,1,0},{31500,0x500,1,0,1,0}};
        for(const auto& p:Povs)if(pov==p.angle){
            d.buttons|=p.bits;
            if(p.up)d.value[8]=0xff;
            if(p.down)d.value[9]=0xff;
            if(p.left)d.value[10]=0xff;
            if(p.right)d.value[11]=0xff;
            break;
        }
        static constexpr unsigned AxisIndex[4]{3,5,6,4};
        for(unsigned n=0;n<4;++n)d.axis[n]=axis(d,js,AxisIndex[n]);
        for(unsigned n=0;n<4;++n){
            const X87 e=(X87(d.axis[n])+X87(k(AxisOffset)))*X87(k(AxisUnit));
            const float f=driving::x87_float(e);
            const X87 sign=X87(k(e<X87(k(Zero))?MinusOne:PlusOne));
            X87 g=(driving::x87_abs(X87(f))-X87(k(DeadZone)))*X87(k(DeadZoneScale));
            if(X87(k(Zero))>g)g=X87(k(Zero));
            d.unit[n]=driving::x87_float(g*sign);
        }
        if(d.axis_field[2]!=7u){                            // one field for both analog buttons
            const std::int32_t v=axis(d,js,2),mag=scale_255((v<0?-v:v)*0xff);
            if(v<-30){d.value[7]=mag;d.value[6]=0;d.buttons|=0x80u;}
            else if(v>30){d.value[6]=mag;d.value[7]=0;d.buttons|=0x40u;}
        }
        if(d.axis_field[0]!=7u){
            const std::int32_t q=scale_255(axis(d,js,0)*0xff);
            if(q>30){d.value[7]=q;d.buttons|=0x80u;}
        }
        if(d.axis_field[1]!=7u){
            const std::int32_t q=scale_255(axis(d,js,1)*0xff);
            if(q>30){d.value[6]=q;d.buttons|=0x40u;}
        }
        return true;
    }
    // 407070(previous buttons): the game's buttons from the selected device
    // (7398D4) and the last one: logical buttons 8..15, axis directions with
    // hysteresis on the previous word, analog buttons 0..7 over 30.
    std::uint32_t game_buttons(std::uint32_t old){
        using driving::X87;
        std::uint32_t out=0;
        for(std::uint32_t i=0;std::int32_t(i)<std::int32_t(s.count_95aec4);++i){
            if(i!=s.current_7398d4&&i!=s.count_95aec4-1u)continue;
            const auto& d=object(device(i));
            const std::uint32_t held=d.previous&d.buttons;
            static constexpr std::uint32_t Held[8]{0x40,0x20,0x100,0x80,0x1,0x200,0x100000,0x8000000};
            for(unsigned n=0;n<8;++n)if(held&(0x100u<<n))out|=Held[n];
            // A direction stays on up to the "held" threshold (>= / <=), turns on past the other one.
            auto positive=[&](std::int32_t x,std::uint32_t bit){
                if(!x)return 0u;
                return (old&bit)?(X87(x)<X87(k(AxisOnHeld))?0u:bit):(X87(x)>X87(k(AxisOn))?bit:0u);};
            auto negative=[&](std::int32_t x,std::uint32_t bit){
                return (old&bit)?(X87(x)>X87(k(AxisOffHeld))?0u:bit):(X87(x)<X87(k(AxisOff))?bit:0u);};
            static constexpr std::uint32_t Directions[4][2]{{0x40000,0x80000},{0x10000,0x20000},{0x2000000,0x4000000},{0x800000,0x1000000}};
            for(unsigned n=0;n<4;++n){out|=positive(d.axis[n],Directions[n][0]);out|=negative(d.axis[n],Directions[n][1]);}
            out|=positive(d.axis[3],0x2u);out|=negative(d.axis[3],0x4u);
            static constexpr std::uint32_t Analog[8]{0x2,0x4,0x8,0x10,0x800,0x400,0x1000,0x2000};
            for(unsigned n=0;n<8;++n)if(d.value[n]>30)out|=Analog[n];
        }
        return out;
    }
    // 407430: the button edges and the analog words of the record.
    void update_record(){
        using driving::X87;
        const std::uint32_t old=rec.buttons,now=game_buttons(old);
        rec.history[0]=rec.buttons=now;
        rec.pressed=~old&now;rec.released=~now&old;
        rec.toggled^=rec.pressed;
        std::memset(rec.axes,0,sizeof rec.axes);
        std::uint16_t* w=rec.axes;
        auto max_value=[&](unsigned n,std::int32_t v){const std::int32_t cur=w[n];w[n]=std::uint16_t(cur>v?cur:v);};
        auto max_magnitude=[&](unsigned n,std::int32_t v){if(std::int32_t(w[n])<(v<0?-v:v))w[n]=std::uint16_t(v);};
        auto trigger=[&](std::int32_t v,std::uint32_t scale){
            return std::uint16_t(driving::x87_ftol64(X87(v)*X87(k(scale))*X87(k(TriggerScale))));};
        static constexpr unsigned Word[14]{1,2,3,4,11,10,12,13,6,5,8,7,0,9};   // analog word of each logical button
        for(std::uint32_t i=0;std::int32_t(i)<std::int32_t(s.count_95aec4);++i){
            const auto& d=object(device(i));
            if(!d.enabled)continue;
            for(unsigned n=0;n<14;++n)max_value(Word[n],d.value[n]);
            max_magnitude(14,d.axis[0]);max_magnitude(15,d.axis[1]);max_magnitude(21,d.axis[2]);max_magnitude(22,d.axis[3]);
            const std::int32_t t=d.axis[3];
            if(t>0){
                const std::uint16_t a=trigger(t,TriggerPlus);
                w[1]=std::max(w[1],a);w[23]=std::max(w[23],a);
            }else if(t<0){
                const std::uint16_t a=trigger(std::int32_t(0u-std::uint32_t(t)),TriggerMinus);
                w[2]=std::max(w[2],a);w[24]=std::max(w[24],a);
            }
        }
    }
    // 4077F0: double taps.
    void double_taps(){
        std::uint32_t out=0;
        for(unsigned i=0;i<28;++i){
            const std::uint32_t bit=1u<<i;
            if(rec.tap_armed[i]==1){
                if(++rec.tap_frames[i]>15)rec.tap_armed[i]=0;
                else if(rec.pressed&bit){out|=bit;rec.tap_armed[i]=0;}
            }else{
                rec.tap_armed[i]=0;
                if(rec.released&bit){rec.tap_armed[i]=1;rec.tap_frames[i]=0;}
            }
        }
        rec.double_tap=out;
    }
    // 407880: auto-repeat of the held buttons in F05FE (at once, after 16
    // frames, then every 8).
    void auto_repeat(){
        std::uint32_t out=0;
        for(unsigned i=0;i<28;++i){
            const std::uint32_t bit=1u<<i;
            if(!(rec.buttons&bit&0xf05feu)){rec.repeat_state[i]=0;continue;}
            auto& n=rec.repeat_frames[i];
            switch(rec.repeat_state[i]){
            case 1:if(++n>0xf){out|=bit;rec.repeat_state[i]=2;n=0;}break;
            case 2:if(++n>7){out|=bit;n=0;}break;
            default:out|=bit;rec.repeat_state[i]=1;n=0;break;
            }
        }
        rec.repeat=out;
    }
    // 406FA0: poll the selected device and the last one (the keyboard), then the record.
    void run_frame(){
        rec.history[3]=rec.history[2];rec.history[2]=rec.history[1];rec.history[1]=rec.history[0];
        const std::int32_t cur=std::int32_t(s.current_7398d4);
        if(cur>=0&&cur<std::int32_t(s.count_95aec4)-1)if(const auto d=device(std::uint32_t(cur)))vcall(d,0x10);
        if(std::int32_t(s.current_7398d4)>=0||s.count_95aec4==1u)if(const auto d=device(s.count_95aec4-1u))vcall(d,0x10);
        update_record();
        double_taps();
        for(unsigned i=0;i<28;++i){
            const std::uint32_t bit=1u<<i;
            if(rec.buttons&bit&0x1eu)++rec.held_frames[i];
            else if(!(rec.released&bit))rec.held_frames[i]=0;
        }
        auto_repeat();
    }
};
bool native_update(NativeRuntimeContext& c){
    auto& s=state();
    try{map_state(c,s);InputFrame(c,s).run_frame();return true;}
    catch(const PcRaceUnmapped& u){char t[96];std::snprintf(t,sizeof t,"input 406FA0: unmapped PC address %08X",u.address);stats_.last_error=t;}
    catch(const std::exception& e){stats_.last_error="input 406FA0: "+std::string(e.what());}
    ++stats_.failures;return false;
}
}
// ---- Options > Controls > Configuration (4D7E00 init, 4D7FB0 control, 4D6A60 display,
// 4D6E50 close), native over the title owner (OwnerBase) and the device objects. The
// owner's own methods and the text services go through the service of run().
namespace {
constexpr std::uint32_t ConfigText=0x0f4a0000u,ConfigTextSize=0x800u;   // the PC formats these on its stack
struct ConfigScreen {
    NativeRuntimeContext& c;State& s;PcRaceMemory& m;const PcRaceService& svc;
    static constexpr std::uint32_t O=OwnerBase,Window=OwnerBase+0x9bc,List=OwnerBase+0x1c6c,Prompt=OwnerBase+0x1ca8;
    static constexpr std::uint32_t Name=ConfigText,Message=ConfigText+0x400u;   // device name, 48CA40 text
    static constexpr std::uint32_t Text84b100=0x84b100u;
    ConfigScreen(NativeRuntimeContext& cc,State& ss,const PcRaceService& service):c(cc),s(ss),m(ss.memory),svc(service){}
    std::uint32_t call(std::uint32_t pc,std::uint32_t ecx,std::initializer_list<std::uint32_t> args){
        PcRaceCall k{};k.pc=pc;k.ecx=ecx;k.argc=std::uint32_t(args.size());unsigned i=0;for(auto a:args)k.args[i++]=a;return svc(k);}
    static std::uint32_t fbits(float f){std::uint32_t u;std::memcpy(&u,&f,4);return u;}
    std::uint32_t text(std::uint32_t id){return call(0x465eb0u,0,{id});}
    std::uint32_t u(std::uint32_t o)const{return m.u32(O+o);}
    std::int32_t i(std::uint32_t o)const{return std::int32_t(m.u32(O+o));}
    void put(std::uint32_t o,std::uint32_t v){m.put32(O+o,v);}
    std::uint32_t g(std::uint32_t a)const{return m.u32(a);}
    std::uint32_t count()const{return s.count_95aec4;}
    std::uint32_t dev(std::uint32_t k)const{return m.u32(0x8606d4u+k*4u);}
    bool keyboard(std::uint32_t d)const{return m.u32(d)==KeyboardVtable;}
    std::uint32_t timer()const{return m.u32(0x692c98u);}
    void set_timer(std::uint32_t v){m.put32(0x692c98u,v);}
    // 404020 / 404030.
    std::uint32_t current()const{return s.current_7398d4;}
    void select_404030(std::uint32_t k){if(k!=count()-1u)s.current_7398d4=k;}
    // Owner methods called through their vtables ([obj]+n).
    std::uint32_t vcall_owner(std::uint32_t obj,std::uint32_t slot,std::initializer_list<std::uint32_t> args){return call(m.u32(m.u32(obj)+slot),obj,args);}
    // 48CAE0 / 48CAD0 (window +1284 / +1280 plus 38 / 17), x87.
    driving::X87 window_y_48cae0(){return driving::X87(m.f32(Window+0x1284))+driving::X87(38.0f);}
    driving::X87 window_x_48cad0(){return driving::X87(m.f32(Window+0x1280))+driving::X87(17.0f);}
    std::int32_t window_height_48ca90(){return std::int32_t(std::int16_t(m.u16(Window+0x36)))-0x25;}
    // 48CA40(window, format, ...): _vsnprintf into 0x400 bytes, then 48EE80(window +4D4).
    void message_48ca40(std::uint32_t format,std::initializer_list<std::uint32_t> args){
        auto it=args.begin();
        const auto text=guest_format(m,format,[&]{return it!=args.end()?*it++:0u;});
        const std::size_t n=std::min<std::size_t>(text.size(),0x400);
        for(std::size_t k=0;k<n;++k)m.put8(Message+std::uint32_t(k),std::uint8_t(text[k]));
        if(n<0x400)m.put8(Message+std::uint32_t(n),0);
        (void)call(0x48ee80u,0,{Window+0x4d4u,Message});
    }
    void sprintf_84b100(std::uint32_t format,std::initializer_list<std::uint32_t> args){
        std::vector<std::uint32_t> list{Text84b100,format};list.insert(list.end(),args.begin(),args.end());
        PcRaceCall k{};k.pc=0x5802ddu;k.argc=std::uint32_t(list.size());for(std::size_t n=0;n<list.size();++n)k.args[n]=list[n];(void)svc(k);
    }
    // ---- the device methods the screen calls (vtable slots of 624B30 / 624B88) ----
    std::uint32_t& slot_word(std::uint32_t d,std::uint32_t a){return *reinterpret_cast<std::uint32_t*>(m.at(d+a,4,true));}
    static std::int32_t button_slot(std::uint32_t field){             // 4024C0 / 402550 / 4033B0 / 403A60 tables
        static constexpr std::int32_t Slots[9]{7,6,0,1,10,11,2,3,12};
        return field<9u?Slots[field]:16;
    }
    std::uint32_t map_base(std::uint32_t d)const{return keyboard(d)?0x3d0u:0x70u;}
    std::uint32_t source_1c(std::uint32_t d,std::uint32_t field){   // +1C: 4024C0 / 403C70
        const auto k=button_slot(field);return k<16?m.u32(d+map_base(d)+std::uint32_t(k)*4u):0x10u;}
    std::uint32_t set_source_20(std::uint32_t d,std::uint32_t field,std::uint32_t v){   // +20: 402550 / 403D10
        if(field>8u)return 0u;m.put32(d+map_base(d)+std::uint32_t(button_slot(field))*4u,v);return 1u;}
    std::uint32_t set_axis_24(std::uint32_t d,std::uint32_t k,std::uint32_t v){         // +24: 402610
        if(std::int32_t(v)<0||std::int32_t(v)>8)return 0u;m.put32(d+0x54+k*4u,v);return 1u;}
    void listen_begin_28(std::uint32_t d){                                               // +28: 403080 / 403A30
        if(keyboard(d)){for(std::uint32_t k=0;k<0x100;++k)m.put8(d+0x2d0+k,m.u8(d+0x1d0+k));return;}
        for(std::uint32_t k=0;k<0x14;++k){m.put32(d+0x1d4+k*4,0);m.put32(d+0x224+k*4,0);m.put32(d+0x274+k*4,0);}
        m.put8(d+0x1d0,1);
    }
    std::uint32_t axis_moved_38(std::uint32_t d){                                        // +38: 4034A0 / 563550
        if(keyboard(d))return 8u;
        std::int32_t best=std::int32_t(0xffff0001u),index=-1;
        for(std::int32_t k=0;k<8;++k){
            std::int32_t v=std::int32_t(m.u32(d+0x274+std::uint32_t(k)*4u));v=v<0?-v:v;
            if(double(v)<12287.625)continue;                                                // 6280D8
            if(v>=best){best=v;index=k;}
        }
        return best<0||index==-1?8u:std::uint32_t(index);
    }
    std::uint32_t listen_2c(std::uint32_t d,std::uint32_t field){                       // +2C: 4030C0 / 403A50
        if(keyboard(d))return 0u;
        if(m.u8(d+0x1d0)){for(std::uint32_t k=0;k<0x14;++k)m.put32(d+0x1d4+k*4,m.u32(d+0x224+k*4));m.put8(d+0x1d0,0);}
        for(std::uint32_t k=0;k<8;++k){
            std::int32_t v=std::int32_t(m.u32(d+0x1d4+k*4))-std::int32_t(m.u32(d+0x224+k*4));v=v<0?-v:v;
            if(v>std::int32_t(m.u32(d+0x274+k*4)))m.put32(d+0x274+k*4,std::uint32_t(v));
        }
        for(std::uint32_t k=0;k<32;++k){
            const std::int32_t now=m.u8(d+0x2a4+k),then=m.u8(d+0x254+k);
            std::int32_t v=now-then;v=v<0?-v:v;
            if(v>now)m.put8(d+0x2a4+k,std::uint8_t(v));
        }
        std::uint32_t axis=field;
        if(field==8u){axis=axis_moved_38(d);}
        else if(field>7u)return 0u;
        return axis<8u?m.u32(d+0x224+axis*4u):0u;
    }
    void apply_axis_30(std::uint32_t d,std::uint32_t field){                           // +30: 403350 / 491E50
        if(keyboard(d))return;
        const std::uint32_t a=axis_moved_38(d);
        if(std::int32_t(a)<0||a==8u)return;
        for(std::uint32_t k=0;k<7;++k)if(m.u32(d+0x54+k*4)==a){m.put32(d+0x54+k*4,m.u32(d+0x54+field*4));break;}   // the field's old axis
        if(std::int32_t(field)>=0&&std::int32_t(field)<7)m.put32(d+0x54+field*4,a);
    }
    // 402450 / 403BD0 (ECX slot, ESI source): the source moves to the slot; a slot that held it gets the slot's old one.
    void move_source(std::uint32_t d,std::int32_t slot,std::uint32_t source){
        if(slot<0||slot>=16)return;
        const std::uint32_t base=d+map_base(d);
        for(std::uint32_t k=0;k<16;++k)if(m.u32(base+k*4)==source)m.put32(base+k*4,m.u32(base+std::uint32_t(slot)*4u));
        m.put32(base+std::uint32_t(slot)*4u,source);
    }
    std::int32_t pressed_3c(std::uint32_t d){                                            // +3C: 403560 / 403C30
        if(keyboard(d)){
            for(std::uint32_t k=0;k<0x100;++k){const auto now=m.u8(d+0x1d0+k);if(now!=m.u8(d+0x2d0+k)&&(now&0x80u))return std::int32_t(k);}
            return -1;}
        for(std::uint32_t k=0;k<32;++k)if(m.u8(d+0x2a4+k)>0x1eu)return std::int32_t(k);
        return -1;
    }
    // 445E30 / 445DA0: the key name made wide and the characters the font lacks replaced.
    void key_name_wide(std::uint16_t* w,const std::uint8_t* narrow,unsigned limit){
        for(unsigned k=0;k<limit&&narrow[k];++k)w[k]=std::uint16_t(std::int16_t(std::int8_t(narrow[k])));
        for(std::uint16_t* p=w;*p;++p){
            const std::uint16_t ch=*p;
            if(ch>=0x20&&ch<=0x7b)continue;
            std::uint32_t k=0;for(;k<0x3d;++k)if(m.u16(0x59dc08u+k*2)==ch)break;
            if(k==0x3d){*p=0xd1;continue;}
            for(std::uint32_t j=0;j<0x40;++j)if(m.u16(0x59db08u+j*4)==ch){*p=m.u16(0x59db0au+j*4);break;}
        }
    }
    // ToAscii (fake): digits, letters (lower case) and space.
    static unsigned to_ascii(std::uint32_t vk,std::uint8_t* out){
        const bool printable=(vk>='0'&&vk<='9')||(vk>='A'&&vk<='Z')||vk==0x20;
        if(printable){const std::uint16_t v=std::uint16_t(vk>='A'&&vk<='Z'?vk+0x20:vk);std::memcpy(out,&v,2);}
        return printable?1u:0u;
    }
    void bind_34(std::uint32_t d,std::uint32_t field){                                  // +34: 4033B0 / 403A60
        if(!keyboard(d)){
            for(std::uint32_t k=0;k<32;++k)if(m.u8(d+0x2a4+k)>0x1eu){move_source(d,button_slot(field),k);return;}
            return;}
        std::int32_t key=-1;
        for(std::uint32_t k=0;k<0x100;++k){const auto now=m.u8(d+0x1d0+k);if(now!=m.u8(d+0x2d0+k)&&(now&0x80u)){key=std::int32_t(k);break;}}
        const std::uint32_t vk=scan_to_vk(std::uint32_t(key));                          // MapVirtualKeyExA(key, 3, GetKeyboardLayout(0))
        if(vk){
            std::uint8_t out[0x100]{};(void)to_ascii(vk,out);
            std::uint16_t wide[0x100]{};key_name_wide(wide,out,3);
            if(wide[0]==0xd1)return;                                                      // a character the font lacks
        }
        move_source(d,button_slot(field),std::uint32_t(key));
    }
    // ---- screen helpers ----
    // 4D6700: the axis field of rows 1..16.
    static std::uint32_t axis_field_4d6700(std::uint32_t row){
        static constexpr std::int32_t Fields[16]{0,1,2,3,5,4,4,5,0,1,2,3,8,6,7,-1};
        return row-1u<16u?std::uint32_t(Fields[row-1u]):0xffffffffu;
    }
    // 4D6790: the name text of an axis field.
    std::uint32_t axis_name_4d6790(std::uint32_t field){
        static constexpr std::uint32_t Ids[9]{0x498,0x499,0x49a,0x49b,0x49c,0x49d,0x49e,0x49f,0x496};
        return field<9u?text(Ids[field]):0u;
    }
    // 4D7080: rows 1..6 and 13 (axes) for the pads, 7 and 8 (two more keys) for the keyboard.
    void rows_4d7080(){
        const bool kb=u(0x1d6c)==count()-1u;
        const std::uint32_t on=0x4ed360u,off=0x4ed390u;
        for(std::uint32_t r=1;r<=6;++r)(void)call(kb?on:off,List,{r});
        (void)call(kb?off:on,List,{7});(void)call(kb?off:on,List,{8});
        (void)call(kb?on:off,List,{0xd});
    }
    // 4D6EA0: the help line of the selected row.
    void help_4d6ea0(){
        const std::uint32_t r=u(0x1c6c)-1u;
        std::uint32_t id=0x4a1;
        if(r<16u){
            static constexpr std::uint8_t Kind[16]{0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,2};
            const auto kind=Kind[r];
            if(kind==0){
                if(i(0x1d70)!=-1&&i(0x1d68)==1){
                    const auto name=axis_name_4d6790(axis_moved_38(dev(u(0x1d70))));
                    message_48ca40(text(0x4a5),{name});return;}
                const std::int32_t st=i(0x1d68);
                if(st<=1)id=0x4a2;
                else{
                    const std::uint32_t f=u(0x1d78);
                    switch(f){
                    case 0:id=0x4a7+(st!=2);break;case 1:id=0x4a9+(st!=2);break;case 2:id=0x4ab+(st==2);break;
                    case 3:id=0x4ad+(st!=2);break;case 4:id=0x4b1+(st!=2);break;case 5:id=0x4af+(st==2);break;
                    default:id=0x4a7;break;}
                }
            }else if(kind==1)id=i(0x1d70)!=-1?0x4a6u:0x4a3u;
            else id=0x4a4;
        }
        message_48ca40(text(id),{});
    }
    // 4D7010(field, axis): wait for the input of a row.
    void listen_4d7010(std::uint32_t field,std::uint32_t axis){
        m.put8(O+0x1d80,std::uint8_t(axis));put(0x1d70,u(0x1d6c));put(0x1d74,current());put(0x1d78,field);
        set_timer(0x25a);listen_begin_28(dev(u(0x1d6c)));
        select_404030(0xffffffffu);
        m.put8(O+0x1c7f,1);put(0x1d68,1);
    }
    // 404050(device): the default assignments.
    void defaults_404050(std::uint32_t k){
        if(k==count()-1u){const std::uint32_t d=dev(k);for(std::uint32_t j=0;j<16;++j)m.put32(d+0x3d0+j*4,m.u32(0x61e6b8u+j*4));return;}
        if(std::int32_t(k)<0||k>=count()-1u)return;
        static constexpr std::uint32_t Axes[7]{8,8,2,0,4,1,3};
        const std::uint32_t d=dev(k);
        for(std::uint32_t j=0;j<16;++j)m.put32(d+0x70+j*4,j);
        for(std::uint32_t j=0;j<7;++j)m.put32(d+0x54+j*4,Axes[j]);
    }
    // ---- 4D7E00 ----
    std::uint32_t init_4d7e00(){
        put(0x1d6c,current());put(0x1d70,0xffffffffu);put(0x1d74,current());put(0x1d7c,7);put(0x1d68,0);put(0x1d88,0);put(0x1d8c,0);
        set_timer(0);
        if(i(0x1d6c)<0&&count()!=0)put(0x1d6c,count()-1u);
        const std::uint32_t mark=(OwnerBase&~0xffu)|(i(0x1d6c)>=0?3u:2u);           // a byte over the pushed ECX
        const std::uint32_t title=text(0x4a1);
        (void)call(0x48d0c0u,Window,{0x6255d5u,title,mark,fbits(float(std::int32_t(g(0x692c40u)))),fbits(float(std::int32_t(g(0x692c2cu)))),
            fbits(float(std::int32_t(g(0x692c3cu)))),fbits(float(std::int32_t(g(0x692c30u)))),g(0x692b28u),0u});
        (void)call(0x48ee80u,0,{Window+0xdec,text(0x295)});                             // 48CBC0
        (void)call(0x48ee80u,0,{Window+0x960,text(0x296)});                             // 48CBE0
        (void)call(0x4ecfb0u,List,{0u,fbits(float(window_height_48ca90())),1u,0u,0u});
        const float y=driving::x87_float(window_y_48cae0()+driving::X87(44.0f));       // 5B0270
        const float x=driving::x87_float(window_x_48cad0());
        (void)call(0x4ed8a0u,List,{fbits(x),fbits(y)});
        (void)call(0x4ed880u,List,{0x41880000u});
        (void)call(0x4ed8c0u,List,{g(0x692b2cu)});
        for(std::uint32_t k=0;k<g(0x692c94u);++k)(void)call(0x4ed160u,List,{text(g(0x692c50u+k*4)),0u});
        rows_4d7080();
        put(0x9ac,u(0x9ac)+1u);
        return 1u;
    }
    // ---- 4D6E50 ----
    std::uint32_t close_4d6e50(){
        if(i(0x1d70)!=-1){put(0x1d6c,u(0x1d70));select_404030(u(0x1d78));}
        put(0x1d70,0xffffffffu);
        (void)call(0x4eda60u,List,{});
        const std::uint32_t r=vcall_owner(Window,0x10,{});
        m.put8(O+0x9b8,1);
        return r;                                                                        // EAX of the window call
    }
    // 4D6900(field, device): the name of the key / button of a field (84B100).
    std::uint32_t source_name_4d6900(std::uint32_t field,std::uint32_t d){
        m.put8(Text84b100,0);
        if(u(0x1d6c)!=count()-1u){
            const std::uint32_t idx=source_1c(d,field);
            sprintf_84b100(text(0x4a0),{idx+1u});return Text84b100;}
        const std::uint32_t key=source_1c(d,field);
        const std::uint32_t vk=scan_to_vk(key);
        if(!vk){                                                                         // 4D6860: by scan code
            for(std::uint32_t e=0x5cc200u,k=0;e<0x5cc8c0u;e+=12,++k)if(m.u32(e)==key){sprintf_84b100(text(m.u32(0x5cc208u+k*12)),{});break;}
            return Text84b100;}
        for(std::uint32_t e=0x5cc204u,k=0;e<0x5cc8c4u;e+=12,++k)                        // 4D68B0: by virtual key
            if(m.u32(e)==vk){sprintf_84b100(text(m.u32(0x5cc208u+k*12)),{});break;}
        if(m.u8(Text84b100))return Text84b100;
        std::uint8_t out[0x100]{};(void)to_ascii(vk,out);
        sprintf_84b100(0x626460u,{std::uint32_t(std::int32_t(std::int8_t(out[0])))});  // "%c"
        std::uint16_t wide[0x100]{};std::uint8_t narrow[4];for(unsigned k=0;k<4;++k)narrow[k]=m.u8(Text84b100+k);
        std::uint8_t zero_ended[5]{narrow[0],narrow[1],narrow[2],narrow[3],0};
        key_name_wide(wide,zero_ended,4);
        for(unsigned k=0;wide[k];++k)m.put8(Text84b100+k,std::uint8_t(wide[k]));            // 445E10 (no terminator)
        return Text84b100;
    }
    // 445D40(device, devices, x, y, height, scale): the arrows of the device row (42D280).
    void arrows_445d40(std::int32_t k,std::int32_t n,std::int32_t x,std::int32_t y,std::int32_t height,std::uint32_t scale){
        if(k>0)(void)call(0x42d280u,0,{0x3004au,std::uint32_t(x-0xc),std::uint32_t(y+3),0u,scale,0xffffffffu});
        if(k<n-1)(void)call(0x42d280u,0,{0x3004au,std::uint32_t(x+height-0xc),std::uint32_t(y+3),1u,scale,0xffffffffu});
    }
    // ---- 4D6A60 ----
    std::uint32_t display_4d6a60(){
        (void)call(0x4ed3e0u,List,{});
        bool none=i(0x1d6c)==-1;
        std::uint32_t d=0;std::array<std::uint8_t,0x244> info{};
        if(!none){d=dev(u(0x1d6c));device_instance(info.data(),keyboard(d));}          // GetDeviceInfo (fake: always succeeds)
        std::int32_t drawn=0;
        for(std::uint32_t row=0;row<0x11;++row){
            std::uint32_t label=0;
            (void)call(0x42ca60u,0,{9});(void)call(0x42ccb0u,0,{g(0x692b30u)});(void)call(0x42cca0u,0,{0xff3f474au});
            const float step=float(drawn)*17.0f;
            auto row_y=[&]{return std::int32_t(float(driving::x87_ftol32(window_y_48cae0())+0x2c)+step);};
            const std::int32_t x=std::int32_t(m.f32(0x692becu));
            (void)call(0x42cc00u,0,{std::uint32_t(x),std::uint32_t(row_y())});
            const bool kb=u(0x1d6c)==count()-1u;
            if(none)label=text(row==0?0x497u:0x496u);
            else switch(row){
            case 0:{
                m.put8(Text84b100,0);
                std::uint32_t name=Name;
                for(unsigned k=0;k<0x104;++k)m.put8(Name+k,info[0x12c+k]);
                if(current()==u(0x1d6c)){sprintf_84b100(0x5ccbe0u,{Name});name=Text84b100;}
                const std::int32_t half=window_height_48ca90()/2;
                arrows_445d40(i(0x1d6c),std::int32_t(count()),x,row_y(),half,fbits(float(std::int32_t(g(0x692b30u)))));
                label=name;break;}
            case 1:case 2:case 3:case 4:case 5:case 6:{
                static constexpr std::uint32_t Fields[7]{0,0,1,2,3,5,4};
                if(kb)break;
                const std::uint32_t k=Fields[row];
                label=axis_name_4d6790(k<7u?m.u32(d+0x54+k*4):8u);break;}
            case 7:if(kb)label=source_name_4d6900(4,d);break;
            case 8:if(kb)label=source_name_4d6900(5,d);break;
            case 9:label=source_name_4d6900(0,d);break;
            case 10:label=source_name_4d6900(1,d);break;
            case 11:label=source_name_4d6900(2,d);break;
            case 12:label=source_name_4d6900(3,d);break;
            case 13:if(!kb)label=source_name_4d6900(8,d);break;
            case 14:label=source_name_4d6900(6,d);break;
            case 15:label=source_name_4d6900(7,d);break;
            default:break;
            }
            if(i(0x1d68)>1&&u(0x1c6c)==row){
                const std::int32_t v=i(0x1d84)+0x7fff;
                std::int32_t q=std::int32_t((std::int64_t(v)*std::int64_t(std::int32_t(0xa003c017u)))>>32)+v;q>>=12;q+=std::int32_t(std::uint32_t(q)>>31);
                m.put32(Prompt+0xac,std::uint32_t(q));                                      // 446430
                (void)call(0x4469c0u,Prompt,{});
                ++drawn;
            }else if(label){
                const std::int32_t half=window_height_48ca90()/2;
                const std::int32_t at=driving::x87_ftol32(driving::X87(half)*driving::X87(0.8));   // 5CCBD8
                (void)call(0x42ce70u,0,{std::uint32_t(at),0x626468u,label});
                ++drawn;
            }
        }
        return 0x11;                                                                     // EAX: the row counter
    }
    // ---- 4D7FB0 ----
    std::uint32_t control_4d7fb0(){
        std::uint32_t choice=0xc;
        const std::int32_t row=i(0x1c6c);
        if(std::int32_t(timer())>0)set_timer(timer()-1u);
        else{
            choice=vcall_owner(O,0x14,{1});
            if(i(0x1d6c)!=-1){
                const std::uint32_t kbd=dev(count()-1u);                              // 4037E0(0x0E): backspace on the keyboard
                if(m.u8(kbd+4)&&(m.u8(kbd+0x1d0+0xe)>>7)){
                    if(row>0&&row<=6){(void)set_axis_24(dev(u(0x1d6c)),axis_field_4d6700(std::uint32_t(row)),8);return 0;}
                    if(row>6){(void)set_source_20(dev(u(0x1d6c)),axis_field_4d6700(std::uint32_t(row)),0xffffffffu);return 0;}
                    return 0;
                }
            }
        }
        const std::int32_t editing=i(0x1d70),state=i(0x1d68);
        if(editing!=-1&&state==1){
            const std::uint32_t d=dev(std::uint32_t(editing));
            bool go=pressed_3c(d)!=-1;
            if(go)set_timer(0);
            else if(std::int32_t(timer())<=0)go=true;
            else{
                (void)InputFrame(c,s).update(d);                                            // +10 (4023E0)
                if(driving::X87(541.799988f)>driving::X87(std::int32_t(timer()))){(void)listen_2c(d,8);}   // 5CCBF8
                help_4d6ea0();return 0;
            }
            if(!m.u8(O+0x1d80)){                                                            // a button / key
                bind_34(d,u(0x1d78));select_404030(u(0x1d74));
                put(0x1d70,0xffffffffu);m.put8(O+0x1c7f,0);put(0x1d68,0);set_timer(0x3c);
                help_4d6ea0();return 0;
            }
            put(0x1d7c,axis_moved_38(d));apply_axis_30(d,u(0x1d78));
            const std::int32_t list_row=i(0x1c6c);
            const std::int32_t top=driving::x87_ftol32(window_y_48cae0());
            const float py=float(std::int32_t(top+std::int32_t(g(0x692ca0u))+0x2c))+float(list_row)*17.0f-240.0f;   // 628074, 6281CC
            (void)call(0x446340u,Prompt,{0x5cc048u,0u,10u,g(0x692bbcu),fbits(py),g(0x692b30u)});
            put(0x1d68,2);set_timer(0x25a);listen_begin_28(d);
            help_4d6ea0();return 0;
        }
        if(state>1){
            const std::uint32_t d=dev(u(0x1d70));
            put(0x1d84,0);
            (void)InputFrame(c,s).update(d);
            if(driving::X87(541.799988f)>driving::X87(std::int32_t(timer()))){
                put(0x1d84,listen_2c(d,u(0x1d7c)));
                if(pressed_3c(d)!=-1&&choice==0xc){set_timer(0x25a);listen_begin_28(d);choice=0;}
            }
            help_4d6ea0();
        }
        switch(choice){
        case 0:{
            if(i(0x1d6c)!=-1&&i(0x1d68)==0){
                if(std::uint32_t(row)>16u)return 0;
                static constexpr std::int32_t Field[17]{-1,0,1,2,3,5,4,4,5,0,1,2,3,8,6,7,-2};
                static constexpr std::uint8_t Axis[17]{0,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0};
                if(row==0){select_404030(u(0x1d6c));return 0;}
                if(row==16){if(i(0x1d6c)>=0)defaults_404050(u(0x1d6c));return 0;}
                listen_4d7010(std::uint32_t(Field[row]),Axis[row]);return 0;
            }
            const std::int32_t st=i(0x1d68);
            if(st<=1)return 0;
            if(st==2){put(0x1d88,u(0x1d84));put(0x1d8c,0);put(0x1d68,3);return 0;}
            set_timer(0x3c);
            put(0x1d8c,u(0x1d84));
            {const std::uint32_t d=dev(u(0x1d70)),e=u(0x1d7c);
             if(!keyboard(d)){m.put32(d+0x170+e*8,u(0x1d88));m.put32(d+0x174+e*8,u(0x1d84));}}   // +18 (403590 / 403C60)
            (void)call(0x465250u,Prompt+4,{});                                            // 4C5440
            put(0x1d84,0);put(0x1d68,0);select_404030(u(0x1d74));put(0x1d70,0xffffffffu);m.put8(O+0x1c7f,0);
            help_4d6ea0();return 0;}
        case 1:{
            std::uint32_t done=1;
            for(std::uint32_t k=0;k<count();++k){
                const std::uint32_t d=dev(k),rec=DevicesBase+k*0xb0u;
                for(std::uint32_t j=0;j<16;++j)m.put8(rec+4+j,m.u8(d+0x3c+j));                // 4D5B40
                for(std::uint32_t j=0;j<7;++j)m.put32(rec+0x14+j*4,m.u32(d+0x54+j*4));
                for(std::uint32_t j=0;j<16;++j)m.put32(rec+0x30+j*4,m.u32(d+map_base(d)+j*4));
                for(std::uint32_t j=0;j<16;++j)m.put32(rec+0x70+j*4,m.u32(d+0x170+j*4));
                m.put8(rec,1);
            }
            m.put8(0x7c27d4u,std::uint8_t(m.u8(0x7c27d4u)|2u));
            put(0x9ac,u(0x9ac)+1u);put(0x9b4,6);put(0x9b0,done);
            return 0;}
        case 2:if(i(0x1d68)<1){(void)call(0x4ed250u,List,{});help_4d6ea0();}return 0;
        case 4:if(i(0x1d68)<1){(void)call(0x4ed2a0u,List,{});help_4d6ea0();}return 0;
        case 3:case 5:{
            if(row!=0||i(0x1d68)>=1)return 0;
            std::int32_t k=i(0x1d6c);
            if(choice==3){if(k<=0)return 0;--k;}
            else{if(k>=std::int32_t(count())-1)return 0;++k;}
            put(0x1d6c,std::uint32_t(k));rows_4d7080();(void)call(0x4249f0u,0,{1});
            return 0;}
        }
        return 0;
    }
};
}
bool native_pc_input_config_runner(void* user,std::uint32_t pc,FrontendTitleWidgets& owner,std::uint32_t& result){
    auto& c=*static_cast<NativeRuntimeContext*>(user);
    if(!state().created&&!native_pc_input_create_403de0(c))return false;
    try{
        const NativeBody body=[&](const PcRaceService& service)->std::uint32_t{
            ConfigScreen screen(c,state(),service);
            switch(pc){
            case 0x4d7e00u:return screen.init_4d7e00();
            case 0x4d7fb0u:return screen.control_4d7fb0();
            case 0x4d6e50u:return screen.close_4d6e50();
            case 0x4d6a60u:return screen.display_4d6a60();
            }
            throw std::runtime_error("configuration: no native body");};
        result=run(c,&owner,body);
        if(pc==0x4d7fb0u&&std::getenv("OR2_INPUT_DEBUG")){
            auto& st=state();auto o=owner.owner_bytes();std::uint32_t timer;std::memcpy(&timer,st.layout_692b00.data()+0x198,4);
            std::uint32_t dev;std::memcpy(&dev,st.globals_8605e0.data()+(0x8606d4u-0x8605e0u),4);
            const auto* d=st.heap.data()+(dev-HeapBase);
            std::fprintf(stderr,"[config] row %d state %u timer %u wait %d buttons %02x %02x %02x %02x pad %02x %02x %02x %02x\n",o.i32(0x1c6c),o.u32(0x1d68),timer,
                std::int32_t(o.u32(0x1d70)),d[0x2a4],d[0x2a5],d[0x2a6],d[0x2a7],st.pad.buttons[0],st.pad.buttons[1],st.pad.buttons[2],st.pad.buttons[3]);
        }
        return true;}
    catch(const PcRaceUnmapped& u){char t[96];std::snprintf(t,sizeof t,"configuration %08X: unmapped PC address %08X",pc,u.address);stats_.last_error=t;}
    catch(const std::exception& e){char t[40];std::snprintf(t,sizeof t,"configuration %08X: ",pc);stats_.last_error=t+std::string(e.what());}
    ++stats_.failures;return false;
}
bool native_pc_input_create_403de0(NativeRuntimeContext& c){
    auto& s=state();
    if(s.created)return true;
    s.created=true;
    map_state(c,s);
    try{(void)InputInit(s).create_403de0();}
    catch(const std::exception& e){stats_.last_error=std::string("input 403DE0: ")+e.what();++stats_.failures;return false;}
    ++stats_.creates;return true;
}
// Assignments of the pad while it has no saved record (the configuration
// screen's names): 0 gear up R, 1 gear down L, 2 horn B, 3 view X, 4 and 12
// start Plus, 6 brake ZL, 7 accelerate ZR; 8..11 unassigned (the D-pad is the
// POV); 13..15 the PC defaults.
// set through the device's +14 method (402420) as 4040F0 does for a saved one.
constexpr std::uint32_t SwitchButtons[16]{5,4,1,2,7,9,10,11,12,13,14,15,7,6,8,9};
constexpr std::uint32_t SwitchAxes[7]{8,8,2,0,4,1,3};   // the PC defaults (402640): steering = lX
bool native_pc_input_profile_4040f0(NativeRuntimeContext& c){
    if(!state().created&&!native_pc_input_create_403de0(c))return false;
    auto& s=state();
    map_state(c,s);
    try{InputInit(s).profile_4040f0();}
    catch(const std::exception& e){stats_.last_error=std::string("input 4040F0: ")+e.what();++stats_.failures;return false;}
    if(s.count_95aec4<2u)return true;                       // no pad
    const auto& common=c.event_function36.frontend_profiles.common;
    for(std::uint32_t k=0;k<4u;++k){
        const auto* r=common.data()+(DevicesBase-CommonBase)+k*0xb0u;
        if(r[0]&&std::memcmp(r+4,PadGuid,16)==0)return true;   // saved by the configuration screen
    }
    std::uint32_t device;std::memcpy(&device,s.globals_8605e0.data()+(0x8606d4u-0x8605e0u),4);
    std::memcpy(s.scratch.data(),SwitchButtons,sizeof SwitchButtons);
    std::memcpy(s.scratch.data()+0x40,SwitchAxes,sizeof SwitchAxes);
    InputInit(s).assign(device,ScratchBase,ScratchBase+0x40u);
    return true;
}
bool native_pc_input_update_406fa0(NativeRuntimeContext& c,PcInputDevice& device){
    auto& s=state();
    if(!s.created&&!native_pc_input_create_403de0(c))return false;
    if(!native_update(c))return false;
    ++stats_.updates;
    std::memcpy(&device.buttons_04,s.record.data()+4,4);
    std::memcpy(device.axes_94.data(),s.record.data()+0x94,sizeof device.axes_94);
    return true;
}
}

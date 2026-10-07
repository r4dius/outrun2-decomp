#include "platform/frontend_license_editor.hpp"
#include "platform/frontend_license_chooser.hpp"
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>
using namespace outrun::platform;
using outrun::driving::Bytes;
namespace {
unsigned checks{};void req(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
struct Call{unsigned pc;std::size_t offset;std::vector<unsigned> args;};
struct Fixture {
    std::array<std::uint8_t,PcLicenseEditorBytes> object{};
    PcLicense license{};std::vector<Call> trace;
    int input{-1};unsigned keyboard{},save_result{2},reject{};bool ready{};
    LicenseEditorServices services{this,call};
    Bytes bytes(){return {object.data(),object.size()};}
    static bool call(void* user,unsigned pc,std::size_t offset,const unsigned* args,std::size_t count,unsigned& result){
        auto& f=*static_cast<Fixture*>(user);f.trace.push_back({pc,offset,{}});
        if(count)f.trace.back().args.assign(args,args+count);
        if(pc==f.reject)return false;
        if(pc==0x48f5f0)result=unsigned(f.input);
        if(pc==0x4652e0)result=f.ready;
        if(pc==8&&offset==0xa78)result=f.keyboard;
        if(pc==0x416420)result=f.save_result;
        return true;
    }
    unsigned tick(){unsigned action=99;req(license_editor_tick_4de2b0(bytes(),license,services,action),"editor service dispatch");return action;}
    void init(){frontend_license_reset_4471a0(license,123);license[0x3f4]|=1;
        std::memcpy(license.data(),"ORIGINAL",9);
        req(license_editor_init_4dd7c0(bytes(),license,services),"editor init");}
};
}
int main(){try{
    Fixture f;f.init();req(f.bytes().u32(0x48)==10,"PC default attribute");
    req(f.trace[0].args[4]==1&&f.trace[0].args[2]==51,"authored entry range and entry flag");
    for(int i=0;i<20;++i)req(f.tick()==0&&f.bytes().u32(0x38)==0,"no forced animation completion");
    f.ready=true;f.tick();req(f.bytes().u32(0x38)==1,"entry completion");
    f.input=0;f.tick();req(f.bytes().u32(0x38)==2&&f.bytes().u8(0x1184)==1,"original name keyboard open");
    f.services.name_text.fill(0);std::memcpy(f.services.name_text.data(),"FERRARI",8);
    f.keyboard=1;f.tick();req(std::memcmp(f.license.data(),"FERRARI",8)==0,"name commit");
    f.tick();req(f.bytes().u32(0x38)==1,"name return after close");
    f.input=4;f.tick();req(f.bytes().u32(0x3c)==2,"PC menu skips item 1 below name");
    f.input=0;f.tick();req(f.bytes().u32(0x38)==4,"26-choice editor");
    for(unsigned stage=3;stage<=5;++stage){
        const int count=stage==3?6:stage==4?26:12,columns=stage==3?2:stage==4?4:3;
        const unsigned field=0x40+(stage-3)*4;
        for(int item=0;item<count;++item)for(int action=-1;action<=6;++action){
            Fixture g;g.init();g.bytes().put32(0x38,stage);g.bytes().puti(field,item);
            auto before=g.license;
            req(license_editor_grid(g.bytes(),g.license,stage,action,g.services),"grid dispatch");
            int expected=item;
            if(action==2&&item>=columns)expected-=columns;
            if(action==4&&item<count-columns)expected+=columns;
            if(action==3&&item%columns)expected--;
            if(action==5&&item%columns!=columns-1&&item<count-1)expected++;
            req(g.bytes().i32(field)==expected,"bounded PC grid navigation");
            req(g.bytes().u32(0x38)==(action==0||action==1?1u:stage),"grid confirm/cancel return");
            if(action!=0)req(g.license==before,"grid navigation/cancel must not commit");
        }
    }
    Fixture cancel;cancel.init();cancel.ready=true;cancel.tick();
    Bytes active(cancel.license.data(),cancel.license.size());
    active.put32(0x18,2);active.put32(0x1c,20);active.put32(0x20,5);
    std::memcpy(cancel.license.data(),"MODIFIED",9);cancel.input=1;cancel.tick();
    req(active.u32(0x18)==10&&active.u32(0x1c)==0&&active.u32(0x20)==5,"PC cancel restores only backed-up fields");
    req(std::memcmp(cancel.license.data(),"ORIGINAL",9)==0,"cancel restores name");
    req(cancel.tick()==2&&(cancel.license[0x3f4]&1)==0,"new license cancel clears occupied flag");
    Fixture existing;existing.init();existing.services.editing_existing=1;existing.ready=true;existing.tick();
    existing.input=1;existing.tick();req(existing.tick()==2&&(existing.license[0x3f4]&1),"existing edit cancellation retains slot");
    Fixture done;done.init();done.ready=true;done.tick();done.bytes().put32(0x3c,4);done.input=0;done.tick();
    req(done.bytes().u32(0x38)==6,"accept plays exit before save");done.ready=false;
    req(done.tick()==0&&done.bytes().u32(0x38)==6,"save waits for authored exit");done.ready=true;
    req(done.tick()==1&&done.bytes().u32(0x38)==7,"new license routes to key 1 after save");
    req(done.tick()==1,"terminal owner action retained");
    Fixture missing;missing.init();missing.reject=0x48f5f0;unsigned action=99;
    req(!license_editor_tick_4de2b0(missing.bytes(),missing.license,missing.services,action)&&action==0&&missing.services.missing_pc==0x48f5f0,"missing input never becomes fake success");
    std::array<std::uint8_t,PcLicenseChooserBytes> chooser{};Bytes cb(chooser.data(),chooser.size());
    cb.put32(0x3c,~0u);cb.put32(0x48,~0u);cb.put32(0x578,2);
    FrontendProfiles profiles;for(auto& r:profiles.licenses)frontend_license_reset_4471a0(r,123);
    LicenseChooserState slide;Fixture service_fixture;
    auto& cs=service_fixture.services;cs.name_text.fill(0);std::memcpy(cs.name_text.data(),"NEW LICENSE",12);
    auto choose_tick=[&](){unsigned result=99;req(license_chooser_tick_4e2970(cb,profiles,slide,cs,result),"chooser dispatch");return result;};
    choose_tick();choose_tick();req(cb.u32(0x40)==1&&cb.u32(0x3c)==0&&slide.slide==-380,"empty bank creates one new-license list item");
    for(unsigned i=0;i<9;++i){choose_tick();req(cb.u32(0x38)==2,"authored entrance duration");}
    choose_tick();req(cb.u32(0x38)==3&&slide.slide==0,"entrance reached original target");
    cs.name_text.fill(0);std::memcpy(cs.name_text.data(),"PLAYER",7);service_fixture.input=0;choose_tick();
    req(cb.u32(4)==24&&cb.u32(0x578)==4&&cb.u32(0x38)==5,"new license pushes PC key 24");
    req(profiles.selected==0&&(profiles.active[0x3f4]&1)&&profiles.active[0]=='P',"new license selects real bank record");
    for(unsigned i=0;i<9;++i)req(choose_tick()==0,"chooser waits for slide exit");
    req(choose_tick()==4,"chooser returns original push action after exit");
    chooser.fill(0);cb.put32(0x3c,~0u);profiles={};
    for(auto& r:profiles.licenses)frontend_license_reset_4471a0(r,123);
    profiles.licenses[0][0x3f4]|=1;profiles.licenses[2][0x3f4]|=1;
    frontend_profiles_select_448520(profiles,0);service_fixture.input=-1;slide={};
    choose_tick();choose_tick();for(unsigned i=0;i<10;++i)choose_tick();
    req(cb.u32(0x40)==3&&cb.u32(0x3c)==2&&cb.u32(0x9ca0)==2,"sparse profiles then first free slot");
    service_fixture.input=5;choose_tick();service_fixture.input=-1;
    for(unsigned i=0;i<20;++i)choose_tick();
    req(cb.u32(0x44)==1&&cb.u32(0x38)==3,"two-part authored right slide changes one item");
    service_fixture.input=3;choose_tick();service_fixture.input=-1;
    for(unsigned i=0;i<20;++i)choose_tick();
    req(cb.u32(0x44)==0&&cb.u32(0x38)==3,"two-part authored left slide");
    service_fixture.input=0;service_fixture.reject=0x4e12b0;action=99;
    req(!license_chooser_tick_4e2970(cb,profiles,slide,cs,action)&&action==0&&cs.missing_pc==0x4e12b0,"unported existing-profile dialog cannot be silently skipped");
    service_fixture.reject=0;cb.put32(0x38,4);cb.put32(0xb004,1);cb.put32(0x44,1);
    req(license_chooser_context_4e1cf0(cb,profiles,slide,cs,action),"existing license edit context");
    req(cs.editing_existing==1&&profiles.selected==2&&slide.restore_slot==0,"edit temporarily selects sparse slot and remembers original");
    req(cb.u32(4)==24&&cb.u32(0x578)==4&&cb.u32(0x38)==5&&slide.target==-380,"context pushes editor after original exit");
    cb.put32(0x38,4);cb.put32(0xb004,0);cb.put32(0x44,1);cb.put8(0x9ca8,1);
    auto before_context=profiles.active;
    req(license_chooser_context_4e1cf0(cb,profiles,slide,cs,action)&&action==0&&cb.u32(0x38)==4&&profiles.active==before_context,"unavailable remote profile cannot be loaded");
    cb.put8(0x9ca8,0);service_fixture.input=1;
    req(license_chooser_context_4e1cf0(cb,profiles,slide,cs,action)&&action==2,"context cancel returns to selector");
    service_fixture.input=0;service_fixture.reject=0x4164d0;
    req(!license_chooser_context_4e1cf0(cb,profiles,slide,cs,action)&&action==0&&cb.u32(0x38)==4,"missing save service must not start exit");
    std::printf("license editor: %u checks; original state/keyboard/grid/cancel/save contracts\n",checks);return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}}

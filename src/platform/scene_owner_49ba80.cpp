#include "platform/scene_owner_49ba80.hpp"
namespace outrun::platform {
std::uint32_t scene_owner_49ba80(std::uint32_t& stage,const SceneOwnerInputs& in,const SceneOwnerCall& call){
    auto c0=[&](std::uint32_t pc){return call(pc,0,{0,0,0});};
    auto c1=[&](std::uint32_t pc,std::uint32_t a){return call(pc,0,{a,0,0});};
    auto c2=[&](std::uint32_t pc,std::uint32_t a,std::uint32_t b){return call(pc,0,{a,b,0});};
    auto c3=[&](std::uint32_t pc,std::uint32_t a,std::uint32_t b,std::uint32_t d){return call(pc,0,{a,b,d});};
    auto reg=[&](std::uint32_t pc,std::uint32_t eax,std::uint32_t a){return call(pc,eax,{a,0,0});};
    const std::uint32_t variant=in.variant_780258;
    // 4999A0 takes EAX = 780258; 4962A0 has no argument.
    auto classified=[&]{return (call(0x4999a0,variant,{0,0,0})&0xffu)!=0u||(c0(0x4962a0)&0xffu)!=0u;};
    auto gate=[&]{return c0(0x448980)!=0u&&c0(0x4f21b0)!=0u;};
    c0(0x440d90);
    if(stage<5u||stage-5u>0x3au)return 0u;
    enum L{S5,S6,S7,B_bae0,B_bb13,B_bb2f,B_bb38,S8,S61,S62,S20,S21,S22,S23,S26,S27,S28,S29,S24,S25,S32,S33,
           S34,S35,S36,S37,S38,S39,S40,S41,S42,S43,S46,S47,S48,S49,S51,S52,S9,S12,S13,S14,S15,S16,S17,S50,S53,S54,S55,S63,S56,RET0};
    L at;
    switch(stage){
    case 5:at=S5;break;case 6:at=S6;break;case 7:at=S7;break;case 8:at=S8;break;case 61:at=S61;break;case 62:at=S62;break;
    case 20:at=S20;break;case 21:at=S21;break;case 22:at=S22;break;case 23:at=S23;break;case 26:at=S26;break;case 27:at=S27;break;
    case 28:at=S28;break;case 29:at=S29;break;case 24:at=S24;break;case 25:at=S25;break;case 32:at=S32;break;case 33:at=S33;break;
    case 34:at=S34;break;case 35:at=S35;break;case 36:at=S36;break;case 37:at=S37;break;case 38:at=S38;break;case 39:at=S39;break;
    case 40:at=S40;break;case 41:at=S41;break;case 42:at=S42;break;case 43:at=S43;break;case 46:at=S46;break;case 47:at=S47;break;
    case 48:at=S48;break;case 49:at=S49;break;case 51:at=S51;break;case 52:at=S52;break;case 9:at=S9;break;case 12:at=S12;break;
    case 13:at=S13;break;case 14:at=S14;break;case 15:at=S15;break;case 16:at=S16;break;case 17:at=S17;break;case 50:at=S50;break;
    case 53:at=S53;break;case 54:at=S54;break;case 55:at=S55;break;case 63:at=S63;break;case 56:at=S56;break;
    default:return 0u;
    }
    std::uint32_t eax=variant;                                  // EAX on entry through the jump table
    auto request_group=[&](std::uint32_t id){if(!classified())reg(0x49b420,id,1);};
    // 49BB8E..49BCB6 (49B390) and 49BCCD..49BDB0 (49B3C0): the same request list.
    auto requests=[&](std::uint32_t pc){
        reg(pc,0x2b,8);
        if(variant==3u||variant==4u||(c0(0x4957f0)&0xffu)!=0u||(c0(0x4b00d0)&0xffu)!=0u)reg(pc,0x2d,8);
        if((c0(0x4957f0)&0xffu)!=0u)reg(pc,0x3e,8);
        if(call(0x55a930,0x7f9460u,{0,0,0})!=0u){                    // ECX = 7F9460
            reg(pc,0x46,8);
            const auto rank=c0(0x46c520);
            if(rank==0u)reg(pc,0x40,8);else if(rank==1u)reg(pc,0x3f,8);else if(rank==2u)reg(pc,0x41,8);
        }
        if(variant==7u)reg(pc,0x4a,8);
        const auto b=c0(0x48b140)&0xffu;
        reg(pc,c1(0x4babe0,b),8);
        if(c0(0x44c2d0)==0x1cu)reg(pc,0x3c,9);
    };
    for(;;)switch(at){
    case S5:stage=6;[[fallthrough]];
    case S6:
        if(eax!=0){at=B_bb13;break;}
        {const auto b=c0(0x48b140)&0xffu;const auto b180=c0(0x48b180);const auto b1a0=c0(0x48b1a0);c3(0x480fe0,b,b1a0,b180);}
        at=B_bae0;break;
    case B_bae0:
        eax=variant;stage=7;[[fallthrough]];
    case S7:
        if(eax!=0){at=B_bb2f;break;}
        {const auto b=c0(0x48b140)&0xffu;const auto b180=c0(0x48b180);const auto b1a0=c0(0x48b1a0);c3(0x481230,b,b1a0,b180);}
        at=B_bb38;break;
    case B_bb13:
        if(eax==7u){c0(0x467ac0);at=B_bae0;break;}
        stage=8;return 0u;
    case B_bb2f:
        if(eax==7u)c0(0x4686c0);
        [[fallthrough]];
    case B_bb38:
        c0(0x440d90);stage=8;[[fallthrough]];
    case S8:
        c1(0x44fce0,1);c1(0x43db00,0);c1(0x43db00,1);stage=0x3d;[[fallthrough]];
    case S61:{
        const bool fight=variant==3u||variant==4u||(c0(0x4957f0)&0xffu)!=0u||(c0(0x48b310)&0xffu)!=0u||(c0(0x495490)&0xffu)!=0u;
        if(fight){
            if(!in.course_7d3188)return 0u;
            if(c2(0x43dba0,in.course_18_18,2)!=0u)return 0u;
            if(c2(0x43dba0,in.course_18_1c,3)!=0u)return 0u;
        }
        requests(0x49b390);
        stage=0x3e;
    }
        [[fallthrough]];
    case S62:
        if(c0(0x42df90)==0u)return 0u;
        requests(0x49b3c0);
        stage=0x14;[[fallthrough]];
    case S20:
        if(c0(0x42df90)==0u)return 0u;
        c0(0x4276b0);stage=0x15;[[fallthrough]];
    case S21:
        if(c0(0x42df90)==0u||c0(0x4299a0)==0u||c0(0x448980)==0u)return 0u;
        if(c1(0x427700,0x45a7)!=0u||c1(0x427700,1)!=0u)return 0u;
        stage=0x16;[[fallthrough]];
    case S22:
        if(c0(0x4299a0)==0u)return 0u;
        reg(0x49b3f0,0xba,8);reg(0x49b3f0,0x57,8);reg(0x49b3f0,0x1ef,8);reg(0x49b3f0,0xbb,8);
        reg(0x49b3f0,c1(0x488090,0),8);
        reg(0x49b420,6,0);reg(0x49b420,0x20,0);
        stage=0x17;[[fallthrough]];
    case S23:
        if(!gate())return 0u;
        eax=variant;stage=0x1a;[[fallthrough]];
    case S26:
        if(!classified())reg(0x49b3f0,0xbc,8);
        stage=0x1b;[[fallthrough]];
    case S27:
        if(!gate())return 0u;
        stage=0x1c;[[fallthrough]];
    case S28:
        if(c0(0x477210)!=0u)reg(0x49b3f0,0xc8,8);
        stage=0x1d;[[fallthrough]];
    case S29:
        if(!gate())return 0u;
        stage=0x18;[[fallthrough]];
    case S24:
        {const auto b=c0(0x48b140)&0xffu;reg(0x49b3f0,c1(0x46bbe0,b),8);}
        c0(0x49b870);stage=0x19;[[fallthrough]];
    case S25:
        if(c0(0x448980)==0u)return 0u;
        eax=variant;stage=0x20;[[fallthrough]];
    case S32:
        if(!classified())reg(0x49b3f0,0x0e,8);
        stage=0x21;[[fallthrough]];
    case S33:if(!gate())return 0u;eax=variant;stage=0x22;[[fallthrough]];
    case S34:request_group(0x23);stage=0x23;[[fallthrough]];
    case S35:if(!gate())return 0u;eax=variant;stage=0x24;[[fallthrough]];
    case S36:request_group(0x24);stage=0x25;[[fallthrough]];
    case S37:if(!gate())return 0u;eax=variant;stage=0x26;[[fallthrough]];
    case S38:request_group(0x25);stage=0x27;[[fallthrough]];
    case S39:if(!gate())return 0u;eax=variant;stage=0x28;[[fallthrough]];
    case S40:request_group(0x26);stage=0x29;[[fallthrough]];
    case S41:if(!gate())return 0u;eax=variant;stage=0x2a;[[fallthrough]];
    case S42:request_group(0x27);stage=0x2b;[[fallthrough]];
    case S43:if(!gate())return 0u;eax=variant;stage=0x2c;[[fallthrough]];
    case S46:request_group(0x22);stage=0x2f;[[fallthrough]];
    case S47:if(!gate())return 0u;eax=variant;stage=0x30;[[fallthrough]];
    case S48:
        if(std::int32_t(variant)<0)return 0u;
        if(!classified())reg(0x49b3f0,c1(0x488090,1),0);
        if(in.preset_78024c==1u||in.preset_78024c==3u){reg(0x49b3f0,0xc3,9);reg(0x49b420,0x17,9);}
        else{reg(0x49b3f0,0x12b,9);reg(0x49b420,0x21,9);}
        c2(0x448ad0,0x67,0);
        stage=0x31;[[fallthrough]];
    case S49:
        if(!gate())return 0u;
        stage=0x33;[[fallthrough]];
    case S51:
        reg(0x49b3f0,c1(0x44dbb0,0),9);
        reg(0x49b3f0,c1(0x44dbb0,2),9);
        reg(0x49b3f0,c1(0x44dbb0,6),9);
        stage=0x34;[[fallthrough]];
    case S52:
        if(!gate()||c0(0x42df90)==0u||c0(0x4299a0)==0u)return 0u;
        stage=9;[[fallthrough]];
    case S9:
        if(c2(0x43dba0,c1(0x44c220,0),0)!=0u)return 0u;
        stage=0xc;[[fallthrough]];
    case S12:
        c0(0x44a080);stage=0xd;[[fallthrough]];
    case S13:{
        const auto r7=c1(0x44c220,7),r6=c1(0x44c220,6),r5=c1(0x44c220,5);
        if(c3(0x44aa80,r5,r6,r7)!=0u)return 0u;
        stage=0xe;[[fallthrough]];
    }
    case S14:
        c0(0x44c2e0);stage=0xf;[[fallthrough]];
    case S15:
        if(c2(0x44c310,c1(0x44c220,4),0)!=0u)return 0u;
        if(c2(0x44c310,c1(0x44c220,0xc),1)!=0u)return 0u;
        stage=0x10;[[fallthrough]];
    case S16:
        c1(0x4f0380,0);c1(0x4f0400,0);
        if(!classified()){c1(0x46fc30,0);c0(0x4ef860);c0(0x4ef850);}
        c1(0x4f0380,1);c1(0x4f0400,1);
        if(!classified())c1(0x46fc30,1);
        c0(0x440d90);stage=0x11;[[fallthrough]];
    case S17:{
        std::uint32_t source;
        if(variant==2u){const auto r=c1(0x45c470,c0(0x44c2c0));source=r!=0u?r:c1(0x44c220,8);}
        else source=c1(0x44c220,8);
        if(c2(0x4f10d0,source,0)!=0u)return 0u;
        c0(0x440d90);
        if(c2(0x4f0430,c1(0x44c220,0xa),0)!=0u)return 0u;
        c0(0x440d90);
        if(variant!=0u&&(c0(0x4962a0)&0xffu)==0u){
            if(c3(0x46fe50,0,c1(0x44dbb0,0xb),0xb)!=0u)return 0u;
            c0(0x440d90);
            if(c2(0x4efb50,c1(0x44c220,0xd),0xd)!=0u)return 0u;
            c0(0x440d90);
            if(c2(0x4efaf0,c1(0x44c220,0xf),0xf)!=0u)return 0u;
            c0(0x440d90);
        }
        if(in.course_64!=0u)c2(0x4efd20,in.course_64,0);
        c0(0x440d90);
        c1(0x44fcc0,0);
        reg(0x49b3f0,c1(0x44dbb0,0xa),0xa);
        c0(0x440d90);
        stage=0x32;[[fallthrough]];
    }
    case S50:
        if(c0(0x448980)==0u)return 0u;
        stage=0x35;[[fallthrough]];
    case S53:
        c0(0x49a650);stage=0x36;[[fallthrough]];
    case S54:
        if(c0(0x47f110)!=0u)return 0u;
        stage=0x37;[[fallthrough]];
    case S55:
        c2(0x452e30,variant,in.preset_78024c);c2(0x452db0,variant,in.preset_78024c);
        stage=0x3f;[[fallthrough]];
    case S63:{
        if(c1(0x427700,1)!=0u)return 0u;
        std::uint32_t tail;
        if(variant==2u){if(c1(0x427700,0x899f)!=0u)return 0u;tail=0x8102;}
        else if(variant==5u){if(c1(0x427700,0x899f)!=0u)return 0u;tail=0x8196;}
        else if(variant==6u||variant==4u){if(c1(0x427700,0x89a0)!=0u)return 0u;tail=0x869b;}
        else{if(c1(0x427700,0x899e)!=0u)return 0u;tail=0x8103;}
        if(c1(0x427700,tail)!=0u||c1(0x427700,0x207)!=0u)return 0u;
        stage=0x38;return 1u;
    }
    case S56:return 1u;
    case RET0:return 0u;
    }
}
}

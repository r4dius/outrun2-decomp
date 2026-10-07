#include "platform/sp_rankings.hpp"
#include "driving/pc_x87.hpp"
namespace outrun::platform {
namespace {
std::uint32_t book(PcRaceMemory& m){return m.u32(0x63960cu);}
void copy16(PcRaceMemory& m,std::uint32_t dst,std::uint32_t src){for(std::uint32_t k=0;k<0x10;k+=4)m.put32(dst+k,m.u32(src+k));}
std::uint32_t w(std::uint32_t v){return v&0xffffu;}   // movzx of a word argument
std::uint32_t at_453090(PcRaceMemory& m,std::uint32_t i,std::uint32_t j,std::uint32_t k){
    i=w(i);j=w(j);k=w(k);
    if(i==2u||i==3u)return book(m)+0x250cu+(k+i*10u)*0x10u;
    return book(m)+0xccu+(k+(i*5u+j)*10u)*0x10u;
}
std::uint32_t at_453120(PcRaceMemory& m,std::uint32_t i,std::uint32_t j,std::uint32_t k){
    i=w(i);j=w(j);k=w(k);
    return book(m)+0x70cu+(k+(i*5u+j)*10u)*0x10u;
}
std::uint32_t at_453170(PcRaceMemory& m,std::uint32_t i,std::uint32_t e,std::uint32_t s,std::uint32_t j,std::uint32_t k){
    i=w(i);e=w(e);s=w(s);j=w(j);k=w(k);
    if(i==2u||i==3u)return book(m)+0x228cu+(k+10u*(s+2u*(e+2u*i)))*0x10u;
    return book(m)+0xd4cu+(k+10u*(j+5u*(s+2u*(e+2u*i))))*0x10u;
}
// 449C10(h, m, s, ms): ((h*60 + m)*60 + s)*1000 + ms
std::uint32_t time_449c10(std::uint32_t h,std::uint32_t mi,std::uint32_t s,std::uint32_t ms){return ((w(h)*0x3cu+w(mi))*0x3cu+w(s))*0x3e8u+w(ms);}
std::int32_t smod(std::int32_t a,std::int32_t b){return a%b;}               // idiv remainder
std::uint32_t mod4(std::uint32_t v){                                          // and 0x80000003 / jns / dec / or -4 / inc
    std::uint32_t r=v&0x80000003u;
    if(std::int32_t(r)<0){r-=1u;r|=0xfffffffcu;r+=1u;}
    return r;
}
// name of the default record: [5DA328 / 5DA32C / 5DA330] by r (memcpy of 4 bytes); other r: none
void name(PcRaceMemory& m,std::uint32_t dst,std::int32_t r){
    std::uint32_t src;
    if(r==0)src=0x5da328u;else if(r==1)src=0x5da32cu;else if(r==2)src=0x5da330u;else return;
    m.put32(dst,m.u32(src));
}
}
void sp_book_record_453090(PcRaceMemory& m,std::uint32_t d,std::uint32_t i,std::uint32_t j,std::uint32_t k){copy16(m,d,at_453090(m,i,j,k));}
void sp_book_record_453120(PcRaceMemory& m,std::uint32_t d,std::uint32_t i,std::uint32_t j,std::uint32_t k){copy16(m,d,at_453120(m,i,j,k));}
void sp_book_record_453170(PcRaceMemory& m,std::uint32_t d,std::uint32_t i,std::uint32_t e,std::uint32_t s,std::uint32_t j,std::uint32_t k){copy16(m,d,at_453170(m,i,e,s,j,k));}
void sp_book_store_452f00(PcRaceMemory& m,std::uint32_t s0,std::uint32_t i,std::uint32_t j,std::uint32_t k){copy16(m,at_453090(m,i,j,k),s0);}
void sp_book_store_452f70(PcRaceMemory& m,std::uint32_t s0,std::uint32_t i,std::uint32_t j,std::uint32_t k){copy16(m,at_453120(m,i,j,k),s0);}
void sp_book_store_452fc0(PcRaceMemory& m,std::uint32_t s0,std::uint32_t i,std::uint32_t e,std::uint32_t s,std::uint32_t j,std::uint32_t k){copy16(m,at_453170(m,i,e,s,j,k),s0);}
void sp_tables_from_book_4f3cc0(PcRaceMemory& m){
    std::uint32_t off=0;
    for(std::uint32_t i=0;i<2;++i){
        std::uint32_t P=0x84e580u+i*0xc80u;
        for(std::uint32_t j=0;j<5;++j)for(std::uint32_t k=0;k<10;++k){
            sp_book_record_453090(m,0x84df40u+off,i,j,k);
            sp_book_record_453120(m,0x8504c0u+off,i,j,k);
            std::uint32_t d=P;
            for(std::uint32_t e=0;e<2;++e)for(std::uint32_t s=0;s<2;++s){sp_book_record_453170(m,d,i,e,s,j,k);d+=0x320u;}
            off+=0x10u;P+=0x10u;
        }
    }
    std::uint32_t Q=0x84fe80u;
    for(std::uint32_t i=0;i<2;++i){
        const std::uint32_t j=i+1u;
        std::uint32_t P2=0x84ffc0u+i*0x280u;
        for(std::uint32_t k=0;k<10;++k){
            sp_book_record_453090(m,Q,i+2u,j,k);
            std::uint32_t d=P2;
            for(std::uint32_t e=0;e<2;++e)for(std::uint32_t s=0;s<2;++s){sp_book_record_453170(m,d,i+2u,e,s,j,k);d+=0xa0u;}
            Q+=0x10u;P2+=0x10u;
        }
    }
}
void sp_book_from_tables_4f3ad0(PcRaceMemory& m){
    std::uint32_t off=0;
    for(std::uint32_t i=0;i<2;++i){
        std::uint32_t P=0x84e580u+i*0xc80u;
        for(std::uint32_t j=0;j<5;++j)for(std::uint32_t k=0;k<10;++k){
            sp_book_store_452f00(m,0x84df40u+off,i,j,k);
            sp_book_store_452f70(m,0x8504c0u+off,i,j,k);
            std::uint32_t d=P;
            for(std::uint32_t e=0;e<2;++e)for(std::uint32_t s=0;s<2;++s){sp_book_store_452fc0(m,d,i,e,s,j,k);d+=0x320u;}
            off+=0x10u;P+=0x10u;
        }
    }
    std::uint32_t Q=0x84fe80u;
    for(std::uint32_t i=0;i<2;++i){
        const std::uint32_t j=i+1u;
        std::uint32_t P2=0x84ffc0u+i*0x280u;
        for(std::uint32_t k=0;k<10;++k){
            sp_book_store_452f00(m,Q,i+2u,j,k);
            std::uint32_t d=P2;
            for(std::uint32_t e=0;e<2;++e)for(std::uint32_t s=0;s<2;++s){sp_book_store_452fc0(m,d,i+2u,e,s,j,k);d+=0xa0u;}
            Q+=0x10u;P2+=0x10u;
        }
    }
    // 453330: 47F110 = 0, nothing
}
void sp_tables_defaults_4f3ea0(PcRaceMemory& m){
    sp_tables_from_book_4f3cc0(m);
    // 47F110 = 0: the default records
    std::uint32_t off=0;
    for(std::uint32_t t=0;t<2;++t){
        std::uint32_t P=0x84e588u+t*0xc80u;                     // cursor (+8 of the record)
        for(std::uint32_t c=0;c<5;++c){
            const std::uint32_t hi=0x5da334u+std::uint32_t(smod(std::int32_t(c),5))*4u;
            for(std::uint32_t k=0;k<10;++k){
                const std::int32_t v=std::int32_t(c+k);
                const std::int32_t r=smod(v,3);
                name(m,0x84df4cu+off,r);name(m,0x8504ccu+off,r);
                m.put32(0x84df40u+off,(m.u32(0x84df40u+off)&0xff0186a0u)|0x186a0u);
                m.put32(0x84df48u+off,m.u32(0x84df48u+off)&0xfffff000u);
                m.put32(0x84df44u+off,time_449c10(0,4,0x32,0));
                {std::uint32_t a=(m.u32(0x5da318u+mod4(std::uint32_t(v))*4u)<<28)|(m.u32(0x84df40u+off)&0xffffffu);
                 a^=(m.u32(hi)&0xfu)<<24;m.put32(0x84df40u+off,a);}
                const std::int32_t rk=smod(std::int32_t(k),3);
                const bool clear=rk!=0&&smod(std::int32_t(k),7)!=0&&w(k)!=0u;
                m.put32(0x84df48u+off,clear?(m.u32(0x84df48u+off)&0xfffdffffu):(m.u32(0x84df48u+off)|0x20000u));
                m.put32(0x8504c0u+off,(m.u32(0x8504c0u+off)&0xff0186a0u)|0x186a0u);
                m.put32(0x8504c8u+off,(m.u32(0x8504c8u+off)&0xfffff0fau)|0xfau);
                m.put32(0x8504c4u+off,time_449c10(0,4,0x32,0));
                {std::uint32_t a=(m.u32(0x5da318u+mod4(c+2u+k-1u)*4u)<<28)|(m.u32(0x8504c0u+off)&0xffffffu);   // [esp+24] = c+2+k
                 a^=(m.u32(hi)&0xfu)<<24;m.put32(0x8504c0u+off,a);}
                m.put32(0x8504c8u+off,clear?(m.u32(0x8504c8u+off)&0xfffdffffu):(m.u32(0x8504c8u+off)|0x20000u));
                const std::uint32_t lead=0x5da318u+mod4(c+2u+k)*4u;
                std::uint32_t e=P;
                for(std::uint32_t q=0;q<2;++q){
                    const std::uint32_t qb=(q&1u)<<17;
                    for(std::uint32_t b=0;b<2;++b){
                        name(m,e+4u,r);
                        m.put32(e-8u,(m.u32(e-8u)&0xff0186a0u)|0x186a0u);
                        m.put32(e,m.u32(e)&0xfffe0000u);
                        m.put32(e-4u,time_449c10(0,4,0x32,0));
                        std::uint32_t a=(m.u32(e-8u)&0xffffffu)|(m.u32(lead)<<28);
                        a^=(m.u32(0x5da334u+c*4u)&0xfu)<<24;m.put32(e-8u,a);
                        std::uint32_t f=(m.u32(e)&0xfffdffffu)|qb;
                        f&=0xfffbffffu;f|=(b&1u)<<18;m.put32(e,f);
                        e+=0x320u;
                    }
                }
                off+=0x10u;P+=0x10u;
            }
        }
    }
    std::uint32_t Q=0x84fe88u;
    for(std::uint32_t t=0;t<2;++t){
        const std::uint32_t u=t+1u;
        std::uint32_t P2=0x84ffc8u+t*0x280u;
        for(std::uint32_t k=0;k<10;++k){
            const std::int32_t v=std::int32_t(u+k);
            const std::int32_t r=smod(v,3);
            name(m,Q+4u,r);
            m.put32(Q-8u,(m.u32(Q-8u)&0xff0186a0u)|0x186a0u);
            m.put32(Q,m.u32(Q)&0xfffff000u);
            m.put32(Q-4u,time_449c10(0,0xe,0x28,0));
            {std::uint32_t a=(m.u32(Q-8u)&0xffffffu)|(m.u32(0x5da318u+mod4(std::uint32_t(v))*4u)<<28);
             a^=(m.u32(0x5da334u+u*4u)&0xfu)<<24;m.put32(Q-8u,a);}
            const std::int32_t rk=smod(std::int32_t(k),3);
            const bool clear=rk!=0&&smod(std::int32_t(k),7)!=0&&w(k)!=0u;
            m.put32(Q,clear?(m.u32(Q)&0xfffdffffu):(m.u32(Q)|0x20000u));
            const std::uint32_t lead=0x5da318u+mod4(u+2u+k)*4u;
            std::uint32_t e=P2;
            for(std::uint32_t q=0;q<2;++q){
                const std::uint32_t qb=(q&1u)<<17;
                for(std::uint32_t b=0;b<2;++b){
                    name(m,e+4u,r);
                    m.put32(e-8u,(m.u32(e-8u)&0xff0186a0u)|0x186a0u);
                    m.put32(e,m.u32(e)&0xfffe0000u);
                    m.put32(e-4u,time_449c10(0,0xe,0x28,0));
                    std::uint32_t a=(m.u32(e-8u)&0xffffffu)|(m.u32(lead)<<28);
                    a^=(m.u32(0x5da334u+u*4u)&0xfu)<<24;m.put32(e-8u,a);
                    std::uint32_t f=(m.u32(e)&0xfffdffffu)|qb;
                    f&=0xfffbffffu;f|=(b&1u)<<18;m.put32(e,f);
                    e+=0xa0u;
                }
            }
            Q+=0x10u;P2+=0x10u;
        }
    }
}
void sp_stats_from_book_453210(PcRaceMemory& m){
    const std::uint32_t b=book(m);
    m.put32(0x7d672cu,m.u8(b+3));m.put32(0x7d6730u,m.u8(b+9));
    m.put32(0x7d6748u,m.u8(b+0x10));m.put32(0x7d674cu,m.u8(b+0x12));
    m.put32(0x7d6734u,m.u8(b+0xe));m.put32(0x7d6738u,m.u8(b+0x11));
    m.put32(0x7d673cu,m.u8(b+0x13));m.put32(0x7d6740u,m.u8(b+0x14));
    m.put32(0x7d6744u,1);
    m.put32(0x7d6754u,m.u8(b+7));m.put32(0x7d6758u,m.u8(b+8));m.put32(0x7d6750u,m.u8(b+6));
}
void sp_book_header_452b80(PcRaceMemory& m){
    const std::uint32_t b=book(m);
    for(std::uint32_t k=0;k<0x30;k+=4)m.put32(b+k,0);
    m.put16(b,0xb);m.put32(b+2,0x2020202u);
    m.put8(b+7,1);m.put8(b+6,1);m.put8(b+8,1);m.put8(b+0xa,1);m.put8(b+0xc,1);m.put8(b+0xe,1);m.put8(b+0xf,1);m.put8(b+0x10,1);
    m.put8(b+0x13,2);m.put8(b+0x14,2);
    m.put8(b+9,0);m.put8(b+0xb,0x80);m.put8(b+0xd,0);m.put8(b+0x12,0);m.put8(b+0x11,0);
    m.put32(b+0x15,0x2020202u);
}
void sp_book_init_453440(PcRaceMemory& m){
    const std::uint32_t b=book(m);
    for(std::uint32_t k=0;k<0x30;k+=4)m.put32(b+k,0);
    m.put16(b,0xb);m.put32(b+2,0x2020202u);
    m.put8(b+9,0);m.put8(b+0xd,0);m.put8(b+0x10,0);m.put8(b+0x12,0);m.put8(b+0x11,0);
    m.put8(b+0x13,2);m.put8(b+0x14,2);
    m.put8(b+7,1);m.put8(b+6,1);m.put8(b+8,1);m.put8(b+0xa,1);m.put8(b+0xb,0x80);m.put8(b+0xc,1);m.put8(b+0xe,1);m.put8(b+0xf,1);
    m.put32(b+0x15,0x2020202u);
    sp_tables_defaults_4f3ea0(m);
    sp_stats_from_book_453210(m);
    for(std::uint32_t k=0;k<0x98;k+=4)m.put32(b+0x30+k,0);
    m.put16(b+0x30,0x12);
    sp_book_from_tables_4f3ad0(m);
}
// 452ED0(word a, word b): [7D6728] = a, [7D6720] = b.
void sp_stats_set_452ed0(PcRaceMemory& m,std::uint32_t a,std::uint32_t b){
    m.put16(0x7d6728u,std::uint16_t(a));m.put16(0x7d6720u,std::uint16_t(b));
}
// 452CD0(word i, word j): [7D675C] = book[+40 + (i + j*12)*4] + 1 (the play count).
void sp_stats_count_452cd0(PcRaceMemory& m,std::uint32_t i,std::uint32_t j){
    m.put32(0x7d675cu,m.u32(book(m)+0x40u+(w(i)+w(j)*12u)*4u)+1u);
}
// 452D20(word i, d): [7D6724] = ftol((t * (n - 1) + d) / n) on the x87, n = [7D675C], t =
// book[+A0 + i*4] (both unsigned: fild + 2^32 when negative).
void sp_stats_average_452d20(PcRaceMemory& m,std::uint32_t i,std::uint32_t d){
    using driving::X87;
    auto fild_u=[](std::uint32_t v){X87 r=X87(std::int32_t(v));if(std::int32_t(v)<0)r=r+X87(4294967296.0f);return r;};   // 628070
    const X87 n=fild_u(m.u32(0x7d675cu));
    const X87 t=fild_u(m.u32(book(m)+0xa0u+w(i)*4u));
    X87 r=(n-X87(1.0f))*t;                       // 62806C
    r=r+fild_u(d);
    r=r/n;
    m.put32(0x7d6724u,std::uint32_t(std::uint64_t(driving::x87_ftol64(r))));
}
// ---- the name entry's record accessors (4F3010 .. 4F3A80) ----
// Record addresses: 84DF40 + ((count + preset*5)*10 + i)*16 (variant 1, presets 0/1),
// 84FE80 + ((preset - 2)*10 + i)*16 (variant 1, presets 2/3), 8504C0 + ((count + preset*5)*10
// + i)*16 (variant 2), 84E580 + ((count + 5*(b + 2*(a + 2*preset)))*10 + i)*16 (variant 0,
// presets 0/1), 84FFC0 + ((b + (a + 2*preset - 4)*2)*10 + i)*16 (variant 0, presets 2/3).
std::uint32_t sp_record_4f3810(std::uint32_t preset,std::uint32_t count,std::uint32_t i){
    if(preset==2u||preset==3u)return 0x84fe80u+((preset*5u-10u)*2u+i)*16u;
    return 0x84df40u+((count+preset*5u)*10u+i)*16u;   // VM 4F3835: add eax,[1039EC8] (= 84DF40)
}
std::uint32_t sp_record_4f3870(std::uint32_t preset,std::uint32_t a,std::uint32_t b,std::uint32_t count,std::uint32_t i){
    if(preset==2u||preset==3u)return 0x84ffc0u+((b+(a+preset*2u-4u)*2u)*10u+i)*16u;
    return 0x84e580u+((count+5u*(b+2u*(a+2u*preset)))*10u+i)*16u;
}
std::uint32_t sp_record_4f3910(std::uint32_t preset,std::uint32_t count,std::uint32_t i){
    return 0x8504c0u+((count+preset*5u)*10u+i)*16u;
}
// 4F3950 / 4F39E0 / 4F3A80: store a record, its +8 bits 12..16 = [7C23FC] (the license's car).
void sp_store_record(PcRaceMemory& m,std::uint32_t at,const std::array<std::uint32_t,4>& r){
    const std::uint32_t r8=r[2]^((r[2]^(m.u32(0x7c23fcu)<<12))&0x1f000u);
    m.put32(at,r[0]);m.put32(at+4u,r[1]);m.put32(at+8u,r8);m.put32(at+0xcu,r[3]);
}
// 4F3010(n, records, ranks, variant, preset, count, a, b): the n (1..4) records, all of the
// same +3 low nibble (else -1), sorted (variant 1: +0 & FFFFFF descending, 2: +8 & FFF
// descending, other: +4 ascending), each inserted into its table of 10 (variant 0: +4 below,
// 1: +0 & FFFFFF above, other: +8 & FFF above the entry; later entries move down, the tenth is
// lost); ranks[k] = its row or -1. The 4B1680 call it makes discards its result.
std::int32_t sp_rank_query_4f3440(const PcRaceMemory& m,std::vector<std::array<std::uint32_t,4>> local,std::vector<std::int32_t>& ranks,
                                  std::uint32_t variant,std::uint32_t preset,std::uint32_t count,std::uint32_t a,std::uint32_t b){
    const std::int32_t n=std::int32_t(local.size());
    if(n>4||n<=0)return -1;
    std::uint32_t table;
    if(variant==1u)table=(preset==2u||preset==3u)?0x84fe80u+(preset*5u-10u)*32u:0x84df40u+(count+preset*5u)*160u;
    else if(variant==2u)table=0x8504c0u+(count+preset*5u)*160u;
    else table=(preset==2u||preset==3u)?0x84ffc0u+(b+(a+preset*2u-4u)*2u)*160u:0x84e580u+(count+5u*(b+2u*(a+2u*preset)))*160u;
    const std::uint32_t nibble=(local[0][0]>>24)&0xfu;                 // byte 3 & 0xF
    for(std::int32_t k=1;k<n;++k)if(((local[std::size_t(k)][0]>>24)&0xfu)!=nibble)return -1;
    for(std::int32_t i=0;i+1<n;++i)for(std::int32_t j=i+1;j<n;++j){
        auto& x=local[std::size_t(i)];auto& y=local[std::size_t(j)];
        bool swap;
        if(variant==1u)swap=(x[0]&0xffffffu)<(y[0]&0xffffffu);
        else if(variant==2u)swap=(x[2]&0xfffu)<(y[2]&0xfffu);
        else swap=x[1]>y[1];
        if(swap)std::swap(x,y);
    }
    ranks.assign(std::size_t(n),-1);
    for(std::int32_t k=0;k<n;++k){
        const auto& r=local[std::size_t(k)];
        std::uint32_t at=0;
        for(;at<10u;++at){
            const std::uint32_t e=table+at*16u;
            if(variant==0u){if(r[1]<m.u32(e+4u))break;}
            else if(variant==1u){if((r[0]&0xffffffu)>(m.u32(e)&0xffffffu))break;}
            else if((r[2]&0xfffu)>(m.u32(e+8u)&0xfffu))break;
        }
        ranks[std::size_t(k)]=at==10u?-1:std::int32_t(at);
    }
    return 0;
}
std::int32_t sp_rank_insert_4f3010(PcRaceMemory& m,std::int32_t n,std::uint32_t records,std::uint32_t ranks,std::uint32_t variant,
                                   std::uint32_t preset,std::uint32_t count,std::uint32_t a,std::uint32_t b){
    if(n>4||n<=0)return -1;
    std::uint32_t table;
    if(variant==1u)table=(preset==2u||preset==3u)?0x84fe80u+(preset*5u-10u)*32u:0x84df40u+(count+preset*5u)*160u;
    else if(variant==2u)table=0x8504c0u+(count+preset*5u)*160u;
    else table=(preset==2u||preset==3u)?0x84ffc0u+(b+(a+preset*2u-4u)*2u)*160u:0x84e580u+(count+5u*(b+2u*(a+2u*preset)))*160u;
    const std::uint32_t nibble=m.u8(records+3u)&0xfu;
    for(std::int32_t k=1;k<n;++k)if((m.u8(records+std::uint32_t(k)*16u+3u)&0xfu)!=nibble)return -1;
    std::array<std::array<std::uint32_t,4>,4> local{};
    for(std::int32_t k=0;k<n;++k)for(std::uint32_t w4=0;w4<4u;++w4)local[std::size_t(k)][w4]=m.u32(records+std::uint32_t(k)*16u+w4*4u);
    for(std::int32_t i=0;i+1<n;++i)for(std::int32_t j=i+1;j<n;++j){
        auto& x=local[std::size_t(i)];auto& y=local[std::size_t(j)];
        bool swap;
        if(variant==1u)swap=(x[0]&0xffffffu)<(y[0]&0xffffffu);
        else if(variant==2u)swap=(x[2]&0xfffu)<(y[2]&0xfffu);
        else swap=x[1]>y[1];
        if(swap)std::swap(x,y);
    }
    for(std::int32_t k=0;k<n;++k){
        const auto& r=local[std::size_t(k)];
        std::uint32_t at=0;
        for(;at<10u;++at){
            const std::uint32_t e=table+at*16u;
            if(variant==0u){if(r[1]<m.u32(e+4u))break;}
            else if(variant==1u){if((r[0]&0xffffffu)>(m.u32(e)&0xffffffu))break;}
            else if((r[2]&0xfffu)>(m.u32(e+8u)&0xfffu))break;
        }
        if(at==10u){m.put32(ranks+std::uint32_t(k)*4u,0xffffffffu);continue;}
        for(std::uint32_t t=9u;t>at;--t)for(std::uint32_t w4=0;w4<16u;w4+=4u)m.put32(table+t*16u+w4,m.u32(table+(t-1u)*16u+w4));
        for(std::uint32_t w4=0;w4<4u;++w4)m.put32(table+at*16u+w4*4u,r[w4]);
        m.put32(ranks+std::uint32_t(k)*4u,at);
    }
    return 0;
}
}

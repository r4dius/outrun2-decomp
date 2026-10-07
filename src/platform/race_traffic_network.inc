    // ==== LAN race (variant 4): the CommRace block 7DE418 and the LAN car collisions =========
    // The CommRace block (ECX = 7DE418): +0 the player count, then one 0x6C entry per player
    // at +4: +0 its session player (network memory: +0 word id, +8 state), +4 the expected id,
    // +24 / +2C / +30 / +34 the latest frame / time words, +3C / +40 / +44 the finish words,
    // +54 the clock of its last packet, +58 / +5C flags, +64 / +68 the progress floats.
    static constexpr std::uint32_t CommRace=0x7de418u;
    std::uint32_t clock(){return u32(0x7f1938u);}
    // 4F53B0: the versus byte [850B04] is set.
    bool versus_4f53b0(){return u8(0x850b04u)!=0u;}
    // 456670 (ECX = entry): the player is connected (id matches, state 3 / 4) and its last
    // packet is less than 0xFF frames old (any age when 4F53B0 is clear).
    bool alive_456670(std::uint32_t e){
        std::int32_t age=std::int32_t(clock()-u32(e+0x54));
        if(!versus_4f53b0())age=0;
        const std::uint32_t p=u32(e);
        if(u16(p)!=u32(e+4))return false;
        const std::uint32_t st=u32(p+8);
        return (st==3u||st==4u)&&age<0xff;
    }
    std::uint32_t players(std::uint32_t s){return u32(s);}
    std::uint32_t entry(std::uint32_t s,std::uint32_t k){return s+4u+k*0x6cu;}
    // 459BB0: the oldest +24 word of the connected players (the clock when none is older).
    std::uint32_t oldest_459bb0(std::uint32_t s){
        std::uint32_t r=clock();
        for(std::uint32_t k=0;k<players(s);++k){
            const std::uint32_t e=entry(s,k);
            if(alive_456670(e)&&u32(e+0x24)<r)r=u32(e+0x24);
        }
        return r;
    }
    // 456870: no connected player still racing (+58 != 1).
    bool all_done_456870(std::uint32_t s){
        for(std::uint32_t k=0;k<players(s);++k){
            const std::uint32_t e=entry(s,k);
            if(alive_456670(e)&&u8(e+0x58)!=1u)return false;
        }
        return true;
    }
    // 459D10(idx): the finish rank of player idx (-1 when it is still unknown).
    std::uint32_t rank_459d10(std::uint32_t s,std::uint32_t idx){
        const std::uint32_t r=s+idx*0x6cu;
        const std::uint32_t fin=u32(r+0x3c),gave=u32(r+0x44);
        std::uint32_t n=0;
        if(fin>oldest_459bb0(s))return 0xffffffffu;
        if(fin){
            const std::uint32_t t=u32(r+0x40);
            for(std::uint32_t k=0;k<players(s);++k){
                const std::uint32_t o=s+k*0x6cu;
                if(!u32(o+0x3c))continue;
                const std::uint32_t x=u32(o+0x40);
                if(t>x||(t==x&&std::int32_t(idx)>std::int32_t(k)))++n;
            }
            return n;
        }
        if(u8(r+0x5c)){
            for(std::uint32_t k=0;k<players(s);++k){
                const std::uint32_t o=s+k*0x6cu;
                if(u32(o+0x3c)){++n;continue;}
                const std::uint32_t x=u32(o+0x44);
                if(gave<x||(gave==x&&std::int32_t(idx)>std::int32_t(k)))++n;
            }
            return n;
        }
        if(alive_456670(r+4u))return 0xffffffffu;
        for(std::uint32_t k=0;k<players(s);++k)
            if(alive_456670(entry(s,k))||std::int32_t(idx)>std::int32_t(k))++n;
        return n;
    }
    // 459E10(idx): the running rank of player idx by the progress floats (+64 / +68).
    std::uint32_t rank_459e10(std::uint32_t s,std::uint32_t idx){
        const std::uint32_t r=s+idx*0x6cu;
        if(u8(r+0x5c))return rank_459d10(s,idx);
        const float zero=K(0x619a34u);
        float mine=0.0f;
        if(alive_456670(r+4u)){mine=f32(r+0x68);if(mine==zero)mine=f32(r+0x6c);}
        std::uint32_t n=0;
        for(std::uint32_t k=0;k<players(s);++k){
            const std::uint32_t e=entry(s,k);
            float v=0.0f;
            if(alive_456670(e)){v=f32(e+0x64);if(v==zero)v=f32(e+0x68);}
            if(v>mine||u8(e+0x58)||(v==mine&&k<idx))++n;
        }
        return n;
    }
    // 45A2B0(id): the player's rank: the 7DF118 table outside the race (mode 0x10); in a
    // variant 3 / 4 race 459D10 once every player is done, else 459E10.
    std::uint32_t rank_45a2b0(std::uint32_t id){
        if(u32(0x78026cu)!=0x10u)return u8(0x7df118u+id);
        const std::uint32_t v=u32(0x780258u);
        if(v!=3u&&v!=4u)return 0;
        return std::uint8_t(all_done_456870(CommRace)?rank_459d10(CommRace,id):rank_459e10(CommRace,id));
    }
    // The index of the first 2-bit field equal to 2 in a packed word (14 when none).
    static std::uint32_t first_two(std::uint32_t v){
        std::uint32_t i=0;
        while(i<0xeu&&(v&3u)!=2u){v>>=2;++i;}
        return i;
    }
    // 456720(packed): every player restarts from the packed word (+30 = 0, +34 = word,
    // +38 = its first-two index); +364 / +368 the same for the block.
    void reset_456720(std::uint32_t s,std::uint32_t v){
        const std::uint32_t i=first_two(v);
        for(std::uint32_t k=0;k<players(s);++k){p32(s+0x30u+k*0x6cu,0);p32(s+0x34u+k*0x6cu,v);p32(s+0x38u+k*0x6cu,i);}
        p32(s+0x364u,v);p32(s+0x368u,first_two(v));
    }
    // 4568E0(count, &key, flag): with flag, [+374] counts the calls (key to +370) and count
    // grows once it is passed; the newest +44 of the connected players beyond count takes the
    // key (+48) and +4C; this machine's entry (7DD138) is raised to it. 1 when it was raised.
    std::uint32_t best_4568e0(std::uint32_t s,std::uint32_t count,std::uint32_t key,std::uint32_t flag){
        if(flag){
            const std::uint32_t n=u32(s+0x374u)+1u;p32(s+0x374u,n);
            p32(s+0x370u,u32(key));
            if(n>count)++count;
        }
        std::uint32_t when=clock();
        for(std::uint32_t k=0;k<players(s);++k){
            const std::uint32_t e=entry(s,k);
            if(!alive_456670(e))continue;
            if(count<u32(e+0x44)){count=u32(e+0x44);p32(key,u32(e+0x48));when=u32(e+0x4c);}
        }
        const std::uint32_t own=s+std::uint32_t(u8(0x7dd138u))*0x6cu;
        if(u32(own+0x48)>=count)return 0;
        p32(own+0x48,count);p32(own+0x4c,u32(key));p32(own+0x50,when);
        return 1;
    }
    // 4569D0(time): with time <= 0, 1 when no connected player's +24 is before clock + time.
    std::uint32_t late_4569d0(std::uint32_t s,std::uint32_t time){
        if(std::int32_t(time)>0)return 0;
        const std::uint32_t lim=time+clock();
        for(std::uint32_t k=0;k<players(s);++k){
            const std::uint32_t e=entry(s,k);
            if(alive_456670(e)&&u32(e+0x24)<lim)return 0;
        }
        return 1;
    }
    // 459C30: the block's packed word / index (+364 / +368) move to the connected player whose
    // +2C is the smallest one not beyond 459BB0 with a later +34 index (+30 its word).
    void next_459c30(std::uint32_t s){
        std::uint32_t word=u32(s+0x364u),idx=u32(s+0x368u);
        const std::uint32_t lim=oldest_459bb0(s);
        std::uint32_t best=lim;
        for(std::uint32_t k=0;k<players(s);++k){
            const std::uint32_t e=entry(s,k);
            if(!alive_456670(e))continue;
            const std::uint32_t t=u32(e+0x2c);
            if(t>lim||u32(e+0x34)<=idx||t>best)continue;
            best=t;word=u32(e+0x30);idx=u32(e+0x34);
        }
        p32(s+0x368u,idx);p32(s+0x364u,word);
    }
    // 456790: the smallest nonzero +3C of the players - clock + 0x256 (0 when none).
    std::uint32_t remaining_456790(std::uint32_t s){
        bool found=false;std::uint32_t best=0;
        for(std::uint32_t k=0;k<u32(s);++k){
            const std::uint32_t v=u32(s+0x3cu+k*0x6cu);
            if(!v)continue;
            if(!found||v<best){best=v;found=true;}
        }
        return found?best-clock()+0x256u:0u;
    }
    // 4F9EA0: the car-car collision pass of a LAN race (4F9450 with the players first, by
    // their +10 slot, no damage-state skips, and the +2C8 last-hit test against [6282A8]).
    void collide_4f9ea0(){
        Frame E(*this,0x1c0);
        const std::uint32_t recB=E(0),recA=E(0xe0);   // two 0xD4 records
        auto live=[&](std::uint32_t id){return (u8(0x79fb48u+id)&3u)==2u;};
        const long double last_hit=[&]{std::uint64_t b=u32(0x6282a8u)|(std::uint64_t(u32(0x6282acu))<<32);
            double d;std::memcpy(&d,&b,8);return (long double)d;}();
        for(std::uint32_t id=8;id<0x20u;++id){
            if(!live(id))continue;
            const std::uint32_t car=work(id);
            if(u8(car+8)&1u)continue;
            p32(car+4,u32(car+4)&0xfba6ffffu);
            p32(car+0xae8,0x19a);p32(car+0xdd4,0x19a);
            if(i16(car+0xb4c)>0)p16(car+0xb4c,u16(car+0xb4c)-1u);
            if(i16(car+0xb4e)>0)p16(car+0xb4e,u16(car+0xb4e)-1u);
            {const std::uint32_t o=u32(car+0xc60);
                if(o!=0x19au&&(!live(o)||last_hit>=(long double)f32(work(o)+0x2c8)))p32(car+0xc60,0x19a);}
            const std::uint32_t n=u8(car+0x335);
            for(std::uint32_t k=0;k<n;++k){
                const std::uint32_t bp=car+0x44c+k*0x1ecu;
                p32(bp-0xe0,0);
                for(std::uint32_t j=0;j<4;++j){const std::uint32_t q=bp-0xd4+j*0x34u;p32(q-4,0);p32(q,0xffffffffu);p32(q-8,0);}
                p32(bp-0xc,0);
                for(std::uint32_t j=0;j<4;++j){const std::uint32_t q=bp+j*0x34u;p32(q-4,0);p32(q,0xffffffffu);p32(q-8,0);}
                for(std::uint32_t q=0;q<0x10;q+=4)p32(bp+0xc8+q,0);
                for(std::uint32_t q=0;q<0x1c;q+=4)p32(bp+0x2c4+q,u32(bp-0x114+q));
                for(std::uint32_t q=0;q<0xc;q+=4)p32(bp+0x2e0+q,u32(bp-0xf8+q));
                for(std::uint32_t q=0;q<0xc;q+=4)p32(bp+0x2ec+q,u32(bp-0xec+q));
                if(gt(f32(car+0xd24),K(0x6281f0u))){
                    const float lo=sadd(f32(bp-0x10c),f32(car+0xd24));pf(bp-0x10c,lo);
                    const float hi=sadd(f32(bp-0x100),f32(car+0xd24));pf(bp-0x100,hi);
                    const float r=f32(bp-0xfc);
                    const float d=smul(ssub(lo,hi),K(0x628064u));
                    const float q=ssub(smul(r,r),smul(d,d));
                    const X87 root=driving::x87_sqrt(X87(driving::x87_float(driving::x87_abs(X87(q)))));
                    pf(bp-0xf8,driving::x87_float(X87(f32(bp-0x114))-root));
                    pf(bp-0xec,driving::x87_float(root+X87(f32(bp-0x108))));
                }
            }
        }
        // the players by their +10 slot, then the other events
        std::array<std::uint32_t,8> slot{};
        const std::uint32_t np=u32(0x680ad4u);
        for(std::uint32_t i=0;std::int32_t(i)<std::int32_t(np);++i)
            if((u8(0x79fb50u+i)&3u)==2u){const std::uint32_t w=u32(0x799d18u+i*0x3cu);slot.at(u8(w+0x10))=w;}
        auto racer=[&](std::uint32_t i)->std::uint32_t{
            if(std::int32_t(i)<std::int32_t(u32(0x680ad4u)))return slot[i];
            return (u8(0x79fb50u+i)&3u)==2u?u32(0x799d18u+i*0x3cu):0u;
        };
        auto usable=[&](std::uint32_t w){
            return !(u8(w+8)&1u)&&!(u8(w+4)&0x40u)&&!gt(K(0x5e0ac0u),f32(w+0x300))&&std::int8_t(u8(w+0x329))<=0xa;
        };
        for(std::uint32_t i=0;i<0x18u;++i){
            const std::uint32_t A=racer(i);
            if(!A||!usable(A))continue;
            push_409ef0();
            std::uint32_t any=0,other=0;
            const float radA=driving::x87_float(driving::x87_abs(X87(f32(A+0xd24)))+X87(f32(model_4866c0(i8(A+0x11))+0x18)));
            for(std::uint32_t j=i+1;j<0x18u;++j){
                const std::uint32_t B=racer(j);
                if(!B||!usable(B))continue;
                const float radB=driving::x87_float(driving::x87_abs(X87(f32(B+0xd24)))+X87(f32(model_4866c0(i8(B+0x11))+0x18)));
                if(dist_40f140(B+0x14,A+0x14)>=X87(radB)+X87(radA))continue;
                {const std::int32_t d=diff_46f990(B+0x5c,A+0x5c);if(d>=0x3c||d<=-3)continue;}
                float ratio=(u32(B+4)&0x20000u)?K(0x62806cu):sdiv(f32(B+0x1c4),sadd(f32(A+0x1c4),f32(B+0x1c4)));
                {const float tb=f32(0x6ac8f0u+(u32(A+0x324)*7u+u32(B+0x324))*4u);if(!jb(tb,ratio))ratio=tb;}
                const std::uint32_t nA=u8(A+0x335);
                const float halfB=smul(radB,K(0x628064u));
                for(std::uint32_t kA=0;kA<nA;++kA){
                    if(gt(ssub(f32(B+0x2dc),halfB),f32(A+0x33c+kA*0x1ecu)))continue;
                    rec_clear_4f6b90(recA);
                    const std::uint32_t nB=u8(B+0x335);
                    const float halfA=smul(radA,K(0x628064u));
                    for(std::uint32_t kB=0;kB<nB;++kB){
                        if(gt(ssub(f32(A+0x2dc),halfA),f32(B+0x33c+kB*0x1ecu)))continue;
                        rec_clear_4f6b90(recB);
                        bool hit=false;
                        if(boxes_4f9000(recB,B,A,kB,kA)){
                            const std::uint32_t dst=B+0x36c+kB*0x1ecu+kA*0xd4u;
                            for(std::uint32_t q=0;q<0xd4;q+=4)p32(dst+q,u32(recB+q));
                            hit=true;
                        }
                        if(boxes_4f9000(recA,A,B,kA,kB)){
                            const std::uint32_t dst=A+0x36c+kA*0x1ecu+kB*0xd4u;
                            for(std::uint32_t q=0;q<0xd4;q+=4)p32(dst+q,u32(recA+q));
                            hit=true;
                        }
                        if(!hit)continue;
                        if(u8(B+4)&1u){if(i16(A+0xb4c)>0)p32(A+0xae8,u32(B));p16(A+0xb4c,0x3c);}
                        if(u8(A+4)&1u){if(i16(B+0xb4c)>0)p32(B+0xae8,u32(A));p16(B+0xb4c,0x3c);}
                        p32(B+0xdd4,u32(A));p32(A+4,u32(A+4)|0x100000u);p32(A+0xdd4,u32(B));
                        if(separate_4f8490(A,B,recA,recB,ratio)){replace_4f69f0(B);p32(B+4,u32(B+4)|0x10000u);}
                        any=1;other=B;
                    }
                }
            }
            if(any==1u&&other){
                const std::uint32_t B=other;
                replace_4f69f0(A);
                p32(A+4,u32(A+4)|0x10000u);
                p32(B+0xdd4,u32(A));p32(A+0xdd4,u32(B));
                const std::uint32_t mode=u32(0x78026cu);
                if(mode==0x10u||mode==3u){
                    const float a2c8=f32(A+0x2c8);
                    if(u32(0x780258u)==4u){
                        if(u8(A+0x10)<u8(B+0x10)){react_4f6fe0(A,B);react_4f6fe0(B,A);}
                        else{react_4f6fe0(B,A);react_4f6fe0(A,B);}
                    }else{
                        if(u32(A)==8u)react_4f6fe0(A,B);else react_4f76e0(A,B);
                        if(u32(B)==8u)react_4f6fe0(B,A);else react_4f76e0(B,A);
                    }
                    if(!jb(K(0x6281f0u),a2c8)){
                        const std::uint32_t a4=u32(A+4),b4=u32(B+4);
                        if((a4&1u)&&(b4&1u)&&((a4&2u)||(b4&2u)))exchange_4f8210(A,B);
                        else exchange_4f7bf0(A,B);
                    }
                }
            }
            pop();
        }
        // restore the boxes saved by the pre-pass
        for(std::uint32_t id=8;id<0x20u;++id){
            if(!live(id))continue;
            const std::uint32_t car=work(id);
            const std::uint32_t n=u8(car+0x335);
            for(std::uint32_t k=0;k<n;++k){
                const std::uint32_t b=car+0x338+k*0x1ecu;
                for(std::uint32_t q=0;q<0x1c;q+=4)p32(b+q,u32(b+0x3d8+q));
                for(std::uint32_t q=0;q<0xc;q+=4)p32(b+0x1c+q,u32(b+0x3f4+q));
                for(std::uint32_t q=0;q<0xc;q+=4)p32(b+0x28+q,u32(b+0x400+q));
            }
        }
    }
    // ---- the remote players' cars (event function 0x3E: init 4A5F10, control 4A60D0) ----
    // The motion tracks 7DD140 + slot * 0x30C (+0 x, +104 z, +208 yaw): +0 samples (0 = no
    // acceleration), +4 frame, +8 value, +C rate, +10 acceleration, +14.. the last rates.
    static std::uint32_t track(std::uint32_t slot,std::uint32_t k){return 0x7dd140u+slot*0x30cu+k*0x104u;}
    // value at frame t: value + rate * dt (+ acc * dt * dt * 0.5 with samples).
    float track_value(std::uint32_t b,std::uint32_t t){
        const float dt=float(std::int32_t(t-u32(b+4)));
        float v=sadd(smul(f32(b+0xc),dt),f32(b+8));
        if(u32(b))v=sadd(smul(smul(smul(f32(b+0x10),dt),dt),K(0x628064u)),v);
        return v;
    }
    // rate at frame t: rate (+ dt * acc with samples).
    float track_rate(std::uint32_t b,std::uint32_t t){
        float v=f32(b+0xc);
        if(u32(b))v=sadd(smul(float(std::int32_t(t-u32(b+4))),f32(b+0x10)),v);
        return v;
    }
    // 455D50(eax = track, ecx = frame, value, xmm0 = rate): a newer sample: the acceleration
    // since the last one is pushed into the history (+14..), +10 becomes its mean.
    void track_sample_455d50(std::uint32_t b,std::uint32_t t,float value,float rate){
        if(std::int32_t(t)<=std::int32_t(u32(b+4)))return;
        const std::int32_t n=std::int32_t(u32(b));
        const float acc=sdiv(ssub(rate,f32(b+0xc)),float(std::int32_t(t-u32(b+4))));
        pf(b+0xc,rate);p32(b+4,t);pf(b+8,value);pf(b+0x10,0.0f);
        for(std::int32_t i=n-1;i>=0;--i){
            const float h=f32(b+0x14+std::uint32_t(i)*4u);
            pf(b+0x18+std::uint32_t(i)*4u,h);
            pf(b+0x10,sadd(h,f32(b+0x10)));
        }
        const float sum=sadd(f32(b+0x10),acc);
        pf(b+0x14,acc);
        pf(b+0x10,sdiv(sum,sadd(float(n),K(0x62806cu))));
        p32(b,std::uint32_t(n+1>=0x3b?0x3b:n+1));
    }
    // 456000(slot, frame): the newest received state of the player (history 7EE240, 64
    // records of 0x24 per slot, every 4th one scanned) not after frame; 0 when none.
    std::uint32_t state_456000(std::uint32_t slot,std::uint32_t t){
        std::uint32_t best=0xffffu,at=0;
        for(std::uint32_t i=3;i<0x40u;i+=4){
            const std::uint32_t r=0x7ee240u+(slot*64u+i)*0x24u;
            const std::uint32_t when=u32(r);
            if(when==0x7fffu)continue;
            const std::int32_t d=std::int32_t(t-when);
            if(d<0||d>=std::int32_t(best))continue;
            best=std::uint32_t(d);at=i;
        }
        if(best==0xffffu)return 0;
        std::uint32_t back=u32(0x7df334u+slot*4u);
        std::uint32_t r=0x7ee240u+(slot*64u-back+at)*0x24u;
        for(unsigned guard=0;;++guard){
            const std::uint32_t cur=r;
            if(back){--back;r+=0x24u;}
            if(u32(cur)!=0x7fffu){r=cur;break;}
            if(guard>64u)throw std::logic_error("race traffic: 456000 history walk without a state");
        }
        p32(0x7df334u+slot*4u,back);
        return r;
    }
    // 4560A0(car) / 4561E0(car): the tracks restart from the newest state (+0 frame, +4 x,
    // +8 z, +C / +10 their rates, +14 / +18 yaw and its rate, +1C bit 0: a restart, +20 the
    // place: kind in bits 30..31, course word in bits 8..23); 4561E0 always restarts and
    // leaves the place alone.
    void restart(std::uint32_t slot,std::uint32_t s,unsigned k){
        const std::uint32_t b=track(slot,k);
        p32(b,0);pf(b+8,f32(s+4u+k*4u));pf(b+0xc,f32(s+0xcu+k*4u));p32(b+4,u32(s));
    }
    void restart_yaw(std::uint32_t slot,std::uint32_t s){
        const std::uint32_t b=track(slot,2);
        p32(b,0);pf(b+8,f32(s+0x14));pf(b+0xc,f32(s+0x18));p32(b+4,u32(s));
    }
    void follow_4560a0(std::uint32_t car){
        const std::uint32_t slot=u32(car+0x1054);
        const std::uint32_t s=state_456000(slot,clock());
        if(!s)return;
        if(u8(s+0x1c)&1u){restart(slot,s,0);restart(slot,s,1);}
        else{
            track_sample_455d50(track(slot,0),u32(s),f32(s+4),f32(s+0xc));
            track_sample_455d50(track(slot,1),u32(s),f32(s+8),f32(s+0x10));
        }
        restart_yaw(slot,s);
        p32(car+0x5c,u32(s+0x20)>>30);
        p16(car+0x262,u16(car+0x260));
        p16(car+0x260,std::uint16_t(u32(s+0x20)>>8));
    }
    void restart_4561e0(std::uint32_t car){
        const std::uint32_t slot=u32(car+0x1054);
        const std::uint32_t s=state_456000(slot,clock());
        if(!s)return;
        restart(slot,s,0);restart(slot,s,1);restart_yaw(slot,s);
    }
    // 455E70(car): the three tracks start still at the car's x / z (yaw 0).
    void tracks_455e70(std::uint32_t car){
        const std::uint32_t slot=u32(car+0x1054);
        std::uint32_t b=track(slot,0);pf(b+8,f32(car+0x14));pf(b+0xc,0.0f);p32(b,0);p32(b+4,0);
        b=track(slot,1);p32(b,0);pf(b+8,f32(car+0x1c));pf(b+0xc,0.0f);p32(b+4,0);
        b=track(slot,2);p32(b,0);pf(b+8,0.0f);pf(b+0xc,0.0f);p32(b+4,0);
    }
    // The orientation matrix +70 from the angle words +2E (y), +2C (x), +30 (z); +162 the
    // heading of the motion (+1FE) relative to it.
    void pose_matrix(std::uint32_t car){
        push_unit();
        rotate_y(word_radians(i16(car+0x2e)));
        rotate_x(word_radians(i16(car+0x2c)));
        rotate_z(word_radians(i16(car+0x30)));
        get_40a0d0(car+0x70);
        pop();
        p16(car+0x162,std::uint16_t(u16(car+0x1fe)-u16(car+0x2e)));
    }
    // The motion angle words +1FC..+200 and the speed +1C4 of the velocity (vx, +24, vz).
    void motion_angles(std::uint32_t car,float vx,float vz){
        Frame L(*this,0x20);
        pf(L(0),ssub(0.0f,vx));pf(L(4),ssub(0.0f,f32(car+0x24)));pf(L(8),ssub(0.0f,vz));
        pf(car+0x1c4,driving::x87_float(len_40f0e0(L(0))));
        angles_449800(L(0x10),L(0));
        p16(car+0x1fc,std::uint16_t(cvtt(smul(f32(L(0x10)),K(0x6282c0u)))));
        p16(car+0x1fe,std::uint16_t(cvtt(smul(f32(L(0x14)),K(0x6282c0u)))));
        p16(car+0x200,std::uint16_t(cvtt(smul(f32(L(0x18)),K(0x6282c0u)))));
    }
    // 457B20(eax = frame, esi = car): no road under the predicted point: the car jumps to the
    // tracks' values at frame (4561E0 restart first).
    void jump_457b20(std::uint32_t car,std::uint32_t t){
        restart_4561e0(car);
        const std::uint32_t slot=u32(car+0x1054);
        pf(car+0x14,track_value(track(slot,0),t));
        pf(car+0x1c,track_value(track(slot,1),t));
        const float vx=track_rate(track(slot,0),t),vz=track_rate(track(slot,1),t);
        pf(car+0x20,vx);pf(car+0x28,vz);
        p16(car+0x2e,std::uint16_t(cvtt(track_value(track(slot,2),t))));
        motion_angles(car,vx,vz);
        pose_matrix(car);
    }
    // 40F080(out, v, len): out = v scaled to len when |v| > [6282A8] (double).
    void set_length_40f080(std::uint32_t o,std::uint32_t v,float len){
        const X87 l=driving::x87_sqrt((x(v)*x(v)+x(v+4)*x(v+4))+x(v+8)*x(v+8));
        double lim;{const std::uint64_t q=(std::uint64_t(u32(0x6282acu))<<32)|u32(0x6282a8u);std::memcpy(&lim,&q,8);}
        if(!(l.v>(long double)lim))return;
        const X87 s=X87(len)/l;
        const float r0=driving::x87_float(s*x(v)),r1=driving::x87_float(s*x(v+4)),r2=driving::x87_float(s*x(v+8));
        pf(o,r0);pf(o+4,r1);pf(o+8,r2);
    }
    // 40EF40(out, a, b): out = a + b.
    void sum_40ef40(std::uint32_t o,std::uint32_t a,std::uint32_t b){
        const float r0=driving::x87_float(x(a)+x(b)),r1=driving::x87_float(x(a+4)+x(b+4)),r2=driving::x87_float(x(a+8)+x(b+8));
        pf(o,r0);pf(o+4,r1);pf(o+8,r2);
    }
    // The length of a correction: (len - cur) * k + cur, at least cur + add; 0 below 0.
    float clamp_step(float len,float cur,float k,float add){
        float lim=sadd(cur,add);
        const float s=sadd(smul(ssub(len,cur),k),cur);
        if(gt(s,lim))lim=s;
        if(gt(0.0f,len))return 0.0f;
        if(gt(len,lim))return lim;
        return len;
    }
    // 457480(edi = car): the progress along the course: the place's course word + the car's
    // position between the two road edges' lines (43E3B0 road info).
    X87 progress_457480(std::uint32_t car){
        Frame L(*this,0xc0);
        const std::uint32_t I=L(0);                                 // road info (0x64)
        if(!call(0x43e3b0u,{I,car+0x5c,0xffffffffu}))return X87(K(0x619a34u));
        const std::uint32_t A=L(0x70),B=L(0x7c),C1=L(0x88),C2=L(0x94),P1=L(0xa0),P2=L(0xac);
        sub_40efa0(A,I+0x30,I+0x24);
        sub_40efa0(B,I+0x48,I+0x3c);
        cross_40eff0(C1,A,I+0x18);
        cross_40eff0(C2,I+0x18,B);
        sub_40efa0(P1,car+0x14,I+0x24);
        sub_40efa0(P2,car+0x14,I+0x3c);
        const float l1=driving::x87_float(len_40f0e0(C1)),l2=driving::x87_float(len_40f0e0(C2));
        const float d1=driving::x87_float(dot_40efd0(P1,C1)/X87(l1));
        const X87 d2=dot_40efd0(P2,C2)/X87(l2);
        return d2/(X87(d1)+d2)+X87(std::int32_t(u16(car+0x260)));
    }
    // 457770: [799D18]'s progress to 7DE480 + [7DD138] * 0x6C.
    void progress_457770(){
        const float p=driving::x87_float(progress_457480(u32(0x799d18u)));
        pf(0x7de480u+std::uint32_t(u8(0x7dd138u))*0x6cu,p);
    }
    // 43F730(point, place out, kind): the course place under the point (kind 4: type 1):
    // type, course word (780228 table), branch 0 / 0x64 / 0x65 / -1 and the 44BDD0 / 450560
    // area; 0 when the point is off the road.
    std::uint32_t place_43f730(std::uint32_t pt,std::uint32_t out,std::uint32_t kind){
        Frame L(*this,0x20);
        const std::uint32_t V=L(0),poly=L(0xc),special=L(0x10),cls=L(0x14);
        p32(V,u32(pt));p32(V+4,u32(pt+4));p32(V+8,u32(pt+8));
        p32(out,0);p32(out+4,0);p32(out+8,0);p32(out+0xc,0);
        p32(poly,kind);   // the argument slot the original reuses for the polygon
        const std::uint32_t type=call(0x43ed20u,{kind==4u?1u:kind,0x100u,V,poly,special,cls});
        if(u32(cls)==1u)return 0;
        p32(out,type);
        const std::uint32_t pg=u32(poly);
        p16(out+8,pg==0xffffffffu?0u:u16(u32(0x780228u+type*4u)+pg*2u));
        if(!type){p32(out+0xc,call(0x44bdd0u,{}));p32(out+4,0);return 1;}
        const std::uint32_t area=call(0x450560u,{call(0x44be00u,{})});
        p32(out+0xc,area);
        const std::uint32_t b=call(0x451350u,{call(0x44c940u,{area})});
        p32(out+4,b==0u?0x64u:(b==1u?0x65u:0xffffffffu));
        return 1;
    }
    // 44C190(out, flag): the middle of the start line (the course word 0 road info edges
    // +3C / +48), shifted by the 680AE8 start offset of area 44C2C0 with flag (49F3A0).
    void start_point_44c190(std::uint32_t out,std::uint32_t flag){
        Frame L(*this,0x80);
        const std::uint32_t P=L(0),I=L(0x10);
        call(0x43e3b0u,{I,P,0xffffffffu});
        const float h=K(0x628064u);
        pf(out,smul(sadd(f32(I+0x48),f32(I+0x3c)),h));
        pf(out+4,smul(sadd(f32(I+0x4c),f32(I+0x40)),h));
        pf(out+8,smul(sadd(f32(I+0x50),f32(I+0x44)),h));
        const std::uint32_t a=u32(0x7d30acu);   // 44C2C0: mov eax,[7D30AC]
        if(flag){
            pf(out,sadd(f32(0x680ae8u+a*16u),f32(out)));
            pf(out+4,sadd(f32(0x680aecu+a*16u),f32(out+4)));
        }
    }
    // 457DB0(car): the remote car follows its tracks: the predicted point one frame back;
    // the velocity is corrected along / across the road heading towards it within limits,
    // then the yaw from the yaw track, the angles, the place and the progress.
    void follow_457db0(std::uint32_t car){
        Frame L(*this,0x120);
        const std::uint32_t t=clock()-1u;
        follow_4560a0(car);
        const std::uint32_t slot=u32(car+0x1054);
        pf(0x7de480u+slot*0x6cu,0.0f);
        const std::uint32_t P=L(0),V=L(0x10),place=L(0x20),D=L(0x30),H=L(0x40),Vh=L(0x50),Vp=L(0x60),Dh=L(0x70),Dp=L(0x80),N=L(0x90),W=L(0xa0),I=L(0xb0);
        pf(P,track_value(track(slot,0),t));pf(P+4,f32(car+0x18));pf(P+8,track_value(track(slot,1),t));
        pf(V,track_rate(track(slot,0),t));pf(V+4,f32(car+0x24));pf(V+8,track_rate(track(slot,1),t));
        auto lost=[&]{jump_457b20(car,t);p32(car+8,u32(car+8)|1u);};
        if(!place_43f730(P,place,u32(car+0x5c))){lost();return;}
        p32(W,u32(car+0x20));p32(W+4,u32(car+0x24));p32(W+8,u32(car+0x28));
        sub_40efa0(D,P,car+0x14);
        if(!call(0x43e3b0u,{I,place,0xffffffffu})){lost();return;}
        {const X87 a=X87(f32(I+0x14));   // fsin / fcos, fchs
            pf(H,-driving::x87_float(driving::x87_sin(a)));pf(H+4,0.0f);pf(H+8,-driving::x87_float(driving::x87_cos(a)));}
        scale_40f050(Vh,H,driving::x87_float(dot_40efd0(V,H)));
        sub_40efa0(Vp,V,Vh);
        scale_40f050(Dh,H,driving::x87_float(dot_40efd0(D,H)));
        sub_40efa0(Dp,D,Dh);
        {const float lv=driving::x87_float(len_40f0e0(Vh));
            const float ld=driving::x87_float(unit_40eeb0(Dh));
            scale_40f050(Dh,Dh,clamp_step(ld,lv,K(0x62813cu),K(0x6280ecu)));}
        {const float lv=driving::x87_float(len_40f0e0(Vp));
            const float ld=driving::x87_float(unit_40eeb0(Dp));
            scale_40f050(Dp,Dp,clamp_step(ld,lv,K(0x6280e8u),K(0x6281c0u)));}
        sum_40ef40(N,Dh,Dp);
        {const float ln=driving::x87_float(len_40f0e0(N)),lw=driving::x87_float(len_40f0e0(W));
            if((X87(ln)-X87(lw)).v>X87(K(0x6280ecu)).v)set_length_40f080(N,N,sadd(lw,K(0x6280ecu)));}
        p32(car+0x20,u32(N));p32(car+0x24,u32(N+4));p32(car+0x28,u32(N+8));
        add_40ef10(car+0x14,N);
        p16(car+0x2e,std::uint16_t(cvtt(track_value(track(slot,2),t))));
        motion_angles(car,f32(car+0x20),f32(car+0x28));
        if(!place_43f730(car+0x14,car+0x5c,u32(car+0x5c))){lost();return;}
        (void)settle_46e4b0(car);
        pose_matrix(car);
        p16(car+0x262,u16(car+0x260));
        const std::uint32_t base=u32(0x780208u+u32(car+0x5c)*4u);
        p16(car+0x25e,std::uint16_t(base));
        p16(car+0x260,std::uint16_t(base+u32(place+8)));
        p32(car+8,u32(car+8)&~1u);
        pf(0x7de480u+slot*0x6cu,driving::x87_float(progress_457480(car)));
    }
    // 4A5F10(car): event function 0x3E init: the car (4A5830, slot kept) on its grid slot
    // (5C2500[456E00(slot)] when 4963B0; mirrored on presets 0 / 2) or the 5C2560 start,
    // put on the start line (44C190) and the ground; the tracks start there.
    void lan_car_init_4a5f10(std::uint32_t car){
        const std::uint32_t slot=u32(car+0x1054);
        call(0x4a5830u,{car});
        p32(car+0x1054,slot);
        call(0x4f6e40u,{car});
        pf(car+0xdd8,K(0x5b005cu));pf(car+0xddc,K(0x5b005cu));
        Frame L(*this,0x10);
        const std::uint32_t pos=car+0x14;
        const bool lan=u32(0x780258u)==4u;
        if(lan){
            std::uint32_t g=0x5c2500u;
            if(call(0x4963b0u,{}))g=0x5c2500u+u32(0x7de478u+u32(car+0x1054)*0x6cu)*12u;
            p32(pos,u32(g));p32(pos+4,u32(g+4));p32(pos+8,u32(g+8));
            const std::uint32_t pr=u32(0x78024cu);
            if(pr==0u||pr==2u)pf(pos,ssub(0.0f,f32(pos)));
        }else{p32(pos,u32(0x5c2560u));p32(pos+4,u32(0x5c2564u));p32(pos+8,u32(0x5c2568u));}
        start_point_44c190(L(0),0);
        add_40ef10(pos,L(0));
        call(0x43eb60u,{0x100u,pos,0,0,0});
        p32(car+0xd28,u32(pos));p32(car+0xd2c,u32(pos+4));p32(car+0xd30,u32(pos+8));
        pf(car+0x178,0.0f);pf(car+0x1c4,0.0f);
        if(lan){
            (void)place_43f730(pos,car+0x5c,u32(car+0x5c));
            p16(car+0x260,u16(car+0x64));
        }else call(0x4a2ee0u,{car});
        tracks_455e70(car);
    }
    // 4A60D0(car): event function 0x3E control: the previous pose saved (+16C.., +184..,
    // +1040 / +1034), 457DB0, the wheel spin (+40 / +44 by speed / the model's radius), then
    // the common car leaves 4A4010 / 504E70 / 4A2400 / 4A2650 / 4A2910 / 4A2EE0.
    void lan_car_control_4a60d0(std::uint32_t car){
        for(std::uint32_t q=0;q<0xc;q+=4)p32(car+0x16c+q,u32(car+0x14+q));
        p16(car+0x17c,u16(car+0x2c));p16(car+0x17e,u16(car+0x2e));p16(car+0x180,u16(car+0x30));
        for(std::uint32_t q=0;q<0x10;q+=4)p32(car+0x184+q,u32(car+0x5c+q));
        for(std::uint32_t q=0;q<0xc;q+=4)p32(car+0x1040+q,u32(car+0x2d8+q));
        for(std::uint32_t q=0;q<0xc;q+=4)p32(car+0x1034+q,u32(car+0x2e4+q));
        follow_457db0(car);
        const std::uint32_t md=model_4866c0(i8(car+0x11));
        for(std::uint32_t o:{0x40u,0x44u}){
            const float s=smul(sdiv(f32(car+0x1c4),f32(md+8)),K(0x6282c0u));
            p16(car+o,std::uint16_t(u16(car+o)+std::uint16_t(cvtt(s))));
        }
        call(0x4a4010u,{car});
        call(0x504e70u,{car});
        call(0x4a2400u,{car});call(0x4a2650u,{car});call(0x4a2910u,{car});call(0x4a2ee0u,{car});
    }
    // ---- LAN traffic (records 7E16F8 + (event - 8) * 0x85C) --------------------------
    // 455150(car): the car's record +E word / +0 / owner bits +4 >> 3, parked (+C5C bit 0)
    // unless +4 bit 0, the +B58 state 0x12 when +4 bit 2.
    void lan_record_455150(std::uint32_t car){
        const std::uint32_t r=(u32(car)-8u)*0x85cu+0x7e16f8u;
        p16(car+0xdd0,u16(r+0xe));
        p32(car+0xdcc,u32(r));
        p8(car+0xdc8,(u32(r+4)>>3)&0xfu);
        if(u8(r+4)&1u)p32(car+0xc5c,u32(car+0xc5c)&0xfffffffeu);
        else p32(car+0xc5c,u32(car+0xc5c)|1u);
        if(u8(r+4)&4u)p32(car+0xb58,0x12);
    }
    // 46EB80(car): a reopened LAN traffic car reset in place (440B30: its event id, known
    // here): speed from its velocity, the model words, the course ground point +C70.
    void lan_reset_46eb80(std::uint32_t car,std::uint32_t id){
        Frame f(*this,0x10);
        call(0x4874f0u,{car,1u});
        p32(car,id);                                                                   // 440B30
        p16(car+0x25e,std::uint16_t(u16(car+0x260)-u16(car+0x64)));
        for(std::uint32_t k=0;k<12u;k+=4u)p32(f(k),u32(car+0x20u+k));
        const X87 v=len_40f0e0(f(0));
        pf(car+0x1c4,driving::x87_float(v));
        p32(car+0x1f4,ftol(v*X87(K(0x5a460cu))));
        p32(car+0x31c,u32(model_4866c0(i8(car+0x11))+0xc));
        p32(car+0x320,u32(model_4866c0(i8(car+0x11))+0x10));
        p32(car+0x324,u32(model_4866c0(i8(car+0x11))+0x14));
        p32(car+0xb14,0);p16(car+0xb50,0);p32(car+0xc5c,2);
        p16(car+0x160,u16(car+0x2e));p16(car+0x162,0);
        if(u32(u32(u32(model_4866c0(i8(car+0x11))))+4)){
            push_unit();translate_40a2d0(car+0x14);
            rotate_y(word_radians(i16(car+0x2e)));rotate_x(word_radians(i16(car+0x2c)));rotate_z(word_radians(i16(car+0x30)));
            driving::pc_matrix_translate_vector(c.matrices,{0.0f,0.0f,12.0f});   // 40A290(0, 0, 12)
            put_vec(car+0xc70,driving::pc_matrix_translation(c.matrices));
            pop();
            call(0x43eb60u,{0x100u,car+0xc70,0,0,0});
        }
        p8(car+0xd20,0);
        call(0x4f6e40u,{car});
    }

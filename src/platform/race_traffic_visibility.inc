    // ==== Course object control (race manager 4F0E40 per frame) =================================
    // The 84CEA0 records (0x28-byte object record, +28 state 1 opened / 2 set up, +2C function,
    // +30 the [84D8F0] frame) written by 4F0BD0 for the gate events 0x134..0x15B, and 84D6D8.. for
    // 0x15C..0x165, are the race manager's 4F0DD0 block (mapped by the runtime).
    // 4F07F0(id, record, frame): the event's work from a record (no allocation; 4F0660's layout)
    // in the course frame 44BEA0 / 44BEC0, then visible (+54 bit 0).
    void object_record_4f07f0(std::uint32_t id,std::uint32_t rec,std::uint32_t frame){
        const std::uint32_t w=u32(0x799b38u+id*0x3cu);
        if(!w)return;
        p32(w+0x58,frame);
        copy3(w+0x40,rec);copy3(w+0x68,rec+0xc);
        p32(w+0x5c,u32(rec+0x18));
        p32(w+0x64,u32(0x5da0a8u+u32(rec+0x18)*8u));
        p32(w+0x4c,u32(0x5da0acu+u32(rec+0x18)*8u));
        p32(w+0x60,0xffffffffu);
        pf(w+0x50,K(0x62806cu));
        if(u32(rec+0x20)==2u)p32(w+0x7c,u32(rec+0x24));
        push_load(frame?call(0x44bec0u,{}):0x7d2da0u);                  // 44BEC0 / 44BEA0 (mov eax,7D2DA0)
        translate_40a2d0(w+0x40);rotate_y(f32(w+0x6c));rotate_x(f32(w+0x68));rotate_z(f32(w+0x70));
        translation_40a250(w+0x40);
        euler_cur_449640(w+0x68);
        identity_40a020();
        translate_40a2d0(w+0x40);rotate_y(f32(w+0x6c));rotate_x(f32(w+0x68));rotate_z(f32(w+0x70));
        get_40a0d0(w);
        pop();
        p32(w+0x54,u32(w+0x54)|1u);
    }
    // 46F040(a, b, c): which side of the line b -> a (XZ) c lies on, as a signed half distance
    // (+-100000 when the three are nearly aligned).
    X87 side_46f040(std::uint32_t a,std::uint32_t b,std::uint32_t c){
        const float cax=ssub(f32(c),f32(a)),cbz=ssub(f32(c+8),f32(b+8)),caz=ssub(f32(c+8),f32(a+8));
        const float abx=ssub(f32(a),f32(b)),abz=ssub(f32(a+8),f32(b+8)),cbx=ssub(f32(c),f32(b));
        const float cross=driving::x87_float(X87(cbx)*X87(abz)-X87(cbz)*X87(abx));
        const float lcb=driving::x87_float(driving::x87_sqrt(X87(sadd(smul(cbz,cbz),smul(cbx,cbx)))));
        const float s=driving::x87_float(X87(cross)/(driving::x87_sqrt(X87(sadd(smul(abz,abz),smul(abx,abx))))*X87(lcb)));
        if(X87(K(0x599434u))>X87(std::fabs(s)))return X87(s>0.0f?K(0x5b436cu):K(0x5b4368u));
        return driving::x87_sqrt(X87(sadd(smul(caz,caz),smul(cax,cax))))/X87(s)*X87(K(0x628064u));
    }
    // 4F0920: gate events 0x134..0x15B: a record opened (state 1) on a live event sets the work up
    // (4F07F0) and its function's init (0x4D 4A9BD0, 0x5D 4ABA80, 0x61 a hoop across the road:
    // facing the road's side vector, tilted down by -0.5 - rand / 32768 (seed 417F70), reversed by
    // the 46F040 side, +B0 = facing * 3, moved 100 back, 4AACD0); a set-up record whose event
    // closed returns to 0.
    void objects_reinit_4f0920(){
        for(std::uint32_t k=0;k<0x28u;++k){
            const std::uint32_t st=0x84cec8u+k*0x34u,id=0x134u+k;
            const bool live=(u8(0x79fb48u+id)&3u)==2u;
            if(u32(st)==2u){if(!live)p32(st,0);continue;}
            if(u32(st)!=1u||!live)continue;
            const std::uint32_t rec=st-0x28u;
            object_record_4f07f0(id,rec,u32(st+8));
            const std::uint32_t fn=u32(rec+0x2c),w=u32(0x799b38u+id*0x3cu);
            if(fn==0x4du)anim_init_4a9bd0(w);
            else if(fn==0x5du)movable_init_4aba80(w);
            else if(fn==0x61u){
                Frame F(*this,0xec);
                const std::uint32_t D=F(0x10),key=F(0x24),o1=F(0x34),o2=F(0x98);
                copy3(w+0xa4,w+0x40);
                p32(key,0);p32(key+4,0);
                p16(key+8,std::uint16_t(section_43f370(w+0x40)));
                call(0x43e570u,{o1,key,0xffffffffu});
                p16(key+8,u16(key+8)+1u);
                call(0x43e570u,{o2,key,0xffffffffu});
                sub_40efa0(D,o1+0x24,o1+0x30);
                push_load(0x7d2da0u);vector_40a820(D,D);pop();   // 44BEA0
                pf(D+4,0.0f);
                (void)unit_40eeb0(D);
                pf(D+4,K(0x6281b8u));
                call(0x580f33u,{call(0x417f70u,{})});
                const std::int32_t r=std::int32_t(call(0x580f40u,{}));
                pf(D+4,ssub(f32(D+4),smul(float(r),K(0x62827cu))));
                const float side=driving::x87_float(side_46f040(o1+0x3c,o1+0x24,o2+0x24));
                bool flip=side>0.0f;
                if(!flip&&side==0.0f)flip=std::int32_t(call(0x580f40u,{}))<0x3fff;
                if(flip){pf(D,smul(f32(D),K(0x6280c4u)));pf(D+8,smul(f32(D+8),K(0x6280c4u)));}
                (void)unit_40eeb0(D);
                scale_40f050(w+0xb0,D,3.0f);
                scale_40f050(D,D,-100.0f);
                add_40ef10(w+0x40,D);
                hoop_init_4aacd0(w);
            }
            p32(st,2);
        }
    }
    // 4F0C30: the same for events 0x15C..0x165 (records 84D6D8.., function 0x4D only).
    void objects_reinit_4f0c30(){
        for(std::uint32_t k=0;k<0xau;++k){
            const std::uint32_t st=0x84d700u+k*0x34u,id=0x15cu+k;
            const bool live=(u8(0x79fb48u+id)&3u)==2u;
            if(u32(st)==2u){if(!live)p32(st,0);continue;}
            if(u32(st)!=1u||!live)continue;
            const std::uint32_t rec=st-0x28u;
            object_record_4f07f0(id,rec,u32(st+8));
            if(u32(rec+0x2c)==0x4du)anim_init_4a9bd0(u32(0x799b38u+id*0x3cu));
            p32(st,2);
        }
    }
    // 4F0E40: object set-ups, the 84D8E0 frame, then each live object (5DA050 lanes 0x20.., 0x11A..,
    // 0x15C.., then the gates 0x134..) is visible (+54 bit 0) when its sphere (+40, radius +50 * +4C)
    // projected by 449E70 through the camera (+140, focal +B4) lies within the depth range
    // +BC..+C0 and the 640 x 480 screen.
    void objects_4f0e40(){
        Frame F(*this,0x28);
        objects_reinit_4f0920();
        objects_reinit_4f0c30();
        const std::uint32_t cam=u32(0x79f574u);
        p32(0x84d8e0u,u32(0x84d8e0u)+1u);
        push_load(cam+0x140);
        auto check=[&](std::uint32_t id){
            if((u8(0x79fb48u+id)&3u)!=2u)return;
            const std::uint32_t w=u32(0x799b38u+id*0x3cu);
            const std::uint32_t P=F(8),S=F(0x18);
            copy3(P,w+0x40);
            const float r=smul(f32(w+0x50),f32(w+0x4c));pf(P+0xc,r);
            // 449E70(focal, P, S): S = (w x, w y, z, |r w|), w = focal / -z (0 within 1e-7 of z = 0)
            const auto t=driving::pc_matrix_point(c.matrices,vec(P));
            float sx=0.0f,sy=0.0f,sz=0.0f,ww=0.0f;
            if(!(K(0x6281f0u)>std::fabs(t.z))){ww=sdiv(f32(cam+0xb4),ssub(0.0f,t.z));sx=smul(ww,t.x);sy=smul(t.y,ww);sz=t.z;}
            const float sr=std::fabs(smul(r,ww));
            pf(S,sx);pf(S+4,sy);pf(S+8,sz);pf(S+0xc,sr);
            bool on=true;
            if(gt(f32(cam+0xbc),ssub(r,sz)))on=false;
            else if(gt(ssub(ssub(0.0f,sz),r),f32(cam+0xc0)))on=false;
            else if(gt(K(0x5da2d4u),sadd(sx,sr)))on=false;
            else if(gt(ssub(sx,sr),K(0x6281c8u)))on=false;
            else if(gt(K(0x5c6fc8u),sadd(sy,sr)))on=false;
            else if(gt(ssub(sy,sr),K(0x6281ccu)))on=false;
            p32(w+0x54,on?u32(w+0x54)|1u:u32(w+0x54)&~1u);
        };
        for(std::uint32_t g=0x5da050u;g<0x5da068u;g+=8u){
            const std::int32_t first=i32(g),end=first+i32(g+4);
            for(std::int32_t id=first;id<end;++id)check(std::uint32_t(id));
        }
        for(std::uint32_t id=0x134u;id<0x15cu;++id)check(id);
        pop();
    }

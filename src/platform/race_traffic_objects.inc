    // ==== course objects (OSO): event functions 0x3D..0x42, 0x4A..0x4F, 0x5C..0x65 =======================
    // Object works: 4F0660 builds them from the AREA object records; the rigid body of the
    // movable objects lives at work +0x80 (516E10 layout: +0 active, +10 matrix, +50 angular
    // velocity, +5C velocity, +68 torque, +74 force, +80 angular momentum, +8C momentum,
    // +98 mass, +9C 1/mass, +A0 inertia, +AC 1/inertia, +B8 / +BC restitution / friction,
    // +C0 / +CC impulses, +D8 centre of mass; its contact shape at work +0x170 / +0x1F0).
    // ---- vector leaves ----
    // 449580(a, b, t): wrap(a) + wrap(wrap(b) - wrap(a)) * t (x87 result).
    X87 lerp_449580(float a,float b,float t){
        a=wrap_449470(a);b=wrap_449470(b);const float d=wrap_449470(ssub(b,a));
        return X87(d)*X87(t)+X87(a);
    }
    // 40F480(out, a, b, t): 449580 per component.
    void lerp_40f480(std::uint32_t o,std::uint32_t a,std::uint32_t b,float t){
        const float r0=driving::x87_float(lerp_449580(f32(a),f32(b),t));pf(o,r0);
        const float r1=driving::x87_float(lerp_449580(f32(a+4),f32(b+4),t));pf(o+4,r1);
        const float r2=driving::x87_float(lerp_449580(f32(a+8),f32(b+8),t));pf(o+8,r2);
    }
    void inv_point_40a840(std::uint32_t o,std::uint32_t v){put_vec(o,driving::pc_matrix_inverse_point(c.matrices,vec(v)));}
    void inv_vector_40a8a0(std::uint32_t o,std::uint32_t v){put_vec(o,driving::pc_matrix_inverse_vector(c.matrices,vec(v)));}
    void points_40a7f0(std::uint32_t o,std::uint32_t i,std::int32_t n){for(std::int32_t k=0;k<n;++k)point_40a7d0(o+std::uint32_t(k)*12u,i+std::uint32_t(k)*12u);}
    void rotate_axis_40a550(std::uint32_t axis,float r){driving::pc_matrix_rotate_axis(c.matrices,vec(axis),r);}
    X87 dot2_40f110(std::uint32_t v){return (x(v)*x(v)+x(v+4)*x(v+4))+x(v+8)*x(v+8);}
    // 519BD0(out, a, b, t): a + (b - a) * t (SSE).
    void lerp_519bd0(std::uint32_t o,std::uint32_t a,std::uint32_t b,float t){
        for(std::uint32_t k=0;k<12;k+=4)pf(o+k,sadd(smul(ssub(f32(b+k),f32(a+k)),t),f32(a+k)));
    }
    // ---- rigid body (516E10 family) ----
    // 516F20(rb, force, point): force += f; torque += (inv_point(p) + centre y) x inv_vector(f).
    void force_516f20(std::uint32_t rb,std::uint32_t f,std::uint32_t p){
        add_40ef10(rb+0x74,f);
        Frame L(*this,0x30);
        push_load(rb+0x10);
        inv_point_40a840(L(4),p);
        pf(L(8),sadd(f32(rb+0xdc),f32(L(8))));
        inv_vector_40a8a0(L(0x10),f);
        cross_40eff0(L(0x1c),L(4),L(0x10));
        add_40ef10(rb+0x68,L(0x1c));
        pop();
    }
    // 516FA0(rb, force, vector, point): force += f; torque += inv_point(p) x inv_vector(v).
    void force_516fa0(std::uint32_t rb,std::uint32_t f,std::uint32_t v,std::uint32_t p){
        add_40ef10(rb+0x74,f);
        Frame L(*this,0x30);
        push_load(rb+0x10);
        inv_point_40a840(L(0x10),p);
        inv_vector_40a8a0(L(4),v);
        cross_40eff0(L(0x1c),L(0x10),L(4));
        add_40ef10(rb+0x68,L(0x1c));
        pop();
    }
    // 5170D0(rb): one integration step (dt [7162C4]) of an active body.
    void integrate_5170d0(std::uint32_t rb){
        if(!u32(rb))return;
        const float dt=K(0x7162c4u);
        float v0=sadd(smul(dt,sadd(smul(f32(rb+0x74),f32(rb+0x9c)),f32(rb+0xc0))),f32(rb+0x5c));pf(rb+0x5c,v0);
        pf(rb+0x60,sadd(smul(dt,sadd(smul(f32(rb+0x78),f32(rb+0x9c)),f32(rb+0xc4))),f32(rb+0x60)));
        pf(rb+0x64,sadd(smul(dt,sadd(smul(f32(rb+0x7c),f32(rb+0x9c)),f32(rb+0xc8))),f32(rb+0x64)));
        pf(rb+0x40,sadd(smul(dt,v0),f32(rb+0x40)));
        pf(rb+0x44,sadd(smul(f32(rb+0x60),dt),f32(rb+0x44)));
        pf(rb+0x48,sadd(smul(f32(rb+0x64),dt),f32(rb+0x48)));
        const float t1=smul(f32(rb+0xb0),f32(rb+0x6c)),t2=smul(f32(rb+0xb4),f32(rb+0x70));
        pf(rb+0x8c,smul(f32(rb+0x98),v0));
        pf(rb+0x90,smul(f32(rb+0x98),f32(rb+0x60)));
        pf(rb+0x94,smul(f32(rb+0x64),f32(rb+0x98)));
        const float t0=smul(f32(rb+0xac),f32(rb+0x68));
        pf(rb+0x50,sadd(smul(dt,sadd(t0,f32(rb+0xcc))),f32(rb+0x50)));
        pf(rb+0x54,sadd(smul(dt,sadd(t1,f32(rb+0xd0))),f32(rb+0x54)));
        pf(rb+0x58,sadd(smul(dt,sadd(t2,f32(rb+0xd4))),f32(rb+0x58)));
        const float len=driving::x87_float(len_40f0e0(rb+0x50));
        if(X87(len)>X87(K(0x5e0ab8u))){
            Frame L(*this,0x10);
            const float inv=sdiv(K(0x62806cu),len);
            pf(L(0),smul(inv,f32(rb+0x50)));pf(L(4),smul(f32(rb+0x54),inv));pf(L(8),smul(f32(rb+0x58),inv));
            push_load(rb+0x10);
            translate3_40a290(ssub(0.0f,f32(rb+0xd8)),ssub(0.0f,f32(rb+0xdc)),ssub(0.0f,f32(rb+0xe0)));
            rotate_axis_40a550(L(0),smul(dt,len));
            translate_40a2d0(rb+0xd8);
            get_40a0d0(rb+0x10);
            pop();
        }
        pf(rb+0x80,smul(f32(rb+0xa0),f32(rb+0x50)));
        pf(rb+0x84,smul(f32(rb+0xa4),f32(rb+0x54)));
        pf(rb+0x88,smul(f32(rb+0xa8),f32(rb+0x58)));
        for(std::uint32_t o:{0xc0u,0xccu,0x74u,0x68u})for(std::uint32_t k=0;k<12;k+=4)p32(rb+o+k,0);
    }
    // 517770 / 5178F0(rb, plane {n, p0, p1}, shape {n, points[], world points +13C}, &depth):
    // bit mask of the shape points behind the plane (n . (q - p0) < 0) and the deepest
    // n . (q - p1); 517770 also pushes the body out along n by that depth.
    std::uint32_t contact_517770(std::uint32_t rb,std::uint32_t plane,std::uint32_t shape,std::uint32_t out,bool push_out){
        push_load(rb+0x10);
        const std::int32_t n=i32(shape);
        points_40a7f0(shape+0x13c,shape+4,n);
        std::uint32_t mask=0,bit=1;float mn=0.0f;
        Frame L(*this,0x20);
        for(std::int32_t i=0;i<n;++i){
            const std::uint32_t p=shape+0x13c+std::uint32_t(i)*12u;
            for(std::uint32_t k=0;k<12;k+=4){pf(L(k),ssub(f32(p+k),f32(plane+0xc+k)));pf(L(0x10+k),ssub(f32(p+k),f32(plane+0x18+k)));}
            const float d1=driving::x87_float(dot_40efd0(L(0),plane));
            const float d2=driving::x87_float(dot_40efd0(L(0x10),plane));
            if(X87(0.0f)>X87(d1)){mask|=bit;if(gt(mn,d2))mn=d2;}
            bit<<=1;
        }
        const float v=std::fabs(mn);
        if(push_out){
            pf(rb+0x40,driving::x87_float(X87(v)*x(plane)+x(rb+0x40)));
            pf(rb+0x44,driving::x87_float(X87(v)*x(plane+4)+x(rb+0x44)));
            pf(rb+0x48,driving::x87_float(X87(v)*x(plane+8)+x(rb+0x48)));
        }
        pop();
        pf(out,v);
        return mask;
    }
    // 517A40(rb, plane, shape, mask, out): the mask points weighted by their depth
    // inv_vector(n) . (q + centre y) below [599430]: out = weighted mean of their world
    // points; 0 when none.
    std::uint32_t contact_point_517a40(std::uint32_t rb,std::uint32_t plane,std::uint32_t shape,std::uint32_t mask,std::uint32_t out){
        Frame L(*this,0x100);
        const std::uint32_t Ln=L(0x24),Lp=L(0x18),W=L(0x30),I=L(0x98);
        push_load(rb+0x10);
        inv_vector_40a8a0(Ln,plane);
        pop();
        const std::int32_t n=i32(shape);
        float total=0.0f;std::int32_t cnt=0;
        mask<<=1;
        for(std::int32_t i=0;i<n;++i){
            mask>>=1;
            if(!(mask&1u))continue;
            const std::uint32_t q=shape+4+std::uint32_t(i)*12u;
            pf(Lp,f32(q));pf(Lp+4,sadd(f32(rb+0xdc),f32(q+4)));pf(Lp+8,f32(q+8));
            const X87 d=dot_40efd0(Ln,Lp);
            if(!(X87(K(0x599430u))>d))continue;
            const float df=driving::x87_float(d);
            p32(I+std::uint32_t(cnt)*4u,std::uint32_t(i));pf(W+std::uint32_t(cnt)*4u,df);
            total=sadd(df,total);++cnt;
        }
        p32(out,0);p32(out+4,0);p32(out+8,0);
        if(!cnt)return 0;
        for(std::int32_t k=0;k<cnt;++k){
            const std::uint32_t j=u32(I+std::uint32_t(k)*4u),p=shape+0x13c+j*12u;const float w=f32(W+std::uint32_t(k)*4u);
            pf(out,sadd(smul(f32(p),w),f32(out)));
            pf(out+4,sadd(smul(f32(p+4),w),f32(out+4)));
            pf(out+8,sadd(smul(f32(p+8),w),f32(out+8)));
        }
        const float inv=sdiv(K(0x62806cu),total);
        pf(out,smul(f32(out),inv));pf(out+4,smul(f32(out+4),inv));pf(out+8,smul(f32(out+8),inv));
        return 1;
    }
    // 517D90(rb, normal, point, out): the contact impulse at point along normal (restitution
    // +B8, friction +BC).
    void impulse_517d90(std::uint32_t rb,std::uint32_t nrm,std::uint32_t point,std::uint32_t out){
        Frame L(*this,0x40);
        const std::uint32_t R=L(0x28),V=L(0x10),T=L(0x1c);
        push_load(rb+0x10);
        inv_point_40a840(R,point);
        pf(R+4,sadd(f32(rb+0xdc),f32(R+4)));
        const float r=driving::x87_float(len_40f0e0(R));
        const float lm=driving::x87_float(len_40f0e0(rb+0x80));
        if(X87(K(0x5a29e0u))>X87(r)||gt(K(0x5a29e0u),lm)){p32(V,0);p32(V+4,0);p32(V+8,0);}
        else{
            const float s=smul(sdiv(lm,r),K(0x628118u));
            cross_40eff0(V,rb+0x80,R);
            normalize_40ef00(V);
            pf(V,smul(s,f32(V)));pf(V+4,smul(s,f32(V+4)));pf(V+8,smul(s,f32(V+8)));
            vector_40a820(V,V);
        }
        pop();
        add_40ef10(V,rb+0x8c);
        const float k=sdiv(K(0x628118u),K(0x7162c4u));
        pf(V,smul(k,f32(V)));pf(V+4,smul(k,f32(V+4)));pf(V+8,smul(k,f32(V+8)));
        const float d=driving::x87_float(-dot_40efd0(nrm,V));
        pf(T,ssub(0.0f,sadd(smul(f32(nrm),d),f32(V))));
        pf(T+4,ssub(0.0f,sadd(smul(f32(nrm+4),d),f32(V+4))));
        pf(T+8,ssub(0.0f,sadd(smul(d,f32(nrm+8)),f32(V+8))));
        const float tl=driving::x87_float(len_40f0e0(T));
        const X87 fx=X87(std::fabs(d))*x(rb+0xbc);const float f=driving::x87_float(fx);
        float t0=f32(T),t1=f32(T+4),t2=f32(T+8);
        if(X87(tl)>fx){const float s=sdiv(f,tl);t0=smul(s,t0);t1=smul(s,t1);t2=smul(s,t2);}
        const float e=smul(sadd(f32(rb+0xb8),K(0x62806cu)),d);
        pf(out,sadd(smul(f32(nrm),e),t0));pf(out+4,sadd(smul(f32(nrm+4),e),t1));pf(out+8,sadd(smul(e,f32(nrm+8)),t2));
    }
    // ---- object work set-up ----
    void copy3(std::uint32_t o,std::uint32_t i){p32(o,u32(i));p32(o+4,u32(i+4));p32(o+8,u32(i+8));}
    std::uint32_t current_event_440b80(){return call(0x440b80u,{});}
    void change_ctrl_440b90(std::uint32_t cb){call(0x440b90u,{cb});}
    void euler_cur_449640(std::uint32_t o){Frame f(*this,0x40);get_40a0d0(f(0));euler_449640(o,f(0));}
    // 4F0660(): the work of the current object event from its AREA record (lane 0: events
    // 0x20.., lane 1: 0x11A..; record 0x28 bytes {position, angles, kind +18, type +20, +24}):
    // 440A60(size [5DA2A0 + type * 4], heap 7 / 2), zeroed, position / angles (world, through
    // the lane's course matrix unless kind 3 / 0x3E), matrix, kind, model [5DA0A8 + kind * 8].
    std::uint32_t object_work_4f0660(){
        const std::uint32_t id=current_event_440b80();
        const std::uint32_t lane=std::int32_t(id)>=0x11a?1u:0u;
        const std::uint32_t h=u32(0x84d6ccu+lane*4u);
        const std::uint32_t rec=(h?u32(h):0u)+(id-u32(0x5da050u+lane*8u))*0x28u;
        const std::uint32_t size=u32(0x5da2a0u+u32(rec+0x20)*4u);
        const std::uint32_t w=call(0x440a60u,{size,lane?2u:7u});
        for(std::uint32_t k=0;k<size;++k)p8(w+k,0);
        p32(w+0x58,lane);
        copy3(w+0x40,rec);copy3(w+0x68,rec+0xc);
        pf(w+0x50,K(0x62806cu));
        if(u32(rec+0x20)==2u)p32(w+0x7c,u32(rec+0x24));
        if(u32(rec+0x18)!=3u&&u32(rec+0x18)!=0x3eu){
            push_load(lane?0x7d3190u:0x7d2da0u);
            translate_40a2d0(w+0x40);rotate_y(f32(w+0x6c));rotate_x(f32(w+0x68));rotate_z(f32(w+0x70));
            translation_40a250(w+0x40);
            euler_cur_449640(w+0x68);
            identity_40a020();
            translate_40a2d0(w+0x40);rotate_y(f32(w+0x6c));rotate_x(f32(w+0x68));rotate_z(f32(w+0x70));
            get_40a0d0(w);
            pop();
        }
        p32(w+0x5c,u32(rec+0x18));
        p32(w+0x64,u32(0x5da0a8u+u32(rec+0x18)*8u));
        p32(w+0x4c,u32(0x5da0acu+u32(rec+0x18)*8u));
        p32(w+0x60,0xffffffffu);
        return w;
    }
    // 4A8EA0(shape, r): the 26-point ball shape of radius r (axes, three rings of 4, the
    // last ring of 8 rotated by pi/4 about Y).
    void ball_shape_4a8ea0(std::uint32_t o,float r){
        p32(o,0x1a);
        const float nr=ssub(0.0f,r);
        const float v[18]{r,0,0, nr,0,0, 0,0,r, 0,0,nr, 0,r,0, 0,nr,0};
        for(std::uint32_t k=0;k<18;++k)pf(o+4+k*4,v[k]);
        auto ang=[&](std::int32_t k){return X87(k)*X87(K(0x628260u))+X87(K(0x5c4020u));};
        for(std::int32_t k=0;k<4;++k){
            const std::uint32_t p=o+0x4c+std::uint32_t(k)*12u;const X87 a=ang(k);
            pf(p+4,0.0f);pf(p,driving::x87_float(driving::x87_cos(a)*X87(r)));pf(p+8,driving::x87_float(driving::x87_sin(a)*X87(r)));
        }
        for(std::int32_t k=0;k<4;++k){
            const std::uint32_t p=o+0x7c+std::uint32_t(k)*12u;const X87 a=ang(k);
            pf(p,0.0f);pf(p+4,driving::x87_float(driving::x87_sin(a)*X87(r)));pf(p+8,driving::x87_float(driving::x87_cos(a)*X87(r)));
        }
        for(std::int32_t k=0;k<4;++k){
            const std::uint32_t p=o+0xac+std::uint32_t(k)*12u;const X87 a=ang(k);
            pf(p+8,0.0f);pf(p,driving::x87_float(driving::x87_cos(a)*X87(r)));pf(p+4,driving::x87_float(driving::x87_sin(a)*X87(r)));
        }
        for(std::uint32_t k=0;k<8;++k){
            const std::uint32_t p=o+0xdc+k*12u;
            push_unit();rotate_y(from_bits(0x3f490fdbu));point_40a7d0(p,p-0x60);pop();
        }
    }
    // 4A90B0(eax w, ebx body table {mass, inertia[3], restitution, friction}, shape data,
    // y offset): rigid body at w +0x80 from the work's angles and position (the ground
    // height when y is 0 or [5A29EC]), shape copied to w +0x170.
    void body_4a90b0(std::uint32_t w,std::uint32_t tbl,std::uint32_t data,float yoff){
        const std::uint32_t rb=w+0x80;
        push_409ef0();
        vehicle_rigid_body_516e10(m.bytes(rb,0xe4),c.matrices);
        p32(rb,0);
        identity_40a020();
        rotate_y(f32(w+0x6c));rotate_x(f32(w+0x68));rotate_z(f32(w+0x70));
        get_40a0d0(rb+0x10);
        const float y=f32(w+0x44);
        if(y!=K(0x619a34u)&&y!=K(0x5a29ecu))copy3(rb+0x40,w+0x40);
        else{
            Frame L(*this,0x18);
            copy3(L(0xc),w+0x40);
            call(0x43eb60u,{0x100,L(0xc),L(8),L(4),L(0)});
            copy3(rb+0x40,L(0xc));
        }
        pf(rb+0x44,sadd(f32(rb+0x44),yoff));
        p32(rb+0x98,u32(tbl));pf(rb+0x9c,sdiv(K(0x62806cu),f32(tbl)));
        copy3(rb+0xa0,tbl+4);
        pf(rb+0xac,sdiv(K(0x62806cu),f32(tbl+4)));
        pf(rb+0xb0,sdiv(K(0x62806cu),f32(tbl+8)));
        pf(rb+0xb4,sdiv(K(0x62806cu),f32(tbl+0xc)));
        p32(rb+0xb8,u32(tbl+0x10));p32(rb+0xbc,u32(tbl+0x14));
        if(u32(w+0x5c)==0xeu||u32(w+0x5c)==0x16u)p32(rb+0xdc,u32(0x5c4024u));
        for(std::uint32_t k=0;k<0x9du*4u;k+=4)p32(rb+0xf0+k,u32(data+k));
        pop();
    }
    // 4ABA80(w): the 0x5C object body by model (+64): sound +3F4, flags +3F8..+400.
    void movable_init_4aba80(std::uint32_t w){
        const std::uint32_t id=u32(w+0x64);
        std::uint32_t tbl=0x5c3f1cu,data=0x5c37b8u;float y=K(0x6280e4u);
        bool keep3fc=false,set400=true;   // keep3fc: +3FC already written
        auto movable=[&](std::uint32_t snd,std::uint32_t model){p32(w+0x3f4,snd);p32(w+0x60,model);};
        if(id==0x57001au||id==0x57001cu||id==0x570023u||id==0x570024u){movable(0x4e,id);}
        else if(id==0u){y=K(0x5c4044u);tbl=0x5c3f34u;data=0x5c3a30u;p32(w+0x3fc,0xffffffffu);p32(w+0x60,0xffffffffu);keep3fc=true;}
        else if(id==0x57002au||id==0x57002bu){tbl=0x5c3f4cu;data=0x5c3ca8u;movable(0x4e,id);y=0.0f;}
        else if(id==0x570039u){movable(0x50,0x570039u);}
        else if(id==0x1ee0001u){
            ball_shape_4a8ea0(0x681548u,2.0f);
            y=0.0f;pf(w+0x34,sadd(f32(w+0x34),K(0x5c3fe0u)));
            tbl=0x5c3f7cu;data=0x681548u;
            p32(w+0x3f4,0x187);p32(w+0x3fc,0xffffffffu);p32(w+0x60,0x1ee0001u);p32(w+0x400,0);
            keep3fc=true;set400=false;
        }
        else if(id==0x1ee0004u){ball_shape_4a8ea0(0x681548u,3.0f);tbl=0x5c3f64u;data=0x6812d0u;movable(0x50,0x1ee0004u);y=0.0f;}
        else{p32(w+0x3f4,0x50);p32(w+0x3fc,0xffffffffu);p32(w+0x60,0xffffffffu);keep3fc=true;}
        if(!keep3fc)p32(w+0x3fc,0xffffffffu);
        if(set400)p32(w+0x400,1);
        p32(w+0x3f8,0);
        body_4a90b0(w,tbl,data,y);
        p32(w+0x3f0,0);
        pf(w+0x50,K(0x62806cu));
    }
    // 40F3A0(out, a, b, c): unit normal of the triangle as the x87 code orders it (d1 = b - a,
    // d2 = c - b); 0 (out unnormalised) when its length is 0.
    std::uint32_t normal_40f3a0(std::uint32_t o,std::uint32_t a,std::uint32_t b,std::uint32_t cc){
        const X87 d1x=x(b)-x(a),d1y=x(b+4)-x(a+4),d1z=x(b+8)-x(a+8);
        const X87 d2x=X87(driving::x87_float(x(cc)-x(b))),d2y=X87(driving::x87_float(x(cc+4)-x(b+4)));
        const X87 d2z=x(cc+8)-x(b+8);
        pf(o,driving::x87_float(d2y*d1z-d2z*d1y));
        pf(o+4,driving::x87_float(d2z*d1x-d2x*d1z));
        pf(o+8,driving::x87_float(d2x*d1y-d2y*d1x));
        const X87 s=(x(o)*x(o)+x(o+4)*x(o+4))+x(o+8)*x(o+8);
        if(s==X87(K(0x619a34u)))return 0;
        const X87 inv=X87(K(0x62806cu))/driving::x87_sqrt(s);
        pf(o,driving::x87_float(inv*x(o)));pf(o+4,driving::x87_float(inv*x(o+4)));pf(o+8,driving::x87_float(inv*x(o+8)));
        return 1;
    }
    // 4A92E0(esi point, eax out plane {normal, point, point}): the ground under point
    // (43EB60 / 43D1D0); a wall polygon (kind & 10F4060) gives its 502E20 face, else the
    // polygon normal with point and point lowered by [6281C0] (point is modified).
    std::uint32_t ground_plane_4a92e0(std::uint32_t pt,std::uint32_t out){
        Frame L(*this,0x40);
        const std::uint32_t r=call(0x43eb60u,{0x100,pt,L(0),L(8),L(4)});
        const std::uint32_t poly=u32(L(0));
        if(poly==0xffffffffu)return 0;
        call(0x43d1d0u,{poly,r,L(0xc),L(0x18),L(0x24),L(0x30)});
        if(u32(L(4))&0x10f4060u){
            call(0x502e20u,{u32(pt),u32(pt+8),L(0x18),L(0xc),L(0x30),L(0x24)});
            copy3(out+0x18,L(0x18));copy3(out,L(0xc));copy3(out+0xc,L(0x30));
            return 1;
        }
        (void)normal_40f3a0(out,L(0xc),L(0x18),L(0x24));
        copy3(out+0xc,pt);
        pf(pt+4,ssub(f32(pt+4),K(0x6281c0u)));
        copy3(out+0x18,pt);
        return 1;
    }
    // 4A9250(esi rb, mask, n): damping; a body resting on more than 2 points with a small
    // angular velocity stops.
    void settle_4a9250(std::uint32_t rb,std::uint32_t mask,std::int32_t n){
        scale_40f050(rb+0x50,rb+0x50,from_bits(0x3f7d70a4u));
        scale_40f050(rb+0x5c,rb+0x5c,from_bits(0x3f7f7ceeu));
        if(n<=0)return;
        std::uint32_t cnt=0;
        for(std::int32_t k=n;k>0;--k){cnt+=mask&1u;mask>>=1;}
        if(cnt<=2u)return;
        double lim;{const std::uint64_t q=(std::uint64_t(u32(0x5c402cu))<<32)|u32(0x5c4028u);std::memcpy(&lim,&q,8);}
        const X87 s=driving::x87_sqrt(X87(lim));
        if(!(s>dot2_40f110(rb+0x50)))return;
        for(std::uint32_t o:{0x50u,0x54u,0x58u,0x5cu,0x60u,0x64u})p32(rb+o,0);
        p32(rb,0);
    }
    // 4ABCE0(w): one physics step of a hit 0x5C object (gravity [6817D0 + kind * 20], a
    // ground contact at shape point +3F0 % n).
    void movable_step_4abce0(std::uint32_t w){
        const std::uint32_t rb=w+0x80,shape=w+0x170;
        Frame L(*this,0x40);
        const std::uint32_t P=L(0),F=L(0xc),Pl=L(0x18),depth=L(0x3c);
        pf(F,0.0f);pf(F+4,smul(smul(K(0x6817d0u+u32(w+0x5c)*20u),f32(rb+0x98)),K(0x5c4048u)));pf(F+8,0.0f);
        copy3(P,rb+0x40);
        force_516f20(rb,F,P);
        const std::int32_t n=i32(shape);
        const std::int32_t j=std::int32_t(i32(w+0x3f0)%n);
        copy3(P,shape+0x13c+std::uint32_t(j)*12u);
        if(!ground_plane_4a92e0(P,Pl)){p32(w+0x3f0,0x384);return;}
        const std::uint32_t mask=contact_517770(rb,Pl,shape,depth,true);
        if(contact_point_517a40(rb,Pl,shape,mask,P)){
            impulse_517d90(rb,Pl,P,F);
            const std::uint32_t kind=u32(w+0x5c);
            if((kind==0xdu||kind==0x17u)&&gt(f32(F+4),K(0x681d24u)))p32(F+4,u32(0x681d24u));
            force_516f20(rb,F,P);
        }
        integrate_5170d0(rb);
        settle_4a9250(rb,mask,i32(shape));
    }
    // 4A9410(w): the cars (the player alone for the models below, else events 8..31) whose
    // box [car +338..+34C] around car +D28 meets the object's ([6817C0 + kind * 20]):
    // the body is activated and pushed (sound +3F4 and car +8 bit 0x2000 for the player
    // in a race), +3FC = the car's event; 1 when any car hit it.
    std::uint32_t hit_cars_4a9410(std::uint32_t w){
        const std::uint32_t id=u32(w+0x64),rb=w+0x80,pos=rb+0x40;
        std::uint32_t count=0x18;
        if(id==0x570021u||id==0x570023u||id==0x570024u||id==0x57002au||id==0x57002bu||id==0x57002cu||
           id==0x570031u||id==0x570039u||id==0x1ee0001u)count=1;
        Frame A(*this,0x120);
        const std::uint32_t res=A(8),R=A(0xc),scl=A(0x24),V=A(0x28),N=A(0x34),Cp=A(0x40),O=A(0x4c),rad=A(0x58),
            depth=A(0x5c),Q=A(0x60),V2=A(0x6c),Vo=A(0x78),E=A(0x84),I=A(0x90),Pc=A(0x9c),hx=A(0xa8),hz=A(0xac),
            P=A(0xb0),Z=A(0xbc),D=A(0xc8),M=A(0xd4);
        const std::uint32_t kt=0x6817c0u+u32(w+0x5c)*20u;
        copy3(E,kt);pf(rad,K(0x5c1c68u));pf(scl,K(kt+0xc));
        if(call(0x55a930u,{},0,0x7f9460u)&&id==0x570023u){
            pf(rad,K(0x6282ccu));
            scale_40f050(E,E,2.5f);
            pf(scl,smul(f32(scl),K(0x5c37b0u)));
        }
        push_409ef0();
        p32(res,0);
        for(std::uint32_t i=0;std::int32_t(i)<std::int32_t(count);++i){
            if((u8(0x79fb50u+i)&3u)!=2u)continue;
            const std::uint32_t car=u32(0x799d18u+i*0x3cu);
            copy3(P,car+0xd28);
            sub_40efa0(D,pos,P);
            if(dot2_40f110(D)>X87(f32(rad)))continue;
            identity_40a020();
            translate_40a2d0(P);
            multiply_40a220(car+0x70);
            get_40a0d0(M);
            invert_40a240();
            point_40a7d0(Q,pos);
            const float k=K(0x628064u);
            const float hzv=smul(ssub(f32(car+0x340),f32(car+0x34c)),k),hxv=smul(ssub(f32(car+0x338),f32(car+0x344)),k);
            const float qz=f32(Q+8),qx=f32(Q);
            const float ez=sadd(f32(E+8),hzv);pf(hz,hzv);
            const float nez=ssub(0.0f,ez);
            pf(hx,hxv);
            if(gt(nez,qz))continue;
            if(gt(qz,ez))continue;
            const float ex=sadd(f32(E),hxv);
            if(gt(qx,ex))continue;
            const float nex=ssub(0.0f,ex);
            if(gt(nex,qx))continue;
            copy3(Cp,Q);
            p32(N,0);p32(N+4,0);p32(N+8,0);
            const X87 sx=(driving::x87_abs(X87(qx))/X87(f32(hx)))*X87(f32(hz));
            if(sx>driving::x87_abs(X87(qz))){
                if(gt(qx,0.0f)){pf(Cp,ex);pf(N,K(0x62806cu));}
                else{pf(N,K(0x6280c4u));pf(Cp,nex);}
            }else{
                if(gt(qz,0.0f)){pf(Cp+8,ez);pf(N+8,K(0x62806cu));}
                else{pf(Cp+8,nez);pf(N+8,K(0x6280c4u));}
            }
            p32(rb,1);
            if(u32(0x78026cu)==0x10u){
                const std::uint32_t fl=u32(car+4);
                if((fl&1u)&&!(fl&2u)){
                    if(const std::uint32_t snd=u32(w+0x3f4))call(0x424940u,{snd});
                    p32(car+8,u32(car+8)|0x2000u);
                }
            }
            load_40a170(M);
            scale_40f050(O,N,from_bits(0xbc23d70au));
            add_40ef10(O,Cp);
            vector_40a820(N,N);
            point_40a7d0(Cp,Cp);
            point_40a7d0(O,O);
            const std::uint32_t kind=u32(w+0x5c),shape=rb+0xf0;
            if(kind!=0x11u&&kind!=0x3cu){
                const std::uint32_t mask=contact_517770(rb,N,shape,depth,true);
                if(contact_point_517a40(rb,N,shape,mask,Pc)){
                    impulse_517d90(rb,N,Pc,I);
                    force_516f20(rb,I,Pc);
                }
            }else{
                const std::uint32_t mask=contact_517770(rb,N,shape,depth,false);
                p32(res,mask);
                sub_40efa0(V,car+0x14,car+0x16c);
                (void)unit_40eeb0(V);
                scale_40f050(V,V,f32(depth));
                add_40ef10(pos,V);
                (void)contact_point_517a40(rb,N,shape,u32(res),Pc);
            }
            copy3(R,pos);
            sub_40ef70(R,P);
            pf(R+4,smul(f32(R+4),f32(scl)));
            normalize_40ef00(R);
            if(u32(w+0x5c)==0x11u){
                p32(Z,0);p32(Z+4,0);p32(Z+8,0);
                sub_40efa0(V2,car+0x14,car+0x16c);
                normalize_40ef00(V2);
                const float d=driving::x87_float(dot_40efd0(R,V2));
                pf(res,d);
                pf(R,f32(V2));pf(R+8,f32(V2+8));pf(R+4,sadd(f32(V2+4),K(0x62813cu)));
                if(X87(K(0x6282a0u))>X87(d)){
                    const float t=sdiv(K(0x628088u),ssub(K(0x62806cu),d));
                    lerp_519bd0(R,V2,R,t);
                    add_40ef10(R,V2);
                    normalize_40ef00(R);
                }
                const std::uint32_t Vc=V;
                p32(Vc,u32(car+0x20));p32(Vc+4,u32(car+0x24));p32(Vc+8,u32(car+0x28));
                const float kv=K(0x5b43a4u);
                pf(Vo+4,smul(f32(rb+0x60),kv));
                pf(Vo,driving::x87_float(x(rb+0x5c)*X87(kv)));
                pf(Vo+8,smul(f32(rb+0x64),kv));
                sub_40ef70(Vc,Vo);
                const X87 lim=X87(K(0x6281f0u));
                float s;
                if(lim>driving::x87_abs(x(Vo))&&lim>driving::x87_abs(x(Vo+4))&&lim>driving::x87_abs(x(Vo+8)))
                    s=driving::x87_float(len_40f0e0(Vc)*X87(K(0x5c4034u)));
                else s=driving::x87_float(len_40f0e0(Vc)*X87(K(0x5c4030u)));
                scale_40f050(I,R,s);
                force_516fa0(rb,I,Z,Pc);
            }else{
                scale_40f050(I,R,smul(f32(car+0x1c4),K(0x5c4034u)));
                force_516f20(rb,I,Pc);
            }
            p32(w+0x3fc,i+8u);
            p32(res,1);
        }
        pop();
        return u32(res);
    }
    // 4ACD50(w) (control of event functions 0x5C / 0x5D): an active (or ball) object hit by a
    // car starts moving: control 4ACBC0, one physics step, work position / matrix from the body.
    void movable_ctrl_4acd50(std::uint32_t w){
        const std::uint32_t id=u32(w+0x64);
        if(!(u8(w+0x54)&1u)&&id!=0x1ee0001u&&id!=0x1ee000du)return;
        if(!hit_cars_4a9410(w))return;
        p32(w+0x3f8,1);
        change_ctrl_440b90(0x4acbc0u);
        movable_step_4abce0(w);
        copy3(w+0x40,w+0xc0);
        for(std::uint32_t k=0;k<0x40;k+=4)p32(w+k,u32(w+0x90+k));
        p32(w+0x3f0,u32(w+0x3f0)+1u);
    }
    // 4ACBC0(w) (control of a moving 0x5C object): cars keep pushing it, physics while the
    // body is active; a timed object (+400) blinks under [628110] frames left and closes at
    // 900 frames; the work matrix follows the body.
    void movable_moving_4acbc0(std::uint32_t w){
        p32(w+0x3f8,0);p32(w+0x3fc,0xffffffffu);
        push_409ef0();
        (void)hit_cars_4a9410(w);
        p32(w+0x3f0,u32(w+0x3f0)+1u);
        if(u32(w+0x80)==1u)movable_step_4abce0(w);
        if(u32(w+0x400)){
            const std::int32_t left=0x384-i32(w+0x3f0);
            if(left<=0)call(0x4401d0u,{current_event_440b80()});
            else if(!jb(K(0x628110u),float(left)))p32(w+0x54,u32(w+0x54)^((std::uint32_t(left)*2u^u32(w+0x54))&2u));
        }
        pop();
        const float f=from_bits(call(0x4493e0u,{}));
        Frame L(*this,0x40);
        const std::uint32_t P=L(0x10),Eb=L(0x1c),Q=L(0x28),Ea=L(0x34);
        copy3(P,w+0xc0);copy3(Q,w+0x30);
        blend_40f180(P,P,f,Q,driving::x87_float(X87(K(0x62806cu))-X87(f)));
        euler_449640(Ea,w);
        euler_449640(Eb,w+0x90);
        lerp_40f480(Eb,Ea,Eb,f);
        push_unit();
        translate_40a2d0(P);rotate_y(f32(Eb+4));rotate_x(f32(Eb));rotate_z(f32(Eb+8));
        get_40a0d0(w);
        pop();
        copy3(w+0x40,w+0xc0);
        for(std::uint32_t k=0;k<0x40;k+=4)p32(w+k,u32(w+0x90+k));
    }
    // 4AA370(w): a car box (events 8..31, or the player alone for the models below) around
    // car +D28 holds the object position within +90: sound +78 for the player in a race,
    // +80 = the car's event; 1 when any.
    std::uint32_t touch_box_4aa370(std::uint32_t w){
        const std::uint32_t d=u32(w+0x64)-0x570021u;
        std::uint32_t count=0x18;
        if(d<=0x18u&&(d<=3u||(d>=9u&&d<=11u)||d==16u||d==24u))count=1;
        Frame A(*this,0x90);
        const std::uint32_t hz=A(0x18),hx=A(0x1c),P=A(0x20),Q=A(0x2c),D=A(0x34),M=A(0x44);
        push_409ef0();
        std::uint32_t res=0;
        for(std::uint32_t i=0;std::int32_t(i)<std::int32_t(count);++i){
            if((u8(0x79fb50u+i)&3u)!=2u)continue;
            const std::uint32_t car=u32(0x799d18u+i*0x3cu);
            const float k=K(0x628064u);
            copy3(P,car+0xd28);
            pf(hx,smul(ssub(f32(car+0x338),f32(car+0x344)),k));
            pf(hz,smul(ssub(f32(car+0x340),f32(car+0x34c)),k));
            sub_40efa0(D,w+0x40,P);
            identity_40a020();translate_40a2d0(P);multiply_40a220(car+0x70);get_40a0d0(M);invert_40a240();
            point_40a7d0(Q,w+0x40);
            const float ez=sadd(f32(w+0x90),f32(hz)),qz=f32(Q+8);
            if(gt(ssub(0.0f,ez),qz)||gt(qz,ez))continue;
            const float ex=sadd(f32(w+0x90),f32(hx)),qx=f32(Q);
            if(gt(qx,ex)||gt(ssub(0.0f,ex),qx))continue;
            if(u32(0x78026cu)==0x10u){
                const std::uint32_t fl=u32(car+4);
                if((fl&1u)&&!(fl&2u))if(const std::uint32_t snd=u32(w+0x78))call(0x424940u,{snd});
            }
            res=1;p32(w+0x80,i+8u);
        }
        pop();
        return res;
    }
    // 4AA140(w): as 4AA370 with a [6282A4] (models 1EE0004: [6282D4]) distance precheck.
    std::uint32_t touch_near_4aa140(std::uint32_t w){
        const std::uint32_t id=u32(w+0x64),d=id-0x570021u;
        std::uint32_t count=0x18;float r=K(0x6282a4u);
        if(id==0x1ee0004u){r=K(0x6282d4u);count=1;}
        else if(id==0x1ee000du||id==0x570039u)count=1;
        else if(d<=0x10u&&(d<=3u||(d>=9u&&d<=11u)||d==16u))count=1;
        Frame A(*this,0x80);
        const std::uint32_t P=A(0x10),Q=A(0x1c),D=A(0x24),M=A(0x34);
        push_409ef0();
        std::uint32_t res=0;
        for(std::uint32_t i=0;std::int32_t(i)<std::int32_t(count);++i){
            if((u8(0x79fb50u+i)&3u)!=2u)continue;
            const std::uint32_t car=u32(0x799d18u+i*0x3cu);
            copy3(P,car+0xd28);
            sub_40efa0(D,w+0x40,P);
            if(dot2_40f110(D)>X87(r)*X87(r))continue;
            identity_40a020();translate_40a2d0(P);multiply_40a220(car+0x70);get_40a0d0(M);invert_40a240();
            point_40a7d0(Q,w+0x40);
            const float k=K(0x628064u);
            const float hzv=smul(ssub(f32(car+0x340),f32(car+0x34c)),k),hxv=smul(ssub(f32(car+0x338),f32(car+0x344)),k);
            const float ez=sadd(f32(w+0x90),hzv),qz=f32(Q+8);
            if(gt(ssub(0.0f,ez),qz)||gt(qz,ez))continue;
            const float ex=sadd(f32(w+0x90),hxv),qx=f32(Q);
            if(gt(qx,ex)||gt(ssub(0.0f,ex),qx))continue;
            if(u32(0x78026cu)==0x10u){
                const std::uint32_t fl=u32(car+4);
                if((fl&1u)&&!(fl&2u))if(const std::uint32_t snd=u32(w+0x78))call(0x424940u,{snd});
            }
            res=1;p32(w+0x80,i+8u);
        }
        pop();
        return res;
    }
    // 4AB160(w) (init of event function 0x62): a hidden marker (scale 0) with the 2C0000
    // sprite timing.
    void marker_init_4ab160(){
        const std::uint32_t w=object_work_4f0660();
        push_load(w);translate3_40a290(0.0f,0.0f,0.0f);get_40a0d0(w);pop();
        p32(w+0x80,0xffffffffu);p32(w+0x60,0xffffffffu);
        pf(w+0x90,K(0x62806cu));p32(w+0x74,0);pf(w+0x50,K(0x6281c0u));p32(w+0x7c,0);p32(w+0x94,1);
        call(0x429780u,{0x2c0000u,w+0x9c,w+0xa0});
        p32(w+0x98,0);
    }
    // 4AC790(w) (control 0x62): a car touching the marker area (+90 * [5C4010], sound 0x92)
    // shows the 2C0000 sprite (control 4AB1F0).
    void marker_ctrl_4ac790(std::uint32_t w){
        push_unit();
        translate_40a2d0(w+0x40);rotate_y(f32(w+0x6c));rotate_x(f32(w+0x68));rotate_z(f32(w+0x70));
        scale_40a360(0.0f,0.0f,0.0f);
        get_40a0d0(w);
        pop();
        Frame A(*this,0xc0);
        const std::uint32_t Lw=A(8);
        copy3(Lw+0x40,w+0x40);
        pf(Lw+0x90,smul(f32(w+0x90),K(0x5c4010u)));p32(Lw+0x78,0x92);p32(Lw+0x64,0x570024u);
        if(!(u8(w+0x54)&1u)||!u32(w+0x94))return;
        p32(w+0x98,u32(w+0x98)+1u);
        if(!touch_box_4aa370(Lw))return;
        p32(w+0x98,u32(w+0x9c));p32(w+0x7c,1);p32(w+0x80,u32(Lw+0x80));
        change_ctrl_440b90(0x4ab1f0u);
        pf(w+0x90,0.0f);
        push_unit();translate_40a2d0(w+0x40);scale_40a360(f32(w+0x90),f32(w+0x90),f32(w+0x90));get_40a0d0(w);pop();
    }
    // 4AB1F0(w) (control after 4AC790): the 2C0000 sprite at the projected marker, frame +98.
    void marker_shown_4ab1f0(std::uint32_t w){
        const std::uint32_t cam=u32(0x79f574u);
        p32(w+0x7c,2);p32(w+0x80,0xffffffffu);
        if(!(u8(w+0x54)&1u)||!u32(w+0x94))return;
        Frame L(*this,0x10);
        push_load(cam+0x140);
        (void)project_449940(f32(cam+0xb0),f32(cam+0xb4),w+0x40,L(0));
        pop();
        call(0x429530u,{0x2c0000u,u32(L(0)),bits(ssub(0.0f,f32(L(4)))),5,u32(w+0x98)});
        const std::uint32_t f=u32(w+0x98)+1u;p32(w+0x98,f);
        if(std::int32_t(f)>=i32(w+0xa0))p32(w+0x98,u32(w+0xa0)+1u);
    }
    // 4AB690(w): a 1.5 scaled ring above the object with the 2C0006 sprite timing.
    void ring_init_4ab690(std::uint32_t w){
        push_load(w);translate3_40a290(0.0f,2.0f,0.0f);scale_40a360(1.5f,1.5f,1.5f);get_40a0d0(w);pop();
        p32(w+0x80,0xffffffffu);p32(w+0x60,0xffffffffu);
        pf(w+0x90,K(0x5b43c0u));p32(w+0x78,0);p32(w+0x7c,0);p32(w+0x74,0);p32(w+0x94,1);
        call(0x429780u,{0x2c0006u,w+0x9c,w+0xa0});
        pf(w+0x50,K(0x6282a0u));p32(w+0xc0,0);p32(w+0xd0,0);p32(w+0x98,u32(w+0x9c));
        p32(w+0xbc,1);p32(w+0xc4,1);p32(w+0x74,0);
    }
    // 4ACB00(w) (control 0x64): the spinning ring (+BC); a near car switches to 4AB960.
    void ring_ctrl_4acb00(std::uint32_t w){
        if(u32(w+0xbc))pf(w+0x6c,sadd(f32(w+0x6c),K(0x62813cu)));
        push_unit();
        translate3_40a290(0.0f,2.0f,0.0f);translate_40a2d0(w+0x40);
        rotate_y(f32(w+0x6c));rotate_x(f32(w+0x68));rotate_z(f32(w+0x70));
        const float s=driving::x87_float(x(w+0x50)*X87(K(0x5c3fe0u)));
        scale_40a360(s,s,s);
        get_40a0d0(w);
        pop();
        if(!(u8(w+0x54)&1u)||!u32(w+0x94)||!touch_near_4aa140(w))return;
        p32(w+0x7c,1);
        change_ctrl_440b90(0x4ab960u);
    }
    // 4AB960(w) (control after 4ACB00): a timed ring (+D0) counts +D4 down, blinks under
    // [5C4040] and closes at 0.
    void ring_taken_4ab960(std::uint32_t w){
        if(!u32(w+0xd0))return;
        const std::int32_t c=i32(w+0xd4)-1;p32(w+0xd4,std::uint32_t(c));
        if(c<=0){call(0x4401d0u,{current_event_440b80()});return;}
        if(jb(K(0x5c4040u),float(c)))return;
        p32(w+0x54,u32(w+0x54)^((std::uint32_t(c)*2u^u32(w+0x54))&2u));
    }
    // 4AB9B0() (init 0x65): the 0x5C body that is placed every frame; models 1EE000D are
    // 1.5 balls.
    void ball_init_4ab9b0(){
        (void)object_work_4f0660();
        const std::uint32_t w=work(current_event_440b80());
        copy3(w+0x404,w+0x40);copy3(w+0x410,w+0x68);
        if(u32(w+0x64)!=0x1ee000du)return;
        ball_shape_4a8ea0(0x681548u,1.5f);
        const float y=ssub(f32(w+0x34),K(0x62806cu));
        p32(w+0x60,0x1ee000du);p32(w+0x3f4,0);p32(w+0x3f8,0);p32(w+0x3fc,0xffffffffu);p32(w+0x400,0);
        pf(w+0x34,y);
        body_4a90b0(w,0x5c3f94u,0x681548u,0.0f);
        p32(w+0x3f0,0);p32(w+0x400,0);pf(w+0x50,K(0x62806cu));
    }
    // 4ACDE0(w) (control 0x65): matrix and body from the current placement, then 4ACD50.
    void ball_ctrl_4acde0(std::uint32_t w){
        if(!(u8(w+0x54)&1u))return;
        push_unit();
        translate_40a2d0(w+0x40);rotate_x(f32(w+0x68));rotate_z(f32(w+0x70));rotate_y(f32(w+0x6c));
        get_40a0d0(w);
        pop();
        body_4a90b0(w,0x5c3f94u,0x681548u,0.0f);
        movable_ctrl_4acd50(w);
    }
    // 40EF40(out, a, b): out = a + b (x87).
    void add3_40ef40(std::uint32_t o,std::uint32_t a,std::uint32_t b){
        const float r0=driving::x87_float(x(a)+x(b)),r1=driving::x87_float(x(a+4)+x(b+4)),r2=driving::x87_float(x(a+8)+x(b+8));
        pf(o,r0);pf(o+4,r1);pf(o+8,r2);
    }
    // 43F3D0(point): the lane strip of the point's road section (43E570 run edges split in
    // n = [run +54] * [628088] + [599438] strips), -1 when outside (point.y is grounded).
    std::int32_t lane_43f3d0(std::uint32_t p){
        Frame E(*this,0x230);
        const std::uint32_t special=E(0),U=E(0x14),N=E(0x20),Vv=E(0x2c),place=E(0x38),A0=E(0x48),A1=E(0x54),
            B0=E(0x60),B1=E(0x6c),out=E(0x78),S=E(0xdc);
        p32(special,0xffffffffu);
        call(0x43eb60u,{0x100,p,0,special,0});
        p32(place,0);p32(place+4,0);
        p16(place+8,section_43f370(p));
        call(0x43e570u,{out,place,u32(special)});
        const std::int32_t n=cvtt(sadd(smul(f32(out+0x54),K(0x628088u)),K(0x599438u)));
        const float step=sdiv(K(0x62806cu),float(n));
        blend_40f180(A0,out+0x24,0.5f,out+0x3c,0.5f);
        blend_40f180(A1,out+0x30,0.5f,out+0x48,0.5f);
        blend_40f180(B0,out+0x24,0.5f,out+0x30,0.5f);
        blend_40f180(B1,out+0x3c,0.5f,out+0x48,0.5f);
        sub_40efa0(U,A1,A0);sub_40efa0(Vv,B1,B0);
        (void)unit_40eeb0(U);(void)unit_40eeb0(Vv);
        cross_40eff0(N,Vv,U);
        if(n>=0){
            float t=0.0f;
            for(std::int32_t k=0;k<=n;++k){
                const std::uint32_t R=S+std::uint32_t(k)*0x30u;
                if(R+0x30u>E(0x22c))throw std::logic_error("43F3D0: more strips than its frame holds");
                const float omt=driving::x87_float(X87(K(0x62806cu))-X87(t));
                blend_40f180(R,out+0x24,omt,out+0x30,t);
                blend_40f180(R+0xc,out+0x3c,omt,out+0x48,t);
                add3_40ef40(R+0x18,N,R);
                sub_40efa0(U,R+0xc,R);
                sub_40efa0(Vv,R+0x18,R+0xc);
                (void)unit_40eeb0(U);(void)unit_40eeb0(Vv);
                cross_40eff0(R+0x24,Vv,U);
                t=sadd(step,t);
            }
        }
        for(std::int32_t i=0;i<n;++i){
            const std::uint32_t R=S+std::uint32_t(i)*0x30u,R1=R+0x30u;
            const float d0=driving::x87_float(dot_40efd0(R+0x24,R));
            float e0=driving::x87_float(X87(d0)-dot_40efd0(R+0x24,p));
            const float d1=driving::x87_float(dot_40efd0(R1+0x24,R1));
            const float e1=driving::x87_float(X87(d1)-dot_40efd0(R1+0x24,p));
            if(X87(K(0x599438u))>driving::x87_abs(X87(e0)))e0=0.0f;
            const float e1c=X87(K(0x599438u))>driving::x87_abs(X87(e1))?0.0f:e1;
            if(jb(0.0f,e0))continue;
            if(gt(e1c,0.0f))return i;
        }
        return -1;
    }
    // 4AB760() (init 0x64): a ring across the road at the object's section, facing along it
    // (randomly reversed), 200 units back (+B0: the facing * 1.75), then 4AB690.
    void ring_place_4ab760(){ring_place(0x5c1d40u,1.75f,false);}
    // 4AB760 / 4AADC0 body: facing y [yk], +B0 = facing * bscale, 4AB690 / 4AACD0.
    void ring_place(std::uint32_t yk,float bscale,bool hoop){
        const std::uint32_t w=object_work_4f0660();
        Frame E(*this,0xe8);
        const std::uint32_t D=E(0),special=E(0xc),place=E(0x10),o1=E(0x20),o2=E(0x84);
        p32(special,0xffffffffu);
        copy3(w+0xa4,w+0x40);
        call(0x43eb60u,{0x100,w+0xa4,0,special,0});
        p32(place,0);p32(place+4,0);
        const std::uint32_t sec=section_43f370(w+0x40);
        p16(w+0xcc,sec);p16(place+8,sec);
        const std::int16_t ln=std::int16_t(lane_43f3d0(w+0x40));
        p16(w+0xce,std::uint16_t(ln));
        if(ln<0)p16(w+0xce,4);
        call(0x43e570u,{o1,place,0xffffffffu});
        p16(place+8,u16(place+8)+1u);
        call(0x43e570u,{o2,place,0xffffffffu});
        sub_40efa0(D,o1+0x24,o1+0x30);
        push_load(0x7d2da0u);vector_40a820(D,D);pop();
        pf(D+4,0.0f);
        (void)unit_40eeb0(D);
        pf(D+4,K(yk));
        call(0x580f33u,{call(0x417f70u,{})});
        const std::int32_t r=std::int32_t(call(0x580f40u,{}));
        pf(D+4,ssub(f32(D+4),smul(float(r),K(0x62827cu))));
        const std::uint32_t stage=call(0x44dc50u,{call(0x44bdd0u,{})});
        p32(w+0xc8,1);
        bool flip;
        if(u32(special)==0xffffffffu)flip=std::int32_t(call(0x580f40u,{}))<0x3fff;
        else flip=stage==0x1cu&&!(u32(special)<call(0x44ddc0u,{stage,u32(place+8)}));
        if(flip){pf(D,smul(f32(D),K(0x6280c4u)));pf(D+8,smul(f32(D+8),K(0x6280c4u)));p32(w+0xc8,0);}
        (void)unit_40eeb0(D);
        scale_40f050(w+0xb0,D,bscale);
        scale_40f050(D,D,-200.0f);
        add_40ef10(w+0x40,D);
        if(hoop)hoop_init_4aacd0(w);else ring_init_4ab690(w);
    }
    // 4AA560() (init 0x5E): a 1-unit-high marker (+90 radius: [6282A4] for kind 0xA) with the
    // 2C0000 sprite timing.
    void heart_init_4aa560(){
        const std::uint32_t w=object_work_4f0660();
        push_load(w);translate3_40a290(0.0f,1.0f,0.0f);scale_40a360(1.0f,1.0f,1.0f);get_40a0d0(w);pop();
        p32(w+0x80,0xffffffffu);p32(w+0x60,0xffffffffu);
        p32(w+0x78,0xd6);p32(w+0x7c,0);pf(w+0x90,K(0x62806cu));
        if(u32(w+0x5c)==0xau)pf(w+0x90,K(0x6282a4u));
        p32(w+0xa4,0);pf(w+0x74,0.0f);p32(w+0x94,1);
        call(0x429780u,{0x2c0000u,w+0x9c,w+0xa0});
        p32(w+0x98,u32(w+0x9c));
    }
    // 4AA620(w) (control after 4ABF20; Heart Attack modes 0xF or a network race): the taken
    // marker rises over the player (+74 time, shrinking +50 after [5C4038]) with the 2C0000
    // sprite at its projection.
    void heart_taken_4aa620(std::uint32_t w){
        const std::uint32_t cam=u32(0x79f574u),car=u32(0x799d18u);
        if(!call(0x55a930u,{},0,0x7f9460u)&&call(0x45c440u,{})!=0xfu)return;
        if(!u32(w+0x7c)&&(!(u8(w+0x54)&1u)||!u32(w+0x94)))return;
        const float t=sadd(f32(w+0x74),K(0x5c3fb0u));
        const float y=sadd(ssub(smul(t,K(0x5c3fb4u)),smul(smul(t,t),K(0x5b02c0u))),f32(car+0x18));
        p32(w+0x7c,0);p32(w+0x80,0xffffffffu);pf(w+0x74,t);
        if(gt(t,K(0x5c4038u))){
            const float s=ssub(f32(w+0x50),K(0x5c3fb8u));pf(w+0x50,s);
            if(gt(0.0f,s)){pf(w+0x50,0.0f);p32(w+0x94,0);}
        }
        pf(w+0x6c,sadd(f32(w+0x6c),K(0x6282d4u)));
        p32(w+0x40,u32(car+0x14));pf(w+0x44,y);p32(w+0x48,u32(car+0x1c));
        push_unit();
        translate_40a2d0(w+0x40);rotate_y(f32(w+0x6c));rotate_x(f32(w+0x68));rotate_z(f32(w+0x70));
        scale_40a360(f32(w+0x50),f32(w+0x50),f32(w+0x50));
        get_40a0d0(w);
        pop();
        Frame L(*this,0x10);
        push_load(cam+0x140);
        (void)project_449940(f32(cam+0xb0),f32(cam+0xb4),w+0x40,L(0));
        pop();
        call(0x429530u,{0x2c0000u,u32(L(0)),bits(ssub(0.0f,f32(L(4)))),5,u32(w+0x98)});
        const std::uint32_t f=u32(w+0x98)+1u;p32(w+0x98,f);
        if(std::int32_t(f)>=i32(w+0xa0))p32(w+0x98,u32(w+0xa0)+1u);
    }
    // 43E790(section): the lane count of a course-0 section (43E570 run width).
    std::int32_t lanes_43e790(std::uint32_t section){
        Frame E(*this,0x80);
        p32(E(0),0);p32(E(4),0);p16(E(8),section);
        call(0x43e570u,{E(0x10),E(0),0xffffffffu});
        return cvtt(sadd(smul(f32(E(0x64)),K(0x628088u)),K(0x599438u)));
    }
    // 4AA7D0() (init 0x5F / 0x63): a heart pickup (2C0002 sprite, +B4..+C0 the two spark
    // offsets of its matrix, +A4 the lane count of its section).
    void spark_init_4aa7d0(){
        const std::uint32_t w=object_work_4f0660();
        p32(w+0x80,0xffffffffu);p32(w+0x60,0xffffffffu);
        pf(w+0x9c,K(0x6282a4u));p32(w+0x78,0xd6);p32(w+0x7c,0);
        for(std::uint32_t o:{0x84u,0x88u,0x8cu,0x90u,0x94u,0x98u,0x74u})pf(w+o,0.0f);
        p32(w+0xa0,1);
        call(0x429780u,{0x2c0002u,w+0xac,w+0xb0});
        p32(w+0xa8,u32(w+0xac));pf(w+0x50,K(0x62806cu));
        Frame E(*this,0x18);
        push_load(w);
        pf(E(0),K(0x6282a4u));pf(E(4),0.0f);pf(E(8),0.0f);
        point_40a7d0(E(0xc),E(0));
        p32(w+0xb4,u32(E(0xc)));p32(w+0xb8,u32(E(0x14)));
        pf(E(0),K(0x5c1d40u));
        point_40a7d0(E(0xc),E(0));
        p32(w+0xbc,u32(E(0xc)));p32(w+0xc0,u32(E(0x14)));
        pop();
        p32(w+0xa4,std::uint32_t(lanes_43e790(section_43f370(w+0x30))));
    }
    // 4AA920(w) (control after 4AC200; modes 0x10 / 0x19 or a network race): the taken
    // pickup spins (+94 by +7C), the five 2C0002 sparks around the player while +A8 runs to
    // +B0, a blink before [6282A4] - +74 reaches 0 and the event closes.
    void spark_taken(std::uint32_t w,std::uint32_t table){   // 4AA920: table 681CAC, 4AB2E0: 681CE8
        const std::uint32_t cam=u32(0x79f574u),car=u32(0x799d18u);
        if(!call(0x55a930u,{},0,0x7f9460u)&&call(0x45c440u,{})!=0x10u&&call(0x45c440u,{})!=0x19u)return;
        const std::int32_t fr=i32(w+0xa8),last=i32(w+0xb0);
        if(fr==last&&!u32(w+0x7c)&&(!(u8(w+0x54)&1u)||!u32(w+0xa0)))return;
        if(u32(w+0x7c)==1u)p32(w+0x94,u32(0x5b02b0u));
        else if(u32(w+0x7c)==2u)p32(w+0x94,u32(0x5c3fdcu));
        pf(w+0x74,sadd(f32(w+0x74),K(0x5c3fb0u)));
        pf(w+0x6c,sadd(f32(w+0x6c),f32(w+0x94)));
        p32(w+0x7c,0);p32(w+0x80,0xffffffffu);
        Frame E(*this,0x90);
        const std::uint32_t S=E(0),V=E(4),Pt=E(0x10),M=E(0x4c);
        if(f32(w+0x94)==0.0f&&!(fr>last)){
            push_unit();
            translate_40a2d0(car+0xd28);
            rotate_y(word_radians(i16(car+0x2e)));rotate_x(word_radians(i16(car+0x2c)));rotate_z(word_radians(i16(car+0x30)));
            for(std::uint32_t e=0;e<0x3cu;e+=0xc){
                const std::int32_t a=i32(w+0xa8);
                const float s=a<=6?sadd(smul(float(6-a),K(0x62813cu)),K(0x5c3fe0u)):1.5f;
                scale_40f050(V,table+e,s);
                const std::uint32_t p=Pt+e;
                vector_40a820(p,V);
                add_40ef10(p,car+0x14);
                const float r=word_radians(i16(car+0x160));
                pf(p,driving::x87_float(x(p)-driving::x87_sin(X87(r))*X87(K(0x6282a4u))));
                const float r2=word_radians(i16(car+0x160));
                pf(p+8,driving::x87_float(x(p+8)-driving::x87_cos(X87(r2))*X87(K(0x6282a4u))));
            }
            pop();
            push_load(cam+0x140);
            for(std::uint32_t k=0;k<5;++k){
                const std::uint32_t p=Pt+k*12u;
                (void)project_449940(f32(cam+0xb0),f32(cam+0xb4),p,p);
                float s=0.0f;
                if(!(X87(K(0x6281f0u))>driving::x87_abs(x(p+8))))
                    s=smul(sdiv(smul(f32(cam+0xb4),K(0x5c403cu)),ssub(0.0f,f32(p+8))),K(0x6282d8u));
                pf(S,s);
                translation_4393a6(M,f32(p),ssub(0.0f,f32(p+4)),0.0f);
                call(0x4289e0u,{0x2c0002u,5,M,u32(w+0xa8),u32(S)});
            }
            pop();
            const std::uint32_t f=u32(w+0xa8)+1u;p32(w+0xa8,f);
            if(std::int32_t(f)>=i32(w+0xb0))p32(w+0xa8,u32(w+0xb0)+1u);
        }
        const float rem=ssub(K(0x6282a4u),f32(w+0x74));
        pf(S,rem);
        if(!jb(0.0f,rem))call(0x4401d0u,{current_event_440b80()});
        else if(!jb(K(0x62806cu),rem)){
            const std::uint32_t b=ftol(X87(rem)*X87(K(0x628134u)));
            p32(w+0x54,u32(w+0x54)^((b<<1^u32(w+0x54))&2u));
        }
        push_unit();
        translate_40a2d0(w+0x40);rotate_x(f32(w+0x68));rotate_z(f32(w+0x70));rotate_y(f32(w+0x6c));
        scale_40a360(f32(w+0x50),f32(w+0x50),f32(w+0x50));
        get_40a0d0(w);
        pop();
    }
    // 4AACD0(w): the 1.5 scaled ring above the object (2C0006 sprite timing, radius [5C3FE0]).
    void hoop_init_4aacd0(std::uint32_t w){
        push_load(w);translate3_40a290(0.0f,2.0f,0.0f);scale_40a360(1.5f,1.5f,1.5f);get_40a0d0(w);pop();
        p32(w+0x80,0xffffffffu);p32(w+0x60,0xffffffffu);
        pf(w+0x90,K(0x5c3fe0u));p32(w+0x78,0xd6);p32(w+0x7c,0);pf(w+0x74,0.0f);p32(w+0x94,1);
        call(0x429780u,{0x2c0006u,w+0x9c,w+0xa0});
        p32(w+0x98,u32(w+0x9c));p32(w+0xbc,1);p32(w+0xc0,0);pf(w+0x50,K(0x62806cu));p32(w+0xc4,1);
    }
    // 4AADC0() (init 0x60): 4AB760 with facing y [6281B8], +B0 = facing * 6, then 4AACD0.
    void hoop_place_4aadc0(){ring_place(0x6281b8u,6.0f,true);}
    // 4AA620 / 4AAFC0 (controls after 4ABF20 / 4AC440): the taken object rises over the
    // player (+74 time, shrinking by `shrink` after [5C4038]) with the 2C0000 sprite.
    void taken_rise(std::uint32_t w,bool aafc0){
        const std::uint32_t cam=u32(0x79f574u),car=u32(0x799d18u);
        if(!aafc0&&!call(0x55a930u,{},0,0x7f9460u)&&call(0x45c440u,{})!=0xfu)return;
        if(aafc0?u32(w+0x7c)==2u&&(!(u8(w+0x54)&1u)||!u32(w+0x94)):!u32(w+0x7c)&&(!(u8(w+0x54)&1u)||!u32(w+0x94)))return;
        const float t=sadd(f32(w+0x74),K(0x5c3fb0u));
        const float y=sadd(ssub(smul(t,K(0x5c3fb4u)),smul(smul(t,t),K(0x5b02c0u))),f32(car+0x18));
        p32(w+0x7c,aafc0?2u:0u);p32(w+0x80,0xffffffffu);pf(w+0x74,t);
        if(gt(t,K(0x5c4038u))){
            const float s=ssub(f32(w+0x50),K(aafc0?0x5c3ff0u:0x5c3fb8u));pf(w+0x50,s);
            if(gt(0.0f,s)){pf(w+0x50,0.0f);p32(w+0x94,0);}
        }
        pf(w+0x6c,sadd(f32(w+0x6c),K(0x6282d4u)));
        p32(w+0x40,u32(car+0x14));pf(w+0x44,y);p32(w+0x48,u32(car+0x1c));
        push_unit();
        translate_40a2d0(w+0x40);rotate_y(f32(w+0x6c));rotate_x(f32(w+0x68));rotate_z(f32(w+0x70));
        scale_40a360(f32(w+0x50),f32(w+0x50),f32(w+0x50));
        get_40a0d0(w);
        pop();
        Frame L(*this,0x10);
        push_load(cam+0x140);
        (void)project_449940(f32(cam+0xb0),f32(cam+0xb4),w+0x40,L(0));
        pop();
        call(0x429530u,{0x2c0000u,u32(L(0)),bits(ssub(0.0f,f32(L(4)))),5,u32(w+0x98)});
        const std::uint32_t f=u32(w+0x98)+1u;p32(w+0x98,f);
        if(std::int32_t(f)>=i32(w+0xa0))p32(w+0x98,u32(w+0xa0)+1u);
    }
    // 4ABF20(w) (control 0x5E): the spinning heart (y [5B43B4], kind 0x39 at 1) is taken by a
    // near car (4AA140): control 4AA620 at the player; when other taken hearts (kinds A /
    // 22 / 39 with +A4 / +94 set) exist and [84D8E0] < 6 it vanishes instead, else
    // [84D8E0] = 0 and +A4 = 1.
    void heart_ctrl_4abf20(std::uint32_t w){
        const float h=u32(w+0x5c)==0x39u?K(0x62806cu):K(0x5b43b4u);
        const std::uint32_t car=u32(0x799d18u);
        pf(w+0x6c,sadd(f32(w+0x6c),K(0x5c3fccu)));
        push_unit();
        translate3_40a290(0.0f,h,0.0f);translate_40a2d0(w+0x40);
        rotate_y(f32(w+0x6c));rotate_x(f32(w+0x68));rotate_z(f32(w+0x70));
        scale_40a360(1.0f,1.0f,1.0f);
        get_40a0d0(w);
        pop();
        if(!(u8(w+0x54)&1u)||!u32(w+0x94)||!touch_near_4aa140(w))return;
        p32(w+0x7c,1);
        change_ctrl_440b90(0x4aa620u);
        p32(w+0x40,u32(car+0x14));p32(w+0x84,u32(car+0x14));
        p32(w+0x88,u32(car+0x18));p32(w+0x44,u32(car+0x18));
        p32(w+0x8c,u32(car+0x1c));p32(w+0x48,u32(car+0x1c));
        pf(w+0x70,0.0f);pf(w+0x6c,0.0f);pf(w+0x68,0.0f);
        pf(w+0x50,K(0x6280e4u));
        std::int32_t n=0;
        for(std::uint32_t id=0x20;id<0x11au;++id){
            if((u8(0x79fb48u+id)&3u)!=2u)continue;
            const std::uint32_t o=work(id),k=u32(o+0x5c);
            if((k==0xau||k==0x22u||k==0x39u)&&u32(o+0xa4)==1u&&u32(o+0x94)==1u)++n;
        }
        if(n>0&&std::int32_t(u32(0x84d8e0u))<6){pf(w+0x50,0.0f);p32(w+0x94,0);return;}
        p32(0x84d8e0u,0);
        p32(w+0xa4,1);
        push_unit();
        translate_40a2d0(w+0x40);rotate_y(f32(w+0x6c));rotate_x(f32(w+0x68));rotate_z(f32(w+0x70));
        scale_40a360(f32(w+0x50),f32(w+0x50),f32(w+0x50));
        get_40a0d0(w);
        pop();
    }
    // 4AC200 / 4AC8C0 (controls 0x5F / 0x63): the player's last move crossing the pickup
    // gate (+B4 segment) above its height, or a car near one of its two spark points, takes
    // it (+7C 3 / 1 / 2, control `next`: 4AA920 / 4AB2E0); mode 0x19 needs car +D36 > 0 for 3.
    void pickup_ctrl(std::uint32_t w,std::uint32_t next){
        const std::uint32_t car=u32(0x799d18u);
        push_unit();
        translate_40a2d0(w+0x40);rotate_y(f32(w+0x6c));rotate_x(f32(w+0x68));rotate_z(f32(w+0x70));
        scale_40a360(f32(w+0x50),f32(w+0x50),f32(w+0x50));
        get_40a0d0(w);
        pop();
        if(!(u8(w+0x54)&1u)||!u32(w+0xa0))return;
        Frame A(*this,0x170);
        const std::uint32_t Sg=A(4),L1=A(0x14),L2=A(0xc4);
        p32(Sg,u32(car+0x16c));p32(Sg+4,u32(car+0x174));p32(Sg+8,u32(car+0x14));p32(Sg+0xc,u32(car+0x1c));
        copy3(w+0x84,car+0x14);
        if(segments_449d00(Sg,w+0xb4)==1u&&gt(sadd(smul(f32(w+0x50),K(0x6282a4u)),f32(w+0x44)),f32(car+0x18))){
            if(call(0x45c440u,{})==0x19u&&!(std::int8_t(u8(car+0xd36))>0)){p32(w+0x7c,1);change_ctrl_440b90(next);return;}
            p32(w+0x7c,3);change_ctrl_440b90(next);return;
        }
        const float k=K(0x6280e4u);
        p32(L1+0x40,u32(w+0xbc));pf(L1+0x44,sadd(f32(w+0x44),k));p32(L1+0x48,u32(w+0xc0));
        pf(L1+0x90,smul(f32(w+0x50),k));p32(L1+0x78,0xcd);p32(L1+0x64,0x570031u);
        if(touch_near_4aa140(L1)){p32(w+0x7c,1);change_ctrl_440b90(next);return;}
        p32(L2+0x40,u32(w+0xb4));pf(L2+0x44,sadd(f32(w+0x44),k));p32(L2+0x48,u32(w+0xb8));
        pf(L2+0x90,smul(f32(w+0x50),k));p32(L2+0x78,0xcd);p32(L2+0x64,0x570031u);
        if(touch_near_4aa140(L2)){p32(w+0x7c,2);change_ctrl_440b90(next);}
    }
    // 4AC440(w) (control 0x60 / 0x61): the spinning hoop with its 2C0006 3D sprite (sound
    // 216E / 10DC at frame +9C) between +A4 (or the object) and +B0; a near car takes it
    // (control 4AAFC0).
    void hoop_ctrl_4ac440(std::uint32_t w){
        const std::uint32_t car=u32(0x799d18u),cam=u32(0x79f574u);
        pf(w+0x6c,sadd(f32(w+0x6c),K(0x62813cu)));
        push_unit();
        translate3_40a290(0.0f,2.0f,0.0f);translate_40a2d0(w+0x40);
        rotate_y(f32(w+0x6c));rotate_x(f32(w+0x68));rotate_z(f32(w+0x70));
        const float sc=driving::x87_float(x(w+0x50)*X87(K(0x5c3fe0u)));
        scale_40a360(sc,sc,sc);
        get_40a0d0(w);
        pop();
        if(!(u8(w+0x54)&1u)||!u32(w+0x94))return;
        if(u32(w+0xc0)&&!(i32(w+0x98)>i32(w+0xa0))&&u32(w+0xc4)){
            if(u32(w+0x98)==u32(w+0x9c))call(0x424940u,{u32(w+0xc8)?0x216eu:0x10dcu});
            Frame E(*this,0x70);
            const std::uint32_t L4=E(4),L10=E(0x10),Lt=E(0x1c),Mt=E(0x28);
            push_load(cam+0x140);
            const std::uint32_t from=u32(w+0xbc)?w+0x40:w+0xa4;
            (void)project_449940(f32(cam+0xb0),f32(cam+0xb4),from,L4);
            add3_40ef40(L10,w+0xb0,from);
            (void)project_449940(f32(cam+0xb0),f32(cam+0xb4),L10,L10);
            const float ang=driving::x87_float(driving::x87_atan2(x(L4)-x(L10),x(L4+4)-x(L10+4)));
            pop();
            float s=0.0f;
            if(!(X87(K(0x6281f0u))>driving::x87_abs(x(L4+8))))
                s=smul(sdiv(smul(f32(cam+0xb4),K(0x6282a4u)),ssub(0.0f,f32(L4+8))),K(0x6282d8u));
            const std::uint32_t m3=0x95d860u+3u*64u;
            put_vec(Lt,driving::pc_transform_point(m.bytes(m3,64),vec(L4)));
            translation_4393a6(Mt,f32(L4),ssub(0.0f,f32(L4+4)),f32(Lt+8));
            call(0x428ac0u,{0x2c0006u,Mt,u32(w+0x98),bits(s),bits(ang),0});
            const std::uint32_t f=u32(w+0x98)+2u;p32(w+0x98,f);
            if(std::int32_t(f)>=i32(w+0xa0))p32(w+0x98,u32(w+0xa0)+1u);
        }
        if(!touch_near_4aa140(w))return;
        change_ctrl_440b90(0x4aafc0u);
        p32(w+0x7c,1);
        p32(w+0x84,u32(car+0x14));p32(w+0x40,u32(car+0x14));
        p32(w+0x88,u32(car+0x18));p32(w+0x44,u32(car+0x18));
        p32(w+0x8c,u32(car+0x1c));p32(w+0x48,u32(car+0x1c));
        pf(w+0x70,0.0f);pf(w+0x6c,0.0f);pf(w+0x68,0.0f);
        pf(w+0x50,smul(f32(w+0x50),K(0x5c403cu)));
        push_unit();
        translate_40a2d0(w+0x40);rotate_y(f32(w+0x6c));rotate_x(f32(w+0x68));rotate_z(f32(w+0x70));
        scale_40a360(f32(w+0x50),f32(w+0x50),f32(w+0x50));
        get_40a0d0(w);
        pop();
    }
    // 4ABE40(w) (control 0x41): a racer / player car (+4 & 0x21) within [6282D4] knocks the
    // object over (+68 [5C4020], +74 = the knock vector) and stops its control (49A650).
    void sign_ctrl_4abe40(std::uint32_t w){
        for(std::uint32_t id=8;std::int32_t(id)<=i32(0x680ad4u)+8;++id){
            if((u8(0x79fb48u+id)&3u)!=2u)continue;
            const std::uint32_t car=work(id);
            if(!(u8(car+4)&0x21u))continue;
            if(!(X87(K(0x6282d4u))>=dist_40f140(w+0x40,car+0x14)))continue;
            pf(w+0x68,K(0x5c4020u));
            push_unit();
            rotate_y(f32(w+0x6c));rotate_x(f32(w+0x68));rotate_z(f32(w+0x70));
            pf(w+0x74,0.0f);pf(w+0x78,0.0f);pf(w+0x7c,K(0x5c404cu));
            point_40a7d0(w+0x74,w+0x74);
            pop();
            change_ctrl_440b90(0x49a650u);
        }
    }
    // Simple object works: 4A8A60 (4F0660), 4A8A70 / 4AADA0 / 4A9010 / 4A9C50 / 4AB2B0 (440A60).
    void blank_work(std::uint32_t size,std::uint32_t heap){const std::uint32_t w=call(0x440a60u,{size,heap});for(std::uint32_t k=0;k<size;k+=4)p32(w+k,0);}
    void lamp_work_4ab2b0(){const std::uint32_t w=call(0x440a60u,{0x80,1});for(std::uint32_t k=0;k<0x80;k+=4)p32(w+k,0);pf(w+0x74,K(0x62806cu));}
    // 4A8B50 (control 0x3D / 0x4A): the matrix of the placed object; 4A9F40 (init 0x41).
    void place_matrix(std::uint32_t w){
        push_unit();translate_40a2d0(w+0x40);rotate_y(f32(w+0x6c));rotate_x(f32(w+0x68));rotate_z(f32(w+0x70));get_40a0d0(w);pop();
    }
    // 4A8BA0 (control 0x3F / 0x4C): a billboard facing the camera eye (camera +F8).
    void billboard_4a8ba0(std::uint32_t w){
        if(!(u8(w+0x54)&1u))return;
        Frame E(*this,0x18);
        sub_40efa0(E(0xc),u32(0x79f574u)+0xf8u,w+0x40);
        angles_449800(E(0),E(0xc));
        push_unit();translate_40a2d0(w+0x40);rotate_y(f32(E(4)));rotate_x(f32(E(0)));rotate_z(f32(E(8)));get_40a0d0(w);pop();
    }
    // 4A8C20() (init 0x4F): display scene 0x100, +74 / +78 cleared.
    void lamp_init_4a8c20(){
        (void)object_work_4f0660();
        call(0x440bd0u,{current_event_440b80(),0x100});
        const std::uint32_t w=work(current_event_440b80());
        pf(w+0x78,0.0f);p32(w+0x74,0);
    }
    // 4A8C60(w) (control 0x4F): a light placed at the player in the course frame (+[5C4014]),
    // facing the camera; +78 fades in on stage 0x1C of course 0.
    void lamp_ctrl_4a8c60(std::uint32_t w){
        const std::uint32_t car=u32(0x799d18u);
        Frame E(*this,0x24);
        push_load(0x7d2da0u);
        invert_40a240();
        point_40a7d0(E(0xc),car+0x14);
        add_40ef10(E(0xc),0x5c4014u);
        load_40a170(0x7d2da0u);
        translate_40a2d0(E(0xc));
        translation_40a250(w+0x40);
        pop();
        sub_40efa0(E(0x18),u32(0x79f574u)+0xf8u,w+0x40);
        angles_449800(E(0),E(0x18));
        push_unit();translate_40a2d0(w+0x40);rotate_y(f32(E(4)));rotate_x(f32(E(0)));rotate_z(f32(E(8)));get_40a0d0(w);pop();
        p32(w+0x54,u32(w+0x54)|1u);
        p32(w+0x74,0);
        if(!u32(car+0x5c)&&call(0x44dc50u,{u32(car+0x68)})==0x1cu)p32(w+0x74,1);
        if(u32(w+0x74)==1u){
            float v=K(0x62806cu);
            if(gt(K(0x62806cu),f32(w+0x78)))v=sadd(f32(w+0x78),K(0x5a91bcu));
            pf(w+0x78,v);
        }
    }
    // ---- animated objects (0x40 / 0x4D) and swaying objects (0x42) ----
    std::uint32_t lane_matrix(std::uint32_t w){return u32(w+0x58)?0x7d3190u:0x7d2da0u;}   // 44BEC0 / 44BEA0
    // 4A9BD0(w): an object on the AREA lane's animation (+7C: entry of [84D6C0 + lane * 4],
    // +78 its duration), looping (+8C), matrices +A0 / +E0 cleared.
    void anim_init_4a9bd0(std::uint32_t w){
        pf(w+0x74,0.0f);p32(w+0x80,0);
        const std::uint32_t d=u32(0x84d6c0u+u32(w+0x58)*4u);
        if(d)p32(w+0x78,u32(d+u32(w+0x7c)*8u+4u));else pf(w+0x78,0.0f);
        p32(w+0x84,1);p32(w+0x88,0);p32(w+0x8c,1);
        unit_40a060(w+0xa0);
        p16(w+0x92,1);
        unit_40a060(w+0xe0);
    }
    void anim_place_4a9c70(){anim_init_4a9bd0(object_work_4f0660());}
    // 5136C0(data, t, pos, ang): six 513650 channels of the motion data.
    void motion_5136c0(std::uint32_t data,float t,std::uint32_t pos,std::uint32_t ang){
        auto ch=[&](std::uint32_t k){
            const std::int16_t count=i16(data+k*8u);const std::uint32_t keys=u32(data+k*8u+4u);
            driving::Bytes view(nullptr,0);
            if(count>0)view=m.bytes(keys,std::size_t(count)*16u);
            return driving::x87_float(driving::pc_sample_crash_channel_x87({count,view},t));
        };
        for(std::uint32_t k=0;k<3;++k)pf(pos+k*4u,ch(k));
        for(std::uint32_t k=0;k<3;++k)pf(ang+k*4u,ch(3+k));
    }
    // 4A9C80(w) (control 0x40 / 0x4D): position / angles from the animation at +74 (step
    // [62812C], looping or holding at +78), matrix in the lane (+88: in +A0), the previous
    // one kept at +E0.
    void anim_ctrl_4a9c80(std::uint32_t w){
        if(u32(w+0x80)||!u32(w+0x84))return;
        const std::uint32_t d=u32(0x84d6c0u+u32(w+0x58)*4u);
        if(d){
            const float t=gt(0.0f,f32(w+0x74))?0.0f:f32(w+0x74);
            motion_5136c0(u32(d+u32(w+0x7c)*8u),t,w+0x40,w+0x68);
        }else{for(std::uint32_t o:{0x40u,0x44u,0x48u,0x68u,0x6cu,0x70u})pf(w+o,0.0f);}
        const float t=sadd(f32(w+0x74),K(0x62812cu));pf(w+0x74,t);
        if(gt(t,f32(w+0x78))){if(u32(w+0x8c))pf(w+0x74,0.0f);else p32(w+0x74,u32(w+0x78));}
        push_load(u32(w+0x88)?w+0xa0:lane_matrix(w));
        translate_40a2d0(w+0x40);rotate_z(f32(w+0x70));rotate_y(f32(w+0x6c));rotate_x(f32(w+0x68));
        translation_40a250(w+0x40);
        if(u16(w+0x92)){p16(w+0x92,0);get_40a0d0(w+0xe0);get_40a0d0(w);}
        else{for(std::uint32_t k=0;k<0x40;k+=4)p32(w+0xe0+k,u32(w+k));get_40a0d0(w);}
        pop();
    }
    // 4A9F80() (init 0x42): a swaying object (+90 rate from its position, +94 / +70 angle).
    void sway_init_4a9f80(){
        const std::uint32_t w=object_work_4f0660();
        const std::int32_t q=cvtt(smul(f32(w+0x48),f32(w+0x40)));
        const std::int32_t rem=q%100;
        const float k=smul(sadd(smul(float(rem),K(0x6281c0u)),K(0x5b4434u)),K(0x628134u));
        copy3(w+0x78,w+0x40);copy3(w+0x84,w+0x68);
        pf(w+0x90,sdiv(K(0x5a29dcu),k));
        p32(w+0x74,current_event_440b80());
        p32(w+0x94,u32(w+0x70));
        push_load(lane_matrix(w));
        translate_40a2d0(w+0x78);rotate_z(f32(w+0x8c));rotate_y(f32(w+0x88));rotate_x(f32(w+0x84));
        translation_40a250(w+0x40);get_40a0d0(w);
        pop();
    }
    // 4AA070(w) (control 0x42): +70 advances by +90, the drawn angle blends +94 -> +70 with
    // the camera rate (4493E0), Y scaled by [7D3184].
    void sway_ctrl_4aa070(std::uint32_t w){
        const std::uint32_t mt=lane_matrix(w);
        pf(w+0x70,sadd(f32(w+0x90),f32(w+0x70)));
        const X87 b=X87(from_bits(call(0x4493e0u,{})));
        const float f=driving::x87_float(b*(x(w+0x70)-x(w+0x94))+x(w+0x94));
        p32(w+0x94,u32(w+0x70));
        push_load(mt);
        scale_40a360(1.0f,f32(0x7d3184u),1.0f);
        translate_40a2d0(w+0x78);rotate_z(f32(w+0x8c));rotate_y(f32(w+0x88));rotate_x(f32(w+0x84));rotate_z(f);
        translation_40a250(w+0x40);get_40a0d0(w);
        pop();
    }
    // ---- displays ----
    // Draw leaves recorded with the current matrix (executed by the runtime's draw list).
    void draw_leaf(std::uint32_t pc,std::initializer_list<std::uint32_t> args){
        if(!c.draws){call(pc,args);return;}   // the oracle: leaves as services
        PcVehicleDrawCall d{};d.pc=pc;d.argc=std::uint32_t(args.size());
        unsigned i=0;for(auto a:args)d.args[i++]=a;
        const auto cur=c.matrices.current();for(unsigned k=0;k<64;++k)d.matrix[k]=cur.u8(k);
        c.draws->push_back(d);
    }
    // 4A8A90(w): the object model (+64) at its matrix (balls scaled 2 / 0.75), LOD 9 for
    // 570021 else 8; kind 0x38 always, others when shown (+54 & 3 == 1).
    void object_disp_4a8a90(std::uint32_t w){
        if(u32(w+0x5c)!=0x38u&&((u8(w+0x54)&2u)||!(u8(w+0x54)&1u)))return;
        push_load(w);
        draw_leaf(0x4044a0u,{});
        const std::uint32_t id=u32(w+0x64);
        if(id==0x1ee0001u||id==0x1ee000du){pf(w+0x50,K(0x6280b0u));scale_40a360(2.0f,2.0f,2.0f);}
        else if(id==0x1ee0004u){pf(w+0x50,K(0x6282a0u));scale_40a360(0.75f,0.75f,0.75f);}
        draw_leaf(0x4044e0u,{u32(w+0x64)==0x570021u?9u:8u});
        draw_leaf(0x405360u,{u32(w+0x64),0,0,0,0xffffffffu,0});
        draw_leaf(0x4044c0u,{});
        pop();
    }
    // 4A9030(w): the shadow model (+60) of a shown object.
    void object_shadow_4a9030(std::uint32_t w){
        if((u8(w+0x54)&2u)||!(u8(w+0x54)&1u)||u32(w+0x60)==0xffffffffu)return;
        push_load(w);
        const std::uint32_t id=u32(w+0x64);
        if(id==0x1ee0001u||id==0x1ee000du){pf(w+0x50,K(0x6280b0u));scale_40a360(2.0f,2.0f,2.0f);}
        draw_leaf(0x405360u,{u32(w+0x60),1,0,0,0xffffffffu,0});
        pop();
    }
    // 4A9DF0(w) (display 0x40 / 0x4D): the model between the previous (+E0) and current
    // matrix with the camera rate (4493E0); +90 word: opaque flag.
    void anim_disp_4a9df0(std::uint32_t w){
        if((u8(w+0x54)&2u)||!(u8(w+0x54)&1u))return;
        const float b=from_bits(call(0x4493e0u,{}));
        Frame E(*this,0x30);
        const std::uint32_t La=E(0),Ea=E(0xc),Lb=E(0x18),Eb=E(0x24);
        copy3(La,w+0x30);copy3(Lb,w+0x110);
        blend_40f180(La,La,b,Lb,driving::x87_float(X87(K(0x62806cu))-X87(b)));
        euler_449640(Ea,w);
        euler_449640(Eb,w+0xe0);
        lerp_40f480(Ea,Eb,Ea,b);
        push_unit();
        translate_40a2d0(La);rotate_y(f32(Ea+4));rotate_x(f32(Ea));rotate_z(f32(Ea+8));
        for(std::uint32_t k=0;k<0x40;k+=4)p32(w+0xe0+k,u32(w+k));
        draw_leaf(0x4044a0u,{});
        draw_leaf(0x4044e0u,{u32(w+0x64)==0x570021u?9u:8u});
        draw_leaf(0x405360u,{u32(w+0x64),0,0,0,0xffffffffu,u16(w+0x90)?1u:0u});
        draw_leaf(0x4044c0u,{});
        pop();
    }
    // 4A8D80(w) (display 0x4F): the lamp model (4056D0 with alpha +78) under the light work
    // 4082B0(2, 0, 0) with its colours cleared / set to [628304] for the draw.
    void lamp_disp_4a8d80(std::uint32_t w){
        const std::uint32_t lw=call(0x4082b0u,{2,0,0});
        std::array<std::uint32_t,4> a{},b{};
        for(std::uint32_t k=0;k<4;++k){a[k]=u32(lw+8+k*4u);b[k]=u32(lw+0x28+k*4u);}
        for(std::uint32_t k=0;k<3;++k){pf(lw+8+k*4u,0.0f);pf(lw+0x28+k*4u,K(0x628304u));}
        call(0x410740u,{});
        call(0x4107a0u,{lw});
        push_load(w);
        draw_leaf(0x4044a0u,{});
        draw_leaf(0x4044e0u,{8});
        draw_leaf(0x4056d0u,{u32(w+0x64),u32(w+0x78),0,0xffffffffu});
        draw_leaf(0x4044c0u,{});
        pop();
        for(std::uint32_t k=0;k<4;++k){p32(lw+8+k*4u,a[k]);p32(lw+0x28+k*4u,b[k]);}
    }
    // Internal routines for the oracle: a0..a4 as the PC stack arguments (4A92E0: esi a0, eax a1).
    std::uint32_t object_debug(std::uint32_t pc,std::uint32_t a0,std::uint32_t a1,std::uint32_t a2,std::uint32_t a3,std::uint32_t a4){
        switch(pc){
        case 0x516f20u:force_516f20(a0,a1,a2);return 0;
        case 0x516fa0u:force_516fa0(a0,a1,a2,a3);return 0;
        case 0x517770u:return contact_517770(a0,a1,a2,a3,true);
        case 0x5178f0u:return contact_517770(a0,a1,a2,a3,false);
        case 0x517a40u:return contact_point_517a40(a0,a1,a2,a3,a4);
        case 0x517d90u:impulse_517d90(a0,a1,a2,a3);return 0;
        case 0x4a92e0u:return ground_plane_4a92e0(a0,a1);
        case 0x43f3d0u:return std::uint32_t(lane_43f3d0(a0));
        default:throw std::logic_error("course objects: no debug entry");
        }
    }
    // ---- event callbacks of the course objects ----
    bool object_callback(std::uint32_t cb,std::uint32_t w){
        switch(cb){
        case 0x4a8a60u:(void)object_work_4f0660();return true;
        case 0x4a8a70u:blank_work(0x80,1);return true;
        case 0x4aada0u:blank_work(0xd0,1);return true;
        case 0x4a9010u:blank_work(0x410,1);return true;
        case 0x4a9c50u:blank_work(0x120,2);return true;
        case 0x4ab2b0u:lamp_work_4ab2b0();return true;
        case 0x4abcd0u:movable_init_4aba80(object_work_4f0660());return true;
        case 0x4ab160u:marker_init_4ab160();return true;
        case 0x4ab760u:ring_place_4ab760();return true;
        case 0x4aadc0u:hoop_place_4aadc0();return true;
        case 0x4ab9b0u:ball_init_4ab9b0();return true;
        case 0x4aa560u:heart_init_4aa560();return true;
        case 0x4aa7d0u:spark_init_4aa7d0();return true;
        case 0x4a8c20u:lamp_init_4a8c20();return true;
        case 0x4a9c70u:anim_place_4a9c70();return true;
        case 0x4a9f40u:place_matrix(object_work_4f0660());return true;
        case 0x4a9f80u:sway_init_4a9f80();return true;
        case 0x4a8b50u:if(u8(w+0x54)&1u)place_matrix(w);return true;
        case 0x4a8ba0u:billboard_4a8ba0(w);return true;
        case 0x4a8c60u:lamp_ctrl_4a8c60(w);return true;
        case 0x4a9c80u:anim_ctrl_4a9c80(w);return true;
        case 0x4aa070u:sway_ctrl_4aa070(w);return true;
        case 0x4abe40u:sign_ctrl_4abe40(w);return true;
        case 0x4acd50u:movable_ctrl_4acd50(w);return true;
        case 0x4acbc0u:movable_moving_4acbc0(w);return true;
        case 0x4ac790u:marker_ctrl_4ac790(w);return true;
        case 0x4ab1f0u:marker_shown_4ab1f0(w);return true;
        case 0x4acb00u:ring_ctrl_4acb00(w);return true;
        case 0x4ab960u:ring_taken_4ab960(w);return true;
        case 0x4acde0u:ball_ctrl_4acde0(w);return true;
        case 0x4abf20u:heart_ctrl_4abf20(w);return true;
        case 0x4aa620u:taken_rise(w,false);return true;
        case 0x4aafc0u:taken_rise(w,true);return true;
        case 0x4ac200u:pickup_ctrl(w,0x4aa920u);return true;
        case 0x4ac8c0u:pickup_ctrl(w,0x4ab2e0u);return true;
        case 0x4aa920u:spark_taken(w,0x681cacu);return true;
        case 0x4ab2e0u:spark_taken(w,0x681ce8u);return true;
        case 0x4ac440u:hoop_ctrl_4ac440(w);return true;
        case 0x4a8a90u:object_disp_4a8a90(w);return true;
        case 0x4a9030u:object_shadow_4a9030(w);return true;
        case 0x4a9df0u:anim_disp_4a9df0(w);return true;
        case 0x4a8d80u:lamp_disp_4a8d80(w);return true;
        case 0x4aef50u:call(0x440b20u,{});return true;
        // internal routines exposed for the oracle (arguments: work, or the body for 5170D0)
        case 0x5170d0u:integrate_5170d0(w);return true;
        case 0x4abce0u:movable_step_4abce0(w);return true;
        case 0x4a9410u:(void)hit_cars_4a9410(w);return true;
        default:return false;
        }
    }

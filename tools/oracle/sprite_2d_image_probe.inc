if(only=="sprite_2d_image_probe"){
    // Original 2D image producers 42D280 (42C2F0 rectangle, flips), 42D300
    // (stretched image as a sprite record, mode table 42D5B0) and 42D5F0
    // (image groups) against pc_image_42d280 / pc_image_42d300 /
    // pc_image_group_42d5f0, over synthetic XST bank headers (image and group
    // tables, the guest copy relocated to absolute pointers as the loader
    // leaves them; natively the offsets stay relative to the header) and
    // random arguments. Compared: the queue nodes (the uninitialised record
    // +0 stack word of sprite records excepted), heads, 956BF4, 95B220.
    using namespace outrun::platform;
    const bool inject=argc>6&&std::string(argv[6])=="inject-mismatch";
    map_original_arithmetic_bridge(file);                  // 42D280's bridge 448102 reads [1039E80]
    for(std::uint32_t page=0x62f000u;page<0x98d000u;page+=0x1000u)mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE);
    map_at(Pc2dNodeBase,0x30000,PROT_READ|PROT_WRITE);
    constexpr std::uint32_t Headers=0x3a820000u,TexArray=0x3a830000u,PoolPtr=0x3a831000u;
    map_at(Headers,0x10000,PROT_READ|PROT_WRITE);map_at(TexArray,0x2000,PROT_READ|PROT_WRITE);
    auto g32=[](std::uint32_t a){return *reinterpret_cast<const std::uint32_t*>(a);};
    auto p32=[](std::uint32_t a,std::uint32_t v){*reinterpret_cast<std::uint32_t*>(a)=v;};
    auto fb=[](float f){std::uint32_t u;std::memcpy(&u,&f,4);return u;};
    p32(PoolPtr,Pc2dNodeBase);p32(0x956bec,PoolPtr);
    std::mt19937 rng(0x42d300u);
    auto uni=[&](float lo,float hi){return lo+(hi-lo)*float(rng()%100000u)/100000.0f;};
    const unsigned cases=std::max(count,1u)*40u;
    unsigned mismatches=0,calls[4]{};
    auto report=[&](const std::string& what){++mismatches;if(mismatches<=12)std::cerr<<"MISMATCH "<<what<<char(10);};
    for(unsigned cs=0;cs<cases;++cs){
        const std::string tag=" case "+std::to_string(cs);
        PcSprite2dState s;
        // Banks 3 and 5: header {0, meta, textures, 0, groups, group table, images, image table}.
        for(std::uint32_t b=0;b<PcSpriteBankCount;++b){p32(0x956d88+b*0x2c,0);p32(0x956d88+b*0x2c+4,0);p32(0x956d88+b*0x2c+8,0);}
        const std::uint32_t banks[2]{3u,5u};
        for(unsigned bi=0;bi<2;++bi){
            const auto b=banks[bi];const std::uint32_t H=Headers+bi*0x8000u;
            const std::uint32_t images=4u+rng()%12u,groups=1u+rng()%4u;
            std::vector<std::uint8_t> h(0x20u+images*0x1cu+groups*8u+groups*4u*0x10u,0);
            auto w32=[&](std::uint32_t o,std::uint32_t v){std::memcpy(h.data()+o,&v,4);};
            auto w16=[&](std::uint32_t o,std::uint16_t v){std::memcpy(h.data()+o,&v,2);};
            const std::uint32_t img=0x20u,grp=img+images*0x1cu,ent=grp+groups*8u;
            w32(8,8);w32(0x10,groups);w32(0x14,grp);w32(0x18,images);w32(0x1c,img);
            for(std::uint32_t i=0;i<images;++i){w32(img+i*0x1c,rng()%8u);
                const std::uint16_t l=std::uint16_t(rng()%200u),t=std::uint16_t(rng()%200u);
                w16(img+i*0x1c+0x14,l);w16(img+i*0x1c+0x16,t);w16(img+i*0x1c+0x18,std::uint16_t(l+rng()%64u));w16(img+i*0x1c+0x1a,std::uint16_t(t+rng()%64u));}
            for(std::uint32_t g=0;g<groups;++g){const std::uint32_t n=1u+rng()%4u;w32(grp+g*8,n);w32(grp+g*8+4,ent+g*0x40u);
                for(std::uint32_t k=0;k<n;++k){const auto e=ent+g*0x40u+k*0x10u;w32(e,rng()%images);w16(e+4,std::uint16_t(rng()%300u));w16(e+6,std::uint16_t(rng()%300u));w16(e+0xa,std::uint16_t(rng()%4u));}}
            s.banks[b].header=h;s.banks[b].state=2;s.banks[b].textures.clear();
            for(unsigned k=0;k<8;++k){const std::uint32_t t=0x51000000u+b*0x100u+k;s.banks[b].textures.push_back(t);p32(TexArray+bi*0x100u+k*4,t);}
            // Guest copy with absolute pointers.
            std::vector<std::uint8_t> gh=h;
            auto g_w32=[&](std::uint32_t o,std::uint32_t v){std::memcpy(gh.data()+o,&v,4);};
            g_w32(0x14,H+grp);g_w32(0x1c,H+img);
            for(std::uint32_t g=0;g<groups;++g){std::uint32_t v;std::memcpy(&v,gh.data()+grp+g*8+4,4);g_w32(grp+g*8+4,H+v);}
            std::memcpy(reinterpret_cast<void*>(std::uintptr_t(H)),gh.data(),gh.size());
            p32(0x956d88+b*0x2c,2);p32(0x956d88+b*0x2c+4,H);p32(0x956d88+b*0x2c+8,TexArray+bi*0x100u);
        }
        s.cursor_956bf4=rng()%Pc2dNodeCount;s.count_95b220=rng()%0x40u;p32(0x956bf4,s.cursor_956bf4);p32(0x95b220,s.count_95b220);
        s.update_index_8a8cdc=rng()%16u==0u?1u:0u;p32(0x8a8cdc,s.update_index_8a8cdc);
        for(std::uint32_t li=0;li<Pc2dLayerCount;++li)p32(0x956c00+li*4,0);
        std::memset(reinterpret_cast<void*>(Pc2dNodeBase),0,Pc2dNodeCount*Pc2dNodeBytes);
        const unsigned n=1u+rng()%6u;
        for(unsigned k=0;k<n;++k){
            const std::uint32_t b=banks[rng()%2u],token=(rng()%8u==0u?(7u<<16):(b<<16))|(rng()%16u);
            const std::int32_t x=std::int32_t(rng()%700u)-50,y=std::int32_t(rng()%520u)-20;
            const float layer=float(rng()%0x16u);const std::uint32_t colour=rng();
            const unsigned which=rng()%4u;++calls[which];
            if(which==0){
                const std::uint32_t flip=rng()%4u;
                prepare(0x42d280);auto* a=reinterpret_cast<unsigned*>(S);a[0]=token;a[1]=std::uint32_t(x);a[2]=std::uint32_t(y);a[3]=flip;a[4]=fb(layer);a[5]=colour;
                run_original32();
                pc_image_42d280(s,token,x,y,flip,layer,colour);
            }else if(which==1){
                const std::uint32_t mode=rng()%5u;const float w=uni(0.f,300.f),h=uni(0.f,200.f);
                prepare(0x42d300);auto* a=reinterpret_cast<unsigned*>(S);
                a[0]=mode;a[1]=token;a[2]=std::uint32_t(x);a[3]=std::uint32_t(y);a[4]=fb(w);a[5]=fb(h);a[6]=0;a[7]=fb(layer);a[8]=colour;
                run_original32();
                pc_image_42d300(s,mode,token,x,y,w,h,layer,colour);
            }else if(which==3){
                const std::uint32_t flip=rng()%4u;const float sx=uni(0.f,100.f);
                prepare(0x42d200);auto* a=reinterpret_cast<unsigned*>(S);a[0]=token;a[1]=std::uint32_t(x);a[2]=std::uint32_t(y);a[3]=fb(sx);a[4]=flip;a[5]=fb(layer);a[6]=colour;
                run_original32();
                pc_image_42d200(s,token,x,y,sx,flip,layer,colour);
            }else{
                const std::uint32_t flags=rng()%4u==0u?0x8000u:0u;const std::uint32_t gtoken=(token&0xffff0000u)|(rng()%5u);
                prepare(0x42d5f0);auto* a=reinterpret_cast<unsigned*>(S);a[0]=gtoken;a[1]=std::uint32_t(x);a[2]=std::uint32_t(y);a[3]=fb(layer);a[4]=colour;a[5]=flags;
                run_original32();
                pc_image_group_42d5f0(s,gtoken,x,y,layer,colour,flags);
            }
        }
        if(s.missing)report("native fault "+std::to_string(s.missing)+tag);
        if(inject&&cs==7u)s.count_95b220^=1u;
        for(std::uint32_t li=0;li<Pc2dLayerCount;++li)if(g32(0x956c00+li*4)!=s.heads_956c00[li]){report("head "+std::to_string(li)+tag);break;}
        if(g32(0x956bf4)!=s.cursor_956bf4)report("956BF4"+tag);
        if(g32(0x95b220)!=s.count_95b220)report("95B220"+tag);
        {const auto* g=reinterpret_cast<const std::uint8_t*>(Pc2dNodeBase);
         for(std::uint32_t nd=0;nd<Pc2dNodeCount;++nd){const std::uint32_t o=nd*Pc2dNodeBytes;
            std::uint32_t kind;std::memcpy(&kind,g+o+0xc,4);
            for(std::uint32_t k=0;k<Pc2dNodeBytes;k+=4){if(kind!=0u&&(k==0x58u||k==0xfcu||k==0x100u))continue;   // +0 stack word; +A4/+A8 not written by 42D300
                std::uint32_t gv,nv;std::memcpy(&gv,g+o+k,4);std::memcpy(&nv,s.nodes.data()+o+k,4);
                if(gv!=nv){std::ostringstream m;m<<"node "<<nd<<" kind "<<kind<<" +"<<std::hex<<k<<" guest "<<gv<<" native "<<nv;if(mismatches<3){m<<" |";for(std::uint32_t j=0x58;j<0x110;j+=4){std::uint32_t a2,b2;std::memcpy(&a2,g+o+j,4);std::memcpy(&b2,s.nodes.data()+o+j,4);if(a2!=b2)m<<" +"<<std::hex<<(j-0x58)<<":"<<a2<<"/"<<b2;}
                    m<<" rec"; for(std::uint32_t j=0x58;j<0x7c;j+=4){std::uint32_t a2;std::memcpy(&a2,g+o+j,4);m<<" "<<a2;}}
                    report(m.str()+tag);nd=Pc2dNodeCount;break;}}}}
    }
    std::cerr<<"2D images: 42D280="<<calls[0]<<" 42D300="<<calls[1]<<" 42D5F0="<<calls[2]<<" 42D200="<<calls[3]<<char(10);
    std::cout<<"{\"routine\":\"sprite_2d_image_probe\",\"total_cases\":"<<cases<<",\"total_mismatching_cases\":"<<mismatches<<"}"<<'\n';return 0;
}

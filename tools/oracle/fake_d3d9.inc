// Guest-side fake IDirect3DDevice9 for original-code oracles. Every method is
// an x86 stub that appends [id, argc, args...] to a log in guest memory and
// returns D3D_OK. A few methods keep state (render/sampler/stage states) or
// create fake objects (buffers with guest backing memory, declarations that
// keep their element arrays, shaders keeping a copy of their tokens,
// textures created through the faked D3DX file loaders).
// Log record: 0x30 bytes: +0 id, +4 argc (without this), +8.. up to 10 args.
namespace fake_d3d9 {
constexpr std::uint32_t Base=0x5e000000u,Size=0x02000000u;
constexpr std::uint32_t Device=Base+0x0,DeviceVt=Base+0x100,BufferVt=Base+0x400,DeclVt=Base+0x500,
    ShaderVt=Base+0x600,TextureVt=Base+0x700,CubeVt=Base+0x800,SurfaceVt=Base+0x880,Heap=Base+0x40,Stubs=Base+0x10000,
    RenderStates=Base+0x4000,SamplerStates=Base+0x6000,StageStates=Base+0x8000,
    Log=Base+0x100000,LogCapacity=0x40000u,HeapStart=Base+0x1000000,
    Textures=Base+0xa000,DataPtr=Base+0xb000,Data=Base+0xd00000,DataEnd=Base+0x1000000,
    // Frame targets: fixed surfaces returned by GetRenderTarget(0) and
    // GetDepthStencilSurface, two more for layer targets, the viewport state.
    BackBuffer=Base+0x9000,DepthSurface=Base+0x9010,SurfaceA=Base+0x9020,SurfaceB=Base+0x9030,ViewportState=Base+0x9100;
constexpr unsigned DeviceArgc[119]={3,1,1,1,1,1,2,2,3,2,4,4,2,3,3,1,2,5,5,3,2,4,3,9,10,8,7,7,9,9,5,3,3,3,6,4,7,3,3,2,2,1,1,7,3,3,3,2,2,2,2,3,3,3,3,3,3,3,3,3,1,2,2,2,3,3,4,4,4,4,2,3,3,2,2,2,2,2,1,2,1,4,7,5,9,7,3,2,2,2,2,3,2,2,4,4,4,4,4,4,5,5,3,3,2,2,3,2,2,4,4,4,4,4,4,4,4,2,3};
struct Emit {
    std::vector<std::uint8_t> b;
    void u8(std::initializer_list<unsigned> v){for(auto x:v)b.push_back(std::uint8_t(x));}
    void u32(std::uint32_t v){for(unsigned k=0;k<4;++k)b.push_back(std::uint8_t(v>>(k*8)));}
};
// Prologue: push esi/edi/ecx/edx; log id/argc/args; eax=0. Args: arg k (this=0) at [esp+0x14+4k].
inline void prologue(Emit& e,std::uint32_t id,unsigned argc){
    e.u8({0x56,0x57,0x51,0x52,0xa1});e.u32(Log);
    e.u8({0x3d});e.u32(LogCapacity);e.u8({0x73,0x00});           // cmp eax,cap; jae skip_log
    const auto patch=e.b.size();
    e.u8({0x69,0xc8});e.u32(0x30);e.u8({0x81,0xc1});e.u32(Log+0x10);
    e.u8({0xc7,0x01});e.u32(id);e.u8({0xc7,0x41,0x04});e.u32(argc-1);
    e.u8({0x8d,0x74,0x24,0x18,0x8d,0x79,0x08,0x51,0xb9});e.u32(std::min(argc-1,10u));
    e.u8({0xf3,0xa5,0x59,0x40,0xa3});e.u32(Log);
    e.b[patch-1]=std::uint8_t(e.b.size()-patch);
    e.u8({0x31,0xc0});                                            // skip_log target
}
inline void epilogue(Emit& e,unsigned argc){e.u8({0x5a,0x59,0x5f,0x5e,0xc2});e.b.push_back(std::uint8_t(argc*4));e.b.push_back(0);}
inline void write_stub(std::uint32_t at,const Emit& e){
    if(e.b.size()>0x100u)throw std::runtime_error("fake d3d9 stub too large");
    std::memcpy(reinterpret_cast<void*>(at),e.b.data(),e.b.size());
}
// eax = new object of `bytes` (16-aligned) with vtable vt.
inline void alloc_fixed(Emit& e,std::uint32_t bytes,std::uint32_t vt){
    e.u8({0xa1});e.u32(Heap);e.u8({0x81,0x05});e.u32(Heap);e.u32((bytes+15u)&~15u);
    e.u8({0xc7,0x00});e.u32(vt);
}
inline std::uint32_t install(){
    map_at(Base,Size,PROT_READ|PROT_WRITE|PROT_EXEC);
    std::memset(reinterpret_cast<void*>(Base),0,0x200000);
    *reinterpret_cast<std::uint32_t*>(Heap)=HeapStart;
    *reinterpret_cast<std::uint32_t*>(DataPtr)=Data;
    *reinterpret_cast<std::uint32_t*>(Device)=DeviceVt;
    std::uint32_t next=Stubs;
    auto place=[&](const Emit& e){write_stub(next,e);const auto at=next;next+=0x100u;return at;};
    for(unsigned slot=0;slot<119;++slot){
        const unsigned n=DeviceArgc[slot];Emit e;prologue(e,slot,n);
        switch(slot){
        case 83:{ // DrawPrimitiveUP(type, count, pData, stride): (count+2)*stride bytes appended to the data log
            e.u8({0x8b,0x44,0x24,0x1c,0x83,0xc0,0x02,0x0f,0xaf,0x44,0x24,0x24,0x89,0xc1,0x8b,0x74,0x24,0x20,0x8b,0x3d});e.u32(DataPtr);
            e.u8({0xf3,0xa4,0x89,0x3d});e.u32(DataPtr);e.u8({0x31,0xc0});break;}
        case 57: // SetRenderState(state,value)
            e.u8({0x8b,0x44,0x24,0x18,0x8b,0x54,0x24,0x1c,0x25});e.u32(0x3ff);
            e.u8({0x89,0x14,0x85});e.u32(RenderStates);e.u8({0x31,0xc0});break;
        case 58: // GetRenderState(state,out)
            e.u8({0x8b,0x44,0x24,0x18,0x25});e.u32(0x3ff);e.u8({0x8b,0x04,0x85});e.u32(RenderStates);
            e.u8({0x8b,0x54,0x24,0x1c,0x89,0x02,0x31,0xc0});break;
        case 66:case 67:case 68:case 69:{ // Get/Set TextureStageState / SamplerState (stage,type,value|out)
            const std::uint32_t table=slot<68?StageStates:SamplerStates;
            e.u8({0x8b,0x44,0x24,0x18,0x83,0xe0,0x0f,0xc1,0xe0,0x05,0x8b,0x54,0x24,0x1c,0x83,0xe2,0x1f,0x01,0xd0});
            if(slot==67||slot==69){e.u8({0x8b,0x54,0x24,0x20,0x89,0x14,0x85});e.u32(table);}
            else{e.u8({0x8b,0x04,0x85});e.u32(table);e.u8({0x8b,0x54,0x24,0x20,0x89,0x02});}
            e.u8({0x31,0xc0});break;}
        case 26:case 27:{ // CreateVertexBuffer/CreateIndexBuffer(Length,..,pp=arg5)
            e.u8({0xa1});e.u32(Heap);e.u8({0x8b,0x4c,0x24,0x18,0x83,0xc1,0x4f,0x83,0xe1,0xf0,0x01,0x0d});e.u32(Heap);
            e.u8({0xc7,0x00});e.u32(BufferVt);e.u8({0x8b,0x4c,0x24,0x18,0x89,0x48,0x04,0x8b,0x54,0x24,0x28,0x89,0x02,0x31,0xc0});break;}
        case 86: // CreateVertexDeclaration(pElements, ppDecl)
            alloc_fixed(e,0x140,DeclVt);
            e.u8({0x8b,0x74,0x24,0x18,0x8d,0x78,0x20,0xb9});e.u32(0x40);e.u8({0xf3,0xa5});
            e.u8({0x8b,0x54,0x24,0x1c,0x89,0x02,0x31,0xc0});break;
        case 91:case 106: // CreateVertexShader/CreatePixelShader(pFunction, pp); tokens copied to +0x20
            alloc_fixed(e,0x1020,ShaderVt);
            e.u8({0x50,0x8d,0x78,0x20,0x8b,0x74,0x24,0x1c,0xad,0xab,0x3d});e.u32(0xffff);e.u8({0x75,0xf7,0x58});e.u8({0x8b,0x4c,0x24,0x18,0x89,0x48,0x04,0x8b,0x54,0x24,0x1c,0x89,0x02,0x31,0xc0});break;
        case 64: // GetTexture(stage, pp): tracked table
            e.u8({0x8b,0x44,0x24,0x18,0x25});e.u32(0xff);e.u8({0x8b,0x04,0x85});e.u32(Textures);
            e.u8({0x8b,0x54,0x24,0x1c,0x89,0x02,0x31,0xc0});break;
        case 65: // SetTexture(stage, texture)
            e.u8({0x8b,0x44,0x24,0x18,0x25});e.u32(0xff);e.u8({0x8b,0x54,0x24,0x1c,0x89,0x14,0x85});e.u32(Textures);
            e.u8({0x31,0xc0});break;
        case 94:case 109: // Set{Vertex,Pixel}ShaderConstantF(start, data, count): data appended to the data log
            e.u8({0x8b,0x74,0x24,0x1c,0x8b,0x4c,0x24,0x20,0xc1,0xe1,0x02,0x8b,0x3d});e.u32(DataPtr);
            e.u8({0xf3,0xa5,0x89,0x3d});e.u32(DataPtr);e.u8({0x31,0xc0});break;
        case 49: // SetMaterial(material): the 17-word D3DMATERIAL9 appended to the data log
            e.u8({0x8b,0x74,0x24,0x18,0xb9,0x11,0x00,0x00,0x00,0x8b,0x3d});e.u32(DataPtr);
            e.u8({0xf3,0xa5,0x89,0x3d});e.u32(DataPtr);e.u8({0x31,0xc0});break;
        case 51: // SetLight(index, light): the 26-word D3DLIGHT9 appended to the data log
            e.u8({0x8b,0x74,0x24,0x1c,0xb9,0x1a,0x00,0x00,0x00,0x8b,0x3d});e.u32(DataPtr);
            e.u8({0xf3,0xa5,0x89,0x3d});e.u32(DataPtr);e.u8({0x31,0xc0});break;
        case 38: // GetRenderTarget(index, pp)
            e.u8({0x8b,0x54,0x24,0x1c,0xc7,0x02});e.u32(BackBuffer);e.u8({0x31,0xc0});break;
        case 40: // GetDepthStencilSurface(pp)
            e.u8({0x8b,0x54,0x24,0x18,0xc7,0x02});e.u32(DepthSurface);e.u8({0x31,0xc0});break;
        case 48: // GetViewport(p): 6 words from the viewport state
            e.u8({0x8b,0x7c,0x24,0x18,0xbe});e.u32(ViewportState);e.u8({0xb9,0x06,0,0,0,0xf3,0xa5,0x31,0xc0});break;
        case 47: // SetViewport(p): 6 words to the data log and the viewport state
            e.u8({0x8b,0x74,0x24,0x18,0xb9,0x06,0,0,0,0x8b,0x3d});e.u32(DataPtr);e.u8({0xf3,0xa5,0x89,0x3d});e.u32(DataPtr);
            e.u8({0x8b,0x74,0x24,0x18,0xbf});e.u32(ViewportState);e.u8({0xb9,0x06,0,0,0,0xf3,0xa5,0x31,0xc0});break;
        case 25: // CreateCubeTexture(Edge,Levels,Usage,Format,Pool,pp=arg6): cube object, +4 edge
            alloc_fixed(e,0x40,CubeVt);e.u8({0x8b,0x4c,0x24,0x18,0x89,0x48,0x04,0x8b,0x54,0x24,0x2c,0x89,0x02,0x31,0xc0});break;
        case 29: // CreateDepthStencilSurface(W,H,Format,MS,Quality,Discard,pp=arg7)
            alloc_fixed(e,0x40,TextureVt);e.u8({0x8b,0x54,0x24,0x30,0x89,0x02,0x31,0xc0});break;
        case 23: // CreateTexture(W,H,Levels,Usage,Format,Pool,pp=arg7)
            alloc_fixed(e,0x40,TextureVt);e.u8({0x8b,0x4c,0x24,0x18,0x89,0x48,0x04,0x8b,0x4c,0x24,0x1c,0x89,0x48,0x08,0x8b,0x54,0x24,0x30,0x89,0x02,0x31,0xc0});break;
        default:break;
        }
        epilogue(e,n);*reinterpret_cast<std::uint32_t*>(DeviceVt+slot*4)=place(e);
    }
    // Resource objects: IUnknown(0-2) + IDirect3DResource9(3-10) + type-specific.
    constexpr unsigned ResourceArgc[11]={3,1,1,2,5,4,2,2,1,1,1};
    for(unsigned slot=0;slot<14;++slot){ // buffers: Lock(11,5) Unlock(12,1) GetDesc(13,2)
        const unsigned n=slot<11?ResourceArgc[slot]:(slot==11?5u:slot==12?1u:2u);Emit e;prologue(e,0x1000u+slot,n);
        if(slot==11){e.u8({0x8b,0x44,0x24,0x14,0x83,0xc0,0x40,0x03,0x44,0x24,0x18,0x8b,0x54,0x24,0x20,0x89,0x02,0x31,0xc0});}
        if(slot==13){e.u8({0x8b,0x44,0x24,0x14,0x8b,0x40,0x04,0x8b,0x54,0x24,0x18,0x89,0x42,0x10,0x31,0xc0});}
        epilogue(e,n);*reinterpret_cast<std::uint32_t*>(BufferVt+slot*4)=place(e);
    }
    for(unsigned slot=0;slot<5;++slot){ // declarations: GetDevice(3,2) GetDeclaration(4,3)
        const unsigned n=slot<3?ResourceArgc[slot]:(slot==3?2u:3u);Emit e;prologue(e,0x2000u+slot,n);
        if(slot==4){
            // esi=elements; ecx=count incl. end marker (Stream 0xFF)
            e.u8({0x8b,0x74,0x24,0x14,0x83,0xc6,0x20,0x31,0xc9,
                  0x66,0x81,0x3c,0xce,0xff,0x00,0x74,0x03,0x41,0xeb,0xf5,0x41,
                  0x8b,0x54,0x24,0x1c,0x85,0xd2,0x74,0x02,0x89,0x0a,
                  0x8b,0x7c,0x24,0x18,0x85,0xff,0x74,0x04,0xd1,0xe1,0xf3,0xa5,0x31,0xc0});
        }
        epilogue(e,n);*reinterpret_cast<std::uint32_t*>(DeclVt+slot*4)=place(e);
    }
    for(unsigned slot=0;slot<5;++slot){const unsigned n=slot<3?ResourceArgc[slot]:(slot==3?2u:3u);Emit e;prologue(e,0x3000u+slot,n);epilogue(e,n);*reinterpret_cast<std::uint32_t*>(ShaderVt+slot*4)=place(e);}
    constexpr unsigned TextureArgc[20]={3,1,1,2,5,4,2,2,1,1,1,2,1,1,2,1,1,3,3,5};
    for(unsigned slot=0;slot<20;++slot){Emit e;prologue(e,0x4000u+slot,TextureArgc[slot]);
        if(slot==18){alloc_fixed(e,0x40,TextureVt);e.u8({0x8b,0x54,0x24,0x1c,0x89,0x02,0x31,0xc0});} // GetSurfaceLevel(level, pp)
        // GetLevelDesc(level, pDesc): Width/Height (+0x18/+0x1C) from the object's +4/+8.
        if(slot==17){e.u8({0x8b,0x44,0x24,0x14,0x8b,0x54,0x24,0x1c,0x8b,0x48,0x04,0x89,0x4a,0x18,0x8b,0x48,0x08,0x89,0x4a,0x1c,0x31,0xc0});}
        epilogue(e,TextureArgc[slot]);*reinterpret_cast<std::uint32_t*>(TextureVt+slot*4)=place(e);}
    // Cube textures: GetCubeMapSurface(face, level, pp) -> surface (+4 face, texels at +0x10).
    constexpr unsigned CubeArgc[22]={3,1,1,2,5,4,2,2,1,1,1,2,1,1,2,1,1,3,4,6,3,3};
    for(unsigned slot=0;slot<22;++slot){Emit e;prologue(e,0x6000u+slot,CubeArgc[slot]);
        if(slot==18){alloc_fixed(e,0x4010,SurfaceVt);e.u8({0x8b,0x4c,0x24,0x18,0x89,0x48,0x04,0x8b,0x54,0x24,0x20,0x89,0x02,0x31,0xc0});}
        epilogue(e,CubeArgc[slot]);*reinterpret_cast<std::uint32_t*>(CubeVt+slot*4)=place(e);}
    // Surfaces: LockRect(pLocked, pRect, Flags) -> {pitch 0x100, bits +0x10}.
    constexpr unsigned SurfaceArgc[17]={3,1,1,2,5,4,2,2,1,1,1,3,2,4,1,2,2};
    for(unsigned slot=0;slot<17;++slot){Emit e;prologue(e,0x7000u+slot,SurfaceArgc[slot]);
        if(slot==13){e.u8({0x8b,0x44,0x24,0x14,0x8d,0x48,0x10,0x8b,0x54,0x24,0x18,0xc7,0x02});e.u32(0x100);e.u8({0x89,0x4a,0x04,0x31,0xc0});}
        epilogue(e,SurfaceArgc[slot]);*reinterpret_cast<std::uint32_t*>(SurfaceVt+slot*4)=place(e);}
    // Faked D3DX file loaders (stdcall): textures keep their source pointer/size.
    auto loader=[&](std::uint32_t id,unsigned argc,unsigned pp_arg){
        Emit e;prologue(e,id,argc+1u); // no 'this'; treat arg0 as 'this' for logging symmetry
        alloc_fixed(e,0x40,TextureVt);
        e.u8({0x8b,0x4c,0x24,0x18,0x89,0x48,0x04,0x8b,0x4c,0x24,0x1c,0x89,0x48,0x08});
        e.u8({0x8b,0x54,0x24});e.b.push_back(std::uint8_t(0x14+pp_arg*4));e.u8({0x89,0x02,0x31,0xc0});
        e.u8({0x5a,0x59,0x5f,0x5e,0xc2});e.b.push_back(std::uint8_t(argc*4));e.b.push_back(0);
        return place(e);
    };
    const auto tex=loader(0x5001,15,14),cube=loader(0x5002,14,13);
    auto* iat=reinterpret_cast<void*>(0x596000u);
    if(mprotect(iat,4096,PROT_READ|PROT_WRITE))throw std::runtime_error("fake d3dx IAT");
    *reinterpret_cast<std::uint32_t*>(0x596290u)=tex;*reinterpret_cast<std::uint32_t*>(0x59628cu)=cube;
    if(mprotect(iat,4096,PROT_READ))throw std::runtime_error("fake d3dx IAT restore");
    for(std::uint32_t surface:{BackBuffer,DepthSurface,SurfaceA,SurfaceB})*reinterpret_cast<std::uint32_t*>(surface)=TextureVt;
    {const std::uint32_t v[6]{0,0,640,480,0,0x3f800000u};std::memcpy(reinterpret_cast<void*>(ViewportState),v,24);}
    if(mprotect(reinterpret_cast<void*>(0x89b000u),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("device pointer page");
    *reinterpret_cast<std::uint32_t*>(0x89bd60u)=Device;
    return Device;
}
inline std::uint32_t log_count(){return *reinterpret_cast<std::uint32_t*>(Log);}
inline const std::uint32_t* log_record(std::uint32_t k){return reinterpret_cast<const std::uint32_t*>(Log+0x10+k*0x30);}
inline void reset_log(){*reinterpret_cast<std::uint32_t*>(Log)=0;*reinterpret_cast<std::uint32_t*>(DataPtr)=Data;}
inline const float* data_log(){return reinterpret_cast<const float*>(Data);}
inline std::uint32_t data_log_words(){return (*reinterpret_cast<std::uint32_t*>(DataPtr)-Data)/4u;}
inline std::uint32_t heap_top(){return *reinterpret_cast<std::uint32_t*>(Heap);}
// Walks the fake objects allocated in [from, heap_top) in creation order.
template<class F> void for_each_object(std::uint32_t from,F&& f){
    for(std::uint32_t p=from;p<heap_top();){
        const auto vt=*reinterpret_cast<const std::uint32_t*>(p);
        std::uint32_t size;
        if(vt==BufferVt)size=(*reinterpret_cast<const std::uint32_t*>(p+4)+0x4fu)&~0xfu;
        else if(vt==DeclVt)size=0x140;
        else if(vt==ShaderVt)size=0x1020;
        else if(vt==TextureVt||vt==CubeVt)size=0x40;
        else if(vt==SurfaceVt)size=0x4010;
        else throw std::runtime_error("fake d3d9 heap walk");
        f(p,vt);p+=size;
    }
}
// Remap Windows SEH fs:[0] accesses inside [start,end) to a private TLS cell.
inline unsigned remap_fs0(std::uint32_t start,std::uint32_t end,std::uint32_t tls){
    unsigned patched=0;
    for(std::uint32_t a=start;a+7<=end;++a){
        const auto* p=reinterpret_cast<const std::uint8_t*>(a);
        if(p[0]!=0x64)continue;
        if((p[1]==0xa1||p[1]==0xa3)&&p[2]==0&&p[3]==0&&p[4]==0&&p[5]==0){
            std::uint8_t c[6]={0x90,p[1],0,0,0,0};std::memcpy(c+2,&tls,4);r060_raw_write(a,c,6);++patched;continue;}
        if((p[1]==0x89||p[1]==0x8b)&&(p[2]&0xc7)==0x05&&p[3]==0&&p[4]==0&&p[5]==0&&p[6]==0){
            std::uint8_t c[7]={0x90,p[1],p[2],0,0,0,0};std::memcpy(c+3,&tls,4);r060_raw_write(a,c,7);++patched;}
    }
    return patched;
}
}

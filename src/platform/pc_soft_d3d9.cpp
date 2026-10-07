#include <cstring>
#include "platform/pc_soft_d3d9.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
namespace outrun::platform {
using pc_shader::V4;
namespace {
enum Kind : std::uint32_t { KBuffer=1,KDecl,KVs,KPs,KTexture,KSurface };
float fbits(std::uint32_t u){float f;std::memcpy(&f,&u,4);return f;}
V4 lerp4(const V4& a,const V4& b,float t){return {a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t,a.w+(b.w-a.w)*t};}
float wrap_coord(float c,std::uint32_t mode,int size,bool& border){
    border=false;
    switch(mode){
    case 2:{float t=std::fmod(std::fabs(c),2.f);return t>1.f?2.f-t:t;}     // MIRROR
    case 3:return std::fmin(std::fmax(c,0.f),1.f);                        // CLAMP
    case 4:if(c<0.f||c>1.f)border=true;return c;                          // BORDER
    case 5:{float t=std::fabs(c);return std::fmin(t,1.f);}                 // MIRRORONCE
    default:return c-std::floor(c);                                       // WRAP
    }
    (void)size;
}
}
PcSoftD3D9Device::PcSoftD3D9Device(std::uint32_t w,std::uint32_t h):width_(w),height_(h),colour_(std::size_t(w)*h*4,0),depth_(std::size_t(w)*h,1.f),
    stencil_(std::size_t(w)*h,0){
    soft_viewport_={0,0,float(w),float(h),0.f,1.f};
    back_width_=w;back_height_=h;
}
void PcSoftD3D9Device::add_ref(std::uint32_t handle){if(auto* o=object(handle))++o->refs;}
std::uint32_t PcSoftD3D9Device::create_occlusion_query(){
    queries_.push_back({});return QueryBase+std::uint32_t(queries_.size()-1u);
}
void PcSoftD3D9Device::query_issue(std::uint32_t handle,std::uint32_t flags){
    if(handle<QueryBase||handle-QueryBase>=queries_.size())return;
    auto& q=queries_[handle-QueryBase];
    if(flags&2u){if(!q.active)++active_queries_;q.active=true;q.count=0;}
    if((flags&1u)&&q.active){q.active=false;--active_queries_;q.result=q.count;}   // drawing is immediate: ready at END
}
std::uint32_t PcSoftD3D9Device::query_get_data(std::uint32_t handle,std::uint32_t& samples){
    samples=0;
    if(handle<QueryBase||handle-QueryBase>=queries_.size())return 1u;
    const auto& q=queries_[handle-QueryBase];
    if(q.active)return 1u;
    samples=q.result;return 0u;
}
void PcSoftD3D9Device::release(std::uint32_t handle){
    auto* o=object(handle);if(!o||--o->refs)return;
    const auto parent=o->parent;
    if(parent){const auto key=(std::uint64_t(parent)<<16)|(o->face<<8)|o->level;surfaces_.erase(key);}
    // Never recycle handles: stale references must not acquire a new object.
    ++released_kinds[o->kind&15u];
    *o=Object{};o->refs=0;
    if(parent)release(parent);
}
std::size_t PcSoftD3D9Device::live_objects()const{
    return std::count_if(objects_.begin(),objects_.end(),[](const Object& o){return o.refs!=0;});
}
void PcSoftD3D9Device::rebind(std::uint32_t& slot,std::uint32_t handle){
    if(slot==handle)return;add_ref(handle);const auto old=slot;slot=handle;release(old);
}
void PcSoftD3D9Device::set_texture(std::uint32_t stage,std::uint32_t handle){if(stage<textures.size())rebind(textures[stage],handle);}
void PcSoftD3D9Device::set_vertex_declaration(std::uint32_t handle){rebind(declaration,handle);}
void PcSoftD3D9Device::set_vertex_shader(std::uint32_t handle){rebind(vertex_shader,handle);}
void PcSoftD3D9Device::set_pixel_shader(std::uint32_t handle){rebind(pixel_shader,handle);}
void PcSoftD3D9Device::set_indices(std::uint32_t handle){rebind(indices,handle);}
void PcSoftD3D9Device::set_stream_source(std::uint32_t stream,std::uint32_t handle,std::uint32_t offset,std::uint32_t stride){
    if(stream>=streams.size())return;rebind(streams[stream].buffer,handle);streams[stream].offset=offset;streams[stream].stride=stride;
}
void PcSoftD3D9Device::set_render_target(std::uint32_t index,std::uint32_t surface){
    if(index!=0u){errors.push_back("SetRenderTarget: only target 0");return;}
    if(surface==0u){errors.push_back("SetRenderTarget(0, NULL)");return;}
    Object* next=nullptr;std::vector<std::uint8_t>* next_pixels=nullptr;
    if(surface!=BackBuffer){
        next=object(surface);
        if(!next||next->kind!=KSurface||!next->parent){errors.push_back("SetRenderTarget: not a texture surface");return;}
        auto* t=object(next->parent);
        if(!t||next->face>=t->rgba.size()||next->level>=t->rgba[next->face].size()){errors.push_back("SetRenderTarget: surface without texels");return;}
        next_pixels=&t->rgba[next->face][next->level];
    }
    if(surface!=colour_target_){
        add_ref(surface);const auto old=colour_target_;
        if(colour_target_==BackBuffer)back_colour_.swap(colour_);
        else if(auto* o=object(colour_target_)){auto* t=object(o->parent);t->rgba[o->face][o->level].swap(colour_);}
        if(surface==BackBuffer){colour_.swap(back_colour_);width_=back_width_;height_=back_height_;}
        else{next_pixels->swap(colour_);width_=next->width;height_=next->height;}
        colour_target_=surface;++render_target_switches;
        release(old);
    }
    soft_viewport_={0,0,float(width_),float(height_),0.f,1.f};
}
void PcSoftD3D9Device::set_depth_stencil_surface(std::uint32_t surface){
    if(surface==depth_target_)return;
    Object* next=nullptr;
    if(surface!=DepthBuffer&&surface!=0u){
        next=object(surface);
        if(!next||next->kind!=KSurface||next->parent){errors.push_back("SetDepthStencilSurface: not a depth surface");return;}
        const std::size_t n=std::size_t(next->width)*next->height;
        if(next->depth.size()!=n){next->depth.assign(n,1.f);next->stencil.assign(n,0);}
    }
    add_ref(surface);const auto old=depth_target_;
    if(depth_target_==DepthBuffer){back_depth_.swap(depth_);back_stencil_.swap(stencil_);}
    else if(auto* o=object(depth_target_)){o->depth.swap(depth_);o->stencil.swap(stencil_);}
    if(surface==DepthBuffer){depth_.swap(back_depth_);stencil_.swap(back_stencil_);}
    else if(next){depth_.swap(next->depth);stencil_.swap(next->stencil);}
    depth_target_=surface;
    release(old);
}
void PcSoftD3D9Device::clear(std::uint32_t argb,float depth){
    for(std::size_t k=0;k<depth_.size();++k){
        colour_[k*4]=std::uint8_t(argb>>16);colour_[k*4+1]=std::uint8_t(argb>>8);colour_[k*4+2]=std::uint8_t(argb);colour_[k*4+3]=std::uint8_t(argb>>24);
        depth_[k]=depth;
    }
}
void PcSoftD3D9Device::clear(std::uint32_t flags,std::uint32_t colour,float z,std::uint32_t stencil){
    if(count_only)return;
    if(flags&1u)for(std::size_t k=0;k*4<colour_.size();++k){colour_[k*4]=std::uint8_t(colour>>16);colour_[k*4+1]=std::uint8_t(colour>>8);colour_[k*4+2]=std::uint8_t(colour);colour_[k*4+3]=std::uint8_t(colour>>24);}
    if(flags&2u)for(auto& d:depth_)d=z;
    if(flags&4u)for(auto& v:stencil_)v=std::uint8_t(stencil);
}
void PcSoftD3D9Device::get_viewport(std::uint32_t out[6]){
    const float f[6]{soft_viewport_.x,soft_viewport_.y,soft_viewport_.w,soft_viewport_.h,soft_viewport_.min_z,soft_viewport_.max_z};
    for(unsigned k=0;k<4;++k)out[k]=std::uint32_t(f[k]);
    std::memcpy(&out[4],&f[4],4);std::memcpy(&out[5],&f[5],4);
}
void PcSoftD3D9Device::set_viewport(const std::uint32_t v[6]){
    float mn,mx;std::memcpy(&mn,&v[4],4);std::memcpy(&mx,&v[5],4);
    set_viewport(v[0],v[1],v[2],v[3],mn,mx);
}
void PcSoftD3D9Device::set_viewport(std::uint32_t x,std::uint32_t y,std::uint32_t w,std::uint32_t h,float mn,float mx){soft_viewport_={float(x),float(y),float(w),float(h),mn,mx};}
std::uint32_t PcSoftD3D9Device::create_vertex_buffer(std::uint32_t n,std::uint32_t,std::uint32_t,std::uint32_t){
    Object o;o.kind=KBuffer;o.bytes.resize(n);objects_.push_back(std::move(o));return std::uint32_t(objects_.size());}
std::uint32_t PcSoftD3D9Device::create_index_buffer(std::uint32_t n,std::uint32_t,std::uint32_t,std::uint32_t){
    Object o;o.kind=KBuffer;o.bytes.resize(n);objects_.push_back(std::move(o));return std::uint32_t(objects_.size());}
std::uint8_t* PcSoftD3D9Device::lock(std::uint32_t h,std::uint32_t off,std::uint32_t size,std::uint32_t){
    auto* o=object(h);if(!o||o->kind!=KBuffer||off>o->bytes.size()||size>o->bytes.size()-off)return nullptr;return o->bytes.data()+off;}
std::uint32_t PcSoftD3D9Device::create_vertex_declaration(const PcVertexElement* e){
    Object o;o.kind=KDecl;for(;;++e){o.elements.push_back(*e);if(e->stream==PcDeclEndStream)break;}
    objects_.push_back(std::move(o));return std::uint32_t(objects_.size());}
std::uint32_t PcSoftD3D9Device::get_declaration(std::uint32_t h,PcVertexElement* out){
    auto* o=object(h);if(!o||o->kind!=KDecl)return 0;
    if(out)std::copy(o->elements.begin(),o->elements.end(),out);return std::uint32_t(o->elements.size());}
std::uint32_t PcSoftD3D9Device::create_vertex_shader(const std::uint32_t* t){
    Object o;o.kind=KVs;if(!pc_shader_split(t,o.program)){errors.push_back("vertex shader is not made of EXE blobs");return 0;}
    if(std::getenv("OR2_BLOB_DEBUG")){std::fprintf(stderr,"[vs] h=%zu blobs=%zu:",objects_.size()+1,o.program.blobs.size());for(auto b:o.program.blobs)std::fprintf(stderr," %u",b);std::fprintf(stderr,"\n");}
    objects_.push_back(std::move(o));return std::uint32_t(objects_.size());}
std::uint32_t PcSoftD3D9Device::create_pixel_shader(const std::uint32_t* t){
    Object o;o.kind=KPs;if(!pc_shader_split(t,o.program)){errors.push_back("pixel shader is not made of EXE blobs");return 0;}
    if(std::getenv("OR2_BLOB_DEBUG")){std::fprintf(stderr,"[ps] h=%zu ver=%x:",objects_.size()+1,o.program.version);for(auto b:o.program.blobs)std::fprintf(stderr," %u",b);std::fprintf(stderr,"\n");}
    objects_.push_back(std::move(o));return std::uint32_t(objects_.size());}
std::uint32_t PcSoftD3D9Device::create_texture_from_file(const PcTextureFileRequest& q){
    Object o;o.kind=KTexture;std::string error;
    if(!pc_d3dx_texture_from_file(q,o.texture,error)){errors.push_back(error);return 0;}
    for(const auto& f:o.texture.faces)for(const auto& l:f)texture_bytes+=l.data.size();
    o.rgba.resize(o.texture.faces.size());
    for(unsigned f=0;f<o.texture.faces.size();++f)for(unsigned l=0;l<o.texture.faces[f].size();++l)o.rgba[f].push_back(pc_dds_rgba(o.texture,f,l));
    objects_.push_back(std::move(o));return std::uint32_t(objects_.size());}
bool PcSoftD3D9Device::shade_vertex(std::uint32_t index,std::int32_t base,Vertex& out){
    auto* decl=object(declaration);auto* vs=object(vertex_shader);
    if(!decl||!vs||vs->kind!=KVs)return false;
    pc_shader::VsState s;std::array<V4,256> c;
    for(unsigned k=0;k<256;++k)c[k]={vs_constants[k][0],vs_constants[k][1],vs_constants[k][2],vs_constants[k][3]};
    s.C=c.data();
    for(auto& v:s.V)v={0,0,0,1};
    for(const auto& e:decl->elements){
        if(e.stream==PcDeclEndStream)break;
        const auto& st=streams.at(e.stream);auto* b=object(st.buffer);if(!b)return false;
        const std::size_t at=std::size_t(st.offset)+std::size_t(std::int64_t(base)+index)*st.stride+e.offset;
        V4 v{0,0,0,1};
        auto rd=[&](unsigned k){float f;std::memcpy(&f,b->bytes.data()+at+k*4,4);return f;};
        const unsigned need=e.type<=3?4u*(e.type+1u):4u;
        if(at+need>b->bytes.size())return false;
        switch(e.type){
        case 0:v={rd(0),0,0,1};break;case 1:v={rd(0),rd(1),0,1};break;case 2:v={rd(0),rd(1),rd(2),1};break;case 3:v={rd(0),rd(1),rd(2),rd(3)};break;
        case 4:{const auto* p=b->bytes.data()+at;v={p[2]/255.f,p[1]/255.f,p[0]/255.f,p[3]/255.f};break;}
        case 5:{const auto* p=b->bytes.data()+at;v={float(p[0]),float(p[1]),float(p[2]),float(p[3])};break;}
        default:break;
        }
        for(const auto& in:vs->program.inputs)if(in.usage==e.usage&&in.usage_index==e.usage_index)s.V[in.reg]=v;
    }
    pc_shader_run_vs(vs->program,s);
    std::memcpy(out.pos,&s.oPos,16);
    out.d[0]=pc_shader::sat(s.oD[0]);out.d[1]=pc_shader::sat(s.oD[1]);
    for(unsigned k=0;k<8;++k)out.t[k]=s.oT[k];
    out.fog=pc_shader::clamp01(s.oFog.x);
    return true;
}
std::uint32_t PcSoftD3D9Device::draw_indexed_primitive(std::uint32_t type,std::int32_t base,std::uint32_t min_index,
    std::uint32_t vertices,std::uint32_t start,std::uint32_t count){
    (void)min_index;(void)vertices;
    auto* ib=object(indices);auto* ps=object(pixel_shader);
    if(!ib)return 1;
    ps_=ps&&ps->kind==KPs?&ps->program:nullptr;
    auto index=[&](std::uint32_t k){std::uint16_t v;std::memcpy(&v,ib->bytes.data()+std::size_t(k)*2,2);return v;};
    std::map<std::uint32_t,Vertex> cache;
    auto vertex=[&](std::uint32_t i)->const Vertex*{
        auto it=cache.find(i);if(it!=cache.end())return &it->second;
        Vertex v{};if(!shade_vertex(i,base,v))return nullptr;return &cache.emplace(i,v).first->second;};
    const std::size_t total=type==5?std::size_t(count)+2:type==4?std::size_t(count)*3:0;
    if(!total||(std::size_t(start)+total)*2>ib->bytes.size())return 1;
    note_dip_state(count);
    if(hash_draws)hash_draw(1,{type,std::uint32_t(base),min_index,vertices,start,count});
    if(count_only)return 0;
    if(!targets_consistent()){errors.push_back("draw with the depth surface smaller than the render target");return 1;}
    // Host debugging aids: OR2_DBG_DRAWSTATS prints one line per draw (triangles,
    // pixels, states, NDC bounds, vertex constants of large draws);
    // OR2_DBG_ONLY=lo-hi rasterises only the draws with that running index.
    static const bool dbg_stats=std::getenv("OR2_DBG_DRAWSTATS")!=nullptr;
    const auto dbg_t0=triangles_drawn;const auto dbg_p0=pixels_shaded;const auto dbg_z0=pixels_depth_rejected;
    static unsigned dbg_index=0;const unsigned dbg_me=dbg_index++;
    static const char* dbg_only=std::getenv("OR2_DBG_ONLY");
    if(dbg_only){ // comma-separated ranges lo-hi
        bool keep=false;
        for(const char* p=dbg_only;*p;){unsigned lo=0,hi=0;int n=0;
            if(std::sscanf(p,"%u-%u%n",&lo,&hi,&n)<2)break;
            if(dbg_me>=lo&&dbg_me<=hi)keep=true;
            p+=n;if(*p==',')++p;}
        if(!keep)return 0;}
    float bmin[3]{1e30f,1e30f,1e30f},bmax[3]{-1e30f,-1e30f,-1e30f};std::uint32_t culled_w=0;
    for(std::uint32_t p=0;p<count;++p){
        std::uint32_t a,b,c;
        if(type==5){a=index(start+p);b=index(start+p+1);c=index(start+p+2);if(p&1)std::swap(a,b);}
        else{a=index(start+p*3);b=index(start+p*3+1);c=index(start+p*3+2);}
        const Vertex* va=vertex(a);const Vertex* vb=vertex(b);const Vertex* vc=vertex(c);
        if(!va||!vb||!vc)return 1;
        if(dbg_stats)for(const Vertex* q:{va,vb,vc}){if(q->pos[3]<=0.f){++culled_w;continue;}for(unsigned k=0;k<3;++k){const float v=q->pos[k]/q->pos[3];bmin[k]=std::fmin(bmin[k],v);bmax[k]=std::fmax(bmax[k],v);}}
        triangle(*va,*vb,*vc);
    }
    if(dbg_stats)std::fprintf(stderr,"DRAW#%u vs=%u ps=%u type=%u count=%u tri=%llu px=%llu zrej=%llu zen=%u cull=%u zf=%u zw=%u ab=%u at=%u cm=%u ndc=[%.2f,%.2f]x[%.2f,%.2f]z[%.3f,%.3f] w<=0:%u\n",
        dbg_me,vertex_shader,pixel_shader,type,count,(unsigned long long)(triangles_drawn-dbg_t0),(unsigned long long)(pixels_shaded-dbg_p0),(unsigned long long)(pixels_depth_rejected-dbg_z0),render[d3d9::RS_ZENABLE],
        render[d3d9::RS_CULLMODE],render[d3d9::RS_ZFUNC],render[d3d9::RS_ZWRITEENABLE],render[d3d9::RS_ALPHABLENDENABLE],render[d3d9::RS_ALPHATESTENABLE],render[d3d9::RS_COLORWRITEENABLE],
        bmin[0],bmax[0],bmin[1],bmax[1],bmin[2],bmax[2],culled_w);
    if(dbg_stats&&count>=700){for(unsigned r=0;r<96;++r)if(vs_constants[r][0]!=0.f||vs_constants[r][1]!=0.f||vs_constants[r][2]!=0.f||vs_constants[r][3]!=0.f)std::fprintf(stderr,"  c%u=(%.3f %.3f %.3f %.3f)",r,vs_constants[r][0],vs_constants[r][1],vs_constants[r][2],vs_constants[r][3]);std::fprintf(stderr,"\n");}
    return 0;
}
void PcSoftD3D9Device::triangle(const Vertex& a,const Vertex& b,const Vertex& c){
    // Clip against z >= 0 and z <= w (D3D clip volume depth range).
    std::vector<Vertex> poly{a,b,c};
    auto lerpv=[](const Vertex& p,const Vertex& q,float t){Vertex r;
        for(unsigned k=0;k<4;++k)r.pos[k]=p.pos[k]+(q.pos[k]-p.pos[k])*t;
        for(unsigned k=0;k<2;++k)r.d[k]=lerp4(p.d[k],q.d[k],t);for(unsigned k=0;k<8;++k)r.t[k]=lerp4(p.t[k],q.t[k],t);
        r.fog=p.fog+(q.fog-p.fog)*t;return r;};
    for(int plane=0;plane<2;++plane){
        std::vector<Vertex> out;
        auto dist=[&](const Vertex& v){return plane==0?v.pos[2]:v.pos[3]-v.pos[2];};
        for(std::size_t k=0;k<poly.size();++k){
            const Vertex& p=poly[k];const Vertex& q=poly[(k+1)%poly.size()];
            const float dp=dist(p),dq=dist(q);
            if(dp>=0.f)out.push_back(p);
            if((dp>=0.f)!=(dq>=0.f))out.push_back(lerpv(p,q,dp/(dp-dq)));
        }
        poly.swap(out);if(poly.size()<3)return;
    }
    for(const auto& v:poly)if(!(v.pos[3]>1e-6f))return;
    raster(poly.data(),unsigned(poly.size()));
}
void PcSoftD3D9Device::raster(const Vertex* v,unsigned n){
    struct S { float x,y,z,iw; };
    std::vector<S> s(n);
    for(unsigned k=0;k<n;++k){
        if(pretransformed_){s[k]={v[k].pos[0],v[k].pos[1],v[k].pos[2],v[k].pos[3]};continue;}
        const float iw=1.f/v[k].pos[3];
        s[k]={(v[k].pos[0]*iw+1.f)*0.5f*soft_viewport_.w+soft_viewport_.x,(1.f-v[k].pos[1]*iw)*0.5f*soft_viewport_.h+soft_viewport_.y,
              soft_viewport_.min_z+v[k].pos[2]*iw*(soft_viewport_.max_z-soft_viewport_.min_z),iw};
    }
    // Pixel bounds: the render target; pretransformed vertices are also
    // clipped to the viewport rectangle.
    int bx0=0,by0=0,bx1=int(width_)-1,by1=int(height_)-1;
    if(pretransformed_){
        bx0=std::max(bx0,int(soft_viewport_.x));by0=std::max(by0,int(soft_viewport_.y));
        bx1=std::min(bx1,int(soft_viewport_.x+soft_viewport_.w)-1);by1=std::min(by1,int(soft_viewport_.y+soft_viewport_.h)-1);
    }
    const auto cull=render[d3d9::RS_CULLMODE];
    const float bias=d3d9::f(render[d3d9::RS_DEPTHBIAS]);
    for(unsigned t=1;t+1<n;++t){
        const unsigned i0=0,i1=t,i2=t+1;
        float area=(s[i1].x-s[i0].x)*(s[i2].y-s[i0].y)-(s[i2].x-s[i0].x)*(s[i1].y-s[i0].y);
        if(area==0.f)continue;
        if(cull==d3d9::CULL_CCW&&area<0.f)continue;
        if(cull==d3d9::CULL_CW&&area>0.f)continue;
        unsigned o[3]{i0,i1,i2};if(area<0.f){std::swap(o[1],o[2]);area=-area;}
        const S& A=s[o[0]];const S& B=s[o[1]];const S& C=s[o[2]];
        ++triangles_drawn;
        const int x0=std::max(bx0,int(std::ceil(std::min({A.x,B.x,C.x})))),x1=std::min(bx1,int(std::floor(std::max({A.x,B.x,C.x}))));
        const int y0=std::max(by0,int(std::ceil(std::min({A.y,B.y,C.y})))),y1=std::min(by1,int(std::floor(std::max({A.y,B.y,C.y}))));
        auto edge=[](const S& p,const S& q,float x,float y){return (q.x-p.x)*(y-p.y)-(q.y-p.y)*(x-p.x);};
        auto topleft=[](const S& p,const S& q){const float dx=q.x-p.x,dy=q.y-p.y;return dy<0.f||(dy==0.f&&dx>0.f);};
        const bool tl0=topleft(B,C),tl1=topleft(C,A),tl2=topleft(A,B);
        auto bary=[&](float x,float y,float& l0,float& l1,float& l2){l0=edge(B,C,x,y)/area;l1=edge(C,A,x,y)/area;l2=edge(A,B,x,y)/area;};
        auto attr=[&](float l0,float l1,float l2,auto get)->V4{
            const float w0=l0*A.iw,w1=l1*B.iw,w2=l2*C.iw,iw=w0+w1+w2;
            const V4 a=get(v[o[0]]),b=get(v[o[1]]),c=get(v[o[2]]);
            return {(a.x*w0+b.x*w1+c.x*w2)/iw,(a.y*w0+b.y*w1+c.y*w2)/iw,(a.z*w0+b.z*w1+c.z*w2)/iw,(a.w*w0+b.w*w1+c.w*w2)/iw};};
        for(int py=y0;py<=y1;++py)for(int px=x0;px<=x1;++px){
            const float x=float(px),y=float(py);
            const float e0=edge(B,C,x,y),e1=edge(C,A,x,y),e2=edge(A,B,x,y);
            if(e0<0.f||e1<0.f||e2<0.f)continue;
            if((e0==0.f&&!tl0)||(e1==0.f&&!tl1)||(e2==0.f&&!tl2))continue;
            float l0,l1,l2;bary(x,y,l0,l1,l2);
            float z=l0*A.z+l1*B.z+l2*C.z+bias;
            const float iw=l0*A.iw+l1*B.iw+l2*C.iw;
            // Varyings and their screen gradients (texture LOD).
            V4 d0=attr(l0,l1,l2,[](const Vertex& q){return q.d[0];}),d1=attr(l0,l1,l2,[](const Vertex& q){return q.d[1];});
            std::array<V4,8> tc,tx,ty;float gx0,gx1,gx2,gy0,gy1,gy2;bary(x+1.f,y,gx0,gx1,gx2);bary(x,y+1.f,gy0,gy1,gy2);
            for(unsigned k=0;k<8;++k){auto get=[k](const Vertex& q){return q.t[k];};tc[k]=attr(l0,l1,l2,get);tx[k]=attr(gx0,gx1,gx2,get);ty[k]=attr(gy0,gy1,gy2,get);}
            const float fog_v=[&]{const float w0=l0*A.iw,w1=l1*B.iw,w2=l2*C.iw;return (v[o[0]].fog*w0+v[o[1]].fog*w1+v[o[2]].fog*w2)/iw;}();
            // Pixel shader.
            V4 colour{};
            pc_shader::PsSampling io;
            io.sample=[&](int stage,const V4& coord){const int k=std::clamp(stage,0,7);
                return sample(stage,coord,tx[std::size_t(k)].x-tc[std::size_t(k)].x,tx[std::size_t(k)].y-tc[std::size_t(k)].y,
                              ty[std::size_t(k)].x-tc[std::size_t(k)].x,ty[std::size_t(k)].y-tc[std::size_t(k)].y);};
            io.bump=[&](int st){return std::array<float,4>{fbits(stage[std::size_t(st)][7]),fbits(stage[std::size_t(st)][8]),fbits(stage[std::size_t(st)][9]),fbits(stage[std::size_t(st)][10])};};
            io.luminance=[&](int st){return std::array<float,2>{fbits(stage[std::size_t(st)][22]),fbits(stage[std::size_t(st)][23])};};
            std::array<V4,8> pc;for(unsigned k=0;k<8;++k)pc[k]={ps_constants[k][0],ps_constants[k][1],ps_constants[k][2],ps_constants[k][3]};
            if(ps_&&ps_->version==0xffff0104u){
                pc_shader::Ps14State st;st.io=&io;st.PC=pc.data();for(unsigned k=0;k<6;++k)st.T[k]=tc[k];st.VC[0]=d0;st.VC[1]=d1;
                pc_shader_run_ps14(*ps_,st);colour=st.R[0];
            }else if(ps_&&ps_->version==0xffff0101u){
                pc_shader::Ps11State st;st.io=&io;st.PC=pc.data();for(unsigned k=0;k<4;++k)st.TC[k]=tc[k];st.VC[0]=d0;st.VC[1]=d1;
                pc_shader_run_ps11(*ps_,st);colour=st.R[0];
            }else{
                // Fixed function (SetPixelShader(NULL)): texture stage cascade.
                colour=fixed_function(d0,d1,tc,tx,ty);
            }
            ++pixels_shaded;
            colour=pc_shader::sat(colour);
            if(render[d3d9::RS_ALPHATESTENABLE]){
                const std::uint32_t a=std::uint32_t(std::lround(colour.w*255.f)),ref=render[d3d9::RS_ALPHAREF]&0xffu;
                bool pass=true;switch(render[d3d9::RS_ALPHAFUNC]){case 1:pass=false;break;case 2:pass=a<ref;break;case 3:pass=a==ref;break;
                    case 4:pass=a<=ref;break;case 5:pass=a>ref;break;case 6:pass=a!=ref;break;case 7:pass=a>=ref;break;default:break;}
                if(!pass)continue;
            }
            if(render[d3d9::RS_FOGENABLE]){
                float f=fog_v;const float dist=1.f/iw;
                switch(render[d3d9::RS_FOGTABLEMODE]){
                case 1:f=std::exp(-d3d9::f(render[d3d9::RS_FOGDENSITY])*dist);break;
                case 2:{const float k=d3d9::f(render[d3d9::RS_FOGDENSITY])*dist;f=std::exp(-k*k);break;}
                case 3:{const float s0=d3d9::f(render[d3d9::RS_FOGSTART]),s1=d3d9::f(render[d3d9::RS_FOGEND]);f=(s1-dist)/(s1-s0);break;}
                default:break;
                }
                f=pc_shader::clamp01(f);const auto fc=render[d3d9::RS_FOGCOLOR];
                colour.x=((fc>>16)&255)/255.f+(colour.x-((fc>>16)&255)/255.f)*f;
                colour.y=((fc>>8)&255)/255.f+(colour.y-((fc>>8)&255)/255.f)*f;
                colour.z=(fc&255)/255.f+(colour.z-(fc&255)/255.f)*f;
            }
            const std::size_t at=std::size_t(py)*width_+std::size_t(px);
            // Stencil (RS 52..59): (ref & mask) FUNC (stencil & mask), then
            // FAIL / ZFAIL / PASS operations through the write mask.
            const bool stencil_on=render[52]!=0u;
            auto stencil_op=[&](std::uint32_t op){
                std::uint32_t v=stencil_[at];
                switch(op){case 2:v=0;break;case 3:v=render[57]&0xffu;break;case 4:v=v<255u?v+1u:255u;break;case 5:v=v>0u?v-1u:0u;break;
                    case 6:v=~v&0xffu;break;case 7:v=(v+1u)&0xffu;break;case 8:v=(v-1u)&0xffu;break;default:return;}
                const std::uint32_t wm=render[59]&0xffu;
                stencil_[at]=std::uint8_t((stencil_[at]&~wm)|(v&wm));};
            if(stencil_on){
                const std::uint32_t mask=render[58]&0xffu,ref=render[57]&mask,s=stencil_[at]&mask;bool pass=true;
                switch(render[56]){case 1:pass=false;break;case 2:pass=ref<s;break;case 3:pass=ref==s;break;case 4:pass=ref<=s;break;
                    case 5:pass=ref>s;break;case 6:pass=ref!=s;break;case 7:pass=ref>=s;break;default:break;}
                if(!pass){stencil_op(render[53]);continue;}
            }
            if(render[d3d9::RS_ZENABLE]){
                z=std::fmin(std::fmax(z,0.f),1.f);const float d=depth_[at];bool pass=true;
                switch(render[d3d9::RS_ZFUNC]){case 1:pass=false;break;case 2:pass=z<d;break;case 3:pass=z==d;break;case 4:pass=z<=d;break;
                    case 5:pass=z>d;break;case 6:pass=z!=d;break;case 7:pass=z>=d;break;default:break;}
                if(!pass){if(stencil_on)stencil_op(render[54]);++pixels_depth_rejected;continue;}
                if(stencil_on)stencil_op(render[55]);
                if(render[d3d9::RS_ZWRITEENABLE])depth_[at]=z;
            }else if(stencil_on)stencil_op(render[55]);
            if(active_queries_)for(auto& q:queries_)if(q.active)++q.count;   // occlusion: samples past depth/stencil
            std::uint8_t* px_out=colour_.data()+at*4;
            V4 dst{px_out[0]/255.f,px_out[1]/255.f,px_out[2]/255.f,px_out[3]/255.f};
            V4 res=colour;
            if(render[d3d9::RS_ALPHABLENDENABLE]){
                auto factor=[&](std::uint32_t f)->V4{
                    switch(f){case 1:return {0,0,0,0};case 2:return {1,1,1,1};case 3:return colour;case 4:return {1-colour.x,1-colour.y,1-colour.z,1-colour.w};
                        case 5:return pc_shader::v4(colour.w);case 6:return pc_shader::v4(1-colour.w);case 7:return pc_shader::v4(dst.w);case 8:return pc_shader::v4(1-dst.w);
                        case 9:return dst;case 10:return {1-dst.x,1-dst.y,1-dst.z,1-dst.w};
                        case 11:{const float k=std::fmin(colour.w,1-dst.w);return {k,k,k,1};}
                        default:return {1,1,1,1};}};
                const V4 fs=factor(render[d3d9::RS_SRCBLEND]),fd=factor(render[d3d9::RS_DESTBLEND]);
                const V4 a=pc_shader::mul(colour,fs),b=pc_shader::mul(dst,fd);
                switch(render[d3d9::RS_BLENDOP]){case 2:res=pc_shader::sub(a,b);break;case 3:res=pc_shader::sub(b,a);break;
                    case 4:res=pc_shader::min(colour,dst);break;case 5:res=pc_shader::max(colour,dst);break;default:res=pc_shader::add(a,b);break;}
                res=pc_shader::sat(res);
            }
            const auto mask=render[d3d9::RS_COLORWRITEENABLE];
            for(unsigned k=0;k<4;++k)if(mask>>k&1u)px_out[k]=std::uint8_t(std::lround(res[k]*255.f));
        }
    }
}
V4 PcSoftD3D9Device::fixed_function(const V4& diffuse,const V4& specular,const std::array<V4,8>& tc,
                                    const std::array<V4,8>& tx,const std::array<V4,8>& ty){
    // D3D9 texture stage cascade (TSS 1..6 COLOROP/ARG1/ARG2/ALPHAOP/ARG1/ARG2,
    // 26/27 ARG0, 28 RESULTARG, 11 TEXCOORDINDEX, 32 CONSTANT). A stage whose
    // COLOROP is DISABLE ends the cascade. D3DTA_TEXTURE of a stage without a
    // texture reads opaque white (as the previous stage-0 fallback did).
    // Bump-mapping ops and PREMODULATE are not modelled (they pass ARG1).
    auto col=[](std::uint32_t c){return V4{((c>>16)&255)/255.f,((c>>8)&255)/255.f,(c&255)/255.f,(c>>24)/255.f};};
    const V4 tfactor=col(render[d3d9::RS_TEXTUREFACTOR]);
    V4 current=diffuse,temp{};
    for(unsigned s=0;s<8;++s){
        const auto& ts=stage[s];
        const std::uint32_t cop=ts[1],aop=ts[4];
        if(cop==1u||cop==0u)break;
        bool have_tex=false;V4 tex{1,1,1,1};
        auto texture=[&]()->V4{
            if(!have_tex){have_tex=true;
                if(textures[s]){const std::size_t k=ts[11]&7u;
                    tex=sample(int(s),tc[k],tx[k].x-tc[k].x,tx[k].y-tc[k].y,ty[k].x-tc[k].x,ty[k].y-tc[k].y);}}
            return tex;};
        auto arg=[&](std::uint32_t a)->V4{
            V4 r;
            switch(a&0xfu){case 0:r=diffuse;break;case 1:r=current;break;case 2:r=texture();break;case 3:r=tfactor;break;
                case 4:r=specular;break;case 5:r=temp;break;case 6:r=col(ts[32]);break;default:r=current;break;}
            if(a&0x10u)r={1-r.x,1-r.y,1-r.z,1-r.w};
            if(a&0x20u)r=pc_shader::v4(r.w);
            return r;};
        auto op=[&](std::uint32_t o,std::uint32_t a0,std::uint32_t a1,std::uint32_t a2)->V4{
            using namespace pc_shader;
            auto blend=[&](float k){const V4 p=arg(a1),q=arg(a2);return add(mul(p,v4(k)),mul(q,v4(1-k)));};
            switch(o){
            case 3:return arg(a2);                                                   // SELECTARG2
            case 4:return mul(arg(a1),arg(a2));                                      // MODULATE
            case 5:return mul(mul(arg(a1),arg(a2)),v4(2));                           // MODULATE2X
            case 6:return mul(mul(arg(a1),arg(a2)),v4(4));                           // MODULATE4X
            case 7:return add(arg(a1),arg(a2));                                      // ADD
            case 8:return sub(add(arg(a1),arg(a2)),v4(0.5f));                        // ADDSIGNED
            case 9:return mul(sub(add(arg(a1),arg(a2)),v4(0.5f)),v4(2));             // ADDSIGNED2X
            case 10:return sub(arg(a1),arg(a2));                                     // SUBTRACT
            case 11:{const V4 p=arg(a1),q=arg(a2);return sub(add(p,q),mul(p,q));}   // ADDSMOOTH
            case 12:return blend(diffuse.w);                                         // BLENDDIFFUSEALPHA
            case 13:return blend(texture().w);                                       // BLENDTEXTUREALPHA
            case 14:return blend(tfactor.w);                                         // BLENDFACTORALPHA
            case 15:{const V4 p=arg(a1),q=arg(a2);return add(p,mul(q,v4(1-texture().w)));} // BLENDTEXTUREALPHAPM
            case 16:return blend(current.w);                                         // BLENDCURRENTALPHA
            case 18:{const V4 p=arg(a1),q=arg(a2);return add(p,mul(v4(p.w),q));}     // MODULATEALPHA_ADDCOLOR
            case 19:{const V4 p=arg(a1),q=arg(a2);return add(mul(p,q),v4(p.w));}     // MODULATECOLOR_ADDALPHA
            case 20:{const V4 p=arg(a1),q=arg(a2);return add(p,mul(v4(1-p.w),q));}   // MODULATEINVALPHA_ADDCOLOR
            case 21:{const V4 p=arg(a1),q=arg(a2);return add(mul(sub(v4(1),p),q),v4(p.w));} // MODULATEINVCOLOR_ADDALPHA
            case 24:{const V4 p=sub(arg(a1),v4(0.5f)),q=sub(arg(a2),v4(0.5f));return v4(4*(p.x*q.x+p.y*q.y+p.z*q.z));} // DOTPRODUCT3
            case 25:return add(arg(a0),mul(arg(a1),arg(a2)));                        // MULTIPLYADD
            case 26:{const V4 k=arg(a0);return add(mul(k,arg(a1)),mul(sub(v4(1),k),arg(a2)));} // LERP
            default:return arg(a1);                                                  // SELECTARG1, unmodelled ops
            }};
        V4 r=pc_shader::sat(op(cop,ts[26],ts[2],ts[3]));
        if(aop==1u||aop==0u)r.w=current.w;                     // alpha DISABLE: pass current alpha
        else if(cop==24u)r.w=r.x;                              // DOTPRODUCT3 replicates into alpha
        else r.w=pc_shader::clamp01(op(aop,ts[27],ts[5],ts[6]).w);
        if(ts[28]==5u)temp=r;else current=r;
    }
    if(render[d3d9::RS_SPECULARENABLE]){current.x+=specular.x;current.y+=specular.y;current.z+=specular.z;}
    return current;
}
void PcSoftD3D9Device::hash_draw(std::uint32_t kind,std::initializer_list<std::uint32_t> args){
    std::uint64_t h=draw_hash;
    auto mix=[&h](std::uint32_t v){h=(h^v)*0x100000001b3ull;};
    auto words=[&](const void* p,std::size_t bytes){const auto* b=static_cast<const std::uint8_t*>(p);
        for(std::size_t k=0;k+4<=bytes;k+=4){std::uint32_t v;std::memcpy(&v,b+k,4);mix(v);}};
    mix(kind);for(auto a:args)mix(a);
    words(render.data(),sizeof render);words(sampler.data(),sizeof sampler);words(stage.data(),sizeof stage);
    words(textures.data(),sizeof textures);words(streams.data(),sizeof streams);
    mix(indices);mix(declaration);mix(vertex_shader);mix(pixel_shader);mix(fvf_);
    words(vs_constants.data(),sizeof vs_constants);words(ps_constants.data(),sizeof ps_constants);
    draw_hash=h;++draws_hashed;
    static const long dump=std::getenv("OR2_HOST_DRAWHASH_DUMP")?std::atol(std::getenv("OR2_HOST_DRAWHASH_DUMP")):-1;
    if(dump>=0&&long(dump_frame)==dump){
        auto part=[](const void* p,std::size_t n){std::uint64_t x=0xcbf29ce484222325ull;const auto* b=static_cast<const std::uint8_t*>(p);
            for(std::size_t k=0;k<n;++k)x=(x^b[k])*0x100000001b3ull;return (unsigned long long)(x&0xffffffu);};
        std::fprintf(stderr,"dh %llu k%u rs=%06llx samp=%06llx tss=%06llx tex=%06llx str=%06llx sh=%x/%x/%x/%x vsc=%06llx psc=%06llx",
            (unsigned long long)draws_hashed,kind,part(render.data(),sizeof render),part(sampler.data(),sizeof sampler),part(stage.data(),sizeof stage),
            part(textures.data(),sizeof textures),part(streams.data(),sizeof streams),indices,declaration,vertex_shader,pixel_shader,
            part(vs_constants.data(),sizeof vs_constants),part(ps_constants.data(),sizeof ps_constants));
        for(auto a:args)std::fprintf(stderr," %x",a);
        std::fprintf(stderr,"\n");
    }
}
void PcSoftD3D9Device::draw_primitive_up(std::uint32_t type,std::uint32_t count,const void* data,std::uint32_t stride){
    const std::uint32_t fvf=fvf_;
    if(hash_draws&&data&&count){
        hash_draw(2,{type,count,stride});
        const auto* b=static_cast<const std::uint8_t*>(data);
        const std::size_t n=(type==4?std::size_t(count)*3:type==5||type==6?std::size_t(count)+2:type==1?count:type==2?std::size_t(count)*2:type==3?std::size_t(count)+1:0)*stride;
        for(std::size_t k=0;k+4<=n;k+=4){std::uint32_t v;std::memcpy(&v,b+k,4);draw_hash=(draw_hash^v)*0x100000001b3ull;}
        static const long dump=std::getenv("OR2_HOST_DRAWHASH_DUMP")?std::atol(std::getenv("OR2_HOST_DRAWHASH_DUMP")):-1;
        if(dump>=0&&long(dump_frame)==dump)std::fprintf(stderr,"dh up fvf=%x data_hash=%016llx\n",fvf,(unsigned long long)draw_hash);
    }
    if(!data||!count||count_only)return;
    if(!targets_consistent()){errors.push_back("draw with the depth surface smaller than the render target");return;}
    const bool rhw=(fvf&0x400eu)==0x004u,xyz=(fvf&0x400eu)==0x002u;
    if(!rhw&&!xyz){errors.push_back("draw_primitive_up: only XYZRHW / XYZ FVFs are supported");return;}
    std::size_t total=0;
    switch(type){case 4:total=std::size_t(count)*3;break;case 5:case 6:total=std::size_t(count)+2;break;
        default:errors.push_back("draw_primitive_up: unsupported primitive type");return;}
    // FVF layout: position (16), [normal 12], [psize 4], [diffuse], [specular], texture sets.
    constexpr std::size_t none=~std::size_t(0);
    std::size_t off=rhw?16:12;if(fvf&0x010u)off+=12;if(fvf&0x020u)off+=4;
    const auto wvp=transform_wvp();
    const std::size_t diffuse_at=fvf&0x040u?off:none;if(fvf&0x040u)off+=4;
    const std::size_t specular_at=fvf&0x080u?off:none;if(fvf&0x080u)off+=4;
    const unsigned sets=std::min(8u,(fvf>>8)&0xfu);std::size_t tex_at[8]{};unsigned tex_n[8]{};
    for(unsigned k=0;k<sets;++k){const unsigned f=(fvf>>(16+2*k))&3u;tex_n[k]=f==0?2:f==1?3:f==2?4:1;tex_at[k]=off;off+=4u*tex_n[k];}
    if(stride<off){errors.push_back("draw_primitive_up: stride smaller than the FVF vertex");return;}
    const auto* bytes=static_cast<const std::uint8_t*>(data);
    up_vertices_.resize(total);
    auto rdf=[](const std::uint8_t* p){float f;std::memcpy(&f,p,4);return f;};
    auto colour=[](const std::uint8_t* p){return V4{p[2]/255.f,p[1]/255.f,p[0]/255.f,p[3]/255.f};}; // D3DCOLOR bytes: B,G,R,A
    // Vertex shader with FVF inputs: POSITION 0, NORMAL 3, COLOR 10 (0/1), TEXCOORD 5 (set).
    auto* vs=rhw?nullptr:object(vertex_shader);
    if(vs&&vs->kind!=KVs)vs=nullptr;
    std::array<V4,256> vc;
    if(vs)for(unsigned k=0;k<256;++k)vc[k]={vs_constants[k][0],vs_constants[k][1],vs_constants[k][2],vs_constants[k][3]};
    const std::size_t normal_at=(fvf&0x010u)?12u:none;
    for(std::size_t i=0;vs&&i<total;++i){
        const auto* p=bytes+i*stride;Vertex& v=up_vertices_[i];v=Vertex{};
        pc_shader::VsState s;s.C=vc.data();for(auto& r:s.V)r={0,0,0,1};
        auto input=[&](std::uint8_t usage,std::uint8_t index,const V4& value){
            for(const auto& in:vs->program.inputs)if(in.usage==usage&&in.usage_index==index)s.V[in.reg]=value;};
        input(0,0,{rdf(p),rdf(p+4),rdf(p+8),1.f});
        if(normal_at!=none)input(3,0,{rdf(p+normal_at),rdf(p+normal_at+4),rdf(p+normal_at+8),1.f});
        if(diffuse_at!=none)input(10,0,colour(p+diffuse_at));
        if(specular_at!=none)input(10,1,colour(p+specular_at));
        for(unsigned k=0;k<sets;++k){V4 t{0,0,0,1};for(unsigned q=0;q<tex_n[k];++q)t[q]=rdf(p+tex_at[k]+4*q);input(5,std::uint8_t(k),t);}
        pc_shader_run_vs(vs->program,s);
        std::memcpy(v.pos,&s.oPos,16);
        v.d[0]=pc_shader::sat(s.oD[0]);v.d[1]=pc_shader::sat(s.oD[1]);
        for(unsigned k=0;k<8;++k)v.t[k]=s.oT[k];
        v.fog=pc_shader::clamp01(s.oFog.x);
    }
    for(std::size_t i=0;!vs&&i<total;++i){
        const auto* p=bytes+i*stride;Vertex& v=up_vertices_[i];v=Vertex{};
        if(rhw)for(unsigned k=0;k<4;++k)v.pos[k]=rdf(p+4*k);
        else{const float x=rdf(p),y=rdf(p+4),z=rdf(p+8);                     // fixed-function transform (no lighting)
            for(unsigned k=0;k<4;++k)v.pos[k]=x*wvp[k]+y*wvp[4+k]+z*wvp[8+k]+wvp[12+k];}
        v.d[0]=diffuse_at!=none?colour(p+diffuse_at):V4{1,1,1,1};
        // Specular alpha is the vertex fog factor.
        if(specular_at!=none){v.d[1]=colour(p+specular_at);v.fog=v.d[1].w;v.d[1].w=0.f;}else{v.d[1]={0,0,0,0};v.fog=1.f;}
        for(unsigned k=0;k<sets;++k){V4 t{0,0,0,1};for(unsigned c=0;c<tex_n[k];++c)t[c]=rdf(p+tex_at[k]+4*c);v.t[k]=t;}
    }
    auto* ps=object(pixel_shader);
    ps_=ps&&ps->kind==KPs?&ps->program:nullptr;
    pretransformed_=rhw;
    static const bool dbg_up=std::getenv("OR2_SHADOW_DEBUG")!=nullptr;
    if(dbg_up&&fvf==0x44u){std::fprintf(stderr,"[up44] ps=%u vs=%u type=%u count=%u rs:",pixel_shader,vertex_shader,type,count);
        for(std::uint32_t r:{7u,14u,15u,19u,20u,22u,24u,25u,27u,52u,53u,54u,55u,56u,57u,58u,59u,168u})std::fprintf(stderr," %u=%x",r,render[r]);
        std::fprintf(stderr," tss0:");for(std::uint32_t k:{1u,2u,3u,4u,5u,6u})std::fprintf(stderr," %x",stage[0][k]);std::fprintf(stderr," tex0=%u\n",textures[0]);}
    Vertex tri[3];
    for(std::uint32_t p=0;p<count;++p){
        std::size_t a,b,c;
        if(type==4){a=std::size_t(p)*3u;b=a+1;c=a+2;}
        else if(type==5){a=p;b=a+1;c=a+2;if(p&1u)std::swap(a,b);}
        else{a=0;b=std::size_t(p)+1;c=b+1;}
        tri[0]=up_vertices_[a];tri[1]=up_vertices_[b];tri[2]=up_vertices_[c];
        if(!rhw){triangle(tri[0],tri[1],tri[2]);continue;}                  // clipped, perspective
        if(!(tri[0].pos[3]>0.f)||!(tri[1].pos[3]>0.f)||!(tri[2].pos[3]>0.f))continue;
        raster(tri,3);
    }
    pretransformed_=false;
}
bool PcSoftD3D9Device::texture_level_size(std::uint32_t texture,std::uint32_t level,std::uint32_t& w,std::uint32_t& h){
    w=h=0;auto* o=object(texture);
    if(!o||o->kind!=KTexture||o->texture.faces.empty()||level>=o->texture.faces[0].size())return false;
    w=o->texture.faces[0][level].width;h=o->texture.faces[0][level].height;return true;
}
void PcSoftD3DXSprite::set_transform(const float m[16]){std::memcpy(transform_,m,sizeof transform_);}
void PcSoftD3DXSprite::begin(std::uint32_t flags){flags_=flags;states_.begin(device_,flags);}
void PcSoftD3DXSprite::end(){states_.end(device_);}
void PcSoftD3DXSprite::draw(std::uint32_t texture,const std::int32_t* rect,const float* center,const float* position,std::uint32_t colour){
    auto& d=device_;
    std::uint32_t tw=0,th=0;
    if(texture&&!d.texture_level_size(texture,0,tw,th))return;
    float l=0,t=0,r=float(tw),b=float(th);
    if(rect){l=float(rect[0]);t=float(rect[1]);r=float(rect[2]);b=float(rect[3]);}
    const float w=r-l,h=b-t;
    const float cx=center?center[0]:0.f,cy=center?center[1]:0.f,cz=center?center[2]:0.f;
    const float px=position?position[0]:0.f,py=position?position[1]:0.f,pz=position?position[2]:0.f;
    const float* M=transform_;
    struct V { float x,y,z,rhw; std::uint32_t c; float u,v; } q[4];
    static_assert(sizeof(V)==28,"FVF 0x144 vertex");
    const float cu[4]{0,w,0,w},cv[4]{0,0,h,h};
    for(unsigned k=0;k<4;++k){
        const float x=cu[k]-cx+px,y=cv[k]-cy+py,z=pz-cz;
        q[k].x=x*M[0]+y*M[4]+z*M[8]+M[12];
        q[k].y=x*M[1]+y*M[5]+z*M[9]+M[13];
        q[k].z=x*M[2]+y*M[6]+z*M[10]+M[14];
        q[k].rhw=1.f;q[k].c=colour;
        q[k].u=tw?(l+cu[k])/float(tw):0.f;q[k].v=th?(t+cv[k])/float(th):0.f;
    }
    // Save what the draw overrides, draw, restore (the game's states persist).
    const auto saved_stage0=d.stage[0],saved_stage1=d.stage[1];
    const std::uint32_t saved_tex=d.textures[0],saved_ps=d.pixel_shader,saved_vs=d.vertex_shader,saved_fvf=d.fvf();
    const std::uint32_t saved_cull=d.render[d3d9::RS_CULLMODE],saved_ab=d.render[d3d9::RS_ALPHABLENDENABLE],
        saved_src=d.render[d3d9::RS_SRCBLEND],saved_dst=d.render[d3d9::RS_DESTBLEND];
    if(!(flags_&2u)){ // !D3DXSPRITE_DONOTMODIFY_RENDERSTATE
        d.render[d3d9::RS_CULLMODE]=d3d9::CULL_NONE;
        if(flags_&0x10u){d.render[d3d9::RS_ALPHABLENDENABLE]=1;d.render[d3d9::RS_SRCBLEND]=5;d.render[d3d9::RS_DESTBLEND]=6;}
    }
    d.stage[0][1]=4;d.stage[0][2]=2;d.stage[0][3]=0;d.stage[0][4]=4;d.stage[0][5]=2;d.stage[0][6]=0;d.stage[0][11]=0;d.stage[0][28]=1;
    d.stage[1][1]=1;d.stage[1][4]=1;
    d.textures[0]=texture;d.pixel_shader=0;d.vertex_shader=0;d.set_fvf(0x144u);
    d.draw_primitive_up(5,2,q,sizeof(V));
    d.stage[0]=saved_stage0;d.stage[1]=saved_stage1;d.textures[0]=saved_tex;d.pixel_shader=saved_ps;d.vertex_shader=saved_vs;d.set_fvf(saved_fvf);
    d.render[d3d9::RS_CULLMODE]=saved_cull;d.render[d3d9::RS_ALPHABLENDENABLE]=saved_ab;d.render[d3d9::RS_SRCBLEND]=saved_src;d.render[d3d9::RS_DESTBLEND]=saved_dst;
}
V4 PcSoftD3D9Device::sample(int st,const V4& c,float dudx,float dvdx,float dudy,float dvdy){
    if(st<0||st>=16)return {0,0,0,0};
    auto* o=object(textures[std::size_t(st)]);if(!o||o->kind!=KTexture)return {0,0,0,1};
    const auto& ss=sampler[std::size_t(st)];
    unsigned face=0;float u=c.x,v=c.y;
    if(o->texture.cube){
        const float ax=std::fabs(c.x),ay=std::fabs(c.y),az=std::fabs(c.z);float ma,sc,tc;
        if(ax>=ay&&ax>=az){ma=ax;if(c.x>=0){face=0;sc=-c.z;tc=-c.y;}else{face=1;sc=c.z;tc=-c.y;}}
        else if(ay>=az){ma=ay;if(c.y>=0){face=2;sc=c.x;tc=c.z;}else{face=3;sc=c.x;tc=-c.z;}}
        else{ma=az;if(c.z>=0){face=4;sc=c.x;tc=-c.y;}else{face=5;sc=-c.x;tc=-c.y;}}
        if(ma==0.f)return {0,0,0,1};
        u=(sc/ma+1.f)*0.5f;v=(tc/ma+1.f)*0.5f;dudx=dvdx=dudy=dvdy=0.f;
    }
    const auto& levels=o->rgba[face];
    const auto& tex=o->texture.faces[face];
    const unsigned base_level=std::min<unsigned>(ss[d3d9::SAMP_MAXMIPLEVEL],unsigned(levels.size()-1));
    float lod=0.f;
    {   const float w0=float(tex[0].width),h0=float(tex[0].height);
        const float px=std::sqrt(dudx*dudx*w0*w0+dvdx*dvdx*h0*h0),py=std::sqrt(dudy*dudy*w0*w0+dvdy*dvdy*h0*h0);
        const float r=std::fmax(px,py);lod=r>0.f?std::log2(r):-100.f;
        lod+=fbits(ss[d3d9::SAMP_MIPMAPLODBIAS]);}
    const bool magnify=lod<=0.f;
    const auto filter=magnify?ss[d3d9::SAMP_MAGFILTER]:ss[d3d9::SAMP_MINFILTER];
    auto fetch=[&](unsigned level,float fu,float fv)->V4{
        const auto& lv=tex[level];const auto& px=levels[level];
        bool bu,bv;const float cu=wrap_coord(fu,ss[d3d9::SAMP_ADDRESSU],int(lv.width),bu),cv=wrap_coord(fv,ss[d3d9::SAMP_ADDRESSV],int(lv.height),bv);
        auto texel=[&](int x,int y)->V4{
            if(ss[d3d9::SAMP_ADDRESSU]==1)x=((x%int(lv.width))+int(lv.width))%int(lv.width);else x=std::clamp(x,0,int(lv.width)-1);
            if(ss[d3d9::SAMP_ADDRESSV]==1)y=((y%int(lv.height))+int(lv.height))%int(lv.height);else y=std::clamp(y,0,int(lv.height)-1);
            const auto* p=px.data()+(std::size_t(y)*lv.width+std::size_t(x))*4;return {p[0]/255.f,p[1]/255.f,p[2]/255.f,p[3]/255.f};};
        if(bu||bv){const auto b=ss[d3d9::SAMP_BORDERCOLOR];return {((b>>16)&255)/255.f,((b>>8)&255)/255.f,(b&255)/255.f,(b>>24)/255.f};}
        const float x=cu*float(lv.width)-0.5f,y=cv*float(lv.height)-0.5f;
        if(filter<=1u)return texel(int(std::floor(x+0.5f)),int(std::floor(y+0.5f)));
        const int ix=int(std::floor(x)),iy=int(std::floor(y));const float ax=x-float(ix),ay=y-float(iy);
        return lerp4(lerp4(texel(ix,iy),texel(ix+1,iy),ax),lerp4(texel(ix,iy+1),texel(ix+1,iy+1),ax),ay);
    };
    const auto mip=ss[d3d9::SAMP_MIPFILTER];
    if(mip==0u||magnify)return fetch(base_level,u,v);
    const float l=std::fmin(std::fmax(lod,0.f),float(levels.size()-1));
    if(mip==1u)return fetch(std::max(base_level,unsigned(std::lround(l))),u,v);
    const unsigned a=std::max(base_level,unsigned(std::floor(l))),b=std::min<unsigned>(a+1,unsigned(levels.size()-1));
    return lerp4(fetch(a,u,v),fetch(b,u,v),l-std::floor(l));
}
bool PcSoftD3D9Device::write_png(const std::string& path)const{
    std::vector<std::uint8_t> raw;raw.reserve((width_*4+1)*height_);
    for(std::uint32_t y=0;y<height_;++y){raw.push_back(0);const auto* row=colour_.data()+std::size_t(y)*width_*4;
        for(std::uint32_t x=0;x<width_;++x){raw.push_back(row[x*4]);raw.push_back(row[x*4+1]);raw.push_back(row[x*4+2]);raw.push_back(255);}}
    std::uint32_t crc_table[256];for(std::uint32_t n=0;n<256;++n){std::uint32_t c=n;for(int k=0;k<8;++k)c=c&1?0xedb88320u^(c>>1):c>>1;crc_table[n]=c;}
    auto crc=[&](const std::vector<std::uint8_t>& d){std::uint32_t c=0xffffffffu;for(auto b:d)c=crc_table[(c^b)&0xff]^(c>>8);return c^0xffffffffu;};
    std::vector<std::uint8_t> z{0x78,0x01};
    for(std::size_t at=0;at<raw.size();){const std::size_t n=std::min<std::size_t>(65535,raw.size()-at);const bool last=at+n==raw.size();
        z.push_back(last?1:0);z.push_back(std::uint8_t(n));z.push_back(std::uint8_t(n>>8));z.push_back(std::uint8_t(~n));z.push_back(std::uint8_t((~n)>>8));
        z.insert(z.end(),raw.begin()+std::ptrdiff_t(at),raw.begin()+std::ptrdiff_t(at+n));at+=n;}
    std::uint32_t a=1,b=0;for(auto x:raw){a=(a+x)%65521u;b=(b+a)%65521u;}const std::uint32_t ad=(b<<16)|a;
    for(int k=3;k>=0;--k)z.push_back(std::uint8_t(ad>>(k*8)));
    std::ofstream f(path,std::ios::binary);if(!f)return false;
    const std::uint8_t sig[8]{0x89,'P','N','G',0x0d,0x0a,0x1a,0x0a};f.write(reinterpret_cast<const char*>(sig),8);
    auto chunk=[&](const char* type,const std::vector<std::uint8_t>& data){
        std::vector<std::uint8_t> c(type,type+4);c.insert(c.end(),data.begin(),data.end());
        const std::uint32_t n=std::uint32_t(data.size());const std::uint8_t len[4]{std::uint8_t(n>>24),std::uint8_t(n>>16),std::uint8_t(n>>8),std::uint8_t(n)};
        f.write(reinterpret_cast<const char*>(len),4);f.write(reinterpret_cast<const char*>(c.data()),std::streamsize(c.size()));
        const auto cc=crc(c);const std::uint8_t cb[4]{std::uint8_t(cc>>24),std::uint8_t(cc>>16),std::uint8_t(cc>>8),std::uint8_t(cc)};f.write(reinterpret_cast<const char*>(cb),4);};
    std::vector<std::uint8_t> ihdr{std::uint8_t(width_>>24),std::uint8_t(width_>>16),std::uint8_t(width_>>8),std::uint8_t(width_),
        std::uint8_t(height_>>24),std::uint8_t(height_>>16),std::uint8_t(height_>>8),std::uint8_t(height_),8,6,0,0,0};
    chunk("IHDR",ihdr);chunk("IDAT",z);chunk("IEND",{});return bool(f);
}
// ---- renderer-init objects (404250) ----
namespace {
// Bytes per texel of the formats the renderer creates empty.
std::uint32_t texel_bytes(std::uint32_t format){
    switch(format){case 21:case 22:return 4;case 23:return 2;case 75:return 4;default:return 0;}
}
}
std::uint32_t PcSoftD3D9Device::empty_texture(bool cube,std::uint32_t w,std::uint32_t h,std::uint32_t levels,std::uint32_t format){
    if(!texel_bytes(format)||!w||!h)return 0;
    if(levels==0){levels=1;for(std::uint32_t m=std::max(w,h);m>1;m>>=1)++levels;}
    Object o;o.kind=KTexture;o.format=format;o.width=w;o.height=h;
    o.texture.cube=cube;o.texture.width=w;o.texture.height=h;o.texture.levels=levels;
    const unsigned faces=cube?6u:1u;
    o.texture.faces.resize(faces);o.rgba.resize(faces);
    for(unsigned f=0;f<faces;++f){
        std::uint32_t lw=w,lh=h;
        for(std::uint32_t l=0;l<levels;++l){
            o.texture.faces[f].push_back({lw,lh,{}});
            o.rgba[f].push_back(std::vector<std::uint8_t>(std::size_t(lw)*lh*4,0));
            lw=std::max(1u,lw>>1);lh=std::max(1u,lh>>1);
        }
    }
    objects_.push_back(std::move(o));return std::uint32_t(objects_.size());
}
std::uint32_t PcSoftD3D9Device::create_cube_texture(std::uint32_t edge,std::uint32_t levels,std::uint32_t,std::uint32_t format,std::uint32_t){
    return empty_texture(true,edge,edge,levels,format);}
std::uint32_t PcSoftD3D9Device::create_texture(std::uint32_t w,std::uint32_t h,std::uint32_t levels,std::uint32_t,std::uint32_t format,std::uint32_t){
    return empty_texture(false,w,h,levels,format);}
std::uint32_t PcSoftD3D9Device::create_depth_stencil_surface(std::uint32_t w,std::uint32_t h,std::uint32_t format,std::uint32_t,std::uint32_t,std::uint32_t){
    if(!texel_bytes(format))return 0;
    Object o;o.kind=KSurface;o.format=format;o.width=w;o.height=h;o.bytes.resize(std::size_t(w)*h*texel_bytes(format));
    objects_.push_back(std::move(o));return std::uint32_t(objects_.size());
}
std::uint32_t PcSoftD3D9Device::surface_of(std::uint32_t t,std::uint32_t face,std::uint32_t level){
    auto* o=object(t);if(!o||o->kind!=KTexture||!o->format)return 0;
    const std::uint64_t key=(std::uint64_t(t)<<16)|(face<<8)|level;
    if(const auto it=surfaces_.find(key);it!=surfaces_.end()){add_ref(it->second);return it->second;}
    if(face>=o->texture.faces.size()||level>=o->texture.faces[face].size())return 0;
    Object s;s.kind=KSurface;s.parent=t;s.face=face;s.level=level;s.format=o->format;
    s.width=o->texture.faces[face][level].width;s.height=o->texture.faces[face][level].height;
    s.bytes.resize(std::size_t(s.width)*s.height*texel_bytes(s.format));
    add_ref(t);objects_.push_back(std::move(s));surfaces_[key]=std::uint32_t(objects_.size());return std::uint32_t(objects_.size());
}
std::uint32_t PcSoftD3D9Device::get_cube_map_surface(std::uint32_t c,std::uint32_t face,std::uint32_t level){
    auto* o=object(c);if(!o||!o->texture.cube)return 0;return surface_of(c,face,level);}
std::uint32_t PcSoftD3D9Device::get_surface_level(std::uint32_t t,std::uint32_t level){
    auto* o=object(t);if(!o||o->texture.cube)return 0;return surface_of(t,0,level);}
std::uint8_t* PcSoftD3D9Device::lock_rect(std::uint32_t s,std::uint32_t& pitch){
    auto* o=object(s);pitch=0;if(!o||o->kind!=KSurface)return nullptr;
    pitch=o->width*texel_bytes(o->format);return o->bytes.data();}
void PcSoftD3D9Device::unlock_rect(std::uint32_t s){
    auto* o=object(s);if(!o||o->kind!=KSurface||!o->parent)return;
    auto* t=object(o->parent);if(!t)return;
    auto& dst=t->rgba[o->face][o->level];
    const std::size_t n=std::size_t(o->width)*o->height;
    for(std::size_t k=0;k<n;++k){
        if(o->format==23){ // R5G6B5
            const std::uint16_t v=std::uint16_t(o->bytes[k*2]|(o->bytes[k*2+1]<<8));
            dst[k*4]=std::uint8_t(((v>>11)&31)*255/31);dst[k*4+1]=std::uint8_t(((v>>5)&63)*255/63);dst[k*4+2]=std::uint8_t((v&31)*255/31);dst[k*4+3]=255;
        }else{ // A8R8G8B8 / X8R8G8B8 (B,G,R,A bytes)
            dst[k*4]=o->bytes[k*4+2];dst[k*4+1]=o->bytes[k*4+1];dst[k*4+2]=o->bytes[k*4];
            dst[k*4+3]=o->format==22?255:o->bytes[k*4+3];
        }
    }
}
}

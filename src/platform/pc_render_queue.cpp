#include "platform/pc_render_queue.hpp"
#include "driving/pc_x87.hpp"
#include "system/perf.hpp"
#include "platform/pc_pmt_records.hpp"
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <string>
namespace outrun::platform {
namespace {
using driving::Bytes;
using driving::CourseProbe;
using driving::PcMatrixStack;
using namespace pmt_records;
// x87 register value: every arithmetic step rounds to the precision control
// (24-bit after the Direct3D device is created without FPU_PRESERVE).
using X=driving::X87;
constexpr u32 Invalid=0xffffffffu;
// The 0x48-byte draw descriptor 405360 builds on its stack: the queue entry
// words, then the walk's own state.
struct Descriptor : DrawEntry {
    u32 alpha_queue;     // +3C 0 opaque queue, 1 alpha queue
    u32 matrix_dirty;    // +40 the current matrix is not in the pool yet (404A80)
    u32 unknown44;
};
static_assert(sizeof(Descriptor)==0x48);
// A new descriptor of object `id` (405360 / 405450 / 405580 headers).
Descriptor descriptor(u32 id,u32 resource,u32 object,std::int32_t opaque,u32 colour,std::int32_t colour_byte){
    Descriptor d{};
    d.palette_count=1;d.id_high=id&0xffff0000u;d.resource=resource;d.object=object;
    d.colour_list=colour_byte>0?colour:0u;d.colour=u32(colour_byte);
    d.id=id;d.alpha_queue=opaque!=0?0u:1u;d.matrix_dirty=1;
    return d;
}
// Object record offset of index in a bank (object table at system +0x18).
u32 object_offset(u32 index){return 0x18u+index*0x3cu;}
// 409FD0 inline form: depth++, and when it fits new = m * current.
void push_multiply(PcMatrixStack& s,Bytes m){
    const bool fits=s.depth+1<s.capacity;driving::pc_matrix_push(s);
    if(fits){std::array<std::uint8_t,64> copy;m.check(0,64);std::memcpy(copy.data(),m.data(),64);
        driving::pc_matrix_multiply_current(s,Bytes(copy.data(),64));}
}
// 40A580: rotation R about the unit axis by (sin, cos), current = R * current.
void rotate_axis_40a580(PcMatrixStack& s,const CourseProbe& a,float sn,float cs){
    const X k=X(1.f)-cs;                          // fld 1; fsub cos (stays in ST)
    const float x=a.x,y=a.y,z=a.z;
    const float xy=float((X(y)*x)*k);
    const float yz=float((X(z)*y)*k);
    const float xz=float((X(z)*x)*k);
    const float xs=float(X(sn)*x);
    const float ys=float(X(y)*sn);
    const float zs=float(X(z)*sn);
    const X xx=X(x)*x,yy=X(y)*y;                  // kept in x87 registers
    const float zz=float(X(z)*z);                 // stored
    std::array<float,16> r{};
    r[0]=float((X(1.f)-xx)*cs+xx);
    r[1]=float(X(zs)+xy);r[2]=float(X(xz)-ys);
    r[4]=float(X(xy)-zs);r[5]=float((X(1.f)-yy)*cs+yy);r[6]=float(X(xs)+yz);
    r[8]=float(X(ys)+xz);r[9]=float(X(yz)-xs);r[10]=float((X(1.f)-zz)*cs+zz);
    r[15]=1.f;
    driving::pc_matrix_multiply_current(s,Bytes(r.data(),64));
}
// Node flag 0x800: the node matrix pushed, then turned about y so its z
// axis faces the camera (eye-to-target direction projected on xz).
void face_camera(PcRenderContext& c,Bytes m){
    auto& s=c.matrices;
    const auto a=driving::pc_transform_point(s.current(),{0.f,0.f,1.f});
    const auto o=driving::pc_transform_point(s.current(),{0.f,0.f,0.f});
    CourseProbe dir{float(X(a.x)-o.x),0.f,float(X(a.z)-o.z)};
    CourseProbe to{float(X(c.view.eye[0])-c.view.target[0]),0.f,float(X(c.view.eye[2])-c.view.target[2])};
    const X len=driving::x87_sqrt(X(to.z)*to.z+X(to.x)*to.x);
    if(len>X(0.0001)){ // 6282A8 (double 1e-4)
        const X inv=X(1.f)/len;
        to.x=float(X(to.x)*inv);to.y=float(X(0.f)*inv);to.z=float(X(to.z)*inv);
    }
    push_multiply(s,m);
    CourseProbe axis{float(X(to.z)*dir.y-X(dir.z)*to.y),
                     float(X(dir.z)*to.x-X(to.z)*dir.x),
                     float(X(dir.x)*to.y-X(to.x)*dir.y)};
    const float dot=float((X(to.z)*dir.z+X(to.x)*dir.x)+X(to.y)*dir.y);
    const X sq=(X(axis.z)*axis.z+X(axis.y)*axis.y)+X(axis.x)*axis.x;
    if(!(driving::x87_abs(sq)<X(1.1920928955078125e-7f)||std::isnan(sq.v))){
        const X root=driving::x87_sqrt(sq);const float sn=float(root);
        if(root>X(0.0001)){
            const X inv=X(1.f)/sn;
            axis.x=float(X(axis.x)*inv);axis.y=float(X(axis.y)*inv);axis.z=float(X(axis.z)*inv);
        }
        rotate_axis_40a580(s,axis,sn,dot);
    }else if(dot<0.f){
        rotate_axis_40a580(s,{0.f,1.f,0.f},0.f,-1.f);
    }
}
// 404A80: matrix pool slot of the next entry: the current matrix is
// appended when it changed since the last entry (a palette's first entry
// reuses the slots 405580 appended).
u32 matrix_slot_404a80(PcRenderQueue& q,Descriptor& d,PcMatrixStack& s){
    if(d.matrix_dirty==0)return q.matrix_count-d.palette_count;
    d.matrix_dirty=0;
    if(std::int32_t(q.matrix_count)>=std::int32_t(q.matrix_capacity))q.matrix_count=q.matrix_capacity-1u;
    u32 slot;
    if(d.palette_count==1){slot=q.matrix_count;++q.matrix_count;}
    else{slot=q.matrix_count-d.palette_count;d.palette_count=1;}
    if(slot>=q.matrices.size())throw std::out_of_range("404A80 matrix pool slot");
    const auto m=s.current();m.check(0,64);std::memcpy(q.matrices[slot].data(),m.data(),64);
    return slot;
}
// 404B00: view-space centre, frustum test (near, the four planes, far) and
// LOD from the projected size (exponent of the screen radius against the
// threshold; nodes with flag 0x10 pick mesh e - bias - 1 in 0..3).
bool node_visible(PcRenderContext& c,const PmtNode& node,CourseProbe& v,std::int32_t& lod_out){
    OR2_PERF_ZONE("404B00 node visible");
    auto& s=c.matrices;
    v=driving::pc_transform_point(s.current(),{node.centre[0],node.centre[1],node.centre[2]});
    {   // push, load the view slot, transform, pop
        const bool fits=s.depth+1<s.capacity;driving::pc_matrix_push(s);
        if(fits){const auto cur=s.current();cur.check(0,64);std::memcpy(cur.data(),c.view.view_95d860.data(),64);}
        v=driving::pc_transform_point(s.current(),v);
        driving::pc_matrix_pop(s);
    }
    const auto u=driving::pc_matrix_vector(s,{0.577350259f,0.577350259f,0.577350259f});
    const float r=float(driving::x87_sqrt((X(u.z)*u.z+X(u.y)*u.y)+X(u.x)*u.x)*node.radius);
    std::int32_t lod=0;bool visible=false;
    const auto& f=c.view.frustum_95bf40;const auto& p=c.view.planes_95bf58;
    const X near_limit=-X(f[4]);
    auto greater=[](X a,X b){return a>b;}; // FCOMP/TEST 41/JE: strictly greater, ordered
    if(greater(X(v.z)-r,near_limit))goto done;
    if(greater(X(p[1][2])*v.z+X(p[1][0])*v.x,r))goto done;
    if(greater(X(p[0][2])*v.z+X(p[0][0])*v.x,r))goto done;
    if(greater(X(p[2][1])*v.y+X(p[2][2])*v.z,r))goto done;
    if(greater(X(p[3][1])*v.y+X(p[3][2])*v.z,r))goto done;
    if((X(r)+v.z)<-X(f[5]))goto done;         // far: TEST 5 / JNP
    {
        std::int32_t e=-126;
        if(!(v.z==0.f)){
            e=127;
            const float w=float((((X(-1.f)/f[3])/v.z)*f[4])*r);
            std::uint32_t bits;std::memcpy(&bits,&w,4);
            e-=std::int32_t((bits>>23)&0xffu);
            if(e>=c.lod_threshold_8999b8)goto done;
        }
        if(node.flags&0x10u){lod=e-node.lod_bias-1;if(lod>3)lod=3;else if(lod<0)lod=0;}
        visible=true;
    }
done:
    lod_out=lod;return visible;
}
// 404D10: the node tree from `first` (siblings in order, children first):
// each node's matrix pushed (flag 0x800: facing the camera), LOD threshold
// 7 under flag 0x40 (restored on the way back), and each visible node's
// mesh of its LOD queued with its depth and matrix.
void walk_404d10(PcRenderContext& c,const PcModelBank& bank,const PmtObject& obj,Descriptor& d,std::int32_t first){
    const Bytes sys=bank.system;
    u32 saved=u32(c.lod_threshold_8999b8); // VM bridge 1039C0C
    std::int32_t index=first;
    for(;;){
        const u32 node_at=obj.nodes+u32(index)*0x38u;
        const auto node=pmt<PmtNode>(sys,node_at);
        if(c.flag_depth_8999a0<0||c.flag_depth_8999a0>=64)throw std::out_of_range("899560 node stack");
        c.flag_stack_899560[std::size_t(c.flag_depth_8999a0)]=saved;
        ++c.flag_depth_8999a0;
        bool pushed=false;
        if(node.flags&0x40u)c.lod_threshold_8999b8=7;
        if(node.matrix>=0){
            const Bytes m=sys.sub(obj.matrices+u32(node.matrix)*0x40u,0x40);
            if(node.flags&0x800u)face_camera(c,m);
            else push_multiply(c.matrices,m);
            d.matrix_dirty=1;pushed=true;
        }
        CourseProbe centre{};std::int32_t lod=0;
        if(node_visible(c,node,centre,lod)&&node.mesh[lod]>=0){
            std::memcpy(&d.depth,&centre.z,4);
            d.mesh=obj.meshes+u32(node.mesh[lod])*8u;
            d.node=node_at;
            auto& q=d.alpha_queue?c.alpha:c.opaque;
            if(q.count<q.capacity){
                const auto slot=q.count;++q.count;
                d.matrix=matrix_slot_404a80(q,d,c.matrices);
                std::memcpy(q.entries[slot].data(),static_cast<const DrawEntry*>(&d),sizeof(DrawEntry));
            }
        }
        if(node.child>=0&&node.child!=first)walk_404d10(c,bank,obj,d,node.child);
        const std::int32_t sibling=pmt<PmtNode>(sys,node_at).sibling;
        if(pushed){driving::pc_matrix_pop(c.matrices);d.matrix_dirty=1;}
        --c.flag_depth_8999a0;
        if(c.flag_depth_8999a0<0||c.flag_depth_8999a0>=64)throw std::out_of_range("899560 node stack");
        saved=c.flag_stack_899560[std::size_t(c.flag_depth_8999a0)];
        c.lod_threshold_8999b8=std::int32_t(saved);
        if(sibling<0)return;
        index=sibling;
    }
}
// The walk of a whole object; in immediate mode (8999B0) the opaque queue
// is flushed at once (4052C0).
void walk_descriptor(PcRenderContext& c,const PcModelBank& bank,const PmtObject& obj,Descriptor& d){
    if(c.immediate_8999b0==0u){walk_404d10(c,bank,obj,d,0);return;}
    c.immediate_8999b0=0;
    walk_404d10(c,bank,obj,d,0);
    if(!c.flush_4052c0)throw std::runtime_error("immediate mode without a 4052C0 owner");
    c.flush_4052c0();
}
const PcModelBank& bank_of(PcRenderContext& c,u32 id,const char* what){
    const auto* bank=c.bank(id>>16);
    if(!bank)throw std::runtime_error(std::string(what)+" object bank not loaded");
    return *bank;
}
}
void render_queues_reset_405160(PcRenderQueue& o,PcRenderQueue& a){
    o=PcRenderQueue{};a=PcRenderQueue{};
    o.capacity=0x100;o.matrix_capacity=0x100;o.entries.resize(0x100);o.matrices.resize(0x100);
    a.capacity=0x600;a.matrix_capacity=0x600;a.entries.resize(0x600);a.matrices.resize(0x600);
}
bool render_node_visible_404b00(PcRenderContext& c,Bytes node,CourseProbe& v,std::int32_t& lod_out){
    return node_visible(c,pmt<PmtNode>(node,0),v,lod_out);
}
void render_rotate_axis_40a580(PcMatrixStack& s,const CourseProbe& a,float sn,float cs){rotate_axis_40a580(s,a,sn,cs);}
// 405360: object `id` queued (opaque or alpha) with its colour list; a node
// list (u16, 0xFFFF-terminated) walks only those nodes, each with its
// sibling link cut for the walk (4050F0).
void render_object_405360(PcRenderContext& c,std::uint32_t id,std::int32_t opaque,Bytes node_list,
    std::uint32_t colour,std::int32_t colour_byte,std::int32_t flag){
    if(id==Invalid)return;
    OR2_PERF_ZONE("405360 object");
    const auto& bank=bank_of(c,id,"405360");
    const auto index=id&0xffffu;
    if(index>=bank.object_count)return;
    const auto object=object_offset(index);
    const auto obj=pmt<PmtObject>(bank.system,object);
    auto d=descriptor(id,id>>16,object,opaque,colour,colour_byte);
    d.flags=flag!=0?1u:0u;
    auto build=[&]{
        OR2_PERF_ZONE("4050F0 walk");
        if(node_list.size()==0){walk_404d10(c,bank,obj,d,0);return;}
        for(std::size_t k=0;;k+=2){
            const auto n=node_list.i16(k);if(std::uint16_t(n)==0xffffu)return;
            const auto sibling=obj.nodes+u32(std::int32_t(n))*0x38u+0x24u;
            const auto saved=bank.system.u32(sibling);
            bank.system.put32(sibling,Invalid);
            walk_404d10(c,bank,obj,d,n);
            bank.system.put32(sibling,saved);
        }
    };
    if(c.immediate_8999b0==0u){build();return;}
    c.immediate_8999b0=0;
    build();
    if(!c.flush_4052c0)throw std::runtime_error("405360 immediate mode without a 4052C0 owner");
    c.flush_4052c0();
}
// 405450: object a drawn with object b of the same bank as its morph
// target (stream 1) at the weight 95AECC.
void render_object_morph_405450(PcRenderContext& c,std::uint32_t a,std::uint32_t b,std::int32_t opaque,
    std::uint32_t colour,std::int32_t colour_byte){
    if(a==Invalid)return;
    const auto& bank_a=bank_of(c,a,"405450");
    if((a&0xffffu)>=bank_a.object_count)return;
    if(b==Invalid)return;
    const auto& bank_b=bank_of(c,b,"405450 morph");
    if((b&0xffffu)>=bank_b.object_count)return;
    if((a>>16)!=(b>>16))throw std::runtime_error("405450 morph target in another bank (descriptor +10 is b's bank)");
    const auto object=object_offset(a&0xffffu);
    auto d=descriptor(a,b>>16,object,opaque,colour,colour_byte);
    d.morph_target=object_offset(b&0xffffu);d.morph_weight=c.morph_weight_95aecc;
    walk_descriptor(c,bank_a,pmt<PmtObject>(bank_a.system,object),d);
}
// 405580: the object with a palette of `count` matrices appended to the
// queue's pool (nothing when it does not fit), walked under palette[0].
void render_object_palette_405580(PcRenderContext& c,std::uint32_t id,std::int32_t opaque,std::uint32_t colour,
    std::int32_t colour_byte,const std::uint8_t* palette,std::uint32_t count){
    if(id==Invalid)return;
    const auto& bank=bank_of(c,id,"405580");
    if((id&0xffffu)>=bank.object_count)return;
    const auto object=object_offset(id&0xffffu);
    auto d=descriptor(id,id>>16,object,opaque,colour,colour_byte);
    d.palette_count=count;d.matrix_dirty=0;
    auto& q=opaque!=0?c.opaque:c.alpha;
    if(std::int32_t(q.matrix_count+count)>std::int32_t(q.matrix_capacity))return;
    for(std::uint32_t k=0;k<count;++k){
        if(q.matrix_count+k>=q.matrices.size())throw std::out_of_range("405580 matrix pool");
        std::memcpy(q.matrices[q.matrix_count+k].data(),palette+k*64u,64);
    }
    q.matrix_count+=count;
    auto& s=c.matrices;
    ++s.depth;
    if(s.depth<s.capacity){s.current_offset+=64;auto cur=s.current();cur.check(0,64);std::memcpy(cur.data(),palette,64);}
    walk_descriptor(c,bank,pmt<PmtObject>(bank.system,object),d);
    --s.depth;
    if(s.depth>=0)s.current_offset-=64;
}
// 4056D0: every colour record's +0C set to value (the old values kept in
// the opaque queue's undo list for 4052C0; nothing when the list would
// reach 0x80), then 405360 into the opaque queue.
void render_object_override_4056d0(PcRenderContext& c,std::uint32_t id,std::uint32_t value,
    std::uint32_t colour,std::int32_t colour_byte){
    if(id==Invalid)return;
    const auto& bank=bank_of(c,id,"4056D0");
    const auto index=id&0xffffu;
    if(index>=bank.object_count)return;
    const auto obj=pmt<PmtObject>(bank.system,object_offset(index));
    const auto n=bank.system.u32(obj.info+0x30);
    if(std::int32_t(std::uint32_t(c.opaque.undo.size())+n)>=0x80)return;
    for(std::uint32_t k=0;k<n;++k){
        const auto a=obj.colours+k*0x48u+0xcu;
        c.opaque.undo.push_back({id>>16,a,bank.system.u32(a)});
        bank.system.put32(a,value);
    }
    render_object_405360(c,id,1,Bytes(nullptr,0),colour,colour_byte,0);
}
}

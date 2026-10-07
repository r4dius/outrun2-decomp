#include "platform/pc_render_flush.hpp"
#include "driving/pc_x87.hpp"
#include "driving/pc_d3dx.hpp"
#include "driving/pc_matrix_stack.hpp"
#include "platform/pc_d3d9_state.hpp"
#include "platform/pc_pmt_loader.hpp"
#include "platform/pc_vertex_shader_setup.hpp"
#include "system/perf.hpp"
#include "platform/pc_pmt_records.hpp"
#include <cmath>
#include <cstring>
#include <stdexcept>
namespace outrun::platform {
namespace {
using driving::Bytes;
using M=std::array<float,16>;
using Vec4=std::array<float,4>;
using render_state::u32;
using render_state::f32;
using render_state::bits;
// x87 register value: every arithmetic step rounds to the precision control
// (24-bit after the Direct3D device is created without FPU_PRESERVE).
using X=driving::X87;
float st(X v){return driving::x87_float(v);}
float exe_f(u32 va){return f32(pc_shader_data_u32(va));}
u32 exe_u(u32 va){return pc_shader_data_u32(va);}
M exe_matrix(u32 va){M m{};for(unsigned k=0;k<16;++k)m[k]=exe_f(va+k*4);return m;}
const M Identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
// MSVC _ftol2 (582194): truncation; out of range and NaN give 0x80000000.
std::int32_t ftol(X v){return driving::x87_ftol32(v);}
// fild of an unsigned value as the PC does it (signed load + 2^32 fix-up).
X fild_u(u32 v){X r=X(std::int32_t(v));if(std::int32_t(v)<0)r+=X(4294967296.0f);return r;}
constexpr u32 Unknown=0xfffffffeu,None=0xffffffffu;

using namespace pmt_records;
DrawEntry& entry(std::array<u32,15>& e){return *reinterpret_cast<DrawEntry*>(e.data());}

struct Flush {
    PcFlushContext& c;
    PcD3D9Device& d;
    PcRenderGlobals& g;
    render_state::MaterialCache& mc;
    render_state::PixelPipeline& px;
    render_state::TransformState& tf;
    render_state::AnimationState& anim;
    explicit Flush(PcFlushContext& ctx):c(ctx),d(ctx.device),g(ctx.g),mc(g.material()),px(g.pixel()),tf(g.transform()),anim(g.animation()){}

    // 4088F0 / 408920 / 408960 and the inline Get/compare/Set copies.
    void rs(u32 state,u32 value){if(d.get_render_state(state)!=value)d.set_render_state(state,value);}
    void tss(u32 stage,u32 type,u32 value){if(d.get_texture_stage_state(stage,type)!=value)d.set_texture_stage_state(stage,type,value);}
    void samp(u32 stage,u32 type,u32 value){if(d.get_sampler_state(stage,type)!=value)d.set_sampler_state(stage,type,value);}
    void vsc(u32 reg,const float* v,u32 n){d.set_vertex_shader_constant_f(reg,v,n);}
    void vsc(u32 reg,const u32 (&v)[4]){const auto f=render_state::load4(v);vsc(reg,f.data(),1);}
    void vsc_exe(u32 reg,u32 va){const Vec4 v{exe_f(va),exe_f(va+4),exe_f(va+8),exe_f(va+12)};vsc(reg,v.data(),1);}

    // ---- combiner entries (stage records 89B638) ----
    // The PC indexes these arrays without bounds: an index past the end runs
    // on into the following words of the pipeline (never seen in the game's
    // materials). Outside the pipeline the old address model threw; so does this.
    u32& pipeline_word(const u32* field,u32 index){
        auto* base=reinterpret_cast<u32*>(&px);
        const std::size_t at=std::size_t(field-base)+index;
        if(at>=sizeof(render_state::PixelPipeline)/4u)throw std::out_of_range("PC renderer global outside the modelled ranges");
        return base[at];
    }
    render_state::CombinerEntry& combiner(u32 s,u32 n){
        auto& first=pipeline_word(&px.stage[s].entry[0].colour_op,n*8u);
        (void)pipeline_word(&first,7);   // the whole entry inside the pipeline
        return *reinterpret_cast<render_state::CombinerEntry*>(&first);
    }
    // Stage-dependent operand bytes: texture register t(s+8), temp (4 or 12).
    static u32 texreg(u32 s){return s+8u;}
    static u32 temp(u32 s){return s!=0u?12u:4u;}
    // 40B7B0: stage alpha entry 0.
    void stage_alpha_40b7b0(u32 s,u32 op,u32 arg){
        auto& st=px.stage[s];
        if(st.entry[0].alpha_op==op&&st.entry[0].alpha_arg==arg&&st.alpha_count==1u)return;
        st.alpha_count=1;px.dirty=1;px.alpha_mode[s]=0x1b;st.entry[0].alpha_op=op;st.entry[0].alpha_arg=arg;
    }
    // 40B710: stage colour entry 0.
    void stage_colour_40b710(u32 s,u32 op,u32 arg){
        auto& st=px.stage[s];
        if(st.entry[0].colour_op==op&&st.entry[0].colour_arg==arg&&st.colour_count==1u)return;
        st.colour_count=1;px.dirty=1;px.colour_mode[s]=0x1b;st.entry[0].colour_op=op;st.entry[0].colour_arg=arg;
    }
    // 40B760: append a colour entry.
    void stage_append_40b760(u32 s,u32 op,u32 arg){
        auto& e=combiner(s,px.stage[s].colour_count++);
        px.dirty=1;px.colour_mode[s]=0x1b;e.colour_op=op;e.colour_arg=arg;
    }
    // Inline alpha defaults of 408C80/409430 (mode 3 or 4).
    void stage_alpha_default(u32 s,u32 mode,u32 op){
        if(px.alpha_mode[s]==mode)return;
        auto& st=px.stage[s];
        px.alpha_mode[s]=mode;st.entry[0].alpha_arg=0xc0;px.dirty=1;st.entry[0].alpha_op=op;st.alpha_count=1;
    }
    void stage_alpha_mode3(u32 s){stage_alpha_default(s,3,(temp(s)<<24)|0x10301010u);}
    void stage_alpha_mode4(u32 s){stage_alpha_default(s,4,(temp(s)<<16)|(texreg(s)<<24)|0x10101010u);}
    // 40B800: stage colour mode.
    void stage_mode_40b800(u32 s,u32 mode){
        if(px.colour_mode[s]==mode)return;
        auto& st=px.stage[s];
        px.colour_mode[s]=mode;px.dirty=1;
        if(s==0u){anim.specular_variant=0;if(mode==4u&&anim.specular_request==1u){mode=5;anim.specular_variant=1;}}
        auto set=[&](u32 op,u32 arg){st.entry[0].colour_op=op;st.entry[0].colour_arg=arg;st.colour_count=1;};
        auto bridge=[&](u32 op){set(op,0xc00);}; // 40BC59 via 1039B64
        const u32 t=texreg(s)<<24,q=temp(s),e=texreg(s);
        switch(mode){
        case 1:set(0,0xc0);break;
        case 2:set(t|0x200000u,0xc0);break;
        case 4:set((q<<16)|t,0xc0);break;
        case 5:set((q<<16)|t,0x100c0);break;
        case 6:set((q<<16)|t,0x200c0);break;
        case 7:bridge((q<<8)|t|0x200020u);break;
        case 8:set((q<<8)|t|0x200020u,0x8c00);break;
        case 9:set((q<<8)|t|0x200020u,0x18c00);break;
        case 10:bridge((q<<8)|t|0x20e020u);break;
        case 11:bridge((e<<24)|(q<<8)|e|0x200020u);break;
        case 12:bridge(t|(q<<8)|0x140034u);break;
        case 13:bridge((e<<24)|(q<<8)|(e<<16)|e|0x100030u);break;
        case 14:bridge(t|(q<<8)|0x110031u);break;
        case 15:bridge((e<<24)|(q<<8)|e|0x300030u);break;
        case 16:set((q<<16)|(q<<8)|t|q|0x100030u,0xc00);break;
        case 17:set(s>=3u?(t|((s+8u)<<16)):(t|((s+9u)<<16)),0xc0);break;
        case 18:bridge((e<<24)|(q<<8)|e|0x200010u);break;
        case 19:bridge((e<<24)|(q<<16)|e|0x2010u);break;
        case 20:bridge((q<<16)|(e<<24)|(e<<8)|0x30000020u);break;
        case 21:bridge((q<<16)|(e<<24)|(e<<8)|0x20001020u);break;
        case 24:set((q<<16)|t,0x20c0);break;
        case 25:bridge((e<<8)|q|0x1200000u);break;
        case 26:bridge((e<<16)|q|0x1002100u);break;
        default: // 40BC73 (modes 3, 22, 23 and out of range)
            if(std::int32_t(s)>0)st.colour_count=0;
            else set((q<<24)|0x200000u,0xc0);
            break;
        }
    }
    // 40B4B0: an ARGB colour as four floats scaled by 1/255.
    static Vec4 argb(u32 v){
        const float k=exe_f(0x6280bc); // 1/255
        return {st(fild_u((v>>16)&0xffu)*k),st(fild_u((v>>8)&0xffu)*k),st(fild_u(v&0xffu)*k),st(fild_u(v>>24)*k)};
    }
    // 40B4B0: combiner constants (stage 4: pixel constant 7 from the second
    // colour, kept in stage 3's first entry as on the PC; other stages:
    // constants 2s / 2s+1).
    void combiner_constants_40b4b0(u32 a,u32 s,u32 b){
        if(s==4u){
            auto& slot=px.stage[3].entry[0].constant_b;
            if(slot==b)return;
            slot=b;px.dirty=1;
            const auto v=argb(b);d.set_pixel_shader_constant_f(7,v.data(),1);return;
        }
        auto& e=px.stage[s].entry[0];
        if(e.constant_a==a&&e.constant_b==b)return;
        e.constant_a=a;e.constant_b=b;px.dirty=1;
        const auto v1=argb(a);d.set_pixel_shader_constant_f(2*s,v1.data(),1);
        const auto v2=argb(b);d.set_pixel_shader_constant_f(2*s+1,v2.data(),1);
    }
    // 40AE80: alpha test and colour write state of a pass.
    void blend_40ae80(u32 colour_write,u32 alpha_test,u32 ref){
        px.alpha_scale=bits(st(X(f32(g.reflection().alpha))*0.25f*4.0f));
        px.colour_write=colour_write;px.alpha_test=alpha_test;px.colour_write_rgb=colour_write&~8u;px.alpha_ref=ref;
        px.unknown_89bc14=1;px.applied_mode=0xc;
        if(alpha_test){
            if(ref>=0x80u)stage_alpha_40b7b0(4,0x10101230,0x4c00);
            else stage_alpha_40b7b0(4,0x1c301010,0xc0);
            return;
        }
        if(px.alpha_mode[4]!=3u){
            auto& st4=px.stage[4];
            px.alpha_mode[4]=3;px.dirty=1;st4.entry[0].alpha_op=0x1c301010;st4.entry[0].alpha_arg=0xc0;st4.alpha_count=1;
        }
    }
    // 404600: pass states.
    bool pass_404600(u32 k){
        const auto& p=g.pass(k);
        if(!p.enabled)return false;
        using namespace d3d9;
        rs(RS_ALPHABLENDENABLE,p.alpha_blend);rs(RS_ZWRITEENABLE,p.z_write);rs(RS_COLORWRITEENABLE,p.colour_write);rs(RS_ALPHAREF,p.alpha_ref);
        blend_40ae80(p.colour_write,p.alpha_test,p.alpha_ref);
        return true;
    }
    // 408AF0: flush begin.
    void begin_408af0(){
        for(auto& f:mc.layer_flags)f|=0x3e00000u;
        for(auto& k:mc.texture_key)k=Unknown;
        mc.saved_texture3=d.get_texture(3);
        mc.saved_address3=d.get_sampler_state(3,d3d9::SAMP_ADDRESSU);
        mc.saved_fog_enable=d.get_render_state(d3d9::RS_FOGENABLE);
        mc.colour_index=None;mc.unknown_89a550[0]=None;mc.object_key=0;mc.unknown_89a50c[0]=0;px.dirty=1;
    }
    // 408BB0: flush end.
    void end_408bb0(){
        for(u32 s=0;s<3;++s){
            if(std::int32_t(mc.texture_key[s])<0)continue;
            d.set_texture(s,0);
            if(px.texture[s].bound){px.texture[s].bound=0;px.dirty=1;}
        }
        if(std::int32_t(mc.texture_key[3])>=0)d.set_texture(3,mc.saved_texture3);
        if(mc.saved_texture3){d.release(mc.saved_texture3);mc.saved_texture3=0;}
        rs(d3d9::RS_FOGENABLE,mc.saved_fog_enable);
        d.set_pixel_shader(0);
    }

    // ---- transforms (95D860) ----
    // 411230: texture matrix constants 68+4s.
    void texture_matrix_411230(u32 mode,u32 s){
        tf.texture_matrix_mode[s]=mode;tf.texture_matrix_eye[s]=0;
        M m=Identity;
        if(mode){
            const auto type=tf.texture_matrix_type[s];
            const auto slot=render_state::load(tf.slot[2+s]);
            if(type==0x10000u||type==0x30000u){
                auto wv=render_state::load(tf.world_view);wv[12]=0;wv[13]=0;wv[14]=0;
                m=driving::pc_d3dx_matrix_multiply(wv,slot);tf.texture_matrix_eye[s]=1;
            }else if(type==0x20000u){
                m=driving::pc_d3dx_matrix_multiply(render_state::load(tf.world_view),slot);tf.texture_matrix_eye[s]=1;
            }else m=slot;
            m=driving::pc_d3dx_matrix_transpose(m);
        }
        vsc(s*4+0x44,m.data(),4);
    }
    // 4111C0: palette matrix constants 4*slot+0x38.
    void palette_4111c0(const M& m,u32 slot){
        const auto t=driving::pc_d3dx_matrix_transpose(driving::pc_d3dx_matrix_multiply(m,render_state::load(tf.inverse_world)));
        vsc(slot*4+0x38,t.data(),4);
    }
    // 411060: world matrix, eye and lights in object space.
    void world_411060(const M& world){
        OR2_PERF_ZONE("411060 world matrix");
        render_state::store(tf.world_view,driving::pc_d3dx_matrix_multiply(world,render_state::load(tf.slot[0])));
        const auto wvp=driving::pc_d3dx_matrix_multiply(world,render_state::load(tf.view_projection));
        render_state::store(tf.world_view_projection,wvp);
        const auto wvp_t=driving::pc_d3dx_matrix_transpose(wvp);
        vsc(0x40,wvp_t.data(),4);
        {M inv=render_state::load(tf.inverse_world);if(driving::pc_d3dx_matrix_inverse(inv,world))render_state::store(tf.inverse_world,inv);}
        const auto inv_world=render_state::load(tf.inverse_world);
        const auto eye=driving::pc_d3dx_vec3_transform_coord({f32(tf.eye[0]),f32(tf.eye[1]),f32(tf.eye[2])},inv_world);
        const Vec4 e4{eye[0],eye[1],eye[2],0.f};vsc(0x11,e4.data(),1);
        for(u32 k=0;k<5;++k){
            if(!tf.light_enabled[k])continue;
            const auto& l=tf.light[k];
            if(l.type!=1u){
                auto dir=driving::pc_d3dx_vec3_transform_normal({f32(l.direction[0]),f32(l.direction[1]),f32(l.direction[2])},inv_world);
                dir=driving::pc_d3dx_vec3_normalize(dir);
                const Vec4 v{dir[0],dir[1],dir[2],0.f};vsc(0x25+k,v.data(),1);
            }
            if(l.type!=3u){
                const auto pos=driving::pc_d3dx_vec3_transform_coord({f32(l.position[0]),f32(l.position[1]),f32(l.position[2])},inv_world);
                const Vec4 v{pos[0],pos[1],pos[2],0.f};vsc(0x27+k,v.data(),1);
            }
        }
        for(u32 s=0;s<4;++s)if(tf.texture_matrix_eye[s])texture_matrix_411230(tf.texture_matrix_mode[s],s);
    }
    // 408A80: environment matrix = inverse view * environment base, column 2 negated.
    void environment_408a80(){
        auto& v=g.view();
        auto m=driving::pc_d3dx_matrix_multiply(render_state::load(v.inverse_view),render_state::load(v.environment_base));
        const X minus_one=X(-1.0f); // fmul (not fchs): a NaN keeps its sign
        m[2]=st(X(m[2])*minus_one);m[12]=0;m[13]=0;m[14]=0;m[6]=st(X(m[6])*minus_one);m[10]=st(X(m[10])*minus_one);
        render_state::store(v.environment,m);
    }
    // 410FF0: view matrix.
    void view_410ff0(const M& view){
        render_state::store(tf.view_projection,driving::pc_d3dx_matrix_multiply(view,render_state::load(tf.slot[1])));
        {M inv=render_state::load(tf.inverse_view);if(driving::pc_d3dx_matrix_inverse(inv,view))render_state::store(tf.inverse_view,inv);}
        tf.eye[1]=tf.inverse_view[13];tf.eye[0]=tf.inverse_view[12];tf.eye[2]=tf.inverse_view[14];
        std::memcpy(g.view().inverse_view,tf.inverse_view,64);
        environment_408a80();
    }
    // 410F90: slot matrix and its derived constants.
    void set_matrix_410f90(const M& m,u32 slot){
        render_state::store(tf.slot[slot],m);
        if(slot==0)view_410ff0(m);
        else if(slot==6)world_411060(m);
        else if(slot>=7&&slot<=9)palette_4111c0(m,slot);
    }

    // ---- lights ----
    // 410D00: light colour constants for the specular variant.
    void light_colours_410d00(bool on){
        if(on&&anim.specular_variant){
            if(anim.specular_colours)return;
            anim.specular_colours=1;vsc_exe(0x10,0x6222dc);
            for(u32 i=0;i<5;++i)if(tf.light_enabled[i])vsc(0x15+i,tf.light_specular[i]);
            return;
        }
        if(!anim.specular_colours)return;
        anim.specular_colours=0;vsc_exe(0x10,0x6222cc);
        for(u32 i=0;i<5;++i)if(tf.light_enabled[i])vsc(0x15+i,tf.light[i].diffuse);
    }
    // 410DD0: material colours of one 0x48-byte colour record (D3DMATERIAL9
    // diffuse +00, ambient +10, specular +20, emissive +30, power +40, alpha +44).
    void material_colours_410dd0(const std::vector<std::uint8_t>& system,u32 record,bool ambient_pass){
        float m[18];
        if(std::size_t(record)+0x48>system.size())Bytes::out_of_view(record,0x48,system.size());
        std::memcpy(m,system.data()+record,0x48);
        vsc(0x12,m+0,1);vsc(0x13,m+4,1);vsc(0x14,m+12,1);
        // Stack vector [esp+0xC..0x18]: w = power bits; xyz from the last lit light.
        Vec4 v=g.stack_410dd0;v[3]=m[16];
        for(std::int32_t k=4;k>=0;--k){
            if(!tf.light_enabled[k])continue;
            const auto& l=tf.light[k];
            v[0]=st(X(f32(l.specular[0]))*m[8]);v[1]=st(X(m[9])*f32(l.specular[1]));v[2]=st(X(f32(l.specular[2]))*m[10]);
            vsc(0x20+u32(k),v.data(),1);
        }
        g.stack_410dd0=v;
        if(ambient_pass){
            const auto& l0=tf.light[0];
            const Vec4 v1{st(X(f32(l0.diffuse[0]))*m[0]),st(X(f32(l0.diffuse[1]))*m[1]),st(X(f32(l0.diffuse[2]))*m[2]),m[3]};
            const Vec4 v2{st(X(f32(tf.ambient_lit[0]))*m[4]+m[12]),st(X(f32(tf.ambient_lit[1]))*m[5]+m[13]),
                          st(X(f32(tf.ambient_lit[2]))*m[6]+m[14]),0.f};
            d.set_pixel_shader_constant_f(0,v1.data(),1);
            d.set_pixel_shader_constant_f(1,v2.data(),1);
            d.set_pixel_shader_constant_f(2,v.data(),1);
        }
        const X alpha=((X(m[17])+1.0f)*0.25f)*f32(px.alpha_scale)*255.0f;
        u32 a8=u32(ftol(alpha));
        if(a8<1u)a8=1;else if(a8>0xffu)a8=0xff;
        combiner_constants_40b4b0(0,4,a8<<24);
    }
    // Light API 410740 / 410710 / 4107A0 (410810 directional, 4109C0 spot,
    // 410B90 point); desc is the 0x94-byte light record: +00 enabled,
    // +04 D3DLIGHT9, +90 specular power.
    void lights_reset_410740(){
        tf.directional_count=0;tf.spot_count=0;tf.point_count=0;
        for(auto& on:tf.light_enabled)on=0;
        anim.specular_variant=0;anim.specular_colours=0;anim.specular_request=0;
        vsc_exe(0x10,0x6222cc);
    }
    static X clamp_power(Bytes desc){
        const float v=desc.f32(0x90);
        X f;
        if(!(std::isnan(v)||v>=1.f))f=X(1.f);        // fcomp 1.0 (test 5/jp): below -> 1.0
        else if(v>2.f)f=X(2.f);                        // fcomp 2.0 (test 41): above -> 2.0
        else f=X(v);                                   // NaN passes through
        return f*X(0.5f);
    }
    void light_specular(u32 slot,Bytes desc){
        const X f=clamp_power(desc);auto& sp=tf.light_specular[slot];
        sp[0]=bits(st(f*desc.f32(8)));sp[1]=bits(st(f*desc.f32(0xc)));sp[2]=bits(st(f*desc.f32(0x10)));sp[3]=desc.u32(0x14);
    }
    void light_record(u32 slot,Bytes desc){
        tf.light_enabled[slot]=1;
        desc.check(4,sizeof(render_state::Light));
        std::memcpy(&tf.light[slot],desc.data()+4,sizeof(render_state::Light));
    }
    static Vec4 desc_vec(Bytes desc,u32 o){return {desc.f32(o),desc.f32(o+4),desc.f32(o+8),desc.f32(o+12)};}
    void light_directional_410810(Bytes desc,u32 index){
        if(desc.u32(4)!=3u||index>=1u)return;
        light_record(index,desc);
        {auto v=desc_vec(desc,8);vsc(0x15+index,v.data(),1);}
        light_specular(index,desc);
        auto c=[&](const u32* a,u32 k){return f32(a[k]);};
        const Vec4 v{st((X(c(tf.light_colour,0))-c(tf.ambient,0))*0.5f),st((X(c(tf.light_colour,1))-c(tf.ambient,1))*0.5f),
                     st((X(c(tf.light_colour,2))-c(tf.ambient,2))*0.5f),0.f};
        vsc(0x1a,v.data(),1);
        for(u32 k=0;k<3;++k)tf.ambient_lit[k]=bits(st(X(c(tf.ambient,k))+desc.f32(0x28+k*4)));
        tf.ambient_lit[3]=0;
        const Vec4 v2{st(X(c(tf.ambient_lit,0))+v[0]),st(X(c(tf.ambient_lit,1))+v[1]),st(X(c(tf.ambient_lit,2))+v[2]),0.f};
        vsc(0x1b,v2.data(),1);
    }
    static X fcos(X v){return driving::x87_cos(v);} // FCOS: precision control not applied
    void light_spot_4109c0(Bytes desc,u32 index){
        if(desc.u32(4)!=2u||index>=2u)return;
        const auto slot=index+1u;
        if(!desc.u32(0)){tf.light_enabled[slot]=0;const Vec4 v{1.f,0.f,0.f,0.f};vsc(0x2c+index,v.data(),1);return;}
        light_record(slot,desc);
        {auto v=desc_vec(desc,8);vsc(0x16+index,v.data(),1);}
        {auto v=desc_vec(desc,0x28);vsc(0x1c+index,v.data(),1);}
        light_specular(slot,desc);
        {const Vec4 v{desc.f32(0x58),desc.f32(0x5c),desc.f32(0x60),desc.f32(0x50)};vsc(0x2c+index,v.data(),1);}
        const float c1=st(fcos(X(st(X(desc.f32(0x68))*0.5f))));
        const X c2=fcos(X(st(X(desc.f32(0x64))*0.5f)));
        const X inv=X(1.0f)/(c2-X(c1));
        // w of this vector is an uninitialised stack slot on the PC (0 here).
        const Vec4 v{st(inv),st(-(inv*X(c1))),desc.f32(0x54),0.f};
        vsc(0x30+index,v.data(),1);
    }
    void light_point_410b90(Bytes desc,u32 index){
        if(desc.u32(4)!=1u||index>=2u)return;
        const auto slot=index+3u;
        if(!desc.u32(0)){tf.light_enabled[slot]=0;const Vec4 v{1.f,0.f,0.f,0.f};vsc(0x2eu+index,v.data(),1);return;}
        light_record(slot,desc);
        {auto v=desc_vec(desc,8);vsc(0x18+index,v.data(),1);}
        {auto v=desc_vec(desc,0x28);vsc(0x1eu+index,v.data(),1);}
        light_specular(slot,desc);
        const Vec4 v{desc.f32(0x58),desc.f32(0x5c),desc.f32(0x60),desc.f32(0x50)};vsc(0x2eu+index,v.data(),1);
    }
    void light_add_4107a0(Bytes desc){
        switch(desc.u32(4)){
        case 3:light_directional_410810(desc,tf.directional_count++);break;
        case 2:if(desc.u32(0))light_spot_4109c0(desc,tf.spot_count++);break;
        case 1:if(desc.u32(0))light_point_410b90(desc,tf.point_count++);break;
        default:break;
        }
    }
    // 40D840 environment state: 40D870 fog, 40DD70 light colours, 40DB80
    // the five lights of the environment.
    void env_fog_40d870(Bytes fog,bool pixel_fog){
        using namespace d3d9;
        u32 enable=0;
        if(fog.u32(0)&&fog.u32(4)&&fog.f32(0x10)>exe_f(0x619a34))enable=1;
        rs(RS_FOGENABLE,enable);
        if(!enable)return;
        rs(RS_FOGCOLOR,fog.u32(0x14));
        const u32 mode=fog.u32(4);
        if(mode==3u){
            rs(RS_FOGSTART,fog.u32(8)^0x80000000u);rs(RS_FOGEND,fog.u32(0xc)^0x80000000u);rs(RS_FOGTABLEMODE,3);return;
        }
        if(mode!=1u&&mode!=2u){rs(RS_FOGTABLEMODE,0);return;}
        if(pixel_fog){rs(RS_FOGDENSITY,fog.u32(0x10));rs(RS_FOGTABLEMODE,mode);return;}
        // Exponential fog approximated by a linear ramp between two densities.
        const bool exp2=mode==2u;
        const X r=X(1.0f)/X(fog.f32(0x10));
        const X a=X(exe_f(exp2?0x61f644:0x61f63c))*r;
        const X b=r*X(exe_f(exp2?0x61f648:0x61f640));
        const X q=X(exe_f(exp2?0x62815c:0x628158))/(b-a);
        const float qf=st(q);
        float l8=st(X(exe_f(0x6280f0))-q*a);
        if(l8>1.0f)l8=1.0f;
        const float inv=st(X(1.0f)/X(qf));
        rs(RS_FOGSTART,bits(st((X(1.0f)-X(l8))*X(inv))));
        rs(RS_FOGEND,bits(st(X(inv)*X(l8))));
        rs(RS_FOGTABLEMODE,3);
    }
    void env_colours_40dd70(Bytes light){
        for(u32 k=0;k<3;++k){tf.light_colour[k]=light.u32(0x6c+k*4);tf.ambient[k]=light.u32(0x78+k*4);}
        tf.light_colour[3]=bits(1.f);tf.ambient[3]=bits(1.f);
    }
    void env_light(u32 index,Bytes light){
        d.light_enable(index,light.u32(0));
        if(light.u32(0)){u32 l[26];for(unsigned k=0;k<26;++k)l[k]=light.u32(4+k*4);d.set_light(index,l);}
        light_add_4107a0(light);
    }

    // ---- pixel pipeline ----
    // 40B0C0: combiner block from the stage entries (at most nine slots;
    // missing colour/alpha entries pass the previous result through).
    void combiner_block_40b0c0(bool skip_final){
        auto& b=px.block;
        // Slot arrays of the block, unbounded as on the PC (see pipeline_word).
        auto at=[&](u32 (&array)[9],u32 slot)->u32&{return pipeline_word(array,slot);};
        u32 slot=0;
        for(u32 s=0;s<5;++s){
            if(skip_final&&s==4u)continue;
            const auto& st=px.stage[s];
            const std::int32_t colours=std::int32_t(st.colour_count),alphas=std::int32_t(st.alpha_count);
            const std::int32_t n=colours>alphas?colours:alphas;
            if(n<=0)continue;
            for(std::int32_t k=0;;){
                const auto& e=combiner(s,u32(k));
                if(k<colours){at(b.colour_op,slot)=e.colour_op;at(b.colour_arg,slot)=e.colour_arg;}
                else{at(b.colour_op,slot)=0xc200000;at(b.colour_arg,slot)=0xc0;}
                if(k<alphas){at(b.alpha_op,slot)=e.alpha_op;at(b.alpha_arg,slot)=e.alpha_arg;}
                else{at(b.alpha_op,slot)=0x1c301010;at(b.alpha_arg,slot)=0xc0;}
                at(b.constant_a,slot)=st.entry[0].constant_a;at(b.constant_b,slot)=st.entry[0].constant_b;
                ++slot;
                if(slot==9u)break;   // the next stages still run (past the arrays) as on the PC
                if(++k>=n)break;
            }
        }
        for(u32 s=slot;s<9u;++s){
            b.constant_a[s]=0;b.constant_b[s]=0;b.colour_op[s]=0;b.colour_arg[s]=0;b.alpha_op[s]=0;b.alpha_arg[s]=0;
        }
        b.slots=slot|0x11100u;
        b.bound_mask=(((((px.texture[3].bound<<5)|px.texture[2].bound)<<5)|px.texture[1].bound)<<5)|px.texture[0].bound;
    }
    // 40B200: pixel shader of the combiner block (linker + cache by Adler-32).
    static void append_fragment(std::vector<u32>& out,u32 fragment){
        for(u32 k=1;;++k){const auto t=exe_u(fragment+k*4);if(t==0xffffu)return;out.push_back(t);}
    }
    // Appends the fragment of the first {key, key2, fragment} table entry
    // whose keys match (key2 not compared when null).
    static void append_matching(std::vector<u32>& out,u32 table,unsigned count,u32 key,const u32* key2){
        for(unsigned k=0;k<count;++k)
            if(exe_u(table+k*12)==key&&(!key2||exe_u(table+k*12+4)==*key2)){append_fragment(out,exe_u(table+k*12+8));return;}
    }
    void pixel_shader_40b200(){
        std::array<std::uint8_t,0x108> block;
        std::memcpy(block.data(),&px.block,0x108);
        // A block hashed recently: its Adler-32 (the draws reuse a few blocks).
        u32 hash=0;bool known=false;
        for(const auto& b:g.ps_blocks)if(b.valid&&b.block==block){hash=b.hash;known=true;break;}
        if(!known){
            hash=pc_adler32_43a6f0(block.data(),0x108);
            auto& b=g.ps_blocks[g.ps_block_next];g.ps_block_next=(g.ps_block_next+1u)%u32(g.ps_blocks.size());
            b.block=block;b.hash=hash;b.valid=true;
        }
        auto& shader=g.shader();
        for(const auto& p:g.pixel_shaders_95aeec)if(p.hash==hash){shader.pixel_shader=p.shader;return;}
        auto& b=px.block;
        const bool v14=g.pixel_shader_14();
        std::vector<u32> t{v14?0xffff0104u:0xffff0101u};
        if(!v14){t.push_back(0x51);for(u32 k=1;;++k){const auto v=exe_u(0x61e9c8+k*4);if(v==0xffffu)break;t.push_back(v);}}
        append_matching(t,v14?0x61f5d0u:0x61f560u,9,b.bound_mask,nullptr);
        const u32 n=b.slots&0xfu;
        for(u32 i=0;i<n;++i){
            const auto colour_arg=pipeline_word(b.colour_arg,i);
            if((colour_arg&0xf0u)==0x50u)continue;
            append_matching(t,v14?0x61f440u:0x61f320u,0x18,pipeline_word(b.colour_op,i),&colour_arg);
            const auto alpha_arg=pipeline_word(b.alpha_arg,i);
            append_matching(t,v14?0x61f2b0u:0x61f240u,9,pipeline_word(b.alpha_op,i),&alpha_arg);
        }
        for(u32 a=v14?0x61f21cu:0x61edc8u;;a+=4){const auto v=exe_u(a);t.push_back(v);if(v==0xffffu)break;}
        // D3DXDisassembleShader on failure only feeds the debug output.
        shader.pixel_shader=d.create_pixel_shader(t.data());
        g.pixel_shaders_95aeec.push_back({hash,shader.pixel_shader});
    }
    // 40AF80: apply the pixel pipeline (combiner shader, or a fixed shader
    // for mode 89BC20 != 0).
    void apply_pixel_40af80(){
        using namespace d3d9;
        const bool skip_final=g.shader().skip_final();
        if(!px.mode){
            if(px.dirty){
                combiner_block_40b0c0(skip_final);
                pixel_shader_40b200();
                d.set_pixel_shader(g.shader().pixel_shader);
                light_colours_410d00(true);
                px.applied_mode=px.mode;px.dirty=0;
            }
            if(!px.alpha_test)return;
            rs(RS_COLORWRITEENABLE,skip_final?px.colour_write_rgb:px.colour_write);
            rs(RS_ALPHAREF,0x80);
            return;
        }
        if(px.applied_mode==px.mode)return;
        d.set_pixel_shader(pipeline_word(px.fixed_shader,px.mode));
        light_colours_410d00(false);
        const auto previous=px.applied_mode;
        if((previous==0u||previous==0xcu)&&px.alpha_test){rs(RS_COLORWRITEENABLE,px.colour_write_rgb);rs(RS_ALPHAREF,px.alpha_ref);}
        px.applied_mode=px.mode;px.dirty=1;
    }
    static u32 texture_of(const PcPmtResources& bank,u32 index){
        return pmt_u32(bank.system,pmt_u32(bank.system,bank.texture_table_24)+index*4u);
    }
    // 408F90: render states of the changed material flags (bit 0 specular,
    // 1 two-sided, 3 depth bias in bits 23..26, 4 no fog, 8..15 blend
    // factors, 16..18 blend op, 19..22 fixed shader mode, 27..28 vertex
    // constant 9).
    void material_states_408f90(u32 changed,u32 flags,const PcPmtResources& bank,const PmtMaterial* mat,u32 mirrored){
        using namespace d3d9;
        const auto low=changed&0xffu,now=flags&0xffu;
        if(low){
            if(low&1u)rs(RS_SPECULARENABLE,now&1u);
            if(low&2u)rs(RS_CULLMODE,(now&2u)?CULL_NONE:(mirrored?CULL_CCW:CULL_CW));
            if(low&8u){
                float bias=0.f;
                if(now&8u)bias=st(-(X(std::int32_t((flags>>23)&0xfu))*exe_f(0x628160)));
                rs(RS_DEPTHBIAS,bits(bias));
            }
            if(low&0x10u)rs(RS_FOGENABLE,(now&0x10u)?0u:mc.saved_fog_enable);
        }
        const auto high=changed&0xffffff00u;
        if(!high)return;
        if(high&0xf00u)rs(RS_SRCBLEND,exe_u(0x61e784+((flags>>8)&0xfu)*4));
        if(high&0xf000u)rs(RS_DESTBLEND,exe_u(0x61e784+((flags>>12)&0xfu)*4));
        if(high&0x70000u)rs(RS_BLENDOP,exe_u(0x61e7b0+((flags>>16)&7u)*4));
        if(high&0x780000u){
            const auto old=(mc.material_flags>>19)&0xfu;
            if(old==4u||old==5u){   // undo the cube / reflection textures of stages 2..3
                if(old==5u)texture_matrix_411230(0,1);
                d.set_texture(3,mc.saved_texture3);
                samp(3,SAMP_ADDRESSU,mc.saved_address3);samp(3,SAMP_ADDRESSV,mc.saved_address3);
                if(old==5u)samp(3,SAMP_MAGFILTER,2);
                else mc.texture_key[2]=Unknown;
                mc.texture_key[0]=Unknown;mc.texture_key[1]=Unknown;mc.texture_key[3]=Unknown;
            }
            const auto f=(flags>>19)&0xfu;
            if(f==4u){              // normal cube in stages 2 and 3
                px.mode=4;
                d.set_texture(2,mc.normal_cube);d.set_texture(3,mc.normal_cube);
                samp(2,SAMP_ADDRESSU,3);samp(2,SAMP_ADDRESSV,3);samp(3,SAMP_ADDRESSU,3);samp(3,SAMP_ADDRESSV,3);
                mc.texture_key[2]=None;mc.colour_index=Unknown;mc.texture_key[3]=0x7fffffff;
            }else if(f==5u){        // environment reflection in stage 3
                std::memcpy(tf.slot[3],g.view().environment,64);
                px.mode=5;tf.texture_matrix_type[1]=0x30000;
                texture_matrix_411230(3,1);
                d.set_texture(3,texture_of(bank,mat->environment_texture));
                samp(3,SAMP_ADDRESSU,3);samp(3,SAMP_ADDRESSV,3);samp(3,SAMP_MINFILTER,2);samp(3,SAMP_MAGFILTER,2);
                mc.texture_key[1]=None;mc.texture_key[3]=0x7fffffff;
            }else px.mode=f;
        }
        if(high&0x18000000u)vsc(9,g.reflection().colour_constant[(flags>>27)&3u]);
    }
    // 409430: texture stage of the changed layer flags (bits 0..9 texture
    // coordinate sources, 10..21 sampler address/filter, 21..25 colour mode).
    void texture_stage_409430(u32 changed,u32 s,u32 flags,const PmtLayer* layer){
        using namespace d3d9;
        u32 sampler_flags=flags,high;
        const auto mode_index=exe_u(0x61e750+((flags>>21)&0x1fu)*4);
        auto load_texture_matrix=[&](const M& m,u32 type,u32 mode){
            set_matrix_410f90(m,s+2);tf.texture_matrix_type[s]=type;texture_matrix_411230(mode,s);
        };
        if(!(changed&0x3ffu)){high=changed;}
        else{
            const auto now=flags&0x3ffu,off=~now&(changed&0x3ffu);
            high=changed&0xfffffc00u;
            if(off&0x20u){stage_mode_40b800(s,mode_index);stage_alpha_mode4(s);}
            if(off&1u)stage_mode_40b800(s,mode_index);
            if(off&0x8eu){stage_mode_40b800(s,mode_index);stage_alpha_mode4(s);tf.texture_matrix_type[s]=0;texture_matrix_411230(0,s);}
            if(now&4u){
                load_texture_matrix(exe_matrix(0x61e710),0x10000,2);
                stage_mode_40b800(s,0xd);stage_alpha_mode3(s);
            }
            if(now&8u){
                load_texture_matrix(render_state::load(g.view().environment),0x30000,3);
                stage_mode_40b800(s,0xd);stage_alpha_mode3(s);
            }
            if(now&0x80u){
                load_texture_matrix(render_state::load(g.view().environment),0x30000,3);
                stage_colour_40b710(s,texreg(s)|0x15203510u,0xd00);
                auto& e=combiner(s,px.stage[s].colour_count++);
                px.dirty=1;px.colour_mode[s]=0x1b;e.colour_arg=0xc00;
                e.colour_op=(temp(s)<<8)|(texreg(s)<<24)|0xd0020u;
                if(s!=0u){
                    stage_append_40b760(s,0x5080000,0x50);stage_append_40b760(s,0x5380000,0x20050);
                    stage_alpha_40b7b0(s,0x14301010,0xc0);
                }else{
                    stage_append_40b760(s,0,0x50);
                    stage_alpha_mode3(s);
                }
            }
            if(now&2u){
                load_texture_matrix(render_state::load(g.view().environment),0x30000,3);
                const auto q=temp(s);
                stage_colour_40b710(s,((((q<<16)|q|0x2010u))<<8)|texreg(s),0xc00);
                stage_alpha_default(s,2,(texreg(s)<<24)|0x10301010u);
            }
            if(now&1u){
                const auto t=texreg(s)<<24;
                stage_colour_40b710(s,t|0x50000u,0x50);
                stage_alpha_default(s,4,(temp(s)<<16)|t|0x10101010u);
            }
            if((now&0x20u)&&layer){
                const auto m=layer->bump_matrix;
                tss(s,TSS_BUMPENVMAT00,m);tss(s,TSS_BUMPENVMAT01,0);tss(s,TSS_BUMPENVMAT10,0);tss(s,TSS_BUMPENVMAT11,m);
            }
        }
        if(!high)return;
        if(high&0x3e00000u){
            if(!(flags&0xafu)){stage_mode_40b800(s,mode_index);stage_alpha_mode4(s);}
        }
        if(high&0x1c00u){       // ADDRESSU (mirror-once becomes mirror)
            if((sampler_flags&0x1c00u)==0x1400u)sampler_flags=(sampler_flags&0xffffebffu)|0x800u;
            samp(s,SAMP_ADDRESSU,(sampler_flags>>10)&7u);
        }
        if(high&0xe000u){       // ADDRESSV
            if((sampler_flags&0xe000u)==0xa000u)sampler_flags=(sampler_flags&0xffff5fffu)|0x4000u;
            samp(s,SAMP_ADDRESSV,(sampler_flags>>13)&7u);
        }
        if(high&0x70000u){      // MAG/MINFILTER (3+: anisotropic when enabled)
            const auto f=(sampler_flags>>16)&7u;
            const auto& sh=g.shader();
            if(f<3u){samp(s,SAMP_MAGFILTER,f);samp(s,SAMP_MINFILTER,f);}
            else{samp(s,SAMP_MAGFILTER,sh.anisotropic_mag()?3u:2u);samp(s,SAMP_MINFILTER,sh.anisotropic_min()?3u:2u);}
        }
        if(high&0x180000u){     // MIPFILTER
            if((sampler_flags&0x180000u)>0x100000u)sampler_flags=(sampler_flags&0xffefffffu)|0x80000u;
            samp(s,SAMP_MIPFILTER,(sampler_flags>>19)&3u);
        }
        if((high&0xf0000000u)&&!(sampler_flags&0x8eu))tf.texture_matrix_type[s]=0;
    }
    // 408C80: material of one draw (render states, three texture layers,
    // colours, then the pixel pipeline).
    void material_408c80(const DrawEntry& e,const PcPmtResources& bank,u32 material_offset){
        OR2_PERF_ZONE("408C80 material");
        const auto mat=pmt<PmtMaterial>(bank.system,material_offset);
        if(!bank.texture_table_24)throw std::runtime_error("408C80 bank without texture table (uninitialised on the PC)");
        if(const auto changed=mc.material_flags^mat.flags){
            material_states_408f90(changed,mat.flags,bank,&mat,e.flags&1u);
            mc.material_flags=mat.flags;
        }
        for(u32 s=0;s<3;++s){
            const auto& layer=mat.layer[s];
            if(layer.texture==None){
                if(mc.texture_key[s]==None)continue;
                mc.texture_key[s]=None;
                d.set_texture(s,0);
                if(px.texture[s].bound){px.texture[s].bound=0;px.dirty=1;}
                stage_mode_40b800(s,3);
                stage_alpha_mode3(s);
                texture_matrix_411230(0,s);
                mc.layer_flags[s]=(mc.layer_flags[s]&0xfffffc00u)|0x3e00000u;
                continue;
            }
            u32 key=layer.texture|e.id_high;
            u32 texture=texture_of(bank,layer.texture);
            u32 alt=0;
            if(e.colour_list&&c.colour_alt&&c.colour_alt(e.colour_list,key,e.colour,alt)&&alt!=None){
                key=alt;
                auto* other=c.bank(alt>>16);
                if(!other)throw std::runtime_error("408C80 colour texture bank not loaded");
                if((alt&0xffffu)>=pmt_u32(other->system,8))texture=0;
                else texture=texture_of(*other,alt&0xffffu);
            }
            if(mc.texture_key[s]!=key){
                if(px.texture[s].bound!=1u){px.texture[s].bound=1;px.dirty=1;}
                mc.texture_key[s]=key;
                d.set_texture(s,texture);
            }
            samp(s,d3d9::SAMP_MIPMAPLODBIAS,layer.lod_bias);
            if(const auto changed=mc.layer_flags[s]^layer.flags){
                mc.layer_flags[s]=layer.flags;
                texture_stage_409430(changed,s,layer.flags,&layer);
            }
        }
        const auto object_key=g.object_key(e.resource,e.object); // symbolic object pointer
        if(mc.colour_index!=mat.colour_index||mc.object_key!=object_key){
            const bool ambient_pass=mc.colour_index==Unknown;
            const auto colours=pmt_u32(bank.system,e.object+0x38);
            material_colours_410dd0(bank.system,colours+mat.colour_index*0x48u,ambient_pass);
            mc.colour_index=mat.colour_index;mc.object_key=object_key;
        }
        const std::uint8_t skip_final=std::uint8_t((mat.layer[0].flags>>8)&1u);
        auto& sh=g.shader();
        if(std::uint8_t(sh.skip_final_word)!=skip_final){sh.set_skip_final(skip_final);px.dirty=1;}
        apply_pixel_40af80();
    }
    // Shader record of (group, material): object +0C, stride = material count.
    static PmtShaderRecord shader_record(const std::vector<std::uint8_t>& sys,const PmtObject& obj,u32 material,u32 group){
        const auto materials=pmt_u32(sys,obj.info+0x2c);
        return pmt<PmtShaderRecord>(sys,obj.shader_records+(materials*group+material)*0x18u);
    }
    // 410680: declaration and vertex shader of a material.
    void shader_410680(const PmtShaderRecord& r){
        d.set_vertex_declaration(r.declaration);
        d.set_vertex_shader(r.vertex_shader);
        if(r.keeps_light_state)return;
        anim.unknown_89ede8=0;anim.unknown_89ede0[0]=0;
        if(anim.specular_request!=0u)anim.specular_request=0;
    }
    // 40ECC0: texture animation constant 7 (blend of two frames).
    void animation_40ecc0(u32 index){
        if(index>=0x400u)return;
        X f=X(exe_f(0x619a34));
        if(const auto& e=anim.table[index];std::int32_t(e.count)>0){   // the morph_var.dat block (40ED70): [(base + clock) % count]
            const std::size_t at=std::size_t(e.unknown)+std::size_t((e.base+anim.clock)%e.count);
            if(at>=g.animation_values.size())throw std::runtime_error("40ECC0: animation value outside morph_var.dat");
            f=X(g.animation_values[at]);
        }
        const auto whole=ftol(f);
        const X frac=f-fild_u(u32(whole));
        const Vec4 v{st(X(1.0f)-frac),st(frac),0.f,0.f};
        vsc(7,v.data(),1);
        anim.frame=u32(whole);
    }
    // 404700: vertex size of a declaration (offset + size of its last element).
    u32 declaration_size(u32 decl){
        if(const auto it=g.declaration_sizes.find(decl);it!=g.declaration_sizes.end())return it->second;
        std::array<PcVertexElement,PcMaxFvfDeclSize> el{};
        const auto count=d.get_declaration(decl,el.data());
        if(count<2u||count>PcMaxFvfDeclSize)throw std::runtime_error("404700 declaration without elements");
        const auto& last=el[count-2u];
        u32 size=last.offset;
        switch(last.type){case 0:size+=4;break;case 1:size+=8;break;case 2:case 4:size+=12;break;default:break;}
        g.declaration_sizes.emplace(decl,size);
        return size;
    }
    // 404700: draw one queue entry (each primitive of its mesh, each draw
    // record of the entry's pass).
    void draw_404700(const DrawEntry& e,const PcPmtResources& bank){
        OR2_PERF_ZONE("404700 draw entry");
        using namespace d3d9;
        const auto& sys=bank.system;
        const auto obj=pmt<PmtObject>(sys,e.object);
        const auto mesh=pmt<PmtMesh>(sys,e.mesh);
        if(e.flags&1u)rs(RS_CULLMODE,CULL_CCW);
        for(std::int32_t i=0;i<mesh.primitives;++i){
            const auto prim=pmt<PmtPrimitive>(sys,obj.primitives+(mesh.first_primitive+u32(i))*0x14u);
            const auto group=pmt<PmtGroup>(sys,obj.groups+prim.group*0x2cu);
            const auto index_buffer=pmt_u32(sys,obj.index_buffers+prim.group*4u);
            const auto stride=group.stride;
            const auto vbs=obj.vertex_buffers+prim.group*16u;
            if(e.morph_target){   // 405450 morph: stream 1 = the target object's buffer of the group
                const auto target=pmt<PmtObject>(sys,e.morph_target);
                d.set_stream_source(0,pmt_u32(sys,vbs),0,stride);
                d.set_stream_source(1,pmt_u32(sys,target.vertex_buffers+prim.group*16u),0,stride);
            }else if(group.frames<=1)d.set_stream_source(0,pmt_u32(sys,vbs),0,stride);
            else{                 // animated vertices: frames k and k+1 of 40ECC0
                u32 k=anim.frame;
                if(k>=u32(group.frames))k%=u32(group.frames);
                u32 k1=k+1u;if(k1>=u32(group.frames))k1=0;
                d.set_stream_source(0,pmt_u32(sys,vbs+k*4u),0,stride);
                d.set_stream_source(1,pmt_u32(sys,vbs+k1*4u),0,stride);
            }
            for(std::int32_t k=0;k<prim.draws[e.pass];++k){
                const auto dr=pmt<PmtDraw>(sys,obj.draws+(prim.first_draw[e.pass]+u32(k))*0x20u);
                const auto shader=shader_record(sys,obj,dr.material,prim.group);
                const auto size=declaration_size(shader.declaration);
                shader_410680(shader);
                d.set_indices(index_buffer);
                material_408c80(e,bank,obj.materials+dr.material*0x58u);
                if(stride!=size)continue;   // vertex layout the stream cannot feed: skipped as on the PC
                const auto range=pmt<PmtRange>(sys,obj.ranges+dr.range*16u);
                const auto vertices=group.vertex_bytes/stride-dr.base_vertex;
                (void)d.draw_indexed_primitive(5,std::int32_t(dr.base_vertex),0,vertices,range.start,range.count);
            }
        }
        if(e.flags&1u)rs(RS_CULLMODE,CULL_CW);
    }
    // 405890: flush of a queue in `order` (opaque pass back to front over
    // node flag 1, then blended pass front to back over node flag 2). The
    // world and palette matrices are reloaded only when their contents change.
    struct MatrixKey { u32 tag{},index{}; bool operator==(const MatrixKey& o)const{return tag==o.tag&&index==o.index;} };
    M matrix_of(PcRenderQueue& q,const MatrixKey& k){
        if(k.tag==0)return render_state::load(tf.slot[6]);
        if(k.index>=q.matrices.size())throw std::out_of_range("405890 matrix pool");
        M m{};std::memcpy(m.data(),q.matrices[k.index].data(),64);return m;
    }
    void flush(PcRenderQueue& q,const std::vector<u32>& order){
        const std::int32_t count=std::int32_t(q.count);
        if(count<=0)return;
        OR2_PERF_ZONE("405890 flush");
        if(order.size()<std::size_t(count))throw std::out_of_range("405890 pointer array");
        const u32 tag=c.queue_tag?c.queue_tag:1u;
        std::array<MatrixKey,64> last{};
        std::int32_t valid=0;
        begin_408af0();
        auto load_matrices=[&](const DrawEntry& e){
            OR2_PERF_ZONE("405890 matrices");
            const MatrixKey world{tag,e.matrix};
            if(!(last[0]==world)){
                const auto a=matrix_of(q,last[0]),b=matrix_of(q,world);
                if(std::memcmp(a.data(),b.data(),64)!=0){
                    render_state::store(tf.slot[6],b);world_411060(b);last[0]=world;valid=0;
                }
            }
            const std::int32_t n=std::int32_t(e.palette_count);
            for(std::int32_t k=1;k<n;++k){
                const MatrixKey key{tag,e.matrix+u32(k)};
                if(k>=64)throw std::out_of_range("405890 palette");
                if(valid>=k){
                    if(last[std::size_t(k)]==key)continue;
                    const auto a=matrix_of(q,last[std::size_t(k)]),b=matrix_of(q,key);
                    if(std::memcmp(a.data(),b.data(),64)==0)continue;
                }else valid=k;
                const auto b=matrix_of(q,key);
                // Slots 7.. without a bound: a long palette runs on over the
                // derived matrices 95DAE0.. as on the PC.
                const std::size_t word=std::size_t(7+k-1)*16u;
                if(word+16u>sizeof(render_state::TransformState)/4u)throw std::out_of_range("PC renderer global outside the modelled ranges");
                std::memcpy(reinterpret_cast<u32*>(&tf)+word,b.data(),64);
                if(k+6<=9)palette_4111c0(b,u32(k+6));
                last[std::size_t(k)]=key;
            }
        };
        // Node flags (+00) and texture animation (+18) of the entry's node.
        auto extras=[&](const DrawEntry& e,const PcPmtResources& bank,u32 flags){
            if(e.morph_target){
                const float a=f32(e.morph_weight);
                const Vec4 v{a,st(X(1.0f)-a),0.f,0.f};vsc(7,v.data(),1);
            }else if(flags&0x20u)animation_40ecc0(pmt_u32(bank.system,e.node+0x18));
        };
        auto bank_of=[&](const DrawEntry& e)->PcPmtResources&{
            auto* bank=c.bank(e.resource);if(!bank)throw std::runtime_error("405890 entry bank not loaded");
            return *bank;
        };
        if(pass_404600(0)){
            for(std::int32_t i=count-1;i>=0;--i){
                auto& e=entry(q.entries.at(order[std::size_t(i)]));
                auto& bank=bank_of(e);
                const auto flags=pmt_u32(bank.system,e.node);
                if(!(flags&1u))continue;
                load_matrices(e);
                e.pass=0;
                extras(e,bank,flags);
                draw_404700(e,bank);
            }
        }
        if(pass_404600(1)){
            for(std::int32_t i=0;i<count;++i){
                auto& e=entry(q.entries.at(order[std::size_t(i)]));
                auto& bank=bank_of(e);
                const auto flags=pmt_u32(bank.system,e.node);
                if(!(flags&2u))continue;
                load_matrices(e);
                e.pass=1;
                if(flags&0x200u)throw std::runtime_error("405CF0 per-primitive path (node flag 0x200) is not ported; no retail PMT uses it");
                extras(e,bank,flags);
                draw_404700(e,bank);
            }
        }
        end_408bb0();
        d.set_indices(0);
        d.set_stream_source(0,0,0,0);
        d.set_stream_source(1,0,0,0);
    }
};
}
PcRenderGlobals::PcRenderGlobals(){
    const std::pair<std::uint32_t,std::uint32_t> spans[]={
        {0x897d30,0x30},{0x89a4f8,0x60},{0x8a89f0,0x10},{0x89b5b4,0x794},{0x89bdb4,0x3038},{0x8a8a00,0x25c},
        {0x95aee8,0x28},{0x95bf40,0x118},{0x95d860,0x6a4},{0x740c98,0x8},
        // 404250 words (the 8999A0 depth lives in PcRenderContext): 8999A4..8999AC,
        // 860D28..860EBC (zeroed; 860EB8 current list), 893D20, shadow targets 95AFC4..95AFCC.
        {0x8999a4,0xc},{0x860d28,0x198},{0x893d20,0x4},{0x95afc4,0xc}};
    for(const auto& [base,size]:spans)ranges.push_back({base,size,std::vector<std::uint32_t>(size/4,0)});
    build_table();
    w(0x740c9c)=pc_shader_data_u32(0x740c9c); // static .data flag (never written by code)
}
void PcRenderGlobals::build_table(){
    table_.clear();if(ranges.empty()||ranges.size()>=0xffu)return;
    std::uint32_t lo=0xffffffffu,hi=0;
    for(const auto& r:ranges){lo=std::min(lo,r.base&~3u);hi=std::max(hi,r.base+r.size);}
    table_base_=lo;table_.assign((hi-lo+3u)/4u,0xffu);
    // Later ranges do not overlap earlier ones; the first match wins like the search.
    for(std::size_t k=ranges.size();k-->0;){const auto& r=ranges[k];
        for(std::uint32_t a=r.base;a<r.base+r.size;a+=4u)if(!(a&3u))table_[(a-lo)>>2]=std::uint8_t(k);}
}
std::uint32_t& PcRenderGlobals::w_search(std::uint32_t a){
    if(a&3u)throw std::invalid_argument("unaligned PC renderer global");
    for(std::size_t k=0;k<ranges.size();++k){auto& r=ranges[k];if(a>=r.base&&a-r.base<r.size){last_range=k;return r.words[(a-r.base)/4];}}
    throw std::out_of_range("PC renderer global outside the modelled ranges");
}
float PcRenderGlobals::f(std::uint32_t a){float v;const auto u=w(a);std::memcpy(&v,&u,4);return v;}
void PcRenderGlobals::putf(std::uint32_t a,float v){std::uint32_t u;std::memcpy(&u,&v,4);w(a)=u;}
std::uint8_t PcRenderGlobals::byte(std::uint32_t a){return std::uint8_t(w(a&~3u)>>((a&3u)*8));}
void PcRenderGlobals::put_byte(std::uint32_t a,std::uint8_t v){
    auto& x=w(a&~3u);const auto sh=(a&3u)*8;x=(x&~(0xffu<<sh))|(std::uint32_t(v)<<sh);}
std::uint32_t PcRenderGlobals::object_key(std::uint32_t resource,std::uint32_t offset){
    const auto k=(std::uint64_t(resource)<<32)|offset;
    if(k==object_key_last)return object_key_last_id;
    const auto it=object_keys.find(k);
    const auto id=it!=object_keys.end()?it->second:std::uint32_t(object_keys.size()+1u)|0x80000000u;
    if(it==object_keys.end())object_keys.emplace(k,id);
    object_key_last=k;object_key_last_id=id;return id;
}
std::array<float,16> PcRenderGlobals::matrix(std::uint32_t a){
    std::array<float,16> m{};
    if(const auto* p=span(a,64)){std::memcpy(m.data(),p,64);return m;}
    for(unsigned k=0;k<16;++k)m[k]=f(a+k*4);return m;}
void PcRenderGlobals::put_matrix(std::uint32_t a,const std::array<float,16>& m){
    if(auto* p=span(a,64)){std::memcpy(p,m.data(),64);return;}
    for(unsigned k=0;k<16;++k)putf(a+k*4,m[k]);}
bool pc_colour_list_alt_408c80(std::uint32_t list,std::uint32_t key,std::uint32_t index,std::uint32_t& alt){
    for(std::uint32_t p=list;;p+=4){
        const auto entry=pc_shader_data_u32(p);
        if(!entry)return false;
        if(pc_shader_data_u32(entry)==key){alt=pc_shader_data_u32(entry+index*4u);return true;}
    }
}
void render_queue_sort_4499e0(const PcRenderQueue& q,std::vector<std::uint32_t>& a,std::int32_t lo,std::int32_t hi){
    auto key=[&](std::int32_t i){float f;std::memcpy(&f,&q.entries.at(a.at(std::size_t(i)))[0],4);return f;};
    while(lo<hi){
        const float pivot=key((lo+hi)/2);
        std::int32_t i=lo,j=hi;
        for(;;){
            while(pivot>key(i))++i;
            while(key(j)>pivot)--j;
            if(i>=j)break;
            std::swap(a[std::size_t(i)],a[std::size_t(j)]);++i;--j;
        }
        render_queue_sort_4499e0(q,a,lo,i-1);
        lo=j+1;
    }
}
void render_queue_flush_405890(PcFlushContext& c,PcRenderQueue& q,const std::vector<std::uint32_t>& order){
    Flush f(c);f.flush(q,order);
}
void render_environment_matrix_4089a0(PcFlushContext& c,std::int32_t env,const std::array<float,16>& m){
    auto& base=c.g.view().environment_base;
    if(env==1){
        M inv=render_state::load(base);
        if(driving::pc_d3dx_matrix_inverse(inv,m))render_state::store(base,inv);
    }else render_state::store(base,Identity);
    Flush f(c);f.environment_408a80();
}
void render_queue_flush_4052c0(PcFlushContext& fc,PcRenderContext& rc){
    OR2_PERF_ZONE("4052C0 opaque flush");
    std::vector<std::uint32_t> order(rc.opaque.count);
    for(std::uint32_t k=0;k<rc.opaque.count;++k)order[k]=k;
    render_queue_flush_405890(fc,rc.opaque,order);
    for(auto it=rc.opaque.undo.rbegin();it!=rc.opaque.undo.rend();++it){
        auto* bank=fc.bank(it->resource);
        if(!bank)throw std::runtime_error("4052C0 undo bank not loaded");
        bank->view().put32(it->offset,it->value);
    }
    rc.opaque.undo.clear();rc.opaque.count=0;rc.opaque.matrix_count=0;rc.immediate_8999b0=1;
}
void render_set_matrix_410f90(PcFlushContext& c,const std::array<float,16>& m,std::uint32_t slot){
    Flush f(c);f.set_matrix_410f90(m,slot);
}
void PcRenderStageOps::mode_40b800(std::uint32_t s,std::uint32_t mode){Flush f(c);f.stage_mode_40b800(s,mode);}
void PcRenderStageOps::colour_40b710(std::uint32_t s,std::uint32_t op,std::uint32_t arg){Flush f(c);f.stage_colour_40b710(s,op,arg);}
void PcRenderStageOps::alpha_40b7b0(std::uint32_t s,std::uint32_t op,std::uint32_t arg){Flush f(c);f.stage_alpha_40b7b0(s,op,arg);}
void PcRenderStageOps::alpha_default3(std::uint32_t s){Flush f(c);f.stage_alpha_mode3(s);}
void PcRenderStageOps::constants_40b4b0(std::uint32_t a,std::uint32_t s,std::uint32_t b){Flush f(c);f.combiner_constants_40b4b0(a,s,b);}
void PcRenderStageOps::bound(std::uint32_t s,std::uint32_t value){
    auto& px=c.g.pixel();
    if(px.texture[s].bound!=value){px.texture[s].bound=value;px.dirty=1;}
}
void PcRenderStageOps::texture_matrix_411230(std::uint32_t mode,std::uint32_t s,std::uint32_t type){
    c.g.transform().texture_matrix_type[s]=type;
    Flush f(c);f.texture_matrix_411230(mode,s);
}
void PcRenderStageOps::sampler(std::uint32_t s,std::uint32_t type,std::uint32_t value){Flush f(c);f.samp(s,type,value);}
// 4116F0 / 4117D0: an object's node tree drawn at once with the current device
// states (the node matrices pushed on the stack and loaded by 411060).
namespace {
void pmt_mesh_draw_4117d0(Flush& f,const std::vector<std::uint8_t>& sys,const PmtObject& obj,u32 mesh_offset){
    auto& d=f.d;
    const auto mesh=pmt<PmtMesh>(sys,mesh_offset);
    for(std::int32_t i=0;i<mesh.primitives;++i){
        const auto prim=pmt<PmtPrimitive>(sys,obj.primitives+(mesh.first_primitive+u32(i))*0x14u);
        const auto group=pmt<PmtGroup>(sys,obj.groups+prim.group*0x2cu);
        const auto index_buffer=pmt_u32(sys,obj.index_buffers+prim.group*4u);
        d.set_stream_source(0,pmt_u32(sys,obj.vertex_buffers+prim.group*16u),0,group.stride);
        for(u32 pass=0;pass<2u;++pass){
            for(std::int32_t k=0;k<prim.draws[pass];++k){
                const auto dr=pmt<PmtDraw>(sys,obj.draws+(prim.first_draw[pass]+u32(k))*0x20u);
                f.shader_410680(Flush::shader_record(sys,obj,dr.material,prim.group));
                d.set_indices(index_buffer);
                const auto range=pmt<PmtRange>(sys,obj.ranges+dr.range*16u);
                const auto vertices=group.vertex_bytes/group.stride-dr.base_vertex;
                (void)d.draw_indexed_primitive(range.type,0,0,vertices,range.start,range.count);
            }
        }
    }
    d.set_indices(0);
    d.set_stream_source(0,0,0,0);
}
void pmt_node_draw_4116f0(Flush& f,driving::PcMatrixStack& st,const std::vector<std::uint8_t>& sys,const PmtObject& obj,u32 node){
    auto load_world=[&]{M m{};std::memcpy(m.data(),st.current().data(),64);render_state::store(f.tf.slot[6],m);f.world_411060(m);};
    for(;;){
        const std::int32_t matrix=std::int32_t(pmt_u32(sys,node+0x1c));
        if(matrix>=0){
            if(st.depth+1<st.capacity){
                const auto local=pmt<M>(sys,obj.matrices+u32(matrix)*0x40u);
                M parent{};std::memcpy(parent.data(),st.current().data(),64);
                const auto r=driving::pc_d3dx_matrix_multiply(local,parent);
                st.current_offset+=0x40;
                std::memcpy(st.current().data(),r.data(),64);
            }
            ++st.depth;
            load_world();
        }
        pmt_mesh_draw_4117d0(f,sys,obj,obj.meshes+pmt_u32(sys,node+0x28)*8u);
        if(const std::int32_t child=std::int32_t(pmt_u32(sys,node+0x20));child>=0)pmt_node_draw_4116f0(f,st,sys,obj,obj.nodes+u32(child)*0x38u);
        if(matrix>=0){
            if(--st.depth>=0)st.current_offset-=0x40;
            load_world();
        }
        const std::int32_t next=std::int32_t(pmt_u32(sys,node+0x24));
        if(next<0)return;
        node=obj.nodes+u32(next)*0x38u;
    }
}
}
bool pmt_object_draw_4116f0(PcFlushContext& c,PcPmtResources& bank,std::uint32_t index,driving::PcMatrixStack& st){
    const auto& sys=bank.system;
    if(index>=pmt_u32(sys,bank.object_count_0c))return false;
    Flush f(c);
    const auto obj=pmt<PmtObject>(sys,pmt_u32(sys,bank.object_table_20)+index*0x3cu);
    pmt_node_draw_4116f0(f,st,sys,obj,obj.nodes);
    return true;
}
void render_environment_40d840(PcFlushContext& c,std::int32_t env,std::uint8_t flags_79fcca,const PcEnvironmentRenderTables& t){
    if((flags_79fcca&3u)!=2u||env==-1)return;
    if(env<0||env>2)throw std::runtime_error("40D840 environment index");
    Flush f(c);
    const auto e=std::uint32_t(env);
    f.env_fog_40d870(t.fog_7d3a10.sub(e*0x1c,0x1c),t.pixel_fog_740c88!=0);
    auto light=[&](std::uint32_t address){return t.lights_899b98.sub(address-0x899b98u,0xa0);};
    f.env_colours_40dd70(light(0x899b98+e*0xa0));
    f.lights_reset_410740();
    f.env_light(0,light(0x899b98+e*0xa0));
    f.env_light(1,light(0x89a138+e*0x140));f.env_light(2,light(0x89a1d8+e*0x140));
    f.env_light(3,light(0x899d78+e*0x140));f.env_light(4,light(0x899e18+e*0x140));
}
void render_lights_reset_410740(PcFlushContext& c){Flush f(c);f.lights_reset_410740();}
void render_light_ambient_410710(PcFlushContext& c,const std::array<float,4>& v){
    auto& ambient=c.g.transform().ambient;for(unsigned k=0;k<4;++k)ambient[k]=bits(v[k]);
}
void render_light_add_4107a0(PcFlushContext& c,driving::Bytes desc){desc.check(0,0x94);Flush f(c);f.light_add_4107a0(desc);}
void render_pass_record_4044f0(PcRenderGlobals& g,std::uint32_t pass,std::uint32_t a1,std::uint32_t a2,std::uint32_t a3,std::uint32_t a4){
    auto& p=g.pass(pass);
    p.enabled=a4!=0u;p.colour_write=a4&0xfu;p.z_write=(a4>>28)&1u;p.alpha_blend=a1;p.alpha_test=a2;p.alpha_ref=a3;
}
void render_pass_defaults_404540(PcRenderGlobals& g,std::uint32_t layer){
    auto& opaque=g.pass(0);auto& blended=g.pass(1);
    opaque.alpha_blend=0;opaque.alpha_ref=0x80;blended.z_write=0;blended.alpha_test=0;blended.alpha_ref=8;
    opaque.enabled=1;opaque.z_write=1;blended.alpha_blend=1;
    if(layer==0u){opaque.colour_write=0xf;opaque.alpha_test=0;blended.colour_write=0xf;blended.enabled=1;}
    else if(layer==2u){opaque.colour_write=0;opaque.alpha_test=0;blended.enabled=0;blended.colour_write=0;}
    else{opaque.colour_write=0xf;opaque.alpha_test=1;blended.colour_write=7;blended.enabled=1;}
}
void render_state_init_40f6b0(PcFlushContext& c){
    auto& anim=c.g.animation();auto& tf=c.g.transform();
    anim.unknown_89ede8=0;anim.unknown_89edbc[0]=anim.unknown_89edbc[1]=anim.unknown_89edbc[2]=0;
    anim.unknown_89edd4[0]=anim.unknown_89edd4[1]=anim.unknown_89edd4[2]=0;anim.unknown_89ede0[0]=0;anim.clock=0;
    auto constant=[&](u32 reg,u32 va){const Vec4 v{exe_f(va),exe_f(va+4),exe_f(va+8),exe_f(va+12)};c.device.set_vertex_shader_constant_f(reg,v.data(),1);};
    constant(0,0x625668);constant(1,0x625658);
    // The ten slots and the three derived matrices 95DAE0..95DB60.
    for(u32 k=0;k<10;++k)render_state::store(tf.slot[k],Identity);
    render_state::store(tf.view_projection,Identity);render_state::store(tf.world_view,Identity);
    render_state::store(tf.world_view_projection,Identity);
    for(u32 k=0;k<4;++k){tf.texture_matrix_type[k]=0;tf.texture_matrix_mode[k]=0;tf.texture_matrix_eye[k]=0;}
    tf.directional_count=0;tf.spot_count=0;tf.point_count=0;
    for(auto& on:tf.light_enabled)on=0;
    anim.specular_variant=0;anim.specular_colours=0;anim.specular_request=0;
    constant(0x10,0x6222cc);
}
// ---- renderer initialisation 404250 ----
void render_reset_states_408880(PcFlushContext& c,std::uint32_t layer_7d25f0){
    render_pass_defaults_404540(c.g,layer_7d25f0);                 // 404540 (448DA0 = 7D25F0)
    Flush f(c);
    (void)f.pass_404600(0);                                        // 404600(eax = 0)
    // Bridge 447EFE: eax = [89BD60]; 408892 GetRenderState(FOGENABLE, &89A54C).
    auto& mc=c.g.material();
    mc.saved_fog_enable=c.device.get_render_state(d3d9::RS_FOGENABLE);
    mc.material_flags=0;
    PcPmtResources none{};                                         // 408F90(eax=-1; 0, 0, 0, 0)
    f.material_states_408f90(0xffffffffu,0,none,nullptr,0);
    for(u32 s=0;s<3;++s){mc.layer_flags[s]=0x22400;f.texture_stage_409430(0xffffffffu,s,0x22400,nullptr);}
}
void render_pixel_state_init_40ac60(PcFlushContext& c){
    auto& d=c.device;auto& px=c.g.pixel();
    // 40AF40 -> 420B00(eax = 89B5B4): the fixed pixel shaders 1..5, then 6..11 from the table 73DBE4.
    const u32 fixed[5]{0x623cf0,0x623d40,0x623da8,0x623bb0,0x623c88};
    for(unsigned k=0;k<5;++k){const auto t=pc_shader_tokens(fixed[k]);if(const auto h=d.create_pixel_shader(t.data()))px.fixed_shader[1+k]=h;}
    for(unsigned k=0;k<6;++k){const auto t=pc_shader_tokens(exe_u(0x73dbe4+k*4));if(const auto h=d.create_pixel_shader(t.data()))px.fixed_shader[6+k]=h;}
    for(auto& st:px.stage){
        st.colour_count=0;st.alpha_count=0;
        for(auto& e:st.entry)e=render_state::CombinerEntry{0,0,0,0,0,0,0xf,0xf};
    }
    for(u32 s=0;s<4;++s)px.texture[s]=render_state::StageTexture{0,0,0,s>1u?s-1u:0u};
    std::memset(&px.block,0,sizeof px.block);
    d.set_pixel_shader(0);
    auto& b=px.block;
    b.unknown_89bd1c[2]=None;b.unknown_89bd1c[3]=None;            // 89BD24 / 89BD28
    const bool reset_ce8=b.unknown_89bce4[1]!=0;                  // cmp [89BCE8],0 (flags kept to 40ADED)
    px.dirty=0;px.mode=0;
    for(u32 k=0;k<4;++k){px.colour_mode[k]=1;px.alpha_mode[k]=1;}
    for(u32 k=0;k<9;++k){b.constant_a[k]=0;b.constant_b[k]=0;}
    b.unknown_89bce4[0]=0;b.bound_mask=0;b.unknown_89bd1c[0]=0;b.unknown_89bd1c[1]=0x210000;
    if(reset_ce8||b.unknown_89bce4[2]!=0){b.unknown_89bce4[1]=0;b.unknown_89bce4[2]=0;px.dirty=1;}
    if(b.unknown_89bd1c[4]!=0x1ffu){b.unknown_89bd1c[4]=0x1ff;px.dirty=1;}
    if(b.unknown_89bc4c[0]!=0x130c0305u){b.unknown_89bc4c[0]=0x130c0305;px.dirty=1;}
    if(b.unknown_89bc4c[1]!=0x1c80u){b.unknown_89bc4c[1]=0x1c80;px.dirty=1;}
    px.alpha_test=0;px.colour_write_rgb=0;px.colour_write=0;px.alpha_ref=0;px.unknown_89bc14=0;px.alpha_scale=0;
}
std::uint32_t render_normal_cube_420b80(PcFlushContext& c,std::uint32_t n,std::uint32_t& out){
    auto& d=c.device;
    const auto cube=d.create_cube_texture(n,1,0,0x16,1);           // X8R8G8B8, D3DPOOL_MANAGED
    if(!cube)return 0x80004005u;
    out=cube;
    // 440D10/440D50 .. 580253 .. 440CD0: a temporary n*n texel buffer.
    std::vector<std::uint32_t> texels(std::size_t(n)*n);
    const X one=X(1.0f),half_range=X(127.5f);
    auto coord=[&](std::uint32_t k,float n1){X t=fild_u(k)/X(n1);t=t+t;return t-one;};  // fild; fdiv; fadd st,st; fsub 1
    for(std::uint32_t face=0;face<6;++face){
        const auto surface=d.get_cube_map_surface(cube,face,0);
        std::uint32_t* p=texels.data();
        if(n){
            const float n1=st(fild_u(n-1));                         // fild; fstp float
            for(std::uint32_t i=0;i<n;++i){
                const float v=st(coord(i,n1));                      // fstp [esp+10]
                for(std::uint32_t j=0;j<n;++j){
                    const X u=coord(j,n1);                          // stays in ST0
                    driving::PcVec3 q{};
                    switch(face){
                    case 0:q={1.f,st(-X(v)),st(-u)};break;
                    case 1:q={-1.f,st(-X(v)),st(u)};break;
                    case 2:q={st(u),1.f,v};break;
                    case 3:q={st(u),-1.f,st(-X(v))};break;
                    case 4:q={st(u),st(-X(v)),1.f};break;
                    default:q={st(-u),st(-X(v)),-1.f};break;
                    }
                    q=driving::pc_d3dx_vec3_normalize(q);           // 4393E8 (in place)
                    auto chan=[&](float x){return std::uint32_t(ftol((X(x)+one)*half_range));};
                    std::uint32_t px=(chan(q[0])|0xffffff00u)<<8;
                    px|=chan(q[1]);px<<=8;px|=chan(q[2]);
                    *p++=px;
                }
            }
        }
        std::uint32_t pitch=0;
        auto* bits=d.lock_rect(surface,pitch);
        if(!bits)throw std::runtime_error("420B80: cube map surface lock failed");
        std::memcpy(bits,texels.data(),texels.size()*4u);            // rep movs n*n*4 bytes (pitch not used)
        d.unlock_rect(surface);
        d.release(surface);
    }
    return 0;
}
void render_material_census(PcFlushContext& c,std::uint32_t resource){
    auto* bank=c.bank(resource);
    if(!bank)throw std::runtime_error("census bank not loaded");
    Flush f(c);f.begin_408af0();(void)f.pass_404600(0);
    const auto& sys=bank->system;
    for(u32 i=0;i<bank->object_count_0c;++i){
        const auto o=pmt_u32(sys,bank->object_table_20)+i*0x3cu;
        const auto obj=pmt<PmtObject>(sys,o);
        const std::int32_t count=std::int32_t(pmt_u32(sys,obj.info+0x2c));
        for(std::int32_t k=0;k<count;++k){
            std::array<u32,15> words{};auto& e=entry(words);e.resource=resource;e.object=o;
            try{f.material_408c80(e,*bank,obj.materials+u32(k)*0x58u);}catch(const std::exception&){}
        }
    }
    f.end_408bb0();
}
std::uint32_t renderer_init_404250(PcFlushContext& c,PcRenderContext& queue,std::uint32_t layer_7d25f0){
    auto& d=c.device;auto& g=c.g;std::uint32_t gaps=0;
    d.set_render_state(0x07,1);d.set_render_state(0x8e,1);d.set_render_state(0x8d,1);
    // 4041E0: alpha test on, ALPHAFUNC >=, pixel constant 3 = 0.
    d.set_render_state(0x0f,1);d.set_render_state(0x19,7);
    {const float zero[4]{};d.set_pixel_shader_constant_f(3,zero,1);}
    render_pass_defaults_404540(g,layer_7d25f0);
    {Flush f(c);(void)f.pass_404600(0);}
    render_state_init_40f6b0(c);
    render_queues_reset_405160(queue.opaque,queue.alpha);
    // 4083F0
    d.set_render_state(0x92,0);d.set_render_state(0x91,1);d.set_render_state(0x93,1);d.set_render_state(0x94,0);
    {   Flush f(c);
        for(std::uint32_t s=0;s<4;++s){
            f.tss(s,0x1c,1);f.tss(s,1,4);f.tss(s,2,2);f.tss(s,3,1);f.tss(s,4,4);f.tss(s,5,2);f.tss(s,6,1);
            d.set_texture(s,0);
        }
        for(std::uint32_t s=0;s<4;++s)f.tss(s,0x16,0x3f800000u);   // BUMPENVLSCALE 1.0
    }
    render_pixel_state_init_40ac60(c);
    if(render_normal_cube_420b80(c,0x40,g.material().normal_cube)!=0u)gaps|=RendererInitNoCube;
    render_reset_states_408880(c,layer_7d25f0);
    {auto& v=g.view();render_state::store(v.inverse_view,Identity);render_state::store(v.environment_base,Identity);
     render_state::store(v.environment,Identity);}
    // back in 404250
    queue.flag_depth_8999a0=0;g.w(0x8999a4)=0;
    // 4227C0: shadow targets (D3DXCreateTexture resolves to CreateTexture
    // with these parameters; GetSurfaceLevel dereferences 95AFC8).
    if(const auto ds=d.create_depth_stencil_surface(0x200,0x200,0x4b,0,0,0))g.w(0x95afcc)=ds;
    if(const auto t=d.create_texture(0x200,0x200,1,1,0x17,0))g.w(0x95afc8)=t;
    if(!g.w(0x95afc8))gaps|=RendererInitNoShadowTexture;
    else if(const auto s=d.get_surface_level(g.w(0x95afc8),0))g.w(0x95afc4)=s;
    g.w(0x860eb8)=0x8606f0u;g.w(0x893d20)=0x8606f0u;
    for(std::uint32_t a=0x860d28;a<0x860eb6;++a)g.put_byte(a,0);
    g.w(0x8999ac)=0;g.w(0x8999a8)=0;
    return gaps;
}
}

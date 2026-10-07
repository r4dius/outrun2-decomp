#pragma once
// Private helpers of the PART_EFC port (race_particles*.cpp).
#include "platform/race_particles.hpp"
#include "driving/pc_matrix_stack.hpp"
#include "driving/pc_x87.hpp"
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <string>
namespace outrun::platform::particles_detail {
using X=driving::X87;
inline float st(X v){return driving::x87_float(v);}
inline float fb(std::uint32_t u){float f;std::memcpy(&f,&u,4);return f;}
inline std::uint32_t bf(float f){std::uint32_t u;std::memcpy(&u,&f,4);return u;}
// _ftol2 (582194): truncation to int64, EAX = low word.
inline std::uint32_t ftol(X v){return std::uint32_t(driving::x87_ftol64(v));}
// fcomp/fcom flag tests. C0: a < b or unordered; C3: a == b or unordered;
// C2: unordered.
inline bool c0(X a,X b){return !(a>=b);}
inline bool c3(X a,X b){return !(a<b)&&!(a>b);}
inline bool c0c3(X a,X b){return !(a>b);}                  // test ah,0x41 != 0
inline bool c2(X a,X b){return !(a<b)&&!(a>b)&&!(a==b);}
// test ah,5; jp: jumps when C0 and C2 are equal (both clear: a >= b, or
// both set: unordered). Returns true when the jump is taken.
inline bool jp5(X a,X b){return (a>=b)||c2(a,b);}
struct P {
    PcParticleContext& pc;PcRaceContext& c;PcRaceMemory& m;
    explicit P(PcParticleContext& p):pc(p),c(p.race),m(p.race.m){}
    std::uint32_t u(std::uint32_t a)const{return m.u32(a);}
    std::int32_t i(std::uint32_t a)const{return m.i32(a);}
    float f(std::uint32_t a)const{return m.f32(a);}
    X x(std::uint32_t a)const{return X(m.f32(a));}
    std::uint16_t u16(std::uint32_t a)const{return m.u16(a);}
    std::int16_t i16(std::uint32_t a)const{return m.i16(a);}
    std::uint8_t u8(std::uint32_t a)const{return m.u8(a);}
    void put(std::uint32_t a,std::uint32_t v)const{m.put32(a,v);}
    void putf(std::uint32_t a,float v)const{m.putf(a,v);}
    void putx(std::uint32_t a,X v)const{m.putf(a,st(v));}
    void put16(std::uint32_t a,std::uint16_t v)const{m.put16(a,v);}
    void put8(std::uint32_t a,std::uint8_t v)const{m.put8(a,v);}
    void copy(std::uint32_t to,std::uint32_t from,std::uint32_t dwords)const{  // forward REP MOVSD
        for(std::uint32_t k=0;k<dwords;++k)put(to+k*4,u(from+k*4));
    }
    void fill(std::uint32_t to,std::uint32_t v,std::uint32_t dwords)const{for(std::uint32_t k=0;k<dwords;++k)put(to+k*4,v);}
    std::uint32_t call(std::uint32_t pc_,std::initializer_list<std::uint32_t> args,std::uint32_t eax=0,std::uint32_t ecx=0)const{
        PcRaceCall k{};k.pc=pc_;k.eax=eax;k.ecx=ecx;k.argc=std::uint32_t(args.size());
        unsigned n=0;for(auto a:args)k.args[n++]=a;
        if(!c.service)throw std::logic_error("particles: no service for PC call");
        return c.service(k);
    }
    // 580F40 rand().
    std::uint32_t rand()const{pc.crt_random=pc.crt_random*0x343fdu+0x269ec3u;return (pc.crt_random>>16)&0x7fffu;}
    // 448810(id): the resource entry 7C2800 + id*0x48.
    static std::uint32_t resource(std::uint32_t id){return id*0x48u+0x7c2800u;}
    // [[r]+8] > n ? [[r+0x24]]+off : 0 (the texture handle pattern after 448810(0x57)).
    std::uint32_t texture(std::uint32_t id,std::uint32_t limit,std::uint32_t offset)const{
        const std::uint32_t r=resource(id);
        if(!(u(u(r)+8)>limit))return 0;
        return u(u(u(r+0x24))+offset);
    }
    // Matrix stack (89B564): the inline push / pop sequences of the module.
    void push_load(std::uint32_t a)const{driving::pc_matrix_push_load(c.matrices,m.bytes(a,64));}
    void pop()const{driving::pc_matrix_pop(c.matrices);}
    driving::Bytes current()const{return c.matrices.current();}
    // A record of the module memory as a typed reference (one lookup for the
    // whole record; the layouts below are may_alias over the PC bytes).
    template<class T> T& at(std::uint32_t a)const{return *reinterpret_cast<T*>(m.at(a,sizeof(T),true));}
    template<class T> T* array(std::uint32_t a,std::uint32_t count)const{
        return count?reinterpret_cast<T*>(m.at(a,std::size_t(count)*sizeof(T),true)):nullptr;}
};
// ---- nlParticle layouts (PC byte images kept in PcParticleState) ----
#define OR2_PC_RECORD struct __attribute__((may_alias))
// Source record (0x84): eleven at 8A8D18, their template at 74E8E0.
OR2_PC_RECORD ParticleSource {
    std::int32_t state;          // +00 bit 31: active, low bits: draw kind
    std::uint32_t slots;         // +04 particle records in the buffer
    std::uint32_t buffer;        // +08 PC address of the first record
    std::uint32_t born;          // +0C PC function run on a new particle
    std::int32_t born_limit;     // +10 a new particle needs generator <= limit (0x7FFF: always)
    float base[6];               // +14 new particle position / velocity ...
    float spread[6];             // +2C ... plus generator * spread
    float size;                  // +44 billboard size (spark: texture width)
    float point_size;            // +48 point sprite size (spark: texture height)
    std::uint32_t colour;        // +4C shared colour
    std::uint32_t texture;       // +50
    std::uint32_t texture_list;  // +54 animated point sprites: texture per frame
    std::int32_t texture_count;  // +58
    std::uint32_t plane;         // +5C road record (+04 point, +10 normal) the particles bounce on
    std::uint32_t free_list;     // +60 PC address of the free-slot stack (0: scan the buffer)
    std::int32_t free_count;     // +64
    std::uint32_t stride;        // +68 record size
    std::uint32_t live;          // +6C particles alive after the last control
    std::uint32_t src_blend;     // +70 D3DRS_SRCBLEND
    std::uint32_t dest_blend;    // +74 D3DRS_DESTBLEND
    std::uint32_t pixel_shader;  // +78 41B550 pixel state when set
    std::uint32_t z_enable;      // +7C
    std::uint32_t unknown80;
};
static_assert(sizeof(ParticleSource)==0x84);
inline constexpr std::uint32_t ParticleSources=0x8a8d18u,ParticleSourceCount=11;
// The sources by index: 1 gravel, 2 grass, 3 spark, 4 player tire smoke,
// 5 water, 6 misc, 7 backfire, 8..10 the 420020 / 420200 / 420470 effects
// (0 is the template copy, never set up).
enum ParticleSourceId : std::uint32_t { SourceGravel=1,SourceGrass=2,SourceSpark=3,SourceTireSmoke=4,SourceWater=5,
    SourceMisc=6,SourceBackfire=7,SourceEffect8=8,SourceEffect9=9,SourceEffect10=10 };
constexpr std::uint32_t source_address(std::uint32_t id){return ParticleSources+id*0x84u;}
inline ParticleSource& source(const P& p,std::uint32_t id){return p.at<ParticleSource>(source_address(id));}
// nlParticle generator (.data 74E980) and its globals.
OR2_PC_RECORD ParticleGenerator {
    std::uint32_t seed;          // +00
    std::uint32_t multiplier;    // +04
    std::uint32_t mask;          // +08
    float scale,offset;          // +0C
};
inline constexpr std::uint32_t ParticleGeneratorTable=0x74e980u;
inline constexpr std::uint32_t ParticleGeneratorValue=0x8a8ce8u;  // last generator step
inline constexpr std::uint32_t ParticleCurrentSource=0x95af50u;   // 418420: source of the next born
inline constexpr std::uint32_t ParticleRecordTemplate=0x95af54u;  // 8 zero dwords copied into new buffers
// Game globals the module reads.
inline constexpr std::uint32_t PlayerCar=0x799d18u;    // player car work
inline constexpr std::uint32_t RootMode=0x78026cu;     // root mode (0x13 attract, 0x16 race, 0x18 arcade ending ...)
inline constexpr std::uint32_t EventFlags=0x79fb48u;   // byte per event (+8: event 8)
inline constexpr std::uint32_t Camera=0x79f574u;       // camera work
// Header shared by every particle record (the effects append their fields).
OR2_PC_RECORD Particle {
    std::int32_t state;          // +00 bit 31: alive (0 once dead)
    float position[3];           // +04
    float velocity[3];           // +10
    std::uint32_t function;      // +1C PC function run every control (0: free slot)
};
static_assert(sizeof(Particle)==0x20);
// Point particles (0x50): sparks (source 3), gravel (1), grass (2).
// The fields 418CF0 draws them with (+20..+37 belong to the effect).
OR2_PC_RECORD PointParticle : Particle {
    std::uint32_t effect[6];
    std::uint32_t colour;        // +38
    float scale;                 // +3C screen size factor
    float uv_max[2];             // +40 texture cell corner of vertex 0
    float uv_min[2];             // +48 opposite corner
};
OR2_PC_RECORD SparkParticle : Particle {
    std::uint32_t unknown20;
    std::int32_t life;           // +24 frames left
    std::uint32_t unknown28[4];
    std::uint32_t colour;        // +38 the source colour at birth
    float scale;                 // +3C
    float uv_max[2],uv_min[2];   // +40 (left from the template)
};
OR2_PC_RECORD DebrisParticle : Particle {
    std::int32_t life;           // +20 frames left
    std::uint32_t unknown24[5];
    std::uint32_t colour;        // +38
    float scale;                 // +3C
    float uv_max[2];             // +40 texture cell (a quarter of the texture)
    float uv_min[2];             // +48
};
// Billboards (0x5C): tire smoke (source 4), water (5), misc (6), backfire (7).
OR2_PC_RECORD Billboard : Particle {
    float size;                  // +20
    std::uint32_t colour[4];     // +24 corner colours (alpha in the top byte)
    float uv_max[2];             // +34
    float uv_min[2];             // +3C
};
OR2_PC_RECORD SmokeParticle : Billboard {
    std::uint32_t life;          // +44 frames left
    std::uint32_t life_start;    // +48
    std::uint32_t unknown4c;
    std::uint32_t alpha;         // +50 alpha at birth (byte; float for 4209D0's)
    std::uint32_t unknown54[2];
};
OR2_PC_RECORD WaterParticle : Billboard {
    std::uint32_t unknown44;
    std::int32_t life;           // +48
    std::uint32_t unknown4c[4];
};
OR2_PC_RECORD MiscParticle : Billboard {
    std::uint32_t life;          // +44
    std::uint32_t life_start;    // +48
    float start_distance;        // +4C distance to the camera at birth
    float start[3];              // +50 position at birth
};
OR2_PC_RECORD BackfireParticle : Billboard {
    std::uint32_t side;          // +44 exhaust 0 / 1
    std::uint32_t flip;          // +48 mirrored (x negated)
    std::int32_t life;           // +4C
    std::uint32_t unknown50[3];
};
// Animated point sprites (0x28): sources 8..10.
OR2_PC_RECORD SpriteParticle : Particle {
    std::int32_t life;           // +20
    std::int16_t period;         // +24 frames per texture frame
    std::int16_t frame;          // +26 texture frame (0..5)
};
// Vertex buffers of the display (module .bss).
OR2_PC_RECORD ScreenVertex { float x,y,z,rhw; std::uint32_t colour; float u,v; };   // FVF 0x144
OR2_PC_RECORD WorldVertex { float x,y,z; std::uint32_t colour; float u,v; };        // FVF 0x142
OR2_PC_RECORD PointVertex { float x,y,z; std::uint32_t colour; };                   // FVF 0x42
static_assert(sizeof(ScreenVertex)==0x1c&&sizeof(WorldVertex)==0x18&&sizeof(PointVertex)==0x10&&sizeof(PointParticle)==0x50);
// Tire mark strip of a player tire (0x28, 9174D8 + tire*0x28): the last edge
// laid and the running texture coordinate.
OR2_PC_RECORD TireMark {
    std::uint32_t active;        // +00 a strip is open
    float length;                // +04
    float v;                     // +08 texture coordinate along the strip
    float left[3];               // +0C edge points of the last quad
    float right[3];              // +18
    std::uint32_t kind;          // +24 0 tarmac (state 0xC02), 1 (0x8004), 2 (0x18)
};
// Road plane of a source (+08 of the source record): a point, the normal and
// the surface kind of the polygon under the car.
OR2_PC_RECORD RoadPlane {
    std::uint32_t unknown0;
    float point[3];              // +04
    float normal[3];             // +10
    std::uint32_t unknown1c[14];
    std::uint32_t surface;       // +54 0 / 3 / 5: tarmac (smoke drag 74FE98), else 74FE9C
};
// The camera work ([79F574]) as the particle controls read it.
OR2_PC_RECORD CameraWork {
    std::uint8_t unknown0[0xd4];
    float position[3];           // +D4 distance reference
    std::uint32_t unknown_e0[6];
    float eye[3];                // +F8
    float look[3];               // +104 look-at point
};
// Tire record (0x5C): 934448 (player, 4), 9345B8 + (car-1)*0x170 (cars 1..23, 4 each).
// 41BD50 / 41C190 fill it each frame, the tire smoke requests read it.
OR2_PC_RECORD TireRecord {
    float moved;                 // +00 distance the contact moved this frame
    float pos[3];                // +04 contact point (world); 41BD50 then stores the tire's +3C in +08
    float normal[3];             // +10 41BD50: tire +70; 41C190: (0, 1, 0)
    float prev_pos[3];           // +1C contact point of the previous frame
    float velocity[3];           // +28 41BD50: tire +40; 41C190: car +D0
    std::uint32_t prev_08;       // +34 +08 of the previous frame
    float slip;                  // +38 41BD50: tire +E0; 41C190: 1 / 0 by the drift angle
    float grip;                  // +3C 41BD50: tire +E4; 41C190: 0 / 1
    std::uint32_t state;         // +40 41BD50: previous +3C; 41C190: 2 (traffic car)
    std::uint32_t prev_state;    // +44
    std::uint16_t surface;       // +48 41BD50: tire +EE
    std::uint16_t unknown4a;
    std::uint32_t lit;           // +4C 41BD50: [82EE60] bit of the tire
    float ground;                // +50 41BD50: ground direction term
    std::uint32_t kind;          // +54 smoke kind
    std::uint32_t colours;       // +58 colour table
};
static_assert(sizeof(SparkParticle)==0x50&&sizeof(DebrisParticle)==0x50&&sizeof(SmokeParticle)==0x5c&&
              sizeof(WaterParticle)==0x5c&&sizeof(MiscParticle)==0x5c&&sizeof(BackfireParticle)==0x5c&&sizeof(SpriteParticle)==0x28&&sizeof(TireRecord)==0x5c&&sizeof(RoadPlane)==0x58&&sizeof(TireMark)==0x28&&sizeof(CameraWork)==0x110);
}

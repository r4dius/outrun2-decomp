#pragma once
// Original PC arithmetic reconstructed from the hash-pinned user binary.
// These are bounded byte views, NOT complete recovered class declarations.
// Guest pointers are never cast to host pointers.
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <type_traits>
namespace outrun::driving {
class Bytes {
    std::uint8_t* p_; std::size_t n_;
public:
    Bytes(void* p, std::size_t n) : p_(static_cast<std::uint8_t*>(p)), n_(n) {
        if (!p && n) throw std::invalid_argument("null byte view");
    }
    std::size_t size() const { return n_; }
    std::uint8_t* data() const { return p_; }
    void check(std::size_t o, std::size_t s) const {
        if (o > n_ || s > n_ - o) out_of_view(o,s,n_);
    }
    // Throws std::out_of_range("PC field outside view +o/s of n"); kept out of line.
    [[noreturn]] static void out_of_view(std::size_t o,std::size_t s,std::size_t n);
    std::uint32_t u32(std::size_t o) const {
        check(o,4); return std::uint32_t(p_[o]) | (std::uint32_t(p_[o+1])<<8) |
             (std::uint32_t(p_[o+2])<<16) | (std::uint32_t(p_[o+3])<<24);
    }
    std::int32_t i32(std::size_t o) const { auto u=u32(o); std::int32_t v; std::memcpy(&v,&u,4); return v; }
    float f32(std::size_t o) const { auto u=u32(o); float f; std::memcpy(&f,&u,4); return f; }
    std::uint8_t u8(std::size_t o) const { check(o,1); return p_[o]; }
    std::int8_t i8(std::size_t o) const { auto u=u8(o); std::int8_t v; std::memcpy(&v,&u,1); return v; }
    std::int16_t i16(std::size_t o) const { check(o,2); auto v=std::uint16_t(p_[o] | (p_[o+1]<<8)); std::int16_t s; std::memcpy(&s,&v,2); return s; }
    void put32(std::size_t o,std::uint32_t v) const { check(o,4); for(unsigned k=0;k<4;++k)p_[o+k]=std::uint8_t(v>>(k*8)); }
    void puti(std::size_t o,std::int32_t v) const { put32(o,static_cast<std::uint32_t>(v)); }
    void putf(std::size_t o,float f) const { std::uint32_t u; std::memcpy(&u,&f,4); put32(o,u); }
    void put8(std::size_t o,std::uint8_t b) const {check(o,1);p_[o]=b;}
    void put16(std::size_t o,std::uint16_t v) const {check(o,2);p_[o]=std::uint8_t(v);p_[o+1]=std::uint8_t(v>>8);}
    Bytes sub(std::size_t o,std::size_t s) const {check(o,s);return Bytes(p_+o,s);}
};
static_assert(sizeof(float)==4 && sizeof(std::int32_t)==4, "32-bit IEEE float required");
constexpr std::size_t event_size=0x1000, work_size=0x800, parameter_size=0x2650;
constexpr std::array<std::size_t,4> embedded_wheel_offsets{0x258,0x34c,0x440,0x534};
struct Tables { // Supplied by an asset/binary data loader, never guest addresses.
    std::array<Bytes,2> torque;
    Bytes brake;
};
float engine_friction(Bytes event, Bytes parameters, const Tables&, float engine_speed);
void auto_clutch_control(Bytes event, Bytes work, Bytes parameters);
void induce_spin(Bytes event, Bytes work);
void tire_grip(Bytes event, Bytes work, Bytes parameters, const std::array<Bytes,4>& wheels);
float brake_pressure(std::int32_t pedal,const Tables&);
void engine_torque(Bytes event, Bytes parameters,const Tables&);
float predicted_engine_speed(Bytes event,Bytes work,Bytes parameters,std::uint32_t gear);
void accel_operation(Bytes event,Bytes work,Bytes parameters);
} // namespace outrun::driving

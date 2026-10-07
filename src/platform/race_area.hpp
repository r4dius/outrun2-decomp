#pragma once
// PC race course AREA manager (event 390, function 0):
//   init 44CB00, control 44F7C0 (= 44E590 area loader state machine,
//   44CF00 stage loader state machine, tail 44F190 event-object/ending
//   state), display 44F120 (44DE10 course geometry, 44E290 stage objects),
//   destroy 44B7F0.
// The port is a line-by-line transliteration over PC addresses: every global
// it reads or writes is addressed by its PC address in a PcRaceMemory whose
// regions the caller maps (the area block 7D2D80..7D34C8 and the area .data
// words 635F2C..636BC4 live in PcRaceAreaState; the course table/descriptor
// data, geometry files, car 799D18 fields, stage objects 79ECC8 etc. are
// mapped by their owner). Reading an unmapped address throws
// PcRaceUnmapped: nothing is ever invented for missing data.
// Every PC function outside the area module is reached through one service
// call (PcRaceCall: entry PC, EAX/ECX register arguments and the stack
// arguments in PC order; the service returns the original EAX). The render
// leaves (405360, 4052B0, 4052C0, 4044E0 ...) are appended to a draw list
// with the current matrix, like PcVehicleDrawCall, for the caller to execute
// on the renderer.
#include "driving/pc_matrix_stack.hpp"
#include "platform/vehicle_model_draw.hpp"
#include <array>
#include <cstdint>
#include <functional>
#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>
namespace outrun::platform {
struct PcRaceUnmapped : std::out_of_range {
    std::uint32_t address;
    PcRaceUnmapped(std::uint32_t a,std::size_t n);
};
// Byte regions addressed by 32-bit PC addresses. Regions are not owned.
class PcRaceMemory {
public:
    struct Region { std::uint32_t base; std::uint8_t* data; std::size_t size; bool writable; };
    void map(std::uint32_t base,std::uint8_t* data,std::size_t size){regions_.push_back({base,data,size,true});added();}
    void map_const(std::uint32_t base,const std::uint8_t* data,std::size_t size){regions_.push_back({base,const_cast<std::uint8_t*>(data),size,false});added();}
    void clear(){regions_.clear();dirty_=true;}
    std::size_t mark() const {return regions_.size();}
    void release(std::size_t mark){   // drops mappings made after mark
        if(mark>=regions_.size())return;
        regions_.resize(mark);if(mark<built_)dirty_=true;
    }
    bool mapped(std::uint32_t a,std::size_t n) const {return find(a,n)!=nullptr;}
    std::uint8_t* at(std::uint32_t a,std::size_t n,bool write=false) const;
    std::uint8_t u8(std::uint32_t a) const {return *at(a,1);}
    std::int8_t i8(std::uint32_t a) const {return std::int8_t(u8(a));}
    std::uint16_t u16(std::uint32_t a) const;
    std::int16_t i16(std::uint32_t a) const {return std::int16_t(u16(a));}
    std::uint32_t u32(std::uint32_t a) const;
    std::int32_t i32(std::uint32_t a) const {return std::int32_t(u32(a));}
    float f32(std::uint32_t a) const;
    void put8(std::uint32_t a,std::uint8_t v) const {*at(a,1,true)=v;}
    void put16(std::uint32_t a,std::uint16_t v) const;
    void put32(std::uint32_t a,std::uint32_t v) const;
    void putf(std::uint32_t a,float v) const;
    // Bounded writable view (matrix leaves).
    driving::Bytes bytes(std::uint32_t a,std::size_t n) const {return driving::Bytes(at(a,n,true),n);}
    const std::vector<Region>& regions() const {return regions_;}
private:
    const Region* find(std::uint32_t a,std::size_t n) const;
    std::vector<Region> regions_;
public:
    // Lookup table: the address space cut at every region edge, each piece
    // owned by the newest region covering it (later mappings shadow earlier
    // ones), sorted; rebuilt after a mapping change; last hit cached. An
    // access must lie inside one piece (an access straddling a shadowing
    // boundary is refused instead of reading the older region).
    struct Piece { std::uint64_t lo,hi; std::size_t region; };
private:
    mutable std::vector<Piece> scratch_;   // build() works here, then swaps into its cache entry
    mutable std::size_t table_{};          // cache_ entry holding the current table
    mutable bool dirty_{true};
    mutable std::size_t last_{};
    // Regions [built_, size) were mapped after the last build (temporary
    // locals of a call, released again by its mark): looked up first, newest
    // first, instead of rebuilding the pieces for each of them.
    mutable std::size_t built_{};
    static constexpr std::size_t MaxOverlays=8;
    // Address span of the overlays mapped since the last build (an access
    // outside it skips their scan; kept as a superset after release()).
    mutable std::uint64_t overlay_lo_{~0ull},overlay_hi_{};
    mutable std::size_t last2_{};          // the piece hit before last_
    void added(){
        if(dirty_)return;
        const auto& r=regions_.back();
        if(r.size){overlay_lo_=std::min<std::uint64_t>(overlay_lo_,r.base);overlay_hi_=std::max<std::uint64_t>(overlay_hi_,std::uint64_t(r.base)+r.size);}
        if(regions_.size()-built_>MaxOverlays)dirty_=true;
    }
    void build() const;
    // The last two built tables with the region lists they were built from:
    // owners that clear and map the same list for every call (the traffic /
    // course object runs, ~160 regions) reuse the table instead of rebuilding
    // it (an O(regions^2) build per call cost ~0.2 ms each on the console).
    struct Built { std::vector<Region> regions; std::vector<Piece> pieces; };
    mutable std::array<Built,4> cache_{};
    mutable std::size_t cache_next_{};
};
// One call of a PC function outside the ported module (or a render leaf).
struct PcRaceCall {
    std::uint32_t pc{};
    std::uint32_t eax{},ecx{};            // register arguments (0 when unused)
    std::uint32_t argc{};
    std::array<std::uint32_t,12> args{};  // stack arguments, first = [esp+4]
};
using PcRaceService=std::function<std::uint32_t(const PcRaceCall&)>;
// Symbolic addresses of PC stack locals passed to services by pointer (the
// service may write them through PcRaceMemory: the port maps them while the
// call runs).
constexpr std::uint32_t PcRaceLocal44E590=0x7fff0000u;   // 44E590 [esp+8] (46C380/4F03A0)
constexpr std::uint32_t PcRaceLocal44C310A=0x7fff0010u;  // 44C310 locals passed to 44FC60
constexpr std::uint32_t PcRaceLocal44C310B=0x7fff0014u;
// Pseudo PC of the Direct3D SetRenderState(state, value) calls in the draw
// list (IDirect3DDevice9 vtable +0xE4 through the device 89BD60).
constexpr std::uint32_t PcRaceSetRenderState=0x000000e4u;
struct PcRaceContext {
    PcRaceMemory& m;
    driving::PcMatrixStack& matrices;
    PcRaceService service;
    std::vector<PcVehicleDrawCall>* draws{}; // render leaves (display only)
};
// Area module state: .bss block 7D2D80..7D34C8 and the module's .data words
// 635F2C..636BC4 (EXE initial image: 635F2C lookup cache, 636048 matrices,
// 636078.. event instance arrays, 6366B8 ending table, 63685C stage table,
// 6368C4 debug ending, 6369C0 event records, 636BB8 counter, 636BBC/636BC0).
struct PcRaceAreaState {
    static constexpr std::uint32_t BlockBase=0x7d2d80u,BlockEnd=0x7d34c8u;
    static constexpr std::uint32_t DataBase=0x635f2cu,DataEnd=0x636bc4u;
    std::vector<std::uint8_t> block=std::vector<std::uint8_t>(BlockEnd-BlockBase);
    std::vector<std::uint8_t> data;
    PcRaceAreaState(); // data = EXE image
    void map(PcRaceMemory& m){m.map(BlockBase,block.data(),block.size());m.map(DataBase,data.data(),data.size());}
};
// Read-only EXE tables the module reads (5D4DC8..5D5E58: 5D4DC8 course ->
// object ID table read by the protected bridge at 44DFBE, 5D4FD8 16-byte
// kind records, 5D5408 0x14-byte course records).
void race_area_map_tables(PcRaceMemory& m);
extern std::uint8_t RaceAreaDataImage[0xc98];   // EXE .data 635F2C..636BC4
// Event callbacks. work is unused by the area callbacks (the PC ignores it).
void race_area_init_44cb00(PcRaceContext&);
void race_area_control_44f7c0(PcRaceContext&,std::uint32_t entry_ecx);
void race_area_display_44f120(PcRaceContext&);
// The car reflection cube's face scene (414050), as draw leaves like 44F120:
// 44CD30 the course environment model (the section model of [7D3188]+14: on a route
// (car +5C) when [7D2E88] == 0x14 (bridge 44CD63, measured) the [7D2E60] objects 0 / 1 / 2 by
// the car's +64 position and the 451350 side; else the section node lists of [7D3070] for
// the [desc+60] objects, then 5D4ED0[[desc]]), between 4044E0(6) / 4044E0(7).
void race_env_model_44cd30(PcRaceContext&);
// 451C20(sky work [79F6DC]): the two layers' environment sky objects (+C, alpha +40 > [6281F0],
// matrix +50) on the inverse view 95DBA0 (411220), then 404540.
void race_env_sky_451c20(PcRaceContext&,std::uint32_t work);
void race_area_destroy_44b7f0(PcRaceContext&);
// Parts, exposed for the probes and for other modules.
void race_area_control_44e590(PcRaceContext&,std::uint32_t entry_ecx);
std::uint32_t race_area_stage_44cf00(PcRaceContext&);
void race_area_objects_44f190(PcRaceContext&);
void race_area_geometry_44de10(PcRaceContext&,std::uint32_t car,std::uint32_t desc);
void race_area_stage_draw_44e290(PcRaceContext&,std::uint32_t car,std::uint32_t desc);
std::uint32_t race_area_ending_44d410(PcRaceContext&);
void race_area_stats_44ede0(PcRaceContext&);
std::uint32_t race_area_geometry_load_44c310(PcRaceContext&,std::uint32_t token,std::uint32_t lane);
std::uint32_t race_area_fade_44c530(PcRaceContext&);
std::uint32_t race_area_record_44c8d0(const PcRaceMemory&,std::uint32_t id);
std::uint32_t race_area_value_44c940(PcRaceMemory&,std::uint32_t id);
void race_area_sky_ids_44c420(PcRaceContext&,std::uint32_t out,std::uint32_t second);
std::uint32_t race_area_sky_time_44c610(const PcRaceMemory&);
std::uint32_t race_area_sky_steps_44c640(const PcRaceMemory&);
}

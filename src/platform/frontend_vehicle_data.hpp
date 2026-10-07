#pragma once
#include "system/exe_image.hpp"
#include "frontend_profiles.hpp"
#include <array>
#include <cstdint>
namespace outrun::platform {
// PC 46BBE0: first dword of each eight-byte record at 5B2FE0.
// This is model-ID order, NOT the order of the menu at 68EA10.
inline constexpr std::array<unsigned,30> VehicleResourceIds{{
    1,2,3,0xa,5,4,7,8,9,0xb,6,0x14a,0x14b,0x14c,0x14d,
    0x14e,0x14f,0x150,0x157,0x152,0x151,0x154,0x155,0x156,0x158,
    0x153,0x159,0x15a,0x15b,0x15c}};
// Resolved through the PC resource filename table 633558 (448850/448932).
inline constexpr std::array<const char*,30> VehicleResourcePaths{{
    "CARS/obj_plcar_f50_pmt.sz","CARS/obj_plcar_dino_pmt.sz",
    "CARS/obj_plcar_gto_pmt.sz","CARS/obj_plcar_512bb_pmt.sz",
    "CARS/obj_plcar_dayts_pmt.sz","CARS/obj_plcar_fx_pmt.sz",
    "CARS/obj_plcar_testa_pmt.sz","CARS/obj_plcar_360sp_pmt.sz",
    "CARS/obj_plcar_f40_pmt.sz","CARS/obj_plcar_250gto_pmt.sz",
    "CARS/obj_plcar_f355sp_pmt.sz","CARS/obj_plcar_328gts_pmt.sz",
    "CARS/obj_plcar_f430_pmt.sz","CARS/obj_plcar_550b_pmt.sz",
    "CARS/obj_plcar_575sa_pmt.sz","CARS/obj_plcar_f50_t_pmt.sz",
    "CARS/obj_plcar_dino_t_pmt.sz","CARS/obj_plcar_gto_t_pmt.sz",
    "CARS/obj_plcar_512bb_t_pmt.sz","CARS/obj_plcar_dayts_t_pmt.sz",
    "CARS/obj_plcar_fx_t_pmt.sz","CARS/obj_plcar_testa_t_pmt.sz",
    "CARS/obj_plcar_360m_t_pmt.sz","CARS/obj_plcar_f40_t_pmt.sz",
    "CARS/obj_plcar_250gto_t_pmt.sz","CARS/obj_plcar_f355sp_t_pmt.sz",
    "CARS/obj_plcar_328gts_t_pmt.sz","CARS/obj_plcar_f430sp_t_pmt.sz",
    "CARS/obj_plcar_550b_t_pmt.sz","CARS/obj_plcar_575sa_t_pmt.sz"}};
inline constexpr std::array<unsigned,30> VehicleMenuModels{{
    1,4,0,7,10,14,6,2,11,9,3,8,13,12,5,
    16,19,15,22,25,29,21,17,26,24,18,23,28,27,20}};
inline constexpr std::array<std::int16_t,30> VehicleMenuUnlocks{{
    0,0,0,1,4,12,3,2,5,6,7,8,11,10,9,13,14,15,16,19,27,18,17,20,21,22,23,26,25,24}};
inline constexpr std::array<std::int8_t,30> VehicleDefaultColours{{
    1,2,0,4,4,0,0,0,1,1,0,0,0,0,0,1,2,0,4,4,0,0,0,1,1,0,0,0,0,0}};
alignas(16) inline std::int16_t VehicleColourUnlocks[30][8]{}; OR2_EXE_COPY(VehicleColourUnlocks,0x68EAC8u,0x1E0u);
inline bool vehicle_resource_46bbe0(unsigned model,unsigned& resource){
    if(model>=VehicleResourceIds.size())return false;
    resource=VehicleResourceIds[model];return true;
}
inline bool vehicle_unlocked_4c8f90(const PcLicense& profile,unsigned menu_index){
    if(menu_index>=VehicleMenuUnlocks.size())return false;
    if(menu_index<2)return true;
    const auto bit=unsigned(VehicleMenuUnlocks[menu_index]);
    return (profile[0x28+bit/8]&(1u<<(bit%8)))!=0;
}
inline bool vehicle_colour_unlocked_4c8fc0(const PcLicense& profile,unsigned menu_index,unsigned colour){
    if(menu_index>=30||colour>=8)return false;
    const auto bit=VehicleColourUnlocks[menu_index][colour];
    if(bit<0)return bit==-1;
    return (profile[0x28+unsigned(bit)/8]&(1u<<(unsigned(bit)%8)))!=0;
}
}

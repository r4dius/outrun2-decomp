// EXE .data 74E8E0..750080: initial image of the PART_EFC module .data
// (particle source template, random tables, effect parameters). Checked
// against the pinned EXE by the particles oracle.
#include "system/exe_image.hpp"
#include "platform/race_particles.hpp"
namespace outrun::platform {
alignas(16) std::uint8_t RaceParticleDataImage[0x17a0]{}; OR2_EXE_COPY(RaceParticleDataImage,0x74E8E0u,0x17A0u);
alignas(16) std::uint8_t RaceTireMarkDataImage[0x3c]{}; OR2_EXE_COPY(RaceTireMarkDataImage,0x64FE1Cu,0x3Cu);
PcParticleState::PcParticleState():data(RaceParticleDataImage,RaceParticleDataImage+sizeof(RaceParticleDataImage)),
    tire_data(RaceTireMarkDataImage,RaceTireMarkDataImage+sizeof(RaceTireMarkDataImage)){}
// EXE .rdata 622528..623C00: glow colour tables (0x42 x 0x44), the ground
// diagonals 623894 and the exhaust offsets 6238C8 read by the module.
alignas(16) std::uint8_t RaceParticleRdataImage[0x16d8]{}; OR2_EXE_COPY(RaceParticleRdataImage,0x622528u,0x16D8u);
void particles_map_tables(PcRaceMemory& m){m.map_const(0x622528u,RaceParticleRdataImage,sizeof(RaceParticleRdataImage));}
}

#pragma once
#include <array>
namespace outrun::platform::mission_tables {
// Unmodified PC tables 5CE4D8..5CE718 and 69E2F4..69E878.
struct Descriptor {unsigned token,first,last;};
struct Point {float x,y;};
inline constexpr unsigned counts[]{5,9,11,15},offsets[]{0,5,14,25};
inline constexpr Descriptor backgrounds[]{{0x4400ac,0,0},{0x4400b0,0,0},{0x4400a8,0,0},{0x4400aa,0,0}};
inline constexpr Descriptor overlays[]{{0x4400ad,0,0},{0x4400b1,0,0},{0x4400a9,0,0},{0x4400ab,0,0}};
inline constexpr Descriptor types[]{{0x4400b9,0,0},{0x4400b9,3,3},{0x4400b9,2,2},{0x4400b9,5,5},{0x4400b9,6,6},{0x4400b9,4,4},{0x4400b9,4,4}};
inline constexpr unsigned labels[]{0x34f,0x350,0x351,0x352,0x34f,0x353,0x353};
inline constexpr Descriptor buttons[]{{0x4400bb,1,1},{0x4400bb,0,0},{0x4400bb,2,2},{0x4400bb,4,4}};
inline constexpr Descriptor footers[]{{0x4400ca,4,4},{0x4400ca,5,5},{0x4400ca,6,6},{0x4400ca,7,7}};
inline constexpr Descriptor nodes[]{{0x4400ae,2,2},{0x4400ae,1,1},{0x4400ae,3,3},{0x4400ae,4,4}};
inline constexpr Descriptor panels[]{{0x4400b8,1,1},{0x4400b8,0,0},{0x4400b8,2,2},{0x4400b8,4,4}};
inline constexpr Descriptor selected[]{{0x4400ba,0,29},{0x4400ba,30,59},{0x4400ba,60,89},{0x4400ba,90,119},{0x4400ba,120,149}};
// PC indexes one element beyond the seven grade descriptors for grade7.
// The adjacent count table supplies {5,9,11}; bank0 is rejected by 428460,
// returning FFFFFFFF (no sprite). Preserve this, rather than drawing a grade.
inline constexpr Descriptor grades[]{{0x4400af,6,6},{0x4400af,5,5},{0x4400af,4,4},{0x4400af,3,3},{0x4400af,2,2},{0x4400af,1,1},{0x4400af,0,0},{5,9,11}};
inline constexpr Point race_positions[]{{84,342},{128,342},{172,342},{216,342},{260,342}};
inline constexpr Point grade_positions[]{{78,358},{122,358},{166,358},{210,358},{254,358}};
inline constexpr Point highlights[4][15]{
 {{105,226},{162,226},{238,188},{308,188},{353,159}},
 {{74,213},{142,213},{214,213},{213,181},{214,249},{281,181},{281,249},{348,181},{405,181}},
 {{76,220},{138,220},{182,177},{251,177},{338,177},{193,220},{251,220},{338,220},{252,279},{338,254},{338,146}},
 {{76,264},{135,263},{135,216},{190,180},{190,155},{253,155},{368,198},{190,263},{190,235},{190,207},{252,231},{321,230},{252,264},{368,165},{429,165}}};
inline constexpr Point positions[4][15]{
 {{123,225},{178,225},{256,186},{324,186},{372,158}},
 {{90,212},{158,212},{230,212},{230,180},{230,247},{297,180},{297,247},{363,180},{421,180}},
 {{93,218},{155,218},{199,175},{269,175},{353,175},{210,218},{268,218},{355,218},{266,277},{355,251},{355,145}},
 {{92,262},{150,262},{151,214},{208,178},{208,154},{268,154},{384,196},{205,262},{206,234},{208,206},{267,228},{337,228},{268,262},{384,164},{445,164}}};
inline constexpr int neighbors[4][15][4]{
 {{-1,-1,1,-1},{-1,-1,2,0},{-1,-1,3,1},{4,-1,4,2},{-1,3,-1,3}},
 {{-1,-1,1,-1},{-1,-1,2,0},{3,4,-1,1},{-1,2,5,-1},{2,-1,6,-1},{-1,-1,7,3},{7,-1,7,4},{-1,6,8,5},{-1,-1,-1,7}},
 {{-1,-1,1,-1},{2,-1,5,0},{-1,1,3,1},{-1,6,4,2},{10,7,-1,3},{-1,8,6,1},{3,-1,7,5},{4,9,-1,6},{5,-1,9,5},{7,8,-1,8},{-1,4,-1,-1}},
 {{-1,-1,1,-1},{2,-1,7,0},{3,1,3,-1},{4,2,-1,2},{-1,3,5,-1},{-1,10,6,4},{13,11,-1,5},{8,-1,12,1},{9,7,-1,-1},{-1,8,10,-1},{5,-1,11,9},{6,12,6,10},{11,-1,11,7},{-1,6,14,-1},{-1,-1,-1,13}}};
}

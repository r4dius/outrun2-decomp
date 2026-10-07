// The particles' pixel shader 623830 (41B550, the 419DC0 smoke quads) for the
// DrawPrimitiveUP path: the same arithmetic as pc_spec_ps11_154_fsh.glsl (blob 54,
// alpha test and fog after it) without the uber program scaffolding.
// Particle mode 3 (measurement only, not the game's look): no texture, no maths,
// the floor of the fill cost.
#version 460
layout(std140,binding=1) uniform PsProgram{ivec4 program[4];ivec4 info;ivec4 stage_kind[2];vec4 bump[8];vec4 lum[2];};
layout(std140,binding=2) uniform PsFixed{vec4 fog_colour;vec4 fog_params;vec4 alpha_test;};
layout(location=0) in vec4 in_d0;
layout(location=2) in vec4 in_t0;
layout(location=10) in float in_fog;
layout(location=0) out vec4 out_colour;
layout(binding=0) uniform sampler2D tex2d0;
layout(binding=6) uniform samplerCube texcube0;
void main(){
    out_colour=in_d0*0.25;
}

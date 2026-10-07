// The particles' pixel shader 623830 (41B550, the 419DC0 smoke quads), particle
// mode 4: drawn into a half-resolution target (pc_particle_comp_* composites it)
// with the hardware depth test against the half-resolution depth of
// pc_particle_depth_fsh. Same arithmetic as pc_spec_ps11_154_fsh.glsl (blob 54);
// draw_particles_half only uses it when the alpha test cannot reject anything
// (419DC0 draws with ALPHAFUNC GREATEREQUAL, ALPHAREF 0) and no depth is written,
// so the depth test may run before the shader.
#version 460
layout(early_fragment_tests) in;
layout(std140,binding=1) uniform PsProgram{ivec4 program[4];ivec4 info;ivec4 stage_kind[2];vec4 bump[8];vec4 lum[2];};
layout(std140,binding=2) uniform PsFixed{vec4 fog_colour;vec4 fog_params;vec4 alpha_test;};
layout(location=0) in vec4 in_d0;
layout(location=2) in vec4 in_t0;
layout(location=10) in float in_fog;
layout(location=0) out vec4 out_colour;
layout(binding=0) uniform sampler2D tex2d0;
layout(binding=6) uniform samplerCube texcube0;
void main(){
    vec4 T0=stage_kind[0][0]==1?texture(texcube0,in_t0.xyz):texture(tex2d0,in_t0.xy);
    vec4 R0=clamp(T0*in_d0,0.0,1.0);
    vec4 t1=(vec4(1.0)-R0.wwww)*(vec4(1.0)-R0.wwww);
    R0.w=clamp(t1,0.0,1.0).w;
    R0.w=(vec4(1.0)-R0.wwww).w;
    R0.xyz=(R0*R0.wwww).xyz;
    vec4 c=R0;
    if(fog_params.x!=0.0){float fz=gl_FragCoord.z;float fw=1.0/gl_FragCoord.w;float d=fog_params.w!=0.0?fw:fz;float f;
        int mode=int(fog_params.x);
        if(mode==3)f=(fog_colour.w-d)/max(fog_colour.w-fog_params.y,1e-20);
        else if(mode==1)f=exp(-fog_params.z*d);
        else if(mode==2)f=exp(-(fog_params.z*d)*(fog_params.z*d));
        else f=in_fog;
        f=clamp(f,0.0,1.0);c.rgb=mix(fog_colour.rgb,c.rgb,f);}
    out_colour=c;
}

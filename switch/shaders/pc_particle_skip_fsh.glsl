// The particles' pixel shader 623830 (41B550, the 419DC0 smoke quads) for the
// DrawPrimitiveUP path: the same arithmetic as pc_spec_ps11_154_fsh.glsl (blob 54,
// alpha test and fog after it) without the uber program scaffolding.
// Particle mode 2: with blending ONE / INVSRCALPHA (checked by the device), a zero
// colour leaves the target unchanged, so it is discarded (no blend read/write).
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
    vec4 T0=stage_kind[0][0]==1?texture(texcube0,in_t0.xyz):texture(tex2d0,in_t0.xy);
    vec4 R0=clamp(T0*in_d0,0.0,1.0);
    vec4 t1=(vec4(1.0)-R0.wwww)*(vec4(1.0)-R0.wwww);
    R0.w=clamp(t1,0.0,1.0).w;
    R0.w=(vec4(1.0)-R0.wwww).w;
    R0.xyz=(R0*R0.wwww).xyz;
    vec4 c=R0;
    if(alpha_test.x!=0.0&&alpha_test.w!=1.0){float a=floor(clamp(c.a,0.0,1.0)*255.0+0.5);float ref=alpha_test.y;int f=int(alpha_test.z);
        bool pass=f==8||(f==2&&a<ref)||(f==3&&a==ref)||(f==4&&a<=ref)||(f==5&&a>ref)||(f==6&&a!=ref)||(f==7&&a>=ref);
        if(!pass)discard;}
    if(fog_params.x!=0.0){float fz=gl_FragCoord.z;float fw=1.0/gl_FragCoord.w;float d=fog_params.w!=0.0?fw:fz;float f;
        int mode=int(fog_params.x);
        if(mode==3)f=(fog_colour.w-d)/max(fog_colour.w-fog_params.y,1e-20);
        else if(mode==1)f=exp(-fog_params.z*d);
        else if(mode==2)f=exp(-(fog_params.z*d)*(fog_params.z*d));
        else f=in_fog;
        f=clamp(f,0.0,1.0);c.rgb=mix(fog_colour.rgb,c.rgb,f);}
    if(c==vec4(0.0))discard;
    out_colour=c;
}

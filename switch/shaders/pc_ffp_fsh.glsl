#version 460
// Direct3D 9 fixed-function texture stage cascade (SetPixelShader(NULL)),
// the same evaluation as PcSoftD3D9Device::fixed_function: per stage
// COLOROP/ARG0..2, ALPHAOP/ARG0..2, RESULTARG, TEXCOORDINDEX; a stage whose
// COLOROP is DISABLE ends the cascade; D3DTA_TEXTURE of a stage without a
// texture reads opaque white. Four stages, sampled from bindings 0..3.
// Then the alpha test and fog of the ubershaders' epilogue.
layout(location=0) in vec4 in_d0;
layout(location=1) in vec4 in_d1;
layout(location=2) in vec4 in_t0;
layout(location=3) in vec4 in_t1;
layout(location=4) in vec4 in_t2;
layout(location=5) in vec4 in_t3;
layout(location=6) in float in_fog;
layout(location=0) out vec4 out_colour;
// op[s]: colorop, colorarg1, colorarg2, alphaop; arg[s]: alphaarg1,
// alphaarg2, colorarg0, alphaarg0; misc[s]: resultarg, texcoordindex,
// has 2D texture, has cube texture; konst[s]: D3DTSS_CONSTANT. tfactor, flags.x = specular enable.
layout(std140,binding=1) uniform FfpStages{ivec4 op[4];ivec4 arg[4];ivec4 misc[4];vec4 konst[4];vec4 tfactor;ivec4 flags;};
layout(std140,binding=2) uniform PsFixed{vec4 fog_colour;vec4 fog_params;vec4 alpha_test;};
layout(binding=0) uniform sampler2D tex0;
layout(binding=1) uniform sampler2D tex1;
layout(binding=2) uniform sampler2D tex2;
layout(binding=3) uniform sampler2D tex3;
// Cube textures of stages 0..3 (D3D9 samples them with the stage's texture
// coordinate as a direction: the environment maps of the car bodies).
layout(binding=6) uniform samplerCube cube0;
layout(binding=7) uniform samplerCube cube1;
layout(binding=8) uniform samplerCube cube2;
layout(binding=9) uniform samplerCube cube3;
vec4 T[4];
vec4 sample_cube(int s){
    const vec3 c=T[misc[s].y&3].xyz;
    if(s==0)return texture(cube0,c);
    if(s==1)return texture(cube1,c);
    if(s==2)return texture(cube2,c);
    return texture(cube3,c);
}
vec4 sample_stage(int s){
    const vec2 c=T[misc[s].y&3].xy;
    if(s==0)return texture(tex0,c);
    if(s==1)return texture(tex1,c);
    if(s==2)return texture(tex2,c);
    return texture(tex3,c);
}
vec4 current,temp,texel,diffuse,specular;
vec4 arg_value(int a,int s){
    vec4 r;
    switch(a&15){
    case 0:r=diffuse;break;case 1:r=current;break;case 2:r=texel;break;case 3:r=tfactor;break;
    case 4:r=specular;break;case 5:r=temp;break;case 6:r=konst[s];break;default:r=current;break;}
    if((a&16)!=0)r=vec4(1.0)-r;
    if((a&32)!=0)r=vec4(r.w);
    return r;
}
// Arguments are evaluated once (they have no side effects).
vec4 op_value(int o,vec4 a0,vec4 p,vec4 q){
    switch(o){
    case 3:return q;                                            // SELECTARG2
    case 4:return p*q;                                          // MODULATE
    case 5:return p*q*2.0;                                      // MODULATE2X
    case 6:return p*q*4.0;                                      // MODULATE4X
    case 7:return p+q;                                          // ADD
    case 8:return p+q-vec4(0.5);                                // ADDSIGNED
    case 9:return (p+q-vec4(0.5))*2.0;                          // ADDSIGNED2X
    case 10:return p-q;                                         // SUBTRACT
    case 11:return p+q-p*q;                                     // ADDSMOOTH
    case 12:return mix(q,p,diffuse.w);                          // BLENDDIFFUSEALPHA
    case 13:return mix(q,p,texel.w);                            // BLENDTEXTUREALPHA
    case 14:return mix(q,p,tfactor.w);                          // BLENDFACTORALPHA
    case 15:return p+q*(1.0-texel.w);                           // BLENDTEXTUREALPHAPM
    case 16:return mix(q,p,current.w);                          // BLENDCURRENTALPHA
    case 18:return p+vec4(p.w)*q;                               // MODULATEALPHA_ADDCOLOR
    case 19:return p*q+vec4(p.w);                               // MODULATECOLOR_ADDALPHA
    case 20:return p+vec4(1.0-p.w)*q;                           // MODULATEINVALPHA_ADDCOLOR
    case 21:return (vec4(1.0)-p)*q+vec4(p.w);                   // MODULATEINVCOLOR_ADDALPHA
    case 24:return vec4(4.0*dot(p.xyz-vec3(0.5),q.xyz-vec3(0.5))); // DOTPRODUCT3
    case 25:return a0+p*q;                                      // MULTIPLYADD
    case 26:return a0*p+(vec4(1.0)-a0)*q;                       // LERP
    default:return p;                                           // SELECTARG1, unmodelled ops
    }
}
void main(){
    T[0]=in_t0;T[1]=in_t1;T[2]=in_t2;T[3]=in_t3;
    diffuse=in_d0;specular=in_d1;current=diffuse;temp=vec4(0.0);
    for(int s=0;s<4;++s){
        const int cop=op[s].x,aop=op[s].w;
        if(cop<=1)break;
        texel=misc[s].z!=0?sample_stage(s):misc[s].w!=0?sample_cube(s):vec4(1.0);
        vec4 r=clamp(op_value(cop,arg_value(arg[s].z,s),arg_value(op[s].y,s),arg_value(op[s].z,s)),0.0,1.0);
        if(aop<=1)r.w=current.w;                                  // alpha DISABLE: pass current alpha
        else if(cop==24)r.w=r.x;                                  // DOTPRODUCT3 replicates into alpha
        else r.w=clamp(op_value(aop,arg_value(arg[s].w,s),arg_value(arg[s].x,s),arg_value(arg[s].y,s)).w,0.0,1.0);
        if(misc[s].x==5)temp=r;else current=r;
    }
    if(flags.x!=0)current.rgb+=specular.rgb;
    vec4 c=clamp(current,0.0,1.0);
    if(alpha_test.x!=0.0){float a=floor(c.a*255.0+0.5);float ref=alpha_test.y;int f=int(alpha_test.z);
        bool pass=f==8||(f==2&&a<ref)||(f==3&&a==ref)||(f==4&&a<=ref)||(f==5&&a>ref)||(f==6&&a!=ref)||(f==7&&a>=ref);
        if(!pass)discard;}
    if(fog_params.x!=0.0){float fz=gl_FragCoord.z;float fw=1.0/gl_FragCoord.w;float d=fog_params.w!=0.0?fw:fz;float f;
        int mode=int(fog_params.x);
        if(mode==3)f=(fog_colour.w-d)/max(fog_colour.w-fog_params.y,1e-20);
        else if(mode==1)f=exp(-fog_params.z*d);
        else if(mode==2)f=exp(-(fog_params.z*d)*(fog_params.z*d));
        else f=in_fog;
        f=clamp(f,0.0,1.0);c.rgb=mix(fog_colour.rgb,c.rgb,f);}
    out_colour=c;
}

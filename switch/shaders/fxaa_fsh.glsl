#version 460
// Port post-process (not original code): FXAA on the 3D scene while it is
// copied (and upscaled) from the offscreen image to the frame, before the 2D
// layer. dst = frame rectangle, src = scene rectangle (pixels, x y w h).
layout(location=0) out vec4 outColor;
layout(binding=0) uniform sampler2D scene;
layout(std140,binding=0) uniform Params { vec4 dst; vec4 src; vec4 texel; };   // texel.xy = 1 / scene image size

vec3 at(vec2 q){
    q=clamp(q,src.xy+0.5,src.xy+src.zw-0.5);
    return textureLod(scene,q*texel.xy,0.0).rgb;
}
float luma(vec3 c){return dot(c,vec3(0.299,0.587,0.114));}

void main(){
    const vec2 q=src.xy+(gl_FragCoord.xy-dst.xy)*(src.zw/dst.zw);   // scene pixel
    const vec3 cM=at(q);
    const float lNW=luma(at(q+vec2(-1.0,-1.0))),lNE=luma(at(q+vec2(1.0,-1.0)));
    const float lSW=luma(at(q+vec2(-1.0,1.0))),lSE=luma(at(q+vec2(1.0,1.0)));
    const float lM=luma(cM);
    const float lMin=min(lM,min(min(lNW,lNE),min(lSW,lSE)));
    const float lMax=max(lM,max(max(lNW,lNE),max(lSW,lSE)));
    vec2 dir=vec2(-((lNW+lNE)-(lSW+lSE)),(lNW+lSW)-(lNE+lSE));
    const float reduce=max((lNW+lNE+lSW+lSE)*(0.25/8.0),1.0/128.0);
    const float rcpMin=1.0/(min(abs(dir.x),abs(dir.y))+reduce);
    dir=clamp(dir*rcpMin,vec2(-8.0),vec2(8.0));
    const vec3 a=0.5*(at(q+dir*(1.0/3.0-0.5))+at(q+dir*(2.0/3.0-0.5)));
    const vec3 b=a*0.5+0.25*(at(q+dir*-0.5)+at(q+dir*0.5));
    const float lB=luma(b);
    const vec3 aa=(lB<lMin||lB>lMax)?a:b;
    // low contrast: the centre pixel unchanged (no early return)
    outColor=vec4((lMax-lMin<max(0.0312,lMax*0.125))?cM:aa,1.0);
}

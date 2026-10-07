#pragma once
// Port post-process (not original code): switch/shaders/fxaa_fsh.glsl in HLSL
// (shader model 4.1), over the whole back buffer before the 2D layer
// (full-screen triangle, no vertex buffer). Options > Settings ANTI-ALIASING.
namespace outrun::xbox_runtime::hlsl {
inline constexpr const char* fxaa=R"(
Texture2D scene:register(t0);
SamplerState linear_clamp:register(s0);
float4 vs_main(uint id:SV_VertexID):SV_Position{float2 p=float2(float((id<<1)&2),float(id&2));return float4(p*2.0-1.0,0.0,1.0);}
float4 at(float2 q,float2 size){q=clamp(q,0.5,size-0.5);return scene.SampleLevel(linear_clamp,q/size,0);}
float luma(float3 c){return dot(c,float3(0.299,0.587,0.114));}
float4 ps_main(float4 position:SV_Position):SV_Target{
    float w,h;scene.GetDimensions(w,h);const float2 size=float2(w,h);
    const float2 q=position.xy;
    const float4 cM=at(q,size);
    const float lNW=luma(at(q+float2(-1.0,-1.0),size).rgb),lNE=luma(at(q+float2(1.0,-1.0),size).rgb);
    const float lSW=luma(at(q+float2(-1.0,1.0),size).rgb),lSE=luma(at(q+float2(1.0,1.0),size).rgb);
    const float lM=luma(cM.rgb);
    const float lMin=min(lM,min(min(lNW,lNE),min(lSW,lSE)));
    const float lMax=max(lM,max(max(lNW,lNE),max(lSW,lSE)));
    float2 dir=float2(-((lNW+lNE)-(lSW+lSE)),(lNW+lSW)-(lNE+lSE));
    const float reduce=max((lNW+lNE+lSW+lSE)*(0.25/8.0),1.0/128.0);
    const float rcpMin=1.0/(min(abs(dir.x),abs(dir.y))+reduce);
    dir=clamp(dir*rcpMin,-8.0,8.0);
    const float3 a=0.5*(at(q+dir*(1.0/3.0-0.5),size).rgb+at(q+dir*(2.0/3.0-0.5),size).rgb);
    const float3 b=a*0.5+0.25*(at(q+dir*-0.5,size).rgb+at(q+dir*0.5,size).rgb);
    const float lB=luma(b);
    const float3 aa=(lB<lMin||lB>lMax)?a:b;
    return float4((lMax-lMin<max(0.0312,lMax*0.125))?cM.rgb:aa,cM.a);
}
)";
}

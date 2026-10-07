#version 460

layout(location=0) in vec3 inNormal;
layout(location=1) in vec2 inUv;
layout(location=2) in vec4 inColor;
layout(location=0) out vec4 outColor;
layout(binding=0) uniform sampler2D baseTexture;

void main(){
    vec3 normal=normalize(inNormal);
    float light=0.30+0.70*max(dot(normal,normalize(vec3(0.40,0.75,0.52))),0.0);
    vec4 color=inColor*texture(baseTexture,inUv);
    if(color.a<0.05)discard;
    outColor=vec4(color.rgb*light,color.a);
}

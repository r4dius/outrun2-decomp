#version 460

layout(location=0) in vec3 inPosition;
layout(location=1) in vec3 inNormal;
layout(location=2) in vec2 inUv;
layout(location=3) in vec4 inColor;
layout(location=4) in float inTransformIndex;
layout(location=0) out vec3 outNormal;
layout(location=1) out vec2 outUv;
layout(location=2) out vec4 outColor;
layout(std140,binding=0) uniform TransformBlock {
    mat4 positionTransform[9];
    mat4 normalTransform[9];
} transforms;

void main(){
    uint transformIndex=uint(inTransformIndex+0.5);
    gl_Position=transforms.positionTransform[transformIndex]*vec4(inPosition,1.0);
    outNormal=normalize(mat3(transforms.normalTransform[transformIndex])*inNormal);
    outUv=inUv;
    outColor=inColor;
}

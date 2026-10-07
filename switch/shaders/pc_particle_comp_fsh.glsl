// Particle mode 4 composite: the half-resolution smoke (rgb = premultiplied colour,
// a = transmittance) over the scene with blending ONE / SRCALPHA (bilinear upsample).
#version 460
layout(std140,binding=3) uniform HalfPass{vec4 half_pass;vec4 half_uv;};   // uv: frame x, y, 0.5 / image width, height
layout(binding=0) uniform sampler2D half_colour;
layout(location=0) out vec4 out_colour;
void main(){
    out_colour=texture(half_colour,(gl_FragCoord.xy-half_uv.xy)*half_uv.zw);
}

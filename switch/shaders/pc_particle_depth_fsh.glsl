// Particle mode 4: the half-resolution depth of the scene, the farthest of each
// 2x2 block of the full-resolution depth (one gather), drawn with the
// pc_particle_comp_vsh triangle into the half-resolution depth target.
#version 460
layout(std140,binding=3) uniform HalfPass{vec4 half_pass;vec4 half_uv;vec4 depth_uv;};   // depth_uv: frame x, y, 1 / depth width, height
layout(binding=12) uniform sampler2D scene_depth;
void main(){
    vec2 corner=depth_uv.xy+floor(gl_FragCoord.xy)*2.0+1.0;
    vec4 d=textureGather(scene_depth,corner*depth_uv.zw,0);
    gl_FragDepth=max(max(d.x,d.y),max(d.z,d.w));
}

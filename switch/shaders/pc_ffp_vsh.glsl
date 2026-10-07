#version 460
// Direct3D 9 pretransformed vertices (FVF XYZRHW, DrawPrimitiveUP of the PC
// 2D renderer): x, y in PC-screen pixels, z the depth, rhw = 1/w.
layout(location=0) in vec4 in_pos;
layout(location=1) in vec4 in_diffuse;
layout(location=2) in vec4 in_specular;
layout(location=3) in vec4 in_t0;
layout(location=4) in vec4 in_t1;
layout(location=5) in vec4 in_t2;
layout(location=6) in vec4 in_t3;
// screen: 2/width, 2/height of the PC screen, D3D9 pixel-centre offset (x, y);
// present: diffuse, specular present in the FVF, z = untransformed (FVF XYZ:
// p * world * view * projection = wvp, D3D row-major uploaded as is, so
// wvp * p in GLSL); fix: the D3D9 half-pixel fix of transformed vertices.
layout(std140,binding=0) uniform FfpScreen{vec4 screen;ivec4 present;vec4 fix;mat4 wvp;};
layout(location=0) out vec4 out_d0;
layout(location=1) out vec4 out_d1;
layout(location=2) out vec4 out_t0;
layout(location=3) out vec4 out_t1;
layout(location=4) out vec4 out_t2;
layout(location=5) out vec4 out_t3;
layout(location=6) out float out_fog;
void main(){
    if(present.z!=0){
        // Fixed-function transform; clip +y is the top as in D3D9 (pc_vs_uber).
        const vec4 c=wvp*vec4(in_pos.xyz,1.0);
        gl_Position=vec4(c.x+fix.x*c.w,c.y-fix.y*c.w,c.z,c.w);
    }else{
        const float w=1.0/in_pos.w;
        // Clip +y is the top of the screen (see pc_vs_uber).
        const float x=(in_pos.x+screen.z)*screen.x-1.0,y=1.0-(in_pos.y+screen.w)*screen.y;
        gl_Position=vec4(x*w,y*w,in_pos.z*w,w);
    }
    out_d0=present.x!=0?in_diffuse:vec4(1.0);
    out_d1=present.y!=0?vec4(in_specular.rgb,0.0):vec4(0.0);
    out_fog=present.y!=0?in_specular.a:1.0;                 // specular alpha = vertex fog factor
    out_t0=in_t0;out_t1=in_t1;out_t2=in_t2;out_t3=in_t3;
}

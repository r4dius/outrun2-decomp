#version 460
// Port post-process: full-screen triangle for the FXAA pass (no vertex buffer).
void main(){
    const vec2 p=vec2(float((gl_VertexID<<1)&2),float(gl_VertexID&2));
    gl_Position=vec4(p*2.0-1.0,0.0,1.0);
}

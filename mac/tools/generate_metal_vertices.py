"""Emit the retained VS arithmetic as MSL without changing the CPU source."""
import re
from pathlib import Path

def vertex_source(root):
    cpu=(root/'src/platform/pc_shader_programs_generated.cpp').read_text()
    math=(root/'src/platform/pc_shader_programs.hpp').read_text()
    math=math[math.index('inline V4 v4('):math.index('struct VsState')]
    math=math.replace('const V4&','V4').replace('V4&','thread V4&').replace('std::','metal::').replace('V4','float4')
    funcs=cpu[cpu.index('void vs_0('):cpu.index('void ps14_0(')]
    funcs=funcs.replace('VsState&','thread VsState&').replace('V4','float4')
    consts='\n'.join(f'constant int4 pc_vs_program{i} [[function_constant({16+i})]];' for i in range(8))
    attrs='\n'.join(f'float4 v{i} [[attribute({i})]];' for i in range(16))
    inputs='\n'.join(f's.V[{i}]=input.v{i};' for i in range(16))
    cases='\n'.join(f'case {i}:vs_{i}(s);break;' for i in range(73))
    runs='\n'.join(f'if(pc_vs_program{i}.{c}>=0)native_vs::run(pc_vs_program{i}.{c},s);' for i in range(8) for c in 'xyzw')
    return consts+'''\nnamespace native_vs {
struct VsState { float4 R[12]{},V[16]{}; constant float4* C; float4 oPos{},oFog{1,1,1,1},oPts{},oD[2]{},oT[8]{}; };
'''+math+funcs+'''\nvoid run(int blob,thread VsState& s){switch(blob){'''+cases+'''\n}}\n}
struct NativeInput {'''+attrs+'''};
vertex Varying pc_native_vertex(NativeInput input [[stage_in]],constant float4* constants [[buffer(5)]],constant float4& viewport [[buffer(6)]]) {
 native_vs::VsState s{};s.C=constants;
'''+inputs+runs+'''
 Varying o; o.p=s.oPos;o.p.x+=o.p.w/viewport.x;o.p.y-=o.p.w/viewport.y;o.p.z+=viewport.z*o.p.w;
 o.d0=native_vs::sat(s.oD[0]);o.d1=native_vs::sat(s.oD[1]);
 o.t0=s.oT[0];o.t1=s.oT[1];o.t2=s.oT[2];o.t3=s.oT[3];o.t4=s.oT[4];o.t5=s.oT[5];o.t6=s.oT[6];o.t7=s.oT[7];o.fog=native_vs::clamp01(s.oFog.x);return o;
}
'''

#!/usr/bin/env python3
"""Translate the retained shader expressions to MSL; no retail EXE needed.
Only the already generated expressions are rewritten. Unknown layout or
constructs fail the build instead of inventing an instruction translation.
"""
import re
import sys
from pathlib import Path
from generate_metal_vertices import vertex_source

root = Path(__file__).resolve().parents[2]
header = r'''#include <metal_stdlib>
#pragma clang fp contract(off)
using namespace metal;
constant int4 pc_program0 [[function_constant(0)]];
constant int4 pc_program1 [[function_constant(1)]];
constant int4 pc_program2 [[function_constant(2)]];
constant int4 pc_program3 [[function_constant(3)]];
struct Input { packed_float4 p,d0,d1,t0,t1,t2,t3,t4,t5,t6,t7; float fog; };
struct Varying { float4 p [[position]]; float4 d0,d1,t0,t1,t2,t3,t4,t5,t6,t7; float fog; };
struct Uniforms {
 float4 PC[8]; int4 program[4],info,stage_kind[2]; float4 bump[8],lum[2];
 int4 op[4],arg[4],misc[4]; float4 konst[4],tfactor; int4 flags;
 float4 fog_colour,fog_params,alpha_test; float lod_bias[6];
};
struct IO {
 array<texture2d<float>,6> tex2d;
 array<texturecube<float>,6> texcube;
 array<sampler,6> smp;
};
vertex Varying pc_vertex(uint i [[vertex_id]],const device Input* v [[buffer(0)]]) {
 Varying o; o.p=v[i].p; o.d0=v[i].d0;o.d1=v[i].d1;
 o.t0=v[i].t0;o.t1=v[i].t1;o.t2=v[i].t2;o.t3=v[i].t3;
 o.t4=v[i].t4;o.t5=v[i].t5;o.t6=v[i].t6;o.t7=v[i].t7;o.fog=v[i].fog;return o;
}
float4 greaterThan(float4 a,float4 b){return select(float4(0),float4(1),a>b);}
'''
fields = 'PC program info stage_kind bump lum op arg misc konst tfactor flags fog_colour fog_params alpha_test'.split()
parts = [header, vertex_source(root)]
for name, file in [('ffp','pc_ffp_fsh.glsl'),('ps11','pc_ps11_uber_fsh.glsl'),('ps14','pc_ps14_uber_fsh.glsl')]:
    source = (root/'switch/shaders'/file).read_text()   # the PS5 copies are identical
    source = re.sub(r'^layout\([^\n]+\n', '', source, flags=re.M)
    source = re.sub(r'^#version[^\n]+\n', '', source, flags=re.M)
    source = source.replace('void main()', 'float4 evaluate()')
    source = re.sub(r'out_colour=([^;]+);return;', r'return \1;', source)
    source = source.replace('out_colour=c;', 'return clamp(c,0.0f,1.0f);')
    source = source.replace('discard;', 'discard_fragment();')
    source = source.replace('gl_FragCoord', 'v.p')
    for field in fields:
        source = re.sub(r'\b'+field+r'\b', 'u.'+field, source)
    source = re.sub(r'u\.program\[([0-3])\]', r'pc_program\1', source)
    source = re.sub(r'\bin_([dt][0-7]|fog)\b', r'v.\1', source)
    for typ,metal in [('ivec4','int4'),('vec4','float4'),('vec3','float3'),('vec2','float2')]:
        source = re.sub(r'\b'+typ+r'\b', metal, source)
    source = source.replace('uintBitsToFloat(', 'as_type<float>(').replace('inversesqrt(', 'rsqrt(')
    # Texture resources are dynamic, but stage indices remain authored constants.
    source = re.sub(r'texture\((tex2d|texcube)(\d),([^;]+?)\)', lambda m:f'io.{m[1]}[{m[2]}].sample(io.smp[{m[2]}],{m[3]},bias(u.lod_bias[{m[2]}]))', source)
    source = re.sub(r'texture\((tex|cube)(\d),([^;]+?)\)', lambda m:f'io.{"tex2d" if m[1]=="tex" else "texcube"}[{m[2]}].sample(io.smp[{m[2]}],{m[3]},bias(u.lod_bias[{m[2]}]))', source)
    if 'layout(' in source or 'texture(' in source or 'out_colour' in source or 'gl_' in source:
        raise ValueError(f'untranslated shader construct in {file}')
    parts.append(f'struct Eval_{name} {{\n constant Uniforms& u; Varying v; IO io;\n'+source+'\n};\n')
    parts.append(f'''fragment float4 pc_{name}(Varying v [[stage_in]],constant Uniforms& u [[buffer(0)]],
 array<texture2d<float>,6> t [[texture(0)]],array<texturecube<float>,6> c [[texture(6)]],
 array<sampler,6> s [[sampler(0)]]) {{ Eval_{name} e{{u,v,{{t,c,s}}}};return e.evaluate(); }}\n''')
source='\n'.join(parts)
out=Path(sys.argv[1]);out.parent.mkdir(parents=True,exist_ok=True)
out.write_text('#pragma once\nnamespace outrun::mac { inline constexpr const char* PcMetalSource=R"OR2MSL('+source+')OR2MSL"; }\n')

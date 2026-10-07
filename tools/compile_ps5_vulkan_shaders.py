#!/usr/bin/env python3
"""Compile the public recovered GLSL to Vulkan SPIR-V; no game files needed."""
import argparse
import re
import struct
import subprocess
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('output', type=Path)
parser.add_argument('--compiler', default='glslangValidator')
parser.add_argument('--validator', default='spirv-val')
parser.add_argument('--optimizer', default='spirv-opt')
parser.add_argument('--profile-cache', action='append', type=Path, default=[], help='Optional private runtime pipeline cache; bake its observed program combinations')
args = parser.parse_args()
root = Path(__file__).resolve().parent.parent
args.output.parent.mkdir(parents=True, exist_ok=True)
sources = {}
for name, source_name in [('ffp', 'pc_ffp_fsh.glsl'), ('ps11', 'pc_ps11_uber_fsh.glsl'), ('ps14', 'pc_ps14_uber_fsh.glsl')]:
    source = (root / 'ps5/shaders' / source_name).read_text()
    source = re.sub(r'layout\(binding=(\d+)\)( uniform sampler)', lambda m: f'layout(set=0,binding={int(m[1])+3}){m[2]}', source)
    if name == 'ffp':
        source = source.replace('layout(location=6) in float in_fog;', 'layout(location=10) in float in_fog;')
    else:
        declaration = 'ivec4 program[4];ivec4 info;'
        assert declaration in source
        source = source.replace(declaration, 'ivec4 unused_program[4];ivec4 unused_info;')
        constants = ''.join(f'layout(constant_id={i}) const int blob{i}=-1;\n' for i in range(16))
        constants += 'layout(constant_id=16) const int blob_count=0;\n'
        # The recovered dispatch is already unrolled. Refer to scalar
        # specialization constants directly (global const arrays are forbidden).
        source = re.sub(r'program\[(\d)\]\.([xyzw])', lambda m: f'blob{int(m[1])*4+"xyzw".index(m[2])}', source)
        source = source.replace('info.x', 'blob_count')
        at = source.index('\n', source.index('uniform PsProgram')) + 1
        source = source[:at] + constants + source[at:]
    sources[name+'.frag'] = source
sources['pc.vert'] = '''#version 450
layout(location=0) in vec4 p;layout(location=1) in vec4 d0;layout(location=2) in vec4 d1;
layout(location=0) out vec4 od0;layout(location=1) out vec4 od1;
''' + ''.join(f'layout(location={3+i}) in vec4 t{i};layout(location={2+i}) out vec4 ot{i};\n' for i in range(8)) + '''
layout(location=11) in float fog;layout(location=10) out float ofog;
void main(){gl_Position=vec4(p.x,-p.y,p.z,p.w);od0=d0;od1=d1;ofog=fog;
''' + ''.join(f'ot{i}=t{i};' for i in range(8)) + '}\n'
# Reuse the recovered Switch vertex blob equations without changing that port.
# Vulkan specializes the linked instruction list per pipeline, so the driver
# can remove the other blobs and all interpreter dispatch at pipeline creation.
native = (root / 'switch/shaders/pc_vs_uber_vsh.glsl').read_text()
native = native.replace('#version 460', '#version 450')
native = native.replace('layout(std140,binding=0) uniform VsConstants{vec4 C[256];vec4 P[4];};',
                        'layout(std140,set=0,binding=15) uniform VsConstants{vec4 C[256];};')
native = native.replace('layout(std140,binding=2) uniform VsFix{vec4 clip_fix;};',
                        'layout(std140,set=0,binding=16) uniform VsFix{vec4 clip_fix;};')
constants = ''.join(f'layout(constant_id={i}) const int instruction{i}=-1;\n' for i in range(32))
constants += 'layout(constant_id=32) const int colour_inputs=0;\n'
native = native[:native.index('void main(){')] + constants + '''void main(){
    for(int k=0;k<12;++k)R[k]=vec4(0.0);
''' + ''.join(f'V[{i}]=in_v{i};if((colour_inputs & {1<<i})!=0)V[{i}]=V[{i}].zyxw;\n' for i in range(16)) + '''
    oPos=vec4(0.0);oFog=vec4(1.0);oPts=vec4(0.0);oD[0]=vec4(0.0);oD[1]=vec4(0.0);
    for(int k=0;k<8;++k)oT[k]=vec4(0.0);
''' + ''.join(f'if(instruction{i}>=0)run(instruction{i});\n' for i in range(32)) + '''
    gl_Position=vec4(oPos.x+clip_fix.x*oPos.w,-oPos.y+clip_fix.y*oPos.w,oPos.z,oPos.w);
    out_d0=clamp(oD[0],0.0,1.0);out_d1=clamp(oD[1],0.0,1.0);
''' + ''.join(f'out_t{i}=oT[{i}];' for i in range(8)) + '''
    out_fog=clamp(oFog.x,0.0,1.0);
}
'''
sources['native.vert'] = native
# A linkable entry calls each recovered void blob once to keep every function
# in the module. Runtime replaces those call targets with the actual program,
# and pads unused slots with a void no-op. No compiler runs on the console.
templates = {}
for name in ('native.vert', 'ps11.frag', 'ps14.frag'):
    source = sources[name]
    functions = sorted(map(int, re.findall(r'void f(\d+)\(\)', source)))
    dispatch = ''.join(f'f{i}();' for i in functions) + 'or2_noop();'
    before, main = source.split('void main(){', 1)
    pattern = r'if\((?:instruction\d+|blob\d+)>=0\)run\((?:instruction\d+|blob\d+)\);'
    main, count = re.subn(pattern, '', main)
    assert count == (32 if name == 'native.vert' else 16)
    # Dispatch follows input-register initialization, before output conversion.
    marker = '    gl_Position=' if name == 'native.vert' else '    vec4 c=R[0];'
    assert marker in main
    main = main.replace(marker, dispatch+'\n'+marker, 1)
    template_name = 'link_'+name
    sources[template_name] = before+'void or2_noop(){}\nvoid main(){'+main
    templates[template_name] = functions
sources['display.vert'] = '''#version 450
layout(location=0) in vec2 p;layout(location=1) in vec4 c;layout(location=2) in vec2 t;
layout(location=0) out vec4 colour;layout(location=1) out vec2 uv;
void main(){gl_Position=vec4(p.x,-p.y,0,1);colour=c;uv=t;}
'''
sources['display.frag'] = '''#version 450
layout(location=0) in vec4 colour;layout(location=1) in vec2 uv;
layout(set=0,binding=3) uniform sampler2D image;layout(location=0) out vec4 pixel;
void main(){pixel=texture(image,uv)*colour;}
'''
sources['full.vert'] = '''#version 450
void main(){vec2 p=vec2(float((gl_VertexIndex<<1)&2),float(gl_VertexIndex&2));gl_Position=vec4(p*2.0-1.0,0,1);}
'''
# Share the established PS5 GL FXAA equation, while keeping GL source untouched.
gl = (root / 'ps5/source/gl_d3d9.cpp').read_text()
fxaa = gl.split('constexpr const char* FxaaFragment=R"(')[1].split(')";')[0]
sources['fxaa.frag'] = fxaa.replace('layout(binding=0)', 'layout(set=0,binding=3)')
header = ['#pragma once', '#include <cstdint>', '#include <cstddef>', '#include <array>', '#include "vk_shader_link.hpp"', 'namespace outrun::ps5_runtime::vulkan::shaders {']
binaries = {}
def emit(symbol, data):
    words = struct.unpack('<'+'I'*(len(data)//4), data)
    header.append(f'inline constexpr std::uint32_t {symbol}[]={{')
    for i in range(0, len(words), 12):
        header.append(','.join(f'0x{w:08x}' for w in words[i:i+12])+',')
    header.append('};')
for name, source in sources.items():
    path = args.output.parent / name
    path.write_text(source)
    binary = path.with_suffix(path.suffix+'.spv')
    subprocess.run([args.compiler, '-V', '--target-env', 'vulkan1.0', '-o', str(binary), str(path)], check=True)
    subprocess.run([args.validator, '--target-env', 'vulkan1.0', str(binary)], check=True)
    data = binary.read_bytes()
    binaries[name]=binary
    symbol = name.replace('.', '_')
    emit(symbol,data)
    if name in templates:
        words = struct.unpack('<'+'I'*(len(data)//4), data)
        names = {}; calls = []; active = None
        instructions = []; offset = 5
        while offset < len(words):
            length, opcode = words[offset] >> 16, words[offset] & 0xffff
            assert length and offset+length <= len(words)
            instructions.append((offset, length, opcode))
            if opcode == 5:
                names[words[offset+1]] = struct.pack('<'+'I'*(length-2), *words[offset+2:offset+length]).split(b'\0')[0].decode()
            offset += length
        function_ids = {int(re.match(r'f(\d+)\(', value)[1]): key for key, value in names.items() if re.match(r'f(\d+)\(', value)}
        noop = next(key for key, value in names.items() if value.startswith('or2_noop('))
        main_id = next(key for key, value in names.items() if value == 'main')
        for offset, length, opcode in instructions:
            if opcode == 54: active = words[offset+2]
            elif opcode == 56: active = None
            elif opcode == 57 and active == main_id and words[offset+3] in {*function_ids.values(), noop}:
                assert length == 4
                calls.append(offset+3)
        functions = templates[name]
        assert set(function_ids) == set(functions)
        assert [words[at] for at in calls] == [function_ids[i] for i in functions]+[noop]
        ids = [function_ids.get(i, 0) for i in range(max(functions)+1)]
        header.append(f'inline constexpr std::uint32_t {symbol}_functions[]={{'+','.join(map(str, ids))+'};')
        header.append(f'inline constexpr std::uint32_t {symbol}_calls[]={{'+','.join(map(str, calls))+'};')
        header.append(f'inline constexpr ShaderTemplate {symbol}_template{{{symbol},std::size({symbol}),{symbol}_functions,std::size({symbol}_functions),{symbol}_calls,std::size({symbol}_calls),{noop}}};')
        # Validate the same target rewrite used on-console, including repeated
        # blobs, holes, negative entries and padding after the actual program.
        for case, program in enumerate((functions[:3], [functions[-1], functions[0], functions[-1]], [-1, max(functions)+1])):
            linked = list(words)
            for i, at in enumerate(calls):
                linked[at] = function_ids.get(program[i], noop) if i < len(program) else noop
            output = binary.with_name(binary.name+f'.linked{case}')
            output.write_bytes(struct.pack('<'+'I'*len(linked), *linked))
            subprocess.run([args.validator, '--target-env', 'vulkan1.0', str(output)], check=True)

def fnv(data):
    result=0xcbf29ce484222325
    for byte in data:result=((result^byte)*0x100000001b3)&0xffffffffffffffff
    return result

# Only shader-program IDs are read from the optional profile. Driver binaries,
# textures, executable code and game assets are never included in this header.
# Unseen programs keep the existing specialization path. Frozen constants and
# offline dead-code removal avoid asking RADV to optimize the entire uber shader
# again for every vertex layout, blend/depth state or MSAA sample count.
programs=set()
for profile in args.profile_cache:
    data=profile.read_bytes()
    if len(data)<40:raise ValueError(f'Truncated pipeline profile: {profile}')
    magic,version,size,count,cache_bytes,_,checksum=struct.unpack_from('<8sIIIIQQ',data)
    if magic!=b'OR2VKPC2' or version!=2 or size!=1460 or count>8192 or len(data)!=40+count*size+cache_bytes or fnv(data[40:])!=checksum:
        raise ValueError(f'Invalid pipeline profile: {profile}')
    for i in range(count):
        key=struct.unpack_from('<365i',data,40+i*size)
        if key[361]==3:programs.add((3,key[273:306]))
        if key[361] in (0,3) and key[360] in (1,2):programs.add((key[360],key[256:273]))
prepared=[]
for index,(kind,key) in enumerate(sorted(programs)):
    name={1:'ps11.frag',2:'ps14.frag',3:'native.vert'}[kind]
    constant_values=' '.join(f'{i}:{value}' for i,value in enumerate(key))
    symbol=f'prepared_{index}'
    output=args.output.parent/(symbol+'.spv')
    subprocess.run([args.optimizer,str(binaries[name]),'--set-spec-const-default-value',constant_values,
                    '--freeze-spec-const','--fold-spec-const-op-composite','-O','-o',str(output)],check=True)
    subprocess.run([args.validator,'--target-env','vulkan1.0',str(output)],check=True)
    data=output.read_bytes();emit(symbol,data)
    padded=tuple(key)+(0,)*(33-len(key))
    prepared.append('{'+str(kind)+',{{'+','.join(map(str,padded))+'}},'+symbol+',sizeof('+symbol+')}')
    print(f'Offline shader {index}: kind={kind}, {binaries[name].stat().st_size} -> {len(data)} bytes')
header.append('struct PreparedShader {unsigned kind;std::array<std::int32_t,33> key;const std::uint32_t* words;std::size_t bytes;};')
header.append('inline constexpr std::array<PreparedShader,'+str(len(prepared))+'> prepared_shaders{{'+','.join(prepared)+'}};')
header.append('}')
args.output.write_text('\n'.join(header)+'\n')

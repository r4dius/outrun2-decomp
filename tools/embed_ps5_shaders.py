"""Embed maintained PS5 GLSL sources for a self-contained native executable."""
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[1]
output=Path(sys.argv[1])
output.parent.mkdir(parents=True,exist_ok=True)
text=['#pragma once\nnamespace outrun::ps5_runtime::gl_shaders {\n']
for name,file in [('ps11','pc_ps11_uber_fsh.glsl'),('ps14','pc_ps14_uber_fsh.glsl'),('ffp','pc_ffp_fsh.glsl')]:
    shader=(root/'ps5/shaders'/file).read_text().replace('#version 460','#version 450')
    assert ')OR2GLSL"' not in shader
    text.append(f'inline constexpr const char* {name}=R"OR2GLSL({shader})OR2GLSL";\n')
text.append('}\n')
output.write_text(''.join(text))

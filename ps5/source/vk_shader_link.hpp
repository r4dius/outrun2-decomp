#pragma once
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace outrun::ps5_runtime::vulkan {
// All recovered blobs are void functions with the same register globals. Only
// OpFunctionCall targets change: instruction sizes, types and result IDs stay
// intact. The driver sees the selected call graph before its inlining pass.
struct ShaderTemplate {
    const std::uint32_t* words; std::size_t word_count;
    const std::uint32_t* functions; std::size_t function_count;
    const std::uint32_t* calls; std::size_t call_count;
    std::uint32_t noop;
};
inline std::vector<std::uint32_t> link_shader(const ShaderTemplate& source,
                                             const std::int32_t* program,std::size_t count){
    if(count>source.call_count)throw std::runtime_error("Shader instruction list exceeds link slots");
    std::vector<std::uint32_t> words(source.words,source.words+source.word_count);
    for(std::size_t i=0;i<source.call_count;++i){
        const auto offset=source.calls[i];
        if(offset<3||offset>=words.size()||words[offset-3]!=((4u<<16)|57u))
            throw std::runtime_error("Invalid SPIR-V link slot");
        const auto blob=i<count?program[i]:-1;
        words[offset]=blob>=0&&std::size_t(blob)<source.function_count&&source.functions[blob]?
            source.functions[blob]:source.noop;
    }
    return words;
}
}

#include "platform/pc_shader_programs.hpp"
namespace outrun::platform {
namespace {
unsigned table_of(std::uint32_t version){
    switch(version){case 0xfffe0101u:return 0;case 0xffff0101u:return 1;case 0xffff0104u:return 2;default:return 3;}
}
}
bool pc_shader_split(const std::uint32_t* tokens,PcShaderProgram& out){
    out=PcShaderProgram{};
    out.version=tokens[0];
    const auto table=table_of(out.version);
    if(table>2)return false;
    std::size_t n=0;while(tokens[1+n]!=0xffffu){++n;if(n>0x10000u)return false;}
    const std::uint32_t* body=tokens+1;
    // Reachability over positions with the blobs of this version (non-empty bodies).
    std::vector<std::int32_t> from(n+1,-2),via(n+1,-1);from[0]=-1;
    for(std::size_t p=0;p<n;++p){
        if(from[p]==-2)continue;
        for(std::size_t b=0;b<PcShaderBlobCount;++b){
            const auto& blob=PcShaderBlobs[b];
            if(table_of(blob.version)!=table||blob.token_count==0||p+blob.token_count>n)continue;
            if(from[p+blob.token_count]!=-2)continue;
            if(std::memcmp(body+p,PcShaderBlobTokens+blob.token_offset,blob.token_count*4u))continue;
            from[p+blob.token_count]=std::int32_t(p);via[p+blob.token_count]=std::int32_t(blob.index);
        }
    }
    if(n&&from[n]==-2)return false;
    for(std::size_t p=n;p>0;p=std::size_t(from[p]))out.blobs.insert(out.blobs.begin(),std::uint32_t(via[p]));
    // Input declarations (vs): dcl usage token then the v register.
    if(table==0){
        for(std::size_t p=0;p<n;){
            const auto op=body[p]&0xffffu;std::size_t q=p+1;
            while(q<n&&(body[q]&0x80000000u))++q;
            if(op==31u&&q==p+3)out.inputs.push_back({std::uint8_t(body[p+1]&0x1fu),std::uint8_t((body[p+1]>>16)&0xfu),std::uint8_t(body[p+2]&0x7ffu)});
            p=q;
        }
    }
    return true;
}
void pc_shader_run_vs(const PcShaderProgram& p,pc_shader::VsState& s){for(auto b:p.blobs)PcShaderBlobFns_vs[b](s);}
void pc_shader_run_ps14(const PcShaderProgram& p,pc_shader::Ps14State& s){for(auto b:p.blobs)PcShaderBlobFns_ps14[b](s);}
void pc_shader_run_ps11(const PcShaderProgram& p,pc_shader::Ps11State& s){for(auto b:p.blobs)PcShaderBlobFns_ps11[b](s);}
}

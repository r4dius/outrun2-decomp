#include "frontend_glyph_render.hpp"
#include <algorithm>
#include <cmath>
namespace outrun::platform {
unsigned frontend_glyph_layer(const FrontendGlyph& g){
    const auto signed_mode=static_cast<std::int32_t>(g.mode);
    return unsigned(std::clamp(signed_mode,0,20));
}
bool frontend_glyph_quad(const FrontendGlyph& g,const GameUiTexture& t,FrontendGlyphQuad& out){
    const int source_right=(g.left==0&&g.right==0)?int(t.width):g.right;
    const int source_bottom=(g.top==0&&g.bottom==0)?int(t.height):g.bottom;
    if(!t.width||!t.height||g.left<0||g.top<0||source_right<g.left||source_bottom<g.top||
       unsigned(source_right)>t.width||unsigned(source_bottom)>t.height||
       !std::isfinite(g.x)||!std::isfinite(g.y)||!std::isfinite(g.scale_x)||!std::isfinite(g.scale_y))return false;
    // PC 42A168 decrements right for source spans wider than two texels.
    const int right=source_right-(source_right-g.left>2?1:0);
    const float x1=g.x+float(right-g.left)*g.scale_x,y1=g.y+float(source_bottom-g.top)*g.scale_y;
    if(!std::isfinite(x1)||!std::isfinite(y1))return false;
    constexpr float half_y=.90f,half_x=.90f*(640.f/480.f)/(1280.f/720.f);
    const std::array<float,4> rgba{{float((g.color>>16)&255)/255.f,float((g.color>>8)&255)/255.f,
                                  float(g.color&255)/255.f,float(g.color>>24)/255.f}};
    for(unsigned i=0;i<4;++i){const bool r=i==1||i==2,b=i>=2;auto& v=out[i];
        v.position={{-half_x+(r?x1:g.x)*(2*half_x/640.f),half_y-(b?y1:g.y)*(2*half_y/480.f),0}};
        v.normal={{.401422f,.752666f,.521849f}};
        // 42A152 flips the source Y span; negative Y scale restores screen Y.
        v.uv={{float(r?right:g.left)/float(t.width),1.f-float(b?source_bottom:g.top)/float(t.height)}};
        v.color=rgba;v.transform_index=8;
    }return true;
}
}

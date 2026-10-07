#pragma once
#include "platform/mesh_preview_pack.hpp"
#include <d3d11.h>
#include <wrl/client.h>
#include <array>
#include <map>
#include <string>
#include <vector>
namespace outrun::xbox_runtime {
using Microsoft::WRL::ComPtr;
// The output's refresh rate, raised to 120 Hz on the first call when the
// output offers it; pace_frames(fps) presents every refresh/fps vblanks.
unsigned output_refresh();
void pace_frames(unsigned fps);
// The composed frame on the window's swap chain: the same interface as the
// PS5 GL display (ps5/source/gl_display.hpp). The D3D11 device and the swap
// chain belong to the process (one window); every display shares them, so
// the setup screen and the game renderer can open one after the other.
class D3D11Display {
public:
    ~D3D11Display();
    bool open(unsigned width,unsigned height,std::string& error);
    void clear();
    ID3D11ShaderResourceView* texels(const void* key,platform::MeshPreviewTextureFormat,unsigned,unsigned,const std::vector<std::uint8_t>&);
    void quad(const std::array<platform::MeshPreviewVertex,4>&,ID3D11ShaderResourceView* texture,unsigned,unsigned);
    void rgba(const void* key,const std::uint8_t* bytes,unsigned,unsigned,int,int,int,int);
    void pc_frame(ID3D11ShaderResourceView* texture,int x,int y,int width,int height);
    bool present(std::string& error);
    ID3D11Device* device()const;
    ID3D11DeviceContext* context()const;
private:
    ID3D11ShaderResourceView* upload(const void*,const std::uint8_t*,unsigned,unsigned,bool);
    void rectangle(ID3D11ShaderResourceView*,int,int,int,int);
    struct Image {ComPtr<ID3D11Texture2D> texture;ComPtr<ID3D11ShaderResourceView> view;unsigned width{},height{};};
    std::map<const void*,Image> images_;
    unsigned width_{},height_{};
    ComPtr<ID3D11VertexShader> vertex_;ComPtr<ID3D11PixelShader> pixel_;ComPtr<ID3D11InputLayout> layout_;
    ComPtr<ID3D11Buffer> vbo_;ComPtr<ID3D11BlendState> blend_,opaque_;ComPtr<ID3D11SamplerState> sampler_;
    ComPtr<ID3D11RasterizerState> raster_;ComPtr<ID3D11DepthStencilState> no_depth_;
};
}

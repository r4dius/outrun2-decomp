// macOS adaptation of ps5/source/sdl_backend.hpp; reference baseline remains unchanged.
#pragma once
#include "platform/mesh_preview_pack.hpp"
#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include <chrono>
struct SDL_Window;struct SDL_Renderer;struct SDL_Texture;
namespace outrun::mac_runtime {
class SdlDisplay {
public:
    ~SdlDisplay();
    bool open(unsigned width,unsigned height,std::string& error);
    void clear();
    SDL_Texture* texels(const void* key,platform::MeshPreviewTextureFormat,unsigned w,unsigned h,const std::vector<std::uint8_t>&);
    void quad(const std::array<platform::MeshPreviewVertex,4>&,SDL_Texture*,unsigned,unsigned);
    void rgba(const void* key,const std::uint8_t* bytes,unsigned w,unsigned h,int x,int y,int width,int height,bool opaque=false);
    bool present(std::string& error);
    bool read_pixels(std::vector<std::uint8_t>& pixels,std::string& error);
private:
    SDL_Texture* upload(const void* key,const std::uint8_t*,unsigned,unsigned,bool update);
    struct Image {SDL_Texture* texture{};unsigned width{},height{};};
    SDL_Window* window_{};SDL_Renderer* renderer_{};
    std::map<const void*,Image> textures_;
    unsigned width_{},height_{};
    std::chrono::steady_clock::time_point next_frame_{};
};
}

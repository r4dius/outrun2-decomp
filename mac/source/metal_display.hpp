#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
namespace outrun::mac {
// Presents the portable renderer's RGBA output. This is not a translation
// of the D3D9 scene pipeline: scene rasterisation remains in PcSoftD3D9Device.
class MetalDisplay {
public:
    MetalDisplay();
    ~MetalDisplay();
    MetalDisplay(const MetalDisplay&)=delete;
    MetalDisplay& operator=(const MetalDisplay&)=delete;
    bool open(unsigned width,unsigned height,bool window,std::string& error);
    bool present(const std::uint8_t* rgba,unsigned width,unsigned height,std::string& error);
    bool readback(std::vector<std::uint8_t>& bgra,std::string& error);
    bool running();
    std::string device_name() const;
    void* native_window() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}

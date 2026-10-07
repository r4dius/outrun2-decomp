#pragma once
#include "platform/runtime_input.hpp"
#include "input/dual_sense.hpp"
#include <memory>
#include <string>
#include <vector>
namespace outrun::mac {
class SdlServices {
public:
    SdlServices();
    ~SdlServices();
    SdlServices(const SdlServices&)=delete;
    SdlServices& operator=(const SdlServices&)=delete;
    bool open(std::string& error,void* native_window=nullptr);
    platform::NativeInputState sample(std::uint32_t mode);
    bool submit(const std::vector<std::int16_t>& pcm,std::string& error);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}

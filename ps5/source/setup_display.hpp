#pragma once
// PS5 display for the first-launch setup screen (system/setup_screen): the
// same display class the renderer uses, opened before it and closed after.
#include "system/setup_screen.hpp"
#include <memory>
namespace outrun::ps5_runtime {
class SetupDisplay {
public:
    SetupDisplay();
    ~SetupDisplay();
    void show(const platform::SetupScreen& screen);
    void close();
    platform::SetupPlatform platform();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    bool failed_=false;
};
}

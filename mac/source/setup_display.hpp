#pragma once
// macOS window for the first-launch setup screen (system/setup_screen):
// an SDL window that shows its pixels until the Metal game window opens.
#include "system/setup_screen.hpp"
struct SDL_Window;struct SDL_Renderer;struct SDL_Texture;
namespace outrun::mac_runtime {
class SetupDisplay {
public:
    ~SetupDisplay(){close();}
    void show(const platform::SetupScreen& screen);
    bool pump();              // handles window events; false once the player closes it
    void wait_for_close();    // after an error: until the window is closed or a key/button is pressed
    void close();
    platform::SetupPlatform platform();
private:
    SDL_Window* window_=nullptr;SDL_Renderer* renderer_=nullptr;SDL_Texture* texture_=nullptr;
    bool quit_=false;
};
}

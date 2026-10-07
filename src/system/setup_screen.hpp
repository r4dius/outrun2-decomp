#pragma once
// First-launch preparation: finds the player's decompressed OR2006C2C.EXE,
// checks it, writes the image cache, and shows the progress on a screen drawn
// in software as plain text (each platform only copies the pixels to its
// display). Nothing is shown when the prepared data is already there.
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
namespace outrun::platform {
class SetupScreen {
public:
    enum class State{Pending,Running,Done,Failed};
    explicit SetupScreen(int width=1280,int height=720);
    void set_steps(const std::vector<std::string>& labels);
    void set_step(std::size_t index,State state);
    void set_progress(double fraction);          // 0..1, negative hides the bar
    void set_detail(const std::string& text);    // one line under the bar
    void set_footer(const std::string& text);
    void set_message(const std::string& text);     // line after the steps and a blank line ("Enjoy!")
    void set_error(const std::string& title,const std::vector<std::string>& lines);
    // Draws the current state; seconds drives the animation.
    void render(double seconds);
    // RGBA8888: bytes R, G, B, A in memory (0xAABBGGRR as a little-endian word).
    const std::uint32_t* pixels() const{return pixels_.data();}
    int width() const{return width_;}
    int height() const{return height_;}
private:
    struct Step{std::string label;State state=State::Pending;};
    void fill(int x,int y,int w,int h,std::uint32_t rgba,int alpha);
    void text(int x,int y,const std::string& s,int scale,std::uint32_t rgba,bool shadow=true);
    int text_width(const std::string& s,int scale) const;
    int width_,height_;
    std::vector<std::uint32_t> pixels_;
    std::vector<Step> steps_;
    double progress_=-1.0;
    std::string detail_,footer_,message_,error_title_;
    std::vector<std::string> error_lines_;
};

// Platform hooks for exe_setup_prepare.
struct SetupPlatform {
    std::function<void(const SetupScreen&)> present;   // show the pixels (may be empty: headless)
    std::function<bool()> keep_running;                // false: the player quits (or the system asks to)
    std::function<void()> wait_for_exit;               // after an error: wait for the player's exit button
    std::string exit_hint;                              // e.g. "Press + to exit."
};

// Loads <game_dir>/OR2006C2C.cache (or <fallback_dir>/OR2006C2C.cache), or on
// first launch (or when it is from another build and the EXE is still there)
// builds it from the EXE found in game_dir while showing the setup screen. It
// is saved next to the game files, or in fallback_dir (the platform's own
// folder, may be empty) when the game folder is read-only. Returns false with
// *error when no usable EXE exists (the screen then shows what to do).
bool exe_setup_prepare(const std::string& game_dir,const std::string& fallback_dir,const SetupPlatform& platform,std::string* error);
}

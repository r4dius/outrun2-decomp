#pragma once
#include <chrono>

namespace outrun::xbox_runtime {
// Cocoa can hold the event pump through a window/fullscreen animation. The
// movie resumes from its current position instead of decoding the whole pause
// into the bounded audio mixer queue.
class MovieClock {
public:
    using Clock=std::chrono::steady_clock;
    void reset(Clock::time_point now){last_=now;elapsed_=0.0;}
    bool advance(Clock::time_point now){
        const double delta=std::chrono::duration<double>(now-last_).count();
        last_=now;
        const bool paused=delta>0.1;
        if(delta>0.0)elapsed_+=paused?1.0/60.0:delta;
        return paused;
    }
    double elapsed()const{return elapsed_;}
private:
    Clock::time_point last_{};
    double elapsed_{};
};
}

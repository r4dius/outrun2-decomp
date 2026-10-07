#include "display_rate.hpp"
#import <AppKit/AppKit.h>

namespace outrun::mac_runtime {
unsigned screen_max_frames_per_second(){
    @autoreleasepool {
        NSScreen* screen=NSApp.keyWindow.screen?:NSApp.mainWindow.screen?:NSScreen.mainScreen;
        if(!screen)return 60;
        if(@available(macOS 12.0,*))return unsigned(screen.maximumFramesPerSecond);
        return 60;
    }
}
}

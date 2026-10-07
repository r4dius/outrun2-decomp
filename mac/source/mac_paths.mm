#include "mac_paths.hpp"
#import <Foundation/Foundation.h>
#import <Cocoa/Cocoa.h>
namespace outrun::mac {
std::string application_folder(){
    @autoreleasepool {
        NSString* bundle=[NSBundle mainBundle].bundlePath;
        if([bundle.pathExtension isEqualToString:@"app"])return bundle.stringByDeletingLastPathComponent.UTF8String;
        return [NSBundle mainBundle].executablePath.stringByDeletingLastPathComponent.UTF8String;
    }
}
bool choose_retail_directory(std::string& path){
    @autoreleasepool {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        [NSApp activateIgnoringOtherApps:YES];
        NSOpenPanel* panel=[NSOpenPanel openPanel];
        panel.canChooseFiles=NO;panel.canChooseDirectories=YES;panel.allowsMultipleSelection=NO;
        panel.message=@"Choose your OutRun 2006 Coast 2 Coast folder (Steam), or put this application in it.";
        if([panel runModal]!=NSModalResponseOK)return false;
        path=panel.URL.path.UTF8String;return true;
    }
}
bool user_paths(Paths& paths,std::string& error){
    @autoreleasepool {
        NSFileManager* manager=[NSFileManager defaultManager];NSError* e=nil;
        NSURL* support=[manager URLForDirectory:NSApplicationSupportDirectory inDomain:NSUserDomainMask appropriateForURL:nil create:YES error:&e];
        if(!support){error=e.localizedDescription.UTF8String;return false;}
        support=[support URLByAppendingPathComponent:@"OutRunMac/Saves" isDirectory:YES];
        if(![manager createDirectoryAtURL:support withIntermediateDirectories:YES attributes:nil error:&e]){error=e.localizedDescription.UTF8String;return false;}
        paths.saves=support.path.UTF8String;return true;
    }
}
}

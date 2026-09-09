#import <AppKit/AppKit.h>
#include "wallclock/platform.h"
#include <algorithm>
#include <cstdio>
#include <stdexcept>

namespace wc { namespace platform {
bool visible(){return true;}
std::pair<int,int> screen_size() {
    @autoreleasepool {
        NSScreen* screen=[NSScreen mainScreen];
        if(!screen) throw std::runtime_error("No graphical desktop session");
        NSSize size=screen.frame.size; CGFloat scale=screen.backingScaleFactor;
        return {int(size.width*scale),int(size.height*scale)};
    }
}
void apply(const std::string& path) {
    @autoreleasepool {
        NSString* string=[NSString stringWithUTF8String:path.c_str()];
        NSURL* url=[NSURL fileURLWithPath:string]; NSError* error=nil;
        for(NSScreen* screen in [NSScreen screens]) {
            if(![[NSWorkspace sharedWorkspace] setDesktopImageURL:url forScreen:screen options:@{} error:&error])
                throw std::runtime_error(error.localizedDescription.UTF8String);
        }
    }
}
void publish(const std::string& from,const std::string& to){if(std::rename(from.c_str(),to.c_str())) throw std::runtime_error("Cannot publish wallpaper");}
void wait_until(std::time_t deadline) {
    @autoreleasepool {
        __block bool wake=false;
        auto block=^(NSNotification*) {wake=true;};
        NSNotificationCenter* system=[NSNotificationCenter defaultCenter];
        NSNotificationCenter* workspace=[[NSWorkspace sharedWorkspace] notificationCenter];
        id clock=[system addObserverForName:NSSystemClockDidChangeNotification object:nil queue:nil usingBlock:block];
        id display=[system addObserverForName:NSApplicationDidChangeScreenParametersNotification object:nil queue:nil usingBlock:block];
        id resume=[workspace addObserverForName:NSWorkspaceDidWakeNotification object:nil queue:nil usingBlock:block];
        // A one-shot run-loop timer keeps the run loop blocked even with no other sources.
        NSTimer* timer=[NSTimer scheduledTimerWithTimeInterval:std::max(0.001,double(deadline-std::time(nullptr))) repeats:NO block:^(NSTimer*){wake=true;}];
        while(!wake) [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate distantFuture]];
        [timer invalidate]; [system removeObserver:clock];[system removeObserver:display];[workspace removeObserver:resume];
    }
}
const char* name(){return "macOS (same image on attached screens; adapter unverified on hardware)";}
const char* image_extension(){return ".png";}
}}

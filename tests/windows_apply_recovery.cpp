// Explicit integration test: reapplies the currently selected wallpaper after an
// adapter failure. Fake time avoids a real-minute delay; desktop selection is retained.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "wallclock/platform.h"
#include "wallclock/scheduler.h"
#include <iostream>
#include <vector>
#include <stdexcept>
int main(){
    try {
        wchar_t selected[32768]={};
        if(!SystemParametersInfoW(SPI_GETDESKWALLPAPER,32768,selected,0) || !selected[0]) throw std::runtime_error("Test requires an existing wallpaper image");
        int length=WideCharToMultiByte(CP_UTF8,0,selected,-1,nullptr,0,nullptr,nullptr);
        std::vector<char> buffer(length);WideCharToMultiByte(CP_UTF8,0,selected,-1,buffer.data(),length,nullptr,nullptr);
        std::string path=buffer.data();std::time_t now=10000;unsigned attempts=0;std::vector<int> sleeps;std::vector<std::string> logs;
        auto missing=path+".absent-recovery-test-"+std::to_string(GetCurrentProcessId());
        wc::LoopHost host{[&]{return now;},[&](std::time_t due){sleeps.push_back(int(due-now));now=due;return wc::Wake::deadline;},[]{return true;},[&](const std::string& log){logs.push_back(log);}};
        wc::run_loop([&](std::time_t time,bool){wc::platform::apply(++attempts==1?missing:path);return time+60;},host,2);
        wchar_t after[32768]={};SystemParametersInfoW(SPI_GETDESKWALLPAPER,32768,after,0);
        if(attempts!=3 || sleeps!=std::vector<int>({60,60}) || logs.size()!=2 || logs[1].find("recovered")==std::string::npos || std::wstring(selected)!=after)
            throw std::runtime_error("Adapter failure recovery regression");
        std::cout<<"Passed Windows adapter failure -> 60s fake-time retry -> two real successful applies; wallpaper selection retained\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}

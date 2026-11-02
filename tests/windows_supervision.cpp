#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include "wallclock/windows_supervision.h"
#include "wallclock/windows_clock.h"
#include <iostream>
#include <stdexcept>
int main(){
    try {
        unsigned checks=0;auto check=[&](bool value){++checks;if(!value)throw std::runtime_error("Supervision regression failed");};
        for(const auto& input:{L"",L"C:\\path with spaces\\",L"a\"b",L"a\\\"b",L"C:\\clock\\file.ini",L"\u00e9\u4e00"}) {
            auto command=L"program "+wc::supervision::quote(input);int count=0;
            auto args=CommandLineToArgvW(command.c_str(),&count);
            check(args && count==2 && std::wstring(args[1])==input);LocalFree(args);
        }
        check(wc::supervision::restart_seconds(1)==60);
        check(wc::supervision::restart_seconds(2)==120);
        check(wc::supervision::restart_seconds(3)==240);
        check(wc::supervision::restart_seconds(4)==480);
        check(wc::supervision::restart_seconds(100)==900);
        check(wc::supervision::failures_after_exit(8,599999)==9);
        check(wc::supervision::failures_after_exit(8,600000)==1);
        check(!wc::supervision::restart_exit(0));check(!wc::supervision::restart_exit(2));
        check(wc::supervision::restart_exit(1));check(wc::supervision::restart_exit(0xc0000005));
        auto before=wc::WindowsClock::now();Sleep(10);auto after=wc::WindowsClock::now();
        check(after>before);
        check(std::chrono::duration_cast<std::chrono::seconds>(after.time_since_epoch()).count()!=std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
        std::cout<<"Passed "<<checks<<" supervision checks\n";return 0;
    } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}

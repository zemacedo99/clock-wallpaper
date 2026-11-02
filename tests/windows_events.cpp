#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "wallclock/platform.h"
#include "wallclock/windows_events.h"
#include <iostream>
#include <stdexcept>
#include <chrono>
#include <fstream>
int checks=0;
void check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
BOOL CALLBACK find_window(HWND candidate,LPARAM result) {
    wchar_t name[128]={};GetClassNameW(candidate,name,128);
    if(std::wstring(name)==L"WallclockWakeWindow"){*reinterpret_cast<HWND*>(result)=candidate;return FALSE;}
    return TRUE;
}
DWORD WINAPI send_event(LPVOID window){Sleep(50);DWORD_PTR result=0;SendMessageTimeoutW(static_cast<HWND>(window),WM_TIMECHANGE,0,0,SMTO_ABORTIFHUNG,5000,&result);return 0;}
int main(){
 try {
    wc::platform::visible();
    HWND window=nullptr;
    EnumThreadWindows(GetCurrentThreadId(),find_window,reinterpret_cast<LPARAM>(&window));
    check(window!=nullptr,"event window exists");
    // Consume the initial power-state notification before measuring a new event.
    while(wc::platform::wait_until(std::time(nullptr))!=wc::Wake::deadline) {}
    auto started=std::chrono::steady_clock::now();
    HANDLE sender=CreateThread(nullptr,0,send_event,window,0,nullptr);
    check(sender!=nullptr,"cross-thread event sender created");
    auto reason=wc::platform::wait_until(std::time(nullptr)+3);
    WaitForSingleObject(sender,INFINITE);CloseHandle(sender);
    auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started).count();
    check(reason==wc::Wake::environment && elapsed<1000,"sent event interrupts blocked GetMessage before timer deadline");
    // Reproduce an event arriving during rendering and drained by visible().
    SendMessageW(window,WM_TIMECHANGE,0,0);
    wc::platform::visible();
    check(wc::platform::wait_until(std::time(nullptr)+2)==wc::Wake::environment,"time change survives visibility drain before wait");
    SendMessageW(window,0x02B1,7,0); // Synthetic session-lock message to our test window only.
    check(!wc::platform::visible(),"lock suppresses rendering");
    check(wc::platform::wait_until(std::time(nullptr)+2)==wc::Wake::visibility,"lock interrupts wait");
    SendMessageW(window,0x02B1,8,0);
    check(wc::platform::visible(),"unlock restores visibility");
    check(wc::platform::wait_until(std::time(nullptr)+2)==wc::Wake::visibility,"unlock interrupts wait");
    struct Power {GUID setting;DWORD length;DWORD state;} power={{0x6fe69556,0x704a,0x47a0,{0x8f,0x24,0xc2,0x8d,0x93,0x6f,0xda,0x47}},sizeof(DWORD),0};
    SendMessageW(window,WM_POWERBROADCAST,0x8013,reinterpret_cast<LPARAM>(&power));
    check(!wc::platform::visible(),"display off suppresses rendering");
    check(wc::platform::wait_until(std::time(nullptr)+2)==wc::Wake::visibility,"display off wakes wait");
    power.state=1;SendMessageW(window,WM_POWERBROADCAST,0x8013,reinterpret_cast<LPARAM>(&power));
    check(wc::platform::visible(),"display on restores rendering");
    check(wc::platform::wait_until(std::time(nullptr)+2)==wc::Wake::visibility,"display on interrupts wait");
    power.state=2;SendMessageW(window,WM_POWERBROADCAST,0x8013,reinterpret_cast<LPARAM>(&power));
    check(wc::platform::visible(),"dim display remains visible");
    wc::platform::wait_until(std::time(nullptr)+2);
    SendMessageW(window,WM_POWERBROADCAST,PBT_APMSUSPEND,0);
    check(!wc::platform::visible(),"suspend suppresses rendering");
    wc::platform::wait_until(std::time(nullptr)+2);
    SendMessageW(window,WM_POWERBROADCAST,PBT_APMRESUMEAUTOMATIC,0);
    check(wc::platform::visible(),"resume clears suspended state");
    check(wc::platform::wait_until(std::time(nullptr)+2)==wc::Wake::environment,"resume recomputes current time");
    SendMessageW(window,WM_SETTINGCHANGE,0,reinterpret_cast<LPARAM>(L"intl"));
    wc::platform::visible();
    check(wc::platform::wait_until(std::time(nullptr)+2)==wc::Wake::environment,"timezone notification survives queue drain");
    check(wc::platform::wait_until(std::time(nullptr)+1)==wc::Wake::deadline,"native timer wakes on deadline");
    std::string folder="test-watch-"+std::to_string(GetCurrentProcessId());
    check(CreateDirectoryA(folder.c_str(),nullptr)!=0,"create isolated watcher directory");
    std::string config=folder+"/active.ini",other=folder+"/other.txt";
    {std::ofstream out(config);out<<"initial";}
    wc::platform::watch_config(config);
    {std::ofstream out(other);out<<"unrelated";}
    check(wc::platform::wait_until(std::time(nullptr)+1)==wc::Wake::deadline && !wc::platform::config_changed(),"unrelated directory change does not reload configuration");
    {std::ofstream out(config);out<<"changed config";}
    check(wc::platform::wait_until(std::time(nullptr)+2)==wc::Wake::environment && wc::platform::config_changed(),"changed configuration wakes native wait");
    check(!wc::platform::config_changed(),"configuration notification consumed once");
    {std::ofstream out(other);out<<"atomic replacement";}
    check(MoveFileExA(other.c_str(),config.c_str(),MOVEFILE_REPLACE_EXISTING)!=0,"atomic config replacement succeeds");
    check(wc::platform::wait_until(std::time(nullptr)+2)==wc::Wake::environment && wc::platform::config_changed(),"atomic editor save triggers reload");
    wc::platform::watch_config("");DeleteFileA(config.c_str());RemoveDirectoryA(folder.c_str());
    PostThreadMessageW(GetCurrentThreadId(),WM_APP+1,0,0);
    wc::platform::visible();
    check(wc::platform::wait_until(std::time(nullptr)+2)==wc::Wake::stop,"stop survives visibility drain");
    SendMessageW(window,WM_CLOSE,0,0);
    check(!wc::platform::visible(),"supervisor close prevents further updates");
    check(wc::platform::wait_until(std::time(nullptr)+2)==wc::Wake::stop,"supervisor close stops runner");
    std::cout<<"Passed "<<checks<<" native event checks (synthetic; no wallpaper changed)\n";
    return 0;
 } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}

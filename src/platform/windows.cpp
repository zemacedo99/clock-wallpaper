#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "wallclock/platform.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace wc { namespace platform { namespace {
std::wstring wide(const std::string& s) {
    int size=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.c_str(),-1,nullptr,0);
    if(!size) throw std::runtime_error("Invalid UTF-8 path");
    std::vector<wchar_t> data(size); MultiByteToWideChar(CP_UTF8,0,s.c_str(),-1,data.data(),size);
    return data.data();
}
std::wstring absolute(const std::string& path) {
    std::wstring input=wide(path); DWORD size=GetFullPathNameW(input.c_str(),0,nullptr,nullptr);
    if(!size) throw std::runtime_error("Cannot resolve wallpaper path");
    std::vector<wchar_t> output(size);
    if(!GetFullPathNameW(input.c_str(),size,output.data(),nullptr)) throw std::runtime_error("Cannot resolve wallpaper path");
    return output.data();
}
bool wake=false;
bool display_on=true;
bool session_locked=false;
const GUID display_state={0x6fe69556,0x704a,0x47a0,{0x8f,0x24,0xc2,0x8d,0x93,0x6f,0xda,0x47}};
struct PowerSetting {GUID setting;DWORD length;unsigned char data[1];};
LRESULT CALLBACK window_proc(HWND hwnd,UINT message,WPARAM wp,LPARAM lp) {
    if(message==WM_TIMER || message==WM_TIMECHANGE || message==WM_DISPLAYCHANGE ||
       (message==WM_POWERBROADCAST && (wp==PBT_APMRESUMEAUTOMATIC || wp==PBT_APMRESUMESUSPEND))) wake=true;
    if(message==WM_TIMECHANGE) _tzset();
    if(message==WM_POWERBROADCAST && wp==0x8013 && lp) { // PBT_POWERSETTINGCHANGE
        auto* setting=reinterpret_cast<PowerSetting*>(lp);
        if(std::memcmp(&setting->setting,&display_state,sizeof(GUID))==0 && setting->length==sizeof(DWORD)) {
            DWORD state=0;std::memcpy(&state,setting->data,sizeof(state));display_on=state!=0;wake=true;
        }
    }
    if(message==0x02B1) { // WM_WTSSESSION_CHANGE
        if(wp==0x7) session_locked=true;
        if(wp==0x8) session_locked=false;
        wake=true;
    }
    return DefWindowProcW(hwnd,message,wp,lp);
}
HWND event_window() {
    static HWND window=[](){
        WNDCLASSW cls={}; cls.lpfnWndProc=window_proc; cls.hInstance=GetModuleHandleW(nullptr); cls.lpszClassName=L"WallclockWakeWindow";
        if(!RegisterClassW(&cls) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS) throw std::runtime_error("Cannot register event window");
        HWND result=CreateWindowW(cls.lpszClassName,L"",0,0,0,0,0,nullptr,nullptr,cls.hInstance,nullptr);
        if(!result) throw std::runtime_error("Cannot create event window");
        using RegisterPower = HANDLE (WINAPI*)(HANDLE,LPCGUID,DWORD);
        auto power=reinterpret_cast<RegisterPower>(GetProcAddress(GetModuleHandleW(L"user32.dll"),"RegisterPowerSettingNotification"));
        if(power) power(result,&display_state,0);
        // The OS releases these registrations when this process exits.
        HMODULE wts=LoadLibraryW(L"wtsapi32.dll");
        using RegisterSession = BOOL (WINAPI*)(HWND,DWORD);
        auto session=wts?reinterpret_cast<RegisterSession>(GetProcAddress(wts,"WTSRegisterSessionNotification")):nullptr;
        if(session) session(result,0);
        return result;
    }();
    return window;
}
}
bool visible() {
    event_window();MSG message;
    while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) {TranslateMessage(&message);DispatchMessageW(&message);}
    return display_on && !session_locked;
}
std::pair<int,int> screen_size() {
    using SetDpi = BOOL (WINAPI*)();
    auto set_dpi=reinterpret_cast<SetDpi>(GetProcAddress(GetModuleHandleW(L"user32.dll"),"SetProcessDPIAware"));
    if(set_dpi) set_dpi();
    return {GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN)};
}
void apply(const std::string& path) {
    std::wstring full=absolute(path);
    if(GetFileAttributesW(full.c_str())==INVALID_FILE_ATTRIBUTES) throw std::runtime_error("Wallpaper file does not exist");
    // Notify desktop, but avoid rewriting the user profile every minute.
    if(!SystemParametersInfoW(SPI_SETDESKWALLPAPER,0,const_cast<wchar_t*>(full.c_str()),SPIF_SENDCHANGE))
        throw std::runtime_error("Windows rejected wallpaper (error "+std::to_string(GetLastError())+")");
}
void publish(const std::string& temporary,const std::string& destination) {
    if(!MoveFileExW(wide(temporary).c_str(),wide(destination).c_str(),MOVEFILE_REPLACE_EXISTING))
        throw std::runtime_error("Cannot publish wallpaper (error "+std::to_string(GetLastError())+")");
}
void wait_until(std::time_t deadline) {
    HWND window=event_window(); wake=false;
    auto seconds=std::max<std::time_t>(0,deadline-std::time(nullptr));
    UINT delay=static_cast<UINT>(std::min<std::time_t>(seconds,26*3600)*1000);
    if(!SetTimer(window,1,std::max(1u,delay),nullptr)) throw std::runtime_error("Cannot arm wallpaper timer");
    MSG message;
    while(!wake) {
        int result=GetMessageW(&message,nullptr,0,0);
        if(result<=0) {KillTimer(window,1); throw std::runtime_error("Event loop ended");}
        TranslateMessage(&message); DispatchMessageW(&message);
    }
    KillTimer(window,1);
}
const char* name(){return "Windows (primary-resolution wallpaper, shared across monitors)";}
const char* image_extension(){return ".bmp";}
}}

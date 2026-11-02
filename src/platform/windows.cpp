#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "wallclock/platform.h"
#include "wallclock/windows_events.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <vector>
#include <chrono>
#include <iostream>

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
Wake wake_reason=Wake::deadline;
DWORD event_thread=0;
bool stopping=false;
bool pending_event=false;
bool suspended=false;
HANDLE config_watch=INVALID_HANDLE_VALUE;
bool config_dirty=false;
std::wstring watched_file;
WIN32_FILE_ATTRIBUTE_DATA config_stamp={};
bool config_exists=false;
std::function<void(const std::string&)> event_log;
void report(const std::string& message){if(event_log) event_log(message);}
BOOL WINAPI control(DWORD type) {
    if(type==CTRL_C_EVENT || type==CTRL_BREAK_EVENT) {PostThreadMessageW(event_thread,WM_APP+1,0,0);return TRUE;}
    return FALSE;
}
bool display_on=true;
bool session_locked=false;
const GUID display_state={0x6fe69556,0x704a,0x47a0,{0x8f,0x24,0xc2,0x8d,0x93,0x6f,0xda,0x47}};
struct PowerSetting {GUID setting;DWORD length;unsigned char data[1];};
LRESULT CALLBACK window_proc(HWND hwnd,UINT message,WPARAM wp,LPARAM lp) {
    // GetMessage dispatches synchronous sent messages internally and may keep
    // blocking afterwards. A queued wake guarantees it returns to our wait loop.
    // Do not post from the wake message itself (or from periodic timer messages).
    if(message==WM_CLOSE || message==WM_ENDSESSION || message==WM_SETTINGCHANGE ||
       message==WM_TIMECHANGE || message==WM_DISPLAYCHANGE || message==WM_POWERBROADCAST || message==0x02B1)
        PostMessageW(hwnd,WM_APP+2,0,0);
    if(message==WM_CLOSE || (message==WM_ENDSESSION && wp)) {
        stopping=true;wake=true;report("event=lifecycle action=stop");return 0;
    }
    if(message==WM_QUERYENDSESSION) return TRUE;
    if(message==WM_SETTINGCHANGE && lp && std::wcscmp(reinterpret_cast<const wchar_t*>(lp),L"intl")==0) {
        _tzset();wake=true;wake_reason=Wake::environment;pending_event=true;
        report("event=lifecycle action=timezone_settings");
    }
    if(message==WM_TIMER || message==WM_TIMECHANGE || message==WM_DISPLAYCHANGE ||
       (message==WM_POWERBROADCAST && (wp==PBT_APMRESUMEAUTOMATIC || wp==PBT_APMRESUMESUSPEND))) wake=true;
    if(message==WM_TIMECHANGE || message==WM_DISPLAYCHANGE ||
       (message==WM_POWERBROADCAST && (wp==PBT_APMRESUMEAUTOMATIC || wp==PBT_APMRESUMESUSPEND))) {
        wake_reason=Wake::environment;pending_event=true;
    }
    if(message==WM_TIMECHANGE) {_tzset();report("event=lifecycle action=time_change");}
    if(message==WM_POWERBROADCAST && wp==PBT_APMSUSPEND) {
        suspended=true;wake=true;wake_reason=Wake::visibility;pending_event=true;
        report("event=lifecycle action=suspend");
    }
    if(message==WM_POWERBROADCAST && (wp==PBT_APMRESUMEAUTOMATIC || wp==PBT_APMRESUMESUSPEND)) {
        suspended=false;_tzset();report("event=lifecycle action=resume");
    }
    if(message==WM_POWERBROADCAST && wp==0x8013 && lp) { // PBT_POWERSETTINGCHANGE
        auto* setting=reinterpret_cast<PowerSetting*>(lp);
        if(std::memcmp(&setting->setting,&display_state,sizeof(GUID))==0 && setting->length==sizeof(DWORD)) {
            DWORD state=0;std::memcpy(&state,setting->data,sizeof(state));display_on=state!=0;wake=true;wake_reason=Wake::visibility;pending_event=true;
            report(std::string("event=lifecycle action=display state=")+(display_on?"on":"off"));
        }
    }
    if(message==0x02B1) { // WM_WTSSESSION_CHANGE
        if(wp==0x7) session_locked=true;
        if(wp==0x8) session_locked=false;
        if(wp==7 || wp==8) report(std::string("event=lifecycle action=session state=")+(session_locked?"locked":"unlocked"));
        wake=true;wake_reason=Wake::visibility;pending_event=true;
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
        if(!power || !power(result,&display_state,0)) std::cerr<<"event=warning power_notifications=unavailable\n";
        // The OS releases these registrations when this process exits.
        HMODULE wts=LoadLibraryW(L"wtsapi32.dll");
        using RegisterSession = BOOL (WINAPI*)(HWND,DWORD);
        auto session=wts?reinterpret_cast<RegisterSession>(GetProcAddress(wts,"WTSRegisterSessionNotification")):nullptr;
        if(!session || !session(result,0)) std::cerr<<"event=warning session_notifications=unavailable\n";
        event_thread=GetCurrentThreadId();SetConsoleCtrlHandler(control,TRUE);
        return result;
    }();
    return window;
}
}
void set_event_log(std::function<void(const std::string&)> logger){event_log=std::move(logger);}
void watch_config(const std::string& path) {
    if(config_watch!=INVALID_HANDLE_VALUE) FindCloseChangeNotification(config_watch);
    config_watch=INVALID_HANDLE_VALUE;config_dirty=false;
    if(path.empty()) return;
    auto full=absolute(path);auto slash=full.find_last_of(L"\\/");watched_file=full;
    config_exists=GetFileAttributesExW(full.c_str(),GetFileExInfoStandard,&config_stamp)!=0;
    config_watch=FindFirstChangeNotificationW(full.substr(0,slash).c_str(),FALSE,FILE_NOTIFY_CHANGE_FILE_NAME|FILE_NOTIFY_CHANGE_LAST_WRITE|FILE_NOTIFY_CHANGE_SIZE);
    if(config_watch==INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot watch config directory");
}
bool config_changed(){bool changed=config_dirty;config_dirty=false;return changed;}
void configure(const std::string& fit) {
    // One user-session engine prevents two native clocks from overwriting each other.
    HANDLE owner=CreateMutexW(nullptr,FALSE,L"Local\\WallclockDesktopEngine");
    if(!owner) throw std::runtime_error("Cannot create engine mutex");
    if(GetLastError()==ERROR_ALREADY_EXISTS){CloseHandle(owner);throw AlreadyRunning("Another wallclock engine is already running in this session");}
    event_window();
    if(fit!="fill") return;
    HKEY key;
    if(RegOpenKeyExW(HKEY_CURRENT_USER,L"Control Panel\\Desktop",0,KEY_QUERY_VALUE|KEY_SET_VALUE,&key)!=ERROR_SUCCESS)
        throw std::runtime_error("Cannot select Fill wallpaper layout; use fit=keep or check desktop policy");
    auto set=[&](const wchar_t* name,const wchar_t* value,DWORD bytes){
        wchar_t existing[32]={};DWORD length=sizeof(existing),type=0;
        if(RegQueryValueExW(key,name,nullptr,&type,reinterpret_cast<BYTE*>(existing),&length)==ERROR_SUCCESS && type==REG_SZ && std::wstring(existing)==value) return LONG(ERROR_SUCCESS);
        return RegSetValueExW(key,name,0,REG_SZ,reinterpret_cast<const BYTE*>(value),bytes);
    };
    LONG a=set(L"WallpaperStyle",L"10",6),b=set(L"TileWallpaper",L"0",4);RegCloseKey(key);
    if(a || b) throw std::runtime_error("Cannot set wallpaper layout");
}
bool visible() {
    event_window();MSG message;
    while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) {
        if(message.message==WM_APP+1) stopping=true;
        TranslateMessage(&message);DispatchMessageW(&message);
    }
    return display_on && !session_locked && !suspended && !stopping;
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
    // Persist and broadcast, matching Python's successfully deployed native call.
    if(!SystemParametersInfoW(SPI_SETDESKWALLPAPER,0,const_cast<wchar_t*>(full.c_str()),SPIF_UPDATEINIFILE|SPIF_SENDCHANGE))
        throw std::runtime_error("Windows rejected wallpaper (error "+std::to_string(GetLastError())+")");
    wchar_t selected[32768]={};
    if(!SystemParametersInfoW(SPI_GETDESKWALLPAPER,32768,selected,0) || full!=selected)
        throw std::runtime_error("Windows wallpaper readback differs; another app or policy may be overriding it");
}
void remove_file(const std::string& path) {DeleteFileW(wide(path).c_str());}
void publish(const std::string& temporary,const std::string& destination) {
    if(!MoveFileExW(wide(temporary).c_str(),wide(destination).c_str(),MOVEFILE_REPLACE_EXISTING))
        throw std::runtime_error("Cannot publish wallpaper (error "+std::to_string(GetLastError())+")");
}
Wake wait_until(std::time_t deadline) {
    if(stopping) return Wake::stop;
    HWND window=event_window();
    if(pending_event){pending_event=false;return wake_reason;}
    wake=false;wake_reason=Wake::deadline;
    // Millisecond remainder avoids drifting late by the fractional current second.
    auto remaining=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::from_time_t(deadline)-std::chrono::system_clock::now()).count();
    UINT delay=static_cast<UINT>(std::max<long long>(1,std::min<long long>(remaining+1,26*3600*1000)));
    if(!SetTimer(window,1,std::max(1u,delay),nullptr)) throw std::runtime_error("Cannot arm wallpaper timer");
    MSG message;
    while(!wake) {
        if(config_watch!=INVALID_HANDLE_VALUE) {
            DWORD result=MsgWaitForMultipleObjects(1,&config_watch,FALSE,INFINITE,QS_ALLINPUT);
            if(result==WAIT_OBJECT_0) {
                if(!FindNextChangeNotification(config_watch)){KillTimer(window,1);throw std::runtime_error("Config watcher failed");}
                WIN32_FILE_ATTRIBUTE_DATA stamp={};bool exists=GetFileAttributesExW(watched_file.c_str(),GetFileExInfoStandard,&stamp)!=0;
                bool changed=exists!=config_exists || stamp.nFileSizeLow!=config_stamp.nFileSizeLow || stamp.nFileSizeHigh!=config_stamp.nFileSizeHigh || CompareFileTime(&stamp.ftLastWriteTime,&config_stamp.ftLastWriteTime)!=0;
                config_exists=exists;config_stamp=stamp;
                if(changed){config_dirty=true;wake=true;wake_reason=Wake::environment;break;}
                continue;
            }
            if(result!=WAIT_OBJECT_0+1){KillTimer(window,1);throw std::runtime_error("Config event wait failed");}
            // Sent messages may have been dispatched inside the wait. Drain only
            // available messages; do not enter a second blocking GetMessage.
            while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) {
                if(message.message==WM_APP+1 || message.message==WM_QUIT){KillTimer(window,1);return Wake::stop;}
                TranslateMessage(&message);DispatchMessageW(&message);
            }
            continue;
        }
        int result=GetMessageW(&message,nullptr,0,0);
        if(result==0 || (result>0 && message.message==WM_APP+1)) {KillTimer(window,1);return Wake::stop;}
        if(result<=0) {KillTimer(window,1); throw std::runtime_error("Event loop ended");}
        TranslateMessage(&message); DispatchMessageW(&message);
    }
    KillTimer(window,1);
    pending_event=false;
    return wake_reason;
}
const char* name(){return "Windows (primary-resolution wallpaper, shared across monitors)";}
const char* image_extension(){return ".bmp";}
}}

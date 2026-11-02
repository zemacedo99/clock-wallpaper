#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include "wallclock/windows_supervision.h"
#include "wallclock/windows_clock.h"
#include <stdexcept>
#include <string>
#include <vector>
#include <ctime>
#include <chrono>

namespace {
const wchar_t* settings=L"Software\\Wallclock";
const wchar_t* run_key=L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
const wchar_t* stop_name=L"Local\\WallclockSupervisorStop";
struct Handle {
    HANDLE value=nullptr;
    explicit Handle(HANDLE v=nullptr):value(v){}
    ~Handle(){if(value && value!=INVALID_HANDLE_VALUE) CloseHandle(value);}
    Handle(const Handle&)=delete;Handle& operator=(const Handle&)=delete;
};
std::wstring log_path;
std::string error(const char* operation){return std::string(operation)+" win32="+std::to_string(GetLastError());}
void log(const std::string& message) {
    const auto line="epoch="+std::to_string(std::time(nullptr))+" "+message+"\r\n";
    OutputDebugStringA(line.c_str());
    WIN32_FILE_ATTRIBUTE_DATA data={};
    if(GetFileAttributesExW(log_path.c_str(),GetFileExInfoStandard,&data) && (data.nFileSizeHigh || data.nFileSizeLow>262144))
        MoveFileExW(log_path.c_str(),(log_path+L".1").c_str(),MOVEFILE_REPLACE_EXISTING);
    Handle file(CreateFileW(log_path.c_str(),FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr));
    DWORD written=0;
    if(file.value==INVALID_HANDLE_VALUE || !WriteFile(file.value,line.data(),DWORD(line.size()),&written,nullptr))
        OutputDebugStringA("Wallclock supervisor log unavailable\n");
}
std::wstring executable() {
    std::vector<wchar_t> path(32768);DWORD n=GetModuleFileNameW(nullptr,path.data(),DWORD(path.size()));
    if(!n || n>=path.size()) throw std::runtime_error(error("GetModuleFileName"));
    return std::wstring(path.data(),n);
}
std::wstring absolute(const std::wstring& value) {
    std::vector<wchar_t> path(32768);DWORD n=GetFullPathNameW(value.c_str(),DWORD(path.size()),path.data(),nullptr);
    if(!n || n>=path.size()) throw std::runtime_error("Invalid or oversized path");
    return std::wstring(path.data(),n);
}
std::wstring read_setting(const wchar_t* name) {
    HKEY key=nullptr;LONG result=RegOpenKeyExW(HKEY_CURRENT_USER,settings,0,KEY_QUERY_VALUE,&key);
    if(result!=ERROR_SUCCESS) throw std::runtime_error("Startup not configured; use --install --config FILE --output PREFIX");
    std::vector<wchar_t> buffer(32768);DWORD bytes=DWORD(buffer.size()*sizeof(wchar_t)),type=0;
    result=RegQueryValueExW(key,name,nullptr,&type,reinterpret_cast<BYTE*>(buffer.data()),&bytes);RegCloseKey(key);
    if(result!=ERROR_SUCCESS || type!=REG_SZ || bytes<sizeof(wchar_t) || bytes%sizeof(wchar_t) || buffer[bytes/sizeof(wchar_t)-1]!=0)
        throw std::runtime_error("Invalid startup setting");
    return buffer.data();
}
void write_setting(const wchar_t* path,const wchar_t* name,const std::wstring& value) {
    HKEY key=nullptr;LONG result=RegCreateKeyExW(HKEY_CURRENT_USER,path,0,nullptr,0,KEY_SET_VALUE,nullptr,&key,nullptr);
    if(result==ERROR_SUCCESS){result=RegSetValueExW(key,name,0,REG_SZ,reinterpret_cast<const BYTE*>(value.c_str()),DWORD((value.size()+1)*sizeof(wchar_t)));RegCloseKey(key);}
    if(result!=ERROR_SUCCESS) throw std::runtime_error("Cannot write per-user startup registry value: "+std::to_string(result));
}
void validate(const std::wstring& config,const std::wstring& output,const std::wstring& engine) {
    for(const auto& file:{config,engine}) {
        DWORD attrs=GetFileAttributesW(file.c_str());
        if(attrs==INVALID_FILE_ATTRIBUTES || (attrs&FILE_ATTRIBUTE_DIRECTORY)) throw std::runtime_error("Config or adjacent wallclock.exe is missing");
    }
    auto directory=output.substr(0,output.find_last_of(L"\\/"));
    DWORD attrs=GetFileAttributesW(directory.c_str());
    if(attrs==INVALID_FILE_ATTRIBUTES || !(attrs&FILE_ATTRIBUTE_DIRECTORY)) throw std::runtime_error("Output directory must exist");
}
BOOL CALLBACK close_window(HWND window,LPARAM pid) {
    DWORD owner=0;GetWindowThreadProcessId(window,&owner);
    if(owner==DWORD(pid)) PostMessageW(window,WM_CLOSE,0,0);
    return TRUE;
}
void stop_child(HANDLE process,DWORD pid,HANDLE job) {
    log("event=stop_requested child_pid="+std::to_string(pid));
    EnumWindows(close_window,LPARAM(pid));
    if(WaitForSingleObject(process,10000)!=WAIT_OBJECT_0){log("event=forced_stop timeout_s=10");TerminateJobObject(job,3);WaitForSingleObject(process,5000);}
}
int supervise(const std::wstring& config,const std::wstring& output,const std::wstring& engine,const std::wstring& engine_log) {
    Handle mutex(CreateMutexW(nullptr,FALSE,L"Local\\WallclockSupervisor"));
    if(!mutex.value) throw std::runtime_error(error("CreateMutex"));
    if(GetLastError()==ERROR_ALREADY_EXISTS){log("event=duplicate_supervisor");return 2;}
    Handle stop(CreateEventW(nullptr,TRUE,FALSE,stop_name));
    if(!stop.value) throw std::runtime_error(error("CreateEvent"));
    ResetEvent(stop.value);
    Handle job(CreateJobObjectW(nullptr,nullptr));JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits={};
    limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if(!job.value || !SetInformationJobObject(job.value,JobObjectExtendedLimitInformation,&limits,sizeof(limits))) throw std::runtime_error(error("Create child job"));
    unsigned failures=0;
    log("event=supervisor_started pid="+std::to_string(GetCurrentProcessId()));
    for(;;) {
        if(WaitForSingleObject(stop.value,0)==WAIT_OBJECT_0) break;
        DWORD exit_code=1;auto started=wc::WindowsClock::now();
        auto uptime=[&](){return std::chrono::duration_cast<std::chrono::milliseconds>(wc::WindowsClock::now()-started).count();};
        const auto command=wc::supervision::quote(engine)+L" --config "+wc::supervision::quote(config)+L" --run "+wc::supervision::quote(output)+L" --auto-size"+(engine_log.empty()?L"":L" --log "+wc::supervision::quote(engine_log));
        std::vector<wchar_t> mutable_command(command.begin(),command.end());mutable_command.push_back(0);
        STARTUPINFOW startup={};startup.cb=sizeof(startup);PROCESS_INFORMATION child={};
        // No inherited file/pipe handles; the engine owns its bounded log.
        if(!CreateProcessW(engine.c_str(),mutable_command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW|CREATE_SUSPENDED,nullptr,nullptr,&startup,&child)) {
            log("event=start_failed "+error("CreateProcess"));
        } else {
            Handle process(child.hProcess),thread(child.hThread);
            if(!AssignProcessToJobObject(job.value,process.value) || ResumeThread(thread.value)==DWORD(-1)) {
                log("event=start_failed "+error("Assign/resume child"));TerminateProcess(process.value,1);WaitForSingleObject(process.value,5000);
            } else {
                log("event=child_started child_pid="+std::to_string(child.dwProcessId));
                HANDLE handles[]={stop.value,process.value};DWORD result=WaitForMultipleObjects(2,handles,FALSE,INFINITE);
                if(result==WAIT_OBJECT_0){stop_child(process.value,child.dwProcessId,job.value);break;}
                if(result!=WAIT_OBJECT_0+1) throw std::runtime_error(error("WaitForMultipleObjects"));
                if(!GetExitCodeProcess(process.value,&exit_code)) throw std::runtime_error(error("GetExitCodeProcess"));
                log("event=child_exit code="+std::to_string(exit_code)+" uptime_ms="+std::to_string(uptime()));
                if(!wc::supervision::restart_exit(exit_code)){log(exit_code==2?"event=existing_engine supervisor_stopping":"event=clean_exit supervisor_stopping");break;}
            }
        }
        failures=wc::supervision::failures_after_exit(failures,uptime());
        unsigned seconds=wc::supervision::restart_seconds(failures);
        log("event=restart_scheduled failures="+std::to_string(failures)+" delay_s="+std::to_string(seconds));
        DWORD result=WaitForSingleObject(stop.value,seconds*1000);
        if(result==WAIT_OBJECT_0) break;
        if(result!=WAIT_TIMEOUT) throw std::runtime_error(error("Restart wait"));
    }
    log("event=supervisor_stopped");return 0;
}
}
int WINAPI WinMain(HINSTANCE,HINSTANCE,LPSTR,int) {
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    try {
        wchar_t local[32768]={};DWORD n=GetEnvironmentVariableW(L"LOCALAPPDATA",local,32768);
        if(!n || n>=32768) throw std::runtime_error("LOCALAPPDATA unavailable");
        std::wstring directory=std::wstring(local)+L"\\Wallclock";
        CreateDirectoryW(directory.c_str(),nullptr);directory+=L"\\logs";CreateDirectoryW(directory.c_str(),nullptr);log_path=directory+L"\\startup.log";
        log("event=launcher_invoked pid="+std::to_string(GetCurrentProcessId()));
        int argc=0;wchar_t** argv=CommandLineToArgvW(GetCommandLineW(),&argc);
        if(!argv) throw std::runtime_error(error("CommandLineToArgvW"));
        std::vector<std::wstring> args(argv,argv+argc);LocalFree(argv);
        std::wstring mode,config,output,engine_log;
        for(size_t i=1;i<args.size();++i) {
            if(args[i]==L"--config" || args[i]==L"--output" || args[i]==L"--log") {
                auto key=args[i];if(++i==args.size()) throw std::runtime_error("Missing path argument");
                (key==L"--config"?config:key==L"--output"?output:engine_log)=absolute(args[i]);
            } else if(mode.empty()) mode=args[i];else throw std::runtime_error("Unexpected argument");
        }
        if(mode==L"--stop") {
            Handle stop(OpenEventW(EVENT_MODIFY_STATE,FALSE,stop_name));
            if(!stop.value){log("event=stop_no_supervisor");return 3;}
            if(!SetEvent(stop.value)) throw std::runtime_error(error("SetEvent"));
            return 0;
        }
        if(mode==L"--status") {
            Handle mutex(OpenMutexW(SYNCHRONIZE,FALSE,L"Local\\WallclockSupervisor"));
            log(mutex.value?"event=status supervisor=running":"event=status supervisor=stopped");return mutex.value?0:3;
        }
        if(mode==L"--uninstall") {
            HKEY key=nullptr;LONG result=RegOpenKeyExW(HKEY_CURRENT_USER,run_key,0,KEY_SET_VALUE,&key);
            if(result==ERROR_SUCCESS){result=RegDeleteValueW(key,L"Wallclock");RegCloseKey(key);}
            if(result!=ERROR_SUCCESS && result!=ERROR_FILE_NOT_FOUND) throw std::runtime_error("Cannot remove startup value");
            log("event=startup_uninstalled running_process_unchanged");return 0;
        }
        if(mode!=L"--install" && mode!=L"--start") throw std::runtime_error("Usage: --install --config FILE --output PREFIX | --start [--config FILE --output PREFIX] | --stop | --status | --uninstall");
        if(config.empty()!=output.empty()) throw std::runtime_error("Specify both config and output");
        if(config.empty()) {config=read_setting(L"Config");output=read_setting(L"Output");try{engine_log=read_setting(L"Log");}catch(const std::exception&){} }
        if(!engine_log.empty()) log_path=engine_log.substr(0,engine_log.find_last_of(L"\\/"))+L"\\startup.log";
        auto self=executable(),engine=self.substr(0,self.find_last_of(L"\\/"))+L"\\wallclock.exe";
        if(mode==L"--install") {
            validate(config,output,engine);
            auto command=wc::supervision::quote(self)+L" --start";
            if(command.size()>260) throw std::runtime_error("Run command exceeds Windows' 260-character limit; move the installation");
            write_setting(settings,L"Config",config);write_setting(settings,L"Output",output);write_setting(settings,L"Log",engine_log);
            write_setting(run_key,L"Wallclock",command);log("event=startup_installed starts_at_next_login");return 0;
        }
        // Missing files at login are retried by the child/start path, not a silent supervisor exit.
        return supervise(config,output,engine,engine_log);
    } catch(const std::exception& e){log(std::string("event=supervisor_fatal error=")+e.what());return 1;}
}

#include "wallclock/platform.h"
#include <sys/timerfd.h>
#include <sys/wait.h>
#include <poll.h>
#include <signal.h>
#include <unistd.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <vector>
#include <limits.h>

namespace wc { namespace platform { namespace {
std::string backend;
volatile sig_atomic_t stopping=0;
void stop(int){stopping=1;}
void command(const std::vector<std::string>& args) {
    // Block SIGCHLD before fork to avoid the exit-before-wait race. No polling timer.
    sigset_t set,old;sigemptyset(&set);sigaddset(&set,SIGCHLD);sigprocmask(SIG_BLOCK,&set,&old);
    pid_t pid=fork();
    if(pid==0) {
        sigprocmask(SIG_SETMASK,&old,nullptr);
        std::vector<char*> argv;for(const auto& a:args) argv.push_back(const_cast<char*>(a.c_str()));argv.push_back(nullptr);
        execvp(argv[0],argv.data());_exit(127);
    }
    if(pid<0){sigprocmask(SIG_SETMASK,&old,nullptr);throw std::runtime_error("Cannot launch wallpaper command");}
    int status=0;timespec limit{15,0};
    int signal=sigtimedwait(&set,nullptr,&limit);
    if(waitpid(pid,&status,WNOHANG)!=pid) {
        kill(pid,SIGKILL);while(waitpid(pid,&status,0)<0 && errno==EINTR){}
        sigprocmask(SIG_SETMASK,&old,nullptr);
        throw std::runtime_error(signal<0?"Wallpaper command timed out/interrupted":"Wallpaper command did not finish");
    }
    sigprocmask(SIG_SETMASK,&old,nullptr);
    if(!WIFEXITED(status) || WEXITSTATUS(status)) throw std::runtime_error("Wallpaper command failed: "+args[0]);
}
std::string uri(const std::string& path) {
    std::string result="file://";const char* hex="0123456789ABCDEF";
    for(unsigned char c:path) {
        if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='/'||c=='-'||c=='_'||c=='.'||c=='~') result+=char(c);
        else {result+='%';result+=hex[c>>4];result+=hex[c&15];}
    }
    return result;
}
}
void configure(const std::string&) {
    const char* force=std::getenv("WALLCLOCK_LINUX_BACKEND");const char* desktop=std::getenv("XDG_CURRENT_DESKTOP");
    if(force) backend=force;
    else if(desktop && (std::strstr(desktop,"GNOME") || std::strstr(desktop,"gnome"))) backend="gnome";
    else if(desktop && (std::strstr(desktop,"KDE") || std::strstr(desktop,"Plasma"))) backend="kde";
    else throw std::runtime_error("Unsupported Linux desktop; use export or WALLCLOCK_LINUX_BACKEND=gnome|kde");
    if(backend!="gnome" && backend!="kde") throw std::runtime_error("Unknown Linux backend");
    struct sigaction action{};action.sa_handler=stop;sigemptyset(&action.sa_mask);
    sigaction(SIGINT,&action,nullptr);sigaction(SIGTERM,&action,nullptr);
}
bool visible(){return true;} // Desktop visibility integration is still pending.
std::pair<int,int> screen_size(){throw std::runtime_error("Linux: specify --size WIDTHxHEIGHT (physical pixels)");}
void apply(const std::string& path) {
    char full[PATH_MAX];if(!realpath(path.c_str(),full)) throw std::runtime_error("Cannot resolve wallpaper file");
    if(backend=="gnome") {
        auto value=uri(full);
        command({"gsettings","set","org.gnome.desktop.background","picture-uri",value});
        command({"gsettings","set","org.gnome.desktop.background","picture-uri-dark",value});
    } else command({"plasma-apply-wallpaperimage",full});
}
void publish(const std::string& from,const std::string& to){if(std::rename(from.c_str(),to.c_str())) throw std::runtime_error("Cannot publish image");}
void remove_file(const std::string& path){std::remove(path.c_str());}
Wake wait_until(std::time_t deadline) {
    if(stopping) return Wake::stop;
    int fd=timerfd_create(CLOCK_REALTIME,TFD_CLOEXEC);
    if(fd<0) throw std::runtime_error("Cannot create native timer");
    itimerspec timer{};timer.it_value.tv_sec=deadline;
    if(timerfd_settime(fd,TFD_TIMER_ABSTIME|TFD_TIMER_CANCEL_ON_SET,&timer,nullptr)<0){close(fd);throw std::runtime_error("Cannot arm native timer");}
    pollfd descriptor{fd,POLLIN,0};int result=poll(&descriptor,1,-1);
    if(stopping){close(fd);return Wake::stop;}
    unsigned long long expirations=0;ssize_t n=result>=0?read(fd,&expirations,sizeof(expirations)):-1;
    close(fd);tzset();
    return n<0?Wake::environment:Wake::deadline;
}
const char* name(){return "Linux GNOME/KDE (native host unverified on hardware)";}
const char* image_extension(){return ".png";}
}}

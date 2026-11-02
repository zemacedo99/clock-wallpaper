#include "wallclock/core.h"
#include "wallclock/renderer.h"
#include "wallclock/platform.h"
#include "wallclock/scheduler.h"
#ifdef _WIN32
#include "wallclock/windows_events.h"
#include "wallclock/windows_clock.h"
#endif
#include <chrono>
#include <cstdint>
#include <deque>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <memory>

namespace {
#ifdef _WIN32
using Clock=wc::WindowsClock;
#else
using Clock=std::chrono::steady_clock;
#endif
double millis(Clock::time_point a,Clock::time_point b){return std::chrono::duration<double,std::milli>(b-a).count();}
std::uint64_t hash(const wc::Image& image) {
    std::uint64_t value=14695981039346656037ULL;
    for(auto byte:image.pixels){value^=byte;value*=1099511628211ULL;}
    return value;
}
class Log {
    std::string path_;
public:
    explicit Log(std::string path):path_(std::move(path)){}
    void write(const std::string& message) {
        std::string line="epoch="+std::to_string(std::time(nullptr))+" monotonic_ms="+std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch()).count())+" "+message;
        std::cout<<line<<std::endl;
        if(path_.empty()) return;
        std::ifstream existing(path_,std::ios::binary|std::ios::ate);
        bool rotate=existing && existing.tellg()>262144;existing.close();
        if(rotate) {
            try {wc::platform::publish(path_,path_+".1");}
            catch(const std::exception& e){std::cerr<<"event=warning log_rotation_failed error="<<e.what()<<std::endl;return;}
        }
        std::ofstream out(path_,std::ios::app);out<<line<<'\n';out.flush();
        if(!out) std::cerr<<"event=warning log_write_failed path="<<path_<<std::endl;
    }
};
void help(){std::cout<<
    "wallclock --config FILE (--render FILE.png | --once PREFIX | --run PREFIX)\n"
    "          [--size WIDTHxHEIGHT | --auto-size] [--at HH:MM] [--log FILE] [--frames N]\n"
    "--render only exports. --once changes wallpaper once. --run keeps updating until Ctrl+C.\n"
    "--at is preview-only. --frames limits successful run updates for verification.\n"
    "Output directories must exist. Run writes PREFIX.log (bounded) by default.\n"
    "Platform: "<<wc::platform::name()<<"\n";}
}
int main(int argc,char** argv) {
    std::string config_path="examples/minimal.ini",mode,output,at,size,log_path;
    bool auto_size=false;unsigned frames=0;
    try {
        for(int i=1;i<argc;++i) {
            std::string arg=argv[i];
            if(arg=="--help" || arg=="-h"){help();return 0;}
            if(arg=="--auto-size"){auto_size=true;continue;}
            if(arg!="--config" && arg!="--size" && arg!="--at" && arg!="--render" && arg!="--once" && arg!="--run" && arg!="--log" && arg!="--frames") throw std::runtime_error("Unknown argument: "+arg);
            if(++i==argc) throw std::runtime_error("Missing value for "+arg);
            std::string value=argv[i];
            if(arg=="--config") config_path=value;
            else if(arg=="--size") size=value;
            else if(arg=="--at") at=value;
            else if(arg=="--log") log_path=value;
            else if(arg=="--frames") {size_t n=0;int f=std::stoi(value,&n);if(n!=value.size() || f<1) throw std::runtime_error("Invalid frame count");frames=unsigned(f);}
            else {if(!mode.empty()) throw std::runtime_error("Choose one output mode");mode=arg;output=value;}
        }
        if(mode.empty()){help();return 1;}
        if(!at.empty() && mode!="--render") throw std::runtime_error("--at is preview-only; applied clocks always use real time");
        if(frames && mode!="--run") throw std::runtime_error("--frames requires --run");
        if(auto_size && !size.empty()) throw std::runtime_error("Choose --size or --auto-size");
        if(log_path.empty() && mode!="--render") log_path=output+".log";
        Log log(log_path);
        wc::Config config=wc::load_config(config_path);
        if(!size.empty()) {
            auto x=size.find('x');size_t a=0,b=0;
            if(x==std::string::npos) throw std::runtime_error("Size must be WIDTHxHEIGHT");
            config.width=std::stoi(size.substr(0,x),&a);config.height=std::stoi(size.substr(x+1),&b);
            if(a!=x || b!=size.size()-x-1) throw std::runtime_error("Size must be WIDTHxHEIGHT");
            wc::validate_size(config.width,config.height);
        }
        std::unique_ptr<wc::Renderer> renderer(new wc::Renderer);renderer->validate(config);
        if(mode=="--render") {
            auto local=wc::local_time(std::time(nullptr));
            if(!at.empty()){int m=wc::parse_time(at);local.tm_hour=m/60;local.tm_min=m%60;local.tm_sec=0;}
            if(auto_size){auto s=wc::platform::screen_size();config.width=s.first;config.height=s.second;}
            wc::write_image(renderer->render(wc::select_scene(config,local),local,config.width,config.height),output);
            log.write("event=exported file="+output+" applied=false");return 0;
        }
#ifdef _WIN32
        wc::platform::set_event_log([&](const std::string& message){log.write(message);});
#endif
        wc::platform::configure(config.fit);
#ifdef _WIN32
        if(mode=="--run") wc::platform::watch_config(config_path);
#endif
        log.write("event=started mode="+mode+" platform="+wc::platform::name());
        std::uint64_t previous_hash=0;bool has_previous=false;std::deque<std::string> retained;
        const std::string ext=wc::platform::image_extension();
        // Read only our strictly numbered generated paths for bounded cross-restart retention.
        std::ifstream manifest(output+".files");std::string stored;
        while(std::getline(manifest,stored)) {
            if(stored.compare(0,output.size()+1,output+"-")!=0 || stored.size()<output.size()+2+ext.size()) continue;
            auto middle=stored.substr(output.size()+1,stored.size()-output.size()-1-ext.size());
            if(stored.substr(stored.size()-ext.size())==ext && middle.find_first_not_of("0123456789-")==std::string::npos) retained.push_back(stored);
        }
        manifest.close();
        auto nonce=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        unsigned serial=0;
        auto update=[&](std::time_t now,bool force){
#ifdef _WIN32
            if(wc::platform::config_changed()) {
                try {
                    auto candidate=wc::load_config(config_path);
                    std::unique_ptr<wc::Renderer> fresh(new wc::Renderer);fresh->validate(candidate);
                    if(!size.empty()){candidate.width=config.width;candidate.height=config.height;}
                    // Stage a render before replacing working state, including asset validation.
                    auto local=wc::local_time(now);auto scene=wc::select_scene(candidate,local);
                    fresh->render(scene,local,auto_size?config.width:candidate.width,auto_size?config.height:candidate.height);
                    if(candidate.fit!=config.fit) throw std::runtime_error("Changing fit requires restart");
                    config=std::move(candidate);renderer=std::move(fresh);force=true;
                    log.write("event=config_reloaded");
                } catch(const std::exception& e){log.write(std::string("event=config_reload_rejected keeping_previous=true error=")+e.what());}
            }
#endif
            auto start=Clock::now();auto local=wc::local_time(now);
            if(auto_size){auto s=wc::platform::screen_size();config.width=s.first;config.height=s.second;}
            auto scene=wc::select_scene(config,local);
            auto next=renderer->next_change(config,scene,now); // Validate deadlines before applying.
            auto image=renderer->render(scene,local,config.width,config.height);auto drawn=Clock::now();
            auto fingerprint=hash(image);
            if(!has_previous || fingerprint!=previous_hash || force) {
                std::string path=output+"-"+std::to_string(nonce)+"-"+std::to_string(serial++)+ext;
                std::string pending=output+"-pending"+ext;
                wc::write_image(image,pending);wc::platform::publish(pending,path);auto written=Clock::now();
                try {wc::platform::apply(path);} catch(...){wc::platform::remove_file(path);throw;}
                auto applied=Clock::now();retained.push_back(path);
                while(retained.size()>2){wc::platform::remove_file(retained.front());retained.pop_front();}
                std::ofstream files(output+".files",std::ios::trunc);for(const auto& p:retained) files<<p<<'\n';
                if(!files) log.write("event=warning retention_manifest_write_failed");
                std::ostringstream info;info<<std::fixed<<std::setprecision(2)<<"event=applied local="<<std::put_time(&local,"%Y-%m-%dT%H:%M:%S")
                    <<" hash="<<fingerprint<<" render_ms="<<millis(start,drawn)<<" write_hash_ms="<<millis(drawn,written)
                    <<" apply_ms="<<millis(written,applied)<<" total_ms="<<millis(start,applied)<<" next="<<next<<" file="<<path;
                log.write(info.str());previous_hash=fingerprint;has_previous=true;
            }
            return next;
        };
        if(mode=="--once") update(std::time(nullptr),true);
        else {
            wc::LoopHost host{[]{return std::time(nullptr);},wc::platform::wait_until,wc::platform::visible,[&](const std::string& m){log.write(m);}};
            host.monotonic_ms=[]{return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch()).count();};
            wc::run_loop(update,host,frames);
        }
        log.write("event=stopped");return 0;
    } catch(const wc::platform::AlreadyRunning& e) {
        Log(log_path).write(std::string("event=duplicate_engine error=")+e.what());return 2;
    } catch(const std::exception& e) {
        Log(log_path).write(std::string("event=fatal error=")+e.what());
        return 1;
    }
}

#include "wallclock/core.h"
#include "wallclock/renderer.h"
#include "wallclock/platform.h"
#include <cstdint>
#include <iostream>
#include <stdexcept>

namespace {
std::uint64_t hash(const wc::Image& image) {
    std::uint64_t value=14695981039346656037ULL;
    for(auto byte:image.pixels) {value^=byte;value*=1099511628211ULL;}
    return value;
}
void help() {
    std::cout<<"wallclock --config FILE (--render FILE.png | --once PREFIX | --run PREFIX)\n"
        "          [--size WIDTHxHEIGHT | --auto-size] [--at HH:MM]\n"
        "--render exports only; --once applies now; --run sleeps between updates.\n"
        "--at is only for previews. Output directories must already exist.\n"
        "Platform: "<<wc::platform::name()<<"\n";
}
}
int main(int argc,char** argv) {
    try {
        std::string config_path="examples/minimal.ini", mode,output,at,size;
        bool auto_size=false;
        for(int i=1;i<argc;++i) {
            std::string arg=argv[i];
            if(arg=="--help" || arg=="-h") {help();return 0;}
            if(arg=="--auto-size") {auto_size=true;continue;}
            if(arg!="--config" && arg!="--size" && arg!="--at" && arg!="--render" && arg!="--once" && arg!="--run") throw std::runtime_error("Unknown argument: "+arg);
            if(++i==argc) throw std::runtime_error("Missing value for "+arg);
            std::string value=argv[i];
            if(arg=="--config") config_path=value;
            else if(arg=="--size") size=value;
            else if(arg=="--at") at=value;
            else {if(!mode.empty()) throw std::runtime_error("Choose one output mode");mode=arg;output=value;}
        }
        if(mode.empty()) {help();return 1;}
        if(!at.empty() && mode!="--render") throw std::runtime_error("--at is only supported with --render");
        if(auto_size && !size.empty()) throw std::runtime_error("Choose --size or --auto-size");
        wc::Config config=wc::load_config(config_path);
        if(!size.empty()) {
            auto x=size.find('x'); size_t a=0,b=0;
            if(x==std::string::npos) throw std::runtime_error("Size must be WIDTHxHEIGHT");
            config.width=std::stoi(size.substr(0,x),&a);config.height=std::stoi(size.substr(x+1),&b);
            if(a!=x || b!=size.size()-x-1) throw std::runtime_error("Size must be WIDTHxHEIGHT");
            wc::validate_size(config.width,config.height);
        }
        wc::Renderer renderer;
        std::string previous_key;std::uint64_t previous_hash=0;bool has_previous=false;int slot=0;
        do {
            std::time_t now=std::time(nullptr);std::tm local=wc::local_time(now);
            if(mode=="--run" && !wc::platform::visible()) {
                wc::platform::wait_until(now+26*3600);
                continue;
            }
            if(!at.empty()) {int minute=wc::parse_time(at);local.tm_hour=minute/60;local.tm_min=minute%60;local.tm_sec=0;}
            if(auto_size) {auto screen=wc::platform::screen_size();config.width=screen.first;config.height=screen.second;}
            wc::Scene scene=wc::select_scene(config,local);
            std::string key=wc::visual_key(scene,local)+":"+std::to_string(config.width)+"x"+std::to_string(config.height);
            if(key!=previous_key) {
                wc::Image image=renderer.render(scene,local,config.width,config.height);
                if(mode=="--render") {wc::write_image(image,output);std::cout<<"Exported "<<output<<"\n";return 0;}
                std::uint64_t fingerprint=hash(image);
                if(!has_previous || fingerprint!=previous_hash) {
                    std::string path=output+"-"+std::to_string(slot)+wc::platform::image_extension();
                    std::string temp=output+"-pending"+wc::platform::image_extension();
                    wc::write_image(image,temp);wc::platform::publish(temp,path);wc::platform::apply(path);
                    std::cout<<"Applied "<<path<<std::endl;
                    slot=1-slot;previous_hash=fingerprint;has_previous=true;
                }
                previous_key=key;
            }
            if(mode=="--once") break;
            // Re-evaluate after expensive work; do not sleep past a boundary crossed during rendering.
            std::time_t deadline=wc::next_change(config,scene,now);
            if(deadline>std::time(nullptr)) wc::platform::wait_until(deadline);
        } while(mode=="--run");
        return 0;
    } catch(const std::exception& e) {std::cerr<<"wallclock: "<<e.what()<<"\n";return 1;}
}

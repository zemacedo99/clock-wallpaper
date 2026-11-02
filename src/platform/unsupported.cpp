#include "wallclock/platform.h"
#include <cstdio>
#include <stdexcept>
namespace wc { namespace platform {
bool visible(){return true;}
void configure(const std::string&){throw std::runtime_error("Platform supports export only");}
void remove_file(const std::string& path){std::remove(path.c_str());}
std::pair<int,int> screen_size(){throw std::runtime_error("Use explicit --size WIDTHxHEIGHT on this platform");}
void apply(const std::string&){throw std::runtime_error("Native wallpaper adapter not implemented on this platform; use --render to export");}
Wake wait_until(std::time_t){throw std::runtime_error("Native scheduling host not implemented on this platform");}
void publish(const std::string& from,const std::string& to){if(std::rename(from.c_str(),to.c_str())) throw std::runtime_error("Cannot publish wallpaper");}
const char* name(){return "Export only";}
const char* image_extension(){return ".png";}
}}

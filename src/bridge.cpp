#include "wallclock/bridge.h"
#include "wallclock/core.h"
#include "wallclock/renderer.h"
#include <memory>
#include <stdexcept>
struct wc_engine {wc::Config config;wc::Renderer renderer;};
// The host contract serializes all bridge calls, so no TLS runtime is needed.
namespace {std::string error;
void require(bool condition){if(!condition)throw std::runtime_error("Null engine or path");}
}
extern "C" {
wc_engine* wc_create(const char* path) {
    try {error.clear();require(path!=nullptr);std::unique_ptr<wc_engine> e(new wc_engine);e->config=wc::load_config(path);return e.release();}
    catch(const std::exception& e){error=e.what();return nullptr;}
}
void wc_destroy(wc_engine* engine){delete engine;}
const char* wc_error(void){return error.c_str();}
int wc_render(wc_engine* engine,int64_t now,int w,int h,const char* output) {
    try {
        error.clear();require(engine && output);auto time=wc::local_time(static_cast<std::time_t>(now));
        auto scene=wc::select_scene(engine->config,time);
        wc::write_image(engine->renderer.render(scene,time,w,h),output);return 1;
    } catch(const std::exception& e){error=e.what();return 0;}
}
int64_t wc_next_change(wc_engine* engine,int64_t now) {
    try {
        error.clear();require(engine!=nullptr);auto epoch=static_cast<std::time_t>(now);
        return wc::next_change(engine->config,wc::select_scene(engine->config,wc::local_time(epoch)),epoch);
    } catch(const std::exception& e){error=e.what();return -1;}
}
}

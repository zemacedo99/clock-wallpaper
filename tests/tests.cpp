#include "wallclock/core.h"
#include "wallclock/renderer.h"
#include "wallclock/bridge.h"
#include "wallclock/scheduler.h"
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <cstdio>

namespace {
int checks=0;
void check(bool condition,const char* message) {++checks;if(!condition) throw std::runtime_error(message);}
template<class F> void rejects(F fn,const char* message) {bool rejected=false;try{fn();}catch(const std::exception&){rejected=true;}check(rejected,message);}
std::tm at(int h,int m) {std::tm t={};t.tm_year=126;t.tm_mon=8;t.tm_mday=9;t.tm_hour=h;t.tm_min=m;t.tm_isdst=-1;return t;}
}
int main(int argc,char** argv) {
    try {
        check(argc==2,"Pass example config path");
        auto c=wc::load_config(argv[1]);
        check(wc::angle(at(0,0),"hour")==0,"midnight hour");
        check(wc::angle(at(12,0),"hour")==0,"noon hour");
        check(wc::angle(at(3,30),"hour")==105,"continuous hour within minute");
        check(wc::angle(at(3,30),"minute")==180,"minute angle");
        check(wc::angle(at(23,59),"hour")==359.5,"last minute hour");
        check(wc::angle(at(18,0),"day")==270,"24 hour orbit");
        rejects([]{wc::parse_time("24:00");},"reject invalid hour");
        rejects([]{wc::parse_time("09:60");},"reject invalid minute");
        rejects([]{wc::parse_time("9:00");},"reject ambiguous syntax");
        rejects([]{wc::validate_size(8192,8192);},"pixel budget");
        check(wc::select_scene(c,at(0,1)).theme.text=="Good night","wrap yesterday");
        check(wc::select_scene(c,at(17,59)).theme.text.empty(),"before office boundary");
        auto leave=wc::select_scene(c,at(18,0));
        check(leave.theme.text=="Leave office","exact office boundary");
        check(wc::select_scene(c,at(20,0)).theme.text=="Slow down","later rule resets text");
        auto time=at(17,59);time.tm_sec=42;auto epoch=std::mktime(&time);
        check(wc::next_change(c,wc::select_scene(c,time),epoch)==epoch+18,"next minute aligned");
        time=at(17,55);epoch=std::mktime(&time);auto slower=wc::select_scene(c,time);slower.theme.update_seconds=900;
        check(wc::next_change(c,slower,epoch)==epoch+300,"schedule interrupts slow cadence");
        slower.theme.update_seconds=0;
        check(wc::next_change(c,slower,epoch)==epoch+300,"static schedule sleeps to rule");
        auto later=time;later.tm_sec=55;
        check(wc::visual_key(slower,time)==wc::visual_key(slower,later),"static key stable");
        wc::Renderer renderer;
        auto normal=wc::select_scene(c,at(10,10));
        auto a=renderer.render(normal,at(10,10),320,180);
        auto b=renderer.render(normal,at(10,10),320,180);
        check(a.pixels==b.pixels,"deterministic rendering");
        auto d=renderer.render(normal,at(10,11),320,180);
        check(a.pixels!=d.pixels,"minute changes actual pixels");
        auto phone=renderer.render(leave,at(18,0),180,320);
        check(phone.width==180 && phone.height==320 && phone.pixels.size()==180*320*4,"portrait dimensions");
        auto with_text=renderer.render(leave,at(18,0),640,360);
        leave.theme.text.clear();auto without_text=renderer.render(leave,at(18,0),640,360);
        check(with_text.pixels!=without_text.pixels,"schedule text rendered");
        wc::Element plugin;plugin.kind="custom";normal.theme.elements.push_back(plugin);
        rejects([&]{renderer.render(normal,at(10,10),320,180);},"unknown renderer explicit error");
        renderer.register_element("custom",[](wc::Image& im,const wc::Element&,const wc::RenderContext&){im.blend(0,0,{255,0,0,255});});
        auto custom=renderer.render(normal,at(10,10),320,180);
        check(custom.pixels[0]==255 && custom.pixels[1]==0,"custom renderer interface");
        wc::write_image(phone,"test-roundtrip.png");
        auto roundtrip=wc::load_image("test-roundtrip.png");std::remove("test-roundtrip.png");
        check(roundtrip.pixels==phone.pixels,"PNG lossless roundtrip");
        wc::write_image(phone,"test-roundtrip.bmp");
        roundtrip=wc::load_image("test-roundtrip.bmp");std::remove("test-roundtrip.bmp");
        check(roundtrip.pixels==phone.pixels,"BMP lossless roundtrip");
        std::ofstream bad("test-invalid.ini");bad<<"[wallpaper]\ntheme = absent\n";bad.close();
        rejects([]{wc::load_config("test-invalid.ini");},"missing theme rejected");std::remove("test-invalid.ini");
        auto engine=wc_create(argv[1]);check(engine!=nullptr,"C ABI create");
        check(wc_next_change(engine,epoch)>epoch,"C ABI deadline");
        check(wc_render(engine,epoch,180,320,"test-bridge.png")==1,"C ABI render");std::remove("test-bridge.png");
        check(wc_render(engine,epoch,0,0,"test-bridge.png")==0,"C ABI catches errors");
        check(std::string(wc_error()).size()>0,"C ABI diagnostic");wc_destroy(engine);
        check(wc_create(nullptr)==nullptr,"C ABI null path");
        wc::Image tiny(8,8,{0,0,255,255});wc::write_image(tiny,"test-background.png");
        normal.theme.elements.clear();normal.theme.background="test-background.png";normal.theme.text.clear();
        auto photo=renderer.render(normal,at(10,10),320,180);std::remove("test-background.png");
        check(photo.pixels[0]==0 && photo.pixels[2]==255,"background image cover and small input");
        std::ofstream duplicate("test-invalid.ini");duplicate<<"[wallpaper]\nwidth=100\nwidth=200\n";duplicate.close();
        rejects([]{wc::load_config("test-invalid.ini");},"duplicate keys rejected");std::remove("test-invalid.ini");
        // Local CRT timezone only affects this test process. US DST dates in 2026.
#ifdef _WIN32
        _putenv("TZ=PST8PDT");_tzset();
#else
        setenv("TZ","PST8PDT",1);tzset();
#endif
        wc::Config dst=c;dst.rules.clear();
        wc::Rule before;before.minute=0;before.text="Before";before.has_text=true;
        wc::Rule skipped;skipped.minute=150;skipped.text="After";skipped.has_text=true;
        dst.rules={before,skipped};
        auto spring=at(1,59);spring.tm_mon=2;spring.tm_mday=8;spring.tm_sec=30;spring.tm_isdst=0;
        auto spring_epoch=std::mktime(&spring);auto spring_scene=wc::select_scene(dst,spring);spring_scene.theme.update_seconds=0;
        auto jump=wc::next_change(dst,spring_scene,spring_epoch);
        check(jump==spring_epoch+30,"DST skipped rule wakes at jump even for static theme");
        check(wc::select_scene(dst,wc::local_time(jump)).theme.text=="After","DST skipped rule selection");
        dst.rules[1].minute=90;
        auto fold=at(1,45);fold.tm_mon=10;fold.tm_mday=1;fold.tm_isdst=1;
        auto fold_epoch=std::mktime(&fold);auto fold_scene=wc::select_scene(dst,fold);fold_scene.theme.update_seconds=0;
        auto rollback=wc::next_change(dst,fold_scene,fold_epoch);
        check(rollback==fold_epoch+900,"DST rollback changes schedule at transition");
        auto second=wc::local_time(rollback);auto second_scene=wc::select_scene(dst,second);second_scene.theme.update_seconds=0;
        check(second_scene.theme.text=="Before","DST repeated hour selects earlier rule");
        check(wc::next_change(dst,second_scene,rollback)==rollback+1800,"DST second rule occurrence scheduled");
        // Preserve Python's useful scheduler tests, and assert actual retry delays.
        std::time_t fake=12000;int attempts=0;std::vector<std::time_t> called;std::vector<int> sleeps;std::vector<std::string> logs;
        wc::LoopHost host{[&]{return fake;},[&](std::time_t until){sleeps.push_back(int(until-fake));fake=until;return wc::Wake::deadline;},[]{return true;},[&](const std::string& s){logs.push_back(s);}};
        wc::run_loop([&](std::time_t n,bool){called.push_back(n);if(++attempts==2) throw std::runtime_error("temporary apply error");return n+60;},host,3);
        check(called==std::vector<std::time_t>({12000,12060,12120,12180}),"transient apply error retries after 60s, not Python's hour");
        check(logs.size()==2 && logs[1].find("recovered")!=std::string::npos,"failure and recovery logged");
        fake=20000;attempts=0;sleeps.clear();
        host.wait=[&](std::time_t until){sleeps.push_back(int(until-fake));fake=until;return ++attempts==5?wc::Wake::stop:wc::Wake::deadline;};
        wc::run_loop([](std::time_t,bool)->std::time_t{throw std::runtime_error("persistent");},host);
        check(sleeps==std::vector<int>({60,120,240,300,300}),"bounded failure backoff");
        fake=30000;attempts=0;called.clear();sleeps.clear();
        host.wait=[&](std::time_t until){sleeps.push_back(int(until-fake));fake=until;return wc::Wake::deadline;};
        wc::run_loop([&](std::time_t n,bool){called.push_back(n);if(++attempts==1) fake+=70;return n+60;},host,2);
        check(called==std::vector<std::time_t>({30000,30070}) && sleeps.empty(),"render crossing boundary catches up without skipping minute");
        bool shown=false;fake=40000;called.clear();
        host.visible=[&]{return shown;};host.wait=[&](std::time_t){fake+=7200;shown=true;return wc::Wake::visibility;};
        wc::run_loop([&](std::time_t n,bool){called.push_back(n);return n+60;},host,1);
        check(called==std::vector<std::time_t>({47200}),"hidden clock does no work and resumes with current time");
        host.visible=[]{return true;};fake=50000;called.clear();
        host.wait=[&](std::time_t){fake-=3600;return wc::Wake::environment;};
        wc::run_loop([&](std::time_t n,bool){called.push_back(n);return n+60;},host,2);
        check(called==std::vector<std::time_t>({50000,46400}),"backwards wall-clock correction forces current-time update");
        fake=60000;attempts=0;sleeps.clear();
        host.wait=[&](std::time_t until){sleeps.push_back(int(until-fake));if(++attempts==1){fake-=3600;return wc::Wake::environment;}fake=until;return wc::Wake::stop;};
        wc::run_loop([](std::time_t,bool)->std::time_t{throw std::runtime_error("transient during clock correction");},host);
        check(sleeps==std::vector<int>({60,60}),"backwards time change preserves bounded failure retry");
        for(int correction:{-3600,3600}) {
            fake=60000;attempts=0;sleeps.clear();long long monotonic=0;int updates=0;
            host.monotonic_ms=[&]{return monotonic;};
            host.wait=[&](std::time_t until){
                sleeps.push_back(int(until-fake));
                if(++attempts==1){fake+=correction;monotonic+=10000;return wc::Wake::environment;}
                monotonic+=(until-fake)*1000;fake=until;return wc::Wake::deadline;
            };
            wc::run_loop([&](std::time_t n,bool)->std::time_t{if(++updates==1)throw std::runtime_error("temporary");return n+60;},host,1);
            check(sleeps==std::vector<int>({60,50}) && monotonic==60000,"monotonic retry survives wall-clock jump without shortening or extending backoff");
            host.monotonic_ms={};
        }
        // Seven days of minute deadlines, including the autumn DST fold. This is
        // simulated scheduling, not a substitute for a real overnight Windows soak.
        auto week_start=at(12,0);week_start.tm_mon=9;week_start.tm_mday=31;
        fake=std::mktime(&week_start);auto first_tick=fake;unsigned ticks=0;bool aligned=true;
        host.wait=[&](std::time_t until){aligned=aligned && until>fake && until-fake<=60;fake=until;return wc::Wake::deadline;};
        wc::run_loop([&](std::time_t n,bool){++ticks;auto local=wc::local_time(n);return renderer.next_change(c,wc::select_scene(c,local),n);},host,10081);
        check(ticks==10081 && aligned && fake-first_tick==7*24*3600,"seven simulated days including DST retain minute alignment without drift");
        wc::Scene static_layers=wc::select_scene(c,at(12,0));static_layers.theme.elements.clear();
        wc::Element marker;marker.kind="markers";static_layers.theme.elements.push_back(marker);
        auto noon=at(12,0);auto noon_epoch=std::mktime(&noon);
        check(renderer.next_change(c,static_layers,noon_epoch)>noon_epoch+60,"static layers do not tick at default theme cadence");
        wc::Element sun;sun.kind="orbit";sun.source="span";sun.visible_from=360;sun.visible_until=1080;
        check(!wc::element_visible(sun,at(5,59)) && wc::element_visible(sun,at(6,0)) && !wc::element_visible(sun,at(18,0)),"sun inclusive appearance/exclusive disappearance");
        check(std::fmod(wc::element_angle(sun,at(12,0)),360)==0,"sun at top halfway through visible span");
        wc::Element moon=sun;moon.visible_from=1080;moon.visible_until=360;
        check(wc::element_visible(moon,at(23,59)) && wc::element_visible(moon,at(0,1)) && !wc::element_visible(moon,at(12,0)),"moon interval wraps midnight");
        auto night=at(22,0);auto night_epoch=std::mktime(&night);
        check(wc::element_next_change(sun,static_layers.theme,night_epoch)>night_epoch+60,"hidden sun still schedules tomorrow instead of returning never");
        wc::Element comet;comet.kind="comet";
        renderer.register_element("comet",[](wc::Image&,const wc::Element&,const wc::RenderContext&){},[](const wc::Element&,const wc::Theme&,std::time_t n){return n+120;});
        static_layers.theme.elements={comet};
        check(renderer.next_change(c,static_layers,noon_epoch)==noon_epoch+120,"custom draw plus next_change controls wake");
        renderer.register_element("comet",[](wc::Image&,const wc::Element&,const wc::RenderContext&){},[](const wc::Element&,const wc::Theme&,std::time_t n){return n;});
        rejects([&]{renderer.next_change(c,static_layers,noon_epoch);},"invalid custom deadline rejected instead of busy loop");
        wc::Image pointer(8,16,{255,0,0,255});wc::write_image(pointer,"test-pointer.png");
        wc::Scene sprite_scene;wc::Element hand;hand.kind="hand";hand.source="minute";hand.shape="image";hand.image="test-pointer.png";hand.length=.3;hand.pivot_y=1;
        sprite_scene.theme.elements={hand};
        auto up=renderer.render(sprite_scene,at(12,0),200,200),right=renderer.render(sprite_scene,at(12,15),200,200);
        check(up.pixels!=right.pixels,"hand image rotates around configured pivot");
        std::remove("test-pointer.png");
        check(renderer.render(sprite_scene,at(12,30),200,200).pixels!=up.pixels,"sprite remains cached between frames");
        std::cout<<"Passed "<<checks<<" checks\n";
        return 0;
    } catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}
}

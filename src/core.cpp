#include "wallclock/core.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>

namespace wc {
void validate_size(int w,int h) {
    if(w<64 || h<64 || w>8192 || h>8192 || 1LL*w*h>34000000)
        throw std::runtime_error("Resolution must be 64..8192 per side and at most 34 megapixels");
}
int parse_time(const std::string& s) {
    if(s.size()!=5 || s[2]!=':' || s[0]<'0' || s[0]>'9' || s[1]<'0' || s[1]>'9' ||
       s[3]<'0' || s[3]>'9' || s[4]<'0' || s[4]>'9') throw std::runtime_error("Expected HH:MM: "+s);
    int h=(s[0]-'0')*10+s[1]-'0', m=(s[3]-'0')*10+s[4]-'0';
    if(h>23 || m>59) throw std::runtime_error("Invalid time: "+s);
    return h*60+m;
}
std::tm local_time(std::time_t t) {
    std::tm* p=std::localtime(&t);
    if(!p) throw std::runtime_error("Local time conversion failed");
    return *p; // MVP is single-threaded; callers on native hosts must serialize this function.
}
double angle(const std::tm& t,const std::string& source) {
    if(source=="hour") return (t.tm_hour%12)*30.0+t.tm_min*.5;
    if(source=="minute") return t.tm_min*6.0;
    if(source=="day") return (t.tm_hour*60+t.tm_min)*.25;
    throw std::runtime_error("Unknown time source: "+source);
}
Scene select_scene(const Config& c,const std::tm& t) {
    Scene result;
    const Rule* rule=nullptr;
    // Daily cyclic rules: before the first entry, yesterday's final rule applies.
    if(!c.rules.empty()) {
        int index=static_cast<int>(c.rules.size())-1;
        for(int i=0;i<static_cast<int>(c.rules.size());++i)
            if(c.rules[i].minute<=t.tm_hour*60+t.tm_min) index=i;
        result.rule=index; rule=&c.rules[index];
    }
    result.theme=c.themes.at(rule && !rule->theme.empty()?rule->theme:c.default_theme);
    if(rule) {
        if(rule->has_background) result.theme.background=rule->background;
        if(rule->has_text) result.theme.text=rule->text;
        if(rule->has_color) result.theme.color=rule->color;
    }
    return result;
}
std::time_t next_change(const Config& c,const Scene& scene,std::time_t now) {
    // Midnight is also a local calendar/zone re-evaluation point for static themes.
    std::tm current=local_time(now);
    std::time_t best=now+26*3600;
    if(scene.theme.update_seconds>0) {
        int elapsed=current.tm_hour*3600+current.tm_min*60+current.tm_sec;
        best=now+scene.theme.update_seconds-elapsed%scene.theme.update_seconds;
    }
    for(int day=0;day<=2;++day) {
        std::tm date=current; date.tm_mday+=day; date.tm_hour=12; date.tm_min=0; date.tm_sec=0;
        date.tm_isdst=-1; std::mktime(&date);
        std::vector<int> times{0};
        for(const auto& rule:c.rules) times.push_back(rule.minute);
        for(int minute:times) for(int dst : {-1,0,1}) {
            std::tm candidate=date; candidate.tm_hour=minute/60; candidate.tm_min=minute%60;
            candidate.tm_sec=0; candidate.tm_isdst=dst;
            std::time_t epoch=std::mktime(&candidate);
            // Reject nonexistent spring-forward times and consider both fall-back occurrences.
            std::tm check=local_time(epoch);
            if(check.tm_year==date.tm_year && check.tm_yday==date.tm_yday &&
               check.tm_hour==minute/60 && check.tm_min==minute%60 && epoch>now)
                best=std::min(best,epoch);
        }
    }
    // A DST jump itself can change schedule selection (including a skipped rule).
    // Probe offsets while planning; this adds no timer wakeups or background polling.
    auto wall_seconds=[](const std::tm& t){return t.tm_hour*3600+t.tm_min*60+t.tm_sec;};
    auto discontinuity=[&](std::time_t epoch){
        int actual=(wall_seconds(local_time(epoch))-wall_seconds(current)+86400)%86400;
        return actual!=int((epoch-now)%86400);
    };
    for(std::time_t left=now;left<best;) {
        std::time_t right=std::min(best,left+3600);
        if(discontinuity(right)) {
            while(right-left>1) {
                std::time_t middle=left+(right-left)/2;
                if(discontinuity(middle)) right=middle;else left=middle;
            }
            best=right;break;
        }
        left=right;
    }
    return best;
}
std::string visual_key(const Scene& scene,const std::tm& t) {
    std::ostringstream out; out<<scene.rule<<':'<<scene.theme.name;
    if(scene.theme.update_seconds>0)
        out<<':'<<t.tm_year<<':'<<t.tm_yday<<':'<<(t.tm_hour*3600+t.tm_min*60+t.tm_sec)/scene.theme.update_seconds;
    return out.str();
}
bool element_visible(const Element& e,const std::tm& t) {
    if(e.visible_from<0) return true;
    int m=t.tm_hour*60+t.tm_min;
    return e.visible_from<e.visible_until ? m>=e.visible_from && m<e.visible_until
        : m>=e.visible_from || m<e.visible_until;
}
double element_angle(const Element& e,const std::tm& t) {
    if(e.source!="span") return angle(t,e.source)+e.offset;
    int duration=(e.visible_until-e.visible_from+1440)%1440;
    if(!duration) throw std::runtime_error("Span requires distinct visibility times");
    int elapsed=(t.tm_hour*60+t.tm_min-e.visible_from+1440)%1440;
    double arc=std::fmod(e.arc_end-e.arc_start+360,360);if(arc==0) arc=360;
    return e.arc_start+arc*elapsed/duration+e.offset;
}
std::time_t element_next_change(const Element& e,const Theme& theme,std::time_t now) {
    auto t=local_time(now);std::time_t best=0;
    if(e.visible_from>=0) {
        Config edges;Rule a,b;a.minute=e.visible_from;b.minute=e.visible_until;edges.rules={a,b};
        Scene static_scene;static_scene.theme.update_seconds=0;
        best=next_change(edges,static_scene,now);
    }
    bool moving=e.kind=="hand" || e.kind=="orbit";
    bool timed_text=e.kind=="text" && e.text.find('%')!=std::string::npos;
    if(element_visible(e,t) && (moving || timed_text || (e.kind!="markers" && e.kind!="text"))) {
        if(e.kind=="orbit" && e.radius==0 && !e.rotate) return best;
        int period=e.update_seconds<0?theme.update_seconds:e.update_seconds;
        if(period>0) {
            int elapsed=t.tm_hour*3600+t.tm_min*60+t.tm_sec;
            auto candidate=now+period-elapsed%period;
            best=best?std::min(best,candidate):candidate;
        }
    }
    return best;
}
}

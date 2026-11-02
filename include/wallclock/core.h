#pragma once
#include <ctime>
#include <map>
#include <string>
#include <vector>

namespace wc {
struct Color { unsigned char r=0, g=0, b=0, a=255; };
struct Element {
    std::string kind="hand", source="minute", image, text;
    Color color{40,45,48,255};
    double x=.5, y=.5, radius=.28, length=.23, width=.006, size=.012;
    double offset=0;
    int count=12;
    int update_seconds=-1, visible_from=-1, visible_until=-1;
    std::string shape="rounded";
    double pivot_x=.5, pivot_y=.5, tail=0, arc_start=270, arc_end=90, heading_offset=0;
    bool rotate=false;
};
struct Theme {
    std::string name, background, text;
    Color color{240,238,231,255}, text_color{55,60,60,255};
    int update_seconds=60; // 0: schedule-only; built-in moving elements require >=60.
    std::vector<Element> elements;
};
struct Rule {
    int minute=0;
    std::string theme, background, text;
    Color color;
    bool has_background=false, has_text=false, has_color=false;
};
struct Config {
    int width=1920, height=1080;
    std::string default_theme;
    std::map<std::string,Theme> themes;
    std::vector<Rule> rules;
    std::string fit="fill";
};
struct Scene { Theme theme; int rule=-1; };
Config load_config(const std::string& path);
int parse_time(const std::string& value);
std::tm local_time(std::time_t epoch);
double angle(const std::tm& time, const std::string& source);
Scene select_scene(const Config& config, const std::tm& time);
std::time_t next_change(const Config& config, const Scene& scene, std::time_t now);
std::string visual_key(const Scene& scene, const std::tm& time);
void validate_size(int width, int height);
bool element_visible(const Element&,const std::tm&);
double element_angle(const Element&,const std::tm&);
std::time_t element_next_change(const Element&,const Theme&,std::time_t);
}

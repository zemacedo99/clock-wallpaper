#include "wallclock/core.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <set>
#include <stdexcept>

namespace wc { namespace {
std::string trim(const std::string& s) {
    auto a=s.find_first_not_of(" \t\r\n");
    return a==std::string::npos?"":s.substr(a,s.find_last_not_of(" \t\r\n")-a+1);
}
double number(const std::string& s,double lo,double hi) {
    size_t end=0; double n=std::stod(s,&end);
    if(end!=s.size() || !std::isfinite(n) || n<lo || n>hi) throw std::runtime_error("Number out of range: "+s);
    return n;
}
int integer(const std::string& s,int lo,int hi) {
    double n=number(s,lo,hi);
    if(n!=std::floor(n)) throw std::runtime_error("Expected integer: "+s);
    return static_cast<int>(n);
}
Color color(const std::string& s) {
    if(s.size()!=7 || s[0]!='#' || s.find_first_not_of("0123456789abcdefABCDEF",1)!=std::string::npos)
        throw std::runtime_error("Expected #RRGGBB: "+s);
    unsigned long n=std::stoul(s.substr(1),nullptr,16);
    return Color{static_cast<unsigned char>(n>>16),static_cast<unsigned char>(n>>8),static_cast<unsigned char>(n),255};
}
std::string asset(const std::string& base,const std::string& value) {
    if(value.empty() || value[0]=='/' || value[0]=='\\' || (value.size()>1 && value[1]==':')) return value;
    return base+value;
}
}
Config load_config(const std::string& path) {
    std::ifstream in(path); if(!in) throw std::runtime_error("Cannot open config: "+path);
    auto slash=path.find_last_of("/\\");
    std::string base=slash==std::string::npos?"":path.substr(0,slash+1);
    Config c; std::string section="wallpaper",section_id="wallpaper",line;
    Theme* theme=nullptr; Element* element=nullptr; Rule* rule=nullptr;
    std::set<std::string> sections,keys;
    int line_no=0;
    try {
        while(std::getline(in,line)) {
            ++line_no; line=trim(line);
            if(line.empty() || line[0]==';' || line[0]=='#') continue;
            if(line[0]=='[') {
                if(line.back()!=']') throw std::runtime_error("Unclosed section");
                section_id=trim(line.substr(1,line.size()-2));
                if(!sections.insert(section_id).second) throw std::runtime_error("Duplicate section");
                auto split=section_id.find(' ');
                section=section_id.substr(0,split);
                std::string name=split==std::string::npos?"":trim(section_id.substr(split+1));
                theme=nullptr; element=nullptr; rule=nullptr;
                if(section=="wallpaper" && name.empty()) {}
                else if(section=="theme" && !name.empty()) {
                    theme=&c.themes[name]; theme->name=name;
                } else if(section=="element" && !name.empty()) {
                    // [element THEME.ID]; declaration order is paint order.
                    auto dot=name.find('.');
                    if(dot==std::string::npos || !c.themes.count(name.substr(0,dot)))
                        throw std::runtime_error("Element must follow its [theme NAME], named NAME.ID");
                    auto& elements=c.themes.at(name.substr(0,dot)).elements;
                    elements.emplace_back(); element=&elements.back();
                } else if(section=="rule" && !name.empty()) {
                    c.rules.emplace_back(); rule=&c.rules.back(); rule->minute=parse_time(name);
                } else throw std::runtime_error("Unknown section: "+section_id);
                continue;
            }
            auto eq=line.find('='); if(eq==std::string::npos) throw std::runtime_error("Expected key = value");
            std::string k=trim(line.substr(0,eq)), v=trim(line.substr(eq+1));
            if(!keys.insert(section_id+"/"+k).second) throw std::runtime_error("Duplicate key: "+k);
            bool known=true;
            if(section=="wallpaper") {
                if(k=="width") c.width=integer(v,64,8192);
                else if(k=="height") c.height=integer(v,64,8192);
                else if(k=="theme") c.default_theme=v;
                else known=false;
            } else if(theme) {
                if(k=="background") theme->background=asset(base,v);
                else if(k=="color") theme->color=color(v);
                else if(k=="text") theme->text=v;
                else if(k=="text_color") theme->text_color=color(v);
                else if(k=="update_seconds") {
                    theme->update_seconds=integer(v,0,86400);
                    if(theme->update_seconds && (theme->update_seconds<60 || theme->update_seconds%60))
                        throw std::runtime_error("Cadence must be 0 or a multiple of 60 seconds");
                } else known=false;
            } else if(rule) {
                if(k=="theme") rule->theme=v;
                else if(k=="background") { rule->background=asset(base,v); rule->has_background=true; }
                else if(k=="text") { rule->text=v; rule->has_text=true; }
                else if(k=="color") { rule->color=color(v); rule->has_color=true; }
                else known=false;
            } else if(element) {
                if(k=="kind") element->kind=v;
                else if(k=="source") element->source=v;
                else if(k=="color") element->color=color(v);
                else if(k=="image") element->image=asset(base,v);
                else if(k=="text") element->text=v;
                else if(k=="x") element->x=number(v,0,1);
                else if(k=="y") element->y=number(v,0,1);
                else if(k=="radius") element->radius=number(v,0,1);
                else if(k=="length") element->length=number(v,0,1);
                else if(k=="width") element->width=number(v,.0001,.5);
                else if(k=="size") element->size=number(v,.0001,.5);
                else if(k=="offset") element->offset=number(v,-360,360);
                else if(k=="count") element->count=integer(v,1,360);
                else known=false;
            } else known=false;
            if(!known) throw std::runtime_error("Unknown key: "+k);
        }
        validate_size(c.width,c.height);
        if(!c.themes.count(c.default_theme)) throw std::runtime_error("Default theme does not exist");
        for(const auto& pair:c.themes) for(const auto& e:pair.second.elements) {
            if(e.source!="hour" && e.source!="minute" && e.source!="day") throw std::runtime_error("Invalid time source");
            if((e.kind=="hand" || e.kind=="orbit") && pair.second.update_seconds==0)
                throw std::runtime_error("Moving elements need a nonzero cadence");
        }
        for(const auto& r:c.rules) if(!r.theme.empty() && !c.themes.count(r.theme)) throw std::runtime_error("Rule theme does not exist");
        std::sort(c.rules.begin(),c.rules.end(),[](const Rule& a,const Rule& b){return a.minute<b.minute;});
    } catch(const std::exception& e) {
        throw std::runtime_error(path+":"+std::to_string(line_no)+": "+e.what());
    }
    return c;
}
}

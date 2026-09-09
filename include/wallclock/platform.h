#pragma once
#include <ctime>
#include <string>
#include <utility>

namespace wc { namespace platform {
// Only this boundary may access device APIs. Paths are UTF-8, absolute preferred.
std::pair<int,int> screen_size();
void apply(const std::string& path);
void publish(const std::string& temporary,const std::string& destination);
// Hosts can suppress work while their wallpaper cannot be seen.
bool visible();
// Wait for the deadline or a native time/resume/display event. Never wake a sleeping device.
void wait_until(std::time_t deadline);
const char* name();
const char* image_extension();
}}

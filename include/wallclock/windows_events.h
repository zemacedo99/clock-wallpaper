#pragma once
#include <functional>
#include <string>
namespace wc { namespace platform {
void set_event_log(std::function<void(const std::string&)> logger);
void watch_config(const std::string& path);
bool config_changed();
}}

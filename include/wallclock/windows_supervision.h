#pragma once
#include <algorithm>
#include <string>
namespace wc { namespace supervision {
// CommandLineToArgvW/MS CRT quoting, including embedded quotes and trailing slashes.
inline std::wstring quote(const std::wstring& value) {
    std::wstring out=L"\"";unsigned slashes=0;
    for(wchar_t ch:value) {
        if(ch==L'\\'){++slashes;continue;}
        out.append(ch==L'"'?slashes*2+1:slashes,L'\\');slashes=0;out+=ch;
    }
    out.append(slashes*2,L'\\');return out+L'"';
}
inline unsigned restart_seconds(unsigned failures) {
    return std::min(900u,60u*(1u<<std::min(4u,failures?failures-1:0)));
}
inline unsigned failures_after_exit(unsigned failures,unsigned long long uptime_ms) {
    return uptime_ms>=600000?1:std::min(100u,failures+1);
}
inline bool restart_exit(unsigned long code){return code!=0 && code!=2;}
}}

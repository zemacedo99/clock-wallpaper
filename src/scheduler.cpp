#include "wallclock/scheduler.h"
#include <algorithm>
#include <stdexcept>
namespace wc {
int retry_seconds(unsigned failures) {
    return std::min(300,60*(1<<std::min(3u,failures?failures-1:0)));
}
void run_loop(const Update& update,const LoopHost& host,unsigned max_updates) {
    std::time_t due=0;bool force=true;unsigned failures=0,successes=0;
    long long retry_due=0;
    while(true) {
        auto now=host.clock();
        if(failures && host.monotonic_ms)
            due=now+std::max(0LL,(retry_due-host.monotonic_ms()+999)/1000);
        if(host.visible() && (force || now>=due)) {
            try {
                auto next=update(now,force);
                if(next<=now) throw std::runtime_error("Layer returned a non-future deadline");
                due=next;force=false;
                if(failures) host.log("event=recovered failures="+std::to_string(failures));
                failures=0;
                if(max_updates && ++successes>=max_updates) return;
            } catch(const std::exception& e) {
                failures=std::min(100u,failures+1);due=host.clock()+retry_seconds(failures);force=false;
                if(host.monotonic_ms) retry_due=host.monotonic_ms()+retry_seconds(failures)*1000LL;
                host.log("event=retry failures="+std::to_string(failures)+" delay_s="+std::to_string(retry_seconds(failures))+" error="+e.what());
            }
        }
        now=host.clock();bool visible=host.visible();
        if(visible && due<=now) continue; // Rendering crossed a boundary: catch up now.
        Wake reason=host.wait(visible?due:now+26*3600);
        if(reason==Wake::stop) return;
        if(reason==Wake::environment || reason==Wake::visibility) {
            if(!failures) force=true; // Native events cannot bypass retry backoff.
            else if(!host.monotonic_ms && host.clock()<now) {
                // A backwards clock correction must not turn a short retry into hours.
                due=host.clock()+std::min<std::time_t>(retry_seconds(failures),std::max<std::time_t>(1,due-now));
            }
        }
    }
}
}

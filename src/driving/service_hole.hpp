#pragma once
// A ported function reached a service member its caller left empty: the
// original runs code at that point (a call, a draw, an allocation), the port
// would skip it without a trace. service_hole() makes the skip visible: one
// stderr line per (function, member) the first time, and a count per site
// (service_hole_report). tools/decomp_empty_services.py lists the sites that
// skip without it.
#include <cstdio>
#include <map>
#include <mutex>
#include <utility>
namespace outrun::driving {
struct ServiceHoleLog {
    std::mutex lock;
    std::map<std::pair<const char*,const char*>,unsigned long> counts;   // (function, member) literals -> times reached
};
inline ServiceHoleLog& service_hole_log(){static ServiceHoleLog log;return log;}
inline void service_hole(const char* function,const char* member){
    auto& log=service_hole_log();
    std::lock_guard<std::mutex> guard(log.lock);
    if(log.counts[{function,member}]++==0)
        std::fprintf(stderr,"[service] hole: %s has no %s (the original runs code here: provide the service or route it to the translation)\n",function,member);
}
// One line per site reached, for the host's end-of-run report.
inline void service_hole_report(std::FILE* out){
    auto& log=service_hole_log();
    std::lock_guard<std::mutex> guard(log.lock);
    if(log.counts.empty())return;
    std::fprintf(out,"service holes:");
    for(const auto& [k,n]:log.counts)std::fprintf(out," %s/%s x%lu",k.first,k.second,n);
    std::fprintf(out,"\n");
}
}

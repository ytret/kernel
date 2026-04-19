#pragma once

#include <stddef.h>
#include <stdint.h>

#define CLOCKS_PER_SEC 1000

typedef uint64_t time_t;
typedef long int clock_t;

struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
#ifdef __TM_GMTOFF
    long __TM_GMTOFF;
#endif
#ifdef __TM_ZONE
    const char *__TM_ZONE;
#endif
};

time_t time(time_t *tloc);
double difftime(time_t time1, time_t time0);

time_t mktime(struct tm *tm);
struct tm *gmtime(const time_t *timep);
struct tm *localtime(const time_t *timep);

size_t strftime(char *s, size_t max, const char *restrict format,
                const struct tm *restrict tm);

clock_t clock(void);

#include <time.h>
#include <ytkernel/panic.h>

time_t time(time_t *tloc) {
    const time_t ret = 0;

    if (tloc) { *tloc = ret; }

    return ret;
}

double difftime(time_t time1, time_t time0) {
    PANIC("stub %s called", __func__);
}

time_t mktime(struct tm *tm) {
    PANIC("stub %s called", __func__);
}

struct tm *gmtime(const time_t *timep) {
    PANIC("stub %s called", __func__);
}

struct tm *localtime(const time_t *timep) {
    PANIC("stub %s called", __func__);
}

size_t strftime(char *s, size_t max, const char *restrict format,
                const struct tm *restrict tm) {
    PANIC("stub %s called", __func__);
}

clock_t clock(void) {
    PANIC("stub %s called", __func__);
}

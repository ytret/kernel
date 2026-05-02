#include <math.h>
#include <ytkernel/panic.h>

int abs(int j) {
    if (j >= 0) {
        return j;
    } else {
        return -j;
    }
}

double acos(double x) {
    (void)x;
    PANIC("stub %s called", __func__);
}

double asin(double x) {
    (void)x;
    PANIC("stub %s called", __func__);
}

double atan2(double y, double x) {
    (void)y;
    (void)x;
    PANIC("stub %s called", __func__);
}

double ceil(double x) {
    (void)x;
    PANIC("stub %s called", __func__);
}

double cos(double x) {
    (void)x;
    PANIC("stub %s called", __func__);
}

double cosh(double x) {
    (void)x;
    PANIC("stub %s called", __func__);
}

double exp(double x) {
    (void)x;
    PANIC("stub %s called", __func__);
}

double fabs(double x) {
    (void)x;
    PANIC("stub %s called", __func__);
}

double floor(double x) {
    (void)x;
    PANIC("stub %s called", __func__);
}

double fmod(double x, double y) {
    (void)x;
    (void)y;
    PANIC("stub %s called", __func__);
}

double frexp(double x, int *e) {
    (void)x;
    (void)e;
    PANIC("stub %s called", __func__);
}

double ldexp(double x, int e) {
    (void)x;
    (void)e;
    PANIC("stub %s called", __func__);
}

double log10(double x) {
    (void)x;
    PANIC("stub %s called", __func__);
}

double log2(double x) {
    (void)x;
    PANIC("stub %s called", __func__);
}

double log(double x) {
    (void)x;
    PANIC("stub %s called", __func__);
}

double pow(double x, double y) {
    (void)x;
    (void)y;
    PANIC("stub %s called", __func__);
}

double sin(double x) {
    (void)x;
    PANIC("stub %s called", __func__);
}

double sinh(double x) {
    (void)x;
    PANIC("stub %s called", __func__);
}

double sqrt(double x) {
    (void)x;
    PANIC("stub %s called", __func__);
}

double tan(double x) {
    (void)x;
    PANIC("stub %s called", __func__);
}

double tanh(double x) {
    (void)x;
    PANIC("stub %s called", __func__);
}

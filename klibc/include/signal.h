#pragma once

#define SIG_DFL ((void *)1)

#define SIGINT 1

typedef int sig_atomic_t;
typedef void (*sighandler_t)(int);

sighandler_t signal(int signum, sighandler_t handler);

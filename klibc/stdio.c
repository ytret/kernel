#include <stdio.h>
#include <ytkernel/panic.h>

FILE *stdin;
FILE *stderr;
FILE *stdout;

char *fgets(char *s, int size, FILE *restrict stream) {
    PANIC("stub %s called", __func__);
}

FILE *fopen(const char *restrict path, const char *restrict mode) {
    PANIC("stub %s called", __func__);
}

FILE *freopen(const char *restrict path, const char *restrict mode,
              FILE *restrict stream) {
    PANIC("stub %s called", __func__);
}

FILE *tmpfile(void) {
    PANIC("stub %s called", __func__);
}

int fclose(FILE *stream) {
    PANIC("stub %s called", __func__);
}

int feof(FILE *stream) {
    PANIC("stub %s called", __func__);
}

int ferror(FILE *stream) {
    PANIC("stub %s called", __func__);
}

int fflush(FILE *stream) {
    PANIC("stub %s called", __func__);
}

int fseek(FILE *stream, long offset, int whence) {
    PANIC("stub %s called", __func__);
}

int setvbuf(FILE *restrict stream, char *buf, int mode, size_t size) {
    PANIC("stub %s called", __func__);
}

long ftell(FILE *stream) {
    PANIC("stub %s called", __func__);
}

size_t fread(void *ptr, size_t size, size_t n, FILE *restrict stream) {
    PANIC("stub %s called", __func__);
}

size_t fwrite(const void *ptr, size_t size, size_t n, FILE *restrict stream) {
    PANIC("stub %s called", __func__);
}

void clearerr(FILE *stream) {
    PANIC("stub %s called", __func__);
}

int fprintf(FILE *restrict stream, const char *restrict format, ...) {
    PANIC("stub %s called", __func__);
}

int fputs(const char *restrict s, FILE *restrict stream) {
    PANIC("stub %s called", __func__);
}

int getc(FILE *stream) {
    PANIC("stub %s called", __func__);
}

int ungetc(int c, FILE *stream) {
    PANIC("stub %s called", __func__);
}

int system(const char *command) {
    PANIC("stub %s called", __func__);
}

int remove(const char *path) {
    PANIC("stub %s called", __func__);
}

int rename(const char *old, const char *new) {
    PANIC("stub %s called", __func__);
}

char *tmpnam(char *s) {
    PANIC("stub %s called", __func__);
}

int snprintf(char *str, size_t size, const char *restrict format, ...) {
    PANIC("stub %s called", __func__);
}

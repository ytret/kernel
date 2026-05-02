#include <stdio.h>
#include <ytkernel/kprintf.h>
#include <ytkernel/panic.h>

FILE *stdin = (FILE *)1;
FILE *stderr = (FILE *)2;
FILE *stdout = (FILE *)3;

char *fgets(char *s, int size, FILE *restrict stream) {
    (void)s;
    (void)size;
    (void)stream;
    PANIC("stub %s called", __func__);
}

FILE *fopen(const char *restrict path, const char *restrict mode) {
    (void)path;
    (void)mode;
    PANIC("stub %s called", __func__);
}

FILE *freopen(const char *restrict path, const char *restrict mode,
              FILE *restrict stream) {
    (void)path;
    (void)mode;
    (void)stream;
    PANIC("stub %s called", __func__);
}

FILE *tmpfile(void) {
    PANIC("stub %s called", __func__);
}

int fclose(FILE *stream) {
    (void)stream;
    PANIC("stub %s called", __func__);
}

int feof(FILE *stream) {
    (void)stream;
    PANIC("stub %s called", __func__);
}

int ferror(FILE *stream) {
    (void)stream;
    PANIC("stub %s called", __func__);
}

int fflush(FILE *stream) {
    if (stream == stdout || stream == stderr) {
        // Do nothing, the output is not buffered.
    } else {
        PANIC("fflush stream %p is not implemented", stream);
    }
}

int fseek(FILE *stream, long offset, int whence) {
    (void)stream;
    (void)offset;
    (void)whence;
    PANIC("stub %s called", __func__);
}

int setvbuf(FILE *restrict stream, char *buf, int mode, size_t size) {
    (void)stream;
    (void)buf;
    (void)mode;
    (void)size;
    PANIC("stub %s called", __func__);
}

long ftell(FILE *stream) {
    (void)stream;
    PANIC("stub %s called", __func__);
}

size_t fread(void *ptr, size_t size, size_t n, FILE *restrict stream) {
    (void)ptr;
    (void)size;
    (void)n;
    (void)stream;
    PANIC("stub %s called", __func__);
}

size_t fwrite(const void *ptr, size_t size, size_t n, FILE *restrict stream) {
    (void)size;
    (void)n;

    if (stream == stdout) {
        return kprintf("%s", (const char *)ptr);
    } else {
        PANIC("fwrite to stream %p is not implemented", stream);
    }
}

void clearerr(FILE *stream) {
    (void)stream;
    PANIC("stub %s called", __func__);
}

int fprintf(FILE *restrict stream, const char *restrict format, ...) {
    int ret;
    va_list ap;

    if (stream == stdout || stream == stderr) {
        va_start(ap, format);
        ret = kvprintf(format, ap);
        va_end(ap);
    } else {
        PANIC("fprintf to stream %p is not implemented", stream);
    }

    return ret;
}

int fputs(const char *restrict s, FILE *restrict stream) {
    (void)s;
    (void)stream;
    PANIC("stub %s called", __func__);
}

int getc(FILE *stream) {
    (void)stream;
    PANIC("stub %s called", __func__);
}

int ungetc(int c, FILE *stream) {
    (void)c;
    (void)stream;
    PANIC("stub %s called", __func__);
}

int system(const char *command) {
    (void)command;
    PANIC("stub %s called", __func__);
}

int remove(const char *path) {
    (void)path;
    PANIC("stub %s called", __func__);
}

int rename(const char *old, const char *new) {
    (void)old;
    (void)new;
    PANIC("stub %s called", __func__);
}

char *tmpnam(char *s) {
    (void)s;
    PANIC("stub %s called", __func__);
}

int snprintf(char *str, size_t size, const char *restrict format, ...) {
    int ret;
    va_list ap;

    va_start(ap, format);
    ret = kvsnprintf(str, size, format, ap);
    va_end(ap);

    return ret;
}

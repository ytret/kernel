#pragma once

#include <stddef.h>

#define BUFSIZ 8192
#define EOF    (-0xDEAD)

#define SEEK_SET 1
#define SEEK_CUR 2
#define SEEK_END 3

#define _IONBF 1
#define _IOLBF 2
#define _IOFBF 3

typedef void *FILE;

extern FILE *stdin;
extern FILE *stderr;
extern FILE *stdout;

int fflush(FILE *stream);
size_t fread(void *ptr, size_t size, size_t n, FILE *restrict stream);
size_t fwrite(const void *ptr, size_t size, size_t n, FILE *restrict stream);
int feof(FILE *stream);
FILE *fopen(const char *restrict path, const char *restrict mode);
FILE *freopen(const char *restrict path, const char *restrict mode,
              FILE *restrict stream);
int fprintf(FILE *restrict stream, const char *restrict format, ...);
int fclose(FILE *stream);
int ferror(FILE *stream);
char *fgets(char *s, int size, FILE *restrict stream);
int fseek(FILE *stream, long offset, int whence);
long ftell(FILE *stream);

FILE *tmpfile(void);

int snprintf(char *str, size_t size, const char *restrict format, ...);

int getc(FILE *stream);
int ungetc(int c, FILE *stream);

int setvbuf(FILE *restrict stream, char *buf, int mode, size_t size);

void clearerr(FILE *stream);

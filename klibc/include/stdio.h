#pragma once

#include <stddef.h>

#define BUFSIZ   8192
#define L_tmpnam 1024

#define EOF (-0xDEAD)

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

char *fgets(char *s, int size, FILE *restrict stream);
FILE *fopen(const char *restrict path, const char *restrict mode);
FILE *freopen(const char *restrict path, const char *restrict mode,
              FILE *restrict stream);
FILE *tmpfile(void);
int fclose(FILE *stream);
int feof(FILE *stream);
int ferror(FILE *stream);
int fflush(FILE *stream);
int fseek(FILE *stream, long offset, int whence);
int setvbuf(FILE *restrict stream, char *buf, int mode, size_t size);
long ftell(FILE *stream);
size_t fread(void *ptr, size_t size, size_t n, FILE *restrict stream);
size_t fwrite(const void *ptr, size_t size, size_t n, FILE *restrict stream);
void clearerr(FILE *stream);

int fprintf(FILE *restrict stream, const char *restrict format, ...);
int fputs(const char *restrict s, FILE *restrict stream);
int getc(FILE *stream);
int ungetc(int c, FILE *stream);

int system(const char *command);
int remove(const char *path);
int rename(const char *old, const char *new);
char *tmpnam(char *s);

int snprintf(char *str, size_t size, const char *restrict format, ...);

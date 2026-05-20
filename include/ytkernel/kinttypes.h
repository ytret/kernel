#pragma once

// NOTE: PRIX* macros are deliberately omitted, use PRIx* instead.

#ifdef __clang__
#define PRIu32  "u"
#define PRId32  "d"
#define PRIuPTR "u"
#define PRIdPTR "u"
#define PRIdOFF PRIdPTR
#define PRIx32  "x"
#define PRIxPTR "x"
#else
#define PRIu32  "lu"
#define PRId32  "ld"
#define PRIuPTR "lu"
#define PRIdPTR "ld"
#define PRIdOFF PRIdPTR
#define PRIx32  "lx"
#define PRIxPTR "lx"
#endif

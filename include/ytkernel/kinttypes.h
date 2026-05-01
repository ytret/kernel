#pragma once

// NOTE: PRIX* macros are deliberately omitted, use PRIx* instead.

#ifdef __clang__
#define PRIu32  "u"
#define PRIuPTR "u"
#define PRIx32  "x"
#define PRIxPTR "x"
#else
#define PRIu32  "lu"
#define PRIuPTR "lu"
#define PRIx32  "lx"
#define PRIxPTR "lx"
#endif

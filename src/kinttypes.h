#pragma once

// NOTE: PRIX* macros are deliberately omitted, use PRIx* instead.

#ifdef __clang__
#define PRIu32  "u"
#define PRIx32  "x"
#define PRIxPTR "x"
#else
#define PRIu32  "lu"
#define PRIx32  "lx"
#define PRIxPTR "lx"
#endif

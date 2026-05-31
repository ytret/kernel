#pragma once

// NOTE: PRIX* macros are deliberately omitted, use PRIx* instead.

#include "types.h"

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

#if VADDR_MAX == UINT32_MAX
#define PRIxVA  PRIx32
#define PRIx0VA "08" PRIx32
#else
#error "TODO"
#endif

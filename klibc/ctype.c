/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <ctype.h>
#include <ytkernel/panic.h>

#define _U 01
#define _L 02
#define _N 04
#define _S 010
#define _P 020
#define _C 040
#define _X 0100
#define _B 0200

static char g_ctype[] = {
    _C,      _C,      _C,      _C,      _C,      _C,      _C,      _C,
    _C,      _C | _S, _C | _S, _C | _S, _C | _S, _C | _S, _C,      _C,
    _C,      _C,      _C,      _C,      _C,      _C,      _C,      _C,
    _C,      _C,      _C,      _C,      _C,      _C,      _C,      _C,
    _S | _B, _P,      _P,      _P,      _P,      _P,      _P,      _P,
    _P,      _P,      _P,      _P,      _P,      _P,      _P,      _P,
    _N,      _N,      _N,      _N,      _N,      _N,      _N,      _N,
    _N,      _N,      _P,      _P,      _P,      _P,      _P,      _P,
    _P,      _U | _X, _U | _X, _U | _X, _U | _X, _U | _X, _U | _X, _U,
    _U,      _U,      _U,      _U,      _U,      _U,      _U,      _U,
    _U,      _U,      _U,      _U,      _U,      _U,      _U,      _U,
    _U,      _U,      _U,      _P,      _P,      _P,      _P,      _P,
    _P,      _L | _X, _L | _X, _L | _X, _L | _X, _L | _X, _L | _X, _L,
    _L,      _L,      _L,      _L,      _L,      _L,      _L,      _L,
    _L,      _L,      _L,      _L,      _L,      _L,      _L,      _L,
    _L,      _L,      _L,      _P,      _P,      _P,      _P,      _C,
    0,       0,       0,       0,       0,       0,       0,       0,
    0,       0,       0,       0,       0,       0,       0,       0,
    0,       0,       0,       0,       0,       0,       0,       0,
    0,       0,       0,       0,       0,       0,       0,       0,
    0,       0,       0,       0,       0,       0,       0,       0,
    0,       0,       0,       0,       0,       0,       0,       0,
    0,       0,       0,       0,       0,       0,       0,       0,
    0,       0,       0,       0,       0,       0,       0,       0,
    0,       0,       0,       0,       0,       0,       0,       0,
    0,       0,       0,       0,       0,       0,       0,       0,
    0,       0,       0,       0,       0,       0,       0,       0,
    0,       0,       0,       0,       0,       0,       0,       0,
    0,       0,       0,       0,       0,       0,       0,       0,
    0,       0,       0,       0,       0,       0,       0,       0,
    0,       0,       0,       0,       0,       0,       0,       0,
    0,       0,       0,       0,       0,       0,       0,       0,
};
static_assert(sizeof(g_ctype) == 256);

int isalnum(int c) {
    return g_ctype[c] & (_U | _L | _N);
}

int isalpha(int c) {
    return g_ctype[c] & (_U | _L);
}

int iscntrl(int c) {
    return g_ctype[c] & _C;
}

int isdigit(int c) {
    return g_ctype[c] & _N;
}

int isgraph(int c) {
    PANIC("stub %s called", __func__);
}

int islower(int c) {
    return (g_ctype[c] & (_U | _L)) == _L;
}

int ispunct(int c) {
    PANIC("stub %s called", __func__);
}

int isspace(int c) {
    PANIC("stub %s called", __func__);
}

int isupper(int c) {
    return (g_ctype[c] & (_U | _L)) == _L;
}

int isxdigit(int c) {
    PANIC("stub %s called", __func__);
}

int toupper(int c) {
    PANIC("stub %s called", __func__);
}

int tolower(int c) {
    PANIC("stub %s called", __func__);
}

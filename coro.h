/*
 * Copyright (c) 2001 Marc Alexander Lehmann <pcg@goof.com>
 * 
 * Redistribution and use in source and binary forms, with or without modifica-
 * tion, are permitted provided that the following conditions are met:
 * 
 *   1.  Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 * 
 *   2.  Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 * 
 *   3.  The name of the author may not be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MER-
 * CHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPE-
 * CIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTH-
 * ERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * This library is modelled strictly after Ralf S. Engelschalls article at
 * http://www.gnu.org/software/pth/rse-pmt.ps.  So most of the credit must
 * go to Ralf S. Engelschall <rse@engelschall.com>.
 *
 * This coroutine library is very much stripped down. You should either
 * build your own process avstraction using it or - better - just use GNU
 * Portable Threads, http://www.gnu.org/software/pth/.
 */

#ifndef CORO_H
#define CORO_H

typedef void (*coro_func)(void *);

#if defined(WINDOWS)
# define CORO_LOOSE 1 /* you don't win with windoze */
#elif defined(HAVE_UCONTEXT_H)
# define CORO_UCONTEXT 1
#elif defined(HAVE_SETJMP_H) && defined(HAVE_SIGALTSTACK)
# define CORO_SJLJ 1
#else
error unknown or unsupported architecture
#endif

#if CORO_UCONTEXT

#include <ucontext.h>

typedef struct {
  ucontext_t uc;
} coro_context;

#define coro_transfer(p,n) swapcontext(&((p)->uc), &((n)->uc))

#elif CORO_SJLJ || CORO_LOOSE

#include <setjmp.h>

typedef struct {
  jmp_buf env;
} coro_context;

#define coro_transfer(p,n) if (!setjmp ((p)->env)) longjmp ((n)->env, 1)

#endif

void coro_create(coro_context *ctx,
                 coro_func coro, void *arg,
                 void *sptr, long ssize);

#endif


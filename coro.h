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
 *
 * VERSION: 0.1
 */

#ifndef CORO_H
#define CORO_H

/*
 * This library consists of only three files
 * coro.h, coro.c and LICENSE
 *
 * It implements what is known as coroutines, in a hopefully
 * portable way. At the moment you have to define which kind
 * of implementation flavour you want:
 *
 * -DCORO_UCONTEXT
 *
 *    This flavour uses SUSv2's get/set/swap/makecontext functions that
 *    unfortunately only newer unices support.
 *    Use this for GNU/Linux + glibc-2.2.3.
 *
 * -DCORO_SJLJ
 *
 *    This flavour uses SUSv'2 setjmp/longjmp and sigaltstack functions to
 *    do it's job. Coroutine creation is much slower than UCONTEXT, but
 *    context switching is often a bit cheaper. It should work on almost
 *    all unices. Use this for GNU/Linux + glibc-2.2. glibc-2.1 and below
 *    do not work with any sane model (neither sigaltstack nor context
 *    functions are implemented)
 *
 * -DCORO_LINUX
 *
 *    Old GNU/Linux systems (<= glibc-2.1) work with this implementation
 *    (very fast).
 *
 * -DCORO_LOOSE
 *
 *    Microsoft's highly proprietary platform doesn't support sigaltstack, and
 *    this automatically selects a suitable workaround for this platform.
 *    (untested)
 *
 * -DCORO_IRIX
 *
 *    SGI's version of Microsoft's NT ;)
 *
 * If you define neither of these symbols, coro.h will try to autodetect
 * the model.  This currently works for CORO_LOOSE only. For the other
 * alternatives you should check (e.g. using autoconf) and define the
 * following symbols: HAVE_UCONTEXT_H / HAVE_SETJMP_H / HAVE_SIGALTSTACK.
 */

/*
 * This is the type for the initialization function of a new coroutine.
 */
typedef void (*coro_func)(void *);

/*
 * A coroutine state is saved in the following structure. Treat it as a
 * opaque type. errno and sigmask might be saved, but don't rely on it,
 * implement your own switching primitive.
 */
typedef struct coro_context coro_context;

/*
 * This function creates a new coroutine. Apart from a pointer to an
 * uninitialized coro_context, it expects a pointer to the entry function
 * and the single pointer value that is given to it as argument.
 *
 * Allocating/deallocating the stack is your own responsibility, so there is
 * no coro_destroy function.
 */
void coro_create(coro_context *ctx,
                 coro_func coro, void *arg,
                 void *sptr, long ssize);

/*
 * The following prototype defines the coroutine switching function. It is
 * usually implemented as a macro, so watch out.
 *
void coro_transfer(coro_context *prev, coro_context *next);
 */

/*
 * That was it. No other user-visible functions are implemented here.
 */

/*****************************************************************************/

#if !defined(CORO_LOOSE) && !defined(CORO_UCONTEXT) \
    && !defined(CORO_SJLJ) && !defined(CORO_LINUX) \
    && !defined(CORO_IRIX)
# if defined(WINDOWS)
#  define CORO_LOOSE 1 /* you don't win with windoze */
# elif defined(__linux) && defined(__x86)
# elif defined(HAVE_UCONTEXT_H)
#  define CORO_UCONTEXT 1
# elif defined(HAVE_SETJMP_H) && defined(HAVE_SIGALTSTACK)
#  define CORO_SJLJ 1
# else
error unknown or unsupported architecture
# endif
#endif

/*****************************************************************************/

#if CORO_UCONTEXT

#include <ucontext.h>

struct coro_context {
  ucontext_t uc;
};

#define coro_transfer(p,n) swapcontext(&((p)->uc), &((n)->uc))

#elif CORO_SJLJ || CORO_LOOSE || CORO_LINUX || CORO_IRIX

#include <setjmp.h>

struct coro_context {
  jmp_buf env;
};

#define coro_transfer(p,n) if (!setjmp ((p)->env)) longjmp ((n)->env, 1)

#endif

#endif


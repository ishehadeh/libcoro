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
 */

#include "coro.h"

#if CORO_LOOSE || CORO_SJLJ || CORO_LINUX

#include <signal.h>

static volatile coro_func coro_init_func;
static volatile void *coro_init_arg;
static volatile coro_context *new_coro, *create_coro;

static void
coro_init (void)
{
  volatile coro_func func = coro_init_func;
  volatile void *arg = coro_init_arg;

  coro_transfer ((coro_context *)new_coro, (coro_context *)create_coro);

  func ((void *)arg);

  /* the new coro returned. bad. just abort() for now */
  abort ();
}

# if CORO_SJLJ
# define coro_save(ctx)     (void)setjmp((ctx)->env)

static volatile int trampoline_count;

/* trampoline signal handler */
static void
trampoline(int sig)
{
  if (setjmp (((coro_context *)new_coro)->env))
    coro_init (); /* start it */
  else
    trampoline_count++;
}

# endif

#elif CORO_UCONTEXT
# define coro_save(ctx)     getcontext(&((ctx)->uc))
#endif

/* initialize a machine state */
void coro_create(coro_context *ctx,
                 coro_func coro, void *arg,
                 void *sptr, long ssize)
{
#if CORO_UCONTEXT

  getcontext (&(ctx->uc));

  ctx->uc.uc_link          =  0;
  ctx->uc.uc_stack.ss_sp    = sptr;
  ctx->uc.uc_stack.ss_size  = (size_t) ssize;
  ctx->uc.uc_stack.ss_flags = 0;

  makecontext (&(ctx->uc), (void (*)()) coro, 1, arg);

#elif CORO_SJLJ || CORO_LOOSE || CORO_LINUX

# if CORO_SJLJ
  stack_t ostk, nstk;
  struct sigaction osa, nsa;
  sigset_t nsig, osig;
# endif
  coro_context nctx;

  coro_init_func = coro;
  coro_init_arg  = arg;

  new_coro    = ctx;
  create_coro = &nctx;

# if CORO_SJLJ
  /* we use SIGUSR2. first block it, then fiddle with it. */

  sigemptyset (&nsig);
  sigaddset (&nsig, SIGUSR2);
  sigprocmask (SIG_BLOCK, &nsig, &osig);

  nsa.sa_handler = trampoline;
  sigemptyset (&nsa.sa_mask);
  nsa.sa_flags = SA_ONSTACK;

  if (sigaction (SIGUSR2, &nsa, &osa))
    perror ("sigaction");

  /* set the new stack */
  nstk.ss_sp    = sptr;
  nstk.ss_size  = ssize;
  nstk.ss_flags = 0;

  if (sigaltstack (&nstk, &ostk) < 0)
    perror ("sigaltstack");

  trampoline_count = 0;
  kill (getpid (), SIGUSR2);
  sigfillset (&nsig); sigdelset (&nsig, SIGUSR2);

  while (!trampoline_count)
    sigsuspend (&nsig);

  sigaltstack (0, &nstk);
  nstk.ss_flags = SS_DISABLE;
  if (sigaltstack (&nstk, 0) < 0)
    perror ("sigaltstack");

  sigaltstack (0, &nstk);
  if (~nstk.ss_flags & SS_DISABLE)
    abort ();

  if (~ostk.ss_flags & SS_DISABLE)
    sigaltstack (&ostk, 0);

  sigaction (SIGUSR1, &osa, 0);

  sigprocmask (SIG_SETMASK, &osig, 0);

# elif CORO_LOOSE

  setjmp (ctx->env);
  ctx->env[7] = (int)((char *)sptr + ssize);
  ctx->env[8] = (int)coro_init;

# elif CORO_LINUX

  setjmp (ctx->env);
#if defined(__GLIBC__) && defined(__GLIBC_MINOR__) \
    && __GLIBC__ >= 2 && __GLIBC_MINOR__ >= 0 && defined(JB_PC) && defined(JB_SP)
  ctx->env[0].__jmpbuf[JB_PC] = (int)coro_init;
  ctx->env[0].__jmpbuf[JB_SP] = (int)((char *)sptr + ssize);
#elif defined(__GLIBC__) && defined(__GLIBC_MINOR__) \
    && __GLIBC__ >= 2 && __GLIBC_MINOR__ >= 0 && defined(__mc68000__)
  ctx->env[0].__jmpbuf[0].__aregs[0] = (long int)coro_init;
  ctx->env[0].__jmpbuf[0].__sp = (int *)((char *)sptr + ssize);
#elif defined(__GNU_LIBRARY__) && defined(__i386__)
  ctx->env[0].__jmpbuf[0].__pc = (char *)coro_init;
  ctx->env[0].__jmpbuf[0].__sp = (void *)((char *)sptr + ssize);
#else
#error "Unsupported Linux (g)libc version and/or platform"
#endif

# endif

  coro_transfer ((coro_context *)create_coro, (coro_context *)new_coro);

#else
error unsupported architecture
#endif
}


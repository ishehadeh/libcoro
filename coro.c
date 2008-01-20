/*
 * Copyright (c) 2001-2008 Marc Alexander Lehmann <schmorp@schmorp.de>
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

#if !defined(STACK_ADJUST_PTR)
/* IRIX is decidedly NON-unix */
# if __sgi
#  define STACK_ADJUST_PTR(sp,ss) ((char *)(sp) + (ss) - 8)
#  define STACK_ADJUST_SIZE(sp,ss) ((ss) - 8)
# elif __i386__ && CORO_LINUX
#  define STACK_ADJUST_PTR(sp,ss) ((char *)(sp) + (ss))
#  define STACK_ADJUST_SIZE(sp,ss) (ss)
# elif __amd64__ && CORO_LINUX
#  define STACK_ADJUST_PTR(sp,ss) ((char *)(sp) + (ss) - 8)
#  define STACK_ADJUST_SIZE(sp,ss) (ss)
# else
#  define STACK_ADJUST_PTR(sp,ss) (sp)
#  define STACK_ADJUST_SIZE(sp,ss) (ss)
# endif
#endif

#if CORO_UCONTEXT
# include <stddef.h>
#endif

#if CORO_SJLJ || CORO_LOSER || CORO_LINUX || CORO_IRIX || CORO_ASM

#include <stdlib.h>

#if CORO_SJLJ
# include <stdio.h>
# include <signal.h>
# include <unistd.h>
#endif

static volatile coro_func coro_init_func;
static volatile void *coro_init_arg;
static volatile coro_context *new_coro, *create_coro;

/* what we really want to detect here is wether we use a new-enough version of GAS */
/* instead, check for gcc 3, ELF and GNU/Linux and hope for the best */
#if __GNUC__ >= 3 && __ELF__ && __linux__
# define HAVE_CFI 1
#endif

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

static volatile int trampoline_count;

/* trampoline signal handler */
static void
trampoline (int sig)
{
  if (setjmp (((coro_context *)new_coro)->env))
    {
#if HAVE_CFI
      asm (".cfi_startproc");
#endif
      coro_init (); /* start it */
#if HAVE_CFI
      asm (".cfi_endproc");
#endif
    }
  else
    trampoline_count++;
}

# endif

#endif

#if CORO_ASM
void __attribute__((__noinline__, __fastcall__))
coro_transfer (struct coro_context *prev, struct coro_context *next)
{
  asm volatile (
#if __amd64
# define NUM_CLOBBERED 5
     "push %%rbx\n\t"
     "push %%r12\n\t"
     "push %%r13\n\t"
     "push %%r14\n\t"
     "push %%r15\n\t"
     "mov  %%rsp, %0\n\t"
     "mov  %1, %%rsp\n\t"
     "pop  %%r15\n\t"
     "pop  %%r14\n\t"
     "pop  %%r13\n\t"
     "pop  %%r12\n\t"
     "pop  %%rbx\n\t"
#elif __i386
# define NUM_CLOBBERED 4
     "push %%ebx\n\t"
     "push %%esi\n\t"
     "push %%edi\n\t"
     "push %%ebp\n\t"
     "mov  %%esp, %0\n\t"
     "mov  %1, %%esp\n\t"
     "pop  %%ebp\n\t"
     "pop  %%edi\n\t"
     "pop  %%esi\n\t"
     "pop  %%ebx\n\t"
#else
# error unsupported architecture
#endif
     : "=m" (prev->sp)
     : "m" (next->sp)
  );
}
#endif

/* initialize a machine state */
void coro_create (coro_context *ctx,
                  coro_func coro, void *arg,
                  void *sptr, long ssize)
{
#if CORO_UCONTEXT

  getcontext (&(ctx->uc));

  ctx->uc.uc_link           =  0;
  ctx->uc.uc_stack.ss_sp    = STACK_ADJUST_PTR (sptr,ssize);
  ctx->uc.uc_stack.ss_size  = (size_t) STACK_ADJUST_SIZE (sptr,ssize);
  ctx->uc.uc_stack.ss_flags = 0;

  makecontext (&(ctx->uc), (void (*)()) coro, 1, arg);

#elif CORO_SJLJ || CORO_LOSER || CORO_LINUX || CORO_IRIX || CORO_ASM

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
    {
      perror ("sigaction");
      abort ();
    }

  /* set the new stack */
  nstk.ss_sp    = STACK_ADJUST_PTR (sptr,ssize); /* yes, some platforms (IRIX) get this wrong. */
  nstk.ss_size  = STACK_ADJUST_SIZE (sptr,ssize);
  nstk.ss_flags = 0;

  if (sigaltstack (&nstk, &ostk) < 0)
    {
      perror ("sigaltstack");
      abort ();
    }

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

  sigaction (SIGUSR2, &osa, 0);

  sigprocmask (SIG_SETMASK, &osig, 0);

# elif CORO_LOSER

  setjmp (ctx->env);
#if __CYGWIN__
  ctx->env[7] = (long)((char *)sptr + ssize);
  ctx->env[8] = (long)coro_init;
#elif defined(_M_IX86)
  ((_JUMP_BUFFER *)&ctx->env)->Eip   = (long)coro_init;
  ((_JUMP_BUFFER *)&ctx->env)->Esp   = (long)STACK_ADJUST_PTR (sptr,ssize);
#elif defined(_M_AMD64)
  ((_JUMP_BUFFER *)&ctx->env)->Rip   = (__int64)coro_init;
  ((_JUMP_BUFFER *)&ctx->env)->Rsp   = (__int64)STACK_ADJUST_PTR (sptr,ssize);
#elif defined(_M_IA64)
  ((_JUMP_BUFFER *)&ctx->env)->StIIP = (__int64)coro_init;
  ((_JUMP_BUFFER *)&ctx->env)->IntSp = (__int64)STACK_ADJUST_PTR (sptr,ssize);
#else
#error "microsoft libc or architecture not supported"
#endif

# elif CORO_LINUX

  _setjmp (ctx->env);
#if __GLIBC__ >= 2 && __GLIBC_MINOR__ >= 0 && defined (JB_PC) && defined (JB_SP)
  ctx->env[0].__jmpbuf[JB_PC] = (long)coro_init;
  ctx->env[0].__jmpbuf[JB_SP] = (long)STACK_ADJUST_PTR (sptr, ssize);
#elif __GLIBC__ >= 2 && __GLIBC_MINOR__ >= 0 && defined (__mc68000__)
  ctx->env[0].__jmpbuf[0].__aregs[0] = (long int)coro_init;
  ctx->env[0].__jmpbuf[0].__sp = (int *)((char *)sptr + ssize);
#elif defined (__GNU_LIBRARY__) && defined (__i386__)
  ctx->env[0].__jmpbuf[0].__pc = (char *)coro_init;
  ctx->env[0].__jmpbuf[0].__sp = (void *)((char *)sptr + ssize);
#elif defined (__GNU_LIBRARY__) && defined (__amd64__)
  ctx->env[0].__jmpbuf[JB_PC]  = (long)coro_init;
  ctx->env[0].__jmpbuf[JB_RSP] = (long)STACK_ADJUST_PTR (sptr, ssize);
#else
# error "linux libc or architecture not supported"
#endif

# elif CORO_IRIX

  setjmp (ctx->env);
  ctx->env[JB_PC] = (__uint64_t)coro_init;
  ctx->env[JB_SP] = (__uint64_t)STACK_ADJUST_PTR (sptr, ssize);

# elif CORO_ASM

  ctx->sp = (volatile void **)(ssize + (char *)sptr);
  *--ctx->sp = (void *)coro_init;
  *--ctx->sp = (void *)coro_init; // this is needed when the prologue saves ebp
  ctx->sp -= NUM_CLOBBERED;

# endif

  coro_transfer ((coro_context *)create_coro, (coro_context *)new_coro);

#else
# error unsupported architecture
#endif
}


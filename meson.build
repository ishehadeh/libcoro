project('libcoro', 'c',
  version : '3.0.0',
  default_options : [ 'warning_level=2' ])

cc = meson.get_compiler('c')
arch = build_machine.cpu_family()
os = build_machine.system()

linux = 0
irix = 0
loser = 0
ucontext = 0
setjmp = 0
pthread = 0
fiber = 0
asm = 0

valgrind = get_option('valgrind') ? 1 : 0
guardpages = get_option('guardpages')
stackalloc = get_option('stackalloc') ? 1 : 0
backend = get_option('coro_backend')

# checks if the standard library is glibc, and if so if it is newer than 2.1
check_glibc = '''
#include <stdlib.h>

int main(void) {
  #if defined __GLIBC__ && (__GLIBC__ < 2 || __GLIBC_MINOR__ <= 1)
    return 1;
  #else
    return 0;
  #endif
}
'''

old_gnu_linux = false

if os == 'linux'
  check_glibc_output =  cc.run(check_glibc, name : 'check for pre glibc-2.1 stdlib')
  old_gnu_linux = check_glibc_output.compiled() and check_glibc_output.returncode() == 1
endif


threads_dep = dependency('threads', required : false)

if backend == 'auto'
  if (os == 'windows' or os == 'linux') and arch.startswith('x86')
    asm = 1
  elif os == 'windows'
    # an alternative here would also be fibers,
    # but since their portablility is more limited we'll go with the safer option
    loser = 1
  elif old_gnu_linux
    linux = 1
  elif check_header('setjmp.h')
    setjmp = 1
  elif check_header('ucontext.h')
    ucontext = 1
  elif threads_dep.found()
    pthread = 1
  else
    error('failed to autodetect a backend, please specify one')
  endif
elif backend == 'ucontext'
  ucontext = 1
elif backend == 'setjmp'
  if os == 'windows'
    loser = 1
  elif old_gnu_linux
    linux = 1
  else
    setjmp = 1
  endif
elif backend == 'fiber'
  fiber = 1
elif backend == 'asm'
  asm = 1
elif backend == 'pthread'
  pthread = 1
endif


configure_file(
  input : 'coroconfig.h.in',
  output : 'coroconfig.h',
  configuration : {
    'linux' : linux,
    'loser' : loser,
    'ucontext' : ucontext,
    'setjmp' : setjmp,
    'asm' : asm,
    'fiber' : fiber,
    'pthread' : pthread,
    'valgrind' : valgrind,
    'guardpages' : guardpages,
    'stackalloc' : stackalloc,
    'irix' : irix,
  }
)

libcoro_inc = include_directories('.', '..')
libcoro_lib = static_library('coro', 'coro.c',
                             dependencies : pthread != 0 ? [ threads_dep ] : [ ])
libcoro_dep = declare_dependency(link_with: [ libcoro_lib ],
                                 include_directories: libcoro_inc)
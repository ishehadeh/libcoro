project('libcoro', 'c',
  version : '3.0.0',
  default_options : [ 'warning_level=2' ])

cc = meson.get_compiler('c')
cpp_args = [ ]

# check for alternative signal stack support support
if cc.check_header('signal.h')
  cpp_args += [ '-DHAVE_SIGALTSTACK=1' ]
endif

# check other headers...
foreach header : [ 'ucontext.h', 'setjmp.h' ]
  if cc.check_header(header)
    cpp_args += [ '-DHAVE_@0@=1'.format(header.underscorify().to_upper()) ]
  endif
endforeach

libcoro_inc = include_directories('.')
libcoro_lib = static_library('coro', 'coro.c',  
           include_directories: libcoro_inc,
           cpp_args: cpp_args)

libcoro_dep = declare_dependency(
            link_with: [ libcoro_lib ],
            include_directories: libcoro_inc)
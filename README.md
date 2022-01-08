pellescc gcc-style compiler frontend for PellesC 8
--------------------------------------------------

PellesC 8 is the last 32bit release of PellesC, which is an astonishingly
good compiler, which beats mingw in POSIX compatibility.
it even features stuff like `mmap`, `fmemopen`, `open_memstream` which are
absent in all other compilers targeting windows (except cygwin).
it also has great compatibility with the C language and a very advanced
optimizer which gets close to gcc -O3.

this compiler driver serves as a gcc-style frontend to the compiler,
assembler and linker so pellesC can be used via wine from standard
unix configure scripts.

read the header of pellescc.c for the specific setup and examples.

pellesar.c is offered as a wrapper for the ar archiver, allowing you to
build libraries like zlib with ease.

PellesC 8.0 is no longer directly offered as a download from the PellesC
homepage, but you can find it [here](http://www.smorgasbordet.com/pellesc/800/setup.exe).

/* Pelles C 8.0 (last win32 release) gcc-style compiler driver.
 * use this in combination with wine to cross-compile stuff from linux
 * for windows using standard configure scripts.
 *
 * example:

   CC=pellescc CPPFLAGS=-I/root/wine/PellesC8/SDL2-2.0.18/Include \
   LDFLAGS=-L/root/wine/PellesC8/SDL2-2.0.18/Lib \
   ./configure --host=i386-pellesc-windows --with-sdl2

 * where pellescc is a shell script in PATH, containing:

 #!/bin/sh
 wine /root/Pelles\ C\ Projects/pellescc/pellescc.exe -U_MSC_VER "$@"

(C) 2022 rofl0r, licensed under the MIT License, as follows:

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the “Software”), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

/* add -DPELLESROOT="C:\\whatever\\your\\pelles\\basedir\\is" to override
 * the hardcoded location. the default one has a couple 0 bytes added
 * to give some leeway so you can even change it with a hexeditor.
 * alternatively, you can override it with the environment variable
 * "PELLESROOT". this is the base path containing "Lib", "Bin" and "Include".
 */

#ifndef PELLESROOT
#define PELLESROOT "Z:\\root\\wine\\PellesC8\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
#endif

#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>

/* these here are the only 2 function using win32-specific APIs */
#include <process.h>

static int run_process(char* const* argv) {
	return _spawnvp(_P_WAIT, argv[0], argv);
}

static unsigned long long get_timestamp(void) {
	return _rdtsc();
}

enum workmode {
	wm_none = 0,
	wm_cc_ld,
	wm_cc,
	wm_cpp,
	wm_as,
	wm_as_ld,
	wm_ld,
};

static void myassert(int cond, char* msg) {if(!cond) {fprintf(stderr, "%s\n", msg); exit(1); }}
static void ignore(char *msg) {fprintf(stderr, "ignoring flag %s\n", msg);}
static char *translate_path(char *s) {
	if(!strchr(s, '/')) return strdup(s);
	char buf[1024], *p = s, *q = buf, *e = buf + sizeof(buf) - 2;
	if(*p == '/') {*(q++) = 'Z'; *(q++) = ':'; }
	while(*p && q < e) {
		if(*p == '/') *(q++) = '\\';
		else *(q++) = *p;
		++p;
	}
	*q = 0;
	return strdup(buf);
}

#define LIST_MAX 128
#define LIST_ADD_PROTO(LIST) \
	static void add_ ## LIST(char *s) {LIST[n_ ## LIST ++] = translate_path(s); myassert(n_ ## LIST < LIST_MAX, "too many " #LIST); }

static char *incdirs[LIST_MAX];
static int n_incdirs;
LIST_ADD_PROTO(incdirs)
static char *libdirs[LIST_MAX];
static int n_libdirs;
LIST_ADD_PROTO(libdirs)
static char *libs[LIST_MAX];
static int n_libs;
LIST_ADD_PROTO(libs)
static char *defines[LIST_MAX];
static int n_defines;
LIST_ADD_PROTO(defines)
static char *undefines[LIST_MAX];
static int n_undefines;
LIST_ADD_PROTO(undefines)
static char *infiles[LIST_MAX];
static int n_infiles;
LIST_ADD_PROTO(infiles)
static char *linkerflags[LIST_MAX];
static int n_linkerflags;
LIST_ADD_PROTO(linkerflags)
static char *assemblerflags[LIST_MAX];
static int n_assemblerflags;
LIST_ADD_PROTO(assemblerflags)
static char *tempfiles[LIST_MAX];
static int n_tempfiles;
LIST_ADD_PROTO(tempfiles)


static int warn, verbose, debug, strip, statik, shared, std=99, unschar;
static char opt;
static char *outfile, *pellesroot = PELLESROOT;
static char tempnambuf[L_tmpnam+1+64];
static enum workmode mode;

static int parse_std(char*s) {
	while(*s && !isdigit(*s)) ++s;
	myassert(*s, "expected numeric value after -std=");
	int ret = atoi(s);
	if(ret > 80 && ret < 100) return 99;
	else return 11;
}

static char* get_bin(char *binname) {
	char buf[1024];
	snprintf(buf, sizeof buf, "%s\\Bin\\%s", pellesroot, binname);
	return strdup(buf);
}

/* the tmpnam() function in pelles stdlib is unfortunately too basic,
 * returning the same paths in concurrent use (like make -j16), which
 * leads to one process overwriting the other's output. */
static char* get_temp(void) {
	if(!tmpnam(tempnambuf)) tempnambuf[0] = 0;
	char buf[64];
	snprintf(buf, sizeof buf, "%016llx%08x.o", get_timestamp(), rand());
	strcat(tempnambuf, buf);
	add_tempfiles(strdup(tempnambuf));
	return tempnambuf;
}

static void cleanup_tempfiles(void) {
	int i;
	for(i = 0; i < n_tempfiles; ++i) {
		unlink(tempfiles[i]);
		free(tempfiles[i]);
	}
}

static void argv_add_incdirs(char** argv, int *n) {
	int i;
	char buf[1024];
	for (i = 0; i < n_incdirs; ++i) {
		snprintf(buf, sizeof buf, "/I%s", incdirs[i]);
		argv[*n] = strdup(buf);
		*n = *n + 1;
	}
}

static void argv_add_defines(char** argv, int *n) {
	int i;
	char buf[1024];
	for (i = 0; i < n_defines; ++i) {
		snprintf(buf, sizeof buf, "/D%s", defines[i]);
		argv[*n] = strdup(buf);
		*n = *n + 1;
	}
}

static char* select_outname(char *infile) {
	char buf[1024];
	// possible combinations:
	// -c and -o was specified ; in which case only one input file is allowed, use -o
	// -c and one more .c files were specified, but no output - use .o corresponding to .c
	// -c was not specified, in which case mode == wm_cc_ld; ignore -o (only for linker,
	//    and use temp names per .o
	// -E was specified, if with -o pass both on, else let dump to stdout.
	if(outfile && (mode == wm_cc || mode == wm_cpp || mode == wm_as)) {
		snprintf(buf, sizeof buf, "/Fo%s", outfile);
	} else if(mode == wm_cc || mode == wm_as) {
		snprintf(buf, sizeof buf, "/Fo%s", infile);
		char *p = strrchr(buf, '.');
		myassert(!!p, "internal error: failed to find file extension");
		*(++p) = 'o';
		*(++p) = 0;
	} else {
		snprintf(buf, sizeof buf, "/Fo%s", get_temp());
	}
	return strdup(buf);
}

static void call_multifile(char **argv, int n) {
	int i, j, in = n;
	for (i = 0; i < n_infiles; ++i) {
		if(i > 0) { free(argv[in]); free(argv[in+1]); }
		n = in;
		argv[n++] = strdup(infiles[i]);
		argv[n++] = select_outname(infiles[i]);
		argv[n] = 0;
		if(verbose) {
			for(j = 0; argv[j]; ++j) fprintf(stdout, "%s ", argv[j]);
			fprintf(stdout, "\n");
		}
		int ret;
		if((ret = run_process(argv))) exit(ret);
		// store temporary object name for eventual later linker call.
		infiles[i] = strdup(&argv[in+1][3]);
	}
}

static void run_compiler(void) {
	int i, n = 0;
	char buf[1024];
	char ** argv = calloc(256, sizeof(char*));
	argv[n++] = get_bin("pocc.exe");

	if(mode == wm_cpp) argv[n++] = strdup("/E");
	if(debug) argv[n++] = strdup("/Zi");
	if(std == 99) argv[n++] = strdup("/std:C99");
	else argv[n++] = strdup("/std:C11");
	switch(opt) {
	case 0: case '0': break;
	case '2': argv[n++] = strdup("/Ot"); break;
	case '3': argv[n++] = strdup("/Ox"); break;
	case '1':
	case 's':
	default:
			argv[n++] = strdup("/Os"); break;
	}
	if(unschar) argv[n++] = strdup("/J");

	argv_add_defines(argv, &n);
	for (i = 0; i < n_undefines; ++i) {
		snprintf(buf, sizeof buf, "/U%s", undefines[i]);
		argv[n++] = strdup(buf);
	}
	argv_add_incdirs(argv, &n);
	argv[n++] = strdup("/Tx86-coff");
	argv[n++] = strdup("/fp:precise");
	snprintf(buf, sizeof buf, "/W%d", warn > 2 ? 2 : warn);
	argv[n++] = strdup(buf);
	argv[n++] = strdup("/Zd");
	argv[n++] = strdup("/Ze");
	argv[n++] = strdup("/Go");
	call_multifile(argv, n);
	for(i = 0; argv[i]; ++i) free(argv[i]);
	free(argv);
}

static void run_assembler(void) {
	int i, n = 0;
	char ** argv = calloc(256, sizeof(char*));
	argv[n++] = get_bin("poasm.exe");
	argv[n++] = strdup("-AIA32");
	argv[n++] = strdup("-Gd");
	if(debug) argv[n++] = strdup("/Zi");
	argv_add_incdirs(argv, &n);
	argv_add_defines(argv, &n);
	call_multifile(argv, n);
	for(i = 0; argv[i]; ++i) free(argv[i]);
	free(argv);
}

static void run_linker(void) {
	int i, n = 0;
	char buf[1024];
	char ** argv = calloc(1024, sizeof(char*));
	argv[n++] = get_bin("polink.exe");
	for (i = 0; i < n_libdirs; ++i) {
		snprintf(buf, sizeof buf, "/libpath:%s", libdirs[i]);
		argv[n++] = strdup(buf);
	}
	argv[n++] = strdup("/subsystem:console");
 	argv[n++] = strdup("/machine:x86");
	if(debug) argv[n++] = strdup("/debug");
	if(debug) argv[n++] = strdup("/debugtype:cv");
	if(debug) argv[n++] = strdup("/map");
	if(shared) argv[n++] = strdup("/dll");
	argv[n++] = strdup("kernel32.lib");
	argv[n++] = strdup("advapi32.lib");
	argv[n++] = strdup("delayimp.lib");
	for (i = 0; i < n_libs; ++i) {
		// FIXME: here we should probably look up all libdirs and if no ".lib" is found, look for ".dll" instead
		snprintf(buf, sizeof buf, "%s.lib", libs[i]);
		argv[n++] = strdup(buf);
	}

	for (i = 0; i < n_infiles; ++i)
		argv[n++] = strdup(infiles[i]);
	snprintf(buf, sizeof buf, "/out:%s", outfile ? outfile : "a.out");
	argv[n++] = strdup(buf);
	argv[n] = 0;
	if(verbose) {
		for(i = 0; argv[i]; ++i) fprintf(stdout, "%s ", argv[i]);
		fprintf(stdout, "\n");
	}
	int ret;
	if((ret = run_process(argv))) exit(ret);
	for(i = 0; argv[i]; ++i) free(argv[i]);
	free(argv);
}

enum workmode mode_from_ext(char *ext) {
	enum workmode ret = wm_none;
	if(!strcmp(ext, "obj") || !strcmp(ext, "o")) ret = wm_ld;
	else if(!strcmp(ext, "asm") || !strcmp(ext, "s")) ret = wm_as;
	else if(!strcmp(ext, "c")) ret = wm_cc;
	return ret;
}

int main(int argc, char** argv) {
	if(getenv("PELLESROOT")) pellesroot = getenv("PELLESROOT");
	srand(get_timestamp());
	atexit(cleanup_tempfiles);
	char buf[1024];
	snprintf(buf, sizeof buf, "%s\\Lib\\Win", pellesroot);
	add_libdirs(strdup(buf));
	snprintf(buf, sizeof buf, "%s\\Lib", pellesroot);
	add_libdirs(strdup(buf));
	snprintf(buf, sizeof buf, "%s\\Include\\Win", pellesroot);
	add_incdirs(strdup(buf));
	snprintf(buf, sizeof buf, "%s\\Include", pellesroot);
	add_incdirs(strdup(buf));

	int i, next_o = 0, next_D = 0, next_U = 0, next_I = 0, next_L = 0;
	for(i = 1; i < argc; ++i) {
		if(next_o) {outfile = argv[i]; next_o = 0; }
		else if(next_D) {add_defines(argv[i]); next_D = 0; }
		else if(next_U) {add_undefines(argv[i]); next_U = 0; }
		else if(next_I) {add_incdirs(argv[i]); next_I = 0; }
		else if(next_L) {add_libdirs(argv[i]); next_L = 0; }
		else if(argv[i][0] == '-') switch(argv[i][1]) {
			case 'g': debug = (argv[i][2] != '0'); break;
			case 'v': verbose = 1; break;
			case 'c': mode = wm_cc; break;
			case 'E': mode = wm_cpp; break;
			case 'S': myassert(0, "output to assembly not supported"); break;
			case 'o':
					myassert(argv[i][2] == 0, "invalid -o arg");
					next_o = 1;
					break;
			case 'i':
					if(!strcmp(&argv[i][2], "dirafter") ||
					   !strcmp(&argv[i][2], "system") ||
					   !strcmp(&argv[i][2], "quote")) // https://gcc.gnu.org/onlinedocs/gcc/Directory-Options.html
						next_I = 1;
					else ignore(argv[i]);
					break;
			case 'O': if(argv[i][2] == 0) opt = '1'; else opt = argv[i][2]; break;
			case 'L': if(argv[i][2] == 0) next_L = 1; else add_libdirs(&argv[i][2]); break;
			case 'I': if(argv[i][2] == 0) next_I = 1; else add_incdirs(&argv[i][2]); break;
			case 'D': if(argv[i][2] == 0) next_D = 1; else add_defines(&argv[i][2]); break;
			case 'U': if(argv[i][2] == 0) next_U = 1; else add_undefines(&argv[i][2]); break;
			case 'l': myassert(argv[i][2] != 0, "expected lib name after -l"); add_libs(&argv[i][2]); break;
			case 's':
					if(argv[i][2] == 0) strip = 1;
					else if(!strcmp(&argv[i][2], "tatic")) statik = 1;
					else if(!strcmp(&argv[i][2], "hared")) shared = 1;
					else if(!strncmp(&argv[i][2], "td=", 4)) std = parse_std(&argv[i][5]);
					else ignore(argv[i]);
					break;
			case 'W':
					if(argv[i][2] == 'l') { myassert(argv[i][3] == ',', "expected ',' after -Wl"); add_linkerflags(&argv[i][3]); }
					else if(argv[i][2] == 'a' && argv[i][3] == ',') add_assemblerflags(&argv[i][3]);
					else if(argv[i][2] == 'n' && argv[i][3] == 'o') ;
					else ++warn;
					break;
			case 'f':
					if(!strcmp(&argv[i][2], "unsigned-char")) unschar = 1;
					else ignore(argv[i]);
					break;
			case 'd':
					if(!strcmp(&argv[i][2], "umpmachine")) {
						fprintf(stdout, "i386-pellesc-windows\n");
						return 0;
					}
					/* fall-through */
			default: myassert(0, "unknown compiler flag"); break;
		}
		else add_infiles(argv[i]);
	}
	myassert(n_infiles, "no input file passed");
	
	char *dot = strrchr(infiles[0], '.');
	myassert(!!dot, "failed to detect file extension");
	++dot;

	if(mode == wm_none) switch(mode_from_ext(dot)) {
		case wm_ld: mode = wm_ld; break;
		case wm_as: mode = wm_as_ld; break;
		case wm_cc: mode = wm_cc_ld; break;
		default:
		myassert(0, "failed to detect workmode");
	} else if(mode == wm_cc) switch(mode_from_ext(dot)) {
		case wm_as: mode = wm_as; break;
	}
	if(outfile && n_infiles > 1) switch(mode) {
		case wm_cpp:
		case wm_cc:
			myassert(0, "error: cannot specify -o with -c, -S or -E with multiple files");
			break;
	}
	switch(mode) {
		case wm_cc_ld:
		case wm_cc:
		case wm_cpp: run_compiler(); break;
		case wm_as_ld:
		case wm_as:  run_assembler();
		case wm_ld:  run_linker();
	}
	switch(mode) {
		case wm_cc_ld: 
		case wm_as_ld:
		run_linker();
	}
	return 0;
}


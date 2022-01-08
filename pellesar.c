/*
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

#include <process.h>

static int run_process(char* const* argv) {
	return _spawnvp(_P_WAIT, argv[0], argv);
}

static char *pellesroot = PELLESROOT;
static char *get_bin(char *binname) {
	char buf[1024];
	snprintf(buf, sizeof buf, "%s\\Bin\\%s", pellesroot, binname);
	return strdup(buf);
}

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

enum e_opts {
	OPT_R = 1,
	OPT_C = 2,
	OPT_S = 4,
	OPT_U = 8,
};

static int opts;
static void check_opt(int opt) {
	switch(opt) {
		case 'r': opts |= OPT_R; break;
		case 'c': opts |= OPT_C; break;
		case 's': opts |= OPT_S; break;
		case 'u': opts |= OPT_U; break;
		default:
			fprintf(stderr, "error: unsupported option %c\n", opt);
			exit(1);
	}
}

static int usage(void) {
	fprintf(stderr, "usage: ar r[csu] LIB OBJ1 [OBJS...]\n");
	return 1;
}

int main(int argc, char** argv) {
	if(getenv("PELLESROOT")) pellesroot = getenv("PELLESROOT");
	int i, j;
	for(i = 1; i < argc; ++i) {
		j = argv[i][0] == '-';
		if(!j && i != 1) break;
		for(; argv[i][j]; ++j) check_opt(argv[i][j]);
	}
	if(i == argc) return usage();
	char *outfn = argv[i++];
	if(i == argc || !(opts & OPT_R)) return usage();
	char **nargv = calloc(sizeof(char*), 1024);
	int n = 0;
	nargv[n++] = get_bin("polib.exe");
	char buf[1024], *p = translate_path(outfn);
	snprintf(buf, sizeof buf, "/OUT:%s", p);
	free(p);
	nargv[n++] = strdup(buf);
	for(; i < argc; ++i) {
		nargv[n++] = translate_path(argv[i]);
		if(n + 1 >= 1024) {fprintf(stderr, "error: too many files\n"); exit(1); }
	}
	nargv[n] = 0;
	int ret;
	if((ret = run_process(nargv))) exit(ret);
	return 0;
}

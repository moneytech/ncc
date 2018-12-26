/* Copyright (c) 2018 Charles E. Youse (charles@gnuless.org). 
   All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 * Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

 * Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */

#include <stdio.h>
#include <stddef.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/types.h>

/* this list will shrink once we implement archive support in the linker .. */

char * libs[] =
{
    "/lib/libc.a"
};

#define NR_LIBS (sizeof(libs)/sizeof(*libs))

char * mem();

/* lists holds the arguments used to invoke external programs */

#define LIST_INC 10

struct list
{
    int     cap; 
    int     len;
    char ** s;
};

struct list cpp;            /* cpp */
struct list cc1;            /* compiler */
struct list as;             /* assembler */
struct list ld;             /* linker */
struct list args;           /* current commands */
struct list temps;          /* list of temporary files (delete before exit) */

#define EXEC_FILE   0       
#define C_FILE      'c'
#define CC1_FILE    'i'
#define ASM_FILE    's'
#define OBJ_FILE    'o'

int    goal = EXEC_FILE;
char * ld_out;

/* print an error message and abort */

#ifdef __STDC__
void
error(char * fmt, ...)
#else
error(fmt)
    char * fmt;
#endif
{
    va_list args;

    if (fmt) {
        va_start(args, fmt);
        fprintf(stderr, "cc: ");
        vfprintf(stderr, fmt, args);
        va_end(args);
        fputc('\n', stderr);
    }

    rmtemps();
    exit(1);
}

/* safe malloc */

char *
mem(sz)
{
    char * p = malloc(sz);

    if (!p) error("out of memory");
    return p;
}


#define LIST_INC 10     /* increment of list element allocations */

#ifdef __STDC__
void
add(struct list * list, ...)
#else
add(list)
    struct list * list;
#endif
{
    va_list ss;
    char **new;
    char *s;

    va_start(ss, list);
    while (s = va_arg(ss, char *)) {
        if (list->len == list->cap) {
            new = (char **) mem(sizeof(char *) * (list->cap + LIST_INC + 1));
            if (new == NULL) error("out of memory");
            if (list->s) {
                memcpy(new, list->s, sizeof(char *) * (list->cap));
                free(list->s);
            }
            list->s = new;
            list->cap += LIST_INC;
        }
        list->s[list->len++] = s;
        list->s[list->len] = NULL;
    }
}

/* copy all the elements from 'dst' to 'src' */

copy(dst, src)
    struct list * dst;
    struct list * src;
{
    int i;

    dst->len = 0;
    if (dst->cap) dst->s[0] = NULL;
    for (i = 0; i < src->len; i++) add(dst, src->s[i], NULL);
}

/* remove all temporary files */

rmtemps()
{
    int i;

    for (i = 0; i < temps.len; i++) 
        unlink(temps.s[i]);
}


/* return the type of a file based on its extension */

type(name)
    char * name;
{
    char *dot;

    dot = strrchr(name, '.');

    if (dot) {
        dot++;
        switch (*dot) {
            case C_FILE:
            case CC1_FILE:
            case ASM_FILE:
            case OBJ_FILE:
                return *dot;
        }
    }

    error("'%s': unknown file type", name);
}

/* return a copy of the name in question with its
   extension changed to 'ext' */

char *
morph(name, ext)
    char * name;
{
    char * dot;
    char * new;

    new = mem(strlen(name) + 1);
    strcpy(new, name);
    dot = strrchr(new, '.');
    dot++;
    *dot = ext;

    return new;
}

/* run the command indicated by 'args', which will
   output to 'out'. 'out' will be removed if the program
   returns an error. */

run(args, out)
    struct list * args;
    char        * out;
{
    pid_t pid;
    int status;

    if ((pid = fork()) == 0) {
        execvp(args->s[0], args->s);
        error("can't exec '%s': %s", args->s[0], strerror(errno));
    } 

    if (pid == -1) error("can't fork: %s", strerror(errno));

    while (pid != wait(&status)) ;

    if (status != 0) {
        error("compilation terminated abnormally");
        add(&temps, out, NULL);
        error(NULL);
    }
}

main(argc, argv)
    char * argv[];
{
    char * new;
    char * src;
    int    i; 

    add(&cpp, "ncpp", NULL); 
    add(&cc1, "ncc1", NULL);
    add(&as, "nas", "-o", NULL);
    add(&ld, "nld", "-b", "0xFFFFFF8000000000", "-e", "cstart", "-o", NULL);

    ++argv;

    while (*argv && (*argv[0] == '-')) {
        switch ((*argv)[1]) {
            case 'D':
            case 'I':
                add(&cpp, *argv, NULL);
                break;
        
            case 'g':
            case 'O':
                add(&cc1, *argv, NULL);
                break;

            case 'S':
            case 'P':
            case 'c':
                if ((*argv)[2]) error("malformed goal option");
                if (goal != EXEC_FILE) error("conflicting goal options");

                goal = ((*argv)[1] == 'S') ? 
                            ASM_FILE 
                        : (((*argv)[1] == 'P') ? CC1_FILE : OBJ_FILE);

                break;

            case 'o':
                if ((*argv)[2] || !argv[1]) error("malformed output option");
                if (ld_out) error("duplicate output option");
                ++argv;
                ld_out = *argv;
                break;

            default:
                error("unrecognized option: %c\n", (*argv)[1]);
        }
        ++argv;
    }

    if (ld_out == NULL) ld_out = "a.out";
    add(&ld, ld_out, "/lib/cstart.o", NULL); 

    if (*argv == NULL) error("no input files");

    while (*argv) {
        src = *argv;
        switch (type(src)) {
            case C_FILE:
                new = morph(src, CC1_FILE);
                copy(&args, &cpp);
                add(&args, src, NULL);
                add(&args, new, NULL);
                run(&args, new);
                if (goal == CC1_FILE) break;
                add(&temps, new, NULL);
                src = new;

            case CC1_FILE:
                new = morph(src, ASM_FILE);
                copy(&args, &cc1);
                add(&args, src, NULL);
                add(&args, new, NULL);
                run(&args, new);
                if (goal == ASM_FILE) break;
                src = new;
                add(&temps, new, NULL);

            case ASM_FILE:
                new = morph(src, OBJ_FILE);
                copy(&args, &as);
                add(&args, new, NULL);
                add(&args, src, NULL);
                run(&args, new);
                if (goal == OBJ_FILE) break;
                add(&temps, new, NULL);
                src = new;

            case OBJ_FILE:
                add(&ld,  src, NULL);
        }
        argv++;
    }

    if (goal == EXEC_FILE) {
        for (i = 0; i < NR_LIBS; ++i) add(&ld, libs[i], NULL);
        run(&ld, ld_out);
    }

    rmtemps();
    exit(0);
}

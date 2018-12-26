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
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include "ncpp.h"

struct input * input_stack;

/* open a new file and put it on top of the input stack. the next call to
   input_line() will return text from this file. ownership of 'path' is
   yielded by the caller. */

input_open(path)
    struct vstring * path;
{
    struct input * input;

    input = (struct input *) safe_malloc(sizeof(struct input));
    input->path = path;
    input->line_number = 0;
    input->stack_link = input_stack;

    input->file = fopen(path->data, "r");
    if (!input->file) fail("can't open '%V' for reading", path);

    input_stack = input;
}

/* erase comments by over-writing with space */

static int in_comment;

static
erase_comments(vstring)
    struct vstring * vstring;
{
    int i = 0;
    int delimiter = 0;

    while (i < vstring->length) {
        if (delimiter) {
            if (vstring->data[i] == delimiter) 
                delimiter = 0;

            if ((vstring->data[i] == '\\') && vstring->data[i]) 
                ++i;
        } else if (in_comment) {
            if ((vstring->data[i] == '*') && (vstring->data[i+1] == '/')) {
                vstring->data[i + 1] = ' ';
                in_comment = 0;
            }
            vstring->data[i] = ' ';
        } else {
            if ((vstring->data[i] == '/') && (vstring->data[i+1] == '*')) {
                vstring->data[i] = ' ';
                vstring->data[i + 1] = ' ';  
                in_comment = 1;
            } else {
                if ((vstring->data[i] == '"') || (vstring->data[i] == '\''))
                    delimiter = vstring->data[i];
            }
        }
        ++i;
    }
}

/* get the next line of input off the input stack. returns NULL if there is
   no more input. this routine is responsible for logical line concatenation. 
   if 'mode' is INPUT_LINE_LIMITED, then refuse to cross a file boundary. */

struct vstring *
input_line(mode)
{
    struct vstring * vstring;
    int              c;
    int              last_was_backslash;  
    struct input *   tmp_input;

    while (input_stack) {
        c = getc(input_stack->file);

        if (c == -1) {
            if (in_comment) fail("file ends mid-comment");
            if (mode == INPUT_LINE_LIMITED) return NULL;

            fclose(input_stack->file);
            tmp_input = input_stack->stack_link;
            free(input_stack);
            input_stack = tmp_input;
        } else {
            ungetc(c, input_stack->file);
            break;
        }
    } 

    if (!input_stack) return NULL;

    vstring = vstring_new(NULL);
    input_stack->line_number++;

    for (;;) {
        last_was_backslash = (c == '\\');
        c = getc(input_stack->file);

        switch (c) {
        case '\n':
            if (last_was_backslash) {
                vstring_rubout(vstring);
                input_stack->line_number++;
                continue;
            }
            /* else fall through */
        case -1:
            erase_comments(vstring);
            return vstring;

        default:
            vstring_putc(vstring, c);
        }
    }
}

/* system include directories- that is, those searched when
   INPUT_INCLUDE_SYSTEM is given to input_include()- are kept 
   in a list that is searched in the reverse order that they
   are specified using input_include_directory(). */

struct include_directory
{
    struct vstring           * path;
    struct include_directory * previous;
};

struct include_directory * include_directories;

/* add a directory to search for system includes. */

input_include_directory(path)
    char * path;
{
    struct include_directory * directory;

    if (strlen(path) == 0) fail("missing include directory argument");

    directory = (struct include_directory *) safe_malloc(sizeof(struct include_directory));
    directory->previous = include_directories;
    directory->path = vstring_new(path);
    include_directories = directory;
}

/* input_include() searches appropriate places for a file with the given 
   path and puts it on top of the input stack with input_open(). the mode 
   governs what constitutes "an appropriate place":

   INPUT_INCLUDE_SYSTEM: search for the given path relative to 
   the 'include_directories'. the first file found wins.

   INPUT_INCLUDE_LOCAL: assume the given path is relative to the 
   path of the current input file (not the current directory). 
   
   ownership of 'path' is yielded by the caller. */

input_include(path, mode)
    struct vstring * path;
{   
    struct include_directory * directory;
    struct vstring           * new_path;

    if (mode == INPUT_INCLUDE_LOCAL) {
        new_path = vstring_copy(input_stack->path);

        while (new_path->length && (new_path->data[new_path->length - 1] != '/'))
            vstring_rubout(new_path);

        vstring_concat(new_path, path);
    } else {
        directory = include_directories;
        while (directory) {
            new_path = vstring_copy(directory->path);
            vstring_putc(new_path, '/');
            vstring_concat(new_path, path);

            if (!access(new_path->data, 0)) 
                break;
            else 
                vstring_free(new_path);

            directory = directory->previous;
        }
        if (directory == NULL) fail("'%V' not found in system include paths", path);
    }

    input_open(new_path);
    vstring_free(path);
}

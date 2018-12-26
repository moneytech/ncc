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

#include <errno.h>
#include <stdlib.h>
#include <limits.h>
#include <ctype.h>
#include "ncc1.h"

static int    yych;         /* current input character */

#define yynext() (yych = getc(yyin))

/* we maintain a dynamically-growing token buffer for
   those token classes with interesting text. */

#define YY_INCREMENT 128    /* grow in these increments */

static int    yycap;        /* capacity of token buffer */
static int    yylen;        /* length of current token */
static char * yybuf;        /* token buffer */
static char * yytext;       /* pointer into yytext */

/* a pushback buffer for peek(). priming this with KK_NL is a trick
   that makes the logic in lex() work properly on the first call. */

static struct token next = { KK_NL };  

static
fcon()
{
    char * endptr;
    int    kk;

    kk = KK_LFCON;
    errno = 0;
    token.u.f = strtod(yytext, &endptr);
    if (toupper(yych) != 'L') {
        errno = 0;
        kk = KK_FCON;
        token.u.f = strtof(yytext, &endptr);
    } else
        yynext();

    if (errno == ERANGE) error(ERROR_FRANGE);
    if (errno || *endptr) error(ERROR_BADFCON);
    return kk;
}

/* returns the next character of a char constant or string literal,
   interpreting escape codes. */

#define ISODIGIT(c)     (isdigit(c) && ((c) < '8'))
#define DIGIT_VALUE(c)  ((c) - '0')

static
escape()
{
    int c;

    if (*yytext == '\\') {
        yytext++;
        switch (*yytext) {
        case 'b': c = '\b'; yytext++; break;
        case 'f': c = '\f'; yytext++; break;
        case 'n': c = '\n'; yytext++; break;
        case 'r': c = '\r'; yytext++; break;
        case 't': c = '\t'; yytext++; break;

        case '0': case '1': case '2': case '3':
        case '4': case '5': case '6': case '7':
            c = DIGIT_VALUE(*yytext);
            yytext++;
            if (ISODIGIT(*yytext)) {
                c <<= 3;
                c += DIGIT_VALUE(*yytext);
                yytext++;
                if (ISODIGIT(*yytext)) {
                    c <<= 3;
                    c += DIGIT_VALUE(*yytext);
                    yytext++;
                }
            }
            if (c > UCHAR_MAX) error(ERROR_ESCAPE);
            break;

        default:
            c = *yytext++;
        }
    } else 
        c = *yytext++;

    return c;
}

static
ccon()
{
    int value = 0;

    yytext++;
    while (*yytext != '\'') {
        if (value & 0xFF000000) error(ERROR_CRANGE);
        value <<= 8;
        value += escape();
    }

    token.u.i = value;
    return KK_ICON;
}

static
strlit()
{
    char *  data = yytext;
    char *  cp = yytext;
    int     length = 0;

    yytext++; 
    while (*yytext != '\"') {
        *cp++ = escape();
        length++;
    }

    token.u.text = stringize(data, length);
    return KK_STRLIT;
}

/* stash character 'c' in the token buffer, growing
   the buffer if necessary. */

static
yystash(c)
{
    char * new_yybuf;

    if (yylen == yycap) {
        new_yybuf = allocate(yycap + YY_INCREMENT + 1);
        memcpy(new_yybuf, yybuf, yycap);
        free(yybuf);
        yybuf = new_yybuf;
        yytext = yybuf;
    }

    yybuf[yylen++] = c;
    yybuf[yylen] = 0;
}

/* called by main() after setting 'yyin' but before the first
   call to lex() to initialize the scanner. */

static char * keyword[] =
{
    "auto", "break", "case", "char", "continue", "default", "do",
    "else", "extern", "float", "for", "goto", "if", "int", "long", 
    "register", "return", "short", "sizeof", "static", "struct", 
    "switch", "typedef", "union", "unsigned", "while"
};

#define NR_KEYWORDS (sizeof(keyword)/sizeof(*keyword))

yyinit()
{
    struct string * k;
    int             i;

    for (i = 0; i < NR_KEYWORDS; ++i) {
        k = stringize(keyword[i], strlen(keyword[i]));
        k->token = KK_AUTO + i;
    }

    yynext();
}

/* determine the type and value of numeric constants. note that 
   we differ from K&R here: constants aren't auto-promoted based
   on value. they're 'int' or 'float' unless suffixed with L, in
   which case they are 'long' or 'long float'. */

static 
icon()
{
    unsigned long value = 0;
    char *        endptr;
    int           kk;

    errno = 0;
    kk = KK_LCON;
    value = strtoul(yytext, &endptr, 0);
    if (errno == ERANGE) error(ERROR_IRANGE);

    if (toupper(yych) != 'L') {
        kk = KK_ICON;
        if ((*yytext != '0') && (value > INT_MAX)) error(ERROR_IRANGE);
        if ((*yytext == '0') && (value > UINT_MAX)) error(ERROR_IRANGE);
    } else 
        yynext();   /* eat 'L' */

    if (errno || *endptr) error(ERROR_BADICON);
    token.u.i = value;
    return kk;
}

/* just like a flex auto-generated yylex, except that we don't
   bother stashing tokens in 'yytext' if the text is irrelevant. */

static
yylex()
{
    int             delim;
    int             backslash;

    while (isspace(yych) && (yych != '\n'))
        yynext();

    yylen = 0;
    yytext = yybuf;

    switch (yych)
    {
    case -1:    return KK_NONE;
    case '\n':  yynext(); return KK_NL;
    case '#':   yynext(); return KK_HASH;
    case '(':   yynext(); return KK_LPAREN;
    case ')':   yynext(); return KK_RPAREN;
    case '[':   yynext(); return KK_LBRACK;
    case ']':   yynext(); return KK_RBRACK;
    case '{':   yynext(); return KK_LBRACE;
    case '}':   yynext(); return KK_RBRACE;
    case ',':   yynext(); return KK_COMMA;
    case ':':   yynext(); return KK_COLON;
    case ';':   yynext(); return KK_SEMI;
    case '?':   yynext(); return KK_QUEST;
    case '~':   yynext(); return KK_TILDE;

    case '!':
        yynext();

        if (yych == '=') {
            yynext();
            return KK_BANGEQ;
        }

        return KK_BANG;

    case '=':
        yynext();

        if (yych == '=') {
            yynext();
            return KK_EQEQ;
        }

        return KK_EQ;

    case '^':
        yynext();

        if (yych == '=') {
            yynext();
            return KK_XOREQ;
        }

        return KK_XOR;

    case '*':
        yynext();

        if (yych == '=') {
            yynext();
            return KK_STAREQ;
        }

        return KK_STAR;

    case '%':
        yynext();

        if (yych == '=') {
            yynext();
            return KK_MODEQ;
        }

        return KK_MOD;

    case '/':
        yynext();

        if (yych == '=') {
            yynext();
            return KK_DIVEQ;
        }

        return KK_DIV;

    case '+':
        yynext();

        if (yych == '+') {
            yynext();
            return KK_INC;
        } else if (yych == '=') {
            yynext();
            return KK_PLUSEQ;
        }

        return KK_PLUS;

    case '-':
        yynext();

        if (yych == '-') {
            yynext();
            return KK_DEC;
        } else if (yych == '=') {
            yynext();
            return KK_MINUSEQ;
        } else if (yych == '>') {
            yynext();
            return KK_ARROW;
        }

        return KK_MINUS;

    case '&':
        yynext();

        if (yych == '&') {
            yynext();
            return KK_ANDAND;
        } else if (yych == '=') {
            yynext();
            return KK_ANDEQ;
        }

        return KK_AND;

    case '|':
        yynext();

        if (yych == '|') {
            yynext();
            return KK_BARBAR;
        } else if (yych == '=') {
            yynext();
            return KK_BAREQ;
        }

        return KK_BAR;

    case '.':
        yynext();

        if (isdigit(yych)) {
            /* whoops, it's a float */
            ungetc(yych, yyin);
            yych = '.';
            break;
        }

        return KK_DOT;

    case '>':
        yynext();

        if (yych == '>') {
            yynext();
            if (yych == '=') {
                yynext();
                return KK_SHREQ;
            }
            return KK_SHR;
        } else if (yych == '=') {
            yynext();
            return KK_GTEQ;
        }

        return KK_GT;

    case '<':
        yynext();

        if (yych == '<') {
            yynext();
            if (yych == '=') {
                yynext();
                return KK_SHLEQ;
            }
            return KK_SHL;
        } else if (yych == '=') {
            yynext();
            return KK_LTEQ;
        }

        return KK_LT;

    case '\'':
    case '\"':
        delim = yych;
        backslash = 1;  /* fake out first loop */

        while ((yych >= 0) && (yych != '\n')) {
            yystash(yych);
            if ((yych == delim) && !backslash) break;
            backslash = (yych == '\\') && !backslash;
            yynext();
        }

        if ((yych < 0) || (yych == '\n')) 
            error((delim == '"') ? ERROR_UNTERM : ERROR_BADCCON);

        yynext();   /* eat closing delimiter */

        if (delim == '"')
            return strlit();
        else
            return ccon();

    default: /* fall through */ ;
    }

    /* identifiers/keywords */

    if (isalpha(yych) || (yych == '_')) {
        while (isalnum(yych) || (yych == '_')) {
            yystash(yych);
            yynext();
        }

        token.u.text = stringize(yytext, yylen);
        return token.u.text->token;
    }

    /* numbers */

    if (isdigit(yych) || (yych == '.')) {
        if (yych == '0') {
            yystash(yych);
            yynext();

            if (toupper(yych) == 'X') {
                yystash(yych);
                yynext();

                while (isxdigit(yych)) {
                    yystash(yych);
                    yynext();
                }

                return icon();
            }
        }

        while (isdigit(yych)) {
            yystash(yych);
            yynext();
        }

        if ((yych == '.') || (toupper(yych) == 'E')) {
            if (yych == '.') {
                yystash(yych);
                yynext();

                while (isdigit(yych)) {
                    yystash(yych);
                    yynext();
                }
            }


            if (toupper(yych) == 'E') {
                yystash(yych);
                yynext();

                if (yych == '-') {
                    yystash(yych);
                    yynext();
                } else if (yych == '+') {
                    yystash(yych);
                    yynext();
                }

                while (isdigit(yych)) {
                    yystash(yych);
                    yynext();
                }
            }

            return fcon();
        } else 
            return icon();
    }

    error(ERROR_LEXICAL);
}

/* peek returns some information about the next token in the 
   input. the parser needs this in a few random circumstances 
   where the grammar is slightly irregular. */

peek(id)
    struct string ** id;
{
    struct token saved;
    int          kk;

    memcpy(&saved, &token, sizeof(struct token));
    lex();
    kk = token.kk;
    if ((kk == KK_IDENT) && (id)) *id = token.u.text;
    memcpy(&next, &token, sizeof(struct token));
    memcpy(&token, &saved, sizeof(struct token));
    return kk;
}

/* the lexical analyzer functions as a three-stage pipeline:

    lex() 
        called by the parser to retrieve the next token.
        this filters the output of ylex(), stripping pseudo 
        tokens, after using them to track the input file
        location by counting newlines and processing directives.

    ylex()
        sits between lex() and yylex(), reinjecting
        tokens buffered by peek()

    yylex()
        the scanner proper, divides the input into tokens */

static
ylex()
{
    if (next.kk == KK_NONE) {
        token.kk = yylex();
    } else {
        memcpy(&token, &next, sizeof(struct token));
        next.kk = KK_NONE;
    }
}

lex()
{
    ylex();
    while (token.kk == KK_NL) {
        line_number++;
        ylex();
        while (token.kk == KK_HASH) {
            ylex();
            if (token.kk == KK_ICON) {
                line_number = token.u.i;
                ylex();
                if (token.kk == KK_STRLIT) {
                    input_name = token.u.text;
                    ylex();
                }
            }

            /* there's a good chance the error will be 
               reported in the wrong location, seeing as
               the error is in grokking the location. */

            if (token.kk != KK_NL) error(ERROR_DIRECTIVE);
            ylex();
        }
    }
}

/* simple parsing helper functions.
   these should be self-explanatory. */

expect(kk)
{
    if (token.kk != kk) error(ERROR_SYNTAX);
}

prohibit(kk)
{
    if (token.kk == kk) error(ERROR_SYNTAX);
}

match(kk)
{
    expect(kk);
    lex();
}

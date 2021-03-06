﻿
/*
** This file contains all sources (including headers) to the LEMON
** LALR(1) parser generator.  The sources have been combined into a
** single file to make it easy to include LEMON in the source tree
** and Makefile of another program.
**
** The author of this program disclaims copyright.
*/

#include "lemon.h"

//Allows the use of fopen/getenv/sprintf

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <assert.h>

#define ISSPACE(X) isspace(static_cast<unsigned char>(X))
#define ISDIGIT(X) isdigit(static_cast<unsigned char>(X))
#define ISALNUM(X) isalnum(static_cast<unsigned char>(X))
#define ISALPHA(X) isalpha(static_cast<unsigned char>(X))
#define ISUPPER(X) isupper(static_cast<unsigned char>(X))
#define ISLOWER(X) islower(static_cast<unsigned char>(X))


#ifndef __WIN32__
#   if defined(_WIN32) || defined(WIN32)
#       define __WIN32__
#   endif
#endif

#ifdef __WIN32__
#ifdef __cplusplus
extern "C" {
#endif
    extern int access(const char* path, int mode);
#ifdef __cplusplus
}
#endif
#else
#include <unistd.h>
#endif

//C++ Headers
#include <algorithm>
#include <numeric>
#include <functional>

#include <iostream>
#include <iomanip>

#include <map>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_set>
#include <iterator>

/* #define PRIVATE static */
#define PRIVATE

#ifdef TEST
#define MAXRHS 5       /* Set low to exercise exception code */
#else
#define MAXRHS 1000
#endif

extern void memory_error();
static int showPrecedenceConflict = 0;
static char* msort(char*, const char**, int(*)(const char*, const char*));

/*
** Compilers are getting increasingly pedantic about type conversions
** as C evolves ever closer to Ada....  To work around the latest problems
** we have to define the following variant of strlen().
*/
#define lemonStrlen(X)   ((int)strlen(X))

/*
** Compilers are starting to complain about the use of sprintf() and strcpy(),
** saying they are unsafe.  So we define our own versions of those routines too.
**
** There are three routines here:  lemon_sprintf(), lemon_vsprintf(), and
** lemon_addtext(). The first two are replacements for sprintf() and vsprintf().
** The third is a helper routine for vsnprintf() that adds texts to the end of a
** buffer, making sure the buffer is always zero-terminated.
**
** The string formatter is a minimal subset of stdlib sprintf() supporting only
** a few simply conversions:
**
**   %d
**   %s
**   %.*s
**
*/
static void lemon_addtext(
    char* zBuf,           /* The buffer to which text is added */
    int* pnUsed,          /* Slots of the buffer used so far */
    const char* zIn,      /* Text to add */
    int nIn,              /* Bytes of text to add.  -1 to use strlen() */
    int iWidth            /* Field width.  Negative to left justify */
) {
    if (nIn < 0) for (nIn = 0; zIn[nIn]; nIn++) {}
    while (iWidth > nIn) { zBuf[(*pnUsed)++] = ' '; iWidth--; }
    if (nIn == 0) return;
    memcpy(&zBuf[*pnUsed], zIn, nIn);
    *pnUsed += nIn;
    while ((-iWidth) > nIn) { zBuf[(*pnUsed)++] = ' '; iWidth++; }
    zBuf[*pnUsed] = 0;
}
static int lemon_vsprintf(char* str, const char* zFormat, va_list ap) {
    int i, j, k, c;
    int nUsed = 0;
    const char* z;
    char zTemp[50];
    str[0] = 0;
    for (i = j = 0; (c = zFormat[i]) != 0; i++) {
        if (c == '%') {
            int iWidth = 0;
            lemon_addtext(str, &nUsed, &zFormat[j], i - j, 0);
            c = zFormat[++i];
            if (ISDIGIT(c) || (c == '-' && ISDIGIT(zFormat[i + 1]))) {
                if (c == '-') i++;
                while (ISDIGIT(zFormat[i])) iWidth = iWidth * 10 + zFormat[i++] - '0';
                if (c == '-') iWidth = -iWidth;
                c = zFormat[i];
            }
            if (c == 'd') {
                int v = va_arg(ap, int);
                if (v < 0) {
                    lemon_addtext(str, &nUsed, "-", 1, iWidth);
                    v = -v;
                }
                else if (v == 0) {
                    lemon_addtext(str, &nUsed, "0", 1, iWidth);
                }
                k = 0;
                while (v > 0) {
                    k++;
                    zTemp[sizeof(zTemp) - k] = (v % 10) + '0';
                    v /= 10;
                }
                lemon_addtext(str, &nUsed, &zTemp[sizeof(zTemp) - k], k, iWidth);
            }
            else if (c == 's') {
                z = va_arg(ap, const char*);
                lemon_addtext(str, &nUsed, z, -1, iWidth);
            }
            else if (c == '.' && memcmp(&zFormat[i], ".*s", 3) == 0) {
                i += 2;
                k = va_arg(ap, int);
                z = va_arg(ap, const char*);
                lemon_addtext(str, &nUsed, z, k, iWidth);
            }
            else if (c == '%') {
                lemon_addtext(str, &nUsed, "%", 1, 0);
            }
            else {
                fprintf(stderr, "illegal format\n");
                exit(1);
            }
            j = i + 1;
        }
    }
    lemon_addtext(str, &nUsed, &zFormat[j], i - j, 0);
    return nUsed;
}
static int lemon_sprintf(char* str, const char* format, ...) {
    va_list ap;
    int rc;
    va_start(ap, format);
    rc = lemon_vsprintf(str, format, ap);
    va_end(ap);
    return rc;
}
static void lemon_strcpy(char* dest, const char* src) {
    while ((*(dest++) = *(src++)) != 0) {}
}
static void lemon_strcat(char* dest, const char* src) {
    while (*dest) dest++;
    lemon_strcpy(dest, src);
}

/********** From the file "build.h" ************************************/
void FindRulePrecedences(lemon&);
void FindFirstSets(lemon&);
void FindStates(lemon&);
void FindLinks(lemon&);
void FindFollowSets(lemon&);
void FindActions(lemon&);


/********* From the file "error.h" ***************************************/
void ErrorMsg(const char*, int, const char*, ...);

/****** From the file "option.h" ******************************************/
enum option_type {
    OPT_FLAG = 1, OPT_INT, OPT_DBL, OPT_STR,
    OPT_FFLAG, OPT_FINT, OPT_FDBL, OPT_FSTR
};
struct s_options {
    option_type type;
    const char* label;
    char* arg;
    const char* message;
};
int    OptInit(char*&);
int    OptNArgs();
char* OptArg(int);
void   OptErr(int);
void   OptPrint();

/******** From the file "parse.h" *****************************************/
void Parse(lemon& lemp);

/********* From the file "plink.h" ***************************************/

/********** From the file "report.h" *************************************/
void Reprint(lemon&);
void ReportOutput(lemon&);
void ReportTable(lemon&, int, int);
void ReportHeader(lemon&);
void CompressTables(lemon&);
void ResortStates(lemon&);

/********** From the file "set.h" ****************************************/
void  SetSize(int);             /* All sets will be of size N */
char* SetNew();               /* A new set for element 0..N */
void  SetFree(char*);             /* Deallocate a set */
int SetAdd(char*, int);            /* Add element to a set */
int SetUnion(char*, const char*);    /* A <- A U B, thru element N */
#define SetFind(X,Y) (X[Y])       /* True if Y is in set X */

/********** From the file "struct.h" *************************************/
/*
** Principal data structures for the LEMON parser generator.
*/

#define NO_OFFSET (-2147483647)


#define MemoryCheck(X) if((X)==nullptr){ \
  memory_error(); \
}

/**************** From the file "table.h" *********************************/
/*
** All code in this file has been automatically generated
** from a specification in the file
**              "table.q"
** by the associative array code building program "aagen".
** Do not edit this file!  Instead, edit the specification
** file, then rerun aagen.
*/
/*
** Code for processing tables in the LEMON parser generator.
*/
/* Routines for handling a strings */
const char* Strsafe(std::string_view);
void Strsafe_init(void);
bool Strsafe_insert(std::string_view);
std::string_view Strsafe_find(std::string_view);

namespace Action
{
/*
** Routines processing parser actions in the LEMON parser generator.
*/


/* Allocate a new parser action */
static action* Action_new(void) {
    static action* actionfreelist = nullptr;
    action* newaction;

    if (actionfreelist == nullptr) {
        int i;
        int amt = 100;
        actionfreelist = (action*)calloc(amt, sizeof(action));
        if (actionfreelist == nullptr) {
            fprintf(stderr, "Unable to allocate memory for a new parser action.");
            exit(1);
        }
        for (i = 0; i < amt - 1; i++) actionfreelist[i].next = &actionfreelist[i + 1];
        actionfreelist[amt - 1].next = nullptr;
    }
    newaction = actionfreelist;
    actionfreelist = actionfreelist->next;
    return newaction;
}

/* Compare two actions for sorting purposes.  Return negative, zero, or
** positive if the first action is less than, equal to, or greater than
** the first
*/
static int actioncmp(
    const action* ap1,
    const action* ap2
) {
    int rc;
    rc = ap1->sp->index - ap2->sp->index;
    if (rc == 0) {
        rc = (int)ap1->type - (int)ap2->type;
    }
    if (rc == 0 && (ap1->type == e_action::REDUCE || ap1->type == e_action::SHIFTREDUCE)) {
        rc = ap1->x.rp->index - ap2->x.rp->index;
    }
    if (rc == 0) {
        rc = (int)(ap2 - ap1);
    }
    return rc;
}

/* Sort parser actions */
static action* Action_sort(
    action* ap
) {
    ap = (action*)msort((char*)ap, (const char**)&ap->next,
        (int(*)(const char*, const char*))actioncmp);
    return ap;
}

void Action_add(
    action** app,
    e_action type,
    symbol* sp,
    char* arg
) {
    action* newaction;
    newaction = Action_new();
    newaction->next = *app;
    *app = newaction;
    newaction->type = type;
    newaction->sp = sp;
    newaction->spOpt = nullptr;
    if (type == e_action::SHIFT) {
        newaction->x.stp = (state*)arg;
    }
    else {
        newaction->x.rp = (rule*)arg;
    }
}
}
using namespace Action;

/********************** New code to implement the "acttab" module ***********/
/*
** This module implements routines use to construct the yy_action[] table.
*/

namespace acctab
{
/*
** The state of the yy_action table under construction is an instance of
** the following structure.
**
** The yy_action table maps the pair (state_number, lookahead) into an
** action_number.  The table is an array of integers pairs.  The state_number
** determines an initial offset into the yy_action array.  The lookahead
** value is then added to this initial offset to get an index X into the
** yy_action array. If the aAction[X].lookahead equals the value of the
** of the lookahead input, then the value of the action_number output is
** aAction[X].action.  If the lookaheads do not match then the
** default action for the state_number is returned.
**
** All actions associated with a single state_number are first entered
** into aLookahead[] using multiple calls to acttab_action().  Then the
** actions for that single state_number are placed into the aAction[]
** array with a single call to acttab_insert().  The acttab_insert() call
** also resets the aLookahead[] array in preparation for the next
** state number.
*/
struct lookahead_action {
    int lookahead;             /* Value of the lookahead token */
    int action;                /* Action to take on the given lookahead */
};

struct acttab {
    int nAction;                 /* Number of used slots in aAction[] */
    int nActionAlloc;            /* Slots allocated for aAction[] */
    lookahead_action
        * aAction,                  /* The yy_action[] table under construction */
        * aLookahead;               /* A single new transaction set */
    int mnLookahead;             /* Minimum aLookahead[].lookahead */
    int mnAction;                /* Action associated with mnLookahead */
    int mxLookahead;             /* Maximum aLookahead[].lookahead */
    int nLookahead;              /* Used slots in aLookahead[] */
    int nLookaheadAlloc;         /* Slots allocated in aLookahead[] */
    int nterminal;               /* Number of terminal symbols */
    int nsymbol;                 /* total number of symbols */
};

/* Return the number of entries in the yy_action table */
#define acttab_lookahead_size(X) ((X)->nAction)

/* The value for the N-th entry in yy_action */
#define acttab_yyaction(X,N)  ((X)->aAction[N].action)

/* The value for the N-th entry in yy_lookahead */
#define acttab_yylookahead(X,N)  ((X)->aAction[N].lookahead)

/* Free all memory associated with the given acttab */
void acttab_free(acttab* p) {
    delete p->aAction;
    delete p->aLookahead;
    delete p;
}

/* Allocate a new acttab structure */
acttab* acttab_alloc(int nsymbol, int nterminal) {
    acttab* p = (acttab*)calloc(1, sizeof(*p));
    if (p == nullptr) {
        fprintf(stderr, "Unable to allocate memory for a new acttab.");
        exit(1);
    }
    memset(p, 0, sizeof(*p));
    p->nsymbol = nsymbol;
    p->nterminal = nterminal;
    return p;
}

/* Add a new action to the current transaction set.
**
** This routine is called once for each lookahead for a particular
** state.
*/
void acttab_action(acttab* p, int lookahead, int action) {
    if (p->nLookahead >= p->nLookaheadAlloc) {
        p->nLookaheadAlloc += 25;
        p->aLookahead = (lookahead_action*)realloc(p->aLookahead,
            sizeof(p->aLookahead[0]) * p->nLookaheadAlloc);
        if (p->aLookahead == nullptr) {
            fprintf(stderr, "malloc failed\n");
            exit(1);
        }
    }
    if (p->nLookahead == 0) {
        p->mxLookahead = lookahead;
        p->mnLookahead = lookahead;
        p->mnAction = action;
    }
    else {
        if (p->mxLookahead < lookahead) p->mxLookahead = lookahead;
        if (p->mnLookahead > lookahead) {
            p->mnLookahead = lookahead;
            p->mnAction = action;
        }
    }
    p->aLookahead[p->nLookahead].lookahead = lookahead;
    p->aLookahead[p->nLookahead].action = action;
    p->nLookahead++;
}

/*
** Add the transaction set built up with prior calls to acttab_action()
** into the current action table.  Then reset the transaction set back
** to an empty set in preparation for a new round of acttab_action() calls.
**
** Return the offset into the action table of the new transaction.
**
** If the makeItSafe parameter is true, then the offset is chosen so that
** it is impossible to overread the yy_lookaside[] table regardless of
** the lookaside token.  This is done for the terminal symbols, as they
** come from external inputs and can contain syntax errors.  When makeItSafe
** is false, there is more flexibility in selecting offsets, resulting in
** a smaller table.  For non-terminal symbols, which are never syntax errors,
** makeItSafe can be false.
*/
int acttab_insert(acttab* p, int makeItSafe) {
    int i, j, k, n, end;
    assert(p->nLookahead > 0);

    /* Make sure we have enough space to hold the expanded action table
    ** in the worst case.  The worst case occurs if the transaction set
    ** must be appended to the current action table
    */
    n = p->nsymbol + 1;
    if (p->nAction + n >= p->nActionAlloc) {
        const int oldAlloc = p->nActionAlloc;
        p->nActionAlloc = p->nAction + n + p->nActionAlloc + 20;
        p->aAction = (lookahead_action*)realloc(p->aAction,
            sizeof(p->aAction[0]) * p->nActionAlloc);
        if (p->aAction == nullptr) {
            fprintf(stderr, "malloc failed\n");
            exit(1);
        }
        for (i = oldAlloc; i < p->nActionAlloc; i++) {
            p->aAction[i].lookahead = -1;
            p->aAction[i].action = -1;
        }
    }

    /* Scan the existing action table looking for an offset that is a
    ** duplicate of the current transaction set.  Fall out of the loop
    ** if and when the duplicate is found.
    **
    ** i is the index in p->aAction[] where p->mnLookahead is inserted.
    */
    end = makeItSafe ? p->mnLookahead : 0;
    for (i = p->nAction - 1; i >= end; i--) {
        if (p->aAction[i].lookahead == p->mnLookahead) {
            /* All lookaheads and actions in the aLookahead[] transaction
            ** must match against the candidate aAction[i] entry. */
            if (p->aAction[i].action != p->mnAction) continue;
            for (j = 0; j < p->nLookahead; j++) {
                k = p->aLookahead[j].lookahead - p->mnLookahead + i;
                if (k < 0 || k >= p->nAction) break;
                if (p->aLookahead[j].lookahead != p->aAction[k].lookahead) break;
                if (p->aLookahead[j].action != p->aAction[k].action) break;
            }
            if (j < p->nLookahead) continue;

            /* No possible lookahead value that is not in the aLookahead[]
            ** transaction is allowed to match aAction[i] */
            n = 0;
            for (j = 0; j < p->nAction; j++) {
                if (p->aAction[j].lookahead < 0) continue;
                if (p->aAction[j].lookahead == j + p->mnLookahead - i) n++;
            }
            if (n == p->nLookahead) {
                break;  /* An exact match is found at offset i */
            }
        }
    }

    /* If no existing offsets exactly match the current transaction, find an
    ** an empty offset in the aAction[] table in which we can add the
    ** aLookahead[] transaction.
    */
    if (i < end) {
        /* Look for holes in the aAction[] table that fit the current
        ** aLookahead[] transaction.  Leave i set to the offset of the hole.
        ** If no holes are found, i is left at p->nAction, which means the
        ** transaction will be appended. */
        i = makeItSafe ? p->mnLookahead : 0;
        for (; i < p->nActionAlloc - p->mxLookahead; i++) {
            if (p->aAction[i].lookahead < 0) {
                for (j = 0; j < p->nLookahead; j++) {
                    k = p->aLookahead[j].lookahead - p->mnLookahead + i;
                    if (k < 0) break;
                    if (p->aAction[k].lookahead >= 0) break;
                }
                if (j < p->nLookahead) continue;
                for (j = 0; j < p->nAction; j++) {
                    if (p->aAction[j].lookahead == j + p->mnLookahead - i) break;
                }
                if (j == p->nAction) {
                    break;  /* Fits in empty slots */
                }
            }
        }
    }
    /* Insert transaction set at index i. */
#if 0
    printf("Acttab:");
    for (j = 0; j < p->nLookahead; j++) {
        printf(" %d", p->aLookahead[j].lookahead);
    }
    printf(" inserted at %d\n", i);
#endif
    for (j = 0; j < p->nLookahead; j++) {
        k = p->aLookahead[j].lookahead - p->mnLookahead + i;
        p->aAction[k] = p->aLookahead[j];
        if (k >= p->nAction) p->nAction = k + 1;
    }
    if (makeItSafe && i + p->nterminal >= p->nAction) p->nAction = i + p->nterminal + 1;
    p->nLookahead = 0;

    /* Return the offset that is added to the lookahead in order to get the
    ** index into yy_action of the action */
    return i - p->mnLookahead;
}

/*
** Return the size of the action table without the trailing syntax error
** entries.
*/
int acttab_action_size(const acttab* p) {
    int n = p->nAction;
    while (n > 0 && p->aAction[n - 1].lookahead < 0) { n--; }
    return n;
}

}
using namespace acctab;

/********************** From the file "build.c" *****************************/
/*
** Routines to construction the finite state machine for the LEMON
** parser generator.
*/

/* Find a precedence symbol of every rule in the grammar.
**
** Those rules which have a precedence symbol coded in the input
** grammar using the "[symbol]" construct will already have the
** rp->precsym field filled.  Other rules take as their precedence
** symbol the first RHS symbol with a defined precedence.  If there
** are not RHS symbols with a defined precedence, the precedence
** symbol field is left blank.
*/
void FindRulePrecedences(lemon& xp)
{
    rule* rp;
    for (rp = xp.rule; rp; rp = rp->next) {
        if (rp->precsym == nullptr) {
            int i, j;
            for (i = 0; i < rp->nrhs && rp->precsym == 0; i++) {
                symbol* sp = rp->rhs[i];
                if (sp->type == symbol_type::MULTITERMINAL) {
                    for (j = 0; j < sp->nsubsym; j++) {
                        if (sp->subsym[j]->prec >= 0) {
                            rp->precsym = sp->subsym[j];
                            break;
                        }
                    }
                }
                else if (sp->prec >= 0) {
                    rp->precsym = rp->rhs[i];
                }
            }
        }
    }
    return;
}

/* Find all nonterminals which will generate the empty string.
** Then go back and compute the first sets of every nonterminal.
** The first set is the set of all terminal symbols which can begin
** a string generated by that nonterminal.
*/
void FindFirstSets(lemon& lemp)
{
    int i, j;
    rule* rp;
    int progress;

    for (i = 0; i < lemp.nsymbol; i++) {
        lemp.symbols[i]->lambda = Boolean::LEMON_FALSE;
    }
    for (i = lemp.nterminal; i < lemp.nsymbol; i++) {
        lemp.symbols[i]->firstset = SetNew();
    }

    /* First compute all lambdas */
    do {
        progress = 0;
        for (rp = lemp.rule; rp; rp = rp->next) {
            if (rp->lhs->lambda == Boolean::LEMON_TRUE) continue;
            for (i = 0; i < rp->nrhs; i++) {
                const symbol* sp = rp->rhs[i];
                assert(sp->type == symbol_type::NONTERMINAL || sp->lambda == Boolean::LEMON_FALSE);
                if (sp->lambda == Boolean::LEMON_FALSE) break;
            }
            if (i == rp->nrhs) {
                rp->lhs->lambda = Boolean::LEMON_TRUE;
                progress = 1;
            }
        }
    } while (progress);

    /* Now compute all first sets */
    do {
        symbol* s1, * s2;
        progress = 0;
        for (rp = lemp.rule; rp; rp = rp->next) {
            s1 = rp->lhs;
            for (i = 0; i < rp->nrhs; i++) {
                s2 = rp->rhs[i];
                if (s2->type == symbol_type::TERMINAL) {
                    progress += SetAdd(s1->firstset, s2->index);
                    break;
                }
                else if (s2->type == symbol_type::MULTITERMINAL) {
                    for (j = 0; j < s2->nsubsym; j++) {
                        progress += SetAdd(s1->firstset, s2->subsym[j]->index);
                    }
                    break;
                }
                else if (s1 == s2) {
                    if (s1->lambda == Boolean::LEMON_FALSE) break;
                }
                else {
                    progress += SetUnion(s1->firstset, s2->firstset);
                    if (s2->lambda == Boolean::LEMON_FALSE) break;
                }
            }
        }
    } while (progress);
    return;
}

/* Compute all LR(0) states for the grammar.  Links
** are added to between some states so that the LR(1) follow sets
** can be computed later.
*/
PRIVATE state* getstate(lemon&);  /* forward reference */
void FindStates(lemon& lemp)
{
    symbol* sp;
    rule* rp;

    Configlist_init();

    /* Find the start symbol */
    if (lemp.start) {
        sp = Symbol_find(lemp.start);
        if (sp == nullptr) {
            ErrorMsg(lemp.filename, 0,
                "The specified start symbol \"%s\" is not "
                "in a nonterminal of the grammar.  \"%s\" will be used as the start "
                "symbol instead.", lemp.start, lemp.startRule->lhs->name);
            lemp.errorcnt++;
            sp = lemp.startRule->lhs;
        }
    }
    else {
        sp = lemp.startRule->lhs;
    }

    /* Make sure the start symbol doesn't occur on the right-hand side of
    ** any rule.  Report an error if it does.  (YACC would generate a new
    ** start symbol in this case.) */
    for (rp = lemp.rule; rp; rp = rp->next) {
        int i;
        for (i = 0; i < rp->nrhs; i++) {
            if (rp->rhs[i] == sp) {   /* FIX ME:  Deal with multiterminals */
                ErrorMsg(lemp.filename, 0,
                    "The start symbol \"%s\" occurs on the "
                    "right-hand side of a rule. This will result in a parser which "
                    "does not work properly.", sp->name);
                lemp.errorcnt++;
            }
        }
    }

    /* The basis configuration set for the first state
    ** is all rules which have the start symbol as their
    ** left-hand side */
    for (rp = sp->rule; rp; rp = rp->nextlhs) {
        config* newcfp;
        rp->lhsStart = 1;
        newcfp = Configlist_addbasis(rp, 0);
        SetAdd(newcfp->fws, 0);
    }

    /* Compute the first state.  All other states will be
    ** computed automatically during the computation of the first one.
    ** The returned pointer to the first state is not used. */
    (void)getstate(lemp);
    return;
}

/* Return a pointer to a state which is described by the configuration
** list which has been built from calls to Configlist_add.
*/
PRIVATE void buildshifts(lemon&, state*); /* Forwd ref */
PRIVATE state* getstate(lemon& lemp)
{
    config* cfp, * bp;
    state* stp;

    /* Extract the sorted basis of the new state.  The basis was constructed
    ** by prior calls to "Configlist_addbasis()". */
    Configlist_sortbasis();
    bp = Configlist_basis();

    /* Get a state with the same basis */
    stp = State_find(bp);
    if (stp) {
        /* A state with the same basis already exists!  Copy all the follow-set
        ** propagation links from the state under construction into the
        ** preexisting state, then return a pointer to the preexisting state */
        config* x, * y;
        for (x = bp, y = stp->bp; x && y; x = x->bp, y = y->bp) {
            Plink_copy(&y->bplp, x->bplp);
            Plink_delete(x->fplp);
            x->fplp = x->bplp = nullptr;
        }
        cfp = Configlist_return();
        Configlist_eat(cfp);
    }
    else {
        /* This really is a new state.  Construct all the details */
        Configlist_closure(lemp);    /* Compute the configuration closure */
        Configlist_sort();           /* Sort the configuration closure */
        cfp = Configlist_return();   /* Get a pointer to the config list */
        stp = State_new();           /* A new state structure */
        MemoryCheck(stp);
        stp->bp = bp;                /* Remember the configuration basis */
        stp->cfp = cfp;              /* Remember the configuration closure */
        stp->statenum = lemp.nstate++; /* Every state gets a sequence number */
        stp->ap = nullptr;                 /* No actions, yet. */
        State_insert(stp, stp->bp);   /* Add to the state table */
        buildshifts(lemp, stp);       /* Recursively compute successor states */
    }
    return stp;
}

/*
** Return true if two symbols are the same.
*/
int same_symbol(const symbol& a, const symbol& b)
{
    int i;
    if (&a == &b) return 1;
    if (a.type != symbol_type::MULTITERMINAL) return 0;
    if (b.type != symbol_type::MULTITERMINAL) return 0;
    if (a.nsubsym != b.nsubsym) return 0;
    for (i = 0; i < a.nsubsym; i++) {
        if (a.subsym[i] != b.subsym[i]) return 0;
    }
    return 1;
}

/* Construct all successor states to the given state.  A "successor"
** state is any state which can be reached by a shift action.
*/
PRIVATE void buildshifts(lemon& lemp, state* stp)
{
    config* cfp;  /* For looping thru the config closure of "stp" */
    config* bcfp; /* For the inner loop on config closure of "stp" */
    config* newcfg;  /* */
    symbol* sp;   /* Symbol following the dot in configuration "cfp" */
    symbol* bsp;  /* Symbol following the dot in configuration "bcfp" */
    state* newstp; /* A pointer to a successor state */

    /* Each configuration becomes complete after it contributes to a successor
    ** state.  Initially, all configurations are incomplete */
    for (cfp = stp->cfp; cfp; cfp = cfp->next) cfp->status = cfgstatus::INCOMPLETE;

    /* Loop through all configurations of the state "stp" */
    for (cfp = stp->cfp; cfp; cfp = cfp->next) {
        if (cfp->status == cfgstatus::COMPLETE) continue; /* Already used by inner loop */
        if (cfp->dot >= cfp->rp->nrhs) continue;          /* Can't shift this config */
        Configlist_reset();                               /* Reset the new config set */
        sp = cfp->rp->rhs[cfp->dot];                      /* Symbol after the dot */

        /* For every configuration in the state "stp" which has the symbol "sp"
        ** following its dot, add the same configuration to the basis set under
        ** construction but with the dot shifted one symbol to the right. */
        for (bcfp = cfp; bcfp; bcfp = bcfp->next) {
            if (bcfp->status == cfgstatus::COMPLETE) continue;    /* Already used */
            if (bcfp->dot >= bcfp->rp->nrhs) continue; /* Can't shift this one */
            bsp = bcfp->rp->rhs[bcfp->dot];           /* Get symbol after dot */
            if (!same_symbol(*bsp, *sp)) continue;      /* Must be same as for "cfp" */
            bcfp->status = cfgstatus::COMPLETE;                  /* Mark this config as used */
            newcfg = Configlist_addbasis(bcfp->rp, bcfp->dot + 1);
            Plink_add(&newcfg->bplp, bcfp);
        }

        /* Get a pointer to the state described by the basis configuration set
        ** constructed in the preceding loop */
        newstp = getstate(lemp);

        /* The state "newstp" is reached from the state "stp" by a shift action
        ** on the symbol "sp" */
        if (sp->type == symbol_type::MULTITERMINAL) {
            int i;
            for (i = 0; i < sp->nsubsym; i++) {
                Action_add(&stp->ap, e_action::SHIFT, sp->subsym[i], (char*)newstp);
            }
        }
        else {
            Action_add(&stp->ap, e_action::SHIFT, sp, (char*)newstp);
        }
    }
}

/*
** Construct the propagation links
*/
void FindLinks(lemon& lemp)
{
    int i;
    config* cfp, * other;
    state* stp;
    plink* plp;

    /* Housekeeping detail:
    ** Add to every propagate link a pointer back to the state to
    ** which the link is attached. */
    for (i = 0; i < lemp.nstate; i++) {
        stp = lemp.sorted[i];
        for (cfp = stp->cfp; cfp; cfp = cfp->next) {
            cfp->stp = stp;
        }
    }

    /* Convert all backlinks into forward links.  Only the forward
    ** links are used in the follow-set computation. */
    for (i = 0; i < lemp.nstate; i++) {
        stp = lemp.sorted[i];
        for (cfp = stp->cfp; cfp; cfp = cfp->next) {
            for (plp = cfp->bplp; plp; plp = plp->next) {
                other = plp->cfp;
                Plink_add(&other->fplp, cfp);
            }
        }
    }
}

/* Compute all followsets.
**
** A followset is the set of all symbols which can come immediately
** after a configuration.
*/
void FindFollowSets(lemon& lemp)
{
    int i;
    config* cfp;
    plink* plp;
    int progress;
    int change;

    for (i = 0; i < lemp.nstate; i++) {
        for (cfp = lemp.sorted[i]->cfp; cfp; cfp = cfp->next) {
            cfp->status = cfgstatus::INCOMPLETE;
        }
    }

    do {
        progress = 0;
        for (i = 0; i < lemp.nstate; i++) {
            for (cfp = lemp.sorted[i]->cfp; cfp; cfp = cfp->next) {
                if (cfp->status == cfgstatus::COMPLETE) continue;
                for (plp = cfp->fplp; plp; plp = plp->next) {
                    change = SetUnion(plp->cfp->fws, cfp->fws);
                    if (change) {
                        plp->cfp->status = cfgstatus::INCOMPLETE;
                        progress = 1;
                    }
                }
                cfp->status = cfgstatus::COMPLETE;
            }
        }
    } while (progress);
}

static int resolve_conflict(action*, action*);

/* Compute the reduce actions, and resolve conflicts.
*/
void FindActions(lemon& lemp)
{
    int i, j;
    config* cfp;
    state* stp;
    symbol* sp;
    rule* rp;

    /* Add all of the reduce actions
    ** A reduce action is added for each element of the followset of
    ** a configuration which has its dot at the extreme right.
    */
    for (i = 0; i < lemp.nstate; i++) {   /* Loop over all states */
        stp = lemp.sorted[i];
        for (cfp = stp->cfp; cfp; cfp = cfp->next) {  /* Loop over all configurations */
            if (cfp->rp->nrhs == cfp->dot) {        /* Is dot at extreme right? */
                for (j = 0; j < lemp.nterminal; j++) {
                    if (SetFind(cfp->fws, j)) {
                        /* Add a reduce action to the state "stp" which will reduce by the
                        ** rule "cfp->rp" if the lookahead symbol is "lemp.symbols[j]" */
                        Action_add(&stp->ap, e_action::REDUCE, lemp.symbols[j], (char*)cfp->rp);
                    }
                }
            }
        }
    }

    /* Add the accepting token */
    if (lemp.start) {
        sp = Symbol_find(lemp.start);
        if (sp == nullptr) sp = lemp.startRule->lhs;
    }
    else {
        sp = lemp.startRule->lhs;
    }
    /* Add to the first state (which is always the starting state of the
    ** finite state machine) an action to ACCEPT if the lookahead is the
    ** start nonterminal.  */
    Action_add(&lemp.sorted[0]->ap, e_action::ACCEPT, sp, 0);

    /* Resolve conflicts */
    for (i = 0; i < lemp.nstate; i++) {
        action* ap, * nap;
        stp = lemp.sorted[i];
        /* assert( stp->ap ); */
        stp->ap = Action_sort(stp->ap);
        for (ap = stp->ap; ap && ap->next; ap = ap->next) {
            for (nap = ap->next; nap && nap->sp == ap->sp; nap = nap->next) {
                /* The two actions "ap" and "nap" have the same lookahead.
                ** Figure out which one should be used */
                lemp.nconflict += resolve_conflict(ap, nap);
            }
        }
    }

    /* Report an error for each rule that can never be reduced. */
    for (rp = lemp.rule; rp; rp = rp->next) rp->canReduce = Boolean::LEMON_FALSE;
    for (i = 0; i < lemp.nstate; i++) {
        action* ap;
        for (ap = lemp.sorted[i]->ap; ap; ap = ap->next) {
            if (ap->type == e_action::REDUCE) ap->x.rp->canReduce = Boolean::LEMON_TRUE;
        }
    }
    for (rp = lemp.rule; rp; rp = rp->next) {
        if (rp->canReduce == Boolean::LEMON_TRUE) continue;
        ErrorMsg(lemp.filename, rp->ruleline, "This rule can not be reduced.\n");
        lemp.errorcnt++;
    }
}

/* Resolve a conflict between the two given actions.  If the
** conflict can't be resolved, return non-zero.
**
** NO LONGER TRUE:
**   To resolve a conflict, first look to see if either action
**   is on an error rule.  In that case, take the action which
**   is not associated with the error rule.  If neither or both
**   actions are associated with an error rule, then try to
**   use precedence to resolve the conflict.
**
** If either action is a SHIFT, then it must be apx.  This
** function won't work if apx->type==REDUCE and apy->type==SHIFT.
*/
static int resolve_conflict(
    action* apx,
    action* apy
) {
    symbol* spx, * spy;
    int errcnt = 0;
    assert(apx->sp == apy->sp);  /* Otherwise there would be no conflict */
    if (apx->type == e_action::SHIFT && apy->type == e_action::SHIFT) {
        apy->type = e_action::SSCONFLICT;
        errcnt++;
    }
    if (apx->type == e_action::SHIFT && apy->type == e_action::REDUCE) {
        spx = apx->sp;
        spy = apy->x.rp->precsym;
        if (spy == nullptr || spx->prec < 0 || spy->prec < 0) {
            /* Not enough precedence information. */
            apy->type = e_action::SRCONFLICT;
            errcnt++;
        }
        else if (spx->prec > spy->prec) {    /* higher precedence wins */
            apy->type = e_action::RD_RESOLVED;
        }
        else if (spx->prec < spy->prec) {
            apx->type = e_action::SH_RESOLVED;
        }
        else if (spx->prec == spy->prec && spx->assoc == e_assoc::RIGHT) { /* Use operator */
            apy->type = e_action::RD_RESOLVED;                             /* associativity */
        }
        else if (spx->prec == spy->prec && spx->assoc == e_assoc::LEFT) {  /* to break tie */
            apx->type = e_action::SH_RESOLVED;
        }
        else {
            assert(spx->prec == spy->prec && spx->assoc == e_assoc::NONE);
            apx->type = e_action::ERROR;
        }
    }
    else if (apx->type == e_action::REDUCE && apy->type == e_action::REDUCE) {
        spx = apx->x.rp->precsym;
        spy = apy->x.rp->precsym;
        if (spx == nullptr || spy == nullptr || spx->prec < 0 ||
            spy->prec < 0 || spx->prec == spy->prec) {
            apy->type = e_action::RRCONFLICT;
            errcnt++;
        }
        else if (spx->prec > spy->prec) {
            apy->type = e_action::RD_RESOLVED;
        }
        else if (spx->prec < spy->prec) {
            apx->type = e_action::RD_RESOLVED;
        }
    }
    else {
        assert(
            apx->type == e_action::SH_RESOLVED ||
            apx->type == e_action::RD_RESOLVED ||
            apx->type == e_action::SSCONFLICT ||
            apx->type == e_action::SRCONFLICT ||
            apx->type == e_action::RRCONFLICT ||
            apy->type == e_action::SH_RESOLVED ||
            apy->type == e_action::RD_RESOLVED ||
            apy->type == e_action::SSCONFLICT ||
            apy->type == e_action::SRCONFLICT ||
            apy->type == e_action::RRCONFLICT
        );
        /* The REDUCE/SHIFT case cannot happen because SHIFTs come before
        ** REDUCEs on the list.  If we reach this point it must be because
        ** the parser conflict had already been resolved. */
    }
    return errcnt;
}
/********************* From the file "configlist.c" *************************/
/*
** Routines to processing a configuration list and building a state
** in the LEMON parser generator.
*/

static config* freelist = nullptr;      /* List of free configurations */
static config* current = nullptr;       /* Top of list of configurations */
static config** currentend = nullptr;   /* Last on list of configs */
static config* basis = nullptr;         /* Top of list of basis configs */
static config** basisend = nullptr;     /* End of list of basis configs */

/* Return a pointer to a new configuration */
PRIVATE config* newconfig(void) {
    config* newcfg;
    if (freelist == nullptr) {
        int i;
        int amt = 3;
        freelist = (config*)calloc(amt, sizeof(config));
        if (freelist == nullptr) {
            fprintf(stderr, "Unable to allocate memory for a new configuration.");
            exit(1);
        }
        for (i = 0; i < amt - 1; i++) freelist[i].next = &freelist[i + 1];
        freelist[amt - 1].next = nullptr;
    }
    newcfg = freelist;
    freelist = freelist->next;
    return newcfg;
}

/* The configuration "old" is no longer used */
PRIVATE void deleteconfig(config* old)
{
    old->next = freelist;
    freelist = old;
}

namespace Configlist
{
/* Initialized the configuration list builder */
void Configlist_init(void) {
    current = nullptr;
    currentend = &current;
    basis = nullptr;
    basisend = &basis;
    Configtable_init();
    return;
}

/* Initialized the configuration list builder */
void Configlist_reset(void) {
    current = nullptr;
    currentend = &current;
    basis = nullptr;
    basisend = &basis;
    Configtable_clear(nullptr);
    return;
}

/* Add another configuration to the configuration list */
config* Configlist_add(
    Rule::rule* rp,    /* The rule */
    int dot             /* Index into the RHS of the rule where the dot goes */
) {
    config* cfp, model;

    assert(currentend != nullptr);
    model.rp = rp;
    model.dot = dot;
    cfp = Configtable_find(&model);
    if (cfp == nullptr) {
        cfp = newconfig();
        cfp->rp = rp;
        cfp->dot = dot;
        cfp->fws = SetNew();
        cfp->stp = nullptr;
        cfp->fplp = cfp->bplp = nullptr;
        cfp->next = nullptr;
        cfp->bp = nullptr;
        *currentend = cfp;
        currentend = &cfp->next;
        Configtable_insert(cfp);
    }
    return cfp;
}

/* Add a basis configuration to the configuration list */
config* Configlist_addbasis(Rule::rule* rp, int dot)
{
    config* cfp, model;

    assert(basisend != nullptr);
    assert(currentend != nullptr);
    model.rp = rp;
    model.dot = dot;
    cfp = Configtable_find(&model);
    if (cfp == nullptr) {
        cfp = newconfig();
        cfp->rp = rp;
        cfp->dot = dot;
        cfp->fws = SetNew();
        cfp->stp = nullptr;
        cfp->fplp = cfp->bplp = nullptr;
        cfp->next = nullptr;
        cfp->bp = nullptr;
        *currentend = cfp;
        currentend = &cfp->next;
        *basisend = cfp;
        basisend = &cfp->bp;
        Configtable_insert(cfp);
    }
    return cfp;
}

/* Compute the closure of the configuration list */
void Configlist_closure(lemon& lemp)
{
    config* cfp, * newcfp;
    rule* rp, * newrp;
    symbol* sp, * xsp;
    int i, dot;

    assert(currentend != nullptr);
    for (cfp = current; cfp; cfp = cfp->next) {
        rp = cfp->rp;
        dot = cfp->dot;
        if (dot >= rp->nrhs) continue;
        sp = rp->rhs[dot];
        if (sp->type == symbol_type::NONTERMINAL) {
            if (sp->rule == nullptr && sp != lemp.errsym) {
                ErrorMsg(lemp.filename, rp->line, "Nonterminal \"%s\" has no rules.",
                    sp->name);
                lemp.errorcnt++;
            }
            for (newrp = sp->rule; newrp; newrp = newrp->nextlhs) {
                newcfp = Configlist_add(newrp, 0);
                for (i = dot + 1; i < rp->nrhs; i++) {
                    xsp = rp->rhs[i];
                    if (xsp->type == symbol_type::TERMINAL) {
                        SetAdd(newcfp->fws, xsp->index);
                        break;
                    }
                    else if (xsp->type == symbol_type::MULTITERMINAL) {
                        int k;
                        for (k = 0; k < xsp->nsubsym; k++) {
                            SetAdd(newcfp->fws, xsp->subsym[k]->index);
                        }
                        break;
                    }
                    else {
                        SetUnion(newcfp->fws, xsp->firstset);
                        if (xsp->lambda == Boolean::LEMON_FALSE) break;
                    }
                }
                if (i == rp->nrhs) Plink_add(&cfp->fplp, newcfp);
            }
        }
    }
    return;
}

/* Sort the configuration list */
void Configlist_sort(void) {
    current = (config*)msort((char*)current, (const char**)&(current->next),
        Config::Configcmp);
    currentend = nullptr;
    return;
}

/* Sort the basis configuration list */
void Configlist_sortbasis(void) {
    basis = (config*)msort((char*)current, (const char**)&(current->bp),
        Config::Configcmp);
    basisend = nullptr;
    return;
}

/* Return a pointer to the head of the configuration list and
** reset the list */
config* Configlist_return(void) {
    config* old;
    old = current;
    current = nullptr;
    currentend = nullptr;
    return old;
}

/* Return a pointer to the head of the configuration list and
** reset the list */
config* Configlist_basis(void) {
    config* old;
    old = basis;
    basis = nullptr;
    basisend = nullptr;
    return old;
}

/* Free all elements of the given configuration list */
void Configlist_eat(config* cfp)
{
    config* nextcfp;
    for (; cfp; cfp = nextcfp) {
        nextcfp = cfp->next;
        assert(cfp->fplp == nullptr);
        assert(cfp->bplp == nullptr);
        if (cfp->fws) SetFree(cfp->fws);
        deleteconfig(cfp);
    }
    return;
}
}
/***************** From the file "error.c" *********************************/
/*
** Code for printing error message.
*/

void ErrorMsg(const char* filename, int lineno, const char* format, ...) {
    va_list ap;
    fprintf(stderr, "%s:%d: ", filename, lineno);
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
    fprintf(stderr, "\n");
}
/**************** From the file "main.c" ************************************/
/*
** Main program file for the LEMON parser generator.
*/

/* Report an out-of-memory condition and abort.  This function
** is used mostly by the "MemoryCheck" macro in struct.h
*/
void memory_error(void) {
    fprintf(stderr, "Out of memory.  Aborting...\n");
    exit(1);
}

static int nDefine = 0;      /* Number of -D options on the command line */
static char** azDefine = nullptr;  /* Name of the -D macros */

/* This routine is called with the argument to each -D command-line option.
** Add the macro defined to the azDefine array.
*/
static void handle_D_option(char* z) {
    char** paz;
    nDefine++;
    azDefine = (char**)realloc(azDefine, sizeof(azDefine[0]) * nDefine);
    if (azDefine == nullptr) {
        fprintf(stderr, "out of memory\n");
        exit(1);
    }
    paz = &azDefine[nDefine - 1];
    *paz = new char[lemonStrlen(z) + 1];
    if (*paz == nullptr) {
        fprintf(stderr, "out of memory\n");
        exit(1);
    }
    lemon_strcpy(*paz, z);
    for (z = *paz; *z && *z != '='; z++) {}
    *z = 0;
}

/* Rember the name of the output directory
*/
static char* outputDir = NULL;
static void handle_d_option(const char* z) {
    outputDir = new char[lemonStrlen(z) + 1]; //std::string or std::string_view
    if (outputDir == nullptr) {
        fprintf(stderr, "out of memory\n");
        exit(1);
    }
    lemon_strcpy(outputDir, z);
}

static char* user_templatename = NULL;
static void handle_T_option(const char* z) {
    user_templatename = new char[lemonStrlen(z) + 1];
    if (user_templatename == nullptr) {
        memory_error();
    }
    lemon_strcpy(user_templatename, z);
}

/* Merge together to lists of rules ordered by rule.iRule */
static rule* Rule_merge(rule* pA, rule* pB) {
    rule* pFirst = nullptr;
    rule** ppPrev = &pFirst;
    while (pA && pB) {
        if (pA->iRule < pB->iRule) {
            *ppPrev = pA;
            ppPrev = &pA->next;
            pA = pA->next;
        }
        else {
            *ppPrev = pB;
            ppPrev = &pB->next;
            pB = pB->next;
        }
    }
    if (pA) {
        *ppPrev = pA;
    }
    else {
        *ppPrev = pB;
    }
    return pFirst;
}

/*
** Sort a list of rules in order of increasing iRule value
*/
static rule* Rule_sort(rule* rp) {
    unsigned int i;
    rule* pNext;
    rule* x[32];
    memset(x, 0, sizeof(x));
    while (rp) {
        pNext = rp->next;
        rp->next = nullptr;
        for (i = 0; i < sizeof(x) / sizeof(x[0]) - 1 && x[i]; i++) {
            rp = Rule_merge(x[i], rp);
            x[i] = 0;
        }
        x[i] = rp;
        rp = pNext;
    }
    rp = nullptr;
    for (i = 0; i < sizeof(x) / sizeof(x[0]); i++) {
        rp = Rule_merge(x[i], rp);
    }
    return rp;
}

/* forward reference */
static const char* minimum_size_type(int lwr, int upr, int* pnByte);

/* Print a single line of the "Parser Stats" output
*/
static void stats_line(const char* zLabel, int iValue) {
    const int nLabel = lemonStrlen(zLabel);
    printf("  %s%.*s %5d\n", zLabel,
        35 - nLabel, "................................",
        iValue);
}

static char** g_argv;
static std::vector<s_options> op;
static FILE* errstream;


/* The main program.  Parse the command line and do it... */
int main(int argc, char** argv) {
    static int version = 0;
    static int rpflag = 0;
    static int basisflag = 0;
    static int compress = 0;
    static int quiet = 0;
    static int statistics = 0;
    static int mhflag = 0;
    static int nolinenosflag = 0;
    static int noResort = 0;
    static int sqlFlag = 0;
    static int printPP = 0;

    op = {
      {OPT_FLAG, "b", (char*)&basisflag, "Print only the basis in report."},
      {OPT_FLAG, "c", (char*)&compress, "Don't compress the action table."},
      {OPT_FSTR, "d", (char*)&handle_d_option, "Output directory.  Default '.'"},
      {OPT_FSTR, "D", (char*)handle_D_option, "Define an %ifdef macro."},
      {OPT_FLAG, "E", (char*)&printPP, "Print input file after preprocessing."},
      {OPT_FSTR, "f", 0, "Ignored.  (Placeholder for -f compiler options.)"},
      {OPT_FLAG, "g", (char*)&rpflag, "Print grammar without actions."},
      {OPT_FSTR, "I", 0, "Ignored.  (Placeholder for '-I' compiler options.)"},
      {OPT_FLAG, "m", (char*)&mhflag, "Output a makeheaders compatible file."},
      {OPT_FLAG, "l", (char*)&nolinenosflag, "Do not print #line statements."},
      {OPT_FSTR, "O", 0, "Ignored.  (Placeholder for '-O' compiler options.)"},
      {OPT_FLAG, "p", (char*)&showPrecedenceConflict,
                      "Show conflicts resolved by precedence rules"},
      {OPT_FLAG, "q", (char*)&quiet, "(Quiet) Don't print the report file."},
      {OPT_FLAG, "r", (char*)&noResort, "Do not sort or renumber states"},
      {OPT_FLAG, "s", (char*)&statistics,
                                     "Print parser stats to standard output."},
      {OPT_FLAG, "S", (char*)&sqlFlag,
                      "Generate the *.sql file describing the parser tables."},
      {OPT_FLAG, "x", (char*)&version, "Print the version number."},
      {OPT_FSTR, "T", (char*)handle_T_option, "Specify a template file."},
      {OPT_FSTR, "W", 0, "Ignored.  (Placeholder for '-W' compiler options.)"}
    };
    int i;
    int exitcode;
    lemon lem;
    rule* rp;

    (void)argc;
    OptInit(*argv);
    if (version) {
        printf("Lemon version 1.0\n");
        exit(0);
    }
    if (OptNArgs() != 1) {
        fprintf(stderr, "Exactly one filename argument is required.\n");
        exit(1);
    }
    memset(&lem, 0, sizeof(lem));
    lem.errorcnt = 0;

    /* Initialize the machine */
    Strsafe_init();
    Symbol_init();
    State_init();
    lem.argv0 = argv[0];
    lem.filename = OptArg(0);
    lem.basisflag = basisflag;
    lem.nolinenosflag = nolinenosflag;
    lem.printPreprocessed = printPP;
    Symbol_new("$");

    /* Parse the input file */
    Parse(lem);
    if (lem.printPreprocessed || lem.errorcnt) exit(lem.errorcnt);
    if (lem.nrule == 0) {
        fprintf(stderr, "Empty grammar.\n");
        exit(1);
    }
    lem.errsym = Symbol_find("error");

    /* Count and index the symbols of the grammar */
    auto default_nonterminal = Symbol_new("{default}");
    lem.nsymbol = Symbol_count();
    lem.symbols = Symbol_arrayof();

    std::sort(lem.symbols.begin(), lem.symbols.end(), &Symbolcmpp);

    std::for_each(lem.symbols.begin(), lem.symbols.end(), [i = 0](auto& sp) mutable {
        sp->index = i++;
    });

    //List: T T T T T ... T T T NT NT NT ... NT NT{{default}} MT MT MT 
    //                                                 ^ first_non_multiterminal
    //                          ^ first_nonterminal
    //Goal: Binary Search ->

    auto [begin_nonterminal, end_nonterminal] = std::equal_range(lem.symbols.begin(), lem.symbols.end(), default_nonterminal, [](const auto& x, const auto& y) {
        return static_cast<int> (x->type) < static_cast<int> (y->type);
        });

    lem.nsymbol = std::distance(lem.symbols.begin(), end_nonterminal) - 1;

    assert(strcmp(lem.symbols[lem.nsymbol]->name, "{default}") == 0);

    lem.nterminal = std::distance(lem.symbols.begin(), begin_nonterminal);

    /* Assign sequential rule numbers.  Start with 0.  Put rules that have no
    ** reduce action C-code associated with them last, so that the switch()
    ** statement that selects reduction actions will have a smaller jump table.
    */
    for (i = 0, rp = lem.rule; rp; rp = rp->next) {
        rp->iRule = rp->code ? i++ : -1;
    }
    lem.nruleWithAction = i;
    for (rp = lem.rule; rp; rp = rp->next) {
        if (rp->iRule < 0) rp->iRule = i++;
    }
    lem.startRule = lem.rule;
    lem.rule = Rule_sort(lem.rule);

    /* Generate a reprint of the grammar, if requested on the command line */
    if (rpflag) {
        Reprint(lem);
    }
    else {
        /* Initialize the size for all follow and first sets */
        SetSize(lem.nterminal + 1);

        /* Find the precedence for every production rule (that has one) */
        FindRulePrecedences(lem);

        /* Compute the lambda-nonterminals and the first-sets for every
        ** nonterminal */
        FindFirstSets(lem);

        /* Compute all LR(0) states.  Also record follow-set propagation
        ** links so that the follow-set can be computed later */
        lem.nstate = 0;
        FindStates(lem);
        lem.sorted = State_arrayof();

        /* Tie up loose ends on the propagation links */
        FindLinks(lem);

        /* Compute the follow set of every reducible configuration */
        FindFollowSets(lem);

        /* Compute the action tables */
        FindActions(lem);

        /* Compress the action tables */
        if (compress == 0) CompressTables(lem);

        /* Reorder and renumber the states so that states with fewer choices
        ** occur at the end.  This is an optimization that helps make the
        ** generated parser tables smaller. */
        if (noResort == 0) ResortStates(lem);

        /* Generate a report of the parser generated.  (the "y.output" file) */
        if (!quiet) ReportOutput(lem);

        /* Generate the source code for the parser */
        ReportTable(lem, mhflag, sqlFlag);

        /* Produce a header file for use by the scanner.  (This step is
        ** omitted if the "-m" option is used because makeheaders will
        ** generate the file for us.) */
        if (!mhflag) ReportHeader(lem);
    }
    if (statistics) {
        printf("Parser statistics:\n");
        stats_line("terminal symbols", lem.nterminal);
        stats_line("non-terminal symbols", lem.nsymbol - lem.nterminal);
        stats_line("total symbols", lem.nsymbol);
        stats_line("rules", lem.nrule);
        stats_line("states", lem.nxstate);
        stats_line("conflicts", lem.nconflict);
        stats_line("action table entries", lem.nactiontab);
        stats_line("lookahead table entries", lem.nlookaheadtab);
        stats_line("total table size (bytes)", lem.tablesize);
    }
    if (lem.nconflict > 0) {
        fprintf(stderr, "%d parsing conflicts.\n", lem.nconflict);
    }

    /* return 0 on success, 1 on failure. */
    exitcode = ((lem.errorcnt > 0) || (lem.nconflict > 0)) ? 1 : 0;
    exit(exitcode);
    return (exitcode);
}
/******************** From the file "msort.c" *******************************/
/*
** A generic merge-sort program.
**
** USAGE:
** Let "ptr" be a pointer to some structure which is at the head of
** a null-terminated list.  Then to sort the list call:
**
**     ptr = msort(ptr,&(ptr->next),cmpfnc);
**
** In the above, "cmpfnc" is a pointer to a function which compares
** two instances of the structure and returns an integer, as in
** strcmp.  The second argument is a pointer to the pointer to the
** second element of the linked list.  This address is used to compute
** the offset to the "next" field within the structure.  The offset to
** the "next" field must be constant for all structures in the list.
**
** The function returns a new pointer which is the head of the list
** after sorting.
**
** ALGORITHM:
** Merge-sort.
*/

/*
** Return a pointer to the next structure in the linked list.
*/
#define NEXT(A) (*(char**)(((char*)A)+offset))

/*
** Inputs:
**   a:       A sorted, null-terminated linked list.  (May be null).
**   b:       A sorted, null-terminated linked list.  (May be null).
**   cmp:     A pointer to the comparison function.
**   offset:  Offset in the structure to the "next" field.
**
** Return Value:
**   A pointer to the head of a sorted list containing the elements
**   of both a and b.
**
** Side effects:
**   The "next" pointers for elements in the lists a and b are
**   changed.
*/
static char* merge(
    char* a,
    char* b,
    int (*cmp)(const char*, const char*),
    int offset
) {
    char* ptr, * head;

    if (a == nullptr) {
        head = b;
    }
    else if (b == nullptr) {
        head = a;
    }
    else {
        if ((*cmp)(a, b) <= 0) {
            ptr = a;
            a = NEXT(a);
        }
        else {
            ptr = b;
            b = NEXT(b);
        }
        head = ptr;
        while (a && b) {
            if ((*cmp)(a, b) <= 0) {
                NEXT(ptr) = a;
                ptr = a;
                a = NEXT(a);
            }
            else {
                NEXT(ptr) = b;
                ptr = b;
                b = NEXT(b);
            }
        }
        if (a) NEXT(ptr) = a;
        else    NEXT(ptr) = b;
    }
    return head;
}

/*
** Inputs:
**   list:      Pointer to a singly-linked list of structures.
**   next:      Pointer to pointer to the second element of the list.
**   cmp:       A comparison function.
**
** Return Value:
**   A pointer to the head of a sorted list containing the elements
**   originally in list.
**
** Side effects:
**   The "next" pointers for elements in list are changed.
*/
#define LISTSIZE 30
static char* msort(
    char* list,
    const char** next,
    int (*cmp)(const char*, const char*)
) {
    unsigned long offset;
    char* ep;
    char* set[LISTSIZE];
    int i;
    offset = (unsigned long)((char*)next - (char*)list);
    for (i = 0; i < LISTSIZE; i++) set[i] = 0;
    while (list) {
        ep = list;
        list = NEXT(list);
        NEXT(ep) = nullptr;
        for (i = 0; i < LISTSIZE - 1 && set[i] != 0; i++) {
            ep = merge(ep, set[i], cmp, offset);
            set[i] = 0;
        }
        set[i] = ep;
    }
    ep = nullptr;
    for (i = 0; i < LISTSIZE; i++) if (set[i]) ep = merge(set[i], ep, cmp, offset);
    return ep;
}
/************************ From the file "option.c" **************************/


#define ISOPT(X) ((X)[0]=='-'||(X)[0]=='+'||strchr((X),'=')!=0)

/*
** Print the command line with a carrot pointing to the k-th character
** of the n-th field.
*/
static void errline(int n, int k)
{
    int spcnt, i;
    if (g_argv[0])
    {
        std::cerr << g_argv[0];
    }
    spcnt = lemonStrlen(g_argv[0]) + 1;
    for (i = 1; i < n && g_argv[i]; i++) {
        std::cerr << " " << g_argv[i];
        spcnt += lemonStrlen(g_argv[i]) + 1;
    }
    spcnt += k;
    for (; g_argv[i]; i++)
    {
        std::cerr << " " << g_argv[i];
    }
    if (spcnt < 20) {
        std::cerr << "\n" << std::setw(spcnt) << "" << "^-- here\n";
    }
    else {
        std::cerr << "\n" << std::setw(spcnt - 7) << "" << "here --^\n";
    }
}

/*
** Return the index of the N-th non-switch argument.  Return -1
** if N is out of range.
*/
static int argindex(int n)
{
    int i;
    int dashdash = 0;
    if (g_argv != nullptr && *g_argv != nullptr) {
        for (i = 1; g_argv[i]; i++) {
            if (dashdash || !ISOPT(g_argv[i])) {
                if (n == 0) return i;
                n--;
            }
            if (strcmp(g_argv[i], "--") == 0) dashdash = 1;
        }
    }
    return -1;
}

static char emsg[] = "Command line syntax error: ";

/*
** Process a flag command line argument.
*/
static int handleflags(int i)
{
    int v;
    int errcnt = 0;
    int j;
    for (j = 0; j < op.size(); j++) {
        if (strncmp(&g_argv[i][1], op[j].label, lemonStrlen(op[j].label)) == 0) break;
    }
    v = g_argv[i][0] == '-' ? 1 : 0;
    if (op[j].label == nullptr) {
        std::cerr << emsg << "undefined option.\n";
        errline(i, 1);
        errcnt++;
    }
    else if (op[j].arg == nullptr) {
        /* Ignore this option */
    }
    else if (op[j].type == OPT_FLAG) {
        *((int*)op[j].arg) = v;
    }
    else if (op[j].type == OPT_FFLAG) {
        (*(void(*)(int))(op[j].arg))(v);
    }
    else if (op[j].type == OPT_FSTR) {
        (*(void(*)(char*))(op[j].arg))(&g_argv[i][2]);
    }
    else {
        std::cerr << emsg << "missing argument on switch.\n";
        errline(i, 1);

        errcnt++;
    }
    return errcnt;
}

/*
** Process a command line switch which has an argument.
*/
static int handleswitch(int i)
{
    int lv = 0;
    double dv = 0.0;
    char* sv = nullptr, * end;
    char* cp;
    int j;
    int errcnt = 0;
    cp = strchr(g_argv[i], '=');
    assert(cp != nullptr);
    *cp = 0;
    for (j = 0; j < op.size(); j++) {
        if (strcmp(g_argv[i], op[j].label) == 0) break;
    }
    *cp = '=';
    if (op[j].label == nullptr) {
        std::cerr << emsg << "undefined option.\n";
        errline(i, 0);

        errcnt++;
    }
    else {
        cp++;
        switch (op[j].type) {
        case OPT_FLAG:
        case OPT_FFLAG:
            std::cerr << emsg << "option requires an argument.\n";
            errline(i, 0);

            errcnt++;
            break;
        case OPT_DBL:
        case OPT_FDBL:
            dv = strtod(cp, &end);
            if (*end) {
                std::cerr << emsg << "illegal character in floating-point argument.\n";
                errline(i, (int)((char*)end - (char*)g_argv[i]));

                errcnt++;
            }
            break;
        case OPT_INT:
        case OPT_FINT:
            lv = strtol(cp, &end, 0);
            if (*end) {

                    std::cerr << emsg << "illegal character in integer argument.\n";
                    errline(i, (int)((char*)end - (char*)g_argv[i]));
                errcnt++;
            }
            break;
        case OPT_STR:
        case OPT_FSTR:
            sv = cp;
            break;
        }
        switch (op[j].type) {
        case OPT_FLAG:
        case OPT_FFLAG:
            break;
        case OPT_DBL:
            *(double*)(op[j].arg) = dv;
            break;
        case OPT_FDBL:
            (*(void(*)(double))(op[j].arg))(dv);
            break;
        case OPT_INT:
            *(int*)(op[j].arg) = lv;
            break;
        case OPT_FINT:
            (*(void(*)(int))(op[j].arg))((int)lv);
            break;
        case OPT_STR:
            *(char**)(op[j].arg) = sv;
            break;
        case OPT_FSTR:
            (*(void(*)(char*))(op[j].arg))(sv);
            break;
        }
    }
    return errcnt;
}

int OptInit(char*& a)
{
    int errcnt = 0;
    g_argv = &a;
    if (g_argv && *g_argv) {
        int i;
        for (i = 1; g_argv[i]; i++) {
            if (g_argv[i][0] == '+' || g_argv[i][0] == '-') {
                errcnt += handleflags(i);
            }
            else if (strchr(g_argv[i], '=')) {
                errcnt += handleswitch(i);
            }
        }
    }
    if (errcnt > 0) {
        std::cerr << "Valid command line options for \"" << a << "\" are:\n";
        OptPrint();
        exit(1);
    }
    return 0;
}

int OptNArgs(void) {
    int cnt = 0;
    int dashdash = 0;
    int i;
    if (g_argv != nullptr && g_argv[0] != nullptr) {
        for (i = 1; g_argv[i]; i++) {
            if (dashdash || !ISOPT(g_argv[i])) cnt++;
            if (strcmp(g_argv[i], "--") == 0) dashdash = 1;
        }
    }
    return cnt;
}

char* OptArg(int n)
{
    int i;
    i = argindex(n);
    return i >= 0 ? g_argv[i] : nullptr;
}

void OptErr(int n)
{
    int i;
    i = argindex(n);
    if (i >= 0) errline(i, 0);
}

void OptPrint() {

    auto type_name = [](auto type) {
        switch (type) {
        case OPT_FLAG:
        case OPT_FFLAG:
            return "";
        //case OPT_INT:
        //case OPT_FINT:
        //    return "<integer>";
        //case OPT_DBL:
        //case OPT_FDBL:
        //    return "<real>";
        case OPT_STR:
        case OPT_FSTR:
            return "<string>";
        }

        return "";
    };

    size_t max = std::transform_reduce(op.begin(), op.end(), 0u, [](auto x, auto y) { return std::max(x, y); }, [&](const auto& e) {
            return strlen(e.label) + strlen(type_name(e.type));
        });

    std::for_each(op.begin(), op.end(), [&](const auto& e) {
        std::cerr << "  -" << e.label << std::setw(max - strlen(e.label)) << type_name(e.type) << "   " << e.message << "\n";
    });
}
/*********************** From the file "parse.c" ****************************/
/*
** Input file parser for the LEMON parser generator.
*/

/* The state of the parser */
enum class e_state {
    INITIALIZE,
    WAITING_FOR_DECL_OR_RULE,
    WAITING_FOR_DECL_KEYWORD,
    WAITING_FOR_DECL_ARG,
    WAITING_FOR_PRECEDENCE_SYMBOL,
    WAITING_FOR_ARROW,
    IN_RHS,
    LHS_ALIAS_1,
    LHS_ALIAS_2,
    LHS_ALIAS_3,
    RHS_ALIAS_1,
    RHS_ALIAS_2,
    PRECEDENCE_MARK_1,
    PRECEDENCE_MARK_2,
    RESYNC_AFTER_RULE_ERROR,
    RESYNC_AFTER_DECL_ERROR,
    WAITING_FOR_DESTRUCTOR_SYMBOL,
    WAITING_FOR_DATATYPE_SYMBOL,
    WAITING_FOR_FALLBACK_ID,
    WAITING_FOR_WILDCARD_ID,
    WAITING_FOR_CLASS_ID,
    WAITING_FOR_CLASS_TOKEN,
    WAITING_FOR_TOKEN_NAME
};
struct pstate {
    char* filename;       /* Name of the input file */
    int tokenlineno;      /* Linenumber at which current token starts */
    int errorcnt;         /* Number of errors so far */
    char* tokenstart;     /* Text of current token */
    lemon* gp;     /* Global state vector */
    e_state state;        /* The state of the parser */
    symbol* fallback;   /* The fallback token */
    symbol* tkclass;    /* Token class symbol */
    symbol* lhs;        /* Left-hand side of current rule */
    const char* lhsalias;      /* Alias for the LHS */
    int nrhs;                  /* Number of right-hand side symbols seen */
    symbol* rhs[MAXRHS];  /* RHS symbols */
    const char* alias[MAXRHS]; /* Aliases for each RHS symbol (or NULL) */
    rule* prevrule;     /* Previous rule parsed */
    const char* declkeyword;   /* Keyword of a declaration */
    char** declargslot;        /* Where the declaration argument should be put */
    int insertLineMacro;       /* Add #line before declaration insert */
    int* decllinenoslot;       /* Where to write declaration line number */
    e_assoc declassoc;    /* Assign this association to decl arguments */
    int preccounter;           /* Assign this precedence to decl arguments */
    rule* firstrule;    /* Pointer to first rule in the grammar */
    rule* lastrule;     /* Pointer to the most recently parsed rule */
};

/* Parse a single token */
static void parseonetoken(pstate* psp)
{
    const char* x;
    x = Strsafe(psp->tokenstart);     /* Save the token permanently */
#if 0
    printf("%s:%d: Token=[%s] state=%d\n", psp->filename, psp->tokenlineno,
        x, psp->state);
#endif
    switch (psp->state) {
    case e_state::INITIALIZE:
        psp->prevrule = nullptr;
        psp->preccounter = 0;
        psp->firstrule = psp->lastrule = nullptr;
        psp->gp->nrule = 0;
        [[fallthrough]];
    case e_state::WAITING_FOR_DECL_OR_RULE:
        if (x[0] == '%') {
            psp->state = e_state::WAITING_FOR_DECL_KEYWORD;
        }
        else if (ISLOWER(x[0])) {
            psp->lhs = Symbol_new(x);
            psp->nrhs = 0;
            psp->lhsalias = nullptr;
            psp->state = e_state::WAITING_FOR_ARROW;
        }
        else if (x[0] == '{') {
            if (psp->prevrule == nullptr) {
                ErrorMsg(psp->filename, psp->tokenlineno,
                    "There is no prior rule upon which to attach the code "
                    "fragment which begins on this line.");
                psp->errorcnt++;
            }
            else if (psp->prevrule->code != nullptr) {
                ErrorMsg(psp->filename, psp->tokenlineno,
                    "Code fragment beginning on this line is not the first "
                    "to follow the previous rule.");
                psp->errorcnt++;
            }
            else if (strcmp(x, "{NEVER-REDUCE") == 0) {
                psp->prevrule->neverReduce = Boolean::LEMON_TRUE;
            }
            else {
                psp->prevrule->line = psp->tokenlineno;
                psp->prevrule->code = &x[1];
                psp->prevrule->noCode = Boolean::LEMON_FALSE;
            }
        }
        else if (x[0] == '[') {
            psp->state = e_state::PRECEDENCE_MARK_1;
        }
        else {
            ErrorMsg(psp->filename, psp->tokenlineno,
                "Token \"%s\" should be either \"%%\" or a nonterminal name.",
                x);
            psp->errorcnt++;
        }
        break;
    case e_state::PRECEDENCE_MARK_1:
        if (!ISUPPER(x[0])) {
            ErrorMsg(psp->filename, psp->tokenlineno,
                "The precedence symbol must be a terminal.");
            psp->errorcnt++;
        }
        else if (psp->prevrule == nullptr) {
            ErrorMsg(psp->filename, psp->tokenlineno,
                "There is no prior rule to assign precedence \"[%s]\".", x);
            psp->errorcnt++;
        }
        else if (psp->prevrule->precsym != nullptr) {
            ErrorMsg(psp->filename, psp->tokenlineno,
                "Precedence mark on this line is not the first "
                "to follow the previous rule.");
            psp->errorcnt++;
        }
        else {
            psp->prevrule->precsym = Symbol_new(x);
        }
        psp->state = e_state::PRECEDENCE_MARK_2;
        break;
    case e_state::PRECEDENCE_MARK_2:
        if (x[0] != ']') {
            ErrorMsg(psp->filename, psp->tokenlineno,
                "Missing \"]\" on precedence mark.");
            psp->errorcnt++;
        }
        psp->state = e_state::WAITING_FOR_DECL_OR_RULE;
        break;
    case e_state::WAITING_FOR_ARROW:
        if (x[0] == ':' && x[1] == ':' && x[2] == '=') {
            psp->state = e_state::IN_RHS;
        }
        else if (x[0] == '(') {
            psp->state = e_state::LHS_ALIAS_1;
        }
        else {
            ErrorMsg(psp->filename, psp->tokenlineno,
                "Expected to see a \":\" following the LHS symbol \"%s\".",
                psp->lhs->name);
            psp->errorcnt++;
            psp->state = e_state::RESYNC_AFTER_RULE_ERROR;
        }
        break;
    case e_state::LHS_ALIAS_1:
        if (ISALPHA(x[0])) {
            psp->lhsalias = x;
            psp->state = e_state::LHS_ALIAS_2;
        }
        else {
            ErrorMsg(psp->filename, psp->tokenlineno,
                "\"%s\" is not a valid alias for the LHS \"%s\"\n",
                x, psp->lhs->name);
            psp->errorcnt++;
            psp->state = e_state::RESYNC_AFTER_RULE_ERROR;
        }
        break;
    case e_state::LHS_ALIAS_2:
        if (x[0] == ')') {
            psp->state = e_state::LHS_ALIAS_3;
        }
        else {
            ErrorMsg(psp->filename, psp->tokenlineno,
                "Missing \")\" following LHS alias name \"%s\".", psp->lhsalias);
            psp->errorcnt++;
            psp->state = e_state::RESYNC_AFTER_RULE_ERROR;
        }
        break;
    case e_state::LHS_ALIAS_3:
        if (x[0] == ':' && x[1] == ':' && x[2] == '=') {
            psp->state = e_state::IN_RHS;
        }
        else {
            ErrorMsg(psp->filename, psp->tokenlineno,
                "Missing \"->\" following: \"%s(%s)\".",
                psp->lhs->name, psp->lhsalias);
            psp->errorcnt++;
            psp->state = e_state::RESYNC_AFTER_RULE_ERROR;
        }
        break;
    case e_state::IN_RHS:
        if (x[0] == '.') {
            rule* rp;
            rp = (rule*)calloc(sizeof(rule) +
                sizeof(symbol*) * psp->nrhs + sizeof(char*) * psp->nrhs, 1);
            if (rp == nullptr) {
                ErrorMsg(psp->filename, psp->tokenlineno,
                    "Can't allocate enough memory for this rule.");
                psp->errorcnt++;
                psp->prevrule = nullptr;
            }
            else {
                int i;
                rp->ruleline = psp->tokenlineno;
                rp->rhs = (symbol**)&rp[1];
                rp->rhsalias = (const char**)&(rp->rhs[psp->nrhs]);
                for (i = 0; i < psp->nrhs; i++) {
                    rp->rhs[i] = psp->rhs[i];
                    rp->rhsalias[i] = psp->alias[i];
                    if (rp->rhsalias[i] != nullptr) { rp->rhs[i]->bContent = 1; }
                }
                rp->lhs = psp->lhs;
                rp->lhsalias = psp->lhsalias;
                rp->nrhs = psp->nrhs;
                rp->code = nullptr;
                rp->noCode = Boolean::LEMON_TRUE;
                rp->precsym = nullptr;
                rp->index = psp->gp->nrule++;
                rp->nextlhs = rp->lhs->rule;
                rp->lhs->rule = rp;
                rp->next = nullptr;
                if (psp->firstrule == nullptr) {
                    psp->firstrule = psp->lastrule = rp;
                }
                else {
                    psp->lastrule->next = rp;
                    psp->lastrule = rp;
                }
                psp->prevrule = rp;
            }
            psp->state = e_state::WAITING_FOR_DECL_OR_RULE;
        }
        else if (ISALPHA(x[0])) {
            if (psp->nrhs >= MAXRHS) {
                ErrorMsg(psp->filename, psp->tokenlineno,
                    "Too many symbols on RHS of rule beginning at \"%s\".",
                    x);
                psp->errorcnt++;
                psp->state = e_state::RESYNC_AFTER_RULE_ERROR;
            }
            else {
                psp->rhs[psp->nrhs] = Symbol_new(x);
                psp->alias[psp->nrhs] = 0;
                psp->nrhs++;
            }
        }
        else if ((x[0] == '|' || x[0] == '/') && psp->nrhs > 0 && ISUPPER(x[1])) {
            symbol* msp = psp->rhs[psp->nrhs - 1];
            if (msp->type != symbol_type::MULTITERMINAL) {
                symbol* origsp = msp;
                msp = (symbol*)calloc(1, sizeof(*msp));
                memset(msp, 0, sizeof(*msp));
                msp->type = symbol_type::MULTITERMINAL;
                msp->nsubsym = 1;
                msp->subsym = (symbol**)calloc(1, sizeof(symbol*));
                msp->subsym[0] = origsp;
                msp->name = origsp->name;
                psp->rhs[psp->nrhs - 1] = msp;
            }
            msp->nsubsym++;
            msp->subsym = (symbol**)realloc(msp->subsym,
                sizeof(symbol*) * msp->nsubsym);
            msp->subsym[msp->nsubsym - 1] = Symbol_new(&x[1]);
            if (ISLOWER(x[1]) || ISLOWER(msp->subsym[0]->name[0])) {
                ErrorMsg(psp->filename, psp->tokenlineno,
                    "Cannot form a compound containing a non-terminal");
                psp->errorcnt++;
            }
        }
        else if (x[0] == '(' && psp->nrhs > 0) {
            psp->state = e_state::RHS_ALIAS_1;
        }
        else {
            ErrorMsg(psp->filename, psp->tokenlineno,
                "Illegal character on RHS of rule: \"%s\".", x);
            psp->errorcnt++;
            psp->state = e_state::RESYNC_AFTER_RULE_ERROR;
        }
        break;
    case e_state::RHS_ALIAS_1:
        if (ISALPHA(x[0])) {
            psp->alias[psp->nrhs - 1] = x;
            psp->state = e_state::RHS_ALIAS_2;
        }
        else {
            ErrorMsg(psp->filename, psp->tokenlineno,
                "\"%s\" is not a valid alias for the RHS symbol \"%s\"\n",
                x, psp->rhs[psp->nrhs - 1]->name);
            psp->errorcnt++;
            psp->state = e_state::RESYNC_AFTER_RULE_ERROR;
        }
        break;
    case e_state::RHS_ALIAS_2:
        if (x[0] == ')') {
            psp->state = e_state::IN_RHS;
        }
        else {
            ErrorMsg(psp->filename, psp->tokenlineno,
                "Missing \")\" following LHS alias name \"%s\".", psp->lhsalias);
            psp->errorcnt++;
            psp->state = e_state::RESYNC_AFTER_RULE_ERROR;
        }
        break;
    case e_state::WAITING_FOR_DECL_KEYWORD:
        if (ISALPHA(x[0])) {
            psp->declkeyword = x;
            psp->declargslot = nullptr;
            psp->decllinenoslot = nullptr;
            psp->insertLineMacro = 1;
            psp->state = e_state::WAITING_FOR_DECL_ARG;
            if (strcmp(x, "name") == 0) {
                psp->declargslot = &(psp->gp->name);
                psp->insertLineMacro = 0;
            }
            else if (strcmp(x, "include") == 0) {
                psp->declargslot = &(psp->gp->include);
            }
            else if (strcmp(x, "code") == 0) {
                psp->declargslot = &(psp->gp->extracode);
            }
            else if (strcmp(x, "token_destructor") == 0) {
                psp->declargslot = &psp->gp->tokendest;
            }
            else if (strcmp(x, "default_destructor") == 0) {
                psp->declargslot = &psp->gp->vardest;
            }
            else if (strcmp(x, "token_prefix") == 0) {
                psp->declargslot = &psp->gp->tokenprefix;
                psp->insertLineMacro = 0;
            }
            else if (strcmp(x, "syntax_error") == 0) {
                psp->declargslot = &(psp->gp->error);
            }
            else if (strcmp(x, "parse_accept") == 0) {
                psp->declargslot = &(psp->gp->accept);
            }
            else if (strcmp(x, "parse_failure") == 0) {
                psp->declargslot = &(psp->gp->failure);
            }
            else if (strcmp(x, "stack_overflow") == 0) {
                psp->declargslot = &(psp->gp->overflow);
            }
            else if (strcmp(x, "extra_argument") == 0) {
                psp->declargslot = &(psp->gp->arg);
                psp->insertLineMacro = 0;
            }
            else if (strcmp(x, "extra_context") == 0) {
                psp->declargslot = &(psp->gp->ctx);
                psp->insertLineMacro = 0;
            }
            else if (strcmp(x, "token_type") == 0) {
                psp->declargslot = &(psp->gp->tokentype);
                psp->insertLineMacro = 0;
            }
            else if (strcmp(x, "default_type") == 0) {
                psp->declargslot = &(psp->gp->vartype);
                psp->insertLineMacro = 0;
            }
            else if (strcmp(x, "stack_size") == 0) {
                psp->declargslot = &(psp->gp->stacksize);
                psp->insertLineMacro = 0;
            }
            else if (strcmp(x, "start_symbol") == 0) {
                psp->declargslot = &(psp->gp->start);
                psp->insertLineMacro = 0;
            }
            else if (strcmp(x, "left") == 0) {
                psp->preccounter++;
                psp->declassoc = e_assoc::LEFT;
                psp->state = e_state::WAITING_FOR_PRECEDENCE_SYMBOL;
            }
            else if (strcmp(x, "right") == 0) {
                psp->preccounter++;
                psp->declassoc = e_assoc::RIGHT;
                psp->state = e_state::WAITING_FOR_PRECEDENCE_SYMBOL;
            }
            else if (strcmp(x, "nonassoc") == 0) {
                psp->preccounter++;
                psp->declassoc = e_assoc::NONE;
                psp->state = e_state::WAITING_FOR_PRECEDENCE_SYMBOL;
            }
            else if (strcmp(x, "destructor") == 0) {
                psp->state = e_state::WAITING_FOR_DESTRUCTOR_SYMBOL;
            }
            else if (strcmp(x, "type") == 0) {
                psp->state = e_state::WAITING_FOR_DATATYPE_SYMBOL;
            }
            else if (strcmp(x, "fallback") == 0) {
                psp->fallback = 0;
                psp->state = e_state::WAITING_FOR_FALLBACK_ID;
            }
            else if (strcmp(x, "token") == 0) {
                psp->state = e_state::WAITING_FOR_TOKEN_NAME;
            }
            else if (strcmp(x, "wildcard") == 0) {
                psp->state = e_state::WAITING_FOR_WILDCARD_ID;
            }
            else if (strcmp(x, "token_class") == 0) {
                psp->state = e_state::WAITING_FOR_CLASS_ID;
            }
            else {
                ErrorMsg(psp->filename, psp->tokenlineno,
                    "Unknown declaration keyword: \"%%%s\".", x);
                psp->errorcnt++;
                psp->state = e_state::RESYNC_AFTER_DECL_ERROR;
            }
        }
        else {
            ErrorMsg(psp->filename, psp->tokenlineno,
                "Illegal declaration keyword: \"%s\".", x);
            psp->errorcnt++;
            psp->state = e_state::RESYNC_AFTER_DECL_ERROR;
        }
        break;
    case e_state::WAITING_FOR_DESTRUCTOR_SYMBOL:
        if (!ISALPHA(x[0])) {
            ErrorMsg(psp->filename, psp->tokenlineno,
                "Symbol name missing after %%destructor keyword");
            psp->errorcnt++;
            psp->state = e_state::RESYNC_AFTER_DECL_ERROR;
        }
        else {
            symbol* sp = Symbol_new(x);
            psp->declargslot = &sp->destructor;
            psp->decllinenoslot = &sp->destLineno;
            psp->insertLineMacro = 1;
            psp->state = e_state::WAITING_FOR_DECL_ARG;
        }
        break;
    case e_state::WAITING_FOR_DATATYPE_SYMBOL:
        if (!ISALPHA(x[0])) {
            ErrorMsg(psp->filename, psp->tokenlineno,
                "Symbol name missing after %%type keyword");
            psp->errorcnt++;
            psp->state = e_state::RESYNC_AFTER_DECL_ERROR;
        }
        else {
            symbol* sp = Symbol_find(x);
            if ((sp) && (sp->datatype)) {
                ErrorMsg(psp->filename, psp->tokenlineno,
                    "Symbol %%type \"%s\" already defined", x);
                psp->errorcnt++;
                psp->state = e_state::RESYNC_AFTER_DECL_ERROR;
            }
            else {
                if (!sp) {
                    sp = Symbol_new(x);
                }
                psp->declargslot = &sp->datatype;
                psp->insertLineMacro = 0;
                psp->state = e_state::WAITING_FOR_DECL_ARG;
            }
        }
        break;
    case e_state::WAITING_FOR_PRECEDENCE_SYMBOL:
        if (x[0] == '.') {
            psp->state = e_state::WAITING_FOR_DECL_OR_RULE;
        }
        else if (ISUPPER(x[0])) {
            symbol* sp;
            sp = Symbol_new(x);
            if (sp->prec >= 0) {
                ErrorMsg(psp->filename, psp->tokenlineno,
                    "Symbol \"%s\" has already be given a precedence.", x);
                psp->errorcnt++;
            }
            else {
                sp->prec = psp->preccounter;
                sp->assoc = psp->declassoc;
            }
        }
        else {
            ErrorMsg(psp->filename, psp->tokenlineno,
                "Can't assign a precedence to \"%s\".", x);
            psp->errorcnt++;
        }
        break;
    case e_state::WAITING_FOR_DECL_ARG:
        if (x[0] == '{' || x[0] == '\"' || ISALNUM(x[0])) {
            const char* zOld, * zNew;
            char* zBuf, * z;
            int nOld, n, nLine = 0, nNew, nBack;
            int addLineMacro;
            char zLine[50];
            zNew = x;
            if (zNew[0] == '"' || zNew[0] == '{') zNew++;
            nNew = lemonStrlen(zNew);
            if (*psp->declargslot) {
                zOld = *psp->declargslot;
            }
            else {
                zOld = "";
            }
            nOld = lemonStrlen(zOld);
            n = nOld + nNew + 20;
            addLineMacro = !psp->gp->nolinenosflag
                && psp->insertLineMacro
                && psp->tokenlineno > 1
                && (psp->decllinenoslot == nullptr || psp->decllinenoslot[0] != 0);
            if (addLineMacro) {
                for (z = psp->filename, nBack = 0; *z; z++) {
                    if (*z == '\\') nBack++;
                }
                lemon_sprintf(zLine, "#line %d ", psp->tokenlineno);
                nLine = lemonStrlen(zLine);
                n += nLine + lemonStrlen(psp->filename) + nBack;
            }
            *psp->declargslot = (char*)realloc(*psp->declargslot, n);
            zBuf = *psp->declargslot + nOld;
            if (addLineMacro) {
                if (nOld && zBuf[-1] != '\n') {
                    *(zBuf++) = '\n';
                }
                memcpy(zBuf, zLine, nLine);
                zBuf += nLine;
                *(zBuf++) = '"';
                for (z = psp->filename; *z; z++) {
                    if (*z == '\\') {
                        *(zBuf++) = '\\';
                    }
                    *(zBuf++) = *z;
                }
                *(zBuf++) = '"';
                *(zBuf++) = '\n';
            }
            if (psp->decllinenoslot && psp->decllinenoslot[0] == 0) {
                psp->decllinenoslot[0] = psp->tokenlineno;
            }
            memcpy(zBuf, zNew, nNew);
            zBuf += nNew;
            *zBuf = 0;
            psp->state = e_state::WAITING_FOR_DECL_OR_RULE;
        }
        else {
            ErrorMsg(psp->filename, psp->tokenlineno,
                "Illegal argument to %%%s: %s", psp->declkeyword, x);
            psp->errorcnt++;
            psp->state = e_state::RESYNC_AFTER_DECL_ERROR;
        }
        break;
    case e_state::WAITING_FOR_FALLBACK_ID:
        if (x[0] == '.') {
            psp->state = e_state::WAITING_FOR_DECL_OR_RULE;
        }
        else if (!ISUPPER(x[0])) {
            ErrorMsg(psp->filename, psp->tokenlineno,
                "%%fallback argument \"%s\" should be a token", x);
            psp->errorcnt++;
        }
        else {
            symbol* sp = Symbol_new(x);
            if (psp->fallback == nullptr) {
                psp->fallback = sp;
            }
            else if (sp->fallback) {
                ErrorMsg(psp->filename, psp->tokenlineno,
                    "More than one fallback assigned to token %s", x);
                psp->errorcnt++;
            }
            else {
                sp->fallback = psp->fallback;
                psp->gp->has_fallback = 1;
            }
        }
        break;
    case e_state::WAITING_FOR_TOKEN_NAME:
        /* Tokens do not have to be declared before use.  But they can be
        ** in order to control their assigned integer number.  The number for
        ** each token is assigned when it is first seen.  So by including
        **
        **     %token ONE TWO THREE
        **
        ** early in the grammar file, that assigns small consecutive values
        ** to each of the tokens ONE TWO and THREE.
        */
        if (x[0] == '.') {
            psp->state = e_state::WAITING_FOR_DECL_OR_RULE;
        }
        else if (!ISUPPER(x[0])) {
            ErrorMsg(psp->filename, psp->tokenlineno,
                "%%token argument \"%s\" should be a token", x);
            psp->errorcnt++;
        }
        else {
            (void)Symbol_new(x);
        }
        break;
    case e_state::WAITING_FOR_WILDCARD_ID:
        if (x[0] == '.') {
            psp->state = e_state::WAITING_FOR_DECL_OR_RULE;
        }
        else if (!ISUPPER(x[0])) {
            ErrorMsg(psp->filename, psp->tokenlineno,
                "%%wildcard argument \"%s\" should be a token", x);
            psp->errorcnt++;
        }
        else {
            symbol* sp = Symbol_new(x);
            if (psp->gp->wildcard == nullptr) {
                psp->gp->wildcard = sp;
            }
            else {
                ErrorMsg(psp->filename, psp->tokenlineno,
                    "Extra wildcard to token: %s", x);
                psp->errorcnt++;
            }
        }
        break;
    case e_state::WAITING_FOR_CLASS_ID:
        if (!ISLOWER(x[0])) {
            ErrorMsg(psp->filename, psp->tokenlineno,
                "%%token_class must be followed by an identifier: %s", x);
            psp->errorcnt++;
            psp->state = e_state::RESYNC_AFTER_DECL_ERROR;
        }
        else if (Symbol_find(x)) {
            ErrorMsg(psp->filename, psp->tokenlineno,
                "Symbol \"%s\" already used", x);
            psp->errorcnt++;
            psp->state = e_state::RESYNC_AFTER_DECL_ERROR;
        }
        else {
            psp->tkclass = Symbol_new(x);
            psp->tkclass->type = symbol_type::MULTITERMINAL;
            psp->state = e_state::WAITING_FOR_CLASS_TOKEN;
        }
        break;
    case e_state::WAITING_FOR_CLASS_TOKEN:
        if (x[0] == '.') {
            psp->state = e_state::WAITING_FOR_DECL_OR_RULE;
        }
        else if (ISUPPER(x[0]) || ((x[0] == '|' || x[0] == '/') && ISUPPER(x[1]))) {
            symbol* msp = psp->tkclass;
            msp->nsubsym++;
            msp->subsym = (symbol**)realloc(msp->subsym,
                sizeof(symbol*) * msp->nsubsym);
            if (!ISUPPER(x[0])) x++;
            msp->subsym[msp->nsubsym - 1] = Symbol_new(x);
        }
        else {
            ErrorMsg(psp->filename, psp->tokenlineno,
                "%%token_class argument \"%s\" should be a token", x);
            psp->errorcnt++;
            psp->state = e_state::RESYNC_AFTER_DECL_ERROR;
        }
        break;
    case e_state::RESYNC_AFTER_RULE_ERROR:
        /*      if( x[0]=='.' ) psp->state = WAITING_FOR_DECL_OR_RULE;
        **      break; */
    case e_state::RESYNC_AFTER_DECL_ERROR:
        if (x[0] == '.') psp->state = e_state::WAITING_FOR_DECL_OR_RULE;
        if (x[0] == '%') psp->state = e_state::WAITING_FOR_DECL_KEYWORD;
        break;
    }
}

/* The text in the input is part of the argument to an %ifdef or %ifndef.
** Evaluate the text as a boolean expression.  Return true or false.
*/
static int eval_preprocessor_boolean(char* z, int lineno) {
    int neg = 0;
    int res = 0;
    int okTerm = 1;
    int i;
    for (i = 0; z[i] != 0; i++) {
        if (ISSPACE(z[i])) continue;
        if (z[i] == '!') {
            if (!okTerm) goto pp_syntax_error;
            neg = !neg;
            continue;
        }
        if (z[i] == '|' && z[i + 1] == '|') {
            if (okTerm) goto pp_syntax_error;
            if (res) return 1;
            i++;
            okTerm = 1;
            continue;
        }
        if (z[i] == '&' && z[i + 1] == '&') {
            if (okTerm) goto pp_syntax_error;
            if (!res) return 0;
            i++;
            okTerm = 1;
            continue;
        }
        if (z[i] == '(') {
            int k;
            int n = 1;
            if (!okTerm) goto pp_syntax_error;
            for (k = i + 1; z[k]; k++) {
                if (z[k] == ')') {
                    n--;
                    if (n == 0) {
                        z[k] = 0;
                        res = eval_preprocessor_boolean(&z[i + 1], -1);
                        z[k] = ')';
                        if (res < 0) {
                            i = i - res;
                            goto pp_syntax_error;
                        }
                        i = k;
                        break;
                    }
                }
                else if (z[k] == '(') {
                    n++;
                }
                else if (z[k] == 0) {
                    i = k;
                    goto pp_syntax_error;
                }
            }
            if (neg) {
                res = !res;
                neg = 0;
            }
            okTerm = 0;
            continue;
        }
        if (ISALPHA(z[i])) {
            int j, k, n;
            if (!okTerm) goto pp_syntax_error;
            for (k = i + 1; ISALNUM(z[k]) || z[k] == '_'; k++) {}
            n = k - i;
            res = 0;
            for (j = 0; j < nDefine; j++) {
                if (strncmp(azDefine[j], &z[i], n) == 0 && azDefine[j][n] == 0) {
                    res = 1;
                    break;
                }
            }
            i = k - 1;
            if (neg) {
                res = !res;
                neg = 0;
            }
            okTerm = 0;
            continue;
        }
        goto pp_syntax_error;
    }
    return res;

pp_syntax_error:
    if (lineno > 0) {
        fprintf(stderr, "%%if syntax error on line %d.\n", lineno);
        fprintf(stderr, "  %.*s <-- syntax error here\n", i + 1, z);
        exit(1);
    }
    else {
        return -(i + 1);
    }
}

/* Run the preprocessor over the input file text.  The global variables
** azDefine[0] through azDefine[nDefine-1] contains the names of all defined
** macros.  This routine looks for "%ifdef" and "%ifndef" and "%endif" and
** comments them out.  Text in between is also commented out as appropriate.
*/
static void preprocess_input(char* z) {
    int i, j, k;
    int exclude = 0;
    int start = 0;
    int lineno = 1;
    int start_lineno = 1;
    for (i = 0; z[i]; i++) {
        if (z[i] == '\n') lineno++;
        if (z[i] != '%' || (i > 0 && z[i - 1] != '\n')) continue;
        if (strncmp(&z[i], "%endif", 6) == 0 && ISSPACE(z[i + 6])) {
            if (exclude) {
                exclude--;
                if (exclude == 0) {
                    for (j = start; j < i; j++) if (z[j] != '\n') z[j] = ' ';
                }
            }
            for (j = i; z[j] && z[j] != '\n'; j++) z[j] = ' ';
        }
        else if (strncmp(&z[i], "%else", 5) == 0 && ISSPACE(z[i + 5])) {
            if (exclude == 1) {
                exclude = 0;
                for (j = start; j < i; j++) if (z[j] != '\n') z[j] = ' ';
            }
            else if (exclude == 0) {
                exclude = 1;
                start = i;
                start_lineno = lineno;
            }
            for (j = i; z[j] && z[j] != '\n'; j++) z[j] = ' ';
        }
        else if (strncmp(&z[i], "%ifdef ", 7) == 0
            || strncmp(&z[i], "%if ", 4) == 0
            || strncmp(&z[i], "%ifndef ", 8) == 0) {
            if (exclude) {
                exclude++;
            }
            else {
                int isNot;
                int iBool;
                for (j = i; z[j] && !ISSPACE(z[j]); j++) {}
                iBool = j;
                isNot = (j == i + 7);
                while (z[j] && z[j] != '\n') { j++; }
                k = z[j];
                z[j] = 0;
                exclude = eval_preprocessor_boolean(&z[iBool], lineno);
                z[j] = k;
                if (!isNot) exclude = !exclude;
                if (exclude) {
                    start = i;
                    start_lineno = lineno;
                }
            }
            for (j = i; z[j] && z[j] != '\n'; j++) z[j] = ' ';
        }
    }
    if (exclude) {
        fprintf(stderr, "unterminated %%ifdef starting on line %d\n", start_lineno);
        exit(1);
    }
}

/* In spite of its name, this function is really a scanner.  It read
** in the entire input file (all at once) then tokenizes it.  Each
** token is passed to the function "parseonetoken" which builds all
** the appropriate data structures in the global state vector "gp".
*/
void Parse(lemon& gp)
{
    pstate ps;
    FILE* fp;
    char* filebuf;
    unsigned int filesize;
    int lineno;
    int c;
    char* cp, * nextcp;
    constexpr int startline = 0;

    memset(&ps, '\0', sizeof(ps));
    ps.gp = &gp;
    ps.filename = gp.filename;
    ps.errorcnt = 0;
    ps.state = e_state::INITIALIZE;

    /* Begin by reading the input file */
    fp = fopen(ps.filename, "rb");
    if (fp == nullptr) {
        ErrorMsg(ps.filename, 0, "Can't open this file for reading.");
        gp.errorcnt++;
        return;
    }
    fseek(fp, 0, 2);
    filesize = ftell(fp);
    rewind(fp);
    filebuf = new char[filesize + 1];
    if (filesize > 100000000 || filebuf == nullptr) {
        ErrorMsg(ps.filename, 0, "Input file too large.");
        delete[] filebuf;
        gp.errorcnt++;
        fclose(fp);
        return;
    }
    if (fread(filebuf, 1, filesize, fp) != filesize) {
        ErrorMsg(ps.filename, 0, "Can't read in all %d bytes of this file.",
            filesize);
        delete[] filebuf;
        gp.errorcnt++;
        fclose(fp);
        return;
    }
    fclose(fp);
    filebuf[filesize] = 0;

    /* Make an initial pass through the file to handle %ifdef and %ifndef */
    preprocess_input(filebuf);
    if (gp.printPreprocessed) {
        printf("%s\n", filebuf);
        return;
    }

    /* Now scan the text of the input file */
    lineno = 1;
    for (cp = filebuf; (c = *cp) != 0; ) {
        if (c == '\n') lineno++;              /* Keep track of the line number */
        if (ISSPACE(c)) { cp++; continue; }  /* Skip all white space */
        if (c == '/' && cp[1] == '/') {          /* Skip C++ style comments */
            cp += 2;
            while ((c = *cp) != 0 && c != '\n') cp++;
            continue;
        }
        if (c == '/' && cp[1] == '*') {          /* Skip C style comments */
            cp += 2;
            while ((c = *cp) != 0 && (c != '/' || cp[-1] != '*')) {
                if (c == '\n') lineno++;
                cp++;
            }
            if (c) cp++;
            continue;
        }
        ps.tokenstart = cp;                /* Mark the beginning of the token */
        ps.tokenlineno = lineno;           /* Linenumber on which token begins */
        if (c == '\"') {                     /* String literals */
            cp++;
            while ((c = *cp) != 0 && c != '\"') {
                if (c == '\n') lineno++;
                cp++;
            }
            if (c == 0) {
                ErrorMsg(ps.filename, startline,
                    "String starting on this line is not terminated before "
                    "the end of the file.");
                ps.errorcnt++;
                nextcp = cp;
            }
            else {
                nextcp = cp + 1;
            }
        }
        else if (c == '{') {               /* A block of C code */
            int level;
            cp++;
            for (level = 1; (c = *cp) != 0 && (level > 1 || c != '}'); cp++) {
                if (c == '\n') lineno++;
                else if (c == '{') level++;
                else if (c == '}') level--;
                else if (c == '/' && cp[1] == '*') {  /* Skip comments */
                    int prevc;
                    cp = &cp[2];
                    prevc = 0;
                    while ((c = *cp) != 0 && (c != '/' || prevc != '*')) {
                        if (c == '\n') lineno++;
                        prevc = c;
                        cp++;
                    }
                }
                else if (c == '/' && cp[1] == '/') {  /* Skip C++ style comments too */
                    cp = &cp[2];
                    while ((c = *cp) != 0 && c != '\n') cp++;
                    if (c) lineno++;
                }
                else if (c == '\'' || c == '\"') {    /* String a character literals */
                    int startchar, prevc;
                    startchar = c;
                    prevc = 0;
                    for (cp++; (c = *cp) != 0 && (c != startchar || prevc == '\\'); cp++) {
                        if (c == '\n') lineno++;
                        if (prevc == '\\') prevc = 0;
                        else              prevc = c;
                    }
                }
            }
            if (c == 0) {
                ErrorMsg(ps.filename, ps.tokenlineno,
                    "C code starting on this line is not terminated before "
                    "the end of the file.");
                ps.errorcnt++;
                nextcp = cp;
            }
            else {
                nextcp = cp + 1;
            }
        }
        else if (ISALNUM(c)) {          /* Identifiers */
            while ((c = *cp) != 0 && (ISALNUM(c) || c == '_')) cp++;
            nextcp = cp;
        }
        else if (c == ':' && cp[1] == ':' && cp[2] == '=') { /* The operator "::=" */
            cp += 3;
            nextcp = cp;
        }
        else if ((c == '/' || c == '|') && ISALPHA(cp[1])) {
            cp += 2;
            while ((c = *cp) != 0 && (ISALNUM(c) || c == '_')) cp++;
            nextcp = cp;
        }
        else {                          /* All other (one character) operators */
            cp++;
            nextcp = cp;
        }
        c = *cp;
        *cp = 0;                        /* Null terminate the token */
        parseonetoken(&ps);             /* Parse the token */
        *cp = (char)c;                  /* Restore the buffer */
        cp = nextcp;
    }
    delete[] filebuf;                    /* Release the buffer after parsing */
    gp.rule = ps.firstrule;
    gp.errorcnt = ps.errorcnt;
}
/*************************** From the file "plink.c" *********************/
namespace Plink
{
/*
** Routines processing configuration follow-set propagation links
** in the LEMON parser generator.
*/
static plink* plink_freelist = nullptr;

/* Allocate a new plink */
plink* Plink_new(void) {
    plink* newlink;

    if (plink_freelist == nullptr) {
        int i;
        int amt = 100;
        plink_freelist = (plink*)calloc(amt, sizeof(plink));
        if (plink_freelist == nullptr) {
            fprintf(stderr,
                "Unable to allocate memory for a new follow-set propagation link.\n");
            exit(1);
        }
        for (i = 0; i < amt - 1; i++) plink_freelist[i].next = &plink_freelist[i + 1];
        plink_freelist[amt - 1].next = nullptr;
    }
    newlink = plink_freelist;
    plink_freelist = plink_freelist->next;
    return newlink;
}

/* Add a plink to a plink list */
void Plink_add(plink** plpp, config* cfp)
{
    plink* newlink;
    newlink = Plink_new();
    newlink->next = *plpp;
    *plpp = newlink;
    newlink->cfp = cfp;
}

/* Transfer every plink on the list "from" to the list "to" */
void Plink_copy(plink** to, plink* from)
{
    plink* nextpl;
    while (from) {
        nextpl = from->next;
        from->next = *to;
        *to = from;
        from = nextpl;
    }
}

/* Delete every plink on the list */
void Plink_delete(plink* plp)
{
    plink* nextpl;

    while (plp) {
        nextpl = plp->next;
        plp->next = plink_freelist;
        plink_freelist = plp;
        plp = nextpl;
    }
}
}
using namespace Plink;
/*********************** From the file "report.c" **************************/
/*
** Procedures for generating reports and tables in the LEMON parser generator.
*/

/* Generate a filename with the given suffix.  Space to hold the
** name comes from malloc() and must be freed by the calling
** function.
*/
PRIVATE char* file_makename(const lemon& lemp, const char* suffix)
{
    char* name;
    char* cp;
    char* filename = lemp.filename;
    int sz;

    if (outputDir) {
        cp = strrchr(filename, '/');
        if (cp) filename = cp + 1;
    }
    sz = lemonStrlen(filename);
    sz += lemonStrlen(suffix);
    if (outputDir) sz += lemonStrlen(outputDir) + 1;
    sz += 5;
    name = new char[sz];
    if (name == nullptr) {
        fprintf(stderr, "Can't allocate space for a filename.\n");
        exit(1);
    }
    name[0] = 0;
    if (outputDir) {
        lemon_strcpy(name, outputDir);
        lemon_strcat(name, "/");
    }
    lemon_strcat(name, filename);
    cp = strrchr(name, '.');
    if (cp) *cp = 0;
    lemon_strcat(name, suffix);
    return name;
}

/* Open a file with a name based on the name of the input file,
** but with a different (specified) suffix, and return a pointer
** to the stream */
PRIVATE FILE* file_open(
    lemon& lemp,
    const char* suffix,
    const char* mode
) {
    FILE* fp;

    if (lemp.outname) delete[] lemp.outname;
    lemp.outname = file_makename(lemp, suffix);
    fp = fopen(lemp.outname, mode);
    if (fp == nullptr && *mode == 'w') {
        fprintf(stderr, "Can't open file \"%s\".\n", lemp.outname);
        lemp.errorcnt++;
        return nullptr;
    }
    return fp;
}

/* Print the text of a rule
*/
void rule_print(FILE* out, const rule* rp) {
    int i, j;
    fprintf(out, "%s", rp->lhs->name);
    /*    if( rp->lhsalias ) fprintf(out,"(%s)",rp->lhsalias); */
    fprintf(out, " ::=");
    for (i = 0; i < rp->nrhs; i++) {
        const symbol* sp = rp->rhs[i];
        if (sp->type == symbol_type::MULTITERMINAL) {
            fprintf(out, " %s", sp->subsym[0]->name);
            for (j = 1; j < sp->nsubsym; j++) {
                fprintf(out, "|%s", sp->subsym[j]->name);
            }
        }
        else {
            fprintf(out, " %s", sp->name);
        }
        /* if( rp->rhsalias[i] ) fprintf(out,"(%s)",rp->rhsalias[i]); */
    }
}

/* Duplicate the input file without comments and without actions
** on rules */
void Reprint(lemon& lemp)
{
    rule* rp;
    symbol* sp;
    int i, j, maxlen, len, ncolumns, skip;
    printf("// Reprint of input file \"%s\".\n// Symbols:\n", lemp.filename);
    maxlen = 10;
    for (i = 0; i < lemp.nsymbol; i++) {
        sp = lemp.symbols[i];
        len = lemonStrlen(sp->name);
        if (len > maxlen) maxlen = len;
    }
    ncolumns = 76 / (maxlen + 5);
    if (ncolumns < 1) ncolumns = 1;
    skip = (lemp.nsymbol + ncolumns - 1) / ncolumns;
    for (i = 0; i < skip; i++) {
        printf("//");
        for (j = i; j < lemp.nsymbol; j += skip) {
            sp = lemp.symbols[j];
            assert(sp->index == j);
            printf(" %3d %-*.*s", j, maxlen, maxlen, sp->name);
        }
        printf("\n");
    }
    for (rp = lemp.rule; rp; rp = rp->next) {
        rule_print(stdout, rp);
        printf(".");
        if (rp->precsym) printf(" [%s]", rp->precsym->name);
        /* if( rp->code ) printf("\n    %s",rp->code); */
        printf("\n");
    }
}

/* Print a single rule.
*/
void RulePrint(FILE* fp, rule* rp, int iCursor) {
    symbol* sp;
    int i, j;
    fprintf(fp, "%s ::=", rp->lhs->name);
    for (i = 0; i <= rp->nrhs; i++) {
        if (i == iCursor) fprintf(fp, " *");
        if (i == rp->nrhs) break;
        sp = rp->rhs[i];
        if (sp->type == symbol_type::MULTITERMINAL) {
            fprintf(fp, " %s", sp->subsym[0]->name);
            for (j = 1; j < sp->nsubsym; j++) {
                fprintf(fp, "|%s", sp->subsym[j]->name);
            }
        }
        else {
            fprintf(fp, " %s", sp->name);
        }
    }
}

/* Print the rule for a configuration.
*/
void ConfigPrint(FILE* fp, config* cfp) {
    RulePrint(fp, cfp->rp, cfp->dot);
}

/* #define TEST */
#if 0
/* Print a set */
PRIVATE void SetPrint(out, set, lemp)
FILE* out;
char* set;
lemon* lemp;
{
    int i;
    char* spacer;
    spacer = "";
    fprintf(out, "%12s[", "");
    for (i = 0; i < lemp.nterminal; i++) {
        if (SetFind(set, i)) {
            fprintf(out, "%s%s", spacer, lemp.symbols[i]->name);
            spacer = " ";
        }
    }
    fprintf(out, "]\n");
}

/* Print a plink chain */
PRIVATE void PlinkPrint(out, plp, tag)
FILE* out;
plink* plp;
char* tag;
{
    while (plp) {
        fprintf(out, "%12s%s (state %2d) ", "", tag, plp->cfp->stp->statenum);
        ConfigPrint(out, plp->cfp);
        fprintf(out, "\n");
        plp = plp->next;
    }
}
#endif

/* Print an action to the given file descriptor.  Return FALSE if
** nothing was actually printed.
*/
int PrintAction(
    action* ap,          /* The action to print */
    FILE* fp,                   /* Print the action here */
    int indent                  /* Indent by this amount */
) {
    int result = 1;
    switch (ap->type) {
    case e_action::SHIFT: {
        const state* stp = ap->x.stp;
        fprintf(fp, "%*s shift        %-7d", indent, ap->sp->name, stp->statenum);
        break;
    }
    case e_action::REDUCE: {
        rule* rp = ap->x.rp;
        fprintf(fp, "%*s reduce       %-7d", indent, ap->sp->name, rp->iRule);
        RulePrint(fp, rp, -1);
        break;
    }
    case e_action::SHIFTREDUCE: {
        rule* rp = ap->x.rp;
        fprintf(fp, "%*s shift-reduce %-7d", indent, ap->sp->name, rp->iRule);
        RulePrint(fp, rp, -1);
        break;
    }
    case e_action::ACCEPT:
        fprintf(fp, "%*s accept", indent, ap->sp->name);
        break;
    case e_action::ERROR:
        fprintf(fp, "%*s error", indent, ap->sp->name);
        break;
    case e_action::SRCONFLICT:
    case e_action::RRCONFLICT:
        fprintf(fp, "%*s reduce       %-7d ** Parsing conflict **",
            indent, ap->sp->name, ap->x.rp->iRule);
        break;
    case e_action::SSCONFLICT:
        fprintf(fp, "%*s shift        %-7d ** Parsing conflict **",
            indent, ap->sp->name, ap->x.stp->statenum);
        break;
    case e_action::SH_RESOLVED:
        if (showPrecedenceConflict) {
            fprintf(fp, "%*s shift        %-7d -- dropped by precedence",
                indent, ap->sp->name, ap->x.stp->statenum);
        }
        else {
            result = 0;
        }
        break;
    case e_action::RD_RESOLVED:
        if (showPrecedenceConflict) {
            fprintf(fp, "%*s reduce %-7d -- dropped by precedence",
                indent, ap->sp->name, ap->x.rp->iRule);
        }
        else {
            result = 0;
        }
        break;
    case e_action::NOT_USED:
        result = 0;
        break;
    }
    if (result && ap->spOpt) {
        fprintf(fp, "  /* because %s==%s */", ap->sp->name, ap->spOpt->name);
    }
    return result;
}

/* Generate the "*.out" log file */
void ReportOutput(lemon& lemp)
{
    int i, n;
    state* stp;
    config* cfp;
    action* ap;
    rule* rp;
    FILE* fp;

    fp = file_open(lemp, ".out", "wb");
    if (fp == nullptr) return;
    for (i = 0; i < lemp.nxstate; i++) {
        stp = lemp.sorted[i];
        fprintf(fp, "State %d:\n", stp->statenum);
        if (lemp.basisflag) cfp = stp->bp;
        else                  cfp = stp->cfp;
        while (cfp) {
            char buf[20];
            if (cfp->dot == cfp->rp->nrhs) {
                lemon_sprintf(buf, "(%d)", cfp->rp->iRule);
                fprintf(fp, "    %5s ", buf);
            }
            else {
                fprintf(fp, "          ");
            }
            ConfigPrint(fp, cfp);
            fprintf(fp, "\n");
#if 0
            SetPrint(fp, cfp->fws, lemp);
            PlinkPrint(fp, cfp->fplp, "To  ");
            PlinkPrint(fp, cfp->bplp, "From");
#endif
            if (lemp.basisflag) cfp = cfp->bp;
            else                  cfp = cfp->next;
        }
        fprintf(fp, "\n");
        for (ap = stp->ap; ap; ap = ap->next) {
            if (PrintAction(ap, fp, 30)) fprintf(fp, "\n");
        }
        fprintf(fp, "\n");
    }
    fprintf(fp, "----------------------------------------------------\n");
    fprintf(fp, "Symbols:\n");
    fprintf(fp, "The first-set of non-terminals is shown after the name.\n\n");
    for (i = 0; i < lemp.nsymbol; i++) {
        int j;
        symbol* sp;

        sp = lemp.symbols[i];
        fprintf(fp, "  %3d: %s", i, sp->name);
        if (sp->type == symbol_type::NONTERMINAL) {
            fprintf(fp, ":");
            if (sp->lambda == Boolean::LEMON_TRUE) {
                fprintf(fp, " <lambda>");
            }
            for (j = 0; j < lemp.nterminal; j++) {
                if (sp->firstset && SetFind(sp->firstset, j)) {
                    fprintf(fp, " %s", lemp.symbols[j]->name);
                }
            }
        }
        if (sp->prec >= 0) fprintf(fp, " (precedence=%d)", sp->prec);
        fprintf(fp, "\n");
    }
    fprintf(fp, "----------------------------------------------------\n");
    fprintf(fp, "Syntax-only Symbols:\n");
    fprintf(fp, "The following symbols never carry semantic content.\n\n");
    for (i = n = 0; i < lemp.nsymbol; i++) {
        int w;
        const symbol* sp = lemp.symbols[i];
        if (sp->bContent) continue;
        w = (int)strlen(sp->name);
        if (n > 0 && n + w > 75) {
            fprintf(fp, "\n");
            n = 0;
        }
        if (n > 0) {
            fprintf(fp, " ");
            n++;
        }
        fprintf(fp, "%s", sp->name);
        n += w;
    }
    if (n > 0) fprintf(fp, "\n");
    fprintf(fp, "----------------------------------------------------\n");
    fprintf(fp, "Rules:\n");
    for (rp = lemp.rule; rp; rp = rp->next) {
        fprintf(fp, "%4d: ", rp->iRule);
        rule_print(fp, rp);
        fprintf(fp, ".");
        if (rp->precsym) {
            fprintf(fp, " [%s precedence=%d]",
                rp->precsym->name, rp->precsym->prec);
        }
        fprintf(fp, "\n");
    }
    fclose(fp);
    return;
}

/* Search for the file "name" which is in the same directory as
** the executable */
PRIVATE char* pathsearch(char* argv0, char* name, int modemask)
{
    const char* pathlist;
    char* pathbufptr = nullptr;
    char* pathbuf = nullptr;
    char* path, * cp;
    char c;

#ifdef __WIN32__
    cp = strrchr(argv0, '\\');
#else
    cp = strrchr(argv0, '/');
#endif
    if (cp) {
        c = *cp;
        *cp = 0;
        path = new char[lemonStrlen(argv0) + lemonStrlen(name) + 2];
        if (path) lemon_sprintf(path, "%s/%s", argv0, name);
        *cp = c;
    }
    else {
        pathlist = getenv("PATH");
        if (pathlist == nullptr) pathlist = ".:/bin:/usr/bin";
        pathbuf = new char[lemonStrlen(pathlist) + 1];
        path = new char[lemonStrlen(pathlist) + lemonStrlen(name) + 2];
        if ((pathbuf != nullptr) && (path != 0)) {
            pathbufptr = pathbuf;
            lemon_strcpy(pathbuf, pathlist);
            while (*pathbuf) {
                cp = strchr(pathbuf, ':');
                if (cp == nullptr) cp = &pathbuf[lemonStrlen(pathbuf)];
                c = *cp;
                *cp = 0;
                lemon_sprintf(path, "%s/%s", pathbuf, name);
                *cp = c;
                if (c == 0) pathbuf[0] = 0;
                else pathbuf = &cp[1];
                if (access(path, modemask) == 0) break;
            }
        }
        delete [] pathbufptr;
    }
    return path;
}

/* Given an action, compute the integer value for that action
** which is to be put in the action table of the generated machine.
** Return negative if no action should be generated.
*/
PRIVATE int compute_action(const lemon& lemp, const action* ap)
{
    switch (ap->type) {
    case e_action::SHIFT:  return ap->x.stp->statenum;
    case e_action::SHIFTREDUCE: {
        /* Since a SHIFT is inherient after a prior REDUCE, convert any
        ** SHIFTREDUCE action with a nonterminal on the LHS into a simple
        ** REDUCE action: */
        if (ap->sp->index >= lemp.nterminal) {
            return lemp.minReduce + ap->x.rp->iRule;
        }
        else {
            return lemp.minShiftReduce + ap->x.rp->iRule;
        }
    }
    case e_action::REDUCE: return lemp.minReduce + ap->x.rp->iRule;
    case e_action::ERROR:  return lemp.errAction;
    case e_action::ACCEPT: return lemp.accAction;
    default:     return -1;
    }
}

#define LINESIZE 1000
/* The next cluster of routines are for reading the template file
** and writing the results to the generated parser */
/* The first function transfers data from "in" to "out" until
** a line is seen which begins with "%%".  The line number is
** tracked.
**
** if name!=0, then any word that begin with "Parse" is changed to
** begin with *name instead.
*/
PRIVATE void tplt_xfer(char* name, FILE* in, FILE* out, int* lineno)
{
    int i, iStart;
    char line[LINESIZE];
    while (fgets(line, LINESIZE, in) && (line[0] != '%' || line[1] != '%')) {
        (*lineno)++;
        iStart = 0;
        if (name) {
            for (i = 0; line[i]; i++) {
                if (line[i] == 'P' && strncmp(&line[i], "Parse", 5) == 0
                    && (i == 0 || !ISALPHA(line[i - 1]))
                    ) {
                    if (i > iStart) fprintf(out, "%.*s", i - iStart, &line[iStart]);
                    fprintf(out, "%s", name);
                    i += 4;
                    iStart = i + 1;
                }
            }
        }
        fprintf(out, "%s", &line[iStart]);
    }
}

/* Skip forward past the header of the template file to the first "%%"
*/
PRIVATE void tplt_skip_header(FILE* in, int* lineno)
{
    char line[LINESIZE];
    while (fgets(line, LINESIZE, in) && (line[0] != '%' || line[1] != '%')) {
        (*lineno)++;
    }
}

/* The next function finds the template file and opens it, returning
** a pointer to the opened file. */
PRIVATE FILE* tplt_open(lemon& lemp)
{
    static char templatename[] = "lempar.c";
    char buf[1000];
    FILE* in;
    char* tpltname;
    char* toFree = nullptr;
    char* cp;

    /* first, see if user specified a template filename on the command line. */
    if (user_templatename != nullptr) {
        if (access(user_templatename, 004) == -1) {
            fprintf(stderr, "Can't find the parser driver template file \"%s\".\n",
                user_templatename);
            lemp.errorcnt++;
            return nullptr;
        }
        in = fopen(user_templatename, "rb");
        if (in == nullptr) {
            fprintf(stderr, "Can't open the template file \"%s\".\n",
                user_templatename);
            lemp.errorcnt++;
            return nullptr;
        }
        return in;
    }

    cp = strrchr(lemp.filename, '.');
    if (cp) {
        lemon_sprintf(buf, "%.*s.lt", (int)(cp - lemp.filename), lemp.filename);
    }
    else {
        lemon_sprintf(buf, "%s.lt", lemp.filename);
    }
    if (access(buf, 004) == 0) {
        tpltname = buf;
    }
    else if (access(templatename, 004) == 0) {
        tpltname = templatename;
    }
    else {
        toFree = tpltname = pathsearch(lemp.argv0, templatename, 0);
    }
    if (tpltname == nullptr) {
        fprintf(stderr, "Can't find the parser driver template file \"%s\".\n",
            templatename);
        lemp.errorcnt++;
        return nullptr;
    }
    in = fopen(tpltname, "rb");
    if (in == nullptr) {
        fprintf(stderr, "Can't open the template file \"%s\".\n", tpltname);
        lemp.errorcnt++;
    }
    delete[] toFree;
    return in;
}

/* Print a #line directive line to the output file. */
PRIVATE void tplt_linedir(FILE* out, int lineno, char* filename)
{
    fprintf(out, "#line %d \"", lineno);
    while (*filename) {
        if (*filename == '\\') putc('\\', out);
        putc(*filename, out);
        filename++;
    }
    fprintf(out, "\"\n");
}

/* Print a string to the file and keep the linenumber up to date */
PRIVATE void tplt_print(FILE* out, lemon& lemp, char* str, int* lineno)
{
    if (str == nullptr) return;
    while (*str) {
        putc(*str, out);
        if (*str == '\n') (*lineno)++;
        str++;
    }
    if (str[-1] != '\n') {
        putc('\n', out);
        (*lineno)++;
    }
    if (!lemp.nolinenosflag) {
        (*lineno)++; tplt_linedir(out, *lineno, lemp.outname);
    }
    return;
}

/*
** The following routine emits code for the destructor for the
** symbol sp
*/
void emit_destructor_code(
    FILE* out,
    symbol* sp,
    lemon& lemp,
    int* lineno
) {
    char* cp = nullptr;

    if (sp->type == symbol_type::TERMINAL) {
        cp = lemp.tokendest;
        if (cp == nullptr) return;
        fprintf(out, "{\n"); (*lineno)++;
    }
    else if (sp->destructor) {
        cp = sp->destructor;
        fprintf(out, "{\n"); (*lineno)++;
        if (!lemp.nolinenosflag) {
            (*lineno)++;
            tplt_linedir(out, sp->destLineno, lemp.filename);
        }
    }
    else if (lemp.vardest) {
        cp = lemp.vardest;
        if (cp == nullptr) return;
        fprintf(out, "{\n"); (*lineno)++;
    }
    else {
        assert(0);  /* Cannot happen */
    }
    for (; *cp; cp++) {
        if (*cp == '$' && cp[1] == '$') {
            fprintf(out, "(yypminor->yy%d)", sp->dtnum);
            cp++;
            continue;
        }
        if (*cp == '\n') (*lineno)++;
        fputc(*cp, out);
    }
    fprintf(out, "\n"); (*lineno)++;
    if (!lemp.nolinenosflag) {
        (*lineno)++; tplt_linedir(out, *lineno, lemp.outname);
    }
    fprintf(out, "}\n"); (*lineno)++;
    return;
}

/*
** Return TRUE (non-zero) if the given symbol has a destructor.
*/
int has_destructor(const symbol& sp, const lemon& lemp)
{
    int ret;
    if (sp.type == symbol_type::TERMINAL) {
        ret = lemp.tokendest != nullptr;
    }
    else {
        ret = lemp.vardest != nullptr || sp.destructor != nullptr;
    }
    return ret;
}

/*
** Append text to a dynamically allocated string.  If zText is nullptr then
** reset the string to be empty again.  Always return the complete text
** of the string (which is overwritten with each call).
**
** n bytes of zText are stored.  If n==0 then all of zText up to the first
** \000 terminator is stored.  zText can contain up to two instances of
** %d.  The values of p1 and p2 are written into the first and second
** %d.
**
** If n==-1, then the previous character is overwritten.
*/
PRIVATE char* append_str(const char* zText, int n, int p1, int p2) {
    static char empty[1] = { 0 };
    static char* z = nullptr;
    static int alloced = 0;
    static int used = 0;
    int c;
    char zInt[40];
    if (zText == nullptr) {
        if (used == 0 && z != nullptr) z[0] = 0;
        used = 0;
        return z;
    }
    if (n <= 0) {
        if (n < 0) {
            used += n;
            assert(used >= 0);
        }
        n = lemonStrlen(zText);
    }
    if ((int)(n + sizeof(zInt) * 2 + used) >= alloced) {
        alloced = n + sizeof(zInt) * 2 + used + 200;
        z = (char*)realloc(z, alloced);
    }
    if (z == nullptr) return empty;
    while (n-- > 0) {
        c = *(zText++);
        if (c == '%' && n > 0 && zText[0] == 'd') {
            lemon_sprintf(zInt, "%d", p1);
            p1 = p2;
            lemon_strcpy(&z[used], zInt);
            used += lemonStrlen(&z[used]);
            zText++;
            n--;
        }
        else {
            z[used++] = (char)c;
        }
    }
    z[used] = 0;
    return z;
}

/*
** Write and transform the rp->code string so that symbols are expanded.
** Populate the rp->codePrefix and rp->codeSuffix strings, as appropriate.
**
** Return 1 if the expanded code requires that "yylhsminor" local variable
** to be defined.
*/
PRIVATE int translate_code(lemon& lemp, rule& rp) {
    char* cp, * xp;
    int i;
    int rc = 0;            /* True if yylhsminor is used */
    int dontUseRhs0 = 0;   /* If true, use of left-most RHS label is illegal */
    const char* zSkip = nullptr; /* The zOvwrt comment within rp->code, or NULL */
    char lhsused = 0;      /* True if the LHS element has been used */
    char lhsdirect;        /* True if LHS writes directly into stack */
    char used[MAXRHS];     /* True for each RHS element which is used */
    char zLhs[50];         /* Convert the LHS symbol into this string */
    char zOvwrt[900];      /* Comment that to allow LHS to overwrite RHS */

    for (i = 0; i < rp.nrhs; i++) used[i] = 0;
    lhsused = 0;

    if (rp.code == nullptr) {
        static char newlinestr[2] = { '\n', '\0' };
        rp.code = newlinestr;
        rp.line = rp.ruleline;
        rp.noCode = Boolean::LEMON_TRUE;
    }
    else {
        rp.noCode = Boolean::LEMON_FALSE;
    }


    if (rp.nrhs == 0) {
        /* If there are no RHS symbols, then writing directly to the LHS is ok */
        lhsdirect = 1;
    }
    else if (rp.rhsalias[0] == nullptr) {
        /* The left-most RHS symbol has no value.  LHS direct is ok.  But
        ** we have to call the destructor on the RHS symbol first. */
        lhsdirect = 1;
        //                    v better not be a nullptr
        if (has_destructor(*rp.rhs[0], lemp)) {
            append_str(nullptr, 0, 0, 0);
            append_str("  yy_destructor(yypParser,%d,&yymsp[%d].minor);\n", 0,
                rp.rhs[0]->index, 1 - rp.nrhs);
            rp.codePrefix = Strsafe(append_str(nullptr, 0, 0, 0));
            rp.noCode = Boolean::LEMON_FALSE;
        }
    }
    else if (rp.lhsalias == nullptr) {
        /* There is no LHS value symbol. */
        lhsdirect = 1;
    }
    else if (strcmp(rp.lhsalias, rp.rhsalias[0]) == 0) {
        /* The LHS symbol and the left-most RHS symbol are the same, so
        ** direct writing is allowed */
        lhsdirect = 1;
        lhsused = 1;
        used[0] = 1;
        if (rp.lhs->dtnum != rp.rhs[0]->dtnum) {
            ErrorMsg(lemp.filename, rp.ruleline,
                "%s(%s) and %s(%s) share the same label but have "
                "different datatypes.",
                rp.lhs->name, rp.lhsalias, rp.rhs[0]->name, rp.rhsalias[0]);
            lemp.errorcnt++;
        }
    }
    else {
        lemon_sprintf(zOvwrt, "/*%s-overwrites-%s*/",
            rp.lhsalias, rp.rhsalias[0]);
        zSkip = strstr(rp.code, zOvwrt);
        if (zSkip != nullptr) {
            /* The code contains a special comment that indicates that it is safe
            ** for the LHS label to overwrite left-most RHS label. */
            lhsdirect = 1;
        }
        else {
            lhsdirect = 0;
        }
    }
    if (lhsdirect) {
        sprintf(zLhs, "yymsp[%d].minor.yy%d", 1 - rp.nrhs, rp.lhs->dtnum);
    }
    else {
        rc = 1;
        sprintf(zLhs, "yylhsminor.yy%d", rp.lhs->dtnum);
    }

    append_str(nullptr, 0, 0, 0);

    /* This const cast is wrong but harmless, if we're careful. */
    for (cp = (char*)rp.code; *cp; cp++) {
        if (cp == zSkip) {
            append_str(zOvwrt, 0, 0, 0);
            cp += lemonStrlen(zOvwrt) - 1;
            dontUseRhs0 = 1;
            continue;
        }
        if (ISALPHA(*cp) && (cp == rp.code || (!ISALNUM(cp[-1]) && cp[-1] != '_'))) {
            char saved;
            for (xp = &cp[1]; ISALNUM(*xp) || *xp == '_'; xp++);
            saved = *xp;
            *xp = 0;
            if (rp.lhsalias && strcmp(cp, rp.lhsalias) == 0) {
                append_str(zLhs, 0, 0, 0);
                cp = xp;
                lhsused = 1;
            }
            else {
                for (i = 0; i < rp.nrhs; i++) {
                    if (rp.rhsalias[i] && strcmp(cp, rp.rhsalias[i]) == 0) {
                        if (i == 0 && dontUseRhs0) {
                            ErrorMsg(lemp.filename, rp.ruleline,
                                "Label %s used after '%s'.",
                                rp.rhsalias[0], zOvwrt);
                            lemp.errorcnt++;
                        }
                        else if (cp != rp.code && cp[-1] == '@') {
                            /* If the argument is of the form @X then substituted
                            ** the token number of X, not the value of X */
                            append_str("yymsp[%d].major", -1, i - rp.nrhs + 1, 0);
                        }
                        else {
                            const symbol* sp = rp.rhs[i];
                            int dtnum;
                            if (sp->type == symbol_type::MULTITERMINAL) {
                                dtnum = sp->subsym[0]->dtnum;
                            }
                            else {
                                dtnum = sp->dtnum;
                            }
                            append_str("yymsp[%d].minor.yy%d", 0, i - rp.nrhs + 1, dtnum);
                        }
                        cp = xp;
                        used[i] = 1;
                        break;
                    }
                }
            }
            *xp = saved;
        }
        append_str(cp, 1, 0, 0);
    } /* End loop */

    /* Main code generation completed */
    cp = append_str(nullptr, 0, 0, 0);
    if (cp && cp[0]) rp.code = Strsafe(cp);
    append_str(nullptr, 0, 0, 0);

    /* Check to make sure the LHS has been used */
    if (rp.lhsalias && !lhsused) {
        ErrorMsg(lemp.filename, rp.ruleline,
            "Label \"%s\" for \"%s(%s)\" is never used.",
            rp.lhsalias, rp.lhs->name, rp.lhsalias);
        lemp.errorcnt++;
    }

    /* Generate destructor code for RHS minor values which are not referenced.
    ** Generate error messages for unused labels and duplicate labels.
    */
    for (i = 0; i < rp.nrhs; i++) {
        if (rp.rhsalias[i]) {
            if (i > 0) {
                int j;
                if (rp.lhsalias && strcmp(rp.lhsalias, rp.rhsalias[i]) == 0) {
                    ErrorMsg(lemp.filename, rp.ruleline,
                        "%s(%s) has the same label as the LHS but is not the left-most "
                        "symbol on the RHS.",
                        rp.rhs[i]->name, rp.rhsalias[i]);
                    lemp.errorcnt++;
                }
                for (j = 0; j < i; j++) {
                    if (rp.rhsalias[j] && strcmp(rp.rhsalias[j], rp.rhsalias[i]) == 0) {
                        ErrorMsg(lemp.filename, rp.ruleline,
                            "Label %s used for multiple symbols on the RHS of a rule.",
                            rp.rhsalias[i]);
                        lemp.errorcnt++;
                        break;
                    }
                }
            }
            if (!used[i]) {
                ErrorMsg(lemp.filename, rp.ruleline,
                    "Label %s for \"%s(%s)\" is never used.",
                    rp.rhsalias[i], rp.rhs[i]->name, rp.rhsalias[i]);
                lemp.errorcnt++;
            }
        }
        //                                      v better not be a nullptr
        else if (i > 0 && has_destructor(*rp.rhs[i], lemp)) {
            append_str("  yy_destructor(yypParser,%d,&yymsp[%d].minor);\n", 0,
                rp.rhs[i]->index, i - rp.nrhs + 1);
        }
    }

    /* If unable to write LHS values directly into the stack, write the
    ** saved LHS value now. */
    if (lhsdirect == 0) {
        append_str("  yymsp[%d].minor.yy%d = ", 0, 1 - rp.nrhs, rp.lhs->dtnum);
        append_str(zLhs, 0, 0, 0);
        append_str(";\n", 0, 0, 0);
    }

    /* Suffix code generation complete */
    cp = append_str(nullptr, 0, 0, 0);
    if (cp && cp[0]) {
        rp.codeSuffix = Strsafe(cp);
        rp.noCode = Boolean::LEMON_FALSE;
    }

    return rc;
}

/*
** Generate code which executes when the rule "rp" is reduced.  Write
** the code to "out".  Make sure lineno stays up-to-date.
*/
PRIVATE void emit_code(
    FILE* out,
    const rule* rp,
    lemon& lemp,
    int* lineno
) {
    const char* cp;

    /* Setup code prior to the #line directive */
    if (rp->codePrefix && rp->codePrefix[0]) {
        fprintf(out, "{%s", rp->codePrefix);
        for (cp = rp->codePrefix; *cp; cp++) { if (*cp == '\n') (*lineno)++; }
    }

    /* Generate code to do the reduce action */
    if (rp->code) {
        if (!lemp.nolinenosflag) {
            (*lineno)++;
            tplt_linedir(out, rp->line, lemp.filename);
        }
        fprintf(out, "{%s", rp->code);
        for (cp = rp->code; *cp; cp++) { if (*cp == '\n') (*lineno)++; }
        fprintf(out, "}\n"); (*lineno)++;
        if (!lemp.nolinenosflag) {
            (*lineno)++;
            tplt_linedir(out, *lineno, lemp.outname);
        }
    }

    /* Generate breakdown code that occurs after the #line directive */
    if (rp->codeSuffix && rp->codeSuffix[0]) {
        fprintf(out, "%s", rp->codeSuffix);
        for (cp = rp->codeSuffix; *cp; cp++) { if (*cp == '\n') (*lineno)++; }
    }

    if (rp->codePrefix) {
        fprintf(out, "}\n"); (*lineno)++;
    }

    return;
}

/*
** Print the definition of the union used for the parser's data stack.
** This union contains fields for every possible data type for tokens
** and nonterminals.  In the process of computing and printing this
** union, also set the ".dtnum" field of every terminal and nonterminal
** symbol.
*/
void print_stack_union(
    FILE* out,                  /* The output stream */
    lemon& lemp,         /* The main info structure for this parser */
    int* plineno,               /* Pointer to the line number */
    int mhflag                  /* True if generating makeheaders output */
) {
    int lineno = *plineno;    /* The line number of the output */
    char** types;             /* A hash table of datatypes */
    int arraysize;            /* Size of the "types" array */
    int maxdtlength;          /* Maximum length of any ".datatype" field. */
    char* stddt;              /* Standardized name for a datatype */
    int i, j;                  /* Loop counters */
    unsigned hash;            /* For hashing the name of a type */
    const char* name;         /* Name of the parser */

    /* Allocate and initialize types[] and allocate stddt[] */
    arraysize = lemp.nsymbol * 2;
    types = new char*[arraysize];
    if (types == nullptr) {
        fprintf(stderr, "Out of memory.\n");
        exit(1);
    }
    for (i = 0; i < arraysize; i++) types[i] = nullptr;
    maxdtlength = 0;
    if (lemp.vartype) {
        maxdtlength = lemonStrlen(lemp.vartype);
    }
    for (i = 0; i < lemp.nsymbol; i++) {
        int len;
        const symbol* sp = lemp.symbols[i];
        if (sp->datatype == nullptr) continue;
        len = lemonStrlen(sp->datatype);
        if (len > maxdtlength) maxdtlength = len;
    }
    stddt = new char [maxdtlength * 2 + 1];
    if (stddt == nullptr) {
        fprintf(stderr, "Out of memory.\n");
        exit(1);
    }

    /* Build a hash table of datatypes. The ".dtnum" field of each symbol
    ** is filled in with the hash index plus 1.  A ".dtnum" value of 0 is
    ** used for terminal symbols.  If there is no %default_type defined then
    ** 0 is also used as the .dtnum value for nonterminals which do not specify
    ** a datatype using the %type directive.
    */
    for (i = 0; i < lemp.nsymbol; i++) {
        symbol* sp = lemp.symbols[i];
        char* cp;
        if (sp == lemp.errsym) {
            sp->dtnum = arraysize + 1;
            continue;
        }
        if (sp->type != symbol_type::NONTERMINAL || (sp->datatype == nullptr && lemp.vartype == nullptr)) {
            sp->dtnum = 0;
            continue;
        }
        cp = sp->datatype;
        if (cp == nullptr) cp = lemp.vartype;
        j = 0;
        while (ISSPACE(*cp)) cp++;
        while (*cp) stddt[j++] = *cp++;
        while (j > 0 && ISSPACE(stddt[j - 1])) j--;
        stddt[j] = 0;
        if (lemp.tokentype && strcmp(stddt, lemp.tokentype) == 0) {
            sp->dtnum = 0;
            continue;
        }
        hash = 0;
        for (j = 0; stddt[j]; j++) {
            hash = hash * 53 + stddt[j];
        }
        hash = (hash & 0x7fffffff) % arraysize;
        while (types[hash]) {
            if (strcmp(types[hash], stddt) == 0) {
                sp->dtnum = hash + 1;
                break;
            }
            hash++;
            if (hash >= (unsigned)arraysize) hash = 0;
        }
        if (types[hash] == nullptr) {
            sp->dtnum = hash + 1;
            types[hash] = new char [lemonStrlen(stddt) + 1];
            if (types[hash] == nullptr) {
                fprintf(stderr, "Out of memory.\n");
                exit(1);
            }
            lemon_strcpy(types[hash], stddt);
        }
    }

    /* Print out the definition of YYTOKENTYPE and YYMINORTYPE */
    name = lemp.name ? lemp.name : "Parse";
    lineno = *plineno;
    if (mhflag) { fprintf(out, "#if INTERFACE\n"); lineno++; }
    fprintf(out, "#define %sTOKENTYPE %s\n", name,
        lemp.tokentype ? lemp.tokentype : "void*");  lineno++;
    if (mhflag) { fprintf(out, "#endif\n"); lineno++; }
    fprintf(out, "typedef union {\n"); lineno++;
    fprintf(out, "  int yyinit;\n"); lineno++;
    fprintf(out, "  %sTOKENTYPE yy0;\n", name); lineno++;
    for (i = 0; i < arraysize; i++) {
        if (types[i] == nullptr) continue;
        fprintf(out, "  %s yy%d;\n", types[i], i + 1); lineno++;
        delete[] types[i];
    }
    if (lemp.errsym && lemp.errsym->useCnt) {
        fprintf(out, "  int yy%d;\n", lemp.errsym->dtnum); lineno++;
    }
    delete[] stddt;
    delete[] types;
    fprintf(out, "} YYMINORTYPE;\n"); lineno++;
    *plineno = lineno;
}

/*
** Return the name of a C datatype able to represent values between
** lwr and upr, inclusive.  If pnByte!=NULL then also write the sizeof
** for that type (1, 2, or 4) into *pnByte.
*/
static const char* minimum_size_type(int lwr, int upr, int* pnByte) {
    const char* zType = "int";
    int nByte = 4;
    if (lwr >= 0) {
        if (upr <= 255) {
            zType = "unsigned char";
            nByte = 1;
        }
        else if (upr < 65535) {
            zType = "unsigned short int";
            nByte = 2;
        }
        else {
            zType = "unsigned int";
            nByte = 4;
        }
    }
    else if (lwr >= -127 && upr <= 127) {
        zType = "signed char";
        nByte = 1;
    }
    else if (lwr >= -32767 && upr < 32767) {
        zType = "short";
        nByte = 2;
    }
    if (pnByte) *pnByte = nByte;
    return zType;
}

/*
** Each state contains a set of token transaction and a set of
** nonterminal transactions.  Each of these sets makes an instance
** of the following structure.  An array of these structures is used
** to order the creation of entries in the yy_action[] table.
*/
struct axset {
    state* stp;   /* A pointer to a state */
    int isTkn;           /* True to use tokens.  False for non-terminals */
    int nAction;         /* Number of actions */
    int iOrder;          /* Original order of action sets */

    /*
    ** Compare to axset structures for sorting purposes
    */
    constexpr friend bool operator< (const axset& a, const axset& b)
    {
        int c = b.nAction - a.nAction;
        if (c == 0) {
            c = a.iOrder - b.iOrder;
        }
        assert(c != 0 || &a == &b);
        return c < 0;
    }
};

/*
** Write text on "out" that describes the rule "rp".
*/
static void writeRuleText(FILE* out, const rule* rp) {
    int j;
    fprintf(out, "%s ::=", rp->lhs->name);
    for (j = 0; j < rp->nrhs; j++) {
        const symbol* sp = rp->rhs[j];
        if (sp->type != symbol_type::MULTITERMINAL) {
            fprintf(out, " %s", sp->name);
        }
        else {
            int k;
            fprintf(out, " %s", sp->subsym[0]->name);
            for (k = 1; k < sp->nsubsym; k++) {
                fprintf(out, "|%s", sp->subsym[k]->name);
            }
        }
    }
}

char INCLUDE_BUFFER[] = "";

/* Generate C source code for the parser */
void ReportTable(
    lemon& lemp,
    int mhflag,     /* Output in makeheaders format if true */
    int sqlFlag     /* Generate the *.sql file too */
) {
    FILE* out, * in, * sql;
    char line[LINESIZE];
    int  lineno;
    state* stp;
    action* ap;
    rule* rp;
    acttab* pActtab;
    int i, j, n, sz;
    int nLookAhead;
    int szActionType;     /* sizeof(YYACTIONTYPE) */
    int szCodeType;       /* sizeof(YYCODETYPE)   */
    const char* name;
    int mnTknOfst, mxTknOfst;
    int mnNtOfst, mxNtOfst;
    axset* ax;
    const char* prefix;

    lemp.minShiftReduce = lemp.nstate;
    lemp.errAction = lemp.minShiftReduce + lemp.nrule;
    lemp.accAction = lemp.errAction + 1;
    lemp.noAction = lemp.accAction + 1;
    lemp.minReduce = lemp.noAction + 1;
    lemp.maxAction = lemp.minReduce + lemp.nrule;

    in = tplt_open(lemp);
    if (in == nullptr) return;
    out = file_open(lemp, ".c", "wb");
    if (out == nullptr) {
        fclose(in);
        return;
    }
    if (sqlFlag == 0) {
        sql = nullptr;
    }
    else {
        sql = file_open(lemp, ".sql", "wb");
        if (sql == nullptr) {
            fclose(in);
            fclose(out);
            return;
        }
        fprintf(sql,
            "BEGIN;\n"
            "CREATE TABLE symbol(\n"
            "  id INTEGER PRIMARY KEY,\n"
            "  name TEXT NOT NULL,\n"
            "  isTerminal BOOLEAN NOT NULL,\n"
            "  fallback INTEGER REFERENCES symbol"
            " DEFERRABLE INITIALLY DEFERRED\n"
            ");\n"
        );
        for (i = 0; i < lemp.nsymbol; i++) {
            fprintf(sql,
                "INSERT INTO symbol(id,name,isTerminal,fallback)"
                "VALUES(%d,'%s',%s",
                i, lemp.symbols[i]->name,
                i < lemp.nterminal ? "TRUE" : "FALSE"
            );
            if (lemp.symbols[i]->fallback) {
                fprintf(sql, ",%d);\n", lemp.symbols[i]->fallback->index);
            }
            else {
                fprintf(sql, ",NULL);\n");
            }
        }
        fprintf(sql,
            "CREATE TABLE rule(\n"
            "  ruleid INTEGER PRIMARY KEY,\n"
            "  lhs INTEGER REFERENCES symbol(id),\n"
            "  txt TEXT\n"
            ");\n"
            "CREATE TABLE rulerhs(\n"
            "  ruleid INTEGER REFERENCES rule(ruleid),\n"
            "  pos INTEGER,\n"
            "  sym INTEGER REFERENCES symbol(id)\n"
            ");\n"
        );
        for (i = 0, rp = lemp.rule; rp; rp = rp->next, i++) {
            assert(i == rp->iRule);
            fprintf(sql,
                "INSERT INTO rule(ruleid,lhs,txt)VALUES(%d,%d,'",
                rp->iRule, rp->lhs->index
            );
            writeRuleText(sql, rp);
            fprintf(sql, "');\n");
            for (j = 0; j < rp->nrhs; j++) {
                const symbol* sp = rp->rhs[j];
                if (sp->type != symbol_type::MULTITERMINAL) {
                    fprintf(sql,
                        "INSERT INTO rulerhs(ruleid,pos,sym)VALUES(%d,%d,%d);\n",
                        i, j, sp->index
                    );
                }
                else {
                    int k;
                    for (k = 0; k < sp->nsubsym; k++) {
                        fprintf(sql,
                            "INSERT INTO rulerhs(ruleid,pos,sym)VALUES(%d,%d,%d);\n",
                            i, j, sp->subsym[k]->index
                        );
                    }
                }
            }
        }
        fprintf(sql, "COMMIT;\n");
    }
    lineno = 1;

    fprintf(out,
        "/* This file is automatically generated by Lemon from input grammar\n"
        "** source file \"%s\". */\n", lemp.filename); lineno += 2;

    /* The first %include directive begins with a C-language comment,
    ** then skip over the header comment of the template file
    */
    if (lemp.include == nullptr) lemp.include = INCLUDE_BUFFER;
    for (i = 0; ISSPACE(lemp.include[i]); i++) {
        if (lemp.include[i] == '\n') {
            lemp.include += i + 1;
            i = -1;
        }
    }
    if (lemp.include[0] == '/') {
        tplt_skip_header(in, &lineno);
    }
    else {
        tplt_xfer(lemp.name, in, out, &lineno);
    }

    /* Generate the include code, if any */
    tplt_print(out, lemp, lemp.include, &lineno);
    if (mhflag) {
        char* incName = file_makename(lemp, ".h");
        fprintf(out, "#include \"%s\"\n", incName); lineno++;
        delete[] incName;
    }
    tplt_xfer(lemp.name, in, out, &lineno);

    /* Generate #defines for all tokens */
    if (lemp.tokenprefix) prefix = lemp.tokenprefix;
    else                   prefix = "";
    if (mhflag) {
        fprintf(out, "#if INTERFACE\n"); lineno++;
    }
    else {
        fprintf(out, "#ifndef %s%s\n", prefix, lemp.symbols[1]->name);
    }
    for (i = 1; i < lemp.nterminal; i++) {
        fprintf(out, "#define %s%-30s %2d\n", prefix, lemp.symbols[i]->name, i);
        lineno++;
    }
    fprintf(out, "#endif\n"); lineno++;
    tplt_xfer(lemp.name, in, out, &lineno);

    /* Generate the defines */
    fprintf(out, "#define YYCODETYPE %s\n",
        minimum_size_type(0, lemp.nsymbol, &szCodeType)); lineno++;
    fprintf(out, "#define YYNOCODE %d\n", lemp.nsymbol);  lineno++;
    fprintf(out, "#define YYACTIONTYPE %s\n",
        minimum_size_type(0, lemp.maxAction, &szActionType)); lineno++;
    if (lemp.wildcard) {
        fprintf(out, "#define YYWILDCARD %d\n",
            lemp.wildcard->index); lineno++;
    }
    print_stack_union(out, lemp, &lineno, mhflag);
    fprintf(out, "#ifndef YYSTACKDEPTH\n"); lineno++;
    if (lemp.stacksize) {
        fprintf(out, "#define YYSTACKDEPTH %s\n", lemp.stacksize);  lineno++;
    }
    else {
        fprintf(out, "#define YYSTACKDEPTH 100\n");  lineno++;
    }
    fprintf(out, "#endif\n"); lineno++;
    if (mhflag) {
        fprintf(out, "#if INTERFACE\n"); lineno++;
    }
    name = lemp.name ? lemp.name : "Parse";
    if (lemp.arg && lemp.arg[0]) {
        i = lemonStrlen(lemp.arg);
        while (i >= 1 && ISSPACE(lemp.arg[i - 1])) i--;
        while (i >= 1 && (ISALNUM(lemp.arg[i - 1]) || lemp.arg[i - 1] == '_')) i--;
        fprintf(out, "#define %sARG_SDECL %s;\n", name, lemp.arg);  lineno++;
        fprintf(out, "#define %sARG_PDECL ,%s\n", name, lemp.arg);  lineno++;
        fprintf(out, "#define %sARG_PARAM ,%s\n", name, &lemp.arg[i]);  lineno++;
        fprintf(out, "#define %sARG_FETCH %s=yypParser->%s;\n",
            name, lemp.arg, &lemp.arg[i]);  lineno++;
        fprintf(out, "#define %sARG_STORE yypParser->%s=%s;\n",
            name, &lemp.arg[i], &lemp.arg[i]);  lineno++;
    }
    else {
        fprintf(out, "#define %sARG_SDECL\n", name); lineno++;
        fprintf(out, "#define %sARG_PDECL\n", name); lineno++;
        fprintf(out, "#define %sARG_PARAM\n", name); lineno++;
        fprintf(out, "#define %sARG_FETCH\n", name); lineno++;
        fprintf(out, "#define %sARG_STORE\n", name); lineno++;
    }
    if (lemp.ctx && lemp.ctx[0]) {
        i = lemonStrlen(lemp.ctx);
        while (i >= 1 && ISSPACE(lemp.ctx[i - 1])) i--;
        while (i >= 1 && (ISALNUM(lemp.ctx[i - 1]) || lemp.ctx[i - 1] == '_')) i--;
        fprintf(out, "#define %sCTX_SDECL %s;\n", name, lemp.ctx);  lineno++;
        fprintf(out, "#define %sCTX_PDECL ,%s\n", name, lemp.ctx);  lineno++;
        fprintf(out, "#define %sCTX_PARAM ,%s\n", name, &lemp.ctx[i]);  lineno++;
        fprintf(out, "#define %sCTX_FETCH %s=yypParser->%s;\n",
            name, lemp.ctx, &lemp.ctx[i]);  lineno++;
        fprintf(out, "#define %sCTX_STORE yypParser->%s=%s;\n",
            name, &lemp.ctx[i], &lemp.ctx[i]);  lineno++;
    }
    else {
        fprintf(out, "#define %sCTX_SDECL\n", name); lineno++;
        fprintf(out, "#define %sCTX_PDECL\n", name); lineno++;
        fprintf(out, "#define %sCTX_PARAM\n", name); lineno++;
        fprintf(out, "#define %sCTX_FETCH\n", name); lineno++;
        fprintf(out, "#define %sCTX_STORE\n", name); lineno++;
    }
    if (mhflag) {
        fprintf(out, "#endif\n"); lineno++;
    }
    if (lemp.errsym && lemp.errsym->useCnt) {
        fprintf(out, "#define YYERRORSYMBOL %d\n", lemp.errsym->index); lineno++;
        fprintf(out, "#define YYERRSYMDT yy%d\n", lemp.errsym->dtnum); lineno++;
    }
    if (lemp.has_fallback) {
        fprintf(out, "#define YYFALLBACK 1\n");  lineno++;
    }

    /* Compute the action table, but do not output it yet.  The action
    ** table must be computed before generating the YYNSTATE macro because
    ** we need to know how many states can be eliminated.
    */
    ax = (axset*)calloc(lemp.nxstate * 2, sizeof(ax[0]));
    if (ax == nullptr) {
        fprintf(stderr, "malloc failed\n");
        exit(1);
    }
    for (i = 0; i < lemp.nxstate; i++) {
        stp = lemp.sorted[i];
        ax[i * 2].stp = stp;
        ax[i * 2].isTkn = 1;
        ax[i * 2].nAction = stp->nTknAct;
        ax[i * 2 + 1].stp = stp;
        ax[i * 2 + 1].isTkn = 0;
        ax[i * 2 + 1].nAction = stp->nNtAct;
    }
    mxTknOfst = mnTknOfst = 0;
    mxNtOfst = mnNtOfst = 0;
    /* In an effort to minimize the action table size, use the heuristic
    ** of placing the largest action sets first */
    for (i = 0; i < lemp.nxstate * 2; i++) ax[i].iOrder = i;
    
    //previously a qsort()
    std::sort(ax, ax + lemp.nxstate * 2);
    
    pActtab = acttab_alloc(lemp.nsymbol, lemp.nterminal);
    for (i = 0; i < lemp.nxstate * 2 && ax[i].nAction>0; i++) {
        stp = ax[i].stp;
        if (ax[i].isTkn) {
            for (ap = stp->ap; ap; ap = ap->next) {
                int action;
                if (ap->sp->index >= lemp.nterminal) continue;
                action = compute_action(lemp, ap);
                if (action < 0) continue;
                acttab_action(pActtab, ap->sp->index, action);
            }
            stp->iTknOfst = acttab_insert(pActtab, 1);
            if (stp->iTknOfst < mnTknOfst) mnTknOfst = stp->iTknOfst;
            if (stp->iTknOfst > mxTknOfst) mxTknOfst = stp->iTknOfst;
        }
        else {
            for (ap = stp->ap; ap; ap = ap->next) {
                int action;
                if (ap->sp->index < lemp.nterminal) continue;
                if (ap->sp->index == lemp.nsymbol) continue;
                action = compute_action(lemp, ap);
                if (action < 0) continue;
                acttab_action(pActtab, ap->sp->index, action);
            }
            stp->iNtOfst = acttab_insert(pActtab, 0);
            if (stp->iNtOfst < mnNtOfst) mnNtOfst = stp->iNtOfst;
            if (stp->iNtOfst > mxNtOfst) mxNtOfst = stp->iNtOfst;
        }
#if 0  /* Uncomment for a trace of how the yy_action[] table fills out */
        { int jj, nn;
        for (jj = nn = 0; jj < pActtab->nAction; jj++) {
            if (pActtab->aAction[jj].action < 0) nn++;
        }
        printf("%4d: State %3d %s n: %2d size: %5d freespace: %d\n",
            i, stp->statenum, ax[i].isTkn ? "Token" : "Var  ",
            ax[i].nAction, pActtab->nAction, nn);
        }
#endif
    }
    free(ax);

    /* Mark rules that are actually used for reduce actions after all
    ** optimizations have been applied
    */
    for (rp = lemp.rule; rp; rp = rp->next) rp->doesReduce = Boolean::LEMON_FALSE;
    for (i = 0; i < lemp.nxstate; i++) {
        for (ap = lemp.sorted[i]->ap; ap; ap = ap->next) {
            if (ap->type == e_action::REDUCE || ap->type == e_action::SHIFTREDUCE) {
                ap->x.rp->doesReduce = Boolean::LEMON_TRUE;
            }
        }
    }

    /* Finish rendering the constants now that the action table has
    ** been computed */
    fprintf(out, "#define YYNSTATE             %d\n", lemp.nxstate);  lineno++;
    fprintf(out, "#define YYNRULE              %d\n", lemp.nrule);  lineno++;
    fprintf(out, "#define YYNRULE_WITH_ACTION  %d\n", lemp.nruleWithAction);
    lineno++;
    fprintf(out, "#define YYNTOKEN             %d\n", lemp.nterminal); lineno++;
    fprintf(out, "#define YY_MAX_SHIFT         %d\n", lemp.nxstate - 1); lineno++;
    i = lemp.minShiftReduce;
    fprintf(out, "#define YY_MIN_SHIFTREDUCE   %d\n", i); lineno++;
    i += lemp.nrule;
    fprintf(out, "#define YY_MAX_SHIFTREDUCE   %d\n", i - 1); lineno++;
    fprintf(out, "#define YY_ERROR_ACTION      %d\n", lemp.errAction); lineno++;
    fprintf(out, "#define YY_ACCEPT_ACTION     %d\n", lemp.accAction); lineno++;
    fprintf(out, "#define YY_NO_ACTION         %d\n", lemp.noAction); lineno++;
    fprintf(out, "#define YY_MIN_REDUCE        %d\n", lemp.minReduce); lineno++;
    i = lemp.minReduce + lemp.nrule;
    fprintf(out, "#define YY_MAX_REDUCE        %d\n", i - 1); lineno++;
    tplt_xfer(lemp.name, in, out, &lineno);

    /* Now output the action table and its associates:
    **
    **  yy_action[]        A single table containing all actions.
    **  yy_lookahead[]     A table containing the lookahead for each entry in
    **                     yy_action.  Used to detect hash collisions.
    **  yy_shift_ofst[]    For each state, the offset into yy_action for
    **                     shifting terminals.
    **  yy_reduce_ofst[]   For each state, the offset into yy_action for
    **                     shifting non-terminals after a reduce.
    **  yy_default[]       Default action for each state.
    */

    /* Output the yy_action table */
    lemp.nactiontab = n = acttab_action_size(pActtab);
    lemp.tablesize += n * szActionType;
    fprintf(out, "#define YY_ACTTAB_COUNT (%d)\n", n); lineno++;
    fprintf(out, "static const YYACTIONTYPE yy_action[] = {\n"); lineno++;
    for (i = j = 0; i < n; i++) {
        int action = acttab_yyaction(pActtab, i);
        if (action < 0) action = lemp.noAction;
        if (j == 0) fprintf(out, " /* %5d */ ", i);
        fprintf(out, " %4d,", action);
        if (j == 9 || i == n - 1) {
            fprintf(out, "\n"); lineno++;
            j = 0;
        }
        else {
            j++;
        }
    }
    fprintf(out, "};\n"); lineno++;

    /* Output the yy_lookahead table */
    lemp.nlookaheadtab = n = acttab_lookahead_size(pActtab);
    lemp.tablesize += n * szCodeType;
    fprintf(out, "static const YYCODETYPE yy_lookahead[] = {\n"); lineno++;
    for (i = j = 0; i < n; i++) {
        int la = acttab_yylookahead(pActtab, i);
        if (la < 0) la = lemp.nsymbol;
        if (j == 0) fprintf(out, " /* %5d */ ", i);
        fprintf(out, " %4d,", la);
        if (j == 9) {
            fprintf(out, "\n"); lineno++;
            j = 0;
        }
        else {
            j++;
        }
    }
    /* Add extra entries to the end of the yy_lookahead[] table so that
    ** yy_shift_ofst[]+iToken will always be a valid index into the array,
    ** even for the largest possible value of yy_shift_ofst[] and iToken. */
    nLookAhead = lemp.nterminal + lemp.nactiontab;
    while (i < nLookAhead) {
        if (j == 0) fprintf(out, " /* %5d */ ", i);
        fprintf(out, " %4d,", lemp.nterminal);
        if (j == 9) {
            fprintf(out, "\n"); lineno++;
            j = 0;
        }
        else {
            j++;
        }
        i++;
    }
    if (j > 0) { fprintf(out, "\n"); lineno++; }
    fprintf(out, "};\n"); lineno++;

    /* Output the yy_shift_ofst[] table */
    n = lemp.nxstate;
    while (n > 0 && lemp.sorted[n - 1]->iTknOfst == NO_OFFSET) n--;
    fprintf(out, "#define YY_SHIFT_COUNT    (%d)\n", n - 1); lineno++;
    fprintf(out, "#define YY_SHIFT_MIN      (%d)\n", mnTknOfst); lineno++;
    fprintf(out, "#define YY_SHIFT_MAX      (%d)\n", mxTknOfst); lineno++;
    fprintf(out, "static const %s yy_shift_ofst[] = {\n",
        minimum_size_type(mnTknOfst, lemp.nterminal + lemp.nactiontab, &sz));
    lineno++;
    lemp.tablesize += n * sz;
    for (i = j = 0; i < n; i++) {
        int ofst;
        stp = lemp.sorted[i];
        ofst = stp->iTknOfst;
        if (ofst == NO_OFFSET) ofst = lemp.nactiontab;
        if (j == 0) fprintf(out, " /* %5d */ ", i);
        fprintf(out, " %4d,", ofst);
        if (j == 9 || i == n - 1) {
            fprintf(out, "\n"); lineno++;
            j = 0;
        }
        else {
            j++;
        }
    }
    fprintf(out, "};\n"); lineno++;

    /* Output the yy_reduce_ofst[] table */
    n = lemp.nxstate;
    while (n > 0 && lemp.sorted[n - 1]->iNtOfst == NO_OFFSET) n--;
    fprintf(out, "#define YY_REDUCE_COUNT (%d)\n", n - 1); lineno++;
    fprintf(out, "#define YY_REDUCE_MIN   (%d)\n", mnNtOfst); lineno++;
    fprintf(out, "#define YY_REDUCE_MAX   (%d)\n", mxNtOfst); lineno++;
    fprintf(out, "static const %s yy_reduce_ofst[] = {\n",
        minimum_size_type(mnNtOfst - 1, mxNtOfst, &sz)); lineno++;
    lemp.tablesize += n * sz;
    for (i = j = 0; i < n; i++) {
        int ofst;
        stp = lemp.sorted[i];
        ofst = stp->iNtOfst;
        if (ofst == NO_OFFSET) ofst = mnNtOfst - 1;
        if (j == 0) fprintf(out, " /* %5d */ ", i);
        fprintf(out, " %4d,", ofst);
        if (j == 9 || i == n - 1) {
            fprintf(out, "\n"); lineno++;
            j = 0;
        }
        else {
            j++;
        }
    }
    fprintf(out, "};\n"); lineno++;

    /* Output the default action table */
    fprintf(out, "static const YYACTIONTYPE yy_default[] = {\n"); lineno++;
    n = lemp.nxstate;
    lemp.tablesize += n * szActionType;
    for (i = j = 0; i < n; i++) {
        stp = lemp.sorted[i];
        if (j == 0) fprintf(out, " /* %5d */ ", i);
        if (stp->iDfltReduce < 0) {
            fprintf(out, " %4d,", lemp.errAction);
        }
        else {
            fprintf(out, " %4d,", stp->iDfltReduce + lemp.minReduce);
        }
        if (j == 9 || i == n - 1) {
            fprintf(out, "\n"); lineno++;
            j = 0;
        }
        else {
            j++;
        }
    }
    fprintf(out, "};\n"); lineno++;
    tplt_xfer(lemp.name, in, out, &lineno);

    /* Generate the table of fallback tokens.
    */
    if (lemp.has_fallback) {
        const int mx = lemp.nterminal - 1;
        /* 2019-08-28:  Generate fallback entries for every token to avoid
        ** having to do a range check on the index */
        /* while( mx>0 && lemp.symbols[mx]->fallback==0 ){ mx--; } */
        lemp.tablesize += (mx + 1) * szCodeType;
        for (i = 0; i <= mx; i++) {
            const symbol* p = lemp.symbols[i];
            if (p->fallback == nullptr) {
                fprintf(out, "    0,  /* %10s => nothing */\n", p->name);
            }
            else {
                fprintf(out, "  %3d,  /* %10s => %s */\n", p->fallback->index,
                    p->name, p->fallback->name);
            }
            lineno++;
        }
    }
    tplt_xfer(lemp.name, in, out, &lineno);

    /* Generate a table containing the symbolic name of every symbol
    */
    for (i = 0; i < lemp.nsymbol; i++) {
        lemon_sprintf(line, "\"%s\",", lemp.symbols[i]->name);
        fprintf(out, "  /* %4d */ \"%s\",\n", i, lemp.symbols[i]->name); lineno++;
    }
    tplt_xfer(lemp.name, in, out, &lineno);

    /* Generate a table containing a text string that describes every
    ** rule in the rule set of the grammar.  This information is used
    ** when tracing REDUCE actions.
    */
    for (i = 0, rp = lemp.rule; rp; rp = rp->next, i++) {
        assert(rp->iRule == i);
        fprintf(out, " /* %3d */ \"", i);
        writeRuleText(out, rp);
        fprintf(out, "\",\n"); lineno++;
    }
    tplt_xfer(lemp.name, in, out, &lineno);

    /* Generate code which executes every time a symbol is popped from
    ** the stack while processing errors or while destroying the parser.
    ** (In other words, generate the %destructor actions)
    */
    if (lemp.tokendest) {
        int once = 1;
        for (i = 0; i < lemp.nsymbol; i++) {
            const symbol* sp = lemp.symbols[i];
            if (sp == nullptr || sp->type != symbol_type::TERMINAL) continue;
            if (once) {
                fprintf(out, "      /* TERMINAL Destructor */\n"); lineno++;
                once = 0;
            }
            fprintf(out, "    case %d: /* %s */\n", sp->index, sp->name); lineno++;
        }
        for (i = 0; i < lemp.nsymbol && lemp.symbols[i]->type != symbol_type::TERMINAL; i++);
        if (i < lemp.nsymbol) {
            emit_destructor_code(out, lemp.symbols[i], lemp, &lineno);
            fprintf(out, "      break;\n"); lineno++;
        }
    }
    if (lemp.vardest) {
        symbol* dflt_sp = nullptr;
        int once = 1;
        for (i = 0; i < lemp.nsymbol; i++) {
            symbol* sp = lemp.symbols[i];
            if (sp == nullptr || sp->type == symbol_type::TERMINAL ||
                sp->index <= 0 || sp->destructor != nullptr) continue;
            if (once) {
                fprintf(out, "      /* Default NON-TERMINAL Destructor */\n"); lineno++;
                once = 0;
            }
            fprintf(out, "    case %d: /* %s */\n", sp->index, sp->name); lineno++;
            dflt_sp = sp;
        }
        if (dflt_sp != nullptr) {
            emit_destructor_code(out, dflt_sp, lemp, &lineno);
        }
        fprintf(out, "      break;\n"); lineno++;
    }
    for (i = 0; i < lemp.nsymbol; i++) {
        const symbol* sp = lemp.symbols[i];
        if (sp == nullptr || sp->type == symbol_type::TERMINAL || sp->destructor == nullptr) continue;
        if (sp->destLineno < 0) continue;  /* Already emitted */
        fprintf(out, "    case %d: /* %s */\n", sp->index, sp->name); lineno++;

        /* Combine duplicate destructors into a single case */
        for (j = i + 1; j < lemp.nsymbol; j++) {
            symbol* sp2 = lemp.symbols[j];
            if (sp2 && sp2->type != symbol_type::TERMINAL && sp2->destructor
                && sp2->dtnum == sp->dtnum
                && strcmp(sp->destructor, sp2->destructor) == 0) {
                fprintf(out, "    case %d: /* %s */\n",
                    sp2->index, sp2->name); lineno++;
                sp2->destLineno = -1;  /* Avoid emitting this destructor again */
            }
        }

        emit_destructor_code(out, lemp.symbols[i], lemp, &lineno);
        fprintf(out, "      break;\n"); lineno++;
    }
    tplt_xfer(lemp.name, in, out, &lineno);

    /* Generate code which executes whenever the parser stack overflows */
    tplt_print(out, lemp, lemp.overflow, &lineno);
    tplt_xfer(lemp.name, in, out, &lineno);

    /* Generate the tables of rule information.  yyRuleInfoLhs[] and
    ** yyRuleInfoNRhs[].
    **
    ** Note: This code depends on the fact that rules are number
    ** sequentially beginning with 0.
    */
    for (i = 0, rp = lemp.rule; rp; rp = rp->next, i++) {
        fprintf(out, "  %4d,  /* (%d) ", rp->lhs->index, i);
        rule_print(out, rp);
        fprintf(out, " */\n"); lineno++;
    }
    tplt_xfer(lemp.name, in, out, &lineno);
    for (i = 0, rp = lemp.rule; rp; rp = rp->next, i++) {
        fprintf(out, "  %3d,  /* (%d) ", -rp->nrhs, i);
        rule_print(out, rp);
        fprintf(out, " */\n"); lineno++;
    }
    tplt_xfer(lemp.name, in, out, &lineno);

    /* Generate code which execution during each REDUCE action */
    i = 0;
    for (rp = lemp.rule; rp; rp = rp->next) {
        i += translate_code(lemp, *rp);
    }
    if (i) {
        fprintf(out, "        YYMINORTYPE yylhsminor;\n"); lineno++;
    }
    /* First output rules other than the default: rule */
    for (rp = lemp.rule; rp; rp = rp->next) {
        rule* rp2;               /* Other rules with the same action */
        if (rp->codeEmitted == Boolean::LEMON_TRUE) continue;
        if (rp->noCode == Boolean::LEMON_TRUE) {
            /* No C code actions, so this will be part of the "default:" rule */
            continue;
        }
        fprintf(out, "      case %d: /* ", rp->iRule);
        writeRuleText(out, rp);
        fprintf(out, " */\n"); lineno++;
        for (rp2 = rp->next; rp2; rp2 = rp2->next) {
            if (rp2->code == rp->code && rp2->codePrefix == rp->codePrefix
                && rp2->codeSuffix == rp->codeSuffix) {
                fprintf(out, "      case %d: /* ", rp2->iRule);
                writeRuleText(out, rp2);
                fprintf(out, " */ yytestcase(yyruleno==%d);\n", rp2->iRule); lineno++;
                rp2->codeEmitted = Boolean::LEMON_TRUE;
            }
        }
        emit_code(out, rp, lemp, &lineno);
        fprintf(out, "        break;\n"); lineno++;
        rp->codeEmitted = Boolean::LEMON_TRUE;
    }
    /* Finally, output the default: rule.  We choose as the default: all
    ** empty actions. */
    fprintf(out, "      default:\n"); lineno++;
    for (rp = lemp.rule; rp; rp = rp->next) {
        if (rp->codeEmitted == Boolean::LEMON_TRUE) continue;
        assert(rp->noCode == Boolean::LEMON_TRUE);
        fprintf(out, "      /* (%d) ", rp->iRule);
        writeRuleText(out, rp);
        if (rp->neverReduce == Boolean::LEMON_TRUE) {
            fprintf(out, " (NEVER REDUCES) */ assert(yyruleno!=%d);\n",
                rp->iRule); lineno++;
        }
        else if (rp->doesReduce == Boolean::LEMON_TRUE) {
            fprintf(out, " */ yytestcase(yyruleno==%d);\n", rp->iRule); lineno++;
        }
        else {
            fprintf(out, " (OPTIMIZED OUT) */ assert(yyruleno!=%d);\n",
                rp->iRule); lineno++;
        }
    }
    fprintf(out, "        break;\n"); lineno++;
    tplt_xfer(lemp.name, in, out, &lineno);

    /* Generate code which executes if a parse fails */
    tplt_print(out, lemp, lemp.failure, &lineno);
    tplt_xfer(lemp.name, in, out, &lineno);

    /* Generate code which executes when a syntax error occurs */
    tplt_print(out, lemp, lemp.error, &lineno);
    tplt_xfer(lemp.name, in, out, &lineno);

    /* Generate code which executes when the parser accepts its input */
    tplt_print(out, lemp, lemp.accept, &lineno);
    tplt_xfer(lemp.name, in, out, &lineno);

    /* Append any addition code the user desires */
    tplt_print(out, lemp, lemp.extracode, &lineno);

    acttab_free(pActtab);
    fclose(in);
    fclose(out);
    if (sql) fclose(sql);
    return;
}

/* Generate a header file for the parser */
void ReportHeader(lemon& lemp)
{
    FILE* out, * in;
    const char* prefix;
    char line[LINESIZE];
    char pattern[LINESIZE];
    int i;

    if (lemp.tokenprefix) prefix = lemp.tokenprefix;
    else                    prefix = "";
    in = file_open(lemp, ".h", "rb");
    if (in) {
        int nextChar;
        for (i = 1; i < lemp.nterminal && fgets(line, LINESIZE, in); i++) {
            lemon_sprintf(pattern, "#define %s%-30s %3d\n",
                prefix, lemp.symbols[i]->name, i);
            if (strcmp(line, pattern)) break;
        }
        nextChar = fgetc(in);
        fclose(in);
        if (i == lemp.nterminal && nextChar == EOF) {
            /* No change in the file.  Don't rewrite it. */
            return;
        }
    }
    out = file_open(lemp, ".h", "wb");
    if (out) {
        for (i = 1; i < lemp.nterminal; i++) {
            fprintf(out, "#define %s%-30s %3d\n", prefix, lemp.symbols[i]->name, i);
        }
        fclose(out);
    }
    return;
}

/* Reduce the size of the action tables, if possible, by making use
** of defaults.
**
** In this version, we take the most frequent REDUCE action and make
** it the default.  Except, there is no default if the wildcard token
** is a possible look-ahead.
*/
void CompressTables(lemon& lemp)
{
    state* stp;
    action* ap, * ap2, * nextap;
    rule* rp, * rp2, * rbest;
    int nbest, n;
    int i;
    int usesWildcard;

    for (i = 0; i < lemp.nstate; i++) {
        stp = lemp.sorted[i];
        nbest = 0;
        rbest = nullptr;
        usesWildcard = 0;

        for (ap = stp->ap; ap; ap = ap->next) {
            if (ap->type == e_action::SHIFT && ap->sp == lemp.wildcard) {
                usesWildcard = 1;
            }
            if (ap->type != e_action::REDUCE) continue;
            rp = ap->x.rp;
            if (rp->lhsStart) continue;
            if (rp == rbest) continue;
            n = 1;
            for (ap2 = ap->next; ap2; ap2 = ap2->next) {
                if (ap2->type != e_action::REDUCE) continue;
                rp2 = ap2->x.rp;
                if (rp2 == rbest) continue;
                if (rp2 == rp) n++;
            }
            if (n > nbest) {
                nbest = n;
                rbest = rp;
            }
        }

        /* Do not make a default if the number of rules to default
        ** is not at least 1 or if the wildcard token is a possible
        ** lookahead.
        */
        if (nbest < 1 || usesWildcard) continue;


        /* Combine matching REDUCE actions into a single default */
        for (ap = stp->ap; ap; ap = ap->next) {
            if (ap->type == e_action::REDUCE && ap->x.rp == rbest) break;
        }
        assert(ap);
        ap->sp = Symbol_new("{default}");
        for (ap = ap->next; ap; ap = ap->next) {
            if (ap->type == e_action::REDUCE && ap->x.rp == rbest) ap->type = e_action::NOT_USED;
        }
        stp->ap = Action_sort(stp->ap);

        for (ap = stp->ap; ap; ap = ap->next) {
            if (ap->type == e_action::SHIFT) break;
            if (ap->type == e_action::REDUCE && ap->x.rp != rbest) break;
        }
        if (ap == nullptr) {
            stp->autoReduce = 1;
            stp->pDfltReduce = rbest;
        }
    }

    /* Make a second pass over all states and actions.  Convert
    ** every action that is a SHIFT to an autoReduce state into
    ** a SHIFTREDUCE action.
    */
    for (i = 0; i < lemp.nstate; i++) {
        stp = lemp.sorted[i];
        for (ap = stp->ap; ap; ap = ap->next) {
            state* pNextState;
            if (ap->type != e_action::SHIFT) continue;
            pNextState = ap->x.stp;
            if (pNextState->autoReduce && pNextState->pDfltReduce != nullptr) {
                ap->type = e_action::SHIFTREDUCE;
                ap->x.rp = pNextState->pDfltReduce;
            }
        }
    }

    /* If a SHIFTREDUCE action specifies a rule that has a single RHS term
    ** (meaning that the SHIFTREDUCE will land back in the state where it
    ** started) and if there is no C-code associated with the reduce action,
    ** then we can go ahead and convert the action to be the same as the
    ** action for the RHS of the rule.
    */
    for (i = 0; i < lemp.nstate; i++) {
        stp = lemp.sorted[i];
        for (ap = stp->ap; ap; ap = nextap) {
            nextap = ap->next;
            if (ap->type != e_action::SHIFTREDUCE) continue;
            rp = ap->x.rp;
            if (rp->noCode == Boolean::LEMON_FALSE) continue;
            if (rp->nrhs != 1) continue;
#if 1
            /* Only apply this optimization to non-terminals.  It would be OK to
            ** apply it to terminal symbols too, but that makes the parser tables
            ** larger. */
            if (ap->sp->index < lemp.nterminal) continue;
#endif
            /* If we reach this point, it means the optimization can be applied */
            nextap = ap;
            for (ap2 = stp->ap; ap2 && (ap2 == ap || ap2->sp != rp->lhs); ap2 = ap2->next) {}
            assert(ap2 != 0);
            ap->spOpt = ap2->sp;
            ap->type = ap2->type;
            ap->x = ap2->x;
        }
    }
}


/*
** Compare two states for sorting purposes.  The smaller state is the
** one with the most non-terminal actions.  If they have the same number
** of non-terminal actions, then the smaller is the one with the most
** token actions.
*/
static int stateResortCompare(const void* a, const void* b) {
    const state* pA = *(const state**)a;
    const state* pB = *(const state**)b;
    int n;

    n = pB->nNtAct - pA->nNtAct;
    if (n == 0) {
        n = pB->nTknAct - pA->nTknAct;
        if (n == 0) {
            n = pB->statenum - pA->statenum;
        }
    }
    assert(n != 0);
    return n;
}


/*
** Renumber and resort states so that states with fewer choices
** occur at the end.  Except, keep state 0 as the first state.
*/
void ResortStates(lemon& lemp)
{
    int i;
    state* stp;
    action* ap;

    for (i = 0; i < lemp.nstate; i++) {
        stp = lemp.sorted[i];
        stp->nTknAct = stp->nNtAct = 0;
        stp->iDfltReduce = -1; /* Init dflt action to "syntax error" */
        stp->iTknOfst = NO_OFFSET;
        stp->iNtOfst = NO_OFFSET;
        for (ap = stp->ap; ap; ap = ap->next) {
            const int iAction = compute_action(lemp, ap);
            if (iAction >= 0) {
                if (ap->sp->index < lemp.nterminal) {
                    stp->nTknAct++;
                }
                else if (ap->sp->index < lemp.nsymbol) {
                    stp->nNtAct++;
                }
                else {
                    assert(stp->autoReduce == 0 || stp->pDfltReduce == ap->x.rp);
                    stp->iDfltReduce = iAction;
                }
            }
        }
    }
    qsort(&lemp.sorted[1], lemp.nstate - 1, sizeof(lemp.sorted[0]),
        stateResortCompare);
    for (i = 0; i < lemp.nstate; i++) {
        lemp.sorted[i]->statenum = i;
    }
    lemp.nxstate = lemp.nstate;
    while (lemp.nxstate > 1 && lemp.sorted[lemp.nxstate - 1]->autoReduce) {
        lemp.nxstate--;
    }
}


/***************** From the file "set.c" ************************************/
/*
** Set manipulation routines for the LEMON parser generator.
*/

static int size = 0;

/* Set the set size */
void SetSize(int n)
{
    size = n + 1;
}

/* Allocate a new set */
char* SetNew(void) {
    char* s;
    s = (char*)calloc(size, 1);
    if (s == nullptr) {
        memory_error();
    }
    return s;
}

/* Deallocate a set */
void SetFree(char* s)
{
    delete[] s;
}

/* Add a new element to the set.  Return TRUE if the element was added
** and FALSE if it was already there. */
int SetAdd(char* s, int e)
{
    int rv;
    assert(e >= 0 && e < size);
    rv = s[e];
    s[e] = 1;
    return !rv;
}

/* Add every element of s2 to s1.  Return TRUE if s1 changes. */
int SetUnion(char* s1, const char* s2)
{
    int i, progress;
    progress = 0;
    for (i = 0; i < size; i++) {
        if (s2[i] == 0) continue;
        if (s1[i] == 0) {
            progress = 1;
            s1[i] = 1;
        }
    }
    return progress;
}
/********************** From the file "table.c" ****************************/
/*
** All code in this file has been automatically generated
** from a specification in the file
**              "table.q"
** by the associative array code building program "aagen".
** Do not edit this file!  Instead, edit the specification
** file, then rerun aagen.
*/
/*
** Code for processing tables in the LEMON parser generator.
*/

static std::unordered_set<std::string> x1a_set;

/* Works like strdup, sort of.  Save a string in malloced memory, but
** keep strings in a table so that the same string is not in more
** than one place.
*/
const char* Strsafe(std::string_view y)
{
    if (y.empty()) return nullptr;
    auto [z, inserted] = x1a_set.emplace(y);
    if (z == x1a_set.end())
    {
        memory_error();
    }

    return z->data();
}

/* Allocate a new associative array */
void Strsafe_init(void) {
    x1a_set.clear();
    x1a_set.reserve(1024);
}

namespace Symbol
{
static std::map<std::string_view, Symbol::symbol> x2a_map;

/* Return a pointer to the (terminal or nonterminal) symbol "x".
** Create a new symbol if this is the first time "x" has been seen.
*/
symbol* Symbol_new(const char* x)
{
    symbol* sp;

    sp = Symbol_find(x);
    if (sp == nullptr) {
        symbol s;

        s.name = Strsafe(x);
        s.index = x2a_map.size()+1; //index in the order as found
        s.type = ISUPPER(*x) ? symbol_type::TERMINAL : symbol_type::NONTERMINAL;
        s.rule = nullptr;
        s.fallback = nullptr;
        s.prec = -1;
        s.assoc = e_assoc::UNK;
        s.firstset = nullptr;
        s.lambda = Boolean::LEMON_FALSE;
        s.useCnt = 0;
        s.destructor = nullptr;
        s.destLineno = 0;
        s.datatype = nullptr;
        s.dtnum = 0;
        s.bContent = 0;
        s.nsubsym = 0;
        s.subsym = nullptr;

        auto [inserted_s, e] = x2a_map.emplace(s.name, s);
        sp = &inserted_s->second;
    }
    ++sp->useCnt;
    return sp;
}

/* Compare two symbols for sorting purposes.  Return negative,
** zero, or positive if a is less then, equal to, or greater
** than b.
**
** Symbols that begin with upper case letters (terminals or tokens)
** must sort before symbols that begin with lower case letters
** (non-terminals).  And MULTITERMINAL symbols (created using the
** %token_class directive) must sort at the very end. Other than
** that, the order does not matter.
**
** We find experimentally that leaving the symbols in their original
** order (the order they appeared in the grammar file) gives the
** smallest parser tables in SQLite.
*/
bool Symbolcmpp(const symbol* a, const symbol* b)
{
    const int i1 = a->type == symbol_type::MULTITERMINAL ? 3 : a->name[0] > 'Z' ? 2 : 1;
    const int i2 = b->type == symbol_type::MULTITERMINAL ? 3 : b->name[0] > 'Z' ? 2 : 1;
    
    return i1 == i2 ? a->index < b->index : i1 < i2;
}

/* Allocate a new associative array */
void Symbol_init(void) {
    x2a_map.clear();
}

/* Return a pointer to data assigned to the given key.  Return NULL
** if no such key. */
symbol* Symbol_find(std::string_view key)
{
    auto res = x2a_map.find(key);
    return (res != x2a_map.end()) ? &res->second : nullptr;
}

/* Return the size of the array */
int Symbol_count()
{
    return x2a_map.size();
}

/* Return an array of pointers to all data in the table.
** The array is obtained from malloc.  Return NULL if memory allocation
** problems, or if the array is empty. */
std::vector<symbol*> Symbol_arrayof()
{
    std::vector<symbol*> vs;
    vs.reserve(x2a_map.size());

    std::transform(x2a_map.begin(), x2a_map.end(), std::back_inserter(vs), [](auto& e)
        {
            return &e.second;
        });

    return vs;
}

}

namespace Config
{
/* Compare two configurations */
int Configcmp(const char* _a, const char* _b)
{
    const config* a = (config*)_a;
    const config* b = (config*)_b;
    int x;
    x = a->rp->index - b->rp->index;
    if (x == 0) x = a->dot - b->dot;
    return x;
}

/* Hash a configuration */
PRIVATE unsigned confighash(const config* a)
{
    unsigned h = 0;
    h = h * 571 + a->rp->index * 37 + a->dot;
    return h;
}
}

namespace State
{
/* Compare two states */
PRIVATE int statecmp(config* a, config* b)
{
    int rc;
    for (rc = 0; rc == 0 && a && b; a = a->bp, b = b->bp) {
        rc = a->rp->index - b->rp->index;
        if (rc == 0) rc = a->dot - b->dot;
    }
    if (rc == 0) {
        if (a) rc = 1;
        if (b) rc = -1;
    }
    return rc;
}

/* Hash a state */
PRIVATE unsigned statehash(config* a)
{
    unsigned h = 0;
    while (a) {
        h = h * 571 + a->rp->index * 37 + a->dot;
        a = a->bp;
    }
    return h;
}

/* Allocate a new state structure */
state* State_new()
{
    state* newstate;
    newstate = (state*)calloc(1, sizeof(state));
    MemoryCheck(newstate);
    return newstate;
}

/* There is one instance of this structure for every data element
** in an associative array of type "x3".
*/
struct s_x3node {
    state* data;                  /* The data */
    config* key;                   /* The key */
    s_x3node* next;   /* Next entry with the same hash */
    s_x3node** from;  /* Previous link */
};

using x3node = s_x3node;

/* There is one instance of the following structure for each
** associative array of type "x3".
*/
struct s_x3 {
    int size;               /* The number of available slots. */
                            /*   Must be a power of 2 greater than or */
                            /*   equal to 1 */
    int count;              /* Number of currently slots filled */
    s_x3node* tbl;  /* The data stored here */
    s_x3node** ht;  /* Hash table for lookups */
};

/* There is only one instance of the array, which is the following */
static s_x3* x3a;

/* Allocate a new associative array */
void State_init(void) {
    if (x3a) return;
    x3a = new s_x3;
    if (x3a) {
        x3a->size = 128;
        x3a->count = 0;
        x3a->tbl = (x3node*)calloc(128, sizeof(x3node) + sizeof(x3node*));
        if (x3a->tbl == nullptr) {
            delete x3a;
            x3a = nullptr;
        }
        else {
            int i;
            x3a->ht = (x3node**)&(x3a->tbl[128]);
            for (i = 0; i < 128; i++) x3a->ht[i] = nullptr;
        }
    }
}
/* Insert a new record into the array.  Return TRUE if successful.
** Prior data with the same key is NOT overwritten */
int State_insert(state* data, config* key)
{
    x3node* np;
    unsigned h;
    unsigned ph;

    if (x3a == nullptr) return 0;
    ph = statehash(key);
    h = ph & (x3a->size - 1);
    np = x3a->ht[h];
    while (np) {
        if (statecmp(np->key, key) == 0) {
            /* An existing entry with the same key is found. */
            /* Fail because overwrite is not allows. */
            return 0;
        }
        np = np->next;
    }
    if (x3a->count >= x3a->size) {
        /* Need to make the hash table bigger */
        int i, arrSize;
        s_x3 array;
        array.size = arrSize = x3a->size * 2;
        array.count = x3a->count;
        array.tbl = (x3node*)calloc(arrSize, sizeof(x3node) + sizeof(x3node*));
        if (array.tbl == 0) return 0;  /* Fail due to malloc failure */
        array.ht = (x3node**)&(array.tbl[arrSize]);
        for (i = 0; i < arrSize; i++) array.ht[i] = nullptr;
        for (i = 0; i < x3a->count; i++) {
            x3node* oldnp, * newnp;
            oldnp = &(x3a->tbl[i]);
            h = statehash(oldnp->key) & (arrSize - 1);
            newnp = &(array.tbl[i]);
            if (array.ht[h]) array.ht[h]->from = &(newnp->next);
            newnp->next = array.ht[h];
            newnp->key = oldnp->key;
            newnp->data = oldnp->data;
            newnp->from = &(array.ht[h]);
            array.ht[h] = newnp;
        }
        free(x3a->tbl);
        *x3a = array;
    }
    /* Insert the new data */
    h = ph & (x3a->size - 1);
    np = &(x3a->tbl[x3a->count++]);
    np->key = key;
    np->data = data;
    if (x3a->ht[h]) x3a->ht[h]->from = &(np->next);
    np->next = x3a->ht[h];
    x3a->ht[h] = np;
    np->from = &(x3a->ht[h]);
    return 1;
}

/* Return a pointer to data assigned to the given key.  Return NULL
** if no such key. */
state* State_find(config* key)
{
    unsigned h;
    x3node* np;

    if (x3a == nullptr) return nullptr;
    h = statehash(key) & (x3a->size - 1);
    np = x3a->ht[h];
    while (np) {
        if (statecmp(np->key, key) == 0) break;
        np = np->next;
    }
    return np ? np->data : nullptr;
}

/* Return an array of pointers to all data in the table.
** The array is obtained from malloc.  Return NULL if memory allocation
** problems, or if the array is empty. */
state** State_arrayof(void)
{
    state** array;
    int i, arrSize;
    if (x3a == nullptr) return nullptr;
    arrSize = x3a->count;
    array = (state**)calloc(arrSize, sizeof(state*));
    if (array) {
        for (i = 0; i < arrSize; i++) array[i] = x3a->tbl[i].data;
    }
    return array;
}

/* There is one instance of this structure for every data element
** in an associative array of type "x4".
*/
struct s_x4node {
    config* data;                  /* The data */
    s_x4node* next;   /* Next entry with the same hash */
    s_x4node** from;  /* Previous link */
};

using x4node = s_x4node;

/* There is one instance of the following structure for each
** associative array of type "x4".
*/
struct s_x4 {
    int size;               /* The number of available slots. */
                            /*   Must be a power of 2 greater than or */
                            /*   equal to 1 */
    int count;              /* Number of currently slots filled */
    s_x4node* tbl;  /* The data stored here */
    s_x4node** ht;  /* Hash table for lookups */
};

/* There is only one instance of the array, which is the following */
static s_x4* x4a;

}

namespace Configtable
{
/* Allocate a new associative array */
void Configtable_init(void) {
    if (x4a) return;
    x4a = new s_x4;
    if (x4a) {
        x4a->size = 64;
        x4a->count = 0;
        x4a->tbl = (x4node*)calloc(64, sizeof(x4node) + sizeof(x4node*));
        if (x4a->tbl == 0) {
            delete x4a;
            x4a = 0;
        }
        else {
            int i;
            x4a->ht = (x4node**)&(x4a->tbl[64]);
            for (i = 0; i < 64; i++) x4a->ht[i] = nullptr;
        }
    }
}

/* Insert a new record into the array.  Return TRUE if successful.
** Prior data with the same key is NOT overwritten */
int Configtable_insert(config* data)
{
    x4node* np;
    unsigned h;
    unsigned ph;

    if (x4a == nullptr) return 0;
    ph = confighash(data);
    h = ph & (x4a->size - 1);
    np = x4a->ht[h];
    while (np) {
        if (Config::Configcmp((const char*)np->data, (const char*)data) == 0) {
            /* An existing entry with the same key is found. */
            /* Fail because overwrite is not allows. */
            return 0;
        }
        np = np->next;
    }
    if (x4a->count >= x4a->size) {
        /* Need to make the hash table bigger */
        int i, arrSize;
        s_x4 array;
        array.size = arrSize = x4a->size * 2;
        array.count = x4a->count;
        array.tbl = (x4node*)calloc(arrSize, sizeof(x4node) + sizeof(x4node*));
        if (array.tbl == nullptr) return 0;  /* Fail due to malloc failure */
        array.ht = (x4node**)&(array.tbl[arrSize]);
        for (i = 0; i < arrSize; i++) array.ht[i] = nullptr;
        for (i = 0; i < x4a->count; i++) {
            x4node* oldnp, * newnp;
            oldnp = &(x4a->tbl[i]);
            h = confighash(oldnp->data) & (arrSize - 1);
            newnp = &(array.tbl[i]);
            if (array.ht[h]) array.ht[h]->from = &(newnp->next);
            newnp->next = array.ht[h];
            newnp->data = oldnp->data;
            newnp->from = &(array.ht[h]);
            array.ht[h] = newnp;
        }
        free(x4a->tbl);
        *x4a = array;
    }
    /* Insert the new data */
    h = ph & (x4a->size - 1);
    np = &(x4a->tbl[x4a->count++]);
    np->data = data;
    if (x4a->ht[h]) x4a->ht[h]->from = &(np->next);
    np->next = x4a->ht[h];
    x4a->ht[h] = np;
    np->from = &(x4a->ht[h]);
    return 1;
}

/* Return a pointer to data assigned to the given key.  Return NULL
** if no such key. */
config* Configtable_find(const config* key)
{
    int h;
    x4node* np;

    if (x4a == nullptr) return nullptr;
    h = confighash(key) & (x4a->size - 1);
    np = x4a->ht[h];
    while (np) {
        if (Config::Configcmp((const char*)np->data, (const char*)key) == 0) break;
        np = np->next;
    }
    return np ? np->data : nullptr;
}

/* Remove all data from the table.  Pass each data to the function "f"
** as it is removed.  ("f" may be null to avoid this step.) */
void Configtable_clear(int(*f)(config*))
{
    int i;
    if (x4a == nullptr || x4a->count == 0) return;
    if (f) for (i = 0; i < x4a->count; i++) (*f)(x4a->tbl[i].data);
    for (i = 0; i < x4a->size; i++) x4a->ht[i] = nullptr;
    x4a->count = 0;
    return;
}
}
/*-
 * Copyright (c) 2004 Nik Clayton
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif

#include "shf.private.h"
#include "shf.h"
#define vfprintf(FILE, FMT, AP) shf_log_vfprintf(FILE, FMT, AP); /* minimal change to redirect tap output into shf logging :-) */
#define  fprintf(FILE, ARGS...) shf_log_fprintf(FILE, ARGS);
#define  fputs(STRING, FILE)    shf_log_fputs(STRING, FILE);
#define  fputc(CHAR, FILE)      shf_log_fputc(CHAR, FILE);

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define flockfile(file)
#define funlockfile(file)
#include <process.h>
#define getpid()          _getpid()

#else /* UNIX */
#include <unistd.h>
#include <signal.h>
#endif

#if defined(__APPLE__) || defined(__FreeBSD__)
#define sighandler_t sig_t
#endif
#if defined(__APPLE__)
#define _NSIG        __DARWIN_NSIG
#elif defined(__FreeBSD__)
#define _NSIG        NSIG
#endif

#include "tap.h"

static int          no_plan        = 0;
static int          skip_all       = 0;
static int          have_plan      = 0;
static unsigned int test_count     = 0; /* Number of tests that have been run */
static unsigned int e_tests        = 0; /* Expected number of tests to run */
static unsigned int failures       = 0; /* Number of tests that failed */
static char *       todo_msg       = NULL;
static const char * todo_msg_fixed = "libtap malloc issue";
static int          todo           = 0;
static int          test_died      = 0;
static FILE *       tap_out        = NULL;
static const char * test_case_name = NULL;
static unsigned     tap_flags      = 0;
static int          test_pid;

#ifndef _WIN32
static sighandler_t tap_saved_sig_handler[_NSIG];
#endif

/* Encapsulate the pthread code in a conditional.  In the absence of
   libpthread the code does nothing */
#ifdef HAVE_LIBPTHREAD
#include <pthread.h>
static pthread_mutex_t M = PTHREAD_MUTEX_INITIALIZER;
# define LOCK pthread_mutex_lock(&M)
# define UNLOCK pthread_mutex_unlock(&M)
#else
# define LOCK
# define UNLOCK
#endif

static void
_expected_tests(unsigned int tests)
{

    fprintf(tap_out, "1..%d\n", tests);
    e_tests = tests;
}

static int
diagv(const char *fmt, va_list ap)
{
    int len;

    tap_init(stdout);
    fputs("# ", tap_out);
    len = vfprintf(tap_out, fmt, ap);
    fputs("\n", tap_out);

    return len + 3;
}

static void
_diag(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    diagv(fmt, ap);
    va_end(ap);
}

/* Implement our own vasprintf for self-contained implementation on Windows.
 */
static int
my_vasprintf(char **strp, const char *fmt, va_list ap)
{
    char buffer[1024];
    int  ret;

    ret = vsnprintf(buffer, sizeof(buffer), fmt, ap);

    if ((*strp = malloc(ret + 1)) == NULL) {
        return -1;
    }

    strncpy(*strp, buffer, ret + 1);
    return ret;
}

/*
 * Generate a test result.
 *
 * type -- 0 = ok, 1 = is, 2 = is_eq, 3 = is_cmp.
 * got -- For ok: boolean, indicates whether or not the test passed. Else: Value we got.
 * expected -- Value we expected (ignored for ok).
 * cmp -- Compare function (only used for is_cmp).
 * to_str -- Convert object to string (only used for is_cmp).
 * test_name -- the name of the test, may be NULL
 * test_comment -- a comment to print afterwards, may be NULL
 */
unsigned int
_gen_result(int type, const void * got, const void * expected,
            int (*cmp)(const void * got, const void * expected),
            const char * (*to_str)(const void * object), const char *func,
            const char *file, unsigned int line, const char *test_name, ...)
{
    va_list ap;
    char  * local_test_name = NULL;
    char  * c;
    int     name_is_digits;
    int     ok;

    switch (type) {
    case 1:
        ok = (int)(got == expected);
        break;

    case 2:
        ok = (int)(strcmp(got, expected) == 0);
        break;

    case 3:
        ok = (int)((*cmp)(got, expected) == 0);
        break;

    case 4:
        ok = (int)(strncmp(got, expected, (size_t)cmp) == 0);
        break;

    case 5:
        ok = (int)(strstr(got, expected) != NULL);
        break;

    default:
        ok = (long)got;
        break;
    }

    LOCK;

    test_count++;

    /* Start by taking the test name and performing any printf()
       expansions on it */
    if (test_name != NULL) {
        va_start(ap, test_name);
        if (my_vasprintf(&local_test_name, test_name, ap) < 0)
            local_test_name = NULL;
        va_end(ap);

        /* Make sure the test name contains more than digits
           and spaces.  Emit an error message and exit if it
           does */
        if(local_test_name) {
            name_is_digits = 1;
            for(c = local_test_name; *c != '\0'; c++) {
                if(!isdigit(*c) && !isspace(*c)) {
                    name_is_digits = 0;
                    break;
                }
            }

            if(name_is_digits) {
                _diag("    You named your test '%s'.  You shouldn't use numbers for your test names.", local_test_name);
                _diag("    Very confusing.");
            }
        }
    }

    if(!ok) {
        fprintf(tap_out, "not ");
        failures++;
    }

    fprintf(tap_out, "ok %d", test_count);

    if (test_case_name != NULL || test_name != NULL) {
        fprintf(tap_out, " - ");
    }

    if (test_case_name != NULL) {
        fputs(test_case_name, tap_out);
    }

    if(test_name != NULL) {
        if (test_case_name != NULL) {
            fprintf(tap_out, ": ");
        }

        /* Print the test name, escaping any '#' characters it
           might contain */
        if(local_test_name != NULL) {
            flockfile(tap_out);
            for(c = local_test_name; *c != '\0'; c++) {
                if(*c == '#')
                    fputc('\\', tap_out);
                fputc((int)*c, tap_out);
            }
            funlockfile(tap_out);
        } else {    /* my_vasprintf() failed, use a fixed message */
            fprintf(tap_out, "%s", todo_msg_fixed);
        }
    }

    /* If we're in a todo_start() block then flag the test as being
       TODO.  todo_msg should contain the message to print at this
       point.  If it's NULL then asprintf() failed, and we should
       use the fixed message.

       This is not counted as a failure, so decrement the counter if
       the test failed. */
    if (todo) {
        fprintf(tap_out, " # TODO %s", todo_msg ? todo_msg : todo_msg_fixed);
        if(!ok)
            failures--;
    }

    fprintf(tap_out, "\n");

    if (!ok) {
        _diag("    Failed %stest (%s:%s() at line %d)",
              todo ? "(TODO) " : "", file, func, line);

        switch (type) {
        case 1:
            _diag("         got: %ld", (long)got);
            _diag("    expected: %ld", (long)expected);
            break;

        case 2:
            _diag("         got: \"%s\"", (const char *)got);
            _diag("    expected: \"%s\"", (const char *)expected);
            break;

        case 3:
            _diag("         got: %s", (*to_str)(got));
            _diag("    expected: %s", (*to_str)(expected));
            break;

        case 4:
            _diag("         got: %.*s", (long)cmp, (const char *)got);
            _diag("    expected: %.*s", (long)cmp, (const char *)expected);
            break;

        case 5:
            _diag("                    got: \"%s\"", (const char *)got);
            _diag("    expected to contain: \"%s\"", (const char *)expected);
            break;

        default:
            break;
        }
    }

    free(local_test_name);

    UNLOCK;

    if (!ok && (tap_flags & TAP_FLAG_ON_FAILURE_EXIT)) {
        exit(test_count);
    }

    /* We only care (when testing) that ok is positive, but here we
       specifically only want to return 1 or 0 */
    return ok ? 1 : 0;
}

/*
 * Cleanup at the end of the run, produce any final output that might be
 * required.
 */
static void
_cleanup(void)
{
    /* If we forked, don't do cleanup in child! */
    if (getpid() != test_pid)
        return;

    LOCK;

    /* If plan_no_plan() wasn't called, and we don't have a plan,
       and we're not skipping everything, then something happened
       before we could produce any output */
    if(!no_plan && !have_plan && !skip_all) {
        _diag("Looks like your test died before it could output anything.");
        UNLOCK;
        return;
    }

    if(test_died) {
        _diag("Looks like your test died just after %d.", test_count);
        UNLOCK;
        return;
    }

    /* No plan provided, but now we know how many tests were run, and can
       print the header at the end */
    if(!skip_all && (no_plan || !have_plan)) {
        fprintf(tap_out, "1..%d\n", test_count);
    }

    if((have_plan && !no_plan) && e_tests < test_count) {
        _diag("Looks like you planned %d tests but ran %d extra.",
              e_tests, test_count - e_tests);
        if(failures) {
            _diag("Looks like you failed %d tests of %d run.",
                  failures, test_count);
        }
        UNLOCK;
        return;
    }

    if((have_plan || !no_plan) && e_tests > test_count) {
        _diag("Looks like you planned %d tests but only ran %d.",
              e_tests, test_count);
        if(failures) {
            _diag("Looks like you failed %d tests of %d run.",
                  failures, test_count);
        }
        UNLOCK;
        return;
    }

    if(failures)
        _diag("Looks like you failed %d tests of %d.",
              failures, test_count);

    UNLOCK;
}

#ifndef _WIN32
static void
tap_debug_sig_handler(int sig)
{
    diag("tap_debug_sig_hander(sig=%d):", sig);

    if (tap_saved_sig_handler[sig] == SIG_DFL) {
        diag("    default handling: exiting with an error");
        exit(1);
    }

    if (tap_saved_sig_handler[sig] == SIG_IGN) {
        diag("    signal ignored");
    }
    else {
        diag("    forwarding signal to function at %p", tap_saved_sig_handler[sig]);
        (*tap_saved_sig_handler)(sig);
    }
}
#endif

/**
 * Initialise the TAP library.  Will only do so once, however many times it's
 * called. If you don't call this, calling plan_* will, with stdout as the file.
 */
void
tap_init(FILE * out)
{
    static int run_once = 0;

    if(!run_once) {
        tap_out  = out;
        test_pid = getpid();
        atexit(_cleanup);

        /* stdout needs to be unbuffered so that the output appears
           in the same place relative to stderr output as it does
           with Test::Harness */
        setbuf(tap_out, 0);
        run_once = 1;
    }

#ifndef _WIN32
    if (tap_flags & TAP_FLAG_DEBUG) {
        tap_saved_sig_handler[SIGINT]  = signal(SIGINT,  tap_debug_sig_handler);
        tap_saved_sig_handler[SIGTERM] = signal(SIGTERM, tap_debug_sig_handler);
    }
#endif
}

/*
 * Note that there's no plan.
 */
void
plan_no_plan(void)
{

    LOCK;

    tap_init(stdout);

    if(have_plan != 0) {
        fprintf(stderr, "You tried to plan twice!\n");
        test_died = 1;
        UNLOCK;
        exit(255);
    }

    have_plan = 1;
    no_plan = 1;

    UNLOCK;
}

/*
 * Note that the plan is to skip all tests
 */
void
plan_skip_all(const char * reason)
{
    LOCK;

    tap_init(stdout);
    skip_all = 1;

    fprintf(tap_out, "1..0");

    if(reason != NULL) {
        fprintf(tap_out, " # Skip %s", reason);
    }

    fprintf(tap_out, "\n");
    UNLOCK;
}

void
tap_plan(unsigned tests, unsigned flags, FILE * output)
{
    LOCK;

    tap_flags = flags;
    tap_init(output != NULL ? output : stdout);

    if (have_plan != 0) {
        fprintf(stderr, "You tried to plan twice!\n");
        test_died = 1;
        UNLOCK;
        exit(255);
    }

    if (tests == 0) {
        fprintf(stderr, "You said to run 0 tests!  You've got to run something.\n");
        test_died = 1;
        UNLOCK;
        exit(255);
    }

    have_plan = 1;
    _expected_tests(tests);

    UNLOCK;
}

/*
 * Note the number of tests that will be run.
 */
void
plan_tests(unsigned int tests)
{
    tap_plan(tests, 0, NULL);
}

int
diag(const char *fmt, ...)
{
    va_list ap;
    int     len;

    LOCK;

    va_start(ap, fmt);
    len = diagv(fmt, ap);
    va_end(ap);

    UNLOCK;
    return len;
}

void
skip(unsigned int n, const char *fmt, ...)
{
    va_list ap;
    char *  skip_msg;

    LOCK;

    va_start(ap, fmt);

    if (my_vasprintf(&skip_msg, fmt, ap) < 0)
        skip_msg = NULL;

    va_end(ap);

    while(n-- > 0) {
        test_count++;
        fprintf(tap_out, "ok %d # skip %s\n", test_count,
               skip_msg != NULL ?
               skip_msg : "libtap():malloc() failed");
    }

    free(skip_msg);
    UNLOCK;
}

void
todo_start(const char *fmt, ...)
{
    va_list ap;

    LOCK;

    va_start(ap, fmt);

    if (my_vasprintf(&todo_msg, fmt, ap) < 0)
        todo_msg = NULL;

    va_end(ap);
    todo = 1;

    UNLOCK;
}

void
todo_end(void)
{

    LOCK;

    todo = 0;
    free(todo_msg);

    UNLOCK;
}

int
exit_status(void)
{
    int r;

    if (tap_flags & TAP_FLAG_DEBUG) {
        diag("tap_debug: exit_status(): entering");
    }

    LOCK;

    /* If there's no plan, just return the number of failures */
    if(no_plan || !have_plan) {
        UNLOCK;
        return failures;
    }

    /* Ran too many tests?  Return the number of tests that were run
       that shouldn't have been */
    if(e_tests < test_count) {
        r = test_count - e_tests;
        UNLOCK;
        return r;
    }

    /* Return the number of tests that failed + the number of tests
       that weren't run */
    r = failures + e_tests - test_count;
    UNLOCK;

    if (tap_flags & TAP_FLAG_DEBUG) {
        diag("tap_debug: exit_status(): returning %u", r);
    }

    return r;
}

const char *
tap_get_test_case_name(void)
{
    return test_case_name;
}

void
tap_set_test_case_name(const char * name)
{
    test_case_name = name;
}

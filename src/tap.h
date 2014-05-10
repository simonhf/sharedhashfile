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

#ifndef __TAP_H__
#define __TAP_H__

#include <stddef.h>
#include <stdio.h>

#if !defined(_WIN32) || defined(MAKE_MINGW)
#   include <stdint.h>    /* For uintptr_t, which Windows puts in <stddef.h>; Windows doesn't have <stdint.h> */
#endif

/**
 * plan_tests - announce the number of tests you plan to run
 * @tests: the number of tests
 *
 * This should be the first call in your test program: it allows tracing
 * of failures which mean that not all tests are run.
 *
 * If you don't know how many tests will actually be run, assume all of them
 * and use skip() if you don't actually run some tests.
 *
 * Example:
 *      plan_tests(13);
 */
void plan_tests(unsigned int tests);

#ifdef _WIN32
#define __func__ __FUNCTION__
#endif

#if (!defined(__STDC_VERSION__) || __STDC_VERSION__ < 199901L) && !defined(__GNUC__) && !defined(_WIN32)
# error "Needs gcc or C99 compiler for variadic macros."
#else

#define TAP_CMP_CAST    int (*)(const void *, const void *)
#define TAP_TO_STR_CAST const char *(*)(const void *)

#define is(g, e, ...)                  _gen_result(1, (const void *)(uintptr_t)(g), (const void *)(uintptr_t)(e), (TAP_CMP_CAST)0, (TAP_TO_STR_CAST)0, \
                                                   __func__, __FILE__, __LINE__, __VA_ARGS__)

#define is_eq(g, e, ...)               _gen_result(2, (const void *)(g), (const void *)(e), (TAP_CMP_CAST)0, (TAP_TO_STR_CAST)0, \
                                                   __func__, __FILE__, __LINE__, __VA_ARGS__)

#define is_cmp(g, e, cmp, to_str, ...) _gen_result(3, (const void *)(g), (const void *)(e), (cmp), (to_str), \
                                                   __func__, __FILE__, __LINE__, __VA_ARGS__)

/* Careful - 'cmp' is being abused as a size_t in _gen_result(4, ...) */
#define is_strncmp(g, e, len, ...)     _gen_result(4, (const void *)(g), (const void *)(e), (TAP_CMP_CAST)(uintptr_t)(len), (TAP_TO_STR_CAST)0, \
                                                   __func__, __FILE__, __LINE__, __VA_ARGS__)

#define is_strstr(g, e, ...)           _gen_result(5, (const void *)(g), (const void *)(e), (TAP_CMP_CAST)0, (TAP_TO_STR_CAST)0, \
                                                   __func__, __FILE__, __LINE__, __VA_ARGS__)

/**
 * ok1 - Simple conditional test
 * @e: the expression which we expect to be true.
 *
 * This is the simplest kind of test: if the expression is true, the
 * test passes.  The name of the test which is printed will simply be
 * file name, line number, and the expression itself.
 *
 * Example:
 *      ok1(init_subsystem() == 1);
 */
# define ok1(e) ((e) ?                                                                                 \
                 _gen_result(0, (const void *)1, (const void *)0, (TAP_CMP_CAST)0, (TAP_TO_STR_CAST)0, \
                             __func__, __FILE__, __LINE__, "%s", #e) :                                 \
                 _gen_result(0, (const void *)0, (const void *)0, (TAP_CMP_CAST)0, (TAP_TO_STR_CAST)0, \
                             __func__, __FILE__, __LINE__, "%s", #e))

/**
 * ok - Conditional test with a name
 * @e: the expression which we expect to be true.
 * @...: the printf-style name of the test.
 *
 * If the expression is true, the test passes.  The name of the test will be
 * the filename, line number, and the printf-style string.  This can be clearer
 * than simply the expression itself.
 *
 * Example:
 *      ok1(init_subsystem() == 1);
 *      ok(init_subsystem() == 0, "Second initialization should fail");
 */
# define ok(e, ...) ((e) ?                                                                                 \
                     _gen_result(0, (const void *)1, (const void *)0, (TAP_CMP_CAST)0, (TAP_TO_STR_CAST)0, \
                                 __func__, __FILE__, __LINE__, __VA_ARGS__) :                              \
                     _gen_result(0, (const void *)0, (const void *)0, (TAP_CMP_CAST)0, (TAP_TO_STR_CAST)0, \
                                 __func__, __FILE__, __LINE__, __VA_ARGS__))

/**
 * pass - Note that a test passed
 * @...: the printf-style name of the test.
 *
 * For complicated code paths, it can be easiest to simply call pass() in one
 * branch and fail() in another.
 *
 * Example:
 *      x = do_something();
 *      if (!checkable(x) || check_value(x))
 *              pass("do_something() returned a valid value");
 *      else
 *              fail("do_something() returned an invalid value");
 */
# define pass(...) ok(1, __VA_ARGS__)

/**
 * fail - Note that a test failed
 * @...: the printf-style name of the test.
 *
 * For complicated code paths, it can be easiest to simply call pass() in one
 * branch and fail() in another.
 */
# define fail(...) ok(0, __VA_ARGS__)

/* I don't find these to be useful. */
# define skip_if(cond, n, ...)                          \
        if (cond) skip((n), __VA_ARGS__);               \
        else

# define skip_start(test, n, ...)                       \
        do {                                            \
                if((test)) {                            \
                        skip(n,  __VA_ARGS__);          \
                        continue;                       \
                }

# define skip_end } while(0)

#ifndef PRINTF_ATTRIBUTE
#ifdef __GNUC__
#define PRINTF_ATTRIBUTE(a1, a2) __attribute__ ((format (__printf__, a1, a2)))
#else
#define PRINTF_ATTRIBUTE(a1, a2)
#endif
#endif

unsigned int _gen_result(int, const void *, const void *,
            TAP_CMP_CAST, TAP_TO_STR_CAST, const char *,
            const char *, unsigned int, const char *, ...) PRINTF_ATTRIBUTE(9, 10);

/**
 * diag - print a diagnostic message (use instead of printf/fprintf)
 * @fmt: the format of the printf-style message
 *
 * diag ensures that the output will not be considered to be a test
 * result by the TAP test harness.  It will append '\n' for you.
 *
 * Example:
 *      diag("Now running complex tests");
 */
int diag(const char *fmt, ...) PRINTF_ATTRIBUTE(1, 2);

/**
 * skip - print a diagnostic message (use instead of printf/fprintf)
 * @n: number of tests you're skipping.
 * @fmt: the format of the reason you're skipping the tests.
 *
 * Sometimes tests cannot be run because the test system lacks some feature:
 * you should explicitly document that you're skipping tests using skip().
 *
 * From the Test::More documentation:
 *   If it's something the user might not be able to do, use SKIP.  This
 *   includes optional modules that aren't installed, running under an OS that
 *   doesn't have some feature (like fork() or symlinks), or maybe you need an
 *   Internet connection and one isn't available.
 *
 * Example:
 *      #ifdef HAVE_SOME_FEATURE
 *      ok1(test_some_feature());
 *      #else
 *      skip(1, "Don't have SOME_FEATURE");
 *      #endif
 */
void skip(unsigned int n, const char *fmt, ...) PRINTF_ATTRIBUTE(2, 3);

/**
 * todo_start - mark tests that you expect to fail.
 * @fmt: the reason they currently fail.
 *
 * It's extremely useful to write tests before you implement the matching fix
 * or features: surround these tests by todo_start()/todo_end().  These tests
 * will still be run, but with additional output that indicates that they are
 * expected to fail.
 *
 * This way, should a test start to succeed unexpectedly, tools like prove(1)
 * will indicate this and you can move the test out of the todo block.  This
 * is much more useful than simply commenting out (or '#if 0') the tests.
 *
 * From the Test::More documentation:
 *   If it's something the programmer hasn't done yet, use TODO.  This is for
 *   any code you haven't written yet, or bugs you have yet to fix, but want to
 *   put tests in your testing script (always a good idea).
 *
 * Example:
 *      todo_start("dwim() not returning true yet");
 *      ok(dwim(), "Did what the user wanted");
 *      todo_end();
 */
void todo_start(const char *fmt, ...) PRINTF_ATTRIBUTE(1, 2);

/**
 * todo_end - end of tests you expect to fail.
 *
 * See todo_start().
 */
void todo_end(void);

/**
 * exit_status - the value that main should return.
 *
 * For maximum compatability your test program should return a particular exit
 * code (ie. 0 if all tests were run, and every test which was expected to
 * succeed succeeded).
 *
 * Example:
 *      exit(exit_status());
 */
int exit_status(void);

/**
 * plan_no_plan - I have no idea how many tests I'm going to run.
 *
 * In some situations you may not know how many tests you will be running, or
 * you are developing your test program, and do not want to update the
 * plan_tests() call every time you make a change.  For those situations use
 * plan_no_plan() instead of plan_tests().  It indicates to the test harness
 * that an indeterminate number of tests will be run.
 *
 * Remember, if you fail to plan, you plan to fail.
 *
 * Example:
 *      plan_no_plan();
 *      while (random() % 2)
 *              ok1(some_test());
 *      exit(exit_status());
 */
void plan_no_plan(void);

/**
 * plan_skip_all - Indicate that you will skip all tests.
 * @reason: the string indicating why you can't run any tests.
 *
 * If your test program detects at run time that some required functionality
 * is missing (for example, it relies on a database connection which is not
 * present, or a particular configuration option that has not been included
 * in the running kernel) use plan_skip_all() instead of plan_tests().
 *
 * Example:
 *      if (!have_some_feature) {
 *              plan_skip_all("Need some_feature support");
 *              exit(exit_status());
 *      }
 *      plan_tests(13);
 */
void plan_skip_all(const char *reason);

/* New in libtap2 - See the man page for help */

#define TAP_FLAG_ON_FAILURE_EXIT 0x00000001
#define TAP_FLAG_DEBUG           0x00000002

void         tap_init(                FILE * out);
void         tap_plan(                unsigned tests, unsigned flags, FILE * output);
const char * tap_get_test_case_name(  void);
void         tap_set_test_case_name(  const char * name);
void *       tap_dup(                 const void * mem, size_t size);

#define tap_test_case_name(name) tap_set_test_case_name(name)

#endif /* C99 or gcc */
#endif /* __TAP_H__  */

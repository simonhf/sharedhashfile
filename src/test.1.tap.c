/* We use the fact that pipes have a buffer greater than the size of
 * any output, and change stdout and stderr to use that.
 *
 * Since we don't use libtap for output, this looks like one big test.
 */
#include "tap.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <limits.h>

/* Provide the things Windows doesn't have.
 */
#ifdef WINDOWS_NT
#include <io.h>
#include <BaseTsd.h>
#define false          0
#define true           1
#define PIPE_BUF       4096
#ifdef MAKE_MINGW
#else
#define STDERR_FILENO  fileno(stderr)
#define STDOUT_FILENO  fileno(stdout)
#endif
#define err(eval, ...) {fprintf(stderr, "test.1.tap.t: " __VA_ARGS__); fprintf(stderr, ": %s", strerror(errno)); exit(eval);}
#define pipe(phandles) _pipe((phandles), PIPE_BUF, _O_BINARY)
#define snprintf       _snprintf
typedef SSIZE_T        ssize_t;

/* Under UNIX, just get them from the OS headers.
 */
#else
#include <err.h>
#include <fnmatch.h>
#include <stdbool.h>
#include <unistd.h>
#endif

static int stderrfd;    /* We dup stderr to here. */

/* Emulate Unix functions under Windows.
 */
#ifdef WINDOWS_NT

static int
fnmatch(const char * pattern, const char * buffer, int unused)
{
    char * star;
    int    len;

    (void)unused;

    /* If there is a leading subpattern.
     */
    if ((star = strchr(pattern, '*')) != NULL) {
        len = star - pattern;
        if (strncmp(pattern, buffer, len) != 0) {
            return 1;
        }

        pattern += len + 1;
        buffer  += len;
    }

    /* If the star matches anything, skip it.
     */
    if ((len = strlen(buffer) - strlen(pattern)) > 0) {
        buffer += len;
    }

    return strcmp(buffer, pattern);
}

#endif

/* write_all inlined here to avoid circular dependency. */
static void write_all(int fd, const void *data, size_t size)
{
    while (size) {
        ssize_t done;

        done = write(fd, data, size);
        if (done <= 0)
            _exit(1);
        data  = (const char *)data + done;
        size -= done;
    }
}

/* Simple replacement for err() */
static void failmsg(const char *fmt, ...)
{
    char buf[1024];
    va_list ap;

    /* Write into buffer. */
    va_start(ap, fmt);
    vsprintf(buf, fmt, ap);
    va_end(ap);

    write_all(stderrfd, "# ", 2);
    write_all(stderrfd, buf, strlen(buf));
    write_all(stderrfd, "\n", 1);
    _exit(1);
}

static void expect(int fd, const char *pattern)
{
    char buffer[PIPE_BUF+1];
    int r;

    r = read(fd, buffer, sizeof(buffer)-1);
    if (r < 0)
        failmsg("reading from pipe");
    buffer[r] = '\0';

    if (fnmatch(pattern, buffer, 0) != 0)
        failmsg("Expected '%s' got '%s'", pattern, buffer);
}

static int
one_int(void)
{
     return 1;
}

static const char *
one_str(void)
{
    return "one";
}

struct obj
{
    int id;
};

static void *
one_obj(void)
{
    static struct obj one;

    one.id = 1;
    return &one;
}

static int
obj_cmp(const void * obj1, const void * obj2)
{
    return(((const struct obj *)obj2)->id - ((const struct obj *)obj1)->id);
}

static const char *
obj_to_str(const void * obj)
{
    static char buf[16];

    snprintf(buf, sizeof(buf), "{id=%d}", ((const struct obj *)obj)->id);
    return buf;
}

int main(int argc, char *argv[])
{
    int        p[2];
    int        stdoutfd;
        struct obj exp;

        (void)argc;
        (void)argv;

    printf("1..1\n");
    fflush(stdout);
    stderrfd = dup(STDERR_FILENO);

    if (stderrfd < 0)
        err(1, "dup of stderr failed");

    stdoutfd = dup(STDOUT_FILENO);

    if (stdoutfd < 0)
        err(1, "dup of stdout failed");

    if (pipe(p) != 0)
        failmsg("pipe failed");

    if (dup2(p[1], STDERR_FILENO) < 0 || dup2(p[1], STDOUT_FILENO) < 0)
        failmsg("Duplicating file descriptor");

    plan_tests(10);
    expect(p[0], "1..10\n");

    ok(1, "msg1");
    expect(p[0], "ok 1 - msg1\n");

    ok(0, "msg2");
    expect(p[0], "not ok 2 - msg2\n"
           "#     Failed test (src/test.1.tap.c:main() at line 199)\n");

    ok1(true);
    expect(p[0], "ok 3 - true\n");

    ok1(false);
    expect(p[0], "not ok 4 - false\n"
           "#     Failed test (src/test.1.tap.c:main() at line 206)\n");

    pass("passed");
    expect(p[0], "ok 5 - passed\n");

    fail("failed");
    expect(p[0], "not ok 6 - failed\n"
           "#     Failed test (src/test.1.tap.c:main() at line 213)\n");

    skip(2, "skipping %s", "test");
    expect(p[0], "ok 7 # skip skipping test\n"
           "ok 8 # skip skipping test\n");

    todo_start("todo");
    ok1(false);
    expect(p[0], "not ok 9 - false # TODO todo\n"
                 "#     Failed (TODO) test (src/test.1.tap.c:main() at line 222)\n");
    ok1(true);
    expect(p[0], "ok 10 - true # TODO todo\n");
    todo_end();

    if (exit_status() != 3)
        failmsg("Expected exit status 3, not %i", exit_status());

        is(one_int(), 1, "one_int() returns 1");
        expect(p[0], "ok 11 - one_int() returns 1\n");
        is(one_int(), 2, "one_int() returns 2");
        expect(p[0], "not ok 12 - one_int() returns 2\n"
               "#     Failed test (src/test.1.tap.c:main() at line 234)\n"
               "#          got: 1\n"
               "#     expected: 2\n");

        is_eq(one_str(), "one", "one_str() returns 'one'");
        expect(p[0], "ok 13 - one_str() returns 'one'\n");
        is_eq(one_str(), "two", "one_str() returns 'two'");
        expect(p[0], "not ok 14 - one_str() returns 'two'\n"
               "#     Failed test (src/test.1.tap.c:main() at line 242)\n"
               "#          got: \"one\"\n"
               "#     expected: \"two\"\n");

        exp.id = 1;
        is_cmp(one_obj(), &exp, obj_cmp, obj_to_str, "one_obj() has id 1");
        expect(p[0], "ok 15 - one_obj() has id 1\n");
        exp.id = 2;
        is_cmp(one_obj(), &exp, obj_cmp, obj_to_str, "one_obj() has id 2");
        expect(p[0], "not ok 16 - one_obj() has id 2\n"
               "#     Failed test (src/test.1.tap.c:main() at line 252)\n"
               "#          got: {id=1}\n"
               "#     expected: {id=2}\n");

        is_strstr(one_str(), "n", "one_str() contains 'n'");
        expect(p[0], "ok 17 - one_str() contains 'n'\n");
        is_strstr(one_str(), "w", "one_str() contains 'w'");
        expect(p[0], "not ok 18 - one_str() contains 'w'\n"
               "#     Failed test (src/test.1.tap.c:main() at line 260)\n"
               "#                     got: \"one\"\n"
               "#     expected to contain: \"w\"\n");
#if 0
    /* Manually run the atexit command. */
    _cleanup();
    expect(p[0], "# Looks like you failed 2 tests of 9.\n");
#endif

    write_all(stdoutfd, "ok 1 - test still alive\n", strlen("ok 1 - test still alive\n"));
    _exit(0);
}

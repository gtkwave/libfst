#include <fstapi.h>
#include <glib.h>

#ifndef _WIN32
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <sys/resource.h>
#include <sys/stat.h>
#endif

#ifndef _WIN32
// Writes `steps` time steps of a 64-bit variable to `name`, flushing each block,
// on a thread of the writer's own where `parallel`, and answers what the writer
// said of its writes before it was closed.
static int write_steps(const char *name, int steps, int parallel, int *close_error)
{
    fstWriterContext *writer = fstWriterCreate(name, 1);
    g_assert_true(writer != NULL);
    fstWriterSetParallelMode(writer, parallel);

    fstHandle var = fstWriterCreateVar(writer, FST_VT_VCD_REG, FST_VD_IMPLICIT, 64, "var", 0);
    char value[65];
    for (int step = 0; step < steps; step++) {
        for (int bit = 0; bit < 64; bit++) {
            value[bit] = ((step >> (bit % 16)) & 1) ? '1' : '0';
        }
        value[64] = 0;
        fstWriterEmitTimeChange(writer, step);
        fstWriterEmitValueChange(writer, var, value);
        if (step % 1000 == 999) {
            fstWriterFlushContext(writer);
        }
    }

    int write_error = fstWriterGetWriteError(writer);
    *close_error = fstWriterClose2(writer);
    return write_error;
}
#endif

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

#ifdef _WIN32
    // No limit on a file's size to run into.
    return 77;
#else
    int close_error = -1;

    // A file the system takes whole reports nothing.
    g_assert_cmpint(write_steps("write_error_whole.fst", 2000, 0, &close_error), ==, 0);
    g_assert_cmpint(close_error, ==, 0);
    g_assert_cmpint(write_steps("write_error_small_whole.fst", 10, 0, &close_error), ==, 0);
    g_assert_cmpint(close_error, ==, 0);
    struct stat small_whole;
    g_assert_cmpint(stat("write_error_small_whole.fst", &small_whole), ==, 0);

    // Past a limit on file size, a write fails with EFBIG rather than ending the
    // process, as SIGXFSZ is ignored.
    signal(SIGXFSZ, SIG_IGN);
    struct rlimit limit = {.rlim_cur = 16384, .rlim_max = RLIM_INFINITY};
    g_assert_cmpint(setrlimit(RLIMIT_FSIZE, &limit), ==, 0);

    // A file too large for the limit: the failure is seen while the file is
    // being written, and still at its close, whichever thread wrote the block.
    g_assert_cmpint(write_steps("write_error_large.fst", 20000, 0, &close_error), ==, EFBIG);
    g_assert_cmpint(close_error, ==, EFBIG);
    g_assert_cmpint(write_steps("write_error_parallel.fst", 20000, 1, &close_error), ==, EFBIG);
    g_assert_cmpint(close_error, ==, EFBIG);

    // A failure met only in the writes that end the file is seen at its close:
    // a file one byte short of what the same steps take whole has room for
    // everything but its last write.
    limit.rlim_cur = small_whole.st_size - 1;
    g_assert_cmpint(setrlimit(RLIMIT_FSIZE, &limit), ==, 0);
    g_assert_cmpint(write_steps("write_error_small.fst", 10, 0, &close_error), ==, 0);
    g_assert_cmpint(close_error, ==, EFBIG);

    remove("write_error_whole.fst");
    remove("write_error_small_whole.fst");
    remove("write_error_large.fst");
    remove("write_error_parallel.fst");
    remove("write_error_small.fst");
    return 0;
#endif
}

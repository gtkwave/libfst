#include <fstapi.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

// How a trace is written.
typedef struct
{
    int parallel; // fstWriterSetParallelMode()
    int flush_every; // fstWriterFlushContext() every that many time steps, 0 for never
} WriterSetup;

#define MAX_VARS 64
#define MAX_WIDTH 128

static uint32_t widths[MAX_VARS + 1]; // by handle

static uint64_t rnd_state;
static uint64_t rnd(void)
{
    rnd_state ^= rnd_state << 13;
    rnd_state ^= rnd_state >> 7;
    rnd_state ^= rnd_state << 17;
    return rnd_state;
}

// A value change as one string, "time handle value", whose order is the order
// of time, then handle.
static char *change_text(uint64_t time, fstHandle handle, const char *value, uint32_t width)
{
    return g_strdup_printf("%020" PRIu64 " %06u %.*s", time, handle, (int)width, value);
}

static int compare_text(gconstpointer a, gconstpointer b)
{
    return strcmp(*(char *const *)a, *(char *const *)b);
}

// Writes `steps` time steps of pseudo-random changes of `vars` variables of 1
// to 128 bits, some of them aliases, to `name`, and keeps every change written
// in `written`. The last time step has no change, as the last time step of a
// simulation often has none.
static void write_trace(const char *name,
                        const WriterSetup *setup,
                        int steps,
                        int vars,
                        GPtrArray *written)
{
    static const uint32_t var_widths[8] = {1, 1, 3, 8, 32, 64, 100, 128};
    fstHandle handles[MAX_VARS];
    char values[MAX_VARS][MAX_WIDTH];
    char value[MAX_WIDTH];

    g_assert_cmpint(vars, <=, MAX_VARS);
    rnd_state = 88172645463325252ULL;

    fstWriterContext *writer = fstWriterCreate(name, 1);
    g_assert_true(writer != NULL);
    fstWriterSetPackType(writer, FST_WR_PT_LZ4);
    // a fixed date, so that two files can be compared byte for byte
    fstWriterSetDate(writer, "write_parallel");
    fstWriterSetParallelMode(writer, setup->parallel);

    fstWriterSetScope(writer, FST_ST_VCD_MODULE, "top", NULL);
    for (int i = 0; i < vars; i++) {
        char var_name[16];
        int alias = i >= 8 && i % 8 == 7; // of the variable eight before it
        snprintf(var_name, sizeof(var_name), "v%d", i);
        handles[i] = fstWriterCreateVar(writer,
                                        FST_VT_VCD_REG,
                                        FST_VD_IMPLICIT,
                                        var_widths[i % 8],
                                        var_name,
                                        alias ? handles[i - 8] : 0);
        widths[handles[i]] = var_widths[i % 8];
    }
    fstWriterSetUpscope(writer);

    for (int step = 0; step < steps; step++) {
        uint64_t time = (uint64_t)step * 10;
        fstWriterEmitTimeChange(writer, time);
        for (int i = 0; i < vars; i++) {
            uint32_t width = widths[handles[i]];
            if (i >= 8 && i % 8 == 7) {
                continue; // an alias changes with the variable it aliases
            }
            if (step > 0 && rnd() % 4 != 0) {
                continue; // every value at time 0, then about one change in four steps
            }
            for (uint32_t bit = 0; bit < width; bit++) {
                uint64_t r = rnd() % 16;
                value[bit] = r < 7 ? '0' : r < 14 ? '1' : r < 15 ? 'x' : 'z';
            }
            if (step > 0 && memcmp(value, values[i], width) == 0) {
                continue;
            }
            memcpy(values[i], value, width);
            fstWriterEmitValueChange(writer, handles[i], value);
            g_ptr_array_add(written, change_text(time, handles[i], value, width));
        }
        if (setup->flush_every && step % setup->flush_every == setup->flush_every - 1) {
            fstWriterFlushContext(writer);
        }
    }
    fstWriterEmitTimeChange(writer, (uint64_t)steps * 10);
    g_assert_cmpint(fstWriterClose2(writer), ==, 0);

    g_ptr_array_sort(written, compare_text);
}

static void read_change(void *user_data,
                        uint64_t time,
                        fstHandle handle,
                        const unsigned char *value)
{
    GPtrArray *read = (GPtrArray *)user_data;
    g_ptr_array_add(read, change_text(time, handle, (const char *)value, widths[handle]));
}

// Reads every value change of `name`, in the order of time, then handle.
static GPtrArray *read_trace(const char *name)
{
    GPtrArray *read = g_ptr_array_new_with_free_func(g_free);

    fstReaderContext *reader = fstReaderOpen(name);
    g_assert_true(reader != NULL);
    fstReaderSetFacProcessMaskAll(reader);
    g_assert_true(fstReaderIterBlocks(reader, read_change, read, NULL));
    fstReaderClose(reader);

    g_ptr_array_sort(read, compare_text);
    return read;
}

static void assert_changes_equal(GPtrArray *written, GPtrArray *read)
{
    g_assert_cmpuint(read->len, ==, written->len);
    for (guint i = 0; i < written->len; i++) {
        g_assert_cmpstr(g_ptr_array_index(read, i), ==, g_ptr_array_index(written, i));
    }
}

static void assert_files_equal(const char *name_a, const char *name_b)
{
    gchar *a, *b;
    gsize len_a, len_b;
    g_assert_true(g_file_get_contents(name_a, &a, &len_a, NULL));
    g_assert_true(g_file_get_contents(name_b, &b, &len_b, NULL));
    g_assert_cmpuint(len_a, ==, len_b);
    g_assert_true(memcmp(a, b, len_a) == 0);
    g_free(a);
    g_free(b);
}

// Writes the trace with `setup` and checks that it reads back as written and
// that the file is the one the serial writer writes for the same trace.
static void check_against_serial(const WriterSetup *setup, int steps, int vars)
{
    WriterSetup serial = *setup;
    serial.parallel = 0;

    GPtrArray *written_serial = g_ptr_array_new_with_free_func(g_free);
    write_trace("write_parallel_serial.fst", &serial, steps, vars, written_serial);

    GPtrArray *written = g_ptr_array_new_with_free_func(g_free);
    write_trace("write_parallel.fst", setup, steps, vars, written);
    GPtrArray *read = read_trace("write_parallel.fst");
    assert_changes_equal(written, read);
    assert_files_equal("write_parallel_serial.fst", "write_parallel.fst");

    g_ptr_array_unref(read);
    g_ptr_array_unref(written);
    g_ptr_array_unref(written_serial);
    g_remove("write_parallel_serial.fst");
    g_remove("write_parallel.fst");
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    // A block flushed every 5 time steps: each flush thread starts while the
    // previous one may still be writing, and the file ends with a time step
    // without changes after the last flush.
    WriterSetup parallel = {.parallel = 1, .flush_every = 5};
    check_against_serial(&parallel, 400, 64);

    // Without explicit flushes, the close flushes the only block.
    parallel.flush_every = 0;
    check_against_serial(&parallel, 400, 64);

    return 0;
}

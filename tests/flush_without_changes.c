#include <fstapi.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

// A flush asked for (fstWriterFlushContext()) when nothing changed since the
// last flush: the time steps of that block must still be read back at their
// times.

static uint64_t rnd_state = 88172645463325252ULL;
static uint64_t rnd(void)
{
    rnd_state ^= rnd_state << 13;
    rnd_state ^= rnd_state >> 7;
    rnd_state ^= rnd_state << 17;
    return rnd_state;
}

// A value change as one string, "time handle value", whose order is the order
// of time, then handle.
static char *change_text(uint64_t time, fstHandle handle, char value)
{
    return g_strdup_printf("%020" PRIu64 " %06u %c", time, handle, value);
}

static int compare_text(gconstpointer a, gconstpointer b)
{
    return strcmp(*(char *const *)a, *(char *const *)b);
}

static void read_change(void *user_data,
                        uint64_t time,
                        fstHandle handle,
                        const unsigned char *value)
{
    g_ptr_array_add((GPtrArray *)user_data, change_text(time, handle, (char)value[0]));
}

// Writes `steps` time steps of `vars` 1-bit variables, each changing in about
// one step in four, with a flush asked for every `flush_every` steps, and
// checks that every change reads back at its time.
static void check(int vars, int flush_every, int steps)
{
    const char *name = "flush_without_changes.fst";
    GPtrArray *written = g_ptr_array_new_with_free_func(g_free);
    GPtrArray *read = g_ptr_array_new_with_free_func(g_free);
    fstHandle handles[4];
    char values[4];

    g_assert_cmpint(vars, <=, 4);
    fstWriterContext *writer = fstWriterCreate(name, 1);
    g_assert_true(writer != NULL);
    fstWriterSetScope(writer, FST_ST_VCD_MODULE, "top", NULL);
    for (int i = 0; i < vars; i++) {
        char var_name[8];
        snprintf(var_name, sizeof(var_name), "v%d", i);
        handles[i] = fstWriterCreateVar(writer, FST_VT_VCD_REG, FST_VD_IMPLICIT, 1, var_name, 0);
    }
    fstWriterSetUpscope(writer);

    for (int step = 0; step < steps; step++) {
        uint64_t time = (uint64_t)step * 10;
        fstWriterEmitTimeChange(writer, time);
        for (int i = 0; i < vars; i++) {
            char value = (rnd() & 1) ? '1' : '0';
            if (step > 0 && (rnd() % 4 != 0 || value == values[i])) {
                continue;
            }
            values[i] = value;
            fstWriterEmitValueChange(writer, handles[i], &values[i]);
            g_ptr_array_add(written, change_text(time, handles[i], value));
        }
        if (step % flush_every == flush_every - 1) {
            fstWriterFlushContext(writer);
        }
    }
    fstWriterEmitTimeChange(writer, (uint64_t)steps * 10);
    g_assert_cmpint(fstWriterClose2(writer), ==, 0);

    fstReaderContext *reader = fstReaderOpen(name);
    g_assert_true(reader != NULL);
    g_assert_cmpuint(fstReaderGetEndTime(reader), ==, (uint64_t)steps * 10);
    fstReaderSetFacProcessMaskAll(reader);
    g_assert_true(fstReaderIterBlocks(reader, read_change, read, NULL));
    fstReaderClose(reader);

    g_ptr_array_sort(written, compare_text);
    g_ptr_array_sort(read, compare_text);
    g_assert_cmpuint(read->len, ==, written->len);
    for (guint i = 0; i < written->len; i++) {
        g_assert_cmpstr(g_ptr_array_index(read, i), ==, g_ptr_array_index(written, i));
    }

    g_ptr_array_unref(read);
    g_ptr_array_unref(written);
    g_remove(name);
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    // One variable, a flush every time step: most blocks have no change.
    check(1, 1, 400);
    // Two variables, a flush every second time step.
    check(2, 2, 400);
    // Four variables, a flush every third time step: few blocks have no change.
    check(4, 3, 400);

    return 0;
}

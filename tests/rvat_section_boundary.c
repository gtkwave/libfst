#include <fstapi.h>
#include <glib.h>

// Random access reads at the end time of a value change section: one where
// the next section begins at that same time, and one at the end of the last
// value change section, in a file that ends later.

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    fstWriterContext *writer = fstWriterCreate("rvat_section_boundary.fst", 1);
    g_assert_true(writer != NULL);

    fstHandle var = fstWriterCreateVar(writer, FST_VT_VCD_WIRE, FST_VD_IMPLICIT, 1, "var", 0);

    fstWriterEmitTimeChange(writer, 0);
    fstWriterEmitValueChange(writer, var, "0");
    fstWriterEmitTimeChange(writer, 1);
    fstWriterEmitValueChange(writer, var, "1");
    fstWriterEmitTimeChange(writer, 2);
    fstWriterEmitValueChange(writer, var, "0");

    // The section is flushed at the next time change: it ends at 2, and the
    // next one begins at 2.
    fstWriterFlushContext(writer);

    fstWriterEmitTimeChange(writer, 3);
    fstWriterEmitValueChange(writer, var, "1");
    fstWriterEmitTimeChange(writer, 4);
    fstWriterEmitValueChange(writer, var, "0");

    // Same again, ending the second section at 4; the third one holds no value
    // changes, so it is not written, but the file still ends at 5.
    fstWriterFlushContext(writer);

    fstWriterEmitTimeChange(writer, 5);

    fstWriterClose(writer);

    fstReaderContext *reader = fstReaderOpen("rvat_section_boundary.fst");
    g_assert_true(reader != NULL);
    g_assert_cmpint(fstReaderGetEndTime(reader), ==, 5);

    char buf[64];

    g_assert_cmpstr(fstReaderGetValueFromHandleAtTime(reader, 2, var, buf), ==, "0");
    g_assert_cmpstr(fstReaderGetValueFromHandleAtTime(reader, 4, var, buf), ==, "0");

    fstReaderClose(reader);

    return 0;
}

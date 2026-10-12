#include <fstapi.h>
#include <glib.h>
#include <string.h>

// The chain table names, for each handle, where its value change data starts in
// the section.  It is parsed once per section into arrays that outlive a single
// section, so the state the parse carries from one entry to the next has to
// start over with every section.
//
// It does not: the index of the previous entry survives, so the first chain of
// a section computes its predecessor's length against a position left over from
// the section before, and writes it at that handle's index.  The result can be
// negative, which is how the format spells "this handle's chain is the same as
// another handle's", and the handle it names is then read with the wrong
// decoder.  A variable length handle reading a fixed length handle's chain
// takes the first bytes of it for a length and runs off the end of the buffer.

static void value_change_callback(void *user_data,
                                  uint64_t time,
                                  fstHandle handle,
                                  const unsigned char *value)
{
    int *seen = user_data;

    (void)time;

    g_assert_cmpint(handle, >=, 1);
    g_assert_cmpint(handle, <=, 6);
    g_assert_true(value != NULL);

    seen[handle - 1]++;
}

static void value_change_callback_varlen(void *user_data,
                                         uint64_t time,
                                         fstHandle handle,
                                         const unsigned char *value,
                                         uint32_t len)
{
    int *seen = user_data;

    (void)time;
    (void)value;

    // The only variable length handle is the last one, and its single change
    // carries no payload.

    g_assert_cmpint(handle, ==, 6);
    g_assert_cmpint(len, ==, 0);

    seen[handle - 1]++;
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    uint32_t words[1] = {0x12345};
    char wide[121];
    int seen[6] = {0, 0, 0, 0, 0, 0};

    fstWriterContext *writer = fstWriterCreate("chain_sections.fst", 0);
    g_assert_true(writer != NULL);

    fstWriterCreateVar(writer, FST_VT_VCD_REAL, FST_VD_OUTPUT, 8, "unused_a", 0);
    fstWriterCreateVar(writer, FST_VT_VCD_WIRE, FST_VD_IMPLICIT, 17, "unused_b", 0);
    fstHandle mid = fstWriterCreateVar(writer, FST_VT_VCD_WIRE, FST_VD_IMPLICIT, 17, "mid", 0);
    fstWriterCreateVar(writer, FST_VT_VCD_WIRE, FST_VD_IMPLICIT, 17, "unused_c", 0);
    fstHandle wide_h = fstWriterCreateVar(writer, FST_VT_VCD_WIRE, FST_VD_IMPLICIT, 120, "wide", 0);
    fstHandle text = fstWriterCreateVar(writer, FST_VT_GEN_STRING, FST_VD_INOUT, 0, "text", 0);

    // The first section carries chains for the third and the last handle, so
    // its chain table parse ends on the last one.

    fstWriterEmitTimeChange(writer, 0);
    fstWriterEmitVariableLengthValueChange(writer, text, "", 0);
    fstWriterEmitTimeChange(writer, 0);
    fstWriterEmitTimeChange(writer, 0);
    fstWriterEmitValueChangeVec32(writer, mid, 17, words);

    fstWriterFlushContext(writer);
    fstWriterEmitTimeChange(writer, 0);

    // The second carries a chain for the wide handle only, and it is not the
    // first entry of that section's chain table.  The last handle has no chain
    // here and must not be given the wide handle's.

    memset(wide, '0', 120);
    wide[0] = 'z';
    wide[120] = 0;
    fstWriterEmitValueChange(writer, wide_h, wide);

    fstWriterClose(writer);

    fstReaderContext *reader = fstReaderOpen("chain_sections.fst");
    g_assert_true(reader != NULL);

    fstReaderSetFacProcessMaskAll(reader);
    g_assert_cmpint(fstReaderIterBlocks2(reader,
                                         value_change_callback,
                                         value_change_callback_varlen,
                                         seen,
                                         NULL),
                    !=,
                    0);

    // One change each for the three handles that have one, and none for the
    // rest.

    g_assert_cmpint(seen[0], ==, 0);
    g_assert_cmpint(seen[1], ==, 0);
    g_assert_cmpint(seen[2], ==, 1);
    g_assert_cmpint(seen[3], ==, 0);
    g_assert_cmpint(seen[4], ==, 1);
    g_assert_cmpint(seen[5], ==, 1);

    fstReaderClose(reader);

    return 0;
}

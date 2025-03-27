#include <fstapi.h>
#include <glib.h>

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    // Write simple FST file

    fstWriterContext *writer = fstWriterCreate("out.fst", 1);
    g_assert_true(writer != NULL);

    fstHandle var = fstWriterCreateVar(writer, FST_VT_VCD_WIRE, FST_VD_IMPLICIT, 1, "var", 0);

    fstWriterEmitTimeChange(writer, 0);
    fstWriterEmitValueChange(writer, var, "0");
    fstWriterEmitTimeChange(writer, 1);
    fstWriterEmitValueChange(writer, var, "1");
    fstWriterEmitTimeChange(writer, 2);

    fstWriterClose(writer);

    // Read generated FST file

    fstReaderContext *reader = fstReaderOpen("out.fst");
    g_assert_true(reader != NULL);

    g_assert_cmpint(fstReaderGetStartTime(reader), ==, 0);
    g_assert_cmpint(fstReaderGetEndTime(reader), ==, 2);
    g_assert_cmpint(fstReaderGetVarCount(reader), ==, 1);
    g_assert_cmpint(fstReaderGetScopeCount(reader), ==, 0);
    g_assert_cmpint(fstReaderGetAliasCount(reader), ==, 0);

    struct fstHier *iter = fstReaderIterateHier(reader);
    g_assert_true(iter != NULL);
    g_assert_cmpint(iter->htyp, ==, FST_HT_VAR);
    g_assert_cmpstr(iter->u.var.name, ==, "var");
    g_assert_cmpint(iter->u.var.length, ==, 1);

    iter = fstReaderIterateHier(reader);
    g_assert_true(iter == NULL);

    fstReaderClose(reader);

    return 0;
}
#include <fstapi.h>
#include <glib.h>

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    // Write empty FST file

    fstWriterContext *writer = fstWriterCreate("empty1.fst", 1);
    g_assert_true(writer != NULL);

    fstWriterClose(writer);

    // Should be able to open the generated FST file

    fstReaderContext *reader = fstReaderOpen("empty1.fst");
    g_assert_true(reader != NULL);

    g_assert_cmpint(fstReaderGetStartTime(reader), ==, 0);
    g_assert_cmpint(fstReaderGetEndTime(reader), ==, 0);
    g_assert_cmpint(fstReaderGetVarCount(reader), ==, 0);
    g_assert_cmpint(fstReaderGetScopeCount(reader), ==, 0);
    g_assert_cmpint(fstReaderGetAliasCount(reader), ==, 0);

    struct fstHier *iter = fstReaderIterateHier(reader);
    g_assert_true(iter == NULL);

    fstReaderClose(reader);

    // An empty file without FST header should still fail

    FILE *f = fopen("empty2.fst", "w");
    g_assert_true(f != NULL);

    fclose(f);

    reader = fstReaderOpen("empty2.fst");
    g_assert_true(reader == NULL);

    return 0;
}

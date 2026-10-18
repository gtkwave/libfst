#include <fstapi.h>
#include <glib.h>
#include <stdio.h>

// Writes one enum attribute for every fstEnumValueType value, reads them back,
// and processes the hierarchy into VCD output. Checks that each subtype round
// trips through the reader and that VCD generation succeeds for all of them.

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    fstWriterContext *writer = fstWriterCreate("attr_enum.fst", 1);
    g_assert_true(writer != NULL);

    fstWriterSetScope(writer, FST_ST_VCD_MODULE, "top", NULL);

    // One attribute per enum value type, from the first value through FST_EV_MAX.

    for (unsigned int subtype = FST_EV_SV_INTEGER; subtype <= FST_EV_MAX; subtype++) {
        char name[32];
        snprintf(name, sizeof(name), "as_%u", subtype);
        fstWriterSetAttrBegin(writer, FST_AT_ENUM, subtype, name, 0);
        fstWriterSetAttrEnd(writer);
    }

    fstHandle var = fstWriterCreateVar(writer, FST_VT_VCD_WIRE, FST_VD_IMPLICIT, 1, "flag", 0);

    fstWriterSetUpscope(writer);

    fstWriterEmitTimeChange(writer, 0);
    fstWriterEmitValueChange(writer, var, "1");

    fstWriterClose(writer);

    fstReaderContext *reader = fstReaderOpen("attr_enum.fst");
    g_assert_true(reader != NULL);

    // Each subtype comes back from the reader exactly once.

    int seen[FST_EV_MAX + 1] = {0};
    struct fstHier *hier;

    while ((hier = fstReaderIterateHier(reader)) != NULL) {
        if (hier->htyp != FST_HT_ATTRBEGIN || hier->u.attr.typ != FST_AT_ENUM) {
            continue;
        }
        g_assert_cmpuint(hier->u.attr.subtype, <=, FST_EV_MAX);
        seen[hier->u.attr.subtype]++;
    }

    for (unsigned int subtype = FST_EV_SV_INTEGER; subtype <= FST_EV_MAX; subtype++) {
        g_assert_cmpint(seen[subtype], ==, 1);
    }

    // Processing the hierarchy into VCD succeeds for every subtype.

    FILE *sink = tmpfile();
    g_assert_true(sink != NULL);

    fstReaderSetVcdExtensions(reader, 1);
    g_assert_cmpint(fstReaderIterateHierRewind(reader), ==, 1);
    g_assert_cmpint(fstReaderProcessHier(reader, sink), !=, 0);

    fclose(sink);
    fstReaderClose(reader);

    return 0;
}

/*
 * A corpus of malformed FST files, each one a minimized fuzz artifact, run
 * through the reader.  Each case is its own test, so that a file the reader
 * does not survive says which file it was and leaves the rest to run: the
 * executable reads the one file it is given on the command line.
 *
 * Nothing here checks what comes back: a file whose lengths disagree with each
 * other has no right answer, and the reader is free to reject it, to report
 * nothing, or to report whatever it can make of it.  What it may not do is read
 * outside its buffers or walk off a table, so these are regression tests for
 * the sanitizer builds, and a plain build only checks that the reader still
 * terminates.
 *
 * A file whose defect the reader is meant to catch with one of its own security
 * checks does not belong here: those abort the process by design.  Every file
 * in the directory has to be one the reader gets through.
 *
 * Adding a case is adding a file to tests/data/malformed and its name to
 * malformed_cases in tests/meson.build, with a note here saying what is wrong
 * with it.
 *
 *   chain_shorter_than_its_length.fst
 *       a handle's value change chain is shorter than the varint at the front
 *       of it, so the length of the data after that varint came out negative
 *
 *   chain_table_overrun.fst
 *       the chain table of a value change section describes more handles than
 *       the section announced, which walked the table it is parsed into past
 *       its end
 *
 *   chain_table_pair_unterminated.fst
 *       a chain table whose last byte is a continuation byte: the varint it
 *       starts runs off the end of the table, reads as the zero that means
 *       "this entry is two varints", and the second varint is not there
 *
 *   chain_table_svarint_overlong.fst
 *       a chain table holding two signed varints: one nine bytes long, ending
 *       at bit 63 with its sign bit set, and one eleven bytes long, with bit
 *       63 set and bits past the 64th
 *
 *   chain_table_unterminated.fst
 *       the last byte of a chain table asks for another byte, so a parse that
 *       stops on reaching the end of the table exactly runs past it
 *
 *   chain_uncompresses_past_traversal.fst
 *       a value change chain says it uncompresses to more than the memory the
 *       section said reading it needs, so the decompressor wrote past the
 *       buffer -- the start of the write was checked, the length was not
 *
 *   chain_varint_pair_past_traversal.fst
 *       a value change chain for a variable length signal made of nothing but
 *       continuation bytes, including the last byte, so its varint is
 *       unterminated
 *
 *   hierarchy_lz4duo_shorter_than_its_varint.fst
 *       a doubly LZ4 packed hierarchy section whose data is the single byte
 *       0x81, a continuation byte, so the varint in front of its data does not
 *       end inside the section
 *
 *   geometry_short.fst
 *       a geometry section that announces three handles but holds the width
 *       of only one
 *
 *   geometry_varint_unterminated.fst
 *       a geometry section whose last byte is a varint continuation byte
 *
 *   frame_maxhandle_too_big.fst
 *       a section's frame says it holds values for two handles, while the file
 *       has one
 *
 *   frame_real_narrower_than_eight.fst
 *       a real declared two bytes wide, so its value in the frame is two bytes
 *       long
 *
 *   rvat_chain_table_overrun.fst
 *       the same overrun as chain_table_overrun.fst reached through
 *       fstReaderGetValueFromHandleAtTime() rather than fstReaderIterBlocks2().
 *
 *   time_chain_empty.fst
 *       a value change section whose time chain says it holds no times at
 *       all, while the section still has value changes to place on it
 *
 *   time_chain_short.fst
 *       a value change section whose time chain holds three times while the
 *       section says it holds 4000
 *
 *   traversal_buffer_smaller_than_chain.fst
 *   traversal_buffer_smaller_than_chain_fastlz.fst
 *       a valid file whose value change sections declare a traversal size of
 *       zero, so the buffer the chains are decompressed into is the 66 bytes
 *       of fastlz overhead alone.  The check on the offset of each chain
 *       passed, because the chain that overflows starts two bytes in; the
 *       length written there is the length the chain claims for itself, which
 *       nothing compared against the buffer.  One file per decompressor: LZ4
 *       writes eight bytes at a time past the end, fastlz one
 *
 *   traversal_size_wraps.fst
 *       the second of two value change sections declares a traversal size of
 *       2**64 - 67
 *
 *   uncompressed_chain_past_traversal.fst
 *       a value change section declaring it needs 7 bytes of memory to read,
 *       whose twenty handles share one uncompressed chain 19 bytes long and so
 *       need 360
 *
 *   uncompressed_chain_shorter_than_its_varint.fst
 *       an uncompressed value change chain that the chain table gives four
 *       bytes and whose leading length varint is spelled in five, so the data
 *       after that varint is minus one byte long
 *
 *   value_change_past_traversal.fst
 *       a handle's value change data starts past the end of the memory the
 *       section says reading it needs
 */

#include <fstapi.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void value_change_callback(void *user_data,
                                  uint64_t time,
                                  fstHandle handle,
                                  const unsigned char *value)
{
    (void)user_data;
    (void)time;
    (void)handle;

    /* reading the bytes is the point: a length the reader took from the file
       against a buffer it sized from somewhere else only disagree if somebody
       touches them */
    if (value) {
        volatile size_t len = strlen((const char *)value);
        (void)len;
    }
}

static void value_change_callback_varlen(void *user_data,
                                         uint64_t time,
                                         fstHandle handle,
                                         const unsigned char *value,
                                         uint32_t len)
{
    (void)user_data;
    (void)time;
    (void)handle;

    if (value) {
        volatile unsigned char sink = 0;
        for (uint32_t i = 0; i < len; i++) {
            sink = (unsigned char)(sink + value[i]);
        }
        (void)sink;
    }
}

static void copy_file(const char *from, const char *to)
{
    FILE *in = fopen(from, "rb");
    FILE *out;
    char buf[4096];
    size_t got;

    g_assert_true(in != NULL);

    out = fopen(to, "wb");
    g_assert_true(out != NULL);

    while ((got = fread(buf, 1, sizeof(buf), in)) != 0) {
        g_assert_cmpuint(fwrite(buf, 1, got, out), ==, got);
    }

    fclose(out);
    fclose(in);
}

/*
 * The reader unpacks a compressed hierarchy into a file of its own beside the
 * one it is reading, so the copy gets a directory to itself and whatever is
 * left in that directory goes with it.
 */
static void remove_dir(const char *dir)
{
    GDir *d = g_dir_open(dir, 0, NULL);
    const char *name;

    g_assert_true(d != NULL);

    while ((name = g_dir_read_name(d)) != NULL) {
        char *path = g_build_filename(dir, name, NULL);

        g_remove(path);
        g_free(path);
    }

    g_dir_close(d);
    g_assert_cmpint(g_rmdir(dir), ==, 0);
}

static void read_case(const char *src)
{
    char *dir;
    char *name;
    char *path;
    char *hier_src;
    char *hier_path;
    char *sink_path;
    fstReaderContext *reader;
    fstHandle handle;
    FILE *sink;

    /* the reader is handed a copy, so that a case cannot be edited by running it */
    dir = g_dir_make_tmp("libfst-malformed-XXXXXX", NULL);
    g_assert_true(dir != NULL);

    name = g_path_get_basename(src);
    path = g_build_filename(dir, name, NULL);
    hier_src = g_strconcat(src, ".hier", NULL);
    hier_path = g_strconcat(path, ".hier", NULL);
    sink_path = g_build_filename(dir, "sink.vcd", NULL);

    copy_file(src, path);
    if (g_file_test(hier_src, G_FILE_TEST_EXISTS)) {
        copy_file(hier_src, hier_path);
    }

    reader = fstReaderOpen(path);
    if (!reader) {
        goto done; /* rejecting the file is a perfectly good outcome */
    }

    sink = fopen(sink_path, "wb");
    g_assert_true(sink != NULL);

    struct fstHier *hier;
    int records = 0;
    while ((hier = fstReaderIterateHier(reader)) != NULL && ++records < 4096) {
    }

    g_assert_cmpint(fstReaderIterateHierRewind(reader), !=, -1);
    (void)fstReaderProcessHier(reader, sink);

    handle = fstReaderGetMaxHandle(reader);

    fstReaderSetFacProcessMaskAll(reader);
    (void)fstReaderIterBlocks2(reader,
                               value_change_callback,
                               value_change_callback_varlen,
                               NULL,
                               NULL);
    (void)fstReaderIterBlocks(reader, value_change_callback, NULL, sink);

    fstReaderIterBlocksSetNativeDoublesOnCallback(reader, 1);
    (void)fstReaderIterBlocks2(reader,
                               value_change_callback,
                               value_change_callback_varlen,
                               NULL,
                               NULL);
    fstReaderIterBlocksSetNativeDoublesOnCallback(reader, 0);

    {
        uint64_t start = fstReaderGetStartTime(reader);
        uint64_t end = fstReaderGetEndTime(reader);
        uint64_t span = (end >= start) ? (end - start) : 0;
        char buf[1024];

        fstReaderSetLimitTimeRange(reader, start + span / 3, end - span / 4);
        (void)fstReaderIterBlocks2(reader,
                                   value_change_callback,
                                   value_change_callback_varlen,
                                   NULL,
                                   NULL);
        fstReaderSetUnlimitedTimeRange(reader);

        for (fstHandle h = 1; h <= handle && h <= 8; h++) {
            for (unsigned t = 0; t < 8; t++) {
                (void)fstReaderGetValueFromHandleAtTime(reader, start + (span * t) / 7, h, buf);
            }
        }
    }

    fclose(sink);
    fstReaderClose(reader);

done:
    remove_dir(dir);

    g_free(sink_path);
    g_free(hier_path);
    g_free(hier_src);
    g_free(path);
    g_free(name);
    g_free(dir);
}

int main(int argc, char **argv)
{
    g_assert_cmpint(argc, ==, 2);

    read_case(argv[1]);

    return 0;
}

#include <fstapi.h>
#include <glib.h>
#include <stdio.h>
#include <unistd.h>

// A packed bit vector requires (width + 7) / 8 bytes.  For widths of
// 0xfffffff9 or greater, this calculation overflows 32 bits.  This test
// declares such a width for a signal with packed values and verifies that both
// fstReaderIterBlocks2() and fstReaderGetValueFromHandleAtTime() refuse the
// value.
//
// The width is read from the geometry section, so the test starts with a file
// produced by the library and rebuilds its geometry to declare WIDE_WIDTH.

#define WIDE_WIDTH 0xfffffffeu
#define SIG_WIDTH 8

#define FST_NAME "packed_extent_wraps.fst"

static int wide_handle_seen;

static void value_change_callback(void *user_data,
                                  uint64_t time,
                                  fstHandle handle,
                                  const unsigned char *value)
{
    (void)user_data;
    (void)time;
    (void)value;

    if (handle == 1) {
        wide_handle_seen++;
    }
}

static size_t put_varint(unsigned char *dst, uint64_t v)
{
    size_t n = 0;

    for (;;) {
        unsigned char byte = (unsigned char)(v & 0x7f);

        v >>= 7;
        if (v) {
            dst[n++] = byte | 0x80;
        } else {
            dst[n++] = byte;
            break;
        }
    }

    return n;
}

static uint64_t get_u64(const unsigned char *p)
{
    uint64_t v = 0;
    int i;

    for (i = 0; i < 8; i++) {
        v = (v << 8) | p[i];
    }

    return v;
}

static void put_u64(unsigned char *p, uint64_t v)
{
    int i;

    for (i = 7; i >= 0; i--) {
        p[i] = (unsigned char)(v & 0xff);
        v >>= 8;
    }
}

// Write a valid FST, then hand back its bytes.
static unsigned char *write_and_slurp(size_t *len_out)
{
    fstWriterContext *writer;
    fstHandle handle;
    char value[SIG_WIDTH + 1];
    unsigned char *buf;
    FILE *f;
    long len;
    int t;

    writer = fstWriterCreate(FST_NAME, 1); // hierarchy as a section of the file
    g_assert_true(writer != NULL);

    handle = fstWriterCreateVar(writer, FST_VT_VCD_WIRE, FST_VD_IMPLICIT, SIG_WIDTH, "sig", 0);

    // Values of nothing but zeroes and ones, so that the writer packs them and
    // the reader takes the packed path.  Two time steps give the signal a
    // value change chain rather than only an initial value.

    for (t = 0; t < 2; t++) {
        int j;

        fstWriterEmitTimeChange(writer, (uint64_t)t * 10);

        for (j = 0; j < SIG_WIDTH; j++) {
            value[j] = ((j + t) & 1) ? '1' : '0';
        }
        value[SIG_WIDTH] = 0;

        fstWriterEmitValueChange(writer, handle, value);
    }

    fstWriterClose(writer);

    f = fopen(FST_NAME, "rb");
    g_assert_true(f != NULL);
    g_assert_cmpint(fseek(f, 0, SEEK_END), ==, 0);
    len = ftell(f);
    g_assert_cmpint(len, >, 0);
    g_assert_cmpint(fseek(f, 0, SEEK_SET), ==, 0);

    buf = (unsigned char *)g_malloc((size_t)len);
    g_assert_cmpint(fread(buf, 1, (size_t)len, f), ==, (size_t)len);
    fclose(f);

    *len_out = (size_t)len;
    return buf;
}

// Replace the geometry section with an uncompressed one declaring WIDE_WIDTH
// for the only handle.
static void rewrite_geometry(const unsigned char *src, size_t src_len)
{
    unsigned char body[10];
    unsigned char header[25];
    size_t body_len;
    size_t geom_off = 0;
    uint64_t geom_seclen = 0;
    size_t off = 0;
    int found = 0;
    FILE *f;

    while (off + 9 <= src_len) {
        unsigned char tag = src[off];
        uint64_t seclen = get_u64(src + off + 1);

        if (tag == FST_BL_GEOM) {
            geom_off = off;
            geom_seclen = seclen;
            found = 1;
            break;
        }

        if (!seclen || off + 1 + seclen > src_len) {
            break;
        }
        off += 1 + seclen;
    }

    g_assert_cmpint(found, ==, 1);

    body_len = put_varint(body, WIDE_WIDTH);

    // tag, then a length that counts itself, the two fields below and the body.
    // Making the compressed and uncompressed lengths equal is what tells the
    // reader the body is stored as it is.

    header[0] = FST_BL_GEOM;
    put_u64(header + 1, (uint64_t)body_len + 24);
    put_u64(header + 9, (uint64_t)body_len);
    put_u64(header + 17, 1);

    f = fopen(FST_NAME, "wb");
    g_assert_true(f != NULL);

    g_assert_cmpint(fwrite(src, 1, geom_off, f), ==, geom_off);
    g_assert_cmpint(fwrite(header, 1, sizeof(header), f), ==, sizeof(header));
    g_assert_cmpint(fwrite(body, 1, body_len, f), ==, body_len);

    off = geom_off + 1 + (size_t)geom_seclen;
    g_assert_cmpint(fwrite(src + off, 1, src_len - off, f), ==, src_len - off);

    g_assert_cmpint(fclose(f), ==, 0);
}

int main(int argc, char **argv)
{
    unsigned char *src;
    size_t src_len;
    fstReaderContext *reader;

    (void)argc;
    (void)argv;

    src = write_and_slurp(&src_len);
    rewrite_geometry(src, src_len);
    g_free(src);

    reader = fstReaderOpen(FST_NAME);
    g_assert_true(reader != NULL);

    fstReaderSetFacProcessMaskAll(reader);
    (void)fstReaderIterBlocks2(reader, value_change_callback, NULL, NULL, NULL);

    g_assert_cmpint(wide_handle_seen, ==, 0);

    // The random-access path decodes the same value on its own, so it is
    // covered separately.
    {
        char value_buf[1024];

        g_assert_null(fstReaderGetValueFromHandleAtTime(reader, 1, 1, value_buf));
    }

    fstReaderClose(reader);

    g_assert_cmpint(unlink(FST_NAME), ==, 0);

    return 0;
}

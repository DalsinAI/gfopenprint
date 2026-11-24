/* Copyright (c) 2026 AmiCradle Team. SPDX-License-Identifier: MIT */
/* PWG Raster writer (src/core/pwg.c): header fields at PWG 5102.4's offsets, and every row read back by a
 * decoder written here from the spec. */
#include "oap_raster.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures, checks;
#define CHECK(c) do { checks++; if (!(c)) { failures++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)

static unsigned long be32(const unsigned char *p)
{
    return ((unsigned long)p[0] << 24) | ((unsigned long)p[1] << 16) | ((unsigned long)p[2] << 8) | p[3];
}

/* One row back from its coding; returns bytes read, 0 when the coding is bad. */
static size_t decode_row(const unsigned char *in, size_t avail, size_t pb, size_t npix, unsigned char *row)
{
    size_t i = 0, x = 0;
    while (x < npix) {
        unsigned c;
        if (i >= avail) return 0;
        c = in[i++];
        if (c < 128) {
            size_t k, n = c + 1;
            if (x + n > npix || i + pb > avail) return 0;
            for (k = 0; k < n; k++) memcpy(row + (x + k) * pb, in + i, pb);
            i += pb; x += n;
        } else {
            size_t n = 257 - c;
            if (c == 128 || x + n > npix || i + n * pb > avail) return 0;
            memcpy(row + x * pb, in + i, n * pb);
            i += n * pb; x += n;
        }
    }
    return i;
}

typedef struct Buf { unsigned char *b; size_t n, cap; } Buf;
static int sink(void *ctx, const unsigned char *bytes, size_t n)
{
    Buf *b = ctx;
    if (b->n + n > b->cap) {
        size_t cap = (b->n + n) * 2;
        unsigned char *nb = realloc(b->b, cap);
        if (!nb) return 0;
        b->b = nb; b->cap = cap;
    }
    memcpy(b->b + b->n, bytes, n); b->n += n;
    return 1;
}

static void roundtrip(const unsigned char *row, size_t bytes, size_t pb, const char *what)
{
    unsigned char *coded = malloc(oap_pwg_row_cap(bytes)), *back = malloc(bytes);
    size_t n = oap_pwg_encode_row(row, bytes, pb, coded);
    CHECK(n > 0 && n <= oap_pwg_row_cap(bytes));
    CHECK(decode_row(coded, n, pb, bytes / pb, back) == n);
    if (memcmp(back, row, bytes)) { failures++; printf("FAIL roundtrip %s\n", what); }
    checks++;
    free(coded); free(back);
}

static void test_rows(void)
{
    unsigned char row[3 * 700], coded[16];
    size_t i, n;
    unsigned seed = 1234;
    /* one pixel; all alike (a run longer than 128 splits); all different; mixed */
    row[0] = 7; roundtrip(row, 1, 1, "one");
    memset(row, 255, 700); roundtrip(row, 700, 1, "white");
    n = oap_pwg_encode_row(row, 700, 1, coded);
    CHECK(n == 12);                         /* 5 runs of 128, one of 60: two bytes each */
    for (i = 0; i < 700; i++) row[i] = (unsigned char)i;
    roundtrip(row, 700, 1, "ramp");
    for (i = 0; i < 700; i++) row[i] = (unsigned char)((i / 3) & 1 ? 0 : 255);
    roundtrip(row, 700, 1, "bars");
    for (i = 0; i < sizeof(row); i++) { seed = seed * 1103515245u + 12345u; row[i] = (unsigned char)((seed >> 16) & 3); }
    roundtrip(row, 700, 1, "random grey");
    roundtrip(row, 3 * 700, 3, "random rgb");
    /* literal then a pair, the edge where a run starts */
    { unsigned char r[5] = {1, 2, 3, 3, 4}; roundtrip(r, 5, 1, "literal-pair");
      n = oap_pwg_encode_row(r, 5, 1, coded);
      CHECK(n == 7 && coded[0] == 255 && coded[3] == 1 && coded[4] == 3 && coded[5] == 0 && coded[6] == 4); }
    CHECK(oap_pwg_encode_row(row, 7, 3, coded) == 0);   /* not a whole number of pixels */
}

static void test_header(void)
{
    OAPPwgPage p;
    unsigned char h[OAP_PWG_HEADER];
    memset(&p, 0, sizeof(p));
    p.width = 4960; p.height = 7016; p.dpi = 600; p.colorspace = OAP_PWG_SGRAY;
    p.page_w_pt = 595; p.page_h_pt = 842; p.copies = 2; p.duplex = 1; p.total_pages = 3;
    p.media_name = "iso_a4_210x297mm";
    CHECK(oap_pwg_header(&p, h));
    CHECK(!memcmp(h, "PwgRaster", 10));
    CHECK(be32(h + 272) == 1 && be32(h + 368) == 0);
    CHECK(be32(h + 276) == 600 && be32(h + 280) == 600);
    CHECK(be32(h + 340) == 2);
    CHECK(be32(h + 352) == 595 && be32(h + 356) == 842);
    CHECK(be32(h + 372) == 4960 && be32(h + 376) == 7016);
    CHECK(be32(h + 384) == 8 && be32(h + 388) == 8 && be32(h + 392) == 4960);
    CHECK(be32(h + 400) == 18 && be32(h + 420) == 1);
    CHECK(be32(h + 452) == 3);
    CHECK(!strcmp((const char *)h + 1732, "iso_a4_210x297mm"));
    p.colorspace = OAP_PWG_SRGB;
    CHECK(oap_pwg_header(&p, h) && be32(h + 388) == 24 && be32(h + 392) == 4960 * 3 && be32(h + 400) == 19);
    p.colorspace = 5; CHECK(!oap_pwg_header(&p, h));
    p.colorspace = OAP_PWG_SGRAY; p.dpi = 0; CHECK(!oap_pwg_header(&p, h));
}

/* Two pages through the writer, read back whole: line repeats included. */
static void test_stream(void)
{
    Buf b = {0, 0, 0};
    OAPPwgWriter w;
    OAPPwgPage p;
    unsigned char row[40], back[40];
    unsigned long y, page;
    size_t at;
    memset(&p, 0, sizeof(p));
    p.width = 40; p.height = 300; p.dpi = 300; p.colorspace = OAP_PWG_SGRAY; p.page_w_pt = 10; p.page_h_pt = 72;
    CHECK(oap_pwg_start(&w, sink, &b));
    for (page = 0; page < 2; page++) {
        CHECK(oap_pwg_page(&w, &p));
        for (y = 0; y < p.height; y++) {
            memset(row, 255, sizeof(row));
            if (y >= 10 && y < 290) row[(y + page) % 40] = 0;   /* a diagonal; blank rows top and bottom */
            if (y >= 100 && y < 400) memset(row, 0, 5);          /* not a repeat: the diagonal moves */
            CHECK(oap_pwg_row(&w, row) || (printf("row %lu\n", y), 0));
        }
        CHECK(!oap_pwg_row(&w, row));                           /* one row too many */
        CHECK(oap_pwg_end_page(&w));
    }
    oap_pwg_free(&w);
    CHECK(b.n > 4 && !memcmp(b.b, "RaS2", 4));
    at = 4;
    for (page = 0; page < 2; page++) {
        unsigned long rows = 0;
        CHECK(at + OAP_PWG_HEADER <= b.n && be32(b.b + at + 376) == 300);
        at += OAP_PWG_HEADER;
        while (rows < 300 && at < b.n) {
            unsigned long repeat = b.b[at++] + 1, k;
            size_t used = decode_row(b.b + at, b.n - at, 1, 40, back);
            CHECK(used > 0);
            if (!used) return;
            at += used;
            for (k = 0; k < repeat; k++, rows++) {
                memset(row, 255, sizeof(row));
                if (rows >= 10 && rows < 290) row[(rows + page) % 40] = 0;
                if (rows >= 100 && rows < 400) memset(row, 0, 5);
                if (memcmp(row, back, 40)) { failures++; printf("FAIL page %lu row %lu\n", page, rows); return; }
            }
        }
        CHECK(rows == 300);
    }
    CHECK(at == b.n);
    free(b.b);
    /* a page ended short is refused */
    memset(&b, 0, sizeof(b));
    CHECK(oap_pwg_start(&w, sink, &b) && oap_pwg_page(&w, &p) && oap_pwg_row(&w, row));
    CHECK(!oap_pwg_end_page(&w));
    oap_pwg_free(&w); free(b.b);
}

int main(void)
{
    test_rows();
    test_header();
    test_stream();
    printf("test_pwg: %d checks, %d failed\n", checks, failures);
    return failures != 0;
}

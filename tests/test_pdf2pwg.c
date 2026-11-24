/* Copyright (c) 2026 AmiCradle Team. SPDX-License-Identifier: MIT */
/* GFOpenPrint's own PDFs as PWG Raster (src/core/pdf2pwg.c), from the PDFs test_driver saves in build/: each page
 * read back from the stream (its own decoder, from PWG 5102.4) is the page as oap_pdfr_render draws it, turned a
 * quarter when it is landscape; the page range; text without a font refused; a failed write reported. */
#include "oap_raster.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures, checks;
#define CHECK(c) do { checks++; if (!(c)) { failures++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)

static unsigned char *load(const char *path, size_t *n)
{
    FILE *f = fopen(path, "rb");
    unsigned char *b;
    long len;
    if (!f) { printf("FAIL %s missing: run make test-driver first\n", path); failures++; return NULL; }
    fseek(f, 0, SEEK_END); len = ftell(f); fseek(f, 0, SEEK_SET);
    b = malloc((size_t)len + 1);
    if (b && fread(b, 1, (size_t)len, f) != (size_t)len) { free(b); b = NULL; }
    fclose(f);
    *n = (size_t)len;
    return b;
}

static void draw(void *ctx, OAPCanvas *c, long x, long y, unsigned long size, unsigned char ch)
{
    (void)ctx; (void)size; (void)ch;
    if (x >= 0 && y >= 0 && (unsigned long)x < c->w && (unsigned long)y < c->h) c->pix[((size_t)y * c->w + (size_t)x) * (size_t)c->channels] = 0;
}

typedef struct Buf { unsigned char *b; size_t n, cap; int fail_after; } Buf;
static int sink(void *ctx, const unsigned char *bytes, size_t n)
{
    Buf *b = ctx;
    if (b->fail_after && b->n + n > (size_t)b->fail_after) return 0;
    if (b->n + n > b->cap) {
        size_t cap = (b->n + n) * 2;
        unsigned char *nb = realloc(b->b, cap);
        if (!nb) return 0;
        b->b = nb; b->cap = cap;
    }
    memcpy(b->b + b->n, bytes, n); b->n += n;
    return 1;
}

static unsigned long be32(const unsigned char *p)
{
    return ((unsigned long)p[0] << 24) | ((unsigned long)p[1] << 16) | ((unsigned long)p[2] << 8) | p[3];
}

/* One page back from the stream at *at: its header's numbers and its pixels (malloc'd). */
typedef struct Back { unsigned long w, h, dpi, cs, bpp, line, total, w_pt, h_pt; char media[65]; unsigned char *pix; } Back;
static int read_page(const Buf *b, size_t *at, Back *pg)
{
    const unsigned char *h = b->b + *at, *end = b->b + b->n, *p;
    unsigned long y = 0, pb;
    if ((size_t)(end - h) < 1796 || memcmp(h, "PwgRaster", 10)) return 0;
    pg->dpi = be32(h + 276); pg->w_pt = be32(h + 352); pg->h_pt = be32(h + 356);
    pg->w = be32(h + 372); pg->h = be32(h + 376); pg->bpp = be32(h + 388); pg->line = be32(h + 392);
    pg->cs = be32(h + 400); pg->total = be32(h + 452);
    memcpy(pg->media, h + 1732, 64); pg->media[64] = 0;
    pb = pg->bpp / 8;
    if (!pb || pg->line != pg->w * pb) return 0;
    pg->pix = malloc(pg->w * pg->h * pb);
    p = h + 1796;
    while (y < pg->h) {
        unsigned long rep, x = 0;
        unsigned char *row = pg->pix + y * pg->line;
        if (p >= end) return 0;
        rep = (unsigned long)*p++ + 1;
        while (x < pg->w) {
            unsigned c;
            if (p >= end) return 0;
            c = *p++;
            if (c < 128) {
                unsigned long k, n = c + 1;
                if (x + n > pg->w || (size_t)(end - p) < pb) return 0;
                for (k = 0; k < n; k++) memcpy(row + (x + k) * pb, p, pb);
                p += pb; x += n;
            } else {
                unsigned long n = 257 - c;
                if (c == 128 || x + n > pg->w || (size_t)(end - p) < n * pb) return 0;
                memcpy(row + x * pb, p, n * pb); p += n * pb; x += n;
            }
        }
        if (y + rep > pg->h) return 0;
        while (--rep) { memcpy(row + pg->line, row, pg->line); row += pg->line; y++; }
        y++;
    }
    *at = (size_t)(p - b->b);
    return 1;
}

/* The page as oap_pdfr_render draws it, for comparing */
static unsigned char *direct(const unsigned char *pdf, size_t n, unsigned long page, unsigned long dpi, int ch,
                             unsigned long *w, unsigned long *h)
{
    static OAPPdfDoc d;
    OAPCanvas c;
    unsigned long wp, hp;
    char err[160];
    if (!oap_pdfr_open(&d, pdf, n, err, sizeof(err)) || !oap_pdfr_page_size(&d, page, &wp, &hp)) return NULL;
    c.w = (wp * dpi + 36) / 72; c.h = (hp * dpi + 36) / 72; c.channels = ch; c.dpi = dpi;
    c.pix = malloc(c.w * c.h * (size_t)ch);
    memset(c.pix, 255, c.w * c.h * (size_t)ch);
    if (!oap_pdfr_render(&d, page, &c, draw, NULL, err, sizeof(err))) { free(c.pix); return NULL; }
    *w = c.w; *h = c.h;
    return c.pix;
}

static void test_landscape_rgb(void)
{
    size_t n, at = 4;
    unsigned char *pdf = load("build/driver-picture.pdf", &n), *want;
    OAPRasterJob j;
    Buf b = {0};
    Back pg;
    char err[160] = "";
    unsigned long page, w, h, x, y, bad = 0;
    if (!pdf) return;
    memset(&j, 0, sizeof(j));
    j.dpi = 72; j.rgb = 1; j.media_name = "iso_a4_210x297mm"; j.draw = draw;
    CHECK(oap_pdf_to_pwg(pdf, n, &j, sink, &b, err, sizeof(err)) == 2 || (printf("%s\n", err), 0));
    CHECK(b.n > 4 && !memcmp(b.b, "RaS2", 4));
    for (page = 0; page < 2; page++) {
        CHECK(read_page(&b, &at, &pg));
        /* the picture's page, 842 x 595 pt landscape, upright on the paper: 595 wide and 842 down; the text's
         * page is upright already */
        CHECK(pg.w == 595 && pg.h == 842 && pg.dpi == 72 && pg.cs == OAP_PWG_SRGB && pg.bpp == 24);
        CHECK(pg.w_pt == 595 && pg.h_pt == 842 && pg.total == 2 && !strcmp(pg.media, "iso_a4_210x297mm"));
        want = direct(pdf, n, page, 72, 3, &w, &h);
        CHECK(want && w == (page ? 595 : 842) && h == (page ? 842 : 595));
        if (want) {
            for (y = 0; y < pg.h; y++) for (x = 0; x < pg.w; x++)
                if (memcmp(pg.pix + (y * pg.w + x) * 3, page ? want + (y * w + x) * 3 : want + (x * w + (w - 1 - y)) * 3, 3)) bad++;
            CHECK(bad == 0);
            if (!page) {      /* the picture's red column (page x 268, y 100): on the paper at x 100, y 573 */
                const unsigned char *r = pg.pix + (573 * pg.w + 100) * 3;
                CHECK(r[0] == 255 && r[1] == 0 && r[2] == 0);
            }
        }
        free(want); free(pg.pix);
    }
    CHECK(at == b.n);
    free(b.b); free(pdf);
}

static void test_range_grey(void)
{
    size_t n, at = 4;
    unsigned char *pdf = load("build/driver-300-lines.pdf", &n), *want;
    OAPRasterJob j;
    Buf b = {0};
    Back pg;
    char err[160] = "";
    unsigned long page, w, h, dark = 0, i;
    if (!pdf) return;
    memset(&j, 0, sizeof(j));
    j.dpi = 100; j.first = 2; j.last = 3; j.draw = draw;
    CHECK(oap_pdf_to_pwg(pdf, n, &j, sink, &b, err, sizeof(err)) == 2 || (printf("%s\n", err), 0));
    for (page = 1; page <= 2; page++) {
        CHECK(read_page(&b, &at, &pg));
        CHECK(pg.w == 826 && pg.h == 1169 && pg.cs == OAP_PWG_SGRAY && pg.bpp == 8 && pg.total == 2 && !pg.media[0]);
        want = direct(pdf, n, page, 100, 1, &w, &h);
        CHECK(want && w == pg.w && h == pg.h && !memcmp(want, pg.pix, w * h));
        for (i = 0; i < pg.w * pg.h; i++) dark += !pg.pix[i];
        free(want); free(pg.pix);
    }
    CHECK(dark == 2 * 66 * 40);           /* the stand-in font's dot for each character */
    CHECK(at == b.n);
    /* past the end: what there is; nothing of it: refused */
    b.n = 0; j.first = 5; j.last = 9;
    CHECK(oap_pdf_to_pwg(pdf, n, &j, sink, &b, err, sizeof(err)) == 1);
    b.n = 0; j.first = 6; j.last = 9;
    CHECK(oap_pdf_to_pwg(pdf, n, &j, sink, &b, err, sizeof(err)) == 0 && strstr(err, "None of the pages"));
    /* text with no font: refused, not printed blank */
    b.n = 0; j.first = j.last = 0; j.draw = NULL;
    CHECK(oap_pdf_to_pwg(pdf, n, &j, sink, &b, err, sizeof(err)) == 0 && strstr(err, "No font"));
    /* the writer failing part way: 0, in words */
    b.n = 0; b.fail_after = 5000; j.draw = draw;
    CHECK(oap_pdf_to_pwg(pdf, n, &j, sink, &b, err, sizeof(err)) == 0 && strstr(err, "Cannot write"));
    /* bad settings, and a PDF that isn't GFOpenPrint's */
    j.dpi = 50;
    CHECK(oap_pdf_to_pwg(pdf, n, &j, sink, &b, err, sizeof(err)) == 0);
    j.dpi = 300;
    CHECK(oap_pdf_to_pwg((const unsigned char *)"%PDF-1.4\nnothing\n", 17, &j, sink, &b, err, sizeof(err)) == 0 && err[0]);
    free(b.b); free(pdf);
}

static void test_pictures_need_no_font(void)
{
    size_t n;
    unsigned char *pdf = load("build/driver-picture.pdf", &n);
    OAPRasterJob j;
    Buf b = {0};
    char err[160] = "";
    if (!pdf) return;
    memset(&j, 0, sizeof(j));
    j.dpi = 150; j.first = j.last = 1;          /* the picture's page has no text: no font needed */
    CHECK(oap_pdf_to_pwg(pdf, n, &j, sink, &b, err, sizeof(err)) == 1 || (printf("%s\n", err), 0));
    b.n = 0; j.first = j.last = 0;
    CHECK(oap_pdf_to_pwg(pdf, n, &j, sink, &b, err, sizeof(err)) == 0 && strstr(err, "No font"));
    free(b.b); free(pdf);
}

int main(void)
{
    test_landscape_rgb();
    test_range_grey();
    test_pictures_need_no_font();
    printf("test_pdf2pwg: %d checks, %d failed\n", checks, failures);
    return failures != 0;
}

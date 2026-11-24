/* Copyright (c) 2026 AmiCradle Team. SPDX-License-Identifier: MIT */
/* GFOpenPrint's own PDFs as pixels (src/core/pdfr.c), from the PDFs test_driver saves in build/ (make test-driver
 * first): pages and paper, each character where Courier puts it, a picture's pixels where cm puts them, and other
 * PDFs refused with words. */
#include "oap_pdfr.h"
#include "oap_inflate.h"
#include "oav_core.h"
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

/* The stand-in font: each character a record, and a dot at its left edge on the baseline. */
typedef struct Drawn { long x, y; unsigned long size; unsigned char ch; } Drawn;
static Drawn drawn[20000];
static unsigned long ndrawn;
static void draw(void *ctx, OAPCanvas *c, long x, long y, unsigned long size, unsigned char ch)
{
    (void)ctx;
    if (ndrawn < sizeof(drawn) / sizeof(*drawn)) { drawn[ndrawn].x = x; drawn[ndrawn].y = y; drawn[ndrawn].size = size; drawn[ndrawn].ch = ch; ndrawn++; }
    if (x >= 0 && y >= 0 && (unsigned long)x < c->w && (unsigned long)y < c->h) c->pix[((size_t)y * c->w + (size_t)x) * (size_t)c->channels] = 0;
}

static int canvas(OAPCanvas *c, const OAPPdfDoc *d, unsigned long page, unsigned long dpi, int channels)
{
    unsigned long w, h;
    if (!oap_pdfr_page_size(d, page, &w, &h)) return 0;
    c->dpi = dpi; c->channels = channels;
    c->w = w * dpi / 72; c->h = h * dpi / 72;
    c->pix = malloc(c->w * c->h * (size_t)channels);
    if (!c->pix) return 0;
    memset(c->pix, 255, c->w * c->h * (size_t)channels);
    return 1;
}

static void test_text(void)
{
    size_t n;
    unsigned char *pdf = load("build/driver-300-lines.pdf", &n);
    OAPPdfDoc d;
    OAPCanvas c;
    char err[160];
    unsigned long w, h, i;
    if (!pdf) return;
    CHECK(oap_pdfr_open(&d, pdf, n, err, sizeof(err)));
    CHECK(d.pages == 5);
    CHECK(oap_pdfr_page_size(&d, 0, &w, &h) && w == 595 && h == 842);
    CHECK(canvas(&c, &d, 0, 300, 1));
    ndrawn = 0;
    CHECK(oap_pdfr_render(&d, 0, &c, draw, NULL, err, sizeof(err)) || (printf("%s\n", err), 0));
    /* 66 lines: "0001 The quick brown fox jumps over the lazy dog." is 40 characters that aren't spaces */
    CHECK(ndrawn == 66 * 40);
    CHECK(drawn[0].ch == '0' && drawn[3].ch == '1' && drawn[4].ch == 'T');
    /* 10 point Courier at 300 dpi: 42 pixels; each character 0.6 of it on (25 pixels a column) */
    CHECK(drawn[0].size == 42);
    CHECK(drawn[4].x - drawn[0].x == 125);         /* "0001 T": T is the sixth column */
    CHECK(drawn[40].y > drawn[0].y && drawn[40].y - drawn[0].y >= 48 && drawn[40].y - drawn[0].y <= 49);  /* 11.7 pt */
    for (i = 0; i < ndrawn; i++) if (drawn[i].x < 0 || drawn[i].y < 0 || (unsigned long)drawn[i].x >= c.w || (unsigned long)drawn[i].y >= c.h) break;
    CHECK(i == ndrawn);                             /* all on the paper */
    CHECK(c.pix[(size_t)drawn[0].y * c.w + (size_t)drawn[0].x] == 0);
    /* the last page: lines 265-300 */
    ndrawn = 0;
    CHECK(oap_pdfr_render(&d, 4, &c, draw, NULL, err, sizeof(err)));
    CHECK(ndrawn == 36 * 40 && drawn[0].ch == '0' && drawn[1].ch == '2' && drawn[2].ch == '6' && drawn[3].ch == '5');
    CHECK(!oap_pdfr_render(&d, 5, &c, draw, NULL, err, sizeof(err)));
    free(c.pix);
    free(pdf);
}

static void test_latin1(void)
{
    size_t n;
    unsigned char *pdf = load("build/driver-latin1.pdf", &n);
    OAPPdfDoc d;
    OAPCanvas c;
    char err[160];
    static const unsigned char want[] = "Gr\xfc\xdf" "e,\xe9t\xe9\xa3";
    unsigned long i;
    if (!pdf) return;
    CHECK(oap_pdfr_open(&d, pdf, n, err, sizeof(err)) && canvas(&c, &d, 0, 72, 1));
    ndrawn = 0;
    CHECK(oap_pdfr_render(&d, 0, &c, draw, NULL, err, sizeof(err)));
    CHECK(ndrawn == sizeof(want) - 1);
    for (i = 0; i < ndrawn && i < sizeof(want) - 1; i++) CHECK(drawn[i].ch == want[i]);
    free(c.pix);
    free(pdf);
}

static void test_picture(void)
{
    size_t n;
    unsigned char *pdf = load("build/driver-picture.pdf", &n);
    OAPPdfDoc d;
    OAPCanvas c;
    char err[160];
    unsigned long w, h;
    if (!pdf) return;
    CHECK(oap_pdfr_open(&d, pdf, n, err, sizeof(err)) && d.pages == 2);
    CHECK(oap_pdfr_page_size(&d, 0, &w, &h) && w == 842 && h == 595);
    /* at 72 dpi the picture (5 x 2) is drawn 762 x 304 at 40,251: canvas x 40-802, y 40-344 */
    CHECK(canvas(&c, &d, 0, 72, 3));
    CHECK(oap_pdfr_render(&d, 0, &c, draw, NULL, err, sizeof(err)) || (printf("%s\n", err), 0));
#define AT(x, y) (c.pix + ((size_t)(y) * c.w + (size_t)(x)) * 3)
    CHECK(AT(100, 100)[0] == 255 && AT(100, 100)[1] == 255);          /* column 0: white */
    CHECK(AT(268, 100)[0] == 255 && AT(268, 100)[1] == 0 && AT(268, 100)[2] == 0);   /* column 1: red */
    CHECK(AT(500, 300)[0] == 0 && AT(500, 300)[1] == 0);              /* columns 2-4: black */
    CHECK(AT(20, 20)[0] == 255 && AT(820, 400)[0] == 255);            /* outside: the paper */
    CHECK(AT(500, 39)[0] == 255 && AT(500, 40)[0] == 0 && AT(500, 343)[0] == 0 && AT(500, 344)[0] == 255);
    free(c.pix);
    /* grey at 300 dpi: red as its brightness */
    CHECK(canvas(&c, &d, 0, 300, 1));
    CHECK(oap_pdfr_render(&d, 0, &c, draw, NULL, err, sizeof(err)));
    CHECK(c.pix[(size_t)(100 * 300 / 72) * c.w + (size_t)(268 * 300 / 72)] == 76);
    free(c.pix);
    /* the text page after it */
    CHECK(canvas(&c, &d, 1, 72, 1));
    ndrawn = 0;
    CHECK(oap_pdfr_render(&d, 1, &c, draw, NULL, err, sizeof(err)) && ndrawn == 5);
    free(c.pix);
    free(pdf);
}

/* Printers and Queue's test page (src/core/pdf_demo.c): a grey box with a black edge, and its text */
int oap_pdf_write_demo(const char *path, const char *title);
static void test_test_page(void)
{
    OAPPdfDoc d;
    OAPCanvas c;
    char err[160];
    size_t n;
    unsigned char *pdf;
    CHECK(oap_pdf_write_demo("build/test-page.pdf", "GFOpenPrint test page"));
    pdf = load("build/test-page.pdf", &n);
    if (!pdf) return;
    CHECK(oap_pdfr_open(&d, pdf, n, err, sizeof(err)) && canvas(&c, &d, 0, 72, 1));
    ndrawn = 0;
    CHECK(oap_pdfr_render(&d, 0, &c, draw, NULL, err, sizeof(err)) || (printf("%s\n", err), 0));
    CHECK(ndrawn == strlen("GFOpenPrint") + strlen("GFOpenPrinttestpage") + strlen("PDFgeneration+nativeIPPprintingfirstlight"));
    /* the box: 40,560 515 x 210 pt, so canvas x 40-555, y 72-282 at 72 dpi */
    CHECK(c.pix[150 * c.w + 300] == 240);                       /* 0.94 g inside */
    CHECK(c.pix[150 * c.w + 40] == 0 && c.pix[72 * c.w + 300] == 0 && c.pix[282 * c.w + 300] == 0 && c.pix[150 * c.w + 555] == 0);
    CHECK(c.pix[150 * c.w + 38] == 255 && c.pix[300 * c.w + 300] == 255 && c.pix[60 * c.w + 300] == 255);
    free(c.pix); free(pdf);
    remove("build/test-page.pdf");
}

/* Not GFOpenPrint's: refused, with words, never drawn wrong. */
static void test_refused(void)
{
    OAPPdfDoc d;
    OAPCanvas c;
    char err[160], pdf[1400];
    unsigned char pix[100];
    const char *body = "0 0 1 rg 10 10 50 50 re f\n";
    int len, off[6], n = 0;
    CHECK(!oap_pdfr_open(&d, (const unsigned char *)"hello", 5, err, sizeof(err)) && strstr(err, "Not a PDF"));
    CHECK(!oap_pdfr_open(&d, (const unsigned char *)"%PDF-1.4\n%%EOF\n", 15, err, sizeof(err)));
    /* a page that draws a filled rectangle: a vector drawing */
    len = snprintf(pdf, sizeof(pdf), "%%PDF-1.4\n");
    off[n++] = len; len += snprintf(pdf + len, sizeof(pdf) - (size_t)len, "1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n");
    off[n++] = len; len += snprintf(pdf + len, sizeof(pdf) - (size_t)len, "2 0 obj\n<< /Type /Pages /Count 1 /Kids [3 0 R ] >>\nendobj\n");
    off[n++] = len; len += snprintf(pdf + len, sizeof(pdf) - (size_t)len, "3 0 obj\n<< /Type /Page /Parent 2 0 R /MediaBox [0 0 100 100] /Contents 4 0 R >>\nendobj\n");
    off[n++] = len; len += snprintf(pdf + len, sizeof(pdf) - (size_t)len, "4 0 obj\n<< /Length %d >>\nstream\n%sendstream\nendobj\n", (int)strlen(body), body);
    {
        int xref = len, i;
        len += snprintf(pdf + len, sizeof(pdf) - (size_t)len, "xref\n0 5\n0000000000 65535 f \n");
        for (i = 0; i < 4; i++) len += snprintf(pdf + len, sizeof(pdf) - (size_t)len, "%010d 00000 n \n", off[i]);
        len += snprintf(pdf + len, sizeof(pdf) - (size_t)len, "trailer << /Size 5 /Root 1 0 R >>\nstartxref\n%d\n%%%%EOF\n", xref);
    }
    CHECK(oap_pdfr_open(&d, (const unsigned char *)pdf, (size_t)len, err, sizeof(err)) || (printf("%s\n", err), 0));
    c.pix = pix; c.w = 10; c.h = 10; c.channels = 1; c.dpi = 7;
    CHECK(!oap_pdfr_render(&d, 0, &c, draw, NULL, err, sizeof(err)) && strstr(err, "printer that takes PDF"));
}

/* zlib streams made by zlib itself (python3 zlib, level 9 and Z_FIXED) of 40 x "GFOpenPrint raster " and the
 * bytes 0-255; and a stored block made here. */
static const unsigned char z_dynamic[304] = {
    120,218,115,119,243,47,72,205,11,40,202,204,43,81,40,74,44,46,73,45,82,112,31,21,
    26,21,26,238,66,12,140,76,204,44,172,108,236,28,156,92,220,60,188,124,252,2,130,66,
    194,34,162,98,226,18,146,82,210,50,178,114,242,10,138,74,202,42,170,106,234,26,154,90,
    218,58,186,122,250,6,134,70,198,38,166,102,230,22,150,86,214,54,182,118,246,14,142,78,
    206,46,174,110,238,30,158,94,222,62,190,126,254,1,129,65,193,33,161,97,225,17,145,81,
    209,49,177,113,241,9,137,73,201,41,169,105,233,25,153,89,217,57,185,121,249,5,133,69,
    197,37,165,101,229,21,149,85,213,53,181,117,245,13,141,77,205,45,173,109,237,29,157,93,
    221,61,189,125,253,19,38,78,154,60,101,234,180,233,51,102,206,154,61,103,238,188,249,11,
    22,46,90,188,100,233,178,229,43,86,174,90,189,102,237,186,245,27,54,110,218,188,101,235,
    182,237,59,118,238,218,189,103,239,190,253,7,14,30,58,124,228,232,177,227,39,78,158,58,
    125,230,236,185,243,23,46,94,186,124,229,234,181,235,55,110,222,186,125,231,238,189,251,15,
    30,62,122,252,228,233,179,231,47,94,190,122,253,230,237,187,247,31,62,126,250,252,229,235,
    183,239,63,126,254,250,253,231,239,191,255,0,196,123,151,24,
};
static const unsigned char z_fixed[304] = {
    120,1,115,119,243,47,72,205,11,40,202,204,43,81,40,74,44,46,73,45,82,112,31,21,
    26,21,26,238,66,12,140,76,204,44,172,108,236,28,156,92,220,60,188,124,252,2,130,66,
    194,34,162,98,226,18,146,82,210,50,178,114,242,10,138,74,202,42,170,106,234,26,154,90,
    218,58,186,122,250,6,134,70,198,38,166,102,230,22,150,86,214,54,182,118,246,14,142,78,
    206,46,174,110,238,30,158,94,222,62,190,126,254,1,129,65,193,33,161,97,225,17,145,81,
    209,49,177,113,241,9,137,73,201,41,169,105,233,25,153,89,217,57,185,121,249,5,133,69,
    197,37,165,101,229,21,149,85,213,53,181,117,245,13,141,77,205,45,173,109,237,29,157,93,
    221,61,189,125,253,19,38,78,154,60,101,234,180,233,51,102,206,154,61,103,238,188,249,11,
    22,46,90,188,100,233,178,229,43,86,174,90,189,102,237,186,245,27,54,110,218,188,101,235,
    182,237,59,118,238,218,189,103,239,190,253,7,14,30,58,124,228,232,177,227,39,78,158,58,
    125,230,236,185,243,23,46,94,186,124,229,234,181,235,55,110,222,186,125,231,238,189,251,15,
    30,62,122,252,228,233,179,231,47,94,190,122,253,230,237,187,247,31,62,126,250,252,229,235,
    183,239,63,126,254,250,253,231,239,191,255,0,196,123,151,24,
};

static void expect(unsigned char *want)
{
    int i;
    for (i = 0; i < 40; i++) memcpy(want + i * 19, "GFOpenPrint raster ", 19);
    for (i = 0; i < 256; i++) want[760 + i] = (unsigned char)i;
}

static void test_inflate(void)
{
    unsigned char want[1016], got[1100], stored[1100];
    size_t n = 0;
    expect(want);
    CHECK(oap_inflate_zlib(z_dynamic, sizeof(z_dynamic), got, sizeof(got), &n) && n == 1016 && !memcmp(got, want, 1016));
    CHECK(oap_inflate_zlib(z_fixed, sizeof(z_fixed), got, sizeof(got), &n) && n == 1016 && !memcmp(got, want, 1016));
    stored[0] = 0x78; stored[1] = 0x01; stored[2] = 1;
    stored[3] = 1016 & 255; stored[4] = 1016 >> 8; stored[5] = (unsigned char)~stored[3]; stored[6] = (unsigned char)~stored[4];
    memcpy(stored + 7, want, 1016);
    CHECK(oap_inflate_zlib(stored, 1023, got, sizeof(got), &n) && n == 1016 && !memcmp(got, want, 1016));
    CHECK(!oap_inflate_zlib(z_dynamic, sizeof(z_dynamic), got, 1000, &n));          /* too small: refused */
    CHECK(!oap_inflate_zlib(z_dynamic, sizeof(z_dynamic) / 2, got, sizeof(got), &n)); /* cut short */
    CHECK(!oap_inflate_zlib(want, 100, got, sizeof(got), &n));                          /* not zlib */
}

/* GFOpenView's own picture PDFs (src/viewer/oav_core.c oav_pdf_rgb): left half red, right half blue, a green
 * band at the top; drawn back at 150 dpi. */
static int view_row(void *ctx, unsigned long y, unsigned char *rgb, size_t bytes)
{
    size_t x, w = bytes / 3;
    (void)ctx;
    for (x = 0; x < w; x++) {
        unsigned char *p = rgb + x * 3;
        if (y < 20) { p[0] = 0; p[1] = 255; p[2] = 0; }
        else if (x < w / 2) { p[0] = 255; p[1] = 0; p[2] = 0; }
        else { p[0] = 0; p[1] = 0; p[2] = 255; }
    }
    return 1;
}

static void test_viewer_pdf(void)
{
    OAVLayout s;
    FILE *f;
    char err[160];
    size_t n;
    unsigned char *pdf;
    OAPPdfDoc d;
    OAPCanvas c;
    unsigned long x, y, red = 0, blue = 0, green = 0, white = 0, other = 0;
    oav_layout_defaults(&s);
    f = fopen("build/pdfr-viewer.pdf", "wb");
    CHECK(f && oav_pdf_rgb(f, 400, 300, &s, view_row, NULL, err, sizeof(err)));
    if (f) fclose(f);
    pdf = load("build/pdfr-viewer.pdf", &n);
    if (!pdf) return;
    CHECK(oap_pdfr_open(&d, pdf, n, err, sizeof(err)) || (printf("%s\n", err), 0));
    CHECK(canvas(&c, &d, 0, 150, 3));
    CHECK(oap_pdfr_render(&d, 0, &c, draw, NULL, err, sizeof(err)) || (printf("%s\n", err), 0));
    for (y = 0; y < c.h; y++) for (x = 0; x < c.w; x++) {
        const unsigned char *p = c.pix + (y * c.w + x) * 3;
        if (p[0] == 255 && p[1] == 0 && p[2] == 0) red++;
        else if (p[0] == 0 && p[1] == 0 && p[2] == 255) blue++;
        else if (p[0] == 0 && p[1] == 255 && p[2] == 0) green++;
        else if (p[0] == 255 && p[1] == 255 && p[2] == 255) white++;
        else other++;
    }
    /* the picture fills the paper's width inside the margins: red and blue alike, green a band of 20/300 */
    CHECK(red > 0 && blue > 0 && green > 0 && white > 0);
    CHECK(red * 100 / blue >= 97 && red * 100 / blue <= 103);
    CHECK(green * 100 / (red + blue + green) >= 5 && green * 100 / (red + blue + green) <= 8);
    CHECK(other * 1000 / (c.w * c.h) < 5);          /* edges where averaging mixed two colours */
    free(c.pix);
    free(pdf);
}

int main(void)
{
    test_text();
    test_latin1();
    test_picture();
    test_refused();
    test_test_page();
    test_inflate();
    test_viewer_pdf();
    printf("test_pdfr: %d checks, %d failed\n", checks, failures);
    return failures != 0;
}

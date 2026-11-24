/* Copyright (c) 2026 AmiCradle Team. SPDX-License-Identifier: MIT */
/* The GFOS test page (src/core/testpage.c): a PDF GFOpenPrint's own reader draws (src/core/pdfr.c), its colour
 * swatches and grey ramp where they belong in colour and in grey, every line's text drawn, ( ) and \ in a value
 * kept; a $VER read from a file; and this machine's address as a printer would see it (src/net/conn.c). */
#include "oap_testpage.h"
#include "oap_pdfr.h"
#include "oap_net.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures, checks;
#define CHECK(c) do { checks++; if (!(c)) { failures++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)

static char drawn[8192];
static size_t ndrawn;
static void draw(void *ctx, OAPCanvas *c, long x, long y, unsigned long size, unsigned char ch)
{
    (void)ctx; (void)c; (void)x; (void)y; (void)size;
    if (ndrawn + 1 < sizeof(drawn)) drawn[ndrawn++] = (char)ch;
}

static unsigned char *load(const char *path, size_t *n)
{
    FILE *f = fopen(path, "rb");
    unsigned char *b;
    long len;
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); len = ftell(f); fseek(f, 0, SEEK_SET);
    b = malloc((size_t)len + 1);
    if (b && fread(b, 1, (size_t)len, f) != (size_t)len) { free(b); b = NULL; }
    fclose(f);
    *n = (size_t)len;
    return b;
}

static void render(const unsigned char *pdf, size_t n, int channels, OAPCanvas *c)
{
    static OAPPdfDoc d;
    char err[160] = "";
    c->dpi = 72; c->channels = channels; c->w = 595; c->h = 842;
    c->pix = malloc(c->w * c->h * (size_t)channels);
    memset(c->pix, 255, c->w * c->h * (size_t)channels);
    ndrawn = 0;
    CHECK(oap_pdfr_open(&d, pdf, n, err, sizeof(err)) || (printf("%s\n", err), 0));
    CHECK(oap_pdfr_render(&d, 0, c, draw, NULL, err, sizeof(err)) || (printf("%s\n", err), 0));
    drawn[ndrawn] = 0;
}

static const unsigned char *at(const OAPCanvas *c, long x, long y_pt)   /* x in points; y in points from the bottom */
{
    return c->pix + ((size_t)(842 - y_pt) * c->w + (size_t)x) * (size_t)c->channels;
}

static void test_page(void)
{
    OAPTestPage *t = malloc(sizeof(*t));
    OAPCanvas c;
    size_t n, i;
    unsigned char *pdf;
    int k;
    oap_testpage_init(t, "GFOS test page", "GFOpenPrint: colour, grey and what printed this page");
    oap_testpage_section(t, "This Amiga");
    oap_testpage_line(t, "Processor", "%s, FPU %s", "68040", "built in (68040/060)");
    oap_testpage_line(t, "Odd text", "a (b) c\\d");
    oap_testpage_section(t, "Sent");
    oap_testpage_line(t, "From address", "%s", "192.168.0.50");
    for (k = 0; k < 60; k++) oap_testpage_line(t, "More", "line %d", k);      /* past what fits: kept to OAP_TP_LINES */
    CHECK(t->n == OAP_TP_LINES);
    CHECK(oap_pdf_write_testpage("build/test-gfos-page.pdf", t));
    pdf = load("build/test-gfos-page.pdf", &n);
    CHECK(pdf != NULL);
    if (!pdf) { free(t); return; }
    CHECK(strstr((char *)pdf, "a \\(b\\) c\\\\d)") != NULL);              /* escaped in the PDF */
    CHECK(strstr((char *)pdf, "/BaseFont /Courier") != NULL);
    render(pdf, n, 3, &c);
    /* the swatches, 60 x 80 pt from x 40 every 65 pt, y 660-740 */
    for (i = 0; i < OAP_TP_SWATCHES; i++)
        CHECK(!memcmp(at(&c, 40 + (long)i * 65 + 30, 700), oap_tp_swatch_rgb[i], 3));
    CHECK(at(&c, 40 + 7 * 65, 700)[0] == 0);                                 /* white's edge */
    CHECK(at(&c, 70, 806)[0] == 219 && at(&c, 70, 806)[2] == 255);          /* the band at the top */
    /* the grey ramp: black at the left, white at the right, mid grey between */
    CHECK(at(&c, 60, 615)[0] == 0 && at(&c, 530, 615)[0] == 255 && at(&c, 40 + 5 * 47 + 20, 615)[0] == 128);
    /* the text, in order: the title first, the escaped value as it was */
    CHECK(!strncmp(drawn, "GFOSt", 5));
    CHECK(strstr(drawn, "a(b)c\\d") != NULL);
    CHECK(strstr(drawn, "FromaddressX") == NULL && strstr(drawn, "Fromaddress192.168.0.50") != NULL);
    CHECK(strstr(drawn, "Cyan") && strstr(drawn, "White"));
    free(c.pix);
    /* the same page on a grey canvas: the swatches as grey */
    render(pdf, n, 1, &c);
    CHECK(at(&c, 40 + 4 * 65 + 30, 700)[0] == 76 && at(&c, 40 + 2 * 65 + 30, 700)[0] == 226);   /* red, yellow */
    free(c.pix); free(pdf); free(t);
    remove("build/test-gfos-page.pdf");
}

static void test_version(void)
{
    char v[64];
    FILE *f = fopen("build/test-ver.bin", "wb");
    fputs("\x01\x02junk $VE $VER: GFOpenPrint.driver 0.4 (24.11.2026)\0more", f);
    fclose(f);
    CHECK(oap_file_version("build/test-ver.bin", v, sizeof(v)) && !strcmp(v, "GFOpenPrint.driver 0.4 (24.11.2026)"));
    CHECK(oap_file_version("build/test-ver.bin", v, 12) && !strcmp(v, "GFOpenPrint"));
    f = fopen("build/test-ver.bin", "wb"); fputs("no version here", f); fclose(f);
    CHECK(!oap_file_version("build/test-ver.bin", v, sizeof(v)) && !v[0]);
    CHECK(!oap_file_version("build/no-such-file", v, sizeof(v)));
    remove("build/test-ver.bin");
}

static void test_address(void)
{
    char ip[32];
    CHECK(oap_net_local_ip("127.0.0.1", 631, ip, sizeof(ip)) && !strcmp(ip, "127.0.0.1"));
    CHECK(!oap_net_local_ip("", 631, ip, sizeof(ip)) && !ip[0]);
    CHECK(!oap_net_local_ip("127.0.0.1", 631, ip, 4));
    CHECK(oap_net_host_name(ip, sizeof(ip)) && ip[0]);
}

int main(void)
{
    test_page();
    test_version();
    test_address();
    printf("test_testpage: %d checks, %d failed\n", checks, failures);
    return failures != 0;
}

/* Copyright (c) 2026 AmiCradle Team. SPDX-License-Identifier: MIT */
/* PWG Raster pages (include/oap_raster.h), written from PWG 5102.4 itself. */
#include "oap_raster.h"
#include <stdlib.h>
#include <string.h>

static void be32(unsigned char *p, unsigned long v)
{
    p[0] = (unsigned char)(v >> 24); p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);  p[3] = (unsigned char)v;
}

static void text(unsigned char *p, size_t cap, const char *s)
{
    size_t n = s ? strlen(s) : 0;
    if (n >= cap) n = cap - 1;
    if (n) memcpy(p, s, n);
}

static size_t pixel_bytes(int colorspace)
{
    return colorspace == OAP_PWG_SGRAY ? 1 : colorspace == OAP_PWG_SRGB ? 3 : 0;
}

size_t oap_pwg_row_bytes(const OAPPwgPage *p)
{
    return p ? (size_t)p->width * pixel_bytes(p->colorspace) : 0;
}

int oap_pwg_header(const OAPPwgPage *p, unsigned char h[OAP_PWG_HEADER])
{
    size_t px = p ? pixel_bytes(p->colorspace) : 0;
    if (!p || !h || !px || !p->width || !p->height || !p->dpi) return 0;
    memset(h, 0, OAP_PWG_HEADER);
    text(h + 0, 64, "PwgRaster");                 /* the MediaClass field names the format */
    be32(h + 272, p->duplex ? 1 : 0);
    be32(h + 276, p->dpi); be32(h + 280, p->dpi);  /* HWResolution */
    be32(h + 340, p->copies ? p->copies : 1);      /* NumCopies */
    be32(h + 352, p->page_w_pt); be32(h + 356, p->page_h_pt);   /* PageSize */
    be32(h + 368, p->tumble ? 1 : 0);
    be32(h + 372, p->width); be32(h + 376, p->height);
    be32(h + 384, 8);                              /* BitsPerColor */
    be32(h + 388, (unsigned long)(8 * px));        /* BitsPerPixel */
    be32(h + 392, (unsigned long)(p->width * px)); /* BytesPerLine */
    be32(h + 396, 0);                              /* ColorOrder: chunky */
    be32(h + 400, (unsigned long)p->colorspace);
    be32(h + 420, (unsigned long)px);              /* NumColors */
    be32(h + 452, p->total_pages);
    be32(h + 456, 1); be32(h + 460, 1);            /* Cross/FeedTransform: as given */
    be32(h + 484, 0);                              /* PrintQuality: the printer's default */
    text(h + 1668, 64, "");                        /* RenderingIntent: the printer's default */
    text(h + 1732, 64, p->media_name);             /* PageSizeName */
    return 1;
}

/* Apple raster's page header: bits a pixel, colour space (0 sGray, 1 sRGB), sides (1 one-sided), quality (4
 * normal), then width, height and dots an inch; the rows that follow are coded as PWG Raster's. */
int oap_urf_header(const OAPPwgPage *p, unsigned char h[OAP_URF_HEADER])
{
    size_t px = p ? pixel_bytes(p->colorspace) : 0;
    if (!p || !h || !px || !p->width || !p->height || !p->dpi) return 0;
    memset(h, 0, OAP_URF_HEADER);
    h[0] = (unsigned char)(8 * px);
    h[1] = p->colorspace == OAP_PWG_SRGB ? 1 : 0;
    h[2] = p->duplex ? (p->tumble ? 2 : 3) : 1;
    h[3] = 4;
    be32(h + 12, p->width); be32(h + 16, p->height); be32(h + 20, p->dpi);
    return 1;
}

size_t oap_pwg_row_cap(size_t row_bytes)
{
    return row_bytes * 2 + 2;
}

/* A control byte, then the pixels: 0..127 is the next pixel n+1 times; 129..255 is 257-n different pixels (2 to
 * 128). A lone pixel goes as a run of one. */
size_t oap_pwg_encode_row(const unsigned char *row, size_t row_bytes, size_t pb, unsigned char *out)
{
    size_t n, i = 0, o = 0;
    if (!row || !out || !pb || row_bytes % pb) return 0;
    n = row_bytes / pb;
    while (i < n) {
        size_t run = 1;
        while (i + run < n && run < 128 && !memcmp(row + (i + run) * pb, row + i * pb, pb)) run++;
        if (run > 1 || i + 1 == n) {
            out[o++] = (unsigned char)(run - 1);
            memcpy(out + o, row + i * pb, pb); o += pb;
            i += run;
            continue;
        }
        /* different pixels, up to the next two alike */
        run = 1;
        while (i + run < n && run < 128 &&
               !(i + run + 1 < n && !memcmp(row + (i + run) * pb, row + (i + run + 1) * pb, pb))) run++;
        if (run == 1) out[o++] = 0;
        else out[o++] = (unsigned char)(257 - run);
        memcpy(out + o, row + i * pb, run * pb); o += run * pb;
        i += run;
    }
    return o;
}

static int put(OAPPwgWriter *w, const unsigned char *b, size_t n)
{
    if (!w->bad && !w->write(w->ctx, b, n)) w->bad = 1;
    return !w->bad;
}

static int flush_held(OAPPwgWriter *w)
{
    unsigned char repeat;
    size_t n;
    if (!w->held_count) return 1;
    repeat = (unsigned char)(w->held_count - 1);
    n = oap_pwg_encode_row(w->held, w->row_bytes, w->pixel_bytes, w->coded);
    w->held_count = 0;
    return put(w, &repeat, 1) && n && put(w, w->coded, n);
}

int oap_pwg_start(OAPPwgWriter *w, OAPPwgWrite write, void *ctx)
{
    if (!w || !write) return 0;
    memset(w, 0, sizeof(*w));
    w->write = write; w->ctx = ctx;
    return put(w, (const unsigned char *)"RaS2", 4);
}

int oap_urf_start(OAPPwgWriter *w, OAPPwgWrite write, void *ctx, unsigned long pages)
{
    unsigned char h[12];
    if (!w || !write || !pages) return 0;
    memset(w, 0, sizeof(*w));
    w->write = write; w->ctx = ctx; w->urf = 1;
    memcpy(h, "UNIRAST", 8);
    be32(h + 8, pages);
    return put(w, h, sizeof(h));
}

int oap_pwg_page(OAPPwgWriter *w, const OAPPwgPage *p)
{
    unsigned char h[OAP_PWG_HEADER];
    size_t hn = w && w->urf ? OAP_URF_HEADER : OAP_PWG_HEADER;
    if (!w || w->bad || !(w->urf ? oap_urf_header(p, h) : oap_pwg_header(p, h))) return 0;
    free(w->held); free(w->coded);
    w->page = *p;
    w->pixel_bytes = pixel_bytes(p->colorspace);
    w->row_bytes = oap_pwg_row_bytes(p);
    w->held = malloc(w->row_bytes);
    w->coded = malloc(oap_pwg_row_cap(w->row_bytes));
    w->held_count = 0; w->rows_done = 0;
    if (!w->held || !w->coded) { w->bad = 1; return 0; }
    return put(w, h, hn);
}

int oap_pwg_row(OAPPwgWriter *w, const unsigned char *row)
{
    if (!w || w->bad || !w->held || !row || w->rows_done >= w->page.height) return 0;
    w->rows_done++;
    if (w->held_count && w->held_count < 256 && !memcmp(w->held, row, w->row_bytes)) {
        w->held_count++;
        return 1;
    }
    if (!flush_held(w)) return 0;
    memcpy(w->held, row, w->row_bytes);
    w->held_count = 1;
    return 1;
}

int oap_pwg_end_page(OAPPwgWriter *w)
{
    if (!w || w->bad) return 0;
    if (w->rows_done != w->page.height) { w->bad = 1; return 0; }
    return flush_held(w);
}

void oap_pwg_free(OAPPwgWriter *w)
{
    if (!w) return;
    free(w->held); free(w->coded);
    w->held = w->coded = NULL;
}

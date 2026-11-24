/* Copyright (c) 2026 AmiCradle Team. SPDX-License-Identifier: MIT */
/* GFOpenPrint's own PDFs to PWG Raster pages (GFO-254): each page drawn (oap_pdfr_render), then written row
 * by row (oap_pwg_row). An upright page is drawn in bands of 8 MB, so 600 dpi colour (100 MB whole) fits a small
 * Amiga; a turned page is drawn whole. PWG Raster or Apple raster (image/urf): the same rows, other headers. */
#include "oap_raster.h"
#include <stdlib.h>
#include <string.h>

#define OAP_RASTER_MAX_BYTES (128UL * 1024 * 1024)   /* one page's pixels, at most (600 dpi colour A4 fits) */
#define OAP_RASTER_BAND_BYTES (8UL * 1024 * 1024)   /* drawn at a time */

static unsigned long fail(char *err, size_t cap, const char *why)
{
    if (err && cap) { strncpy(err, why, cap - 1); err[cap - 1] = 0; }
    return 0;
}

static unsigned long px(unsigned long pt, unsigned long dpi) { return (pt * dpi + 36) / 72; }

unsigned long oap_pdf_to_pwg(const unsigned char *pdf, size_t len, const OAPRasterJob *job,
                             OAPPwgWrite write, void *ctx, char *err, size_t cap)
{
    OAPPdfDoc *d;
    OAPPwgWriter w;
    OAPCanvas c;
    unsigned char *row = NULL;
    unsigned long first, last, page, done = 0;
    int ch;
    if (!pdf || !job || !write || job->dpi < 72 || job->dpi > 1200) return fail(err, cap, "No raster settings");
    ch = job->rgb ? 3 : 1;
    d = malloc(sizeof(*d));
    if (!d) return fail(err, cap, "Not enough memory to read the PDF");
    if (!oap_pdfr_open(d, pdf, len, err, cap)) { free(d); return 0; }
    first = job->first > 0 ? (unsigned long)job->first : 1;
    last = job->last > 0 ? (unsigned long)job->last : d->pages;
    if (last > d->pages) last = d->pages;
    if (first > last) { free(d); return fail(err, cap, "None of the pages asked for are in the document"); }
    memset(&c, 0, sizeof(c));
    if (!(job->urf ? oap_urf_start(&w, write, ctx, last - first + 1) : oap_pwg_start(&w, write, ctx))) {
        free(d); return fail(err, cap, "Cannot write the raster pages");
    }
    for (page = first; page <= last; page++) {
        unsigned long w_pt, h_pt, y, full_w, full_h, band, top;
        OAPPwgPage pg;
        int turn;
        if (!oap_pdfr_page_size(d, page - 1, &w_pt, &h_pt)) { fail(err, cap, "This PDF's page has no paper size"); goto out; }
        full_w = px(w_pt, job->dpi); full_h = px(h_pt, job->dpi);
        turn = full_w > full_h;
        if (!full_w || !full_h || full_w > OAP_RASTER_MAX_BYTES / full_h / (unsigned long)ch) {
            fail(err, cap, "This page is too big to print as raster"); goto out;
        }
        /* an upright page is drawn a band of rows at a time (600 dpi colour A4 is 100 MB whole); a turned page
         * is read by columns, so it is drawn whole */
        band = full_h;
        if (!turn) {
            unsigned long bytes = job->band_bytes ? job->band_bytes : OAP_RASTER_BAND_BYTES;
            band = bytes / (full_w * (unsigned long)ch);
            if (band < 1) band = 1;
            if (band > full_h) band = full_h;
        }
        c.w = full_w; c.h = band; c.channels = ch; c.dpi = job->dpi; c.y0 = 0;
        c.pix = malloc((size_t)c.w * c.h * ch);
        if (!c.pix) { fail(err, cap, "Not enough memory for a raster page"); goto out; }
        memset(&pg, 0, sizeof(pg));
        pg.width = turn ? full_h : full_w; pg.height = turn ? full_w : full_h; pg.dpi = job->dpi;
        pg.colorspace = job->rgb ? OAP_PWG_SRGB : OAP_PWG_SGRAY;
        pg.page_w_pt = turn ? h_pt : w_pt; pg.page_h_pt = turn ? w_pt : h_pt;
        pg.copies = 1; pg.total_pages = last - first + 1; pg.media_name = job->media_name;
        if (turn) {
            row = malloc((size_t)pg.width * ch);
            if (!row) { fail(err, cap, "Not enough memory for a raster page"); goto out; }
            memset(c.pix, 255, (size_t)c.w * c.h * ch);
            if (!oap_pdfr_render(d, page - 1, &c, job->draw, job->draw_ctx, err, cap)) goto out;
            if (!oap_pwg_page(&w, &pg)) { fail(err, cap, "Cannot write the raster pages"); goto out; }
            for (y = 0; y < pg.height; y++) {
                /* paper row y is the page's column w-1-y, read from the page's top down */
                unsigned long x, col = c.w - 1 - y;
                for (x = 0; x < pg.width; x++)
                    memcpy(row + (size_t)x * ch, c.pix + ((size_t)x * c.w + col) * ch, (size_t)ch);
                if (!oap_pwg_row(&w, row)) { fail(err, cap, "Cannot write the raster pages"); goto out; }
            }
        } else {
            for (top = 0; top < full_h; top += band) {
                c.y0 = top; c.h = full_h - top < band ? full_h - top : band;
                memset(c.pix, 255, (size_t)c.w * c.h * ch);
                if (!oap_pdfr_render(d, page - 1, &c, job->draw, job->draw_ctx, err, cap)) goto out;
                if (!top && !oap_pwg_page(&w, &pg)) { fail(err, cap, "Cannot write the raster pages"); goto out; }
                for (y = 0; y < c.h; y++)
                    if (!oap_pwg_row(&w, c.pix + (size_t)y * c.w * ch)) { fail(err, cap, "Cannot write the raster pages"); goto out; }
            }
        }
        if (!oap_pwg_end_page(&w)) { fail(err, cap, "Cannot write the raster pages"); goto out; }
        free(row); row = NULL;
        free(c.pix); c.pix = NULL;
        done++;
    }
out:
    free(row);
    free(c.pix);
    oap_pwg_free(&w);
    free(d);
    return page > last ? done : 0;
}

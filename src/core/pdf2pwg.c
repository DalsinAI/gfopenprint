/* Copyright (c) 2026 AmiCradle Team. SPDX-License-Identifier: MIT */
/* GFOpenPrint's own PDFs to PWG Raster pages (GFO-254): each page drawn whole (oap_pdfr_render), then written row
 * by row (oap_pwg_row). An A4 page at 300 dpi is 8.7 MB grey, 26 MB colour. */
#include "oap_raster.h"
#include <stdlib.h>
#include <string.h>

#define OAP_RASTER_MAX_BYTES (96UL * 1024 * 1024)    /* one page's pixels, at most */

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
    if (!oap_pwg_start(&w, write, ctx)) { free(d); return fail(err, cap, "Cannot write the raster pages"); }
    for (page = first; page <= last; page++) {
        unsigned long w_pt, h_pt, y;
        OAPPwgPage pg;
        int turn;
        if (!oap_pdfr_page_size(d, page - 1, &w_pt, &h_pt)) { fail(err, cap, "This PDF's page has no paper size"); goto out; }
        c.w = px(w_pt, job->dpi); c.h = px(h_pt, job->dpi); c.channels = ch; c.dpi = job->dpi;
        if (!c.w || !c.h || c.w > OAP_RASTER_MAX_BYTES / c.h / (unsigned long)ch) {
            fail(err, cap, "This page is too big to print as raster"); goto out;
        }
        c.pix = malloc((size_t)c.w * c.h * ch);
        if (!c.pix) { fail(err, cap, "Not enough memory for a raster page"); goto out; }
        memset(c.pix, 255, (size_t)c.w * c.h * ch);
        if (!oap_pdfr_render(d, page - 1, &c, job->draw, job->draw_ctx, err, cap)) goto out;
        turn = c.w > c.h;
        memset(&pg, 0, sizeof(pg));
        pg.width = turn ? c.h : c.w; pg.height = turn ? c.w : c.h; pg.dpi = job->dpi;
        pg.colorspace = job->rgb ? OAP_PWG_SRGB : OAP_PWG_SGRAY;
        pg.page_w_pt = turn ? h_pt : w_pt; pg.page_h_pt = turn ? w_pt : h_pt;
        pg.copies = 1; pg.total_pages = last - first + 1; pg.media_name = job->media_name;
        if (!oap_pwg_page(&w, &pg)) { fail(err, cap, "Cannot write the raster pages"); goto out; }
        if (turn) {
            row = malloc((size_t)pg.width * ch);
            if (!row) { fail(err, cap, "Not enough memory for a raster page"); goto out; }
        }
        for (y = 0; y < pg.height; y++) {
            const unsigned char *r;
            if (!turn) r = c.pix + (size_t)y * c.w * ch;
            else {
                /* paper row y is the page's column w-1-y, read from the page's top down */
                unsigned long x, col = c.w - 1 - y;
                for (x = 0; x < pg.width; x++)
                    memcpy(row + (size_t)x * ch, c.pix + ((size_t)x * c.w + col) * ch, (size_t)ch);
                r = row;
            }
            if (!oap_pwg_row(&w, r)) { fail(err, cap, "Cannot write the raster pages"); goto out; }
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

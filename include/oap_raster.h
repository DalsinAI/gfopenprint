/* Copyright (c) 2026 AmiCradle Team. SPDX-License-Identifier: MIT */
/* PWG Raster (PWG 5102.4, image/pwg-raster): the page format IPP Everywhere printers take when they take no PDF.
 * A stream is "RaS2", then for each page a 1796-byte header and its rows, run-length coded. Our own code: no
 * CUPS or libcups source was used. */
#ifndef OAP_RASTER_H
#define OAP_RASTER_H
#include <stddef.h>
#include <stdio.h>
#include "oap_pdfr.h"

#define OAP_FORMAT_PWG "image/pwg-raster"
#define OAP_PWG_HEADER 1796

/* PWG colour spaces (PWG 5102.4 table 2): 8-bit grey and 8-bit sRGB are the two every IPP Everywhere raster
 * printer takes (pwg-raster-document-type-supported sgray_8, srgb_8). */
enum { OAP_PWG_SGRAY = 18, OAP_PWG_SRGB = 19 };

typedef struct OAPPwgPage {
    unsigned long width, height;        /* pixels */
    unsigned long dpi;                  /* the same across and down */
    int colorspace;                     /* OAP_PWG_SGRAY (1 byte a pixel) or OAP_PWG_SRGB (3) */
    unsigned long page_w_pt, page_h_pt; /* the paper, in points */
    int duplex, tumble;                 /* two-sided; turned on the short edge */
    unsigned long copies;
    unsigned long total_pages;          /* 0: not known */
    const char *media_name;             /* PWG media name, e.g. "iso_a4_210x297mm" (or NULL) */
} OAPPwgPage;

/* Bytes in one row of the page: width x (1 or 3). 0 for a colour space it doesn't know. */
size_t oap_pwg_row_bytes(const OAPPwgPage *p);
/* The 1796-byte page header, big-endian, into out. 1 on success. */
int oap_pwg_header(const OAPPwgPage *p, unsigned char out[OAP_PWG_HEADER]);
/* Run-length code one row (PWG 5102.4 section 4.3.1.2): out gets at most oap_pwg_row_cap(row bytes) bytes;
 * returns how many it wrote. The line-repeat byte is not part of it. */
size_t oap_pwg_row_cap(size_t row_bytes);
size_t oap_pwg_encode_row(const unsigned char *row, size_t row_bytes, size_t pixel_bytes,
                          unsigned char *out);

/* Writing a stream: start once ("RaS2"), then for each page a header and every row in order. Identical rows
 * next to each other are written once with a repeat count (up to 256). */
typedef int (*OAPPwgWrite)(void *ctx, const unsigned char *bytes, size_t n);
typedef struct OAPPwgWriter {
    OAPPwgWrite write; void *ctx;
    OAPPwgPage page; size_t row_bytes, pixel_bytes;
    unsigned char *held, *coded;        /* the row waiting for more repeats, and its coding */
    unsigned long held_count, rows_done;
    int bad;
} OAPPwgWriter;
int oap_pwg_start(OAPPwgWriter *w, OAPPwgWrite write, void *ctx);
int oap_pwg_page(OAPPwgWriter *w, const OAPPwgPage *p);
int oap_pwg_row(OAPPwgWriter *w, const unsigned char *row);
/* Ends the page: every row given (height of them). 1 when all was written. */
int oap_pwg_end_page(OAPPwgWriter *w);
void oap_pwg_free(OAPPwgWriter *w);

/* GFOpenPrint's own PDF (oap_pdfr.h) as a PWG Raster stream, for a printer that takes no PDF. A page wider than
 * tall is turned a quarter (its top to the paper's left) to stand on upright paper, as a printer does with a
 * landscape PDF. Pages first..last (1 first; 0, 0 for all). copies, sides and media go as IPP job attributes. */
typedef struct OAPRasterJob {
    unsigned long dpi;                  /* from pwg-raster-document-resolution-supported */
    int rgb;                            /* srgb_8, else sgray_8 */
    int first, last;
    const char *media_name;             /* for the page headers, e.g. "iso_a4_210x297mm" (or NULL) */
    OAPDrawChar draw; void *draw_ctx;   /* the text font: NULL refuses pages with text */
} OAPRasterJob;
/* Pages written, or 0 with words in err. */
unsigned long oap_pdf_to_pwg(const unsigned char *pdf, size_t len, const OAPRasterJob *job,
                             OAPPwgWrite write, void *ctx, char *err, size_t cap);
/* The text font for raster pages: src/amiga/raster_font.c on the Amiga (diskfont), a stand-in in tests. NULL when
 * there is none; close lets it go. */
OAPDrawChar oap_raster_font(void **ctx);
void oap_raster_font_close(void);
#endif

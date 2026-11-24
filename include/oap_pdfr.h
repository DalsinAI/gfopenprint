/* Copyright (c) 2026 AmiCradle Team. SPDX-License-Identifier: MIT */
/* Pages of GFOpenPrint's own PDFs as pixels, for printers that take raster and no PDF (GFO-254).
 *
 * Only what GFOpenPrint itself writes is read: the printer driver's PDFs (src/driver/openprint_driver.c: Courier
 * text, BT/Tf/Td/Tj, and pictures as inline RGB images, /ASCII85Decode + /RunLengthDecode, placed with cm). Any
 * other PDF (fonts of its own, vector drawing, image XObjects) is refused with words saying so: never printed
 * wrong. Our own code, from the PDF 1.4 reference. */
#ifndef OAP_PDFR_H
#define OAP_PDFR_H
#include <stddef.h>

typedef struct OAPCanvas {
    unsigned char *pix;         /* rows top first, channels bytes a pixel, 255 white */
    unsigned long w, h;
    int channels;               /* 1 grey, 3 RGB */
    unsigned long dpi;
} OAPCanvas;

/* Draws one character, Latin-1, of the text font (Courier's place: 0.6 of size_px wide) with its left edge at x
 * and its baseline at y, canvas pixels. The caller's font: graphics.library on the Amiga, a stand-in in tests. */
typedef void (*OAPDrawChar)(void *ctx, OAPCanvas *c, long x, long y, unsigned long size_px, unsigned char ch);

#define OAP_PDFR_MAX_PAGES 4096
typedef struct OAPPdfDoc {
    const unsigned char *pdf; size_t len;
    unsigned long pages;
    size_t page_obj[OAP_PDFR_MAX_PAGES];    /* each page's object, by its offset */
} OAPPdfDoc;

/* The PDF's pages. 1 when it is one GFOpenPrint can draw; 0 with words in err when not. */
int oap_pdfr_open(OAPPdfDoc *d, const unsigned char *pdf, size_t len, char *err, size_t cap);
/* A page's paper, in points. */
int oap_pdfr_page_size(const OAPPdfDoc *d, unsigned long page, unsigned long *w_pt, unsigned long *h_pt);
/* Draws page (0 first) onto c, which the caller has made white and sized to the page at c->dpi. A page with text
 * and no draw is refused. */
int oap_pdfr_render(const OAPPdfDoc *d, unsigned long page, OAPCanvas *c,
                    OAPDrawChar draw, void *ctx, char *err, size_t cap);
#endif

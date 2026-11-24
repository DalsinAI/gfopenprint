/* Copyright (c) 2026 AmiCradle Team. SPDX-License-Identifier: MIT */
/* The GFOS test page (Printers and Queue's "Print a test page", OAPIPPSTest testpage): colour bars and a grey
 * ramp, then what printed it: the Amiga, its drivers, the printer and the address it was sent from. A PDF of
 * GFOpenPrint's own kind (Courier text, filled boxes), so raster printers get it too (oap_pdfr.h). */
#ifndef OAP_TESTPAGE_H
#define OAP_TESTPAGE_H
#include <stddef.h>

#define OAP_TP_LINES 48
#define OAP_TP_TEXT 80
typedef struct OAPTestPage {
    char title[OAP_TP_TEXT], subtitle[OAP_TP_TEXT];
    int n;
    char label[OAP_TP_LINES][24];       /* "" with a value: a section's heading */
    char value[OAP_TP_LINES][OAP_TP_TEXT];
} OAPTestPage;

void oap_testpage_init(OAPTestPage *t, const char *title, const char *subtitle);
void oap_testpage_section(OAPTestPage *t, const char *heading);
/* A label and its value, printf style; cut to fit */
void oap_testpage_line(OAPTestPage *t, const char *label, const char *fmt, ...)
#ifdef __GNUC__
    __attribute__((format(printf, 3, 4)))
#endif
    ;
/* The A4 page as a PDF at path. 1 on success. */
int oap_pdf_write_testpage(const char *path, const OAPTestPage *t);

/* A program's or driver's version string from its file: what follows "$VER: " up to the end of the line, or 0
 * when it has none (or no such file). */
int oap_file_version(const char *path, char *out, size_t cap);

/* The test page's swatches, left to right: their names and colours (0-255), for the page and its tests */
#define OAP_TP_SWATCHES 8
extern const char *const oap_tp_swatch_name[OAP_TP_SWATCHES];
extern const unsigned char oap_tp_swatch_rgb[OAP_TP_SWATCHES][3];
#endif

/* Copyright (c) 2026 AmiCradle Team. SPDX-License-Identifier: MIT */
/* The GFOS test page (include/oap_testpage.h). */
#include "oap_testpage.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *const oap_tp_swatch_name[OAP_TP_SWATCHES] = {"Cyan", "Magenta", "Yellow", "Black", "Red", "Green", "Blue", "White"};
const unsigned char oap_tp_swatch_rgb[OAP_TP_SWATCHES][3] = {
    {0, 255, 255}, {255, 0, 255}, {255, 255, 0}, {0, 0, 0}, {255, 0, 0}, {0, 255, 0}, {0, 0, 255}, {255, 255, 255}};

static void copy(char *d, size_t cap, const char *s)
{
    size_t n = s ? strlen(s) : 0;
    if (n >= cap) n = cap - 1;
    if (n) memcpy(d, s, n);
    d[n] = 0;
}

void oap_testpage_init(OAPTestPage *t, const char *title, const char *subtitle)
{
    memset(t, 0, sizeof(*t));
    copy(t->title, sizeof(t->title), title);
    copy(t->subtitle, sizeof(t->subtitle), subtitle);
}

void oap_testpage_section(OAPTestPage *t, const char *heading)
{
    if (t->n >= OAP_TP_LINES) return;
    t->label[t->n][0] = 0;
    copy(t->value[t->n], sizeof(t->value[t->n]), heading);
    t->n++;
}

void oap_testpage_line(OAPTestPage *t, const char *label, const char *fmt, ...)
{
    va_list ap;
    if (t->n >= OAP_TP_LINES) return;
    copy(t->label[t->n], sizeof(t->label[t->n]), label && label[0] ? label : " ");
    va_start(ap, fmt);
    vsnprintf(t->value[t->n], sizeof(t->value[t->n]), fmt, ap);
    va_end(ap);
    t->n++;
}

/* A PDF string's bytes: ( ) and \ escaped, control bytes as spaces */
static size_t pdf_text(char *out, size_t cap, const char *s)
{
    size_t n = 0;
    for (; *s && n + 3 < cap; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '(' || c == ')' || c == '\\') out[n++] = '\\';
        out[n++] = c < 32 ? ' ' : (char)c;
    }
    out[n] = 0;
    return n;
}

typedef struct Buf { char *b; size_t n, cap; int bad; } Buf;
static void put(Buf *b, const char *fmt, ...)
{
    va_list ap;
    int k;
    if (b->bad) return;
    va_start(ap, fmt);
    k = vsnprintf(b->b + b->n, b->cap - b->n, fmt, ap);
    va_end(ap);
    if (k < 0 || (size_t)k >= b->cap - b->n) { b->bad = 1; return; }
    b->n += (size_t)k;
}

static void text(Buf *b, double size, double x, double y, const char *s)
{
    char esc[2 * OAP_TP_TEXT + 64];
    pdf_text(esc, sizeof(esc), s);
    put(b, "0 g BT /F1 %g Tf %g %g Td (%s) Tj ET\n", size, x, y, esc);   /* text in black, as raster pages draw it */
}

static double c255(unsigned char v) { return v / 255.0; }

int oap_pdf_write_testpage(const char *path, const OAPTestPage *t)
{
    Buf b;
    FILE *f;
    long off[6], xref;
    int i, ok;
    double y;
    if (!path || !t) return 0;
    b.cap = 32768; b.n = 0; b.bad = 0;
    b.b = malloc(b.cap);
    if (!b.b) return 0;
    /* the band at the top, with the title */
    put(&b, "0.86 0.91 1 rg 0 772 595 70 re f 0.11 0.27 0.53 rg 0 768 595 4 re f\n");
    text(&b, 24, 40, 800, t->title);
    text(&b, 11, 40, 782, t->subtitle);
    /* colour swatches, each named under it; white has an edge to show it */
    for (i = 0; i < OAP_TP_SWATCHES; i++) {
        const unsigned char *c = oap_tp_swatch_rgb[i];
        double x = 40 + i * 65;
        put(&b, "%.3f %.3f %.3f rg %g 660 60 80 re f\n", c255(c[0]), c255(c[1]), c255(c[2]), x);
        if (c[0] == 255 && c[1] == 255 && c[2] == 255) put(&b, "0 G 0.5 w %g 660 60 80 re S\n", x);
        text(&b, 8, x, 648, oap_tp_swatch_name[i]);
    }
    /* the grey ramp: black to white in eleven steps */
    for (i = 0; i <= 10; i++) put(&b, "%.1f g %g 600 46 30 re f\n", i / 10.0, 40 + i * 46.8);
    put(&b, "0 G 0.5 w 40 600 515 30 re S\n");
    text(&b, 8, 40, 588, "black");
    text(&b, 8, 285, 588, "grey");
    text(&b, 8, 531, 588, "white");
    /* what printed it */
    y = 560;
    for (i = 0; i < t->n && y > 40; i++) {
        if (!t->label[i][0]) {
            if (i) y -= 6;
            put(&b, "0.11 0.27 0.53 rg 40 %g 515 1 re f 0 g\n", y - 3);
            text(&b, 12, 40, y, t->value[i]);
            y -= 17;
        } else {
            char line[24 + OAP_TP_TEXT + 4];
            snprintf(line, sizeof(line), "%-18s %s", t->label[i], t->value[i]);
            text(&b, 10, 40, y, line);
            y -= 12.5;
        }
    }
    put(&b, "0 g\n");
    if (b.bad) { free(b.b); return 0; }
    f = fopen(path, "wb");
    if (!f) { free(b.b); return 0; }
    fprintf(f, "%%PDF-1.4\n%%OAP1\n");
    off[1] = ftell(f); fprintf(f, "1 0 obj << /Type /Catalog /Pages 2 0 R >> endobj\n");
    off[2] = ftell(f); fprintf(f, "2 0 obj << /Type /Pages /Kids [3 0 R] /Count 1 >> endobj\n");
    off[3] = ftell(f); fprintf(f, "3 0 obj << /Type /Page /Parent 2 0 R /MediaBox [0 0 595 842] /Resources << /Font << /F1 5 0 R >> >> /Contents 4 0 R >> endobj\n");
    off[4] = ftell(f); fprintf(f, "4 0 obj << /Length %lu >> stream\n", (unsigned long)b.n);
    fwrite(b.b, 1, b.n, f); fprintf(f, "endstream\nendobj\n");
    off[5] = ftell(f); fprintf(f, "5 0 obj << /Type /Font /Subtype /Type1 /BaseFont /Courier /Encoding /WinAnsiEncoding >> endobj\n");
    xref = ftell(f); fprintf(f, "xref\n0 6\n0000000000 65535 f \n");
    for (i = 1; i <= 5; i++) fprintf(f, "%010ld 00000 n \n", off[i]);
    fprintf(f, "trailer << /Size 6 /Root 1 0 R >>\nstartxref\n%ld\n%%%%EOF\n", xref);
    ok = !ferror(f);
    if (fclose(f)) ok = 0;
    free(b.b);
    return ok;
}

int oap_file_version(const char *path, char *out, size_t cap)
{
    FILE *f;
    static const char tag[] = "$VER: ";
    unsigned char buf[4096];
    size_t keep = 0, n, i, matched = 0;
    int found = 0;
    if (!out || !cap) return 0;
    out[0] = 0;
    if (!path || !(f = fopen(path, "rb"))) return 0;
    while (!found && (n = fread(buf, 1, sizeof(buf), f)) > 0) {
        for (i = 0; i < n; i++) {
            if (matched < sizeof(tag) - 1) {
                matched = buf[i] == (unsigned char)tag[matched] ? matched + 1 : buf[i] == '$' ? 1 : 0;
                continue;
            }
            if (!buf[i] || buf[i] == '\n' || buf[i] == '\r' || keep + 1 >= cap) { found = 1; break; }
            out[keep++] = (char)buf[i];
        }
    }
    fclose(f);
    out[keep] = 0;
    return keep > 0;
}

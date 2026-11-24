/* Copyright (c) 2026 AmiCradle Team. SPDX-License-Identifier: MIT */
/* GFOpenPrint's own PDFs as page pixels (include/oap_pdfr.h). */
#include "oap_pdfr.h"
#include "oap_inflate.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(char *err, size_t cap, const char *why)
{
    if (err && cap) snprintf(err, cap, "%s", why);
    return 0;
}

static int is_space(int c) { return c == ' ' || c == '\n' || c == '\r' || c == '\t' || c == '\f' || c == 0; }
static int is_delim(int c) { return c == '(' || c == ')' || c == '<' || c == '>' || c == '[' || c == ']' ||
                                    c == '{' || c == '}' || c == '/' || c == '%'; }

/* ---- objects, through the xref table ------------------------------------------------------------------ */

static const unsigned char *find(const unsigned char *p, const unsigned char *end, const char *s)
{
    size_t n = strlen(s);
    while (p + n <= end) {
        if (*p == (unsigned char)s[0] && !memcmp(p, s, n)) return p;
        p++;
    }
    return NULL;
}

static int read_ulong(const unsigned char **pp, const unsigned char *end, unsigned long *v)
{
    const unsigned char *p = *pp;
    unsigned long x = 0;
    int any = 0;
    while (p < end && is_space(*p)) p++;
    while (p < end && *p >= '0' && *p <= '9') { x = x * 10 + (unsigned long)(*p - '0'); p++; any = 1; }
    if (!any) return 0;
    *v = x; *pp = p;
    return 1;
}

typedef struct Xref { const unsigned char *pdf; size_t len; const unsigned char *table; unsigned long first, count; } Xref;

static int xref_open(Xref *x, const unsigned char *pdf, size_t len)
{
    const unsigned char *end = pdf + len, *p, *last = NULL;
    unsigned long off;
    memset(x, 0, sizeof(*x));
    x->pdf = pdf; x->len = len;
    for (p = pdf; (p = find(p, end, "startxref")) != NULL; p++) last = p;
    if (!last) return 0;
    p = last + 9;
    if (!read_ulong(&p, end, &off) || off + 4 > len || memcmp(pdf + off, "xref", 4)) return 0;
    p = pdf + off + 4;
    if (!read_ulong(&p, end, &x->first) || !read_ulong(&p, end, &x->count)) return 0;
    while (p < end && is_space(*p)) p++;
    if ((size_t)(end - p) < x->count * 20) return 0;
    x->table = p;
    return 1;
}

/* Where object num starts (after "num 0 obj"), or NULL. */
static const unsigned char *object(const Xref *x, unsigned long num, const unsigned char **obj_end)
{
    const unsigned char *e, *p, *end = x->pdf + x->len;
    unsigned long off, n;
    char head[32];
    if (num < x->first || num >= x->first + x->count) return NULL;
    e = x->table + (num - x->first) * 20;
    p = e;
    if (!read_ulong(&p, e + 20, &off) || e[17] != 'n' || off >= x->len) return NULL;
    p = x->pdf + off;
    if (!read_ulong(&p, end, &n) || n != num) return NULL;
    snprintf(head, sizeof(head), " 0 obj");
    if ((size_t)(end - p) < 6 || memcmp(p, head, 6)) return NULL;
    p += 6;
    e = find(p, end, "endobj");
    if (!e) return NULL;
    if (obj_end) *obj_end = e;
    return p;
}

/* key's value in a dictionary between p and end: a pointer past the key. */
static const unsigned char *key(const unsigned char *p, const unsigned char *end, const char *k)
{
    size_t n = strlen(k);
    const unsigned char *q = p;
    while ((q = find(q, end, k)) != NULL) {
        if (q + n < end && (is_space(q[n]) || is_delim(q[n]))) return q + n;
        q += n;
    }
    return NULL;
}

static int ref(const unsigned char *p, const unsigned char *end, unsigned long *num)
{
    unsigned long gen;
    if (!p || !read_ulong(&p, end, num) || !read_ulong(&p, end, &gen)) return 0;
    while (p < end && is_space(*p)) p++;
    return p < end && *p == 'R';
}

int oap_pdfr_open(OAPPdfDoc *d, const unsigned char *pdf, size_t len, char *err, size_t cap)
{
    Xref x;
    const unsigned char *t, *o, *oe, *k;
    unsigned long root, pages, kid;
    if (!d || !pdf) return fail(err, cap, "No PDF");
    memset(d, 0, sizeof(*d));
    if (len < 8 || memcmp(pdf, "%PDF-", 5)) return fail(err, cap, "Not a PDF");
    if (!xref_open(&x, pdf, len)) return fail(err, cap, "This PDF has no table of its parts (xref) that GFOpenPrint can read");
    t = find(x.table, pdf + len, "trailer");
    if (!t || !ref(key(t, pdf + len, "/Root"), pdf + len, &root)) return fail(err, cap, "This PDF names no catalog");
    o = object(&x, root, &oe);
    if (!o || !ref(key(o, oe, "/Pages"), oe, &pages)) return fail(err, cap, "This PDF's catalog names no pages");
    o = object(&x, pages, &oe);
    k = o ? key(o, oe, "/Kids") : NULL;
    while (k && k < oe && is_space(*k)) k++;
    if (!k || *k != '[') return fail(err, cap, "This PDF lists no pages");
    k++;
    while (k < oe) {
        const unsigned char *po, *pe;
        while (k < oe && is_space(*k)) k++;
        if (k < oe && *k == ']') break;
        if (!ref(k, oe, &kid)) return fail(err, cap, "This PDF's page list is not one GFOpenPrint wrote");
        while (k < oe && *k != 'R') k++;
        k++;
        po = object(&x, kid, &pe);
        if (!po || !find(po, pe, "/Type /Page")) return fail(err, cap, "This PDF has pages within pages: only GFOpenPrint's own PDFs go to this printer");
        if (d->pages >= OAP_PDFR_MAX_PAGES) return fail(err, cap, "Too many pages");
        d->page_obj[d->pages++] = (size_t)(po - pdf);
    }
    if (!d->pages) return fail(err, cap, "This PDF has no pages");
    d->pdf = pdf; d->len = len;
    return 1;
}

static int page_dict(const OAPPdfDoc *d, unsigned long page, const unsigned char **p, const unsigned char **end)
{
    if (!d || page >= d->pages) return 0;
    *p = d->pdf + d->page_obj[page];
    *end = find(*p, d->pdf + d->len, "endobj");
    return *end != NULL;
}

/* ---- the content stream ------------------------------------------------------------------------------- */

typedef struct Lex { const unsigned char *p, *end; } Lex;
enum { T_END, T_NUM, T_NAME, T_STR, T_OP, T_OPEN, T_CLOSE, T_BAD };
typedef struct Tok { int type; double num; const unsigned char *s; size_t n; } Tok;

static void skip(Lex *l)
{
    for (;;) {
        while (l->p < l->end && is_space(*l->p)) l->p++;
        if (l->p < l->end && *l->p == '%') { while (l->p < l->end && *l->p != '\n' && *l->p != '\r') l->p++; continue; }
        return;
    }
}

static int next(Lex *l, Tok *t)
{
    const unsigned char *s;
    skip(l);
    memset(t, 0, sizeof(*t));
    if (l->p >= l->end) return t->type = T_END;
    s = l->p;
    if (*s == '[') { l->p++; return t->type = T_OPEN; }
    if (*s == ']') { l->p++; return t->type = T_CLOSE; }
    if (*s == '/') {
        l->p++;
        while (l->p < l->end && !is_space(*l->p) && !is_delim(*l->p)) l->p++;
        t->s = s + 1; t->n = (size_t)(l->p - s - 1);
        return t->type = T_NAME;
    }
    if (*s == '(') {
        int depth = 1;
        l->p++;
        t->s = l->p;
        while (l->p < l->end && depth) {
            if (*l->p == '\\') { l->p += 2; continue; }
            if (*l->p == '(') depth++;
            else if (*l->p == ')') depth--;
            l->p++;
        }
        if (depth) return t->type = T_BAD;
        t->n = (size_t)(l->p - 1 - t->s);
        return t->type = T_STR;
    }
    if (*s == '-' || *s == '+' || *s == '.' || (*s >= '0' && *s <= '9')) {
        double v = 0, f = 0.1;
        int neg = 0, frac = 0, any = 0;
        if (*l->p == '-' || *l->p == '+') { neg = *l->p == '-'; l->p++; }
        while (l->p < l->end && ((*l->p >= '0' && *l->p <= '9') || (*l->p == '.' && !frac))) {
            if (*l->p == '.') frac = 1;
            else if (!frac) { v = v * 10 + (*l->p - '0'); any = 1; }
            else { v += (*l->p - '0') * f; f /= 10; any = 1; }
            l->p++;
        }
        if (!any) return t->type = T_BAD;
        t->num = neg ? -v : v;
        return t->type = T_NUM;
    }
    if (is_delim(*s)) return t->type = T_BAD;
    while (l->p < l->end && !is_space(*l->p) && !is_delim(*l->p)) l->p++;
    t->s = s; t->n = (size_t)(l->p - s);
    return t->type = T_OP;
}

static int op_is(const Tok *t, const char *s) { return t->type == T_OP && t->n == strlen(s) && !memcmp(t->s, s, t->n); }
static int name_is(const Tok *t, const char *s) { return t->type == T_NAME && t->n == strlen(s) && !memcmp(t->s, s, t->n); }

/* A page's MediaBox, in points (GFOpenView's carry hundredths). */
static int media(const OAPPdfDoc *d, unsigned long page, double *w, double *h)
{
    const unsigned char *p, *e, *m;
    Lex l;
    Tok t[4];
    int i;
    if (!page_dict(d, page, &p, &e) || !(m = key(p, e, "/MediaBox"))) return 0;
    l.p = m; l.end = e;
    if (next(&l, &t[0]) != T_OPEN) return 0;
    for (i = 0; i < 4; i++) if (next(&l, &t[i]) != T_NUM) return 0;
    if (t[0].num != 0 || t[1].num != 0 || t[2].num < 1 || t[3].num < 1 || t[2].num > 14400 || t[3].num > 14400) return 0;
    *w = t[2].num; *h = t[3].num;
    return 1;
}

int oap_pdfr_page_size(const OAPPdfDoc *d, unsigned long page, unsigned long *w_pt, unsigned long *h_pt)
{
    double w, h;
    if (!media(d, page, &w, &h)) return 0;
    *w_pt = (unsigned long)(w + 0.5); *h_pt = (unsigned long)(h + 0.5);
    return 1;
}

typedef struct Matrix { double a, b, c, d, e, f; } Matrix;
static Matrix mul(Matrix m, Matrix n)    /* m then n */
{
    Matrix r;
    r.a = m.a * n.a + m.b * n.c; r.b = m.a * n.b + m.b * n.d;
    r.c = m.c * n.a + m.d * n.c; r.d = m.c * n.b + m.d * n.d;
    r.e = m.e * n.a + m.f * n.c + n.e; r.f = m.e * n.b + m.f * n.d + n.f;
    return r;
}

typedef struct Render {
    OAPCanvas *c; double scale; double page_h;   /* points to pixels, and the page's height in points */
    OAPDrawChar draw; void *ctx;
    long clip_x0, clip_y0, clip_x1, clip_y1;     /* canvas pixels a picture may cover (re W n) */
} Render;

static long px_x(const Render *r, double x) { return (long)(x * r->scale + 0.5); }
static long px_y(const Render *r, double y) { return (long)((r->page_h - y) * r->scale + 0.5); }

/* A string's bytes, escapes undone, one character at a time. */
static void show(const Render *r, const Tok *s, Matrix m, double size)
{
    const unsigned char *p = s->s, *end = s->s + s->n;
    double sx = size * m.a, x = m.e, y = m.f;
    long size_px = (long)(sx * r->scale + 0.5);
    unsigned long i = 0;
    if (size_px < 1) size_px = 1;
    while (p < end) {
        unsigned char ch = *p++;
        if (ch == '\\' && p < end) {
            unsigned char e = *p++;
            if (e >= '0' && e <= '7') {
                int v = e - '0', k;
                for (k = 0; k < 2 && p < end && *p >= '0' && *p <= '7'; k++) v = v * 8 + (*p++ - '0');
                ch = (unsigned char)v;
            } else if (e == 'n') ch = '\n';
            else if (e == 'r') ch = '\r';
            else if (e == 't') ch = '\t';
            else if (e == '\n' || e == '\r') continue;
            else ch = e;
        }
        if (ch > ' ' && r->draw) r->draw(r->ctx, r->c, px_x(r, x + i * 0.6 * sx), px_y(r, y), (unsigned long)size_px, ch);
        i++;
    }
}

/* ---- inline pictures: ASCII85, then run-length, RGB rows ---------------------------------------------- */

typedef struct A85 { const unsigned char *p, *end; unsigned char out[4]; int n, at, done; } A85;
static int a85_byte(A85 *a)
{
    while (a->at >= a->n) {
        unsigned long v = 0;
        int k = 0, i;
        if (a->done) return -1;
        while (k < 5) {
            unsigned char c;
            if (a->p >= a->end) return -1;
            c = *a->p++;
            if (is_space(c)) continue;
            if (c == '~') { a->done = 1; break; }
            if (c == 'z' && !k) { v = 0; k = 5; break; }
            if (c < '!' || c > 'u') return -1;
            v = v * 85 + (unsigned long)(c - '!');
            k++;
        }
        if (k == 1) return -1;
        if (!k) return -1;
        for (i = k; i < 5; i++) v = v * 85 + 84;     /* a short last group, padded */
        for (i = 0; i < 4; i++) a->out[i] = (unsigned char)(v >> (24 - 8 * i));
        a->n = k == 5 ? 4 : k - 1; a->at = 0;
    }
    return a->out[a->at++];
}

typedef struct Runs { A85 *a; int left, repeat, value, eod; } Runs;
static int run_byte(Runs *r)
{
    while (!r->left) {
        int c = a85_byte(r->a);
        if (c < 0 || c == 128) { r->eod = 1; return -1; }
        if (c < 128) { r->left = c + 1; r->repeat = 0; }
        else {
            int v = a85_byte(r->a);
            if (v < 0) { r->eod = 1; return -1; }
            r->left = 257 - c; r->repeat = 1; r->value = v;
        }
    }
    r->left--;
    return r->repeat ? r->value : a85_byte(r->a);
}

static void put_row(const Render *r, long y, long x0, long x1, const unsigned char *rgb, unsigned long w)
{
    OAPCanvas *c = r->c;
    long x, from = x0 < r->clip_x0 ? r->clip_x0 : x0, to = x1 > r->clip_x1 ? r->clip_x1 : x1;
    if (y < r->clip_y0 || y >= r->clip_y1 || x1 <= x0) return;
    for (x = from; x < to; x++) {
        unsigned long sx = (unsigned long)(((double)(x - x0) + 0.5) * w / (double)(x1 - x0));
        const unsigned char *s;
        unsigned char *d = c->pix + ((size_t)y * c->w + (size_t)x) * (size_t)c->channels;
        if (sx >= w) sx = w - 1;
        s = rgb + sx * 3;
        if (c->channels == 3) { d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; }
        else d[0] = (unsigned char)((s[0] * 299u + s[1] * 587u + s[2] * 114u + 500u) / 1000u);
    }
}

static int picture(const Render *r, Lex *l, Matrix m, char *err, size_t cap)
{
    Tok t, v;
    unsigned long w = 0, h = 0, bpc = 0, row, i;
    int rgb = 0, filters = 0, ok = 0;
    long x0, x1, y_top, y_bot, y;
    unsigned char *line = NULL;
    A85 a; Runs runs;
    while (next(l, &t) == T_NAME) {
        if (name_is(&t, "W") || name_is(&t, "Width")) { if (next(l, &v) != T_NUM) break; w = (unsigned long)v.num; }
        else if (name_is(&t, "H") || name_is(&t, "Height")) { if (next(l, &v) != T_NUM) break; h = (unsigned long)v.num; }
        else if (name_is(&t, "BPC") || name_is(&t, "BitsPerComponent")) { if (next(l, &v) != T_NUM) break; bpc = (unsigned long)v.num; }
        else if (name_is(&t, "CS") || name_is(&t, "ColorSpace")) {
            if (next(l, &v) != T_NAME) break;
            rgb = name_is(&v, "RGB") || name_is(&v, "DeviceRGB");
        } else if (name_is(&t, "F") || name_is(&t, "Filter")) {
            if (next(l, &v) != T_OPEN) break;
            if (next(l, &v) == T_NAME && (name_is(&v, "A85") || name_is(&v, "ASCII85Decode")) &&
                next(l, &v) == T_NAME && (name_is(&v, "RL") || name_is(&v, "RunLengthDecode")) &&
                next(l, &v) == T_CLOSE) filters = 1;
            else break;
        } else break;
    }
    if (!op_is(&t, "ID")) return fail(err, cap, "A picture in this PDF is not one GFOpenPrint wrote");
    if (!w || !h || bpc != 8 || !rgb || !filters || w > 65535 || h > 65535)
        return fail(err, cap, "A picture in this PDF is not RGB in GFOpenPrint's own coding");
    if (m.b != 0 || m.c != 0 || m.a <= 0 || m.d <= 0)
        return fail(err, cap, "A picture in this PDF is turned or mirrored: only GFOpenPrint's own PDFs go to this printer");
    if (l->p < l->end) l->p++;                     /* the one white-space byte after ID */
    x0 = px_x(r, m.e); x1 = px_x(r, m.e + m.a);
    y_top = px_y(r, m.f + m.d); y_bot = px_y(r, m.f);
    line = malloc(w * 3);
    if (!line) return fail(err, cap, "Not enough memory for a picture");
    memset(&a, 0, sizeof(a)); a.p = l->p; a.end = l->end;
    memset(&runs, 0, sizeof(runs)); runs.a = &a;
    y = y_top;
    for (row = 0; row < h; row++) {
        for (i = 0; i < w * 3; i++) {
            int b = run_byte(&runs);
            if (b < 0) { fail(err, cap, "A picture in this PDF ends early"); goto out; }
            line[i] = (unsigned char)b;
        }
        /* every canvas row whose middle falls in this picture row */
        while (y < y_bot) {
            unsigned long src = (unsigned long)(((double)(y - y_top) + 0.5) * h / (double)(y_bot - y_top));
            if (src != row) break;
            put_row(r, y, x0, x1, line, w);
            y++;
        }
    }
    /* the picture's own end: past "~>", then EI */
    while (!a.done && a85_byte(&a) >= 0) {}
    l->p = a.p;
    while (l->p < l->end && *l->p != '>') l->p++;
    if (l->p < l->end) l->p++;
    if (next(l, &t) != T_OP || !op_is(&t, "EI")) { fail(err, cap, "A picture in this PDF has no end (EI)"); goto out; }
    ok = 1;
out:
    free(line);
    return ok;
}

/* An image XObject as GFOpenView writes it: DeviceRGB, 8 bits, /FlateDecode with PNG rows (/Predictor 10-15). */
static int xobject(const OAPPdfDoc *d, const Render *r, const unsigned char *pg, const unsigned char *pe,
                   const Tok *name, Matrix m, char *err, size_t cap)
{
    Xref x;
    char k[40];
    const unsigned char *xd, *o, *oe, *data, *v;
    unsigned long num, w = 0, h = 0, bpc = 0, length = 0, pred = 1, colors = 3, cols = 0, row, y_row;
    unsigned char *raw = NULL, *pix = NULL;
    size_t got = 0, rowbytes;
    long x0, x1, y_top, y_bot, y;
    int ok = 0;
    Lex l;
    Tok t;
    if (name->n > 30) return fail(err, cap, "This PDF names a picture GFOpenPrint didn't put there");
    snprintf(k, sizeof(k), "/%.*s", (int)name->n, (const char *)name->s);
    xd = key(pg, pe, "/XObject");
    if (!xd || !ref(key(xd, pe, k), pe, &num) || !xref_open(&x, d->pdf, d->len) || !(o = object(&x, num, &oe)))
        return fail(err, cap, "This PDF names a picture it doesn't hold");
    data = find(o, oe, "stream");
    if (!data || !find(o, data, "/Subtype /Image") || !find(o, data, "/DeviceRGB") || !find(o, data, "/FlateDecode"))
        return fail(err, cap, "A picture in this PDF is not one GFOpenView wrote: it needs a printer that takes PDF");
#define NUM(keyname, var) if ((v = key(o, data, keyname)) != NULL) { l.p = v; l.end = data; if (next(&l, &t) != T_NUM) return fail(err, cap, "A picture's size is unreadable"); var = (unsigned long)t.num; }
    NUM("/Width", w) NUM("/Height", h) NUM("/BitsPerComponent", bpc) NUM("/Predictor", pred) NUM("/Colors", colors) NUM("/Columns", cols)
#undef NUM
    if ((v = key(o, data, "/Length")) != NULL) {
        if (ref(v, data, &num)) { const unsigned char *lo = object(&x, num, NULL); if (!lo || !read_ulong(&lo, d->pdf + d->len, &length)) length = 0; }
        else read_ulong(&v, data, &length);
    }
    data += 6;
    if (data < oe && *data == '\r') data++;
    if (data < oe && *data == '\n') data++;
    if (!w || !h || w > 65535 || h > 65535 || bpc != 8 || colors != 3 || (pred != 1 && (pred < 10 || cols != w)) ||
        !length || data + length > oe)
        return fail(err, cap, "A picture in this PDF is not RGB in GFOpenView's own coding");
    if (m.b != 0 || m.c != 0 || m.a <= 0 || m.d <= 0)
        return fail(err, cap, "A picture in this PDF is turned or mirrored: only GFOpenPrint's own PDFs go to this printer");
    rowbytes = (size_t)w * 3;
    raw = malloc((rowbytes + (pred >= 10)) * h);
    pix = pred >= 10 ? malloc(rowbytes * h) : NULL;
    if (!raw || (pred >= 10 && !pix)) { fail(err, cap, "Not enough memory for a picture"); goto out; }
    if (!oap_inflate_zlib(data, length, raw, (rowbytes + (pred >= 10)) * h, &got) || got != (rowbytes + (pred >= 10)) * h ||
        (pred >= 10 && !oap_png_unfilter(raw, h, rowbytes, 3, pix))) {
        fail(err, cap, "A picture in this PDF is damaged"); goto out;
    }
    if (!pix) { pix = raw; raw = NULL; }
    x0 = px_x(r, m.e); x1 = px_x(r, m.e + m.a);
    y_top = px_y(r, m.f + m.d); y_bot = px_y(r, m.f);
    for (y = y_top; y < y_bot; y++) {
        y_row = (unsigned long)(((double)(y - y_top) + 0.5) * h / (double)(y_bot - y_top));
        if (y_row >= h) y_row = h - 1;
        put_row(r, y, x0, x1, pix + y_row * rowbytes, w);
    }
    (void)row;
    ok = 1;
out:
    free(raw); free(pix);
    return ok;
}

static int content(const OAPPdfDoc *d, const unsigned char *p, const unsigned char *e,
                   const unsigned char **s, size_t *n)
{
    Xref x;
    const unsigned char *o, *oe, *k, *data, *stop;
    unsigned long num, length;
    if (!ref(key(p, e, "/Contents"), e, &num) || !xref_open(&x, d->pdf, d->len)) return 0;
    o = object(&x, num, &oe);
    if (!o || !(k = key(o, oe, "/Length"))) return 0;
    if (!ref(k, oe, &num)) { if (!read_ulong(&k, oe, &length)) return 0; }
    else {
        const unsigned char *lo = object(&x, num, NULL);
        if (!lo || !read_ulong(&lo, d->pdf + d->len, &length)) return 0;
    }
    data = find(o, oe, "stream");
    if (!data) return 0;
    data += 6;
    if (data < oe && *data == '\r') data++;
    if (data < oe && *data == '\n') data++;
    stop = data + length;
    if (stop > d->pdf + d->len || stop > oe) return 0;
    if (find(o, data, "/Filter")) return 0;        /* our driver's pages are never compressed */
    *s = data; *n = length;
    return 1;
}

int oap_pdfr_render(const OAPPdfDoc *d, unsigned long page, OAPCanvas *c,
                    OAPDrawChar draw, void *ctx, char *err, size_t cap)
{
    const unsigned char *p, *e, *s;
    size_t n;
    unsigned long w_pt, h_pt;
    Lex l;
    Tok t, nums[6];
    int count = 0, in_text = 0, depth = 0;
    Matrix ctm = {1, 0, 0, 1, 0, 0}, stack[16], tlm = {1, 0, 0, 1, 0, 0};
    long clips[16][4];
    double size = 0, page_w;
    Render r;
    if (!c || !c->pix || (c->channels != 1 && c->channels != 3) || !c->dpi)
        return fail(err, cap, "No page to draw on");
    if (!page_dict(d, page, &p, &e) || !oap_pdfr_page_size(d, page, &w_pt, &h_pt))
        return fail(err, cap, "This PDF's page has no paper size");
    if (!content(d, p, e, &s, &n))
        return fail(err, cap, "This PDF's page is not one GFOpenPrint wrote: it needs a printer that takes PDF");
    r.c = c; r.scale = c->dpi / 72.0; r.draw = draw; r.ctx = ctx;
    if (!media(d, page, &page_w, &r.page_h)) return fail(err, cap, "This PDF's page has no paper size");
    r.clip_x0 = 0; r.clip_y0 = 0; r.clip_x1 = (long)c->w; r.clip_y1 = (long)c->h;
    l.p = s; l.end = s + n;
    for (;;) {
        int type = next(&l, &t);
        if (type == T_END) break;
        if (type == T_NUM) { if (count < 6) nums[count++] = t; else return fail(err, cap, "Too many numbers in a row in this PDF"); continue; }
        if (type == T_STR) {
            Tok op;
            if (next(&l, &op) != T_OP || !op_is(&op, "Tj") || !in_text || size <= 0)
                return fail(err, cap, "This PDF's text is not GFOpenPrint's own");
            show(&r, &t, mul(tlm, ctm), size);
            count = 0;
            continue;
        }
        if (type == T_NAME) {
            Tok sz, op;
            Lex peek = l;
            if (next(&peek, &op) == T_OP && op_is(&op, "Do") && !in_text) {
                if (!xobject(d, &r, p, e, &t, ctm, err, cap)) return 0;
                l = peek; count = 0;
                continue;
            }
            if (!name_is(&t, "F1") || next(&l, &sz) != T_NUM || next(&l, &op) != T_OP || !op_is(&op, "Tf"))
                return fail(err, cap, "This PDF uses a font GFOpenPrint didn't put there");
            size = sz.num; count = 0;
            continue;
        }
        if (type != T_OP) return fail(err, cap, "This PDF's page has parts GFOpenPrint can't draw");
        if (op_is(&t, "BT")) { in_text = 1; tlm.a = 1; tlm.b = 0; tlm.c = 0; tlm.d = 1; tlm.e = 0; tlm.f = 0; }
        else if (op_is(&t, "ET")) in_text = 0;
        else if (op_is(&t, "Td") && count == 2 && in_text) {
            Matrix tr = {1, 0, 0, 1, 0, 0};
            tr.e = nums[0].num; tr.f = nums[1].num;
            tlm = mul(tr, tlm);
        } else if (op_is(&t, "q")) { if (depth >= 16) return fail(err, cap, "This PDF saves too deep"); clips[depth][0] = r.clip_x0; clips[depth][1] = r.clip_y0; clips[depth][2] = r.clip_x1; clips[depth][3] = r.clip_y1; stack[depth++] = ctm; }
        else if (op_is(&t, "Q")) { if (!depth) return fail(err, cap, "This PDF restores what it never saved"); ctm = stack[--depth]; r.clip_x0 = clips[depth][0]; r.clip_y0 = clips[depth][1]; r.clip_x1 = clips[depth][2]; r.clip_y1 = clips[depth][3]; }
        else if (op_is(&t, "cm") && count == 6) {
            Matrix m;
            m.a = nums[0].num; m.b = nums[1].num; m.c = nums[2].num; m.d = nums[3].num; m.e = nums[4].num; m.f = nums[5].num;
            ctm = mul(m, ctm);
        } else if (op_is(&t, "re") && count == 4 && !in_text) {
            /* only as a clip (re W n), the way GFOpenView crops a picture to the paper's printable part */
            Tok w1, n1;
            double x = nums[0].num, y = nums[1].num, cw = nums[2].num, ch = nums[3].num;
            long a, b, cc, dd;
            if (next(&l, &w1) != T_OP || !op_is(&w1, "W") || next(&l, &n1) != T_OP || !op_is(&n1, "n") ||
                ctm.b != 0 || ctm.c != 0 || ctm.a <= 0 || ctm.d <= 0)
                return fail(err, cap, "This PDF's page draws in ways GFOpenPrint's own PDFs never do: it needs a printer that takes PDF");
            a = px_x(&r, ctm.a * x + ctm.e); cc = px_x(&r, ctm.a * (x + cw) + ctm.e);
            b = px_y(&r, ctm.d * (y + ch) + ctm.f); dd = px_y(&r, ctm.d * y + ctm.f);
            if (a > r.clip_x0) r.clip_x0 = a;
            if (b > r.clip_y0) r.clip_y0 = b;
            if (cc < r.clip_x1) r.clip_x1 = cc;
            if (dd < r.clip_y1) r.clip_y1 = dd;
        } else if (op_is(&t, "BI")) { if (!picture(&r, &l, ctm, err, cap)) return 0; }
        else return fail(err, cap, "This PDF's page draws in ways GFOpenPrint's own PDFs never do: it needs a printer that takes PDF");
        count = 0;
    }
    if (in_text || depth) return fail(err, cap, "This PDF's page ends half way");
    return 1;
}

/* Copyright (c) 2026 AmiCradle Team. SPDX-License-Identifier: MIT */
/* zlib streams (RFC 1950) of deflate blocks (RFC 1951) back to their bytes, all at once into a buffer the caller
 * sized. Written from the RFCs; no zlib code. For the pictures GFOpenView puts in its PDFs (GFO-254). */
#include "oap_inflate.h"
#include <string.h>

typedef struct Bits { const unsigned char *p, *end; unsigned long buf; int n; int bad; } Bits;

static unsigned long need(Bits *b, int n)
{
    unsigned long v;
    while (b->n < n) {
        if (b->p >= b->end) { b->bad = 1; return 0; }
        b->buf |= (unsigned long)*b->p++ << b->n;
        b->n += 8;
    }
    v = b->buf & ((1UL << n) - 1);
    b->buf >>= n; b->n -= n;
    return v;
}

typedef struct Huff { unsigned short count[16], sym[320]; } Huff;

static int build(Huff *h, const unsigned char *len, int n)
{
    unsigned short off[16];
    int i, left = 1;
    memset(h->count, 0, sizeof(h->count));
    for (i = 0; i < n; i++) h->count[len[i]]++;
    h->count[0] = 0;
    for (i = 1; i < 16; i++) { left <<= 1; left -= h->count[i]; if (left < 0) return 0; }
    off[1] = 0;
    for (i = 1; i < 15; i++) off[i + 1] = (unsigned short)(off[i] + h->count[i]);
    for (i = 0; i < n; i++) if (len[i]) h->sym[off[len[i]]++] = (unsigned short)i;
    return 1;
}

/* One symbol: codes are read a bit at a time, first bit the code's highest (RFC 1951 3.1.1). */
static int decode(Bits *b, const Huff *h)
{
    int code = 0, first = 0, index = 0, len;
    for (len = 1; len < 16; len++) {
        code |= (int)need(b, 1);
        if (b->bad) return -1;
        if (code - h->count[len] < first) return h->sym[index + (code - first)];
        index += h->count[len];
        first = (first + h->count[len]) << 1;
        code <<= 1;
    }
    return -1;
}

static const unsigned short lbase[29] = {3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
static const unsigned char lextra[29] = {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
static const unsigned short dbase[30] = {1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
static const unsigned char dextra[30] = {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};

static int codes(Bits *b, const Huff *lit, const Huff *dist, unsigned char *out, size_t cap, size_t *at)
{
    for (;;) {
        int s = decode(b, lit);
        if (s < 0) return 0;
        if (s < 256) { if (*at >= cap) return 0; out[(*at)++] = (unsigned char)s; continue; }
        if (s == 256) return 1;
        s -= 257;
        if (s >= 29) return 0;
        {
            size_t len = lbase[s] + need(b, lextra[s]), d;
            int ds = decode(b, dist);
            if (b->bad || ds < 0 || ds >= 30) return 0;
            d = dbase[ds] + need(b, dextra[ds]);
            if (b->bad || d > *at || *at + len > cap) return 0;
            while (len--) { out[*at] = out[*at - d]; (*at)++; }
        }
    }
}

static int fixed(Huff *lit, Huff *dist)
{
    unsigned char l[288];
    int i;
    for (i = 0; i < 144; i++) l[i] = 8;
    for (; i < 256; i++) l[i] = 9;
    for (; i < 280; i++) l[i] = 7;
    for (; i < 288; i++) l[i] = 8;
    if (!build(lit, l, 288)) return 0;
    for (i = 0; i < 30; i++) l[i] = 5;
    return build(dist, l, 30);
}

static int dynamic(Bits *b, Huff *lit, Huff *dist)
{
    static const unsigned char order[19] = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
    unsigned char len[320];
    Huff cl;
    int nl = (int)need(b, 5) + 257, nd = (int)need(b, 5) + 1, nc = (int)need(b, 4) + 4, i = 0;
    if (b->bad || nl > 286 || nd > 30) return 0;
    memset(len, 0, sizeof(len));
    for (i = 0; i < nc; i++) len[order[i]] = (unsigned char)need(b, 3);
    if (b->bad || !build(&cl, len, 19)) return 0;
    memset(len, 0, sizeof(len));
    for (i = 0; i < nl + nd;) {
        int s = decode(b, &cl), rep = 0, val = 0;
        if (s < 0) return 0;
        if (s < 16) { len[i++] = (unsigned char)s; continue; }
        if (s == 16) { if (!i) return 0; val = len[i - 1]; rep = 3 + (int)need(b, 2); }
        else if (s == 17) rep = 3 + (int)need(b, 3);
        else rep = 11 + (int)need(b, 7);
        if (b->bad || i + rep > nl + nd) return 0;
        while (rep--) len[i++] = (unsigned char)val;
    }
    if (!len[256]) return 0;
    return build(lit, len, nl) && build(dist, len + nl, nd);
}

int oap_inflate_zlib(const unsigned char *in, size_t n, unsigned char *out, size_t cap, size_t *got)
{
    Bits b;
    size_t at = 0;
    int last = 0;
    Huff lit, dist;
    if (!in || !out || n < 2 || (in[0] & 15) != 8 || ((in[0] << 8) | in[1]) % 31 || (in[1] & 32)) return 0;
    memset(&b, 0, sizeof(b));
    b.p = in + 2; b.end = in + n;
    while (!last) {
        int type;
        last = (int)need(&b, 1);
        type = (int)need(&b, 2);
        if (b.bad) return 0;
        if (type == 0) {
            unsigned len, nlen;
            b.buf = 0; b.n = 0;                     /* to the byte */
            if (b.end - b.p < 4) return 0;
            len = b.p[0] | (b.p[1] << 8); nlen = b.p[2] | (b.p[3] << 8); b.p += 4;
            if ((len ^ 0xFFFFu) != nlen || (size_t)(b.end - b.p) < len || at + len > cap) return 0;
            memcpy(out + at, b.p, len); at += len; b.p += len;
        } else if (type == 1) {
            if (!fixed(&lit, &dist) || !codes(&b, &lit, &dist, out, cap, &at)) return 0;
        } else if (type == 2) {
            if (!dynamic(&b, &lit, &dist) || !codes(&b, &lit, &dist, out, cap, &at)) return 0;
        } else return 0;
    }
    if (got) *got = at;
    return 1;
}

static unsigned paeth(int a, int b, int c)
{
    int p = a + b - c, pa = p > a ? p - a : a - p, pb = p > b ? p - b : b - p, pc = p > c ? p - c : c - p;
    return (unsigned)(pa <= pb && pa <= pc ? a : pb <= pc ? b : c);
}

int oap_png_unfilter(unsigned char *data, size_t rows, size_t row_bytes, size_t bpp, unsigned char *out)
{
    size_t y, i;
    for (y = 0; y < rows; y++) {
        const unsigned char *in = data + y * (row_bytes + 1);
        unsigned char *cur = out + y * row_bytes;
        const unsigned char *up = y ? cur - row_bytes : NULL;
        int t = in[0];
        in++;
        for (i = 0; i < row_bytes; i++) {
            int a = i >= bpp ? cur[i - bpp] : 0, b = up ? up[i] : 0, c = (up && i >= bpp) ? up[i - bpp] : 0;
            unsigned v;
            switch (t) {
            case 0: v = 0; break;
            case 1: v = (unsigned)a; break;
            case 2: v = (unsigned)b; break;
            case 3: v = (unsigned)((a + b) / 2); break;
            case 4: v = paeth(a, b, c); break;
            default: return 0;
            }
            cur[i] = (unsigned char)(in[i] + v);
        }
    }
    return 1;
}

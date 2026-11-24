/* Copyright (c) 2026 AmiCradle Team. SPDX-License-Identifier: MIT */
#ifndef OAP_INFLATE_H
#define OAP_INFLATE_H
#include <stddef.h>
/* A zlib stream's bytes into out (cap bytes at most); got: how many. 1 when the stream was whole and fitted. */
int oap_inflate_zlib(const unsigned char *in, size_t n, unsigned char *out, size_t cap, size_t *got);
/* PNG row filters undone (/Predictor 10-15): data holds rows of a filter-type byte and row_bytes bytes; out gets
 * rows * row_bytes. bpp: bytes a pixel. 1 when every filter type was known. */
int oap_png_unfilter(unsigned char *data, size_t rows, size_t row_bytes, size_t bpp, unsigned char *out);
#endif

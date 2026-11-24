/* Copyright (c) 2026 AmiCradle Team. SPDX-License-Identifier: MIT */
/* No text font for raster pages (oap_raster.h): for host builds of the network code. Pages with text are then
 * refused in words; pictures still print. The Amiga's is src/amiga/raster_font.c. */
#include "oap_raster.h"
OAPDrawChar oap_raster_font(void **ctx) { *ctx = 0; return 0; }
void oap_raster_font_close(void) {}

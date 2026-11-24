/* Copyright (c) 2026 AmiCradle Team. SPDX-License-Identifier: MIT */
/* The text font for raster pages (oap_raster.h, GFO-254): each character drawn with diskfont.library's
 * DejaVu Sans Mono (an outline font, scaled to the size asked; its advance is Courier's 0.6 em), else topaz, into
 * a one-plane bitmap, then its set pixels made black on the page. */
#include <exec/types.h>
#include <exec/memory.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <graphics/text.h>
#include <graphics/gfxbase.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/diskfont.h>
#include <string.h>
#include "oap_raster.h"

struct GfxBase *GfxBase;
struct Library *DiskfontBase;

#define CELL_MAX 400                     /* pixels: a character at 1200 dpi and 20 pt is 333 */

typedef struct Font {
    struct TextFont *font;
    unsigned long size;
    struct BitMap bm;
    struct RastPort rp;
    UWORD w, h;
    int have_plane;
} Font;
static Font F;

static void drop_font(void)
{
    if (F.have_plane) { FreeRaster(F.bm.Planes[0], F.w, F.h); F.have_plane = 0; }
    if (F.font) { CloseFont(F.font); F.font = NULL; }
    F.size = 0;
}

static int open_size(unsigned long size)
{
    struct TextAttr ta;
    drop_font();
    if (!size || size > CELL_MAX / 2) return 0;
    ta.ta_Name = (STRPTR)"dejavusansmono.font"; ta.ta_YSize = (UWORD)size; ta.ta_Style = 0; ta.ta_Flags = 0;
    F.font = OpenDiskFont(&ta);
    if (!F.font) { ta.ta_Name = (STRPTR)"topaz.font"; F.font = OpenDiskFont(&ta); }
    if (!F.font) return 0;
    F.w = (UWORD)(((F.font->tf_XSize * 2 + 16) + 15) & ~15);
    F.h = (UWORD)(F.font->tf_YSize + 4);
    if (F.w > CELL_MAX) F.w = CELL_MAX;
    if (F.h > CELL_MAX) F.h = CELL_MAX;
    InitBitMap(&F.bm, 1, F.w, F.h);
    F.bm.Planes[0] = AllocRaster(F.w, F.h);
    if (!F.bm.Planes[0]) { drop_font(); return 0; }
    F.have_plane = 1;
    InitRastPort(&F.rp);
    F.rp.BitMap = &F.bm;
    SetFont(&F.rp, F.font);
    SetAPen(&F.rp, 1);
    SetDrMd(&F.rp, JAM1);
    F.size = size;
    return 1;
}

static void draw(void *ctx, OAPCanvas *c, long x, long y, unsigned long size_px, unsigned char ch)
{
    UWORD gx, gy;
    long top;
    (void)ctx;
    if (F.size != size_px && !open_size(size_px)) return;
    SetRast(&F.rp, 0);
    Move(&F.rp, 0, (WORD)F.font->tf_Baseline);
    Text(&F.rp, (CONST_STRPTR)&ch, 1);
    WaitBlit();
    top = y - F.font->tf_Baseline;
    for (gy = 0; gy < F.h; gy++) {
        const UBYTE *line = (const UBYTE *)F.bm.Planes[0] + (ULONG)gy * F.bm.BytesPerRow;
        long py = top + gy;
        if (py < 0 || (unsigned long)py >= c->h) continue;
        for (gx = 0; gx < F.w; gx++) {
            long px = x + gx;
            if (!(line[gx >> 3] & (0x80 >> (gx & 7))) || px < 0 || (unsigned long)px >= c->w) continue;
            memset(c->pix + ((size_t)py * c->w + (size_t)px) * (size_t)c->channels, 0, (size_t)c->channels);
        }
    }
}

OAPDrawChar oap_raster_font(void **ctx)
{
    *ctx = NULL;
    if (!GfxBase) GfxBase = (struct GfxBase *)OpenLibrary((CONST_STRPTR)"graphics.library", 37);
    if (!DiskfontBase) DiskfontBase = OpenLibrary((CONST_STRPTR)"diskfont.library", 37);
    if (!GfxBase || !DiskfontBase) { oap_raster_font_close(); return NULL; }
    return draw;
}

void oap_raster_font_close(void)
{
    drop_font();
    if (DiskfontBase) { CloseLibrary(DiskfontBase); DiskfontBase = NULL; }
    if (GfxBase) { CloseLibrary((struct Library *)GfxBase); GfxBase = NULL; }
}

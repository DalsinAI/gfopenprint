/* Copyright (c) 2026 AmiCradle Team. SPDX-License-Identifier: MIT */
/* The GFOS test page's lines about this Amiga, its drivers, the printer and where the page came from
 * (include/oap_testpage_amiga.h). */
#include <exec/types.h>
#include <exec/execbase.h>
#include <exec/memory.h>
#include <graphics/gfxbase.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/datetime.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "oap.h"
#include "oap_discovery.h"
#include "oap_net.h"
#include "oap_paper.h"
#include "oap_testpage_amiga.h"

extern struct ExecBase *SysBase;
extern struct GfxBase *GfxBase;
extern struct Library *SocketBase;
static const char *cpu_name(UWORD f)
{
    if (f & (1U << 7)) return "68060";
    if (f & AFF_68040) return "68040";
    if (f & AFF_68030) return "68030";
    if (f & AFF_68020) return "68020";
    if (f & AFF_68010) return "68010";
    return "68000";
}

static const char *fpu_name(UWORD f)
{
    if (f & (1U << 6)) return "built in (68040/060)";
    if (f & AFF_68882) return "68882";
    if (f & AFF_68881) return "68881";
    return "none";
}

static const char *chips(void)
{
    UBYTE bits;
    if (!GfxBase) return "not known";
    bits = GfxBase->ChipRevBits0;
    if (bits & GFXF_AA_ALICE) return "AGA (Alice, Lisa)";
    if (bits & (GFXF_HR_AGNUS | GFXF_HR_DENISE)) return "ECS";
    return "OCS";
}

static void mem(char *out, size_t cap, ULONG kind)
{
    ULONG total = AvailMem(kind | MEMF_TOTAL), free_ = AvailMem(kind);
    if (total >= 4UL * 1024 * 1024)
        snprintf(out, cap, "%lu MB, %lu MB free", (unsigned long)(total >> 20), (unsigned long)(free_ >> 20));
    else
        snprintf(out, cap, "%lu KB, %lu KB free", (unsigned long)(total >> 10), (unsigned long)(free_ >> 10));
}

/* Whether a path is there, without "Please insert volume" when it isn't. */
static int exists(const char *path)
{
    struct Process *me = (struct Process *)FindTask(NULL);
    APTR window = me->pr_WindowPtr;
    BPTR l;
    me->pr_WindowPtr = (APTR)-1;
    l = Lock((CONST_STRPTR)path, ACCESS_READ);
    me->pr_WindowPtr = window;
    if (!l) return 0;
    UnLock(l);
    return 1;
}

static void version_line(OAPTestPage *t, const char *label, const char *a, const char *b)
{
    char v[OAP_TP_TEXT];
    if (oap_file_version(a, v, sizeof(v)) || (b && oap_file_version(b, v, sizeof(v))))
        oap_testpage_line(t, label, "%s", v);
    else
        oap_testpage_line(t, label, "not found");
}

static void driver_lines(OAPTestPage *t)
{
    char name[40], path[80];
    unsigned char *prefs = NULL;
    long n = 0;
    FILE *f = fopen("ENV:Sys/printer.prefs", "rb");
    name[0] = 0;
    if (f) {
        prefs = malloc(4096);
        if (prefs) n = (long)fread(prefs, 1, 4096, f);
        fclose(f);
    }
    if (prefs && n > 0 && oap_printer_prefs_driver(prefs, (size_t)n, name, sizeof(name))) {
        snprintf(path, sizeof(path), "DEVS:Printers/%s", name);
        version_line(t, "Printer driver", path, NULL);
        if (!strncmp(t->value[t->n - 1], "not found", 9))
            snprintf(t->value[t->n - 1], sizeof(t->value[0]), "%s (no version found)", name);
    } else
        oap_testpage_line(t, "Printer driver", "none chosen in Prefs/Printer");
    free(prefs);
    version_line(t, "printer.device", "DEVS:printer.device", "SYS:Devs/printer.device");
    version_line(t, "oapspool.device", "DEVS:oapspool.device", NULL);
    version_line(t, "Print window", "PROGDIR:GFOpenPrint", "C:GFOpenPrint");
    version_line(t, "Print worker", "PROGDIR:OAVWorker", "C:OAVWorker");
    version_line(t, "Discovery", "PROGDIR:OAPDiscover", "C:OAPDiscover");
    if (SocketBase && SocketBase->lib_IdString)
        oap_testpage_line(t, "bsdsocket.library", "%.70s", (const char *)SocketBase->lib_IdString);
    else
        oap_testpage_line(t, "bsdsocket.library", "not open");
}

static void host_of(const char *uri, char *host, size_t cap, unsigned *port)
{
    OAPUri u;
    host[0] = 0; *port = 631;
    if (uri && oap_parse_ipp_uri(uri, &u)) {
        snprintf(host, cap, "%s", u.host);
        *port = u.port;
    }
}

void oap_testpage_amiga(OAPTestPage *t, const char *printer_name, const char *uri, int route, const char *note,
                        const char *program)
{
    char a[OAP_TP_TEXT], b[OAP_TP_TEXT], host[256], when[48];
    struct Library *ver;
    struct DateTime dt;
    char day[LEN_DATSTRING], date[LEN_DATSTRING], clock_[LEN_DATSTRING];
    UWORD f = SysBase->AttnFlags;
    unsigned port;
    int had_net = SocketBase != NULL, net = oap_net_start(), own_gfx = 0;
    struct Process *me = (struct Process *)FindTask(NULL);
    APTR window = me->pr_WindowPtr;
    me->pr_WindowPtr = (APTR)-1;              /* a missing volume is a line on the page, not a requester */
    if (!GfxBase) { GfxBase = (struct GfxBase *)OpenLibrary((CONST_STRPTR)"graphics.library", 33); own_gfx = 1; }

    oap_testpage_section(t, "This Amiga");
    oap_testpage_line(t, "System", "%s", exists("GFOSBOOT:") ? "GFOS (AmiCradle)" : "AmigaOS");
    if (net && oap_net_host_name(a, sizeof(a))) oap_testpage_line(t, "Host name", "%s", a);
    oap_testpage_line(t, "Kickstart", "exec %u.%u", (unsigned)SysBase->LibNode.lib_Version, (unsigned)SysBase->LibNode.lib_Revision);
    ver = OpenLibrary((CONST_STRPTR)"version.library", 0);
    if (ver) { oap_testpage_line(t, "Workbench", "%u.%u", (unsigned)ver->lib_Version, (unsigned)ver->lib_Revision); CloseLibrary(ver); }
    oap_testpage_line(t, "Processor", "%s, FPU %s", cpu_name(f), fpu_name(f));
    oap_testpage_line(t, "Chip set", "%s", chips());
    mem(a, sizeof(a), MEMF_CHIP); oap_testpage_line(t, "Chip memory", "%s", a);
    mem(b, sizeof(b), MEMF_FAST); oap_testpage_line(t, "Fast memory", "%s", b);

    oap_testpage_section(t, "Drivers and programs");
    oap_testpage_line(t, "This page by", "%s", program ? program : "GFOpenPrint");
    driver_lines(t);

    oap_testpage_section(t, "Printer");
    oap_testpage_line(t, "Name", "%s", printer_name && printer_name[0] ? printer_name : "not named");
    oap_testpage_line(t, "Address", "%s", uri ? uri : "");
    if ((route & ~OAP_TP_URF) != OAP_PDF_RASTER) oap_testpage_line(t, "Sent as", "PDF (application/pdf)");
    else if ((route & OAP_TP_URF) || (note && strstr(note, "Apple raster")))
        oap_testpage_line(t, "Sent as", "Apple raster pages (image/urf), drawn on this Amiga");
    else oap_testpage_line(t, "Sent as", "PWG Raster pages (image/pwg-raster), drawn on this Amiga");
    if (note && note[0]) oap_testpage_line(t, "It said", "%s", note);

    oap_testpage_section(t, "Sent");
    host_of(uri, host, sizeof(host), &port);
    if (net && host[0] && oap_net_local_ip(host, port, a, sizeof(a)))
        oap_testpage_line(t, "From address", "%s", a);
    else
        oap_testpage_line(t, "From address", "not known");
    DateStamp(&dt.dat_Stamp);
    dt.dat_Format = FORMAT_DOS; dt.dat_Flags = 0;
    dt.dat_StrDay = (STRPTR)day; dt.dat_StrDate = (STRPTR)date; dt.dat_StrTime = (STRPTR)clock_;
    if (DateToStr(&dt)) { snprintf(when, sizeof(when), "%s %s %s", day, date, clock_); oap_testpage_line(t, "At", "%s", when); }
    if (own_gfx && GfxBase) { CloseLibrary((struct Library *)GfxBase); GfxBase = NULL; }
    me->pr_WindowPtr = window;
    if (net && !had_net) oap_net_stop();
}

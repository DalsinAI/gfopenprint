/* Copyright (c) 2026 AmiCradle Team. SPDX-License-Identifier: MIT */
/* GFOpenPrint's real network, TLS and IPP code from the command line: the
 * host tests (tests/host/test_ipps.sh) run it against a printer emulator on
 * x86 or ARM64 cores, and the Amiga lab runs it as C:OAPIPPSTest.
 *   ipps_tool query URI          what the printer can do and its certificate
 *   ipps_tool print URI DOC [COPIES [NAME]]
 *                                Print-Job of a PDF or PostScript, as C:OAVWorker sends it
 *   ipps_tool trust KEY FP SUBJ  remember a printer's certificate
 * OAP_TLS=none leaves TLS out ("opentls": the one backend), OAP_TRUST_FILE the
 * remembered certificates, OAP_TLS_CA_FILE an extra authority, and
 * OAP_LOGIN_FILE the logins; on the Amiga, ENV:GFOpenPrint/TLS,
 * ENV:GFOpenPrint/TrustedPrinters and ENV:GFOpenPrint/Logins. */
#include "oap.h"
#include "oap_net.h"
#include "oap_tls.h"
#include "oap_stack.h"
#include "oap_testpage.h"
#ifdef __amigaos__
#include "oap_testpage_amiga.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct GfxBase *GfxBase;      /* raster pages' text font (src/amiga/raster_font.c) opens graphics.library into it */

#ifdef __amigaos__
unsigned long __stack = 65536;
static const char oap_version[] __attribute__((used)) = "$VER: OAPIPPSTest 0.2 (24.11.2026)";
#endif

static const char *verdict_name(int v)
{
    static const char *names[] = { "none", "ok", "trusted", "ask", "changed", "name", "dates", "bad", "notls", "failed" };
    return v >= 0 && v <= OAP_CERT_FAILED ? names[v] : "?";
}

static int tool_main(int argc, char **argv)
{
    if (argc == 3 && !strcmp(argv[1], "query")) {
        OAPCaps caps;
        OAPTlsInfo tls;
        char note[256];
        int ok;
        oap_net_start();
        ok = oap_query_pdf(argv[2], &caps, &tls, note, sizeof(note));
        printf("ok=%d pdf=%d ps=%d verdict=%s backend=%s protocol=%s\nfingerprint=%s\nsubject=%s\nnote=%s\n", ok, caps.pdf, caps.ps,
               verdict_name(tls.verdict), tls.backend, tls.protocol, tls.fingerprint, tls.subject, note);
        printf("pwg=%d urf=%d dpi=%lu types=%d color=%d duplex=%d\n", caps.pwg, caps.urf, caps.pwg_dpi, caps.pwg_types, caps.color, caps.duplex);
        oap_net_stop();
        return ok ? 0 : 1;
    }
    if ((argc == 4 || argc == 5) && !strcmp(argv[1], "testpage")) {
        /* the GFOS test page for a printer, as Printers and Queue makes it: testpage URI OUT [raster] */
        OAPCaps caps;
        OAPTlsInfo tls;
        char note[256];
        int ok, route;
        OAPTestPage *t = malloc(sizeof(*t));
        if (!t) return 1;
        oap_net_start();
        ok = oap_query_pdf(argv[2], &caps, &tls, note, sizeof(note));
        route = argc == 5 && !strcmp(argv[4], "raster") ? OAP_PDF_RASTER : ok ? oap_caps_route(&caps) : OAP_PDF_UNKNOWN;
        oap_testpage_init(t, "GFOS test page", "GFOpenPrint: colour, grey and what printed this page");
#ifdef __amigaos__
        oap_testpage_amiga(t, caps.name[0] ? caps.name : caps.model, argv[2], route | (route == OAP_PDF_RASTER && caps.urf ? OAP_TP_URF : 0),
                           note, oap_version + 6);
#else
        oap_testpage_line(t, "Printer", "%s", argv[2]);
#endif
        ok = oap_pdf_write_testpage(argv[3], t);
        printf("testpage=%d route=%d lines=%d\n", ok, route, t->n);
        free(t);
        oap_net_stop();
        return ok ? 0 : 1;
    }
    if (argc >= 4 && argc <= 7 && !strcmp(argv[1], "print")) {
        OAPJobOptions o;
        char status[300];
        int rc;
        oap_job_defaults(&o);
        snprintf(o.printer_uri, sizeof(o.printer_uri), "%s", argv[2]);
        snprintf(o.job_name, sizeof(o.job_name), "%s", argc > 5 ? argv[5] : "GFOpenPrint IPPS host test");
        if (argc > 4)
            o.copies = atoi(argv[4]);
        o.raster = argc > 6 && !strcmp(argv[6], "raster");
        rc = oap_ipp_submit_doc_ex(argv[3], &o, status, sizeof(status), NULL, NULL);
        printf("rc=%d\nstatus=%s\n", rc, status);
        if (rc == OAP_SEND_ACCEPTED)
            printf("job=%lu\n", oap_ipp_last_job_id());
        return rc == OAP_SEND_ACCEPTED ? 0 : 1;
    }
    if (argc == 4 && !strcmp(argv[1], "job")) {
        /* how the printer is getting on with a job it took: job URI ID */
        OAPJobState js;
        char words[200], note[200];
        int ok;
        oap_net_start();
        ok = oap_query_job(argv[2], strtoul(argv[3], NULL, 10), &js, note, sizeof(note));
        if (ok) {
            oap_job_state_words(&js, words, sizeof(words));
            printf("state=%d pages=%lu reasons=%s message=%s\nwords=%s\n", js.state, js.pages, js.reasons, js.message, words);
        } else
            printf("job query failed: %s\n", note);
        oap_net_stop();
        return ok ? 0 : 1;
    }
    if (argc == 5 && !strcmp(argv[1], "trust"))
        return oap_trust_remember(argv[2], argv[3], argv[4]) ? 0 : 1;
    fprintf(stderr, "usage: OAPIPPSTest query URI | job URI ID | testpage URI OUT [raster] | print URI DOC [COPIES [NAME [raster]]] | trust KEY FINGERPRINT SUBJECT\n");
    return 2;
}

int main(int argc, char **argv)
{
    return oap_main_with_stack(tool_main, argc, argv, 65536);   /* a TLS handshake needs 16 KB of stack */
}

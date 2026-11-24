/* Copyright (c) 2026 AmiCradle Team. SPDX-License-Identifier: MIT */
/* What the GFOS test page says about the Amiga that printed it (src/amiga/testpage_info.c): fills t. route:
 * OAP_PDF_YES or OAP_PDF_RASTER (oap_discovery.h); note: what the printer said last (or NULL); program: the $VER
 * of the program making the page. Opens bsdsocket.library for the host name and the address, if it can. */
#ifndef OAP_TESTPAGE_AMIGA_H
#define OAP_TESTPAGE_AMIGA_H
#include "oap_testpage.h"
void oap_testpage_amiga(OAPTestPage *t, const char *printer_name, const char *uri, int route, const char *note,
                        const char *program);
#endif

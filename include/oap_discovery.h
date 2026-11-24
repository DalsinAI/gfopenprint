/* Copyright (c) 2026 AmiCradle Team. SPDX-License-Identifier: MIT */
#ifndef OAP_DISCOVERY_H
#define OAP_DISCOVERY_H
#include <stddef.h>
#include <stdint.h>
#define OAP_DISC_MAX 24
#define OAP_HOST_MAX 48
#define OAP_HTTP_MAX 196608U
#define OAP_BODY_MAX 131072U
#define OAP_DISC_SELECTION "ENV:GFOpenPrint/PrinterURI"
#define OAP_DISC_SAVED "ENVARC:GFOpenPrint/PrinterURI"
/* Before 4 Oct 2026 GFOpenPrint was OpenAmigaPrint: its settings are read from
 * the old place when the new one has none. */
#define OAP_DISC_SELECTION_OLD "ENV:OpenAmigaPrint/PrinterURI"
#define OAP_DISC_SAVED_OLD "ENVARC:OpenAmigaPrint/PrinterURI"
/* How a printer takes documents. _RASTER: no PDF, but PWG Raster (image/pwg-raster), which GFOpenPrint makes from
 * its own PDFs (oap_pdfr.h); only ever a route, never a caps.pdf value. */
enum { OAP_PDF_UNKNOWN=0, OAP_PDF_YES=1, OAP_PDF_NO=2, OAP_PDF_RASTER=3 };
#define OAP_ROUTE_OK(r) ((r) == OAP_PDF_YES || (r) == OAP_PDF_RASTER)
enum { OAP_PWG_GREY8=1, OAP_PWG_RGB8=2 };
typedef struct OAPName { unsigned char wire[256]; size_t len; } OAPName;
/* pdf and ps: OAP_PDF_YES when document-format-supported lists application/pdf (application/postscript), _NO when
 * it lists others but not that one, _UNKNOWN when the printer didn't say. */
/* pwg: as pdf, for image/pwg-raster, and _YES only with a sgray_8 or srgb_8 type (pwg_types, OAP_PWG_*) and a square
 * resolution in dpi (pwg_dpi: 300 when offered, else the nearest). A printer without PWG Raster that takes Apple
 * raster (image/urf, urf-supported W8 or SRGB24 and RS dots an inch) has the same, with urf set: the same pages go
 * with Apple's headers. */
typedef struct OAPCaps { int pdf, ps, accepting, state, color, duplex, pwg, pwg_types, urf; unsigned long pwg_dpi; unsigned status; uint32_t job_id; char name[128], model[128], location[128], reasons[256]; } OAPCaps;
typedef struct OAPDiscovered { OAPName service, target; char label[128], host[256], ip[16], path[256], uri[640], note[256]; unsigned port; int secure, have_srv, have_txt, alive; OAPCaps caps;
    int cert;                       /* ipps://: OAP_CERT_* (oap_tls.h), what the check found */
    char fingerprint[96], cert_subject[128];
} OAPDiscovered;
typedef struct OAPHost { OAPName name; char ip[16]; } OAPHost;
typedef struct OAPDiscovery { OAPDiscovered printers[OAP_DISC_MAX]; OAPHost hosts[OAP_HOST_MAX]; size_t count, host_count; } OAPDiscovery;
int oap_dns_name_text(const char *, OAPName *);
size_t oap_dns_query(const OAPName *, unsigned, uint16_t, unsigned char *, size_t);
int oap_dns_packet(OAPDiscovery *, const unsigned char *, size_t);
void oap_discovery_resolve(OAPDiscovery *);
size_t oap_ipp_query_request(const char *, uint32_t, unsigned char *, size_t);
int oap_ipp_parse_caps(const unsigned char *, size_t, uint32_t, OAPCaps *);
/* A job at the printer (Get-Job-Attributes): state 3 pending ... 5 processing, 6 stopped, 7 canceled, 8 aborted,
 * 9 completed (RFC 8011 5.3.7); pages: impressions done. */
typedef struct OAPJobState { int state; unsigned status; unsigned long pages; char reasons[192], message[128]; } OAPJobState;
size_t oap_ipp_job_request(const char *uri, uint32_t id, uint32_t job, unsigned char *out, size_t cap);
int oap_ipp_parse_job(const unsigned char *, size_t, uint32_t id, OAPJobState *);
/* The state in words ("Printed: 1 page", "Failed at the printer: ..."); 1 once the job has ended (7-9). */
int oap_job_state_words(const OAPJobState *, char *out, size_t cap);
int oap_http_response(const unsigned char *, size_t, int, unsigned char *, size_t, size_t *, int *);
int oap_http_header(const unsigned char *, size_t, const char *, char *, size_t);
int oap_pdf_eligible(const OAPDiscovered *);
/* OAP_PDF_YES when it takes PDF, _RASTER when only PWG Raster, else caps->pdf (_NO or _UNKNOWN). */
static inline int oap_caps_route(const OAPCaps *c)
{ return c->pdf == OAP_PDF_YES ? OAP_PDF_YES : c->pwg == OAP_PDF_YES ? OAP_PDF_RASTER : c->pdf; }
/* A printer that offers both ipp:// and ipps:// is shown once: by ipps://
 * when this Amiga has TLS (`secure_ok`), else by ipp://. */
void oap_discovery_prefer(OAPDiscovery *, int secure_ok);
int oap_discovery_save(const char *, const OAPDiscovery *, int, const char *);
int oap_discovery_load(const char *, OAPDiscovery *, int *, char *, size_t);
/* C:OAPDiscover: searches the network and writes what it finds to `output`
 * (oap_discovery_save) as it goes. Its connections are in oap_net.h. */
int oap_discover_run(const char *output);
/* Asks one printer what it can do (oap_query_pdf) and keeps what was found,
 * its certificate too, in the entry. */
int oap_query_found(OAPDiscovered *e);
#endif

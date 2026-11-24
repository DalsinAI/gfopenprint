# Host tests, on x86 or ARM64 cores: no Amiga, no printer, no network
# (test-discovery-sendfail injects a refused mDNS send).
CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra -Werror -std=c11
CPPFLAGS += -Iinclude
CORE = src/core/job.c src/core/ipp.c src/core/pdf_demo.c

.PHONY: all test clean test-core test-discovery test-submit test-auth test-viewer test-driver test-discovery-sendfail test-discovery-bind test-build-out test-build-rc test-ipps test-paper test-paper-page test-rawps test-pwg test-pdfr test-pdf2pwg test-testpage
all: test
test: test-core test-discovery test-submit test-auth test-viewer test-driver test-discovery-sendfail test-discovery-bind test-build-out test-build-rc test-paper test-paper-page test-pwg test-pdfr test-pdf2pwg test-testpage

build/test_core: tests/test_core.c $(CORE) include/oap.h
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -D_GNU_SOURCE -o $@ tests/test_core.c $(CORE)
test-core: build/test_core
	./build/test_core

build/test_discovery: tests/test_discovery.c src/discovery/protocol.c src/discovery/http.c include/oap_discovery.h
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ tests/test_discovery.c src/discovery/protocol.c src/discovery/http.c
test-discovery: build/test_discovery
	./build/test_discovery

SUBMIT_RASTER = src/core/pdf2pwg.c src/core/pwg.c src/core/pdfr.c src/core/inflate.c
build/test_submit: tests/test_submit.c src/net/ipp_submit.c src/core/ipp.c src/core/job.c src/discovery/protocol.c src/tls/tls.c src/tls/trust.c include/oap.h include/oap_net.h include/oap_tls.h $(SUBMIT_RASTER) include/oap_raster.h
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ tests/test_submit.c src/net/ipp_submit.c src/core/ipp.c src/core/job.c src/discovery/protocol.c src/tls/tls.c src/tls/trust.c $(SUBMIT_RASTER)
# the raster jobs send build/driver-300-lines.pdf, which test-driver writes
test-submit: build/test_submit test-driver
	./build/test_submit

build/test_auth: tests/test_auth.c src/net/http_auth.c src/tls/tls.c src/tls/trust.c src/discovery/protocol.c src/discovery/http.c include/oap_net.h include/oap_tls.h
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -D_DEFAULT_SOURCE -o $@ tests/test_auth.c src/net/http_auth.c src/tls/tls.c src/tls/trust.c src/discovery/protocol.c src/discovery/http.c
test-auth: build/test_auth
	./build/test_auth

build/test_viewer_core: tests/test_viewer_core.c src/viewer/oav_core.c include/oav_core.h
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -D_GNU_SOURCE -o $@ tests/test_viewer_core.c src/viewer/oav_core.c
test-viewer: build/test_viewer_core
	./build/test_viewer_core

# The printer driver (the PDF writer behind printer.device), against stub headers in tests/host/driver
build/test_driver: tests/test_driver.c src/driver/openprint_driver.c $(wildcard tests/host/driver/*/*.h)
	mkdir -p build
	$(CC) -Itests/host/driver $(CFLAGS) -o $@ tests/test_driver.c src/driver/openprint_driver.c
test-driver: build/test_driver
	./build/test_driver

# PWG Raster pages (image/pwg-raster) for printers that take no PDF: header offsets and rows read back
build/test_pwg: tests/test_pwg.c src/core/pwg.c include/oap_raster.h
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ tests/test_pwg.c src/core/pwg.c
test-pwg: build/test_pwg
	./build/test_pwg

# GFOpenPrint's own PDFs (the driver's, GFOpenView's) as page pixels for raster printers; reads the PDFs
# test-driver writes to build/, and makes GFOpenView PDFs with oav_core.c
build/test_pdfr: tests/test_pdfr.c src/core/pdfr.c src/core/inflate.c src/core/pdf_demo.c src/viewer/oav_core.c include/oap_pdfr.h include/oap_inflate.h
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -D_GNU_SOURCE -o $@ tests/test_pdfr.c src/core/pdfr.c src/core/inflate.c src/core/pdf_demo.c src/viewer/oav_core.c
test-pdfr: build/test_pdfr test-driver
	./build/test_pdfr

# Those pages as PWG Raster: read back with the test's own decoder, landscape turned upright, page ranges
build/test_pdf2pwg: tests/test_pdf2pwg.c src/core/pdf2pwg.c src/core/pwg.c src/core/pdfr.c src/core/inflate.c include/oap_raster.h include/oap_pdfr.h
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ tests/test_pdf2pwg.c src/core/pdf2pwg.c src/core/pwg.c src/core/pdfr.c src/core/inflate.c
test-pdf2pwg: build/test_pdf2pwg test-driver
	./build/test_pdf2pwg

# The GFOS test page: drawn by GFOpenPrint's own reader in colour and grey, versions from files, the sender's address
TESTPAGE_NET = src/net/conn.c src/net/mdns.c src/tls/tls.c src/tls/trust.c src/discovery/protocol.c src/discovery/http.c
build/test_testpage: tests/test_testpage.c src/core/testpage.c src/core/pdfr.c src/core/inflate.c $(TESTPAGE_NET) include/oap_testpage.h
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -D_DEFAULT_SOURCE -o $@ tests/test_testpage.c src/core/testpage.c src/core/pdfr.c src/core/inflate.c $(TESTPAGE_NET)
test-testpage: build/test_testpage
	./build/test_testpage

test-discovery-sendfail:
	sh tests/test_discovery_sendfail.sh
test-discovery-bind:
	sh tests/test_discovery_bind.sh
test-build-out:
	sh tests/test_build_out.sh
test-build-rc:
	sh tests/test_build_rc.sh

# Where the Print window's paper starts (.job, GFOpenPrefs' Printing page, its own) and each size's IPP media name;
# writes each paper's Print-Job request start to build/paper-*.ipp (files only, nothing sent).
build/test_paper: tests/test_paper.c src/core/paper.c src/core/ipp.c src/core/job.c include/oap.h include/oap_paper.h
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ tests/test_paper.c src/core/paper.c src/core/ipp.c src/core/job.c
test-paper: build/test_paper
	./build/test_paper

# The Printing page's own writer (gfopenprefs Printing/src/pr_prefs.c) against paper.c's reader; skips with no
# gfopenprefs checkout beside this one (OPENAMIGAPREFS=dir).
test-paper-page:
	sh tests/test_paper_page.sh

# IPPS end to end against printers on 127.0.0.1 (ippeveprinter and a login
# printer), the GFOpenTLS backend; skips when ippeveprinter or OpenSSL is missing.
test-ipps:
	sh tests/host/test_ipps.sh

# Raw PostScript end to end against stand-in printers on 127.0.0.1 (tests/host/ps_printer.py; ippeveprinter
# too when installed): byte for byte, its attributes, refusals and no hang. Prints nothing; skips with no python3.
test-rawps:
	sh tests/test_rawps.sh

clean:
	rm -f build/test_core build/test_discovery build/test_submit build/test_auth build/test_viewer_core build/test_driver build/oap-firstlight.pdf
	rm -f build/test_paper build/test_paper_page build/paper-*.ipp
	rm -rf build/rawps

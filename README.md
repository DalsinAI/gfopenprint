# GFOpenPrint

GFOpenPrint is a standalone, freely distributable modern printing stack for classic Amiga systems.

GFOpenPrint and its viewer GFOpenView were called OpenAmigaPrint and OpenAmigaView until 4 October 2026. The repository was `DalsinAI/openamigaprint` until the AmiCradle rename of 1–2 November 2026, and is now `DalsinAI/gfopenprint`. The Installer moves the old programs to `SYS:Storage/GFOpenPrint-Superseded`, and settings saved under the old name are still read. `openprint.readme` is the Aminet readme.

Its design goal is deliberately conventional on the Amiga side:

```
application
    |
   PRT: / printer.device
    |
DEVS:Printers/GFOpenPrint
    |  streaming PDF 1.4
    v
DEVS:oapspool.device
    |  T: spool, document boundary at %%EOF
    v
C:GFOpenPrint
    |
    +-- Save PDF
    +-- native Amiga print window
    +-- IPP and IPPS over bsdsocket.library
           (IPPS: gfopentls.library)
```

There is no AmiCradle dependency in the core stack.
## First-light status

Implemented and building for AmigaOS 3.x on the 68040:

- classic V35 printer-driver segment;
- text-to-PDF streaming through `printer.device`;
- `PRD_DUMPRPORT` RGB raster path;
- bounded-memory PDF generation with xref/trailer;
- `oapspool.device` port device and PDF job boundary detection;
- native GadTools/ASL print window;
- Save PDF;
- plain `ipp://` Print-Job submission through `bsdsocket.library`;
- `ipps://` (IPP over TLS, 9 October 2026): TLS on the same socket through gfopentls.library (AmiSSL 5 too until 19 October 2026), with SNI, certificate and host-name checks, a "Trust this printer?" question for a printer's own certificate, remembered per printer by its fingerprint, and HTTP Basic and Digest logins sent only over a trusted TLS connection (see [IPPS](#ipps-ipp-over-tls));
- copies, paper (A4, Letter, Legal, A3, A5), portrait/landscape, colour/mono, duplex and page-range IPP attributes;
- the Print window starts on the document's own paper, else on the paper set on GFOpenPrefs' Printing page (`printer.prefs`), unless another was chosen in the window since that page's paper last changed (`include/oap_paper.h`);
- PostScript a program writes itself (`PRT:RAW`, PageStream and the like; kept in the queue as `.ps`) is printed from Printers and Queue to a printer that lists `application/postscript`, as it is, byte for byte, with `document-format`, its `%%Title` as `job-name`, and copies (27 October 2026). The printer is asked first; one that doesn't take PostScript is sent nothing and the Print window says so. PCL and other printer data stay in the queue (`make test-rawps`);
- host IPP/PDF codec tests;
- Amiga spool and printer.device smoke-test programs.

The code is source-built independently of AmiCradle.
## Not yet implemented

The following are intentionally not claimed by first light:

- real PDF page rendering inside the preview pane;
- fully qualified native discovery on all target TCP/IP stacks;
- capability negotiation beyond the PDF/accepting-jobs gate;
- a password question for printers that ask for a login (logins are kept in `ENVARC:GFOpenPrint/Logins` for now), and Digest with SHA-256;
- TLS 1.3 over gfopentls.library (version 1 speaks TLS 1.2; it comes with the library);
- PWG Raster fallback for printers that do not accept PDF;
- complete mapping of every classic text-style command;
- broad application compatibility testing;
- qualification on real hardware;
- automated installer or Prefs application.

The current preview identifies the real PDF job and displays a page placeholder. It is not yet a PDF interpreter.

## Build

Host core:

```sh
make test
```

Classic AmigaOS 3.x:

```sh
./build-amiga.sh
```

Every 68k compile has `-Wall -Wextra -Werror`. `scripts/build-compilers.sh` builds every program with the os32 GCC 6.5 stove and the os32 GCC 16 stove (`OS32_GCC65`, `OS32_GCC16`) into `build/gcc65` and `build/gcc16`, and fails on any diagnostic from either. `AMIGA_CC` and `OAP_OUT` choose the compiler and the output folder of the three build scripts.
## Installing with the Amiga Installer

Build the programs (`./build-amiga.sh`, `./build-browser.sh`, `./build-viewer.sh`), then the package:

```sh
python3 tools/make_package.py --lha-module ~/AmiCradle/scripts
```

`build/package/GFOpenPrint` is a drawer to copy to the Amiga; `build/package/GFOpenPrint.lha` is the same as one archive (`--lha-module` names the folder with AmiCradle's `lha_archive.py`; without it only the drawer is made). On the Amiga, double-click **Install GFOpenPrint** (it runs SYS:System/Installer on OS 3.2, C:Installer elsewhere). It installs the five commands in C:, `oapspool.device` in DEVS:, the printer driver in DEVS:Printers, the spooler in WBStartup if you want it, and a GFOpenPrint drawer (SYS:Utilities by default) with GFOpenView, Printers and Queue, a test picture and a test page. The script is `package/Install-GFOpenPrint`.

## GFOpenView as MultiView (GFOpenView 0.5)

GFOpenView shows what a datatype reads; no file kind is named in it. GFOpenUp 0.7.2 installs it at `SYS:Utilities/MultiView`, keeping the original in `SYS:Storage/GFOpenUp-Superseded`, so everything that runs MultiView runs GFOpenView.

- **Command line:** MultiView 47's template, word for word: `FILE,CLIPBOARD/S,CLIPUNIT/K/N,SCREEN/S,PUBSCREEN/K,REQUESTER/S,BOOKMARK/S,FONTNAME/K,FONTSIZE/K/N,BACKDROP/S,WINDOW/S,PORTNAME/K,IMMEDIATE/S,REPEAT/S,PRTUNIT/K/N,WINDOWLEFT/K/N,WINDOWTOP/K/N,WINDOWWIDTH/K/N,WINDOWHEIGHT/K/N,AUTORESIZE/S`. Honoured: FILE, PUBSCREEN, REQUESTER, FONTNAME, FONTSIZE, WINDOW, PORTNAME, IMMEDIATE, REPEAT, WINDOWLEFT/TOP/WIDTH/HEIGHT. Read and ignored: CLIPUNIT, SCREEN, BOOKMARK, BACKDROP, PRTUNIT, AUTORESIZE. CLIPBOARD is read and said so in the status line (GFOpenView does not show the Clipboard). With no FILE and no WINDOW, started from Workbench, a file requester opens first, as MultiView's does; from a Shell (where scripts start it) the window opens empty, so no requester holds up its ARexx port, and REQUESTER asks. Started from Workbench, the project icon's tool types are read as the same keywords.
- **ARexx:** each viewer has a port `MULTIVIEW.n` (n from 1, the first free) or the PORTNAME, as MultiView's windows have, as well as `GFOPENVIEW` (the first viewer). MultiView's commands answer: OPEN [NAME], RELOAD, PRINT, ABOUT, QUIT, GETFILEINFO, GETCURRENTDIR, GETOBJECTINFO (VAR or STEM), GETTRIGGERINFO, DOTRIGGERMETHOD, MINIMUMSIZE, NORMALSIZE, MAXIMUMSIZE, WINDOWTOFRONT, WINDOWTOBACK, SCREENTOFRONT, SCREENTOBACK, ACTIVATEWINDOW, BEEPSCREEN, CLEARSELECTED, SCREEN, PUBSCREEN. COPY, PASTE and SAVEAS return 5 (not supported; SAVEPDF saves a picture as PDF). `ACTIVATE` (window and screen to the front), `DEACTIVATE` (RC 5: no icon to go to), `VERSION`, `HELP`, `FAULT code` (a DOS error's text) and `LASTERROR` (why the last command failed) are the commands every Open app's port has. Return codes as every Open app's port (amicradle-teams ARexx-RULE.md): 0 done, 5 not done but nothing wrong, 10 an error (OPEN without a file, a file that can't be shown, nothing open), 20 an unknown command (MultiView returns 0). No command shows a requester: OPEN without a file answers 10 rather than asking. `Rexx/ViewerInfo.rexx` is an example and `tests/arexx.rexx` checks the port.
- **Not the same:** AmigaGuide `system` and `rx` links are expected to stay off (GFOpenView sets AGA_Secure; not tested); a file over 64 MiB or a picture over 16 megapixels is refused; Clipboard viewing, Save as in the file's own format and block selection are not there; after a link into another AmigaGuide file the title and GETFILEINFO still name the first file.

## First-light Amiga installation

Copy:

```
C/GFOpenPrint          -> C:GFOpenPrint
C/OAPPrintTest            -> C:OAPPrintTest
C/OAPSpoolTest            -> C:OAPSpoolTest
C/OAPStatusTest           -> C:OAPStatusTest
Devs/oapspool.device      -> DEVS:oapspool.device
Devs/Printers/GFOpenPrint -> DEVS:Printers/GFOpenPrint
GFOpenPrint.info         -> DEVS:Printers/GFOpenPrint.info
```

In **Printer Preferences** select:

- Printer Type: `GFOpenPrint`
- Printer Port: device
- Device Unit: `oapspool.device`, unit 0

Run `OAPStatusTest` first to verify the virtual port reports ready with paper-out and busy permanently clear. Then `OAPPrintTest` exercises the ordinary `printer.device` path. See `HOW_TO_TEST_FIRST_LIGHT.md` for the complete acceptance test.

The generated PDF is spooled under `T:GFOpenPrint-job-XXXX.pdf`; once `%%EOF` is received, `oapspool.device` launches `C:GFOpenPrint` asynchronously.
## Network model

The client takes printer addresses such as:

```
ipps://printer.local/ipp/print
ipp://192.168.1.50:631/ipp/print
```

Port 631 is the default for both. Networking is exclusively through the public `bsdsocket.library` API. Roadshow, AmiTCP, Miami, Genesis, ACNet or another compatible provider can supply that API. A printer's own `.local` name is resolved with one multicast DNS question when the stack can't resolve it.

## IPPS (IPP over TLS)

IPPS sits behind a TLS abstraction (`include/oap_tls.h`), and plain `ipp://` needs no TLS library at all. The programs that talk to printers (`C:OAPDiscover` and `C:OAVWorker`) open **gfopentls.library** (GFOpenTLS, on BearSSL; MIT licensed and free; API version 1), from DalsinAI/gfopentls. Its headers are in `third_party/opentls`; the library is opened at run time. No AmiSSL SDK is needed or used: AmiSSL 5 was a second backend until 19 October 2026, when the Team's programs moved to GFOpenTLS only.

Without it, an `ipps://` printer says so in words ("This printer uses IPPS (encrypted IPP), which needs gfopentls.library..."), and nothing is sent. `ENV:GFOpenPrint/TLS` set to `none` acts as if it weren't there, for testing.

**Choosing it.** Printers and Queue lists a printer that offers both `ipp://` and `ipps://` once, by `ipps://` when this Amiga has a TLS library. The address field starts with `ipps://`, and `ipp://` addresses work as before. The Print requester shows an `ipps://` printer as encrypted, and offers it once its certificate is accepted.

**Certificates.** TLS goes over the same socket, with the printer's name in SNI. A certificate is accepted when an authority the TLS library trusts signed it for the printer's name, or when it is the one the user chose to trust for that printer. Most printers make their own certificate, so the second is the usual case:

- the check in Printers and Queue still reads what the printer can do, and shows "Trust it to use it";
- **Use for printing** shows the certificate's fingerprint (SHA-256) and who it is issued to, and asks "Trust this printer?" ("Don't trust it" comes first);
- the answer is remembered in `ENVARC:GFOpenPrint/TrustedPrinters`, one line per printer (`host:port`, the fingerprint, the name);
- from then on that certificate, and only that one, is accepted for that printer. A different certificate is "Certificate changed since you trusted it": nothing is printed until the user looks at it and trusts the new one.

A certificate for another name, out of date (check the Amiga's clock), or unreadable is refused. A print job, or a password, only ever goes over a connection whose certificate was accepted.

**Logins.** A printer that answers 401 gets HTTP Basic or Digest (MD5), only over `ipps://` with an accepted certificate; over plain `ipp://` GFOpenPrint refuses to send a password and says to use the printer's `ipps://` address. The logins are in `ENVARC:GFOpenPrint/Logins`, one line per printer: `host:port`, a tab, the user name, a tab, the password (kept as plain text, as Amiga programs do; use a login for printing only). A 401 means the printer did not take the job, so the job is sent once more with the login; any other lost answer stays "uncertain" and is never resent by itself.

**Testing.** `make test-ipps` (`tests/host/test_ipps.sh`) runs GFOpenPrint's own connection, TLS, IPP and Print-Job code on x86 or ARM64 cores against printers on 127.0.0.1 only: CUPS's `ippeveprinter` with its own certificate, a second one with a certificate from a test authority, and a small printer that asks for a login (`tests/host/auth_printer.py`). It runs the GFOpenTLS backend, either GFOpenTLS's own code built for the host (`OPENTLS=` a gfopentls checkout with `build-host/` built) or its calls on OpenSSL (`tests/host/opentls_host.c`). `C:OAPIPPSTest` (`query`, `print`, `trust`) is the same tool for the Amiga.

## Licence

GFOpenPrint is free software under the MIT licence (`LICENSE`, Copyright (c) 2026 AmiCradle Team): anyone may use it, change it, fork it and ship it, commercially too. We, 4 October 2026: projects we call Open are MIT. It was BSD 2-Clause until then.

The licence's one condition keeps the credit: the copyright notice and the licence text stay with every copy and fork. We also ask, as a courtesy rather than a condition, that a fork or a port say it is based on GFOpenPrint by the AmiCradle Team.

The printer driver is written from scratch against the published classic Amiga printer-driver ABI; AROS sources were used as behavioural reference, not copied into this tree.

## Queue and graphics printing

GFOpenPrint now treats completed printer jobs as durable queue entries rather than transient one-shot files.

- Install `OAPSpooler` as a Workbench-startup tool (`SYS:WBStartup/OAPSpooler`) with `DONOTWAIT`. Ctrl-C ends it (GFOpenBench asks with Ctrl-C when it quits).
- The spooler stores completed jobs under `SYS:Spool/GFOpenPrint/` as a PDF spool artifact plus a `.job` metadata record.
- Install `GFOpenPrint` plus `GFOpenPrintTool.info` as `SYS:Tools/GFOpenPrint` and `SYS:Tools/GFOpenPrint.info`. Double-click it, or run `GFOpenPrint` with no arguments (or `GFOpenPrint QUEUE`), to open the native queue window.
- Selecting **Open** on a queued job opens the native per-job window, where the job can be saved/exported or sent to an IPP printer.
- `OAPImageTest` exercises the real `PRD_DUMPRPORT` graphics path. By default it prints a full-width, aspect-correct, centered colour test card; pass `1TO1` for a diagnostic unscaled dump.

The classic printer-driver graphics path uses the canonical ExecBase pointer at absolute address 4, as required by traditional Amiga printer-driver init glue. Raster transfer honours `pi_xpos` and `pi_ScaleX`, so printer.device controls rotation, centering and scaling according to the active printer preferences.

## PDF printer browser

The windows are GadTools (We, 4 October 2026: OS 3.x applications use GadTools or MUI, not ReAction), so they run on AmigaOS 3.0 and later. The `feature/pdf-printer-browser` integration adds `OAPPrinters`, an asynchronous DNS-SD discovery worker, explicit PDF capability checks, and Browse buttons in the print dialog and native viewer. Print-Job submission rechecks PDF support and validates a complete IPP response with a job identifier. See [PRINTER_BROWSER.md](docs/PRINTER_BROWSER.md) for installation, native validation status, and current protocol limits.

## Contributors

GFOpenPrint is created and maintained by [SacredTrees](https://github.com/SacredTrees) with The AmiCradle Team, copyright AmiCradle Team. Everyone whose work it includes is credited in [`CONTRIBUTORS.md`](CONTRIBUTORS.md).

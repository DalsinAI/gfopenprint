/* Copyright (c) 2026 AmiCradle Team. SPDX-License-Identifier: MIT */
#define _GNU_SOURCE
#include "oap_discovery.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void p16(unsigned char *p,unsigned x){p[0]=x>>8;p[1]=x;}
static size_t attr(unsigned char *p,size_t x,unsigned t,const char *key,const char *v){size_t a=strlen(key),b=strlen(v);p[x++]=t;p16(p+x,a);x+=2;memcpy(p+x,key,a);x+=a;p16(p+x,b);x+=2;memcpy(p+x,v,b);return x+b;}
static size_t response(unsigned char *p,const char *mime){size_t x=8;memset(p,0,1024);p[0]=1;p[1]=1;p[7]=42;p[x++]=4;x=attr(p,x,0x41,"printer-make-and-model","HP test");x=attr(p,x,0x49,"document-format-supported","image/pwg-raster");if(mime)x=attr(p,x,0x49,"",mime);p[x++]=3;return x;}
static size_t rr(unsigned char *p,size_t x,const OAPName *owner,unsigned t,const void *data,size_t n){memcpy(p+x,owner->wire,owner->len);x+=owner->len;p16(p+x,t);p16(p+x+2,0x8001);p[x+7]=120;p16(p+x+8,n);x+=10;memcpy(p+x,data,n);return x+n;}

/* GFO-254: a printer without PDF that takes PWG Raster, like the LaserJet M232 (sgray_8 at 300 and 600 dpi) */
static size_t res(unsigned char *p,size_t x,const char *key,unsigned long xd,unsigned long yd,unsigned units){size_t a=strlen(key);p[x++]=0x32;p16(p+x,a);x+=2;memcpy(p+x,key,a);x+=a;p16(p+x,9);x+=2;p[x]=xd>>24;p[x+1]=xd>>16;p[x+2]=xd>>8;p[x+3]=xd;p[x+4]=yd>>24;p[x+5]=yd>>16;p[x+6]=yd>>8;p[x+7]=yd;p[x+8]=units;return x+9;}
static size_t raster(unsigned char *p,const char *format,const unsigned long *dpi,size_t ndpi,const char *type1,const char *type2)
{
    size_t x=8,i;
    memset(p,0,1024);p[0]=1;p[1]=1;p[7]=42;p[x++]=4;
    x=attr(p,x,0x41,"printer-make-and-model","HP LaserJet M232-M237");
    x=attr(p,x,0x49,"document-format-supported","application/octet-stream");
    if(format)x=attr(p,x,0x49,"",format);
    x=attr(p,x,0x49,"","image/urf");x=attr(p,x,0x49,"","application/PCLm");
    for(i=0;i<ndpi;i++)x=res(p,x,i?"":"pwg-raster-document-resolution-supported",dpi[i],dpi[i],3);
    if(type1)x=attr(p,x,0x44,"pwg-raster-document-type-supported",type1);
    if(type2)x=attr(p,x,0x44,"",type2);
    p[x++]=3;return x;
}
static void raster_checks(OAPDiscovery *d)
{
    unsigned char ipp[1024];size_t n,i;OAPCaps caps;int done;char note[64];OAPDiscovery *e=calloc(1,sizeof(*e));
    static const unsigned long m232[]={300,600},hi[]={600,1200},low[]={150,600},tie[]={400,200},odd[]={50,2400};
    n=raster(ipp,"image/pwg-raster",m232,2,"sgray_8",NULL);
    CHECK(oap_ipp_parse_caps(ipp,n,42,&caps));
    CHECK(caps.pdf==OAP_PDF_NO&&caps.ps==OAP_PDF_NO&&caps.pwg==OAP_PDF_YES&&caps.pwg_dpi==300&&caps.pwg_types==OAP_PWG_GREY8);
    CHECK(oap_caps_route(&caps)==OAP_PDF_RASTER);
    for(i=0;i<n;i++)CHECK(!oap_ipp_parse_caps(ipp,i,42,&caps));
    n=raster(ipp,"image/pwg-raster",hi,2,"srgb_8","sgray_8");CHECK(oap_ipp_parse_caps(ipp,n,42,&caps));
    CHECK(caps.pwg==OAP_PDF_YES&&caps.pwg_dpi==600&&caps.pwg_types==(OAP_PWG_GREY8|OAP_PWG_RGB8));
    n=raster(ipp,"image/pwg-raster",low,2,"srgb_8",NULL);CHECK(oap_ipp_parse_caps(ipp,n,42,&caps));CHECK(caps.pwg_dpi==150&&caps.pwg_types==OAP_PWG_RGB8);
    n=raster(ipp,"image/pwg-raster",tie,2,"sgray_8",NULL);CHECK(oap_ipp_parse_caps(ipp,n,42,&caps));CHECK(caps.pwg_dpi==200);
    /* nothing GFOpenPrint can make: no raster format, no 8-bit type, no usable resolution */
    n=raster(ipp,NULL,m232,2,"sgray_8",NULL);CHECK(oap_ipp_parse_caps(ipp,n,42,&caps));CHECK(caps.pwg==OAP_PDF_NO&&!caps.pwg_dpi&&!caps.pwg_types&&oap_caps_route(&caps)==OAP_PDF_NO);
    n=raster(ipp,"image/pwg-raster",m232,2,"black_1","sgray_16");CHECK(oap_ipp_parse_caps(ipp,n,42,&caps));CHECK(caps.pwg==OAP_PDF_NO);
    n=raster(ipp,"image/pwg-raster",odd,2,"sgray_8",NULL);CHECK(oap_ipp_parse_caps(ipp,n,42,&caps));CHECK(caps.pwg==OAP_PDF_NO);
    n=raster(ipp,"image/pwg-raster",NULL,0,"sgray_8",NULL);CHECK(oap_ipp_parse_caps(ipp,n,42,&caps));CHECK(caps.pwg==OAP_PDF_NO);
    {size_t x=8;memset(ipp,0,1024);ipp[0]=1;ipp[1]=1;ipp[7]=42;ipp[x++]=4;x=attr(ipp,x,0x49,"document-format-supported","image/pwg-raster");
     x=res(ipp,x,"pwg-raster-document-resolution-supported",300,600,3);x=res(ipp,x,"",118,118,4);x=attr(ipp,x,0x44,"pwg-raster-document-type-supported","sgray_8");ipp[x++]=3;
     CHECK(oap_ipp_parse_caps(ipp,x,42,&caps));CHECK(caps.pwg==OAP_PDF_NO);}      /* not square; dots a cm */
    /* PDF wins when a printer takes both */
    {size_t x=8;memset(ipp,0,1024);ipp[0]=1;ipp[1]=1;ipp[7]=42;ipp[x++]=4;x=attr(ipp,x,0x49,"document-format-supported","application/pdf");x=attr(ipp,x,0x49,"","image/pwg-raster");
     x=res(ipp,x,"pwg-raster-document-resolution-supported",300,300,3);x=attr(ipp,x,0x44,"pwg-raster-document-type-supported","sgray_8");ipp[x++]=3;
     CHECK(oap_ipp_parse_caps(ipp,x,42,&caps));CHECK(caps.pdf==OAP_PDF_YES&&caps.pwg==OAP_PDF_YES&&oap_caps_route(&caps)==OAP_PDF_YES);}
    /* the query asks for both raster attributes */
    n=oap_ipp_query_request("ipp://m232.local:631/ipp/print",9,ipp,sizeof(ipp));CHECK(n);
    CHECK(memmem(ipp,n,"pwg-raster-document-resolution-supported",40)&&memmem(ipp,n,"pwg-raster-document-type-supported",34));
    /* saved and read back as a raster printer, and usable */
    memset(d,0,sizeof(*d));d->count=1;d->printers[0].alive=1;strcpy(d->printers[0].uri,"ipp://m232.local:631/ipp/print");
    n=raster(ipp,"image/pwg-raster",m232,2,"sgray_8",NULL);CHECK(oap_ipp_parse_caps(ipp,n,42,&d->printers[0].caps));
    CHECK(oap_pdf_eligible(&d->printers[0]));
    CHECK(oap_discovery_save("build/test-disc-raster.tsv",d,1,"done"));
    CHECK(oap_discovery_load("build/test-disc-raster.tsv",e,&done,note,sizeof(note)));
    CHECK(e->count==1&&e->printers[0].caps.pdf==OAP_PDF_NO&&e->printers[0].caps.pwg==OAP_PDF_YES&&oap_caps_route(&e->printers[0].caps)==OAP_PDF_RASTER&&oap_pdf_eligible(&e->printers[0]));
    d->printers[0].caps.pwg=OAP_PDF_NO;CHECK(!oap_pdf_eligible(&d->printers[0]));
    CHECK(oap_discovery_save("build/test-disc-raster.tsv",d,1,"done"));CHECK(oap_discovery_load("build/test-disc-raster.tsv",e,&done,note,sizeof(note)));
    CHECK(e->printers[0].caps.pdf==OAP_PDF_NO&&e->printers[0].caps.pwg!=OAP_PDF_YES);
    {FILE *f=fopen("build/test-disc-raster.tsv","w");CHECK(f);fputs("OAPB1\t1\tdone\nP\t9\t1\t0\tX\tipp://x:631/\t\n",f);fclose(f);}
    CHECK(oap_discovery_load("build/test-disc-raster.tsv",e,&done,note,sizeof(note)));CHECK(e->printers[0].caps.pdf==OAP_PDF_UNKNOWN&&!oap_pdf_eligible(&e->printers[0]));
    remove("build/test-disc-raster.tsv");free(e);
}
static size_t int_attr(unsigned char *p,size_t x,unsigned t,const char *key,unsigned long v){size_t a=strlen(key);p[x++]=t;p16(p+x,a);x+=2;memcpy(p+x,key,a);x+=a;p16(p+x,4);x+=2;p[x]=v>>24;p[x+1]=v>>16;p[x+2]=v>>8;p[x+3]=v;return x+4;}
/* Get-Job-Attributes: the request, the answer, and what the queue says (24 Nov 2026) */
static size_t job_answer(unsigned char *p,unsigned long state,unsigned long pages,const char *reason,const char *message)
{size_t x=8;memset(p,0,1024);p[0]=1;p[1]=1;p[7]=77;p[x++]=1;x=attr(p,x,0x47,"attributes-charset","utf-8");p[x++]=2;x=int_attr(p,x,0x23,"job-state",state);x=int_attr(p,x,0x21,"job-impressions-completed",pages);if(reason)x=attr(p,x,0x44,"job-state-reasons",reason);if(reason)x=attr(p,x,0x44,"","job-printing");if(message)x=attr(p,x,0x41,"job-state-message",message);p[x++]=3;return x;}
static void job_checks(void)
{
    unsigned char p[1024];char words[128];OAPJobState js;size_t n,i;
    n=oap_ipp_job_request("ipp://192.168.0.6:631/ipp/print",77,14,p,sizeof(p));
    CHECK(n>0&&p[3]==9&&p[7]==77&&p[n-1]==3);
    {size_t k;int found=0;for(k=9;k+14<n;k++)if(p[k]==0x21&&!memcmp(p+k+3,"job-id",6)&&p[k+14]==14&&!p[k+13]&&!p[k+12]&&!p[k+11])found=1;CHECK(found);}
    CHECK(!oap_ipp_job_request("ipp://192.168.0.6:631/ipp/print",77,14,p,40));CHECK(!oap_ipp_job_request(NULL,77,14,p,sizeof(p)));
    n=job_answer(p,9,1,NULL,NULL);CHECK(oap_ipp_parse_job(p,n,77,&js));CHECK(js.state==9&&js.pages==1&&!js.reasons[0]);
    CHECK(oap_job_state_words(&js,words,sizeof(words))==1);CHECK(!strcmp(words,"Printed: 1 page"));
    js.pages=3;oap_job_state_words(&js,words,sizeof(words));CHECK(!strcmp(words,"Printed: 3 pages"));
    for(i=0;i<n;i++)CHECK(!oap_ipp_parse_job(p,i,77,&js));
    CHECK(!oap_ipp_parse_job(p,n,78,&js));
    n=job_answer(p,5,0,"job-incoming",NULL);CHECK(oap_ipp_parse_job(p,n,77,&js));CHECK(js.state==5&&!strcmp(js.reasons,"job-incoming, job-printing"));
    CHECK(oap_job_state_words(&js,words,sizeof(words))==0);CHECK(!strcmp(words,"Printing"));
    n=job_answer(p,3,0,NULL,NULL);CHECK(oap_ipp_parse_job(p,n,77,&js));oap_job_state_words(&js,words,sizeof(words));CHECK(!strcmp(words,"Waiting at the printer"));
    n=job_answer(p,8,0,"document-format-error",NULL);CHECK(oap_ipp_parse_job(p,n,77,&js));CHECK(oap_job_state_words(&js,words,sizeof(words))==1);CHECK(!strcmp(words,"Failed at the printer: document-format-error, job-printing"));
    n=job_answer(p,7,0,"job-canceled-at-device","Cancelled on the panel");CHECK(oap_ipp_parse_job(p,n,77,&js));oap_job_state_words(&js,words,sizeof(words));CHECK(!strcmp(words,"Cancelled at the printer: Cancelled on the panel"));
    n=job_answer(p,12,0,NULL,NULL);CHECK(!oap_ipp_parse_job(p,n,77,&js));                     /* no such state */
    n=job_answer(p,9,1,NULL,NULL);p[2]=4;p[3]=6;CHECK(!oap_ipp_parse_job(p,n,77,&js));CHECK(js.status==0x0406);   /* client-error-not-found */
    memset(&js,0,sizeof(js));oap_job_state_words(&js,words,sizeof(words));CHECK(!strcmp(words,"Not known"));
}
int main(void){unsigned char ipp[1024],http[4096],out[2048],pkt[2048],rd[512];OAPCaps caps;size_t n,hn,bn,i,x;int status;OAPName service,host,base;OAPDiscovery *d=calloc(1,sizeof(*d));
 n=response(ipp,"application/pdf");CHECK(oap_ipp_parse_caps(ipp,n,42,&caps));CHECK(caps.pdf==OAP_PDF_YES);CHECK(!strcmp(caps.model,"HP test"));for(i=0;i<n;i++)CHECK(!oap_ipp_parse_caps(ipp,i,42,&caps));CHECK(!oap_ipp_parse_caps(ipp,n,43,&caps));
 n=response(ipp,"application/pdf-invalid");CHECK(oap_ipp_parse_caps(ipp,n,42,&caps));CHECK(caps.pdf==OAP_PDF_NO);
 n=response(ipp,NULL);CHECK(oap_ipp_parse_caps(ipp,n,42,&caps));CHECK(caps.pdf==OAP_PDF_NO);
 memset(ipp,0,9);ipp[0]=1;ipp[7]=42;ipp[8]=3;CHECK(oap_ipp_parse_caps(ipp,9,42,&caps));CHECK(caps.pdf==OAP_PDF_UNKNOWN);
 /* PostScript, for a program's own (27 Oct 2026): listed, not listed, not said */
 CHECK(caps.ps==OAP_PDF_UNKNOWN);n=response(ipp,"application/postscript");CHECK(oap_ipp_parse_caps(ipp,n,42,&caps));CHECK(caps.ps==OAP_PDF_YES&&caps.pdf==OAP_PDF_NO);
 n=response(ipp,"application/pdf");CHECK(oap_ipp_parse_caps(ipp,n,42,&caps));CHECK(caps.ps==OAP_PDF_NO&&caps.pdf==OAP_PDF_YES);
 n=response(ipp,"application/postscript-x");CHECK(oap_ipp_parse_caps(ipp,n,42,&caps));CHECK(caps.ps==OAP_PDF_NO);
 n=response(ipp,"application/pdf");ipp[2]=4;CHECK(!oap_ipp_parse_caps(ipp,n,42,&caps));ipp[2]=0;
 hn=(size_t)sprintf((char *)http,"HTTP/1.1 200 OK\r\nContent-Type: application/ipp\r\nContent-Length: %lu\r\n\r\n",(unsigned long)n);memcpy(http+hn,ipp,n);hn+=n;for(i=0;i<hn;i++)CHECK(oap_http_response(http,i,0,out,sizeof(out),&bn,&status)==0);CHECK(oap_http_response(http,hn,0,out,sizeof(out),&bn,&status)==1);CHECK(bn==n&&!memcmp(ipp,out,n));CHECK(oap_http_response(http,hn-1,1,out,sizeof(out),&bn,&status)==-1);
 hn=(size_t)sprintf((char *)http,"HTTP/1.1 100 Continue\r\n\r\nHTTP/1.1 200 OK\r\nContent-Type: application/ipp\r\nTransfer-Encoding: chunked\r\n\r\n%lx\r\n",(unsigned long)n);memcpy(http+hn,ipp,n);hn+=n;memcpy(http+hn,"\r\n0\r\n\r\n",7);hn+=7;for(i=0;i<hn;i++)CHECK(oap_http_response(http,i,0,out,sizeof(out),&bn,&status)==0);CHECK(oap_http_response(http,hn,0,out,sizeof(out),&bn,&status)==1);CHECK(bn==n&&!memcmp(ipp,out,n));
 strcpy((char *)http,"HTTP/1.1 200 OK\r\nContent-Type: application/ipp\r\nContent-Length: 1\r\nContent-Length: 2\r\n\r\nAB");CHECK(oap_http_response(http,strlen((char *)http),1,out,sizeof(out),&bn,&status)==-1);
 oap_dns_name_text("HP LaserJet._ipp._tcp.local",&service);oap_dns_name_text("hp.local",&host);oap_dns_name_text("_ipp._tcp.local",&base);memset(pkt,0,sizeof(pkt));pkt[2]=0x84;p16(pkt+6,4);x=12;
 {unsigned char ip[4]={192,168,0,6};x=rr(pkt,x,&host,1,ip,4);}rd[0]=12;memcpy(rd+1,"rp=ipp/print",12);x=rr(pkt,x,&service,16,rd,13);x=rr(pkt,x,&base,12,service.wire,service.len);memset(rd,0,6);p16(rd+4,8631);memcpy(rd+6,host.wire,host.len);x=rr(pkt,x,&service,33,rd,6+host.len);
 CHECK(oap_dns_packet(d,pkt,x));CHECK(d->count==1);CHECK(!strcmp(d->printers[0].uri,"ipp://hp.local:8631/ipp/print"));CHECK(!strcmp(d->printers[0].ip,"192.168.0.6"));strcpy(d->printers[0].host,"bad name");oap_discovery_resolve(d);CHECK(!strcmp(d->printers[0].uri,"ipp://192.168.0.6:8631/ipp/print"));strcpy(d->printers[0].host,"hp.local");oap_discovery_resolve(d);CHECK(!oap_pdf_eligible(&d->printers[0]));d->printers[0].caps.pdf=OAP_PDF_YES;CHECK(oap_pdf_eligible(&d->printers[0]));d->printers[0].secure=1;CHECK(!oap_pdf_eligible(&d->printers[0]));
 for(i=0;i<x;i++){OAPDiscovery *bad=calloc(1,sizeof(*bad));CHECK(!oap_dns_packet(bad,pkt,i));CHECK(bad->count==0);free(bad);}
 memset(pkt,0,20);pkt[2]=0x84;p16(pkt+6,1);pkt[12]=0xc0;pkt[13]=12;CHECK(!oap_dns_packet(d,pkt,20));
 n=oap_ipp_query_request("ipp://192.168.0.6:631/ipp/print",42,ipp,sizeof(ipp));CHECK(n>0&&ipp[3]==11&&ipp[n-1]==3);CHECK(!oap_ipp_query_request("ipp://host/print",42,ipp,8));
 /* Print-Job acknowledgements must carry a valid integer job-id. */
 memset(ipp,0,32);ipp[0]=1;ipp[1]=1;ipp[7]=42;ipp[8]=2;ipp[9]=0x21;p16(ipp+10,6);memcpy(ipp+12,"job-id",6);p16(ipp+18,4);ipp[23]=7;ipp[24]=3;
 CHECK(oap_ipp_parse_caps(ipp,25,42,&caps));CHECK(caps.job_id==7);CHECK(!oap_ipp_parse_caps(ipp,24,42,&caps));
 job_checks();raster_checks(d);free(d);printf("PASS: %u discovery, MIME gating, DNS bounds, complete HTTP and IPP checks\n",checks);return 0;}

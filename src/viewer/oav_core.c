/* Copyright (c) 2026 AmiCradle Team. SPDX-License-Identifier: MIT */
#include "oav_core.h"
#include "oap_str.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <stdint.h>
void oav_layout_defaults(OAVLayout *s){memset(s,0,sizeof(*s));s->margin_cpt=3600;}
int oav_safe_field(const char *s)
{
    size_t n=0;if(!s||!*s)return 0;
    while(*s){unsigned char c=(unsigned char)*s++;if(c<32||c==127||++n>=OAV_PATH_MAX)return 0;}
    return 1;
}
static int extis(const char *s,const char *x)
{
    const char *p=strrchr(s?s:"",'.');if(!p)return 0;
    do {if(tolower((unsigned char)*p++)!=tolower((unsigned char)*x++))return 0;}while(*x);
    return *p==0;
}
const char *oav_format_note(const char *p)
{
    if(extis(p,".docx"))return "DOCX needs an OOXML document renderer; text extraction is not full layout.";
    if(extis(p,".pptx"))return "PPTX needs an OOXML slide renderer; slide text alone is not a slide.";
    if(extis(p,".webp"))return "WebP requires a picture provider; animated WebP additionally needs playback.";
    if(extis(p,".gif"))return "GIF loaded as a picture is still-only; animation requires an animation provider.";
    if(extis(p,".pdf"))return "PDF viewing depends on the installed PDF datatype; PDF reflow is not supported.";
    if(extis(p,".mp4")||extis(p,".avi")||extis(p,".mkv")||extis(p,".mov"))return "Video requires both a container reader and the codecs used inside this file.";
    return "A compatible installed datatype is required.";
}
/* Geometry is hundredths of a point; no FPU or math-library dependency. */
long oav_scale(long value,long numerator,long denominator)
{
 long sign=value<0?-1:1;unsigned long v=(unsigned long)(value<0?-value:value);
 if(numerator<0||denominator<=0)return 0;
 return sign*(long)((v/(unsigned long)denominator)*(unsigned long)numerator+
       ((v%(unsigned long)denominator)*(unsigned long)numerator)/(unsigned long)denominator);
}
int oav_parse_points(const char *s,long *cpt)
{
 unsigned long whole=0,frac=0,n=0;if(!s||!cpt||!*s)return 0;
 while(*s>='0'&&*s<='9'){whole=whole*10+(unsigned long)(*s++-'0');if(whole>144)return 0;n++;}
 if(!n)return 0;
 if(*s=='.'){s++;if(*s<'0'||*s>'9')return 0;frac=(unsigned long)(*s++-'0')*10;if(*s>='0'&&*s<='9')frac+=(unsigned long)(*s++-'0');}
 if(*s||whole*100+frac>14400)return 0;
 *cpt=(long)(whole*100+frac);return 1;
}
int oav_place(const OAVLayout *s,unsigned long w,unsigned long h,OAVPlacement *p)
{
 long t;int bywidth;
 if(!s||!p||!w||!h||w>OAV_DIM_LIMIT||h>OAV_DIM_LIMIT||w>OAV_PIXEL_LIMIT/h)return 0;
 if(s->paper!=OAV_A4&&s->paper!=OAV_LETTER)return 0;
 if(s->landscape!=0&&s->landscape!=1)return 0;
 if(s->scale!=OAV_FIT&&s->scale!=OAV_FILL)return 0;
 p->page_w=s->paper==OAV_LETTER?61200:59528;p->page_h=s->paper==OAV_LETTER?79200:84189;
 if(s->landscape){t=p->page_w;p->page_w=p->page_h;p->page_h=t;}
 if(s->margin_cpt<0||s->margin_cpt>=p->page_w/2||s->margin_cpt>=p->page_h/2)return 0;
 p->clip_x=p->clip_y=s->margin_cpt;p->clip_w=p->page_w-2*s->margin_cpt;p->clip_h=p->page_h-2*s->margin_cpt;
 bywidth=(unsigned long)p->clip_w*h<=(unsigned long)p->clip_h*w;if(s->scale==OAV_FILL)bywidth=!bywidth;
 if(bywidth){p->w=p->clip_w;p->h=(long)((h*(unsigned long)p->clip_w+w/2)/w);}
 else{p->h=p->clip_h;p->w=(long)((w*(unsigned long)p->clip_h+h/2)/h);}
 p->x=(p->page_w-p->w)/2;p->y=(p->page_h-p->h)/2;return 1;
}
static void point_string(char *out,long v)
{
 unsigned long a=(unsigned long)(v<0?-v:v);
 /* snprintf: amiga.lib's sprintf (RawDoFmt) can take C's place at link time */
 snprintf(out,24,"%s%lu.%02lu",v<0?"-":"",a/100,a%100);
}
static void fail(char *e,size_t n,const char *s){oap_copy(e,n,s);}
/* ---- A picture in a PDF: no more pixels than the paper takes, Flate-compressed ----------------------
 * A zlib stream (RFC 1950/1951) written as it comes: LZ77 over a 32 KiB window, each block with its own
 * Huffman codes. A picture's rows go in PNG-filtered (/Predictor 15), which is what makes photos shrink. */
#define ZW 32768UL
#define ZMAXD (ZW-262UL)
#define ZBLK 16384U
#define ZHASH(p) ((((unsigned)(p)[0]<<10)^((unsigned)(p)[1]<<5)^(unsigned)(p)[2])&32767U)
typedef struct Zs {
 FILE *f;int bad;unsigned long bits;int nbits;unsigned long a,b,len,pos;unsigned nsym,adler_n;
 unsigned char win[2*ZW];unsigned short head[32768],prev[ZW],sym[ZBLK],dist[ZBLK];
} Zs;
static const unsigned short zlb[29]={3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
static const unsigned char zle[29]={0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
static const unsigned short zdb[30]={1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
static const unsigned char zde[30]={0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};
static const unsigned char zord[19]={16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
static void zput(Zs *z,unsigned long v,int n)
{
 z->bits|=v<<z->nbits;z->nbits+=n;
 while(z->nbits>=8){if(putc((int)(z->bits&255),z->f)==EOF)z->bad=1;z->bits>>=8;z->nbits-=8;}
}
static int zlcode(unsigned l){int c=28;while(zlb[c]>l)c--;return c;}
static int zdcode(unsigned d){int c=29;while(zdb[c]>d)c--;return c;}
/* Huffman code lengths for freq[0..n), none over lim bits: when one is, the counts are flattened and it is
 * built again. Two queues (sorted leaves, then the joined nodes in the order made): no sort per join. */
static void zlengths(const unsigned long *freq,int n,int lim,unsigned char *len)
{
 unsigned long s[288],iw[288],v;short leaf[288],par[576],depth[576];int i,j,m,k,a,b,t,pick[2],shift=0,over;
 for(;;){
  for(m=i=0;i<n;i++){
   len[i]=0;if(!freq[i])continue;
   v=(freq[i]>>shift)|1;for(j=m;j>0&&s[j-1]>v;j--){s[j]=s[j-1];leaf[j]=leaf[j-1];}
   s[j]=v;leaf[j]=(short)i;m++;
  }
  if(m<2){if(m)len[leaf[0]]=1;return;}
  for(a=b=k=0;k<m-1;k++){
   for(t=0;t<2;t++)pick[t]=a<m&&(b>=k||s[a]<=iw[b])?a++:m+b++;
   iw[k]=(pick[0]<m?s[pick[0]]:iw[pick[0]-m])+(pick[1]<m?s[pick[1]]:iw[pick[1]-m]);
   par[pick[0]]=par[pick[1]]=(short)(m+k);
  }
  depth[2*m-2]=0;for(k=2*m-3;k>=m;k--)depth[k]=(short)(depth[par[k]]+1);
  for(over=i=0;i<m;i++){int d=depth[par[i]]+1;if(d>lim)over=1;len[leaf[i]]=(unsigned char)d;}
  if(!over)return;
  shift++;
 }
}
static void zcodes(const unsigned char *len,int n,unsigned short *code)
{
 unsigned next[16],c=0,v,r;int cnt[16],i,b;
 memset(cnt,0,sizeof(cnt));for(i=0;i<n;i++)cnt[len[i]]++;cnt[0]=0;
 for(b=1;b<16;b++){c=(c+(unsigned)cnt[b-1])<<1;next[b]=c;}
 for(i=0;i<n;i++)if(len[i]){v=next[len[i]]++;for(r=0,b=0;b<len[i];b++){r=(r<<1)|(v&1);v>>=1;}code[i]=(unsigned short)r;}
}
/* at least two codes in an alphabet, so every inflater takes it */
static void ztwo(unsigned long *f,int n){int i,u=0;for(i=0;i<n;i++)u+=f[i]!=0;if(u<2){if(!f[0])f[0]=1;if(!f[1])f[1]=1;}}
static void zblock(Zs *z,int last)
{
 unsigned long lf[286],df[30],cf[19];unsigned char ll[286],dl[30],cl[19],seq[316],rs[316],rx[316];
 unsigned short lc[286],dc[30],cc[19];unsigned i;int hlit=286,hdist=30,hclen=19,n,nr=0,k,c;
 memset(lf,0,sizeof(lf));memset(df,0,sizeof(df));memset(cf,0,sizeof(cf));
 for(i=0;i<z->nsym;i++){
  if(z->sym[i]<256)lf[z->sym[i]]++;
  else{lf[257+zlcode(z->sym[i]-256U)]++;df[zdcode(z->dist[i])]++;}
 }
 lf[256]=1;ztwo(lf,286);ztwo(df,30);
 zlengths(lf,286,15,ll);zlengths(df,30,15,dl);
 while(hlit>257&&!ll[hlit-1])hlit--;
 while(hdist>1&&!dl[hdist-1])hdist--;
 memcpy(seq,ll,(size_t)hlit);memcpy(seq+hlit,dl,(size_t)hdist);n=hlit+hdist;
 for(k=0;k<n;){                                 /* the lengths, run-length coded (16, 17, 18) */
  int run=1,r;while(k+run<n&&seq[k+run]==seq[k])run++;
  if(!seq[k]&&run>=3){r=run>138?138:run;rs[nr]=r>=11?18:17;rx[nr++]=(unsigned char)(r>=11?r-11:r-3);k+=r;}
  else if(seq[k]&&run>=4){r=run-1>6?6:run-1;rs[nr]=seq[k];rx[nr++]=0;rs[nr]=16;rx[nr++]=(unsigned char)(r-3);k+=1+r;}
  else{rs[nr]=seq[k];rx[nr++]=0;k++;}
 }
 for(k=0;k<nr;k++)cf[rs[k]]++;
 ztwo(cf,19);zlengths(cf,19,7,cl);
 while(hclen>4&&!cl[zord[hclen-1]])hclen--;
 zcodes(ll,286,lc);zcodes(dl,30,dc);zcodes(cl,19,cc);
 zput(z,(unsigned long)last,1);zput(z,2,2);zput(z,(unsigned long)(hlit-257),5);zput(z,(unsigned long)(hdist-1),5);zput(z,(unsigned long)(hclen-4),4);
 for(k=0;k<hclen;k++)zput(z,cl[zord[k]],3);
 for(k=0;k<nr;k++){
  zput(z,cc[rs[k]],cl[rs[k]]);
  if(rs[k]==16)zput(z,rx[k],2);else if(rs[k]==17)zput(z,rx[k],3);else if(rs[k]==18)zput(z,rx[k],7);
 }
 for(i=0;i<z->nsym;i++){
  unsigned s=z->sym[i];
  if(s<256){zput(z,lc[s],ll[s]);continue;}
  s-=256;c=zlcode(s);zput(z,lc[257+c],ll[257+c]);if(zle[c])zput(z,s-zlb[c],zle[c]);
  s=z->dist[i];c=zdcode(s);zput(z,dc[c],dl[c]);if(zde[c])zput(z,s-zdb[c],zde[c]);
 }
 zput(z,lc[256],ll[256]);z->nsym=0;
}
static void zinsert(Zs *z,unsigned long i){unsigned h=ZHASH(z->win+i);z->prev[i&(ZW-1)]=z->head[h];z->head[h]=(unsigned short)i;}
/* Codes what is in the window: all of it when flushing, else up to the last 262 bytes (a match's room). */
static void zrun(Zs *z,int flush)
{
 while(z->pos<z->len&&(flush||z->len-z->pos>=262)){
  unsigned long p=z->pos,avail=z->len-p,best=0,bd=0,c,k,max;int chain=24;
  if(avail>=3){
   const unsigned char *x=z->win+p,*y;
   max=avail>258?258:avail;c=z->head[ZHASH(x)];zinsert(z,p);
   while(c&&c<p&&p-c<=ZMAXD&&chain--){
    y=z->win+c;
    if(y[best]==x[best]){for(k=0;k<max&&x[k]==y[k];k++);if(k>best){best=k;bd=p-c;if(k==max)break;}}
    c=z->prev[c&(ZW-1)];
   }
  }
  if(best>=3){
   z->sym[z->nsym]=(unsigned short)(256+best);z->dist[z->nsym++]=(unsigned short)bd;
   for(k=1;k<best;k++)if(p+k+2<z->len)zinsert(z,p+k);
   z->pos+=best;
  }else{z->sym[z->nsym]=z->win[p];z->dist[z->nsym++]=0;z->pos++;}
  if(z->nsym==ZBLK)zblock(z,0);
 }
}
static void zfeed(Zs *z,const unsigned char *d,unsigned long n)
{
 unsigned long k,i;
 while(n){
  k=2*ZW-z->len;if(k>n)k=n;
  memcpy(z->win+z->len,d,k);
  for(i=0;i<k;i++){z->a+=d[i];z->b+=z->a;if(++z->adler_n==5552){z->a%=65521UL;z->b%=65521UL;z->adler_n=0;}}
  z->len+=k;d+=k;n-=k;zrun(z,0);
  if(z->len==2*ZW){                              /* slide: keep the last 32 KiB */
   memmove(z->win,z->win+ZW,ZW);z->len-=ZW;z->pos-=ZW;
   for(i=0;i<32768;i++)z->head[i]=(unsigned short)(z->head[i]>=ZW?z->head[i]-ZW:0);
   for(i=0;i<ZW;i++)z->prev[i]=(unsigned short)(z->prev[i]>=ZW?z->prev[i]-ZW:0);
  }
 }
}
static void zend(Zs *z)
{
 int i;zrun(z,1);zblock(z,1);if(z->nbits)zput(z,0,8-z->nbits);
 z->a%=65521UL;z->b%=65521UL;
 for(i=3;i>=0;i--)if(putc((int)((((z->b<<16)|z->a)>>(i*8))&255),z->f)==EOF)z->bad=1;
}
/* A PNG filter for a row of RGB (3 bytes a pixel): the one with the smallest sum of residuals. */
static unsigned predict(int t,const unsigned char *cur,const unsigned char *up,size_t i)
{
 unsigned a=i>=3?cur[i-3]:0,b=up[i],c=i>=3?up[i-3]:0;int p,pa,pb,pc;
 switch(t){case 0:return 0;case 1:return a;case 2:return b;case 3:return (a+b)>>1;default:break;}
 p=(int)(a+b)-(int)c;pa=abs(p-(int)a);pb=abs(p-(int)b);pc=abs(p-(int)c);
 return pa<=pb&&pa<=pc?a:pb<=pc?b:c;
}
static void png_row(const unsigned char *cur,const unsigned char *up,size_t n,unsigned char *out)
{
 unsigned long best=~0UL,sum;int t,bt=0;size_t i;unsigned r;
 for(t=0;t<5;t++){
  for(sum=0,i=0;i<n&&sum<best;i++){r=(cur[i]-predict(t,cur,up,i))&255;sum+=r<128?r:256-r;}
  if(sum<best){best=sum;bt=t;}
 }
 out[0]=(unsigned char)bt;for(i=0;i<n;i++)out[1+i]=(unsigned char)(cur[i]-predict(bt,cur,up,i));
}
/* Pixels across a printed length (hundredths of a point) at OAV_PRINT_DPI; never more than the picture has. */
static unsigned long print_pixels(long cpt,unsigned long have)
{
 long n=oav_scale(cpt,OAV_PRINT_DPI,7200);
 return n<1?1:(unsigned long)n>have?have:(unsigned long)n;
}
int oav_pdf_rgb(FILE *f,unsigned long w,unsigned long h,const OAVLayout *s,
               OAVReadRow readrow,void *ctx,char *err,size_t cap)
{
    OAVPlacement p;long off[8],xref,start,end;unsigned char *row=NULL,*cur=NULL,*up=NULL,*out=NULL;
    unsigned long *acc=NULL,*xb=NULL,y,ow,oh,ox,oy,x,y1,c;Zs *z=NULL;
    char content[512],numbers[10][24];int clen,i,ok=0;size_t rowbytes,obytes;
    if(!f||!readrow||!oav_place(s,w,h,&p)){fail(err,cap,"Invalid dimensions or page layout");return 0;}
    if(w>SIZE_MAX/3){fail(err,cap,"Image row exceeds address range");return 0;}
    ow=print_pixels(p.w,w);oh=print_pixels(p.h,h);
    rowbytes=(size_t)w*3;obytes=(size_t)ow*3;
    row=malloc(rowbytes);cur=calloc(1,obytes);up=calloc(1,obytes);out=malloc(obytes+1);
    acc=malloc(obytes*sizeof(*acc));xb=malloc((ow+1)*sizeof(*xb));z=calloc(1,sizeof(*z));
    if(!row||!cur||!up||!out||!acc||!xb||!z){fail(err,cap,"Not enough memory for image row");goto out;}
    for(ox=0;ox<=ow;ox++)xb[ox]=ox*w/ow;
    point_string(numbers[0],p.clip_x);point_string(numbers[1],p.clip_y);
    point_string(numbers[2],p.clip_w);point_string(numbers[3],p.clip_h);
    point_string(numbers[4],p.w);point_string(numbers[5],p.h);
    point_string(numbers[6],p.x);point_string(numbers[7],p.y);
    point_string(numbers[8],p.page_w);point_string(numbers[9],p.page_h);
    clen=snprintf(content,sizeof(content),"q\n%s %s %s %s re W n\n%s 0 0 %s %s %s cm\n/Im0 Do\nQ\n",
        numbers[0],numbers[1],numbers[2],numbers[3],numbers[4],numbers[5],numbers[6],numbers[7]);
    if(clen<0||(size_t)clen>=sizeof(content)){fail(err,cap,"Page command overflow");goto out;}
    fputs("%PDF-1.4\n% GFOpenView RGB export\n",f);
    off[1]=ftell(f);fputs("1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n",f);
    off[2]=ftell(f);fputs("2 0 obj\n<< /Type /Pages /Kids [3 0 R] /Count 1 >>\nendobj\n",f);
    off[3]=ftell(f);fprintf(f,"3 0 obj\n<< /Type /Page /Parent 2 0 R /MediaBox [0 0 %s %s] /Resources << /XObject << /Im0 5 0 R >> >> /Contents 4 0 R >>\nendobj\n",numbers[8],numbers[9]);
    off[4]=ftell(f);fprintf(f,"4 0 obj\n<< /Length %d >>\nstream\n",clen);fwrite(content,1,(size_t)clen,f);fputs("\nendstream\nendobj\n",f);
    /* the picture at the paper's resolution: each pixel the average of the source pixels it covers */
    off[5]=ftell(f);fprintf(f,"5 0 obj\n<< /Type /XObject /Subtype /Image /Width %lu /Height %lu /ColorSpace /DeviceRGB /BitsPerComponent 8 "
        "/Filter /FlateDecode /DecodeParms << /Predictor 15 /Colors 3 /BitsPerComponent 8 /Columns %lu >> /Length 6 0 R >>\nstream\n",ow,oh,ow);
    start=ftell(f);z->f=f;z->a=1;putc(0x78,f);putc(0x01,f);
    for(y=oy=0;oy<oh;oy++){
        unsigned long n,cnt;unsigned char *t;
        memset(acc,0,obytes*sizeof(*acc));
        for(y1=(oy+1)*h/oh;y<y1;y++){
            if(!readrow(ctx,y,row,rowbytes)){fail(err,cap,"Decode cancelled or pixel read failed");goto out;}
            for(ox=0;ox<ow;ox++)for(x=xb[ox];x<xb[ox+1];x++)for(c=0;c<3;c++)acc[ox*3+c]+=row[x*3+c];
        }
        n=y1-oy*h/oh;
        for(ox=0;ox<ow;ox++){cnt=(xb[ox+1]-xb[ox])*n;for(c=0;c<3;c++)cur[ox*3+c]=(unsigned char)((acc[ox*3+c]+cnt/2)/cnt);}
        png_row(cur,up,obytes,out);zfeed(z,out,(unsigned long)obytes+1);
        t=up;up=cur;cur=t;
        if(z->bad){fail(err,cap,"Output write failed");goto out;}
    }
    zend(z);end=ftell(f);
    fputs("\nendstream\nendobj\n",f);
    off[6]=ftell(f);fprintf(f,"6 0 obj\n%ld\nendobj\n",end-start);
    xref=ftell(f);fputs("xref\n0 7\n0000000000 65535 f \n",f);
    for(i=1;i<=6;i++){if(off[i]<0){fail(err,cap,"Output position failed");goto out;}fprintf(f,"%010ld 00000 n \n",off[i]);}
    fprintf(f,"trailer\n<< /Size 7 /Root 1 0 R >>\nstartxref\n%ld\n%%%%EOF\n",xref);
    if(z->bad||start<0||end<0||xref<0||ferror(f)||fflush(f)){fail(err,cap,"PDF finalisation failed");goto out;}
    fail(err,cap,"Saved");ok=1;
out:
    free(row);free(cur);free(up);free(out);free(acc);free(xb);free(z);
    return ok;
}
/* ---- Find ---------------------------------------------------------------- */
static int lc1(unsigned char c)
{
 if(c>='A'&&c<='Z')return c+32;
 if(c>=0xc0&&c<=0xde&&c!=0xd7)return c+32;      /* Latin-1 capitals */
 return c;
}
static int at_word(const char *b,long len,long i,const char *w)
{
 while(*w){if(i>=len||lc1((unsigned char)b[i])!=lc1((unsigned char)*w))return 0;i++;w++;}
 return 1;
}
int oav_is_guide(const char *buf,long len)
{
 long i=0;
 while(i<len&&(buf[i]==' '||buf[i]=='\t'||buf[i]=='\r'||buf[i]=='\n'))i++;
 return at_word(buf,len,i,"@database");
}
/* A guide line that is a command (@node, @title, ...), not shown; "@{" is a link inside text. */
static int command_line(const char *b,long len,long s)
{
 return s<len&&b[s]=='@'&&!(s+1<len&&b[s+1]=='{');
}
static int match_at(const char *b,long len,long i,const char *w,long wl)
{
 long k;
 if(i+wl>len)return 0;
 for(k=0;k<wl;k++)if(lc1((unsigned char)b[i+k])!=lc1((unsigned char)w[k]))return 0;
 return 1;
}
/* Fills in the line (and for a guide the node) of the match at `at`; 0 when a
 * guide does not show that place (a command line, or outside any node). */
static int place(const char *b,long len,long at,int guide,OAVFound *f)
{
 long i,s=0,line=0,nline=0;int innode=0;
 f->node[0]=0;
 for(i=0;i<at;i++){
  if(b[i]!='\n')continue;
  if(guide){
   if(at_word(b,len,s,"@node")){
    long p=s+5,n=0;char end=' ';
    while(p<len&&(b[p]==' '||b[p]=='\t'))p++;
    if(p<len&&b[p]=='"'){end='"';p++;}
    while(p<len&&b[p]!=end&&b[p]!='\n'&&b[p]!='\r'&&!(end==' '&&b[p]=='\t')&&n<(long)sizeof(f->node)-1)f->node[n++]=b[p++];
    f->node[n]=0;innode=1;nline=0;
   }else if(at_word(b,len,s,"@endnode"))innode=0;
   else if(innode&&!command_line(b,len,s))nline++;
  }
  line++;s=i+1;
 }
 f->line=line;f->node_line=nline;
 if(guide&&(!innode||command_line(b,len,s)||at_word(b,len,s,"@node")))return 0;
 return 1;
}
int oav_find(const char *buf,long len,const char *what,long from,int guide,OAVFound *f)
{
 long wl=what?(long)strlen(what):0,i,pass;
 memset(f,0,sizeof(*f));f->at=-1;
 if(!buf||wl<=0||len<wl)return 0;
 if(from<0||from>len)from=0;
 for(pass=0;pass<2;pass++){
  long a=pass?0:from,z=pass?from:len;
  for(i=a;i<z&&i+wl<=len;i++)
   if(match_at(buf,len,i,what,wl)&&place(buf,len,i,guide,f)){f->at=i;f->wrapped=(int)pass;return 1;}
 }
 return 0;
}

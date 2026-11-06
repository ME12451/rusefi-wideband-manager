#ifndef WB_PROTOCOL_H
#define WB_PROTOCOL_H
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
typedef struct { uint32_t id; uint8_t ext, len, data[8]; } Frame;
#define WB_ACK 0x727573u
#define WB_ENTER 0x0ef00000u
#define WB_ERASE 0x0ef15a5au
#define WB_DATA 0x0ef20000u
#define WB_REBOOT 0x0ef30000u
#define WB_INDEX 0x0ef40000u
#define WB_STATUS 0x0ef50000u
#define WB_PING 0x0ef60000u
#define WB_SENSOR 0x0ef70000u
#define WB_BITRATE 0x0ef80000u
static uint16_t le16(const uint8_t *p) { return p[0] | ((uint16_t)p[1]<<8); }
static uint32_t le32(const uint8_t *p) { return le16(p)|((uint32_t)le16(p+2)<<16); }
static uint32_t crc32wb(const uint8_t *p,size_t n) { uint32_t c=~0u; while(n--){c^=*p++;for(int i=0;i<8;i++)c=(c>>1)^((c&1)?0xedb88320u:0);}return ~c; }
static int valid_image(const uint8_t *p,size_t n) {
 if(n!=25600)return 0;
 uint32_t sp=le32(p),rv=le32(p+4),crc=crc32wb(p,n-4);
 return sp>=0x20000100 && sp<=0x20001800 && !(sp&3) && (rv&1) && rv>=0x08001801 && rv<0x08007c00 &&
 p[n-4]==(uint8_t)(crc>>24) && p[n-3]==(uint8_t)(crc>>16) && p[n-2]==(uint8_t)(crc>>8) && p[n-1]==(uint8_t)crc;
}
static int slcan_encode(const Frame *f,char *s) {
 if(f->len>8 || f->id>(f->ext?0x1fffffffu:0x7ffu))return 0;
 int n=sprintf(s,f->ext?"T%08X%1X":"t%03X%1X",f->id,f->len);
 for(int i=0;i<f->len;i++){n+=sprintf(s+n,"%02X",f->data[i]);}
 s[n++]='\r';s[n]=0;return n;
}
static int hexn(const char *p,int n,uint32_t *v){*v=0;for(int i=0;i<n;i++){char c=p[i];int h=c>='0'&&c<='9'?c-'0':c>='A'&&c<='F'?c-'A'+10:c>='a'&&c<='f'?c-'a'+10:-1;if(h<0)return 0;*v=(*v<<4)|h;}return 1;}
static int slcan_decode(const char *s,Frame *f){
 size_t n=strlen(s);if(n<5 || (s[0]!='T'&&s[0]!='t'))return 0;
 int digits=s[0]=='T'?8:3;uint32_t id,len,v;
 if(n<(size_t)(digits+2)||!hexn(s+1,digits,&id)||!hexn(s+1+digits,1,&len)||len>8)return 0;
 size_t end=digits+2+2*len;if(n!=end && n!=end+4)return 0;
 if(id>(digits==8?0x1fffffffu:0x7ffu))return 0;
 memset(f,0,sizeof(*f));f->id=id;f->ext=digits==8;f->len=(uint8_t)len;
 for(unsigned i=0;i<len;i++){if(!hexn(s+digits+2+2*i,2,&v))return 0;f->data[i]=(uint8_t)v;}
 if(n==end+4&&!hexn(s+end,4,&v))return 0;
 return 1;
}
#endif

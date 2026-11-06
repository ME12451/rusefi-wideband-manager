#include <assert.h>
#include "../src/protocol.h"
int main(int argc,char **argv){
 assert(WB_ENTER==0x0ef00000 && WB_ERASE==0x0ef15a5a && WB_DATA==0x0ef20000);
 assert(crc32wb((const uint8_t*)"123456789",9)==0xcbf43926);
 Frame f={WB_INDEX,1,2,{7,4}},g;char s[64];assert(slcan_encode(&f,s)==15);assert(!strcmp(s,"T0EF4000020704\r"));s[strlen(s)-1]=0;assert(slcan_decode(s,&g));assert(g.id==WB_INDEX&&g.ext&&g.len==2&&g.data[0]==7&&g.data[1]==4);
 assert(slcan_decode("t1908A001102734120000",&g));assert(le16(g.data+2)==10000);assert(le16(g.data+4)==0x1234);
 assert(slcan_decode("t1900ABCD",&g));
 const char *bad[]={"","T","TFFFFFFFF0","t8000","T0EF400009","T0EF40000207","t1901GG","t190000000","t1900zzzz","R0EF400000"};
 for(unsigned i=0;i<sizeof bad/sizeof *bad;i++)assert(!slcan_decode(bad[i],&g));
 uint8_t b[25600]={0};assert(!valid_image(b,sizeof b));
 b[1]=0x18;b[3]=0x20;b[4]=1;b[5]=0x19;b[7]=8;uint32_t crc=crc32wb(b,sizeof b-4);for(int i=0;i<4;i++)b[sizeof b-4+i]=(uint8_t)(crc>>(24-8*i));assert(valid_image(b,sizeof b));assert(!valid_image(b,sizeof b-1));b[100]^=1;assert(!valid_image(b,sizeof b));
 if(argc>1){FILE *fbin=fopen(argv[1],"rb");assert(fbin);assert(fread(b,1,sizeof b,fbin)==sizeof b);assert(fgetc(fbin)==EOF);fclose(fbin);assert(valid_image(b,sizeof b));}
 puts("Protocol, malformed SLCAN, vectors and CRC tests passed.");return 0;
}

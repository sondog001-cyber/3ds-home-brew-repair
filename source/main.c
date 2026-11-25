#include <3ds.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
static int calstate=0; static u8 cal[2]; static Result calresult=0;
static void readcal(void){
 Result r=cfguInit(); if(R_FAILED(r)){calresult=r;calstate=-1;return;}
 r=CFG_GetConfigInfoBlk8(2,0x00050000,cal);cfguExit();calresult=r;calstate=R_SUCCEEDED(r)?1:-1;
}
static const char *names[]={"Gray steps","Near black","Near white","Gradient","Vertical stripes","Horizontal stripes","Checkerboard","Red","Green","Blue","White","Black","Adjustable gray","Color cycle"};
static const int widths[]={1,2,4,8,16};
static void draw(gfxScreen_t s,int p,int stripe,int gray,unsigned frame){
 u16 h,w;u8 *fb=gfxGetFramebuffer(s,GFX_LEFT,&h,&w);
 for(int x=0;x<w;x++)for(int y=0;y<h;y++){
 int v=0;switch(p){case 0:v=(x*11/w)*255/10;break;case 1:v=x*32/w;break;case 2:v=224+x*32/w;break;case 3:v=x*255/(w-1);break;case 4:v=((x/stripe)%2)*255;break;case 5:v=((y/stripe)%2)*255;break;case 6:v=(((x/stripe)+(y/stripe))%2)*255;break;case 10:v=255;break;case 12:v=gray;break;}
 u8 r=v,g=v,b=v;if(p==7)r=255;if(p==8)g=255;if(p==9)b=255;if(p==13){unsigned n=(frame/120)%3;r=n==0?255:0;g=n==1?255:0;b=n==2?255:0;}
 size_t i=(x*h+h-1-y)*3;fb[i]=b;fb[i+1]=g;fb[i+2]=r;
 }}
int main(void){
 gfxInitDefault();gfxSet3D(false);gfxSetDoubleBuffering(GFX_BOTTOM,false);consoleInit(GFX_BOTTOM,NULL);
 int p=0,wi=0,gray=128,compare=0,dirty=1,saved=0;unsigned frame=0;
 while(aptMainLoop()){
 gspWaitForVBlank();hidScanInput();u32 k=hidKeysDown();
 if(k&KEY_START)break;
 if(k&KEY_RIGHT){p=(p+1)%14;dirty=1;}
 if(k&KEY_LEFT){p=(p+13)%14;dirty=1;}
 if(k&KEY_UP){wi=(wi+1)%5;dirty=1;}
 if(k&KEY_DOWN){wi=(wi+4)%5;dirty=1;}
 if(k&KEY_L){gray=gray>=8?gray-8:0;dirty=1;}
 if(k&KEY_R){gray=gray<=247?gray+8:255;dirty=1;}
 if(k&KEY_X){compare=!compare;dirty=1;}
 if((k&KEY_Y)&&!compare&&!calstate){
 consoleClear();printf("Reading stored CFG calibration...\n");gfxFlushBuffers();gfxSwapBuffers();gspWaitForVBlank();
 readcal();dirty=1;
 }
 if(k&KEY_A){
 mkdir("sdmc:/3ds",0777);mkdir("sdmc:/3ds/LCDLab",0777);
 FILE *f=fopen("sdmc:/3ds/LCDLab/report-v04.txt","a");
 if(f){fprintf(f,"v0.4 pattern=%s width=%d gray=%d compare=%d MCU=disabled\n",names[p],widths[wi],gray,compare);fprintf(f,"CFG state=%d result=%08lX top=%u bottom=%u\n",calstate,(unsigned long)calresult,cal[0],cal[1]);saved=fclose(f)==0?1:-1;}else saved=-1;dirty=1;}
 if(!compare&&dirty){consoleClear();printf("LCD Lab v0.4 - CFG READ\n\n%d/14 %s\nStripe width: %d px\nGray: %d/255\n\nLeft/Right: pattern\nUp/Down: stripe width\nL/R: gray intensity\nX: compare both screens\nA: append report\nSTART: exit\n\nY: read saved calibration\nY: read saved calibration\nMCU disabled. No writes.\nReport: %s\n",p+1,names[p],widths[wi],gray,saved==1?"saved":saved==-1?"failed":"not saved");if(calstate==1)printf("Stored top: %u bottom: %u\n",cal[0],cal[1]);else if(calstate<0)printf("CFG error: %08lX\n",(unsigned long)calresult);dirty=0;}
 draw(GFX_TOP,p,widths[wi],gray,frame);
 if(compare)draw(GFX_BOTTOM,p,widths[wi],gray,frame);
 frame++;gfxFlushBuffers();gfxSwapBuffers();
 }
 gfxExit();return 0;
}

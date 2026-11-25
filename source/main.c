#include <3ds.h>
#include <stdio.h>
#include <sys/stat.h>
static const char *names[]={"Gray steps","Near black","Near white","Gradient","Vertical stripes","Horizontal stripes","Checkerboard","Red","Green","Blue","White","Black","Mid gray","Color cycle"};
static void draw(int p,unsigned frame){u16 h,w;u8 *fb=gfxGetFramebuffer(GFX_TOP,GFX_LEFT,&h,&w);for(int x=0;x<w;x++)for(int y=0;y<h;y++){int v=0;switch(p){case 0:v=(x*11/w)*255/10;break;case 1:v=x*32/w;break;case 2:v=224+x*32/w;break;case 3:v=x*255/(w-1);break;case 4:v=(x%2)*255;break;case 5:v=(y%2)*255;break;case 6:v=((x+y)%2)*255;break;case 10:v=255;break;case 12:v=128;break;}u8 r=v,g=v,b=v;if(p==7)r=255;if(p==8)g=255;if(p==9)b=255;if(p==13){unsigned n=(frame/120)%3;r=n==0?255:0;g=n==1?255:0;b=n==2?255:0;}size_t i=(x*h+h-1-y)*3;fb[i]=b;fb[i+1]=g;fb[i+2]=r;}}
int main(void){
 gfxInitDefault();
 gfxSet3D(false);
 gfxSetDoubleBuffering(GFX_BOTTOM,false);
 consoleInit(GFX_BOTTOM,NULL);
 int p=0,saved=0,dirty=1;
 unsigned frame=0;
 printf("LCD Lab v0.2\nSafe display-only mode\nNo MCU access.\n");
 gfxFlushBuffers();gfxSwapBuffers();gspWaitForVBlank();
 while(aptMainLoop()){
  gspWaitForVBlank();
  hidScanInput();u32 k=hidKeysDown();
  if(k&KEY_START)break;
  if(k&KEY_RIGHT){p=(p+1)%14;dirty=1;}
  if(k&KEY_LEFT){p=(p+13)%14;dirty=1;}
  if(k&KEY_A){
   mkdir("sdmc:/3ds",0777);mkdir("sdmc:/3ds/LCDLab",0777);
   FILE *f=fopen("sdmc:/3ds/LCDLab/report-v02.txt","w");
   if(f){fprintf(f,"LCD Lab v0.2 display only\nMCU not attempted\nPattern: %s\n",names[p]);saved=fclose(f)==0?1:-1;}else saved=-1;
   dirty=1;
  }
  if(dirty){consoleClear();printf("LCD Lab v0.2 - SAFE MODE\n\nPattern %d/14: %s\n\nLeft/Right: change pattern\nA: save report\nSTART: exit\n\nMCU access disabled\nNo calibration writes\nReport: %s\n",p+1,names[p],saved==1?"saved":saved==-1?"failed":"not saved");dirty=0;}
  draw(p,frame++);
  gfxFlushBuffers();gfxSwapBuffers();
 }
 gfxExit();return 0;
}

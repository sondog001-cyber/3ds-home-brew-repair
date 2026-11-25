#include <3ds.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
static int calstate=0; static u8 cal[2]; static Result calresult=0;
static int live=-1, original=-1, job=0, target=0, exitAfter=0;
static Result liveResult=0;
static Thread worker=NULL; static u64 started=0; static int timedout=0;
static void logstep(const char *s){mkdir("sdmc:/3ds",0777);mkdir("sdmc:/3ds/LCDLab",0777);FILE *f=fopen("sdmc:/3ds/LCDLab/progress-v05.txt","a");if(f){fprintf(f,"%s\n",s);fclose(f);}}
static void readcal(void *arg){
 (void)arg;
 Result r=cfguInit(); if(R_FAILED(r)){calresult=r;calstate=-1;return;}
 r=CFG_GetConfigInfoBlk8(2,0x00050000,cal);cfguExit();calresult=r;calstate=R_SUCCEEDED(r)?1:-1;
}
static Result rtcRead(Handle h,int *v){
 u32 *b=getThreadCommandBuffer();b[0]=0x00440000;
 Result r=svcSendSyncRequest(h);if(R_FAILED(r))return r;
 r=(Result)b[1];if(R_SUCCEEDED(r)){if((b[2]&~255u)!=0)return (Result)-1;*v=(int)b[2];}return r;
}
static void liveWorker(void *arg){
 (void)arg;Handle h=0;int v=-1;
 logstep("RTC service lookup");
 Result r=srvGetServiceHandle(&h,"mcu::RTC");
 if(R_SUCCEEDED(r)){
 logstep("RTC reading top");
 r=rtcRead(h,&v);
 if(R_SUCCEEDED(r)&&job==1){original=v;live=v;}
 if(R_SUCCEEDED(r)&&job==2){
 if(v!=live||original<0||target<0||target>255||target<original-2||target>original+2)r=(Result)-1;
 else {logstep("RTC temporary top write");u32 *b=getThreadCommandBuffer();b[0]=0x00430040;b[1]=(u32)target;
 r=svcSendSyncRequest(h);if(R_SUCCEEDED(r))r=(Result)b[1];
 if(R_SUCCEEDED(r)){logstep("RTC write readback");r=rtcRead(h,&v);if(R_SUCCEEDED(r)&&v!=target)r=(Result)-1;}
 if(R_SUCCEEDED(r))live=v;else live=-1;
 }}
 svcCloseHandle(h);
 }
 liveResult=r;
}
static void launchLive(int j,int t){job=j;target=t;started=osGetTime();worker=threadCreate(liveWorker,NULL,16384,0x30,-2,false);if(!worker){liveResult=-1;}}
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
 if(worker){if(R_SUCCEEDED(threadJoin(worker,0))){threadFree(worker);worker=NULL;logstep("Worker completed");dirty=1;if(exitAfter&&live==original&&R_SUCCEEDED(liveResult))break;exitAfter=0;}else if(!timedout&&osGetTime()-started>=5000){timedout=1;logstep("CFG worker timeout; no retry");dirty=1;}}
 if(k&KEY_START){if(!worker){if(original>=0&&live!=original){if(live>=0&&!timedout){exitAfter=1;launchLive(2,original);}else break;}else break;}dirty=1;}
 if(!worker&&!timedout&&!compare&&!exitAfter&&!(k&KEY_START)){
 if((k&KEY_SELECT)&&original<0&&R_SUCCEEDED(liveResult)){launchLive(1,0);dirty=1;}
 else if(original>=0&&live>=0&&R_SUCCEEDED(liveResult)){
 if((k&KEY_B)&&live!=original){launchLive(2,original);dirty=1;}
 else if((k&KEY_L)&&(hidKeysHeld()&KEY_Y)&&live>original-2&&live>0){launchLive(2,live-1);dirty=1;}
 else if((k&KEY_R)&&(hidKeysHeld()&KEY_Y)&&live<original+2&&live<255){launchLive(2,live+1);dirty=1;}
 }}
 if(k&KEY_RIGHT){p=(p+1)%14;dirty=1;}
 if(k&KEY_LEFT){p=(p+13)%14;dirty=1;}
 if(k&KEY_UP){wi=(wi+1)%5;dirty=1;}
 if(k&KEY_DOWN){wi=(wi+4)%5;dirty=1;}
 if((k&KEY_L)&&!(hidKeysHeld()&KEY_Y)){gray=gray>=8?gray-8:0;dirty=1;}
 if((k&KEY_R)&&!(hidKeysHeld()&KEY_Y)){gray=gray<=247?gray+8:255;dirty=1;}
 if(k&KEY_X){compare=!compare;dirty=1;}
 if((k&KEY_Y)&&!compare&&!worker&&!timedout&&!calstate&&original<0&&!(hidKeysHeld()&(KEY_L|KEY_R))){
 consoleClear();printf("Reading stored CFG calibration...\n");gfxFlushBuffers();gfxSwapBuffers();gspWaitForVBlank();
 job=0;logstep("Starting read-only CFG worker");started=osGetTime();worker=threadCreate(readcal,NULL,8192,0x30,-2,false);if(!worker){calstate=-1;calresult=-1;}dirty=1;
 }
 if(k&KEY_A){
 mkdir("sdmc:/3ds",0777);mkdir("sdmc:/3ds/LCDLab",0777);
 FILE *f=fopen("sdmc:/3ds/LCDLab/report-v05.txt","a");
 if(f){fprintf(f,"v0.5 pattern=%s width=%d gray=%d compare=%d MCU=manual\n",names[p],widths[wi],gray,compare);if(!worker)fprintf(f,"CFG state=%d result=%08lX raw=%u,%u\n",calstate,(unsigned long)calresult,cal[0],cal[1]);else fprintf(f,"CFG pending or stalled\n");if(!worker)fprintf(f,"Live=%d original=%d RTC=%08lX\n",live,original,(unsigned long)liveResult);saved=fclose(f)==0?1:-1;}else saved=-1;dirty=1;}
 if(!compare&&dirty){consoleClear();printf("LCD Lab v0.5 - CALIBRATION\n\n%d/14 %s\nStripe width: %d px\nGray: %d/255\n\nLeft/Right: pattern\nUp/Down: stripe width\nL/R: gray intensity\nX: compare both screens\nA: append report\nSTART: exit\n\nY: read saved calibration\nSELECT: read live top\nY+L/R: top -/+1 (max 2)\nB: restore live baseline\nReport: %s\n",p+1,names[p],widths[wi],gray,saved==1?"saved":saved==-1?"failed":"not saved");if(worker)printf(timedout?"Request stalled. Power off if needed.\n":"Hardware request pending...\n");else if(calstate==1)printf("Stored raw bytes: %u, %u\n",cal[0],cal[1]);else if(calstate<0)printf("CFG error: %08lX\n",(unsigned long)calresult);if(!worker&&original>=0&&live<0)printf("Unknown live value: power cycle.\n");if(!worker)printf("Live: %d Original: %d\nRTC result: %08lX\n",live,original,(unsigned long)liveResult);dirty=0;}
 draw(GFX_TOP,p,widths[wi],gray,frame);
 if(compare)draw(GFX_BOTTOM,p,widths[wi],gray,frame);
 frame++;gfxFlushBuffers();gfxSwapBuffers();
 }
 if(worker)svcExitProcess();
 gfxExit();return 0;
}

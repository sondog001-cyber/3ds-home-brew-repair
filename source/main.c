#include <3ds.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <stdint.h>
static int calstate=0; static u8 cal[2]; static Result calresult=0;
static int job=0, attempted=0, done=0; static u8 live[2][2]; static int reads=0;
static Result result; static int current=-1, requested=93, locked=0, exitAfter=0, armed=0;
static Thread worker=NULL; static u64 started=0; static int timedout=0;
static void logstep(const char *s){mkdir("sdmc:/3ds",0777);mkdir("sdmc:/3ds/LCDLab",0777);FILE *f=fopen("sdmc:/3ds/LCDLab/progress-v06.txt","a");if(f){fprintf(f,"%s\n",s);fclose(f);}}
static void logresult(const char *stage,Result r){
 char b[160];snprintf(b,sizeof(b),"%s result=%08lX level=%lu summary=%lu module=%lu desc=%lu",stage,(unsigned long)r,(unsigned long)R_LEVEL(r),(unsigned long)R_SUMMARY(r),(unsigned long)R_MODULE(r),(unsigned long)R_DESCRIPTION(r));logstep(b);
}
static void probe(void *arg){
 (void)arg;bool registered=false;
 Result r=srvIsServiceRegistered(&registered,"mcu::HWC");logresult("HWC registration",r);
 if(R_SUCCEEDED(r)&&registered){
 srvSetBlockingPolicy(true);r=mcuHwcInit();srvSetBlockingPolicy(false);
 logresult("HWC init",r);
 if(R_SUCCEEDED(r)){
 for(int i=0;i<2&&R_SUCCEEDED(r);i++){
 for(int j=0;j<2&&R_SUCCEEDED(r);j++){
 char stage[80];snprintf(stage,sizeof(stage),"Pass %d register %02X",i+1,j+3);logstep(stage);
 r=MCUHWC_ReadRegister((u8)(j+3),&live[i][j],1);logresult(stage,r);
 if(R_SUCCEEDED(r)){reads++;snprintf(stage,sizeof(stage),"Value %u",live[i][j]);logstep(stage);}
 }}
 mcuHwcExit();logstep("HWC released");
 }
 }else if(R_SUCCEEDED(r))r=(Result)-1;
 result=r;done=1;if(R_SUCCEEDED(r)&&reads==4&&live[0][0]==93&&live[1][0]==93&&live[0][1]==110&&live[1][1]==110)current=93;
}
static void adjust(void *arg){
 (void)arg;u8 top=0,bottom=0;bool registered=false;
 int want=requested;Result r=srvIsServiceRegistered(&registered,"mcu::HWC");logresult("Adjust registration",r);
 if(R_SUCCEEDED(r)&&registered){
 srvSetBlockingPolicy(true);r=mcuHwcInit();srvSetBlockingPolicy(false);logresult("Adjust init",r);
 if(R_SUCCEEDED(r)){
 r=MCUHWC_ReadRegister(3,&top,1);logresult("Pre top",r);
 if(R_SUCCEEDED(r)){r=MCUHWC_ReadRegister(4,&bottom,1);logresult("Pre bottom",r);}
 if(R_SUCCEEDED(r)&&(top<91||top>95||bottom!=110||want<91||want>95||(want!=93&&top!=current)))r=(Result)-1;
 if(R_SUCCEEDED(r)){
 char b[80];snprintf(b,sizeof(b),"Set top %u -> %d",top,want);logstep(b);
 r=MCUHWC_SetTopLcdFlicker((u8)want);logresult("Set top",r);
 }
 if(R_SUCCEEDED(r)){r=MCUHWC_ReadRegister(3,&top,1);logresult("Verify top",r);}
 if(R_SUCCEEDED(r)){r=MCUHWC_ReadRegister(4,&bottom,1);logresult("Verify bottom",r);}
 if(R_SUCCEEDED(r)&&(top!=want||bottom!=110))r=(Result)-1;
 if(R_SUCCEEDED(r))current=top;else {current=-1;locked=1;}
 mcuHwcExit();
 }
 }else if(R_SUCCEEDED(r))r=(Result)-1;
 if(R_FAILED(r))locked=1;
 result=r;
}
static void launchAdjust(int value){requested=value;job=2;started=osGetTime();worker=threadCreate(adjust,NULL,16384,0x30,-2,false);if(!worker){locked=1;result=(Result)-1;}}
static void readcal(void *arg){
 (void)arg;
 Result r=cfguInit(); logresult("cfguInit",r); if(R_FAILED(r)){calresult=r;calstate=-1;return;}
 r=CFG_GetConfigInfoBlk8(2,0x00050000,cal);logresult("CFG block 00050000 size 2",r);cfguExit();calresult=r;calstate=R_SUCCEEDED(r)?1:-1;
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
 gfxInitDefault();gfxSet3D(false);gfxSetDoubleBuffering(GFX_TOP,false);gfxSetDoubleBuffering(GFX_BOTTOM,false);consoleInit(GFX_BOTTOM,NULL);
 int p=0,wi=0,gray=128,compare=0,dirty=1,saved=0;unsigned frame=0;int lastp=-1,lastwi=-1,lastgray=-1,lastcompare=-1;unsigned lastcycle=999;
 while(aptMainLoop()){
 gspWaitForVBlank();hidScanInput();u32 k=hidKeysDown();
 if(worker){if(R_SUCCEEDED(threadJoin(worker,0))){threadFree(worker);worker=NULL;logstep("Worker completed");if(job==0&&calstate==1){char b[80];snprintf(b,sizeof(b),"Stored bytes %u,%u",cal[0],cal[1]);logstep(b);}dirty=1;if(exitAfter&&current==93&&R_SUCCEEDED(result))break;exitAfter=0;}else if(!timedout&&osGetTime()-started>=5000){timedout=1;logstep("CFG worker timeout; no retry");dirty=1;}}
 if(k&KEY_START){if(!worker){if(current==93||(!armed&&current<0))break;if(!timedout){exitAfter=1;launchAdjust(93);}}dirty=1;}
 if(!worker&&!timedout&&!compare&&!(k&KEY_START)){
 if((k&KEY_B)&&armed){launchAdjust(93);dirty=1;}
 else if((k&KEY_X)&&(hidKeysHeld()&KEY_Y)&&current==93&&calstate==1&&cal[0]==93&&cal[1]==110&&!locked){armed=1;dirty=1;}
 else if(armed&&!locked&&current>=91&&current<=95&&(hidKeysHeld()&KEY_Y)&&((k&KEY_L)||(k&KEY_R))){int t=current+((k&KEY_R)?1:-1);if(t>=91&&t<=95){launchAdjust(t);dirty=1;}}
 else if((k&KEY_SELECT)&&!attempted){attempted=1;job=1;started=osGetTime();worker=threadCreate(probe,NULL,16384,0x30,-2,false);if(!worker){result=(Result)-1;done=1;logstep("Worker creation failed");}dirty=1;}
 }
 if(k&KEY_RIGHT){p=(p+1)%14;dirty=1;}
 if(k&KEY_LEFT){p=(p+13)%14;dirty=1;}
 if(k&KEY_UP){wi=(wi+1)%5;dirty=1;}
 if(k&KEY_DOWN){wi=(wi+4)%5;dirty=1;}
 if((k&KEY_L)&&!(hidKeysHeld()&KEY_Y)){gray=gray>=8?gray-8:0;dirty=1;}
 if((k&KEY_R)&&!(hidKeysHeld()&KEY_Y)){gray=gray<=247?gray+8:255;dirty=1;}
 if((k&KEY_X)&&!(hidKeysHeld()&KEY_Y)){compare=!compare;dirty=1;}
 if((k&KEY_Y)&&!compare&&!worker&&!timedout&&!calstate&&!(hidKeysHeld()&(KEY_X|KEY_L|KEY_R))){
 consoleClear();printf("Reading stored CFG calibration...\n");gfxFlushBuffers();gfxSwapBuffers();gspWaitForVBlank();
 job=0;logstep("Starting read-only CFG worker");started=osGetTime();worker=threadCreate(readcal,NULL,8192,0x30,-2,false);if(!worker){calstate=-1;calresult=-1;}dirty=1;
 }
 if(k&KEY_A){
 mkdir("sdmc:/3ds",0777);mkdir("sdmc:/3ds/LCDLab",0777);
 FILE *f=fopen("sdmc:/3ds/LCDLab/report-v06.txt","a");
 if(f){fprintf(f,"v0.6 pattern=%s width=%d gray=%d compare=%d MCU=probe\n",names[p],widths[wi],gray,compare);if(!worker)fprintf(f,"CFG state=%d result=%08lX raw=%u,%u\n",calstate,(unsigned long)calresult,cal[0],cal[1]);else fprintf(f,"CFG pending or stalled\n");if(!worker)fprintf(f,"Current=%d armed=%d locked=%d\n",current,armed,locked);if(!worker&&done)fprintf(f,"HWC=%08lX reads=%d live=%u,%u / %u,%u\n",(unsigned long)result,reads,live[0][0],live[0][1],live[1][0],live[1][1]);saved=fclose(f)==0?1:-1;}else saved=-1;dirty=1;}
 if(!compare&&dirty){consoleClear();printf("LCD Lab v0.6 - TOP CALIBRATION\n\n%d/14 %s\nStripe width: %d px\nGray: %d/255\n\nLeft/Right: pattern\nUp/Down: stripe width\nL/R: gray intensity\nX: compare both screens\nA: append report\nSTART: exit\n\nY: read saved calibration\nSELECT: read live twice\nY+X: arm after matching reads\nY+L/R: adjust 91-95\nB: restore 93\nReport: %s\n",p+1,names[p],widths[wi],gray,saved==1?"saved":saved==-1?"failed":"not saved");if(worker)printf(timedout?"Request stalled; power off if needed.\n":"Request pending; please wait.\n");else if(calstate==1)printf("Stored raw bytes: %u, %u\n",cal[0],cal[1]);else if(calstate<0)printf("CFG error: %08lX\n",(unsigned long)calresult);if(!worker){if(done){printf("HWC result: %08lX\n",(unsigned long)result);if(reads==4)printf("Live 1: %u, %u\nLive 2: %u, %u\n%s\n",live[0][0],live[0][1],live[1][0],live[1][1],memcmp(live[0],live[1],2)==0?"Repeat matched":"Repeat DIFFERENT");else printf("Completed reads: %d/4\n",reads);}else printf("HWC not tested\n");}if(!worker)printf("Current: %d %s\n",current,locked?"LOCKED - restore or reboot":armed?"ARMED":"DISARMED");dirty=0;}
 if(p!=lastp||wi!=lastwi||gray!=lastgray||compare!=lastcompare||(p==13&&lastcycle!=frame/120)){draw(GFX_TOP,p,widths[wi],gray,frame);if(compare)draw(GFX_BOTTOM,p,widths[wi],gray,frame);lastp=p;lastwi=wi;lastgray=gray;lastcompare=compare;lastcycle=frame/120;}
 frame++;gfxFlushBuffers();
 }
 if(worker)svcExitProcess();
 gfxExit();return 0;
}

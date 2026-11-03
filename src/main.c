/* TTT ULUS10537 / CAMERA DIAG02 -- read-only camera sampling.
 * Battle callsite 0x0885AD5C calls original 0x088068FC with a0 = current camera.
 * The site is in battle-draw 0x0885AD48; different path from CAM01.
 * This test does not write any camera values, nor touch viewport or framerate.
 */
#include <pspuser.h>
#include <pspiofilemgr.h>
#include <psputils.h>
#include <stdint.h>
#include <stddef.h>
PSP_MODULE_INFO("TTTCamDiag02", PSP_MODULE_USER, 1, 0);
PSP_NO_CREATE_MAIN_THREAD();
#define SITE 0x0885AD5Cu
#define ORIG_JAL 0x0E201A3Fu
#define ORIG_DELAY 0x02002025u
#define ORIGINAL 0x088068FCu
#define GLOBAL_CAM_PTR 0x08A7E25Cu
#define LOG_FILE "ms0:/PSP/PLUGINS/TTT_CAMERA_DIAG02/diagnostico_camera_v02.txt"
typedef void (*CallOriginal)(void *);
static volatile uint32_t g_calls=0, g_last_arg=0;
static volatile int g_installed=0,g_running=0;
static SceUID g_thread=-1;
static __attribute__((noinline)) void camera_site_probe(void *camera) {
    ++g_calls;
    g_last_arg=(uint32_t)(uintptr_t)camera;
    ((CallOriginal)(uintptr_t)ORIGINAL)(camera);
}
static uint32_t jal_word(uintptr_t a) {return 0x0C000000u | (((uint32_t)a>>2)&0x03FFFFFFu);}
static void flush(void *p) {sceKernelDcacheWritebackInvalidateRange(p,4); sceKernelIcacheInvalidateRange(p,4);}
static void str(char **p,char *e,const char *s){while(*s && *p<e)*(*p)++=*s++;}
static void dec(char **p,char *e,uint32_t v){char a[10];unsigned n=0;do{a[n++]=(char)('0'+v%10);v/=10;}while(v&&n<10);while(n&&*p<e)*(*p)++=a[--n];}
static void hex(char **p,char *e,uint32_t v){static const char h[]="0123456789ABCDEF";int n;for(n=28;n>=0;n-=4)if(*p<e)*(*p)++=h[(v>>n)&15];}
static void log(const char *msg){unsigned n=0;SceUID fd;while(msg[n]&&n<900)n++;fd=sceIoOpen(LOG_FILE,PSP_O_CREAT|PSP_O_WRONLY|PSP_O_APPEND,0777);if(fd<0)return;sceIoWrite(fd,msg,n);sceIoClose(fd);}
static int valid(uint32_t p){return (p>=0x08800000u && p<0x0BBFFC00u && (p&3u)==0u);}
static void sample(const char *name,uint32_t *last) {
    char line[600],*p=line,*end=line+sizeof(line)-1;
    uint32_t hits=g_calls, cb=g_last_arg, glob=*(volatile uint32_t *)(uintptr_t)GLOBAL_CAM_PTR;
    str(&p,end,"[CAM02] ");str(&p,end,name);str(&p,end," hits=");dec(&p,end,hits);
    str(&p,end," (+");dec(&p,end,hits-*last);str(&p,end,"/5s)");*last=hits;
    str(&p,end," arg=0x");hex(&p,end,cb);str(&p,end," global=0x");hex(&p,end,glob);
    if(valid(cb)) {
        volatile uint32_t *d=(volatile uint32_t *)(uintptr_t)cb;
        str(&p,end," mat40_axisZ_bits=");hex(&p,end,d[0x60/4]);str(&p,end,",");hex(&p,end,d[0x64/4]);str(&p,end,",");hex(&p,end,d[0x68/4]);
        str(&p,end," mat40_xyz_bits=");hex(&p,end,d[0x70/4]);str(&p,end,",");hex(&p,end,d[0x74/4]);str(&p,end,",");hex(&p,end,d[0x78/4]);
        str(&p,end," view140_translation_bits=");hex(&p,end,d[0x170/4]);str(&p,end,",");hex(&p,end,d[0x174/4]);str(&p,end,",");hex(&p,end,d[0x178/4]);
    }
    str(&p,end,"\n");*p=0;log(line);
}
static int install(void) {
    volatile uint32_t *site=(volatile uint32_t *)(uintptr_t)SITE;
    uintptr_t target=(uintptr_t)camera_site_probe;
    if((target&3u)||((target>>28)!=((SITE+4u)>>28))) {log("[CAM02] ERRO wrapper fora da regiao JAL\n");return -1;}
    if(site[0]!=ORIG_JAL||site[1]!=ORIG_DELAY){
        char msg[160],*p=msg,*e=msg+sizeof(msg)-1;
        str(&p,e,"[CAM02] ERRO codigo inesperado JAL=0x");hex(&p,e,site[0]);str(&p,e," delay=0x");hex(&p,e,site[1]);str(&p,e,"\n");*p=0;log(msg);return -2;
    }
    site[0]=jal_word(target);flush((void *)(uintptr_t)SITE);
    g_installed=1;log("[CAM02] Hook instalado. Chamada original mantida. Somente leitura.\n");return 0;
}
static void uninstall(void){volatile uint32_t *s=(volatile uint32_t *)(uintptr_t)SITE;if(!g_installed)return;if(s[0]==jal_word((uintptr_t)camera_site_probe)){s[0]=ORIG_JAL;flush((void *)(uintptr_t)SITE);}g_installed=0;}
static int worker(SceSize argc,void *argv){unsigned count=0,tries;uint32_t previous=0;(void)argc;(void)argv;
    for(tries=0;tries<100 && g_running;tries++) {
        if(*(volatile uint32_t *)(uintptr_t)SITE==ORIG_JAL)break;
        sceKernelDelayThread(100000u);
    }
    if(!g_running||tries==100||install()!=0){log("[CAM02] Sem hook ativo.\n");sceKernelExitDeleteThread(0);return 0;}
    while(g_running){sceKernelDelayThread(50000u);if(++count>=100u){count=0;sample("AMOSTRA",&previous);}}
    uninstall();log("[CAM02] Finalizado.\n");sceKernelExitDeleteThread(0);return 0;
}
int module_start(SceSize argc,void *argv){(void)argc;(void)argv;
    log("\n[CAM02] INICIO. DIAGNOSTICO SEM ALTERACAO DE CAMERA. ULUS10537.\n");g_running=1;
    g_thread=sceKernelCreateThread("TTTCamDiagWorker",worker,0x20,0x2000,0,NULL);
    if(g_thread<0){g_running=0;log("[CAM02] ERRO thread create\n");return 0;}
    if(sceKernelStartThread(g_thread,0,NULL)<0){g_running=0;log("[CAM02] ERRO thread start\n");}
    return 0;
}
int module_stop(SceSize argc,void *argv){(void)argc;(void)argv;g_running=0;uninstall();return 0;}

/*
 * TTT Camera Distance V03 - ULUS10537 - PPSSPP only.
 * Experimental. Two JAL call sites:
 *   0x089AC4E0 -> 0x089ABE54: rebuilds matrices, used to shift camera only
 *       after 0x089ABEE0 has recomputed transform (+0x40) from its source.
 *   0x0885AD5C -> 0x088068FC: known active in battle, observation only.
 * Preserves each delay slot, calls each original function exactly once.
 * No viewport/FOV/frame timing changes. Starts disabled.
 */
#include <pspuser.h>
#include <pspiofilemgr.h>
#include <psputils.h>
#include <pspctrl.h>
#include <stdint.h>
#include <stddef.h>

PSP_MODULE_INFO("TTTCameraDist03", PSP_MODULE_USER, 1, 0);
PSP_NO_CREATE_MAIN_THREAD();

#define SITE_REBUILD      0x089AC4E0u
#define WORD_REBUILD_JAL  0x0E26AF95u
#define WORD_REBUILD_DS   0x02402825u
#define ORIG_REBUILD      0x089ABE54u
#define SITE_DRAW         0x0885AD5Cu
#define WORD_DRAW_JAL     0x0E201A3Fu
#define WORD_DRAW_DS      0x02002025u
#define ORIG_DRAW         0x088068FCu
#define ACTIVE_CAM_PTR    0x08A7E25Cu
#define LOG_FILE "ms0:/PSP/PLUGINS/TTT_CAMERA_DIST03/diagnostico_camera_v03.txt"
#define COMBO (PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER | PSP_CTRL_TRIANGLE)

typedef void (*RebuildFn)(void*,void*);
typedef void (*DrawFn)(void*);
static volatile uint32_t g_rebuild_calls=0, g_draw_calls=0, g_modified=0, g_rejected=0;
static volatile uint32_t g_last_rebuild_cam=0, g_last_draw_cam=0, g_last_xbits=0, g_last_ybits=0, g_last_zbits=0;
static volatile int g_mode=0, g_running=0, g_installed=0;
static SceUID g_worker=-1;

static uint32_t bits(float f){union {float f;uint32_t u;} n;n.f=f;return n.u;}
static float distance_for_mode(int mode){
    switch(mode){case 1:return 40.0f;case 2:return 80.0f;case 3:return -40.0f;default:return 0.0f;}
}
static int valid_ptr(uint32_t p){return p>=0x08800000u && p<=0x0BBFFD00u && !(p&3u);}
static void move_camera(void *p){
    uint32_t addr=(uint32_t)(uintptr_t)p;
    volatile float *m;
    float scale=distance_for_mode(g_mode),dx,dy,dz,mag,x,y,z;
    if(scale==0.0f)return;
    if(!valid_ptr(addr)){++g_rejected;return;}
    m=(volatile float *)(uintptr_t)(addr+0x40u);
    dx=m[8];dy=m[9];dz=m[10];
    x=m[12];y=m[13];z=m[14];
    mag=dx*dx+dy*dy+dz*dz;
    if(!(mag>0.85f&&mag<1.15f&&x>-50000.f&&x<50000.f&&y>-50000.f&&y<50000.f&&z>-50000.f&&z<50000.f)){
        ++g_rejected;return;
    }
    g_last_rebuild_cam=addr;
    g_last_xbits=bits(x);g_last_ybits=bits(y);g_last_zbits=bits(z);
    m[12]=x+dx*scale;
    m[13]=y+dy*scale;
    m[14]=z+dz*scale;
    ++g_modified;
}
static __attribute__((noinline)) void camera_rebuild_hook(void *camera,void *reference){
    ++g_rebuild_calls;
    g_last_rebuild_cam=(uint32_t)(uintptr_t)camera;
    move_camera(camera);
    ((RebuildFn)(uintptr_t)ORIG_REBUILD)(camera,reference);
}
static __attribute__((noinline)) void camera_draw_probe(void *camera){
    ++g_draw_calls;
    g_last_draw_cam=(uint32_t)(uintptr_t)camera;
    ((DrawFn)(uintptr_t)ORIG_DRAW)(camera);
}
static uint32_t make_jal(uintptr_t addr){return 0x0C000000u|(((uint32_t)addr>>2)&0x03FFFFFFu);}
static void flush_code(uint32_t addr){void *p=(void*)(uintptr_t)addr;sceKernelDcacheWritebackInvalidateRange(p,4);sceKernelIcacheInvalidateRange(p,4);}
static void put_str(char **p,char *e,const char *s){while(*s && *p<e)*(*p)++=*s++;}
static void put_uint(char **p,char *e,uint32_t v){char t[10];int i=0;do{t[i++]=(char)('0'+v%10u);v/=10u;}while(v&&i<10);while(i&&*p<e)*(*p)++=t[--i];}
static void put_hex(char **p,char *e,uint32_t v){static const char H[]="0123456789ABCDEF";int i;for(i=28;i>=0;i-=4)if(*p<e)*(*p)++=H[(v>>i)&15];}
static void write_log(const char *msg){unsigned n=0;SceUID fd;while(msg[n]&&n<1024u)n++;fd=sceIoOpen(LOG_FILE,PSP_O_CREAT|PSP_O_WRONLY|PSP_O_APPEND,0777);if(fd<0)return;sceIoWrite(fd,msg,n);sceIoClose(fd);}
static void log_sample(const char *tag,uint32_t *prev){
    char line[600], *p=line, *e=line+sizeof(line)-1;
    uint32_t r=g_rebuild_calls,d=g_draw_calls,m=g_modified,x=g_rejected;
    uint32_t active=*(volatile uint32_t*)(uintptr_t)ACTIVE_CAM_PTR;
    put_str(&p,e,"[CAM03] ");put_str(&p,e,tag);
    put_str(&p,e," mode=");put_uint(&p,e,(uint32_t)g_mode);
    put_str(&p,e," rebuild=");put_uint(&p,e,r);put_str(&p,e,"(+");put_uint(&p,e,r-prev[0]);put_str(&p,e,")");
    put_str(&p,e," draw=");put_uint(&p,e,d);put_str(&p,e,"(+");put_uint(&p,e,d-prev[1]);put_str(&p,e,")");
    put_str(&p,e," modified=");put_uint(&p,e,m);put_str(&p,e,"(+");put_uint(&p,e,m-prev[2]);put_str(&p,e,")");
    put_str(&p,e," rejected=");put_uint(&p,e,x);
    put_str(&p,e," rebuild_cam=0x");put_hex(&p,e,g_last_rebuild_cam);
    put_str(&p,e," draw_cam=0x");put_hex(&p,e,g_last_draw_cam);
    put_str(&p,e," active=0x");put_hex(&p,e,active);
    put_str(&p,e," old_xyz_bits=");put_hex(&p,e,g_last_xbits);put_str(&p,e,",");put_hex(&p,e,g_last_ybits);put_str(&p,e,",");put_hex(&p,e,g_last_zbits);
    put_str(&p,e,"\n");*p=0;write_log(line);
    prev[0]=r;prev[1]=d;prev[2]=m;
}
static int install(void){
    volatile uint32_t *r=(volatile uint32_t*)(uintptr_t)SITE_REBUILD;
    volatile uint32_t *d=(volatile uint32_t*)(uintptr_t)SITE_DRAW;
    uintptr_t wr=(uintptr_t)camera_rebuild_hook,wd=(uintptr_t)camera_draw_probe;
    if((wr&3u)||(wd&3u)||((wr>>28)!=((SITE_REBUILD+4u)>>28))||((wd>>28)!=((SITE_DRAW+4u)>>28))){write_log("[CAM03] ERRO enderecos dos wrappers fora do alcance JAL\n");return -1;}
    if(r[0]!=WORD_REBUILD_JAL||r[1]!=WORD_REBUILD_DS){write_log("[CAM03] ERRO instrucoes no hook REBUILD nao conferem\n");return -2;}
    if(d[0]!=WORD_DRAW_JAL||d[1]!=WORD_DRAW_DS){write_log("[CAM03] ERRO instrucoes no hook DRAW nao conferem\n");return -3;}
    r[0]=make_jal(wr);flush_code(SITE_REBUILD);
    d[0]=make_jal(wd);flush_code(SITE_DRAW);
    g_installed=1;
    write_log("[CAM03] HOOKS INSTALADOS 2/2 - chamadas originais preservadas.\n");
    return 0;
}
static void uninstall(void){
    volatile uint32_t *r=(volatile uint32_t*)(uintptr_t)SITE_REBUILD;
    volatile uint32_t *d=(volatile uint32_t*)(uintptr_t)SITE_DRAW;
    if(!g_installed)return;
    if(r[0]==make_jal((uintptr_t)camera_rebuild_hook)){r[0]=WORD_REBUILD_JAL;flush_code(SITE_REBUILD);}
    if(d[0]==make_jal((uintptr_t)camera_draw_probe)){d[0]=WORD_DRAW_JAL;flush_code(SITE_DRAW);}
    g_installed=0;
}
static int worker(SceSize argc,void *argv){
    unsigned n=0,i;int pressed=0;uint32_t prev[3]={0,0,0};SceCtrlData ctrl;
    (void)argc;(void)argv;
    for(i=0;i<100u&&g_running;i++){
        if(*(volatile uint32_t*)(uintptr_t)SITE_REBUILD==WORD_REBUILD_JAL&&*(volatile uint32_t*)(uintptr_t)SITE_DRAW==WORD_DRAW_JAL)break;
        sceKernelDelayThread(100000u);
    }
    if(!g_running||i==100u||install()!=0){write_log("[CAM03] Nenhum hook instalado.\n");sceKernelExitDeleteThread(0);return 0;}
    write_log("[CAM03] INICIA DESLIGADO. L+R+TRIANGULO: original, +40, +80, -40.\n");
    while(g_running){
        int down;
        sceKernelDelayThread(50000u);
        sceCtrlPeekBufferPositive(&ctrl,1);
        down=((ctrl.Buttons&COMBO)==COMBO);
        if(down&&!pressed){g_mode=(g_mode+1)%4;log_sample("MODO ALTERADO",prev);}
        pressed=down;
        if(++n>=100u){n=0;log_sample("AMOSTRA 5s",prev);}
    }
    uninstall();write_log("[CAM03] FINALIZADO\n");sceKernelExitDeleteThread(0);return 0;
}
int module_start(SceSize argc,void *argv){(void)argc;(void)argv;
    write_log("\n[CAM03] INICIO V03 Camera Distance experimental ULUS10537.\n");
    g_running=1;g_worker=sceKernelCreateThread("TTTCameraDist03",worker,0x20,0x2400,0,NULL);
    if(g_worker<0){g_running=0;write_log("[CAM03] ERRO sceKernelCreateThread\n");return 0;}
    if(sceKernelStartThread(g_worker,0,NULL)<0){g_running=0;write_log("[CAM03] ERRO sceKernelStartThread\n");}
    return 0;
}
int module_stop(SceSize argc,void *argv){(void)argc;(void)argv;g_running=0;uninstall();return 0;}

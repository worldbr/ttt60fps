/*
 * TTT Camera FOV V0.5 - ULUS10537 / PPSSPP only
 * Experimental perspective-matrix test. No camera-position or viewport changes.
 *
 * Known working call site: 0x089AC4E0 JAL -> 0x089ABE54.
 * 0x089AB784 builds matrices +0x80/+0xC0/+0x100.
 * 0x089ABE54 combines +0xC0 with view (+0x40) into +0x180.
 *
 * This experiment scales only the diagonal X/Y projection coefficients of
 * the +0xC0 matrix, calls the original routine, and restores the coefficients.
 * Thus the original camera projection is not permanently altered or compounded.
 * No claim of exact angular field-of-view increments until validated in game.
 */
#include <pspuser.h>
#include <pspiofilemgr.h>
#include <psputils.h>
#include <pspctrl.h>
#include <stdint.h>
#include <stddef.h>

PSP_MODULE_INFO("TTTCameraFov05", PSP_MODULE_USER, 1, 0);
PSP_NO_CREATE_MAIN_THREAD();

#define SITE_REBUILD        0x089AC4E0u
#define WORD_REBUILD_JAL    0x0E26AF95u
#define WORD_REBUILD_DS     0x02402825u
#define ORIG_REBUILD        0x089ABE54u
#define SITE_DRAW           0x0885AD5Cu
#define WORD_DRAW_JAL       0x0E201A3Fu
#define WORD_DRAW_DS        0x02002025u
#define ORIG_DRAW           0x088068FCu
#define ACTIVE_CAM_PTR      0x08A7E25Cu
#define LOG_FILE            "ms0:/PSP/PLUGINS/TTT_CAMERA_FOV05/diagnostico_fov_v05.txt"
#define COMBO               (PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER | PSP_CTRL_TRIANGLE)

typedef void (*RebuildFn)(void*,void*);
typedef void (*DrawFn)(void*);

static volatile uint32_t g_rebuild=0,g_draw=0,g_applied=0,g_invalid=0;
static volatile uint32_t g_rebuild_cam=0,g_draw_cam=0,g_active_cam=0;
static volatile uint32_t g_p00=0,g_p11=0,g_p23=0,g_p33=0,g_output00=0,g_output11=0;
static volatile uint32_t g_factor_bits=0;
static volatile int g_mode=0,g_running=0,g_installed=0;
static SceUID g_worker=-1;

static uint32_t bits(float f) { union { float f; uint32_t u; } u;u.f=f;return u.u; }
static int valid_ptr(uint32_t p) {
    /* Includes room for a camera struct up to +0x290. */
    return p>=0x08800000u && p<=0x0BBFF000u && (p&3u)==0;
}
static float factor_for_mode(int m) {
    switch(m) {
        case 1: return 0.92f;
        case 2: return 0.84f;
        case 3: return 0.76f;
        default:return 1.0f;
    }
}

static __attribute__((noinline)) void camera_rebuild_fov(void *camera,void *reference) {
    uint32_t addr=(uint32_t)(uintptr_t)camera;
    int mode=g_mode;
    volatile float *proj;
    volatile float *result;
    float p00,p11,factor;
    ++g_rebuild;
    g_rebuild_cam=addr;
    if(!valid_ptr(addr)) {
        ++g_invalid;
        ((RebuildFn)(uintptr_t)ORIG_REBUILD)(camera,reference);
        return;
    }
    proj=(volatile float *)(uintptr_t)(addr+0xC0u);
    result=(volatile float *)(uintptr_t)(addr+0x180u);
    p00=proj[0];p11=proj[5];
    g_p00=bits(p00);g_p11=bits(p11);
    g_p23=bits(proj[11]);g_p33=bits(proj[15]);
    factor=factor_for_mode(mode);
    g_factor_bits=bits(factor);
    /* Very broad sanity range. Check both coordinates, not viewport sizes. */
    if(mode!=0 &&
       ((p00>0.005f && p00<1000.0f)||(p00< -0.005f && p00> -1000.0f)) &&
       ((p11>0.005f && p11<1000.0f)||(p11< -0.005f && p11> -1000.0f))) {
        /* Both axes are scaled equally to retain the original aspect ratio. */
        proj[0]=p00*factor;
        proj[5]=p11*factor;
        ((RebuildFn)(uintptr_t)ORIG_REBUILD)(camera,reference);
        /* Original code has already written its combined matrices. */
        g_output00=bits(result[0]);g_output11=bits(result[5]);
        proj[0]=p00;
        proj[5]=p11;
        ++g_applied;
    } else {
        if(mode!=0) ++g_invalid;
        ((RebuildFn)(uintptr_t)ORIG_REBUILD)(camera,reference);
        g_output00=bits(result[0]);g_output11=bits(result[5]);
    }
}
static __attribute__((noinline)) void camera_draw_probe(void *camera) {
    ++g_draw;
    g_draw_cam=(uint32_t)(uintptr_t)camera;
    g_active_cam=*(volatile uint32_t *)(uintptr_t)ACTIVE_CAM_PTR;
    ((DrawFn)(uintptr_t)ORIG_DRAW)(camera);
}
static uint32_t make_jal(uintptr_t p){ return 0x0C000000u|(((uint32_t)p>>2)&0x03FFFFFFu); }
static void flush_code(uint32_t a){void *p=(void *)(uintptr_t)a;sceKernelDcacheWritebackInvalidateRange(p,4);sceKernelIcacheInvalidateRange(p,4);}
static void put_str(char **p,char *e,const char *s){while(*s&&*p<e)*(*p)++=*s++;}
static void put_uint(char **p,char *e,uint32_t v){char t[10];int i=0;do{t[i++]=(char)('0'+v%10u);v/=10u;}while(v&&i<10);while(i&&*p<e)*(*p)++=t[--i];}
static void put_hex(char **p,char *e,uint32_t v){static const char h[]="0123456789ABCDEF";int i;for(i=28;i>=0;i-=4)if(*p<e)*(*p)++=h[(v>>i)&15];}
static void write_log(const char *s){unsigned n=0;SceUID fd;while(s[n]&&n<1024u)n++;fd=sceIoOpen(LOG_FILE,PSP_O_CREAT|PSP_O_WRONLY|PSP_O_APPEND,0777);if(fd<0)return;sceIoWrite(fd,s,n);sceIoClose(fd);}
static void log_sample(const char *tag,uint32_t *prev){
    char line[530],*p=line,*e=line+sizeof(line)-1;
    uint32_t r=g_rebuild,d=g_draw,a=g_applied;
    put_str(&p,e,"[FOV05] ");put_str(&p,e,tag);
    put_str(&p,e," mode=");put_uint(&p,e,(uint32_t)g_mode);
    put_str(&p,e," rebuild=");put_uint(&p,e,r);put_str(&p,e,"(+");put_uint(&p,e,r-prev[0]);put_str(&p,e,")");
    put_str(&p,e," draw=");put_uint(&p,e,d);put_str(&p,e,"(+");put_uint(&p,e,d-prev[1]);put_str(&p,e,")");
    put_str(&p,e," applied=");put_uint(&p,e,a);put_str(&p,e,"(+");put_uint(&p,e,a-prev[2]);put_str(&p,e,")");
    put_str(&p,e," invalid=");put_uint(&p,e,g_invalid);
    put_str(&p,e," factor_bits=");put_hex(&p,e,g_factor_bits);
    put_str(&p,e," rebuild_cam=0x");put_hex(&p,e,g_rebuild_cam);
    put_str(&p,e," draw_cam=0x");put_hex(&p,e,g_draw_cam);
    put_str(&p,e," active_cam=0x");put_hex(&p,e,g_active_cam);
    put_str(&p,e," p00=");put_hex(&p,e,g_p00);
    put_str(&p,e," p11=");put_hex(&p,e,g_p11);
    put_str(&p,e," p23=");put_hex(&p,e,g_p23);
    put_str(&p,e," p33=");put_hex(&p,e,g_p33);
    put_str(&p,e," out00=");put_hex(&p,e,g_output00);
    put_str(&p,e," out11=");put_hex(&p,e,g_output11);
    put_str(&p,e,"\n");*p=0;write_log(line);
    prev[0]=r;prev[1]=d;prev[2]=a;
}
static int install(void){
    volatile uint32_t *r=(volatile uint32_t *)(uintptr_t)SITE_REBUILD;
    volatile uint32_t *d=(volatile uint32_t *)(uintptr_t)SITE_DRAW;
    uintptr_t wr=(uintptr_t)camera_rebuild_fov,wd=(uintptr_t)camera_draw_probe;
    if((wr&3u)||(wd&3u)||((wr>>28)!=((SITE_REBUILD+4u)>>28))||((wd>>28)!=((SITE_DRAW+4u)>>28))){write_log("[FOV05] ERRO wrappers fora do alcance JAL\n");return -1;}
    if(r[0]!=WORD_REBUILD_JAL||r[1]!=WORD_REBUILD_DS){write_log("[FOV05] ERRO site REBUILD nao confere. Desative outros PRXs.\n");return -2;}
    if(d[0]!=WORD_DRAW_JAL||d[1]!=WORD_DRAW_DS){write_log("[FOV05] ERRO site DRAW nao confere. Desative outros PRXs.\n");return -3;}
    r[0]=make_jal(wr);flush_code(SITE_REBUILD);
    d[0]=make_jal(wd);flush_code(SITE_DRAW);
    g_installed=1;
    write_log("[FOV05] HOOKS INSTALADOS 2/2. Chamadas originais preservadas.\n");
    return 0;
}
static void uninstall(void){
    volatile uint32_t *r=(volatile uint32_t *)(uintptr_t)SITE_REBUILD;
    volatile uint32_t *d=(volatile uint32_t *)(uintptr_t)SITE_DRAW;
    if(!g_installed)return;
    if(r[0]==make_jal((uintptr_t)camera_rebuild_fov)){r[0]=WORD_REBUILD_JAL;flush_code(SITE_REBUILD);}
    if(d[0]==make_jal((uintptr_t)camera_draw_probe)){d[0]=WORD_DRAW_JAL;flush_code(SITE_DRAW);}
    g_installed=0;
}
static int worker(SceSize argc,void *argv){
    unsigned n=0,i;int pressed=0;uint32_t prev[3]={0,0,0};SceCtrlData ctrl;
    (void)argc;(void)argv;
    for(i=0;i<100u&&g_running;i++){
        if(*(volatile uint32_t*)(uintptr_t)SITE_REBUILD==WORD_REBUILD_JAL && *(volatile uint32_t*)(uintptr_t)SITE_DRAW==WORD_DRAW_JAL)break;
        sceKernelDelayThread(100000u);
    }
    if(!g_running||i==100u||install()!=0){write_log("[FOV05] FALHA AO INSTALAR HOOKS.\n");sceKernelExitDeleteThread(0);return 0;}
    write_log("[FOV05] INICIA DESLIGADO. L+R+TRIANGULO: 0 original, 1 0.92, 2 0.84, 3 0.76 (escala matriz).\n");
    while(g_running){
        int down;
        sceKernelDelayThread(50000u);
        sceCtrlPeekBufferPositive(&ctrl,1);
        down=((ctrl.Buttons&COMBO)==COMBO);
        if(down&&!pressed){g_mode=(g_mode+1)%4;log_sample("MODO ALTERADO",prev);}
        pressed=down;
        if(++n>=100u){n=0;log_sample("AMOSTRA 5s",prev);}
    }
    uninstall();write_log("[FOV05] FINALIZADO.\n");sceKernelExitDeleteThread(0);return 0;
}
int module_start(SceSize argc,void *argv){
    (void)argc;(void)argv;
    write_log("\n[FOV05] INICIO V0.5 PROJECAO EXPERIMENTAL ULUS10537.\n");
    g_running=1;
    g_worker=sceKernelCreateThread("TTTCameraFov05",worker,0x20,0x2400,0,NULL);
    if(g_worker<0){g_running=0;write_log("[FOV05] ERRO createThread\n");return 0;}
    if(sceKernelStartThread(g_worker,0,NULL)<0){g_running=0;write_log("[FOV05] ERRO startThread\n");}
    return 0;
}
int module_stop(SceSize argc,void *argv){(void)argc;(void)argv;g_running=0;uninstall();return 0;}

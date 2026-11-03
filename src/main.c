/*
 * TTT Camera FOV V0.6 - ULUS10537 / PPSSPP only
 * Diagnostic A/B projection matrices, without camera/viewport changes.
 *
 * Known working call site: 0x089AC4E0 JAL -> 0x089ABE54.
 * 0x089AB784 builds matrices +0x80/+0xC0/+0x100.
 * 0x089ABE54 combines +0xC0 with view (+0x40) into +0x180.
 *
 * Modes independently scale +0x80 (A), +0xC0 (B), or both matrices.
 * Instrument the final camera upload at 0x089AC4F0 to detect if these
 * matrices reach the renderer. Restore input coefficients after rebuild.
 * These are diagnostics, not validated FOV angles.
 */
#include <pspuser.h>
#include <pspiofilemgr.h>
#include <psputils.h>
#include <pspctrl.h>
#include <stdint.h>
#include <stddef.h>

PSP_MODULE_INFO("TTTCameraFov06", PSP_MODULE_USER, 1, 0);
PSP_NO_CREATE_MAIN_THREAD();

#define SITE_REBUILD        0x089AC4E0u
#define WORD_REBUILD_JAL    0x0E26AF95u
#define WORD_REBUILD_DS     0x02402825u
#define ORIG_REBUILD        0x089ABE54u
#define SITE_UPLOAD         0x089AC4F0u
#define WORD_UPLOAD_JAL     0x0E26B018u
#define WORD_UPLOAD_DS      0x34050001u
#define ORIG_UPLOAD         0x089AC060u
#define MATRIX_A_GPU_PTR    0x08B5E718u
#define MATRIX_B_GPU_PTR    0x08B5E714u
#define SITE_DRAW           0x0885AD5Cu
#define WORD_DRAW_JAL       0x0E201A3Fu
#define WORD_DRAW_DS        0x02002025u
#define ORIG_DRAW           0x088068FCu
#define ACTIVE_CAM_PTR      0x08A7E25Cu
#define LOG_FILE            "ms0:/PSP/PLUGINS/TTT_CAMERA_FOV06/diagnostico_fov_v06.txt"
#define COMBO               (PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER | PSP_CTRL_TRIANGLE)

typedef void (*RebuildFn)(void*,void*);
typedef void (*DrawFn)(void*);
typedef void (*UploadFn)(void*,int);

static volatile uint32_t g_rebuild=0,g_draw=0,g_upload=0,g_applied=0,g_invalid=0;
static volatile uint32_t g_mod_a=0,g_mod_b=0;
static volatile uint32_t g_rebuild_cam=0,g_draw_cam=0,g_upload_cam=0,g_active_cam=0;
static volatile uint32_t g_a00=0,g_a11=0,g_b00=0,g_b11=0,g_c00=0,g_c11=0;
static volatile uint32_t g_outA00=0,g_outA11=0,g_outB00=0,g_outB11=0;
static volatile uint32_t g_sentA00=0,g_sentA11=0,g_sentB00=0,g_sentB11=0;
static volatile uint32_t g_gpuA00=0,g_gpuB00=0;
static volatile uint32_t g_factor_bits=0;
static volatile int g_mode=0,g_running=0,g_installed=0;
static SceUID g_worker=-1;

static uint32_t bits(float f) { union { float f; uint32_t u; } u;u.f=f;return u.u; }
static int valid_ptr(uint32_t p) {
    /* Includes room for a camera struct up to +0x290. */
    return p>=0x08800000u && p<=0x0BBFF000u && (p&3u)==0;
}
static int valid_scale(float x,float y) {
    return ((x>0.001f&&x<50000.f)||(x< -0.001f&&x> -50000.f)) &&
           ((y>0.001f&&y<50000.f)||(y< -0.001f&&y> -50000.f));
}
static __attribute__((noinline)) void camera_rebuild_fov(void *camera,void *reference) {
    uint32_t addr=(uint32_t)(uintptr_t)camera;
    volatile float *matA,*matB,*matC,*outA,*outB;
    float a0,a5,b0,b5;
    int mode=g_mode, touchA=0,touchB=0;
    ++g_rebuild;
    g_rebuild_cam=addr;
    if(!valid_ptr(addr)) {
        ++g_invalid;
        ((RebuildFn)(uintptr_t)ORIG_REBUILD)(camera,reference);
        return;
    }
    matA=(volatile float *)(uintptr_t)(addr+0x80u);
    matB=(volatile float *)(uintptr_t)(addr+0xC0u);
    matC=(volatile float *)(uintptr_t)(addr+0x100u);
    outA=(volatile float *)(uintptr_t)(addr+0x140u);
    outB=(volatile float *)(uintptr_t)(addr+0x180u);
    a0=matA[0];a5=matA[5];b0=matB[0];b5=matB[5];
    g_a00=bits(a0);g_a11=bits(a5);
    g_b00=bits(b0);g_b11=bits(b5);
    g_c00=bits(matC[0]);g_c11=bits(matC[5]);
    g_factor_bits=bits(mode==0 ? 1.0f : 0.70f);
    if((mode==1||mode==3)&&valid_scale(a0,a5)) {
        matA[0]=a0*0.70f;
        matA[5]=a5*0.70f;
        touchA=1;
        ++g_mod_a;
    }
    if((mode==2||mode==3)&&valid_scale(b0,b5)) {
        matB[0]=b0*0.70f;
        matB[5]=b5*0.70f;
        touchB=1;
        ++g_mod_b;
    }
    if(mode!=0&&!touchA&&!touchB) ++g_invalid;
    ((RebuildFn)(uintptr_t)ORIG_REBUILD)(camera,reference);
    g_outA00=bits(outA[0]);g_outA11=bits(outA[5]);
    g_outB00=bits(outB[0]);g_outB11=bits(outB[5]);
    if(touchA) {matA[0]=a0;matA[5]=a5;}
    if(touchB) {matB[0]=b0;matB[5]=b5;}
    if(touchA||touchB) ++g_applied;
}
static __attribute__((noinline)) void camera_upload_probe(void *camera,int flag) {
    uint32_t addr=(uint32_t)(uintptr_t)camera;
    ++g_upload;
    g_upload_cam=addr;
    if(valid_ptr(addr)) {
        volatile float *a=(volatile float *)(uintptr_t)(addr+0x140u);
        volatile float *b=(volatile float *)(uintptr_t)(addr+0x180u);
        g_sentA00=bits(a[0]);g_sentA11=bits(a[5]);
        g_sentB00=bits(b[0]);g_sentB11=bits(b[5]);
    }
    ((UploadFn)(uintptr_t)ORIG_UPLOAD)(camera,flag);
    /* Global pointers used as upload destinations. Only read if valid. */
    {
        uint32_t ga=*(volatile uint32_t *)(uintptr_t)MATRIX_A_GPU_PTR;
        uint32_t gb=*(volatile uint32_t *)(uintptr_t)MATRIX_B_GPU_PTR;
        if(valid_ptr(ga))g_gpuA00=bits(*(volatile float*)(uintptr_t)ga);
        if(valid_ptr(gb))g_gpuB00=bits(*(volatile float*)(uintptr_t)gb);
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
    char line[970],*p=line,*e=line+sizeof(line)-1;
    uint32_t r=g_rebuild,d=g_draw,u=g_upload,a=g_applied;
    put_str(&p,e,"[FOV06] ");put_str(&p,e,tag);
    put_str(&p,e," mode=");put_uint(&p,e,(uint32_t)g_mode);
    put_str(&p,e," rebuild=");put_uint(&p,e,r);put_str(&p,e,"(+");put_uint(&p,e,r-prev[0]);put_str(&p,e,")");
    put_str(&p,e," draw=");put_uint(&p,e,d);put_str(&p,e,"(+");put_uint(&p,e,d-prev[1]);put_str(&p,e,")");
    put_str(&p,e," upload=");put_uint(&p,e,u);put_str(&p,e,"(+");put_uint(&p,e,u-prev[2]);put_str(&p,e,")");
    put_str(&p,e," applied=");put_uint(&p,e,a);put_str(&p,e,"(+");put_uint(&p,e,a-prev[3]);put_str(&p,e,")");
    put_str(&p,e," A=");put_uint(&p,e,g_mod_a);put_str(&p,e," B=");put_uint(&p,e,g_mod_b);
    put_str(&p,e," invalid=");put_uint(&p,e,g_invalid);
    put_str(&p,e," factor=");put_hex(&p,e,g_factor_bits);
    put_str(&p,e," rebuild_cam=");put_hex(&p,e,g_rebuild_cam);
    put_str(&p,e," upload_cam=");put_hex(&p,e,g_upload_cam);
    put_str(&p,e," draw_cam=");put_hex(&p,e,g_draw_cam);
    put_str(&p,e," active=");put_hex(&p,e,g_active_cam);
    put_str(&p,e," inA=");put_hex(&p,e,g_a00);put_str(&p,e,",");put_hex(&p,e,g_a11);
    put_str(&p,e," inB=");put_hex(&p,e,g_b00);put_str(&p,e,",");put_hex(&p,e,g_b11);
    put_str(&p,e," inC=");put_hex(&p,e,g_c00);put_str(&p,e,",");put_hex(&p,e,g_c11);
    put_str(&p,e," outA=");put_hex(&p,e,g_outA00);put_str(&p,e,",");put_hex(&p,e,g_outA11);
    put_str(&p,e," outB=");put_hex(&p,e,g_outB00);put_str(&p,e,",");put_hex(&p,e,g_outB11);
    put_str(&p,e," sentA=");put_hex(&p,e,g_sentA00);put_str(&p,e,",");put_hex(&p,e,g_sentA11);
    put_str(&p,e," sentB=");put_hex(&p,e,g_sentB00);put_str(&p,e,",");put_hex(&p,e,g_sentB11);
    put_str(&p,e," gpuA=");put_hex(&p,e,g_gpuA00);put_str(&p,e," gpuB=");put_hex(&p,e,g_gpuB00);
    put_str(&p,e,"\n");*p=0;write_log(line);
    prev[0]=r;prev[1]=d;prev[2]=u;prev[3]=a;
}
static int install(void){
    volatile uint32_t *r=(volatile uint32_t *)(uintptr_t)SITE_REBUILD;
    volatile uint32_t *d=(volatile uint32_t *)(uintptr_t)SITE_DRAW;
    volatile uint32_t *u=(volatile uint32_t *)(uintptr_t)SITE_UPLOAD;
    uintptr_t wr=(uintptr_t)camera_rebuild_fov,wd=(uintptr_t)camera_draw_probe,wu=(uintptr_t)camera_upload_probe;
    if((wr&3u)||(wd&3u)||(wu&3u)||
       ((wr>>28)!=((SITE_REBUILD+4u)>>28))||
       ((wd>>28)!=((SITE_DRAW+4u)>>28))||
       ((wu>>28)!=((SITE_UPLOAD+4u)>>28))){
        write_log("[FOV06] ERRO wrappers fora de alcance JAL.\n");return -1;
    }
    if(r[0]!=WORD_REBUILD_JAL||r[1]!=WORD_REBUILD_DS||
       d[0]!=WORD_DRAW_JAL||d[1]!=WORD_DRAW_DS||
       u[0]!=WORD_UPLOAD_JAL||u[1]!=WORD_UPLOAD_DS){
        write_log("[FOV06] ERRO opcode/delay do ELF; desative outros PRXs.\n");return -2;
    }
    r[0]=make_jal(wr);flush_code(SITE_REBUILD);
    d[0]=make_jal(wd);flush_code(SITE_DRAW);
    u[0]=make_jal(wu);flush_code(SITE_UPLOAD);
    g_installed=1;
    write_log("[FOV06] HOOKS INSTALADOS 3/3. Chamadas originais preservadas.\n");
    return 0;
}
static void uninstall(void){
    volatile uint32_t *r=(volatile uint32_t *)(uintptr_t)SITE_REBUILD;
    volatile uint32_t *d=(volatile uint32_t *)(uintptr_t)SITE_DRAW;
    volatile uint32_t *u=(volatile uint32_t *)(uintptr_t)SITE_UPLOAD;
    if(!g_installed)return;
    if(r[0]==make_jal((uintptr_t)camera_rebuild_fov)){r[0]=WORD_REBUILD_JAL;flush_code(SITE_REBUILD);}
    if(d[0]==make_jal((uintptr_t)camera_draw_probe)){d[0]=WORD_DRAW_JAL;flush_code(SITE_DRAW);}
    if(u[0]==make_jal((uintptr_t)camera_upload_probe)){u[0]=WORD_UPLOAD_JAL;flush_code(SITE_UPLOAD);}
    g_installed=0;
}
static int worker(SceSize argc,void *argv){
    unsigned n=0,i;int pressed=0;uint32_t prev[4]={0,0,0,0};SceCtrlData ctrl;
    (void)argc;(void)argv;
    for(i=0;i<100u&&g_running;i++){
        if(*(volatile uint32_t*)(uintptr_t)SITE_REBUILD==WORD_REBUILD_JAL && *(volatile uint32_t*)(uintptr_t)SITE_DRAW==WORD_DRAW_JAL)break;
        sceKernelDelayThread(100000u);
    }
    if(!g_running||i==100u||install()!=0){write_log("[FOV06] FALHA AO INSTALAR HOOKS.\n");sceKernelExitDeleteThread(0);return 0;}
    write_log("[FOV06] INICIA DESLIGADO. L+R+TRIANGULO: 0 original; 1 matriz A (+80); 2 B (+C0); 3 A+B. Fator 0.70.\n");
    while(g_running){
        int down;
        sceKernelDelayThread(50000u);
        sceCtrlPeekBufferPositive(&ctrl,1);
        down=((ctrl.Buttons&COMBO)==COMBO);
        if(down&&!pressed){g_mode=(g_mode+1)%4;log_sample("MODO ALTERADO",prev);}
        pressed=down;
        if(++n>=100u){n=0;log_sample("AMOSTRA 5s",prev);}
    }
    uninstall();write_log("[FOV06] FINALIZADO.\n");sceKernelExitDeleteThread(0);return 0;
}
int module_start(SceSize argc,void *argv){
    (void)argc;(void)argv;
    write_log("\n[FOV06] INICIO V0.6 DIAGNOSTICO MATRIZES A/B ULUS10537.\n");
    g_running=1;
    g_worker=sceKernelCreateThread("TTTCameraFov06",worker,0x20,0x2400,0,NULL);
    if(g_worker<0){g_running=0;write_log("[FOV06] ERRO createThread\n");return 0;}
    if(sceKernelStartThread(g_worker,0,NULL)<0){g_running=0;write_log("[FOV06] ERRO startThread\n");}
    return 0;
}
int module_stop(SceSize argc,void *argv){(void)argc;(void)argv;g_running=0;uninstall();return 0;}

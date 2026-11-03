/*
 * TTT Camera Safe V04.1 - ULUS10537 / PPSSPP only
 * Corrects V04 bug: restrictive family-of-4 address check prevented ALL writes.
 * Modes 1-3 follow exactly the V03 validated change, without new pointer gates.
 * Mode 4 adds guarded camera-type + movement attenuation (experimental).
 * No viewport, FOV, FPS or effect changes. Starts OFF.
 * JAL sites and delay slots identical to V03 and V04, preserve original functions.
 */
#include <pspuser.h>
#include <pspiofilemgr.h>
#include <psputils.h>
#include <pspctrl.h>
#include <stdint.h>
#include <stddef.h>

PSP_MODULE_INFO("TTTCameraSafe041", PSP_MODULE_USER, 1, 0);
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
#define LOG_FILE "ms0:/PSP/PLUGINS/TTT_CAMERA_SAFE041/diagnostico_camera_v041.txt"
#define COMBO (PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER | PSP_CTRL_TRIANGLE)

typedef void (*RebuildFn)(void*,void*);
typedef void (*DrawFn)(void*);
static volatile uint32_t g_rebuild=0, g_draw=0, g_modified=0, g_rejected=0;
static volatile uint32_t g_mode3_modified=0,g_auto_modified=0,g_auto_skipped=0,g_auto_far=0,g_auto_script=0,g_auto_not_ready=0;
static volatile uint32_t g_last_rebuild=0,g_last_draw=0,g_active_draw=0,g_last_bits[3]={0,0,0};
static volatile int g_mode=0,g_running=0,g_installed=0;
static volatile float g_last_shift=0.0f,g_smooth_shift=0.0f;
static volatile uint32_t g_track_events=0,g_track_seq=0;
static volatile int g_track_cooldown=0;
static float g_px=0,g_py=0,g_pz=0,g_dx=0,g_dy=0,g_dz=0;
static int g_track_valid=0;
static SceUID g_worker=-1;

static uint32_t bits(float f){union{float f;uint32_t u;} t;t.f=f;return t.u;}
static int valid_ptr(uint32_t p){return p>=0x08800000u && p<=0x0BBFFD00u && !(p&3u);}
static uint32_t abs_diff(uint32_t a,uint32_t b){return a>=b ? a-b : b-a;}
static float direct_distance(int mode){
    switch(mode){case 1:return -15.f;case 2:return -25.f;case 3:return -40.f;default:return 0.f;}
}
/* NOTE: mode4 is a heuristic; it does not know fighter coordinates / screen margins. */
static int auto_safe(uint32_t cam){
    uint32_t draw=g_last_draw;
    uint32_t active=g_active_draw;
    if(!valid_ptr(draw) || !valid_ptr(active)){++g_auto_not_ready;return 0;}
    /* In prior logs, normal battle cameras were 0x0975..., scripted cuts 0x08E4...
       This is a heuristic, NOT a universal engine invariant. */
    if(draw<0x09000000u || active<0x09000000u || abs_diff(draw,active)>0x2000u){
        ++g_auto_script;return 0;
    }
    /* Battle camera rebuild records can be 2 or 4 per frame, spaced 0x290.
       Do NOT require exactly 4, as V04 erroneously did. */
    if(abs_diff(cam,draw)>0x1800u){++g_auto_far;return 0;}
    return 1;
}
/* Called for each camera matrix in draw preparation. Originals are called once. */
static void shift_camera(void *p){
    uint32_t cam=(uint32_t)(uintptr_t)p;
    volatile float *m;
    int mode=g_mode;
    float x,y,z,dx,dy,dz,mag,shift;
    if(mode==0) {g_smooth_shift=0;g_track_valid=0;return;}
    if(!valid_ptr(cam)){++g_rejected;return;}
    if(mode==4 && !auto_safe(cam)){++g_auto_skipped;return;}
    m=(volatile float *)(uintptr_t)(cam+0x40u);
    dx=m[8];dy=m[9];dz=m[10];
    x=m[12];y=m[13];z=m[14];
    mag=dx*dx+dy*dy+dz*dz;
    if(!(mag>0.85f && mag<1.15f && x>-50000.f && x<50000.f && y>-50000.f && y<50000.f && z>-50000.f && z<50000.f)){
        ++g_rejected;return;
    }
    if(mode==4){
        uint32_t frame=g_draw;
        /* Update detector only once per drawn frame, not once per rebuilt camera. */
        if(g_track_seq!=frame){
            float target, diff;
            if(g_track_valid){
                float px=x-g_px, py=y-g_py, pz=z-g_pz;
                float dot=dx*g_dx+dy*g_dy+dz*g_dz;
                if(px*px+py*py+pz*pz>144.0f || dot<0.985f){
                    g_track_cooldown=15;
                    ++g_track_events;
                }else if(g_track_cooldown>0){--g_track_cooldown;}
            }
            g_px=x;g_py=y;g_pz=z;
            g_dx=dx;g_dy=dy;g_dz=dz;
            g_track_valid=1;
            g_track_seq=frame;
            target=g_track_cooldown ? -8.f : -40.f;
            diff=target-g_smooth_shift;
            if(diff>3.f) diff=3.f;
            if(diff< -3.f) diff=-3.f;
            g_smooth_shift+=diff;
        }
        shift=g_smooth_shift;
    } else {
        shift=direct_distance(mode);
    }
    if(shift==0.f)return;
    g_last_bits[0]=bits(x);g_last_bits[1]=bits(y);g_last_bits[2]=bits(z);
    g_last_shift=shift;
    m[12]=x+dx*shift;
    m[13]=y+dy*shift;
    m[14]=z+dz*shift;
    ++g_modified;
    if(mode==4)++g_auto_modified;
    if(mode==3)++g_mode3_modified;
}
static __attribute__((noinline)) void camera_rebuild_hook(void *camera,void *reference){
    ++g_rebuild;g_last_rebuild=(uint32_t)(uintptr_t)camera;
    shift_camera(camera);
    ((RebuildFn)(uintptr_t)ORIG_REBUILD)(camera,reference);
}
static __attribute__((noinline)) void camera_draw_probe(void *camera){
    ++g_draw;g_last_draw=(uint32_t)(uintptr_t)camera;
    g_active_draw=*(volatile uint32_t*)(uintptr_t)ACTIVE_CAM_PTR;
    ((DrawFn)(uintptr_t)ORIG_DRAW)(camera);
}
static uint32_t make_jal(uintptr_t p){return 0x0C000000u|(((uint32_t)p>>2)&0x03FFFFFFu);}
static void flush_code(uint32_t a){void *p=(void*)(uintptr_t)a;sceKernelDcacheWritebackInvalidateRange(p,4);sceKernelIcacheInvalidateRange(p,4);}
static void put_str(char **p,char *e,const char *s){while(*s && *p<e)*(*p)++=*s++;}
static void put_uint(char **p,char *e,uint32_t v){char t[10];int i=0;do{t[i++]=(char)('0'+v%10u);v/=10u;}while(v&&i<10);while(i&&*p<e)*(*p)++=t[--i];}
static void put_hex(char **p,char *e,uint32_t v){static const char h[]="0123456789ABCDEF";int i;for(i=28;i>=0;i-=4)if(*p<e)*(*p)++=h[(v>>i)&15];}
static void write_log(const char *s){unsigned n=0;SceUID fd;while(s[n]&&n<1024u)n++;fd=sceIoOpen(LOG_FILE,PSP_O_CREAT|PSP_O_WRONLY|PSP_O_APPEND,0777);if(fd<0)return;sceIoWrite(fd,s,n);sceIoClose(fd);}
static void log_sample(const char *tag,uint32_t *prev){
    char line[700],*p=line,*e=line+sizeof(line)-1;
    uint32_t r=g_rebuild,d=g_draw,m=g_modified;
    put_str(&p,e,"[CAM041] ");put_str(&p,e,tag);
    put_str(&p,e," mode=");put_uint(&p,e,(uint32_t)g_mode);
    put_str(&p,e," rebuild=");put_uint(&p,e,r);put_str(&p,e,"(+");put_uint(&p,e,r-prev[0]);put_str(&p,e,")");
    put_str(&p,e," draw=");put_uint(&p,e,d);put_str(&p,e,"(+");put_uint(&p,e,d-prev[1]);put_str(&p,e,")");
    put_str(&p,e," modified=");put_uint(&p,e,m);put_str(&p,e,"(+");put_uint(&p,e,m-prev[2]);put_str(&p,e,")");
    put_str(&p,e," rejected=");put_uint(&p,e,g_rejected);
    put_str(&p,e," mode3_modified=");put_uint(&p,e,g_mode3_modified);
    put_str(&p,e," auto_modified=");put_uint(&p,e,g_auto_modified);
    put_str(&p,e," auto_skipped=");put_uint(&p,e,g_auto_skipped);
    put_str(&p,e," not_ready=");put_uint(&p,e,g_auto_not_ready);
    put_str(&p,e," script_skip=");put_uint(&p,e,g_auto_script);
    put_str(&p,e," far_skip=");put_uint(&p,e,g_auto_far);
    put_str(&p,e," track_event=");put_uint(&p,e,g_track_events);
    put_str(&p,e," cooldown=");put_uint(&p,e,(uint32_t)g_track_cooldown);
    put_str(&p,e," shift_bits=");put_hex(&p,e,bits(g_last_shift));
    put_str(&p,e," rebuild_cam=0x");put_hex(&p,e,g_last_rebuild);
    put_str(&p,e," draw_cam=0x");put_hex(&p,e,g_last_draw);
    put_str(&p,e," active_cam=0x");put_hex(&p,e,g_active_draw);
    put_str(&p,e," xyz_bits=");put_hex(&p,e,g_last_bits[0]);put_str(&p,e,",");put_hex(&p,e,g_last_bits[1]);put_str(&p,e,",");put_hex(&p,e,g_last_bits[2]);
    put_str(&p,e,"\n");*p=0;write_log(line);
    prev[0]=r;prev[1]=d;prev[2]=m;
}
static int install(void){
    volatile uint32_t *r=(volatile uint32_t*)(uintptr_t)SITE_REBUILD;
    volatile uint32_t *d=(volatile uint32_t*)(uintptr_t)SITE_DRAW;
    uintptr_t wr=(uintptr_t)camera_rebuild_hook,wd=(uintptr_t)camera_draw_probe;
    if((wr&3u)||(wd&3u)||((wr>>28)!=((SITE_REBUILD+4u)>>28))||((wd>>28)!=((SITE_DRAW+4u)>>28))){write_log("[CAM041] ERRO enderecos wrappers fora do alcance JAL\n");return -1;}
    if(r[0]!=WORD_REBUILD_JAL||r[1]!=WORD_REBUILD_DS){write_log("[CAM041] ERRO instrucoes no REBUILD nao conferem\n");return -2;}
    if(d[0]!=WORD_DRAW_JAL||d[1]!=WORD_DRAW_DS){write_log("[CAM041] ERRO instrucoes no DRAW nao conferem\n");return -3;}
    r[0]=make_jal(wr);flush_code(SITE_REBUILD);
    d[0]=make_jal(wd);flush_code(SITE_DRAW);
    g_installed=1;
    write_log("[CAM041] HOOKS INSTALADOS 2/2. Chamadas originais preservadas.\n");
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
        if(*(volatile uint32_t*)(uintptr_t)SITE_REBUILD==WORD_REBUILD_JAL && *(volatile uint32_t*)(uintptr_t)SITE_DRAW==WORD_DRAW_JAL)break;
        sceKernelDelayThread(100000u);
    }
    if(!g_running || i==100u || install()!=0){write_log("[CAM041] Nenhum hook instalado.\n");sceKernelExitDeleteThread(0);return 0;}
    write_log("[CAM041] L+R+TRIANGULO: 0 original, 1=-15, 2=-25, 3=-40 igual V03, 4=auto -40/-8.\n");
    while(g_running){
        int down;
        sceKernelDelayThread(50000u);
        sceCtrlPeekBufferPositive(&ctrl,1);
        down=((ctrl.Buttons&COMBO)==COMBO);
        if(down&&!pressed){
            int newmode=(g_mode+1)%5;
            g_mode=newmode;
            g_track_valid=0;g_track_cooldown=0;g_track_seq=0;
            g_smooth_shift=0;g_last_shift=0;
            log_sample("MODO ALTERADO",prev);
        }
        pressed=down;
        if(++n>=100u){n=0;log_sample("AMOSTRA 5s",prev);}
    }
    uninstall();write_log("[CAM041] FINALIZADO\n");sceKernelExitDeleteThread(0);return 0;
}
int module_start(SceSize argc,void *argv){
    (void)argc;(void)argv;
    write_log("\n[CAM041] INICIO V04.1 CORRECAO ULUS10537.\n");
    g_running=1;g_worker=sceKernelCreateThread("TTTCameraSafe041",worker,0x20,0x2400,0,NULL);
    if(g_worker<0){g_running=0;write_log("[CAM041] ERRO createThread\n");return 0;}
    if(sceKernelStartThread(g_worker,0,NULL)<0){g_running=0;write_log("[CAM041] ERRO startThread\n");}
    return 0;
}
int module_stop(SceSize argc,void *argv){(void)argc;(void)argv;g_running=0;uninstall();return 0;}

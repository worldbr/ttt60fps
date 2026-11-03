/*
 * TTT Camera Distance experimental V0.1 - ULUS10537 - PPSSPP
 * Reference ELF SHA256:
 * 834605685873da14647b1dfe7bf332e326dd2a99a48840877da13d211ea18b1e
 *
 * Unlike viewport cheats, this modifies the TRANSLATION of the camera's
 * transform (cam + 0x40, translation + 0x30..0x38), along its local +Z axis.
 * Actual zoom direction and effect compatibility MUST be checked in-game.
 *
 * Two validated JAL callsites call the camera matrix rebuild routine
 * 0x089ABE54. Delay-slot instructions remain intact. Both hooks call the
 * original destination once; NO gameplay logic/frame rate is modified.
 */
#include <pspuser.h>
#include <pspiofilemgr.h>
#include <psputils.h>
#include <pspctrl.h>
#include <stdint.h>
#include <stddef.h>

PSP_MODULE_INFO("TTTCamDist01", PSP_MODULE_USER, 1, 0);
PSP_NO_CREATE_MAIN_THREAD();

#define LOG_PATH "ms0:/PSP/PLUGINS/TTT_CAMERA_DIST01/diagnostico_camera.txt"
#define SITE_A 0x089AB02Cu
#define SITE_B 0x089AB734u
#define ORIG_JAL 0x0E26AF95u
#define ORIG_A_DELAY 0x02202825u
#define ORIG_B_DELAY 0x02E02825u
#define ORIG_FN 0x089ABE54u
#define BUTTON_COMBO (PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER | PSP_CTRL_TRIANGLE)

typedef void (*RebuildFn)(void *, void *);
static volatile uint32_t g_count[2];
static volatile uint32_t g_applied;
static volatile uint32_t g_skipped;
static volatile uint32_t g_last_cam;
static volatile uint32_t g_last_xbits;
static volatile uint32_t g_last_ybits;
static volatile uint32_t g_last_zbits;
static volatile uint32_t g_last_basis_len2bits;
static volatile int g_mode = 0;  /* SAFE START: press L+R+TRIANGLE to enable */
static volatile int g_installed = 0;
static volatile int g_running = 0;
static SceUID g_thread = -1;

static uint32_t float_bits(float x) { union { float f; uint32_t u; } value; value.f = x; return value.u; }

/* Modes: off, +80, +160, -80 world units.
   Negative mode is diagnostic if +Z moves forward instead of backward. */
static float mode_distance(int mode) {
    switch (mode) {
    case 1: return 80.0f;
    case 2: return 160.0f;
    case 3: return -80.0f;
    default: return 0.0f;
    }
}

static void adjust_camera(void *ptr) {
    uintptr_t address = (uintptr_t)ptr;
    float dist = mode_distance(g_mode);
    volatile float *m;
    float dx, dy, dz, len2, x, y, z;
    if (dist == 0.0f) return;
    if (address < 0x08800000u || address > 0x0BBFF000u || (address & 3u)) {
        ++g_skipped;
        return;
    }
    m = (volatile float *)(address + 0x40u);
    /* PSP's 4x4 camera transform: local Z = third column.
       The third column (m[8..10]) is expected to be a normalized axis. */
    dx = m[8]; dy = m[9]; dz = m[10];
    x = m[12]; y = m[13]; z = m[14];
    len2 = dx*dx + dy*dy + dz*dz;
    /* Protect against invalid/uninitialized matrices and absurd coordinates.
       Explicit comparisons with NaNs evaluate false. */
    if (!(len2 > 0.50f && len2 < 1.50f &&
          x > -50000.0f && x < 50000.0f &&
          y > -50000.0f && y < 50000.0f &&
          z > -50000.0f && z < 50000.0f)) {
        ++g_skipped;
        return;
    }
    g_last_cam = (uint32_t)address;
    g_last_xbits = float_bits(x);
    g_last_ybits = float_bits(y);
    g_last_zbits = float_bits(z);
    g_last_basis_len2bits = float_bits(len2);
    m[12] = x + dx * dist;
    m[13] = y + dy * dist;
    m[14] = z + dz * dist;
    ++g_applied;
}

/* The original JAL delay slots set a1 ($5), so wrappers receive it intact. */
static __attribute__((noinline)) void wrapper_a(void *cam, void *ref) {
    ++g_count[0];
    adjust_camera(cam);
    ((RebuildFn)(uintptr_t)ORIG_FN)(cam, ref);
}
static __attribute__((noinline)) void wrapper_b(void *cam, void *ref) {
    ++g_count[1];
    adjust_camera(cam);
    ((RebuildFn)(uintptr_t)ORIG_FN)(cam, ref);
}

static uint32_t jal_word(uintptr_t address) {
    return 0x0C000000u | (((uint32_t)address >> 2) & 0x03FFFFFFu);
}
static void flush_code(void *ptr) {
    sceKernelDcacheWritebackInvalidateRange(ptr, 4);
    sceKernelIcacheInvalidateRange(ptr, 4);
}
static void add_str(char **p, char *end, const char *s) {
    while (s && *s && *p < end) *(*p)++ = *s++;
}
static void add_u32(char **p, char *end, uint32_t n) {
    char rev[10]; unsigned i=0;
    do { rev[i++] = (char)('0' + n % 10u); n /= 10u; } while(n && i<10);
    while(i && *p < end) *(*p)++ = rev[--i];
}
static void add_hex(char **p, char *end, uint32_t n) {
    static const char hex[] = "0123456789ABCDEF";
    int i;
    for (i=28; i>=0 && *p<end; i-=4) *(*p)++ = hex[(n >> i) & 15u];
}
static void log_msg(const char *s) {
    SceUID fd; size_t n=0;
    if (!s) return;
    while (s[n] && n<1024u) ++n;
    fd = sceIoOpen(LOG_PATH, PSP_O_CREAT|PSP_O_APPEND|PSP_O_WRONLY, 0777);
    if (fd < 0) return;
    sceIoWrite(fd, s, (SceSize)n);
    sceIoClose(fd);
}
static void sample_log(const char *title, uint32_t *last) {
    char line[420]; char *p=line; char *end=line+sizeof(line)-1;
    uint32_t ca=g_count[0], cb=g_count[1], applied=g_applied, skipped=g_skipped;
    add_str(&p,end,"[CAM01] "); add_str(&p,end,title);
    add_str(&p,end," modo="); add_u32(&p,end,(uint32_t)g_mode);
    add_str(&p,end," call_A="); add_u32(&p,end,ca); add_str(&p,end,"(+"); add_u32(&p,end,ca-last[0]); add_str(&p,end,")");
    add_str(&p,end," call_B="); add_u32(&p,end,cb); add_str(&p,end,"(+"); add_u32(&p,end,cb-last[1]); add_str(&p,end,")");
    add_str(&p,end," aplicado="); add_u32(&p,end,applied); add_str(&p,end,"(+"); add_u32(&p,end,applied-last[2]); add_str(&p,end,")");
    add_str(&p,end," ignorado="); add_u32(&p,end,skipped);
    add_str(&p,end," cam=0x"); add_hex(&p,end,g_last_cam);
    add_str(&p,end," posbits="); add_hex(&p,end,g_last_xbits);
    add_str(&p,end,","); add_hex(&p,end,g_last_ybits);
    add_str(&p,end,","); add_hex(&p,end,g_last_zbits);
    add_str(&p,end," len2bits="); add_hex(&p,end,g_last_basis_len2bits);
    add_str(&p,end,"\n"); *p=0;
    last[0]=ca;last[1]=cb;last[2]=applied;
    log_msg(line);
}
static void log_err(const char *detail, uint32_t actual) {
    char line[220]; char *p=line; char *end=line+sizeof(line)-1;
    add_str(&p,end,"[CAM01] ERRO ");add_str(&p,end,detail);
    add_str(&p,end," obtido=0x");add_hex(&p,end,actual);
    add_str(&p,end,"\n");*p=0; log_msg(line);
}
static int install(void) {
    volatile uint32_t *a=(volatile uint32_t *)(uintptr_t)SITE_A;
    volatile uint32_t *b=(volatile uint32_t *)(uintptr_t)SITE_B;
    uintptr_t wa=(uintptr_t)wrapper_a, wb=(uintptr_t)wrapper_b;
    if ((wa & 3u) || (wb & 3u) ||
        ((wa>>28) != ((SITE_A+4u)>>28)) || ((wb>>28) != ((SITE_B+4u)>>28))) {
        log_err("wrapper fora do alcance JAL",(uint32_t)wa); return -1;
    }
    if (a[0]!=ORIG_JAL) {log_err("opcode A nao original",a[0]);return -2;}
    if (a[1]!=ORIG_A_DELAY) {log_err("delay A incorreto",a[1]);return -3;}
    if (b[0]!=ORIG_JAL) {log_err("opcode B nao original",b[0]);return -4;}
    if (b[1]!=ORIG_B_DELAY) {log_err("delay B incorreto",b[1]);return -5;}
    a[0]=jal_word(wa);flush_code((void *)(uintptr_t)SITE_A);
    b[0]=jal_word(wb);flush_code((void *)(uintptr_t)SITE_B);
    g_installed=1;
    log_msg("[CAM01] HOOKS INSTALADOS 2/2. Rotina original preservada.\n");
    return 0;
}
static void uninstall(void) {
    volatile uint32_t *a=(volatile uint32_t *)(uintptr_t)SITE_A;
    volatile uint32_t *b=(volatile uint32_t *)(uintptr_t)SITE_B;
    if (!g_installed) return;
    if (a[0]==jal_word((uintptr_t)wrapper_a)) {a[0]=ORIG_JAL;flush_code((void *)(uintptr_t)SITE_A);}
    if (b[0]==jal_word((uintptr_t)wrapper_b)) {b[0]=ORIG_JAL;flush_code((void *)(uintptr_t)SITE_B);}
    g_installed=0;
}
static int worker(SceSize argc, void *argv) {
    uint32_t last[3]={0,0,0}; unsigned tries; int held=0;unsigned ticks=0;
    SceCtrlData buttons;
    (void)argc; (void)argv;
    log_msg("[CAM01] Worker iniciou; verificando EBOOT.\n");
    for (tries=0; tries<100u && g_running; ++tries) {
        if (*(volatile uint32_t *)(uintptr_t)SITE_A==ORIG_JAL) break;
        sceKernelDelayThread(100000u);
    }
    if (tries==100u || !g_running || install()!=0) {
        log_msg("[CAM01] Sem hook: nenhum codigo de camera alterado.\n");
        sceKernelExitDeleteThread(0);return 0;
    }
    log_msg("[CAM01] INICIA DESLIGADO. L+R+TRIANGULO: 0, +80, +160, -80 unidades.\n");
    while (g_running) {
        int down;
        sceKernelDelayThread(50000u);
        sceCtrlPeekBufferPositive(&buttons,1);
        down=((buttons.Buttons & BUTTON_COMBO)==BUTTON_COMBO);
        if (down && !held) {
            g_mode=(g_mode+1)%4;
            sample_log("MUDANCA DE MODO",last);
        }
        held=down;
        if (++ticks>=100u) {sample_log("AMOSTRA 5s",last);ticks=0;}
    }
    uninstall(); log_msg("[CAM01] Hook removido.\n");
    sceKernelExitDeleteThread(0); return 0;
}
int module_start(SceSize args, void *argp) {
    (void)args;(void)argp;
    log_msg("\n[CAM01] INICIO CAMARA TESTE V01 (sem alterar viewport).\n");
    g_running=1;
    g_thread=sceKernelCreateThread("TTTCamWorker01",worker,0x20,0x2400,0,NULL);
    if(g_thread<0) {g_running=0;log_msg("[CAM01] Erro criar thread.\n");return 0;}
    if(sceKernelStartThread(g_thread,0,NULL)<0) {g_running=0;log_msg("[CAM01] Erro iniciar thread.\n");}
    return 0;
}
int module_stop(SceSize args, void *argp) {
    (void)args;(void)argp;
    g_running=0;uninstall();return 0;
}

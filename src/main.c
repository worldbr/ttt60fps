/*
 * Dragon Ball Z: Tenkaichi Tag Team - ULUS10537 - PPSSPP
 * TESTE 07: diagnosis ONLY, original game calls execute EVERY frame.
 * Does not patch the VBlank value. Does not skip or throttle updates.
 * Original ELF SHA-256: 834605685873da14647b1dfe7bf332e326dd2a99a48840877da13d211ea18b1e
 *
 * Five JAL callsites are temporarily redirected to wrappers which increment
 * a counter and then call the ORIGINAL target. Original MIPS delay slots remain.
 * No IO in any hook; disk access only occurs in the separate worker thread.
 */
#include <pspuser.h>
#include <pspiofilemgr.h>
#include <psputils.h>
#include <stdint.h>
#include <stddef.h>

PSP_MODULE_INFO("TTTFPSTest07", PSP_MODULE_USER, 1, 0);
PSP_NO_CREATE_MAIN_THREAD();

#define FPS_LOG "ms0:/PSP/PLUGINS/TTT_FPS_TEST07/diagnostico_teste07.txt"
#define VBLANK_STEP_ADDR 0x08A78E0Cu
#define SAMPLE_DELAY_US 5000000u
#define HOOKS 5u

/* Return values at these specific callsites are unused. For root walkers,
 * the original delay-slot loads a0, which the wrapper receives unchanged. */
typedef void (*VoidFn)(void);
typedef void (*RootFn)(void *);

static volatile uint32_t g_hits[HOOKS];
static volatile int g_running;
static volatile int g_installed;
static SceUID g_worker = -1;

static __attribute__((noinline)) void observe_update_group(void) {
    ++g_hits[0];
    ((VoidFn)(uintptr_t)0x0887B9ACu)();
}
static __attribute__((noinline)) void observe_task_update(void *root) {
    ++g_hits[1];
    ((RootFn)(uintptr_t)0x08901E10u)(root);
}
static __attribute__((noinline)) void observe_draw_group(void) {
    ++g_hits[2];
    ((VoidFn)(uintptr_t)0x0887BA50u)();
}
static __attribute__((noinline)) void observe_task_draw(void *root) {
    ++g_hits[3];
    ((RootFn)(uintptr_t)0x08902134u)(root);
}
static __attribute__((noinline)) void observe_frame_composer(void) {
    ++g_hits[4];
    ((VoidFn)(uintptr_t)0x08837B68u)();
}

typedef struct HookSite {
    uint32_t callsite;
    uint32_t expected_jal;
    uint32_t expected_delay;
    void (*wrapper)(void);
    const char *name;
} HookSite;

static const HookSite g_sites[HOOKS] = {
    {0x0885AC9Cu, 0x0E21EE6Bu, 0x00000000u, observe_update_group,   "update_group"},
    {0x0887B9D4u, 0x0E240784u, 0x8C840000u, (void (*)(void))observe_task_update, "task_update"},
    {0x0885ACACu, 0x0E21EE94u, 0x00000000u, observe_draw_group,     "draw_group"},
    {0x0887BB0Cu, 0x0E24084Du, 0x8C840000u, (void (*)(void))observe_task_draw,   "task_draw"},
    {0x08804728u, 0x0E20DEDAu, 0x00000000u, observe_frame_composer, "frame_composer"},
};

static uint32_t jal_word(uintptr_t target) {
    return 0x0C000000u | (((uint32_t)target >> 2) & 0x03FFFFFFu);
}
static void flush_code(void *site) {
    sceKernelDcacheWritebackInvalidateRange(site, 4);
    sceKernelIcacheInvalidateRange(site, 4);
}

/* Explicit bounded formatting: no stdio, snprintf, libc, heap allocation. */
static void add_text(char **w, char *last, const char *s) {
    if (!s) return;
    while (*s && *w < last) *(*w)++ = *s++;
}
static void add_u32(char **w, char *last, uint32_t n) {
    char reverse[10];
    unsigned k = 0;
    do {
        reverse[k++] = (char)('0' + n % 10u);
        n /= 10u;
    } while (n && k < 10u);
    while (k && *w < last) *(*w)++ = reverse[--k];
}
static void add_hex(char **w, char *last, uint32_t n) {
    static const char h[] = "0123456789ABCDEF";
    int k;
    for (k = 28; k >= 0 && *w < last; k -= 4) *(*w)++ = h[(n >> k) & 0xFu];
}
static void log_line(const char *s) {
    SceUID file;
    size_t n = 0;
    if (!s) return;
    while (s[n] && n < 1023u) ++n;
    file = sceIoOpen(FPS_LOG, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0777);
    if (file < 0) return;
    sceIoWrite(file, s, (SceSize)n);
    sceIoClose(file);
}
static void log_issue(const char *label, unsigned index, uint32_t expected, uint32_t actual) {
    char msg[240];
    char *w = msg;
    char *end = msg + sizeof(msg) - 1;
    add_text(&w, end, "[T07] ERRO "); add_text(&w, end, label);
    add_text(&w, end, " site="); add_text(&w, end, g_sites[index].name);
    add_text(&w, end, " endereco=0x"); add_hex(&w, end, g_sites[index].callsite);
    add_text(&w, end, " esperado=0x"); add_hex(&w, end, expected);
    add_text(&w, end, " lido=0x"); add_hex(&w, end, actual);
    add_text(&w, end, "\n"); *w = 0;
    log_line(msg);
}
static int validate_sites(void) {
    unsigned i;
    for (i=0; i<HOOKS; ++i) {
        const HookSite *h = &g_sites[i];
        volatile uint32_t *mem = (volatile uint32_t *)(uintptr_t)h->callsite;
        uintptr_t wrapper = (uintptr_t)h->wrapper;
        if ((wrapper & 3u) || (wrapper >> 28) != ((h->callsite + 4u) >> 28)) {
            log_issue("JAL fora do alcance", i, h->expected_jal, (uint32_t)wrapper);
            return -1;
        }
        if (mem[0] != h->expected_jal) {
            log_issue("opcode incompativel", i, h->expected_jal, mem[0]);
            return -2;
        }
        if (mem[1] != h->expected_delay) {
            log_issue("delay slot incompativel", i, h->expected_delay, mem[1]);
            return -3;
        }
    }
    return 0;
}
static int install_hooks(void) {
    unsigned i;
    if (validate_sites() < 0) return -1;
    for (i=0; i<HOOKS; ++i) {
        volatile uint32_t *mem = (volatile uint32_t *)(uintptr_t)g_sites[i].callsite;
        mem[0] = jal_word((uintptr_t)g_sites[i].wrapper);
        flush_code((void *)(uintptr_t)mem);
    }
    g_installed = 1;
    log_line("[T07] HOOKS INSTALADOS 5/5; chamadas originais preservadas.\n");
    return 0;
}
static void remove_hooks(void) {
    unsigned i;
    if (!g_installed) return;
    for (i=0; i<HOOKS; ++i) {
        const HookSite *h = &g_sites[i];
        volatile uint32_t *mem = (volatile uint32_t *)(uintptr_t)h->callsite;
        if (mem[0] == jal_word((uintptr_t)h->wrapper)) {
            mem[0] = h->expected_jal;
            flush_code((void *)(uintptr_t)mem);
        }
    }
    g_installed = 0;
}
static void sample_log(const char *label, uint32_t *prev) {
    char msg[550];
    char *w = msg;
    char *end = msg + sizeof(msg) - 1;
    uint32_t step = *(volatile uint32_t *)(uintptr_t)VBLANK_STEP_ADDR;
    unsigned i;
    add_text(&w,end,"[T07] "); add_text(&w,end,label);
    add_text(&w,end," step="); add_u32(&w,end,step);
    add_text(&w,end," instalado="); add_u32(&w,end,(uint32_t)g_installed);
    for (i=0; i<HOOKS; ++i) {
        uint32_t current = g_hits[i];
        add_text(&w,end," "); add_text(&w,end,g_sites[i].name);
        add_text(&w,end,"="); add_u32(&w,end,current);
        add_text(&w,end,"(+"); add_u32(&w,end,current-prev[i]);
        add_text(&w,end,"/5s)");
        prev[i]=current;
    }
    add_text(&w,end,"\n"); *w=0;
    log_line(msg);
}
static int worker_thread(SceSize args, void *argp) {
    uint32_t previous[HOOKS] = {0,0,0,0,0};
    unsigned tries;
    (void)args; (void)argp;
    log_line("[T07] Worker iniciado. Conferindo sites de hook...\n");
    /* Wait for EBOOT to finish loading; allow a short startup period. */
    for (tries=0; tries<100u && g_running; ++tries) {
        if (*(volatile uint32_t *)(uintptr_t)g_sites[0].callsite == g_sites[0].expected_jal) break;
        sceKernelDelayThread(100000u);
    }
    if (!g_running || tries == 100u || install_hooks() != 0) {
        log_line("[T07] HOOK NAO INSTALADO; nao foi modificada nenhuma instrucao.\n");
        sceKernelExitDeleteThread(0);
        return 0;
    }
    log_line("[T07] Pronto. Capture amostras no menu (30 FPS baseline) e na batalha 60 FPS.\n");
    while (g_running) {
        sceKernelDelayThread(SAMPLE_DELAY_US);
        if (g_running) sample_log("AMOSTRA", previous);
    }
    remove_hooks();
    log_line("[T07] Hook removido.\n");
    sceKernelExitDeleteThread(0);
    return 0;
}
int module_start(SceSize args, void *argp) {
    (void)args; (void)argp;
    g_running = 1;
    log_line("\n[T07] INICIO - DIAGNOSTICO SOMENTE; ZERO updates pulados.\n");
    g_worker = sceKernelCreateThread("TTTFPSWorker07", worker_thread, 0x20, 0x2400, 0, NULL);
    if (g_worker < 0) {
        g_running=0;
        log_line("[T07] ERRO ao criar worker.\n");
        return 0;
    }
    if (sceKernelStartThread(g_worker, 0, NULL) < 0) {
        g_running=0;
        log_line("[T07] ERRO ao iniciar worker.\n");
    }
    return 0;
}
int module_stop(SceSize args, void *argp) {
    (void)args; (void)argp;
    g_running = 0;
    remove_hooks();
    return 0;
}

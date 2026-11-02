/*
 * Tenkaichi Tag Team ULUS10537 - experimental 60 Hz draw / 30 Hz task update
 * Requires companion CWCheat forcing vblank step=1:
 *   _L 0x20278E0C 0x00000001
 *
 * WARNING: independent, experimental plugin. Not a full 60 FPS solution.
 * Verified against EBOOT.OLD.ELF SHA256
 * 834605685873da14647b1dfe7bf332e326dd2a99a48840877da13d211ea18b1e
 */
#include <pspuser.h>
#include <pspiofilemgr.h>
#include <psputils.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

PSP_MODULE_INFO("TTTFPSTest05", PSP_MODULE_USER, 1, 0);
PSP_NO_CREATE_MAIN_THREAD();

#define TTT_UPDATE_CALL_ADDR 0x0887B9D4u
#define TTT_UPDATE_CALL_WORD 0x0E240784u /* jal 0x08901E10 */
#define TTT_UPDATE_FN_ADDR   0x08901E10u
#define TTT_VBLANK_STEP_ADDR 0x08A78E0Cu
#define FPS_LOG "ms0:/PSP/PLUGINS/TTT_FPS_TEST/diagnostico_60fps.txt"

static volatile int g_running = 0;
static volatile int g_installed = 0;
static volatile uint32_t g_hits = 0;
static volatile uint32_t g_actual_updates = 0;
static volatile uint32_t g_skipped_updates = 0;
static SceUID g_worker = -1;

typedef void (*UpdateWalker)(void *root);

/* Executes in the game thread, invoked once per original update call.
 * The original call site delay-slot has already loaded a0 with the root
 * object pointer, so the C function receives exactly that pointer.
 * Never perform disk IO or other expensive work from this hook.
 */
static __attribute__((noinline)) void update_every_other_frame(void *root) {
    uint32_t hit = g_hits++;
    if ((hit & 1u) == 0u) {
        g_actual_updates++;
        ((UpdateWalker)(uintptr_t)TTT_UPDATE_FN_ADDR)(root);
    } else {
        g_skipped_updates++;
    }
}

static uint32_t encode_jal(uintptr_t address) {
    return UINT32_C(0x0C000000) | (((uint32_t)address >> 2) & UINT32_C(0x03FFFFFF));
}

static void log_line(const char *message) {
    SceUID fd;
    size_t len = 0;
    if (!message) return;
    while (message[len] && len < 1023u) ++len;
    fd = sceIoOpen(FPS_LOG, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0777);
    if (fd < 0) return;
    sceIoWrite(fd, message, (SceSize)len);
    sceIoClose(fd);
}

static void flush_code(volatile uint32_t *site) {
    void *p = (void *)(uintptr_t)site;
    sceKernelDcacheWritebackInvalidateRange(p, 4);
    sceKernelIcacheInvalidateRange(p, 4);
}

static void log_status(const char *label) {
    char msg[250];
    uint32_t word = *(volatile uint32_t *)(uintptr_t)TTT_UPDATE_CALL_ADDR;
    uint32_t step = *(volatile uint32_t *)(uintptr_t)TTT_VBLANK_STEP_ADDR;
    int n = snprintf(msg, sizeof(msg),
        "[TESTE05] %s callsite=%08X step=%u hits=%u actual=%u skipped=%u installed=%d\n",
        label, (unsigned)word, (unsigned)step,
        (unsigned)g_hits, (unsigned)g_actual_updates,
        (unsigned)g_skipped_updates, g_installed);
    if (n > 0) log_line(msg);
}

static int install_hook(void) {
    volatile uint32_t *site = (volatile uint32_t *)(uintptr_t)TTT_UPDATE_CALL_ADDR;
    uintptr_t hook = (uintptr_t)&update_every_other_frame;
    uint32_t value;
    /* MIPS JAL and this particular callsite must reside in the same 256 MB
     * address region.  Align and range checks prevent invalid jumps. */
    if ((hook & 3u) || ((hook >> 28) != ((TTT_UPDATE_CALL_ADDR + 4u) >> 28))) {
        log_line("[TESTE05] ERRO: funcao hook nao alcancavel por JAL.\n");
        return -1;
    }
    value = *site;
    if (value != TTT_UPDATE_CALL_WORD) {
        char msg[110];
        snprintf(msg, sizeof(msg), "[TESTE05] ERRO: opcode esperado=%08X lido=%08X. NADA ALTERADO.\n",
                 (unsigned)TTT_UPDATE_CALL_WORD, (unsigned)value);
        log_line(msg);
        return -2;
    }
    g_hits = 0;
    g_actual_updates = 0;
    g_skipped_updates = 0;
    *site = encode_jal(hook);
    flush_code(site);
    g_installed = 1;
    log_status("HOOK INSTALADO");
    return 0;
}

static void remove_hook(void) {
    volatile uint32_t *site = (volatile uint32_t *)(uintptr_t)TTT_UPDATE_CALL_ADDR;
    uint32_t patched = encode_jal((uintptr_t)&update_every_other_frame);
    if (g_installed && *site == patched) {
        *site = TTT_UPDATE_CALL_WORD;
        flush_code(site);
    }
    g_installed = 0;
}

static int worker_thread(SceSize args, void *argp) {
    int tries;
    (void)args; (void)argp;
    log_line("[TESTE05] Worker iniciado. Aguardando EBOOT ULUS10537...\n");
    for (tries = 0; g_running && tries < 100; ++tries) {
        uint32_t word = *(volatile uint32_t *)(uintptr_t)TTT_UPDATE_CALL_ADDR;
        if (word == TTT_UPDATE_CALL_WORD) break;
        sceKernelDelayThread(100000u);
    }
    if (g_running && tries < 100 && install_hook() == 0) {
        int iterations = 0;
        while (g_running) {
            sceKernelDelayThread(1000000u);
            ++iterations;
            if ((iterations % 5) == 0) log_status("AMOSTRA");
        }
        log_status("PARANDO");
        remove_hook();
    } else {
        log_status("HOOK NAO INSTALADO");
    }
    sceKernelExitDeleteThread(0);
    return 0;
}

int module_start(SceSize args, void *argp) {
    (void)args; (void)argp;
    g_running = 1;
    log_line("\n[TESTE05] Inicio. Controle de tarefas alternado 60/30 Hz.\n");
    g_worker = sceKernelCreateThread("TTTFPSWorker05", worker_thread, 0x20, 0x2000, 0, NULL);
    if (g_worker < 0) {
        log_line("[TESTE05] ERRO ao criar thread.\n");
        g_running = 0;
        return 0;
    }
    if (sceKernelStartThread(g_worker, 0, NULL) < 0) {
        log_line("[TESTE05] ERRO ao iniciar thread.\n");
        g_running = 0;
    }
    return 0;
}

int module_stop(SceSize args, void *argp) {
    (void)args; (void)argp;
    g_running = 0;
    remove_hook();
    return 0;
}

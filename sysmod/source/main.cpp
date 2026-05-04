/**
 * switch-engine-mod entry point.
 *
 * Sysmods rodam SEM applet/AM. Definimos manualmente:
 *   - __nx_applet_type     = AppletType_None
 *   - __nx_fs_num_sessions = 1
 *   - heap interno fixo (nao podemos depender de svcSetHeapSize agressivo)
 *
 * Fluxo:
 *   __libnx_initheap -> __appInit (sm/pm/fs + sdmc mount) -> main -> server loop
 *
 * Nao usamos applet* nem time* aqui: sysmodules nao rodam em contexto de applet
 * (ver templates/sysmodule do switch-examples); appletInitialize falha em
 * varias versoes recentes do HOS e derruba o processo antes do primeiro log.
 *
 * Logging:
 *   sdmc:/switch-engine_mod.log         eventos (init, IPCs, sessoes)
 *   sdmc:/switch-engine_mod_crash.log   dump de exception (override do
 *                                       __libnx_exception_handler)
 *
 * Exception handler:
 *   Definimos __libnx_exception_handler para capturar qualquer exception,
 *   logar PC/LR/registers em SD e abortar limpo via svcExitProcess. Sem
 *   isso, o handler default tenta svcReturnFromException (svc 0x28) e o
 *   processo morre num "Undefined System Call" 2168-0008 mascarando o
 *   crash original. (svcReturnFromException tambem foi adicionado ao NPDM
 *   para garantir compatibilidade caso o handler default seja chamado.)
 */

#include <switch.h>
#include <switch/runtime/hosversion.h>
#include <switch/services/ns.h>
#include <switch/services/set.h>

#include <cstring>

#include "IpcServer.hpp"
#include "Debugger.hpp"
#include "FreezeManager.hpp"
#include "SysmodLog.hpp"

extern "C" {
    u32 __nx_applet_type     = AppletType_None;
    u32 __nx_fs_num_sessions = 1;

    #define INNER_HEAP_SIZE 0x80000   // 512 KB para o sysmod, conservador
    size_t nx_inner_heap_size = INNER_HEAP_SIZE;
    char   nx_inner_heap[INNER_HEAP_SIZE];

    void __libnx_initheap(void);
    void __appInit(void);
    void __appExit(void);

    // Override do exception handler do libnx. O default chama
    // svcReturnFromException(0); se nao tiver permissao, vira "Undefined
    // System Call" mascarando a exception original. O nosso loga e mata
    // o processo limpo.
    void __libnx_exception_handler(ThreadExceptionDump *ctx);
}

void __libnx_initheap(void) {
    extern char *fake_heap_start;
    extern char *fake_heap_end;
    fake_heap_start = nx_inner_heap;
    fake_heap_end   = nx_inner_heap + nx_inner_heap_size;
}

void __appInit(void) {
    Result rc;

    rc = smInitialize();
    if (R_FAILED(rc)) diagAbortWithResult(rc);

    // Versao do HOS (necessaria para varios servicos pm/fs).
    if (hosversionGet() == 0) {
        rc = setsysInitialize();
        if (R_SUCCEEDED(rc)) {
            SetSysFirmwareVersion fw{};
            rc = setsysGetFirmwareVersion(&fw);
            if (R_SUCCEEDED(rc)) {
                hosversionSet(MAKEHOSVERSION(fw.major, fw.minor, fw.micro));
            }
            setsysExit();
        }
    }

    rc = pmdmntInitialize();
    if (R_FAILED(rc)) diagAbortWithResult(rc);

    rc = pminfoInitialize();
    if (R_FAILED(rc)) diagAbortWithResult(rc);

    rc = fsInitialize();
    if (R_FAILED(rc)) diagAbortWithResult(rc);

    // CRITICO: monta sdmc para podermos abrir nossos arquivos de log.
    // Tem que ser DEPOIS de fsInitialize. Se falhar, o sysmod ainda
    // funciona (so' nao loga em arquivo).
    fsdevMountSdmc();

    // ns:am2 para ler NACP (nomes reais dos jogos). Nao e critico.
    rc = nsInitialize();
    if (R_FAILED(rc)) {
        // Se falhar, continuamos sem nomes reais (fallback para TID).
    }

    seng::mod::log::init();
    seng::mod::log::write("INFO [init] sm/setsys+hos/pm/fs/sdmc/ns OK (ns rc=0x%08X)", rc);
}

void __appExit(void) {
    seng::mod::log::writeRaw("[exit] __appExit");
    seng::mod::log::close();
    fsdevUnmountAll();
    fsExit();
    nsExit();
    Debugger::releaseAuxServicesForExit();
    pminfoExit();
    pmdmntExit();
    smExit();
}

// =========================================================================
// Exception handler: loga snapshot do contexto da CPU e termina o processo.
// Chamado pelo runtime do libnx via __libnx_exception_entry quando ocorre
// uma exception (data abort, undefined instruction, etc.).
// =========================================================================
extern "C" void __libnx_exception_handler(ThreadExceptionDump *ctx) {
    if (!ctx) {
        seng::mod::log::crashLog("[crash] context=NULL");
        svcExitProcess();
    }

    // Tipo da exception (campo error_desc).
    seng::mod::log::crashLog(
        "===== EXCEPTION ===== type=0x%X PC=0x%016llX LR=0x%016llX",
        static_cast<unsigned>(ctx->error_desc),
        static_cast<unsigned long long>(ctx->pc.x),
        static_cast<unsigned long long>(ctx->lr.x));

    seng::mod::log::crashLog(
        "[crash] FP=0x%016llX SP=0x%016llX FAR=0x%016llX",
        static_cast<unsigned long long>(ctx->fp.x),
        static_cast<unsigned long long>(ctx->sp.x),
        static_cast<unsigned long long>(ctx->far.x));

    // Despeja registradores X0..X28 em pares para nao explodir no log.
    for (int i = 0; i < 29; i += 4) {
        seng::mod::log::crashLog(
            "[crash] X%02d=0x%016llX X%02d=0x%016llX X%02d=0x%016llX X%02d=0x%016llX",
            i + 0, static_cast<unsigned long long>(ctx->cpu_gprs[i + 0].x),
            i + 1, static_cast<unsigned long long>(ctx->cpu_gprs[i + 1].x),
            i + 2, static_cast<unsigned long long>(ctx->cpu_gprs[i + 2].x),
            i + 3, static_cast<unsigned long long>(ctx->cpu_gprs[i + 3].x));
    }

    seng::mod::log::crashLog("[crash] aborting via svcExitProcess");
    seng::mod::log::close();

    svcExitProcess();
}

// =========================================================================
int main(int /*argc*/, char ** /*argv*/) {
    seng::mod::log::write("INFO [main] entry");

    Debugger::init();
    seng::mod::log::write("INFO [main] Debugger::init OK");

    FreezeManager::init(0);
    seng::mod::log::write("INFO [main] FreezeManager::init OK");

    IpcServer server;
    Result rc = server.registerService();
    if (R_FAILED(rc)) {
        seng::mod::log::crashLog("[main] registerService FAILED rc=0x%08X", rc);
        // Nao retorna -- sleep loop pra nao consumir CPU.
        while (true) svcSleepThread(1'000'000'000ULL);
    }
    seng::mod::log::write("INFO [main] registerService OK; entering runForever");

    server.runForever();

    seng::mod::log::write("WARN [main] runForever returned (unexpected)");
    FreezeManager::shutdown();
    Debugger::shutdown();
    return 0;
}

/**
 * switch-engine-mod entry point.
 *
 * Sysmods rodam SEM applet/AM. Definimos manualmente:
 *   - __nx_applet_type     = AppletType_None
 *   - __nx_fs_num_sessions = 1
 *   - heap interno fixo (nao podemos depender de svcSetHeapSize agressivo)
 *
 * Fluxo:
 *   __libnx_initheap -> __appInit (sm/pm/fs) -> main -> server loop infinito.
 */

#include <switch.h>
#include <cstring>

#include "IpcServer.hpp"
#include "Debugger.hpp"

extern "C" {
    u32 __nx_applet_type     = AppletType_None;
    u32 __nx_fs_num_sessions = 1;

    #define INNER_HEAP_SIZE 0x80000   // 512 KB para o sysmod, conservador
    size_t nx_inner_heap_size = INNER_HEAP_SIZE;
    char   nx_inner_heap[INNER_HEAP_SIZE];

    void __libnx_initheap(void);
    void __appInit(void);
    void __appExit(void);

    // Stubs: sysmods nao precisam de hora nem socket nem userland appservices
    void __libnx_init_time(void) {}
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

    rc = pmdmntInitialize();
    if (R_FAILED(rc)) diagAbortWithResult(rc);

    rc = pminfoInitialize();
    if (R_FAILED(rc)) diagAbortWithResult(rc);

    // fs ainda eh util para logs, mas nao montamos sdmc para nao competir
    // com o overlay pelo file descriptor.
    rc = fsInitialize();
    if (R_FAILED(rc)) diagAbortWithResult(rc);
}

void __appExit(void) {
    fsExit();
    pminfoExit();
    pmdmntExit();
    smExit();
}

int main(int /*argc*/, char ** /*argv*/) {
    Debugger::init();

    IpcServer server;
    if (R_FAILED(server.registerService())) {
        // Se falhar registro, dorme para nao loopar consumindo CPU.
        while (true) svcSleepThread(1'000'000'000ULL);
    }

    server.runForever();

    Debugger::shutdown();
    return 0;
}

#pragma once

#include <switch.h>
#include <cstdint>

#include "seng_ipc.hpp"

/**
 * Debugger
 *
 * Estado global do "debug attach". Apenas UM processo attached por vez --
 * suficiente para um Memory Scanner. Reentrancia controlada por mutex porque
 * o IpcServer pode atender requests em paralelo (quando suportarmos).
 */
namespace Debugger {

    void   init();
    void   shutdown();

    Result attach(uint64_t pid);
    void   detach();
    bool   isAttached();

    Result getForegroundPid(uint64_t *out_pid);
    Result getTitleId(uint64_t pid, uint64_t *out_tid);

    Result queryMemory(uint64_t addr, seng::MemoryRegion *out);
    Result readMemory(uint64_t addr, void *dst, size_t size);
    Result writeMemory(uint64_t addr, const void *src, size_t size);

    // Lista ate 'max' processos vivos no sistema. Para cada PID, tenta
    // resolver o TitleID via pminfo; se nao tiver (kernel/sysmod), tid=0.
    // *out_count recebe o numero de entradas preenchidas.
    Result listProcesses(seng::ProcessEntry *out,
                         size_t              max,
                         size_t             *out_count);

} // namespace Debugger

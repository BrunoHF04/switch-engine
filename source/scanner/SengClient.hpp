#pragma once

#include <switch.h>
#include <cstdint>

#include "seng_ipc.hpp"

/**
 * SengClient
 *
 * Cliente CMIF que conversa com o sysmod switch-engine-mod via servico "seng".
 * Singleton: o overlay so precisa de uma sessao.
 *
 * Uso:
 *   if (R_SUCCEEDED(SengClient::initialize())) {
 *       uint64_t pid;
 *       SengClient::getForegroundPid(&pid);
 *       SengClient::attach(pid);
 *       ...
 *       SengClient::finalize();
 *   }
 */
namespace SengClient {

    Result initialize();
    /** Tenta `smGetService("seng")` ate `maxWaitNs` (evita bloqueio indefinido). */
    Result initializeTimed(u64 maxWaitNs);
    void   finalize();
    bool   isInitialized();

    Result getVersion(uint32_t *out_version);
    Result getForegroundPid(uint64_t *out_pid);
    Result getTitleId(uint64_t pid, uint64_t *out_tid);
    Result attach(uint64_t pid);
    Result detach();
    Result isAttached(bool *out_attached);

    Result queryMemory(uint64_t addr, seng::MemoryRegion *out);
    Result readMemory(uint64_t addr, void *dst, size_t size, size_t *out_read);
    Result writeMemory(uint64_t addr, const void *src, size_t size, size_t *out_written);

    // Lista todos os processos vivos no sistema.
    //   out:       array com pelo menos 'max' entradas, alocado pelo cliente.
    //   max:       capacidade do buffer (entries, nao bytes).
    //   out_count: numero de entries efetivamente preenchidas (pode ser 0).
    Result listProcesses(seng::ProcessEntry *out,
                         size_t              max,
                         size_t             *out_count);

    /** Cmd 11: primeiro plano de scan no sysmod (attach + svc + results.bin). */
    Result startMemoryScan(uint64_t pid, uint32_t value, uint64_t *out_total_hits);

} // namespace SengClient

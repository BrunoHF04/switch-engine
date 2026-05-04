#pragma once

#include <switch.h>
#include <cstdint>

#include "seng_ipc.hpp"

/**
 * SengClient
 *
 * Cliente CMIF que conversa com o sysmod switch-engine-mod via servico "seng".
 * Singleton: o overlay so precisa de uma sessao.
 */
namespace SengClient {

    Result initialize();
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

    Result listProcesses(seng::ProcessEntry *out,
                         size_t              max,
                         size_t             *out_count);

    Result startMemoryScan(const seng::ScanParams &params, uint64_t *out_total_hits);

    // Freeze
    Result addFreeze(uint64_t addr, seng::ValueType type, uint64_t value,
                     uint8_t *out_slot);
    Result removeFreeze(uint8_t slot);
    Result listFreezes(seng::FreezeEntry *out, size_t max, size_t *out_count);
    Result clearFreezes();

} // namespace SengClient

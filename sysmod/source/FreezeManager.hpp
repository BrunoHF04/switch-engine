#pragma once

#include <switch.h>

#include <cstdint>

#include "seng_ipc.hpp"

/**
 * FreezeManager
 *
 * Mantem ate kMaxFreezeSlots enderecos "congelados": uma thread dedicada
 * faz attach -> write -> detach periodicamente (a cada ~16ms) para manter
 * os valores fixos no jogo.
 *
 * Usa seu proprio debug handle, independente do Debugger global, para nao
 * conflitar com scans.
 */
namespace FreezeManager {

    void init(uint64_t pid);
    void shutdown();

    void setTargetPid(uint64_t pid);

    Result addFreeze(uint64_t addr, seng::ValueType type, uint64_t value,
                     uint8_t *out_slot);
    void   removeFreeze(uint8_t slot);
    size_t listFreezes(seng::FreezeEntry *out, size_t max);
    void   clearAll();

} // namespace FreezeManager

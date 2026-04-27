#pragma once

/**
 * Switch Engine IPC contract.
 *
 * Compartilhado entre o overlay (cliente) e o sysmod (servidor).
 * Comandos sao IDs CMIF estaveis. NUNCA reordenar -- so adicionar no final.
 */

#include <cstdint>

namespace seng {

    constexpr const char *kServiceName = "seng";

    constexpr uint64_t kSysmodTitleId = 0x420000000053454EULL;

    constexpr uint32_t kIpcVersion = 1;

    enum class Cmd : uint32_t {
        GetVersion         = 0,   // ()              -> u32
        GetForegroundPid   = 1,   // ()              -> u64 pid
        GetTitleId         = 2,   // (u64 pid)       -> u64 tid
        AttachProcess      = 3,   // (u64 pid)       -> ()
        DetachProcess      = 4,   // ()              -> ()
        QueryMemory        = 5,   // (u64 addr)      -> MemoryRegion
        ReadMemory         = 6,   // (u64 addr,u64 size) + outBuf  -> ()
        WriteMemory        = 7,   // (u64 addr,u64 size) + inBuf   -> ()
        IsAttached         = 8,   // ()              -> u8
        ListProcesses      = 9,   // (u64 max) + outBuf<ProcessEntry[]> -> u64 count
    };

    // Layout fixo: enviado por wire. NAO mexer alinhamento sem bumpar versao.
    struct MemoryRegion {
        uint64_t addr;
        uint64_t size;
        uint32_t type;
        uint32_t attr;
        uint32_t perm;
        uint32_t page_info;
    };
    static_assert(sizeof(MemoryRegion) == 32, "MemoryRegion ABI mudou");

    // Layout para Cmd::ListProcesses. Enviada como elemento de array em
    // buffer Type-B. tid==0 indica processo sem TID conhecido (kernel/sysmod).
    struct ProcessEntry {
        uint64_t pid;
        uint64_t tid;
    };
    static_assert(sizeof(ProcessEntry) == 16, "ProcessEntry ABI mudou");

    constexpr size_t kMaxChunkBytes  = 0x10000; // 64 KB por Read/Write IPC
    constexpr size_t kMaxProcessList = 64;       // hard cap p/ ListProcesses

} // namespace seng

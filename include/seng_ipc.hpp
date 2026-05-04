#pragma once

/**
 * Switch Engine IPC contract  (v3).
 *
 * Compartilhado entre o overlay (cliente) e o sysmod (servidor).
 * Comandos sao IDs CMIF estaveis. NUNCA reordenar -- so adicionar no final.
 */

#include <cstdint>

namespace seng {

    constexpr const char *kServiceName = "seng";

    constexpr uint64_t kSysmodTitleId = 0x420000000053454EULL;

    constexpr uint32_t kIpcVersion = 3;

    // -----------------------------------------------------------------
    // Tipos de valor suportados pelo scanner.
    // -----------------------------------------------------------------
    enum class ValueType : uint8_t {
        U8  = 0,
        U16 = 1,
        U32 = 2,
        U64 = 3,
        F32 = 4,
        F64 = 5,
    };

    inline size_t valueTypeSize(ValueType t) {
        switch (t) {
            case ValueType::U8:  return 1;
            case ValueType::U16: return 2;
            case ValueType::U32: return 4;
            case ValueType::U64: return 8;
            case ValueType::F32: return 4;
            case ValueType::F64: return 8;
        }
        return 4;
    }

    inline const char *valueTypeName(ValueType t) {
        switch (t) {
            case ValueType::U8:  return "u8";
            case ValueType::U16: return "u16";
            case ValueType::U32: return "u32";
            case ValueType::U64: return "u64";
            case ValueType::F32: return "f32";
            case ValueType::F64: return "f64";
        }
        return "u32";
    }

    // -----------------------------------------------------------------
    // Operadores de comparacao suportados no scan.
    // -----------------------------------------------------------------
    enum class CompareOp : uint8_t {
        Equal          = 0,
        NotEqual       = 1,
        GreaterThan    = 2,
        LessThan       = 3,
        GreaterOrEqual = 4,
        LessOrEqual    = 5,
        Between        = 6,
        Changed        = 7,
        Unchanged      = 8,
        Unknown        = 9,
    };

    inline const char *compareOpSymbol(CompareOp op) {
        switch (op) {
            case CompareOp::Equal:          return "==";
            case CompareOp::NotEqual:       return "!=";
            case CompareOp::GreaterThan:    return ">";
            case CompareOp::LessThan:       return "<";
            case CompareOp::GreaterOrEqual: return ">=";
            case CompareOp::LessOrEqual:    return "<=";
            case CompareOp::Between:        return "[]";
            case CompareOp::Changed:        return "chg";
            case CompareOp::Unchanged:      return "unch";
            case CompareOp::Unknown:        return "?";
        }
        return "==";
    }

    // -----------------------------------------------------------------
    // Comandos IPC do servico "seng".
    // -----------------------------------------------------------------
    enum class Cmd : uint32_t {
        GetVersion          = 0,   // ()              -> u32
        GetForegroundPid    = 1,   // ()              -> u64 pid
        GetTitleId          = 2,   // (u64 pid)       -> u64 tid
        AttachProcess       = 3,   // (u64 pid)       -> ()
        DetachProcess       = 4,   // ()              -> ()
        QueryMemory         = 5,   // (u64 addr)      -> MemoryRegion
        ReadMemory          = 6,   // (u64 addr,u64 size) + outBuf  -> u64 read
        WriteMemory         = 7,   // (u64 addr,u64 size) + inBuf   -> u64 written
        IsAttached          = 8,   // ()              -> u8
        ListProcessesLegacy = 9,   // legado
        ListProcesses       = 10,  // (u64 max) + outBuf<ProcessEntry[]> -> u64 count
        StartMemoryScan     = 11,  // (ScanParams) -> u64 total_hits; escreve results.bin
        AddFreeze           = 12,  // (FreezeParams) -> u8 slot
        RemoveFreeze        = 13,  // (u8 slot) -> ()
        ListFreezes         = 14,  // () + outBuf<FreezeEntry[]> -> u8 count
        ClearFreezes        = 15,  // () -> ()
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

    struct ProcessEntry {
        uint64_t pid;
        uint64_t tid;
        char     name[32];
    };
    static_assert(sizeof(ProcessEntry) == 48, "ProcessEntry ABI mudou");

    // Parametros para Cmd::StartMemoryScan (raw data no CMIF).
    struct ScanParams {
        uint64_t  pid;
        uint64_t  value;
        uint64_t  value2;     // usado para CompareOp::Between
        ValueType type;
        CompareOp op;
        uint8_t   _pad[6];
    };
    static_assert(sizeof(ScanParams) == 32, "ScanParams ABI mudou");

    // Parametros para Cmd::AddFreeze.
    struct FreezeParams {
        uint64_t  addr;
        uint64_t  value;
        ValueType type;
        uint8_t   _pad[7];
    };
    static_assert(sizeof(FreezeParams) == 24, "FreezeParams ABI mudou");

    // Slot de freeze retornado por ListFreezes.
    struct FreezeEntry {
        uint64_t  addr;
        uint64_t  value;
        ValueType type;
        uint8_t   active;
        uint8_t   _pad[6];
    };
    static_assert(sizeof(FreezeEntry) == 24, "FreezeEntry ABI mudou");

    constexpr size_t kMaxChunkBytes  = 0x10000; // 64 KB por Read/Write IPC
    constexpr size_t kMaxProcessList = 64;       // hard cap p/ ListProcesses
    constexpr size_t kMaxFreezeSlots = 16;

} // namespace seng

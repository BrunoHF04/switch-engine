#include "FreezeManager.hpp"
#include "SysmodLog.hpp"

#include <atomic>
#include <cstring>
#include <mutex>

namespace FreezeManager {

    namespace {
        constexpr u64 kFreezeIntervalNs = 16'000'000ULL; // ~60 fps

        std::mutex          g_mtx;
        uint64_t            g_pid = 0;
        seng::FreezeEntry   g_slots[seng::kMaxFreezeSlots]{};

        std::atomic<bool>   g_running{false};
        Thread              g_thread{};

        alignas(0x1000) static u8 g_threadStack[0x4000]; // 16 KB stack

        void freezeThreadFunc(void * /*arg*/) {
            while (g_running.load(std::memory_order_relaxed)) {
                svcSleepThread(kFreezeIntervalNs);

                std::lock_guard lock(g_mtx);
                if (g_pid == 0) continue;

                bool anyActive = false;
                for (size_t i = 0; i < seng::kMaxFreezeSlots; ++i) {
                    if (g_slots[i].active) { anyActive = true; break; }
                }
                if (!anyActive) continue;

                Handle dbg = INVALID_HANDLE;
                Result rc = svcDebugActiveProcess(&dbg, g_pid);
                if (R_FAILED(rc)) continue;

                for (size_t i = 0; i < seng::kMaxFreezeSlots; ++i) {
                    if (!g_slots[i].active) continue;
                    size_t sz = seng::valueTypeSize(g_slots[i].type);
                    svcWriteDebugProcessMemory(dbg,
                                               const_cast<void *>(
                                                   static_cast<const void *>(&g_slots[i].value)),
                                               g_slots[i].addr, sz);
                }

                svcCloseHandle(dbg);
            }
        }
    } // namespace

    void init(uint64_t pid) {
        g_pid = pid;
        if (g_running.load()) return;

        g_running.store(true);
        Result rc = threadCreate(&g_thread, freezeThreadFunc, nullptr,
                                 g_threadStack, sizeof(g_threadStack), 0x3F, -2);
        if (R_SUCCEEDED(rc)) {
            threadStart(&g_thread);
            seng::mod::log::write("INFO [freeze] thread started pid=%llu",
                                  static_cast<unsigned long long>(pid));
        } else {
            g_running.store(false);
            seng::mod::log::write("ERR  [freeze] threadCreate failed rc=0x%08X", rc);
        }
    }

    void shutdown() {
        g_running.store(false);
        threadWaitForExit(&g_thread);
        threadClose(&g_thread);
        clearAll();
    }

    void setTargetPid(uint64_t pid) {
        std::lock_guard lock(g_mtx);
        if (g_pid != pid) {
            clearAll();
            g_pid = pid;
        }
    }

    Result addFreeze(uint64_t addr, seng::ValueType type, uint64_t value,
                     uint8_t *out_slot) {
        std::lock_guard lock(g_mtx);
        for (size_t i = 0; i < seng::kMaxFreezeSlots; ++i) {
            if (!g_slots[i].active) {
                g_slots[i].addr   = addr;
                g_slots[i].value  = value;
                g_slots[i].type   = type;
                g_slots[i].active = 1;
                std::memset(g_slots[i]._pad, 0, sizeof(g_slots[i]._pad));
                if (out_slot) *out_slot = static_cast<uint8_t>(i);
                seng::mod::log::write("INFO [freeze] add slot=%zu addr=0x%010llX",
                                      i, static_cast<unsigned long long>(addr));
                return 0;
            }
        }
        return MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);
    }

    void removeFreeze(uint8_t slot) {
        std::lock_guard lock(g_mtx);
        if (slot < seng::kMaxFreezeSlots) {
            g_slots[slot].active = 0;
            seng::mod::log::write("INFO [freeze] remove slot=%u", slot);
        }
    }

    size_t listFreezes(seng::FreezeEntry *out, size_t max) {
        std::lock_guard lock(g_mtx);
        size_t n = 0;
        for (size_t i = 0; i < seng::kMaxFreezeSlots && n < max; ++i) {
            if (g_slots[i].active) {
                out[n++] = g_slots[i];
            }
        }
        return n;
    }

    void clearAll() {
        std::lock_guard lock(g_mtx);
        std::memset(g_slots, 0, sizeof(g_slots));
    }

} // namespace FreezeManager

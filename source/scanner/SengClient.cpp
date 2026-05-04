#include "SengClient.hpp"

#include <atomic>
#include <cstring>

namespace SengClient {

    namespace {
        Service g_srv{};
        std::atomic<bool> g_initialized{false};

        Result connect() {
            return smGetService(&g_srv, seng::kServiceName);
        }

        template <typename Out>
        Result dispatchOut(seng::Cmd cmd, Out *out) {
            return serviceDispatchOut(&g_srv, static_cast<u32>(cmd), *out);
        }

        Result dispatchVoid(seng::Cmd cmd) {
            return serviceDispatch(&g_srv, static_cast<u32>(cmd));
        }

        template <typename In>
        Result dispatchIn(seng::Cmd cmd, const In &in) {
            return serviceDispatchIn(&g_srv, static_cast<u32>(cmd), in);
        }

        template <typename In, typename Out>
        Result dispatchInOut(seng::Cmd cmd, const In &in, Out *out) {
            return serviceDispatchInOut(&g_srv, static_cast<u32>(cmd), in, *out);
        }
    } // namespace

    Result initialize() {
        if (g_initialized.load()) return 0;
        Result rc = connect();
        if (R_FAILED(rc)) return rc;
        g_initialized.store(true);
        return 0;
    }

    Result initializeTimed(u64 maxWaitNs) {
        if (g_initialized.load()) return 0;

        constexpr u64 kSliceNs = 50'000'000ULL;
        u64     waited = 0;
        Result  lastRc = 0;

        while (waited < maxWaitNs) {
            std::memset(&g_srv, 0, sizeof(g_srv));
            lastRc = connect();
            if (R_SUCCEEDED(lastRc)) {
                g_initialized.store(true);
                return 0;
            }
            svcSleepThread(kSliceNs);
            waited += kSliceNs;
        }
        std::memset(&g_srv, 0, sizeof(g_srv));
        return lastRc;
    }

    void finalize() {
        if (!g_initialized.exchange(false)) return;
        serviceClose(&g_srv);
    }

    bool isInitialized() { return g_initialized.load(); }

    Result getVersion(uint32_t *out_version) {
        return dispatchOut(seng::Cmd::GetVersion, out_version);
    }

    Result getForegroundPid(uint64_t *out_pid) {
        return dispatchOut(seng::Cmd::GetForegroundPid, out_pid);
    }

    Result getTitleId(uint64_t pid, uint64_t *out_tid) {
        return dispatchInOut(seng::Cmd::GetTitleId, pid, out_tid);
    }

    Result attach(uint64_t pid) {
        return dispatchIn(seng::Cmd::AttachProcess, pid);
    }

    Result detach() {
        return dispatchVoid(seng::Cmd::DetachProcess);
    }

    Result isAttached(bool *out_attached) {
        u8 a = 0;
        Result rc = dispatchOut(seng::Cmd::IsAttached, &a);
        if (out_attached) *out_attached = (a != 0);
        return rc;
    }

    Result queryMemory(uint64_t addr, seng::MemoryRegion *out) {
        return dispatchInOut(seng::Cmd::QueryMemory, addr, out);
    }

    Result readMemory(uint64_t addr, void *dst, size_t size, size_t *out_read) {
        if (size > seng::kMaxChunkBytes) size = seng::kMaxChunkBytes;

        struct InArgs  { u64 addr; u64 size; };
        struct OutArgs { u64 read; };

        InArgs  in{ addr, size };
        OutArgs out{ 0 };

        Result rc = serviceDispatchInOut(
            &g_srv, static_cast<u32>(seng::Cmd::ReadMemory), in, out,
            .buffer_attrs = {
                SfBufferAttr_HipcMapAlias | SfBufferAttr_Out,
            },
            .buffers = {
                { dst, size },
            }
        );
        if (R_SUCCEEDED(rc) && out_read) *out_read = out.read;
        return rc;
    }

    Result writeMemory(uint64_t addr, const void *src, size_t size, size_t *out_written) {
        if (size > seng::kMaxChunkBytes) size = seng::kMaxChunkBytes;

        struct InArgs  { u64 addr; u64 size; };
        struct OutArgs { u64 written; };

        InArgs  in{ addr, size };
        OutArgs out{ 0 };

        Result rc = serviceDispatchInOut(
            &g_srv, static_cast<u32>(seng::Cmd::WriteMemory), in, out,
            .buffer_attrs = {
                SfBufferAttr_HipcMapAlias | SfBufferAttr_In,
            },
            .buffers = {
                { const_cast<void *>(src), size },
            }
        );
        if (R_SUCCEEDED(rc) && out_written) *out_written = out.written;
        return rc;
    }

    Result listProcesses(seng::ProcessEntry *out,
                         size_t              max,
                         size_t             *out_count) {
        if (!out || !out_count) {
            return MAKERESULT(Module_Libnx, LibnxError_BadInput);
        }
        if (max > seng::kMaxProcessList) max = seng::kMaxProcessList;

        struct InArgs  { u64 max; };
        struct OutArgs { u64 count; };

        InArgs  in{ max };
        OutArgs o{ 0 };

        Result rc = serviceDispatchInOut(
            &g_srv, static_cast<u32>(seng::Cmd::ListProcesses), in, o,
            .buffer_attrs = {
                SfBufferAttr_HipcMapAlias | SfBufferAttr_Out,
            },
            .buffers = {
                { out, max * sizeof(seng::ProcessEntry) },
            }
        );
        if (R_SUCCEEDED(rc)) {
            *out_count = static_cast<size_t>(o.count);
            if (*out_count > max) *out_count = max;
        } else {
            *out_count = 0;
        }
        return rc;
    }

    Result startMemoryScan(const seng::ScanParams &params, uint64_t *out_total_hits) {
        struct OutArgs { u64 total_hits; };
        OutArgs out{ 0 };

        Result rc = serviceDispatchInOut(
            &g_srv, static_cast<u32>(seng::Cmd::StartMemoryScan), params, out);
        if (R_SUCCEEDED(rc) && out_total_hits) {
            *out_total_hits = out.total_hits;
        } else if (out_total_hits) {
            *out_total_hits = 0;
        }
        return rc;
    }

    Result addFreeze(uint64_t addr, seng::ValueType type, uint64_t value,
                     uint8_t *out_slot) {
        seng::FreezeParams fp{};
        fp.addr  = addr;
        fp.value = value;
        fp.type  = type;

        u8 slot = 0xFF;
        Result rc = dispatchInOut(seng::Cmd::AddFreeze, fp, &slot);
        if (out_slot) *out_slot = slot;
        return rc;
    }

    Result removeFreeze(uint8_t slot) {
        return dispatchIn(seng::Cmd::RemoveFreeze, slot);
    }

    Result listFreezes(seng::FreezeEntry *out, size_t max, size_t *out_count) {
        if (!out || !out_count) {
            return MAKERESULT(Module_Libnx, LibnxError_BadInput);
        }
        if (max > seng::kMaxFreezeSlots) max = seng::kMaxFreezeSlots;

        u8 count_out = 0;

        Result rc = serviceDispatchOut(
            &g_srv, static_cast<u32>(seng::Cmd::ListFreezes), count_out,
            .buffer_attrs = {
                SfBufferAttr_HipcMapAlias | SfBufferAttr_Out,
            },
            .buffers = {
                { out, max * sizeof(seng::FreezeEntry) },
            }
        );
        if (R_SUCCEEDED(rc)) {
            *out_count = count_out;
        } else {
            *out_count = 0;
        }
        return rc;
    }

    Result clearFreezes() {
        return dispatchVoid(seng::Cmd::ClearFreezes);
    }

} // namespace SengClient

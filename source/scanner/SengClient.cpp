#include "SengClient.hpp"

#include <atomic>
#include <cstring>

namespace SengClient {

    namespace {
        Service g_srv{};
        std::atomic<bool> g_initialized{false};

        Result connect() {
            SmServiceName name = smEncodeName(seng::kServiceName);
            return smGetService(&g_srv, seng::kServiceName);
        }

        // Helpers tipados para reduzir boilerplate.
        // <In> sao os argumentos enviados (alem do command id).
        // <Out> e' o que retorna alem do Result.

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

    void finalize() {
        if (!g_initialized.exchange(false)) return;
        serviceClose(&g_srv);
    }

    bool isInitialized() { return g_initialized.load(); }

    // ---------------------------------------------------------------------
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

    // ---------------------------------------------------------------------
    // ReadMemory: usa Type-B (server escreve, cliente le).
    // ---------------------------------------------------------------------
    Result readMemory(uint64_t addr, void *dst, size_t size, size_t *out_read) {
        if (size > seng::kMaxChunkBytes) size = seng::kMaxChunkBytes;

        struct InArgs  { u64 addr; u64 size; } __attribute__((packed));
        struct OutArgs { u64 read; }            __attribute__((packed));

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

    // ---------------------------------------------------------------------
    // ListProcesses: usa Type-B (server escreve buffer com array de
    // ProcessEntry; reply payload tem o count efetivo).
    // ---------------------------------------------------------------------
    Result listProcesses(seng::ProcessEntry *out,
                         size_t              max,
                         size_t             *out_count) {
        if (!out || !out_count) {
            return MAKERESULT(Module_Libnx, LibnxError_BadInput);
        }
        if (max > seng::kMaxProcessList) max = seng::kMaxProcessList;

        struct InArgs  { u64 max; }   __attribute__((packed));
        struct OutArgs { u64 count; } __attribute__((packed));

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

    // ---------------------------------------------------------------------
    // WriteMemory: usa Type-A (cliente envia, server le).
    // ---------------------------------------------------------------------
    Result writeMemory(uint64_t addr, const void *src, size_t size, size_t *out_written) {
        if (size > seng::kMaxChunkBytes) size = seng::kMaxChunkBytes;

        struct InArgs  { u64 addr; u64 size; } __attribute__((packed));
        struct OutArgs { u64 written; }         __attribute__((packed));

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

} // namespace SengClient

#include "IpcServer.hpp"
#include "Debugger.hpp"
#include "seng_ipc.hpp"

#include <cstring>

// =========================================================================
// CMIF protocol constants (em libnx: <switch/sf/cmif.h>)
// =========================================================================
namespace {
    constexpr u32 kSfciMagic = 0x49434653u; // 'SFCI' = request from client
    constexpr u32 kSfcoMagic = 0x4F434653u; // 'SFCO' = response from server

    struct CmifInHdr  { u32 magic; u32 version; u32 cmd_id; u32 token; };
    struct CmifOutHdr { u32 magic; u32 version; Result result; u32 token; };

    // CMIF header fica em offset 16-byte-alinhado dentro da data section.
    template <typename T>
    inline T *alignTo16(void *p) {
        uintptr_t a = (reinterpret_cast<uintptr_t>(p) + 15u) & ~uintptr_t(15);
        return reinterpret_cast<T *>(a);
    }

    // Constroi um HIPC reply com CmifOutHdr + payload no TLS.
    Result writeResponse(Result rc,
                         const void *payload,
                         size_t      payload_size) {
        // Calcula data_words de forma a caber CmifOutHdr alinhado + payload + slack.
        size_t bytes = sizeof(CmifOutHdr) + payload_size + 16; // +16 slack p/ alinhamento
        size_t data_words = (bytes + 3) / 4;

        HipcMetadata meta = {};
        meta.num_data_words = data_words;

        HipcRequest req = hipcMakeRequestInline(armGetTls(), meta);

        CmifOutHdr *out = alignTo16<CmifOutHdr>(req.data_words);
        out->magic   = kSfcoMagic;
        out->version = 0;
        out->result  = rc;
        out->token   = 0;
        if (payload && payload_size > 0) {
            std::memcpy(out + 1, payload, payload_size);
        }
        return 0;
    }

    // Pega ponteiro+size de buffer CMIF (Type-A send ou Type-B recv).
    void *bufferAddress(const HipcBufferDescriptor *d, u64 *out_size) {
        if (out_size) *out_size = hipcGetBufferSize(d);
        return hipcGetBufferAddress(d);
    }
}

// =========================================================================
IpcServer::IpcServer()  = default;

IpcServer::~IpcServer() {
    if (m_port_handle != INVALID_HANDLE) svcCloseHandle(m_port_handle);
    for (int i = 0; i < m_num_sessions; ++i) svcCloseHandle(m_handles[i]);
}

// =========================================================================
Result IpcServer::registerService() {
    SmServiceName name = smEncodeName(seng::kServiceName);
    return smRegisterService(&m_port_handle, name, false, kPortMaxSessions);
}

// =========================================================================
Result IpcServer::acceptNewSession() {
    if (m_num_sessions >= kMaxSessions) {
        return MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);
    }
    Handle s = INVALID_HANDLE;
    Result rc = svcAcceptSession(&s, m_port_handle);
    if (R_FAILED(rc)) return rc;
    m_handles[m_num_sessions++] = s;
    return 0;
}

void IpcServer::closeSession(int idx) {
    if (idx < 0 || idx >= m_num_sessions) return;
    svcCloseHandle(m_handles[idx]);
    for (int i = idx; i < m_num_sessions - 1; ++i) {
        m_handles[i] = m_handles[i + 1];
    }
    m_num_sessions--;
}

// =========================================================================
// Dispatcher por command id
// =========================================================================
Result IpcServer::handleSession(int /*idx*/) {
    void *tls = armGetTls();
    HipcParsedRequest req = hipcParseRequest(tls);

    CmifInHdr *in_hdr = alignTo16<CmifInHdr>(req.data.data_words);
    if (in_hdr->magic != kSfciMagic) {
        return writeResponse(MAKERESULT(Module_Libnx, LibnxError_BadInput), nullptr, 0);
    }
    void *in_payload = static_cast<void *>(in_hdr + 1);

    switch (static_cast<seng::Cmd>(in_hdr->cmd_id)) {

        case seng::Cmd::GetVersion: {
            u32 v = seng::kIpcVersion;
            return writeResponse(0, &v, sizeof(v));
        }

        case seng::Cmd::GetForegroundPid: {
            u64 pid = 0;
            Result rc = Debugger::getForegroundPid(&pid);
            return writeResponse(rc, &pid, sizeof(pid));
        }

        case seng::Cmd::GetTitleId: {
            u64 pid = *static_cast<u64 *>(in_payload);
            u64 tid = 0;
            Result rc = Debugger::getTitleId(pid, &tid);
            return writeResponse(rc, &tid, sizeof(tid));
        }

        case seng::Cmd::AttachProcess: {
            u64 pid = *static_cast<u64 *>(in_payload);
            return writeResponse(Debugger::attach(pid), nullptr, 0);
        }

        case seng::Cmd::DetachProcess: {
            Debugger::detach();
            return writeResponse(0, nullptr, 0);
        }

        case seng::Cmd::IsAttached: {
            u8 a = Debugger::isAttached() ? 1 : 0;
            return writeResponse(0, &a, sizeof(a));
        }

        case seng::Cmd::QueryMemory: {
            u64 addr = *static_cast<u64 *>(in_payload);
            seng::MemoryRegion mr{};
            Result rc = Debugger::queryMemory(addr, &mr);
            return writeResponse(rc, &mr, sizeof(mr));
        }

        case seng::Cmd::ReadMemory: {
            // Args: (u64 addr, u64 size). Output: Type-B buffer.
            u64 *args = static_cast<u64 *>(in_payload);
            u64  addr = args[0];
            u64  size = args[1];
            if (req.meta.num_recv_buffers < 1) {
                return writeResponse(MAKERESULT(Module_Libnx, LibnxError_BadInput), nullptr, 0);
            }
            u64   bsize = 0;
            void *dst   = bufferAddress(&req.data.recv_buffers[0], &bsize);
            if (size > bsize)                 size = bsize;
            if (size > seng::kMaxChunkBytes)  size = seng::kMaxChunkBytes;

            Result rc = Debugger::readMemory(addr, dst, size);
            return writeResponse(rc, &size, sizeof(size));
        }

        case seng::Cmd::WriteMemory: {
            // Args: (u64 addr, u64 size). Input: Type-A buffer.
            u64 *args = static_cast<u64 *>(in_payload);
            u64  addr = args[0];
            u64  size = args[1];
            if (req.meta.num_send_buffers < 1) {
                return writeResponse(MAKERESULT(Module_Libnx, LibnxError_BadInput), nullptr, 0);
            }
            u64         bsize = 0;
            const void *src   = bufferAddress(&req.data.send_buffers[0], &bsize);
            if (size > bsize)                 size = bsize;
            if (size > seng::kMaxChunkBytes)  size = seng::kMaxChunkBytes;

            Result rc = Debugger::writeMemory(addr, src, size);
            return writeResponse(rc, &size, sizeof(size));
        }

        default:
            return writeResponse(MAKERESULT(Module_Libnx, LibnxError_NotFound), nullptr, 0);
    }
}

// =========================================================================
// Loop principal
// =========================================================================
void IpcServer::runForever() {
    Handle reply_target = INVALID_HANDLE;

    while (true) {
        // Monta vetor [port, sessao_0, sessao_1, ...]
        Handle handles[kMaxHandles];
        handles[0] = m_port_handle;
        for (int i = 0; i < m_num_sessions; ++i) {
            handles[1 + i] = m_handles[i];
        }
        s32 num = 1 + m_num_sessions;

        s32    idx = -1;
        Result rc  = svcReplyAndReceive(&idx, handles, num, reply_target, UINT64_MAX);
        reply_target = INVALID_HANDLE;

        if (R_FAILED(rc)) {
            // Sessao fechada por peer? Procura qual handle e remove.
            if (rc == KERNELRESULT(ConnectionClosed) && idx >= 1) {
                closeSession(idx - 1);
            }
            // Outros erros: ignora e continua.
            continue;
        }

        if (idx == 0) {
            acceptNewSession(); // ignora erro: se nao conseguir aceitar, segue
        } else {
            int session_idx = idx - 1;
            if (R_SUCCEEDED(handleSession(session_idx))) {
                reply_target = m_handles[session_idx];
            } else {
                closeSession(session_idx);
            }
        }
    }
}

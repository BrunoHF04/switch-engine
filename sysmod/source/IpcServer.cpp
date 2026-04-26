#include "IpcServer.hpp"
#include "Debugger.hpp"
#include "SysmodLog.hpp"
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
    Result rc = smRegisterService(&m_port_handle, name, false, kPortMaxSessions);
    seng::mod::log::write("[ipc] registerService(seng) rc=0x%08X port=0x%X",
                          rc, m_port_handle);
    return rc;
}

// =========================================================================
Result IpcServer::acceptNewSession() {
    if (m_num_sessions >= kMaxSessions) {
        seng::mod::log::write("[ipc] acceptNewSession: REJECTED (max=%d)",
                              kMaxSessions);
        return MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);
    }
    Handle s = INVALID_HANDLE;
    Result rc = svcAcceptSession(&s, m_port_handle);
    if (R_FAILED(rc)) {
        seng::mod::log::write("[ipc] svcAcceptSession FAILED rc=0x%08X", rc);
        return rc;
    }
    m_handles[m_num_sessions++] = s;
    seng::mod::log::write("[ipc] session accepted h=0x%X (total=%d)",
                          s, m_num_sessions);
    return 0;
}

void IpcServer::closeSession(int idx) {
    if (idx < 0 || idx >= m_num_sessions) {
        seng::mod::log::write("[ipc] closeSession: idx=%d OUT OF RANGE (sessions=%d)",
                              idx, m_num_sessions);
        return;
    }
    Handle h = m_handles[idx];
    svcCloseHandle(h);
    for (int i = idx; i < m_num_sessions - 1; ++i) {
        m_handles[i] = m_handles[i + 1];
    }
    m_handles[m_num_sessions - 1] = INVALID_HANDLE;
    m_num_sessions--;
    seng::mod::log::write("[ipc] session closed h=0x%X idx=%d (remaining=%d)",
                          h, idx, m_num_sessions);
}

// =========================================================================
// Dispatcher por command id
// =========================================================================
Result IpcServer::handleSession(int idx) {
    void *tls = armGetTls();
    HipcParsedRequest req = hipcParseRequest(tls);

    CmifInHdr *in_hdr = alignTo16<CmifInHdr>(req.data.data_words);
    if (!in_hdr) {
        seng::mod::log::write("[ipc] handleSession idx=%d: in_hdr=NULL", idx);
        return writeResponse(MAKERESULT(Module_Libnx, LibnxError_BadInput), nullptr, 0);
    }
    if (in_hdr->magic != kSfciMagic) {
        // Pode ser o "Control" command (CloseSession etc.). Logamos e
        // respondemos BadInput; o cliente vai receber o reply e seguir.
        seng::mod::log::write("[ipc] handleSession idx=%d: bad magic 0x%08X (esperado SFCI)",
                              idx, in_hdr->magic);
        return writeResponse(MAKERESULT(Module_Libnx, LibnxError_BadInput), nullptr, 0);
    }
    void *in_payload = static_cast<void *>(in_hdr + 1);

    seng::mod::log::write("[ipc] cmd=%u idx=%d (sessions=%d)",
                          in_hdr->cmd_id, idx, m_num_sessions);

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
//
// Estrategia de erros:
//   - svcReplyAndReceive falha: tipicamente ConnectionClosed (peer fechou).
//     Identificamos qual sessao ficou ruim pelo idx retornado e fechamos.
//     Para erros desconhecidos COM idx valido, fechamos mesmo assim para
//     evitar busy loop em handle quebrado.
//   - handleSession falha (writeResponse retornou erro): fechamos a sessao.
//   - Caso o reply em si falhe na proxima iteracao, o erro propaga para
//     o tratamento acima.
//
// O handle de "port" sempre fica em handles[0]. Sessoes vivas em [1..N].
// =========================================================================
void IpcServer::runForever() {
    Handle reply_target = INVALID_HANDLE;

    while (true) {
        // Monta vetor [port, sessao_0, sessao_1, ...]. Limite de seguranca
        // no caso de m_num_sessions estar corrompido por algum motivo.
        Handle handles[kMaxHandles];
        handles[0] = m_port_handle;
        int copied = 0;
        for (int i = 0; i < m_num_sessions && i < kMaxSessions; ++i) {
            handles[1 + i] = m_handles[i];
            ++copied;
        }
        s32 num = 1 + copied;

        s32    idx = -1;
        Result rc  = svcReplyAndReceive(&idx, handles, num, reply_target, UINT64_MAX);
        Handle prev_reply = reply_target;
        reply_target = INVALID_HANDLE;

        if (R_FAILED(rc)) {
            seng::mod::log::write("[ipc] svcReplyAndReceive rc=0x%08X idx=%d num=%d reply=0x%X",
                                  rc, idx, num, prev_reply);
            // idx aponta para a sessao que sinalizou (>=1). Se for valida,
            // fechamos -- assim nao loopamos em handle ruim.
            if (idx >= 1 && idx <= copied) {
                closeSession(idx - 1);
            } else if (rc == KERNELRESULT(ConnectionClosed)) {
                // ConnectionClosed sem idx valido: anomalia, mas seguimos.
                seng::mod::log::write("[ipc] ConnectionClosed sem idx valido (idx=%d)",
                                      idx);
            }
            continue;
        }

        if (idx < 0 || idx >= num) {
            seng::mod::log::write("[ipc] idx fora de range (idx=%d num=%d)", idx, num);
            continue;
        }

        if (idx == 0) {
            // Porta sinalizou: novo cliente quer conectar.
            acceptNewSession();
        } else {
            int session_idx = idx - 1;
            if (session_idx < 0 || session_idx >= m_num_sessions) {
                seng::mod::log::write("[ipc] session_idx invalido (%d, sessions=%d)",
                                      session_idx, m_num_sessions);
                continue;
            }
            Result hrc = handleSession(session_idx);
            if (R_SUCCEEDED(hrc)) {
                // Confirma que ainda esta dentro do range antes de assinar
                // o reply target -- closeSession dentro do handler poderia
                // ter mexido em m_num_sessions.
                if (session_idx < m_num_sessions) {
                    reply_target = m_handles[session_idx];
                }
            } else {
                seng::mod::log::write("[ipc] handleSession idx=%d FAILED rc=0x%08X",
                                      session_idx, hrc);
                closeSession(session_idx);
            }
        }
    }
}

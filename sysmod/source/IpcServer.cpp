#include "IpcServer.hpp"
#include "Debugger.hpp"
#include "FreezeManager.hpp"
#include "ScanRunner.hpp"
#include "SysmodLog.hpp"
#include "seng_ipc.hpp"

#include <cstring>
#include <switch/sf/cmif.h>

namespace {

    alignas(16) static u8 g_ipc_stash[0x400];
    constexpr size_t kTlsCopyBytes = sizeof(g_ipc_stash);

    Result writeResponse(Result rc,
                         const void *payload,
                         size_t      payload_size) {
        size_t bytes = sizeof(CmifOutHeader) + payload_size + 16;
        size_t data_words = (bytes + 3) / 4;

        HipcMetadata meta = {};
        meta.type             = CmifCommandType_Request;
        meta.num_data_words   = static_cast<u32>(data_words);

        void       *base = armGetTls();
        HipcRequest req  = hipcMakeRequestInline(base, meta);

        CmifOutHeader *out = reinterpret_cast<CmifOutHeader *>(
            cmifGetAlignedDataStart(req.data_words, base));
        out->magic   = CMIF_OUT_HEADER_MAGIC;
        out->version = 0;
        out->result  = rc;
        out->token   = 0;
        if (payload && payload_size > 0) {
            std::memcpy(out + 1, payload, payload_size);
        }
        return 0;
    }

    void *bufferAddress(const HipcBufferDescriptor *d, u64 *out_size) {
        if (out_size) *out_size = hipcGetBufferSize(d);
        return hipcGetBufferAddress(d);
    }

    bool resolveClientOutBuffer(const HipcParsedRequest &req,
                                void                    **out_ptr,
                                u64                      *out_size) {
        *out_ptr  = nullptr;
        *out_size = 0;
        if (req.data.recv_buffers && req.meta.num_recv_buffers > 0) {
            for (u32 i = 0; i < req.meta.num_recv_buffers; ++i) {
                u64   sz = 0;
                void *p  = bufferAddress(&req.data.recv_buffers[i], &sz);
                if (p && sz > 0) {
                    *out_ptr  = p;
                    *out_size = sz;
                    return true;
                }
            }
        }
        if (req.data.recv_list && req.meta.num_recv_statics != 0 &&
            req.meta.num_recv_statics != HIPC_AUTO_RECV_STATIC) {
            for (u32 i = 0; i < req.meta.num_recv_statics; ++i) {
                const HipcRecvListEntry &e = req.data.recv_list[i];
                const uintptr_t addr =
                    static_cast<uintptr_t>(e.address_low) |
                    ((static_cast<uintptr_t>(e.address_high) & 0xFFFFu) << 32);
                const u32 sz = static_cast<u32>(e.size);
                if (addr != 0 && sz > 0) {
                    *out_ptr  = reinterpret_cast<void *>(addr);
                    *out_size = sz;
                    return true;
                }
            }
        }
        return false;
    }
}

IpcServer::IpcServer()  = default;

IpcServer::~IpcServer() {
    if (m_port_handle != INVALID_HANDLE) svcCloseHandle(m_port_handle);
    for (int i = 0; i < m_num_sessions; ++i) svcCloseHandle(m_handles[i]);
}

Result IpcServer::registerService() {
    SmServiceName name = smEncodeName(seng::kServiceName);
    Result rc = smRegisterService(&m_port_handle, name, false, kPortMaxSessions);
    seng::mod::log::write("INFO [ipc] registerService(seng) rc=0x%08X port=0x%X",
                          rc, m_port_handle);
    return rc;
}

Result IpcServer::acceptNewSession() {
    if (m_num_sessions >= kMaxSessions) {
        seng::mod::log::write("WARN [ipc] acceptNewSession: REJECTED (max=%d)",
                              kMaxSessions);
        return MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);
    }
    Handle s = INVALID_HANDLE;
    Result rc = svcAcceptSession(&s, m_port_handle);
    if (R_FAILED(rc)) {
        seng::mod::log::write("ERR  [ipc] svcAcceptSession FAILED rc=0x%08X", rc);
        return rc;
    }
    m_handles[m_num_sessions++] = s;
    seng::mod::log::write("INFO [ipc] session accepted h=0x%X (total=%d)",
                          s, m_num_sessions);
    return 0;
}

void IpcServer::closeSession(int idx) {
    if (idx < 0 || idx >= m_num_sessions) {
        seng::mod::log::write("ERR  [ipc] closeSession: idx=%d OUT OF RANGE (sessions=%d)",
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
    seng::mod::log::write("INFO [ipc] session closed h=0x%X idx=%d (remaining=%d)",
                          h, idx, m_num_sessions);
}

Result IpcServer::handleSession(int idx) {
    void *tls = armGetTls();
    std::memcpy(g_ipc_stash, tls, kTlsCopyBytes);

    void              *msg_base = g_ipc_stash;
    HipcParsedRequest  req      = hipcParseRequest(msg_base);

    if (!req.data.data_words) {
        seng::mod::log::write("ERR  [ipc] handleSession idx=%d: data_words=NULL", idx);
        return writeResponse(MAKERESULT(Module_Libnx, LibnxError_BadInput), nullptr, 0);
    }

    const u32 hipc_type = req.meta.type;
    if (hipc_type == CmifCommandType_Control ||
        hipc_type == CmifCommandType_ControlWithContext) {
        CmifInHeader *ctrl = reinterpret_cast<CmifInHeader *>(
            cmifGetAlignedDataStart(req.data.data_words, msg_base));
        const u32 ctrl_cmd = (ctrl->magic == CMIF_IN_HEADER_MAGIC)
                                 ? ctrl->command_id : 0xFFFFFFFFu;
        seng::mod::log::write("INFO [ipc] CONTROL cmd=%u idx=%d", ctrl_cmd, idx);
        if (ctrl_cmd == 3) {
            u16 pbsz = 0;
            return writeResponse(0, &pbsz, sizeof(pbsz));
        }
        return writeResponse(0, nullptr, 0);
    }

    if (hipc_type == CmifCommandType_Close) {
        seng::mod::log::write("INFO [ipc] CLOSE idx=%d", idx);
        return writeResponse(0, nullptr, 0);
    }

    CmifInHeader *in_hdr = reinterpret_cast<CmifInHeader *>(
        cmifGetAlignedDataStart(req.data.data_words, msg_base));
    if (in_hdr->magic != CMIF_IN_HEADER_MAGIC) {
        seng::mod::log::write("ERR  [ipc] handleSession idx=%d: bad magic 0x%08X tipo=%u",
                              idx, in_hdr->magic, hipc_type);
        return writeResponse(MAKERESULT(Module_Libnx, LibnxError_BadInput), nullptr, 0);
    }

    void *in_payload = reinterpret_cast<u8 *>(in_hdr) + sizeof(CmifInHeader);
    const u32 cmd_id = in_hdr->command_id;

    seng::mod::log::write("INFO [ipc] cmd=%u idx=%d (sessions=%d)",
                          cmd_id, idx, m_num_sessions);

    switch (static_cast<seng::Cmd>(cmd_id)) {

        case seng::Cmd::GetVersion: {
            seng::mod::log::write("INFO [ipc] -> GetVersion");
            u32 v = seng::kIpcVersion;
            return writeResponse(0, &v, sizeof(v));
        }

        case seng::Cmd::GetForegroundPid: {
            seng::mod::log::write("INFO [ipc] -> GetForegroundPid");
            u64 pid = 0;
            Result rc = Debugger::getForegroundPid(&pid);
            seng::mod::log::write("INFO [ipc] GetForegroundPid rc=0x%08X pidLo=%u",
                                  rc, static_cast<u32>(pid & 0xFFFFFFFFu));
            return writeResponse(rc, &pid, sizeof(pid));
        }

        case seng::Cmd::GetTitleId: {
            u64 pid = *static_cast<u64 *>(in_payload);
            u64 tid = 0;
            Result rc = Debugger::getTitleId(pid, &tid);
            seng::mod::log::write("INFO [ipc] GetTitleId pidLo=%u tidLo=%X rc=0x%08X",
                                  static_cast<u32>(pid & 0xFFFFFFFFu),
                                  static_cast<u32>(tid & 0xFFFFFFFFu), rc);
            return writeResponse(rc, &tid, sizeof(tid));
        }

        case seng::Cmd::AttachProcess: {
            u64 pid = *static_cast<u64 *>(in_payload);
            Result rc = Debugger::attach(pid);
            seng::mod::log::write("INFO [ipc] AttachProcess pidLo=%u rc=0x%08X",
                                  static_cast<u32>(pid & 0xFFFFFFFFu), rc);
            return writeResponse(rc, nullptr, 0);
        }

        case seng::Cmd::DetachProcess: {
            seng::mod::log::write("INFO [ipc] -> DetachProcess");
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
            u64 *args = static_cast<u64 *>(in_payload);
            u64  addr = args[0];
            u64  size = args[1];
            void *dst   = nullptr;
            u64   bsize = 0;
            if (!resolveClientOutBuffer(req, &dst, &bsize)) {
                return writeResponse(MAKERESULT(Module_Libnx, LibnxError_BadInput), nullptr, 0);
            }
            if (size > bsize)                 size = bsize;
            if (size > seng::kMaxChunkBytes)  size = seng::kMaxChunkBytes;

            Result rc = Debugger::readMemory(addr, dst, size);
            return writeResponse(rc, &size, sizeof(size));
        }

        case seng::Cmd::WriteMemory: {
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

        case seng::Cmd::ListProcessesLegacy:
        case seng::Cmd::ListProcesses: {
            seng::mod::log::write("INFO [ipc] -> ListProcesses");

            u64 max_entries = *static_cast<u64 *>(in_payload);
            void *dst   = nullptr;
            u64   bsize = 0;
            if (!resolveClientOutBuffer(req, &dst, &bsize)) {
                seng::mod::log::write("ERR  [ipc] ListProcesses: NO OUT BUFFER");
                return writeResponse(MAKERESULT(Module_Libnx, LibnxError_BadInput),
                                     nullptr, 0);
            }

            const size_t can_fit = bsize / sizeof(seng::ProcessEntry);
            size_t cap = max_entries < can_fit ? max_entries : can_fit;
            if (cap > seng::kMaxProcessList) cap = seng::kMaxProcessList;

            size_t actual = 0;
            Result rc = Debugger::listProcesses(
                static_cast<seng::ProcessEntry *>(dst), cap, &actual);

            seng::mod::log::write("INFO [ipc] ListProcesses cap=%u actual=%u rc=0x%08X",
                                  static_cast<u32>(cap),
                                  static_cast<u32>(actual), rc);

            u64 count_out = actual;
            return writeResponse(rc, &count_out, sizeof(count_out));
        }

        case seng::Cmd::StartMemoryScan: {
            seng::mod::log::write("INFO [ipc] -> StartMemoryScan");
            const seng::ScanParams *sp =
                static_cast<const seng::ScanParams *>(in_payload);
            u64 total = 0;
            const Result rc = seng::mod::runFirstScan(sp->pid, *sp, &total);
            seng::mod::log::write(
                "INFO [ipc] StartMemoryScan rc=0x%08X hitsLo=%u pidLo=%u type=%u op=%u",
                rc,
                static_cast<u32>(total & 0xFFFFFFFFu),
                static_cast<u32>(sp->pid & 0xFFFFFFFFu),
                static_cast<u32>(sp->type),
                static_cast<u32>(sp->op));
            return writeResponse(rc, &total, sizeof(total));
        }

        case seng::Cmd::AddFreeze: {
            seng::mod::log::write("INFO [ipc] -> AddFreeze");
            const seng::FreezeParams *fp =
                static_cast<const seng::FreezeParams *>(in_payload);
            uint8_t slot = 0xFF;
            Result rc = FreezeManager::addFreeze(fp->addr, fp->type, fp->value, &slot);
            return writeResponse(rc, &slot, sizeof(slot));
        }

        case seng::Cmd::RemoveFreeze: {
            u8 slot = *static_cast<u8 *>(in_payload);
            seng::mod::log::write("INFO [ipc] -> RemoveFreeze slot=%u", slot);
            FreezeManager::removeFreeze(slot);
            return writeResponse(0, nullptr, 0);
        }

        case seng::Cmd::ListFreezes: {
            void *dst   = nullptr;
            u64   bsize = 0;
            if (!resolveClientOutBuffer(req, &dst, &bsize)) {
                return writeResponse(MAKERESULT(Module_Libnx, LibnxError_BadInput),
                                     nullptr, 0);
            }
            size_t max_slots = bsize / sizeof(seng::FreezeEntry);
            if (max_slots > seng::kMaxFreezeSlots) max_slots = seng::kMaxFreezeSlots;
            size_t n = FreezeManager::listFreezes(
                static_cast<seng::FreezeEntry *>(dst), max_slots);
            u8 count_out = static_cast<u8>(n);
            return writeResponse(0, &count_out, sizeof(count_out));
        }

        case seng::Cmd::ClearFreezes: {
            seng::mod::log::write("INFO [ipc] -> ClearFreezes");
            FreezeManager::clearAll();
            return writeResponse(0, nullptr, 0);
        }

        default:
            seng::mod::log::write("WARN [ipc] comando nao implementado cmd=%u", cmd_id);
            return writeResponse(MAKERESULT(Module_Libnx, LibnxError_NotFound), nullptr, 0);
    }
}

void IpcServer::runForever() {
    Handle reply_target = INVALID_HANDLE;

    while (true) {
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
            seng::mod::log::write("WARN [ipc] svcReplyAndReceive rc=0x%08X idx=%d num=%d reply=0x%X",
                                  rc, idx, num, prev_reply);
            if (idx >= 1 && idx <= copied) {
                closeSession(idx - 1);
            } else if (rc == KERNELRESULT(ConnectionClosed)) {
                seng::mod::log::write("WARN [ipc] ConnectionClosed sem idx valido (idx=%d)",
                                      idx);
            }
            continue;
        }

        if (idx < 0 || idx >= num) {
            seng::mod::log::write("WARN [ipc] idx fora de range (idx=%d num=%d)", idx, num);
            continue;
        }

        if (idx == 0) {
            acceptNewSession();
        } else {
            int session_idx = idx - 1;
            if (session_idx < 0 || session_idx >= m_num_sessions) {
                seng::mod::log::write("WARN [ipc] session_idx invalido (%d, sessions=%d)",
                                      session_idx, m_num_sessions);
                continue;
            }
            Result hrc = handleSession(session_idx);
            if (R_SUCCEEDED(hrc)) {
                if (session_idx < m_num_sessions) {
                    reply_target = m_handles[session_idx];
                }
            } else {
                seng::mod::log::write("ERR  [ipc] handleSession idx=%d FAILED rc=0x%08X",
                                      session_idx, hrc);
                closeSession(session_idx);
            }
        }
    }
}

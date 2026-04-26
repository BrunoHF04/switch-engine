#pragma once

#include <switch.h>
#include <cstdint>

/**
 * IpcServer
 *
 * Servidor CMIF "raw" sobre HIPC. Sem libstratosphere -- so libnx + svc.
 *
 * Modelo:
 *   - registerService("seng") com sm:
 *   - loop unico em runForever() multiplexa N sessoes via svcReplyAndReceive
 *   - cada request: parseamos CmifInHeader, despachamos por command_id,
 *     escrevemos CmifOutHeader + payload de saida.
 *
 * Limites:
 *   - kMaxSessions = 4 (um overlay so abre 1 mesmo)
 *   - buffers via HIPC map alias (Type-A in / Type-B out), tamanho <= 64KB.
 */
class IpcServer {
public:
    IpcServer();
    ~IpcServer();

    Result registerService();
    void   runForever();

private:
    static constexpr int  kMaxSessions = 4;
    static constexpr int  kMaxHandles  = kMaxSessions + 1;
    static constexpr u64  kPortMaxSessions = kMaxSessions;

    Handle m_port_handle = INVALID_HANDLE;
    Handle m_handles[kMaxHandles] = {};
    int    m_num_sessions = 0;

    Result acceptNewSession();
    Result handleSession(int session_idx);
    void   closeSession(int session_idx);
};

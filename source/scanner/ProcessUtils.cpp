#include "ProcessUtils.hpp"
#include "SengClient.hpp"

namespace ProcessUtils {

    Result getForegroundApplication(uint64_t *out_pid, uint64_t *out_title_id) {
        if (!out_pid || !out_title_id) {
            return MAKERESULT(Module_Libnx, LibnxError_BadInput);
        }

        // Garante conexao com o sysmod (lazy init). Se o sysmod nao estiver
        // instalado, smGetService retorna 0xE15 (notFound).
        Result rc = SengClient::initialize();
        if (R_FAILED(rc)) return rc;

        rc = SengClient::getForegroundPid(out_pid);
        if (R_FAILED(rc)) return rc;

        return SengClient::getTitleId(*out_pid, out_title_id);
    }

    Result attachDebug(uint64_t pid, Handle * /*out_debug_handle*/) {
        // Compat: o overlay nao guarda mais um Handle local; o sysmod e' que
        // mantem o debug attached. Mantemos a assinatura para nao quebrar
        // codigo cliente -- mas o handle retorna sempre INVALID_HANDLE.
        Result rc = SengClient::initialize();
        if (R_FAILED(rc)) return rc;
        return SengClient::attach(pid);
    }

    void detachDebug(Handle /*debug_handle*/) {
        // Idem: o handle real vive no sysmod.
        if (SengClient::isInitialized()) {
            SengClient::detach();
        }
    }

} // namespace ProcessUtils

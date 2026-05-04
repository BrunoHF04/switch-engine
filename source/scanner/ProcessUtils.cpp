#include "ProcessUtils.hpp"
#include "SengClient.hpp"

namespace ProcessUtils {

    Result getForegroundApplication(uint64_t *out_pid, uint64_t *out_title_id) {
        if (!out_pid || !out_title_id) {
            return MAKERESULT(Module_Libnx, LibnxError_BadInput);
        }

        Result rc = SengClient::initialize();
        if (R_FAILED(rc)) return rc;

        rc = SengClient::getForegroundPid(out_pid);
        if (R_FAILED(rc)) return rc;
        if (*out_pid <= 1) {
            return MAKERESULT(Module_Libnx, LibnxError_NotFound);
        }

        return SengClient::getTitleId(*out_pid, out_title_id);
    }

    Result attachDebug(uint64_t pid, void * /*unused*/) {
        Result rc = SengClient::initialize();
        if (R_FAILED(rc)) return rc;
        return SengClient::attach(pid);
    }

    void detachDebug(int /*unused*/) {
        if (SengClient::isInitialized()) {
            SengClient::detach();
        }
    }

} // namespace ProcessUtils

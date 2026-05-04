#pragma once

#include <switch.h>
#include <cstdint>

/**
 * ProcessUtils
 *
 * Helpers para descobrir o processo alvo e gerenciar attach/detach via IPC.
 * O debug handle real vive no sysmod — o overlay apenas faz attach/detach
 * remoto via SengClient.
 */
namespace ProcessUtils {

    Result getForegroundApplication(uint64_t *out_pid, uint64_t *out_title_id);

    Result attachDebug(uint64_t pid, void *unused);

    void   detachDebug(int unused);

} // namespace ProcessUtils

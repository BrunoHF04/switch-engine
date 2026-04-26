#pragma once

#include <switch.h>
#include <cstdint>

/**
 * ProcessUtils
 *
 * Helpers para descobrir QUEM o usuario quer "scanear":
 *   1. PID do Application em primeiro plano (via pmdmnt).
 *   2. TitleID a partir do PID (via pminfo).
 *   3. Abertura do debug handle via svcDebugActiveProcess.
 *
 * Importante (capabilities NPDM):
 *   - svcDebugActiveProcess (SVC 0x60) precisa estar habilitado.
 *   - Se voce esta rodando como overlay puro, ovlloader NAO concede esse SVC.
 *     Nesse caso, mova esta logica para um SysModule companion e exponha via
 *     IPC. Veja README.md para o esqueleto.
 */
namespace ProcessUtils {

    Result getForegroundApplication(uint64_t *out_pid, uint64_t *out_title_id);

    Result attachDebug(uint64_t pid, Handle *out_debug_handle);

    void   detachDebug(Handle debug_handle);

} // namespace ProcessUtils

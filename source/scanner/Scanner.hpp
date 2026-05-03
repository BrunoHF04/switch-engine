#pragma once

#include <switch.h>

#include <cstdint>

/**
 * Passos de "ativacao" do scanner no overlay: cmd 10 (lista) + deteccao
 * de primeiro plano via IPC existente.
 */
namespace Scanner {

/** Garante sessao seng, envia ListProcesses (cmd 10), depois resolve PID/TID. */
Result detectForegroundAfterProcessList(uint64_t *out_pid, uint64_t *out_tid);

} // namespace Scanner

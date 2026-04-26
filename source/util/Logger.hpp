#pragma once

/**
 * Logger
 *
 * Append-only logger em sdmc:/switch-engine.log. Usado para diagnosticar
 * o boot do overlay (por que ele nao aparece, por que crasha, etc.) sem
 * precisar de nxlink ou debugger.
 *
 * O arquivo e' aberto na primeira chamada de log() e mantido aberto.
 * Cada log() faz fflush() para garantir que mensagens sobrevivem a um
 * crash do overlay.
 *
 * Uso:
 *   seng::log::write("[overlay] init pid=%llu", pid);
 *
 * Importante: ZERO dependencias do nosso codigo (so fopen/fprintf), entao
 * pode ser chamado de qualquer lugar -- inclusive antes de initServices().
 */
namespace seng::log {

    constexpr const char *kPath = "sdmc:/switch-engine.log";

    void write(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
    void close();
    void reset(); // truncates the log file

} // namespace seng::log

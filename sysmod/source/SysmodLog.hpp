#pragma once

/**
 * Logger persistente para o switch-engine-mod.
 *
 * Niveis: INFO, WARN, ERR (prefixados automaticamente no timestamp).
 * Timestamps: monotonic ms desde boot (armGetSystemTick).
 *
 * Arquivos:
 *   - sdmc:/switch-engine_mod.log         tracer de execucao
 *   - sdmc:/switch-engine_mod_crash.log   dump de exceptions
 */

namespace seng::mod::log {

    void init();

    void write(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

    void writeRaw(const char *msg);

    void close();

    void crashLog(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

} // namespace seng::mod::log

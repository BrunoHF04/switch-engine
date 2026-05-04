#pragma once

/**
 * Logger
 *
 * Append-only logger em sdmc:/switch-engine.log com timestamps monotonicos
 * e niveis (INFO/WARN/ERR embutidos na mensagem pelo caller).
 *
 * Uso:
 *   seng::log::write("INFO [overlay] init pid=%llu", pid);
 *   seng::log::write("ERR  [overlay] falhou rc=0x%08X", rc);
 */
namespace seng::log {

    constexpr const char *kPath = "sdmc:/switch-engine.log";

    void write(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
    void close();
    void reset();

} // namespace seng::log

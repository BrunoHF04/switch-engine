#pragma once

/**
 * Logger persistente para o switch-engine-mod.
 *
 * O sysmod e' headless (sem stdout/stderr): tudo que voce quer rastrear
 * tem que ser escrito em arquivo no SD. Usamos:
 *
 *   - sdmc:/switch-engine_mod.log         tracer de execucao (IPCs, init, exit)
 *   - sdmc:/switch-engine_mod_crash.log   dump de exceptions (PC/LR/registros)
 *
 * As funcoes fazem flush imediato apos cada escrita (anti-crash) e abrem
 * o arquivo em modo append a cada chamada -- mais lento, porem nao perde
 * nada quando o processo morre.
 *
 * Init: precisa ter fsdevMountSdmc() chamado antes (no __appInit / main).
 */

namespace seng::mod::log {

    void init();

    void write(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

    void writeRaw(const char *msg);

    void close();

    void crashLog(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

} // namespace seng::mod::log

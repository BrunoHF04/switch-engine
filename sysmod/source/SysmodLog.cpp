#include "SysmodLog.hpp"

#include <switch.h>

#include <cstdarg>
#include <cstdio>
#include <ctime>

namespace seng::mod::log {

    namespace {
        constexpr const char *kRunLog   = "sdmc:/switch-engine_mod.log";
        constexpr const char *kCrashLog = "sdmc:/switch-engine_mod_crash.log";

        // Ticks -> ms desde boot. Usamos so' como timestamp relativo e' o
        // suficiente para correlacionar eventos.
        u64 monotonicMs() {
            // armGetSystemTick conta a 19200000 Hz -- ms = ticks / 19200.
            return armTicksToNs(armGetSystemTick()) / 1'000'000ULL;
        }

        // Helper: append uma linha + flush + close.
        // Reabre a cada chamada (mais lento) para nao perder log se crashar.
        void appendLine(const char *path, const char *line) {
            FILE *fp = std::fopen(path, "a");
            if (!fp) return;
            std::fputs(line, fp);
            std::fflush(fp);
            std::fclose(fp);
        }
    } // namespace

    void init() {
        // Marca inicio com cabecalho que inclui wallclock E build hash.
        // O build hash e' fornecido pelo Makefile (gera string unica por
        // build) -- assim, mesmo que rodemos a mesma sessao com sysmod
        // antigo, vemos imediatamente qual binario esta vivo.
        u64        nano = armTicksToNs(armGetSystemTick());
        time_t     now  = std::time(nullptr);

        // Tag do build: armado por -DSENG_BUILD_TAG=... no Makefile.
        // Fallback simples se nao definir.
#ifndef SENG_BUILD_TAG
#define SENG_BUILD_TAG "unknown"
#endif

        char header[160];
        std::snprintf(header, sizeof(header),
                      "\n=== switch-engine-mod start tick=%llu wallclock=%ld build=%s ===\n",
                      static_cast<unsigned long long>(nano / 1'000'000ULL),
                      static_cast<long>(now),
                      SENG_BUILD_TAG);
        appendLine(kRunLog, header);
    }

    void writeRaw(const char *msg) {
        if (!msg) return;
        char buf[512];
        std::snprintf(buf, sizeof(buf), "[%010llu] %s\n",
                      static_cast<unsigned long long>(monotonicMs()), msg);
        appendLine(kRunLog, buf);
    }

    void write(const char *fmt, ...) {
        if (!fmt) return;
        char body[480];
        va_list ap;
        va_start(ap, fmt);
        std::vsnprintf(body, sizeof(body), fmt, ap);
        va_end(ap);

        char line[512];
        std::snprintf(line, sizeof(line), "[%010llu] %s\n",
                      static_cast<unsigned long long>(monotonicMs()), body);
        appendLine(kRunLog, line);
    }

    void close() {
        // Modelo "abre/fecha por chamada": nada a finalizar de fato.
        // Marcamos um exit explicito para correlacionar com runs.
        appendLine(kRunLog, "=== switch-engine-mod exit ===\n");
    }

    void crashLog(const char *fmt, ...) {
        if (!fmt) return;
        char body[480];
        va_list ap;
        va_start(ap, fmt);
        std::vsnprintf(body, sizeof(body), fmt, ap);
        va_end(ap);

        char line[512];
        std::snprintf(line, sizeof(line), "[%010llu] %s\n",
                      static_cast<unsigned long long>(monotonicMs()), body);
        // Crashes vao para os DOIS arquivos: o run log (para correlacao
        // temporal) e o crash log dedicado (para grep rapido).
        appendLine(kRunLog,   line);
        appendLine(kCrashLog, line);
    }

} // namespace seng::mod::log

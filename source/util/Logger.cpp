#include "Logger.hpp"

#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <mutex>

namespace seng::log {

    namespace {
        std::mutex g_mutex;
        FILE      *g_fp = nullptr;

        void ensureOpen_locked() {
            if (g_fp != nullptr) return;
            // "a" -> append. Se nao existir, cria.
            g_fp = std::fopen(kPath, "a");
        }

        void writeTimestamp_locked() {
            // tick contador (sem RTC, evita dependencia em time services).
            // Suficiente para ver ordenacao das mensagens.
            static unsigned long long counter = 0;
            std::fprintf(g_fp, "[%06llu] ", counter++);
        }
    }

    void write(const char *fmt, ...) {
        std::lock_guard<std::mutex> lock(g_mutex);
        ensureOpen_locked();
        if (g_fp == nullptr) return;

        writeTimestamp_locked();

        va_list args;
        va_start(args, fmt);
        std::vfprintf(g_fp, fmt, args);
        va_end(args);

        std::fputc('\n', g_fp);
        std::fflush(g_fp);
    }

    void close() {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (g_fp != nullptr) {
            std::fclose(g_fp);
            g_fp = nullptr;
        }
    }

    void reset() {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (g_fp != nullptr) {
            std::fclose(g_fp);
            g_fp = nullptr;
        }
        // Sobrescreve com tamanho 0.
        FILE *fp = std::fopen(kPath, "w");
        if (fp) std::fclose(fp);
    }

} // namespace seng::log

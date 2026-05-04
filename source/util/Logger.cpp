#include "Logger.hpp"

#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <mutex>

#include <switch.h>

namespace seng::log {

    namespace {
        std::mutex g_mutex;
        FILE      *g_fp = nullptr;

        void ensureOpen_locked() {
            if (g_fp != nullptr) return;
            g_fp = std::fopen(kPath, "a");
        }

        void writeTimestamp_locked() {
            u64 ms = armTicksToNs(armGetSystemTick()) / 1'000'000ULL;
            std::fprintf(g_fp, "[%010llu] ",
                         static_cast<unsigned long long>(ms));
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
        FILE *fp = std::fopen(kPath, "w");
        if (fp) std::fclose(fp);
    }

} // namespace seng::log

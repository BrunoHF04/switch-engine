#include "ResultsStore.hpp"

#include <cstdio>
#include <cstring>
#include <sys/stat.h>

namespace ResultsStore {

    namespace {
        struct Header {
            uint32_t magic;
            uint32_t version;
            uint64_t count;
        };

        // Estado de escrita global. So um scan por vez.
        FILE   *g_writer       = nullptr;
        size_t  g_writeCount   = 0;

        // Buffer de escrita: agrupa Entries antes de fwrite. Reduz drasticamente
        // o overhead de syscalls de fs (cada fwrite no FAT e' caro).
        constexpr size_t kFlushEntries = 256;
        Entry            g_writeBuf[kFlushEntries];
        size_t           g_writeBufLen = 0;

        void flushWriteBuf() {
            if (g_writer && g_writeBufLen > 0) {
                std::fwrite(g_writeBuf, sizeof(Entry), g_writeBufLen, g_writer);
                g_writeBufLen = 0;
            }
        }

        bool readHeader(FILE *fp, Header *out) {
            std::rewind(fp);
            if (std::fread(out, sizeof(Header), 1, fp) != 1) return false;
            return out->magic == kMagic && out->version == kVersion;
        }

        void writeHeader(FILE *fp, uint64_t count) {
            Header h{ kMagic, kVersion, count };
            std::rewind(fp);
            std::fwrite(&h, sizeof(Header), 1, fp);
        }
    } // namespace

    // =========================================================================
    void beginWrite() {
        if (g_writer) {
            std::fclose(g_writer);
            g_writer = nullptr;
        }
        g_writer = std::fopen(kResultsPath, "wb");
        if (!g_writer) return;

        // Reserva header; sera reescrito em endWrite().
        Header h{ kMagic, kVersion, 0 };
        std::fwrite(&h, sizeof(Header), 1, g_writer);

        g_writeCount  = 0;
        g_writeBufLen = 0;
    }

    void pushHit(uint64_t address, uint32_t value) {
        if (!g_writer) return;
        g_writeBuf[g_writeBufLen++] = Entry{ address, value, 0 };
        g_writeCount++;
        if (g_writeBufLen == kFlushEntries) {
            flushWriteBuf();
        }
    }

    void endWrite() {
        if (!g_writer) return;
        flushWriteBuf();
        writeHeader(g_writer, g_writeCount);
        std::fclose(g_writer);
        g_writer     = nullptr;
        g_writeCount = 0;
    }

    // =========================================================================
    bool filterU32(std::function<bool(uint64_t, uint32_t, uint32_t *)> predicate) {
        FILE *src = std::fopen(kResultsPath, "rb");
        if (!src) return false;

        Header srcH{};
        if (!readHeader(src, &srcH)) {
            std::fclose(src);
            return false;
        }

        FILE *dst = std::fopen(kTempPath, "wb");
        if (!dst) {
            std::fclose(src);
            return false;
        }

        Header dstH{ kMagic, kVersion, 0 };
        std::fwrite(&dstH, sizeof(Header), 1, dst);

        constexpr size_t kReadBatch = 256;
        Entry            inBuf[kReadBatch];
        Entry            outBuf[kReadBatch];
        size_t           outLen = 0;
        uint64_t         kept   = 0;

        for (uint64_t i = 0; i < srcH.count; ) {
            const size_t want = (srcH.count - i < kReadBatch) ? (srcH.count - i)
                                                              : kReadBatch;
            const size_t got  = std::fread(inBuf, sizeof(Entry), want, src);
            if (got == 0) break;

            for (size_t j = 0; j < got; j++) {
                uint32_t newVal = inBuf[j].value;
                if (predicate(inBuf[j].address, inBuf[j].value, &newVal)) {
                    outBuf[outLen++] = Entry{ inBuf[j].address, newVal, 0 };
                    kept++;
                    if (outLen == kReadBatch) {
                        std::fwrite(outBuf, sizeof(Entry), outLen, dst);
                        outLen = 0;
                    }
                }
            }
            i += got;
        }

        if (outLen > 0) {
            std::fwrite(outBuf, sizeof(Entry), outLen, dst);
        }

        writeHeader(dst, kept);
        std::fclose(dst);
        std::fclose(src);

        // Substitui o arquivo principal.
        std::remove(kResultsPath);
        std::rename(kTempPath, kResultsPath);
        return true;
    }

    // =========================================================================
    void readPage(size_t pageIndex, size_t pageSize,
                  std::function<void(const Entry &)> cb) {
        FILE *fp = std::fopen(kResultsPath, "rb");
        if (!fp) return;

        Header h{};
        if (!readHeader(fp, &h)) {
            std::fclose(fp);
            return;
        }

        const uint64_t startIdx = static_cast<uint64_t>(pageIndex) * pageSize;
        if (startIdx >= h.count) {
            std::fclose(fp);
            return;
        }

        std::fseek(fp,
                   sizeof(Header) + startIdx * sizeof(Entry),
                   SEEK_SET);

        const size_t want = (startIdx + pageSize > h.count)
                            ? (h.count - startIdx)
                            : pageSize;

        Entry entry{};
        for (size_t i = 0; i < want; i++) {
            if (std::fread(&entry, sizeof(Entry), 1, fp) != 1) break;
            cb(entry);
        }
        std::fclose(fp);
    }

    // =========================================================================
    size_t count() {
        FILE *fp = std::fopen(kResultsPath, "rb");
        if (!fp) return 0;
        Header h{};
        const bool ok = readHeader(fp, &h);
        std::fclose(fp);
        return ok ? static_cast<size_t>(h.count) : 0;
    }

    void reset() {
        std::remove(kResultsPath);
        std::remove(kTempPath);
    }

} // namespace ResultsStore

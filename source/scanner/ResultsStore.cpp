#include "ResultsStore.hpp"

#include <cstdio>
#include <cstring>
#include <sys/stat.h>

namespace ResultsStore {

    namespace {
        FILE   *g_writer     = nullptr;
        size_t  g_writeCount = 0;

        seng::ValueType g_writeType = seng::ValueType::U32;
        seng::CompareOp g_writeOp   = seng::CompareOp::Equal;

        constexpr size_t kFlushEntries = 256;
        Entry            g_writeBuf[kFlushEntries];
        size_t           g_writeBufLen = 0;

        // Cached count (invalidado por endWrite / filterTyped / reset).
        bool   g_countValid  = false;
        size_t g_cachedCount = 0;

        // Cached header metadata.
        seng::ValueType g_cachedType = seng::ValueType::U32;
        seng::CompareOp g_cachedOp   = seng::CompareOp::Equal;

        void flushWriteBuf() {
            if (g_writer && g_writeBufLen > 0) {
                std::fwrite(g_writeBuf, sizeof(Entry), g_writeBufLen, g_writer);
                g_writeBufLen = 0;
            }
        }

        bool readHeader(FILE *fp, Header *out) {
            std::rewind(fp);
            if (std::fread(out, sizeof(Header), 1, fp) != 1) return false;
            return out->magic == seng::resultsbin::kMagic &&
                   out->version == seng::resultsbin::kVersion;
        }

        void writeHeader(FILE *fp, uint64_t count,
                         seng::ValueType type, seng::CompareOp op) {
            Header h{};
            h.magic      = seng::resultsbin::kMagic;
            h.version    = seng::resultsbin::kVersion;
            h.count      = count;
            h.value_type = static_cast<uint8_t>(type);
            h.compare_op = static_cast<uint8_t>(op);
            std::rewind(fp);
            std::fwrite(&h, sizeof(Header), 1, fp);
        }

        void updateCache(size_t cnt, seng::ValueType t, seng::CompareOp o) {
            g_cachedCount = cnt;
            g_countValid  = true;
            g_cachedType  = t;
            g_cachedOp    = o;
        }
    } // namespace

    void beginWrite(seng::ValueType type, seng::CompareOp op) {
        if (g_writer) {
            std::fclose(g_writer);
            g_writer = nullptr;
        }
        g_writer = std::fopen(seng::resultsbin::kResultsPath, "wb");
        if (!g_writer) return;

        g_writeType = type;
        g_writeOp   = op;

        Header h{};
        h.magic      = seng::resultsbin::kMagic;
        h.version    = seng::resultsbin::kVersion;
        h.count      = 0;
        h.value_type = static_cast<uint8_t>(type);
        h.compare_op = static_cast<uint8_t>(op);
        std::fwrite(&h, sizeof(Header), 1, g_writer);

        g_writeCount  = 0;
        g_writeBufLen = 0;
    }

    void pushHit(uint64_t address, uint64_t raw_value) {
        if (!g_writer) return;
        g_writeBuf[g_writeBufLen++] = Entry{ address, raw_value };
        g_writeCount++;
        if (g_writeBufLen == kFlushEntries) {
            flushWriteBuf();
        }
    }

    void endWrite() {
        if (!g_writer) return;
        flushWriteBuf();
        writeHeader(g_writer, g_writeCount, g_writeType, g_writeOp);
        std::fclose(g_writer);
        g_writer = nullptr;
        updateCache(g_writeCount, g_writeType, g_writeOp);
        g_writeCount = 0;
    }

    bool filterTyped(seng::ValueType type, seng::CompareOp op,
                     uint64_t /*searchVal*/, uint64_t /*searchVal2*/,
                     std::function<bool(uint64_t, uint64_t, uint64_t *)> readCurrent) {
        FILE *src = std::fopen(seng::resultsbin::kResultsPath, "rb");
        if (!src) return false;

        Header srcH{};
        if (!readHeader(src, &srcH)) {
            std::fclose(src);
            return false;
        }

        FILE *dst = std::fopen(seng::resultsbin::kTempPath, "wb");
        if (!dst) {
            std::fclose(src);
            return false;
        }

        Header dstH{};
        dstH.magic      = seng::resultsbin::kMagic;
        dstH.version    = seng::resultsbin::kVersion;
        dstH.count      = 0;
        dstH.value_type = static_cast<uint8_t>(type);
        dstH.compare_op = static_cast<uint8_t>(op);
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
                uint64_t newRaw = inBuf[j].raw_value;
                if (readCurrent(inBuf[j].address, inBuf[j].raw_value, &newRaw)) {
                    outBuf[outLen++] = Entry{ inBuf[j].address, newRaw };
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

        writeHeader(dst, kept, type, op);
        std::fclose(dst);
        std::fclose(src);

        std::remove(seng::resultsbin::kResultsPath);
        std::rename(seng::resultsbin::kTempPath, seng::resultsbin::kResultsPath);

        updateCache(static_cast<size_t>(kept), type, op);
        return true;
    }

    void readPage(size_t pageIndex, size_t pageSize,
                  std::function<void(const Entry &)> cb) {
        FILE *fp = std::fopen(seng::resultsbin::kResultsPath, "rb");
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

    size_t count() {
        if (g_countValid) return g_cachedCount;

        FILE *fp = std::fopen(seng::resultsbin::kResultsPath, "rb");
        if (!fp) {
            g_cachedCount = 0;
            g_countValid  = true;
            return 0;
        }
        Header h{};
        const bool ok = readHeader(fp, &h);
        std::fclose(fp);

        g_cachedCount = ok ? static_cast<size_t>(h.count) : 0;
        if (ok) {
            g_cachedType = static_cast<seng::ValueType>(h.value_type);
            g_cachedOp   = static_cast<seng::CompareOp>(h.compare_op);
        }
        g_countValid = true;
        return g_cachedCount;
    }

    void invalidateCache() {
        g_countValid = false;
    }

    void reset() {
        std::remove(seng::resultsbin::kResultsPath);
        std::remove(seng::resultsbin::kTempPath);
        g_cachedCount = 0;
        g_countValid  = true;
        g_cachedType  = seng::ValueType::U32;
        g_cachedOp    = seng::CompareOp::Equal;
    }

    seng::ValueType currentValueType() {
        count(); // ensure cache is loaded
        return g_cachedType;
    }

    seng::CompareOp currentCompareOp() {
        count();
        return g_cachedOp;
    }

} // namespace ResultsStore

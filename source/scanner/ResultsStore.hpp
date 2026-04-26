#pragma once

#include <cstdint>
#include <cstddef>
#include <functional>

/**
 * ResultsStore
 *
 * Persistencia paginada de hits no SD card. Por que arquivo, e nao std::vector?
 *   - Heap do overlay e' minusculo (~6MB).
 *   - Um First Scan tipico em jogo grande gera milhoes de matches (60+ MB
 *     em vector<uint64_t>). Inviavel.
 *
 * Layout binario simples (little-endian, fixo):
 *
 *   struct Header { uint32_t magic; uint32_t version; uint64_t count; };
 *   struct Entry  { uint64_t address; uint32_t value; uint32_t _pad; };
 *
 * Operacoes:
 *   beginWrite() / pushHit() / endWrite()  -> usado pelo first scan
 *   filterU32(predicado)                   -> next scan (le -> filtra -> escreve novo arquivo)
 *   readPage(idx, n, callback)             -> usado pela GUI para mostrar hits
 *   reset()                                -> apaga results.bin
 *   count()                                -> retorna o header.count atual
 */
namespace ResultsStore {

    constexpr const char *kResultsPath = "sdmc:/switch/switch-engine/results.bin";
    constexpr const char *kTempPath    = "sdmc:/switch/switch-engine/results.tmp";
    constexpr uint32_t    kMagic       = 0x53454E47; // 'SENG'
    constexpr uint32_t    kVersion     = 1;

    struct Entry {
        uint64_t address;
        uint32_t value;
        uint32_t _pad;
    };

    void   beginWrite();
    void   pushHit(uint64_t address, uint32_t value);
    void   endWrite();

    bool   filterU32(std::function<bool(uint64_t addr,
                                        uint32_t oldVal,
                                        uint32_t *newVal)> predicate);

    void   readPage(size_t pageIndex, size_t pageSize,
                    std::function<void(const Entry &)> cb);

    size_t count();
    void   reset();

} // namespace ResultsStore

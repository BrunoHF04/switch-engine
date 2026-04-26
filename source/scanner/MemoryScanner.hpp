#pragma once

#include <switch.h>
#include <cstdint>
#include <functional>

/**
 * MemoryScanner
 *
 * Motor de busca usando SVCs de debug (svcQueryDebugProcessMemory +
 * svcReadDebugProcessMemory).
 *
 * Restricoes de RAM:
 *   - Tesla overlays/sysmods rodam com heap pequeno (~6 MB total).
 *   - Por isso lemos a memoria do jogo em CHUNKS de SCAN_BUFFER_SIZE
 *     e gravamos imediatamente os hits no SD via ResultsStore (paginado).
 *   - NUNCA mantemos a lista de resultados inteira em memoria.
 *
 * Uso tipico:
 *   scanner.setTargetPid(pid);
 *   scanner.firstScanU32(100);   // popula sdmc:/.../results.bin
 *   scanner.nextScanU32(100);    // re-filtra os hits, escreve novo arquivo
 */
class MemoryScanner {
public:
    MemoryScanner();
    ~MemoryScanner();

    void     setTargetPid(uint64_t pid) { m_targetPid = pid; }
    uint64_t getTargetPid() const       { return m_targetPid; }

    // Helpers tipados. Internamente delegam para o template scanInternal.
    Result firstScanU32(uint32_t value);
    Result nextScanU32(uint32_t value);

    // Tamanho do chunk lido por iteracao do svcReadDebugProcessMemory.
    // 64KB = bom equilibrio entre numero de syscalls e uso de stack/heap.
    static constexpr size_t kScanBufferSize = 0x10000;

private:
    uint64_t m_targetPid = 0;

    // Itera sobre todas as regioes mapeadas do processo alvo, chamando cb com
    // cada MemoryInfo cuja permissao seja R/W.
    Result iterateRwRegions(Handle debugHandle,
                            std::function<Result(const MemoryInfo &)> cb);

    // Le um intervalo [addr, addr+size) em chunks, chamando cb para cada chunk.
    Result readInChunks(Handle      debugHandle,
                        uint64_t    addr,
                        uint64_t    size,
                        std::function<Result(uint64_t baseAddr,
                                             const uint8_t *buf,
                                             size_t bufSize)> cb);
};

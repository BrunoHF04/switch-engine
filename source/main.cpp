/**
 * Switch Engine - Memory Scanner Overlay
 *
 * Entry point. Mantemos este arquivo o mais enxuto possivel:
 * - Define TESLA_INIT_IMPL UMA UNICA VEZ no projeto inteiro (regra do libtesla).
 *   Essa macro instancia __appInit, __appExit, layer config e demais
 *   simbolos que o nx-ovlloader exige para reconhecer o NRO como overlay.
 * - Monta sdmc: ANTES de qualquer fopen/mkdir (libtesla NAO monta o SD em
 *   __appInit, so' faz fsInitialize() -- sem fsdevMountSdmc o devoptab nao
 *   conhece o prefixo "sdmc:/" e mkdir crasha com data abort).
 * - Chama tsl::loop com a nossa classe de Overlay raiz.
 *
 * Logging (estrategia de boot tracing):
 *   - sdmc:/switch-engine_debug.log: tracer bullets via fopen() cru. Cada linha
 *     que aparecer aqui significa que o NRO executou ate aquele ponto vivo.
 *   - sdmc:/switch-engine.log: log estruturado (Logger.hpp).
 *
 * Se NENHUM dos dois aparecer no SD apos voce abrir o Tesla, o overlay
 * crashou antes do main() conseguir montar o SD.
 */
#define TESLA_INIT_IMPL
#include <tesla.hpp>

#include "Overlay.hpp"
#include "util/Logger.hpp"

#include <switch.h>
#include <cstdio>

namespace {
    constexpr const char *kDebugPath = "sdmc:/switch-engine_debug.log";

    void rawDebugMark(const char *msg) {
        FILE *fp = std::fopen(kDebugPath, "a");
        if (!fp) return;
        std::fputs(msg, fp);
        std::fputc('\n', fp);
        std::fflush(fp);
        std::fclose(fp);
    }
} // namespace

int main(int argc, char **argv) {
    // CRITICO: libtesla nao chama fsdevMountSdmc() em seu __appInit override.
    // Sem isso, qualquer fopen("sdmc:/...") retorna NULL silenciosamente e
    // qualquer mkdir("sdmc:/...") crasha (null deref dentro do newlib).
    // Tem que ser a primeira coisa do main(), antes de qualquer log.
    Result mountRc = fsdevMountSdmc();

    {
        char buf[80];
        std::snprintf(buf, sizeof(buf), "[debug] main entry; fsdevMountSdmc=0x%08X", mountRc);
        rawDebugMark(buf);
    }

    if (R_FAILED(mountRc)) {
        // Se nao conseguiu montar, nao adianta seguir -- toda I/O vai falhar.
        // Tenta logar via stdout (pode nao chegar a lugar nenhum) e sai limpo.
        return mountRc;
    }

    seng::log::write("[main] entry argc=%d argv0=%s", argc,
                     (argc > 0 && argv && argv[0]) ? argv[0] : "(null)");

    int rc = tsl::loop<SwitchEngineOverlay>(argc, argv);

    {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "[debug] main exit rc=%d", rc);
        rawDebugMark(buf);
    }
    seng::log::write("[main] tsl::loop returned %d", rc);
    seng::log::close();

    fsdevUnmountAll();
    return rc;
}

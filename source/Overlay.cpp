#include "Overlay.hpp"

#include "gui/MainGui.hpp"
#include "scanner/SengClient.hpp"
#include "util/Logger.hpp"

#include <switch.h>
#include <sys/stat.h>

#include <cstdio>

namespace {
    constexpr const char *kBaseDir   = "sdmc:/switch/switch-engine";
    constexpr const char *kDebugPath = "sdmc:/switch-engine_debug.log";

    void ensureStorageDirs() {
        // Idempotente: se ja existem, mkdir falha com EEXIST (ignoramos).
        // A chamada e' segura porque main() ja fez fsdevMountSdmc().
        mkdir("sdmc:/switch", 0777);
        mkdir(kBaseDir, 0777);
    }

    // Escrita de debug "burra": fopen/fputs/fclose. Sem locks, sem state
    // global, sem dependencias do nosso codigo. Se o NSO chega aqui, o
    // arquivo aparece no SD -- mesmo que tudo crashe na linha seguinte.
    // Usar somente em pontos onde queremos "tracer bullets" de boot.
    void rawDebugMark(const char *msg) {
        FILE *fp = std::fopen(kDebugPath, "a");
        if (!fp) return;
        std::fputs(msg, fp);
        std::fputc('\n', fp);
        std::fflush(fp);
        std::fclose(fp);
    }
} // namespace

SwitchEngineOverlay::SwitchEngineOverlay() {
    // ATENCAO: este e' o primeiro ponto do nosso codigo que executa apos
    // o nx-ovlloader+ instanciar a classe via tsl::loop<>. Se este arquivo
    // de debug NAO aparecer em sdmc:/switch-engine_debug.log, significa
    // que o ovlloader sequer chegou aqui (problema de loader/specs/cache).
    rawDebugMark("[debug] Overlay Iniciado (ctor)");
    seng::log::write("[overlay] ctor");
}

SwitchEngineOverlay::~SwitchEngineOverlay() {
    rawDebugMark("[debug] Overlay finalizado (dtor)");
    seng::log::write("[overlay] dtor");
}

void SwitchEngineOverlay::initServices() {
    rawDebugMark("[debug] initServices begin");
    seng::log::write("[overlay] initServices begin");

    ensureStorageDirs();

    // Conexao com switch-engine-mod (servico "seng"). Se o sysmod nao estiver
    // instalado/ligado, initialize falha silenciosamente -- a UI mostra o erro
    // quando o usuario tentar uma operacao.
    Result rc = SengClient::initialize();
    if (R_FAILED(rc)) {
        seng::log::write("[overlay] SengClient::initialize FAILED rc=0x%08X", rc);
    } else {
        seng::log::write("[overlay] SengClient::initialize OK");
    }

    rawDebugMark("[debug] initServices end");
    seng::log::write("[overlay] initServices end");
}

void SwitchEngineOverlay::exitServices() {
    seng::log::write("[overlay] exitServices");
    SengClient::finalize();
}

void SwitchEngineOverlay::onShow() {
    rawDebugMark("[debug] onShow");
    seng::log::write("[overlay] onShow");
}

void SwitchEngineOverlay::onHide() {
    seng::log::write("[overlay] onHide");
}

std::unique_ptr<tsl::Gui> SwitchEngineOverlay::loadInitialGui() {
    rawDebugMark("[debug] loadInitialGui -> MainGui");
    seng::log::write("[overlay] loadInitialGui -> MainGui");
    return initially<MainGui>();
}

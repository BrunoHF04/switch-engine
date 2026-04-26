#include "Overlay.hpp"

#include "gui/MainGui.hpp"
#include "scanner/SengClient.hpp"
#include "util/Logger.hpp"

#include <switch.h>
#include <sys/stat.h>

namespace {
    constexpr const char *kBaseDir = "sdmc:/switch/switch-engine";

    void ensureStorageDirs() {
        mkdir("sdmc:/switch", 0777);
        mkdir(kBaseDir, 0777);
    }
} // namespace

SwitchEngineOverlay::SwitchEngineOverlay() {
    seng::log::write("[overlay] ctor");
}

SwitchEngineOverlay::~SwitchEngineOverlay() {
    seng::log::write("[overlay] dtor");
}

void SwitchEngineOverlay::initServices() {
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

    seng::log::write("[overlay] initServices end");
}

void SwitchEngineOverlay::exitServices() {
    seng::log::write("[overlay] exitServices");
    SengClient::finalize();
}

void SwitchEngineOverlay::onShow() {
    seng::log::write("[overlay] onShow");
}

void SwitchEngineOverlay::onHide() {
    seng::log::write("[overlay] onHide");
}

std::unique_ptr<tsl::Gui> SwitchEngineOverlay::loadInitialGui() {
    seng::log::write("[overlay] loadInitialGui -> MainGui");
    return initially<MainGui>();
}

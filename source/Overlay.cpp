#include "Overlay.hpp"
#include "gui/MainGui.hpp"
#include "scanner/SengClient.hpp"

#include <switch.h>
#include <sys/stat.h>

namespace {
    constexpr const char *kBaseDir = "sdmc:/switch/switch-engine";

    void ensureStorageDirs() {
        mkdir("sdmc:/switch", 0777);
        mkdir(kBaseDir, 0777);
    }
} // namespace

void SwitchEngineOverlay::initServices() {
    ensureStorageDirs();
    // Conexao com switch-engine-mod (servico "seng"). Se o sysmod nao estiver
    // instalado/ligado, initialize falha silenciosamente -- a UI mostra o erro
    // quando o usuario tentar uma operacao.
    SengClient::initialize();
}

void SwitchEngineOverlay::exitServices() {
    SengClient::finalize();
}

void SwitchEngineOverlay::onShow() {}
void SwitchEngineOverlay::onHide() {}

std::unique_ptr<tsl::Gui> SwitchEngineOverlay::loadInitialGui() {
    return initially<MainGui>();
}

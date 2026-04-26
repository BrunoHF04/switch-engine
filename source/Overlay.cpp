#include "Overlay.hpp"
#include "gui/MainGui.hpp"

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

    // Servicos minimos. Outras inicializacoes (ldr:dmnt, pm:shell) ficam dentro
    // do MemoryScanner para nao segurar handles enquanto o overlay esta ocioso.
    pmdmntInitialize();
    pminfoInitialize();
}

void SwitchEngineOverlay::exitServices() {
    pminfoExit();
    pmdmntExit();
}

void SwitchEngineOverlay::onShow() {}
void SwitchEngineOverlay::onHide() {}

std::unique_ptr<tsl::Gui> SwitchEngineOverlay::loadInitialGui() {
    return initially<MainGui>();
}

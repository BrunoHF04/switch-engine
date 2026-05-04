#include "Overlay.hpp"

#include "gui/MainGui.hpp"
#include "gui/SysmodMissingGui.hpp"
#include "scanner/SengClient.hpp"
#include "seng_ipc.hpp"
#include "util/Logger.hpp"

#include <switch.h>
#include <sys/stat.h>

#include <cinttypes>
#include <cstdio>

namespace {
    constexpr const char *kBaseDir = "sdmc:/switch/switch-engine";

    void ensureStorageDirs() {
        mkdir("sdmc:/switch", 0777);
        mkdir(kBaseDir, 0777);
    }
}

SwitchEngineOverlay::SwitchEngineOverlay() {
    seng::log::write("INFO [overlay] ctor");
}

SwitchEngineOverlay::~SwitchEngineOverlay() {
    seng::log::write("INFO [overlay] dtor");
}

void SwitchEngineOverlay::initServices() {
    m_sengReady = false;
    ensureStorageDirs();

    seng::log::write("INFO [overlay] initServices begin, expected TID 0x%016" PRIx64,
                     static_cast<unsigned long long>(seng::kSysmodTitleId));

    constexpr u64 kSengConnectMaxNs = 2'000'000'000ULL;
    Result rc = SengClient::initializeTimed(kSengConnectMaxNs);
    m_sengReady = R_SUCCEEDED(rc);

    seng::log::write("INFO [overlay] seng probe rc=0x%08X ready=%d", rc, m_sengReady ? 1 : 0);

    if (m_sengReady) {
        // Verificacao de versao IPC: detecta mismatch overlay <-> sysmod.
        uint32_t sysmodVer = 0;
        Result vrc = SengClient::getVersion(&sysmodVer);
        if (R_SUCCEEDED(vrc) && sysmodVer != seng::kIpcVersion) {
            seng::log::write("WARN [overlay] version mismatch: sysmod=%u overlay=%u",
                             sysmodVer, seng::kIpcVersion);
            m_versionMismatch = true;
            m_sysmodVersion   = sysmodVer;
        }
    }

    seng::log::write("INFO [overlay] initServices end");
}

void SwitchEngineOverlay::exitServices() {
    seng::log::write("INFO [overlay] exitServices");
    SengClient::finalize();
}

void SwitchEngineOverlay::onShow() {
    seng::log::write("INFO [overlay] onShow");
}

void SwitchEngineOverlay::onHide() {
    seng::log::write("INFO [overlay] onHide");
}

std::unique_ptr<tsl::Gui> SwitchEngineOverlay::loadInitialGui() {
    if (!m_sengReady) {
        seng::log::write("INFO [overlay] loadInitialGui -> SysmodMissingGui");
        return initially<SysmodMissingGui>();
    }
    seng::log::write("INFO [overlay] loadInitialGui -> MainGui");
    return initially<MainGui>();
}

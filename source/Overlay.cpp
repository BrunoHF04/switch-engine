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
    m_sengReady = false;

    // Logs em SD so depois de mkdir: reduz risco de I/O antes de paths existirem.
    ensureStorageDirs();
    rawDebugMark("[debug] initServices after ensureStorageDirs");

    {
        char tidbuf[96];
        std::snprintf(
            tidbuf, sizeof(tidbuf),
            "[debug] expected sysmod TID 0x%016" PRIx64 " (seng)",
            static_cast<unsigned long long>(seng::kSysmodTitleId));
        rawDebugMark(tidbuf);
    }

    // Ate ~2 s tentando "seng" (evita bloqueio indefinido se SM/sysmod atrasar).
    constexpr u64 kSengConnectMaxNs = 2'000'000'000ULL;
    Result rc = SengClient::initializeTimed(kSengConnectMaxNs);
    m_sengReady = R_SUCCEEDED(rc);

    seng::log::write("[overlay] initServices seng probe rc=0x%08X ready=%d",
                     rc, m_sengReady ? 1 : 0);
    if (R_FAILED(rc)) {
        seng::log::write("[overlay] sysmod unreachable (service seng). TID esperado=0x%016" PRIx64,
                         static_cast<unsigned long long>(seng::kSysmodTitleId));
    }

    rawDebugMark("[debug] initServices passed seng probe");
    seng::log::write("[overlay] initServices end (pos-probe seng)");
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
    if (!m_sengReady) {
        rawDebugMark("[debug] loadInitialGui -> SysmodMissingGui");
        seng::log::write("[overlay] loadInitialGui -> SysmodMissingGui");
        return initially<SysmodMissingGui>();
    }
    rawDebugMark("[debug] loadInitialGui -> MainGui");
    seng::log::write("[overlay] loadInitialGui -> MainGui");
    return initially<MainGui>();
}

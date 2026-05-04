/**
 * Switch Engine - Memory Scanner Overlay
 *
 * Entry point. Define TESLA_INIT_IMPL, monta sdmc, carrega i18n e entra
 * no loop do libtesla.
 */
#define TESLA_INIT_IMPL
#include <tesla.hpp>

#include "Overlay.hpp"
#include "util/Language.hpp"
#include "util/Logger.hpp"

#include <switch.h>
#include <cstdio>

int main(int argc, char **argv) {
    Result mountRc = fsdevMountSdmc();

    if (R_FAILED(mountRc)) {
        return mountRc;
    }

    seng::log::write("INFO [main] entry argc=%d fsdevMountSdmc=0x%08X",
                     argc, mountRc);

    seng::i18n::load();
    seng::log::write("INFO [main] i18n loaded lang=%s", seng::i18n::currentCode());

    int rc = tsl::loop<SwitchEngineOverlay>(argc, argv);

    seng::log::write("INFO [main] tsl::loop returned %d", rc);
    seng::log::close();

    fsdevUnmountAll();
    return rc;
}

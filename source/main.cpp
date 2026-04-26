/**
 * Switch Engine - Memory Scanner Overlay
 *
 * Entry point. Mantemos este arquivo o mais enxuto possivel:
 * - Define TESLA_INIT_IMPL UMA UNICA VEZ no projeto inteiro (regra do libtesla).
 * - Chama tesla::loop com a nossa classe de Overlay raiz.
 */
#define TESLA_INIT_IMPL
#include <tesla.hpp>

#include "Overlay.hpp"

int main(int argc, char **argv) {
    return tsl::loop<SwitchEngineOverlay>(argc, argv);
}

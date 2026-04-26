/**
 * Switch Engine - Memory Scanner Overlay
 *
 * Entry point. Mantemos este arquivo o mais enxuto possivel:
 * - Define TESLA_INIT_IMPL UMA UNICA VEZ no projeto inteiro (regra do libtesla).
 *   Essa macro instancia __appInit, __appExit, layer config e demais
 *   simbolos que o nx-ovlloader exige para reconhecer o NSO como overlay.
 * - Chama tsl::loop com a nossa classe de Overlay raiz.
 *
 * Logging: gravamos uma linha em sdmc:/switch-engine.log assim que o NSO
 * comeca a executar. Se voce nao ver esse arquivo aparecer no SD ao abrir
 * o overlay, o NSO sequer foi carregado pelo ovlloader (problema de path
 * ou cache do menu).
 */
#define TESLA_INIT_IMPL
#include <tesla.hpp>

#include "Overlay.hpp"
#include "util/Logger.hpp"

int main(int argc, char **argv) {
    seng::log::write("[main] entry argc=%d argv0=%s", argc,
                     (argc > 0 && argv && argv[0]) ? argv[0] : "(null)");

    int rc = tsl::loop<SwitchEngineOverlay>(argc, argv);

    seng::log::write("[main] tsl::loop returned %d", rc);
    seng::log::close();
    return rc;
}

#pragma once

/**
 * SwitchEngineOverlay
 *
 * Classe raiz do overlay. Responsavel por:
 *  - Inicializar/desinicializar servicos do sistema (sm, pm:dmnt, fs, hid).
 *  - Garantir que a pasta sdmc:/switch/switch-engine exista.
 *  - Carregar a GUI inicial (MainGui).
 *
 * Nao inicializamos servicos pesados aqui (ldr:dmnt, debug handles): isso fica
 * sob demanda dentro do MemoryScanner para nao manter o jogo "attachado" sem
 * necessidade.
 */

#include <tesla.hpp>
#include <memory>

class SwitchEngineOverlay : public tsl::Overlay {
public:
    SwitchEngineOverlay();
    ~SwitchEngineOverlay() override;

    void initServices() override;
    void exitServices() override;

    void onShow() override;
    void onHide() override;

    std::unique_ptr<tsl::Gui> loadInitialGui() override;
};

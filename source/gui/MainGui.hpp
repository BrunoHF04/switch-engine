#pragma once

#include <tesla.hpp>
#include <memory>

#include "../scanner/MemoryScanner.hpp"

/**
 * MainGui
 *
 * Tela inicial do overlay. Mostra:
 *  - PID/titleId do processo em primeiro plano (com botao para re-detectar).
 *  - Acoes de scan (First Scan, Next Scan, Reset).
 *  - Pagina atual de resultados (lida do results.bin sob demanda).
 *
 * O scanner em si e propriedade da MainGui mas as operacoes de scan sao
 * disparadas em outra thread no futuro (TODO) para nao travar o redraw do
 * Tesla. Por enquanto, executamos sincrono e mostramos um spinner.
 */
class MainGui : public tsl::Gui {
public:
    MainGui();
    ~MainGui() override = default;

    tsl::elm::Element *createUI() override;
    void               update() override;
    bool               handleInput(u64 keysDown, u64 keysHeld,
                                   const HidTouchState &touchPos,
                                   HidAnalogStickState  joyStickPosLeft,
                                   HidAnalogStickState  joyStickPosRight) override;

private:
    std::unique_ptr<MemoryScanner> m_scanner;

    tsl::elm::List         *m_list           = nullptr;
    tsl::elm::ListItem     *m_targetItem     = nullptr;
    tsl::elm::ListItem     *m_resultCountItem = nullptr;

    // Estado da UI
    uint32_t m_searchValue = 0;
    size_t   m_currentPage = 0;
    static constexpr size_t kPageSize = 32;

    void rebuildResultsSection();
    void onDetectTarget();
    void onFirstScan();
    void onNextScan();
    void onResetScan();
};

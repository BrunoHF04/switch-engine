#pragma once

#include <tesla.hpp>

#include <atomic>
#include <memory>
#include <string>

#include "../scanner/MemoryScanner.hpp"

/**
 * MainGui
 *
 * Tela inicial do overlay. Compoe a UI com:
 *   - "Target Process": detecta o jogo em foreground (PID + TitleID).
 *   - "Search value": abre o NumericInputGui para digitar o valor procurado.
 *   - "First Scan", "Next Scan", "Reset": acoes do scanner via SengClient.
 *   - "Show Results": abre o ResultsListGui (com poke ao clicar num hit).
 *   - "Status": ultima mensagem (resultado / erro).
 *
 * Os scans rodam SINCRONOS no thread do click handler. O Tesla mantem o
 * draw em outra thread, entao a UI continua animando, mas inputs ficam
 * presos durante o scan. (TODO: thread separada para nao travar inputs.)
 */
class MainGui : public tsl::Gui {
public:
    MainGui();
    ~MainGui() override = default;

    tsl::elm::Element *createUI() override;
    void               update() override;

private:
    std::unique_ptr<MemoryScanner> m_scanner;

    tsl::elm::ListItem *m_targetItem  = nullptr;
    tsl::elm::ListItem *m_valueItem   = nullptr;
    tsl::elm::ListItem *m_resultsItem = nullptr;
    tsl::elm::ListItem *m_statusItem  = nullptr;

    uint64_t    m_titleId      = 0;
    uint32_t    m_searchValue  = 0;
    std::string m_status       = "(idle)";

    void onDetectTarget();
    void onPickProcess();
    void onProcessPicked(uint64_t pid, uint64_t tid);
    void onPickSearchValue();
    void onFirstScan();
    void onNextScan();
    void onResetScan();
    void onShowResults();
};

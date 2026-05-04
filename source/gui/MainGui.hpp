#pragma once

#include <tesla.hpp>

#include <atomic>
#include <memory>
#include <string>
#include <thread>

#include "../scanner/MemoryScanner.hpp"
#include "seng_ipc.hpp"

/**
 * MainGui
 *
 * Tela inicial do overlay com suporte a:
 *   - Selecao de tipo (u8..f64) e comparador (==, !=, >, <, etc.)
 *   - Scan assincrono em thread separada (UI nao trava)
 *   - Freeze, undo poke, export cheats (delegados a ResultsListGui)
 */
class MainGui : public tsl::Gui {
public:
    MainGui();
    ~MainGui() override;

    tsl::elm::Element *createUI() override;
    void               update() override;

private:
    std::unique_ptr<MemoryScanner> m_scanner;

    tsl::elm::ListItem *m_targetItem   = nullptr;
    tsl::elm::ListItem *m_valueItem    = nullptr;
    tsl::elm::ListItem *m_typeItem     = nullptr;
    tsl::elm::ListItem *m_compareItem  = nullptr;
    tsl::elm::ListItem *m_resultsItem  = nullptr;
    tsl::elm::ListItem *m_statusItem   = nullptr;

    uint64_t         m_titleId      = 0;
    uint64_t         m_searchValue  = 0;
    uint64_t         m_searchValue2 = 0;
    seng::ValueType  m_valueType    = seng::ValueType::U32;
    seng::CompareOp  m_compareOp    = seng::CompareOp::Equal;
    std::string      m_status;

    // Scan assincrono
    std::atomic<bool> m_scanning{false};
    std::string       m_pendingStatus;
    std::thread       m_scanThread;

    void onDetectTarget();
    void onPickProcess();
    void onProcessPicked(uint64_t pid, uint64_t tid);
    void onPickSearchValue();
    void onCycleValueType();
    void onCycleCompareOp();
    void onFirstScan();
    void onNextScan();
    void onResetScan();
    void onShowResults();
};

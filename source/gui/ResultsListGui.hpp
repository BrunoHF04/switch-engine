#pragma once

#include <tesla.hpp>

#include <cstdint>
#include <vector>

/**
 * ResultsListGui
 *
 * Mostra hits paginados com suporte a:
 *   - Poke com undo (guarda ultimo valor antes de escrever)
 *   - Freeze (congela endereco via FreezeManager no sysmod)
 *   - Export cheats no formato Atmosphere
 */
class ResultsListGui : public tsl::Gui {
public:
    static constexpr size_t kPageSize = 24;
    static constexpr size_t kUndoStackSize = 32;

    explicit ResultsListGui(uint64_t targetPid, uint64_t titleId,
                            size_t initialPage = 0);

    tsl::elm::Element *createUI() override;
    void               update() override;

private:
    uint64_t        m_targetPid;
    uint64_t        m_titleId;
    size_t          m_currentPage;
    size_t          m_total            = 0;
    size_t          m_lastBuiltCount   = 0;
    bool            m_pendingFocus     = false;

    tsl::elm::List *m_list = nullptr;

    struct UndoEntry {
        uint64_t addr;
        uint64_t oldValue;
        size_t   valueSize;
    };
    std::vector<UndoEntry> m_undoStack;

    void rebuild();
    void doPoke(uint64_t addr, uint64_t value);
    void doUndo();
    void doFreeze(uint64_t addr, uint64_t value);
    void doExportCheats();
};

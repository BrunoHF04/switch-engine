#pragma once

#include <tesla.hpp>

#include <cstdint>

/**
 * ResultsListGui
 *
 * Mostra os hits salvos em sdmc:/switch/switch-engine/results.bin de forma
 * paginada (kPageSize por pagina). Clicar num hit abre o NumericInputGui com
 * "poke" -- o valor digitado e' escrito no endereco via SengClient::writeMemory.
 *
 * A pid alvo e' passada do MainGui para que possamos garantir que o sysmod
 * esteja attached antes de cada poke (caso o usuario abra essa tela sem ter
 * rodado um scan na sessao atual).
 *
 * Pagina atual e' rebuildada in-place com List::clear()+addItem (libtesla
 * defere as duas operacoes para o proximo draw, sem use-after-free).
 */
class ResultsListGui : public tsl::Gui {
public:
    static constexpr size_t kPageSize = 24;

    explicit ResultsListGui(uint64_t targetPid, size_t initialPage = 0);

    tsl::elm::Element *createUI() override;
    void               update() override;

private:
    uint64_t        m_targetPid;
    size_t          m_currentPage;
    size_t          m_total            = 0;
    size_t          m_lastBuiltCount   = 0;
    bool            m_pendingFocus     = false;

    tsl::elm::List *m_list = nullptr;

    void rebuild();
};

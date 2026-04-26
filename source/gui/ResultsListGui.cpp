#include "ResultsListGui.hpp"

#include "NumericInputGui.hpp"
#include "../scanner/ResultsStore.hpp"
#include "../scanner/SengClient.hpp"

#include <cinttypes>
#include <cstdio>

ResultsListGui::ResultsListGui(uint64_t targetPid, size_t initialPage)
    : m_targetPid(targetPid), m_currentPage(initialPage) {}

tsl::elm::Element *ResultsListGui::createUI() {
    auto *frame = new tsl::elm::OverlayFrame("Switch Engine", "Results");
    m_list = new tsl::elm::List();
    rebuild();
    frame->setContent(m_list);
    return frame;
}

void ResultsListGui::update() {
    // Se o numero total mudou (ex: o usuario voltou a fazer scan ainda nesse
    // overlay), rebuilda automaticamente.
    const size_t cur = ResultsStore::count();
    if (cur != m_lastBuiltCount) {
        // Re-clamp pagina se diminuiu.
        const size_t totalPages = (cur + kPageSize - 1) / kPageSize;
        if (m_currentPage >= totalPages && totalPages > 0)
            m_currentPage = totalPages - 1;
        if (totalPages == 0)
            m_currentPage = 0;

        this->removeFocus();
        m_list->clear();
        rebuild();
        m_pendingFocus = true;
    }

    // Quando o List acaba de aplicar clear()+itemsToAdd no proximo draw,
    // requestFocus() falha (returns nullptr). Tentamos novamente todo frame
    // ate' ser aceito, sem causar mais shake (passamos shake=false).
    if (m_pendingFocus && this->getFocusedElement() == nullptr) {
        this->requestFocus(m_list, tsl::FocusDirection::None, false);
        if (this->getFocusedElement() != nullptr) {
            m_pendingFocus = false;
        }
    }
}

void ResultsListGui::rebuild() {
    m_total          = ResultsStore::count();
    m_lastBuiltCount = m_total;

    const size_t totalPages = m_total == 0 ? 1
                                           : (m_total + kPageSize - 1) / kPageSize;

    char hdrBuf[64];
    std::snprintf(hdrBuf, sizeof(hdrBuf),
                  "%zu hits | page %zu/%zu",
                  m_total, m_currentPage + 1, totalPages);
    m_list->addItem(new tsl::elm::CategoryHeader(hdrBuf));

    if (m_total == 0) {
        m_list->addItem(new tsl::elm::ListItem("(no results)",
                                               "run First Scan"));
        return;
    }

    // -------------------------------------------------------------------
    // Hits da pagina atual.
    // -------------------------------------------------------------------
    const uint64_t pidCaptured = m_targetPid;
    ResultsStore::readPage(m_currentPage, kPageSize,
        [this, pidCaptured](const ResultsStore::Entry &e) {
            char addrBuf[24];
            char valBuf[16];
            std::snprintf(addrBuf, sizeof(addrBuf), "0x%010" PRIx64, e.address);
            std::snprintf(valBuf,  sizeof(valBuf),  "%" PRIu32,      e.value);

            auto *it = new tsl::elm::ListItem(addrBuf, valBuf);
            const uint64_t addr = e.address;

            it->setClickListener([addr, pidCaptured](u64 keys) {
                if (!(keys & HidNpadButton_A)) return false;

                tsl::changeTo<NumericInputGui>(
                    "Poke value (uint32)",
                    static_cast<uint64_t>(0),
                    static_cast<uint64_t>(UINT32_MAX),
                    [addr, pidCaptured](uint64_t v) {
                        // Garante attach (no-op se ja' attached na mesma pid).
                        if (pidCaptured != 0) {
                            (void)SengClient::attach(pidCaptured);
                        }
                        const uint32_t value = static_cast<uint32_t>(v);
                        size_t wrote = 0;
                        (void)SengClient::writeMemory(addr, &value,
                                                      sizeof(value), &wrote);
                    });
                return true;
            });
            m_list->addItem(it);
        });

    // -------------------------------------------------------------------
    // Navegacao entre paginas (rebuild in-place).
    // -------------------------------------------------------------------
    if (totalPages > 1) {
        m_list->addItem(new tsl::elm::CategoryHeader("Navigation"));

        if (m_currentPage > 0) {
            auto *prev = new tsl::elm::ListItem("Previous Page");
            prev->setClickListener([this](u64 keys) {
                if (!(keys & HidNpadButton_A)) return false;
                if (m_currentPage == 0) return false;
                m_currentPage--;
                this->removeFocus();
                m_list->clear();
                rebuild();
                m_pendingFocus = true;
                return true;
            });
            m_list->addItem(prev);
        }

        if (m_currentPage + 1 < totalPages) {
            auto *next = new tsl::elm::ListItem("Next Page");
            next->setClickListener([this, totalPages](u64 keys) {
                if (!(keys & HidNpadButton_A)) return false;
                if (m_currentPage + 1 >= totalPages) return false;
                m_currentPage++;
                this->removeFocus();
                m_list->clear();
                rebuild();
                m_pendingFocus = true;
                return true;
            });
            m_list->addItem(next);
        }
    }
}

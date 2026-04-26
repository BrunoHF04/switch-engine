#include "MainGui.hpp"

#include "NumericInputGui.hpp"
#include "ResultsListGui.hpp"
#include "../scanner/ProcessUtils.hpp"
#include "../scanner/ResultsStore.hpp"
#include "../scanner/SengClient.hpp"
#include "../util/Logger.hpp"

#include <cinttypes>
#include <cstdio>

MainGui::MainGui() : m_scanner(std::make_unique<MemoryScanner>()) {
    // Marker explicitamente pedido para diagnostico de boot:
    // se voce ve esta linha em sdmc:/switch-engine.log, o ovlloader
    // carregou a NSO, executou main(), instanciou SwitchEngineOverlay
    // e passou o controle para a UI raiz. Restando "apenas" o draw.
    seng::log::write("[maingui] ctor");
}

tsl::elm::Element *MainGui::createUI() {
    auto *frame = new tsl::elm::OverlayFrame("Switch Engine", "Memory Scanner v0.2");
    auto *list  = new tsl::elm::List();

    // -------------------------------------------------------------------
    // Target
    // -------------------------------------------------------------------
    list->addItem(new tsl::elm::CategoryHeader("Target Process"));

    m_targetItem = new tsl::elm::ListItem("Detect foreground", "(no target)");
    m_targetItem->setClickListener([this](u64 keys) {
        if (keys & HidNpadButton_A) {
            onDetectTarget();
            return true;
        }
        return false;
    });
    list->addItem(m_targetItem);

    // -------------------------------------------------------------------
    // Search value
    // -------------------------------------------------------------------
    list->addItem(new tsl::elm::CategoryHeader("Search Value (uint32)"));

    m_valueItem = new tsl::elm::ListItem("Set value", "0");
    m_valueItem->setClickListener([this](u64 keys) {
        if (keys & HidNpadButton_A) {
            onPickSearchValue();
            return true;
        }
        return false;
    });
    list->addItem(m_valueItem);

    // -------------------------------------------------------------------
    // Scan actions
    // -------------------------------------------------------------------
    list->addItem(new tsl::elm::CategoryHeader("Scan"));

    auto *firstScan = new tsl::elm::ListItem("First Scan");
    firstScan->setClickListener([this](u64 keys) {
        if (keys & HidNpadButton_A) {
            onFirstScan();
            return true;
        }
        return false;
    });
    list->addItem(firstScan);

    auto *nextScan = new tsl::elm::ListItem("Next Scan");
    nextScan->setClickListener([this](u64 keys) {
        if (keys & HidNpadButton_A) {
            onNextScan();
            return true;
        }
        return false;
    });
    list->addItem(nextScan);

    auto *resetScan = new tsl::elm::ListItem("Reset");
    resetScan->setClickListener([this](u64 keys) {
        if (keys & HidNpadButton_A) {
            onResetScan();
            return true;
        }
        return false;
    });
    list->addItem(resetScan);

    // -------------------------------------------------------------------
    // Results
    // -------------------------------------------------------------------
    list->addItem(new tsl::elm::CategoryHeader("Results"));

    m_resultsItem = new tsl::elm::ListItem("Show Results", "0 hits");
    m_resultsItem->setClickListener([this](u64 keys) {
        if (keys & HidNpadButton_A) {
            onShowResults();
            return true;
        }
        return false;
    });
    list->addItem(m_resultsItem);

    // -------------------------------------------------------------------
    // Status
    // -------------------------------------------------------------------
    list->addItem(new tsl::elm::CategoryHeader("Status"));
    m_statusItem = new tsl::elm::ListItem("Last action", "(idle)");
    list->addItem(m_statusItem);

    frame->setContent(list);
    return frame;
}

void MainGui::update() {
    if (m_valueItem) {
        char buf[24];
        std::snprintf(buf, sizeof(buf), "%" PRIu32, m_searchValue);
        m_valueItem->setValue(buf);
    }

    if (m_resultsItem) {
        char buf[24];
        std::snprintf(buf, sizeof(buf), "%zu hits", ResultsStore::count());
        m_resultsItem->setValue(buf);
    }

    if (m_statusItem) {
        m_statusItem->setValue(m_status);
    }
}

// ===========================================================================
// Acoes
// ===========================================================================

void MainGui::onDetectTarget() {
    uint64_t pid = 0;
    uint64_t tid = 0;

    Result rc = ProcessUtils::getForegroundApplication(&pid, &tid);
    if (R_FAILED(rc) || pid == 0) {
        char buf[40];
        std::snprintf(buf, sizeof(buf), "detect failed (rc=0x%08X)", rc);
        m_status = buf;
        m_targetItem->setValue("error");
        return;
    }

    m_scanner->setTargetPid(pid);
    m_titleId = tid;

    char buf[40];
    std::snprintf(buf, sizeof(buf), "TID %016" PRIx64, tid);
    m_targetItem->setValue(buf);
    m_status = "target detected";
}

void MainGui::onPickSearchValue() {
    tsl::changeTo<NumericInputGui>(
        std::string("Search value (uint32)"),
        static_cast<uint64_t>(m_searchValue),
        static_cast<uint64_t>(UINT32_MAX),
        [this](uint64_t v) {
            m_searchValue = static_cast<uint32_t>(v);
        });
}

void MainGui::onFirstScan() {
    if (m_scanner->getTargetPid() == 0) {
        m_status = "no target (run Detect first)";
        return;
    }

    m_status = "scanning...";
    if (m_statusItem) m_statusItem->setValue(m_status);

    Result rc = m_scanner->firstScanU32(m_searchValue);
    char buf[64];
    if (R_FAILED(rc)) {
        std::snprintf(buf, sizeof(buf), "first scan failed (rc=0x%08X)", rc);
    } else {
        std::snprintf(buf, sizeof(buf), "first scan: %zu hits", ResultsStore::count());
    }
    m_status = buf;
}

void MainGui::onNextScan() {
    if (m_scanner->getTargetPid() == 0) {
        m_status = "no target (run Detect first)";
        return;
    }
    if (ResultsStore::count() == 0) {
        m_status = "no previous results (run First Scan)";
        return;
    }

    m_status = "filtering...";
    if (m_statusItem) m_statusItem->setValue(m_status);

    Result rc = m_scanner->nextScanU32(m_searchValue);
    char buf[64];
    if (R_FAILED(rc)) {
        std::snprintf(buf, sizeof(buf), "next scan failed (rc=0x%08X)", rc);
    } else {
        std::snprintf(buf, sizeof(buf), "next scan: %zu hits", ResultsStore::count());
    }
    m_status = buf;
}

void MainGui::onResetScan() {
    ResultsStore::reset();
    m_status = "results cleared";
}

void MainGui::onShowResults() {
    if (ResultsStore::count() == 0) {
        m_status = "no results to show";
        return;
    }
    tsl::changeTo<ResultsListGui>(m_scanner->getTargetPid(),
                                  static_cast<size_t>(0));
}

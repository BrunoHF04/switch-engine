#include "MainGui.hpp"
#include "../scanner/ProcessUtils.hpp"
#include "../scanner/ResultsStore.hpp"

#include <cstdio>
#include <cinttypes>

MainGui::MainGui() : m_scanner(std::make_unique<MemoryScanner>()) {}

tsl::elm::Element *MainGui::createUI() {
    auto *frame = new tsl::elm::OverlayFrame("Switch Engine", "Memory Scanner v0.1");
    auto *list  = new tsl::elm::List();
    m_list = list;

    // === Target ============================================================
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

    // === Scan actions ======================================================
    list->addItem(new tsl::elm::CategoryHeader("Scan (uint32 = 100)"));

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

    // === Results ===========================================================
    list->addItem(new tsl::elm::CategoryHeader("Results"));
    m_resultCountItem = new tsl::elm::ListItem("Hits", "0");
    list->addItem(m_resultCountItem);

    frame->setContent(list);
    return frame;
}

void MainGui::update() {
    if (m_resultCountItem) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%zu", ResultsStore::count());
        m_resultCountItem->setValue(buf);
    }
}

bool MainGui::handleInput(u64, u64, const HidTouchState &,
                          HidAnalogStickState, HidAnalogStickState) {
    return false;
}

// === Acoes ==================================================================

void MainGui::onDetectTarget() {
    uint64_t pid     = 0;
    uint64_t titleId = 0;
    if (R_FAILED(ProcessUtils::getForegroundApplication(&pid, &titleId))) {
        m_targetItem->setValue("error");
        return;
    }
    m_scanner->setTargetPid(pid);

    char buf[48];
    std::snprintf(buf, sizeof(buf), "TID %016" PRIx64, titleId);
    m_targetItem->setValue(buf);
}

void MainGui::onFirstScan() {
    m_scanner->firstScanU32(m_searchValue ? m_searchValue : 100);
}

void MainGui::onNextScan() {
    m_scanner->nextScanU32(m_searchValue ? m_searchValue : 100);
}

void MainGui::onResetScan() {
    ResultsStore::reset();
    m_currentPage = 0;
}

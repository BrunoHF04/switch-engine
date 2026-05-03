#include "MainGui.hpp"

#include "LanguageGui.hpp"
#include "NumericInputGui.hpp"
#include "ProcessListGui.hpp"
#include "ResultsListGui.hpp"
#include "../scanner/ProcessUtils.hpp"
#include "../scanner/Scanner.hpp"
#include "../scanner/ResultsStore.hpp"
#include "../scanner/SengClient.hpp"
#include "../util/Language.hpp"
#include "../util/Logger.hpp"

#include <cinttypes>
#include <cstdio>

namespace i18n = seng::i18n;

namespace {
    void rawDebugMark(const char *msg) {
        FILE *fp = std::fopen("sdmc:/switch-engine_debug.log", "a");
        if (!fp) return;
        std::fputs(msg, fp);
        std::fputc('\n', fp);
        std::fflush(fp);
        std::fclose(fp);
    }
} // namespace

MainGui::MainGui() : m_scanner(std::make_unique<MemoryScanner>()) {
    rawDebugMark("[debug] MainGui ctor");
    seng::log::write("[maingui] ctor");
    // Status default e' "(idle)". Inicializamos com a string traduzida
    // do idioma atual -- assim o primeiro draw ja' aparece no idioma certo.
    m_status = i18n::tr(i18n::S::Idle);
}

tsl::elm::Element *MainGui::createUI() {
    auto *frame = new tsl::elm::OverlayFrame("Switch Engine",
                                              i18n::tr(i18n::S::OverlaySubtitle));
    auto *list  = new tsl::elm::List();

    // -------------------------------------------------------------------
    // Target
    // -------------------------------------------------------------------
    list->addItem(new tsl::elm::CategoryHeader(
        i18n::tr(i18n::S::TargetProcessHeader)));

    m_targetItem = new tsl::elm::ListItem(i18n::tr(i18n::S::DetectForeground),
                                           i18n::tr(i18n::S::NoTarget));
    m_targetItem->setClickListener([this](u64 keys) {
        if (keys & HidNpadButton_A) {
            onDetectTarget();
            return true;
        }
        return false;
    });
    list->addItem(m_targetItem);

    auto *pickItem = new tsl::elm::ListItem(i18n::tr(i18n::S::PickProcess));
    pickItem->setClickListener([this](u64 keys) {
        if (keys & HidNpadButton_A) {
            onPickProcess();
            return true;
        }
        return false;
    });
    list->addItem(pickItem);

    // -------------------------------------------------------------------
    // Search value
    // -------------------------------------------------------------------
    list->addItem(new tsl::elm::CategoryHeader(
        i18n::tr(i18n::S::SearchValueHeader)));

    m_valueItem = new tsl::elm::ListItem(i18n::tr(i18n::S::SetValue), "0");
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
    list->addItem(new tsl::elm::CategoryHeader(
        i18n::tr(i18n::S::ScanHeader)));

    auto *firstScan = new tsl::elm::ListItem(i18n::tr(i18n::S::FirstScan));
    firstScan->setClickListener([this](u64 keys) {
        if (keys & HidNpadButton_A) {
            onFirstScan();
            return true;
        }
        return false;
    });
    list->addItem(firstScan);

    auto *nextScan = new tsl::elm::ListItem(i18n::tr(i18n::S::NextScan));
    nextScan->setClickListener([this](u64 keys) {
        if (keys & HidNpadButton_A) {
            onNextScan();
            return true;
        }
        return false;
    });
    list->addItem(nextScan);

    auto *resetScan = new tsl::elm::ListItem(i18n::tr(i18n::S::Reset));
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
    list->addItem(new tsl::elm::CategoryHeader(
        i18n::tr(i18n::S::ResultsHeader)));

    {
        char hits[24];
        std::snprintf(hits, sizeof(hits),
                      i18n::tr(i18n::S::HitsSuffixFmt), static_cast<size_t>(0));
        m_resultsItem = new tsl::elm::ListItem(i18n::tr(i18n::S::ShowResults),
                                                hits);
    }
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
    list->addItem(new tsl::elm::CategoryHeader(
        i18n::tr(i18n::S::StatusHeader)));
    m_statusItem = new tsl::elm::ListItem(i18n::tr(i18n::S::LastAction),
                                           m_status.c_str());
    list->addItem(m_statusItem);

    // -------------------------------------------------------------------
    // Settings (idioma)
    // -------------------------------------------------------------------
    list->addItem(new tsl::elm::CategoryHeader(
        i18n::tr(i18n::S::SettingsHeader)));

    auto *langItem = new tsl::elm::ListItem(i18n::tr(i18n::S::Language),
                                             i18n::currentCode());
    langItem->setClickListener([](u64 keys) {
        if (keys & HidNpadButton_A) {
            tsl::changeTo<LanguageGui>();
            return true;
        }
        return false;
    });
    list->addItem(langItem);

    // -------------------------------------------------------------------
    // Creditos (informativo; sem acao)
    // -------------------------------------------------------------------
    list->addItem(new tsl::elm::CategoryHeader(
        i18n::tr(i18n::S::CreditsHeader)));
    list->addItem(new tsl::elm::ListItem(
        i18n::tr(i18n::S::CreditsDeveloperLabel),
        i18n::tr(i18n::S::CreditsDeveloperName)));
    list->addItem(new tsl::elm::ListItem(
        i18n::tr(i18n::S::CreditsWebLabel),
        i18n::tr(i18n::S::CreditsWebUrl)));

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
        std::snprintf(buf, sizeof(buf),
                      i18n::tr(i18n::S::HitsSuffixFmt), ResultsStore::count());
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

    Result rc = Scanner::detectForegroundAfterProcessList(&pid, &tid);
    if (R_FAILED(rc) || pid == 0) {
        char buf[64];
        std::snprintf(buf, sizeof(buf),
                      i18n::tr(i18n::S::StatusDetectFailedFmt), rc);
        m_status = buf;
        m_targetItem->setValue(i18n::tr(i18n::S::StatusError));
        return;
    }

    m_scanner->setTargetPid(pid);
    m_titleId = tid;

    char buf[40];
    std::snprintf(buf, sizeof(buf), "TID %016" PRIx64, tid);
    m_targetItem->setValue(buf);
    m_status = i18n::tr(i18n::S::StatusTargetDetected);
}

void MainGui::onPickProcess() {
    // O ProcessListGui chama goBack() apos invocar o callback, entao quando
    // voltarmos para o MainGui o m_scanner ja' tera a pid setada e o
    // targetItem atualizado.
    tsl::changeTo<ProcessListGui>(
        [this](uint64_t pid, uint64_t tid) {
            this->onProcessPicked(pid, tid);
        });
}

void MainGui::onProcessPicked(uint64_t pid, uint64_t tid) {
    if (pid == 0) return;
    m_scanner->setTargetPid(pid);
    m_titleId = tid;

    if (m_targetItem) {
        char buf[40];
        if (tid != 0) {
            std::snprintf(buf, sizeof(buf), "TID 0x%016" PRIx64, tid);
        } else {
            std::snprintf(buf, sizeof(buf), "PID %" PRIu64, pid);
        }
        m_targetItem->setValue(buf);
    }

    char status[64];
    std::snprintf(status, sizeof(status),
                  i18n::tr(i18n::S::StatusProcessPickedFmt),
                  static_cast<unsigned long long>(pid));
    m_status = status;
}

void MainGui::onPickSearchValue() {
    tsl::changeTo<NumericInputGui>(
        std::string(i18n::tr(i18n::S::SearchValueTitle)),
        static_cast<uint64_t>(m_searchValue),
        static_cast<uint64_t>(UINT32_MAX),
        [this](uint64_t v) {
            m_searchValue = static_cast<uint32_t>(v);
        });
}

void MainGui::onFirstScan() {
    if (m_scanner->getTargetPid() == 0) {
        m_status = i18n::tr(i18n::S::StatusNoTargetRunDetect);
        return;
    }

    m_status = i18n::tr(i18n::S::StatusScanning);
    if (m_statusItem) m_statusItem->setValue(m_status);

    Result rc = m_scanner->firstScanU32(m_searchValue);
    char buf[80];
    if (R_FAILED(rc)) {
        std::snprintf(buf, sizeof(buf),
                      i18n::tr(i18n::S::StatusFirstScanFailedFmt), rc);
    } else {
        std::snprintf(buf, sizeof(buf),
                      i18n::tr(i18n::S::StatusFirstScanResultFmt),
                      ResultsStore::count());
    }
    m_status = buf;
}

void MainGui::onNextScan() {
    if (m_scanner->getTargetPid() == 0) {
        m_status = i18n::tr(i18n::S::StatusNoTargetRunDetect);
        return;
    }
    if (ResultsStore::count() == 0) {
        m_status = i18n::tr(i18n::S::StatusNoPreviousResults);
        return;
    }

    m_status = i18n::tr(i18n::S::StatusFiltering);
    if (m_statusItem) m_statusItem->setValue(m_status);

    Result rc = m_scanner->nextScanU32(m_searchValue);
    char buf[80];
    if (R_FAILED(rc)) {
        std::snprintf(buf, sizeof(buf),
                      i18n::tr(i18n::S::StatusNextScanFailedFmt), rc);
    } else {
        std::snprintf(buf, sizeof(buf),
                      i18n::tr(i18n::S::StatusNextScanResultFmt),
                      ResultsStore::count());
    }
    m_status = buf;
}

void MainGui::onResetScan() {
    ResultsStore::reset();
    m_status = i18n::tr(i18n::S::StatusResultsCleared);
}

void MainGui::onShowResults() {
    if (ResultsStore::count() == 0) {
        m_status = i18n::tr(i18n::S::StatusNoResultsToShow);
        return;
    }
    tsl::changeTo<ResultsListGui>(m_scanner->getTargetPid(),
                                  static_cast<size_t>(0));
}

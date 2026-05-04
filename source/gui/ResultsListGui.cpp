#include "ResultsListGui.hpp"

#include "NumericInputGui.hpp"
#include "../scanner/ResultsStore.hpp"
#include "../scanner/SengClient.hpp"
#include "../util/Language.hpp"
#include "../util/Logger.hpp"
#include "seng_ipc.hpp"

#include <cinttypes>
#include <cstdio>
#include <sys/stat.h>

namespace i18n = seng::i18n;

ResultsListGui::ResultsListGui(uint64_t targetPid, uint64_t titleId,
                                size_t initialPage)
    : m_targetPid(targetPid), m_titleId(titleId), m_currentPage(initialPage) {}

tsl::elm::Element *ResultsListGui::createUI() {
    auto *frame = new tsl::elm::OverlayFrame("Switch Engine",
                                              i18n::tr(i18n::S::ResultsTitle));
    m_list = new tsl::elm::List();
    rebuild();
    frame->setContent(m_list);
    return frame;
}

void ResultsListGui::update() {
    const size_t cur = ResultsStore::count();
    if (cur != m_lastBuiltCount) {
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

    char hdrBuf[80];
    std::snprintf(hdrBuf, sizeof(hdrBuf),
                  i18n::tr(i18n::S::PageInfoFmt),
                  m_total, m_currentPage + 1, totalPages);
    m_list->addItem(new tsl::elm::CategoryHeader(hdrBuf));

    if (m_total == 0) {
        m_list->addItem(new tsl::elm::ListItem(i18n::tr(i18n::S::NoResults),
                                               i18n::tr(i18n::S::RunFirstScan)));
        return;
    }

    // -------------------------------------------------------------------
    // Hits da pagina atual
    // -------------------------------------------------------------------
    const seng::ValueType vtype = ResultsStore::currentValueType();
    const size_t valSize = seng::valueTypeSize(vtype);
    const uint64_t pidCaptured = m_targetPid;
    const uint64_t tidCaptured = m_titleId;

    ResultsStore::readPage(m_currentPage, kPageSize,
        [this, pidCaptured, tidCaptured, vtype, valSize](const ResultsStore::Entry &e) {
            char addrBuf[24];
            char valBuf[24];
            std::snprintf(addrBuf, sizeof(addrBuf), "0x%010" PRIx64, e.address);
            std::snprintf(valBuf,  sizeof(valBuf),  "%" PRIu64, e.raw_value);

            auto *it = new tsl::elm::ListItem(addrBuf, valBuf);
            const uint64_t addr   = e.address;
            const uint64_t oldVal = e.raw_value;

            it->setClickListener(
                [this, addr, pidCaptured, vtype, valSize, oldVal](u64 keys) {
                if (!(keys & HidNpadButton_A)) return false;

                tsl::changeTo<NumericInputGui>(
                    std::string(i18n::tr(i18n::S::PokeValueTitle)),
                    static_cast<uint64_t>(0),
                    static_cast<uint64_t>(UINT64_MAX),
                    [this, addr, pidCaptured, vtype, valSize](uint64_t v) {
                        doPoke(addr, v);
                    });
                return true;
            });
            m_list->addItem(it);
        });

    // -------------------------------------------------------------------
    // Acoes extras: undo, freeze, export
    // -------------------------------------------------------------------
    m_list->addItem(new tsl::elm::CategoryHeader(
        i18n::tr(i18n::S::ActionHeader)));

    // Undo
    auto *undoItem = new tsl::elm::ListItem(i18n::tr(i18n::S::UndoPoke));
    undoItem->setClickListener([this](u64 keys) {
        if (!(keys & HidNpadButton_A)) return false;
        doUndo();
        return true;
    });
    m_list->addItem(undoItem);

    // Freeze clear
    auto *freezeClearItem = new tsl::elm::ListItem(i18n::tr(i18n::S::FreezeClear));
    freezeClearItem->setClickListener([](u64 keys) {
        if (!(keys & HidNpadButton_A)) return false;
        SengClient::clearFreezes();
        return true;
    });
    m_list->addItem(freezeClearItem);

    // Export cheats
    auto *exportItem = new tsl::elm::ListItem(i18n::tr(i18n::S::ExportCheats));
    exportItem->setClickListener([this](u64 keys) {
        if (!(keys & HidNpadButton_A)) return false;
        doExportCheats();
        return true;
    });
    m_list->addItem(exportItem);

    // -------------------------------------------------------------------
    // Navegacao entre paginas
    // -------------------------------------------------------------------
    if (totalPages > 1) {
        m_list->addItem(new tsl::elm::CategoryHeader(
            i18n::tr(i18n::S::Navigation)));

        if (m_currentPage > 0) {
            auto *prev = new tsl::elm::ListItem(
                i18n::tr(i18n::S::PreviousPage));
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
            auto *next = new tsl::elm::ListItem(
                i18n::tr(i18n::S::NextPage));
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

void ResultsListGui::doPoke(uint64_t addr, uint64_t value) {
    if (m_targetPid == 0) return;

    const seng::ValueType vtype = ResultsStore::currentValueType();
    const size_t valSize = seng::valueTypeSize(vtype);

    const Result ar = SengClient::attach(m_targetPid);
    if (R_FAILED(ar)) {
        seng::log::write("ERR  [poke] attach failed pid=%llu rc=0x%08X",
                         static_cast<unsigned long long>(m_targetPid), ar);
        return;
    }

    // Undo: ler valor atual antes de escrever.
    uint64_t oldVal = 0;
    size_t   readGot = 0;
    SengClient::readMemory(addr, &oldVal, valSize, &readGot);
    if (readGot == valSize) {
        if (m_undoStack.size() >= kUndoStackSize) {
            m_undoStack.erase(m_undoStack.begin());
        }
        m_undoStack.push_back(UndoEntry{ addr, oldVal, valSize });
    }

    size_t wrote = 0;
    const Result wr = SengClient::writeMemory(addr, &value, valSize, &wrote);
    SengClient::detach();

    if (R_FAILED(wr) || wrote != valSize) {
        seng::log::write("ERR  [poke] write addr=0x%010" PRIx64
                         " rc=0x%08X wrote=%zu",
                         addr, wr, wrote);
    }
}

void ResultsListGui::doUndo() {
    if (m_undoStack.empty()) return;

    auto last = m_undoStack.back();
    m_undoStack.pop_back();

    const Result ar = SengClient::attach(m_targetPid);
    if (R_FAILED(ar)) return;

    size_t wrote = 0;
    SengClient::writeMemory(last.addr, &last.oldValue, last.valueSize, &wrote);
    SengClient::detach();

    seng::log::write("INFO [undo] addr=0x%010" PRIx64 " restored",
                     last.addr);
}

void ResultsListGui::doFreeze(uint64_t addr, uint64_t value) {
    const seng::ValueType vtype = ResultsStore::currentValueType();
    uint8_t slot = 0xFF;
    Result rc = SengClient::addFreeze(addr, vtype, value, &slot);
    if (R_FAILED(rc)) {
        seng::log::write("ERR  [freeze] addFreeze failed rc=0x%08X", rc);
    }
}

void ResultsListGui::doExportCheats() {
    if (m_titleId == 0 || m_total == 0) return;

    char dirPath[128];
    std::snprintf(dirPath, sizeof(dirPath),
                  "sdmc:/atmosphere/contents/%016" PRIx64 "/cheats",
                  m_titleId);
    mkdir("sdmc:/atmosphere", 0777);
    mkdir("sdmc:/atmosphere/contents", 0777);

    char tidDir[96];
    std::snprintf(tidDir, sizeof(tidDir),
                  "sdmc:/atmosphere/contents/%016" PRIx64,
                  m_titleId);
    mkdir(tidDir, 0777);
    mkdir(dirPath, 0777);

    char filePath[160];
    std::snprintf(filePath, sizeof(filePath), "%s/switch-engine.txt", dirPath);

    FILE *fp = std::fopen(filePath, "w");
    if (!fp) {
        seng::log::write("ERR  [export] fopen failed: %s", filePath);
        return;
    }

    std::fprintf(fp, "[Switch Engine Export]\n");

    const seng::ValueType vtype = ResultsStore::currentValueType();
    const size_t valSize = seng::valueTypeSize(vtype);
    size_t exported = 0;

    // Exporta todas as paginas.
    const size_t totalPages = (m_total + kPageSize - 1) / kPageSize;
    for (size_t page = 0; page < totalPages; ++page) {
        ResultsStore::readPage(page, kPageSize,
            [&](const ResultsStore::Entry &e) {
                // Formato Atmosphere cheat: write static
                // 0VVVVVVV AAAAAAAA AAAAAAAA VVVVVVVV
                // Tipo 0 = store estático com width encoding.
                uint32_t widthBits = 0;
                switch (valSize) {
                    case 1: widthBits = 1; break;
                    case 2: widthBits = 2; break;
                    case 4: widthBits = 4; break;
                    case 8: widthBits = 8; break;
                }
                std::fprintf(fp, "04000000 %08" PRIX64 " %08" PRIX64 "\n",
                             e.address,
                             e.raw_value);
                ++exported;
            });
    }

    std::fflush(fp);
    std::fclose(fp);

    seng::log::write("INFO [export] %zu cheats -> %s", exported, filePath);
}

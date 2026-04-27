#include "ProcessListGui.hpp"

#include "../scanner/SengClient.hpp"
#include "../util/Language.hpp"

#include <cinttypes>
#include <cstdio>

namespace i18n = seng::i18n;

namespace {
    // Heuristica simples para classificar o processo na UI. Nao queremos
    // ESCONDER nada -- o user pode escolher qualquer um -- so' rotular.
    //
    //   - tid == 0          -> "Process" (provavelmente init/kernel-side)
    //   - tid em [0x0100000000000000, 0x01FFFFFFFFFFFFFF] -> "App"
    //   - tid em [0x0500000000000000, ..]                 -> "Sysmod"
    //   - resto             -> "Process"
    const char *classifyTid(uint64_t tid) {
        if (tid == 0) return "Process";
        if ((tid >> 56) == 0x01) {
            // Faixa de aplicativos comerciais.
            // Range exato (de pmla): 0x010000_00000_00000 .. 0x01FFFFFFFFFFFFFF
            return "App";
        }
        if (tid >= 0x0100000000000000ULL && tid < 0x0200000000000000ULL) {
            return "App";
        }
        if (tid >= 0x0500000000000000ULL && tid < 0x0600000000000000ULL) {
            return "Sysmod";
        }
        if ((tid >> 60) == 0x4 || (tid >> 60) == 0x5) {
            // 0x4...... e 0x5...... sao TIDs custom (sysmods de homebrew).
            return "Sysmod";
        }
        return "Process";
    }
}

ProcessListGui::ProcessListGui(PickCallback onPick)
    : m_onPick(std::move(onPick)) {}

tsl::elm::Element *ProcessListGui::createUI() {
    auto *frame = new tsl::elm::OverlayFrame("Switch Engine",
                                              i18n::tr(i18n::S::ProcessListTitle));
    auto *list  = new tsl::elm::List();

    // Faz a chamada IPC e popula a UI. Em caso de erro, mostra status.
    seng::ProcessEntry entries[seng::kMaxProcessList] = {};
    size_t count = 0;
    Result rc = SengClient::listProcesses(entries, seng::kMaxProcessList,
                                          &count);

    if (R_FAILED(rc)) {
        char errBuf[64];
        std::snprintf(errBuf, sizeof(errBuf),
                      i18n::tr(i18n::S::ProcessListErrorFmt), rc);
        list->addItem(new tsl::elm::CategoryHeader(
            i18n::tr(i18n::S::ProcessListTitle)));
        list->addItem(new tsl::elm::ListItem(errBuf));
        frame->setContent(list);
        return frame;
    }

    if (count == 0) {
        list->addItem(new tsl::elm::CategoryHeader(
            i18n::tr(i18n::S::ProcessListTitle)));
        list->addItem(new tsl::elm::ListItem(
            i18n::tr(i18n::S::ProcessListEmpty)));
        frame->setContent(list);
        return frame;
    }

    char hdrBuf[48];
    std::snprintf(hdrBuf, sizeof(hdrBuf),
                  i18n::tr(i18n::S::ProcessListCountFmt), count);
    list->addItem(new tsl::elm::CategoryHeader(hdrBuf));

    for (size_t i = 0; i < count; ++i) {
        const seng::ProcessEntry &e = entries[i];

        char title[48];
        char sub[48];
        std::snprintf(title, sizeof(title),
                      "%s  PID %" PRIu64,
                      classifyTid(e.tid),
                      e.pid);
        if (e.tid != 0) {
            std::snprintf(sub, sizeof(sub),
                          "TID 0x%016" PRIx64, e.tid);
        } else {
            std::snprintf(sub, sizeof(sub), "-");
        }

        auto *it = new tsl::elm::ListItem(title, sub);
        const uint64_t pid = e.pid;
        const uint64_t tid = e.tid;
        it->setClickListener([this, pid, tid](u64 keys) {
            if (!(keys & HidNpadButton_A)) return false;
            if (m_onPick) m_onPick(pid, tid);
            tsl::goBack();
            return true;
        });
        list->addItem(it);
    }

    frame->setContent(list);
    return frame;
}

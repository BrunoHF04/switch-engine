#include "ProcessListGui.hpp"

#include "../scanner/SengClient.hpp"
#include "../util/Language.hpp"

#include <cinttypes>
#include <cstdio>
#include <cstring>

namespace i18n = seng::i18n;

namespace {
    const char *classifyTid(uint64_t tid) {
        if (tid == 0) return "Process";
        if ((tid >> 56) == 0x01) return "App";
        if (tid >= 0x0100000000000000ULL && tid < 0x0200000000000000ULL) return "App";
        if (tid >= 0x0500000000000000ULL && tid < 0x0600000000000000ULL) return "Sysmod";
        if ((tid >> 60) == 0x4 || (tid >> 60) == 0x5) return "Sysmod";
        return "Process";
    }
}

ProcessListGui::ProcessListGui(PickCallback onPick)
    : m_onPick(std::move(onPick)) {}

tsl::elm::Element *ProcessListGui::createUI() {
    auto *frame = new tsl::elm::OverlayFrame("Switch Engine",
                                              i18n::tr(i18n::S::ProcessListTitle));
    auto *list  = new tsl::elm::List();

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

    size_t usable = 0;
    for (size_t i = 0; i < count; ++i) {
        if (entries[i].pid != 0) ++usable;
    }

    if (usable == 0) {
        list->addItem(new tsl::elm::CategoryHeader(
            i18n::tr(i18n::S::ProcessListTitle)));
        list->addItem(new tsl::elm::ListItem(
            i18n::tr(i18n::S::ProcessListEmpty)));
        frame->setContent(list);
        return frame;
    }

    char hdrBuf[48];
    std::snprintf(hdrBuf, sizeof(hdrBuf),
                  i18n::tr(i18n::S::ProcessListCountFmt), usable);
    list->addItem(new tsl::elm::CategoryHeader(hdrBuf));

    for (size_t i = 0; i < count; ++i) {
        const seng::ProcessEntry &e = entries[i];
        if (e.pid == 0) continue;

        char title[48];
        char sub[96];

        // Usa o name do ProcessEntry quando disponivel.
        if (e.name[0] != '\0') {
            std::snprintf(title, sizeof(title), "%s (PID %" PRIu64 ")",
                          e.name, e.pid);
        } else {
            std::snprintf(title, sizeof(title), "PID %" PRIu64, e.pid);
        }

        if (e.tid != 0) {
            std::snprintf(sub, sizeof(sub), "%s  0x%016" PRIx64,
                          classifyTid(e.tid), e.tid);
        } else {
            std::snprintf(sub, sizeof(sub), "%s  (sem TID)",
                          classifyTid(e.tid));
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

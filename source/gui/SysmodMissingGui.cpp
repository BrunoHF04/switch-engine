#include "SysmodMissingGui.hpp"

#include "../util/Language.hpp"

namespace i18n = seng::i18n;

tsl::elm::Element *SysmodMissingGui::createUI() {
    auto *frame = new tsl::elm::OverlayFrame(
        "Switch Engine",
        i18n::tr(i18n::S::SysmodMissingTitle));
    auto *list = new tsl::elm::List();

    list->addItem(new tsl::elm::CategoryHeader(
        i18n::tr(i18n::S::SysmodMissingTitle)));
    list->addItem(new tsl::elm::ListItem(
        i18n::tr(i18n::S::SysmodMissingBody),
        i18n::tr(i18n::S::SysmodMissingHint)));

    frame->setContent(list);
    return frame;
}

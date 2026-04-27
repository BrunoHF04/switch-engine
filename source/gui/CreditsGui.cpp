#include "CreditsGui.hpp"

#include "../util/Language.hpp"

namespace i18n = seng::i18n;

tsl::elm::Element *CreditsGui::createUI() {
    auto *frame = new tsl::elm::OverlayFrame("Switch Engine",
                                              i18n::tr(i18n::S::CreditsScreenTitle));
    auto *list  = new tsl::elm::List();

    list->addItem(new tsl::elm::CategoryHeader(
        i18n::tr(i18n::S::CreditsScreenTitle)));

    list->addItem(new tsl::elm::ListItem(
        i18n::tr(i18n::S::CreditsAppName),
        i18n::tr(i18n::S::CreditsAppTagline)));

    list->addItem(new tsl::elm::ListItem(
        i18n::tr(i18n::S::CreditsDeveloperName),
        i18n::tr(i18n::S::CreditsDeveloperRole)));

    list->addItem(new tsl::elm::ListItem(
        i18n::tr(i18n::S::CreditsPortfolioUrl),
        i18n::tr(i18n::S::CreditsPortfolioRole)));

    frame->setContent(list);
    return frame;
}

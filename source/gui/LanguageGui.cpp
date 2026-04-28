#include "LanguageGui.hpp"

#include "../util/Language.hpp"

namespace i18n = seng::i18n;

namespace {
    void selectAndExit(i18n::Lang l) {
        i18n::setCurrent(l);
        i18n::save();
        tsl::goBack();
    }

    // Cria um item do tipo radio: aparece o label + um marcador no value
    // ("[*]" ou "[ ]") indicando se e' o idioma corrente.
    tsl::elm::ListItem *makeLangItem(const char *label, i18n::Lang l) {
        auto *it = new tsl::elm::ListItem(label,
                                          i18n::current() == l ? "[*]" : "[ ]");
        it->setClickListener([l](u64 keys) {
            if (keys & HidNpadButton_A) {
                selectAndExit(l);
                return true;
            }
            return false;
        });
        return it;
    }
}

tsl::elm::Element *LanguageGui::createUI() {
    auto *frame = new tsl::elm::OverlayFrame("Switch Engine",
                                              i18n::tr(i18n::S::LanguageTitle));
    auto *list  = new tsl::elm::List();

    list->addItem(new tsl::elm::CategoryHeader(
        i18n::tr(i18n::S::LanguageTitle)));

    list->addItem(makeLangItem(i18n::tr(i18n::S::LanguageEnglish),
                               i18n::Lang::En));
    list->addItem(makeLangItem(i18n::tr(i18n::S::LanguagePortugueseBr),
                               i18n::Lang::PtBr));

    frame->setContent(list);
    return frame;
}

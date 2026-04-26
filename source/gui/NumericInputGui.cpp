#include "NumericInputGui.hpp"

#include <cinttypes>
#include <cstdio>

NumericInputGui::NumericInputGui(std::string title,
                                 uint64_t    initial,
                                 uint64_t    maxValue,
                                 Callback    onConfirm)
    : m_title(std::move(title)),
      m_value(initial > maxValue ? maxValue : initial),
      m_maxValue(maxValue),
      m_onConfirm(std::move(onConfirm)) {}

tsl::elm::Element *NumericInputGui::createUI() {
    auto *frame = new tsl::elm::OverlayFrame(m_title, "Switch Engine");
    auto *list  = new tsl::elm::List();

    list->addItem(new tsl::elm::CategoryHeader("Value (decimal)"));
    m_display = new tsl::elm::ListItem("Value");
    refreshDisplay();
    list->addItem(m_display);

    list->addItem(new tsl::elm::CategoryHeader("Digits"));
    for (int d = 0; d <= 9; ++d) {
        char label[2] = { static_cast<char>('0' + d), 0 };
        auto *btn = new tsl::elm::ListItem(label);
        btn->setClickListener([this, d](u64 keys) {
            if (keys & HidNpadButton_A) {
                this->appendDigit(d);
                return true;
            }
            return false;
        });
        list->addItem(btn);
    }

    list->addItem(new tsl::elm::CategoryHeader("Edit"));
    auto *bs = new tsl::elm::ListItem("Backspace");
    bs->setClickListener([this](u64 keys) {
        if (keys & HidNpadButton_A) {
            this->backspace();
            return true;
        }
        return false;
    });
    list->addItem(bs);

    auto *cl = new tsl::elm::ListItem("Clear");
    cl->setClickListener([this](u64 keys) {
        if (keys & HidNpadButton_A) {
            this->clearAll();
            return true;
        }
        return false;
    });
    list->addItem(cl);

    list->addItem(new tsl::elm::CategoryHeader("Action"));
    auto *cf = new tsl::elm::ListItem("Confirm");
    cf->setClickListener([this](u64 keys) {
        if (keys & HidNpadButton_A) {
            if (m_onConfirm) m_onConfirm(m_value);
            tsl::goBack();
            return true;
        }
        return false;
    });
    list->addItem(cf);

    auto *cn = new tsl::elm::ListItem("Cancel");
    cn->setClickListener([](u64 keys) {
        if (keys & HidNpadButton_A) {
            tsl::goBack();
            return true;
        }
        return false;
    });
    list->addItem(cn);

    frame->setContent(list);
    return frame;
}

void NumericInputGui::appendDigit(int d) {
    // Detecta overflow tanto pelo cap m_maxValue quanto por overflow real do u64.
    constexpr uint64_t kU64Max = UINT64_MAX;
    if (m_value > (kU64Max - static_cast<uint64_t>(d)) / 10) return;
    uint64_t next = m_value * 10ull + static_cast<uint64_t>(d);
    if (next > m_maxValue) return;
    m_value = next;
    refreshDisplay();
}

void NumericInputGui::backspace() {
    m_value /= 10;
    refreshDisplay();
}

void NumericInputGui::clearAll() {
    m_value = 0;
    refreshDisplay();
}

void NumericInputGui::refreshDisplay() {
    if (!m_display) return;
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%" PRIu64, m_value);
    m_display->setValue(buf);
}

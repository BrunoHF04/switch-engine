#pragma once

#include <tesla.hpp>

#include <cstdint>
#include <functional>
#include <string>

/**
 * NumericInputGui
 *
 * Teclado numerico decimal reutilizavel. Sem dependencia de swkbd (que nao
 * funciona dentro de overlays Tesla). O usuario digita um inteiro positivo
 * usando ListItems "0".."9", "Backspace" e "Clear", e confirma com "Confirm".
 *
 * Quando confirma, chama o callback fornecido com o valor (uint64_t) e empilha
 * goBack() automaticamente. Ao cancelar (B ou item "Cancel"), apenas goBack().
 *
 * Uso:
 *   tsl::changeTo<NumericInputGui>(
 *       "Search value",
 *       m_searchValue,         // valor inicial
 *       UINT32_MAX,            // valor maximo aceito
 *       [this](uint64_t v) { m_searchValue = static_cast<uint32_t>(v); });
 */
class NumericInputGui : public tsl::Gui {
public:
    using Callback = std::function<void(uint64_t)>;

    NumericInputGui(std::string title,
                    uint64_t    initial,
                    uint64_t    maxValue,
                    Callback    onConfirm);

    tsl::elm::Element *createUI() override;

private:
    std::string         m_title;
    uint64_t            m_value;
    uint64_t            m_maxValue;
    Callback            m_onConfirm;
    tsl::elm::ListItem *m_display = nullptr;

    void appendDigit(int d);
    void backspace();
    void clearAll();
    void refreshDisplay();
};

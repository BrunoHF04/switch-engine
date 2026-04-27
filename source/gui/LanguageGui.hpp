#pragma once

#include <tesla.hpp>

/**
 * LanguageGui
 *
 * Tela simples para selecionar o idioma do overlay.
 * Ao clicar em uma opcao:
 *   - Atualiza seng::i18n::setCurrent + save() (config.ini no SD).
 *   - Volta para o MainGui (goBack).
 *   - O MainGui ja' construido NAO troca de idioma sozinho -- so' apos
 *     fechar+reabrir o overlay (ou se navegar para uma tela nova).
 *     Mostramos um aviso "reopen to apply" no Status quando relevante.
 */
class LanguageGui : public tsl::Gui {
public:
    LanguageGui() = default;
    ~LanguageGui() override = default;

    tsl::elm::Element *createUI() override;
};
